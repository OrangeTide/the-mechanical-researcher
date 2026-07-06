# Map Generation Architecture

Urizen builds maps by composing small generator functions. This doc covers the
data model, the generation pipeline, and how generators are configured, using
`building_roadhouse` as the worked example. The design is described in
language-neutral terms: the reference implementation happens to be in one
language, but nothing here depends on it.

## Overview

Urizen is a procedural generation library for dungeons, buildings, and rooms.
A generator is a routine that returns a `Map`. The same `Map` can then be
handed to a visualizer to produce an image. There is no central engine or scene
graph; generation is plain function composition.

The pipeline for any single map:

```
generator (e.g. building_roadhouse)
    |  validate parameters
    |  resolve materials (keyword or random -> cell type)
    v
room_default(...) -> base Map filled with floor, walled border
    |  build sub-rooms (more Maps) with helper routines
    |  M.meld(sub_room, x, y)            stamp sub-rooms into the base
    |  M.scatter(...) / cell.put(...)    place Things and Actors
    |  optional M.hmirror / vmirror / transpose
    v
return Map
    |
    v
visualizer (tiled / pixel) -> image
```

Everything is code-driven. There are no external building config files.
Configuration here means the arguments passed to a generator plus the tileset
metadata that backs the entity types.

## Component layout

```
core/                 data model + entity loading
  map                 Map: 2D cell grid + geometry ops
  cell                Cell: one square of terrain
  thing               Thing: inert object placed on a cell
  actor               Actor: creature/NPC placed on a cell
  tile                Tile: a single bitmap cropped from a tileset
  metatile            Metatile: named logical entity, 1..N tile variants
  entity_collection   builds the C / T / A namespaces from tilesets
  utils               generator discovery for the GUI
generators/
  rooms/              atomic room templates (room_default, ...)
  buildings/          building_inn (inn + roadhouse), bathhouse, ...
  dungeons/           bsp_tree, drunkard, cellular, grid
  regions/ worlds/ sprites/   stubs for future work
visualizers/
  tiled               tileset-composited rendering
  pixel               one-pixel-per-cell rendering
data/
  tilesets/           sprite sheets + per-tile metadata
  themes/             GUI theme
gui/                  interactive browser over all generators
```

Generators are exposed through a single public namespace, so a caller reaches
them by name, e.g. `building_roadhouse(...)`.

## Core data model

### Map

A `Map` is a width, a height, and a row-major matrix of `Cell` values
(`cells[y][x]`). Indexing is column-first for callers: `M[x, y]` resolves to
`cells[y][x]`.

Operations generators rely on:

- `meld(other, x, y)`: copy every cell of `other` into this map at offset
  `(x, y)`. This is the primary composition primitive. Sub-rooms are built as
  their own small maps and stamped into the parent. It errors if the sub-map
  would overflow the bounds.
- `scatter(x1, y1, x2, y2, entities, exclude)`: collect the empty cells in the
  bounding box (no actors, no things, not excluded), then place a random sample
  of `entities` onto a random sample of those cells. This distributes furniture
  and animals without overlap.
- `hmirror`, `vmirror`, `transpose`: flip horizontally, flip vertically, or
  swap axes. Generators use these to produce orientation variants
  (`direction = up/down/left/right`) and random mirroring from one hand-authored
  layout.
- `bordering(x, y)`, `surrounding(x, y)`: 4- and 8-neighbour lookups, used by
  algorithmic generators (cellular automata) and the tiled visualizer.

### Cell, Thing, Actor

A `Cell` is one square of the map. It carries terrain attributes (passable
flag, colors, tags) plus a `metatile` reference for rendering, and it holds
`things` and `actors` lists. `cell.put(e)` appends a `Thing` or `Actor`.

`Thing` (a barrel, table, bag) and `Actor` (cat, horse, spider) record what
occupies a cell. Both are placed with `put` or `scatter`.

### Tile and Metatile

A `Tile` is a single bitmap cropped from a tileset image, with an orientation
and an animation frame. A `Metatile` is a named logical entity ("wall_block",
"furniture_table") that owns one or more `Tile` variants. Geometry is one of:

- `default`: a single representation.
- `linear`: directional variants (H, V, U, D, L, R, C, ...) chosen at render
  time from a cell's neighbours, e.g. walls and fences that connect.
- `square`: `S0..S3` style variants.

### The C / T / A namespaces

The entity collection is the bridge between the tileset assets and the
generators. At load time it:

1. Reads each tileset's sprite sheet and metadata.
2. For every tile, joins its `group1/group2/group3` properties into a name
   (e.g. `wall` + `block` -> `wall_block`) and reads `type`
   (`cell`/`thing`/`actor`), `orientation`, `frame`, and `tags`.
