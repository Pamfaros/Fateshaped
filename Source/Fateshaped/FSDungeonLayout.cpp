#include "FSDungeonLayout.h"
#include "FSDungeonPlan.h"

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
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
constexpr double TileSize = 240.0;
constexpr double WallThickness = 24.0;
constexpr double WallHeight = 460.0;
using FSDungeon::ERoomRole;
using FSDungeon::FRoom;
const FName LegacyGeometryTag(TEXT("FS_LegacyDungeonGeometry"));
const FName LayoutAttemptedTag(TEXT("FS_DungeonLayoutAttempted"));
const FName GateTag(TEXT("ExitGate"));
const FName FirstEncounterTag(TEXT("Enc1"));
const FName FinalEncounterTag(TEXT("Enc2"));

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

UPointLightComponent* AddLight(AActor& Owner, const FVector& Location, const FLinearColor& Color, float Intensity, float Radius)
{
    UPointLightComponent* Light = NewObject<UPointLightComponent>(&Owner);
    Light->SetupAttachment(Owner.GetRootComponent());
    Light->SetRelativeLocation(Location);
    Light->SetLightColor(Color);
    Light->SetIntensity(Intensity);
    Light->SetAttenuationRadius(Radius);
    Light->SetCastShadows(false);
    Light->RegisterComponent();
    return Light;
}

UTextRenderComponent* AddLabel(AActor& Owner, FName Name, const FVector& Location, double Yaw, const FString& Text)
{
    UTextRenderComponent* Label = NewObject<UTextRenderComponent>(&Owner, Name);
    Label->SetupAttachment(Owner.GetRootComponent());
    Label->SetRelativeLocation(Location);
    Label->SetRelativeRotation(FRotator(0, Yaw, 0));
    Label->SetHorizontalAlignment(EHTA_Center);
    Label->SetWorldSize(44);
    Label->SetText(FText::FromString(Text));
    Label->RegisterComponent();
    return Label;
}
}

