#include "FSDungeonPlan.h"

#include "Algo/Reverse.h"
#include "CompGeom/Delaunay2.h"

namespace FSDungeon
{
FIntPoint DoorTile(const FRoom& Room, FIntPoint Direction)
{
    return Room.Center + Direction * (Direction.X != 0 ? Room.HalfSize.X : Room.HalfSize.Y);
}

uint8 RoomOpenings(const FPlan& Plan, const FRoom& Room)
{
    uint8 Openings = 0;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Directions); ++Index)
    {
        const FIntPoint Side = Directions[Index];
        const FIntPoint Door = DoorTile(Room, Side);
        if (Plan.Corridors.Contains(Door + Side)
            || (Plan.DirectRoomDoors.Contains(Door) && Plan.DirectRoomDoors.Contains(Door + Side)))
        {
            Openings |= 1 << Index;
        }
    }
    return Openings;
}

uint8 CorridorOpenings(const FPlan& Plan, FIntPoint Tile)
{
    uint8 Openings = 0;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Directions); ++Index)
    {
        const FIntPoint Side = Directions[Index];
        const FIntPoint Neighbor = Tile + Side;
        if (Plan.Corridors.Contains(Neighbor) || Plan.Rooms.ContainsByPredicate([&](const FRoom& Room)
        {
            return DoorTile(Room, Side * -1) == Neighbor;
        }))
        {
            Openings |= 1 << Index;
        }
    }
    return Openings;
}

bool HasSpacedCorners(const FPlan& Plan)
{
    const auto IsCorner = [](uint8 Ports)
    {
        return FMath::CountBits(static_cast<uint32>(Ports)) == 2 && Ports != 0b0101 && Ports != 0b1010;
    };
    for (FIntPoint Tile : Plan.Corridors)
    {
        const uint8 Ports = CorridorOpenings(Plan, Tile);
        if (!IsCorner(Ports)) continue;
        for (int32 Side = 0; Side < UE_ARRAY_COUNT(Directions); ++Side)
        {
            if (!(Ports & (1 << Side))) continue;
            const uint8 StraightPorts = (1 << Side) | (1 << ((Side + 2) % 4));
            for (int32 Step = 1; Step <= MinStraightTilesBetweenCorners; ++Step)
            {
                const FIntPoint Next = Tile + Directions[Side] * Step;
                if (!Plan.Corridors.Contains(Next)) break;
                const uint8 NextPorts = CorridorOpenings(Plan, Next);
                if (IsCorner(NextPorts)) return false;
                if (NextPorts != StraightPorts) break;
            }
        }
    }
    return true;
}

bool IsStraight(uint8 Openings)
{
    return Openings == 0b0101 || Openings == 0b1010;
}

