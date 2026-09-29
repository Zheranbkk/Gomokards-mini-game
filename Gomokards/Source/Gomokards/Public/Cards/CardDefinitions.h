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

struct FCardDefinition
{
    ECardId Id;
    bool bRequiresTarget;
};

GOMOKARDS_API const FCardDefinition* FindCardDefinition(ECardId Id);
GOMOKARDS_API TConstArrayView<FCardDefinition> GetPlayableCards();
}
