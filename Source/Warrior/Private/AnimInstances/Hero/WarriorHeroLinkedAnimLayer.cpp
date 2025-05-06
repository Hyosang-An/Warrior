#include "AnimInstances/Hero/WarriorHeroLinkedAnimLayer.h"

#include "AnimInstances/Hero/WarriorHeroAnimInstance.h"

UWarriorHeroAnimInstance* UWarriorHeroLinkedAnimLayer::GetHeroAnimInstance() const
{
	// GetOwningComponent는 현재 AnimInstance가 재생중인 SkeletalMeshComponent를 반환합니다.
	return Cast<UWarriorHeroAnimInstance>(GetOwningComponent()->GetAnimInstance());
}