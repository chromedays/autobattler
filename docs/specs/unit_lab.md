# 유닛 실험실 스펙 (Unit Lab)

상태: 구현됨 (2026-11-05). Windows 빌드와 그 스크립트 검사는 아직 하지 않았다("단계" 4). 유닛 값의 키가 위키 `unit_stats.csv`의 열이 되었다(2026-11-06, `unit_fields.md`).

### 목표

유닛 **하나**가 어떻게 움직이고 싸우는지 가까이에서 보고, 그 자리에서 값을 고쳐 다시 보는 씬이다. 전투(`battle.md`)는 군대끼리의
결과를 보여 주지만, 유닛 하나의 사거리, 조준, 명중, 처치 시간을 읽기는 어렵다. 실험실은 그것을 숫자로 보여 준다.

- 고른 유닛 정의의 유닛 하나를 세운다. 분대(`squad_size` > 1)여도 분대의 유닛 **한 기**만 세운다.
- 그 앞에 **임시 표적** 둘을 세운다: 지상 표적과 공중 표적. 유닛이 칠 수 있는 레이어와 상관없이 언제나 둘 다 있다(칠 수 없는 표적은
  노리지 않고, 포탄은 그 표적을 지나간다는 것도 눈으로 본다).
- 유닛은 **전투와 같은 규칙**(`src/battle.c`)으로 움직인다. 실험실 전용 시뮬레이션은 없다. 실험실에서 본 것이 전투에서 일어나는
  일이다.
- 패널에서 유닛의 값을 고치고(`.abproj`에 다시 쓴다), 표적과 실험 조건을 바꾸고, 측정값(발사, 명중, 피해, DPS, 처치 시간)을 읽는다.

범위는 지금의 형식이 담는 유닛이다(지금은 `Crawler` 하나). 위키의 유닛 10종(U01–U10)과 그 필드(근접, 유도탄, 범위 피해, 곡사)는 이
스펙 밖이다("나중에" 절). 그것들이 형식에 들어오면 실험실이 그 시험장이 된다.

### 용어

- **유닛:** 실험하는 유닛 하나. 플레이어 편(`TEAM_PLAYER`)이다.
- **표적:** 실험실이 세우는 더미 유닛 둘. 적 편(`TEAM_ENEMY`)이다. 체력과 방어력이 있어 맞으면 깎이고 죽는다. 움직이지 않거나(기본)
  좌우로 오가고, 노리지도 쏘지도 않는다. 죽으면 다시 생긴다.
- **실험:** 유닛과 표적을 처음 자리에 세운 때부터 지금까지. Restart 버튼, 다른 유닛 고르기, 그리고 Restart on change가 켜져 있을
  때의 바꾸기가 실험을 처음부터 다시 시작한다("바꾸기와 다시 시작" 절).

### 화면과 들어가는 법

- Battle 패널의 언어 콤보 아래에 **모드 버튼 둘**: `Battle`과 `Unit Lab`(고른 것이 밝다). 정의를 읽지 못했으면 `Unit Lab`은 꺼진다.
- `Unit Lab`은 전투가 **배치 단계일 때만** 누를 수 있다. 전투 중이나 결과 단계에서는 꺼지고 "Retry로 배치 단계로 돌아가면 쓸 수 있다"는
  줄을 보인다. 실험실에 있는 동안 전투는 배치 단계에 머문다(틱이 돌지 않는다). `Battle`로 돌아가면 플레이어 배치가 그대로 있다(실험실에서
  고친 값 때문에 놓일 수 없게 된 분대는 규칙 고치기와 같은 규칙으로 빠진다; `abproj.md`의 "규칙 고치기").
- 뷰포트와 패널의 자리, 카메라 조작(드래그, 회전, 확대)은 전투와 같다. 실험실에서는 탭이 아무것도 놓지 않는다.
- 실험실에 들어가면 카메라가 유닛과 표적의 가운데를 보도록 다시 맞춘다(피치 50°, 거리는 유닛과 표적 사이 거리의 2배, 최소 40 m; 공중 표적까지 보인다).
  `Battle`로 돌아가면 전투의 처음 카메라로 간다.

### 배치 (실험실의 전장)

전장은 프로젝트의 규칙(`rules`) 그대로다(칸 크기, 격자). 구역 평면과 배치 격자는 그리지 않는다.

- **유닛:** 전장 너비의 가운데, 전장 길이의 1/4(`z = field_length × 0.25`)에 선다. +Z를 본다. 공중 유닛이면 자기 `altitude`에 뜬다.
- **표적:** 유닛 앞 `distance` m(z = 유닛의 z + `distance`)에 선다. 지상 표적은 가운데에서 −X 쪽으로 3 m, 공중 표적은 +X 쪽으로
  3 m(`LAB_TARGET_GAP` 6 m의 절반씩). 공중 표적은 `altitude` 높이에 뜬다.
