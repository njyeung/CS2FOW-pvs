<div align="center">

# CS2FOW

### Server-side anti-wallhack for Counter-Strike 2 community servers

[![Version](https://img.shields.io/github/v/release/karola3vax/CS2FOW?style=for-the-badge&label=version)](https://github.com/karola3vax/CS2FOW/releases/latest)
[![Downloads](https://img.shields.io/github/downloads/karola3vax/CS2FOW/total?style=for-the-badge&label=downloads)](https://github.com/karola3vax/CS2FOW/releases)
[![Issues](https://img.shields.io/github/issues/karola3vax/CS2FOW?style=for-the-badge&label=issues)](https://github.com/karola3vax/CS2FOW/issues)
[![License](https://img.shields.io/github/license/karola3vax/CS2FOW?style=for-the-badge&label=license)](LICENSE)

![CS2FOW demonstration](docs/CS2FOW.gif)

</div>

CS2FOW is a server-side anti-wallhack plugin for Counter-Strike 2 community servers. It withholds a living enemy's visual entities when solid map walls fully block that enemy from a living opponent.

## Quickstart

1. Install [Metamod:Source](https://www.sourcemm.net/) on the CS2 server.
2. Download the Windows or Linux `0.2.0-preview` core ZIP from the [CS2FOW releases page](https://github.com/karola3vax/CS2FOW/releases).
3. Extract the ZIP into the server's `game/csgo` folder. Keep the folders inside the ZIP unchanged.
4. Start the server and load a map.
5. Run `cs2fow_status` in the server console.

The first load may say that an automatic bake is in progress. The server remains playable and CS2FOW shows everyone normally until that bake finishes and validates. The optional official-maps ZIP avoids this first-load wait for the maps included in it.

CS2FOW requires an x86-64 CPU with AVX support exposed to the operating system. Some virtual-server providers hide AVX even when the physical CPU supports it.

## What CS2FOW does

CS2FOW runs only on the server. Players install nothing.

When a living enemy is fully behind solid map geometry, CS2FOW may remove that enemy's current visual group from the primary entity-send list for one recipient. The visual group includes the pawn, weapons, wearables, a carried hostage prop, and directly linked owner/effect entities found by the plugin. The enemy still exists on the server, so movement, hit registration, damage, wall penetration, and game rules continue normally.

This removes the main live position data used by wallhacks. It does not make every form of cheating impossible: sound, teammate information, last-known positions, and other game clues still exist.

HLTV, spectators, dead players, teammates, and a player viewing themself are not filtered.

## The six-step runtime flow

1. **Load map:** find the mounted map VPK and identify its physics data.
2. **Bake walls:** turn static collision triangles into a compact bounding volume hierarchy with eight children per node (BVH8).
3. **Collect player points:** copy each living player's position, movement, body bounds, eye direction, latency, and held-weapon class on the game thread.
4. **Cast rays:** a background worker checks ten safe recipient points against up to forty target points. Targets include eight axis-aligned bounding box corners, fifteen custom body points, and a weapon-muzzle point, with current and predicted positions when movement prediction is safe.
5. **Decide visibility:** one open ray reveals the target. A short hold keeps a recently revealed target visible to reduce corner pop-in.
6. **Withhold hidden entities:** `CheckTransmit` reads the finished decision and clears only the verified primary transmit-list bits for a hidden enemy's visual group.

The worker receives copied numbers only. It never reads live CS2 objects.

## Installation and configuration

The core ZIP installs these operator-facing paths:

```text
addons/cs2fow/bin/
addons/cs2fow/gamedata/cs2fow.games.txt
addons/metamod/cs2fow.vdf
cfg/cs2fow.cfg
tools/cs2fow_baker
tools/vrf/
```

Defaults in `cfg/cs2fow.cfg` are:

| Setting | Default | Meaning |
| --- | ---: | --- |
| `cs2fow_enable` | `1` | Enable filtering when all required data is valid. |
| `cs2fow_update_interval_ms` | `1` | Minimum time between player snapshots sent to the worker. |
| `cs2fow_max_lookahead_ms` | `500` | Maximum movement/latency lookahead. Set to `0` to disable lookahead. |
| `cs2fow_min_lookahead_ms` | `200` | Early-reveal lookahead before latency is added. |
| `cs2fow_peek_margin_units` | `160` | Maximum speed-based early-reveal distance in Source units. |
| `cs2fow_visibility_hold_ms` | `50` | Minimum time a newly visible pair stays visible. |
| `cs2fow_debug` | `0` | Collect real primary-list clears for later inspection. |

Automatic baking needs write access to `addons/cs2fow/data/maps`. On Linux, the packaged baker and VRF program must remain executable.

## Status and debug commands

`cs2fow_status` prints whether the plugin is active, why it is fail open when inactive, map and bake details, worker timings, result age, pair counts, and automatic-bake progress.

`cs2fow_debug 1` starts silent evidence collection. It adds a record only when CS2FOW found a primary transmit bit set immediately before clearing it. It does not print every clear.

Use:

```text
cs2fow_entity                 list buffered records, newest first
cs2fow_entity <edict>         show records for one entity index
cs2fow_entity clear           clear the evidence buffer
```

Records show the classname, relationship to the pawn (`direct`, `owner_link`, `effect_link`, or `owner+effect`), handles, recipients, reasons, count, and age. Turning debugging off stops new records but keeps existing ones until a map/plugin reset or `clear`.

## Fail-open behavior and known limits

“Fail open” means CS2FOW sends entities normally whenever it is unsure. Examples include missing or stale worker results, missing map data, a corrupt or outdated bake, changed map CRC/size, invalid live player state, a missing visual-group member, unavailable schema data, and unsupported CPU features.

Important limits:

- Visibility uses baked static map geometry. Smokes, doors, breakables, props, particles, projectiles, and other moving blockers are not occluders.
- Sound events, bomb information, teammate information, and other non-entity clues are not filtered.
- Version 2 and older BVH8 files are rejected. The plugin automatically rebakes when it can and remains fail open otherwise.
- After CS2FOW hides a visual group, it keeps that group hidden until CS2 naturally gives that recipient a full update. CS2FOW never requests a full update, so a newly visible enemy can remain hidden longer than the ray decision alone would suggest.
- `CheckTransmit` changes only `m_pTransmitEntity`, the verified primary send list. Full-update snapshots are never filtered.
- Builds and unit tests cannot reproduce a live CS2 transmit list or prove that a server will never hit a game-engine entity-copy crash. Live-server packet testing is a separate validation step.

## Troubleshooting

**`cs2fow_status` says AVX is missing:** check that the host exposes AVX and operating-system AVX state to the virtual machine. A physical CPU supporting AVX is not enough if the guest cannot see it.

**Automatic bake says permission denied on Linux:** restore execute permission and check mount/container policy:

```sh
chmod +x game/csgo/tools/cs2fow_baker
chmod +x game/csgo/tools/vrf/linux64/Source2Viewer-CLI
```

**A bake is rejected after a CS2 update:** this is expected when Valve changes the source VPK. CS2FOW compares the stored source CRC and size to the mounted map and rebakes instead of trusting stale walls.

**A bug needs investigation:** include the CS2FOW version, operating system, map, `cs2fow_status` output, nearby server logs, player state (alive/dead/spectating/bot), and a short clip for visibility or pop-in issues.

## Developer links

- [Code tour](docs/CODE_TOUR.md): plain-language architecture, threads, safety rules, and build/release steps.
- [Visibility point editor](tools/visibility_point_editor/README.md): view and tune the body, bounds, and muzzle samples.
- [Bake Service](https://github.com/karola3vax/CS2FOW-Bake-Service): bounded public Workshop-map baking service.

Manual baker examples:

```text
cs2fow_baker --game <cs2-root> --map de_dust2 --output de_dust2.bvh8
cs2fow_baker --list-maps --vpk <outer_dir.vpk>
cs2fow_baker --game <cs2-root> --map workshop/123/de_example --vpk <outer_dir.vpk> --output de_example.bvh8
```

Generated map data is derived from Counter-Strike 2 game data and is covered by `DATA_NOTICE`, not the project's MIT code license. See `LICENSE`, `THIRD_PARTY_NOTICES`, and `DATA_NOTICE`.
