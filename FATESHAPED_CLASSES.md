# Fateshaped — Essence Classes

This specification covers the three starter classes and all nine classes unlocked by a second essence. The starter cards translate the supplied design. The second-tier kits and their tuning values are playtest proposals.

| First essence | Level 1 class | Choose V at Level 2 | Choose S at Level 2 | Choose M at Level 2 |
|---|---|---|---|---|
| **V** Vitality | Barbarian | Juggernaut | Berserk | Blood Mage |
| **S** Stamina | Warrior | Mercenary | Swordsman | Soul's Blade |
| **M** Mana | Wizard | Paladin | Spell weaver | Mage |

## Shared rules

### Progression and saving

- At Level 1, interact with the Essence Altar in the Guild to choose V, S, or M with an empty history. Before choosing, the hero can move, jump, aim and interact. Closing the selection screen preserves the opportunity.
- Reaching Level 2 grants one pending essence choice. Confirming it at the Guild altar appends the new essence to the history and unlocks the corresponding class and second skill. Repeated essences are valid. The first class remains playable while the choice is pending.
- The ordered history is authoritative. Apply the new class and consume the pending choice only when its save succeeds. Rebuild the kit from the history on spawn and reload. Keep level, XP, appearance, inventory, equipped items and pending choices. If a class weapon is needed, supply it without discarding prior equipment.
- An existing exact `[S]` save becomes Warrior. An existing exact `[S, M]` save becomes Soul's Blade and retains access to Arcane Wave. The current prototype calls these paths Swordsman and Spellsword respectively. Use ordered history to migrate them because Swordsman is now the display name for `[S, S]`. For an unsupported history, retain the original record, load its longest supported prefix and give the player a clear message.
- Clear temporary actions, spawned effects, held inputs and cooldowns on death, map travel or reload. Keep the current quest, defeat and reward rules.

### Inputs and camera

| Action | Keyboard and mouse | Xbox | PlayStation | Behavior |
|---|---|---|---|---|
| `Attack_Button` | Left mouse | RT | R2 | Use the class's basic attack. |
| `AimMode_Button` | Right mouse | LT | L2 | Hold for a closer camera, reticle and aiming stance. Release to return to the normal camera. Move at 70% speed while aiming. |
| `Interaction_button` | E | X | □ | Interact with Gerald, the altar, a chest, or another nearby interactable. |
| `Jump_Button` | Space | A | ✕ | Jump. |
| `Evade_Button` | Shift | B | ◯ | Perform the class's evade. |
| `Skill_Button_1` | F | LB | L1 | Use the first class skill. |
| `Skill_Button_2` | Q | RB | R1 | Use the second class skill. The slot is empty on the three starters. |

### Combat and resource defaults

- **V** is health on all paths and has no passive regeneration. A V cost must leave at least 1 V. **S**, when active, recovers 25/s after 0.60 s without spending. **M**, when active, recovers 8/s up to a cap of 60, including from zero. Zero-max resources are hidden. The HUD displays one decimal place and gameplay values retain full precision.
- Fill the active pools on the first class choice, entry to the Guild, expedition start and profile load. On evolution, preserve current values in existing pools up to their new maximums and fill newly activated pools. The class cards govern any exceptions. Regeneration runs during active gameplay.
- The Guild supports the same moves and cooldowns. V-cost skills in Guild practice spend and restore no V. S and M use their regular costs and recovery. A training dummy gives contact feedback without healing, XP, or rewards. Real dungeon kills use the existing XP path and are counted once.
- Ordinary locomotion, camera, jump and interaction carry across classes. An unspecified action requires the grounded, free state. Directional melee starts from camera heading unless specified. A move with steering uses movement input as stated in its card. When two movement speed limits apply, use the lower one. Actions fire once per press or per specified hold/release cycle.
- Heavy Axe Swing, Sword Cut and Arcane Spark stop walking on activation and keep the player planted until the action ends or transitions into an allowed cancel. Melee deals damage only when its active sweep makes contact, and Arcane Spark deals damage only when its projectile hits.
- A shared evade takes the movement direction at press or the camera's horizontal forward direction at neutral. The S/M roll plays over **0.90 s**. The swept capsule follows the authored pace for up to **340 cm**. Solid geometry may stop travel early. Immunity and the action lock last until the roll ends. Any named class cooldown can further delay reuse. Barbarian Backstep follows its own card.
- Times in the cards are real seconds at the authored playback rate. Start animation and its opening cues together. Check contacts during the marked animation window using swept volumes. Stop authored displacement on a solid obstruction or a forbidden floor edge. Walls occlude strikes and intercept projectiles. Damage values are before target defenses. Keep damage, spent costs and cooldowns already committed when a move is interrupted.
- Unless the card gives a different rule, start cooldown and spend resources when a valid action begins. Reject invalid input without cost or cooldown. An evade cancels only a move that permits it. When two eligible actions request the same transition, resolve evade, Skill 2, Skill 1, then basic attack. Finish the last contact sample before entering a cancel at the end timestamp. A release after a canceled or automatically released hold has no effect.
- Provide a start cue, contact or release cue, and a readable failure cue for each new move. Asset names in these cards are candidates. Match the timing and silhouette on the player rig before finalizing a clip or effect.

The altar and active-resource HUD are target behavior from the supplied design. The current first choice through Gerald and the current three-bar HUD are the integration baseline. The ordered-history rules and existing Arcane Wave are documented in [TASKS.md](TASKS.md) and [FATESHAPED_PROJECT.md](FATESHAPED_PROJECT.md).

### Acceptance for every path

- Open the altar at Level 1 and on the Level 2 milestone. Check that backing out, restarting, or encountering a save failure preserves the available choice. Confirm one choice and reopen the altar to verify it cannot be spent again.
- Exercise each binding on keyboard and mouse, Xbox and PlayStation layouts. Verify that the starter's empty Skill 2 does nothing and that the mapped Skill 2 becomes available after evolution.
- Recreate each path through selection and through reload. Verify the kit, active resource bars, quest state, and preservation of XP and equipment. Repeat after the Guild-to-dungeon transition and after defeat.
- Run the 45-second feel tests with no target, XP, loot or scripted objective. Test mechanics in an encounter separately. Record actual runtime evidence for the proposed parameter values.

## V — Barbarian

### 1. Identity and unlock

**Barbarian** · `V_Barbarian` · `[V]` · Vitality starter. Choose V first at the Guild altar at Level 1.

**Player fantasy** — “I carry a great battleaxe, commit to one heavy swing and throw myself forward when I am willing to stake my health.” Vitality lets the player choose between keeping a safety margin and spending it to close distance.

### 2. Play in an empty room

- **Swing → leap → turn.** Make a broad swing, let the blade complete its arc, then press F as soon as the leap transition opens. Turn after landing and swing when ready. Vary the pause before the leap and aim the body toward different landing points. The weight of the axe, sudden forward motion and landing give feedback on an empty floor.
- **Windup → backstep → re-entry.** Start a swing, cancel it with a backstep, turn and re-enter from another angle. Compare a backstep before contact with one after the blade passes. Swing and backstep remain available at 1 V.

### 3. Kit and combat role

Heavy Axe Swing replaces the basic attack, Backstep replaces the common evade and Fury Leap occupies F. Q is empty. There is no passive or separate guard stance. Equip a class battleaxe and retain other items in inventory.

Use the leap to reach one target, the broad swing to hit a group, and the short backstep to stay within striking distance. Against a boss, save enough room for the backstep and spend V during a clear opening. A blocked leap may return less health than it costs.

### 4. Resources

