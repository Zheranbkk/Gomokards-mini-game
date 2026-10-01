#pragma once

#include "GameFramework/GameStateBase.h"
#include "Runtime/MatchNetTypes.h"
#include "LocalMatchGameState.generated.h"

UCLASS()
class GOMOKARDS_API ALocalMatchGameState : public AGameStateBase
{
    GENERATED_BODY()
public:
    const FMatchPublicView& GetPublicView() const { return PublicView; }
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
protected:
    virtual void BeginPlay() override;
private:
    UPROPERTY(ReplicatedUsing=OnRep_PublicView) FMatchPublicView PublicView;
    UFUNCTION() void OnRep_PublicView();
    void Publish(const FMatchPublicView& View);
    friend class ALocalMatchGameMode;
    friend class FMatchNetworkTest;
    friend class FBasicCardNetworkTest;
    friend class FTargetedCardNetworkTest;
    friend class FPersistentCardNetworkTest;
};
