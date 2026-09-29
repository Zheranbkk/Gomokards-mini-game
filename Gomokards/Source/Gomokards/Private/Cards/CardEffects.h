#pragma once

#include "Core/MatchState.h"

namespace Gomokards
{
GOMOKARDS_API ECardId DrawCard(FRandomStream& Random);
// Internal: validated candidate only, played card already consumed by common resolver.
bool ExecuteCardEffect(FMatchState& Candidate, int32 ActorIndex, ECardId Card, TOptional<FIntPoint> Target);
}