AFSDungeonLayout::AFSDungeonLayout()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    PrimaryActorTick.TickInterval = 0.08f;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("LayoutRoot")));
    Floor = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Floor"));
    Walls = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Walls"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> StoneFloor(TEXT("/Game/Medieval_Sewer_Dungeon/Materials_Instance/MI_Large_Room_Floor.MI_Large_Room_Floor"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> StoneWall(TEXT("/Game/Medieval_Sewer_Dungeon/Materials_Instance/MI_Large_Wall.MI_Large_Wall"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Torch(TEXT("/Game/Medieval_Sewer_Dungeon/Static_Meshes/Props/SM_Torch_1.SM_Torch_1"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Crate(TEXT("/Game/Medieval_Sewer_Dungeon/Static_Meshes/Props/SM_Crate.SM_Crate"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Barrel(TEXT("/Game/Medieval_Sewer_Dungeon/Static_Meshes/Props/SM_Barrel.SM_Barrel"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Chest(TEXT("/Game/Medieval_Sewer_Dungeon/Static_Meshes/Props/SM_Chest.SM_Chest"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Pipe(TEXT("/Game/Medieval_Sewer_Dungeon/Static_Meshes/Props/SM_Pipe.SM_Pipe"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Telegraph(TEXT("/Game/Combat/FX/MI_FX_Telegraph.MI_FX_Telegraph"));
    TorchMesh = Torch.Object;
    CrateMesh = Crate.Object;
    BarrelMesh = Barrel.Object;
    ChestMesh = Chest.Object;
    PipeMesh = Pipe.Object;
    SpikeMesh = Cone.Object;
    FumeMesh = Sphere.Object;
    TelegraphMaterial = Telegraph.Object;
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

    FSDungeon::FPlan Plan;
    if (!FSDungeon::Generate(RunId, Plan))
    {
        return false;
    }

    TArray<FTransform> FloorBoxes;
    TArray<FTransform> WallBoxes;
    for (const FIntPoint Tile : Plan.Tiles)
    {
        AddBox(FloorBoxes, TileCenter(Tile, -20.0), FVector(TileSize, TileSize, 40.0));
        for (const FIntPoint Neighbor : FSDungeon::Directions)
        {
            if (!Plan.Tiles.Contains(Tile + Neighbor))
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

    FRandomStream Random(RunId);
    for (const FRoom& Room : Plan.Rooms)
    {
        const FVector Center = TileCenter(Room.Center);
        if (Room.Role == ERoomRole::Exit)
        {
            continue;
        }
        const FVector Corner = TileCenter(Room.Center + FIntPoint(-Room.HalfSize.X, Room.HalfSize.Y));
        AddProp(*this, TorchMesh, Corner + FVector(0, 65, 105), 180);
        AddLight(*this, Corner + FVector(0, 0, 200), FLinearColor(1.0f, 0.44f, 0.16f), 18000.0f, 980.0f);

        switch (Room.Role)
        {
        case ERoomRole::Guardian:
            Guardian->SetActorLocation(Center + FVector(0, 0, 100), false, nullptr, ETeleportType::TeleportPhysics);
            AddProp(*this, BarrelMesh, Corner + FVector(60, -10, 0), 20);
            break;
        case ERoomRole::Boss:
        {
            const int32 Lane = Random.RandRange(-1, 1);
            FinalEnemies[0]->SetActorLocation(TileCenter(Room.Center + FIntPoint(0, Lane), 100), false, nullptr, ETeleportType::TeleportPhysics);
            FinalEnemies[1]->SetActorLocation(TileCenter(Room.Center + FIntPoint(1, Lane), 100), false, nullptr, ETeleportType::TeleportPhysics);
            AddLight(*this, Center + FVector(0, 0, 270), FLinearColor(0.72f, 0.18f, 0.08f), 23000, 1100);
            break;
        }
        case ERoomRole::Spikes:
        case ERoomRole::Poison:
            AddTrap(Center, Room.Role == ERoomRole::Spikes, Random.RandRange(0, 1) == 0);
            if (Room.Role == ERoomRole::Poison)
            {
                AddProp(*this, PipeMesh, Corner + FVector(45, 0, 0), 90);
            }
            break;
        case ERoomRole::Cache:
            AddProp(*this, ChestMesh, Center + FVector(40, 0, 0), 180);
            AddProp(*this, CrateMesh, Corner + FVector(60, -10, 0), 20);
            AddLight(*this, Center + FVector(0, 0, 160), FLinearColor(1.0f, 0.72f, 0.24f), 14000, 600);
            break;
        default:
            AddProp(*this, CrateMesh, Corner + FVector(60, -10, 0), 20);
            break;
        }
    }

    ExitCenter = TileCenter(Plan.Rooms.Last().Center, 90.0);
    const FRotator GateRotation = TileCenter(Plan.ExitDirection).Rotation();
    Gate->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
    Gate->SetActorLocationAndRotation(TileCenter(Plan.GateTile, WallHeight / 2.0), GateRotation, false, nullptr, ETeleportType::TeleportPhysics);
    Gate->SetActorScale3D(FVector(0.35, TileSize / 100.0, WallHeight / 100.0));

    AddLabel(*this, TEXT("ExitLabel"), ExitCenter + FVector(0, 0, 105), GateRotation.Yaw + 180, TEXT("RETURN TO GUILD"));
    AddLight(*this, ExitCenter + FVector(0, 0, 100), FLinearColor(0.1f, 0.65f, 1.0f), 14000.0f, 850.0f);

    for (AActor* Actor : LegacyGeometry)
    {
        Actor->SetActorHiddenInGame(true);
        Actor->SetActorEnableCollision(false);
    }
    SetActorTickEnabled(true);
    UE_LOG(LogTemp, Display, TEXT("Forgotten Crypt run %d: %d rooms, %d tiles, %d L bends, %d T junctions, %d shortcuts, %d traps."),
        RunId, Plan.Rooms.Num(), Plan.Tiles.Num(), Plan.Corners, Plan.Junctions, Plan.Shortcuts, Traps.Num());
    return true;
}

void AFSDungeonLayout::AddTrap(const FVector& Center, bool bSpikes, bool bAlongX)
{
    FTrap& Trap = Traps.AddDefaulted_GetRef();
    Trap.Center = Center;
    Trap.bSpikes = bSpikes;
    Trap.HalfExtent = bSpikes ? FVector(180, 92, 55) : FVector(165, 165, 140);
    if (bSpikes && !bAlongX)
    {
        Swap(Trap.HalfExtent.X, Trap.HalfExtent.Y);
    }

    const FLinearColor Color = bSpikes ? FLinearColor(1.0f, 0.22f, 0.06f) : FLinearColor(0.24f, 1.0f, 0.28f);
    UStaticMeshComponent* Plate = NewObject<UStaticMeshComponent>(this);
    Plate->SetupAttachment(GetRootComponent());
    Plate->SetMobility(EComponentMobility::Movable);
    Plate->SetStaticMesh(Floor->GetStaticMesh());
    Plate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Plate->SetRelativeLocation(Center + FVector(0, 0, 2));
    Plate->SetRelativeScale3D(FVector(Trap.HalfExtent.X / 50, Trap.HalfExtent.Y / 50, 0.025));
    if (UMaterialInstanceDynamic* Material = Plate->CreateDynamicMaterialInstance(0, TelegraphMaterial))
    {
        Material->SetVectorParameterValue(TEXT("Color"), Color);
    }
    Plate->RegisterComponent();

    for (int32 X = -1; X <= 1; ++X)
    {
        for (int32 Y = -1; Y <= 1; ++Y)
        {
            UStaticMeshComponent* Hazard = NewObject<UStaticMeshComponent>(this);
            Hazard->SetupAttachment(GetRootComponent());
            Hazard->SetMobility(EComponentMobility::Movable);
            Hazard->SetStaticMesh(bSpikes ? SpikeMesh : FumeMesh);
            Hazard->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Hazard->SetRelativeLocation(Center + FVector(X * Trap.HalfExtent.X * 0.59, Y * Trap.HalfExtent.Y * 0.59, bSpikes ? 29 : 55));
            Hazard->SetRelativeScale3D(bSpikes ? FVector(0.40, 0.40, 0.60) : FVector(1.15, 1.15, 0.9));
            if (!bSpikes)
            {
                Hazard->SetMaterial(0, Plate->GetMaterial(0));
            }
            Hazard->RegisterComponent();
            Trap.Hazards.Add(Hazard);
        }
    }

    Trap.Light = AddLight(*this, Center + FVector(0, 0, 95), Color, 1500, 530);
    Trap.Label = AddLabel(*this, bSpikes ? FName(TEXT("SpikeTrap")) : FName(TEXT("PoisonTrap")), Center + FVector(0, 0, 135), 180, TEXT(""));
    Trap.Label->SetTextRenderColor(Color.ToFColor(true));
    SetTrapPhase(Trap, FTrap::EPhase::Idle, 0);
}

void AFSDungeonLayout::SetTrapPhase(FTrap& Trap, FTrap::EPhase Phase, double Time)
{
    Trap.Phase = Phase;
    const bool bActive = Phase == FTrap::EPhase::Active;
    const bool bWarning = Phase == FTrap::EPhase::Warning;
    for (UStaticMeshComponent* Hazard : Trap.Hazards)
    {
        Hazard->SetVisibility(bActive);
    }
    Trap.Light->SetIntensity(bActive ? 18000 : (bWarning ? 10500 : 1500));
    const TCHAR* Message = Trap.bSpikes ? TEXT("SPIKES") : TEXT("POISON VENT");
    if (bWarning)
    {
        Message = Trap.bSpikes ? TEXT("SPIKES ARMING") : TEXT("POISON BUILDING");
    }
    Trap.Label->SetText(FText::FromString(Message));
    if (bWarning)
    {
        Trap.NextPhase = Time + 0.85;
    }
    else if (bActive)
    {
        Trap.NextPhase = Time + (Trap.bSpikes ? 0.45 : 2.05);
        Trap.NextDamage = Time;
    }
    else if (Phase == FTrap::EPhase::Recovering)
    {
        Trap.NextPhase = Time + 1.25;
    }
}

void AFSDungeonLayout::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    ACharacter* Player = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!IsValid(Player))
    {
        return;
    }

    const double Time = GetWorld()->GetTimeSeconds();
    const FVector Position = Player->GetActorLocation();
    const double FeetZ = Position.Z - Player->GetSimpleCollisionHalfHeight();
    for (FTrap& Trap : Traps)
    {
        using EPhase = FTrap::EPhase;
        const FVector Offset = Position - Trap.Center;
        const bool bInFootprint = FMath::Abs(Offset.X) <= Trap.HalfExtent.X && FMath::Abs(Offset.Y) <= Trap.HalfExtent.Y;
        const bool bInReach = bInFootprint && FeetZ < Trap.Center.Z + Trap.HalfExtent.Z;
        Trap.Label->SetRelativeRotation(FRotator(0, Offset.Rotation().Yaw, 0));

        switch (Trap.Phase)
        {
        case EPhase::Idle:
            if (bInReach) SetTrapPhase(Trap, EPhase::Warning, Time);
            break;
        case EPhase::Warning:
            if (Time >= Trap.NextPhase) SetTrapPhase(Trap, EPhase::Active, Time);
            break;
        case EPhase::Active:
            if (Time >= Trap.NextPhase) SetTrapPhase(Trap, EPhase::Recovering, Time);
            break;
        case EPhase::Recovering:
            if (Time >= Trap.NextPhase && !bInFootprint) SetTrapPhase(Trap, EPhase::Idle, Time);
            break;
        }

        if (Trap.Phase == EPhase::Active && bInReach && Time >= Trap.NextDamage)
        {
            UGameplayStatics::ApplyDamage(Player, Trap.bSpikes ? 24.0f : 7.0f, nullptr, this, nullptr);
            Trap.NextDamage = Trap.bSpikes ? Trap.NextPhase : Time + 0.55;
        }
    }
}
