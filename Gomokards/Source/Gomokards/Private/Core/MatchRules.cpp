#include "Core/MatchRules.h"
#include "Cards/CardEffects.h"

namespace Gomokards
{
static const FIntPoint Directions[] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};

FActionRequest FActionRequest::Place(FPlayerId Player, FIntPoint Coordinate)
{
    FActionRequest Request;
    Request.Player = Player;
    Request.Coordinate = Coordinate;
    return Request;
}
FActionRequest FActionRequest::Play(FPlayerId Player, ECardId Card, TOptional<FIntPoint> Target)
{
    FActionRequest Request;
    Request.Type = EActionType::PlayCard;
    Request.Player = Player;
    Request.Card = Card;
    Request.Target = Target;
    return Request;
}

bool HasWinningLine(const FBoard& Board, FIntPoint Origin, EStone Stone)
{
    if (!FBoard::Contains(Origin) || Stone == EStone::Empty || Board.At(Origin).Stone != Stone) { return false; }
    for (FIntPoint Direction : Directions)
    {
        int32 Count = 1;
        for (int32 Sign : {-1, 1})
        {
            const FIntPoint Step = Direction * Sign;
            for (FIntPoint P = Origin + Step; FBoard::Contains(P) && Board.At(P).Stone == Stone; P += Step) { ++Count; }
        }
        if (Count >= FBoard::WinLength) { return true; }
    }
    return false;
}

bool HasSuccessfulBlock(const FBoard& Board, FIntPoint Origin, EStone PlacedStone)
{
    if (!FBoard::Contains(Origin)) { return false; }
    const EStone Opponent = OppositeStone(PlacedStone);
    if (Opponent == EStone::Empty) { return false; }
    for (FIntPoint Direction : Directions)
    {
        for (int32 Sign : {-1, 1})
        {
            const FIntPoint Step = Direction * Sign;
            FIntPoint P = Origin + Step;
            if (!FBoard::Contains(P) || Board.At(P).Stone != Opponent) { continue; }
            do { P += Step; } while (FBoard::Contains(P) && Board.At(P).Stone == Opponent);
            if (!FBoard::Contains(P) || Board.At(P).Stone != EStone::Empty) { return true; }
        }
    }
    return false;
}

EActionError ValidateAction(const FMatchState& State, const FActionRequest& Request)
{
    if (State.Result.Status != EMatchStatus::InProgress) { return EActionError::MatchStopped; }
    if (SingleOpponentIndex(State, State.CurrentPlayerIndex) == INDEX_NONE
        || State.Players[0].Id == State.Players[1].Id
        || OppositeStone(State.Players[0].AssignedStone) == EStone::Empty
        || OppositeStone(State.Players[0].AssignedStone) != State.Players[1].AssignedStone)
    { return EActionError::InvalidState; }
    const auto& Actor = State.Players[State.CurrentPlayerIndex];
    if (Request.Player != Actor.Id) { return EActionError::WrongPlayer; }
    switch (Request.Type)
    {
    case EActionType::PlaceStone:
        if (!FBoard::Contains(Request.Coordinate)) { return EActionError::InvalidCoordinate; }
        if (State.Board.At(Request.Coordinate).Stone != EStone::Empty) { return EActionError::Occupied; }
        if (State.Board.At(Request.Coordinate).bForbidden) { return EActionError::Forbidden; }
        return EActionError::None;
    case EActionType::PlayCard:
    {
        const auto* Definition = FindCardDefinition(Request.Card);
        if (!Definition) { return EActionError::UnsupportedCard; }
        if (!Actor.Hand.Contains(Request.Card)) { return EActionError::CardNotOwned; }
        if (Definition->bRequiresTarget)
        {
            if (!Request.Target.IsSet() || !FBoard::Contains(Request.Target.GetValue())) { return EActionError::InvalidTarget; }
        }
        else if (Request.Target.IsSet()) { return EActionError::InvalidTarget; }
        return EActionError::None;
    }
    default:
        return EActionError::UnsupportedAction;
    }
}

static bool HasLegalAction(const FMatchState& State)
{
    for (const FCell& Cell : State.Board.Cells)
    {
        if (Cell.Stone == EStone::Empty && !Cell.bForbidden) { return true; }
    }
    for (ECardId Card : State.Players[State.CurrentPlayerIndex].Hand)
    {
        // All four Phase 1 cards are playable on a full board (including empty-hand Steal).
        if (FindCardDefinition(Card)) { return true; }
    }
    return false;
}

FActionResult ResolveAction(FMatchState& State, const FActionRequest& Request)
{
    FActionResult Result{ValidateAction(State, Request), false};
    if (!Result.IsAccepted()) { return Result; }
    FMatchState Candidate = State; // 361 cells: simple atomicity, including RNG rollback.
    const int32 ActorIndex = Candidate.CurrentPlayerIndex;
    auto& Actor = Candidate.Players[ActorIndex];
    if (Request.Type == EActionType::PlaceStone)
    {
        const EStone PlacedStone = EffectivePlacementStone(Actor);
        Candidate.Board.At(Request.Coordinate).Stone = PlacedStone;
        if (HasWinningLine(Candidate.Board, Request.Coordinate, PlacedStone))
        { Candidate.Result = {EMatchStatus::Won, PlacedStone, EDecisionReason::None}; }
        Result.bBlockingReward = HasSuccessfulBlock(Candidate.Board, Request.Coordinate, PlacedStone);
        if (Result.bBlockingReward) { Actor.Hand.Add(DrawCard(Candidate.Random)); }
    }
    else
    {
        Actor.Hand.RemoveAt(Actor.Hand.Find(Request.Card));
        if (!ExecuteCardEffect(Candidate, ActorIndex, Request.Card, Request.Target))
        { return {EActionError::UnsupportedCard, false}; }
    }
    ++Candidate.CompletedActions;
    if (Candidate.Result.Status == EMatchStatus::InProgress)
    {
        Candidate.CurrentPlayerIndex = SingleOpponentIndex(Candidate, ActorIndex);
        if (!HasLegalAction(Candidate))
        {
            // Explicitly isolate the unresolved rule; never invent a draw/pass/winner.
            Candidate.Result = {EMatchStatus::AwaitingRuleDecision, EStone::Empty, EDecisionReason::NoLegalAction};
        }
    }
    State = MoveTemp(Candidate);
    return Result;
}
}
