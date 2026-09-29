#pragma once

#include "CoreMinimal.h"
#include "Containers/StaticArray.h"
#include "Math/RandomStream.h"
#include "Cards/CardDefinitions.h"

namespace Gomokards
{
enum class EStone : uint8 { Empty, Black, White };
using FPlayerId = int32;

struct FCell
{
    EStone Stone = EStone::Empty;
    bool bForbidden = false;
    bool operator==(const FCell&) const = default;
};

struct GOMOKARDS_API FBoard
{
    static constexpr int32 Size = 19;
    static constexpr int32 WinLength = 5;
    TStaticArray<FCell, Size * Size> Cells;

    static bool Contains(FIntPoint P);
    // Checked conversions: callers validate untrusted coordinates before using these.
    static int32 ToIndex(FIntPoint P);
    static FIntPoint ToCoordinate(int32 Index);
    const FCell& At(FIntPoint P) const { return Cells[ToIndex(P)]; }
    FCell& At(FIntPoint P) { return Cells[ToIndex(P)]; }
    bool operator==(const FBoard& Other) const;
};

struct FPlayerState
{
    FPlayerId Id = INDEX_NONE;
    EStone AssignedStone = EStone::Empty;
    TArray<ECardId> Hand;
    bool operator==(const FPlayerState&) const = default;
};

enum class EMatchStatus : uint8 { InProgress, Won, AwaitingRuleDecision };
enum class EDecisionReason : uint8 { None, NoLegalAction };

struct FMatchResult
{
    EMatchStatus Status = EMatchStatus::InProgress;
    EStone WinningStone = EStone::Empty;
    EDecisionReason Decision = EDecisionReason::None;
    bool operator==(const FMatchResult&) const = default;
};

// Value state owned by the caller. Only ResolveAction/Reset should mutate a live match.
// Public fields also allow explicit, UI-free test fixtures; they are not a UI write API.
struct GOMOKARDS_API FMatchState
{
    FBoard Board;
    TArray<FPlayerState> Players;
    int32 CurrentPlayerIndex = 0;
    uint64 CompletedActions = 0;
    FMatchResult Result;
    FRandomStream Random;

    explicit FMatchState(int32 Seed = 0);
    void Reset(int32 Seed = 0);
    bool operator==(const FMatchState& Other) const;
};

// All current two-player assumptions live here, rather than inside card effects.
GOMOKARDS_API int32 SingleOpponentIndex(const FMatchState& State, int32 PlayerIndex);
GOMOKARDS_API EStone OppositeStone(EStone Stone);
GOMOKARDS_API EStone EffectivePlacementStone(const FPlayerState& Player);
}
