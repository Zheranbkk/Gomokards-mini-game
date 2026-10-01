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
    case ECardId::Ghost:
        Candidate.GhostPhase = EGhostPhase::Preparation;
        Candidate.GhostPlacementsCompleted = 0;
        return true; // Timer transition belongs to the runtime owner, never to the core.
    case ECardId::Tetris:
        return true; // Common resolver starts mode after normal action completion/turn transfer.
    case ECardId::BackToBasics:
        Candidate.bCardsDisabled = true;
        return true; // Board history is preserved; the common resolver clears active Confusion.
    default:
        return false;
    }
}
}
