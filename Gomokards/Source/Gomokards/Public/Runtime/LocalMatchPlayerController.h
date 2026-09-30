#pragma once

#include "GameFramework/PlayerController.h"
#include "Runtime/MatchNetTypes.h"
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
    bool IsPresentationReady() const;
    bool CanPlace(FIntPoint Point) const;
    bool CanPlayCard(uint8 CardId) const;
    void RequestCard(uint8 CardId);
    bool CanDevelopmentRestart() const;
    void RequestPlace(FIntPoint Point);
    void RequestDevelopmentRestart();
    void RefreshPresentation();
    FString StatusLabel() const;
    FMatchPresentationChanged OnPresentationChanged;

    UFUNCTION(Server, Reliable) void ServerPlaceStone(uint64 Epoch, uint64 ExpectedCompletedActions, int32 X, int32 Y);
    UFUNCTION(Server, Reliable) void ServerPlayCard(uint64 Epoch, uint64 ExpectedCompletedActions, uint8 CardId);
    UFUNCTION(Server, Reliable) void ServerDevelopmentRestart(uint64 Epoch);
    UFUNCTION(Client, Reliable) void ClientActionResult(FMatchActionAck Ack);
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY(ReplicatedUsing=OnRep_PrivateView) FMatchPrivateView PrivateView;
    UFUNCTION() void OnRep_PrivateView();
    void PublishPrivate(const FMatchPrivateView& View);
    FMatchPublicView DisplayPublic;
    FMatchPrivateView DisplayPrivate;
    bool bCoherent = false;
    bool bPending = false;
    TOptional<FMatchActionAck> PendingAck;
    FString Feedback;
    TSharedPtr<SLocalMatchView> MatchView;
    friend class ALocalMatchGameMode;
    friend class FMatchNetworkTest;
    friend class FBasicCardNetworkTest;
};
