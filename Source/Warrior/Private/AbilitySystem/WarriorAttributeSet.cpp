#include "AbilitySystem/WarriorAttributeSet.h"

#include "GameplayEffectExtension.h"
#include "WarriorDebugHelper.h"

UWarriorAttributeSet::UWarriorAttributeSet()
{
	InitCurrentHealth(1.f);
	InitMaxHealth(1.f);
	InitCurrentRage(1.f);
	InitMaxRage(1.f);
	InitAttackPower(1.f);
	InitDefensePower(1.f);

}

// TODO: PostGameplayEffectExecute 간단하게 리팩토링
/*
	UWarriorAttributeSet 핵심 구현 방법론 요약

	1) 델리게이트 맵 초기화
	   - 각 Attribute(FGameplayAttribute)를 키로,
		 해당 Attribute 변경 후 처리 로직을 수행할 델리게이트를 값으로 매핑한다.

	2) PostGameplayEffectExecute에서 델리게이트 호출
	   - GAS가 속성을 변경한 직후 전달되는 FGameplayEffectModCallbackData::EvaluatedData.Attribute을 조회하여
	   - 1)에서 만든 맵에서 해당 Attribute에 바인딩된 델리게이트를 찾아
	   - Broadcast()를 호출해 후처리 로직을 실행한다.

	3) 델리게이트 핸들러(후처리 로직) 구현
	   - Clamp, CurrentHealth 갱신, UI 알림, 사망 처리 등
	   - Attribute별로 분리된 함수에서 필요한 비즈니스 로직만 작성한다.

	→ 이 세 단계만 기억하면, if/else 체인 없이
	   Attribute 수가 늘어나더라도
	   델리게이트 맵에 등록만으로 유연하게 확장·관리할 수 있습니다.
*/

void UWarriorAttributeSet::PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data)
{
	if (Data.EvaluatedData.Attribute == GetCurrentHealthAttribute())
	{
		const float NewCurrentHealth = FMath::Clamp(GetCurrentHealth(), 0.f, GetMaxHealth());

		SetCurrentHealth(NewCurrentHealth);
	}

	if (Data.EvaluatedData.Attribute == GetCurrentRageAttribute())
	{
		const float NewCurrentRage = FMath::Clamp(GetCurrentRage(), 0.f, GetMaxRage());

		SetCurrentRage(NewCurrentRage);
	}

	if (Data.EvaluatedData.Attribute == GetDamageTakenAttribute())
	{
		const float OldHealth = GetCurrentHealth();
		const float DamageDone = GetDamageTaken();

		const float NewCurrentHealth = FMath::Clamp(OldHealth - DamageDone, 0.f, GetMaxHealth());

		SetCurrentHealth(NewCurrentHealth);

		const FString DebugString = FString::Printf(TEXT("Old Health: %f, Damage Done: %f, New Health: %f"), OldHealth, DamageDone, NewCurrentHealth);

		Debug::Print(DebugString, FColor::Green);

		// TODO: Notify the UI

		// TODO: Handle character death
		if (NewCurrentHealth == 0.f) {}
	}
}
