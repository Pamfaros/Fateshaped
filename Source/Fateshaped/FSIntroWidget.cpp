#include "FSIntroWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Overlay.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Styling/CoreStyle.h"

namespace
{
struct FPage
{
    const TCHAR* Text;
    float Hold;
    bool bBlackGap;
};

const FPage Prologue[] = {
    {TEXT("Another day. Another sleepless night.\n\nLife had become an endless repetition of things that no longer mattered."), 4.6f, false},
    {TEXT("That night, something was different.\n\nThe shadows beneath your feet began to move."), 4.4f, false},
    {TEXT("The floor cracked open, revealing a darkness without end.\n\nYou stared into the abyss.\n\nAnd for a moment, it felt as though something stared back."), 5.6f, false},
    {TEXT("Then, you fell."), 2.4f, true},
    {TEXT("Darkness.\n\nA distant voice. The faint flutter of wings.\n\nAnd then... warmth."), 4.8f, false},
};

const TCHAR* Dialogue[] = {
    TEXT("Oh, you're awake! Wonderful. I found you lying in the street, and carrying you here was an absolute nightmare. So, naturally, you owe me."),
    TEXT("Luckily for you, the Guild is short on adventurers. And luckily for me, you seem perfectly capable of walking into danger."),
    TEXT("What are dungeons, you ask? Nobody really knows. Some say they're the dreams of dying gods. Others say they're what's left of them."),
    TEXT("Personally, I don't think it matters much. The important thing is that people pay very well for whatever comes out of them."),
    TEXT("But look at you! You haven't even started your journey. Before I send you anywhere, you'll need to visit the Essence Altar."),
    TEXT("Vitality, Stamina or Mana. Choose whichever calls to you. Just remember: once you've made your choice, there's no taking it back."),
    TEXT("Once you're done, come back to me. I'll give you your first contract and open the portal. Try not to die on your first day, would you?"),
};

const FLinearColor Violet(0.62f, 0.48f, 0.95f);
const FLinearColor Parchment(0.93f, 0.90f, 0.84f);

UObject* SaveObject(const UGameInstance& Instance)
{
    if (const FObjectProperty* Save = FindFProperty<FObjectProperty>(Instance.GetClass(), TEXT("Save")))
    {
        return Save->GetObjectPropertyValue_InContainer(&Instance);
    }
    return nullptr;
}

bool ReadFlag(const UObject* Object, const TCHAR* Name)
{
    const FBoolProperty* Property = Object ? FindFProperty<FBoolProperty>(Object->GetClass(), Name) : nullptr;
    return Property && Property->GetPropertyValue_InContainer(Object);
}

void WriteFlag(UObject* Object, const TCHAR* Name, bool bValue)
{
    if (FBoolProperty* Property = Object ? FindFProperty<FBoolProperty>(Object->GetClass(), Name) : nullptr)
    {
        Property->SetPropertyValue_InContainer(Object, bValue);
    }
}

FString ReadText(const UObject* Object, const TCHAR* Name)
{
    const FStrProperty* Property = Object ? FindFProperty<FStrProperty>(Object->GetClass(), Name) : nullptr;
    return Property ? Property->GetPropertyValue_InContainer(Object) : FString();
}

int32 ReadInt(const UObject* Object, const TCHAR* Name)
{
    const FIntProperty* Property = Object ? FindFProperty<FIntProperty>(Object->GetClass(), Name) : nullptr;
    return Property ? Property->GetPropertyValue_InContainer(Object) : 0;
}

bool SaveProfile(UGameInstance& Instance)
{
    if (UFunction* SaveNow = Instance.FindFunction(TEXT("SaveNow")))
    {
        Instance.ProcessEvent(SaveNow, nullptr);
        return !ReadFlag(&Instance, TEXT("SaveFailed")) && !ReadFlag(&Instance, TEXT("SaveBlocked"));
    }
    return false;
}

bool HasProgress(const UObject* Save, const UGameInstance& Instance)
{
    return ReadFlag(Save, TEXT("IntroCompleted"))
        || !ReadText(Save, TEXT("EssenceHistory")).IsEmpty()
        || ReadInt(Save, TEXT("XP")) > 0
        || ReadInt(Save, TEXT("CharLevel")) > 1
        || !ReadText(Save, TEXT("CompletedQuests")).IsEmpty()
        || ReadFlag(&Instance, TEXT("QuestAccepted"));
}
}

