/*
 * Released under GNU AGPL v3: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */
#include "JourneyDb.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <set>
#include <unordered_set>

namespace journey
{
    // ---------------------------------------------------------------------------------------- stat budgets

    uint32_t PropertyPoints::Get(int itemLevel, int quality) const
    {
        if (itemLevel <= 0 || size_t(itemLevel) >= byItemLevel.size())
            return 0;
        auto const& points = byItemLevel[size_t(itemLevel)];
        return quality >= 4 ? points[0] : quality == 3 ? points[1] : points[2];
    }

    bool ReadPropertyPoints(std::string const& dbcFile, PropertyPoints& out)
    {
        // WDBC: a header of five words, then fixed records - here the item level and three times five budgets.
        std::ifstream file(dbcFile, std::ios::binary);
        if (!file)
            return false;
        uint32_t header[5] = {};
        if (!file.read(reinterpret_cast<char*>(header), sizeof(header)) || header[0] != 0x43424457 /* WDBC */ ||
            header[2] < 16 || header[3] != header[2] * 4)
            return false;
        std::vector<uint32_t> record(header[2]);
        for (uint32_t i = 0; i < header[1]; ++i)
        {
            if (!file.read(reinterpret_cast<char*>(record.data()), header[3]))
                return false;
            uint32_t const itemLevel = record[0];
            if (itemLevel > 2000)
                continue;
            if (out.byItemLevel.size() <= itemLevel)
                out.byItemLevel.resize(itemLevel + 1, { 0, 0, 0 });
            out.byItemLevel[itemLevel] = { record[1], record[6], record[11] };
        }
        return !out.byItemLevel.empty();
    }

    namespace
    {
        // ------------------------------------------------------------------------------------ the ledger
        //
        // Every table the module changes gets a ledger of its own in the world database: for each row it touches,
        // the original value of each column and the value the module gave it. A value somebody else changed since
        // (an update of the database) is noticed - it no longer matches what the module gave - and taken as the
        // new original. Switched off, the module gives every row its original back.

        struct Column
        {
            std::string name;
            bool real = false;
        };

        struct Ledger
        {
            std::string source;
            std::string backup;
            std::string comment;
            std::vector<std::string> keys;
            std::vector<Column> columns;
            std::vector<std::string> extras;    // read along, over `s` and `join`
            std::string join;
            std::string where;                  // which rows of the source are kept on the ledger, over `s`
            uint64_t chunk = 1000000000ULL;     // rows are read by ranges of the first key
        };

        struct Entry
        {
            Row keys;
            std::vector<double> orig;
            Row extra;
        };

        struct Tally
        {
            size_t rows = 0;
            size_t moved = 0;       // differs from the original
            size_t written = 0;     // differs from what was there
        };

        std::string Quote(std::string const& name) { return "`" + name + "`"; }

        std::string Format(double value, bool real)
        {
            char buffer[64];
            if (real)
                std::snprintf(buffer, sizeof(buffer), "%.6g", value);
            else
                std::snprintf(buffer, sizeof(buffer), "%lld", static_cast<long long>(std::llround(value)));
            return buffer;
        }

        bool Same(double a, double b, bool real)
        {
            if (!real)
                return std::llround(a) == std::llround(b);
            return std::fabs(a - b) <= 1e-5 * std::max(1.0, std::fabs(a));
        }

        double Number(std::string const& text) { return text.empty() ? 0.0 : std::strtod(text.c_str(), nullptr); }
        long Int(std::string const& text) { return text.empty() ? 0 : std::strtol(text.c_str(), nullptr, 10); }

        template <typename Make>
        std::string List(size_t count, Make make, char const* separator = ", ")
        {
            std::string out;
            for (size_t i = 0; i < count; ++i)
                out += (i ? separator : "") + make(i);
            return out;
        }

