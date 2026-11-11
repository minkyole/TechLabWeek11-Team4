# AnimGraph 수동 검증 목록

실행 테스트는 사용자에게 맡긴다. 아래 항목은 **미실행 상태**이며 완료 표시가 검증 결과를 의미한다.

빌드 확인: 2026-11-11, Debug x64의 `HitoriEngine.lib`와 `HitoriEditor.exe` 생성 성공(오류 0). Visual Studio PDB 관리자 불일치(C1902)를 우회하기 위해 해당 빌드 프로세스에만 `_CL_=/Z7`을 적용했다. 프로젝트의 영구 컴파일 옵션은 변경하지 않았다. 로그: `Build/AnimGraphVerifiedBuild.log`. 게임·에디터 실행 및 테스트 실행 파일은 실행하지 않았다. 신규 소스는 기존 `premake5.lua`의 Runtime/Editor 재귀 소스 수집에 포함된다.

## 준비

- [ ] 동일 Skeletal Mesh의 서로 다른 Sequence A/B, 루프 이동 클립, 비루프 공격 클립을 준비한다.
- [ ] 같은 메시의 1D BlendSpace와 기존 `.animsm`을 준비한다. 원본 `.animsm` 및 씬은 복사본을 사용한다.
- [ ] Notify/NotifyState가 있는 클립과 RootMotion이 있는 클립을 준비하고 로그·캐릭터 이동을 관찰할 수 있게 한다.

## 최상위 그래프

- [ ] Content Drawer에서 AnimGraph를 생성하고 `Sequence → Output Pose`를 연결한다. 저장 전 Apply to 3D Preview에서 재생된다.
- [ ] `Sequence A/B → Two-way Blend → Output Pose`를 구성한다. Alpha 0은 A, 1은 B, 0.5는 두 포즈의 혼합이다.
- [ ] Alpha를 범위 밖(-1, 2)으로 입력하면 양 끝 포즈로 제한된다.
- [ ] Float 파라미터를 Alpha에 바인딩하고 프리뷰 입력을 바꿀 때 포즈가 변한다.
- [ ] BlendSpace를 연결하고 Axis 상수·Float 바인딩으로 샘플 혼합이 변한다.
- [ ] Loop, Playing, Play rate 0·양수·음수, Play/Pause, Reset 동작을 확인한다.
- [ ] Alpha가 0/1일 때 가중치가 없는 가지의 시간이 진행하지 않고, 다시 가중치를 주면 이어서 진행한다.
- [ ] 노드 이동, 연결 삭제, 노드 삭제, Frame all, Ctrl+S, 재열기 후 노드 위치와 설정이 보존된다.
- [ ] 수정한 그래프를 닫거나 다른 에셋을 열 때 Save/Discard/Cancel이 의도대로 동작한다.

## State Machine과 상태 내부

- [ ] `State Machine(.animsm) + Sequence → Blend → Output`을 구성하고 두 입력이 혼합된다.
- [ ] State Machine Editor에서 상태를 더블클릭하여 `Sequence A/B → Blend → Output`을 작성한다. 상위 탐색 버튼으로 돌아갈 수 있다.
- [ ] 전이선을 더블클릭하면 기존 Rule Graph가 열리고 조건·우선순위·Blend duration이 유지된다.
- [ ] Bool·Float·Trigger 전이와 SourceFinished 조건이 상태 내부 포즈 그래프에서 동작한다.
- [ ] 비루프 A/B 혼합의 SourceFinished는 유효 가중치 입력이 모두 완료된 뒤 참이다. 루프 입력에 유효 가중치가 있으면 완료되지 않는다.
- [ ] 재진입·동일 상태 재시작·AnyState·전환 도중 인터럽트를 확인한다. 재진입 시 하위 Player 시간이 초기화된다.
- [ ] 비활성 상태에 중첩 State Machine을 두어도 초기화 시 assert가 없고 해당 상태 진입 전에는 진행하지 않는다.
- [ ] 중첩 State Machine의 파라미터가 이름·타입으로 연결되고 Trigger 소비가 상위에서도 관찰된다.
- [ ] State Machine 상태 내부에 다른 `.animsm`을 참조한 구조를 저장·재로드해 재생한다.

