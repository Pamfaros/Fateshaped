#include "FSClassCombat.h"

#include "FSCharacterActions.h"
#include "FSCryptEnemy.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequenceBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SaveGame.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/EngineTypes.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "UObject/UnrealType.h"

namespace
{
constexpr int32 Basic = 0, Dodge = 1, First = 2, Second = 3;
constexpr float SharedDodgeDistance = 340, SharedDodgeTime = 0.22f;
const TCHAR* AxeClip = TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_ChargedAttack.MM_ChargedAttack");
const TCHAR* SwordClip = TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01.MM_Attack_01");
const TCHAR* FollowClip = TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_02.MM_Attack_02");
const TCHAR* CastClip = TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_03.MM_Attack_03");
const TCHAR* DashClip = TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_Dash.MM_Dash");
const FName PracticeTag(TEXT("PracticeTarget"));

template<typename T> const T* Property(const UObject& Owner, const TCHAR* Name)
{
    return FindFProperty<T>(Owner.GetClass(), Name);
}

void SetString(UObject& Owner, const TCHAR* Name, const FString& Value)
{
    if (const auto* Field = Property<FStrProperty>(Owner, Name))
    {
        Field->SetPropertyValue_InContainer(&Owner, Value);
    }
}

bool Flag(const UObject& Owner, const TCHAR* Name)
{
    const auto* Field = Property<FBoolProperty>(Owner, Name);
    return Field && Field->GetPropertyValue_InContainer(&Owner);
}

void CallEvent(ACharacter& Owner, const TCHAR* Name)
{
    if (UFunction* Function = Owner.FindFunction(Name))
    {
        Owner.ProcessEvent(Function, nullptr);
    }
}

FCollisionObjectQueryParams Walls()
{
    FCollisionObjectQueryParams Objects(ECC_WorldStatic);
    Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
    return Objects;
}

bool IsEnemy(const AActor& Actor)
{
    if (const auto* Enemy = Cast<AFSCryptEnemy>(&Actor))
    {
        return !Enemy->IsDefeated();
    }
    return !Actor.IsHidden() && Actor.GetClass()->GetName() == TEXT("BP_MeleeEnemy_C") && !Flag(Actor, TEXT("bDead"));
}

AStaticMeshActor* Marker(UWorld& World, const FVector& Position, const FVector& Scale, bool bCylinder = false)
{
    auto* Actor = World.SpawnActor<AStaticMeshActor>(Position, FRotator::ZeroRotator);
    if (Actor)
    {
        Actor->SetActorEnableCollision(false);
        auto* Mesh = Actor->GetStaticMeshComponent();
        Mesh->SetMobility(EComponentMobility::Movable);
        Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, bCylinder
            ? TEXT("/Engine/BasicShapes/Cylinder.Cylinder") : TEXT("/Engine/BasicShapes/Sphere.Sphere")));
        Mesh->SetMaterial(0, LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Combat/FX/MI_FX_Wave.MI_FX_Wave")));
        Actor->SetActorScale3D(Scale);
    }
    return Actor;
}

UFSClassCombat* Combat(ACharacter* Character, bool bCreate = false)
{
    if (!IsValid(Character))
    {
        return nullptr;
    }
    auto* Component = Character->FindComponentByClass<UFSClassCombat>();
    if (!Component && bCreate)
    {
        Component = NewObject<UFSClassCombat>(Character);
        Character->AddInstanceComponent(Component);
        Component->RegisterComponent();
    }
    return Component;
}
}

UFSClassCombat::UFSClassCombat()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

ACharacter& UFSClassCombat::Player() const
{
    return *CastChecked<ACharacter>(GetOwner());
}

void UFSClassCombat::BeginPlay()
{
    Super::BeginPlay();
    OriginalWalkSpeed = Player().GetCharacterMovement()->MaxWalkSpeed;
    if (auto* Arm = Player().FindComponentByClass<USpringArmComponent>())
    {
        OriginalArm = Arm->TargetArmLength;
    }
    SpawnAltar();
}

void UFSClassCombat::EndPlay(const EEndPlayReason::Type Reason)
{
    if (auto* Controller = InputController.Get(); Controller && Input)
    {
        Controller->PopInputComponent(Input);
    }
    ClearEffects();
    Super::EndPlay(Reason);
}

float UFSClassCombat::Value(const TCHAR* Name) const
{
    if (const auto* Field = Property<FNumericProperty>(Player(), Name); Field && Field->IsFloatingPoint())
    {
        return Field->GetFloatingPointPropertyValue(Field->ContainerPtrToValuePtr<void>(&Player()));
    }
    return 0;
}

void UFSClassCombat::SetValue(const TCHAR* Name, float Amount)
{
    if (const auto* Field = Property<FNumericProperty>(Player(), Name); Field && Field->IsFloatingPoint())
    {
        Field->SetFloatingPointPropertyValue(Field->ContainerPtrToValuePtr<void>(&Player()), Amount);
    }
}

void UFSClassCombat::SetFlag(const TCHAR* Name, bool bValue)
{
    if (const auto* Field = Property<FBoolProperty>(Player(), Name))
    {
        Field->SetPropertyValue_InContainer(&Player(), bValue);
    }
}

void UFSClassCombat::Message(const FString& Text) const
{
    SetString(Player(), TEXT("Message"), Text);
}

void UFSClassCombat::Restore(bool bEvolution)
{
    if (!bEvolution)
    {
        Cancel();
        ClearEffects();
        for (double& Availability : Cooldown) Availability = 0;
        LastStaminaSpend = -10;
    }
    USaveGame* Profile = FSEssence::Profile(Player());
    const FString Path = FSEssence::History(Profile);
    bool bSupported;
    const FSEssence::FClass& NewClass = FSEssence::Resolve(Path, bSupported);
    auto SetPool = [this, bEvolution](const TCHAR* Current, const TCHAR* Maximum, float Cap)
    {
        const float Initial = bEvolution && Value(Maximum) > 0 ? FMath::Clamp(Value(Current), 0.0f, Cap) : Cap;
        SetValue(Maximum, Cap);
        SetValue(Current, Initial);
    };
    SetPool(TEXT("Health"), TEXT("MaxHealth"), NewClass.Vitality);
    SetPool(TEXT("Stamina"), TEXT("MaxStamina"), NewClass.Stamina);
    SetPool(TEXT("Mana"), TEXT("MaxMana"), NewClass.Mana);
    Class = &NewClass;
    if (bSupported && Class->Kit != EKit::Unshaped) FSEssence::UpdateCachedId(Profile, *Class);
    SetString(Player(), TEXT("ClassName"), Class->Name);
    SetString(Player(), TEXT("History"), Path);
    // The existing quest graph uses this flag as its class-selected gate.
    SetFlag(TEXT("bSwordsman"), Class->Kit != EKit::Unshaped);
    SetFlag(TEXT("bSpellsword"), Class->Kit == EKit::SoulsBlade);
    EquipClassVisual();
    if (!bSupported)
    {
        Message(TEXT("Unsupported essence history. Using the last known path."));
    }
}

char UFSClassCombat::Branch() const
{
    return Class ? static_cast<char>(Class->Path[0]) : 0;
}

bool UFSClassCombat::Unavailable() const
{
    return Flag(Player(), TEXT("bDead")) || Flag(Player(), TEXT("bWon"));
}

bool UFSClassCombat::Grounded() const { return Player().GetCharacterMovement()->IsMovingOnGround(); }

bool UFSClassCombat::IsGuild() const { return UGameplayStatics::GetCurrentLevelName(this) == TEXT("L_Guild"); }

bool UFSClassCombat::Ready(int32 Slot) const { return GetWorld()->GetTimeSeconds() >= Cooldown[Slot]; }

bool UFSClassCombat::CanPay(float V, float S, float M) const
{
    return (V <= 0 || IsGuild() || Value(TEXT("Health")) >= V + 1)
        && Value(TEXT("Stamina")) >= S && Value(TEXT("Mana")) >= M;
}

bool UFSClassCombat::Spend(float V, float S, float M)
{
    if (!CanPay(V, S, M))
    {
        Message(V > 0 && !IsGuild() && Value(TEXT("Health")) < V + 1 ? TEXT("Not enough Vitality")
            : Value(TEXT("Stamina")) < S ? TEXT("Not enough Stamina") : TEXT("Not enough Mana"));
        return false;
    }
    if (V > 0 && !IsGuild()) SetValue(TEXT("Health"), Value(TEXT("Health")) - V);
    if (S > 0)
    {
        SetValue(TEXT("Stamina"), Value(TEXT("Stamina")) - S);
        LastStaminaSpend = GetWorld()->GetTimeSeconds();
    }
    if (M > 0) SetValue(TEXT("Mana"), Value(TEXT("Mana")) - M);
    return true;
}

