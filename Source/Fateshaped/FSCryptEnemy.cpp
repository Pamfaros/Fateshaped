#include "FSCryptEnemy.h"

#include "FSCryptBehavior.h"
#include "FSCharacterActions.h"
#include "AIController.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "BrainComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

namespace
{
constexpr float AlertRange = 1050.0f;

bool PlayerIsDefeated(const ACharacter& Player)
{
    const auto* Dead = FindFProperty<FBoolProperty>(Player.GetClass(), TEXT("bDead"));
    return Dead && Dead->GetPropertyValue_InContainer(&Player);
}
}

AFSCryptEnemy::AFSCryptEnemy()
{
    PrimaryActorTick.bCanEverTick = true;
    AIControllerClass = AAIController::StaticClass();
    AutoPossessAI = EAutoPossessAI::Disabled;
    bUseControllerRotationYaw = false;
    GetCapsuleComponent()->InitCapsuleSize(42.0f, 89.0f);
    GetMesh()->SetRelativeLocation(FVector(0, 0, -89));
    GetMesh()->SetRelativeRotation(FRotator(0, -90, 0));
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    auto* Movement = GetCharacterMovement();
    Movement->bOrientRotationToMovement = false;
    Movement->MaxAcceleration = 1800;
    Movement->BrakingDecelerationWalking = 2000;
    Movement->bCanWalkOffLedges = false;

    Telegraph = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AttackTelegraph"));
    Telegraph->SetupAttachment(GetRootComponent());
    Telegraph->SetAbsolute(true, true, true);
    Telegraph->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Telegraph->SetCastShadow(false);
    Telegraph->SetVisibility(false);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Disc(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Lane(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Warning(TEXT("/Game/Combat/FX/MI_FX_Telegraph.MI_FX_Telegraph"));
    WarningDisc = Disc.Object;
    WarningLane = Lane.Object;
    Telegraph->SetStaticMesh(WarningDisc);
    Telegraph->SetMaterial(0, Warning.Object);

    Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("EnemyStatus"));
    Label->SetupAttachment(GetRootComponent());
    Label->SetRelativeLocation(FVector(0, 0, 145));
    Label->SetHorizontalAlignment(EHTA_Center);
    Label->SetWorldSize(26);
    Label->SetVisibility(false);

    // Hard references make the imported meshes and their matching clips available in the cooked game.
    for (const TCHAR* Model : {TEXT("Orc_Hummer"), TEXT("Troll")})
    {
        const FString Folder = FString::Printf(TEXT("/Game/Fantasy_Pack/Characters/%s"), Model);
        const FString MeshName = FString::Printf(TEXT("SK_%s"), Model);
        ConstructorHelpers::FObjectFinder<USkeletalMesh> MeshAsset(*(Folder / TEXT("Mesh") / (MeshName + TEXT(".") + MeshName)));
        EnemyMeshes.Add(MeshAsset.Object);
        for (const TCHAR* Suffix : {TEXT("Idle"), TEXT("Walk"), TEXT("Run"), TEXT("Attack_1"), TEXT("Attack_2"),
            TEXT("Attack_3"), TEXT("Jump"), TEXT("Get_hit"), TEXT("Dead")})
        {
            const FString Name = FString::Printf(TEXT("Anim_%s_%s"), Model, Suffix);
            ConstructorHelpers::FObjectFinder<UAnimSequence> Clip(*(Folder / TEXT("Animations") / (Name + TEXT(".") + Name)));
            EnemyClips.Add(Clip.Object);
        }
    }
}

void AFSCryptEnemy::Configure(EFSCryptRole InRole, const FVector& RoomCenter, const FVector2D& RoomHalfExtent, int32 Seed, int32 RoomSlot)
{
    CombatRole = InRole;
    Home = GetActorLocation();
    BoundsCenter = RoomCenter;
    BoundsHalfExtent = RoomHalfExtent;
    SpawnSlot = RoomSlot;
    Random.Initialize(Seed);
    OrbitSide = Random.RandRange(0, 1) ? 1 : -1;

    const bool bTroll = CombatRole != EFSCryptRole::Skirmisher;
    const bool bBoss = CombatRole == EFSCryptRole::Warden;
    MaxHealth = bBoss ? 220.0f : (bTroll ? 90.0f : 55.0f);
    Health = MaxHealth;
    GetMesh()->SetSkeletalMesh(EnemyMeshes[bTroll ? 1 : 0]);
    // Fit the imported meshes to their capsule from their authored bounds.
    if (const USkeletalMesh* MeshAsset = GetMesh()->GetSkeletalMeshAsset())
    {
        const FBoxSphereBounds Bounds = MeshAsset->GetBounds();
        const double Height = bBoss ? 300.0 : (bTroll ? 232.0 : 187.0);
        const double Scale = Height / FMath::Max(1.0, Bounds.BoxExtent.Z * 2.0);
        GetMesh()->SetRelativeScale3D(FVector(Scale));
        GetMesh()->SetRelativeLocation(FVector(0, 0, -89 - (Bounds.Origin.Z - Bounds.BoxExtent.Z) * Scale));
        Label->SetRelativeLocation(FVector(0, 0, Height - 55));
    }
    const int32 Start = bTroll ? 9 : 0;
    Idle = EnemyClips[Start];
    Walk = EnemyClips[Start + (bTroll ? 1 : 2)];
    Attacks = {EnemyClips[Start + 5], EnemyClips[Start + 3], EnemyClips[Start + 6], EnemyClips[Start + 5]};
    React = EnemyClips[Start + 7];
    Death = EnemyClips[Start + 8];
}

void AFSCryptEnemy::BeginPlay()
{
    Super::BeginPlay();
    GetMesh()->RegisterOnBoneTransformsFinalizedDelegate(
        FOnBoneTransformsFinalizedMultiCast::FDelegate::CreateUObject(this, &AFSCryptEnemy::OnAttackPoseEvaluated));
    SetAnimation(Idle, true);
    SpawnDefaultController();
    if (AAIController* AI = Cast<AAIController>(GetController()))
    {
        Tree = CreateCryptBehaviorTree(*this);
        AI->RunBehaviorTree(Tree);
    }
}

void AFSCryptEnemy::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bDead)
    {
        return;
    }
    if (!MoveDirection.IsNearlyZero())
    {
        AddMovementInput(MoveDirection);
    }
    UpdateLabel(UGameplayStatics::GetPlayerCharacter(this, 0));
}

