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
    void Refresh();
    void BoardClick(FIntPoint Coordinate);
    bool CanPlace(FIntPoint Coordinate) const;
    uint8 PreviewStone() const;
    const FMatchPublicView& GetPublicView() const;
private:
    TWeakObjectPtr<ALocalMatchPlayerController> Owner;
    TSharedPtr<SVerticalBox> Hands;
    TSharedPtr<SWidget> BoardView;
    FDelegateHandle ChangedHandle;
};
