#pragma once
#include "Core/MatchState.h"
namespace Gomokards
{
// Validate first, then mutate only the board. No hand, actor, RNG or turn required.
GOMOKARDS_API bool ApplyTacticalNuke(FBoard& Board, FIntPoint Target);
GOMOKARDS_API bool ApplyPolarity(FBoard& Board, FIntPoint Anchor);
GOMOKARDS_API bool ApplyBarrier(FBoard& Board, FIntPoint Anchor);
}
