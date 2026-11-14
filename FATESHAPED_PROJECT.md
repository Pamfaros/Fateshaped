# FATESHAPED — Complete Game Project Specification

> **Document:** Master Game Design Document (GDD) + Technical Design Document (TDD) + initial implementation plan  
> **Working game title:** **Fateshaped**  
> **Game-jam demo title:** **Fateshaped: The First Descent**  
> **Tagline:** *Your Legend Is Yours to Shape.*  
> **Genre:** Third-person fantasy action RPG / dungeon crawler with persistent character progression  
> **Engine:** Unreal Engine 5 (verify the exact installed version in the project)  
> **Initial release platform:** Windows x64  
> **Game jam:** Epic AI MegaJam 2026, theme *Director's Cut*  
> **Target submission deadline:** November 16, 2026, 03:59 Europe/Kyiv (verify against the jam page before submitting)  
> **Document language:** English; class, asset, and programming identifiers are also in English  
> **Status:** Design specification. The systems described here are **not** claimed to be implemented.

---

## 0. How to Read This Document

Decision labels:

- **[REQUIREMENT]** — An explicitly stated intention from the game creator that must be preserved in the design.
- **[PROPOSAL]** — A recommended implementation or design decision that may change after testing.
- **[JAM/P0]** — Essential for the smallest complete jam demo.
- **[JAM/P1]** — Desirable for the jam, but may be cut to protect the build.
- **[POST-JAM]** — A long-term direction; do not automatically implement during the jam.

**Instruction for Claude Code:** This file describes the product vision; it is **not a queue of every feature to implement before the deadline**. For a given milestone, initially read Sections 1, 3, 4, 6, 12, and 16–18 plus only the relevant sections. Do not reread this entire GDD at every step. If a separate `CLAUDE.md` exists, follow its operational and permission rules alongside this document. The game creator's most recent direct requirements take precedence for product decisions.

---

# Part I. Product Vision and Intended Experience

## 1. Concise Product Specification

**In one sentence:** Fateshaped is a fantasy adventure in which players return to an adventurers' guild after dangerous quests, earn experience and equipment, and progressively shape their own class through an ordered sequence of three essences: Vitality, Stamina, and Mana.

### 1.1. Core Design Pillars

1. **The character is the toy.** The hero is the primary source of enjoyment. Movement, attacks, dodging, magic, and combinations of abilities should be satisfying even without external rewards.
2. **Shape your class, don't merely select it.** A class emerges from the player's history of choices. Essence order matters: `S→M` and `M→S` should eventually produce different styles.
3. **A world worth inhabiting.** The fantasy world should feel welcoming, wondrous, and alive: the guild is home, while dungeons are distinct adventures.
4. **Persistent growth.** Level, XP, chosen essences, class, items, and equipment survive both dungeon runs and game restarts. This is **not** a game where core progression resets after every run.
5. **Fair, readable challenges.** Enemies should feel dangerous because of their behavior, not merely inflated HP; successful play must convey growing mastery.
6. **Curiosity and discovery.** New routes, classes, magical interactions, and equipment should invite experimentation.

### 1.2. Core Player Promise

“I begin as an ordinary adventurer, but my decisions, victories, and mistakes gradually shape a unique hero. I return to the guild, prepare, and embark on another adventure with abilities I did not have before.”

### 1.3. What This Game Must **Not** Become

- Not an MMORPG or a large open world in the first release.
- Not a pure roguelite that resets level and chosen essences after each expedition.
- Not a menu-driven simulator in which impressive-looking abilities have no gameplay impact.
- Not a project that uses machine learning simply for the sake of using ML.
- Not a collection of partially completed Blueprints without a fully playable game loop.

### 1.4. Target Audience [PROPOSAL]

Players who enjoy adventurous action RPGs, build experimentation, cooperative dungeon crawlers, fantasy guilds, crafting, and persistent character growth. The demo must be understandable even to someone who has never seen Fateshaped before.

### 1.5. Intended Emotional Arc

**Curiosity → belonging in the world → first danger → learning combat → victory → reward → leveling up → discovering new power → another adventure → desire to continue.**

---

## 2. Fantasy World, Visual Style, and Tone

### 2.1. Setting [REQUIREMENT: atmosphere; PROPOSAL: lore]

The player arrives in a world of guilds, wizards, mercenaries, mysterious ruins, dangerous creatures, and magical places. The overall tone is **sincere fantasy with a sense of adventure**, complemented by localized humor and eccentric NPCs. The world should not be relentlessly grim.

A possible mythology: Vitality, Stamina, and Mana are three manifestations of a fundamental life force. Certain adventurers can deliberately combine them to change their combat path. Such heroes may be called *Fateshaped*. This is a lore proposal, not a jam implementation requirement.

### 2.2. The Guild as Home

A warm interior of wood and stone with inviting lighting, shelving, Gerald's reception desk, a regional map, tables, and the insignia of magical essences. A few locked doors can hint at a forge, an enchanting room, and other future facilities. Decorative objects must not falsely imply that their functionality is already implemented.

### 2.3. The Dungeon as Contrast

Cool stone, moss, runes, dim magical illumination, dangerous halls, and small resting spaces. Visual design must help gameplay: exits should be easy to locate, enemy attacks visible, and interactive objects distinguishable.

### 2.4. The Memorable, Comical Fairy [REQUIREMENT]

The fairy **looks like a middle-aged man**, perhaps with tiny wings and an excessively bureaucratic attitude toward his work. In nature, he is a fairy, but visually he deliberately contradicts the usual delicate, ethereal stereotype.

Roles:
- **First launch:** Personally welcomes the hero and briefly introduces the world, basic controls, guild, and adventuring.
- **All later launches:** Does not repeat the introduction; remains at reception, talks to the player, and provides quests.
- **After a run:** Comments on victory and reminds the player about rewards and progression.

Dialogue style: short, friendly sarcasm, with no long cutscenes. A possible joke concerns the “mandatory novice survival quota.” Dialogue is subject to further editing; these examples are not mandatory canon.

---

# Part II. Complete Gameplay Flow

## 3. The Non-Negotiable Game Loop [REQUIREMENT]

```text
Launch game
   │
   ├─ No save → Create/select appearance → Guild → One-time fairy introduction
   │
   └─ Existing save → Guild → Restore character → Fairy at reception
   │
   ▼
Gerald at reception → Accept quest → Activate portal
   │
   ▼
Travel to a separate quest level (Dungeon)
   │
   ▼
Explore → Fight → Meet objective → Complete expedition
   │
   ▼
XP + Materials/Items → Save progression → Return to Guild
   │
   ▼
Check for new level → Available essence choices → Class growth/evolution
   │
   ▼
Equip / Prepare → New quest → Next expedition
```

**Invariants:** Level, XP, every chosen essence, unspent essence choices, appearance, items, and equipment persist between expeditions and after a full restart. The fairy does not replay the introduction once completed. Processing a completion event multiple times cannot grant the same run's reward twice.

