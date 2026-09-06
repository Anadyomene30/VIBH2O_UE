"""Builds the VibH2O demonstration material, into the plugin's own content.

Run from the repository root:

    "C:/Program Files/Epic Games/UE_5.5/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" ^
        VIBH2O_UE.uproject -run=pythonscript ^
        -script="Tools/make_demo_material.py" -unattended -nosplash

Like the demo map, this is a *generator*: the asset it writes is disposable and
regenerable, and this script documents exactly what the material contains.

The completion criterion for roadmap step 6 is that a test material reacts
visibly to EACH pushed parameter. Every parameter therefore has its own,
distinguishable effect:

    BeatPhase            expanding concentric rings (frac(phase - r * freq))
    BeatPulse            emissive flash on the beat
    Bpm                  ring frequency (a faster heart packs more rings)
    Excitation           overall emissive intensity
    Synchrony            rim glow (per-person share)
    CollectiveSynchrony  rim glow (room share)
    StriationSpeed       scrolling speed of fine radial stripes
    FocusMask            multiplies everything - out-of-focus bubbles dim
    Staleness            kills the pattern and desaturates to grey
    TintColor            the base colour itself
    BlendAlpha           hue shift toward green as the vortex takes over

The material is Opaque / DefaultLit, per the spec: a translucent material
would receive neither shadows nor caustics properly.
"""

import unreal

PACKAGE_PATH = '/VibH2O'
ASSET_NAME = 'M_VibH2ODemoBubble'

MEL = unreal.MaterialEditingLibrary

# Recreate from scratch so the script stays idempotent.
full_path = f'{PACKAGE_PATH}/{ASSET_NAME}'
if unreal.EditorAssetLibrary.does_asset_exist(full_path):
    unreal.EditorAssetLibrary.delete_asset(full_path)

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
material = asset_tools.create_asset(ASSET_NAME, PACKAGE_PATH, unreal.Material, unreal.MaterialFactoryNew())
assert material is not None, 'creation du materiau impossible - le contenu du plugin est-il monte ?'

# ---------------------------------------------------------------------------
# Helpers. Positions only matter for humans opening the graph later; a rough
# left-to-right layout keeps it readable.
# ---------------------------------------------------------------------------

_y = [0]

def node(cls, x, **props):
    _y[0] += 120
    expr = MEL.create_material_expression(material, cls, x, _y[0] % 1800)
    for key, value in props.items():
        expr.set_editor_property(key, value)
    return expr

def scalar(name, default, x=-2200):
    return node(unreal.MaterialExpressionScalarParameter, x,
                parameter_name=name, default_value=default)

def connect(src, out_name, dst, in_name):
    ok = MEL.connect_material_expressions(src, out_name, dst, in_name)
    assert ok, f'connexion {out_name} -> {in_name} refusee'

def to_property(src, prop):
    ok = MEL.connect_material_property(src, '', prop)
    assert ok, f'connexion vers {prop} refusee'

# ---------------------------------------------------------------------------
# Parameters - the whole contract.
# ---------------------------------------------------------------------------

beat_phase = scalar('BeatPhase', 0.0)
beat_pulse = scalar('BeatPulse', 0.0)
bpm = scalar('Bpm', 60.0)
excitation = scalar('Excitation', 0.0)
synchrony = scalar('Synchrony', 0.5)
collective = scalar('CollectiveSynchrony', 0.5)
striation_speed = scalar('StriationSpeed', 0.0)
focus_mask = scalar('FocusMask', 1.0)
staleness = scalar('Staleness', 0.0)
blend_alpha = scalar('BlendAlpha', 0.0)

tint = node(unreal.MaterialExpressionVectorParameter, -2200,
            parameter_name='TintColor',
            default_value=unreal.LinearColor(0.05, 0.35, 0.75, 1.0))

# ---------------------------------------------------------------------------
# Radial coordinate: distance from the UV centre.
# ---------------------------------------------------------------------------