        Tally Keep(Db const& db, Ledger const& ledger, std::function<std::vector<double>(Entry const&)> const& compute)
        {
            auto const& keys = ledger.keys;
            auto const& columns = ledger.columns;
            std::string const source = Quote(ledger.source), backup = Quote(ledger.backup);
            std::string const keyJoin = List(keys.size(), [&](size_t i) { return "b." + Quote(keys[i]) + " = s." + Quote(keys[i]); }, " AND ");
            // Whether a row of the source differs from what the module gave it.
            std::string const differs = "(" + List(columns.size(), [&](size_t i)
                { return "NOT (s." + Quote(columns[i].name) + " <=> b." + Quote("applied_" + columns[i].name) + ")"; }, " OR ") + ")";
            std::string const pairs = List(columns.size(), [&](size_t i)
                { return "s." + Quote(columns[i].name) + " AS " + Quote("orig_" + columns[i].name) + ", s." + Quote(columns[i].name) + " AS " + Quote("applied_" + columns[i].name); });
            std::string const keyList = List(keys.size(), [&](size_t i) { return Quote(keys[i]); });
            std::string const sKeys = List(keys.size(), [&](size_t i) { return "s." + Quote(keys[i]); });

            db.execute("CREATE TABLE IF NOT EXISTS " + backup + " (PRIMARY KEY (" + keyList + ")) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='" +
                ledger.comment + "' SELECT " + sKeys + ", " + pairs + " FROM " + source + " s LIMIT 0");
            db.execute("INSERT IGNORE INTO " + backup + " SELECT " + sKeys + ", " + List(columns.size(), [&](size_t i)
                { return "s." + Quote(columns[i].name) + ", s." + Quote(columns[i].name); }) + " FROM " + source + " s" +
                (ledger.where.empty() ? "" : " WHERE " + ledger.where));
            // Changed by somebody else since (an update of the database): a new original - for that column alone,
            // the others keep theirs. Two steps, so that no assignment depends on another of the same statement.
            db.execute("UPDATE " + backup + " b JOIN " + source + " s ON " + keyJoin + " SET " + List(columns.size(), [&](size_t i)
                { return "b." + Quote("orig_" + columns[i].name) + " = IF(s." + Quote(columns[i].name) + " <=> b." + Quote("applied_" + columns[i].name) +
                    ", b." + Quote("orig_" + columns[i].name) + ", s." + Quote(columns[i].name) + ")"; }) + " WHERE " + differs);
            db.execute("UPDATE " + backup + " b JOIN " + source + " s ON " + keyJoin + " SET " + List(columns.size(), [&](size_t i)
                { return "b." + Quote("applied_" + columns[i].name) + " = s." + Quote(columns[i].name); }) + " WHERE " + differs);

            double low = 0.0, high = -1.0;
            db.query("SELECT MIN(" + Quote(keys[0]) + "), MAX(" + Quote(keys[0]) + ") FROM " + backup, [&](Row const& row)
            {
                if (!row[0].empty())
                {
                    low = Number(row[0]);
                    high = Number(row[1]);
                }
            });

            Tally tally;
            std::string const select = "SELECT " + List(keys.size(), [&](size_t i) { return "b." + Quote(keys[i]); }) + ", " +
                List(columns.size(), [&](size_t i) { return "b." + Quote("orig_" + columns[i].name); }) + ", " +
                List(columns.size(), [&](size_t i) { return "b." + Quote("applied_" + columns[i].name); }) +
                (ledger.extras.empty() ? "" : ", " + List(ledger.extras.size(), [&](size_t i) { return ledger.extras[i]; })) +
                " FROM " + backup + " b JOIN " + source + " s ON " + keyJoin + (ledger.join.empty() ? "" : " " + ledger.join);
            std::string const insert = "INSERT INTO " + backup + " (" + keyList + ", " + List(columns.size(), [&](size_t i)
                { return Quote("orig_" + columns[i].name) + ", " + Quote("applied_" + columns[i].name); }) + ") VALUES ";
            std::string const update = " ON DUPLICATE KEY UPDATE " + List(columns.size(), [&](size_t i)
                { return Quote("applied_" + columns[i].name) + " = VALUES(" + Quote("applied_" + columns[i].name) + ")"; });

            for (double from = low; from <= high; from += double(ledger.chunk))
            {
                std::string const range = "b." + Quote(keys[0]) + " BETWEEN " + Format(from, false) + " AND " + Format(from + double(ledger.chunk) - 1, false);
                std::vector<std::string> rows;
                db.query(select + " WHERE " + range, [&](Row const& row)
                {
                    Entry entry;
                    entry.keys.assign(row.begin(), row.begin() + keys.size());
                    std::vector<double> applied(columns.size());
                    entry.orig.resize(columns.size());
                    for (size_t i = 0; i < columns.size(); ++i)
                    {
                        entry.orig[i] = Number(row[keys.size() + i]);
                        applied[i] = Number(row[keys.size() + columns.size() + i]);
                    }
                    entry.extra.assign(row.begin() + keys.size() + 2 * columns.size(), row.end());

                    std::vector<double> const next = compute(entry);
                    ++tally.rows;
                    bool moved = false, write = false;
                    for (size_t i = 0; i < columns.size(); ++i)
                    {
                        moved |= !Same(next[i], entry.orig[i], columns[i].real);
                        write |= !Same(next[i], applied[i], columns[i].real);
                    }
                    tally.moved += moved;
                    if (!write)
                        return;
                    rows.push_back("(" + List(keys.size(), [&](size_t i) { return entry.keys[i]; }) + ", " + List(columns.size(), [&](size_t i)
                        { return Format(entry.orig[i], columns[i].real) + ", " + Format(next[i], columns[i].real); }) + ")");
                });
                if (rows.empty())
                    continue;
                tally.written += rows.size();
                for (size_t i = 0; i < rows.size(); i += 1000)
                {
                    std::string values;
                    for (size_t j = i; j < std::min(rows.size(), i + 1000); ++j)
                        values += (j > i ? "," : "") + rows[j];
                    db.execute(insert + values + update);
                }
                db.execute("UPDATE " + source + " s JOIN " + backup + " b ON " + keyJoin + " SET " + List(columns.size(), [&](size_t i)
                    { return "s." + Quote(columns[i].name) + " = b." + Quote("applied_" + columns[i].name); }) +
                    " WHERE " + range + " AND " + differs);
            }
            return tally;
        }

