#include "highmaul.h"
#include "GameObjectAI.h"
#include "Group.h"

Position const g_CenterPos = { 3903.39f, 8608.15f, 364.71f, 5.589f };

/// Ko'ragh <Breaker of Magic> - 79015
class boss_koragh : public CreatureScript
{
    public:
        boss_koragh() : CreatureScript("boss_koragh") { }

        enum eSpells
        {
            /// Cosmetic
            RuneChargingPermanent       = 174415,
            RuneChargingTemporary       = 160721,
            NullificationRuneEmpowered  = 166482,
            CausticEnergyAreaTrigger    = 160720,
            /// Nullification Barrier
            NullificationBarrierPower   = 163612,
            NullificationBarrierAbsorb  = 156803,
            BreakersStrength            = 162569,
            NullificationBarrierAbsorb2 = 163134,   ///< For Players
            MarkOfNullification         = 172886,
            VulnerabilityAura           = 160734,
            KnockbackForRecharge        = 174856,
            VolatileAnomaliesAura       = 161378,
            /// Expel Magic: Fire
            ExpelMagicFireDoT           = 162185,
            /// Expel Magic: Arcane
            ExpelMagicArcaneAura        = 162186,
            /// Expel Magic: Shadow
            ExpelMagicShadow            = 162184,
            /// Expel Magic: Frost
            ExpelMagicFrostAreaTrigger  = 172747,
            ExpelMagicFrostDamage       = 161411,
            ExpelMagicFrostAura         = 172813,
            /// Expell Magic: Fel
            ExpelMagicFelDummy          = 172895,
            /// Suppression Field
            SuppressionFieldAura        = 161328,
            SuppressionFieldMissile     = 161331,   ///< Triggers 161330 (AreaTrigger) and 161358 (Damage)
            SuppressionFieldDoT         = 161345,
            SuppressionFieldSilence     = 162595,
            /// Overflowing Energy
            OverflowingEnergySpawn      = 161574,
            /// Loot
            KoraghBonus                 = 177526
        };

        enum eEvents
        {
            EventExpelMagicFire = 1,
            EventExpelMagicArcane,
            EventExpelMagicFrost,
            EventExpelMagicShadow,
            EventSuppressionField,
            EventOverflowingEnergy,
            EventBerserk,
            EventExpelMagicFel
        };

        enum eCosmeticEvents
        {
            EventBreakersStrength = 1,
            EventEndOfCharging
        };

        enum eActions
        {
            CancelBreakersStrength,
            ActionSuppressionField
        };

        enum eCreatures
        {
            BreakerOfFel    = 86330,
            BreakerOfFire   = 86329,
            BreakerOfFrost  = 86326
        };

        enum eTalks
        {
            Intro,
            Aggro,
            BarrierShattered,
            ExpelMagic,
            SuppressionField,
            DominatingPower,
            Berserk,
            Slay,
            Death,
            CausticEnergyWarn,
            ExpelMagicFelWarn,
            ExpelMagicArcaneWarn
        };

        enum eAnimKit
        {
            AnimWaiting = 7289
        };

        enum eMove
        {
            MoveToCenter
        };

        enum eDatas
        {
            DataMaxRunicPlayers,
            DataRunicPlayersCount
        };

        struct boss_koraghAI : public BossAI
        {
            boss_koraghAI(Creature* p_Creature) : BossAI(p_Creature, eHighmaulDatas::BossKoragh)
            {
                m_Instance      = p_Creature->GetInstanceScript();
                m_Init          = false;
            }

            EventMap m_Events;
            EventMap m_CosmeticEvents;

            InstanceScript* m_Instance;

            std::set<ObjectGuid> m_RitualistGuids;
            bool m_Init;

            ObjectGuid m_FlyingRune;
            ObjectGuid m_FloorRune;

            bool m_Charging;

            uint8 m_RunicPlayers;
            uint8 m_RunicPlayersCount;

            ObjectGuid m_SuppressionFieldTarget;

            uint32 m_overflowingOrbsCount;

            void Reset() override
            {
                m_Events.Reset();

                _Reset();

                me->RemoveAurasDueToSpell(Berserker);

                if (m_RitualistGuids.empty() && m_Init)
                {
                    me->SetReactState(ReactStates::REACT_AGGRESSIVE);
                    me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_PC | UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE);

                    me->SetAIAnimKitId(0);
                }
                else
                {
                    me->SetReactState(ReactStates::REACT_PASSIVE);
                    me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_PC | UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE);

                    me->SetAIAnimKitId(eAnimKit::AnimWaiting);
                }

                me->RemoveAllAreaTriggers();

                summons.DespawnAll();

