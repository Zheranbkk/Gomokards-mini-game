#pragma once

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

GOMOKARDS_API FString CardLabel(ECardId Card);
GOMOKARDS_API FString StoneLabel(EStone Stone);
GOMOKARDS_API FString ResultLabel(const FMatchState& State);
GOMOKARDS_API FString RejectionLabel(EActionError Error);
}