void AFSCryptEnemy::SetAnimation(UAnimSequence* Clip, bool bLoop, float Rate)
{
    if (!Clip || (bLoop && PlayingClip == Clip))
    {
        return;
    }
    PlayingClip = Clip;
    GetMesh()->PlayAnimation(Clip, bLoop);
    GetMesh()->SetPlayRate(Rate);
}

void AFSCryptEnemy::Face(const FVector& Point)
{
    const FVector Direction = (Point - GetActorLocation()).GetSafeNormal2D();
    if (!Direction.IsNearlyZero())
    {
        SetActorRotation(Direction.Rotation());
    }
}

void AFSCryptEnemy::MoveAlong(const FVector& Direction, float Speed)
{
    MoveDirection = Direction;
    GetCharacterMovement()->MaxWalkSpeed = Speed;
    SetAnimation(Direction.IsNearlyZero() ? Idle.Get() : Walk.Get(), true);
}

bool AFSCryptEnemy::IsInRoom(const FVector& Position, double Padding) const
{
    const FVector Offset = Position - BoundsCenter;
    return FMath::Abs(Offset.X) <= BoundsHalfExtent.X - Padding && FMath::Abs(Offset.Y) <= BoundsHalfExtent.Y - Padding;
}

bool AFSCryptEnemy::CanSee(const ACharacter& Player) const
{
    return FVector::DistSquared2D(Player.GetActorLocation(), GetActorLocation()) < FMath::Square(AlertRange)
        && !PlayerIsDefeated(Player) && UFSCharacterActions::HasClearStrike(this, &Player);
}