                AddTimedDelayedOperation(1 * TimeConstants::IN_MILLISECONDS, [this]() -> void
                {
                    if (m_RitualistGuids.empty() && !m_Init)
                    {
                        std::list<Creature*> l_BreakerList;
                        me->GetCreatureListWithEntryInGrid(l_BreakerList, eCreatures::BreakerOfFel, 30.0f);

                        for (Creature* l_Breaker : l_BreakerList)
                        {
                            if (l_Breaker->IsAlive())
                                m_RitualistGuids.insert(l_Breaker->GetGUID());
                        }

                        l_BreakerList.clear();
                        me->GetCreatureListWithEntryInGrid(l_BreakerList, eCreatures::BreakerOfFire, 30.0f);

                        for (Creature* l_Breaker : l_BreakerList)
                        {
                            if (l_Breaker->IsAlive())
                                m_RitualistGuids.insert(l_Breaker->GetGUID());
                        }

                        l_BreakerList.clear();
                        me->GetCreatureListWithEntryInGrid(l_BreakerList, eCreatures::BreakerOfFrost, 30.0f);

                        for (Creature* l_Breaker : l_BreakerList)
                        {
                            if (l_Breaker->IsAlive())
                                m_RitualistGuids.insert(l_Breaker->GetGUID());
                        }

                        m_Init = true;
                    }

                    std::list<Creature*> l_RuneList;
                    me->GetCreatureListWithEntryInGrid(l_RuneList, eHighmaulCreatures::RuneOfNullification, 50.0f);

                    for (Creature* l_Rune : l_RuneList)
                    {
                        if (l_Rune->GetPositionZ() > me->GetPositionZ() + 10.0f)
                            m_FlyingRune = l_Rune->GetGUID();
                        else
                            m_FloorRune = l_Rune->GetGUID();
                    }

                    if (m_RitualistGuids.empty() && m_Init)
                    {
                        me->SetReactState(ReactStates::REACT_AGGRESSIVE);
                        me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_PC | UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE);

                        me->SetAIAnimKitId(0);
                    }
                    else
                    {
                        me->SetReactState(ReactStates::REACT_PASSIVE);
                        me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_PC | UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE);

                        me->SetAIAnimKitId(eAnimKit::AnimWaiting);
                    }
                });

                AddTimedDelayedOperation(2 * TimeConstants::IN_MILLISECONDS, [this]() -> void
                {
                    if (me->GetReactState() == ReactStates::REACT_AGGRESSIVE)
                        return;

                    if (Creature* l_Flying = ObjectAccessor::GetCreature(*me, m_FlyingRune))
                    {
                        l_Flying->CastSpell(me, eSpells::RuneChargingPermanent, true);

                        l_Flying->SetReactState(ReactStates::REACT_PASSIVE);
                        l_Flying->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_PC | UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE);
                    }
                });

                /// One player may enter the rune with Ko'ragh, absorbing a portion of its energy.
                /// In Mythic difficulty, two players may enter the rune per phase of Charging.
                /// In Raid Finder difficulty, five players may enter the rune per phase of Charging.
                m_RunicPlayers = IsMythic() ? 2 : (IsLFR() ? 5 : 1);
                m_RunicPlayersCount = 0;

                m_SuppressionFieldTarget = ObjectGuid::Empty;

                m_overflowingOrbsCount = 1;

                m_Charging = false;
            }

            void JustReachedHome() override
            {
                if (m_RitualistGuids.empty() && m_Init)
                {
                    me->SetReactState(ReactStates::REACT_AGGRESSIVE);
                    me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_PC | UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE);

                    me->SetAIAnimKitId(0);
                }
                else
                {
                    me->SetReactState(ReactStates::REACT_PASSIVE);
                    me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_PC | UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE);

                    me->SetAIAnimKitId(eAnimKit::AnimWaiting);
                }

                m_Events.Reset();

                summons.DespawnAll();
                me->DespawnCreaturesInArea(eHighmaulCreatures::VolatileAnomaly, 400);
            }

            void SetGUID(ObjectGuid p_Guid, int32 p_ID) override
            {
                m_RitualistGuids.erase(p_Guid);

                if (m_RitualistGuids.empty())
                {
                    if (Creature* l_Flying = ObjectAccessor::GetCreature(*me, m_FlyingRune))
                        l_Flying->RemoveAura(eSpells::RuneChargingPermanent);

                    me->SetReactState(ReactStates::REACT_AGGRESSIVE);
                    me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_PC | UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE);

                    me->SetAIAnimKitId(0);

                    Talk(eTalks::Intro);
                }
            }

            bool CanRespawn() override
            {
                return false;
            }

            void DoAction(int32 const p_Action) override
            {
                switch (p_Action)
                {
                    case eActions::CancelBreakersStrength:
                    {
                        m_CosmeticEvents.CancelEvent(eCosmeticEvents::EventBreakersStrength);
                        me->RemoveAura(eSpells::BreakersStrength);

                        /// When the Ko'ragh's Nullification Barrier is removed, he begins to recharge. After 20 sec, the barrier is restored.
                        Talk(eTalks::BarrierShattered);
                        m_Charging = true;

                        me->SetReactState(ReactStates::REACT_PASSIVE);
                        //me->ClearAllUnitState();

                        me->GetMotionMaster()->Clear();
                        me->GetMotionMaster()->MovePoint(eMove::MoveToCenter, g_CenterPos);
                        m_overflowingOrbsCount++;

                        m_CosmeticEvents.ScheduleEvent(eCosmeticEvents::EventEndOfCharging, 15 * TimeConstants::IN_MILLISECONDS);
                        break;
                    }
                    case eActions::ActionSuppressionField:
                    {
                        if (Unit* l_Target = ObjectAccessor::GetUnit(*me, m_SuppressionFieldTarget))
                        {
                            if (me->GetDistance(l_Target) > 5.0f)
                            {
                                float l_Orientation = me->GetAngle(l_Target);
                                float l_Radius = me->GetDistance(l_Target) - 4.0f;
                                float l_X = me->GetPositionX() + (l_Radius * cos(l_Orientation));
                                float l_Y = me->GetPositionY() + (l_Radius * sin(l_Orientation));

                                me->GetMotionMaster()->Clear();
                                me->GetMotionMaster()->MoveCharge(l_X, l_Y, l_Target->GetPositionZ(), 10.0f, eSpells::SuppressionFieldAura);
                                m_SuppressionFieldTarget = ObjectGuid::Empty;
                            }
                            else
                            {
                                if (Unit* l_Target = ObjectAccessor::GetUnit(*me, m_SuppressionFieldTarget))
                                    me->SetFacingTo(me->GetAngle(l_Target));

                                me->RemoveAura(eSpells::SuppressionFieldAura);
                                me->CastSpell(me, eSpells::SuppressionFieldMissile, true);

                                AddTimedDelayedOperation(2 * TimeConstants::IN_MILLISECONDS, [this]() -> void
                                {
                                    if (Unit* l_Target = SelectTarget(SelectAggroTarget::SELECT_TARGET_TOPAGGRO))
                                    {
                                        me->GetMotionMaster()->Clear();
                                        me->GetMotionMaster()->MoveChase(l_Target);
                                    }
                                });

                                m_SuppressionFieldTarget = ObjectGuid::Empty;
                            }
                        }

                        break;
                    }
                    default:
                        break;
                }
            }

            void MovementInform(uint32 p_Type, uint32 p_ID) override
            {
                switch (p_ID)
                {
                    case eMove::MoveToCenter:
                    {
                        if (p_Type != MovementGeneratorType::POINT_MOTION_TYPE)
                            break;

                        if (Creature* l_Flying = ObjectAccessor::GetCreature(*me, m_FlyingRune))
                            l_Flying->CastSpell(l_Flying, eSpells::RuneChargingTemporary, true);

                        if (Creature* l_Grounding = ObjectAccessor::GetCreature(*me, m_FloorRune))
                        {
                            l_Grounding->CastSpell(l_Grounding, eSpells::CausticEnergyAreaTrigger, true, nullptr, nullptr, me->GetGUID());
                            l_Grounding->CastSpell(l_Grounding, eSpells::VolatileAnomaliesAura, true, nullptr, nullptr, me->GetGUID());
                        }

                        me->SetAIAnimKitId(eAnimKit::AnimWaiting);
                        me->AddUnitState(UnitState::UNIT_STATE_STUNNED);

                        me->CastSpell(me, eSpells::KnockbackForRecharge, true);
                        me->CastSpell(me, eSpells::VulnerabilityAura, true);

                        me->SetFlag(UNIT_FIELD_FLAGS_2, UNIT_FLAG2_DISABLE_TURN);

                        m_Events.DelayEvent(eEvents::EventOverflowingEnergy, 20 * TimeConstants::IN_MILLISECONDS);

                        m_CosmeticEvents.CancelEvent(eCosmeticEvents::EventEndOfCharging);
                        m_CosmeticEvents.ScheduleEvent(eCosmeticEvents::EventEndOfCharging, 20 * TimeConstants::IN_MILLISECONDS);
                        break;
                    }
                    case eSpells::SuppressionFieldAura:
                    {
                        if (Unit* l_Target = ObjectAccessor::GetUnit(*me, m_SuppressionFieldTarget))
                            me->SetFacingTo(me->GetAngle(l_Target));

                        me->CastSpell(me, eSpells::SuppressionFieldMissile, true);

                        AddTimedDelayedOperation(2 * TimeConstants::IN_MILLISECONDS, [this]() -> void
                        {
                            if (Unit* l_Target = SelectTarget(SelectAggroTarget::SELECT_TARGET_TOPAGGRO))
                            {
                                me->GetMotionMaster()->Clear();
                                me->GetMotionMaster()->MoveChase(l_Target);
                            }
                        });

                        break;
                    }
                    default:
                        break;
                }
            }

            void EnterCombat(Unit* p_Attacker) override
            {
                m_overflowingOrbsCount = 1;

                Talk(eTalks::Aggro);

                if (m_Instance != nullptr)
                    m_Instance->SendEncounterUnit(EncounterFrameType::ENCOUNTER_FRAME_ENGAGE, me, 1);

                if (Creature* l_Flying = ObjectAccessor::GetCreature(*me, m_FlyingRune))
                    l_Flying->CastSpell(me, eSpells::NullificationRuneEmpowered, true);

                me->CastSpell(me, eSpells::NullificationBarrierPower, true);
                me->CastSpell(me, eSpells::NullificationBarrierAbsorb, true);

                me->SetPower(Powers::POWER_ALTERNATE_POWER, 100);

                /// While Ko'ragh maintains a Nullification Barrier, his damage done is increased by 6% every 10 seconds.
                /// This effect stacks. When the barrier expires, Breaker's Strength is removed.
                m_CosmeticEvents.ScheduleEvent(eCosmeticEvents::EventBreakersStrength, 10 * TimeConstants::IN_MILLISECONDS);

                m_Events.ScheduleEvent(eEvents::EventExpelMagicFire, 6 * TimeConstants::IN_MILLISECONDS);
                m_Events.ScheduleEvent(eEvents::EventExpelMagicArcane, 30 * TimeConstants::IN_MILLISECONDS);
                m_Events.ScheduleEvent(eEvents::EventExpelMagicFrost, 40 * TimeConstants::IN_MILLISECONDS);
                m_Events.ScheduleEvent(eEvents::EventExpelMagicShadow, 55 * TimeConstants::IN_MILLISECONDS);
                m_Events.ScheduleEvent(eEvents::EventSuppressionField, 15 * TimeConstants::IN_MILLISECONDS);
                m_Events.ScheduleEvent(eEvents::EventOverflowingEnergy, (IsMythic() ? 21 : 36) * TimeConstants::IN_MILLISECONDS);
                m_Events.ScheduleEvent(eEvents::EventBerserk, 570 * TimeConstants::IN_MILLISECONDS);

                if (IsMythic())
                    m_Events.ScheduleEvent(eEvents::EventExpelMagicFel, 12 * TimeConstants::IN_MILLISECONDS);

                _EnterCombat();
            }

            void KilledUnit(Unit* p_Killed) override
            {
                if (p_Killed->GetTypeId() == TypeID::TYPEID_PLAYER)
                    Talk(eTalks::Slay);
            }

            void JustDied(Unit* p_Killer) override
            {
                _JustDied();

                if (m_Instance != nullptr)
                {
                    m_Instance->SendEncounterUnit(EncounterFrameType::ENCOUNTER_FRAME_DISENGAGE, me);

                    m_Instance->DoRemoveAurasDueToSpellOnPlayers(eSpells::NullificationBarrierPower);
                    m_Instance->DoRemoveAurasDueToSpellOnPlayers(eSpells::NullificationBarrierAbsorb2);
                    m_Instance->DoRemoveAurasDueToSpellOnPlayers(eSpells::MarkOfNullification);
                    m_Instance->DoRemoveAurasDueToSpellOnPlayers(eSpells::ExpelMagicFireDoT);
                    m_Instance->DoRemoveAurasDueToSpellOnPlayers(eSpells::ExpelMagicArcaneAura);
                    m_Instance->DoRemoveAurasDueToSpellOnPlayers(eSpells::ExpelMagicShadow);
                    m_Instance->DoRemoveAurasDueToSpellOnPlayers(eSpells::SuppressionFieldDoT);
                    m_Instance->DoRemoveAurasDueToSpellOnPlayers(eSpells::SuppressionFieldSilence);
                    m_Instance->DoRemoveAurasDueToSpellOnPlayers(eSpells::ExpelMagicFrostAura);

                    me->RemoveAllAreaTriggers();

                    me->DespawnCreaturesInArea(eHighmaulCreatures::VolatileAnomaly, 400);
                    summons.DespawnAll();

                    CastSpellToPlayers(me->GetMap(), me, eSpells::KoraghBonus, true);

                    if (IsLFR())
                    {
                        Map::PlayerList const& l_PlayerList = m_Instance->instance->GetPlayers();
                        if (l_PlayerList.isEmpty())
                            return;

                        /*for (Map::PlayerList::const_iterator l_Itr = l_PlayerList.begin(); l_Itr != l_PlayerList.end(); ++l_Itr)
                        {
                            if (Player* l_Player = l_Itr->GetSource())
                            {
                                uint32 l_DungeonID = l_Player->GetGroup() ? sLFGMgr->GetDungeon(l_Player->GetGroup()->GetGUID()) : 0;
                                if (!me || l_Player->IsAtGroupRewardDistance(me))
                                    sLFGMgr->RewardDungeonDoneFor(l_DungeonID, l_Player);
                            }
                        }

                        Player* l_Player = me->GetMap()->GetPlayers().begin()->GetSource();
                        if (l_Player && l_Player->GetGroup())
                            sLFGMgr->AutomaticLootAssignation(me, l_Player->GetGroup());*/
                    }
                }

                Talk(eTalks::Death);
            }

            void EnterEvadeMode(EvadeReason /*why*/) override
            {
                CreatureAI::EnterEvadeMode();

                m_overflowingOrbsCount = 1;

                if (m_Instance != nullptr)
                {
                    m_Instance->SetBossState(eHighmaulDatas::BossKoragh, EncounterState::FAIL);

                    m_Instance->SendEncounterUnit(EncounterFrameType::ENCOUNTER_FRAME_DISENGAGE, me);

                    me->RemoveAurasDueToSpell(eHighmaulSpells::Berserker);

                    m_Instance->DoRemoveAurasDueToSpellOnPlayers(eSpells::NullificationBarrierPower);
                    m_Instance->DoRemoveAurasDueToSpellOnPlayers(eSpells::NullificationBarrierAbsorb2);
                    m_Instance->DoRemoveAurasDueToSpellOnPlayers(eSpells::MarkOfNullification);
                    m_Instance->DoRemoveAurasDueToSpellOnPlayers(eSpells::ExpelMagicFireDoT);
                    m_Instance->DoRemoveAurasDueToSpellOnPlayers(eSpells::ExpelMagicArcaneAura);
                    m_Instance->DoRemoveAurasDueToSpellOnPlayers(eSpells::ExpelMagicShadow);
                    m_Instance->DoRemoveAurasDueToSpellOnPlayers(eSpells::SuppressionFieldDoT);
                    m_Instance->DoRemoveAurasDueToSpellOnPlayers(eSpells::SuppressionFieldSilence);
                    m_Instance->DoRemoveAurasDueToSpellOnPlayers(eSpells::ExpelMagicFrostAura);
                }

                summons.DespawnAll();
            }

            uint32 GetData(uint32 p_ID) const override
            {
                switch (p_ID)
                {
                    case eDatas::DataMaxRunicPlayers:
                        return m_RunicPlayers;
                    case eDatas::DataRunicPlayersCount:
                        return m_RunicPlayersCount;
                    default:
                        break;
                }

                return 0;
            }

            void SetData(uint32 p_ID, uint32 p_Value) override
            {
                switch (p_ID)
                {
                    case eDatas::DataRunicPlayersCount:
                    {
                        m_RunicPlayersCount = p_Value;

                        if (m_Instance != nullptr)
                            m_Instance->SetData(eHighmaulDatas::KoraghNullificationBarrier, 1);

                        break;
                    }
                    default:
                        break;
                }
            }

            void SpellHitTarget(Unit* p_Target, SpellInfo const* p_SpellInfo) override
            {
                if (p_Target == nullptr)
                    return;

                switch (p_SpellInfo->Id)
                {
                    case eSpells::ExpelMagicArcaneAura:
                    {
                        Talk(eTalks::ExpelMagicArcaneWarn, p_Target);
                        break;
                    }
                    default:
                        break;
                }
            }

            void UpdateAI(uint32 const p_Diff) override
            {
                UpdateOperations(p_Diff);

                m_CosmeticEvents.Update(p_Diff);

                switch (m_CosmeticEvents.ExecuteEvent())
                {
                    case eCosmeticEvents::EventBreakersStrength:
                    {
                        me->CastSpell(me, eSpells::BreakersStrength, true);
                        m_CosmeticEvents.ScheduleEvent(eCosmeticEvents::EventBreakersStrength, 10 * TimeConstants::IN_MILLISECONDS);
                        break;
                    }
                    case eCosmeticEvents::EventEndOfCharging:
                    {
                        me->SetAIAnimKitId(0);
                        me->ClearUnitState(UnitState::UNIT_STATE_STUNNED);

                        m_Charging = false;

                        me->SetPower(Powers::POWER_ALTERNATE_POWER, 100);
                        me->CastSpell(me, eSpells::NullificationBarrierAbsorb, true);

                        me->RemoveAura(eSpells::VulnerabilityAura);
                        me->SetReactState(ReactStates::REACT_AGGRESSIVE);

                        me->RemoveFlag(UNIT_FIELD_FLAGS_2, UNIT_FLAG2_DISABLE_TURN);

                        me->GetMotionMaster()->Clear();

                        if (Unit* l_Target = SelectTarget(SelectAggroTarget::SELECT_TARGET_TOPAGGRO))
                            me->GetMotionMaster()->MoveChase(l_Target);

                        if (Creature* l_Grounding = ObjectAccessor::GetCreature(*me, m_FloorRune))
                            l_Grounding->RemoveAura(eSpells::VolatileAnomaliesAura);

                        m_RunicPlayersCount = 0;
                        break;
                    }
                    default:
                        break;
                }

                if (!UpdateVictim() || m_Charging)
                    return;

                m_Events.Update(p_Diff);

                if (me->HasUnitState(UnitState::UNIT_STATE_CASTING))
                    return;

                switch (m_Events.ExecuteEvent())
                {
                    case eEvents::EventExpelMagicFire:
                    {
                        me->CastSpell(me, eSpells::ExpelMagicFireDoT, false);
                        Talk(eTalks::ExpelMagic);
                        m_Events.ScheduleEvent(eEvents::EventExpelMagicFire, 60 * TimeConstants::IN_MILLISECONDS);
                        break;
                    }
                    case eEvents::EventExpelMagicArcane:
                    {
                        if (Unit* l_Target = SelectTarget(SelectAggroTarget::SELECT_TARGET_TOPAGGRO))
                            me->CastSpell(l_Target, eSpells::ExpelMagicArcaneAura, false);
                        Talk(eTalks::ExpelMagic);
                        m_Events.ScheduleEvent(eEvents::EventExpelMagicArcane, 30 * TimeConstants::IN_MILLISECONDS);
                        break;
                    }
                    case eEvents::EventExpelMagicFrost:
                    {
                        if (Unit* l_Target = SelectTarget(SelectAggroTarget::SELECT_TARGET_RANDOM, 0, -10.0f))
                        {
                            me->CastSpell(l_Target, eSpells::ExpelMagicFrostAreaTrigger, false);

                            AddTimedDelayedOperation(1500, [this]() -> void
                            {
                                me->CastSpell(me, eSpells::ExpelMagicFrostDamage, true);
                            });
                        }

                        Talk(eTalks::ExpelMagic);
                        m_Events.ScheduleEvent(eEvents::EventExpelMagicFrost, 60 * TimeConstants::IN_MILLISECONDS);
                        break;
                    }
                    case eEvents::EventExpelMagicShadow:
                    {
                        me->CastSpell(me, eSpells::ExpelMagicShadow, false);
                        Talk(eTalks::ExpelMagic);
                        m_Events.ScheduleEvent(eEvents::EventExpelMagicShadow, 60 * TimeConstants::IN_MILLISECONDS);
                        break;
                    }
                    case eEvents::EventSuppressionField:
                    {
                        Unit* victim;

                        victim = SelectTarget(SELECT_TARGET_FARTHEST, 0, 50.f, true);
                        if (victim == nullptr)
                            victim = SelectTarget(SELECT_TARGET_RANDOM, 0, 50.f, true);

                        if (victim)
                        {
                            m_SuppressionFieldTarget = victim->GetGUID();
                            Position pos = victim->GetPosition();
                            me->GetMotionMaster()->MoveCharge(&pos, 60.0f, SuppressionFieldAura);
                        }

                        Talk(eTalks::SuppressionField);
                        m_Events.ScheduleEvent(eEvents::EventSuppressionField, 17 * TimeConstants::IN_MILLISECONDS);
                        break;
                    }
                    case eEvents::EventOverflowingEnergy:
                    {
                        for(uint32 i = 0; i < m_overflowingOrbsCount; i++)
                            me->CastSpell(me, eSpells::OverflowingEnergySpawn, true);

                        m_Events.ScheduleEvent(eEvents::EventOverflowingEnergy, (IsMythic() ? 15 : 30 )* TimeConstants::IN_MILLISECONDS);
                        break;
                    }
                    case eEvents::EventBerserk:
                    {
                        Talk(eTalks::Berserk);
                        me->CastSpell(me, eHighmaulSpells::Berserker, true);
                        break;
                    }
                    case EventExpelMagicFel:
                    {
                        me->CastSpell(me, eSpells::ExpelMagicFelDummy, true);
                        m_Events.ScheduleEvent(eEvents::EventExpelMagicFel, 15.5 * TimeConstants::IN_MILLISECONDS);
                        break;
                    }
                    default:
                        break;
                }

                DoMeleeAttackIfReady();
            }
        };

        CreatureAI* GetAI(Creature* p_Creature) const override
        {
            return new boss_koraghAI(p_Creature);
        }
};

