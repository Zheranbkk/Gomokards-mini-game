#pragma once
#include "Widgets/SLeafWidget.h"
#include "Runtime/LocalMatchPlayerController.h"
#include "UObject/StrongObjectPtr.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"

// Eleven cooked textures, owned for the lifetime of the local UI, with dynamic text above them.
struct FDemoCardArt
{
    FDemoCardArt();
    const FSlateBrush* Brush(uint8 Card) const;
    TArray<TStrongObjectPtr<UTexture2D>> Textures;
    TMap<uint8,FSlateBrush> Brushes;
};
class SDemoCard : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SDemoCard) {} SLATE_ARGUMENT(TSharedPtr<FDemoCardArt>, Art) SLATE_ATTRIBUTE(uint8, Card) SLATE_END_ARGS()
    void Construct(const FArguments& Args) { Art=Args._Art; Card=Args._Card; }
    virtual FVector2D ComputeDesiredSize(float) const override { return {176,238}; }
    virtual int32 OnPaint(const FPaintArgs&,const FGeometry&,const FSlateRect&,FSlateWindowElementList&,int32,const FWidgetStyle&,bool) const override;
private:
    TSharedPtr<FDemoCardArt> Art;
    TAttribute<uint8> Card;
};
class SDemoHand : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SDemoHand) {} SLATE_ARGUMENT(TSharedPtr<FDemoCardArt>, Art)
        SLATE_ARGUMENT(ALocalMatchPlayerController*, Owner) SLATE_ARGUMENT(bool, Opponent)
        SLATE_ARGUMENT(TWeakPtr<SWidget>, FocusTarget) SLATE_END_ARGS()
    void Construct(const FArguments& Args);
    virtual FVector2D ComputeDesiredSize(float) const override { return {800,bOpponent ? 64.f : 244.f}; }
    virtual int32 OnPaint(const FPaintArgs&,const FGeometry&,const FSlateRect&,FSlateWindowElementList&,int32,const FWidgetStyle&,bool) const override;
    virtual FReply OnMouseMove(const FGeometry&,const FPointerEvent&) override;
    virtual void OnMouseLeave(const FPointerEvent&) override;
    virtual FReply OnMouseButtonDown(const FGeometry&,const FPointerEvent&) override;
    void ResetHover() { Hovered=INDEX_NONE; Invalidate(EInvalidateWidgetReason::Paint); }
private:
    int32 Count() const;
    FVector2D Position(int32 Index,float Width,bool Raised) const;
    int32 Hit(FVector2D Point,float Width) const;
    TSharedPtr<FDemoCardArt> Art;
    TWeakObjectPtr<ALocalMatchPlayerController> Owner;
    TWeakPtr<SWidget> FocusTarget;
    bool bOpponent=false;
    int32 Hovered=INDEX_NONE;
};
