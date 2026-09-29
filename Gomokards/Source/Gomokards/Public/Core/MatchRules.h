#pragma once

#include "Core/MatchState.h"

namespace Gomokards
{
enum class EActionType : uint8 { PlaceStone, PlayCard };
enum class EActionError : uint8
{
    None, MatchStopped, InvalidState, WrongPlayer, UnsupportedAction,
    InvalidCoordinate, Occupied, Forbidden, UnsupportedCard, CardNotOwned, InvalidTarget, CardsDisabled, GhostPreparation, GhostCardsRestricted
};

struct FActionRequest
{
    EActionType Type = EActionType::PlaceStone;
    FPlayerId Player = INDEX_NONE;
    FIntPoint Coordinate = FIntPoint::ZeroValue;
    ECardId Card = ECardId::Invalid;
    TOptional<FIntPoint> Target;

    static FActionRequest Place(FPlayerId Player, FIntPoint Coordinate);
    static FActionRequest Play(FPlayerId Player, ECardId Card, TOptional<FIntPoint> Target = {});
};

struct FActionResult
{
    EActionError Error = EActionError::None;
    bool bBlockingReward = false;
    bool IsAccepted() const { return Error == EActionError::None; }
};

GOMOKARDS_API bool HasWinningLine(const FBoard& Board, FIntPoint Origin, EStone Stone);
// Runtime timer invokes this explicit deterministic transition; never a player action.
GOMOKARDS_API bool BeginGhostHidden(FMatchState& State);
GOMOKARDS_API bool CanPlayCards(const FMatchState& State);
GOMOKARDS_API FMatchResult EvaluateBoardResult(const FBoard& Board);
// Exact legacy post-placement predicate; deliberately ignores forbidden metadata/history.
GOMOKARDS_API bool HasSuccessfulBlock(const FBoard& Board, FIntPoint Origin, EStone PlacedStone);
GOMOKARDS_API EActionError ValidateAction(const FMatchState& State, const FActionRequest& Request);
GOMOKARDS_API FActionResult ResolveAction(FMatchState& State, const FActionRequest& Request);
}
