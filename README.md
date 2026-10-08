# mod-ageless-sixty

An AzerothCore module for Conquest of Azeroth: the whole world - the old world, Outland and Northrend - as one
journey from level 1 to 60.

The classes of Conquest of Azeroth end at level 60, so Outland and Northrend, made for levels 58 to 80, are out
of their reach. This module compresses the levels of the world so that the road from 1 to 60 runs through all of
it - the old world first, then through the Dark Portal, then to Northrend - and every zone stays open until 60.

| Part of the world | Zones open from (original) | Zones open from (with this module) |
|---|---|---|
| Old world | 1 to 55 | 1 to 35 |
| Outland | 58 to 67 | 30 to 43 |
| Northrend | 68 to 77 | 40 to 55 |
| Raids and heroic dungeons of Outland and Northrend | 70 / 80 | 60 |

The order of the zones within each part is the original one. A character above a zone's level meets its creatures
at their own level, lifted by the open-world scaling of CoA as always. The full table of zones and instances is in
`data/zones.csv` and `data/instances.csv`, built by `tools/make_levels.py` from the zone ranges of the CoA client and
the levels in the CoA world database.

**Status: early test version.** Compiled, and its database part tried on a copy of a CoA world database; not yet
played on a server.

## How it works

- **Creatures.** At every start, before the world reads its data, the levels of the creatures in the world database
  are set to their compressed values - by the map they are found on and the level they were made for - and their
  money with them. Their health, armor and blows follow from the level by themselves. The bosses of the old world
  and the classic raids stay as they are.
- **Spells.** What a creature casts hits with the numbers of the level it was made for; the module scales that damage
  down with the level.
- **Quests.** Quest level, minimum level and money follow the same journey, by the map of whoever starts the quest.
- **Instances.** The entry levels of the dungeons follow the journey; the raids and heroic dungeons of Outland and
  Northrend open at 60.
- **Bots.** The random bots of mod-playerbots choose their hunting grounds by the levels of the creatures, so they
  follow the new journey without being told.
- **Reversible.** The original values are kept in two tables of the world database (`ageless_sixty_creature`,
  `ageless_sixty_quest`). Switched off (`AgelessSixty.Enable = 0`), the module puts them back at the next start. An
  update of the database that brings new values for a creature or quest is noticed and taken as the new original.

## Not yet

- The items of Outland and Northrend still ask for level 61 to 80 and carry their original stats.
- The world map of the client still shows the original level ranges.
- The random dungeon finder still uses its own level ranges.

## Requirements

- AzerothCore with the Conquest of Azeroth changes (`jealous-sound/azerothcore-wotlk-coa`), including its open-world
  scaling (mod-destiny-weaver).
- `MaxPlayerLevel = 60` in worldserver.conf; with mod-playerbots, `AiPlayerbot.RandomBotMaxLevel = 60`. AFK Realm sets
  both when it installs the module.

## Installation

With [AFK Realm](https://github.com/aspollon/AFK-Realm): add the module there. By hand: clone it into `modules/`,
rebuild, copy `conf/mod_ageless_sixty.conf.dist` next to the other module configs as `mod_ageless_sixty.conf`, and
set the two values above.

## License

GNU AGPL v3, like AzerothCore.
