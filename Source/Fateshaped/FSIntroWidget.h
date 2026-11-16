#pragma once

#include "Blueprint/UserWidget.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "FSIntroWidget.generated.h"

class ACameraActor;

UCLASS()
class UFSIntroWidget final : public UUserWidget
{
    GENERATED_BODY()

public:
    static UFSIntroWidget* TryBegin(APlayerController& Player);
    static void CommitPrologue(APlayerController& Player);
    static void CommitIntroduction(APlayerController& Player);

    FSimpleDelegate OnPrologueComplete;
    FSimpleDelegate OnDialogueComplete;

    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeConstruct() override;
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

    UFUNCTION()
    void SkipClicked();

    UFUNCTION()
    void ContinueClicked();

private:
    enum class EPhase : uint8 { Prologue, Awakening, Dialogue, Finished };

    void ShowProloguePage();
    void ShowDialogueLine();
    void Advance();
    void Skip();
    void BeginAwakening();
    void BeginDialogue();
    void Finish(bool bDialogue);
    void FrameGerald();
    void SetLine(const FString& Text, float Alpha);
    void SetHint(const FString& Text);
    void SetEdgeButtonsVisible(bool bVisible);
    void FitTextFrame(const FGeometry& Geometry);

    UPROPERTY()
    TObjectPtr<UBorder> Backdrop;

    UPROPERTY()
    TObjectPtr<UTextBlock> Speaker;

    UPROPERTY()
    TObjectPtr<UTextBlock> Body;

    UPROPERTY()
    TObjectPtr<UTextBlock> Hint;

    UPROPERTY()
    TObjectPtr<UButton> SkipButton;

    UPROPERTY()
    TObjectPtr<UButton> ContinueButton;

    UPROPERTY()
    TObjectPtr<USizeBox> TextFrame;

    UPROPERTY()
    TObjectPtr<ACameraActor> ViewCamera;

    EPhase Phase = EPhase::Prologue;
    bool bResumeAtDialogue = false;
    int32 Index = 0;
    float Age = 0.0f;
    bool bInputLocked = false;
};
