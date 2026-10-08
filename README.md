# mod-world-journey

An AzerothCore module for Conquest of Azeroth: the whole world - the old world, Outland and Northrend - as one
journey from level 1 to 60.

The classes of Conquest of Azeroth end at level 60, so Outland and Northrend, made for levels 58 to 80, are out of
their reach. This module compresses the world instead of stretching the classes: the road from 1 to 60 runs through
all of it - the old world first, then through the Dark Portal, then to Northrend - and every zone stays open until 60.
The classes stay exactly as CoA made them.

| Part of the world | Zones open from (original) | Zones open from (with this module) |
|---|---|---|
| Old world | 1 to 55 | 1 to 35 |
| Outland | 58 to 67 | 30 to 43 |
| Northrend | 68 to 77 | 40 to 55 |
| Raids and heroic dungeons of Outland and Northrend | 70 / 80 | 60 |

The order of the zones within each part is the original one. A character above a zone's level meets its creatures at
their own level, lifted by the open-world scaling of CoA as always, so a zone left out on the way can still be played
later. The full table of zones and instances is in `data/zones.csv` and `data/instances.csv`, built by
`tools/make_levels.py` from the zone ranges of the CoA client and the levels in the CoA world database.

**Status: test version.** Compiled, and its database part run against a copy of a CoA world database (the result
checked, run twice, switched off and back on); not yet played on a server.

## What follows the journey

- **Creatures.** Their levels, by the map they are found on and the level they were made for, and their money with
  them. On the way, a creature of Outland or Northrend takes the base stats of the old world, which rise evenly to
  60. The bosses of the old world and the classic raids stay as they are.
- **Their spells.** What a creature casts hits with the numbers of the level it was made for; the module scales that
  damage down with the level.
- **Quests.** Quest level, minimum level and money, by the map of whoever starts the quest. A quest whose reward asks
  for more - one that leads into a raid - stands at the level of its reward.
- **Items.** Required level, item level, stats, armor, weapon damage and price. The level an item of Outland or
  Northrend was made for is read from its item level (most of Northrend's gear asks for 68, whatever zone it comes
  from), and its stats become those of an item of its new level. The stat budgets come from the server's own
  `dbc/RandPropPoints.dbc`.
- **The end of the journey** is a ladder at 60, the way the games followed each other: the raids of the old world
  (item levels 66 to 92), then Outland's (up to about 110), then Northrend's (up to 140). The raids and heroics of
  Outland and Northrend keep the stronger base stats of their game, so they are the harder end.
- **Potions, food, elixirs, flasks, trinkets and procs.** The spells of an item are scaled with it; a potion of
  Northrend heals what a potion of its new level should.
- **Gems and enchantments** of Outland and Northrend - from gems, socket bonuses, enchanting and the other
  professions - by what the top rewards of their game keep of their stats.
- **Riding and the professions.** Their training levels: riding at 13 and 26, flying at 33 and 47, cold weather flying
  at 55, the grand master of a profession at 40. The class trainers are not touched.
- **Instances and the Dungeon Finder.** The entry levels of the dungeons, and the Dungeon Finder offers every dungeon
  - Outland's and Northrend's included - from its new entry level to 60.
- **Battlegrounds.** The Eye of the Storm, the Strand of the Ancients, the Isle of Conquest and Wintergrasp open at 60.
- **Bots.** With mod-playerbots, the random bots stop at 60, go to each zone at its new levels and mount and fly when
  the players can.
- **The world map** of the client shows the new zone levels, with the small addon `client/AddOns/ZoneLevels`.

## Reversible

Every value the module changes in the world database is kept, with its original, in tables of the module
(`journey_creature`, `journey_quest`, `journey_item`, `journey_trainer_spell`, `journey_battleground`). An update of
the database that brings a new value is noticed and taken as the new original. Switched off (`Journey.Enable = 0`),
the module puts every original back at the next start. Spells, gems, enchantments, the Dungeon Finder and the
battleground brackets are only changed in memory.

The first start with the module takes about a minute longer than usual; later starts about 15 seconds.

## Known limits

- **Tooltips of spells, gems and enchantments** are written in the client and still show the original numbers. What
  counts is what the server applies: the character sheet shows the real values. Items themselves show their new
  values once the client's cache is cleared (see below).
- **The Dungeon Finder window** of the client may still show the original level ranges of Outland's and Northrend's
  dungeons; the server decides who may queue.
- **The PvP window** of the client may still show the Eye of the Storm and the newer battlegrounds as closed below
  their original level; the battlemasters let a character of 60 in.
- **Balance** is worked out from the data, not played: how hard the raids of Outland and Northrend are at 60, and how
  strong their rewards are, will need a look in the game.
- The consumables of the old world move with it: a Flask of the Titans now belongs to the middle of the journey. At 60,
  the consumables of Outland and Northrend are the strong ones.

## Requirements

- AzerothCore with the Conquest of Azeroth changes (`jealous-sound/azerothcore-wotlk-coa`), including its open-world
  scaling.
- In worldserver.conf: `MaxPlayerLevel = 60`, `DungeonFinder.MaxExpansion = 2`, `Wintergrasp.PlayerMinLvl = 60`.
  With mod-playerbots, in playerbots.conf: `AiPlayerbot.RandomBotMaxLevel = 60`, the mount levels and a zone bracket
  for every zone. `afk-realm.json` lists them all with their values.

## Installation

With [AFK Realm](https://github.com/aspollon/AFK-Realm): add the module there; it sets the values above.

By hand: clone the module into `modules/`, rebuild, copy `conf/mod_world_journey.conf.dist` next to the other module
configs as `mod_world_journey.conf`, and set the values listed in `afk-realm.json`.

**The client:** copy `client/AddOns/ZoneLevels` into `Interface/AddOns` of the game client, and delete the client's
`Cache` folder once, so that it forgets the old levels of items, creatures and quests.

Bots that are above 60 already stay there until the random bots are reset.

## Renaming the module

Its tables and config options do not carry the module's name. To rename it, rename the folder and
`src/mod_world_journey_loader.cpp` with the function in it (the core calls `Add<folder>Scripts`), and the config file.

## License

GNU AGPL v3, like AzerothCore.
