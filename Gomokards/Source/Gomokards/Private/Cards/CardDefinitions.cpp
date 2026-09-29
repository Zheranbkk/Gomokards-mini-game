#include "Cards/CardDefinitions.h"

namespace Gomokards
{
static constexpr FCardDefinition Definitions[] = {
    {ECardId::Restock, false}, {ECardId::SwapHands, false},
    {ECardId::Steal, false}, {ECardId::TacticalNuke, true}
};

TConstArrayView<FCardDefinition> GetPlayableCards() { return MakeArrayView(Definitions); }

const FCardDefinition* FindCardDefinition(ECardId Id)
{
    for (const FCardDefinition& Definition : Definitions)
    {
        if (Definition.Id == Id) { return &Definition; }
    }
    return nullptr;
}
}
