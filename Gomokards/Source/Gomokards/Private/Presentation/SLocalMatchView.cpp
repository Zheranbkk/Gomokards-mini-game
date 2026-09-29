#include "Presentation/SLocalMatchView.h"
#include "Runtime/LocalMatchGameMode.h"
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
        const FBoard& Board = Pinned->GetMatch().Board;
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
        for (int32 I=0; I<FBoard::Size*FBoard::Size; ++I)
        {
            const auto& Cell = Board.Cells[I];
            const FVector2D Center = FBoardLayout::Center(FBoard::ToCoordinate(I));
            if (Cell.bForbidden)
            {
                FSlateDrawElement::MakeBox(Out, Layer+2, G.ToPaintGeometry(FVector2D(26), FSlateLayoutTransform(Center-FVector2D(13))), White, ESlateDrawEffect::None, FLinearColor(.6f,.08f,.05f));
                FSlateDrawElement::MakeLines(Out, Layer+3, G.ToPaintGeometry(), {Center-FVector2D(8),Center+FVector2D(8)}, ESlateDrawEffect::None, FLinearColor::White, true, 2);
                FSlateDrawElement::MakeLines(Out, Layer+3, G.ToPaintGeometry(), {Center+FVector2D(-8,8),Center+FVector2D(8,-8)}, ESlateDrawEffect::None, FLinearColor::White, true, 2);
            }
            else if (Cell.Stone != EStone::Empty)
            {
                FSlateDrawElement::MakeBox(Out, Layer+2, G.ToPaintGeometry(FVector2D(24), FSlateLayoutTransform(Center-FVector2D(12))), &StoneBrush,
                    ESlateDrawEffect::None, Cell.Stone == EStone::Black ? FLinearColor(.015f,.015f,.015f) : FLinearColor(.96f,.96f,.96f));
            }
        }
        if (Pinned->IsTargeting() && Hover.IsSet())
        {
            const FVector2D Center = FBoardLayout::Center(Hover.GetValue());
            FSlateDrawElement::MakeLines(Out, Layer+4, G.ToPaintGeometry(),
                {Center+FVector2D(-14,-14),Center+FVector2D(14,-14),Center+FVector2D(14,14),Center+FVector2D(-14,14),Center+FVector2D(-14,-14)},
                ESlateDrawEffect::None, FLinearColor::Yellow, true, 2);
        }
        return Layer+4;
    }
    virtual FReply OnMouseMove(const FGeometry& G, const FPointerEvent& E) override
    { Hover = FBoardLayout::ToCoordinate(G.AbsoluteToLocal(E.GetScreenSpacePosition())); Invalidate(EInvalidateWidgetReason::Paint); return FReply::Handled(); }
    virtual void OnMouseLeave(const FPointerEvent& E) override { Hover.Reset(); Invalidate(EInvalidateWidgetReason::Paint); SLeafWidget::OnMouseLeave(E); }
    virtual FReply OnMouseButtonDown(const FGeometry& G, const FPointerEvent& E) override
    {
        const auto Pinned = View.Pin(); if (!Pinned) { return FReply::Unhandled(); }
        if (E.GetEffectingButton() == EKeys::RightMouseButton) { Pinned->Cancel(); }
        else if (E.GetEffectingButton() == EKeys::LeftMouseButton)
        {
            const auto Coordinate = FBoardLayout::ToCoordinate(G.AbsoluteToLocal(E.GetScreenSpacePosition()));
            if (Coordinate.IsSet()) { Pinned->BoardClick(Coordinate.GetValue()); }
        }
        return FReply::Handled().SetUserFocus(Pinned.ToSharedRef());
    }
private:
    TWeakPtr<SLocalMatchView> View;
    TOptional<FIntPoint> Hover;
};