void AFSCryptEnemy::GetRoomAllies(TArray<const AFSCryptEnemy*>& Allies) const
{
    for (TActorIterator<AFSCryptEnemy> Other(GetWorld()); Other; ++Other)
    {
        if (*Other != this && !Other->bDead && Other->BoundsCenter.Equals(BoundsCenter))
        {
            Allies.Add(*Other);
        }
    }
}

FVector AFSCryptEnemy::GetAttackPosition(const FVector& Target, const TArray<const AFSCryptEnemy*>& Allies) const
{
    const AFSCryptEnemy* Anchor = this;
    for (const AFSCryptEnemy* Ally : Allies)
    {
        if (Ally->CombatRole > Anchor->CombatRole || (Ally->CombatRole == Anchor->CombatRole && Ally->SpawnSlot < Anchor->SpawnSlot))
        {
            Anchor = Ally;
        }
    }

    FVector Front = (Anchor->GetActorLocation() - Target).GetSafeNormal2D();
    if (Front.IsNearlyZero())
    {
        Front = (Anchor->Home - Target).GetSafeNormal2D();
    }
    if (Front.IsNearlyZero())
    {
        Front = FVector::ForwardVector;
    }

    int32 Rank = 0;
    for (const AFSCryptEnemy* Ally : Allies)
    {
        Rank += Ally != Anchor && Ally->SpawnSlot < SpawnSlot;
    }
    const double Angles[] = {82.0, -82.0, 162.0};
    const double PreferredAngle = Anchor == this ? 0.0 : Angles[FMath::Min(Rank, 2)];
    const double Radius = CombatRole == EFSCryptRole::Skirmisher ? 168.0 : 197.0;
    const FVector Desired = Target + Front.RotateAngleAxis(PreferredAngle, FVector::UpVector) * Radius;
    FVector Best = Desired;
    double BestScore = -TNumericLimits<double>::Max();
    for (const double Offset : {0.0, 35.0, -35.0, 70.0, -70.0, 140.0, -140.0})
    {
        const FVector Slot = Target + Front.RotateAngleAxis(PreferredAngle + Offset, FVector::UpVector) * Radius;
        if (!IsInRoom(Slot, 58))
        {
            continue;
        }
        double Score = -FMath::Abs(Offset) - 0.035 * FVector::Dist2D(Slot, GetActorLocation());
        for (const AFSCryptEnemy* Ally : Allies)
        {
            const double Clearance = FVector::Dist2D(Slot, Ally->GetActorLocation());
            Score -= FMath::Max(0.0, 145.0 - Clearance) * (Ally == Anchor ? 1.5 : 0.8);
        }
        if (Score > BestScore)
        {
            BestScore = Score;
            Best = Slot;
        }
    }
    return Best;
}

FVector AFSCryptEnemy::SteerAroundAllies(const FVector& Direction, const TArray<const AFSCryptEnemy*>& Allies) const
{
    const FVector Position = GetActorLocation();
    const FVector Test = Position + Direction * 118.0;
    for (const AFSCryptEnemy* Ally : Allies)
    {
        const FVector Separation = (Position - Ally->GetActorLocation()).GetSafeNormal2D();
        if (FVector::DistSquared2D(Test, Ally->GetActorLocation()) < FMath::Square(110.0)
            && FVector::DotProduct(Separation, Direction) < 0.0)
        {
            const FVector Side = FVector::CrossProduct(FVector::UpVector, Separation);
            const FVector First = FVector::DotProduct(Side, Direction) >= 0.0 ? Side : -Side;
            for (const FVector Bypass : {First, -First})
            {
                if (IsInRoom(Position + Bypass * 115.0, 56))
                {
                    return (Bypass + Separation * 0.3).GetSafeNormal2D();
                }
            }
        }
    }
    return Direction;
}

