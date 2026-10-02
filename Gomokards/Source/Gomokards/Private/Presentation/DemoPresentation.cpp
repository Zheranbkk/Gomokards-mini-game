#include "Presentation/DemoPresentation.h"
#include "Misc/Paths.h"
#include "Fonts/CompositeFont.h"
#include "Core/MatchState.h"

namespace Gomokards
{
namespace
{
const FDemoCardPresentation DemoCardEntries[] = {
    {ECardId::Restock,TEXT("补充库存"),TEXT("抽取 2 张卡牌。"),TEXT("/Game/UI/Cards/T_CardArt_Restock_Placeholder.T_CardArt_Restock_Placeholder")},
    {ECardId::SwapHands,TEXT("战术换家"),TEXT("打出本牌后，与对手交换剩余手牌。"),TEXT("/Game/UI/Cards/T_CardArt_SwapHands_Placeholder.T_CardArt_SwapHands_Placeholder")},
    {ECardId::Steal,TEXT("取之有道"),TEXT("随机获得对手 1 张手牌；若对手无牌，则无事发生。"),TEXT("/Game/UI/Cards/T_CardArt_Steal_Placeholder.T_CardArt_Steal_Placeholder")},
    {ECardId::TacticalNuke,TEXT("战术核弹"),TEXT("清空并永久封锁一个目标位置。"),TEXT("/Game/UI/Cards/T_CardArt_TacticalNuke_Placeholder.T_CardArt_TacticalNuke_Placeholder")},
    {ECardId::Polarity,TEXT("两极反转"),TEXT("翻转所选 2×2 区域内所有棋子的颜色。"),TEXT("/Game/UI/Cards/T_CardArt_Polarity_Placeholder.T_CardArt_Polarity_Placeholder")},
    {ECardId::Confusion,TEXT("定位混淆"),TEXT("接下来 2 次成功行动共享混淆次数：落子颜色反转，出牌也会消耗 1 次。"),TEXT("/Game/UI/Cards/T_CardArt_Confusion_Placeholder.T_CardArt_Confusion_Placeholder")},
    {ECardId::Barrier,TEXT("阴阳屏障"),TEXT("在所选区域放置屏障，阻断穿过该区域的棋子连线。"),TEXT("/Game/UI/Cards/T_CardArt_Barrier_Placeholder.T_CardArt_Barrier_Placeholder")},
    {ECardId::BackToBasics,TEXT("回归基本功"),TEXT("清除定位混淆，并使双方本局后续无法再出牌。"),TEXT("/Game/UI/Cards/T_CardArt_BackToBasics_Placeholder.T_CardArt_BackToBasics_Placeholder")},
    {ECardId::Ghost,TEXT("幽灵棋子"),TEXT("记忆棋盘 5 秒后隐藏所有棋子颜色；完成 6 次成功落子后恢复并结算胜负。"),TEXT("/Game/UI/Cards/T_CardArt_Ghost_Placeholder.T_CardArt_Ghost_Placeholder")},
    {ECardId::Tetris,TEXT("俄罗斯方块"),TEXT("双方交替操控共 6 个方块；方块颜色与操作者相反，锁定后若形成 5 子及以上同色直线则消除。"),TEXT("/Game/UI/Cards/T_CardArt_Tetris_Placeholder.T_CardArt_Tetris_Placeholder")}
};
}
TConstArrayView<FDemoCardPresentation> DemoCards() { return DemoCardEntries; }
const FDemoCardPresentation* DemoCard(uint8 Id)
{ for (const auto& Card : DemoCardEntries) { if (uint8(Card.Id)==Id) { return &Card; } } return nullptr; }
const TCHAR* DemoCardBackPath() { return TEXT("/Game/UI/Cards/T_CardBack_Placeholder.T_CardBack_Placeholder"); }
FSlateFontInfo DemoFont(int32 Size)
{
    static const TSharedPtr<const FCompositeFont> Font=[]
    {
        auto Composite=MakeShared<FCompositeFont>();
        Composite->DefaultTypeface.Fonts.Emplace(TEXT("Regular"),FPaths::EngineContentDir()/TEXT("Slate/Fonts/Roboto-Regular.ttf"),EFontHinting::Default,EFontLoadingPolicy::LazyLoad);
        Composite->FallbackTypeface.Typeface.Fonts.Emplace(TEXT("Regular"),FPaths::EngineContentDir()/TEXT("Slate/Fonts/DroidSansFallback.ttf"),EFontHinting::Default,EFontLoadingPolicy::LazyLoad);
        return Composite;
    }();
    return FSlateFontInfo(Font,Size,TEXT("Regular"));
}
FString DemoSide(uint8 Stone) { return Stone==1 ? TEXT("黑方") : Stone==2 ? TEXT("白方") : TEXT("未分配"); }
uint8 DemoPlayerStone(const FMatchPublicView& View,int32 Id)
{ for (const auto& Seat : View.Seats) { if (Seat.PlayerId==Id) { return Seat.Stone; } } return 0; }
int32 DemoOpponentCount(const FMatchPublicView& View,int32 LocalId)
{
    if (LocalId==INDEX_NONE) { return 0; }
    for (const auto& Seat : View.Seats) { if (Seat.PlayerId!=LocalId) { return FMath::Max(0,Seat.HandCount); } }
    return 0;
}
uint8 DemoDisplayCard(const FMatchPublicView& View,uint8 LocalSelection)
{
    if (IsTargetedNetworkCardEnabled(LocalSelection)) { return LocalSelection; }
    if (View.GhostPhase!=EMatchGhostPhase::None) { return uint8(ECardId::Ghost); }
    if (View.bTetrisActive) { return uint8(ECardId::Tetris); }
    return View.LastPlayedCard;
}
FString DemoResult(const FMatchPublicView& View)
{
    if (View.Result==uint8(EMatchStatus::Won)) { return DemoSide(View.WinningStone)+TEXT("获胜"); }
    if (View.Result==uint8(EMatchStatus::Draw)) { return TEXT("平局"); }
    return {};
}
void FDemoGameLog::Add(FString Line)
{
    if (Line.IsEmpty()) { return; }
    Lines.Add(MoveTemp(Line));
    if (Lines.Num()>12) { Lines.RemoveAt(0,Lines.Num()-12); }
}
void FDemoGameLog::Update(const FMatchPublicView& P,const FMatchPrivateView& V)
{
    // Log only coherent, assigned snapshots. No early private/public cross-revision mixing.
    if (!P.Epoch || P.Epoch!=V.Epoch || P.Revision!=V.Revision || V.PlayerId==INDEX_NONE) { return; }
    if (bInitialized && P.Epoch==Epoch && P.Revision<=Revision) { return; }
    if (!bInitialized || P.Epoch!=Epoch)
    {
        Lines.Reset(); Add(TEXT("新对局。"));
        bInitialized=true; Epoch=P.Epoch; Revision=P.Revision; Actions=P.CompletedActions;
        LastCardAction=P.LastPlayedCardAction; PreviousPlayer=P.CurrentPlayerId;
        OwnerId=V.PlayerId; Hand=V.Hand; Seats=P.Seats;
        Ghost=P.GhostPhase; bTetris=P.bTetrisActive; Result=P.Result;
        return; // Joining a snapshot must not invent historical draws or actions.
    }
    const bool bNewCard=P.LastPlayedCardAction>LastCardAction;
    if (bNewCard)
    {
        if (const auto* Card=DemoCard(P.LastPlayedCard))
        { Add(DemoSide(DemoPlayerStone(P,P.LastPlayedCardActor))+FString::Printf(TEXT("打出【%s】。"),Card->Name)); }
    }
    // Coalesced snapshots are not an event stream; do not invent missing intermediate actions.
    if (P.CompletedActions==Actions+1 && P.LastPlayedCardAction!=P.CompletedActions)
    { Add(DemoSide(DemoPlayerStone(P,PreviousPlayer))+TEXT("落子。")); }
    for (const auto& Seat : P.Seats)
    {
        const auto* Old=Seats.FindByPredicate([&](const FMatchSeatView& S){return S.PlayerId==Seat.PlayerId;});
        if (Old && Seat.HandCount>Old->HandCount)
        { Add(DemoSide(Seat.Stone)+FString::Printf(TEXT("手牌增加了 %d 张。"),Seat.HandCount-Old->HandCount)); }
    }
    if (OwnerId==V.PlayerId && P.CompletedActions==Actions+1)
    {
        auto Remaining=Hand;
        if (bNewCard && P.LastPlayedCardActor==V.PlayerId)
        { const int32 Played=Remaining.Find(P.LastPlayedCard); if (Played!=INDEX_NONE) { Remaining.RemoveAt(Played); } }
        for (uint8 Id : V.Hand)
        {
            const int32 Old=Remaining.Find(Id);
            if (Old!=INDEX_NONE) { Remaining.RemoveAt(Old); }
            else if (const auto* Card=DemoCard(Id)) { Add(FString::Printf(TEXT("你获得了【%s】。"),Card->Name)); }
        }
    }
    if (P.GhostPhase!=Ghost)
    {
        if (P.GhostPhase==EMatchGhostPhase::Preparation) { Add(TEXT("幽灵棋子：记住棋盘。")); }
        else if (P.GhostPhase==EMatchGhostPhase::Hidden) { Add(TEXT("进入幽灵棋子隐藏阶段。")); }
        else if (Ghost!=EMatchGhostPhase::None) { Add(TEXT("幽灵棋子结束，棋子颜色已恢复。")); }
    }
    if (P.bTetrisActive!=bTetris) { Add(P.bTetrisActive ? TEXT("俄罗斯方块开始。") : TEXT("俄罗斯方块结束。")); }
    if (P.Result!=Result) { const auto Label=DemoResult(P); if (!Label.IsEmpty()) { Add(Label+TEXT("！")); } }
    Epoch=P.Epoch; Revision=P.Revision; Actions=P.CompletedActions; LastCardAction=P.LastPlayedCardAction;
    PreviousPlayer=P.CurrentPlayerId; OwnerId=V.PlayerId; Hand=V.Hand; Seats=P.Seats;
    Ghost=P.GhostPhase; bTetris=P.bTetrisActive; Result=P.Result;
}
}
