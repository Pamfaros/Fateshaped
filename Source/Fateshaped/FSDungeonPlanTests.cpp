#include "FSDungeonPlan.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSDungeonPlanTest, "Fateshaped.Dungeon.SeededPlans",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFSDungeonPlanTest::RunTest(const FString& Parameters)
{
    using namespace FSDungeon;
    TSet<FIntPoint> BossLocations;
    bool bSawSpikes = false;
    bool bSawPoison = false;
    for (int32 Seed = 1; Seed <= 256; ++Seed)
    {
        FPlan Plan;
        if (!TestTrue(FString::Printf(TEXT("Seed %d generates a connected plan"), Seed), Generate(Seed, Plan)))
        {
            continue;
        }
        TestEqual(TEXT("Room count including the exit"), Plan.Rooms.Num(), 6);
        TestTrue(TEXT("Two straight tiles separate consecutive corridor bends"), HasSpacedCorners(Plan));

        TSet<ERoomRole> Roles;
        for (const FRoom& Room : Plan.Rooms)
        {
            Roles.Add(Room.Role);
            TestTrue(TEXT("Room center is inside the dense placement field"),
                FMath::Abs(Room.Center.X) <= RoomFieldHalfExtent && FMath::Abs(Room.Center.Y) <= RoomFieldHalfExtent);
            const FIntPoint ExpectedHalfSize = Room.Role == ERoomRole::Boss ? BossHalfSize
                : Room.Role == ERoomRole::Exit ? ExitHalfSize : NormalHalfSize;
            TestTrue(TEXT("Room matches its authored module footprint"), Room.HalfSize == ExpectedHalfSize);
            TestTrue(TEXT("Room has a connected doorway"), RoomOpenings(Plan, Room) != 0);
            if (Room.Role == ERoomRole::Boss)
            {
                BossLocations.Add(Room.Center);
            }
        }
        for (FIntPoint Tile : Plan.Corridors)
        {
            const uint8 Ports = CorridorOpenings(Plan, Tile);
            TestNotEqual(TEXT("A corridor has at most three exits"), Ports, static_cast<uint8>(0b1111));
            for (int32 Side = 0; Side < UE_ARRAY_COUNT(Directions); ++Side)
            {
                TestEqual(TEXT("Corridor adjacency agrees with the assembled door openings"),
                    (Ports & (1 << Side)) != 0, Plan.Tiles.Contains(Tile + Directions[Side]));
            }
        }
        TestTrue(TEXT("All required room roles occur exactly once"), Roles.Num() == 6 && Roles.Contains(ERoomRole::Entrance)
            && Roles.Contains(ERoomRole::Guardian) && Roles.Contains(ERoomRole::Boss)
            && Roles.Contains(ERoomRole::Cache) && Roles.Contains(ERoomRole::Exit));
        const bool bHasSpikes = Roles.Contains(ERoomRole::Spikes);
        const bool bHasPoison = Roles.Contains(ERoomRole::Poison);
        TestTrue(TEXT("Exactly one trap room occurs per run"), bHasSpikes != bHasPoison);
        bSawSpikes |= bHasSpikes;
        bSawPoison |= bHasPoison;
    }
    TestTrue(TEXT("The final encounter moves across runs"), BossLocations.Num() > 10);
    TestTrue(TEXT("Both trap types occur across different seeds"), bSawSpikes && bSawPoison);

    FPlan First;
    FPlan Repeat;
    if (Generate(42, First) && Generate(42, Repeat))
    {
        TestEqual(TEXT("Same seed has the same number of tiles"), Repeat.Tiles.Num(), First.Tiles.Num());
        for (FIntPoint Tile : First.Tiles)
        {
            TestTrue(TEXT("Same seed has the same floor tile"), Repeat.Tiles.Contains(Tile));
        }
        for (int32 Index = 0; Index < First.Rooms.Num(); ++Index)
        {
            TestTrue(TEXT("Same seed has the same room role and location"),
                First.Rooms[Index].Center == Repeat.Rooms[Index].Center && First.Rooms[Index].Role == Repeat.Rooms[Index].Role);
        }
    }
    FTrapMaze Maze;
    TestTrue(TEXT("Random walk lays out hidden trap cells"), BuildTrapMaze(7, 0b0101, 120.0, 120.0, 240.0, Maze));
    int32 TrapCount = 0;
    int32 SafeCount = 0;
    FIntPoint SafeStart = FIntPoint::ZeroValue;
    for (int32 Y = 0; Y < FTrapMaze::Grid; ++Y)
    {
        for (int32 X = 0; X < FTrapMaze::Grid; ++X)
        {
            if (Maze.bTrap[X][Y]) ++TrapCount;
            else
            {
                if (SafeCount == 0) SafeStart = FIntPoint(X, Y);
                ++SafeCount;
            }
        }
    }
    for (int32 X = 0; X < FTrapMaze::Grid; ++X)
    {
        TestTrue(TEXT("A closed side stays trapped"), Maze.bTrap[X][0]);
    }
    TestTrue(TEXT("The floor keeps hidden traps and a safe route"), TrapCount > 0 && SafeCount > 1);
    TestFalse(TEXT("Door cells are safe"), Maze.bTrap[0][3] || Maze.bTrap[0][4] || Maze.bTrap[7][3] || Maze.bTrap[7][4]);
    bool Seen[FTrapMaze::Grid][FTrapMaze::Grid] = {};
    TArray<FIntPoint> Pending;
    Pending.Add(SafeStart);
    Seen[SafeStart.X][SafeStart.Y] = true;
    int32 Connected = 0;
    while (!Pending.IsEmpty())
    {
        const FIntPoint Cell = Pending.Pop(EAllowShrinking::No);
        ++Connected;
        const FIntPoint Steps[] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
        for (const FIntPoint Step : Steps)
        {
            const FIntPoint Next = Cell + Step;
            if (Next.X < 0 || Next.X >= FTrapMaze::Grid || Next.Y < 0 || Next.Y >= FTrapMaze::Grid
                || Seen[Next.X][Next.Y] || Maze.bTrap[Next.X][Next.Y])
            {
                continue;
            }
            Seen[Next.X][Next.Y] = true;
            Pending.Add(Next);
        }
    }
    TestEqual(TEXT("Safe cells form one path"), Connected, SafeCount);
    FTrapMaze SeededAgain;
    const bool bSameSeed = BuildTrapMaze(7, 0b0101, 120.0, 120.0, 240.0, SeededAgain);
    TestTrue(TEXT("The same seed builds the same hidden traps"), bSameSeed
        && FMemory::Memcmp(Maze.bTrap, SeededAgain.bTrap, sizeof(Maze.bTrap)) == 0);
    return true;
}
#endif
