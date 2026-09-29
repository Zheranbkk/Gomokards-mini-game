#pragma once

#include "CoreMinimal.h"

namespace Gomokards
{
// Stable gameplay IDs; never localized names. Gated IDs are recognizable, not playable.
enum class ECardId : uint8
{
    Invalid, Restock, SwapHands, Steal, TacticalNuke,
    Polarity, Confusion, Barrier, BackToBasics, Ghost, Tetris, FastDuel, Undo, Joker
};

// Fixed target domains for the current cards, not a generalized targeting framework.
enum class ECardTarget : uint8 { None, Intersection, RegionTopLeft, CellCenter };

struct FCardDefinition
{
    ECardId Id;
    ECardTarget Target;
    bool RequiresTarget() const { return Target != ECardTarget::None; }
};

GOMOKARDS_API const FCardDefinition* FindCardDefinition(ECardId Id);
GOMOKARDS_API TConstArrayView<FCardDefinition> GetPlayableCards();
}