bool AFSCryptEnemy::CanCommitAttack(const FVector& Target, const TArray<const AFSCryptEnemy*>& Allies, double ImpactTime) const
{
    const FVector Bearing = (GetActorLocation() - Target).GetSafeNormal2D();
    int32 ActiveAttacks = 0;
    for (const AFSCryptEnemy* Ally : Allies)
    {
        if (!Ally->IsWindingUp())
        {
            continue;
        }
        ++ActiveAttacks;
        const FVector AllyBearing = (Ally->GetActorLocation() - Target).GetSafeNormal2D();
        if (FVector::DotProduct(Bearing, AllyBearing) > 0.5 || FMath::Abs(ImpactTime - Ally->ExpectedImpact) < 0.28)
        {
            return false;
        }
    }
    return ActiveAttacks < 2;
}

bool AFSCryptEnemy::TryBehavior(EFSCryptBranch Branch)
{
    if (bDead)
    {
        return false;
    }
    const double Now = GetWorld()->GetTimeSeconds();
    ACharacter* Player = UGameplayStatics::GetPlayerCharacter(this, 0);
    const FVector Position = GetActorLocation();
    const FVector Target = Player ? Player->GetActorLocation() : Home;
    const FVector ToTarget = (Target - Position).GetSafeNormal2D();
    const double Distance = FVector::Dist2D(Target, Position);
    const bool bBoss = CombatRole == EFSCryptRole::Warden;
    const bool bLight = CombatRole == EFSCryptRole::Skirmisher;
    const bool bEngaged = Player && IsInRoom(Target, -130.0) && CanSee(*Player);
    if (bEngaged)
    {
        LastSight = Now;
    }

    switch (Branch)
    {
    case EFSCryptBranch::ResolveAction:
        if (Action == EAction::None)
        {
            return false;
        }
        if (IsAttacking() ? !bAttackFinished : Now < ActionEnd)
        {
            return true;
        }
        Telegraph->SetVisibility(false);
        MoveDirection = FVector::ZeroVector;
        if (IsAttacking())
        {
            const bool bFollowUp = bCombo && bEngaged && Distance < 340.0 && IsInRoom(Target);
            bCombo = false;
            if (bFollowUp)
            {
                StartAttack(EAction::Sweep, Target, Now);
            }
            else
            {
                Action = EAction::Recover;
                ActionEnd = Now + (bBoss ? 0.36 : 0.19);
                NextAttack = ActionEnd + Random.FRandRange(0.08f, 0.25f);
            }
            return true;
        }
        Action = EAction::None;
        return false;

    case EFSCryptBranch::ReturnHome:
        if (IsInRoom(Position, 16.0) && (bEngaged || Now - LastSight < 1.2 || FVector::Dist2D(Position, Home) < 75.0))
        {
            return false;
        }
        Face(Home);
        MoveAlong((Home - Position).GetSafeNormal2D(), 250);
        return true;

    case EFSCryptBranch::Defend:
        if (!bEngaged || Distance > 400.0 || Now < NextDefense || Now - LastHit > 1.5 || Pressure < (bBoss ? 2 : 3))
        {
            return false;
        }
        Pressure = 0;
        NextDefense = Now + (bBoss ? 3.2 : 4.7);
        Face(Target);
        if (!bLight && (!bBoss || bGuardNext))
        {
            MoveAlong(FVector::ZeroVector, 0);
            Action = EAction::Guard;
            ActionEnd = Now + 0.72;
            SetAnimation(React, false, 0.75f);
        }
        else
        {
            const FVector Side = FVector::CrossProduct(FVector::UpVector, ToTarget) * OrbitSide;
            const FVector Direction = (Side - ToTarget * 0.52).GetSafeNormal2D();
            const FVector Alternative = (-Side - ToTarget * 0.52).GetSafeNormal2D();
            const FVector Evade = IsInRoom(Position + Direction * 185, 55) ? Direction : Alternative;
            if (!IsInRoom(Position + Evade * 185, 55))
            {
                return false;
            }
            MoveAlong(Evade, 540);
            Action = EAction::Evade;
            ActionEnd = Now + 0.34;
        }
        if (bBoss)
        {
            bGuardNext = !bGuardNext;
        }
        return true;

    case EFSCryptBranch::Attack:
    {
        if (!bEngaged || !IsInRoom(Target) || Now < NextAttack || Distance > (bBoss ? 490.0 : 235.0))
        {
            return false;
        }
        EAction Attack = bLight && PreviousAttack != EAction::Cut ? EAction::Cut : EAction::Sweep;
        if (bBoss)
        {
            Attack = Distance > 300.0 ? EAction::Lunge
                : PreviousAttack == EAction::Stomp ? EAction::Sweep : EAction::Stomp;
        }
        TArray<const AFSCryptEnemy*> Allies;
        GetRoomAllies(Allies);
        if (!CanCommitAttack(Target, Allies, Now + GetTimeUntilContact(Attack)))
        {
            return false;
        }
        if (!Allies.IsEmpty() && FVector::DistSquared2D(Position, GetAttackPosition(Target, Allies)) > FMath::Square(98.0))
        {
            return false;
        }
        bCombo = bBoss && Health <= MaxHealth * 0.5f && Attack != EAction::Sweep;
        StartAttack(Attack, Target, Now);
        return true;
    }

    case EFSCryptBranch::Position:
    {
        if (!bEngaged)
        {
            return false;
        }
        Face(Target);
        TArray<const AFSCryptEnemy*> Allies;
        GetRoomAllies(Allies);
        if (!Allies.IsEmpty())
        {
            const FVector Slot = GetAttackPosition(Target, Allies);
            if (IsInRoom(Slot, 45))
            {
                FVector Direction = (Slot - Position).GetSafeNormal2D();
                const FVector FromTarget = -ToTarget;
                const FVector DesiredBearing = (Slot - Target).GetSafeNormal2D();
                if (FVector::DotProduct(FromTarget, DesiredBearing) < 0.50 && Distance < 340)
                {
                    const FVector Side = FVector::CrossProduct(FVector::UpVector, FromTarget);
                    const double Turn = FVector::DotProduct(Side, DesiredBearing) >= 0.0 ? 1.0 : -1.0;
                    Direction = (Side * Turn + FromTarget * (Distance < 175 ? 0.65 : -0.12)).GetSafeNormal2D();
                }
                Direction = FVector::DistSquared2D(Position, Slot) < FMath::Square(38.0)
                    ? FVector::ZeroVector : SteerAroundAllies(Direction, Allies);
                if (IsInRoom(Position + Direction * 90, 45))
                {
                    MoveAlong(Direction, bLight ? 345.0f : 255.0f);
                    return true;
                }
            }
        }
        if (Distance > (bLight ? 170.0 : 215.0) && IsInRoom(Position + ToTarget * 90.0, 45))
        {
            MoveAlong(SteerAroundAllies(ToTarget, Allies), bLight ? 345.0f : 245.0f);
            return true;
        }
        if (Now >= OrbitUntil)
        {
            OrbitSide *= -1;
            OrbitUntil = Now + Random.FRandRange(0.7f, 1.3f);
        }
        {
            const FVector Side = FVector::CrossProduct(FVector::UpVector, ToTarget) * OrbitSide;
            const FVector Direction = (Side - ToTarget * (Distance < 145.0 ? 0.6 : 0.05)).GetSafeNormal2D();
            if (IsInRoom(Position + Direction * 105, 50))
            {
                MoveAlong(SteerAroundAllies(Direction, Allies), bLight ? 230.0f : 145.0f);
                return true;
            }
        }
        return false;
    }

    case EFSCryptBranch::Hold:
        MoveAlong(FVector::ZeroVector, 0);
        return true;
    }
    return false;
}