/// Breaker Ritualist <Breaker of Fel> - 86330
class npc_highmaul_breaker_of_fel : public CreatureScript
{
    public:
        npc_highmaul_breaker_of_fel() : CreatureScript("npc_highmaul_breaker_of_fel") { }

        enum eSpells
        {
            /// Cosmetic
            FelBreakerFelChannel    = 172664,

            /// Damaging Spells
            FelBlast                = 174422,
            FelNova                 = 174403
        };

        enum eEvent
        {
            EventFelNova = 1
        };

        struct npc_highmaul_breaker_of_felAI : public ScriptedAI
        {
            npc_highmaul_breaker_of_felAI(Creature* p_Creature) : ScriptedAI(p_Creature)
            {
                m_Instance = p_Creature->GetInstanceScript();
            }

            EventMap m_Events;
            InstanceScript* m_Instance;

            void Reset() override
            {
                m_Events.Reset();

                me->CastSpell(me, eSpells::FelBreakerFelChannel, false);
            }

            void EnterCombat(Unit* p_Attacker) override
            {
                m_Events.ScheduleEvent(eEvent::EventFelNova, 12 * TimeConstants::IN_MILLISECONDS);
            }

            void JustDied(Unit* p_Killer)
            {
                if (m_Instance == nullptr)
                    return;

                if (Creature* l_Boss = ObjectAccessor::GetCreature(*me, m_Instance->GetGuidData(eHighmaulCreatures::Koragh)))
                {
                    if (l_Boss->IsAIEnabled)
                        l_Boss->AI()->SetGUID(me->GetGUID());
                }
            }

