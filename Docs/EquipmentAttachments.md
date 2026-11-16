# Equipment attachments

`UFSClassCombat` loads `/Game/Combat/Equipment/DA_FS_ClassEquipment` for class visuals and model-space damage trace endpoints. Equipment components attach with `SnapToTargetIncludingScale` and an identity relative transform. Class changes reattach both components, so no transform from the previous class survives.

The attachment pivots are **mesh sockets** on `SKM_Quinn_Simple` and `SKM_Manny_Simple`. Source model geometry and the shared skeleton are unchanged.

| Definition / socket | Model | Bone | Location (cm) | Rotation (Pitch, Yaw, Roll) | Uniform scale |
| --- | --- | --- | --- | --- | --- |
| `axel_barbar` | `/Game/Fantasy_Pack/Characters/Barbarian/Mesh/SM_Axel` | `hand_r` | (-9, 3.5, -29.75) | (0, 180, 0) | 0.85 |
| `sword` | `/Game/Fantasy_Pack/Characters/Warrior/Mesh/SM_Sword` | `hand_r` | (-7.5, 2.8, -12.48) | (0, 90, 0) | 0.78 |
| `round_shield` | `/Game/Models/RoundShield/RoundShield` | `hand_l` | (7.52, 3.41, 0) | (0, 0, 0) | 0.9 |

For a replacement model, edit the definition's Mesh and contact endpoints, then adjust the matching mesh socket in the Skeletal Mesh Editor. `Grip` is the model-space handle center; `Palm` is the intended hand-bone-space contact point, used for QA. Socket translation is `Palm - Rotation(Scale * Grip)`. The sword trace runs along the blade; the double axe trace runs across its head. Notify windows still determine when traces deal damage.

`Tools/BuildEquipmentSockets.py` creates the Data Asset and validates that the authored sockets exist. It does not rewrite source models or create sockets. `Tools/PreviewEquipmentSockets.py` renders temporary poses in an unsaved blank map. `Tools/TestEquipmentSockets.py` checks actual PIE component transforms during attacks and seven essence/class transitions. Run QA scripts against an isolated editor project/profile; the gameplay fixture modifies the active profile.

FS-151 evidence: main and isolated Editor builds succeeded; seven attachment cases and four Dungeon hit/miss cases passed. Screenshots and JSON reports are under `Saved/FS151Preview`, `Saved/Attachments151PIE.json` and `Saved/Attachments151Combat.json`. Normal device input and the updated Windows package remain NOT_TESTED. Restart the existing main editor to load `UnrealEditor-Fateshaped-0151.dll`.
