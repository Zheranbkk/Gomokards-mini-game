#pragma once
#include "Core/TetrisRules.h"
#include "InputCoreTypes.h"

#include "Core/MatchRules.h"

namespace Gomokards
{
// Pixel conversion shared by painting, click handling and tests. Each intersection
// has a cell-sized hit area; bounds are half-open and never clamp outside clicks.
struct GOMOKARDS_API FBoardLayout
{
    static constexpr float CellSize = 30.f;
    static constexpr float Extent = CellSize * FBoard::Size;
    static TOptional<FIntPoint> ToCoordinate(FVector2D Local);
    static FVector2D Center(FIntPoint Coordinate);
    static TOptional<FIntPoint> TargetAt(FVector2D Local, ECardId Card);
    // Endpoints of the small cross, derived from the core's four-corner cell definition.
    static TStaticArray<FVector2D, 4> BarrierCross(FIntPoint Anchor);
};

GOMOKARDS_API FLinearColor StoneDisplayColor(const FMatchState& State, EStone Stone);
GOMOKARDS_API TOptional<ETetrisInput> TetrisInputForKey(const FKey& Key);
}