AFSCryptEnemy::FAttackWindow AFSCryptEnemy::GetAttackWindow(EAction Attack) const
{
    // The normalized intervals cover the weapon's forward motion or the troll's landing pose.
    switch (Attack)
    {
    case EAction::Cut: return {0.30f, 0.39f, 1.15f};
    case EAction::Stomp: return {0.54f, 0.62f, 1.45f};
    case EAction::Lunge: return {0.285f, 0.345f, 1.35f};
    default:
        return CombatRole == EFSCryptRole::Skirmisher ? FAttackWindow{0.40f, 0.49f, 1.30f}
            : FAttackWindow{0.235f, 0.29f, 1.40f};
    }
}

double AFSCryptEnemy::GetTimeUntilContact(EAction Attack) const
{
    const FAttackWindow Window = GetAttackWindow(Attack);
    const UAnimSequence* Clip = Attacks[static_cast<int32>(Attack) - static_cast<int32>(EAction::Cut)];
    return Clip ? Clip->GetPlayLength() * Window.Open / Window.PlayRate : 0.0;
}

bool AFSCryptEnemy::IsAttacking() const
{
    return Action >= EAction::Cut && Action <= EAction::Lunge;
}

bool AFSCryptEnemy::IsWindingUp() const
{
    return IsAttacking() && !bAttackFinished && PreviousAttackPose < GetAttackWindow(Action).Open;
}