- `distance`의 범위는 1 m부터 전장 끝까지(유닛의 z에서 전장 길이 − 2 m까지)다. 지금의 전장(96 m 길이)에서 약 70 m다.
- 두 표적이 유닛에게서 같은 거리이면 전투 규칙 그대로 슬롯 번호가 작은 지상 표적을 먼저 노린다.

### 실험 조건 (패널에서 바꾼다, 저장하지 않는다)

| 설정 | 기본 | 범위 | 뜻 |
|---|---|---|---|
| 유닛 | 첫 유닛 정의 | 프로젝트의 유닛 정의 | 실험할 유닛 정의. 분대면 "분대 6기 중 1기로 실험" 줄을 보인다 |
| Walk into range | 켬 | 켬/끔 | 켜면 전투처럼 대상으로 걸어가 사거리의 `stop_fraction` 안에서 멈춘다. 끄면 제자리에 서서 돌기만 하고, 사거리 안의 대상만 쏜다 |
| Distance (m) | 무기 사거리 × 1.5 | 위의 범위 | 유닛에서 표적까지. 유닛을 바꾸면 그 유닛의 기본으로 돌아간다 |
| Targets move | 끔 | 켬/끔 | 켜면 표적 둘이 X축으로 좌우로 오간다 |
| Move speed (m/s) | 4 | 0–30 | 표적이 오가는 속도 |
| Move width (m) | 16 | 0–40 | 표적이 오가는 폭(처음 자리에서 양쪽으로 절반씩). 전장 가장자리에서 잘린다 |
| Target health | 300 | 1–100000 | 두 표적의 체력 |
| Target armor | 0 | 0–1000 | 두 표적의 방어력 |
| Target radius / height (m) | 1 / 2 | 0.1–5 / 0.1–10 | 두 표적의 원기둥(피격 판정과 그림) |
| Air altitude (m) | 10 | 1–50 | 공중 표적이 뜨는 높이 |
| Respawn delay (s) | 1 | 0–10 | 표적이 죽고 다시 생길 때까지 |
| Restart on change | 끔 | 켬/끔 | 켜면 바꿀 때마다 실험이 처음부터 다시 시작한다("바꾸기와 다시 시작" 절) |

- 설정은 실행하는 동안 `Game`에 남고(모드를 오가도 그대로), 파일에 저장하지 않는다. 표적은 게임 데이터가 아니라 시험 도구다.

### 바꾸기와 다시 시작

**바꾸기**는 위 표의 실험 조건(유닛 고르기와 Restart on change 자신은 빼고), 유닛 값 고치기, 규칙 고치기다.

- **Restart on change 끔(기본): 바꾼 것이 지금 실험에 바로 걸린다.** 실험과 측정값은 이어진다. 보면서 Targets move를 켜고 끄거나
  거리를 끌어 바꿀 수 있다. 측정값은 여러 조건에 걸칠 수 있으므로, 패널은 지난 Reset results 뒤에 바꾼 횟수를 "Results span 3
  changes"로 보인다. 기대값은 언제나 지금의 조건으로 계산한다.
- **Restart on change 켬: 바꿀 때마다 실험이 처음부터 다시 시작하고 측정값을 지운다.** 측정값이 언제나 한 조건의 것이다.
- **언제나 다시 시작하는 것:** Restart 버튼과 다른 유닛 고르기(다른 유닛이므로).

바로 걸 때 이미 있는 것은 이렇게 된다:

| 바꾼 것 | 지금 실험에서 |
|---|---|
| 체력의 최대(유닛의 `hp`, Target health) | 살아 있는 것은 지금 체력을 그대로 두되 새 최대를 넘지 않게 자른다. 다시 생기는 표적은 새 값으로 찬다 |
| 유닛의 `shield_hp` | 남은 실드를 새 값에서 자른다 |
| Distance, Move width, Air altitude, 표적과 유닛의 `radius_m`, `height_m`, `altitude_m` | 표적의 처음 자리가 새 자리로 옮겨지고 살아 있는 표적은 그 자리로 간다(오가기는 처음 자리에서 +X로 다시). 유닛은 제자리에 있고 공중이면 새 고도로 간다 |
| Walk into range | 유닛의 `UNIT_HOLD`를 켜거나 끈다 |
| 그 밖(공격의 값, 속도, 방어력, 규칙, Move speed, Respawn delay) | 다음 틱부터 새 값으로 돈다. 날고 있는 포탄은 쏠 때의 값 그대로다 |

