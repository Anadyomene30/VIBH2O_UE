"""Builds the VibH2O demonstration map.

Run from the repository root:

    "C:/Program Files/Epic Games/UE_5.5/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" ^
        VIBH2O_UE.uproject -run=pythonscript ^
        -script="Tools/make_demo_map.py" -unattended -nosplash

This is a *generator*, not a hand-made asset. The map it produces is disposable:
delete Content/Maps and run it again. That keeps the repository free of binary
assets that nobody can review in a diff, and it documents exactly what the test
scene contains.
"""

import unreal

MAP_PACKAGE = '/Game/Maps/VibH2O_Demo'

editor_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

editor_subsystem.new_level(MAP_PACKAGE)

# --- Lighting. Nothing elaborate: the artistic environment is underwater_bp,
# which lives outside this plugin. This is just enough to see the bubbles.
sun = actor_subsystem.spawn_actor_from_class(
    unreal.DirectionalLight, unreal.Vector(0, 0, 900), unreal.Rotator(-40, 30, 0))
sun.set_actor_label('Sun')
sun.light_component.set_editor_property('intensity', 3.0)

sky_light = actor_subsystem.spawn_actor_from_class(
    unreal.SkyLight, unreal.Vector(0, 0, 900))
sky_light.set_actor_label('SkyLight')

sky = actor_subsystem.spawn_actor_from_class(
    unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))
sky.set_actor_label('SkyAtmosphere')

# --- The installation itself.
#
# It sits deliberately off the origin and slightly rotated: the very first thing
# to verify is that moving and turning the stage actor carries the whole room
# with it. A room parked at the origin, axis-aligned, would hide a world-space
# computation instead of exposing it.
stage = actor_subsystem.spawn_actor_from_class(
    unreal.VibH2OStageActor,
    unreal.Vector(400.0, -250.0, 150.0),
    unreal.Rotator(0.0, 0.0, 15.0))
stage.set_actor_label('VibH2O_Stage')

stage.set_editor_property('preview_columns', 7)
stage.set_editor_property('preview_rows', 3)
stage.set_editor_property('preview_in_editor', True)
stage.set_editor_property('preview_show_orientation', True)

# A gentle rake and a touch of relief, so the demonstration room does not read
# as a flat spreadsheet.
stage.set_editor_property('elevation_per_row', 45.0)
stage.set_editor_property('depth_amplitude', 25.0)
stage.set_editor_property('curvature_angle', 4.0)

# One focus group, so the focus functions have something to chew on out of the
# box. "HautGauche" is the group named in the roadmap's step 8.
group = unreal.VibH2OFocusGroup()
group.set_editor_property('group_name', 'HautGauche')
group.set_editor_property('use_rect', True)
group.set_editor_property('column_min', 0)
group.set_editor_property('column_max', 2)
group.set_editor_property('row_min', 0)
group.set_editor_property('row_max', 1)
stage.set_editor_property('focus_groups', [group])

# --- A camera, placed at the framing distance the plugin itself computes.
# It is a modest but real check that GetFocusFitDistance returns something
# usable, rather than a number that merely looks plausible.
camera = actor_subsystem.spawn_actor_from_class(
    unreal.CameraActor, unreal.Vector(-1400.0, -250.0, 700.0), unreal.Rotator(-12.0, 0.0, 0.0))
camera.set_actor_label('VibH2O_Camera')

editor_subsystem.save_current_level()

unreal.log('VibH2O: carte de demonstration ecrite dans {}'.format(MAP_PACKAGE))
