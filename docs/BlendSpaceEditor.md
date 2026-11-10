# BlendSpace 1D 편집기

## 사용

1. Content Drawer의 빈 영역을 우클릭하여 **Create > Animation > BlendSpace 1D**를 선택한다.
2. 이름과 기준 Skeletal Mesh를 지정한다. 기본 축은 Speed, 범위는 0~600이다.
3. Asset Browser의 애니메이션을 Blend Graph에 드래그한다. `+` 버튼은 축 중앙에 추가한다.
4. 마름모를 드래그하거나 Sample Details의 X 값을 입력해 위치를 바꾼다. 같은 위치에 두 샘플을 둘 수 없다.
5. 그래프를 선택한 상태에서 Delete 또는 샘플 우클릭 메뉴로 삭제한다. 드래그 중 Escape는 이동을 취소한다.
6. Save 또는 편집기에 포커스가 있을 때 Ctrl+S로 저장한다. Content Drawer에서 파일을 더블클릭하여 다시 연다.

좌우 패널 폭은 구분선을 드래그하여 조절한다. Sample Details와 Asset Browser 사이의 수평 구분선으로 높이를 조절한다. 미저장 상태로 닫거나 다른 에셋을 열면 Save / Discard / Cancel을 선택한다. 편집 중에는 작업 사본만 바뀌며 저장 성공 후 등록 객체에 반영된다.

이번 구현에는 3D 미리보기, 2D, Undo/Redo, Save As와 추가 보간 설정이 없다.

## 파일과 참조

`.blendspace`는 버전 1 JSON이다. 필드는 `Version`, `Type`, `SkeletalMeshAssetKey`, `Axis` (`Name`, `Min`, `Max`), `Samples` (`AnimationAssetKey`, `X`)이다. 빈 샘플 목록도 저장할 수 있다.

애니메이션은 FBX 임포트와 베이크 로드에서 `FBX 에셋 키#Animation/클립 인덱스`로 등록된다. BlendSpace는 UAnimSequence의 GetPath()를 저장하고 AssetManager 조회로 복원한다. FBX 내부 참조를 직접 해석하지 않는다. 현재 같은 Skeletal Mesh를 사용하는 클립만 허용한다. 재임포트로 클립 순서가 바뀌면 인덱스 기반 참조를 다시 확인해야 한다.

시작 시 원본 에셋과 애니메이션을 먼저 등록한 뒤 BlendSpace를 로드한다. 없는 참조, 잘못된 타입·버전·범위·중복 샘플은 파일 전체의 로드 실패로 처리한다. 저장은 같은 폴더의 임시 파일 작성 후 교체하며 실패 시 이전 파일을 보존한다.

## 빌드와 테스트

프로젝트 파일을 다시 생성할 때 premake의 기존 Runtime/Editor 소스 glob이 새 파일을 포함한다. Debug Runtime·Editor 빌드 후 **x64 Visual Studio Developer Command Prompt**에서 다음을 실행한다.

```bat
Tools\Tests\RunBlendSpaceTests.cmd
```

- `BlendSpaceAssetTests.cpp`: 47개 검사. 등록 참조, 저장·복원, 가중치, 손상 파일, 호환성, 저장 실패 보존, 단계별 스캔을 확인한다.
- `BlendSpaceEditorTests.cpp`: 15개 검사. 실제 ImGui 프레임에서 그래프 표시, 선택, 이동, Ctrl+S, 작업 사본, 삭제, 닫기·버리기와 재열기를 확인한다. GPU와 OS 창은 필요하지 않다.

테스트는 Assets 아래에 프로세스별 임시 폴더를 만들고 성공 후 제거한다. 실패한 테스트의 `_BlendSpaceTests_숫자` 또는 `_BlendSpaceUITests_숫자` 폴더는 실패 조사 후 제거할 수 있다.

검증 환경의 C1902 디버그 정보 도구 충돌은 로컬 빌드 옵션에서 `/Z7`과 PCH 비활성화를 사용해 우회했다. 저장소의 공통 빌드 설정은 변경하지 않았다.