        void Report(Db const& db, Settings const& settings, Tally const& tally, char const* what)
        {
            db.log(std::to_string(tally.moved) + " of " + std::to_string(tally.rows) + " " + what + " " +
                (settings.enabled ? "on the journey from 1 to 60" : "back at their original values") +
                " (" + std::to_string(tally.written) + " written)");
        }

        // ------------------------------------------------------------------------------------ where things are

        /// The map most spawns of each creature are on.
        std::unordered_map<uint32_t, uint32_t> SpawnMaps(Db const& db)
        {
            std::string idColumn = "id";
            db.query("SHOW COLUMNS FROM `creature` LIKE 'id1'", [&](Row const&) { idColumn = "id1"; });
            std::unordered_map<uint32_t, std::pair<uint32_t, long>> best;
            db.query("SELECT `" + idColumn + "`, `map`, COUNT(*) FROM `creature` GROUP BY `" + idColumn + "`, `map`", [&](Row const& row)
            {
                auto& known = best[uint32_t(Int(row[0]))];
                long const count = Int(row[2]);
                if (count > known.second)
                    known = { uint32_t(Int(row[1])), count };
            });
            std::unordered_map<uint32_t, uint32_t> maps;
            for (auto const& entry : best)
                maps[entry.first] = entry.second.first;
            return maps;
        }

        // ------------------------------------------------------------------------------------ creatures