UFSIntroWidget* UFSIntroWidget::TryBegin(APlayerController& Player)
{
    if (UGameplayStatics::GetCurrentLevelName(&Player) != TEXT("L_Guild")) return nullptr;
    UGameInstance* Instance = Player.GetGameInstance();
    UObject* Save = Instance ? SaveObject(*Instance) : nullptr;
    if (!Instance || !Save) return nullptr;

    const bool bPrologueDone = ReadFlag(Save, TEXT("PrologueCompleted"));
    const bool bIntroDone = ReadFlag(Save, TEXT("IntroCompleted"));
    if (HasProgress(Save, *Instance))
    {
        bool bDirty = false;
        if (!bPrologueDone)
        {
            WriteFlag(Save, TEXT("PrologueCompleted"), true);
            bDirty = true;
        }
        if (!bIntroDone)
        {
            WriteFlag(Save, TEXT("IntroCompleted"), true);
            bDirty = true;
        }
        if (bDirty) SaveProfile(*Instance);
        return nullptr;
    }
    if (bIntroDone) return nullptr;

    UFSIntroWidget* Widget = CreateWidget<UFSIntroWidget>(&Player);
    if (!Widget) return nullptr;
    Widget->bResumeAtDialogue = bPrologueDone;
    Widget->AddToViewport(150);
    if (APawn* Pawn = Player.GetPawn())
    {
        Pawn->DisableInput(&Player);
        Widget->bInputLocked = true;
    }
    Player.SetShowMouseCursor(true);
    FInputModeUIOnly Input;
    Input.SetWidgetToFocus(Widget->TakeWidget());
    Input.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    Player.SetInputMode(Input);
    return Widget;
}

