

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_ExecuteTaskOnTick.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAbilityTaskTickDelegate, float, DeltaTime);

/**
 * Static factory for this AbilityTask.
 *
 * 언리얼 블루프린트 컴파일러는 다음 조건을 만족하는 static 함수만
 * “Async Task” 노드로 특수하게 다뤄줍니다:
 *  1. 반환 타입이 UAbilityTask (또는 그 하위) 이어야 함.
 *  2. UFUNCTION meta 에 BlueprintInternalUseOnly="true" 가 지정되어 있어야 함.
 *
 * 이 Async Task 노드는 클래스 내에 UPROPERTY(BlueprintAssignable) 으로 선언된
 * 모든 멀티캐스트 델리게이트를 “Delegate 출력 핀” 으로 자동 노출하며,
 * 블루프린트 컴파일 시 AddDynamic 바인딩과 ReadyForActivation() 호출을
 * 내부에서 처리해 줍니다.
 */
UCLASS()
class WARRIOR_API UAbilityTask_ExecuteTaskOnTick : public UAbilityTask
{
	GENERATED_BODY()

public:
	UAbilityTask_ExecuteTaskOnTick();
	
	UFUNCTION(BlueprintCallable, Category= "Warrior|AbilityTasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true"))
	static UAbilityTask_ExecuteTaskOnTick* ExecuteTaskOnTick(UGameplayAbility* OwningAbility);

	//~ Begin UGameplayTask Interface
	virtual void TickTask(float DeltaTime) override;
	//~ End UGameplayTask Interface

	UPROPERTY(BlueprintAssignable)
	FOnAbilityTaskTickDelegate OnAbilityTaskTick;
};