            void UpdateAI(uint32 const p_Diff) override
            {
                if (!UpdateVictim())
                    return;

                m_Events.Update(p_Diff);

                if (me->HasUnitState(UnitState::UNIT_STATE_CASTING))
                    return;

                switch (m_Events.ExecuteEvent())
                {
                    case eEvent::EventFelNova:
                        me->CastSpell(me, eSpells::FelNova, false);
                        m_Events.ScheduleEvent(eEvent::EventFelNova, 25 * TimeConstants::IN_MILLISECONDS);
                        break;
                    default:
                        break;
                }

                DoSpellAttackIfReady(eSpells::FelBlast);
            }
        };

        CreatureAI* GetAI(Creature* p_Creature) const override
        {
            return new npc_highmaul_breaker_of_felAI(p_Creature);
        }
};

/// Breaker Ritualist <Breaker of Fire> - 86329
class npc_highmaul_breaker_of_fire : public CreatureScript
{
    public:
        npc_highmaul_breaker_of_fire() : CreatureScript("npc_highmaul_breaker_of_fire") { }

        enum eSpells
        {
            /// Cosmetic
            FelBreakerFireChannel   = 172455,

            /// Damaging Spells
            WildFlamesSearcher      = 173763,
            WildFlamesMissile       = 173618
        };

        enum eEvent
        {
            EventWildFlames = 1
        };

        struct npc_highmaul_breaker_of_fireAI : public ScriptedAI
        {
            npc_highmaul_breaker_of_fireAI(Creature* p_Creature) : ScriptedAI(p_Creature)
            {
                m_Instance = p_Creature->GetInstanceScript();
            }

            EventMap m_Events;
            InstanceScript* m_Instance;

            void Reset() override
            {
                m_Events.Reset();

                me->CastSpell(me, eSpells::FelBreakerFireChannel, false);
            }

            void EnterCombat(Unit* p_Attacker) override
            {
                m_Events.ScheduleEvent(eEvent::EventWildFlames, 4 * TimeConstants::IN_MILLISECONDS);
            }

            void EnterEvadeMode(EvadeReason /*why*/) override
            {
                CreatureAI::EnterEvadeMode();

                summons.DespawnAll();
            }

            void SpellHitTarget(Unit* p_Target, SpellInfo const* p_SpellInfo) override
            {
                if (p_Target == nullptr)
                    return;

                switch (p_SpellInfo->Id)
                {
                    case eSpells::WildFlamesSearcher:
                        me->CastSpell(p_Target, eSpells::WildFlamesMissile, true);
                        break;
                    default:
                        break;
                }
            }

            void JustDied(Unit* p_Killer)
            {
                if (m_Instance == nullptr)
                    return;

                if (Creature* l_Boss = ObjectAccessor::GetCreature(*me, m_Instance->GetGuidData(eHighmaulCreatures::Koragh)))
                {
                    if (l_Boss->IsAIEnabled)
                        l_Boss->AI()->SetGUID(me->GetGUID());
                }
            }

            void UpdateAI(uint32 const p_Diff) override
            {
                if (!UpdateVictim())
                    return;

                m_Events.Update(p_Diff);

                if (me->HasUnitState(UnitState::UNIT_STATE_CASTING))
                    return;

                switch (m_Events.ExecuteEvent())
                {
                    case eEvent::EventWildFlames:
                        me->CastSpell(me, eSpells::WildFlamesSearcher, false);
                        m_Events.ScheduleEvent(eEvent::EventWildFlames, 11 * TimeConstants::IN_MILLISECONDS);
                        break;
                    default:
                        break;
                }

                DoMeleeAttackIfReady();
            }
        };

        CreatureAI* GetAI(Creature* p_Creature) const override
        {
            return new npc_highmaul_breaker_of_fireAI(p_Creature);
        }
};

/// Wild Flames - 86875
class npc_highmaul_wild_flames : public CreatureScript
{
    public:
        npc_highmaul_wild_flames() : CreatureScript("npc_highmaul_wild_flames") { }

        enum eSpells
        {
            WildFlamesAreaTrigger   = 173616,
            WildFlamesSearcher      = 173799,
            WildFlamesMissile       = 173823
        };

        struct npc_highmaul_wild_flamesAI : public ScriptedAI
        {
            npc_highmaul_wild_flamesAI(Creature* p_Creature) : ScriptedAI(p_Creature) { }

            void Reset() override
            {
                me->SetReactState(ReactStates::REACT_PASSIVE);
                me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_PC | UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE);

                me->CastSpell(me, eSpells::WildFlamesAreaTrigger, true);

                SetCanSeeEvenInPassiveMode(true);
            }

            void JustDied(Unit* /*killer*/) override
            {
                me->RemoveAllAreaTriggers();
            }

            void SpellHitTarget(Unit* p_Target, SpellInfo const* p_SpellInfo) override
            {
                if (p_Target == nullptr)
                    return;

                switch (p_SpellInfo->Id)
                {
                    case eSpells::WildFlamesSearcher:
                        me->CastSpell(p_Target, eSpells::WildFlamesMissile, true);
                        break;
                    default:
                        break;
                }
            }
        };

        CreatureAI* GetAI(Creature* p_Creature) const override
        {
            return new npc_highmaul_wild_flamesAI(p_Creature);
        }
};

/// Breaker Ritualist <Breaker of Frost> - 86326
class npc_highmaul_breaker_of_frost : public CreatureScript
{
    public:
        npc_highmaul_breaker_of_frost() : CreatureScript("npc_highmaul_breaker_of_frost") { }

        enum eSpells
        {
            /// Cosmetic
            FelBreakerFrostChannel  = 172448,

            /// Damaging Spells
            FrozenCore              = 174404
        };

        enum eEvent
        {
            EventFrozenCore = 1
        };

        struct npc_highmaul_breaker_of_frostAI : public ScriptedAI
        {
            npc_highmaul_breaker_of_frostAI(Creature* p_Creature) : ScriptedAI(p_Creature)
            {
                m_Instance = p_Creature->GetInstanceScript();
            }

            EventMap m_Events;
            InstanceScript* m_Instance;

            void Reset() override
            {
                m_Events.Reset();

                me->CastSpell(me, eSpells::FelBreakerFrostChannel, false);
            }

            void EnterCombat(Unit* p_Attacker) override
            {
                m_Events.ScheduleEvent(eEvent::EventFrozenCore, 4 * TimeConstants::IN_MILLISECONDS);
            }

            void JustDied(Unit* p_Killer)
            {
                if (m_Instance == nullptr)
                    return;

                if (Creature* l_Boss = ObjectAccessor::GetCreature(*me, m_Instance->GetGuidData(eHighmaulCreatures::Koragh)))
                {
                    if (l_Boss->IsAIEnabled)
                        l_Boss->AI()->SetGUID(me->GetGUID());
                }
            }

            void UpdateAI(uint32 const p_Diff) override
            {
                if (!UpdateVictim())
                    return;

                m_Events.Update(p_Diff);

                if (me->HasUnitState(UnitState::UNIT_STATE_CASTING))
                    return;

                switch (m_Events.ExecuteEvent())
                {
                    case eEvent::EventFrozenCore:
                        me->CastSpell(me, eSpells::FrozenCore, false);
                        m_Events.ScheduleEvent(eEvent::EventFrozenCore, 13 * TimeConstants::IN_MILLISECONDS);
                        break;
                    default:
                        break;
                }

                DoMeleeAttackIfReady();
            }
        };

        CreatureAI* GetAI(Creature* p_Creature) const override
        {
            return new npc_highmaul_breaker_of_frostAI(p_Creature);
        }
};

/// Volatile Anomaly - 79956
class npc_highmaul_koragh_volatile_anomaly : public CreatureScript
{
    public:
        npc_highmaul_koragh_volatile_anomaly() : CreatureScript("npc_highmaul_koragh_volatile_anomaly") { }

        enum eSpells
        {
            AlphaFadeOut            = 141608,
            Destabilize             = 163466,
            SuppressionFieldSilence = 162595
        };

        struct npc_highmaul_koragh_volatile_anomalyAI : public ScriptedAI
        {
            npc_highmaul_koragh_volatile_anomalyAI(Creature* p_Creature) : ScriptedAI(p_Creature)
            {
                m_Exploded = false;
            }

            bool m_Exploded;

            void Reset() override
            {
                me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_NPC | UNIT_FLAG_IMMUNE_TO_PC | UNIT_FLAG_NOT_SELECTABLE);

                if (Player* l_Target = me->SelectNearestPlayer(30.0f))
                    AttackStart(l_Target);
            }

            void DamageTaken(Unit* p_Attacker, uint32& p_Damage) override
            {
                if (m_Exploded)
                    return;

                if (p_Damage > me->GetHealth() && !me->HasAura(eSpells::SuppressionFieldSilence))
                {
                    m_Exploded = true;
                    me->CastSpell(me, eSpells::AlphaFadeOut, true);
                    me->CastSpell(me, eSpells::Destabilize, true);
                }
            }
        };

        CreatureAI* GetAI(Creature* p_Creature) const override
        {
            return new npc_highmaul_koragh_volatile_anomalyAI(p_Creature);
        }
};

/// Chain - 233127
class go_highmaul_chain : public GameObjectScript
{
    public:
        go_highmaul_chain() : GameObjectScript("go_highmaul_chain") { }

        struct go_highmaul_chainAI : public GameObjectAI
        {
            go_highmaul_chainAI(GameObject* p_GameObject) : GameObjectAI(p_GameObject) { }

            enum eAnimID
            {
                AnimKit = 947
            };

            void Reset() override
            {
                go->SetAnimKitId(eAnimID::AnimKit, false);
            }
        };

        GameObjectAI* GetAI(GameObject* p_GameObject) const override
        {
            return new go_highmaul_chainAI(p_GameObject);
        }
};

/// Frozen Core - 174404
class spell_highmaul_frozen_core : public SpellScriptLoader
{
    public:
        spell_highmaul_frozen_core() : SpellScriptLoader("spell_highmaul_frozen_core") { }

