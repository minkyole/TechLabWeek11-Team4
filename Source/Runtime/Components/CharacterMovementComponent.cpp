#include "EnginePCH.h"
#include "CharacterMovementComponent.h"

#include "Collision/HitResult.h"
#include "Components/ShapeComponent.h"
#include "GameFramework/Actor.h"

#include <cmath>

namespace
{
	FVector MoveVelocityTowards(
		const FVector& Current,
		const FVector& Target,
		float MaxChange)
	{
		const FVector Difference = Target - Current;
		const float Distance =
		    std::hypot(Difference.X, Difference.Y);

		if (Distance <= MaxChange || Distance <= 0.000001f)
			return Target;

		return Current + Difference * (MaxChange / Distance);
	}
}

UCharacterMovementComponent::UCharacterMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// PrePhysics의 Lua 입력 처리가 끝난 뒤 이동한다.
	PrimaryComponentTick.TickGroup = ETickingGroup::DuringPhysics;

	Velocity = FVector::ZeroVector;
}

void UCharacterMovementComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);

	AActor* Owner = GetOwner();

	if (!Owner || Owner->IsActorBeingDestroyed())
		return;

	if (!UpdatedComponent)
	{
		Velocity = FVector::ZeroVector;
		return;
	}

	// 초기 포즈 갱신 등 0초 업데이트에서는 기존 속도를 보존한다.
	if (!std::isfinite(DeltaTime) || DeltaTime <= 0.0f)
		return;

	// 현재 엔진에서 충동 Sweep을 사용하려면
	// 이동 대상이 blocking Shape여야 한다.
	UShapeComponent* Shape =
	    Cast<UShapeComponent>(UpdatedComponent);

	if (!Shape || !Shape->IsBlockComponent())
	{
		if (!bWarnedSweepWithoutShape)
		{
			HTR_LOG(
			    Warning,
			    "[CharacterMovement] A blocking Shape root is required.");

			bWarnedSweepWithoutShape = true;
		}

		StopMovementImmediately();
		return;
	}

	//const FVector DesiredVelocity = MoveInput * MaxSpeed;
	//const FVector MoveDelta = DesiredVelocity * DeltaTime;
	const FVector TargetVelocity = MoveInput * MaxSpeed;

	FVector CurrentVelocity = Velocity;
	CurrentVelocity.Z = 0.0f;

	const float CurrentSpeed =
	    std::hypot(CurrentVelocity.X, CurrentVelocity.Y);

	const float TargetSpeed =
	    std::hypot(TargetVelocity.X, TargetVelocity.Y);

	// 입력 해제 또는 목표 속도 감소에는 제동 감속도를 사용한다.
	// 같은 속도로 방향을 전환할 때에는 Acceleration을 사용한다.
	const bool bSlowingDown = TargetSpeed < CurrentSpeed;

	const float ChangeRate =
	    bSlowingDown ? BrakingDeceleration : Acceleration;

	const FVector NextVelocity = MoveVelocityTowards(
	    CurrentVelocity,
	    TargetVelocity,
	    ChangeRate * DeltaTime);

	const FVector MoveDelta = NextVelocity * DeltaTime;

	if (!std::isfinite(MoveDelta.X) ||
	    !std::isfinite(MoveDelta.Y) ||
	    !std::isfinite(MoveDelta.Z))
	{
		StopMovementImmediately();
		return;
	}

	// 이동이 없는 프레임에는 이전 속도를 반드시 지운다.
	if (MoveDelta.X == 0.0f && MoveDelta.Y == 0.0f)
	{
		Velocity = FVector::ZeroVector;
		UpdateComponentVelocity();
		return;
	}

	USceneComponent* MovingComponent = UpdatedComponent;

	const FVector PreviousLocation =
	    MovingComponent->GetWorldLocation();

	const FRotator Rotation =
	    MovingComponent->GetWorldRotation();

	FHitResult Hit;

	// false가 반환되더라도 실제 이동 결과를 위치 차이로 계산한다.
	// 충돌 정보는 반환 bool 대신 Hit를 통해 판단한다.
	MoveUpdatedComponent(
	    MoveDelta,
	    Rotation,
	    true,
	    &Hit);

	// MoveComponent의 OnHit에서 액터 파괴나
	// 이동 대상 변경이 요청될 수 있다.
	if (Owner->IsActorBeingDestroyed() ||
		UpdatedComponent != MovingComponent)
	{
		Velocity = FVector::ZeroVector;
		UpdateComponentVelocity();
		return;
	}

	const FVector CurrentLocation =
	    UpdatedComponent->GetWorldLocation();

	Velocity = (CurrentLocation - PreviousLocation) / DeltaTime;

	UpdateComponentVelocity();
}

void UCharacterMovementComponent::SetMoveInput(
	const FVector& WorldDirection)
{
	if (!std::isfinite(WorldDirection.X) ||
		!std::isfinite(WorldDirection.Y) ||
		!std::isfinite(WorldDirection.Z))
	{
		MoveInput = FVector::ZeroVector;
		return;
	}

	// 수평 이동만 지원한다.
	MoveInput = FVector(
	    WorldDirection.X,
	    WorldDirection.Y,
	    0.0f);

	// 대각선 이동 속도 증가 방지.
	// 길이 1 미만 입력은 유지하여 아날로그 입력도 허용한다.
	const double Length = std::hypot(
	    static_cast<double>(MoveInput.X),
	    static_cast<double>(MoveInput.Y));

	if (Length > 1.0)
	{
		MoveInput.X = static_cast<float>(MoveInput.X / Length);
		MoveInput.Y = static_cast<float>(MoveInput.Y / Length);
	}
}

void UCharacterMovementComponent::SetMaxSpeed(float InMaxSpeed)
{
	if (!std::isfinite(InMaxSpeed) || InMaxSpeed < 0.0f)
	{
		HTR_LOG(
		    Warning,
		    "[CharacterMovement] MaxSpeed must be finite and non-negative.");

		return;
	}

	MaxSpeed = InMaxSpeed;
}

void UCharacterMovementComponent::SetAcceleration(float Value)
{
	if (std::isfinite(Value) && Value >= 0.0f)
	{
		Acceleration = Value;
	}
}

void UCharacterMovementComponent::SetBrakingDeceleration(float Value)
{
	if (std::isfinite(Value) && Value >= 0.0f)
	{
		BrakingDeceleration = Value;
	}
}

float UCharacterMovementComponent::GetGroundSpeed() const
{
	return std::sqrt(
	    Velocity.X * Velocity.X +
	    Velocity.Y * Velocity.Y);
}

void UCharacterMovementComponent::StopMovementImmediately()
{
	MoveInput = FVector::ZeroVector;

	// 부모가 Velocity를 0으로 만들고 ComponentVelocity도 갱신한다.
	Super::StopMovementImmediately();
}