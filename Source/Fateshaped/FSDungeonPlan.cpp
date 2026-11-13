#include "FSDungeonPlan.h"

#include "CompGeom/Delaunay2.h"

namespace FSDungeon
{
namespace
{
constexpr int32 RoomCount = 8;
constexpr int32 RoomMargin = 3;

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
    Rooms.Add({FIntPoint::ZeroValue, FIntPoint(2, 2), ERoomRole::Entrance});
    for (int32 Attempt = 0; Attempt < 220 && Rooms.Num() < RoomCount; ++Attempt)
    {
        const FRoom Candidate{{Random.RandRange(0, 31), Random.RandRange(-15, 15)},
            {Random.RandRange(2, 3), Random.RandRange(2, 3)}};
        if (Fits(Rooms, Candidate, RoomMargin))
        {
            Rooms.Add(Candidate);
        }
    }
    return Rooms.Num() == RoomCount;
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
        Result.Add({Edge.X, Edge.Y, Delta.X * Delta.X + Delta.Y * Delta.Y});
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

void SplitTreeAndShortcuts(const TArray<FEdge>& Edges, TArray<int32> (&Tree)[RoomCount], TArray<FEdge>& Shortcuts)
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
        if (A == B)
        {
            Shortcuts.Add(Edge);
            continue;
        }
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
        const int32 Wall = Direction.X ? Arena.HalfSize.X : Arena.HalfSize.Y;
        const FIntPoint Door = Arena.Center + Direction * (Wall + 1);
        const FRoom Exit{Door + Direction * 4, FIntPoint(1, 1), ERoomRole::Exit};
        if (!Fits(Plan.Rooms, Exit, 1))
        {
            continue;
        }