namespace
{
constexpr int32 RoomCount = 5;
constexpr int32 RoutingHalfExtent = RoomFieldHalfExtent + 7;
constexpr int32 MaxRouteBends = 4;
constexpr int32 StepsBetweenBends = MinStraightTilesBetweenCorners + 1;

struct FEdge
{
    int32 A;
    int32 B;
    int32 Weight;
};

using FRoomTiles = TMap<FIntPoint, int32>;

int32 Distance(FIntPoint A, FIntPoint B)
{
    return FMath::Abs(A.X - B.X) + FMath::Abs(A.Y - B.Y);
}

bool IsInRoomField(FIntPoint Center)
{
    return FMath::Abs(Center.X) <= RoomFieldHalfExtent && FMath::Abs(Center.Y) <= RoomFieldHalfExtent;
}

bool Fits(const TArray<FRoom>& Rooms, const FRoom& Candidate, int32 Margin)
{
    for (const FRoom& Room : Rooms)
    {
        const FIntPoint Delta = Room.Center - Candidate.Center;
        if (FMath::Abs(Delta.X) <= Room.HalfSize.X + Candidate.HalfSize.X + Margin
            && FMath::Abs(Delta.Y) <= Room.HalfSize.Y + Candidate.HalfSize.Y + Margin)
        {
            return false;
        }
    }
    return true;
}

void IndexRoom(FRoomTiles& RoomTiles, const FRoom& Room, int32 Index)
{
    for (int32 X = -Room.HalfSize.X; X <= Room.HalfSize.X; ++X)
    {
        for (int32 Y = -Room.HalfSize.Y; Y <= Room.HalfSize.Y; ++Y)
        {
            RoomTiles.Add(Room.Center + FIntPoint(X, Y), Index);
        }
    }
}

bool PlaceRooms(FRandomStream& Random, TArray<FRoom>& Rooms)
{
    Rooms.Add({FIntPoint::ZeroValue, NormalHalfSize, ERoomRole::Entrance});
    while (Rooms.Num() < RoomCount)
    {
        TArray<FIntPoint> Candidates;
        TArray<FIntPoint> AlignedCandidates;
        // Give the encounter route enough depth before adding the cache candidate.
        const bool bGrowEncounterRoute = Rooms.Num() < RoomCount - 1;
        const int32 FirstAnchor = bGrowEncounterRoute ? Rooms.Num() - 1 : 0;
        for (int32 Index = FirstAnchor; Index < Rooms.Num(); ++Index)
        {
            const FRoom& Room = Rooms[Index];
            for (FIntPoint Direction : Directions)
            {
                const FIntPoint Side(-Direction.Y, Direction.X);
                for (int32 Step : {3, 4})
                {
                    for (int32 Offset = -1; Offset <= 1; ++Offset)
                    {
                        const FIntPoint Center = Room.Center + Direction * Step + Side * Offset;
                        if (!IsInRoomField(Center) || !Fits(Rooms, {Center, NormalHalfSize}, 0)) continue;
                        if (bGrowEncounterRoute && Rooms.ContainsByPredicate([&](const FRoom& Other)
                        {
                            return &Other != &Room && Distance(Other.Center, Center) <= Distance(Room.Center, Center);
                        }))
                        {
                            continue;
                        }
                        Candidates.AddUnique(Center);
                        if (Offset == 0) AlignedCandidates.AddUnique(Center);
                    }
                }
            }
        }
        if (Candidates.IsEmpty()) return false;
        // Seeded choices vary which rooms share a wall axis.
        const TArray<FIntPoint>& Pool = !AlignedCandidates.IsEmpty() && Random.RandRange(0, 2) != 0
            ? AlignedCandidates : Candidates;
        FIntPoint Center = Pool[Random.RandHelper(Pool.Num())];
        for (int32 Draw = 0; !bGrowEncounterRoute && Draw < 3; ++Draw)
        {
            const FIntPoint Option = Pool[Random.RandHelper(Pool.Num())];
            if (Distance(Option, FIntPoint::ZeroValue) < Distance(Center, FIntPoint::ZeroValue)) Center = Option;
        }
        Rooms.Add({Center, NormalHalfSize});
    }
    return true;
}

TArray<FEdge> Triangulate(const TArray<FRoom>& Rooms, FRandomStream& Random)
{
    TArray<FVector2D> Centers;
    for (const FRoom& Room : Rooms)
    {
        Centers.Emplace(Room.Center.X, Room.Center.Y);
    }

    UE::Geometry::FDelaunay2 Delaunay;
    Delaunay.RandomStream.Initialize(Random.GetUnsignedInt());
    TSet<FIntPoint> Edges;
    if (Delaunay.Triangulate(Centers))
    {
        for (const UE::Geometry::FIndex3i& Triangle : Delaunay.GetTriangles())
        {
            for (int32 Index = 0; Index < 3; ++Index)
            {
                const int32 A = Triangle[Index];
                const int32 B = Triangle[(Index + 1) % 3];
                Edges.Add(FIntPoint(FMath::Min(A, B), FMath::Max(A, B)));
            }
        }
    }

    TArray<FEdge> Result;
    for (const FIntPoint& Edge : Edges)
    {
        const FIntPoint Delta = Rooms[Edge.X].Center - Rooms[Edge.Y].Center;
        Result.Add({Edge.X, Edge.Y, FMath::Abs(Delta.X) + FMath::Abs(Delta.Y)});
    }
    Result.Sort([](const FEdge& A, const FEdge& B)
    {
        if (A.Weight != B.Weight) return A.Weight < B.Weight;
        if (A.A != B.A) return A.A < B.A;
        return A.B < B.B;
    });
    return Result;
}

int32 Root(const TArray<int32>& Parents, int32 Node)
{
    while (Parents[Node] != Node)
    {
        Node = Parents[Node];
    }
    return Node;
}

void BuildTree(const TArray<FEdge>& Edges, TArray<int32> (&Tree)[RoomCount])
{
    TArray<int32> Parents;
    for (int32 Index = 0; Index < RoomCount; ++Index)
    {
        Parents.Add(Index);
    }
    for (const FEdge& Edge : Edges)
    {
        const int32 A = Root(Parents, Edge.A);
        const int32 B = Root(Parents, Edge.B);
        if (A == B) continue;
        Parents[B] = A;
        Tree[Edge.A].Add(Edge.B);
        Tree[Edge.B].Add(Edge.A);
    }
}

bool PlaceExit(FPlan& Plan, int32 Boss, FRoomTiles& RoomTiles, TSet<FIntPoint>& Reserved)
{
    const FRoom& Arena = Plan.Rooms[Boss];
    for (const FIntPoint Direction : Directions)
    {
        const FIntPoint Door = DoorTile(Arena, Direction) + Direction;
        constexpr int32 ExitPathLength = 1;
        const FRoom Exit{Door + Direction * (ExitPathLength + ExitHalfSize.X), ExitHalfSize, ERoomRole::Exit};
        if (!IsInRoomField(Exit.Center) || !Fits(Plan.Rooms, Exit, 1))
        {
            continue;
        }

        TSet<FIntPoint> Throat;
        for (int32 Step = 0; Step < ExitPathLength; ++Step)
        {
            Throat.Add(Door + Direction * Step);
        }
        const FIntPoint Side(-Direction.Y, Direction.X);
        bool bBlocked = false;
        for (FIntPoint Tile : Throat)
        {
            bBlocked |= RoomTiles.Contains(Tile) || RoomTiles.Contains(Tile + Side) || RoomTiles.Contains(Tile - Side);
        }
        if (bBlocked)
        {
            continue;
        }

        Plan.ExitDirection = Direction;
        Plan.GateTile = Door;
        IndexRoom(RoomTiles, Exit, Plan.Rooms.Add(Exit));
        Reserved = MoveTemp(Throat);
        return true;
    }
    return false;
}

TArray<FIntPoint> MakeL(FIntPoint Start, FIntPoint End, bool bXFirst)
{
    const FIntPoint Corner = bXFirst ? FIntPoint(End.X, Start.Y) : FIntPoint(Start.X, End.Y);
    TArray<FIntPoint> Path{Start};
    for (const FIntPoint Goal : {Corner, End})
    {
        while (Path.Last() != Goal)
        {
            const FIntPoint Delta = Goal - Path.Last();
            Path.Add(Path.Last() + FIntPoint(FMath::Sign(Delta.X), FMath::Sign(Delta.Y)));
        }
    }
    return Path;
}

bool IsAllowedRoom(const FRoomTiles& RoomTiles, FIntPoint Tile, int32 A, int32 B)
{
    const int32* Room = RoomTiles.Find(Tile);
    return !Room || *Room == A || *Room == B;
}

bool CanCarveTile(const FPlan& Plan, const FRoomTiles& RoomTiles, const TSet<FIntPoint>& Reserved,
    FIntPoint Tile, int32 A, int32 B, const FIntPoint* AdjacentJunction)
{
    if (!IsAllowedRoom(RoomTiles, Tile, A, B) || Plan.Corridors.Contains(Tile) || Reserved.Contains(Tile)) return false;
    if (RoomTiles.Contains(Tile)) return true;
    for (FIntPoint Side : Directions)
    {
        const FIntPoint Neighbor = Tile + Side;
        if (!IsAllowedRoom(RoomTiles, Neighbor, A, B) || Reserved.Contains(Neighbor)) return false;
        if (const int32* Room = RoomTiles.Find(Neighbor); Room && DoorTile(Plan.Rooms[*Room], Side * -1) != Neighbor) return false;
        if (Plan.Corridors.Contains(Neighbor) && (!AdjacentJunction || Neighbor != *AdjacentJunction)) return false;
    }
    return true;
}

bool CanCarve(const FPlan& Plan, const TArray<FIntPoint>& Path, const FRoomTiles& RoomTiles,
    const TSet<FIntPoint>& Reserved, int32 A, int32 B, const FIntPoint* Junction)
{
    for (int32 Index = 0; Index < Path.Num(); ++Index)
    {
        const FIntPoint Tile = Path[Index];
        if (Junction && Tile == *Junction)
        {
            continue;
        }
        if (!CanCarveTile(Plan, RoomTiles, Reserved, Tile, A, B, Index == Path.Num() - 2 ? Junction : nullptr)) return false;
    }
    return true;
}

// A centered socket sometimes needs an extra bend to clear the room wall.
TArray<FIntPoint> RouteBetweenPorts(const FPlan& Plan, const FRoomTiles& RoomTiles,
    const TSet<FIntPoint>& Reserved, int32 A, int32 B, const FIntPoint* Junction = nullptr)
{
    struct FStep
    {
        FIntPoint Tile;
        int32 Side;
        int32 StraightSteps;
        int32 Bends;
        int32 Parent;

        FIntVector Key() const
        {
            return {Tile.X, Tile.Y, ((Bends * (StepsBetweenBends + 1) + StraightSteps) * 4) + Side};
        }
    };
    TMap<FIntPoint, int32> Goals;
    TArray<FStep> Queue;
    TSet<FIntVector> Seen;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Directions); ++Index)
    {
        const FIntPoint Side = Directions[Index];
        const FIntPoint Start = DoorTile(Plan.Rooms[A], Side) + Side;
        if (CanCarveTile(Plan, RoomTiles, Reserved, Start, A, B, nullptr))
        {
            const FStep& Step = Queue.Add_GetRef({Start, Index, StepsBetweenBends, 0, INDEX_NONE});
            Seen.Add(Step.Key());
        }
        Goals.Add(Junction ? *Junction + Side : DoorTile(Plan.Rooms[B], Side) + Side, (Index + 2) % 4);
    }
    for (int32 Index = 0; Index < Queue.Num(); ++Index)
    {
        const FStep Step = Queue[Index];
        const auto CanContinue = [&](int32 Side)
        {
            return Side == Step.Side || (Side != (Step.Side + 2) % 4
                && Step.StraightSteps == StepsBetweenBends && Step.Bends < MaxRouteBends);
        };
        if (const int32* ExitSide = Goals.Find(Step.Tile))
        {
            if (CanContinue(*ExitSide))
            {
                TArray<FIntPoint> Path;
                if (Junction) Path.Add(*Junction);
                for (int32 Node = Index; Node != INDEX_NONE; Node = Queue[Node].Parent)
                {
                    Path.Add(Queue[Node].Tile);
                }
                Algo::Reverse(Path);
                return Path;
            }
            continue;
        }
        for (const int32 Side : {Step.Side, (Step.Side + 1) % 4, (Step.Side + 3) % 4})
        {
            if (!CanContinue(Side)) continue;
            const FIntPoint Next = Step.Tile + Directions[Side];
            const bool bBend = Side != Step.Side;
            const FStep Candidate{Next, Side, bBend ? 1 : FMath::Min(StepsBetweenBends, Step.StraightSteps + 1),
                Step.Bends + (bBend ? 1 : 0), Index};
            if (FMath::Abs(Next.X) > RoutingHalfExtent || FMath::Abs(Next.Y) > RoutingHalfExtent || Seen.Contains(Candidate.Key())
                || RoomTiles.Contains(Next)
                || !CanCarveTile(Plan, RoomTiles, Reserved, Next, A, B, Goals.Contains(Next) ? Junction : nullptr))
            {
                continue;
            }
            Seen.Add(Candidate.Key());
            Queue.Add(Candidate);
        }
    }
    return {};
}

