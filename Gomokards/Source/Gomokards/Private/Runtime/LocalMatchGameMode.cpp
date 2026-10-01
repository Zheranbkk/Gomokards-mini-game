#include "Runtime/LocalMatchGameMode.h"
#include "Runtime/LocalMatchPlayerController.h"
#include "Runtime/LocalMatchGameState.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Guid.h"
#include "HAL/PlatformTime.h"

ALocalMatchGameMode::ALocalMatchGameMode()
{
    DefaultPawnClass = nullptr;
    GameStateClass = ALocalMatchGameState::StaticClass();
    PlayerControllerClass = ALocalMatchPlayerController::StaticClass();
}
void ALocalMatchGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);
    bSessionInitialized=true;
    bStandaloneSession=GetNetMode()==NM_Standalone;
    if (UGameplayStatics::HasOption(Options, TEXT("Seed")))
    { StartWithSeed(UGameplayStatics::GetIntOption(Options, TEXT("Seed"), 0)); }
    else { NewMatch(); }
}
void ALocalMatchGameMode::NewMatch()
{
    const FGuid SeedGuid = FGuid::NewGuid();
    int32 Seed = static_cast<int32>(SeedGuid.A ^ SeedGuid.B ^ SeedGuid.C ^ SeedGuid.D);
    if (Seed == 0 || Seed == Match.Random.GetInitialSeed()) { Seed = Match.Random.GetInitialSeed() ^ 0x5a179b3d; }
    StartWithSeed(Seed);
}
void ALocalMatchGameMode::StartWithSeed(int32 Seed)
{
    CancelGhostPreparation();
    CancelTetrisGravity();
    Match.Reset(Seed);
    ++MatchEpoch; Revision=0;
    PublishViews();
    OnMatchChanged.Broadcast();
}
Gomokards::FActionResult ALocalMatchGameMode::Submit(const Gomokards::FActionRequest& Request)
{
    if (bSessionInitialized && !bStandaloneSession && Request.Type==Gomokards::EActionType::PlayCard && !IsNetworkCardEnabled(uint8(Request.Card)) && !IsTargetedNetworkCardEnabled(uint8(Request.Card)))
    { return {Gomokards::EActionError::UnsupportedAction, false}; }
    const auto Result = Gomokards::ResolveAction(Match, Request);
    if (Result.IsAccepted())
    {
        if (Match.GhostPhase == Gomokards::EGhostPhase::Preparation) { ScheduleGhostPreparation(); }
        if (Match.Tetris.bActive) { ScheduleTetrisGravity(); }
        PublishViews();
        OnMatchChanged.Broadcast();
    }
    return Result;
}

void ALocalMatchGameMode::ScheduleGhostPreparation()
{
    CancelGhostPreparation();
    GhostDeadline = FPlatformTime::Seconds() + 5.0;
    const uint64 Generation = GhostTimerGeneration;
    // Core ticker + monotonic deadline: five real seconds, unaffected by world time dilation/pause.
    GhostTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,
        [this, Generation](float) { return PollGhostPreparation(FPlatformTime::Seconds(), Generation); }));
}
void ALocalMatchGameMode::CancelGhostPreparation()
{
    FTSTicker::RemoveTicker(GhostTicker);
    GhostTicker.Reset();
    GhostDeadline = 0;
    ++GhostTimerGeneration;
}
bool ALocalMatchGameMode::PollGhostPreparation(double Now, uint64 Generation)
{
    if (Generation != GhostTimerGeneration) { return false; }
    if (Now < GhostDeadline) { return true; }
    CancelGhostPreparation();
    if (Gomokards::BeginGhostHidden(Match)) { PublishViews(); OnMatchChanged.Broadcast(); }
    return false;
}
double ALocalMatchGameMode::GhostPreparationSecondsRemaining() const
{
    return Match.GhostPhase == Gomokards::EGhostPhase::Preparation ? FMath::Max(0.0, GhostDeadline-FPlatformTime::Seconds()) : 0.0;
}
void ALocalMatchGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
    CancelGhostPreparation();
    CancelTetrisGravity();
    Super::EndPlay(Reason);
}
void ALocalMatchGameMode::BeginDestroy()
{
    CancelGhostPreparation();
    CancelTetrisGravity();
    Super::BeginDestroy();
}

