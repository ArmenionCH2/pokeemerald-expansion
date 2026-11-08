# AtlasGBA — Complete Gameplay Vision and Feature Plan

**Purpose.** This document describes the intended *game*, not its implementation, code architecture, development workflow, or the current completion status. It consolidates the AtlasGBA vision and historical feature ideas from the original project, with later clarifications taking precedence when older ideas conflict. Unresolved ideas are explicitly identified as open; they are not instructions to implement everything immediately.

**Project identity.** AtlasGBA is a Pokémon-inspired, top-down, open-world living-world game. It is a separate game project from other Pokémon Expansion projects on the owner's accounts. Pokémon Emerald Expansion is the intended future foundation for integrating AtlasGBA's custom gameplay, not evidence that another fork's story, mechanics, regions, or design rules apply here.

## 1. The experience

- Explore an enormous, coherent, Earth-like procedural world rather than a collection of disconnected, randomly decorated rooms.
- The world is not built around a compulsory Emerald storyline, forced linear region order, or repeated random-grass encounters.
- Discovery, travel, companionship, observing wildlife, learning geography, and affecting the environment are central activities.
- The same place remains recognizably the same when revisited: continents, routes, lakes, climate patterns, names and world identity must not change because the player loaded a chunk in a different order.
- The world and its inhabitants have their own ongoing existence. The player's arrival is not the cause of the ecosystem, resources or geography appearing.
- Preserve the familiar feel of classic GBA Pokémon exploration wherever it serves the vision, while extending it into a wider living world.

## 2. Earth-scale world and persistent identity

- A procedurally generated planet-like setting with enormous traversable land and sea, broad latitude zones, multiple continents and islands, connecting passages, mountain systems, valleys, rivers, lakes and distinctive local environments.
- Earth-like physical relationships matter more than merely scattering terrain tiles: mountains shape travel and rainfall; river water follows coherent drainage; wet basins support wetlands; different rock and climate contexts lead to different landforms.
- Longitudes connect when traveling east or west, so a journey around the world can return to where it started. North and south are polar regions; simply wrapping from north to south is not the intended behavior.
- The earlier Atlas design described a cylinder-like longitude-wrap/latitude-cap gameplay world, not a geometrically exact spherical simulation. More faithful polar traversal or projection can be considered separately; it must not be assumed complete.
- One historical reference scale is approximately 40,075,000 east-west by 20,003,000 north-south logical tiles, roughly one metre per tile. This records the intended *scale* and is not a requirement to preserve a particular technical grid, memory allocation, or map adapter.
- The world has a persistent seed and generation identity. Traveling, viewing a map, and revisiting cannot change its underlying unmodified terrain.
- Distinguish the stable world baseline from changes caused by players, Pokémon, natural recovery and large events. Existing changes and exploration history should survive saving and returning, within honest platform limits.
- Worlds should ultimately be exportable/shareable while retaining their identities and their deliberate changes. Future multiplayer/shared-world play is an intended extension, not a claimed existing feature.
- Internal map chunks are invisible streaming/storage units, never player-facing artificial borders or named geographic divisions.

## 3. Geography, landforms and waters

- Diverse large-scale patterns: continents, ocean basins, archipelagos, peninsulas, inland seas, coasts, plateaus, mountains, ridges, foothills, passes, plains, valleys and canyons.
- Connected hydrology: headwaters, rivers, tributaries, confluences, lakes, outlets, wetlands, floodplains and river mouths that fit their surrounding terrain.
- Coasts can have beaches, bays, headlands, cliffs, estuaries, deltas, shoals, islands and lagoons where their geography warrants them.
- Other meaningful environments may include dunes and rocky deserts, mesas, badlands, seasonal or permanent snow, alpine/glacial forms, volcanoes/lava fields, karst/sinkholes, springs and cave networks. Rare landmarks should actually be rare and geographically plausible.
- Physical classification is contextual: a valley is lower than its surroundings, a plateau is relatively flat and elevated, a wetland depends on drainage/water, and a snowy mountain needs suitable climate. No single elevation threshold alone defines every feature.
- Terrain art, navigability, collision and environment should tell a coherent visual story. Shorelines, riverbanks, mountain openings, cliffs and other transitions should connect naturally.
- Geographic variety should emerge across scales: broad continent and climate patterns, regional features, and detailed local terrain without abrupt chunk seams.

