# Animated actions

`UFSAnimatedAction` owns one concrete montage instance. Successful playback starts the action;
natural montage completion releases it. Interrupted blend-out releases it immediately. Instance IDs
and generation tokens reject notifications and callbacks belonging to earlier playback.
Cancellation, death, level teardown and AnimInstance replacement clear action windows, movement and
jump locks, facing overrides, uncommitted emission and action travel. Already emitted projectiles
and persistent wards retain their existing gameplay rules.

The player uses `/Game/Combat/Actions/DA_FS_PlayerActions`. Each entry holds a montage and a play
rate. `FSClassCombat` finds an existing `FSAnimatedAction` component or creates one at BeginPlay.
A Blueprint can supply another action set or call `PlayAction` / `PlayMontage`, and bind the
started, ended, window and cue delegates. New animation definitions do not require C++ changes.

## Animation phases

Author `FS Action Window` notify states and `FS Action Event` notifies on the montage:

| Name | Purpose |
| --- | --- |
| `Hit` | Weapon/capsule contact; existing per-target damage deduplication remains in combat. |
| `Commit` (event) | Emit a projectile, wave, seal or impact exactly once; cancel before it to prevent emission. |
| `Invulnerable` | Dodge damage immunity. |
| `MoveLock` | Suppress normal locomotion during the authored phase. |
| `Travel` | Swept action displacement, sampled from animation progress rather than elapsed seconds. |
| `Guard` | Directional damage reduction. |
| `Charged` | Charged release and its feedback. |
| `Recovery`, `Combo`, `Cancel` | Skill transitions, buffered basic attacks and permitted evade cancellation. |

The current actions each use one contiguous window per phase. Pose-to-pose hit sweeps are clipped
to the actual notify interval, including frames crossing an entire window. Travel uses the same
animation interval, and roll/backstep displacement follows the assigned sequence's root curve.
No action age, end-time, independent movement clock or recovery timeout determines completion.
Travel uses CharacterMovement's capsule floor test, so a beveled floor tile does not reject a leap
because a single ray happened to hit its sloped edge. Existing maximum floor-drop limits remain.

Natural blend-out must finish at the end of the clip. The supplied montages have automatic
blend-out and a zero-duration outgoing blend; incoming blending remains visual polish. Do not
add an outgoing hold or disable automatic blend-out on a finite action.

## Replacing an animation

For a one-segment action, use Unreal Python:

```python
import unreal as u
montage = u.load_asset('/Game/Combat/Actions/AM_FS_Sword')
clip = u.load_asset('/Game/MyAnimations/AN_NewSwordAttack')
assert u.FSActionSet.replace_animation(montage, clip)
u.EditorAssetLibrary.save_loaded_asset(montage)
```

This keeps the skeleton contract, replaces the segment, calculates the real segment duration
(including sequence `RateScale`), and rescales proportionally linked notify boundaries. Review
the contact poses when replacing choreography. A different swing can have different contact
poses even when its normalized timing is the same. No Blueprint or C++ timing edits are needed.
Montage editor segment changes use Unreal's proportional notify links. Change play rate in the
action set or call `SetActionPlayRate` during playback; notifies and completion follow playback.

`Tools/BuildActionAssets.py` creates missing montage assets. `-FSRebuildActions` explicitly
reauthors the supplied action tracks and clips; do not use it after hand-authoring those tracks.

## Migration audit

| Previous dependency | Replacement |
| --- | --- |
| `FSClassCombat::Start`, `EndTime`, `ActionClockRate`, action-age thresholds | Montage instance end/interruption plus authored notify phases. |
| `MoveTime`, `MoveAge`, fixed travel windows | `Travel` notify interval and actual montage position. |
| Attack/evade availability timers | Actual action completion; removed the extra basic/evade recovery lock. |
| Timed Fury Leap clip switch and forced montage stop | One action montage, swept leap travel and `Hit` phase. |
| Legacy `FSCharacterActions::BeginDodge` 0.22-second root force | Routes to the same class evade implementation. |
| Player defeat-age comparison | Non-looping SingleNode playback retains the actual final pose. Defeat persists until returning to Guild by design. |
| Player Blueprint action events | Already route through `FSClassBridge`; no action-ending Delay remains there. |

Ability cooldowns, resource regeneration, projectile range/speed, ward/anchor lifetime, camera
smoothing, audio feedback and UI messages remain independent gameplay/presentation values.
Guild/chest interactions currently complete synchronously and have no animated action lock to
migrate. Crypt NPC attack completion already samples actual SingleNode playback; NPC tactical
decision/defense timers are outside the player action implementation.

Fury Leap keeps its takeoff and strike as two segments of one montage. The editor-only
`SetAnimationSegments` helper assembles clips before authoring notify tracks; clip switching then
belongs entirely to the montage timeline.

## Validation

Run `Tools/TestAnimatedActions.py` in an editor with an isolated `-UserDir`. It writes
`Saved/Actions150PIE.json`. Fixtures use a transient action set and unsaved duplicate montages;
they do not alter the real profile. The report covers Warrior locomotion release, dodge jump
locking, play-rate changes, animation replacement, pause, interruption, death and class skills.

FS-150 evidence: Win64 Editor compilation passed, as did 28 scripted lifecycle cases and 13
Dungeon damage/contact cases (`Tools/TestAnimatedCombat.py`). `Tools/TestAnimatedLoop.py` passed
contract acceptance, Dungeon travel, reward deduplication, Guild return and a new-PIE save reload.
Runtime reports are under `Saved/Actions150*.json`; normal device input and packaged playback
remain NOT_TESTED.
