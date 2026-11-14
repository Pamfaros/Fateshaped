#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FSClassProfile.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FSClassCombat.generated.h"

class ACharacter;
class AHUD;
class APlayerController;
class UInputComponent;
class USpringArmComponent;

UCLASS()
class UFSClassCombat : public UActorComponent
{
    GENERATED_BODY()

public:
    UFSClassCombat();
    void Restore(bool bEvolution);
    void Attack();
    void Evade();
    void SkillOne();
    void SkillTwo();
    bool InteractAtAltar();
    float GuardDamage(float Damage, AActor* Causer);
    void Draw(AHUD& HUD) const;

protected:
    void BeginPlay() override;
    void EndPlay(const EEndPlayReason::Type Reason) override;
    void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;

private:
    friend class UFSClassBridge;
    using EKit = FSEssence::EKit;
    enum class EMove : uint8
    {
        Idle, Axe, Sword, Dodge, Leap, ShieldHold, ShieldStep, Spark, OrbHold, Orb, StoneHold, Stone,
        Pivot, Thread, Recall, PaidStand, Slip, Wave, Seal, BendHold, Bend
    };

    struct FShot
    {
        FVector Position;
        FVector Direction;
        float Radius;
        float Speed;
        float Range;
        float Damage;
        float TurnDegrees = 0;
        float HalfWidth = 0;
        float HalfHeight = 0;
        int32 TargetLimit = 1;
        bool bThread = false;
        bool bCheckEmission = true;
        TSet<TWeakObjectPtr<AActor>> Hit;
        TWeakObjectPtr<AActor> Visual;
    };

    ACharacter& Player() const;
    bool Unavailable() const;
    bool Grounded() const;
    bool IsGuild() const;
    char Branch() const;
    float Value(const TCHAR* Field) const;
    void SetValue(const TCHAR* Field, float Amount);
    void SetFlag(const TCHAR* Field, bool bValue);
    void Message(const FString& Text) const;
    bool Ready(int32 Slot) const;
    bool CanPay(float V, float S, float M) const;
    bool Spend(float V, float S, float M);
    void Start(EMove Next, float End, const TCHAR* Clip = nullptr);
    void Finish();
    void Cancel();
    void TickAction(float DeltaTime);
    void TickShots(float DeltaTime);
    void ClearEffects();
    void ReleaseSkillOne();
    void ReleaseSkillTwo();
    void BeginAim();
    void EndAim();
    void Interaction();
    void PreviousOption();
    void NextOption();
    void CloseSelection();
    void QueueAttack();
    void QueueEvade();
    void QueueSkillOne();
    void QueueSkillTwo();
    void QueueFirstRelease();
    void QueueSecondRelease();
    void ConsumeInput();
    void JumpPressed();
    void JumpReleased();
    void SelectEssence(TCHAR Essence);
    void SetupInput(APlayerController& Controller);
    void UpdateCamera(float DeltaTime);
    FVector AimDirection(bool bVertical = false) const;
    FVector MoveInput(FVector Fallback) const;
    FVector Hand() const;
    void WeaponEdge(FVector& Start, FVector& End) const;
    bool HasTravelFloor(const FVector& Target) const;
    bool MovePlayer(const FVector& Offset, FHitResult& Hit);
    void Travel(const FVector& Direction, float Distance, float Duration, float Arc = 0);
    void BladeContact(float From, float To, float Radius, float Damage, int32 Limit);
    void LeapContact(const FVector& Start, const FVector& Desired, float FromAge, float ToAge);
    void PlayLeapStrike();
    float HitTarget(AActor& Target, float Damage);
    void Fire(FShot Shot);
    void PlaySound(const TCHAR* Name) const;
    void EquipClassVisual();
    void SetWard(const FVector& Ground);
    bool WardGround(FVector& Ground) const;
    bool FindWardFloor(const FVector& Point, FVector& Ground) const;
    bool InsideWard() const;
    void PulseWard();

