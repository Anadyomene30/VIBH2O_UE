"""Builds the VibH2O demonstration materials, into the plugin's own content.

    "C:/Program Files/Epic Games/UE_5.5/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" ^
        "<repo>/VIBH2O_UE.uproject" -run=pythonscript ^
        -script="<repo>/Tools/make_demo_material.py" -unattended -nosplash

Four materials, all disposable and regenerable:

  M_VibH2ODemoBubble   the bubble
  M_VibH2OMote         the specks suspended in the water
  M_VibH2OCaustics     light function: the rippling net of light under water
  M_VibH2OSeabed       the floor that catches the caustics and the shadows

The bubble follows the main scene's (VIBH2o_PPCM, Bubble_Mat): bands of light
running across a sphere you can partly see through. Three things make it
better than that original:

  * it is LIT. A subsurface material: the rig's lights and the caustics land
    on it, back light glows through it in its own hue - where the original
    glowed the same white disc in any light and burned out under exposure.
  * it answers to the ROOM. The stage sends the mood's colour (TintColor) and
    energy (MoodGlow, the speed of StripePhase, MoodTurbulence); synchrony
    lines the bands up from bubble to bubble (Coherence) and lights a halo,
    without touching the colour; a ring of light runs pole to pole on each
    heartbeat (BeatPhase).
  * nothing is uniform. Each bubble has a stable seed of its own (BubbleSeed)
    that bends its bands differently and starts them at a different phase.

The material is Masked / Subsurface, per the spec: a translucent material
would receive neither shadows nor caustics properly. The see-through is a
temporal dither on the mask, the same trick as the original, thinning the
shell between the bands; the face turned to the viewer is also darker - deep
water seen through the bubble - and the grazing edge brighter. Transparency at
zero gives a solid bubble.

Colour is not computed here any more: the three-mood palettes live on the
stage (and in its presets), where they can be tuned in the Details panel.
"""

import unreal

PACKAGE_PATH = '/VibH2O'
MEL = unreal.MaterialEditingLibrary


def fresh_material(name):
    path = f'{PACKAGE_PATH}/{name}'
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mat = tools.create_asset(name, PACKAGE_PATH, unreal.Material, unreal.MaterialFactoryNew())
    assert mat is not None, f'creation de {name} impossible'
    return mat, path


class Graph:
    """Thin helper over MaterialEditingLibrary, with a readable layout."""

    def __init__(self, material):
        self.m = material
        self.y = 0

    def n(self, cls, x, **props):
        self.y += 110
        e = MEL.create_material_expression(self.m, cls, x, self.y % 2400)
        for k, v in props.items():
            # A few expression properties are read-only from Python (Noise's
            # Function, for one). Refusing one must not lose the whole graph -
            # but it must be said, not swallowed, or the material silently
            # differs from what this file describes.
            try:
                e.set_editor_property(k, v)
            except Exception as exc:
                unreal.log_warning(
                    f'VibH2O: {cls.__name__}.{k} non applicable, valeur par defaut conservee ({exc})')
        return e

    def scalar(self, name, default, x=-2600):
        return self.n(unreal.MaterialExpressionScalarParameter, x,
                      parameter_name=name, default_value=default)

    def const(self, value, x=-2600):
        return self.n(unreal.MaterialExpressionConstant, x, r=value)

    def rgb(self, r, g, b, x=-2600):
        return self.n(unreal.MaterialExpressionConstant3Vector, x,
                      constant=unreal.LinearColor(r, g, b, 1.0))

    def link(self, src, out, dst, inp):
        # Input names in the Python API do not always match what the graph
        # shows: some single-input nodes expose theirs as the empty string.
        # Try the named input, then the unnamed one, and only give up after
        # both - a silent unconnected input would ship a wrong material.
        for candidate in (inp, '') if inp else ('',):
            if MEL.connect_material_expressions(src, out, dst, candidate):
                return
        raise AssertionError(f'connexion refusee: {out!r} -> {inp!r}')

    def out(self, src, prop):
        ok = MEL.connect_material_property(src, '', prop)
        assert ok, f'connexion refusee vers {prop}'

    def mul(self, a, b, x=0):
        e = self.n(unreal.MaterialExpressionMultiply, x)
        self.link(a, '', e, 'A')
        self.link(b, '', e, 'B')
        return e

    def mulc(self, a, k, x=0):
        e = self.n(unreal.MaterialExpressionMultiply, x, const_b=k)
        self.link(a, '', e, 'A')
        return e

    def add(self, a, b, x=0):
        e = self.n(unreal.MaterialExpressionAdd, x)
        self.link(a, '', e, 'A')
        self.link(b, '', e, 'B')
        return e

    def addc(self, a, k, x=0):
        e = self.n(unreal.MaterialExpressionAdd, x, const_b=k)
        self.link(a, '', e, 'A')
        return e

    def sub(self, a, b, x=0):
        e = self.n(unreal.MaterialExpressionSubtract, x)
        self.link(a, '', e, 'A')
        self.link(b, '', e, 'B')
        return e

    def lerp(self, a, b, alpha, x=0):
        e = self.n(unreal.MaterialExpressionLinearInterpolate, x)
        self.link(a, '', e, 'A')
        self.link(b, '', e, 'B')
        self.link(alpha, '', e, 'Alpha')
        return e

    def sine(self, a, x=0):
        e = self.n(unreal.MaterialExpressionSine, x)
        self.link(a, '', e, '')
        return e

    def frac(self, a, x=0):
        e = self.n(unreal.MaterialExpressionFrac, x)
        self.link(a, '', e, '')
        return e

    def power(self, a, k, x=0):
        e = self.n(unreal.MaterialExpressionPower, x, const_exponent=k)
        self.link(a, 'Base', e, 'Base') if False else self.link(a, '', e, 'Base')
        return e

    def saturate(self, a, x=0):
        e = self.n(unreal.MaterialExpressionClamp, x, min_default=0.0, max_default=1.0)
        self.link(a, '', e, '')
        return e

    def one_minus(self, a, x=0):
        e = self.n(unreal.MaterialExpressionOneMinus, x)
        self.link(a, '', e, '')
        return e