void UFSClassCombat::PlaySound(const TCHAR* Name) const
{
    const FString Path = FString::Printf(TEXT("/Game/Combat/Audio/%s.%s"), Name, Name);
    if (auto* Sound = LoadObject<USoundBase>(nullptr, *Path))
    {
        UGameplayStatics::PlaySoundAtLocation(this, Sound, Player().GetActorLocation());
    }
}

void UFSClassCombat::Start(EMove Next, float End, const TCHAR* Clip)
{
    Move = Next;
    Age = 0;
    EndTime = End;
    bCommitted = false;
    bHadTurnCue = false;
    HitActors.Reset();
    StrikePoint = FVector::ZeroVector;
    LastBlade = FVector::ZeroVector;
    SetFlag(TEXT("bAttacking"), Next != EMove::Dodge);
    SetFlag(TEXT("bDodging"), Next == EMove::Dodge);
    if (Clip)
    {
        if (auto* Animation = LoadObject<UAnimSequenceBase>(nullptr, Clip))
        {
            UFSCharacterActions::PlayActionAnimation(&Player(), Animation, Animation->GetPlayLength() / FMath::Max(0.1f, End));
        }
    }
    if (Next == EMove::ShieldHold || Next == EMove::StoneHold || Next == EMove::OrbHold || Next == EMove::BendHold)
    {
        UFSCharacterActions::BeginAttackFacing(&Player());
        PlaySound(TEXT("SFX_Swing"));
    }
}

void UFSClassCombat::Finish()
{
    Move = EMove::Idle;
    MoveTime = 0;
    MoveAge = 0;
    SetFlag(TEXT("bAttacking"), false);
    SetFlag(TEXT("bDodging"), false);
    UFSCharacterActions::EndAttackFacing(&Player());
}

void UFSClassCombat::Cancel()
{
    Player().StopAnimMontage();
    if (auto* Mesh = Player().GetMesh(); Mesh && Mesh->GetAnimInstance())
    {
        Mesh->GetAnimInstance()->Montage_Stop(0.06f);
    }
    BufferedBasicUntil = 0;
    Finish();
}

FVector UFSClassCombat::AimDirection(bool bVertical) const
{
    const auto* Controller = Cast<APlayerController>(Player().GetController());
    if (!bAim || !bVertical || !Controller || !Controller->PlayerCameraManager)
    {
        return UFSCharacterActions::CameraHeading(&Player());
    }
    const FVector Camera = Controller->PlayerCameraManager->GetCameraLocation();
    const FVector Far = Camera + Controller->PlayerCameraManager->GetCameraRotation().Vector() * 2500;
    FHitResult AimHit;
    const FCollisionQueryParams Query(SCENE_QUERY_STAT(FSClassAim), false, &Player());
    const FVector Point = GetWorld()->LineTraceSingleByChannel(AimHit, Camera, Far, ECC_Visibility, Query) ? AimHit.ImpactPoint : Far;
    return (Point - Hand()).GetSafeNormal();
}

FVector UFSClassCombat::Hand() const
{
    return Player().GetMesh()->GetSocketLocation(TEXT("hand_r"));
}

FVector UFSClassCombat::MoveInput(FVector Fallback) const
{
    FVector Direction = Player().GetPendingMovementInputVector().GetSafeNormal2D();
    if (Direction.IsNearlyZero())
    {
        Direction = Player().GetCharacterMovement()->GetCurrentAcceleration().GetSafeNormal2D();
    }
    return Direction.IsNearlyZero() ? Fallback : Direction;
}

bool UFSClassCombat::MovePlayer(const FVector& Offset, bool bRequireFloor)
{
    FCollisionQueryParams Query(SCENE_QUERY_STAT(FSClassMove), false, &Player());
    const FVector Target = Player().GetActorLocation() + Offset;
    FHitResult Floor;
    if (bRequireFloor && (!GetWorld()->LineTraceSingleByObjectType(Floor, Target, Target - FVector(0, 0, 220), Walls(), Query)
        || Floor.ImpactNormal.Z < Player().GetCharacterMovement()->GetWalkableFloorZ()
        || (Move != EMove::Leap && Target.Z - Floor.ImpactPoint.Z > Player().GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 48)))
    {
        return false;
    }
    FHitResult Hit;
    Player().AddActorWorldOffset(Offset, true, &Hit);
    return !Hit.bBlockingHit;
}

void UFSClassCombat::Travel(const FVector& Direction, float Distance, float Duration, float Arc)
{
    TravelHeading = Direction.GetSafeNormal2D();
    MoveTime = Duration;
    MoveAge = 0;
    MoveDistance = Distance;
    MoveArc = Arc;
    Player().GetCharacterMovement()->StopMovementImmediately();
}

void UFSClassCombat::Attack()
{
    if (Unavailable() || bSelecting || !Branch() || !Grounded()) return;
    if (Move == EMove::Slip && Age >= 0.25f)
    {
        BufferedBasicUntil = GetWorld()->GetTimeSeconds() + 0.20;
        return;
    }
    if (Move != EMove::Idle) return;
    if (!Ready(Basic)) { Message(TEXT("Attack recovering")); return; }
    Heading = AimDirection(Branch() == 'M');
    UFSCharacterActions::BeginAttackFacing(&Player());
    const EMove AttackMove = Branch() == 'V' ? EMove::Axe : Branch() == 'S' ? EMove::Sword : EMove::Spark;
    Start(AttackMove, AttackMove == EMove::Axe ? 0.95f : AttackMove == EMove::Sword ? 0.52f : 0.38f,
        AttackMove == EMove::Axe ? AxeClip : AttackMove == EMove::Sword ? SwordClip : CastClip);
    Cooldown[Basic] = GetWorld()->GetTimeSeconds() + (AttackMove == EMove::Axe ? 1.20 : 0.65);
    PlaySound(TEXT("SFX_Swing"));
}

void UFSClassCombat::Evade()
{
    if (Unavailable() || bSelecting || !Branch() || !Grounded() || !Ready(Dodge)) return;
    const bool bCanCancel = Move == EMove::Idle || Move == EMove::Axe || Move == EMove::Sword || Move == EMove::ShieldHold
        || Move == EMove::OrbHold || (Move == EMove::Orb && !bCommitted && !bStepped)
        || Move == EMove::StoneHold || (Move == EMove::Thread && !bCommitted)
        || (Move == EMove::Recall && !bCommitted) || (Move == EMove::Slip && Age >= 0.21f)
        || (Move == EMove::Wave && (!bCommitted || Age >= 0.29f)) || (Move == EMove::Seal && !bCommitted)
        || Move == EMove::BendHold || (Move == EMove::Bend && !bCommitted)
        || (Move == EMove::Spark && !bCommitted);
    if (!bCanCancel || (Move == EMove::OrbHold && bStepped && MoveTime > 0) ||
        !Spend(0, Branch() == 'S' || Class->Kit == EKit::SpellWeaver ? 25 : 0, 0)) return;
    const bool bBackstep = Branch() == 'V';
    const FVector Direction = MoveInput(bBackstep ? -Player().GetActorForwardVector() : AimDirection());
    Cancel();
    Player().SetActorRotation(Direction.Rotation());
    Start(EMove::Dodge, bBackstep ? 0.30f : SharedDodgeTime, DashClip);
    Travel(Direction, SharedDodgeDistance * (bBackstep ? 0.60f : 1.0f), EndTime - 0.001f);
    Cooldown[Dodge] = GetWorld()->GetTimeSeconds() + (bBackstep ? 0.80 : Branch() == 'M' ? 1.0 : EndTime);
}

void UFSClassCombat::SkillOne()
{
    if (Unavailable() || bSelecting || !Branch() || !Grounded()) return;
    if (!Ready(First)) { Message(TEXT("Skill recovering")); return; }
    const double Now = GetWorld()->GetTimeSeconds();
    if (Branch() == 'V' && (Move == EMove::Idle || (Move == EMove::Axe && Age >= 0.42f)) && Spend(15, 0, 0))
    {
        Heading = Player().GetActorForwardVector().GetSafeNormal2D();
        Start(EMove::Leap, 0.55f, AxeClip);
        Travel(Heading, 320, 0.55f, 55);
        Cooldown[First] = Now + 3;
        PlaySound(TEXT("SFX_Swing"));
    }
    else if (Branch() == 'S' && Move == EMove::Idle && Spend(0, 20, 0))
    {
        Start(EMove::ShieldHold, 0.65f);
        Cooldown[First] = Now + 1.20;
    }
    else if (Branch() == 'M' && Move == EMove::Idle)
    {
        if (!CanPay(0, 0, 20)) { Message(TEXT("Not enough Mana")); return; }
        bStepped = false;
        Start(EMove::OrbHold, 0.80f);
    }
}

