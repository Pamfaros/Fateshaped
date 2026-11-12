# Fateshaped implementation tracker

**Current task** — FS-001, stage 0, TODO. **Blockers** — none recorded.

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

## Stage 1 — Playable skeleton and smoke build

- **FS-002A · Controller baseline.** Reuse the third-person template or existing controller. Confirm player spawn, movement and camera. Set up a short route with an obvious placeholder finish.
- **FS-009A · Early Windows smoke build.** Confirm Game Default Map, GameMode and player pawn. Package Windows x64 and launch the executable outside the editor. Observe startup map, spawn, movement, camera, and reachable finish. Record the build method and output location for release. Report exact blockers if packaging or launch fails.

**Checkpoint.** A packaged executable can be started and the short route completed. Scope the test to the skeleton.

## Stage 2 — Combat and resources

- **FS-002B · Combat controls.** Wire basic attack and dodge or dash input. Use a small animation or feedback fallback where assets are missing. Confirm inputs trigger once per action and actions are easy to read.
- **FS-003 · V/S/M.** Add health and damage, stamina spending and recovery, and the mana pool and spending behavior from `GAME_DESIGN.md`. Reuse a small resource component if it simplifies implementation. Verify all three HUD values are visible, stamina depletion blocks dodge with feedback, and basic melee remains available. Verify the magic attack spends mana in FS-006.
- **FS-005 · Enemy combat.** Implement one melee enemy with detection, approach, telegraph, attack, recovery, and death. Verify hits and readable feedback in both directions, then defeat the enemy. On a separate run, die and verify restart restores the starting state.

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
