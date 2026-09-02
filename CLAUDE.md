# imgui-video-player 프로젝트 원칙 (Project guidelines)

이 문서는 세션과 에이전트가 바뀌어도 유지되는 상시 목표와 규칙을 기록한다.
QKeyboard 저장소의 규칙 구조(CLAUDE.md + AGENTS.md)를 따른다. 에이전트 공통
작업 규칙은 [AGENTS.md](AGENTS.md), 설계는 [docs/architecture.md](docs/architecture.md),
UI 기준선은 [docs/wireframes.md](docs/wireframes.md)에 있다.

## 프로젝트 목표

1. **하드웨어 가속 디코딩이 기본이다.** 새 디코딩 경로는 항상 HW를 먼저
   시도하고 SW 폴백을 가져야 한다. SW를 무기한 강제하는 코드(예: 편의상
   `hw_policy=kOff` 하드코딩)를 넣지 않는다. UI는 사용자가 항상 현재
   디코더 경로(`(d3d11va)`/`(sw)` 등)를 볼 수 있게 한다 — "HW가 켜졌는지
   모르는 상태"는 버그로 본다.
2. **계층 분리를 지킨다.** (`docs/architecture.md` §3)
   - `media/`는 ImGui/D3D11 헤더를 include하지 않는다. 렌더러 디바이스
     전달 같은 예외는 `SourceOptions`의 `void*` 필드로 중립적으로 유지
     (decoder.cpp의 Windows 전용 glue만 D3D11 헤더를 include).
   - `render/`는 FFmpeg 헤더를 include하지 않는다. 프레임은 `media/frame.h`
     (VideoFrame)로만 받는다.
   - `ui/`는 `AVFrame*`을 직접 다루지 않는다. 화면에는 `VideoTexture`만 넘긴다.
   - 플랫폼 전용 코드는 `#ifdef` + 인터페이스 뒤로 숨기고, macOS(Metal)/
     Linux(GL) 이식(M6)을 깨는 변경을 하지 않는다.
3. **스레딩 계약.** GPU 작업(텍스처 업로드, 변환 패스, ImGui 렌더)은
   렌더 스레드에서만. 미디어 워커 스레드는 GPU API를 호출하지 않는다.
   스레드 간 데이터 전달은 FrameQueue(드롭 정책 있음) 또는 아토믹/뮤텍스로
   보호된 스냅샷으로만 한다.
4. **실시간 정책: 최신 프레임 우선.** 어떤 큐도 무한정 쌓이지 않는다. 새
   큐/버퍼를 추가하면 반드시 capacity와 만원 시 동작(드롭 정책)을 명시한다.
   CCTV 뷰어에서 누적 버퍼링은 지연이므로 결함이다.
5. **C++20 + Effective C++.** const 정확성, 비트라비얼 타입은 `const&` 전달,
   암묵 변환 방지용 `explicit`, `enum class`, 멤버 초기화 리스트, 소유권은
   RAII(ComPtr/unique_ptr)로. 가상 소멸자는 다형적 기반 클래스에만.
