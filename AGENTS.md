# imgui-video-player — AI 에이전트 공통 규칙

이 저장소에서 작업하는 모든 AI 에이전트(Claude Code, Copilot coding agent,
ZCode 등)에 적용되는 공통 계약이다. 에이전트별 세션 메모는 각자의 위치에
두고(예: `.agents/`), 이 파일이 교차 에이전트 기준이다. 프로젝트 자체
원칙은 [CLAUDE.md](CLAUDE.md)에 있다.

## 브랜치

### 브랜치 이름

**형식: `<agent-name>/issue-<number>-<short-description>`**

예:
- `claude/issue-8-channel-manager`
- `zcode/issue-12-rtsp-source`
- `copilot/issue-16-layout-presets`

- 반드시 **이슈 번호**를 포함한다 — 어떤 이슈를 다루는 브랜치인지, 두
  에이전트가 같은 일을 하는지 즉시 식별할 수 있어야 한다.
- 설명 부분은 kebab-case (공백/슬래시 금지).
- master에 직접 커밋하지 않는다. 모든 변경은 master 대상 PR로 올린다.
  (예외 없음 — 문서 변경도 동일.)

### 작업 시작 전

1. `git fetch origin` 후 master를 최신으로 (`git checkout master && git pull`)
   — 베이스라인을 최신으로 맞춘다.
2. 원격 PR 상태를 확인한다 (`gh pr list --state all --limit 10`): 다루려는
   이슈가 이미 해결/리뷰/머지 중인지 확인.
3. 같은 이슈의 브랜치가 원격에 이미 있는지 확인:
   ```
   git branch -r | grep issue-<number>
   ```
4. 같은 이슈 브랜치가 이미 있으면 **새로 만들지 않는다.** 해당 브랜치를
   체크아웃하고 커밋을 검토한 뒤 이어서 작업한다 (작업 복제 금지).

### 푸시 전

- `git fetch`로 브랜치가 원격에서 움직이지 않았는지 재확인.
- 푸시가 거부되면 다른 에이전트가 커밋을 추가한 것이다. 병합 후 재검증한다.
  **다른 에이전트의 커밋 위로 force-push하지 않는다.**

### PR

- PR 본문: 이슈 참조(`Closes #N`), 변경 요약, **검증 방법**(실행 명령,
  기대 로그, 수동 절차)을 반드시 적는다. 검증 방법 없는 PR은 리뷰 대상이
  아니다.
- media/render의 로직 변경(큐, 판별기, 변환 수학, 상태 머신 등)은 관련
  단위 테스트 추가/갱신을 같은 PR에 포함한다 (`docs/testing.md`).
  GPU/화면 검증은 수동 절차로 PR 본문에 문서화한다.
- CI(clang-format → cppcheck → build+ctest)가 그린이어야 리뷰를 요청한다.
- 머지는 저장소 소유자가 한다. 자기 PR을 자기가 머지하지 않는다.
- PR은 하나의 관심사만 다룬다. 리뷰 중 발견된 별개 문제는 별도 이슈+
  브랜치로 간다 (현재 체크아웃된 브랜치에 얹지 않는다).
