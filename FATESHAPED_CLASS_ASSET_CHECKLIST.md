# Fateshaped — Class Asset Checklist and Production Prompts

This handoff covers the three starters and all nine Level 2 evolutions. Prioritize the actions the player repeats and the moments where a pose, effect or sound changes a decision. In Guild rehearsal, the movement, weight, timing and transitions should be enjoyable before a target or reward enters the picture.

- **First pass** — finish the shared controls and the three starter kits, then give each evolution a distinctive Skill 2 silhouette and cue.
- **Polish** — add optional gear looks, extra audio takes and spoken Gerald lines after the normal-input packaged expedition passes.
- **Keep** means the asset is already used and can be calibrated. **Adapt** means the named asset is present or the current behavior provides a starting point. **Make** marks a missing asset or cue. A candidate still needs a rig, timing or import check.

The ordered paths, input bindings and timings come from [class cards](FATESHAPED_CLASSES.md). Current gaps come from the [class test report](FATESHAPED_CLASS_TEST_REPORT.md) and [combat implementation](Source/Fateshaped/FSClassCombat.cpp). Durations below are in game seconds, measured from the start of the named action or from release where specified.

## Running the prompts

Copy the relevant brief below and append the prompt for one asset or class. Split a bundle at its named clips or events if your tool returns one file per request. Include an existing mannequin, mesh, silhouette or clip as a reference when the tool supports it. Use the prompts for generation, manual authoring or briefing an artist. The file names and export settings are proposed handoff conventions.

| Asset type | Copyable shared brief |
|---|---|
| **Animation** | Create gameplay animation for Fateshaped, a grounded fantasy action RPG seen from a close third-person shoulder camera. Use the project's Unreal mannequin as the target rig. Export one FBX per clip and a timing sheet with contact, release, foot plant and recovery markers in seconds. Keep driven travel in-place and the root stable. Show weight transfer, planted feet and clear hand or weapon silhouettes. Supply left/right versions where asked, with the sword, shield and casting hand staying on their assigned sides. Separate hold loops, releases and additive layers where requested. |
| **3D equipment** | Create a game-ready low-poly fantasy equipment prop for Fateshaped. Match worn steel, dark timber, leather bindings and restrained decoration. Preserve an identifiable silhouette at shoulder-camera distance. Provide a real-scale mesh in centimeters, grip-centered pivot or forearm attachment reference, UVs and PBR Base Color, Normal, Roughness and Metallic textures. Include an orthographic front, side and grip view for review. Use a single material slot when practical. |
| **VFX** | Author a real-time Unreal Niagara, ribbon or decal effect for a torch-lit stone dungeon. Keep the gameplay silhouette readable in grayscale. Use short fills, clean edges and controlled emissive values. Expose the listed origin, scale, travel, clip point, charge state or lifetime as inputs. Attach the effect to the actual socket or sampled path. Provide separate start, commit, impact, wall stop, miss, cancellation and expiry cues wherever the action card calls for them. Enemy danger telegraphs keep visual priority. |
| **Audio** | Create dry game SFX as individual WAVs, preferably mono, 48 kHz, 24-bit. Provide clean attacks, a short tail and two or three variations for high-frequency cues. Separate startup, readiness, emission, contact, failure and expiry so each can be triggered independently. Favor tactile foley and restrained magic. World cues need a clear point source. UI cues need a stable level and an audible distinction between selection, confirmation and refusal. |
| **UI and icons** | Create a cohesive fantasy action RPG icon family. Single high-contrast silhouette per action, square silhouette mask or transparent PNG, 256 px source, clean at 24–32 px. Use cutouts, line weight and shape to communicate hold, ready, blocked or recast. Keep text and device glyphs as separate UI elements. Provide normal, ready, unavailable and cooldown state references as applicable. |
| **Voice** | Record one dry English WAV per Gerald line in the optional voice list below. Gerald is a middle-aged guild receptionist with a dry, friendly delivery and a short pause before the punch word. Speak the supplied text verbatim. Give two takes, understated and playful. Use the approved text for the corresponding subtitle. |

