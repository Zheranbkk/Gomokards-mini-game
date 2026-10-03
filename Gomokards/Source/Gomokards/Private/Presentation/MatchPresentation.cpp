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
    const auto* Definition = FindCardDefinition(Card);
    const ECardTarget Domain = Definition ? Definition->Target : ECardTarget::Intersection;
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
bool FTargetSelection::Toggle(const FMatchState& State, ECardId Selected)
{
    if (!CanPlayCards(State)) { Clear(); return false; }
    const auto* Definition = FindCardDefinition(Selected);
    const auto& Actor = State.Players[State.CurrentPlayerIndex];
    if (!Definition || !Definition->RequiresTarget() || !Actor.Hand.Contains(Selected)) { return false; }
    if (Card == Selected && Player == Actor.Id) { Clear(); }
    else { Card = Selected; Player = Actor.Id; }
    return true;
}
FActionRequest FTargetSelection::BoardRequest(const FMatchState& State, FIntPoint Target) const
{
    if (IsActive()) { return FActionRequest::Play(Player, Card, Target); }
    return FActionRequest::Place(State.Players[State.CurrentPlayerIndex].Id, Target);
}
FLinearColor StoneDisplayColor(const FMatchState& State, EStone Stone)
{
    if (Stone == EStone::Empty) { return FLinearColor::Transparent; }
    if (State.GhostPhase == EGhostPhase::Hidden) { return FLinearColor(.45f,.45f,.45f); }
    return Stone == EStone::Black ? FLinearColor(.015f,.015f,.015f) : FLinearColor(.96f,.96f,.96f);
}
FString GhostLabel(const FMatchState& State, double PreparationSeconds)
{
    if (State.GhostPhase == EGhostPhase::Preparation)
    { return FString::Printf(TEXT("GHOST: Memorize the board - %.1fs. Gameplay frozen."),FMath::Max(0.0,PreparationSeconds)); }
    if (State.GhostPhase == EGhostPhase::Hidden)
    { return FString::Printf(TEXT("GHOST: Colors hidden - %d placements remaining. Cards temporarily unavailable."),FMatchState::GhostPlacementLimit-State.GhostPlacementsCompleted); }
    return {};
}
FString CardLabel(ECardId Card)
{
    switch (Card)
    {
    case ECardId::Restock: return TEXT("Restock (+2 cards)");
    case ECardId::SwapHands: return TEXT("Swap Hands");
    case ECardId::Steal: return TEXT("Steal (1 card)");
    case ECardId::TacticalNuke: return TEXT("Tactical Nuke (target)");
    case ECardId::Polarity: return TEXT("Polarity (2x2 target)");
    case ECardId::Confusion: return TEXT("Confusion (next 2 actions)");
    case ECardId::Barrier: return TEXT("Barrier (cell center)");
    case ECardId::Ghost: return TEXT("Ghost (5s memorize, 6 placements)");
    case ECardId::Tetris: return TEXT("Tetris (6 alternating blocks)");
    case ECardId::BackToBasics: return TEXT("Back to Basics (disable cards)");
    default: return TEXT("Unavailable card");
    }
}
FString StoneLabel(EStone Stone) { return Stone == EStone::Black ? TEXT("Black") : Stone == EStone::White ? TEXT("White") : TEXT("None"); }
FString EffectLabel(const FMatchState& State)
{
    const FString CardStatus = State.bCardsDisabled
        ? TEXT("BACK TO BASICS: cards disabled until restart. Remaining cards are inert.") : State.Tetris.bActive ? TEXT("Cards temporarily unavailable during Tetris.") : State.GhostPhase != EGhostPhase::None ? TEXT("Cards temporarily unavailable during Ghost.") : TEXT("Cards enabled.");
    if (State.Tetris.bActive && State.ConfusionActionsRemaining>0)
    { return CardStatus+FString::Printf(TEXT("\nCONFUSION: %d action(s) saved for ordinary play; does not affect Tetris."),State.ConfusionActionsRemaining); }
    if (State.ConfusionActionsRemaining > 0 && State.GhostPhase == EGhostPhase::Hidden)
    { return CardStatus + FString::Printf(TEXT("\nCONFUSION: %d successful action(s) remain. Placement color hidden."),State.ConfusionActionsRemaining); }
    if (State.ConfusionActionsRemaining > 0)
    { return CardStatus + FString::Printf(TEXT("\nCONFUSION: %d successful action(s) remain. Placement color: %s."),
        State.ConfusionActionsRemaining, *StoneLabel(EffectivePlacementStone(State.Players[State.CurrentPlayerIndex],true))); }
    return CardStatus + TEXT(" No Confusion active.");
}
FString TargetingLabel(ECardId Selected)
{
    FString Label;
    switch (Selected)
    {
    case ECardId::TacticalNuke: Label=TEXT("NUKE: select one intersection."); break;
    case ECardId::Polarity: Label=TEXT("POLARITY: select the top-left intersection of a 2x2 region (last row/column invalid)."); break;
    case ECardId::Barrier: Label=TEXT("BARRIER: select the CENTER of a cell between four intersections."); break;
    default: return TEXT("PLACEMENT MODE\nClick an intersection to place a stone. Only the active hand can play cards.");
    }
    return Label + TEXT("\nRight-click or reselect the active card to cancel. Escape exits the game.");
}
FString ResultLabel(const FMatchState& State)
{
    if (State.Tetris.bActive)
    { return FString::Printf(TEXT("TETRIS  |  Completed actions: %llu  |  %s resumes ordinary play"),State.CompletedActions,*StoneLabel(State.Players[State.CurrentPlayerIndex].AssignedStone)); }
    if (State.Result.Status == EMatchStatus::Draw) { return TEXT("Draw: both colors have five in a row. Start a new match."); }
    if (State.Result.Status == EMatchStatus::Won) { return StoneLabel(State.Result.WinningStone) + TEXT(" wins!  Start a new match to play again."); }
    if (State.Result.Status == EMatchStatus::AwaitingRuleDecision)
    { return TEXT("Awaiting rule decision: no legal action. No winner/draw assigned. Restart available."); }
    return FString::Printf(TEXT("%s to act  |  Completed actions: %llu"), *StoneLabel(State.Players[State.CurrentPlayerIndex].AssignedStone), State.CompletedActions);
}
FString RejectionLabel(EActionError Error)
{
    switch (Error)
    {
    case EActionError::TetrisActive: return TEXT("俄罗斯方块进行中，请使用方向键和空格。");
    case EActionError::GhostPreparation: return TEXT("请记住棋盘，倒计时结束后继续。");
    case EActionError::GhostCardsRestricted: return TEXT("隐藏阶段无法出牌。");
    case EActionError::CardsDisabled: return TEXT("回归基本功：本局无法再出牌。");
    case EActionError::Occupied: return TEXT("此处已有棋子。");
    case EActionError::Forbidden: return TEXT("此处已被战术核弹封锁。");
    case EActionError::InvalidCoordinate: return TEXT("请选择棋盘内的交点。");
    case EActionError::InvalidTarget: return TEXT("目标无效，请重新选择。");
    case EActionError::MatchStopped: return TEXT("对局已结束。");
    case EActionError::WrongPlayer: return TEXT("当前不是你的回合。");
    case EActionError::CardNotOwned: return TEXT("手中没有这张卡牌。");
    case EActionError::UnsupportedCard: return TEXT("当前无法出牌。");
    default: return TEXT("行动无效，局面未改变。");
    }
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
FString TetrisLabel(const FMatchState& State)
{
    if (!State.Tetris.bActive) { return {}; }
    const auto& T=State.Tetris;
    const TCHAR* Edge=TEXT(""); const TCHAR* Gravity=TEXT("");
    switch (T.Edge)
    {
    case ETetrisEdge::Top: Edge=TEXT("Top"); Gravity=TEXT("Down"); break;
    case ETetrisEdge::Bottom: Edge=TEXT("Bottom"); Gravity=TEXT("Up"); break;
    case ETetrisEdge::Left: Edge=TEXT("Left"); Gravity=TEXT("Right"); break;
    case ETetrisEdge::Right: Edge=TEXT("Right"); Gravity=TEXT("Left"); break;
    }
    return FString::Printf(TEXT("TETRIS: block %d / 6\n%s controls %s\nSpawn: %s | Gravity: %s (0.5s)\nArrows = absolute movement | Space = clockwise rotate\nOnly blocked automatic gravity locks a block."),
        T.BlockNumber,*StoneLabel(State.Players[T.OperatorIndex].AssignedStone),*StoneLabel(T.Stone),Edge,Gravity);
}
}
