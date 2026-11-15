#include "FSAnimatedAction.h"

#include "Animation/ActiveMontageInstanceScope.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"

void UFSActionSet::LinkPhasesToAnimation(UAnimMontage* Montage)
{
#if WITH_EDITOR
    if (!Montage) return;
    Montage->Modify();
    for (FAnimNotifyEvent& Notify : Montage->Notifies)
    {
        const float Start = Notify.GetTime(), End = Start + Notify.GetDuration();
        Notify.Link(Montage, Start);
        Notify.ChangeLinkMethod(EAnimLinkMethod::Proportional);
        if (Notify.NotifyStateClass)
        {
            Notify.EndLink.Link(Montage, End);
            Notify.EndLink.ChangeLinkMethod(EAnimLinkMethod::Proportional);
        }
    }
    Montage->MarkPackageDirty();
#endif
}

bool UFSActionSet::ReplaceAnimation(UAnimMontage* Montage, UAnimSequence* Animation)
{
#if WITH_EDITOR
    if (!Montage || !Animation || Montage->SlotAnimTracks.Num() != 1
        || Montage->SlotAnimTracks[0].AnimTrack.AnimSegments.Num() != 1
        || !Montage->GetSkeleton() || !Montage->GetSkeleton()->IsCompatibleForEditor(Animation->GetSkeleton())) return false;
    LinkPhasesToAnimation(Montage);
    Montage->SlotAnimTracks[0].AnimTrack.AnimSegments[0].SetAnimReference(Animation, true);
    // Segment length also includes the sequence's RateScale; GetPlayLength alone leaves a frozen tail.
    Montage->SetCompositeLength(Montage->CalculateSequenceLength());
    Montage->UpdateLinkableElements();
    Montage->RefreshCacheData();
    Montage->MarkPackageDirty();
    return true;
#else
    return false;
#endif
}

bool UFSActionSet::SetAnimationSegments(UAnimMontage* Montage, const TArray<UAnimSequence*>& Animations, const TArray<float>& EndFractions)
{
#if WITH_EDITOR
    if (!Montage || Montage->SlotAnimTracks.Num() != 1 || !Montage->Notifies.IsEmpty()
        || Animations.IsEmpty() || Animations.Num() != EndFractions.Num())
    {
        UE_LOG(LogTemp, Error, TEXT("FS montage authoring requires one slot and no notifies (notifies=%d)"), Montage ? Montage->Notifies.Num() : -1);
        return false;
    }
    for (int32 Index = 0; Index < Animations.Num(); ++Index)
    {
        if (!Animations[Index] || !Montage->GetSkeleton() || !Montage->GetSkeleton()->IsCompatibleForEditor(Animations[Index]->GetSkeleton())
            || !FMath::IsFinite(EndFractions[Index]) || EndFractions[Index] <= 0 || EndFractions[Index] > 1)
        {
            UE_LOG(LogTemp, Error, TEXT("FS montage authoring: incompatible animation %s"), *GetNameSafe(Animations[Index]));
            return false;
        }
    }
    Montage->Modify();
    auto& Segments = Montage->SlotAnimTracks[0].AnimTrack.AnimSegments;
    Segments.Reset();
    float Position = 0;
    for (int32 Index = 0; Index < Animations.Num(); ++Index)
    {
        FAnimSegment Segment;
        Segment.SetAnimReference(Animations[Index], true);
        Segment.AnimEndTime *= EndFractions[Index];
        Segment.StartPos = Position;
        Position += Segment.GetLength();
        Segments.Add(Segment);
    }
    Montage->SetCompositeLength(Position);
    Montage->RefreshCacheData();
    Montage->MarkPackageDirty();
    return true;
#else
    return false;
#endif
}

UFSAnimatedAction::UFSAnimatedAction()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UFSAnimatedAction::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
    Super::TickComponent(DeltaTime, TickType, TickFunction);
    if (!bActive) return;
    auto* Character = Cast<ACharacter>(GetOwner());
    if (!Instance.IsValid() || !Character || Character->GetMesh()->GetAnimInstance() != Instance.Get()
        || !Instance->GetMontageInstanceForID(MontageInstanceID)) Release(true);
}

bool UFSAnimatedAction::PlayAction(FName Action)
{
    const FFSActionAnimation* Definition = ActionSet ? ActionSet->Actions.Find(Action) : nullptr;
    return PlayMontage(Action, Definition ? Definition->Montage.Get() : nullptr, Definition ? Definition->PlayRate : 1);
}

void UFSAnimatedAction::SetActionSet(UFSActionSet* NewSet)
{
    CancelAction();
    ActionSet = NewSet;
}

