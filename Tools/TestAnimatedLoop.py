"""Scripted Guild/quest/reward/reload regression. Uses a dedicated -UserDir."""
import json, traceback
from pathlib import Path
import unreal as u

root=Path(__file__).resolve().parent.parent
editor=u.get_editor_subsystem(u.UnrealEditorSubsystem)
level=u.get_editor_subsystem(u.LevelEditorSubsystem)
level.load_level('/Game/Maps/L_Guild')
u.EditorPythonScripting.set_keep_python_script_alive(True)
level.editor_request_begin_play()
phase='guild'
frames=0
out={}

def finish():
    (root/'Saved/Actions150Loop.json').write_text(json.dumps(out,indent=2))
    u.unregister_slate_post_tick_callback(callback)
    level.editor_request_end_play()
    u.EditorPythonScripting.set_keep_python_script_alive(False)
    u.SystemLibrary.quit_editor()

def tagged(world,tag): return u.GameplayStatics.get_all_actors_with_tag(world,tag)[0]
def position(p,point):
    p.get_movement_component().stop_movement_immediately()
    p.set_actor_location(u.Vector(point.x,point.y,110),False,True)

def tick(_):
    global phase,frames,since,before_xp,reward_xp,reward_items
    try:
        frames+=1
        if frames>5000:raise RuntimeError('Timeout '+phase)
        world=editor.get_game_world()
        if phase=='restart' and not world:
            level.editor_request_begin_play();phase='reload';return
        p=u.GameplayStatics.get_player_character(world,0) if world else None
        if not p:return
        now=u.GameplayStatics.get_time_seconds(world)
        if now<.65:return
        name=u.GameplayStatics.get_current_level_name(world,True)
        gi=p.get_editor_property('GI')
        save=gi.get_editor_property('Save')
        if phase=='guild':
            assert u.FSClassBridge.set_editor_essence_fixture(p,'S',2,0,120)
            position(p,tagged(world,'Fairy').get_actor_location())
            p.call_method('DoInteract');p.call_method('DoInteract')
            assert gi.get_editor_property('QuestAccepted')
            position(p,tagged(world,'QuestPortal').get_actor_location())
            phase='dungeon'
        elif phase=='dungeon' and name=='L_Dungeon_01':
            enemies=u.GameplayStatics.get_all_actors_of_class(world,u.FSCryptEnemy)
            if not enemies:return
            out['enemies']=len(enemies)
            for enemy in enemies:
                enemy.get_controller().get_component_by_class(u.BrainComponent).stop_logic('Action regression')
            u.FSClassBridge.class_input(p,0,True)
            assert p.get_editor_property('bAttacking')
            since=now;phase='evade'
        elif phase=='evade' and now-since>.15:
            u.FSClassBridge.class_input(p,1,True)
            assert p.get_editor_property('bDodging') and p.get_editor_property('jump_max_count')==0
            phase='clear';since=now
        elif phase=='clear' and now-since>1.2:
            assert not p.get_editor_property('bAttacking') and not p.get_editor_property('bDodging')
            assert p.get_editor_property('jump_max_count')==1
            for enemy in u.GameplayStatics.get_all_actors_of_class(world,u.FSCryptEnemy):
                u.GameplayStatics.apply_damage(enemy,10000,p.get_controller(),p,None)
            since=now;phase='exit'
        elif phase=='exit' and now-since>.4:
            before_xp=save.get_editor_property('XP')
            layout=u.GameplayStatics.get_all_actors_of_class(world,u.FSDungeonLayout)[0]
            exit_point=next(c.get_world_location() for c in layout.get_components_by_class(u.TextRenderComponent) if c.get_name()=='ExitLabel')
            position(p,exit_point)
            since=now;phase='reward'
        elif phase=='reward' and p.get_editor_property('bWon'):
            reward_xp=save.get_editor_property('XP')
            reward_items=save.get_editor_property('Inventory')
            assert reward_xp==before_xp+120,(reward_xp,before_xp)
            assert 'Crypt Shard' in reward_items
            p.call_method('CompleteQuest');p.call_method('CompleteQuest')
            assert save.get_editor_property('XP')==reward_xp
            assert save.get_editor_property('Inventory')==reward_items
            out['reward_xp']=reward_xp;out['reward_items']=reward_items
            p.call_method('DoRestart');phase='returned'
        elif phase=='reward' and now-since>3:raise RuntimeError('Exit did not complete quest')
        elif phase=='returned' and name=='L_Guild':
            assert not gi.get_editor_property('QuestAccepted')
            assert save.get_editor_property('XP')==reward_xp
            out['guild_return']=True
            phase='restart';level.editor_request_end_play()
        elif phase=='reload':
            assert name=='L_Guild'
            assert save.get_editor_property('XP')==reward_xp
            assert save.get_editor_property('Inventory')==reward_items
            out['new_game_instance_reload']=True;out['result']='PASS';finish()
    except Exception:
        out['error']=traceback.format_exc();finish()

callback=u.register_slate_post_tick_callback(tick)
