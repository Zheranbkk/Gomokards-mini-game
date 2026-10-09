#include "Presentation/MatchPresentation.h"

namespace Gomokards
{
TOptional<FIntPoint> FBoardLayout::ToCoordinate(FVector2D Local)
{
    if (!FMath::IsFinite(Local.X) || !FMath::IsFinite(Local.Y)
        || Local.X < 0 || Local.Y < 0 || Local.X >= Extent || Local.Y >= Extent) { return {}; }
    return FIntPoint(FMath::FloorToInt(Local.X / CellSize), FMath::FloorToInt(Local.Y / CellSize));
}
FVector2D FBoardLayout::Center(FIntPoint Coordinate)
{ return FVector2D((Coordinate.X + .5f) * CellSize, (Coordinate.Y + .5f) * CellSize); }
TOptional<FIntPoint> FBoardLayout::TargetAt(FVector2D Local, ECardId Card)
{
    const ECardTarget Domain = BoardEffectTarget(Card);
    // A cell's hit area is the square between its four intersections, not a stone's hit box.
    auto Target = ToCoordinate(Domain == ECardTarget::CellCenter ? Local-Center({0,0}) : Local);
    if (Target.IsSet() && (Domain == ECardTarget::CellCenter || Domain == ECardTarget::RegionTopLeft)
        && !FBoard::ContainsAnchor(Target.GetValue())) { Target.Reset(); }
    return Target;
}
TStaticArray<FVector2D, 4> FBoardLayout::BarrierCross(FIntPoint Anchor)
{
    const auto Corners = FBoard::RegionCorners(Anchor);
    const FVector2D A=Center(Corners[0]), B=Center(Corners[1]), C=Center(Corners[2]), D=Center(Corners[3]);
    // Visual overhang only: logical corners, anchor domain and blocked links are unchanged.
    const float Overhang = CellSize * .125f;
    return {(A+C)*.5-FVector2D(Overhang,0), (B+D)*.5+FVector2D(Overhang,0),
        (A+B)*.5-FVector2D(0,Overhang), (C+D)*.5+FVector2D(0,Overhang)};
}
FLinearColor StoneDisplayColor(const FMatchState& State, EStone Stone)
{
    if (Stone == EStone::Empty) { return FLinearColor::Transparent; }
    if (State.GhostPhase == EGhostPhase::Hidden) { return FLinearColor(.45f,.45f,.45f); }
    return Stone == EStone::Black ? FLinearColor(.015f,.015f,.015f) : FLinearColor(.96f,.96f,.96f);
}
TOptional<ETetrisInput> TetrisInputForKey(const FKey& Key)
{
    if (Key==EKeys::Up) { return ETetrisInput::Up; }
    if (Key==EKeys::Down) { return ETetrisInput::Down; }
    if (Key==EKeys::Left) { return ETetrisInput::Left; }
    if (Key==EKeys::Right) { return ETetrisInput::Right; }
    if (Key==EKeys::SpaceBar) { return ETetrisInput::Rotate; }
    return {};
}
}