void UFSClassCombat::ReleaseSkillOne()
{
    if (Move == EMove::ShieldHold)
    {
        const bool bLong = Age >= 0.30f;
        Heading = MoveInput(Player().GetActorForwardVector().GetSafeNormal2D());
        const float Duration = bLong ? 0.26f : 0.16f;
        Start(EMove::ShieldStep, Duration + 0.12f, DashClip);
        Travel(Heading, bLong ? 180 : 90, Duration);
        PlaySound(TEXT("SFX_Swing"));
    }
    else if (Move == EMove::OrbHold)
    {
        bFull = Age >= 0.45f;
        Heading = AimDirection(true);
        UFSCharacterActions::BeginAttackFacing(&Player());
        Start(EMove::Orb, 0.28f, CastClip);
    }
}

void UFSClassCombat::SkillTwo()
{
    if (Unavailable() || bSelecting || !Branch()) return;
    const double Now = GetWorld()->GetTimeSeconds();
    const bool bAxeRecovery = Move == EMove::Axe && Age >= 0.42f;
    const bool bSwordRecovery = Move == EMove::Sword && Age >= 0.24f;
    if (Class->Kit == EKit::BloodMage && AnchorUntil > Now && Move == EMove::Idle && Grounded())
    {
        Start(EMove::Recall, 0.20f, CastClip);
        return;
    }
    if (!Grounded()) return;
    if (!Ready(Second)) { Message(TEXT("Skill recovering")); return; }
    switch (Class->Kit)
    {
    case EKit::Juggernaut:
        if ((Move == EMove::Idle || bAxeRecovery) && CanPay(18, 0, 0))
        {
            Heading = AimDirection();
            Start(EMove::StoneHold, 0.80f);
        }
        else if (Move == EMove::Idle || bAxeRecovery) Message(TEXT("Not enough Vitality"));
        break;
    case EKit::Berserk:
        if (bAxeRecovery && Age <= 0.78f && Spend(0, 24, 0))
        {
            const FVector Right = FVector::CrossProduct(FVector::UpVector, Heading);
            const FVector InputDirection = MoveInput(Heading.RotateAngleAxis(60, FVector::UpVector));
            const float Angle = FMath::RadiansToDegrees(FMath::Atan2(FVector::DotProduct(Right, InputDirection), FVector::DotProduct(Heading, InputDirection)));
            const FVector Direction = Heading.RotateAngleAxis(FMath::Clamp(Angle, -120.0f, 120.0f), FVector::UpVector);
            Start(EMove::Pivot, 0.46f, FollowClip);
            Travel(Direction, 185, 0.30f);
            Cooldown[Second] = Now + 1.8;
            PlaySound(TEXT("SFX_Swing"));
        }
        else if (Move == EMove::Idle) Message(TEXT("Follow a heavy swing"));
        break;
    case EKit::BloodMage:
        if (Move == EMove::Idle || bAxeRecovery)
        {
            if (!CanPay(12, 0, 18)) { Message(TEXT("Blood Thread requires 12 Vitality and 18 Mana")); break; }
            Heading = AimDirection(true).GetSafeNormal2D();
            Start(EMove::Thread, 0.32f, CastClip);
        }
        break;
    case EKit::Mercenary:
        if ((Move == EMove::Idle || bSwordRecovery || Move == EMove::ShieldHold) && Spend(12, 0, 0))
        {
            Heading = AimDirection();
            UFSCharacterActions::BeginAttackFacing(&Player());
            Start(EMove::PaidStand, 0.80f, FollowClip);
            Cooldown[Second] = Now + 3;
        }
        break;
    case EKit::Swordsman:
        if ((Move == EMove::Idle || bSwordRecovery) && Spend(0, 16, 0))
        {
            Heading = AimDirection();
            UFSCharacterActions::BeginAttackFacing(&Player());
            Start(EMove::Slip, 0.34f, FollowClip);
            Travel(MoveInput(FVector::CrossProduct(FVector::UpVector, Heading)), 160, 0.20f);
            Cooldown[Second] = Now + 0.8;
            PlaySound(TEXT("SFX_Swing"));
        }
        break;
    case EKit::SoulsBlade:
        if (Move == EMove::Idle || bSwordRecovery)
        {
            if (!CanPay(0, 0, 30)) { Message(TEXT("Not enough Mana")); break; }
            Heading = AimDirection();
            UFSCharacterActions::BeginAttackFacing(&Player());
            Start(EMove::Wave, 0.55f, CastClip);
        }
        break;
    case EKit::Paladin:
        if (Move == EMove::Idle)
        {
            FVector Ground;
            if (!WardGround(Ground)) { Message(TEXT("Choose clear ground for the seal")); break; }
            if (!CanPay(0, 0, 18)) { Message(TEXT("Not enough Mana")); break; }
            WardCenter = Ground;
            Start(EMove::Seal, 0.40f, CastClip);
        }
        break;
    case EKit::SpellWeaver:
        if (Move == EMove::OrbHold && !bStepped)
        {
            const FVector Direction = MoveInput(FVector::CrossProduct(FVector::UpVector, AimDirection()));
            FHitResult Hit;
            FCollisionQueryParams Query(SCENE_QUERY_STAT(FSWeaveStep), false, &Player());
            auto* Capsule = Player().GetCapsuleComponent();
            if (GetWorld()->SweepSingleByChannel(Hit, Player().GetActorLocation(), Player().GetActorLocation() + Direction * 30,
                FQuat::Identity, Capsule->GetCollisionObjectType(), Capsule->GetCollisionShape(), Query,
                FCollisionResponseParams(Capsule->GetCollisionResponseToChannels())))
            {
                Message(TEXT("Path blocked"));
                break;
            }
            if (!Spend(0, 22, 0)) break;
            bStepped = true;
            Travel(Direction, 210, 0.22f);
            Cooldown[Second] = Now + 1.2;
            if (auto* Trail = Marker(*GetWorld(), Player().GetActorLocation() - FVector(0, 0, 82), FVector(0.5, 0.5, 0.025)))
            {
                Trail->SetLifeSpan(0.32f);
            }
        }
        break;
    case EKit::Mage:
        if (Move == EMove::Idle)
        {
            if (!CanPay(0, 0, 22)) { Message(TEXT("Not enough Mana")); break; }
            CurveSide = 0;
            Start(EMove::BendHold, 0.90f);
        }
        break;
    default:
        break;
    }
}

void UFSClassCombat::ReleaseSkillTwo()
{
    if (Move == EMove::StoneHold)
    {
        if (!Spend(18, 0, 0)) { Finish(); return; }
        bFull = Age >= 0.50f;
        Heading = AimDirection();
        Player().SetActorRotation(Heading.Rotation());
        Start(EMove::Stone, 0.55f, AxeClip);
        Cooldown[Second] = GetWorld()->GetTimeSeconds() + 3.8;
    }
    else if (Move == EMove::BendHold)
    {
        Heading = AimDirection(true);
        Heading = FRotator(FMath::Clamp(Heading.Rotation().Pitch, -35.0, 35.0), Heading.Rotation().Yaw, 0).Vector();
        Start(EMove::Bend, 0.32f, CastClip);
    }
}

float UFSClassCombat::HitTarget(AActor& Target, float Damage)
{
    if (Target.ActorHasTag(PracticeTag))
    {
        if (auto* Effect = LoadClass<AActor>(nullptr, TEXT("/Game/Combat/FX/BP_FX_Hit.BP_FX_Hit_C")))
        {
            GetWorld()->SpawnActor<AActor>(Effect, Target.GetActorLocation(), FRotator::ZeroRotator);
        }
        return 0;
    }
    const auto* Enemy = Cast<AFSCryptEnemy>(&Target);
    const float Before = Enemy ? Enemy->GetHealth() : 0;
    const float Reported = UGameplayStatics::ApplyDamage(&Target, Damage, Player().GetController(), &Player(), nullptr);
    if (Reported > 0)
    {
        if (auto* Effect = LoadClass<AActor>(nullptr, TEXT("/Game/Combat/FX/BP_FX_Hit.BP_FX_Hit_C")))
        {
            GetWorld()->SpawnActor<AActor>(Effect, Target.GetActorLocation(), FRotator::ZeroRotator);
        }
    }
    return Enemy ? FMath::Max(0.0f, Before - Enemy->GetHealth()) : Reported;
}

