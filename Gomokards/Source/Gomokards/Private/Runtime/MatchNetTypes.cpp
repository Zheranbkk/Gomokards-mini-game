#include "Runtime/MatchNetTypes.h"
#include "Core/MatchState.h"

FMatchPublicView MakePublicView(const Gomokards::FMatchState& State, const TArray<int32>& OccupiedIds,
    EMatchSession Session, uint64 Epoch, uint64 Revision)
{
    FMatchPublicView View;
    View.Epoch=Epoch; View.Revision=Revision; View.Session=Session;
    for (const auto& Cell : State.Board.Cells)
    {
        FMatchDisplayCell Display;
        Display.Stone=static_cast<uint8>(Cell.Stone); Display.bForbidden=Cell.bForbidden;
        View.Cells.Add(Display);
    }
    View.Barriers=State.Board.Barriers;
    for (const auto& Player : State.Players)
    {
        FMatchSeatView Seat;
        Seat.PlayerId=Player.Id; Seat.Stone=static_cast<uint8>(Player.AssignedStone);
        Seat.bOccupied=OccupiedIds.Contains(Player.Id); Seat.HandCount=Player.Hand.Num();
        View.Seats.Add(Seat);
    }
    if (State.Players.IsValidIndex(State.CurrentPlayerIndex)) { View.CurrentPlayerId=State.Players[State.CurrentPlayerIndex].Id; }
    View.CompletedActions=State.CompletedActions;
    View.Result=static_cast<uint8>(State.Result.Status); View.WinningStone=static_cast<uint8>(State.Result.WinningStone);
    View.DecisionReason=static_cast<uint8>(State.Result.Decision);
    View.bCardsDisabled=State.bCardsDisabled; View.ConfusionRemaining=State.ConfusionActionsRemaining;
    return View;
}
FMatchPrivateView MakePrivateView(const Gomokards::FMatchState& State, int32 PlayerId, bool bAdmin, uint64 Epoch, uint64 Revision)
{
    FMatchPrivateView View;
    View.Epoch=Epoch; View.Revision=Revision; View.bDevelopmentAdmin=bAdmin;
    for (const auto& Player : State.Players)
    {
        if (Player.Id!=PlayerId) { continue; }
        View.PlayerId=Player.Id; View.Stone=static_cast<uint8>(Player.AssignedStone);
        for (auto Card : Player.Hand) { View.Hand.Add(static_cast<uint8>(Card)); }
        break;
    }
    return View;
}
