#pragma once
// Editor-only regression fixture policy. Never use as SP gameplay API.
#include "Core/MatchRules.h"
#include "Core/TetrisRules.h"
namespace Gomokards
{
enum class EActionType : uint8 { PlaceStone, PlayCard };
enum class EActionError : uint8
{
    None, MatchStopped, InvalidState, WrongPlayer, UnsupportedAction,
    InvalidCoordinate, Occupied, Forbidden, UnsupportedCard, CardNotOwned, InvalidTarget, CardsDisabled, GhostPreparation, GhostCardsRestricted, TetrisActive
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

const FCardDefinition* FindLegacyCardDefinition(ECardId Id);
TConstArrayView<FCardDefinition> GetPlayableCards();
bool CanPlayCards(const FMatchState& State);
bool HasLegalAction(const FMatchState& State);
EActionError ValidateAction(const FMatchState& State, const FActionRequest& Request);
FActionResult ResolveAction(FMatchState& State, const FActionRequest& Request);
bool BeginTetris(FMatchState& State);
bool StepTetrisGravity(FMatchState& State);
}
