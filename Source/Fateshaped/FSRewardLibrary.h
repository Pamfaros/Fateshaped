#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FSRewardLibrary.generated.h"

USTRUCT(BlueprintType)
struct FFSReward
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reward", meta=(ClampMin="0"))
    int32 Experience = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reward")
    FString Item;
};

UCLASS()
class UFSRewardLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // A pickup or other reward source should consume itself only after this succeeds.
    UFUNCTION(BlueprintCallable, Category="Fateshaped|Rewards")
    static bool TryGrantExpeditionReward(AActor* Player, const FFSReward& Reward);
};
