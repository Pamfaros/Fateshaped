#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameFramework/SaveGame.h"
#include "FSProfileStorageLibrary.generated.h"

// Read the primitive-only Fateshaped profile without invoking UE 5.8's
// legacy save fallback on potentially corrupt bytes.
UCLASS()
class FATESHAPED_API UFSProfileStorageLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="Fateshaped|Save")
    static USaveGame* LoadCheckedProfile(const FString& SlotName,
        TSubclassOf<USaveGame> ProfileClass, FString& FailureReason);
};