        void Creatures(Db const& db, Settings const& settings, std::unordered_map<uint32_t, uint32_t> const& maps, Outcome& outcome)
        {
            std::unordered_map<uint32_t, uint32_t> heroicOf;    // a template of a heroic or raid difficulty -> its normal one
            db.query("SELECT `entry`, `difficulty_entry_1`, `difficulty_entry_2`, `difficulty_entry_3` FROM `creature_template` "
                "WHERE `difficulty_entry_1` OR `difficulty_entry_2` OR `difficulty_entry_3`", [&](Row const& row)
            {
                for (size_t i = 1; i <= 3; ++i)
                    if (uint32_t const other = uint32_t(Int(row[i])))
                        heroicOf[other] = uint32_t(Int(row[0]));
            });

            Ledger ledger;
            ledger.source = "creature_template";
            ledger.backup = settings.prefix + "_creature";
            ledger.comment = "The original levels of the creatures, and what the journey made of them";
            ledger.keys = { "entry" };
            ledger.columns = { { "minlevel" }, { "maxlevel" }, { "mingold" }, { "maxgold" }, { "exp" } };
            ledger.extras = { "s.`rank`" };

            Tally const tally = Keep(db, ledger, [&](Entry const& entry)
            {
                std::vector<double> next = entry.orig;
                if (!settings.enabled)
                    return next;
                uint32_t const id = uint32_t(Int(entry.keys[0]));
                int const origMin = int(entry.orig[0]), origMax = int(entry.orig[1]), origExp = int(entry.orig[4]);
                bool const worldBoss = Int(entry.extra[0]) == 3;

                Era era;
                auto heroic = heroicOf.find(id);
                uint32_t const spawnedAs = heroic != heroicOf.end() ? heroic->second : id;
                auto map = maps.find(spawnedAs);
                if (map != maps.end())
                    era = EraOf(map->second, origMax, heroic != heroicOf.end());
                else
                    era = worldBoss && origMax <= 63 ? ERA_KEEP : EraByLevel(origMax);     // Ragnaros, summoned by his steward
                // The bosses of the old world - Azuregos, Kazzak, the dragons - stay as they are. Whatever else stands
                // above 60 in the old world was sent there from Outland or Northrend, and goes by their measure.
                if (era == ERA_CLASSIC && worldBoss)
                    era = ERA_KEEP;
                else if (era == ERA_CLASSIC && origMax > 60)
                    era = EraByLevel(origMax);

                int const newMin = Compress(origMin, era);
                int const newMax = std::max(newMin, Compress(origMax, era));
                next[0] = newMin;
                next[1] = newMax;
                next[2] = ScaleMoney(uint32_t(entry.orig[2]), origMin, newMin);
                next[3] = ScaleMoney(uint32_t(entry.orig[3]), origMax, newMax);
                // On the way, a creature of Outland or Northrend is a creature of the journey: the base stats of the old
                // world, which go up to 60 without the jumps of the later tables. The raids and heroics keep theirs -
                // they are the harder end of the journey.
                bool const onTheWay = era == ERA_CLASSIC || era == ERA_OUTLAND || era == ERA_NORTHREND;
                if (onTheWay && (newMin != origMin || newMax != origMax))
                    next[4] = 0;
                if (newMin != origMin || newMax != origMax)
                    outcome.shifts[id] = { uint8_t(origMin), uint8_t(origMax), uint8_t(newMin), uint8_t(newMax), uint8_t(origExp), uint8_t(next[4]) };
                return next;
            });
            Report(db, settings, tally, "creatures");
        }

        // ------------------------------------------------------------------------------------ quests

        void Quests(Db const& db, Settings const& settings, std::unordered_map<uint32_t, uint32_t> const& creatureMaps)
        {
            std::unordered_map<uint32_t, uint32_t> maps;    // quest -> the map of whoever or whatever starts it
            db.query("SELECT `id`, `quest` FROM `creature_queststarter`", [&](Row const& row)
            {
                auto found = creatureMaps.find(uint32_t(Int(row[0])));
                if (found != creatureMaps.end())
                    maps.emplace(uint32_t(Int(row[1])), found->second);
            });
            db.query("SELECT q.`quest`, MAX(g.`map`) FROM `gameobject_queststarter` q JOIN `gameobject` g ON g.`id` = q.`id` GROUP BY q.`quest`",
                [&](Row const& row) { maps.emplace(uint32_t(Int(row[0])), uint32_t(Int(row[1]))); });

            // What a quest gives can ask for more than its new level: a quest of the old world that leads into the
            // raids, or one of Outland that hands out what Outland's raids hand out. Such a quest stands at the level
            // of its reward. (The items are on the journey already.)
            std::unordered_map<uint32_t, int> rewardLevel;
            for (char const* column : { "RewardItem1", "RewardItem2", "RewardItem3", "RewardItem4", "RewardChoiceItemID1", "RewardChoiceItemID2",
                "RewardChoiceItemID3", "RewardChoiceItemID4", "RewardChoiceItemID5", "RewardChoiceItemID6" })
                db.query(std::string("SELECT q.`ID`, i.`RequiredLevel` FROM `quest_template` q JOIN `item_template` i ON i.`entry` = q.`") + column +
                    "` WHERE i.`class` IN (2, 4) AND i.`RequiredLevel` > 1", [&](Row const& row)
                {
                    int& level = rewardLevel[uint32_t(Int(row[0]))];
                    level = std::max(level, int(Int(row[1])));
                });

            Ledger ledger;
            ledger.source = "quest_template";
            ledger.backup = settings.prefix + "_quest";
            ledger.comment = "The original levels of the quests, and what the journey made of them";
            ledger.keys = { "ID" };
            ledger.columns = { { "QuestLevel" }, { "MinLevel" }, { "RewardMoney" } };

            Tally const tally = Keep(db, ledger, [&](Entry const& entry)
            {
                std::vector<double> next = entry.orig;
                if (!settings.enabled)
                    return next;
                int const level = int(entry.orig[0]), minLevel = int(entry.orig[1]);
                long const money = long(entry.orig[2]);
                int const reference = std::clamp(level > 0 ? level : minLevel, 0, 255);

                auto map = maps.find(uint32_t(Int(entry.keys[0])));
                Era era = map != maps.end() ? EraOf(map->second, reference, false) : EraByLevel(reference);
                if (era == ERA_CLASSIC && reference > 60)
                    era = EraByLevel(reference);        // the way to Outland and Northrend starts in the old world
                int newLevel = level > 0 ? Compress(std::min(level, 255), era) : level;
                auto reward = rewardLevel.find(uint32_t(Int(entry.keys[0])));
                if (newLevel > 0 && reward != rewardLevel.end() && reward->second > newLevel + 3)
                    newLevel = std::min(std::max(reward->second, newLevel), std::max(level, 1));
                int newMin = minLevel ? Compress(minLevel, era) : minLevel;
                if (newLevel > 0 && newMin > newLevel)
                    newMin = newLevel;
                next[0] = newLevel;
                next[1] = newMin;
                if (money > 0 && level > 0)
                    next[2] = ScaleMoney(uint32_t(money), std::min(level, 255), newLevel);
                return next;
            });
            Report(db, settings, tally, "quests");
        }

