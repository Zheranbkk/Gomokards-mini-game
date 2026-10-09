#include "Legacy/LegacyRules.h"

namespace Gomokards
{
static constexpr FCardDefinition Definitions[] = {
    {ECardId::Restock, ECardTarget::None}, {ECardId::SwapHands, ECardTarget::None},
    {ECardId::Steal, ECardTarget::None}, {ECardId::TacticalNuke, ECardTarget::Intersection},
    {ECardId::Polarity, ECardTarget::RegionTopLeft}, {ECardId::Confusion, ECardTarget::None},
    {ECardId::Barrier, ECardTarget::CellCenter}, {ECardId::BackToBasics, ECardTarget::None},
    {ECardId::Ghost, ECardTarget::None}, {ECardId::Tetris, ECardTarget::None}
};

TConstArrayView<FCardDefinition> GetPlayableCards() { return MakeArrayView(Definitions); }

const FCardDefinition* FindLegacyCardDefinition(ECardId Id)
{
    for (const FCardDefinition& Definition : Definitions)
    {
        if (Definition.Id == Id) { return &Definition; }
    }
    return nullptr;
}
}
