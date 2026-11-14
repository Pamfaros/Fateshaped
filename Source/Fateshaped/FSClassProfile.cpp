#include "FSClassProfile.h"

#include "Algo/Find.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Character.h"
#include "GameFramework/SaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UnrealType.h"

namespace FSEssence
{
namespace
{
constexpr FClass Classes[] = {
    {EKit::Unshaped,   TEXT(""),     TEXT(""),               TEXT("Unshaped"),     100, 0, 0},
    {EKit::Barbarian,  TEXT("V"),    TEXT("V_Barbarian"),    TEXT("Barbarian"),    200, 0, 0},
    {EKit::Juggernaut, TEXT("V, V"), TEXT("VV_Juggernaut"), TEXT("Juggernaut"),   240, 0, 0},
    {EKit::Berserk,    TEXT("V, S"), TEXT("VS_Berserk"),    TEXT("Berserk"),      200, 100, 0},
    {EKit::BloodMage,  TEXT("V, M"), TEXT("VM_BloodMage"),  TEXT("Blood Mage"),   200, 0, 60},
    {EKit::Warrior,    TEXT("S"),    TEXT("S_Warrior"),      TEXT("Warrior"),      100, 100, 0},
    {EKit::Mercenary,  TEXT("S, V"), TEXT("SV_Mercenary"),  TEXT("Mercenary"),    145, 100, 0},
    {EKit::Swordsman,  TEXT("S, S"), TEXT("SS_Swordsman"),  TEXT("Swordsman"),    100, 100, 0},
    {EKit::SoulsBlade, TEXT("S, M"), TEXT("SM_SoulsBlade"), TEXT("Soul's Blade"), 100, 100, 60},
    {EKit::Wizard,     TEXT("M"),    TEXT("M_Wizard"),       TEXT("Wizard"),        80, 0, 60},
    {EKit::Paladin,    TEXT("M, V"), TEXT("MV_Paladin"),    TEXT("Paladin"),       135, 0, 60},
    {EKit::SpellWeaver,TEXT("M, S"), TEXT("MS_SpellWeaver"),TEXT("Spell weaver"),   80, 100, 60},
    {EKit::Mage,       TEXT("M, M"), TEXT("MM_Mage"),       TEXT("Mage"),          80, 0, 60},
};

const FClass* FindPath(const FString& Path)
{
    return Algo::FindByPredicate(Classes, [&Path](const FClass& Row) { return Path.Equals(Row.Path, ESearchCase::CaseSensitive); });
}

template<typename TProperty>
const TProperty* Field(const UObject* Object, const TCHAR* Name)
{
    return Object ? FindFProperty<TProperty>(Object->GetClass(), Name) : nullptr;
}

FString TextField(const UObject* Object, const TCHAR* Name)
{
    const FStrProperty* Property = Field<FStrProperty>(Object, Name);
    return Property ? Property->GetPropertyValue_InContainer(Object) : FString();
}

int32 NumberField(const UObject* Object, const TCHAR* Name)
{
    const FIntProperty* Property = Field<FIntProperty>(Object, Name);
    return Property ? Property->GetPropertyValue_InContainer(Object) : 0;
}

bool Flag(const UObject* Object, const TCHAR* Name)
{
    const FBoolProperty* Property = Field<FBoolProperty>(Object, Name);
    return !Property || Property->GetPropertyValue_InContainer(Object);
}
}

const FClass& Resolve(const FString& HistoryText, bool& bSupported)
{
    FString Path;
    TArray<FString> Choices;
    if (!HistoryText.IsEmpty())
    {
        HistoryText.ParseIntoArray(Choices, TEXT(","), false);
    }
    const FClass* Resolved = &Classes[0];
    for (const FString& Choice : Choices)
    {
        if (!Path.IsEmpty())
        {
            Path += TEXT(", ");
        }
        Path += Choice.TrimStartAndEnd();
        const FClass* Next = FindPath(Path);
        if (!Next || Next == &Classes[0])
        {
            bSupported = false;
            return *Resolved;
        }
        Resolved = Next;
    }
    bSupported = true;
    return *Resolved;
}

USaveGame* Profile(const ACharacter& Character)
{
    UGameInstance* Instance = Character.GetGameInstance();
    const FObjectProperty* Save = Field<FObjectProperty>(Instance, TEXT("Save"));
    return Save ? Cast<USaveGame>(Save->GetObjectPropertyValue_InContainer(Instance)) : nullptr;
}

FString History(const USaveGame* Save)
{
    return TextField(Save, TEXT("EssenceHistory"));
}

void UpdateCachedId(USaveGame* Save, const FClass& Class)
{
    if (const FStrProperty* Id = Field<FStrProperty>(Save, TEXT("ClassId")))
    {
        Id->SetPropertyValue_InContainer(Save, Class.Id);
    }
}

int32 PendingChoices(const USaveGame* Save)
{
    return NumberField(Save, TEXT("PendingEssenceChoices"));
}

bool CanChoose(const ACharacter& Character)
{
    UGameInstance* Instance = Character.GetGameInstance();
    if (UGameplayStatics::GetCurrentLevelName(&Character) != TEXT("L_Guild")
        || Flag(Instance, TEXT("SaveBlocked")) || Flag(Instance, TEXT("RewardPending")))
    {
        return false;
    }
    const USaveGame* Save = Profile(Character);
    if (!Save || NumberField(Save, TEXT("CharLevel")) < 1)
    {
        return false;
    }
    const FString Path = History(Save);
    bool bSupported;
    const FClass& Class = Resolve(Path, bSupported);
    return bSupported && (Path.IsEmpty() || (FCString::Strlen(Class.Path) == 1
        && NumberField(Save, TEXT("CharLevel")) >= 2 && PendingChoices(Save) > 0));
}

bool TryChoose(ACharacter& Character, TCHAR Essence, FString& Message)
{
    if (!CanChoose(Character) || (Essence != 'V' && Essence != 'S' && Essence != 'M'))
    {
        Message = TEXT("No essence choice is available.");
        return false;
    }

    UGameInstance* Instance = Character.GetGameInstance();
    USaveGame* Original = Profile(Character);
    const auto* Save = Field<FObjectProperty>(Instance, TEXT("Save"));
    const auto* HistoryField = Field<FStrProperty>(Original, TEXT("EssenceHistory"));
    const auto* ClassId = Field<FStrProperty>(Original, TEXT("ClassId"));
    const auto* Pending = Field<FIntProperty>(Original, TEXT("PendingEssenceChoices"));
    const FString Slot = TextField(Instance, TEXT("SaveSlot"));
    if (!Save || !HistoryField || !ClassId || !Pending || Slot.IsEmpty())
    {
        Message = TEXT("Profile unavailable. Essence choice preserved.");
        return false;
    }

    FString Path = History(Original);
    const bool bEvolution = !Path.IsEmpty();
    if (bEvolution)
    {
        Path += TEXT(", ");
    }
    Path.AppendChar(Essence);
    bool bSupported;
    const FClass& Class = Resolve(Path, bSupported);
    if (!bSupported)
    {
        Message = TEXT("This path is unavailable.");
        return false;
    }

    USaveGame* Candidate = DuplicateObject(Original, GetTransientPackage());
    HistoryField->SetPropertyValue_InContainer(Candidate, Class.Path);
    ClassId->SetPropertyValue_InContainer(Candidate, Class.Id);
    if (bEvolution)
    {
        Pending->SetPropertyValue_InContainer(Candidate, PendingChoices(Original) - 1);
    }
    if (!UGameplayStatics::SaveGameToSlot(Candidate, Slot, 0))
    {
        Message = TEXT("Could not save the essence choice. Try again.");
        return false;
    }
    Save->SetObjectPropertyValue_InContainer(Instance, Candidate);
    Message = FString::Printf(TEXT("%s unlocked"), Class.Name);
    return true;
}
}
