// Built by tools/make_levels.py from data/ - do not edit by hand. The original levels, as they came; the
// module lays them over the journey at every start, by its regulators.
#pragma once

#include <cstdint>

namespace journey
{
    struct ZoneData { uint32_t zone; uint32_t map; uint8_t part; uint8_t originalEntry; char const* key; };
    struct InstanceData { uint32_t map; uint8_t difficulty; uint8_t kind; uint8_t originalEntry; uint8_t contentLevel; char const* name; };
    struct BotZoneData { uint32_t zone; uint8_t originalMin; uint8_t originalMax; uint8_t part; };

    inline constexpr ZoneData Zones[] =
    {
        { 14, 1, 0, 1, "Durotar" },
        { 215, 1, 0, 1, "Mulgore" },
        { 17, 1, 0, 10, "Barrens" },
        { 36, 0, 0, 30, "Alterac" },
        { 45, 0, 0, 30, "Arathi" },
        { 3, 0, 0, 35, "Badlands" },
        { 4, 0, 0, 45, "BlastedLands" },
        { 85, 0, 0, 1, "Tirisfal" },
        { 130, 0, 0, 10, "Silverpine" },
        { 28, 0, 0, 51, "WesternPlaguelands" },
        { 139, 0, 0, 53, "EasternPlaguelands" },
        { 267, 0, 0, 20, "Hilsbrad" },
        { 47, 0, 0, 45, "Hinterlands" },
        { 1, 0, 0, 1, "DunMorogh" },
        { 51, 0, 0, 43, "SearingGorge" },
        { 46, 0, 0, 50, "BurningSteppes" },
        { 12, 0, 0, 1, "Elwynn" },
        { 41, 0, 0, 55, "DeadwindPass" },
        { 10, 0, 0, 20, "Duskwood" },
        { 38, 0, 0, 10, "LochModan" },
        { 44, 0, 0, 15, "Redridge" },
        { 33, 0, 0, 30, "Stranglethorn" },
        { 8, 0, 0, 35, "SwampOfSorrows" },
        { 40, 0, 0, 10, "Westfall" },
        { 11, 0, 0, 20, "Wetlands" },
        { 141, 1, 0, 1, "Teldrassil" },
        { 148, 1, 0, 10, "Darkshore" },
        { 331, 1, 0, 18, "Ashenvale" },
        { 400, 1, 0, 25, "ThousandNeedles" },
        { 406, 1, 0, 15, "StonetalonMountains" },
        { 405, 1, 0, 30, "Desolace" },
        { 357, 1, 0, 40, "Feralas" },
        { 15, 1, 0, 35, "Dustwallow" },
        { 440, 1, 0, 40, "Tanaris" },
        { 16, 1, 0, 45, "Aszhara" },
        { 361, 1, 0, 48, "Felwood" },
        { 490, 1, 0, 48, "UngoroCrater" },
        { 1377, 1, 0, 55, "Silithus" },
        { 618, 1, 0, 53, "Winterspring" },
        { 3430, 530, 0, 1, "EversongWoods" },
        { 3433, 530, 0, 10, "Ghostlands" },
        { 3524, 530, 0, 1, "AzuremystIsle" },
        { 3525, 530, 0, 10, "BloodmystIsle" },
        { 3483, 530, 1, 58, "Hellfire" },
        { 3521, 530, 1, 60, "Zangarmarsh" },
        { 3519, 530, 1, 62, "TerokkarForest" },
        { 3518, 530, 1, 64, "Nagrand" },
        { 3522, 530, 1, 65, "BladesEdgeMountains" },
        { 3523, 530, 1, 67, "Netherstorm" },
        { 3520, 530, 1, 67, "ShadowmoonValley" },
        { 4080, 530, 1, 70, "Sunwell" },
        { 3537, 571, 2, 68, "BoreanTundra" },
        { 495, 571, 2, 68, "HowlingFjord" },
        { 65, 571, 2, 71, "Dragonblight" },
        { 394, 571, 2, 73, "GrizzlyHills" },
        { 66, 571, 2, 74, "ZulDrak" },
        { 3711, 571, 2, 75, "SholazarBasin" },
        { 210, 571, 2, 77, "IcecrownGlacier" },
        { 67, 571, 2, 77, "TheStormPeaks" },
        { 2817, 571, 2, 77, "CrystalsongForest" },
        { 4197, 571, 2, 77, "LakeWintergrasp" },
        { 4742, 571, 2, 77, "HrothgarsLanding" },
    };

