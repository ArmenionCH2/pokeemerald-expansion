# Native Pokémon terrain source and Atlas adaptation

The active terrain catalogue uses the original Emerald graphics, metatiles,
attributes and palettes already in `data/tilesets/primary/general/`, plus the
Cave floor and its palette in `data/tilesets/secondary/cave/`. The generated
manifest records the exact input hashes and source metatile references. Original
Pokémon artwork belongs to Nintendo, Creatures and GAME FREAK; it is not open-source
art. Existing game/engine credits remain applicable.

Source project references: https://github.com/pret/pokeemerald and
https://github.com/rh-hideout/pokeemerald-expansion . No external tileset, map,
character, story event or script is imported by this compiler.

Atlas preserves its terrain role IDs and collision rules. Water/bank masks use
native pond/ocean references and original bank quarters; primary water and flower
animation tile references remain live. Snow, sand-bank and rock-bank colors are
Atlas palette derivatives of native palettes, not claims of original snow artwork.
The 3x3 grove adapter uses original tree metatiles 0x1D4/0x1D5/0x1E4/0x1E5 with
nearest-neighbor scaling and transparent ground colors to retain generation-7
footprints. Native cliff quarters supply the terrace catalogue. The pass roles use
native grass/snow floor with reduced rock foreground from metatile 0x13B. Its blue
water fringe is masked and its original water behavior is not copied; these
details remain walkable ground.

Wetland ground cover uses original long-grass graphics from metatile 0x15 while
retaining Atlas's existing short/tall-grass behavior value 2. It does not import
the original long-grass movement restrictions. The previously unused catalogue
slot 900 supplies this role; terrain and grove collision remain unchanged.
Dry wetland cover also takes priority over the generic low-coast sand floor.

Sand/grass ground boundaries assemble original General 0x118..0x12A quarters
using the four cardinal neighbors. Roles 901..914 and reclaimed role 887 cover
the fifteen border cases; full sand keeps its floor variant. Original sand
behavior/layer (0x1021) is retained for every border. Full-water roles share
their identical native animation references, freeing 887 without dropping a bank
material. Water, rock and snow neighbors do not introduce a green fringe.
This cardinal catalogue rounds outer corners; it does not yet treat diagonal-only
concave notches or every biome blend. Rock/grass and snow/grass are described below.

Run `python3 dev_scripts/compile_atlas_native.py`. Generated C and manifest are
reviewed source; the preview stays under `build/atlas-native/`. No PNG decoding,
quantization or asset conversion takes place in the ROM. Original input files are
read-only to the compiler. Historical recovery inputs remain unchanged.

Atlas's primary metatile catalogue copies General, reclaiming unused building roles
384–430 for rock cliff masks and 431–434 for ground variants. Other layouts keep
General's catalogue. Primary graphics/palettes and General's animation callback
remain shared. Rock ground uses Cave 0x201 pixels quantized offline to the derived
rock palette. Cliff quarters retain native General geometry, with Cave ground
beneath grey rock walls. This adds four secondary graphic tiles and no extra VRAM
bank. It does not import cave maps, scripts, movement restrictions or a cave system.

Generation-19 sand ridges reclaim primary roles 435–481 using the same original
General cliff quarters over native sand. The derived sand palette is shared with
sand banks. Their attributes retain native sand behavior/layer 0x1021; Atlas
renders these low ridge faces without cliff collision. Other layouts still use General directly.

Primary roles 482–496 and 497–511 round rock/grass and snow/grass ground borders.
They retain the original General sand-edge quarter geometry, recolored with
secondary palettes 10/11; fully interior quarters use the actual Cave-derived rock
or native snow floor. No additional graphic tiles are needed. Edges use normal
walkable-ground attributes 0x1000, preserve map collision/elevation, and add no
terrain height or saved state. General primary roles 0–383 remain byte-identical;
Atlas now uses all reclaimed primary roles as well as all secondary metatile slots.
The borders use cardinal topology. Diagonal-only notches and transitions between
two non-green materials still require additional catalogue planning.

Generation-22 sea cliffs reuse the existing native rock/snow cliff masks and
bank materials. Grassy tops retain their native ground artwork; exposed dry tops
use the existing Cave-derived rock floor. This adds no graphics or metatile roles.

Rock/sand borders reclaim Atlas-only unused primary building roles 352–366. They
reuse the General rounded sand-edge silhouette with textured Cave rock interiors.
Derived secondary palette 12 uses the same rock shades as palette 10 and the
native sand floor's dominant outside color. No original graphics, palettes, native
animation bindings or other layouts are modified. This adds no raw graphic tiles.
Three-material junctions retain the existing green outline priority; diagonal-only
notches and other non-green material pairs remain future work.

Rock/snow boundaries reclaim Atlas-only primary building roles 337–351. Their
rounded silhouette comes from the same General sand-edge quarters; native snow
floor texture replaces the exterior green pixels. Fully interior quarters retain
Cave rock. Eleven compiled 4bpp subtiles use unused palette-12 entries 1–4 for
native snow colors and existing rock entries 11–14. Other palettes and original
input assets stay unchanged. The compile step performs this adaptation once;
there is no runtime art conversion. Native terrain graphic usage is 68/512.

Snow/sand boundaries reclaim Atlas-only primary roles 322–336. They use the
same rounded native silhouette, native snow interiors and textured General sand
outside. Eleven additional compiled subtiles use palette-12 entries 5–8 for sand,
preserving all earlier 68 graphics exactly. This brought graphic use to 79/512 before the alternate canopy was added. This is
renderer/asset support; controlled host terrain validates the rare pair. No claim
is made that every random world contains it or that a natural cold beach was
visited in the native emulator. Existing green and rock junction priority stays.

Walkable forest understory reclaims Atlas-only primary roles 367–368. These
copy General's original leafy shrub metatile 0x193, using only a horizontal
mirror for the second variant so its vertical lighting stays intact. Native
graphic references and palettes remain unchanged; no new raw tiles are added.
The adapter uses normal ground behavior/layer (0x1000), while terrain placement
keeps tree footprints, tall grass and protected approaches intact. This is
small ground-cover variety, not new palm/conifer artwork or canopy layering.

Alternate broadleaf crowns use General 0x1D6/0x1D7/0x1E6/0x1E7, with the same
transparent-ground adapter and nearest scaling as the original 3x3 grove.
Atlas-only primary roles 369–377 keep the original native palette and canopy
layer. Twenty-five deduplicated subtiles are appended after the established
79-tile bank (104/512 total); previous graphics and the secondary catalogue
remain byte-identical. The shared grove hash chooses one whole family without
moving its footprint. This is another native crown variant, not a new species,
conifer/palm set, walk-under canopy system or extra runtime RAM allocation.

Warm-climate bloom variants reclaim primary roles 378–381. They reindex the
native General 0x004 flower frame into spare entries 1–9 of palettes 10/11,
changing only its center/petal colors to purple or gold. Every leaf and grass
pixel retains its native color. Existing cliff/snow entries 11–15 stay intact;
UI palettes 13–15 are not used. Four deduplicated subtiles append after the
104 existing graphics (108/512 total). These new color variants are static;
the original native animated flower references remain unchanged.