bool HasCorridorModuleSpace(const FPlan& Plan)
{
    for (FIntPoint Tile : Plan.Corridors)
    {
        const uint8 Openings = CorridorOpenings(Plan, Tile);
        const int32 Degree = FMath::CountBits(static_cast<uint32>(Openings));
        if (Degree < 2 || Degree > 3) return false;
        if (IsStraight(Openings)) continue;
        // A bend or junction needs a straight cell before the next shaped corridor.
        for (int32 Side = 0; Side < UE_ARRAY_COUNT(Directions); ++Side)
        {
            const FIntPoint Neighbor = Tile + Directions[Side];
            if ((Openings & (1 << Side)) && Plan.Corridors.Contains(Neighbor)
                && !IsStraight(CorridorOpenings(Plan, Neighbor)))
            {
                return false;
            }
        }
    }
    return HasSpacedCorners(Plan);
}

bool TryCarve(FPlan& Plan, const FRoomTiles& RoomTiles, const TArray<FIntPoint>& Path, TArray<FIntPoint>* Added = nullptr)
{
    TArray<FIntPoint> NewTiles;
    for (FIntPoint Tile : Path)
    {
        if (!RoomTiles.Contains(Tile) && !Plan.Corridors.Contains(Tile))
        {
            Plan.Corridors.Add(Tile);
            NewTiles.Add(Tile);
        }
    }
    if (!HasCorridorModuleSpace(Plan))
    {
        for (FIntPoint Tile : NewTiles) Plan.Corridors.Remove(Tile);
        return false;
    }
    if (Added) Added->Append(NewTiles);
    return true;
}

