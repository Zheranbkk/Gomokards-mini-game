#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Runtime/MatchNetTypes.h"

class ALocalMatchPlayerController;
class SVerticalBox;
class SLocalMatchView : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SLocalMatchView) {} SLATE_ARGUMENT(ALocalMatchPlayerController*, Owner) SLATE_END_ARGS()
    void Construct(const FArguments& Args);
    virtual ~SLocalMatchView() override;
    virtual bool SupportsKeyboardFocus() const override { return true; }
    virtual FReply OnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
    virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
    uint8 SelectedCard() const;
    void CancelTargeting();
    void Refresh();
    void BoardClick(FIntPoint Coordinate);
    bool CanPlace(FIntPoint Coordinate) const;
    uint8 PreviewStone() const;
    const FMatchPublicView& GetPublicView() const;
    const FMatchTetrisPose* GetTetrisPose() const;
private:
    TWeakObjectPtr<ALocalMatchPlayerController> Owner;
    TSharedPtr<SVerticalBox> Hands;
    TSharedPtr<SWidget> BoardView;
    FDelegateHandle ChangedHandle;
    uint64 HandEpoch = MAX_uint64, HandRevision = MAX_uint64;
};
