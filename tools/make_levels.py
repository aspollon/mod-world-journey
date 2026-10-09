#!/usr/bin/env python3
"""Builds what the module takes from the original data, so that the module can compute the journey itself at every
start, from its regulators:

- src/JourneyData.h: the zones (with the original entry levels of the CoA client, Interface/FrameXML/Data/Maps.lua),
  the instances (entry levels and creature levels of the CoA world database) and the default zone brackets of the
  random bots, as they came.
- afk-realm.json: what AFK Realm sets in other config files and installs in the client.

Nothing here depends on the regulators: change them in mod_world_journey.conf, not here."""
import json, os

here = os.path.dirname(os.path.abspath(__file__)); root = os.path.join(here, '..'); data = os.path.join(root, 'data')
MODULE = 'mod-world-journey'
ADDON = 'ZoneLevels'
TITLE = 'Zone Levels'
PART = {'classic': 0, 'tbc': 1, 'wotlk': 2}

RAIDS = {249, 309, 409, 469, 509, 531, 532, 533, 534, 544, 548, 550, 564, 565, 568, 580, 603, 615, 616, 624, 631, 649, 724}

def lines(name):
    for line in open(os.path.join(data, name), encoding='utf-8'):
        if line.startswith('#') or not line.strip():
            continue
        yield line.rstrip('\n')

zones = []
for line in lines('source_zones.tsv'):
    key, area, mapid, lo, hi, era = line.split()
    zones.append((int(area), int(mapid), PART[era], int(lo), key))

content = {}
for part in open(os.path.join(data, 'source_instance_levels.txt')).read().strip().split(';'):
    m, lo, avg, hi, n = map(int, part.split())
    content[m] = avg
instances = []
for line in lines('source_instance_access.tsv'):
    mapid, diff, lo, hi, name = line.split('\t')
    mapid, diff, lo = int(mapid), int(diff), int(lo)
    kind = 2 if mapid in RAIDS else 1 if diff > 0 else 0
    instances.append((mapid, diff, kind, lo, max(content.get(mapid, lo), lo), name.replace('"', "'")))

bots = []
for line in lines('source_bot_zones.tsv'):
    zone, lo, hi, era, name = line.split('\t')
    bots.append((int(zone), int(lo), int(hi), PART[era], name))

with open(os.path.join(root, 'src', 'JourneyData.h'), 'w', newline='\n') as f:
    f.write('// Built by tools/make_levels.py from data/ - do not edit by hand. The original levels, as they came; the\n'
            '// module lays them over the journey at every start, by its regulators.\n#pragma once\n\n#include <cstdint>\n\n'
            'namespace journey\n{\n')
    f.write('    struct ZoneData { uint32_t zone; uint32_t map; uint8_t part; uint8_t originalEntry; char const* key; };\n')
    f.write('    struct InstanceData { uint32_t map; uint8_t difficulty; uint8_t kind; uint8_t originalEntry; uint8_t contentLevel; char const* name; };\n')
    f.write('    struct BotZoneData { uint32_t zone; uint8_t originalMin; uint8_t originalMax; uint8_t part; };\n\n')
    f.write('    inline constexpr ZoneData Zones[] =\n    {\n')
    for z in zones:
        f.write(f'        {{ {z[0]}, {z[1]}, {z[2]}, {z[3]}, "{z[4]}" }},\n')
    f.write('    };\n\n    inline constexpr InstanceData Instances[] =\n    {\n')
    for i in instances:
        f.write(f'        {{ {i[0]}, {i[1]}, {i[2]}, {i[3]}, {i[4]}, "{i[5]}" }},\n')
    f.write('    };\n\n    // The zone brackets mod-playerbots ships with (the levels its random bots hunt at).\n'
            '    inline constexpr BotZoneData BotZones[] =\n    {\n')
    for b in bots:
        f.write(f'        {{ {b[0]}, {b[1]}, {b[2]}, {b[3]} }},     // {b[4]}\n')
    f.write('    };\n}\n')

manifest = {
    'about': 'Read by AFK Realm (https://github.com/aspollon/AFK-Realm) when it builds the server. Without AFK Realm, '
             'apply the patches, set these values by hand and copy the addon into the client: see README.md.',
    'patches': [
        {'name': 'Bots follow the journey', 'target': 'mod-playerbots', 'file': 'patches/playerbots-world-journey.patch',
         'why': 'The random bots take their zone levels, their highest level and their mount levels from World Journey, '
                'and keep Outland and Northrend when they stop at 60.'},
        {'name': 'A window for lifted creatures', 'target': 'core', 'file': 'patches/core-level-window.patch',
         'why': 'Creatures lifted to a character keep their place in their zone - from two levels below to two above - '
                'instead of all standing at the same level.'},
    ],
    'settings': [
        {'file': 'worldserver.conf', 'key': 'MaxPlayerLevel', 'value': '60',
         'why': 'The journey ends at 60, where the classes of Conquest of Azeroth end.'},
        {'file': 'worldserver.conf', 'key': 'DungeonFinder.MaxExpansion', 'value': '2',
         'why': 'The Dungeon Finder offers the dungeons of Outland and Northrend too: they are part of the journey.'},
        {'file': 'worldserver.conf', 'key': 'Wintergrasp.PlayerMinLvl', 'value': '60',
         'why': 'Wintergrasp opens at the end of the journey.'},
        {'file': 'playerbots.conf', 'key': 'AiPlayerbot.RandomBotMaxLevel', 'value': '60',
         'why': 'Random bots end at 60 like the players.'},
        {'file': 'destiny_weaver.conf', 'key': 'DestinyWeaver.Scaling.Offset', 'value': '2',
         'why': 'Without the level window, lifted creatures stand two levels below a character instead of three.'},
    ],
    'client': {'addons': [f'client/AddOns/{ADDON}'], 'clearCache': True,
               'why': 'The world map shows the zone levels of the journey and gems and enchantments show their real '
                      'values; the cache of the client forgets the old levels of items, creatures and quests.'},
}
with open(os.path.join(root, 'afk-realm.json'), 'w', newline='\n') as f:
    json.dump(manifest, f, indent=2)
    f.write('\n')
print(len(zones), 'zones,', len(instances), 'instance entries,', len(bots), 'bot zones')
