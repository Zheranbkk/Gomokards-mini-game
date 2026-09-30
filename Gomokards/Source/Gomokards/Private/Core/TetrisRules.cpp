#include "Core/TetrisRules.h"
#include "Core/MatchRules.h"

namespace Gomokards
{
static FIntPoint FootprintSize(TConstArrayView<FIntPoint> Offsets)
{
    FIntPoint Size(0,0);
    for (FIntPoint P : Offsets) { Size.X=FMath::Max(Size.X,P.X+1); Size.Y=FMath::Max(Size.Y,P.Y+1); }
    return Size;
}
TArray<FIntPoint> TetrisOffsets(ETetrisShape Shape, uint8 Rotation)
{
    TArray<FIntPoint> Cells;
    switch (Shape)
    {
    case ETetrisShape::Square: Cells={{0,0},{1,0},{0,1},{1,1}}; break;
    case ETetrisShape::L: Cells={{0,0},{0,1},{0,2},{1,2}}; break;
    case ETetrisShape::Cross: Cells={{1,0},{0,1},{1,1},{2,1},{1,2}}; break;
    case ETetrisShape::Line: Cells={{0,0},{0,1},{0,2},{0,3}}; break;
    case ETetrisShape::Z: Cells={{0,0},{1,0},{1,1},{2,1}}; break;
    case ETetrisShape::T: Cells={{0,0},{1,0},{2,0},{1,1}}; break;
    default: return {};
    }
    // Clockwise in screen coordinates, around the fixed top-left bounding-box origin; no kicks.
    for (uint8 R=0; R<Rotation%4; ++R)
    {
        const int32 Height=FootprintSize(Cells).Y;
        for (FIntPoint& P : Cells) { P=FIntPoint(Height-1-P.Y,P.X); }
    }
    return Cells;
}
FIntPoint TetrisGravity(ETetrisEdge Edge)
{
    switch (Edge)
    {
    case ETetrisEdge::Top: return {0,1};
    case ETetrisEdge::Bottom: return {0,-1};
    case ETetrisEdge::Left: return {1,0};
    case ETetrisEdge::Right: return {-1,0};
    default: return FIntPoint::ZeroValue;
    }
}
FIntPoint TetrisTranslation(ETetrisInput Input)
{
    switch (Input)
    {
    case ETetrisInput::Up: return {0,-1};
    case ETetrisInput::Down: return {0,1};
    case ETetrisInput::Left: return {-1,0};
    case ETetrisInput::Right: return {1,0};
    default: return FIntPoint::ZeroValue;
    }
}
bool TetrisFits(const FBoard& Board, TConstArrayView<FIntPoint> Offsets, FIntPoint Origin)
{
    if (Offsets.IsEmpty()) { return false; }
    for (FIntPoint Offset : Offsets)
    {
        const FIntPoint P=Origin+Offset;
        if (!FBoard::Contains(P) || Board.At(P).Stone!=EStone::Empty || Board.At(P).bForbidden) { return false; }
    }
    return true; // Barrier changes connectivity, never physical collision.
}
static bool BetterSpawn(const FTetrisSpawn& A, const FTetrisSpawn& B)
{
    if (!A.bLegal) { return false; }
    if (!B.bLegal) { return true; }
    if (A.Clearance!=B.Clearance) { return A.Clearance>B.Clearance; }
    if (A.CenterDistance!=B.CenterDistance) { return A.CenterDistance<B.CenterDistance; }
    if (A.Edge!=B.Edge) { return A.Edge<B.Edge; }
    if (A.Origin.Y!=B.Origin.Y) { return A.Origin.Y<B.Origin.Y; }
    return A.Origin.X<B.Origin.X;
}
FTetrisSpawn BestTetrisSpawnAtEdge(const FBoard& Board, ETetrisShape Shape, ETetrisEdge Edge)
{
    FTetrisSpawn Best;
    const auto Cells=TetrisOffsets(Shape);
    if (Cells.IsEmpty()) { return Best; }
    const FIntPoint Size=FootprintSize(Cells), Gravity=TetrisGravity(Edge);
    if (Gravity==FIntPoint::ZeroValue) { return Best; }
    const bool bHorizontalEdge=Edge==ETetrisEdge::Top || Edge==ETetrisEdge::Bottom;
    const int32 Limit=FBoard::Size-(bHorizontalEdge ? Size.X : Size.Y);
    for (int32 Along=0; Along<=Limit; ++Along)
    {
        FIntPoint Origin;
        switch (Edge)
        {
        case ETetrisEdge::Top: Origin={Along,0}; break;
        case ETetrisEdge::Bottom: Origin={Along,FBoard::Size-Size.Y}; break;
        case ETetrisEdge::Left: Origin={0,Along}; break;
        case ETetrisEdge::Right: Origin={FBoard::Size-Size.X,Along}; break;
        default: return Best;
        }
        if (!TetrisFits(Board,Cells,Origin)) { continue; }
        FTetrisSpawn Candidate; Candidate.bLegal=true; Candidate.Edge=Edge; Candidate.Origin=Origin;
        while (TetrisFits(Board,Cells,Origin+Gravity*(Candidate.Clearance+1))) { ++Candidate.Clearance; }
        const FIntPoint CenterDelta=Origin*2+Size-FIntPoint(FBoard::Size,FBoard::Size);
        Candidate.CenterDistance=CenterDelta.X*CenterDelta.X+CenterDelta.Y*CenterDelta.Y;
        if (BetterSpawn(Candidate,Best)) { Best=Candidate; }
    }
    return Best;
}
FTetrisSpawn ChooseTetrisSpawn(const FBoard& Board, ETetrisShape Shape, FRandomStream& Random)
{
    TArray<FTetrisSpawn> Preferred;
    FTetrisSpawn Best;
    for (ETetrisEdge Edge : {ETetrisEdge::Top,ETetrisEdge::Bottom,ETetrisEdge::Left,ETetrisEdge::Right})
    {
        const auto Candidate=BestTetrisSpawnAtEdge(Board,Shape,Edge);
        if (Candidate.bLegal && Candidate.Clearance>=FTetrisSpawn::PreferredClearance) { Preferred.Add(Candidate); }
        if (BetterSpawn(Candidate,Best)) { Best=Candidate; }
    }
    // No RNG draw is needed when there is only one preferred edge or a deterministic fallback.
    if (Preferred.Num()==1) { return Preferred[0]; }
    return Preferred.IsEmpty() ? Best : Preferred[Random.RandRange(0,Preferred.Num()-1)];
}
int32 ClearTetrisLines(FBoard& Board)
{
    TArray<int32> Removed;
    for (int32 I=0; I<FBoard::Size*FBoard::Size; ++I)
    {
        // Every cell on a qualifying connected run passes the same authoritative win predicate.
        if (HasWinningLine(Board,FBoard::ToCoordinate(I),Board.Cells[I].Stone)) { Removed.Add(I); }
    }
    for (int32 I : Removed) { Board.Cells[I].Stone=EStone::Empty; }
    return Removed.Num();
}
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
static bool HasActiveTetris(const FMatchState& State)
{
    return State.Result.Status==EMatchStatus::InProgress && State.Tetris.bActive
        && State.GhostPhase==EGhostPhase::None;
}
bool ApplyTetrisInput(FMatchState& State, ETetrisInput Input)
{
    if (!HasActiveTetris(State)) { return false; }
    auto Candidate=State.Tetris;
    if (Input==ETetrisInput::Rotate) { Candidate.Rotation=(Candidate.Rotation+1)%4; }
    else
    {
        const auto Delta=TetrisTranslation(Input);
        if (Delta==FIntPoint::ZeroValue) { return false; }
        Candidate.Origin+=Delta;
    }
    if (!TetrisFits(State.Board,TetrisOffsets(Candidate.Shape,Candidate.Rotation),Candidate.Origin)) { return false; }
    State.Tetris=Candidate;
    return true;
}
bool StepTetrisGravity(FMatchState& State)
{
    if (!HasActiveTetris(State)) { return false; }
    auto& T=State.Tetris;
    const auto Cells=TetrisOffsets(T.Shape,T.Rotation);
    if (!TetrisFits(State.Board,Cells,T.Origin)) { return false; } // Trusted fixtures must still be physically valid.
    const auto Next=T.Origin+TetrisGravity(T.Edge);
    if (TetrisFits(State.Board,Cells,Next)) { T.Origin=Next; return true; }
    for (FIntPoint Offset : Cells) { State.Board.At(T.Origin+Offset).Stone=T.Stone; }
    ClearTetrisLines(State.Board);
    ++T.BlockNumber; T.OperatorIndex=SingleOpponentIndex(State,T.OperatorIndex);
    SpawnOrSkipTetris(State);
    return true;
}
}