void UFSClassCombat::BladeContact(float From, float To, float Radius, float Damage, int32 Limit)
{
    const FVector Current = StrikePoint.IsNearlyZero()
        ? Hand() + Player().GetActorForwardVector() * (Branch() == 'V' ? 65 : 52) : StrikePoint;
    const FVector Previous = LastBlade.IsNearlyZero() ? Current : LastBlade;
    LastBlade = Current;
    if (Age < From || PreviousAge > To || HitActors.Num() >= Limit) return;
    const float Span = FMath::Max(KINDA_SMALL_NUMBER, Age - PreviousAge);
    const FVector Start = FMath::Lerp(Previous, Current, FMath::Clamp((From - PreviousAge) / Span, 0.0f, 1.0f));
    const FVector End = FMath::Lerp(Previous, Current, FMath::Clamp((To - PreviousAge) / Span, 0.0f, 1.0f));
    for (TActorIterator<AActor> It(GetWorld()); It && HitActors.Num() < Limit; ++It)
    {
        AActor* Target = *It;
        if (HitActors.Contains(Target) || !(IsEnemy(*Target) || Target->ActorHasTag(PracticeTag))
            || !UFSCharacterActions::HasClearStrike(&Player(), Target)) continue;
        const auto* Capsule = Target->FindComponentByClass<UCapsuleComponent>();
        const FVector Center = Target->GetActorLocation();
        const double TargetRadius = Capsule ? Capsule->GetScaledCapsuleRadius() : 33;
        const FVector Vertical = Capsule ? FVector(0, 0, Capsule->GetScaledCapsuleHalfHeight() - TargetRadius) : FVector::ZeroVector;
        FVector Strike, Body;
        FMath::SegmentDistToSegmentSafe(Start, End, Center - Vertical, Center + Vertical, Strike, Body);
        if (FVector::DistSquared(Strike, Body) <= FMath::Square(Radius + TargetRadius))
        {
            HitActors.Add(Target);
            const float Actual = HitTarget(*Target, Damage);
            if (Move == EMove::Leap && !Target->ActorHasTag(PracticeTag))
            {
                MoveTime = 0;
                if (!IsGuild()) SetValue(TEXT("Health"), FMath::Min(Value(TEXT("MaxHealth")), Value(TEXT("Health")) + Actual * 0.5f));
            }
        }
    }
}

void UFSClassCombat::Fire(FShot Shot)
{
    if (Shot.HalfWidth == 0)
    {
        const FVector Origin = Player().GetActorLocation();
        FCollisionQueryParams Query(SCENE_QUERY_STAT(FSClassEmission), false, &Player());
        FHitResult Hit;
        if (FVector::DistSquared(Shot.Position, Hand()) < 1.0
            && GetWorld()->LineTraceSingleByObjectType(Hit, Origin, Shot.Position, Walls(), Query))
        {
            PlaySound(TEXT("SFX_Hit"));
            return;
        }
    }
    Shot.Visual = Marker(*GetWorld(), Shot.Position, FVector(FMath::Max(0.12f, Shot.Radius / 50)));
    if (auto* Visual = Shot.Visual.Get(); Visual && Shot.HalfWidth > 0)
    {
        Visual->SetActorRotation(Shot.Direction.Rotation());
        Visual->SetActorScale3D(FVector(0.45, Shot.HalfWidth / 50, 0.28));
    }
    Shots.Add(MoveTemp(Shot));
    PlaySound(TEXT("SFX_Wave"));
}

void UFSClassCombat::TickShots(float DeltaTime)
{
    for (int32 Index = Shots.Num() - 1; Index >= 0; --Index)
    {
        FShot& Shot = Shots[Index];
        float Remaining = FMath::Min(Shot.Range, Shot.Speed * DeltaTime);
        bool bStop = false;
        while (Remaining > KINDA_SMALL_NUMBER && !bStop)
        {
            const float Step = FMath::Min(20.0f, Remaining);
            const FVector MidHeading = Shot.Direction.RotateAngleAxis(Shot.TurnDegrees * Step * 0.5f, FVector::UpVector);
            const FVector Desired = Shot.Position + MidHeading * Step;
            FCollisionQueryParams Query(SCENE_QUERY_STAT(FSClassShot), false, &Player());
            FHitResult Wall;
            bStop = GetWorld()->SweepSingleByObjectType(Wall, Shot.Position, Desired, FQuat::Identity, Walls(),
                FCollisionShape::MakeSphere(FMath::Max(1.0f, Shot.Radius)), Query);
            const FVector End = bStop ? Wall.Location : Desired;
            TArray<TPair<double, AActor*>> Contacts;
            for (TActorIterator<AActor> It(GetWorld()); It && Shot.Hit.Num() < Shot.TargetLimit; ++It)
            {
                AActor* Target = *It;
                if (Shot.Hit.Contains(Target) || !(IsEnemy(*Target) || Target->ActorHasTag(PracticeTag))) continue;
                const auto* Capsule = Target->FindComponentByClass<UCapsuleComponent>();
                const FVector Center = Target->GetActorLocation();
                const double TargetRadius = Capsule ? Capsule->GetScaledCapsuleRadius() : 30;
                const FVector Height = Capsule ? FVector(0, 0, Capsule->GetScaledCapsuleHalfHeight() - TargetRadius) : FVector::ZeroVector;
                const FVector Relative = Center - End;
                FVector Along, Body;
                FMath::SegmentDistToSegmentSafe(Shot.Position, End, Center - Height, Center + Height, Along, Body);
                const bool bBox = Shot.HalfWidth > 0;
                const bool bOverlap = bBox
                    ? FMath::Abs(FVector::DotProduct(Relative, FVector::CrossProduct(FVector::UpVector, Shot.Direction))) <= Shot.HalfWidth + TargetRadius
                      && FMath::Abs(FVector::DotProduct(Relative, Shot.Direction)) <= 22.5 + TargetRadius
                      && Relative.Z >= -TargetRadius && Relative.Z <= Shot.HalfHeight + Height.Z + TargetRadius
                    : FVector::DistSquared(Along, Body) <= FMath::Square(Shot.Radius + TargetRadius);
                if (!bOverlap) continue;
                Query.AddIgnoredActor(Target);
                if (!GetWorld()->LineTraceTestByObjectType(End, Center, Walls(), Query))
                {
                    Contacts.Emplace(FVector::DistSquared(Shot.Position, Along), Target);
                }
            }
            Contacts.Sort([](const auto& A, const auto& B) { return A.Key < B.Key; });
            for (const auto& Contact : Contacts)
            {
                Shot.Hit.Add(Contact.Value);
                HitTarget(*Contact.Value, Shot.Damage);
                if (Shot.Hit.Num() >= Shot.TargetLimit)
                {
                    if (Shot.TargetLimit == 1) bStop = true;
                    break;
                }
            }
            Shot.Position = End;
            Shot.Direction = Shot.Direction.RotateAngleAxis(Shot.TurnDegrees * Step, FVector::UpVector);
            Shot.Range -= Step;
            Remaining -= Step;
        }
        if (AActor* Visual = Shot.Visual.Get()) Visual->SetActorLocation(Shot.Position);
        if (bStop || Shot.Range <= KINDA_SMALL_NUMBER)
        {
            if (Shot.bThread)
            {
                Anchor = Shot.Position;
                AnchorUntil = GetWorld()->GetTimeSeconds() + 2.5;
                AnchorVisual = Marker(*GetWorld(), Anchor, FVector(0.28));
            }
            if (AActor* Visual = Shot.Visual.Get()) Visual->Destroy();
            Shots.RemoveAt(Index);
        }
    }
}