void ALocalMatchGameMode::ScheduleTetrisGravity()
{
    CancelTetrisGravity();
    TetrisDeadline=FPlatformTime::Seconds()+0.5;
    const uint64 Generation=TetrisTimerGeneration;
    TetrisTicker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,
        [this,Generation](float) { return PollTetrisGravity(FPlatformTime::Seconds(),Generation); }));
}
void ALocalMatchGameMode::CancelTetrisGravity()
{
    FTSTicker::RemoveTicker(TetrisTicker);
    TetrisTicker.Reset(); TetrisDeadline=0; ++TetrisTimerGeneration;
}
bool ALocalMatchGameMode::PollTetrisGravity(double Now, uint64 Generation)
{
    if (Generation!=TetrisTimerGeneration) { return false; }
    if (!Match.Tetris.bActive) { CancelTetrisGravity(); return false; }
    if (Now<TetrisDeadline) { return true; }
    const bool bChanged=Gomokards::StepTetrisGravity(Match);
    if (Match.Tetris.bActive)
    {
        // One step per deadline, never a burst of catch-up locks after a stalled/suspended process.
        TetrisDeadline+=0.5;
        if (TetrisDeadline<=Now) { TetrisDeadline=Now+0.5; }
    }
    else { CancelTetrisGravity(); }
    if (bChanged) { OnMatchChanged.Broadcast(); }
    return Generation==TetrisTimerGeneration && Match.Tetris.bActive;
}
bool ALocalMatchGameMode::SubmitTetris(Gomokards::ETetrisInput Input)
{ return SubmitTetrisAt(Input,FPlatformTime::Seconds()); }
bool ALocalMatchGameMode::SubmitTetrisAt(Gomokards::ETetrisInput Input, double Now)
{
    const bool bSoftDrop=Gomokards::TetrisTranslation(Input)==Gomokards::TetrisGravity(Match.Tetris.Edge);
    if (!Gomokards::ApplyTetrisInput(Match,Input)) { return false; }
    if (bSoftDrop) { TetrisDeadline=Now+0.5; }
    OnMatchChanged.Broadcast();
    return true;
}


void ALocalMatchGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
    if (auto* PC=Cast<ALocalMatchPlayerController>(NewPlayer))
    {
        // Permission is server granted, independent of seat, never supplied by an RPC.
        Join(PC, PC->IsLocalController() && GetNetMode()!=NM_DedicatedServer);
    }
}
void ALocalMatchGameMode::Logout(AController* Exiting)
{
    if (auto* PC=Cast<ALocalMatchPlayerController>(Exiting)) { Leave(PC); }
    Super::Logout(Exiting);
}
void ALocalMatchGameMode::Join(ALocalMatchPlayerController* Controller, bool bGrantAdmin)
{
    if (!Controller || Assignments.Contains(Controller)) { return; }
    if (Assignments.Num()>=2 || Session==EMatchSession::SessionEnded)
    {
        Controller->PublishPrivate(MakePrivateView(Match,INDEX_NONE,false,MatchEpoch,Revision));
        return; // Deliberately unassigned; no spectator/reconnect protocol.
    }
    TArray<int32> Used; Assignments.GenerateValueArray(Used);
    for (const auto& Player : Match.Players)
    {
        if (!Used.Contains(Player.Id)) { Assignments.Add(Controller,Player.Id); break; }
    }
    if (bGrantAdmin) { DevelopmentAdmins.Add(Controller); }
    if (bStandaloneSession || Assignments.Num()==2)
    {
        Session=EMatchSession::Playing;
        // Preserve an explicit development seed while resetting board/hands for both seats.
        StartWithSeed(Match.Random.GetInitialSeed());
    }
    else { PublishViews(); }
}
void ALocalMatchGameMode::Leave(ALocalMatchPlayerController* Controller)
{
    DevelopmentAdmins.Remove(Controller);
    if (Assignments.Remove(Controller)==0) { return; }
    Session=EMatchSession::SessionEnded;
    CancelGhostPreparation(); CancelTetrisGravity();
    PublishViews();
}
FMatchActionAck ALocalMatchGameMode::Acknowledgement(EMatchIntentError Error) const
{
    FMatchActionAck Ack;
    Ack.Epoch=MatchEpoch; Ack.Revision=Revision; Ack.Error=Error;
    Ack.bAccepted=Error==EMatchIntentError::None;
    return Ack;
}
FMatchActionAck ALocalMatchGameMode::PlaceFrom(ALocalMatchPlayerController* Controller, uint64 Epoch, uint64 ExpectedActions, FIntPoint Point)
{
    const int32* Assigned=Assignments.Find(Controller);
    if (!Assigned) { return Acknowledgement(EMatchIntentError::Unassigned); }
    if (Session!=EMatchSession::Playing) { return Acknowledgement(EMatchIntentError::NotPlaying); }
    if (Epoch!=MatchEpoch) { return Acknowledgement(EMatchIntentError::StaleEpoch); }
    if (ExpectedActions!=Match.CompletedActions) { return Acknowledgement(EMatchIntentError::StaleAction); }
    // Only an explicitly initialized standalone world permits hot-seat operation.
    const int32 Actor=bStandaloneSession && GetNetMode()==NM_Standalone && Controller->IsLocalController()
        ? Match.Players[Match.CurrentPlayerIndex].Id : *Assigned;
    const auto Result=Submit(Gomokards::FActionRequest::Place(Actor,Point));
    auto Ack=Acknowledgement(Result.IsAccepted() ? EMatchIntentError::None : EMatchIntentError::RuleRejected);
    Ack.RuleError=static_cast<uint8>(Result.Error); Ack.bBlockingReward=Result.bBlockingReward;
    return Ack;
}
FMatchActionAck ALocalMatchGameMode::CardFrom(ALocalMatchPlayerController* Controller, uint64 Epoch, uint64 ExpectedActions, uint8 CardId)
{
    const int32* Assigned=Assignments.Find(Controller);
    if (!Assigned) { return Acknowledgement(EMatchIntentError::Unassigned); }
    if (Session!=EMatchSession::Playing) { return Acknowledgement(EMatchIntentError::NotPlaying); }
    if (Epoch!=MatchEpoch) { return Acknowledgement(EMatchIntentError::StaleEpoch); }
    if (ExpectedActions!=Match.CompletedActions) { return Acknowledgement(EMatchIntentError::StaleAction); }
    // Reject both valid later-phase cards and unknown byte values before entering the core.
    if (!IsNetworkCardEnabled(CardId)) { return Acknowledgement(EMatchIntentError::CardNotNetworkEnabled); }
    const int32 Actor=bStandaloneSession && GetNetMode()==NM_Standalone && Controller->IsLocalController()
        ? Match.Players[Match.CurrentPlayerIndex].Id : *Assigned;
    const auto Result=Submit(Gomokards::FActionRequest::Play(Actor,static_cast<Gomokards::ECardId>(CardId)));
    auto Ack=Acknowledgement(Result.IsAccepted() ? EMatchIntentError::None : EMatchIntentError::RuleRejected);
    Ack.RuleError=static_cast<uint8>(Result.Error); Ack.bBlockingReward=Result.bBlockingReward;
    return Ack;
}
FMatchActionAck ALocalMatchGameMode::TargetedCardFrom(ALocalMatchPlayerController* Controller, uint64 Epoch, uint64 ExpectedActions, uint8 CardId, FIntPoint Target)
{
    const int32* Assigned=Assignments.Find(Controller);
    if (!Assigned) { return Acknowledgement(EMatchIntentError::Unassigned); }
    if (Session!=EMatchSession::Playing) { return Acknowledgement(EMatchIntentError::NotPlaying); }
    if (Epoch!=MatchEpoch) { return Acknowledgement(EMatchIntentError::StaleEpoch); }
    if (ExpectedActions!=Match.CompletedActions) { return Acknowledgement(EMatchIntentError::StaleAction); }
    if (!IsTargetedNetworkCardEnabled(CardId)) { return Acknowledgement(EMatchIntentError::CardNotNetworkEnabled); }
    const int32 Actor=bStandaloneSession && GetNetMode()==NM_Standalone && Controller->IsLocalController()
        ? Match.Players[Match.CurrentPlayerIndex].Id : *Assigned;
    const auto Result=Submit(Gomokards::FActionRequest::Play(Actor,static_cast<Gomokards::ECardId>(CardId),Target));
    auto Ack=Acknowledgement(Result.IsAccepted() ? EMatchIntentError::None : EMatchIntentError::RuleRejected);
    Ack.RuleError=static_cast<uint8>(Result.Error); Ack.bBlockingReward=Result.bBlockingReward;
    return Ack;
}
FMatchActionAck ALocalMatchGameMode::RestartFrom(ALocalMatchPlayerController* Controller, uint64 Epoch)
{
    if (!Assignments.Contains(Controller) || !DevelopmentAdmins.Contains(Controller))
    { return Acknowledgement(EMatchIntentError::Unauthorized); }
    if (Epoch!=MatchEpoch) { return Acknowledgement(EMatchIntentError::StaleEpoch); }
    // A disconnected session cannot be revived into an unsupported reconnect flow.
    if (Session!=EMatchSession::Playing) { return Acknowledgement(EMatchIntentError::NotPlaying); }
    NewMatch();
    return Acknowledgement(EMatchIntentError::None);
}
void ALocalMatchGameMode::PublishViews()
{
    ++Revision;
    TArray<int32> Occupied; Assignments.GenerateValueArray(Occupied);
    if (bStandaloneSession && !Assignments.IsEmpty())
    { Occupied.Reset(); for (const auto& Player : Match.Players) { Occupied.Add(Player.Id); } }
    // End only when this slice exposes no remaining action; never invent a core draw.
    if (bSessionInitialized && Session==EMatchSession::Playing && Match.Result.Status==Gomokards::EMatchStatus::InProgress
        && Match.GhostPhase!=Gomokards::EGhostPhase::Preparation)
    {
        bool bPlaceExists=false;
        for (const auto& Cell : Match.Board.Cells)
        { if (Cell.Stone==Gomokards::EStone::Empty && !Cell.bForbidden) { bPlaceExists=true; break; } }
        const bool bCardExists=Gomokards::CanPlayCards(Match) && Match.Players[Match.CurrentPlayerIndex].Hand.ContainsByPredicate(
            [](Gomokards::ECardId Card){return IsNetworkCardEnabled(uint8(Card)) || IsTargetedNetworkCardEnabled(uint8(Card));});
        if (!bPlaceExists && !bCardExists) { Session=EMatchSession::SessionEnded; CancelGhostPreparation(); CancelTetrisGravity(); }
    }
    if (auto* GS=Cast<ALocalMatchGameState>(GameState))
    {
        auto Public=MakePublicView(Match,Occupied,Session,MatchEpoch,Revision);
        if (Match.GhostPhase==Gomokards::EGhostPhase::Preparation)
        { Public.GhostDisplayEndServerTime=GS->GetServerWorldTimeSeconds()+GhostPreparationSecondsRemaining(); }
        GS->Publish(Public);
    }
    for (const auto& Pair : Assignments)
    {
        if (auto* PC=Pair.Key.Get())
        {
            const int32 ViewedPlayer=bStandaloneSession ? Match.Players[Match.CurrentPlayerIndex].Id : Pair.Value;
            PC->PublishPrivate(MakePrivateView(Match,ViewedPlayer,DevelopmentAdmins.Contains(PC),MatchEpoch,Revision));
        }
    }
}