        class spell_highmaul_frozen_core_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_highmaul_frozen_core_SpellScript);

            void CorrectTargets(std::list<WorldObject*>& p_Targets)
            {
                if (p_Targets.empty())
                    return;

                if (Unit* l_Caster = GetCaster())
                    p_Targets.remove_if(Trinity::UnitAuraCheck(true, GetSpellInfo()->Id, l_Caster->GetGUID()));
            }

            void Register() override
            {
                OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_highmaul_frozen_core_SpellScript::CorrectTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENEMY);
                OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_highmaul_frozen_core_SpellScript::CorrectTargets, EFFECT_1, TARGET_UNIT_SRC_AREA_ENEMY);
                OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_highmaul_frozen_core_SpellScript::CorrectTargets, EFFECT_2, TARGET_UNIT_SRC_AREA_ENEMY);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_highmaul_frozen_core_SpellScript();
        }

        class spell_highmaul_frozen_core_AuraScript : public AuraScript
        {
            PrepareAuraScript(spell_highmaul_frozen_core_AuraScript);

            enum eSpell
            {
                FrozenCoreAura = 174405
            };

            uint32 m_DamageTimer;

            bool Load()
            {
                m_DamageTimer = 200;
                return true;
            }

            void OnUpdate(uint32 p_Diff)
            {
                if (m_DamageTimer)
                {
                    if (m_DamageTimer <= p_Diff)
                    {
                        m_DamageTimer = 200;

                        if (Unit* l_Target = GetUnitOwner())
                        {
                            std::list<Unit*> l_TargetList;
                            Trinity::AnyFriendlyUnitInObjectRangeCheck l_Check(l_Target, l_Target, 8.0f);
                            Trinity::UnitListSearcher<Trinity::AnyFriendlyUnitInObjectRangeCheck> l_Searcher(l_Target, l_TargetList, l_Check);
                            Cell::VisitGridObjects(l_Target, l_Searcher, 8.0f);

                            for (Unit* l_Unit : l_TargetList)
                                l_Target->CastSpell(l_Unit, eSpell::FrozenCoreAura, true);
                        }
                    }
                    else
                        m_DamageTimer -= p_Diff;
                }
            }

            void Register() override
            {
                OnAuraUpdate += AuraUpdateFn(spell_highmaul_frozen_core_AuraScript::OnUpdate);
            }
        };

        AuraScript* GetAuraScript() const override
        {
            return new spell_highmaul_frozen_core_AuraScript();
        }
};

/// Wild Flames (AreaTrigger) - 173616
class spell_highmaul_wild_flames_areatrigger : public SpellScriptLoader
{
    public:
        spell_highmaul_wild_flames_areatrigger() : SpellScriptLoader("spell_highmaul_wild_flames_areatrigger") { }

        class spell_highmaul_wild_flames_areatrigger_AuraScript : public AuraScript
        {
            PrepareAuraScript(spell_highmaul_wild_flames_areatrigger_AuraScript);

            enum eSpells
            {
                WildFlamesSearcher  = 173799,
                WildFlamesDoT       = 173827
            };

            uint32 m_DamageTimer;

            bool Load()
            {
                m_DamageTimer = 500;
                return true;
            }

            void OnUpdate(uint32 p_Diff)
            {
                if (m_DamageTimer)
                {
                    if (m_DamageTimer <= p_Diff)
                    {
                        if (Unit* l_Target = GetUnitOwner())
                        {
                            std::list<Unit*> l_TargetList;
                            float l_Radius = 6.0f;

                            Trinity::AnyUnfriendlyUnitInObjectRangeCheck l_Check(l_Target, l_Target, l_Radius);
                            Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> l_Searcher(l_Target, l_TargetList, l_Check);
                            Cell::VisitGridObjects(l_Target, l_Searcher, l_Radius);

                            for (Unit* l_Iter : l_TargetList)
                            {
                                if (l_Iter->GetDistance(l_Target) <= 3.0f)
                                    l_Iter->CastSpell(l_Iter, eSpells::WildFlamesDoT, true);
                                else
                                    l_Iter->RemoveAura(eSpells::WildFlamesDoT);
                            }
                        }

                        m_DamageTimer = 500;
                    }
                    else
                        m_DamageTimer -= p_Diff;
                }
            }

            void OnRemove(AuraEffect const* /*p_AurEff*/, AuraEffectHandleModes /*p_Mode*/)
            {
                if (Unit* l_Caster = GetCaster())
                    l_Caster->CastSpell(l_Caster, eSpells::WildFlamesSearcher, true);
            }

            void Register() override
            {
                OnAuraUpdate += AuraUpdateFn(spell_highmaul_wild_flames_areatrigger_AuraScript::OnUpdate);
                AfterEffectRemove += AuraEffectRemoveFn(spell_highmaul_wild_flames_areatrigger_AuraScript::OnRemove, EFFECT_0, SPELL_AURA_AREA_TRIGGER, AURA_EFFECT_HANDLE_REAL);
            }
        };

        AuraScript* GetAuraScript() const override
        {
            return new spell_highmaul_wild_flames_areatrigger_AuraScript();
        }
};

/// Nullification Barrier - 156803
class spell_highmaul_nullification_barrier : public SpellScriptLoader
{
    public:
        spell_highmaul_nullification_barrier() : SpellScriptLoader("spell_highmaul_nullification_barrier") { }

        class spell_highmaul_nullification_barrier_AuraScript : public AuraScript
        {
            PrepareAuraScript(spell_highmaul_nullification_barrier_AuraScript);

            enum eSpell
            {
                BreakersStrength = 162569
            };

            enum eAction
            {
                CancelBreakersStrength
            };

            int32 m_AbsorbAmount;

            bool Load()
            {
                m_AbsorbAmount = 0;
                return true;
            }

            void AfterApply(AuraEffect const* p_AurEff, AuraEffectHandleModes p_Mode)
            {
                m_AbsorbAmount = p_AurEff->GetAmount();
            }

            void OnTick(AuraEffect const* /*p_AurEff*/)
            {
                if (m_AbsorbAmount <= 0)
                    return;

                if (Unit* l_Target = GetTarget())
                {
                    if (AuraEffect* l_AbsorbAura = l_Target->GetAuraEffect(GetSpellInfo()->Id, EFFECT_0, l_Target->GetGUID()))
                    {
                        int32 l_Pct = ((float)l_AbsorbAura->GetAmount() / (float)m_AbsorbAmount) * 100.0f;
                        l_Target->SetPower(Powers::POWER_ALTERNATE_POWER, l_Pct);
                    }
                }
            }

            void OnRemove(AuraEffect const* p_AurEff, AuraEffectHandleModes p_Mode)
            {
                if (GetUnitOwner() == nullptr)
                    return;

                if (Creature* l_Boss = GetUnitOwner()->ToCreature())
                {
                    l_Boss->SetPower(Powers::POWER_ALTERNATE_POWER, 0);

                    AuraRemoveMode l_RemoveMode = GetTargetApplication()->GetRemoveMode();
                    if (l_Boss->IsAIEnabled && l_RemoveMode != AuraRemoveMode::AURA_REMOVE_BY_DEATH)
                        l_Boss->AI()->DoAction(eAction::CancelBreakersStrength);
                }
            }

            void Register() override
            {
                AfterEffectApply += AuraEffectApplyFn(spell_highmaul_nullification_barrier_AuraScript::AfterApply, EFFECT_0, SPELL_AURA_SCHOOL_ABSORB, AURA_EFFECT_HANDLE_REAL);
                OnEffectPeriodic += AuraEffectPeriodicFn(spell_highmaul_nullification_barrier_AuraScript::OnTick, EFFECT_1, SPELL_AURA_PERIODIC_DUMMY);
                OnEffectRemove += AuraEffectRemoveFn(spell_highmaul_nullification_barrier_AuraScript::OnRemove, EFFECT_0, SPELL_AURA_SCHOOL_ABSORB, AURA_EFFECT_HANDLE_REAL);
            }
        };

        AuraScript* GetAuraScript() const override
        {
            return new spell_highmaul_nullification_barrier_AuraScript();
        }
};

/// Caustic Energy - 160720
class spell_highmaul_caustic_energy : public SpellScriptLoader
{
    public:
        spell_highmaul_caustic_energy() : SpellScriptLoader("spell_highmaul_caustic_energy") { }

        class spell_highmaul_caustic_energy_AuraScript : public AuraScript
        {
            PrepareAuraScript(spell_highmaul_caustic_energy_AuraScript);

            enum eSpell
            {
                CausticEnergyDoT = 161242
            };

            enum eDatas
            {
                DataMaxRunicPlayers,
                DataRunicPlayersCount
            };

            enum eTalk
            {
                CausticEnergyWarn = 9
            };

            enum eCreature
            {
                VolatileAnomaly = 79956
            };

            uint32 m_DamageTimer;

            bool Load()
            {
                m_DamageTimer = 200;
                return true;
            }

