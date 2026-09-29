#include "Presentation/MatchPresentation.h"

namespace Gomokards
{
TOptional<FIntPoint> FBoardLayout::ToCoordinate(FVector2D Local)
{
    if (!FMath::IsFinite(Local.X) || !FMath::IsFinite(Local.Y)
        || Local.X < 0 || Local.Y < 0 || Local.X >= Extent || Local.Y >= Extent) { return {}; }
    return FIntPoint(FMath::FloorToInt(Local.X / CellSize), FMath::FloorToInt(Local.Y / CellSize));
}
FVector2D FBoardLayout::Center(FIntPoint Coordinate)
{ return FVector2D((Coordinate.X + .5f) * CellSize, (Coordinate.Y + .5f) * CellSize); }
bool FTargetSelection::Toggle(const FMatchState& State, ECardId Selected)
{
    if (State.Result.Status != EMatchStatus::InProgress) { Clear(); return false; }
    const auto* Definition = FindCardDefinition(Selected);
    const auto& Actor = State.Players[State.CurrentPlayerIndex];
    if (!Definition || !Definition->bRequiresTarget || !Actor.Hand.Contains(Selected)) { return false; }
    if (Card == Selected && Player == Actor.Id) { Clear(); }
    else { Card = Selected; Player = Actor.Id; }
    return true;
}
FActionRequest FTargetSelection::BoardRequest(const FMatchState& State, FIntPoint Target) const
{
    if (IsActive()) { return FActionRequest::Play(Player, Card, Target); }
    return FActionRequest::Place(State.Players[State.CurrentPlayerIndex].Id, Target);
}
FString CardLabel(ECardId Card)
{
    switch (Card)
    {
    case ECardId::Restock: return TEXT("Restock (+2 cards)");
    case ECardId::SwapHands: return TEXT("Swap Hands");
    case ECardId::Steal: return TEXT("Steal (1 card)");
    case ECardId::TacticalNuke: return TEXT("Tactical Nuke (target)");
    default: return TEXT("Unavailable card");
    }
}
FString StoneLabel(EStone Stone) { return Stone == EStone::Black ? TEXT("Black") : Stone == EStone::White ? TEXT("White") : TEXT("None"); }
FString ResultLabel(const FMatchState& State)
{
    if (State.Result.Status == EMatchStatus::Won) { return StoneLabel(State.Result.WinningStone) + TEXT(" wins!  Start a new match to play again."); }
    if (State.Result.Status == EMatchStatus::AwaitingRuleDecision)
    { return TEXT("Awaiting rule decision: no legal action. No winner/draw assigned. Restart available."); }
    return FString::Printf(TEXT("%s to act  |  Completed actions: %llu"), *StoneLabel(State.Players[State.CurrentPlayerIndex].AssignedStone), State.CompletedActions);
}
FString RejectionLabel(EActionError Error)
{
    switch (Error)
    {
    case EActionError::Occupied: return TEXT("That point is occupied. Choose another point.");
    case EActionError::Forbidden: return TEXT("That point is forbidden by a Nuke.");
    case EActionError::InvalidCoordinate: case EActionError::InvalidTarget: return TEXT("Choose a point inside the board.");
    case EActionError::MatchStopped: return TEXT("Match stopped. Use New Match.");
    case EActionError::WrongPlayer: return TEXT("It is the other player's turn.");
    case EActionError::CardNotOwned: return TEXT("That card is no longer in the active hand.");
    case EActionError::UnsupportedCard: return TEXT("This card is not available in this slice.");
    default: return TEXT("Action rejected; the match has not changed.");
    }
}
}