    UPROPERTY(Transient)
    TObjectPtr<UInputComponent> Input;
    UPROPERTY(Transient)
    TObjectPtr<class UStaticMeshComponent> ClassWeapon;
    UPROPERTY(Transient)
    TObjectPtr<class UStaticMeshComponent> ClassShield;
    UPROPERTY(Transient)
    TObjectPtr<AActor> Altar;

    const FSEssence::FClass* Class = nullptr;
    TWeakObjectPtr<APlayerController> InputController;
    TWeakObjectPtr<USpringArmComponent> CameraArm;
    TWeakObjectPtr<AActor> LeapContactTarget;
    TArray<FShot> Shots;
    TSet<TWeakObjectPtr<AActor>> HitActors;
    TWeakObjectPtr<AActor> WardVisual;
    TWeakObjectPtr<AActor> AnchorVisual;
    TWeakObjectPtr<class UTextRenderComponent> AltarLabel;
    EMove Move = EMove::Idle;
    double Cooldown[4] = {};
    double LastStaminaSpend = -10;
    double AnchorUntil = 0;
    double WardUntil = 0;
    double WardPulseUntil = 0;
    double BufferedBasicUntil = 0;
    float Age = 0;
    float PreviousAge = 0;
    float EndTime = 0;
    float MoveDistance = 0;
    float MoveTime = 0;
    float MoveAge = 0;
    float MoveArc = 0;
    float WardCapacity = 0;
    float OriginalWalkSpeed = 0;
    float PreLeapFlySpeed = 0;
    float OriginalArm = 0;
    FVector OriginalSocketOffset = FVector::ZeroVector;
    float CameraAimBlend = 0;
    int32 SelectionIndex = 0;
    int32 CurveSide = 0;
    bool bSelecting = false;
    bool bAim = false;
    bool bAimFacing = false;
    bool bLeapStrikeStarted = false;
    bool bCommitted = false;
    bool bFull = false;
    bool bStepped = false;
    bool bWasInsideWard = false;
    bool bHadTurnCue = false;
    bool bJumpForwarded = false;
    uint8 RequestedInputs = 0;
    FVector Heading = FVector::ForwardVector;
    FVector TravelHeading = FVector::ZeroVector;
    FVector StrikePoint = FVector::ZeroVector;
    FVector LastBladeStart = FVector::ZeroVector;
    FVector LastBladeEnd = FVector::ZeroVector;
    bool bHaveBladeSample = false;
    FVector Anchor = FVector::ZeroVector;
    FVector WardCenter = FVector::ZeroVector;
    FVector PendingWardCenter = FVector::ZeroVector;
    FVector BendOffset = FVector::ZeroVector;
};

UCLASS()
class UFSClassBridge : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="Fateshaped|Essence", meta=(DefaultToSelf="Character"))
    static void RestoreClass(ACharacter* Character);
    UFUNCTION(BlueprintCallable, Category="Fateshaped|Essence")
    static void ClassInput(ACharacter* Character, int32 Action, bool bPressed);
    UFUNCTION(BlueprintCallable, Category="Fateshaped|Essence", meta=(DevelopmentOnly))
    static bool SetEditorEssenceFixture(ACharacter* Character, const FString& Path, int32 Level, int32 Pending, int32 XP);
    UFUNCTION(BlueprintCallable, Category="Fateshaped|Essence")
    static void ClassAttack(ACharacter* Character);
    UFUNCTION(BlueprintCallable, Category="Fateshaped|Essence")
    static void ClassEvade(ACharacter* Character);
    UFUNCTION(BlueprintCallable, Category="Fateshaped|Essence")
    static void ClassSkillTwo(ACharacter* Character);
    UFUNCTION(BlueprintCallable, Category="Fateshaped|Essence")
    static bool UseEssenceAltar(ACharacter* Character);
    UFUNCTION(BlueprintPure, Category="Fateshaped|Essence")
    static FString GuildClassPrompt(ACharacter* Character);
    UFUNCTION(BlueprintCallable, Category="Fateshaped|Essence")
    static float FilterClassDamage(ACharacter* Character, float Damage, AActor* Causer);
    UFUNCTION(BlueprintCallable, Category="Fateshaped|Essence")
    static void DrawClassHUD(ACharacter* Character, AHUD* HUD);
};
