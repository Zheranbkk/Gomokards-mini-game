#pragma once

#include "Core/MatchState.h"

namespace Gomokards
{
enum class ETetrisInput : uint8 { Up, Down, Left, Right, Rotate };
struct FTetrisSpawn
{
    static constexpr int32 PreferredClearance = 5;
    bool bLegal = false;
    ETetrisEdge Edge = ETetrisEdge::Top;
    FIntPoint Origin = FIntPoint::ZeroValue;
    int32 Clearance = 0;
    int32 CenterDistance = 0; // Squared distance of doubled bounding-box center from board center.
    bool operator==(const FTetrisSpawn&) const = default;
};

GOMOKARDS_API TArray<FIntPoint> TetrisOffsets(ETetrisShape Shape, uint8 Rotation = 0);
GOMOKARDS_API FIntPoint TetrisGravity(ETetrisEdge Edge);
GOMOKARDS_API FIntPoint TetrisTranslation(ETetrisInput Input);
GOMOKARDS_API bool TetrisFits(const FBoard& Board, TConstArrayView<FIntPoint> Offsets, FIntPoint Origin);
GOMOKARDS_API FTetrisSpawn BestTetrisSpawnAtEdge(const FBoard& Board, ETetrisShape Shape, ETetrisEdge Edge);
GOMOKARDS_API FTetrisSpawn ChooseTetrisSpawn(const FBoard& Board, ETetrisShape Shape, FRandomStream& Random);
// Called only after the card resolver has completed/transferred the ordinary action.
GOMOKARDS_API bool BeginTetris(FMatchState& State);
GOMOKARDS_API bool ApplyTetrisInput(FMatchState& State, ETetrisInput Input);
GOMOKARDS_API bool StepTetrisGravity(FMatchState& State);
// Collect all qualifying cells before deleting; committed board cells never collapse.
GOMOKARDS_API int32 ClearTetrisLines(FBoard& Board);
}
