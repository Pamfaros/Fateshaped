#pragma once

#include "BehaviorTree/BTTaskNode.h"
#include "FSCryptEnemy.h"
#include "FSCryptBehavior.generated.h"

UCLASS()
class UBTTask_FSCryptBehavior : public UBTTaskNode
{
    GENERATED_BODY()

public:
    void Configure(EFSCryptBranch InBranch, const TCHAR* Name);
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

private:
    UPROPERTY()
    EFSCryptBranch Branch = EFSCryptBranch::Hold;
};

UBehaviorTree* CreateCryptBehaviorTree(UObject& Owner);
