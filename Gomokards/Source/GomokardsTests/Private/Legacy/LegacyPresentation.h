#pragma once
#include "Presentation/MatchPresentation.h"
#include "Legacy/LegacyRules.h"
namespace Gomokards
{
// Only local intent: no board, hands, result or RNG copy lives here.
struct FTargetSelection
{
    ECardId Card = ECardId::Invalid;
    FPlayerId Player = INDEX_NONE;
    bool IsActive() const { return Card != ECardId::Invalid; }
    void Clear() { Card = ECardId::Invalid; Player = INDEX_NONE; }
    bool Toggle(const FMatchState& State, ECardId Selected);
    FActionRequest BoardRequest(const FMatchState& State, FIntPoint Target) const;
};

FString GhostLabel(const FMatchState& State, double PreparationSeconds);
FString TetrisLabel(const FMatchState& State);
FString CardLabel(ECardId Card);
FString StoneLabel(EStone Stone);
FString EffectLabel(const FMatchState& State);
FString TargetingLabel(ECardId Selected);
FString ResultLabel(const FMatchState& State);
FString RejectionLabel(EActionError Error);
}