            void OnUpdate(uint32 p_Diff)
            {
                if (m_DamageTimer)
                {
                    if (m_DamageTimer <= p_Diff)
                    {
                        if (Unit* l_Target = GetUnitOwner())
                        {
                            std::list<Unit*> l_TargetList;
                            float l_Radius = 10.0f;

                            Trinity::AnyUnitInObjectRangeCheck l_Check(l_Target, l_Radius);
                            Trinity::UnitListSearcher<Trinity::AnyUnitInObjectRangeCheck> l_Searcher(l_Target, l_TargetList, l_Check);
                            Cell::VisitGridObjects(l_Target, l_Searcher, l_Radius);

                            if (l_TargetList.empty())
                                return;

                            l_TargetList.remove_if([this](Unit* p_Unit) -> bool
                            {
                                if (p_Unit == nullptr || p_Unit->ToCreature())
                                    return true;

                                return false;
                            });

                            bool l_CanChargePlayer = false;
                            Creature* l_Boss = nullptr;
                            uint8 l_ChargingCount = 0;

                            if (InstanceScript* l_Instance = l_Target->GetInstanceScript())
                            {
                                if (l_Boss = ObjectAccessor::GetCreature(*l_Target, l_Instance->GetGuidData(eHighmaulCreatures::Koragh)))
                                {
                                    if (l_Boss->IsAIEnabled)
                                    {
                                        l_ChargingCount = l_Boss->AI()->GetData(eDatas::DataRunicPlayersCount);
                                        l_CanChargePlayer = l_ChargingCount < l_Boss->AI()->GetData(eDatas::DataMaxRunicPlayers);
                                    }
                                }
                            }

                            if (l_Boss == nullptr || !l_Boss->IsAIEnabled)
                            {
                                m_DamageTimer = 200;
                                return;
                            }

                            for (Unit* l_Iter : l_TargetList)
                            {
                                if (l_Iter->GetDistance(l_Target) <= 7.0f)
                                {
                                    if (!l_Iter->HasAura(eSpell::CausticEnergyDoT) && l_CanChargePlayer)
                                    {
                                        l_Iter->CastSpell(l_Iter, eSpell::CausticEnergyDoT, true);

                                        if (l_Boss != nullptr && l_Boss->IsAIEnabled)
                                        {
                                            l_Boss->TextEmote(eTalk::CausticEnergyWarn, l_Iter);
                                            l_Boss->AI()->SetData(eDatas::DataRunicPlayersCount, l_ChargingCount + 1);
                                        }
                                    }
                                }
                                else
                                {
                                    if (l_Iter->HasAura(eSpell::CausticEnergyDoT))
                                        l_Iter->RemoveAura(eSpell::CausticEnergyDoT);
                                }

                                /// Must be updated
                                l_ChargingCount = l_Boss->AI()->GetData(eDatas::DataRunicPlayersCount);
                                l_CanChargePlayer = l_ChargingCount < l_Boss->AI()->GetData(eDatas::DataMaxRunicPlayers);
                            }
                        }

                        m_DamageTimer = 200;
                    }
                    else
                        m_DamageTimer -= p_Diff;
                }
            }

            void OnRemove(AuraEffect const* p_AurEff, AuraEffectHandleModes p_Mode)
            {
                if (Unit* l_Target = GetUnitOwner())
                {
                    std::list<Unit*> l_TargetList;
                    float l_Radius = 10.0f;

                    Trinity::AnyUnfriendlyUnitInObjectRangeCheck l_Check(l_Target, l_Target, l_Radius);
                    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> l_Searcher(l_Target, l_TargetList, l_Check);
                    Cell::VisitGridObjects(l_Target, l_Searcher, l_Radius);

                    for (Unit* l_Iter : l_TargetList)
                        l_Iter->RemoveAura(eSpell::CausticEnergyDoT);
                }
            }

            void Register() override
            {
                OnAuraUpdate += AuraUpdateFn(spell_highmaul_caustic_energy_AuraScript::OnUpdate);
                OnEffectRemove += AuraEffectRemoveFn(spell_highmaul_caustic_energy_AuraScript::OnRemove, EFFECT_0, SPELL_AURA_AREA_TRIGGER, AURA_EFFECT_HANDLE_REAL);
            }
        };

        AuraScript* GetAuraScript() const override
        {
            return new spell_highmaul_caustic_energy_AuraScript();
        }
};

/// Caustic Energy (DoT) - 161242
class spell_highmaul_caustic_energy_dot : public SpellScriptLoader
{
    public:
        spell_highmaul_caustic_energy_dot() : SpellScriptLoader("spell_highmaul_caustic_energy_dot") { }

        class spell_highmaul_caustic_energy_dot_AuraScript : public AuraScript
        {
            PrepareAuraScript(spell_highmaul_caustic_energy_dot_AuraScript);

            enum eSpells
            {
                NullificationBarrierPower   = 163612,
                NullificationBarrierAbsorb  = 163134,
                MarkOfNullification         = 172886
            };

            enum eDatas
            {
                DataRunicPlayersCount = 1
            };

            void OnApply(AuraEffect const* p_AurEff, AuraEffectHandleModes p_Mode)
            {
                if (Unit* l_Target = GetTarget())
                {
                    l_Target->CastSpell(l_Target, eSpells::NullificationBarrierPower, true);
                    l_Target->CastSpell(l_Target, eSpells::MarkOfNullification, true);
                    l_Target->SetPower(Powers::POWER_ALTERNATE_POWER, 0);
                }
            }

            void OnRemove(AuraEffect const* p_AurEff, AuraEffectHandleModes p_Mode)
            {
                if (Unit* l_Caster = GetCaster())
                {
                    if (Unit* l_Target = GetTarget())
                    {
                        l_Target->CastSpell(l_Target, eSpells::NullificationBarrierAbsorb, true);
                        l_Target->RemoveAura(eSpells::MarkOfNullification);

                        if (InstanceScript* l_Instance = l_Caster->GetInstanceScript())
                        {
                            if (Creature* l_Boss = ObjectAccessor::GetCreature(*l_Target, l_Instance->GetGuidData(eHighmaulCreatures::Koragh)))
                            {
                                if (l_Boss->IsAIEnabled)
                                {
                                    uint8 l_ActualCount = l_Boss->AI()->GetData(eDatas::DataRunicPlayersCount);

                                    if (l_ActualCount > 0)
                                        l_Boss->AI()->SetData(eDatas::DataRunicPlayersCount, l_ActualCount - 1);
                                }
                            }
                        }

                        if (AuraEffect* l_Absorb = l_Target->GetAuraEffect(eSpells::NullificationBarrierAbsorb, EFFECT_0))
                        {
                            /// The Nullification Barrier received by players can absorb up to 15000000 Magic damage.
                            int32 l_MaxAmount = 15000000;

                            int32 l_Pct = l_Target->GetPower(Powers::POWER_ALTERNATE_POWER);

                            /// When the rune powers down, the player receives their own Nullification Barrier, proportional to the amount of time spent inside the rune.
                            l_Absorb->ChangeAmount(CalculatePct(l_MaxAmount, l_Pct));
                        }
                    }
                }
            }

            void Register() override
            {
                OnEffectApply += AuraEffectApplyFn(spell_highmaul_caustic_energy_dot_AuraScript::OnApply, EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE, AURA_EFFECT_HANDLE_REAL);
                OnEffectRemove += AuraEffectRemoveFn(spell_highmaul_caustic_energy_dot_AuraScript::OnRemove, EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE, AURA_EFFECT_HANDLE_REAL);
            }
        };

        AuraScript* GetAuraScript() const override
        {
            return new spell_highmaul_caustic_energy_dot_AuraScript();
        }
};

/// Expel Magic: Fire - 162185
class spell_highmaul_expel_magic_fire : public SpellScriptLoader
{
    public:
        spell_highmaul_expel_magic_fire() : SpellScriptLoader("spell_highmaul_expel_magic_fire") { }

        class spell_highmaul_expel_magic_fire_AuraScript : public AuraScript
        {
            PrepareAuraScript(spell_highmaul_expel_magic_fire_AuraScript);

            enum eSpell
            {
                ExpelMagicFireAoE = 172685
            };

            void OnRemove(AuraEffect const* p_AurEff, AuraEffectHandleModes p_Mode)
            {
                if (Unit* l_Target = GetTarget())
                    l_Target->CastSpell(l_Target, eSpell::ExpelMagicFireAoE, true);
            }

            void Register() override
            {
                OnEffectRemove += AuraEffectRemoveFn(spell_highmaul_expel_magic_fire_AuraScript::OnRemove, EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE, AURA_EFFECT_HANDLE_REAL);
            }
        };

        AuraScript* GetAuraScript() const override
        {
            return new spell_highmaul_expel_magic_fire_AuraScript();
        }
};

/// Expel Magic: Arcane - 162186
class spell_highmaul_expel_magic_arcane : public SpellScriptLoader
{
    public:
        spell_highmaul_expel_magic_arcane() : SpellScriptLoader("spell_highmaul_expel_magic_arcane") { }

        class spell_highmaul_expel_magic_arcane_AuraScript : public AuraScript
        {
            PrepareAuraScript(spell_highmaul_expel_magic_arcane_AuraScript);

            enum eSpell
            {
                ExpelMagicArcaneMissile = 162398
            };

            void OnTick(AuraEffect const* p_AurEff)
            {
                if (Unit* l_Target = GetTarget())
                    l_Target->CastSpell(l_Target, eSpell::ExpelMagicArcaneMissile, true);
            }

            void Register() override
            {
                OnEffectPeriodic += AuraEffectPeriodicFn(spell_highmaul_expel_magic_arcane_AuraScript::OnTick, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
            }
        };

        AuraScript* GetAuraScript() const override
        {
            return new spell_highmaul_expel_magic_arcane_AuraScript();
        }
};

/// Nullification Barrier (Absorb) - 163134
class spell_highmaul_nullification_barrier_player : public SpellScriptLoader
{
    public:
        spell_highmaul_nullification_barrier_player() : SpellScriptLoader("spell_highmaul_nullification_barrier_player") { }

        class spell_highmaul_nullification_barrier_player_AuraScript : public AuraScript
        {
            PrepareAuraScript(spell_highmaul_nullification_barrier_player_AuraScript);

            enum eSpell
            {
                NullificationBarrierPower = 163612
            };

            void OnAbsorb(AuraEffect* p_AurEff, DamageInfo& p_DmgInfo, uint32& p_AbsorbAmount)
            {
                if (Unit* l_Target = GetTarget())
                {
                    /// The Nullification Barrier received by players can absorb up to 15000000 Magic damage.
                    int32 l_MaxAbsorb = 15000000;

                    int32 l_Pct = ((float)p_AurEff->GetAmount() / (float)l_MaxAbsorb) * 100.0f;
                    l_Target->SetPower(Powers::POWER_ALTERNATE_POWER, l_Pct);
                }
            }

            void OnRemove(AuraEffect const* p_AurEff, AuraEffectHandleModes p_Mode)
            {
                if (Unit* l_Target = GetUnitOwner())
                    l_Target->RemoveAura(eSpell::NullificationBarrierPower);
            }

