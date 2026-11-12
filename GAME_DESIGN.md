# Fateshaped: The First Descent

A single-player third-person fantasy action RPG and dungeon crawler for Windows. Jam theme: **Director's Cut**. Aim for an 8–12 minute run. A shorter complete adventure is acceptable.

## Design update 2026-11-12 — persistent guild loop (supersedes conflicting text below)

The owner replaced the single-run route with a persistent RPG loop. Where this section and older sections disagree, this section wins.

**Loop.** Launch → Guild Hub (`L_Guild`) → accept quest → portal activates → dungeon (`L_Dungeon_01`) → complete objective → rewards → return to guild → level up → choose essence → repeat. Character progression persists between runs and game restarts. Death ends the run without rewards and returns the player to the guild.

**Guild Hub.** Restores character state; shows level, XP and inventory; hosts the quest board, the quest portal and Gerald, the fairy who looks like a middle-aged man. On first launch he introduces the guild; afterwards `IntroCompleted` is saved and he stays at reception with a short line.

**Quest.** Data: ID, name, description, minimum level, target dungeon, completion condition, XP reward, item reward. First quest: *The Forgotten Crypt* (`Q_ForgottenCrypt`): defeat the crypt's final enemy; reward 120 XP and one Crypt Shard. Rewards are granted exactly once per accepted run (run ID compared with the last rewarded run ID in the save).

**Persistence.** `BP_FS_GameInstance` holds runtime state across level loads and owns the loaded `BP_FS_SaveGame` (slot `FateshapedSave`). Saved: save version, appearance ID, level, XP, ordered essence history, pending essence choices, class ID, inventory, completed quests, intro flag, run counter and last rewarded run. Transient combat state is not saved. A missing save creates defaults in memory; nothing is written until the first real save. Save failures are reported on screen.

**Levels and essences.** Level 1: first essence choice (S, Swordsman). Level 2 (100 XP): grants one *pending* essence choice, saved, and spent in the guild; S then M resolves to Spellsword and unlocks the Arcane Wave. Level 5 is reserved. Order matters; only implemented choices are offered.

**Inventory.** Minimal: one material (Crypt Shard), optionally one equipment item, shown in the guild. No crafting, rarity, stats or trading.

**Carried over.** Combat, V/S/M, the melee enemy, telegraphs, the Arcane Wave and the HUD from the earlier route are reused. The in-dungeon pedestal and altar are temporary until essence selection moves to the guild.

## Player experience

The player character is the toy. Moving, fighting, and trying a new ability should feel good on their own. The first encounter teaches the Swordsman's basic kit. Evolution then changes how the player fights, and the final encounter gives them room to try the new move. Use magical fantasy, ancient architecture, and light humor to make the descent feel like one small adventure.

## Route and progression

### Playable baseline

1. **Entrance.** Introduce movement and combat controls. The player confirms Stamina (S) to become a Swordsman, then enters the dungeon.
2. **First encounter.** A small room teaches closing distance, reading a melee wind-up, dodging, and landing a counterattack. Clearing it opens the route to the altar.
3. **Evolution altar.** The player confirms Mana (M). Show the Spellsword class change, refill enough Mana for the new attack, and show its input before the final fight.
4. **Final encounter.** Reuse the melee enemy in a layout that lets the player try the new attack. Clearing the encounter unlocks the exit. The basic attack keeps victory possible if the player runs out of Mana.
5. **Exit.** Show a clear victory screen with the game title and a short continuation line. Offer a restart.

Use predefined rooms and simple transitions. Doors and triggers follow cleared encounters. Death offers a restart at the entrance and resets health, resources, essence history, encounters, and doors. The route must work from a fresh launch without editor commands.

### Demo presentation target

Frame the same route as the hero's first guild contract. A brief guild arrival introduces the fairy, the quest board and the portal. A second pre-altar room can reinforce basic combat when the schedule allows. The altar provides a quiet reveal, followed by a stronger encounter in which the Spellsword can use the new move. The victory beat completes the contract. A guild return and the fairy's hint at the world beyond the demo can extend that beat if the existing map supports it.

Keep the guild, second pre-altar fight and return modular so the entrance → first fight → altar → final fight → ending route can ship on its own. Use the dungeon entrance and victory screen as the fallback. Reuse the melee enemy across rooms and add a ranged or elite variant only when it improves the final encounter within the two-archetype scope.

## Player and combat