## Shared player assets

| Status | Asset or cue | Copyable English prompt |
|---|---|---|
| Adapt | Locomotion and aim layers | Author a neutral relaxed stance and an aim enter, held aim offset and exit for an over-the-shoulder fantasy hero. Build an upper-body aim layer with a forward eye line, grounded shoulders and directional locomotion underneath. Supply axe, sword-and-shield and empty casting-hand poses. Keep input release responsive, preserve the existing 70% aim movement and avoid steering the camera from the root. |
| Keep/calibrate | Idle, walk/run, strafe and turn, jump start, airborne loop and landing | Review the existing mannequin locomotion on the three equipment silhouettes. Add restrained turn-in-place or recovery adjustments only where needed. Export grounded idle, forward/back/left/right locomotion loops and jump start/air/land references. Keep carried props stable through turns and landing. |
| Adapt | Shared directional evade for S/M | Adapt `MM_Dash` into forward, back and side in-place evasive poses. The capsule owns 340 cm over 0.22 s. Use a clear push-off and recovery foot plant, and preserve the held weapon or casting hand. Supply cloth movement and a small friction puff as separate cues. |
| Adapt | Hit reaction, block reaction and defeat | Create front and side ordinary hit reactions, a short shield recoil additive and a shared collapse with weapon-safe poses. Keep brief hit feedback compatible with protected or committed states. Play the defeat sequence from the defeat signal. |
| Adapt | Common footsteps, jump/land, hit and wall stop | Produce light and heavy stone footsteps, a jump takeoff, normal and hard landings, cloth movement for evade, muted practice contact, flesh/armor contact and a separate stone or wood wall stop. Footsteps need variation and a short tail for corridor repetition. Start from `SFX_Swing` and `SFX_Hit` as placeholders where useful. |
| Adapt | Ground dust, surface hit and practice feedback | Build small dust puffs for foot plants, a weapon contact sparkle, a blunt stone chip and a subdued practice dummy marker. Concentrate the brightest accent at the strike point. Provide dust-only visuals for empty floor contacts and a clean dissolve for a projectile reaching range. |
| Adapt | Altar, class HUD, reticle and input glyphs | Lay out the existing Essence Altar flow with V/S/M, ordered path, class name, one-sentence preview, pending choice, confirm and cancel. Use a readable aim reticle and reserve a distinct broken or crossed shape for invalid placement or blocked aim. Keep the one-decimal active resource values, shared cooldown readout and keyboard/mouse, Xbox and PlayStation glyphs. Check 761 × 441, 720p and 1080p. |
| Make | Essence and basic action icons | Produce three essence marks using a heart or pulse shape for V, a boot or traction shape for S and a faceted light shape for M. Provide separate silhouettes for Heavy Axe Swing, Sword Cut, Arcane Spark, Backstep and shared evade. Keep resource colors secondary to the silhouettes. |
| Adapt | Shared UI sounds and unlock presentation | Create dry select, confirm, back, refusal, cooldown ready and a brief class unlock chime. Add a short altar pulse on successful save confirmation, using the existing `BP_FX_Evolve` and `SFX_Evolve.wav` as candidates. The player still sees a distinct message for cooldown, resource shortage, blocked path or failed save. |
| Polish | Controller feedback | Author short haptic envelopes for light contact, heavy contact, blocked strike and spell commitment. Use subtle one-shot cues with a clear stop at interruption and a settings-controlled intensity. |

## Equipment and points of attachment

Equipment follows the first essence. Evolutions inherit it. The core kits use the same player rig and gear proportions across the twelve paths.