**되감기를 위한 기록:** 바로 건 바꾸기는 그 틱과 바꾼 뒤의 실험실 정의 사본과 설정을 기록에 남긴다(`Lab.changes`, 최대 4096개: 위젯을 끌면 프레임마다 하나가 생기므로, 1분 넘게 끌어야 찬다). `lab_seek`는
처음부터 다시 돌리면서 기록된 틱마다 같은 바꾸기를 같은 방식으로 건다. 그래서 바꾸기가 섞인 실험도 되감기가 정확하다. 기록이 차면
더 바꿀 수는 있지만 기록하지 않고, 한 틱 뒤로 버튼이 꺼지며 "Too many changes to step back; Restart to step back again"을 보인다. Restart와
다시 시작하는 바꾸기가 기록을 비운다.

### 규칙 (전투 규칙과 다른 것만)

실험실은 `Battle` 하나를 따로 갖고 `battle_tick`으로 돈다. 그 `Battle`이 읽는 정의는 실험실이 만든 **작은 `BattleDefs` 사본**이다:

- `rules`: 프로젝트의 규칙 그대로.
- `units[0]`: 고른 유닛 정의의 사본. `units[1]`: 지상 표적, `units[2]`: 공중 표적(실험 조건의 체력, 방어력, 반지름, 높이, 고도; 속도 0,
  능력 없음; 무기는 쓰지 않는다). `unit_count` 3.
- `seed`: 프로젝트 스테이지의 `seed`. 적 배치는 없다.

`battle.c`에 더하는 것(전투의 라운드는 바뀌지 않는다; "테스트"의 황금 해시):

- **`UNIT_DUMMY` 플래그:** 표적. 대상을 고르지 않고(언제나 0), 쏘지 않는다. 이동 단계에서 자기 `velocity`를 그대로 갖고(0으로 지우지
  않는다) 그만큼 움직인다. 속도는 틱 전에 실험실이 정한다. 분리(밀어내기)와 전장 가장자리 규칙은 다른 유닛과 같다. 리드 조준은 이
  `velocity`를 읽으므로 움직이는 표적에 리드 조준이 그대로 걸린다.
- **`UNIT_HOLD` 플래그:** Walk into range를 끈 유닛. 움직이지 않고(`velocity` 0) 대상 쪽으로 돌기만 한다. 사거리 검사와 발사는 그대로다.
- **`Battle.endless`:** 끝 검사(`check_end`)를 하지 않는다. 한쪽이 전멸해도, `round_time`이 지나도 실험은 이어진다.
- **`battle_add_unit(battle, def, team, position, flags)`:** 분대 배치를 거치지 않고 유닛 하나를 그 자리에 세운다(체력, 실드 에너지는
  정의대로; 플레이어는 +Z를, 적은 −Z를 본다). 실험실이 쓴다.

실험실의 틱(`lab_tick`, `src/lab.c`):

1. **표적의 속도:** Targets move가 켜져 있으면 살아 있는 표적마다 X 방향으로 `move_speed`. 처음 자리에서 폭의 절반을 넘었거나 전장
   가장자리에 닿았으면 방향을 바꾼다. 꺼져 있으면 0.
2. `battle_tick` 한 번.
3. **측정:** 이번 틱에 더해진 이벤트를 읽어 측정값에 더한다("측정값" 절). 이벤트 목록은 비우지 않는다(뷰가 이펙트로 쓰고 비운다).
4. **다시 생기기:** 죽은 표적은 `respawn_delay`(틱으로 올림, 0이면 다음 틱) 뒤에 처음 자리에서 체력을 채워 다시 산다(같은 슬롯; 움직임은
   +X 방향부터). 유닛은 대상이 죽으면 다음 틱에 전투 규칙대로 다시 고른다.

`lab.c`는 `battle.c`처럼 GPU, ImGui, 시계를 읽지 않는다. 같은 정의와 설정은 언제나 같은 실험을 낸다.

### 측정값

패널의 Results 절. 표 둘(Measured, Expected)에 표적마다 한 열(지상, 공중). 유닛이 칠 수 없는 표적의 열은 "-"이고, 표 아래에
"The unit's weapon cannot hit the air target." 같은 줄을 보인다. 빗나감은 두 표 사이에 합계로 보인다.

| 값 | 뜻 |
|---|---|
| Shots | 공격 수: 발사(`BATTLE_EVENT_FIRE`)와 근접 타격(`BATTLE_EVENT_STRIKE`). 공격한 순간 유닛의 대상이 그 표적이면 그 표적의 것. 근접 타격 하나는 `hits_or_shots_per_attack`번의 명중이 된다 |
| Hits | 그 표적에 맞은 수(`BATTLE_EVENT_HIT`, 이벤트의 `def`로 표적을 안다) |
| Misses | 합계만: 발사 − 명중 − 날고 있는 포탄 |
| Damage | 그 표적에 준 피해(방어력을 뺀 뒤, 이벤트의 `size`)의 합 |
| Kills | 그 표적이 죽은 수 |
| Kill time, last / mean (s) | 표적이 생긴 틱부터 죽은 틱까지(두 행: 마지막 값과 평균). 첫 처치는 걸어간 시간을 포함한다 |
| DPS | 피해 합 ÷ 유닛에게 대상이 있던 시간(표적이 다시 생기기를 기다린 시간은 빠진다) |

