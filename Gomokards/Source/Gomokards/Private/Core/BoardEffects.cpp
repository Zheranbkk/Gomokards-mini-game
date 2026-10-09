#include "Core/BoardEffects.h"
namespace Gomokards
{
bool ApplyTacticalNuke(FBoard& Board, FIntPoint Target)
{
    if (!FBoard::Contains(Target)) { return false; }
    Board.At(Target) = {EStone::Empty, true};
    return true;
}
bool ApplyPolarity(FBoard& Board, FIntPoint Anchor)
{
    if (!FBoard::ContainsAnchor(Anchor)) { return false; }
    for (FIntPoint Corner : FBoard::RegionCorners(Anchor))
    { Board.At(Corner).Stone = OppositeStone(Board.At(Corner).Stone); }
    return true;
}
bool ApplyBarrier(FBoard& Board, FIntPoint Anchor)
{
    if (!FBoard::ContainsAnchor(Anchor)) { return false; }
    Board.Barriers.AddUnique(Anchor);
    return true;
}
}
