#include "FSCharacterActions.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/RootMotionSource.h"
#include "Kismet/GameplayStatics.h"

namespace
{
constexpr float DodgeSeconds = 0.22f;
constexpr double DodgeDistance = 340.0;
constexpr double WaveRange = 850.0;
constexpr double WaveHalfWidth = 170.0;
const FName PracticeTag(TEXT("PracticeTarget"));
const FName FacingTag(TEXT("FS_RestoreMovementFacing"));

FCollisionQueryParams WorldQuery(const AActor* Attacker)
{
    return FCollisionQueryParams(SCENE_QUERY_STAT(FSCharacterStrike), false, Attacker);
}

FCollisionObjectQueryParams StrikeObjects()
{
    // Use only the world channels so a front enemy cannot shield the next one.
    FCollisionObjectQueryParams Types(ECC_WorldStatic);
    Types.AddObjectTypesToQuery(ECC_WorldDynamic);
    return Types;
}

void SpawnEffect(UWorld& World, TSubclassOf<AActor> Effect, const FVector& Location, const FRotator& Rotation, const FVector& Scale)
{
    if (!Effect)
    {
        return;
    }
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    World.SpawnActor<AActor>(Effect, FTransform(Rotation, Location, Scale), Params);
}
}

FVector UFSCharacterActions::CameraHeading(const ACharacter* Character)
{
    return Character ? FRotator(0, Character->GetControlRotation().Yaw, 0).Vector() : FVector::ForwardVector;
}

void UFSCharacterActions::BeginAttackFacing(ACharacter* Character)
{
    if (!Character)
    {
        return;
    }
    UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    if (Movement->bOrientRotationToMovement)
    {
        Character->Tags.AddUnique(FacingTag);
        Movement->bOrientRotationToMovement = false;
    }
    Character->SetActorRotation(CameraHeading(Character).Rotation());
}

void UFSCharacterActions::EndAttackFacing(ACharacter* Character)
{
    if (Character && Character->Tags.Remove(FacingTag) != 0)
    {
        Character->GetCharacterMovement()->bOrientRotationToMovement = true;
    }
}

void UFSCharacterActions::PlayActionAnimation(ACharacter* Character, UAnimSequenceBase* Animation, float Rate)
{
    if (Character && Animation)
    {
        if (UAnimInstance* Instance = Character->GetMesh()->GetAnimInstance())
        {
            UAnimMontage* Montage = Instance->PlaySlotAnimationAsDynamicMontage(Animation, TEXT("DefaultSlot"), 0.05f, 0.12f, Rate);
            if (FAnimMontageInstance* Action = Montage ? Instance->GetActiveInstanceForMontage(Montage) : nullptr)
            {
                Action->PushDisableRootMotion();
            }
        }
    }
}

void UFSCharacterActions::BeginDodge(ACharacter* Character)
{
    if (!Character)
    {
        return;
    }
    UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    FVector Direction = Character->GetPendingMovementInputVector().GetSafeNormal2D();
    if (Direction.IsNearlyZero())
    {
        Direction = Movement->GetCurrentAcceleration().GetSafeNormal2D();
    }
    if (Direction.IsNearlyZero())
    {
        Direction = CameraHeading(Character);
    }
    Character->SetActorRotation(Direction.Rotation());

    // CharacterMovement sweeps the capsule and preserves gravity during the dash.
    TSharedPtr<FRootMotionSource_ConstantForce> Dash = MakeShared<FRootMotionSource_ConstantForce>();
    Dash->InstanceName = TEXT("FS_Dodge");
    Dash->Priority = 10;
    Dash->AccumulateMode = ERootMotionAccumulateMode::Override;
    Dash->Settings.SetFlag(ERootMotionSourceSettingsFlags::IgnoreZAccumulate);
    Dash->Duration = DodgeSeconds;
    Dash->Force = Direction * (DodgeDistance / DodgeSeconds);
    Dash->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::ClampVelocity;
    Dash->FinishVelocityParams.ClampVelocity = FMath::Min(Movement->MaxWalkSpeed, 200.0f);
    Movement->ApplyRootMotionSource(Dash);
}

