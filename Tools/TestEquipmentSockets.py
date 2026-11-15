"""PIE attachment regression across class changes and animation playback; isolated -UserDir only."""
import json,traceback
from pathlib import Path
import unreal as u

root=Path(__file__).resolve().parent.parent
editor=u.get_editor_subsystem(u.UnrealEditorSubsystem)
level=u.get_editor_subsystem(u.LevelEditorSubsystem)
level.load_level('/Game/Maps/L_Guild')
u.EditorPythonScripting.set_keep_python_script_alive(True)
level.editor_request_begin_play()
paths=['V','S','S, V','M','S','V','S, M']
index=0
phase='ready'
frames=0
results=[]
data=u.load_asset('/Game/Combat/Equipment/DA_FS_ClassEquipment')
definitions={str(k):v for k,v in data.get_editor_property('visuals').items()}

def finish():
    (root/'Saved/Attachments151PIE.json').write_text(json.dumps(results,indent=2))
    u.unregister_slate_post_tick_callback(callback)
    level.editor_request_end_play()
    u.EditorPythonScripting.set_keep_python_script_alive(False)
    u.SystemLibrary.quit_editor()

def tick(_):
    global index,phase,frames,since,record
    try:
        frames+=1
        if frames>6000:raise RuntimeError('Attachment timeout')
        world=editor.get_game_world()
        p=u.GameplayStatics.get_player_character(world,0) if world else None
        if not p:return
        now=u.GameplayStatics.get_time_seconds(world)
        if now<.7:return
        if phase=='ready':
            if index>=len(paths):finish();return
            path=paths[index]
            assert u.FSClassBridge.set_editor_essence_fixture(p,path,2,0,120)
            record=dict(path=path,max_grip_error=0,samples=0,models=[])
            since=now;phase='settle'
        elif phase=='settle' and now-since>.2:
            if paths[index]!='M':u.FSClassBridge.class_input(p,0,True)
            since=now;phase='sample'
        elif phase=='sample':
            expected=['axel_barbar'] if paths[index].startswith('V') else [] if paths[index]=='M' else ['sword','round_shield']
            actual=[]
            for component in p.get_components_by_class(u.StaticMeshComponent):
                model=component.get_editor_property('static_mesh')
                if not model:continue
                key=next((key for key,definition in definitions.items() if definition.mesh==model),None)
                if not key:continue
                actual.append(key)
                definition=definitions[key]
                assert str(component.get_attach_socket_name())==str(definition.attachment_socket)
                assert component.get_editor_property('relative_location').length()<.001
                grip=u.MathLibrary.transform_location(component.get_world_transform(),definition.grip)
                parent=p.get_editor_property('mesh').get_socket_transform('hand_l' if key=='round_shield' else 'hand_r')
                expected_point=u.MathLibrary.transform_location(parent,definition.palm)
                error=(grip-expected_point).length()
                record['max_grip_error']=max(record['max_grip_error'],error)
                assert error<.05,(key,error)
            assert sorted(actual)==sorted(expected),(actual,expected)
            record['models']=actual;record['samples']+=1
            if now-since<2.7:return
            assert p.get_editor_property('jump_max_count')==1
            record['result']='PASS';results.append(record)
            index+=1;phase='ready'
    except Exception:
        results.append(dict(error=traceback.format_exc()));finish()

callback=u.register_slate_post_tick_callback(tick)
