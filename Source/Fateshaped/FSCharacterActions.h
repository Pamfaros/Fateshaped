#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FSCharacterActions.generated.h"

class ACharacter;
class UAnimSequenceBase;

UCLASS()
class UFSCharacterActions : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category="Fateshaped|Character")
    static FVector CameraHeading(const ACharacter* Character);

    UFUNCTION(BlueprintCallable, Category="Fateshaped|Character")
    static void BeginAttackFacing(ACharacter* Character);

    UFUNCTION(BlueprintCallable, Category="Fateshaped|Character")
    static void EndAttackFacing(ACharacter* Character);

    UFUNCTION(BlueprintCallable, Category="Fateshaped|Character")
    static void PlayActionAnimation(ACharacter* Character, UAnimSequenceBase* Animation, float Rate);

    UFUNCTION(BlueprintCallable, Category="Fateshaped|Character")
    static void BeginDodge(ACharacter* Character);

    UFUNCTION(BlueprintPure, Category="Fateshaped|Character")
    static bool CanMeleeHit(const ACharacter* Character, const AActor* Target);

    UFUNCTION(BlueprintPure, Category="Fateshaped|Character")
    static bool HasClearStrike(const AActor* Attacker, const AActor* Target);

    UFUNCTION(BlueprintCallable, Category="Fateshaped|Character")
    static bool CastArcaneWave(ACharacter* Character, TSubclassOf<AActor> EnemyClass, TSubclassOf<AActor> WaveEffect,
        TSubclassOf<AActor> HitEffect, float Damage);
};
