# CS2FOW Visibility Studio

Local-only legacy LOS Point Editor, baked-wall simulator, and first-person CS2FOW runtime-LOS preview. It opens directly as a compact desktop workspace: file and Preview/Edit/Play controls are in the top bar, while Points, Model, Weapon, Bounds, and Map share one inspector on the right.

The editor shows:

- local CT SAS and Phoenix GLB models
- optional USP-S, M4A1-S, and AWP previews attached to the right hand
- Valve's 19 animated player hitbox capsules, which drive the runtime-LOS preview
- 8 generated runtime AABB fallback points plus 15 editable legacy body points
- the runtime's separate dynamic weapon-muzzle point when a weapon is selected
- a second SAS model fixed 256 units away before a map loads, then placeable in Source 2 world coordinates
- a runtime preview where all 19 capsules follow the same named bones and local endpoints used by the plugin
- direct BVH8 loading with simple gray collision walls and the geometry-clear or smoke-test rays emitted by the browser visibility decision
- a 64 Hz first-person test range with map collision, a site-defending Phoenix bot, geometry-aware synthetic smoke, HE/bullet clearing, real local CS2 sounds and particle artwork, and Runtime-LOS/Debug visibility

Choose the standing, walking, running, crouching, jumping, firing, or reloading pose under Model. The list follows the selected weapon, uses matching exported SAS clips, and cross-fades between them. These clips preview capsule motion but are not a live CS2 bone snapshot with the engine's current aim layers and IK. Play keeps directional movement, draw, fire, reload, knife, and grenade actions on the third-person player instead of replacing them with a generic forward run; reload sounds follow the current CS2 clip events. Preview moves the runtime capsules, legacy comparison points, weapon, and muzzle with the SAS bones; Edit restores the standing layout and point controls. Play starts from the placed viewer and target and writes their final positions back when you leave it. The Studio starts in Edit when your system requests reduced motion.

The Map tab loads version 3 baked visibility files (BVH8) directly in a background worker. Local Mirage loads automatically. Ancient and other maps can be selected with **Load BVH8**. Recognized Ancient, Anubis, Inferno, Mirage, Nuke, and Overpass maps start with a default spawn pair; every map still supports click or numeric placement. Toggle the filled collision walls to wireframe when you need to see through the full map.

The worker mirrors the current raw-LOS decision order: reveal-hold reuse, up to six ping-scaled viewing origins, a direct chest-capsule probe, eight padded AABB-corner rays, a separate muzzle ray, then 96-leaf compactable front-to-back occluder proofs and a target-fitted 32x32 capsule depth buffer only when every cheap ray is blocked. Exact smoke rays are used for geometry-clear samples under a shared 75 ms fail-open budget. Transferred capsules and fallback points are aligned to the worker's current simulated actor pose before every query. The plugin runs the raster step through Intel MaskedOcclusionCulling while Studio uses a scalar JavaScript rasterizer with the same constants and conservative fail-open boundary, so sub-pixel edge results are representative rather than bit-identical. The Map tab exposes the plugin's shoulder, reveal-hold, and HE convar values; their defaults match the shipped configuration.

Play adds controlled smoke and HE inputs so you can exercise the runtime density and clearing rules. Smoke spreads only through connected baked-map space, bullets briefly carve narrow channels, and HE clears nearby cells for the configured duration. The browser uses real locally exported CS2 smoke and explosion artwork, but Three.js does not execute Valve's particle engine, so the generated cloud is test input rather than a copy of Valve's live density snapshot. Studio also does not pretend to benchmark the plugin's multi-thread scheduling or simulate lifecycle, stale-snapshot, full-update, quarantine, team, and visual-group transmit safeguards; those server-only safeguards can reveal a target but never make the browser hide one. Valve models, navigation, sounds, and particle artwork remain ignored local assets and must not be committed.

## Play controls

Click **Play**, then click the canvas to capture the mouse. Use `W/A/S/D` to move, `Shift` to walk silently, `Ctrl` to crouch, and `Space` to jump. Press `1` for your chosen M4A1-S/AWP primary, `2` for the USP-S, `3` for the Karambit, and `4` to switch between Smoke and HE grenades. Guns use their real magazine sizes and fire rates with unlimited reserve ammo, so `R` must finish a real reload before the clip refills. Hold and release left click for a full grenade throw; right click gives an underhand throw, AWP zoom, or Karambit heavy swing. `F` inspects, `J` switches first/third person, `V` switches between Runtime LOS and the full Debug view, and `C` independently shows or hides Debug rays. `Escape` releases the mouse and pauses the range.