6. **로드맵은 마일스톤/이슈가 단일 출처다.** GitHub 마일스톤 M0~M6 +
   이슈(#1~#27)가 진행 상태이며, `docs/architecture.md` §8과 일치를 유지한다.
   작업은 이슈를 만들거나 지정하고, 커밋/PR에서 참조한다.

## 워크플로

- 이슈 1개 = 브랜치 1개 = PR 1개. 브랜치 규칙과 시작 전/푸시 전 체크는
  [AGENTS.md](AGENTS.md)를 따른다. master에 직접 푸시하지 않는다.
- PR은 저장소 소유자가 머지한다. 자기 PR을 자기가 머지하지 않는다.
- 커밋 메시지/PR 본문에 이슈를 참조한다 (`Closes #N`).
- UI 변경은 구현 전에 `docs/wireframes.md`에 와이어프레임/명세를 먼저 넣는다.

## 테스트

전략·컨벤션의 단일 출처는 [docs/testing.md](docs/testing.md)다. 요약:

1. **로직은 단위 테스트를 강제한다.** `tests/`(GoogleTest) — 조건부가 아닌
   기본. media/render의 순수 로직(큐 정책, 판별기, 변환 수학, 상태 머신)을
   고치거나 추가하면 대응 테스트를 같은 PR에 넣는다.
2. **테스트하려는 로직이 GPU/FFmpeg 타입에 묻어 있으면 먼저 분리한다.**
   순수 함수로 추출(선례: `render/color_conversion.cpp`)한 뒤 테스트한다.
   테스트 때문에 계층 규칙(목표 2)을 어기지 않는다.
3. **CI가 테스트를 실행한다.** 빌드 잡의 ctest가 실패하면 머지 불가.
   테스트가 깨진 채로 두는 것은 master를 깨는 것과 같다.
4. **GPU/화면 검증은 수동 절차로 문서화한다.** D3D11 렌더링 자체는 L1
   단위 테스트 대상이 아니다 — 재현 절차를 PR에 적고, 반복 가능해지면
   테스트로 승격한다.
5. **버그 fix는 회귀 테스트 우선.** 단위 테스트로 표현 가능한 버그는
   "실패하는 테스트 → fix" 순서로 작업한다. 불가능하면 재현 절차를 PR에.

## 엔지니어링 규율 (실제 사고에서 나온 규칙 — 희망이 아닌 체크리스트)

푸시/PR 전에 아래를 점검한다. 각 규칙의 출실 사건을 적어 둔다.

1. **FFmpeg 트랩은 문서화한다.** 이미 당한 것들:
   - `get_format`은 `avcodec_open2` 이후 첫 프레임 때 지연 호출된다 →
     디코더 경로(HW/SW)는 첫 프레임에서 확정해야 한다 (decoder.cpp 참고).
   - SRV desc의 `MipLevels = 0`(영 초기화)은 E_INVALIDARG다 — MipLevels를
     반드시 설정 (video_texture.cpp 참고).
   - 제로카피를 위해선 FFmpeg hwdevice를 렌더러 D3D11 디바이스에서 생성해야
     한다 (`av_hwdevice_ctx_alloc` + `AVD3D11VADeviceContext`).
   새 트랩을 만나면 PR 본문에 기록하고, 범용이면 이 규칙에 추가한다.
2. **진단 로그는 1회성으로.** 프레임마다 반복되는 오류 로그(2026-09-02
   SRV 오류 300+행 스팸)는 로그를 무용지물로 만든다. 상태 전이 시 1회만
   남기고, HRESULT/에러코드는 숫자로 기록한다.
3. **깨진 시도와 대체 fix를 함께 남기지 않는다.** 같은 문제를 고칠 수 있는
   곳이 둘 이상이면 정확히 한 곳만 고친다. 환경 전용 문제(CI 도구체인)는
   CI 레벨에서, 소비자도 필요로 하는 것은 CMake에서 (2026-09-01 CI
   VCPKG_ROOT 이 incident 참고).
4. **푸시 전에 로컬 게이트 통과.** `clang-format --dry-run -Werror`와
   cppcheck를 로컬에서 먼저 돌린다. CI가 첫 검증기가 되지 않게 한다.
5. **UI 문서-코드 정렬.** 구현이 `docs/wireframes.md` 명세와 어긋나면
   같은 PR에서 문서 또는 코드 중 하나를 맞춘다.
6. **버그 fix는 회귀 검증과 함께.** 단위 테스트로 표현 가능한 버그는
   실패하는 테스트를 먼저 추가하고 fix한다 (테스트 절 5). 불가능한 경우
   fix PR 본문에 재현 절차(명령줄/입력/기대 로그)를 적는다.
7. **빌드 산출물/에이전트 아티팩트 커밋 금지.** `build/`, `.zcode/` 등은
   .gitignore 관리 하에 둔다. 커밋에 포함되면 즉시 제거한다.

## 버전

Semantic Versioning, 현재 `0.1.0` (pre-1.0). 버전의 단일 출처는 최상위
`CMakeLists.txt`의 `project(imgui-video-player VERSION ...)`이다. 버전을
바꾸는 유일한 이유: 배포 지점. 내부 마일스톤 진행은 버전을 올리지 않는다.
