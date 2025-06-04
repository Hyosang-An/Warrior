#include "Characters/WarriorHeroCharacter.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "EnhancedActionKeyMapping.h"
#include "WarriorDebugHelper.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputSubsystems.h"
#include "WarriorGameplayTags.h"
#include "AbilitySystem/WarriorAbilitySystemComponent.h"
#include "AbilitySystem/WarriorAttributeSet.h"
#include "Components/Input/WarriorInputComponent.h"
#include "DataAssets/Input/DataAsset_InputConfig.h"
#include "DataAssets/StartUpData/DataAsset_HeroStartUpData.h"
#include "Components/Combat/HeroCombatComponent.h"
#include "Components/UI/HeroUIComponent.h"
#include "GameModes/WarriorBaseGameMode.h"
#include "Utilities/RealTimeTimer.h"

AWarriorHeroCharacter::AWarriorHeroCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	bUseControllerRotationRoll = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 200.f;
	CameraBoom->SocketOffset = CameraBoomOffset;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 500.f, 0.f);
	GetCharacterMovement()->MaxWalkSpeed = 400.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;

	HeroCombatComponent = CreateDefaultSubobject<UHeroCombatComponent>(TEXT("HeroCombatComponent"));

	HeroUIComponent = CreateDefaultSubobject<UHeroUIComponent>(TEXT("HeroUIComponent"));
}

UPawnCombatComponent* AWarriorHeroCharacter::GetPawnCombatComponent() const
{
	return HeroCombatComponent;
}

UPawnUIComponent* AWarriorHeroCharacter::GetPawnUIComponent() const
{
	return HeroUIComponent;
}

UHeroUIComponent* AWarriorHeroCharacter::GetHeroUIComponent() const
{
	return HeroUIComponent;
}

void AWarriorHeroCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (!CharacterStartUpData.IsNull())
	{
		if (UDataAsset_StartUpDataBase* LoadedData = CharacterStartUpData.LoadSynchronous())
		{
			int32 AbilityApplyLevel = 1;

			if (AWarriorBaseGameMode* BaseGameMode = GetWorld()->GetAuthGameMode<AWarriorBaseGameMode>())
			{
				switch (BaseGameMode->GetCurrentGameDifficulty())
				{
					case EWarriorGameDifficulty::Easy:
						AbilityApplyLevel = 4;
						break;

					case EWarriorGameDifficulty::Normal:
						AbilityApplyLevel = 3;
						break;

					case EWarriorGameDifficulty::Hard:
						AbilityApplyLevel = 2;
						break;

					case EWarriorGameDifficulty::VeryHard:
						AbilityApplyLevel = 1;
						break;

					default:
						break;
				}
			}

			LoadedData->GiveToAbilitySystemComponent(WarriorAbilitySystemComponent, AbilityApplyLevel);
		}
	}
}

void AWarriorHeroCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// [언리얼 엔진 입력 바인딩 흐름 요약]
	// 1. 플레이어 컨트롤러가 이 캐릭터를 Possess 할 때 (APlayerController::Possess(MyCharacter))
	// 2. → ACharacter::PossessedBy() 함수가 호출됨
	// 3. → 그 내부에서 SetupPlayerInputComponent()가 호출됨
	// 4. → UInputComponent는 엔진이 자동 생성해 넘겨줌
	// 5. → 여기에서 입력 액션과 함수 바인딩을 설정하면 실제 플레이어 입력과 연결됨

	checkf(InputConfigDataAsset, TEXT("Forgot to assign a valid data asset as input config"));

	ULocalPlayer*                       LocalPlayer = GetController<APlayerController>()->GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();

	check(Subsystem);

	Subsystem->AddMappingContext(InputConfigDataAsset->DefaultMappingContext, 0);

	UWarriorInputComponent* WarriorInputComponent = CastChecked<UWarriorInputComponent>(PlayerInputComponent);

	WarriorInputComponent->BindNativeInputAction(InputConfigDataAsset, WarriorGameplayTags::InputTag_Move, ETriggerEvent::Triggered, this, &ThisClass::Input_Move);
	WarriorInputComponent->BindNativeInputAction(InputConfigDataAsset, WarriorGameplayTags::InputTag_Look, ETriggerEvent::Triggered, this, &ThisClass::Input_Look);

	WarriorInputComponent->BindNativeInputAction(InputConfigDataAsset, WarriorGameplayTags::InputTag_SwitchTarget, ETriggerEvent::Triggered, this, &ThisClass::Input_SwitchTargetTriggered);
	WarriorInputComponent->BindNativeInputAction(InputConfigDataAsset, WarriorGameplayTags::InputTag_SwitchTarget, ETriggerEvent::Completed, this, &ThisClass::Input_SwitchTargetCompleted);

	WarriorInputComponent->BindNativeInputAction(InputConfigDataAsset, WarriorGameplayTags::InputTag_PickUp_Stones, ETriggerEvent::Started, this, &ThisClass::Input_PickUpStonesStarted);

	WarriorInputComponent->BindAbilityInputAction(InputConfigDataAsset, this, &ThisClass::Input_AbilityInputPressed, &ThisClass::Input_AbilityInputReleased);
}

void AWarriorHeroCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void AWarriorHeroCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bShouldRestoreCameraBoomOffset || bShouldRestoreCameraRotationOffset)
		RestoreCameraOffsetTick(DeltaTime);
}

void AWarriorHeroCharacter::Input_Move(const FInputActionValue& InputActionValue)
{
	const FVector2D MovementVector = InputActionValue.Get<FVector2D>();
	const FRotator  MovementRotation = FRotator(0.f, Controller->GetControlRotation().Yaw, 0.f);

	if (MovementVector.Y != 0.f)
	{
		const FVector ForwardDirection = MovementRotation.RotateVector(FVector::ForwardVector);
		AddMovementInput(ForwardDirection, MovementVector.Y);
	}

	if (MovementVector.X != 0.f)
	{
		const FVector RightDirection = MovementRotation.RotateVector(FVector::RightVector);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void AWarriorHeroCharacter::Input_Look(const FInputActionValue& InputActionValue)
{
	const FVector2D LookVector = InputActionValue.Get<FVector2D>();

	if (LookVector.X != 0.f)
	{
		AddControllerYawInput(LookVector.X);
	}

	if (LookVector.Y != 0.f)
	{
		AddControllerPitchInput(LookVector.Y);
	}

	bShouldRestoreCameraRotationOffset = false;
}

void AWarriorHeroCharacter::Input_SwitchTargetTriggered(const FInputActionValue& InputActionValue)
{
	SwitchDirection = InputActionValue.Get<FVector2D>();

	SwitchTargetInputAccumulation += SwitchDirection;
}

void AWarriorHeroCharacter::Input_SwitchTargetCompleted(const FInputActionValue& InputActionValue)
{
	const float InputAccumulationLength = SwitchTargetInputAccumulation.Length();
	SwitchTargetInputAccumulation = FVector2D::ZeroVector;
	if (InputAccumulationLength < 10.f)
	{
		return;
	}
	//Debug::Print(FString::Printf(TEXT("Target Input Accumulation %f"), InputAccumulationLength));

	FGameplayEventData Data;

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this,
		SwitchDirection.X > 0.f ? WarriorGameplayTags::Player_Event_SwitchTarget_Right : WarriorGameplayTags::Player_Event_SwitchTarget_Left,
		Data);

	//Debug::Print(FString::Printf(TEXT("Switch Direction Input %s"), *SwitchDirection.ToString()));
}

void AWarriorHeroCharacter::Input_PickUpStonesStarted(const FInputActionValue& InputActionValue)
{
	FGameplayEventData Data;

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, WarriorGameplayTags::Player_Event_ConsumeStones, Data);
}

void AWarriorHeroCharacter::Input_AbilityInputPressed(FGameplayTag InInputTag)
{
	WarriorAbilitySystemComponent->OnAbilityInputPressed(InInputTag);
}

void AWarriorHeroCharacter::Input_AbilityInputReleased(FGameplayTag InInputTag)
{
	WarriorAbilitySystemComponent->OnAbilityInputReleased(InInputTag);
}

void AWarriorHeroCharacter::SetOrientRotationToMovement(bool bOrient) const
{
	GetCharacterMovement()->bOrientRotationToMovement = bOrient;
}

void AWarriorHeroCharacter::BeginRestoreCameraOffset(float OriginalCameraPitch)
{
	bShouldRestoreCameraBoomOffset = true;
	bShouldRestoreCameraRotationOffset = true;

	TargetCameraPitch = OriginalCameraPitch;
}

void AWarriorHeroCharacter::CancelCameraOffsetRestore()
{
	bShouldRestoreCameraBoomOffset = false;
	bShouldRestoreCameraRotationOffset = false;
}

void AWarriorHeroCharacter::RestoreCameraOffsetTick(float DeltaTime)
{
	if (bShouldRestoreCameraBoomOffset)
	{
		const FVector CurrentOffset = GetCameraBoom()->SocketOffset;
		const FVector TargetOffset = CameraBoomOffset;
		const FVector NewOffset = FMath::VInterpTo(CurrentOffset, TargetOffset, DeltaTime, 5.f);

		GetCameraBoom()->SocketOffset = NewOffset;

		// 충분히 가까워지면 종료
		if (NewOffset.Equals(TargetOffset, 0.1f))
		{
			GetCameraBoom()->SocketOffset = CameraBoomOffset;
			bShouldRestoreCameraBoomOffset = false;
		}
	}

	if (bShouldRestoreCameraRotationOffset)
	{
		const float    CurrentCameraPitch = GetController()->GetControlRotation().Pitch;
		const FRotator TargetRotator = FRotator(TargetCameraPitch, GetController()->GetControlRotation().Yaw, GetController()->GetControlRotation().Roll);
		const FRotator NewRotator = FMath::RInterpTo(GetController()->GetControlRotation(), TargetRotator, DeltaTime, 5.f);

		GetController()->SetControlRotation(NewRotator);

		// 충분히 가까워지면 종료
		if (FMath::Abs(CurrentCameraPitch - TargetCameraPitch) < 0.2f)
		{
			GetController()->SetControlRotation(NewRotator);
			bShouldRestoreCameraRotationOffset = false;
		}
	}
}