/*
 * mod-ageless-sixty - the whole world, Outland and Northrend included, as one journey from 1 to 60.
 * Released under GNU AGPL v3: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 *
 * The classes of Conquest of Azeroth end at level 60, so Outland and Northrend - made for 58 to 80 - are out of
 * their reach. This module compresses the levels of the world so that the road from 1 to 60 runs through all of
 * it: the old world first, then through the Dark Portal, then to Northrend, every zone open until 60.
 *
 *     old world      entries  1..55  ->  1..35
 *     Outland        entries 58..67  -> 30..43
 *     Northrend      entries 68..77  -> 40..55
 *     raids and heroic dungeons of Outland and Northrend: at 60
 *
 * How: at every start, before the world reads its data, the levels of creatures and quests in the world
 * database are set to their compressed values, from originals the module keeps in tables of its own
 * (ageless_sixty_creature, ageless_sixty_quest). Whatever the core then does with a level - the stats of a
 * creature, the experience of a quest, the zones the bots go to - follows by itself, and the open-world
 * scaling of CoA lifts a creature to the character in front of it as it always does. Switched off, the
 * module puts the original values back.
 *
 * What a level does not reach, the module does at runtime: the spells of a creature hit with the numbers of
 * its original level, so their damage is scaled down with it; and the entry levels of the instances follow
 * the same journey.
 */

#include "AsInstances.h"

