# 아키텍처 설계 (Architecture)

> imgui-video-player — 다채널 하드웨어 가속 비디오 플레이어
> 대상: 데스크톱 3-OS (Windows → macOS/Linux 순 이식). 모바일은 추후.

## 1. 요구사항

- 하드웨어 가속 디코딩이 **기본** 동작 (실패 시 SW 자동 폴백)
- 여러 비디오 소스 동시 렌더링 (CCTV NVR/뷰어 유형)
- 유연한 UI 구성 (도킹, 프리셋, 사용자 재배치)
- 멀티 모니터 지원
- RTSP / DASH 등 스트리밍 프로토콜 재생
- 파일 재생 (시크/배속) 및 실시간 소스 (지연 최소화, 드롭)

## 2. 기술 결정 및 근거

| 결정 | 선택 | 근거 |
|---|---|---|
| 미디어 엔진 | FFmpeg 직접 사용 | OS별 hwaccel(d3d11va/videotoolbox/vaapi) 직접 제어, RTSP/DASH 데먹서 내장, GPU 텍스처 제로카피, CCTV 계열 프로젝트 표준 |
| 렌더링 | 플랫폼별 네이티브 (D3D11/Metal/GL) + 공통 RHI | 각 OS에서 디코더↔렌더러 제로카피 인터롭 최적 (D3D11VA→SRV, VideoToolbox→MTLTexture). ImGui 3개 백엔드 모두 공식 지원 |
| UI | Dear ImGui **docking 브랜치** | 도킹+멀티뷰포트는 master에 없음. docking은 master의 superset으로 주기 병합됨. 프로덕션 사용 광범위. **태그 핀으로 API 변경 리스크 관리** |
| 플랫폼 | GLFW 3.4 | ImGui 공식 platform 백엔드, 모니터 열거/드래그앤드롭 지원, 멀티뷰포트 지원 |
| 시작 플랫폼 | Windows 우선 | D3D11VA→SRV 경로가 가장 검증됨. 멀티뷰포트도 Windows에서 가장 성숙. 추상화는 처음부터 3-OS 이식 가능하게 설계 |

## 3. 계층 구조

```
┌──────────────────────────────────────────────────┐
│ App     main · 설정(JSON) · 채널 매니저 · 전역 상태    │
├──────────────────────────────────────────────────┤
│ UI      DockSpace · VideoView 위젯 · 패널 ·           │
│         레이아웃 프리셋/영속화 · 멀티뷰포트 제어        │
├────────────────────────┬─────────────────────────┤
│ Render  RHI 추상화      │ Media  IVideoSource      │
│  ├ d3d11 (1차)         │   → Demuxer (스레드)      │
│  ├ metal (M6)          │   → Decoder (hwaccel→SW) │
│  └ gl    (M6)          │   → FrameQueue (최신유지)  │
│  NV12→RGBA 변환 패스    │   → VideoFrame → Present │
├────────────────────────┴─────────────────────────┤
│ Platform   GLFW (창, 모니터, 이벤트, 파일 드롭)        │
└──────────────────────────────────────────────────┘
```

- UI·RHI·Media는 플랫폼 독립 인터페이스로 정의. OS 이식 시 백엔드 구현만 추가.
- 렌더 컨텍스트는 ImGui 백엔드와 **공유** → 변환 결과 텍스처가 곧 `ImTextureID`.

## 4. 미디어 파이프라인 (채널당 인스턴스)

```
VideoSource(URL/파일)
  └─ Demuxer 스레드: avformat_open_input → read_frame → PacketQueue
       └─ Decoder 스레드: avcodec_send/receive
            ├─ hwaccel: AV_PIX_FMT_D3D11 (GPU 프레임 유지)
            └─ 폴백: SW 디코딩
                 └─ FrameQueue: 최신 프레임 유지 ring (소비 지연 시 드롭)
                      └─ 렌더 스레드: GPU 변환/업로드 → VideoTexture → UI 표시
```

### 4.1 소스 추상화 (`IVideoSource`)
- 종류: `file` / `rtsp` / `dash` (URL 스킴으로 자동 판별 + 명시적 지정 가능)
- RTSP 옵션: `rtsp_transport=tcp`(기본, 안정), 타임아웃, **재접속 지수 백오프**, 수신 버퍼 최소화
- 파일: 시크/배속/일시정지 (데코더 시크 지원)

### 4.2 하드웨어 가속 정책
- 플랫폼별 우선순위: Windows `d3d11va` → (M6) macOS `videotoolbox` → Linux `vaapi`
- `av_hwdevice_ctx_create` 실패 또는 코덱 미지원 시 **자동 SW 폴백**, 상태 HUD에 경로 표시
- hw 프레임은 GPU에 유지(제로카피). 변환 실패 시에만 GPU/CPU 복사 경로 사용

