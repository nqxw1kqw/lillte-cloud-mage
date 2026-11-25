# Phù thủy nhảy mây: cloud set v2 (inspired by WMO cloud types)

Ten static DirectX `.x` clouds. They use the same exporter and conventions as v1 (`../_tools/export_x.py`, unchanged), and v1 (`../cloud_1..3.x`) is untouched.
**New convention: the highest point of every mesh is at Y = 0** (the landing top). The code places the top directly; the model origin is the centre on X/Z at the top height.
Y-up, X right, Z forward (DxLib file space). 1 unit = 1 game unit (the witch is 3.9 tall). All files and their PNGs are in this folder, `clouds_v2/`.

| # | file | look | size X × Y × Z | top Y | tris | material (texture) | collision (clouds_v2.json) |
|---|---|---|---|---|---|---|---|
| 1 | `cu_humilis.x` | small flat fair-weather puff (hard, small landing target) | 3.00 × 1.19 × 2.40 | 0.000 | 800 | `cv2_white` (cv2_white.png) | hw 0.45 / hd 0.55; surface -0.074..-0.001 (median -0.018) |
| 2 | `cu_mediocris.x` | standard puffy cumulus, small bumps on a broad top | 5.20 × 2.19 × 4.00 | 0.000 | 1056 | `cv2_white` (cv2_white.png) | hw 1.35 / hd 0.7; surface -0.186..-0.005 (median -0.046) |
| 3 | `cu_congestus.x` | tall cauliflower tower, flat-ish top | 4.50 × 5.99 × 4.00 | 0.000 | 1450 | `cv2_white` (cv2_white.png) | hw 0.8 / hd 0.45; surface -0.131..-0.0 (median -0.012) |
| 4 | `sc_stratocumulus.x` | wide lumpy platform (easy, safe landing) | 9.00 × 1.59 × 5.50 | 0.000 | 782 | `cv2_white` (cv2_white.png) | hw 2.9 / hd 0.95; surface -0.131..-0.007 (median -0.027) |
| 5 | `ac_floccus.x` | 6 separate tufts with ragged bottoms, one mesh | 5.93 × 1.44 × 4.02 | 0.000 | 1030 | `cv2_white` (cv2_white.png) | hw 0.5 / hd 0.3 @(0.005,-0.215); surface -0.118..-0.001 (median -0.02) |
| 6 | `ac_lenticularis.x` | two stacked smooth lenses with crisp edges (duplicatus) | 6.50 × 1.40 × 4.00 | 0.000 | 1144 | `cv2_white` (cv2_white.png) | hw 1.2 / hd 0.85; surface -0.199..-0.0 (median -0.038) |
| 7 | `ci_fibratus.x` | thin wispy streaks with hooked ends (uncinus); alpha ribbons, decorative | 10.00 × 0.30 × 2.50 | 0.000 | 576 | `cv2_cirrus` (cv2_cirrus_alpha.png) **ALPHA** | not landable |
| 8 | `cb_incus.x` | huge cumulonimbus: tower, flat anvil, mamma pouches; landmark/background | 12.00 × 13.99 × 10.00 | 0.000 | 1500 | `cv2_white` (cv2_white.png) | not landable |
| 9 | `ns_virga.x` | darker grey rain layer, pannus scraps, virga streaks (alpha cards) | 8.07 × 2.50 × 5.00 | 0.000 | 1346 | `cv2_grey` (cv2_grey.png) + `cv2_virga` (cv2_virga_alpha.png) **ALPHA** | hw 1.4 / hd 1.4; surface -0.035..-0.006 (median -0.022) |
| 10 | `nlc_noctilucent.x` | thin rippled (undulatus) glowing silver-blue layer (bonus) | 7.08 × 0.80 × 4.06 | 0.000 | 1192 | `cv2_nlc` (cv2_noctilucent.png) | hw 2.5 / hd 0.9; surface -0.346..-0.007 (median -0.178) |

Total: 10,876 tris. Size and top Y come from parsing the exported `.x` (`src/verify_v2.txt`) and match assimp's reported bounds.

## Materials / textures / alpha
- `cv2_white.png` (64², opaque): moonlight white `#f2f6ff` on top, fading through `#cdd9f0` to `#b4c6e7` on the underside. It has 3 soft lighting steps baked in (UV u = lighting term, v = height), so it looks right unlit or with Lambert lighting.
- `cv2_grey.png` (64², opaque): the nimbostratus body, `#a9b1ca` → `#8a93b0` → `#68718e`.
- `cv2_noctilucent.png` (128×64, opaque, bright): silver-blue `#eef8ff` on the ripple crests, `#7fa6e6` in the troughs, darker `#5b7fc4` at the edges. The ripples are coloured
  through UV u = crest brightness. Emissive in the `.x` is 0; draw it unlit or emissive (e.g. `MV1SetMaterialEmiColor` or with lighting off) for the glow.
- **Alpha materials that need alpha blending: `cv2_cirrus` (`ci_fibratus.x`, all faces) and `cv2_virga` (`ns_virga.x`, the 96 streak-card faces).**
  Both PNGs are real RGBA (min alpha 0, soft edges). Draw them after the opaque geometry with depth-write off (or with alpha test plus blending) so they sort cleanly.
  Every other material is fully opaque (alpha 255).