3. Groups tiles by name into metatiles, choosing geometry from orientation.
4. Builds three namespaces of factories:
   - `C`: cell types (`C.wall_block`, `C.floor_dirt`, `C.door_closed`, ...)
   - `T`: thing types (`T.furniture_table`, `T.bag`, `T.food_meat`, ...)
   - `A`: actor types (`A.animal_cat`, `A.animal_horse`, ...)

Each entry is a factory; a generator invokes it to instantiate, e.g.
`C.wall_block()`. The available terrain, items, and creatures are therefore
defined by the tilesets, not by hand-written enums. Adding a tile with the
right properties to a tileset adds a new `C`/`T`/`A` member with no code change.

## Generators

A generator returns a `Map`. There are two broad styles:

- **Algorithmic** (`dungeons/`): `dungeon_bsp_tree`, `dungeon_drunkard`,
  `dungeon_cellular_simple`, `dungeon_grid_simple`. These compute layout from an
  algorithm (binary space partition, random walk, cellular automata) and are
  largely size-driven.
- **Compositional** (`buildings/`): inn, roadhouse, bathhouse, mansion, kitchen
  garden, shop, bank, stables, house, prison, and others. These assemble a
  hand-designed layout from room templates and place furniture.

`room_default` is the shared base for compositional generators. It returns a
map filled with `floor_type`, bordered by one cell of `wall_type`:

```
function room_default(w, h, wall_type, floor_type):
    M = new Map(w, h) filled with floor_type
    set top row and bottom row to wall_type
    set left column and right column to wall_type
    return M
```

### Generator discovery

A discovery routine walks the `generators` tree and builds an index of what is
available. The GUI uses this index, so a new generator shows up automatically
once added to the tree. Nothing has to be registered by hand.

## Configuration

Configuration lives in two places:

1. **Generator arguments**: each generator's public knobs. Common conventions:
   - `w`, `h`: map size, validated against a per-generator min/max.
   - `wall_material`, `floor_material`: a known keyword, or unset for a random
     choice. The keyword maps onto a cell type in `C`.
   - `direction`: `up | down | left | right`, applied with mirror/transpose at
     the end.
   - generator-specific flags such as `has_exterior` (inn) or `shop_type`.

2. **Tileset metadata**: the asset configuration. It declares which sprite is
   which entity, its name groups, orientation, frames, and tags. It does not
   describe building layouts. It defines the vocabulary of cells, things, and
   actors that generators draw from.

There is deliberately no per-building config file. A building's structure is its
code. Materials and size are the only externally tunable inputs, validated and
resolved inside the generator.

### Configuration layers

The arguments above stack into roughly five layers. A given generator uses
whichever apply; the layers are independent, so they compose freely.

1. **Generator selection.** The caller picks a generator by name
   (`building_inn`, `building_shop`, ...). This is the outermost choice.
2. **Size to algorithm dispatch.** Some generators pick a layout from the
   dimensions rather than a separate flag. `building_prison(w, h)` is a pure
   dispatcher: it routes to a rectangular or linear sub-generator by size, and
   the linear one branches again on orientation.
   ```
   if w >= 17 and h >= 17:  building_prison_rectangular(w, h)
   elif w >= h:             building_prison_linear(w, h)                 # horizontal
   else:                    building_prison_linear(w, h, "vertical")
   ```
   `building_bathhouse` does the same by aspect ratio, choosing a horizontal,
   vertical, or square layout helper from `w` versus `h`.
3. **Named variants sharing helpers.** One module can expose several
   generators that share the same room-builder helpers and configuration
   conventions, differing only in which rooms they assemble and how big they
   are. `building_inn` and `building_roadhouse` are this pattern (worked example
   below); so are `building_prison_linear` and `building_prison_rectangular`.
4. **Orthogonal axes.** Material, content, and orientation are independent of
   the layout and of each other:
   - *material*: `wall_material` / `floor_material`, validated then resolved to
     a cell type.
   - *content*: `building_shop`'s `shop_type` (one of food, jewelry, clothe,
     weapon, armor, potion, tool, magic) does not change the layout at all. It
     is a key into a lookup table of items that `scatter` then places, so a
     weapon shop and a potion shop are the same building with a different data
     row.
     ```
     type_items = { 'food': [...], 'weapon': [...], 'magic': [...], ... }
     M.scatter(2, 1, w-2, 2, type_items[shop_type])      # counter display
     M.scatter(..., type_items[shop_type], exclude=...)  # storage stock
     ```
   - *orientation*: `direction` (`up/down/left/right`) is authored once facing
     one way, then applied as a final `transpose`/`hmirror`/`vmirror`. Used by
     the mansion, kitchen garden, two-room house, and prison cell.