bool UFSAnimatedAction::PlayMontage(FName Action, UAnimMontage* Montage, float Rate)
{
    CancelAction();
    auto* Character = Cast<ACharacter>(GetOwner());
    UAnimInstance* Anim = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
    if (!Anim || !Montage || !FMath::IsFinite(Rate) || Rate <= 0 || Montage->GetPlayLength() <= 0)
    {
        UE_LOG(LogTemp, Error, TEXT("FS action '%s': missing/invalid montage, rate or AnimInstance"), *Action.ToString());
        return false;
    }
    if (Anim->Montage_Play(Montage, Rate) <= 0) return false;
    FAnimMontageInstance* Playing = Anim->GetActiveInstanceForMontage(Montage);
    if (!Playing) return false;
    ActiveMontage = Montage;
    Instance = Anim;
    MontageInstanceID = Playing->GetInstanceID();
    ActionName = Action;
    PreviousPosition = FramePosition = 0;
    bCompleted = false;
    bActive = true;
    const uint32 Token = ++Generation;
    // Gameplay movement sweeps the capsule using the authored Travel phase/root curve.
    Playing->PushDisableRootMotion();
    FOnMontageEnded End;
    End.BindUObject(this, &UFSAnimatedAction::Ended, Token);
    Anim->Montage_SetEndDelegate(End, Montage);
    FOnMontageBlendingOutStarted Blend;
    Blend.BindUObject(this, &UFSAnimatedAction::BlendingOut, Token);
    Anim->Montage_SetBlendingOutDelegate(Blend, Montage);
    OnActionStarted.Broadcast(Action);
    return true;
}

void UFSAnimatedAction::CancelAction()
{
    UAnimMontage* OldMontage = ActiveMontage;
    UAnimInstance* OldInstance = Instance.Get();
    const bool bWasActive = bActive;
    const FName OldAction = ActionName;
    const TMap<FName, int32> Windows = MoveTemp(OpenWindows);
    OpenWindows.Reset();
    bActive = false;
    ++Generation; // Late delegates and notify-end calls from an old instance cannot release its successor.
    ActiveMontage = nullptr;
    Instance.Reset();
    Events.Reset();
    bCompleted = false;
    MontageInstanceID = INDEX_NONE;
    if (OldInstance && OldMontage) OldInstance->Montage_Stop(0, OldMontage);
    // Clear the old playback before broadcasting, so event handlers can safely start a successor.
    if (bWasActive)
    {
        for (const auto& Window : Windows) OnWindowChanged.Broadcast(Window.Key, false);
        OnActionEnded.Broadcast(OldAction, true);
    }
}

void UFSAnimatedAction::Release(bool bInterrupted)
{
    if (!bActive) return;
    bActive = false;
    bCompleted = !bInterrupted;
    if (bInterrupted) Events.Reset();
    const TMap<FName, int32> Windows = MoveTemp(OpenWindows);
    OpenWindows.Reset();
    const FName EndedAction = ActionName;
    for (const auto& Window : Windows) OnWindowChanged.Broadcast(Window.Key, false);
    OnActionEnded.Broadcast(EndedAction, bInterrupted);
}

void UFSAnimatedAction::Ended(UAnimMontage* Montage, bool bInterrupted, uint32 Token)
{
    if (Token != Generation || Montage != ActiveMontage) return;
    if (!bInterrupted) FramePosition = Montage->GetPlayLength();
    Release(bInterrupted);
}

void UFSAnimatedAction::BlendingOut(UAnimMontage* Montage, bool bInterrupted, uint32 Token)
{
    // Natural blend-out may start before the final pose: only interruption releases here.
    if (bInterrupted && Token == Generation && Montage == ActiveMontage) Release(true);
}

float UFSAnimatedAction::GetPosition() const
{
    return bActive && Instance.IsValid() ? Instance->Montage_GetPosition(ActiveMontage) : FramePosition;
}

void UFSAnimatedAction::SetActionPlayRate(float Rate)
{
    if (bActive && Instance.IsValid() && FMath::IsFinite(Rate) && Rate > 0) Instance->Montage_SetPlayRate(ActiveMontage, Rate);
}

void UFSAnimatedAction::SampleFrame()
{
    if (bActive)
    {
        auto* Character = Cast<ACharacter>(GetOwner());
        if (!Instance.IsValid() || !Character || Character->GetMesh()->GetAnimInstance() != Instance.Get()
            || !Instance->GetMontageInstanceForID(MontageInstanceID))
        {
            Release(true); // Mesh/AnimInstance replacement must also unlock the character.
            return;
        }
        FramePosition = GetPosition();
    }
}

