#include "FSDungeonLayout.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
constexpr double TileSize = 240.0;
constexpr double WallThickness = 24.0;
constexpr double WallHeight = 460.0;
const FName LegacyGeometryTag(TEXT("FS_LegacyDungeonGeometry"));
const FName LayoutAttemptedTag(TEXT("FS_DungeonLayoutAttempted"));
const FName GateTag(TEXT("ExitGate"));
const FName FirstEncounterTag(TEXT("Enc1"));
const FName FinalEncounterTag(TEXT("Enc2"));

struct FRoom
{
    FIntPoint Center;
    int32 HalfWidth;
    int32 HalfDepth;
};

void AddRoom(TSet<FIntPoint>& Tiles, const FRoom& Room)
{
    for (int32 X = -Room.HalfWidth; X <= Room.HalfWidth; ++X)
    {
        for (int32 Y = -Room.HalfDepth; Y <= Room.HalfDepth; ++Y)
        {
            Tiles.Add(Room.Center + FIntPoint(X, Y));
        }
    }
}

void AddCorridor(TSet<FIntPoint>& Tiles, const FRoom& From, const FRoom& To)
{
    const int32 BendX = (From.Center.X + From.HalfWidth + To.Center.X - To.HalfWidth) / 2;
    for (int32 X = From.Center.X; X <= BendX; ++X)
    {
        Tiles.Add(FIntPoint(X, From.Center.Y));
    }
    for (int32 Y = FMath::Min(From.Center.Y, To.Center.Y); Y <= FMath::Max(From.Center.Y, To.Center.Y); ++Y)
    {
        Tiles.Add(FIntPoint(BendX, Y));
    }
    for (int32 X = BendX; X <= To.Center.X; ++X)
    {
        Tiles.Add(FIntPoint(X, To.Center.Y));
    }
}

FVector TileCenter(const FIntPoint& Tile, double Height = 0.0)
{
    return FVector(Tile.X * TileSize, Tile.Y * TileSize, Height);
}

void AddBox(TArray<FTransform>& Boxes, const FVector& Center, const FVector& Dimensions)
{
    // The engine basic cube measures 100 cm per side.
    Boxes.Emplace(FRotator::ZeroRotator, Center, Dimensions / 100.0);
}

void AddProp(AActor& Owner, UStaticMesh* Mesh, const FVector& Base, double Yaw)
{
    if (!Mesh)
    {
        return;
    }
    UStaticMeshComponent* Prop = NewObject<UStaticMeshComponent>(&Owner);
    Prop->SetupAttachment(Owner.GetRootComponent());
    Prop->SetMobility(EComponentMobility::Movable);
    Prop->SetStaticMesh(Mesh);
    Prop->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    const FBoxSphereBounds Bounds = Mesh->GetBounds();
    Prop->SetRelativeLocation(Base - FVector(0, 0, Bounds.Origin.Z - Bounds.BoxExtent.Z));
    Prop->SetRelativeRotation(FRotator(0, Yaw, 0));
    Prop->RegisterComponent();
}

void AddLight(AActor& Owner, const FVector& Location, const FLinearColor& Color, float Intensity, float Radius)
{
    UPointLightComponent* Light = NewObject<UPointLightComponent>(&Owner);
    Light->SetupAttachment(Owner.GetRootComponent());
    Light->SetRelativeLocation(Location);
    Light->SetLightColor(Color);
    Light->SetIntensity(Intensity);
    Light->SetAttenuationRadius(Radius);
    Light->SetCastShadows(false);
    Light->RegisterComponent();
}
}

AFSDungeonLayout::AFSDungeonLayout()
{
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("LayoutRoot")));
    Floor = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Floor"));
    Walls = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Walls"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> StoneFloor(TEXT("/Game/Medieval_Sewer_Dungeon/Materials_Instance/MI_Large_Room_Floor.MI_Large_Room_Floor"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> StoneWall(TEXT("/Game/Medieval_Sewer_Dungeon/Materials_Instance/MI_Large_Wall.MI_Large_Wall"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Torch(TEXT("/Game/Medieval_Sewer_Dungeon/Static_Meshes/Props/SM_Torch_1.SM_Torch_1"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Crate(TEXT("/Game/Medieval_Sewer_Dungeon/Static_Meshes/Props/SM_Crate.SM_Crate"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Barrel(TEXT("/Game/Medieval_Sewer_Dungeon/Static_Meshes/Props/SM_Barrel.SM_Barrel"));
    TorchMesh = Torch.Object;
    CrateMesh = Crate.Object;
    BarrelMesh = Barrel.Object;
    for (UInstancedStaticMeshComponent* Component : {Floor.Get(), Walls.Get()})
    {
        Component->SetupAttachment(GetRootComponent());
        Component->SetMobility(EComponentMobility::Movable);
        Component->SetStaticMesh(Cube.Object);
        Component->SetCollisionProfileName(TEXT("BlockAll"));
        Component->SetGenerateOverlapEvents(false);
        Component->SetCanEverAffectNavigation(false);
    }
    if (StoneFloor.Succeeded())
    {
        Floor->SetMaterial(0, StoneFloor.Object);
    }
    if (StoneWall.Succeeded())
    {
        Walls->SetMaterial(0, StoneWall.Object);
    }
}