bool UFSCharacterActions::HasClearStrike(const AActor* Attacker, const AActor* Target)
{
    if (!IsValid(Attacker) || !IsValid(Target))
    {
        return false;
    }
    FCollisionQueryParams Params = WorldQuery(Attacker);
    Params.AddIgnoredActor(Target);
    return !Attacker->GetWorld()->LineTraceTestByObjectType(
        Attacker->GetActorLocation(), Target->GetActorLocation(), StrikeObjects(), Params);
}

bool UFSCharacterActions::CanMeleeHit(const ACharacter* Character, const AActor* Target)
{
    if (!IsValid(Character) || !IsValid(Target))
    {
        return false;
    }
    const FVector Offset = Target->GetActorLocation() - Character->GetActorLocation();
    const FVector Forward = Character->GetActorForwardVector();
    return FVector::DotProduct(Offset, Forward) > 0.0
        && FVector::DistSquared(Offset, Forward * 105.0) <= FMath::Square(160.0)
        && HasClearStrike(Character, Target);
}

bool UFSCharacterActions::CastArcaneWave(ACharacter* Character, TSubclassOf<AActor> EnemyClass,
    TSubclassOf<AActor> WaveEffect, TSubclassOf<AActor> HitEffect, float Damage)
{
    if (!IsValid(Character))
    {
        return false;
    }
    UWorld& World = *Character->GetWorld();
    const FVector Origin = Character->GetActorLocation();
    const FVector CameraForward = CameraHeading(Character);
    const FCollisionObjectQueryParams Objects = StrikeObjects();
    const FCollisionQueryParams Params = WorldQuery(Character);
    FVector Forward = CameraForward;
    if (const APlayerController* PC = Cast<APlayerController>(Character->GetController()); PC && PC->PlayerCameraManager)
    {
        const FVector CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
        const double TraceDistance = FVector::Distance(CameraLocation, Origin) + WaveRange;
        FHitResult AimHit;
        if (World.LineTraceSingleByObjectType(AimHit, CameraLocation, CameraLocation + CameraForward * TraceDistance, Objects, Params))
        {
            const FVector TowardAim = (AimHit.ImpactPoint - Origin).GetSafeNormal2D();
            if (FVector::DotProduct(TowardAim, CameraForward) > 0.85)
            {
                Forward = TowardAim;
            }
        }
    }
    Character->SetActorRotation(Forward.Rotation());

    FHitResult Obstruction;
    const bool bObstructed = World.LineTraceSingleByObjectType(Obstruction, Origin, Origin + Forward * WaveRange, Objects, Params);
    const double Length = bObstructed ? FMath::Max(0.0, Obstruction.Distance - 5.0) : WaveRange;
    const FVector Center = Origin + Forward * (Length * 0.5) - FVector(0, 0, 60);
    if (Length > 10.0)
    {
        SpawnEffect(World, WaveEffect, Center, Forward.Rotation(), FVector(Length / 100.0, WaveHalfWidth / 50.0, 0.5));
    }

    const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
    bool bHitPractice = false;
    for (TActorIterator<AActor> Actor(&World); Actor; ++Actor)
    {
        const bool bPractice = Actor->ActorHasTag(PracticeTag) && !Actor->IsHidden();
        const bool bEnemy = EnemyClass && Actor->IsA(EnemyClass);
        if (!bPractice && !bEnemy)
        {
            continue;
        }
        const FVector Offset = Actor->GetActorLocation() - Origin;
        const double Along = FVector::DotProduct(Offset, Forward);
        if (Along <= 0.0 || Along > Length || FMath::Abs(FVector::DotProduct(Offset, Right)) > WaveHalfWidth
            || FMath::Abs(Offset.Z) > 175.0 || !HasClearStrike(Character, *Actor))
        {
            continue;
        }
        if (bEnemy)
        {
            UGameplayStatics::ApplyDamage(*Actor, Damage, Character->GetController(), Character, nullptr);
        }
        else
        {
            bHitPractice = true;
            SpawnEffect(World, HitEffect, Actor->GetActorLocation(), FRotator::ZeroRotator, FVector(1.2));
        }
    }
    return bHitPractice;
}