**기대값**(정의에서 계산; 시뮬레이션과 비교하는 기준): 한 발 피해 `max(damage − armor, damage × min_damage_fraction)`, 처치에 필요한
발 수 `ceil(ceil(hp ÷ 한 발 피해) ÷ 공격당 명중)`(공격당 명중은 근접이면 `hits_or_shots_per_attack`, 과도기의 포탄은 1), 기대 DPS
`한 발 피해 × 공격당 명중 ÷ 공격 간격`(간격은 틱으로 올린 값 ÷ 30), 기대 처치 시간 `(발 수 − 1) × 공격 간격 + 비행 시간`(비행 시간은
걸어가면 멈추는 거리에서, 서 있으면 그 자리에서 쏜 마지막 포탄의 것으로, 몸 가운데에서 `BATTLE_INTERIM_LAUNCH_ANGLE`로 쏜다. 근접은 0). Expected 표의 행은 Per hit, Shots to kill,
Kill time (s), DPS다.

Reset results 버튼은 측정값만 지운다(실험은 그대로 이어진다).

### 유닛 값 고치기

패널의 Unit 절. Rules 절(`abproj.md`의 "규칙 고치기")과 같은 방식으로 **프로젝트의 글을 제자리에서** 고친다.

- **고칠 수 있는 키(수와 정수만, `unit_fields.md`의 "키"):** `cost`, `squad_size`, `footprint_w`, `footprint_d`, `radius_m`, `height_m`,
  `altitude_m`, `hp`, `armor`, `shield_hp`, `move_speed`, `range`, `damage`, `attack_interval`, `hits_or_shots_per_attack`, `splash_radius`,
  그리고 투사체의 `initial_speed_mps`, `forward_acceleration_mps2`, `max_speed_mps`, `lifetime_s`, `max_travel_m`, `collision_radius_m`,
  `gravity_scale`, `turn_rate_deg_s`, `guidance_delay_s`, `aim_point_scatter_m`, `launch_spread_deg`, `piercing_count`, `arc_height_m`.
  위젯은 그 유닛에 맞는 키만 보인다(키 표의 `shown`): `altitude_m`은 공중 유닛, 투사체 키는 `Melee`가 아닌 유닛, `turn_rate_deg_s`와
  `guidance_delay_s`는 `GuidedProjectile`, `arc_height_m`은 `ScriptedArc`. 위젯은 파일의 단위로 보인다(각도는 도, 시간은 초). 파일이 정수로
  쓰는 키(`hp`, `armor`, `shield_hp`, `damage` 등)는 정수 위젯이다. 실험이 읽지 않는 `cost`, `squad_size`, `footprint_w`, `footprint_d`(배치의
  값)는 맨 아래 "Battle only" 제목 아래에 둔다(키 표의 `battle_only`). 라운드가 아직 따르지 않는 키(`unit_fields.md`의 "과도기")도 고치고
  저장할 수 있지만 실험은 바뀌지 않는다.
- **이번에 고치지 않는 것:** `name`, 낱말 키(`unit_layer`, `target_layer`, `attack_type`, `trajectory_mode` 등), 불리언, `roles`, 새 유닛 만들기.
  "나중에" 절.
- **글이 원본이다.** `defs_unit_get(text, size, unit, key, &value)`가 값을 읽고 `defs_unit_set(text, size, unit, key, value, out, capacity)`이
  그 키의 줄에서 값만 바꾼 글을 만든다. `unit`은 `unit_id`다. 블록이 하나뿐이라(`unit_fields.md`의 "결정" 3) 블록을 가리키는 인자는 없다.
  값의 쓰기, 들여쓰기, 주석 열은 `defs_rule_set`과 같다(같은 도우미를 쓴다).
- **없는 선택 키:** 파일에 없는 선택 키(`shield_hp`, `max_speed_mps` 등)는 위젯이 기본값을 보이고, 고치면 유닛의 마지막 키 줄 뒤에 그
  들여쓰기로 새 줄 하나를 더한다(주석 없음).
- **읽기가 판단한다:** 새 글 전체를 `defs_read_project`로 읽는다. 좋으면 `game_set_unit_value`가 그것을 놓고(defs와 글, "(changed)" 표시,
  전투의 배치는 규칙 고치기와 같이 남기거나 뺀다), 실험실은 새 값을 받는다("바꾸기와 다시 시작" 절). 나쁘면 아무것도 바꾸지 않고 "Not
  changed: <첫 오류>"를 빨강으로 보인다. 예: `squad_size`를 늘려 분대의 유닛이 겹침, `footprint_d`가 구역보다 큼.