- Keep third-person movement and camera responsive. Expose basic attack, dodge or short dash, and the active class ability through readable prompts.
- Basic melee remains usable with depleted Stamina and Mana. Use an existing attack animation or a clear timing and feedback fallback.
- Dodge consumes Stamina and provides enough movement to evade a telegraphed attack. Insufficient Stamina prevents dodge and gives immediate feedback.
- Successful hits give visible confirmation. Enemy attacks have a clear wind-up and recovery window. Prioritize readable timing, input response, and hit feedback before expensive animation or effects work.

## V/S/M resources

All three resources are visible in the HUD from the start. Essence choices specialize the character while all three resources stay available.

| Resource | Meaning | Jam behavior |
| --- | --- | --- |
| **V · Vitality** | Health, resilience, and future sacrifice mechanics | Damage reduces current V. Zero V triggers death. Restart restores health. |
| **S · Stamina** | Physical techniques, movement, and evasion | Dodge spends S. S recovers during play. |
| **M · Mana** | Spellcasting and arcane attacks | The altar grants enough M for at least one Spellsword attack. Insufficient M prevents activation and gives feedback. Mana regeneration can be added if tuning needs it. |

Keep values within their valid ranges and refresh the HUD on changes. Use distinct colors and labels so all three resources are recognizable during combat.

## Essence choice and evolution

The jam exposes one playable sequence. At the entrance the player confirms **S → Swordsman**, with basic melee and dodge. After the first fight the altar appends **M**, creating **[S, M] → Spellsword**. Store selections in order for the current run. The altar applies evolution once per run.

Unlock one enchanted strike or magical follow-up that visibly differs from the basic attack, consumes Mana, and deals damage. A short arcane wave from the sword is one way to give the strike a different reach and timing. Give the player a short class-change message and a usable input hint. Preserve the base kit after evolution. Show choices only when their combat kit works. The full starting choice and other ordered combinations are described in `POST_JAM_BACKLOG.md`.

## Enemies

One melee archetype covers both required encounters. Its loop is detect → approach → telegraph → attack → recover. A defeated enemy stops attacking and counts once toward clearing the room. Reuse the same enemy and adjust placement or numbers before adding a second behavior.

An optional second archetype can be a ranged variant that keeps distance, telegraphs a projectile, and repositions. An elite variant of the existing melee enemy is the simpler choice if ranged behavior would threaten the build. Use one of those variants at most.

## Presentation and ending

Use a consistent fantasy palette, magical lighting, and existing modular assets. Keep HUD, enemy wind-ups, hit effects, and the evolution strike distinct from environmental effects. A few clear audio cues for hits and evolution can support readability.

For an optional guild opening, dress the entrance as a compact guild with a quest board and a dungeon portal. A fairy who looks like a middle-aged man gives one short, humorous line that points to the dungeon. If present at the ending, he acknowledges the descent and hints at future adventures. The entrance and victory screen support the same route on their own.

## Implementation notes

Use existing project conventions and reusable UE systems. Prefer Blueprints and Enhanced Input when available. Reuse GAS only if it is already present and practical. Use C++ for a concrete blocker when it simplifies the implementation. Split responsibilities as the slice needs them.

| Responsibility | Smallest useful owner |
| --- | --- |
| Input, movement, camera, animation | Existing player character and input mappings |
| V/S/M, spending, recovery | Player state or a reusable resource component |
| Attacks, damage, hit feedback | Player and enemy combat logic, shared where practical |
| Ordered choices and unlock | Run state or an essence component |
| Ability execution and cost | A player ability function or component when reuse warrants it |
| Detection, attack timing, death | One enemy Blueprint using simple states or existing AI |
| Encounter clears, doors, win and restart | Level logic or a small dungeon manager |
| Resources, choice, input hints, ending | HUD and lightweight widgets |

Keep rules separate from presentation where practical. Create components, data assets, or tables when they support the working slice. Follow existing asset paths and names. For new assets without a local convention, use `/Game/Fateshaped/` and prefixes such as `BP_FS_`, `BPC_FS_`, `WBP_FS_`, `DA_FS_`, `DT_FS_`, `E_FS_`, `ST_FS_`, `M_FS_`, `MI_FS_`, and `L_FS_`.

## Optional jam polish

After a verified packaged candidate, add polish in individually testable changes within the remaining time and budget. Prefer clearer combat feedback or a stronger evolution cue, then consider the guild cameo or encounter variation. Reverify the packaged run after any addition.