| Status | Equipment | Copyable English prompt |
|---|---|---|
| Keep/calibrate | **V** battleaxe, `SM_Viking_Axe` | Fit the existing broad Viking battleaxe to the mannequin's right-hand grip. Mark the cutting edge and axe head for weapon sweeps and trail sockets. Deliver a grip pose and rear/side silhouette checks for ready, overhead windup, backward retreat and ground impact. |
| Keep/calibrate | **S** sword, `SM_Sword` | Fit the existing steel sword to the mannequin's right hand. Mark blade base, cutting edge and tip for sweeps, tracer and impact sampling. Preserve a clear one-handed sword silhouette with the shield raised. |
| Make | **S** round shield | Create a compact round shield for the left forearm. Use a shallow timber face, restrained steel rim, central boss, grip and two worn leather straps. Start around 52 cm in diameter to match the current visual proxy. Supply front and rear views and forearm attachment reference. Check shoulder-camera framing, guard posture, push and recoil. |
| Keep/calibrate | **M** empty hand and emitter | Set up a palm emitter for the mannequin's right hand. Mark the launch point and direction on Spark, Orb and Bend reference poses, with a second transform for an off-hand support effect. Keep the emitting hand readable during movement and Weave Step. |
| Polish | Branch armor accents and optional focus | Design three modular cosmetic concepts on the same mannequin. V uses broad leather and iron protection and an unobstructed axe arc. S uses articulated forearm armor and a shield-compatible shoulder. M uses layered cloth with clear hands and a short sleeve near the emitter. Supply optional bracer or belt charm concepts for evolutions. An optional staff or focus stays cosmetic and preserves hand emission. Keep the attachment and hit geometry from the base kit. |
| Polish | Existing persistent reward items | Create two matching inventory icons for the game's existing rewards. Crypt Shard is an asymmetric chipped relic with a dark solid silhouette. Arcane Crystal is a faceted prism with a clear pointed shape and a small internal glow. Supply transparent PNGs and optional low-poly inspection props for later use. Both silhouettes should read without color. |

## Production order

| Order | Deliverables | Review condition |
|---|---|---|
| 1 | Fury Leap montage and landing/contact cues, shield model and Shield Advance raise/hold/push, Spark and both Orb forms with distinct hand poses and sounds | The three starters feel distinct while moving, aiming, attacking and recovering in an empty room. |
| 2 | Stonebreak short/full, Rage Pivot left/right, Blood Thread and Recall, Paid Stand, Slip Cut left/right, Arcane Wave, Sanctuary Seal, Weave Step and Arcane Bend | Every evolution adds an identifiable new pose, motion or projectile and a cue that helps time it. |
| 3 | Shared reticle, slot states and control glyphs, companion sounds and practice feedback | The HUD and audio communicate why an action is ready, spent, blocked or canceled in a dim corridor. |
| 4 | Optional armor, cosmetics, reward icons, voice and haptics | The core loop has passed packaged play with normal controls. |

## V branch — Barbarian, Juggernaut, Berserk, Blood Mage

### V · Barbarian

- [ ] **Animation — adapt Heavy Axe Swing and Backstep, make Fury Leap.** “Author a 0.95 s heavy axe swing with contact through 0.25–0.42 s and a recovery that can flow into a leap. Use `MM_ChargedAttack` as a weight reference. Author Fury Leap as one continuous 0.55 s takeoff, overhead axe silhouette, descending cut, landing and recovery. The capsule drives up to 320 cm with a 55 cm arc. Present the cut during 0.25–0.43 s. Supply an arrested-travel blend that can complete the strike against an early enemy or land after a clipped route. Author a 0.30 s backward retreat with the torso and axe facing the threat, plus directional side versions. The neutral retreat travels 204 cm.”
- [ ] **VFX — make weapon trail, takeoff, impact and healing.** “Track the swing's edge and leap strike during their active windows. Add takeoff grit, landing dust at the actual feet and a separate axe contact burst. Add a brief upward curl close to the hero only when real Vitality is restored. Keep a short dust-only landing for a wall or whiff.”
- [ ] **Audio — make weighted swing and leap layers.** “Produce heavy axe whoosh, short takeoff grunt, weighted boot landing, hard axe contact and a quiet restorative rise. Keep landing and enemy contact separate. Add a compact boot scrape for Backstep and stone impact for an obstruction.”
- [ ] **UI — adapt Fury Leap and Backstep.** “Draw a descending axe icon with a small upward recovery accent and a separate backward footprint icon. Show actual restored V to one decimal on hit, a miss cue on whiff and Vitality or cooldown refusal when relevant.”