- **언제:** 실험실에 있는 동안 언제나(전투가 배치 단계이므로). 로컬 파일을 읽거나 쓰는 중에는 꺼진다(규칙 고치기와 같다). Rules 절도
  실험실에서 그대로 쓸 수 있고, 바꾸면 유닛 값처럼 실험실이 받는다.
- 저장은 프로젝트 줄의 Save, Save as...다. 실행 취소는 없다.

### 시간

전투와 같은 게임 시각과 재생 막대(`battle.md`의 "재생 막대", "시간 조작")다: Restart, Pause/Play, 한 틱 뒤로, 한 틱 앞으로, 0.5x, 1x, 2x, 4x.
실험은 끝이 없으므로 한 틱 앞으로는 언제나 켜져 있다.

- 타임라인은 0부터 실험이 가 본 가장 먼 틱(`Lab.furthest_tick`)까지다. 앞으로 끌면 지금 상태에서 그 틱까지 돌리고(기록이 차도 된다),
  뒤로 끌면 `lab_seek`가 처음부터 다시 돌린다(기록이 차면 뒤로는 가지 않는다). 되감은 뒤에 바꾸면 그 뒤의 기록과 함께 가장 먼 틱도
  지금 틱으로 돌아온다.

- 한 틱 뒤로는 `lab_seek(lab, tick)`: 실험의 처음 정의와 설정으로 처음부터 그 틱까지 다시 돌리고, 기록된 바꾸기를 그 틱마다 다시 건다
  ("바꾸기와 다시 시작" 절). 다시 돌린 상태와 측정값은 그때와 같다(해시가 같다). 비용은 틱 수에 비례한다(유닛 셋이라 10분 실험도 작다).
  되감기는 기록을 지우지 않는다. 되감은 뒤에 바꾸면 그 틱 뒤의 기록을 버리고 새 바꾸기를 남긴다.
- 실험 시간(초와 틱)은 타임라인 안에 쓴다.

### 그리기

- 유닛은 전투와 같은 메시(그 유닛 정의의 것, 아군 색)다. 사거리를 땅 위의 원(디버그 라인)으로 그리고, Walk into range가 켜져 있으면
  멈추는 거리(사거리 × `stop_fraction`)도 흐린 원으로 그린다. 원의 선분은 약 0.5 m마다 하나다(20–256).
- 표적은 회색 원기둥(반지름과 높이 그대로)이다. 유닛이 칠 수 없는 표적은 더 어둡게 그린다. Targets move가 켜져 있으면 오가는 길을 그
  높이의 선으로 그린다.
- 체력 바는 전투 중처럼 언제나 그린다. 포탄, 궤적, 이펙트(발사, 명중, 죽음)는 전투와 같다.
- 실험실의 메시는 실험실 정의(유닛, 지상 표적, 공중 표적)마다 하나씩, 자기 슬롯에서 만든다(전투의 유닛 메시와 따로).

**치수선.** 길이는 도면의 치수선으로 보인다: 두 끝에 가로 눈금, 가운데 틈에 이름과 값(`|-----Range 20.0 m-----|`, 어두운 바탕, 재는 것의
색). 화면에서 선이 글자보다 짧으면 글자는 선 옆으로 간다(정해진 쪽).

**View 절.** 그리는 것마다 체크 하나(두 줄에 셋씩, 기본은 모두 켬, 저장하지 않음, 실험의 바꾸기가 아니다; `Game.lab_hidden`, `LAB_SHOW_*`):
Range(사거리 원과 그 치수), Stop distance(정지 거리 원과 그 치수), Distance to target, Altitude, Body sizes, Target paths(움직이는 표적의 길).

| 치수 | 어디 | 색 |
|---|---|---|
| Range | 유닛에서 사거리 원까지, 유닛이 보는 방향의 왼쪽으로(쏘는 길을 비켜) | 사거리 원의 노랑 |
| Stop | Walk into range가 켜져 있으면 멈추는 거리의 원까지, 오른쪽으로 | 흐린 노랑 |
| Distance | 유닛에서 지금 대상까지 땅 위로(규칙이 재는 XZ 거리). 걷거나 표적이 움직이면 따라 바뀐다 | 흰색 |
| Altitude | 공중 유닛(공중 표적, 그리고 날면 실험하는 유닛)의 땅에서 몸 아랫면까지. 실험실은 전투의 세로 디버그 라인 대신 이것을 그린다 | 흰색 |
| Height, Width | 몸마다(유닛과 두 표적): 높이는 몸 옆(화면 오른쪽), 폭(지름)은 몸 위쪽을 가로질러. 화면에서 20픽셀이 안 되면 그리지 않는다(가까이 보면 나온다). 글자가 옆으로 가면 높이는 오른쪽, 폭은 위로 | 하늘색 |

