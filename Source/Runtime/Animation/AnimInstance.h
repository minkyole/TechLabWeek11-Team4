#pragma once

#include "UObject/Object.h"

class FAnimNode_Base;

class UAnimInstance : public UObject
{
	virtual void NativeInitializeAnimation();               // 그래프를 만든다 (한 번)
	virtual void NativeUpdateAnimation(float DeltaSeconds); // 변수를 갱신한다 (매 프레임)
	//...
protected : 
	FAnimNode_Base* RootNode;                   // 그래프의 시작 노드 (StateMachine 등)
};

class UAnimSingleNodeInstance : public UAnimInstance
{
};