**V 200 · S 0 · M 0.** Heavy Axe Swing and Backstep are free. Fury Leap spends **15 V** on start in an expedition. Its first live contact heals **50% of effective health removed** after defense and overkill, up to maximum V. A full block, miss or dummy gives zero healing. Guild rehearsal uses no V.

At 80 V, start the leap at 65 V. A hit removing 40.8 health restores 20.4, leaving 85.4 V. Killing a target with 8 health remaining restores 4 V.

### 5. Action cards

| Action | Input, timing and contact |
|---|---|
| **Heavy Axe Swing** · `HeavyAxeSwing` | Attack with a fixed camera heading. **0.95 s** duration, **0.25–0.42 s** contact, **1.20 s** cooldown. Sweep a **22 cm** radius along the axe blade. Deal **34 physical damage** once each to up to three targets. Fury Leap may cancel recovery after the contact window, including after a miss. |
| **Fury Leap** · `FuryLeap` | F from grounded idle or the allowed swing recovery. Move immediately along locked **body** heading for up to **320 cm**, with a **55 cm** arc over **0.55 s**. Cooldown **3.00 s**. Sweep the body capsule extended **50 cm** forward during **0.25–0.43 s**. The first live target contacted in the window ends the travel and takes **1.2×** the basic attack's damage. If a live enemy blocks travel before the window, stop and check that same target during the swing while it remains in reach. A wall or forbidden ledge ends travel without dealing damage. The leap grants no invulnerability. Other combat actions wait for landing or the end of the action, whichever is later. |
| **Backstep** · `BarbarianBackstep` | Shift, movement input or backward from the body at neutral. Travel **60%** of normal evade distance over **0.30 s**, with the first **0.24 s** using the existing evade immunity. The next backstep is available **0.50 s** after the animation ends. Can cancel a basic attack before or after contact. Unavailable during Fury Leap. |

**Timing example** — Swing at 0.00 s, finish its active window at 0.42 s, leap until 0.97 s and swing again at 1.20 s. The leap uses recovery time while the swing's cooldown continues.

### 6. World, UI and assets

Gerald calls Barbarians the adventurers who look for a fight they will remember for the rest of their lives. That reputation leaves the hero's appearance and personality to the player.

- **Altar** — “Wield a heavy axe. Spend Vitality to leap into battle.”
- **Unlock** — “Barbarian unlocked · Fury Leap”
- **Gerald** — “A good fight gets your blood moving. The trick is having enough left for the next one.”
- **Feedback** — A heavy whoosh on the swing, overhead axe near the middle of the leap, a landing thud and dust, and a separate hit spark. Use “Not enough Vitality” or the cooldown indicator on refusal.
- **Asset candidates** — `SM_Viking_Axe`, `MM_ChargedAttack` and `MM_Dash`. The leap needs a readable overhead pose, strike and landing. Align the blade contact with the authored window.

### 7. Persistence and acceptance

- At 15 V the leap fails without spending. At 16 V, an expedition leap into empty space leaves 1 V. Repeated Guild leaps leave V unchanged.
- Backstep before the contact window prevents the hit. Backstep after the hit preserves it. Fury Leap preserves the basic attack cooldown.
- Turn the camera away from the stationary body and verify that the leap follows the body. A wall between hero and foe stops travel and prevents damage and healing.
- Reload `[V]` and verify the Barbarian kit and full 200 V with a single visible resource bar.
- **45-second empty-room check** — The player tries both the swing-to-leap and windup-to-backstep sequences and predicts when the blade will finish its arc.

## VV — Juggernaut

### 1. Identity and unlock

**Juggernaut** · `VV_Juggernaut` · `[V, V]` · evolved from Barbarian. At level 2 or later, spend an unspent essence choice on a second V at the Guild altar.

**Player fantasy** — “I plant my feet, let the force build, and decide when to drive it into the floor.” The first V gives the axe and health-powered commitment. The second makes bracing and releasing weight a deliberate part of the fight.

### 2. Play in an empty room

- **Set → turn → break.** Hold Q to plant the axe, turn the camera toward a doorway, then release just before or just after the full-brace cue. The early release makes a compact ripple. The full release sends a longer pulse down the room. The lower stance, impact and visible range let the player judge the release by feel.
- **Leap → land → anchor.** Use Fury Leap to cross the room, then plant and release Stonebreak across the direction of travel. Repeat with different landing angles and release times. The player can feel the transition from fast movement to complete arrest, even when the pulse hits empty space. Swing and Backstep remain available when V is too low for either skill.

### 3. Kit and combat role

Retain Heavy Axe Swing, Fury Leap, Backstep and the Barbarian axe. Add **Stonebreak** to Skill 2. No persistent passive or equipment requirement beyond the axe profile. Against one foe, catch an approach in the short pulse. Against a group, line up a narrow lane. Against the Warden, release during a punish window. Bracing cannot reposition the hero and exposed sides stay vulnerable.

### 4. Resources

**V 240, S 0, M 0.** No passive V recovery. Fury Leap retains its **15 V** cost and healing rules. Stonebreak costs **18 V** at release in expeditions and never heals. At least **1 V** must remain after payment. At 40 V, releasing Stonebreak leaves 22 V, enough for one Fury Leap. Guild practice preserves the brace, damage feedback, recovery and cooldown with zero V cost, zero healing and zero rewards.

### 5. Action card

**Stonebreak** · `Stonebreak` · `Skill_Button_2` (Q / RB / R1). Press from neutral on the ground or after Heavy Axe Swing's active window. Hold to brace in place. Camera yaw sets facing until release. From **0.12 s** of hold, eligible attacks within **±50°** of the brace facing are reduced by **45%**. Floor hazards, the Warden stomp and attacks outside that sector use normal damage. The guard ends at release. Release before **0.50 s** for a **320 cm** pulse with **85 cm** half-width and **34 physical damage**. Release at or after **0.50 s** for a **520 cm** pulse with **110 cm** half-width and **50 physical damage**. At **0.80 s**, release automatically once per press.

On release, validate and pay **18 V**, start a **3.8 s** cooldown and lock horizontal heading. The axe contacts the ground **0.18 s** into the release clip. The pulse travels along the floor at **1,600 cm/s**, checking a box with the specified half-width, **45 cm** longitudinal depth and **75 cm** height each frame. Static geometry clips forward travel. A separate wall check protects targets to either side of the lane. Hit up to **four** living enemies once each. Boss mitigation and kill XP use normal rules. Dummies show contact feedback only. Release recovery ends **0.55 s** after release. During brace before release, Backstep cancels without payment, and a valid simultaneous Backstep takes priority. Normal damage leaves brace active. Defeat and level transitions end the action. Backstep and other attacks remain locked during release recovery. A committed pulse that meets a wall or misses keeps its cost and cooldown.

### 6. World, UI and assets

Gerald teaches Juggernauts beside the oldest gate in the Guild. He asks them to listen for the point where the axe stops trembling.

- **Choice** — “Brace with your axe. Release a ground strike before or after it reaches full weight.”
- **Unlock** — “Juggernaut unlocked · Stonebreak”
- **Gerald** — “Plant both feet. When the handle goes quiet, the floor will do the talking.”
- **Asset candidates** — retain `SM_Viking_Axe`. Use `MM_ChargedAttack` as a starting point for the impact pose and `BP_FX_Hit` for contact. Author a braced idle, a grounded impact event and floor FX that match the two pulse widths and ranges. Use a shoulder drop and low double beat at full brace. Show “Not enough Vitality” when release fails.

### 7. Persistence and acceptance