# ===========================================================================
# The bubble
# ===========================================================================
#
# After the main scene's bubbles (VIBH2o_PPCM: Sphere_BP / Bubble_Mat): bands
# of light running across a sphere you can partly see through. What changes is
# that they are LIT - subsurface, so the rig's lights and the caustics land on
# them instead of every bubble glowing the same white disc - and that every
# band, colour and speed now answers to the room.
#
# Three readings, kept apart on purpose:
#
#   * the MOOD (calm, normal, excited) is the colour the stage sends in
#     TintColor and the energy in MoodGlow / StripePhase speed / MoodTurbulence;
#   * SYNCHRONY is coherence, never colour: in step, the bands leave each
#     bubble's own frame for the room's (world height, shared phase) so they
#     line up from one bubble to the next like a single wave crossing the
#     room, and a halo lights the rims; out of step, every bubble runs its own;
#   * the HEART is a ring of light sliding pole to pole once per beat, on
#     BeatPhase, so a change of BPM never makes it jump.

bubble, bubble_path = fresh_material('M_VibH2ODemoBubble')
# Subsurface, MASKED rather than translucent: drawn in the opaque pass, so it
# receives shadows and caustics like any solid - the reason the spec rules
# translucency out. The see-through comes from a temporal dither on the mask,
# as in the main scene's Bubble_Mat; Transparency at zero makes it solid.
bubble.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_SUBSURFACE)
bubble.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
# Two-sided: through the front, the bands on the far side show, which is what
# gives a shell its volume.
bubble.set_editor_property('two_sided', True)
g = Graph(bubble)

# --- the contract, and the mood
beat_phase = g.scalar('BeatPhase', 0.0)
beat_pulse = g.scalar('BeatPulse', 0.0)
g.scalar('Bpm', 60.0)
excitation = g.scalar('Excitation', 0.0)
g.scalar('Synchrony', 0.5)
g.scalar('CollectiveSynchrony', 0.5)
striation_speed = g.scalar('StriationSpeed', 0.0)
focus_mask = g.scalar('FocusMask', 1.0)
staleness = g.scalar('Staleness', 0.0)
g.scalar('BlendAlpha', 0.0)
tint = g.n(unreal.MaterialExpressionVectorParameter, -2600,
           parameter_name='TintColor',
           default_value=unreal.LinearColor(0.015, 0.26, 0.36, 1.0))