## 4. First Launch — Step-by-Step UX

### 4.1. Creating the Hero

**[REQUIREMENT]** Lightweight customization: a few preset appearances/models/materials, without an elaborate face, hair, or body editor. Appearance does not affect combat attributes.

**[PROPOSAL]** For the jam: one base character skeleton with two or three preset visual variants; if assets are unavailable, use one polished appearance and a confirmation button. Save `AppearanceId` immediately.

### 4.2. The First Guild Scene

The player spawns in `L_Guild`, and the one-time introduction with the fairy begins. It explains only the guild, controls, quests, the three essences, and the fact that additional essence selections unlock at particular levels. Aim for roughly 30–90 seconds; the dialogue should be skippable. Mark the introduction complete only after it has properly finished or the player deliberately skips it; persist `bIntroCompleted=true`.

**Important:** The fairy remains physically present in the guild afterward; he does not disappear from the game. He may approach or speak to the hero initially and then move to reception. For the jam, it is acceptable for the initial conversation to take place with him already behind the reception desk.

### 4.3. First Essence Selection

At **Level 1**, the hero may select the initial `V`, `S`, or `M` essence. All three resource bars always exist, but the first selection determines the initial combat archetype. The selection is made once, appended to the ordered history, and saved to disk.

The selection interface explains each essence in one sentence and communicates that the choice shapes the hero's path. Do not present supposedly distinct classes that behave identically.

### 4.4. The First Quest

Gerald offers `The Forgotten Crypt` at reception after the introduction and first essence selection. Interact with Gerald to accept the contract. The portal activates, and the hero travels to `L_Dungeon_01`. A simple visual cue must make it obvious that the quest is active and the portal is available.

## 5. Subsequent Launches

1. Load `SaveGame` before or immediately after creating the authoritative runtime state.
2. Open `L_Guild`.
3. Spawn the hero with saved appearance, level, XP, essences, class, inventory, and equipment.
4. If `bIntroCompleted=true`, do not replay the opening scene; the fairy remains at reception.
5. Allow the player to accept another quest, inspect the character, or spend an unlocked essence choice.

If a save is corrupted or has an incompatible version, never silently overwrite it. Report the problem and offer a safe alternative (backup or new slot), to the extent that current scope allows.

## 6. Game Structure by Unreal Maps

| Map | Purpose | Jam |
|---|---|---|
| `L_Guild` | Persistent hub: Gerald's reception and contracts, portal, progression, menus | **P0** |
| `L_Dungeon_01` | First quest: exploration, combat, final objective | **P0** |
| `L_CombatTest` | Development-only arena for testing gameplay, not necessarily part of the release | **Development only** |
| Additional `L_Dungeon_*`, biomes, town, outskirts | Expanded world and content | **POST-JAM** |

The portal is functional, not just decorative: it activates only after a quest is accepted and loads the quest's destination map. After returning, it deactivates until another quest is accepted. Level transitions must not reset character progression.

## 7. The Guild as a Functional Level

### 7.1. Interactive Elements

| Object | Minimum behavior | Long-term potential |
|---|---|---|
| Fairy at reception | Short dialogue + offering/selecting a quest | Reputation reactions, lore, multiple quests |
| Gerald's contracts | Quest title, objective, reward, and acceptance | Filters, regions, difficulty, daily contracts |
| Portal | Enabled after quest acceptance; loads the map | Multiple locations, parties, routes |
| Character Screen | Level, XP, class, V/S/M, essence history | Detailed advancement tree |
| Essence Selection | Available only after reaching a milestone level | Branches, synergies, evolutions |
| Inventory/Equipment | Minimal reward list; one equip action if feasible | Crafting, enchantments, forging, trading |
| Decorative workshops | Non-functional, marked as unavailable | Fully interactive workshops |

### 7.2. Fairy State Machine

```text
NoSave / IntroIncomplete → IntroAvailable → IntroPlaying → IntroComplete
                                                        │
                                                        ▼
                                                ReceptionIdle
                                                        │
                                             Interact / AcceptQuest
                                                        │
                                                        ▼
                                                ReceptionIdle
```

The NPC must support situations in which the introduction is complete but no quest has been accepted, as well as situations in which a quest is accepted but the player has not yet entered the portal.

## 8. Quests and Individual Runs

### 8.1. Quest Data Model

**[REQUIREMENT]** The guild issues quests that lead to separate levels. Completing a quest awards XP, materials and/or items, and the hero returns to the guild.

**[PROPOSAL]** A quest definition contains:

```text
QuestDefinition
  QuestId: Name / GameplayTag
  Title: Text
  Description: Text
  MinLevel: int
  DestinationMap: SoftObjectPath / map reference
  Objectives: list of typed requirements
  XPReward: int
  ItemRewards: [{ItemId, Quantity}]
  Repeatable: bool
  OptionalDifficultyTier: int
```

Store **run state separately from quest definition**. `QuestDefinition` is static content; `QuestRunState` describes a specific expedition.

### 8.2. First Quest [PROPOSAL]

- **QuestId:** `Q_ForgottenCrypt`
- **Title:** `The Forgotten Crypt`
- **Description:** Explore an ancient crypt and eliminate the source of unusual monster activity.
- **Minimum level:** 1.
- **Destination:** `L_Dungeon_01`.
- **Objective:** Defeat the final enemy and activate the exit.
- **Reward:** 120 XP + one predefined `ArcaneCrystal` material or a simple piece of equipment.
- **Repeatable during the jam:** Yes, so the second run can test the evolved class without building another large level.

120 XP is an initial balancing suggestion, not a final value. It should take the hero from Level 1 to Level 2 if the threshold is 100 XP.

### 8.3. Quest/Run States

```text
Available ──Accept──> Accepted ──EnterPortal──> InProgress
                                           │
                                           ├─Fail/Abandon──> Failed
                                           │
                                           └─ObjectiveMet──> CompletionPendingSave
                                                                   │
                                                         Save succeeded
                                                                   │
                                                                   ▼
                                                                 Rewarded
                                                                   │
                                                              ReturnPortal
                                                                   │
                                                                   ▼
                                                               BackToGuild
```

Every acceptance of a repeatable quest generates a **new `RunId`**. `QuestId` cannot be the sole reward identifier; otherwise a legitimate second completion would incorrectly be blocked.

### 8.4. Preventing Duplicate Rewards

Quest completion pseudocode:

