# AtlasGBA

AtlasGBA is a procedural exploration game built on Pokémon Emerald Expansion.
New worlds use broad continental cores and an Earth-like macro land/ocean balance;
Generation 6 adds latitude wind belts, cold-ocean vapor supply and rain shadows;
Generation 7 adds complete forest groves on suitable dry, flat ground;
Generation 8 adds lower continental interiors and curved uplift belts tied to
continental margins, with sea-level atmospheric exposure at coasts.
Existing generation-3 through generation-7 worlds keep their original geography and walkability.
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
temperature, humidity, soil moisture, drainage, weather-front and regional-relief layers. GEO TP
in the start menu can find landforms, waters and forest/desert/tundra/wetland
locations, plus regional plateaus. Its fourth page also finds a sand/grass boundary
for inspecting native ground edges. Physical location breadcrumbs name the landmass,
catchment and terrain; regional plateau/valley/ridge descriptors use surrounding relief.
The exploration journal has a fixed save capacity and warns when space is low.
Use a fresh save for this port; back up saves from other engines/versions.

Engine documentation remains in `docs/`, [FEATURES.md](FEATURES.md),
[INSTALL.md](INSTALL.md) and [CREDITS.md](CREDITS.md). Expansion is based on
[pret/pokeemerald](https://github.com/pret/pokeemerald) and
[rh-hideout/pokeemerald-expansion](https://github.com/rh-hideout/pokeemerald-expansion).
Both are read-only sources for this project. Atlas terrain art is compiled from the original Emerald assets already present in this
Expansion checkout. See [native asset provenance](assets/atlas-native/SOURCE.md).
Original Pokémon artwork is not open-source art; existing engine/game credits apply.
