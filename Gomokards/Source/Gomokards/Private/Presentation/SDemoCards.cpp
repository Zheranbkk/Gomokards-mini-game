#include "Presentation/SDemoCards.h"
#include "Presentation/DemoPresentation.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Styling/CoreStyle.h"
using namespace Gomokards;
namespace
{
const FVector2D CardSize(192,238);
const FLinearColor DemoCardInk(.09f,.11f,.13f);
void Box(FSlateWindowElementList& Out,int32 Layer,const FGeometry& G,FVector2D P,FVector2D Size,FLinearColor Color,const FSlateBrush* Brush=nullptr)
{ FSlateDrawElement::MakeBox(Out,Layer,G.ToPaintGeometry(Size,FSlateLayoutTransform(P)),Brush ? Brush : FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,Color); }
void Text(FSlateWindowElementList& Out,int32 Layer,const FGeometry& G,FVector2D P,const FString& Value,int32 Size,FLinearColor Color=DemoCardInk)
{ FSlateDrawElement::MakeText(Out,Layer,G.ToPaintGeometry(FVector2D(186,28),FSlateLayoutTransform(P)),Value,DemoFont(Size),ESlateDrawEffect::None,Color); }
void Card(FSlateWindowElementList& Out,int32 Layer,const FGeometry& G,FVector2D P,uint8 Id,const FDemoCardArt& Art,bool Muted)
{
    const auto* Info=DemoCard(Id); if (!Info) { return; }
    Box(Out,Layer,G,P+FVector2D(3,4),CardSize,FLinearColor(0,0,0,.2f));
    Box(Out,Layer+1,G,P,CardSize,Muted ? FLinearColor(.38f,.40f,.42f) : FLinearColor(.19f,.23f,.26f));
    Box(Out,Layer+2,G,P+FVector2D(2),CardSize-FVector2D(4),Muted ? FLinearColor(.80f,.80f,.77f) : FLinearColor(.98f,.97f,.91f));
    Text(Out,Layer+3,G,P+FVector2D(9,10),Info->Name,18);
    Box(Out,Layer+3,G,P+FVector2D(9,41),{174,72},FLinearColor::White,Art.Brush(Id));
    // Simple CJK-aware wrapping of this small frozen description, not a localization/layout framework.
    const auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
    const auto Font=DemoFont(12);
    FString Line; float Y=121;
    for (TCHAR C : FString(Info->Description))
    {
        const FString Candidate=Line+FString::Chr(C);
        if (!Line.IsEmpty() && Measure->Measure(Candidate,Font).X>174)
        { Text(Out,Layer+3,G,P+FVector2D(9,Y),Line,12); Y+=18; Line.Reset(); }
        Line.AppendChar(C);
    }
    if (!Line.IsEmpty()) { Text(Out,Layer+3,G,P+FVector2D(9,Y),Line,12); }
}
}
FDemoCardArt::FDemoCardArt()
{
    const auto Load=[this](uint8 Id,const TCHAR* Path)
    {
        auto* Texture=LoadObject<UTexture2D>(nullptr,Path);
        if (!ensureMsgf(Texture,TEXT("Missing cooked Demo card texture: %s"),Path)) { return; }
        Textures.Emplace(Texture);
        auto& B=Brushes.Add(Id); B.SetResourceObject(Texture); B.ImageSize=FVector2D(4);
        B.DrawAs=ESlateBrushDrawType::Image;
    };
    for (const auto& C : DemoCards()) { Load(uint8(C.Id),C.ArtPath); }
    Load(0,DemoCardBackPath());
}
const FSlateBrush* FDemoCardArt::Brush(uint8 Id) const { return Brushes.Find(Id); }
int32 SDemoCard::OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle&,bool) const
{ if (Art) { ::Card(Out,Layer,G,FVector2D::ZeroVector,Card.Get(),*Art,false); } return Layer+4; }
void SDemoHand::Construct(const FArguments& Args)
{ Art=Args._Art; Owner=Args._Owner; bOpponent=Args._Opponent; FocusTarget=Args._FocusTarget; SetClipping(EWidgetClipping::ClipToBounds); }
int32 SDemoHand::Count() const
{
    if (!Owner.IsValid()) { return 0; }
    // The opponent path has no card-ID source or hover-details path.
    return bOpponent ? DemoOpponentCount(Owner->GetPublicView(),Owner->GetPrivateView().PlayerId) : Owner->GetPrivateView().Hand.Num();
}
FVector2D SDemoHand::Position(int32 Index,float Width,bool Raised) const
{
    const int32 N=Count(); const float W=bOpponent ? 96.f : float(CardSize.X);
    const float Step=N>1 ? FMath::Min(bOpponent ? 62.f : 154.f,FMath::Max(1.f,(Width-W-12)/(N-1))) : 0.f;
    return {FMath::Max(6.f,(Width-W-Step*(N-1))*.5f)+Index*Step,bOpponent ? 3.f : Raised ? 4.f : 36.f};
}
int32 SDemoHand::Hit(FVector2D P,float Width) const
{
    if (bOpponent) { return INDEX_NONE; }
    const auto Inside=[&](int32 I,bool Raised){const auto A=Position(I,Width,Raised); return P.X>=A.X && P.X<A.X+CardSize.X && P.Y>=A.Y && P.Y<A.Y+CardSize.Y;};
    if (Hovered>=0 && Hovered<Count() && Inside(Hovered,true)) { return Hovered; }
    for (int32 I=Count()-1;I>=0;--I) { if (Inside(I,false)) { return I; } }
    return INDEX_NONE;
}
int32 SDemoHand::OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle&,bool) const
{
    if (!Owner.IsValid() || !Art) { return Layer; }
    if (Count()==0) { Text(Out,Layer,G,{G.GetLocalSize().X*.5f-50,20},bOpponent ? TEXT("对手暂无手牌") : TEXT("暂无手牌"),12,FLinearColor(.4f,.4f,.4f)); return Layer+1; }
    if (bOpponent)
    {
        for (int32 I=0;I<Count();++I)
        {
            const auto P=Position(I,G.GetLocalSize().X,false);
            Box(Out,Layer+I*2,G,P,{96,119},FLinearColor(.27f,.3f,.33f));
            Box(Out,Layer+I*2+1,G,P+FVector2D(3),{90,113},FLinearColor(.68f,.70f,.72f),Art->Brush(0));
        }
        return Layer+Count()*2;
    }
    const auto& Hand=Owner->GetPrivateView().Hand;
    const auto Draw=[&](int32 I,int32 Z,bool Raised)
    { const uint8 Id=Hand[I]; ::Card(Out,Z,G,Position(I,G.GetLocalSize().X,Raised),Id,*Art,!Owner->CanPlayCard(Id) && !Owner->CanTargetCard(Id)); };
    for (int32 I=0;I<Hand.Num();++I) { if (I!=Hovered) { Draw(I,Layer+I*5,false); } }
    if (Hand.IsValidIndex(Hovered)) { Draw(Hovered,Layer+Hand.Num()*5,true); }
    return Layer+(Hand.Num()+1)*5;
}
FReply SDemoHand::OnMouseMove(const FGeometry& G,const FPointerEvent& E)
{ const int32 Next=Hit(G.AbsoluteToLocal(E.GetScreenSpacePosition()),G.GetLocalSize().X); if (Next!=Hovered) { Hovered=Next; Invalidate(EInvalidateWidgetReason::Paint); } return FReply::Handled(); }
void SDemoHand::OnMouseLeave(const FPointerEvent& E) { ResetHover(); SLeafWidget::OnMouseLeave(E); }
FReply SDemoHand::OnMouseButtonDown(const FGeometry& G,const FPointerEvent& E)
{
    if (!Owner.IsValid()) { return FReply::Unhandled(); }
    if (E.GetEffectingButton()==EKeys::RightMouseButton) { Owner->CancelTargeting(); }
    else if (!bOpponent && E.GetEffectingButton()==EKeys::LeftMouseButton)
    {
        const int32 Index=Hit(G.AbsoluteToLocal(E.GetScreenSpacePosition()),G.GetLocalSize().X);
        if (Owner->GetPrivateView().Hand.IsValidIndex(Index))
        {
            const uint8 Id=Owner->GetPrivateView().Hand[Index];
            if (Owner->CanTargetCard(Id)) { Owner->ToggleTargeting(Id); }
            else if (Owner->CanPlayCard(Id)) { Owner->RequestCard(Id); }
        }
    }
    auto Reply=FReply::Handled(); if (const auto Focus=FocusTarget.Pin()) { Reply.SetUserFocus(Focus.ToSharedRef()); } return Reply;
}
