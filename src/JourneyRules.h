/*
 * The rules of the journey: what a level, an item, an amount of money becomes. Plain arithmetic, nothing of the
 * core - the same rules serve the world database at startup and the test bench that checks them.
 * Released under GNU AGPL v3: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */
#pragma once

#include <cstdint>

namespace journey
{
    // ---------------------------------------------------------------------------------------------- levels

    enum Era : uint8_t
    {
        ERA_KEEP,           // left as it is: classic raids, world bosses, what is above the journey
        ERA_CLASSIC,        // the old world, its dungeons, the starting zones of the blood elves and draenei
        ERA_OUTLAND,        // Outland and its normal dungeons
        ERA_NORTHREND,      // Northrend and its normal dungeons
        ERA_TOP_OUTLAND,    // raids and heroic dungeons of Outland: the end of the journey, at 60
        ERA_TOP_NORTHREND   // ... and of Northrend
    };

    /// The old world: 1..55 to 1..35, and 60 to 38.
    int Classic(int level);
    /// Outland: 58..67 to 30..43, and 70 to 47. Below 58 like the old world.
    int Outland(int level);
    /// Northrend: 68..77 to 40..55, and 80 to 60. Below 68 like the old world.
    int Northrend(int level);
    /// Raids and heroics: their trash at 60, their bosses a few levels above.
    int Top(int level, int base);
    int Compress(int level, Era era);

    /// The era of something found on a map, at a level.
    Era EraOf(uint32_t map, int level, bool heroic);
    /// Something that is nowhere to be found: judged by its level.
    Era EraByLevel(int level);
    bool IsInstanceOfOutland(uint32_t map);
    bool IsInstanceOfNorthrend(uint32_t map);

    /// Money goes with the square of the level, roughly; it shrinks with it.
    uint32_t ScaleMoney(uint32_t money, int from, int to);

    // ---------------------------------------------------------------------------------------------- items

    enum ItemEra : uint8_t
    {
        ITEM_KEEP,          // no level, or the raids of the old world: as it is
        ITEM_CLASSIC,
        ITEM_TBC,
        ITEM_WOTLK
    };

    struct ItemPlan
    {
        ItemEra era = ITEM_KEEP;
        bool endgame = false;   // a reward of the raids, heroics and arenas of Outland or Northrend: at 60, above the old raids
        int requiredLevel = 0;
        int itemLevel = 0;
    };

    /// What an item becomes: its required level follows the journey, its item level follows the required level -
    /// so that the stats of a level-37 item are those of a level-37 item, whichever game it came from.
    ItemPlan PlanItem(int quality, int requiredLevel, int itemLevel);
}
