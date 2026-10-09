#pragma once
#include "Cards/CardDefinitions.h"
#include "Fonts/SlateFontInfo.h"
namespace Gomokards
{
// Display-only data supplied by a future composition; no playable catalog or ownership policy.
struct FDemoCardPresentation
{
    uint8 Id = 0;
    FString Name;
    FString Description;
    bool bEnabled = true;
};
struct FCardArtReference { ECardId Id; const TCHAR* Path; };
// Historical placeholder assets, not a list of playable SP cards.
GOMOKARDS_API TConstArrayView<FCardArtReference> CardArtReferences();
GOMOKARDS_API const TCHAR* DemoCardBackPath();
GOMOKARDS_API FSlateFontInfo DemoFont(int32 Size);
}