        // ------------------------------------------------------------------------------------ items

        /// What a character is worth at a level, in each game: the health of a creature of that level and game - the
        /// measure for what a potion, a meal or an elixir should give.
        struct Worth
        {
            std::vector<std::array<double, 3>> health;

            double At(int level, int expansion) const
            {
                if (health.empty())
                    return 0.0;
                auto const& row = health[size_t(std::clamp(level, 1, int(health.size()) - 1))];
                for (int e = std::clamp(expansion, 0, 2); e >= 0; --e)
                    if (row[e] > 1.0)
                        return row[e];
                return 0.0;
            }
        };

        Worth ReadWorth(Db const& db)
        {
            Worth worth;
            db.query("SELECT `level`, `basehp0`, `basehp1`, `basehp2` FROM `creature_classlevelstats` WHERE `class` = 1 ORDER BY `level`", [&](Row const& row)
            {
                size_t const level = size_t(Int(row[0]));
                if (level > 255)
                    return;
                if (worth.health.size() <= level)
                    worth.health.resize(level + 1, { 0.0, 0.0, 0.0 });
                worth.health[level] = { Number(row[1]), Number(row[2]), Number(row[3]) };
            });
            return worth;
        }

        int ExpansionOf(ItemEra era) { return era == ITEM_WOTLK ? 2 : era == ITEM_TBC ? 1 : 0; }

