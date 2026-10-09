/*
 * The journey in the world database: what the module changes there at startup, and what it keeps of the originals.
 * Talks to the database through three small callbacks, so the same code runs in the server and on a test bench.
 * Released under GNU AGPL v3: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */
#pragma once

#include "JourneyRules.h"

#include <array>
#include <cstdint>
#include <functional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace journey
{
    using Row = std::vector<std::string>;

    struct Db
    {
        /// Runs a query and hands over its rows one by one (NULL as an empty string).
        std::function<void(std::string const& sql, std::function<void(Row const&)> const& onRow)> query;
        std::function<void(std::string const& sql)> execute;
        std::function<void(std::string const& line)> log;
    };

    /// A table of a DBC file: fixed records of 32-bit fields, and a block of strings they point into.
    struct DbcFile
    {
        uint32_t fields = 0;
        std::vector<uint32_t> values;           // records x fields
        std::string strings;
        std::unordered_map<uint32_t, size_t> rowOf;     // id (field 0) -> record

        bool Read(std::string const& file);
        bool Has(uint32_t id) const { return rowOf.count(id) != 0; }
        uint32_t Get(uint32_t id, uint32_t field) const { return values[rowOf.at(id) * fields + field]; }
        std::string String(uint32_t offset) const;
    };

    /// The stat budget of an item level (RandPropPoints.dbc), for epic, rare and lesser quality.
    struct PropertyPoints
    {
        std::vector<std::array<uint32_t, 3>> byItemLevel;
        uint32_t Get(int itemLevel, int quality) const;
        bool Empty() const { return byItemLevel.empty(); }
    };
    bool ReadPropertyPoints(std::string const& dbcFile, PropertyPoints& out);

    /// A creature's levels - and the table of base stats it uses - before and after.
    struct Shift
    {
        uint8_t origMin, origMax, newMin, newMax, origExpansion, newExpansion;
    };

    /// How hard the creatures are: multipliers by part of the world and rank, -1 = take the general one.
    struct Difficulty
    {
        enum Stat { HEALTH, DAMAGE, SPELL, ARMOR, STATS };
        enum Rank { NORMAL, ELITE, RARE_ELITE, WORLD_BOSS, RARE, RANKS };
        static constexpr int PARTS = 4;         // old world, Outland, Northrend, endgame
        double general[RANKS][STATS];
        double byPart[PARTS][RANKS][STATS];

        Difficulty();
        double Get(int part, int rank, int stat) const;
    };

    /// What the server does at runtime with what was found in the database.
    struct Outcome
    {
        std::unordered_map<uint32_t, Shift> shifts;                 // creature entry
        std::unordered_map<uint32_t, double> spellMultipliers;      // creature entry: difficulty of its spells
        std::unordered_map<uint32_t, uint8_t> questParts;           // quest -> part of the world
        std::unordered_map<uint32_t, double> spellFactors;          // spells of items: what their numbers are worth now
        std::unordered_map<uint32_t, double> enchantFactors;        // socket bonuses and enchantments of items' spells
        std::unordered_map<uint32_t, uint8_t> gemEras;              // GemProperties id -> ITEM_TBC / ITEM_WOTLK
        std::unordered_map<uint32_t, uint8_t> craftEras;            // a spell taught by a profession -> ITEM_TBC / ITEM_WOTLK
        std::set<uint32_t> tooltipSpells;                           // scaled in spell_dbc: the client gets them too
        std::array<double, 4> endgameRatio{ 1.0, 1.0, 1.0, 1.0 };  // by ItemEra: what the stats of its top rewards keep
        std::array<int, 4> mountLevels{ 20, 40, 60, 70 };           // riding, fast riding, flying, fast flying
    };

    struct Settings
    {
        bool enabled = true;
        std::string prefix;             // of the module's own tables
        bool items = true;
        bool consumables = true;
        bool enchantments = true;
        bool trainers = true;
        bool battlegrounds = true;
        bool tooltips = true;           // scaled spells of items go to spell_dbc, so the client shows them
        bool journeyBaseStats = true;   // creatures on the way take the base stats of the old world
        Difficulty difficulty;
    };

    /// Everything the module does to the world database, in one go: creatures, items, quests, trainers,
    /// battlegrounds, the spells of items and the tables the bots read. Run before the server reads them.
    /// `spellDbc` is the server's Spell.dbc (may be empty: then the spells of items are scaled in memory only).
    void Run(Db const& db, Settings const& settings, PropertyPoints const& points, DbcFile const& spellDbc, Outcome& outcome);
}
