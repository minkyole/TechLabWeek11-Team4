# AnimGraph 사용 방법

## 구조

`UAnimGraphAsset`(`.animgraph`)은 공유 가능한 정의이며, `UAnimGraphInstance`는 컴포넌트마다 노드와 재생 상태를 생성한다. 최상위 그래프와 `.animsm`의 상태 내부는 같은 `FAnimPoseGraph` 모델과 편집 위젯을 사용한다.

지원 노드는 Sequence Player, 1D BlendSpace Player, Two-way Blend, State Machine, Output Pose다. State Machine 노드는 `.animsm` 파일 경로를 참조한다. 각 상태 내부에도 같은 포즈 그래프를 작성할 수 있다. 기존 Transition Rule Graph의 사용 방법은 동일하다.

## 그래프 작성

1. Content Drawer의 빈 공간을 우클릭하고 Create 메뉴에서 **AnimGraph**를 선택한다.
2. Skeletal Mesh와 저장 경로(`Assets/.../*.animgraph`)를 지정한다.
3. 노드 종류를 선택하고 **Add node**로 추가한다.
4. 노드를 선택하고 오른쪽 Selection에서 에셋, 재생 옵션, Alpha/Axis 값을 지정한다.
5. 출력 Pose 핀에서 입력 핀으로 드래그한다. Two-way Blend의 A/B를 연결하고 그 출력을 Output Pose에 연결한다.
6. **Save** 또는 Ctrl+S로 저장한다. Content Drawer에서 파일을 더블클릭하면 다시 열린다.

그래프마다 Output Pose는 하나이며 삭제할 수 없다. 입력·출력 핀 하나에는 연결 하나만 허용한다. 순환 연결은 거부한다. 연결을 지우려면 선택 후 Delete를 누른다. 연결되지 않은 노드는 저장되지만 실행에서 제외된다. 활성 출력에 필요한 입력이 빠져 있으면 저장·프리뷰에 오류가 표시된다.

`.animsm`은 State Machine Editor에서 연다. **상태 더블클릭** 또는 **Edit Pose Graph** 버튼으로 상태 내부 포즈를 편집하고, 위쪽 **State Machine** 버튼으로 돌아간다. 전이선을 더블클릭하면 기존 Rule Graph로 들어간다. 구버전 상태는 로드 시 `Player → Output Pose`로 변환되며, 저장할 때 `.animsm` 버전 2로 기록된다. 로드만으로 원본 파일을 변경하지 않는다.

## 파라미터와 스크립트

Parameters에서 Bool/Float/Trigger를 선언한다. Blend Alpha와 BlendSpace Axis는 Float 파라미터 또는 상수를 사용한다. Alpha는 실행 시 0~1로 제한된다. Play rate는 음수로 역재생, 0으로 정지할 수 있다.

내장 `.animsm`의 파라미터는 이름과 타입으로 상위 저장소에 합쳐진다. 같은 이름·같은 타입은 같은 값이며 타입 충돌은 오류다. 기본값은 상위 그래프의 선언이 우선하고, 상위에 없는 이름은 참조 에셋에서 가져온다. 동일한 이름을 여러 참조가 선언하면 먼저 발견한 기본값을 사용하므로, 공유 파라미터의 초기값은 최상위에 명시하는 것을 권장한다. Trigger 소비는 동일 저장소에 반영된다.

AnimGraph의 Optional UpdateScript는 아래 함수를 제공한다.

```lua
function UpdateAnimation(anim, dt)
    local movement = anim:GetMovementComponent()
    -- 게임에 맞는 입력으로 파라미터를 갱신한다.
    anim:SetFloat("BlendAlpha", 0.5)
end
```

`SetBool`, `SetFloat`, `SetTrigger`, `ResetTrigger`, `GetBool`, `GetFloat`, `IsTriggerActive`, `GetMovementComponent`를 사용할 수 있다. 캐릭터 Lua에서도 기존 `actor:GetAnimInstance()`로 현재 인스턴스를 얻는다. C++에서는 `GetAnimInstance()`가 반환하는 `UAnimInstance`의 공통 파라미터 API를 사용한다.

Lua 갱신 후 입력 바인딩을 반영하고 포즈를 Update/Evaluate한다. AnimGraph 안에서는 **최상위 UpdateScript만 실행**한다. 참조 `.animsm`의 스크립트는 독립 StateMachine 모드에서만 실행한다. 스크립트 Update가 실패하면 오류를 기록하고 해당 콜백의 반복 실행을 중단한다.

## 컴포넌트 설정

SkeletalMeshComponent Details의 **Animation Mode**를 선택한다.

| 모드 | 실행체 | 설정 |
|---|---|---|
| Lua | ULuaAnimInstance | AnimationScriptPath |
| StateMachine | UAssetAnimInstance | StateMachineAssetPath |
| AnimGraph | UAnimGraphInstance | AnimGraphAssetPath |
| SingleNode | UAnimSingleNodeInstance | 기존 단일 재생 API |

경로 입력은 모드를 자동으로 바꾸지 않는다. 모드를 바꿔도 각 경로는 보존된다. 경로는 `Assets/...` 형태로 입력한다. 노드의 Sequence/BlendSpace는 에셋 선택 목록 또는 기존 에셋 키를 사용한다. 신규 컴포넌트는 Lua가 기본이다. AnimationMode가 저장되지 않은 구버전 씬은 `.animsm` 경로가 있으면 StateMachine, 없으면 Lua로 읽는다.

`SetAnimInstanceClass()`는 코드에서만 사용하는 사용자 클래스 지정 경로다. 이를 호출하면 내부 Custom 모드를 사용하며, 명시적 모드를 다시 선택하면 해당 모드의 실행체로 돌아간다.

## 프리뷰와 갱신

**Apply to 3D Preview**는 저장 전 작업 복사본을 Skeletal Mesh Viewer에서 실행한다. 프리뷰에서는 Lua 갱신을 끄고 수동 파라미터 입력, Play/Pause, Reset을 제공한다. 내장 State Machine의 현재 상태 ID와 전환 진행률도 표시한다. `.animgraph` 또는 의존 `.animsm`을 편집기에서 저장하면 해당 에셋을 사용하는 컴포넌트를 다시 구성하고 재생 상태를 초기화한다. 외부 프로그램에서 수정한 파일의 자동 감시는 포함하지 않는다.

상태 내부 그래프, 중첩 State Machine은 지원하지만 임의의 Blueprint 실행, 연산 핀, Cached Pose, 포즈 출력 공유, 2D BlendSpace 및 스켈레톤 리타게팅은 지원하지 않는다.

실행 검증 절차는 [AnimGraphValidationChecklist.md](AnimGraphValidationChecklist.md)를 따른다.