    inline constexpr InstanceData Instances[] =
    {
        { 33, 0, 0, 14, 38, "Shadowfang Keep" },
        { 34, 0, 0, 15, 24, "Stormwind Stockades" },
        { 36, 0, 0, 10, 19, "Deadmines (DM)" },
        { 43, 0, 0, 10, 19, "Wailing Caverns (WC)" },
        { 47, 0, 0, 17, 26, "Razorfen Kraul" },
        { 48, 0, 0, 19, 23, "Blackfathom Deeps" },
        { 70, 0, 0, 30, 38, "Uldaman" },
        { 90, 0, 0, 15, 27, "Gnomeregan" },
        { 109, 0, 0, 35, 48, "Sunken Temple (of Atal'Hakkar)" },
        { 129, 0, 0, 25, 36, "Razorfen Downs" },
        { 189, 0, 0, 20, 35, "Scarlet Monastery (SM) - All wings" },
        { 209, 0, 0, 35, 44, "Zul'Farrak (ZF)" },
        { 229, 0, 0, 45, 58, "Blackrock Spire - Both Lower (LBRS) & Upper (UBRS) - 5/10man" },
        { 230, 0, 0, 40, 52, "Blackrock Depths (BRD)" },
        { 249, 0, 2, 80, 80, "Onyxia's Lair - 10man" },
        { 249, 1, 2, 80, 80, "Onyxia's Lair - 25man" },
        { 269, 0, 0, 66, 68, "Caverns Of Time: Black Morass/Opening the Dark Portal - Normal" },
        { 269, 1, 1, 70, 70, "Caverns Of Time: Black Morass/Opening the Dark Portal - Heroic" },
        { 289, 0, 0, 45, 59, "Scholomance" },
        { 309, 0, 2, 50, 60, "Zul'Gurub (ZG) - 20man" },
        { 329, 0, 0, 45, 58, "Stratholme" },
        { 349, 0, 0, 30, 45, "Maraudon - All wings" },
        { 389, 0, 0, 8, 15, "Ragefire Chasm (RF)" },
        { 409, 0, 2, 50, 62, "Molten Core - 40man" },
        { 429, 0, 0, 45, 57, "Dire Maul - All wings" },
        { 469, 0, 2, 60, 60, "Blackwing Lair (BWL) - 40man" },
        { 509, 0, 2, 50, 60, "Ahn'Qiraj Ruins (AQ20) - 20man" },
        { 531, 0, 2, 50, 60, "Ahn'Qiraj Temple (AQ40) - 40man" },
        { 532, 0, 2, 68, 70, "Karazhan - 10man" },
        { 533, 0, 2, 80, 81, "Naxxramas - 10man" },
        { 533, 1, 2, 80, 81, "Naxxramas" },
        { 534, 0, 2, 70, 70, "Battle Of Mount Hyjal,Alliance Base" },
        { 540, 0, 0, 55, 69, "The Shattered Halls" },
        { 540, 1, 1, 70, 70, "The Shattered Halls" },
        { 542, 0, 0, 55, 61, "The Blood Furnace" },
        { 542, 1, 1, 70, 70, "The Blood Furnace" },
        { 543, 0, 0, 55, 61, "Hellfire Ramparts" },
        { 543, 1, 1, 70, 70, "Hellfire Ramparts" },
        { 544, 0, 2, 65, 65, "Hellfire Citadel: Magtheridon's Lair - 25man" },
        { 545, 0, 0, 55, 70, "The Steamvault" },
        { 545, 1, 1, 70, 70, "The Steamvault" },
        { 546, 0, 0, 55, 63, "The Underbog" },
        { 546, 1, 1, 70, 70, "The Underbog" },
        { 547, 0, 0, 55, 63, "The Slave Pens" },
        { 547, 1, 1, 70, 70, "The Slave Pens" },
        { 548, 0, 2, 68, 70, "Coilfang Reservoir: Serpentshrine Cavern - 25man" },
        { 550, 0, 2, 70, 70, "The Eye" },
        { 552, 0, 0, 68, 70, "The Arcatraz" },
        { 552, 1, 1, 70, 70, "The Arcatraz" },
        { 553, 0, 0, 67, 70, "The Botanica" },
        { 553, 1, 1, 70, 70, "The Botanica" },
        { 554, 0, 0, 67, 70, "The Mechanar" },
        { 554, 1, 1, 70, 70, "The Mechanar" },
        { 555, 0, 0, 65, 70, "Shadow Labyrinth" },
        { 555, 1, 1, 70, 70, "Shadow Labyrinth" },
        { 556, 0, 0, 55, 68, "Sethekk Halls" },
        { 556, 1, 1, 70, 70, "Sethekk Halls" },
        { 557, 0, 0, 55, 65, "Mana Tombs" },
        { 557, 1, 1, 70, 70, "Mana Tombs" },
        { 558, 0, 0, 55, 66, "Auchenai Crypts" },
        { 558, 1, 1, 70, 70, "Auchenai Crypts" },
        { 560, 0, 0, 64, 64, "Caverns Of Time: Old Hillsbrad Foothills/Escape from Durnholde - Normal" },
        { 560, 1, 1, 70, 70, "Caverns Of Time: Old Hillsbrad Foothills/Escape from Durnholde - Heroic" },
        { 564, 0, 2, 70, 70, "Black Temple" },
        { 565, 0, 2, 70, 72, "Gruul's Lair" },
        { 568, 0, 2, 70, 70, "Zul'Aman" },
        { 574, 0, 0, 65, 70, "Utgarde Keep" },
        { 574, 1, 1, 80, 80, "Utgarde Keep" },
        { 575, 0, 0, 75, 79, "Utgarde Pinnacle" },
        { 575, 1, 1, 80, 80, "Utgarde Pinnacle" },
        { 576, 0, 0, 66, 71, "The Nexus" },
        { 576, 1, 1, 80, 80, "The Nexus" },
        { 578, 0, 0, 75, 78, "The Oculus" },
        { 578, 1, 1, 80, 80, "The Oculus" },
        { 580, 0, 2, 70, 71, "Sunwell Plateau" },
        { 585, 0, 0, 65, 70, "Magisters' Terrace - Normal" },
        { 585, 1, 1, 70, 70, "Magisters' Terrace - Heroic" },
        { 595, 0, 0, 75, 75, "Culling of Stratholme" },
        { 595, 1, 1, 80, 80, "Culling of Stratholme" },
        { 599, 0, 0, 72, 77, "Ulduar,Halls of Stone" },
        { 599, 1, 1, 80, 80, "Ulduar,Halls of Stone" },
        { 600, 0, 0, 69, 74, "Drak'Tharon Keep" },
        { 600, 1, 1, 80, 80, "Drak'Tharon Keep" },
        { 601, 0, 0, 67, 69, "Azjol-Nerub" },
        { 601, 1, 1, 80, 80, "Azjol-Nerub" },
        { 602, 0, 0, 75, 80, "Ulduar,Halls of Lightning" },
        { 602, 1, 1, 80, 80, "Ulduar,Halls of Lightning" },
        { 603, 0, 2, 80, 80, "Ulduar - 10man" },
        { 603, 1, 2, 80, 80, "Ulduar" },
        { 604, 0, 0, 71, 77, "Gundrak (North entrance)" },
        { 604, 1, 1, 80, 80, "Gundrak (North entrance)" },
        { 608, 0, 0, 70, 76, "Violet Hold" },
        { 608, 1, 1, 80, 80, "Violet Hold" },
        { 615, 0, 2, 80, 80, "The Obsidian Sanctum - 10man" },
        { 615, 1, 2, 80, 80, "Chamber of Aspects,Obsidian Sanctum" },
        { 616, 0, 2, 80, 80, "The Eye of Eternity (Malygos) - 10man" },
        { 616, 1, 2, 80, 80, "The Eye of Eternity" },
        { 619, 0, 0, 68, 73, "Ahn'Kahet" },
        { 619, 1, 1, 80, 80, "Ahn'Kahet" },
        { 624, 0, 2, 80, 80, "Vault of Archavon - 10man" },
        { 624, 1, 2, 80, 80, "Vault of Archavon" },
        { 631, 0, 2, 80, 80, "Icecrown Citadel - 10man Normal" },
        { 631, 1, 2, 80, 80, "IceCrown Citadel" },
        { 631, 2, 2, 80, 80, "IceCrown Citadel" },
        { 631, 3, 2, 80, 80, "IceCrown Citadel" },
        { 632, 0, 0, 75, 80, "Forge of Souls" },
        { 632, 1, 1, 80, 80, "Forge of Souls" },
        { 649, 0, 2, 80, 80, "Trial of the Crusader - 10man Normal" },
        { 649, 1, 2, 80, 80, "Trial of the Crusader" },
        { 649, 2, 2, 80, 80, "Trial of the Crusader" },
        { 649, 3, 2, 80, 80, "Trial of the Crusader" },
        { 650, 0, 0, 75, 76, "Trial of the Champion" },
        { 650, 1, 1, 80, 80, "Trial of the Champion" },
        { 658, 0, 0, 78, 80, "Pit of Saron" },
        { 658, 1, 1, 80, 80, "Pit of Saron" },
        { 668, 0, 0, 78, 78, "Halls of Reflection" },
        { 668, 1, 1, 80, 80, "Halls of Reflection" },
        { 724, 0, 2, 80, 81, "The Ruby Sanctum - 10man Normal" },
        { 724, 1, 2, 80, 81, "The Ruby Sanctum" },
        { 724, 2, 2, 80, 81, "The Ruby Sanctum" },
        { 724, 3, 2, 80, 81, "The Ruby Sanctum" },
    };

