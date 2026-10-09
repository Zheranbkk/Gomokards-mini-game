#pragma once
#include "Core/MatchState.h"
namespace Gomokards
{
enum class EPlacementError : uint8 { None, InvalidStone, InvalidCoordinate, Occupied, Forbidden };
struct FPlacementResult
{
    EPlacementError Error = EPlacementError::None;
    bool bSuccessfulBlock = false;
    bool bWinningLine = false;
    bool IsAccepted() const { return Error == EPlacementError::None; }
};
// Board-only mutation: no reward, hand, turn, RNG, or mode policy.
GOMOKARDS_API FPlacementResult TryPlaceStone(FBoard& Board, FIntPoint Coordinate, EStone Stone);
GOMOKARDS_API bool HasWinningLine(const FBoard& Board, FIntPoint Origin, EStone Stone);
GOMOKARDS_API FMatchResult EvaluateBoardResult(const FBoard& Board);
GOMOKARDS_API bool HasSuccessfulBlock(const FBoard& Board, FIntPoint Origin, EStone PlacedStone);
GOMOKARDS_API bool BeginGhostHidden(FMatchState& State);
}
