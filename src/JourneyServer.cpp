/*
 * World Journey in the server: the regulators of mod_world_journey.conf, the world database at startup (JourneyDb),
 * what has to be done in memory - spells and enchantments, instances, the Dungeon Finder, the battlegrounds - and
 * what happens while the realm runs: creatures of zones set by hand, the window of lifted creatures, the spells of
 * creatures that now stand lower, quest experience, and what the client's addon is told at login.
 * Released under GNU AGPL v3: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "JourneyData.h"
#include "JourneyDb.h"

#include "Chat.h"
#include "Config.h"
#include "Creature.h"
#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "LFGMgr.h"
#include "LocalLevelScaling.h"
#include "Log.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "QuestDef.h"
#include "ScriptMgr.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"

#if __has_include("AscensionClientSpellPatches.h")
#include "AscensionClientSpellPatches.h"
#define WORLD_JOURNEY_CLIENT_SPELL_PATCHES 1
#endif

#include <algorithm>
#include <cmath>
#include <deque>
#include <map>
#include <sstream>
#include <unordered_map>

namespace
{
    char const* const TablePrefix = "journey";
    char const* const AddonPrefix = "ZoneLevels";

    /// What the regulators decide beyond the database.
    struct Runtime
    {
        bool enabled = true;
        bool debug = false;
        bool guard = true;
        bool forced = true;
        bool window = true;
        int below = 2;
        int above = 2;
        bool map = true;
        bool tooltips = true;
        bool battlegrounds = true;
        std::array<double, 4> questXp{ 1.0, 1.0, 1.0, 1.0 };
    };

    Runtime runtime;
    journey::Settings settings;
    journey::Outcome outcome;

    std::unordered_map<uint32, std::pair<uint8, uint8>> zoneData;   // zone -> original entry, part
    std::unordered_map<uint32, int> zoneShift;                      // zone set by hand -> levels moved
    std::unordered_map<uint32, int> mapShift;                       // instance set by hand -> levels moved
    std::vector<std::pair<std::string, std::string>> enchantTexts;  // what a gem or enchantment says, before and after

    void Say(std::string const& line)
    {
        LOG_INFO("server.loading", ">> World Journey: {}", line);
    }

    void Warn(std::string const& line)
    {
        LOG_WARN("server.loading", ">> World Journey: {}", line);
    }

    // ------------------------------------------------------------------------------------------ the regulators

    journey::Range ReadRange(std::string const& name, journey::Range fallback)
    {
        std::string const text = sConfigMgr->GetOption<std::string>(name, "", false);
        int from = 0, to = 0;
        char dash = 0;
        std::istringstream in(text);
        if (text.empty() || !(in >> from >> dash >> to) || dash != '-' || from < 1 || to < from || to > 255)
        {
            if (!text.empty())
                Warn(name + " = " + text + " is not a range like 30-43; " + std::to_string(fallback.from) + "-" + std::to_string(fallback.to) + " is used");
            return fallback;
        }
        return { from, to };
    }

    std::map<uint32, int> ReadEntries(std::string const& prefix)
    {
        std::map<uint32, int> entries;
        for (std::string const& key : sConfigMgr->GetKeysByString(prefix))
        {
            uint32 const id = uint32(std::strtoul(key.c_str() + prefix.size(), nullptr, 10));
            int const level = sConfigMgr->GetOption<int32>(key, 0);
            if (id && level > 0)
                entries[id] = level;
        }
        return entries;
    }

    void ReadRegulators()
    {
        runtime = Runtime();
        runtime.enabled = sConfigMgr->GetOption<bool>("Journey.Enable", true);
        runtime.debug = sConfigMgr->GetOption<bool>("Journey.Debug", false);
        runtime.guard = sConfigMgr->GetOption<bool>("Journey.Guard.Enable", true);
        runtime.forced = sConfigMgr->GetOption<bool>("Journey.Scaling.Forced", true);
        runtime.window = sConfigMgr->GetOption<bool>("Journey.Window.Enable", true);
        runtime.below = std::clamp(sConfigMgr->GetOption<int32>("Journey.Window.Below", 2), 0, 10);
        runtime.above = std::clamp(sConfigMgr->GetOption<int32>("Journey.Window.Above", 2), 0, 10);
        runtime.map = sConfigMgr->GetOption<bool>("Journey.Map.Enable", true);
        runtime.tooltips = sConfigMgr->GetOption<bool>("Journey.Tooltips.Enable", true);
        runtime.battlegrounds = sConfigMgr->GetOption<bool>("Journey.Battlegrounds.Enable", true);

        static char const* const Parts[] = { "OldWorld", "Outland", "Northrend", "Endgame" };
        float const xp = sConfigMgr->GetOption<float>("Journey.Quests.XpRate", 1.0f);
        for (int part = 0; part < 4; ++part)
        {
            float const own = sConfigMgr->GetOption<float>(std::string("Journey.Quests.XpRate.") + Parts[part], -1.0f, false);
            runtime.questXp[part] = std::clamp(double(own >= 0.0f ? own : xp), 0.0, 10.0);
        }

        journey::Shape shape;
        shape.classic = ReadRange("Journey.Classic.Range", shape.classic);
        shape.outlandRange = ReadRange("Journey.Outland.Range", shape.outlandRange);
        shape.northrendRange = ReadRange("Journey.Northrend.Range", shape.northrendRange);
        shape.endgame = sConfigMgr->GetOption<int32>("Journey.Endgame.Level", 60);
        shape.bossLevels = sConfigMgr->GetOption<int32>("Journey.Endgame.BossLevels", 3);
        shape.outlandItems = ReadRange("Journey.Endgame.Outland.ItemLevels", shape.outlandItems);
        shape.northrendItems = ReadRange("Journey.Endgame.Northrend.ItemLevels", shape.northrendItems);
        shape.zoneEntries = ReadEntries("Journey.Zone.");
        shape.instanceEntries = ReadEntries("Journey.Instance.");
        journey::UseShape(shape);

        settings = journey::Settings();
        settings.enabled = runtime.enabled;
        settings.prefix = TablePrefix;
        settings.items = sConfigMgr->GetOption<bool>("Journey.Items.Enable", true);
        settings.consumables = sConfigMgr->GetOption<bool>("Journey.Consumables.Enable", true);
        settings.enchantments = sConfigMgr->GetOption<bool>("Journey.Enchantments.Enable", true);
        settings.trainers = sConfigMgr->GetOption<bool>("Journey.Trainers.Enable", true);
        settings.battlegrounds = runtime.battlegrounds;
        settings.tooltips = runtime.tooltips;
        settings.journeyBaseStats = sConfigMgr->GetOption<std::string>("Journey.Creatures.BaseStats", "journey") != "original";

        static char const* const Ranks[] = { "Normal", "Elite", "RareElite", "WorldBoss", "Rare" };
        static char const* const Stats[] = { "Health", "Damage", "SpellDamage", "Armor" };
        for (int rank = 0; rank < journey::Difficulty::RANKS; ++rank)
            for (int stat = 0; stat < journey::Difficulty::STATS; ++stat)
            {
                std::string const name = std::string(Ranks[rank]) + "." + Stats[stat];
                double& general = settings.difficulty.general[rank][stat];
                general = std::clamp(double(sConfigMgr->GetOption<float>("Journey.Difficulty." + name, float(general), false)), 0.05, 20.0);
                for (int part = 0; part < journey::Difficulty::PARTS; ++part)
                {
                    float const own = sConfigMgr->GetOption<float>("Journey.Difficulty." + std::string(Parts[part]) + "." + name, -1.0f, false);
                    settings.difficulty.byPart[part][rank][stat] = own >= 0.0f ? std::clamp(double(own), 0.05, 20.0) : -1.0;
                }
            }
    }

    /// The journey ends at 60: the realm and the Dungeon Finder have to agree, whatever the config files say.
    void Guard()
    {
        if (!runtime.enabled || !runtime.guard)
            return;
        if (sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL) > 60)
        {
            Warn("MaxPlayerLevel = " + std::to_string(sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL)) +
                " in worldserver.conf: the journey ends at 60, the realm runs with 60 (set it there to silence this)");
            sWorld->setIntConfig(CONFIG_MAX_PLAYER_LEVEL, 60);
        }
        if (sWorld->getIntConfig(CONFIG_LFG_MAX_EXPANSION) < 2)
        {
            Warn("DungeonFinder.MaxExpansion = " + std::to_string(sWorld->getIntConfig(CONFIG_LFG_MAX_EXPANSION)) +
                ": the dungeons of Outland and Northrend are part of the journey, the Dungeon Finder runs with 2");
            sWorld->setIntConfig(CONFIG_LFG_MAX_EXPANSION, 2);
        }
        if (runtime.battlegrounds && sWorld->getIntConfig(CONFIG_WINTERGRASP_PLR_MIN_LVL) > 60)
            sWorld->setIntConfig(CONFIG_WINTERGRASP_PLR_MIN_LVL, 60);
#ifdef LOCAL_LEVEL_SCALING_WORLD_JOURNEY
        LocalLevelScaling::ScalingChoiceForced.store(runtime.forced, std::memory_order_relaxed);
#else
        if (runtime.forced || runtime.window)
            Warn("the core is built without World Journey's patch (patches/core-level-window.patch): open world scaling "
                "stays each character's choice, and lifted creatures stand where CoA puts them");
#endif
    }

    // ------------------------------------------------------------------------------------------ the database

    journey::Db WorldDb()
    {
        journey::Db db;
        db.query = [](std::string const& sql, std::function<void(journey::Row const&)> const& onRow)
        {
            QueryResult result = WorldDatabase.Query(std::string_view(sql));
            if (!result)
                return;
            uint32 const count = result->GetFieldCount();
            journey::Row row(count);
            do
            {
                Field* fields = result->Fetch();
                for (uint32 i = 0; i < count; ++i)
                    row[i] = fields[i].IsNull() ? std::string() : fields[i].Get<std::string>();
                onRow(row);
            } while (result->NextRow());
        };
        db.execute = [](std::string const& sql) { WorldDatabase.DirectExecute(std::string_view(sql)); };
        db.log = [](std::string const& line) { Say(line); };
        return db;
    }

    std::string DataPath()
    {
        std::string path = sConfigMgr->GetOption<std::string>("DataDir", "./");
        if (path.empty() || (path.back() != '/' && path.back() != '\\'))
            path += '/';
        return path;
    }

    void RunDatabase()
    {
        uint32 const started = getMSTime();
        journey::PropertyPoints points;
        journey::DbcFile spells;
        if (runtime.enabled)
        {
            if (!journey::ReadPropertyPoints(DataPath() + "dbc/RandPropPoints.dbc", points))
                Warn(DataPath() + "dbc/RandPropPoints.dbc not readable - the stats of items follow their item level instead");
            if (settings.tooltips && !spells.Read(DataPath() + "dbc/Spell.dbc"))
                Warn(DataPath() + "dbc/Spell.dbc not readable - the spells of items are scaled in memory, their tooltips keep the old numbers");
        }
        outcome = journey::Outcome();
        journey::Run(WorldDb(), settings, points, spells, outcome);
        Say("the world database " + std::string(runtime.enabled ? "is on the journey" : "is as it came") + " (" +
            std::to_string(GetMSTimeDiffToNow(started)) + " ms)");

        zoneData.clear();
        zoneShift.clear();
        mapShift.clear();
        if (!runtime.enabled)
            return;
        for (journey::ZoneData const& zone : journey::Zones)
        {
            zoneData[zone.zone] = { zone.originalEntry, zone.part };
            if (int const delta = journey::ZoneEntry(zone.zone, zone.part, zone.originalEntry, true) -
                journey::ZoneEntry(zone.zone, zone.part, zone.originalEntry, false))
                zoneShift[zone.zone] = delta;
        }
        for (journey::BotZoneData const& zone : journey::BotZones)
            zoneData.try_emplace(zone.zone, zone.originalMin, zone.part);
        for (journey::InstanceData const& instance : journey::Instances)
            if (instance.difficulty == 0)
                if (int const delta = journey::InstanceEntry(instance.map, journey::InstanceKind(instance.kind), instance.originalEntry, instance.contentLevel, true) -
                    journey::InstanceEntry(instance.map, journey::InstanceKind(instance.kind), instance.originalEntry, instance.contentLevel, false))
                    mapShift[instance.map] = delta;
    }

    // ------------------------------------------------------------------------------------------ spells and enchantments

    bool IsAmount(SpellEffectInfo const& effect) { return journey::IsAmountEffect(effect.Effect, effect.ApplyAuraName); }
    bool IsTrigger(SpellEffectInfo const& effect) { return journey::IsTriggerEffect(effect.Effect, effect.ApplyAuraName); }
    bool IsEnchant(SpellEffectInfo const& effect) { return journey::IsEnchantEffect(effect.Effect); }

    /// Works out what every spell and enchantment touched by the journey in memory is worth, following triggered
    /// spells and the spells of enchantments; then changes them, once each. Shared by things of different levels,
    /// the gentler factor wins. Spells already scaled in spell_dbc are left alone.
    struct Rescale
    {
        std::unordered_map<uint32, double> spells, enchants;
        std::deque<std::pair<bool, uint32>> pending;   // (enchant?, id)

        void Spell(uint32 id, double factor) { Note(spells, false, id, factor); }
        void Enchant(uint32 id, double factor) { Note(enchants, true, id, factor); }

        void Note(std::unordered_map<uint32, double>& known, bool enchant, uint32 id, double factor)
        {
            if (!id)
                return;
            auto found = known.find(id);
            if (found != known.end() && found->second >= factor)
                return;
            known[id] = factor;
            pending.emplace_back(enchant, id);
        }

        void Follow()
        {
            while (!pending.empty())
            {
                auto [enchant, id] = pending.front();
                pending.pop_front();
                if (enchant)
                {
                    double const factor = enchants[id];
                    if (SpellItemEnchantmentEntry const* entry = sSpellItemEnchantmentStore.LookupEntry(id))
                        for (uint32 i = 0; i < MAX_SPELL_ITEM_ENCHANTMENT_EFFECTS; ++i)
                            if (entry->type[i] == ITEM_ENCHANTMENT_TYPE_COMBAT_SPELL || entry->type[i] == ITEM_ENCHANTMENT_TYPE_EQUIP_SPELL ||
                                entry->type[i] == ITEM_ENCHANTMENT_TYPE_USE_SPELL)
                                Spell(entry->spellid[i], factor);
                    continue;
                }
                double const factor = spells[id];
                if (SpellInfo const* info = sSpellMgr->GetSpellInfo(id))
                    for (SpellEffectInfo const& effect : info->GetEffects())
                    {
                        if (IsTrigger(effect))
                            Spell(effect.TriggerSpell, factor);
                        else if (IsEnchant(effect) && effect.MiscValue > 0)
                            Enchant(uint32(effect.MiscValue), factor);
                    }
            }
        }

        static int32 Scaled(int32 value, double factor) { return int32(std::lround(value * factor)); }

        static uint32 LevelOf(uint32 level)
        {
            return level > 1 ? uint32(journey::Compress(int(level), journey::ERA_CLASSIC)) : level;
        }

        uint32 ApplySpells()
        {
            uint32 changed = 0;
            for (auto const& [id, factor] : spells)
            {
                if (factor >= 0.999 || outcome.tooltipSpells.count(id))
                    continue;
                SpellInfo* info = const_cast<SpellInfo*>(sSpellMgr->GetSpellInfo(id));
                if (!info)
                    continue;
                bool any = false;
                for (SpellEffectInfo& effect : info->Effects)
                {
                    if (!IsAmount(effect))
                        continue;
                    // The amount of an effect is BasePoints + 1 .. BasePoints + DieSides.
                    int32 const low = effect.BasePoints + (effect.DieSides ? 1 : 0);
                    int32 const newLow = Scaled(low, factor);
                    effect.DieSides = effect.DieSides > 1 ? std::max(1, Scaled(effect.DieSides, factor)) : effect.DieSides;
                    effect.BasePoints = newLow - (effect.DieSides ? 1 : 0);
                    effect.RealPointsPerLevel = float(effect.RealPointsPerLevel * factor);
                    any = true;
                }
                // A buff of level 70 does not take on a character of 47: the levels of the spell follow the journey.
                info->SpellLevel = LevelOf(info->SpellLevel);
                info->BaseLevel = LevelOf(info->BaseLevel);
                if (info->MaxLevel > 60)
                    info->MaxLevel = LevelOf(info->MaxLevel);
                changed += any;
                if (runtime.debug)
                    LOG_INFO("module", "World Journey: spell {} at {:.0f}% (in memory)", id, factor * 100.0);
            }
            return changed;
        }

        /// The text of an enchantment with its numbers changed: what the addon puts into a tooltip.
        static std::string Retext(std::string text, std::vector<std::pair<uint32, uint32>> const& changes)
        {
            size_t from = 0;
            for (auto const& [before, after] : changes)
            {
                std::string const old = std::to_string(before);
                size_t at = text.find(old, from);
                while (at != std::string::npos && ((at > 0 && std::isdigit(uint8(text[at - 1]))) ||
                    (at + old.size() < text.size() && std::isdigit(uint8(text[at + old.size()])))))
                    at = text.find(old, at + 1);
                if (at == std::string::npos)
                    continue;
                std::string const now = std::to_string(after);
                text.replace(at, old.size(), now);
                from = at + now.size();
            }
            return text;
        }

        uint32 ApplyEnchants()
        {
            uint32 changed = 0;
            uint32 const locale = sWorld->GetDefaultDbcLocale();
            for (auto const& [id, factor] : enchants)
            {
                if (factor >= 0.999)
                    continue;
                auto* entry = const_cast<SpellItemEnchantmentEntry*>(sSpellItemEnchantmentStore.LookupEntry(id));
                if (!entry)
                    continue;
                std::vector<std::pair<uint32, uint32>> changes;
                for (uint32 i = 0; i < MAX_SPELL_ITEM_ENCHANTMENT_EFFECTS; ++i)
                    if (entry->amount[i] && (entry->type[i] == ITEM_ENCHANTMENT_TYPE_DAMAGE ||
                        entry->type[i] == ITEM_ENCHANTMENT_TYPE_RESISTANCE || entry->type[i] == ITEM_ENCHANTMENT_TYPE_STAT))
                    {
                        uint32 const before = entry->amount[i];
                        entry->amount[i] = std::max<uint32>(1, uint32(std::lround(before * factor)));
                        changes.emplace_back(before, entry->amount[i]);
                    }
                if (changes.empty())
                    continue;
                ++changed;
                char const* description = locale < 16 ? entry->description[locale] : nullptr;
                if (!description || !*description)
                    description = entry->description[0];
                if (description && *description)
                {
                    std::string const before = description, after = Retext(before, changes);
                    if (after != before && before.size() < 100 && after.size() < 100)
                        enchantTexts.emplace_back(before, after);
                }
            }
            return changed;
        }
    };

    void RescaleSpellsAndEnchants()
    {
        Rescale rescale;
        // Spells in spell_dbc are scaled already, and their triggers with them; what is left is what only memory holds.
        if (outcome.tooltipSpells.empty())
            for (auto const& [id, factor] : outcome.spellFactors)
                rescale.Spell(id, factor);
        for (auto const& [id, factor] : outcome.enchantFactors)
            rescale.Enchant(id, factor);
        if (settings.enchantments)
        {
            // Gems and the enchantments of the professions of Outland and Northrend: by what the top rewards of their
            // game keep of their stats.
            for (GemPropertiesEntry const* gem : sGemPropertiesStore)
            {
                auto era = outcome.gemEras.find(gem->ID);
                if (era != outcome.gemEras.end())
                    rescale.Enchant(gem->spellitemenchantement, outcome.endgameRatio[era->second]);
            }
            for (auto const& [spell, era] : outcome.craftEras)
                if (SpellInfo const* info = sSpellMgr->GetSpellInfo(spell))
                    for (SpellEffectInfo const& effect : info->GetEffects())
                        if (IsEnchant(effect) && effect.MiscValue > 0)
                            rescale.Enchant(uint32(effect.MiscValue), outcome.endgameRatio[era]);
        }
        rescale.Follow();
        enchantTexts.clear();
        uint32 const spells = rescale.ApplySpells();
        uint32 const enchants = rescale.ApplyEnchants();

        // An enchantment that asks for a level above the journey asks for 60.
        uint32 lowered = 0;
        for (SpellItemEnchantmentEntry const* entry : sSpellItemEnchantmentStore)
            if (entry->requiredLevel > 60)
            {
                const_cast<SpellItemEnchantmentEntry*>(entry)->requiredLevel = 60;
                ++lowered;
            }

        // The spells scaled in spell_dbc go to the client with CoA's own spell patches: their tooltips are right.
        uint32 registered = 0;
#ifdef WORLD_JOURNEY_CLIENT_SPELL_PATCHES
        for (uint32 id : outcome.tooltipSpells)
        {
            Ascension::ClientSpellPatches::Instance().Register(id);
            ++registered;
        }
#endif
        Say(std::to_string(outcome.tooltipSpells.size()) + " spell(s) of items scaled in spell_dbc (" + std::to_string(registered) +
            " sent to the client), " + std::to_string(spells) + " more in memory; " + std::to_string(enchants) +
            " enchantment(s) and gem(s) scaled, " + std::to_string(lowered) + " open at 60");
    }

    // ------------------------------------------------------------------------------------------ instances, Dungeon Finder, battlegrounds

    int EntryOf(journey::InstanceData const& instance)
    {
        return journey::InstanceEntry(instance.map, journey::InstanceKind(instance.kind), instance.originalEntry, instance.contentLevel);
    }

    void SetInstanceEntries()
    {
        uint32 changed = 0;
        for (journey::InstanceData const& instance : journey::Instances)
            if (DungeonProgressionRequirements const* access = sObjectMgr->GetAccessRequirement(instance.map, Difficulty(instance.difficulty)))
            {
                auto* writable = const_cast<DungeonProgressionRequirements*>(access);
                uint8 const entry = uint8(EntryOf(instance));
                if (writable->levelMin != entry)
                {
                    writable->levelMin = entry;
                    ++changed;
                }
            }
        Say(std::to_string(changed) + " instance(s) can be entered at their new levels");
    }

    /// The Dungeon Finder offers every dungeon from its new entry level to 60; a random dungeon from the lowest
    /// entry among the dungeons it draws from.
    void SetDungeonFinder()
    {
        std::map<std::pair<uint32, uint32>, uint8> entries;
        for (journey::InstanceData const& instance : journey::Instances)
            entries[{ instance.map, instance.difficulty }] = uint8(EntryOf(instance));
        uint8 const top = uint8(std::min<uint32>(sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL), 255));
        std::map<std::pair<uint8, uint8>, uint8> lowestOfGroup;   // (group, difficulty) -> lowest entry
        std::vector<lfg::LFGDungeonData*> randoms;
        uint32 changed = 0;
        for (LFGDungeonEntry const* dbc : sLFGDungeonStore)
        {
            auto* dungeon = const_cast<lfg::LFGDungeonData*>(sLFGMgr->GetLFGDungeon(dbc->ID));
            if (!dungeon)
                continue;
            if (dungeon->type == lfg::LFG_TYPE_RANDOM)
            {
                randoms.push_back(dungeon);
                continue;
            }
            auto entry = entries.find({ dungeon->map, uint32(dungeon->difficulty) });
            if (entry == entries.end())
                continue;
            dungeon->minlevel = entry->second;
            dungeon->maxlevel = std::max<uint8>(dungeon->minlevel, top);
            auto& lowest = lowestOfGroup.try_emplace({ dungeon->group, uint8(dungeon->difficulty) }, dungeon->minlevel).first->second;
            lowest = std::min(lowest, dungeon->minlevel);
            ++changed;
        }
        for (lfg::LFGDungeonData* random : randoms)
        {
            auto lowest = lowestOfGroup.find({ random->group, uint8(random->difficulty) });
            if (lowest == lowestOfGroup.end())
                continue;
            random->minlevel = lowest->second;
            random->maxlevel = std::max<uint8>(random->minlevel, top);
            ++changed;
        }
        Say(std::to_string(changed) + " entr(ies) of the Dungeon Finder on the journey");
    }

    /// The battlegrounds that open above 60 - the Eye of the Storm, the Strand of the Ancients, the Isle of Conquest -
    /// take a character of 60 into their first bracket.
    void SetBattlegroundBrackets()
    {
        uint32 changed = 0;
        for (uint32 map : { 30u, 489u, 529u, 566u, 607u, 628u })
        {
            PvPDifficultyEntry* first = nullptr;
            for (uint32 id = 0; id < MAX_BATTLEGROUND_BRACKETS; ++id)
                if (PvPDifficultyEntry const* bracket = GetBattlegroundBracketById(map, BattlegroundBracketId(id)))
                    if (!first || bracket->minLevel < first->minLevel)
                        first = const_cast<PvPDifficultyEntry*>(bracket);
            if (first && first->minLevel > 60)
            {
                first->minLevel = 60;
                ++changed;
            }
        }
        Say(std::to_string(changed) + " battleground bracket(s) open at 60");
    }

    // ------------------------------------------------------------------------------------------ while the realm runs

    /// The level a creature stood at before the journey: the same place in its original range.
    int OriginalLevel(Creature const* creature)
    {
        CreatureTemplate const* info = creature->GetCreatureTemplate();
        int const now = creature->GetLevel();
        auto shift = info ? outcome.shifts.find(info->Entry) : outcome.shifts.end();
        if (shift == outcome.shifts.end())
            return now;
        journey::Shift const& s = shift->second;
        double const place = s.newMax > s.newMin ? double(std::clamp<int>(now, s.newMin, s.newMax) - s.newMin) / double(s.newMax - s.newMin) : 0.0;
        return int(std::lround(s.origMin + place * (s.origMax - s.origMin)));
    }

    /// Where a lifted creature stands for a character: its place in its zone, inside the window around the character.
    uint8 Window(Player const* viewer, Creature const* creature, uint8 own)
    {
        if (!runtime.enabled || !runtime.window || !viewer || !creature)
            return 0;
        auto zone = zoneData.find(creature->GetZoneId());
        if (zone == zoneData.end())
            return 0;
        CreatureTemplate const* info = creature->GetCreatureTemplate();
        uint32 const rank = info ? info->rank : 0;
        if (rank == CREATURE_ELITE_WORLDBOSS)
            return 0;
        bool const eliteOrRare = rank == CREATURE_ELITE_ELITE || rank == CREATURE_ELITE_RARE || rank == CREATURE_ELITE_RAREELITE;
        int const offset = journey::WindowOffset(OriginalLevel(creature), zone->second.first, zone->second.second, eliteOrRare,
            runtime.below, runtime.above);
        int const placed = std::clamp<int>(int(viewer->GetLevel()) + offset, 1, 255);
        return uint8(std::max<int>(own, placed));
    }

    /// What a blow is worth at a level: the weapon damage and attack power of that level's base stats.
    double HitAt(uint8 level, uint8 unitClass, uint8 expansion)
    {
        CreatureBaseStats const* stats = sObjectMgr->GetCreatureBaseStats(level, unitClass);
        if (!stats)
            return 0.0;
        return stats->BaseDamage[std::min<uint8>(expansion, MAX_EXPANSIONS - 1)] + stats->AttackPower / 14.0 * 2.0;
    }

    /// How hard a creature's spells hit now: the numbers in its spells are those of its original level, scaled down
    /// with the level, then by the difficulty of its spells.
    double SpellFactor(Unit* attacker)
    {
        Creature* creature = attacker ? attacker->ToCreature() : nullptr;
        if (!creature && attacker)
            if (Unit* owner = attacker->GetCharmerOrOwner())
                creature = owner->ToCreature();      // a totem or a summon of a creature
        if (!creature)
            return 1.0;
        CreatureTemplate const* info = creature->GetCreatureTemplate();
        if (!info)
            return 1.0;
        double factor = 1.0;
        auto multiplier = outcome.spellMultipliers.find(info->Entry);
        if (multiplier != outcome.spellMultipliers.end())
            factor = multiplier->second;
        auto shift = outcome.shifts.find(info->Entry);
        if (shift == outcome.shifts.end())
            return factor;
        journey::Shift const& s = shift->second;
        uint8 const now = creature->GetLevel();
        uint8 const was = uint8(std::clamp(OriginalLevel(creature), 1, 255));
        double const before = HitAt(was, info->unit_class, s.origExpansion);
        double const after = HitAt(std::clamp<uint8>(now, 1, 255), info->unit_class, s.newExpansion);
        if (before > 0.0 && after > 0.0 && after < before)
            factor *= after / before;
        return factor;
    }

    // ------------------------------------------------------------------------------------------ the addon

    void SendAddon(Player* player, std::string const& body)
    {
        WorldPacket data;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, std::string(AddonPrefix) + "\t" + body);
        player->SendDirectMessage(&data);
    }

    /// At login the addon learns the zone levels of the journey and what gems and enchantments really give.
    void TellAddon(Player* player)
    {
        if (!runtime.enabled || !player || !player->GetSession())
            return;
        SendAddon(player, "B");
        if (runtime.map)
        {
            std::string chunk;
            for (journey::ZoneData const& zone : journey::Zones)
            {
                int const entry = journey::ZoneEntry(zone.zone, zone.part, zone.originalEntry);
                std::string const part = std::string(zone.key) + "=" + std::to_string(entry) + "-" + std::to_string(std::max(entry, 60)) + ";";
                if (chunk.size() + part.size() > 230)
                {
                    SendAddon(player, "Z" + chunk);
                    chunk.clear();
                }
                chunk += part;
            }
            if (!chunk.empty())
                SendAddon(player, "Z" + chunk);
        }
        if (runtime.tooltips)
            for (auto const& [before, after] : enchantTexts)
                if (before.find_first_of("~^\t") == std::string::npos && after.find_first_of("~^\t") == std::string::npos)
                    SendAddon(player, "E" + before + "~" + after);
        SendAddon(player, "D");
    }
}

class JourneyWorld : public WorldScript
{
public:
    JourneyWorld() : WorldScript("JourneyWorld") { }

    void OnAfterConfigLoad(bool reload) override
    {
        ReadRegulators();
        Guard();
        if (reload)
            return;     // the database is set once, before the world reads it
        RunDatabase();
    }

    void OnBeforeWorldInitialized() override
    {
        if (!runtime.enabled)
            return;
        RescaleSpellsAndEnchants();
        SetInstanceEntries();
        SetDungeonFinder();
        if (runtime.battlegrounds)
            SetBattlegroundBrackets();
    }

    void OnStartup() override
    {
#ifdef LOCAL_LEVEL_SCALING_WORLD_JOURNEY
        LocalLevelScaling::CreatureWindowOwner.store(runtime.enabled && runtime.window ? &Window : nullptr, std::memory_order_relaxed);
        if (runtime.enabled)
            Say(std::string(runtime.forced ? "every character plays the scaled world" : "open world scaling stays each character's choice") +
                (runtime.window ? ", lifted creatures stand from " + std::to_string(runtime.below) + " below to " +
                    std::to_string(runtime.above) + " above a character" : ""));
#endif
    }

    void OnShutdown() override
    {
#ifdef LOCAL_LEVEL_SCALING_WORLD_JOURNEY
        LocalLevelScaling::CreatureWindowOwner.store(nullptr);
        LocalLevelScaling::ScalingChoiceForced.store(false);
#endif
    }
};

/// A zone or an instance set by hand moves its creatures with it.
class JourneyCreatures : public AllCreatureScript
{
public:
    JourneyCreatures() : AllCreatureScript("JourneyCreatures") { }

    void OnBeforeCreatureSelectLevel(CreatureTemplate const* /*info*/, Creature* creature, uint8& level) override
    {
        if (!runtime.enabled || !creature || (zoneShift.empty() && mapShift.empty()))
            return;
        Map* map = creature->FindMap();
        if (!map)
            return;
        int delta = 0;
        auto instance = mapShift.find(map->GetId());
        if (instance != mapShift.end())
            delta = instance->second;
        else if (!map->Instanceable())
        {
            auto zone = zoneShift.find(map->GetZoneId(creature->GetPhaseMask(), creature->GetPositionX(), creature->GetPositionY(), creature->GetPositionZ()));
            if (zone != zoneShift.end())
                delta = zone->second;
        }
        if (delta)
            level = uint8(std::clamp<int>(int(level) + delta, 1, 63));
    }
};