    // The zone brackets mod-playerbots ships with (the levels its random bots hunt at).
    inline constexpr BotZoneData BotZones[] =
    {
        { 1, 5, 12, 0 },     // Dun Morogh
        { 12, 5, 12, 0 },     // Elwynn Forest
        { 14, 5, 12, 0 },     // Durotar
        { 85, 5, 12, 0 },     // Tirisfal Glades
        { 141, 5, 12, 0 },     // Teldrassil
        { 215, 5, 12, 0 },     // Mulgore
        { 3430, 5, 12, 0 },     // Eversong Woods
        { 3524, 5, 12, 0 },     // Azuremyst Isle
        { 17, 10, 25, 0 },     // Barrens
        { 38, 10, 20, 0 },     // Loch Modan
        { 40, 10, 21, 0 },     // Westfall
        { 130, 10, 23, 0 },     // Silverpine Forest
        { 148, 10, 21, 0 },     // Darkshore
        { 3433, 10, 22, 0 },     // Ghostlands
        { 3525, 10, 21, 0 },     // Bloodmyst Isle
        { 10, 19, 33, 0 },     // Duskwood
        { 11, 21, 30, 0 },     // Wetlands
        { 44, 16, 28, 0 },     // Redridge Mountains
        { 267, 20, 34, 0 },     // Hillsbrad Foothills
        { 331, 18, 33, 0 },     // Ashenvale
        { 400, 24, 36, 0 },     // Thousand Needles
        { 406, 16, 29, 0 },     // Stonetalon Mountains
        { 3, 36, 46, 0 },     // Badlands
        { 8, 36, 46, 0 },     // Swamp of Sorrows
        { 15, 35, 46, 0 },     // Dustwallow Marsh
        { 16, 45, 52, 0 },     // Azshara
        { 33, 32, 47, 0 },     // Stranglethorn Vale
        { 45, 30, 42, 0 },     // Arathi Highlands
        { 47, 42, 51, 0 },     // Hinterlands
        { 51, 45, 51, 0 },     // Searing Gorge
        { 357, 40, 52, 0 },     // Feralas
        { 405, 30, 41, 0 },     // Desolace
        { 440, 41, 52, 0 },     // Tanaris
        { 4, 52, 57, 0 },     // Blasted Lands
        { 28, 50, 60, 0 },     // Western Plaguelands
        { 46, 51, 60, 0 },     // Burning Steppes
        { 139, 54, 62, 0 },     // Eastern Plaguelands
        { 361, 47, 57, 0 },     // Felwood
        { 490, 49, 56, 0 },     // Un'Goro Crater
        { 618, 54, 61, 0 },     // Winterspring
        { 1377, 54, 63, 0 },     // Silithus
        { 3483, 58, 66, 1 },     // Hellfire Peninsula
        { 3518, 64, 70, 1 },     // Nagrand
        { 3519, 62, 73, 1 },     // Terokkar Forest
        { 3520, 66, 73, 1 },     // Shadowmoon Valley
        { 3521, 60, 67, 1 },     // Zangarmarsh
        { 3522, 64, 73, 1 },     // Blade's Edge Mountains
        { 3523, 67, 73, 1 },     // Netherstorm
        { 4080, 68, 73, 1 },     // Isle of Quel'Danas
        { 65, 71, 77, 2 },     // Dragonblight
        { 66, 74, 80, 2 },     // Zul'Drak
        { 67, 77, 80, 2 },     // Storm Peaks
        { 210, 77, 80, 2 },     // Icecrown Glacier
        { 394, 72, 78, 2 },     // Grizzly Hills
        { 495, 68, 74, 2 },     // Howling Fjord
        { 2817, 77, 80, 2 },     // Crystalsong Forest
        { 3537, 68, 75, 2 },     // Borean Tundra
        { 3711, 75, 80, 2 },     // Sholazar Basin
        { 4197, 79, 80, 2 },     // Wintergrasp
    };
}
