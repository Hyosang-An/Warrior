#include "AbilitySystem/Abilities/HeroGameplayAbility_TargetLock.h"

#include "EnhancedInputSubsystems.h"
#include "WarriorDebugHelper.h"
#include "WarriorFunctionLibrary.h"
#include "WarriorGameplayTags.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Characters/WarriorHeroCharacter.h"
#include "Components/SizeBox.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Widgets/WarriorWidgetBase.h"
#include "Warrior/Public/Controllers/WarriorHeroController.h"

void UHeroGameplayAbility_TargetLock::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	TryLockOnTarget();
	InitTargetLockMovement();
	InitTargetLockMappingContext();

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UHeroGameplayAbility_TargetLock::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	ResetTargetLockMovement();
	ResetTargetLockMappingContext();
	CleanUp();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UHeroGameplayAbility_TargetLock::OnTargetLockTick(float DeltaTime)
{
	if (!CurrentLockedActor ||
		UWarriorFunctionLibrary::NativeDoesActorHaveTag(CurrentLockedActor, WarriorGameplayTags::Shared_Status_Dead) ||
		UWarriorFunctionLibrary::NativeDoesActorHaveTag(GetHeroCharacterFromActorInfo(), WarriorGameplayTags::Shared_Status_Dead))
	{
		CancelTargetLockAbility();
		return;
	}

	SetTargetLockWidgetPosition();

	const bool bShouldOverrideRotation = !UWarriorFunctionLibrary::NativeDoesActorHaveTag(GetHeroCharacterFromActorInfo(), WarriorGameplayTags::Player_Status_Rolling)
		&& !UWarriorFunctionLibrary::NativeDoesActorHaveTag(GetHeroCharacterFromActorInfo(), WarriorGameplayTags::Player_Status_Blocking);

	if (bShouldOverrideRotation)
	{
		const FRotator LookAtRot = UKismetMathLibrary::FindLookAtRotation(GetHeroCharacterFromActorInfo()->GetActorLocation(), CurrentLockedActor->GetActorLocation());

		const FRotator CurrentControlRot = GetHeroControllerFromActorInfo()->GetControlRotation();
		const FRotator NewControlRot = FMath::RInterpTo(CurrentControlRot, LookAtRot, DeltaTime, TargetLockRotationInterpSpeed);
		GetHeroControllerFromActorInfo()->SetControlRotation(FRotator(NewControlRot.Pitch, NewControlRot.Yaw, 0.f));

		GetHeroCharacterFromActorInfo()->SetOrientRotationToMovement(false);
		const FRotator CurrentActorRot = GetHeroCharacterFromActorInfo()->GetActorRotation();
		const FRotator NewActorRot = FMath::RInterpTo(CurrentActorRot, LookAtRot, DeltaTime, TargetLockRotationInterpSpeed);
		GetHeroCharacterFromActorInfo()->SetActorRotation(FRotator(0.f, NewActorRot.Yaw, 0.f));

		// Custom
		// 카메라 오프셋 부드럽게 보간
		USpringArmComponent* CameraBoom = GetHeroCharacterFromActorInfo()->GetCameraBoom();
		if (CameraBoom == nullptr)
		{
			UE_LOG(LogTemp, Warning, TEXT("CameraBoom is null"));
			return;
		}
		const FVector CurrentOffset = CameraBoom->SocketOffset;
		if (CurrentOffset.Equals(LockOnCameraBoomSocketOffset, 0.1f))
		{
			CameraBoom->SocketOffset = LockOnCameraBoomSocketOffset;
		}
		else
		{
			const FVector NewOffset = FMath::VInterpTo(CurrentOffset, LockOnCameraBoomSocketOffset, DeltaTime, 5);
			CameraBoom->SocketOffset = NewOffset;
		}
	}
	else
	{
		GetHeroCharacterFromActorInfo()->SetOrientRotationToMovement(true);
	}
}

