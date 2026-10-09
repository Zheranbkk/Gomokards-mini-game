#include "Cards/CardDefinitions.h"
namespace Gomokards
{
ECardTarget BoardEffectTarget(ECardId Id)
{
    switch (Id)
    {
    case ECardId::TacticalNuke: return ECardTarget::Intersection;
    case ECardId::Polarity: return ECardTarget::RegionTopLeft;
    case ECardId::Barrier: return ECardTarget::CellCenter;
    default: return ECardTarget::None;
    }
}
}
