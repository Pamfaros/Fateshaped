#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FSDungeonLayout.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;

UCLASS(NotBlueprintable)
class AFSDungeonLayout : public AActor
{
    GENERATED_BODY()

public:
    AFSDungeonLayout();

    UFUNCTION(BlueprintCallable, Category="Fateshaped|Dungeon")
    static void GenerateForContract(AActor* Player, const FString& QuestId, int32 RunId);

    UFUNCTION(BlueprintPure, Category="Fateshaped|Dungeon")
    static bool HasReachedExit(const AActor* Player);

private:
    bool Build(int32 RunId);

    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> Floor;

    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> Walls;

    UPROPERTY()
    TObjectPtr<UStaticMesh> TorchMesh;

    UPROPERTY()
    TObjectPtr<UStaticMesh> CrateMesh;

    UPROPERTY()
    TObjectPtr<UStaticMesh> BarrelMesh;

    FVector ExitCenter = FVector::ZeroVector;
};
