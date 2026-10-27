"""Builds the VibH2O mood presets, into the plugin's own content.

    "C:/Program Files/Epic Games/UE_5.5/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" ^
        "<repo>/VIBH2O_UE.uproject" -run=pythonscript ^
        -script="<repo>/Tools/make_mood_presets.py" -unattended -nosplash

Two presets, as starting points - drop one on the stage actor (Details panel,
VibH2O > Ambiance > "Preset d'ambiance") and it loads at once:

  DA_VibH2O_Ambiance_FroidChaud   cold to warm: deep blues, the water's aqua,
                                  amber to coral. The stage's own default.
  DA_VibH2O_Ambiance_Aqua         the main scene's palette: blue-greens
                                  throughout, the mood read through light,
                                  saturation and energy rather than hue.

Tune on the stage, then "Enregistrer dans le preset" writes the tuning back
into the asset. These two are regenerable; a preset of your own, saved under
another name, is never touched by this script.
"""

import unreal

PACKAGE_PATH = '/VibH2O'


def fresh_preset(name):
    path = f'{PACKAGE_PATH}/{name}'
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property('data_asset_class', unreal.VibH2OMoodPreset)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    asset = tools.create_asset(name, PACKAGE_PATH, unreal.VibH2OMoodPreset, factory)
    assert asset is not None, f'creation de {name} impossible'
    return asset, path


def colour(r, g, b):
    return unreal.LinearColor(r, g, b, 1.0)


def save(asset, path):
    assert unreal.EditorAssetLibrary.save_asset(path), f'sauvegarde de {path} impossible'
    unreal.log(f'VibH2O: preset d\'ambiance ecrit dans {path}')


# --- Cold to warm: exactly the C++ defaults, which a fresh asset already holds.
warm, warm_path = fresh_preset('DA_VibH2O_Ambiance_FroidChaud')
save(warm, warm_path)

# --- Aqua: the main scene's water, everywhere. The mood moves in light and
# saturation - pale and dim when calm, dense and bright when excited.
aqua, aqua_path = fresh_preset('DA_VibH2O_Ambiance_Aqua')
palette = aqua.get_editor_property('palette')

calm = palette.get_editor_property('calm')
calm.set_editor_property('color_low', colour(0.010, 0.060, 0.120))
calm.set_editor_property('color_high', colour(0.030, 0.160, 0.260))
palette.set_editor_property('calm', calm)

normal = palette.get_editor_property('normal')
normal.set_editor_property('color_low', colour(0.015, 0.260, 0.360))
normal.set_editor_property('color_high', colour(0.060, 0.520, 0.560))
palette.set_editor_property('normal', normal)

excited = palette.get_editor_property('excited')
excited.set_editor_property('color_low', colour(0.020, 0.700, 0.700))
excited.set_editor_property('color_high', colour(0.450, 1.000, 0.900))
excited.set_editor_property('glow', 2.3)
palette.set_editor_property('excited', excited)

aqua.set_editor_property('palette', palette)
save(aqua, aqua_path)
