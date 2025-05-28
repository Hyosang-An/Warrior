#include "Controllers/WarriorAIController.h"

#include "WarriorDebugHelper.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Navigation/CrowdFollowingComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"


AWarriorAIController::AWarriorAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UCrowdFollowingComponent>("PathFollowingComponent"))
{
	AISenseConfig_Sight = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("AISenseConfig_Sight"));
	AISenseConfig_Sight->DetectionByAffiliation.bDetectEnemies = true;
	AISenseConfig_Sight->DetectionByAffiliation.bDetectFriendlies = false;
	AISenseConfig_Sight->DetectionByAffiliation.bDetectNeutrals = false;
	AISenseConfig_Sight->SightRadius = 5000.f;
	AISenseConfig_Sight->LoseSightRadius = 5000.f;
	AISenseConfig_Sight->PeripheralVisionAngleDegrees = 360.f;

	// UAIPerceptionComponent를 AIController에서 설정하지만 위치나 회전 기준은 Possess된 Pawn이다.
	EnemyPerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("EnemyPerceptionComponent"));
	EnemyPerceptionComponent->ConfigureSense(*AISenseConfig_Sight);
	EnemyPerceptionComponent->SetDominantSense(UAISenseConfig_Sight::StaticClass());
	EnemyPerceptionComponent->OnTargetPerceptionUpdated.AddUniqueDynamic(this, &AWarriorAIController::OnEnemyPerceptionUpdated);

	AAIController::SetGenericTeamId(FGenericTeamId(1));
}

ETeamAttitude::Type AWarriorAIController::GetTeamAttitudeTowards(const AActor& Other) const
{
	// return Super::GetTeamAttitudeTowards(Other);

	const APawn* PawnToCheck = Cast<const APawn>(&Other);

	const IGenericTeamAgentInterface* OtherTeamAgent = Cast<const IGenericTeamAgentInterface>(PawnToCheck->GetController());

	if (OtherTeamAgent && OtherTeamAgent->GetGenericTeamId() < GetGenericTeamId())
	{
		return ETeamAttitude::Hostile;
	}

	return ETeamAttitude::Friendly;
}

void AWarriorAIController::BeginPlay()
{
	Super::BeginPlay();

	if (UCrowdFollowingComponent* CrowdComp = Cast<UCrowdFollowingComponent>(GetPathFollowingComponent()))
	{
		if (bEnableDetourCrowdAvoidance)
		{
			if (!bUseObstacleOnly)
				CrowdComp->SetCrowdSimulationState(ECrowdSimulationState::Enabled);
			else
				CrowdComp->SetCrowdSimulationState(ECrowdSimulationState::ObstacleOnly);
		}
		else
		{
			CrowdComp->SetCrowdSimulationState(ECrowdSimulationState::Disabled);
		}

		switch (DetourCrowdAvoidanceQuality)
		{
			case 1:
				CrowdComp->SetCrowdAvoidanceQuality(ECrowdAvoidanceQuality::Low);
				break;
			case 2:
				CrowdComp->SetCrowdAvoidanceQuality(ECrowdAvoidanceQuality::Medium);
				break;
			case 3:
				CrowdComp->SetCrowdAvoidanceQuality(ECrowdAvoidanceQuality::Good);
				break;
			case 4:
				CrowdComp->SetCrowdAvoidanceQuality(ECrowdAvoidanceQuality::High);
				break;
			default:
				break;
		}

		CrowdComp->SetAvoidanceGroup(1);
		CrowdComp->SetGroupsToAvoid(1);
		CrowdComp->SetCrowdCollisionQueryRange(CollisionQueryRange);
		// CrowdComp->SetCrowdSeparation(true);
		// CrowdComp->SetCrowdSeparationWeight(50.0f);
	}
}

void AWarriorAIController::OnEnemyPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (UBlackboardComponent* BlackboardComponent = GetBlackboardComponent())
	{
		if (!BlackboardComponent->GetValueAsObject(FName("TargetActor")))
		{
			if (Stimulus.WasSuccessfullySensed() && Actor)
			{
				BlackboardComponent->SetValueAsObject(FName("TargetActor"), Actor);
			}
		}
	}
}