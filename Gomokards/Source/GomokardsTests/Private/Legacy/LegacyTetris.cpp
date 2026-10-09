#include "Legacy/LegacyRules.h"
namespace Gomokards
{
static void FinishTetris(FMatchState& State)
{
    State.Tetris={};
    State.Result=EvaluateBoardResult(State.Board);
    if (State.Result.Status==EMatchStatus::InProgress && !HasLegalAction(State))
    { State.Result={EMatchStatus::AwaitingRuleDecision,EStone::Empty,EDecisionReason::NoLegalAction}; }
}
static void SpawnOrSkipTetris(FMatchState& State)
{
    auto& T=State.Tetris;
    while (T.BlockNumber<=FTetrisState::BlockLimit)
    {
        T.Shape=static_cast<ETetrisShape>(State.Random.RandRange(0,static_cast<int32>(ETetrisShape::Count)-1));
        T.Rotation=0;
        T.Stone=OppositeStone(State.Players[T.OperatorIndex].AssignedStone);
        const auto Spawn=ChooseTetrisSpawn(State.Board,T.Shape,State.Random);
        if (Spawn.bLegal) { T.Edge=Spawn.Edge; T.Origin=Spawn.Origin; return; }
        ++T.BlockNumber; T.OperatorIndex=SingleOpponentIndex(State,T.OperatorIndex);
    }
    // Skips write/clear no cells. Final evaluation still runs even when all six were skipped.
    FinishTetris(State);
}
bool BeginTetris(FMatchState& State)
{
    if (State.Result.Status!=EMatchStatus::InProgress || State.Tetris.bActive
        || State.GhostPhase!=EGhostPhase::None || SingleOpponentIndex(State,State.CurrentPlayerIndex)==INDEX_NONE) { return false; }
    State.Tetris={}; State.Tetris.bActive=true; State.Tetris.BlockNumber=1;
    State.Tetris.OperatorIndex=State.CurrentPlayerIndex;
    SpawnOrSkipTetris(State);
    return true;
}
bool StepTetrisGravity(FMatchState& State)
{
    const auto Result = StepTetrisPiece(State);
    if (Result == ETetrisStep::Rejected) { return false; }
    if (Result == ETetrisStep::Locked)
    {
        State.Tetris.bActive=true;
        ++State.Tetris.BlockNumber;
        State.Tetris.OperatorIndex=SingleOpponentIndex(State,State.Tetris.OperatorIndex);
        SpawnOrSkipTetris(State);
    }
    return true;
}
}
