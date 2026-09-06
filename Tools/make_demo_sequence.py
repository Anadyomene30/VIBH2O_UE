"""Builds the VibH2O demonstration Level Sequence, and drops a player into the map.

Run from the repository root:

    "C:/Program Files/Epic Games/UE_5.5/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" ^
        VIBH2O_UE.uproject -run=pythonscript ^
        -script="Tools/make_demo_sequence.py" -unattended -nosplash

The sequence animates the three parameters the roadmap names for the Sequencer
check - BlendAlpha, CurvatureAngle, FocusAmount - over ten seconds at 30 fps.
That they can be added as float tracks AT ALL is already half the check: the
binding only resolves because the properties carry the Interp specifier.

A LevelSequenceActor is placed in the demo map, auto-play OFF: the DemoSweep
launches it at the right moment, otherwise the two would fight over the same
properties.
"""

import unreal

SEQ_PACKAGE = '/Game/Sequences'
SEQ_NAME = 'LS_VibH2O_Recette'
MAP_PACKAGE = '/Game/Maps/VibH2O_Demo'

FPS = 30
LENGTH_FRAMES = 300  # 10 s

editor_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

# The sequence binds an actor of the map, so the map must be the current level.
editor_subsystem.load_level(MAP_PACKAGE)

stage = None
for actor in actor_subsystem.get_all_level_actors():
    if isinstance(actor, unreal.VibH2OStageActor):
        stage = actor
        break
assert stage is not None, 'aucun VibH2OStageActor dans la carte de demonstration'

# Recreate the asset from scratch: the script stays idempotent.
full_path = f'{SEQ_PACKAGE}/{SEQ_NAME}'
if unreal.EditorAssetLibrary.does_asset_exist(full_path):
    unreal.EditorAssetLibrary.delete_asset(full_path)

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
sequence = asset_tools.create_asset(SEQ_NAME, SEQ_PACKAGE, unreal.LevelSequence, unreal.LevelSequenceFactoryNew())
assert sequence is not None, 'creation de la sequence impossible'

sequence.set_display_rate(unreal.FrameRate(FPS, 1))
sequence.set_playback_start(0)
sequence.set_playback_end(LENGTH_FRAMES)

binding = sequence.add_possessable(stage)

def add_float_track(property_name, keys):
    """One float track on the stage actor, with (frame, value) keys."""
    track = binding.add_track(unreal.MovieSceneFloatTrack)
    track.set_property_name_and_path(property_name, property_name)
    section = track.add_section()
    section.set_range(0, LENGTH_FRAMES)
    channel = section.get_all_channels()[0]
    for frame, value in keys:
        channel.add_key(unreal.FrameNumber(frame), value)
    return track

# The morph goes out and comes back; the curvature sweeps negative to positive
# through exactly straight; the focus pulses once in the middle.
add_float_track('BlendAlpha', [(0, 0.0), (150, 1.0), (300, 0.0)])
add_float_track('CurvatureAngle', [(0, -10.0), (150, 0.0), (300, 10.0)])
add_float_track('FocusAmount', [(0, 0.0), (120, 1.0), (180, 1.0), (300, 0.0)])

saved = unreal.EditorAssetLibrary.save_asset(full_path)
assert saved, 'sauvegarde de la sequence impossible'

# The player actor, auto-play off - DemoSweep triggers it.
for actor in actor_subsystem.get_all_level_actors():
    if isinstance(actor, unreal.LevelSequenceActor):
        actor_subsystem.destroy_actor(actor)

player = actor_subsystem.spawn_actor_from_class(
    unreal.LevelSequenceActor, unreal.Vector(0, 0, 200))
player.set_actor_label('VibH2O_SequencePlayer')
player.set_sequence(sequence)

saved_level = editor_subsystem.save_current_level()
assert saved_level, 'sauvegarde de la carte impossible - fichier verrouille ?'

unreal.log(f'VibH2O: sequence ecrite dans {full_path}, acteur de lecture place dans la carte')