uv = node(unreal.MaterialExpressionTextureCoordinate, -2000)
center = node(unreal.MaterialExpressionConstant2Vector, -2000, r=0.5, g=0.5)
radial = node(unreal.MaterialExpressionDistance, -1800)
connect(uv, '', radial, 'A')
connect(center, '', radial, 'B')

# ---------------------------------------------------------------------------
# Rings: frac(BeatPhase - r * ringfreq) ^ 6, ringfreq driven by Bpm.
# ---------------------------------------------------------------------------

ring_freq = node(unreal.MaterialExpressionMultiply, -1800, const_b=0.08)
connect(bpm, '', ring_freq, 'A')
ring_freq2 = node(unreal.MaterialExpressionAdd, -1600, const_b=4.0)
connect(ring_freq, '', ring_freq2, 'A')

r_scaled = node(unreal.MaterialExpressionMultiply, -1600)
connect(radial, '', r_scaled, 'A')
connect(ring_freq2, '', r_scaled, 'B')

ring_in = node(unreal.MaterialExpressionSubtract, -1400)
connect(beat_phase, '', ring_in, 'A')
connect(r_scaled, '', ring_in, 'B')

ring_frac = node(unreal.MaterialExpressionFrac, -1200)
connect(ring_in, '', ring_frac, '')

rings = node(unreal.MaterialExpressionPower, -1000, const_exponent=6.0)
connect(ring_frac, '', rings, 'Base')

# ---------------------------------------------------------------------------
# Striations: fine stripes over U, scrolled by Time * StriationSpeed.
# ---------------------------------------------------------------------------

u_only = node(unreal.MaterialExpressionComponentMask, -1800, r=True, g=False, b=False, a=False)
connect(uv, '', u_only, '')

u_freq = node(unreal.MaterialExpressionMultiply, -1600, const_b=30.0)
connect(u_only, '', u_freq, 'A')

time = node(unreal.MaterialExpressionTime, -1800)
scroll = node(unreal.MaterialExpressionMultiply, -1600)
connect(time, '', scroll, 'A')
connect(striation_speed, '', scroll, 'B')

stripe_phase = node(unreal.MaterialExpressionAdd, -1400)
connect(u_freq, '', stripe_phase, 'A')
connect(scroll, '', stripe_phase, 'B')

stripe_sine = node(unreal.MaterialExpressionSine, -1200)
connect(stripe_phase, '', stripe_sine, '')

stripe_pos = node(unreal.MaterialExpressionMultiply, -1000, const_b=0.5)
connect(stripe_sine, '', stripe_pos, 'A')
stripe_pos2 = node(unreal.MaterialExpressionAdd, -800, const_b=0.5)
connect(stripe_pos, '', stripe_pos2, 'A')

stripes = node(unreal.MaterialExpressionPower, -600, const_exponent=5.0)
connect(stripe_pos2, '', stripes, 'Base')

# ---------------------------------------------------------------------------
# Intensity: a base glow, boosted by the beat pulse and by arousal.
# ---------------------------------------------------------------------------

pulse_boost = node(unreal.MaterialExpressionMultiply, -1400, const_b=2.5)
connect(beat_pulse, '', pulse_boost, 'A')

exc_boost = node(unreal.MaterialExpressionMultiply, -1400, const_b=1.5)
connect(excitation, '', exc_boost, 'A')

boost_sum = node(unreal.MaterialExpressionAdd, -1200)
connect(pulse_boost, '', boost_sum, 'A')
connect(exc_boost, '', boost_sum, 'B')

intensity = node(unreal.MaterialExpressionAdd, -1000, const_b=0.4)
connect(boost_sum, '', intensity, 'A')

# ---------------------------------------------------------------------------
# Rim: fresnel scaled by the two synchronies.
# ---------------------------------------------------------------------------

fresnel = node(unreal.MaterialExpressionFresnel, -1400, exponent=3.0)

