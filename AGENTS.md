# AtlasGBA

Read `README.md`, then `docs/atlas/DEVELOPMENT.md`. Read the relevant part of
`docs/atlas/FULL_PLAN.md` for game intent. Source and executed tests establish
implementation; the vision does not establish completion.

## Non-negotiable destination

Write only to **ArmenionCH2/pokeemerald-expansion**. `rh-hideout/pokeemerald-expansion`
is strictly read-only. `despairarc000/pokeemerald-expansion` is a different game;
`despairarc000/atlasgba` is read-only provenance. Never submit upstream PRs.
Before any remote write, verify repository owner, connection push permission,
remote URL and current destination head. GitHub connectors bypass local hooks:
pass the exact target repository explicitly on every mutation. Stop on missing
permission; never substitute another repository. The owner prefers direct tested
commits to `master`, without PRs or long-lived feature branches. Never force-push.

## Working loop

1. Inspect `git status --short --branch`, `git log -5 --oneline`, remotes and
   worktrees. Preserve other people's changes. Run `python3 dev_scripts/atlas.py setup`.
2. Search narrowly: `rg 'AtlasWorld_' src include`, `rg --files src -g 'atlas*'`.
   Use source callers and tests instead of inventing a parallel implementation index.
3. Keep Expansion interfaces and features intact when adapting old code. The
   recovery patch is evidence, never an overlay to apply to this engine.
4. Run the relevant host contracts and ARM build. For gameplay/save changes also
   run emulator checks. Distinguish host mocks, compiled ROM and observed gameplay.
   Back up saves; do not silently migrate unknown versions or reuse flash sectors.
5. Review `git diff --check` and staged paths; commit concrete behavior and evidence.
   A commit message records validation and unresolved limitations. Update a guide
   only if its instructions changed. No mandatory diary/status mirrors.

Keep experiments, reports, screenshots, ROMs, saves and generated indexes under
ignored `build/`. Use a temporary local worktree for risky experiments; remove it
when finished. One active integration branch is sufficient. Do not create archive
branches, mandatory task boards, model-specific memories, or copied old engines.
Keep attribution and immutable provenance. Do not import reference-project features.
