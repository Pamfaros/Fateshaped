#pragma once

#include "CoreMinimal.h"

class AActor;
class UMaterialInterface;

struct FStormCeiling
{
    void Cover(FVector Center, FVector Forward, FVector2D HalfSize, double WallTop);
    void RegisterCloud(AActor& Owner, UMaterialInterface& Material) const;

private:
    TMap<FIntPoint, double> RoofCells;
};
