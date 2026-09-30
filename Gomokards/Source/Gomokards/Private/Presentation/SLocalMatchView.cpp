#include "Presentation/SLocalMatchView.h"
#include "Runtime/LocalMatchPlayerController.h"
#include "Presentation/MatchPresentation.h"
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
                FSlateDrawElement::MakeBox(Out, Layer+2, G.ToPaintGeometry(FVector2D(24), FSlateLayoutTransform(Center-FVector2D(12))), &StoneBrush,
                    ESlateDrawEffect::None, Cell.Stone==1 ? FLinearColor(.04f,.04f,.05f) : FLinearColor(.94f,.94f,.9f));
            }
        }
        const auto DrawBarrier = [&](FIntPoint Anchor, FLinearColor Color)
        {
            const auto Ends = FBoardLayout::BarrierCross(Anchor);
            FSlateDrawElement::MakeLines(Out, Layer+4, G.ToPaintGeometry(), TArray<FVector2D>{Ends[0],Ends[1]}, ESlateDrawEffect::None, Color, true, 3);
            FSlateDrawElement::MakeLines(Out, Layer+4, G.ToPaintGeometry(), TArray<FVector2D>{Ends[2],Ends[3]}, ESlateDrawEffect::None, Color, true, 3);
        };
        for (FIntPoint Anchor : Board.Barriers) { DrawBarrier(Anchor, FLinearColor(0,.8f,1)); }
        const auto Target=Hover.IsSet() ? FBoardLayout::ToCoordinate(Hover.GetValue()) : TOptional<FIntPoint>{};
        if (Target.IsSet() && Pinned->CanPlace(Target.GetValue()))
        {
            FLinearColor Preview=Pinned->PreviewStone()==1 ? FLinearColor(.04f,.04f,.05f,.35f) : FLinearColor(.94f,.94f,.9f,.35f);
            FSlateDrawElement::MakeBox(Out,Layer+5,G.ToPaintGeometry(FVector2D(24),FSlateLayoutTransform(FBoardLayout::Center(Target.GetValue())-FVector2D(12))),
                &StoneBrush,ESlateDrawEffect::None,Preview);
        }
        return Layer+5;
    }
    virtual FReply OnMouseMove(const FGeometry& G, const FPointerEvent& E) override
    { Hover = G.AbsoluteToLocal(E.GetScreenSpacePosition()); Invalidate(EInvalidateWidgetReason::Paint); return FReply::Handled(); }
    virtual void OnMouseLeave(const FPointerEvent& E) override { Hover.Reset(); Invalidate(EInvalidateWidgetReason::Paint); SLeafWidget::OnMouseLeave(E); }
    virtual FReply OnMouseButtonDown(const FGeometry& G, const FPointerEvent& E) override
    {
        const auto Pinned = View.Pin(); if (!Pinned) { return FReply::Unhandled(); }
        if (E.GetEffectingButton() == EKeys::LeftMouseButton)
        {
            const auto Coordinate = FBoardLayout::ToCoordinate(G.AbsoluteToLocal(E.GetScreenSpacePosition()));
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
    ChildSlot
    [SNew(SBorder).Padding(20).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.035f,.045f,.065f))
        [SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,10)
            [SNew(STextBlock).Text(FText::FromString(TEXT("GOMOKARDS | Phase 4B.1"))).Font(FCoreStyle::GetDefaultFontStyle("Bold",22))]
            +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)
            [SNew(STextBlock).Text_Lambda([this]{return FText::FromString(Owner->StatusLabel());}).ColorAndOpacity(FLinearColor(.95f,.8f,.35f))]
            +SVerticalBox::Slot().AutoHeight()
            [SNew(SHorizontalBox)
                +SHorizontalBox::Slot().AutoWidth()
                [SNew(SBox).WidthOverride(FBoardLayout::Extent).HeightOverride(FBoardLayout::Extent)
                    [SAssignNew(BoardView,SMatchBoard).View(SharedThis(this))]]
                +SHorizontalBox::Slot().FillWidth(1).Padding(20,0,0,0)
                [SNew(SVerticalBox)
                    +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,10)
                    [SNew(SButton).Text(FText::FromString(TEXT("Development server restart")))
                        .IsEnabled_Lambda([this]{return Owner->CanDevelopmentRestart();})
                        .OnClicked_Lambda([this]{Owner->RequestDevelopmentRestart();return FReply::Handled();})]
                    +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,10)
                    [SNew(STextBlock).AutoWrapText(true).Text(FText::FromString(TEXT("Restock, Swap Hands and Steal are playable. Other cards await a later networking phase.\nOnly your card contents are shown. Both hand counts are public.")))]
                    +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,10)
                    [SNew(STextBlock).Text_Lambda([this]{const auto& V=GetPublicView(); return FText::FromString(FString::Printf(TEXT("Basics: %s | Confusion: %d"),V.bCardsDisabled ? TEXT("on") : TEXT("off"),V.ConfusionRemaining));})]
                    +SVerticalBox::Slot().FillHeight(1)[SNew(SScrollBox)+SScrollBox::Slot()[SAssignNew(Hands,SVerticalBox)]]
                ]]
            +SVerticalBox::Slot().AutoHeight().Padding(0,12,0,0)
            [SNew(STextBlock).AutoWrapText(true).Text_Lambda([this]{return FText::FromString(Owner->GetFeedback());})]
        ]
    ];
    ChangedHandle=Owner->OnPresentationChanged.AddSP(this,&SLocalMatchView::Refresh);
    Refresh();
}
SLocalMatchView::~SLocalMatchView()
{ if (Owner.IsValid()) { Owner->OnPresentationChanged.Remove(ChangedHandle); } }
const FMatchPublicView& SLocalMatchView::GetPublicView() const { return Owner->GetPublicView(); }
bool SLocalMatchView::CanPlace(FIntPoint Point) const { return Owner->CanPlace(Point); }
uint8 SLocalMatchView::PreviewStone() const
{
    const uint8 Stone=Owner->GetPrivateView().Stone;
    return GetPublicView().ConfusionRemaining>0 ? (Stone==1 ? 2 : 1) : Stone;
}
void SLocalMatchView::Refresh()
{
    BoardView->Invalidate(EInvalidateWidgetReason::Paint);
    Hands->ClearChildren();
    const auto& Public=Owner->GetPublicView();
    const auto& Private=Owner->GetPrivateView();
    for (const auto& Seat : Public.Seats)
    {
        Hands->AddSlot().AutoHeight().Padding(0,8)
            [SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("%s hand: %d%s"),*StoneLabel(static_cast<EStone>(Seat.Stone)),Seat.HandCount,
                Seat.PlayerId==Private.PlayerId ? TEXT(" (you)") : TEXT(""))))];
    }
    // Only the owner-private projection can provide card IDs; there is no opponent-hand query.
    for (uint8 Card : Private.Hand)
    {
        Hands->AddSlot().AutoHeight().Padding(0,2)
            [SNew(SButton)
                .IsEnabled_Lambda([this,Card]{return Owner->CanPlayCard(Card);})
                .Text(FText::FromString(CardLabel(static_cast<ECardId>(Card)) + (IsNetworkCardEnabled(Card) ? TEXT("") : TEXT(" — networking not enabled yet"))))
                .OnClicked_Lambda([this,Card]{Owner->RequestCard(Card);return FReply::Handled();})];
    }
}
void SLocalMatchView::BoardClick(FIntPoint Coordinate) { Owner->RequestPlace(Coordinate); }
