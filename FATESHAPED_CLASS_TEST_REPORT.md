# Fateshaped — Class Acceptance Test Report

**Date** 2026-11-14, Europe/Kyiv  
**Scope** All 12 essence classes on the RC14-era working tree, using the current Win64 Unreal Editor module.  
**Acceptance source** [FATESHAPED_CLASSES.md](FATESHAPED_CLASSES.md).  
**Branch** `feat/guild-essence-persistence`. Gameplay files already had local FS-116 and FS-117 changes at the start of the audit. No gameplay fixes were applied for this report.

## Test method and limits

- The automated suite started an isolated PIE profile in `L_Guild`, accepted the real contract and entered `L_Dungeon_01` through the portal. It repositioned a stationary native Crypt enemy and temporarily reused the exit gate as a wall for marked cases. No game map was saved.
- The suite reset ordered class paths through an editor-only fixture and invoked the class action bridge. This exercises action state, combat, resource mutation and collision, while bypassing actual keyboard/controller events and their queue. Rotation was fixed for repeatable straight-line cases.
- Damage was measured as the enemy health delta. Costs were measured from the lowest sampled pool value, with passive regeneration allowed to continue. Distance is horizontal net displacement, so a curved 185 cm travel path need not end 185 cm from its origin.
- The FS-118 combat suite ran without rendering or sound. The separate FS-119 addendum below records rendered viewport input, screenshot observations, and expedition attempts. Physical controller bindings and subjective 45-second feel checks remain **NOT_TESTED**.
- Earlier path, save and travel checks are identified as historical evidence below. A passing targeted check establishes the behavior measured by that check. Each class still has untested acceptance criteria.
- The original save was checked by hash. Test data uses ignored local files under `Saved/FS115QA`.

## Priority findings

### P1 — Blood Thread can miss on recall after hitting the same target outbound

