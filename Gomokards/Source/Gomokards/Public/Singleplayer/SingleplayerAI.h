#pragma once
#include "Core/MatchState.h"
namespace Gomokards
{
// Only observable board data; no hand, deck, RNG, runtime or mutable authority.
GOMOKARDS_API TOptional<FIntPoint> ChooseSingleplayerMove(const FBoard& Board);
}