## Animation Mode와 씬

- [ ] Lua / StateMachine / AnimGraph / SingleNode 선택 시 실제 실행체가 바뀌고 해당 모드의 경로만 표시된다.
- [ ] 모드를 반복 전환해도 각 경로가 보존되며, 경로 입력만으로 모드가 바뀌지 않는다.
- [ ] 활성 에셋·스크립트·메시를 변경하면 새 설정으로 초기화되고 이전 NotifyState가 종료된다.
- [ ] 서로 다른 모드의 컴포넌트를 씬에 저장하고 재로드한 뒤 모드와 경로가 유지된다.
- [ ] Actor/컴포넌트를 복제하면 설정이 같고, 실행 시간·파라미터·현재 상태는 서로 독립적이다.
- [ ] 구버전 씬은 `.animsm` 경로 유무에 따라 StateMachine/Lua로 복원된다.
- [ ] 기존 캐릭터 Lua, 기존 `.animsm`, Sequence/BlendSpace 뷰어의 SingleNode 프리뷰가 계속 동작한다.
- [ ] 편집기에서 그래프 또는 의존 `.animsm`을 저장하면 관련 컴포넌트만 재구성되며 재생 상태가 초기화된다.

## Lua·Notify·RootMotion

- [ ] AnimGraph UpdateScript가 프레임마다 파라미터를 갱신하고 그 프레임 포즈에 반영된다.
- [ ] 캐릭터 Lua의 `GetAnimInstance()`로 AnimGraph를 얻어 Bool/Float/Trigger를 설정·조회할 수 있다.
- [ ] 독립 StateMachine에서는 `.animsm` UpdateScript가 실행되고, AnimGraph 안에서는 최상위 스크립트만 실행된다.
- [ ] 스크립트 오류는 로그에 표시되고 동일 오류가 매 프레임 반복 실행되지 않는다.
- [ ] 가중치 0 가지에서 Notify와 RootMotion이 발생하지 않고, 혼합 중에는 가중치에 맞게 RootMotion이 반영된다.
- [ ] 루프·역재생·전환·인터럽트에서 Notify의 중복 발생이나 누락을 확인한다.
- [ ] NotifyState 도중 모드 변경, 에셋 변경, 메시 변경, 재빌드, 컴포넌트 삭제 시 End가 한 번 발생한다.
- [ ] 같은 에셋을 사용하는 두 캐릭터의 Notify·RootMotion·Trigger가 서로 섞이지 않는다.

## 오류와 저장 호환성

- [ ] 순환 연결, 같은 출력의 다중 연결, 같은 입력의 중복 연결을 편집기가 거부한다.
- [ ] 출력 필수 입력 누락, 잘못된 에셋 키, 메시 불일치, 비어 있는 BlendSpace를 저장·프리뷰에서 오류로 안내한다.
- [ ] 이름 중복 파라미터, Float 바인딩 타입 불일치, 내장 `.animsm`과의 타입 충돌을 오류로 안내한다.
- [ ] 직접/간접 `.animsm` 순환 참조를 거부하며 크래시·무한 재귀가 없다.
- [ ] 연결되지 않은 미완성 노드를 저장할 수 있고 실행 결과에는 영향을 주지 않는다.
- [ ] 파일 누락·잘못된 JSON·지원하지 않는 버전에서 선택 모드가 유지되고 참조 포즈와 오류를 제공한다.
- [ ] 구버전 `.animsm`을 열기만 하면 파일이 바뀌지 않는다. 저장 후 Version 2 및 PoseGraph가 기록되고 재열기 후 동일하게 동작한다.

## 검증 기록

| 항목 | 결과 | 재현 에셋/씬 | 메모 |
|---|---|---|---|
| 최상위 그래프 | 미실행 | | |
| 상태 내부/중첩 | 미실행 | | |
| 모드/저장/복제 | 미실행 | | |
| Lua | 미실행 | | |
| Notify/RootMotion | 미실행 | | |
| 오류/호환성 | 미실행 | | |
