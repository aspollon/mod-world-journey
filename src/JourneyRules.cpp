/*
 * Released under GNU AGPL v3: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */
#include "JourneyRules.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace journey
{
    namespace
    {
        std::set<uint32_t> const OutlandInstances = { 269, 540, 542, 543, 545, 546, 547, 552, 553, 554, 555, 556, 557, 558, 560, 585 };
        std::set<uint32_t> const NorthrendInstances = { 574, 575, 576, 578, 595, 599, 600, 601, 602, 604, 608, 619, 632, 650, 658, 668 };
        std::set<uint32_t> const OutlandRaids = { 532, 534, 544, 548, 550, 564, 565, 568, 580 };
        std::set<uint32_t> const NorthrendRaids = { 249, 533, 603, 615, 616, 624, 631, 649, 724 };
        std::set<uint32_t> const ClassicRaids = { 309, 409, 469, 509, 531 };

        Shape shape;

        int Clamp(double level)
        {
            return int(std::clamp(std::lround(level), 1L, 63L));
        }

        /// A level of the original range [low, high] laid over a range of the journey.
        double Lay(int level, int low, int high, Range const& range)
        {
            return range.from + double(level - low) * double(range.to - range.from) / double(high - low);
        }

        /// The usual item level of a leveling item at a required level, in each game. The games grew their item
        /// levels at different speeds: a level-58 item of Outland is worth an item level of about 90.
        double UsualClassic(int level) { return level + 5.0; }
        double UsualTbc(int level) { return std::max(UsualClassic(level), 90.0 + (level - 58) * 25.0 / 12.0); }
        double UsualWotlk(int level) { return std::max(UsualTbc(level), 130.0 + (level - 68) * 57.0 / 12.0); }
    }

    void UseShape(Shape const& s)
    {
        shape = s;
        shape.endgame = std::clamp(shape.endgame, 50, 60);
        shape.bossLevels = std::clamp(shape.bossLevels, 0, 3);
    }

    Shape const& CurrentShape() { return shape; }

    int Classic(int level)
    {
        return Clamp(Lay(level, 1, 55, shape.classic));
    }

    int Outland(int level)
    {
        if (level < 58)
            return Classic(level);
        if (!shape.outland)
            return level;
        return Clamp(Lay(level, 58, 67, shape.outlandRange));
    }

    int Northrend(int level)
    {
        if (level < 68)
            return Classic(level);
        if (!shape.northrend)
            return level;
        return std::min(Clamp(Lay(level, 68, 77, shape.northrendRange)), shape.endgame);
    }

    int Top(int level, int base)
    {
        return std::clamp(shape.endgame + level - base, shape.endgame, shape.endgame + shape.bossLevels);
    }

    int Compress(int level, Era era)
    {
        if (level <= 0)
            return level;
        switch (era)
        {
            // Above 60 in the old world: what Outland and Northrend sent there - the heralds at the Dark Portal, the
            // guards of the capitals, the generals of the Lich King's invasion.
            case ERA_CLASSIC:       return level <= 60 ? Classic(level) : level <= 72 ? Outland(level) : Northrend(level);
            case ERA_OUTLAND:       return Outland(level);
            case ERA_NORTHREND:     return Northrend(level);
            case ERA_TOP_OUTLAND:   return level >= 61 && shape.outland ? Top(level, 70) : level;
            case ERA_TOP_NORTHREND: return level >= 61 && shape.northrend ? Top(level, 80) : level;
            default:                return level;
        }
    }

    bool IsInstanceOfOutland(uint32_t map) { return OutlandInstances.count(map) || OutlandRaids.count(map); }
    bool IsInstanceOfNorthrend(uint32_t map) { return NorthrendInstances.count(map) || NorthrendRaids.count(map); }
    bool IsClassicRaid(uint32_t map) { return ClassicRaids.count(map) != 0; }

    Era EraOf(uint32_t map, int level, bool heroic)
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

    // Lower is the safe side: CoA's open-world scaling lifts a creature that is too low for a character, but
    // nothing lowers one that is too high.
    Era EraByLevel(int level)
    {
        if (level <= 60)
            return ERA_CLASSIC;
        return level <= 72 ? ERA_OUTLAND : ERA_NORTHREND;
    }

    int PartOf(Era era)
    {
        switch (era)
        {
            case ERA_OUTLAND:       return 1;
            case ERA_NORTHREND:     return 2;
            case ERA_TOP_OUTLAND:
            case ERA_TOP_NORTHREND: return 3;
            default:                return 0;
        }
    }

    uint32_t ScaleMoney(uint32_t money, int from, int to)
    {
        if (!money || from <= 0 || to >= from)
            return money;
        double const share = double(to) / double(from);
        return uint32_t(std::max(1.0, std::round(money * share * share)));
    }

    int ZoneEntry(uint32_t zone, int part, int originalEntry, bool manual)
    {
        auto set = shape.zoneEntries.find(zone);
        if (manual && set != shape.zoneEntries.end())
            return std::clamp(set->second, 1, shape.endgame);
        if ((part == 1 && !shape.outland) || (part == 2 && !shape.northrend))
            return originalEntry;           // a part left as it came keeps its levels
        int entry;
        if (originalEntry <= 1)
            entry = 1;
        else if (part == 1)
            entry = Outland(std::max(originalEntry, 58));
        else if (part == 2)
            entry = Northrend(std::max(originalEntry, 68));
        else
            entry = Classic(originalEntry);
        if (zone == 4080 && shape.outland)
            entry = shape.endgame - 5;      // the Isle of Quel'Danas: the last raid of Outland
        return std::clamp(entry, 1, std::max(1, shape.endgame - 5));
    }

    int InstanceEntry(uint32_t map, InstanceKind kind, int originalEntry, int contentLevel, bool manual)
    {
        auto set = shape.instanceEntries.find(map);
        if (manual && set != shape.instanceEntries.end())
            return std::clamp(set->second, 1, shape.endgame);

        bool const outland = OutlandInstances.count(map) || OutlandRaids.count(map);
        bool const northrend = NorthrendInstances.count(map) || NorthrendRaids.count(map);
        if ((outland && !shape.outland) || (northrend && !shape.northrend))
            return originalEntry;
        if (kind == INSTANCE_RAID && ClassicRaids.count(map))
            return originalEntry;
        if (kind != INSTANCE_DUNGEON)
            return shape.endgame;

        int level = std::max(contentLevel, originalEntry);
        int entry;
        if (outland)
        {
            if (map == 560)
                level = 66;     // Old Hillsbrad: the many townsfolk pull the average down
            entry = std::max(shape.outlandRange.from, Outland(std::min(level, 70) - 3));
        }
        else if (northrend)
            entry = std::max(shape.northrendRange.from, Northrend(std::min(level, 80) - 3));
        else
            // The old dungeons: their entry level in the database is a better guide than their creatures, some of
            // which CoA has added at other levels.
            entry = Classic(std::min(originalEntry + 3, 55));
        return std::clamp(entry, 1, shape.endgame);
    }

    ItemPlan PlanItem(int quality, int requiredLevel, int itemLevel)
    {
        ItemPlan plan;
        plan.requiredLevel = requiredLevel;
        plan.itemLevel = itemLevel;
        if (requiredLevel <= 1 || itemLevel <= 0)
            return plan;

        // Which game an item belongs to, by its levels. Outland's items are worth much more than the old world's
        // at the same required level, Northrend's more again; Northrend's leveling gear starts at 68 with item
        // levels Outland's top rewards never had at blue or green.
        if (requiredLevel > 70 || (requiredLevel >= 68 && itemLevel >= 125 && quality <= 3))
            plan.era = ITEM_WOTLK;
        else if (requiredLevel > 60 || (requiredLevel >= 58 && (itemLevel >= 93 || (itemLevel >= 80 && quality <= 3))))
            plan.era = ITEM_TBC;
        else if (requiredLevel == 60 && itemLevel >= 65)
            return plan;            // the raids of the old world - and their rewards - stay at 60
        else
            plan.era = ITEM_CLASSIC;

        if ((plan.era == ITEM_TBC && !shape.outland) || (plan.era == ITEM_WOTLK && !shape.northrend))
        {
            plan.era = ITEM_KEEP;   // a part left as it came keeps its items as they came
            return plan;
        }

        plan.endgame = quality >= 4 && ((plan.era == ITEM_TBC && requiredLevel >= 70) || (plan.era == ITEM_WOTLK && requiredLevel >= 80));
        if (plan.endgame)
        {
            // The end of the journey is a ladder: the raids of the old world (item levels 66 to 92), then
            // Outland's, then Northrend's - as they followed each other once.
            plan.requiredLevel = shape.endgame;
            Range const& range = plan.era == ITEM_TBC ? shape.outlandItems : shape.northrendItems;
            double const level = plan.era == ITEM_TBC
                ? range.from + (itemLevel - 110) * double(range.to - range.from) / 54.0
                : range.from + (itemLevel - 200) * double(range.to - range.from) / 84.0;
            double const low = range.from - 10.0, high = range.to + 20.0;
            plan.itemLevel = std::min(itemLevel, int(std::lround(std::clamp(level, low, high))));
            return plan;
        }

        // Outland and Northrend ask little of their gear - most of Northrend's rewards ask for 68, whatever the zone
        // they come from - so the level an item was made for is read from its item level there.
        double usual;
        int madeFor;
        switch (plan.era)
        {
            case ITEM_TBC:
                madeFor = std::clamp(int(std::lround(58.0 + (itemLevel - 90) * 12.0 / 25.0)), requiredLevel, 70);
                plan.requiredLevel = Outland(madeFor);
                usual = UsualTbc(madeFor);
                break;
            case ITEM_WOTLK:
                madeFor = std::clamp(int(std::lround(68.0 + (itemLevel - 130) * 12.0 / 57.0)), requiredLevel, 80);
                plan.requiredLevel = Northrend(madeFor);
                usual = UsualWotlk(madeFor);
                break;
            default:
                plan.requiredLevel = Classic(requiredLevel);
                usual = UsualClassic(requiredLevel);
                break;
        }
        plan.requiredLevel = std::min(plan.requiredLevel, shape.endgame);
        plan.itemLevel = std::clamp(int(std::lround(itemLevel * UsualClassic(plan.requiredLevel) / usual)), 1, itemLevel);
        return plan;
    }

    int WindowOffset(int originalLevel, int zoneOriginalEntry, int part, bool eliteOrRare, int below, int above)
    {
        // A zone of the old world spans about ten levels from its entry, one of Outland or Northrend about six.
        double const span = part == 0 ? 10.0 : 6.0;
        double const place = std::clamp((originalLevel - zoneOriginalEntry) / span, 0.0, 1.0);
        int offset = int(std::lround(-below + place * (below + above)));
        if (eliteOrRare)
            ++offset;
        return std::clamp(offset, -below, above);
    }

    bool IsAmountEffect(uint32_t effect, uint32_t aura)
    {
        switch (effect)
        {
            case 2:     // school damage
            case 9:     // health leech
            case 10:    // heal
            case 30:    // energize
            case 75:    // heal mechanical
                return true;
            case 6:     // apply aura
            case 27:    // persistent area aura
            case 35:    // area aura: party
            case 65:    // area aura: raid
            case 128:   // area aura: friend
                switch (aura)
                {
                    case 3:     // periodic damage
                    case 8:     // periodic heal
                    case 13:    // damage done
                    case 15:    // damage shield
                    case 22:    // resistance
                    case 24:    // periodic energize
                    case 29:    // stat
                    case 34:    // increase health
                    case 35:    // increase energy
                    case 53:    // periodic leech
                    case 69:    // school absorb
                    case 83:    // base resistance
                    case 85:    // power regen
                    case 97:    // mana shield
                    case 99:    // attack power
                    case 123:   // target resistance
                    case 124:   // ranged attack power
                    case 135:   // healing done
                    case 189:   // rating
                        return true;
                    default:
                        return false;
                }
            default:
                return false;
        }
    }

    bool IsTriggerEffect(uint32_t effect, uint32_t aura)
    {
        if (effect == 64 || effect == 32)  // trigger spell, trigger missile
            return true;
        bool const isAura = effect == 6 || effect == 27 || effect == 35 || effect == 65 || effect == 128;
        return isAura && (aura == 23 || aura == 42 || aura == 231);     // periodic trigger, proc trigger (with value)
    }

    bool IsEnchantEffect(uint32_t effect)
    {
        return effect == 53 || effect == 54 || effect == 92;    // enchant item, temporary, held item
    }
}