void AFSDungeonLayout::GenerateForContract(AActor* Player, const FString& QuestId, int32 RunId)
{
    if (!IsValid(Player) || Player->ActorHasTag(LayoutAttemptedTag)
        || QuestId != TEXT("Q_ForgottenCrypt") || RunId <= 0
        || UGameplayStatics::GetCurrentLevelName(Player) != TEXT("L_Dungeon_01"))
    {
        return;
    }

    Player->Tags.Add(LayoutAttemptedTag);
    UWorld* World = Player->GetWorld();
    if (TActorIterator<AFSDungeonLayout>(World))
    {
        return;
    }

    AFSDungeonLayout* Layout = World->SpawnActor<AFSDungeonLayout>();
    if (Layout && !Layout->Build(RunId))
    {
        Layout->Destroy();
        UE_LOG(LogTemp, Warning, TEXT("Forgotten Crypt run %d: layout unavailable, using the authored route."), RunId);
    }
}

bool AFSDungeonLayout::HasReachedExit(const AActor* Player)
{
    if (!IsValid(Player))
    {
        return false;
    }

    const FVector Position = Player->GetActorLocation();
    if (TActorIterator<AFSDungeonLayout> Layout(Player->GetWorld()); Layout)
    {
        const FVector Offset = Position - Layout->ExitCenter;
        return FMath::Abs(Offset.X) <= 155.0 && FMath::Abs(Offset.Y) <= 155.0 && FMath::Abs(Offset.Z) <= 180.0;
    }

    return Position.X > 2400.0;
}

