"""Scripted PIE acceptance on an isolated -UserDir; writes a JSON report without saving fixtures."""
import json
import traceback
from pathlib import Path
import unreal as u

root = Path(__file__).resolve().parent.parent
exec(compile((root/'Tools/BuildActionAssets.py').read_text(), str(root/'Tools/BuildActionAssets.py'), 'exec'))
report = root/'Saved/Actions150PIE.json'
editor = u.get_editor_subsystem(u.UnrealEditorSubsystem)
level = u.get_editor_subsystem(u.LevelEditorSubsystem)
level.load_level('/Game/Maps/L_Guild')
u.EditorPythonScripting.set_keep_python_script_alive(True)
level.editor_request_begin_play()
results = {}
cases = [
    ('warrior_attack','S',0,1,None), ('warrior_combo','S',0,1,None), ('warrior_roll','S',1,2,None),
    ('attack_rate_2','S',0,2,None), ('attack_rate_half','S',0,.5,None),
    ('short_replacement','S',0,1,'/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_Jump'),
    ('long_replacement','S',0,1,'/Game/Combat/SharedAnimations/AN_FS_EvadeRoll'),
    ('pause_attack','S',0,1,None), ('rate_change','S',0,1,None),
    ('interrupt_attack','S',0,1,None), ('interrupt_roll','S',1,2,None), ('interrupt_spark','M',0,1,None),
    ('attack_to_evade','S',0,1,None),
    ('barbarian_axe','V',0,1,None), ('barbarian_backstep','V',1,1,None),
    ('spark','M',0,1,None), ('orb_auto','M',2,1,None),
    ('shield_auto','S',2,1,None), ('stone_auto','V, V',3,1,None),
    ('thread','V, M',3,1,None), ('paid_stand','S, V',3,1,None),
    ('slip','S, S',3,1,None), ('wave','S, M',3,1,None),
    ('bend_auto','M, M',3,1,None), ('leap','V',2,1,None),
    ('weave_step','M, S',2,1,None), ('mesh_interrupt','S',0,1,None), ('death_roll','S',1,2,None),
]
phase='wait'
index=0
frames=0

def finish():
    report.write_text(json.dumps(results,indent=2))
    u.unregister_slate_post_tick_callback(callback)
    level.editor_request_end_play()
    u.EditorPythonScripting.set_keep_python_script_alive(False)
    u.SystemLibrary.quit_editor()

def read(p, name): return p.get_editor_property(name)

def cue(event):
    if phase=='measure': record['cues'].append(str(event))

def action_started(action):
    if phase=='measure': record['start_count']=record.get('start_count',1)+1