치수는 3D 씬이 아니라 ImGui의 배경 그리기 목록에 2D로 그린다: 끝점을 카메라로 화면에 투영하고, 카메라를 그 프레임의 자리에 놓은 뒤
(`view_update`), 유닛이 그려지는 자리(틱 사이의 보간)에서 그린다. 그래서 씬 위에 늘 보이고(유닛에 가려지지 않는다), 화면에서 크기가 일정하고,
패널 뒤에 그려진다.

### 패널

실험실 모드에서 위에서 아래로(전투처럼 자주 쓰는 것이 위; `battle.md`의 "Battle 패널"): 단계 대신 "Unit Lab", 모드 버튼, 유닛
고르기(이름만의 버튼과 레이어 줄; 분대 수와 비용은 배치의 정보라 버튼에 쓰지 않는다; 역할 필터는 전투와 같이 쓴다), 분대 줄("A squad of
6: the lab tries one of them."), Restart on change 체크, Lab 절(실험 조건, 펼침), View 절(그리는 것의 체크, 펼침), Unit 절(유닛 값 고치기,
접힘), Results 절(측정값, 빗나감, 기대값, 바꾼 횟수, Reset results; 펼침), 프로젝트 줄(Open..., Reload, Save, Save as...), Rules 절(접힌 채
시작; 실험실에서 싸움을 바꾸는 넷만: Gravity, Retarget interval, Stop fraction, Min. damage fraction. 구역이 없고 라운드가 끝나지 않으며
전장 크기는 표적 거리의 한계일 뿐이라, Cell size, Grid, Zone rows, Round time은 보이지 않는다), 언어 콤보, UI scale 콤보(`battle.md`의
"실행 파일"), 짧은 도움말. Restart, 시간 버튼과 실험 시간은 재생 막대에 있다("시간" 절). 패널은 길어서 세로로 스크롤한다. 모든 글자는
`T()`/`TL()`을 거치고 한국어 행을 `src/strings.c`에 더한다. Unit 절의 이름표는 키 표(`lab_view.c`의 `unit_value_keys`, `.label =`)에 있고
보일 때 `T()`를 거친다.

### 코드 위치

| 파일 | 내용 |
|---|---|
| `src/battle.c`, `battle.h` | `UNIT_DUMMY`, `UNIT_HOLD`, `Battle.endless`, `battle_add_unit` |
| `src/lab.c`, `lab.h` (새 파일) | `LabSettings`, `LabResults`, `Lab`(실험실의 `BattleDefs` 사본, `Battle`, 바꾸기 기록 `changes`), `lab_default_settings`, `lab_start`, `lab_change`(바로 걸거나 다시 시작), `lab_reset_results`, `lab_tick`, `lab_seek`, `lab_misses`, `lab_expected`. GPU, ImGui, 시계 없음 |
| `src/defs.c` | `defs_unit_get`, `defs_unit_set` |
| `src/main.c` | `game_set_mode`, `game_set_unit_value`, `game_lab_settings_changed`, `game_lab_choose_unit`, `game_shown_battle`; 프레임 루프가 모드에 따라 전투나 실험실을 돌린다; 디버그 내보내기 |
| `src/lab_view.c` (새 파일) | 실험실의 카메라(`lab_view_start`), 메시(`lab_view_draw`), 원과 길과 치수선(`lab_view_overlay`), 패널의 실험실 절들(`lab_view_panel`), Unit 절의 키 표 |
| `src/battle_view.c` | 보이는 전투(`game_shown_battle`)를 그린다: 실험실에서는 실험실의 메시, 표적의 회색, 구역을 감춤. 유닛 버튼, 시간 버튼, 원, 유닛 메시를 실험실과 같이 쓴다 |
| `src/game.h` | `GameMode`, `UnitValueKey`, `Lab lab`, `lab_settings`, 실험실의 메시와 재질 |

`Lab`은 `Battle`을 하나 더 가지므로 `Game`이 약 0.8 MB 커진다(영구 아레나).

### 디버그 내보내기 (Debug 빌드)

Windows의 스크립트(`engine/debug_script.h`)가 받는 인수는 정수와 실수 몇 가지 모양뿐이라, 값은 정수 백분의 일로 받는다(`_battle_debug(11)`의
속도 × 100과 같다).

- `_battle_debug_lab(n)`: 0 실험실 모드인가, 1 실험 틱, 2 발사, 3 명중, 4 빗나감, 5 지상 표적 처치, 6 공중 표적 처치, 7 피해 합(정수로 내림),
  8 상태 해시, 9 고른 유닛 정의, 10 유닛의 대상(0 없음, 1 지상, 2 공중), 11 기록된 바꾸기 수, 12 지난 Reset results 뒤에 바꾼 횟수, 13 기록이
  찼는가, 14 View 절이 감춘 것(`LAB_SHOW_*`의 마스크).
