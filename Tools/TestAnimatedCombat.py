import json, traceback
from pathlib import Path
import unreal as ue

if '-FSRebuildActions' in ue.SystemLibrary.get_command_line():
    build=Path(__file__).resolve().parent/'BuildActionAssets.py'
    exec(compile(build.read_text(),str(build),'exec'),{})

root=Path(__file__).resolve().parent.parent/'Saved'
profile='Actions150Combat'
editor=ue.get_editor_subsystem(ue.UnrealEditorSubsystem)
level=ue.get_editor_subsystem(ue.LevelEditorSubsystem)
level.load_level('/Game/Maps/L_Guild')
ue.EditorPythonScripting.set_keep_python_script_alive(True)
level.editor_request_begin_play()
bridge=ue.FSClassBridge
out={}
phase='guild'
frames=0
index=0
cases=[
 ('V','axe',(100,0),[(0,0,True)],34,0,0), ('V','axe_miss',(140,155),[(0,0,True)],0,0,0),
 ('S','sword',(119,0),[(0,0,True)],22,0,0), ('S','sword_miss',(120,380),[(0,0,True)],0,0,0),
 ('M','spark',(250,0),[(0,0,True)],12,0,0), ('M','spark_miss',(250,155),[(0,0,True)],0,0,0),
 ('M','spark_wall',(250,0),[(0,0,True)],0,0,0),
 ('S, M','wave',(320,0),[(0,3,True)],40,0,0),
 ('S, M','wave_cancel',(320,0),[(0,3,True),(.08,1,True)],0,None,0),
 ('M','orb_broad',(250,0),[(0,2,True),(.18,2,False)],24,0,0),
 ('M','orb_focused',(250,0),[(0,2,True),(1.05,2,False)],32,0,0),
 ('V','leap_close',(145,0),[(0,2,True)],40.8,None,5.4),
 ('V','leap_far',(300,0),[(0,2,True)],40.8,None,5.4),
]
if '-FSInspectAxe' in ue.SystemLibrary.get_command_line(): cases=cases[:2]
if '-FSInspectLeap' in ue.SystemLibrary.get_command_line(): cases=[c for c in cases if c[1].startswith('leap')]
mana_cost={'thread_recall':18,'wave':30,'wave_wall':30,'orb_broad':20,'orb_focused':20,'seal':18,'weave':20,'weave_wall':20,'bend_straight':22}
stamina_cost={'pivot_valid':24,'shield_short':20,'shield_long':20,'shield_auto':20,'slip':16,'wave_cancel':25,'weave':22}

def finish():
    (root/(profile+'.json')).write_text(json.dumps(out,indent=2),encoding='utf-8')
    ue.unregister_slate_post_tick_callback(callback)
    level.editor_request_end_play()
    ue.EditorPythonScripting.set_keep_python_script_alive(False)
    ue.SystemLibrary.quit_editor()

def find(world,tag):
    return ue.GameplayStatics.get_all_actors_with_tag(world,tag)[0]

def point(actor):
    v=actor.get_actor_location()
    return ue.Vector(v.x,v.y,92)

def teleport(actor,dest):
    if isinstance(actor,ue.Character):
        actor.get_movement_component().stop_movement_immediately()
    actor.set_actor_location(dest,False,True)
    actor.set_actor_rotation(ue.Rotator(0,0,0),False)

def read(p,name):
    return p.get_editor_property(name)

def clear_arc(world,p,start,enemies):
    previous=start
    for index in range(1,25):
        alpha=index/24
        point=start+ue.Vector(320*alpha,0,4*55*alpha*(1-alpha))
        hit=ue.SystemLibrary.capsule_trace_single_by_profile(world,previous,point,42,90,
            'Pawn',False,[p,*enemies],ue.DrawDebugTrace.NONE,True)
        if hit is not None:
            return str(hit)
        floor=ue.SystemLibrary.line_trace_single(world,point,point-ue.Vector(0,0,220),
            ue.TraceTypeQuery.TRACE_TYPE_QUERY1,False,[p,*enemies],ue.DrawDebugTrace.NONE,True)
        if floor is None:return 'NoFloor',str(point)
        previous=point
    return None

