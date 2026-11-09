# Native Pokémon terrain source and Atlas adaptation

The active terrain catalogue uses the original Emerald graphics, metatiles,
attributes and palettes already in `data/tilesets/primary/general/`. The generated
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

Run `python3 dev_scripts/compile_atlas_native.py`. Generated C and manifest are
reviewed source; the preview stays under `build/atlas-native/`. No PNG decoding,
quantization or asset conversion takes place in the ROM. Original input files are
read-only to the compiler. Historical recovery inputs remain unchanged.