- `[V, V]` restores Juggernaut, its axe kit and a single 240 V bar after loading. `[V]` continues to restore Barbarian.
- Hold **0.30 s** and **0.65 s** to verify distinct range and damage. At **18 V**, reject release without cooldown. At **19 V**, a whiff leaves **1 V**. A **40**-damage eligible frontal hit in brace loses **22 V**. A side hit loses **40 V**.
- Verify a wall stops the pulse, a target receives one hit, and Backstep before release prevents both pulse and payment.
- **45-second feel test.** The player chooses short and full pulses, reorients during brace and uses the landing-to-plant transition. They can identify the full-brace moment by pose or sound.

## VS — Berserk

### 1. Identity and unlock

**Berserk** · `VS_Berserk` · `[V, S]` · evolved from Barbarian. At level 2 or later, spend an unspent essence choice on S at the Guild altar.

**Player fantasy** — “Once the axe has passed, I can spend Stamina to carry its momentum around a new angle.” V establishes the heavy swing and health-powered leap. S opens controlled footwork inside the axe's recovery. Berserk begins by committing the axe.

### 2. Play in an empty room

- **Swing → left or right pivot.** Swing into open space. As the blade passes, hold a movement direction and tap Q. The new cut travels around the chosen side. Alternate directions and watch the resulting path rather than waiting for a target reaction.
- **Leap → land → swing → pivot out.** Cross the room with Fury Leap, land facing one direction, then swing and pivot along a different angle. Practice early and late entries into the pivot window. When S runs out, alternate the inherited swing and Backstep while it returns.

### 3. Kit and combat role

Retain the Barbarian axe, Heavy Axe Swing, Fury Leap and free Backstep. Add **Rage Pivot** to Skill 2. Rage Pivot has no passive prerequisites and receives its cancel window from the basic swing. Against a single foe, follow a heavy swing with a cut at a new angle. In a group, choose the side with open floor. Against the Warden, save Backstep for defense and plan where the pivot ends. The pivot commits the hero without invulnerability.

### 4. Resources

**V 200, S 100, M 0.** No passive V recovery. Fury Leap retains its **15 V** cost and healing. Rage Pivot costs **24 S**. Stamina returns at **25/s** after **0.60 s** without a Stamina spend. At 0 S, the hero needs **1.56 s** from the last spend to regain **24 S**. The Guild uses ordinary S and the shared V practice rule.

### 5. Action card

**Rage Pivot** · `RagePivot` · `Skill_Button_2` (Q / RB / R1). Grounded only. Press from **0.42 s** through **0.78 s** after starting Heavy Axe Swing, including after a whiff. Use camera-relative horizontal movement input, clamped to **120°** of turn from the swing's locked heading. Neutral input turns **60°** to the character's right. Lock the chosen turn at activation. Move along a curved path up to **185 cm** over **0.30 s** using swept capsule collision. Sweep the axe from the swing heading toward the travel heading during **0.10–0.30 s**. Collision with walls, enemies or an unsupported ledge stops travel. The axe can finish its active frames in place behind the clipped stop point, with wall checks still active.

On start, spend **24 S** and start a **1.80 s** cooldown. Sweep a **24 cm** sphere along the blade between poses and deal **24 physical damage** to up to **two** living enemies, once per target. An enemy hit by the preceding Heavy Axe Swing can be hit by the follow-up as a separate action. No stun, refund or healing. The action ends at **0.46 s**. The original Heavy Axe Swing cooldown continues from its own start. Rage Pivot and Fury Leap are competing exits from the same swing. At equal timing, a valid Backstep takes priority, then Rage Pivot, then Fury Leap. Once Rage Pivot starts, other actions wait through its recovery. Death and transitions terminate the sweep immediately. Failed input spends nothing and leaves the current action running.

### 6. World, UI and assets

Guild sparring partners call the move a wager. The fighter puts their next step behind the axe and shows where they intend to be a moment later.

- **Choice** — “Carry a heavy axe swing into a directional follow-up at a Stamina cost.”
- **Unlock** — “Berserk unlocked · Rage Pivot”
- **Gerald** — “If the axe is already moving, give it somewhere useful to go.”
- **Asset candidates** — keep the Barbarian axe, use `MM_Attack_02` as a follow-up pose candidate and `BP_FX_Hit` on contact. Add mirrored footwork and weapon trails that reveal the turn, with a sharp foot plant at the end. Briefly highlight Skill 2 during the cancel window. Show “Follow a heavy swing” or “Not enough Stamina” where applicable.

### 7. Persistence and acceptance

- `[V, S]` restores Berserk with full V and S and the inherited F slot. Swapping the order resolves its own evolution.
- Q at **0.35 s** fails, Q at **0.44 s** spends **24 S** and winds down by **0.90 s**, and Heavy Axe Swing becomes ready at **1.20 s**. A whiff still opens the pivot window. Testing with S below **24** leaves all costs and timers untouched.
- Left/right inputs produce corresponding paths, a wall clips movement and contacts, and two living targets take at most one hit each.
- **45-second feel test.** The player alternates left and right pivots, uses a different turn after landing and can predict when the next heavy swing is available.

## VM — Blood Mage

### 1. Identity and unlock

**Blood Mage** · `VM_BloodMage` · `[V, M]` · evolved from Barbarian. At level 2 or later, spend an unspent essence choice on M at the Guild altar.

**Player fantasy** — “I pay with my own Vitality to leave blood magic in the room, then pull it back through an angle I create.” The first V establishes close-range axe combat and bodily risk. The second essence gives the axe fighter a positional spell that starts with a health sacrifice.

### 2. Play in an empty room

- **Cast → circle → recall.** Throw a Blood Thread toward one side of the room. Step left or right before pressing Q again. Compare the outbound and returning paths. The hovering anchor and tightening strand show how the new standing position changes the return line.
- **Cast → leap sideways → recall.** Turn the body and use Fury Leap to cross the room at an angle, then recall the thread from the landing point. Try different launch ranges and recall timings before the anchor expires. Basic axe swings and Backstep remain available while Mana recovers or V is too low to place another thread.

### 3. Kit and combat role

Keep the Barbarian axe, Heavy Axe Swing, Fury Leap and free Backstep. Add **Blood Thread** to Skill 2. The spell uses one temporary anchor. It adds no persistent passive or slot. Against one foe, step past their side to bring the return line back through them. In groups, use the axe and outbound line to make space, then recall through a different lane. Against the Warden, watch the escape route before committing V. Blood Thread serves the melee kit's positioning. A paid commitment can miss twice.

### 4. Resources

**V 200, S 0, M 60.** No passive V recovery. M restores at **8/s**, including from zero. Blood Thread costs **12 V + 18 M** on placement and keeps at least **1 V**. Recall has no further cost or healing. Example at 30 V and 20 M, placement leaves **18 V and 2 M**, excluding concurrent regeneration. Fury Leap retains its own payment and healing. In the Guild, both skills use zero V with zero healing. Thread still spends and regenerates M normally.

### 5. Action card

**Blood Thread** · `BloodThread` · `Skill_Button_2` (Q / RB / R1). Grounded press from neutral or after Heavy Axe Swing's active window. Aim along camera yaw at start or along the crosshair ray while Aim Mode is held, projected onto the floor at the launch height. Placement has a **0.14 s** windup, launch, and **0.18 s** recovery. Lock launch direction at press. Check both costs before windup and again at launch. At launch, pay both together and start a **3.50 s** cooldown. Before launch, Backstep cancels without cost or cooldown. No target is needed.

