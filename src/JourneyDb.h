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

    /// What the server does at runtime with what was found in the database.
    struct Outcome
    {
        std::unordered_map<uint32_t, Shift> shifts;                 // creature entry
        std::unordered_map<uint32_t, double> spellFactors;          // spells of items: what their numbers are worth now
        std::unordered_map<uint32_t, double> enchantFactors;        // socket bonuses
        std::unordered_map<uint32_t, uint8_t> gemEras;              // GemProperties id -> ITEM_TBC / ITEM_WOTLK
        std::unordered_map<uint32_t, uint8_t> craftEras;            // a spell taught by a profession -> ITEM_TBC / ITEM_WOTLK
        std::array<double, 4> endgameRatio{ 1.0, 1.0, 1.0, 1.0 };  // by ItemEra: what the stats of its top rewards keep
    };

    struct Settings
    {
        bool enabled = true;
        std::string prefix;     // of the module's own tables
    };

    /// Everything the module does to the world database, in one go: creatures, quests, items, trainers and
    /// battlegrounds. Run before the server reads them.
    void Run(Db const& db, Settings const& settings, PropertyPoints const& points, Outcome& outcome);
}
