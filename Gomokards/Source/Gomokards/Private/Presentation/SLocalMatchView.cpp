#include "Presentation/SLocalMatchView.h"
#include "Runtime/LocalMatchPlayerController.h"
#include "Presentation/MatchPresentation.h"
#include "Presentation/DemoPresentation.h"
#include "Presentation/SDemoCards.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SSpacer.h"
#include "Core/TetrisRules.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Rendering/DrawElements.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/CoreStyle.h"

using namespace Gomokards;

namespace
{
FLinearColor DisplayStoneColor(uint8 Stone)
{
    switch (static_cast<EMatchDisplayStone>(Stone))
    {
    case EMatchDisplayStone::Black: return FLinearColor(.04f,.04f,.05f);
    case EMatchDisplayStone::White: return FLinearColor(.94f,.94f,.9f);
    case EMatchDisplayStone::HiddenOccupied: return FLinearColor(.45f,.45f,.45f);
    default: return FLinearColor::Transparent;
    }
}
}

class SMatchBoard : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SMatchBoard) {} SLATE_ARGUMENT(TWeakPtr<SLocalMatchView>, View) SLATE_END_ARGS()
    void Construct(const FArguments& Args) { View = Args._View; }
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(FBoardLayout::Extent); }
    virtual int32 OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&, FSlateWindowElementList& Out,
        int32 Layer, const FWidgetStyle&, bool) const override
    {
        const auto Pinned = View.Pin(); if (!Pinned) { return Layer; }
        const auto& Board = Pinned->GetPublicView();
        const auto* White = FCoreStyle::Get().GetBrush("WhiteBrush");
        FSlateDrawElement::MakeBox(Out, Layer, G.ToPaintGeometry(), White, ESlateDrawEffect::None, FLinearColor(.72f,.52f,.27f));
        for (int32 I=0; I<FBoard::Size; ++I)
        {
            const float V = FBoardLayout::Center({I,I}).X;
            const float First = FBoardLayout::Center({0,0}).X;
            const float Last = FBoardLayout::Center({FBoard::Size-1,FBoard::Size-1}).X;
            FSlateDrawElement::MakeLines(Out, Layer+1, G.ToPaintGeometry(), TArray<FVector2D>{{First,V},{Last,V}}, ESlateDrawEffect::None, FLinearColor(.15f,.12f,.08f), true);
            FSlateDrawElement::MakeLines(Out, Layer+1, G.ToPaintGeometry(), TArray<FVector2D>{{V,First},{V,Last}}, ESlateDrawEffect::None, FLinearColor(.15f,.12f,.08f), true);
        }
        static const FSlateRoundedBoxBrush StoneBrush(FLinearColor::White, 12.f);
        for (int32 I=0; I<Board.Cells.Num(); ++I)
        {
            const auto& Cell = Board.Cells[I];
            const FVector2D Center = FBoardLayout::Center(FBoard::ToCoordinate(I));
            if (Cell.bForbidden)
            {
                FSlateDrawElement::MakeBox(Out, Layer+2, G.ToPaintGeometry(FVector2D(26), FSlateLayoutTransform(Center-FVector2D(13))), White, ESlateDrawEffect::None, FLinearColor(.6f,.08f,.05f));
                FSlateDrawElement::MakeLines(Out, Layer+3, G.ToPaintGeometry(), {Center-FVector2D(8),Center+FVector2D(8)}, ESlateDrawEffect::None, FLinearColor::White, true, 2);
                FSlateDrawElement::MakeLines(Out, Layer+3, G.ToPaintGeometry(), {Center+FVector2D(-8,8),Center+FVector2D(8,-8)}, ESlateDrawEffect::None, FLinearColor::White, true, 2);
            }
            else if (Cell.Stone != 0)
            {
                FSlateDrawElement::MakeBox(Out, Layer+2, G.ToPaintGeometry(FVector2D(24), FSlateLayoutTransform(Center-FVector2D(12))), Board.bTetrisActive ? White : &StoneBrush,
                    ESlateDrawEffect::None, DisplayStoneColor(Cell.Stone));
            }
        }
        const auto DrawBarrier = [&](FIntPoint Anchor, FLinearColor Color)
        {
            const auto Ends = FBoardLayout::BarrierCross(Anchor);
            FSlateDrawElement::MakeLines(Out, Layer+4, G.ToPaintGeometry(), TArray<FVector2D>{Ends[0],Ends[1]}, ESlateDrawEffect::None, Color, true, 3);
            FSlateDrawElement::MakeLines(Out, Layer+4, G.ToPaintGeometry(), TArray<FVector2D>{Ends[2],Ends[3]}, ESlateDrawEffect::None, Color, true, 3);
        };
        for (FIntPoint Anchor : Board.Barriers) { DrawBarrier(Anchor, FLinearColor(0,.8f,1)); }
        const auto Card=static_cast<ECardId>(Pinned->SelectedCard());
        const auto Target=Hover.IsSet() ? FBoardLayout::TargetAt(Hover.GetValue(),Card) : TOptional<FIntPoint>{};
        if (Target.IsSet() && IsTargetedNetworkCardEnabled(uint8(Card)))
        {
            if (Card==ECardId::Barrier) { DrawBarrier(Target.GetValue(),FLinearColor::Yellow); }
            else
            {
                const FVector2D First=FBoardLayout::Center(Target.GetValue())-FVector2D(14);
                const FIntPoint LastPoint=Card==ECardId::Polarity ? FBoard::RegionCorners(Target.GetValue())[3] : Target.GetValue();
                const FVector2D Last=FBoardLayout::Center(LastPoint)+FVector2D(14);
                FSlateDrawElement::MakeLines(Out,Layer+5,G.ToPaintGeometry(),{First,{Last.X,First.Y},Last,{First.X,Last.Y},First},
                    ESlateDrawEffect::None,FLinearColor::Yellow,true,2);
            }
        }
        else if (Target.IsSet() && Pinned->SelectedCard()==0 && Pinned->CanPlace(Target.GetValue()))
        {
            FLinearColor Preview=DisplayStoneColor(Pinned->PreviewStone()); Preview.A*=.35f;
            FSlateDrawElement::MakeBox(Out,Layer+5,G.ToPaintGeometry(FVector2D(24),FSlateLayoutTransform(FBoardLayout::Center(Target.GetValue())-FVector2D(12))),
                &StoneBrush,ESlateDrawEffect::None,Preview);
        }
        if (const auto* Pose=Pinned->GetTetrisPose())
        {
            for (const auto Offset : TetrisOffsets(static_cast<ETetrisShape>(Pose->Shape),Pose->Rotation))
            {
                const FVector2D Center=FBoardLayout::Center(Pose->Origin+Offset);
                FSlateDrawElement::MakeBox(Out,Layer+6,G.ToPaintGeometry(FVector2D(28),FSlateLayoutTransform(Center-FVector2D(14))),White,ESlateDrawEffect::None,FLinearColor(1,.65f,.05f));
                FSlateDrawElement::MakeBox(Out,Layer+7,G.ToPaintGeometry(FVector2D(22),FSlateLayoutTransform(Center-FVector2D(11))),White,ESlateDrawEffect::None,DisplayStoneColor(Pose->Stone));
            }
        }
        return Layer+7;
    }
    virtual FReply OnMouseMove(const FGeometry& G, const FPointerEvent& E) override
    { Hover = G.AbsoluteToLocal(E.GetScreenSpacePosition()); Invalidate(EInvalidateWidgetReason::Paint); return FReply::Handled(); }
    virtual void OnMouseLeave(const FPointerEvent& E) override { Hover.Reset(); Invalidate(EInvalidateWidgetReason::Paint); SLeafWidget::OnMouseLeave(E); }
    virtual FReply OnMouseButtonDown(const FGeometry& G, const FPointerEvent& E) override
    {
        const auto Pinned = View.Pin(); if (!Pinned) { return FReply::Unhandled(); }
        if (E.GetEffectingButton()==EKeys::RightMouseButton) { Pinned->CancelTargeting(); }
        else if (E.GetEffectingButton() == EKeys::LeftMouseButton)
        {
            const auto Coordinate = FBoardLayout::TargetAt(G.AbsoluteToLocal(E.GetScreenSpacePosition()),static_cast<ECardId>(Pinned->SelectedCard()));
            Pinned->BoardClick(Coordinate.Get(FIntPoint(-1,-1)));
        }
        return FReply::Handled().SetUserFocus(Pinned.ToSharedRef());
    }
