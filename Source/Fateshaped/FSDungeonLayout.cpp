#include "FSDungeonLayout.h"
#include "FSDungeonPlan.h"
#include "FSRewardLibrary.h"
#include "FSCryptEnemy.h"

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
#include "UObject/UnrealType.h"

namespace
{
constexpr double TileSize = 240.0;
constexpr double WallHeight = 460.0;
using FSDungeon::ERoomRole;
using FSDungeon::FRoom;
const FName LegacyGeometryTag(TEXT("FS_LegacyDungeonGeometry"));
const FName LayoutAttemptedTag(TEXT("FS_DungeonLayoutAttempted"));
const FName GateTag(TEXT("ExitGate"));
const FName FirstEncounterTag(TEXT("Enc1"));
const FName FinalEncounterTag(TEXT("Enc2"));
const FName DeathHandledTag(TEXT("FS_DeathHandled"));
constexpr double ChestReach = 230.0;

enum EModule : int32 { NormalRoom, BossRoom, ExitRoom, Straight, RightBend, LeftBend, TJunction, Cross, WallPlug };
const FName DoorTags[] = {TEXT("FS_Door_PX"), TEXT("FS_Door_PY"), TEXT("FS_Door_NX"), TEXT("FS_Door_NY")};

FVector TileCenter(const FIntPoint& Tile, double Height = 0.0)
{
    return FVector(Tile.X * TileSize, Tile.Y * TileSize, Height);
}

uint8 RotateOpenings(uint8 Mask, int32 Turns)
{
    return ((Mask << Turns) | (Mask >> (4 - Turns))) & 0b1111;
}

USceneComponent* FindModulePoint(AActor& Module, FName Tag)
{
    const TArray<UActorComponent*> Points = Module.GetComponentsByTag(USceneComponent::StaticClass(), Tag);
    return Points.Num() == 1 ? Cast<USceneComponent>(Points[0]) : nullptr;
}

EModule CorridorModule(uint8 Openings, FIntPoint Tile, double& Yaw)
{
    const int32 Degree = FMath::CountBits(static_cast<uint32>(Openings));
    const bool bStraight = Openings == 0b0101 || Openings == 0b1010;
    const bool bRight = (Tile.X + Tile.Y) % 2 == 0;
    const EModule Module = Degree == 4 ? Cross : Degree == 3 ? TJunction : bStraight ? Straight : bRight ? RightBend : LeftBend;
    constexpr uint8 DefaultOpenings[] = {0b0101, 0b0110, 0b1100, 0b0111, 0b1111};
    for (int32 Turns = 0; Turns < 4; ++Turns)
    {
        if (RotateOpenings(DefaultOpenings[Module - Straight], Turns) == Openings)
        {
            Yaw = Turns * 90.0;
            return Module;
        }
    }
    return WallPlug;
}

UStaticMeshComponent* AddProp(AActor& Owner, UStaticMesh* Mesh, const FVector& Base, double Yaw)
{
    if (!Mesh)
    {
        return nullptr;
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
    return Prop;
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

bool IsMarked(const AActor& Actor, FName Name)
{
    const auto* Property = FindFProperty<FBoolProperty>(Actor.GetClass(), Name);
    return Property && Property->GetPropertyValue_InContainer(&Actor);
}

void ShowRewardMessage(AActor& Player, const FString& Text)
{
    if (const auto* Message = FindFProperty<FStrProperty>(Player.GetClass(), TEXT("Message")))
    {
        Message->SetPropertyValue_InContainer(&Player, Text);
    }
}
}

AFSDungeonLayout::AFSDungeonLayout()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    PrimaryActorTick.TickInterval = 0.08f;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("LayoutRoot")));
    static const ConstructorHelpers::FClassFinder<AActor> Normal(TEXT("/Game/Dungeon/Modules/BP_FS_Room_Normal"));
    static const ConstructorHelpers::FClassFinder<AActor> Boss(TEXT("/Game/Dungeon/Modules/BP_FS_Room_Boss"));
    static const ConstructorHelpers::FClassFinder<AActor> Exit(TEXT("/Game/Dungeon/Modules/BP_FS_Room_Exit"));
    static const ConstructorHelpers::FClassFinder<AActor> Line(TEXT("/Game/Dungeon/Modules/BP_FS_Corridor_Straight"));
    static const ConstructorHelpers::FClassFinder<AActor> Right(TEXT("/Game/Dungeon/Modules/BP_FS_Corridor_Right"));
    static const ConstructorHelpers::FClassFinder<AActor> Left(TEXT("/Game/Dungeon/Modules/BP_FS_Corridor_Left"));
    static const ConstructorHelpers::FClassFinder<AActor> Tee(TEXT("/Game/Dungeon/Modules/BP_FS_Corridor_TJunction"));
    static const ConstructorHelpers::FClassFinder<AActor> Crossing(TEXT("/Game/Dungeon/Modules/BP_FS_Corridor_Cross"));
    static const ConstructorHelpers::FClassFinder<AActor> Plug(TEXT("/Game/Dungeon/Modules/BP_FS_Door_WallPlug"));
    ModuleClasses = {Normal.Class, Boss.Class, Exit.Class, Line.Class, Right.Class, Left.Class, Tee.Class, Crossing.Class, Plug.Class};
    static const ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Crate(TEXT("/Game/Medieval_Sewer_Dungeon/Static_Meshes/Props/SM_Crate.SM_Crate"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Barrel(TEXT("/Game/Medieval_Sewer_Dungeon/Static_Meshes/Props/SM_Barrel.SM_Barrel"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Chest(TEXT("/Game/Medieval_Sewer_Dungeon/Static_Meshes/Props/SM_Chest.SM_Chest"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> ChestLidAsset(TEXT("/Game/Medieval_Sewer_Dungeon/Static_Meshes/Props/SM_Lid_Chest.SM_Lid_Chest"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Pipe(TEXT("/Game/Medieval_Sewer_Dungeon/Static_Meshes/Props/SM_Pipe.SM_Pipe"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Telegraph(TEXT("/Game/Combat/FX/MI_FX_Telegraph.MI_FX_Telegraph"));
    TrapPlateMesh = Cube.Object;
    CrateMesh = Crate.Object;
    BarrelMesh = Barrel.Object;
    ChestMesh = Chest.Object;
    ChestLidMesh = ChestLidAsset.Object;
    PipeMesh = Pipe.Object;
    SpikeMesh = Cone.Object;
    FumeMesh = Sphere.Object;
    TelegraphMaterial = Telegraph.Object;
}

AActor* AFSDungeonLayout::SpawnModule(int32 Module, const FVector& Location, double Yaw)
{
    FActorSpawnParameters Params;
    Params.Owner = this;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AActor* Actor = GetWorld()->SpawnActor<AActor>(ModuleClasses[Module], Location, FRotator(0, Yaw, 0), Params);
    if (Actor) SpawnedModules.Add(Actor);
    return Actor;
}

void AFSDungeonLayout::Destroyed()
{
    for (AActor* Actor : SpawnedModules)
    {
        if (IsValid(Actor)) Actor->Destroy();
    }
    SpawnedModules.Reset();
    Super::Destroyed();
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
        return Layout->PendingKillExperience == 0 && FMath::Abs(Offset.X) <= 155.0
            && FMath::Abs(Offset.Y) <= 155.0 && FMath::Abs(Offset.Z) <= 180.0;
    }

    return Position.X > 2400.0;
}

bool AFSDungeonLayout::HasLivingCryptEnemies(const AActor* Player)
{
    if (IsValid(Player))
    {
        for (TActorIterator<AFSCryptEnemy> Enemy(Player->GetWorld()); Enemy; ++Enemy)
        {
            if (!Enemy->IsDefeated())
            {
                return true;
            }
        }
    }
    return false;
}

bool AFSDungeonLayout::Build(int32 RunId)
{
    TArray<AActor*> LegacyGeometry;
    AStaticMeshActor* Gate = nullptr;
    TArray<ACharacter*> LegacyEnemies;
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
            if (Enemy->ActorHasTag(FirstEncounterTag) || Enemy->ActorHasTag(FinalEncounterTag))
            {
                LegacyEnemies.Add(Enemy);
            }
        }
    }
    if (!Gate || LegacyGeometry.IsEmpty() || ModuleClasses.Contains(nullptr))
    {
        return false;
    }
    FSDungeon::FPlan Plan;
    if (!FSDungeon::Generate(RunId, Plan))
    {
        return false;
    }

    const FRotator GateRotation = TileCenter(Plan.ExitDirection).Rotation();
    for (const FRoom& Room : Plan.Rooms)
    {
        const bool bExit = Room.Role == ERoomRole::Exit;
        const EModule Shape = bExit ? ExitRoom : Room.Role == ERoomRole::Boss ? BossRoom : NormalRoom;
        const double Yaw = bExit ? GateRotation.Yaw + 180 : 0;
        AActor* Module = SpawnModule(Shape, TileCenter(Room.Center), Yaw);
        if (!Module) return false;
        if (bExit)
        {
            const USceneComponent* ExitPoint = FindModulePoint(*Module, TEXT("FS_ExitPoint"));
            if (!ExitPoint) return false;
            ExitCenter = ExitPoint->GetComponentLocation();
        }
        const uint8 Openings = FSDungeon::RoomOpenings(Plan, Room);
        const int32 Rotation = (FMath::RoundToInt(Yaw / 90.0) % 4 + 4) % 4;
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(FSDungeon::Directions); ++Index)
        {
            const USceneComponent* Door = FindModulePoint(*Module, DoorTags[Index]);
            if (!Door) return false;
            if (!(Openings & (1 << ((Index + Rotation) % 4)))
                && !SpawnModule(WallPlug, Door->GetComponentLocation(), Door->GetComponentRotation().Yaw))
            {
                return false;
            }
        }
    }
    for (const FIntPoint Tile : Plan.Corridors)
    {
        double Yaw = 0;
        const EModule Shape = CorridorModule(FSDungeon::CorridorOpenings(Plan, Tile), Tile, Yaw);
        if (Shape == WallPlug || !SpawnModule(Shape, TileCenter(Tile), Yaw)) return false;
    }

    FRandomStream Random(RunId);
    TArray<AFSCryptEnemy*> Spawned;
    const auto SpawnEnemy = [&](const FRoom& Room, EFSCryptRole EnemyRole, FIntPoint Offset, FName Tag = NAME_None, int32 RoomSlot = 0)
    {
        const FTransform Transform(FRotator::ZeroRotator, TileCenter(Room.Center + Offset, 94));
        AFSCryptEnemy* Enemy = GetWorld()->SpawnActorDeferred<AFSCryptEnemy>(AFSCryptEnemy::StaticClass(), Transform,
            this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (Enemy)
        {
            Enemy->SetActorTransform(Transform);
            Enemy->Configure(EnemyRole, TileCenter(Room.Center), FVector2D((Room.HalfSize.X + 0.5) * TileSize, (Room.HalfSize.Y + 0.5) * TileSize),
                Random.GetUnsignedInt(), RoomSlot);
            if (!Tag.IsNone())
            {
                Enemy->Tags.Add(Tag);
            }
            Enemy->OnTakeAnyDamage.AddDynamic(this, &AFSDungeonLayout::OnEncounterDamage);
            Enemy->FinishSpawning(Transform);
            Spawned.Add(Enemy);
        }
        return Enemy != nullptr;
    };
    for (const FRoom& Room : Plan.Rooms)
    {
        const bool bGuardian = Room.Role == ERoomRole::Guardian;
        const bool bBoss = Room.Role == ERoomRole::Boss;
        if ((bGuardian || bBoss) && !SpawnEnemy(Room, bBoss ? EFSCryptRole::Warden : EFSCryptRole::Vanguard,
            FIntPoint::ZeroValue, bBoss ? FinalEncounterTag : FirstEncounterTag))
        {
            for (AFSCryptEnemy* Enemy : Spawned)
            {
                Enemy->Destroy();
            }
            return false;
        }
    }
    for (ACharacter* Enemy : LegacyEnemies)
    {
        Enemy->Destroy();
    }

    TArray<EFSCryptRole> EnemyBag{EFSCryptRole::Skirmisher, EFSCryptRole::Skirmisher, EFSCryptRole::Skirmisher, EFSCryptRole::Vanguard};
    constexpr float SpawnChance = 0.60f;
    for (const FRoom& Room : Plan.Rooms)
    {
        if (Room.Role != ERoomRole::Passage)
        {
            continue;
        }
        const FIntPoint Positions[] = {{-1, -1}, {1, 1}, {-1, 1}, {1, -1}};
        int32 Slot = 0;
        while (!EnemyBag.IsEmpty() && Random.FRand() < SpawnChance)
        {
            const int32 Pick = Random.RandRange(0, EnemyBag.Num() - 1);
            if (!SpawnEnemy(Room, EnemyBag[Pick], Positions[Slot], NAME_None, Slot))
            {
                break;
            }
            ++Slot;
            EnemyBag.RemoveAtSwap(Pick, EAllowShrinking::No);
        }
    }

    for (const FRoom& Room : Plan.Rooms)
    {
        const FVector Center = TileCenter(Room.Center);
        if (Room.Role == ERoomRole::Exit)
        {
            continue;
        }
        const FVector Corner = TileCenter(Room.Center + FIntPoint(-Room.HalfSize.X, Room.HalfSize.Y));

        switch (Room.Role)
        {
        case ERoomRole::Guardian:
            AddProp(*this, BarrelMesh, Corner + FVector(60, -10, 0), 20);
            break;
        case ERoomRole::Boss:
            break;
        case ERoomRole::Spikes:
        case ERoomRole::Poison:
            AddTrap(Center, Room.Role == ERoomRole::Spikes, Random.RandRange(0, 1) == 0);
            if (Room.Role == ERoomRole::Poison)
            {
                AddProp(*this, PipeMesh, Corner + FVector(45, 0, 0), 90);
            }
            break;
        case ERoomRole::Cache:
            AddChest(Center);
            AddProp(*this, CrateMesh, Corner + FVector(60, -10, 0), 20);
            AddLight(*this, Center + FVector(0, 0, 160), FLinearColor(1.0f, 0.72f, 0.24f), 14000, 600);
            break;
        default:
            AddProp(*this, CrateMesh, Corner + FVector(60, -10, 0), 20);
            break;
        }
    }

    Gate->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
    Gate->SetActorLocationAndRotation(TileCenter(Plan.GateTile, WallHeight / 2.0), GateRotation, false, nullptr, ETeleportType::TeleportPhysics);
    Gate->SetActorScale3D(FVector(0.35, TileSize / 100.0, WallHeight / 100.0));

    AddLabel(*this, TEXT("ExitLabel"), ExitCenter + FVector(0, 0, 105), GateRotation.Yaw + 180, TEXT("RETURN TO GUILD"));

    for (AActor* Actor : LegacyGeometry)
    {
        Actor->SetActorHiddenInGame(true);
        Actor->SetActorEnableCollision(false);
    }
    SetActorTickEnabled(true);
    UE_LOG(LogTemp, Display, TEXT("Forgotten Crypt run %d: %d rooms, %d tiles, %d L bends, %d T junctions, %d shortcuts, %d traps, %d enemies."),
        RunId, Plan.Rooms.Num(), Plan.Tiles.Num(), Plan.Corners, Plan.Junctions, Plan.Shortcuts, Traps.Num(), Spawned.Num());
    return true;
}

void AFSDungeonLayout::AddChest(const FVector& Center)
{
    ChestCenter = Center;
    const UStaticMeshComponent* Base = AddProp(*this, ChestMesh, Center, 180);
    const double Height = Base ? Base->Bounds.GetBox().Max.Z : 45;
    ChestLid = AddProp(*this, ChestLidMesh, Center + FVector(0, 0, Height), 180);
    ChestLabel = AddLabel(*this, TEXT("CacheChest"), Center + FVector(0, 0, Height + 80), 180, TEXT("[E] OPEN CHEST"));
    ChestLabel->SetWorldSize(32);
    ChestLabel->SetTextRenderColor(FColor(255, 205, 110));
    ChestLabel->SetVisibility(false);
}

void AFSDungeonLayout::InteractWithChest(AActor* Player)
{
    if (IsValid(Player) && !IsMarked(*Player, TEXT("bDead")) && !IsMarked(*Player, TEXT("bWon")))
    {
        if (TActorIterator<AFSDungeonLayout> Layout(Player->GetWorld()); Layout)
        {
            Layout->TryOpenChest(*Player);
        }
    }
}

void AFSDungeonLayout::TryOpenChest(AActor& Player)
{
    if (!ChestLabel || bChestOpened || FVector::DistSquared(Player.GetActorLocation(), ChestCenter) > FMath::Square(ChestReach))
    {
        return;
    }

    const FFSReward Reward{0, TEXT("Arcane Crystal")};
    if (!UFSRewardLibrary::TryGrantExpeditionReward(&Player, Reward))
    {
        ShowRewardMessage(Player, TEXT("Chest reward could not be saved. Press E to retry."));
        return;
    }

    bChestOpened = true;
    ChestLabel->SetText(FText::FromString(TEXT("EMPTY CHEST")));
    if (ChestLid)
    {
        ChestLid->AddRelativeLocation(FVector(0, -15, 16));
        ChestLid->SetRelativeRotation(FRotator(62, 180, 0));
    }
    ShowRewardMessage(Player, TEXT("Chest opened: Arcane Crystal"));
}

void AFSDungeonLayout::UpdateChest(const AActor& Player)
{
    if (!ChestLabel)
    {
        return;
    }
    const FVector Offset = Player.GetActorLocation() - ChestCenter;
    const bool bNearby = Offset.SizeSquared() <= FMath::Square(ChestReach);
    ChestLabel->SetVisibility(bNearby);
    if (bNearby)
    {
        ChestLabel->SetRelativeRotation(FRotator(0, Offset.Rotation().Yaw, 0));
    }
}

void AFSDungeonLayout::OnEncounterDamage(AActor* Enemy, float Damage, const UDamageType* DamageType,
    AController* EventInstigator, AActor* DamageCauser)
{
    // Unreal calls the Blueprint damage event before broadcasting this delegate.
    const AFSCryptEnemy* CryptEnemy = Cast<AFSCryptEnemy>(Enemy);
    if (!IsValid(CryptEnemy) || Enemy->ActorHasTag(DeathHandledTag) || !CryptEnemy->IsDefeated())
    {
        return;
    }
    Enemy->Tags.Add(DeathHandledTag);

    AActor* Player = UGameplayStatics::GetPlayerPawn(this, 0);
    if (!IsValid(Player) || (DamageCauser != Player && (!EventInstigator || EventInstigator->GetPawn() != Player)))
    {
        return;
    }
    PendingKillExperience += CryptEnemy->GetExperienceReward();
    TryGrantKillExperience(*Player);
}

void AFSDungeonLayout::TryGrantKillExperience(AActor& Player)
{
    if (PendingKillExperience == 0 || GetWorld()->GetTimeSeconds() < NextRewardRetry)
    {
        return;
    }
    const FFSReward Reward{PendingKillExperience, FString()};
    if (UFSRewardLibrary::TryGrantExpeditionReward(&Player, Reward))
    {
        ShowRewardMessage(Player, FString::Printf(TEXT("Enemy defeated: +%d XP"), PendingKillExperience));
        PendingKillExperience = 0;
    }
    else
    {
        NextRewardRetry = GetWorld()->GetTimeSeconds() + 0.5;
        ShowRewardMessage(Player, TEXT("Enemy XP pending: could not save. Retrying."));
    }
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
    Plate->SetStaticMesh(TrapPlateMesh);
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

    UpdateChest(*Player);
    TryGrantKillExperience(*Player);

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
