#!/usr/bin/env python3
"""Builds everything the module takes from the journey table, from the original data: the zone ranges of the CoA
client (Interface/FrameXML/Data/Maps.lua), the instance entry levels and creature levels of the CoA world database and
the default zone brackets of the random bots. The journey from 1 to 60 is compressed so that it runs naturally through
the old world, Outland and Northrend; the order within each part stays as it was.

Writes data/zones.csv, data/instances.csv, src/JourneyInstances.h, the world map addon for the client and afk-realm.json."""
import csv, json, os
here = os.path.dirname(os.path.abspath(__file__)); root = os.path.join(here, '..'); data = os.path.join(root, 'data')
MODULE = 'mod-world-journey'       # the name of the module (its folder and repository)
ADDON = 'ZoneLevels'               # the name of its addon in the client
TITLE = 'Zone Levels'               # how the addon is shown in the client

def classic(level):      # old world entries 1..55 -> 1..35
    return max(1, round(1 + (level - 1) * 34 / 54))
def tbc(level):          # Outland 58..67 -> 30..43
    return round(30 + (level - 58) * 13 / 9)
def wotlk(level):        # Northrend 68..77 -> 40..55
    return round(40 + (level - 68) * 15 / 9)
ERA = {'classic': classic, 'tbc': tbc, 'wotlk': wotlk}

rows = []
for line in open(os.path.join(data, 'source_zones.tsv')):
    if line.startswith('#') or not line.strip(): continue
    key, area, mapid, lo, hi, era = line.split()
    lo = int(lo)
    entry = 1 if lo == 1 else ERA[era](lo)
    if key == 'Sunwell': entry = 55           # the isle of the last raid of Outland
    rows.append([key, area, mapid, era, f'{lo}-{hi}', min(entry, 55), 60])
with open(os.path.join(data, 'zones.csv'), 'w', newline='') as f:
    w = csv.writer(f); w.writerow(['zone', 'area', 'map', 'era', 'original', 'entry', 'max']); w.writerows(rows)

RAIDS = {249, 309, 409, 469, 509, 531, 532, 533, 534, 544, 548, 550, 564, 565, 568, 580, 603, 615, 616, 624, 631, 649, 724}
TBC_DUNGEONS = {269, 540, 542, 543, 545, 546, 547, 552, 553, 554, 555, 556, 557, 558, 560, 585}
WOTLK_DUNGEONS = {574, 575, 576, 578, 595, 599, 600, 601, 602, 604, 608, 619, 632, 650, 658, 668}
content = {}
for part in open(os.path.join(data, 'source_instance_levels.txt')).read().strip().split(';'):
    m, lo, avg, hi, n = map(int, part.split()); content[m] = avg
rows = []
for line in open(os.path.join(data, 'source_instance_access.tsv')):
    mapid, diff, lo, hi, name = line.rstrip('\n').split('\t')
    mapid, diff, lo = int(mapid), int(diff), int(lo)
    level = max(content.get(mapid, lo), lo)
    if mapid in RAIDS:
        kind = 'raid'
        entry = 60 if (mapid in TBC_DUNGEONS or mapid not in {309, 409, 469, 509, 531}) else lo
    elif diff > 0:
        kind, entry = 'heroic', 60
    elif mapid in TBC_DUNGEONS:
        kind = 'dungeon'
        if mapid == 560: level = 66                  # Old Hillsbrad: the many townsfolk pull the average down
        entry = max(30, tbc(min(level, 70) - 3))
    elif mapid in WOTLK_DUNGEONS:
        kind = 'dungeon'
        entry = max(40, wotlk(min(level, 80) - 3))
    else:
        # The old dungeons: their entry level in the database is a better guide than their creatures, some of
        # which CoA has added at other levels.
        kind = 'dungeon'
        entry = classic(min(lo + 3, 55))
    rows.append([mapid, diff, name, kind, lo, min(entry, 60)])
with open(os.path.join(data, 'instances.csv'), 'w', newline='') as f:
    w = csv.writer(f); w.writerow(['map', 'difficulty', 'name', 'kind', 'original_entry', 'entry']); w.writerows(rows)
print(len(open(os.path.join(data, 'zones.csv')).readlines()) - 1, 'zones,', len(rows), 'instance entries')

# The entry levels of the instances, built into the module.
with open(os.path.join(here, '..', 'src', 'JourneyInstances.h'), 'w') as f:
    f.write('// Built by tools/make_levels.py from data/instances.csv - do not edit by hand.\n#pragma once\n\n')
    f.write('struct JourneyInstance { unsigned map; unsigned difficulty; unsigned entry; };\n\n')
    f.write('static JourneyInstance const JourneyInstances[] =\n{\n')
    for r in rows:
        f.write(f'    {{ {r[0]}, {r[1]}, {r[5]} }},     // {r[2]}\n')
    f.write('};\n')