bool ConnectRooms(FPlan& Plan, const FRoomTiles& RoomTiles, const TSet<FIntPoint>& Reserved,
    int32 A, int32 B, FRandomStream& Random, TArray<FIntPoint>* Added = nullptr)
{
    const bool bXFirst = Random.RandRange(0, 1) == 0;
    for (bool bBend : {bXFirst, !bXFirst})
    {
        const TArray<FIntPoint> Path = MakeL(Plan.Rooms[A].Center, Plan.Rooms[B].Center, bBend);
        if (CanCarve(Plan, Path, RoomTiles, Reserved, A, B, nullptr) && TryCarve(Plan, RoomTiles, Path, Added))
        {
            const FRoom& Start = Plan.Rooms[A];
            const FRoom& End = Plan.Rooms[B];
            const FIntPoint Delta = End.Center - Start.Center;
            const FIntPoint Direction(FMath::Sign(Delta.X), FMath::Sign(Delta.Y));
            if (FMath::Abs(Direction.X) + FMath::Abs(Direction.Y) == 1)
            {
                const FIntPoint First = DoorTile(Start, Direction);
                const FIntPoint Second = DoorTile(End, Direction * -1);
                if (First + Direction == Second)
                {
                    Plan.DirectRoomDoors.Add(First);
                    Plan.DirectRoomDoors.Add(Second);
                }
            }
            return true;
        }
    }
    const TArray<FIntPoint> Path = RouteBetweenPorts(Plan, RoomTiles, Reserved, A, B);
    return !Path.IsEmpty() && TryCarve(Plan, RoomTiles, Path, Added);
}

