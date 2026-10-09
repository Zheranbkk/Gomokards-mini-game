#pragma once
#include "Widgets/SLeafWidget.h"
#include "Presentation/MatchPresentation.h"
namespace Gomokards
{
struct FBoardPresentation
{
    FBoard Board;
    bool bHideColors = false;
    bool bSquareStones = false;
    TOptional<FTetrisState> TetrisPose;
    ECardId SelectedTarget = ECardId::Invalid;
    EStone PreviewStone = EStone::Empty;
};
}
DECLARE_DELEGATE_OneParam(FOnBoardClicked, FIntPoint);
DECLARE_DELEGATE_RetVal_OneParam(bool, FCanPreviewPlacement, FIntPoint);
// Read-only display input and explicit intent callbacks; this widget owns only pointer hover.
class GOMOKARDS_API SMatchBoard : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SMatchBoard) {} SLATE_ATTRIBUTE(Gomokards::FBoardPresentation, Display)
        SLATE_EVENT(FOnBoardClicked, OnBoardClicked) SLATE_EVENT(FSimpleDelegate, OnCancel)
        SLATE_EVENT(FCanPreviewPlacement, CanPreviewPlacement)
        SLATE_ARGUMENT(TWeakPtr<SWidget>, FocusTarget) SLATE_END_ARGS()
    void Construct(const FArguments& Args);
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(Gomokards::FBoardLayout::Extent); }
    virtual int32 OnPaint(const FPaintArgs&,const FGeometry&,const FSlateRect&,FSlateWindowElementList&,int32,const FWidgetStyle&,bool) const override;
    virtual FReply OnMouseMove(const FGeometry&,const FPointerEvent&) override;
    virtual void OnMouseLeave(const FPointerEvent&) override;
    virtual FReply OnMouseButtonDown(const FGeometry&,const FPointerEvent&) override;
private:
    TAttribute<Gomokards::FBoardPresentation> Display;
    FOnBoardClicked OnBoardClicked;
    FSimpleDelegate OnCancel;
    FCanPreviewPlacement CanPreviewPlacement;
    TWeakPtr<SWidget> FocusTarget;
    TOptional<FVector2D> Hover;
};
