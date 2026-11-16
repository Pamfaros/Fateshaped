#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FSDungeonLayout.generated.h"

class UDamageType;
class UMaterialInterface;
class UPointLightComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;
namespace FSDungeon { struct FPlan; }

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
    virtual void Destroyed() override;
    bool Build(int32 RunId);
    bool BuildModules(const FSDungeon::FPlan& Plan, double& TilePitch, TArray<FVector2D>& RoomExtents);
    void BuildStormCeiling();
    AActor* SpawnModule(int32 Module, const FTransform& Transform);
    void AddTrap(const FVector& Center, bool bSpikes, const FVector2D& RoomExtent, uint8 Openings, int32 Seed);
    void AddChest(const FVector& Center);
    AActor* SpawnChestVisual(TSubclassOf<AActor> Visual);
    void UpdateChest(const AActor& Player);
    void TryOpenChest(AActor& Player);
    void TryGrantKillExperience(AActor& Player);

    UFUNCTION()
    void OnEncounterDamage(AActor* Enemy, float Damage, const UDamageType* DamageType, AController* EventInstigator, AActor* DamageCauser);

    UTextRenderComponent* ChestLabel = nullptr;
    UPROPERTY(Transient)
    TObjectPtr<AActor> ChestVisual;
    FVector ChestCenter = FVector::ZeroVector;
    bool bChestOpened = false;
    int32 PendingKillExperience = 0;
    double NextRewardRetry = 0.0;

    struct FTrap
    {
        enum class EPhase : uint8 { Idle, Warning, Active, Recovering };

        struct FCell
        {
            FVector Center = FVector::ZeroVector;
            double HalfX = 0;
            double HalfY = 0;
            EPhase Phase = EPhase::Idle;
            double NextPhase = 0;
            double NextDamage = 0;
            UStaticMeshComponent* Floor = nullptr;
            UStaticMeshComponent* Hazard = nullptr;
        };

        FVector Center;
        double HazardHeight = 70;
        TArray<FCell> Cells;
        bool bSpikes = false;
    };

    void SetCellPhase(FTrap& Trap, FTrap::FCell& Cell, FTrap::EPhase Phase, double Time);
    TArray<FTrap> Traps;

    UPROPERTY()
    TArray<TSubclassOf<AActor>> ModuleClasses;

    UPROPERTY()
    TArray<TObjectPtr<AActor>> SpawnedModules;

    UPROPERTY()
    TObjectPtr<UStaticMesh> TrapPlateMesh;

    UPROPERTY()
    TSubclassOf<AActor> ClosedChestClass;

    UPROPERTY()
    TSubclassOf<AActor> OpenChestClass;

    UPROPERTY()
    TObjectPtr<UStaticMesh> SpikeMesh;

    UPROPERTY()
    TObjectPtr<UStaticMesh> FumeMesh;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> TelegraphMaterial;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> StormCloudMaterial;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UPointLightComponent>> StormFlashLights;

    FVector ExitCenter = FVector::ZeroVector;
};
