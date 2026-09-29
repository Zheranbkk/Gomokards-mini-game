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
            for (FIntPoint P = Origin + Step; FBoard::Contains(P) && Board.At(P).Stone == Stone; P += Step)
            {
                if (Board.IsLinkBlocked(P-Step, P)) { break; }
                ++Count;
            }
        }
        if (Count >= FBoard::WinLength) { return true; }
    }
    return false;
}

FMatchResult EvaluateBoardResult(const FBoard& Board)
{
    bool bBlack = false, bWhite = false;
    for (int32 I=0; I<FBoard::Size*FBoard::Size; ++I)
    {
        const EStone Stone = Board.Cells[I].Stone;
        if (HasWinningLine(Board,FBoard::ToCoordinate(I),Stone))
        { bBlack |= Stone == EStone::Black; bWhite |= Stone == EStone::White; }
    }
    if (bBlack && bWhite) { return {EMatchStatus::Draw, EStone::Empty}; }
    if (bBlack || bWhite) { return {EMatchStatus::Won, bBlack ? EStone::Black : EStone::White}; }
    return {};
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

bool BeginGhostHidden(FMatchState& State)
{
    if (State.Result.Status != EMatchStatus::InProgress || State.GhostPhase != EGhostPhase::Preparation) { return false; }
    State.GhostPhase = EGhostPhase::Hidden;
    State.GhostPlacementsCompleted = 0;
    return true;
}
bool CanPlayCards(const FMatchState& State)
{
    return State.Result.Status == EMatchStatus::InProgress && !State.bCardsDisabled && State.GhostPhase == EGhostPhase::None;
}

EActionError ValidateAction(const FMatchState& State, const FActionRequest& Request)
{
    if (State.Result.Status != EMatchStatus::InProgress) { return EActionError::MatchStopped; }
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
        const auto* Definition = FindCardDefinition(Request.Card);
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

static bool HasLegalAction(const FMatchState& State)
{
    for (const FCell& Cell : State.Board.Cells)
    {
        if (Cell.Stone == EStone::Empty && !Cell.bForbidden) { return true; }
    }
    if (!CanPlayCards(State)) { return false; }
    for (ECardId Card : State.Players[State.CurrentPlayerIndex].Hand)
    {
        // All current cards have a legal target/effect even on a full board, including repeat Barriers.
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
        const EStone PlacedStone = EffectivePlacementStone(Actor, State.ConfusionActionsRemaining > 0);
        Candidate.Board.At(Request.Coordinate).Stone = PlacedStone;
        if (State.GhostPhase != EGhostPhase::Hidden && HasWinningLine(Candidate.Board, Request.Coordinate, PlacedStone))
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
    // Only a successful resolution consumes the effect that was active on entry.
    // A recast then replaces the old duration with two future actions. Basics uses this normal lifecycle.
    Candidate.ConfusionActionsRemaining = FMath::Max(0, State.ConfusionActionsRemaining-1);
    if (Request.Type == EActionType::PlayCard)
    {
        if (Request.Card == ECardId::Confusion) { Candidate.ConfusionActionsRemaining = 2; }
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
        if (Candidate.GhostPhase != EGhostPhase::Preparation && !HasLegalAction(Candidate))
        {
            // Explicitly isolate the unresolved rule; never invent a draw/pass/winner.
            Candidate.Result = {EMatchStatus::AwaitingRuleDecision, EStone::Empty, EDecisionReason::NoLegalAction};
        }
    }
    State = MoveTemp(Candidate);
    return Result;
}
}
