/*
 * The rules of the journey: what a level, an item, an amount of money becomes. Plain arithmetic, nothing of the
 * core - the same rules serve the world database at startup, the server at runtime and the test bench.
 * Released under GNU AGPL v3: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */
#pragma once

#include <cstdint>
#include <map>

namespace journey
{
    // ---------------------------------------------------------------------------------------------- the shape

    struct Range
    {
        int from = 1;
        int to = 1;
    };

    /// Everything the regulators of the module decide. The original levels each part is laid over are fixed:
    /// the old world's zones open from 1 to 55, Outland's from 58 to 67, Northrend's from 68 to 77.
    struct Shape
    {
        bool outland = true;                    // always on: a part left as it came is above the cap (kept for the bench)
        bool northrend = true;
        Range classic{ 1, 35 };
        Range outlandRange{ 30, 43 };
        Range northrendRange{ 40, 55 };
        int endgame = 60;                       // the level of Outland's and Northrend's heroics and raids
        int bossLevels = 3;                     // how far above it their bosses may stand
        Range outlandItems{ 75, 110 };          // item levels of Outland's top rewards at the endgame
        Range northrendItems{ 95, 140 };
        std::map<uint32_t, int> zoneEntries;    // zone id -> entry level, set by hand
        std::map<uint32_t, int> instanceEntries; // map id -> entry level, set by hand
    };

    void UseShape(Shape const& shape);
    Shape const& CurrentShape();

    // ---------------------------------------------------------------------------------------------- levels

    enum Era : uint8_t
    {
        ERA_KEEP,           // left as it is: classic raids, world bosses, what is above the journey
        ERA_CLASSIC,        // the old world, its dungeons, the starting zones of the blood elves and draenei
        ERA_OUTLAND,        // Outland and its normal dungeons
        ERA_NORTHREND,      // Northrend and its normal dungeons
        ERA_TOP_OUTLAND,    // raids and heroic dungeons of Outland: the end of the journey
        ERA_TOP_NORTHREND   // ... and of Northrend
    };

    /// The old world: entries 1..55 laid over the classic range.
    int Classic(int level);
    /// Outland: entries 58..67 laid over its range; below 58 like the old world.
    int Outland(int level);
    /// Northrend: entries 68..77 laid over its range, never above the endgame; below 68 like the old world.
    int Northrend(int level);
    /// Raids and heroics: their trash at the endgame, their bosses a few levels above.
    int Top(int level, int base);
    int Compress(int level, Era era);

    /// The era of something found on a map, at a level.
    Era EraOf(uint32_t map, int level, bool heroic);
    /// Something that is nowhere to be found: judged by its level.
    Era EraByLevel(int level);
    bool IsInstanceOfOutland(uint32_t map);
    bool IsInstanceOfNorthrend(uint32_t map);
    bool IsClassicRaid(uint32_t map);

    /// The part of the world an era belongs to, for the regulators that go by part: 0 old world, 1 Outland,
    /// 2 Northrend, 3 the endgame of Outland and Northrend.
    int PartOf(Era era);

    /// Money goes with the square of the level, roughly; it shrinks with it.
    uint32_t ScaleMoney(uint32_t money, int from, int to);

    // ---------------------------------------------------------------------------------------------- zones and instances

    /// The entry level of a zone, from its original one and the part it belongs to (0 classic, 1 Outland, 2
    /// Northrend). A level set by hand wins.
    int ZoneEntry(uint32_t zone, int part, int originalEntry, bool manual = true);

    enum InstanceKind : uint8_t { INSTANCE_DUNGEON, INSTANCE_HEROIC, INSTANCE_RAID };
    /// The entry level of an instance, from its original entry level and the level of its creatures.
    int InstanceEntry(uint32_t map, InstanceKind kind, int originalEntry, int contentLevel, bool manual = true);

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
        bool endgame = false;   // a reward of the raids, heroics and arenas of Outland or Northrend
        int requiredLevel = 0;
        int itemLevel = 0;
    };

    /// What an item becomes: its required level follows the journey, its item level follows the required level -
    /// so that the stats of a level-37 item are those of a level-37 item, whichever game it came from.
    ItemPlan PlanItem(int quality, int requiredLevel, int itemLevel);

    // ---------------------------------------------------------------------------------------------- the window

    /// Where a creature lifted to a character stands, relative to that character: from its place in its zone - the
    /// zone's lowest creatures `below` levels under the character, its highest `above` levels over - plus one for
    /// an elite or a rare, within the window.
    int WindowOffset(int originalLevel, int zoneOriginalEntry, int part, bool eliteOrRare, int below, int above);

    // ---------------------------------------------------------------------------------------------- spells

    /// A spell effect whose numbers are plain amounts - damage, healing, mana, stats, absorbs - not percentages,
    /// speeds or anything else (by the effect and aura numbers of Spell.dbc).
    bool IsAmountEffect(uint32_t effect, uint32_t aura);
    /// An effect that casts another spell: that spell goes with this one.
    bool IsTriggerEffect(uint32_t effect, uint32_t aura);
    /// An effect that puts an enchantment on an item (its MiscValue).
    bool IsEnchantEffect(uint32_t effect);
}
