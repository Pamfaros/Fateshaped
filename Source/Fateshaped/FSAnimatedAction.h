#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataAsset.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "FSAnimatedAction.generated.h"

class UAnimInstance;
class UAnimMontage;
class UAnimSequence;

USTRUCT(BlueprintType)
struct FFSActionAnimation
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UAnimMontage> Montage;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.01")) float PlayRate = 1;
};

/** Author action clips, play rates and notify phases in assets, independently of cooldowns. */
UCLASS(BlueprintType)
class UFSActionSet : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TMap<FName, FFSActionAnimation> Actions;
    UFUNCTION(BlueprintCallable, Category="Action|Authoring")
    static void LinkPhasesToAnimation(UAnimMontage* Montage);
    UFUNCTION(BlueprintCallable, Category="Action|Authoring")
    static bool ReplaceAnimation(UAnimMontage* Montage, UAnimSequence* Animation);
    /** Editor authoring only: assemble an empty montage from clips before adding notify tracks. */
    UFUNCTION(BlueprintCallable, Category="Action|Authoring")
    static bool SetAnimationSegments(UAnimMontage* Montage, const TArray<UAnimSequence*>& Animations, const TArray<float>& EndFractions);
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFSActionStarted, FName, Action);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FFSActionEnded, FName, Action, bool, bInterrupted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFSActionEvent, FName, Event);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FFSActionWindowChanged, FName, Phase, bool, bActive);

/** One montage instance owns one action. No duration, timeout or gameplay clock. */
UCLASS(ClassGroup=(Fateshaped), meta=(BlueprintSpawnableComponent))
class UFSAnimatedAction : public UActorComponent
{
    GENERATED_BODY()
public:
    UFSAnimatedAction();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Action") TObjectPtr<UFSActionSet> ActionSet;
    UPROPERTY(BlueprintAssignable) FFSActionStarted OnActionStarted;
    UPROPERTY(BlueprintAssignable) FFSActionEnded OnActionEnded;
    UPROPERTY(BlueprintAssignable) FFSActionEvent OnActionEvent;
    UPROPERTY(BlueprintAssignable) FFSActionWindowChanged OnWindowChanged;

    UFUNCTION(BlueprintCallable, Category="Action") bool PlayAction(FName Action);
    UFUNCTION(BlueprintCallable, Category="Action") void SetActionSet(UFSActionSet* NewSet);
    UFUNCTION(BlueprintCallable, Category="Action") bool PlayMontage(FName Action, UAnimMontage* Montage, float Rate = 1);
    UFUNCTION(BlueprintCallable, Category="Action") void CancelAction();
    UFUNCTION(BlueprintPure, Category="Action") bool IsActionActive() const { return bActive; }
    UFUNCTION(BlueprintPure, Category="Action") bool IsInPhase(FName Phase) const;
    UFUNCTION(BlueprintPure, Category="Action") UAnimMontage* GetActionMontage() const { return ActiveMontage; }
    UFUNCTION(BlueprintPure, Category="Action") FName GetActionName() const { return ActionName; }
    UFUNCTION(BlueprintPure, Category="Action") float GetPosition() const;
    UFUNCTION(BlueprintCallable, Category="Action") void SetActionPlayRate(float Rate);

    // Sample after the skeletal pose refresh. Retain swept windows even if a whole window fits in one frame.
    void SampleFrame();
    void ConsumeFrame();
    bool WindowOverlap(FName Phase, float& From, float& To) const;
    bool WindowProgress(FName Phase, float& From, float& To) const;
    bool TakeEvent(FName Event);
    bool HasCompleted() const { return bCompleted; }
    void ReceiveEvent(FName Event, UAnimSequenceBase* Animation, int32 InstanceID);
    void ReceiveWindow(FName Phase, bool bOpen, UAnimSequenceBase* Animation, int32 InstanceID);

protected:
    void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
    void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    void Ended(UAnimMontage* Montage, bool bInterrupted, uint32 Token);
    void BlendingOut(UAnimMontage* Montage, bool bInterrupted, uint32 Token);
    void Release(bool bInterrupted);
    bool Matches(UAnimSequenceBase* Animation, int32 InstanceID) const;
    UPROPERTY(Transient) TObjectPtr<UAnimMontage> ActiveMontage;
    TWeakObjectPtr<UAnimInstance> Instance;
    TSet<FName> Events;
    TMap<FName, int32> OpenWindows;
    FName ActionName;
    int32 MontageInstanceID = INDEX_NONE;
    uint32 Generation = 0;
    float PreviousPosition = 0, FramePosition = 0;
    bool bActive = false, bCompleted = false;
};

/** Named gameplay phase on a montage: Hit, Invulnerable, Travel, Guard, Combo, Cancel, Charged, MoveLock. */
UCLASS(meta=(DisplayName="FS Action Window"))
class UFSActionWindow : public UAnimNotifyState
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Action") FName Phase;
    void NotifyBegin(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation, float Duration, const FAnimNotifyEventReference& Ref) override;
    void NotifyEnd(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& Ref) override;
    FString GetNotifyName_Implementation() const override { return Phase.ToString(); }
};

/** One-shot release/impact cue; cancellation discards cues that have not committed. */
UCLASS(meta=(DisplayName="FS Action Event"))
class UFSActionCue : public UAnimNotify
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Action") FName Event;
    void Notify(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& Ref) override;
    FString GetNotifyName_Implementation() const override { return Event.ToString(); }
};
