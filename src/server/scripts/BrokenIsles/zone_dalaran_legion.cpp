/*
 * Copyright (C) 2017-2018 AshamaneProject <https://github.com/AshamaneProject>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "Conversation.h"
#include "GameObject.h"
#include "MapManager.h"
#include "ObjectMgr.h"
#include "PhasingHandler.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellMgr.h"
#include "ScriptedCreature.h"
#include "ScriptedEscortAI.h"
#include "ScriptedGossip.h"
#include "SpellScript.h"

/*
 * Dalaran above Karazhan
 *
 * Legion Intro
 */

enum DalaranIntro
{
    PHASE_DALARAN_KARAZHAN  = 5829,
    QUEST_BLINK_OF_AN_EYE   = 44663,
};

enum DalaranIntroSpells
{
    SPELL_MAGE_LEARN_GUARDIAN_HALL_TP   = 204287,
    SPELL_WAR_LEARN_JUMP_TO_SKYHOLD     = 192084,
    SPELL_DRUID_CLASS_HALL_TP           = 204874,
    SPELL_CREATE_CLASS_HALL_ALLIANCE    = 185506,
    SPELL_CREATE_CLASS_HALL_HORDE       = 192191,
};

enum DalaranIntroEtc
{
    CONVERSATION_KHADGAR_BLINK_OF_EYE = 3827,
};

// TODO : All this script is temp fix,
// remove it when legion start quests are properly fixed
class OnLegionArrival : public PlayerScript
{
public:
    OnLegionArrival() : PlayerScript("OnLegionArrival") { }



    void OnLogin(Player* player, bool firstLogin) override
    {
        // Can happen in recovery cases
        if (player->getLevel() >= 100 && firstLogin)
            HandleLegionArrival(player);
    }

    void OnLevelChanged(Player* player, uint8 oldLevel) override
    {
        if (oldLevel < 100 && player->getLevel() >= 100)
            HandleLegionArrival(player);
    }

    void HandleLegionArrival(Player* player)
    {
        switch (player->getClass())
        {
            case CLASS_MAGE:
                player->CastSpell(player, SPELL_MAGE_LEARN_GUARDIAN_HALL_TP, true);
                break;
            case CLASS_WARRIOR:
                player->CastSpell(player, SPELL_WAR_LEARN_JUMP_TO_SKYHOLD, true);
                break;
            case CLASS_DRUID:
                player->CastSpell(player, SPELL_DRUID_CLASS_HALL_TP, true);
                break;
            case CLASS_HUNTER:
                player->m_taxi.SetTaximaskNode(1848); // Hunter Class Hall
                break;
            default:
                break;
        }

        player->CastSpell(player, player->IsInAlliance() ? SPELL_CREATE_CLASS_HALL_ALLIANCE : SPELL_CREATE_CLASS_HALL_HORDE, true);

        if (player->GetQuestStatus(QUEST_BLINK_OF_AN_EYE) == QUEST_STATUS_NONE)
        {
            Conversation::CreateConversation(CONVERSATION_KHADGAR_BLINK_OF_EYE, player, player->GetPosition(), { player->GetGUID() });

            if (const Quest* quest = sObjectMgr->GetQuestTemplate(QUEST_BLINK_OF_AN_EYE))
                player->AddQuest(quest, nullptr);
        }
    }
};

enum OnLevelReached
{
    QUEST_UNITING_THE_ISLES = 43341,
};

class On110Arrival : public PlayerScript
{
public:
    On110Arrival() : PlayerScript("On110Arrival") { }

    void OnLogin(Player* player, bool firstLogin) override
    {
        // Can happen in recovery cases
        if (player->getLevel() >= 110 && firstLogin)
            Handle110Arrival(player);
    }

    void OnLevelChanged(Player* player, uint8 oldLevel) override
    {
        if (oldLevel < 110 && player->getLevel() >= 110)
            Handle110Arrival(player);
    }

    void Handle110Arrival(Player* player)
    {
        if (player->GetQuestStatus(QUEST_UNITING_THE_ISLES) == QUEST_STATUS_NONE)
            if (const Quest* quest = sObjectMgr->GetQuestTemplate(QUEST_UNITING_THE_ISLES))
                player->AddQuest(quest, nullptr);
    }
};

// 228329 & 228330 - TÚlÚportation
class spell_dalaran_teleportation : public SpellScript
{
    PrepareSpellScript(spell_dalaran_teleportation);

    void EffectTeleportDalaranKarazhan(SpellEffIndex effIndex)
    {
        if (Player* player = GetCaster()->ToPlayer())
        {
            if (player->getLevel() < 100 || player->GetQuestStatus(QUEST_BLINK_OF_AN_EYE) != QUEST_STATUS_INCOMPLETE)
                PreventHitEffect(effIndex);
            else
            {
                if (SpellTargetPosition const* targetPosition = sSpellMgr->GetSpellTargetPosition(GetSpellInfo()->Id, effIndex))
                    if (Map* map = sMapMgr->FindMap(targetPosition->target_mapId, 0))
                        map->LoadGrid(targetPosition->target_X, targetPosition->target_Y);
            }
        }
    }

    void EffectTeleportDalaranLegion(SpellEffIndex effIndex)
    {
        if (Player* player = GetCaster()->ToPlayer())
            if (player->getLevel() < 100 || player->GetQuestStatus(QUEST_BLINK_OF_AN_EYE) == QUEST_STATUS_INCOMPLETE)
                PreventHitEffect(effIndex);
    }