```pseudo
CompleteQuest(runId):
    if runId != ActiveRun.RunId: return ERROR_INVALID_RUN
    if ActiveRun.State not in {InProgress, CompletionPendingSave}: return NOOP
    if runId in SaveData.RewardedRunIds: return ALREADY_REWARDED
    if not ObjectivesSatisfied(): return ERROR_OBJECTIVES_NOT_MET

    candidateSave = CopyCurrentSaveSnapshot()
    candidateSave.GrantXP(Quest.XPReward)
    candidateSave.AddItems(Quest.ItemRewards)
    candidateSave.RewardedRunIds.Add(runId)
    candidateSave.ActiveRun.State = Rewarded

    if SaveToSlot(candidateSave) succeeds:
        AdoptCandidateAsAuthoritativeState()
        EnableReturnPortal()
        return SUCCESS
    else:
        KeepCompletedRunPending()
        ShowSaveRetryState()
        return ERROR_SAVE_FAILED
```

**Important:** Unreal's `SaveGameToSlot` does not guarantee fully transactional file-system writes in every unexpected-shutdown scenario. For the jam, the minimum is to check the result of saving, support safe retry with the same `RunId`, and prevent a second grant. Dual slots and backups are potential future reliability improvements.

### 8.5. Interrupted, Failed, and Repeated Runs

**[PROPOSAL for the jam]** On death, the hero either returns to the guild or restarts from a checkpoint in the same run (choose one consistent policy). The simplest option is **failure → Guild**, with no quest-completion reward; previously saved level and inventory remain unchanged. The player can accept the quest again. No loss of level, class, or essence history.

If the game crashes during `InProgress`, load the guild on the next launch and allow the quest to be restarted without granting an unearned reward. If the run is already `Rewarded` and that state was successfully saved, the rewards remain owned after restart; do not grant them again, and safely resume in the guild.

---

# Part III. Character, Progression, and Combat

## 9. Attributes, V/S/M, and Selection Rules

### 9.1. Three Fundamental Resources [REQUIREMENT]

**EVERY character has all three resource bars**, regardless of specialization:

| Resource | Abbreviation | Combat meaning | Example costs |
|---|---|---|---|
| Vitality | `V` | Life, survival, sacrifice, resilience | Incoming damage, HP-powered abilities |
| Stamina | `S` | Physical techniques, dashes, mobility | Dodge, special strike, defense |
| Mana | `M` | Spellcasting, elemental effects, magical techniques | Arcane Bolt, Arcane Slash |

Choosing an essence does **not** remove the other two bars. It defines the character's combat foundation: which techniques they can use and how they spend resources.

### 9.2. Regeneration and Constraints [PROPOSAL]

- **Vitality:** Little or no passive regeneration after damage; recover through supplies, abilities, or resets between expeditions. HP-consuming abilities must have controlled risk and should not unintentionally kill the player.
- **Stamina:** Regenerates quickly after a short pause; basic attacks remain available even when Stamina is depleted.
- **Mana:** Regenerates at a moderate rate or returns through active play. Spells should be usable regularly, not infinitely.

Initial tuning values (not canon): `MaxHP=100`, `MaxStamina=100`, `MaxMana=100`, `DodgeCost=20`, `ManaSkillCost=25`. These are not approved balance values; validate through PIE and playtests.

### 9.3. Levels and XP [REQUIREMENT: gating; PROPOSAL: values]

The player earns XP from completed quests. Character level persists. **A new essence can only be chosen after reaching a specified character level**, not arbitrarily in the middle of each dungeon run.

For the demo:

| Level | Unlock |
|---|---|
| 1 | Initial `V/S/M` selection |
| 2 | Second essence choice, enabling a potential hybrid class |
| 3–4 | Attribute increases / future rewards |
| 5 | Another essence choice **after the jam** |

Proposed cumulative XP thresholds: L1 = 0, L2 = 100, L3 = 260, L4 = 480, L5 = 760. Store `TotalXP` and either derive `Level` from thresholds or store it while checking consistency. Earning 120 XP at Level 1 advances the character to Level 2 and retains 20 XP toward the next threshold; overflow is not discarded.

**Example logic:**

```pseudo
AddXP(amount):
    assert amount >= 0
    previousLevel = Level
    TotalXP += amount
    while TotalXP >= CumulativeXPRequired(Level + 1):
        Level += 1
        if Level in EssenceMilestoneLevels:
            PendingEssenceChoices += 1
    PersistProgress()
```

Leveling up must **not automatically choose** the next essence. Instead, grant `PendingEssenceChoices`. If the player quits before choosing, that opportunity persists after the next launch. Do not grant the same milestone choice again after reloading.

### 9.4. Essence Order Matters [REQUIREMENT]

For example, `EssenceHistory = [S, M]` is an **ordered array**, not a set or mere counters.

| Sequence | Conceptual class | Playstyle distinction |
|---|---|---|
| `S→M` | **Spellsword** | Swordsman who later enchants physical attacks with magic |
| `M→S` | **Battlemage** | Mage who develops mobility and close-combat techniques |
| `V→M` | Blood Mage | Trades health for magic |
| `M→V` | Warlock | Curses and life draining |
| `S→V` | Reaver | Aggressive physical strikes and recovery |
| `V→S` | Juggernaut | Resilience and heavy attacks |

Repeated selections `S→S`, `M→M`, and `V→V` may deepen a specialization in the full game, but **these are not finished classes designed for the jam**. The number of sequences grows as `3^n`; use composable modifiers rather than implementing a separate Blueprint for every possible sequence.

### 9.5. Jam Scope for Essence Choices

Aim to provide all three **starting** specializations (`V`, `S`, `M`) with at least minimally distinct functioning abilities. The **only required fully featured hybrid evolution** is `S→M = Spellsword`.

Do not present other branches as finished if they lack corresponding behavior. If time does not permit full support for every initial selection and second combination, an explicitly labeled **restricted `S→M` showcase** is acceptable, but it must not be disguised as an unrestricted class system. This is a product-level UX decision the game creator should be aware of before release.

### 9.6. Where Evolution Occurs in the Loop

**For the jam, Level 2 unlocks after the first completed expedition.** The hero returns to the Guild, sees the level-up notification, opens the second-essence selection screen, and only then starts the second run with the new ability.

**Do not** move the mandatory second essence selection to the middle of the first dungeon: doing so contradicts the level-gated, persistent progression design. Future dungeon altars may provide temporary blessings or different mechanics, but should not replace level-based essence selection.

## 10. Starting Classes and Abilities

### 10.1. `V` — Berserker

A powerful melee fighter who treats HP as a risk resource. **[PROPOSAL]** `Blood Strike`: deals increased damage at a small HP cost and cannot activate when health is critically low. Visual cue: reddish slash or aura.

### 10.2. `S` — Swordsman

A mobile sword fighter. **[PROPOSAL]** `Dash Strike`: a short lunging strike that consumes Stamina. Visual cue: rapid weapon trail.

### 10.3. `M` — Sorcerer

A ranged spellcaster. **[PROPOSAL]** `Arcane Bolt`: a visible projectile that spends Mana and has readable cast/travel timing.

### 10.4. `S→M` — Spellsword [PRIMARY SHOWCASE]

The basic physical attack remains, but the hero gains **Arcane Slash**: a short-range energy wave or enchanted strike that consumes Mana, extends combat reach, and changes tactics. Selecting the essence updates the weapon effect, ability indicator, and, where feasible, sound/animation.

