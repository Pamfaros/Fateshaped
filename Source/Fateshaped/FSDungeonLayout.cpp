#include "FSDungeonLayout.h"
#include "FSDungeonPlan.h"
#include "FSRewardLibrary.h"
#include "FSCryptEnemy.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

namespace
{
constexpr double DoorWidth = 240.0;
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

enum EModule : int32 { NormalRoom, BossRoom, ExitRoom, Straight, RightBend, LeftBend, TJunction, Cross, WallPlug, FinalGate };
const FName DoorTags[] = {TEXT("FS_Door_PX"), TEXT("FS_Door_PY"), TEXT("FS_Door_NX"), TEXT("FS_Door_NY")};

FVector TileCenter(const FIntPoint& Tile, double Pitch, double Height = 0.0)
{
    return FVector(Tile.X * Pitch, Tile.Y * Pitch, Height);
}

FIntVector PortKey(FIntPoint Tile, int32 Side)
{
    return {Tile.X, Tile.Y, Side};
}

struct FConnectorMesh
{
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FProcMeshTangent> Tangents;

    void AddLoft(const FTransform& A, const FTransform& B, double MinY, double MaxY, double MinZ, double MaxZ)
    {
        const auto Edge = [](const FTransform& Port, double Side, double Height)
        {
            const FVector Forward = Port.GetRotation().GetForwardVector().GetSafeNormal2D();
            const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
            return Port.GetLocation() - Forward * 3.0
                + Right * (Side * FMath::Abs(Port.GetScale3D().Y))
                + FVector(0, 0, Height * FMath::Abs(Port.GetScale3D().Z));
        };
        const FVector Points[] = {
            Edge(A, MinY, MinZ), Edge(A, MaxY, MinZ), Edge(A, MaxY, MaxZ), Edge(A, MinY, MaxZ),
            Edge(B, -MinY, MinZ), Edge(B, -MaxY, MinZ), Edge(B, -MaxY, MaxZ), Edge(B, -MinY, MaxZ)
        };
        const FVector Center = (Points[0] + Points[6]) * 0.5;
        constexpr int32 Faces[][4] = {{0,1,2,3}, {4,5,6,7}, {0,1,5,4}, {1,2,6,5}, {2,3,7,6}, {3,0,4,7}};
        for (const auto& Face : Faces)
        {
            const int32 Base = Vertices.Num();
            const FVector P0 = Points[Face[0]], P1 = Points[Face[1]], P2 = Points[Face[2]], P3 = Points[Face[3]];
            const FVector Normal = FVector::CrossProduct(P2 - P0, P1 - P0).GetSafeNormal();
            const bool bOutward = FVector::DotProduct(Normal, (P0 + P1 + P2 + P3) * 0.25 - Center) > 0;
            Vertices.Append({P0, P1, P2, P3});
            if (bOutward) Triangles.Append({Base, Base + 1, Base + 2, Base, Base + 2, Base + 3});
            else Triangles.Append({Base, Base + 2, Base + 1, Base, Base + 3, Base + 2});
            const double U = FVector::Distance(P0, P1) / DoorWidth;
            const double V = FVector::Distance(P0, P3) / DoorWidth;
            UVs.Append({{0,0}, {U,0}, {U,V}, {0,V}});
            for (int32 Vertex = 0; Vertex < 4; ++Vertex)
            {
                Normals.Add(bOutward ? Normal : -Normal);
                Tangents.Emplace((P1 - P0).GetSafeNormal(), false);
            }
        }
    }

