/*
 * World Journey: dungeons alone through the Dungeon Finder.
 *
 * A player chooses in the Dungeon Finder (the "Alone" box of the addon, or .journey solo on) to go alone. The core patch
 * core-solo-dungeon-finder.patch keeps him apart from everybody else in the queue and starts his dungeon at once; without
 * the box the Dungeon Finder waits for a group of five as always. A dungeon he enters that way, alone, is a solo dungeon:
 * its creatures get the factors of Journey.SoloDungeon.* - health when they are made, melee and spell damage when they hit.
 * Everything else, other instances of the same dungeon included, stays as it is.
 * Released under GNU AGPL v3: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "Chat.h"
#include "ChatCommand.h"
#include "CommandScript.h"
#include "Config.h"
#include "Creature.h"
#include "Group.h"
#include "LFGMgr.h"
#include "Log.h"
#include "Map.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellInfo.h"
#include "WorldPacket.h"
#include "WorldSession.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

using namespace Acore::ChatCommands;

namespace
{
    char const* const SoloPrefix = "ZoneLevels";

    struct SoloSettings
    {
        bool enabled = false;
        double health = 0.5, damage = 0.6, spell = 0.6;
        double bossHealth = 0.5, bossDamage = 0.6, bossSpell = 0.6;
    };
    SoloSettings solo;

    /// Players who chose to go alone (as long as they are logged in; the addon tells the server again at login).
    std::unordered_set<ObjectGuid::LowType> alone;
    /// Solo dungeons by instance id: for each creature, the health it was given (so it is not scaled twice).
    std::unordered_map<uint32, std::unordered_map<ObjectGuid::LowType, uint32>> soloInstances;
    std::atomic<size_t> soloCount{ 0 };
    std::mutex lock;

    bool Available()
    {
#ifdef LFG_SOLO_QUEUE
        return solo.enabled;
#else
        return false;
#endif
    }

    void SendSolo(Player* player, bool on)
    {
        if (!player || !player->GetSession())
            return;
        WorldPacket data;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player,
            std::string(SoloPrefix) + "\tS" + (Available() ? "1" : "0") + (on ? "1" : "0"));
        player->SendDirectMessage(&data);
    }

    bool IsAlone(Player* player)
    {
        std::lock_guard<std::mutex> guard(lock);
        return alone.count(player->GetGUID().GetCounter()) != 0;
    }

    void SetAlone(Player* player, bool on)
    {
        if (!player)
            return;
        on = on && Available();
        {
            std::lock_guard<std::mutex> guard(lock);
            if (on)
                alone.insert(player->GetGUID().GetCounter());
            else
                alone.erase(player->GetGUID().GetCounter());
        }
#ifdef LFG_SOLO_QUEUE
        sLFGMgr->SetSoloQueue(player->GetGUID(), on);
#endif
        SendSolo(player, on);
    }

    double Clamp(float value) { return std::clamp(double(value), 0.05, 10.0); }

    bool IsBoss(Creature* creature)
    {
        return creature->IsDungeonBoss() || creature->isWorldBoss();
    }

    bool Counts(Creature* creature)
    {
        return creature && !creature->IsPet() && !creature->IsTotem() && !creature->IsCharmedOwnedByPlayerOrPlayer() &&
            creature->GetCreateHealth() > 0;
    }

    /// Gives a creature of a solo dungeon its health; nothing when it already has it.
    void ScaleHealth(Creature* creature, std::unordered_map<ObjectGuid::LowType, uint32>& given)
    {
        if (!Counts(creature))
            return;
        uint32 const now = creature->GetCreateHealth();
        auto it = given.find(creature->GetGUID().GetCounter());
        if (it != given.end() && it->second == now)
            return;     // given already, and not made anew since
        double const factor = IsBoss(creature) ? solo.bossHealth : solo.health;
        uint32 const health = uint32(std::max<double>(1.0, std::lround(now * factor)));
        float const percent = creature->GetHealthPct();
        creature->SetCreateHealth(health);
        creature->SetStatFlatModifier(UNIT_MOD_HEALTH, BASE_VALUE, float(health));
        creature->UpdateMaxHealth();
        creature->SetHealth(std::max<uint32>(1, uint32(creature->GetMaxHealth() * percent / 100.0f)));
        given[creature->GetGUID().GetCounter()] = health;
    }

    /// The factor of the damage a unit deals, when it is a creature of a solo dungeon.
    double DamageFactor(Unit* attacker, bool spell)
    {
        if (!soloCount.load(std::memory_order_relaxed) || !attacker)
            return 1.0;
        Creature* creature = attacker->ToCreature();
        if (!creature)
            if (Unit* owner = attacker->GetCharmerOrOwner())
                creature = owner->ToCreature();     // a totem or a summon of a creature
        if (!creature || creature->IsCharmedOwnedByPlayerOrPlayer())
            return 1.0;
        Map* map = creature->FindMap();
        if (!map || !map->IsDungeon())
            return 1.0;
        {
            std::lock_guard<std::mutex> guard(lock);
            if (!soloInstances.count(map->GetInstanceId()))
                return 1.0;
        }
        bool const boss = IsBoss(creature);
        return spell ? (boss ? solo.bossSpell : solo.spell) : (boss ? solo.bossDamage : solo.damage);
    }

    /// A player who chose to go alone enters a dungeon with the group the Dungeon Finder made for him alone.
    void MaybeSolo(Map* map, Player* player)
    {
        if (!Available() || !map || !player || !map->IsDungeon() || map->IsRaid() || !map->GetInstanceId())
            return;
        Group* group = player->GetGroup();
        if (!group || !group->isLFGGroup() || group->GetMembersCount() != 1 || !IsAlone(player))
            return;
        std::lock_guard<std::mutex> guard(lock);
        auto [it, made] = soloInstances.try_emplace(map->GetInstanceId());
        if (!made)
            return;
        soloCount.store(soloInstances.size(), std::memory_order_relaxed);
        // The creatures that are there already; those that come later get their health when they are made.
        for (auto const& [spawn, creature] : map->GetCreatureBySpawnIdStore())
            if (creature && creature->IsInWorld())
                ScaleHealth(creature, it->second);
        LOG_INFO("module", ">> World Journey: {} goes into {} alone (instance {})", player->GetName(), map->GetMapName(), map->GetInstanceId());
    }
}

class JourneySoloWorld : public WorldScript
{
public:
    JourneySoloWorld() : WorldScript("JourneySoloWorld", { WORLDHOOK_ON_AFTER_CONFIG_LOAD }) { }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        solo.enabled = sConfigMgr->GetOption<bool>("Journey.Enable", true) && sConfigMgr->GetOption<bool>("Journey.SoloDungeon.Enable", false);
        solo.health = Clamp(sConfigMgr->GetOption<float>("Journey.SoloDungeon.Health", 0.5f));
        solo.damage = Clamp(sConfigMgr->GetOption<float>("Journey.SoloDungeon.Damage", 0.6f));
        solo.spell = Clamp(sConfigMgr->GetOption<float>("Journey.SoloDungeon.SpellDamage", 0.6f));
        solo.bossHealth = Clamp(sConfigMgr->GetOption<float>("Journey.SoloDungeon.Boss.Health", float(solo.health)));
        solo.bossDamage = Clamp(sConfigMgr->GetOption<float>("Journey.SoloDungeon.Boss.Damage", float(solo.damage)));
        solo.bossSpell = Clamp(sConfigMgr->GetOption<float>("Journey.SoloDungeon.Boss.SpellDamage", float(solo.spell)));
#ifndef LFG_SOLO_QUEUE
        if (solo.enabled)
            LOG_WARN("server.loading", ">> World Journey: Journey.SoloDungeon.Enable = 1, but the core lacks core-solo-dungeon-finder.patch; dungeons alone stay off");
#endif
    }
};

class JourneySoloPlayers : public PlayerScript
{
public:
    // At login the addon tells the server the choice it keeps for the character ("s1" / "s0") and gets the answer.
    JourneySoloPlayers() : PlayerScript("JourneySoloPlayers", { PLAYERHOOK_ON_LOGOUT, PLAYERHOOK_CAN_PLAYER_USE_PRIVATE_CHAT,
        PLAYERHOOK_CAN_JOIN_LFG }) { }

    void OnPlayerLogout(Player* player) override
    {
        {
            std::lock_guard<std::mutex> guard(lock);
            alone.erase(player->GetGUID().GetCounter());
        }
#ifdef LFG_SOLO_QUEUE
        sLFGMgr->SetSoloQueue(player->GetGUID(), false);
#endif
    }

    /// A dungeon entered alone is the group's dungeon too: a group of one inside it that queues again (the Dungeon Finder
    /// of the client asks to fill the missing places) would get a second dungeon of its own at once. It stays where it is.
    bool OnPlayerCanJoinLfg(Player* player, uint8 /*roles*/, lfg::LfgDungeonSet& /*dungeons*/, std::string const& /*comment*/) override
    {
#ifdef LFG_SOLO_QUEUE
        Group* group = player ? player->GetGroup() : nullptr;
        if (!group || !group->isLFGGroup() || group->GetMembersCount() != 1 || !IsAlone(player))
            return true;
        if (sLFGMgr->GetState(group->GetGUID()) != lfg::LFG_STATE_DUNGEON)
            return true;
        Map* map = player->GetMap();
        if (!map || !map->IsDungeon())
            return true;
        LOG_INFO("module", "World Journey: {} is in his dungeon alone; the Dungeon Finder does not queue him again", player->GetName());
        return false;
#else
        (void)player;
        return true;
#endif
    }

    /// The addon: "s1" / "s0" (the box), "s?" (what is it now).
    bool OnPlayerCanUseChat(Player* player, uint32 /*type*/, uint32 language, std::string& msg, Player* receiver) override
    {
        std::string const head = std::string(SoloPrefix) + "\ts";
        if (language != LANG_ADDON || receiver != player || msg.size() != head.size() + 1 || msg.rfind(head, 0) != 0)
            return true;
        char const value = msg.back();
        if (value == '1' || value == '0')
            SetAlone(player, value == '1');
        else
            SendSolo(player, IsAlone(player));
        return false;
    }
};