bool JoinCorridor(FPlan& Plan, const FRoomTiles& RoomTiles, const TSet<FIntPoint>& Reserved,
    int32 Room, const TArray<FIntPoint>& Trunk, TArray<FIntPoint>& Added)
{
    const FIntPoint Start = Plan.Rooms[Room].Center;
    TArray<FIntPoint> Junctions = Trunk;
    Junctions.StableSort([&](FIntPoint A, FIntPoint B) { return Distance(Start, A) < Distance(Start, B); });
    for (FIntPoint Junction : Junctions)
    {
        const uint8 Ports = CorridorOpenings(Plan, Junction);
        if (Ports != 0b0101 && Ports != 0b1010) continue;
        const bool bHorizontal = Ports == 0b0101;
        const TArray<FIntPoint> Path = MakeL(Start, Junction, bHorizontal);
        if (Path.Num() >= 2 && (bHorizontal ? Path[Path.Num() - 2].X == Junction.X : Path[Path.Num() - 2].Y == Junction.Y)
            && CanCarve(Plan, Path, RoomTiles, Reserved, Room, INDEX_NONE, &Junction)
            && TryCarve(Plan, RoomTiles, Path, &Added))
        {
            return true;
        }
    }
    for (FIntPoint Junction : Junctions)
    {
        const uint8 Ports = CorridorOpenings(Plan, Junction);
        if (Ports != 0b0101 && Ports != 0b1010) continue;
        const TArray<FIntPoint> Path = RouteBetweenPorts(Plan, RoomTiles, Reserved, Room, INDEX_NONE, &Junction);
        if (!Path.IsEmpty() && TryCarve(Plan, RoomTiles, Path, &Added)) return true;
    }
    return false;
}

