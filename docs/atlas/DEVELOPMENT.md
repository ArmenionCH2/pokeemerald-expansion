# Development

## Layout and authority

Keep the Expansion build layout. `FULL_PLAN.md` is the owner's game vision,
including explicitly undecided mechanics. `src/` is the implementation authority;
`tests/atlas/` contains executable host contracts; `test/` is Expansion's native
battle test framework. Do not describe a host mock as emulator evidence.

| Task | Entry points |
| --- | --- |
| Physical location names and boundary announcements | `src/atlas_place.c`, `src/atlas_location.c` |
| Contextual regional relief | `src/atlas_region.c`, `src/atlas_map.c`, `src/atlas_geo.c` |
| World seed, position, startup | `src/atlas_world.c`, `src/atlas_creation.c` |
| Regional weather and atlas layers | `src/atlas_weather.c`, `src/atlas_map.c` |
| Geography, hydrology, habitat | `src/atlas_plan.c`, `src/atlas_terrain.c`, `src/atlas_water.c`, `src/atlas_habitat.c` |
| Streaming, rendering, art | `src/atlas_stream.c`, `src/atlas_tiles.c`, `src/atlas_tiles_data.c`, `assets/atlas-native/` |
| Exploration journal and map | `src/atlas_state.c`, `src/atlas_map.c`, `include/atlas_state.h` |
| Wildlife selection and ownership | `src/atlas_wildlife.c`, `src/atlas_pokemon.c`, `src/atlas_actor_*.c` |
| Engine integration | `src/fieldmap.c`, `src/overworld.c`, `src/event_object_movement.c`, `src/main_menu.c`, `src/new_game.c`, `src/start_menu.c` |
| Original changes and exact hashes | `migration/atlas/` |

Use `rg` to follow callers. A callable subsystem is not necessarily wired into
play: the wildlife loop wires up to three habitat-selected, wandering native
actors; identity/admission tests do not demonstrate persistent populations,
autonomous ecology or relationships. No implementation-status mirror is maintained.

## Build and verify

On Debian/Ubuntu, install `build-essential binutils-arm-none-eabi gcc-arm-none-eabi
libnewlib-arm-none-eabi libpng-dev python3` using your environment's package manager.
Host tests need a C compiler with AddressSanitizer and UBSan. The native asset
regeneration contract also needs Pillow (`python3-pil`, or a local virtual environment). Emulator checks
use `libmgba-dev`. No private repository credential is required for normal development.

Run the commands in README. For a built development ROM, run
`python3 dev_scripts/test_atlas_emulator.py` for key-driven mGBA checks of world
entry, map isolation and save/reset/Continue; screenshots stay in `build/atlas-emulator/`.
 `make` builds the Emerald target; other Expansion game
targets are retained engine code, not supported Atlas releases. `make release`
enables the engine's release/LTO mode. Build artifacts stay in `build/` or the
engine's existing ignored output paths. CI checks; it never commits or publishes ROMs.

Preserved tests exercise real Atlas C with host mocks. `dev_scripts/test_atlas.py`
compiles separate cases with sanitizers. New regressions should protect behavior
or data integrity, not mirror source formatting. Use `git diff --check` before commit.

Regenerate art with `python3 dev_scripts/compile_atlas_native.py`; review the C,
header and manifest diff. The PNG preview and detailed role index go to `build/atlas-native/`. Preserve original native inputs and credits. Do not add arbitrary art from other ROM hacks.

## Compatibility contracts

- Start from Expansion commit `d7ee392a2ca13fb708258940614a48b32a90992c`.
  The original Atlas engine was a different pret/pokeemerald revision. Do not
  copy its allocator, linker script, sprite engine or save code over Expansion.
- World generation version and save-journal version are separate identities.
  New worlds retain version 5's five seeded, periodic continental cores with varying
  aspect ratios, slanted axes and irregular coast detail. This morphological model
  replaces fragmented short-wavelength outlines; it is not a tectonic simulation.
  Version 6 adds symmetric trade/westerly/polar wind belts, latitude-dependent ocean
  vapor supply, inland vapor depletion, windward precipitation and lee drying, plus
  smooth equatorial/subtropical/storm-belt rainfall envelopes. Climate is ordinal,
  not measured mm/year; there is no ocean-current or numerical atmosphere solver.
  Versions 4 onward normalize macro height to approximately 29.2% land before mountain
  uplift. Generation 7 adds vegetation collision, retaining generation 6's macro
  height/climate/drainage rules. Existing versions 3 through 6 retain their original
  algorithms, checksums and walkability. Never retag an existing world to add trees
  or upgrade terrain.
  Version 4 also labels connected ocean/sea components using a tagged existing
  node field, without increasing permanent plan storage.
  Restore validates them and the seed. Unknown Atlas formats fail closed.
  Saves from the old engine still need a fresh world. This port's version-1
  journal upgrades losslessly in memory on Continue; it is written on the next Save.
- The original optional recipe cache wrote sectors 28–31. Expansion owns these
  sectors and its per-sector SaveBlock3 extension. The port deliberately uses
  native save blocks and reconstructs terrain from the seed. Do not enable the
  raw-flash backend from the recovery patch.
- `AtlasSaveState` lives in SaveBlock3, subject to the engine's compile-time save
  budget assertion. Version 2 uses the same 1456 bytes: lossless row/column
  intervals or exact bitmaps preserve every recorded tile. Up to 130 compact
  chunks fit; highly fragmented coverage fits fewer (at least 25 maximum-size
  entries). It never evicts earlier visits; an update that cannot fit is refused
  without mutation. The map warns when space is low. This is bounded exploration
  persistence, not unlimited planet history.