- **Alpha cards are double-sided geometry**: each card is two coincident faces with opposite winding and their own normals. **Keep back-face culling ON**;
  you don't need to turn culling off. If culling is turned off, both faces draw and the overlapping half-transparent layers look roughly twice as opaque.
- The solid clouds are closed, smooth-shaded meshes with outward normals. The lenticular rims use split normals for a crisp edge.

## clouds_v2.json schema
```
{ "version": 2, "units", "axes", "note",
  "clouds": { "<name>": {
      "file": "<name>.x", "tris": int, "landable": bool, "topY": float (max Y of the mesh, always 0.0),
      "bounds": {"min": [x,y,z], "max": [x,y,z], "size": [x,y,z]},             // all 10
      "collision": {                                                          // landable ones only (1-6, 9, 10)
          "cx", "cz": centre of the box in model space (0,0 except ac_floccus),
          "hw", "hd": conservative half sizes along X / Z of the solid landable top,
          "topY": highest surface point inside the box, "surfaceY_min", "surfaceY_median": the measured top surface inside the box,
          "tolerance": the box only covers surface within [-tolerance, 0],
          "ellipse_rx", "ellipse_rz": optional larger inscribed ellipse (same aspect) if you prefer elliptical tests },
      "tufts": [ {"cx","cz","hw","hd","topY","main"} ]                       // ac_floccus only: one box per tuft
  } } }
```
The boxes come from a 3D raycast heightfield of each top. A box is the **largest centred rectangle in which every sample has a top surface within the tolerance of Y = 0**,
then shrunk by 5% and rounded down to 0.05. So they are conservative: they stay off the sloping puff edges and the wisps. Because the tops are roughly elliptical,
the rectangle is about 70% of the solid top's width. Use `ellipse_rx/rz` for a somewhat larger safe area.
**ac_floccus:** `collision` covers only the **main central tuft** (centre (0.005, -0.215)); the 5 smaller tufts are listed in `tufts` with their own small boxes and tops (Y -0.05 to -0.13).
**nlc_noctilucent** is wavy by design: within its box the surface runs from -0.35 to 0 (median -0.18). Snap the feet to `surfaceY_median` or treat the ripples as cosmetic.
**ci_fibratus** and **cb_incus** are decorative or background (`landable: false`).

## Previews (`previews/`, PNG + JPG under 300 KB)
- `clouds_v2_lineup.png/.jpg`: all 10 in two rows at one shared scale, labelled with size and tris, with the witch standing on `cu_mediocris`.
- `clouds_v2_gamecam.png/.jpg`: game camera (11 u behind, 4.5 u above, 60° vertical FOV). The witch stands on `cu_mediocris` at Y = 0, with the other clouds at different heights and distances (cb_incus far away as a landmark).

## QA
- `src/verify_v2.py` (an independent parse in plain python): **ALL OK**. Every file has the `xof 0303txt` header, 1 mesh, top Y = 0.0000 and is centred at X/Z = (0,0).
  Texture paths are relative and exist, and the alpha PNGs really have alpha. There are no zero-area faces, and normals are unit length and match the DirectX
  clockwise winding (98.8–100%; the few faces that disagree are small slivers left by decimation on smooth puffs). Every closed component has a positive (outward) volume, and the only open parts are the alpha cards.
- `../_tools/verify_x.py` (the Dance Raid checker): RESULT OK for all 10 (`src/verify_x_v2.txt`).
- `assimp info` 5.4.3 loads all 10 with matching face counts, bounds and texture refs (`src/assimp_v2.txt`).

## Source / rebuild (`src/`)
`make_clouds_v2.py` (geometry, textures, export) → `clouds_v2.blend`. `analyze_v2.py` writes `clouds_v2.json`. `render_v2.py` + `compose_v2.py` make the previews. `verify_v2.py` runs the QA.
```
cd /workspace/mahoushojp-assets/clouds_v2/src
blender -b -P make_clouds_v2.py && blender -b clouds_v2.blend -P analyze_v2.py && python3 verify_v2.py
blender -b clouds_v2.blend -P render_v2.py && python3 compose_v2.py
```

## Limitations / DxLib risks
- Not yet loaded in DxLib 3.24f; checked only with the parsers, assimp and Blender renders. The pipeline is the same one used for v1 and Dance Raid.
- The faceted low-poly look comes from metaballs decimated to about 1k tris. The tops were flattened for landing, so they read as stylised plateaus rather than true cumulus domes.
- The `cb_incus` mamma pouches are few and angular at 1,500 tris. `cu_congestus` has a small landing box (hw 0.8 / hd 0.45) because its top is a narrow tower.
- `ns_virga` is 8.07 wide (the slanted virga cards stick out 0.03 past 8.0); its Y range is -2.50 to 0 including the streaks, and the solid grey body bottom is at about -1.6, with pannus scraps down to about -1.95.
- Alpha sorting with other transparent objects is up to the game code (see Materials).
