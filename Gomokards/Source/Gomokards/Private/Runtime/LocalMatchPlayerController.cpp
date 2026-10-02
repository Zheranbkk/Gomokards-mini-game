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
        const auto& Pose=GS->GetTetrisPose();
        if (Pose.Epoch>LatestTetrisPose.Epoch || (Pose.Epoch==LatestTetrisPose.Epoch &&
            (Pose.ActivationToken>LatestTetrisPose.ActivationToken || (Pose.ActivationToken==LatestTetrisPose.ActivationToken && Pose.PoseSequence>LatestTetrisPose.PoseSequence))))
        { LatestTetrisPose=Pose; }
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
        { Feedback=Ack.bBlockingReward ? TEXT("成功阻挡，获得 1 张卡牌。") : TEXT("行动成功。"); }
        else if (Ack.Error==EMatchIntentError::RuleRejected)
        { Feedback=Gomokards::RejectionLabel(static_cast<Gomokards::EActionError>(Ack.RuleError)); }
        else
        {
            switch (Ack.Error)
            {
            case EMatchIntentError::Unassigned: Feedback=TEXT("尚未分配玩家位置。"); break;
            case EMatchIntentError::NotPlaying: Feedback=TEXT("当前对局无法继续操作。"); break;
            case EMatchIntentError::CardNotNetworkEnabled: Feedback=TEXT("当前无法使用这张卡牌。"); break;
            case EMatchIntentError::Unauthorized: Feedback=TEXT("只有主机可以重新开始。"); break;
            default: Feedback=TEXT("局面已更新，请重试。"); break;
            }
        }
    }
    if (PreviousEpoch!=DisplayPublic.Epoch || PreviousRevision!=DisplayPublic.Revision || !CanTargetCard(SelectedTargetedCard))
    { SelectedTargetedCard=0; }
    if (bCoherent) { GameLog.Update(DisplayPublic,DisplayPrivate); }
    OnPresentationChanged.Broadcast();
}
bool ALocalMatchPlayerController::IsPresentationReady() const
{ return bCoherent && DisplayPrivate.PlayerId!=INDEX_NONE && DisplayPublic.Cells.Num()==361 && DisplayPublic.Seats.Num()==2; }
bool ALocalMatchPlayerController::CanPlace(FIntPoint Point) const
{
    if (!IsPresentationReady() || bPending || DisplayPublic.Session!=EMatchSession::Playing || DisplayPublic.Result!=0 ||
        DisplayPublic.bTetrisActive || DisplayPublic.GhostPhase==EMatchGhostPhase::Preparation || DisplayPublic.CurrentPlayerId!=DisplayPrivate.PlayerId || Point.X<0 || Point.Y<0 || Point.X>=19 || Point.Y>=19) { return false; }
    const auto& Cell=DisplayPublic.Cells[Point.Y*19+Point.X];
    return Cell.Stone==0 && !Cell.bForbidden;
}
bool ALocalMatchPlayerController::CanPlayCard(uint8 CardId) const
{
    return IsPresentationReady() && !bPending && DisplayPublic.Session==EMatchSession::Playing && DisplayPublic.Result==0 &&
        !DisplayPublic.bTetrisActive && !DisplayPublic.bCardsDisabled && DisplayPublic.GhostPhase==EMatchGhostPhase::None && DisplayPublic.CurrentPlayerId==DisplayPrivate.PlayerId &&
        IsNetworkCardEnabled(CardId) && DisplayPrivate.Hand.Contains(CardId);
}
void ALocalMatchPlayerController::RequestCard(uint8 CardId)
{
    if (!IsPresentationReady() || bPending || DisplayPublic.bTetrisActive) { return; }
    SelectedTargetedCard=0;
    bPending=true; Feedback=TEXT("等待服务器响应……");
    ServerPlayCard(DisplayPublic.Epoch,DisplayPublic.CompletedActions,CardId);
    OnPresentationChanged.Broadcast();
}
bool ALocalMatchPlayerController::CanTargetCard(uint8 CardId) const
{
    return IsPresentationReady() && !bPending && DisplayPublic.Session==EMatchSession::Playing && DisplayPublic.Result==0 &&
        !DisplayPublic.bTetrisActive && !DisplayPublic.bCardsDisabled && DisplayPublic.GhostPhase==EMatchGhostPhase::None && DisplayPublic.CurrentPlayerId==DisplayPrivate.PlayerId &&
        IsTargetedNetworkCardEnabled(CardId) && DisplayPrivate.Hand.Contains(CardId);
}
void ALocalMatchPlayerController::ToggleTargeting(uint8 CardId)
{
    if (SelectedTargetedCard==CardId) { CancelTargeting(); return; }
    if (!CanTargetCard(CardId)) { return; }
    SelectedTargetedCard=CardId;
    Feedback=TEXT("请选择目标；按 Esc 或右键取消。");
    OnPresentationChanged.Broadcast();
}
void ALocalMatchPlayerController::CancelTargeting()
{
    if (SelectedTargetedCard==0) { return; }
    SelectedTargetedCard=0;
    Feedback=TEXT("已取消选择。");
    OnPresentationChanged.Broadcast();
}
void ALocalMatchPlayerController::RequestBoardClick(FIntPoint Point)
{
    if (SelectedTargetedCard!=0) { RequestTargetedCard(SelectedTargetedCard,Point); }
    else { RequestPlace(Point); }
}
void ALocalMatchPlayerController::RequestTargetedCard(uint8 CardId, FIntPoint Target)
{
    if (!IsPresentationReady() || bPending || DisplayPublic.bTetrisActive) { return; }
    SelectedTargetedCard=0; // Rejection can be retried by selecting the card again.
    bPending=true; Feedback=TEXT("等待服务器响应……");
    ServerPlayTargetedCard(DisplayPublic.Epoch,DisplayPublic.CompletedActions,CardId,Target.X,Target.Y);
    OnPresentationChanged.Broadcast();
}
bool ALocalMatchPlayerController::CanDevelopmentRestart() const
{ return IsPresentationReady() && !bPending && DisplayPrivate.bDevelopmentAdmin && DisplayPublic.Session==EMatchSession::Playing; }
void ALocalMatchPlayerController::RequestPlace(FIntPoint Point)
{
    // Preparation freezes local board input; the server independently rejects crafted requests.
    // Otherwise let the server explain wrong-turn/invalid-cell requests.
    if (!IsPresentationReady() || bPending || DisplayPublic.bTetrisActive || DisplayPublic.GhostPhase==EMatchGhostPhase::Preparation) { return; }
    SelectedTargetedCard=0;
    bPending=true; Feedback=TEXT("等待服务器响应……");
    ServerPlaceStone(DisplayPublic.Epoch,DisplayPublic.CompletedActions,Point.X,Point.Y);
    OnPresentationChanged.Broadcast();
}
void ALocalMatchPlayerController::RequestDevelopmentRestart()
{
    if (!CanDevelopmentRestart()) { return; }
    SelectedTargetedCard=0;
    bPending=true; Feedback=TEXT("等待重新开始……");
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
    if (!bCoherent) { return TEXT("正在同步对局……"); }
    if (DisplayPrivate.PlayerId==INDEX_NONE) { return TEXT("没有可用的玩家位置。仅支持两位玩家。"); }
    const FString Identity=DemoSide(DisplayPrivate.Stone)+TEXT("（你）  ·  ");
    if (DisplayPublic.Session==EMatchSession::WaitingForPlayers) { return Identity+TEXT("等待另一位玩家加入……"); }
    if (DisplayPublic.Session==EMatchSession::SessionEnded) { return TEXT("对方已离开，对局结束。请开启新对局。"); }
    const auto Result=DemoResult(DisplayPublic);
    if (!Result.IsEmpty()) { return Result; }
    if (DisplayPublic.Result==uint8(EMatchStatus::AwaitingRuleDecision)) { return TEXT("当前无合法行动，对局已暂停。"); }
    return Identity+TEXT("轮到")+DemoSide(DemoPlayerStone(DisplayPublic,DisplayPublic.CurrentPlayerId));
}

