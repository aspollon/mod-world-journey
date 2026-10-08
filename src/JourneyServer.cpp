/*
 * The journey in the server: the world database at startup (JourneyDb), and what has to be done in memory - the
 * numbers of spells and enchantments, the entry levels of instances, the Dungeon Finder and the battlegrounds - and
 * in combat, the spells of creatures that now stand at a lower level.
 * Released under GNU AGPL v3: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "JourneyInstances.h"
#include "JourneyDb.h"

#include "Config.h"
#include "Creature.h"
#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "LFGMgr.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "World.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <map>
#include <unordered_map>

namespace
{
    char const* const ConfigPrefix = "Journey";
    char const* const TablePrefix = "journey";

    bool enabled = true;
    bool debug = false;
    journey::Outcome outcome;

    std::string Option(char const* name)
    {
        return std::string(ConfigPrefix) + "." + name;
    }

    void Say(std::string const& line)
    {
        LOG_INFO("server.loading", ">> {}: {}", ConfigPrefix, line);
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

    /// The stat budgets of the item levels, from the server's own copy of RandPropPoints.dbc.
    journey::PropertyPoints ReadPoints()
    {
        std::string dataPath = sConfigMgr->GetOption<std::string>("DataDir", "./");
        if (dataPath.empty() || (dataPath.back() != '/' && dataPath.back() != '\\'))
            dataPath += '/';
        journey::PropertyPoints points;
        if (!journey::ReadPropertyPoints(dataPath + "dbc/RandPropPoints.dbc", points))
            LOG_WARN("server.loading", ">> {}: {}dbc/RandPropPoints.dbc not readable - the stats of items follow their item level instead.",
                ConfigPrefix, dataPath);
        return points;
    }

    // ------------------------------------------------------------------------------------------ spells and enchantments

    /// The numbers of a spell effect that are plain amounts - damage, healing, mana, stats, absorbs - not percentages,
    /// speeds or anything else.
    bool IsAmount(SpellEffectInfo const& effect)
    {
        switch (effect.Effect)
        {
            case SPELL_EFFECT_SCHOOL_DAMAGE:
            case SPELL_EFFECT_HEAL:
            case SPELL_EFFECT_ENERGIZE:
            case SPELL_EFFECT_HEALTH_LEECH:
            case SPELL_EFFECT_HEAL_MECHANICAL:
                return true;
            case SPELL_EFFECT_APPLY_AURA:
            case SPELL_EFFECT_APPLY_AREA_AURA_PARTY:
            case SPELL_EFFECT_APPLY_AREA_AURA_RAID:
            case SPELL_EFFECT_APPLY_AREA_AURA_FRIEND:
            case SPELL_EFFECT_PERSISTENT_AREA_AURA:
                switch (effect.ApplyAuraName)
                {
                    case SPELL_AURA_PERIODIC_DAMAGE:
                    case SPELL_AURA_PERIODIC_HEAL:
                    case SPELL_AURA_PERIODIC_ENERGIZE:
                    case SPELL_AURA_PERIODIC_LEECH:
                    case SPELL_AURA_DAMAGE_SHIELD:
                    case SPELL_AURA_MOD_STAT:
                    case SPELL_AURA_MOD_RESISTANCE:
                    case SPELL_AURA_MOD_BASE_RESISTANCE:
                    case SPELL_AURA_MOD_DAMAGE_DONE:
                    case SPELL_AURA_MOD_HEALING_DONE:
                    case SPELL_AURA_MOD_ATTACK_POWER:
                    case SPELL_AURA_MOD_RANGED_ATTACK_POWER:
                    case SPELL_AURA_MOD_POWER_REGEN:
                    case SPELL_AURA_SCHOOL_ABSORB:
                    case SPELL_AURA_MANA_SHIELD:
                    case SPELL_AURA_MOD_INCREASE_HEALTH:
                    case SPELL_AURA_MOD_INCREASE_ENERGY:
                    case SPELL_AURA_MOD_RATING:
                    case SPELL_AURA_MOD_TARGET_RESISTANCE:
                        return true;
                    default:
                        return false;
                }
            default:
                return false;
        }
    }

    bool IsTrigger(SpellEffectInfo const& effect)
    {
        if (effect.Effect == SPELL_EFFECT_TRIGGER_SPELL || effect.Effect == SPELL_EFFECT_TRIGGER_MISSILE)
            return true;
        return effect.IsAura() && (effect.ApplyAuraName == SPELL_AURA_PROC_TRIGGER_SPELL ||
            effect.ApplyAuraName == SPELL_AURA_PERIODIC_TRIGGER_SPELL || effect.ApplyAuraName == SPELL_AURA_PROC_TRIGGER_SPELL_WITH_VALUE);
    }

    bool IsEnchant(SpellEffectInfo const& effect)
    {
        return effect.Effect == SPELL_EFFECT_ENCHANT_ITEM || effect.Effect == SPELL_EFFECT_ENCHANT_ITEM_TEMPORARY ||
            effect.Effect == SPELL_EFFECT_ENCHANT_HELD_ITEM;
    }

    /// Works out what every spell and enchantment touched by the journey is worth, following triggered spells and
    /// the spells of enchantments; then changes them, once each. Shared by things of different levels, the gentler
    /// factor wins.
    struct Rescale
    {
        std::unordered_map<uint32, double> spells, enchants;
        std::deque<std::pair<bool, uint32>> pending;   // (enchant?, id)

        void Spell(uint32 id, double factor)
        {
            Note(spells, false, id, factor);
        }

        void Enchant(uint32 id, double factor)
        {
            Note(enchants, true, id, factor);
        }

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

        static int32 Scaled(int32 value, double factor)
        {
            return int32(std::lround(value * factor));
        }

        static uint32 LevelOf(uint32 level)
        {
            return level > 1 ? uint32(journey::Compress(int(level), journey::ERA_CLASSIC)) : level;
        }

        uint32 ApplySpells()
        {
            uint32 changed = 0;
            for (auto const& [id, factor] : spells)
            {
                if (factor >= 0.999)
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
                if (debug)
                    LOG_INFO("module", "{}: spell {} at {:.0f}%", ConfigPrefix, id, factor * 100.0);
            }
            return changed;
        }

        uint32 ApplyEnchants()
        {
            uint32 changed = 0;
            for (auto const& [id, factor] : enchants)
            {
                if (factor >= 0.999)
                    continue;
                auto* entry = const_cast<SpellItemEnchantmentEntry*>(sSpellItemEnchantmentStore.LookupEntry(id));
                if (!entry)
                    continue;
                bool any = false;
                for (uint32 i = 0; i < MAX_SPELL_ITEM_ENCHANTMENT_EFFECTS; ++i)
                    if (entry->amount[i] && (entry->type[i] == ITEM_ENCHANTMENT_TYPE_DAMAGE ||
                        entry->type[i] == ITEM_ENCHANTMENT_TYPE_RESISTANCE || entry->type[i] == ITEM_ENCHANTMENT_TYPE_STAT))
                    {
                        entry->amount[i] = std::max<uint32>(1, uint32(std::lround(entry->amount[i] * factor)));
                        any = true;
                    }
                changed += any;
            }
            return changed;
        }
    };

    void RescaleSpellsAndEnchants()
    {
        Rescale rescale;
        for (auto const& [id, factor] : outcome.spellFactors)
            rescale.Spell(id, factor);
        for (auto const& [id, factor] : outcome.enchantFactors)
            rescale.Enchant(id, factor);
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
        rescale.Follow();
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
        Say(std::to_string(spells) + " spell(s) of items and " + std::to_string(enchants) + " enchantment(s) and gem(s) scaled to the journey, " +
            std::to_string(lowered) + " enchantment(s) open at 60");
    }

    // ------------------------------------------------------------------------------------------ instances

    std::map<std::pair<uint32, uint32>, uint32> InstanceEntries()
    {
        std::map<std::pair<uint32, uint32>, uint32> entries;
        for (JourneyInstance const& instance : JourneyInstances)
            entries[{ instance.map, instance.difficulty }] = instance.entry;
        return entries;
    }

    void SetInstanceEntries()
    {
        uint32 changed = 0;
        for (JourneyInstance const& instance : JourneyInstances)
            if (DungeonProgressionRequirements const* access = sObjectMgr->GetAccessRequirement(instance.map, Difficulty(instance.difficulty)))
            {
                auto* writable = const_cast<DungeonProgressionRequirements*>(access);
                if (writable->levelMin != instance.entry)
                {
                    writable->levelMin = uint8(instance.entry);
                    ++changed;
                }
            }
        Say(std::to_string(changed) + " instance(s) can be entered at their new levels");
    }

    /// The Dungeon Finder offers every dungeon from its new entry level to 60; a random dungeon from the lowest
    /// entry among the dungeons it draws from.
    void SetDungeonFinder()
    {
        auto const entries = InstanceEntries();
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
            dungeon->minlevel = uint8(entry->second);
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
        Say(std::to_string(changed) + " battleground(s) open at 60");
    }

    // ------------------------------------------------------------------------------------------ in combat

    /// What a blow is worth at a level: the weapon damage and attack power of that level's base stats.
    double HitAt(uint8 level, uint8 unitClass, uint8 expansion)
    {
        CreatureBaseStats const* stats = sObjectMgr->GetCreatureBaseStats(level, unitClass);
        if (!stats)
            return 0.0;
        return stats->BaseDamage[std::min<uint8>(expansion, MAX_EXPANSIONS - 1)] + stats->AttackPower / 14.0 * 2.0;
    }

    /// How much less a creature's spells hit now than at the level they were made for: their numbers are those of
    /// the original level.
    double SpellFactor(Unit* attacker)
    {
        Creature* creature = attacker ? attacker->ToCreature() : nullptr;
        if (!creature && attacker)
            if (Unit* owner = attacker->GetCharmerOrOwner())
                creature = owner->ToCreature();      // a totem or a summon of a creature
        if (!creature)
            return 1.0;
        CreatureTemplate const* info = creature->GetCreatureTemplate();
        auto shift = info ? outcome.shifts.find(info->Entry) : outcome.shifts.end();
        if (shift == outcome.shifts.end())
            return 1.0;
        journey::Shift const& s = shift->second;
        uint8 const now = creature->GetLevel();
        // The original level this creature stands for: the same place in its original range.
        double const place = s.newMax > s.newMin ? double(std::clamp<int>(now, s.newMin, s.newMax) - s.newMin) / double(s.newMax - s.newMin) : 0.0;
        uint8 const was = uint8(std::lround(s.origMin + place * (s.origMax - s.origMin)));
        double const before = HitAt(was, info->unit_class, s.origExpansion);
        double const after = HitAt(now, info->unit_class, s.newExpansion);
        if (before <= 0.0 || after <= 0.0 || after >= before)
            return 1.0;
        return after / before;
    }
}

class JourneyWorld : public WorldScript
{
public:
    JourneyWorld() : WorldScript("JourneyWorld") { }

    void OnAfterConfigLoad(bool reload) override
    {
        enabled = sConfigMgr->GetOption<bool>(Option("Enable"), true);
        debug = sConfigMgr->GetOption<bool>(Option("Debug"), false);
        if (reload)
            return;     // set once, before the world reads its data

        uint32 const started = getMSTime();
        journey::Settings settings;
        settings.enabled = enabled;
        settings.prefix = TablePrefix;
        outcome = journey::Outcome();
        journey::Run(WorldDb(), settings, enabled ? ReadPoints() : journey::PropertyPoints(), outcome);
        Say("the world database " + std::string(enabled ? "is on the journey" : "is as it came") + " (" + std::to_string(GetMSTimeDiffToNow(started)) + " ms)");
    }

    void OnBeforeWorldInitialized() override
    {
        if (!enabled)
            return;
        RescaleSpellsAndEnchants();
        SetInstanceEntries();
        SetDungeonFinder();
        SetBattlegroundBrackets();
    }
};

class JourneyCombat : public UnitScript
{
public:
    JourneyCombat() : UnitScript("JourneyCombat", true, { UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN, UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK }) { }

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

void AddSC_world_journey()
{
    new JourneyWorld();
    new JourneyCombat();
}