The outbound sphere has **20 cm** radius and travels **1,800 cm/s** for up to **540 cm**. It deals **16 magical damage** to up to **three** living enemies, once each, continuing through them. A static obstruction clips the path and places the anchor on the reachable side. At maximum range, place the anchor at the last valid position. If the launch origin is blocked, cancel placement with no cost or cooldown. The anchor persists for **2.50 s** from placement and shows its remaining time.

Press Q again from neutral after placement while the anchor exists, even while the placement cooldown is active. The recall consumes the anchor after **0.08 s** and fires a **26 cm** radius sweep toward the character's horizontal position sampled at that moment. Travel at **2,200 cm/s** for up to **750 cm**, stopping at walls. Trail ends at the sampled destination or at the range limit. Deal **28 magical damage** to up to **three** living enemies, once each. Both legs can hit the same enemy separately. Release recovery ends **0.20 s** after recall press. Backstep before the recall fires interrupts it and leaves the anchor available until expiry. Defeat, expiry and level transitions clear pending effects. Dummies give one feedback contact per leg. The ordinary damage and XP path handles living enemies. Whiffs, blocks, expired anchors and wall-clipped paths give no refund.

### 6. World, UI and assets

Gerald describes Blood Thread as an axe fighter's spell. The caster supplies the first drop and the rest of the spell follows the fighter's movement around the room.

- **Choice** — “Spend Vitality and Mana to place a blood anchor. Move and pull the thread through a new line.”
- **Unlock** — “Blood Mage unlocked · Blood Thread”
- **Gerald** — “Leave a little behind. Then decide what stands between you and it.”
- **Asset candidates** — use a hand or axe mounting point, a dark ribbon derived from `BP_FX_Wave` for a test effect, and `BP_FX_Hit` for impact. Author an anchor mesh or looping Niagara effect, a readable countdown pulse and distinct outbound and recall sounds. Display “Not enough Vitality” or “Not enough Mana” before placement. Replace the slot prompt with “Recall” while an anchor is active.

### 7. Persistence and acceptance

- `[V, M]` restores Blood Mage with full V and 60 M, both slots intact. Death or level travel clears the thread and its recast.
- Launch with **12 V** or below **18 M** rejects both costs. A committed empty cast deducts both once, and recall charges nothing. A canceled windup preserves both resources.
- Test outbound placement against a wall, a recall lane blocked by a different wall, expiry, and a target on both clear lanes receiving **16** then **28** damage. A dummy provides feedback without healing, XP or rewards.
- **45-second feel test.** The player casts toward empty space, intentionally changes the return angle, and learns to use Fury Leap to reposition without losing track of the anchor.

## S — Warrior

### 1. Identity and unlock

**Warrior** · `S_Warrior` · `[S]` · Stamina starter. Choose S first at the Guild altar at Level 1.

**Player fantasy** — “I control distance with sword and shield. I decide when to cover up and when to step in.” Stamina pays for advancing and evading, so aggressive positioning uses some of the same reserve as defense.

### 2. Play in an empty room

- **Cut → short step → cut.** Swing, then tap F once the cut finishes. The short step changes distance without breaking the quick rhythm.
- **Brace → turn → long step.** Hold F, turn the shield with the camera, choose a diagonal and release. The stance, shield sound and longer travel show the result before any contact. At zero S, use free cuts and normal footwork while waiting for the step to return.

### 3. Kit and combat role

Sword Cut replaces the basic attack and Shield Advance occupies F. The evade uses shared travel and animation. Q is empty. There is no passive. The class supplies a sword and shield and retains previous items.

Meet a frontal strike with the shield and step into useful range. Against groups, use the step to get to the edge of the crowd. Against the Warden, a shield reduces blockable frontal strikes. Move clear of the stomp or evade it.

### 4. Resources

**V 100 · S 100 · M 0.** Shield Advance costs **20 S**, the evade costs **25 S**, and Sword Cut is free. Taking a blockable hit while the same guard is active carries no additional S cost. Ignoring regeneration between actions, two advances and two evades leave 10 S. A further 0.60 s pause and 0.40 s of regeneration bring it back to 20 S.

### 5. Action cards

| Action | Input, timing and contact |
|---|---|
| **Sword Cut** · `SwordCut` | Attack with a fixed horizontal camera heading. **0.52 s** duration, **0.14–0.24 s** contact, **0.65 s** cooldown. Sweep a **16 cm** radius along the blade. Deal **22 physical damage** once each to up to two targets. |
| **Shield Advance — guard** · `ShieldAdvance` | Press F from grounded idle to pay **20 S** and start the **1.20 s** cooldown. Move at **55%** speed while holding and steer the shield with the camera. A blockable strike in the frontal **100°** sector deals **40%** of its incoming damage. Flanking attacks, traps and floor stomps bypass the guard. |
| **Shield Advance — release** | Hold under **0.30 s** for up to **90 cm** in **0.16 s**. Hold at least **0.30 s** for up to **180 cm** in **0.26 s**. Auto-release at **0.65 s**. Use movement input or forward from the body at neutral. The guard ends at release. From **0.06 s** through step end, sweep a **34 cm** sphere in front of the shield for **12 physical damage** to one target. There is no extra stun. A wall, body or forbidden floor edge stops the step and leaves the cost spent. Lock attack, evade and jump through the step plus **0.12 s** recovery. |
| **Evade** | Shift for **25 S**, using shared travel and animation. Can cancel Sword Cut. During the held guard, an affordable evade cancels the stance and consumes its own cost. During the step, wait for recovery. |

### 6. World, UI and assets

The Guild teaches this stance to people expected to hold a doorway and make it back from an expedition. Gerald talks about discipline and spacing.

- **Altar** — “Fight with sword and shield. Hold to guard, then release to advance.”
- **Unlock** — “Warrior unlocked · Shield Advance”
- **Gerald** — “A shield buys you a moment. Make sure you know what you’ll do with it.”
- **Feedback and asset candidates** — Try `SM_Sword` and `MM_Attack_01`. The shield needs a model or proxy, a left forearm attachment, a guard pose and step motion. Signal the long-step threshold with the pose and a strap click. Use metal impact and shield recoil on a block. Show “Not enough Stamina” when a required cost cannot be paid.

### 7. Persistence and acceptance

- A frontal blockable strike for 20 costs 8 V in the stance. A flank or Warden stomp for 20 costs 20 V.
- Holds of 0.15 s and 0.40 s give short and long steps for 20 S each. A wall stops travel and prevents a hit on a target behind it.
- At 19 S, F fails without spending. A basic cut still works and F becomes available again after regeneration.
- Load an old exact `[S]` save and verify Warrior with level, items and pending choice intact. Verify the old `[S, M]` save loads Soul's Blade and Arcane Wave.
- **45-second empty-room check** — The player alternates step lengths and directions and can identify the long-step threshold during a cut-and-step rhythm.

## SV — Mercenary

### 1. Identity and unlock

**Mercenary** · `SV_Mercenary` · `[S, V]` · evolves from Warrior. At level 2 or later, spend one pending essence choice on V at the Guild altar after reaching `[S]`.

**Player fantasy** — “I set my shield, pay what it takes to hold my ground, then put the point of my sword where it matters.” Stamina trained the footwork and shield first. Vitality gives the Warrior a costly, committed answer when yielding ground would be worse.

### 2. Play in an empty room

- **Cut → plant → thrust.** Swing with `Attack_Button`, then press `Skill_Button_2` just after the blade passes. See the forward motion stop in a planted stance, then feel the straight thrust and abrupt brake. Try waiting for a different angle before committing.
- **Raise shield → change plan.** Start Shield Advance. Release it to step past an imaginary opponent, or press `Skill_Button_2` during the guard to plant and thrust in the direction the shield faces. Alternate the two outcomes while moving around the room. In an expedition with too little V, the Warrior's shield step and sword remain available.

