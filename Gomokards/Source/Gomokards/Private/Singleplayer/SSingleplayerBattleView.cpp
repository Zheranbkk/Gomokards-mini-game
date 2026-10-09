#include "Singleplayer/SSingleplayerBattleView.h"
#include "Presentation/SMatchBoard.h"
#include "Presentation/SDemoCards.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"
using namespace Gomokards;

void SSingleplayerBattleView::Construct(const FArguments& Args)
{
    Owner = Args._Owner;
    const auto Art = MakeShared<FDemoCardArt>();
    const auto* White = FCoreStyle::Get().GetBrush("WhiteBrush");
    const FLinearColor Ink(.09f,.11f,.13f), Panel(.91f,.92f,.89f);
    ChildSlot
    [SNew(SBorder).Padding(0).BorderImage(White).BorderBackgroundColor(FLinearColor(.14f,.17f,.19f))
        [SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
            [SNew(SBox).WidthOverride(1920).HeightOverride(1112)
                [SNew(SBorder).Padding(12).BorderImage(White).BorderBackgroundColor(FLinearColor(.97f,.97f,.94f))
                    [SNew(SVerticalBox)
                        +SVerticalBox::Slot().AutoHeight()
                        [SNew(SBox).HeightOverride(70)
                            [SNew(SHorizontalBox)
                                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)
                                [SNew(SButton).OnClicked_Lambda([this]{ Owner->ExitGame(); return FReply::Handled(); })
                                    [SNew(STextBlock).Font(DemoFont(21)).Text(FText::FromString(TEXT("退出游戏")))]]
                                +SHorizontalBox::Slot().FillWidth(1).HAlign(HAlign_Center).VAlign(VAlign_Center)
                                [SNew(STextBlock).Font(DemoFont(24)).ColorAndOpacity(Ink).Text(FText::FromString(TEXT("GOMOKARDS · 单人对局")))]]]
                        +SVerticalBox::Slot().AutoHeight()
                        [SNew(SBox).HeightOverride(700)
                            [SNew(SHorizontalBox)
                                +SHorizontalBox::Slot().FillWidth(.23f).Padding(0,4,12,4)
                                [SNew(SBorder).Padding(18).BorderImage(White).BorderBackgroundColor(Panel)
                                    [SNew(SVerticalBox)
                                        +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,16)
                                        [SNew(STextBlock).Font(DemoFont(21)).ColorAndOpacity(Ink).Text(FText::FromString(TEXT("对局记录")))]
                                        +SVerticalBox::Slot().FillHeight(1)
                                        [SNew(STextBlock).Font(DemoFont(15)).ColorAndOpacity(Ink).AutoWrapText(true)
                                            .Text_Lambda([this]{ return FText::FromString(Owner->GetLogText()); })]]]
                                +SHorizontalBox::Slot().FillWidth(.54f)
                                [SNew(SOverlay)
                                    +SOverlay::Slot()
                                    [SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
                                        [SNew(SBox).WidthOverride(FBoardLayout::Extent).HeightOverride(FBoardLayout::Extent)
                                            [SNew(SMatchBoard).FocusTarget(SharedThis(this))
                                                .Display_Lambda([this]
                                                {
                                                    FBoardPresentation Display;
                                                    Display.Board = Owner->GetPresentation().Board;
                                                    Display.SelectedTarget = Owner->GetSelection().TargetCard;
                                                    Display.PreviewStone = EStone::Black;
                                                    return Display;
                                                })
                                                .CanPreviewPlacement_Lambda([this](FIntPoint Point)
                                                {
                                                    const auto& View = Owner->GetPresentation();
                                                    return View.CanAct() && !Owner->GetSelection().bReplacing && FBoard::Contains(Point)
                                                        && View.Board.At(Point).Stone == EStone::Empty && !View.Board.At(Point).bForbidden;
                                                })
                                                .OnBoardClicked_Lambda([this](FIntPoint Point){ Owner->ClickBoard(Point); })
                                                .OnCancel_Lambda([this]{ Owner->CancelSelection(); })]]]
                                    +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
                                    [SNew(SBorder).Padding(32,20).BorderImage(White).BorderBackgroundColor(FLinearColor(.96f,.95f,.89f,.96f))
                                        .Visibility_Lambda([this]{ return Owner->GetPresentation().Result.Status == EMatchStatus::InProgress ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
                                        [SNew(STextBlock).Font(DemoFont(28)).ColorAndOpacity(Ink)
                                            .Text_Lambda([this]{ return FText::FromString(SingleplayerResultText(Owner->GetPresentation().Result)); })]]]
                                +SHorizontalBox::Slot().FillWidth(.23f).Padding(12,4,0,4)
                                [SNew(SBorder).Padding(18).BorderImage(White).BorderBackgroundColor(Panel)
                                    [SNew(SVerticalBox)
                                        +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,18)
                                        [SNew(STextBlock).Font(DemoFont(21)).ColorAndOpacity(Ink).Text(FText::FromString(TEXT("玩家：黑方\nAI：白方")))]
                                        +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,18)
                                        [SNew(STextBlock).Font(DemoFont(21)).ColorAndOpacity(Ink).Text_Lambda([this]
                                        {
                                            const auto& View = Owner->GetPresentation();
                                            FString Text = SingleplayerResultText(View.Result);
                                            if (Text.IsEmpty()) { Text = View.CurrentSide == 0 ? TEXT("玩家回合") : TEXT("AI 回合"); }
                                            return FText::FromString(Text);
                                        })]
                                        +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,18)
                                        [SNew(STextBlock).Font(DemoFont(18)).ColorAndOpacity(Ink).Text_Lambda([this]
                                        {
                                            const auto& View = Owner->GetPresentation();
                                            return FText::FromString(FString::Printf(TEXT("手牌：%d / 5\n抽牌堆：%d\n弃牌堆：%d\n移出：%d"), View.Hand.Num(), View.DrawCount, View.DiscardCount, View.ExhaustCount));
                                        })]
                                        +SVerticalBox::Slot().AutoHeight()
                                        [SNew(SButton).IsEnabled_Lambda([this]{ return Owner->GetPresentation().CanDraw(); })
                                            .OnClicked_Lambda([this]{ Owner->ClickDraw(); return FReply::Handled().SetUserFocus(SharedThis(this)); })
                                            [SNew(STextBlock).Font(DemoFont(21)).Text_Lambda([this]{ return FText::FromString(Owner->GetPresentation().IsReplace() ? TEXT("换牌") : TEXT("抽牌")); })]]
                                        +SVerticalBox::Slot().AutoHeight().Padding(0,18,0,0)
                                        [SNew(STextBlock).Font(DemoFont(16)).ColorAndOpacity(Ink).AutoWrapText(true)
                                            .Text_Lambda([this]{ return FText::FromString(Owner->GetSelection().Instruction()); })]
                                        +SVerticalBox::Slot().FillHeight(1)[SNew(SSpacer)]
                                        +SVerticalBox::Slot().AutoHeight()
                                        [SNew(SButton).OnClicked_Lambda([this]{ Owner->RestartBattle(); return FReply::Handled().SetUserFocus(SharedThis(this)); })
                                            [SNew(STextBlock).Font(DemoFont(18)).Text(FText::FromString(TEXT("重新开始")))]]]]]]
                        +SVerticalBox::Slot().AutoHeight()
                        [SNew(SBox).HeightOverride(276)
                            [SAssignNew(Hand, SDemoHand).Art(Art).FocusTarget(SharedThis(this))
                                .Cards_Lambda([this]
                                {
                                    TArray<FDemoCardPresentation> Cards;
                                    const auto& View = Owner->GetPresentation();
                                    for (const auto& Card : View.Hand)
                                    {
                                        auto Display = SingleplayerCardPresentation(Card.Id);
                                        Display.bEnabled = View.CanAct();
                                        Cards.Add(MoveTemp(Display));
                                    }
                                    return Cards;
                                })
                                .OnCardClicked_Lambda([this](int32 Index){ Owner->ClickHand(Index); })
                                .OnCancel_Lambda([this]{ Owner->CancelSelection(); })]]
                        +SVerticalBox::Slot().FillHeight(1)
                        [SNew(STextBlock).Font(DemoFont(14)).ColorAndOpacity(Ink).Justification(ETextJustify::Center)
                            .Text_Lambda([this]{ return FText::FromString(Owner->GetFeedback()); })]
                    ]
                ]
            ]
        ]
    ];
}
FReply SSingleplayerBattleView::OnPreviewKeyDown(const FGeometry&, const FKeyEvent& Event)
{
    if (Owner.IsValid() && IsSingleplayerExitKey(Event.GetKey())) { Owner->ExitGame(); return FReply::Handled(); }
    return FReply::Unhandled();
}
FReply SSingleplayerBattleView::OnMouseButtonDown(const FGeometry&, const FPointerEvent& Event)
{
    if (Owner.IsValid() && Event.GetEffectingButton() == EKeys::RightMouseButton)
    { Owner->CancelSelection(); return FReply::Handled().SetUserFocus(SharedThis(this)); }
    return FReply::Unhandled();
}
void SSingleplayerBattleView::ResetHover() { if (Hand) { Hand->ResetHover(); } }