class JourneySoloMaps : public AllMapScript
{
public:
    JourneySoloMaps() : AllMapScript("JourneySoloMaps", { ALLMAPHOOK_ON_PLAYER_ENTER_ALL, ALLMAPHOOK_ON_DESTROY_INSTANCE }) { }

    void OnPlayerEnterAll(Map* map, Player* player) override
    {
        MaybeSolo(map, player);
    }

    void OnDestroyInstance(MapInstanced* /*mapInstanced*/, Map* map) override
    {
        if (!map)
            return;
        std::lock_guard<std::mutex> guard(lock);
        soloInstances.erase(map->GetInstanceId());
        soloCount.store(soloInstances.size(), std::memory_order_relaxed);
    }
};

class JourneySoloCreatures : public AllCreatureScript
{
public:
    JourneySoloCreatures() : AllCreatureScript("JourneySoloCreatures") { }

    /// A creature made (or made anew: respawn, level change) in a solo dungeon gets its health.
    void OnCreatureSelectLevel(CreatureTemplate const* /*info*/, Creature* creature) override
    {
        if (!soloCount.load(std::memory_order_relaxed) || !creature)
            return;
        Map* map = creature->FindMap();
        if (!map || !map->IsDungeon())
            return;
        std::lock_guard<std::mutex> guard(lock);
        auto it = soloInstances.find(map->GetInstanceId());
        if (it != soloInstances.end())
            ScaleHealth(creature, it->second);
    }

