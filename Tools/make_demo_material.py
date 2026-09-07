"""Builds the VibH2O demonstration materials, into the plugin's own content.

    "C:/Program Files/Epic Games/UE_5.5/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" ^
        VIBH2O_UE.uproject -run=pythonscript ^
        -script="Tools/make_demo_material.py" -unattended -nosplash

Two materials, both disposable and regenerable:

  M_VibH2ODemoBubble   the bubble
  M_VibH2OMote         the specks suspended in the water

The bubble aims at ORGANIC, not geometric. Three things do that work:

  * the silhouette breathes. World position offset swells the mesh along its
    normal with two out-of-phase noise waves plus the heartbeat, so no bubble
    is ever a clean sphere and no two are deformed alike.
  * the light wraps. A jellyfish bell is translucent, so its rim glows where
    light passes through it; a hard Lambert edge is the single thing that most
    makes a sphere read as plastic.
  * nothing is uniform. Every bubble gets a hue and a rate of its own, drawn
    from its position, so a room of two hundred never looks stamped.

Arousal drives a three-stop gradient rather than a two-colour lerp: deep blue
when calm, teal through the middle, hot amber when excited. A straight lerp
between two ends passes through a dead grey in the middle, which is exactly
where most of the audience sits.

The material is Opaque / DefaultLit, per the spec: a translucent material
would receive neither shadows nor caustics properly.
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

bubble, bubble_path = fresh_material('M_VibH2ODemoBubble')
g = Graph(bubble)

# --- the contract, plus the two extras
beat_phase = g.scalar('BeatPhase', 0.0)
beat_pulse = g.scalar('BeatPulse', 0.0)
bpm = g.scalar('Bpm', 60.0)
excitation = g.scalar('Excitation', 0.0)
synchrony = g.scalar('Synchrony', 0.5)
collective = g.scalar('CollectiveSynchrony', 0.5)
striation_speed = g.scalar('StriationSpeed', 0.0)
focus_mask = g.scalar('FocusMask', 1.0)
staleness = g.scalar('Staleness', 0.0)
blend_alpha = g.scalar('BlendAlpha', 0.0)
tint = g.n(unreal.MaterialExpressionVectorParameter, -2600,
           parameter_name='TintColor',
           default_value=unreal.LinearColor(0.05, 0.35, 0.75, 1.0))

time = g.n(unreal.MaterialExpressionTime, -2600)

# --- per-bubble identity
#
# Object position seeds a value that differs from one bubble to the next, so
# noise, hue and rate are never in step across the room. Without this the whole
# room breathes as one animal.
obj_pos = g.n(unreal.MaterialExpressionObjectPositionWS, -2600)
seed_dot = g.n(unreal.MaterialExpressionDotProduct, -2400)
seed_vec = g.rgb(0.017, 0.031, 0.011, -2600)
g.link(obj_pos, '', seed_dot, 'A')
g.link(seed_vec, '', seed_dot, 'B')
seed = g.frac(seed_dot, -2200)

# ------------------------------------------------------------ organic noise
#
# The silhouette is deformed in C++, on the actor's scale: Unreal 5.5 does not
# expose the material's world position offset input to Python, so a generated
# material cannot reach it. What the material does instead is break the
# GEOMETRY of the pattern - a perfectly concentric ring on a perfect sphere is
# the single thing that most makes a bubble read as a manufactured object.
world_pos = g.n(unreal.MaterialExpressionWorldPosition, -2600)

noise_a = g.n(unreal.MaterialExpressionNoise, -2200,
              scale=0.05, quality=1, turbulence=True, levels=2,
              output_min=-1.0, output_max=1.0)
g.link(world_pos, '', noise_a, 'Position')

# The second field is offset in space over time, so the distortion crawls
# across the surface instead of sitting still on it.
drift = g.mulc(time, 11.0, -2400)
drift_xy = g.n(unreal.MaterialExpressionAppendVector, -2300)
g.link(drift, '', drift_xy, 'A')
g.link(drift, '', drift_xy, 'B')
drift_xyz = g.n(unreal.MaterialExpressionAppendVector, -2200)
g.link(drift_xy, '', drift_xyz, 'A')
g.link(drift, '', drift_xyz, 'B')
pos_drift = g.add(world_pos, drift_xyz, -2100)

noise_b = g.n(unreal.MaterialExpressionNoise, -2000,
              scale=0.14, quality=1, turbulence=True, levels=2,
              output_min=-1.0, output_max=1.0)
g.link(pos_drift, '', noise_b, 'Position')

# Arousal deepens the distortion: a stirred bubble is visibly less regular
# than a calm one, before a single colour has changed.
warp_amt = g.addc(g.mulc(excitation, 0.075, -1900), 0.030, -1800)

# ------------------------------------------------------------------- pattern
uv = g.n(unreal.MaterialExpressionTextureCoordinate, -2600)
centre = g.n(unreal.MaterialExpressionConstant2Vector, -2600, r=0.5, g=0.5)
radial = g.n(unreal.MaterialExpressionDistance, -2400)
g.link(uv, '', radial, 'A')
g.link(centre, '', radial, 'B')

# Rings: frac(BeatPhase * N - r * K), N following the heart rate so a fast
# heart packs more of them.
ring_n = g.addc(g.mulc(bpm, 0.07, -2200), 3.5, -2100)
radial_warp = g.add(radial, g.mul(noise_a, warp_amt, -2100), -2050)
r_scaled = g.mul(radial_warp, ring_n, -2000)
ring_in = g.sub(beat_phase, r_scaled, -1900)
rings = g.power(g.frac(ring_in, -1800), 5.0, -1700)

# Striations: fine radial stripes scrolling at the bubble's measured speed.
u = g.n(unreal.MaterialExpressionComponentMask, -2400, r=True, g=False, b=False, a=False)
g.link(uv, '', u, '')
scroll = g.mul(time, striation_speed, -2200)
stripe_in = g.add(g.add(g.mulc(u, 34.0, -2100), scroll, -2000),
                  g.mulc(noise_b, 2.6, -2000), -1950)
stripes = g.power(g.addc(g.mulc(g.sine(stripe_in, -1900), 0.5, -1800), 0.5, -1700), 4.0, -1600)

# Wrapped light: N·L pushed into the shadow side, then a rim. Together they
# stand in for the subsurface scattering of a translucent bell without paying
# for a translucent material.
fresnel = g.n(unreal.MaterialExpressionFresnel, -1800, exponent=2.4, base_reflect_fraction=0.04)
sync_sum = g.add(g.mulc(synchrony, 0.55, -1900), g.mulc(collective, 0.45, -1900), -1800)
rim = g.mul(fresnel, g.addc(sync_sum, 0.35, -1700), -1600)

# ------------------------------------------------------------------- colour
#
# A three-stop arousal ramp. A two-colour lerp would pass through grey exactly
# where most of the audience sits, so the middle gets a colour of its own.
calm = g.rgb(0.012, 0.09, 0.28, -2600)
mid = g.rgb(0.03, 0.42, 0.40, -2600)
hot = g.rgb(0.85, 0.20, 0.045, -2600)

exc = g.saturate(excitation, -2200)
low_ramp = g.saturate(g.mulc(exc, 2.0, -2100), -2000)
high_ramp = g.saturate(g.addc(g.mulc(exc, 2.0, -2100), -1.0, -2000), -1900)
ramp = g.lerp(g.lerp(calm, mid, low_ramp, -1800), hot, high_ramp, -1700)

# The artist's TintColor still has a say - it tilts the ramp rather than being
# ignored, so changing it in the Details panel does something visible.
base_hue = g.lerp(ramp, tint, g.const(0.25, -2200), -1600)

# Per-bubble hue drift, so two hundred bubbles at the same arousal are not two
# hundred copies.
hue_jitter = g.mulc(g.addc(seed, -0.5, -2100), 0.22, -2000)
jitter_col = g.rgb(0.10, 0.34, 0.55, -2200)
hue = g.add(base_hue, g.mul(jitter_col, hue_jitter, -1900), -1500)

# Vortex tilt: the school reads greener than the seated room.
vortex_hue = g.rgb(0.06, 0.62, 0.45, -2200)
hue_blend = g.lerp(hue, vortex_hue, g.mulc(blend_alpha, 0.55, -1800), -1400)

# Quiet sensor: the colour drains out and the pattern dies with it.
grey = g.rgb(0.16, 0.18, 0.20, -2200)
colour = g.lerp(hue_blend, grey, staleness, -1300)

# ------------------------------------------------------------------ assembly
alive = g.one_minus(staleness, -1700)

# Kept deliberately low: a glow that blows out to white takes the arousal
# ramp with it, and the colour is the whole point of the parameter.
glow = g.addc(g.add(g.mulc(beat_pulse, 1.15, -1600), g.mulc(excitation, 0.85, -1600), -1500), 0.16, -1400)
pattern = g.add(g.add(g.mul(rings, glow, -1300), g.mulc(stripes, 0.30, -1300), -1200), rim, -1100)
pattern_live = g.mul(pattern, alive, -1000)

emissive = g.mul(g.mul(colour, pattern_live, -800), focus_mask, -600)
base_colour = g.mul(g.mulc(colour, 0.85, -800), focus_mask, -600)

# A wet, tight highlight; slightly rougher when the sensor has gone quiet, so
# a dead bubble even reads as a duller material.
roughness = g.lerp(g.const(0.18, -1200), g.const(0.55, -1200), staleness, -1000)
specular = g.const(0.75, -1200)

g.out(base_colour, unreal.MaterialProperty.MP_BASE_COLOR)
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
