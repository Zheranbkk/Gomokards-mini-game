#pragma once

#include "GameFramework/PlayerController.h"
#include "Runtime/MatchNetTypes.h"
#include "Presentation/DemoPresentation.h"
#include "LocalMatchPlayerController.generated.h"

class SLocalMatchView;
DECLARE_MULTICAST_DELEGATE(FMatchPresentationChanged);
UCLASS()
class GOMOKARDS_API ALocalMatchPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    const FMatchPublicView& GetPublicView() const { return DisplayPublic; }
    const FMatchPrivateView& GetPrivateView() const { return DisplayPrivate; }
    const FString& GetFeedback() const { return Feedback; }
    const Gomokards::FDemoGameLog& GetGameLog() const { return GameLog; }
    bool IsPresentationReady() const;
    bool CanPlace(FIntPoint Point) const;
    bool CanPlayCard(uint8 CardId) const;
    void RequestCard(uint8 CardId);
    bool CanTargetCard(uint8 CardId) const;
    uint8 GetSelectedTargetedCard() const { return SelectedTargetedCard; }
    void ToggleTargeting(uint8 CardId);
    void CancelTargeting();
    void RequestBoardClick(FIntPoint Point);
    void RequestTargetedCard(uint8 CardId, FIntPoint Target);
    bool CanDevelopmentRestart() const;
    void RequestPlace(FIntPoint Point);
    void RequestDevelopmentRestart();
    void RefreshPresentation();
    FString StatusLabel() const;
    FString GhostStatusLabel() const;
    FString TetrisStatusLabel() const;
    const FMatchTetrisPose* GetDisplayTetrisPose() const;
    bool CanSendTetrisInput() const;
    void RequestTetrisInput(EMatchTetrisInput Input);
    FMatchPresentationChanged OnPresentationChanged;

    UFUNCTION(Server, Reliable) void ServerTetrisInput(uint64 Epoch, uint64 ActivationToken, int32 BlockNumber, EMatchTetrisInput Input);
    UFUNCTION(Server, Reliable) void ServerPlaceStone(uint64 Epoch, uint64 ExpectedCompletedActions, int32 X, int32 Y);
    UFUNCTION(Server, Reliable) void ServerPlayCard(uint64 Epoch, uint64 ExpectedCompletedActions, uint8 CardId);
    UFUNCTION(Server, Reliable) void ServerPlayTargetedCard(uint64 Epoch, uint64 ExpectedCompletedActions, uint8 CardId, int32 TargetX, int32 TargetY);
    UFUNCTION(Server, Reliable) void ServerDevelopmentRestart(uint64 Epoch);
    UFUNCTION(Client, Reliable) void ClientActionResult(FMatchActionAck Ack);
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY(ReplicatedUsing=OnRep_PrivateView) FMatchPrivateView PrivateView;
    UFUNCTION() void OnRep_PrivateView();
    void PublishPrivate(const FMatchPrivateView& View);
    FMatchTetrisPose LatestTetrisPose;
    FMatchPublicView DisplayPublic;
    FMatchPrivateView DisplayPrivate;
    uint8 SelectedTargetedCard = 0; // Local presentation only: no stored player identity.
    bool bCoherent = false;
    bool bPending = false;
    TOptional<FMatchActionAck> PendingAck;
    FString Feedback;
    Gomokards::FDemoGameLog GameLog;
    friend class FDemoNetworkTest;
    TSharedPtr<SLocalMatchView> MatchView;
    friend class ALocalMatchGameMode;
    friend class FMatchNetworkTest;
    friend class FBasicCardNetworkTest;
    friend class FTargetedCardNetworkTest;
    friend class FPersistentCardNetworkTest;
    friend class FGhostNetworkTest;
    friend class FTetrisNetworkTest;
};
