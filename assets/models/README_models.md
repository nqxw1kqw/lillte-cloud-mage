# Phù thủy nhảy mây (mahoushojp-adventure): Stage 1 3D assets

These are built procedurally in headless Blender 4.2.3 LTS (`/workspace/blender_app/`, available as `~/.local/bin/blender`) and exported with the same DirectX `.x`
exporter the Dance Raid assets use (`_tools/export_x.py`, unchanged copy). The Windows project folder and `/workspace/dancing-battle-assets/` were not modified.

## Deliverables (each `.x` has its PNG in the same folder)
| file | what | tris | texture |
|---|---|---|---|
| `witch_inplace.x` | witch + broom: 1 skinned mesh, 1 armature (27 bones), 6 AnimationSets, in place | 3,704 | `witch_diffuse.png` (256², toon palette) |
| `cloud_1.x` | static cloud A | 1,000 | `cloud_diffuse.png` (64², moonlit gradient) |
| `cloud_2.x` | static cloud B (three-dome top) | 1,050 | `cloud_diffuse.png` |
| `cloud_3.x` | static cloud C (lumpy, asymmetric) | 1,100 | `cloud_diffuse.png` |

`TextureFilename` holds just the bare file name, such as `"witch_diffuse.png"`, so DxLib resolves it next to the `.x`. Each file uses one material, `witch_mat` or `cloud_mat`. The diffuse colour is white
1,1,1, specular and emissive are 0, and there are no PBR maps. Lighting is baked into the textures (3-tone toon cells on the witch, a soft lit/height gradient on the clouds), so
the assets look right unlit or with plain Lambert lighting.

Previews are in `previews/`. Each PNG has a JPG copy under 300 KB:
- `witch_turnaround.png/.jpg`: front, 3/4, back, side, and the game camera (11 u behind, 4.5 u up) at idle frame 0.
- `witch_clips.png/.jpg`: idle f0/f30, fly f10 (3/4), **fly f10 side view**, fly from the game camera, jump crouch and launch, fall, land squash and settle, bounce squash and stretch.
- `clouds_preview.png/.jpg`: the witch standing on `cloud_1` seen from the game camera (with cloud_2/3 behind), the 3 clouds side by side, a front elevation, and a side view of the feet on the cloud.

Source is in `src/`: `build_witch.py` + `witchkit.py` → `witch.blend`; `export_witch.py` writes `witch_inplace.x`; `make_clouds.py` → `clouds.blend` + the cloud `.x` files;
`render_previews.py` + `compose_previews.py` make the previews; `qa_witch.py` and `_tools/verify_x.py` do the QA. The reports are `src/qa_witch.txt`, `src/verify_*.txt`
and `src/assimp_*.txt`.
Rebuild everything:
```
cd /workspace/mahoushojp-assets/src
blender -b -P build_witch.py && blender -b witch.blend -P export_witch.py && blender -b -P make_clouds.py
blender -b witch.blend -P ../_tools/verify_x.py -- ../witch_inplace.x verify_witch_x.txt
blender -b witch.blend -P qa_witch.py -- qa_witch.txt
blender -b witch.blend -P render_previews.py && python3 compose_previews.py
```

## Conventions (same as Dance Raid's `export_x.py`)
- Blender (x, y, z) Z-up RH is converted to `.x`/DxLib (x, z, y) Y-up LH, keeping the face index order (DxLib front face = clockwise, so back-face culling works).
  `AnimTicksPerSecond 30`, one 4×4 matrix key per frame, so **animation time 1 = one 30 fps frame**.
- The witch **faces +Z** in the file. Her right hand is +X, the broom's front tip points +Z and the bristles point **-Z**. The game camera (11 u behind = -Z, 4.5 u up) sees her back.
  Note: the Dance Raid characters actually face **-Z** in their files (toes and face at -Z), even though their README says +Z. If your code reused Dance Raid yaw
  values, the witch may need a different yaw (π).
