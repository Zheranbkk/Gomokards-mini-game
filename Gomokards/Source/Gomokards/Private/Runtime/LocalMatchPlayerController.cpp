#include "Runtime/LocalMatchPlayerController.h"
#include "Runtime/LocalMatchGameMode.h"
#include "Presentation/SLocalMatchView.h"
#include "Engine/GameViewportClient.h"

void ALocalMatchPlayerController::BeginPlay()
{
    Super::BeginPlay();
    auto* MatchOwner = GetWorld()->GetAuthGameMode<ALocalMatchGameMode>();
    if (IsLocalController() && MatchOwner && GetWorld()->GetGameViewport())
    {
        SAssignNew(MatchView, SLocalMatchView).Owner(MatchOwner);
        GetWorld()->GetGameViewport()->AddViewportWidgetContent(MatchView.ToSharedRef(), 10);
        bShowMouseCursor = true;
        FInputModeUIOnly Mode;
        Mode.SetWidgetToFocus(MatchView);
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        SetInputMode(Mode);
    }
}
void ALocalMatchPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
    if (MatchView.IsValid() && GetWorld() && GetWorld()->GetGameViewport())
    { GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(MatchView.ToSharedRef()); }
    MatchView.Reset();
    Super::EndPlay(Reason);
}