# ------------------------------------------------------------------------------------------ the world map of the client
# The client shows the level range of a zone from WORLD_MAP_LEVELS (Interface/FrameXML/Data/Maps.lua). The addon puts
# the journey's ranges there; nothing else of the client is touched.
zones = list(csv.DictReader(open(os.path.join(data, 'zones.csv'))))
addon = os.path.join(root, 'client', 'AddOns', ADDON)
os.makedirs(addon, exist_ok=True)
with open(os.path.join(addon, ADDON + '.toc'), 'w', newline='\r\n') as f:
    f.write(f"## Interface: 30300\n## Title: {TITLE}\n## Notes: The world map shows the zone levels of the journey from 1 to 60.\n"
            f"## Author: {MODULE}\n## Version: 1\n{ADDON}.lua\n")
with open(os.path.join(addon, ADDON + '.lua'), 'w', newline='\r\n') as f:
    f.write(f"-- {TITLE}: the world map shows the zone levels of the journey from 1 to 60.\n"
            f"-- Built by tools/make_levels.py of {MODULE} - do not edit by hand.\n\n"
            "if type(WORLD_MAP_LEVELS) ~= \"table\" then return end\n\n")
    for z in zones:
        f.write(f'WORLD_MAP_LEVELS["{z["zone"]}"] = {{ {z["entry"]}, {z["max"]} }}\n')

# ------------------------------------------------------------------------------------------ AFK Realm
def compress(level, era):
    if era == 'tbc' and level >= 58: value = tbc(level)
    elif era == 'wotlk' and level >= 68: value = min(wotlk(level), 60)
    else: value = classic(level)
    return max(1, min(60, value))

settings = [
    ('worldserver.conf', 'MaxPlayerLevel', '60', 'The journey ends at 60, where the classes of Conquest of Azeroth end.'),
    ('worldserver.conf', 'DungeonFinder.MaxExpansion', '2', 'The Dungeon Finder offers the dungeons of Outland and Northrend too: they are part of the journey.'),
    ('worldserver.conf', 'Wintergrasp.PlayerMinLvl', '60', 'Wintergrasp opens at the end of the journey.'),
    ('playerbots.conf', 'AiPlayerbot.RandomBotMaxLevel', '60', 'Random bots end at 60 like the players.'),
    ('playerbots.conf', 'AiPlayerbot.UseGroundMountAtMinLevel', str(classic(20)), 'Riding follows the journey.'),
    ('playerbots.conf', 'AiPlayerbot.UseFastGroundMountAtMinLevel', str(classic(40)), 'Riding follows the journey.'),
    ('playerbots.conf', 'AiPlayerbot.UseFlyMountAtMinLevel', str(tbc(60)), 'Flying follows the journey.'),
    ('playerbots.conf', 'AiPlayerbot.UseFastFlyMountAtMinLevel', str(tbc(70)), 'Flying follows the journey.'),
]
for line in open(os.path.join(data, 'source_bot_zones.tsv')):
    if line.startswith('#') or not line.strip(): continue
    zone, lo, hi, era, name = line.rstrip('\n').split('\t')
    lo, hi = compress(int(lo), era), compress(int(hi), era)
    if zone == '4080': lo, hi = 55, 60          # the Isle of Quel'Danas: the last raid of Outland
    settings.append(('playerbots.conf', f'AiPlayerbot.ZoneBracket.{zone}', f'{lo},{max(lo, hi)}', f'Random bots go to {name} at the levels of the journey.'))
manifest = {
    'about': 'Read by AFK Realm (https://github.com/aspollon/AFK-Realm) when it builds the server. Without AFK Realm, set these values by hand and copy the addon into the client: see README.md.',
    'patches': [],
    'settings': [{'file': f, 'key': k, 'value': v, 'why': w} for f, k, v, w in settings],
    'client': {'addons': [f'client/AddOns/{ADDON}'], 'clearCache': True,
               'why': 'The world map shows the zone levels of the journey; the cache of the client forgets the old levels of items, creatures and quests.'},
}
with open(os.path.join(root, 'afk-realm.json'), 'w', newline='\n') as f:
    json.dump(manifest, f, indent=2); f.write('\n')
print(len(zones), 'zones on the map,', len(settings), 'settings for AFK Realm')
