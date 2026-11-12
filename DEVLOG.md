# Fateshaped development log

## Current state

| Field | Last confirmed |
| --- | --- |
| Engine version / MCP implementation | UE 5.8 (`EngineAssociation` in `Fateshaped.uproject`; not checked against the running editor) / Unreal MCP via `ModelContextProtocol` + `AllToolsets` plugins, reachable |
| Startup map | `/Game/Maps/L_Guild` (GameDefaultMap and EditorStartupMap); GameInstance `BP_FS_GameInstance_C`; GlobalDefaultGameMode `BP_ThirdPersonGameMode_C` |
| Verified submission candidate | `Saved/PackagedBuild/Fateshaped_RC1_Win64.zip` (532 MB, Win64 Development), PACKAGED_VERIFIED by the owner for the full playthrough and death/restart in the extracted build |
| Latest verified build | Skeleton build `Saved/PackagedBuild/Windows/Fateshaped.exe` (2026-11-12), PACKAGED_VERIFIED by the owner for spawn, movement, jumping, camera and finish (pre-combat) |
| Account (separate from stage spend) / stage spend | Total spend $8.29, remaining credits $16.71, $1.02 since the previous checkpoint (owner-reported 2026-11-12) / Stage 0 under $1, Stage 1 within $4 cap (owner-confirmed), Stage 2 within $5 cap (estimate), Stage 3 estimate about $3 to $4 of $6, Stage 4 $1.02 of $3 (owner-reported) |
| Latest candidate build | `Saved/PackagedBuild/Fateshaped_RC2_Win64.zip` (501 MB), extracted startup observed, owner playthrough pending |
| Known blockers | Sound import needs an owner step (see `TASKS.md`) |

Refresh a field when its evidence changes. Read `TASKS.md` for the current task, status, and acceptance criteria.

## Entries

Add a short entry per checkpoint or blocker, newest first. Capture the test result and any decision that changes the plan. Keep recurring facts in Current state.

> **2026-11-12 · Redesign / FS-101 · PIE_TESTED via tools (budget checkpoint)**
> The owner replaced the design with a persistent guild loop; `GAME_DESIGN.md`, `CLAUDE.md`, `TASKS.md` and `POST_JAM_BACKLOG.md` updated. Added `BP_FS_SaveGame`, `BP_FS_GameInstance`, `L_Guild` and `L_Dungeon_01` (copies of `L_Route`), and set the GameInstance class and default map.
> PIE: intro once, quest accepted, portal to the dungeon, completion granted 120 XP, level 2, one pending essence choice and a Crypt Shard exactly once (repeated completion calls did nothing), return to the guild, and the same values after a PIE restart with the intro skipped.
> Not tested: real input, packaged build. Essence selection is still inside the dungeon (FS-102). Spend unknown; stopped for the owner's budget check. No commit; the index is untouched.

> **2026-11-12 · Polish / FS-011, FS-010, RC2 · IMPLEMENTED + COMPILED, PIE logic observed, RC2 startup observed (checkpoint)**
> The owner approved a $9 polish cap. Added attack, dash and enemy wind-up animations via `DefaultSlot`, a telegraph circle that matches the hit check, hit sparks, ragdoll deaths, an Arcane Wave slab that matches its hit box, a practice dummy with a Mana refund, the class-change banner, HUD prompts, world labels, victory and death screens with R/Esc, and the FS-010 board and Gerald line. Removed player-facing debug draws. Fixed placed enemies that were missing their anim class.
> PIE (test flags and teleports): S choice, guardian telegraph and strike, death and full reset, evolution, practice refund, one wave hitting both lined-up enemies, Mana exhausted then a melee victory. No runtime errors after the fix.
> RC2: `Saved/PackagedBuild/Fateshaped_RC2_Win64.zip` (501 MB); the extracted exe starts on `L_Route` with no errors. Not verified: real input, visuals, timing, the R/Esc actions and RC2 gameplay. Sounds are not imported (editor auto-import did not fire; no MCP import tool); the hooks are in place.
> Spend: estimate about $5 to $7 of $9 (no billing source). No commit; the index is untouched. Next: owner imports the WAVs (optional) and plays RC2.