### VV · Juggernaut

- [ ] **Animation — make Stonebreak brace and release.** “From axe ready or swing recovery, plant both feet and brace the axe across the front. Make the protected pose readable at 0.12 s of hold, and add a shoulder drop at full charge, 0.50 s held. Author short and full grounded axe slams with ground contact 0.18 s after release and recovery through 0.55 s. Supply a pre-release Backstep transition.”
- [ ] **VFX — make charge and two floor pulses.** “Create a short hard-edged stone pulse 320 cm long × 170 cm wide and a full pulse 520 × 220 cm. Start both at axe impact and travel at 1,600 cm/s along the floor. Use rubble, grit and an advancing edge that ends at geometry. Full charge gets a second low ripple and a stronger edge. Use the current pulse marker for scale review.”
- [ ] **Audio and UI — separate brace, threshold and impact.** “Create a planted boot pair, leather and haft strain, a low double beat at full charge and separate short/full stone impacts. Draw an axe over cracked ground with distinct short and full charge states. Show brace, charged readiness and insufficient Vitality as applicable.”

### VS · Berserk

- [ ] **Animation — make mirrored Rage Pivot follow-up.** “Continue from the outgoing Heavy Axe Swing recovery into a left or right sliding pivot. Keep the axe in the right hand, a visible sweep at 0.10–0.30 s, a committed foot plant and recovery by 0.46 s. The capsule owns the curved path of up to 185 cm over 0.30 s. Aim blends must support the locked turn up to 120°, with the default 60° right turn. Provide a planted finish for clipped travel.”
- [ ] **VFX and audio — show the turn and brake.** “Trace a compact curved axe-edge ribbon that follows the actual turn. Add one grit streak at the push-off, a clean final foot plant, a rising curved whoosh and a firm foot brake. Keep the weapon contact effect separate from the ground movement.”
- [ ] **UI — make the combo window legible.** “Draw a hooked axe and a curved directional arrow. Highlight Skill 2 during 0.42–0.78 s of Heavy Axe Swing, including after a miss. Use separate follow-a-heavy-swing and resource or cooldown messages.”

### VM · Blood Mage

- [ ] **Animation — make placement and recall gestures.** “Keep the battleaxe in the right hand and bring the left hand forward for a clear casting silhouette. Author a 0.32 s placement with a 0.14 s launch, then a separate 0.20 s recall with a sharp pulling gesture at 0.08 s. Leave a clean pre-emission retreat transition.” **Integration check.** The current outbound emitter samples `hand_r`. An off-hand launch requires binding VFX and collision to the same new emitter. For an animation-only pass, keep emission at the current right-hand or axe origin and use the left hand for the gesture.
- [ ] **VFX — replace the sphere with a thread and anchor.** “Create a dark red thread with a distinct moving head for the 20 cm radius outbound cast, up to 540 cm. Plant a compact knotted anchor at the last reachable point for 2.50 s and pulse faster near expiry. Recall shows an inward directional draw and a wider 26 cm radius moving front over up to 750 cm, using the sampled caster destination. Both ribbons end at walls and use separate contact cues. Keep the effect readable on red floors with shape, a pale core and restrained bloom.”
- [ ] **Audio and UI — distinguish the two legs.** “Create a short outward cord snap, anchor set, quiet anchored tick, inward taut-string pull, sharp recall hit and soft expiry. Draw a thread-and-knot icon that changes to an inward arrow and visible countdown while recall is available. Provide distinct Vitality and Mana shortage labels.”

## S branch — Warrior, Mercenary, Swordsman, Soul's Blade

### S · Warrior