void UFSClassCombat::TickAction(float DeltaTime)
{
    PreviousAge = Age;
    Age = FMath::Min(EndTime, Age + DeltaTime);
    const double Now = GetWorld()->GetTimeSeconds();
    if (MoveTime > 0)
    {
        const float From = MoveAge / MoveTime;
        MoveAge = FMath::Min(MoveAge + DeltaTime, MoveTime);
        const float To = MoveAge / MoveTime;
        const bool bCurve = Move == EMove::Pivot;
        const FVector Direction = bCurve ? FMath::Lerp(Heading, TravelHeading, (From + To) * 0.5f).GetSafeNormal2D() : TravelHeading;
        const float ArcDelta = 4 * MoveArc * (To * (1 - To) - From * (1 - From));
        if (bCurve) Player().SetActorRotation(Direction.Rotation());
        const bool bClear = MovePlayer(Direction * MoveDistance * (To - From) + FVector(0, 0, ArcDelta), Move != EMove::Dodge);
        if (!bClear || To >= 1)
        {
            if (!bClear && Move == EMove::Leap && Age < 0.25f) bCommitted = true;
            MoveTime = 0;
        }
    }

    switch (Move)
    {
    case EMove::Idle:
        if (BufferedBasicUntil > Now && Ready(Basic))
        {
            BufferedBasicUntil = 0;
            Attack();
        }
        return;
    case EMove::Axe: BladeContact(0.25f, 0.42f, 22, 34, 3); break;
    case EMove::Sword: BladeContact(0.14f, 0.24f, 16, 22, 2); break;
    case EMove::Dodge:
        if (Branch() == 'V' && Age >= 0.24f) SetFlag(TEXT("bDodging"), false);
        break;
    case EMove::Leap:
        StrikePoint = Player().GetActorLocation() + Heading * 50;
        if (!bCommitted) BladeContact(0.25f, 0.43f, Player().GetCapsuleComponent()->GetScaledCapsuleRadius(), 40.8f, 1);
        if (Age >= EndTime)
        {
            PlaySound(TEXT("SFX_Hit"));
        }
        break;
    case EMove::ShieldHold:
    case EMove::StoneHold:
        Player().SetActorRotation(AimDirection().Rotation());
        if (!bHadTurnCue && Age >= (Move == EMove::ShieldHold ? 0.30f : 0.50f))
        {
            bHadTurnCue = true;
            PlaySound(TEXT("SFX_Hit"));
        }
        break;
    case EMove::ShieldStep:
        StrikePoint = Player().GetActorLocation() + Heading * 60;
        BladeContact(0.06f, EndTime - 0.12f, 34, 12, 1);
        break;
    case EMove::Spark:
        if (!bCommitted && Age >= 0.10f) { bCommitted = true; Fire({Hand(), Heading, 9, 1600, 550, 12}); }
        break;
    case EMove::OrbHold:
        if (!bHadTurnCue && Age >= 0.45f) { bHadTurnCue = true; PlaySound(TEXT("SFX_Hit")); }
        break;
    case EMove::Orb:
        if (!bCommitted && Age >= 0.12f)
        {
            bCommitted = true;
            if (Spend(0, 0, 20))
            {
                Cooldown[First] = Now + 1.20;
                Fire(bFull ? FShot{Hand(), Heading, 14, 1800, 1100, 32} : FShot{Hand(), Heading, 32, 900, 600, 24});
                if (WardUntil > Now && FVector::Dist2D(Player().GetActorLocation(), WardCenter) <= 175) PlaySound(TEXT("SFX_Hit"));
            }
        }
        break;
    case EMove::Stone:
        if (!bCommitted && Age >= 0.18f)
        {
            bCommitted = true;
            const FVector Feet = Player().GetActorLocation() - FVector(0, 0, Player().GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
            FShot Pulse{Feet + FVector(0, 0, 18), Heading, 10, 1600, bFull ? 520.0f : 320.0f, bFull ? 50.0f : 34.0f};
            Pulse.HalfWidth = bFull ? 110 : 85;
            Pulse.HalfHeight = 75;
            Pulse.TargetLimit = 4;
            Fire(MoveTemp(Pulse));
        }
        break;
    case EMove::Pivot: BladeContact(0.10f, 0.30f, 24, 24, 2); break;
    case EMove::Thread:
        if (!bCommitted && Age >= 0.14f)
        {
            bCommitted = true;
            const FCollisionQueryParams Query(SCENE_QUERY_STAT(FSThreadOrigin), false, &Player());
            if (GetWorld()->LineTraceTestByObjectType(Player().GetActorLocation(), Hand(), Walls(), Query))
            {
                Message(TEXT("Path blocked"));
                break;
            }
            if (Spend(12, 0, 18))
            {
                Cooldown[Second] = Now + 3.5;
                FShot Thread{Hand(), Heading, 20, 1800, 540, 16};
                Thread.TargetLimit = 3;
                Thread.bThread = true;
                Fire(MoveTemp(Thread));
            }
        }
        break;
    case EMove::Recall:
        if (!bCommitted && Age >= 0.08f)
        {
            bCommitted = true;
            if (AnchorUntil > Now)
            {
                const FVector Delta = Player().GetActorLocation() - Anchor;
                FShot Thread{Anchor, Delta.GetSafeNormal2D(), 26, 2200, FMath::Min(750.0, Delta.Size2D()), 28};
                Thread.TargetLimit = 3;
                Fire(MoveTemp(Thread));
                AnchorUntil = 0;
                if (AActor* Visual = AnchorVisual.Get()) Visual->Destroy();
                AnchorVisual.Reset();
            }
        }
        break;
    case EMove::PaidStand:
        if (!bCommitted && Age >= 0.32f) { bCommitted = true; Travel(Heading, 145, 0.20f); }
        BladeContact(0.38f, 0.52f, 18, 36, 1);
        break;
    case EMove::Slip: BladeContact(0.10f, 0.20f, 17, 16, 2); break;
    case EMove::Wave:
        if (!bCommitted && Age >= 0.20f)
        {
            bCommitted = true;
            if (Spend(0, 0, 30))
            {
                Cooldown[Second] = Now + 1.10;
                UFSCharacterActions::CastDirectedArcaneWave(&Player(), LoadClass<AActor>(nullptr, TEXT("/Game/Combat/BP_MeleeEnemy.BP_MeleeEnemy_C")),
                    LoadClass<AActor>(nullptr, TEXT("/Game/Combat/FX/BP_FX_Wave.BP_FX_Wave_C")),
                    LoadClass<AActor>(nullptr, TEXT("/Game/Combat/FX/BP_FX_Hit.BP_FX_Hit_C")), 40, Heading);
                PlaySound(TEXT("SFX_Wave"));
            }
        }
        break;
    case EMove::Seal:
        if (!bCommitted && Age >= 0.18f)
        {
            bCommitted = true;
            const FCollisionQueryParams Query(SCENE_QUERY_STAT(FSWardCommit), false, &Player());
            if (!GetWorld()->LineTraceTestByObjectType(Player().GetActorLocation(), WardCenter + FVector(0, 0, 5), Walls(), Query)
                && Spend(0, 0, 18))
            {
                Cooldown[Second] = Now + 4.5;
                SetWard(WardCenter);
            }
        }
        break;
    case EMove::BendHold:
    {
        const float Axis = FVector::DotProduct(Player().GetCharacterMovement()->GetCurrentAcceleration(),
            FVector::CrossProduct(FVector::UpVector, AimDirection()))
            / FMath::Max(1.0f, Player().GetCharacterMovement()->GetMaxAcceleration());
        if (FMath::Abs(Axis) > 0.35f)
        {
            const int32 Side = Axis > 0 ? 1 : -1;
            if (Side != CurveSide) PlaySound(TEXT("SFX_Hit"));
            CurveSide = Side;
        }
        break;
    }
    case EMove::Bend:
        if (!bCommitted && Age >= 0.14f)
        {
            bCommitted = true;
            if (Spend(0, 0, 22))
            {
                Cooldown[Second] = Now + 1.40;
                FShot Shot{Hand(), Heading, 18, 1450, 900, 28};
                Shot.TurnDegrees = CurveSide * 70.0f / 900.0f;
                Fire(MoveTemp(Shot));
            }
        }
        break;
    }
    if (Age >= EndTime && !(Move == EMove::Orb && MoveTime > 0))
    {
        if (Move == EMove::OrbHold || Move == EMove::ShieldHold) ReleaseSkillOne();
        else if (Move == EMove::StoneHold || Move == EMove::BendHold) ReleaseSkillTwo();
        else Finish();
    }
}

void UFSClassCombat::ClearEffects()
{
    for (const FShot& Shot : Shots)
    {
        if (AActor* Visual = Shot.Visual.Get()) Visual->Destroy();
    }
    Shots.Reset();
    if (AActor* Visual = AnchorVisual.Get()) Visual->Destroy();
    if (AActor* Visual = WardVisual.Get()) Visual->Destroy();
    AnchorVisual.Reset();
    WardVisual.Reset();
    AnchorUntil = WardUntil = 0;
}

void UFSClassCombat::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
    Super::TickComponent(DeltaTime, TickType, TickFunction);
    if (auto* Controller = Cast<APlayerController>(Player().GetController()); Controller && Controller->IsLocalController() && Controller != InputController)
    {
        SetupInput(*Controller);
    }
    if (!Class || Unavailable())
    {
        if (Move != EMove::Idle) Cancel();
        ClearEffects();
        bSelecting = bAim = false;
        RequestedInputs = 0;
        UpdateCamera(DeltaTime);
        return;
    }

    const double Now = GetWorld()->GetTimeSeconds();
    if (AnchorUntil && Now >= AnchorUntil)
    {
        if (AActor* Visual = AnchorVisual.Get()) Visual->Destroy();
        AnchorVisual.Reset();
        AnchorUntil = 0;
    }
    if (WardUntil && Now >= WardUntil)
    {
        if (AActor* Visual = WardVisual.Get()) Visual->Destroy();
        WardVisual.Reset();
        WardUntil = 0;
    }
    if (AnchorUntil > Now)
    {
        if (AActor* Visual = AnchorVisual.Get()) Visual->SetActorScale3D(FVector(0.20 + 0.08 * FMath::Sin(Now * (AnchorUntil - Now < 0.65 ? 20 : 8))));
    }
    if (bSelecting && (!IsValid(Altar) || FVector::Dist2D(Altar->GetActorLocation(), Player().GetActorLocation()) > 250))
    {
        CloseSelection();
    }
    if (Class->Stamina > 0 && Now >= LastStaminaSpend + 0.60)
    {
        SetValue(TEXT("Stamina"), FMath::Min(Class->Stamina, Value(TEXT("Stamina")) + 25 * DeltaTime));
    }
    if (Class->Mana > 0)
    {
        SetValue(TEXT("Mana"), FMath::Min(Class->Mana, Value(TEXT("Mana")) + 8 * DeltaTime));
    }
    TickAction(DeltaTime);
    ConsumeInput();
    TickShots(DeltaTime);
    UpdateCamera(DeltaTime);
}

