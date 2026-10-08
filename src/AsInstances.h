// Built by tools/make_levels.py from data/instances.csv - do not edit by hand.
#pragma once

struct AsInstanceEntry { unsigned map; unsigned difficulty; unsigned entry; };

static AsInstanceEntry const AsInstances[] =
{
    { 33, 0, 11 },     // Shadowfang Keep
    { 34, 0, 12 },     // Stormwind Stockades
    { 36, 0, 9 },     // Deadmines (DM)
    { 43, 0, 9 },     // Wailing Caverns (WC)
    { 47, 0, 13 },     // Razorfen Kraul
    { 48, 0, 14 },     // Blackfathom Deeps
    { 70, 0, 21 },     // Uldaman
    { 90, 0, 12 },     // Gnomeregan
    { 109, 0, 24 },     // Sunken Temple (of Atal'Hakkar)
    { 129, 0, 18 },     // Razorfen Downs
    { 189, 0, 15 },     // Scarlet Monastery (SM) - All wings
    { 209, 0, 24 },     // Zul'Farrak (ZF)
    { 229, 0, 31 },     // Blackrock Spire - Both Lower (LBRS) & Upper (UBRS) - 5/10man
    { 230, 0, 27 },     // Blackrock Depths (BRD)
    { 249, 0, 60 },     // Onyxia's Lair - 10man
    { 249, 1, 60 },     // Onyxia's Lair - 25man
    { 269, 0, 40 },     // Caverns Of Time: Black Morass/Opening the Dark Portal - Normal
    { 269, 1, 60 },     // Caverns Of Time: Black Morass/Opening the Dark Portal - Heroic
    { 289, 0, 31 },     // Scholomance
    { 309, 0, 50 },     // Zul'Gurub (ZG) - 20man
    { 329, 0, 31 },     // Stratholme
    { 349, 0, 21 },     // Maraudon - All wings
    { 389, 0, 7 },     // Ragefire Chasm (RF)
    { 409, 0, 50 },     // Molten Core - 40man
    { 429, 0, 31 },     // Dire Maul - All wings
    { 469, 0, 60 },     // Blackwing Lair (BWL) - 40man
    { 509, 0, 50 },     // Ahn'Qiraj Ruins (AQ20) - 20man
    { 531, 0, 50 },     // Ahn'Qiraj Temple (AQ40) - 40man
    { 532, 0, 60 },     // Karazhan - 10man
    { 533, 0, 60 },     // Naxxramas - 10man
    { 533, 1, 60 },     // Naxxramas
    { 534, 0, 60 },     // Battle Of Mount Hyjal,Alliance Base
    { 540, 0, 42 },     // The Shattered Halls
    { 540, 1, 60 },     // The Shattered Halls
    { 542, 0, 30 },     // The Blood Furnace
    { 542, 1, 60 },     // The Blood Furnace
    { 543, 0, 30 },     // Hellfire Ramparts
    { 543, 1, 60 },     // Hellfire Ramparts
    { 544, 0, 60 },     // Hellfire Citadel: Magtheridon's Lair - 25man
    { 545, 0, 43 },     // The Steamvault
    { 545, 1, 60 },     // The Steamvault
    { 546, 0, 33 },     // The Underbog
    { 546, 1, 60 },     // The Underbog
    { 547, 0, 33 },     // The Slave Pens
    { 547, 1, 60 },     // The Slave Pens
    { 548, 0, 60 },     // Coilfang Reservoir: Serpentshrine Cavern - 25man
    { 550, 0, 60 },     // The Eye
    { 552, 0, 43 },     // The Arcatraz
    { 552, 1, 60 },     // The Arcatraz
    { 553, 0, 43 },     // The Botanica
    { 553, 1, 60 },     // The Botanica
    { 554, 0, 43 },     // The Mechanar
    { 554, 1, 60 },     // The Mechanar
    { 555, 0, 43 },     // Shadow Labyrinth
    { 555, 1, 60 },     // Shadow Labyrinth
    { 556, 0, 40 },     // Sethekk Halls
    { 556, 1, 60 },     // Sethekk Halls
    { 557, 0, 36 },     // Mana Tombs
    { 557, 1, 60 },     // Mana Tombs
    { 558, 0, 37 },     // Auchenai Crypts
    { 558, 1, 60 },     // Auchenai Crypts
    { 560, 0, 37 },     // Caverns Of Time: Old Hillsbrad Foothills/Escape from Durnholde - Normal
    { 560, 1, 60 },     // Caverns Of Time: Old Hillsbrad Foothills/Escape from Durnholde - Heroic
    { 564, 0, 60 },     // Black Temple
    { 565, 0, 60 },     // Gruul's Lair
    { 568, 0, 60 },     // Zul'Aman
    { 574, 0, 40 },     // Utgarde Keep
    { 574, 1, 60 },     // Utgarde Keep
    { 575, 0, 53 },     // Utgarde Pinnacle
    { 575, 1, 60 },     // Utgarde Pinnacle
    { 576, 0, 40 },     // The Nexus
    { 576, 1, 60 },     // The Nexus
    { 578, 0, 52 },     // The Oculus
    { 578, 1, 60 },     // The Oculus
    { 580, 0, 60 },     // Sunwell Plateau
    { 585, 0, 43 },     // Magisters' Terrace - Normal
    { 585, 1, 60 },     // Magisters' Terrace - Heroic
    { 595, 0, 47 },     // Culling of Stratholme
    { 595, 1, 60 },     // Culling of Stratholme
    { 599, 0, 50 },     // Ulduar,Halls of Stone
    { 599, 1, 60 },     // Ulduar,Halls of Stone
    { 600, 0, 45 },     // Drak'Tharon Keep
    { 600, 1, 60 },     // Drak'Tharon Keep
    { 601, 0, 40 },     // Azjol-Nerub
    { 601, 1, 60 },     // Azjol-Nerub
    { 602, 0, 55 },     // Ulduar,Halls of Lightning
    { 602, 1, 60 },     // Ulduar,Halls of Lightning
    { 603, 0, 60 },     // Ulduar - 10man
    { 603, 1, 60 },     // Ulduar
    { 604, 0, 50 },     // Gundrak (North entrance)
    { 604, 1, 60 },     // Gundrak (North entrance)
    { 608, 0, 48 },     // Violet Hold
    { 608, 1, 60 },     // Violet Hold
    { 615, 0, 60 },     // The Obsidian Sanctum - 10man
    { 615, 1, 60 },     // Chamber of Aspects,Obsidian Sanctum
    { 616, 0, 60 },     // The Eye of Eternity (Malygos) - 10man
    { 616, 1, 60 },     // The Eye of Eternity
    { 619, 0, 43 },     // Ahn'Kahet
    { 619, 1, 60 },     // Ahn'Kahet
    { 624, 0, 60 },     // Vault of Archavon - 10man
    { 624, 1, 60 },     // Vault of Archavon
    { 631, 0, 60 },     // Icecrown Citadel - 10man Normal
    { 631, 1, 60 },     // IceCrown Citadel
    { 631, 2, 60 },     // IceCrown Citadel
    { 631, 3, 60 },     // IceCrown Citadel
    { 632, 0, 55 },     // Forge of Souls
    { 632, 1, 60 },     // Forge of Souls
    { 649, 0, 60 },     // Trial of the Crusader - 10man Normal
    { 649, 1, 60 },     // Trial of the Crusader
    { 649, 2, 60 },     // Trial of the Crusader
    { 649, 3, 60 },     // Trial of the Crusader
    { 650, 0, 48 },     // Trial of the Champion
    { 650, 1, 60 },     // Trial of the Champion
    { 658, 0, 55 },     // Pit of Saron
    { 658, 1, 60 },     // Pit of Saron
    { 668, 0, 52 },     // Halls of Reflection
    { 668, 1, 60 },     // Halls of Reflection
    { 724, 0, 60 },     // The Ruby Sanctum - 10man Normal
    { 724, 1, 60 },     // The Ruby Sanctum
    { 724, 2, 60 },     // The Ruby Sanctum
    { 724, 3, 60 },     // The Ruby Sanctum
};
