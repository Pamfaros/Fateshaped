#include "FSGameViewportClient.h"

#include "AudioDevice.h"
#include "Engine/Console.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerController.h"
#include "InputKeyEventArgs.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/ConfigCacheIni.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "FateshapedPauseMenu"

namespace
{
constexpr TCHAR AudioSection[] = TEXT("Fateshaped.Audio");
constexpr TCHAR MasterVolumeKey[] = TEXT("MasterVolume");
constexpr TCHAR InputSection[] = TEXT("Fateshaped.Input");
constexpr TCHAR VibrationKey[] = TEXT("VibrationIntensity");
const FLinearColor Gold(0.91f, 0.69f, 0.35f);
const FLinearColor Parchment(0.90f, 0.86f, 0.76f);
const FLinearColor Panel(0.035f, 0.041f, 0.046f);

FText GetQualityLabel(int32 Quality)
{
    switch (Quality)
    {
    case 0: return LOCTEXT("Low", "Low");
    case 1: return LOCTEXT("Medium", "Medium");
    case 2: return LOCTEXT("High", "High");
    case 3: return LOCTEXT("Epic", "Epic");
    case 4: return LOCTEXT("Cinematic", "Cinematic");
    default: return LOCTEXT("Custom", "Custom");
    }
}
}

class SFSPauseMenu final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SFSPauseMenu) : _InitialVolume(1.0f) {}
        SLATE_ARGUMENT(float, InitialVolume)
        SLATE_EVENT(FOnFloatValueChanged, OnVolumeChanged)
        SLATE_EVENT(FSimpleDelegate, OnVolumeCommitted)
        SLATE_EVENT(FSimpleDelegate, OnResume)
        SLATE_EVENT(FSimpleDelegate, OnQuit)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args)
    {
        Volume = Args._InitialVolume;
        GConfig->GetFloat(InputSection, VibrationKey, Vibration, GGameUserSettingsIni);
        OnVolumeChanged = Args._OnVolumeChanged;
        OnVolumeCommitted = Args._OnVolumeCommitted;
        OnResume = Args._OnResume;
        OnQuit = Args._OnQuit;
        const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush("WhiteBrush");

        ChildSlot
        [
            SNew(SBorder)
            .BorderImage(WhiteBrush)
            .BorderBackgroundColor(FLinearColor(0.009f, 0.011f, 0.014f, 0.78f))
            .HAlign(HAlign_Center).VAlign(VAlign_Center)
            .Padding(18)
            [
                SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
                [
                    SNew(SBox).WidthOverride(440)
                    [
                        SNew(SBorder)
                        .BorderImage(WhiteBrush)
                        .BorderBackgroundColor(Gold)
                        .Padding(FMargin(1, 3, 1, 1))
                        [
                            SNew(SBorder)
                            .BorderImage(WhiteBrush)
                            .BorderBackgroundColor(Panel)
                            .Padding(27)
                            [
                                SNew(SVerticalBox)
                                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 23)
                                [
                                    SNew(STextBlock)
                                    .Text_Lambda([this] { return bConfirmQuit ? LOCTEXT("ConfirmHeading", "Exit to Desktop") : LOCTEXT("Title", "Paused"); })
                                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 28))
                                    .ColorAndOpacity(Gold)
                                ]
                                + SVerticalBox::Slot().AutoHeight()
                                [ BuildSettings() ]
                                + SVerticalBox::Slot().AutoHeight()
                                [ BuildExitConfirmation() ]
                            ]
                        ]
                    ]
                ]
            ]
        ];
    }

    TSharedPtr<SWidget> GetInitialFocus() const { return ResumeButton; }

    virtual FReply OnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override
    {
        if (Event.GetKey() != EKeys::Escape)
        {
            return SCompoundWidget::OnPreviewKeyDown(Geometry, Event);
        }

        if (Event.IsRepeat())
        {
            return FReply::Handled();
        }
        if (bConfirmQuit)
        {
            return ShowQuitConfirmation(false);
        }
        OnResume.ExecuteIfBound();
        return FReply::Handled();
    }