- Units: 1 unit = 1 game unit. The pivot is at the feet: the origin is (0,0,0) on the ground plane between her boots, at Y = 0.
- Bone and frame names: Blender `.L/.R` become `_L/_R` in the `.x`, because the exporter makes names identifier-safe (e.g. `upper_arm_R`, `skirt_L`).

## witch_inplace.x
- **Dimensions** (bind pose, from the parsed `.x`): height **3.900** (min Y 0.000, max Y 3.900 = hat tip), x [-0.86, 1.15], z [-1.80, 1.61] (this includes the broom resting at her right side in bind pose).
  The hat brim radius is **0.86**, the skirt hem radius is **0.57** (0.55 plus the scallops) with the hem at Y 0.56, and the head is Ø 0.84 (chibi proportions).
  The broom is **3.40** long overall: a 2.5 u stick plus golden bristles tied with a brown cord, with the bristle end at -Z. In `fly` the bristle end is at Z -1.80 and the stick tip at Z +1.61.
- **Palette** (`witch_diffuse.png`, 32 px cells, highlight / base / shade band per colour): skirt `#6a2c91`, bodice/arms `#7b3fa8`, skin `#f5c9a5`, hair `#2b1b3d`,
  hat `#1a1a2e` with a violet `#8e4ccf` band, gold buckle and gold star charms, broom stick `#8b5a2b`, bristles `#c9a227` (with a darker straw stripe `#a8831c`), cord `#5e3a1c`,
  plus a white collar, violet bow, dark plum stockings, brown curled-toe boots, a darker skirt lining (the skirt is double-sided) and face decals (eyes with highlights, lashes, blush, mouth).
- **Skeleton** (27 bones): root, hips, spine, chest, neck, head, hat, hat_tip, hair_1, hair_2, upper_arm/lower_arm/hand `.L/.R`, upper_leg/lower_leg/foot `.L/.R`,
  skirt_front, skirt_back, skirt `.L/.R`, **broom** (child of hips). The broom is 100% weighted to `broom`. The hands' IK and the broom were baked to plain FK keys, so the file has no constraints.
- **Skin**: every vertex is weighted, weights sum to 1, max 3 influences per vertex (XSkinMeshHeader 3/9/26).

### Animation clips (AnimationSets, in this order; `MV1GetAnimName` returns these names)
| # | name | frames (30 fps) | keys | loop | content |
|---|---|---|---|---|---|
| 0 | `idle` | 0–60 (2.00 s) | 61 | yes, f60 = f0 | standing on a cloud, breathing and bob, hat tip and hair sway; broom held in the right hand, diagonal at her right side, bristles behind and below (≥ 0.09 above the floor) |
| 1 | `fly` | 0–40 (1.33 s) | 41 | yes, f40 = f0 | straddling the broom, both hands on the stick, legs dangling, hips bob ±0.045, hat tip, hair and skirt trail back; torso 12–14° forward of vertical (close to upright, so code can add ±0.6 rad tilt) |
| 2 | `jump` | 0–20 (0.67 s) | 21 | no | crouch (f6), launch (f11), rise into the air pose (f20) = `fall` f0 exactly |
| 3 | `fall` | 0–30 (1.00 s) | 31 | yes, f30 = f0 | airborne descent: hips +0.72, skirt flares up, left arm flutters, broom held level in the right hand |
| 4 | `land` | 0–16 (0.53 s) | 17 | no | legs reach down (f0), touch down (f3), squash (f7), overshoot (f12), settle (f16) = `idle` f0 (0.1 mm) |
| 5 | `bounce` | 0–24 (0.80 s) | 25 | no | quick squash (f4–6), big stretch launch (f11), tuck (f17), ends in the air pose (f24) = `fall` f0 exactly |