### 4.3 실시간 정책 (CCTV)
- FrameQueue는 **최신 프레임 우선**: 렌더가 늦으면 구프레임 드롭 (재생 버퍼링 없음)
- 지연 지표 수집: 디코드 지연, 큐 적체, 네트워크 재접속 횟수 → 채널 HUD 표시
- 스레딩: 채널당 demux+decode 스레드 / GPU 업로드는 단일 렌더 스레드
  (D3D11 immediate context 단일 스레드 제약 — M2에서 업로드 큐로 확장 검토)

## 5. 렌더링 (Windows / D3D11 경로)

- D3D11VA 디코더 출력: `AV_PIX_FMT_D3D11` = `ID3D11Texture2D` 배열 + 슬라이스 인덱스
- 절차:
  1. 슬라이스별 SRV 생성 (Y: `R8_UNORM`, UV: `R8G8_UNORM`)
  2. **NV12→RGBA 픽셀 셰이더 패스** (채널당 RT, BT.601/709 · limited/full range 지원)
  3. RT의 SRV를 `ImTextureID`로 ImGui에 표시
- SW 폴백: `sws_scale` → RGBA 스테이징 → 텍스처 업로드
- RHI 공통 인터페이스: `Device` / `Texture` / `Shader` / `VideoConvertPass`
  - macOS(M6): VideoToolbox `CVPixelBuffer` → `CVMetalTextureCache` → 동일 ConvertPass(MSL)
  - Linux(M6): VAAPI. 1차는 hw→CPU 다운로드 후 GL 업로드(1 copy), 추후 EGL/DMABUF 제로카피

## 6. UI / 레이아웃

- 중앙 **DockSpace** + `VideoView` 위젯 (종횡비 fit/cover, 리터 박싱/크로핑)
- 레이아웃 프리셋: 그리드 1×1~5×5, 1포커스+필름스트립 등 CCTV 스타일
- 영속화: `imgui.ini`(도킹 상태) + `layout.json`(채널↔뷰 매핑, 프리셋, 설정)
- 멀티 모니터:
  1. 멀티뷰포트 — 임의 뷰를 보조 모니터로 드래그 아웃 (ImGui docking 브랜치 기능)
  2. **wall 모드** — 지정 모니터에 전체화면 전용 뷰포트로 그리드 표시 (운영용)
- 패널: 채널 브라우저(추가/편집), 컨트롤 바(파일: 재생/시크/배속), 채널 HUD, 로그, 설정

## 7. 디렉터리 구조

```
imgui-video-player/
├── CMakeLists.txt / CMakePresets.json / vcpkg.json
├── docs/architecture.md
├── src/
│   ├── main.cpp
│   ├── app/       config · channel_manager · app_state
│   ├── ui/        dockspace · video_view · panels/ · layout_presets
│   ├── media/     video_source · demuxer · decoder · frame_queue
│   ├── render/    rhi/ · d3d11/  (→ metal/ · gl/)
│   └── platform/  window · monitors
└── shaders/       hlsl (→ msl/glsl)
```

## 8. 마일스톤

| 단계 | 내용 |
|---|---|
| M0 | 인프라: repo, 빌드 스캐폴드, GLFW+ImGui(docking) 창 |
| M1 | 1채널 파일 재생: D3D11VA 제로카피 + NV12 셰이더 + 컨트롤 |
| M2 | 멀티채널 동시 재생 + 그리드 레이아웃 + 채널 브라우저 |
| M3 | RTSP: 재접속, TCP, 드롭 정책, 상태 HUD |
| M4 | 레이아웃 프리셋/저장 + wall 모드 (멀티 모니터) |
| M5 | DASH, SW 폴백 고도화, 16채널 1080p 벤치 |
| M6 | macOS(Metal) / Linux(GL) 이식 |

이후 (별도 트랙): 오디오, ONVIF/PTZ, 모바일(Android/iOS)

## 9. 리스크 및 완화

| 리스크 | 완화 |
|---|---|
| docking 브랜치 API 변경 | 태그 핀 + 변경 시 마이그레이션 노트 |
| D3D11VA 드라이버/코덱 예외 | SW 자동 폴백 + HUD 표시 |
| FFmpeg DASH 데먹서 한계 (일부 MPD 미지원) | M5에서 검증, 필요 시 대안 평가 |
| 멀티뷰포트 macOS 성숙도 | M6에서 검증 (Windows 우선이라 당장 무관) |
| 16채널+ 성능 | 디코더 세션 관리, 서브샘플링 표시 정책, 업로드 큐 |

## 10. 참고

- ImGui docking/multi-viewport: https://github.com/ocornut/imgui/tree/docking
- FFmpeg HWAccel: https://trac.ffmpeg.org/wiki/HWAccelIntro
- 2026-08 기준 docking 브랜치: master 대비 +1,823 / -12 커밋, 멀티뷰포트는 Windows에서 가장 성숙
