# Auto-battler project file spec (`.abproj`)

- [한국어](#한국어)
- [English](#english)

## 한국어

상태: 구현됨 (2026-10-05). 분대(`squad`, `footprint`)를 더함 (2026-10-11). 유닛 블록이 위키의 `unit_stats.csv` 필드를 따른다(2026-11-06, `unit_fields.md`). 형식은 개발 중이라 버전 1 그대로다. 이 스펙의 변경은 먼저 합의한다.

### 목표

오토배틀러의 디자인 데이터 전부를 **파일 하나**에 담는다. 전에는 두 파일(`units.txt`, `stage.txt`)에 나뉘어 있었고, 코드에
`#define`으로 있던 규칙 상수(격자 크기, 라운드 시간, 중력 등)는 코드 안에 있었다. 이제 모두 이 파일에 있다. 데이터의 가짓수가
적으므로 파일을 나누지 않는다.

이 파일은 나중에 디자이너가 쓰는 에디터가 열고 저장하는 단위이자, 데이터를 별도 리포로 옮길 때 옮기는 단위다(둘 다 이 스펙
밖, "나중에" 절).

### 파일

| 파일 | 패키지 안 경로 | 내용 |
|---|---|---|
| `data/default.abproj` | `/data/default.abproj` | 규칙, 유닛 정의, 스테이지 |

- 확장자는 `.abproj`(auto-battler project). 텍스트이므로 일반 Git에 두고(`.gitattributes`에 LFS 패턴을 더하지 않는다), 코드처럼
  리뷰한다.
- `nv_setup_executable`의 `PRELOAD`는 폴더(`data@/data`) 그대로다.
- 형식은 직접 만든 줄 단위 텍스트다(`battle.md`의 "서드파티 후보"에서 결정). 새 서드파티는 없다.

### 문법

- UTF-8 텍스트, 줄 끝은 LF나 CRLF.
- 한 줄에 문장 하나: 키 하나와 그 값들을 스페이스로 나눈다. 빈 줄은 무시한다.
- `#`부터 줄 끝까지는 주석이다.
- **들여쓰기가 중첩을 정한다.** 블록을 여는 문장(`rules`, `roles`, `unit`, `stage`) 다음에 더 깊게 들여 쓴 줄들이 그
  블록에 속한다. 같은 블록의 줄은 들여쓰기가 같아야 한다. 들여쓰기는 스페이스만 쓴다(탭은 오류).
- 값의 종류:
  - **수:** 10진수, 부호와 소수점 가능(`120`, `0.5`, `-3`). 지수 표기는 없다. 정수 자리에 소수가 오면 오류다.
  - **이름:** `[A-Za-z_][A-Za-z0-9_]*`, 31바이트까지(`U01`).
  - **낱말(열거형):** 키마다 정해진 목록 중 하나이고 대소문자를 가린다(`Ground`, `UnguidedProjectile`).
  - **불리언:** `true` 또는 `false`.
  - **글:** 키 뒤에서 주석 앞까지의 나머지 줄, 안의 공백까지, 1–31바이트(`name Scout Swarm`).
  - **목록:** 쉼표로 나눈 글(`roles Chaff/Screen, Flanker`).
- 각도는 파일에서 도(degree)이고 읽을 때 라디안으로 바꾼다. 거리는 m, 시간은 초(읽을 때 틱으로 바꾼다, "규칙" 절).
- 다음은 오류다: 모르는 키, 블록 밖에 온 블록 안 키, 같은 블록 안에서 두 번 나온 키, 빠진 필수 키, 값 개수가 틀린 줄, 범위를
  벗어난 값, 탭, 같은 `unit_id`의 유닛 두 개, 정해진 자리가 아닌 곳의 최상위 문장.
- **최상위 문장의 순서가 정해져 있다.** 최상위 문장은 들여쓰지 않은 문장(`abproj_version`, `rules`, `unit`, `stage`)이고,
  이 순서로 온다: `abproj_version` → `rules` → `roles`(선택) → `unit` 하나 이상 → `stage`. 순서가 정해져 있으면 한 번 훑어 읽을 수 있고(유닛을
  읽을 때 규칙이, 스테이지를 읽을 때 유닛이 이미 있다), 에디터가 쓰는 파일도 모양이 하나로 정해진다. 자리가 아닌 곳의 문장은
  오류 하나로 알리고 그 블록은 건너뛴다(두 블록을 바꿔 쓴 실수가 오류 하나가 되고, 거기에 기대는 것들의 오류가 줄줄이 따라오지
  않는다).

### 구조

| 최상위 문장 | 개수 | 내용 |
|---|---|---|
| `abproj_version <정수>` | 정확히 하나, 첫 문장 | 형식 버전("버전" 절). 지금은 `1` |
| `rules` | 블록, 정확히 하나 | 규칙 상수 |
| `roles` | 블록, 선택, 하나까지 | 유닛이 가질 수 있는 역할의 이름(사람이 분류하려고 쓴다) |
| `unit <unit_id>` | 블록, 하나 이상, 최대 16 | 유닛 정의(위키 `unit_stats.csv`의 한 행) |
| `stage` | 블록, 정확히 하나 | 공급, 시드, 적 배치 |

#### `rules`

모든 키가 필수다. 기본값을 코드에 두지 않으므로 규칙의 값은 이 파일에만 있다.

| 키 | 값 | 범위 | 값 (전에 코드의 상수였던 것) |
|---|---|---|---|
| `cell_size` | 수, m | 0 초과 10 이하 | 2 (`BATTLE_CELL_SIZE`) |
| `grid` | 정수 둘: 너비(칸), 길이(행) | 너비 1–64, 길이 2–128 | 32 48 (`BATTLE_GRID_WIDTH`, `_LENGTH`) |
| `zone_rows` | 정수, 행 | 1–32, 길이의 절반 이하 | 14 (`BATTLE_ZONE_ROWS`) |
| `round_time` | 수, 초 | 0 초과 600 이하 | 60 (`BATTLE_MAX_TICKS` / 30) |
| `gravity` | 수, m/s² | 0 초과 100 이하 | 9.8 (`BATTLE_GRAVITY`) |
| `retarget_interval` | 수, 초 | 0 초과 10 이하 | 0.25 (`BATTLE_RETARGET_TICKS` 8) |
| `stop_fraction` | 수 | 0 초과 1 이하 | 0.9 (`BATTLE_STOP_FRACTION`) |
| `min_damage_fraction` | 수 | 0–1 | 0.25 (`BATTLE_MIN_DAMAGE_FRACTION`) |

- 플레이어 구역은 행 `0`부터 `zone_rows - 1`, 적 구역은 `길이 - zone_rows`부터 `길이 - 1`이다. 두 구역은 겹치지 않는다.
- 시간은 읽을 때 틱으로 바꾼다. 쿨다운과 같은 규칙이다(초 × 30, 올림, `battle_seconds_to_ticks`). `round_time 60`은 1800틱,
  `retarget_interval 0.25`는 8틱(7.5의 올림)이다. 언제나 1틱 이상이다(0틱이 나오면 1틱으로 올린다).
- 규칙 키는 하나의 의미만 가진다. `stop_fraction`은 "목표가 사거리의 이 비율 안에 들면 멈춘다", `min_damage_fraction`은 "방어력이
  한 발에서 깎는 피해는 이 비율 이상을 남긴다"(`max(피해 - 방어력, 피해 × min_damage_fraction)`)다.

**코드에 남는 것과 그 이유:**
- **틱 속도(30 Hz, `BATTLE_TICK_RATE`):** 모든 시간을 틱으로 바꾸는 기준이고, 뷰의 프레임 루프와 속도 조절이 그 위에 있다.
  바꾸면 같은 파일의 결과가 달라지므로(결정론) 데이터가 아니라 엔진의 결정이다.
- **용량:** 배열 크기를 정하는 값(`BATTLE_MAX_UNIT_DEFS`, `BATTLE_MAX_PROJECTILES`, `BATTLE_MAX_EVENTS`, 이름 길이)은 매크로로
  남는다. 격자 크기가 데이터가 되었으므로 그 상한도 매크로다: `BATTLE_MAX_GRID_WIDTH 64`, `BATTLE_MAX_GRID_LENGTH 128`,
  `BATTLE_MAX_ZONE_ROWS 32`, `BATTLE_MAX_PLACES (BATTLE_MAX_ZONE_ROWS * BATTLE_MAX_GRID_WIDTH)`. 위 표의 범위는 이 상한에서
  나온다. 분대의 상한도 매크로다: `BATTLE_MAX_SQUAD_SIZE 64`, `BATTLE_MAX_FOOTPRINT 8`, 그리고 한 편의 유닛 수
  `BATTLE_MAX_TEAM_UNITS 2048`.

#### `roles`

선택. 유닛이 가질 수 있는 **역할 태그**를 선언한다. 사람이 유닛을 분류하려고 다는 것이고, 규칙(`battle.c`)은 읽지 않는다. 그래서 역할은
라운드를 바꾸지 않는다. 한 줄에 역할 하나이고, 이름은 그 줄 전체의 글이다(공백 가능, 쉼표는 안 됨, 31바이트까지: `Chaff Clear`,
`Support(future)`). 최대 32개다. 주석으로 뜻을 적는다. 유닛의 `roles`는 선언한 역할을 쉼표로 나눠 적는다(`roles Chaff/Screen, Flanker`).

| 오류 | 예 |
|---|---|
| 이름에 쉼표, 31바이트 넘음 | `a role's name cannot hold ','`, `a role's name is longer than 31 bytes` |
| 같은 이름을 두 번 선언 | `role 'frontline' is declared twice` |
| 32개를 넘음 | `too many roles (at most 32)` |
| 두 번 나옴, 값이 없음 | `'roles' is given twice`, `'roles' takes no value` |
| 자리가 틀림(`rules` 앞, 유닛 뒤) | `'roles' must come after 'rules' and before the units` |

#### `unit <unit_id>`

유닛 블록은 위키 [유닛 수치](https://github.com/chromedays/autobattler/wiki/Unit‐Stats)의 `unit_stats.csv` 한 행이다
(`docs/specs/unit_fields.md`). 키는 CSV 열 이름 그대로이고 값의 표기도 CSV 그대로다: CSV의 빈칸이 아닌 칸마다 한 줄, 빈칸은 줄이 없다.
블록의 이름은 `unit_id`다. 키마다의 값, 필수, 범위, 기본값과 서로 기대는 검사(위키의 로드 시 검증)는 `unit_fields.md`의 "키"와 "읽을 때의
검사"가 정한다. 여기서는 되풀이하지 않는다. 묶음만 적으면:

| 묶음 | 키 |
|---|---|
| 언제나 필수 | `name`, `cost`, `squad_size`, `unit_layer`, `target_layer`, `footprint_w`, `footprint_d`, `radius_m`, `height_m`, `hp`, `armor`, `move_speed`, `range`, `damage`, `attack_interval`, `hits_or_shots_per_attack`, `splash_radius`, `attack_type` |
| 조건부 | `altitude_m`(`unit_layer Air`면 필수, `Ground`면 오류), `target_priority`(`target_layer Both`일 때만) |
| 선택 | `roles`, `shield_hp` |
| 투사체(`attack_type`이 `Melee`면 오류) | 필수: `initial_speed_mps`, `forward_acceleration_mps2`, `lifetime_s`, `max_travel_m`, `collision_radius_m`, `trajectory_mode`, `target_leading`. 선택: `max_speed_mps`, `gravity_scale`, `aim_point_scatter_m`, `launch_spread_deg`, `piercing_count`, `interceptable`, `retargeting_allowed`. `arc_height_m`은 `ScriptedArc`면 필수, `Straight`면 오류 |
| 유도 | `GuidedProjectile`이면 필수: `turn_rate_deg_s`, `guidance_delay_s`, `guidance_mode`. 선택: `target_loss_behavior` |

- 유닛 정의는 **분대**로 놓인다(`battle.md`의 "분대"): 한 번 놓으면 `footprint_w` × `footprint_d` 칸(+X로 w, 행 쪽으로 d)에 `squad_size`개의
  유닛이 서고, 값은 `cost`다. 나머지 키는 분대의 유닛 하나하나의 것이다.
- `footprint_w`가 격자 너비보다 크면 그 줄에 `'footprint_w' must be at most 32 (the grid's width)`, `footprint_d`가 구역의 행 수보다 크면
  `'footprint_d' must be at most 14 (a zone's rows)`. 규칙에 오류가 있으면 따지지 않는다.
- 분대의 유닛은 놓일 때 서로 겹치면 안 된다. 격자는 위키의 D-07이다: 칸 수가 w × d인 footprint의 미터 크기를 W, D라 하면 열은
  `ceil(sqrt(squad_size × W / D))`(1–`squad_size`), 행은 유닛 수를 열로 나눈 올림, 간격은 `min(W / 열, D / 행)`(`battle_squad_grid`). 돌리지 않은
  방향과 W, D를 바꾼 방향 모두에서 간격이 유닛의 폭(`2 × radius_m`)보다 작으면 `squad_size` 줄에 `24 units on 5x2 cells stand 1 m apart when
  turned, under their width (1.1 m)`. 맞닿는 것(간격 = 폭)은 된다. 하나뿐인 분대도 같다(footprint의 짧은 변이 폭 이상). 규칙이나 그 유닛에
  다른 오류가 있으면 따지지 않는다(실수 하나가 오류 하나).
- 라운드가 아직 따르지 않는 키(`unit_fields.md`의 "과도기")는 읽고 검사하고 담는다. 읽기는 그 키 이름을 `BattleDefs.unfollowed`에 적고,
  프로젝트를 불러올 때(`game_load_project`) 경고 한 줄이 남는다.

#### 레이어

- 유닛은 **지상**(`unit_layer Ground`)이나 **공중**(`unit_layer Air`)이다. 공중 유닛은 `altitude_m` 높이로 날고 `altitude_m`이 필요하다(`unit 'U06'
  flies (unit_layer Air) and needs 'altitude_m'`). 지상 유닛에 `altitude_m`을 두면 오류다(`'altitude_m' is only for air units`). 공중 유닛의 몸은
  그 높이에서 위로 `height_m`만큼이다.
- `target_layer`는 칠 수 있는 레이어다(`Ground`, `Air`, `Both`). 목표는 이 레이어의 적 중에서 고르고, 포탄은 칠 수 없는 레이어의 유닛을
  **지나간다**. 공중을 칠 때는 공중 적의 몸 가운데를 겨눈다. `target_priority`(`Both`일 때 먼저 고를 레이어)는 읽지만 아직 따르지 않는다.
- 서로 다른 레이어의 유닛은 서로 밀어내지 않는다(공중 유닛은 지상 유닛 위를 지나도 된다).
- 레이어는 라운드를 바꾼다. 역할(`roles`)은 바꾸지 않는다.

#### `stage`

| 키 | 값 | 필수 | 범위 |
|---|---|---|---|
| `supply <정수>` | 양쪽의 공급 | 예, 한 번 | 1–100000 |
| `seed <정수>` | 난수 시드(퍼짐). 같은 배치는 같은 결과 | 아니요, 한 번 | 0–4294967295, 기본 1 |
| `place <unit_id> <칸 x> <칸 행>` | 적 분대 하나. 칸은 footprint에서 원점에 가장 가까운 모서리 칸 | 하나 이상 | 위의 `unit` 중 하나. footprint 전체가 적 구역 안(x 0–(너비 − `footprint_w`), 행은 적 구역의 첫 행부터 (길이 − `footprint_d`)까지). footprint끼리 겹치지 않는다. 비용 합 ≤ `supply`, 유닛 수 합 ≤ 2048 |

`place`의 행은 전장 전체의 행 번호다(`grid 32 48`, `zone_rows 14`이면 34–47). 범위는 `rules`에서 계산한다. `rules`에 오류가 있으면
칸을 따질 격자를 모르므로 `place`의 칸 검사는 하지 않는다(틀린 격자 때문에 맞는 줄이 오류가 되지 않게). 이름과 수의 검사는 한다.

### 예

`data/default.abproj`의 앞부분. 규칙의 격자와 라운드 시간은 위키의 D-17, D-24다. 유닛은 위키의 U01–U10이고, 아래는 그중 근접 유닛 하나와
실드가 있는 투사체 유닛 하나다.

```
# autobattler/data/default.abproj: the auto-battler's rules, units and stage (docs/specs/abproj.md). The units are the wiki's
# unit_stats.csv (docs/specs/unit_fields.md): one line per cell of a row, none for an empty cell.
abproj_version 1

rules
    cell_size 2              # m
    grid 32 68               # cells across, rows long (the wiki's D-17)
    zone_rows 14             # each side's deployment rows
    round_time 90            # s (the wiki's D-24)
    gravity 9.8              # m/s²
    retarget_interval 0.25   # s
    stop_fraction 0.9        # of the range
    min_damage_fraction 0.25 # armor never takes more than 75% of a hit

roles
    Chaff/Screen
    Flanker
    Tank/Anchor
    Support(future)
    # ... the roster's other roles

unit U01
    name Scout Swarm
    cost 50
    squad_size 24
    roles Chaff/Screen, Flanker
    unit_layer Ground
    target_layer Ground
    footprint_w 5
    footprint_d 2
    radius_m 0.5
    height_m 0.8
    hp 45
    armor 0
    move_speed 15
    range 6
    damage 7
    attack_interval 0.7
    hits_or_shots_per_attack 1
    splash_radius 0
    attack_type Melee

unit U04
    name Shield Guard
    cost 150
    squad_size 1
    roles Tank/Anchor, Support(future)
    unit_layer Ground
    target_layer Ground
    footprint_w 3
    footprint_d 3
    radius_m 2.0
    height_m 3.5
    hp 2000
    armor 20
    shield_hp 500
    move_speed 5
    range 12
    damage 50
    attack_interval 1.5
    hits_or_shots_per_attack 1
    splash_radius 0
    attack_type UnguidedProjectile
    initial_speed_mps 80
    forward_acceleration_mps2 0
    lifetime_s 0.5
    max_travel_m 25
    collision_radius_m 0.15
    gravity_scale 0
    aim_point_scatter_m 0
    launch_spread_deg 0
    piercing_count 0
    interceptable false
    trajectory_mode Straight
    target_leading true

# ... U02, U03 and U05 to U10

stage
    supply 1000
    seed 1
    place U01 0 54
    place U01 26 54
    place U01 8 60
    place U02 6 54
    place U02 20 54
    place U04 14 54
    place U03 10 57
    place U10 14 60
```

### 버전

- 첫 문장 `abproj_version <n>`이 형식 버전이다. 없거나 첫 문장이 아니면 오류다
  (`default.abproj:1: the file must start with 'abproj_version <version>'`; 비어 있는 파일은 줄 번호 없이). 첫 문장이 다른 것이면 그
  하나만 알리고 버전이 있는 것처럼 계속 읽는다.
- 형식을 바꿀 때마다(키를 더하거나 빼거나, 필수가 되거나, 의미나 단위가 바뀔 때) 이 절의 표에 바뀐 것을 적는다. 형식이 개발 중인
  동안에는 버전을 올리지 않는다(지금은 1, 표의 1행에 적는다). 형식이 굳으면(별도 리포나 파일을 쓰는 에디터가 생길 때) 그때부터 바꿀 때마다
  코드의 `ABPROJ_VERSION`(`battle.h`)을 올리고 표에 행을 더한다.
- 읽는 쪽은 `1`부터 `ABPROJ_VERSION`까지 읽는다. 옛 버전은 읽을 때 지금 형식으로 바꾼다(표의 "옛 파일 읽기"). 더 높은 버전은
  `default.abproj:1: format 2 is newer than this build reads (1)` 오류 하나이고, 그 뒤는 읽지 않는다(새 형식이 무엇을 뜻하는지
  모르므로).
- `abproj_version`은 1 이상 `ABPROJ_VERSION` 이하의 정수다.

| 버전 | 바뀐 것 | 옛 파일 읽기 |
|---|---|---|
| 1 | 첫 형식. 개발 중이라 버전을 올리지 않고 바뀐 것을 여기 적는다. 유닛의 `squad`, `footprint`(분대; `place`의 칸은 footprint의 모서리). 둘 다 없으면 한 칸에 유닛 하나. `roles` 블록과 유닛의 `roles`(역할 태그), `layer`, `altitude`, 무기의 `targets`(레이어). 유닛 블록이 위키 `unit_stats.csv`의 필드가 됨(2026-11-06, `unit_fields.md`): 키는 CSV 열 이름, 블록은 평평하고(`weapon`, `ability` 블록 없음) 이름은 `unit_id`, 값의 종류에 글, 목록, 불리언을 더함, 역할 이름은 글. 옛 키(`squad`, `footprint`, `health`, `speed`, `layer`, `targets`, `cooldown`, `launch_angle`, `spread`, `muzzle`, 실드 블록)는 없어졌다 | 옛 파일은 읽히지 않는다(개발 중) |

### 읽기와 오류

- `src/defs.c`가 읽는다. 함수 하나다:

  ```c
  b32 defs_read_project(BattleDefs* defs, const char* file_name, const char* text, umm size);
  ```

  메모리의 텍스트를 받으므로 테스트는 문자열로 시험한다. `main.c`는 `/data/default.abproj`를 `nv_file_read`로 임시 아레나에 읽어
  넘긴다. `defs`는 0으로 비운 채 시작한다. GPU와 ImGui를 쓰지 않는다.
- 결과는 고정 용량의 평평한 배열이다: `UnitDef units[16]`, 적 배치 목록, 그리고 `BattleRules rules`. 시간은 틱으로, 각도는 라디안으로
  바뀌어 있다:

  ```c
  typedef struct BattleRules {
      f32 cell_size;                 // meters
      u32 grid_width, grid_length;   // cells, rows
      u32 zone_rows;                 // each side's; the enemy's start at grid_length - zone_rows
      u32 round_ticks, retarget_ticks;
      f32 gravity;                   // m/s²
      f32 stop_fraction, min_damage_fraction;
  } BattleRules;
  ```

- 규칙을 읽는 곳은 `defs->rules`다: `battle.c`(이동, 조준, 포탄, 라운드 끝, 칸 중심 `battle_cell_center(rules, x, row)`, 적 구역의
  첫 행 `battle_enemy_first_row(rules)`), `battle_view.c`(필드 평면, 구역 격자, 칸 고르기, 카메라 목표의 제한, 패널의 시간)와
  `game.h`의 `field_width`, `field_length`. `Battle.placed`는 `[BATTLE_MAX_ZONE_ROWS][BATTLE_MAX_GRID_WIDTH]`이고 쓰는 범위는 규칙의
  크기다. 카메라의 시작 거리는 전장 길이에 비례한다(96 m 길이에서 104 m), 최대 거리와 far 평면도 그에 따라 커진다.
- 수는 `strtod`로 읽는다. 올바르게 반올림하므로 같은 파일은 언제나 같은 값이 된다(결정론).
- 오류마다 파일 이름과 줄 번호를 붙여 `nv_log(NV_LOG_ERROR, "battle", "default.abproj:12: unknown key 'healt'")`로 알린다. 첫
  오류에서 멈추지 않고 파일 끝까지 읽어 오류를 모두 알린다(20개까지). 로그 글은 영어다(`AGENTS.md`). 오류가 하나라도 있으면 읽기는
  실패한다. Battle 패널은 첫 오류와 "Definitions could not be loaded"를 보이고 Start를 막는다. 데이터 오류는 실행 중 일어나는
  실패이므로 assert하지 않는다(코딩 표준).
- 파일 전체의 모자람(`missing 'rules'`, `no units`, `missing 'stage'`)은 다른 오류가 없을 때만 알린다. 앞에서부터 처음 모자란 것
  하나다. 블록의 모자람은 블록의 줄에 알린다(`block 'rules' is missing 'gravity'`, `unit 'A' is missing 'health'`,
  `'stage' has no 'place' lines`). `zone_rows`가 길이의 절반을 넘으면 `zone_rows` 줄에 `'zone_rows' must be at most 24`.
- 파일을 못 읽었거나 오류가 있으면 `main.c`는 뷰가 빈 전장과 오류를 그릴 수 있게 고정된 `BattleRules`(`fallback_rules`)를 `defs.rules`에
  넣는다. 전투는 이 규칙으로 돌지 않는다(`defs_ok`가 거짓이면 `battle_init`을 부르지 않는다).

### 불러오기와 저장

추가(2026-10-05). 패키지에 든 `default.abproj` 말고, 사용자 컴퓨터의 `.abproj` 파일을 열고 저장한다(`docs/specs/local_files.md`).
다시 빌드하지 않고 바꾼 값을 바로 해 본다.

- **패널:** Battle 패널의 언어 아래 "Project: <파일 이름>"과 버튼 넷: **Open...**(대화 상자로 연다), **Reload**(붙잡아 둔 파일을 다시
  읽는다), **Save**(그 파일에 제자리로 쓴다), **Save as...**(새 파일에 쓴다, 대체 경로는 내려받기). 그 아래 마지막 결과("Opened x",
  "Saved x", 오류는 빨강).
- **불러오기(`game_load_project`):** 읽은 글을 임시 `BattleDefs`로 읽는다. 좋으면 그것이 defs가 되고, 전투는 배치 단계로 돌아가고
  (`battle_init`), 뷰는 전장 평면, 유닛 메시, 카메라를 새로 맞춘다(`view_apply_project`). 메시는 자기 슬롯에서 다시 만든다
  (`nv_renderer_replace_mesh`), 그래서 여러 번 불러도 메시 슬롯이 늘지 않는다. 오류가 있으면 지금 프로젝트를 그대로 두고
  "x has N error(s), so the project in use stays. The first: ..."를 보인다. 좋은 프로젝트가 아직 없을 때(시작 때 실패)만 그 오류가
  패널의 오류 표시가 된다.
- **저장은 읽은 글에 패널의 고침만 더한 것이다.** 프로젝트의 글(`Game.project_text`, 256 KB까지)을 그대로 두었다가 쓰므로 주석과
  순서가 그대로다. 패널의 Rules 절은 이 글에서 바뀐 값만 제자리에서 바꾼다("규칙 고치기").
- **제자리 저장은 이 프로젝트의 파일에만:** 오류 있는 파일을 열면 브라우저가 그 파일을 붙잡으므로, 지금 프로젝트의 글을 거기에 쓰지
  않도록 Save를 끈다(`kept_is_project`). Reload는 된다(그 파일을 고치고 다시 읽는다). 바깥에서 고친 파일에는 쓰지 않는다
  (`local_files.md`의 바깥 수정 보호).
- 디자이너의 작업: 리포를 받아 둔 폴더의 `.abproj`를 Open하고, 텍스트 편집기에서 고치고, Reload해서 해 본다. 다시 빌드할 필요가 없다.
- Debug 빌드는 `Module._battle_debug_load(name, text)`, `_battle_debug_project_action(action)`, `_battle_debug_project_message()`와
  `_battle_debug(14)`(놓인 프로젝트 수), `(15)`(Save가 제자리에 쓸 수 있는지), `(16)`(격자 너비), `(17)`(로컬 파일 상태)를 내보낸다.

### 규칙 고치기

추가(2026-10-05). Battle 패널에서 규칙(`rules` 블록의 여덟 키)을 고친다. 텍스트 편집기 없이 값을 바꿔 바로 해 보고, Save로 파일에 쓴다.

- **패널:** 프로젝트 줄 아래 접힌 **Rules** 머리. 펼치면 키마다 위젯 하나: Cell size (m), Grid (across, long)(정수 둘), Zone rows,
  Round time (s), Gravity (m/s²), Retarget interval (s), Stop fraction, Min. damage fraction. 끌거나, 두 번 눌러(또는 Ctrl+클릭)
  값을 친다. 위젯의 범위는 끌기를 돕는 것뿐이고, 판단은 읽기가 한다.
- **글이 원본이다.** 위젯은 프로젝트 글에 적힌 값을 보인다(`defs_rule_get`: 시간은 틱이 아니라 파일의 초). 바꾸면 그 키의 줄에서 값만
  새 값으로 바꾼 글을 만들고(`defs_rule_set`), 그 글 전체를 `defs_read_project`로 읽는다. 다른 줄과 그 줄의 들여쓰기, 주석은 그대로다.
  값이 길어지거나 짧아지면 주석 앞의 공백이 줄거나 늘어 주석이 제 열에 남는다(적어도 한 칸). 값은 정수거나 소수점 아래 넷째 자리까지
  쓰고 끝의 0은 뺀다("9.8", "60"). 지수 표기는 쓰지 않는다(형식에 없다).
- **좋으면 바로 놓인다(`game_set_rule`):** 새 defs와 글이 자리에 들어가고, 플레이어의 배치는 새 규칙에서도 되는 칸에 그대로 남는다
  (footprint가 구역 밖이거나 공급을 넘는 분대는 빠진다). 전장 평면은 새로 맞추고, 카메라는 전장 크기가 바뀔 때만 처음 자리로 간다(`view_apply_rules`).
  유닛 메시는 그대로다. 놓인 프로젝트 수(`project_loads`)는 늘지 않는다.
- **나쁘면 아무것도 바꾸지 않는다:** "Not changed: <첫 오류>"를 빨강으로 보인다. 예: `zone_rows`가 격자 길이의 반을 넘음, 격자를 줄여
  스테이지의 `place`가 적 구역 밖으로 나감, `zone_rows`를 어느 유닛의 `footprint_d`보다 작게 함, `cell_size`를 줄여 분대의 유닛이 겹침. 다음에 받아들여진 고침이 이 알림을 지운다.
- **배치 단계에서만:** 라운드가 시작되면 위젯이 꺼진다("Rules change only in deployment"). 되감기(`battle_seek`)는 라운드를 처음부터 다시
  돌리므로, 규칙이 라운드 중에 바뀌면 그 라운드를 재현할 수 없다. 로컬 파일을 읽거나 쓰는 중에도 꺼진다(저장이 보낸 글과 "저장됨"
  표시가 맞도록).
- **바뀜 표시:** 고친 뒤에는 "Project: x (changed)". Save나 Save as가 끝나면(대체 경로의 내려받기 포함) 지운다. Open과 Reload는 고침을
  버린다(묻지 않는다).
- 실행 취소는 없다(값을 다시 바꾸거나 Reload한다).
- Debug 빌드는 `Module._battle_debug_set_rule(key, a, b)`(`b`는 `grid`의 길이), `_battle_debug_project_text()`와
  `_battle_debug(18)`(저장하지 않은 고침이 있는지), `(19)`(라운드 길이, 틱)를 내보낸다.

### 테스트

`tests/battle_test.c`:
- **형식 검사는 테스트 안의 텍스트로만** 한다. 저장소 파일의 값을 확인하지 않는다(디자이너가 값을 바꿔도 테스트가 깨지지 않게).
  `full_project`는 `default.abproj`의 한 때의 사본이고, 값 확인과 라운드는 이것으로 한다.
- 저장소의 `default.abproj`는 오류 없이 읽히고, 플레이어 구역을 채운 한 라운드가 `round_ticks` 안에 끝나는지만 본다.
- 규칙 고치기: 파일의 값 읽기(초 그대로, 규칙이 아닌 키는 0), 모든 규칙을 자기 값으로 다시 쓰면 바이트가 같다, 길고 짧은 값과 주석 열,
  공백 한 칸 남기기, 주석 없는 줄과 CRLF, 정수 반올림, `grid` 두 값, -0, 범위 밖 값도 쓰고 읽기가 거절한다, 규칙이 아닌 키·값 개수·
  자리 모자람은 거절.
- 규칙 키마다 빠짐과 범위(범위 밖, 값 개수, 정수 아님), 구역 겹침, 버전 줄(없음, 첫 문장이 아님, 0, 정수 아님, 더 높음), 최상위 순서(자리가
  아닌 `unit`, `stage`, `rules`, 두 번째 `stage`), 파일 전체의 모자람, 시간의 틱 변환(0.25 → 8, 60 → 1800, 0.01 → 1).
- 규칙을 바꾼 장면: 다른 격자와 칸 크기(칸 중심, 구역 밖, 전장 가장자리), 짧은 `round_time`에서 그 틱에 끝난다, `retarget_interval`,
  `stop_fraction`, `gravity`(포탄이 어느 중력에서도 목표점에 떨어지고 약하면 오래 난다), `min_damage_fraction` 0과 1.
- 레이어: `unit_layer Air`와 `altitude_m`, 공중인데 `altitude_m` 없음, 지상인데 `altitude_m`, `altitude_m 0`, 모르는 낱말(대소문자), 두 번 나옴, 빈 값, `target_layer`의 낱말(`Both`, 모름, 값 둘, 빈 값).
- 유닛 필드(`unit_fields.md`의 "테스트"): 열마다의 빠짐, 범위, 정수, 기본값, 새 값의 종류(글, 낱말, 불리언), 위키의 로드 시 검증 표의 줄마다 한 장면, 위키의 열 유닛이 읽히고 아직 따르지 않는 키가 적힌다, D-07 격자와 두 방향의 간격, 직사각형 배치, 실드 HP, 근접.
- 역할: `roles` 블록의 오류(이름의 쉼표, 31바이트 넘음, 두 번 선언, 32개 초과, 자리)와 유닛의 `roles` 오류(선언되지 않은 역할, 두 번, 빈 항목, 빈 줄)가 맞는 줄에 나온다. 공백이 든 이름과 쉼표 주위의 공백. 마스크는 선언 순서의 비트다. 역할을 바꿔도 시드 1의 라운드 해시가 같다.
- 분대: `squad_size`, `footprint_w`, `footprint_d`의 범위(0, 상한 초과, 정수 아님, 두 번), 구역보다 깊거나 격자보다 넓은
  footprint와 그렇게 줄이는 규칙 고치기의 거절, 겹치는 분대(맞닿음은 된다, 다른 오류가 있으면 더하지 않는다, `cell_size`를 줄이는 고침의 거절), `place`의 footprint가 적 구역 안(x와 행의 끝), 겹침, 모르는 유닛의 칸, 분대 단위 공급, 적
  유닛 2048 초과.
- **황금 해시:** `full_project`(위키의 유닛으로 바꿨을 때의 `default.abproj` 사본)로 시드 1–4의 라운드를 돌린 틱 수, 결과, 300틱의 해시,
  끝의 해시, 남은 적 수. 유닛이 위키의 것으로 바뀐 때(2026-11-06) 다시 기록했다. 규칙의 계산을 바꿔서 라운드가 달라졌다면 이 값이 바뀌는
  것이 맞고, 그때 일부러 고친다.

### 단계

1. **끝남:** `BattleRules`와 용량 매크로, 규칙을 읽는 코드로 바꾸기(값 그대로, 해시가 같음).
2. **끝남:** `defs_read_project`와 `default.abproj`, 두 텍스트 파일 지우기, 테스트 정리.
3. **끝남:** `battle.md`의 "정의 파일" 절을 이 스펙으로 옮기고 `AGENTS.md`의 `autobattler/` 설명 갱신.

### 나중에 (이 스펙 밖)

- **에디터:** 유닛의 수 키는 유닛 실험실의 Unit 절이 고친다(2026-11-05, `unit_lab.md`의 "유닛 값 고치기"; `defs_unit_set`). 남은 것: 유닛의 낱말 키와 스테이지(배치 포함)도 패널에서 고친다. 규칙처럼 바뀐 값만 제자리에서 바꾼다("규칙 고치기"). 새 문장을 더하는
  곳은 최상위 순서가 정해져 있어 분명하다. 실행 취소도 그때 정한다.
- **별도 리포:** `default.abproj`를 데이터 리포로 옮기고 엔진 리포가 서브모듈로 고정한다. 버전 줄이 둘의 호환을 확인한다.
- **스테이지 여러 개:** `stage`에 이름을 붙이고(`stage <이름>`) 여럿을 허용한다. 스테이지마다 전장 크기가 달라지면 `grid`를 `stage`로
  옮긴다. 버전을 올린다.
- **유닛의 키:** 위키의 열이 더해지거나 바뀌면 같은 이름으로 따라간다(`unit_fields.md`의 "나중에"). 스테이지의 `prop` 같은 키도 같은
  문법으로 더해진다. 모르는 키는 여전히 오류다.
- **이펙트, 유닛 외형:** 지금은 `battle_view.c`의 코드다. 디자이너가 다뤄야 하면 블록으로 더한다.

### 열린 질문

1. ~~**파일 이름**~~: 해결됨(2026-10-05). 확장자는 `.abproj`, 파일은 `default.abproj`. 리포를 나누면 그 리포의 맨 위에 놓인다.
2. ~~**격자 크기의 위치**~~: 해결됨(2026-10-05). `rules`에 둔다. 스테이지마다 전장 크기가 달라지면 그때 `stage`로 옮기고 버전을
   올린다.
3. ~~**최상위 순서**~~: 해결됨(2026-10-05). 고정한다("문법" 절).

## English

Status: built (2026-10-05). Squads (`squad`, `footprint`) added (2026-10-11). The unit block follows the fields of the wiki's `unit_stats.csv` (2026-11-06, `unit_fields.md`). The format stays at version 1 while it is in development. Changes to this spec are agreed first.

### Goal

Keep all of the auto-battler's design data in **one file**. It used to be split over two files (`units.txt`, `stage.txt`), and the
rule constants (grid size, round length, gravity and so on) were `#define`s in code. Now all of it is in this file. There is little
data, so it is not split into files.

This file is what a designer's editor will later open and save, and what moves if the data goes to a repository of its own (both
outside this spec; see "Later").

### File

| File | Path in the package | Contents |
|---|---|---|
| `data/default.abproj` | `/data/default.abproj` | Rules, unit definitions, the stage |

- The extension is `.abproj` (auto-battler project). It is text, so it stays in plain Git (no LFS pattern in `.gitattributes`) and
  is reviewed like code.
- `nv_setup_executable`'s `PRELOAD` keeps the folder (`data@/data`).
- The format is our own line-based text (decided in `battle.md`, "Third-party candidates"). No new third-party code.

### Syntax

- UTF-8 text, LF or CRLF line ends.
- One statement per line: a key and its values, separated by spaces. Blank lines are ignored.
- `#` starts a comment that runs to the end of the line.
- **Indentation sets nesting.** Lines indented deeper after a statement that opens a block (`rules`, `roles`, `unit`, `stage`)
  belong to that block. Lines of one block have the same indentation. Indentation is spaces only (a tab is an error).
- Kinds of values:
  - **Number:** decimal, with an optional sign and point (`120`, `0.5`, `-3`). No exponents. A fraction where an integer goes is
    an error.
  - **Name:** `[A-Za-z_][A-Za-z0-9_]*`, up to 31 bytes (`U01`).
  - **Word (enum):** one of a list fixed per key, case counting (`Ground`, `UnguidedProjectile`).
  - **Boolean:** `true` or `false`.
  - **Text:** the rest of the line after the key up to a comment, spaces and all, 1 to 31 bytes (`name Scout Swarm`).
  - **List:** texts separated by commas (`roles Chaff/Screen, Flanker`).
- Angles are degrees in the file and become radians when read. Distances are meters, times seconds (they become ticks when read; see
  "Rules").
- These are errors: an unknown key, a block's key outside its block, a key twice in one block, a missing required key, a line with
  the wrong number of values, a value out of range, a tab, two units with the same `unit_id`, a top-level statement out of its place.
- **Top-level statements come in a fixed order.** Top-level statements are the unindented ones (`abproj_version`, `rules`,
  `unit`, `stage`), and they come in this order: `abproj_version` → `rules` → `roles` (optional) → one or more `unit` → `stage`. A fixed order lets one
  pass read the file (the rules are there when units are read, the units when the stage is) and gives the files an editor writes one
  shape. A statement out of its place is one error and its block is skipped (a swap of two blocks is one error, and not the errors of
  everything that depends on them).

### Structure

| Top-level statement | Count | Contents |
|---|---|---|
| `abproj_version <integer>` | Exactly one, the first statement | The format version ("Versions"). Now `1` |
| `rules` | Block, exactly one | Rule constants |
| `roles` | Block, optional, at most one | The names of the roles a unit may carry (for sorting units by people) |
| `unit <unit_id>` | Block, one or more, at most 16 | A unit definition (a row of the wiki's `unit_stats.csv`) |
| `stage` | Block, exactly one | Supply, seed, the enemy deployment |

#### `rules`

Every key is required. No defaults live in code, so the rules' values are only in this file.

| Key | Value | Range | Value (the constant it was in code) |
|---|---|---|---|
| `cell_size` | Number, m | Above 0, at most 10 | 2 (`BATTLE_CELL_SIZE`) |
| `grid` | Two integers: width (cells), length (rows) | Width 1–64, length 2–128 | 32 48 (`BATTLE_GRID_WIDTH`, `_LENGTH`) |
| `zone_rows` | Integer, rows | 1–32, at most half the length | 14 (`BATTLE_ZONE_ROWS`) |
| `round_time` | Number, s | Above 0, at most 600 | 60 (`BATTLE_MAX_TICKS` / 30) |
| `gravity` | Number, m/s² | Above 0, at most 100 | 9.8 (`BATTLE_GRAVITY`) |
| `retarget_interval` | Number, s | Above 0, at most 10 | 0.25 (`BATTLE_RETARGET_TICKS` 8) |
| `stop_fraction` | Number | Above 0, at most 1 | 0.9 (`BATTLE_STOP_FRACTION`) |
| `min_damage_fraction` | Number | 0–1 | 0.25 (`BATTLE_MIN_DAMAGE_FRACTION`) |

- The player's zone is rows `0` to `zone_rows - 1`, the enemy's `length - zone_rows` to `length - 1`. The two never overlap.
- Times become ticks when read, by the same rule as cooldowns (seconds × 30, rounded up, `battle_seconds_to_ticks`). `round_time 60`
  is 1800 ticks and `retarget_interval 0.25` is 8 ticks (7.5 rounded up). It is always at least 1 tick (0 ticks is raised to 1).
- Each rule key means one thing. `stop_fraction` is "stop once the target is within this share of the range";
  `min_damage_fraction` is "armor always leaves at least this share of a hit" (`max(damage - armor, damage × min_damage_fraction)`).

**What stays in code, and why:**
- **The tick rate (30 Hz, `BATTLE_TICK_RATE`):** every time is turned into ticks by it, and the view's frame loop and speed controls
  sit on it. Changing it changes what the same file plays out to (determinism), so it is the engine's decision, not data.
- **Capacities:** the values that size arrays (`BATTLE_MAX_UNIT_DEFS`, `BATTLE_MAX_PROJECTILES`, `BATTLE_MAX_EVENTS`, the name
  length) stay macros. Since the grid's size is data, its upper limits are macros too: `BATTLE_MAX_GRID_WIDTH 64`,
  `BATTLE_MAX_GRID_LENGTH 128`, `BATTLE_MAX_ZONE_ROWS 32`, `BATTLE_MAX_PLACES (BATTLE_MAX_ZONE_ROWS * BATTLE_MAX_GRID_WIDTH)`. The
  ranges in the table above come from them. So do a squad's: `BATTLE_MAX_SQUAD_SIZE 64`, `BATTLE_MAX_FOOTPRINT 8`, and the units a
  side may have, `BATTLE_MAX_TEAM_UNITS 2048`.

#### `roles`

Optional. Declares the **role tags** a unit may carry. People use them to sort units; the rules (`battle.c`) never read them, so a role
never changes a round. One role a line, its name the line's whole text (spaces allowed, no comma, up to 31 bytes: `Chaff Clear`,
`Support(future)`), at most 32, with a comment for what each means. A unit's `roles` lists declared roles with commas between them
(`roles Chaff/Screen, Flanker`).

| Error | Example |
|---|---|
| a comma in a name, over 31 bytes | `a role's name cannot hold ','`, `a role's name is longer than 31 bytes` |
| a name declared twice | `role 'frontline' is declared twice` |
| more than 32 | `too many roles (at most 32)` |
| given twice, or with a value | `'roles' is given twice`, `'roles' takes no value` |
| out of place (before `rules`, after a unit) | `'roles' must come after 'rules' and before the units` |

#### `unit <unit_id>`

A unit block is a row of the wiki's [Unit Stats](https://github.com/chromedays/autobattler/wiki/Unit‐Stats) `unit_stats.csv`
(`docs/specs/unit_fields.md`). The keys are the CSV's column names and the values are written as the CSV writes them: a line for each
cell of the row that is not empty, none for an empty one. The block's name is the `unit_id`. Each key's value, whether it is required,
its range and default, and the checks keys make of each other (the wiki's load-time checks) are `unit_fields.md`'s "키" (Keys) and "읽을 때의
검사" (Checks when reading); they are not repeated here. In groups:

| Group | Keys |
|---|---|
| Always required | `name`, `cost`, `squad_size`, `unit_layer`, `target_layer`, `footprint_w`, `footprint_d`, `radius_m`, `height_m`, `hp`, `armor`, `move_speed`, `range`, `damage`, `attack_interval`, `hits_or_shots_per_attack`, `splash_radius`, `attack_type` |
| Conditional | `altitude_m` (required with `unit_layer Air`, an error with `Ground`), `target_priority` (only with `target_layer Both`) |
| Optional | `roles`, `shield_hp` |
| Projectile (an error when `attack_type` is `Melee`) | Required: `initial_speed_mps`, `forward_acceleration_mps2`, `lifetime_s`, `max_travel_m`, `collision_radius_m`, `trajectory_mode`, `target_leading`. Optional: `max_speed_mps`, `gravity_scale`, `aim_point_scatter_m`, `launch_spread_deg`, `piercing_count`, `interceptable`, `retargeting_allowed`. `arc_height_m` is required with `ScriptedArc`, an error with `Straight` |
| Guidance | Required with `GuidedProjectile`: `turn_rate_deg_s`, `guidance_delay_s`, `guidance_mode`. Optional: `target_loss_behavior` |

- A unit def is placed as a **squad** (`battle.md`, "Squads"): one placement puts `squad_size` units on `footprint_w` × `footprint_d`
  cells (w along +X, d along the rows), for `cost`. The other keys are each unit's own.
- A `footprint_w` wider than the grid is reported at its line, `'footprint_w' must be at most 32 (the grid's width)`; a `footprint_d`
  deeper than a zone, `'footprint_d' must be at most 14 (a zone's rows)`. They are not judged when the rules have an error.
- A squad's units must not overlap where they are placed. The grid is the wiki's D-07: with W and D the footprint's size in meters,
  there are `ceil(sqrt(squad_size × W / D))` columns (1 to `squad_size`) and the units over the columns, rounded up, rows, and they are
  `min(W / columns, D / rows)` apart (`battle_squad_grid`). When that is less than a unit's width (`2 × radius_m`), the footprint as it
  stands or turned (W and D swapped), the `squad_size` line says `24 units on 5x2 cells stand 1 m apart when turned, under their width
  (1.1 m)`. Touching (the gap equal to the width) is fine. A squad of one too (the footprint's shorter side at least its width). It is
  not judged when the rules or that unit have another error (one mistake, one error).
- Keys the round does not follow yet (`unit_fields.md`, "과도기" (Interim)) are read, checked and kept. The read writes their names
  into `BattleDefs.unfollowed`, and loading a project (`game_load_project`) logs one warning of them.

#### Layers

- A unit is **ground** (`unit_layer Ground`) or **air** (`unit_layer Air`). An air unit flies at its `altitude_m`, which it needs (`unit 'U06'
  flies (unit_layer Air) and needs 'altitude_m'`). A ground unit with an `altitude_m` is an error (`'altitude_m' is only for air units`). An
  air unit's body stands from that height up by its `height_m`.
- `target_layer` is the layers a unit can hit (`Ground`, `Air`, `Both`). A target is picked among the enemies of those layers, and a
  shell **passes** the units of other layers. An air target is aimed at the middle of its body. `target_priority` (the layer picked first
  with `Both`) is read but not followed yet.
- Units of different layers do not push each other apart (an air unit may pass over a ground unit).
- A layer changes a round. A role (`roles`) does not.

#### `stage`

| Key | Value | Required | Range |
|---|---|---|---|
| `supply <integer>` | Each side's supply | Yes, once | 1–100000 |
| `seed <integer>` | The random seed (spread); the same deployment gives the same result | No, once | 0–4294967295, default 1 |
| `place <unit_id> <cell x> <cell row>` | One enemy squad. The cell is its footprint's corner nearest the origin | One or more | One of the `unit`s above. The whole footprint in the enemy zone (x 0 to (width − `footprint_w`), the row from the enemy zone's first to (length − `footprint_d`)). Footprints do not overlap. Total cost ≤ `supply`, total units ≤ 2048 |

A `place` row is the row on the whole field (34–47 with `grid 32 48` and `zone_rows 14`). Its range is computed from `rules`. When
`rules` has an error, the grid the cell would be judged against is not known, so `place` has no cell checks (so a wrong grid does not
make right lines errors). Its unit name and numbers are still checked.

### Example

The start of `data/default.abproj`. The rules' grid and round time are the wiki's D-17 and D-24. The units are the wiki's U01 to U10;
below are one melee unit and one projectile unit with a shield.

```
# autobattler/data/default.abproj: the auto-battler's rules, units and stage (docs/specs/abproj.md). The units are the wiki's
# unit_stats.csv (docs/specs/unit_fields.md): one line per cell of a row, none for an empty cell.
abproj_version 1

rules
    cell_size 2              # m
    grid 32 68               # cells across, rows long (the wiki's D-17)
    zone_rows 14             # each side's deployment rows
    round_time 90            # s (the wiki's D-24)
    gravity 9.8              # m/s²
    retarget_interval 0.25   # s
    stop_fraction 0.9        # of the range
    min_damage_fraction 0.25 # armor never takes more than 75% of a hit

roles
    Chaff/Screen
    Flanker
    Tank/Anchor
    Support(future)
    # ... the roster's other roles

unit U01
    name Scout Swarm
    cost 50
    squad_size 24
    roles Chaff/Screen, Flanker
    unit_layer Ground
    target_layer Ground
    footprint_w 5
    footprint_d 2
    radius_m 0.5
    height_m 0.8
    hp 45
    armor 0
    move_speed 15
    range 6
    damage 7
    attack_interval 0.7
    hits_or_shots_per_attack 1
    splash_radius 0
    attack_type Melee

unit U04
    name Shield Guard
    cost 150
    squad_size 1
    roles Tank/Anchor, Support(future)
    unit_layer Ground
    target_layer Ground
    footprint_w 3
    footprint_d 3
    radius_m 2.0
    height_m 3.5
    hp 2000
    armor 20
    shield_hp 500
    move_speed 5
    range 12
    damage 50
    attack_interval 1.5
    hits_or_shots_per_attack 1
    splash_radius 0
    attack_type UnguidedProjectile
    initial_speed_mps 80
    forward_acceleration_mps2 0
    lifetime_s 0.5
    max_travel_m 25
    collision_radius_m 0.15
    gravity_scale 0
    aim_point_scatter_m 0
    launch_spread_deg 0
    piercing_count 0
    interceptable false
    trajectory_mode Straight
    target_leading true

# ... U02, U03 and U05 to U10

stage
    supply 1000
    seed 1
    place U01 0 54
    place U01 26 54
    place U01 8 60
    place U02 6 54
    place U02 20 54
    place U04 14 54
    place U03 10 57
    place U10 14 60
```

### Versions

- The first statement, `abproj_version <n>`, is the format version. A file without it, or with it anywhere but first, is an error
  (`default.abproj:1: the file must start with 'abproj_version <version>'`; an empty file, without a line). If the first statement
  is something else, only that is reported and the read goes on as if there were a version.
- Every change to the format (a key added or removed, made required, or given a new meaning or unit) is written in this section's
  table. While the format is in development the version does not rise: it stays at 1, and the changes are listed in the row for 1. Once
  the format settles (its own repository, or an editor that writes the files), each change raises `ABPROJ_VERSION` (`battle.h`) and adds
  a row.
- The reader reads versions `1` to `ABPROJ_VERSION`. An older version is turned into today's format as it is read (the table's
  "Reading older files"). A newer version is one error, `default.abproj:1: format 2 is newer than this build reads (1)`, and the rest is
  not read (it is not known what the new format means).
- `abproj_version` is an integer from 1 to `ABPROJ_VERSION`.

| Version | What changed | Reading older files |
|---|---|---|
| 1 | The first format. Still in development, so the version is not raised: the changes are listed here. A unit's `squad` and `footprint` (squads; a `place`'s cell is the footprint's corner). Without them a unit is one on one cell. The `roles` block and a unit's `roles` (role tags), `layer`, `altitude` and a weapon's `targets` (layers). The unit block became the fields of the wiki's `unit_stats.csv` (2026-11-06, `unit_fields.md`): the keys are the CSV's column names, the block is flat (no `weapon` or `ability` block) and named by its `unit_id`, text, list and boolean values were added, and role names are text. The old keys (`squad`, `footprint`, `health`, `speed`, `layer`, `targets`, `cooldown`, `launch_angle`, `spread`, `muzzle`, the shield block) are gone | Older files do not read (in development) |

### Reading and errors

- `src/defs.c` reads it, with one function:

  ```c
  b32 defs_read_project(BattleDefs* defs, const char* file_name, const char* text, umm size);
  ```

  It takes text in memory, so tests try strings. `main.c` reads `/data/default.abproj` with `nv_file_read` into a scratch arena and
  hands it over. `defs` starts zeroed. It uses no GPU or ImGui.
- The result is flat arrays of fixed capacity: `UnitDef units[16]`, the enemy deployment list, and a `BattleRules rules`, with times
  already in ticks and angles in radians:

  ```c
  typedef struct BattleRules {
      f32 cell_size;                 // meters
      u32 grid_width, grid_length;   // cells, rows
      u32 zone_rows;                 // each side's; the enemy's start at grid_length - zone_rows
      u32 round_ticks, retarget_ticks;
      f32 gravity;                   // m/s²
      f32 stop_fraction, min_damage_fraction;
  } BattleRules;
  ```

- What reads the rules reads `defs->rules`: `battle.c` (movement, aiming, shells, the end of the round, a cell's center
  `battle_cell_center(rules, x, row)`, the enemy zone's first row `battle_enemy_first_row(rules)`), `battle_view.c` (the field's
  planes, the zone grids, picking a cell, the camera target's limits, the panel's time) and `field_width`, `field_length` in `game.h`.
  `Battle.placed` is `[BATTLE_MAX_ZONE_ROWS][BATTLE_MAX_GRID_WIDTH]`, used up to the rules' size. The camera's starting distance is in
  proportion to the field's length (104 m for a 96 m length), and its largest distance and far plane grow with it.
- Numbers are read with `strtod`. It rounds correctly, so the same file always gives the same values (determinism).
- Each error is reported with the file name and line number through `nv_log(NV_LOG_ERROR, "battle", "default.abproj:12: unknown key
  'healt'")`. Reading does not stop at the first error: it goes to the end of the file and reports them all (up to 20). Log text is
  English (`AGENTS.md`). With any error, reading fails. The Battle panel shows the first error and "Definitions could not be loaded",
  and Start is disabled. A data error is a failure that happens at run time, so it does not assert (the coding standard).
- What the whole file lacks (`missing 'rules'`, `no units`, `missing 'stage'`) is reported only when there is no other error, and only
  the first missing part, in order. What a block lacks is reported at the block's line (`block 'rules' is missing 'gravity'`,
  `unit 'A' is missing 'health'`, `'stage' has no 'place' lines`). A `zone_rows` over half the length is reported at the `zone_rows`
  line: `'zone_rows' must be at most 24`.
- When the file could not be read or has an error, `main.c` puts fixed rules (`fallback_rules`) into `defs.rules` so that the view can
  draw an empty field and the error. The battle does not run on them (`battle_init` is not called when `defs_ok` is false).

### Loading and saving

Added (2026-10-05). Besides the `default.abproj` in the package, a `.abproj` file on the user's computer can be opened and saved
(`docs/specs/local_files.md`), so a changed value can be tried at once, without a rebuild.

- **The panel:** under the language in the Battle panel, "Project: <file name>" and four buttons: **Open...** (a dialog),
  **Reload** (reads the kept file again), **Save** (writes it in place), **Save as...** (writes a new file; in the fallback, a
  download). Under them, the last result ("Opened x", "Saved x", errors in red).
- **Loading (`game_load_project`):** the text is read into a scratch `BattleDefs`. A good one becomes the defs, the battle goes back
  to deployment (`battle_init`), and the view fits the field's planes, the unit meshes and the camera to it (`view_apply_project`).
  The meshes are remade in their own slots (`nv_renderer_replace_mesh`), so loading again and again uses up no mesh slots. With an
  error, the project in use stays and the panel says "x has N error(s), so the project in use stays. The first: ...". Only while no
  good project is in place yet (a failure at start) do its errors become the panel's error display.
- **Saving writes the text as it was read, with only the panel's edits.** The project's text (`Game.project_text`, up to 256 KB) is
  kept and written, so comments and order stay. The panel's Rules section changes only the edited values in it, in place ("Editing
  the rules").
- **In place only to this project's file:** after a file with errors is opened, the browser keeps that file, so Save is disabled
  rather than writing the project in use into it (`kept_is_project`). Reload works (fix that file and read it again). A file changed
  outside is not written (`local_files.md`, the outside-edit guard).
- A designer's work: Open the `.abproj` in the cloned repository, edit it in a text editor, and Reload to try it. No rebuild.
- Debug builds export `Module._battle_debug_load(name, text)`, `_battle_debug_project_action(action)`, `_battle_debug_project_message()`
  and `_battle_debug(14)` (projects put in place), `(15)` (whether Save can write in place), `(16)` (the grid's width), `(17)` (the
  local file's status).

### Editing the rules

Added (2026-10-05). The rules (the eight keys of the `rules` block) are edited in the Battle panel: a value is changed and tried at
once, without a text editor, and Save writes it to the file.

- **The panel:** a closed **Rules** header under the project's row. Opened, it has a widget per key: Cell size (m), Grid (across,
  long) (two integers), Zone rows, Round time (s), Gravity (m/s²), Retarget interval (s), Stop fraction, Min. damage fraction. Drag,
  or double-click (or Ctrl+click) and type. The widgets' ranges only guide a drag; the reader judges.
- **The text is the source.** A widget shows the value the project's text gives (`defs_rule_get`: times in the file's seconds, not
  ticks). A change makes the text with only the values on that key's line replaced (`defs_rule_set`) and reads all of it with
  `defs_read_project`. Every other line, and the line's indentation and comment, stay. When a value gets longer or shorter, the
  spaces before the comment shrink or grow, so the comment keeps its column (one space at the least). A value is written as an
  integer or with at most four decimals, without trailing zeros ("9.8", "60"); never with an exponent, which the format does not have.