TSharedRef<SWidget> UFSIntroWidget::RebuildWidget()
{
    if (!WidgetTree->RootWidget)
    {
        Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Backdrop"));
        Backdrop->SetBrushColor(FLinearColor(0.012f, 0.008f, 0.02f, 1.0f));
        Backdrop->SetPadding(FMargin(0));
        Backdrop->SetHorizontalAlignment(HAlign_Fill);
        Backdrop->SetVerticalAlignment(VAlign_Fill);

        UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Lines"));
        Speaker = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Speaker"));
        Body = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Body"));
        Hint = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Hint"));
        Speaker->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 22));
        Speaker->SetColorAndOpacity(Violet);
        Speaker->SetJustification(ETextJustify::Center);
        Body->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 26));
        Body->SetColorAndOpacity(Parchment);
        Body->SetJustification(ETextJustify::Center);
        Body->SetAutoWrapText(true);
        Hint->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 14));
        Hint->SetColorAndOpacity(FLinearColor(Violet.R, Violet.G, Violet.B, 0.8f));
        Hint->SetJustification(ETextJustify::Center);

        auto Add = [Column](UWidget* Widget, float Top)
        {
            if (UVerticalBoxSlot* Slot = Column->AddChildToVerticalBox(Widget))
            {
                Slot->SetPadding(FMargin(0, Top, 0, 0));
                Slot->SetHorizontalAlignment(HAlign_Fill);
            }
        };
        Add(Speaker, 0);
        Add(Body, 18);
        Add(Hint, 36);

        TextFrame = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("TextFrame"));
        if (USizeBoxSlot* FrameSlot = Cast<USizeBoxSlot>(TextFrame->AddChild(Column)))
        {
            FrameSlot->SetHorizontalAlignment(HAlign_Fill);
            FrameSlot->SetVerticalAlignment(VAlign_Center);
        }

        auto MakeEdgeButton = [this](const TCHAR* Name, const TCHAR* Label, const FName Handler)
        {
            UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
            FButtonStyle Style = FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button");
            const FSlateColor Fill(FLinearColor(0.07f, 0.04f, 0.12f, 0.92f));
            const FSlateColor Hover(FLinearColor(0.16f, 0.10f, 0.28f, 0.96f));
            Style.Normal.TintColor = Fill;
            Style.Hovered.TintColor = Hover;
            Style.Pressed.TintColor = FSlateColor(Violet);
            Style.NormalPadding = FMargin(28, 16);
            Style.PressedPadding = FMargin(28, 17, 28, 15);
            Button->SetStyle(Style);
            UTextBlock* Caption = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("%sLabel"), Name));
            Caption->SetText(FText::FromString(Label));
            Caption->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 18));
            Caption->SetColorAndOpacity(Parchment);
            Button->AddChild(Caption);
            FScriptDelegate Clicked;
            Clicked.BindUFunction(this, Handler);
            Button->OnClicked.Add(Clicked);
            return Button;
        };
        SkipButton = MakeEdgeButton(TEXT("Skip"), TEXT("Skip"), GET_FUNCTION_NAME_CHECKED(UFSIntroWidget, SkipClicked));
        ContinueButton = MakeEdgeButton(TEXT("Continue"), TEXT("Continue"), GET_FUNCTION_NAME_CHECKED(UFSIntroWidget, ContinueClicked));

        UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Overlay"));
        if (UOverlaySlot* TextSlot = Overlay->AddChildToOverlay(TextFrame))
        {
            TextSlot->SetHorizontalAlignment(HAlign_Center);
            TextSlot->SetVerticalAlignment(VAlign_Center);
        }
        if (UOverlaySlot* SkipSlot = Overlay->AddChildToOverlay(SkipButton))
        {
            SkipSlot->SetHorizontalAlignment(HAlign_Left);
            SkipSlot->SetVerticalAlignment(VAlign_Center);
            SkipSlot->SetPadding(FMargin(28, 0, 0, 0));
        }
        if (UOverlaySlot* ContinueSlot = Overlay->AddChildToOverlay(ContinueButton))
        {
            ContinueSlot->SetHorizontalAlignment(HAlign_Right);
            ContinueSlot->SetVerticalAlignment(VAlign_Center);
            ContinueSlot->SetPadding(FMargin(0, 0, 28, 0));
        }
        Backdrop->AddChild(Overlay);
        WidgetTree->RootWidget = Backdrop;
    }
    return Super::RebuildWidget();
}

void UFSIntroWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetIsFocusable(true);
    if (bResumeAtDialogue) BeginAwakening();
    else ShowProloguePage();
}

void UFSIntroWidget::SkipClicked()
{
    Skip();
}

void UFSIntroWidget::ContinueClicked()
{
    if (Phase != EPhase::Awakening) Advance();
}

void UFSIntroWidget::SetEdgeButtonsVisible(bool bVisible)
{
    const ESlateVisibility ButtonVisibility = bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
    if (SkipButton) SkipButton->SetVisibility(ButtonVisibility);
    if (ContinueButton) ContinueButton->SetVisibility(ButtonVisibility);
}

void UFSIntroWidget::FitTextFrame(const FGeometry& Geometry)
{
    if (!TextFrame) return;
    const FVector2D Size = Geometry.GetLocalSize();
    if (Size.X < 1.0f || Size.Y < 1.0f) return;
    TextFrame->SetWidthOverride(Size.X * 0.7f);
    TextFrame->SetHeightOverride(Size.Y * 0.7f);
}

void UFSIntroWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    FitTextFrame(MyGeometry);
    if (!Backdrop.Get() || !Body.Get() || Phase == EPhase::Finished) return;
    Age += InDeltaTime;
    if (Phase == EPhase::Awakening)
    {
        const float Alpha = 1.0f - FMath::Clamp((Age - 0.15f) / 1.7f, 0.0f, 1.0f);
        Backdrop->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, Alpha));
        if (Age >= 2.05f) BeginDialogue();
        return;
    }
    if (Phase == EPhase::Finished) return;

    const bool bDialogue = Phase == EPhase::Dialogue;
    const float Hold = bDialogue ? 3.5f : Prologue[Index].Hold;
    const float Fade = 0.45f;
    float Opacity = 1.0f;
    if (Age < Fade) Opacity = Age / Fade;
    else if (Age > Fade + Hold) Opacity = 1.0f - FMath::Clamp((Age - Fade - Hold) / Fade, 0.0f, 1.0f);
    SetLine(bDialogue ? Dialogue[Index] : Prologue[Index].Text, Opacity);
    if (!bDialogue && Prologue[Index].bBlackGap && Age > Fade + Hold + Fade)
    {
        Backdrop->SetBrushColor(FLinearColor::Black);
        if (Age < Fade + Hold + Fade + 1.35f) return;
    }
    if (Age >= Fade + Hold + Fade + (( !bDialogue && Prologue[Index].bBlackGap) ? 1.35f : 0.0f))
    {
        Advance();
    }
}

FReply UFSIntroWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && Phase != EPhase::Awakening)
    {
        Advance();
    }
    return FReply::Handled();
}

FReply UFSIntroWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    const FKey Key = InKeyEvent.GetKey();
    if (Key == EKeys::Escape || Key == EKeys::Gamepad_Special_Right)
    {
        Skip();
        return FReply::Handled();
    }
    if (Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom)
    {
        if (Phase != EPhase::Awakening) Advance();
        return FReply::Handled();
    }
    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UFSIntroWidget::ShowProloguePage()
{
    Age = 0.0f;
    Phase = EPhase::Prologue;
    Speaker->SetText(FText::GetEmpty());
    Backdrop->SetBrushColor(FLinearColor(0.012f, 0.008f, 0.02f, 1.0f));
    SetHint(TEXT("Enter or A continues     Esc skips"));
    SetEdgeButtonsVisible(true);
    SetLine(Prologue[Index].Text, 0.0f);
}

void UFSIntroWidget::ShowDialogueLine()
{
    Age = 0.0f;
    Phase = EPhase::Dialogue;
    Backdrop->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.28f));
    Speaker->SetText(FText::FromString(TEXT("Gerald")));
    SetHint(TEXT("Enter or A continues     Esc skips the introduction"));
    SetEdgeButtonsVisible(true);
    SetLine(Dialogue[Index], 0.0f);
}

void UFSIntroWidget::SetLine(const FString& Text, float Alpha)
{
    Body->SetText(FText::FromString(Text));
    Body->SetColorAndOpacity(FLinearColor(Parchment.R, Parchment.G, Parchment.B, Alpha));
}

void UFSIntroWidget::SetHint(const FString& Text)
{
    Hint->SetText(FText::FromString(Text));
}

void UFSIntroWidget::Advance()
{
    if (Phase == EPhase::Prologue)
    {
        if (Index + 1 < UE_ARRAY_COUNT(Prologue))
        {
            ++Index;
            ShowProloguePage();
            return;
        }
        OnPrologueComplete.ExecuteIfBound();
        BeginAwakening();
        return;
    }
    if (Phase == EPhase::Dialogue)
    {
        if (Index + 1 < UE_ARRAY_COUNT(Dialogue))
        {
            ++Index;
            ShowDialogueLine();
            return;
        }
        Finish(true);
    }
}

void UFSIntroWidget::Skip()
{
    if (Phase == EPhase::Finished) return;
    if (Phase == EPhase::Awakening)
    {
        BeginDialogue();
        return;
    }
    if (Phase == EPhase::Dialogue)
    {
        Finish(true);
        return;
    }
    OnPrologueComplete.ExecuteIfBound();
    BeginAwakening();
}

