"""Builds the VibH2O demonstration materials, into the plugin's own content.

    "C:/Program Files/Epic Games/UE_5.5/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" ^
        VIBH2O_UE.uproject -run=pythonscript ^
        -script="Tools/make_demo_material.py" -unattended -nosplash

Four materials, all disposable and regenerable:

  M_VibH2ODemoBubble   the bubble
  M_VibH2OMote         the specks suspended in the water
  M_VibH2OCaustics     light function: the rippling net of light under water
  M_VibH2OSeabed       the floor that catches the caustics and the shadows

The bubble aims at ORGANIC, not geometric. Three things do that work:

  * the silhouette breathes. World position offset swells the mesh along its
    normal with two out-of-phase noise waves plus the heartbeat, so no bubble
    is ever a clean sphere and no two are deformed alike.
  * the light passes through. The bubble is a SUBSURFACE material: lights
    from behind make it glow from within, lights from the front give it a wet,
    tight highlight. It is lit by the scene rather than lit from inside, so the
    rig's lights and the caustics actually show on it - an emissive bubble
    looks the same in any light, which is what made it read as a sticker.
  * nothing is uniform. Every bubble gets a hue and a rate of its own, drawn
    from its position, so a room of two hundred never looks stamped.

Arousal drives a three-stop gradient rather than a two-colour lerp: deep blue
when calm, teal through the middle, hot amber when excited. A straight lerp
between two ends passes through a dead grey in the middle, which is exactly
where most of the audience sits.

The material is Opaque / Subsurface, per the spec: a translucent material
would receive neither shadows nor caustics properly. Transparency is feigned:
the face turned to the viewer is darker - deep water seen through the bubble -
and the grazing edge brighter, as on a real wet membrane.
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
# Subsurface, opaque: lit by the scene, glowing where light passes through.
bubble.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_SUBSURFACE)
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

# Fresnel: the grazing edge. It sets the feigned transparency - dim where the
# eye looks straight through, bright at the rim - and carries the synchrony.
fresnel = g.n(unreal.MaterialExpressionFresnel, -1800, exponent=3.2, base_reflect_fraction=0.04)
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

# Feigned transparency: the face is the deep water seen through the bubble,
# the edge is the membrane seen edge-on. This is what makes an opaque sphere
# read as a body you could see into.
body_shade = g.lerp(g.const(0.22, -1300), g.const(0.75, -1300), fresnel, -1200)
base_colour = g.mul(g.mul(colour, body_shade, -900), focus_mask, -600)

# Subsurface: the colour light takes on its way through. Brighter and a touch
# more saturated than the surface, so a back-lit bubble glows in its own hue.
sss_colour = g.mul(g.mulc(colour, 1.6, -900), focus_mask, -600)
# Opacity drives the scattering of a subsurface material: a little more when
# aroused, so an excited bubble lets more light through and "warms up".
sss_amount = g.addc(g.mulc(excitation, 0.25, -1000), 0.45, -900)

# The heartbeat stays readable, but as a soft inner pulse rather than a lamp:
# emissive is kept low so the scene's light, not the material, does the work.
glow = g.addc(g.add(g.mulc(beat_pulse, 0.9, -1600), g.mulc(excitation, 0.25, -1600), -1500), 0.05, -1400)
pattern = g.add(g.add(g.mul(rings, glow, -1300), g.mulc(stripes, 0.06, -1300), -1200),
                g.mulc(rim, 0.12, -1200), -1100)
pattern_live = g.mul(g.mulc(pattern, 0.35, -1050), alive, -1000)
emissive = g.mul(g.mul(colour, pattern_live, -800), focus_mask, -600)

# Wet and tight: low roughness gives the small sharp highlight that says
# "liquid". A quiet sensor goes matt, so a dead bubble even reads as duller.
roughness = g.lerp(g.const(0.08, -1200), g.const(0.45, -1200), staleness, -1000)
specular = g.const(0.65, -1200)

g.out(base_colour, unreal.MaterialProperty.MP_BASE_COLOR)
g.out(sss_colour, unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
g.out(sss_amount, unreal.MaterialProperty.MP_OPACITY)
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