### 3. Kit and combat role

Keep Warrior's movement, sword and shield, Sword Cut, Shield Advance and 25 S evade. Add **Paid Stand** to `Skill_Button_2`. No other passive or gear modifier. Time the thrust against a Skirmisher, hold a clear front angle near a group, and keep room to step out of the Warden's floor attack. The V-first Berserk channels effort into axe tempo and pursuit. The S-first Mercenary starts from the shield and chooses when to stand and trade ground.

### 4. Resources

**V 145 · S 100 · M 0.** Use the standard S recovery and no passive V recovery. Paid Stand costs 12 V in expeditions and leaves at least 1 V. It neither heals nor returns V on contact. For example, starting with 50 V, a successful activation leaves 38 V. A 20-damage blockable hit during the brace then takes 6 V, leaving 32. In the Guild, Paid Stand uses its normal timing without a V debit or restoration. S actions use their normal costs.

### 5. Action card — Paid Stand · `PaidStand`

- **Input.** Press `Skill_Button_2` on the ground from neutral, during Shield Advance's guard before its step, or after Sword Cut's active window ends at 0.24 s. The latter transitions preserve the original costs and cooldowns. A failed check leaves the current action in place. Shield release and the new press on the same frame resolve the new press first when eligible.
- **Timing and movement.** At start, spend 12 V and start a 3.00 s cooldown. Capture horizontal camera yaw. Plant for 0.32 s with no voluntary translation. Until 0.32 s, incoming blockable attacks from the forward 90° sector deal 30% of their normal post-defense amount. Side hits, hazards and the Warden's floor stomp follow their ordinary rules. From 0.32–0.52 s, advance up to 145 cm using swept movement. Walls, enemy bodies and unsafe ledges stop movement. Full action ends at 0.80 s.
- **Contact.** From 0.38–0.52 s, sweep an 18 cm sphere from the blade's previous to current tip position. The first visible live enemy contacted takes 36 physical damage once. World geometry blocks contact. No forced stagger. Practice targets play contact feedback only. A miss or interrupted action keeps the cost and cooldown. No voluntary cancel. Defeat, a hard interrupt or a map transition discards remaining movement and damage.

### 6. World, UI and assets

- **Altar preview** — “Plant your shield and spend Vitality to hold firm, then thrust.”
- **Unlock** — “Mercenary unlocked · Paid Stand”
- **Gerald** — “You can always give up another step. Knowing which one to keep is the job.”
- Reuse the Warrior's equipment. Use `MM_Attack_02` as a candidate for the thrust and `BP_FX_Hit` for contact. Author a brief planted shield pose, leather creak at commitment and a clean push-off. The shield drops before the thrust so the end of protection reads in pose. On failure, show “Not enough Vitality” or the remaining cooldown.

### 7. Persistence and acceptance

- Resolve exact `[S, V]` to `SV_Mercenary`. Retain Warrior while the choice is pending. The shared save and transition policies apply.
- At 12 V in an expedition, press `Skill_Button_2` during guard. Paid Stand fails, Shield Advance continues and V stays the same. At 13 V, a deliberate whiff leaves 1 V and starts cooldown.
- Send the same 20-damage blockable hit from the front at 0.20 s, from the side at 0.20 s and from the front at 0.40 s. Expected damage is 6, 20 and 20. Verify one damage event per thrust and no contact through a wall.
- In 45 seconds of free practice, the player chooses both the mobile shield release and the planted alternative, and can identify when the shield is up from the pose alone.

## SS — Swordsman

### 1. Identity and unlock

**Swordsman** · `SS_Swordsman` · `[S, S]` · evolves from Warrior. At level 2 or later, spend one pending choice on a second S at the Guild altar.

**Player fantasy** — “My steps set the edge of the fight. I cut, pass to a new angle and decide how soon to cut again.” A second Stamina selection refines the existing weapon and shield into quicker offensive footwork. The Swordsman commits less body weight than the Barbarian and keeps a deliberate shield option from the Warrior.

### 2. Play in an empty room

- **Cut → slip left → cut → slip right.** After a basic cut, hold a lateral direction and tap `Skill_Button_2` as the edge passes. Change sides and aim the next basic attack independently. Follow two floor tiles as guideposts and vary how sharply you cross them.
- **Slip past → turn back → shield step.** Take a directional Slip Cut past a pillar corner, then pivot and use the shortest Shield Advance to return. Try a neutral versus a moving Slip Cut. The planted beat, blade swish and lateral trail show the change in trajectory even on air. Free cuts and walking bridge a depleted S pool.

### 3. Kit and combat role

Keep Sword Cut, Shield Advance and Warrior's evade, movement and equipment. Add **Slip Cut** to `Skill_Button_2`. There are no new passives or animation-speed modifiers on inherited attacks. Slip Cut changes the line of a duel, arcs across the edge of a group and lets the player leave the line of committed boss swings. True evasion still requires the separate evade action. Spend on footwork with enough S left to defend.

### 4. Resources

**V 100 · S 100 · M 0.** Standard S recovery, no passive V recovery. Slip Cut spends 16 S, Shield Advance 20 S and evade 25 S. For example, 41 S covers a Slip Cut and one evade exactly before regeneration. Sword Cut, normal movement and aiming remain available at 0 S. Guild practice uses the normal S rules.

### 5. Action card — Slip Cut · `SlipCut`

- **Input and aim.** Press `Skill_Button_2` on the ground from neutral or after Sword Cut's 0.24 s active window. Capture horizontal camera heading for the blade. If movement input magnitude is at least 0.20, use its ground direction for translation. Otherwise slip to camera right. Aim and movement directions remain fixed until this cut ends.
- **Timing, cost and movement.** At start, debit 16 S and start a 0.80 s cooldown. Sweep the capsule up to 160 cm over 0.20 s. Walls, enemy bodies and unsafe ledges stop the translation. The blade continues on its normal timeline from the actual reached position. The full action lasts 0.34 s. From 0.25 s, a new `Attack_Button` press may buffer one Sword Cut for the first legal frame at or after 0.34 s. Keep the base cut's own cooldown and expire the buffer 0.20 s after the input. The buffered attack uses fresh camera heading.
- **Contact and cancel.** From 0.10–0.20 s, sweep a 17 cm sphere along the moving blade. Deal 16 physical damage to at most two live enemies, once per target, with clear line of strike. No invulnerability or added stagger. Other attack and skill inputs during movement are ignored unless specified above. From 0.21 s, evade may cancel recovery if 25 S is available and discard buffered attack input. Keep any already incurred costs and damage. A whiff pays the cost. A rejected input pays nothing. Hard interruption discards the remaining contact and buffer.

### 6. World, UI and assets

- **Altar preview** — “Use a quick sliding cut to change your angle while keeping a guard in reserve.”
- **Unlock** — “Swordsman unlocked · Slip Cut”
- **Gerald** — “The edge goes where your hand sends it. The fight goes where your feet take it.”
- Use `MM_Dash` and `MM_Attack_02` as timing references for a dedicated sliding slash. Keep clean foot contact and an open view of the blade. A brief scrape and a short lateral trail describe the movement without relying on color. Display the actual reason if the action is unavailable.

### 7. Persistence and acceptance

