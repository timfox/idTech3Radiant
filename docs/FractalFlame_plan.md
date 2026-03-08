# Fractal Flame Editor – Integration Plan

Integrate a fractal flame editor into NetRadiant as a plugin for creating flares, textures, skybox content, and procedural art. Based on the [Fractal Flame Algorithm](https://flam3.com/flame.pdf) (Scott Draves, Erik Reckase, 2003) and inspired by [Apophysis 7x](https://github.com/wanily/apophysis7x).

## Overview

The fractal flame algorithm is an Iterated Function System (IFS) that produces organic, high-quality images via:
- **Non-linear variations** (sinusoidal, spherical, swirl, horseshoe, etc.)
- **Log-density display** (handles power-law density distribution)
- **Structural coloring** (color conveys which functions generated each region)

## Architecture

```
contrib/flameplug/
├── flameplug.cpp       # Plugin + Qt dialog
├── flameplug.def       # Windows DLL exports
├── flame_renderer.h    # Chaos game + variations
├── flame_renderer.cpp  # Render to RGBA buffer
└── flame_variations.h  # Variation functions (V0–V48+)
```

## Phases

### Phase 1: Minimal plugin (MVP)
- [x] Plugin skeleton (menu entry, dialog)
- [x] Basic chaos game with 11 variations (linear, sinusoidal, spherical, swirl, horseshoe, polar, disc, heart, fisheye, bubble, exponential)
- [x] Preview widget (256×256)
- [x] Export to PNG at 256/512/1024
- [x] Preset configs (Flame, Flare, Spiral, Organic, Star)
- [x] Log-density + gamma display

### Phase 3: Production features
- [ ] Post transforms, final transform
- [ ] Symmetry (rotational, dihedral)
- [ ] .flam3 / .flame file import/export (Apophysis compatibility)
- [ ] Preset library (flares, skies, decals)
- [ ] Batch export for texture atlases

## Algorithm Summary

**Chaos game (simplified):**
```
(x,y) = random in [-1,1]²
c = random in [0,1]
for 20 burn-in + N iterations:
  i = random function index (weighted)
  (x,y) = F_i(x,y)   # affine + variation
  c = (c + c_i) / 2
  plot(x, y, c)
```

**Per-function:** `F_i(x,y) = Σ_j v_ij V_j(a_i x + b_i y + c_i, d_i x + e_i y + f_i)`

**Key variations (from paper):**
- V0: linear (identity)
- V1: sinusoidal
- V2: spherical
- V3: swirl
- V4: horseshoe
- … (see Appendix A for full catalog)

## Use Cases in Radiant

1. **Flares** – Export as `textures/flares/myflare.png`, reference in shader
2. **Skybox accents** – Cloud layers, aurora, nebulae
3. **Decals** – Organic patterns, damage, stains
4. **Light cookies** – Gobo textures for q3map2
5. **Menu/UI** – Backgrounds, icons

## References

- Draves & Reckase: *The Fractal Flame Algorithm* (2003, rev 2008)
- Apophysis 7x: https://github.com/wanily/apophysis7x
- flam3 format: https://github.com/scottdraves/flam3