sync_a = node(unreal.MaterialExpressionMultiply, -1400, const_b=0.6)
connect(synchrony, '', sync_a, 'A')
sync_b = node(unreal.MaterialExpressionMultiply, -1400, const_b=0.6)
connect(collective, '', sync_b, 'A')
sync_sum = node(unreal.MaterialExpressionAdd, -1200)
connect(sync_a, '', sync_sum, 'A')
connect(sync_b, '', sync_sum, 'B')

rim = node(unreal.MaterialExpressionMultiply, -1000)
connect(fresnel, '', rim, 'A')
connect(sync_sum, '', rim, 'B')

# ---------------------------------------------------------------------------
# Pattern sum, silenced by staleness.
# ---------------------------------------------------------------------------

ring_lit = node(unreal.MaterialExpressionMultiply, -800)
connect(rings, '', ring_lit, 'A')
connect(intensity, '', ring_lit, 'B')

stripe_dim = node(unreal.MaterialExpressionMultiply, -800, const_b=0.35)
connect(stripes, '', stripe_dim, 'A')

pattern_a = node(unreal.MaterialExpressionAdd, -600)
connect(ring_lit, '', pattern_a, 'A')
connect(stripe_dim, '', pattern_a, 'B')

pattern = node(unreal.MaterialExpressionAdd, -400)
connect(pattern_a, '', pattern, 'A')
connect(rim, '', pattern, 'B')

alive = node(unreal.MaterialExpressionOneMinus, -800)
connect(staleness, '', alive, '')

pattern_live = node(unreal.MaterialExpressionMultiply, -200)
connect(pattern, '', pattern_live, 'A')
connect(alive, '', pattern_live, 'B')

# ---------------------------------------------------------------------------
# Colour: tint, shifted toward green by the vortex blend, greyed by staleness.
# ---------------------------------------------------------------------------

vortex_hue = node(unreal.MaterialExpressionConstant3Vector, -1400,
                  constant=unreal.LinearColor(0.15, 0.95, 0.55, 1.0))

blend_half = node(unreal.MaterialExpressionMultiply, -1400, const_b=0.6)
connect(blend_alpha, '', blend_half, 'A')

tint_blend = node(unreal.MaterialExpressionLinearInterpolate, -1200)
connect(tint, '', tint_blend, 'A')
connect(vortex_hue, '', tint_blend, 'B')
connect(blend_half, '', tint_blend, 'Alpha')

grey = node(unreal.MaterialExpressionConstant3Vector, -1200,
            constant=unreal.LinearColor(0.25, 0.25, 0.27, 1.0))

tint_final = node(unreal.MaterialExpressionLinearInterpolate, -1000)
connect(tint_blend, '', tint_final, 'A')
connect(grey, '', tint_final, 'B')
connect(staleness, '', tint_final, 'Alpha')

# ---------------------------------------------------------------------------
# Outputs, both gated by the focus mask.
# ---------------------------------------------------------------------------

emissive_raw = node(unreal.MaterialExpressionMultiply, 0)
connect(tint_final, '', emissive_raw, 'A')
connect(pattern_live, '', emissive_raw, 'B')

emissive = node(unreal.MaterialExpressionMultiply, 200)
connect(emissive_raw, '', emissive, 'A')
connect(focus_mask, '', emissive, 'B')

base_color = node(unreal.MaterialExpressionMultiply, 200)
connect(tint_final, '', base_color, 'A')
connect(focus_mask, '', base_color, 'B')

roughness = node(unreal.MaterialExpressionConstant, 200, r=0.35)

to_property(base_color, unreal.MaterialProperty.MP_BASE_COLOR)
to_property(emissive, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
to_property(roughness, unreal.MaterialProperty.MP_ROUGHNESS)

MEL.recompile_material(material)
saved = unreal.EditorAssetLibrary.save_asset(full_path)
assert saved, 'sauvegarde du materiau impossible'

unreal.log(f'VibH2O: materiau de demonstration ecrit dans {full_path}')
