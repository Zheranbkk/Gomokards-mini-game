#pragma once
#include "CoreMinimal.h"
#include "Cards/CardDefinitions.h"
#include "Runtime/MatchNetTypes.h"
#include "Fonts/SlateFontInfo.h"

namespace Gomokards
{
struct FDemoCardPresentation
{
    ECardId Id;
    const TCHAR* Name;
    const TCHAR* Description;
    const TCHAR* ArtPath;
};
GOMOKARDS_API TConstArrayView<FDemoCardPresentation> DemoCards();
GOMOKARDS_API const FDemoCardPresentation* DemoCard(uint8 Id);
GOMOKARDS_API const TCHAR* DemoCardBackPath();
GOMOKARDS_API FSlateFontInfo DemoFont(int32 Size);
GOMOKARDS_API FString DemoSide(uint8 Stone);
GOMOKARDS_API uint8 DemoPlayerStone(const FMatchPublicView& View, int32 PlayerId);
GOMOKARDS_API int32 DemoOpponentCount(const FMatchPublicView& View, int32 LocalPlayerId);
GOMOKARDS_API uint8 DemoDisplayCard(const FMatchPublicView& View, uint8 LocalSelection);
GOMOKARDS_API FString DemoResult(const FMatchPublicView& View);

// Bounded, local text history. Keeps only public counts/phase markers and this owner's
// previous hand; never stores a board, another owner's hand or authoritative state.
class GOMOKARDS_API FDemoGameLog
{
public:
    void Update(const FMatchPublicView& Public, const FMatchPrivateView& Private);
    const TArray<FString>& GetLines() const { return Lines; }
    FString Text() const { return FString::Join(Lines,TEXT("\n\n")); }
private:
    void Add(FString Line);
    bool bInitialized = false;
    uint64 Epoch = 0, Revision = 0, Actions = 0, LastCardAction = 0;
    int32 OwnerId = INDEX_NONE, PreviousPlayer = INDEX_NONE;
    uint8 Result = 0;
    EMatchGhostPhase Ghost = EMatchGhostPhase::None;
    bool bTetris = false;
    TArray<FMatchSeatView> Seats;
    TArray<uint8> Hand;
    TArray<FString> Lines;
};
}
