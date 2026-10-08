#!/usr/bin/env python3
"""Builds data/zones.csv and data/instances.csv - the one table the module plays by - from the original data:
the zone ranges of the CoA client (Interface/FrameXML/Data/Maps.lua) and the instance entry levels and creature
levels of the CoA world database. The journey from 1 to 60 is compressed so that it runs naturally through the old
world, Outland and Northrend; the order within each part stays as it was."""
import csv, os
here = os.path.dirname(os.path.abspath(__file__)); data = os.path.join(here, '..', 'data')

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
with open(os.path.join(here, '..', 'src', 'AsInstances.h'), 'w') as f:
    f.write('// Built by tools/make_levels.py from data/instances.csv - do not edit by hand.\n#pragma once\n\n')
    f.write('struct AsInstanceEntry { unsigned map; unsigned difficulty; unsigned entry; };\n\n')
    f.write('static AsInstanceEntry const AsInstances[] =\n{\n')
    for r in rows:
        f.write(f'    {{ {r[0]}, {r[1]}, {r[5]} }},     // {r[2]}\n')
    f.write('};\n')
