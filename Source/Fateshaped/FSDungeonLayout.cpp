#include "FSDungeonLayout.h"
#include "FSDungeonPlan.h"
#include "FSRewardLibrary.h"
#include "FSCryptEnemy.h"
#include "FSStormCeiling.h"

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
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

namespace
{
constexpr double DoorWidth = 240.0;
constexpr double CorridorJoinOverlap = 12.0;
using FSDungeon::ERoomRole;
using FSDungeon::FRoom;
const FName LayoutAttemptedTag(TEXT("FS_DungeonLayoutAttempted"));
const FName GateTag(TEXT("ExitGate"));
const FName FirstEncounterTag(TEXT("Enc1"));
const FName FinalEncounterTag(TEXT("Enc2"));
const FName DeathHandledTag(TEXT("FS_DeathHandled"));
constexpr double ChestReach = 230.0;

enum EModule : int32 { NormalRoom, BossRoom, ExitRoom, Straight, RightBend, TJunction, WallPlug, FinalGate };
const FName DoorTags[] = {TEXT("FS_Door_PX"), TEXT("FS_Door_PY"), TEXT("FS_Door_NX"), TEXT("FS_Door_NY")};

FVector TileCenter(const FIntPoint& Tile, double Pitch, double Height = 0.0)
{
    return FVector(Tile.X * Pitch, Tile.Y * Pitch, Height);
}

FIntVector PortKey(FIntPoint Tile, int32 Side)
{
    return {Tile.X, Tile.Y, Side};
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

EModule CorridorModule(uint8 Openings, bool bMirrorBend, double& Yaw, bool& bMirrored)
{
    const int32 Degree = FMath::CountBits(static_cast<uint32>(Openings));
    if (Degree < 2 || Degree > 3) return WallPlug;
    const bool bStraight = Openings == 0b0101 || Openings == 0b1010;
    const EModule Module = Degree == 3 ? TJunction : bStraight ? Straight : RightBend;
    bMirrored = Module == RightBend && bMirrorBend;
    constexpr uint8 DefaultOpenings[] = {0b0101, 0b0110, 0b0111};
    const uint8 LocalOpenings = bMirrored ? 0b1100 : DefaultOpenings[Module - Straight];
    for (int32 Turns = 0; Turns < 4; ++Turns)
    {
        if (RotateOpenings(LocalOpenings, Turns) == Openings)
        {
            Yaw = Turns * 90.0;
            return Module;
        }
    }
    return WallPlug;
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
    static const ConstructorHelpers::FClassFinder<AActor> Tee(TEXT("/Game/Dungeon/Modules/BP_FS_Corridor_TJunction"));
    static const ConstructorHelpers::FClassFinder<AActor> Plug(TEXT("/Game/Dungeon/Modules/BP_FS_Door_WallPlug"));
    static const ConstructorHelpers::FClassFinder<AActor> Gate(TEXT("/Game/Dungeon/Modules/BP_FS_Door_FinalGate"));
    ModuleClasses = {Normal.Class, Boss.Class, Exit.Class, Line.Class, Right.Class, Tee.Class, Plug.Class, Gate.Class};
    static const ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static const ConstructorHelpers::FClassFinder<AActor> ClosedChest(TEXT("/Game/Medieval_Sewer_Dungeon/Blueprints/Props/BP_Chest_1"));
    static const ConstructorHelpers::FClassFinder<AActor> OpenChest(TEXT("/Game/Medieval_Sewer_Dungeon/Blueprints/Props/BP_Chest_3"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Telegraph(TEXT("/Game/Combat/FX/MI_FX_Telegraph.MI_FX_Telegraph"));
    static const ConstructorHelpers::FObjectFinder<UMaterialInterface> Cloud(TEXT("/Game/Dungeon/Materials/M_FS_StormCloud.M_FS_StormCloud"));
    TrapPlateMesh = Cube.Object;
    ClosedChestClass = ClosedChest.Class;
    OpenChestClass = OpenChest.Class;
    SpikeMesh = Cone.Object;
    FumeMesh = Sphere.Object;
    TelegraphMaterial = Telegraph.Object;
    StormCloudMaterial = Cloud.Object;
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
    if (IsValid(ChestVisual)) ChestVisual->Destroy();
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
    if (!Layout || !Layout->Build(RunId))
    {
        if (Layout) Layout->Destroy();
        UE_LOG(LogTemp, Error, TEXT("Forgotten Crypt run %d: custom modules could not be assembled. Returning to the Guild."), RunId);
        UGameplayStatics::OpenLevel(Player, FName(TEXT("L_Guild")));
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

    return false;
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
    FVector StraightMouths[2]{FVector::ZeroVector, FVector::ZeroVector};
    FVector ExitMouth = FVector::ZeroVector;
    double RoomReach = 0.0;
    double JunctionReach = 0.0;
    // Measure the actual component hierarchy without using visual mesh bounds.
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
            if (!Door)
            {
                if (Shape == Straight && Side % 2 == 0) return false;
                continue;
            }
            const FTransform Local = Door->GetComponentTransform().GetRelativeTransform(ProbeFrame);
            const FVector Position = Local.GetLocation();
            const FVector HalfMouth = Local.GetRotation().GetRightVector().GetSafeNormal2D()
                * (DoorWidth * 0.5 * FMath::Abs(Local.GetScale3D().Y));
            Extents[Shape].X = FMath::Max(Extents[Shape].X, FMath::Abs(Position.X) + FMath::Abs(HalfMouth.X));
            Extents[Shape].Y = FMath::Max(Extents[Shape].Y, FMath::Abs(Position.Y) + FMath::Abs(HalfMouth.Y));
            if (Shape == Straight && Side % 2 == 0)
            {
                StraightMouths[Side / 2] = Position;
            }
            else if (Shape == ExitRoom && Side == 0)
            {
                ExitMouth = Position;
            }
        }
        Probe->Destroy();
        SpawnedModules.Pop();
        const double Reach = FMath::Max(Extents[Shape].X, Extents[Shape].Y);
        if (Shape < Straight) RoomReach = FMath::Max(RoomReach, Reach);
        else if (Shape != Straight) JunctionReach = FMath::Max(JunctionReach, Reach);
    }
    const FVector LocalSpan = StraightMouths[0] - StraightMouths[1];
    if (LocalSpan.X <= CorridorJoinOverlap || !FMath::IsNearlyZero(LocalSpan.Y, 0.1)
        || !FMath::IsNearlyZero(LocalSpan.Z, 0.1))
    {
        return false;
    }
    // The nearest room pair is three cells apart. A room and turn or two turns can be two cells apart.
    const double MinimumPassage = 0.32 * LocalSpan.X;
    const double RoomPairPitch = (2.0 * RoomReach + MinimumPassage) / 3.0;
    const double MixedPairPitch = (RoomReach + JunctionReach + MinimumPassage) * 0.5;
    const double JunctionPairPitch = JunctionReach + MinimumPassage * 0.5;
    TilePitch = FMath::Max(FMath::Max(LocalSpan.X, RoomPairPitch), FMath::Max(MixedPairPitch, JunctionPairPitch));

    TMap<FIntVector, FTransform> Ports;
    const auto SpawnFittedModule = [&](int32 Shape, const FQuat& Rotation, const FVector& Location, const FVector& Scale)
    {
        AActor* Module = SpawnModule(Shape, FTransform(Rotation, Location));
        if (Module) Module->SetActorScale3D(Module->GetActorScale3D() * Scale);
        return Module;
    };
    const auto Seal = [&](int32 Shape, const FTransform& Port)
    {
        const FVector Scale(1, FMath::Abs(Port.GetScale3D().Y), FMath::Abs(Port.GetScale3D().Z));
        return SpawnFittedModule(Shape, Port.GetRotation(), Port.GetLocation(), Scale);
    };
    const FRotator GateRotation = FVector(Plan.ExitDirection.X, Plan.ExitDirection.Y, 0).Rotation();
    const FRoom* Boss = Plan.Rooms.FindByPredicate([](const FRoom& Room) { return Room.Role == ERoomRole::Boss; });
    if (!Boss) return false;
    const int32 ExitSide = (FMath::RoundToInt(GateRotation.Yaw / 90.0) + 4) % 4;
    for (const FRoom& Room : Plan.Rooms)
    {
        const bool bExit = Room.Role == ERoomRole::Exit;
        const EModule Shape = bExit ? ExitRoom : Room.Role == ERoomRole::Boss ? BossRoom : NormalRoom;
        const double Yaw = bExit ? GateRotation.Yaw + 180 : 0;
        const FRotator Rotation(0, Yaw, 0);
        FVector Center = TileCenter(Room.Center, TilePitch);
        if (bExit)
        {
            const FTransform* BossPort = Ports.Find(PortKey(FSDungeon::DoorTile(*Boss, Plan.ExitDirection), ExitSide));
            if (!BossPort) return false;
            // Leave one native section between the two door mouths, allowing for floor height differences.
            const double ExitPassage = LocalSpan.X - 2.0 * CorridorJoinOverlap - 1.0;
            Center = BossPort->GetLocation() + GateRotation.Vector() * ExitPassage - Rotation.RotateVector(ExitMouth);
            Center.Z = 0.0;
        }
        AActor* Module = SpawnModule(Shape, FTransform(Rotation, Center));
        if (!Module) return false;
        RoomExtents.Add(Extents[Shape]);
        if (bExit)
        {
            const USceneComponent* ExitPoint = FindModulePoint(*Module, TEXT("FS_ExitPoint"));
            if (!ExitPoint) return false;
            ExitCenter = ExitPoint->GetComponentLocation();
        }
        const uint8 Openings = FSDungeon::RoomOpenings(Plan, Room);
        const int32 QuarterTurns = (FMath::RoundToInt(Yaw / 90.0) % 4 + 4) % 4;
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(DoorTags); ++Index)
        {
            const int32 Side = (Index + QuarterTurns) % 4;
            const USceneComponent* Door = FindModulePoint(*Module, DoorTags[Index]);
            if (!Door) return false;
            const FTransform Port = Door->GetComponentTransform();
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
    int32 BendsPlaced = 0;
    for (const FIntPoint Tile : Plan.Corridors)
    {
        double Yaw = 0;
        const uint8 Openings = FSDungeon::CorridorOpenings(Plan, Tile);
        if (FSDungeon::IsStraight(Openings)) continue;
        bool bMirrored = false;
        const EModule Shape = CorridorModule(Openings, BendsPlaced % 2 != 0, Yaw, bMirrored);
        if (Shape == WallPlug) return false;
        const FVector Scale(1, bMirrored ? -1 : 1, 1);
        AActor* Module = SpawnModule(Shape, FTransform(FRotator(0, Yaw, 0), TileCenter(Tile, TilePitch), Scale));
        if (!Module) return false;
        if (Shape == RightBend)
        {
            ++BendsPlaced;
            FVector WorldScale = Module->GetActorScale3D();
            if ((WorldScale.X * WorldScale.Y * WorldScale.Z < 0) != bMirrored)
            {
                WorldScale.Y *= -1;
                Module->SetActorScale3D(WorldScale);
            }
        }
        const int32 Rotation = FMath::RoundToInt(Yaw / 90.0);
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(DoorTags); ++Index)
        {
            const int32 LocalSide = bMirrored ? (4 - Index) % 4 : Index;
            const int32 Side = (LocalSide + Rotation) % 4;
            if (!(Openings & (1 << Side))) continue;
            const USceneComponent* Port = FindModulePoint(*Module, DoorTags[Index]);
            if (!Port) return false;
            const FTransform World = Port->GetComponentTransform();
            const FVector Outward = World.TransformVector(FVector::ForwardVector).GetSafeNormal2D();
            Ports.Add(PortKey(Tile, Side), FTransform(Outward.Rotation(), World.GetLocation(), World.GetScale3D().GetAbs()));
        }
    }

    int32 StraightPieces = 0;
    const auto PlaceStraightRun = [&](const FTransform& StartPort, const FTransform& EndPort, FIntPoint Direction)
    {
        const FVector Start = StartPort.GetLocation();
        const FVector Span = EndPort.GetLocation() - Start;
        if (FVector::DotProduct(Span, FVector(Direction.X, Direction.Y, 0)) <= 0.0) return false;
        const double Distance = Span.Size();
        const int32 PieceCount = FMath::Max(1, FMath::CeilToInt((Distance + CorridorJoinOverlap) / (LocalSpan.X - CorridorJoinOverlap)));
        // Spread the rounding difference across joints. Every piece keeps its authored length.
        const double Overlap = (PieceCount * LocalSpan.X - Distance) / (PieceCount + 1);
        const FVector Axis = Span.GetSafeNormal();
        const FQuat Rotation = FRotationMatrix::MakeFromXZ(Axis, FVector::UpVector).ToQuat();
        const FVector Origin = Start - Axis * Overlap - Rotation.RotateVector(StraightMouths[1]);
        for (int32 Index = 0; Index < PieceCount; ++Index)
        {
            if (!SpawnModule(Straight, FTransform(Rotation, Origin + Axis * ((LocalSpan.X - Overlap) * Index)))) return false;
        }
        StraightPieces += PieceCount;
        return true;
    };
    for (const FIntPoint Tile : Plan.Corridors)
    {
        const uint8 Openings = FSDungeon::CorridorOpenings(Plan, Tile);
        if (!FSDungeon::IsStraight(Openings)) continue;
        const int32 Side = Openings == 0b0101 ? 0 : 1;
        const FIntPoint Direction = FSDungeon::Directions[Side];
        const auto ContinuesRun = [&](FIntPoint Point)
        {
            return Plan.Corridors.Contains(Point) && FSDungeon::CorridorOpenings(Plan, Point) == Openings;
        };
        if (ContinuesRun(Tile - Direction)) continue;

        FIntPoint End = Tile + Direction;
        while (ContinuesRun(End)) End += Direction;

        const FTransform* StartPort = Ports.Find(PortKey(Tile - Direction, Side));
        const FTransform* EndPort = Ports.Find(PortKey(End, (Side + 2) % 4));
        if (!StartPort || !EndPort || !PlaceStraightRun(*StartPort, *EndPort, Direction)) return false;
    }
    // A route can bend at its first cell outside a room. Fit the same authored straight to that short span.
    for (const auto& Entry : Ports)
    {
        const int32 Side = Entry.Key.Z;
        if (Side >= 2) continue;
        const FIntPoint Tile(Entry.Key.X, Entry.Key.Y);
        const FIntPoint Direction = FSDungeon::Directions[Side];
        if (const FTransform* Mate = Ports.Find(PortKey(Tile + Direction, (Side + 2) % 4)))
        {
            if (!PlaceStraightRun(Entry.Value, *Mate, Direction)) return false;
        }
    }
    UE_LOG(LogTemp, Display, TEXT("Forgotten Crypt used %.1f cm spacing and %d straight corridor modules with %.1f cm native length."),
        TilePitch, StraightPieces, LocalSpan.X);
    return true;
}

void AFSDungeonLayout::BuildStormCeiling()
{
    if (!StormCloudMaterial) return;

    FStormCeiling Storm;
    for (AActor* Module : SpawnedModules)
    {
        const int32 Shape = ModuleClasses.IndexOfByPredicate([Module](const TSubclassOf<AActor>& Class)
        {
            return Class == Module->GetClass();
        });
        if (Shape == INDEX_NONE || Shape >= WallPlug) continue;

        const UStaticMeshComponent* Geometry = nullptr;
        double LargestArea = 0.0;
        TInlineComponentArray<UStaticMeshComponent*> Meshes(Module);
        for (const UStaticMeshComponent* Mesh : Meshes)
        {
            const FVector Extent = Mesh->Bounds.BoxExtent;
            const double Area = Extent.X * Extent.Y;
            if (Mesh->GetStaticMesh() && Area > LargestArea)
            {
                Geometry = Mesh;
                LargestArea = Area;
            }
        }
        if (!Geometry) continue;

        const FTransform& MeshFrame = Geometry->GetComponentTransform();
        const FBoxSphereBounds LocalMesh = Geometry->GetStaticMesh()->GetBounds();
        const FVector Size = LocalMesh.BoxExtent * MeshFrame.GetScale3D().GetAbs();
        const FVector Center = MeshFrame.TransformPosition(LocalMesh.Origin);
        const double WallTop = Geometry->Bounds.GetBox().Max.Z;
        const bool bRoom = Shape < Straight;
        if (bRoom || Shape == Straight)
        {
            Storm.Cover(Center, MeshFrame.TransformVector(FVector::ForwardVector), FVector2D(Size.X, Size.Y), WallTop);
        }

        else
        {
            for (const FName Tag : DoorTags)
            {
                const USceneComponent* Point = FindModulePoint(*Module, Tag);
                if (!Point) continue;
                const FTransform& Port = Point->GetComponentTransform();
                const FVector Mouth = Port.GetLocation();
                const FVector Outward = Port.TransformVector(FVector::ForwardVector).GetSafeNormal2D();
                const double HalfWidth = DoorWidth * 0.5 * FMath::Abs(Port.GetScale3D().Y);
                const double Arm = FMath::Abs(FVector::DotProduct(Mouth - Module->GetActorLocation(), Outward));
                Storm.Cover(Mouth - Outward * (Arm * 0.5), Outward, FVector2D(Arm * 0.5 + 55.0, HalfWidth + 92.0), WallTop);
            }
        }

        const FVector LightAt(Center.X, Center.Y, Module->GetActorLocation().Z + 275.0);
        AddLight(*this, LightAt, FLinearColor(0.48f, 0.60f, 1.0f), bRoom ? 23500.0f : 8500.0f, bRoom ? 1600.0f : 1060.0f);
        if (bRoom)
        {
            StormFlashLights.Add(AddLight(*this, FVector(Center.X, Center.Y, WallTop - 125.0),
                FLinearColor(0.58f, 0.39f, 1.0f), 4300.0f, 1350.0f));
        }
    }

    Storm.RegisterCloud(*this, *StormCloudMaterial);
}

bool AFSDungeonLayout::Build(int32 RunId)
{
    if (ModuleClasses.Contains(nullptr))
    {
        return false;
    }
    FSDungeon::FPlan Plan;
    if (!FSDungeon::Generate(RunId, Plan))
    {
        if (!FSDungeon::Generate(1, Plan)) return false;
        UE_LOG(LogTemp, Warning, TEXT("Forgotten Crypt run %d used fallback plan seed 1."), RunId);
    }

    TArray<FVector2D> RoomExtents;
    double TilePitch = 0;
    if (!BuildModules(Plan, TilePitch, RoomExtents)) return false;
    BuildStormCeiling();

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

    TArray<EFSCryptRole> EnemyBag{EFSCryptRole::Skirmisher, EFSCryptRole::Vanguard};
    constexpr float SpawnChance = 0.60f;
    for (const FRoom& Room : Plan.Rooms)
    {
        if (Room.Role != ERoomRole::Guardian)
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
        switch (Room.Role)
        {
        case ERoomRole::Spikes:
        case ERoomRole::Poison:
        {
            const int32 RoomIndex = &Room - Plan.Rooms.GetData();
            AddTrap(Center, Room.Role == ERoomRole::Spikes, RoomExtents[RoomIndex],
                FSDungeon::RoomOpenings(Plan, Room), Random.GetUnsignedInt());
            break;
        }
        case ERoomRole::Cache:
            AddChest(Center);
            AddLight(*this, Center + FVector(0, 0, 160), FLinearColor(1.0f, 0.72f, 0.24f), 14000, 600);
            break;
        default:
            break;
        }
    }

    const FRotator GateRotation = FVector(Plan.ExitDirection.X, Plan.ExitDirection.Y, 0).Rotation();
    AddLabel(*this, TEXT("ExitLabel"), ExitCenter + FVector(0, 0, 105), GateRotation.Yaw + 180, TEXT("RETURN TO GUILD"));

    SetActorTickEnabled(true);
    UE_LOG(LogTemp, Display, TEXT("Forgotten Crypt run %d: %d rooms, %d tiles, %d L bends, %d T junctions, %d traps, %d enemies."),
        RunId, Plan.Rooms.Num(), Plan.Tiles.Num(), Plan.Corners, Plan.Junctions, Traps.Num(), Spawned.Num());
    return true;
}

void AFSDungeonLayout::AddChest(const FVector& Center)
{
    ChestCenter = Center;
    ChestVisual = SpawnChestVisual(ClosedChestClass);
    ChestLabel = AddLabel(*this, TEXT("CacheChest"), Center + FVector(0, 0, 125), 180, TEXT("[E] OPEN CHEST"));
    ChestLabel->SetWorldSize(32);
    ChestLabel->SetTextRenderColor(FColor(255, 205, 110));
    ChestLabel->SetVisibility(false);
}

AActor* AFSDungeonLayout::SpawnChestVisual(TSubclassOf<AActor> Visual)
{
    if (!Visual) return nullptr;
    FActorSpawnParameters Params;
    Params.Owner = this;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AActor* Chest = GetWorld()->SpawnActor<AActor>(Visual, ChestCenter, FRotator(0, 180, 0), Params);
    if (Chest)
    {
        Chest->SetActorEnableCollision(false);
        // Both props share a chest body with an offset pivot. Align the body rather than the open lid or loot.
        TInlineComponentArray<UStaticMeshComponent*> Meshes(Chest);
        for (const UStaticMeshComponent* Mesh : Meshes)
        {
            if (Mesh->GetFName() != TEXT("SM_Chest")) continue;
            const FBoxSphereBounds& Bounds = Mesh->Bounds;
            const FVector Base(Bounds.Origin.X, Bounds.Origin.Y, Bounds.Origin.Z - Bounds.BoxExtent.Z);
            Chest->AddActorWorldOffset(ChestCenter - Base, false, nullptr, ETeleportType::TeleportPhysics);
            break;
        }
    }
    return Chest;
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

    AActor* OpenVisual = SpawnChestVisual(OpenChestClass);
    if (!OpenVisual)
    {
        ShowRewardMessage(Player, TEXT("Chest could not be opened. Press E to retry."));
        return;
    }

    const FFSReward Reward{0, TEXT("Arcane Crystal")};
    if (!UFSRewardLibrary::TryGrantExpeditionReward(&Player, Reward))
    {
        OpenVisual->Destroy();
        ShowRewardMessage(Player, TEXT("Chest reward could not be saved. Press E to retry."));
        return;
    }

    bChestOpened = true;
    ChestLabel->SetText(FText::FromString(TEXT("EMPTY CHEST")));
    if (IsValid(ChestVisual)) ChestVisual->Destroy();
    ChestVisual = OpenVisual;
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

void AFSDungeonLayout::AddTrap(const FVector& Center, bool bSpikes, const FVector2D& RoomExtent, uint8 Openings, int32 Seed)
{
    constexpr int32 Grid = FSDungeon::FTrapMaze::Grid;
    const double FitX = FMath::Max(RoomExtent.X - DoorWidth * 0.5, RoomExtent.X * 0.72);
    const double FitY = FMath::Max(RoomExtent.Y - DoorWidth * 0.5, RoomExtent.Y * 0.72);
    const double CellX = FMath::Max(40.0, FitX * 2.0 / Grid);
    const double CellY = FMath::Max(40.0, FitY * 2.0 / Grid);
    const FVector Origin = Center - FVector(CellX * Grid * 0.5, CellY * Grid * 0.5, 0);

    FSDungeon::FTrapMaze Maze;
    const bool bMaze = FSDungeon::BuildTrapMaze(Seed, Openings, CellX, CellY, DoorWidth, Maze);
    FTrap& Trap = Traps.AddDefaulted_GetRef();
    Trap.Center = Center;
    Trap.bSpikes = bSpikes;
    Trap.HazardHeight = bSpikes ? 70.0 : 140.0;
    const FLinearColor Color = bSpikes ? FLinearColor(1.0f, 0.82f, 0.08f) : FLinearColor(0.12f, 0.86f, 0.18f);
    UMaterialInstanceDynamic* FloorMaterial = TelegraphMaterial ? UMaterialInstanceDynamic::Create(TelegraphMaterial, this) : nullptr;
    if (FloorMaterial) FloorMaterial->SetVectorParameterValue(TEXT("Color"), Color);

    const auto AddMesh = [&](UStaticMesh* Mesh, const FVector& Location, const FVector& Size)
    {
        UStaticMeshComponent* Box = NewObject<UStaticMeshComponent>(this);
        Box->SetupAttachment(GetRootComponent());
        Box->SetMobility(EComponentMobility::Movable);
        Box->SetStaticMesh(Mesh);
        if (FloorMaterial) Box->SetMaterial(0, FloorMaterial);
        Box->SetRelativeLocation(Location);
        Box->SetRelativeScale3D(FVector(Size.X / 100.0, Size.Y / 100.0, Size.Z / 100.0));
        Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Box->SetVisibility(false);
        Box->RegisterComponent();
        return Box;
    };

    for (int32 Y = 0; Y < Grid; ++Y)
    {
        for (int32 X = 0; X < Grid; ++X)
        {
            if (bMaze && !Maze.bTrap[X][Y]) continue;
            FTrap::FCell& Cell = Trap.Cells.AddDefaulted_GetRef();
            Cell.Center = Origin + FVector((X + 0.5) * CellX, (Y + 0.5) * CellY, 0);
            Cell.HalfX = CellX * 0.5;
            Cell.HalfY = CellY * 0.5;
            Cell.Floor = AddMesh(TrapPlateMesh, Cell.Center + FVector(0, 0, 4), FVector(CellX * 0.92, CellY * 0.92, 2.5));
            Cell.Hazard = AddMesh(bSpikes ? SpikeMesh : FumeMesh, Cell.Center + FVector(0, 0, bSpikes ? 28 : 55),
                bSpikes ? FVector(CellX * 0.28, CellY * 0.28, 56) : FVector(CellX * 0.42, CellY * 0.42, 70));
        }
    }
}

void AFSDungeonLayout::SetCellPhase(FTrap& Trap, FTrap::FCell& Cell, FTrap::EPhase Phase, double Time)
{
    Cell.Phase = Phase;
    const bool bActive = Phase == FTrap::EPhase::Active;
    const bool bWarning = Phase == FTrap::EPhase::Warning;
    if (Cell.Floor) Cell.Floor->SetVisibility(bWarning || bActive);
    if (Cell.Hazard) Cell.Hazard->SetVisibility(bActive);
    if (bWarning)
    {
        Cell.NextPhase = Time + 0.85;
    }
    else if (bActive)
    {
        Cell.NextPhase = Time + (Trap.bSpikes ? 0.45 : 2.05);
        Cell.NextDamage = Time;
    }
    else if (Phase == FTrap::EPhase::Recovering)
    {
        Cell.NextPhase = Time + 1.25;
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
    for (int32 Index = 0; Index < StormFlashLights.Num(); ++Index)
    {
        const double Phase = FMath::Frac((Time + 1.75 * Index) / 8.1);
        const double Flash = FMath::Square(FMath::Max(0.0, 1.0 - 16.0 * FMath::Abs(Phase - 0.41)));
        StormFlashLights[Index]->SetIntensity(static_cast<float>(4300.0 + 5200.0 * Flash));
    }
    const FVector Position = Player->GetActorLocation();
    const double FeetZ = Position.Z - Player->GetSimpleCollisionHalfHeight();
    for (FTrap& Trap : Traps)
    {
        using EPhase = FTrap::EPhase;
        for (FTrap::FCell& Cell : Trap.Cells)
        {
            const bool bInside = FMath::Abs(Position.X - Cell.Center.X) <= Cell.HalfX
                && FMath::Abs(Position.Y - Cell.Center.Y) <= Cell.HalfY
                && FeetZ < Cell.Center.Z + Trap.HazardHeight;
            switch (Cell.Phase)
            {
            case EPhase::Idle:
                if (bInside) SetCellPhase(Trap, Cell, EPhase::Warning, Time);
                break;
            case EPhase::Warning:
                if (Time >= Cell.NextPhase) SetCellPhase(Trap, Cell, EPhase::Active, Time);
                break;
            case EPhase::Active:
                if (Time >= Cell.NextPhase) SetCellPhase(Trap, Cell, EPhase::Recovering, Time);
                break;
            case EPhase::Recovering:
                if (Time >= Cell.NextPhase && !bInside) SetCellPhase(Trap, Cell, EPhase::Idle, Time);
                break;
            }
            if (Cell.Phase == EPhase::Active && bInside && Time >= Cell.NextDamage)
            {
                UGameplayStatics::ApplyDamage(Player, Trap.bSpikes ? 24.0f : 7.0f, nullptr, this, nullptr);
                Cell.NextDamage = Trap.bSpikes ? Cell.NextPhase : Time + 0.55;
            }
        }
    }
}