private:
    TSharedRef<SWidget> BuildSettings()
    {
        return SNew(SVerticalBox)
            .Visibility_Lambda([this] { return bConfirmQuit ? EVisibility::Collapsed : EVisibility::Visible; })
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1)
                [ SNew(STextBlock).Text(LOCTEXT("MasterVolume", "Master Volume")).ColorAndOpacity(Parchment) ]
                + SHorizontalBox::Slot().AutoWidth()
                [ SNew(STextBlock).Text_Lambda([this] { return FText::AsPercent(Volume); }).ColorAndOpacity(Gold) ]
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 22)
            [
                SNew(SSlider)
                .AccessibleText(LOCTEXT("MasterVolume", "Master Volume"))
                .Value(Volume)
                .StepSize(0.05f)
                .SliderBarColor(Parchment)
                .SliderHandleColor(Gold)
                .OnValueChanged_Lambda([this](float Value)
                {
                    Volume = Value;
                    OnVolumeChanged.ExecuteIfBound(Value);
                })
                .OnMouseCaptureEnd_Lambda([this] { OnVolumeCommitted.ExecuteIfBound(); })
                .OnControllerCaptureEnd_Lambda([this] { OnVolumeCommitted.ExecuteIfBound(); })
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 16)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1)
                [ SNew(STextBlock).Text(LOCTEXT("Vibration", "Controller Vibration")).ColorAndOpacity(Parchment) ]
                + SHorizontalBox::Slot().AutoWidth()
                [
                    SNew(SButton)
                    .OnClicked_Lambda([this]
                    {
                        Vibration = Vibration > 0.75f ? 0.5f : Vibration > 0.25f ? 0.0f : 1.0f;
                        GConfig->SetFloat(InputSection, VibrationKey, Vibration, GGameUserSettingsIni);
                        GConfig->Flush(false, GGameUserSettingsIni);
                        return FReply::Handled();
                    })
                    [
                        SNew(STextBlock).Text_Lambda([this] { return FText::AsPercent(Vibration); }).ColorAndOpacity(Gold)
                    ]
                ]
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 22)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
                [ SNew(STextBlock).Text(LOCTEXT("Graphics", "Graphics Quality")).ColorAndOpacity(Parchment) ]
                + SHorizontalBox::Slot().AutoWidth()
                [
                    SNew(SBox).MinDesiredWidth(136)
                    [
                        SNew(SButton).HAlign(HAlign_Center).ContentPadding(FMargin(12, 7))
                        .ToolTipText(LOCTEXT("CycleQuality", "Cycle through graphics presets"))
                        .OnClicked(this, &SFSPauseMenu::CycleQuality)
                        [
                            SNew(STextBlock).Text_Lambda([]
                            {
                                const UGameUserSettings* Settings = GEngine->GetGameUserSettings();
                                return FText::Format(LOCTEXT("QualityButton", "{0}  ›"), GetQualityLabel(Settings->GetOverallScalabilityLevel()));
                            })
                        ]
                    ]
                ]
            ]
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 6, 0)
                [
                    SAssignNew(ResumeButton, SButton).HAlign(HAlign_Center).ContentPadding(FMargin(10, 9))
                    .Text(LOCTEXT("Resume", "Resume"))
                    .OnClicked_Lambda([this] { OnResume.ExecuteIfBound(); return FReply::Handled(); })
                ]
                + SHorizontalBox::Slot().FillWidth(1).Padding(6, 0, 0, 0)
                [
                    SAssignNew(ExitButton, SButton).HAlign(HAlign_Center).ContentPadding(FMargin(10, 9))
                    .Text(LOCTEXT("Exit", "Exit Game"))
                    .OnClicked_Lambda([this] { return ShowQuitConfirmation(true); })
                ]
            ];
    }

    TSharedRef<SWidget> BuildExitConfirmation()
    {
        return SNew(SVerticalBox)
            .Visibility_Lambda([this] { return bConfirmQuit ? EVisibility::Visible : EVisibility::Collapsed; })
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 22)
            [
                SNew(STextBlock).AutoWrapText(true)
                .Text(LOCTEXT("QuitWarning", "Any active expedition will end. Saved character progress carries over."))
                .ColorAndOpacity(Parchment)
            ]
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 6, 0)
                [
                    SAssignNew(CancelButton, SButton).HAlign(HAlign_Center).ContentPadding(FMargin(10, 9))
                    .Text(LOCTEXT("Cancel", "Cancel"))
                    .OnClicked_Lambda([this] { return ShowQuitConfirmation(false); })
                ]
                + SHorizontalBox::Slot().FillWidth(1).Padding(6, 0, 0, 0)
                [
                    SNew(SButton).HAlign(HAlign_Center).ContentPadding(FMargin(10, 9))
                    .Text(LOCTEXT("ConfirmExit", "Exit Game"))
                    .OnClicked_Lambda([this] { OnQuit.ExecuteIfBound(); return FReply::Handled(); })
                ]
            ];
    }

    FReply ShowQuitConfirmation(bool bShow)
    {
        bConfirmQuit = bShow;
        return FReply::Handled().SetUserFocus((bShow ? CancelButton : ExitButton).ToSharedRef(), EFocusCause::SetDirectly);
    }

    FReply CycleQuality()
    {
        UGameUserSettings* Settings = GEngine->GetGameUserSettings();
        const int32 Quality = Settings->GetOverallScalabilityLevel();
        Settings->SetOverallScalabilityLevel(Quality >= 0 && Quality < 3 ? Quality + 1 : 0);
        Settings->ApplyNonResolutionSettings();
        Settings->SaveSettings();
        return FReply::Handled();
    }

    TSharedPtr<SButton> ResumeButton;
    TSharedPtr<SButton> ExitButton;
    TSharedPtr<SButton> CancelButton;
    FOnFloatValueChanged OnVolumeChanged;
    FSimpleDelegate OnVolumeCommitted;
    FSimpleDelegate OnResume;
    FSimpleDelegate OnQuit;
    float Volume = 1.0f;
    float Vibration = 1.0f;
    bool bConfirmQuit = false;
};