void UFSAnimatedAction::ConsumeFrame() { PreviousPosition = FramePosition; Events.Reset(); }

bool UFSAnimatedAction::IsInPhase(FName Phase) const
{
    if (!bActive || !ActiveMontage) return false;
    const float Position = GetPosition();
    for (const FAnimNotifyEvent& Notify : ActiveMontage->Notifies)
    {
        const auto* Window = Cast<UFSActionWindow>(Notify.NotifyStateClass);
        if (Window && Window->Phase == Phase && Position >= Notify.GetTime() && Position < Notify.GetEndTriggerTime()) return true;
    }
    return false;
}

bool UFSAnimatedAction::WindowOverlap(FName Phase, float& From, float& To) const
{
    if (!ActiveMontage || (!bActive && !bCompleted) || FramePosition <= PreviousPosition) return false;
    for (const FAnimNotifyEvent& Notify : ActiveMontage->Notifies)
    {
        const auto* Window = Cast<UFSActionWindow>(Notify.NotifyStateClass);
        if (!Window || Window->Phase != Phase) continue;
        const float Begin = FMath::Max(PreviousPosition, Notify.GetTime());
        const float End = FMath::Min(FramePosition, Notify.GetEndTriggerTime());
        if (End <= Begin) continue;
        From = (Begin - PreviousPosition) / (FramePosition - PreviousPosition);
        To = (End - PreviousPosition) / (FramePosition - PreviousPosition);
        return true;
    }
    return false;
}

bool UFSAnimatedAction::WindowProgress(FName Phase, float& From, float& To) const
{
    if (!ActiveMontage || (!bActive && !bCompleted)) return false;
    for (const FAnimNotifyEvent& Notify : ActiveMontage->Notifies)
    {
        const auto* Window = Cast<UFSActionWindow>(Notify.NotifyStateClass);
        if (!Window || Window->Phase != Phase) continue;
        const float Duration = Notify.GetDuration();
        if (Duration <= 0) continue;
        From = FMath::Clamp((PreviousPosition - Notify.GetTime()) / Duration, 0.0f, 1.0f);
        To = FMath::Clamp((FramePosition - Notify.GetTime()) / Duration, 0.0f, 1.0f);
        return To > From;
    }
    return false;
}

bool UFSAnimatedAction::TakeEvent(FName Event) { return Events.Remove(Event) != 0; }

bool UFSAnimatedAction::Matches(UAnimSequenceBase* Animation, int32 ID) const
{
    return bActive && ActiveMontage == Animation && ID == MontageInstanceID;
}

void UFSAnimatedAction::ReceiveEvent(FName Event, UAnimSequenceBase* Animation, int32 ID)
{
    if (!Matches(Animation, ID)) return;
    Events.Add(Event);
    OnActionEvent.Broadcast(Event);
}

void UFSAnimatedAction::ReceiveWindow(FName Phase, bool bOpen, UAnimSequenceBase* Animation, int32 ID)
{
    if (!Matches(Animation, ID)) return;
    int32& Count = OpenWindows.FindOrAdd(Phase);
    const bool bWasOpen = Count > 0;
    Count = FMath::Max(0, Count + (bOpen ? 1 : -1));
    if (bWasOpen != (Count > 0)) OnWindowChanged.Broadcast(Phase, Count > 0);
}

void UFSAnimatedAction::EndPlay(const EEndPlayReason::Type Reason)
{
    CancelAction();
    Super::EndPlay(Reason);
}

namespace
{
UFSAnimatedAction* Driver(USkeletalMeshComponent* Mesh)
{
    return Mesh && Mesh->GetOwner() ? Mesh->GetOwner()->FindComponentByClass<UFSAnimatedAction>() : nullptr;
}
int32 NotifyInstance(const FAnimNotifyEventReference& Ref)
{
    const auto* Context = Ref.GetContextData<UE::Anim::FAnimNotifyMontageInstanceContext>();
    return Context ? Context->MontageInstanceID : INDEX_NONE;
}
}

void UFSActionWindow::NotifyBegin(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation, float, const FAnimNotifyEventReference& Ref)
{
    if (auto* Action = Driver(Mesh)) Action->ReceiveWindow(Phase, true, Animation, NotifyInstance(Ref));
}
void UFSActionWindow::NotifyEnd(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& Ref)
{
    if (auto* Action = Driver(Mesh)) Action->ReceiveWindow(Phase, false, Animation, NotifyInstance(Ref));
}
void UFSActionCue::Notify(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& Ref)
{
    if (auto* Action = Driver(Mesh)) Action->ReceiveEvent(Event, Animation, NotifyInstance(Ref));
}