The core condition: **evolution must change what the player can do, not merely a damage number or a menu label**. The new ability must work during the second run and survive restarting the game.

## 11. Controls, Camera, Combat, and Death

### 11.1. Suggested Controls [PROPOSAL]

| Input | Action |
|---|---|
| `WASD` | Movement |
| Mouse | Camera control |
| `LMB` | Basic attack |
| `RMB` | Active/class ability |
| `Shift` | Dodge / Dash |
| `E` | Interact with NPCs, portals, and items |
| `Space` | Jump, **only if required** |
| `Esc` | Pause / Menu |
| `Tab` or `I` | Character/Inventory menu, if implemented |

Use Enhanced Input where already available. Input prompts must be consistent with actual controls.

### 11.2. Required Combat Feel

- Input response is fast and predictable.
- Attacks have readable startup, active hit, and recovery phases.
- One strike does not accidentally deal multiple hits to a single target unless designed as a multi-hit attack.
- Successful hits produce sound, short effects, and visible enemy HP/reaction feedback.
- Dodging has a clear Stamina cost and controlled recovery timing.
- Interrupted animations cannot leave the character permanently locked in an attack state.
- When Mana/Stamina is insufficient, the UI explains why an ability is unavailable.
- Enemies must not deal invisible, untelegraphed damage.

### 11.3. Basic Attack Flow

```text
Input → ValidateState/Resources → StartAttackAnimation
      → ActivateHitWindow → Trace/Overlap → ApplyDamageOncePerTarget
      → VFX/SFX/HitReaction → EndHitWindow → Recovery → Idle
```

For the jam, simple capsule/box traces, a short animation, and basic hit reactions are sufficient. A complete combo framework, sophisticated lock-on, or limb dismemberment physics is unnecessary.

### 11.4. Death and Retry

When `Vitality <= 0`, stop combat actions, display a clear defeat message, and provide a safe `Retry` or `Return to Guild` action. Do not permanently remove core progression. If returning to the guild, an incomplete run must not grant quest-completion rewards.

## 12. Enemies and Difficulty

### 12.1. Melee Enemy [JAM/P0]

Minimal state machine:

```text
Idle → Detect → Chase → Windup → Attack → Recovery → Chase
                       └────────── Health<=0 → Death
```

The enemy tracks range, approaches, telegraphs an attack, strikes, and pauses briefly afterward. The telegraph gives the player a fair chance to dodge. Attacks must not pin the hero against walls indefinitely.

### 12.2. Ranged Enemy [JAM/P1]

Maintains distance, fires a visible projectile, and has a cooldown. Do not implement it while the melee enemy or quest-completion loop is still broken.

### 12.3. Elite / Boss [JAM/P1]

An upgraded version of a known enemy, with more prominent telegraphs or one additional attack. A full multiphase boss is not required for the jam.

### 12.4. Intended Difficulty

Threat should come from readable attacks, positioning, limited resources, and choosing moments to counterattack. Do not compensate for weak AI by giving enemies enormous HP pools. After losing, the player should understand what they could have done differently.

---

# Part IV. Content, Rewards, and World Systems

## 13. Dungeon Design

### 13.1. Jam Dungeon Map [JAM/P0]

Use a **handcrafted and validated map** built from modular rooms (approximately three to five functional areas):

1. **Entrance:** Orientation and a brief tutorial.
2. **Combat A:** Safe first exposure to enemy attacks.
3. **Combat B:** Tests movement and class ability usage.
4. **Final Encounter:** A tougher enemy or an additional objective.
5. **Exit/Return:** A clear portal or completion event.

The second run may use **the same map** with a small encounter variation (different enemy positioning, an additional enemy type, or cosmetic lighting changes). From a scope perspective, demonstrating the new Spellsword ability matters far more than building an entirely new biome.

### 13.2. Long-Term Procedural Generation [POST-JAM]

The creator's proposed algorithm:

```text
Room placement
    ↓
Delaunay triangulation of room centers
    ↓
Weighted candidate graph
    ↓
Minimum spanning tree (MST) for connectivity
    ↓
Extra edges for loops and alternative paths
    ↓
Orthogonal / L-shaped / T-shaped corridor routing
    ↓
Door placement + room roles + encounter assignment
    ↓
Collision, reachability, and gameplay validation
```

Validate corridor intersections with unrelated rooms, adequate passage width, navigation connectivity, exit reachability, and behavior under different random seeds. If an L/T-shaped corridor is obstructed, use grid-based A* or another obstacle-aware routing method. Account for Unreal's NavMesh behavior on procedurally modified levels; do not assume runtime navigation is automatically valid.

### 13.3. Encounter Director [JAM/P2 / POST-JAM]

The objective is to regulate the **pacing** of an adventure, not continually punish a successful player. A lightweight, rule-based director could evaluate HP, health lost in recent encounters, clear times, resource use, and recent difficult encounters.

Possible choices for the next room:
- Rest or healing following high tension.
- A normal fight when the player's state is stable.
- An optional additional threat for especially strong performance.

The director must not spawn unfair enemies behind the player, unexpectedly mutate an ongoing fight, or secretly negate character growth.

### 13.4. Reinforcement Learning Research [POST-JAM / R&D]

Potential methods include contextual bandits, DQN, or PPO, with a constrained action space of selecting a prepared encounter, room, or resource allocation from a valid modular library (the “bag of materials”). The discrete state could include health, success rate, pace, playstyle, and statistics from previous rooms. Rewards should account for tension, variety, fairness, and completion rates—not only death rate. Training requires a simulator, data, and an evaluation method. **Do not build it during the jam.**

## 14. Rewards, Inventory, and Equipment

### 14.1. Requirement

Completing a quest grants XP and materials or items that may later be used to craft or improve character equipment. These rewards persist across runs and after restarting the game.

### 14.2. Minimum Jam Content

- **Material:** `ArcaneCrystal` ×1.
- **Equipment:** One simple item, if time permits (for example, `ApprenticeCharm`, which slightly raises MaxMana).
- **Inventory UI:** A small quantity list, with icons if practical.
- **Equip UI:** A single slot or equip button if the underlying systems already work.
- **No** full crafting recipes, rarities, sorting systems, economy, or dozens of item types.

If time or API budget requires scope reduction, preserve at least one persistent material/item that is visible in the guild after a return and a full restart.

### 14.3. Item Data [PROPOSAL]

```text
ItemDefinition:
  ItemId
  DisplayName
  ItemType: Material | Weapon | Armor | Accessory | Consumable
  Icon
  Description
  Stackable
  MaxStack
  StatModifiers

InventoryEntry:
  ItemId
  Quantity
  InstanceId (only for unique equipment, if necessary)

EquipmentEntry:
  SlotId
  ItemId / InstanceId
```

