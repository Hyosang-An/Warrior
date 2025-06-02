#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RealTimeTimer.generated.h"

DECLARE_DYNAMIC_DELEGATE(FRealTimeTimerDynamicDelegate);

DECLARE_DELEGATE(FRealTimeTimerDelegate);

/**
 * 전역으로 사용 가능한 리얼타임 타이머 시스템
 * Time Dilation의 영향을 받지 않는 실제 시간 기반 타이머
 */
UCLASS()
class WARRIOR_API URealTimeTimerManager : public UObject
{
	GENERATED_BODY()

public:
	URealTimeTimerManager();

	/**
	 * 리얼타임 타이머 시작
	 * @param InDuration - 지속 시간 (초)
	 * @param InCallback - 타이머 완료 시 호출할 델리게이트
	 * @return - 타이머 핸들 (타이머 취소 시 사용)
	 */
	FString StartRealTimeTimer(float InDuration, FRealTimeTimerDynamicDelegate InCallback);
	FString StartRealTimeTimer(float InDuration, FRealTimeTimerDelegate InCallback);

	/**
	 * 리얼타임 타이머 취소
	 * @param InTimerHandle - 취소할 타이머의 핸들
	 */
	void CancelRealTimeTimer(const FString& InTimerHandle);

	/**
	 * 모든 타이머 업데이트 (내부적으로 호출)
	 * @param DeltaTime - 경과 시간
	 */
	void UpdateTimers();

private:
	struct FRealTimeTimerInfo
	{
		double                        StartTime;
		float                         Duration;
		FRealTimeTimerDynamicDelegate DynamicCallback;
		FRealTimeTimerDelegate        Callback;
		FString                       Handle;
		bool                          bIsActive;
	};

	TMap<FString, FRealTimeTimerInfo> ActiveTimers;

	/**
	 * 타이머 핸들 생성
	 */
	FString GenerateTimerHandle();
};

/**
 * 블루프린트에서 리얼타임 타이머를 사용하기 위한 함수 라이브러리
 */
UCLASS()
class WARRIOR_API URealTimeTimerLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 싱글턴 인스턴스 가져오기 */
	static URealTimeTimerManager* GetTimerManager();

	/**
	 * 리얼타임 타이머 시작 (블루프린트 호출 가능)
	 */
	UFUNCTION(BlueprintCallable, Category = "Real Time Timer")
	static FString StartRealTimeTimer(float Duration, const FRealTimeTimerDynamicDelegate& Callback);

	/**
	 * 리얼타임 타이머 취소 (블루프린트 호출 가능)
	 */
	UFUNCTION(BlueprintCallable, Category = "Real Time Timer")
	static void CancelRealTimeTimer(const FString& TimerHandle);

	/**
	 * 지연 시간 후 이벤트 실행 (블루프린트 호출 가능)
	 */
	UFUNCTION(BlueprintCallable, Category = "Real Time Timer", meta = (Latent, LatentInfo = "LatentInfo", Duration = "1.0"))
	static void RealTimeDelay(const UObject* WorldContextObject, float Duration, struct FLatentActionInfo LatentInfo);

private:
	/** 싱글턴 인스턴스 */
	static URealTimeTimerManager* TimerManager;
};

/**
 * 리얼타임 타이머 서브시스템
 * Engine Tick을 통해 타이머를 업데이트
 * 
 * URealTimeTimerSubsystem이 UEngineSubsystem을 상속하고 있어서,
 * Unreal Engine의 엔진 서브시스템 매니저가 실행 시점에 자동으로:
 * 1. 클래스를 찾아 NewObject로 인스턴스 생성
 * 2. Initialize(FSubsystemCollectionBase&) 호출
 * 
 * 별도의 모듈 설정이나 DefaultEngine.ini 등록 없이도
 * 엔진이 부팅될 때 서브시스템으로 자동 등록됩니다.
 */
UCLASS()
class WARRIOR_API URealTimeTimerSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	FTSTicker::FDelegateHandle TickDelegateHandle;
	bool                       Tick(float DeltaTime);
};