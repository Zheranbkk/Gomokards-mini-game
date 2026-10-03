#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Runtime/MatchNetTypes.h"

class ALocalMatchPlayerController;
class SDemoHand;
class SScrollBox;
struct FDemoCardArt;
class SLocalMatchView : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SLocalMatchView) {} SLATE_ARGUMENT(ALocalMatchPlayerController*, Owner) SLATE_EVENT(FSimpleDelegate, OnExitGame) SLATE_END_ARGS()
    void Construct(const FArguments& Args);
    virtual ~SLocalMatchView() override;
    virtual void Tick(const FGeometry&,double CurrentTime,float DeltaTime) override;
    virtual bool SupportsKeyboardFocus() const override { return true; }
    virtual FReply OnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
    virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
    uint8 SelectedCard() const;
    void CancelTargeting();
    FReply ExitGame();
    void Refresh();
    void BoardClick(FIntPoint Coordinate);
    bool CanPlace(FIntPoint Coordinate) const;
    uint8 PreviewStone() const;
    const FMatchPublicView& GetPublicView() const;
    const FMatchTetrisPose* GetTetrisPose() const;
private:
    TWeakObjectPtr<ALocalMatchPlayerController> Owner;
    FSimpleDelegate OnExitGame;
    bool IsSideTurn(uint8 Stone) const;
    bool bBlinkOn = true;
    double NextBlink = 0;
    TSharedPtr<FDemoCardArt> Art;
    TSharedPtr<SDemoHand> OwnHand, OpponentHand;
    TSharedPtr<SWidget> BoardView;
    TSharedPtr<SScrollBox> LogScroll;
    FString LastLogText;
    FDelegateHandle ChangedHandle;
    uint64 HandEpoch = MAX_uint64, HandRevision = MAX_uint64;
};