- The saved clock advances during overworld simulation, pauses in menus, and
  saturates instead of wrapping. Regional weather is deterministic from
  seed, place, saved time and climate. A longitude-periodic field drifts and blends
  over six-hour epochs, avoiding arbitrary hourly/grid rerolls. Regional macro
  climate drives native rain, snow or fog effects, checked once per unpaused
  overworld second and on map entry. Weather queries never prepare remote detailed
  hydrology. The atlas footer uses the same regional model. This does not implement
  terrain changes or a complete seasons/ecology simulation.
- Physical names derive from connected macro landmasses, water bodies and drainage catchments.
  The local label describes the terrain inside that catchment; individual forests,
  ridges and river branches are not separately component-labelled. Names are seed
  stable, cross chunk boundaries and announce only changed hierarchy levels after
  a debounce/cooldown. This adds no terrain or save identity change.
  Dry riverbanks/lake shores/coasts use nearby prepared terrain semantics, within
  three tiles, without generating geography or changing discovery during the query.
  Regional plateau/valley/ridge descriptions compare an interpolated 400 km
  neighborhood. The eight-layer admin atlas exposes this relief classification;
  GEO TP can locate a classified plateau with a safe final 3x3 landing. These are
  annotations of existing geography, not newly carved features or individually
  named components. No generation/save version change is needed. Regional queries
  do not prepare hydrology; map rendering uses a bounded two-pixel budget.
  Ground presentation uses native floor details on high grass/snow and four
  sand variants. Sand behavior/layer matches the engine's original sand floor;
  these details preserve collision/elevation and require no new world version.
  Wetland ground cover uses native long-grass artwork with the existing grass
  behavior, preserving running/bicycle rules rather than importing new restrictions.
  It takes priority over generic coast sand on dry wetland cells.
  Its prepared bank material is green as well, matching the ground floor.
  Sand/grass borders use cardinal neighbor semantics and native sand quarters,
  preserving movement/elevation and the native sand behavior. The ring waits for
  the already scheduled stencil; border rendering performs no terrain queries.
  Fourteen spare roles plus one shared full-water duplicate supply the catalogue
  within 512 slots. Diagonal-only notches and other biome blends remain future work.
  GEO TP's fourth page finds a real sand/grass boundary by resumable bisection of
  dry ground, rejecting water interruptions and verifying a final safe 3x3 patch.
  Generation-7 forests/jungles/taiga can propose 3x3 groves from the native
  tree catalog. Whole footprints must be suitable, dry, flat and free of terrace
  connectors; incomplete proposals become ordinary grass, never partial canopies.
  Grove proposals leave periodic rows/columns free of tree collision. These are
  ground/cover corridors, not generated roads or a proof that every forest has a
  global route through pre-existing cliffs/water. Direct and ring rendering share
  footprint rules. The ring reads prepared semantics and retries within a two-tile
  radius as cells arrive; it adds no terrain query while rendering a grove.
  Two native flowering ground tiles decorate suitable mild/moist clearings without
  changing collision, elevation or normal-ground behavior. They retain native
  animation references and fill the remaining two secondary metatile slots (512/512); future
  catalog expansion needs reuse/bank planning, not an out-of-range tile index.
- The player map projects only genuinely visited tiles at every zoom. Admin-map
  reads never mark discovery. START cycles eight diagnostic layers in admin view.
  Distant admin pixels summarize macro notes; close views use final terrain, with
  resumable water preparation and bounded sampling work to preserve input response.
  Large terrain/elevation/basin views overlay major macro drainage paths with a
  bounded node budget; player fog never uses that global overlay.
  GEO TP also verifies real forest/desert/tundra/wetland destinations and 3x3 final
  collision before travel. Developer teleport is not normal traversal.
- Atlas reserves the upper 55,296 bytes of Expansion's existing 115,968-byte
  heap while its world is active. Native allocation then uses the lower 60,672
  bytes. Save relocation is compile-time checked against that boundary; the
  complete heap remains available outside Atlas scenes. The reservation begins
  only after the previous UI releases its allocations. Do not enter modes using
  fixed upper-heap scratch (such as contests) while Atlas owns that region. Check
  linker EWRAM/IWRAM/ROM totals and emulator allocations after changing budgets.
  Host sanitizers cannot establish ARM memory safety or frame performance.

## Branches, continuity and research

Use `master` as the single active branch. Direct commits are authorized by the
owner after checks. A short local worktree is useful for risky research; merge only
reviewed changes and remove the worktree. Avoid standing experiment branches.
A commit message should state behavior, checks actually run, and remaining limits.
Use Git history for decisions rather than a second taskboard or daily diary.

Reference research considered the bounded reading route and source/evidence
separation in `despairarc000/dgunmod`'s development workspace, and onboarding in
`despairarc000/pokeemerald-expansion`. Their separate status catalogs, gameplay
rules and branch policies were not imported. Atlas uses this short guide, the
vision and source searches because its source is directly available.

Before implementing further vision, trace the current call graph and choose an
observable vertical slice. Larger ecology, trust, terrain alteration, caves,
legendaries, evolution, survival and shared-world play need real integration and
acceptance tests. Design questions explicitly left open in FULL_PLAN remain open;
a mock, debug screen or document does not finish them.
