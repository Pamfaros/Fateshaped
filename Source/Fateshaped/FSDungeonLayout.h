#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FSDungeonLayout.generated.h"

class UInstancedStaticMeshComponent;
class UDamageType;
class UMaterialInterface;
class UPointLightComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;

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

    UFUNCTION(BlueprintCallable, Category="Fateshaped|Dungeon")
    static void InteractWithChest(AActor* Player);

    UFUNCTION(BlueprintPure, Category="Fateshaped|Dungeon")
    static bool HasLivingCryptEnemies(const AActor* Player);

private:
    virtual void Tick(float DeltaSeconds) override;
    bool Build(int32 RunId);
    void AddTrap(const FVector& Center, bool bSpikes, bool bAlongX);
    void AddChest(const FVector& Center);
    void UpdateChest(const AActor& Player);
    void TryOpenChest(AActor& Player);
    void TryGrantKillExperience(AActor& Player);

    UFUNCTION()
    void OnEncounterDamage(AActor* Enemy, float Damage, const UDamageType* DamageType, AController* EventInstigator, AActor* DamageCauser);

    UTextRenderComponent* ChestLabel = nullptr;
    UStaticMeshComponent* ChestLid = nullptr;
    FVector ChestCenter = FVector::ZeroVector;
    bool bChestOpened = false;
    int32 PendingKillExperience = 0;
    double NextRewardRetry = 0.0;

    struct FTrap
    {
        enum class EPhase : uint8 { Idle, Warning, Active, Recovering };

        FVector Center;
        FVector HalfExtent;
        TArray<UStaticMeshComponent*> Hazards;
        UPointLightComponent* Light = nullptr;
        UTextRenderComponent* Label = nullptr;
        EPhase Phase = EPhase::Idle;
        double NextPhase = 0.0;
        double NextDamage = 0.0;
        bool bSpikes = false;
    };

    void SetTrapPhase(FTrap& Trap, FTrap::EPhase Phase, double Time);
    TArray<FTrap> Traps;

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

    UPROPERTY()
    TObjectPtr<UStaticMesh> ChestMesh;

    UPROPERTY()
    TObjectPtr<UStaticMesh> ChestLidMesh;

    UPROPERTY()
    TObjectPtr<UStaticMesh> PipeMesh;

    UPROPERTY()
    TObjectPtr<UStaticMesh> SpikeMesh;

    UPROPERTY()
    TObjectPtr<UStaticMesh> FumeMesh;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> TelegraphMaterial;

    FVector ExitCenter = FVector::ZeroVector;
};
