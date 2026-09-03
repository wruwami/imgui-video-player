# imgui-video-player

[![CI](https://github.com/wruwami/imgui-video-player/actions/workflows/ci.yml/badge.svg)](https://github.com/wruwami/imgui-video-player/actions/workflows/ci.yml)

Dear ImGui 기반 **다채널 하드웨어 가속 비디오 플레이어** (CCTV 뷰어 유형).

Multi-channel hardware-accelerated video player built with Dear ImGui, FFmpeg, and platform-native renderers.

## 특징 (Goals)

- **하드웨어 가속 디코딩 기본**: Windows D3D11VA → macOS VideoToolbox → Linux VAAPI, 실패 시 SW 폴백
- **다중 채널 동시 렌더링**: 채널당 독립 디코딩 파이프라인 (실시간 드롭 정책)
- **유연한 UI**: ImGui docking 기반 자유 배치 + CCTV 스타일 그리드 프리셋
- **멀티 모니터**: ImGui 멀티뷰포트 + 전용 wall(전체화면) 모드
- **프로토콜**: 파일 / RTSP / DASH (FFmpeg avformat)

## 기술 스택

| 구분 | 선택 |
|---|---|
| 언어/빌드 | C++20 · CMake ≥ 3.24 · vcpkg |
| UI | Dear ImGui (docking branch, 핀 버전) |
| 플랫폼 | GLFW 3.4 |
| 미디어 | FFmpeg (hwaccel: d3d11va / videotoolbox / vaapi) |
| 렌더러 | 플랫폼별 네이티브: D3D11(Win) / Metal(macOS) / OpenGL(Linux) + 공통 RHI |

## 마일스톤

- [ ] **M0** 인프라: 빌드 스캐폴드, GLFW + ImGui(docking) 창
- [ ] **M1** 1채널 파일 재생 (D3D11VA 제로카피 + NV12→RGBA 셰이더)
- [ ] **M2** 멀티채널 + 그리드 레이아웃 + 채널 브라우저
- [ ] **M3** RTSP (재접속, TCP, 상태 HUD)
- [ ] **M4** 레이아웃 프리셋/저장 + wall 모드(멀티 모니터)
- [ ] **M5** DASH, SW 폴백 고도화, 16채널 벤치
- [ ] **M6** macOS(Metal) / Linux(OpenGL) 이식

설계 문서: [docs/architecture.md](docs/architecture.md) · UI 와이어프레임: [docs/wireframes.md](docs/wireframes.md)
프로젝트 원칙: [CLAUDE.md](CLAUDE.md) · AI 에이전트 규칙: [AGENTS.md](AGENTS.md)

## 빌드 (Windows, 예정)

```sh
cmake --preset windows-default
cmake --build --preset windows-default
```

## 라이선스

[MIT](LICENSE)

CI: clang-format (Google 스타일) → cppcheck 정적 분석 → Windows 빌드, 순차 실행.