5. **Room template and random fill.** The innermost layer. A room template is
   parameterized by wall/floor type, and a leaf room may pick one furniture set
   at random from a small set of named templates, so the same room varies
   between runs. `room_poor` (used by the inn and roadhouse) does this:
   ```
   room_items = { 'default_room': [...], 'poor_room': [...], 'poor_room_2': [...] }
   M.scatter(..., random pick from room_items, ...)
   ```

The roadhouse below is layer 3 (a named variant) configured by layers 4
(material) and 5 (its room helpers). The prison and shop are called out above
because they show the two contrasting ways a sub-type gets chosen: automatic
dispatch from size, versus an explicit content key.

### Worked example: `building_roadhouse`

The roadhouse is a compact inn variant. It shares the private room-builder
helpers with `building_inn`.

Configuration:

- `w`, `h`: must be in `[15, 21]`; outside this range it errors.
- `wall_material`: one of `block`, `plank`, `brick`, `stone`, or unset for a
  random pick among those four.
- `floor_material`: one of `dirt`, `parquet`, `cobblestone`, or unset for a
  random pick.

Materials are handled with the standard two-step used across building
generators: validate the keyword, then resolve it to a cell type.

```
if w or h outside [15, 21]:
    error "building too small / too big"

if wall_material unset:
    wall_material = random pick from {block, plank, brick, stone}
else if wall_material not in {block, plank, brick, stone}:
    error
wall_material = the C cell type named for that keyword   # block -> C.wall_block, ...

# floor_material resolved the same way against {dirt, parquet, cobblestone}
```

With materials resolved to cell types, the layout is built by composition:

```
M = room_default(w, h, wall_type=wall_material, floor_type=floor_material)
M[13, h-1] = C.door_closed_window()                  # front entrance
M.meld(room_kitchen(w, 6, ...),        0, 0)         # kitchen + storage, top strip
M.meld(room_living(9, h-5, ...),       0, 5)         # guest rooms, left column
M.meld(interior_vending(w-10, h-7,...),9, 6)         # saloon/bar interior, right
return M
```

