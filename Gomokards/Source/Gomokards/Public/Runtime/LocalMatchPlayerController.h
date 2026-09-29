#pragma once

#include "GameFramework/PlayerController.h"
#include "LocalMatchPlayerController.generated.h"

class SLocalMatchView;
UCLASS()
class GOMOKARDS_API ALocalMatchPlayerController : public APlayerController
{
    GENERATED_BODY()
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    TSharedPtr<SLocalMatchView> MatchView;
};
