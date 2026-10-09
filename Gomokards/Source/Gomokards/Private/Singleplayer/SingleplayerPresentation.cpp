#include "Singleplayer/SingleplayerPresentation.h"
namespace Gomokards
{
FSingleplayerView MakeSingleplayerView(const FSingleplayerBattleState& State, uint64 Generation)
{
    FSingleplayerView View;
    View.Board = State.Match.Board;
    View.Result = State.Match.Result;
    View.CurrentSide = State.Match.CurrentPlayerIndex;
    View.Actions = State.Match.CompletedActions;
    View.Generation = Generation;
    View.Hand = State.PlayerDeck.Hand;
    View.DrawCount = State.PlayerDeck.DrawPile.Num();
    View.DiscardCount = State.PlayerDeck.DiscardPile.Num();
    View.ExhaustCount = State.PlayerDeck.ExhaustPile.Num();
    View.bReady = true;
    return View;
}
FDemoCardPresentation SingleplayerCardPresentation(ECardId Id)
{
    switch (Id)
    {
    case ECardId::Polarity: return {uint8(Id), TEXT("两极反转"), TEXT("翻转所选 2×2 区域内所有棋子的颜色。")};
    case ECardId::Barrier: return {uint8(Id), TEXT("阴阳屏障"), TEXT("在所选区域放置屏障，阻断穿过该区域的棋子连线。")};
    case ECardId::TacticalNuke: return {uint8(Id), TEXT("战术核弹"), TEXT("清空并永久封锁一个目标位置。")};
    case ECardId::Restock: return {uint8(Id), TEXT("补充库存"), TEXT("抽取至多 3 张牌，手牌最多 5 张。")};
    default: return {0, TEXT(""), TEXT(""), false};
    }
}
FString SingleplayerResultText(const FMatchResult& Result)
{
    if (Result.Status == EMatchStatus::Draw) { return TEXT("平局"); }
    if (Result.Status == EMatchStatus::Won) { return Result.WinningStone == EStone::Black ? TEXT("玩家获胜") : TEXT("AI 获胜"); }
    return {};
}
FString SingleplayerErrorText(ESingleplayerError Error)
{
    switch (Error)
    {
    case ESingleplayerError::None: return {};
    case ESingleplayerError::Stopped: return TEXT("对局已结束，可重新开始。");
    case ESingleplayerError::WrongTurn: return TEXT("请等待 AI 完成落子。");
    case ESingleplayerError::Occupied: return TEXT("此处已有棋子。");
    case ESingleplayerError::Forbidden: return TEXT("此处已被战术核弹封锁。");
    case ESingleplayerError::InvalidTarget: return TEXT("目标无效，请重新选择。");
    case ESingleplayerError::NoDrawableCard: return TEXT("没有可抽取的牌。");
    case ESingleplayerError::StaleAction: return TEXT("局面已改变，请重新选择。");
    default: return TEXT("行动无效，局面未改变。");
    }
}
bool IsSingleplayerExitKey(const FKey& Key) { return Key == EKeys::Escape; }
TOptional<FSingleplayerActionRequest> FSingleplayerSelection::Draw(const FSingleplayerView& View)
{
    if (!View.CanDraw()) { return {}; }
    Clear();
    if (View.IsReplace()) { bReplacing = true; return {}; }
    return FSingleplayerActionRequest{ESingleplayerActionType::Draw, View.Actions};
}
TOptional<FSingleplayerActionRequest> FSingleplayerSelection::HandClick(const FSingleplayerView& View, int32 Index)
{
    if (!View.CanAct() || !View.Hand.IsValidIndex(Index)) { return {}; }
    const auto Card = View.Hand[Index];
    if (bReplacing) { return FSingleplayerActionRequest{ESingleplayerActionType::Replace, View.Actions, Card.InstanceId}; }
    if (BoardEffectTarget(Card.Id) == ECardTarget::None)
    { return FSingleplayerActionRequest{ESingleplayerActionType::PlayCard, View.Actions, Card.InstanceId}; }
    if (TargetInstance == Card.InstanceId) { Clear(); }
    else { TargetInstance = Card.InstanceId; TargetCard = Card.Id; }
    return {};
}
TOptional<FSingleplayerActionRequest> FSingleplayerSelection::BoardClick(const FSingleplayerView& View, FIntPoint Point) const
{
    if (!View.CanAct() || bReplacing) { return {}; }
    return FSingleplayerActionRequest{TargetInstance == INDEX_NONE ? ESingleplayerActionType::PlaceStone : ESingleplayerActionType::PlayCard,
        View.Actions, TargetInstance, Point};
}
FString FSingleplayerSelection::Instruction() const
{
    if (bReplacing) { return TEXT("请选择一张手牌进行替换；右键取消。"); }
    switch (TargetCard)
    {
    case ECardId::Polarity: return TEXT("选择 2×2 区域的左上交点；右键取消。");
    case ECardId::Barrier: return TEXT("选择四个交点之间的格心；右键取消。");
    case ECardId::TacticalNuke: return TEXT("选择要封锁的交点；右键取消。");
    default: return TEXT("落子、出牌或抽牌，任选其一结束本回合。Esc 退出游戏。");
    }
}
}