void SLocalMatchView::Construct(const FArguments& Args)
{
    Owner = Args._Owner;
    Feedback = TEXT("Place a stone or play a card. Cards are earned only by successful blocking.");
    ChildSlot
    [SNew(SBorder).Padding(20).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.035f,.045f,.065f))
        [SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,10)
            [SNew(STextBlock).Text(FText::FromString(TEXT("GOMOKARDS  |  local hot-seat"))).Font(FCoreStyle::GetDefaultFontStyle("Bold",22))]
            +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)
            [SNew(STextBlock).Text_Lambda([this]{return FText::FromString(ResultLabel(GetMatch()));}).ColorAndOpacity(FLinearColor(.95f,.8f,.35f)).Font(FCoreStyle::GetDefaultFontStyle("Bold",16))]
            +SVerticalBox::Slot().AutoHeight()
            [SNew(SHorizontalBox)
                +SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(FBoardLayout::Extent).HeightOverride(FBoardLayout::Extent)
                    [SAssignNew(BoardView,SMatchBoard).View(SharedThis(this))]]
                +SHorizontalBox::Slot().FillWidth(1).Padding(20,0,0,0)
                [SNew(SVerticalBox)
                    +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,10)
                    [SNew(SButton).Text(FText::FromString(TEXT("New Match / Restart"))).OnClicked_Lambda([this]{Owner->NewMatch(); Feedback=TEXT("New match. Black starts; both hands are empty."); return FReply::Handled().SetUserFocus(SharedThis(this));})]
                    +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,10)
                    [SNew(STextBlock).AutoWrapText(true).Text_Lambda([this]{return FText::FromString(Selection.IsActive() ? TEXT("NUKE TARGETING\nClick a point. Right-click, Escape or click Nuke again to cancel.") : TEXT("PLACEMENT MODE\nClick a point to place a stone.\nOnly the active hand can play cards."));})]
                    +SVerticalBox::Slot().FillHeight(1)[SNew(SScrollBox)+SScrollBox::Slot()[SAssignNew(Hands,SVerticalBox)]]
                    +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).AutoWrapText(true).Text(FText::FromString(TEXT("Development view: both hands visible.\nRed X = forbidden point.\nNo hidden hand rule is implied.")))]
                ]]
            +SVerticalBox::Slot().AutoHeight().Padding(0,12,0,0)
            [SNew(STextBlock).AutoWrapText(true).Text_Lambda([this]{return FText::FromString(Feedback);})]
        ]
    ];
    ChangedHandle = Owner->OnMatchChanged.AddSP(this, &SLocalMatchView::Refresh);
    Refresh();
}
SLocalMatchView::~SLocalMatchView() { if (Owner.IsValid()) { Owner->OnMatchChanged.Remove(ChangedHandle); } }
const FMatchState& SLocalMatchView::GetMatch() const { return Owner->GetMatch(); }
void SLocalMatchView::Refresh()
{
    Selection.Clear();
    BoardView->Invalidate(EInvalidateWidgetReason::Paint);
    Hands->ClearChildren();
    const auto& Match = GetMatch();
    for (int32 Index=0; Index<Match.Players.Num(); ++Index)
    {
        const auto& Player = Match.Players[Index];
        Hands->AddSlot().AutoHeight().Padding(0,10,0,5)
            [SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("%s hand (%d)%s"), *StoneLabel(Player.AssignedStone), Player.Hand.Num(), Index==Match.CurrentPlayerIndex ? TEXT("  < active") : TEXT("")))).Font(FCoreStyle::GetDefaultFontStyle("Bold",14))];
        if (Player.Hand.IsEmpty()) { Hands->AddSlot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(TEXT("No cards")))]; }
        for (ECardId Card : Player.Hand)
        {
            Hands->AddSlot().AutoHeight().Padding(0,2)
                [SNew(SButton).Text(FText::FromString(CardLabel(Card)))
                    .IsEnabled(Index==Match.CurrentPlayerIndex && Match.Result.Status==EMatchStatus::InProgress)
                    .OnClicked_Lambda([this, Id=Player.Id, Card]{return CardClick(Id,Card);})];
        }
    }
}
void SLocalMatchView::Submit(const FActionRequest& Request, const FString& ActionLabel)
{
    const auto Result = Owner->Submit(Request);
    Feedback = Result.IsAccepted() ? ActionLabel + (Result.bBlockingReward ? TEXT(" Successful block: +1 card.") : TEXT("")) : RejectionLabel(Result.Error);
}
void SLocalMatchView::BoardClick(FIntPoint Coordinate)
{
    if (GetMatch().Result.Status != EMatchStatus::InProgress) { Feedback=RejectionLabel(EActionError::MatchStopped); return; }
    const bool bTarget = Selection.IsActive();
    const auto Request = Selection.BoardRequest(GetMatch(),Coordinate);
    Submit(Request, FString::Printf(TEXT("%s at (%d, %d)."),bTarget ? TEXT("Nuke used") : TEXT("Stone placed"),Coordinate.X,Coordinate.Y));
}
FReply SLocalMatchView::CardClick(FPlayerId Player, ECardId Card)
{
    const auto* Definition = FindCardDefinition(Card);
    if (Definition && Definition->bRequiresTarget)
    {
        if (Selection.Toggle(GetMatch(),Card)) { BoardView->Invalidate(EInvalidateWidgetReason::Paint); Feedback=Selection.IsActive() ? TEXT("Nuke selected. No action spent yet.") : TEXT("Targeting cancelled. Match unchanged."); }
    }
    else { Selection.Clear(); Submit(FActionRequest::Play(Player,Card),CardLabel(Card)+TEXT(" played.")); }
    return FReply::Handled().SetUserFocus(SharedThis(this));
}
void SLocalMatchView::Cancel() { Selection.Clear(); BoardView->Invalidate(EInvalidateWidgetReason::Paint); Feedback=TEXT("Targeting cancelled. Match unchanged."); }
FReply SLocalMatchView::OnKeyDown(const FGeometry& G, const FKeyEvent& E)
{ if(E.GetKey()==EKeys::Escape){Cancel(); return FReply::Handled();} return SCompoundWidget::OnKeyDown(G,E); }
FReply SLocalMatchView::OnMouseButtonDown(const FGeometry& G, const FPointerEvent& E)
{ if(E.GetEffectingButton()==EKeys::RightMouseButton){Cancel();return FReply::Handled();} return SCompoundWidget::OnMouseButtonDown(G,E); }
