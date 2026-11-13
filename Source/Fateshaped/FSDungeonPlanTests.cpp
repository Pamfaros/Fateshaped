#include "FSDungeonPlan.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSDungeonPlanTest, "Fateshaped.Dungeon.SeededPlans",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFSDungeonPlanTest::RunTest(const FString& Parameters)
{
    using namespace FSDungeon;
    TSet<FIntPoint> BossLocations;
    for (int32 Seed = 1; Seed <= 256; ++Seed)
    {
        FPlan Plan;
        if (!TestTrue(FString::Printf(TEXT("Seed %d generates a connected plan"), Seed), Generate(Seed, Plan)))
        {
            continue;
        }
        TestEqual(TEXT("Room count"), Plan.Rooms.Num(), 9);
        TestTrue(TEXT("Orthogonal bends"), Plan.Corners > 0);
        TestTrue(TEXT("T junctions"), Plan.Junctions > 0);
        TestEqual(TEXT("Optional loop"), Plan.Shortcuts, 1);

        TSet<ERoomRole> Roles;
        for (const FRoom& Room : Plan.Rooms)
        {
            Roles.Add(Room.Role);
            if (Room.Role == ERoomRole::Boss)
            {
                BossLocations.Add(Room.Center);
            }
        }
        TestTrue(TEXT("Both trap types occur"), Roles.Contains(ERoomRole::Spikes) && Roles.Contains(ERoomRole::Poison));
    }
    TestTrue(TEXT("The final encounter moves across runs"), BossLocations.Num() > 10);

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
    return true;
}
#endif