void UHeroGameplayAbility_TargetLock::SwitchTarget(const FGameplayTag& InSwitchDirectionTag)
{
	GetAvailableActorsToLock();

	TArray<AActor*> ActorsOnLeft;
	TArray<AActor*> ActorsOnRight;
	AActor*         NewTargetActor = nullptr;
	GetAvailableActorsAroundTarget(ActorsOnLeft, ActorsOnRight);

	if (InSwitchDirectionTag == WarriorGameplayTags::Player_Event_SwitchTarget_Left)
	{
		NewTargetActor = GetNearestTargetFromAvailableActors(ActorsOnLeft);
	}
	else
	{
		NewTargetActor = GetNearestTargetFromAvailableActors(ActorsOnRight);
	}

	if (NewTargetActor)
	{
		CurrentLockedActor = NewTargetActor;
	}
}

void UHeroGameplayAbility_TargetLock::TryLockOnTarget()
{
	GetAvailableActorsToLock();

	if (AvailableActorsToLock.IsEmpty())
	{
		CancelTargetLockAbility();
		return;
	}

	CurrentLockedActor = GetNearestTargetFromAvailableActors(AvailableActorsToLock);

	if (CurrentLockedActor)
	{
		DrawTargetLockWidget();

		SetTargetLockWidgetPosition();

		// Custom
		OriginalCameraBoomSocketOffset = GetHeroCharacterFromActorInfo()->CameraBoomOffset;
		LockOnCameraBoomSocketOffset = OriginalCameraBoomSocketOffset + FVector(-100.f, 0.f, 50.f);
		GetWorld()->GetTimerManager().ClearTimer(GetHeroCharacterFromActorInfo()->CameraOffsetRestoreHandle); // 이걸 안하면 Offset 보간이 서로 충돌함
	}
	else
	{
		CancelTargetLockAbility();
	}
}

void UHeroGameplayAbility_TargetLock::GetAvailableActorsToLock()
{
	AvailableActorsToLock.Empty();

	TArray<FHitResult> BoxTraceHits;

	// BoxTraceChannel 타입 액터들을 검출. 해당 액터들이 어떤 Response를 가지는지와 무관.
	UKismetSystemLibrary::BoxTraceMultiForObjects(GetHeroCharacterFromActorInfo(), GetHeroCharacterFromActorInfo()->GetActorLocation(),
		GetHeroCharacterFromActorInfo()->GetActorLocation() + GetHeroCharacterFromActorInfo()->GetActorForwardVector() * BoxTraceDistance,
		TraceBoxSize / 2.f, GetHeroCharacterFromActorInfo()->GetActorForwardVector().ToOrientationRotator(),
		BoxTraceChannel, false, TArray<AActor*>(),
		bShowPersistentDebugShape ? EDrawDebugTrace::Persistent : EDrawDebugTrace::None,
		BoxTraceHits, true);

	for (const FHitResult& TraceHit : BoxTraceHits)
	{
		if (AActor* HitActor = TraceHit.GetActor())
		{
			if (HitActor != GetHeroCharacterFromActorInfo())
			{
				AvailableActorsToLock.AddUnique(HitActor);
			}
		}
	}
}

AActor* UHeroGameplayAbility_TargetLock::GetNearestTargetFromAvailableActors(const TArray<AActor*>& InAvailableActors)
{
	float NearestDistance = 0.f;
	return UGameplayStatics::FindNearestActor(GetHeroCharacterFromActorInfo()->GetActorLocation(), InAvailableActors, NearestDistance);
}

void UHeroGameplayAbility_TargetLock::GetAvailableActorsAroundTarget(TArray<AActor*>& OutActorsOnLeft, TArray<AActor*>& OutActorsOnRight)
{
	if (!CurrentLockedActor || AvailableActorsToLock.IsEmpty())
	{
		CancelTargetLockAbility();
		return;
	}

	const FVector PlayerLocation = GetHeroCharacterFromActorInfo()->GetActorLocation();
	const FVector PlayerToCurrentNormalized = (CurrentLockedActor->GetActorLocation() - PlayerLocation).GetSafeNormal();
	const FVector PlayerToCurrentRight = FVector::CrossProduct(FVector::UpVector, PlayerToCurrentNormalized);

	for (AActor* AvailableActor : AvailableActorsToLock)
	{
		if (!AvailableActor || AvailableActor == CurrentLockedActor)
			continue;

		const FVector PlayerToAvailableNormalized = (AvailableActor->GetActorLocation() - PlayerLocation).GetSafeNormal();

		FVector::DotProduct(PlayerToCurrentRight, PlayerToAvailableNormalized) > 0.f ? OutActorsOnRight.AddUnique(AvailableActor) : OutActorsOnLeft.AddUnique(AvailableActor);
	}
}