        TSet<FIntPoint> Throat;
        for (int32 Step = 0; Step < 3; ++Step)
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
        Plan.GateTile = Door + Direction;
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

bool CanCarve(const TArray<FIntPoint>& Path, const FRoomTiles& RoomTiles, const TSet<FIntPoint>& Corridors,
    const TSet<FIntPoint>& Reserved, int32 A, int32 B, const FIntPoint* Junction)
{
    for (int32 Index = 0; Index < Path.Num(); ++Index)
    {
        const FIntPoint Tile = Path[Index];
        if (Junction && Tile == *Junction)
        {
            continue;
        }
        if (!IsAllowedRoom(RoomTiles, Tile, A, B) || Corridors.Contains(Tile) || Reserved.Contains(Tile))
        {
            return false;
        }
        if (RoomTiles.Contains(Tile))
        {
            continue;
        }

        for (FIntPoint Direction : Directions)
        {
            const FIntPoint Neighbor = Tile + Direction;
            if (!IsAllowedRoom(RoomTiles, Neighbor, A, B) || Reserved.Contains(Neighbor))
            {
                return false;
            }
            if (Corridors.Contains(Neighbor) && !(Junction && Index == Path.Num() - 2 && Neighbor == *Junction))
            {
                return false;
            }
        }
    }
    return true;
}

void Carve(FPlan& Plan, const FRoomTiles& RoomTiles, const TArray<FIntPoint>& Path, TArray<FIntPoint>* Added = nullptr)
{
    for (FIntPoint Tile : Path)
    {
        if (!RoomTiles.Contains(Tile) && !Plan.Corridors.Contains(Tile))
        {
            Plan.Corridors.Add(Tile);
            if (Added)
            {
                Added->Add(Tile);
            }
        }
    }
}

bool ConnectRooms(FPlan& Plan, const FRoomTiles& RoomTiles, const TSet<FIntPoint>& Reserved,
    int32 A, int32 B, FRandomStream& Random, TArray<FIntPoint>* Added = nullptr)
{
    const bool bXFirst = Random.RandRange(0, 1) == 0;
    for (bool bBend : {bXFirst, !bXFirst})
    {
        const TArray<FIntPoint> Path = MakeL(Plan.Rooms[A].Center, Plan.Rooms[B].Center, bBend);
        if (CanCarve(Path, RoomTiles, Plan.Corridors, Reserved, A, B, nullptr))
        {
            Carve(Plan, RoomTiles, Path, Added);
            return true;
        }
    }
    return false;
}

bool JoinCorridor(FPlan& Plan, const FRoomTiles& RoomTiles, const TSet<FIntPoint>& Reserved,
    int32 Room, const TArray<FIntPoint>& Trunk, TArray<FIntPoint>& Added)
{
    const FIntPoint Start = Plan.Rooms[Room].Center;
    for (FIntPoint Junction : Trunk)
    {
        const auto IsOpen = [&](FIntPoint Direction)
        {
            return Plan.Corridors.Contains(Junction + Direction) || RoomTiles.Contains(Junction + Direction);
        };
        const bool bHorizontal = IsOpen(Directions[0]) && IsOpen(Directions[2]) && !IsOpen(Directions[1]) && !IsOpen(Directions[3]);
        const bool bVertical = IsOpen(Directions[1]) && IsOpen(Directions[3]) && !IsOpen(Directions[0]) && !IsOpen(Directions[2]);
        if (!bHorizontal && !bVertical)
        {
            continue;
        }
        const TArray<FIntPoint> Path = MakeL(Start, Junction, bHorizontal);
        if (Path.Num() >= 2 && (bHorizontal ? Path[Path.Num() - 2].X == Junction.X : Path[Path.Num() - 2].Y == Junction.Y)
            && CanCarve(Path, RoomTiles, Plan.Corridors, Reserved, Room, INDEX_NONE, &Junction))
        {
            Carve(Plan, RoomTiles, Path, &Added);
            return true;
        }
    }
    return false;
}

TSet<FIntPoint> Flood(const TSet<FIntPoint>& Tiles, const FIntPoint* ClosedGate = nullptr)
{
    TSet<FIntPoint> Reached{FIntPoint::ZeroValue};
    TArray<FIntPoint> Queue{FIntPoint::ZeroValue};
    for (int32 Index = 0; Index < Queue.Num(); ++Index)
    {
        for (FIntPoint Direction : Directions)
        {
            const FIntPoint Neighbor = Queue[Index] + Direction;
            if ((!ClosedGate || Neighbor != *ClosedGate) && Tiles.Contains(Neighbor) && !Reached.Contains(Neighbor))
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
    TArray<FEdge> Shortcuts;
    SplitTreeAndShortcuts(Triangulate(Plan.Rooms, Random), Tree, Shortcuts);
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
    Plan.Rooms[Boss].HalfSize = FIntPoint(3, 3);
    const bool bSpikesNearBoss = Random.RandRange(0, 1) == 0;
    Plan.Rooms[MainTrap].Role = bSpikesNearBoss ? ERoomRole::Spikes : ERoomRole::Poison;

    // Prefer dead ends for the cache and the second trap.
    TArray<int32> FreeRooms;
    for (int32 Room : Order)
    {
        if (Plan.Rooms[Room].Role == ERoomRole::Passage)
        {
            FreeRooms.Add(Room);
        }
    }
    FreeRooms.StableSort([&](int32 A, int32 B) { return Tree[A].Num() < Tree[B].Num(); });
    Plan.Rooms[FreeRooms[0]].Role = ERoomRole::Cache;
    Plan.Rooms[FreeRooms[1]].Role = bSpikesNearBoss ? ERoomRole::Poison : ERoomRole::Spikes;

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
        if (!bPreserveEncounterRoute && JoinCorridor(Plan, RoomTiles, Reserved, Room, Incoming[From], Incoming[Room]))
        {
            ++Plan.Junctions;
            continue;
        }
        if (!ConnectRooms(Plan, RoomTiles, Reserved, From, Room, Random, &Incoming[Room]))
        {
            return false;
        }
    }

    int32 Regions[RoomCount] = {};
    for (int32 Index = 1; Index < Order.Num(); ++Index)
    {
        const int32 Room = Order[Index];
        Regions[Room] = Regions[Parent[Room]] + (Room == Guardian || Room == MainTrap ? 1 : 0);
    }
    // Add a loop without skipping the guardian or the trap approach.
    for (int32 Index = Shortcuts.Num() - 1; Index > 0; --Index)
    {
        Shortcuts.Swap(Index, Random.RandRange(0, Index));
    }
    for (const FEdge& Edge : Shortcuts)
    {
        if (Regions[Edge.A] != Regions[Edge.B] || Edge.A == Boss || Edge.B == Boss
            || Plan.Rooms[Edge.A].Role == ERoomRole::Cache || Plan.Rooms[Edge.B].Role == ERoomRole::Cache)
        {
            continue;
        }
        if (ConnectRooms(Plan, RoomTiles, Reserved, Edge.A, Edge.B, Random))
        {
            ++Plan.Shortcuts;
            break;
        }
    }
    if (Plan.Shortcuts == 0)
    {
        return false;
    }

    Plan.Corridors.Append(Reserved);
    Plan.Tiles = Plan.Corridors;
    for (const TPair<FIntPoint, int32>& Tile : RoomTiles)
    {
        Plan.Tiles.Add(Tile.Key);
    }
    for (FIntPoint Tile : Plan.Corridors)
    {
        const int32 Horizontal = Plan.Tiles.Contains(Tile + Directions[0]) + Plan.Tiles.Contains(Tile + Directions[2]);
        const int32 Vertical = Plan.Tiles.Contains(Tile + Directions[1]) + Plan.Tiles.Contains(Tile + Directions[3]);
        Plan.Corners += Horizontal == 1 && Vertical == 1;
    }

    const TSet<FIntPoint> BeforeGate = Flood(Plan.Tiles, &Plan.GateTile);
    if (Plan.Corners == 0 || Plan.Junctions == 0 || BeforeGate.Contains(Plan.Rooms.Last().Center)
        || Flood(Plan.Tiles).Num() != Plan.Tiles.Num())
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
}

bool Generate(int32 Seed, FPlan& OutPlan)
{
    FRandomStream Random(Seed);
    for (int32 Attempt = 0; Attempt < 96; ++Attempt)
    {
        FPlan Candidate;
        if (Build(Random, Candidate))
        {
            OutPlan = MoveTemp(Candidate);
            return true;
        }
    }
    return false;
}
}
