# Puny World source and GBA adaptation

Artist: Shade (merchant-shade). Asset: Free 16x16 Puny World Tileset.

- Creator: https://merchant-shade.itch.io/16x16-puny-world
- Creator's OpenGameArt upload: https://opengameart.org/content/16x16-puny-world-tileset
- Download: https://opengameart.org/sites/default/files/punyworld-overworld-tileset_0.png
- License: CC0 1.0 Universal, confirmed on both creator pages on 2026-10-16; legal text in CC0-1.0.txt.
- Original source.png SHA256: 14d285bde8dee96cb0113947c8cce4922b86bf1d033b19123112145c7cef3a38.

The untouched source image is retained here. Only the selected environmental tiles are compiled into the ROM; no source characters/buildings are automatically placed. This is original third-party RPG art, not a Pokémon game tileset licensed by Nintendo.

Atlas adaptation: crops and quarter-tile assembly for 47 canonical river-bank shapes; grass/sand/rock/snow palette variants; native 15-bit colors and 4bpp indices; 8x8 subtile deduplication; stock two-layer metatile definitions; explicit freshwater/ocean behaviors. Ground greens are adapted to the existing native grass palette. Snow is an Atlas recolor of the source's grass texture, not a claim that Shade supplied snow art. The mountain renderer additionally assembles the authored cliff quadrants into 47 boundary shapes each for grass and recolored snow, using palette slots 10 and 12. Stepped local relief has blocked contours, walkable tops and a reserved southern access corridor. Trees remain catalog options pending whole-footprint placement rules. Conversion/derived art can be used under CC0; existing native Emerald assets retain their separate provenance.

Regenerate with python3 dev_scripts/compile_atlas_nature.py (Pillow). The generated C data, header, manifest and preview describe the selected options. The raw tiles array is padded to the full secondary upload size. No runtime PNG parsing, color conversion or pixel drawing is performed. Custom banks currently use one static animation frame.

Compared candidates: Kenney's Roguelike/RPG pack (CC0) is a useful larger future source; Tuxemon/Buch's older 16px pack uses CC BY-SA 3.0; Puny World was chosen for this prototype's river/landscape vocabulary and CC0 permission. No assets from those other packs are imported in this change.
