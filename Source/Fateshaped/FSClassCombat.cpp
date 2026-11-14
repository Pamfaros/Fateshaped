#include "FSClassCombat.h"

#include "FSCharacterActions.h"
#include "FSCryptEnemy.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSequenceBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture2D.h"
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
#include "Misc/ConfigCacheIni.h"
#include "Sound/SoundBase.h"
#include "UObject/UnrealType.h"

namespace
{
constexpr int32 Basic = 0, Dodge = 1, First = 2, Second = 3;
constexpr float SharedDodgeDistance = 340, SharedDodgeTime = 0.90f;
constexpr float DefeatPlaybackTime = 2.0f;
const TCHAR* AxeClip = TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_ChargedAttack.MM_ChargedAttack");
const TCHAR* SwordClip = TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01.MM_Attack_01");
const TCHAR* FollowClip = TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_02.MM_Attack_02");
const TCHAR* CastClip = TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_03.MM_Attack_03");
const TCHAR* DashClip = TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_Dash.MM_Dash");
const TCHAR* RollClip = TEXT("/Game/Combat/SharedAnimations/AN_FS_EvadeRoll.AN_FS_EvadeRoll");
const TCHAR* LeapClip = TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_Jump.MM_Jump");
constexpr float LeapStrikeStart = 0.18f, LeapContactOpen = 0.25f, LeapContactClose = 0.43f, LeapDuration = 0.55f;
constexpr float WardRadius = 175, WardHeight = 35, WardMaxCapacity = 42;
constexpr float ThreadRecallRadius = 26;
const FVector FollowShoulder(0, 55, 20);
const FVector AimShoulder(0, 72, 27);
const FName PracticeTag(TEXT("PracticeTarget"));
const FName AltarTag(TEXT("EssenceAltar"));
const TCHAR* const SharedIconNames[] = {
    TEXT("Vitality"), TEXT("Stamina"), TEXT("Mana"), TEXT("HeavyAxeSwing"),
    TEXT("SwordCut"), TEXT("ArcaneSpark"), TEXT("Backstep"), TEXT("Evade")
};
const TCHAR* const EssencePreviews[] = {
    TEXT("Wield a heavy axe and spend Vitality to leap into battle."),
    TEXT("Raise your shield, then release to advance."),
    TEXT("Cast free sparks and charge a faster Arcane Orb."),
    TEXT("Brace and release a ground-shaking axe strike."),
    TEXT("Carry a heavy swing into a directional follow-up."),
    TEXT("Place a blood anchor and recall it through enemies."),
    TEXT("Plant your shield, spend Vitality and thrust."),
    TEXT("Change your angle with a sliding sword cut."),
    TEXT("Send a broad wave from your sword."),
    TEXT("Place a seal that softens direct attacks."),
    TEXT("Reposition while preparing Arcane Orb."),
    TEXT("Bend a projectile around cover.")
};

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

FString ReadString(const UObject& Owner, const TCHAR* Name)
{
    const auto* Field = Property<FStrProperty>(Owner, Name);
    return Field ? Field->GetPropertyValue_InContainer(&Owner) : FString();
}

int32 ReadInteger(const UObject* Owner, const TCHAR* Name)
{
    const auto* Field = Owner ? Property<FIntProperty>(*Owner, Name) : nullptr;
    return Field ? Field->GetPropertyValue_InContainer(Owner) : 0;
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

bool IsPracticeTarget(const AActor& Actor)
{
    return !Actor.IsHidden() && Actor.ActorHasTag(PracticeTag);
}

FVector BendDirection(const FVector& Aim)
{
    const FRotator Rotation = Aim.Rotation();
    return FRotator(FMath::Clamp(Rotation.Pitch, -35.0, 35.0), Rotation.Yaw, 0).Vector();
}

FVector ShotEndpoint(const FVector& Position, const FVector& Direction, float Turn, float Step)
{
    return Position + Direction.RotateAngleAxis(Turn * Step * 0.5f, FVector::UpVector) * Step;
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
    FootstepSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/Sounds/SC_Footstep.SC_Footstep"));
    for (const TCHAR* Name : SharedIconNames)
    {
        const FString Path = FString::Printf(TEXT("/Game/Icons/T_FS_%s.T_FS_%s"), Name, Name);
        SharedIcons.Add(LoadObject<UTexture2D>(nullptr, *Path));
    }
    auto* Mesh = Player().GetMesh();
    Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    AddTickPrerequisiteComponent(Mesh);
    OriginalWalkSpeed = Player().GetCharacterMovement()->MaxWalkSpeed;
    if (auto* Arm = Player().FindComponentByClass<USpringArmComponent>())
    {
        CameraArm = Arm;
        OriginalArm = Arm->TargetArmLength;
        OriginalSocketOffset = Arm->SocketOffset;
        // Drive the boom from view rotation only, and apply the aim blend before its collision probe.
        Arm->SetUsingAbsoluteRotation(true);
        Arm->bUsePawnControlRotation = true;
        Arm->SocketOffset += FollowShoulder;
        Arm->AddTickPrerequisiteComponent(this);
    }
    if (IsGuild())
    {
        TArray<AActor*> Altars;
        UGameplayStatics::GetAllActorsWithTag(this, AltarTag, Altars);
        if (!Altars.IsEmpty())
        {
            Altar = Altars[0];
            AltarLabel = Altar->FindComponentByClass<UTextRenderComponent>();
        }
    }
}

void UFSClassCombat::EndPlay(const EEndPlayReason::Type Reason)
{
    StopRumble();
    SetSelectionOpen(false);
    SetBasicAttackMovementLock(false);
    if (bPlayedDefeat && InputController.IsValid()) InputController->SetIgnoreMoveInput(false);
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

void UFSClassCombat::PlaySharedSound(const TCHAR* Name, bool bUI) const
{
    const FString Path = FString::Printf(TEXT("/Game/Combat/Audio/Shared/SFX_FS_%s.SFX_FS_%s"), Name, Name);
    if (auto* Sound = LoadObject<USoundBase>(nullptr, *Path))
    {
        if (bUI) UGameplayStatics::PlaySound2D(this, Sound);
        else UGameplayStatics::PlaySoundAtLocation(this, Sound, Player().GetActorLocation());
    }
}

void UFSClassCombat::UpdateFootsteps(float DeltaTime)
{
    const auto* Movement = Player().GetCharacterMovement();
    const bool bGrounded = Movement->IsMovingOnGround();
    if (bGrounded && !bWasGrounded && LastAirSpeed > 100)
    {
        PlaySharedSound(LastAirSpeed > 600 ? TEXT("HardLand") : TEXT("Land"));
        FootstepDistance = 0;
    }
    if (!bGrounded) LastAirSpeed = FMath::Max(0.0, -Movement->Velocity.Z);
    bWasGrounded = bGrounded;
    if (!bGrounded || Move != EMove::Idle || !FootstepSound) return;
    FootstepDistance += Movement->Velocity.Size2D() * DeltaTime;
    if (FootstepDistance >= 135.0f)
    {
        FootstepDistance = FMath::Fmod(FootstepDistance, 135.0f);
        constexpr float MinPitch = 0.94f;
        constexpr float MaxPitch = 1.06f;
        constexpr float MinPitchChange = 0.025f;
        float Pitch = FMath::FRandRange(MinPitch, MaxPitch);
        if (FMath::Abs(Pitch - LastFootstepPitch) < MinPitchChange)
        {
            Pitch = LastFootstepPitch < 1.0f ? LastFootstepPitch + MinPitchChange : LastFootstepPitch - MinPitchChange;
        }
        LastFootstepPitch = Pitch;
        UGameplayStatics::PlaySoundAtLocation(this, FootstepSound, Player().GetActorLocation(), FRotator::ZeroRotator,
            1.0f, Pitch);
    }
}

void UFSClassCombat::UpdateDefeat(float DeltaTime)
{
    auto* Mesh = Player().GetMesh();
    auto* Movement = Player().GetCharacterMovement();
    if (!Flag(Player(), TEXT("bDead")))
    {
        if (bPlayedDefeat)
        {
            if (InputController.IsValid()) InputController->SetIgnoreMoveInput(false);
            Mesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
            Movement->SetDefaultMovementMode();
            bPlayedDefeat = false;
        }
        return;
    }
    if (!bPlayedDefeat)
    {
        bPlayedDefeat = true;
        DefeatAge = DefeatDuration = 0;
        StopRumble();
        Player().StopJumping();
        Player().ConsumeMovementInputVector();
        Movement->StopMovementImmediately();
        Movement->DisableMovement();
        if (InputController.IsValid()) InputController->SetIgnoreMoveInput(true);
        if (auto* Clip = LoadObject<UAnimSequence>(nullptr,
            TEXT("/Game/Combat/SharedAnimations/AN_FS_Anim_Warrior_Dead_1.AN_FS_Anim_Warrior_Dead_1")))
        {
            DefeatDuration = Clip->GetPlayLength();
            Mesh->PlayAnimation(Clip, false);
            Mesh->SetPlayRate(DefeatDuration / DefeatPlaybackTime);
        }
    }
    const float End = DefeatAge + DeltaTime;
    if (DefeatDuration > 0 && DefeatAge < DefeatPlaybackTime && End >= DefeatPlaybackTime)
    {
        Mesh->SetPosition(DefeatDuration, false);
        Mesh->Stop();
    }
    DefeatAge = End;
}

void UFSClassCombat::PlayReaction(const TCHAR* Name, float Duration)
{
    const FString Path = FString::Printf(TEXT("/Game/Combat/SharedAnimations/AN_FS_Anim_Warrior_%s.AN_FS_Anim_Warrior_%s"), Name, Name);
    if (auto* Clip = LoadObject<UAnimSequenceBase>(nullptr, *Path))
    {
        UFSCharacterActions::PlayActionAnimation(&Player(), Clip, Clip->GetPlayLength() / Duration);
    }
}

void UFSClassCombat::Rumble(float Intensity, float Duration)
{
    auto* Controller = InputController.Get();
    if (!Controller) return;
    float Scale = 1.0f;
    GConfig->GetFloat(TEXT("Fateshaped.Input"), TEXT("VibrationIntensity"), Scale, GGameUserSettingsIni);
    StopRumble();
    if (Scale > 0)
    {
        RumbleHandle = Controller->PlayDynamicForceFeedback(Intensity * FMath::Clamp(Scale, 0.0f, 1.0f), Duration,
            true, true, true, true, EDynamicForceFeedbackAction::Start, 0);
    }
}

void UFSClassCombat::StopRumble()
{
    if (auto* Controller = InputController.Get(); Controller && RumbleHandle)
    {
        Controller->PlayDynamicForceFeedback(0, 0, true, true, true, true, EDynamicForceFeedbackAction::Stop, RumbleHandle);
    }
    RumbleHandle = 0;
}

void UFSClassCombat::Start(EMove Next, float End, const TCHAR* Clip)
{
    Message(FString());
    Move = Next;
    SetBasicAttackMovementLock(IsBasicAttacking());
    Age = 0;
    EndTime = End;
    bCommitted = false;
    bHadTurnCue = false;
    HitActors.Reset();
    StrikePoint = FVector::ZeroVector;
    bHaveBladeSample = false;
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
    if (Move == EMove::Leap)
    {
        auto* Movement = Player().GetCharacterMovement();
        Movement->MaxFlySpeed = PreLeapFlySpeed;
        if (Movement->MovementMode == MOVE_Flying) Movement->SetMovementMode(MOVE_Falling);
    }
    Move = EMove::Idle;
    SetBasicAttackMovementLock(false);
    DodgeSequence = nullptr;
    MoveTime = 0;
    MoveAge = 0;
    SetFlag(TEXT("bAttacking"), false);
    SetFlag(TEXT("bDodging"), false);
    LeapContactTarget.Reset();
    UFSCharacterActions::EndAttackFacing(&Player());
}

void UFSClassCombat::Cancel()
{
    StopRumble();
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

void UFSClassCombat::WeaponEdge(FVector& Base, FVector& Tip) const
{
    if (ClassWeapon && ClassWeapon->GetStaticMesh())
    {
        const FTransform& Transform = ClassWeapon->GetComponentTransform();
        // Weapon meshes have their long axis along local Z. The axe head spans local Y.
        if (Branch() == 'V')
        {
            Base = Transform.TransformPosition(FVector(0, -14, 67));
            Tip = Transform.TransformPosition(FVector(0, 14, 67));
        }
        else
        {
            Tip = Transform.TransformPosition(FVector(0, 0, 145));
            Base = Move == EMove::PaidStand ? Tip : Transform.TransformPosition(FVector(0, 0, 42));
        }
        return;
    }
    Base = Hand();
    Tip = Base + Player().GetActorForwardVector() * 60;
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

bool UFSClassCombat::HasTravelFloor(const FVector& Target) const
{
    FCollisionQueryParams Query(SCENE_QUERY_STAT(FSClassMove), false, &Player());
    FHitResult Floor;
    return GetWorld()->LineTraceSingleByObjectType(Floor, Target, Target - FVector(0, 0, 220), Walls(), Query)
        && Floor.ImpactNormal.Z >= Player().GetCharacterMovement()->GetWalkableFloorZ()
        && Target.Z - Floor.ImpactPoint.Z <= Player().GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + (Move == EMove::Leap ? 75 : 48);
}

bool UFSClassCombat::MovePlayer(const FVector& Offset, FHitResult& Hit)
{
    if (Move != EMove::Dodge && !HasTravelFloor(Player().GetActorLocation() + Offset)) return false;
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
        || ((Move == EMove::OrbHold || (Move == EMove::Orb && !bCommitted)) && !bStepped)
        || Move == EMove::StoneHold || (Move == EMove::Thread && !bCommitted)
        || (Move == EMove::Recall && !bCommitted) || (Move == EMove::Slip && Age >= 0.21f)
        || (Move == EMove::Wave && (!bCommitted || Age >= 0.29f)) || (Move == EMove::Seal && !bCommitted)
        || Move == EMove::BendHold || (Move == EMove::Bend && !bCommitted)
        || (Move == EMove::Spark && !bCommitted);
    if (!bCanCancel || !Spend(0, Branch() == 'S' || Class->Kit == EKit::SpellWeaver ? 25 : 0, 0)) return;
    const bool bBackstep = Branch() == 'V';
    const FVector Direction = MoveInput(bBackstep ? -Player().GetActorForwardVector() : AimDirection());
    Cancel();
    Player().GetCharacterMovement()->bOrientRotationToMovement = false;
    if (!bBackstep) Player().SetActorRotation(Direction.Rotation());
    const float Duration = bBackstep ? 0.30f : SharedDodgeTime;
    Start(EMove::Dodge, Duration, bBackstep ? DashClip : nullptr);
    if (!bBackstep)
    {
        DodgeSequence = LoadObject<UAnimSequence>(nullptr, RollClip);
        if (DodgeSequence)
        {
            UFSCharacterActions::PlayActionAnimation(&Player(), DodgeSequence, DodgeSequence->GetPlayLength() / Duration);
        }
    }
    PlaySharedSound(TEXT("EvadeCloth"));
    Travel(Direction, SharedDodgeDistance * (bBackstep ? 0.60f : 1.0f), Duration);
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
        UFSCharacterActions::BeginAttackFacing(&Player());
        Player().SetActorRotation(Heading.Rotation());
        Start(EMove::Leap, LeapDuration, LeapClip);
        Travel(Heading, 320, LeapDuration, 55);
        auto* Movement = Player().GetCharacterMovement();
        PreLeapFlySpeed = Movement->MaxFlySpeed;
        Movement->MaxFlySpeed = 0;
        Movement->SetMovementMode(MOVE_Flying);
        LeapContactTarget.Reset();
        bLeapStrikeStarted = false;
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
            PendingWardCenter = Ground;
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
            if (!HasTravelFloor(Player().GetActorLocation() + Direction * 30)
                || GetWorld()->SweepSingleByChannel(Hit, Player().GetActorLocation(), Player().GetActorLocation() + Direction * 30,
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
        Heading = BendDirection(AimDirection(true));
        // Freeze the previewed emitter relative to the caster. The cast pose does not steer the shot.
        BendOffset = Hand() - Player().GetActorLocation();
        Start(EMove::Bend, 0.32f, CastClip);
    }
}

float UFSClassCombat::HitTarget(AActor& Target, float Damage, const FVector& ContactPoint)
{
    if (Target.ActorHasTag(PracticeTag))
    {
        PlaySharedSound(TEXT("Practice"));
        if (auto* Effect = LoadClass<AActor>(nullptr, TEXT("/Game/Combat/FX/BP_FX_Hit.BP_FX_Hit_C")))
        {
            GetWorld()->SpawnActor<AActor>(Effect, FTransform(FRotator::ZeroRotator, ContactPoint, FVector(0.55)));
        }
        return 0;
    }
    const auto* Enemy = Cast<AFSCryptEnemy>(&Target);
    const float Before = Enemy ? Enemy->GetHealth() : 0;
    const float Reported = UGameplayStatics::ApplyDamage(&Target, Damage, Player().GetController(), &Player(), nullptr);
    if (Reported > 0)
    {
        Rumble(Damage >= 30 ? 0.36f : 0.18f, Damage >= 30 ? 0.10f : 0.05f);
        if (auto* Effect = LoadClass<AActor>(nullptr, TEXT("/Game/Combat/FX/BP_FX_Hit.BP_FX_Hit_C")))
        {
            GetWorld()->SpawnActor<AActor>(Effect, ContactPoint, FRotator::ZeroRotator);
        }
    }
    return Enemy ? FMath::Max(0.0f, Before - Enemy->GetHealth()) : Reported;
}

void UFSClassCombat::BladeContact(float From, float To, float Radius, float Damage, int32 Limit)
{
    FVector BladeStart = StrikePoint, BladeEnd = StrikePoint;
    if (StrikePoint.IsNearlyZero()) WeaponEdge(BladeStart, BladeEnd);
    const FVector PriorStart = bHaveBladeSample ? LastBladeStart : BladeStart;
    const FVector PriorEnd = bHaveBladeSample ? LastBladeEnd : BladeEnd;
    LastBladeStart = BladeStart;
    LastBladeEnd = BladeEnd;
    bHaveBladeSample = true;
    if (Age < From || PreviousAge > To || HitActors.Num() >= Limit) return;
    const float Span = FMath::Max(KINDA_SMALL_NUMBER, Age - PreviousAge);
    const float Begin = FMath::Clamp((From - PreviousAge) / Span, 0.0f, 1.0f);
    const float End = FMath::Clamp((To - PreviousAge) / Span, 0.0f, 1.0f);
    const FVector BaseFrom = FMath::Lerp(PriorStart, BladeStart, Begin);
    const FVector TipFrom = FMath::Lerp(PriorEnd, BladeEnd, Begin);
    const FVector BaseTo = FMath::Lerp(PriorStart, BladeStart, End);
    const FVector TipTo = FMath::Lerp(PriorEnd, BladeEnd, End);
    const int32 Segments = FMath::Max(1, FMath::CeilToInt(FMath::Max(FVector::Distance(BaseFrom, TipFrom), FVector::Distance(BaseTo, TipTo)) / Radius));
    for (TActorIterator<AActor> It(GetWorld()); It && HitActors.Num() < Limit; ++It)
    {
        AActor* Target = *It;
        if (HitActors.Contains(Target) || !(IsEnemy(*Target) || IsPracticeTarget(*Target))
            || !UFSCharacterActions::HasClearStrike(&Player(), Target)) continue;
        const auto* Capsule = Target->FindComponentByClass<UCapsuleComponent>();
        const FVector Center = Target->GetActorLocation();
        const double TargetRadius = Capsule ? Capsule->GetScaledCapsuleRadius() : 33;
        const FVector Vertical = Capsule ? FVector(0, 0, Capsule->GetScaledCapsuleHalfHeight() - TargetRadius) : FVector::ZeroVector;
        for (int32 Index = 0; Index <= Segments; ++Index)
        {
            const float AlongBlade = static_cast<float>(Index) / Segments;
            FVector Strike, Body;
            FMath::SegmentDistToSegmentSafe(FMath::Lerp(BaseFrom, TipFrom, AlongBlade), FMath::Lerp(BaseTo, TipTo, AlongBlade),
                Center - Vertical, Center + Vertical, Strike, Body);
            if (FVector::DistSquared(Strike, Body) <= FMath::Square(Radius + TargetRadius))
            {
                HitActors.Add(Target);
                HitTarget(*Target, Damage, Strike);
                break;
            }
        }
    }
}

void UFSClassCombat::LeapContact(const FVector& Start, const FVector& Desired, float FromAge, float ToAge)
{
    if (bCommitted || ToAge < LeapContactOpen || FromAge >= LeapContactClose || ToAge <= FromAge) return;
    const FVector A = FMath::Lerp(Start, Desired, FMath::Clamp((LeapContactOpen - FromAge) / (ToAge - FromAge), 0.0f, 1.0f));
    const FVector B = FMath::Lerp(Start, Desired, FMath::Clamp((LeapContactClose - FromAge) / (ToAge - FromAge), 0.0f, 1.0f));
    const auto* Capsule = Player().GetCapsuleComponent();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(FSFuryLeap), false, &Player());
    FCollisionObjectQueryParams Pawns(ECC_Pawn);
    TArray<FHitResult> Contacts;
    GetWorld()->SweepMultiByObjectType(Contacts, A, B + Heading * 50, FQuat::Identity, Pawns, Capsule->GetCollisionShape(), Query);
    TArray<AActor*> Candidates;
    for (const FHitResult& Contact : Contacts)
    {
        AActor* Target = Contact.GetActor();
        if (IsValid(Target) && (IsEnemy(*Target) || IsPracticeTarget(*Target))) Candidates.AddUnique(Target);
    }
    if (IsGuild())
    {
        TArray<AActor*> Dummies;
        UGameplayStatics::GetAllActorsWithTag(this, PracticeTag, Dummies);
        for (AActor* Dummy : Dummies)
        {
            if (!IsPracticeTarget(*Dummy) || Candidates.Contains(Dummy)) continue;
            const FVector Center = Dummy->GetActorLocation();
            FVector Sweep, Body;
            const double CylinderHeight = Capsule->GetScaledCapsuleHalfHeight() - Capsule->GetScaledCapsuleRadius();
            const FVector Height(0, 0, CylinderHeight);
            FMath::SegmentDistToSegmentSafe(A, B + Heading * 50, Center - Height, Center + Height, Sweep, Body);
            if (FVector::DistSquared(Sweep, Body) <= FMath::Square(Capsule->GetScaledCapsuleRadius() + 30.0))
            {
                Candidates.Add(Dummy);
            }
        }
    }
    for (AActor* Target : Candidates)
    {
        if (!UFSCharacterActions::HasClearStrike(&Player(), Target)) continue;
        if (LeapContactTarget.IsValid() && Target != LeapContactTarget.Get()) continue;
        if (FVector::DotProduct((Target->GetActorLocation() - A).GetSafeNormal2D(), Heading) <= 0) continue;
        bCommitted = true;
        if (!IsPracticeTarget(*Target)) MoveTime = 0;
        PlaySound(TEXT("SFX_Hit"));
        const float Actual = HitTarget(*Target, 40.8f, Target->GetActorLocation());
        if (!IsGuild() && Actual > 0)
        {
            const float Before = Value(TEXT("Health"));
            const float After = FMath::Min(Value(TEXT("MaxHealth")), Before + Actual * 0.5f);
            SetValue(TEXT("Health"), After);
            Message(FString::Printf(TEXT("Fury Leap  +%.1f V"), After - Before));
        }
        break;
    }
}

void UFSClassCombat::PlayLeapStrike()
{
    bLeapStrikeStarted = true;
    if (auto* Animation = LoadObject<UAnimSequenceBase>(nullptr, FollowClip))
    {
        UFSCharacterActions::PlayActionAnimation(&Player(), Animation, Animation->GetPlayLength() / (LeapDuration - LeapStrikeStart));
    }
}

void UFSClassCombat::Fire(FShot Shot)
{
    if (Shot.HalfWidth == 0)
    {
        const FVector Origin = Player().GetActorLocation();
        FCollisionQueryParams Query(SCENE_QUERY_STAT(FSClassEmission), false, &Player());
        FHitResult Hit;
        if (Shot.bCheckEmission
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
    if (Branch() == 'M' || Move == EMove::Thread || Move == EMove::Recall) Rumble(0.14f, 0.07f);
}

void UFSClassCombat::TickShots(float DeltaTime)
{
    for (int32 Index = Shots.Num() - 1; Index >= 0; --Index)
    {
        FShot& Shot = Shots[Index];
        float Remaining = FMath::Min(Shot.Range, Shot.Speed * DeltaTime);
        bool bStop = false;
        FHitResult Wall;
        while (Remaining > KINDA_SMALL_NUMBER && !bStop)
        {
            const float Step = FMath::Min(20.0f, Remaining);
            const FVector Desired = ShotEndpoint(Shot.Position, Shot.Direction, Shot.TurnDegrees, Step);
            FCollisionQueryParams Query(SCENE_QUERY_STAT(FSClassShot), false, &Player());
            // Reserve room for the return sweep even when the thread reaches maximum range beside a wall.
            const float WallRadius = Shot.bThread ? ThreadRecallRadius + 1 : FMath::Max(1.0f, Shot.Radius);
            bStop = GetWorld()->SweepSingleByObjectType(Wall, Shot.Position, Desired, FQuat::Identity, Walls(),
                FCollisionShape::MakeSphere(WallRadius), Query);
            const FVector End = bStop ? Wall.Location : Desired;
            struct FShotContact
            {
                double DistanceSquared;
                AActor* Target;
                FVector Point;
            };
            TArray<FShotContact> Contacts;
            for (TActorIterator<AActor> It(GetWorld()); It && Shot.Hit.Num() < Shot.TargetLimit; ++It)
            {
                AActor* Target = *It;
                if (Shot.Hit.Contains(Target) || !(IsEnemy(*Target) || IsPracticeTarget(*Target))) continue;
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
                    Contacts.Add({FVector::DistSquared(Shot.Position, Along), Target, bBox ? End : Along});
                }
            }
            Contacts.Sort([](const FShotContact& A, const FShotContact& B) { return A.DistanceSquared < B.DistanceSquared; });
            for (const auto& Contact : Contacts)
            {
                Shot.Hit.Add(Contact.Target);
                HitTarget(*Contact.Target, Shot.Damage, Contact.Point);
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
            if (Wall.bBlockingHit) PlaySharedSound(TEXT("StoneStop"));
            if (Shot.bThread)
            {
                if (Wall.bStartPenetrating) PlaySound(TEXT("SFX_Hit"));
                else
                {
                    Anchor = Shot.Position;
                    AnchorUntil = GetWorld()->GetTimeSeconds() + 2.5;
                    AnchorVisual = Marker(*GetWorld(), Anchor, FVector(0.28));
                }
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
        const FVector StartLocation = Player().GetActorLocation();
        double TravelFraction = To - From;
        if (DodgeSequence)
        {
            const double Length = DodgeSequence->GetPlayLength();
            const auto RootPosition = [&](double Alpha)
            {
                return DodgeSequence->ExtractRootTrackTransform(FAnimExtractContext(Alpha * Length), nullptr).GetTranslation();
            };
            const FVector RootTravel = RootPosition(1) - RootPosition(0);
            if (!RootTravel.IsNearlyZero())
            {
                TravelFraction = FVector::DotProduct(RootPosition(To) - RootPosition(From), RootTravel) / RootTravel.SizeSquared();
            }
        }
        const FVector Offset = Direction * MoveDistance * TravelFraction + FVector(0, 0, ArcDelta);
        FHitResult Obstruction;
        const bool bClear = MovePlayer(Offset, Obstruction);
        if (Move == EMove::Leap && (bClear || Obstruction.bBlockingHit))
        {
            const float TravelEnd = bClear ? Age : FMath::Lerp(PreviousAge, Age, Obstruction.Time);
            if (!bClear && TravelEnd < LeapContactOpen && IsValid(Obstruction.GetActor()) && IsEnemy(*Obstruction.GetActor()))
            {
                LeapContactTarget = Obstruction.GetActor();
                Player().GetCharacterMovement()->SetMovementMode(MOVE_Falling);
            }
            LeapContact(StartLocation, Player().GetActorLocation(), PreviousAge, TravelEnd);
        }
        if (!bClear || To >= 1)
        {
            MoveTime = 0;
        }
        if (Move == EMove::Leap && MoveTime <= 0 && Player().GetCharacterMovement()->MovementMode == MOVE_Flying)
        {
            Player().GetCharacterMovement()->SetMovementMode(MOVE_Falling);
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
        if (!bLeapStrikeStarted && Age >= LeapStrikeStart) PlayLeapStrike();
        if (LeapContactTarget.IsValid() && !bCommitted)
        {
            const FVector Position = Player().GetActorLocation();
            LeapContact(Position, Position, PreviousAge, Age);
        }
        if (Age >= EndTime)
        {
            if (!bCommitted) Message(TEXT("Fury Leap missed"));
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
        if (!bCommitted && Age >= 0.10f)
        {
            bCommitted = true;
            Fire({Hand(), Heading, 9, 1600, 550, 12});
            if (InsideWard()) PulseWard();
        }
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
                if (InsideWard()) PulseWard();
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
            const FVector Origin = Hand();
            if (GetWorld()->LineTraceTestByObjectType(Player().GetActorLocation(), Origin, Walls(), Query)
                || GetWorld()->OverlapAnyTestByObjectType(Origin, FQuat::Identity, Walls(), FCollisionShape::MakeSphere(ThreadRecallRadius + 1), Query))
            {
                Message(TEXT("Path blocked"));
                break;
            }
            if (Spend(12, 0, 18))
            {
                Cooldown[Second] = Now + 3.5;
                FShot Thread{Origin, Heading, 20, 1800, 540, 16};
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
                FShot Thread{Anchor, Delta.GetSafeNormal2D(), ThreadRecallRadius, 2200, FMath::Min(750.0, Delta.Size2D()), 28};
                Thread.TargetLimit = 3;
                Thread.bCheckEmission = false;
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
            FVector Ground;
            if (!FindWardFloor(PendingWardCenter, Ground) || FMath::Abs(Ground.Z - PendingWardCenter.Z) > 3)
            {
                Message(TEXT("Choose clear ground for the seal"));
            }
            else if (Spend(0, 0, 18))
            {
                Cooldown[Second] = Now + 4.5;
                SetWard(Ground);
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
                FShot Shot{Player().GetActorLocation() + BendOffset, Heading, 18, 1450, 900, 28};
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
    WardPulseUntil = 0;
    bWasInsideWard = false;
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
        UpdateDefeat(DeltaTime);
        ClearEffects();
        SetSelectionOpen(false);
        bAim = false;
        RequestedInputs = 0;
        UpdateCamera(DeltaTime);
        return;
    }
    if (bPlayedDefeat) UpdateDefeat(DeltaTime);

    const double Now = GetWorld()->GetTimeSeconds();
    for (int32 Slot : {First, Second})
    {
        if (Cooldown[Slot] > LastReadyCue && Cooldown[Slot] <= Now) PlaySharedSound(TEXT("Ready"), true);
    }
    LastReadyCue = Now;
    UpdateFootsteps(DeltaTime);
    if (auto* Label = AltarLabel.Get(); Label && InputController.IsValid() && InputController->PlayerCameraManager)
    {
        const FVector ToCamera = InputController->PlayerCameraManager->GetCameraLocation() - Label->GetComponentLocation();
        Label->SetWorldRotation(ToCamera.GetSafeNormal2D().Rotation());
    }
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
    const bool bInsideWard = InsideWard();
    if (bInsideWard != bWasInsideWard && WardUntil > Now) PulseWard();
    bWasInsideWard = bInsideWard;
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
    if (Damage <= 0 || Unavailable() || Flag(Player(), TEXT("bDodging"))) return Damage;
    const auto* Enemy = Cast<AFSCryptEnemy>(Causer);
    const bool bBlockable = (Enemy && Enemy->IsCurrentAttackBlockable())
        || (Causer && Causer->GetClass()->GetName() == TEXT("BP_MeleeEnemy_C"));
    const float Incoming = Damage;
    if (!bBlockable)
    {
        if (Move == EMove::Idle && Damage < Value(TEXT("Health"))) PlayReaction(TEXT("Get_Hit_1"), 0.32f);
        return Damage;
    }
    const FVector Offset = (Causer->GetActorLocation() - Player().GetActorLocation()).GetSafeNormal2D();
    const float Dot = FVector::DotProduct(Player().GetActorForwardVector(), Offset);
    if (Dot >= 0.642788f)
    {
        if (Move == EMove::ShieldHold) Damage *= 0.40f;
        else if (Move == EMove::StoneHold && Age >= 0.12f) Damage *= 0.55f;
        else if (Move == EMove::PaidStand && Age < 0.32f && Dot >= 0.707107f) Damage *= 0.30f;
    }
    if (InsideWard())
    {
        const float Absorbed = FMath::Min(Damage * 0.35f, WardCapacity);
        WardCapacity -= Absorbed;
        Damage -= Absorbed;
        PulseWard();
        if (WardCapacity <= KINDA_SMALL_NUMBER)
        {
            WardUntil = 0;
            if (AActor* Visual = WardVisual.Get()) Visual->Destroy();
            WardVisual.Reset();
        }
    }
    if (Damage < Incoming)
    {
        Rumble(0.26f, 0.08f);
    }
    else if (Move == EMove::Idle && Damage < Value(TEXT("Health")))
    {
        PlayReaction(Dot >= 0.64f ? TEXT("Get_Hit_1") : TEXT("Get_Hit_2"), 0.32f);
    }
    return Damage;
}

void UFSClassCombat::SetWard(const FVector& Ground)
{
    if (AActor* Visual = WardVisual.Get()) Visual->Destroy();
    WardVisual = Marker(*GetWorld(), Ground + FVector(0, 0, 3), FVector(0.12, 0.12, 0.025), true);
    WardCenter = Ground;
    WardUntil = GetWorld()->GetTimeSeconds() + 3.2;
    WardCapacity = WardMaxCapacity;
    bWasInsideWard = false;
}

bool UFSClassCombat::InsideWard() const
{
    const FVector Feet = Player().GetActorLocation() - FVector(0, 0, Player().GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    return WardUntil > GetWorld()->GetTimeSeconds()
        && FVector::DistSquared2D(Feet, WardCenter) <= FMath::Square(WardRadius)
        && FMath::Abs(Feet.Z - WardCenter.Z) <= WardHeight;
}

void UFSClassCombat::PulseWard()
{
    WardPulseUntil = GetWorld()->GetTimeSeconds() + 0.24;
    PlaySound(TEXT("SFX_Hit"));
}

bool UFSClassCombat::WardGround(FVector& Ground) const
{
    const FVector Origin = Player().GetActorLocation();
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
    return FindWardFloor(Horizontal, Ground);
}

bool UFSClassCombat::FindWardFloor(const FVector& Point, FVector& Ground) const
{
    const FVector Origin = Player().GetActorLocation();
    const float Feet = Origin.Z - Player().GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    FHitResult Hit;
    const FCollisionQueryParams Query(SCENE_QUERY_STAT(FSWardGround), false, &Player());
    const bool bGround = GetWorld()->LineTraceSingleByObjectType(Hit, FVector(Point.X, Point.Y, Feet + 100),
        FVector(Point.X, Point.Y, Feet - 100), Walls(), Query);
    Ground = bGround ? Hit.ImpactPoint : FVector(Point.X, Point.Y, Feet);
    return bGround && Hit.ImpactNormal.Z >= Player().GetCharacterMovement()->GetWalkableFloorZ()
        && !GetWorld()->LineTraceTestByObjectType(Origin, Ground + FVector(0, 0, 5), Walls(), Query);
}

void UFSClassCombat::BeginAim()
{
    if (!bSelecting && !Unavailable()) bAim = true;
}

void UFSClassCombat::EndAim() { bAim = false; }

bool UFSClassCombat::IsBasicAttacking() const
{
    return Move == EMove::Axe || Move == EMove::Sword || Move == EMove::Spark;
}

void UFSClassCombat::SetBasicAttackMovementLock(bool bLocked)
{
    AController* Controller = bLocked ? Player().GetController() : nullptr;
    if (Controller == AttackMovementController.Get()) return;
    if (auto* Previous = AttackMovementController.Get()) Previous->SetIgnoreMoveInput(false);
    AttackMovementController = Controller;
    if (Controller)
    {
        Controller->SetIgnoreMoveInput(true);
        Player().ConsumeMovementInputVector();
        Player().GetCharacterMovement()->StopMovementImmediately();
    }
}

void UFSClassCombat::UpdateCamera(float DeltaTime)
{
    if (auto* Arm = CameraArm.Get())
    {
        CameraAimBlend = FMath::Lerp(CameraAimBlend, bAim ? 1.0f : 0.0f, 1 - FMath::Exp(-13 * DeltaTime));
        Arm->TargetArmLength = OriginalArm * FMath::Lerp(1.0f, 0.72f, CameraAimBlend);
        Arm->SocketOffset = OriginalSocketOffset + FMath::Lerp(FollowShoulder, AimShoulder, CameraAimBlend);
    }
    float Scale = bAim ? 0.70f : 1.0f;
    if (Move == EMove::ShieldHold || Move == EMove::BendHold) Scale = FMath::Min(Scale, 0.55f);
    if (Move == EMove::OrbHold) Scale = FMath::Min(Scale, 0.60f);
    if (IsBasicAttacking() || MoveTime > 0 || Move == EMove::Dodge || Move == EMove::StoneHold || Move == EMove::PaidStand || bSelecting)
    {
        Scale = 0;
    }
    auto* Movement = Player().GetCharacterMovement();
    Movement->MaxWalkSpeed = OriginalWalkSpeed * Scale;
    const bool bWantsAimFacing = bAim && Move == EMove::Idle;
    if (bWantsAimFacing != bAimFacing)
    {
        bAimFacing = bWantsAimFacing;
        Movement->bUseControllerDesiredRotation = bAimFacing;
        Movement->bOrientRotationToMovement = !bAimFacing && Move == EMove::Idle;
    }
    if (Move == EMove::Idle) Movement->bOrientRotationToMovement = !bAimFacing;
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
    ClassWeapon->SetRelativeRotation(FRotator(90, 0, 0));
    ClassWeapon->SetRelativeLocation(FVector(Branch() == 'V' ? -16 : -14, 0, 0));
    ClassWeapon->SetRelativeScale3D(FVector(Branch() == 'V' ? 1.25 : 0.78));
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
        SetSelectionOpen(true);
        bAim = false;
        SelectionIndex = 0;
        PlaySharedSound(TEXT("Select"), true);
    }
    else
    {
        Message(TEXT("No essence choice is available."));
        PlaySharedSound(TEXT("Refuse"), true);
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
        SetSelectionOpen(false);
        ClearEffects();
        Restore(bEvolution);
        PlaySharedSound(TEXT("Confirm"), true);
        PlaySharedSound(TEXT("Unlock"), true);
        if (IsValid(Altar))
        {
            if (auto* Pulse = LoadClass<AActor>(nullptr, TEXT("/Game/Combat/FX/BP_FX_Evolve.BP_FX_Evolve_C")))
            {
                FActorSpawnParameters Params;
                Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                GetWorld()->SpawnActor<AActor>(Pulse, Altar->GetActorLocation() + FVector(0, 0, 48), FRotator::ZeroRotator, Params);
            }
        }
        SetString(Player(), TEXT("BannerText"), Text);
        SetValue(TEXT("BannerUntil"), GetWorld()->GetTimeSeconds() + 3.0);
    }
    else PlaySharedSound(TEXT("Refuse"), true);
    Message(Text);
}

void UFSClassCombat::PreviousOption()
{
    if (!bSelecting) return;
    SelectionIndex = (SelectionIndex + 2) % 3;
    PlaySharedSound(TEXT("Select"), true);
}

void UFSClassCombat::NextOption()
{
    if (!bSelecting) return;
    SelectionIndex = (SelectionIndex + 1) % 3;
    PlaySharedSound(TEXT("Select"), true);
}

void UFSClassCombat::CloseSelection()
{
    if (bSelecting) PlaySharedSound(TEXT("Back"), true);
    SetSelectionOpen(false);
}

void UFSClassCombat::SetSelectionOpen(bool bOpen)
{
    if (bSelecting == bOpen) return;
    if (auto* Controller = InputController.Get()) Controller->SetIgnoreMoveInput(bOpen);
    if (bOpen)
    {
        Player().ConsumeMovementInputVector();
        Player().GetCharacterMovement()->StopMovementImmediately();
    }
    bSelecting = bOpen;
}
void UFSClassCombat::Interaction()
{
    if (!Unavailable()) CallEvent(Player(), TEXT("DoInteract"));
}

void UFSClassCombat::QueueAttack() { RequestedInputs |= 1 << 0; }
void UFSClassCombat::QueueEvade() { RequestedInputs |= 1 << 1; }
void UFSClassCombat::QueueSkillOne() { RequestedInputs |= 1 << 2; }
void UFSClassCombat::QueueSkillTwo() { RequestedInputs |= 1 << 3; }
void UFSClassCombat::QueueFirstRelease() { RequestedInputs |= 1 << 4; }
void UFSClassCombat::QueueSecondRelease() { RequestedInputs |= 1 << 5; }

void UFSClassCombat::JumpPressed()
{
    bJumpForwarded = !Unavailable() && !bSelecting && Move == EMove::Idle;
    if (bJumpForwarded)
    {
        if (Player().CanJump()) PlaySharedSound(TEXT("Takeoff"));
        Player().Jump();
    }
}

void UFSClassCombat::JumpReleased()
{
    if (bJumpForwarded) Player().StopJumping();
    bJumpForwarded = false;
}

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
    SetSelectionOpen(false);
    SetBasicAttackMovementLock(IsBasicAttacking());
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
    for (FKey Key : {EKeys::SpaceBar, EKeys::Gamepad_FaceButton_Bottom})
    {
        Bind(Key, IE_Pressed, &UFSClassCombat::JumpPressed);
        Bind(Key, IE_Released, &UFSClassCombat::JumpReleased);
    }
    for (FKey Key : {EKeys::Up, EKeys::Left, EKeys::MouseScrollUp, EKeys::Gamepad_DPad_Up, EKeys::Gamepad_DPad_Left})
    {
        Bind(Key, IE_Pressed, &UFSClassCombat::PreviousOption);
    }
    for (FKey Key : {EKeys::Down, EKeys::Right, EKeys::MouseScrollDown, EKeys::Gamepad_DPad_Down, EKeys::Gamepad_DPad_Right})
    {
        Bind(Key, IE_Pressed, &UFSClassCombat::NextOption);
    }
    Bind(EKeys::BackSpace, IE_Pressed, &UFSClassCombat::CloseSelection, false);
    Bind(EKeys::Gamepad_Special_Left, IE_Pressed, &UFSClassCombat::CloseSelection, false);
    Controller.PushInputComponent(Input);
    InputController = &Controller;
}

void UFSClassCombat::Draw(AHUD& HUD) const
{
    APlayerController* Controller = HUD.GetOwningPlayerController();
    if (!Class || !Controller) return;
    int32 PixelWidth, PixelHeight;
    Controller->GetViewportSize(PixelWidth, PixelHeight);
    if (PixelWidth <= 0 || PixelHeight <= 0) return;
    const float UIScale = FMath::Min(PixelWidth / 1280.0f, PixelHeight / 720.0f);
    const float Width = PixelWidth / UIScale;
    const float Height = PixelHeight / UIScale;
    const double Now = GetWorld()->GetTimeSeconds();
    const FLinearColor Panel(0.025f, 0.035f, 0.052f, 0.96f);
    const FLinearColor Gold(1.0f, 0.85f, 0.3f);
    const auto Rect = [&](FLinearColor Color, float X, float Y, float W, float H)
    {
        HUD.DrawRect(Color, X * UIScale, Y * UIScale, W * UIScale, H * UIScale);
    };
    const auto DrawLabel = [&](const FString& Value, FLinearColor Color, float X, float Y, float Size)
    {
        HUD.DrawText(Value, Color, X * UIScale, Y * UIScale, nullptr, Size * UIScale);
    };
    const auto Icon = [&](int32 Index, float X, float Y, float Size)
    {
        if (SharedIcons.IsValidIndex(Index) && SharedIcons[Index])
        {
            HUD.DrawTexture(SharedIcons[Index], X * UIScale, Y * UIScale, Size * UIScale, Size * UIScale,
                0, 0, 1, 1, FLinearColor::White, BLEND_Translucent);
        }
    };
    const auto Text = [&](const FString& Label, FLinearColor Color, float X, float Y, float Scale, bool bCentered = false)
    {
        if (Label.IsEmpty()) return;
        float TextWidth, TextHeight;
        HUD.GetTextSize(Label, TextWidth, TextHeight, nullptr, 1.0f);
        Scale = FMath::Min(Scale, (Width - 28.0f - (bCentered ? 0.0f : X)) / FMath::Max(1.0f, TextWidth));
        if (bCentered) X = (Width - TextWidth * Scale) * 0.5f;
        Rect(Panel, X - 6, Y - 3, TextWidth * Scale + 12, TextHeight * Scale + 6);
        DrawLabel(Label, Color, X, Y, Scale);
    };
    const auto Key = [&](const TCHAR* Name, float X, float Y)
    {
        Rect(FLinearColor(0.25f, 0.30f, 0.38f), X, Y, 28, 28);
        Rect(Panel, X + 1, Y + 1, 26, 26);
        float W, H;
        HUD.GetTextSize(Name, W, H, nullptr, 1.4f);
        DrawLabel(Name, Gold, X + (28 - W) * 0.5f, Y + (28 - H) * 0.5f, 1.4f);
    };
    const bool bWon = Flag(Player(), TEXT("bWon"));
    const bool bDead = Flag(Player(), TEXT("bDead"));
    const UGameInstance* Instance = GetWorld()->GetGameInstance();
    if (bWon || bDead)
    {
        const bool bPending = Instance && Flag(*Instance, TEXT("RewardPending"));
        Rect(bWon ? FLinearColor(0.02f, 0.025f, 0.055f, 0.84f)
                         : FLinearColor(0.13f, 0.02f, 0.03f, 0.84f), 0, 0, Width, Height);
        Text(bDead ? TEXT("YOU DIED") : bPending ? TEXT("REWARD PENDING") : TEXT("CONTRACT COMPLETE"),
            bDead ? FLinearColor(1, 0.38f, 0.3f) : Gold, 0, Height * 0.28f, 3.4f, true);
        if (bWon)
        {
            Text(bPending ? TEXT("Reward save failed. Retry to keep your reward.")
                : FString::Printf(TEXT("Saved reward: %d XP + %s"), ReadInteger(Instance, TEXT("QuestXPReward")),
                    Instance ? *ReadString(*Instance, TEXT("QuestItemReward")) : TEXT("")),
                FLinearColor::White, 0, Height * 0.45f, 1.5f, true);
            if (!bPending) Text(TEXT("Gerald: 'Not bad, champ. Same time next dungeon?'"),
                FLinearColor(0.7f, 0.9f, 1), 0, Height * 0.55f, 1.2f, true);
        }
        Text(bPending ? TEXT("[R] Retry reward save     [Esc] Quit (reward unsaved)")
                      : TEXT("[R] Return to Guild     [Esc] Quit"), Gold, 0, Height * 0.70f, 1.6f, true);
        return;
    }
    float Y = 30.0f;
    if (bAim && !bSelecting)
    {
        const float X = PixelWidth * 0.5f, CenterY = PixelHeight * 0.5f;
        const float Radius = 8 * UIScale;
        const FCollisionQueryParams Query(SCENE_QUERY_STAT(FSAimHandPreview), false, &Player());
        const bool bBlocked = Branch() == 'M'
            && GetWorld()->LineTraceTestByObjectType(Player().GetActorLocation(), Hand(), Walls(), Query);
        if (bBlocked)
        {
            HUD.DrawLine(X - Radius, CenterY - Radius, X + Radius, CenterY + Radius, Gold, 2 * UIScale);
            HUD.DrawLine(X - Radius, CenterY + Radius, X + Radius, CenterY - Radius, Gold, 2 * UIScale);
        }
        else
        {
            HUD.DrawLine(X - Radius, CenterY, X + Radius, CenterY, FLinearColor::White, UIScale);
            HUD.DrawLine(X, CenterY - Radius, X, CenterY + Radius, FLinearColor::White, UIScale);
        }
    }
    const auto Resource = [&](int32 Index, const TCHAR* Label, const TCHAR* Field, float Max, FLinearColor Color)
    {
        if (Max <= 0) return;
        const float Current = Value(Field);
        const float BarWidth = FMath::Min(306.0f, Width - 52.0f);
        Rect(Panel, 26, Y, BarWidth, 26);
        Rect(Color, 26, Y, BarWidth * FMath::Clamp(Current / Max, 0.0f, 1.0f), 26);
        Icon(Index, 29, Y + 1, 24);
        DrawLabel(FString::Printf(TEXT("%s   %.1f / %.1f"), Label, Current, Max), FLinearColor::White, 59, Y + 3, 1.15f);
        Y += 30;
    };
    Resource(0, TEXT("V"), TEXT("Health"), Class->Vitality, FLinearColor(0.78f, 0.08f, 0.10f));
    Resource(1, TEXT("S"), TEXT("Stamina"), Class->Stamina, FLinearColor(0.09f, 0.50f, 0.18f));
    Resource(2, TEXT("M"), TEXT("Mana"), Class->Mana, FLinearColor(0.10f, 0.31f, 0.80f));
    const USaveGame* Profile = FSEssence::Profile(Player());
    Text(FString::Printf(TEXT("Class: %s"), Class->Name), FLinearColor::White, 30, Y + 6, 1.4f);
    const FString History = FSEssence::History(Profile);
    if (!History.IsEmpty()) Text(FString::Printf(TEXT("Essence: %s"), *History), FLinearColor::White, 30, Y + 30, 1.2f);
    Text(FString::Printf(TEXT("Level: %d   Total XP: %d"), ReadInteger(Profile, TEXT("CharLevel")), ReadInteger(Profile, TEXT("XP"))),
        FLinearColor::White, 30, Y + 52, 1.2f);
    const FString Inventory = Profile ? ReadString(*Profile, TEXT("Inventory")) : FString();
    if (!Inventory.IsEmpty()) Text(TEXT("Inventory: ") + Inventory, FLinearColor::White, 30, Y + 74, 1.1f);
    const int32 Pending = FSEssence::PendingChoices(Profile);
    if (Pending > 0) Text(FString::Printf(TEXT("Unspent essence choices: %d"), Pending), FLinearColor::White, 30, Y + 96, 1.1f);
    Text(ReadString(Player(), TEXT("Message")), Gold, 30, Y + 121, 1.2f);
    if (!bSelecting)
    {
        Text(ReadString(Player(), TEXT("Prompt")), FLinearColor::White, 0, Height - 78.0f, 1.5f, true);
    }
    if (const auto* Banner = Property<FNumericProperty>(Player(), TEXT("BannerUntil"));
        Banner && Now < Banner->GetFloatingPointPropertyValue(Banner->ContainerPtrToValuePtr<void>(&Player())))
    {
        Text(ReadString(Player(), TEXT("BannerText")), FLinearColor(0.5f, 0.75f, 1), 0, Height * 0.17f, 2.0f, true);
    }
    if (Instance) Text(ReadString(*Instance, TEXT("RecoveryMessage")), Gold, 0, Height * 0.59f, 1.1f, true);
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
    if (Branch() && !bSelecting)
    {
        const int32 AttackIcon = Branch() == 'V' ? 3 : Branch() == 'S' ? 4 : 5;
        const float ActionY = Height - 58.0f;
        Icon(AttackIcon, 22, ActionY, 44);
        Text(TEXT("LMB / RT / R2"), FLinearColor::White, 75, ActionY + 12, 1.35f);
        Icon(Branch() == 'V' ? 6 : 7, 250, ActionY, 44);
        Text(TEXT("Shift / B / Circle"), FLinearColor::White, 303, ActionY + 12, 1.35f);
        const float SkillY = Height - 126.0f;
        const auto DrawSkill = [&](const TCHAR* Button, const TCHAR* Pad, const TCHAR* Name, int32 Slot, float X,
            bool bRecall = false)
        {
            const double Remaining = FMath::Max(0.0, (bRecall ? AnchorUntil : Cooldown[Slot]) - Now);
            const FString Label = Remaining > 0
                ? FString::Printf(TEXT("%s  %.1fs"), Name, Remaining) : FString(Name);
            Key(Button, X, SkillY);
            Text(Label, bRecall ? Gold : FLinearColor::White, X + 40, SkillY + 3, 1.35f);
            DrawLabel(Pad, FLinearColor(0.80f, 0.85f, 0.95f), X + 1, SkillY + 30, 0.95f);
        };
        DrawSkill(TEXT("F"), TEXT("LB / L1"), SkillOneName, First, 28);
        if (AnchorUntil > Now)
        {
            DrawSkill(TEXT("Q"), TEXT("RB / R1"), TEXT("Recall"), Second, 286, true);
        }
        else if (SkillTwoName) DrawSkill(TEXT("Q"), TEXT("RB / R1"), SkillTwoName, Second, 286);
    }
    if (WardUntil > Now && Move != EMove::OrbHold)
    {
        Text(FString::Printf(TEXT("Seal  %.1f / %.1f  %s"), WardCapacity, WardMaxCapacity, InsideWard() ? TEXT("Inside") : TEXT("Outside")),
            FLinearColor::White, 28, Height - 155.0f, 1.0f);
    }
    if (Move == EMove::OrbHold)
    {
        Text(Age >= 0.45f ? TEXT("Focused Orb") : TEXT("Broad Orb"), FLinearColor::White, 28, Height - 155.0f, 1.0f);
    }
    if (Move == EMove::ShieldHold)
        Text(Age >= 0.30f ? TEXT("Guard ready") : TEXT("Guard raised"), Gold, 28, Height - 155.0f, 1.0f);
    if (Move == EMove::StoneHold)
        Text(Age >= 0.50f ? TEXT("Stonebreak charged") : TEXT("Bracing"), Gold, 28, Height - 155.0f, 1.0f);
    const auto Ring = [&](const FVector& Center, float Radius, FLinearColor Color, bool bDashed)
    {
        for (int32 Index = 0; Index < 36; Index += bDashed ? 2 : 1)
        {
            const FVector A = HUD.Project(Center + FVector(Radius, 0, 5).RotateAngleAxis(Index * 10, FVector::UpVector));
            const FVector B = HUD.Project(Center + FVector(Radius, 0, 5).RotateAngleAxis((Index + 1) * 10, FVector::UpVector));
            if (A.Z > 0 && B.Z > 0)
            {
                HUD.DrawLine(A.X, A.Y, B.X, B.Y, FLinearColor::Black, 5 * UIScale);
                HUD.DrawLine(A.X, A.Y, B.X, B.Y, Color, 2.5f * UIScale);
            }
        }
    };
    if (WardUntil > Now)
    {
        const bool bPulse = WardPulseUntil > Now;
        Ring(WardCenter, WardRadius, bPulse ? FLinearColor::White : FLinearColor(0.35f, 0.85f, 1), false);
        if (bPulse) Ring(WardCenter, WardRadius * FMath::Clamp((0.24 - (WardPulseUntil - Now)) / 0.24, 0.0, 1.0), Gold, false);
    }
    if (Class->Kit == EKit::Paladin && bAim && Move == EMove::Idle && Ready(Second))
    {
        FVector Ground;
        const bool bClear = WardGround(Ground);
        if (bClear) Ring(Ground, WardRadius, FLinearColor::White, false);
        else
        {
            Ring(Ground, WardRadius, Gold, true);
            Text(TEXT("Seal blocked"), Gold, Width * 0.5f + 16, Height * 0.5f + 16, 1.0f);
        }
    }
    if (Move == EMove::BendHold)
    {
        FVector Position = Hand();
        FVector Direction = BendDirection(AimDirection(true));
        const float Turn = CurveSide * 70.0f / 900.0f;
        const FCollisionQueryParams Query(SCENE_QUERY_STAT(FSBendPreview), false, &Player());
        for (int32 Index = 0; Index < 45; ++Index)
        {
            const FVector End = ShotEndpoint(Position, Direction, Turn, 20);
            FHitResult Hit;
            const bool bBlocked = GetWorld()->SweepSingleByObjectType(Hit, Position, End, FQuat::Identity, Walls(), FCollisionShape::MakeSphere(18), Query);
            const FVector StartScreen = HUD.Project(Position);
            const FVector EndScreen = HUD.Project(bBlocked ? Hit.Location : End);
            const FLinearColor Color = bBlocked ? Gold : FLinearColor(0.5f, 0.8f, 1);
            if (StartScreen.Z > 0 && EndScreen.Z > 0)
            {
                HUD.DrawLine(StartScreen.X, StartScreen.Y, EndScreen.X, EndScreen.Y, FLinearColor::Black, 5 * UIScale);
                HUD.DrawLine(StartScreen.X, StartScreen.Y, EndScreen.X, EndScreen.Y, Color, 2 * UIScale);
                if (bBlocked || Index == 44)
                {
                    const float MarkerSize = 5 * UIScale;
                    HUD.DrawLine(EndScreen.X - MarkerSize, EndScreen.Y - MarkerSize, EndScreen.X + MarkerSize, EndScreen.Y + MarkerSize, Color, 2 * UIScale);
                    HUD.DrawLine(EndScreen.X - MarkerSize, EndScreen.Y + MarkerSize, EndScreen.X + MarkerSize, EndScreen.Y - MarkerSize, Color, 2 * UIScale);
                }
            }
            if (bBlocked) break;
            Position = End;
            Direction = Direction.RotateAngleAxis(Turn * 20, FVector::UpVector);
        }
    }
    if (!bSelecting) return;
    const float MenuWidth = 530;
    const float X = (Width - MenuWidth) * 0.5f, Top = Height * 0.5f - 160;
    Rect(Panel, X, Top, MenuWidth, 320);
    DrawLabel(TEXT("Choose an essence"), FLinearColor::White, X + 22, Top + 14, 2.0f);
    if (!History.IsEmpty()) Text(TEXT("Current path: ") + History, FLinearColor::White, 0, Top + 44, 1.05f, true);
    for (int32 Index = 0; Index < 3; ++Index)
    {
        FString Path = Class->Path;
        if (!Path.IsEmpty()) Path += TEXT(", ");
        Path.AppendChar(TEXT("VSM")[Index]);
        bool bSupported;
        const auto& Target = FSEssence::Resolve(Path, bSupported);
        const FLinearColor Color = Index == SelectionIndex ? FLinearColor(1, 0.78f, 0.30f) : FLinearColor::White;
        const float RowY = Top + 71 + Index * 43;
        if (Index == SelectionIndex)
        {
            Rect(FLinearColor(0.15f, 0.17f, 0.22f), X + 16, RowY, MenuWidth - 32, 40);
            Rect(Gold, X + 16, RowY, 3, 40);
        }
        Icon(Index, X + 26, RowY + 2, 36);
        DrawLabel(FString::Printf(TEXT("%s    %s"), *Path, Target.Name), Color, X + 73, RowY + 8, 1.5f);
        if (Index == SelectionIndex)
        {
            const int32 PreviewIndex = !Branch() ? Index : 3 + (Branch() == 'V' ? 0 : Branch() == 'S' ? 3 : 6) + Index;
            Text(EssencePreviews[PreviewIndex], Color, 0, Top + 208, 1.16f, true);
        }
    }
    Text(TEXT("Up / Down, mouse wheel or D-pad to choose"), FLinearColor(0.8f, 0.85f, 0.95f), 0, Top + 246, 1.17f, true);
    Text(TEXT("E / X / Square to confirm"), FLinearColor(0.8f, 0.85f, 0.95f), 0, Top + 270, 1.25f, true);
    Text(TEXT("Backspace / View / Create to close"), FLinearColor(0.8f, 0.85f, 0.95f), 0, Top + 296, 1.05f, true);
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
    case 7: if (bPressed) Component->BeginAim(); else Component->EndAim(); break;
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
    if (const auto* Instance = Character->GetGameInstance(); Instance && Flag(*Instance, TEXT("QuestAccepted")))
    {
        return TEXT("Contract active. Enter the Guild portal.");
    }
    const FString History = FSEssence::History(FSEssence::Profile(*Character));
    if (FSEssence::CanChoose(*Character))
    {
        return History.IsEmpty() ? TEXT("Choose your first essence at the Guild altar.") : TEXT("An essence choice is available at the Guild altar.");
    }
    return TEXT("Guild Hall. Speak to Gerald for a contract.");
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
