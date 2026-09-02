# 테스트 전략 및 컨벤션 (Testing)

> 이 문서가 테스트의 단일 기준이다. 테스트 규칙 요약은 [CLAUDE.md](CLAUDE.md)
> "테스트" 절, PR 요건은 [AGENTS.md](AGENTS.md)에 있다.

## 레벨

| 레벨 | 대상 | 제약 | 실행 |
|---|---|---|---|
| **L1 단위** | 순수 로직: 큐 정책, URL 판별, 색변환 수학, 상태 전이 | I/O·GPU·FFmpeg 데모 디코드 없음. 밀리초 내 실행 | `ctest` (로컬/CI) |
| **L2 통합** | 실제 미디어 파일을 파이프라인으로 통과 (demux→decode→frame) | 커밋된 소형 fixture 또는 생성 파일 필요 | `ctest` (fixture 태그) |
| **L3 수동 검증** | 화면 출력, HW 가속 경로, 멀티채널 부하 | 사람 눈 + 로그 | `docs/wireframes.md` 명세 + PR 본문 절차 |

- L1은 **항상** CI에서 실행된다 (`.github/workflows/ci.yml`의 Test 단계).
- L2는 fixture가 준비된 범위만, 실패해도 환경 탓이 아님을 확인 가능한 형태로.
- L3은 PR 본문의 "검증 방법"으로 문서화한다 (AGENTS.md PR 요건).

## 현재 커버 (`tests/`)

| 테스트 | 대상 | 검증 내용 |
|---|---|---|
| `test_frame_queue.cpp` | `FrameQueue` | capacity 초과 시 drop-oldest, `PopLatest` 미소비분 드롭 집계, `Close` 후 push 무시, Clear 집계 |
| `test_source_kind.cpp` | `DetectSourceKind` | rtsp/rtsps(대소문자), .mpd=DASH, 비-MPD HTTP=Unknown, 나머지=File |
| `test_color_conversion.cpp` | `FillConversionConstants` | limited/full offset·scale, BT.601/709 행렬(컬럼 메이저 패딩 포함), 메타데이터 없을 때 해상도 휴리스틱, 명시 메타데이터가 휴리스틱 Override |

`src/render/color_conversion.{h,cpp}`는 video_texture.cpp에서 뽑아낸 순수
함수다 — **테스트하려는 로직이 GPU/FFmpeg 타입에 묻어 있으면 먼저 분리한다**
(이번이 그 선례: HLSL cbuffer 패킹 규약은 주석으로 양쪽에 명시).

## 실행 방법

```sh
# 설정/빌드 (windows-default 또는 windows-ci 프리셋 — 테스트는 항상 켜짐)
cmake --preset windows-default
cmake --build --preset windows-default

# 전체 실행
ctest --test-dir build/windows-default --output-on-failure -C Release

# 하나만
ctest --test-dir build/windows-default -R FrameQueue -C Release -V
```

CI: push/PR 시 build 잡에서 `ctest --test-dir build/windows-ci` 실행.

정적 검사 스코프: cppcheck는 `src/`만 검사한다 (gtest 매크로는 컴파일 DB
없이 파싱되지 않아 tests/ 포함 시 오탐 syntaxError가 발생 — compile
database 기반 검사를 도입하면 스코프 확대 검토).

## 컨벤션

1. **파일 1개 = 테스트 대상 1개.** `tests/test_<대상>.cpp`.
2. **이름은 동작 문장.** `<대상>_<조건>_<기대>` —
   `Push_BeyondCapacity_DropsOldest`. 테스트 실패 메시지가 그대로 버그
   보고서가 되게 한다.
3. **AAA 구조** (준비-실행-검증) 유지, 공용 픽스처 남발 금지 — 중복보다
   명시가 우선.
4. **시간/sleep 의존 테스트 금지.** 페이싱·타임아웃 로직은 주입 가능한
   클럭으로 설계하고 결정론적으로 검증한다.
5. **플랫키 테스트는 결함이다.** 3회 중 1회 실패하는 테스트는 skip하지
   말고 원인을 고치거나 삭제한다. 재시도(retry)로 그린을 만드는 것 금지.
6. **GPU 의존 테스트는 CI에서 skip.** D3D11 디바이스가 필요한 검증은 L3
   수동 절차로 문서화하거나, 추후 WARP(소프트웨어 래스터라이저) 기반으로
   별도 활성화한다 (M5 검토).
7. **실제 값 모양을 테스트한다.** 프로덕션이 만드는 값(예: AVFrame의 enum
   값 그대로)을 입력으로 쓴다. 테스트 편의용으로 재단한 입력은 버그를
   영원히 가린다 (QKeyboard #78 교훈 계승).

## 로드맵

| 시점 | 계획 |
|---|---|
| M2 | `Channel` 상태 머신 단위 테스트 (소스 인터페이스 모킹) |
| M3 | RTSP 옵션 조립 로직 단위 테스트, 재접속 백오프 결정론 테스트 |
| M4 | 레이아웃 프리셋 직렬화(저장/복원) 왕복 테스트 |
| M5 | 소형 미디어 fixture 커밋(≤200KB) 후 L2 통합 테스트 활성화, WARP 기반 셰이더 스모크 검토 |