def tick(_dt):
    global phase,frames,origin,target,targets,wall,index,started,base,steps,last,record
    try:
        frames+=1
        if frames>9500: raise RuntimeError('Timeout during '+phase)
        world=editor.get_game_world()
        p=ue.GameplayStatics.get_player_pawn(world,0) if world else None
        if not p: return
        now=ue.GameplayStatics.get_time_seconds(world)
        if now<0.45: return
        name=ue.GameplayStatics.get_current_level_name(world,True)
        ctl=ue.GameplayStatics.get_player_controller(world,0)
        if phase=='guild':
            ue.SystemLibrary.execute_console_command(world,'log LogTemp Verbose',ctl)
            assert bridge.set_editor_essence_fixture(p,'V',1,0,0)
            teleport(p,point(find(world,'Fairy')))
            p.call_method('DoInteract')
            p.call_method('DoInteract')
            teleport(p,point(find(world,'QuestPortal')))
            phase='dungeon'
        elif phase=='dungeon' and name=='L_Dungeon_01':
            origin=p.get_actor_location()
            target=find(world,'Enc2')
            targets=list(ue.GameplayStatics.get_all_actors_of_class(world,ue.FSCryptEnemy))
            globals()['targets']=sorted(targets,key=lambda actor:-read(actor,'Health'))
            obstruction=clear_arc(world,p,origin,targets)
            if obstruction:
                original=origin
                out['original_fixture_obstruction']=obstruction
                for dx,dy in [(0,350),(0,-350),(350,0),(-350,0),(350,350),(-350,-350),(600,0),(-600,0)]:
                    candidate=original+ue.Vector(dx,dy,0)
                    if clear_arc(world,p,candidate,targets) is None:
                        origin=candidate
                        out['fixture_offset']=[dx,dy]
                        break
                else:raise RuntimeError('No unobstructed leap fixture in entrance room')
            for foe in ue.GameplayStatics.get_all_actors_of_class(world,ue.FSCryptEnemy):
                brain=foe.get_controller().get_component_by_class(ue.BrainComponent)
                brain.stop_logic('Class acceptance')
                foe.set_actor_tick_enabled(False)
                foe.get_movement_component().disable_movement()
                if foe!=target: teleport(foe,origin+ue.Vector(-1200,0,0))
            wall=find(world,'ExitGate')
            wall.get_component_by_class(ue.StaticMeshComponent).set_static_mesh(ue.load_asset('/Engine/BasicShapes/Cube'))
            wall.set_actor_scale3d(ue.Vector(.18,2.4,3.6))
            phase='prepare'
        elif phase=='prepare':
            if index>=len(cases): finish(); return
            path,key,offset,schedule,damage,travel,vitality=cases[index]
            assert bridge.set_editor_essence_fixture(p,path,2,0,120)
            ctl.set_control_rotation(ue.Rotator(0,0,0))
            teleport(p,origin)
            wall.get_component_by_class(ue.StaticMeshComponent).set_collision_enabled(ue.CollisionEnabled.NO_COLLISION)
            teleport(wall,origin+ue.Vector(0,900,0))
            if key in ('leap_wall','wave_wall','weave_wall','spark_wall'):
                wall.set_actor_rotation(ue.Rotator(0,90 if key=='weave_wall' else 0,0),False)
                dest=origin+(ue.Vector(0,55,0) if key=='weave_wall' else ue.Vector(132,0,0))
                wall.set_actor_location(dest,False,True)
                wall.get_component_by_class(ue.StaticMeshComponent).set_collision_enabled(ue.CollisionEnabled.QUERY_AND_PHYSICS)
            teleport(target,origin+ue.Vector(-1200,0,0))
            target=max(targets,key=lambda actor:read(actor,'Health'))
            teleport(target,origin+ue.Vector((offset or (0,630))[0],(offset or (0,630))[1],0))
            before={'leap_close':80,'leap_far':80,'leap_15v':15,'leap_16v':16,'paid_12v':12,'paid_13v':13}.get(key)
            if before is not None:
                ue.GameplayStatics.apply_damage(p,read(p,'Health')-before,None,None,None)
            started=now
            steps=list(schedule)
            phase='ready'
        elif phase=='ready' and now-started>=0.15:
            path,key,offset,schedule,damage,travel,vitality=cases[index]
            vals=[read(p,k) for k in ('Health','Stamina','Mana')]
            record={'path':path,'class':read(p,'ClassName'),'before':vals,'minimum':list(vals),'expected':{'damage':damage,'travel':travel,'vitality_delta':vitality,'mana_cost':mana_cost.get(key,0),'stamina_cost':stamina_cost.get(key,0)},'inputs':schedule}
            if key.startswith('leap'):
                record['fixture_sweep']=str(ue.SystemLibrary.capsule_trace_single(world,origin,origin+ue.Vector(320,0,0),42,88,
                    ue.TraceTypeQuery.TRACE_TYPE_QUERY1,False,[p,*targets],ue.DrawDebugTrace.NONE,True))
                record['origin']=str(origin)
                capsule=p.get_component_by_class(ue.CapsuleComponent)
                record['capsule']=[read(capsule,'capsule_half_height'),str(p.get_actor_scale3d())]
            base=read(target,'Health')
            last=max(t for t,_,_ in steps)
            started=now
            phase='measure'
        elif phase=='measure':
            path,key,offset,schedule,damage,travel,vitality=cases[index]
            elapsed=now-started
            while steps and elapsed>=steps[0][0]:
                _,action,pressed=steps.pop(0)
                bridge.class_input(p,action,pressed)
            vals=[read(p,k) for k in ('Health','Stamina','Mana')]
            record['minimum']=[min(a,b) for a,b in zip(record['minimum'],vals)]
            # Keep the geometry fixture stationary outside authored action travel.
            anim=p.get_editor_property('mesh').get_anim_instance()
            montage=anim.get_current_active_montage()
            if montage:
                rate=anim.montage_get_play_rate(montage)
                record['max_montage_rate']=max(record.get('max_montage_rate',0),rate)
                record['montage_length']=montage.get_play_length()
                if key.startswith('leap'):
                    delta=p.get_actor_location()-origin
                    record.setdefault('positions',[]).append([anim.montage_get_position(montage),[delta.x,delta.y,delta.z]])
                    if .14<anim.montage_get_position(montage)<.25:
                        at=p.get_actor_location()
                        probe=ue.SystemLibrary.line_trace_single_for_objects(world,at+ue.Vector(10,0,3),at+ue.Vector(10,0,-217),
                            [ue.ObjectTypeQuery.OBJECT_TYPE_QUERY1,ue.ObjectTypeQuery.OBJECT_TYPE_QUERY2],False,[p],ue.DrawDebugTrace.NONE,True)
                        if probe: record.setdefault('floor_probes',[]).append(str(probe))
                if key.startswith('axe'):
                    weapon=next(c for c in p.get_components_by_class(ue.StaticMeshComponent) if c.get_editor_property('static_mesh') and c.get_editor_property('static_mesh').get_name()=='SM_Viking_Axe')
                    transform=weapon.get_world_transform()
                    base_point=ue.MathLibrary.transform_location(transform,ue.Vector(0,-14,67))-origin
                    tip_point=ue.MathLibrary.transform_location(transform,ue.Vector(0,14,67))-origin
                    record.setdefault('weapon_samples',[]).append([anim.montage_get_position(montage),
                        [base_point.x,base_point.y,base_point.z],[tip_point.x,tip_point.y,tip_point.z]])
            if read(p,'bAttacking'):
                record['locked_distance']=max(record.get('locked_distance',0),(p.get_actor_location()-origin).length2d())
            elif elapsed>.12:
                record['released_speed']=p.get_movement_component().get_editor_property('max_walk_speed')
            if elapsed<last+3.05:return
            record.update(after=vals,damage=round(base-read(target,'Health'),2),travel=round((p.get_actor_location()-origin).length2d(),2), message=read(p,'Message'))
            rec=record
            rec['result']='PASS' if abs(rec['damage']-damage)<=.1 and (travel is None or abs(rec.get('locked_distance',999)-travel)<=0.25) and abs(vals[0]-rec['before'][0]-vitality)<=.15 and abs(rec['before'][1]-rec['minimum'][1]-stamina_cost.get(key,0))<=.3 and abs(rec['before'][2]-rec['minimum'][2]-mana_cost.get(key,0))<=.4 else 'FAIL'
            if key=='leap_wall' and '20.4' in rec['message']: rec['result']='FAIL'
            if rec.get('max_montage_rate',0)<=0: rec['result']='FAIL'
            if rec.get('released_speed',0)<=0:rec['result']='FAIL'
            out[key]=rec
            index+=1
            phase='prepare'
    except Exception:
        out['error']=traceback.format_exc()
        finish()

callback=ue.register_slate_post_tick_callback(tick)