    void Register() override
    {
        OnEffectLaunch += SpellEffectFn(spell_dalaran_teleportation::EffectTeleportDalaranKarazhan, EFFECT_0, SPELL_EFFECT_TRIGGER_SPELL);
        OnEffectLaunch += SpellEffectFn(spell_dalaran_teleportation::EffectTeleportDalaranLegion, EFFECT_1, SPELL_EFFECT_TRIGGER_SPELL);
    }
};

enum KhadgarDalaranDeadwind
{
    SPELL_PLAY_DALARAN_TELEPORTATION_SCENE = 227861
};

// 113986 - Khadgar
class npc_dalaran_karazhan_khadgar : public CreatureScript
{
public:
    npc_dalaran_karazhan_khadgar() : CreatureScript("npc_dalaran_karazhan_khadgar") { }

    bool OnGossipSelect(Player* player, Creature* /*creature*/, uint32 /*uiSender*/, uint32 /*uiAction*/) override
    {
        player->CastSpell(player, SPELL_PLAY_DALARAN_TELEPORTATION_SCENE, true);
        return true;
    }
};

class scene_dalaran_kharazan_teleportion : public SceneScript
{
public:
    scene_dalaran_kharazan_teleportion() : SceneScript("scene_dalaran_kharazan_teleportion") { }

    void OnSceneTriggerEvent(Player* player, uint32 /*sceneInstanceID*/, SceneTemplate const* /*sceneTemplate*/, std::string const& triggerName) override
    {
        if (triggerName == "invisibledalaran")
            PhasingHandler::AddPhase(player, PHASE_DALARAN_KARAZHAN);
    }

    void OnSceneEnd(Player* player, uint32 /*sceneInstanceID*/, SceneTemplate const* /*sceneTemplate*/) override
    {
        player->KilledMonsterCredit(114506);
        player->TeleportTo(MAP_BROKEN_ISLANDS, -827.82f, 4369.25f, 738.64f, 1.893364f);
    }
};

enum OrderHallIntroSpells
{
    SPELL_SUMMON_EITRIGG                = 216443,
    SPELL_SUMMON_DALTON                 = 216497,
    SPELL_SUMMON_PALADIN                = 190886,
    SPELL_SUMMON_SNOWFEATHER            = 196908,
    SPELL_SUMMON_RAVENHOLDT_COURIER     = 201208,
    SPELL_SUMMON_PRIEST_MESSENGER_H     = 226409,
    SPELL_SUMMON_PRIEST_MESSENGER_A     = 226412,
    SPELL_AN_AUDIENCE_WITH_THE_KING     = 200023,
    SPELL_MAGE_ORDERHALL_FORMATION      = 195356,
    SPELL_SUMMON_RISSTYN                = 204860,
    SPELL_SUMMON_DA_NEL                 = 193978,
    SPELL_SUMMON_ARCH_DRUID_HAMUUL      = 199277,
    SPELL_SUMMON_KORVAS_BLOODTHORN      = 195286
};

/*
 * Legion Dalaran
 */

// Zone 8392
class zone_legion_dalaran_underbelly : public ZoneScript
{
public:
    zone_legion_dalaran_underbelly() : ZoneScript("zone_legion_dalaran_underbelly") { }

    void OnPlayerEnter(Player* player) override
    {
        player->SeamlessTeleportToMap(MAP_DALARAN_UNDERBELLY);
    }

    void OnPlayerExit(Player* player) override
    {
        if (player->GetMapId() == MAP_DALARAN_UNDERBELLY)
            player->SeamlessTeleportToMap(MAP_BROKEN_ISLANDS);
    }
};

enum KhadgarAzsunaIntro
{
    QUEST_DOWN_TO_AZSUNA                = 41220,
    SPELL_TAXI_DALARAN_AZSUNA_ALLIANCE  = 205098,
    SPELL_TAXI_DALARAN_AZSUNA_HORDE     = 205203,
    KHADGAR_SAY_SECOND_LINE             = 1,
};

class npc_archmage_khadgar_86563 : public CreatureScript
{
public:
    npc_archmage_khadgar_86563() : CreatureScript("npc_archmage_khadgar_86563") { }



    bool OnGossipSelect(Player* player, Creature* /*creature*/, uint32 /*sender*/, uint32 /*action*/) override
    {
        if (player->HasQuest(QUEST_DOWN_TO_AZSUNA) || player->GetQuestStatus(QUEST_DOWN_TO_AZSUNA) == QUEST_STATUS_INCOMPLETE)
            player->CastSpell(player, player->IsInAlliance() ? SPELL_TAXI_DALARAN_AZSUNA_ALLIANCE : SPELL_TAXI_DALARAN_AZSUNA_HORDE, true); // KillCredit & SendTaxi

        return true;
    }

    bool OnQuestAccept(Player* /*player*/, Creature* creature, Quest const* quest) override
    {
        if (quest->GetQuestId() == QUEST_DOWN_TO_AZSUNA)
        {
            creature->AI()->Talk(KHADGAR_SAY_SECOND_LINE);
        }
        return true;
    }
};

void AddSC_dalaran_legion()
{
    new OnLegionArrival();
    new On110Arrival();

    RegisterSpellScript(spell_dalaran_teleportation);
    new npc_dalaran_karazhan_khadgar();
    new scene_dalaran_kharazan_teleportion();
    new zone_legion_dalaran_underbelly();
	 new npc_archmage_khadgar_86563();
}
