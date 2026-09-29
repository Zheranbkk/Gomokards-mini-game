#pragma once

#include "GameFramework/GameModeBase.h"
#include "Core/MatchRules.h"
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
private:
    Gomokards::FMatchState Match;
};
