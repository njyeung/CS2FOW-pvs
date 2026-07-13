<div align="center">

# CS2FOW

### Server-side anti-wallhack for Counter-Strike 2 community servers

[![Version](https://img.shields.io/github/v/release/karola3vax/CS2FOW?style=for-the-badge&label=version)](https://github.com/karola3vax/CS2FOW/releases/latest) [![Downloads](https://img.shields.io/github/downloads/karola3vax/CS2FOW/total?style=for-the-badge&label=downloads)](https://github.com/karola3vax/CS2FOW/releases) [![Issues](https://img.shields.io/github/issues/karola3vax/CS2FOW?style=for-the-badge&label=issues)](https://github.com/karola3vax/CS2FOW/issues) [![License](https://img.shields.io/github/license/karola3vax/CS2FOW?style=for-the-badge&label=license)](LICENSE)

<img src="docs/ancient.gif" width="800" alt="CS2FOW hiding players behind solid map geometry on Ancient">

</div>

CS2FOW stops a server from sending the live visual entities of a fully occluded player to an opponent. A wallhack cannot draw an exact position the client never received.

- **Server-side:** CS2FOW runs on the community server, not on players' computers.
- **Zero client setup:** players install nothing and the plugin does not inject into CS2 clients.
- **Fail open:** missing, stale, or unsafe data makes CS2FOW show players normally instead of hiding them incorrectly.

## Across different maps

Each map uses its own baked static collision for visibility checks.

<table>
<tr>
<td width="50%" align="center">
<img src="docs/cache.gif" width="100%" alt="CS2FOW operating on Cache"><br>
<strong>Cache</strong>
</td>
<td width="50%" align="center">
<img src="docs/dust2b.gif" width="100%" alt="CS2FOW operating around the Dust II B site"><br>
<strong>Dust II — B site</strong>
</td>
</tr>
<tr>
<td colspan="2" align="center">
<img src="docs/dust2long.gif" width="72%" alt="CS2FOW operating across Dust II long sightlines"><br>
<strong>Dust II — Long</strong>
</td>
</tr>
</table>

## FAQ

<details>
<summary><strong>What is CS2FOW?</strong></summary>

CS2FOW is a server-side anti-wallhack plugin for Counter-Strike 2 community servers. When solid map walls or live smoke completely block a living player, the server can stop sending that player's visual entities to one opponent.

It is not a screen filter and does not run on the player's computer.

</details>

<details>
<summary><strong>Does it work in Premier or Valve matchmaking?</strong></summary>

No. It requires a community or dedicated server running Metamod:Source. Valve would need to add a similar system for official matchmaking.

</details>

<details>
<summary><strong>Do players install anything or risk a VAC ban?</strong></summary>

Players install nothing. CS2FOW runs only on the server and does not modify or inject into the CS2 client.

</details>

<details>
<summary><strong>Can a cheat bypass it?</strong></summary>

A cheat cannot recover an exact live enemy position that the server never sent. It can still use sound, teammate information, last-known positions, common prefire spots, and other game clues. CS2FOW removes the main wallhack data source; it does not make all cheating impossible.

</details>

<details>
<summary><strong>What exactly gets hidden?</strong></summary>

CS2FOW groups only a living player's pawn, known carried weapons, wearables, and currently carried hostage prop. Unknown and independent gameplay entities stay visible.

Self, dead players, spectators, and HLTV remain unfiltered. Teammates remain unfiltered by default; an optional setting applies the same gate to living teammates.

</details>

<details>
<summary><strong>Can players still wallbang a hidden enemy?</strong></summary>

Yes. The player remains fully present on the server. Movement, hit registration, penetration, damage, and game rules continue normally.

</details>

<details>
<summary><strong>Does it block radar cheats or sound ESP?</strong></summary>

It reduces radar cheats that depend on live enemy entity positions. It does not remove footsteps, gunshots, teammate information, bomb information, or every other clue.

</details>

<details>
<summary><strong>What about smokes, doors, breakables, and moving props?</strong></summary>

Smokes are visibility blockers. CS2FOW copies CS2's live smoke voxel grid and checks wall-clear rays against it, including changing edges, overlap, growth, fade, and grenade-made holes.

Doors, breakables, and moving props are not blockers in the current preview because baked map data contains static geometry only.

</details>

<details>
<summary><strong>How does it avoid enemies appearing too late around corners?</strong></summary>

The worker checks body points, bounding-box corners, and the held weapon muzzle. Movement prediction starts gradually above 75 units per second and becomes fully active at 100. It looks ahead using movement and recipient latency, while shoulder origins widen with latency to reduce high-ping corner pop-in. A short visibility hold reduces one-tick flicker.

When the worker sees a player again, CS2FOW stops removing that player from ordinary snapshots immediately.

</details>

<details>
<summary><strong>Does it run expensive engine traces every tick?</strong></summary>

No. Map collision is baked into a compact BVH8 file. A background worker casts rays against that data, while `CheckTransmit` only reads the finished answer.

</details>

<details>
<summary><strong>Do custom and Workshop maps work?</strong></summary>

Yes, when their physics can be baked. The server can bake a mounted map automatically, and the [CS2FOW Map Baker](https://cs2fow-bake-service.onrender.com/) can prepare downloadable `.bvh8` and `.json` files from a public Workshop item.

</details>

<details>
<summary><strong>What happens when a map changes?</strong></summary>

CS2FOW compares the current source CRC and size with the stored bake. A mismatched or outdated bake is rejected, players remain visible, and an automatic rebake can create current data.

</details>

<details>
<summary><strong>What does "fail open" mean?</strong></summary>

If CS2FOW is missing information or is not sure filtering is safe, it sends players normally instead of hiding them.

</details>

## Quickstart

1. Install [Metamod:Source](https://www.sourcemm.net/) on the CS2 server.
2. Download the Windows or Linux `0.2.1-preview` core ZIP from the [releases page](https://github.com/karola3vax/CS2FOW/releases).
3. Extract the ZIP into the server's `game/csgo` folder. Keep the folders inside the ZIP unchanged.
4. Start the server and load a map.
5. Run `cs2fow_status` in the server console.

The first load may report that an automatic bake is in progress. The server remains playable and everyone stays visible until the bake finishes and validates. The optional official-maps ZIP avoids this first-load wait for its included maps.

CS2FOW requires an x86-64 CPU with AVX exposed to the operating system. It also verifies that its private gamedata matches the loaded CS2 server binary by file size and CRC32. After an unknown Valve update, CS2FOW stays disabled until compatible gamedata is installed.

## The protection boundary

CS2FOW hides a deliberately small visual group:

- the player pawn;
- active, last, and carried weapons, including carried C4;
- wearables;
- a currently carried hostage prop.

It deliberately leaves independent gameplay entities alone. Planted C4, dropped objectives, dropped weapons, grenade projectiles, infernos, sounds, and unknown entities never disappear merely because their former owner is hidden. Movement, collision, hit registration, damage, penetration, and game rules remain server-authoritative and unchanged.

HLTV, spectators, dead players, and a player viewing themself are not filtered. Teammates remain visible by default; `cs2fow_filter_teammates 1` applies the same visibility gate to living teammates and may also remove their client-side markers and radar information.

Live smoke blocks visibility rays. An HE detonation opens a 100-unit viewing channel through affected smoke for 2.5 seconds by default, but only when the smoke already existed when the HE detonated. Walls and overlapping smoke clouds still block independently.

## How it works

1. **Load the map:** identify the mounted VPK and its physics data.
2. **Bake static walls:** turn collision triangles into a compact eight-child bounding volume hierarchy (BVH8).
3. **Capture players:** safely copy positions, bounds, movement, view direction, latency, and held-weapon class on the game thread.
4. **Cast rays:** a background worker checks eight recipient origins against current and predicted body, bounds, and muzzle points.
5. **Decide visibility:** one ray clear of both baked walls and live smoke reveals the player.
6. **Filter the snapshot:** `CheckTransmit` marks the verified `dont_transmit` bit, then removes the matching primary send bit for each hidden visual entity.

The worker receives copied numbers only. It never dereferences live CS2 objects.

### Baked map geometry

<table>
<tr>
<td width="52%">
<p>The baker reduces a map's static collision to the geometry CS2FOW actually needs for line-of-sight checks. The plugin validates the BVH8 structure, source CRC, source size, and report metadata before trusting it.</p>
<p>Mounted maps can bake automatically. Public Workshop maps can also be prepared through the <a href="https://cs2fow-bake-service.onrender.com/">CS2FOW Map Baker</a>.</p>
</td>
<td width="48%" align="center">
<img src="docs/scan_cbbl.png" width="100%" alt="Static cobblestone collision mesh used by CS2FOW">
</td>
</tr>
</table>

## Operating the server

The plugin executes `cfg/cs2fow.cfg` after registering its convars and again at every map start. The packaged defaults enable wall and smoke filtering, leave teammate filtering off, and use Valve's `sv_enable_donttransmit 0` compatibility mode. Paired transmit-list handling also supports mode `1`.

`cs2fow_status` reports activation, fail-open reasons, map and bake identity, worker, snapshot, and transmit timings, snapshot age, pair counts, smoke and HE state, teammate filtering, and automatic-bake progress.

For entity-level evidence:

```text
cs2fow_debug 1              start silent evidence collection
cs2fow_entity               list buffered records, newest first
cs2fow_entity <edict>       show records for one entity index
cs2fow_entity clear         clear the evidence buffer
```

Debug records are created only for primary bits CS2FOW actually removes. Turning debugging off stops collection but preserves existing evidence until a reset or explicit clear.

<details>
<summary><strong>Complete configuration reference</strong></summary>

| Setting | Default | Meaning |
| --- | ---: | --- |
| `sv_enable_donttransmit` | `0` | Use Valve's compatibility mode. CS2FOW's paired-list handling also supports mode `1`. |
| `cs2fow_enable` | `1` | Enable filtering when all required data is valid. |
| `cs2fow_smoke_occlusion` | `1` | Use CS2's live smoke grid. Smoke alone fails open if private smoke data is unavailable. |
| `cs2fow_he_clear_radius_units` | `100` | Radius of the temporary viewing channel created by an HE. Set to `0` to disable HE clearing. |
| `cs2fow_he_clear_seconds` | `2.5` | Seconds before an HE-created viewing channel closes. Set to `0` to disable HE clearing. |
| `cs2fow_filter_teammates` | `0` | Apply the same visibility filtering to living teammates. |
| `cs2fow_update_interval_ms` | `1` | Minimum time between player snapshots sent to the worker. |
| `cs2fow_base_lookahead_ms` | `75` | Fixed movement lookahead before the RTT contribution. |
| `cs2fow_rtt_lookahead_scale` | `1.5` | Multiplier applied to the recipient's current round-trip latency. |
| `cs2fow_max_lookahead_ms` | `375` | Maximum movement and latency lookahead. Set to `0` to disable movement prediction. |
| `cs2fow_max_prediction_units` | `96` | Maximum predicted movement for each player. Set to `0` to disable movement prediction. |
| `cs2fow_shoulder_base_units` | `24` | Minimum left and right shoulder-origin distance. |
| `cs2fow_shoulder_rtt_scale` | `0.64` | Shoulder units added per millisecond of recipient RTT. |
| `cs2fow_max_shoulder_units` | `128` | Maximum left and right shoulder-origin distance. |
| `cs2fow_visibility_hold_ms` | `16` | Minimum time a newly visible pair stays visible. |
| `cs2fow_debug` | `0` | Collect evidence for entity bits actually hidden by CS2FOW. |

Existing custom configs must add `sv_enable_donttransmit 0` and the two HE-clearance controls. They must update the former `50`/`150` lookahead defaults, add the RTT, prediction-distance, and shoulder controls, replace `cs2fow_min_lookahead_ms` with `cs2fow_base_lookahead_ms`, and remove `cs2fow_peek_margin_units`.

Automatic baking needs write access to `addons/cs2fow/data/maps`. On Linux, the packaged baker and VRF program must remain executable.

</details>

## Honest limits

- Static baked geometry and live smoke are occluders. Doors, breakables, moving props, particles, projectiles, and other moving objects are not.
- Prediction is tuned for normal competitive and casual movement. Surf, KZ, and unusually fast boosts can reveal late.
- Larger latency compensation reduces corner pop-in by revealing players farther around corners, including while the recipient is stationary.
- Sound, bomb information, teammate information, last-known positions, and other non-entity clues remain available.
- Unknown CS2 server binaries disable the plugin instead of allowing unsafe private reads.
- Full-update snapshots remain untouched. `CheckTransmit` changes neither transmit list if either required pointer is unavailable.
- Builds and unit tests cannot reproduce a live CS2 transmit list or prove that an engine entity-copy crash is impossible. Live-server validation remains separate.

<details>
<summary><strong>Troubleshooting</strong></summary>

**`cs2fow_status` says AVX is missing:** check that the host exposes AVX and operating-system AVX state to the virtual machine. Physical CPU support is not enough if the guest cannot see it.

**Automatic bake says permission denied on Linux:** restore execute permission and check mount or container policy:

```sh
chmod +x game/csgo/tools/cs2fow_baker
chmod +x game/csgo/tools/vrf/linux64/Source2Viewer-CLI
```

Automatic-baker and VRF failures include the newest 8 KiB of combined output. Cancellation and timeout terminate the complete child process tree on both platforms.

**The server binary does not match:** install a CS2FOW build with gamedata verified for the current Valve server binary. There is no unsafe override.

**A bake is rejected after a CS2 update:** this is expected when Valve changes the source VPK. CS2FOW compares stored and mounted source identity and rebakes instead of trusting stale walls.

**A bug needs investigation:** include the CS2FOW version, operating system, map, `cs2fow_status` output, nearby server logs, player state, and a short clip for visibility or pop-in issues.

</details>

## Developer and project links

- [Releases](https://github.com/karola3vax/CS2FOW/releases): Windows, Linux, and official-map packages.
- [Code tour](docs/CODE_TOUR.md): plain-language architecture, threads, safety rules, and build and release steps.
- [Visibility Studio](tools/visibility_point_editor/README.md): view and tune body, bounds, and muzzle samples.
- [CS2FOW Map Baker](https://cs2fow-bake-service.onrender.com/): prepare visibility data from a public Workshop map.
- [Bake Service source](https://github.com/karola3vax/CS2FOW-Bake-Service): bounded public baking service.

Manual baker examples:

```text
cs2fow_baker --game <cs2-root> --map de_dust2 --output de_dust2.bvh8
cs2fow_baker --list-maps --vpk <outer_dir.vpk>
cs2fow_baker --inspect-bvh8 <file>
cs2fow_baker --game <cs2-root> --map workshop/123/de_example --vpk <outer_dir.vpk> --output de_example.bvh8
```

Generated map data is derived from Counter-Strike 2 game data and is covered by `DATA_NOTICE`, not the project's MIT code license. Core packages include the exact cgltf, ValveResourceFormat 19.2, native-library, and self-contained .NET notices under `licenses/`. See [LICENSE](LICENSE), [THIRD_PARTY_NOTICES](THIRD_PARTY_NOTICES), and [DATA_NOTICE](DATA_NOTICE).
