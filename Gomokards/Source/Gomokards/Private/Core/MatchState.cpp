#include "Core/MatchState.h"

namespace Gomokards
{
bool FBoard::Contains(FIntPoint P) { return P.X >= 0 && P.Y >= 0 && P.X < Size && P.Y < Size; }
bool FBoard::ContainsAnchor(FIntPoint P) { return P.X >= 0 && P.Y >= 0 && P.X < Size-1 && P.Y < Size-1; }
TStaticArray<FIntPoint, 4> FBoard::RegionCorners(FIntPoint Anchor)
{
    check(ContainsAnchor(Anchor));
    return {Anchor, Anchor+FIntPoint(1,0), Anchor+FIntPoint(0,1), Anchor+FIntPoint(1,1)};
}
bool FBoard::IsLinkBlocked(FIntPoint From, FIntPoint To) const
{
    if (From == To) { return false; }
    for (FIntPoint Anchor : Barriers)
    {
        bool bFrom = false, bTo = false;
        for (FIntPoint Corner : RegionCorners(Anchor))
        { bFrom |= Corner == From; bTo |= Corner == To; }
        if (bFrom && bTo) { return true; } // Exactly the six unordered pairs of four corners.
    }
    return false;
}
int32 FBoard::ToIndex(FIntPoint P) { check(Contains(P)); return P.Y * Size + P.X; }
FIntPoint FBoard::ToCoordinate(int32 Index)
{
    check(Index >= 0 && Index < Size * Size);
    return FIntPoint(Index % Size, Index / Size);
}
bool FBoard::operator==(const FBoard& Other) const
{
    for (int32 I = 0; I < Size * Size; ++I) { if (!(Cells[I] == Other.Cells[I])) { return false; } }
    return Barriers == Other.Barriers;
}

FMatchState::FMatchState(int32 Seed) : Random(Seed)
{
    Players.Add({0, EStone::Black, {}});
    Players.Add({1, EStone::White, {}});
}
void FMatchState::Reset(int32 Seed) { *this = FMatchState(Seed); }
bool FMatchState::operator==(const FMatchState& Other) const
{
    return Board == Other.Board && Players == Other.Players
        && CurrentPlayerIndex == Other.CurrentPlayerIndex && CompletedActions == Other.CompletedActions
        && Result == Other.Result && ConfusionActionsRemaining == Other.ConfusionActionsRemaining
        && bCardsDisabled == Other.bCardsDisabled && GhostPhase == Other.GhostPhase
        && GhostPlacementsCompleted == Other.GhostPlacementsCompleted && Random.GetInitialSeed() == Other.Random.GetInitialSeed()
        && Random.GetCurrentSeed() == Other.Random.GetCurrentSeed();
}
int32 SingleOpponentIndex(const FMatchState& State, int32 PlayerIndex)
{
    if (State.Players.Num() != 2 || !State.Players.IsValidIndex(PlayerIndex)) { return INDEX_NONE; }
    return PlayerIndex == 0 ? 1 : 0;
}
EStone OppositeStone(EStone Stone)
{
    if (Stone == EStone::Black) { return EStone::White; }
    if (Stone == EStone::White) { return EStone::Black; }
    return EStone::Empty;
}
EStone EffectivePlacementStone(const FPlayerState& Player, bool bConfused)
{ return bConfused ? OppositeStone(Player.AssignedStone) : Player.AssignedStone; }
}
