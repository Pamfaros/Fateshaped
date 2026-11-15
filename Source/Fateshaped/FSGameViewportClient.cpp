#include "FSGameViewportClient.h"
#include "FSIntroWidget.h"

#include "AudioDevice.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/Console.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "InputKeyEventArgs.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/ConfigCacheIni.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
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

class SFSMainMenu final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SFSMainMenu) {}
        SLATE_EVENT(FSimpleDelegate, OnStart)
        SLATE_EVENT(FSimpleDelegate, OnQuit)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args)
    {
        OnStart = Args._OnStart;
        OnQuit = Args._OnQuit;
        TArray<FString> Entries;
        GConfig->GetArray(TEXT("Fateshaped.Credits"), TEXT("Entries"), Entries, GGameIni);
        FString Credits;
        for (const FString& Entry : Entries)
        {
            if (!Credits.IsEmpty()) Credits += TEXT("\n\n");
            Credits += Entry;
        }
        const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush("WhiteBrush");
        ChildSlot
        [
            SNew(SBorder).BorderImage(WhiteBrush)
            .BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f))
            .HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(FMargin(72, 48))
            [
                SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
                [
                    SNew(SBox).WidthOverride(560)
                    [
                        SNew(SBorder).BorderImage(WhiteBrush).BorderBackgroundColor(Gold).Padding(1)
                        [
                            SNew(SBorder).BorderImage(WhiteBrush).BorderBackgroundColor(Panel).Padding(36)
                            [
                                SNew(SVerticalBox)
                                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 10)
                                [
                                    SNew(STextBlock).Text(LOCTEXT("GameTitle", "FATESHAPED"))
                                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 38)).ColorAndOpacity(Gold)
                                ]
                                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 30)
                                [
                                    SNew(STextBlock).Text_Lambda([this]
                                    {
                                        return bCreditsOpen ? LOCTEXT("CreditsTitle", "Credits") : LOCTEXT("GameTagline", "Every choice shapes your fate.");
                                    }).Font(FCoreStyle::GetDefaultFontStyle("Regular", 16)).ColorAndOpacity(Parchment)
                                ]
                                + SVerticalBox::Slot().AutoHeight()
                                [
                                    SNew(SVerticalBox)
                                    .Visibility_Lambda([this] { return bCreditsOpen ? EVisibility::Collapsed : EVisibility::Visible; })
                                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
                                    [
                                        SAssignNew(StartButton, SButton).HAlign(HAlign_Center).ContentPadding(FMargin(20, 14))
                                        .Text(LOCTEXT("StartGame", "Start Game"))
                                        .OnClicked_Lambda([this] { OnStart.ExecuteIfBound(); return FReply::Handled(); })
                                    ]
                                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
                                    [
                                        SAssignNew(CreditsButton, SButton).HAlign(HAlign_Center).ContentPadding(FMargin(20, 14))
                                        .Text(LOCTEXT("Credits", "Credits"))
                                        .OnClicked_Lambda([this] { return ShowCredits(true); })
                                    ]
                                    + SVerticalBox::Slot().AutoHeight()
                                    [
                                        SNew(SButton).HAlign(HAlign_Center).ContentPadding(FMargin(20, 14))
                                        .Text(LOCTEXT("MainMenuExit", "Exit"))
                                        .OnClicked_Lambda([this] { OnQuit.ExecuteIfBound(); return FReply::Handled(); })
                                    ]
                                ]
                                + SVerticalBox::Slot().AutoHeight()
                                [
                                    SNew(SVerticalBox)
                                    .Visibility_Lambda([this] { return bCreditsOpen ? EVisibility::Visible : EVisibility::Collapsed; })
                                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 24)
                                    [
                                        SNew(SBox).MaxDesiredHeight(320)
                                        [
                                            SNew(SScrollBox)
                                            + SScrollBox::Slot().Padding(0, 0, 16, 0)
                                            [
                                                SNew(STextBlock).Text(FText::FromString(Credits)).WrapTextAt(430)
                                                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 18)).ColorAndOpacity(Parchment)
                                            ]
                                        ]
                                    ]
                                    + SVerticalBox::Slot().AutoHeight()
                                    [
                                        SAssignNew(BackButton, SButton).HAlign(HAlign_Center).ContentPadding(FMargin(20, 14))
                                        .Text(LOCTEXT("CreditsBack", "Back"))
                                        .OnClicked_Lambda([this] { return ShowCredits(false); })
                                    ]
                                ]
                            ]
                        ]
                    ]
                ]
            ]
        ];
    }

    TSharedPtr<SWidget> GetInitialFocus() const { return StartButton; }

    virtual FReply OnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override
    {
        if (Event.GetKey() == EKeys::Escape)
        {
            // Escape returns from credits; only Start Game can dismiss the startup screen.
            return bCreditsOpen && !Event.IsRepeat() ? ShowCredits(false) : FReply::Handled();
        }
        return SCompoundWidget::OnPreviewKeyDown(Geometry, Event);
    }