- [ ] **Animation — adapt Sword Cut, make Shield Advance.** “Author a 0.52 s one-handed Sword Cut with edge contact at 0.14–0.24 s. Use `MM_Attack_01` as a candidate. Create shield raise, moving guard hold layer, recoil additive, short and long shield pushes and recovery. The guard can turn with the camera while moving. Show the long-step readiness change at 0.30 s held and permit auto-release at 0.65 s. Display a distinct shield-forward push during both 90 cm/0.16 s and 180 cm/0.26 s releases, with contact from 0.06 s to step end and 0.12 s of recovery.”
- [ ] **VFX — make sword trail, block spark and step contact.** “Create a thin steel edge trail and a directional spark at an actual frontal shield block, separate from the blunt shield-bash contact and boot dust. Keep the raised shield silhouette as the main protection cue. Make a small secondary response for wall clipping.”
- [ ] **Audio and UI — separate raise, ready, block and push.** “Create shield lift, strap click at the long-step threshold, directional rim impact, blunt bash, boot push-off and a light sword cut whoosh. Draw a round shield icon with an advance wedge. Use a short/long readiness marker and an explicit Stamina gate.”

### SV · Mercenary

- [ ] **Animation — make Paid Stand.** “Start from neutral, Sword Cut recovery or a raised shield. Plant with the shield forward from 0.00–0.32 s, visibly lower it at 0.32 s, drive a straight sword thrust during the 145 cm advance at 0.32–0.52 s, and settle by 0.80 s. Contact belongs at 0.38–0.52 s. Show an abrupt brake and a weapon-safe finish when travel clips.”
- [ ] **VFX — make brace and thrust accents.** “Use a compact floor scuff under the planted feet, a brief shield recoil on blocked contact and a narrow blade-tip tracer. End the protection accent when the shield drops. Use no persistent barrier or healing VFX for this action.”
- [ ] **Audio and UI — make commitment legible.** “Create a boot stomp, leather creak at commitment, a short shield rim recoil and a straight dry thrust whoosh. Draw shield and sword-tip shapes with distinct planted and thrust states. Surface insufficient Vitality and cooldown using the shared UI.”

### SS · Swordsman

- [ ] **Animation — make Slip Cut and return to Sword Cut.** “Use `MM_Dash` and `MM_Attack_02` as references for a 0.34 s sliding slash. Make left and right footwork variants with the blade in the right hand. Travel is capsule-driven up to 160 cm over 0.20 s. Keep contact at 0.10–0.20 s, leave space for the evade transition from 0.21 s and a smooth transition to a buffered Sword Cut after recovery.”
- [ ] **VFX and audio — distinguish a slash from an evade.** “Provide a short blade ribbon and low lateral grit streak following actual travel. Use a dry sideways scrape with a clean slicing tail. Keep the tracer visible if movement stops and the blade still sweeps. Reserve a quiet wall-stop cue for clipped movement.”
- [ ] **UI — make side movement readable.** “Draw a sword crossing two offset footprints. Use the shared resource and cooldown states. Show a brief valid follow-up cue only when the existing buffer can accept it.”

### SM · Soul's Blade

- [ ] **Animation — adapt an Arcane Wave sword release.** “Author a 0.55 s shield-compatible sword extension with the blade presented at 0.20 s. The hand action reads as a sword release from the rear shoulder for both neutral and Sword Cut recovery entry. Provide clean cancellation before emission and a recovery break pose available from 0.29 s.”
- [ ] **VFX — adapt `BP_FX_Wave` to the actual corridor.** “Create a brief sword flash and broad horizontal corridor effect centered on the player's locked aim. Maximum size is 850 cm long × 340 cm wide. Use the actual obstacle-clipped length. Mark the full usable width with a short contour, and play a separate effect on each confirmed contact. Fade quickly so floor threats remain visible. The hit resolves on emission, so express the whole corridor immediately.”
- [ ] **Audio and UI — make a blade-ring release.** “Produce a close metallic harmonic at release with a brief broad arcane tail, a softer wall cutoff and discrete confirmed target contact. Draw a sword with a broad fan shape. Keep the 30 Mana cost and readiness readable.”

## M branch — Wizard, Paladin, Spell weaver, Mage

### M · Wizard