> **2026-11-12 · Stage 4 / FS-009B · PACKAGED_VERIFIED (owner-verified), Stage 4 closed**  
> The owner manually verified the full playthrough and death/restart in the extracted RC1 build. `Saved/PackagedBuild/Fateshaped_RC1_Win64.zip` is recorded as the verified submission candidate (Win64 Development). Account totals (owner-reported): $8.29 spent, $16.71 remaining, $1.02 spent since the previous checkpoint. Archive and Git index preserved. Publication and jam submission stay with the owner.

> **2026-11-12 · Stage 4 / FS-009B · IMPLEMENTED, extracted-build startup observed (checkpoint)**  
> Packaged the full game with the FS-009A UAT method (Live Coding disabled for the build and restored). ZIP `Saved/PackagedBuild/Fateshaped_RC1_Win64.zip` (532 MB), extracted to `Saved/PackagedBuild/Extracted_RC1/`. The extracted exe launched outside the editor: LoadMap `L_Route`, `BP_ThirdPersonGameMode_C`, alive after 30 s, no Error lines or Blueprint runtime errors (only optional profiler DLL load messages).  
> Not verified: the playthrough, input, HUD, ending and death/restart in the extracted build. Owner steps are in `TASKS.md`. Spend: Stage 4 estimate about $1 of $3 (no billing source). No commit; index untouched.

> **2026-11-12 · Stage 3 · PIE_VERIFIED (owner-verified)**  
> The owner manually verified the full Stage 3 route and death/restart in PIE: FS-004, FS-006, FS-007 and FS-008 are PIE_VERIFIED. Account totals (owner-reported, separate from the Stage 3 estimate): $7.27 total spend, $17.73 remaining credits. Stage 4 approved with a $3 cap, FS-009B only.

> **2026-11-12 · Stage 3 / FS-004, FS-006, FS-007, FS-008 · IMPLEMENTED + COMPILED, route observed in PIE (checkpoint)**  
> Connected the route in `L_Route`: S pedestal, guardian, M altar, two-enemy final fight, exit gate, ending. Logic lives in `BP_PlayerCombat` (E interact, Q magic strike, tick world update) with tagged level actors; the HUD gained Class and Essence lines. Blueprints compiled; no runtime errors in the log.  
> PIE (live pawn state, driven by instance test flags and pawn teleports): S choice, locked and ready altar messages, evolution to `S, M` with M 60, one-use altar, magic strike spending M 60 to 0 with "Not enough mana!" and killing the final enemies, exit opening and gate hiding, victory only after the exit opened, and death/restart resetting essence, V and enemies.  
> Not observed: real key input, hand-walked route, readability of HUD and debug text, the gate's physical block. Gamepad has no E/Q binding. Owner steps are in `TASKS.md`.  
> Spend: Stage 3 estimate about $3 to $4 of the $6 cap (no billing source); account figures separate. No commit; index and Git config untouched. Next: owner playthrough, then approval for Stage 4.

> **2026-11-12 · Stage 2 · PIE_VERIFIED (owner-verified)**  
> The owner manually verified in PIE on `L_Route`: HUD after the fix, attack, dodge, stamina recovery, and death/restart. Stage 3 approved with a $6 cap. Magic attack and M spending are the first Stage 3 work.

> **2026-11-12 · Stage 2 / FS-003 · HUD fix, owner confirmation pending**  
> No HUD in PIE. No map GameMode override; the live GameMode had `HUDClass = Engine.HUD` and the active HUD actor was base `HUD_0`, so `BP_CombatHUD` was never spawned (the earlier setting had not persisted). Re-set `HUDClass` on `BP_ThirdPersonGameMode`, compiled and saved. PIE now spawns `BP_CombatHUD_C_0`; no runtime errors in the log.  
> Not observed: the bars themselves (the viewport capture shows the editor camera, not PIE). Owner to confirm V/S/M visibility. Only the GameMode asset changed; spend is small and within the Stage 2 cap (estimate). No commit; index not touched.