void UHeroGameplayAbility_TargetLock::DrawTargetLockWidget()
{

	if (!DrawnTargetLockWidget)
	{
		checkf(TargetLockWidgetClass, TEXT("Forgot to assign a valid TargetLockWidgetClass in Blueprint"));

		DrawnTargetLockWidget = CreateWidget<UWarriorWidgetBase>(GetHeroControllerFromActorInfo(), TargetLockWidgetClass);

		check(DrawnTargetLockWidget);

		DrawnTargetLockWidget->AddToViewport();
	}

}

void UHeroGameplayAbility_TargetLock::SetTargetLockWidgetPosition()
{
	if (!DrawnTargetLockWidget || !CurrentLockedActor)
	{
		CancelTargetLockAbility();
		return;
	}

	FVector2D ScreenPosition;
	UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(GetHeroControllerFromActorInfo(), CurrentLockedActor->GetActorLocation(), ScreenPosition, true);

	if (TargetLockWidgetSize == FVector2D::ZeroVector)
	{
		DrawnTargetLockWidget->WidgetTree->ForEachWidget([this](UWidget* FoundWidget)
		{
			if (USizeBox* FoundSizeBox = Cast<USizeBox>(FoundWidget))
			{
				TargetLockWidgetSize.X = FoundSizeBox->GetWidthOverride();
				TargetLockWidgetSize.Y = FoundSizeBox->GetHeightOverride();
			}
		});
	}
	ScreenPosition.X -= TargetLockWidgetSize.X / 2.f;
	DrawnTargetLockWidget->SetPositionInViewport(ScreenPosition, false);
}

void UHeroGameplayAbility_TargetLock::InitTargetLockMovement()
{
	CachedDefaultMaxWalkSpeed = GetHeroCharacterFromActorInfo()->GetCharacterMovement()->MaxWalkSpeed;

	GetHeroCharacterFromActorInfo()->GetCharacterMovement()->MaxWalkSpeed = TargetLockMaxWalkSpeed;

	//GetHeroCharacterFromActorInfo()->SetOrientRotationToMovement(false);
}

void UHeroGameplayAbility_TargetLock::InitTargetLockMappingContext()
{
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetHeroControllerFromActorInfo()->GetLocalPlayer());

	check(Subsystem);

	Subsystem->AddMappingContext(TargetLockMappingContext, 3);
}

void UHeroGameplayAbility_TargetLock::CancelTargetLockAbility()
{
	CancelAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true);
}

void UHeroGameplayAbility_TargetLock::CleanUp()
{
	AvailableActorsToLock.Empty();

	CurrentLockedActor = nullptr;

	if (DrawnTargetLockWidget)
		DrawnTargetLockWidget->RemoveFromParent();

	DrawnTargetLockWidget = nullptr;

	TargetLockWidgetSize = FVector2D::ZeroVector;

	CachedDefaultMaxWalkSpeed = 0.f;

	if (AWarriorHeroCharacter* Hero = GetHeroCharacterFromActorInfo())
	{
		Hero->BeginRestoreCameraBoomOffset();
	}
}

void UHeroGameplayAbility_TargetLock::ResetTargetLockMovement()
{
	if (CachedDefaultMaxWalkSpeed > 0.f)
	{
		GetHeroCharacterFromActorInfo()->GetCharacterMovement()->MaxWalkSpeed = CachedDefaultMaxWalkSpeed;
	}

	GetHeroCharacterFromActorInfo()->SetOrientRotationToMovement(true);
}

void UHeroGameplayAbility_TargetLock::ResetTargetLockMappingContext()
{
	if (AWarriorHeroController* HeroController = GetHeroControllerFromActorInfo())
	{
		UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(HeroController->GetLocalPlayer());

		check(Subsystem);

		Subsystem->RemoveMappingContext(TargetLockMappingContext);
	}
}