#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_ActivateAbilityAndWait.generated.h"

class UWarriorAbilitySystemComponent;
/**
 * 
 */
UCLASS()
class WARRIOR_API UBTTask_ActivateAbilityAndWait : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_ActivateAbilityAndWait();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;
	virtual FString GetStaticDescription() const override;

protected:
	UPROPERTY(EditAnywhere, Category = "Ability")
	FGameplayTag AbilityTag;

	UPROPERTY(EditAnywhere, Category = "Ability")
	bool bWaitForAbilityEnd = true;

private:
	UFUNCTION()
	void OnAbilityEnded(const FAbilityEndedData& AbilityEndedData);

	// 인스턴스 멤버 변수들
	UPROPERTY()
	TWeakObjectPtr<UBehaviorTreeComponent> CachedOwnerComp;
	TWeakObjectPtr<UWarriorAbilitySystemComponent> CachedAbilitySystemComponent;
    
	FGameplayAbilitySpecHandle TrackedAbilityHandle;
	FDelegateHandle AbilityEndedHandle;
	bool bIsWaitingForAbility = false;
    
	void CleanupDelegates();
};