void UFSGameViewportClient::Init(FWorldContext& WorldContext, UGameInstance* OwningGameInstance, bool bCreateNewAudioDevice)
{
    Super::Init(WorldContext, OwningGameInstance, bCreateNewAudioDevice);
    GConfig->GetFloat(AudioSection, MasterVolumeKey, MasterVolume, GGameUserSettingsIni);
    SetMasterVolume(MasterVolume);
    bVolumeChanged = false;
}

bool UFSGameViewportClient::InputKey(const FInputKeyEventArgs& EventArgs)
{
    if (EventArgs.Key != EKeys::Escape || IsSimulateInEditorViewport() || (ViewportConsole && ViewportConsole->ConsoleActive()))
    {
        return Super::InputKey(EventArgs);
    }

    const FModifierKeysState Modifiers = FSlateApplication::Get().GetModifierKeys();
    if (!Modifiers.IsAltDown() && !Modifiers.IsControlDown() && !Modifiers.IsShiftDown() && !Modifiers.IsCommandDown())
    {
        if (EventArgs.Event == IE_Pressed && !PauseMenu && GameInstance && GetGameViewportWidget())
        {
            if (APlayerController* Player = GameInstance->GetFirstLocalPlayerController())
            {
                OpenPauseMenu(*Player);
            }
        }
        if (PauseMenu)
        {
            return true;
        }
    }
    return Super::InputKey(EventArgs);
}

void UFSGameViewportClient::DetachViewportClient()
{
    SaveMasterVolume();
    PauseMenu.Reset();
    Super::DetachViewportClient();
}

void UFSGameViewportClient::OpenPauseMenu(APlayerController& Player)
{
    PausedPlayer = &Player;
    bPausedByMenu = !Player.IsPaused() && Player.SetPause(true);
    bPreviousMouseCursor = Player.bShowMouseCursor;
    bPreviousIgnoreInput = IgnoreInput();
    PreviousCaptureMode = GetMouseCaptureMode();
    PreviousLockMode = GetMouseLockMode();

    PauseMenu = SNew(SFSPauseMenu)
        .InitialVolume(MasterVolume)
        .OnVolumeChanged(FOnFloatValueChanged::CreateUObject(this, &UFSGameViewportClient::SetMasterVolume))
        .OnVolumeCommitted(FSimpleDelegate::CreateUObject(this, &UFSGameViewportClient::SaveMasterVolume))
        .OnResume(FSimpleDelegate::CreateUObject(this, &UFSGameViewportClient::ClosePauseMenu))
        .OnQuit(FSimpleDelegate::CreateUObject(this, &UFSGameViewportClient::QuitGame));
    AddViewportWidgetContent(PauseMenu.ToSharedRef(), 100);

    FInputModeUIOnly MenuInput;
    MenuInput.SetWidgetToFocus(PauseMenu->GetInitialFocus());
    MenuInput.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    Player.SetInputMode(MenuInput);
    Player.bShowMouseCursor = true;
}

void UFSGameViewportClient::ClosePauseMenu()
{
    SaveMasterVolume();
    const TSharedPtr<SFSPauseMenu> ClosingMenu = MoveTemp(PauseMenu);
    RemoveViewportWidgetContent(ClosingMenu.ToSharedRef());

    if (APlayerController* Player = PausedPlayer.Get())
    {
        FInputModeGameOnly GameInput;
        GameInput.SetConsumeCaptureMouseDown(false);
        Player->SetInputMode(GameInput);
        SetIgnoreInput(bPreviousIgnoreInput);
        SetMouseCaptureMode(PreviousCaptureMode);
        SetMouseLockMode(PreviousLockMode);
        Player->bShowMouseCursor = bPreviousMouseCursor;
        if (bPausedByMenu)
        {
            Player->SetPause(false);
        }
    }
    PausedPlayer.Reset();
}

void UFSGameViewportClient::SetMasterVolume(float Value)
{
    const float Volume = FMath::Clamp(Value, 0.0f, 1.0f);
    bVolumeChanged |= Volume != MasterVolume;
    MasterVolume = Volume;
    if (AudioDevice.IsValid())
    {
        // Keep the player setting on the mixer output so level travel and camera fades can use their own volume.
        AudioDevice->SetSubmixOutputVolume(&AudioDevice->GetMainSubmixObject(), MasterVolume);
    }
}

void UFSGameViewportClient::SaveMasterVolume()
{
    if (bVolumeChanged)
    {
        GConfig->SetFloat(AudioSection, MasterVolumeKey, MasterVolume, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);
        bVolumeChanged = false;
    }
}

void UFSGameViewportClient::QuitGame()
{
    SaveMasterVolume();
    UKismetSystemLibrary::QuitGame(this, PausedPlayer.Get(), EQuitPreference::Quit, false);
}

#undef LOCTEXT_NAMESPACE