- Resolve exact `[S, S]` to `SS_Swordsman`. The old trial's name “Swordsman” on `[S]` still migrates via ordered history to Warrior. Complete and reload the `[S, S]` choice separately.
- Directional and neutral casts follow their locked movement vectors. Verify 160 cm on flat ground, clipped travel against a wall and no hit through geometry. At 15 S, Slip Cut fails without affecting a Sword Cut in recovery.
- Trigger a Slip Cut at 0.24 s after Sword Cut starts. A new basic action starts no earlier than the original cut's 0.65 s cooldown, even when buffered. Check damage once per target and evade after the contact window.
- In 45 seconds of free practice, the player willingly changes sides, times the transition from the cut and demonstrates a deliberate stop using the retained shield stance.

## SM — Soul's Blade

### 1. Identity and unlock

**Soul's Blade** · `SM_SoulsBlade` · `[S, M]` · evolves from Warrior. At level 2 or later, spend one pending choice on M at the Guild altar. The running prototype's `[S, M]` class is named **Spellsword**.

**Player fantasy** — “I swing through the opening and let my sword carry the cut across the room.” Stamina established a grounded sword-and-shield cadence. Mana projects that cadence into space. The reverse path begins from spellcasting and adds footwork. Soul's Blade first chooses a sword line and then decides whether to send it past melee reach.

### 2. Play in an empty room

- **Cut → wave → advance.** Make one close-range cut, turn the camera during the recovery and tap `Skill_Button_2` to send a wave down a different lane. After recovery, use Shield Advance to follow the line the wave drew. Compare a straight sequence with a right-angle sequence.
- **Guard → step → draw a line.** Use a short shield step to move away from a pillar, then release a wave parallel to a wall. Repeat from a different angle to see how the corridor shortens against geometry. On low M, continue practicing short shield steps, ordinary movement and sword cuts while the charge rebuilds.

### 3. Kit and combat role

Keep Sword Cut, Shield Advance, Warrior's evade, shield and ordinary equipment. Add **Arcane Wave** to `Skill_Button_2`. No new passive. Use the wave to reach a separated enemy or cut through enemies lined up at a doorway. It can strike several targets along one line. Warden defenses, defeat rewards and blockers still use ordinary rules. The wave is a committed swing, leaving Shield Advance and evade to manage close pressure between casts.

### 4. Resources

**V 100 · S 100 · M 60.** Use standard resource recovery and parent costs. The first unlock fills M to 60. Arcane Wave costs 30 M. Two waves consume 60 M in total, before accounting for regeneration. From 0 M, the next cast becomes affordable after 3.75 seconds of active regeneration. In this design, Guild practice has the normal M cost and recovery even when a training target is struck. The current prototype's practice refund is a legacy rule.

### 5. Action card — Arcane Wave · `ArcaneWave`

- **Input and timing.** Press `Skill_Button_2` from neutral on the ground or after Sword Cut's active window ends. Check at least 30 M at start. Capture the horizontal camera aim using the existing Arcane Wave direction rules. Play a 0.55 s swing. At 0.20 s, on blade extension, check and spend 30 M, resolve one wave and start a 1.10 s cooldown. A hard interrupt or an eligible evade before release cancels the wave without M spend or wave cooldown. After release, an evade may interrupt recovery from 0.29 s onward. An emitted wave remains resolved and cooldown remains active. The base Sword Cut cooldown remains independent.
- **Contact.** Retain the existing instantaneous, player-origin corridor, **850 cm long × 340 cm wide**, with targets within **175 cm** vertically. The central world trace shortens the corridor to 5 cm before the first obstacle. Check line of strike to each candidate as well. Each valid enemy within the corridor takes **40 magic damage** once using the existing damage and reward pipeline. Multiple enemies may be hit. No effect behind the actor. A whiff or a blocked wave spends M. Practice targets yield effects only.
- **Feedback.** Trace a brief outline along the valid corridor at release using the same dimensions as the contact check. Flash the blade and play a distinct blade-ring at release. No visible target is required. Report low Mana and cooldown clearly. Retune the animation around its contact pose if necessary, keeping contact, expense and release synchronized.

### 6. World, UI and assets

- **Altar preview** — “Keep your shield technique and send a broad arc of magic from your sword.”
- **Unlock** — “Soul's Blade unlocked · Arcane Wave”
- **Gerald** — “That looks like the same cut you learned yesterday. The far wall would disagree.”
- Use the existing sword, `BP_FX_Wave`, `BP_FX_Hit` and `MM_Attack_03` as integration candidates. Align the wave effect and sword extension with the release event. A cloak or cosmetic sword skin does not change the damage corridor.

### 7. Persistence and acceptance

- Resolve all exact `[S, M]` profiles, including legacy Spellsword saves, to `SM_SoulsBlade`. Retain Arcane Wave, the ordered history, earned choices, equipment, progression and quest state. `[M, S]` resolves separately. Check the 60 M cap after load and travel.
- Start at 60 M, fire two waves and compare M immediately before and after each release to verify the 30 M debit. When a press is processed with 29 M, the action fails. Evade before release and confirm no wave or M debit. An empty cast at sufficient M still pays.
- Test targets on both sides of the width boundary and behind a thin wall. Check one damage event each on three unobstructed in-lane enemies. Validate that the effect clips to the same corridor as damage.
- In 45 seconds of free practice, the player discovers how to redirect the wave between a cut and a step, can anticipate wall truncation, and returns to the sword-and-shield rhythm while Mana recovers.

## M — Wizard

### 1. Identity and unlock

**Wizard** · `M_Wizard` · `[M]` · Mana starter. Choose M first at the Guild altar at Level 1.

**Player fantasy** — “I shape a spell in my hand and choose between a broad quick release and a focused shot at range.” Mana sets the rhythm between freely cast sparks and stronger orbs.

### 2. Play in an empty room

- **Spark → turn → spark.** Move between two points, turn the camera and fire at different angles. Hold aim to shoot at beams, columns or a far wall and watch each shot's path.
- **Broad orb → pause → focused orb.** Tap F for a broad, slow projectile. Hold it on the next cast until the shape and sound change, then release toward a distant point. Keep practicing footwork and sparks while Mana returns.

### 3. Kit and combat role

Arcane Spark replaces the basic attack and Arcane Orb occupies F. Q is empty. The evade uses shared travel and animation, costs no resource and has a **1.00 s** cooldown. There is no passive. Cast from the hand. A visual staff or focus follows the same launch-point rules.

Play at medium or long range. The focused orb rewards a steady aim at one target. The broad orb helps catch a nearby moving foe and stops at the first target. Against a boss, prepare outside its immediate reach. Lower V and reduced movement during preparation make spacing matter.

### 4. Resources

**V 80 · S 0 · M 60.** Spark and evade are free. Both Orb forms cost **20 M** on release at the cast's launch event. Recovery from 0 to 20 M takes **2.50 s**. The same costs and recovery apply in the Guild.

### 5. Action cards

| Action | Input, timing and contact |
|---|---|
| **Arcane Spark** · `ArcaneSpark` | Attack, fix aim at cast start. **0.38 s** duration, launch at **0.10 s**, **0.65 s** cooldown. Projectile radius **9 cm**, speed **1,600 cm/s**, range **550 cm**, **12 magic damage** to its first live target. |
| **Arcane Orb** · `ArcaneOrb` | Press F on the ground with at least **20 M** and a ready cooldown. Move at **60%** speed while holding. Release to begin a **0.28 s** cast, fix aim at cast start and check M again at launch (**0.12 s** into the cast). Spend **20 M** and start **1.20 s** cooldown at that event. Auto-release once at **0.80 s** held. |
| **Broad Orb** | Release before **0.45 s** held. Projectile radius **32 cm**, speed **900 cm/s**, range **600 cm**, **24 magic damage** to its first live target. |
| **Focused Orb** | Release at or after **0.45 s** held. Projectile radius **14 cm**, speed **1,800 cm/s**, range **1,100 cm**, **32 magic damage** to its first live target. |

