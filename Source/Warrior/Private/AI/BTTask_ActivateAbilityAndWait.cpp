#include "AI/BTTask_ActivateAbilityAndWait.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "AIController.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystem/WarriorAbilitySystemComponent.h"


UBTTask_ActivateAbilityAndWait::UBTTask_ActivateAbilityAndWait()
{
	NodeName = "Activate Ability By Tag and Wait";
	bNotifyTaskFinished = true;
	bCreateNodeInstance = true;
	bIsWaitingForAbility = false;
}

EBTNodeResult::Type UBTTask_ActivateAbilityAndWait::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	// 이전 상태 정리
	CleanupDelegates();

	CachedOwnerComp = &OwnerComp;
	bIsWaitingForAbility = false;
	TrackedAbilityHandle = FGameplayAbilitySpecHandle();

	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		return EBTNodeResult::Failed;
	}

	APawn* ControlledPawn = AIController->GetPawn();
	if (!ControlledPawn)
	{
		return EBTNodeResult::Failed;
	}

	// AbilitySystemInterface를 통해 ASC 가져오기
	IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(ControlledPawn);
	if (!AbilitySystemInterface)
	{
		return EBTNodeResult::Failed;
	}

	CachedAbilitySystemComponent = Cast<UWarriorAbilitySystemComponent>(AbilitySystemInterface->GetAbilitySystemComponent());
	if (!CachedAbilitySystemComponent.IsValid())
	{
		return EBTNodeResult::Failed;
	}

	// 어빌리티 태그가 유효한지 확인
	if (!AbilityTag.IsValid())
	{
		return EBTNodeResult::Failed;
	}

	// 어빌리티 활성화 시도
	TArray<FGameplayAbilitySpec*> FoundAbilitySpecs;
	CachedAbilitySystemComponent->GetActivatableGameplayAbilitySpecsByAllMatchingTags(AbilityTag.GetSingleTagContainer(), FoundAbilitySpecs);

	if (FoundAbilitySpecs.IsEmpty())
	{
		return EBTNodeResult::Failed;
	}

	// 랜덤하게 어빌리티 선택 (기존 로직과 동일)
	const int32           RandomAbilityIndex = FMath::RandRange(0, FoundAbilitySpecs.Num() - 1);
	FGameplayAbilitySpec* SpecToActivate = FoundAbilitySpecs[RandomAbilityIndex];

	if (!SpecToActivate || SpecToActivate->IsActive())
	{
		return EBTNodeResult::Failed;
	}

	// 어빌리티 활성화
	bool bActivationSuccess = CachedAbilitySystemComponent->TryActivateAbility(SpecToActivate->Handle);
	if (!bActivationSuccess)
	{
		return EBTNodeResult::Failed;
	}

	TrackedAbilityHandle = SpecToActivate->Handle;

	// 즉시 완료하고 싶다면 Succeed 반환
	if (!bWaitForAbilityEnd)
	{
		return EBTNodeResult::Succeeded;
	}

	// 어빌리티 종료 델리게이트 바인딩
	AbilityEndedHandle = CachedAbilitySystemComponent->OnAbilityEnded.AddUObject(this, &UBTTask_ActivateAbilityAndWait::OnAbilityEnded);
	bIsWaitingForAbility = true;

	// InProgress 상태로 대기
	return EBTNodeResult::InProgress;
}

EBTNodeResult::Type UBTTask_ActivateAbilityAndWait::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	// CleanupDelegates();
	return EBTNodeResult::Aborted;
}

void UBTTask_ActivateAbilityAndWait::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
	CleanupDelegates();
	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
}

FString UBTTask_ActivateAbilityAndWait::GetStaticDescription() const
{
	const FString AbilityTagName = AbilityTag.IsValid() ? AbilityTag.ToString() : TEXT("None");
	const FString WaitText = bWaitForAbilityEnd ? TEXT("and wait for completion") : TEXT("without waiting");
   
	return FString::Printf(TEXT("Activate ability with tag %s %s"), *AbilityTagName, *WaitText);
}

void UBTTask_ActivateAbilityAndWait::OnAbilityEnded(const FAbilityEndedData& AbilityEndedData)
{
	// 현재 추적 중인 어빌리티가 종료되었는지 확인
	if (bIsWaitingForAbility && TrackedAbilityHandle == AbilityEndedData.AbilitySpecHandle)
	{
		if (CachedOwnerComp.IsValid())
		{
			// 어빌리티가 완료되었으므로 태스크를 성공으로 완료
			FinishLatentTask(*CachedOwnerComp.Get(), EBTNodeResult::Succeeded);
		}

		// CleanupDelegates();
	}
}

void UBTTask_ActivateAbilityAndWait::CleanupDelegates()
{
	if (AbilityEndedHandle.IsValid())
	{
		if (CachedAbilitySystemComponent.IsValid())
		{
			CachedAbilitySystemComponent->OnAbilityEnded.Remove(AbilityEndedHandle);
		}
	}

	AbilityEndedHandle.Reset();
	bIsWaitingForAbility = false;
	TrackedAbilityHandle = FGameplayAbilitySpecHandle();
	CachedOwnerComp.Reset();
	CachedAbilitySystemComponent.Reset();
}