TSet<FIntPoint> Flood(const FPlan& Plan, const FRoomTiles& RoomTiles, const FIntPoint* ClosedGate = nullptr)
{
    TSet<FIntPoint> Reached{FIntPoint::ZeroValue};
    TArray<FIntPoint> Queue{FIntPoint::ZeroValue};
    for (int32 Index = 0; Index < Queue.Num(); ++Index)
    {
        const int32* CurrentRoom = RoomTiles.Find(Queue[Index]);
        for (FIntPoint Direction : Directions)
        {
            const FIntPoint Neighbor = Queue[Index] + Direction;
            const int32* NeighborRoom = RoomTiles.Find(Neighbor);
            if (CurrentRoom && NeighborRoom && *CurrentRoom != *NeighborRoom
                && !(Plan.DirectRoomDoors.Contains(Queue[Index]) && Plan.DirectRoomDoors.Contains(Neighbor)
                    && DoorTile(Plan.Rooms[*CurrentRoom], Direction) == Queue[Index]
                    && DoorTile(Plan.Rooms[*NeighborRoom], Direction * -1) == Neighbor))
            {
                continue;
            }
            if ((!ClosedGate || Neighbor != *ClosedGate) && Plan.Tiles.Contains(Neighbor) && !Reached.Contains(Neighbor))
            {
                Reached.Add(Neighbor);
                Queue.Add(Neighbor);
            }
        }
    }
    return Reached;
}

