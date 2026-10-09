#include "Presentation/DemoPresentation.h"
#include "Misc/Paths.h"
#include "Fonts/CompositeFont.h"
namespace Gomokards
{
TConstArrayView<FCardArtReference> CardArtReferences()
{
    static const FCardArtReference References[] = {
        {ECardId::Restock,TEXT("/Game/UI/Cards/T_CardArt_Restock_Placeholder.T_CardArt_Restock_Placeholder")},
        {ECardId::SwapHands,TEXT("/Game/UI/Cards/T_CardArt_SwapHands_Placeholder.T_CardArt_SwapHands_Placeholder")},
        {ECardId::Steal,TEXT("/Game/UI/Cards/T_CardArt_Steal_Placeholder.T_CardArt_Steal_Placeholder")},
        {ECardId::TacticalNuke,TEXT("/Game/UI/Cards/T_CardArt_TacticalNuke_Placeholder.T_CardArt_TacticalNuke_Placeholder")},
        {ECardId::Polarity,TEXT("/Game/UI/Cards/T_CardArt_Polarity_Placeholder.T_CardArt_Polarity_Placeholder")},
        {ECardId::Confusion,TEXT("/Game/UI/Cards/T_CardArt_Confusion_Placeholder.T_CardArt_Confusion_Placeholder")},
        {ECardId::Barrier,TEXT("/Game/UI/Cards/T_CardArt_Barrier_Placeholder.T_CardArt_Barrier_Placeholder")},
        {ECardId::BackToBasics,TEXT("/Game/UI/Cards/T_CardArt_BackToBasics_Placeholder.T_CardArt_BackToBasics_Placeholder")},
        {ECardId::Ghost,TEXT("/Game/UI/Cards/T_CardArt_Ghost_Placeholder.T_CardArt_Ghost_Placeholder")},
        {ECardId::Tetris,TEXT("/Game/UI/Cards/T_CardArt_Tetris_Placeholder.T_CardArt_Tetris_Placeholder")},
    };
    return References;
}
const TCHAR* DemoCardBackPath() { return TEXT("/Game/UI/Cards/T_CardBack_Placeholder.T_CardBack_Placeholder"); }
FSlateFontInfo DemoFont(int32 Size)
{
    static const TSharedPtr<const FCompositeFont> Font=[]
    {
        auto Composite=MakeShared<FCompositeFont>();
        Composite->DefaultTypeface.Fonts.Emplace(TEXT("Regular"),FPaths::EngineContentDir()/TEXT("Slate/Fonts/Roboto-Regular.ttf"),EFontHinting::Default,EFontLoadingPolicy::LazyLoad);
        Composite->FallbackTypeface.Typeface.Fonts.Emplace(TEXT("Regular"),FPaths::EngineContentDir()/TEXT("Slate/Fonts/DroidSansFallback.ttf"),EFontHinting::Default,EFontLoadingPolicy::LazyLoad);
        return Composite;
    }();
    return FSlateFontInfo(Font,Size,TEXT("Regular"));
}
}
