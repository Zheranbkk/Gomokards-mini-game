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
    case ECardId::Polarity:
        if (!Target.IsSet() || !FBoard::ContainsAnchor(Target.GetValue())) { return false; }
        for (FIntPoint Corner : FBoard::RegionCorners(Target.GetValue()))
        { Candidate.Board.At(Corner).Stone = OppositeStone(Candidate.Board.At(Corner).Stone); }
        return true;
    case ECardId::Barrier:
        if (!Target.IsSet() || !FBoard::ContainsAnchor(Target.GetValue())) { return false; }
        Candidate.Board.Barriers.AddUnique(Target.GetValue());
        return true;
    case ECardId::Confusion:
        return true; // Common resolver owns the explicit old-duration / refresh ordering.
    case ECardId::BackToBasics:
        Candidate.bCardsDisabled = true;
        Candidate.Board.Barriers.Empty();
        for (FCell& Cell : Candidate.Board.Cells) { Cell.bForbidden = false; }
        return true; // Stones and remaining hands are intentionally retained.
    default:
        return false;
    }
}
}
