#include "Core/MatchRules.h"

namespace Gomokards
{
static const FIntPoint Directions[] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};
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

FPlacementResult TryPlaceStone(FBoard& Board, FIntPoint Coordinate, EStone Stone)
{
    if (Stone != EStone::Black && Stone != EStone::White) { return {EPlacementError::InvalidStone}; }
    if (!FBoard::Contains(Coordinate)) { return {EPlacementError::InvalidCoordinate}; }
    if (Board.At(Coordinate).Stone != EStone::Empty) { return {EPlacementError::Occupied}; }
    if (Board.At(Coordinate).bForbidden) { return {EPlacementError::Forbidden}; }
    FBoard Candidate = Board;
    Candidate.At(Coordinate).Stone = Stone;
    FPlacementResult Result;
    Result.bSuccessfulBlock = HasSuccessfulBlock(Candidate, Coordinate, Stone);
    Result.bWinningLine = HasWinningLine(Candidate, Coordinate, Stone);
    Board = MoveTemp(Candidate);
    return Result;
}
}