        void Items(Db const& db, Settings const& settings, PropertyPoints const& points, Outcome& outcome)
        {
            Worth const worth = ReadWorth(db);

            Ledger ledger;
            ledger.source = "item_template";
            ledger.backup = settings.prefix + "_item";
            ledger.comment = "The original levels and stats of the items, and what the journey made of them";
            ledger.keys = { "entry" };
            ledger.columns = { { "RequiredLevel" }, { "ItemLevel" } };
            for (int i = 1; i <= 10; ++i)
                ledger.columns.push_back({ "stat_value" + std::to_string(i) });                                  // 2..11
            ledger.columns.push_back({ "armor" });                                                              // 12
            ledger.columns.push_back({ "block" });                                                              // 13
            for (char const* name : { "dmg_min1", "dmg_max1", "dmg_min2", "dmg_max2" })
                ledger.columns.push_back({ name, true });                                                       // 14..17
            for (char const* name : { "holy_res", "fire_res", "nature_res", "frost_res", "shadow_res", "arcane_res" })
                ledger.columns.push_back({ name });                                                             // 18..23
            ledger.columns.push_back({ "SellPrice" });                                                          // 24
            ledger.columns.push_back({ "BuyPrice" });                                                           // 25
            ledger.extras = { "s.`Quality`", "s.`class`", "s.`GemProperties`", "s.`socketBonus`" };
            for (int i = 1; i <= 5; ++i)
                ledger.extras.push_back("s.`spellid_" + std::to_string(i) + "`");                                // 4..8
            for (int i = 1; i <= 5; ++i)
                ledger.extras.push_back("s.`spelltrigger_" + std::to_string(i) + "`");                           // 9..13
            ledger.where = "s.`RequiredLevel` > 1 AND s.`Quality` < 7 AND s.`ScalingStatDistribution` = 0";
            ledger.chunk = 50000;

            auto keepMost = [](std::unordered_map<uint32_t, double>& factors, uint32_t id, double factor)
            {
                // Shared by items of different levels: the gentler factor wins.
                auto found = factors.find(id);
                if (found == factors.end() || factor > found->second)
                    factors[id] = factor;
            };

            std::array<double, 4> endgameSum{ 0.0, 0.0, 0.0, 0.0 };
            std::array<size_t, 4> endgameCount{ 0, 0, 0, 0 };
            Tally const tally = Keep(db, ledger, [&](Entry const& entry)
            {
                std::vector<double> next = entry.orig;
                if (!settings.enabled)
                    return next;
                uint32_t const id = uint32_t(Int(entry.keys[0]));
                int const required = int(entry.orig[0]), itemLevel = int(entry.orig[1]);
                int const quality = int(Int(entry.extra[0])), itemClass = int(Int(entry.extra[1]));
                bool const equipment = itemClass == 2 /* weapon */ || itemClass == 4 /* armor */;

                ItemPlan const plan = PlanItem(quality, required, itemLevel);
                double statRatio = 1.0, spellRatio = 1.0;
                if (plan.era != ITEM_KEEP && (plan.requiredLevel != required || plan.itemLevel != itemLevel))
                {
                    next[0] = plan.requiredLevel;
                    if (equipment && itemLevel > 0)
                    {
                        next[1] = plan.itemLevel;
                        double const linear = double(plan.itemLevel) / double(itemLevel);
                        uint32_t const before = points.Get(itemLevel, quality), after = points.Get(plan.itemLevel, quality);
                        statRatio = before && after ? double(after) / double(before) : linear;
                        spellRatio = statRatio;
                        auto scale = [&](size_t i, double ratio)
                        {
                            if (entry.orig[i] == 0.0)
                                return;
                            double const value = std::round(entry.orig[i] * ratio);
                            next[i] = value != 0.0 ? value : (entry.orig[i] > 0 ? 1.0 : -1.0);
                        };
                        for (size_t i = 2; i <= 11; ++i)
                            scale(i, statRatio);
                        scale(12, linear);
                        scale(13, linear);
                        for (size_t i = 14; i <= 17; ++i)
                            if (entry.orig[i] != 0.0)
                                next[i] = std::max(1.0, std::round(entry.orig[i] * linear * 100.0) / 100.0);
                        for (size_t i = 18; i <= 23; ++i)
                            scale(i, statRatio);
                        next[24] = std::round(entry.orig[24] * linear * linear);
                        next[25] = std::round(entry.orig[25] * linear * linear);
                        if (plan.endgame)
                        {
                            endgameSum[plan.era] += statRatio;
                            ++endgameCount[plan.era];
                        }
                    }
                    else
                    {
                        double const before = worth.At(required, ExpansionOf(plan.era)), after = worth.At(plan.requiredLevel, 0);
                        if (before > 0.0 && after > 0.0)
                            spellRatio = std::min(1.0, after / before);
                    }
                }

                // A gem of Outland or Northrend: its stats come from the enchantment it carries.
                if (uint32_t const gem = uint32_t(Int(entry.extra[2])))
                    outcome.gemEras[gem] = uint8_t(id < 36000 ? ITEM_TBC : ITEM_WOTLK);
                // The spells of the item - on use, on equip, on hit - and its socket bonus go with it.
                for (size_t i = 0; i < 5; ++i)
                {
                    uint32_t const spell = uint32_t(Int(entry.extra[4 + i]));
                    long const trigger = Int(entry.extra[9 + i]);
                    if (spell && (trigger == 0 || trigger == 1 || trigger == 2 || trigger == 5))
                        keepMost(outcome.spellFactors, spell, spellRatio);
                }
                if (uint32_t const bonus = uint32_t(Int(entry.extra[3])))
                    keepMost(outcome.enchantFactors, bonus, statRatio);
                return next;
            });
            for (size_t era = 0; era < endgameSum.size(); ++era)
                if (endgameCount[era])
                    outcome.endgameRatio[era] = endgameSum[era] / double(endgameCount[era]);
            Report(db, settings, tally, "items");
            if (settings.enabled)
                db.log("the top rewards of Outland keep " + std::to_string(int(std::lround(outcome.endgameRatio[ITEM_TBC] * 100))) +
                    "% of their stats, those of Northrend " + std::to_string(int(std::lround(outcome.endgameRatio[ITEM_WOTLK] * 100))) + "%" +
                    (points.Empty() ? " (no RandPropPoints.dbc: stats follow the item level)" : ""));
        }

