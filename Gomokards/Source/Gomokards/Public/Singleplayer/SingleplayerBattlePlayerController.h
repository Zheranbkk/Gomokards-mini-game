#pragma once
#include "GameFramework/PlayerController.h"
#include "Singleplayer/SingleplayerPresentation.h"
#include "SingleplayerBattlePlayerController.generated.h"
class ASingleplayerBattleGameMode;
class SSingleplayerBattleView;

UCLASS()
class GOMOKARDS_API ASingleplayerBattlePlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    // Explicit binding also permits UI-free lifecycle verification.
    void BindBattle(ASingleplayerBattleGameMode* Mode);
    const Gomokards::FSingleplayerView& GetPresentation() const { return View; }
    const Gomokards::FSingleplayerSelection& GetSelection() const { return Selection; }
    const FString& GetFeedback() const { return Feedback; }
    FString GetLogText() const;
    void ClickBoard(FIntPoint Point);
    void ClickHand(int32 Index);
    void ClickDraw();
    void CancelSelection();
    void RestartBattle();
    void ExitGame();
private:
    TWeakObjectPtr<ASingleplayerBattleGameMode> OwnerMode;
    TSharedPtr<SSingleplayerBattleView> Widget;
    FDelegateHandle ChangedHandle;
    Gomokards::FSingleplayerView View;
    Gomokards::FSingleplayerSelection Selection;
    FString Feedback;
    void Refresh();
    void Submit(TOptional<Gomokards::FSingleplayerActionRequest> Request);
};