void UFSIntroWidget::CommitPrologue(APlayerController& Player)
{
    UGameInstance* Instance = Player.GetGameInstance();
    UObject* Save = Instance ? SaveObject(*Instance) : nullptr;
    if (!Save || ReadFlag(Save, TEXT("PrologueCompleted"))) return;
    WriteFlag(Save, TEXT("PrologueCompleted"), true);
    if (!SaveProfile(*Instance)) WriteFlag(Save, TEXT("PrologueCompleted"), false);
}

void UFSIntroWidget::CommitIntroduction(APlayerController& Player)
{
    UGameInstance* Instance = Player.GetGameInstance();
    UObject* Save = Instance ? SaveObject(*Instance) : nullptr;
    if (!Save || ReadFlag(Save, TEXT("IntroCompleted"))) return;
    WriteFlag(Save, TEXT("IntroCompleted"), true);
    if (!SaveProfile(*Instance))
    {
        WriteFlag(Save, TEXT("IntroCompleted"), false);
        return;
    }
    if (APawn* Pawn = Player.GetPawn())
    {
        if (FStrProperty* Message = FindFProperty<FStrProperty>(Pawn->GetClass(), TEXT("Message")))
        {
            Message->SetPropertyValue_InContainer(Pawn, TEXT("Visit the Essence Altar and choose your first essence."));
        }
    }
}

void UFSIntroWidget::BeginAwakening()
{
    Phase = EPhase::Awakening;
    Age = 0.0f;
    Index = 0;
    Speaker->SetText(FText::GetEmpty());
    Body->SetText(FText::GetEmpty());
    Hint->SetText(FText::GetEmpty());
    SetEdgeButtonsVisible(false);
    Backdrop->SetBrushColor(FLinearColor::Black);
    FrameGerald();
}

void UFSIntroWidget::BeginDialogue()
{
    if (Phase == EPhase::Dialogue) return;
    Index = 0;
    ShowDialogueLine();
}

void UFSIntroWidget::Finish(bool bDialogue)
{
    if (Phase == EPhase::Finished) return;
    Phase = EPhase::Finished;
    if (APlayerController* Player = GetOwningPlayer())
    {
        if (APawn* Pawn = Player->GetPawn())
        {
            if (bInputLocked) Pawn->EnableInput(Player);
            Player->SetViewTarget(Pawn);
        }
    }
    bInputLocked = false;
    if (ViewCamera)
    {
        ViewCamera->Destroy();
        ViewCamera = nullptr;
    }
    if (bDialogue) OnDialogueComplete.ExecuteIfBound();
    RemoveFromParent();
}

void UFSIntroWidget::FrameGerald()
{
    APlayerController* Player = GetOwningPlayer();
    if (!Player || !Player->GetPawn()) return;
    AActor* Gerald = nullptr;
    for (TActorIterator<AActor> Actor(GetWorld()); Actor; ++Actor)
    {
        if (Actor->ActorHasTag(TEXT("Fairy")))
        {
            Gerald = *Actor;
            break;
        }
    }
    if (!Gerald) return;
    const FVector Focus = Gerald->GetActorLocation() + FVector(0, 0, 50);
    const FVector Approach = (Focus - Player->GetPawn()->GetActorLocation()).GetSafeNormal2D();
    const FVector Forward = Approach.IsNearlyZero() ? FVector(1, 0, 0) : Approach;
    const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
    const FVector CameraLocation = Player->GetPawn()->GetActorLocation() - Forward * 230.0 + Right * 80.0 + FVector(0, 0, 90);
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ViewCamera = GetWorld()->SpawnActor<ACameraActor>(CameraLocation, (Focus - CameraLocation).Rotation(), Params);
    if (!ViewCamera) return;
    if (UCameraComponent* Component = ViewCamera->GetCameraComponent())
    {
        Component->SetFieldOfView(58.0f);
        Component->bConstrainAspectRatio = false;
    }
    Player->SetViewTarget(ViewCamera);
}