bool AFSDungeonLayout::Build(int32 RunId)
{
    TArray<AActor*> LegacyGeometry;
    AStaticMeshActor* Gate = nullptr;
    ACharacter* Guardian = nullptr;
    TArray<ACharacter*> FinalEnemies;
    for (TActorIterator<AActor> Actor(GetWorld()); Actor; ++Actor)
    {
        if (Actor->ActorHasTag(LegacyGeometryTag))
        {
            LegacyGeometry.Add(*Actor);
        }
        if (Actor->ActorHasTag(GateTag))
        {
            Gate = Cast<AStaticMeshActor>(*Actor);
        }
        if (ACharacter* Enemy = Cast<ACharacter>(*Actor))
        {
            if (Enemy->ActorHasTag(FirstEncounterTag))
            {
                Guardian = Enemy;
            }
            if (Enemy->ActorHasTag(FinalEncounterTag))
            {
                FinalEnemies.Add(Enemy);
            }
        }
    }
    if (!Gate || !Guardian || FinalEnemies.Num() != 2 || LegacyGeometry.IsEmpty()
        || !Floor->GetStaticMesh() || !Walls->GetStaticMesh())
    {
        return false;
    }
    FinalEnemies.Sort([](const ACharacter& A, const ACharacter& B)
    {
        return A.GetActorLocation().X < B.GetActorLocation().X;
    });

    FRandomStream Random(RunId);
    const int32 Side = RunId % 2 == 0 ? -1 : 1;
    const int32 GuardianX = Random.RandRange(7, 8);
    const int32 FinalX = GuardianX + Random.RandRange(8, 9);
    const FRoom Rooms[] = {
        {FIntPoint(0, 0), 2, 2},
        {FIntPoint(GuardianX, Side * Random.RandRange(2, 4)), 2, Random.RandRange(2, 3)},
        {FIntPoint(FinalX, -Side * Random.RandRange(2, 4)), 2, Random.RandRange(2, 3)},
        {FIntPoint(FinalX + 7, 0), 1, 1},
    };

    // Consecutive rooms are connected by construction. Boundary walls follow
    // the union of room and corridor tiles, leaving every entrance open.
    TSet<FIntPoint> Tiles;
    AddRoom(Tiles, Rooms[0]);
    for (int32 Index = 1; Index < UE_ARRAY_COUNT(Rooms); ++Index)
    {
        AddRoom(Tiles, Rooms[Index]);
        AddCorridor(Tiles, Rooms[Index - 1], Rooms[Index]);
    }

    TArray<FTransform> FloorBoxes;
    TArray<FTransform> WallBoxes;
    const FIntPoint Neighbors[] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (const FIntPoint& Tile : Tiles)
    {
        AddBox(FloorBoxes, TileCenter(Tile, -20.0), FVector(TileSize, TileSize, 40.0));
        for (const FIntPoint& Neighbor : Neighbors)
        {
            if (!Tiles.Contains(Tile + Neighbor))
            {
                const FVector EdgeOffset(Neighbor.X * TileSize / 2.0, Neighbor.Y * TileSize / 2.0, 0.0);
                const FVector WallSize = Neighbor.X == 0
                    ? FVector(TileSize + WallThickness, WallThickness, WallHeight)
                    : FVector(WallThickness, TileSize + WallThickness, WallHeight);
                AddBox(WallBoxes, TileCenter(Tile, WallHeight / 2.0) + EdgeOffset, WallSize);
            }
        }
    }

    Floor->AddInstances(FloorBoxes, false);
    Walls->AddInstances(WallBoxes, false);

    for (int32 Index = 0; Index < 3; ++Index)
    {
        const FRoom& Room = Rooms[Index];
        const FVector Corner = TileCenter(Room.Center + FIntPoint(-Room.HalfWidth, Room.HalfDepth));
        AddProp(*this, TorchMesh, Corner + FVector(0, 65, 105), 180);
        AddLight(*this, Corner + FVector(0, 0, 200), FLinearColor(1.0f, 0.44f, 0.16f), 18000.0f, 980.0f);
        AddProp(*this, Index == 1 ? BarrelMesh.Get() : CrateMesh.Get(), Corner + FVector(60, -10, 0), 20);
    }

    const FIntPoint GuardianTile = Rooms[1].Center + FIntPoint(1, Random.RandRange(-1, 1));
    const int32 FinalLane = Random.RandRange(-1, 1);
    Guardian->SetActorLocation(TileCenter(GuardianTile, 100.0), false, nullptr, ETeleportType::TeleportPhysics);
    FinalEnemies[0]->SetActorLocation(TileCenter(Rooms[2].Center + FIntPoint(0, FinalLane), 100.0), false, nullptr, ETeleportType::TeleportPhysics);
    FinalEnemies[1]->SetActorLocation(TileCenter(Rooms[2].Center + FIntPoint(1, FinalLane), 100.0), false, nullptr, ETeleportType::TeleportPhysics);

    const FIntPoint ExitTile = Rooms[3].Center;
    ExitCenter = TileCenter(ExitTile, 90.0);
    Gate->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
    Gate->SetActorLocation(TileCenter(ExitTile - FIntPoint(2, 0), WallHeight / 2.0), false, nullptr, ETeleportType::TeleportPhysics);
    Gate->SetActorScale3D(FVector(0.35, TileSize / 100.0, WallHeight / 100.0));

    UTextRenderComponent* ExitLabel = NewObject<UTextRenderComponent>(this);
    ExitLabel->SetupAttachment(GetRootComponent());
    ExitLabel->SetRelativeLocation(ExitCenter + FVector(0, 0, 105));
    ExitLabel->SetRelativeRotation(FRotator(0, 180, 0));
    ExitLabel->SetHorizontalAlignment(EHTA_Center);
    ExitLabel->SetWorldSize(55);
    ExitLabel->SetText(FText::FromString(TEXT("RETURN TO GUILD")));
    ExitLabel->RegisterComponent();

    AddLight(*this, ExitCenter + FVector(0, 0, 100), FLinearColor(0.1f, 0.65f, 1.0f), 14000.0f, 850.0f);

    for (AActor* Actor : LegacyGeometry)
    {
        Actor->SetActorHiddenInGame(true);
        Actor->SetActorEnableCollision(false);
    }
    UE_LOG(LogTemp, Display, TEXT("Forgotten Crypt run %d: %d tiles, guardian (%d,%d), final room (%d,%d)."),
        RunId, Tiles.Num(), GuardianTile.X, GuardianTile.Y, FinalX, Rooms[2].Center.Y);
    return true;
}