g.scalar('MoodCalm', 0.0)
g.scalar('MoodNormal', 1.0)
g.scalar('MoodExcited', 0.0)
g.scalar('RoomExcitation', 0.0)
glow_mood = g.scalar('MoodGlow', 1.0)
turbulence = g.scalar('MoodTurbulence', 1.0)
stripe_phase = g.scalar('StripePhase', 0.0)
room_phase = g.scalar('RoomStripePhase', 0.0)
coherence = g.scalar('Coherence', 0.0)
sync_halo = g.scalar('SyncHalo', 1.0)
brightness = g.scalar('BubbleBrightness', 1.0)
seed = g.scalar('BubbleSeed', 0.0)

# --- the artist's dials: the look itself, not data
stripe_count = g.scalar('StripeCount', 11.0)
stripe_sharpness = g.scalar('StripeSharpness', 3.0)
world_spacing = g.scalar('StripeWorldSpacing', 14.0)
transparency = g.scalar('Transparency', 0.6)
# Emissive levels, in scene light. They look tiny, and must: the demo map
# runs a manual exposure of +10 EV (the main scene +10.5), and at such a
# setting a value of 1 burns the whole room into one glare - which the first
# version of this bubble did. An EyeAdaptationInverse node was tried to make
# them exposure-proof and changed nothing measurable under manual exposure,
# so the levels are set here for those scenes, and BubbleBrightness on the
# stage scales them for any other.
stripe_light = g.scalar('StripeLight', 0.05)
rim_light = g.scalar('RimLight', 0.008)
body_light = g.scalar('BodyLight', 0.0006)
heart_light = g.scalar('HeartLight', 0.02)

time = g.n(unreal.MaterialExpressionTime, -2600)
world_pos = g.n(unreal.MaterialExpressionWorldPosition, -2600)
uv = g.n(unreal.MaterialExpressionTextureCoordinate, -2600)
v = g.n(unreal.MaterialExpressionComponentMask, -2400, r=False, g=True, b=False, a=False)
g.link(uv, '', v, '')

# ------------------------------------------------------------- the warp
#
# The bands are never ruled lines: a slow noise bends them, more so as the
# mood stirs. The seed shifts each bubble's noise so no two bend alike, and a
# second field drifts with time so the bending crawls along the bands.
seed_shift = g.mulc(seed, 517.0, -2400)
pos_seeded = g.add(world_pos, seed_shift, -2300)
noise_a = g.n(unreal.MaterialExpressionNoise, -2200,
              scale=0.035, quality=1, turbulence=True, levels=2,
              output_min=-1.0, output_max=1.0)
g.link(pos_seeded, '', noise_a, 'Position')

drift = g.mulc(time, 9.0, -2400)
drift_xy = g.n(unreal.MaterialExpressionAppendVector, -2300)
g.link(drift, '', drift_xy, 'A')
g.link(drift, '', drift_xy, 'B')
drift_xyz = g.n(unreal.MaterialExpressionAppendVector, -2200)
g.link(drift_xy, '', drift_xyz, 'A')
g.link(drift, '', drift_xyz, 'B')
noise_b = g.n(unreal.MaterialExpressionNoise, -2000,
              scale=0.11, quality=1, turbulence=True, levels=2,
              output_min=-1.0, output_max=1.0)
g.link(g.add(pos_seeded, drift_xyz, -2100), '', noise_b, 'Position')

warp = g.mul(g.add(g.mulc(noise_a, 0.30, -1900), g.mulc(noise_b, 0.12, -1900), -1850), turbulence, -1800)


def power_by(base, exponent, x):
    """Power with the exponent from a node, not a constant.

    No fallback to the unnamed input here: on a Power node the unnamed input
    is the BASE, so a refused exponent pin silently overwrote the base with
    the exponent - every band read 3 squared, and the bubbles came out as
    uniform white discs. Named pins only, or fail loudly.
    """
    e = g.n(unreal.MaterialExpressionPower, x)
    g.link(base, '', e, 'Base')
    for pin in ('Exp', 'Exponent'):
        if MEL.connect_material_expressions(exponent, '', e, pin):
            return e
    raise AssertionError('connexion refusee: exposant de Power')


def bands(arg, x):
    """0.5 + 0.5 sin(2 pi arg), sharpened: bright thin bands, dark gaps.

    Unreal's Sine node already has a Period of 1 - it computes sin(2 pi x) -
    so the argument goes in as it is. Multiplied by 2 pi once more, the first
    version packed some four hundred bands on a bubble, which averaged out to
    an even glow with no band in sight.
    """
    wave = g.sine(arg, x - 150)
    return power_by(g.addc(g.mulc(wave, 0.5, x - 100), 0.5, x - 50), stripe_sharpness, x)