- `_battle_debug_lab_enter(on)`: 1이면 실험실로, 0이면 전투로. 못 하면(배치 단계가 아니거나 정의가 없으면) 0.
- `_battle_debug_lab_set(setting, value)`: 실험 조건 하나를 패널처럼 바꾼다. `value`는 백분의 일(스위치는 0이나 100). `setting`: 0 유닛 정의,
  1 Walk into range, 2 Distance, 3 Targets move, 4 Move speed, 5 Move width, 6 Target health, 7 Target armor, 8 Target radius, 9 Target
  height, 10 Air altitude, 11 Respawn delay, 12 Restart on change, 13 View 절이 감출 것(`LAB_SHOW_*`의 마스크 × 100; 실험의 바꾸기가 아니다).
- `_battle_debug_lab_action(action)`: 0 Restart, 1 Reset results.
- `_battle_debug_lab_run(ticks)`: 실험을 그만큼 바로 돌린다(뷰의 이펙트와 함께).
- `_battle_debug_set_unit_value(index, value)`: Unit 절처럼 유닛 값 하나를 고친다. `index`는 키 표(`lab_view.c`의 `unit_value_keys`)의
  순서(0 `cost`, 1 `squad_size`, 2 `footprint_w`, 3 `footprint_d`, 4 `radius_m`, 5 `height_m`, 6 `altitude_m`, 7 `hp`, 8 `armor`, 9 `shield_hp`,
  10 `move_speed`, 11 `range`, 12 `damage`, 13 `attack_interval`, 14 `hits_or_shots_per_attack`, 15 `splash_radius`, 16부터 투사체 키), `value`는
  백분의 일. 고쳐졌으면 1.
- Windows에서는 여섯 모두 `src/main.c`의 `battle_debug_calls`에 한 행씩 있다(`lab`, `lab_enter`, `lab_set`, `lab_action`, `lab_run`,
  `set_unit_value`).

### 테스트

`tests/battle_test.c`(테스트 안의 텍스트로만; 저장소 파일의 값은 확인하지 않는다):

- **전투가 그대로:** `battle.c`에 더한 것을 쓰지 않는 라운드의 황금 해시(시드 1–4)가 바뀌지 않는다.
- **분대 중 한 기:** `squad_size 6`인 유닛도 실험실에는 유닛 하나와 표적 둘, 모두 셋이다.
- **제자리(Walk 끔):** 사거리 안의 멈춘 표적에 공격 간격마다 한 발씩 쏘고 모두 맞힌다(퍼짐 0). 처치 시간과 발 수가 기대값과 같다. 사거리
  밖이면 쏘지 않고 움직이지 않는다.
- **걸어가기(Walk 켬):** 사거리 밖의 표적으로 걸어가 사거리 × `stop_fraction`에서 멈춘다.
- **레이어:** 지상만 치는 유닛은 공중 표적을 노리지 않고, 공중 표적과 겹치는 길의 포탄이 그것을 지나간다(공중 표적의 명중 0).
  두 레이어를 치는 유닛은 같은 거리의 두 표적 중 지상 표적을 먼저 노린다.
- **움직이는 표적:** 곧게 움직이는 표적을 리드 조준으로 맞힌다. 표적이 폭 끝에서 방향을 바꾸고 전장 가장자리를 넘지 않는다.
- **다시 생기기:** 죽은 표적이 `respawn_delay` 뒤에 처음 자리에서 체력이 찬 채 살아나고, 유닛이 다시 노린다. 지연 0은 다음 틱.
- **근접:** 타격이 Shots로, 타격마다 `hits_or_shots_per_attack`번이 Hits로 세어진다. 포탄이 없고 빗나감이 0이다. 처치 시간과 발 수가 기대값과 같다.
- **끝이 없음:** `round_time`이 지나도, 표적이 모두 죽어도 실험이 이어진다.
- **결정론과 되감기:** 같은 정의와 설정은 같은 해시. `lab_seek`로 어느 틱으로 돌아가도 그 틱에 기록한 해시와 측정값이 같다.
- **바로 걸기(Restart on change 끔):** 실험 중에 바꿔도 틱과 측정값이 이어진다. 표의 행마다: 최대 체력을 줄이면 지금 체력이 잘리고
  다시 생기는 표적은 새 값, `shield_hp`, 거리를 바꾸면 표적이 새 자리로, Walk를 끄면 그 틱부터 멈춘다, 공격의 값은 다음 발사부터.
  바꾸기가 섞인 실험도 `lab_seek`가 그 틱의 해시와 측정값을 되살린다. 되감은 뒤에 바꾸면 그 뒤의 기록이 버려진다. 기록이 차면
  `lab_seek`가 거절한다.
