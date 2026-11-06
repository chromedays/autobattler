# 위키 필드에 맞춘 유닛 형식 스펙

상태: 초안 (2026-11-06). 합의 전이다. `abproj.md`의 `unit` 블록을 바꾸므로, 합의되면 구현과 함께 `abproj.md`의 표와 예를 이 스펙대로
고친다("단계" 5).

### 목표

[위키](https://github.com/chromedays/autobattler/wiki)의 [유닛 수치](https://github.com/chromedays/autobattler/wiki/Unit‐Stats)
(`unit_stats.csv`)가 유닛 값의 기준이다. 지금 `.abproj`의 `unit` 블록은 그 CSV와 필드 구성이 다르다: 43개 열 중 15개만 뜻이 같고, 8개는
일부만 있으며, 20개는 없다. 거꾸로 `weapon` 블록, `launch_angle`, `muzzle`, 실드의 `radius`, `regen`, `regen_delay`는 위키에 없다.

이 스펙은 `unit` 블록을 **CSV 한 행과 같은 필드**로 바꾼다. 그러면 위키의 행을 칸 하나당 한 줄로 옮겨 적을 수 있고, 위키와 저장소가
같은 이름으로 같은 값을 가리킨다.

### 범위

| 이 스펙 | 이 스펙 밖 (따로 스펙을 쓴다) |
|---|---|
| `unit` 블록의 키, 값의 종류, 필수와 기본값, 읽을 때의 검사(위키 [유닛 설계 문서의 로드 시 검증](https://github.com/chromedays/autobattler/wiki/Unit-Design-Document#로드-시-검증)) | 일제 사격(전투 규칙 §5), 범위 피해(§7), 직선탄, 유도탄, 스크립트 궤적(투사체 사양 §1–5) |
| 옛 키가 사라져서 함께 바꿔야 하는 규칙: 직사각형 footprint와 생성 격자(D-07), 모델별 실드 HP(D-05, D-10). 그리고 근접(D-13) | 대상 유지(D-20), 지상과 공중의 우선순위(D-25), 분리 무게(D-19) |
| 키가 바뀌어 함께 바뀌는 코드: `defs.c`, 실험실의 Unit 절, 뷰가 읽는 값, 테스트 | 정수 방어력 공식(D-23), 90초와 생존 점수(D-24), 아군 피해 제거(D-09) |
| `default.abproj`를 위키의 U01–U10으로 바꾸기 | footprint 회전, 여러 라운드 |

라운드가 아직 따르지 않는 키도 이 스펙에서 형식에 들어간다. 읽고, 검사하고, `UnitDef`에 담는다. 그 키를 따르는 규칙은 위 표의 오른쪽
스펙이 만든다. 그때까지는 "과도기" 절대로 돈다.

### 결정

1. **키 이름은 CSV 열 이름 그대로다.** 단위 접미사도 그대로 둔다(`radius_m`, `move_speed`, `initial_speed_mps`). 위키가 기준이므로
   옮겨 적을 때 이름을 바꾸지 않고, 위키를 읽는 사람이 파일에서 같은 이름을 찾는다. 지금의 짧은 이름(`health`, `speed`, `cooldown`)은
   없어진다("옛 키" 절).
2. **값도 CSV 표기 그대로다.** 열거형은 대소문자까지 그대로(`Ground`, `Both`, `UnguidedProjectile`), 불리언은 `true`와 `false`다.
   지금의 소문자 낱말(`ground`, `air`, `shield`)은 없어진다.
3. **블록은 평평하다.** 위키는 유닛마다 무기가 하나이고 능력 필드가 없다. 그래서 `weapon <이름>` 블록과 `ability shield` 블록이 없어지고,
   모든 키가 `unit` 블록에 바로 온다. 여러 무기는 "나중에" 절에 둔다.
4. **빈칸은 줄이 없는 것이다.** CSV의 빈칸이 아닌 칸마다 한 줄을 쓰고, 빈칸은 쓰지 않는다. 빈칸의 뜻은 위키를 따른다: 그 공격 방식에
   해당하지 않거나, 공통 기본값이 있는 열이면 그 기본값이다([투사체 사양 §7.1](https://github.com/chromedays/autobattler/wiki/Projectile‐Spec#71-공통-기본값-필드가-비어-있고-그-필드에-기본값이-있을-때-적용)).
   0은 "적었고 값이 0"이다.
5. **유닛의 이름은 `unit_id`다.** 블록은 `unit U01`로 열고, `place`도 `unit_id`로 가리킨다. 보이는 이름은 `name` 키다(공백 가능).
6. **형식 버전은 1 그대로다.** 형식이 개발 중이므로 `abproj.md`의 "버전" 표 1행에 바뀐 것을 적는다.

### 값의 종류

`abproj.md`의 "문법"에 다음을 더한다. 수, 정수, 이름은 그대로다.

- **글:** 키 뒤의 공백부터 주석(`#`) 앞까지에서 앞뒤 공백을 뺀 것. 1–31바이트의 UTF-8이고, 안에 공백이 있어도 된다(`name Scout Swarm`).
  비어 있으면 오류다.
- **목록:** 쉼표로 나눈 글들. 항목마다 앞뒤 공백을 빼고, 빈 항목과 같은 항목 두 번은 오류다(`roles Chaff/Screen, Flanker`).
- **열거형:** 키마다 정해진 낱말 중 하나이고, 대소문자를 가린다. 모르는 낱말은 오류다(`unknown attack_type 'melee' (Melee,
  UnguidedProjectile, GuidedProjectile)`).
- **불리언:** `true` 또는 `false`.

역할 이름도 글이 된다(`Chaff Clear`, `Support(future)`). `roles` 블록은 그대로 두고, 한 줄에 역할 하나를 글로 선언한다. 쉼표는 역할
이름에 쓸 수 없다.

### 키

열 순서는 CSV와 같다. 블록 안의 순서는 읽기가 따지지 않지만, 옮겨 적을 때는 CSV 순서로 쓴다. "쓰임"은 이 스펙을 구현한 뒤 라운드가 그
키를 따르는지다: **예**는 지금 따른다, **바뀜**은 이 스펙에서 규칙이 바뀐다, **과도기**는 아직 따르지 않는다("과도기" 절).

#### 식별과 배치

| CSV 열 | 키 | 값 | 필수 | 범위, 기본값 | 쓰임 |
|---|---|---|---|---|---|
| `unit_id` | `unit <id>` | 이름 | 예 | 파일 안에서 하나뿐. 최대 16개 | 예 |
| `name` | `name` | 글 | 예 | 보이는 이름 | 예 (패널과 실험실이 보인다) |
| `cost` | `cost` | 정수, 분대 전체 | 예 | 1–10000 | 예 |
| `squad_size` | `squad_size` | 정수 | 예 | 1–64 | 예 |
| `roles` | `roles` | 목록 | 아니오 | `roles` 블록에 선언한 역할 | 예 (메타데이터, 라운드는 읽지 않는다) |
| `unit_layer` | `unit_layer` | `Ground`, `Air` | 예 | | 예 |
| `target_layer` | `target_layer` | `Ground`, `Air`, `Both` | 예 | | 예 |
| `target_priority` | `target_priority` | `Ground`, `Air` | `Both`일 때만 쓸 수 있다 | 기본 `Ground` | 과도기 |
| `footprint_w` | `footprint_w` | 정수, 칸 (+x) | 예 | 1–8, 격자 너비 이하 | 바뀜 |
| `footprint_d` | `footprint_d` | 정수, 칸 (+z) | 예 | 1–8, `zone_rows` 이하 | 바뀜 |

#### 몸과 생존

| CSV 열 | 키 | 값 | 필수 | 범위, 기본값 | 쓰임 |
|---|---|---|---|---|---|
| `radius_m` | `radius_m` | 수, m | 예 | > 0 | 예 |
| `height_m` | `height_m` | 수, m | 예 | > 0 | 예 |
| `altitude_m` | `altitude_m` | 수, m | `Air`면 예, `Ground`면 금지 | > 0 | 예 |
| `hp` | `hp` | 정수 | 예 | ≥ 1 | 예 |
| `armor` | `armor` | 정수 | 예 | ≥ 0 | 예 (공식은 지금 것, D-23은 밖) |
| `shield_hp` | `shield_hp` | 정수 | 아니오 | ≥ 1. 없으면 실드 없음 | 바뀜 |
| `move_speed` | `move_speed` | 수, m/s | 예 | ≥ 0 | 예 |

#### 공격

| CSV 열 | 키 | 값 | 필수 | 범위, 기본값 | 쓰임 |
|---|---|---|---|---|---|
| `range` | `range` | 수, m | 예 | > 0 | 예 |
| `damage` | `damage` | 정수 | 예 | ≥ 0 | 예 |
| `attack_interval` | `attack_interval` | 수, 초 | 예 | > 0. 틱으로 바꾼다(올림) | 예 |
| `hits_or_shots_per_attack` | `hits_or_shots_per_attack` | 정수 | 예 | 1–16 | 근접은 바뀜, 투사체는 과도기 |
| `splash_radius` | `splash_radius` | 수, m | 예 | ≥ 0. 0은 단일 대상 | 과도기 |
| `attack_type` | `attack_type` | `Melee`, `UnguidedProjectile`, `GuidedProjectile` | 예 | | `Melee`는 바뀜, 투사체는 과도기 |

#### 투사체 (`attack_type`이 `Melee`가 아닐 때만)

`Melee` 유닛에 이 표의 키가 하나라도 있으면 오류다(`'initial_speed_mps' is for projectile attacks; unit 'U01' is Melee`).

| CSV 열 | 키 | 값 | 필수 | 범위, 기본값 | 쓰임 |
|---|---|---|---|---|---|
| `initial_speed_mps` | `initial_speed_mps` | 수, m/s | 예 | > 0 | 과도기 |
| `forward_acceleration_mps2` | `forward_acceleration_mps2` | 수, m/s² | 예 | ≥ 0 | 과도기 |
| `max_speed_mps` | `max_speed_mps` | 수, m/s | 아니오 | > 0. 없으면 상한 없음 | 과도기 |
| `lifetime_s` | `lifetime_s` | 수, 초 | 예 | > 0 | 과도기 |
| `max_travel_m` | `max_travel_m` | 수, m | 예 | > 0 | 과도기 |
| `collision_radius_m` | `collision_radius_m` | 수, m | 예 | > 0 | 과도기 |
| `gravity_scale` | `gravity_scale` | 수 | 아니오 | ≥ 0, 기본 0 | 과도기 |
| `aim_point_scatter_m` | `aim_point_scatter_m` | 수, m | 아니오 | ≥ 0, 기본 0 | 예 (지금의 `spread`) |
| `launch_spread_deg` | `launch_spread_deg` | 수, 도 | 아니오 | 0 이상 90 미만, 기본 0 | 과도기 |
| `piercing_count` | `piercing_count` | 정수 | 아니오 | ≥ 0, 기본 0 | 과도기 |
| `interceptable` | `interceptable` | 불리언 | 아니오 | 기본 `false` | 과도기 |
| `trajectory_mode` | `trajectory_mode` | `Straight`, `ScriptedArc` | 예 | | 과도기 |
| `arc_height_m` | `arc_height_m` | 수, m | `ScriptedArc`면 예, `Straight`면 금지 | > 0 | 과도기 |
| `retargeting_allowed` | `retargeting_allowed` | 불리언 | 아니오 | 유도탄은 기본 `false` | 과도기 |
| `target_leading` | `target_leading` | 불리언 | 예 | | 과도기 |

#### 유도 (`attack_type`이 `GuidedProjectile`일 때만)

필수는 유도탄에만 있다. `UnguidedProjectile` 유닛에 이 표의 키가 있어도 오류가 아니다(위키가 정하지 않았다; 읽고 담지만 쓰지 않는다).
`Melee` 유닛에는 투사체 키이므로 오류다(위 "투사체" 절).

| CSV 열 | 키 | 값 | 필수 | 범위, 기본값 | 쓰임 |
|---|---|---|---|---|---|
| `turn_rate_deg_s` | `turn_rate_deg_s` | 수, 도/초 | 유도탄이면 예 | > 0 | 과도기 |
| `guidance_delay_s` | `guidance_delay_s` | 수, 초 | 유도탄이면 예 | ≥ 0 | 과도기 |
| `guidance_mode` | `guidance_mode` | `PredictiveInterception` | 유도탄이면 예 | | 과도기 |
| `target_loss_behavior` | `target_loss_behavior` | `ContinueStraight` | 아니오 | 기본값 없음(위키 §7.2: 유도 전용) | 과도기 |

#### 키가 없는 열

- `projectile_hp`: 위키에서 언제나 빈칸이고 쓰이지 않는다. 키를 만들지 않는다(쓰면 모르는 키 오류).

### 읽을 때의 검사

위키의 로드 시 검증 표를 모두 오류로 만든다. 지금의 오류(모르는 키, 두 번 나온 키, 범위, 빠진 필수 키)는 그대로이고, 다음이 더해진다.

| 조건 | 오류의 예 |
|---|---|
| `unit_id` 중복 | `unit 'U01' is defined twice` (지금의 이름 중복과 같다) |
| 모르는 열거형 값, 불리언이 아님 | `unknown trajectory_mode 'Arc' (Straight, ScriptedArc)` |
| `Melee`인데 투사체 키가 있음 | 위 "투사체" 표 |
| 유도탄인데 `turn_rate_deg_s`, `guidance_delay_s`, `guidance_mode` 중 하나가 빠짐 | `unit 'U06' is missing 'turn_rate_deg_s'` |
| `ScriptedArc`인데 `arc_height_m`이 없음, `Straight`인데 있음 | `'arc_height_m' is only for ScriptedArc` |
| `Air`인데 `altitude_m`이 없음, `Ground`인데 있음 | 지금과 같다 |
| `target_layer`가 `Both`가 아닌데 `target_priority`가 있음 | `'target_priority' is only for target_layer Both` |
| 생성 격자의 간격이 `2 × radius_m`보다 작음(돌리지 않은 방향과 돌린 방향 둘 다) | `24 units on 5x2 cells stand 1 m apart when turned, under their width (1.2 m)` |
| footprint가 구역보다 큼 | `'footprint_d' must be at most 14 (a zone's rows)` |

빠진 필수 키는 지금처럼 블록의 줄에 알린다. 어떤 키가 필수인지가 `attack_type`, `trajectory_mode`, `unit_layer`, `target_layer`에 달려
있으므로, 그 키에 오류가 있으면 그 키에 기대는 필수 검사와 금지 검사는 하지 않는다(실수 하나가 오류 하나, `abproj.md`와 같은 원칙).

### 옛 키

모두 없어진다. 옛 파일은 읽히지 않는다(버전 1 안의 변경, `abproj.md`의 "버전"). 저장소에 있는 `.abproj`는 `default.abproj` 하나다.

| 옛 키 | 새 키 |
|---|---|
| `unit <이름>` | `unit <unit_id>`와 `name` |
| `squad` | `squad_size` |
| `footprint` | `footprint_w`, `footprint_d` |
| `health` | `hp` (정수) |
| `armor` | `armor` (정수, 필수) |
| `speed` | `move_speed` |
| `radius`, `height`, `altitude` | `radius_m`, `height_m`, `altitude_m` |
| `layer ground`, `layer air` | `unit_layer Ground`, `unit_layer Air` |
| `weapon <이름>` 블록 | 없음. 키가 `unit` 블록으로 온다 |
| `targets ground`, `targets air`, `targets ground air` | `target_layer Ground`, `Air`, `Both` |
| `cooldown` | `attack_interval` |
| `damage`, `range` | 그대로 (`damage`는 정수) |
| `spread` | `aim_point_scatter_m` |
| `launch_angle` | 없음 (과도기에는 코드의 상수) |
| `muzzle` | 없음 (투사체는 히트박스 가운데에서 나간다) |
| `ability shield` 블록, `capacity` | `shield_hp` |
| 실드의 `radius`, `regen`, `regen_delay` | 없음 (D-05, D-10) |

### 함께 바뀌는 규칙

옛 키가 없어지므로 그 키를 쓰던 규칙도 위키의 결정으로 바꾼다.

**직사각형 footprint (D-07).** 분대는 `footprint_w` × `footprint_d` 칸을 차지한다. 회전은 아직 없다(돌리지 않은 방향만 놓는다).
`battle_place`, `battle_remove`, `battle_squad_at`, 구역과 겹침 검사가 w와 d를 따로 쓴다. 생성 격자는 위키의 공식이다:

```text
W, D    = footprint_w × cell_size, footprint_d × cell_size
columns = ceil(sqrt(squad_size × W / D)), [1, squad_size]로 제한
rows    = ceil(squad_size / columns)
spacing = min(W / columns, D / rows)
```

정사각형이면 지금의 `ceil(sqrt(squad_size))`와 같다. 읽을 때 두 방향(돌리지 않은 것과 W, D를 바꾼 것) 모두 `spacing ≥ 2 × radius_m`을
검사한다. 회전을 더할 때 이미 읽힌 파일이 오류가 되지 않게 하려는 것이다. `BATTLE_MAX_FOOTPRINT`는 4에서 8로 늘린다(로스터의 가장 긴 변은 5).

**모델별 실드 HP (D-05, D-10).** 실드는 이제 유닛을 감싸는 구가 아니라 유닛 자신의 HP 앞에 있는 값이다.
- 실드가 0보다 큰 유닛이 맞으면 실드가 공격을 통째로 받는다: `shield = max(0, shield − damage)`. 남는 피해는 버리고, 방어력은 쓰지 않는다.
- 실드는 라운드 중에 차지 않는다. 라운드를 시작할 때 가득 차 있다.
- 이웃을 지키는 것, 포탄이 실드 경계에서 멈추거나 남은 피해로 계속 나는 것은 없어진다. 포탄은 유닛의 원기둥에 닿을 때 그 유닛의 실드에 닿는다.
- 뷰는 실드가 남은 유닛에 몸을 감싸는 반투명한 구를 그린다(크기는 뷰가 정한다). 실드 피격 이펙트는 그대로다.

**근접 (D-13).** `attack_type Melee`는 투사체를 만들지 않는다.
- 쿨다운이 끝났고 대상이 `range` 안이면(지면 평면, 중심 사이) 그 틱에 대상에게 피해 사건을 `hits_or_shots_per_attack`번 낸다. 트리거
  볼륨이나 몸의 접촉은 따지지 않는다. 사거리 값이 곧 트리거 거리다.
- 피해 사건 하나는 포탄 명중과 같은 길을 간다: 실드가 있으면 실드가 통째로 받고, 없으면 방어력을 뺀 HP 피해가 틱 끝에 들어간다.
- 쿨다운, 멈춤 거리(`stop_fraction`), 대상 고르기는 다른 공격과 같다.
- `fire_weapons`(`battle.c`)에서 `fire_shell` 대신 이 갈래를 탄다. 이벤트는 피해 사건마다 `BATTLE_EVENT_HIT`(실드가 받으면 `BATTLE_EVENT_SHIELD_HIT`)이고, 발사 이벤트(`BATTLE_EVENT_FIRE`)는 없다.
  뷰는 공격자와 대상 사이에 짧은 타격 이펙트를 그린다.

### 과도기

라운드가 "과도기" 키를 따르게 될 때까지는 다음과 같다.

- **투사체 공격은 모두 지금의 포탄으로 쏜다.** `attack_type`이 `UnguidedProjectile`이든 `GuidedProjectile`이든, `trajectory_mode`가 무엇이든 그렇다(근접은 위 "근접"). 발사각은
  45° 상수(`BATTLE_INTERIM_LAUNCH_ANGLE`, `battle.c`)이고, 퍼짐은 `aim_point_scatter_m`, 시작점은 히트박스 가운데다. 중력은 규칙의 `gravity`다.
- 투사체 공격 한 번은 한 발이다(`hits_or_shots_per_attack` 무시). 범위 피해, 관통, 대공 우선순위는 없다.
- `defs_read_project`는 라운드가 따르지 않는 값이 있으면 경고 한 줄을 남긴다(오류가 아니므로 읽기는 성공한다). 따르지 않는 값은:
  `Melee`가 아닌 `attack_type`, 투사체 유닛의 1보다 큰 `hits_or_shots_per_attack`, 0보다 큰 `splash_radius`, `target_priority`,
  `aim_point_scatter_m`을 뺀 투사체 키와 유도 키다. 줄은:
  `default.abproj: the round does not follow these keys yet: attack_type, hits_or_shots_per_attack, ... (docs/specs/unit_fields.md)`.
  키 이름은 처음 나온 순서로 한 번씩 적는다. 규칙이 하나씩 생길 때마다 그 키를 이 목록에서 뺀다.

### 코드

- **`defs.c`:** 필드 표(`unit_fields`)가 이 스펙의 키 표가 된다. `weapon_fields`, `shield_fields`, 블록 스택의 셋째 단은 없어진다. 새
  `FieldKind`: `FIELD_TEXT`, `FIELD_LIST`(역할), `FIELD_ENUM`(낱말 목록을 가진다), `FIELD_BOOL`. 조건부 필수와 금지(`attack_type`,
  `trajectory_mode`, `unit_layer`, `target_layer`에 따른 것)는 블록이 닫힐 때 검사한다.
- **`UnitDef`(`battle.h`):** CSV 한 행이다. 평평한 필드에, 공격과 투사체 값은 하위 구조체로 묶는다:

  ```c
  typedef struct ProjectileDef {
      f32 initial_speed, forward_acceleration, max_speed; // m/s, m/s²; max_speed 0 when it has none
      u32 lifetime_ticks;
      f32 max_travel, collision_radius, gravity_scale, aim_point_scatter, launch_spread; // launch_spread in radians
      u32 piercing_count;
      b32 interceptable, retargeting_allowed, target_leading;
      ProjectileTrajectory trajectory; // PROJECTILE_STRAIGHT, PROJECTILE_SCRIPTED_ARC
      f32 arc_height;
      f32 turn_rate, guidance_delay;   // radians per second, seconds; guided only
      GuidanceMode guidance_mode;      // GUIDANCE_NONE, GUIDANCE_PREDICTIVE_INTERCEPTION
      TargetLossBehavior target_loss;  // TARGET_LOSS_NONE, TARGET_LOSS_CONTINUE_STRAIGHT
  } ProjectileDef;
  ```

  `UnitDef`는 `id`(이름), `name`(글), `cost`, `squad_size`, `footprint_w`, `footprint_d`, `layer`, `target_layers`, `target_priority`,
  `radius`, `height`, `altitude`, `hp`, `armor`, `shield_hp`, `move_speed`, `range`, `damage`, `attack_ticks`, `hits_per_attack`,
  `splash_radius`, `attack_type`, `projectile`을 가진다. `WeaponDef`, `AbilityDef`, `ABILITY_SHIELD`는 없어진다. 정수 키는 정수로 읽지만
  `hp`, `armor`, `damage`는 D-23 전까지 `f32`로 담는다(라운드의 계산이 아직 부동소수점이다).
- **`defs_unit_get`, `defs_unit_set`:** 블록이 하나뿐이므로 `block` 인자와 `DefsBlock`이 없어진다. 없는 선택 키를 더할 때는 블록의
  마지막 줄 뒤에 둔다(지금과 같다).
- **실험실(`lab_view.c`의 `unit_value_keys`):** 고칠 수 있는 키는 수와 정수 키 전부다(열거형, 불리언, 글, 목록은 아직 고치지 않는다,
  `unit_lab.md`의 "나중에"). 라벨은 CSV 이름을 읽기 쉽게 한 영어이고(`HP`, `Move speed (m/s)`, `Attack interval (s)`) `src/strings.c`에
  한국어 행을 더한다. "Battle only"는 `cost`, `squad_size`, `footprint_w`, `footprint_d`다. 실험실의 표적(`UNIT_DUMMY`)도 새 `UnitDef`로 만든다.
- **뷰:** 배치 목록과 실험실의 유닛 고르기는 `name`을 보인다(`Scout Swarm x24 (50)`). 이름은 데이터이므로 `T()`를 거치지 않는다.
- **디버그 내보내기:** 이름은 그대로다. `_battle_debug_set_unit_value(index, value)`는 새 키 표의 번호를 쓴다. 실드 에너지를 돌려주는
  값은 유닛의 실드 HP를 돌려준다.

### `default.abproj`

유닛은 위키의 U01–U10이고, 값은 [유닛 수치의 CSV](https://github.com/chromedays/autobattler/wiki/Unit‐Stats#원본-csv)를 칸마다 한 줄로
옮긴 것이다. 규칙은 위키에 맞춰 `grid 32 68`(D-17)과 `round_time 90`(D-24)으로 바꾼다(값만 바뀐다). 나머지 규칙은 그대로다.
`roles` 블록은 로스터의 역할 12개를 선언한다.

적 배치(공급 1000 중 950, 시작점일 뿐이다; 칸은 footprint의 원점 쪽 모서리, 적 구역은 행 54–67):

| 유닛 | 칸 (x, 행) |
|---|---|
| U01 Scout Swarm | (0, 54), (26, 54), (8, 60) |
| U02 Rifle Squad | (6, 54), (20, 54) |
| U04 Shield Guard | (14, 54) |
| U03 Auto Cannon | (10, 57) |
| U10 Artillery Platform | (14, 60) |

예(일부):

```
roles
    Chaff/Screen
    Flanker
    Anti-Air
    Chaff Clear
    Tank/Anchor
    Support(future)
    Siege
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

unit U06
    name Interceptor
    cost 200
    squad_size 4
    roles Anti-Air, Chaff Clear
    unit_layer Air
    target_layer Both
    target_priority Air
    footprint_w 4
    footprint_d 2
    radius_m 0.9
    height_m 0.6
    altitude_m 10
    hp 230
    armor 0
    move_speed 20
    range 55
    damage 32
    attack_interval 0.45
    hits_or_shots_per_attack 2
    splash_radius 0
    attack_type GuidedProjectile
    initial_speed_mps 50
    forward_acceleration_mps2 100
    max_speed_mps 120
    lifetime_s 2.5
    max_travel_m 200
    collision_radius_m 0.15
    gravity_scale 0
    turn_rate_deg_s 240
    guidance_delay_s 0.05
    guidance_mode PredictiveInterception
    aim_point_scatter_m 0
    launch_spread_deg 0
    piercing_count 0
    interceptable false
    trajectory_mode Straight
    retargeting_allowed false
    target_loss_behavior ContinueStraight
    target_leading true

unit U10
    name Artillery Platform
    cost 300
    squad_size 1
    roles Siege, Chaff Clear
    unit_layer Ground
    target_layer Ground
    footprint_w 3
    footprint_d 3
    radius_m 2.0
    height_m 3.0
    hp 900
    armor 10
    move_speed 3
    range 130
    damage 350
    attack_interval 4.0
    hits_or_shots_per_attack 1
    splash_radius 8
    attack_type UnguidedProjectile
    initial_speed_mps 55
    forward_acceleration_mps2 0
    lifetime_s 6.0
    max_travel_m 260
    collision_radius_m 0.35
    gravity_scale 0
    aim_point_scatter_m 0
    launch_spread_deg 0
    piercing_count 0
    interceptable false
    trajectory_mode ScriptedArc
    arc_height_m 30
    retargeting_allowed false
    target_leading false
```

### 테스트

`tests/battle_test.c`. 형식 검사는 지금처럼 테스트 안의 텍스트로만 한다.

- **CSV 열마다:** 빠짐(필수일 때), 범위 밖, 정수 자리의 소수, 두 번, 값 개수. 기본값(`gravity_scale`, `aim_point_scatter_m`,
  `launch_spread_deg`, `piercing_count`, `interceptable`, `retargeting_allowed`, `target_priority`)이 읽힌다.
- **새 값의 종류:** 글(공백 포함, 빈 값, 32바이트, 주석 앞에서 끝남), 목록(빈 항목, 같은 항목, 선언되지 않은 역할), 열거형(모르는 낱말,
  대소문자가 다름), 불리언(`True`, `1`은 오류).
- **로드 시 검증 표의 줄마다** 한 장면. 기대는 키에 오류가 있을 때 기대는 검사가 오류를 더하지 않는 것도 본다.
- **위키의 로스터:** 테스트 안의 텍스트로 U01–U10 열 개를 옮긴 것이 오류 없이 읽힌다(위키는 모든 분대가 두 방향의 간격 검사를
  통과한다고 적는다). 경고 줄에 과도기 키가 나온다.
- **생성 격자:** 위키 [배치와 위치 지정 §5](https://github.com/chromedays/autobattler/wiki/Deployment‐and‐Placement#5-생성-위치-배정)의
  계산 예(U01 8 × 3, 간격 1.25 × 1.33 m; U02 5 × 3에 마지막 행 2; U06 3 × 2에 마지막 행 1)와 같은 자리, 돌린 방향의 간격 오류.
- **직사각형 배치:** 구역의 x와 행 끝, 겹침(w와 d가 다른 두 분대), `battle_squad_at`이 footprint의 모든 칸에서 찾는다.
- **실드:** 큰 공격 하나를 통째로 받고 넘치지 않는다, 실드 피격에 방어력을 쓰지 않는다, 회복하지 않는다, 실드가 0이면 HP로 간다.
- **근접:** 사거리 안에서 쿨다운마다 피해 사건 `hits_or_shots_per_attack`번(2로 놓고 센다), 투사체가 생기지 않는다, 사거리 밖이면 치지
  않는다, 실드를 먼저 친다, 방어력 식. 근접 유닛만 있는 파일에는 경고 줄이 없다.
- **황금 해시:** 실드와 footprint가 바뀌므로 다시 기록한다(일부러 바꾸는 것임을 커밋에 적는다).
- 저장소의 `default.abproj`는 오류 없이 읽히고 한 라운드가 `round_ticks` 안에 끝난다(지금과 같다).

웹의 Release와 Debug, Windows의 Release와 Debug를 모두 빌드하고, 실험실에서 U01–U10을 하나씩 골라 콘솔에 WebGPU 오류가 없는지 본다.

### 서드파티

없다. 지금의 줄 단위 읽기(`engine/text.h`의 `NvText`)에 값의 종류를 더하는 것으로 충분하다. CSV를 직접 읽지 않으므로 CSV 라이브러리도
필요 없다(열린 질문 2).

### 단계

1. **형식:** `defs.c`의 키 표, 새 값의 종류, 조건부 검사, 경고 줄, `UnitDef`. 형식 테스트.
2. **규칙:** 직사각형 footprint와 생성 격자(D-07), 실드 HP(D-05, D-10), 근접(D-13), 과도기의 포탄. 테스트와 황금 해시.
3. **화면:** 실험실의 키 표와 라벨(`strings.c`), 실드 그리기, 이름 보이기, 디버그 내보내기(`battle_debug_calls`도).
4. **데이터:** `default.abproj`를 U01–U10과 새 규칙 값으로.
5. **문서:** `abproj.md`(키 표, 값의 종류, 예, 버전 표 1행), `unit_lab.md`("유닛 값 고치기"의 키), `AGENTS.md`(형식 설명). 이 스펙의
   상태를 "구현됨"으로. `battle.md`는 더 이상 쓰지 않는 문서라 고치지 않는다(규칙의 기준은 위키다).

### 나중에 (이 스펙 밖)

- 과도기 키를 따르는 규칙: 투사체의 일제 사격, 범위 피해, 투사체 사양(직선, 유도, 스크립트 궤적), 대상 유지와 우선순위. 하나씩 스펙을 쓰고,
  만들 때마다 경고 줄에서 뺀다.
- 실험실에서 열거형, 불리언, 글 키 고치기.
- 유닛마다 무기 여럿: 위키가 그렇게 바뀌면 블록을 다시 생각한다.
- footprint 회전(위키 배치와 위치 지정 §3).

### 열린 질문

1. ~~**키 이름**~~: 해결됨(2026-11-06). CSV 열 이름 그대로 쓴다("결정" 1).
2. ~~**CSV를 직접 읽기**~~: 해결됨(2026-11-06). `unit` 블록에 쓴다. CSV 파일을 읽지 않는다.
3. ~~**`target_loss_behavior`**~~: 해결됨(2026-11-06). 위키대로 요구하지 않는다(선택, 기본값 없음).
4. ~~**유도 키의 금지**~~: 해결됨(2026-11-06). 위키대로 오류가 아니다(`Melee`에만 오류).
5. **위키 안의 이름 불일치:** 위키 전투 규칙 §3은 이동 속도를 `speed_mps`라고, §5는 탄의 속도를 `projectile_speed_mps`라고 쓰는데, CSV
   열은 `move_speed`, `initial_speed_mps`다. 위키의 그 두 줄을 CSV 이름으로 고친다.
6. ~~**`battle.md`의 B 규칙**~~: 해결됨(2026-11-06). `battle.md`는 더 이상 쓰지 않는다. 규칙의 기준은 위키다.
7. ~~**과도기의 근접**~~: 해결됨(2026-11-06). 근접(D-13)을 이 스펙에 넣는다("함께 바뀌는 규칙"의 "근접").