            void Register() override
            {
                OnEffectAbsorb += AuraEffectAbsorbFn(spell_highmaul_nullification_barrier_player_AuraScript::OnAbsorb, EFFECT_0, SPELL_AURA_SCHOOL_ABSORB);
                OnEffectRemove += AuraEffectRemoveFn(spell_highmaul_nullification_barrier_player_AuraScript::OnRemove, EFFECT_0, SPELL_AURA_SCHOOL_ABSORB, AURA_EFFECT_HANDLE_REAL);
            }
        };

        AuraScript* GetAuraScript() const override
        {
            return new spell_highmaul_nullification_barrier_player_AuraScript();
        }
};

/// Expel Magic: Frost - 172813
class spell_highmaul_expel_magic_frost_aura : public SpellScriptLoader
{
    public:
        spell_highmaul_expel_magic_frost_aura() : SpellScriptLoader("spell_highmaul_expel_magic_frost_aura") { }

        class spell_highmaul_expel_magic_frost_aura_AuraScript : public AuraScript
        {
            PrepareAuraScript(spell_highmaul_expel_magic_frost_aura_AuraScript);

            enum eSpell
            {
                ExpelMagicFrostAreaTrigger = 172747
            };

            void OnTick(AuraEffect const* p_AurEff)
            {
                if (Unit* l_Target = GetTarget())
                {
                    if (AreaTrigger* l_AreaTrigger = l_Target->SelectNearestAreaTrigger(eSpell::ExpelMagicFrostAreaTrigger, 30.0f))
                    {
                        float l_Distance = l_Target->GetDistance(l_AreaTrigger);
                        float l_MinDistance = 5.0f;
                        if (l_Distance <= l_MinDistance)
                            return;

                        float l_MaxDistance = 25.0f;
                        int32 l_MaxAmount = 0;

                        if (AuraEffect* l_SpeedEffect = p_AurEff->GetBase()->GetEffect(EFFECT_0))
                        {
                            l_MaxAmount = l_SpeedEffect->GetBaseAmount();
                            if (!l_MaxAmount)
                                return;

                            int32 l_NewAmount = l_MaxAmount * (l_Distance / l_MaxDistance);
                            l_SpeedEffect->ChangeAmount(l_NewAmount);
                        }
                    }
                    else
                        p_AurEff->GetBase()->Remove();
                }
            }

            void Register() override
            {
                OnEffectPeriodic += AuraEffectPeriodicFn(spell_highmaul_expel_magic_frost_aura_AuraScript::OnTick, EFFECT_1, SPELL_AURA_PERIODIC_DUMMY);
            }
        };

        AuraScript* GetAuraScript() const override
        {
            return new spell_highmaul_expel_magic_frost_aura_AuraScript();
        }
};

/// Suppression Field (aura) - 161328
class spell_highmaul_suppression_field_aura : public SpellScriptLoader
{
    public:
        spell_highmaul_suppression_field_aura() : SpellScriptLoader("spell_highmaul_suppression_field_aura") { }

        class spell_highmaul_suppression_field_aura_AuraScript : public AuraScript
        {
            PrepareAuraScript(spell_highmaul_suppression_field_aura_AuraScript);

            enum eAction
            {
                SuppressionField = 1
            };

            void OnRemove(AuraEffect const* p_AurEff, AuraEffectHandleModes p_Mode)
            {
                if (Creature* l_Target = GetTarget()->ToCreature())
                {
                    if (l_Target->IsAIEnabled)
                        l_Target->AI()->DoAction(eAction::SuppressionField);
                }
            }

            void Register() override
            {
                OnEffectRemove += AuraEffectRemoveFn(spell_highmaul_suppression_field_aura_AuraScript::OnRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
            }
        };

        AuraScript* GetAuraScript() const override
        {
            return new spell_highmaul_suppression_field_aura_AuraScript();
        }
};

/// Expel Magic: Fel (dummy) - 172895
class spell_highmaul_expel_magic_fel : public SpellScriptLoader
{
public:
    spell_highmaul_expel_magic_fel() : SpellScriptLoader("spell_highmaul_expel_magic_fel") { }

    enum eSpell
    {
        ExpellMagicAT   = 172915
    };

    class spell_highmaul_expel_magic_fel_SpellScript : public SpellScript
    {
        PrepareSpellScript(spell_highmaul_expel_magic_fel_SpellScript);

        void CorrectTargets(std::list<WorldObject*>& p_Targets)
        {
            p_Targets.clear();
            std::list<Player*> l_PlayerList;

            if (Unit* l_Caster = GetCaster())
                l_Caster->GetPlayerListInGrid(l_PlayerList, 300.0f);

            if (l_PlayerList.empty())
                return;

            for (auto itr : l_PlayerList)
                p_Targets.push_back(itr);

            Trinity::Containers::RandomResize(p_Targets, GetSpellInfo()->MaxAffectedTargets);

            if (Creature* koragh = GetCaster()->ToCreature())
                for (auto itr : p_Targets)
                    koragh->AI()->Talk(/*eTalks::ExpelMagicFelWarn*/10, itr);
        }

        void Register() override
        {
            OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_highmaul_expel_magic_fel_SpellScript::CorrectTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENEMY);
        }
    };

    SpellScript* GetSpellScript() const override
    {
        return new spell_highmaul_expel_magic_fel_SpellScript();
    }

    class spell_highmaul_expel_magic_fel_AuraScript : public AuraScript
    {
        PrepareAuraScript(spell_highmaul_expel_magic_fel_AuraScript);

        void OnApply(AuraEffect const* p_AurEff, AuraEffectHandleModes p_Mode)
        {
            if (Unit* victim = GetTarget())
                DepartPos = victim->GetPosition();
        }

        Position DepartPos;

        void OnRemove(AuraEffect const* p_AurEff, AuraEffectHandleModes p_Mode)
        {
            if (Unit* victim = GetTarget())
                if (Creature* koragh = victim->GetInstanceScript()->instance->GetCreature(victim->GetInstanceScript()->GetGuidData(eHighmaulCreatures::Koragh)))
                {
                    Position FinishPos = victim->GetPosition();

                    koragh->CastSpell(DepartPos, ExpellMagicAT, true);
                    koragh->CastSpell(FinishPos, ExpellMagicAT, true);

                    // '+' : AT
                    /* dep   <- maxdi ->   fin */
                    /*  + -- + -- + -- + -- +  */

                    float maxdist = 1.5f;
                    float distBetween = DepartPos.GetExactDist(&FinishPos);

                    if (distBetween < maxdist)
                        return;

                    uint16 stepsCount = uint16(distBetween / maxdist) + 1;
                    float stepSize = distBetween / stepsCount;
                    float HalfstepSize = stepSize / 2;
                    float orientation = std::atan2(FinishPos.m_positionY - DepartPos.m_positionY, FinishPos.m_positionX - DepartPos.m_positionX);
                    Map* map = koragh->GetMap();

                    for (uint8 i = 0; i <= stepsCount; i++)
                    {
                        float x = DepartPos.m_positionX + (HalfstepSize + i * stepSize) * std::cos(orientation);
                        float y = DepartPos.m_positionY + (HalfstepSize + i * stepSize) * std::sin(orientation);

                        if (!Trinity::IsValidMapCoord(x, y))
                            continue;

                        float z = map->GetHeight(koragh->GetPhaseShift(), x, y, std::max(DepartPos.m_positionZ, FinishPos.m_positionZ), true, 25);
                        koragh->CastSpell(x, y, z, ExpellMagicAT, true);
                    }
                }
        }

        void Register() override
        {
            OnEffectApply += AuraEffectApplyFn(spell_highmaul_expel_magic_fel_AuraScript::OnApply, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
            OnEffectRemove += AuraEffectRemoveFn(spell_highmaul_expel_magic_fel_AuraScript::OnRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        }
    };

    AuraScript* GetAuraScript() const override
    {
        return new spell_highmaul_expel_magic_fel_AuraScript();
    }
};

/// Wild Flames - 173824
class areatrigger_highmaul_wild_flames : public AreaTriggerAI
{
    public:
        areatrigger_highmaul_wild_flames(AreaTrigger* areatrigger) : AreaTriggerAI(areatrigger)
        {
            m_DamageTimer = 200;
        }

        enum eSpell
        {
            WildFlames = 173827
        };

        uint32 m_DamageTimer;

        void OnUpdate(uint32 p_Time) override
        {
            if (m_DamageTimer)
            {
                if (m_DamageTimer <= p_Time)
                {
                    if (Unit* l_Caster = at->GetCaster())
                    {
                        std::list<Unit*> l_TargetList;
                        float l_Radius = 6.0f;

                        Trinity::AnyUnfriendlyUnitInObjectRangeCheck l_Check(at, l_Caster, l_Radius);
                        Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> l_Searcher(at, l_TargetList, l_Check);
                        Cell::VisitAllObjects(at, l_Searcher, l_Radius);

                        for (Unit* l_Unit : l_TargetList)
                        {
                            if (l_Unit->GetDistance(l_Caster) <= 3.0f)
                                l_Unit->CastSpell(l_Unit, eSpell::WildFlames, true);
                            else
                                l_Unit->RemoveAura(eSpell::WildFlames);
                        }
                    }

                    m_DamageTimer = 200;
                }
                else
                    m_DamageTimer -= p_Time;
            }
        }
};

/// Suppression Field - 161330
class areatrigger_highmaul_suppression_field : public AreaTriggerAI
{
    public:
        areatrigger_highmaul_suppression_field(AreaTrigger* areatrigger) : AreaTriggerAI(areatrigger) { }

        enum eSpells
        {
            SuppressionFieldDoT     = 161345,
            SuppressionFieldSilence = 162595
        };

