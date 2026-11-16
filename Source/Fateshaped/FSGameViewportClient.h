#pragma once

#include "Engine/GameViewportClient.h"
#include "FSGameViewportClient.generated.h"

class ACameraActor;
class APawn;
class SFSPauseMenu;
class SFSMainMenu;
class UFSIntroWidget;

UCLASS()
class UFSGameViewportClient final : public UGameViewportClient
{
    GENERATED_BODY()

public:
    virtual void Init(FWorldContext& WorldContext, UGameInstance* OwningGameInstance, bool bCreateNewAudioDevice) override;
    virtual void Tick(float DeltaTime) override;
    virtual bool InputKey(const FInputKeyEventArgs& EventArgs) override;
    virtual void DetachViewportClient() override;

private:
    void OpenMainMenu(APlayerController& Player);
    void FrameExitPortal(APlayerController& Player);
    void ClearMenuView();
    void StartGame();
    void SavePrologue();
    void SaveGeraldIntro();
    void CaptureMenuInput(APlayerController& Player, TSharedPtr<SWidget> InitialFocus);
    void RestoreGameInput();
    void OpenPauseMenu(APlayerController& Player);
    void ClosePauseMenu();
    void SetMasterVolume(float Value);
    void SaveMasterVolume();
    void QuitGame();

    TSharedPtr<SFSPauseMenu> PauseMenu;
    TSharedPtr<SFSMainMenu> MainMenu;
    TObjectPtr<UFSIntroWidget> IntroWidget;
    TWeakObjectPtr<APlayerController> PausedPlayer;
    TWeakObjectPtr<ACameraActor> MenuCamera;
    TWeakObjectPtr<APawn> HiddenMenuPawn;
    bool bMenuPawnWasHidden = false;
    float MasterVolume = 1.0f;
    bool bVolumeChanged = false;
    bool bPausedByMenu = false;
    bool bPreviousMouseCursor = false;
    bool bPreviousIgnoreInput = false;
    bool bStartupMenuShown = false;
    bool bPreviousShowHUD = true;
    EMouseCaptureMode PreviousCaptureMode = EMouseCaptureMode::NoCapture;
    EMouseLockMode PreviousLockMode = EMouseLockMode::DoNotLock;
};