Save **identifiers and state**, not direct references to actors in a Dungeon Map. On load, resolve definitions through Data Assets/Data Tables.

### 14.4. Full System [POST-JAM]

Forge, enchantment workshop, alchemy, scrolls, rare materials, recipes, upgrades, enchanting, equipment sets, and a market/auction. The economy should create new and interesting build possibilities, rather than merely enabling endless numerical stat inflation.

## 15. Social Features [POST-JAM]

In the long term, the guild may become a place to meet friends, organize cooperative expeditions, trade, and prepare together. This will require:

- An authoritative source of truth for XP, items, rewards, and quests.
- Server-validated transactions and duplication prevention.
- Replication of movement, abilities, and enemies.
- Parties, lobbies, and matchmaking.
- A clear save and migration policy for existing single-player characters.

**Do not make network replication mandatory during the jam.** A correct local SaveGame system and a clear separation between gameplay state and presentation are sufficient foundations for now.

---

# Part V. Unreal Engine Architecture

## 16. Technical Principles

1. **Blueprint-first.** Use built-in Unreal systems, existing templates, animations, UMG, and Enhanced Input where available.
2. **C++ only when justified.** For example, an efficient graph generator or complex logic that the connected MCP cannot reasonably implement in Blueprint, if such a need has actually been established.
3. **Separate data from presentation.** Quest, Item, Ability, and Class definitions should live in Data Assets/Data Tables or equivalent structures.
4. **Avoid unnecessary frameworks.** Do not introduce GAS, a custom ECS, or a server architecture during the jam without a specific reason.
5. **Never edit `.uasset` or `.umap` as text.** Use Unreal Editor and whichever MCP tools are actually available.
6. **Develop through end-to-end tests.** First establish a working Guild–Dungeon–Guild loop with persistence, then progressively add gameplay.

## 17. Suggested Folder Structure

```text
Fateshaped/
├── Fateshaped.uproject
├── CLAUDE.md                     # Brief Claude Code rules, if needed
├── FATESHAPED_PROJECT.md         # This complete project specification
├── TASKS.md                      # Concise operational tracker, if needed
├── DEVLOG.md                     # Concise factual development log, if needed
├── Config/
├── Source/                       # Only if C++ is chosen
└── Content/Fateshaped/
    ├── Core/
    ├── Characters/
    ├── Combat/
    ├── Abilities/
    ├── Evolution/
    ├── Quests/
    ├── Items/
    ├── Enemies/
    ├── Guild/
    ├── Dungeon/
    ├── UI/
    ├── Materials/
    ├── VFX/
    ├── Audio/
    └── Maps/
         ├── L_Guild
         ├── L_Dungeon_01
         └── L_CombatTest
```

This is a target structure—not an instruction to create empty folders and thirty placeholder Blueprints before implementing gameplay.

**Naming:** `BP_FS_*` (Blueprint), `BPC_FS_*` (components), `WBP_FS_*` (UI), `DA_FS_*` (Data Asset), `DT_FS_*` (Data Table), `ST_FS_*` (structs), `E_FS_*` (enums), `L_*` (maps), `M_FS_*`/`MI_FS_*` (materials).

## 18. Components and Responsibilities

| Component | Responsibility | Jam need |
|---|---|---|
| `BP_FS_GameInstance` | Maintains runtime data across `OpenLevel`; initializes save/load and map transitions | P0 |
| `BP_FS_SaveGame` | Serializes persistent character, quest, introduction, and inventory data | P0 |
| `BP_FS_PlayerCharacter` | Movement, camera, input actions, visual effects, interactions | P0 |
| `BPC_FS_Resources` | Current/maximum V/S/M, costs, regeneration | P0 |
| `BPC_FS_Progression` | XP, level, thresholds, pending essence choices | P0 |
| `BPC_FS_Essence` | Ordered essence history, class resolution | P0 |
| `BPC_FS_Combat` | Attacks, hit windows, damage, combat state | P0 |
| `BPC_FS_Abilities` | Ability activation, costs, cooldowns, class-to-ability mapping | P0, minimal |
| `BP_FS_QuestManager` | Active quest run, objectives, completion/reward logic | P0 |
| `BP_FS_Portal` | Verifies active quest and transitions to destination map | P0 |
| `BP_FS_Fairy` | NPC, one-time introduction / reception behavior | P1 (can be simple) |
| `BP_FS_GuildManager` | Hub initialization, introduction, quest access rules | P0/P1 |
| `BP_FS_EnemyBase` | Enemy movement, health, attack telegraph, damage | P0 |
| `WBP_FS_HUD` | V/S/M bars, XP/class, current objective | P0 |
| `WBP_FS_Quest` | Quest acceptance and reward display | P0 |
| `WBP_FS_EssenceSelect` | Initial/subsequent essence selection | P0 |
| `WBP_FS_Inventory` | Basic display of awarded items | P1 |

These names are recommendations. If the project already contains working architecture, **adapt it rather than duplicating it**.

### 18.1. Lifecycle Across Maps

- Before `OpenLevel`, persist meaningful changes in the authoritative runtime state and, at defined checkpoints, in SaveGame.
- Do not assume the Pawn in `L_Guild` physically survives `OpenLevel`. A new Pawn restores state from GameInstance/SaveGame.
- `GameMode` stores map-specific rules, not the hero's long-term data.
- UI never owns authoritative XP or items; it only renders them.
- `ActiveQuestRun` remains accessible across Dungeon/Hub transitions.

### 18.2. Minimum API Contracts [PROPOSAL]

```text
SaveService.LoadOrCreateProfile() -> SaveResult
SaveService.CommitProfile(snapshot) -> Success/Failure
Progression.AddXP(amount) -> LevelUps, NewPendingChoices
Essence.CanChooseEssence() -> bool
Essence.Choose(nextEssence) -> Success/Failure
Quest.Accept(questId) -> ActiveRunId
Portal.CanEnter() -> bool
Quest.MarkObjectiveComplete(objectiveId)
Quest.TryCompleteActiveRun() -> RewardCommitResult
Dungeon.ReturnToGuild() -> TransitionResult
Inventory.Add(itemId, quantity)
Equipment.TryEquip(itemId, slot)
```

These are conceptual contracts, not pre-existing Blueprint functions. Implement only the contracts needed for the active milestone.

## 19. SaveGame Data Model [JAM/P0]

```text
SaveVersion: int
ProfileId: string/guid (optional for one-slot demo)
AppearanceId: Name
Level: int
TotalXP: int
EssenceHistory: Array<EssenceType>
PendingEssenceChoices: int
IntroCompleted: bool
Inventory: Array<InventoryEntry>
Equipment: Array<EquipmentEntry>
CompletedQuestIds: Array<Name>
ActiveRun:
  RunId
  QuestId
  State
  ObjectiveProgress
RewardedRunIds: Array<RunId>
LastSafeMap: Guild
```

### 19.1. What Not to Save