The USP-S, M4A1-S, and AWP magazine, fire-rate, and movement values come from the [CS2 weapon spreadsheet](https://docs.google.com/spreadsheets/d/11tDzUNBq9zIX6_9Rel__fdAUezAQzSnh5AVYzCP060c/edit?gid=0#gid=0). Mouse input defaults to `m_yaw 0.022` and sensitivity `0.94`. Air movement uses Source-style directional acceleration with the 30-unit air wish cap and the Studio's existing 320-unit global speed limit.

The first-person camera renders the arms and weapon separately at `viewmodel_fov 44` with offsets `2.5 / 2 / -1`. Each weapon also has an editable first-person XYZ offset. Movement adds procedural CS-style viewmodel bob. Gunfire uses locally exported casing, muzzle, tracer, and concrete-impact artwork; the Play HUD uses the current CS2 weapon icons, bullet marks fade after two seconds, and no damage is added. Positional sounds stop at 1,250 units and fade toward that edge. The capsule/wall/smoke decision is the browser port of the runtime rules.

Runtime LOS mode hides the Phoenix and its carried weapon only when the chest probe, all eight AABB corners, the muzzle, and the animated capsule silhouette are proven blocked. Invalid input, sub-pixel uncertainty, or a spent worker budget reveals the player, matching the plugin's raw-LOS safety boundary under a stable transmit state. Positional footsteps stay audible. Debug mode keeps a translucent ghost and shows every bot's capsules, viewing origins, actual probe/geometry-clear/smoke/AABB rays, navigation route, synthetic smoke, and final wall/smoke reason; it does not invent point rays for depth-buffer occlusion.

## Export Local Assets

Install Node.js, Python, and the .NET 10 SDK. Install the Studio's locked local Three.js and glTF Transform dependencies once, then run the exporter:

```powershell
npm ci --prefix tools/visibility_point_editor
python tools/visibility_point_editor/export_assets.py --game "C:\Program Files (x86)\Steam\steamapps\common\Counter-Strike Global Offensive\game\csgo"
```

The exporter uses only that installed glTF Transform version to retain the complete CS2 world-animation library once, reuse it for SAS and Phoenix, discard unused player textures, and compact the result. Its small .NET helper is pinned to ValveResourceFormat `19.2.6339` and exports the installed maps' navigation graphs. Independent resources are exported four at a time, while asset and map fingerprints are tracked separately.

This writes ignored files under:

```text
tools/visibility_point_editor/local_assets/
```

Each compact player file is normally about 6 MB instead of the full export's roughly 328 MB. Source fingerprints keep unchanged local exports from being rebuilt.

The rendered weapon model is visual context; the purple muzzle marker uses the runtime's weapon-class length and stance adjustment. The SMG class exposes its 28-unit muzzle without inventing a local weapon model. Use the Weapon tab to choose a weapon and adjust its visual position, rotation, and scale. Use the Export menu to copy or download the unchanged legacy LOS JSON format.

## Run

```powershell
cd C:\path\to\CS2FOW
npm ci --prefix tools/visibility_point_editor
python -m http.server 8765
```

Open:

```text
http://127.0.0.1:8765/tools/visibility_point_editor/viewer.html
```

Serving the repository root lets the Studio reach `data/maps/de_mirage.bvh8` and its matching JSON report. If Mirage is not present, the rest of the editor stays available and **Load BVH8** still works.

The editor exports only the ordered body-point preset:

```json
{
	"version": 1,
	"coordinate_space": "source_local",
	"model": "ctm_sas",
	"point_count": 1,
	"points": [{"name": "head", "x": 0, "y": 0, "z": 64}]
}
```

Copy and download reject blank or duplicate names, non-finite coordinates, and presets outside the 1-32 point limit. The final point cannot be deleted.

The exported body-point preset remains a comparison and editing artifact. Current CS2FOW runtime decisions consume the fixed 19-entry Valve capsule binding table, the separate muzzle sample, and the eight generated AABB corners; the AABB points are not exported. `node tools/visibility_point_editor/check_bvh8.mjs` checks the browser BVH8 parser and tracer with a small synthetic map.

`node tools/visibility_point_editor/check_fps.mjs` checks the movement constants, capsule collision, jumping, navigation, smoke, HE timing, malformed-input fail-open behavior, wall occlusion, and the current target-fitted capsule layout. The baker's optional `--studio-surfaces <file>` output binds compact per-triangle surface IDs to one exact BVH8 payload. When that sidecar is missing or rejected, Play safely uses concrete footsteps.
