#include "Runtime/LocalMatchGameState.h"
#include "Runtime/LocalMatchPlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

void ALocalMatchGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ALocalMatchGameState, PublicView);
}
void ALocalMatchGameState::Publish(const FMatchPublicView& View)
{
    check(HasAuthority());
    PublicView=View;
    ForceNetUpdate();
    OnRep_PublicView(); // C++ assignment does not call RepNotify on the listen host.
}
void ALocalMatchGameState::BeginPlay()
{
    Super::BeginPlay();
    OnRep_PublicView();
}
void ALocalMatchGameState::OnRep_PublicView()
{
    for (auto It=GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (auto* PC=Cast<ALocalMatchPlayerController>(It->Get()); PC && PC->IsLocalController()) { PC->RefreshPresentation(); }
    }
}
