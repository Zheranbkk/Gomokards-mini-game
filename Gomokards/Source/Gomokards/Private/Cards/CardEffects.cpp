#include "Cards/CardEffects.h"

namespace Gomokards
{
ECardId DrawCard(FRandomStream& Random)
{
    const auto Pool = GetPlayableCards();
    return Pool[Random.RandRange(0, Pool.Num() - 1)].Id;
}

bool ExecuteCardEffect(FMatchState& Candidate, int32 ActorIndex, ECardId Card, TOptional<FIntPoint> Target)
{
    auto& Hand = Candidate.Players[ActorIndex].Hand;
    auto& OtherHand = Candidate.Players[SingleOpponentIndex(Candidate, ActorIndex)].Hand;
    switch (Card)
    {
    case ECardId::Restock:
        Hand.Add(DrawCard(Candidate.Random));
        Hand.Add(DrawCard(Candidate.Random));
        return true;
    case ECardId::SwapHands:
        Swap(Hand, OtherHand);
        return true;
    case ECardId::Steal:
        if (!OtherHand.IsEmpty())
        {
            const ECardId Stolen = OtherHand[Candidate.Random.RandRange(0, OtherHand.Num() - 1)];
            // Match Python list.remove(value), including duplicate-card ordering.
            OtherHand.RemoveAt(OtherHand.Find(Stolen));
            Hand.Add(Stolen);
        }
        return true;
    case ECardId::TacticalNuke:
        if (!Target.IsSet() || !FBoard::Contains(Target.GetValue())) { return false; }
        Candidate.Board.At(Target.GetValue()) = {EStone::Empty, true};
        return true;
    default:
        return false;
    }
}
}
