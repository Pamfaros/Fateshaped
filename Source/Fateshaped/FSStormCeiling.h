#pragma once

#include "CoreMinimal.h"

class AActor;
class UMaterialInterface;
class UStaticMesh;

struct FStormCeiling
{
    void Cover(FVector Center, FVector Forward, FVector2D HalfSize, double WallTop);
    void AddConduit(FVector Start, FVector End, double Width);
    void RegisterCloud(AActor& Owner, UMaterialInterface& Material) const;
    void RegisterConduits(AActor& Owner, UStaticMesh& Cube, UMaterialInterface& Material) const;

private:
    TMap<FIntPoint, double> RoofCells;
    TArray<FTransform> ConduitSegments;
};