        void OnUpdate(uint32 p_Time) override
        {
            if (Unit* l_Caster = at->GetCaster())
            {
                std::list<Unit*> l_TargetList;
                float l_Radius = 15.0f;

                Trinity::AnyUnitInObjectRangeCheck l_Check(at, l_Radius);
                Trinity::UnitListSearcher<Trinity::AnyUnitInObjectRangeCheck> l_Searcher(at, l_TargetList, l_Check);
                Cell::VisitAllObjects(at, l_Searcher, l_Radius);

                for (Unit* l_Unit : l_TargetList)
                {
                    if (l_Unit->GetDistance(at) <= 6.0f)
                    {
                        if (l_Unit->GetEntry() == eHighmaulCreatures::VolatileAnomaly)
                        {
                            if (!l_Unit->HasAura(eSpells::SuppressionFieldSilence))
                                l_Caster->AddAura(eSpells::SuppressionFieldSilence, l_Unit);
                        }
                        else if (l_Unit->IsHostileTo(l_Caster))
                        {
                            if (!l_Unit->HasAura(eSpells::SuppressionFieldDoT))
                                l_Caster->CastSpell(l_Unit, eSpells::SuppressionFieldDoT, true);

                            if (!l_Unit->HasAura(eSpells::SuppressionFieldSilence))
                                l_Caster->CastSpell(l_Unit, eSpells::SuppressionFieldSilence, true);
                        }
                    }
                    else if (!l_Unit->SelectNearestAreaTrigger(at->GetSpellId(), 6.0f))
                    {
                        if (l_Unit->HasAura(eSpells::SuppressionFieldDoT))
                            l_Unit->RemoveAura(eSpells::SuppressionFieldDoT);

                        if (l_Unit->HasAura(eSpells::SuppressionFieldSilence))
                            l_Unit->RemoveAura(eSpells::SuppressionFieldSilence);
                    }
                }
            }
        }

        void OnRemove() override
        {
            if (Unit* l_Caster = at->GetCaster())
            {
                std::list<Unit*> l_TargetList;
                float l_Radius = 15.0f;

                Trinity::AnyUnitInObjectRangeCheck l_Check(at, l_Radius);
                Trinity::UnitListSearcher<Trinity::AnyUnitInObjectRangeCheck> l_Searcher(at, l_TargetList, l_Check);
                Cell::VisitAllObjects(at, l_Searcher, l_Radius);

                for (Unit* l_Unit : l_TargetList)
                {
                    if (!l_Unit->SelectNearestAreaTrigger(at->GetSpellId(), 6.0f))
                    {
                        if (l_Unit->HasAura(eSpells::SuppressionFieldDoT))
                            l_Unit->RemoveAura(eSpells::SuppressionFieldDoT);

                        if (l_Unit->HasAura(eSpells::SuppressionFieldSilence))
                            l_Unit->RemoveAura(eSpells::SuppressionFieldSilence);
                    }
                }
            }
        }
};

/// Expel Magic: Frost - 172747
class areatrigger_highmaul_expel_magic_frost : public AreaTriggerAI
{
    public:
        areatrigger_highmaul_expel_magic_frost(AreaTrigger* areatrigger) : AreaTriggerAI(areatrigger)
        {
            m_DamageTimer = 200;
        }

        enum eSpell
        {
            ExpelMagicFrostAura = 172813
        };

        uint32 m_DamageTimer;

        void OnUpdate(uint32 p_Time) override
        {
            if (m_DamageTimer)
            {
                if (m_DamageTimer <= p_Time)
                {
                    if (Unit* l_Caster = at->GetCaster())
                    {
                        std::list<Unit*> l_TargetList;
                        float l_Radius = 30.0f;

                        Trinity::AnyUnfriendlyUnitInObjectRangeCheck l_Check(at, l_Caster, l_Radius);
                        Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> l_Searcher(at, l_TargetList, l_Check);
                        Cell::VisitAllObjects(at, l_Searcher, l_Radius);

                        for (Unit* l_Unit : l_TargetList)
                            l_Caster->CastSpell(l_Unit, eSpell::ExpelMagicFrostAura, true);
                    }

                    m_DamageTimer = 200;
                }
                else
                    m_DamageTimer -= p_Time;
            }
        }
};

/// Overflowing Energy - 161574
class areatrigger_highmaul_overflowing_energy : public AreaTriggerAI
{
    public:
        areatrigger_highmaul_overflowing_energy(AreaTrigger* areatrigger) : AreaTriggerAI(areatrigger)
        {
            m_FirstMove = true;
        }

        enum eSpells
        {
            GroundMarker            = 173048,
            OverflowingEnergyAoE    = 161576,
            OverflowingEnergyDamage = 161612
        };

        enum eVisual
        {
            OverflowingVisual = 39136
        };

        bool m_FirstMove;

        void OnCreate() override
        {
            float l_Rotation = frand(0.0f, 2 * M_PI);
            float l_Range = frand(10.0f, 30.0f);

            Position dest =
            {
                g_CenterPos.m_positionX + (l_Range * cos(l_Rotation)),
                g_CenterPos.m_positionY + (l_Range * sin(l_Rotation)),
                at->m_positionZ - 3.5f
            };

            at->GetCaster()->CastSpell(dest, eSpells::GroundMarker, true);
            at->SetDestination(dest, 10000);
        }

        void OnUpdate(uint32 p_Time) override
        {
            if (Unit* l_Caster = at->GetCaster())
            {
                if (Player* l_Target = at->SelectNearestPlayer(1.0f))
                {
                    l_Caster->CastSpell(l_Target, eSpells::OverflowingEnergyDamage, true);
                    at->Remove();
                }
            }
        }

        void OnDestinationReached() override
        {
            if (m_FirstMove)
            {
                m_FirstMove = false;

                Position l_Pos = *at;
                l_Pos.m_positionZ = g_CenterPos.m_positionZ;

                uint32 l_Time = 8 * TimeConstants::IN_MILLISECONDS;
                uint32 l_OldTime = 8 * TimeConstants::IN_MILLISECONDS;

                at->SetDestination(l_Pos, l_Time + l_OldTime);

                if (Unit* l_Boss = at->GetCaster())
                    l_Boss->SendPlaySpellVisual(l_Pos, eVisual::OverflowingVisual, 6.5f);
            }
            else
            {
                if (Unit* l_Caster = at->GetCaster())
                {
                    l_Caster->CastSpell(*at, eSpells::OverflowingEnergyAoE, true);
                    at->Remove();

                    if (InstanceScript* l_InstanceScript = l_Caster->GetInstanceScript())
                        l_InstanceScript->SetData(eHighmaulDatas::KoraghOverflowingEnergy, 1);
                }
            }
        }

        void OnRemove() override
        {
            if (AreaTrigger* l_Visual = at->SelectNearestAreaTrigger(eSpells::GroundMarker, 2.0f))
                l_Visual->Remove();
        }
};

/// Expel Magic: Fel (AT) - 172915
class areatrigger_highmaul_expel_magic_fel : public AreaTriggerAI
{
public:
    areatrigger_highmaul_expel_magic_fel(AreaTrigger* areatrigger) : AreaTriggerAI(areatrigger) { }

    enum eSpell
    {
        ExpellMagicFelDmg   = 172917
    };

    void OnUnitEnter(Unit* p_Unit) override
    {
        if (!p_Unit->HasAura(ExpellMagicFelDmg) && (p_Unit->GetTypeId() == TYPEID_PLAYER || p_Unit->IsCharmedOwnedByPlayerOrPlayer()))
            p_Unit->AddAura(ExpellMagicFelDmg, p_Unit);
    }

    void OnUnitExit(Unit* p_Unit) override
    {
        if (p_Unit->HasAura(ExpellMagicFelDmg))
            p_Unit->RemoveAura(ExpellMagicFelDmg);
    }

    void OnRemove() override
    {
        for (auto l_Guid : at->GetInsideUnits())
            if (Unit* l_Target = ObjectAccessor::GetUnit(*at, l_Guid))
                l_Target->RemoveAurasDueToSpell(ExpellMagicFelDmg);
    }
};

/// Pair Annihilation - 8976
class achievement_highmaul_pair_annihilation : public AchievementCriteriaScript
{
    public:
        achievement_highmaul_pair_annihilation() : AchievementCriteriaScript("achievement_highmaul_pair_annihilation") { }

        bool OnCheck(Player* p_Source, Unit* /*p_Target*/) override
        {
            if (!p_Source || !p_Source->GetInstanceScript())
                return false;

            if (p_Source->GetInstanceScript()->GetData(eHighmaulDatas::KoraghAchievement))
                return true;

            return false;
        }
};

void AddSC_boss_koragh()
{
    /// Boss
    new boss_koragh();

    /// NPCs
    new npc_highmaul_breaker_of_fel();
    new npc_highmaul_breaker_of_fire();
    new npc_highmaul_wild_flames();
    new npc_highmaul_breaker_of_frost();
    new npc_highmaul_koragh_volatile_anomaly();

    /// GameObjects
    new go_highmaul_chain();

    /// Spells
    new spell_highmaul_frozen_core();
    new spell_highmaul_wild_flames_areatrigger();
    new spell_highmaul_nullification_barrier();
    new spell_highmaul_caustic_energy();
    new spell_highmaul_caustic_energy_dot();
    new spell_highmaul_expel_magic_fire();
    new spell_highmaul_expel_magic_arcane();
    new spell_highmaul_nullification_barrier_player();
    new spell_highmaul_expel_magic_frost_aura();
    new spell_highmaul_suppression_field_aura();
    new spell_highmaul_frozen_core();
    new spell_highmaul_expel_magic_fel();

    /// AreaTriggers
    RegisterAreaTriggerAI(areatrigger_highmaul_wild_flames);
    RegisterAreaTriggerAI(areatrigger_highmaul_suppression_field);
    RegisterAreaTriggerAI(areatrigger_highmaul_expel_magic_frost);
    RegisterAreaTriggerAI(areatrigger_highmaul_overflowing_energy);
    RegisterAreaTriggerAI(areatrigger_highmaul_expel_magic_fel);

    /// Achievement
    new achievement_highmaul_pair_annihilation();
}