- [ ] **Animation — adapt separate Spark and Orb gestures.** “Create a free 0.38 s finger cast with launch at 0.10 s. For Orb, create a hand cradle hold with a visible squeeze at 0.45 s, auto-release readiness at 0.80 s, then a 0.28 s release with launch at 0.12 s. Supply aim and movement-compatible upper-body layers and a pre-launch evade exit. Keep the emitter visible through all poses.”
- [ ] **VFX — make three projectile silhouettes.** “Build a tiny pointed Spark, 9 cm radius, 1,600 cm/s, 550 cm range. Form a broad soft-edged Orb with an open halo, 32 cm radius, 900 cm/s and 600 cm range. Form a dense Focused Orb with a thin forward tail, 14 cm radius, 1,800 cm/s and 1,100 cm range. Collapse the held halo at the charge threshold. Use short independent hand flash, live/dummy contact, wall interception and range fade cues. Review `NS_Magma_Shot_Projectile` and `NS_Magma_Shot_Impact` as candidates.”
- [ ] **Audio and UI — sell the two forms.** “Create a restrained snap for Spark, a wider soft flutter for the Broad Orb, a narrow bright pitch shift at focus threshold and a sharper Focused Orb release. Provide a short hold loop with a clean cancel. Draw Spark and Orb icons with expanded-ring and condensed-core held states, readable with the cooldown and 20 Mana requirement.”

### MV · Paladin

- [ ] **Animation — make Sanctuary Seal cast.** “Create a 0.40 s one-handed sealing gesture with a groundward palm and commit at 0.18 s. The targeting stance works underfoot and aimed. Use the Wizard pose family and provide a pre-commit evade exit.”
- [ ] **VFX — adapt preview and make a ring.** “Build a flat 175 cm radius seal using a narrow decal or Niagara ring, alive for up to 3.20 s. Use a solid outline for valid targeting and a broken outline for invalid ground. Add a foot glow inside, a brief boundary cue when crossing, a wall-safe impact ripple on absorption and a smaller sympathetic pulse on Spark or Orb. Keep hazard telegraphs legible through the field.”
- [ ] **Audio and UI — show remaining capacity.** “Create seal placement, soft entry and exit tones, a short absorbed-hit chime and a dry dissolve on depletion or expiry. Draw a floor-circle seal icon and segmented capacity indicator for the 42-point pool, with inside/outside and blocked placement states. Keep the different purposes of activation, absorption and cast pulse audible.”

### MS · Spell weaver

- [ ] **Animation — make lower-body Weave Step.** “Author a directional 0.22 s lower-body step driven by up to 210 cm of capsule movement. Default direction is camera right. Supply left/right and useful front/back variants. Keep a stable upper-body Orb hold and release layer, and compare firing during travel versus after the step. Support a clean foot plant when clipped.”
- [ ] **VFX and audio — show movement and hand release independently.** “Use a short heel trail along actual travel, distinct from evade, and a small hand flash at the Orb's actual emission position. Preserve the broad/focused forms while moving. Add a quick two-foot cloth-and-leather shuffle, a short heel brake and a soft blocked-step scuff. A refused step leaves the held Orb cues intact.”
- [ ] **UI — show availability during the hold.** “Draw an Orb above staggered footprints. Show once-per-hold availability, committed step and recovery clearly. Distinguish a blocked path or low Stamina from insufficient Mana for eventual Orb launch.”

### MM · Mage

- [ ] **Animation — adapt Arcane Bend hold and release.** “Use the Wizard hand pose family. Create a neutral gathering hold and asymmetric left/right hand effects or additive gestures over moving feet. The released clip lasts 0.32 s, with the visual hand emission at 0.14 s. Preserve a planted, readable emitter and a pre-emission cancel transition.”
- [ ] **VFX — adapt the path preview and make the shot ribbon.** “Draw a neutral straight route and mirrored left/right routes turning 70° over 900 cm with pitch limited to ±35°. Use the sampled gameplay centerline, a visible final endpoint and a different obstruction marker. The released 18 cm radius shot travels at 1,450 cm/s. Build a ribbon following its actual swept points and ending at the first obstruction or target. Use shape to identify the chosen side while holding.”
- [ ] **Audio and UI — distinguish left, neutral and right.** “Create subtle left/right curved arpeggios with distinguishable contours, a still neutral tone, a single switch tick and a release that ends cleanly on impact or timeout. Draw a curved projectile icon plus small left/right chevrons and a neutral state. Preserve the cooldown and Mana requirement.”

