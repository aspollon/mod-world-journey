# mod-world-journey

An AzerothCore module for class systems made for level 60 only - above all the 21 classes of
**Conquest of Azeroth** (CoA): the whole world - the old world, Outland and Northrend - as one journey from 1 to 60.

## The concept

The classes of CoA end at 60: their talents, stats and most spell ranks stop there. Outland and Northrend, made for
levels 58 to 80, are out of their reach - and raising the level cap would stretch the classes past what they were
made for. This module compresses the world instead: the road from 1 to 60 runs through all of it - the old world
first, then through the Dark Portal, then to Northrend - and the classes stay exactly as CoA made them.

| Part of the world | Zones open from (original) | Zones open from (default) | Regulator |
|---|---|---|---|
| Old world | 1 to 55 | 1 to 35 | `Journey.Classic.Range` |
| Outland | 58 to 67 | 30 to 43 | `Journey.Outland.Range` |
| Northrend | 68 to 77 | 40 to 55 | `Journey.Northrend.Range` |
| Raids and heroics of Outland and Northrend | 70 / 80 | 60 | `Journey.Endgame.Level` |

The order of the zones within each part is the original one; single zones and instances can be set by hand. Every zone
stays open until 60: a character above a zone meets its creatures lifted to its own level by CoA's open world
scaling, each one in a **window** around the character - the weakest of a zone two levels below, the strongest two
above, elites and rares one more - so that a zone left out on the way is still worth playing later.

Everything is a regulator in `mod_world_journey.conf`. Change one and restart: the journey is computed anew from the
originals.

**Status: test version.** Compiled against the CoA core; its database part run against a copy of a CoA world database
(the result checked, run twice, switched off and back on, every original restored). Not yet played on a server.

## What follows the journey

- **Creatures.** Their levels, by the map they are found on and the level they were made for, and their money with
  them. On the way, creatures of Outland and Northrend take the base stats of the old world, which rise evenly to 60
  (`Journey.Creatures.BaseStats`). The bosses of the old world and the classic raids stay as they are.
- **Difficulty.** Multipliers for health, damage, spell damage and armor by rank (normal, elite, rare elite, world
  boss, rare) and part of the world; rares are stronger by default. Per map or creature in the table
  `journey_difficulty`. They are written into `creature_template`, so CoA's scaling honours them.
- **Their spells.** What a creature casts hits with the numbers of the level it was made for; the module scales that
  damage down with the level, and by the difficulty of its spells.
- **Quests.** Quest level, minimum level and money, by the map of whoever starts the quest; quests of zones set by
  hand move with them. A quest whose reward asks for more - one that leads into a raid - stands at the level of its
  reward. Experience per part of the world: `Journey.Quests.XpRate`.
