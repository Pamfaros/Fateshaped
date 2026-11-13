#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "FSCryptEnemy.generated.h"

class UAnimSequence;
class UBehaviorTree;
class USkeletalMesh;
class UStaticMeshComponent;
class UTextRenderComponent;

UENUM()
enum class EFSCryptRole : uint8 { Skirmisher, Vanguard, Warden };

UENUM()
enum class EFSCryptBranch : uint8 { ResolveAction, ReturnHome, Defend, Attack, Position, Hold };

UCLASS(NotBlueprintable)
class AFSCryptEnemy : public ACharacter
{
    GENERATED_BODY()

public:
    AFSCryptEnemy();
    void Configure(EFSCryptRole InRole, const FVector& RoomCenter, const FVector2D& RoomHalfExtent, int32 Seed);
    bool TryBehavior(EFSCryptBranch Branch);
    int32 GetExperienceReward() const { return CombatRole == EFSCryptRole::Warden ? 60 : 20; }
    bool IsDefeated() const { return bDead; }

    virtual float TakeDamage(float Damage, const FDamageEvent& Event, AController* DamageInstigator, AActor* Causer) override;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

private:
    enum class EAction : uint8 { None, Cut, Sweep, Stomp, Lunge, Guard, Evade, Recover };

    void SetAnimation(UAnimSequence* Clip, bool bLoop, float Rate = 1.0f);
    void Face(const FVector& Point);
    void MoveAlong(const FVector& Direction, float Speed);
    void StartAttack(EAction Attack, const FVector& Target, double Now);
    void ShowTelegraph();
    void ResolveHit(ACharacter& Player);
    bool CanSee(const ACharacter& Player) const;
    bool IsInRoom(const FVector& Position, double Padding = 0.0) const;
    bool AnotherEnemyIsAttacking() const;
    void UpdateLabel(ACharacter* Player);

    UPROPERTY()
    TObjectPtr<UBehaviorTree> Tree;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> Telegraph;

    UPROPERTY()
    TObjectPtr<UStaticMesh> WarningDisc;

    UPROPERTY()
    TObjectPtr<UStaticMesh> WarningLane;

    UPROPERTY()
    TObjectPtr<UTextRenderComponent> Label;

    UPROPERTY()
    TArray<TObjectPtr<USkeletalMesh>> EnemyMeshes;

    UPROPERTY()
    TArray<TObjectPtr<UAnimSequence>> EnemyClips;

    UPROPERTY()
    TObjectPtr<UAnimSequence> Idle;

    UPROPERTY()
    TObjectPtr<UAnimSequence> Walk;

    UPROPERTY()
    TArray<TObjectPtr<UAnimSequence>> Attacks;

    UPROPERTY()
    TObjectPtr<UAnimSequence> React;

    UPROPERTY()
    TObjectPtr<UAnimSequence> Death;

    UPROPERTY()
    TObjectPtr<UAnimSequence> PlayingClip;

    UPROPERTY(VisibleInstanceOnly)
    EFSCryptRole CombatRole = EFSCryptRole::Skirmisher;

    UPROPERTY(VisibleInstanceOnly)
    float Health = 55.0f;

    UPROPERTY(VisibleInstanceOnly)
    bool bDead = false;

    FVector Home = FVector::ZeroVector;
    FVector BoundsCenter = FVector::ZeroVector;
    FVector2D BoundsHalfExtent = FVector2D::ZeroVector;
    FVector MoveDirection = FVector::ZeroVector;
    FVector StrikeOrigin = FVector::ZeroVector;
    FVector StrikeDirection = FVector::ForwardVector;
    FRandomStream Random;
    EAction Action = EAction::None;
    EAction PreviousAttack = EAction::None;
    double ActionEnd = 0.0;
    double NextAttack = 0.0;
    double NextDefense = 0.0;
    double LastHit = -10.0;
    double LastSight = -10.0;
    double OrbitUntil = 0.0;
    float MaxHealth = 55.0f;
    int32 OrbitSide = 1;
    int32 Pressure = 0;
    bool bCombo = false;
    bool bGuardNext = true;
    FString LastLabel;
};