# Own frame: latitude bands on the bubble, at its own phase. The phase arrives
# accumulated from C++ (never time * speed), so a change of mood changes how
# fast the bands run and never where they are.
local_arg = g.sub(g.add(g.mul(v, stripe_count, -1700), warp, -1650), stripe_phase, -1600)
local_bands = bands(local_arg, -1400)

# The room's frame: bands set by world height, at the phase the whole room
# shares. Every bubble cuts the same slices of the same wave.
world_z = g.n(unreal.MaterialExpressionComponentMask, -2400, r=False, g=False, b=True, a=False)
g.link(world_pos, '', world_z, '')
z_div = g.n(unreal.MaterialExpressionDivide, -1700)
g.link(world_z, '', z_div, 'A')
g.link(world_spacing, '', z_div, 'B')
world_arg = g.sub(g.add(z_div, g.mulc(warp, 0.5, -1650), -1620), room_phase, -1600)
world_bands = bands(world_arg, -1400)

stripes = g.lerp(local_bands, world_bands, coherence, -1200)

# ------------------------------------------------------------- the heart
#
# One ring of light from pole to pole per beat: BeatPhase is the ring's
# latitude. The pulse envelope makes it flare as it leaves.
heart_dist = g.n(unreal.MaterialExpressionAbs, -1700)
g.link(g.sub(v, beat_phase, -1750), '', heart_dist, '')
heart_ring = g.power(g.saturate(g.one_minus(g.mulc(heart_dist, 7.0, -1650), -1600), -1550), 3.0, -1500)
heart = g.mul(heart_ring, g.addc(g.mulc(beat_pulse, 1.5, -1500), 0.35, -1450), -1400)

# ------------------------------------------------------------- the rim
fresnel = g.n(unreal.MaterialExpressionFresnel, -1800, exponent=2.6, base_reflect_fraction=0.03)
# A faint rim always, the synchrony halo on top of it.
halo = g.mul(fresnel, g.addc(g.mul(coherence, sync_halo, -1600), 0.35, -1550), -1500)

# ------------------------------------------------------------- colour
alive = g.one_minus(staleness, -1700)
grey = g.rgb(0.16, 0.18, 0.20, -2200)
colour = g.lerp(tint, grey, staleness, -1500)
# The bands burn a little whiter in their cores, as light-filled water does.
band_colour = g.add(colour, g.mulc(g.power(stripes, 3.0, -1300), 0.18, -1250), -1200)

# The bands' light: the mood's glow, a touch more for an aroused person, and
# a touch more when the bubble moves fast through the water.
moving = g.saturate(g.mulc(striation_speed, 0.5, -1500), -1450)
band_gain = g.mul(g.mul(glow_mood, stripe_light, -1400),
                  g.addc(g.add(g.mulc(excitation, 0.35, -1400), g.mulc(moving, 0.25, -1400), -1350), 0.8, -1300), -1250)

emit_bands = g.mul(band_colour, g.mul(stripes, band_gain, -1100), -1000)
emit_rim = g.mul(colour, g.mul(halo, rim_light, -1100), -1000)
emit_heart = g.mul(colour, g.mul(heart, heart_light, -1100), -1000)
emit_body = g.mul(colour, body_light, -1000)
emissive_raw = g.add(g.add(emit_bands, emit_rim, -900), g.add(emit_heart, emit_body, -900), -850)
emissive = g.mul(g.mul(g.mul(emissive_raw, brightness, -800), focus_mask, -700), alive, -650)

# Lit body: dark through the middle (the deep water seen through the shell),
# brighter towards the rim. Subsurface colour richer, so back light glows in
# the bubble's own hue.
body_shade = g.lerp(g.const(0.03, -1300), g.const(0.30, -1300), fresnel, -1200)
base_colour = g.mul(g.mul(colour, body_shade, -900), focus_mask, -600)
sss_colour = g.mul(g.mulc(colour, 0.9, -900), focus_mask, -600)

# See-through: the shell is solid on the bands, at the rim and on the heart
# ring, and thins out between them - then dithered, frame to frame, into the
# illusion of a partial veil.
coverage = g.saturate(g.add(g.add(g.mulc(stripes, 0.9, -1100), g.mulc(fresnel, 1.4, -1100), -1050),
                            g.addc(g.mulc(heart_ring, 0.6, -1050), 0.10, -1000), -950), -900)
