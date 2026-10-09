#pragma once
#include "GameFramework/GameModeBase.h"
#include "Singleplayer/SingleplayerPresentation.h"
#include "TimerManager.h"
#include "SingleplayerBattleGameMode.generated.h"

UCLASS()
class GOMOKARDS_API ASingleplayerBattleGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ASingleplayerBattleGameMode();
    virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    const Gomokards::FSingleplayerBattleState& GetBattle() const { return Battle; }
    Gomokards::FSingleplayerView GetPresentation() const;
    const TArray<FString>& GetLog() const { return Log; }
    Gomokards::FSingleplayerActionResult SubmitPlayer(const Gomokards::FSingleplayerActionRequest& Request, uint64 ExpectedGeneration);
    void RestartBattle(); // Same initialization seed for reproducible retries.
    // The timer and deterministic runtime tests use this same guarded boundary.
    bool ExecutePendingAI(uint64 ExpectedGeneration, uint64 ExpectedActions);
    FSimpleMulticastDelegate OnBattleChanged;
private:
    Gomokards::FSingleplayerBattleState Battle;
    int32 BattleSeed = 0;
    uint64 Generation = 0;
    bool bReady = false;
    bool bAIPending = false;
    FTimerHandle AITimer;
    TArray<FString> Log;
    void AddLog(FString Line);
    void ScheduleAI();
};
