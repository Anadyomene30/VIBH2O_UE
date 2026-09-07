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

# new_level over an EXISTING package leaves save_current_level failing: the
# regenerate path must delete the old asset first, like the material and
# sequence generators do. The target may also be the editor startup map, i.e.
# the level currently loaded - step through a scratch level so it can be
# deleted at all.
if unreal.EditorAssetLibrary.does_asset_exist(MAP_PACKAGE):
    editor_subsystem.new_level('/Game/Maps/VibH2O_Scratch')
    deleted = unreal.EditorAssetLibrary.delete_asset(MAP_PACKAGE)
    assert deleted, 'suppression de l ancienne carte impossible - fichier verrouille ?'

created = editor_subsystem.new_level(MAP_PACKAGE)
assert created, 'creation du niveau impossible'

# --- Lighting. Nothing elaborate: the artistic environment is underwater_bp,
# which lives outside this plugin. This is just enough to see the bubbles - and
# to not see a black void, which is what you get when a SkyAtmosphere has no
# sun declared.
sun = actor_subsystem.spawn_actor_from_class(
    unreal.DirectionalLight, unreal.Vector(0, 0, 900), unreal.Rotator(-42, 25, 0))
sun.set_actor_label('Sun')
# Deliberately dim: the bubbles are emissive, so a darker world makes them
# read, and deep water is not a bright sky.
sun.light_component.set_editor_property('intensity', 0.8)
# Without this the SkyAtmosphere has nothing to scatter and renders pure black.
sun.light_component.set_editor_property('atmosphere_sun_light', True)
# Shafts need the light to take part in the volume, and to cast the shadows
# that carve them.
sun.light_component.set_editor_property('volumetric_scattering_intensity', 3.5)
sun.light_component.set_editor_property('cast_volumetric_shadow', True)
# Light shafts: the god rays that read as sunlight reaching down through water.
sun.light_component.set_editor_property('enable_light_shaft_bloom', True)
sun.light_component.set_editor_property('enable_light_shaft_occlusion', True)
sun.light_component.set_editor_property('light_color', unreal.Color(196, 226, 235, 255))

sky_light = actor_subsystem.spawn_actor_from_class(
    unreal.SkyLight, unreal.Vector(0, 0, 900))
sky_light.set_actor_label('SkyLight')
sky_light.light_component.set_editor_property('real_time_capture', True)
sky_light.light_component.set_editor_property('intensity', 0.5)

sky = actor_subsystem.spawn_actor_from_class(
    unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))
sky.set_actor_label('SkyAtmosphere')

# Fog turns the empty void into water: it gives depth cues, separates the near
# rows from the far ones, and reads as the underwater world the piece lives in.
fog = actor_subsystem.spawn_actor_from_class(
    unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0))
fog.set_actor_label('Fog')
fog_component = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
if fog_component is not None:
    fog_component.set_editor_property('fog_density', 0.025)
    fog_component.set_editor_property('fog_height_falloff', 0.05)
    fog_component.set_editor_property('fog_inscattering_luminance', unreal.LinearColor(0.008, 0.045, 0.075, 1.0))
    # Volumetric fog is what makes the sun read as shafts through water rather
    # than a flat tint, and it is what the motes catch the light against.
    fog_component.set_editor_property('enable_volumetric_fog', True)
    fog_component.set_editor_property('volumetric_fog_extinction_scale', 2.5)
    fog_component.set_editor_property('volumetric_fog_albedo', unreal.Color(120, 190, 210, 255))
    fog_component.set_editor_property('volumetric_fog_scattering_distribution', 0.55)

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

# --- The built-in simulator: press Play, the room lives, nothing to wire up.
#
# It auto-starts, and steps aside on its own the moment real OSC traffic
# arrives from Max - so this same map serves both the zero-wiring demo and the
# real rehearsal.
simulator = actor_subsystem.spawn_actor_from_class(
    unreal.VibH2OSimulatorActor, unreal.Vector(0.0, 0.0, 100.0))
simulator.set_actor_label('VibH2O_Simulateur')
simulator.set_editor_property('columns', 7)
simulator.set_editor_property('rows', 3)
# A travelling wave rather than the calm room: it sweeps arousal across the
# whole range, so the colour ramp is visible the moment Play is pressed.
simulator.set_editor_property('scenario', unreal.VibH2OSimulatorScenario.WAVE)