float UFSClassCombat::GuardDamage(float Damage, AActor* Causer)
{
    const auto* Enemy = Cast<AFSCryptEnemy>(Causer);
    const bool bBlockable = (Enemy && Enemy->IsCurrentAttackBlockable())
        || (Causer && Causer->GetClass()->GetName() == TEXT("BP_MeleeEnemy_C"));
    if (Damage <= 0 || !bBlockable || Unavailable() || Flag(Player(), TEXT("bDodging"))) return Damage;
    const FVector Offset = (Causer->GetActorLocation() - Player().GetActorLocation()).GetSafeNormal2D();
    const float Dot = FVector::DotProduct(Player().GetActorForwardVector(), Offset);
    if (Dot >= 0.642788f)
    {
        if (Move == EMove::ShieldHold) Damage *= 0.40f;
        else if (Move == EMove::StoneHold && Age >= 0.12f) Damage *= 0.55f;
        else if (Move == EMove::PaidStand && Age < 0.32f && Dot >= 0.707107f) Damage *= 0.30f;
    }
    const FVector Feet = Player().GetActorLocation() - FVector(0, 0, Player().GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    if (WardUntil > GetWorld()->GetTimeSeconds() && FVector::Dist2D(Feet, WardCenter) <= 175 && FMath::Abs(Feet.Z - WardCenter.Z) <= 35)
    {
        const float Absorbed = FMath::Min(Damage * 0.35f, WardCapacity);
        WardCapacity -= Absorbed;
        Damage -= Absorbed;
        PlaySound(TEXT("SFX_Hit"));
        if (WardCapacity <= KINDA_SMALL_NUMBER)
        {
            WardUntil = 0;
            if (AActor* Visual = WardVisual.Get()) Visual->Destroy();
            WardVisual.Reset();
        }
    }
    return Damage;
}

void UFSClassCombat::SetWard(const FVector& Ground)
{
    if (AActor* Visual = WardVisual.Get()) Visual->Destroy();
    WardVisual = Marker(*GetWorld(), Ground + FVector(0, 0, 3), FVector(3.5, 3.5, 0.025), true);
    WardCenter = Ground;
    WardUntil = GetWorld()->GetTimeSeconds() + 3.2;
    WardCapacity = 42;
}

bool UFSClassCombat::WardGround(FVector& Ground) const
{
    const FVector Origin = Player().GetActorLocation();
    const float Feet = Origin.Z - Player().GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    FVector Horizontal = Origin;
    if (bAim && Player().GetController())
    {
        FVector Eye;
        FRotator View;
        Player().GetController()->GetPlayerViewPoint(Eye, View);
        const FCollisionQueryParams Query(SCENE_QUERY_STAT(FSWardAim), false, &Player());
        FHitResult Aim;
        const FVector End = Eye + View.Vector() * 2500;
        const FVector Point = GetWorld()->LineTraceSingleByObjectType(Aim, Eye, End, Walls(), Query) ? Aim.ImpactPoint : End;
        Horizontal += (Point - Origin).GetClampedToMaxSize2D(400);
    }
    FHitResult Hit;
    const FCollisionQueryParams Query(SCENE_QUERY_STAT(FSWardGround), false, &Player());
    const bool bGround = GetWorld()->LineTraceSingleByObjectType(Hit, FVector(Horizontal.X, Horizontal.Y, Feet + 102),
        FVector(Horizontal.X, Horizontal.Y, Feet - 102), Walls(), Query);
    Ground = Hit.ImpactPoint;
    return bGround && Hit.ImpactNormal.Z >= Player().GetCharacterMovement()->GetWalkableFloorZ()
        && !GetWorld()->LineTraceTestByObjectType(Origin, Ground + FVector(0, 0, 5), Walls(), Query);
}

void UFSClassCombat::BeginAim()
{
    if (!bSelecting && !Unavailable()) bAim = true;
}

void UFSClassCombat::EndAim() { bAim = false; }

void UFSClassCombat::UpdateCamera(float DeltaTime)
{
    if (auto* Arm = Player().FindComponentByClass<USpringArmComponent>())
    {
        Arm->TargetArmLength = FMath::FInterpTo(Arm->TargetArmLength, bAim ? OriginalArm * 0.72f : OriginalArm, DeltaTime, 14);
    }
    float Scale = bAim ? 0.70f : 1.0f;
    if (Move == EMove::ShieldHold || Move == EMove::BendHold) Scale = FMath::Min(Scale, 0.55f);
    if (Move == EMove::OrbHold) Scale = FMath::Min(Scale, 0.60f);
    if (MoveTime > 0 || Move == EMove::StoneHold || Move == EMove::PaidStand || bSelecting) Scale = 0;
    Player().GetCharacterMovement()->MaxWalkSpeed = OriginalWalkSpeed * Scale;
}

void UFSClassCombat::SpawnAltar()
{
    if (!IsGuild()) return;
    // The current Guild map has reception and a contract board. Place the altar in its open west aisle.
    Altar = Marker(*GetWorld(), FVector(-220, -40, 15), FVector(0.9, 0.9, 0.35), true);
    if (!Altar) return;
    Altar->Tags.Add(TEXT("EssenceAltar"));
    auto* Label = NewObject<UTextRenderComponent>(Altar);
    Label->SetupAttachment(Altar->GetRootComponent());
    Label->SetAbsolute(false, false, true);
    Label->SetRelativeLocation(FVector(0, 0, 190));
    Label->SetWorldSize(20);
    Label->SetHorizontalAlignment(EHTA_Center);
    Label->SetText(FText::FromString(TEXT("Essence Altar")));
    Label->RegisterComponent();
}

void UFSClassCombat::EquipClassVisual()
{
    auto Equip = [this](TObjectPtr<UStaticMeshComponent>& Mesh, const TCHAR* Path, FName Socket)
    {
        if (!Mesh)
        {
            Mesh = NewObject<UStaticMeshComponent>(&Player());
            Player().AddInstanceComponent(Mesh);
            Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Mesh->SetupAttachment(Player().GetMesh(), Socket);
            Mesh->RegisterComponent();
        }
        Mesh->SetStaticMesh(Path ? LoadObject<UStaticMesh>(nullptr, Path) : nullptr);
    };
    const TCHAR* Weapon = Branch() == 'V' ? TEXT("/Game/Fantasy_Pack/Characters/Viking_Ulf/Mesh/SM_Viking_Axe.SM_Viking_Axe")
        : Branch() == 'S' ? TEXT("/Game/Fantasy_Pack/Characters/Warrior/Mesh/SM_Sword.SM_Sword") : nullptr;
    Equip(ClassWeapon, Weapon, TEXT("hand_r"));
    Equip(ClassShield, Branch() == 'S' ? TEXT("/Engine/BasicShapes/Cylinder.Cylinder") : nullptr, TEXT("lowerarm_l"));
    ClassShield->SetRelativeScale3D(FVector(0.52, 0.52, 0.065));
    ClassShield->SetRelativeRotation(FRotator(90, 0, 0));
}

bool UFSClassCombat::InteractAtAltar()
{
    if (Unavailable() || !IsValid(Altar) || FVector::Dist2D(Altar->GetActorLocation(), Player().GetActorLocation()) > 175) return false;
    if (bSelecting)
    {
        SelectEssence(TEXT("VSM")[SelectionIndex]);
    }
    else if (FSEssence::CanChoose(Player()) && Move == EMove::Idle)
    {
        bSelecting = true;
        bAim = false;
        SelectionIndex = 0;
    }
    else
    {
        Message(TEXT("No essence choice is available."));
    }
    return true;
}

void UFSClassCombat::SelectEssence(TCHAR Essence)
{
    if (!bSelecting || Unavailable()) return;
    const bool bEvolution = Branch() != 0;
    FString Text;
    if (FSEssence::TryChoose(Player(), Essence, Text))
    {
        CloseSelection();
        ClearEffects();
        Restore(bEvolution);
        SetString(Player(), TEXT("BannerText"), Text);
        SetValue(TEXT("BannerUntil"), GetWorld()->GetTimeSeconds() + 3.0);
    }
    Message(Text);
}

void UFSClassCombat::PreviousOption() { if (bSelecting) SelectionIndex = (SelectionIndex + 2) % 3; }
void UFSClassCombat::NextOption() { if (bSelecting) SelectionIndex = (SelectionIndex + 1) % 3; }
void UFSClassCombat::CloseSelection() { bSelecting = false; }
void UFSClassCombat::Interaction() { CallEvent(Player(), TEXT("DoInteract")); }

void UFSClassCombat::QueueAttack() { RequestedInputs |= 1 << 0; }
void UFSClassCombat::QueueEvade() { RequestedInputs |= 1 << 1; }
void UFSClassCombat::QueueSkillOne() { RequestedInputs |= 1 << 2; }
void UFSClassCombat::QueueSkillTwo() { RequestedInputs |= 1 << 3; }
void UFSClassCombat::QueueFirstRelease() { RequestedInputs |= 1 << 4; }
void UFSClassCombat::QueueSecondRelease() { RequestedInputs |= 1 << 5; }

void UFSClassCombat::ConsumeInput()
{
    const uint8 Actions = RequestedInputs;
    RequestedInputs = 0;
    const EMove Before = Move;
    if (Actions & (1 << 1)) Evade();
    if (Move == Before && (Actions & (1 << 3))) SkillTwo();
    if (Move == Before && (Actions & (1 << 2))) SkillOne();
    if (Actions & (1 << 4)) ReleaseSkillOne();
    if (Actions & (1 << 5)) ReleaseSkillTwo();
    if (Move == Before && (Actions & (1 << 0))) Attack();
}

void UFSClassCombat::SetupInput(APlayerController& Controller)
{
    if (auto* Previous = InputController.Get(); Previous && Input) Previous->PopInputComponent(Input);
    Input = NewObject<UInputComponent>(&Player());
    Input->RegisterComponent();
    Input->Priority = 20;
    const auto Bind = [this](FKey Key, EInputEvent Event, void (UFSClassCombat::*Action)(), bool bConsume = true)
    {
        Input->BindKey(Key, Event, this, Action).bConsumeInput = bConsume;
    };
    for (FKey Key : {EKeys::LeftMouseButton, EKeys::Gamepad_RightTrigger}) Bind(Key, IE_Pressed, &UFSClassCombat::QueueAttack);
    for (FKey Key : {EKeys::LeftShift, EKeys::Gamepad_FaceButton_Right}) Bind(Key, IE_Pressed, &UFSClassCombat::QueueEvade);
    for (FKey Key : {EKeys::F, EKeys::Gamepad_LeftShoulder})
    {
        Bind(Key, IE_Pressed, &UFSClassCombat::QueueSkillOne);
        Bind(Key, IE_Released, &UFSClassCombat::QueueFirstRelease);
    }
    for (FKey Key : {EKeys::Q, EKeys::Gamepad_RightShoulder})
    {
        Bind(Key, IE_Pressed, &UFSClassCombat::QueueSkillTwo);
        Bind(Key, IE_Released, &UFSClassCombat::QueueSecondRelease);
    }
    for (FKey Key : {EKeys::RightMouseButton, EKeys::Gamepad_LeftTrigger})
    {
        Bind(Key, IE_Pressed, &UFSClassCombat::BeginAim);
        Bind(Key, IE_Released, &UFSClassCombat::EndAim);
    }
    for (FKey Key : {EKeys::E, EKeys::Gamepad_FaceButton_Left}) Bind(Key, IE_Pressed, &UFSClassCombat::Interaction);
    for (FKey Key : {EKeys::Left, EKeys::Gamepad_DPad_Left}) Bind(Key, IE_Pressed, &UFSClassCombat::PreviousOption, false);
    for (FKey Key : {EKeys::Right, EKeys::Gamepad_DPad_Right}) Bind(Key, IE_Pressed, &UFSClassCombat::NextOption, false);
    Bind(EKeys::BackSpace, IE_Pressed, &UFSClassCombat::CloseSelection, false);
    Bind(EKeys::Gamepad_Special_Left, IE_Pressed, &UFSClassCombat::CloseSelection, false);
    Controller.PushInputComponent(Input);
    InputController = &Controller;
}

void UFSClassCombat::Draw(AHUD& HUD) const
{
    APlayerController* Controller = HUD.GetOwningPlayerController();
    if (!Class || !Controller) return;
    int32 Width, Height;
    Controller->GetViewportSize(Width, Height);
    float Y = 30.0f;
    if (bAim && !Unavailable())
    {
        const float X = Width * 0.5f, CenterY = Height * 0.5f;
        HUD.DrawLine(X - 7, CenterY, X + 7, CenterY, FLinearColor::White);
        HUD.DrawLine(X, CenterY - 7, X, CenterY + 7, FLinearColor::White);
    }
    const auto Resource = [&](const TCHAR* Label, const TCHAR* Field, float Max, FLinearColor Color)
    {
        if (Max <= 0) return;
        const float Current = Value(Field);
        HUD.DrawRect(FLinearColor(0.02, 0.02, 0.02, 0.8), 26, Y, 306, 21);
        HUD.DrawRect(Color, 26, Y, 306 * FMath::Clamp(Current / Max, 0.0f, 1.0f), 21);
        HUD.DrawText(FString::Printf(TEXT("%s   %.1f / %.1f"), Label, Current, Max), FLinearColor::White, 32, Y + 1, nullptr, 1.0f);
        Y += 24;
    };
    Resource(TEXT("V"), TEXT("Health"), Class->Vitality, FLinearColor(0.78f, 0.08f, 0.10f));
    Resource(TEXT("S"), TEXT("Stamina"), Class->Stamina, FLinearColor(0.09f, 0.50f, 0.18f));
    Resource(TEXT("M"), TEXT("Mana"), Class->Mana, FLinearColor(0.10f, 0.31f, 0.80f));
    const double Now = GetWorld()->GetTimeSeconds();
    const TCHAR* SkillOneName = Branch() == 'V' ? TEXT("Fury Leap") : Branch() == 'S' ? TEXT("Shield Advance") : TEXT("Arcane Orb");
    const TCHAR* SkillTwoName = nullptr;
    switch (Class->Kit)
    {
    case EKit::Juggernaut: SkillTwoName = TEXT("Stonebreak"); break;
    case EKit::Berserk: SkillTwoName = TEXT("Rage Pivot"); break;
    case EKit::BloodMage: SkillTwoName = TEXT("Blood Thread"); break;
    case EKit::Mercenary: SkillTwoName = TEXT("Paid Stand"); break;
    case EKit::Swordsman: SkillTwoName = TEXT("Slip Cut"); break;
    case EKit::SoulsBlade: SkillTwoName = TEXT("Arcane Wave"); break;
    case EKit::Paladin: SkillTwoName = TEXT("Sanctuary Seal"); break;
    case EKit::SpellWeaver: SkillTwoName = TEXT("Weave Step"); break;
    case EKit::Mage: SkillTwoName = TEXT("Arcane Bend"); break;
    default: break;
    }
    if (Branch() && !Unavailable())
    {
        const auto DrawSkill = [&](const TCHAR* Key, const TCHAR* Name, int32 Slot, float X)
        {
            const float Remaining = FMath::Max(0.0, Cooldown[Slot] - Now);
            const FString Label = Remaining > 0
                ? FString::Printf(TEXT("%s  %s  %.1fs"), Key, Name, Remaining) : FString::Printf(TEXT("%s  %s"), Key, Name);
            HUD.DrawText(Label, FLinearColor::White, X, Height - 122.0f, nullptr);
        };
        DrawSkill(TEXT("F / LB"), SkillOneName, First, 28);
        if (SkillTwoName) DrawSkill(TEXT("Q / RB"), SkillTwoName, Second, 235);
    }
    if (WardUntil > Now) HUD.DrawText(FString::Printf(TEXT("Seal   %.1f"), WardCapacity), FLinearColor::White, 28, 105, nullptr);
    if (AnchorUntil > Now) HUD.DrawText(FString::Printf(TEXT("Q / RB  Recall   %.1fs"), AnchorUntil - Now), FLinearColor::White, 28, 105, nullptr);
    if (Move == EMove::OrbHold)
    {
        HUD.DrawText(Age >= 0.45f ? TEXT("Focused Orb") : TEXT("Broad Orb"), FLinearColor::White, 28, 105, nullptr);
    }
    if (Class->Kit == EKit::Paladin && bAim && Move == EMove::Idle && Ready(Second))
    {
        FVector Ground;
        const bool bClear = WardGround(Ground);
        if (bClear)
        {
            for (int32 Index = 0; Index < 24; ++Index)
            {
                const FVector A = HUD.Project(Ground + FVector(175, 0, 5).RotateAngleAxis(Index * 15, FVector::UpVector));
                const FVector B = HUD.Project(Ground + FVector(175, 0, 5).RotateAngleAxis((Index + 1) * 15, FVector::UpVector));
                if (A.Z > 0 && B.Z > 0) HUD.DrawLine(A.X, A.Y, B.X, B.Y, FLinearColor::White);
            }
        }
        else
        {
            HUD.DrawText(TEXT("Seal: blocked"), FLinearColor::White, Width * 0.5f + 16, Height * 0.5f + 16, nullptr);
        }
    }
    if (Move == EMove::BendHold)
    {
        FVector Position = Hand();
        FVector Direction = AimDirection(true);
        const float Turn = CurveSide * 70.0f / 900.0f;
        const FCollisionQueryParams Query(SCENE_QUERY_STAT(FSBendPreview), false, &Player());
        for (int32 Index = 0; Index < 45; ++Index)
        {
            const FVector End = Position + Direction.RotateAngleAxis(Turn * 10, FVector::UpVector) * 20;
            FHitResult Hit;
            const bool bBlocked = GetWorld()->SweepSingleByObjectType(Hit, Position, End, FQuat::Identity, Walls(), FCollisionShape::MakeSphere(18), Query);
            const FVector StartScreen = HUD.Project(Position);
            const FVector EndScreen = HUD.Project(bBlocked ? Hit.Location : End);
            if (StartScreen.Z > 0 && EndScreen.Z > 0) HUD.DrawLine(StartScreen.X, StartScreen.Y, EndScreen.X, EndScreen.Y, FLinearColor(0.5, 0.8, 1), 2);
            if (bBlocked) break;
            Position = End;
            Direction = Direction.RotateAngleAxis(Turn * 20, FVector::UpVector);
        }
    }
    if (!bSelecting) return;
    const float X = Width * 0.5f - 210, Top = Height * 0.5f - 102;
    HUD.DrawRect(FLinearColor(0.025f, 0.035f, 0.052f, 0.96f), X, Top, 420, 204);
    HUD.DrawText(TEXT("Choose an essence"), FLinearColor::White, X + 22, Top + 14, nullptr, 1.7f);
    for (int32 Index = 0; Index < 3; ++Index)
    {
        FString Path = Class->Path;
        if (!Path.IsEmpty()) Path += TEXT(", ");
        Path.AppendChar(TEXT("VSM")[Index]);
        bool bSupported;
        const auto& Target = FSEssence::Resolve(Path, bSupported);
        const FLinearColor Color = Index == SelectionIndex ? FLinearColor(1, 0.78f, 0.30f) : FLinearColor::White;
        HUD.DrawText(FString::Chr(TEXT("VSM")[Index]) + TEXT("  ") + Target.Name, Color, X + 27, Top + 52 + Index * 30, nullptr, 1.35f);
    }
    HUD.DrawText(TEXT("Arrows / D-pad: choose    Interact: confirm    Backspace / View: close"), FLinearColor(0.8f, 0.85f, 0.95f),
        X + 20, Top + 161, nullptr, 1.0);
}

void UFSClassBridge::RestoreClass(ACharacter* Character)
{
    if (auto* Component = Combat(Character, true)) Component->Restore(false);
}

void UFSClassBridge::ClassInput(ACharacter* Character, int32 Action, bool bPressed)
{
    auto* Component = Combat(Character);
    if (!Component) return;
    switch (Action)
    {
    case 0: if (bPressed) Component->Attack(); break;
    case 1: if (bPressed) Component->Evade(); break;
    case 2: if (bPressed) Component->SkillOne(); else Component->ReleaseSkillOne(); break;
    case 3: if (bPressed) Component->SkillTwo(); else Component->ReleaseSkillTwo(); break;
    case 4: if (bPressed) Component->NextOption(); break;
    case 5: if (bPressed) Component->PreviousOption(); break;
    case 6: if (bPressed) Component->CloseSelection(); break;
    default: break;
    }
}

bool UFSClassBridge::SetEditorEssenceFixture(ACharacter* Character, const FString& Path, int32 Level, int32 Pending, int32 XP)
{
#if WITH_EDITOR
    if (Character && Character->GetWorld()->WorldType == EWorldType::PIE)
    {
        if (auto* Profile = FSEssence::Profile(*Character))
        {
            SetString(*Profile, TEXT("EssenceHistory"), Path);
            SetString(*Profile, TEXT("ClassId"), TEXT("legacy"));
            Property<FIntProperty>(*Profile, TEXT("CharLevel"))->SetPropertyValue_InContainer(Profile, Level);
            Property<FIntProperty>(*Profile, TEXT("PendingEssenceChoices"))->SetPropertyValue_InContainer(Profile, Pending);
            Property<FIntProperty>(*Profile, TEXT("XP"))->SetPropertyValue_InContainer(Profile, XP);
            RestoreClass(Character);
            return true;
        }
    }
#endif
    return false;
}

void UFSClassBridge::ClassAttack(ACharacter* Character) { if (auto* Component = Combat(Character)) Component->Attack(); }
void UFSClassBridge::ClassEvade(ACharacter* Character) { if (auto* Component = Combat(Character)) Component->Evade(); }
void UFSClassBridge::ClassSkillTwo(ACharacter* Character) { if (auto* Component = Combat(Character)) Component->SkillTwo(); }

bool UFSClassBridge::UseEssenceAltar(ACharacter* Character)
{
    auto* Component = Combat(Character);
    return Component && Component->InteractAtAltar();
}

FString UFSClassBridge::GuildClassPrompt(ACharacter* Character)
{
    if (!Character) return {};
    const FString History = FSEssence::History(FSEssence::Profile(*Character));
    if (FSEssence::CanChoose(*Character))
    {
        return History.IsEmpty() ? TEXT("Choose your first essence at the Guild altar.") : TEXT("An essence choice is available at the Guild altar.");
    }
    return TEXT("Guild Hall. Take a contract from the board.");
}

float UFSClassBridge::FilterClassDamage(ACharacter* Character, float Damage, AActor* Causer)
{
    auto* Component = Combat(Character);
    return Component ? Component->GuardDamage(Damage, Causer) : Damage;
}

void UFSClassBridge::DrawClassHUD(ACharacter* Character, AHUD* HUD)
{
    if (auto* Component = Combat(Character); Component && HUD) Component->Draw(*HUD);
}
