#pragma once

#include "CoreMinimal.h"

class ACharacter;
class USaveGame;

namespace FSEssence
{
enum class EKit : uint8
{
    Unshaped, Barbarian, Juggernaut, Berserk, BloodMage, Warrior, Mercenary,
    Swordsman, SoulsBlade, Wizard, Paladin, SpellWeaver, Mage
};

struct FClass
{
    EKit Kit;
    const TCHAR* Path;
    const TCHAR* Id;
    const TCHAR* Name;
    float Vitality;
    float Stamina;
    float Mana;
};

const FClass& Resolve(const FString& History, bool& bSupported);
USaveGame* Profile(const ACharacter& Character);
FString History(const USaveGame* Save);
void UpdateCachedId(USaveGame* Save, const FClass& Class);
int32 PendingChoices(const USaveGame* Save);
bool CanChoose(const ACharacter& Character);
bool TryChoose(ACharacter& Character, TCHAR Essence, FString& Message);
}