Transient sprint state, current animation, hit windows, short-lived buffs, random states of minor enemies, or scene objects through direct world references. At the start of each new expedition, initialize current V/S/M according to an explicit policy (for example, full resource bars).

### 19.2. When to Save

- After character creation / appearance selection.
- After completing or skipping the initial introduction.
- After the initial essence selection.
- After accepting a quest (the `RunId` should survive restarts if required by the chosen recovery policy).
- After granting rewards and XP as one gameplay-level commit.
- After selecting the second essence.
- After changing equipment.

### 19.3. Schema and Migration

`SaveVersion` is required. The jam may have only `v1`; an unknown version must not silently turn into a blank profile. Later releases should add `MigrateSave(vN→vN+1)`, backups, and corrupted-file tests.

### 19.4. Consistency Conditions

```text
Level >= 1
TotalXP >= 0
PendingEssenceChoices >= 0
EssenceHistory.length >= 1 after initial selection
EssenceHistory contains only V/S/M
Class can be derived from EssenceHistory
Every RewardedRunId is unique
Every ItemId has a valid definition or a safe fallback
```

Prefer to **derive the class** from `EssenceHistory`, rather than storing a second independently authoritative field without need. If `ClassId` is cached, validate its consistency with the history on load.

## 20. Interface and User Experience

### HUD

- Three compact, visually distinct resource bars with **icons/letters**, not colors alone.
- Current ability, cooldown, cost, and feedback when a resource is insufficient.
- Brief current quest objective.
- Enemy HP or hit feedback when it improves readability.

### Character Menu

- Level, TotalXP, and progress toward the next level.
- Primary Essence, EssenceHistory in selection order, and current specialization.
- `PendingEssenceChoices` with a way to choose an essence while in the Guild.
- Inventory/equipment when available.

### Quest UI

- Title, objective, minimum level, rewards, and Accept button.
- Active state and portal availability indicator.
- On completion: reward screen, new level, and a notification about unlocked evolution.

### Clarity Requirements

The player must understand what to do next, where to go, why the portal is locked, why an ability failed, where XP came from, why evolution is available, and which progress survives the run.

## 21. Sound and Visual Effects

**Priorities:** weapon-hit audio, impact confirmation, incoming-damage feedback, magical evolution effects, portal sound, UI confirmation, and ambient sound in both guild and dungeon. A small coherent set of effects is better than a chaotic mixture of unrelated assets.

Do not import an entire multi-gigabyte content pack for a handful of objects; include only needed assets and verify licensing and provenance.

---

# Part VI. Jam Vertical Slice

## 22. Jam Release Scope

**Target:** A small but **complete** Windows game built around two short expeditions, demonstrating persistent progression and a meaningful change in combat style. Target duration: **12–18 minutes** (a design goal, not a hard limit).

### 22.1. Mandatory Behavior [JAM/P0]

1. Launch the game and enter a dedicated `L_Guild` map.
2. Show the introduction once, and reliably skip it on later launches (minimal text dialogue is acceptable).
3. The character can move, attack, and dodge.
4. V/S/M resources work, and initial essence selection is visible and saved.
5. The guild provides a quest, activates the portal, and transitions to `L_Dungeon_01`.
6. At least one enemy has a readable attack, and the quest can be completed properly.
7. Completion awards XP plus a material/item, without duplicate grants.
8. The player returns to the Guild; the reward advances Level 1 → Level 2.
9. While in the Guild, the player can select the next essence; at minimum, `S→M` is a real hybrid evolution.
10. A second expedition uses the new ability without resetting earlier progression.
11. Restarting the game preserves level, XP, essence history, inventory, and intro flag.
12. A standalone Windows x64 build launches outside Unreal Editor.

### 22.2. Desirable Features [JAM/P1]

- Two enemy types and an elite final opponent.
- A short funny NPC dialogue and a stylized reception area.
- One equippable item with a visible effect.
- Memorable evolution effects and basic presentation polish.
- Sound, readable lighting, and a closing screen hinting at more adventures.

### 22.3. Deferred Features [POST-JAM]

Co-op, auction house, friends, server-backed profiles, full crafting, complex equipment, the complete branching class tree, RL/DQN/PPO, Delaunay/MST dungeon geometry generation, large worlds, and a full game economy.

## 23. Two-Expedition Demo Scenario

| Time | Stage | Intended feeling |
|---|---|---|
| 0:00–1:30 | First Guild launch, fairy, hero introduction | “I'm in a fantasy world.” |
| 1:30–2:30 | Initial essence, accepting the quest | “I'm choosing my own style.” |
| 2:30–6:30 | Dungeon Run #1: basic combat and final objective | “I've learned how to fight.” |
| 6:30–8:00 | Reward, return to Guild, Level 2 | “My achievements stay with me.” |
| 8:00–9:00 | Second essence choice, Spellsword | “My hero has genuinely changed.” |
| 9:00–14:00 | Dungeon Run #2: using the enchanted strike | “This new style gives me new actions.” |
| 14:00–16:00 | Return, save, hint at the larger world | “I want to continue progressing.” |

This is an example pacing target. If development time runs short, shorten rooms—but **do not remove the persistent-growth loop between the two expeditions**.

## 24. Implementation Order: Incremental, Not All at Once

| ID | Milestone | Minimum evidence of completion |
|---|---|---|
| `M0` | Audit existing UE project, MCP, maps, Git | Factual project-state report; read-only MCP call works |
| `M1` | `Guild → Quest → Portal → Dungeon → DummyComplete → Guild` | Loop playable in PIE, even without combat |
| `M2` | SaveGame + intro flag + profile restoration | Data restored after closing and reopening the game |
| `M3` | Character + basic combat + one melee enemy | Enemy deals/takes damage; death/retry works |
| `M4` | Real rewards, Level 2, XP overflow, one item | Quest grants XP/item exactly once; character reaches Level 2 |
| `M5` | Level 2 essence choice + `S→M` Spellsword | New ability available and persists after reload |
| `M6` | Two runs end-to-end, all transitions tested | Character retains progress, second run completes |
| `M7` | Fairy/world/FX/UI polish | Existing functionality stays intact; UX is understandable |
| `M8` | Windows packaging, tests, submission assets | Standalone `.exe` launches, entire loop is playable |

**Packaging priority:** Do not wait until `M8` for the first test package. Create a smoke-test packaged build after `M1–M2`, or as soon as the basic project is stable. A task is not DONE merely because its Blueprint has been created.

## 25. Scope-Cutting Order During the Jam

Cut features in this order:

1. Any ML/Dungeon Director.
2. Procedural generation and corridor randomization.
3. Second enemy type and elaborate boss.
4. Extra items, complex equipment, and stores.
5. Unnecessary character cosmetic options.
6. Decorative guild rooms.
7. Optional class branches (clearly disable or label them).
8. Combat-room count and run duration.

