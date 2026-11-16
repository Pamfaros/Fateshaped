"""Render temporary editor poses to review equipment grips; does not save the level."""
import json,traceback
from pathlib import Path
import unreal as u

root=Path(__file__).resolve().parent.parent
folder=root/'Saved/FS151Preview'
folder.mkdir(exist_ok=True)
level=u.get_editor_subsystem(u.LevelEditorSubsystem)
u.EditorLoadingAndSavingUtils.new_blank_map(False)
editor=u.get_editor_subsystem(u.UnrealEditorSubsystem)
actors=u.get_editor_subsystem(u.EditorActorSubsystem)
equipment=u.load_asset('/Game/Combat/Equipment/DA_FS_ClassEquipment')
definitions={str(k):v for k,v in equipment.get_editor_property('visuals').items()}
created=[]
hero=actors.spawn_actor_from_class(u.SkeletalMeshActor,u.Vector(0,0,5),u.Rotator(0,-90,0),transient=True)
created.append(hero)
body=hero.get_component_by_class(u.SkeletalMeshComponent)
body.set_skeletal_mesh_asset(u.load_asset('/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple'))
body.set_update_animation_in_editor(True)
body.set_collision_enabled(u.CollisionEnabled.NO_COLLISION)
held={}
for name,definition in definitions.items():
    actor=actors.spawn_actor_from_class(u.StaticMeshActor,u.Vector(),transient=True)
    created.append(actor)
    component=actor.get_component_by_class(u.StaticMeshComponent)
    component.set_static_mesh(definition.mesh)
    component.set_collision_enabled(u.CollisionEnabled.NO_COLLISION)
    component.set_mobility(u.ComponentMobility.MOVABLE)
    component.attach_to_component(body,definition.attachment_socket,u.AttachmentRule.SNAP_TO_TARGET,
        u.AttachmentRule.SNAP_TO_TARGET,u.AttachmentRule.SNAP_TO_TARGET,False)
    component.set_relative_transform(u.Transform(),False,True)
    held[name]=component

for location in [u.Vector(150,-170,220),u.Vector(30,180,170)]:
    light=actors.spawn_actor_from_class(u.PointLight,location,transient=True)
    created.append(light)
    component=light.get_component_by_class(u.PointLightComponent)
    component.set_intensity(300)
    component.set_attenuation_radius(600)

cases=[('SwordFull','sword',.40,'full'),('SwordGrip','sword',.40,'grip'),
       ('ShieldBack','sword',.40,'shield'),('AxeFull','axel_barbar',.10,'full'),('AxeGrip','axel_barbar',.10,'grip')]
index=0
phase='setup'
frames=0
report={}
u.EditorPythonScripting.set_keep_python_script_alive(True)

def done():
    (folder/'Report.json').write_text(json.dumps(report,indent=2))
    u.unregister_slate_post_tick_callback(callback)
    for actor in created:actors.destroy_actor(actor)
    u.EditorPythonScripting.set_keep_python_script_alive(False)
    u.SystemLibrary.quit_editor()

def tick(_):
    global index,phase,frames,task
    try:
        frames+=1
        if frames>1800:raise RuntimeError('Preview timeout')
        if index>=len(cases):done();return
        name,weapon,fraction,view=cases[index]
        if phase=='setup':
            for key,component in held.items():component.set_visibility(key==weapon or (weapon=='sword' and key=='round_shield'))
            path='/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01' if weapon=='sword' else '/Game/Fantasy_Pack/Animations/1With_Weapon/Anim_Warrior_Attack_1'
            clip=u.load_asset(path)
            body.override_animation_data(clip,False,True,clip.get_play_length()*fraction,0)
            phase='pose';frames=0
        elif phase=='pose' and frames>18:
            if view=='full':
                focus=u.Vector(45,0,110)
                camera=focus+u.Vector(240,-270,100)
            else:
                definition=definitions['round_shield' if view=='shield' else weapon]
                bone='hand_l' if view=='shield' else 'hand_r'
                transform=body.get_socket_transform(bone)
                focus=u.MathLibrary.transform_location(transform,definition.palm)
                camera=u.MathLibrary.transform_location(transform,definition.palm+u.Vector(6,-65 if view=='shield' else 55,25))
            editor.set_level_viewport_camera_info(camera,u.MathLibrary.find_look_at_rotation(camera,focus))
            phase='capture';frames=0
        elif phase=='capture' and frames>10:
            filename=folder/(name+'.png')
            task=u.AutomationLibrary.take_high_res_screenshot(1024,768,str(filename))
            report[name]=dict(file=str(filename),grip_errors={})
            for key in [weapon]+(['round_shield'] if weapon=='sword' else []):
                definition=definitions[key]
                actual=u.MathLibrary.transform_location(held[key].get_world_transform(),definition.grip)
                expected=u.MathLibrary.transform_location(body.get_socket_transform('hand_l' if key=='round_shield' else 'hand_r'),definition.palm)
                report[name]['grip_errors'][key]=(actual-expected).length()
            phase='wait';frames=0
        elif phase=='wait' and frames>14:
            if not (folder/(name+'.png')).exists():return
            index+=1;phase='setup';frames=0
    except Exception:
        report['error']=traceback.format_exc();done()

callback=u.register_slate_post_tick_callback(tick)
