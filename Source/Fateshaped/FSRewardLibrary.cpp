#include "FSRewardLibrary.h"

#include "Engine/GameInstance.h"
#include "GameFramework/SaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UnrealType.h"

bool UFSRewardLibrary::TryGrantExpeditionReward(AActor* Player, const FFSReward& Reward)
{
    if (!IsValid(Player) || Reward.Experience < 0 || Reward.Item.Contains(TEXT(";"))
        || (Reward.Experience == 0 && Reward.Item.IsEmpty())
        || Player != UGameplayStatics::GetPlayerPawn(Player, 0)
        || UGameplayStatics::GetCurrentLevelName(Player) != TEXT("L_Dungeon_01"))
    {
        return false;
    }

    UGameInstance* Instance = Player->GetGameInstance();
    if (!Instance)
    {
        return false;
    }

    // The GameInstance and profile are Blueprint classes. Use their existing slot and fields.
    const UClass* InstanceClass = Instance->GetClass();
    const auto* Save = FindFProperty<FObjectProperty>(InstanceClass, TEXT("Save"));
    const auto* Slot = FindFProperty<FStrProperty>(InstanceClass, TEXT("SaveSlot"));
    const auto* SaveBlocked = FindFProperty<FBoolProperty>(InstanceClass, TEXT("SaveBlocked"));
    const auto* RewardPending = FindFProperty<FBoolProperty>(InstanceClass, TEXT("RewardPending"));
    const auto* QuestAccepted = FindFProperty<FBoolProperty>(InstanceClass, TEXT("QuestAccepted"));
    const auto* RunId = FindFProperty<FIntProperty>(InstanceClass, TEXT("RunId"));
    if (!Save || !Slot || !SaveBlocked || !RewardPending || !QuestAccepted || !RunId
        || SaveBlocked->GetPropertyValue_InContainer(Instance) || RewardPending->GetPropertyValue_InContainer(Instance)
        || !QuestAccepted->GetPropertyValue_InContainer(Instance) || RunId->GetPropertyValue_InContainer(Instance) <= 0)
    {
        return false;
    }

    USaveGame* Profile = Cast<USaveGame>(Save->GetObjectPropertyValue_InContainer(Instance));
    if (!IsValid(Profile))
    {
        return false;
    }
    const UClass* ProfileClass = Profile->GetClass();
    const auto* XP = FindFProperty<FIntProperty>(ProfileClass, TEXT("XP"));
    const auto* Level = FindFProperty<FIntProperty>(ProfileClass, TEXT("CharLevel"));
    const auto* Choices = FindFProperty<FIntProperty>(ProfileClass, TEXT("PendingEssenceChoices"));
    const auto* Inventory = FindFProperty<FStrProperty>(ProfileClass, TEXT("Inventory"));
    if (!XP || !Level || !Choices || !Inventory)
    {
        return false;
    }

    USaveGame* Candidate = DuplicateObject(Profile, GetTransientPackage());
    const int64 Total = static_cast<int64>(XP->GetPropertyValue_InContainer(Candidate)) + Reward.Experience;
    XP->SetPropertyValue_InContainer(Candidate, static_cast<int32>(FMath::Min<int64>(Total, MAX_int32)));
    if (Reward.Experience > 0 && Total >= 100 && Level->GetPropertyValue_InContainer(Candidate) < 2)
    {
        Level->SetPropertyValue_InContainer(Candidate, 2);
        Choices->SetPropertyValue_InContainer(Candidate, Choices->GetPropertyValue_InContainer(Candidate) + 1);
    }
    if (!Reward.Item.IsEmpty())
    {
        Inventory->SetPropertyValue_InContainer(Candidate, Inventory->GetPropertyValue_InContainer(Candidate) + Reward.Item + TEXT("; "));
    }

    if (!UGameplayStatics::SaveGameToSlot(Candidate, Slot->GetPropertyValue_InContainer(Instance), 0))
    {
        return false;
    }
    Save->SetObjectPropertyValue_InContainer(Instance, Candidate);
    return true;
}
