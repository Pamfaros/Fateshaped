#include "FSCryptBehavior.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/Composites/BTComposite_Selector.h"
#include "BehaviorTree/Composites/BTComposite_Sequence.h"
#include "BehaviorTree/Tasks/BTTask_Wait.h"

void UBTTask_FSCryptBehavior::Configure(EFSCryptBranch InBranch, const TCHAR* Name)
{
    Branch = InBranch;
    NodeName = Name;
}

EBTNodeResult::Type UBTTask_FSCryptBehavior::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8*)
{
    AAIController* Controller = OwnerComp.GetAIOwner();
    AFSCryptEnemy* Enemy = Controller ? Cast<AFSCryptEnemy>(Controller->GetPawn()) : nullptr;
    return Enemy && Enemy->TryBehavior(Branch) ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
}

UBehaviorTree* CreateCryptBehaviorTree(UObject& Owner)
{
    UBehaviorTree* Tree = NewObject<UBehaviorTree>(&Owner, TEXT("CryptCombatTree"));
    auto* Sequence = NewObject<UBTComposite_Sequence>(Tree);
    auto* Selector = NewObject<UBTComposite_Selector>(Tree);
    auto* Wait = NewObject<UBTTask_Wait>(Tree);
    Wait->WaitTime = 0.10f;
    Tree->RootNode = Sequence;
    Sequence->Children.AddDefaulted_GetRef().ChildComposite = Selector;
    Sequence->Children.AddDefaulted_GetRef().ChildTask = Wait;

    const TPair<EFSCryptBranch, const TCHAR*> Priorities[] = {
        {EFSCryptBranch::ResolveAction, TEXT("Finish attack or recovery")},
        {EFSCryptBranch::ReturnHome, TEXT("Return to encounter room")},
        {EFSCryptBranch::Defend, TEXT("Guard or evade under pressure")},
        {EFSCryptBranch::Attack, TEXT("Choose a telegraphed attack")},
        {EFSCryptBranch::Position, TEXT("Approach or circle target")},
        {EFSCryptBranch::Hold, TEXT("Hold position")},
    };
    for (const auto& Priority : Priorities)
    {
        auto* Task = NewObject<UBTTask_FSCryptBehavior>(Tree);
        Task->Configure(Priority.Key, Priority.Value);
        Selector->Children.AddDefaulted_GetRef().ChildTask = Task;
    }
    return Tree;
}
