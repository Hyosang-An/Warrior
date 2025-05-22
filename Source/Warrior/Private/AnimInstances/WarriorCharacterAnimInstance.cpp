#include "AnimInstances/WarriorCharacterAnimInstance.h"

#include "KismetAnimationLibrary.h"
#include "Characters/WarriorBaseCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

void UWarriorCharacterAnimInstance::NativeInitializeAnimation()
{
	OwningCharacter = Cast<AWarriorBaseCharacter>(TryGetPawnOwner());

	if (OwningCharacter)
	{
		OwningMovementComponent = OwningCharacter->GetCharacterMovement();
	}
}

void UWarriorCharacterAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	// NativeUpdateAnimation에서 안전하게 계산한 값을
	// NativeThreadSafeUpdateAnimation에서 읽기만 하면 된다.
	
	if (!OwningCharacter || !OwningMovementComponent)
	{
		return;
	}

	GroundSpeed = OwningCharacter->GetVelocity().Size2D();

	bHasAcceleration = OwningMovementComponent->GetCurrentAcceleration().SizeSquared2D() > 0.f;

	LocomotionDirection = UKismetAnimationLibrary::CalculateDirection(OwningCharacter->GetVelocity(), OwningCharacter->GetActorRotation());

	// Blend Space 축 값 계산
	const float Degree = FMath::DegreesToRadians(LocomotionDirection);
	float XValue = GroundSpeed * FMath::Sin(Degree);
	float YValue = GroundSpeed * FMath::Cos(Degree);
	MovementVelocity = FVector2D(XValue, YValue);
}