def tick(_dt):
    global phase,index,frames,p,driver,started,record,origin,actions,action_set,events,paused_at
    try:
        frames+=1
        if frames>18000: raise RuntimeError('PIE timeout: '+phase)
        world=editor.get_game_world()
        if not world: return
        p=u.GameplayStatics.get_player_character(world,0)
        if not p: return
        now=u.GameplayStatics.get_time_seconds(world)
        if now<.5:return
        if phase=='wait':
            driver=p.get_component_by_class(u.FSAnimatedAction)
            assert driver, 'No animated action component'
            original_set=driver.get_editor_property('action_set')
            action_set=u.FSActionSet()
            action_set.set_editor_property('actions',original_set.get_editor_property('actions'))
            driver.set_action_set(action_set)
            driver.on_action_event.add_callable(cue)
            driver.on_action_started.add_callable(action_started)
            actions={str(k):v for k,v in action_set.get_editor_property('actions').items()}
            origin=p.get_actor_location()
            phase='prepare'
        if phase=='prepare':
            if index>=len(cases): finish();return
            key,path,action,rate,replacement=cases[index]
            p.get_editor_property('mesh').set_animation_mode(u.AnimationMode.ANIMATION_BLUEPRINT)
            assert u.FSClassBridge.set_editor_essence_fixture(p,path,2,0,120)
            assert read(p,'GI'), 'Test fixture invalidated the player GameInstance reference'
            p.get_movement_component().set_movement_mode(u.MovementMode.MOVE_WALKING)
            p.get_movement_component().stop_movement_immediately()
            p.set_actor_location(origin,False,True)
            p.set_actor_rotation(u.Rotator(0,0,0),False)
            # Reset in-memory definitions between cases.
            action_set.set_editor_property('actions',actions)
            selected='Dodge' if action==1 and path.startswith('S') else 'Backstep' if action==1 else 'Sword' if path=='S' and action==0 else None
            if selected:
                changed=dict(actions)
                definition=changed[selected]
                montage=u.AssetToolsHelpers.get_asset_tools().duplicate_asset('Fixture_'+key,'/Game/Developers/FS150QA',definition.montage)
                assert montage, 'Could not duplicate test montage'
                if replacement:
                    assert u.FSActionSet.replace_animation(montage,u.load_asset(replacement)), 'Replacement failed'
                changed[selected]=u.FSActionAnimation(montage=montage,play_rate=rate)
                action_set.set_editor_property('actions',changed)
            started=now
            phase='ready'
            return
        key,path,action,rate,replacement=cases[index]
        if phase=='ready':
            if now-started<.15:return
            u.FSClassBridge.class_input(p,action,True)
            assert driver.is_action_active(), 'Input did not start '+key
            started=now
            montage=driver.get_action_montage()
            record=dict(name=str(driver.get_action_name()),length=montage.get_play_length(),rate=rate,
                        expected=montage.get_play_length()/rate, samples=[],actions=[str(driver.get_action_name())],
                        mana_before=read(p,'Mana'),stamina_before=read(p,'Stamina'),cues=[])
            paused_at=None
            events=set()
            phase='measure'
            return
        elapsed=now-started
        mesh=p.get_editor_property('mesh')
        anim=mesh.get_anim_instance()
        active=driver.is_action_active()
        name=str(driver.get_action_name())
        if active and name not in record['actions']:record['actions'].append(name)
        if driver.is_in_phase('Charged'): record['charged']=True
        if driver.is_in_phase('Hit'): record['hit_window']=True
        if driver.is_in_phase('Invulnerable'): record['invulnerable']=True
        if key=='warrior_roll' and active: assert read(p,'jump_max_count')==0, 'Roll allowed jumping'
        if key=='warrior_combo' and elapsed>.60 and 'combo' not in events:
            events.add('combo');u.FSClassBridge.class_input(p,0,True)
        if key=='pause_attack':
            if elapsed>.20 and 'pause' not in events:
                events.add('pause');anim.montage_pause(driver.get_action_montage());paused_at=now
            if paused_at and now-paused_at>.40 and 'resume' not in events:
                assert active and read(p,'bAttacking'), 'Paused action expired'
                events.add('resume');anim.montage_resume(driver.get_action_montage())
                record['expected']+=now-paused_at
        if key=='rate_change' and elapsed>.25 and 'rate' not in events:
            position=driver.get_position()
            driver.set_action_play_rate(2.0)
            record['expected']=elapsed+(record['length']-position)/2
            events.add('rate')
        if key in ('interrupt_attack','interrupt_roll','interrupt_spark') and elapsed>.2 and 'interrupt' not in events:
            events.add('interrupt')
            anim.montage_stop(0.0,driver.get_action_montage())
            record['interrupted_at']=elapsed
        if key=='mesh_interrupt' and elapsed>.2 and 'mesh' not in events:
            events.add('mesh');mesh.set_animation_mode(u.AnimationMode.ANIMATION_SINGLE_NODE)
        if key=='attack_to_evade' and elapsed>.15 and 'cancel' not in events:
            events.add('cancel')
            u.FSClassBridge.class_input(p,1,True)
            assert str(driver.get_action_name())=='Dodge','Attack did not cancel into roll'
            record['roll_started']=elapsed
        if key=='weave_step' and elapsed>.2 and 'step' not in events:
            events.add('step');u.FSClassBridge.class_input(p,3,True)
        if key=='death_roll' and elapsed>.2 and 'death' not in events:
            events.add('death');p.call_method('Die')
        speed=p.get_movement_component().get_editor_property('max_walk_speed')
        if read(p,'bAttacking') and speed>0 and 'movement_released_at' not in record:
            record['movement_released_at']=elapsed
        if key.startswith('warrior') and len(record['samples'])<500:
            record['samples'].append([round(elapsed,4),bool(active),bool(read(p,'bAttacking')),bool(read(p,'bDodging')),read(p,'jump_max_count'),round(speed)])
        active=driver.is_action_active()
        if active:
            if elapsed>12: raise RuntimeError('Stuck action '+key)
            return
        if key not in ('interrupt_attack','interrupt_roll','interrupt_spark','mesh_interrupt','death_roll') and elapsed<.10:return
        record.update(finished_at=elapsed,attacking=read(p,'bAttacking'),dodging=read(p,'bDodging'),jump_count=read(p,'jump_max_count'),distance=(p.get_actor_location()-origin).length2d(),mana_after=read(p,'Mana'))
        assert not read(p,'bAttacking') and not read(p,'bDodging'),'Stale action flags'
        assert read(p,'jump_max_count')==1,'Jump lock leaked'
        if key in ('interrupt_attack','interrupt_roll','interrupt_spark','mesh_interrupt','death_roll'):
            assert elapsed<.4,'Interruption did not unlock immediately'
        elif len(record['actions'])==1 and key not in ('attack_to_evade','rate_change','warrior_combo'):
            assert abs(elapsed-record['expected'])<.15, f"Duration mismatch {elapsed} != {record['expected']}"
        if key=='warrior_attack':
            assert record.get('movement_released_at',99)<record['finished_at']-.15,'Warrior movement locked through recovery'
        if key=='warrior_combo':
            assert record.get('start_count')==2 and elapsed<1.8,'Buffered combo waited for a separate recovery timeout'
        if key in ('spark','orb_auto','stone_auto','thread','wave','bend_auto','paid_stand'):
            assert record['cues'].count('Commit')==1,'Missing or duplicate animation commit cue'
        if key=='interrupt_spark': assert not record['cues'], 'Cancelled cast emitted a cue'
        assert read(p,'GI'), 'Test fixture cleared the player GameInstance'
        record['result']='PASS'
        results[key]=record
        index+=1
        phase='prepare'
    except Exception:
        results['error']=dict(case=cases[index][0] if index<len(cases) else 'finish',traceback=traceback.format_exc())
        finish()

callback=u.register_slate_post_tick_callback(tick)
