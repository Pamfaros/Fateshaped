"""Create equipment definitions after authoring hand sockets with Unreal SkeletalMeshTools.

Source model geometry is preserved. Skeletal sockets own the attachment pivot and scale.
"""
import json
from pathlib import Path
import unreal as u

definitions={
    'axel_barbar':dict(path='/Game/Fantasy_Pack/Characters/Barbarian/Mesh/SM_Axel',
        grip=(0,0,35),palm=(-9,3.5,0),edge=((-26,0,98),(26,0,98))),
    'sword':dict(path='/Game/Fantasy_Pack/Characters/Warrior/Mesh/SM_Sword',
        grip=(0,0,16),palm=(-7.5,2.8,0),edge=((0,0,35),(0,0,146))),
    'round_shield':dict(path='/Game/Models/RoundShield/RoundShield',
        grip=(0,-6.6,0),palm=(7.52,-2.53,0),edge=((0,0,0),(0,0,0))),
}
folder='/Game/Combat/Equipment'
path=folder+'/DA_FS_ClassEquipment'
asset=u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else None
if not asset:
    factory=u.DataAssetFactory()
    factory.set_editor_property('data_asset_class',u.FSClassEquipment)
    asset=u.AssetToolsHelpers.get_asset_tools().create_asset('DA_FS_ClassEquipment',folder,u.FSClassEquipment,factory)
values={}
for name,data in definitions.items():
    values[name]=u.FSEquipmentVisual(mesh=u.load_asset(data['path']),attachment_socket=name,
        edge_base=u.Vector(*data['edge'][0]),edge_tip=u.Vector(*data['edge'][1]),
        grip=u.Vector(*data['grip']),palm=u.Vector(*data['palm']))
asset.set_editor_property('visuals',values)
assert u.EditorAssetLibrary.save_loaded_asset(asset,False)
out={}
for name in ['SKM_Quinn_Simple','SKM_Manny_Simple']:
    mesh=u.load_asset('/Game/Characters/Mannequins/Meshes/'+name)
    out[name]={}
    for socket_name in definitions:
        socket=mesh.find_socket(socket_name)
        assert socket, (name,socket_name)
        out[name][socket_name]=dict(parent=str(socket.bone_name),transform=str(socket.get_socket_local_transform()))
Path('C:/GameProjects/Fateshaped/Saved/Attachments151Assets.json').write_text(json.dumps(out,indent=2))
print('FS151: saved equipment definitions and confirmed attachment sockets')
