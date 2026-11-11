# AtlasGBA

AtlasGBA is a procedural exploration game built on Pokémon Emerald Expansion.
New worlds use broad continental cores and an Earth-like macro land/ocean balance;
Generation 6 adds latitude wind belts, cold-ocean vapor supply and rain shadows;
Generation 7 adds complete forest groves on suitable dry, flat ground;
Generation 8 adds lower continental interiors and curved uplift belts tied to
continental margins, with sea-level atmospheric exposure at coasts.
Generation 9 shapes dry riverbank floors and supplies local soil moisture from
rivers and lake-relative shorelines; inland low ground no longer implies a beach.
Generation 10 applies bounded runoff-driven regional erosion, then rebuilds
climate and drainage from the eroded heights.
Generation 11 shapes continental shelves, ocean slopes, abyssal basins and broad
submerged ridge belts, with coastal river erosion referenced to sea level.
Generation 12 warps continental outlines and their uplift frames together, and
reserves part of the same land budget for sparse oceanic islands and chains.
Generation 13 adds rare dormant volcanic cones, raised crater rims and lower
caldera floors, with native cliff terraces and a walkable radial gap.
Generation 14 adds exposed native rock ground in volcanic interiors and high
uplands, with matching cliff material and snow priority.
Generation 15 flares selected lowland river outlets toward their fitted sea
intersection, with a dedicated estuary bank inspection target.
Generation 16 exposes connected small-channel drainage down to coastal outlets,
with safe small-river inspection and corresponding atlas overlays.
Generation 17 scales dry floodplain incision and river-fed soil moisture with
channel width, with named floodplain inspection and a close soil-moisture atlas.
Generation 18 refines suitable inland reaches with deterministic, terrain-checked
local tributary creeks, creekbank names and developer inspection.
Generation 19 adds warm, dry dune fields with shared height sampling, walkable
native sand ridges, physical naming and developer inspection.
Generation 20 adds rare dry-country mesas with flat caps, warped shoulders,
native rock contours, walkable approaches and developer inspection.
Generation 21 adds smaller terrain-checked headwater tributaries feeding local
creeks, with stable joins, physical naming and developer inspection.
Generation 22 adds raised coastal uplands and seaward cliff faces, with smooth
ocean influence, native rock/snow walls and climate-appropriate ground cover.
Generation 23 adds rare wet-upland karst depressions, shared lowered floors,
native inward-facing contours and walkable approaches.
Generation 24 adds lake-safe channel incision and connected bedrock canyons,
with subdued floors, steep walls and yielding geographic inspection.
Generation 25 shapes humid lowland riverbanks into raised natural levees and
lower, wetter backswamps, with native marsh cover and geographic inspection.
Generation 26 adds terrain-checked coastal distributary channels beside suitable
lowland river mouths, retaining their parent channel and flared estuary.
Generation 27 adds screen-scale lowland hills, shared physical height variation,
native slopes with southern ramps, and complete trees on safe raised hilltops.
Existing generation-3 through generation-26 worlds keep their original geography and walkability.
This repository is the development home: **ArmenionCH2/pokeemerald-expansion**.
The full game vision is [FULL_PLAN.md](docs/atlas/FULL_PLAN.md). It includes goals
beyond the current implementation; a successful build does not mean that vision
is complete.

Start with [AGENTS.md](AGENTS.md) and the [development guide](docs/atlas/DEVELOPMENT.md).
The live Atlas implementation is `src/atlas_*.c` and `include/atlas_*.h`, with small
integration hooks in Expansion's existing files. [Recovery provenance](migration/atlas/README.md)
preserves the exact original work without a second engine checkout in this repo.

```sh
python3 dev_scripts/atlas.py setup
python3 dev_scripts/atlas.py verify
python3 dev_scripts/test_atlas.py
make -j4
```

See [INSTALL.md](INSTALL.md) for native compiler dependencies and platform setup.
Build tools are compiled by Make. Use this fork when cloning, regardless of URLs
in inherited engine documentation. `make release -j4` omits Atlas developer map
access and geographic teleport tools. Neither target produces an official game.

In Atlas: START opens the menu; WORLD MAP uses D-pad to pan, L/R to zoom,
A to center on the player and B to return. Unknown terrain stays black. Development
builds allow SELECT to toggle the admin map and START to cycle terrain, elevation,
temperature, humidity, soil moisture, drainage, weather-front, regional-relief and local-relief layers. GEO TP
in the start menu can find landforms, waters and forest/desert/tundra/wetland
locations, plus regional plateaus. Its fourth page also finds a sand/grass boundary
for inspecting native ground edges, islands for checking small landmasses, and
dunes for checking actual sand relief. The fifth page locates rounded rock/grass
and snow/grass ground borders, mesas, headwater creeks, sea cliffs and rounded rock/sand boundaries. The border artwork preserves terrain
heights and walking collision. Mesas require generation 20 or newer; headwaters require 21 and sea cliffs 22. A sixth page finds karst sinkholes in generation 23 and rounded rock/snow boundaries. Its River canyon entry requires generation 24; Backswamp and Natural levee entries require generation 25.
The sixth page also finds Coastal distributary channels in generation 26 or later worlds.
A seventh page finds flowering clearings, including separate purple/gold targets, cold sea ice and diagonal sand/grass and rock/grass inner corners. Page eight finds snow/grass inner corners, lowland hills and wooded hills. Hills require a new generation-27 world. L/R jumps between inspector pages.
The volcano entry finds dormant cones in generation 13 worlds. Physical location breadcrumbs name the landmass,
catchment and terrain. Rivers follow stable mainstem/tributary identities from the drainage graph.
Connected equal-level lakes, individual mesas, sinkholes and volcanic cone/caldera complexes
have seed-stable landmark names; regional plateau/valley/ridge descriptors use surrounding relief.
Groves mix two native broadleaf crowns while retaining their tree footprints.
Warm, moist forest clearings also contain walkable native leafy undergrowth.
Warm clearings also vary flower colors with soil moisture: humid sites can have
purple blossoms and moderately dry sites gold. The color variants animate with native
flower frames; mirrored plants move half a cycle apart. These visual updates work on existing saves without regenerating terrain.
The exploration journal has a fixed save capacity and warns when space is low.
Use a fresh save for this port; back up saves from other engines/versions.

Engine documentation remains in `docs/`, [FEATURES.md](FEATURES.md),
[INSTALL.md](INSTALL.md) and [CREDITS.md](CREDITS.md). Expansion is based on
[pret/pokeemerald](https://github.com/pret/pokeemerald) and
[rh-hideout/pokeemerald-expansion](https://github.com/rh-hideout/pokeemerald-expansion).
Both are read-only sources for this project. Atlas terrain art is compiled from the original Emerald assets already present in this
Expansion checkout. See [native asset provenance](assets/atlas-native/SOURCE.md).
Original Pokémon artwork is not open-source art; existing engine/game credits apply.
