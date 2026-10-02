#pragma once

#include "CoreMinimal.h"
#include "MatchNetTypes.generated.h"

UENUM()
enum class EMatchSession : uint8 { WaitingForPlayers, Playing, SessionEnded };
UENUM()
enum class EMatchIntentError : uint8 { None, Unassigned, NotPlaying, StaleEpoch, StaleAction, Unauthorized, RuleRejected, CardNotNetworkEnabled };

// Display encoding is independent of Core EStone: hidden occupancy never carries true color.
enum class EMatchDisplayStone : uint8 { Empty, Black, White, HiddenOccupied };
UENUM()
enum class EMatchGhostPhase : uint8 { None, Preparation, Hidden };
UENUM()
enum class EMatchTetrisInput : uint8 { Up, Down, Left, Right, Rotate };

// Current public piece only; never Core FTetrisState, RNG, future spawn or timer data.
USTRUCT()
struct GOMOKARDS_API FMatchTetrisPose
{
    GENERATED_BODY()
    UPROPERTY() bool bActive = false;
    UPROPERTY() uint64 Epoch = 0;
    UPROPERTY() uint64 BoardRevision = 0;
    UPROPERTY() uint64 ActivationToken = 0;
    UPROPERTY() uint64 PoseSequence = 0;
    UPROPERTY() int32 BlockNumber = 0;
    UPROPERTY() int32 OperatorPlayerId = INDEX_NONE;
    UPROPERTY() uint8 Shape = 0;
    UPROPERTY() uint8 Rotation = 0;
    UPROPERTY() FIntPoint Origin = FIntPoint::ZeroValue;
    UPROPERTY() uint8 SpawnEdge = 0;
    UPROPERTY() uint8 Stone = 0;
};

USTRUCT()
struct GOMOKARDS_API FMatchDisplayCell
{
    GENERATED_BODY()
    UPROPERTY() uint8 Stone = 0;
    UPROPERTY() bool bForbidden = false;
};
USTRUCT()
struct GOMOKARDS_API FMatchSeatView
{
    GENERATED_BODY()
    UPROPERTY() int32 PlayerId = INDEX_NONE;
    UPROPERTY() uint8 Stone = 0;
    UPROPERTY() bool bOccupied = false;
    UPROPERTY() int32 HandCount = 0;
};
USTRUCT()
struct GOMOKARDS_API FMatchPublicView
{
    GENERATED_BODY()
    UPROPERTY() TArray<FMatchDisplayCell> Cells;
    UPROPERTY() TArray<FIntPoint> Barriers;
    UPROPERTY() TArray<FMatchSeatView> Seats;
    UPROPERTY() int32 CurrentPlayerId = INDEX_NONE;
    UPROPERTY() uint64 CompletedActions = 0;
    // Only already accepted plays, never acquired or held card identities.
    UPROPERTY() uint8 LastPlayedCard = 0;
    UPROPERTY() int32 LastPlayedCardActor = INDEX_NONE;
    UPROPERTY() uint64 LastPlayedCardAction = 0;
    UPROPERTY() uint8 Result = 0;
    UPROPERTY() uint8 WinningStone = 0;
    UPROPERTY() uint8 DecisionReason = 0;
    UPROPERTY() bool bCardsDisabled = false;
    UPROPERTY() bool bTetrisActive = false;
    UPROPERTY() int32 ConfusionRemaining = 0;
    UPROPERTY() EMatchGhostPhase GhostPhase = EMatchGhostPhase::None;
    UPROPERTY() int32 GhostPlacementsCompleted = 0;
    UPROPERTY() double GhostDisplayEndServerTime = 0;
    UPROPERTY() EMatchSession Session = EMatchSession::WaitingForPlayers;
    UPROPERTY() uint64 Epoch = 0;
    UPROPERTY() uint64 Revision = 0;
};
USTRUCT()
struct GOMOKARDS_API FMatchPrivateView
{
    GENERATED_BODY()
    UPROPERTY() int32 PlayerId = INDEX_NONE;
    UPROPERTY() uint8 Stone = 0;
    UPROPERTY() TArray<uint8> Hand;
    UPROPERTY() bool bDevelopmentAdmin = false;
    UPROPERTY() uint64 Epoch = 0;
    UPROPERTY() uint64 Revision = 0;
};
USTRUCT()
struct GOMOKARDS_API FMatchActionAck
{
    GENERATED_BODY()
    UPROPERTY() uint64 Epoch = 0;
    UPROPERTY() uint64 Revision = 0;
    UPROPERTY() bool bAccepted = false;
    UPROPERTY() EMatchIntentError Error = EMatchIntentError::None;
    UPROPERTY() uint8 RuleError = 0;
    UPROPERTY() bool bBlockingReward = false;
};

namespace Gomokards { struct FMatchState; }
GOMOKARDS_API FMatchPublicView MakePublicView(const Gomokards::FMatchState& State,
    const TArray<int32>& OccupiedIds, EMatchSession Session, uint64 Epoch, uint64 Revision);
GOMOKARDS_API FMatchPrivateView MakePrivateView(const Gomokards::FMatchState& State,
    int32 PlayerId, bool bAdmin, uint64 Epoch, uint64 Revision);

// Migration boundary only; the authoritative draw pool and core definitions remain unchanged.
GOMOKARDS_API bool IsNetworkCardEnabled(uint8 CardId);
GOMOKARDS_API bool IsTargetedNetworkCardEnabled(uint8 CardId);