**Do not cut:** A working Guild, separate Dungeon map, quest/portal/return flow, persistent XP/Level/Essence, minimal combat, one meaningful evolution path, and a stable packaged build.

## 26. Release Rules and Build Size

Target a packaged Windows x64 **Shipping** build. Prefer a ZIP smaller than **1 GB**; that is a practical target for straightforward upload, not a claim about a strict jam limit. Before submission, verify the official rules and allowed upload alternatives. A **30–60-second** gameplay video was previously planned; verify the current requirement before submitting.

Optimization:
- Cook only required maps and resources.
- Exclude editor-only assets.
- Compress packaged data and audio.
- Keep texture resolution appropriate without sacrificing readability.
- Avoid bundling entire sample packs in the packaged build.
- Verify startup map, input, SaveGame, and absence of editor-only runtime dependencies.

**Do not automatically publish the build or submit it to the jam without the creator's explicit approval.**

---

# Part VII. Testing, Quality, and Risks

## 27. Test Matrix

| Test ID | Scenario | Expected result |
|---|---|---|
| `T01` | Start with no SaveGame | Create profile / show one-time introduction; Guild accessible |
| `T02` | Finish intro, restart | Intro is not repeated; fairy is at reception |
| `T03` | Select `S`, restart | History `[S]` and corresponding combat style are restored |
| `T04` | Attempt a second essence choice at Level 1 | Selection is locked; level requirement is clear |
| `T05` | Try portal without an accepted quest | Transition is blocked, with explanation |
| `T06` | Accept quest and enter portal | `L_Dungeon_01` loads; profile state persists |
| `T07` | Hit/kill an enemy | Enemy HP drops; attack telegraph and death work |
| `T08` | Complete the quest | Receive 120 XP + item/material; return becomes available |
| `T09` | Trigger completion repeatedly | No second grant for the same `RunId` |
| `T10` | Finish quest when save fails | No silent data loss or inconsistent return state |
| `T11` | Return to Guild | Level, XP, and items are preserved |
| `T12` | Earn 120 XP from L1 with a 100 XP threshold | Reach L2 and retain 20 XP of overflow |
| `T13` | At L2, quit before choosing the next essence | Pending choice survives restart |
| `T14` | Select `M` after `S` | `[S,M]`, Spellsword, usable Arcane Slash |
| `T15` | Quit/relaunch after evolution | Spellsword and its ability remain available |
| `T16` | Repeat the same `QuestId` legitimately | New `RunId`; a second legitimate reward is allowed |
| `T17` | Die/fail a quest | No completion reward; existing level is retained |
| `T18` | Interrupt game during Dungeon run | Safe return to Guild / defined retry behavior |
| `T19` | Attempt ability with insufficient Mana/Stamina | Ability fails safely; resource never becomes negative |
| `T20` | Launch packaged executable | Entire core loop works outside the editor |

Use explicit `NOT_TESTED`, `PASS`, `FAIL`, and `BLOCKED` results. Never record `PASS` by intuition or from compilation alone when runtime verification is required.

## 28. Agent Verification Levels

- **CREATED:** File or asset exists.
- **COMPILED:** Blueprint/C++ compilation succeeds without errors.
- **PIE_TESTED:** The relevant scenario was actually tested in Play In Editor.
- **PACKAGED_TESTED:** The scenario was verified in a standalone Windows executable.

`CREATED` is not synonymous with `DONE`. Each milestone requires the verification stated in its acceptance criteria. If the MCP does not expose the necessary test capability, honestly report `BLOCKED` or `NEEDS_MANUAL_TEST` rather than fabricating a result.

## 29. Product Success Criteria

### 29.1. Functional Criteria

- The game runs outside Unreal Editor.
- A persistent character history exists.
- The Guild and Dungeon are separate maps.
- Accepting a quest activates the portal.
- A completed quest's reward is not duplicated.
- XP raises character level; essence selection is available only at milestone levels.
- The player makes the Level 2 selection in the Guild, and the second run demonstrates the resulting class change.
- A full restart does not delete progression.

### 29.2. Qualitative Criteria [PROPOSAL]

- A new player learns basic controls in roughly one minute.
- It is clear why an attack hit or missed.
- Evolution visibly changes a combat action within seconds.
- Portal, NPC, and quest interactions do not require verbal explanations from the developer.
- After two expeditions, players want to test other essence combinations.

### 29.3. Playtest Questions

1. Was moving and attacking enjoyable even without enemies?
2. Did you understand the difference between V/S/M?
3. Did your level and items feel genuinely persistent?
4. Was the Level 2 moment and the new essence choice clear?
5. Did you notice a gameplay difference after `S→M`?
6. Did you want to start another expedition?
7. What disrupted the pacing, felt frustrating, or seemed unfinished?

## 30. Risk Register

| Risk | Consequence | Mitigation |
|---|---|---|
| Scope creep | Unfinished demo | Strict P0/P1/POST-JAM separation; cut according to Section 25 |
| Incompatible or limited MCP | Agent cannot actually edit Blueprint content | Early capability audit; supported Unreal alternative |
| Unreliable SaveGame | Lost level, XP, or items | Early end-to-end test, single source of truth, save-result checks |
| Duplicate reward grants | Economy exploit | `RunId`, idempotent grants, T09/T16 |
| Incorrect `OpenLevel` handling | Character or quest state resets | GameInstance/SaveGame and explicit transitions |
| Too many classes | Weak, unfinished evolutions | Polish one `S→M` branch; lock or simplify others |
| Unsatisfying combat | Character is not enjoyable to play | Early combat playground, hit feedback, playtesting |
| Packaging problems | No valid submission | Smoke-package before final day; Shipping build test |
| API credits run out | Agent cannot finish release tasks | Single model/agent, targeted MCP calls, reserve |
| Incompatible or unlicensed assets | Release risk | Track source and licenses; minimal import |

---

# Part VIII. Development Through Claude Code + Unreal MCP

## 31. Working Configuration

**[REQUIREMENT / current context]** Claude Code CLI is connected to Unreal MCP. The initial budget is approximately **$24 in Claude API credits**, and the selected model is **Claude Sonnet 5.5**. Verify actual model availability/name and pricing within the working environment; the balance may change as requests are made.

**[PROPOSAL]** Use one primary agent with medium reasoning/effort where available; increase it only for difficult debugging. Do not spawn subagents by default. Avoid spending the budget on lengthy, repetitive architectural discussion where a testable incremental change can be made.

## 32. MCP Usage Rules

1. **Once at the start**, discover the available Unreal MCP tools, particularly for editor state, assets, Blueprint, map actors, UI, compilation, PIE/logs, and saving.
2. Do not invent tool/API names; call only tools that actually exist.
3. For each milestone, inspect only relevant schemas and assets, not all available ones.
4. Do not retry identical failing calls without analyzing the error.
5. Perform editor-changing operations sequentially when needed; save afterward.
6. Retrieve targeted errors from logs; avoid inserting megabytes of stack traces into context.
7. Maintain concise `TASKS.md` and `DEVLOG.md` reflecting actual status; do not copy this entire GDD into them.
8. Do not assume a Blueprint compiling proves it functions in gameplay.
9. Reserve both time and credits for a real packaged Windows build and critical fixes.

