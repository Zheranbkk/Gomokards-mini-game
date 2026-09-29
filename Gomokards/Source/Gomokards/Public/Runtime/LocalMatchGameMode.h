#pragma once

#include "GameFramework/GameModeBase.h"
#include "Core/MatchRules.h"
#include "Containers/Ticker.h"
#include "LocalMatchGameMode.generated.h"

DECLARE_MULTICAST_DELEGATE(FLocalMatchChanged);

UCLASS()
class GOMOKARDS_API ALocalMatchGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ALocalMatchGameMode();
    virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
    const Gomokards::FMatchState& GetMatch() const { return Match; }
    Gomokards::FActionResult Submit(const Gomokards::FActionRequest& Request);
    void NewMatch();
    void StartWithSeed(int32 Seed); // Explicit reproducible session; never changes rules RNG policy.
    FLocalMatchChanged OnMatchChanged;
    double GhostPreparationSecondsRemaining() const;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void BeginDestroy() override;
private:
    Gomokards::FMatchState Match;
    void ScheduleGhostPreparation();
    void CancelGhostPreparation();
    bool PollGhostPreparation(double Now, uint64 Generation);
    FTSTicker::FDelegateHandle GhostTicker;
    double GhostDeadline = 0;
    uint64 GhostTimerGeneration = 0;
    friend class FGhostRuntimeTest; // Deterministic runtime timer coverage; no public state injection API.
};