## Optional Gerald voice

Use these English lines with the shared voice brief. Each line comes from the corresponding class card.

| Path | Spoken line |
|---|---|
| V | “A good fight gets your blood moving. The trick is having enough left for the next one.” |
| VV | “Plant both feet. When the handle goes quiet, the floor will do the talking.” |
| VS | “If the axe is already moving, give it somewhere useful to go.” |
| VM | “Leave a little behind. Then decide what stands between you and it.” |
| S | “A shield buys you a moment. Make sure you know what you’ll do with it.” |
| SV | “You can always give up another step. Knowing which one to keep is the job.” |
| SS | “The edge goes where your hand sends it. The fight goes where your feet take it.” |
| SM | “That looks like the same cut you learned yesterday. The far wall would disagree.” |
| M | “Hold it until the note changes. Then see how far you can send it.” |
| MV | “Draw yourself a little patch of courage. Then keep your eyes outside it.” |
| MS | “Keep the spell in your hand. Move everything else.” |
| MM | “Anybody can throw a spark in a line. Show me what happens when you give it a turn.” |

## Export and integration checklist

- [ ] **Confirm candidate assets before commissioning replacements.** Preview the mannequin clips and Fantasy Pack gear on the actual player rig. Inspect the installed Mixed Magic projectile examples and the Mega Magic arcane, energy and curse systems. Verify scale, facing, sockets, material references and collision in Unreal. Use `BP_FX_Hit`, `BP_FX_Wave`, `MM_Dash` and the existing short SFX as temporary controls when comparing new work.
- [ ] **Use stable names for exports.** Suggested names include `AN_FS_Barbarian_FuryLeap`, `SM_FS_Warrior_RoundShield`, `NS_FS_BloodThread_Recall`, `SFX_FS_Orb_FocusReady` and `T_FS_Icon_ArcaneBend`. Supply a brief note per asset with its source, license or provenance, target skeleton, sockets, timings, shared use and owning Unreal asset.
- [ ] **Bind visual events to gameplay.** Place clip markers for start, ready threshold, contact on/off, emission and recovery. Compare the marker to the event that spends resources, checks collision or applies damage. Account for play rate, action interruption and clips played during translation. Exercise left/right actions with handed equipment.
- [ ] **Inspect the whole effect lifetime.** Start, both hold thresholds where applicable, successful commit, cancel, miss, wall clipping, contact, cooldown, expiry, death, travel and reload. The practice dummy gives contact feedback with no heal, XP or rewards. Prioritize player silhouette and the Warden's floor warnings over spell decoration.
- [ ] **Review audio during real combat.** Listen through a sustained run with footsteps, enemy cues and repeated casts. Keep windup, actual damage or emission, obstruction and resource refusal distinguishable. Review the level of the repeated Spark and melee sounds.
- [ ] **Run one 45-second empty-room session per path.** V alternates Swing → Leap → Backstep. VV lands, braces and releases both pulse sizes. VS pivots both directions out of a swing. VM changes position between thread and recall. S alternates short/long guard steps. SV swaps guard for a planted stand. SS slips both directions into a follow-up. SM waves into open and blocked lanes. M alternates free Spark, Broad Orb and Focused Orb. MV enters and exits a seal and casts inside and outside. MS fires while moving and after planting. MM previews and fires straight and on both curves.
- [ ] **Verify the shipped experience.** Check initial choice, a Level 2 evolution, aim in a tight corner, floor hazards, live shield timing, optional chest, boss, reward, return and persistence after relaunch in a normal-input packaged expedition. Use keyboard/mouse and verify physical controller layouts when available. Record evidence in the class test report before marking the corresponding asset accepted.