class JourneyPlayers : public PlayerScript
{
public:
    JourneyPlayers() : PlayerScript("JourneyPlayers", { PLAYERHOOK_ON_LOGIN, PLAYERHOOK_ON_QUEST_COMPUTE_EXP,
        PLAYERHOOK_CAN_PLAYER_USE_PRIVATE_CHAT }) { }

    void OnPlayerLogin(Player* player) override
    {
        TellAddon(player);
    }

    /// The addon asks again once the interface is up (what came at login may have come before it was loaded).
    bool OnPlayerCanUseChat(Player* player, uint32 /*type*/, uint32 language, std::string& msg, Player* receiver) override
    {
        if (language != LANG_ADDON || receiver != player || msg.rfind(std::string(AddonPrefix) + "\t?", 0) != 0)
            return true;
        TellAddon(player);
        return false;
    }

    void OnPlayerQuestComputeXP(Player* /*player*/, Quest const* quest, uint32& xpValue) override
    {
        if (!runtime.enabled || !quest)
            return;
        auto part = outcome.questParts.find(quest->GetQuestId());
        double const rate = runtime.questXp[part != outcome.questParts.end() ? part->second : 0];
        if (std::fabs(rate - 1.0) > 1e-6)
            xpValue = uint32(std::lround(xpValue * rate));
    }
};

class JourneyCombat : public UnitScript
{
public:
    JourneyCombat() : UnitScript("JourneyCombat", true, { UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN, UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK }) { }

    void ModifySpellDamageTaken(Unit* /*target*/, Unit* attacker, int32& damage, SpellInfo const* /*spellInfo*/) override
    {
        if (!runtime.enabled || damage <= 0)
            return;
        double const factor = SpellFactor(attacker);
        if (std::fabs(factor - 1.0) > 1e-6)
            damage = std::max<int32>(1, int32(std::lround(damage * factor)));
    }

    void ModifyPeriodicDamageAurasTick(Unit* target, Unit* attacker, uint32& damage, SpellInfo const* spellInfo) override
    {
        // The same hook carries heals over time; those are left alone.
        if (!runtime.enabled || !damage || (spellInfo && spellInfo->IsPositive()) || (target && attacker && target->IsFriendlyTo(attacker)))
            return;
        double const factor = SpellFactor(attacker);
        if (std::fabs(factor - 1.0) > 1e-6)
            damage = std::max<uint32>(1, uint32(std::lround(damage * factor)));
    }
};

void AddSC_world_journey()
{
    new JourneyWorld();
    new JourneyCreatures();
    new JourneyPlayers();
    new JourneyCombat();
}