## 4. Climate, biomes, weather and time

- Climate depends on location and environmental causes, including latitude, elevation, proximity to water, landform exposure and seasonal patterns.
- Treat temperature, moisture, precipitation, soil wetness, water and landform as related but distinct aspects of the world. A hot mountain is not automatically a desert; wet air does not automatically mean permanent swamp.
- Broad ecological regions can include forests, jungles, grasslands, savannas, deserts, taiga, tundra, wetlands and aquatic areas; transitions should make environmental sense.
- Day/night, seasonal variation and weather are desired over time. Weather can bring rain, snow, fog, warmth, cold and hazards; a short storm should not instantly redefine a biome.
- Conditions should remain consistent across nearby places and across saves/revisits. The world should not randomly reroll its weather whenever a location becomes visible.
- Water and climate affect terrain, vegetation, resources, Pokémon habitat, movement and world events.
- No claim is made that AtlasGBA currently has full seasons, weather dynamics, geological simulation or ocean currents; these belong to the overall vision.

## 5. Physical geographic regions and naming

- Geographic places are defined by the world, not arbitrary numbered chunks or political borders.
- A historical naming hierarchy was **Continent → Territory → Region → Tract → Area → unnamed Chunk → unnamed Tile**. These are conceptual scales, not mandatory rectangular boxes or countries.
- Places may be named based on coherent physical features: mountain ranges, valleys, forests, coasts, islands, gorges, wetlands and other real structures in the generated world.
- A named feature can cross many chunks; rivers, mountains, watersheds and ecological regions can overlap. The world must not be cut into implausible geographic pieces solely to satisfy a grid size.
- Names and place identities remain stable, independent of discovery order.
- When entering a genuinely new named place, give a concise location breadcrumb, omitting higher-level geographic names that have not changed. Avoid repetitive announcements on a borderline.
- Names emphasize physical geography and the experience of discovering a living world; arbitrary political jurisdictions are not part of the original Atlas vision.

## 6. Traversal, world interaction and verticality

- Walk, run, explore shores and mountains, navigate rivers and seas, and discover practical routes through natural obstacles.
- Meaningful elevation and terrain shape movement. The historical vision includes small automatic hops, deliberate climbing, ladders, stairs, slopes, caves and other terrain-dependent traversal.
- A layered world is desired: surface, underground/caves and suitable elevated or aerial/underwater areas. This need not be visually represented as fully continuous 3D; player-facing continuity matters.
- Bridges, passes, cave entrances, water routes and other connections should make exploration feel physically grounded.
- Above-ground overhangs and roofs should not make the player's position unreadable; interior/cave presentation should remain clear.
- Pokémon with relevant abilities can make different terrain accessible: swimming, flying, burrowing, riding and other species-appropriate movement, with natural limitations.
- Terrain may be transformed, destroyed or constructed by applicable moves and environmental events. Examples: fire scorches grass, cold freezes water, water creates wet patches, suitable moves raise rock or encourage plant growth.
- Terrain changes can be temporary, condition-dependent or persistent. Their recovery follows plausible environmental causes rather than arbitrary resets.
- Wild Pokémon can also affect the environment through their own behavior; the world is not exclusively editable by the player.
- Camps, shelters or bases may emerge through world interaction and persistent alterations rather than relying on a required predefined player-home system.

## 7. Exploration map and discovery