mask_value = g.lerp(g.const(1.0, -1000), coverage, transparency, -800)
dither = g.n(unreal.MaterialExpressionMaterialFunctionCall, -600)
dither.set_editor_property(
    'material_function',
    unreal.load_asset('/Engine/Functions/Engine_MaterialFunctions02/Utility/DitherTemporalAA'))
g.link(mask_value, '', dither, 'Alpha Threshold')

# Wet and tight: a small sharp highlight says "liquid". A quiet sensor goes
# matt, so a dead bubble even reads as duller.
roughness = g.lerp(g.const(0.07, -1200), g.const(0.45, -1200), staleness, -1000)
specular = g.const(0.6, -1200)

g.out(base_colour, unreal.MaterialProperty.MP_BASE_COLOR)
g.out(sss_colour, unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
g.out(g.const(0.5, -600), unreal.MaterialProperty.MP_OPACITY)
g.out(dither, unreal.MaterialProperty.MP_OPACITY_MASK)
g.out(emissive, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
g.out(roughness, unreal.MaterialProperty.MP_ROUGHNESS)
g.out(specular, unreal.MaterialProperty.MP_SPECULAR)

MEL.recompile_material(bubble)
assert unreal.EditorAssetLibrary.save_asset(bubble_path), 'sauvegarde du materiau bulle impossible'
unreal.log(f'VibH2O: materiau de bulle ecrit dans {bubble_path}')


# ===========================================================================
# The motes
# ===========================================================================

mote, mote_path = fresh_material('M_VibH2OMote')
# Left on the DEFAULT shading model and blend mode on purpose.
#
# Unlit and additive were both tried here and both came out as black specks -
# measured on screen, not assumed. The bubble material renders correctly in
# this same scene with the default lit/opaque setup, so the motes use exactly
# that: copying the configuration known to work beats debugging the one that
# should. Emissive carries the whole look either way.

m = Graph(mote)
mtime = m.n(unreal.MaterialExpressionTime, -1400)
mpos = m.n(unreal.MaterialExpressionObjectPositionWS, -1400)

# Each mote drifts on its own slow path. The seed comes from where it stands,
# so a field of thousands never pulses in unison.
# PerInstanceRandom is the only seed that actually differs from one instance
# to the next: object position is the component's, shared by all 2600.
mseed = m.n(unreal.MaterialExpressionPerInstanceRandom, -1200)

# Motes do not move in the material: Unreal 5.5 keeps the world position
# offset input out of Python's reach. The whole field drifts instead, as one
# slow body, from AVibH2OWaterMotes - which is closer to how suspended silt
# actually behaves in a current, and costs one transform rather than 2600.
phase = m.add(m.mulc(mtime, 0.35, -1200), m.mulc(mseed, 6.283, -1200), -1000)
twinkle = m.addc(m.mulc(m.sine(phase, -900), 0.25, -850), 0.75, -800)

# Colour: pale blue-green, brightness varying per mote so the field has depth.
mote_col = m.rgb(0.30, 0.72, 0.85, -1400)
mfres = m.n(unreal.MaterialExpressionFresnel, -900, exponent=1.6)
soft = m.addc(m.mulc(mfres, 0.65, -850), 0.35, -800)
bright = m.mul(m.mul(m.addc(m.mulc(mseed, 0.9, -900), 0.25, -800), twinkle, -700), soft, -650)
m.out(m.mul(mote_col, m.mulc(bright, 0.9, -620), -600), unreal.MaterialProperty.MP_EMISSIVE_COLOR)
m.out(m.rgb(0.01, 0.02, 0.03, -600), unreal.MaterialProperty.MP_BASE_COLOR)
m.out(m.const(0.9, -600), unreal.MaterialProperty.MP_ROUGHNESS)

# Soft edges: a hard-edged speck reads as dirt on the lens. In additive mode
# the falloff belongs in the emissive itself, there being no opacity to shape.


MEL.recompile_material(mote)
assert unreal.EditorAssetLibrary.save_asset(mote_path), 'sauvegarde du materiau de particule impossible'
unreal.log(f'VibH2O: materiau de particule ecrit dans {mote_path}')


# ===========================================================================
# The caustics
# ===========================================================================
#
# A light function on the sun: the net of light that the surface ripples throw
# on everything below. It is what most immediately says "under water", and it
# lands on the bubbles and the seabed alike - which is the whole reason the
# bubble is lit rather than emissive.
#
# The Noise node's Voronoi function is out of Python's reach in 5.5 (its
# Function property is refused), so the net is built from gradient noise
# instead: 1 - |noise| makes thin bright ridges where the noise crosses zero,
# and two layers drifting apart cross into the familiar cells. The third noise
# coordinate is time, so the net morphs in place like real caustics rather
# than sliding across the floor like a projected slide.

caustics, caustics_path = fresh_material('M_VibH2OCaustics')
caustics.set_editor_property('material_domain', unreal.MaterialDomain.MD_LIGHT_FUNCTION)
c = Graph(caustics)

c_pos = c.n(unreal.MaterialExpressionWorldPosition, -1800)
c_xy = c.n(unreal.MaterialExpressionComponentMask, -1700, r=True, g=True, b=False, a=False)
c.link(c_pos, '', c_xy, '')
c_time = c.n(unreal.MaterialExpressionTime, -1800)


def ridge_of(scale, speed, offset, x):
    """One layer of ridges: |noise| inverted and sharpened into thin lines."""
    z = c.addc(c.mulc(c_time, speed, x - 300), offset, x - 250)
    pos = c.n(unreal.MaterialExpressionAppendVector, x - 200)
    c.link(c_xy, '', pos, 'A')
    c.link(z, '', pos, 'B')
    noise = c.n(unreal.MaterialExpressionNoise, x - 100,
                scale=scale, quality=1, turbulence=False, levels=1,
                output_min=-1.0, output_max=1.0)
    c.link(pos, '', noise, 'Position')
    absn = c.n(unreal.MaterialExpressionAbs, x)
    c.link(noise, '', absn, '')
    return c.power(c.one_minus(absn, x + 50), 7.0, x + 100)


# Cells of roughly half a metre to a metre: a few per bubble spacing, so the
# net reads on a single bubble as well as across the floor.
ridge_a = ridge_of(0.011, 18.0, 0.0, -1200)
ridge_b = ridge_of(0.017, -13.0, 517.0, -800)

# Never fully dark between the ridges: real caustics sit on an even wash of
# light, they do not cut the sun into lines.
net = c.add(c.mulc(c.add(ridge_a, ridge_b, -300), 1.6, -200), c.const(0.35, -300), -100)
c.out(net, unreal.MaterialProperty.MP_EMISSIVE_COLOR)

MEL.recompile_material(caustics)
assert unreal.EditorAssetLibrary.save_asset(caustics_path), 'sauvegarde du materiau de caustiques impossible'
unreal.log(f'VibH2O: caustiques ecrites dans {caustics_path}')


# ===========================================================================
# The seabed
# ===========================================================================
#
# Something for the light to land on. Without a floor the caustics and the
# bubbles' shadows have nowhere to show, and the room floats in a void; with
# one, the eye reads depth and scale at once.

seabed, seabed_path = fresh_material('M_VibH2OSeabed')
b = Graph(seabed)

b_pos = b.n(unreal.MaterialExpressionWorldPosition, -1400)
b_noise = b.n(unreal.MaterialExpressionNoise, -1200,
              scale=0.004, quality=1, turbulence=True, levels=4,
              output_min=0.0, output_max=1.0)
b.link(b_pos, '', b_noise, 'Position')
b_fine = b.n(unreal.MaterialExpressionNoise, -1200,
             scale=0.05, quality=1, turbulence=True, levels=2,
             output_min=0.0, output_max=1.0)
b.link(b_pos, '', b_fine, 'Position')

# Dark wet sand, mottled at two scales: large patches and fine grain. Kept dark
# so the caustics, not the floor, are what catches the eye.
sand_dark = b.rgb(0.035, 0.040, 0.038, -1200)
sand_light = b.rgb(0.11, 0.115, 0.095, -1200)
mottle = b.add(b.mulc(b_noise, 0.75, -1000), b.mulc(b_fine, 0.25, -1000), -900)
b.out(b.lerp(sand_dark, sand_light, mottle, -700), unreal.MaterialProperty.MP_BASE_COLOR)
b.out(b.addc(b.mulc(b_fine, 0.15, -800), 0.8, -700), unreal.MaterialProperty.MP_ROUGHNESS)
b.out(b.const(0.3, -700), unreal.MaterialProperty.MP_SPECULAR)

MEL.recompile_material(seabed)
assert unreal.EditorAssetLibrary.save_asset(seabed_path), 'sauvegarde du materiau de fond impossible'
unreal.log(f'VibH2O: fond marin ecrit dans {seabed_path}')