FName AFSCryptEnemy::GetAttackSocket() const
{
    static const FName OrcPalm(TEXT("CATRigRArmPalm"));
    static const FName TrollWeapon(TEXT("weapon_socket"));
    return CombatRole == EFSCryptRole::Skirmisher ? OrcPalm : TrollWeapon;
}

void AFSCryptEnemy::OnAttackPoseEvaluated()
{
    if (!IsAttacking() || bDead || bAttackFinished)
    {
        return;
    }
    UAnimSingleNodeInstance* Anim = GetMesh()->GetSingleNodeInstance();
    if (!Anim || !PlayingClip || Anim->GetAnimationAsset() != PlayingClip)
    {
        bAttackFinished = true;
        Telegraph->SetVisibility(false);
        return;
    }
    const FAttackWindow Window = GetAttackWindow(Action);
    const float Pose = FMath::Clamp(Anim->GetCurrentTime() / PlayingClip->GetPlayLength(), 0.0f, 1.0f);
    if (Action == EAction::Lunge)
    {
        const float Travel = 270.0f * FMath::Clamp((Pose - 0.12f) / (0.30f - 0.12f), 0.0f, 1.0f);
        const FVector Step = StrikeDirection * (Travel - StrikeTravel);
        if (!Step.IsNearlyZero() && IsInRoom(GetActorLocation() + Step, 55.0))
        {
            AddActorWorldOffset(Step, true);
        }
        StrikeTravel = Travel;
    }

    const FVector Center = Action == EAction::Stomp ? StrikeOrigin : GetMesh()->GetSocketLocation(GetAttackSocket());
    const FVector BodyCenter = GetActorLocation() + StrikeDirection * 70.0 + FVector(0, 0, 20);
    if (Pose >= Window.Open)
    {
        Telegraph->SetVisibility(false);
        if (!bHitPlayer && PreviousAttackPose <= Window.Close && Pose > PreviousAttackPose)
        {
            // Clip the swept hitbox to the active frames, even when one frame crosses the whole window.
            const float Span = Pose - PreviousAttackPose;
            const float Start = FMath::Max(0.0f, (Window.Open - PreviousAttackPose) / Span);
            const float End = FMath::Min(1.0f, (Window.Close - PreviousAttackPose) / Span);
            TestAttackHitbox(FMath::Lerp(PreviousHitboxCenter, Center, Start), FMath::Lerp(PreviousHitboxCenter, Center, End),
                FMath::Lerp(PreviousBodyCenter, BodyCenter, Start), FMath::Lerp(PreviousBodyCenter, BodyCenter, End));
        }
    }
    PreviousHitboxCenter = Center;
    PreviousBodyCenter = BodyCenter;
    PreviousAttackPose = Pose;
    bAttackFinished = !Anim->IsPlaying() || Pose >= 1.0f;
}

