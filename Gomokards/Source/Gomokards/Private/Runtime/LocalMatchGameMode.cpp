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
    Match.Reset(Seed);
    OnMatchChanged.Broadcast();
}
Gomokards::FActionResult ALocalMatchGameMode::Submit(const Gomokards::FActionRequest& Request)
{
    const auto Result = Gomokards::ResolveAction(Match, Request);
    if (Result.IsAccepted())
    {
        if (Match.GhostPhase == Gomokards::EGhostPhase::Preparation) { ScheduleGhostPreparation(); }
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
    Super::EndPlay(Reason);
}
void ALocalMatchGameMode::BeginDestroy()
{
    CancelGhostPreparation();
    Super::BeginDestroy();
}
