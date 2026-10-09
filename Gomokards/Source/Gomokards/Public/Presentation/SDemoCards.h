#pragma once
#include "Widgets/SLeafWidget.h"
#include "Presentation/DemoPresentation.h"
#include "UObject/StrongObjectPtr.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"

struct GOMOKARDS_API FDemoCardArt
{
    FDemoCardArt();
    const FSlateBrush* Brush(uint8 Card) const;
    TArray<TStrongObjectPtr<UTexture2D>> Textures;
    TMap<uint8,FSlateBrush> Brushes;
};
class GOMOKARDS_API SDemoCard : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SDemoCard) {} SLATE_ARGUMENT(TSharedPtr<FDemoCardArt>, Art)
        SLATE_ATTRIBUTE(Gomokards::FDemoCardPresentation, Card) SLATE_END_ARGS()
    void Construct(const FArguments& Args) { Art=Args._Art; Card=Args._Card; }
    virtual FVector2D ComputeDesiredSize(float) const override { return {192,238}; }
    virtual int32 OnPaint(const FPaintArgs&,const FGeometry&,const FSlateRect&,FSlateWindowElementList&,int32,const FWidgetStyle&,bool) const override;
private:
    TSharedPtr<FDemoCardArt> Art;
    TAttribute<Gomokards::FDemoCardPresentation> Card;
};
DECLARE_DELEGATE_OneParam(FOnHandCardClicked, int32);
class GOMOKARDS_API SDemoHand : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SDemoHand) {} SLATE_ARGUMENT(TSharedPtr<FDemoCardArt>, Art)
        SLATE_ATTRIBUTE(TArray<Gomokards::FDemoCardPresentation>, Cards)
        SLATE_EVENT(FOnHandCardClicked, OnCardClicked) SLATE_EVENT(FSimpleDelegate, OnCancel)
        SLATE_ARGUMENT(TWeakPtr<SWidget>, FocusTarget) SLATE_END_ARGS()
    void Construct(const FArguments& Args);
    virtual FVector2D ComputeDesiredSize(float) const override { return {800,244}; }
    virtual int32 OnPaint(const FPaintArgs&,const FGeometry&,const FSlateRect&,FSlateWindowElementList&,int32,const FWidgetStyle&,bool) const override;
    virtual FReply OnMouseMove(const FGeometry&,const FPointerEvent&) override;
    virtual void OnMouseLeave(const FPointerEvent&) override;
    virtual FReply OnMouseButtonDown(const FGeometry&,const FPointerEvent&) override;
    void ResetHover() { Hovered=INDEX_NONE; Invalidate(EInvalidateWidgetReason::Paint); }
private:
    int32 Count() const { return Cards.Get().Num(); }
    FVector2D Position(int32 Index,float Width,bool Raised) const;
    int32 Hit(FVector2D Point,float Width) const;
    TSharedPtr<FDemoCardArt> Art;
    TAttribute<TArray<Gomokards::FDemoCardPresentation>> Cards;
    FOnHandCardClicked OnCardClicked;
    FSimpleDelegate OnCancel;
    TWeakPtr<SWidget> FocusTarget;
    int32 Hovered=INDEX_NONE;
};
