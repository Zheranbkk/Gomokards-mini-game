#include "Legacy/LegacyRules.h"
#include "Legacy/LegacyCardEffects.h"
namespace Gomokards
{
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

bool CanPlayCards(const FMatchState& State)
{
    return State.Result.Status == EMatchStatus::InProgress && !State.bCardsDisabled && State.GhostPhase == EGhostPhase::None && !State.Tetris.bActive;
}

EActionError ValidateAction(const FMatchState& State, const FActionRequest& Request)
{
    if (State.Result.Status != EMatchStatus::InProgress) { return EActionError::MatchStopped; }
    if (State.Tetris.bActive) { return EActionError::TetrisActive; }
    if (State.GhostPhase == EGhostPhase::Preparation) { return EActionError::GhostPreparation; }
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
        if (State.bCardsDisabled) { return EActionError::CardsDisabled; }
        if (State.GhostPhase == EGhostPhase::Hidden) { return EActionError::GhostCardsRestricted; }
        const auto* Definition = FindLegacyCardDefinition(Request.Card);
        if (!Definition) { return EActionError::UnsupportedCard; }
        if (!Actor.Hand.Contains(Request.Card)) { return EActionError::CardNotOwned; }
        if (Definition->RequiresTarget())
        {
            if (!Request.Target.IsSet()) { return EActionError::InvalidTarget; }
            const FIntPoint Target = Request.Target.GetValue();
            const bool bValid = Definition->Target == ECardTarget::Intersection
                ? FBoard::Contains(Target) : FBoard::ContainsAnchor(Target);
            if (!bValid) { return EActionError::InvalidTarget; }
        }
        else if (Request.Target.IsSet()) { return EActionError::InvalidTarget; }
        return EActionError::None;
    }
    default:
        return EActionError::UnsupportedAction;
    }
}

bool HasLegalAction(const FMatchState& State)
{
    for (const FCell& Cell : State.Board.Cells)
    {
        if (Cell.Stone == EStone::Empty && !Cell.bForbidden) { return true; }
    }
    if (!CanPlayCards(State)) { return false; }
    for (ECardId Card : State.Players[State.CurrentPlayerIndex].Hand)
    {
        // All current cards have a legal target/effect even on a full board, including repeat Barriers.
        if (FindLegacyCardDefinition(Card)) { return true; }
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
        const EStone PlacedStone = EffectivePlacementStone(Actor, State.ConfusionActionsRemaining > 0);
        const auto Placement = TryPlaceStone(Candidate.Board, Request.Coordinate, PlacedStone);
        check(Placement.IsAccepted());
        if (State.GhostPhase != EGhostPhase::Hidden && Placement.bWinningLine)
        { Candidate.Result = {EMatchStatus::Won, PlacedStone, EDecisionReason::None}; }
        Result.bBlockingReward = Placement.bSuccessfulBlock;
        if (Result.bBlockingReward) { Actor.Hand.Add(DrawCard(Candidate.Random)); }
    }
    else
    {
        Actor.Hand.RemoveAt(Actor.Hand.Find(Request.Card));
        if (!ExecuteCardEffect(Candidate, ActorIndex, Request.Card, Request.Target))
        { return {EActionError::UnsupportedCard, false}; }
    }
    // Only a successful resolution consumes the effect that was active on entry.
    // A recast refreshes two future actions; Basics explicitly clears the persistent rule effect.
    Candidate.ConfusionActionsRemaining = FMath::Max(0, State.ConfusionActionsRemaining-1);
    if (Request.Type == EActionType::PlayCard)
    {
        if (Request.Card == ECardId::Confusion) { Candidate.ConfusionActionsRemaining = 2; }
        if (Request.Card == ECardId::BackToBasics) { Candidate.ConfusionActionsRemaining = 0; }
        if (Request.Card == ECardId::Polarity)
        { Candidate.Result = EvaluateBoardResult(Candidate.Board); }
    }
    if (Request.Type == EActionType::PlaceStone && State.GhostPhase == EGhostPhase::Hidden)
    {
        ++Candidate.GhostPlacementsCompleted;
        if (Candidate.GhostPlacementsCompleted == FMatchState::GhostPlacementLimit)
        {
            Candidate.GhostPhase = EGhostPhase::None;
            Candidate.GhostPlacementsCompleted = 0;
            Candidate.Result = EvaluateBoardResult(Candidate.Board);
        }
    }
    ++Candidate.CompletedActions;
    if (Candidate.Result.Status == EMatchStatus::InProgress)
    {
        Candidate.CurrentPlayerIndex = SingleOpponentIndex(Candidate, ActorIndex);
        if (Request.Type == EActionType::PlayCard && Request.Card == ECardId::Tetris) { BeginTetris(Candidate); }
        if (Candidate.Result.Status == EMatchStatus::InProgress && !Candidate.Tetris.bActive
            && Candidate.GhostPhase != EGhostPhase::Preparation && !HasLegalAction(Candidate))
        {
            // Explicitly isolate the unresolved rule; never invent a draw/pass/winner.
            Candidate.Result = {EMatchStatus::AwaitingRuleDecision, EStone::Empty, EDecisionReason::NoLegalAction};
        }
    }
    State = MoveTemp(Candidate);
    return Result;
}

}
