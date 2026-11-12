# Fateshaped implementation tracker

**Current task** — Stage 2 at checkpoint (stop; Stage 3 needs owner approval). FS-002B, FS-003 and FS-005 are IMPLEMENTED + COMPILED, with logic observed in PIE; owner manual checks pending (keyboard/gamepad input, HUD look, telegraph readability). FS-009A is PACKAGED_VERIFIED (owner-verified). FS-002A is PIE_VERIFIED. **Blockers** — none. Spend: Stage 1 within its $4 cap (owner-confirmed); Stage 2 estimate roughly $3 to $4 of the $5 cap (no billing source; owner to reconcile). Account figures kept separate: dashboard credits $22.79, organization month-to-date spend $2.22 (owner-reported before Stage 2).

## Budget and checkpoints

| Stage | Scope | Stage cap | Cumulative cap |
| --- | --- | ---: | ---: |
| 0 | Audit | $1 | $1 |
| 1 | Playable skeleton and smoke build | $4 | $5 |
| 2 | Combat and resources | $5 | $10 |
| 3 | Evolution and full dungeon | $6 | $16 |
| 4 | Release | $3 | $19 |
| Reserve | Progression or packaging blockers | $5 | $24 |

Record the actual starting credit balance. Check headroom against the stage and cumulative caps before each batch of work. Use session cost estimates if available and label them as estimates. At each checkpoint, reconcile against actual account usage or ask the owner for the current balance. If the balance is lower than planned, revise the remaining caps with the owner. Checkpoints are spending decisions. Stop after each stage, before the next batch would risk exceeding a cap, or if spend cannot be established at the checkpoint. Moving funds between stages or using reserve requires owner approval.