void AFSCryptEnemy::StartAttack(EAction Attack, const FVector& Target, double Now)
{
    Face(Target);
    MoveDirection = FVector::ZeroVector;
    GetCharacterMovement()->StopMovementImmediately();
    Action = Attack;
    PreviousAttack = Attack;
    StrikeOrigin = GetActorLocation();
    StrikeDirection = GetActorForwardVector().GetSafeNormal2D();
    ExpectedImpact = Now + GetTimeUntilContact(Attack);
    bHitPlayer = false;
    bAttackFinished = true;
    PreviousAttackPose = 0.0f;
    StrikeTravel = 0.0f;
    const int32 Index = static_cast<int32>(Attack) - static_cast<int32>(EAction::Cut);
    if (UAnimSequence* Clip = Attacks[Index])
    {
        SetAnimation(Clip, false, GetAttackWindow(Attack).PlayRate);
        if (GetMesh()->GetSingleNodeInstance())
        {
            bAttackFinished = false;
            PreviousHitboxCenter = GetMesh()->GetSocketLocation(GetAttackSocket());
            PreviousBodyCenter = GetActorLocation() + StrikeDirection * 70.0 + FVector(0, 0, 20);
            ShowTelegraph();
        }
    }
}

void AFSCryptEnemy::ShowTelegraph()
{
    const bool bBoss = CombatRole == EFSCryptRole::Warden;
    const bool bStomp = Action == EAction::Stomp;
    const bool bLunge = Action == EAction::Lunge;
    Telegraph->SetStaticMesh(bStomp ? WarningDisc : WarningLane);
    const double Length = bStomp ? 540.0 : (bLunge ? 540.0 : (bBoss ? 340.0 : 245.0));
    const double Width = bStomp ? Length : (bLunge ? 175.0 : (bBoss ? 380.0 : 215.0));
    const FVector Center = StrikeOrigin + (bStomp ? FVector::ZeroVector : StrikeDirection * (Length * 0.5)) - FVector(0, 0, 85);
    Telegraph->SetWorldLocationAndRotation(Center, StrikeDirection.Rotation());
    Telegraph->SetWorldScale3D(FVector(Length / 100, Width / 100, 0.035));
    Telegraph->SetVisibility(true);
}

void AFSCryptEnemy::TestAttackHitbox(const FVector& From, const FVector& To, const FVector& BodyFrom, const FVector& BodyTo)
{
    ACharacter* Player = UGameplayStatics::GetPlayerCharacter(this, 0);
    if (!IsValid(Player) || PlayerIsDefeated(*Player) || !UFSCharacterActions::HasClearStrike(this, Player))
    {
        return;
    }
    const UCapsuleComponent* Capsule = Player->GetCapsuleComponent();
    const FVector PlayerCenter = Capsule->GetComponentLocation();
    const double PlayerRadius = Capsule->GetScaledCapsuleRadius();
    const double PlayerHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
    const bool bBoss = CombatRole == EFSCryptRole::Warden;
    const bool bStomp = Action == EAction::Stomp;
    const double HalfSegment = PlayerHalfHeight - PlayerRadius;
    const FVector Vertical(0, 0, HalfSegment);
    const auto SweptSphereTouchesPlayer = [&](const FVector& Start, const FVector& End, double Radius)
    {
        FVector StrikePoint;
        FVector CapsulePoint;
        FMath::SegmentDistToSegmentSafe(Start, End, PlayerCenter - Vertical, PlayerCenter + Vertical, StrikePoint, CapsulePoint);
        return FVector::DistSquared(StrikePoint, CapsulePoint) <= FMath::Square(Radius + PlayerRadius);
    };
    bool bTouchesPlayer = false;
    if (bStomp)
    {
        // Landing creates a short cylinder at floor level. Test its distance to the capsule's core segment.
        const double FloorZ = StrikeOrigin.Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        const double HorizontalGap = FMath::Max(0.0, FVector::Dist2D(PlayerCenter, StrikeOrigin) - 270.0);
        const double VerticalGap = FMath::Max3(0.0, PlayerCenter.Z - HalfSegment - (FloorZ + 58.0), FloorZ - PlayerCenter.Z - HalfSegment);
        bTouchesPlayer = FMath::Square(HorizontalGap) + FMath::Square(VerticalGap) <= FMath::Square(PlayerRadius);
    }
    else if (GetMesh()->DoesSocketExist(GetAttackSocket()))
    {
        const double WeaponRadius = bBoss ? 65.0 : (CombatRole == EFSCryptRole::Vanguard ? 57.0 : 49.0);
        bTouchesPlayer = SweptSphereTouchesPlayer(From, To, WeaponRadius);
    }
    bTouchesPlayer |= Action == EAction::Lunge && SweptSphereTouchesPlayer(BodyFrom, BodyTo, 62.0);
    if (!bTouchesPlayer)
    {
        return;
    }
    bHitPlayer = true;
    const float Damage = bBoss ? (bStomp ? 26 : 21) : (CombatRole == EFSCryptRole::Vanguard ? 16 : 12);
    UGameplayStatics::ApplyDamage(Player, Damage, GetController(), this, nullptr);
}

