#pragma once
#include "Widgets/SCompoundWidget.h"
#include "Singleplayer/SingleplayerBattlePlayerController.h"
class SDemoHand;
class GOMOKARDS_API SSingleplayerBattleView : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SSingleplayerBattleView) {} SLATE_ARGUMENT(TWeakObjectPtr<ASingleplayerBattlePlayerController>, Owner) SLATE_END_ARGS()
    void Construct(const FArguments& Args);
    virtual bool SupportsKeyboardFocus() const override { return true; }
    virtual FReply OnPreviewKeyDown(const FGeometry&, const FKeyEvent&) override;
    virtual FReply OnMouseButtonDown(const FGeometry&, const FPointerEvent&) override;
    void ResetHover();
private:
    TWeakObjectPtr<ASingleplayerBattlePlayerController> Owner;
    TSharedPtr<SDemoHand> Hand;
};