- **A good change is put in place at once (`game_set_rule`):** the new defs and text take over, and the player's deployment stays on
  the cells the new rules still allow (squads whose footprint leaves the zone, or over the supply, drop out). The field's planes are fitted again, and
  the camera starts over only when the field's size changed (`view_apply_rules`). The unit meshes stay. It does not count as a
  project put in place (`project_loads`).
- **A bad change changes nothing:** the panel says "Not changed: <the first error>" in red; for example, `zone_rows` over half the
  grid's length, a smaller grid that leaves a stage `place` outside the enemy zone, `zone_rows` under a unit's `footprint_d`, or a smaller `cell_size` that makes
  a squad's units overlap. The next change made clears that message.
- **Only in deployment:** once a round starts the widgets are disabled ("Rules change only in deployment"). Stepping back
  (`battle_seek`) plays the round again from its start, so a rule changed during a round would make that round impossible to
  replay. They are also disabled while a local file is read or written (so the text a save sends matches what it marks as saved).
- **The changed mark:** after an edit, "Project: x (changed)". A finished Save or Save as (the fallback's download too) clears it.
  Open and Reload drop the edits (without asking).
- There is no undo (change the value back, or Reload).
- Debug builds export `Module._battle_debug_set_rule(key, a, b)` (`b` is the `grid`'s length), `_battle_debug_project_text()` and
  `_battle_debug(18)` (whether there are edits not yet saved) and `(19)` (the round's length in ticks).

### Tests

`tests/battle_test.c`:
- **The format is tested only with text inside the test.** No test checks the repository file's values (so a designer changing a
  value breaks no test). `full_project` is a copy of `default.abproj` from one time, and the value checks and rounds run on it.
- The repository's `default.abproj` is only checked to read without errors and to play a round, with the player's zone filled, that
  ends within `round_ticks`.
- Editing the rules: reading the file's values (seconds as they are; 0 for a key that is not a rule), writing every rule's own
  values back gives the same bytes, longer and shorter values and the comment's column, keeping one space, a line without a comment
  and CRLF, integers rounded, `grid`'s two values, -0, a value out of range is written and the reader refuses it, and refusing a key
  that is not a rule, the wrong number of values and too little room.
- Each `rules` key missing and out of range (out of range, value count, not an integer), zones that overlap, the version line (missing,
  not first, 0, not an integer, newer), the top-level order (a `unit`, `stage` or `rules` out of place, a second `stage`), what the
  whole file lacks, and times as ticks (0.25 → 8, 60 → 1800, 0.01 → 1).
- Scenes on changed rules: another grid and cell size (cell centers, outside the zone, the field's edge), a short `round_time` ends at
  that tick, `retarget_interval`, `stop_fraction`, `gravity` (a shell lands on its aim point under any pull, and flies longer under
  less), `min_damage_fraction` 0 and 1.
- Layers: `unit_layer Air` with `altitude_m`, air without `altitude_m`, ground with `altitude_m`, `altitude_m 0`, an unknown word (case), given twice, no value, `target_layer` words (`Both`, unknown, two values, no value).
- Unit fields (`unit_fields.md`, "테스트" (Tests)): each column missing, out of range, not an integer, its default; the new kinds of value (text, words, booleans); a scene for each row of the wiki's load-time check table; the wiki's ten units read and the keys not followed yet are listed; the D-07 grid and its spacing both ways; rectangular placement; shield HP; melee.
- Roles: the `roles` block's errors (a comma in a name, over 31 bytes, declared twice, over 32, its place) and a unit's `roles` errors (undeclared, given twice, an empty item, no role) are at their lines. Names with spaces, and spaces around the commas. A mask is the bits in declaration order. A round with other roles has the same hash.
- Squads: the ranges of `squad_size`, `footprint_w` and `footprint_d` (0, over the
  limit, not an integer, twice), a footprint deeper than a zone or wider than the grid and a rules edit that would make it so refused, a squad whose units overlap (touching is fine, no error
  on top of another, a `cell_size` edit that would make it so refused), a `place`'s footprint
  inside the enemy zone (the last x and row), overlap, an unknown unit's cell, supply by squad, and over 2048 enemy units.
- **Golden hashes:** rounds of seeds 1 to 4 on `full_project` (a copy of `default.abproj` from when its units became the wiki's) give
  the tick count, the outcome, the hash at tick 300, the hash at the end and the enemies left. They were recorded again when the units
  became the wiki's (2026-11-06). If a change to the rules' arithmetic changes a round, these are meant to change, and are changed on
  purpose.

### Phases

1. **Done:** `BattleRules`, the capacity macros, and code that reads the rules (same values, same hashes).
2. **Done:** `defs_read_project` and `default.abproj`; the two text files deleted; the tests cleaned up.
3. **Done:** `battle.md`'s "Definition files" section moved into this spec, and the `autobattler/` lines in `AGENTS.md` updated.

### Later (outside this spec)

- **Editor:** a unit's number keys are edited by the unit lab's Unit section (2026-11-05, `unit_lab.md`, "Editing the unit's values"; `defs_unit_set`). Still to come: a unit's word keys and the stage (its places too) are edited in the panel as well, changing only the edited values in place as the
  rules do ("Editing the rules"). The fixed top-level order makes it clear where new statements go. Undo is decided then too.
- **A repository of its own:** `default.abproj` moves to a data repository, pinned by the engine repository as a submodule. The
  version line checks that the two fit.
- **Several stages:** `stage` gets a name (`stage <name>`) and there can be several. If stages come to have fields of different
  sizes, `grid` moves to `stage`. Raises the version.
- **A unit's keys:** when the wiki adds or changes a column, the format follows it under the same name (`unit_fields.md`, "나중에"
  (Later)). Other keys, such as the stage's `prop`, join with the same syntax. An unknown key is still an error.
- **Effects and unit looks:** code in `battle_view.c` for now. They become blocks if designers need to work on them.

### Open questions

1. ~~**File name**~~: settled (2026-10-05). The extension is `.abproj` and the file `default.abproj`. If the data gets its own
   repository, it sits at that repository's top.
2. ~~**Where the grid size lives**~~: settled (2026-10-05). In `rules`. If stages come to have fields of different sizes, it moves to
   `stage` then, with a version raise.
3. ~~**Top-level order**~~: settled (2026-10-05). Fixed ("Syntax").
