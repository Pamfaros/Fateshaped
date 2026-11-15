"""Run in Unreal Editor Python after compiling Fateshaped. Creates missing action assets only.

Notify positions below migrate the old contact poses into editable animation data.
Existing assets are never overwritten; further tuning belongs in the Montage editor.
"""
import json
from pathlib import Path
import unreal as u

ROOT = '/Game/Combat/Actions'
ATTACK = '/Game/Characters/Mannequins/Anims/Unarmed/Attack/'
JUMP = '/Game/Characters/Mannequins/Anims/Unarmed/Jump/'
clips = {
    'axe': '/Game/Fantasy_Pack/Animations/1With_Weapon/Anim_Warrior_Attack_1',
    'sword': ATTACK+'MM_Attack_01', 'follow': ATTACK+'MM_Attack_02', 'cast': ATTACK+'MM_Attack_03',
    'dash': JUMP+'MM_Dash', 'roll': '/Game/Combat/SharedAnimations/AN_FS_EvadeRoll', 'jump': JUMP+'MM_Jump',
}
# Phases are normalized only for initial asset authoring, never interpreted as gameplay seconds.
definitions = {
    'Axe': ('axe', {'Hit':(.07,.14), 'MoveLock':(0,.14), 'Recovery':(.14,1), 'Combo':(.14,.82)}, {}),
    'Sword': ('sword', {'Hit':(.14/.52,.24/.52), 'MoveLock':(0,.24/.52), 'Recovery':(.24/.52,1), 'Combo':(.24/.52,1)}, {}),
    'Dodge': ('roll', {'Invulnerable':(0,1), 'MoveLock':(0,1), 'Travel':(0,1)}, {}),
    'Backstep': ('dash', {'Invulnerable':(0,.55/.9666667), 'MoveLock':(0,1), 'Travel':(0,1)}, {}),
    'Leap': ('follow', {'Hit':(.37/.9666667,.86/.9666667), 'MoveLock':(0,1), 'Travel':(0,1)}, {}),
    'ShieldHold': ('sword', {'Charged':(.30/.65,1), 'Guard':(0,1)}, {}),
    'ShieldStep': ('dash', {'Hit':(.06/.38,.26/.38), 'Travel':(0,.26/.38), 'MoveLock':(0,.26/.38)}, {}),
    'Spark': ('cast', {'MoveLock':(0,.10/.38), 'Combo':(.10/.38,1)}, {'Commit':.10/.38}),
    'OrbHold': ('cast', {'Charged':(.45/.80,1)}, {}),
    'Orb': ('cast', {}, {'Commit':.12/.28}),
    'StoneHold': ('axe', {'Charged':(.50/.80,1), 'Guard':(.12/.80,1), 'MoveLock':(0,1)}, {}),
    'Stone': ('axe', {'MoveLock':(0,.18/.55)}, {'Commit':.18/.55}),
    'Pivot': ('follow', {'Hit':(.10/.46,.30/.46), 'Travel':(0,.30/.46)}, {}),
    'Thread': ('cast', {}, {'Commit':.14/.32}),
    'Recall': ('cast', {}, {'Commit':.08/.20}),
    'PaidStand': ('follow', {'Guard':(0,.32/.80), 'Travel':(.32/.80,.52/.80), 'Hit':(.38/.80,.52/.80), 'MoveLock':(0,1)}, {'Commit':.32/.80}),
    'Slip': ('follow', {'Hit':(.10/.34,.20/.34), 'Travel':(0,.20/.34), 'Cancel':(.21/.34,1), 'Combo':(.25/.34,1)}, {}),
    'Wave': ('cast', {'Cancel':(.29/.55,1)}, {'Commit':.20/.55}),
    'Seal': ('cast', {}, {'Commit':.18/.40}),
    'BendHold': ('cast', {}, {}),
    'Bend': ('cast', {}, {'Commit':.14/.32}),
    'WeaveStep': ('dash', {'Travel':(0,1)}, {}),
}

tools = u.AssetToolsHelpers.get_asset_tools()
u.EditorAssetLibrary.make_directory(ROOT)
out = {}
actions = {}
rebuild = '-FSRebuildActions' in u.SystemLibrary.get_command_line()
for name, (clip, windows, cues) in definitions.items():
    path = f'{ROOT}/AM_FS_{name}'
    montage = u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else None
    created = not montage
    if created or rebuild:
        source = u.load_asset(clips[clip])
        assert source, clips[clip]
        if created:
            factory = u.AnimMontageFactory()
            factory.set_editor_property('source_animation', source)
            montage = tools.create_asset('AM_FS_'+name, ROOT, u.AnimMontage, factory)
        assert montage, path
        # Explicit rebuild owns these generated montage tracks; source-sequence notifies remain on the clips.
        u.AnimationLibrary.remove_all_animation_notify_tracks(montage)
        if name == 'Leap':
            # Preserve takeoff followed by the complete strike inside one animation-owned action.
            takeoff=u.load_asset(clips['jump'])
            assert u.FSActionSet.set_animation_segments(montage, [takeoff, source], [.20, 1.0]), [
                str(montage.get_editor_property('skeleton')), str(takeoff.get_editor_property('skeleton')),
                str(source.get_editor_property('skeleton'))]
        else:
            assert u.FSActionSet.replace_animation(montage, source)
        blend = montage.get_editor_property('blend_out')
        blend.set_editor_property('blend_time', 0.0)
        montage.set_editor_property('blend_out', blend)
        blend_in = montage.get_editor_property('blend_in')
        blend_in.set_editor_property('blend_time', .05)
        montage.set_editor_property('blend_in', blend_in)
        montage.set_editor_property('blend_out_trigger_time', -1.0)
        montage.set_editor_property('enable_auto_blend_out', True)
        length = montage.get_play_length()
        u.AnimationLibrary.add_animation_notify_track(montage, 'FS Action')
        for phase, (start, end) in windows.items():
            state = u.AnimationLibrary.add_animation_notify_state_event(montage, 'FS Action', start*length, (end-start)*length, u.FSActionWindow)
            state.set_editor_property('phase', phase)
        for event, fraction in cues.items():
            cue = u.AnimationLibrary.add_animation_notify_event(montage, 'FS Action', fraction*length, u.FSActionCue)
            cue.set_editor_property('event', event)
        u.EditorAssetLibrary.save_loaded_asset(montage)
    u.FSActionSet.link_phases_to_animation(montage)
    u.EditorAssetLibrary.save_loaded_asset(montage)
    # Warrior rolls were >2 s at native speed. Tempo remains asset data, and all windows follow playback.
    rate = 2.0 if name == 'Dodge' else 1.0
    actions[name] = u.FSActionAnimation(montage=montage, play_rate=rate)
    out[name] = dict(path=path, length=montage.get_play_length(), play_rate=rate)

set_path = ROOT+'/DA_FS_PlayerActions'
if not u.EditorAssetLibrary.does_asset_exist(set_path):
    factory = u.DataAssetFactory()
    factory.set_editor_property('data_asset_class', u.FSActionSet)
    action_set = tools.create_asset('DA_FS_PlayerActions', ROOT, u.FSActionSet, factory)
    action_set.set_editor_property('actions', actions)
    u.EditorAssetLibrary.save_loaded_asset(action_set)

Path(u.Paths.project_saved_dir(), 'FSAction150Assets.json').write_text(json.dumps(out, indent=2))
print('FS150: created/checked '+str(len(actions))+' action montages')