private:
    FReply ShowCredits(bool bShow)
    {
        bCreditsOpen = bShow;
        return FReply::Handled().SetUserFocus((bShow ? BackButton : CreditsButton).ToSharedRef(), EFocusCause::SetDirectly);
    }

    TSharedPtr<SButton> StartButton, CreditsButton, BackButton;
    FSimpleDelegate OnStart, OnQuit;
    bool bCreditsOpen = false;
};

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

void UFSGameViewportClient::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (!bStartupMenuShown && !IsSimulateInEditorViewport() && GameInstance && GetGameViewportWidget())
    {
        APlayerController* Player = GameInstance->GetFirstLocalPlayerController();
        if (Player && Player->HasActorBegunPlay() && Player->GetPawn())
        {
            bStartupMenuShown = true;
            OpenMainMenu(*Player);
        }
    }
}

bool UFSGameViewportClient::InputKey(const FInputKeyEventArgs& EventArgs)
{
    if (MainMenu)
    {
        return true;
    }
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
    ClearMenuView();
    if (IntroWidget)
    {
        IntroWidget->RemoveFromParent();
        IntroWidget = nullptr;
    }
    PauseMenu.Reset();
    MainMenu.Reset();
    PausedPlayer.Reset();
    Super::DetachViewportClient();
}

void UFSGameViewportClient::OpenMainMenu(APlayerController& Player)
{
    FrameExitPortal(Player);
    MainMenu = SNew(SFSMainMenu)
        .OnStart(FSimpleDelegate::CreateUObject(this, &UFSGameViewportClient::StartGame))
        .OnQuit(FSimpleDelegate::CreateUObject(this, &UFSGameViewportClient::QuitGame));
    AddViewportWidgetContent(MainMenu.ToSharedRef(), 100);
    if (AHUD* HUD = Player.GetHUD())
    {
        bPreviousShowHUD = HUD->bShowHUD;
        HUD->bShowHUD = false;
    }
    CaptureMenuInput(Player, MainMenu->GetInitialFocus());
}

void UFSGameViewportClient::FrameExitPortal(APlayerController& Player)
{
    UWorld* MenuWorld = Player.GetWorld();
    if (!MenuWorld) return;

    AActor* Portal = nullptr;
    int32 BestScore = 0;
    for (TActorIterator<AActor> Actor(MenuWorld); Actor; ++Actor)
    {
        const bool bTagged = Actor->ActorHasTag(TEXT("QuestPortal"));
        const bool bNamed = Actor->GetName().Contains(TEXT("QuestPortal"));
        if (!bTagged && !bNamed) continue;
        int32 Score = (bTagged ? 4 : 0) + (bNamed ? 2 : 0);
        if (Actor->GetName().Contains(TEXT("QuestPortal2"))) Score -= 1;
        if (Score > BestScore)
        {
            BestScore = Score;
            Portal = *Actor;
        }
    }
    if (!Portal)
    {
        UE_LOG(LogTemp, Warning, TEXT("Startup menu could not find the Guild exit portal."));
        return;
    }

    FVector Origin = FVector::ZeroVector;
    FVector Extent = FVector::ZeroVector;
    Portal->GetActorBounds(false, Origin, Extent);
    const FVector Focus = Origin + FVector(0, 0, Extent.Z * 0.12);
    const FVector FromSpawn = Player.GetPawn() ? Focus - Player.GetPawn()->GetActorLocation() : Portal->GetActorForwardVector();
    const FVector Approach = FromSpawn.GetSafeNormal2D().IsNearlyZero() ? FVector(1, 0, 0) : FromSpawn.GetSafeNormal2D();
    const FVector Right = FVector::CrossProduct(FVector::UpVector, Approach);
    const double Distance = FMath::Clamp(FMath::Max3(Extent.X, Extent.Y, 180.0) * 3.4, 460.0, 1200.0);
    // Shift the camera left so the portal sits in the open right side of the frame.
    const FVector CameraLocation = Focus - Approach * Distance - Right * Distance * 0.3 + FVector(0, 0, 120);

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ACameraActor* Camera = MenuWorld->SpawnActor<ACameraActor>(CameraLocation, (Focus - CameraLocation).Rotation(), Params);
    if (!Camera) return;
    if (UCameraComponent* Component = Camera->GetCameraComponent())
    {
        Component->SetFieldOfView(62.0f);
        Component->bConstrainAspectRatio = false;
    }
    Player.SetViewTarget(Camera);
    MenuCamera = Camera;
    if (APawn* Pawn = Player.GetPawn())
    {
        HiddenMenuPawn = Pawn;
        bMenuPawnWasHidden = Pawn->IsHidden();
        Pawn->SetActorHiddenInGame(true);
    }
}