#include "Config.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellInfo.h"
#include "World.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
    bool enabled = true;
    bool debug = false;

    // ------------------------------------------------------------------------------------------ the journey

    enum Era : uint8
    {
        ERA_KEEP,           // left as it is: classic raids, world bosses, what is above the journey
        ERA_CLASSIC,        // the old world, its dungeons, the starting zones of the blood elves and draenei
        ERA_OUTLAND,        // Outland and its normal dungeons
        ERA_NORTHREND,      // Northrend and its normal dungeons
        ERA_TOP_OUTLAND,    // raids and heroic dungeons of Outland: the end of the journey, at 60
        ERA_TOP_NORTHREND   // ... and of Northrend
    };

    std::set<uint32> const OutlandInstances = { 269, 540, 542, 543, 545, 546, 547, 552, 553, 554, 555, 556, 557, 558, 560, 585 };
    std::set<uint32> const NorthrendInstances = { 574, 575, 576, 578, 595, 599, 600, 601, 602, 604, 608, 619, 632, 650, 658, 668 };
    std::set<uint32> const OutlandRaids = { 532, 534, 544, 548, 550, 564, 565, 568, 580 };
    std::set<uint32> const NorthrendRaids = { 249, 533, 603, 615, 616, 624, 631, 649, 724 };
    std::set<uint32> const ClassicRaids = { 309, 409, 469, 509, 531 };

    uint8 Clamp(double level)
    {
        return uint8(std::clamp(std::lround(level), 1L, 63L));
    }

    /// What a level of the old world becomes: 1..55 to 1..35, and 60 to 38.
    uint8 Classic(uint8 level)
    {
        return Clamp(1.0 + (level - 1) * 34.0 / 54.0);
    }

    /// Outland: 58..67 to 30..43, and 70 to 47.
    uint8 Outland(uint8 level)
    {
        return level < 58 ? Classic(level) : Clamp(30.0 + (level - 58) * 13.0 / 9.0);
    }

    /// Northrend: 68..77 to 40..55, and 80 to 60.
    uint8 Northrend(uint8 level)
    {
        return level < 68 ? Classic(level) : uint8(std::min<long>(Clamp(40.0 + (level - 68) * 15.0 / 9.0), 60));
    }

    /// Raids and heroics: their trash at 60, their bosses a few levels above.
    uint8 Top(uint8 level, uint8 base)
    {
        return uint8(std::clamp<int>(60 + int(level) - int(base), 60, 63));
    }

    uint8 Compress(uint8 level, Era era)
    {
        if (!level)
            return level;
        switch (era)
        {
            // Above 60 in the old world: what Outland and Northrend sent there - the heralds at the Dark Portal, the
            // guards of the capitals, the generals of the Lich King's invasion.
            case ERA_CLASSIC:       return level <= 60 ? Classic(level) : level <= 72 ? Outland(level) : Northrend(level);
            case ERA_OUTLAND:       return Outland(level);
            case ERA_NORTHREND:     return Northrend(level);
            case ERA_TOP_OUTLAND:   return level >= 61 ? Top(level, 70) : level;
            case ERA_TOP_NORTHREND: return level >= 61 ? Top(level, 80) : level;
            default:                return level;
        }
    }

    /// The era of something found on a map, at a level.
    Era EraOf(uint32 map, uint8 level, bool heroic)
    {
        if (ClassicRaids.count(map))
            return ERA_KEEP;
        if (OutlandRaids.count(map) || (heroic && OutlandInstances.count(map)))
            return ERA_TOP_OUTLAND;
        if (NorthrendRaids.count(map) || (heroic && NorthrendInstances.count(map)))
            return ERA_TOP_NORTHREND;
        if (OutlandInstances.count(map))
            return ERA_OUTLAND;
        if (NorthrendInstances.count(map))
            return ERA_NORTHREND;
        if (map == 530)
            return level >= 58 ? ERA_OUTLAND : ERA_CLASSIC;
        if (map == 571)
            return ERA_NORTHREND;
        return ERA_CLASSIC;
    }

    /// Something that is nowhere to be found by its spawns: judged by its level. Lower is the safe side - CoA's
    /// open-world scaling lifts a creature that is too low for a character, but nothing lowers one that is too high.
    Era EraByLevel(uint8 level)
    {
        if (level <= 60)
            return ERA_CLASSIC;
        return level <= 72 ? ERA_OUTLAND : ERA_NORTHREND;
    }

    /// Money goes with the square of the level, roughly; it shrinks with it.
    uint32 ScaleMoney(uint32 money, uint8 from, uint8 to)
    {
        if (!money || !from || to >= from)
            return money;
        double const share = double(to) / double(from);
        return uint32(std::max(1.0, std::round(money * share * share)));
    }

    // ------------------------------------------------------------------------------------------ the database

    std::string Join(std::vector<std::string> const& parts)
    {
        std::string out;
        for (std::string const& part : parts)
            out += (out.empty() ? "" : ",") + part;
        return out;
    }

    /// Writes many rows at once: INSERT ... VALUES (..),(..) ON DUPLICATE KEY UPDATE ...
    void WriteRows(std::string const& head, std::vector<std::string> const& rows, std::string const& tail)
    {
        for (size_t i = 0; i < rows.size(); i += 1000)
        {
            std::vector<std::string> chunk(rows.begin() + i, rows.begin() + std::min(rows.size(), i + 1000));
            WorldDatabase.DirectExecute(head + Join(chunk) + tail);
        }
    }

    /// The map most spawns of each creature are on.
    std::unordered_map<uint32, uint32> SpawnMaps()
    {
        std::string const idColumn = WorldDatabase.Query("SHOW COLUMNS FROM `creature` LIKE 'id1'") ? "id1" : "id";
        std::unordered_map<uint32, std::pair<uint32, uint32>> best;     // entry -> map, spawns there
        if (QueryResult result = WorldDatabase.Query("SELECT `{}`, `map`, COUNT(*) FROM `creature` GROUP BY `{}`, `map`", idColumn, idColumn))
            do
            {
                Field* fields = result->Fetch();
                uint32 const entry = fields[0].Get<uint32>();
                uint32 const count = uint32(fields[2].Get<uint64>());
                auto& known = best[entry];
                if (count > known.second)
                    known = { fields[1].Get<uint32>(), count };
            } while (result->NextRow());
        std::unordered_map<uint32, uint32> maps;
        for (auto const& entry : best)
            maps[entry.first] = entry.second.first;
        return maps;
    }

    void CompressCreatures()
    {
        WorldDatabase.DirectExecute(
            "CREATE TABLE IF NOT EXISTS `ageless_sixty_creature` ("
            "`entry` INT UNSIGNED NOT NULL PRIMARY KEY, "
            "`orig_min` TINYINT UNSIGNED NOT NULL DEFAULT 0, `orig_max` TINYINT UNSIGNED NOT NULL DEFAULT 0, `orig_mingold` INT UNSIGNED NOT NULL DEFAULT 0, `orig_maxgold` INT UNSIGNED NOT NULL DEFAULT 0, "
            "`applied_min` TINYINT UNSIGNED NOT NULL DEFAULT 0, `applied_max` TINYINT UNSIGNED NOT NULL DEFAULT 0, `applied_mingold` INT UNSIGNED NOT NULL DEFAULT 0, `applied_maxgold` INT UNSIGNED NOT NULL DEFAULT 0"
            ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='mod-ageless-sixty: the original levels of the creatures, and what the module made of them'");
        // New creatures are taken as they are; one whose values were changed by somebody else since (an update of
        // the database) has new originals.
        WorldDatabase.DirectExecute(
            "INSERT IGNORE INTO `ageless_sixty_creature` SELECT `entry`, `minlevel`, `maxlevel`, `mingold`, `maxgold`, `minlevel`, `maxlevel`, `mingold`, `maxgold` "
            "FROM `creature_template`");
        WorldDatabase.DirectExecute(
            "UPDATE `ageless_sixty_creature` a JOIN `creature_template` t ON t.`entry` = a.`entry` SET "
            "a.`orig_min` = t.`minlevel`, a.`orig_max` = t.`maxlevel`, a.`orig_mingold` = t.`mingold`, a.`orig_maxgold` = t.`maxgold`, "
            "a.`applied_min` = t.`minlevel`, a.`applied_max` = t.`maxlevel`, a.`applied_mingold` = t.`mingold`, a.`applied_maxgold` = t.`maxgold` "
            "WHERE t.`minlevel` <> a.`applied_min` OR t.`maxlevel` <> a.`applied_max` OR t.`mingold` <> a.`applied_mingold` OR t.`maxgold` <> a.`applied_maxgold`");

        std::unordered_map<uint32, uint32> const maps = SpawnMaps();
        std::unordered_map<uint32, uint32> heroicOf;        // a template of a heroic or raid difficulty -> its normal one
        if (QueryResult result = WorldDatabase.Query("SELECT `entry`, `difficulty_entry_1`, `difficulty_entry_2`, `difficulty_entry_3` FROM `creature_template` "
            "WHERE `difficulty_entry_1` OR `difficulty_entry_2` OR `difficulty_entry_3`"))
            do
            {
                Field* fields = result->Fetch();
                for (uint32 i = 1; i <= 3; ++i)
                    if (uint32 const other = fields[i].Get<uint32>())
                        heroicOf[other] = fields[0].Get<uint32>();
            } while (result->NextRow());

        std::vector<std::string> rows;
        uint32 changed = 0;
        if (QueryResult result = WorldDatabase.Query("SELECT a.`entry`, a.`orig_min`, a.`orig_max`, a.`orig_mingold`, a.`orig_maxgold`, t.`rank` "
            "FROM `ageless_sixty_creature` a JOIN `creature_template` t ON t.`entry` = a.`entry`"))
            do
            {
                Field* fields = result->Fetch();
                uint32 const entry = fields[0].Get<uint32>();
                uint8 const origMin = fields[1].Get<uint8>(), origMax = fields[2].Get<uint8>();
                uint32 const minGold = fields[3].Get<uint32>(), maxGold = fields[4].Get<uint32>();
                uint8 const rank = fields[5].Get<uint8>();

                Era era;
                auto heroic = heroicOf.find(entry);
                uint32 const spawnedAs = heroic != heroicOf.end() ? heroic->second : entry;
                auto map = maps.find(spawnedAs);
                if (map != maps.end())
                    era = EraOf(map->second, origMax, heroic != heroicOf.end());
                else
                    era = rank == CREATURE_ELITE_WORLDBOSS && origMax <= 63 ? ERA_KEEP : EraByLevel(origMax);   // Ragnaros, summoned by his steward
                // The bosses of the old world - Azuregos, Kazzak, the dragons - stay as they are. Whatever else stands
                // above 60 in the old world was sent there from Outland or Northrend, and goes by their measure.
                if (era == ERA_CLASSIC && rank == CREATURE_ELITE_WORLDBOSS)
                    era = ERA_KEEP;
                else if (era == ERA_CLASSIC && origMax > 60)
                    era = EraByLevel(origMax);

                uint8 const newMin = enabled ? Compress(origMin, era) : origMin;
                uint8 const newMax = enabled ? std::max(newMin, Compress(origMax, era)) : origMax;
                uint32 const newMinGold = ScaleMoney(minGold, origMin, newMin);
                uint32 const newMaxGold = ScaleMoney(maxGold, origMax, newMax);
                if (newMin != origMin || newMax != origMax)
                    ++changed;
                rows.push_back(Acore::StringFormat("({},{},{},{},{})", entry, newMin, newMax, newMinGold, newMaxGold));
            } while (result->NextRow());

        WriteRows("INSERT INTO `ageless_sixty_creature` (`entry`, `applied_min`, `applied_max`, `applied_mingold`, `applied_maxgold`) VALUES ", rows,
            " ON DUPLICATE KEY UPDATE `applied_min` = VALUES(`applied_min`), `applied_max` = VALUES(`applied_max`), "
            "`applied_mingold` = VALUES(`applied_mingold`), `applied_maxgold` = VALUES(`applied_maxgold`)");
        WorldDatabase.DirectExecute(
            "UPDATE `creature_template` t JOIN `ageless_sixty_creature` a ON a.`entry` = t.`entry` SET "
            "t.`minlevel` = a.`applied_min`, t.`maxlevel` = a.`applied_max`, t.`mingold` = a.`applied_mingold`, t.`maxgold` = a.`applied_maxgold`");
        LOG_INFO("server.loading", ">> AgelessSixty: {} of {} creature(s) {}.", changed, rows.size(),
            enabled ? "on the journey from 1 to 60" : "back at their original levels");
    }

    /// Where a quest belongs: the map of whoever or whatever starts it.
    std::unordered_map<uint32, uint32> QuestMaps(std::unordered_map<uint32, uint32> const& creatureMaps)
    {
        std::unordered_map<uint32, uint32> maps;
        if (QueryResult result = WorldDatabase.Query("SELECT `id`, `quest` FROM `creature_queststarter`"))
            do
            {
                auto found = creatureMaps.find((*result)[0].Get<uint32>());
                if (found != creatureMaps.end())
                    maps.emplace((*result)[1].Get<uint32>(), found->second);
            } while (result->NextRow());
        if (QueryResult result = WorldDatabase.Query("SELECT q.`quest`, MAX(g.`map`) FROM `gameobject_queststarter` q JOIN `gameobject` g ON g.`id` = q.`id` GROUP BY q.`quest`"))
            do
                maps.emplace((*result)[0].Get<uint32>(), (*result)[1].Get<uint32>());
            while (result->NextRow());
        return maps;
    }

    void CompressQuests()
    {
        WorldDatabase.DirectExecute(
            "CREATE TABLE IF NOT EXISTS `ageless_sixty_quest` ("
            "`id` INT UNSIGNED NOT NULL PRIMARY KEY, "
            "`orig_level` SMALLINT NOT NULL DEFAULT 0, `orig_min` TINYINT UNSIGNED NOT NULL DEFAULT 0, `orig_money` INT NOT NULL DEFAULT 0, "
            "`applied_level` SMALLINT NOT NULL DEFAULT 0, `applied_min` TINYINT UNSIGNED NOT NULL DEFAULT 0, `applied_money` INT NOT NULL DEFAULT 0"
            ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='mod-ageless-sixty: the original levels of the quests, and what the module made of them'");
        WorldDatabase.DirectExecute(
            "INSERT IGNORE INTO `ageless_sixty_quest` SELECT `ID`, `QuestLevel`, `MinLevel`, `RewardMoney`, `QuestLevel`, `MinLevel`, `RewardMoney` FROM `quest_template`");
        WorldDatabase.DirectExecute(
            "UPDATE `ageless_sixty_quest` a JOIN `quest_template` q ON q.`ID` = a.`id` SET "
            "a.`orig_level` = q.`QuestLevel`, a.`orig_min` = q.`MinLevel`, a.`orig_money` = q.`RewardMoney`, "
            "a.`applied_level` = q.`QuestLevel`, a.`applied_min` = q.`MinLevel`, a.`applied_money` = q.`RewardMoney` "
            "WHERE q.`QuestLevel` <> a.`applied_level` OR q.`MinLevel` <> a.`applied_min` OR q.`RewardMoney` <> a.`applied_money`");

        std::unordered_map<uint32, uint32> const maps = QuestMaps(SpawnMaps());
        std::vector<std::string> rows;
        uint32 changed = 0;
        if (QueryResult result = WorldDatabase.Query("SELECT `id`, `orig_level`, `orig_min`, `orig_money` FROM `ageless_sixty_quest`"))
            do
            {
                Field* fields = result->Fetch();
                uint32 const id = fields[0].Get<uint32>();
                int32 const level = fields[1].Get<int32>();
                uint8 const minLevel = fields[2].Get<uint8>();
                int32 const money = fields[3].Get<int32>();
                uint8 const reference = uint8(std::clamp<int32>(level > 0 ? level : minLevel, 0, 255));

                auto map = maps.find(id);
                Era era = map != maps.end() ? EraOf(map->second, reference, false) : EraByLevel(reference);
                if (era == ERA_CLASSIC && reference > 60)
                    era = EraByLevel(reference);       // the way to Outland and Northrend starts in the old world
                int32 const newLevel = enabled && level > 0 ? Compress(uint8(std::min<int32>(level, 255)), era) : level;
                uint8 newMin = enabled && minLevel ? Compress(minLevel, era) : minLevel;
                if (enabled && newLevel > 0 && newMin > newLevel)
                    newMin = uint8(newLevel);
                int32 const newMoney = money > 0 && level > 0 ? int32(ScaleMoney(uint32(money), uint8(std::min<int32>(level, 255)), uint8(newLevel))) : money;
                if (newLevel != level || newMin != minLevel)
                    ++changed;
                rows.push_back(Acore::StringFormat("({},{},{},{})", id, newLevel, newMin, newMoney));
            } while (result->NextRow());

        WriteRows("INSERT INTO `ageless_sixty_quest` (`id`, `applied_level`, `applied_min`, `applied_money`) VALUES ", rows,
            " ON DUPLICATE KEY UPDATE `applied_level` = VALUES(`applied_level`), `applied_min` = VALUES(`applied_min`), `applied_money` = VALUES(`applied_money`)");
        WorldDatabase.DirectExecute(
            "UPDATE `quest_template` q JOIN `ageless_sixty_quest` a ON a.`id` = q.`ID` SET "
            "q.`QuestLevel` = a.`applied_level`, q.`MinLevel` = a.`applied_min`, q.`RewardMoney` = a.`applied_money`");
        LOG_INFO("server.loading", ">> AgelessSixty: {} of {} quest(s) {}.", changed, rows.size(),
            enabled ? "on the journey from 1 to 60" : "back at their original levels");
    }

    // ------------------------------------------------------------------------------------------ at runtime

    /// What a creature's spells are worth at its new level: its levels before and after.
    struct Shift
    {
        uint8 origMin, origMax, newMin, newMax;
    };
    std::unordered_map<uint32, Shift> shifts;

    void LoadShifts()
    {
        shifts.clear();
        if (QueryResult result = WorldDatabase.Query("SELECT `entry`, `orig_min`, `orig_max`, `applied_min`, `applied_max` FROM `ageless_sixty_creature` "
            "WHERE `orig_min` <> `applied_min` OR `orig_max` <> `applied_max`"))
            do
            {
                Field* fields = result->Fetch();
                shifts[fields[0].Get<uint32>()] = { fields[1].Get<uint8>(), fields[2].Get<uint8>(), fields[3].Get<uint8>(), fields[4].Get<uint8>() };
            } while (result->NextRow());
    }

    /// What a blow is worth at a level: the weapon damage and attack power of that level's base stats.
    double HitAt(uint8 level, CreatureTemplate const* info)
    {
        CreatureBaseStats const* stats = sObjectMgr->GetCreatureBaseStats(level, info->unit_class);
        if (!stats)
            return 0.0;
        uint8 const expansion = uint8(std::min<int32>(info->expansion, MAX_EXPANSIONS - 1));
        return stats->BaseDamage[expansion] + stats->AttackPower / 14.0 * 2.0;
    }

    /// How much less a creature's spells hit now than at the level they were made for.
    double SpellFactor(Unit* attacker)
    {
        Creature* creature = attacker ? attacker->ToCreature() : nullptr;
        if (!creature && attacker)
            if (Unit* owner = attacker->GetCharmerOrOwner())
                creature = owner->ToCreature();      // a totem or a summon of a creature
        if (!creature)
            return 1.0;
        CreatureTemplate const* info = creature->GetCreatureTemplate();
        auto shift = info ? shifts.find(info->Entry) : shifts.end();
        if (shift == shifts.end())
            return 1.0;
        Shift const& s = shift->second;
        uint8 const now = creature->GetLevel();
        // The original level this creature stands for: the same place in its original range.
        double const place = s.newMax > s.newMin ? double(std::clamp<int>(now, s.newMin, s.newMax) - s.newMin) / double(s.newMax - s.newMin) : 0.0;
        uint8 const was = uint8(std::lround(s.origMin + place * (s.origMax - s.origMin)));
        double const before = HitAt(was, info);
        double const after = HitAt(now, info);
        if (before <= 0.0 || after <= 0.0 || after >= before)
            return 1.0;
        return after / before;
    }

    /// The entry levels of the instances follow the journey.
    void SetInstanceEntries()
    {
        uint32 changed = 0;
        for (AsInstanceEntry const& instance : AsInstances)
            if (DungeonProgressionRequirements const* access = sObjectMgr->GetAccessRequirement(instance.map, Difficulty(instance.difficulty)))
            {
                auto* writable = const_cast<DungeonProgressionRequirements*>(access);
                if (writable->levelMin != instance.entry)
                {
                    writable->levelMin = uint8(instance.entry);
                    ++changed;
                }
            }
        LOG_INFO("server.loading", ">> AgelessSixty: {} instance(s) can be entered at their new levels.", changed);
    }
}

class AgelessSixtyWorld : public WorldScript
{
public:
    AgelessSixtyWorld() : WorldScript("AgelessSixtyWorld") { }

    void OnAfterConfigLoad(bool reload) override
    {
        enabled = sConfigMgr->GetOption<bool>("AgelessSixty.Enable", true);
        debug = sConfigMgr->GetOption<bool>("AgelessSixty.Debug", false);
        if (reload)
            return;     // the levels are set once, before the world reads them
        // Runs before the core loads creatures and quests: whatever it reads is already on the journey.
        CompressCreatures();
        CompressQuests();
    }

    void OnBeforeWorldInitialized() override
    {
        LoadShifts();
        if (enabled)
            SetInstanceEntries();
    }
};

class AgelessSixtyDamage : public UnitScript
{
public:
    AgelessSixtyDamage() : UnitScript("AgelessSixtyDamage", true, { UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN, UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK }) { }

    void ModifySpellDamageTaken(Unit* /*target*/, Unit* attacker, int32& damage, SpellInfo const* /*spellInfo*/) override
    {
        if (!enabled || damage <= 0)
            return;
        double const factor = SpellFactor(attacker);
        if (factor < 1.0)
            damage = std::max<int32>(1, int32(std::lround(damage * factor)));
    }

    void ModifyPeriodicDamageAurasTick(Unit* target, Unit* attacker, uint32& damage, SpellInfo const* spellInfo) override
    {
        // The same hook carries heals over time; those are left alone.
        if (!enabled || !damage || (spellInfo && spellInfo->IsPositive()) || (target && attacker && target->IsFriendlyTo(attacker)))
            return;
        double const factor = SpellFactor(attacker);
        if (factor < 1.0)
            damage = std::max<uint32>(1, uint32(std::lround(damage * factor)));
    }
};

void AddSC_ageless_sixty()
{
    new AgelessSixtyWorld();
    new AgelessSixtyDamage();
}
