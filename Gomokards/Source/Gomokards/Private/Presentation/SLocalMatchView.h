#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Presentation/MatchPresentation.h"

class ALocalMatchGameMode;
class SVerticalBox;

class SLocalMatchView : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SLocalMatchView) {} SLATE_ARGUMENT(ALocalMatchGameMode*, Owner) SLATE_END_ARGS()
    void Construct(const FArguments& Args);
    virtual ~SLocalMatchView() override;
    virtual bool SupportsKeyboardFocus() const override { return true; }
    virtual FReply OnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
    virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
    virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
    void Refresh();
    void BoardClick(FIntPoint Coordinate);
    void Cancel();
    bool IsTargeting() const { return Selection.IsActive(); }
    Gomokards::ECardId SelectedCard() const { return Selection.Card; }
    const Gomokards::FMatchState& GetMatch() const;
private:
    void Submit(const Gomokards::FActionRequest& Request, const FString& ActionLabel);
    FReply CardClick(Gomokards::FPlayerId Player, Gomokards::ECardId Card);
    TWeakObjectPtr<ALocalMatchGameMode> Owner;
    TSharedPtr<SVerticalBox> Hands;
    TSharedPtr<SWidget> BoardView;
    Gomokards::FTargetSelection Selection;
    FString Feedback;
    FDelegateHandle ChangedHandle;
};