While aiming, launch from the hand toward the reticle's world point or the end of the camera ray in empty space. Outside Aim Mode, use the horizontal camera heading. The launch point is the hand's current transform. An obstruction at the hand can intercept a shot even when the camera sees past it. Sweep from the previous projectile position each frame and stop at the first wall, live enemy, practice dummy or range limit. The evade can cancel preparation or casting before launch. A canceled Orb has no Mana cost or cooldown. A committed launch is paid on a miss, wall or block. The next basic attack waits for the current cast to finish.

### 6. World, UI and assets

The Guild teaches Wizards to show the shape of an unfinished spell. Broad magic occupies space and focused magic gathers around a small core.

- **Altar** — “Cast free sparks. Hold Arcane Orb to focus it into a faster, longer shot.”
- **Unlock** — “Wizard unlocked · Arcane Orb”
- **Gerald** — “Hold it until the note changes. Then see how far you can send it.”
- **Feedback and asset candidates** — Try `MM_Attack_03`, `NS_Magma_Shot_Projectile` and `BP_FX_Hit`. At **0.45 s**, collapse the broad pulse into a narrow silhouette and play a higher note. Set up a hand launch point and a pose that reads correctly at **0.12 s** into the cast. Indicate “Not enough Mana” or cooldown on refusal.

### 7. Persistence and acceptance

- Holds of 0.20 s and 0.50 s produce broad and focused orbs at 20 M each. Holding past 0.80 s produces one launch.
- At 19 M, Orb fails and Spark and evade remain available. Evading before launch creates no Orb and spends no M.
- A wall between the hand and the reticle intercepts the shot. Two enemies in line receive one hit total.
- Save `[M]`, travel and reload. Verify Wizard, full 60 M, V and M on the HUD, and no surviving projectiles.
- **45-second empty-room check** — The player alternates Spark with both Orb releases, predicts the selected form from its shape or sound and resumes casting after Mana recovers.

## MV — Paladin

### 1. Identity and unlock

**Paladin** · `MV_Paladin` · `[M, V]` · evolves from Wizard. At level 2 or above, spend one pending choice on `V` at the Guild essence altar. The player keeps the Wizard kit until confirming. Mana provides the spells. Vitality gives the caster the confidence to establish a position and hold it. Blood Mage starts with an axe and makes magic from a bodily cost. Paladin starts with aimed magic and uses Vitality to stay near a ward.

### 2. Play in an empty room

- **Plant → circle → shoot.** Tap Q without aiming to form a ward underfoot. Walk around its rim, alternate Spark and Orb, then watch the ring light under the character at each release. Step across the rim to compare the sound and visual response inside and outside.
- **Place → advance → pivot.** Hold aim and put a ward several steps ahead. Sprint or evade into its center, turn and fire back across the room. Try the same route with the ward near a corner and then in the open. The placement preview and the floor effect show exactly which ground the caster chose to hold.

### 3. Kit and combat role

Keep Arcane Spark, Arcane Orb, the Wizard evade and the shared movement, jump, aim and interaction. Add **Sanctuary Seal** in slot 2. Keep the Wizard hand emitter and optional visual focus. Against one enemy, cast from within the seal while lining up an Orb. Against groups, place a seal by the edge of the fight to retain a route out. The ward softens direct strikes from the Warden. Floor effects and traps require movement.

### 4. Resources

**V 135, S 0, M 60.** This evolution raises the health cap from 80 to 135 and applies the shared evolution rule to current V and M. V has no passive regeneration. M regenerates at 8 per second. Orb remains 20 M, Sanctuary Seal costs 18 M, Spark and evade are free. At zero M the player still has basic movement, Spark and evade. From full M, one seal and one Orb spend 38 M before regeneration, leaving 22 M. The same costs and regeneration apply in the Guild.

### 5. Action card

**Sanctuary Seal** · `SanctuarySeal` · `Skill_Button_2`. Press Q / RB / R1 while grounded and able to start a new cast. Without aim, target the floor under the caster. While aiming, trace the crosshair to the floor. Clamp the horizontal distance to 400 cm, then trace down to the walkable surface at that point. Require a valid floor within 100 cm vertically of the caster's feet and a clear path from the caster to the proposed center. The aimed preview shows invalid locations with a broken outline.

| Part | Rule |
|---|---|
| Timing | Capture the center on input. Cast for 0.40 s, commit at 0.18 s. Evade can cancel before commit. Jump, attack and other skills wait until cast recovery ends. |
| Cost and lifetime | At commit, revalidate floor clearance and M, spend 18 M and start a 4.50 s cooldown. The single fixed ground field lasts 3.20 s. A failed or interrupted pre-commit cast spends nothing. |
| Coverage | Protect the caster when the capsule base is within 175 cm horizontally and within 35 cm vertically of the seal's floor level. |
| Absorption | Soak 35% of qualifying incoming physical or magical enemy attack damage after ordinary mitigation. The field can absorb 42 damage total and expires when this budget reaches zero. Remainder goes to V. Dodge immunity resolves before the ward. Ground hazards, traps and environmental damage bypass it. |
| Feedback | Show the placed ring, remaining capacity pips and a hit ripple. Spark or Orb released from inside makes the ring pulse without spending capacity. Crossing the edge changes the sound and foot glow. Expiration clears the ring and the HUD indicator. |

### 6. World, UI and assets

Guild wardkeepers use luminous floor seals to buy time to cast. **Choice** — “Keep your spells. Place a seal that softens direct attacks while you stand inside.” **Unlock** — “Paladin unlocked · Sanctuary Seal.” **Gerald** — “Draw yourself a little patch of courage. Then keep your eyes outside it.” Candidate cast animation `MM_Attack_03`. Create a ground decal or Niagara ring that matches the 175 cm radius and an impact ripple that reads by shape without color. Reuse a hand flash from the Wizard cast and add a soft tone on entry.

### 7. Persistence and acceptance

Save the exact path and derive the new class from `[M, V]`. Active seals expire on defeat, travel or reload. A 40-point direct hit inside the seal absorbs 14 and applies 26 damage, leaving 28 capacity. The same hit outside applies 40. An aimed seal takes its position once and cannot appear across a wall. Cancel at 0.10 s and verify no seal, no spend and no cooldown. Relog and confirm Paladin, retained Spark and Orb, 60 M and no leftover ring. In 45 seconds of empty-room practice, look for both underfoot and projected placements, plus a voluntary shot from each side of the boundary.

## MS — Spell weaver

### 1. Identity and unlock

**Spell weaver** · `MS_SpellWeaver` · `[M, S]` · evolves from Wizard at level 2 or above by spending one pending choice on `S` at the Guild essence altar. Mana establishes the cast. Stamina lets the caster change where it comes from after preparation has already begun. Soul's Blade starts with swordplay and extends it with magic. Spell weaver starts charging a projectile and changes the angle before release.

### 2. Play in an empty room

- **Prepare → sidestep → release in motion.** Hold F, give left or right movement input, tap Q and release F before the step ends. Watch the trail and projectile separate from a single moving point.
- **Prepare → sidestep → settle → release.** Let the step finish, turn toward a different wall and then release F. Compare shooting early, shooting from the endpoint and deliberately skipping the step. Movement and Spark remain available while either resource refills.

### 3. Kit and combat role