> **2026-11-12 · Stage 2 / FS-002B, FS-003, FS-005 · IMPLEMENTED + COMPILED, logic observed in PIE (checkpoint)**  
> Added `Content/Combat/` `BP_PlayerCombat`, `BP_MeleeEnemy`, `BP_CombatHUD`; GameMode now uses the new pawn and HUD; one enemy placed in `L_Route`. Blueprints compiled with no errors and the PIE log has no runtime errors. Fixed a bug found in PIE where health was recomputed after being set, so death triggered at 20 V.  
> PIE (read from live objects): the enemy chased, telegraphed and hit the player; auto-attack defeated the enemy; auto-dodge drained S, was refused with feedback and S recovered; death showed the message and the restart reloaded to V 100 and the enemy at 60 HP.  
> Not observed: real keyboard/mouse input (tools can't reach the PIE viewport; test flags drive the same events), HUD look, readability of the debug-draw feedback. Mana is a 0/100 pool only; the magic attack stays in Stage 3. Owner steps are in `TASKS.md`.  
> Spend: Stage 2 estimate about $3 to $4 of the $5 cap (no billing source); account figures stay separate.  
> No commit. Index: `Content/Combat/BP_CombatHUD`, `BP_MeleeEnemy` and `BP_PlayerCombat` appeared as staged (editor source-control add, not by me); the last two also have unstaged edits, so I left them. Next: owner checks, then approval for Stage 3.

> **2026-11-12 · Stage 1 / FS-009A · PACKAGED_VERIFIED (owner-verified)**  
> The owner manually ran the packaged Windows build: spawn, movement, jumping and camera work, and the finish platform is reachable. Stage 1 is within budget. Account figures, recorded separately from stage spend: $22.79 credits, $2.22 organization month-to-date spend. Stage 2 approved with a $5 cap.

> **2026-11-12 · Stage 1 / FS-009A · IMPLEMENTED (packaged build, owner test pending)**  
> After the owner installed MSVC and Windows SDK 10.0.26100.0, UAT `BuildCookRun` (Win64 Development, `-build -cook -stage -pak -archive`) passed on the fifth attempt, after two fixes: Live Coding temporarily disabled in editor preferences and restored afterwards, and a `GameFeatureData` Asset Manager entry added to `Config/DefaultGame.ini` to clear a cook error. Output: `Saved/PackagedBuild/Windows/Fateshaped.exe` (about 1 GB).  
> Launched outside the editor: the log shows LoadMap `/Game/Maps/L_Route` and Game class `BP_ThirdPersonGameMode_C` with 0 errors. Spawn, movement, camera and finish are not yet observed in the packaged build (owner steps in `TASKS.md`).  
> Spend: Stage 1 estimate is still within the $4 cap, but the repeated builds used more than expected (no billing source).  
> No commit. The Git index is unchanged (only `Content/Maps/L_Route.umap` staged). New untracked `Build/` (UBT file-open-order logs) is left unstaged for you to decide on. Next: owner runs the packaged exe, then FS-009A can go to PACKAGED_VERIFIED.

> **2026-11-12 · Stage 1 / FS-009A · BLOCKED (packaging toolchain)** (resolved above)  
> Ran UAT `BuildCookRun` for Win64 Development (`-cook -stage -pak -archive`, map `/Game/Maps/L_Route`, archive dir `Saved/PackagedBuild`). With `-build`, UnrealBuildTool failed: Windows SDK 10.0.19041.0 not found, and no MSVC toolchain under VS 2022. Retry without `-build` failed on the missing `FateshapedEditor.target`. Nothing was packaged or launched. Logs: `Saved/PackagedBuild/uat_attempt1.log` and `uat.log`.  
> Spend: Stage 1 estimate still within the $4 cap (no billing source).  
> No commit. The Git index is unchanged (only `Content/Maps/L_Route.umap` staged, as before). Next: owner installs VS 2022 C++ workload and the Windows SDK, then retry FS-009A.

> **2026-11-12 · Stage 1 / FS-002A · PIE_VERIFIED (owner-verified)**  
> The owner manually tested `L_Route` in PIE. Movement, jumping and camera control work with keyboard and mouse and with a gamepad. They reached the finish platform with the pillar from the spawn point. No separate Blueprint compile check was run. The earlier entry below shows the tool-observed spawn.

> **2026-11-12 · Stage 1 / FS-002A · IMPLEMENTED (PIE spawn only)**  
> Copied the Third Person template content from `C:\Games\UE_5.8\Templates` into `Content/ThirdPerson`, `Content/Input` and `Content/Characters/Mannequins` (about 98 MB; Pistol, Rifle and Death anims skipped). The copy was a file copy because MCP has no template-import tool. Created `Content/Maps/L_Route.umap` from `Template_Default` with route blocks and a finish pillar and pad at x=2500. Set the startup map and default GameMode in `Config/DefaultEngine.ini` through ConfigSettingsToolset, then saved all assets.  
> PIE: GameMode, PlayerController and `BP_ThirdPersonCharacter` spawned at PlayerStart (0,0,91.65), grounded; CameraBoom and FollowCamera exist. Movement, camera view and the finish were not observed, because `PressKey` cannot hold a key and the editor capture was covered by the Terminal panel. Dependencies of the character BP resolve. No Blueprint compile or error check was run. Pending owner PIE test (steps in `TASKS.md`).  
> Spend: Stage 1 so far is an estimate within the $4 cap, with room left for FS-009A (no billing source).  
> No commit: `Content/Maps/L_Route.umap` is already staged in the index (not by me) and the changes are mixed, so I left everything unstaged. Next: owner PIE test, then FS-009A.

> **2026-11-12 · Stage 0 / FS-001 · VERIFIED (audit)**  
> Read-only audit; only `TASKS.md` and `DEVLOG.md` edited. Project: `Fateshaped.uproject`, UE 5.8; plugins ModelingToolsEditorMode, ModelContextProtocol, AllToolsets, Terminal. `Source/` is empty (Blueprint-only), `Plugins/` absent. `Content/` has no assets, only stray `Content/.claude/settings*.json`, which are LFS-tracked because of the `Content/**` rule. No third-person template, controller, or reusable assets found. No packaged build or runtime evidence. Latest `Saved/Logs` holds only AutoSDK files.  
> Git: branch `main`, remote `origin` https://github.com/Pamfaros/Fateshaped.git, git-lfs 3.6.1, `.gitattributes` sends `Content/**` to LFS; `.mcp.json` is ignored. Latest commit `9e72d94`; only my two doc edits are modified.  
> MCP: `list_toolsets` listed the toolsets. One read, `get_current_level` in `editor_toolset.toolsets.scene.SceneTools`, returned `/Temp/Untitled_1`. Limits: `call_tool` takes the short tool name plus `toolset_name`; the fully prefixed name failed with "Unknown tool". Useful toolsets: Scene, Asset, Blueprint, Object, Actor, ConfigSettings, EditorApp (PIE control), Logs, UMG, DataTable, Automation. No PIE, packaging, or write calls tested.  
> Stage spend / cumulative: estimate under $1 / under $1 (no billing source).  
> No commit; edits left unstaged for owner review. Next: FS-002A.

## Release records

- **Assets and licenses** — Record third-party asset name, source, license, and required credit when used.
- **AI tools used** — Claude Code, Unreal MCP. Sound effects in `Saved/SFXSource/` were synthesized procedurally by a Claude-written script (no third-party audio). Add other tools when used for the project.
- **Latest tested archive** — `Saved/PackagedBuild/Fateshaped_RC1_Win64.zip`, 532 MB, built from the working tree on top of the latest commit at build time (uncommitted docs and a tracked build log); owner-verified extracted playthrough and death/restart (Stage 4 checkpoint, 2026-11-12).