**Owner setup and checkpoint.** Before the first prompt, use `/status` to confirm the intended model and account, and record the balance in the billing workspace that holds the jam credits. At checkpoints, use `/usage` (alias `/cost`) for the session estimate and the billing workspace for confirmed project spend. Use `/context` if tools or history take up excessive context. Save the checkpoint before starting a fresh session for a new stage if the old history is no longer useful. These caps guide work and do not enforce billing limits. [Claude Code costs](https://code.claude.com/docs/en/costs) · [Commands](https://code.claude.com/docs/en/commands).

Submission deadline **2026-11-16 03:59 Europe/Kyiv**. Freeze features and start final packaging by **2026-11-15 22:00 Europe/Kyiv**, with time to test the extracted archive and upload. If time or budget tightens, skip optional work, reuse the melee enemy, and shorten the route. Keep combat, one meaningful evolution, a reachable ending, and a playable Windows build.

## Status and evidence

Each item starts at `TODO`. Record the highest confirmed state for the current item and cite brief evidence in the latest devlog entry.

| State | Evidence |
| --- | --- |
| `IMPLEMENTED` | Change is in source or saved Unreal assets. |
| `COMPILED` | Relevant Blueprints or code compile. |
| `PIE_VERIFIED` | Play In Editor behavior was observed against the item's acceptance criteria. |
| `PACKAGED_VERIFIED` | The relevant behavior was observed in the standalone packaged executable. |
| `VERIFIED` | Audit or non-runtime checklist evidence is recorded. |
| `BLOCKED` | Record the cause, last confirmed state, and next diagnostic or owner action. |

Record test scope, actual result, and any untested requirement. A smoke test verifies only its stated scope. Advance when the stage acceptance criteria pass. After major integration, check affected Blueprint errors, missing references, and relevant runtime errors. If the owner must perform a test, give exact steps and wait for their result before advancing its verification state.

## Stage 0 — Audit

**FS-001 · Project audit.** Locate the `.uproject`, verify the engine version where available, and identify the startup map, playable template, and reusable assets from a targeted inspection. Follow the read-only Git checks in `CLAUDE.md`. Verify the connected MCP with one harmless read and note useful capabilities or limits from targeted tool discovery. Record existing runtime evidence if available and leave new runtime tests for the next stage. Stage 0 may update the tracker and devlog.

**Checkpoint.** Record confirmed project facts, limitations, blockers, and the smallest stage 1 task. Record actual spend and stop.

**FS-001 result (VERIFIED, 2026-11-12).** UE 5.8, Blueprint-only (`Source/` empty), no project assets in `Content/`, startup map is the engine OpenWorld template, no GameMode or pawn set, no build or runtime evidence. Git `main` with `origin` and LFS (`Content/**`) ready. MCP read OK (`get_current_level` returned `/Temp/Untitled_1`). Smallest Stage 1 step: FS-002A, using Unreal MCP to add the Third Person template content, or a minimal map with a Character, GameMode and camera if the template is unavailable. Then save a short route map `/Game/Maps/L_Route` with a placeholder finish and set it as the Game Default Map. Owner approval for Stage 1 is required. Config and Git were not modified.

## Stage 1 — Playable skeleton and smoke build

- **FS-002A · Controller baseline.** Reuse the third-person template or existing controller. Confirm player spawn, movement and camera. Set up a short route with an obvious placeholder finish.
  - *Status: PIE_VERIFIED (owner-verified, 2026-11-12).* The owner played `L_Route` in PIE and confirmed that movement, jumping and camera control work with keyboard and mouse and with a gamepad, and that they reached the finish platform and pillar from the spawn point. This is a manual owner test, not an automated one. No separate Blueprint compile check was run.
  - *Implementation:* Template content copied from `C:\Games\UE_5.8\Templates` into `Content/ThirdPerson` (BP character, game mode, controller, MI), `Content/Input` and `Content/Characters/Mannequins` (Pistol, Rifle and Death anims left out). Map `/Game/Maps/L_Route` was created from `Template_Default`, with route blocks at x=800 and x=1500 and a finish pillar and pad at x=2500. `DefaultEngine.ini` now sets GameDefaultMap, EditorStartupMap and GlobalDefaultGameMode (`BP_ThirdPersonGameMode_C`).
  - *Observed in PIE:* `BP_ThirdPersonGameMode`, `BP_ThirdPersonPlayerController` and `BP_ThirdPersonCharacter` spawned at the PlayerStart (0,0,91.65), grounded. The character has CameraBoom, FollowCamera and CharMoveComp.
  - *Not run:* Blueprint compile and error checks.
  - *Git:* changes are uncommitted. `Content/Maps/L_Route.umap` shows as staged "A" in the index; I did not stage it.
- **FS-009A status: PACKAGED_VERIFIED (owner-verified, 2026-11-12).** The owner ran the packaged Windows build manually: the player spawns, movement, jumping and camera work, and the finish platform is reachable. Manual owner test, not automated. Stage 1 is within budget per the owner. Account figures (not Stage 1 spend): $22.79 credits, $2.22 organization month-to-date spend.
  - *Earlier status:* IMPLEMENTED (packaged build exists), owner test pending.
  - *Build method (release record):* `C:\Games\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat BuildCookRun -project=C:\GameProjects\Fateshaped\Fateshaped.uproject -noP4 -platform=Win64 -clientconfig=Development -build -cook -stage -pak -archive -archivedirectory=C:\GameProjects\Fateshaped\Saved\PackagedBuild -map=/Game/Maps/L_Route -utf8output -unattended -nosplash`. It took about 3 min and ended with BUILD SUCCESSFUL (attempt 5). Output is about 1 GB.
  - *Executable:* `C:\GameProjects\Fateshaped\Saved\PackagedBuild\Windows\Fateshaped.exe` (git-ignored via `Saved`).
  - *Fixes needed since the earlier blocker:* the new Windows SDK 10.0.26100.0 was accepted. The editor's Live Coding stopped the build ("Unable to build while Live Coding is active"), so I set the user-level editor preference Live Coding `bEnabled` to false for the build and restored it to true afterwards. The cook then exited with code 1 although all 575 packages were cooked, because of a "no Asset Manager rule for GameFeatureData" error. I added `[/Script/Engine.AssetManagerSettings]` with a `GameFeatureData` `PrimaryAssetTypesToScan` entry to `Config/DefaultGame.ini` and the cook passed.
  - *Observed in the packaged exe, launched outside the editor with `-windowed -log` (log `Saved/PackagedBuild/run.log`):* browse and LoadMap `/Game/Maps/L_Route`, "Game class is BP_ThirdPersonGameMode_C", the world came up for play, and the log had 0 "Error:" lines. I stopped the process afterwards.
  - *Not observed:* spawn, movement, camera and reaching the finish in the packaged build, because the tools can't hold input keys. Owner steps: run the executable above, check that the character appears on the grey floor with a third-person camera, move with WASD, mouse look and space, and walk about 25 m along +X to the tall pillar and pad. Report whether it all works. After that I'll mark FS-009A PACKAGED_VERIFIED.
  - *Earlier blocker (resolved):* attempts 1 to 4 failed on the missing SDK, then Live Coding, then the GameFeatureData cook error. Logs are `Saved/PackagedBuild/uat_attempt1.log` to `uat_attempt4.log` and `uat.log`.
  - *Method tried:* `RunUAT.bat BuildCookRun -project=Fateshaped.uproject -platform=Win64 -clientconfig=Development -build -cook -stage -pak -archive -archivedirectory=<project>\Saved\PackagedBuild -map=/Game/Maps/L_Route` (engine at `C:\Games\UE_5.8`).
  - *Attempt 1 (with `-build`):* failed in UnrealBuildTool with "Platform Win64 is not a valid platform to build. SDK validation failed: Sdk: not found. Required version 10.0.19041.0". `C:\Program Files (x86)\Windows Kits\10` has no `Include` folder, and `Program Files\Microsoft Visual Studio\2022` has no `VC\Tools\MSVC`.
  - *Attempt 2 (without `-build`):* failed with a missing `Binaries\Win64\FateshapedEditor.target`; UAT needs the project targets built even for a Blueprint-only project.
  - *Not changed:* no installs and no Git changes. Logs are in `Saved/PackagedBuild/` (git-ignored).
  - *Owner action:* install "Desktop development with C++" in Visual Studio 2022 with the Windows 10 SDK 10.0.19041 (or newer 10.0.22621, if UE 5.8 accepts it) and the MSVC v143 toolset, then tell me to retry. This is a system dependency, so I did not install it without approval. Alternatively package from the editor: Platforms > Windows > Package Project, output `C:\GameProjects\Fateshaped\Saved\PackagedBuild`; that needs the same toolchain.
  - *Untested:* packaged startup map, spawn, movement, camera and finish.
- **FS-009A · Early Windows smoke build.** Confirm Game Default Map, GameMode and player pawn. Package Windows x64 and launch the executable outside the editor. Observe startup map, spawn, movement, camera, and reachable finish. Record the build method and output location for release. Report exact blockers if packaging or launch fails.

**Checkpoint.** A packaged executable can be started and the short route completed. Scope the test to the skeleton.

## Stage 2 — Combat and resources

- **FS-002B · Combat controls.** Wire basic attack and dodge or dash input. Use a small animation or feedback fallback where assets are missing. Confirm inputs trigger once per action and actions are easy to read.
- **FS-003 · V/S/M.** Add health and damage, stamina spending and recovery, and the mana pool and spending behavior from `GAME_DESIGN.md`. Reuse a small resource component if it simplifies implementation. Verify all three HUD values are visible, stamina depletion blocks dodge with feedback, and basic melee remains available. Verify the magic attack spends mana in FS-006.
- **FS-005 · Enemy combat.** Implement one melee enemy with detection, approach, telegraph, attack, recovery, and death. Verify hits and readable feedback in both directions, then defeat the enemy. On a separate run, die and verify restart restores the starting state.

**Stage 2 result (2026-11-12).**
- *Implementation:* `Content/Combat/BP_PlayerCombat` (child of `BP_ThirdPersonCharacter`, set as GameMode default pawn), `BP_MeleeEnemy` (Character, placed once in `L_Route` at x=1150), `BP_CombatHUD` (HUD class set in `BP_ThirdPersonGameMode`; draws V/S/M bars, numbers and a message line). Controls: LMB attack, Left Shift dodge, R restart when dead (raw key events, no gamepad binding yet). Tuning: V 100, S 100 (+25/s, dodge costs 25, 0.3 s dodge with damage immunity), M 0 of 100 (altar grants it in Stage 3), attack 25 damage, enemy 60 HP, enemy hit 20, 0.7 s telegraph, 0.9 s recovery.
- *Placeholder feedback:* no attack animations exist. Attack, enemy telegraph and hit feedback use debug spheres and debug text, and the enemy uses the Quinn mesh in T-pose. These show in editor, PIE and Development builds, not Shipping.
- *Compiled:* all three Blueprints compiled with no errors reported; no Blueprint runtime errors in the PIE log.
- *Observed in PIE (state read from the running pawn and enemy):* the enemy chased, telegraphed and hit the player (V 100 to 60 to 0). With an auto-attack test flag the enemy took hits, was defeated and removed, and the player lost only 20 V. With an auto-dodge flag, S fell to about 0, dodge was refused with "Not enough stamina!", and S recovered. With an auto-restart flag, V 0 and the death message appeared, then the level reloaded with V 100, `bDead` false, message cleared and the enemy back at 60 HP.
- *Test hooks:* `bAutoAttack`, `bAutoDodge` and `bAutoRestart` on `BP_PlayerCombat` (default false) call the same `DoAttack`, `DoDodge` and `DoRestart` events from Tick. They were needed because the tools cannot send keys to the PIE viewport (a Slate `PressKey` of R did not restart the game). They bypass the key bindings, so those are untested.
- *Not observed:* real keyboard or mouse input, HUD appearance (the editor capture was covered by the Terminal panel), debug-draw readability, dodging an actual telegraphed hit, packaged-build behavior. The magic attack and M spending belong to FS-006; M only exists as a displayed pool at 0.
- *Owner manual check (PIE, `L_Route`):* press Play; check the three bars top-left. Walk toward the enemy at x=1150. Click LMB to swing (yellow sphere) and confirm the enemy shows HIT and is defeated after 3 hits. Press Left Shift to dodge during the red telegraph sphere. Press Left Shift repeatedly to see the stamina message. Let the enemy kill you, then press R and confirm everything resets. Report any input that fails.
- *HUD fix (2026-11-12, follow-up):* owner saw no HUD in PIE. Checked: `L_Route` has no GameMode override (`defaultGameMode` None) and the active GameMode is `BP_ThirdPersonGameMode_C`. Cause confirmed: the live PIE GameMode had `HUDClass = /Script/Engine.HUD` and the spawned HUD actor was the base `HUD_0`, so `BP_CombatHUD` never ran, even though the class defaults read back as `BP_CombatHUD` (the earlier setting had not persisted into the saved asset). Fix: re-set `HUDClass` on `BP_ThirdPersonGameMode`, compiled it and saved it. Rerun in PIE: the HUD actor is now `BP_CombatHUD_C_0` and the player controller and pawn are unchanged; no Blueprint runtime errors in the log. *Not observed:* the drawn bars. `CaptureViewport` returned the editor level camera (PlayerStart icon, enemy), not the PIE view, and I could not read draw output. The draw event reading the pawn is unconfirmed beyond the cast-and-read logic compiling. **Owner to confirm** the V/S/M bars (top-left) and message line are visible in PIE.
- *Known issue:* the enemy notices the player from the spawn point (1200 units) and attacks at once; move it or shorten the range if the first encounter feels too abrupt.

**Checkpoint.** Observe movement, attack, dodge, damage, stamina behavior, enemy death, and player death/restart in PIE. Log mana behavior that is present and carry the magic attack test into stage 3.

## Stage 3 — Evolution and full dungeon

- **FS-004 · First essence.** Offer the working S choice at the entrance, apply Swordsman, and record S as the first essence. Verify selection activates the expected class and controls.
- **FS-006 · Evolution.** After the first encounter, allow the altar's M choice once per run. Record the ordered history `[S, M]`, show the Spellsword change and ability input, and unlock the magic strike. Verify that the strike is unavailable before evolution, consumes M afterward, and visibly damages an enemy. Check insufficient Mana feedback and continued access to the basic attack. Verify that the altar cannot be used twice and restart resets the evolution.
- **FS-007 · Dungeon progression.** Use a fixed entrance, first combat area, altar, final fight, and exit. Encounter completion unlocks the next required step. Verify the altar and exit gates, including that the exit opens only after the final encounter.
- **FS-008 · End-to-end PIE.** Start fresh, choose S, win the first fight, evolve at the altar, use the new ability in the final fight, win, and reach the ending. Verify death and restart on a separate run. Complete both paths using normal player controls.

**Checkpoint.** Record an uninterrupted successful PIE playthrough and restart result. Package the integrated build next.

## Stage 4 — Package and submit

**FS-009B · Release candidate.** Package Windows x64, archive it, and test the extracted ZIP outside the editor. Check the intended map, GameMode, spawn, input, HUD, enemy behavior, evolution, exit and ending. Repeat FS-008 from the extracted executable. Check for missing assets or relevant runtime errors. If a fix changes the candidate, rebuild and rerun the affected checks. Verify the final candidate end to end.

**Checkpoint.** Record the exact tested build/archive path, size, source revision, and observed tests. Target a ZIP under 1 GB when practical. If larger, check the submission limit and allowed upload method.

## Optional polish

Consider these one at a time after the packaged full loop passes, with time and room in the working budget before feature freeze. Retest the final archive after polish.

- **FS-010 · Guild and fairy.** Brief guide dialogue, portal and quest-board set dressing if the existing entrance supports them. Use the demo presentation target in `GAME_DESIGN.md` for an optional return or extra room.
- **FS-011 · Presentation.** Prioritize useful combat sound, hit feedback, evolution effects, lighting and readable UI.
- **FS-012 · Optional encounter variation.** A second enemy archetype, such as a ranged unit or elite variant, if it can reuse working combat. Defer director, boss and reward systems to `POST_JAM_BACKLOG.md`.

## Submission checklist

- [ ] Verified Windows archive ready for the owner to submit.
- [ ] Gameplay clip of roughly 30–60 seconds showing combat and the Spellsword evolution, and itch.io page if required by the jam. Prepare these from a stable candidate while there is time to upload.
- [ ] Credits, external asset licenses, and AI tool disclosure prepared.
- [ ] Owner confirms final upload and jam submission.