## 33. API Budget Plan [PROPOSAL, NOT A FORECAST]

Approximate allocation of the $24 budget:

| Area | Internal target |
|---|---:|
| UE/MCP/Git audit and first-step planning | $1.50 |
| Guild ↔ Dungeon loop, quest, save foundation | $5.00 |
| Character, combat, one enemy | $5.00 |
| XP, Level 2, V/S/M, Spellsword | $5.00 |
| Integration and basic polish | $3.50 |
| **Reserve for packaging and critical bugs** | **$4.00** |
| **Total** | **$24.00** |

This is a **budget allocation**, not a prediction or guarantee that implementation will cost this amount. The model cannot automatically see the API balance without a separate billing integration. Check actual spending through Claude Code `/cost` and Console billing after the audit, first complete loop, combat, evolution, and before packaging.

**Budget stop rule:** When the remaining balance approaches the reserved $4, stop implementing optional systems. Spend the remainder only on verification and release blockers. If the first small milestone proves unexpectedly expensive, reduce MCP result volume and inspect what the tools are returning.

## 34. Git and Version History

- Inspect actual `git status`, branch, remotes, `.gitignore`, and Git LFS configuration.
- Track `.uasset`/`.umap` with LFS when correctly configured and consistent with the existing repository.
- Exclude `Saved`, `Intermediate`, `DerivedDataCache`, temporary build output, secrets, and unnecessary large files.
- Create small **local commits after verified milestones**.
- **`git push` only after explicit approval from the creator.** Never force-push, discard others' changes, or create a public repository without permission.
- A commit does not prove the packaged game works.

## 35. Agent Autonomy Policy

**Allowed autonomously:** Audit, targeted implementation of approved P0 features, saving assets, compilation, supported tests, concise development log updates, safe local commits.  
**Requires approval:** GitHub push, publishing/submitting the game, destructive Git operations, major design changes, purchasing paid assets or services, installing unnecessary plugins.

The agent must never announce “everything is complete” if it has only created files.

---

# Part IX. Post-Jam Development Roadmap

## 36. Version 0.1 — Stable Foundations

- Refactor and harden SaveGame/Quest logic based on real testing.
- Full selection of all three starting essences, with meaningful differences.
- Hybrid classes for every pair, including `M→S ≠ S→M`.
- Better controls, animations, and combinations of physical and magical actions.
- Two or three enemy types with distinct roles and counterplay.

## 37. Version 0.2 — Generation and Replay Variety

- Procedural room placement + Delaunay/MST + corridor routing.
- Different room roles, optional branches, secrets.
- Encounter tiers, loot, more complex bosses.
- Repeatable expeditions with varied scenarios.

## 38. Version 0.3 — Fantasy World and Professions

- Expanded Guild and surrounding areas.
- Forge, enchanting, alchemy, crafting, gathering materials.
- Reputation, NPC stories, factions, contracts.
- Unique items and their synergies with essences.

## 39. Version 0.4 — Adaptive Director and AI R&D

- Rule-based director with measurable tension.
- Telemetry collection (with permission) and simulators.
- Test contextual bandits, DQN/PPO only after valid metrics and baselines exist.
- A/B comparisons against fixed scenarios and interpretable rules.
- Fair-play constraints and limits on adaptive changes.

## 40. Version 0.5+ — Cooperative Play and Social Guild

- Co-op lobby; replication of movement, abilities, and enemies.
- Team V/S/M synergies and group encounters.
- Profiles and persistence protected from reward duplication/tampering.
- Friends, guild economy, and auction only after validating the network architecture.

The post-jam roadmap may change based on playtest findings. Move to another stage only when the previous core is stable.

---

# Part X. Final Development Contract

## 41. What Makes This Game “Fateshaped”

**Established identity:** This is an action RPG about adventures in a fantasy world and the persistent growth of a personally shaped hero. The Guild is a separate home level. A fairy who looks like a middle-aged man introduces the game once, then works at reception. The player accepts a quest, enters a standalone Dungeon level through a portal, defeats enemies, earns XP/items/materials, returns to the Guild, and grows stronger. Level, chosen essences, and inventory **never reset between runs**. New essence selections unlock only at particular levels. The ordered essence sequence determines combat identity.

**Jam focus:** Demonstrate this concept through two short expeditions, one compact dungeon, and one distinctive `S→M` evolution. Avoid unnecessary machine learning, networking, and complex crafting.

## 42. Definition of Done for the Jam

The game is ready only when:

- [ ] The Windows executable runs outside the editor.
- [ ] On the first launch, the hero enters the Guild and completes a short introduction.
- [ ] On later launches, the introduction is skipped, and the fairy is accessible at reception.
- [ ] The hero has V/S/M resources and functional combat.
- [ ] A quest can be accepted in the Guild, enabling the portal.
- [ ] The portal takes the hero to a separate Dungeon map.
- [ ] A combat encounter and win/fail conditions exist.
- [ ] Completion grants XP/item **exactly once per `RunId`**.
- [ ] After returning, the hero sees retained rewards and Level 2.
- [ ] Level 2 grants one persistent essence choice in the Guild.
- [ ] `S→M` changes a usable combat ability.
- [ ] The second expedition uses the saved class.
- [ ] Restarting the game does not erase class, XP, level, or inventory.
- [ ] No known critical bug prevents completing the full loop.
- [ ] Submission materials are prepared; official rules are checked before final submission.

---

## 43. Glossary

| Term | Meaning |
|---|---|
| **Guild / Hub** | Separate persistent home map between expeditions |
| **Quest** | Definition of a mission with prerequisites, objectives, and rewards |
| **Run** | A specific attempt to complete a quest, identified by its own `RunId` |
| **Dungeon** | Separate mission level with a combat objective |
| **Essence** | Selected foundational combat affinity V/S/M |
| **EssenceHistory** | Ordered list of every essence selected |
| **Class** | Current combat style derived from essence history |
| **Level** | Persistent character level earned through XP |
| **PendingEssenceChoices** | Number of earned but unspent essence selections |
| **SaveGame** | Persistent local save across application launches |
| **GameInstance** | Runtime state that survives map changes within a session |
| **PIE** | Play In Editor, Unreal's in-editor testing mode |
| **Vertical Slice** | Short, cohesive playable experience demonstrating key systems |
| **Dungeon Director** | System that manages pacing and difficulty of future encounters |
| **P0 / P1 / POST-JAM** | Required now / desirable now / future development |

## 44. Final Principle

> **Build the adventure, not the framework.**  
> **The player should return from the first adventure stronger, choose a new path, and want to try it in the next one.**

If a new system does not help deliver this experience in a working build, it must wait.