Keep Spark, both forms of Orb, hand casting and optional visual focus. Add **Weave Step** in slot 2. Wizard's free evade becomes a 25 S evade with its existing distance, animation and 1.0 s cooldown. Shared movement, jump, aim and interaction remain. The step changes position with no contact damage or dodge protection. Its advantage is moving the cast origin without dropping the preparation. Against groups, step out of a blocked firing lane. Against the Warden, reserve S for a true evade if an attack is imminent.

### 4. Resources

**V 80, S 100, M 60.** S is newly active and fills on selection. It returns at 25/s after 0.60 s without spending. M returns at 8/s. Orb costs 20 M. Weave Step costs 22 S and standard evade costs 25 S. Spark remains free. At zero S, aim and cast from the current position. At zero M, use Spark and preserve S for ordinary evasion until Orb becomes available. Guild costs and recovery match the expedition rules.

### 5. Action card

**Weave Step** · `WeaveStep` · `Skill_Button_2`. Press Q / RB / R1 once per grounded Arcane Orb preparation, before cast start. The Orb's F hold time continues during the step.

| Part | Rule |
|---|---|
| Direction and travel | Capture camera-relative movement input at step start. Default to camera right if input is neutral. Sweep the capsule up to 210 cm over 0.22 s. End travel at a solid obstacle, blocking enemy or floor edge. If a path check shows less than 30 cm of travel at start, reject the step. |
| Payment | Spend 22 S and start a 1.20 s cooldown immediately on a valid step. A rejected step spends nothing and keeps the Orb held. Obstacles after movement starts do not refund S. |
| Cast link | Releasing F, or reaching its inherited 0.80 s hold limit, starts the normal Orb cast while the step continues. Aim direction locks at cast start and spawn position comes from the actual hand at the 0.12 s launch event. When a Q press and an F release arrive together, test the step first. Step may end before or after launch. The inherited 0.45 s form threshold remains. |
| Interruption | Until both committed actions end, new jump, evade, basic attack and skill inputs are declined. Before a step starts, Wizard's normal cancel rules apply. During the step, defeat, hard interruption or travel clears the action. A canceled Orb before release spends no mana. A started step remains paid. |
| Collision and signals | Orb checks clear space between caster and hand and along the launched projectile path as usual. Step strikes nothing and never grants immunity. A short heel trail shows the path and a flash marks the actual firing point. |

### 6. World, UI and assets

Guild runners describe this style as keeping the spell steady while the feet do the dangerous part. **Choice** — “Use Stamina to change your angle without dropping Arcane Orb.” **Unlock** — “Spell weaver unlocked · Weave Step.” **Gerald** — “Keep the spell in your hand. Move everything else.” Test `MM_Dash` as a candidate for the step and a split upper-body casting layer to keep the release readable. Add a directional foot trail and reserve a separate sound from the ordinary evade.

### 7. Persistence and acceptance

Derive the class from `[M, S]`. On defeat, travel or reload, clear held Orb, step and projectile. With full resources, a valid moving cast spends 22 S at step start and 20 M at release. Test a low obstacle and a wall within 20 cm. The wall rejects the step without losing the held cast. In a 45-second empty-room session, observe a cast released mid-step and one from its endpoint. The player should be able to predict the firing point and distinguish relocation from an evade.

## MM — Mage

### 1. Identity and unlock

**Mage** · `MM_Mage` · `[M, M]` · evolves from Wizard at level 2 or above by spending one pending choice on a second `M` at the Guild essence altar. Wizard already changes a spell's speed and shape. Mage learns to decide how it moves through space. The second Mana choice deepens control while keeping the Mana cap at 60.

### 2. Play in an empty room

- **Draw two arcs.** Hold Q and give a left or right direction, then release. Alternate the curve around either side of a pillar or compare where left and right casts touch the far wall. The preview turns the room into a practice surface.
- **Straight Spark → curved cast.** Use Spark to mark a straight route, then use an arc release from the same point to look for a different arrival. Change aim height or swap the side of the bend. While waiting for M, rehearse the preview when the pool allows or fire free Sparks along a new route.

### 3. Kit and combat role

Retain the Wizard hand casting, Spark, both Orb forms, free evade and shared controls. Add **Arcane Bend** in slot 2. No new equipment requirement, mobility bonus or passive. Orb remains the direct choice for range and on-demand accuracy. Bend gives the player a route around finite cover. Sweep collision remains active on the entire arc, so a bad launch angle still strikes the near side. Against groups or a boss, decide whether the indirect approach is worth its slower setup.

### 4. Resources

**V 80, S 0, M 60.** M returns at 8/s. Arcane Bend costs 22 M, Arcane Orb remains 20 M and Spark and evade are free. No passive V regeneration. From full, two Bend casts cost 44 M total and leave 16 M before regeneration. From 0 M it takes 2.75 s to restore the cost of Bend. Standard M costs and regeneration apply in the Guild.

### 5. Action card

**Arcane Bend** · `ArcaneBend` · `Skill_Button_2`. On ground, hold Q / RB / R1 to preview an arc and release to cast. Choose left or right during the hold by exceeding 35% lateral movement input relative to camera. The last chosen side is retained through neutral input. If no direction was chosen, use a straight path. The preview updates as aim changes. The player moves at 55% speed during the hold. Auto-release at 0.90 s, once per press.

| Part | Rule |
|---|---|
| Cast and payment | Cast for 0.32 s after release. Lock side and starting aim heading at cast start. Spend 22 M and start a 1.40 s cooldown at the 0.14 s emission event. Insufficient mana or cancel before emission produces no projectile, spend or cooldown. |
| Curve | Start at the real hand emitter in the locked aim direction. At 1,450 cm/s, turn the heading at a constant rate per centimeter traveled, reaching 70° of yaw toward the chosen side at 900 cm. Preserve the initial pitch clamped to ±35°. Use the same curve for preview and projectile. The neutral path has 0° turn. |
| Contact | Sphere radius 18 cm, 900 cm maximum path length, about 0.62 s lifetime. Sweep the emission and every segment in steps of at most 20 cm. Stop at first solid obstruction, valid living enemy, practice target or range limit. Deal 28 magic damage once to a living enemy through current mitigation and XP handling. |
| State rules | Evade cancels preparation or pre-emission cast. Attack and Orb wait until cast recovery ends. A released projectile persists until its own end, defeat or map travel. A shot into a wall, block or empty space stays paid. |
| Signals | Show the centerline, endpoint and first projected obstruction. An asymmetric hand swirl and left/right sound cue identify the chosen side. The released trail follows the swept path. |

### 6. World, UI and assets

Guild mages treat the shape of a spell's path as part of the casting gesture. **Choice** — “Choose a curve before release to send a spell around cover.” **Unlock** — “Mage unlocked · Arcane Bend.” **Gerald** — “Anybody can throw a spark in a line. Show me what happens when you give it a turn.” Reuse the Wizard hand cast pose as a candidate and create a ribbon in Niagara for the curved trail. Use a simple ground or world-space curve and endpoint marker for the preview. Silhouette distinguishes the curves independently of color.

### 7. Persistence and acceptance

Save both M selections in order. Clear preview and airborne spells on defeat, travel or reload. Left and right at the same pose create mirrored routes. Check one unobstructed cast and one curve that hits the near face of a pillar. When the route is blocked, stop at the first contact without damaging anything behind it. Confirm 21 M rejects the cast, 22 M can emit once, and the cap remains 60 after reloading. In 45 seconds of empty-room play, look for a deliberate change in side and aim, followed by a repeatable route around a piece of scenery.
