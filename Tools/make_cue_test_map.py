"""Builds the cue test map: a Level Sequence and a cue director, nothing else.

Run with ABSOLUTE paths, <repo> being the repository root - a relative
-script resolves against the engine binaries folder and silently loads nothing:

    "C:/Program Files/Epic Games/UE_5.5/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" ^
        "<repo>/VIBH2O_UE.uproject" -run=pythonscript ^
        -script="<repo>/Tools/make_cue_test_map.py" -unattended -nosplash

Then play it as a game, Resolume in front, and drive it with
Tools/vibh2o_cue_check.py - see docs/CUES.md, "Verifier sans Resolume".

The cues it holds are the ones docs/CUES.md uses as examples:

    MIDI note 60 (any channel)          -> sequence from the start
    MIDI note 61                        -> stop
    OSC /composition/columns/1/connect  -> jump to marker "Acte2" and play
    OSC /composition/columns/2/connect  -> pause
    /Cue/Vortex (by name)               -> BlendAlpha 1
    Ctrl+Maj+F12 (keyboard cues on)     -> play

Generated and disposable, like the demo map: delete it and run this again.
"""

import unreal

SEQ_PACKAGE = '/Game/Sequences'
SEQ_NAME = 'LS_VibH2O_CueTest'
MAP_PACKAGE = '/Game/Maps/VibH2O_CueTest'

FPS = 30
LENGTH_SECONDS = 30
MARKER_SECONDS = 10

editor_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

# --- The map, recreated from scratch. Step through an unsaved blank map first:
# the target may be the level currently loaded, which cannot be deleted.
if unreal.EditorAssetLibrary.does_asset_exist(MAP_PACKAGE):
    unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    assert unreal.EditorAssetLibrary.delete_asset(MAP_PACKAGE), 'suppression de l ancienne carte impossible'

# --- The sequence: empty, thirty seconds, one marker. What is tested is the
# transport - play, pause, jump - not what the sequence animates.
full_path = f'{SEQ_PACKAGE}/{SEQ_NAME}'
if unreal.EditorAssetLibrary.does_asset_exist(full_path):
    unreal.EditorAssetLibrary.delete_asset(full_path)

sequence = asset_tools.create_asset(SEQ_NAME, SEQ_PACKAGE, unreal.LevelSequence, unreal.LevelSequenceFactoryNew())
assert sequence is not None, 'creation de la sequence impossible'
sequence.set_display_rate(unreal.FrameRate(FPS, 1))
sequence.set_playback_start(0)
sequence.set_playback_end(LENGTH_SECONDS * FPS)

# The frame is given in display rate; the time-unit overload says so (the
# overload without it is deprecated, and reads tick resolution).
marker = unreal.MovieSceneMarkedFrame()
marker.set_editor_property('frame_number', unreal.FrameNumber(MARKER_SECONDS * FPS))
marker.set_editor_property('label', 'Acte2')
sequence.add_marked_frame_to_sequence(marker, unreal.MovieSceneTimeUnit.DISPLAY_RATE)
assert unreal.EditorAssetLibrary.save_asset(full_path), 'sauvegarde de la sequence impossible'

assert editor_subsystem.new_level(MAP_PACKAGE), 'creation du niveau impossible'

player = actor_subsystem.spawn_actor_from_class(unreal.LevelSequenceActor, unreal.Vector(0, 0, 0))
player.set_actor_label('VibH2O_CueTestSequence')
player.set_sequence(sequence)

director = actor_subsystem.spawn_actor_from_class(unreal.VibH2OCueDirector, unreal.Vector(0, 0, 0))
director.set_actor_label('VibH2O_CueDirector')


def cue(name, source, action, **fields):
    """One cue; the remaining fields are trigger or action settings."""
    c = unreal.VibH2OCue()
    c.set_editor_property('name', name)
    c.set_editor_property('source', source)
    c.set_editor_property('action', action)
    for key, value in fields.items():
        c.set_editor_property(key, value)
    return c


def make_key(name):
    k = unreal.Key()
    k.set_editor_property('key_name', name)
    return k


Source = unreal.VibH2OCueSource
Action = unreal.VibH2OCueAction

director.set_editor_property('cues', [
    cue('Note60', Source.MIDI_NOTE, Action.PLAY_SEQUENCE_FROM_START, midi_number=60, sequence=player),
    cue('Note61', Source.MIDI_NOTE, Action.STOP_SEQUENCE, midi_number=61, sequence=player),
    cue('Colonne1', Source.OSC, Action.JUMP_TO_MARKER,
        osc_address='/composition/columns/1/connect', marker_name='Acte2', play_after_jump=True, sequence=player),
    cue('Colonne2', Source.OSC, Action.PAUSE_SEQUENCE, osc_address='/composition/columns/2/connect', sequence=player),
    cue('Vortex', Source.OSC, Action.SET_BLEND_ALPHA, out_value=1.0),
    # Only answers when "Lire le clavier en arriere-plan" is on.
    cue('ToucheF12', Source.KEY, Action.PLAY_SEQUENCE, key=make_key('F12'),
        require_ctrl=True, require_shift=True, sequence=player),
])
# Every input in the log: this map exists to be watched.
director.set_editor_property('log_inputs', True)

assert editor_subsystem.save_current_level(), 'sauvegarde de la carte impossible'
unreal.log(f'VibH2O: carte de test des cues ecrite dans {MAP_PACKAGE}')