- **Items.** Required level, item level, stats, armor, weapon damage and price. The level an item of Outland or
  Northrend was made for is read from its item level, and its stats become those of an item of its new level (stat
  budgets from the server's `dbc/RandPropPoints.dbc`). This happens in `item_template` itself, so it holds for every
  source of an item: loot, quests, vendors, the Trading Post, the auction house.
- **The end of the journey** is a ladder at 60, the way the games followed each other: the raids of the old world
  (item levels 66 to 92), then Outland's (75 to 110), then Northrend's (95 to 140) - both configurable. The raids and
  heroics of Outland and Northrend keep the stronger base stats of their game: they are the harder end.
- **Potions, food, elixirs, flasks, trinkets and procs.** The spells of an item are scaled with it; a potion of
  Northrend heals what a potion of its new level should.
- **Gems and enchantments** of Outland and Northrend - from gems, socket bonuses, enchanting scrolls and the other
  professions - by what the top rewards of their game keep of their stats.
- **Riding and the professions.** Riding at 13 and 26, flying at 33 and 47, cold weather flying at 55, the grand
  master of a profession at 40. The class trainers are not touched.
- **Instances and the Dungeon Finder.** The entry levels of the dungeons, and the Dungeon Finder offers every dungeon -
  Outland's and Northrend's included - from its new entry level to 60.
- **Battlegrounds.** The Eye of the Storm, the Strand of the Ancients, the Isle of Conquest and Wintergrasp open at 60.
- **Bots.** With mod-playerbots, the random bots stop at 60, hunt in each zone at its new levels - Outland and
  Northrend included - and mount and fly when the players can.
- **The client.** The world map shows the new zone levels, and the tooltips the real values of an item's spells,
  gems and enchantments.

## What the module checks

The journey ends at 60. With `Journey.Guard.Enable` (default) the realm runs with `MaxPlayerLevel 60`, the Dungeon
Finder with the dungeons of Outland and Northrend, and Wintergrasp from 60 - whatever worldserver.conf says, with a
warning in the log. With the playerbots patch, the random bots are kept at 60 too (a higher
`AiPlayerbot.RandomBotMaxLevel` is lowered, with a warning).

## CoA's scaling, and what the module adds to it

CoA scales the open world for a character who switched it on at the Destiny Weaver: creatures below the character
are **lifted** to it (never lowered), `DestinyWeaver.Scaling.Offset` levels below; dungeons scale both ways.

- `Journey.Scaling.Forced = 1` (default): every character plays the scaled world from its creation, and the Destiny
  Weaver no longer offers to switch it off.
- `Journey.Window.*`: lifted creatures keep their place in their zone instead of all standing at the same level.

Both need `patches/core-level-window.patch`. Without it the module still works; scaling stays each character's
choice, and lifted creatures stand where CoA puts them.

The module itself lowers: it compresses the world database for everyone, scaling on or off. CoA's scaling then lifts
what is left behind.

## Reversible

Every value the module changes in the world database is kept, with its original, in tables of the module
(`journey_creature`, `journey_quest`, `journey_item`, `journey_trainer_spell`, `journey_battleground`,
`journey_spell`). An update of the database that brings a new value is noticed and taken as the new original.
Switched off (`Journey.Enable = 0`), the module puts every original back at the next start, and removes the rows it
added to `spell_dbc`. Gems, enchantments, the Dungeon Finder and the battleground brackets are only changed in memory.
The tables `journey_zone` and `journey_bot` are written at every start for the bots.

The first start with the module takes about a minute longer than usual; later starts about 15 seconds.

## Known limits

- **Tooltips.** The spells of items are written into `spell_dbc` and reach the client with CoA's spell patches, so
  their tooltips are right. Gems and enchantments are corrected by the addon `ZoneLevels`; without it they show the
  old numbers (what counts is what the server applies). Items show their new values once the client's cache is
  cleared.
- **The Dungeon Finder window** of the client may still show the original level ranges of Outland's and Northrend's
  dungeons; the server decides who may queue.
- **The PvP window** may still show the newer battlegrounds as closed below their original level; the battlemasters
  let a character of 60 in.
- **Balance** is worked out from the data, not played: how hard the raids of Outland and Northrend are at 60 and how
  strong their rewards are will need a look in the game - that is what the difficulty regulators are for.
- The consumables of the old world move with it: a Flask of the Titans now belongs to the middle of the journey.

## Requirements

- AzerothCore with the Conquest of Azeroth changes (`jealous-sound/azerothcore-wotlk-coa`).
- Optional: mod-playerbots (CoA branch) with `patches/playerbots-world-journey.patch`.

## Installation

**With [AFK Realm](https://github.com/aspollon/AFK-Realm):** add the module. AFK Realm reads `afk-realm.json`: it
applies the two patches, and sets `MaxPlayerLevel`, `DungeonFinder.MaxExpansion`, `Wintergrasp.PlayerMinLvl`,
`AiPlayerbot.RandomBotMaxLevel` and `DestinyWeaver.Scaling.Offset` in the other config files.

**By hand:**

1. Clone the module into `modules/`.
2. Apply the patches: `git apply modules/mod-world-journey/patches/core-level-window.patch` in the core, and
   `git apply ../../mod-world-journey/patches/playerbots-world-journey.patch` in `modules/mod-playerbots`.
3. Rebuild, and copy `conf/mod_world_journey.conf.dist` next to the other module configs as `mod_world_journey.conf`.
4. Set the values listed under `settings` in `afk-realm.json`.

**The client** (each player): copy `client/AddOns/ZoneLevels` into `Interface/AddOns` of the game client, and delete
the client's `Cache` folder once, so that it forgets the old levels of items, creatures and quests.

Bots that are above 60 already are brought back to 60 by Playerbots at their next reset.

## Renaming the module

Its tables and config options do not carry the module's name. To rename it, rename the folder and
`src/mod_world_journey_loader.cpp` with the function in it (the core calls `Add<folder>Scripts`), and the config file.

## License

GNU AGPL v3, like AzerothCore.