private:
    TWeakPtr<SLocalMatchView> View;
    TOptional<FVector2D> Hover;
};

void SLocalMatchView::Construct(const FArguments& Args)
{
    Owner=Args._Owner;
    Art=MakeShared<FDemoCardArt>();
    const auto* White=FCoreStyle::Get().GetBrush("WhiteBrush");
    const FLinearColor Ink(.09f,.11f,.13f), Panel(.93f,.93f,.90f);
    static const FSlateRoundedBoxBrush TurnDot(FLinearColor::Black,9.f);
    const auto SideRow=[this,Ink](uint8 Stone)
    {
        return SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [SNew(STextBlock).Font(DemoFont(21)).ColorAndOpacity(Ink)
                .Text_Lambda([this,Stone]{return FText::FromString(DemoSide(Stone)+(Owner->GetPrivateView().Stone==Stone ? TEXT("（你）") : TEXT("")));})]
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10,0)
            [SNew(SBox).WidthOverride(18).HeightOverride(18)
                [SNew(SImage).Image(&TurnDot).Visibility_Lambda([this,Stone]{return IsSideTurn(Stone) && bBlinkOn ? EVisibility::Visible : EVisibility::Hidden;})]];
    };
    ChildSlot
    [SNew(SBorder).Padding(0).BorderImage(White).BorderBackgroundColor(FLinearColor(.14f,.17f,.19f))
        [SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
            [SNew(SBox).WidthOverride(1920).HeightOverride(1112)
                [SNew(SBorder).Padding(12).BorderImage(White).BorderBackgroundColor(FLinearColor(.97f,.97f,.94f))
                    [SNew(SVerticalBox)
                        +SVerticalBox::Slot().AutoHeight()
                        [SNew(SBox).HeightOverride(124)[SAssignNew(OpponentHand,SDemoHand).Art(Art).Owner(Owner.Get()).Opponent(true).FocusTarget(SharedThis(this))]]
                        +SVerticalBox::Slot().AutoHeight()
                        [SNew(SBox).HeightOverride(700)
                            [SNew(SHorizontalBox)
                                +SHorizontalBox::Slot().FillWidth(.23f).Padding(0,4,12,4)
                                [SNew(SBorder).Padding(18).BorderImage(White).BorderBackgroundColor(Panel)
                                    [SNew(SVerticalBox)
                                        +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,12)
                                        [SNew(STextBlock).Text(FText::FromString(TEXT("GOMOKARDS\n对局记录"))).Font(DemoFont(21)).ColorAndOpacity(Ink)]
                                        +SVerticalBox::Slot().FillHeight(1)
                                        [SAssignNew(LogScroll,SScrollBox).Orientation(Orient_Vertical)+SScrollBox::Slot()
                                            [SNew(STextBlock).Font(DemoFont(15)).ColorAndOpacity(Ink).WrapTextAt(345).AutoWrapText(true)
                                                .Text_Lambda([this]{return FText::FromString(Owner->GetGameLog().Text());})]]]]
                                +SHorizontalBox::Slot().FillWidth(.54f)
                                [SNew(SOverlay)
                                    +SOverlay::Slot()
                                    [SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
                                        [SNew(SBox).WidthOverride(FBoardLayout::Extent).HeightOverride(FBoardLayout::Extent)
                                            [SAssignNew(BoardView,SMatchBoard).View(SharedThis(this))]]]
                                    +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
                                    [SNew(SBorder).Padding(32,20).BorderImage(White).BorderBackgroundColor(FLinearColor(.96f,.95f,.89f,.96f))
                                        .Visibility_Lambda([this]{return DemoResult(GetPublicView()).IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible;})
                                        [SNew(STextBlock).Font(DemoFont(28)).ColorAndOpacity(Ink).Text_Lambda([this]{return FText::FromString(DemoResult(GetPublicView()));})]]]
                                +SHorizontalBox::Slot().FillWidth(.23f).Padding(12,4,0,4)
                                [SNew(SBorder).Padding(14).BorderImage(White).BorderBackgroundColor(Panel)
                                    [SNew(SVerticalBox)
                                        +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,5)[SideRow(2)]
                                        +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)[SideRow(1)]
                                        +SVerticalBox::Slot().AutoHeight()
                                        [SNew(STextBlock).Font(DemoFont(14)).ColorAndOpacity(Ink).AutoWrapText(true)
                                            .Visibility_Lambda([this]{return !Owner->IsPresentationReady() || GetPublicView().Session!=EMatchSession::Playing || GetPublicView().Result==uint8(EMatchStatus::AwaitingRuleDecision) ? EVisibility::Visible : EVisibility::Collapsed;})
                                            .Text_Lambda([this]{return FText::FromString(Owner->StatusLabel());})]
                                        +SVerticalBox::Slot().AutoHeight()
                                        [SNew(STextBlock).Font(DemoFont(14)).ColorAndOpacity(Ink).AutoWrapText(true)
                                            .Text_Lambda([this]
                                            {
                                                const auto& V=GetPublicView(); FString Label;
                                                if (V.ConfusionRemaining>0) { Label=FString::Printf(TEXT("定位混淆 · 剩余 %d 次\n"),V.ConfusionRemaining); }
                                                if (V.bCardsDisabled) { Label+=TEXT("回归基本功 · 卡牌已禁用\n"); }
                                                Label+=Owner->GhostStatusLabel()+Owner->TetrisStatusLabel();
                                                return FText::FromString(Label);
                                            })]
                                        +SVerticalBox::Slot().AutoHeight().Padding(0,8,0,5)
                                        [SNew(STextBlock).Font(DemoFont(14)).ColorAndOpacity(Ink)
                                            .Text_Lambda([this]
                                            {
                                                if (SelectedCard()) { return FText::FromString(TEXT("待选择目标")); }
                                                if (GetPublicView().GhostPhase!=EMatchGhostPhase::None || GetPublicView().bTetrisActive) { return FText::FromString(TEXT("当前生效")); }
                                                return FText::FromString(GetPublicView().LastPlayedCard ? TEXT("最近出牌") : TEXT(""));
                                            })]
                                        +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                                        [SNew(SBox).WidthOverride(288).HeightOverride(357)
                                            .Visibility_Lambda([this]{return DemoDisplayCard(GetPublicView(),SelectedCard()) ? EVisibility::Visible : EVisibility::Collapsed;})
                                            [SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SDemoCard).Art(Art).Card_Lambda([this]{return DemoDisplayCard(GetPublicView(),SelectedCard());})
                                            .Visibility_Lambda([this]{return DemoDisplayCard(GetPublicView(),SelectedCard()) ? EVisibility::Visible : EVisibility::Collapsed;})]]]
                                        +SVerticalBox::Slot().FillHeight(1)[SNew(SSpacer)]
                                        +SVerticalBox::Slot().AutoHeight().Padding(0,8,0,0)
                                        [SNew(SButton).IsEnabled_Lambda([this]{return Owner->CanDevelopmentRestart();})
                                            .Visibility_Lambda([this]{return Owner->GetPrivateView().bDevelopmentAdmin ? EVisibility::Visible : EVisibility::Collapsed;})
                                            .OnClicked_Lambda([this]{Owner->RequestDevelopmentRestart();return FReply::Handled().SetUserFocus(SharedThis(this));})
                                            [SNew(STextBlock).Text(FText::FromString(TEXT("重新开始"))).Font(DemoFont(13))]]]]]]
                        +SVerticalBox::Slot().AutoHeight()
                        [SNew(SBox).HeightOverride(244)[SAssignNew(OwnHand,SDemoHand).Art(Art).Owner(Owner.Get()).Opponent(false).FocusTarget(SharedThis(this))]]
                        +SVerticalBox::Slot().AutoHeight()
                        [SNew(STextBlock).Font(DemoFont(14)).ColorAndOpacity(Ink).Justification(ETextJustify::Center).AutoWrapText(true)
                            .Visibility_Lambda([this]{return Owner->GetFeedback().IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;})
                            .Text_Lambda([this]{return FText::FromString(Owner->GetFeedback());})]
                    ]]]]
    ];
    ChangedHandle=Owner->OnPresentationChanged.AddSP(this,&SLocalMatchView::Refresh);
    Refresh();
}
void SLocalMatchView::Tick(const FGeometry& G,double CurrentTime,float DeltaTime)
{
    SCompoundWidget::Tick(G,CurrentTime,DeltaTime);
    if (CurrentTime>=NextBlink)
    { bBlinkOn=!bBlinkOn; NextBlink=CurrentTime+.5; Invalidate(EInvalidateWidgetReason::Layout); }
}
bool SLocalMatchView::IsSideTurn(uint8 Stone) const
{
    const auto& P=GetPublicView();
    if (P.Session!=EMatchSession::Playing || P.Result!=0) { return false; }
    const auto* Pose=GetTetrisPose();
    return DemoPlayerStone(P,Pose ? Pose->OperatorPlayerId : P.CurrentPlayerId)==Stone;
}
SLocalMatchView::~SLocalMatchView()
{ if (Owner.IsValid()) { Owner->OnPresentationChanged.Remove(ChangedHandle); } }
const FMatchPublicView& SLocalMatchView::GetPublicView() const { return Owner->GetPublicView(); }
const FMatchTetrisPose* SLocalMatchView::GetTetrisPose() const { return Owner->GetDisplayTetrisPose(); }
bool SLocalMatchView::CanPlace(FIntPoint Point) const { return Owner->CanPlace(Point); }
uint8 SLocalMatchView::PreviewStone() const
{
    if (GetPublicView().GhostPhase==EMatchGhostPhase::Hidden) { return uint8(EMatchDisplayStone::HiddenOccupied); }
    const uint8 Stone=Owner->GetPrivateView().Stone;
    return GetPublicView().ConfusionRemaining>0 ? (Stone==1 ? 2 : 1) : Stone;
}
void SLocalMatchView::Refresh()
{
    const FString CurrentLog=Owner->GetGameLog().Text();
    if (CurrentLog!=LastLogText) { LastLogText=CurrentLog; LogScroll->ScrollToEnd(); }
    BoardView->Invalidate(EInvalidateWidgetReason::Paint);
    OwnHand->Invalidate(EInvalidateWidgetReason::Paint);
    OpponentHand->Invalidate(EInvalidateWidgetReason::Paint);
    const auto& Public=Owner->GetPublicView();
    // Selection/feedback changes redraw; pose-only changes do not disturb hand hover or focus.
    if (HandEpoch!=Public.Epoch || HandRevision!=Public.Revision)
    { OwnHand->ResetHover(); HandEpoch=Public.Epoch; HandRevision=Public.Revision; }
    Invalidate(EInvalidateWidgetReason::Paint);
}
uint8 SLocalMatchView::SelectedCard() const { return Owner->GetSelectedTargetedCard(); }
void SLocalMatchView::CancelTargeting() { Owner->CancelTargeting(); }
void SLocalMatchView::BoardClick(FIntPoint Coordinate) { Owner->RequestBoardClick(Coordinate); }
FReply SLocalMatchView::OnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    if (GetPublicView().bTetrisActive)
    {
        const auto Input=TetrisInputForKey(Event.GetKey());
        if (Input.IsSet()) { Owner->RequestTetrisInput(static_cast<EMatchTetrisInput>(Input.GetValue())); return FReply::Handled(); }
    }
    if (Event.GetKey()==EKeys::Escape) { CancelTargeting(); return FReply::Handled(); }
    return SCompoundWidget::OnPreviewKeyDown(Geometry,Event);
}
FReply SLocalMatchView::OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
    if (Event.GetEffectingButton()==EKeys::RightMouseButton) { CancelTargeting(); return FReply::Handled(); }
    return SCompoundWidget::OnMouseButtonDown(Geometry,Event);
}