`MV1GetAttachAnimTotalTime` = last frame (60/40/20/30/16/24). Suggested chains: `jump`→`fall`, `bounce`→`fall`, `land`→`idle`; these joins are seamless.
**In place:** the `root` key equals its rest matrix in every frame of every clip (max |M − rest| = 0). All vertical motion (crouch, hop, airborne height) is on `hips`.
In the standing frames (all of idle, land f3+, jump f0–7, bounce f0–7) the boot soles stay at Y = 0.0000 (−0.0001 at worst) and nothing goes below Y = 0 in any frame
(the broom stays ≥ 0.09 above the floor). In the airborne clips the feet are above Y 0.18 (`fly`) and 0.77 (`fall`).

## cloud_1.x / cloud_2.x / cloud_3.x
- Static meshes with no frames or bones besides `Scene_Root` > `cloud_N`, smooth normals, closed outward surfaces (made from metaballs plus a flattened plateau ellipsoid, decimated, then welded and cleaned).
- **Exact bounds** (parsed from the `.x`): x [-2.600, 2.600], **y [-0.800, +1.200]**, z [-2.000, 2.000], so 5.20 × 2.00 × 4.00 centred on (0,0,0). The upper surface of the central
  area is pulled up into a broad, gently bumpy landing plateau. Raycasts over the central ±1.5 (x) × ±1.2 (z) area put the top surface at a **median of y +1.14 to +1.15**
  (max +1.20, 10th percentile +0.89 to +1.03; lower values are at the edge slopes). A witch placed at `c.top = y + 1.2` stands on the plateau, a few cm above the shallow dips.
- `cloud_diffuse.png`: moonlight white `#f2f6ff` on top, fading to `#cdd9f0` and then `#b4c6e7` on the underside, with 3 soft toon steps baked in from a top light.

## QA summary
- `_tools/verify_x.py` (an independent `.x` parser): `witch_inplace.x` gives **RESULT: OK**. It has 7,391 verts / 3,704 tris (flat-shaded split verts), the texture path is relative and the file exists,
  all 6 AnimationSets have 27 bone tracks each with consecutive key times, and root horizontal offset is 0 in every clip. Re-skinning the `.x` data by hand (frames × offsets × weights)
  matches Blender's deformed mesh to within **0.004 mm** at frames 0, ⅓, ⅔ and the end of every clip. Each `cloud_N.x` also gives **RESULT: OK**.
- `assimp info` (5.4.3) loads `witch_inplace.x`: 1 mesh, 26 bones, 6 animations, 162 channels, texture ref `witch_diffuse.png`. assimp flips Z when it converts to right-handed, so it reports the bristles at +Z; DxLib reads the file left-handed as written.
- `qa_witch.py`: transforms applied (loc/rot 0, scale 1), 0 loose verts, 0 zero-area faces, all 40 closed parts have outward normals (positive volume),
  0 unweighted verts, and the loop seams for idle, fly and fall are 0.000 mm.
- Polycount: witch 3,704 tris; clouds 1,000 / 1,050 / 1,100 tris.

## Known limitations / DxLib risks
- These files have **not been loaded in DxLib 3.24f** yet, only checked with the parser, assimp and Blender. They use the same exporter and layout as the Dance Raid `.x` files that are already loaded in-game.
- Facing: see the note under Conventions. The witch faces file +Z as requested, which is the opposite of the Dance Raid characters.
- Bind pose (no animation attached) shows the broom horizontal at her right side. Attach `idle` straight away.
- `fly`: the hands reach the stick through the hand mesh; the wrist sits about 3–5 cm short of the IK target because the arms are short. The thighs straddle the 0.05-radius stick with the skirt front swung up.
  Some skirt/thigh and sleeve/skirt intersection is possible at the extremes, which is normal for low-poly rigid cloth.
- Low-poly faceted toon look: flat shading, one 256² palette texture. The skirt lining and face decals are single-sided open shells, by design.
- The hat sways, so the top of the hat reaches Y 3.83–3.95 during `idle` (3.90 at rest).
