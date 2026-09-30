#include "Runtime/LocalMatchGameMode.h"
#include "Runtime/LocalMatchPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Guid.h"
#include "HAL/PlatformTime.h"

ALocalMatchGameMode::ALocalMatchGameMode()
{
    DefaultPawnClass = nullptr;
    PlayerControllerClass = ALocalMatchPlayerController::StaticClass();
}
void ALocalMatchGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);
    if (UGameplayStatics::HasOption(Options, TEXT("Seed")))
    { StartWithSeed(UGameplayStatics::GetIntOption(Options, TEXT("Seed"), 0)); }
    else { NewMatch(); }
}
void ALocalMatchGameMode::NewMatch()
{
    const FGuid Session = FGuid::NewGuid();
    int32 Seed = static_cast<int32>(Session.A ^ Session.B ^ Session.C ^ Session.D);
    if (Seed == 0 || Seed == Match.Random.GetInitialSeed()) { Seed = Match.Random.GetInitialSeed() ^ 0x5a179b3d; }
    StartWithSeed(Seed);
}
void ALocalMatchGameMode::StartWithSeed(int32 Seed)
{
    CancelGhostPreparation();
    CancelTetrisGravity();
    Match.Reset(Seed);
    OnMatchChanged.Broadcast();
}
Gomokards::FActionResult ALocalMatchGameMode::Submit(const Gomokards::FActionRequest& Request)
{
    const auto Result = Gomokards::ResolveAction(Match, Request);
    if (Result.IsAccepted())
    {
        if (Match.GhostPhase == Gomokards::EGhostPhase::Preparation) { ScheduleGhostPreparation(); }
        if (Match.Tetris.bActive) { ScheduleTetrisGravity(); }
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
    if (Gomokards::BeginGhostHidden(Match)) { OnMatchChanged.Broadcast(); }
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
