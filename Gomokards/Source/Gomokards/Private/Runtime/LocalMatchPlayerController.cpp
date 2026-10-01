#include "Runtime/LocalMatchPlayerController.h"
#include "Runtime/LocalMatchGameMode.h"
#include "Runtime/LocalMatchGameState.h"
#include "Presentation/SLocalMatchView.h"
#include "Presentation/MatchPresentation.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

void ALocalMatchPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(ALocalMatchPlayerController, PrivateView, COND_OwnerOnly);
}
void ALocalMatchPlayerController::BeginPlay()
{
    Super::BeginPlay();
    RefreshPresentation();
    if (IsLocalController() && GetNetMode()!=NM_DedicatedServer && GetWorld()->GetGameViewport())
    {
        SAssignNew(MatchView, SLocalMatchView).Owner(this);
        GetWorld()->GetGameViewport()->AddViewportWidgetContent(MatchView.ToSharedRef(), 10);
        bShowMouseCursor=true;
        FInputModeUIOnly Mode;
        Mode.SetWidgetToFocus(MatchView);
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        SetInputMode(Mode);
    }
}
void ALocalMatchPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
    if (MatchView.IsValid() && GetWorld() && GetWorld()->GetGameViewport())
    { GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(MatchView.ToSharedRef()); }
    MatchView.Reset();
    Super::EndPlay(Reason);
}
void ALocalMatchPlayerController::PublishPrivate(const FMatchPrivateView& View)
{
    check(HasAuthority());
    PrivateView=View;
    ForceNetUpdate();
    OnRep_PrivateView();
}
void ALocalMatchPlayerController::OnRep_PrivateView() { RefreshPresentation(); }
void ALocalMatchPlayerController::RefreshPresentation()
{
    const auto* GS=GetWorld() ? GetWorld()->GetGameState<ALocalMatchGameState>() : nullptr;
    const uint64 PreviousEpoch=DisplayPublic.Epoch, PreviousRevision=DisplayPublic.Revision;
    bCoherent=false;
    if (GS)
    {
        const auto& Public=GS->GetPublicView();
        // Never retain old hand contents across reset while waiting for the other Actor.
        if (FMath::Max(Public.Epoch,PrivateView.Epoch)>DisplayPublic.Epoch)
        {
            DisplayPublic=FMatchPublicView{}; DisplayPrivate=FMatchPrivateView{};
            Feedback.Reset();
        }
        if (Public.Epoch>0 && Public.Epoch==PrivateView.Epoch && Public.Revision==PrivateView.Revision &&
            (Public.Epoch>DisplayPublic.Epoch || (Public.Epoch==DisplayPublic.Epoch && Public.Revision>=DisplayPublic.Revision)))
        {
            DisplayPublic=Public; DisplayPrivate=PrivateView; bCoherent=true;
        }
        // A newer Hidden snapshot must mask the old display immediately, even before private catch-up.
        // No true-color cache is retained for reveal; the coherent server snapshot replaces this view.
        if (!bCoherent && Public.Epoch==DisplayPublic.Epoch && Public.Revision>DisplayPublic.Revision
            && Public.GhostPhase==EMatchGhostPhase::Hidden)
        {
            for (auto& Cell : DisplayPublic.Cells)
            { if (Cell.Stone!=uint8(EMatchDisplayStone::Empty)) { Cell.Stone=uint8(EMatchDisplayStone::HiddenOccupied); } }
            DisplayPublic.GhostPhase=EMatchGhostPhase::Hidden;
        }
        if (PendingAck.IsSet() && Public.Epoch>PendingAck->Epoch) { PendingAck.Reset(); bPending=false; }
    }
    if (bCoherent && PendingAck.IsSet() && PendingAck->Epoch==DisplayPublic.Epoch && PendingAck->Revision<=DisplayPublic.Revision)
    {
        const auto Ack=PendingAck.GetValue(); PendingAck.Reset(); bPending=false;
        if (Ack.bAccepted)
        { Feedback=Ack.bBlockingReward ? TEXT("Successful block: +1 card in your hand.") : TEXT("Server accepted."); }
        else if (Ack.Error==EMatchIntentError::RuleRejected)
        { Feedback=Gomokards::RejectionLabel(static_cast<Gomokards::EActionError>(Ack.RuleError)); }
        else
        {
            switch (Ack.Error)
            {
            case EMatchIntentError::Unassigned: Feedback=TEXT("No gameplay seat assigned."); break;
            case EMatchIntentError::NotPlaying: Feedback=TEXT("Session is not playing."); break;
            case EMatchIntentError::CardNotNetworkEnabled: Feedback=TEXT("Card networking not available until a later phase."); break;
            case EMatchIntentError::Unauthorized: Feedback=TEXT("Development restart is not authorized."); break;
            default: Feedback=TEXT("Stale request rejected. Use the current view."); break;
            }
        }
    }
    if (PreviousEpoch!=DisplayPublic.Epoch || PreviousRevision!=DisplayPublic.Revision || !CanTargetCard(SelectedTargetedCard))
    { SelectedTargetedCard=0; }
    OnPresentationChanged.Broadcast();
}
bool ALocalMatchPlayerController::IsPresentationReady() const
{ return bCoherent && DisplayPrivate.PlayerId!=INDEX_NONE && DisplayPublic.Cells.Num()==361 && DisplayPublic.Seats.Num()==2; }
bool ALocalMatchPlayerController::CanPlace(FIntPoint Point) const
{
    if (!IsPresentationReady() || bPending || DisplayPublic.Session!=EMatchSession::Playing || DisplayPublic.Result!=0 ||
        DisplayPublic.GhostPhase==EMatchGhostPhase::Preparation || DisplayPublic.CurrentPlayerId!=DisplayPrivate.PlayerId || Point.X<0 || Point.Y<0 || Point.X>=19 || Point.Y>=19) { return false; }
    const auto& Cell=DisplayPublic.Cells[Point.Y*19+Point.X];
    return Cell.Stone==0 && !Cell.bForbidden;
}
bool ALocalMatchPlayerController::CanPlayCard(uint8 CardId) const
{
    return IsPresentationReady() && !bPending && DisplayPublic.Session==EMatchSession::Playing && DisplayPublic.Result==0 &&
        !DisplayPublic.bCardsDisabled && DisplayPublic.GhostPhase==EMatchGhostPhase::None && DisplayPublic.CurrentPlayerId==DisplayPrivate.PlayerId &&
        IsNetworkCardEnabled(CardId) && DisplayPrivate.Hand.Contains(CardId);
}
void ALocalMatchPlayerController::RequestCard(uint8 CardId)
{
    if (!IsPresentationReady() || bPending) { return; }
    SelectedTargetedCard=0;
    bPending=true; Feedback=TEXT("Waiting for server...");
    ServerPlayCard(DisplayPublic.Epoch,DisplayPublic.CompletedActions,CardId);
    OnPresentationChanged.Broadcast();
}
bool ALocalMatchPlayerController::CanTargetCard(uint8 CardId) const
{
    return IsPresentationReady() && !bPending && DisplayPublic.Session==EMatchSession::Playing && DisplayPublic.Result==0 &&
        !DisplayPublic.bCardsDisabled && DisplayPublic.GhostPhase==EMatchGhostPhase::None && DisplayPublic.CurrentPlayerId==DisplayPrivate.PlayerId &&
        IsTargetedNetworkCardEnabled(CardId) && DisplayPrivate.Hand.Contains(CardId);
}
void ALocalMatchPlayerController::ToggleTargeting(uint8 CardId)
{
    if (SelectedTargetedCard==CardId) { CancelTargeting(); return; }
    if (!CanTargetCard(CardId)) { return; }
    SelectedTargetedCard=CardId;
    Feedback=TEXT("Target selected locally. Choose a board target, or cancel without spending an action.");
    OnPresentationChanged.Broadcast();
}
void ALocalMatchPlayerController::CancelTargeting()
{
    if (SelectedTargetedCard==0) { return; }
    SelectedTargetedCard=0;
    Feedback=TEXT("Targeting cancelled. No action spent.");
    OnPresentationChanged.Broadcast();
}
void ALocalMatchPlayerController::RequestBoardClick(FIntPoint Point)
{
    if (SelectedTargetedCard!=0) { RequestTargetedCard(SelectedTargetedCard,Point); }
    else { RequestPlace(Point); }
}
void ALocalMatchPlayerController::RequestTargetedCard(uint8 CardId, FIntPoint Target)
{
    if (!IsPresentationReady() || bPending) { return; }
    SelectedTargetedCard=0; // Rejection can be retried by selecting the card again.
    bPending=true; Feedback=TEXT("Waiting for server...");
    ServerPlayTargetedCard(DisplayPublic.Epoch,DisplayPublic.CompletedActions,CardId,Target.X,Target.Y);
    OnPresentationChanged.Broadcast();
}
bool ALocalMatchPlayerController::CanDevelopmentRestart() const
{ return IsPresentationReady() && !bPending && DisplayPrivate.bDevelopmentAdmin && DisplayPublic.Session==EMatchSession::Playing; }
void ALocalMatchPlayerController::RequestPlace(FIntPoint Point)
{
    // Preparation freezes local board input; the server independently rejects crafted requests.
    // Otherwise let the server explain wrong-turn/invalid-cell requests.
    if (!IsPresentationReady() || bPending || DisplayPublic.GhostPhase==EMatchGhostPhase::Preparation) { return; }
    SelectedTargetedCard=0;
    bPending=true; Feedback=TEXT("Waiting for server...");
    ServerPlaceStone(DisplayPublic.Epoch,DisplayPublic.CompletedActions,Point.X,Point.Y);
    OnPresentationChanged.Broadcast();
}
void ALocalMatchPlayerController::RequestDevelopmentRestart()
{
    if (!CanDevelopmentRestart()) { return; }
    SelectedTargetedCard=0;
    bPending=true; Feedback=TEXT("Waiting for development restart...");
    ServerDevelopmentRestart(DisplayPublic.Epoch);
    OnPresentationChanged.Broadcast();
}
void ALocalMatchPlayerController::ServerPlaceStone_Implementation(uint64 Epoch, uint64 ExpectedCompletedActions, int32 X, int32 Y)
{
    if (auto* MatchOwner=GetWorld()->GetAuthGameMode<ALocalMatchGameMode>())
    { ClientActionResult(MatchOwner->PlaceFrom(this,Epoch,ExpectedCompletedActions,{X,Y})); }
}
void ALocalMatchPlayerController::ServerPlayCard_Implementation(uint64 Epoch, uint64 ExpectedCompletedActions, uint8 CardId)
{
    if (auto* MatchOwner=GetWorld()->GetAuthGameMode<ALocalMatchGameMode>())
    { ClientActionResult(MatchOwner->CardFrom(this,Epoch,ExpectedCompletedActions,CardId)); }
}
void ALocalMatchPlayerController::ServerPlayTargetedCard_Implementation(uint64 Epoch, uint64 ExpectedCompletedActions, uint8 CardId, int32 TargetX, int32 TargetY)
{
    if (auto* MatchOwner=GetWorld()->GetAuthGameMode<ALocalMatchGameMode>())
    { ClientActionResult(MatchOwner->TargetedCardFrom(this,Epoch,ExpectedCompletedActions,CardId,{TargetX,TargetY})); }
}
void ALocalMatchPlayerController::ServerDevelopmentRestart_Implementation(uint64 Epoch)
{
    if (auto* MatchOwner=GetWorld()->GetAuthGameMode<ALocalMatchGameMode>()) { ClientActionResult(MatchOwner->RestartFrom(this,Epoch)); }
}
void ALocalMatchPlayerController::ClientActionResult_Implementation(FMatchActionAck Ack)
{
    if (Ack.Epoch<DisplayPublic.Epoch)
    {
        // Keep one outstanding request across resets until its acknowledgement is accounted for.
        bPending=false; PendingAck.Reset(); RefreshPresentation(); return;
    }
    PendingAck=Ack;
    RefreshPresentation();
}
FString ALocalMatchPlayerController::StatusLabel() const
{
    using namespace Gomokards;
    if (!bCoherent) { return TEXT("Synchronizing public board and private hand..."); }
    if (DisplayPrivate.PlayerId==INDEX_NONE) { return TEXT("No gameplay seat available (two players only)."); }
    const FString Identity=FString::Printf(TEXT("You: %s (ID %d) | "),*StoneLabel(static_cast<EStone>(DisplayPrivate.Stone)),DisplayPrivate.PlayerId);
    if (DisplayPublic.Session==EMatchSession::WaitingForPlayers) { return Identity+TEXT("Waiting for second player."); }
    if (DisplayPublic.Session==EMatchSession::SessionEnded) { return Identity+TEXT("Session ended. Start a new session to continue."); }
    switch (static_cast<EMatchStatus>(DisplayPublic.Result))
    {
    case EMatchStatus::Won: return Identity+StoneLabel(static_cast<EStone>(DisplayPublic.WinningStone))+TEXT(" wins.");
    case EMatchStatus::Draw: return Identity+TEXT("Draw: simultaneous wins.");
    case EMatchStatus::AwaitingRuleDecision: return Identity+TEXT("Awaiting rule decision: no legal action.");
    default: return Identity+FString::Printf(TEXT("Current player: %d | Actions: %llu"),DisplayPublic.CurrentPlayerId,DisplayPublic.CompletedActions);
    }
}

FString ALocalMatchPlayerController::GhostStatusLabel() const
{
    if (!bCoherent) { return {}; }
    if (DisplayPublic.GhostPhase==EMatchGhostPhase::Preparation)
    {
        const auto* GS=GetWorld() ? GetWorld()->GetGameState<ALocalMatchGameState>() : nullptr;
        const double Remaining=GS ? FMath::Max(0.0,DisplayPublic.GhostDisplayEndServerTime-GS->GetServerWorldTimeSeconds()) : 0.0;
        return FString::Printf(TEXT("GHOST: Memorize the board - %.1fs. Gameplay frozen; waiting for server."),Remaining);
    }
    if (DisplayPublic.GhostPhase==EMatchGhostPhase::Hidden)
    { return FString::Printf(TEXT("GHOST: Colors hidden - %d placements remaining. Cards unavailable."),6-DisplayPublic.GhostPlacementsCompleted); }
    return {};
}