- **Expected.** A live enemy intersecting both clear legs takes 16 then 28 damage. Recall consumes the anchor and has no additional cost.
- **Observed.** At 235 cm directly ahead, the cast paid 12 V and 18 M and dealt 16 total after two Q presses. The matched test expected 44. An independent run reproduced 16 total with the stationary target at 140, 200, 235 and 300 cm. The anchor in open-lane cases appeared about 567–568 cm forward and 17–30 cm laterally from the starting capsule center, then disappeared after recall, with no change in enemy health. With a wall centered at 250 cm and the enemy at 100 cm, the anchor stopped at about 221 cm and the return again dealt zero. The outbound-only control dealt 16 and retained the anchor during the observation. These measurements confirm the symptom and anchor consumption. The exact reason the return fails contact is unresolved.
- **Code to inspect.** Recall derives a horizontal direction toward the player and starts from the anchor height; the outbound origin is the cast hand. The two-leg result depends on the world-space hand offset, anchor and caster position, so the return line is a separate geometric test. Review recorded positions before changing damage, width or costs. [Recall implementation](Source/Fateshaped/FSClassCombat.cpp#L971)

### P1 — Spell weaver can cancel a committed Orb preparation after its step ends

- **Observed and reproduced in PIE.** Start F, press Q at approximately 0.13 s for a valid step, keep holding F, and press evade at 0.41 s. The hero moved 210 cm sideways, then another 340 cm forward, spent **47 S** in total and spent **0 M** because the Orb was canceled. The matched control pressed evade at 0.19 s while travel was in progress. It stayed on the 210 cm step, spent **22 S**, and the Orb subsequently spent **20 M**.
- **Code.** The Orb-hold evade gate requires both `bStepped` and `MoveTime > 0` to block an evade. The specification keeps new evade input locked until both linked actions finish. [Evade gate](Source/Fateshaped/FSClassCombat.cpp#L424)

### P2 — Neutral Barbarian Backstep flips the actor and sends the next Backstep in the opposite direction

- **Expected interpretation.** With a fixed view, a neutral retreat carries the character away from the direction they were facing. A second neutral retreat after cooldown should keep retreating unless the player changes their facing or movement input.
- **Observed in PIE.** A single neutral Backstep traveled 204 cm. With a second neutral evade at 0.90 s and no camera or movement changes, net displacement was approximately 0 cm. The action derives the neutral direction from negative actor forward and then turns the actor to face the movement direction. The two moves therefore oppose each other. The specification defines neutral direction from the body but does not explicitly say whether the animation may turn the body, so this is a confirmed usability issue and a facing-rule decision. [Backstep direction](Source/Fateshaped/FSClassCombat.cpp#L438)

### P1 — Placeholder poses and effects obscure when several actions become active

- **Evidence from source and existing owner feedback.** Fury Leap's visual currently combines a jump clip and a separate generic strike, while the contact interval is defined by action elapsed time. Shield Advance and Stonebreak hold states enter without dedicated hold clips. The Paladin seal is a primitive cylinder and the Mage projectile is a moving marker. This makes the numeric action tests insufficient evidence for visual readability. [Action startup](Source/Fateshaped/FSClassCombat.cpp#L270) · [Fury Leap strike](Source/Fateshaped/FSClassCombat.cpp#L738) · [Seal visual](Source/Fateshaped/FSClassCombat.cpp#L1152)
- **Acceptance.** In rendered play, check visible contact, readable guard/brace transitions, matching launch cues, and visible whiff/blocked feedback. The user previously described the Barbarian animation as rough. RC14 animation appearance still needs human review.

### P2 — Paladin seal feedback and validation are incomplete

- **Evidence from source.** On commit, the seal checks the line to the stored center and Mana. The commit path does not repeat the walkable-floor trace at the captured location. The inside check for the Orb sound uses horizontal distance alone, while damage absorption uses both radius and floor height. Spark has no seal pulse branch. The ward code uses a damage-capacity number and an impact sound, while edge cues and casting pulses in the specification require further implementation. [Commit and cast feedback](Source/Fateshaped/FSClassCombat.cpp#L927) · [Ward conditions](Source/Fateshaped/FSClassCombat.cpp#L1121)
- **Acceptance.** Cast onto an aimed point, invalidate its floor during windup, and verify refusal without spending. Compare entry, exit, Spark and Orb inside and outside the ring, including on a different elevation.

### P2 — Mage trajectory preview can diverge from the emitted shot

- **Evidence from source.** The release direction clamps pitch to ±35°, while the preview uses the raw aim direction. The preview starts from the hand during the hold, while the shot starts from the hand at emission, after the cast pose changes. The preview uses fixed 20 cm steps and the projectile integrates in steps up to 20 cm. Neither a projected endpoint nor a distinct obstruction marker is drawn. [Release](Source/Fateshaped/FSClassCombat.cpp#L630) · [Preview](Source/Fateshaped/FSClassCombat.cpp#L1449)
- **Acceptance.** Hold aim steeply upward and downward and compare the preview with the actual launch. Repeat with mirrored curves around a pillar. The neutral shot and pre-emission cancel are covered by the main matrix.

### P2 — Practice targets are filtered inconsistently

- **Evidence from source.** Melee and Fury Leap reject hidden practice targets. The moving-shot loop accepts any actor with the practice tag regardless of visibility. Fury Leap queries only `ECC_Pawn`, so a visible dummy on another channel will not be found by that sweep. [Melee filter](Source/Fateshaped/FSClassCombat.cpp#L685) · [Leap filter](Source/Fateshaped/FSClassCombat.cpp#L709) · [Projectile filter](Source/Fateshaped/FSClassCombat.cpp#L790)
- **Acceptance.** Exercise all three paths against visible, hidden and non-pawn practice targets in the Guild. Require feedback for supported visible targets and zero XP, healing or rewards.

### P2 — Damage feedback can outlive its action

- **Observed in PIE.** The wall-blocked Fury Leap dealt zero damage and restored zero V, but the last HUD message still read `Fury Leap +20.4 V` from an earlier successful hit. The fixture retained the same pawn between actions, so this is a valid stale-message observation across actions. It does not establish what is visible in the rendered HUD during a normal fight.
- **Acceptance.** Without reloading, land a successful leap then repeat against a wall or empty floor. The UI should distinguish the earlier recovery from the new miss.

## Class-by-class conclusions

| Path | Class | Verified in the current combat suite | Outstanding acceptance and issues |
|---|---|---|---|
| V | Barbarian | Heavy Axe Swing dealt 34. Close and farther Fury Leap each dealt 40.8, spent 15 V and restored 20.4 V. Wall blocked both damage and healing. 15 V refused, 16 V whiff left 1 V. A single Backstep traveled 204 cm. | Two neutral Backsteps returned to the starting point. Check rendered leap continuity, evade immunity and recovery boundaries, mitigation and overkill healing. Clear stale healing text. |
| V → V | Juggernaut | Short and full Stonebreak dealt 34 and 50 and spent 18 V. Backstep canceled before payment. | Test brace against frontal, side and stomp hits. Check start boundary at 18/19 V, pulse truncation at a wall, four-target limit and charge cues. |
| V → S | Berserk | Early Q spent zero S. Q in recovery spent 24 S and produced 176.06 cm net displacement along the default curved route. | Left and right paths, exact opening and closing frame, two-target contact, a wall-clipped pivot and cooldown preservation. A damaging pivot was outside the main fixture. |
| V → M | Blood Mage | Outbound leg dealt 16 and paid 12 V + 18 M. Backstep before launch preserved both pools. | Investigate recall geometry and reproduce two hits on a registered intersection. Check anchor expiry, two-wall cases, low resource boundaries and cleanup after defeat/travel. |
| S | Warrior | Sword Cut dealt 22. Guard releases traveled 90/180 cm. The long auto-release traveled 180 cm. Each guard activation spent 20 S. | Incoming block strength and sector, Warden stomp bypass, shield damage, wall and low-Stamina cases. Shield proxy and absent dedicated hold pose need rendered review. |
| S → V | Mercenary | Paid Stand rejected 12 V. At 13 V it left 1 V and traveled 145 cm. | Verify the paid stance while already holding the guard, frontal/side hit mitigation, thrust hit window and wall occlusion. The measured activation used neutral input. |
| S → S | Swordsman | Neutral Slip Cut traveled 160 cm and spent 16 S. | Directional paths, blade damage and target cap, 15 S refusal, buffered cut near the existing cooldown, evade recovery and wall contact. |
| S → M | Soul's Blade | Arcane Wave dealt 40 for 30 M, wall blocked damage with cost retained, evade before emission preserved M. | Corridor edges and vertical tolerance, multi-target per-cast cap, second cast cooldown and three-target lane, normal-input migration playthrough. |
| M | Wizard | Spark dealt 12. Broad and focused Orb dealt 24 and 32 and each spent 20 M. Cancel before launch preserved M and prevented damage. | Wall at the hand versus camera, nearest of two targets, low Mana boundary, long-hold auto-release, form recognition in rendered play. |
| M → V | Paladin | Seal at the player position spent 18 M. Evade before commit preserved Mana. | Aimed placement, 35% absorption and 42-point depletion, outside/height and trap bypass, duration and cleanup. Visual cues and floor revalidation gaps above. |
| M → S | Spell weaver | Moving cast traveled 210 cm and spent 22 S and 20 M. A near wall rejected the step, preserved S and let the Orb cast for 20 M. Evade during travel was refused. | Evade after travel canceled the linked Orb and spent an extra 25 S. Check simultaneous hardware input, exact moving launch origin, ledges and defeat cleanup. |
| M → M | Mage | Neutral Arcane Bend dealt 28 for 22 M. Cancel before emission preserved M and prevented damage. | Mirror curves, geometry and preview/emission mismatch, 21/22 M boundary, curved collision and effect readability. |

## Existing progression and packaging evidence

- The earlier FS-115 matrix selected all 12 exact histories at the altar, wrote stable IDs, and checked class maxima and one defining action per path. It verified repeated essences and distinct ordered paths. This run directly exercises class kits through fixtures.
- A legacy `[S, M]` copy opened as Soul's Blade with 720 XP, inventory intact and Arcane Wave available. The read-only-slot case preserved Warrior's pending choice on failure and consumed it once on retry. A separate process reloaded Mage with 400 XP and preserved it through defeat and return. Unsupported `[S, M, V]` retained the original history and used the supported prefix. See [FS-115 and FS-117 in DEVLOG.md](DEVLOG.md).
- Earlier FS-117 fixed-view and strafe camera probes showed stable yaw in PIE. The FS-119 addendum records separate rendered and input evidence.

## Reproduction and raw results

- Editor scripts and logs are under `Saved/FS115QA`. The main script is `ClassAcceptance118.py`, and `Class118c.json` holds the raw matrix with expected values, observed resource minima, damage and travel. `Recall118.py` and `Recall118.json` contain the Blood Thread follow-up. Earlier aborted runs stopped in the fixture when resetting read-only enemy health or reading its target list. They are excluded from the pass/fail totals. The local MCP endpoint did not respond during discovery. Tests ran through the editor Python entry point.
- In Unreal Editor use the existing `Fateshaped.uproject`, an isolated `-UserDir`, and run the script through `-ExecutePythonScript`. The test uses existing editor-only class fixtures and temporarily changes actors in the PIE world only.
- `Input118.py` and `Input118.json` contain the three follow-up input cases for Weave Step and Backstep. The report's main totals cover the 34 primary cases. The recall and input follow-ups are reported separately.
- Recommended next acceptance milestone is to fix and retest Blood Thread recall and the Weave Step evade gate, then run a rendered Barbarian Guild → Crypt → Guild playthrough and the remaining 45-second class sessions.

### Main PIE matrix


**34 targeted checks across 12 classes. 33 passed and 1 failed the specified fixture.**

| Case | Result | Damage, expected → actual | Travel cm, expected → actual | V delta | S debit | M debit |
|---|---|---:|---:|---:|---:|---:|
| `axe` | PASS | 34 → 34 | 0 → 0 | 0 | 0.00 | 0.00 |
| `leap_close` | PASS | 40.8 → 40.8 | — → 67.06 | 5.4 | 0.00 | 0.00 |
| `leap_far` | PASS | 40.8 → 40.8 | — → 185.97 | 5.4 | 0.00 | 0.00 |
| `leap_wall` | PASS | 0 → 0 | — → 87.04 | -15 | 0.00 | 0.00 |
| `leap_15v` | PASS | 0 → 0 | 0 → 0 | 0 | 0.00 | 0.00 |
| `leap_16v` | PASS | 0 → 0 | 320 → 320 | -15 | 0.00 | 0.00 |
| `backstep` | PASS | 0 → 0 | 204 → 204 | 0 | 0.00 | 0.00 |
| `stone_short` | PASS | 34 → 34 | 0 → 0 | -18 | 0.00 | 0.00 |
| `stone_full` | PASS | 50 → 50 | 0 → 0 | -18 | 0.00 | 0.00 |
| `stone_cancel` | PASS | 0 → 0 | 204 → 204 | 0 | 0.00 | 0.00 |
| `pivot_early` | PASS | 0 → 0 | 0 → 0 | 0 | 0.00 | 0.00 |
| `pivot_valid` | PASS | 0 → 0 | — → 176.06 | 0 | 24.00 | 0.00 |
| `thread_recall` | FAIL | 44 → 16 | 0 → 0 | -12 | 0.00 | 18.00 |
| `thread_cancel` | PASS | 0 → 0 | 204 → 204 | 0 | 0.00 | 0.00 |
| `sword` | PASS | 22 → 22 | 0 → 0 | 0 | 0.00 | 0.00 |
| `shield_short` | PASS | 0 → 0 | 90 → 90 | 0 | 20.00 | 0.00 |
| `shield_long` | PASS | 0 → 0 | 180 → 180 | 0 | 20.00 | 0.00 |
| `shield_auto` | PASS | 0 → 0 | 180 → 180 | 0 | 20.00 | 0.00 |
| `paid_12v` | PASS | 0 → 0 | 0 → 0 | 0 | 0.00 | 0.00 |
| `paid_13v` | PASS | 0 → 0 | 145 → 145 | -12 | 0.00 | 0.00 |
| `slip` | PASS | 0 → 0 | 160 → 160 | 0 | 16.00 | 0.00 |
| `wave` | PASS | 40 → 40 | 0 → 0 | 0 | 0.00 | 30.00 |
| `wave_wall` | PASS | 0 → 0 | 0 → 0 | 0 | 0.00 | 30.00 |
| `wave_cancel` | PASS | 0 → 0 | — → 241.97 | 0 | 25.00 | 0.00 |
| `spark` | PASS | 12 → 12 | 0 → 0 | 0 | 0.00 | 0.00 |
| `orb_broad` | PASS | 24 → 24 | 0 → 0 | 0 | 0.00 | 20.00 |
| `orb_focused` | PASS | 32 → 32 | 0 → 0 | 0 | 0.00 | 20.00 |
| `orb_cancel` | PASS | 0 → 0 | — → 171.97 | 0 | 0.00 | 0.00 |
| `seal` | PASS | 0 → 0 | 0 → 0 | 0 | 0.00 | 18.00 |
| `seal_cancel` | PASS | 0 → 0 | — → 340 | 0 | 0.00 | 0.00 |
| `weave` | PASS | 0 → 0 | 210 → 210 | 0 | 22.00 | 20.00 |
| `weave_wall` | PASS | 0 → 0 | 0 → 0 | 0 | 0.00 | 20.00 |
| `bend_straight` | PASS | 28 → 28 | 0 → 0 | 0 | 0.00 | 22.00 |
| `bend_cancel` | PASS | 0 → 0 | — → 171.97 | 0 | 0.00 | 0.00 |

## FS-119 — Rendered controls and expedition follow-up

### Method

- Started the extracted RC14 with an isolated profile and rendering enabled. Issued a real Windows keyboard E to the game window and inspected the resulting greeting and progression prompt. Separately ran rendered D3D12 PIE with an isolated profile and a temporary driver delivering keyboard/mouse press and release events through Unreal's viewport input path. This checks the game bindings and input queue. It is simulated play, and the 12-class visual sweep used editor-only class selection fixtures to avoid replaying leveling. All actions in that sweep used viewport input.
- Captured one timed image per class at 761 × 441. Each sequence lasted about four seconds and included aim/strafe, primary attack, class ability inputs, and evade. The location carried over between class samples, so some attacks were rehearsed against a room boundary. The images verify the frames shown and provide a readability sample. They leave montage continuity, player comprehension, audio mix and performance during a long run open.
- [12-class image sheet](Saved/FS115QA/Visual119-ContactSheet.jpg) · Raw input schedules and telemetry are in `Saved/FS115QA/Visual119b.json`.

### Readability and controls findings

- **High · A bright white cube near the portal competes with the player and UI.** In the initial RC14 desktop view it was already visible before a contract was selected. During the Guild sequence it masked parts of spell effects or the silhouette in the Soul's Blade, Wizard, Paladin and Swordsman frames. Its asset identity was not established.
- **High · The altar's world text faces the wrong way from the initial approach.** At the RC14 fresh spawn, “Essence Altar” appeared mirrored. At another viewing angle it became nearly edge-on. Gerald's prompt gave a usable text fallback.
- **High at small viewport sizes · The bottom prompt clips horizontally.** At 761 × 441 the opening sentence and some contract prompts begin outside the frame. At the larger packaged display the same opening prompt was readable. Verify responsive layout at 720p, 1080p and the actual UI scale.
- **Medium · HUD contrast varies with the background.** Resource bars retain their solid backing, while white class text and yellow status text wash out over the bright portal/cube or strongly lit walls. The Guild screenshots show readable V/S/M when active and no inactive bars.
- **Medium · Status messages persist across later actions.** In the Guild samples a “Skill recovering” status remained on a Paladin or Mage frame while a new move was already being shown. As in the earlier leap miss case, a new action needs timely status clearance.
- **Camera.** Held right-mouse input reduced the 400 cm boom to about 312–317 cm within the short pulse for all twelve samples, and lateral movement was observed. Several near-wall frames brought the camera tight to the character. The minimum distance over dynamic narrow-corner turns and visual camera jitter were not measured.

### Visual notes by class

| Class | What the image and input trace establish | What still requires a longer view |
|---|---|---|
| Barbarian | Axe silhouette and lifted weapon read during Fury Leap. Only V is shown, and the leap timer starts. | Full lift-to-impact-to-landing transition, hit cue against a live target. |
| Juggernaut | Stonebreak's bright floor pulse reads at release. V stays full in Guild practice. | Distinguish short and full loads by pose and sound before releasing. |
| Berserk | Rage Pivot activates from the swing input and takes 24 S. The resulting motion is visible. | Compare left/right trajectories and readability of the cancel window. |
| Blood Mage | Recall timer appears after placing an anchor and the cast spends Mana. The anchor is difficult to isolate against the bright Guild background in this capture. | Read the outbound/recall path and anchor expiration while fighting. |
| Warrior | Round shield, guard timer and Stamina debit are visible. | Brace/advance silhouettes and protection cues during a hit. |
| Mercenary | A planted or thrust pose is visible at Paid Stand. The bright object cuts into the frame. | Tell this commitment apart from ordinary shielding and read the end of protection. |
| Swordsman | Sideways movement and Slip Cut input were recorded, with the sword and shield visible. | Contrast blade contact and slip motion from multiple camera angles. |
| Soul's Blade | Blue rectangular wave and 30 M debit are visible. The cube obscures the central overlap. | Read its actual width and truncation in a fight and near a wall. |
| Wizard | Orb starts cooldown and pays 20 M. The small capture and bright object mask the fired visual. | Side-by-side visual recognition of both Orb forms against darker backgrounds. |
| Paladin | Large luminous floor disk and `Seal 42.0` are readable. | Recognize the boundary and capacity change during combat. |
| Spell weaver | The trace shows substantial sideways motion, both S/M consumption and a visible step pose. | Compare launch within the step to the endpoint cast and read the heel trail. |
| Mage | Arcane Bend consumes M and the caster is framed close to a wall. This sample does not establish preview or trail clarity. | Mirrored curves around a pillar with the preview, launch and collision all visible. |



### Expedition runs

The navigation script reads the generated floor grid and steers the view to its chosen heading. Movement, altar selection, quest acceptance, combat, and return are keyboard/mouse viewport events. The expedition driver does not reposition actors, inject damage, disable AI or alter the save. Both runs start with a fresh profile. Camera steering uses controller rotation directly, so these runs cannot establish mouse sensitivity or corner jitter.

| Route | Outcome | Evidence |
|---|---|---|
| Barbarian, generated run 1 | **Complete.** Chose V at the altar, accepted the contract, entered the Crypt, cleared all six enemies including the Warden, crossed the exit and returned with R. | Victory at **38.4/200 V**. **280 XP** and **Crypt Shard** appeared on victory and after return. Guild entry restored **200 V**. The optional chest was not opened. [Victory frame](Saved/FS115QA/Run119cBarbarian/Saved/Screenshots/WindowsEditor/ScreenShot00000.png) |
| Wizard, generated run 1 | **Complete.** M selected with two Right events and E. Cleared the same generated layout and six enemies including the Warden, then entered the exit and returned with R. | Victory at **64/80 V**. **280 XP** and **Crypt Shard** survived the return, and Guild entry restored **80 V**. The optional chest was not opened. [Victory frame](Saved/FS115QA/Run119cWizard/Saved/Screenshots/WindowsEditor/ScreenShot00000.png) |

- Both runs reached the reward overlay and returned with **160 enemy XP + 120 contract XP**. The reward panel made completion clear. At the captured size, the old HUD and exit subtitle remain visibly layered behind it.
- A fresh editor process reopened each profile. Both restored the original class and ordered history, Level 2, 280 XP, one Crypt Shard, one pending essence choice and rewarded run ID 1. See `Saved/FS115QA/Run119cBarbarian-reload.json` and `Saved/FS115QA/Run119cWizard-reload.json`.
- The Barbarian controller closed to capsule contact against the Warden. The fight continued at roughly 77 cm separation and the character finished at 38.4 V. Wizard completed with 64 V after spacing while casting. These are single runs of different scripted tactics on the same seed. They do not establish relative class balance.
- No full expedition was driven on the extracted RC14 executable. The packaged evidence is rendered Guild appearance and actual Windows E input. The two full expeditions are **PIE_TESTED**, with simulated input and scripted navigation.
- Earlier failed automation attempts stopped in the harness due to input binding conversion or incorrect camera steering. They were discarded as fixture failures. The successful attempts used independent profiles.
- More coverage remains for all nine evolutions and Warrior in complete expeditions, real mouse motion and fast turns, physical gamepads, 45-second human play sessions, sound, chest interaction by native input, and rendered corner jitter at multiple resolutions.
- Removed the temporary input bridge after capture and rebuilt the Win64 Editor. The gameplay source matched its pre-test snapshot and the owner profile hash matched the baseline.

## FS-120 — Repairs and regression results

**COMPILED.** Win64 Editor target and `BP_CombatHUD` compiled. Windows Development RC15 cooked, staged, archived and loaded Guild with rendering enabled and a copied profile. The archive is `Saved/PackagedBuild/Fateshaped_RC15_Win64.zip` (757,340,381 bytes). The packaged check ends at Guild startup.

| Finding | Change and evidence |
|---|---|
| Blood Thread return | The outbound sweep reserved clearance for its wider return. The trace had identified a 4.67 cm initial overlap with generated wall geometry. The same target now took **16 + 28** at 140, 200, 235 and 300 cm, and in the outbound-wall fixture. Inserting a wall after outbound yielded only **16**, as did withholding recall. Seven of seven regression checks passed. |
| Weave Step interruption | Evade stays locked for both parts of the committed combo. Presses during and after the step both produced **210 cm**, spent **22 S** and allowed the Orb to spend **20 M**. |
| Neutral Backstep reversal | The step retains the Barbarian's body heading. Two presses with a fixed view and no movement produced **408 cm** net retreat. |
| Stale Fury Leap feedback | Action start clears the previous combat message. A wall-blocked leap dealt zero damage and showed `Fury Leap missed`. Close and far hits still dealt **40.8** and restored **20.4 V** in the main matrix. |
| Sanctuary Seal | Cast commit rechecks the captured floor. Spark, Orb and guard absorption use the same radius and height. The rendered ring shows the boundary with capacity and inside/outside text. Pulse and edge cues share the field lifetime. The main matrix verified normal cast and early cancel. A separate fixture disabled floor collision during windup. Mana stayed at 60 and the cast reported invalid ground, compared with 42 Mana for the valid control. |
| Arcane Bend | Preview and launch share the clamped heading, step calculation and release-time hand offset. The preview now marks the endpoint or a wall hit. A rendered sample shows the obstruction marker. The main matrix verified straight damage and pre-emission cancel. |
| Practice target filtering | Moving shots ignore hidden tagged actors. Fury Leap includes visible non-Pawn Guild practice targets and does not stop its travel or heal from that contact. |
| Guild readability and HUD | The portal uses the existing boards material and the inherited route cubes were removed. The altar label turns toward the camera. A 761 × 441 capture shows the full opening prompt and backed class text. Results draw before any gameplay HUD and return early. The scripted victory route saved 280 XP plus a Crypt Shard, then returned to Guild. The victory capture shows the contract reward and return controls without ordinary class HUD layers. |
| Placeholder animation and effects | Added timed guard and Stonebreak text, an immediate Fury Leap contact cue, and seal and Bend geometry cues. Dedicated guard and brace poses, a continuous Fury Leap montage and polished projectile effects remain open. |

The original 34-case class matrix passed **34 of 34** on the repaired module. The seven recall and three input checks are separate follow-ups. Raw evidence is in `Saved/FS115QA/Class120.json`, `Recall120b.json` and `Input120.json`. The rendered reward smoke used scripted travel and injected kill damage. Normal-input full RC15 expeditions, physical controllers, audio and detailed animation feel remain **NOT_TESTED**.

### Current Guild map follow-up

The Guild map changed after the first package. The updated hall actor pointed to a missing package after the mesh asset was renamed, and PIE showed the player dropping below the map. Rebound the existing actor to `GuildRoom`, aligned the mesh's lower bound to zero and set static triangle collision for the hall. Retained the new map layout and reapplied the portal's lower-glare material. The post-repair rendered fixture started grounded at Z 107 cm, used the portal, saved 280 XP and a Crypt Shard, and returned to Guild. See `Visual120Hall.json` and its screenshots. The owner save had changed between work periods. Follow-up tests used isolated profiles.

The focused Seal and dummy matrix passed **6 of 6** on the updated Guild. Spark and Fury Leap produced a contact effect on a visible non-Pawn dummy and no effect on its hidden counterpart. Fury Leap traveled the full 320 cm in both cases and Vitality stayed at 200. The time-sensitive floor change passed alongside the normal Seal cast. See `FocusedHall120c.json`.

**PACKAGED_TESTED (startup).** RC16 cooks and archives the current Guild and Crypt with the already compiled code. The extracted `Saved/PackagedBuild/Fateshaped_RC16_Win64.zip` package (754,571,488 bytes) loaded Guild with rendering enabled and an isolated copy of the current save. See `rc16_smoke.log`. The normal-input packaged route remains **NOT_TESTED**.
