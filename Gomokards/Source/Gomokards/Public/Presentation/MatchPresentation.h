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

// Only local intent: no board, hands, result or RNG copy lives here.
struct GOMOKARDS_API FTargetSelection
{
    ECardId Card = ECardId::Invalid;
    FPlayerId Player = INDEX_NONE;
    bool IsActive() const { return Card != ECardId::Invalid; }
    void Clear() { Card = ECardId::Invalid; Player = INDEX_NONE; }
    bool Toggle(const FMatchState& State, ECardId Selected);
    FActionRequest BoardRequest(const FMatchState& State, FIntPoint Target) const;
};

GOMOKARDS_API FLinearColor StoneDisplayColor(const FMatchState& State, EStone Stone);
GOMOKARDS_API FString GhostLabel(const FMatchState& State, double PreparationSeconds);
GOMOKARDS_API FString TetrisLabel(const FMatchState& State);
GOMOKARDS_API TOptional<ETetrisInput> TetrisInputForKey(const FKey& Key);
GOMOKARDS_API FString CardLabel(ECardId Card);
GOMOKARDS_API FString StoneLabel(EStone Stone);
GOMOKARDS_API FString EffectLabel(const FMatchState& State);
GOMOKARDS_API FString TargetingLabel(ECardId Selected);
GOMOKARDS_API FString ResultLabel(const FMatchState& State);
GOMOKARDS_API FString RejectionLabel(EActionError Error);
}
