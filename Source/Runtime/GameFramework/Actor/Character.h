#pragma once

#include "GameFramework/Actor.h"

class UCapsuleComponent;
class USkeletalMeshComponent;
class UCharacterMovementComponent;
class UScriptComponent;
class UCameraComponent;
class ULuaAnimInstance;

class ACharacter : public AActor
{
	DECLARE_CLASS(ACharacter, AActor)
public:
	ACharacter();

	void BeginPlay() override;

	UCapsuleComponent* GetCapsuleComponent() const;
	USkeletalMeshComponent* GetMesh() const;
	UCharacterMovementComponent* GetCharacterMovement() const;
	UScriptComponent* GetScriptComponent() const;
	UCameraComponent* GetCameraComponent() const;
	ULuaAnimInstance* GetLuaAnimInstance() const;

private:
	// Actor의 기본 컴포넌트를 참조한다.
	// 여기서 별도로 delete하지 않는다.
	UCapsuleComponent* CapsuleComponent = nullptr;
	USkeletalMeshComponent* SkeletalMeshComponent = nullptr;
	UCharacterMovementComponent* MovementComponent = nullptr;
	UScriptComponent* ScriptComponent = nullptr;
	UCameraComponent* CameraComponent = nullptr;
};