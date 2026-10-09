#include "Singleplayer/SingleplayerAI.h"
#include "Core/MatchRules.h"
namespace Gomokards
{
namespace
{
int32 DevelopmentScore(const FBoard& Board, FIntPoint Origin, EStone Stone)
{
    int32 Score = 0;
    for (FIntPoint Direction : {FIntPoint(1,0), FIntPoint(0,1), FIntPoint(1,1), FIntPoint(1,-1)})
    {
        int32 Count = 1, OpenEnds = 0;
        for (int32 Sign : {-1, 1})
        {
            FIntPoint Previous = Origin, Point = Origin + Direction * Sign;
            while (FBoard::Contains(Point) && !Board.IsLinkBlocked(Previous, Point))
            {
                const auto& Cell = Board.At(Point);
                if (Cell.Stone != Stone)
                {
                    if (Cell.Stone == EStone::Empty && !Cell.bForbidden) { ++OpenEnds; }
                    break;
                }
                ++Count;
                Previous = Point;
                Point += Direction * Sign;
            }
        }
        Score += Count * Count * (1 + OpenEnds);
    }
    return Score;
}
}
TOptional<FIntPoint> ChooseSingleplayerMove(const FBoard& Board)
{
    TOptional<FIntPoint> Win, Block, Best;
    int32 BestScore = MIN_int32;
    // Stable row-major order resolves ties. Candidate boards never escape this function.
    for (int32 I = 0; I < FBoard::Size * FBoard::Size; ++I)
    {
        if (Board.Cells[I].Stone != EStone::Empty || Board.Cells[I].bForbidden) { continue; }
        const FIntPoint Point = FBoard::ToCoordinate(I);
        FBoard White = Board, Black = Board;
        const auto WhitePlacement = TryPlaceStone(White, Point, EStone::White);
        const auto BlackPlacement = TryPlaceStone(Black, Point, EStone::Black);
        if (WhitePlacement.bWinningLine && !Win.IsSet()) { Win = Point; }
        if (BlackPlacement.bWinningLine && !Block.IsSet()) { Block = Point; }
        const int32 Score = 12 * DevelopmentScore(White, Point, EStone::White)
            + 10 * DevelopmentScore(Black, Point, EStone::Black)
            - 2 * (FMath::Abs(Point.X - 9) + FMath::Abs(Point.Y - 9));
        if (Score > BestScore) { BestScore = Score; Best = Point; }
    }
    return Win.IsSet() ? Win : Block.IsSet() ? Block : Best;
}
}
