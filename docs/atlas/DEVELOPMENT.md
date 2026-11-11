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
Focused native checks are available with `--tour materials`, `--tour mesa`, `--tour headwaters`, `--tour coasts`, `--tour rock-sand`, `--tour karst`, `--tour snowline`, `--tour landmarks`, `--tour canyons`, `--tour wetlands`, `--tour lakes`, `--tour distributaries` or `--tour rivers`,
optionally with `--save path/to/a/copied/test.sav`; the harness copies supplied
saves and still uses genuine key input and emulation. The native runner holds a
POSIX file lock before copying the shared ROM/save/symbol/executable files; an
additional invocation exits before mutation. The emulator child inherits that
lock, so a terminated wrapper cannot expose a still-mapped ROM to overwrite.
An isolated host fixture verifies the busy-workspace rejection. Full tours allow up to ten
minutes of host wall time as inspector coverage grows; GBA frame budgets are
checked separately. Do not describe this host timeout as game loading time.
 `make` builds the Emerald target; other Expansion game
targets are retained engine code, not supported Atlas releases. `make release`
enables the engine's release/LTO mode. Build artifacts stay in `build/` or the
engine's existing ignored output paths. CI checks; it never commits or publishes ROMs.

Preserved tests exercise real Atlas C with host mocks. `dev_scripts/test_atlas.py`
compiles separate cases with sanitizers. Repeat `--case NAME` to select relevant
contracts for a focused change; omit it for the complete suite. Native asset/script
contracts still run. New regressions should protect behavior
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
  Generation 8 preserves the macro land/ocean mask and component identities while
  compressing the inland foundation and adding curved, tapering uplift belts tied
  to the seeded continental frames. Belts vary their side and orientation. The
  resulting heights feed climate and drainage before any local terrain is sampled.
  Atmospheric rise from an ocean cell starts at sea level, not its underwater
  floor, preventing ocean depth from manufacturing coastal orographic rainfall.
  This is bounded macro morphology, not tectonics, physical erosion or measured
  Earth hypsometry.
  Versions 4 onward normalize macro height to approximately 29.2% land before mountain
  uplift. Generation 7 adds vegetation collision, retaining generation 6's macro
  height/climate/drainage rules. Generation 9 retains generation 8's macro atlas
  while shaping dry riverbanks from cached channel pieces: eight distance bands
  taper incision back to the original floor across an 80-tile apron. Wet channel
  membership and water levels retain their previous ownership rules. Soil moisture
  blends toward saturation near rivers without changing atmospheric humidity or
  rainfall. Lake-bank wetness uses the existing fitted dry apron and height above
  its own lake; warm elevated lake shores can therefore become wetlands. Newly
  lowered inland banks do not become beaches; coarse coastal context also requires
  a sea corner in the interpolation footprint. These are bounded local profiles,
  not sediment transport, channel migration or full regional erosion.
  Generation 10 adds one bounded regional erosion pass after initial runoff.
  Incision depends on discharge and downhill grade, is capped at 12 ordinal
  height units, and cannot cut below the downstream base level or sea margin.
  Frozen input heights make the pass independent of iteration order. The pass
  reuses temporary scratch, then rebuilds climate, flood routing and runoff;
  statistics and lake spill levels come from that final reconstruction. Importing
  final height notes skips erosion, avoiding a second incision on restore.
  Creation progress accounts for both passes and land/ocean component labeling.
  This does not simulate transported sediment or repeated geological epochs.
  Generation 11 shapes submerged relief after land outlines: nearby land defines
  shelf/slope bands, periodic broad noise defines basin-floor variation, and three
  curved submerged ridge belts cross deep water. All outputs remain below sea
  level and preserve the macro land mask. Atmospheric and fluvial rise from ocean
  neighbors uses sea level, never floor depth. Imported final notes skip both
  bathymetry and erosion; different creation batch sizes reproduce the same nodes.
  The elevation atlas reads seabed heights in close sea views as well as distant
  summaries. This remains coarse morphological bathymetry, not ocean circulation
  or an underwater traversal layer.
  Generation 12 applies a shared periodic domain warp to continental and uplift
  frames. Sparse ridge-associated oceanic island seeds take land out of the same
  1345-cell budget before the continental quantile; a deterministic fallback ensures
  an island seed. Each seed feeds normal climate, erosion, drainage and component
  labeling. This creates morphological chains, not a plate/hotspot simulation.
  Island GEO TP uses the existing physical naming threshold (fewer than 32 connected
  land cells) and validates a real dry 3x3 landing. Developer searches yield and
  stop after 4096 steps; retry continues the search cursor, and cancellation remains
  available. A failed bounded search does not establish that a feature is absent.
  Continental seed descriptors are cached in creation scratch without changing
  older generation results. The native test prints generation-stage frame costs.
  Generation 13 adds rare dormant cones on final dry lower upland nodes, excluding
  lake centres and main river nodes. Hash-selected sites have 2–5 km radii, bounded
  octagonal cone profiles, raised rims and lower crater floors. The same uplift
  feeds direct ground height, hydrology's terrain queries and final habitat; local
  native cliff tiers follow the cone profile. A radial gap permits walking through
  all tiers. Water and carved-valley ownership retain priority. Site descriptors
  use a single-owner spatial cache, reset with the world seed; no all-world scan,
  extra saved state, or permanent site list is needed. Macro climate/drainage and
  land percentage remain generation 12's. This is dormant morphology, not eruptions,
  lava, plate tectonics, a crater-lake model or a cave entrance. Close admin map
  summaries include local uplift without preparing remote hydrology. GEO TP checks
  the real dry landing and scans a camera viewport for native cliff art, yielding
  between points. Physical popups distinguish volcanic cones from caldera floors.
  Generation 14 exposes rock ground in the inner three quarters of dormant cones
  and on non-tundra terrain above height 205. Native Cave floor pixels are compiled
  into the shared secondary graphics with a derived rock palette; Atlas alone uses
  a clone of General's primary metatile catalogue. Reclaimed primary roles 384–430
  supply 47 rock cliff masks and 431–434 supply four floor variants. All other
  primary definitions/attributes initially remained byte-identical; primary graphics, palette
  ownership and animation callback remain General's. Other layouts keep General.
  Rock, grass and snow cliff selections preserve the same collision/elevation
  topology; snow retains priority. Exposed rock clears vegetation, so new saves
  receive generation 14 rather than changing old worlds' walkability. This is a
  material/exposure layer, not sediment/soil ageing or every biome transition.
  Generation 15 flares selected lowland main-river outlets near their fitted sea
  intersection. Selection uses the existing runoff threshold 600, node water
  height at most 30 units above sea, and a seed predicate. The final straight
  coastal piece contributes a tapered overlay up to 3000 tiles long, widening
  from the main-channel width to four times that width (capped at 400). Original
  endpoints are retained for short tapers, so adjacent water buckets evaluate
  the same width, height and bank profile without independent clipping rounding.
  The 80-tile dry apron remains inside the 480-tile cache margin. Macro nodes and
  climate are unchanged. New worlds admit up to 96 pieces per bucket: 16 owners
  times four base pieces, one possible receiving-shore split and one flare.
  Older generations retain their original 24-piece admission cap and fixed
  widths. The four bounded piece banks add approximately 4.6 KiB of static RAM;
  no world-sized river list or new saved state is introduced. Estuary GEO TP
  searches a safe visible bank upstream of the sea cut; ordinary river-mouth TP
  still requires both sea and river in its viewport. These are flared outlets,
  not simulated tides, salinity, sediment transport, deltas or distributary fans.
  New-world admin views at 100 tiles/pixel use actual prepared local terrain and
  water rather than macro summaries, so resolvable outlet widths are visible.
  A minimum-width cartographic stroke reads prepared channel capsules with a
  bounded 71-tile footprint to keep narrow stretches visible between samples;
  this does not change physical water membership or any other climate layer.
  The stroke never touches LRU state or prepares terrain. Painting still yields,
  accepts input and leaves the exploration journal alone.
  Generation 16 renders every qualifying small-channel edge (runoff 240–599)
  whose parent is another qualifying channel, a main river or the sea. This
  exposes upstream chains and small coastal outlets that the previous direct-to-
  main-river filter hid. Tributary classification retains land receivers, while
  the small-river predicate also includes coastal outlets. Each selected path
  follows the existing priority-flood parent graph all the way downstream;
  no random disconnected stream or new macro runoff is introduced. Existing
  receiving-shore fitting, narrow channel width, bank carving and wetness apply.
  The small-river inspector and coarse river overlay use the same predicate.
  River statistics still count main channels at runoff 600 and above. Coarse
  notes remain approximately 420 km apart: this is not metre-scale drainage
  refinement or a claim that all local creeks have been generated.
  The hot native planner, continental-frame, neighbor and heap-push routines use
  ARM mode together; the ARM7 long multiplier accelerates exact reciprocal and
  constant-divisor arithmetic while avoiding repeated ARM/Thumb handoffs. Other
  engine code retains its existing mode. This changes compilation, not formulas,
  generation identity, saved geometry or storage. Native checks compare real save
  checksums and journals across builds and time the same saved location.
  Seed-only continental reciprocals replace repeated divisions with a high
  product and exact remainder correction. Signed frame calculations retain C's
  truncation toward zero. They use creation scratch only and preserve older
  macro outputs; the full-width arithmetic contract covers every frame divisor.
  Generation 17 scales the dry bank apron to 16 times the sampled channel half-
  width, bounded to 80–1600 tiles. Eight distance bands retain the graded floor
  and soil-moisture profile; broad rivers therefore have wider low, wet ground.
  Tapered outlets use their local interpolated width. Cache clipping expands to
  a 2000-tile apron so a wide floodplain survives a bucket boundary; older
  generations retain their original 480-tile clipping and 80-tile bank reach.
  This reuses the same piece storage and does not alter macro notes or add saved
  fields. Low dry inner plains receive the physical Floodplain descriptor;
  adjacent water still takes shoreline naming priority. GEO TP validates dry
  low ground with actual valley/moisture semantics and a safe 3x3 landing beyond
  the immediate bank. The close moisture atlas exposes the same sampled wetness.
  Larger clipped geometry belongs to the new generation identity; no universal
  bit-identity with an older generation's detailed raster is implied. This is
  bounded incision/riparian moisture, not sediment deposition, levees, seasonal
  inundation or a complete flood model.
  Generation 18 adds one local tributary level beside suitable dry inland reaches.
  Seeded root sites follow the existing channel at 8192-tile intervals; rainfall
  controls density, branch lengths are 4096–8191 tiles, and heads angle upstream.
  Widths are 2–10 tiles. A root check plus 16 ground probes reject lake crossings
  and profiles lacking two units of clearance below sampled original terrain.
  Root water height is resolved in its own canonical main-channel bucket; creek
  profiles descend into a flat receiving reach across the parent floodplain.
  All main pieces are prepared first and retain water ownership. The second
  preparation pass yields after at most one ground query, including when four
  cache frontiers interleave. Only nearby branch geometry is considered. The
  128-piece bank reserves 96 main slots and 32 local slots; static RAM increases
  by approximately 2.1 KiB, with no world-sized creek list or new save fields.
  Short creek endpoints remain uncut across bucket boundaries. Internally the
  high widthB bit marks a creek, its upper fraction bits encode the flat reach,
  and TipWidth decodes the physical width; callers must not treat flags as width.
  Local creeks receive Creekbank naming and a real safe inspector target. Close
  admin terrain strokes also preserve visibility at 4/20 tiles per pixel;
  read-only queries never prepare terrain or mark player discovery. Native QA
  additionally saves at a creek and times cold Continue, checking identical
  visible terrain, position, seed and journal. This is one bounded local branch
  level, not a complete fine-resolution watershed solver, spring/groundwater
  simulation or proof of physically exact flow everywhere between probes.
  Generation 19 adds elongated sand ridges in coherent warm, dry lower-upland
  fields. An independent broad substrate field and smooth climate/height envelope
  bound their occurrence; local relief contributes 0–6 ordinal height units to
  both direct ground queries and final terrain before water carving. Volcanic
  cones suppress dunes with a surrounding fade. New local creeks reject dune
  fields during their existing bounded terrain probes; established river and lake
  ownership retains priority. This is seeded morphology, not wind transport,
  sediment conservation or migrating dunes. Macro nodes remain generation 18's.
  Dune trough/crest landforms append existing cell enums without changing layouts
  or saved fields; the native fauna validator admits both. Atlas primary roles
  435–481 reuse original cliff quarters over sand, with derived sand colors and
  native sand behavior. These ridge faces remain walkable at the ground elevation,
  unlike mountain walls. Other layouts retain General. A yielding Dunes GEO TP
  search validates a safe 3x3 patch and visible native ridge faces. The ninth admin
  layer shows local relief, including dunes, at close zoom; queries do not mark
  exploration. Native QA checks cold Continue independently at a creek and dune,
  preserving visible tiles, seed, position and discovery. Floodplain inspection
  additionally checks the location announcer's three-tile shoreline stencil and
  rejects volcanic precedence, so a curved bank is not mislabeled as a plain.
  Generation 20 adds rare flat-capped dry-country uplands on warm, relatively
  dry macro nodes at heights 156–185, excluding flooded centres and main channels.
  A seed predicate selects sites; 2–5 km bounded octagonal footprints receive
  smooth local outline perturbations. The inner half-radius adds a constant
  24–39 ordinal units; surrounding shoulders taper to zero. Existing small ground
  variations remain, so a cap is locally subdued rather than a perfectly level
  mathematical floor. The same uplift feeds direct ground and final habitat before
  water carving. Riparian floors/water keep priority. Dunes fade outside the mesa
  footprint rather than crossing the cap. Native rock ground and three contour
  tiers follow uplift, with snow priority and a narrow walkable axial approach.
  A single-owner descriptor cache, an exact repeated-point memo and existing noise
  fields avoid a permanent site list, new graphics or saved fields. Macro climate/drainage notes remain generation
  19's. This is bounded morphology, not stratigraphic geology, sediment transport
  or a regional erosion reconstruction. Mesa naming, a fifth-page inspector and
  close relief views use the actual feature. Inspection bisects a contour on the
  approach, then validates dry mesa ground, safe collision and visible native rock
  walls. Native QA checks cold Continue there independently. Older generations
  retain their terrain identity; never retag a save to add this feature.
  Generation 21 adds one smaller tributary level on validated local creeks.
  Up to two seeded roots lie at thirds of each parent segment; branches are
  nominal spans of 512–1535 tiles with a bounded upstream bias and channel
  half-widths 1–3. Root rain must
  be at least 96. One root check and 16 profile probes reject lake/dune crossings
  and insufficient original ground clearance. Head rise is capped at three
  ordinal units above its receiver; the flat join covers the parent bank apron.
  Whole parent endpoints and canonical world-coordinate root hashes keep child
  geometry, validation and receiving heights independent of the querying bucket.
  Parent culling expands to 1900 tiles to include children reaching nearby banks.
  Three preparation passes preserve main-river, parent-creek, then child ownership;
  each step still evaluates at most one ground point, with state kept per bucket
  for interleaved frontiers. New worlds admit 192 pieces (96 main/96 local), while
  generations 18–20 retain their 128-piece admission limit. Permanent banks add
  4096 bytes, and bucket bookkeeping adds 16 bytes. No save fields are added.
  WidthB bit 0x80 marks a child alongside the existing 0x8000 creek flag; join
  fraction bits 8–14 are unchanged. Always decode physical width with TipWidth.
  A read-only resident-piece query supports a safe fifth-page Headwater inspector
  and the Headwater creek descriptor. Close terrain-atlas strokes include the
  new water without changing exploration. Macro drainage notes remain generation
  20's; this is two bounded local tributary levels, not a complete fine watershed,
  groundwater, discharge-conservation or spring model.
  Native QA saves/resets/Continues at a child creek and checks actual resident
  children, visible tiles, position, seed and journal. Legacy snapshots also
  fingerprint the visible metatile words, in addition to macro/habitat checksums. A separate six-seed host
  streaming check runs across both 4 km water-cache axes and reverses direction;
  it compares visible tiles only after walking, so the direct oracle cannot warm
  hydrology ahead of the streaming scheduler and mask a readiness wait.
  Generation 22 adds a periodic 20 km coastal-exposure field over original dry
  low coast ground (pre-modifier height 121–135), excluding lake context. Its
  0–24 ordinal uplift fades inland. Interpolated ocean contribution from the same
  four macro corners fades influence smoothly to zero at inland cell boundaries;
  the sampler memo holds one extra byte, without a world-sized distance field.
  This modifier never raises a floor below sea level. Mobile dunes yield to the
  raised coastal material. Direct ground and final terrain use the same rise;
  macro notes remain generation 21's. This is resistant-coast morphology, not
  computed wave energy, tides, sediment transport or a coastal erosion solver.
  New rocky-coast cells encode relief tier 6. Prepared stencils emit a blocking
  native cliff only toward sea at least eight height units below, leaving inland
  shoulders and river openings traversable. Grass/rock/snow tops and water priority
  retain their behavior; humid tops keep native ground cover, while dry exposure
  uses rock. Groves are excluded from this wind-exposed cliff context, avoiding
  canopies that hide its edge. Water banks below grassy cliff tops use rock/snow
  geology independently of the top's vegetation color. No new graphics are needed.
  The native coastal wildlife selector admits the new landform using the existing
  coast roster and climate precedence. Location popups distinguish Coastal upland
  from Sea cliffs near actual ocean water. A fifth-page inspector bisects a dry/
  sea interval, validates a safe inland 3x3 patch and scans the camera for both
  real sea and native cliff faces. Close relief views show this local feature.
  Native QA verifies blocked wall words, safe approach, journal-isolated map reads
  and cold Continue. Host streaming fixtures cross the cliff and both 4 km cache
  axes at running cadence, comparing visible terrain afterward to avoid oracle
  prewarming. Diagnostic tours can retry exhausted headwater/coast search batches
  through real menu input; each game search remains bounded and cancellable.
  Generation 23 adds rare closed surface depressions on wet, warm lower uplands
  (macro height 145–175, rain at least 132, moisture at least 128 and temperature
  at least 100), excluding flooded centres, qualifying river nodes and dormant
  cone/mesa sites. Seeded selection produces 512–1280-tile warped octagonal bowls
  with a subdued inner quarter and 16–31 ordinal units of potential lowering.
  Actual lowering is capped at eight units above sea level. One owner descriptor
  and exact repeated-point memo add 60 static bytes including the inspector cursor;
  no world-sized depression list, new graphic tiles or saved fields are needed.
  Direct ground and final terrain use the same cut before normal water/bank
  ownership. Macro climate/drainage notes remain generation 22's. This is bounded
  karst-like surface morphology, not limestone chemistry, groundwater transport,
  sinkhole collapse, new lake impoundment or an underground cave implementation.
  Dry, uncarved bowl cells use relief tag 7; native rock contours face lower actual
  eight-unit height bands. Existing river/lake openings retain priority. A clear
  axial corridor crosses the contours; humid floor cover stays, but groves do not
  hide the depression. Physical naming identifies Sinkhole, and close local-relief
  views show pits in pink. The sixth inspector page bisects a real contour just
  outside the walkable axis and validates safe landing plus visible rock walls.
  Bisection follows that off-axis column, avoiding errors from a warped outline
  shifting the centreline contour outside the camera. A 64-seed search contract
  checks inspection coverage; worlds without suitable karst sites can legitimately
  report no safe destination. Searches remain bounded and cancellable.
  Native QA checks actual walls, naming, safe approach, journal-isolated atlas,
  cached crossing and cold Continue. Six-seed streaming checks compare visible
  words after 15360 camera steps, without oracle prewarming. Older-world native
  snapshots check visible tiles, macro/habitat checksums and the unchanged journal.
  Focused emulator tours fail if they never reach an actual matching feature.
  Generation 24 derives a 576-byte ocean-draining/no-intervening-lake bitmap in
  the existing parent-first flood-order pass. Detailed channel levels on those
  routes are incised by three quarters of their height above sea+8, capped at 24
  ordinal units. This monotone height transform preserves downhill node joins;
  nodes whose route enters a lake retain their original level, as do the lake
  receiving planes. Source interpolation and fitted sea/lake outlets use the same
  channel level. Macro nodes, their water notes, climate/runoff and journal format
  remain generation 23's; the derived bitmap is rebuilt on Continue.
  Suitable non-creek channels (half-width 16–96) in original dry ground at least
  145 and at least 16 above the river acquire a bounded canyon section within the
  existing bank apron. Incision strength fades over a further 16 height units.
  The eight-band section has a subdued inner floor and steeper outer shoulders;
  lowest fitted floor owns its material, with order-independent ties. Main channel
  paths stay connected; local tributary admission can change with their receiving
  levels, which is why this is a new generation rather than a save retag.
  Dry canyon banks use actual eight-unit height contours and native rock/snow
  walls. Inner floors and existing smooth saddle fields supply walkable openings.
  Groves do not hide these walls. Canyon naming remains contextual within its
  catchment, not a separately component-labelled gorge. The sixth-page inspector
  filters local river/ground relief, then searches both perpendicular banks at
  four-tile spacing to avoid aliasing narrow walls; it validates a safe 3x3 patch
  and actual visible rock faces. Searches still yield, stop at their budget and
  remain cancellable. Close local-relief views also use the existing read-only
  minimum-width river stroke, so narrow water stays continuous at map resolution.
  Tests cover monotone levels, lake-route exclusion, unchanged macro notes, bank
  overlap order, native walls/naming/map isolation/save reload and six-seed strict
  running-cadence streaming across both water-cache axes. This is bounded incised
  morphology, not geological epochs, sediment transport or a full erosion solver.
  Generation 25 shapes suitable humid, low-relief riverbanks into natural levees
  and backswamps. The existing connected channel supplies position and water level;
  original dry ground must be at most 155, 4–16 units above the river, with rainfall
  at least 96 and vapor supply at least 80. Non-creek channels have half-widths
  24–128; lake context is excluded. These are ordinal bounds, not measured slopes
  or sediment transport. The profile does not depend on bucket-clipped endpoint
  grades, avoiding a cache-boundary-dependent eligibility test.
  The innermost bank band lies three units above the river, followed by lower
  two- and one-unit floors. The outer three bands taper back to original ground.
  Levee wetness is lower than the backswamp's, while atmospheric vapor/rain notes
  stay unchanged. No ground is raised above the original floor or lowered into
  the channel; river/lake membership and receiving planes retain their ownership.
  Lowest fitted floor owns its feature flags, with order-independent ties.
  The temporary bank profile adds one shape byte; no persistent geographic array,
  raw art or saved fields are added. Macro notes remain generation 24's.
  Warm backswamps use the native wetland biome and marsh cover; frozen/cold climate
  retains its priority. Levees retain native ground rather than marsh-cover art.
  Groves are excluded from these generated bank shapes. Both remain walkable and
  use contextual Backswamp/Natural levee names inside the catchment. Sixth-page
  inspectors search real perpendicular bank sections and verify dry 3x3 landings
  plus the announcer's three-tile shoreline stencil, so a bank is not mislabeled.
  Tests cover the cross-section, climate/lake/width/creek gates, overlap order,
  six-seed unchanged macro notes, safe names/habitat and running-cadence streaming
  across both cache axes. Native tours check marsh/levee contrast, actual popups,
  map isolation, cached walking and cold save reloads. This is static bounded
  floodplain morphology, not deposition history, seasonal flooding, groundwater
  flow, cut-off lakes or distributary deltas.
  Detailed map painting shares a budget of eight water-preparation steps per
  frame across all pixels. This replaces the previous one-step cold-pixel stall,
  retaining bounded work and responsive menu input. Pixel-for-pixel native image
  comparisons verify that batching changes preparation timing, not final colors
  or exploration state.
  Generation 26 adds two short coastal distributaries at suitable fitted estuary
  mouths. Their canonical split is 1,600 tiles upstream of the sea cut, with
  outlets 512 tiles seaward and 640 tiles to either side. Parent receiving height
  must lie within half a height bin of sea level, keeping both branches on the
  same integer plane. Width follows parent runoff, capped at 48. Both branches
  must pass 17 fixed ground probes: dry lowland root, no lake, ground at most
  sea+12, a sea endpoint, and no sampled return to land after entering sea.
  Both pass or neither is admitted. This is sampled morphological validation,
  not a continuous slope proof, sediment model, tides/salinity or deposited land.
  The parent and existing estuary flare remain intact and own overlapping water.
  Whole short endpoints are retained in neighboring water banks; flag decoding
  preserves the real width. Warm steps still make at most one ground query.
  Four banks grow from 192 to 224 pieces each (2,048 bytes total), covering the
  conservative bound of 128 main plus 96 local pieces. The three validation bytes
  reuse bucket padding; its size stays 68 bytes. Older generations retain their
  original admission caps. No new raw art or journal fields are added.
  Sixth-page Coastal distributary inspection warms the actual branch, bisects its
  bank and validates safe 3x3 collision plus proximity. Location announcements use
  Distributary bank; close atlas strokes include the prepared branch geometry.
  Seven-seed tests check connected centerlines and unchanged macro notes; strict
  camera walks check streaming, with a one-query budget/rejection fixture.
  Native QA visits, labels, maps, cached crossings and cold save reloads cover the
  actual feature. Use a new generation-26 world for these channels.
  Existing versions 3 through 25 retain their original
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
  The local label describes the terrain inside that catchment; individual forests
  and ridges remain contextual descriptors. Bounded volcanic
  cones/calderas, mesas and sinkholes additionally receive a landmark identity from
  their seeded macro site, not their containing render chunk. Their root names are
  deterministic and distinct across site IDs; a cone and its caldera share one
  root. Local labels fit the existing 21-byte buffer. Shoreline priority replaces a bounded
  landmark identity when the local label becomes a bank/coast. Switching landmarks
  in one catchment queues only the changed leaf, with normal debounce/cooldown.
  This identifier lives only in the ephemeral place/tracker; no saved fields,
  world generation, geometry, map fog or journal format change. Native landmark
  tours verify actual owner IDs, visible proper-name labels and cold reloads.
  Lakes additionally retain stage 8's connected equal-level component identity
  in a 1,728-byte packed cardinal forest. Roots use the smallest node ID, with
  longitude wrapping and latitude capped. The existing lake-fitting owner supplies
  that identity on actual lake water and nearby shores; it does not reuse the
  surrounding catchment ID. Different lakes in one catchment can have distinct
  names. Dry shoreline detection still reads only resident terrain semantics.
  This derived index is rebuilt during normal New Game/Continue, never saved;
  it preserves generation-3 through generation-25 geometry and the journal.
  `--tour lakes` verifies a proper-name shore popup and identity after cold reload.
  River names follow the drainage graph: at each confluence the greatest incoming
  runoff continues the mainstem, with the smaller node ID breaking a tie. Other
  incoming branches retain a distinct identity ending at that confluence. Rivers
  end at a receiving lake or the sea; dry outflow reaches take the downstream
  river identity. This is a deterministic naming convention using ordinal runoff,
  not a claim about historical real-world river names or simulated discharge.
  Four-bit owner slots retain each prepared piece's macro source (448 bytes total,
  no RiverPiece/bucket size changes). Read-only bank lookup uses only resident
  geometry, gives the larger river priority at overlaps, and follows the graph
  once for the selected source. It never prepares water, changes LRU order or
  marks discovery. Creek/headwater/distributary descriptors keep their priority.
  Proper river names reuse the existing place/leaf debounce and 21-byte buffer.
  No generation bump or saved fields are needed; supported older saves gain names
  while retaining terrain. `--tour rivers` checks native main/estuary/tributary
  labels, bank travel and identity after cold Continue. Host checks use an
  independent strongest-child oracle across legacy/current worlds, include lake
  outflows and verify that identity queries leave resident buckets untouched.
  Generic forest/ridge labels remain contextual descriptors. Names are seed
  stable, cross chunk boundaries and announce only changed hierarchy levels after
  a debounce/cooldown. This adds no terrain or save identity change.
  Dry riverbanks/lake shores/coasts use nearby prepared terrain semantics, within
  three tiles, without generating geography or changing discovery during the query.
  Regional plateau/valley/ridge descriptions compare an interpolated 400 km
  neighborhood. Valley/ridge axes can slope: their center must fit the opposite
  longitudinal samples, with higher/lower cross-slopes on both sides. Closed pits
  and isolated peaks fail that profile test. A relative ridge need not exceed an
  absolute highland threshold. These descriptions do not carve terrain heights.
  The nine-layer admin atlas exposes this relief classification;
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
  Rock/grass and snow/grass boundaries use primary roles 482–496 and 497–511,
  with the same rounded native cardinal geometry and derived palettes 10/11.
  Interior quarters retain the existing floor art; no graphic tile, height,
  collision or saved-state expansion is needed. Flat material floors wait for
  the already prepared ring stencil, with no terrain query during border rendering.
  Rock connectors retain their floor behavior. Water and other colored materials
  suppress a false green fringe. A fifth GEO TP page bisects real dry material/
  grass transitions and validates an actual edge tile plus the safe 3x3 patch.
  Native QA times cold Continue at the snow boundary and checks cached crossings.
  The 512 secondary metatile slots and previously reclaimed primary roles 384–511
  are occupied; additional catalogues require explicit unused-role/bank planning. These are presentation
  changes; supported worlds retain their existing height and collision rules.
  Fourteen spare roles plus one shared full-water duplicate supply the sand catalogue
  within 512 slots. Rock/sand borders additionally reclaim Atlas-only primary building roles 352–366.
  They use the same native rounded cardinal geometry, textured rock interiors and
  derived secondary palette 12, whose outside color matches the dominant native
  sand floor. Palette 12 participates in the normal terrain weather/time blending.
  This uses no new graphic tiles, saved data, height changes or terrain queries.
  Water/snow continue the rock edge. At three-material junctions, existing green
  outlines retain priority; a complete three-material blend remains future work.
  The sixth entry on the fifth inspector page bisects a real dry rock/sand interval,
  validates the border and checks a safe 3x3 landing. Other layouts keep General's
  original building roles. Rock/snow transitions reclaim primary roles 337–351 and reuse the same rounded
  silhouette with native snow texture outside and actual Cave rock interiors.
  Eleven compiled 4bpp subtiles initially raise graphic use to 68/512; primary/secondary
  catalogue sizes and RAM stay fixed. Previously unused palette-12 entries 1–4
  hold native snow colors, preserving its rock/sand entries 11–15. The selector
  reads existing prepared cardinal semantics and adds no geography queries.
  Green and rock/sand outlines keep priority at mixed junctions. A sixth-page
  Rock / snow inspector validates an actual dry edge and a safe 3x3 landing.
  Native QA checks visible edges, cached walking and cold save reload there.
  Primary roles 367–383 remain untouched.
  Snow/sand boundaries additionally reclaim primary roles 322–336 and eleven
  compiled subtiles, bringing current graphic use to 79/512. They retain actual
  snow interiors and textured native sand outside. Palette-12 entries 5–8 hold
  native sand shades; all first 68 graphic tiles remain byte-identical. Water,
  green and rock junctions retain their existing priority. A controlled coastal
  terrain fixture verifies the actual temperature boundary and ring/direct
  agreement; six-seed random worlds are not assumed to contain this rare pair.
  There is no dedicated snow/sand inspector entry, and no claim of a native visit
  to a natural cold beach. Native snowline travel/save checks cover the combined
  graphics bank. Diagonal-only notches and complete three-material blends remain
  future work.
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
  reads never mark discovery. START cycles nine diagnostic layers in admin view.
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