float AFSCryptEnemy::TakeDamage(float Damage, const FDamageEvent& Event, AController* DamageInstigator, AActor* Causer)
{
    if (bDead || Damage <= 0)
    {
        return 0;
    }
    if (Action == EAction::Evade)
    {
        return 0;
    }
    const FVector From = Causer ? (Causer->GetActorLocation() - GetActorLocation()).GetSafeNormal2D() : FVector::ZeroVector;
    if (Action == EAction::Guard && FVector::DotProduct(From, GetActorForwardVector()) > 0.30)
    {
        Damage *= 0.30f;
    }

    // Update defeat before Super broadcasts the standard death-credit damage callback.
    Health = FMath::Max(0.0f, Health - Damage);
    bDead = Health <= 0;
    if (bDead)
    {
        if (AAIController* AI = Cast<AAIController>(GetController()); AI && AI->GetBrainComponent())
        {
            AI->GetBrainComponent()->StopLogic(TEXT("Defeated"));
        }
        Telegraph->SetVisibility(false);
        Label->SetVisibility(false);
        GetCharacterMovement()->DisableMovement();
        SetActorEnableCollision(false);
        SetAnimation(Death, false);
        SetLifeSpan(1.6f);
    }
    else
    {
        const double Now = GetWorld()->GetTimeSeconds();
        Pressure = Now - LastHit < 2.0 ? FMath::Min(Pressure + 1, 3) : 1;
        LastHit = Now;
        if (Action == EAction::None && CombatRole != EFSCryptRole::Warden && Pressure == 1)
        {
            MoveDirection = FVector::ZeroVector;
            Action = EAction::Recover;
            ActionEnd = Now + 0.22;
            SetAnimation(React, false, 1.35f);
        }
    }
    return Super::TakeDamage(Damage, Event, DamageInstigator, Causer);
}

void AFSCryptEnemy::UpdateLabel(ACharacter* Player)
{
    const bool bVisible = Player && FVector::DistSquared2D(Player->GetActorLocation(), GetActorLocation()) < FMath::Square(820.0)
        && UFSCharacterActions::HasClearStrike(this, Player);
    Label->SetVisibility(bVisible);
    if (!bVisible)
    {
        return;
    }
    const FRotator ToPlayer = (Player->GetActorLocation() - GetActorLocation()).Rotation();
    Label->SetWorldRotation(FRotator(0, ToPlayer.Yaw, 0));
    const TCHAR* Name = CombatRole == EFSCryptRole::Warden ? TEXT("CRYPT WARDEN") : (CombatRole == EFSCryptRole::Vanguard ? TEXT("VANGUARD") : TEXT("SKIRMISHER"));
    const TCHAR* State = TEXT("");
    switch (Action)
    {
    case EAction::Guard: State = TEXT("  GUARD"); break;
    case EAction::Evade: State = TEXT("  EVADE"); break;
    case EAction::Stomp: State = TEXT("  STOMP"); break;
    case EAction::Lunge: State = TEXT("  LUNGE"); break;
    case EAction::Sweep: State = TEXT("  SWEEP"); break;
    default: break;
    }
    const FString Status = FString::Printf(TEXT("%s  %d/%d%s"), Name, FMath::CeilToInt(Health), FMath::CeilToInt(MaxHealth), State);
    if (Status != LastLabel)
    {
        LastLabel = Status;
        Label->SetText(FText::FromString(Status));
        Label->SetTextRenderColor(Action == EAction::Guard ? FColor(111, 194, 255) : (CombatRole == EFSCryptRole::Warden ? FColor(255, 190, 95) : FColor::White));
    }
}