- **다시 시작(Restart on change 켬):** 바꾸면 틱 0, 측정값과 기록이 빈다. 다른 유닛 고르기와 Restart는 끔이어도 다시 시작한다.
- **유닛 값 고치기:** 모든 수 키를 자기 값으로 다시 쓰면 바이트가 같다, 다른 유닛의 같은 키는 그대로, 정수 키는 정수로, 각도는 도로, 없는
  선택 키를 더하는 줄(빈 줄 앞, 들여쓰기, CRLF), 모르는 유닛과 키, 수가 아닌 키(`name`, 낱말, 불리언)는 거절, 범위 밖 값과 근접 유닛의
  투사체 키는 쓰고 읽기가 거절한다.

Playwright(Release, Debug, 데스크톱과 폰 크기; 손으로 돌리는 검사이고 스크립트는 저장소에 없다): 실험실에 들어가고(Release는 패널의
Unit Lab 버튼을 눌러) 틱이 실시간으로 간다; 발사와 명중이 늘고 지상 표적이 죽었다가 다시 생기며, Crawler는 공중 표적을 노리지도
죽이지도 않는다; Restart on change가 꺼져 있으면 조건을 바꿔도 틱과 측정값이 이어지고, 켜져 있으면 처음부터 다시 시작한다; `damage`를
바꾸면 "(changed)"가 붙고 글에 새 값이 쓰이며, 나쁜 값은 이유와 함께 거절된다; 한 틱 앞으로가 한 틱 가고 한 틱 뒤로가 같은 해시로 돌아간다;
`Battle`로 돌아가면 배치 단계이고, 라운드 중에는 실험실이 열리지 않는다; 패널을 스크롤해 Results와 Unit 절을 보고, 폰 크기에서 패널이
아래로 간다; 콘솔에는 favicon의 404와 거절한 값의 오류 로그 말고는 오류와 경고가 없다. Windows Debug 빌드는 같은 흐름을 `--script`로
돌려 `[d3d12]` 메시지가 없는지 본다(아직 하지 않음).

### 서드파티

필요 없다. 규칙은 `battle.c`, 그리기와 패널은 엔진의 렌더러, 디버그 라인, ImGui로 충분하다.

### 단계

1. **규칙 (끝남):** `battle.c`의 더하기, `lab.c`, 그 테스트. 화면 없음.
2. **값 고치기 (끝남):** `defs_unit_get`, `defs_unit_set`, `game_set_unit_value`, 그 테스트.
3. **화면 (웹은 끝남):** 모드, `lab_view.c`(그리기, 패널), 한국어 행, 디버그 내보내기(Windows 행 포함), 웹의 Release와 Debug 빌드.
   Windows 빌드는 아직이다.
4. **확인과 문서 (웹은 끝남):** Playwright 검사, `AGENTS.md`(파일 목록, 내보내기, 실험실 규칙)와 `battle.md`에서 이 스펙으로의 연결.
   Windows의 Release와 Debug 빌드와 스크립트 검사가 남았다.

### 나중에 (이 스펙 밖)

- **위키의 유닛(U01–U10):** 유도탄(`PredictiveInterception`), 범위 피해, 곡사(`ScriptedArc`), `target_leading` 끄기를 라운드가 따르게
  되면(`unit_fields.md`의 "과도기") 실험실에 그 조건과 측정값(범위 피해에 맞은 수, 유도탄의 선회)을 더한다.
- **낱말 값 고치기:** `unit_layer`(공중으로 바꾸면 `altitude_m`이 같이 필요하다), `target_layer`, `attack_type`(투사체 키가 같이 필요하다),
  불리언, `roles`, 새 유닛 만들기.
- **표적 하나씩 켜고 끄기**, 표적마다 다른 체력과 크기, 표적 여럿(범위 피해 시험).
- **긴 사거리:** 사거리가 전장보다 긴 유닛(위키의 U10은 130 m)을 위해 실험실의 전장 크기를 거리에 맞춘다.
- **분대 전체 실험:** 분대의 유닛들이 서로 밀어내며 싸우는 모습.

### 열린 질문

1. **표적 둘의 자리 (디자이너 확인 필요):** 지금은 같은 거리에 좌우 3 m씩 떨어뜨리고, 두 레이어를 치는 유닛은 같은 거리면 전투
   규칙대로 지상 표적을 먼저 노린다. 이 규칙으로 구현하되, 디자이너에게 이것이 맞는지(공중 표적을 조금 더 가깝게 두거나, 노릴 표적을
   고르게 할지) 확인한다.
2. ~~**설정을 바꾸면 다시 시작**~~: 해결됨(2026-11-05). 기본은 바로 걸고, Restart on change 토글(기본 끔)로 다시 시작을 고른다("바꾸기와
   다시 시작" 절).
3. ~~**측정 DPS의 분모**~~: 해결됨(2026-11-05). 유닛에게 대상이 있던 시간(걸어간 시간은 포함).
