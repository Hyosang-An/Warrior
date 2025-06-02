#include "WarriorTypes/WarriorCountDownAction.h"

#include "WarriorDebugHelper.h"

void FWarriorCountDownAction::UpdateOperation(FLatentResponse& Response)
{
	if (bNeedToCancel)
	{
		// 참조 변수기 때문에 원본이 변경됨
		CountDownOutput = EWarriorCountDownActionOutput::Cancelled;

		Response.FinishAndTriggerIf(true, ExecutionFunction, OutputLink, CallbackTarget);

		return;
	}

	// if (ElapsedTimeSinceStart >= TotalCountDownTime)
	// {
	// 	// 참조 변수기 때문에 원본이 변경됨
	// 	CountDownOutput = EWarriorCountDownActionOutput::Completed;
	//
	// 	Response.FinishAndTriggerIf(true, ExecutionFunction, OutputLink, CallbackTarget);
	// 	return;
	// }

	// 1) 지난 틱 경과 시간
	const float DeltaTime = Response.ElapsedTime();

	// 2) 유효 인터벌(EffectiveInterval) 계산
	const float EffectiveInterval = FMath::Max(UpdateInterval, DeltaTime);

	// 3) 경과 시간 누적
	ElapsedInterval += DeltaTime;

	// 4) 일정 간격 이상 쌓였을 때만 한 번 처리
	if (ElapsedInterval < EffectiveInterval)
	{
		return;
	}

	// 5) 전체 경과 시간 갱신
	ElapsedTimeSinceStart += EffectiveInterval;
	OutRemainingTime = FMath::Max(0.f, TotalCountDownTime - ElapsedTimeSinceStart);

	// 6) 완료 여부 분기
	if (OutRemainingTime == 0.f)
	{
		// 참조 변수기 때문에 원본이 변경됨
		CountDownOutput = EWarriorCountDownActionOutput::Completed;
		Response.FinishAndTriggerIf(true, ExecutionFunction, OutputLink, CallbackTarget);
		return;
	}

	// 참조 변수기 때문에 원본이 변경됨
	CountDownOutput = EWarriorCountDownActionOutput::Updated;
	Response.TriggerLink(ExecutionFunction, OutputLink, CallbackTarget);

	ElapsedInterval -= EffectiveInterval;
}

void FWarriorCountDownAction::CancelAction()
{
	bNeedToCancel = true;
}