- The world map is a separate exploration view, not merely a zoomed duplicate of the player camera.
- The map can span the planet, zooming from regions/chunks down to individual tile-scale geography where appropriate.
- A familiar Pokémon-style cursor and intuitive pan/zoom interaction are part of the intended experience.
- Scroll beyond the currently visible viewport and progressively reveal the appropriate world data without treating the edge of the screen as an edge of the world.
- The **player map** conceals never-explored areas in black and reveals places as they are genuinely visited; map zoom must not accidentally reveal unknown locations.
- The **admin/developer map** can view generated geography without player fog restrictions, but must not falsify or overwrite exploration progress.
- Map labels, geographic identity, navigation and historical visits should be coherent with the real world representation.
- Diagnostic facilities such as walk-through-walls or developer HUDs are temporary development conveniences, not permanent game mechanics or progress substitutes.

## 8. Visible Pokémon and ecology

- Wild Pokémon should appear as living individuals in the overworld. The desired experience does not depend on conventional random battles triggered by tall grass.
- They move, rest, forage, respond to hazards, avoid threats, defend suitable territory and interact with other Pokémon and the player according to species and individual tendencies.
- Species appear where the environment makes sense: climate, altitude, water, landforms, vegetation, time and local resources contribute to habitat suitability. Rarity reflects genuine ecological constraints and scarcity, not merely hidden roll tables.
- Each important creature can have its own identity, age/history, temperament, preferences, memories, movement abilities, needs and relationship history.
- The player should be able to observe, approach, feed, help, befriend and potentially travel with Pokémon without being forced into battle or traditional capture as the only means of companionship.
- A Pokémon's behavior should be understandable as a response to needs, opportunities, memories, the environment or orders rather than arbitrary player-triggered scripts.
- Ordinary Pokémon may compete over food or territory. A historical clarification favors **berry/plant-based resource competition instead of Pokémon preying on one another**, with avoidance, dispute and migration possible responses.
- Wildlife populations should exist independently of visits. Off-screen activity should continue in a believable, appropriately abstracted way instead of stopping all life outside the camera.
- Ecological changes can include births/eggs, growth, migration, scarcity, population shifts and habitat changes. Rich ecology is a long-term goal and must remain appropriately scoped to the platform.

## 9. Companionship, trust and Pokémon growth

- Trust develops through repeated interactions and individual experience. Feeding, kindness, shared time and protection may strengthen relationships; harm, neglect or witnessed danger may weaken them.
- Personality affects how quickly an individual trusts, flees, cooperates or reacts. A timid creature can be harder to approach than a bold one.
- Relationships should also exist between Pokémon, not exclusively between each Pokémon and the player.
- Long-term ambition includes a broad Pokémon species roster, with individual species behaviors and habitat requirements rather than only a small selection of familiar encounters.
- Following, staying, riding and other cooperative behaviors become available when the particular relationship, creature and situation support them.
- Memory and place history should matter to behavior, travel and relationship development. Pokémon capable of teleportation should not automatically know every place in the world; an earlier idea limited destinations to places that particular Pokémon had actually visited.
- Growth need not revolve around repetitive XP grinding. The historic concept emphasizes age/maturity, activity, practice, ability development and meaningful conditions for evolution.
- Evolution and reproduction/egg systems are part of the long-term game vision. Their precise mechanics should be reconciled with the selected game foundation and available resources rather than assumed already decided.
- A historical proposal distinguishes ordinary fainting from severe incidents, with rare lasting consequences and a **dormant** state preserving individual identity/history. The exact revival mechanism remains open.
- Individual long-term histories must be bounded realistically by device/storage constraints, but the gameplay goal remains that meaningful individuals and relationships can persist.

## 10. Interaction, moves and conflict