    void OnCreatureAddWorld(Creature* creature) override
    {
        OnCreatureSelectLevel(nullptr, creature);
    }
};

class JourneySoloCombat : public UnitScript
{
public:
    JourneySoloCombat() : UnitScript("JourneySoloCombat", true,
        { UNITHOOK_MODIFY_MELEE_DAMAGE, UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN, UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK }) { }

    void ModifyMeleeDamage(Unit* /*target*/, Unit* attacker, uint32& damage) override
    {
        double const factor = damage ? DamageFactor(attacker, false) : 1.0;
        if (std::fabs(factor - 1.0) > 1e-6)
            damage = std::max<uint32>(1, uint32(std::lround(damage * factor)));
    }

    void ModifySpellDamageTaken(Unit* /*target*/, Unit* attacker, int32& damage, SpellInfo const* /*spellInfo*/) override
    {
        double const factor = damage > 0 ? DamageFactor(attacker, true) : 1.0;
        if (std::fabs(factor - 1.0) > 1e-6)
            damage = std::max<int32>(1, int32(std::lround(damage * factor)));
    }

    void ModifyPeriodicDamageAurasTick(Unit* target, Unit* attacker, uint32& damage, SpellInfo const* spellInfo) override
    {
        if (!damage || (spellInfo && spellInfo->IsPositive()) || (target && attacker && target->IsFriendlyTo(attacker)))
            return;
        double const factor = DamageFactor(attacker, true);
        if (std::fabs(factor - 1.0) > 1e-6)
            damage = std::max<uint32>(1, uint32(std::lround(damage * factor)));
    }
};

class JourneySoloCommands : public CommandScript
{
public:
    JourneySoloCommands() : CommandScript("JourneySoloCommands") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable journey =
        {
            { "solo", HandleSolo, SEC_PLAYER, Console::No },
        };
        static ChatCommandTable root =
        {
            { "journey", journey },
        };
        return root;
    }

    /// .journey solo [on|off]: go into dungeons of the Dungeon Finder alone, or with a group again.
    static bool HandleSolo(ChatHandler* handler, Optional<std::string> choice)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;
        if (!Available())
        {
            handler->SendSysMessage("Dungeons alone are switched off on this realm (Journey.SoloDungeon.Enable).");
            return true;
        }
        bool on = !IsAlone(player);
        if (choice)
        {
            std::string text = *choice;
            std::transform(text.begin(), text.end(), text.begin(), ::tolower);
            on = text == "on" || text == "1" || text == "an" || text == "ein";
        }
        SetAlone(player, on);
        handler->SendSysMessage(on ? "Dungeon Finder: you go alone - your dungeon starts as soon as one is found for you."
                                   : "Dungeon Finder: you wait for a group of five again.");
        return true;
    }
};

void AddSC_world_journey_solo()
{
    new JourneySoloWorld();
    new JourneySoloPlayers();
    new JourneySoloMaps();
    new JourneySoloCreatures();
    new JourneySoloCombat();
    new JourneySoloCommands();
}