FString ALocalMatchPlayerController::GhostStatusLabel() const
{
    if (!bCoherent) { return {}; }
    if (DisplayPublic.GhostPhase==EMatchGhostPhase::Preparation)
    {
        const auto* GS=GetWorld() ? GetWorld()->GetGameState<ALocalMatchGameState>() : nullptr;
        const double Remaining=GS ? FMath::Max(0.0,DisplayPublic.GhostDisplayEndServerTime-GS->GetServerWorldTimeSeconds()) : 0.0;
        return FString::Printf(TEXT("幽灵棋子\n记住棋盘 · %.1f 秒\n暂时无法行动"),Remaining);
    }
    if (DisplayPublic.GhostPhase==EMatchGhostPhase::Hidden)
    { return FString::Printf(TEXT("幽灵棋子\n颜色隐藏 · 剩余 %d 次落子"),6-DisplayPublic.GhostPlacementsCompleted); }
    return {};
}

const FMatchTetrisPose* ALocalMatchPlayerController::GetDisplayTetrisPose() const
{
    return bCoherent && DisplayPublic.Session==EMatchSession::Playing && DisplayPublic.bTetrisActive && LatestTetrisPose.bActive &&
        LatestTetrisPose.Epoch==DisplayPublic.Epoch && LatestTetrisPose.BoardRevision==DisplayPublic.Revision ? &LatestTetrisPose : nullptr;
}
bool ALocalMatchPlayerController::CanSendTetrisInput() const
{
    const auto* Pose=GetDisplayTetrisPose();
    return IsPresentationReady() && Pose && (DisplayPrivate.PlayerId==Pose->OperatorPlayerId ||
        (GetNetMode()==NM_Standalone && IsLocalController()));
}
void ALocalMatchPlayerController::RequestTetrisInput(EMatchTetrisInput Input)
{
    if (!CanSendTetrisInput() || uint8(Input)>uint8(EMatchTetrisInput::Rotate)) { return; }
    const auto& Pose=LatestTetrisPose;
    ServerTetrisInput(Pose.Epoch,Pose.ActivationToken,Pose.BlockNumber,Input);
}
void ALocalMatchPlayerController::ServerTetrisInput_Implementation(uint64 Epoch, uint64 ActivationToken, int32 BlockNumber, EMatchTetrisInput Input)
{
    if (auto* MatchOwner=GetWorld()->GetAuthGameMode<ALocalMatchGameMode>())
    { MatchOwner->TetrisInputFrom(this,Epoch,ActivationToken,BlockNumber,Input); }
}
FString ALocalMatchPlayerController::TetrisStatusLabel() const
{
    if (!bCoherent || !DisplayPublic.bTetrisActive || DisplayPublic.Session!=EMatchSession::Playing) { return {}; }
    const auto* Pose=GetDisplayTetrisPose();
    if (!Pose) { return TEXT("俄罗斯方块\n正在同步方块……"); }
    static const TCHAR* Gravity[]={TEXT("↓"),TEXT("↑"),TEXT("→"),TEXT("←")};
    FString Label=FString::Printf(TEXT("俄罗斯方块\n第 %d / 6 块\n当前操作：%s\n重力方向：%s"),
        Pose->BlockNumber,*Gomokards::DemoSide(Gomokards::DemoPlayerStone(DisplayPublic,Pose->OperatorPlayerId)),Gravity[FMath::Min(uint8(3),Pose->SpawnEdge)]);
    if (CanSendTetrisInput()) { Label+=TEXT("\n方向键：移动 · 空格：旋转"); }
    return Label;
}