    void AddSection(UProceduralMeshComponent& Mesh, int32 Index, UMaterialInterface* Material) const
    {
        Mesh.CreateMeshSection_LinearColor(Index, Vertices, Triangles, Normals, UVs, {}, Tangents, true);
        Mesh.SetMaterial(Index, Material);
    }
};

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
    static const ConstructorHelpers::FClassFinder<AActor> Gate(TEXT("/Game/Dungeon/Modules/BP_FS_Door_FinalGate"));
    ModuleClasses = {Normal.Class, Boss.Class, Exit.Class, Line.Class, Right.Class, Left.Class, Tee.Class, Crossing.Class, Plug.Class, Gate.Class};
    static const ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Crate(TEXT("/Game/Medieval_Sewer_Dungeon/Static_Meshes/Props/SM_Crate.SM_Crate"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Barrel(TEXT("/Game/Medieval_Sewer_Dungeon/Static_Meshes/Props/SM_Barrel.SM_Barrel"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Chest(TEXT("/Game/Medieval_Sewer_Dungeon/Static_Meshes/Props/SM_Chest.SM_Chest"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> ChestLidAsset(TEXT("/Game/Medieval_Sewer_Dungeon/Static_Meshes/Props/SM_Lid_Chest.SM_Lid_Chest"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Pipe(TEXT("/Game/Medieval_Sewer_Dungeon/Static_Meshes/Props/SM_Pipe.SM_Pipe"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Telegraph(TEXT("/Game/Combat/FX/MI_FX_Telegraph.MI_FX_Telegraph"));
    static const ConstructorHelpers::FObjectFinder<UMaterialInterface> FloorMaterial(TEXT("/Game/Medieval_Sewer_Dungeon/Materials_Instance/MI_Large_Room_Floor.MI_Large_Room_Floor"));
    static const ConstructorHelpers::FObjectFinder<UMaterialInterface> WallMaterial(TEXT("/Game/Medieval_Sewer_Dungeon/Materials_Instance/MI_Large_Wall.MI_Large_Wall"));
    ConnectorFloorMaterial = FloorMaterial.Object;
    ConnectorWallMaterial = WallMaterial.Object;
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

AActor* AFSDungeonLayout::SpawnModule(int32 Module, const FTransform& Transform)
{
    FActorSpawnParameters Params;
    Params.Owner = this;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AActor* Actor = GetWorld()->SpawnActor<AActor>(ModuleClasses[Module], Transform, Params);
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

bool AFSDungeonLayout::BuildModules(const FSDungeon::FPlan& Plan, double& TilePitch, TArray<FVector2D>& RoomExtents)
{
    FVector2D Extents[WallPlug];
    double Clearance = 0;
    // Measure the actual component hierarchy without using any visual mesh bounds.
    for (int32 Shape = NormalRoom; Shape < WallPlug; ++Shape)
    {
        AActor* Probe = SpawnModule(Shape, FTransform(FVector(0, 0, -100000)));
        if (!Probe) return false;
        Extents[Shape] = FVector2D::ZeroVector;
        // Keep any authored root scale in the world-space footprint measurement.
        const FTransform ProbeFrame(Probe->GetActorQuat(), Probe->GetActorLocation());
        for (int32 Side = 0; Side < UE_ARRAY_COUNT(DoorTags); ++Side)
        {
            const USceneComponent* Door = FindModulePoint(*Probe, DoorTags[Side]);
            if (!Door) continue;
            const FTransform Local = Door->GetComponentTransform().GetRelativeTransform(ProbeFrame);
            const FVector Position = Local.GetLocation();
            const FVector HalfMouth = Local.GetRotation().GetRightVector().GetSafeNormal2D()
                * (DoorWidth * 0.5 * FMath::Abs(Local.GetScale3D().Y));
            Extents[Shape].X = FMath::Max(Extents[Shape].X, FMath::Abs(Position.X) + FMath::Abs(HalfMouth.X));
            Extents[Shape].Y = FMath::Max(Extents[Shape].Y, FMath::Abs(Position.Y) + FMath::Abs(HalfMouth.Y));
            const double Slide = FMath::Abs(Side % 2 == 0 ? Position.Y : Position.X);
            if (Slide > 0.1 || FMath::Abs(Position.Z) > 0.1 || !Local.GetScale3D().Equals(FVector::OneVector))
            {
                Clearance = FMath::Max(Clearance, FMath::Max(40.0 + 0.65 * Slide, 4.0 * FMath::Abs(Position.Z)));
            }
        }
        Probe->Destroy();
        SpawnedModules.Pop();
        const double HalfCells = Shape == NormalRoom ? FSDungeon::NormalHalfSize.X + 0.5
            : Shape == BossRoom ? FSDungeon::BossHalfSize.X + 0.5
            : Shape == ExitRoom ? FSDungeon::ExitHalfSize.X + 0.5 : 0.5;
        TilePitch = FMath::Max(TilePitch, FMath::Max(Extents[Shape].X, Extents[Shape].Y) / HalfCells);
    }
    TilePitch += Clearance;

    TMap<FIntVector, FTransform> Ports;
    const auto Seal = [&](int32 Shape, const FTransform& Port)
    {
        const FVector Scale(1, FMath::Abs(Port.GetScale3D().Y), FMath::Abs(Port.GetScale3D().Z));
        return SpawnModule(Shape, FTransform(Port.GetRotation(), Port.GetLocation(), Scale));
    };
    const FRotator GateRotation = FVector(Plan.ExitDirection.X, Plan.ExitDirection.Y, 0).Rotation();
    for (const FRoom& Room : Plan.Rooms)
    {
        const bool bExit = Room.Role == ERoomRole::Exit;
        const EModule Shape = bExit ? ExitRoom : Room.Role == ERoomRole::Boss ? BossRoom : NormalRoom;
        const double Yaw = bExit ? GateRotation.Yaw + 180 : 0;
        AActor* Module = SpawnModule(Shape, FTransform(FRotator(0, Yaw, 0), TileCenter(Room.Center, TilePitch)));
        if (!Module) return false;
        RoomExtents.Add(Extents[Shape]);
        if (bExit)
        {
            const USceneComponent* ExitPoint = FindModulePoint(*Module, TEXT("FS_ExitPoint"));
            if (!ExitPoint) return false;
            ExitCenter = ExitPoint->GetComponentLocation();
        }
        const uint8 Openings = FSDungeon::RoomOpenings(Plan, Room);
        const int32 Rotation = (FMath::RoundToInt(Yaw / 90.0) % 4 + 4) % 4;
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(DoorTags); ++Index)
        {
            const USceneComponent* Door = FindModulePoint(*Module, DoorTags[Index]);
            if (!Door) return false;
            const FTransform Port = Door->GetComponentTransform();
            const int32 Side = (Index + Rotation) % 4;
            if (Openings & (1 << Side))
            {
                Ports.Add(PortKey(FSDungeon::DoorTile(Room, FSDungeon::Directions[Side]), Side), Port);
                if (bExit)
                {
                    AActor* Gate = Seal(FinalGate, Port);
                    if (!Gate) return false;
                    Gate->Tags.AddUnique(GateTag);
                }
            }
            else if (!Seal(WallPlug, Port)) return false;
        }
    }
    for (const FIntPoint Tile : Plan.Corridors)
    {
        double Yaw = 0;
        const uint8 Openings = FSDungeon::CorridorOpenings(Plan, Tile);
        const EModule Shape = CorridorModule(Openings, Tile, Yaw);
        if (Shape == WallPlug) return false;
        AActor* Module = SpawnModule(Shape, FTransform(FRotator(0, Yaw, 0), TileCenter(Tile, TilePitch)));
        if (!Module) return false;
        const int32 Rotation = FMath::RoundToInt(Yaw / 90.0);
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(DoorTags); ++Index)
        {
            const int32 Side = (Index + Rotation) % 4;
            if (!(Openings & (1 << Side))) continue;
            const USceneComponent* Port = FindModulePoint(*Module, DoorTags[Index]);
            if (!Port) return false;
            Ports.Add(PortKey(Tile, Side), Port->GetComponentTransform());
        }
    }

    FConnectorMesh FloorMesh, WallMesh;
    int32 Connectors = 0;
    for (const auto& Entry : Ports)
    {
        const int32 Side = Entry.Key.Z;
        const FIntPoint Tile(Entry.Key.X, Entry.Key.Y);
        const FTransform* Mate = Ports.Find(PortKey(Tile + FSDungeon::Directions[Side], (Side + 2) % 4));
        if (!Mate) return false;
        // Visit each seam once, from its positive-axis port.
        if (Side >= 2 || Entry.Value.GetLocation().Equals(Mate->GetLocation(), 0.1)) continue;
        constexpr double HalfWidth = DoorWidth * 0.5;
        FloorMesh.AddLoft(Entry.Value, *Mate, -HalfWidth, HalfWidth, -40, 0);
        WallMesh.AddLoft(Entry.Value, *Mate, -HalfWidth - 12, -HalfWidth + 12, 0, WallHeight);
        WallMesh.AddLoft(Entry.Value, *Mate, HalfWidth - 12, HalfWidth + 12, 0, WallHeight);
        ++Connectors;
    }
    if (Connectors > 0)
    {
        auto* Mesh = NewObject<UProceduralMeshComponent>(this, TEXT("AdaptiveCorridorSeams"));
        Mesh->SetupAttachment(GetRootComponent());
        Mesh->SetCanEverAffectNavigation(false);
        Mesh->SetCollisionProfileName(TEXT("BlockAll"));
        Mesh->SetGenerateOverlapEvents(false);
        Mesh->RegisterComponent();
        FloorMesh.AddSection(*Mesh, 0, ConnectorFloorMaterial);
        WallMesh.AddSection(*Mesh, 1, ConnectorWallMaterial);
    }
    UE_LOG(LogTemp, Display, TEXT("Forgotten Crypt fitted %.1f cm grid spacing and %d adaptive corridor seams."), TilePitch, Connectors);
    return true;
}

bool AFSDungeonLayout::Build(int32 RunId)
{
    TArray<AActor*> LegacyGeometry;
    TArray<AActor*> LegacyGates;
    TArray<ACharacter*> LegacyEnemies;
    for (TActorIterator<AActor> Actor(GetWorld()); Actor; ++Actor)
    {
        if (Actor->ActorHasTag(LegacyGeometryTag))
        {
            LegacyGeometry.Add(*Actor);
        }
        if (Actor->ActorHasTag(GateTag))
        {
            LegacyGates.Add(*Actor);
            LegacyGeometry.AddUnique(*Actor);
        }
        if (ACharacter* Enemy = Cast<ACharacter>(*Actor))
        {
            if (Enemy->ActorHasTag(FirstEncounterTag) || Enemy->ActorHasTag(FinalEncounterTag))
            {
                LegacyEnemies.Add(Enemy);
            }
        }
    }
    if (LegacyGeometry.IsEmpty() || ModuleClasses.Contains(nullptr))
    {
        return false;
    }
    FSDungeon::FPlan Plan;
    if (!FSDungeon::Generate(RunId, Plan))
    {
        return false;
    }

    TArray<FVector2D> RoomExtents;
    double TilePitch = 0;
    if (!BuildModules(Plan, TilePitch, RoomExtents)) return false;

    FRandomStream Random(RunId);
    TArray<AFSCryptEnemy*> Spawned;
    const auto SpawnEnemy = [&](const FRoom& Room, EFSCryptRole EnemyRole, FIntPoint Offset, FName Tag = NAME_None, int32 RoomSlot = 0)
    {
        const FVector2D Extent = RoomExtents[&Room - Plan.Rooms.GetData()];
        const FVector Center = TileCenter(Room.Center, TilePitch);
        const FTransform Transform(FRotator::ZeroRotator,
            Center + FVector(Offset.X * Extent.X * 0.4, Offset.Y * Extent.Y * 0.4, 94));
        AFSCryptEnemy* Enemy = GetWorld()->SpawnActorDeferred<AFSCryptEnemy>(AFSCryptEnemy::StaticClass(), Transform,
            this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (Enemy)
        {
            Enemy->SetActorTransform(Transform);
            Enemy->Configure(EnemyRole, Center, Extent,
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
        const FVector Center = TileCenter(Room.Center, TilePitch);
        if (Room.Role == ERoomRole::Exit)
        {
            continue;
        }
        const FVector2D Extent = RoomExtents[&Room - Plan.Rooms.GetData()];
        const FVector Corner = Center + FVector(-Extent.X * 0.8, Extent.Y * 0.8, 0);

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

    const FRotator GateRotation = FVector(Plan.ExitDirection.X, Plan.ExitDirection.Y, 0).Rotation();
    AddLabel(*this, TEXT("ExitLabel"), ExitCenter + FVector(0, 0, 105), GateRotation.Yaw + 180, TEXT("RETURN TO GUILD"));

    for (AActor* Gate : LegacyGates) Gate->Tags.Remove(GateTag);
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