The helper routines (`room_kitchen`, `room_living`, `interior_vending`, plus
the inn's `room_rich`, `room_poor`, `interior_bar`, `room_outdoor`) each return
their own map and stamp in furniture and creatures with `meld`, `scatter`, and
`cell.put`. For instance `room_kitchen` scatters barrels, boxes, and bags into
the storage corner, places a hearth and tables, and drops a cat with
`M.scatter(6, 1, 14, 5, [A.animal_cat()])`. Because every helper takes the
resolved materials, the whole building stays materially consistent.

The difference between `building_inn` and `building_roadhouse` is purely
compositional. The inn is larger (`[22, 27]`), adds rich private rooms, a
separate bar interior, and an optional outdoor stable yard via `has_exterior`.
The roadhouse omits those and uses a smaller footprint. They share the same
helpers and the same configuration conventions.

### Example invocations

```
M = building_roadhouse()                                          # random materials, 15x15
M = building_roadhouse(w=18, h=18,
                       wall_material="brick",
                       floor_material="cobblestone")
render_tiled(M, scale=3)                                          # tile compositor
```

## Visualization

The tiled visualizer walks the finished map and, for each cell, picks the right
tile from the cell's metatile. For `linear` geometry it inspects the four
neighbours to choose a connecting orientation: a wall between two walls renders
as a straight segment, a corner renders as a corner, and so on. It then
composites every tile into one image. The pixel visualizer is the simpler path:
one colored pixel per cell, scaled up, useful for quick previews.

The visualizer is decoupled from generation. A generator never touches pixels.
It only sets cell types and entities; the tileset metadata and the neighbour
rules decide the final appearance.

### Neighbour-driven tile selection

Auto-connecting walls depend on this rule. For a cell whose metatile is
`linear`, look at the four orthogonal neighbours and ask, for each, whether it
matches this cell's type. That yields four booleans `(up, down, left, right)`,
which index a 16-entry table to a variant name:

```
(0,0,0,0)->H   (0,0,0,1)->L   (0,0,1,0)->R   (0,0,1,1)->H
(0,1,0,0)->U   (0,1,0,1)->RD  (0,1,1,0)->DL  (0,1,1,1)->RDL
(1,0,0,0)->D   (1,0,0,1)->UR  (1,0,1,0)->UL  (1,0,1,1)->URL
(1,1,0,0)->V   (1,1,0,1)->URD (1,1,1,0)->UDL (1,1,1,1)->C
```

"Same type" means the neighbour's type identity matches this cell's. Cells
compare by cell type; things and actors compare against the neighbour's first
thing or actor. Off-map neighbours count as non-matching. If the chosen variant
is missing from the metatile, fall back to a known "unknown" tile rather than
failing. A metatile with several tiles for the same variant picks one at random,
which gives floors and walls visual variety.

## Tileset data format

To render at all you need a sprite sheet plus per-tile metadata. The reference
sets are 12x12 tiles on one image with a 1px margin and 1px spacing between
tiles, described by a sidecar file. A reimplementation can keep this format or
replace it wholesale; only the loader cares.

Per sheet the loader needs: column count, tile width, tile height. The pixel box
for tile `index` is:

```
x = (index mod columns) * (tilewidth  + spacing) + margin
y = (index div columns) * (tileheight + spacing) + margin
crop the rect (x, y, x + tilewidth, y + tileheight)
```

Per tile the loader reads:

- `type`: `cell`, `thing`, or `actor`. Selects which namespace (C/T/A) the
  entity lands in. Tiles with no usable name are skipped.
- `group1`, `group2`, `group3`: joined with `_`, skipping empties, to form the
  entity name (`wall` + `block` -> `wall_block`). Tiles sharing a name are
  merged into one metatile.
- `orientation`: absent -> `default` geometry; starts with `S` -> `square`;
  otherwise `linear` (and the value is the variant name, e.g. `H`, `C`, `RDL`).
- `frame`: present -> the metatile is animated and this tile belongs to that
  frame; absent -> static.
- `index`: position of this tile within its variant list (for the random pick).
- `tags`: comma-separated labels, carried onto the entity for tag queries.

## Implementation notes

Details the reference implementation exposed that are easy to get wrong in a
port:

- **Per-cell collections must be owned, not shared.** Scalar cell attributes
  (terrain, colors, metatile, passable) can be shared defaults on the type, but
  each cell needs its own `things` and `actors` lists. Aliasing one list across
  every cell is the most common porting bug. Tags are read-only and may be
  shared.
- **Cells carry a type identity.** Neighbour matching for `linear` tiles
  compares type identity (the reference uses the type name). Store a stable type
  id or name on every cell so neighbour comparison is cheap.
- **`meld` transfers cells, it does not deep-copy terrain.** Sub-rooms are
  throwaway scratch maps, so moving or shallow-copying their cells into the
  parent is fine and avoids per-cell duplication. In a manually managed language
  decide ownership explicitly: move the cells out of the sub-map, or copy and
  free the sub-map.
- **Render precedence is actor, then thing, then cell.** Each square shows one
  sprite. Only the first actor (else first thing, else the cell) is drawn, even
  though a cell may hold several. Generation can stack many; rendering shows the
  top of that stack.
- **Edge neighbour queries return a sentinel, they do not wrap or error.** The
  four directional lookups return an "unknown/off-map" value at the borders, so
  walls correctly terminate at the map edge instead of connecting across it.
- **Two sentinel types do real work.** `void` is an empty/transparent cell used
  as a base fill before stamping real rooms in (build a void shell, then `meld`
  rooms onto it). `unknown` is the render fallback when a variant is missing.
  Reserve both.
- **`scatter` is sample-without-replacement on both sides.** Gather the free
  cells in the box (no thing, no actor, not excluded), then pair a random subset
  of cells with a random subset of entities, count = min of the two. Duplicates
  in the entity list raise an item's frequency; surplus entities are dropped
  when cells run short. Nothing overlaps.
- **Doors and stairs are cells, not things.** The taxonomy is: terrain, doors,
  and stairs are cell types placed by direct assignment; furniture and items are
  things; creatures are actors. Keep the split, since rendering precedence and
  neighbour matching treat the three differently.
- **Thread an explicit RNG/seed.** The reference leans on a global RNG and seeds
  only the visualizer, which makes generation hard to reproduce. Pass a seeded
  generator through both generation and rendering if you want deterministic
  output.

## Minimum viable path

Enough to get a rendered map, in order:

1. `Map` with `[x, y]` access over a `(y, x)` store, plus `meld`, `scatter`,
   `put`, and the four neighbour lookups. `hmirror`/`vmirror`/`transpose` can
   come later; they only add orientation variants.
2. `Cell`/`Thing`/`Actor` records carrying a type identity and, for cells, owned
   thing/actor lists.
3. A handful of cell types and a `room_default` (floor fill + wall border).
   At this point one compositional generator will already produce a valid map.
4. A tileset loader and the neighbour-driven selection above, or skip straight
   to the pixel visualizer (one color per cell type) to see results before
   building any tile pipeline.

The tiled path is the same map with sprite lookup and the orientation table
layered on top.

---
Made by a machine. PUBLIC DOMAIN (CC0-1.0)
