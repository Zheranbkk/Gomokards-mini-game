#include "Runtime/LocalMatchGameMode.h"
#include "Runtime/LocalMatchPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Guid.h"

ALocalMatchGameMode::ALocalMatchGameMode()
{
    DefaultPawnClass = nullptr;
    PlayerControllerClass = ALocalMatchPlayerController::StaticClass();
}
void ALocalMatchGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);
    if (UGameplayStatics::HasOption(Options, TEXT("Seed")))
    { StartWithSeed(UGameplayStatics::GetIntOption(Options, TEXT("Seed"), 0)); }
    else { NewMatch(); }
}
void ALocalMatchGameMode::NewMatch()
{
    const FGuid Session = FGuid::NewGuid();
    int32 Seed = static_cast<int32>(Session.A ^ Session.B ^ Session.C ^ Session.D);
    if (Seed == 0 || Seed == Match.Random.GetInitialSeed()) { Seed = Match.Random.GetInitialSeed() ^ 0x5a179b3d; }
    StartWithSeed(Seed);
}
void ALocalMatchGameMode::StartWithSeed(int32 Seed)
{
    Match.Reset(Seed);
    OnMatchChanged.Broadcast();
}
Gomokards::FActionResult ALocalMatchGameMode::Submit(const Gomokards::FActionRequest& Request)
{
    const auto Result = Gomokards::ResolveAction(Match, Request);
    if (Result.IsAccepted()) { OnMatchChanged.Broadcast(); }
    return Result;
}