- Familiar Pokémon-style controls and quick contextual actions are desired: move, interact, cancel/run, use held items, throw or use them, and potentially simple close-range actions.
- A single interaction may have different natural outcomes depending on the subject, held item, relationship and surroundings. A berry might be eaten, planted or offered; a particular Pokémon might be petted, fed, followed or ridden when appropriate.
- Pokémon moves can have more than battle effects: affecting terrain, assisting travel, offering protection, changing environmental conditions, helping others, or enabling interactions.
- Effects may include damage, temporary changes in ability, recovery, growth, teleportation, world transformation and trust consequences.
- Combat/conflict can exist in the living overworld, but the world is not only a battle arena. Its consequences should be part of the same persistent world instead of a disconnected minigame.
- Avoid encounters with no underlying cause, magical knowledge of unseen opponents, arbitrary invalid targets, or events that occur merely because the game wants drama.

## 11. Legendaries and world events

- Legendary/mythical Pokémon are exceptional, persistent individuals rather than randomly respawning ordinary encounters.
- Their appearance and activity can have far-reaching consequences for weather, geography, other creatures and player travel.
- Large events should propagate through the same world rules that govern smaller events where feasible, so the affected landscape is visible and consequential.
- Significant environmental damage and eventual recovery can persist or evolve; it should not be automatically wiped when a chunk unloads.
- Their exact placement, reawakening, revival, rarity, story framing and world consequences remain design topics rather than proofs of existing features.

## 12. Player identity, survival and continuity

- The player is an individual traveling in a world that exists independently of them.
- The historical Atlas concept includes player durability, stamina, hunger and life force, with needs and danger influencing travel without reducing the entire game to survival micromanagement.
- A portal-like arrival narrative, optional real-world-geolocation-inspired initial spawn, and matching initial local day/night were explored as ideas. GPS/location integration is **not** a requirement of the GBA engine and must be treated as optional future design, not a current capability.
- The original historical design explored death, a brief otherworldly waiting scene, and reappearance in a familiar visited area weighted by time spent there. This is a **historical gameplay proposal** requiring renewed approval, not a statement about current save or respawn implementation.
- The player's exploration history, deliberate transformations, connections and meaningful experiences should persist across play sessions wherever feasible.
- Safe starting conditions should be naturally discoverable and compatible with the world rather than achieved by fabricating a perfect meadow that contradicts the generated geography.

## 13. Exported and shared worlds

- The long-term vision includes sharing/exporting world identities and carrying deliberate modifications or persistent progression between compatible instances.
- A later multiplayer/shared-world model should aim for consistent terrain, objects, environmental state and relevant creatures for all participants.
- Cooperative exploration and emergent social interactions are possibilities; no particular network protocol, authoritative server or multiplayer feature set is committed here.
- Local single-player world identity and persistence are valuable even before any shared-world features exist.

## 14. Visual and player-experience direction

- Preserve recognizable 2D pixel-art GBA exploration and smooth everyday movement.
- Favor terrain that looks naturally connected: continuous riverbanks, coastlines, mountains, vegetation gradients, safe-looking foothills, coherent objects and visible wildlife.
- Use convincing world art only where it is legitimately available and properly licensed. The existing AtlasGBA Puny World source/credits are examples of previously added materials, not proof of blanket permission to use other ROM hacks' artwork.
- Readable UI, map labels and exploration feedback matter more than technical complexity the player cannot see.
- No debug-only illusion should be represented as finished gameplay.

## 15. What this plan deliberately does not decide

This is a *feature-vision document*. It deliberately does not dictate source architecture, how Pokémon Emerald Expansion should be modified, a folder structure, specific build machinery, AI staffing, branch policy, or numerical RAM/save limits. The eventual implementer must inspect the actual platform and preserve what genuinely works. Important future decisions include exact world dimensions/projection, biome balance, weather detail, cave/vertical gameplay, saved-individual capacity, evolution/reproduction rules, encounter/combat emphasis, player death/return, revival of dormant Pokémon, and multiplayer scope.

**Interpretation rule:** Distinguish a *desired gameplay capability* from an *implemented feature*. The existing AtlasGBA source is the evidence of what was actually written. This plan is the evidence of what the project intends. The source and this vision should not be confused with the completely separate living-world Pokémon Expansion fork under a different project.