void UFSGameViewportClient::ClearMenuView()
{
    if (APlayerController* Player = PausedPlayer.Get())
    {
        if (APawn* Pawn = HiddenMenuPawn.Get())
        {
            Pawn->SetActorHiddenInGame(bMenuPawnWasHidden);
            Player->SetViewTarget(Pawn);
        }
    }
    if (ACameraActor* Camera = MenuCamera.Get())
    {
        Camera->Destroy();
    }
    MenuCamera.Reset();
    HiddenMenuPawn.Reset();
}

void UFSGameViewportClient::StartGame()
{
    if (!MainMenu) return;
    const TSharedPtr<SFSMainMenu> ClosingMenu = MoveTemp(MainMenu);
    RemoveViewportWidgetContent(ClosingMenu.ToSharedRef());
    APlayerController* Player = PausedPlayer.Get();
    ClearMenuView();
    if (Player)
    {
        if (UFSIntroWidget* Intro = UFSIntroWidget::TryBegin(*Player))
        {
            if (bPausedByMenu)
            {
                Player->SetPause(false);
                bPausedByMenu = false;
            }
            Intro->OnPrologueComplete.BindUObject(this, &UFSGameViewportClient::SavePrologue);
            Intro->OnDialogueComplete.BindUObject(this, &UFSGameViewportClient::SaveGeraldIntro);
            IntroWidget = Intro;
            return;
        }
        if (AHUD* HUD = Player->GetHUD()) HUD->bShowHUD = bPreviousShowHUD;
    }
    RestoreGameInput();
}

void UFSGameViewportClient::SavePrologue()
{
    if (APlayerController* Player = PausedPlayer.Get())
    {
        UFSIntroWidget::CommitPrologue(*Player);
    }
}

void UFSGameViewportClient::SaveGeraldIntro()
{
    if (APlayerController* Player = PausedPlayer.Get())
    {
        UFSIntroWidget::CommitIntroduction(*Player);
        if (AHUD* HUD = Player->GetHUD()) HUD->bShowHUD = bPreviousShowHUD;
    }
    IntroWidget = nullptr;
    RestoreGameInput();
}

void UFSGameViewportClient::CaptureMenuInput(APlayerController& Player, TSharedPtr<SWidget> InitialFocus)
{
    PausedPlayer = &Player;
    bPausedByMenu = !Player.IsPaused() && Player.SetPause(true);
    bPreviousMouseCursor = Player.bShowMouseCursor;
    bPreviousIgnoreInput = IgnoreInput();
    PreviousCaptureMode = GetMouseCaptureMode();
    PreviousLockMode = GetMouseLockMode();

    FInputModeUIOnly MenuInput;
    MenuInput.SetWidgetToFocus(InitialFocus);
    MenuInput.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    Player.SetInputMode(MenuInput);
    Player.bShowMouseCursor = true;
}

void UFSGameViewportClient::OpenPauseMenu(APlayerController& Player)
{

    PauseMenu = SNew(SFSPauseMenu)
        .InitialVolume(MasterVolume)
        .OnVolumeChanged(FOnFloatValueChanged::CreateUObject(this, &UFSGameViewportClient::SetMasterVolume))
        .OnVolumeCommitted(FSimpleDelegate::CreateUObject(this, &UFSGameViewportClient::SaveMasterVolume))
        .OnResume(FSimpleDelegate::CreateUObject(this, &UFSGameViewportClient::ClosePauseMenu))
        .OnQuit(FSimpleDelegate::CreateUObject(this, &UFSGameViewportClient::QuitGame));
    AddViewportWidgetContent(PauseMenu.ToSharedRef(), 100);

    CaptureMenuInput(Player, PauseMenu->GetInitialFocus());
}

void UFSGameViewportClient::ClosePauseMenu()
{
    SaveMasterVolume();
    const TSharedPtr<SFSPauseMenu> ClosingMenu = MoveTemp(PauseMenu);
    RemoveViewportWidgetContent(ClosingMenu.ToSharedRef());
    RestoreGameInput();
}

void UFSGameViewportClient::RestoreGameInput()
{
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
