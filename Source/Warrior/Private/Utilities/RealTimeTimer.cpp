#include "Utilities/RealTimeTimer.h"
#include "Engine/Engine.h"
#include "LatentActions.h"

// 싱글턴 인스턴스 초기화
URealTimeTimerManager* URealTimeTimerLibrary::TimerManager = nullptr;

/**
 * 리얼타임 타이머 매니저 구현
 */
URealTimeTimerManager::URealTimeTimerManager()
{
	// 초기화
}

FString URealTimeTimerManager::StartRealTimeTimer(float InDuration, FRealTimeTimerDelegate InCallback)
{
	// 새 타이머 정보 생성
	FRealTimeTimerInfo TimerInfo;
	TimerInfo.StartTime = FPlatformTime::Seconds();
	TimerInfo.Duration = InDuration;
	TimerInfo.Callback = InCallback;
	TimerInfo.Handle = GenerateTimerHandle();
	TimerInfo.bIsActive = true;

	// 활성 타이머 목록에 추가
	ActiveTimers.Add(TimerInfo.Handle, TimerInfo);

	return TimerInfo.Handle;
}

void URealTimeTimerManager::CancelRealTimeTimer(const FString& InTimerHandle)
{
	// 타이머 목록에서 제거
	ActiveTimers.Remove(InTimerHandle);
}

void URealTimeTimerManager::UpdateTimers()
{
	// 현재 실제 시간 가져오기
	double          CurrentTime = FPlatformTime::Seconds();
	TArray<FString> CompletedTimers;

	// 모든 활성 타이머 업데이트
	for (auto& TimerPair : ActiveTimers)
	{
		FRealTimeTimerInfo& TimerInfo = TimerPair.Value;

		if (TimerInfo.bIsActive && (CurrentTime - TimerInfo.StartTime >= TimerInfo.Duration))
		{
			// 타이머 완료
			if (TimerInfo.Callback.IsBound())
			{
				TimerInfo.Callback.Execute();
			}

			// 완료된 타이머 목록에 추가
			CompletedTimers.Add(TimerPair.Key);
		}
	}

	// 완료된 타이머 제거
	for (const FString& TimerHandle : CompletedTimers)
	{
		ActiveTimers.Remove(TimerHandle);
	}
}

FString URealTimeTimerManager::GenerateTimerHandle()
{
	// 고유한 타이머 핸들 생성
	static uint32 Counter = 0;
	return FString::Printf(TEXT("RealTimer_%u"), ++Counter);
}

/**
 * 리얼타임 타이머 라이브러리 구현
 */
URealTimeTimerManager* URealTimeTimerLibrary::GetTimerManager()
{
	// 싱글턴 패턴 - 인스턴스가 없으면 생성
	if (!TimerManager)
	{
		TimerManager = NewObject<URealTimeTimerManager>();
		TimerManager->AddToRoot(); // 가비지 컬렉션 방지
	}
	return TimerManager;
}

FString URealTimeTimerLibrary::StartRealTimeTimer(float Duration, const FRealTimeTimerDelegate& Callback)
{
	return GetTimerManager()->StartRealTimeTimer(Duration, Callback);
}

void URealTimeTimerLibrary::CancelRealTimeTimer(const FString& TimerHandle)
{
	GetTimerManager()->CancelRealTimeTimer(TimerHandle);
}

// 리얼타임 지연을 위한 Latent Action
class FRealTimeDelayAction : public FPendingLatentAction
{
public:
	float          Duration;
	FName          ExecutionFunction;
	int32          OutputLink;
	FWeakObjectPtr CallbackTarget;
	double         StartTime;

	FRealTimeDelayAction(float InDuration, const FLatentActionInfo& LatentInfo)
		: Duration(InDuration)
		, ExecutionFunction(LatentInfo.ExecutionFunction)
		, OutputLink(LatentInfo.Linkage)
		, CallbackTarget(LatentInfo.CallbackTarget)
	{
		StartTime = FPlatformTime::Seconds();
	}

	virtual void UpdateOperation(FLatentResponse& Response) override
	{
		double CurrentTime = FPlatformTime::Seconds();
		bool   bIsComplete = (CurrentTime - StartTime) >= Duration;
		Response.FinishAndTriggerIf(bIsComplete, ExecutionFunction, OutputLink, CallbackTarget);
	}

#if WITH_EDITOR
	virtual FString GetDescription() const override
	{
		return FString::Printf(TEXT("Real Time Delay: %.2f seconds"), Duration);
	}
#endif
};

void URealTimeTimerLibrary::RealTimeDelay(const UObject* WorldContextObject, float Duration, FLatentActionInfo LatentInfo)
{
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		FLatentActionManager& LatentActionManager = World->GetLatentActionManager();
		if (LatentActionManager.FindExistingAction<FRealTimeDelayAction>(LatentInfo.CallbackTarget, LatentInfo.UUID) == nullptr)
		{
			LatentActionManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, new FRealTimeDelayAction(Duration, LatentInfo));
		}
	}
}

/**
 * Real Time Timer Subsystem 구현
 */
void URealTimeTimerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	// Engine Tick에 업데이트 함수 연결
	TickDelegateHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &URealTimeTimerSubsystem::Tick)
		);
}

void URealTimeTimerSubsystem::Deinitialize()
{
	// Engine Tick에서 연결 해제
	FTSTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);
}

bool URealTimeTimerSubsystem::Tick(float DeltaTime)
{
	// 타이머 업데이트
	URealTimeTimerLibrary::GetTimerManager()->UpdateTimers();
	return true;
}