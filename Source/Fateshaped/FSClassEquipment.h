#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FSClassEquipment.generated.h"

class UStaticMesh;

USTRUCT(BlueprintType)
struct FFSEquipmentVisual
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UStaticMesh> Mesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName AttachmentSocket;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector EdgeBase = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector EdgeTip = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector Grip = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector Palm = FVector::ZeroVector;
};

/** Model references/contact points; skeletal sockets own grip orientation, pivot and scale. */
UCLASS(BlueprintType)
class UFSClassEquipment : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TMap<FName, FFSEquipmentVisual> Visuals;
};
