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

        int Clamp(double level)
        {
            return int(std::clamp(std::lround(level), 1L, 63L));
        }

        /// The usual item level of a leveling item at a required level, in each game. The games grew their item
        /// levels at different speeds: a level-58 item of Outland is worth an item level of about 90.
        double UsualClassic(int level) { return level + 5.0; }
        double UsualTbc(int level) { return std::max(UsualClassic(level), 90.0 + (level - 58) * 25.0 / 12.0); }
        double UsualWotlk(int level) { return std::max(UsualTbc(level), 130.0 + (level - 68) * 57.0 / 12.0); }
    }

    int Classic(int level)
    {
        return Clamp(1.0 + (level - 1) * 34.0 / 54.0);
    }

    int Outland(int level)
    {
        return level < 58 ? Classic(level) : Clamp(30.0 + (level - 58) * 13.0 / 9.0);
    }

    int Northrend(int level)
    {
        return level < 68 ? Classic(level) : std::min(Clamp(40.0 + (level - 68) * 15.0 / 9.0), 60);
    }

    int Top(int level, int base)
    {
        return std::clamp(60 + level - base, 60, 63);
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
            case ERA_TOP_OUTLAND:   return level >= 61 ? Top(level, 70) : level;
            case ERA_TOP_NORTHREND: return level >= 61 ? Top(level, 80) : level;
            default:                return level;
        }
    }

    bool IsInstanceOfOutland(uint32_t map) { return OutlandInstances.count(map) || OutlandRaids.count(map); }
    bool IsInstanceOfNorthrend(uint32_t map) { return NorthrendInstances.count(map) || NorthrendRaids.count(map); }

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

    uint32_t ScaleMoney(uint32_t money, int from, int to)
    {
        if (!money || from <= 0 || to >= from)
            return money;
        double const share = double(to) / double(from);
        return uint32_t(std::max(1.0, std::round(money * share * share)));
    }

    ItemPlan PlanItem(int quality, int requiredLevel, int itemLevel)
    {
        ItemPlan plan;
        plan.requiredLevel = requiredLevel;
        plan.itemLevel = itemLevel;
        if (requiredLevel <= 1 || itemLevel <= 0)
            return plan;

        // Which game an item belongs to, by its levels. Outland's items are worth much more than the old world's
        // at the same required level, Northrend's more again.
        // Northrend's leveling gear starts at 68 with item levels Outland's top rewards never had at blue or green.
        if (requiredLevel > 70 || (requiredLevel >= 68 && itemLevel >= 125 && quality <= 3))
            plan.era = ITEM_WOTLK;
        else if (requiredLevel > 60 || (requiredLevel >= 58 && (itemLevel >= 93 || (itemLevel >= 80 && quality <= 3))))
            plan.era = ITEM_TBC;
        else if (requiredLevel == 60 && itemLevel >= 65)
            return plan;            // the raids of the old world - and their rewards - stay at 60
        else
            plan.era = ITEM_CLASSIC;

        plan.endgame = quality >= 4 && ((plan.era == ITEM_TBC && requiredLevel >= 70) || (plan.era == ITEM_WOTLK && requiredLevel >= 80));
        if (plan.endgame)
        {
            // The end of the journey is a ladder at 60: the raids of the old world (item levels 66 to 92), then
            // Outland's (to 110), then Northrend's (to 140) - as they followed each other once.
            plan.requiredLevel = 60;
            double const level = plan.era == ITEM_TBC
                ? std::clamp(75.0 + (itemLevel - 110) * 35.0 / 54.0, 66.0, 120.0)
                : std::clamp(95.0 + (itemLevel - 200) * 45.0 / 84.0, 85.0, 160.0);
            plan.itemLevel = std::min(itemLevel, int(std::lround(level)));
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
        plan.requiredLevel = std::min(plan.requiredLevel, 60);
        plan.itemLevel = std::clamp(int(std::lround(itemLevel * UsualClassic(plan.requiredLevel) / usual)), 1, itemLevel);
        return plan;
    }
}
