# Exact Atlas source recovery

The supplied ZIP contained a vision, exact source references, and a transfer
helper. The helper was executed against `despairarc000/atlasgba` at commit
`4c3e95f3c78aa364e0aa81abe0aa75d7d5893628`, relative to pristine import
`ae2c4a415eecdfe9cfed53f8a64206bc7c3fc6a9`: **37 additions, 23 modifications**.
All 60 outputs were materialized and SHA-256 checked. This is recovered source,
not a placeholder manifest. The original SOURCE_REF status describes the input
package; EXTRACTED_SOURCE_PROVENANCE records the recovered bytes.

`original.patch` is the full binary Git delta for the game and added assets.
It preserves original modifications without keeping untouched old engine copies.
It is an immutable migration input, not a patch for this Expansion revision.
The active implementation was adapted separately. The original transfer helper
is retained verbatim as evidence; use the current utility below for recovery.

```sh
# With the original read-only source checkout containing the pinned commits:
python3 dev_scripts/atlas.py recover --checkout /path/to/atlasgba
# Or with a public pret/pokeemerald checkout containing the pinned upstream commit:
python3 dev_scripts/atlas.py recover --checkout /path/to/pokeemerald --pristine
```

Recovery reconstructs only changed files, verifies all 60 recorded hashes, and
creates `build/atlas-recovery/AtlasGBA_FULL_GAME_SOURCE.zip`. It never modifies
the source checkout or the live game. Existing recovery output is not overwritten.
The public base commit is `731ad5bfd6e6f265508d0efcca0ba42f9dcf5881`.

`supporting-files.json` separately attributes useful original tests/mocks and the
art compiler recovered from the same source commit, outside the helper's scope.
Their current copies use this repository's paths. Old automation, archives,
reports and unrelated reference-game designs were deliberately excluded.

`integrity.json` protects the immutable patch, original metadata/helper, vision
and licensed source artwork. It is not a live implementation-status document.