# --- Where to stand.
#
# Both viewpoints aim at the room, and both matter: without a PlayerStart the
# default pawn spawns at the world origin, which looks away from a room parked
# at (400, -250, 150) - the room is then off-frame in a black void, and the map
# appears empty. That was a real bug in an earlier version of this file.
# --- Motes in the water: the parallax that says "volume", not "backdrop".
motes = actor_subsystem.spawn_actor_from_class(
    unreal.VibH2OWaterMotes, unreal.Vector(400.0, -250.0, 400.0))
motes.set_actor_label('VibH2O_Particules')
motes.set_editor_property('count', 320)
motes.set_editor_property('radius', 2400.0)
motes.set_editor_property('height', 2000.0)

# Assign the mote material here rather than trusting the actor constructor:
# ConstructorHelpers resolves once, when the class default object is built, so
# an actor placed before the material existed keeps a null and renders with the
# default lit material - which, in water this dark, reads as black specks.
assert unreal.EditorAssetLibrary.does_asset_exist('/VibH2O/M_VibH2OMote'),     'M_VibH2OMote introuvable - lancer make_demo_material.py avant'

# --- Post process: the grade, the bloom the emissive bubbles need, and the
# volumetric fog that turns the directional light into shafts through water.
ppv = actor_subsystem.spawn_actor_from_class(
    unreal.PostProcessVolume, unreal.Vector(0.0, 0.0, 0.0))
ppv.set_actor_label('PostProcess')
ppv.set_editor_property('unbound', True)

settings = ppv.get_editor_property('settings')
settings.set_editor_property('override_bloom_intensity', True)
settings.set_editor_property('bloom_intensity', 1.4)
settings.set_editor_property('override_auto_exposure_method', True)
settings.set_editor_property('auto_exposure_method', unreal.AutoExposureMethod.AEM_MANUAL)
settings.set_editor_property('override_auto_exposure_bias', True)
settings.set_editor_property('auto_exposure_bias', 11.0)
# A cool, slightly desaturated grade: water eats warm light with distance, and
# the arousal ramp reads better against a cold ground.
settings.set_editor_property('override_color_saturation', True)
settings.set_editor_property('color_saturation', unreal.Vector4(0.92, 0.98, 1.06, 1.0))
settings.set_editor_property('override_color_contrast', True)
settings.set_editor_property('color_contrast', unreal.Vector4(1.06, 1.04, 1.02, 1.0))
settings.set_editor_property('override_vignette_intensity', True)
settings.set_editor_property('vignette_intensity', 0.45)
ppv.set_editor_property('settings', settings)

ROOM_CENTER = unreal.Vector(400.0, -250.0, 240.0)
# 820 cm out along a three-quarter view: close enough that a bubble is a
# bubble and not a dot, far enough that the whole room fits.
VIEWPOINT = unreal.Vector(-219.0, -539.0, 693.0)
LOOK_AT = unreal.MathLibrary.find_look_at_rotation(VIEWPOINT, ROOM_CENTER)

camera = actor_subsystem.spawn_actor_from_class(unreal.CameraActor, VIEWPOINT, LOOK_AT)
camera.set_actor_label('VibH2O_Camera')
# Play uses this camera straight away, instead of a default pawn at the origin.
camera.set_editor_property('auto_activate_for_player', unreal.AutoReceiveInput.PLAYER0)

# A PlayerStart as well, so a map opened with a different GameMode - or with the
# camera deleted - still starts somewhere the room is visible.
player_start = actor_subsystem.spawn_actor_from_class(unreal.PlayerStart, VIEWPOINT, LOOK_AT)
player_start.set_actor_label('PlayerStart')

# The editor viewport is saved with the level: framing it here means the room is
# on screen the moment the map is opened, with no Play and no navigating.
unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(
    VIEWPOINT, LOOK_AT)

# Assert on the save: a locked file (an editor still shutting down, an
# antivirus scan) fails the save WITHOUT failing the script otherwise, and a
# generator that lies about having written its asset is worse than one that
# stops.
# new_level writes its file at once: the scratch stepping-stone must not
# survive the run.
if unreal.EditorAssetLibrary.does_asset_exist('/Game/Maps/VibH2O_Scratch'):
    unreal.EditorAssetLibrary.delete_asset('/Game/Maps/VibH2O_Scratch')

saved = editor_subsystem.save_current_level()
assert saved, 'sauvegarde de la carte impossible - le fichier est-il verrouille par un autre processus ?'

unreal.log('VibH2O: carte de demonstration ecrite dans {}'.format(MAP_PACKAGE))