bool Build(FRandomStream& Random, FPlan& Plan)
{
    if (!PlaceRooms(Random, Plan.Rooms))
    {
        return false;
    }

    TArray<int32> Tree[RoomCount];
    BuildTree(Triangulate(Plan.Rooms, Random), Tree);
    TArray<int32> Order{0};
    int32 Parent[RoomCount] = {};
    int32 Depth[RoomCount] = {};
    int32 Boss = 0;
    for (int32 Index = 0; Index < Order.Num(); ++Index)
    {
        const int32 Room = Order[Index];
        if (Depth[Room] >= 3 && Tree[Room].Num() == 1 && Distance(Plan.Rooms[Room].Center, FIntPoint::ZeroValue)
            > Distance(Plan.Rooms[Boss].Center, FIntPoint::ZeroValue))
        {
            Boss = Room;
        }
        for (int32 Next : Tree[Room])
        {
            if (!Order.Contains(Next))
            {
                Parent[Next] = Room;
                Depth[Next] = Depth[Room] + 1;
                Order.Add(Next);
            }
        }
    }
    if (Order.Num() != RoomCount || Boss == 0)
    {
        return false;
    }

    const int32 MainTrap = Parent[Boss];
    int32 Guardian = MainTrap;
    while (Parent[Guardian] != 0)
    {
        Guardian = Parent[Guardian];
    }
    Plan.Rooms[Guardian].Role = ERoomRole::Guardian;
    Plan.Rooms[Boss].Role = ERoomRole::Boss;
    Plan.Rooms[Boss].HalfSize = BossHalfSize;
    Plan.Rooms[MainTrap].Role = Random.RandRange(0, 1) == 0 ? ERoomRole::Spikes : ERoomRole::Poison;

    // The final unassigned room hosts the cache.
    FRoom* Cache = Plan.Rooms.FindByPredicate([](const FRoom& Room) { return Room.Role == ERoomRole::Passage; });
    if (!Cache)
    {
        return false;
    }
    Cache->Role = ERoomRole::Cache;

    FRoomTiles RoomTiles;
    for (int32 Index = 0; Index < RoomCount; ++Index)
    {
        IndexRoom(RoomTiles, Plan.Rooms[Index], Index);
    }
    TSet<FIntPoint> Reserved;
    if (!PlaceExit(Plan, Boss, RoomTiles, Reserved))
    {
        return false;
    }

    TArray<FIntPoint> Incoming[RoomCount];
    for (int32 Index = 1; Index < Order.Num(); ++Index)
    {
        const int32 Room = Order[Index];
        const int32 From = Parent[Room];
        const bool bPreserveEncounterRoute = From == Guardian || From == MainTrap;
        const FIntPoint Delta = Plan.Rooms[Room].Center - Plan.Rooms[From].Center;
        const bool bShortAligned = (Delta.X == 0 || Delta.Y == 0)
            && Distance(Delta, FIntPoint::ZeroValue) <= 4;
        if (!bShortAligned && !bPreserveEncounterRoute
            && JoinCorridor(Plan, RoomTiles, Reserved, Room, Incoming[From], Incoming[Room]))
        {
            continue;
        }
        if (!ConnectRooms(Plan, RoomTiles, Reserved, From, Room, Random, &Incoming[Room])
            && !(bShortAligned && !bPreserveEncounterRoute
                && JoinCorridor(Plan, RoomTiles, Reserved, Room, Incoming[From], Incoming[Room])))
        {
            return false;
        }
    }

    Plan.Corridors.Append(Reserved);
    if (!HasCorridorModuleSpace(Plan)) return false;
    Plan.Tiles = Plan.Corridors;
    for (const TPair<FIntPoint, int32>& Tile : RoomTiles)
    {
        Plan.Tiles.Add(Tile.Key);
    }
    for (FIntPoint Tile : Plan.Corridors)
    {
        const uint8 Openings = CorridorOpenings(Plan, Tile);
        const int32 Degree = FMath::CountBits(static_cast<uint32>(Openings));
        Plan.Corners += Degree == 2 && !IsStraight(Openings);
        Plan.Junctions += Degree == 3;
    }

    const TSet<FIntPoint> BeforeGate = Flood(Plan, RoomTiles, &Plan.GateTile);
    if (BeforeGate.Contains(Plan.Rooms.Last().Center)
        || Flood(Plan, RoomTiles).Num() != Plan.Tiles.Num())
    {
        return false;
    }
    for (int32 Room = 0; Room < RoomCount; ++Room)
    {
        if (!BeforeGate.Contains(Plan.Rooms[Room].Center))
        {
            return false;
        }
    }
    return true;
}

int32 CorridorCost(const FPlan& Plan)
{
    // Count the space actually routed outside rooms, including direct links across neighboring doors.
    return Plan.Corridors.Num() + Plan.DirectRoomDoors.Num() / 2 + (Plan.Corners == 0 ? 1 : 0);
}
}

bool Generate(int32 Seed, FPlan& OutPlan)
{
    bool bFound = false;
    FRandomStream Random(Seed);
    for (int32 Attempt = 0; Attempt < 64; ++Attempt)
    {
        FPlan Candidate;
        if (Build(Random, Candidate) && (!bFound || CorridorCost(Candidate) < CorridorCost(OutPlan)))
        {
            OutPlan = MoveTemp(Candidate);
            bFound = true;
        }
    }
    return bFound;
}
}
