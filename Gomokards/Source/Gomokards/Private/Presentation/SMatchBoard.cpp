#include "Presentation/SMatchBoard.h"
#include "Rendering/DrawElements.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/CoreStyle.h"
using namespace Gomokards;
namespace
{
FLinearColor DisplayStoneColor(EStone Stone, bool Hidden=false)
{
    if (Stone==EStone::Empty) { return FLinearColor::Transparent; }
    if (Hidden) { return FLinearColor(.45f,.45f,.45f); }
    return Stone==EStone::Black ? FLinearColor(.04f,.04f,.05f) : FLinearColor(.94f,.94f,.9f);
}
}
void SMatchBoard::Construct(const FArguments& Args)
{
    Display=Args._Display; OnBoardClicked=Args._OnBoardClicked; OnCancel=Args._OnCancel;
    CanPreviewPlacement=Args._CanPreviewPlacement; FocusTarget=Args._FocusTarget;
}
    int32 SMatchBoard::OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&, FSlateWindowElementList& Out,
        int32 Layer, const FWidgetStyle&, bool) const
    {
        const auto Data = Display.Get();
        const auto& Board = Data.Board;
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
                FSlateDrawElement::MakeBox(Out, Layer+2, G.ToPaintGeometry(FVector2D(24), FSlateLayoutTransform(Center-FVector2D(12))), Data.bSquareStones ? White : &StoneBrush,
                    ESlateDrawEffect::None, DisplayStoneColor(Cell.Stone,Data.bHideColors));
            }
        }
        const auto DrawBarrier = [&](FIntPoint Anchor, FLinearColor Color)
        {
            const auto Ends = FBoardLayout::BarrierCross(Anchor);
            FSlateDrawElement::MakeLines(Out, Layer+4, G.ToPaintGeometry(), TArray<FVector2D>{Ends[0],Ends[1]}, ESlateDrawEffect::None, Color, true, 3);
            FSlateDrawElement::MakeLines(Out, Layer+4, G.ToPaintGeometry(), TArray<FVector2D>{Ends[2],Ends[3]}, ESlateDrawEffect::None, Color, true, 3);
        };
        for (FIntPoint Anchor : Board.Barriers) { DrawBarrier(Anchor, FLinearColor(0,.8f,1)); }
        const auto Card=Data.SelectedTarget;
        const auto Target=Hover.IsSet() ? FBoardLayout::TargetAt(Hover.GetValue(),Card) : TOptional<FIntPoint>{};
        if (Target.IsSet() && BoardEffectTarget(Card)!=ECardTarget::None)
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
        else if (Target.IsSet() && Card==ECardId::Invalid && CanPreviewPlacement.IsBound() && CanPreviewPlacement.Execute(Target.GetValue()))
        {
            FLinearColor Preview=DisplayStoneColor(Data.PreviewStone,Data.bHideColors); Preview.A*=.35f;
            FSlateDrawElement::MakeBox(Out,Layer+5,G.ToPaintGeometry(FVector2D(24),FSlateLayoutTransform(FBoardLayout::Center(Target.GetValue())-FVector2D(12))),
                &StoneBrush,ESlateDrawEffect::None,Preview);
        }
        if (const auto* Pose=Data.TetrisPose.IsSet() ? &Data.TetrisPose.GetValue() : nullptr)
        {
            for (const auto Offset : TetrisOffsets(Pose->Shape,Pose->Rotation))
            {
                const FVector2D Center=FBoardLayout::Center(Pose->Origin+Offset);
                FSlateDrawElement::MakeBox(Out,Layer+6,G.ToPaintGeometry(FVector2D(28),FSlateLayoutTransform(Center-FVector2D(14))),White,ESlateDrawEffect::None,FLinearColor(1,.65f,.05f));
                FSlateDrawElement::MakeBox(Out,Layer+7,G.ToPaintGeometry(FVector2D(22),FSlateLayoutTransform(Center-FVector2D(11))),White,ESlateDrawEffect::None,DisplayStoneColor(Pose->Stone));
            }
        }
        return Layer+7;
    }

FReply SMatchBoard::OnMouseMove(const FGeometry& G, const FPointerEvent& E)
{ Hover=G.AbsoluteToLocal(E.GetScreenSpacePosition()); Invalidate(EInvalidateWidgetReason::Paint); return FReply::Handled(); }
void SMatchBoard::OnMouseLeave(const FPointerEvent& E)
{ Hover.Reset(); Invalidate(EInvalidateWidgetReason::Paint); SLeafWidget::OnMouseLeave(E); }
FReply SMatchBoard::OnMouseButtonDown(const FGeometry& G, const FPointerEvent& E)
{
    if (E.GetEffectingButton()==EKeys::RightMouseButton) { OnCancel.ExecuteIfBound(); }
    else if (E.GetEffectingButton()==EKeys::LeftMouseButton)
    {
        const auto Coordinate=FBoardLayout::TargetAt(G.AbsoluteToLocal(E.GetScreenSpacePosition()),Display.Get().SelectedTarget);
        if (Coordinate.IsSet()) { OnBoardClicked.ExecuteIfBound(Coordinate.GetValue()); }
    }
    else { return FReply::Unhandled(); }
    auto Reply=FReply::Handled(); if (const auto Focus=FocusTarget.Pin()) { Reply.SetUserFocus(Focus.ToSharedRef()); } return Reply;
}
