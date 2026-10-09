#include "Singleplayer/SingleplayerBattlePlayerController.h"
#include "Singleplayer/SingleplayerBattleGameMode.h"
#include "Singleplayer/SSingleplayerBattleView.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Framework/Application/SlateApplication.h"
using namespace Gomokards;

void ASingleplayerBattlePlayerController::BeginPlay()
{
    Super::BeginPlay();
    if (!IsLocalController()) { return; }
    BindBattle(GetWorld()->GetAuthGameMode<ASingleplayerBattleGameMode>());
    if (!OwnerMode.IsValid()) { return; }
    if (!GetWorld()->GetGameViewport()) { return; }
    SAssignNew(Widget, SSingleplayerBattleView).Owner(this);
    GetWorld()->GetGameViewport()->AddViewportWidgetContent(Widget.ToSharedRef());
    bShowMouseCursor = true;
    FInputModeUIOnly Input;
    Input.SetWidgetToFocus(Widget);
    Input.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(Input);
    FSlateApplication::Get().SetUserFocus(0, Widget, EFocusCause::SetDirectly);
}
void ASingleplayerBattlePlayerController::BindBattle(ASingleplayerBattleGameMode* Mode)
{
    if (OwnerMode.IsValid()) { OwnerMode->OnBattleChanged.Remove(ChangedHandle); }
    OwnerMode = Mode;
    View = {};
    Selection.Clear();
    Feedback.Reset();
    if (OwnerMode.IsValid())
    {
        ChangedHandle = OwnerMode->OnBattleChanged.AddUObject(this, &ThisClass::Refresh);
        Refresh();
    }
}
void ASingleplayerBattlePlayerController::Refresh()
{
    if (!OwnerMode.IsValid()) { return; }
    View = OwnerMode->GetPresentation();
    Selection.Clear();
    Feedback.Reset();
    if (Widget) { Widget->ResetHover(); }
}
void ASingleplayerBattlePlayerController::Submit(TOptional<FSingleplayerActionRequest> Request)
{
    if (!Request.IsSet() || !OwnerMode.IsValid()) { return; }
    const auto Result = OwnerMode->SubmitPlayer(Request.GetValue(), View.Generation);
    Feedback = SingleplayerErrorText(Result.Error);
}
void ASingleplayerBattlePlayerController::ClickBoard(FIntPoint Point) { Submit(Selection.BoardClick(View, Point)); }
void ASingleplayerBattlePlayerController::ClickHand(int32 Index) { Submit(Selection.HandClick(View, Index)); }
void ASingleplayerBattlePlayerController::ClickDraw() { Submit(Selection.Draw(View)); }
void ASingleplayerBattlePlayerController::CancelSelection() { Selection.Clear(); Feedback.Reset(); }
void ASingleplayerBattlePlayerController::RestartBattle() { if (OwnerMode.IsValid()) { OwnerMode->RestartBattle(); } }
void ASingleplayerBattlePlayerController::ExitGame() { UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false); }
FString ASingleplayerBattlePlayerController::GetLogText() const
{ return OwnerMode.IsValid() ? FString::Join(OwnerMode->GetLog(), TEXT("\n\n")) : FString(); }
void ASingleplayerBattlePlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
    if (OwnerMode.IsValid()) { OwnerMode->OnBattleChanged.Remove(ChangedHandle); }
    if (Widget && GetWorld() && GetWorld()->GetGameViewport()) { GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(Widget.ToSharedRef()); }
    Widget.Reset();
    OwnerMode.Reset();
    Selection.Clear();
    View = {};
    Super::EndPlay(Reason);
}
