#pragma once

#include "Components/MovementComponent.h"

class UCharacterMovementComponent : public UMovementComponent
{
	DECLARE_CLASS(UCharacterMovementComponent, UMovementComponent)

public:
	UCharacterMovementComponent();

	void TickComponent(float DeltaTime) override;

	// 월드 좌표 기준 방향.
	// 값은 다음 SetMoveInput 또는 Stop까지 유지된다.
	void SetMoveInput(const FVector& WorldDirection);
	void SetMaxSpeed(float InMaxSpeed);

	FVector GetVelocity() const { return Velocity; }
	float GetGroundSpeed() const;

	void StopMovementImmediately() override;

private:
	FVector MoveInput = FVector::ZeroVector;

	float MaxSpeed = 5.0f;
};