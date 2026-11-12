# Fateshaped — Claude Code instructions

Build a complete, playable Windows x64 vertical slice for the 2026 Epic AI MegaJam. A task must advance playable progression, combat, the S → M evolution, essential feedback, or release reliability. Protect the submission deadline in `TASKS.md`.

## Context and budget

- Use the configured Sonnet 5.5 session at medium effort when available. Work on one task at a time with one agent. Escalate effort only for a specific blocker with budget headroom. Keep output compact and use deterministic local commands for routine checks.
- Start with the current FS task and budget in `TASKS.md`. Use heading search and line ranges for the current task or stage, relevant sections of `GAME_DESIGN.md`, and the latest `DEVLOG.md` entry when resuming. Read `Implementation notes` when choosing architecture and `Submission checklist` during release. Keep supporting files out of automatic imports. Reuse facts already confirmed in this session.
- Work within the stage and cumulative caps. Stop at the stage checkpoint, before a batch likely to exceed its cap, or when usage cannot be reconciled at the checkpoint. Owner approval is required to proceed to the next stage, reallocate caps, or use the reserve. Treat usage estimates as estimates.
- Keep the playable baseline before optional polish. Keep procedural generation, RL/ML, co-op, economy, and additional hybrids in `POST_JAM_BACKLOG.md`.

## Unreal workflow

- Inspect the existing implementation and define the observable result. Reuse working assets and engine systems. Prefer Blueprints and add C++ only when it resolves a concrete blocker more simply. Preserve runtime independence from Unreal Editor and MCP.
- Use the connected Unreal MCP for supported editor work. Probe with a narrow read during the audit. Discover only the next needed tool and its schema. Request bounded fields or counts where supported. Avoid full schema dumps, recursive Content scans, and complete log reads. Filter logs by file, asset, or error code and start with up to 40 relevant lines. Expand for a specific diagnostic question.
- Make editor writes sequentially. Confirm asset references, save affected assets, compile touched Blueprints or code, then test the task's acceptance criteria. Edit `.uasset` and `.umap` through Unreal. After a failed call, inspect the error and retry once only after a material correction. If still blocked, record the limitation and use a supported fallback or give the owner exact test steps.
- Use the verification states in `TASKS.md`. Record what was actually observed, including any pending player or packaged tests. Update the current task after a meaningful state change and add one concise devlog entry per checkpoint or blocker.

## Git and owner approval

- Check the branch, worktree, and index before changing files. During the audit, also check remotes, relevant ignore rules, and LFS availability or tracking without printing credentials. Preserve owner changes and existing Git configuration.
- Commit verified work locally when Git is ready. Stage explicit paths, review the staged diff or binary inventory, and check existing LFS rules for affected Unreal assets. If the index contains unrelated changes or a file has mixed edits, leave your changes unstaged and report it. Exclude secrets, logs, caches, and packaged builds.
- No destructive reset, clean, force-push, remote branch deletion, or history rewrite. Get owner approval before deleting working systems, adding costly dependencies, or making major scope changes. **Push only after explicit approval for the remote, branch, and commit.** Publication and jam submission stay with the owner.

Report in Ukrainian after each checkpoint. Include the FS task, changed paths, verification evidence, spend or estimate, blocker if any, and next action. Use English for project docs, code, and asset names.
