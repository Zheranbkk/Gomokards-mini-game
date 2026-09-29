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
TOptional<FIntPoint> FBoardLayout::TargetAt(FVector2D Local, ECardId Card)
{
    const auto* Definition = FindCardDefinition(Card);
    const ECardTarget Domain = Definition ? Definition->Target : ECardTarget::Intersection;
    // A cell's hit area is the square between its four intersections, not a stone's hit box.
    auto Target = ToCoordinate(Domain == ECardTarget::CellCenter ? Local-Center({0,0}) : Local);
    if (Target.IsSet() && (Domain == ECardTarget::CellCenter || Domain == ECardTarget::RegionTopLeft)
        && !FBoard::ContainsAnchor(Target.GetValue())) { Target.Reset(); }
    return Target;
}
TStaticArray<FVector2D, 4> FBoardLayout::BarrierCross(FIntPoint Anchor)
{
    const auto Corners = FBoard::RegionCorners(Anchor);
    const FVector2D A=Center(Corners[0]), B=Center(Corners[1]), C=Center(Corners[2]), D=Center(Corners[3]);
    return {(A+C)*.5, (B+D)*.5, (A+B)*.5, (C+D)*.5};
}
bool FTargetSelection::Toggle(const FMatchState& State, ECardId Selected)
{
    if (State.Result.Status != EMatchStatus::InProgress || State.bCardsDisabled) { Clear(); return false; }
    const auto* Definition = FindCardDefinition(Selected);
    const auto& Actor = State.Players[State.CurrentPlayerIndex];
    if (!Definition || !Definition->RequiresTarget() || !Actor.Hand.Contains(Selected)) { return false; }
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
    case ECardId::Polarity: return TEXT("Polarity (2x2 target)");
    case ECardId::Confusion: return TEXT("Confusion (next 2 actions)");
    case ECardId::Barrier: return TEXT("Barrier (cell center)");
    case ECardId::BackToBasics: return TEXT("Back to Basics (disable cards)");
    default: return TEXT("Unavailable card");
    }
}
FString StoneLabel(EStone Stone) { return Stone == EStone::Black ? TEXT("Black") : Stone == EStone::White ? TEXT("White") : TEXT("None"); }
FString EffectLabel(const FMatchState& State)
{
    if (State.bCardsDisabled) { return TEXT("BACK TO BASICS: cards disabled until restart. Remaining cards are inert."); }
    if (State.ConfusionActionsRemaining > 0)
    { return FString::Printf(TEXT("CONFUSION: %d successful action(s) remain. Placement color: %s."),
        State.ConfusionActionsRemaining, *StoneLabel(EffectivePlacementStone(State.Players[State.CurrentPlayerIndex],true))); }
    return TEXT("Cards enabled. No Confusion active.");
}
FString TargetingLabel(ECardId Selected)
{
    FString Label;
    switch (Selected)
    {
    case ECardId::TacticalNuke: Label=TEXT("NUKE: select one intersection."); break;
    case ECardId::Polarity: Label=TEXT("POLARITY: select the top-left intersection of a 2x2 region (last row/column invalid)."); break;
    case ECardId::Barrier: Label=TEXT("BARRIER: select the CENTER of a cell between four intersections."); break;
    default: return TEXT("PLACEMENT MODE\nClick an intersection to place a stone. Only the active hand can play cards.");
    }
    return Label + TEXT("\nRight-click, Escape or reselect the active card to cancel.");
}
FString ResultLabel(const FMatchState& State)
{
    if (State.Result.Status == EMatchStatus::Draw) { return TEXT("Draw: both colors have five in a row. Start a new match."); }
    if (State.Result.Status == EMatchStatus::Won) { return StoneLabel(State.Result.WinningStone) + TEXT(" wins!  Start a new match to play again."); }
    if (State.Result.Status == EMatchStatus::AwaitingRuleDecision)
    { return TEXT("Awaiting rule decision: no legal action. No winner/draw assigned. Restart available."); }
    return FString::Printf(TEXT("%s to act  |  Completed actions: %llu"), *StoneLabel(State.Players[State.CurrentPlayerIndex].AssignedStone), State.CompletedActions);
}
FString RejectionLabel(EActionError Error)
{
    switch (Error)
    {
    case EActionError::CardsDisabled: return TEXT("Cards are disabled by Back to Basics until restart.");
    case EActionError::Occupied: return TEXT("That point is occupied. Choose another point.");
    case EActionError::Forbidden: return TEXT("That point is forbidden by a Nuke.");
    case EActionError::InvalidCoordinate: return TEXT("Choose an intersection inside the board.");
    case EActionError::InvalidTarget: return TEXT("Choose a valid target: Nuke intersection, Polarity top-left, or Barrier cell center.");
    case EActionError::MatchStopped: return TEXT("Match stopped. Use New Match.");
    case EActionError::WrongPlayer: return TEXT("It is the other player's turn.");
    case EActionError::CardNotOwned: return TEXT("That card is no longer in the active hand.");
    case EActionError::UnsupportedCard: return TEXT("This card is not available in this slice.");
    default: return TEXT("Action rejected; the match has not changed.");
    }
}
}