        // ------------------------------------------------------------------------------------ trainers and battlegrounds

        void Trainers(Db const& db, Settings const& settings)
        {
            // Riding and the professions; the class trainers teach the classes as CoA made them.
            Ledger ledger;
            ledger.source = "trainer_spell";
            ledger.backup = settings.prefix + "_trainer_spell";
            ledger.comment = "The original levels of riding and the professions, and what the journey made of them";
            ledger.keys = { "TrainerId", "SpellId" };
            ledger.columns = { { "ReqLevel" } };
            ledger.where = "s.`ReqLevel` > 0 AND s.`TrainerId` IN (SELECT `Id` FROM `trainer` WHERE `Type` IN (1, 2))";

            Tally const tally = Keep(db, ledger, [&](Entry const& entry)
            {
                std::vector<double> next = entry.orig;
                if (!settings.enabled)
                    return next;
                int const level = int(entry.orig[0]);
                long const spell = Int(entry.keys[1]);
                // Expert riding: flying, once at the doors of Outland.
                next[0] = spell == 34090 ? Outland(level) : Compress(level, ERA_CLASSIC);
                return next;
            });
            Report(db, settings, tally, "trainer spells");
        }

        void Battlegrounds(Db const& db, Settings const& settings)
        {
            // The Eye of the Storm, the Strand of the Ancients, the Isle of Conquest: open at 60.
            Ledger ledger;
            ledger.source = "battleground_template";
            ledger.backup = settings.prefix + "_battleground";
            ledger.comment = "The original entry levels of the battlegrounds, and what the journey made of them";
            ledger.keys = { "ID" };
            ledger.columns = { { "MinLvl" } };
            ledger.where = "s.`MinLvl` > 60";

            Tally const tally = Keep(db, ledger, [&](Entry const& entry)
            {
                std::vector<double> next = entry.orig;
                if (settings.enabled)
                    next[0] = std::min(entry.orig[0], 60.0);
                return next;
            });
            Report(db, settings, tally, "battlegrounds");
        }

        /// The enchantments the professions teach beyond the old world: which game they belong to, by the skill.
        void Crafts(Db const& db, Outcome& outcome)
        {
            auto note = [&](uint32_t spell, long rank)
            {
                if (spell && rank > 300)
                    outcome.craftEras[spell] = uint8_t(rank > 375 ? ITEM_WOTLK : ITEM_TBC);
            };
            db.query("SELECT `SpellId`, MAX(`ReqSkillRank`) FROM `trainer_spell` WHERE `ReqSkillLine` > 0 AND `ReqSkillRank` > 300 GROUP BY `SpellId`",
                [&](Row const& row) { note(uint32_t(Int(row[0])), Int(row[1])); });
            db.query("SELECT `RequiredSkillRank`, `spellid_1`, `spelltrigger_1`, `spellid_2`, `spelltrigger_2`, `spellid_3`, `spelltrigger_3` "
                "FROM `item_template` WHERE `class` = 9 AND `RequiredSkillRank` > 300", [&](Row const& row)
            {
                for (size_t i = 1; i + 1 < row.size(); i += 2)
                    if (Int(row[i + 1]) == 6)       // learn
                        note(uint32_t(Int(row[i])), Int(row[0]));
            });
        }
    }

    void Run(Db const& db, Settings const& settings, PropertyPoints const& points, Outcome& outcome)
    {
        std::unordered_map<uint32_t, uint32_t> const maps = SpawnMaps(db);
        Creatures(db, settings, maps, outcome);
        Items(db, settings, points, outcome);
        Quests(db, settings, maps);
        Trainers(db, settings);
        Battlegrounds(db, settings);
        if (settings.enabled)
            Crafts(db, outcome);
    }
}
