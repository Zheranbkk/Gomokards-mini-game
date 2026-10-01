#pragma once

#include "GameFramework/GameModeBase.h"
#include "Core/MatchRules.h"
#include "Core/TetrisRules.h"
#include "Containers/Ticker.h"
#include "Runtime/MatchNetTypes.h"
#include "LocalMatchGameMode.generated.h"

class ALocalMatchPlayerController;

DECLARE_MULTICAST_DELEGATE(FLocalMatchChanged);

UCLASS()
class GOMOKARDS_API ALocalMatchGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ALocalMatchGameMode();
    virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;
    FMatchActionAck PlaceFrom(ALocalMatchPlayerController* Controller, uint64 Epoch, uint64 ExpectedActions, FIntPoint Point);
    FMatchActionAck CardFrom(ALocalMatchPlayerController* Controller, uint64 Epoch, uint64 ExpectedActions, uint8 CardId);
    FMatchActionAck TargetedCardFrom(ALocalMatchPlayerController* Controller, uint64 Epoch, uint64 ExpectedActions, uint8 CardId, FIntPoint Target);
    bool TetrisInputFrom(ALocalMatchPlayerController* Controller, uint64 Epoch, uint64 ActivationToken, int32 BlockNumber, EMatchTetrisInput Input);
    FMatchActionAck RestartFrom(ALocalMatchPlayerController* Controller, uint64 Epoch);
    const Gomokards::FMatchState& GetMatch() const { return Match; }
    Gomokards::FActionResult Submit(const Gomokards::FActionRequest& Request);
    void NewMatch();
    void StartWithSeed(int32 Seed); // Explicit reproducible session; never changes rules RNG policy.
    FLocalMatchChanged OnMatchChanged;
    double GhostPreparationSecondsRemaining() const;
    bool SubmitTetris(Gomokards::ETetrisInput Input);
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void BeginDestroy() override;
private:
    Gomokards::FMatchState Match;
    TMap<TWeakObjectPtr<ALocalMatchPlayerController>, int32> Assignments;
    TSet<TWeakObjectPtr<ALocalMatchPlayerController>> DevelopmentAdmins;
    EMatchSession Session = EMatchSession::WaitingForPlayers;
    uint64 MatchEpoch = 0;
    uint64 Revision = 0;
    bool bStandaloneSession = false;
    bool bSessionInitialized = false;
    void Join(ALocalMatchPlayerController* Controller, bool bGrantAdmin);
    void Leave(ALocalMatchPlayerController* Controller);
    void PublishViews();
    void PublishTetrisPose();
    uint64 TetrisActivationToken = 0;
    uint64 TetrisPoseSequence = 0;
    FMatchActionAck Acknowledgement(EMatchIntentError Error) const;
    friend class FMatchNetworkTest;
    friend class FBasicCardNetworkTest;
    friend class FTargetedCardNetworkTest;
    friend class FPersistentCardNetworkTest;
    friend class FGhostNetworkTest;
    friend class FTetrisNetworkTest;
    void ScheduleGhostPreparation();
    void CancelGhostPreparation();
    bool PollGhostPreparation(double Now, uint64 Generation);
    FTSTicker::FDelegateHandle GhostTicker;
    double GhostDeadline = 0;
    uint64 GhostTimerGeneration = 0;
    friend class FGhostRuntimeTest; // Deterministic runtime timer coverage; no public state injection API.
    void ScheduleTetrisGravity();
    void CancelTetrisGravity();
    bool PollTetrisGravity(double Now, uint64 Generation);
    bool SubmitTetrisAt(Gomokards::ETetrisInput Input, double Now);
    FTSTicker::FDelegateHandle TetrisTicker;
    double TetrisDeadline = 0;
    uint64 TetrisTimerGeneration = 0;
    friend class FTetrisRuntimeTest;
};
