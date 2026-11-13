#include "FSCryptEnemy.h"

#include "FSCryptBehavior.h"
#include "FSCharacterActions.h"
#include "AIController.h"
#include "Animation/AnimSequence.h"
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

void AFSCryptEnemy::Configure(EFSCryptRole InRole, const FVector& RoomCenter, const FVector2D& RoomHalfExtent, int32 Seed)
{
    CombatRole = InRole;
    Home = GetActorLocation();
    BoundsCenter = RoomCenter;
    BoundsHalfExtent = RoomHalfExtent;
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
    Attacks = {EnemyClips[Start + 3], EnemyClips[Start + 4], EnemyClips[Start + 5], EnemyClips[Start + 6]};
    React = EnemyClips[Start + 7];
    Death = EnemyClips[Start + 8];
}

void AFSCryptEnemy::BeginPlay()
{
    Super::BeginPlay();
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

bool AFSCryptEnemy::AnotherEnemyIsAttacking() const
{
    for (TActorIterator<AFSCryptEnemy> Other(GetWorld()); Other; ++Other)
    {
        if (*Other != this && !Other->bDead && Other->BoundsCenter.Equals(BoundsCenter)
            && Other->Action >= EAction::Cut && Other->Action <= EAction::Lunge)
        {
            return true;
        }
    }
    return false;
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
        if (Now < ActionEnd)
        {
            return true;
        }
        Telegraph->SetVisibility(false);
        MoveDirection = FVector::ZeroVector;
        if (Action >= EAction::Cut && Action <= EAction::Lunge)
        {
            if (Player && !PlayerIsDefeated(*Player))
            {
                ResolveHit(*Player);
            }
            const bool bFollowUp = bCombo && bEngaged && Distance < 340.0 && IsInRoom(Target);
            bCombo = false;
            if (bFollowUp)
            {
                StartAttack(EAction::Sweep, Target, Now);
            }
            else
            {
                Action = EAction::Recover;
                ActionEnd = Now + (bBoss ? 0.90 : 0.67);
                NextAttack = ActionEnd + Random.FRandRange(0.1f, 0.42f);
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
        if (!bEngaged || !IsInRoom(Target) || Now < NextAttack || Distance > (bBoss ? 560.0 : 235.0) || AnotherEnemyIsAttacking())
        {
            return false;
        }
        if (bBoss)
        {
            const bool bEnraged = Health <= MaxHealth * 0.5f;
            const EAction Attack = Distance > 320.0 ? EAction::Lunge
                : PreviousAttack == EAction::Stomp ? EAction::Sweep : EAction::Stomp;
            bCombo = bEnraged && Attack != EAction::Sweep;
            StartAttack(Attack, Target, Now);
        }
        else
        {
            StartAttack(bLight && PreviousAttack != EAction::Cut ? EAction::Cut : EAction::Sweep, Target, Now);
        }
        return true;

    case EFSCryptBranch::Position:
        if (!bEngaged)
        {
            return false;
        }
        Face(Target);
        if (Distance > (bLight ? 170.0 : 215.0) && IsInRoom(Position + ToTarget * 90.0, 45))
        {
            MoveAlong(ToTarget, bLight ? 345.0f : 245.0f);
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
                MoveAlong(Direction, bLight ? 230.0f : 145.0f);
                return true;
            }
        }
        return false;

    case EFSCryptBranch::Hold:
        MoveAlong(FVector::ZeroVector, 0);
        return true;
    }
    return false;
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
    const float Windup = Attack == EAction::Cut ? 0.47f : (Attack == EAction::Stomp ? 1.02f : (Attack == EAction::Lunge ? 0.85f : 0.73f));
    ActionEnd = Now + Windup;
    const int32 Index = static_cast<int32>(Attack) - static_cast<int32>(EAction::Cut);
    if (UAnimSequence* Clip = Attacks[Index])
    {
        SetAnimation(Clip, false, FMath::Clamp(Clip->GetPlayLength() * 0.52f / Windup, 0.5f, 1.8f));
    }
    ShowTelegraph();
}

void AFSCryptEnemy::ShowTelegraph()
{
    const bool bBoss = CombatRole == EFSCryptRole::Warden;
    const bool bStomp = Action == EAction::Stomp;
    const bool bLunge = Action == EAction::Lunge;
    Telegraph->SetStaticMesh(bStomp ? WarningDisc : WarningLane);
    const double Length = bStomp ? 540.0 : (bLunge ? 620.0 : (bBoss ? 340.0 : 245.0));
    const double Width = bStomp ? Length : (bLunge ? 175.0 : (bBoss ? 380.0 : 215.0));
    const FVector Center = StrikeOrigin + (bStomp ? FVector::ZeroVector : StrikeDirection * (Length * 0.5)) - FVector(0, 0, 85);
    Telegraph->SetWorldLocationAndRotation(Center, StrikeDirection.Rotation());
    Telegraph->SetWorldScale3D(FVector(Length / 100, Width / 100, 0.035));
    Telegraph->SetVisibility(true);
}

void AFSCryptEnemy::ResolveHit(ACharacter& Player)
{
    const bool bBoss = CombatRole == EFSCryptRole::Warden;
    const FVector Offset = Player.GetActorLocation() - StrikeOrigin;
    const double Along = FVector::DotProduct(Offset, StrikeDirection);
    const FVector Right = FVector::CrossProduct(FVector::UpVector, StrikeDirection);
    const double Across = FMath::Abs(FVector::DotProduct(Offset, Right));
    const double Range = bBoss ? 340.0 : 245.0;
    const bool bStomp = Action == EAction::Stomp;
    const bool bLunge = Action == EAction::Lunge;
    const bool bInside = bStomp ? Offset.SizeSquared2D() <= FMath::Square(270.0)
        : Along >= 0 && Along <= (bLunge ? 620.0 : Range) && Across <= (bLunge ? 87.5 : (bBoss ? 190.0 : 107.5));
    const bool bClear = UFSCharacterActions::HasClearStrike(this, &Player);
    if (bInside && bClear && FMath::Abs(Offset.Z) <= (bStomp ? 105.0 : 160.0))
    {
        const float Damage = bBoss ? (bStomp ? 26 : 21) : (CombatRole == EFSCryptRole::Vanguard ? 16 : 12);
        UGameplayStatics::ApplyDamage(&Player, Damage, GetController(), this, nullptr);
    }
    if (bLunge)
    {
        const FVector Destination = StrikeOrigin + StrikeDirection * FMath::Clamp(Along - 120.0, 100.0, 400.0);
        if (IsInRoom(Destination, 65))
        {
            GetCharacterMovement()->AddImpulse(StrikeDirection * 650.0, true);
        }
    }
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
