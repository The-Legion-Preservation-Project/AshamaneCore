# include "highmaul.h"

uint8 GetEnergyGainFromHealth(float p_HealthPct)
{
    if (p_HealthPct >= 50.0f)
        return 1;
    else if (p_HealthPct >= 20.0f)
        return 2;
    else if (p_HealthPct >= 10.0f)
        return 3;
    else if (p_HealthPct >= 5.0f)
        return 4;
    else
        return 5;
}

void RespawnGuardians(Creature* p_Source, InstanceScript* p_Instance)
{
    if (p_Source == nullptr || p_Instance == nullptr)
        return;

    if (Creature* l_Rokka = ObjectAccessor::GetCreature(*p_Source, p_Instance->GetGuidData(eHighmaulCreatures::Rokka)))
    {
        l_Rokka->Respawn();
        l_Rokka->GetMotionMaster()->MoveTargetedHome();
    }

    if (Creature* l_Lokk = ObjectAccessor::GetCreature(*p_Source, p_Instance->GetGuidData(eHighmaulCreatures::Lokk)))
    {
        l_Lokk->Respawn();
        l_Lokk->GetMotionMaster()->MoveTargetedHome();
    }

    if (Creature* l_Oro = ObjectAccessor::GetCreature(*p_Source, p_Instance->GetGuidData(eHighmaulCreatures::Oro)))
    {
        l_Oro->Respawn();
        l_Oro->GetMotionMaster()->MoveTargetedHome();
    }
}

void StartGuardians(Creature* p_Source, Unit* p_Target)
{
    if (p_Source == nullptr || p_Target == nullptr)
        return;

    if (Creature* l_Rokka = p_Source->FindNearestCreature(eHighmaulCreatures::Rokka, 100.0f))
        l_Rokka->AI()->AttackStart(p_Target);

    if (Creature* l_Lokk = p_Source->FindNearestCreature(eHighmaulCreatures::Lokk, 100.0f))
        l_Lokk->AI()->AttackStart(p_Target);

    if (Creature* l_Oro = p_Source->FindNearestCreature(eHighmaulCreatures::Oro, 100.0f))
        l_Oro->AI()->AttackStart(p_Target);
}

/// Tectus <The Living Mountain> - 78948
/// Shard of Tectus <Shard of the Mountain> - 80551
/// Mote of Tectus <Mote of the Mountain> - 80557
class boss_tectus : public CreatureScript
{
    public:
        boss_tectus() : CreatureScript("boss_tectus") { }

        enum eSpells
        {
            /// Misc
            BreakPlayerTargetting       = 140562,
            SuicideNoBloodNoLogging     = 117624,
            SpawnTectusShards           = 169931,
            EncounterEvent              = 181089,   ///< Don't know why, maybe useful
            MidsizeTectusDamageReduct   = 178193,   ///< -30% damage done
            MoteOfTectusDamageReduct    = 178194,   ///< -60% damage done
            EarthFurySpawnDustCloud     = 169949,
            Grow                        = 166306,
            /// Energy Management
            ZeroPowerZeroRegen          = 118357,
            /// Periodic dummy of 1s
            TheLivingMountain           = 162287,
            ShardOfTheMountain          = 162658,
            MoteOfTheMountain           = 162674,
            /// Arrow visual on player
            SpellCrystallineBarrage     = 162346,
            CrystallineBarrageSummon    = 162371,
            CrystallineBarrageDoT       = 162370,
            /// +5% damage done
            Accretion                   = 162288,
            Petrification               = 163809,
            /// Fracture
            FractureSearcher            = 163214,   ///< Must trigger 163208
            FractureMissile             = 163208,   ///< Missile: 163209
            /// Tectonic Upheaval
            SpellTectonicUpheaval       = 162475,    ///< 2s cast time, triggers 162510 each 1.5s
            /// Loot
            TectusBonus                 = 177523,
            /// Mythic
            HollowedHeartOfTheMountain  = 178191,
            HollowedShardOfTheMountain  = 169729,
        };

        enum eEvents
        {
            EventEnrage = 1,
            EventSpawnBersererk,
            EventSpawnEarthwarper,
            EventCrystallineBarrage,
            EventSpawnCrystalline,
            /// Used at 25 energy
            EventEarthenPillar,
            EventFracture,
            EventAccretion,
            EventTectonicUpheaval
        };

        enum eActions
        {
            GuardianDead,
            ScheduleEarthenPillar,
            ScheduleTectonicUpheaval,
            MoteKilled
        };

        enum eAnimKits
        {
            AnimRise    = 6961,
            AnimRise2   = 6958,
            AnimFall    = 6918,
            AnimWounded = 2156
        };

        enum eCreatures
        {
            EarthenPillarStalker    = 80476,
            ShardOfTectus           = 80551,
            MoteOfTectus            = 80557,
            NightTwistedBerserker   = 80822,
            NightTwistedEarthwarper = 80599
        };

        enum eTalks
        {
            Aggro,
            Shattered,
            TectonicUpheaval,
            TectonicUpheavalCompleted,
            EarthenPillar,
            Slay,
            Death,
            CrystallineBarrage
        };

        enum eMiscs
        {
            ShardSpawnCount = 2,
            MotesSpawnCount = 4,
            InvisDisplay    = 11686
        };

        struct boss_tectusAI : public BossAI
        {
            boss_tectusAI(Creature* p_Creature) : BossAI(p_Creature, eHighmaulDatas::BossTectus)
            {
                m_Instance = p_Creature->GetInstanceScript();
            }

            EventMap m_Events;
            InstanceScript* m_Instance;

            ObjectGuid m_CrystallineBarrageTarget;

            Position m_FirstCrystalline;

            bool m_TectonicScheduled;
            bool m_CanEternalTalk;

            uint8 m_MoteKilled;

            void Reset() override
            {
                m_Events.Reset();

                if (me->GetEntry() == eHighmaulCreatures::Tectus)
                    _Reset();

                me->CastSpell(me, eSpells::ZeroPowerZeroRegen, true);

                me->ClearUnitState(UnitState::UNIT_STATE_STUNNED);
                me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_REMOVE_CLIENT_CONTROL | UNIT_FLAG_PREPARATION | UNIT_FLAG_DISARMED);

                switch (me->GetEntry())
                {
                    case eHighmaulCreatures::Tectus:
                    {
                        me->CastSpell(me, eSpells::TheLivingMountain, true);

                        if (m_Instance)
                            m_Instance->DoRemoveAurasDueToSpellOnPlayers(eSpells::SpellCrystallineBarrage);

                        if (!AllGardiansDead())
                        {
                            me->SetReactState(ReactStates::REACT_PASSIVE);
                            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_IMMUNE_TO_PC);
                            me->SetAIAnimKitId(eAnimKits::AnimFall);
                        }
                        else
                        {
                            me->SetAIAnimKitId(0);
                            me->SetReactState(ReactStates::REACT_AGGRESSIVE);
                            me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_IMMUNE_TO_PC);
                        }

                        break;
                    }
                    case eCreatures::ShardOfTectus:
                    {
                        me->CastSpell(me, eSpells::ShardOfTheMountain, true);
                        me->CastSpell(me, eSpells::Grow, true);
                        me->SetDisplayId(eMiscs::InvisDisplay);
                        me->SetReactState(ReactStates::REACT_PASSIVE);
                        me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_IMMUNE_TO_PC);
                        me->AddUnitState(UnitState::UNIT_STATE_STUNNED);
                        break;
                    }
                    case eCreatures::MoteOfTectus:
                    {
                        me->CastSpell(me, eSpells::MoteOfTheMountain, true);
                        me->CastSpell(me, eSpells::Grow, true);
                        me->SetDisplayId(eMiscs::InvisDisplay);
                        me->SetReactState(ReactStates::REACT_PASSIVE);
                        me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_IMMUNE_TO_PC);
                        me->AddUnitState(UnitState::UNIT_STATE_STUNNED);
                        break;
                    }
                    default:
                        break;
                }

                me->RemoveAura(eHighmaulSpells::Berserker);

                me->RemoveAllAreaTriggers();

                me->SetPower(Powers::POWER_ENERGY, 0);
                me->SetMaxPower(Powers::POWER_ENERGY, 100);
                me->RemoveFlag(UNIT_FIELD_FLAGS_2, UNIT_FLAG2_REGENERATE_POWER | UNIT_FLAG2_DISABLE_TURN);

                m_CrystallineBarrageTarget = ObjectGuid::Empty;

                m_FirstCrystalline = Position();

                m_TectonicScheduled = false;
                m_CanEternalTalk = true;

                m_MoteKilled = 0;
            }

            void JustReachedHome() override
            {
                if (!AllGardiansDead())
                {
                    me->SetReactState(ReactStates::REACT_PASSIVE);
                    me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_IMMUNE_TO_PC);
                }
                else
                {
                    me->SetAIAnimKitId(0);
                    me->SetReactState(ReactStates::REACT_AGGRESSIVE);
                    me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_IMMUNE_TO_PC);
                }

                me->RemoveAura(eHighmaulSpells::Berserker);

                me->RemoveAllAreaTriggers();

                me->SetPower(Powers::POWER_ENERGY, 0);
                me->SetMaxPower(Powers::POWER_ENERGY, 100);
                me->RemoveFlag(UNIT_FIELD_FLAGS_2, UNIT_FLAG2_REGENERATE_POWER | UNIT_FLAG2_DISABLE_TURN);

                m_CrystallineBarrageTarget = ObjectGuid::Empty;

                m_FirstCrystalline = Position();

                m_TectonicScheduled = false;
                m_CanEternalTalk = true;

                m_MoteKilled = 0;
            }

            bool CanRespawn() override
            {
                return false;
            }

            void DoAction(int32 const p_Action) override
            {
                switch (p_Action)
                {
                    case eActions::GuardianDead:
                    {
                        if (!AllGardiansDead())
                            return;

                        me->SetAIAnimKitId(0);
                        me->PlayOneShotAnimKitId(eAnimKits::AnimRise);

                        AddTimedDelayedOperation(4500, [this]() -> void
                        {
                            me->SetReactState(ReactStates::REACT_AGGRESSIVE);
                            me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_IMMUNE_TO_PC);
                        });

                        break;
                    }
                    case eActions::ScheduleEarthenPillar:
                    {
                        m_Events.RescheduleEvent(eEvents::EventEarthenPillar, 100);
                        break;
                    }
                    case eActions::ScheduleTectonicUpheaval:
                    {
                        if (m_TectonicScheduled || me->HasAura(eSpells::SpellTectonicUpheaval))
                            break;
                        m_TectonicScheduled = true;
                        m_Events.ScheduleEvent(eEvents::EventTectonicUpheaval, 100);
                        break;
                    }
                    case eActions::MoteKilled:
                    {
                        if (m_Instance != nullptr)
                            m_Instance->SetData(eHighmaulDatas::TectusAchievement, time(nullptr));

                        ++m_MoteKilled;

                        if (m_MoteKilled >= (eMiscs::MotesSpawnCount * 2))
                        {
                            me->SetAIAnimKitId(0);
                            me->PlayOneShotAnimKitId(eAnimKits::AnimRise);

                            AddTimedDelayedOperation(4 * TimeConstants::IN_MILLISECONDS, [this]() -> void
                            {
                                me->SetAIAnimKitId(eAnimKits::AnimWounded);
                                Talk(eTalks::Death);
                            });

                            AddTimedDelayedOperation(8 * TimeConstants::IN_MILLISECONDS, [this]() -> void
                            {
                                me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_IMMUNE_TO_PC);
                                if (Player* l_Player = me->GetMap()->GetPlayers().begin()->GetSource())
                                {
                                    l_Player->CastSpell(me, eSpells::SuicideNoBloodNoLogging, true);
                                    if(me->GetLootRecipient() == nullptr)
                                        me->SetLootRecipient(l_Player);
                                }
                            });
                        }

                        break;
                    }
                    default:
                        break;
                }
            }

            void EnterCombat(Unit* p_Attacker) override
            {
                if (!AllGardiansDead())
                {
                    EnterEvadeMode(EVADE_REASON_OTHER);
                    return;
                }

                if (me->GetEntry() == eHighmaulCreatures::Tectus)
                    _EnterCombat();

                m_Events.ScheduleEvent(eEvents::EventFracture, 8 * TimeConstants::IN_MILLISECONDS);
                m_Events.ScheduleEvent(eEvents::EventAccretion, 5 * TimeConstants::IN_MILLISECONDS);

                switch (me->GetEntry())
                {
                    case eCreatures::ShardOfTectus:
                        me->CastSpell(me, eSpells::MidsizeTectusDamageReduct, true);
                        if (m_Instance)
                            m_Instance->SendEncounterUnit(EncounterFrameType::ENCOUNTER_FRAME_ENGAGE, me, 1);
                        m_Events.ScheduleEvent(eEvents::EventCrystallineBarrage, 8 * TimeConstants::IN_MILLISECONDS);
                        break;
                    case eCreatures::MoteOfTectus:
                        me->CastSpell(me, eSpells::MoteOfTectusDamageReduct, true);
                        if (m_Instance)
                            m_Instance->SendEncounterUnit(EncounterFrameType::ENCOUNTER_FRAME_ENGAGE, me, 2);
                        m_Events.ScheduleEvent(eEvents::EventCrystallineBarrage, 8 * TimeConstants::IN_MILLISECONDS);
                        break;
                    case eHighmaulCreatures::Tectus:
                        if (m_Instance)
                            m_Instance->SendEncounterUnit(EncounterFrameType::ENCOUNTER_FRAME_ENGAGE, me);
                        Talk(eTalks::Aggro);
                        m_Events.ScheduleEvent(eEvents::EventEnrage, IsMythic() ? 480 * TimeConstants::IN_MILLISECONDS : 600 * TimeConstants::IN_MILLISECONDS);
                        m_Events.ScheduleEvent(eEvents::EventCrystallineBarrage, 5 * TimeConstants::IN_MILLISECONDS);
                        m_Events.ScheduleEvent(eEvents::EventSpawnBersererk, 18 * TimeConstants::IN_MILLISECONDS);
                        m_Events.ScheduleEvent(eEvents::EventSpawnEarthwarper, 8 * TimeConstants::IN_MILLISECONDS);
                        break;
                    default:
                        m_Events.ScheduleEvent(eEvents::EventCrystallineBarrage, 5 * TimeConstants::IN_MILLISECONDS);
                        break;
                }
            }

            void KilledUnit(Unit* p_Killed) override
            {
                if (p_Killed->GetTypeId() == TypeID::TYPEID_PLAYER && me->GetEntry() == eHighmaulCreatures::Tectus)
                    Talk(eTalks::Slay);
            }

            void JustDied(Unit* p_Killer) override
            {
                if (me->GetEntry() == eHighmaulCreatures::Tectus)
                    _JustDied();

                me->RemoveAllAreaTriggers();
                DespawnNightTwistedAdds();

                m_Events.Reset();

                if (m_Instance != nullptr)
                {
                    m_Instance->SendEncounterUnit(EncounterFrameType::ENCOUNTER_FRAME_DISENGAGE, me);

                    m_Instance->DoRemoveAurasDueToSpellOnPlayers(eSpells::SpellCrystallineBarrage);
                    m_Instance->DoRemoveAurasDueToSpellOnPlayers(eSpells::CrystallineBarrageDoT);

                    Map::PlayerList const& l_PlayerList = m_Instance->instance->GetPlayers();
                    if (l_PlayerList.isEmpty())
                        return;

                    for (Map::PlayerList::const_iterator l_Itr = l_PlayerList.begin(); l_Itr != l_PlayerList.end(); ++l_Itr)
                    {
                        if (Player* l_Player = l_Itr->GetSource())
                        {
                            /// Hacky but don't know why combat doesn't stop
                            if (!l_Player->isAttackingPlayer())
                                l_Player->CombatStop();
                        }
                    }

                    if (Creature* tectus = instance->instance->GetCreature(instance->GetGuidData(eHighmaulCreatures::Tectus)))
                        CastSpellToPlayers(tectus->GetMap(), tectus, eSpells::TectusBonus, true);

                    /*if (IsLFR())
                    {
                        Player* l_Player = l_PlayerList.begin()->GetSource();
                        if (l_Player && l_Player->GetGroup())
                            sLFGMgr->AutomaticLootAssignation(me, l_Player->GetGroup());
                    }*/
                }

                std::list<Creature*> l_Creatures;
                me->GetCreatureListWithEntryInGrid(l_Creatures, eCreatures::MoteOfTectus, 150.0f);

                for (Creature* l_Creature : l_Creatures)
                {
                    me->Kill(l_Creature);
                    l_Creature->DespawnOrUnsummon();
                }

                l_Creatures.clear();

                me->GetCreatureListWithEntryInGrid(l_Creatures, eCreatures::EarthenPillarStalker, 150.0f);

                for (Creature* l_Creature : l_Creatures)
                {
                    me->Kill(l_Creature);
                    l_Creature->DespawnOrUnsummon();
                }
            }

            void EnterEvadeMode(EvadeReason /*why*/) override
            {
                me->ClearUnitState(UnitState::UNIT_STATE_STUNNED);

                CreatureAI::EnterEvadeMode();

                if (m_Instance != nullptr)
                {
                    m_Instance->SetBossState(eHighmaulDatas::BossTectus, EncounterState::FAIL);

                    m_Instance->SendEncounterUnit(EncounterFrameType::ENCOUNTER_FRAME_DISENGAGE, me);

                    m_Instance->DoRemoveAurasDueToSpellOnPlayers(eSpells::SpellCrystallineBarrage);
                    m_Instance->DoRemoveAurasDueToSpellOnPlayers(eSpells::CrystallineBarrageDoT);
                }

                std::list<Creature*> l_Motes;
                me->GetCreatureListWithEntryInGrid(l_Motes, eCreatures::MoteOfTectus, 150.0f);

                for (Creature* l_Creature : l_Motes)
                {
                    if (l_Creature->IsAIEnabled)
                        l_Creature->AI()->Reset();

                    l_Creature->DespawnOrUnsummon();
                }

                me->RemoveAurasDueToSpell(HollowedHeartOfTheMountain);

                DespawnNightTwistedAdds();
                m_MoteKilled = 0;
            }

            void SpellHitTarget(Unit* p_Target, SpellInfo const* p_SpellInfo) override
            {
                if (p_Target == nullptr)
                    return;

                switch (p_SpellInfo->Id)
                {
                    case eSpells::FractureSearcher:
                        me->CastSpell(p_Target, eSpells::FractureMissile, true);
                        break;
                    case eSpells::Petrification:
                        if (m_CanEternalTalk)
                        {
                            m_CanEternalTalk = false;
                            Talk(eTalks::TectonicUpheavalCompleted);
                            AddTimedDelayedOperation(8 * TimeConstants::IN_MILLISECONDS, [this]() -> void { m_CanEternalTalk = true; });
                        }
                        if (me->GetEntry() == eHighmaulCreatures::Tectus && !me->HasAura(HollowedHeartOfTheMountain))
                            me->CastSpell(me, eSpells::TheLivingMountain, true);
                        else if (me->GetEntry() == eCreatures::ShardOfTectus)
                            me->CastSpell(me, eSpells::ShardOfTheMountain, true);
                        break;
                    case eSpells::SpellTectonicUpheaval:
                        m_TectonicScheduled = false;
                        break;
                    case eSpells::SpawnTectusShards:
                    {
                        p_Target->PlayOneShotAnimKitId(eAnimKits::AnimRise2);
                        p_Target->RestoreDisplayId();

                        ObjectGuid l_Guid = p_Target->GetGUID();
                        AddTimedDelayedOperation(4 * TimeConstants::IN_MILLISECONDS, [this, l_Guid]() -> void
                        {
                            if (Unit* l_Target = ObjectAccessor::GetUnit(*me, l_Guid))
                                l_Target->CastSpell(l_Target, eSpells::EarthFurySpawnDustCloud, true);
                        });

                        AddTimedDelayedOperation(6 * TimeConstants::IN_MILLISECONDS, [this, l_Guid]() -> void
                        {
                            if (Unit* l_Target = ObjectAccessor::GetUnit(*me, l_Guid))
                                l_Target->ClearUnitState(UnitState::UNIT_STATE_STUNNED);
                        });

                        break;
                    }
                    default:
                        break;
                }
            }

            void DamageTaken(Unit* p_Attacker, uint32& p_Damage) override
            {
                /// Prevent bug when boss instakills himself
                if (p_Attacker == me)
                    return;

                /// Prevent boss from dying too soon
                if (me->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE))
                {
                    p_Damage = 0;
                    return;
                }

                /// This buff cause Tectus to be unkillable, although he can still be damaged during this time.
                if (p_Damage > me->GetHealth())
                {
                    if (me->HasAura(eSpells::TheLivingMountain) || me->HasAura(eSpells::ShardOfTheMountain))
                    {
                        me->SetHealth(1);
                        p_Damage = 0;
                        return;
                    }
                    else
                    {
                        /// All shards in NM or mythic respawned
                        if (!IsMythic() || (me->GetEntry() == eHighmaulCreatures::Tectus && me->HasAura(HollowedHeartOfTheMountain))
                            || (me->GetEntry() == eCreatures::ShardOfTectus && me->HasAura(HollowedShardOfTheMountain)) || me->GetEntry() == eCreatures::MoteOfTectus)
                        {
                            m_Events.Reset();

                            me->RemoveAllAuras();
                            me->InterruptNonMeleeSpells(true);
                            me->RemoveAllAreaTriggers();
                            me->SetHealth(1);
                            p_Damage = 0;

                            me->CastSpell(me, eSpells::EncounterEvent, true);
                            me->CastSpell(me, eSpells::BreakPlayerTargetting, true);
                            me->CastSpell(me, eSpells::ZeroPowerZeroRegen, true);

                            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_IMMUNE_TO_PC);
                            me->SetFlag(UNIT_FIELD_FLAGS_2, UNIT_FLAG2_DISABLE_TURN);

                            me->AddUnitState(UnitState::UNIT_STATE_STUNNED);
                            me->SetAIAnimKitId(eAnimKits::AnimFall);

                            if (m_Instance)
                                m_Instance->SendEncounterUnit(EncounterFrameType::ENCOUNTER_FRAME_DISENGAGE, me);

                            if (IsMythic() && me->GetEntry() == eHighmaulCreatures::Tectus)
                            {
                                m_Events.ScheduleEvent(eEvents::EventSpawnBersererk, 18 * TimeConstants::IN_MILLISECONDS);
                                m_Events.ScheduleEvent(eEvents::EventSpawnEarthwarper, 8 * TimeConstants::IN_MILLISECONDS);
                            }

                            if (IsMythic() && (me->GetEntry() == eHighmaulCreatures::Tectus || me->GetEntry() == eCreatures::ShardOfTectus))
                                return;
                        }
                        else if (IsMythic() && (!me->HasAura(HollowedHeartOfTheMountain) && me->GetEntry() == eHighmaulCreatures::Tectus) ||
                            (!me->HasAura(HollowedShardOfTheMountain) && me->GetEntry() == eCreatures::ShardOfTectus))
                        {
                            me->CastStop();
                            me->InterruptNonMeleeSpells(true);
                            me->RemoveAllAreaTriggers();
                            me->SetAIAnimKitId(eAnimKits::AnimFall);
                            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_IMMUNE_TO_PC);
                            me->SetFlag(UNIT_FIELD_FLAGS_2, UNIT_FLAG2_DISABLE_TURN);
                            me->AddUnitState(UnitState::UNIT_STATE_STUNNED);

                            (me->GetEntry() == eHighmaulCreatures::Tectus) ? me->CastSpell(me, HollowedHeartOfTheMountain) :
                                me->CastSpell(me, HollowedShardOfTheMountain);

                            me->SetHealth(me->GetMaxHealth() * 0.2f);
                            p_Damage = 0;

                            me->CastSpell(me, eSpells::ZeroPowerZeroRegen, true);
                            me->RemoveAurasDueToSpell(TheLivingMountain);

                            AddTimedDelayedOperation(3000, [this]() -> void
                            {
                                me->ClearUnitState(UNIT_STATE_STUNNED);
                                me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_IMMUNE_TO_PC | UNIT_FLAG_STUNNED);
                                me->RemoveFlag(UNIT_FIELD_FLAGS_2, UNIT_FLAG2_DISABLE_TURN);
                                me->SetAIAnimKitId(eAnimKits::AnimRise);
                            });
                        }

                        if (me->GetEntry() == eHighmaulCreatures::Tectus)
                        {
                            Talk(eTalks::Shattered);
                            SpawnShards();
                            if (!IsMythic())
                                DespawnNightTwistedAdds();
                        }
                        else if (me->GetEntry() == eCreatures::ShardOfTectus)
                            SpawnMotes();
                        else
                        {
                            if (Creature* l_Tectus = ObjectAccessor::GetCreature(*me, m_Instance->GetGuidData(eHighmaulCreatures::Tectus)))
                                l_Tectus->AI()->DoAction(eActions::MoteKilled);
                        }
                    }
                }
            }

            void RegeneratePower(Powers p_Power, int32& p_Value) override
            {
                /// Tectus only regens by script
                p_Value = 0;
            }

            void UpdateAI(uint32 const p_Diff) override
            {
                UpdateOperations(p_Diff);

                if (!UpdateVictim())
                    return;

                m_Events.Update(p_Diff);

                if (me->HasUnitState(UnitState::UNIT_STATE_CASTING))
                    return;

                switch (m_Events.ExecuteEvent())
                {
                    case eEvents::EventEnrage:
                    {
                        me->CastSpell(me, eHighmaulSpells::Berserker, true);

                        std::list<Creature*> l_Creature;
                        me->GetCreatureListWithEntryInGrid(l_Creature, eCreatures::ShardOfTectus, 150.0f);

                        for (Creature* l_Shard : l_Creature)
                            l_Shard->CastSpell(l_Shard, eHighmaulSpells::Berserker, true);

                        l_Creature.clear();
                        me->GetCreatureListWithEntryInGrid(l_Creature, eCreatures::MoteOfTectus, 150.0f);

                        for (Creature* l_Mote : l_Creature)
                            l_Mote->CastSpell(l_Mote, eHighmaulSpells::Berserker, true);

                        break;
                    }
                    case eEvents::EventSpawnBersererk:
                    {
                        SpawnAdd(eCreatures::NightTwistedBerserker);
                        m_Events.ScheduleEvent(eEvents::EventSpawnBersererk, 40 * TimeConstants::IN_MILLISECONDS);
                        break;
                    }
                    case eEvents::EventSpawnEarthwarper:
                    {
                        SpawnAdd(eCreatures::NightTwistedEarthwarper);
                        m_Events.ScheduleEvent(eEvents::EventSpawnEarthwarper, 40 * TimeConstants::IN_MILLISECONDS);
                        break;
                    }
                    case eEvents::EventCrystallineBarrage:
                    {
                        Unit* l_Target = SelectTarget(SELECT_TARGET_FARTHEST, 0);

                        if (l_Target == nullptr)
                            l_Target = SelectTarget(SELECT_TARGET_RANDOM, 0, 60, true);

                        if (l_Target)
                        {
                            Talk(eTalks::CrystallineBarrage, l_Target);

                            me->CastSpell(l_Target, eSpells::SpellCrystallineBarrage, true);
                            m_CrystallineBarrageTarget = l_Target->GetGUID();

                            AddTimedDelayedOperation(1 * TimeConstants::IN_MILLISECONDS, [this]() -> void
                            {
                                Position pos = me->GetPosition();
                                me->CastSpell(pos, eSpells::CrystallineBarrageSummon, true);
                                m_FirstCrystalline = pos;

                                m_Events.ScheduleEvent(eEvents::EventSpawnCrystalline, 500);
                            });
                        }

                        m_Events.ScheduleEvent(eEvents::EventCrystallineBarrage, 30 * TimeConstants::IN_MILLISECONDS);
                        break;
                    }
                    case eEvents::EventSpawnCrystalline:
                    {
                        if (Unit* l_Target = ObjectAccessor::GetUnit(*me, m_CrystallineBarrageTarget))
                        {
                            if (!l_Target->HasAura(eSpells::SpellCrystallineBarrage))
                            {
                                m_CrystallineBarrageTarget = ObjectGuid::Empty;
                                m_FirstCrystalline = Position();
                                break;
                            }

                            float l_Range = 2.0f;
                            float l_Z = me->GetPositionZ();

                            float l_Orientation = m_FirstCrystalline.GetAngle(l_Target);
                            float l_X = m_FirstCrystalline.m_positionX + (l_Range * cos(l_Orientation));
                            float l_Y = m_FirstCrystalline.m_positionY + (l_Range * sin(l_Orientation));
                            me->CastSpell(l_X, l_Y, l_Z, eSpells::CrystallineBarrageSummon, true);

                            m_FirstCrystalline.m_positionX = l_X;
                            m_FirstCrystalline.m_positionY = l_Y;
                            m_FirstCrystalline.m_positionZ = l_Z;
                        }

                        m_Events.ScheduleEvent(eEvents::EventSpawnCrystalline, 500);
                        break;
                    }
                    case eEvents::EventFracture:
                    {
                        me->CastSpell(me, eSpells::FractureSearcher, true);
                        m_Events.ScheduleEvent(eEvents::EventFracture, 6 * TimeConstants::IN_MILLISECONDS);
                        break;
                    }
                    case eEvents::EventEarthenPillar:
                    {
                        Talk(eTalks::EarthenPillar);

                        if (Unit* l_Target = SelectTarget(SELECT_TARGET_FARTHEST, 0))
                            me->SummonCreature(eCreatures::EarthenPillarStalker, l_Target->GetPositionX(), l_Target->GetPositionY(), me->GetPositionZ());
                        // If no ranged damage dealer found, target random
                        else if (Unit* victim = SelectTarget(SELECT_TARGET_RANDOM, 0, 60, true))
                            me->SummonCreature(eCreatures::EarthenPillarStalker, victim->GetPositionX(), victim->GetPositionY(), me->GetPositionZ());

                        break;
                    }
                    case eEvents::EventAccretion:
                    {
                        me->CastSpell(me, eSpells::Accretion, true);
                        m_Events.ScheduleEvent(eEvents::EventAccretion, 5 * TimeConstants::IN_MILLISECONDS);
                        break;
                    }
                    case eEvents::EventTectonicUpheaval:
                    {
                        /// If Tectus's health is depleted during Tectonic Upheaval, he will Shatter.
                        Talk(eTalks::TectonicUpheaval);
                        me->RemoveAura(eSpells::TheLivingMountain);
                        me->RemoveAura(eSpells::ShardOfTheMountain);
                        me->CastSpell(me, eSpells::SpellTectonicUpheaval, false);

                        /// Delay summons
                        m_Events.DelayEvent(EventSpawnBersererk, 12 * IN_MILLISECONDS);
                        m_Events.DelayEvent(EventSpawnEarthwarper, 12 * IN_MILLISECONDS);
                        break;
                    }
                    default:
                        break;
                }

                DoMeleeAttackIfReady();
            }

            bool AllGardiansDead() const
            {
                if (m_Instance == nullptr)
                    return false;

                if (Creature* l_Rokka = ObjectAccessor::GetCreature(*me, m_Instance->GetGuidData(eHighmaulCreatures::Rokka)))
                {
                    if (l_Rokka->IsAlive())
                        return false;
                }

                if (Creature* l_Lokk = ObjectAccessor::GetCreature(*me, m_Instance->GetGuidData(eHighmaulCreatures::Lokk)))
                {
                    if (l_Lokk->IsAlive())
                        return false;
                }

                if (Creature* l_Oro = ObjectAccessor::GetCreature(*me, m_Instance->GetGuidData(eHighmaulCreatures::Oro)))
                {
                    if (l_Oro->IsAlive())
                        return false;
                }

                return true;
            }

            void SpawnShards()
            {
                float l_OrigX = me->GetPositionX();
                float l_OrigY = me->GetPositionY();
                float l_Z = me->GetPositionZ();
                float l_Range = 7.0f;

                for (uint8 l_I = 0; l_I < eMiscs::ShardSpawnCount; ++l_I)
                {
                    float l_Orientation = frand(0, 2 * M_PI);
                    float l_X = l_OrigX + (l_Range * cos(l_Orientation));
                    float l_Y = l_OrigY + (l_Range * sin(l_Orientation));

                    if (Creature* l_Shard = me->SummonCreature(eCreatures::ShardOfTectus, l_X, l_Y, l_Z))
                        me->CastSpell(l_Shard, eSpells::SpawnTectusShards, true);
                }
            }

            void SpawnMotes()
            {
                float l_OrigX = me->GetPositionX();
                float l_OrigY = me->GetPositionY();
                float l_Z = me->GetPositionZ();
                float l_Range = 7.0f;

                for (uint8 l_I = 0; l_I < eMiscs::MotesSpawnCount; ++l_I)
                {
                    float l_Orientation = frand(0, 2 * M_PI);
                    float l_X = l_OrigX + (l_Range * cos(l_Orientation));
                    float l_Y = l_OrigY + (l_Range * sin(l_Orientation));

                    if (Creature* l_Mote = me->SummonCreature(eCreatures::MoteOfTectus, l_X, l_Y, l_Z))
                        me->CastSpell(l_Mote, eSpells::SpawnTectusShards, true);
                }
            }

            void SpawnAdd(uint32 p_Entry)
            {
                if (Unit* l_Target = SelectTarget(SelectAggroTarget::SELECT_TARGET_FARTHEST, 0, 70.0f, true))
                {
                    float l_O = frand(0, 2 * M_PI);
                    float l_Range = 5.0f;
                    float l_X = l_Target->GetPositionX() + (l_Range * cos(l_O));
                    float l_Y = l_Target->GetPositionY() + (l_Range * sin(l_O));

                    if (Creature* l_Add = me->SummonCreature(p_Entry, l_X, l_Y, me->GetPositionZ()))
                    {
                        if (l_Add->IsAIEnabled)
                            l_Add->AI()->AttackStart(l_Target);
                    }
                }
            }

            void DespawnNightTwistedAdds()
            {
                std::list<Creature*> addsList;
                me->GetCreatureListWithEntryInGridAppend(addsList, NightTwistedBerserker, 500);
                me->GetCreatureListWithEntryInGridAppend(addsList, NightTwistedEarthwarper, 500);
                for (Creature *itr : addsList)
                    if (itr)
                        itr->DespawnOrUnsummon();
            }
        };

        CreatureAI* GetAI(Creature* p_Creature) const override
        {
            return new boss_tectusAI(p_Creature);
        }
};

/// Night-Twisted Supplicant - 86185
class npc_highmaul_night_twisted_supplicant : public CreatureScript
{
    public:
        npc_highmaul_night_twisted_supplicant() : CreatureScript("npc_highmaul_night_twisted_supplicant") { }

        enum eSpell
        {
            NightTwistedCovenant = 172138
        };

        enum eDisplay
        {
            InvisDisplay = 11686
        };

        enum eAction
        {
            Rise
        };

        struct npc_highmaul_night_twisted_supplicantAI : public ScriptedAI
        {
            npc_highmaul_night_twisted_supplicantAI(Creature* p_Creature) : ScriptedAI(p_Creature) { }

            void Reset() override
            {
                me->SetReactState(ReactStates::REACT_PASSIVE);
                me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_PC | UNIT_FLAG_NOT_SELECTABLE);
                SetCanSeeEvenInPassiveMode(true);
            }

            bool CanRespawn() override
            {
                /// This mob is only cosmetic for Rokka, Lokk and Oro spawn
                return false;
            }

            void SpellHitTarget(Unit* p_Target, SpellInfo const* p_SpellInfo)
            {
                if (p_Target == nullptr || p_SpellInfo->Id != eSpell::NightTwistedCovenant)
                    return;

                p_Target->SetDisplayId(eDisplay::InvisDisplay);
            }

            void MoveInLineOfSight(Unit* p_Mover) override
            {
                if (me->GetDisplayId() == eDisplay::InvisDisplay)
                    return;

                if (me->HasUnitState(UnitState::UNIT_STATE_CASTING))
                    return;

                if (p_Mover->GetTypeId() != TypeID::TYPEID_PLAYER)
                    return;

                std::list<Creature*> l_OtherMe;
                me->GetCreatureListWithEntryInGrid(l_OtherMe, me->GetEntry(), 100.0f);

                for (Creature* l_Covenant : l_OtherMe)
                    l_Covenant->CastSpell(l_Covenant, eSpell::NightTwistedCovenant, false);

                AddTimedDelayedOperation(5000, [this]() -> void
                {
                    if (Creature* l_Rokka = me->FindNearestCreature(eHighmaulCreatures::Rokka, 100.0f))
                        l_Rokka->AI()->DoAction(eAction::Rise);

                    if (Creature* l_Lokk = me->FindNearestCreature(eHighmaulCreatures::Lokk, 100.0f))
                        l_Lokk->AI()->DoAction(eAction::Rise);

                    if (Creature* l_Oro = me->FindNearestCreature(eHighmaulCreatures::Oro, 100.0f))
                        l_Oro->AI()->DoAction(eAction::Rise);
                });
            }
        };

        CreatureAI* GetAI(Creature* p_Creature) const override
        {
            return new npc_highmaul_night_twisted_supplicantAI(p_Creature);
        }
};

/// Rokkaa <Heart of Tectus> - 86071
/// Lokk <Hands of Tectus> - 86073
class npc_highmaul_rokka_and_lokk : public CreatureScript
{
    public:
        npc_highmaul_rokka_and_lokk() : CreatureScript("npc_highmaul_rokka_and_lokk") { }

        enum eAnimKit
        {
            AnimRise = 6961
        };

        enum eActions
        {
            Rise         = 0,
            GuardianDead = 0
        };

        enum eSpells
        {
            /// Grow scale, regens 10% of health every second
            Reconstitution      = 172116,
            ReconstitutionScale = 172126,
            /// AoE on caster
            EarthenThrust       = 172115,
            /// Trigger 172073 missile
            /// If no player hit, casts 172078
            MeteoricEarthspire  = 172110
        };

        enum eEvents
        {
            EventEarthenThrust = 1,
            EventMeteoricEarthspire,
            EventReconstitution
        };

        struct npc_highmaul_rokka_and_lokkAI : public ScriptedAI
        {
            npc_highmaul_rokka_and_lokkAI(Creature* p_Creature) : ScriptedAI(p_Creature)
            {
                m_Instance = p_Creature->GetInstanceScript();
                m_Risen = false;
            }

            bool m_Risen;

            EventMap m_Events;
            InstanceScript* m_Instance;

            void Reset() override
            {
                if (!m_Risen)
                {
                    me->SetReactState(ReactStates::REACT_PASSIVE);
                    me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_IMMUNE_TO_PC);
                }
                else
                    me->SetAIAnimKitId(0);

                m_Events.Reset();

                RespawnGuardians(me, m_Instance);
            }

            void JustReachedHome() override
            {
                if (m_Risen)
                    me->SetAIAnimKitId(0);
            }

            bool CanRespawn() override
            {
                return false;
            }

            void EnterCombat(Unit* p_Attacker) override
            {
                StartGuardians(me, p_Attacker);

                m_Events.ScheduleEvent(eEvents::EventEarthenThrust, 6 * TimeConstants::IN_MILLISECONDS);
                m_Events.ScheduleEvent(eEvents::EventMeteoricEarthspire, 10 * TimeConstants::IN_MILLISECONDS);
            }

            void DamageTaken(Unit* p_Attacker, uint32& p_Damage) override
            {
                if (me->HasAura(eSpells::ReconstitutionScale))
                    return;

                if (me->HealthBelowPctDamaged(21, p_Damage))
                    m_Events.RescheduleEvent(eEvents::EventReconstitution, 100);
            }

            void DoAction(int32 const p_Action) override
            {
                if (p_Action == eActions::Rise && !m_Risen)
                {
                    m_Risen = true;
                    me->SetAIAnimKitId(0);
                    me->PlayOneShotAnimKitId(eAnimKit::AnimRise);

                    AddTimedDelayedOperation(4500, [this]() -> void
                    {
                        me->SetReactState(ReactStates::REACT_AGGRESSIVE);
                        me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_IMMUNE_TO_PC);
                    });
                }
            }

            void JustDied(Unit* p_Killer) override
            {
                if (Creature* l_Tectus = me->FindNearestCreature(eHighmaulCreatures::Tectus, 100.0f))
                    l_Tectus->AI()->DoAction(eActions::GuardianDead);
            }

            void UpdateAI(uint32 const p_Diff) override
            {
                ScriptedAI::UpdateAI(p_Diff);

                if (!UpdateVictim())
                    return;

                m_Events.Update(p_Diff);

                if (me->HasUnitState(UnitState::UNIT_STATE_CASTING))
                    return;

                switch (m_Events.ExecuteEvent())
                {
                    case eEvents::EventEarthenThrust:
                        me->CastSpell(me, eSpells::EarthenThrust, false);
                        m_Events.ScheduleEvent(eEvents::EventEarthenThrust, 12 * TimeConstants::IN_MILLISECONDS);
                        break;
                    case eEvents::EventMeteoricEarthspire:
                        if (Unit* l_Target = SelectTarget(SelectAggroTarget::SELECT_TARGET_RANDOM))
                            me->CastSpell(l_Target, eSpells::MeteoricEarthspire, false);
                        m_Events.ScheduleEvent(eEvents::EventMeteoricEarthspire, 12 * TimeConstants::IN_MILLISECONDS);
                        break;
                    case eEvents::EventReconstitution:
                        me->CastSpell(me, eSpells::ReconstitutionScale, true);
                        me->CastSpell(me, eSpells::Reconstitution, false);
                        break;
                    default:
                        break;
                }

                DoMeleeAttackIfReady();
            }
        };

        CreatureAI* GetAI(Creature* p_Creature) const override
        {
            return new npc_highmaul_rokka_and_lokkAI(p_Creature);
        }
};

/// Oro <Wrath of Tectus> - 86072
class npc_highmaul_oro : public CreatureScript
{
    public:
        npc_highmaul_oro() : CreatureScript("npc_highmaul_oro") { }

        enum eAnimKit
        {
            AnimRise = 6961
        };

        enum eActions
        {
            Rise         = 0,
            GuardianDead = 0
        };

        enum eSpells
        {
            /// Grow scale, regens 10% of health every second
            Reconstitution      = 172116,
            ReconstitutionScale = 172126,
            /// Launches Stonebolt at targets further than 5 yards
            StoneboltVolley     = 172058,
            /// Poisons a player, inflicting damage to his allies
            RadiatingPoison     = 172066
        };

        enum eEvents
        {
            EventStoneboltVolley = 1,
            EventRadiatingPoison,
            EventReconstitution
        };

        struct npc_highmaul_oroAI : public ScriptedAI
        {
            npc_highmaul_oroAI(Creature* p_Creature) : ScriptedAI(p_Creature)
            {
                m_Instance = p_Creature->GetInstanceScript();
                m_Risen = false;
            }

            bool m_Risen;

            EventMap m_Events;
            InstanceScript* m_Instance;

            void Reset() override
            {
                if (!m_Risen)
                {
                    me->SetReactState(ReactStates::REACT_PASSIVE);
                    me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_IMMUNE_TO_PC);
                }
                else
                    me->SetAIAnimKitId(0);

                m_Events.Reset();

                RespawnGuardians(me, m_Instance);
            }

            void JustReachedHome() override
            {
                if (m_Risen)
                    me->SetAIAnimKitId(0);
            }

            bool CanRespawn() override
            {
                return false;
            }

            void EnterCombat(Unit* p_Attacker) override
            {
                StartGuardians(me, p_Attacker);

                m_Events.ScheduleEvent(eEvents::EventStoneboltVolley, 6 * TimeConstants::IN_MILLISECONDS);
                m_Events.ScheduleEvent(eEvents::EventRadiatingPoison, 10 * TimeConstants::IN_MILLISECONDS);
            }

            void DoAction(int32 const p_Action) override
            {
                if (p_Action == eActions::Rise && !m_Risen)
                {
                    m_Risen = true;
                    me->SetAIAnimKitId(0);
                    me->PlayOneShotAnimKitId(eAnimKit::AnimRise);

                    AddTimedDelayedOperation(4500, [this]() -> void
                    {
                        me->SetReactState(ReactStates::REACT_AGGRESSIVE);
                        me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_IMMUNE_TO_PC);
                    });
                }
            }

            void DamageTaken(Unit* p_Attacker, uint32& p_Damage) override
            {
                if (m_Events.HasEvent(eEvents::EventReconstitution) || me->HasAura(eSpells::ReconstitutionScale))
                    return;

                if (me->HealthBelowPctDamaged(21, p_Damage))
                    m_Events.ScheduleEvent(eEvents::EventReconstitution, 100);
            }

            void JustDied(Unit* p_Killer) override
            {
                if (Creature* l_Tectus = me->FindNearestCreature(eHighmaulCreatures::Tectus, 100.0f))
                    l_Tectus->AI()->DoAction(eActions::GuardianDead);
            }

            void UpdateAI(uint32 const p_Diff) override
            {
                ScriptedAI::UpdateAI(p_Diff);

                if (!UpdateVictim())
                    return;

                m_Events.Update(p_Diff);

                if (me->HasUnitState(UnitState::UNIT_STATE_CASTING))
                    return;

                switch (m_Events.ExecuteEvent())
                {
                    case eEvents::EventStoneboltVolley:
                        me->CastSpell(me, eSpells::StoneboltVolley, false);
                        m_Events.ScheduleEvent(eEvents::EventStoneboltVolley, 12 * TimeConstants::IN_MILLISECONDS);
                        break;
                    case eEvents::EventRadiatingPoison:
                        if (Unit* l_Target = SelectTarget(SelectAggroTarget::SELECT_TARGET_RANDOM))
                            me->CastSpell(l_Target, eSpells::RadiatingPoison, false);
                        m_Events.ScheduleEvent(eEvents::EventRadiatingPoison, 12 * TimeConstants::IN_MILLISECONDS);
                        break;
                    case eEvents::EventReconstitution:
                        me->CastSpell(me, eSpells::ReconstitutionScale, true);
                        me->CastSpell(me, eSpells::Reconstitution, false);
                        break;
                    default:
                        break;
                }

                DoMeleeAttackIfReady();
            }
        };

        CreatureAI* GetAI(Creature* p_Creature) const override
        {
            return new npc_highmaul_oroAI(p_Creature);
        }
};

/// Earthen Pillar Stalker - 80476
class npc_highmaul_earthen_pillar_stalker : public CreatureScript
{
    public:
        npc_highmaul_earthen_pillar_stalker() : CreatureScript("npc_highmaul_earthen_pillar_stalker") { }

        struct npc_highmaul_earthen_pillar_stalkerAI: public ScriptedAI
        {
            npc_highmaul_earthen_pillar_stalkerAI(Creature* p_Creature) : ScriptedAI(p_Creature) {}

            ObjectGuid m_PillarGuid;

            enum eSpells
            {
                EarthenPillarTimer  = 166024,
                EarthenPillarKill   = 162522
            };

            enum eGameObject
            {
                GoBEarthenPillar = 229649
            };

            void Reset() override
            {
                me->SetReactState(ReactStates::REACT_PASSIVE);
                me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_PC | UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_NON_ATTACKABLE);

                me->AddUnitState(UnitState::UNIT_STATE_ROOT);

                me->CastSpell(me, eSpells::EarthenPillarTimer, true);
            }

            void DoAction(int32 const p_Action) override
            {
                if (p_Action)
                    return;

                me->CastSpell(me, eSpells::EarthenPillarKill, true);

                if (GameObject* l_Pillar = me->SummonGameObject(eGameObject::GoBEarthenPillar, *me, QuaternionData(), 0))
                {
                    //l_Pillar->EnableCollision(false);
                    m_PillarGuid = l_Pillar->GetGUID();
                }

                AddTimedDelayedOperation(60 * TimeConstants::IN_MILLISECONDS, [this]() -> void
                {
                    if (GameObject* l_Pillar = ObjectAccessor::GetGameObject(*me, m_PillarGuid))
                    {
                        l_Pillar->EnableCollision(false);
                        l_Pillar->Delete();
                    }

                    me->DespawnOrUnsummon();
                });
            }
        };

        CreatureAI* GetAI(Creature* p_Creature) const override
        {
            return new npc_highmaul_earthen_pillar_stalkerAI(p_Creature);
        }
};

/// Night-Twisted Berserker - 80822
class npc_highmaul_night_twisted_berserker : public CreatureScript
{
    public:
        npc_highmaul_night_twisted_berserker() : CreatureScript("npc_highmaul_night_twisted_berserker") { }

        struct npc_highmaul_night_twisted_berserkerAI : public ScriptedAI
        {
            npc_highmaul_night_twisted_berserkerAI(Creature* p_Creature) : ScriptedAI(p_Creature) { }

            enum eSpells
            {
                EncounterSpawn  = 181113,
                RavingAssault   = 163312
            };

            enum eEvent
            {
                EventRavingAssault = 1
            };

            enum eTalk
            {
                Aggro
            };

            EventMap m_Events;

            void Reset() override
            {
                me->CastSpell(me, eSpells::EncounterSpawn, true);

                m_Events.Reset();
            }

            void EnterCombat(Unit* p_Attacker) override
            {
                Talk(eTalk::Aggro);

                m_Events.ScheduleEvent(eEvent::EventRavingAssault, 8 * TimeConstants::IN_MILLISECONDS);
            }

            void SpellHitTarget(Unit* p_Victim, SpellInfo const* p_SpellInfo) override
            {
                if (p_Victim == nullptr || p_Victim == me->ToUnit() || p_SpellInfo->Id != eSpells::RavingAssault)
                    return;

                Position l_Pos  = *p_Victim;
                float l_Angle   = me->GetRelativeAngle(l_Pos.GetPositionX(), l_Pos.GetPositionY());
                float l_Dist    = me->GetDistance(l_Pos);

                l_Pos = me->GetFirstCollisionPosition(l_Dist, l_Angle);
                me->GetMotionMaster()->MoveCharge(&l_Pos, SPEED_CHARGE, eSpells::RavingAssault);
            }

            void MovementInform(uint32 p_Type, uint32 p_ID) override
            {
                if (p_ID != eSpells::RavingAssault)
                    return;

                me->RemoveAura(eSpells::RavingAssault);
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
                    case eEvent::EventRavingAssault:
                    {
                        if (Unit* l_Target = SelectTarget(SelectAggroTarget::SELECT_TARGET_RANDOM))
                            me->CastSpell(l_Target, eSpells::RavingAssault, false);
                        m_Events.ScheduleEvent(eEvent::EventRavingAssault, 20 * TimeConstants::IN_MILLISECONDS);
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
            return new npc_highmaul_night_twisted_berserkerAI(p_Creature);
        }
};

/// Night-Twisted Earthwarper - 80599
class npc_highmaul_night_twisted_earthwarper : public CreatureScript
{
    public:
        npc_highmaul_night_twisted_earthwarper() : CreatureScript("npc_highmaul_night_twisted_earthwarper") { }

        struct npc_highmaul_night_twisted_earthwarperAI : public ScriptedAI
        {
            npc_highmaul_night_twisted_earthwarperAI(Creature* p_Creature) : ScriptedAI(p_Creature) { }

            enum eSpells
            {
                EncounterSpawn      = 181113,
                GiftOfEarth         = 162894,
                EarthenFlechettes   = 162968
            };

            enum eEvents
            {
                EventEarthenFlechettes = 1,
                EventGiftOfEarth
            };

            enum eTalk
            {
                Aggro
            };

            EventMap m_Events;

            void Reset() override
            {
                me->CastSpell(me, eSpells::EncounterSpawn, true);

                m_Events.Reset();
            }

            void EnterCombat(Unit* p_Attacker) override
            {
                Talk(eTalk::Aggro);

                m_Events.ScheduleEvent(eEvents::EventEarthenFlechettes, 5 * TimeConstants::IN_MILLISECONDS);
                m_Events.ScheduleEvent(eEvents::EventGiftOfEarth, 12 * TimeConstants::IN_MILLISECONDS);
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
                    case eEvents::EventEarthenFlechettes:
                        me->CastSpell(me, eSpells::EarthenFlechettes, false);
                        m_Events.ScheduleEvent(eEvents::EventEarthenFlechettes, 15 * TimeConstants::IN_MILLISECONDS);
                        break;
                    case eEvents::EventGiftOfEarth:
                        me->CastSpell(me, eSpells::GiftOfEarth, false);
                        m_Events.ScheduleEvent(eEvents::EventGiftOfEarth, 25 * TimeConstants::IN_MILLISECONDS);
                        break;
                    default:
                        break;
                }

                DoMeleeAttackIfReady();
            }
        };

        CreatureAI* GetAI(Creature* p_Creature) const override
        {
            return new npc_highmaul_night_twisted_earthwarperAI(p_Creature);
        }
};

/// Meteoric Earthspire - 172073
class spell_highmaul_meteoric_earthspire : public SpellScriptLoader
{
    public:
        spell_highmaul_meteoric_earthspire() : SpellScriptLoader("spell_highmaul_meteoric_earthspire") { }

        class spell_highmaul_meteoric_earthspire_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_highmaul_meteoric_earthspire_SpellScript);

            enum eSpell
            {
                RupturedEarth = 172078
            };

            void CorrectTargets(std::list<WorldObject*>& p_Targets)
            {
                if (!p_Targets.empty())
                    return;

                if (Unit* l_Caster = GetCaster())
                {
                    if (WorldLocation const* l_Location = GetExplTargetDest())
                        l_Caster->CastSpell(*l_Location, eSpell::RupturedEarth, true);
                }
            }

            void Register() override
            {
                OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_highmaul_meteoric_earthspire_SpellScript::CorrectTargets, EFFECT_0, TARGET_UNIT_DEST_AREA_ENEMY);
            }
        };

        SpellScript* GetSpellScript() const
        {
            return new spell_highmaul_meteoric_earthspire_SpellScript();
        }
};

/// Stonebolt Volley - 172058
class spell_highmaul_stonebolt_volley : public SpellScriptLoader
{
    public:
        spell_highmaul_stonebolt_volley() : SpellScriptLoader("spell_highmaul_stonebolt_volley") { }

        class spell_highmaul_stonebolt_volley_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_highmaul_stonebolt_volley_SpellScript);

            enum eSpell
            {
                RupturedEarth = 172078
            };

            void CorrectTargets(std::list<WorldObject*>& p_Targets)
            {
                if (p_Targets.empty())
                    return;

                Unit* l_Caster = GetCaster();
                if (l_Caster == nullptr)
                    return;

                p_Targets.remove_if([this, l_Caster](WorldObject* p_Object) -> bool
                {
                    if (p_Object == nullptr)
                        return true;

                    if (p_Object->GetDistance(l_Caster) <= 5.0f)
                        return true;

                    return false;
                });
            }

            void Register() override
            {
                OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_highmaul_stonebolt_volley_SpellScript::CorrectTargets, EFFECT_0, TARGET_UNIT_DEST_AREA_ENEMY);
            }
        };

        SpellScript* GetSpellScript() const
        {
            return new spell_highmaul_stonebolt_volley_SpellScript();
        }
};

/// The Living Mountain - 162287
/// Shard of the Mountain - 162658
/// Mote of the Mountain - 162674
class spell_highmaul_tectus_energy_gain : public SpellScriptLoader
{
    public:
        spell_highmaul_tectus_energy_gain() : SpellScriptLoader("spell_highmaul_tectus_energy_gain") { }

        class spell_highmaul_tectus_energy_gain_AuraScript : public AuraScript
        {
            PrepareAuraScript(spell_highmaul_tectus_energy_gain_AuraScript);

            enum eActions
            {
                ScheduleEarthenPillar = 1,
                ScheduleTectonicUpheaval
            };

            void OnTick(AuraEffect const* p_AurEff)
            {
                if (Creature* l_Target = GetTarget()->ToCreature())
                {
                    if (!l_Target->IsInCombat())
                        return;

                    uint32 l_OldPower = l_Target->GetPower(Powers::POWER_ENERGY);
                    int32 l_PowerGain = GetEnergyGainFromHealth(l_Target->GetHealthPct());
                    l_Target->EnergizeBySpell(l_Target, GetSpellInfo()->Id, l_PowerGain, Powers::POWER_ENERGY);
                    uint32 l_NewPower = l_OldPower + l_PowerGain;

                    if (l_Target->IsAIEnabled)
                    {
                        if (l_NewPower >= 100)
                            l_Target->AI()->DoAction(eActions::ScheduleTectonicUpheaval);
                        else
                        {
                            /// On Mythic difficulty, Tectus also uses this ability at 50 Energy.
                            if (l_Target->GetMap()->IsMythic())
                            {
                                if ((l_OldPower < 25 && l_NewPower >= 25) ||
                                    (l_OldPower < 50 && l_NewPower >= 50))
                                    l_Target->AI()->DoAction(eActions::ScheduleEarthenPillar);
                            }
                            else
                            {
                                if (l_OldPower < 25 && l_NewPower >= 25)
                                    l_Target->AI()->DoAction(eActions::ScheduleEarthenPillar);
                            }
                        }
                    }
                }
            }

            void Register() override
            {
                OnEffectPeriodic += AuraEffectPeriodicFn(spell_highmaul_tectus_energy_gain_AuraScript::OnTick, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
            }
        };

        AuraScript* GetAuraScript() const override
        {
            return new spell_highmaul_tectus_energy_gain_AuraScript();
        }
};

/// Earthen Pillar (timer) - 166024
class spell_highmaul_earthen_pillar_timer : public SpellScriptLoader
{
    public:
        spell_highmaul_earthen_pillar_timer() : SpellScriptLoader("spell_highmaul_earthen_pillar_timer") { }

        class spell_highmaul_earthen_pillar_timer_AuraScript : public AuraScript
        {
            PrepareAuraScript(spell_highmaul_earthen_pillar_timer_AuraScript);

            void OnRemove(AuraEffect const* p_AurEff, AuraEffectHandleModes p_Mode)
            {
                if (Creature* l_Target = GetTarget()->ToCreature())
                {
                    if (l_Target->IsAIEnabled)
                        l_Target->AI()->DoAction(0);
                }
            }

            void Register() override
            {
                OnEffectRemove += AuraEffectRemoveFn(spell_highmaul_earthen_pillar_timer_AuraScript::OnRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
            }
        };

        AuraScript* GetAuraScript() const override
        {
            return new spell_highmaul_earthen_pillar_timer_AuraScript();
        }
};

/// Accretion - 162288
class spell_highmaul_accretion : public SpellScriptLoader
{
    public:
        spell_highmaul_accretion() : SpellScriptLoader("spell_highmaul_accretion") { }

        class spell_highmaul_accretion_AuraScript : public AuraScript
        {
            PrepareAuraScript(spell_highmaul_accretion_AuraScript);

            uint32 m_DamageTaken;

            enum eCreatures
            {
                ShardOfTectus   = 80551,
                MoteOfTectus    = 80557
            };

            bool Load()
            {
                m_DamageTaken = 0;
                return true;
            }

            void OnProc(AuraEffect const* p_AurEff, ProcEventInfo& p_EventInfo)
            {
                PreventDefaultAction();

                if (Unit* l_Target = GetTarget())
                {
                    m_DamageTaken += p_EventInfo.GetDamageInfo()->GetDamage();

                    if (m_DamageTaken >= l_Target->CountPctFromMaxHealth(2))
                    {
                        p_AurEff->GetBase()->ModStackAmount(-1);
                        m_DamageTaken = 0;
                    }
                }
            }

            /*

            // @Use this only if tectus didn't buff allies 
            void HandleMythicAllyBuffing(AuraEffect const* aurEff, AuraEffectHandleModes mode)
            {
                // trigger ally buffing in MM only if the caster is buffing himself !
                // occurs when gaining stacks from EventAccretion
                if(GetCaster() != nullptr && GetTarget() != nullptr)
                    if(GetCaster()->GetMap()->IsMythic())
                        if (GetCaster()->GetGUID() == GetTarget()->GetGUID())
                        {
                            Creature *caster = GetCaster()->ToCreature();
                            if (caster == nullptr)
                                return;

                            std::list<Creature*> AlliesToBuff;
                            caster->GetCreatureListWithEntryInGridAppend(AlliesToBuff, Tectus, 500);
                            caster->GetCreatureListWithEntryInGridAppend(AlliesToBuff, ShardOfTectus, 500);
                            caster->GetCreatureListWithEntryInGridAppend(AlliesToBuff, MoteOfTectus, 500);

                            for (Creature *itr : AlliesToBuff)
                                if (itr->GetGUID() != caster->GetGUID())
                                    caster->CastSpell(itr, GetSpellInfo()->Id);
                        }
            }
            */
            void Register() override
            {
                //OnEffectApply += AuraEffectApplyFn(spell_highmaul_accretion_AuraScript::HandleMythicAllyBuffing, EFFECT_0, SPELL_AURA_MOD_DAMAGE_PERCENT_DONE, AURA_EFFECT_HANDLE_REAL_OR_REAPPLY_MASK);
               // OnEffectApply += AuraEffectApplyFn(spell_highmaul_accretion_AuraScript::HandleMythicAllyBuffing, EFFECT_0, SPELL_AURA_MOD_DAMAGE_PERCENT_DONE, AURA_EFFECT_HANDLE_CHANGE_AMOUNT);
                OnEffectProc += AuraEffectProcFn(spell_highmaul_accretion_AuraScript::OnProc, EFFECT_0, SPELL_AURA_MOD_DAMAGE_PERCENT_DONE);
            }
        };

        AuraScript* GetAuraScript() const override
        {
            return new spell_highmaul_accretion_AuraScript();
        }
};

/// Tectonic Upheaval - 162475
class spell_highmaul_tectonic_upheaval : public SpellScriptLoader
{
    public:
        spell_highmaul_tectonic_upheaval() : SpellScriptLoader("spell_highmaul_tectonic_upheaval") { }

        class spell_highmaul_tectonic_upheaval_AuraScript : public AuraScript
        {
            PrepareAuraScript(spell_highmaul_tectonic_upheaval_AuraScript);

            enum eSpell
            {
                Petrification = 163809
            };

            void OnTick(AuraEffect const* p_AurEff)
            {
                if (Unit* l_Target = GetTarget())
                    l_Target->EnergizeBySpell(l_Target, GetSpellInfo()->Id, -10, Powers::POWER_ENERGY);
            }

            void OnRemove(AuraEffect const* p_AurEff, AuraEffectHandleModes p_Mode)
            {
                if (Unit* l_Target = GetTarget())
                {
                    AuraRemoveMode l_RemoveMode = GetTargetApplication()->GetRemoveMode();
                    if (l_RemoveMode != AuraRemoveMode::AURA_REMOVE_BY_CANCEL)
                        l_Target->CastSpell(l_Target, eSpell::Petrification, true);

                    l_Target->SetPower(Powers::POWER_ENERGY, 0);
                }
            }

            void Register() override
            {
                OnEffectPeriodic += AuraEffectPeriodicFn(spell_highmaul_tectonic_upheaval_AuraScript::OnTick, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
                OnEffectRemove += AuraEffectRemoveFn(spell_highmaul_tectonic_upheaval_AuraScript::OnRemove, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
            }
        };

        AuraScript* GetAuraScript() const override
        {
            return new spell_highmaul_tectonic_upheaval_AuraScript();
        }
};

/// Highmaul Raid - Earth Fury - Spawn Dust Cloud - 169949
class spell_highmaul_spawn_dust_cloud : public SpellScriptLoader
{
    public:
        spell_highmaul_spawn_dust_cloud() : SpellScriptLoader("spell_highmaul_spawn_dust_cloud") { }

        class spell_highmaul_spawn_dust_cloud_AuraScript : public AuraScript
        {
            PrepareAuraScript(spell_highmaul_spawn_dust_cloud_AuraScript);

            void OnRemove(AuraEffect const* p_AurEff, AuraEffectHandleModes p_Mode)
            {
                if (GetTarget() == nullptr)
                    return;

                if (Creature* l_Target = GetTarget()->ToCreature())
                {
                    l_Target->SetReactState(ReactStates::REACT_AGGRESSIVE);
                    l_Target->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_IMMUNE_TO_PC);
                }
            }

            void Register() override
            {
                OnEffectRemove += AuraEffectRemoveFn(spell_highmaul_spawn_dust_cloud_AuraScript::OnRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
            }
        };

        AuraScript* GetAuraScript() const override
        {
            return new spell_highmaul_spawn_dust_cloud_AuraScript();
        }
};

/// Earthen Flechettes - 162968
class spell_highmaul_earthen_flechettes : public SpellScriptLoader
{
    public:
        spell_highmaul_earthen_flechettes() : SpellScriptLoader("spell_highmaul_earthen_flechettes") { }

        class spell_highmaul_earthen_flechettes_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_highmaul_earthen_flechettes_SpellScript);

            enum eSpell
            {
                TargetRestrict = 22531
            };

            void CorrectTargets(std::list<WorldObject*>& p_Targets)
            {
                if (p_Targets.empty())
                    return;

                SpellTargetRestrictionsEntry const* l_Restriction = sSpellTargetRestrictionsStore.LookupEntry(eSpell::TargetRestrict);
                if (l_Restriction == nullptr)
                    return;

                Unit* l_Caster = GetCaster();
                if (l_Caster == nullptr)
                    return;

                float l_Angle = 2 * float(M_PI) / 360 * l_Restriction->ConeDegrees;
                p_Targets.remove_if([l_Caster, l_Angle](WorldObject* p_Object) -> bool
                {
                    if (p_Object == nullptr)
                        return true;

                    if (!p_Object->isInFront(l_Caster, l_Angle))
                        return true;

                    return false;
                });
            }

            void Register() override
            {
                OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_highmaul_earthen_flechettes_SpellScript::CorrectTargets, EFFECT_0, TARGET_UNIT_CONE_ENEMY_104);
            }
        };

        SpellScript* GetSpellScript() const
        {
            return new spell_highmaul_earthen_flechettes_SpellScript();
        }
};

/// Petrification - 162892
class spell_highmaul_petrification : public SpellScriptLoader
{
    public:
        spell_highmaul_petrification() : SpellScriptLoader("spell_highmaul_petrification") { }

        class spell_highmaul_petrification_AuraScript : public AuraScript
        {
            PrepareAuraScript(spell_highmaul_petrification_AuraScript);

            enum eSpell
            {
                Petrification = 163809
            };

            void OnProc(AuraEffect const* p_AurEff, ProcEventInfo& p_EventInfo)
            {
                PreventDefaultAction();

                if (Unit* l_Target = GetTarget())
                {
                    if (Unit* l_Attacker = p_EventInfo.GetActor())
                    {
                        if (l_Attacker->GetEntry() != eHighmaulCreatures::Tectus)
                            return;

                        l_Attacker->CastSpell(l_Attacker, eSpell::Petrification, true);
                    }
                }
            }

            void Register() override
            {
                OnEffectProc += AuraEffectProcFn(spell_highmaul_petrification_AuraScript::OnProc, EFFECT_0, SPELL_AURA_MOD_DECREASE_SPEED);
            }
        };

        AuraScript* GetAuraScript() const override
        {
            return new spell_highmaul_petrification_AuraScript();
        }
};

/// Raving Assault - 163312
class spell_highmaul_raving_assault : public SpellScriptLoader
{
    public:
        spell_highmaul_raving_assault() : SpellScriptLoader("spell_highmaul_raving_assault") { }

        class spell_highmaul_raving_assault_AuraScript : public AuraScript
        {
            PrepareAuraScript(spell_highmaul_raving_assault_AuraScript);

            enum eSpell
            {
                RavingAssaultDamage = 163318
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
                        if (Unit* l_Caster = GetCaster())
                        {
                            std::list<Unit*> l_TargetList;
                            float l_Radius = 1.0f;

                            Trinity::AnyUnfriendlyUnitInObjectRangeCheck l_Check(l_Caster, l_Caster, l_Radius);
                            Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> l_Searcher(l_Caster, l_TargetList, l_Check);
                            Cell::VisitAllObjects(l_Caster, l_Searcher, l_Radius);

                            for (Unit* l_Iter : l_TargetList)
                                l_Caster->CastSpell(l_Iter, eSpell::RavingAssaultDamage, true);
                        }

                        m_DamageTimer = 500;
                    }
                    else
                        m_DamageTimer -= p_Diff;
                }
            }

            void Register() override
            {
                OnAuraUpdate += AuraUpdateFn(spell_highmaul_raving_assault_AuraScript::OnUpdate);
            }
        };

        AuraScript* GetAuraScript() const override
        {
            return new spell_highmaul_raving_assault_AuraScript();
        }
};

/// Crystalline Barrage - 162371
class areatrigger_highmaul_crystalline_barrage : public AreaTriggerAI
{
public:
    areatrigger_highmaul_crystalline_barrage(AreaTrigger* areatrigger) : AreaTriggerAI(areatrigger) { }

    enum eSpell
    {
        CrystallineBarrage = 162370
    };

    void OnUpdate(uint32 p_Time) override
    {
        if (Unit* l_Caster = at->GetCaster())
        {
            std::list<Unit*> l_TargetList;
            float l_Radius = 10.0f;

            Trinity::AnyUnfriendlyUnitInObjectRangeCheck l_Check(at, l_Caster, l_Radius);
            Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> l_Searcher(at, l_TargetList, l_Check);

            Cell::VisitAllObjects(at, l_Searcher, l_Radius);

            for (Unit* l_Unit : l_TargetList)
            {
                if (l_Unit->GetDistance(at) <= 2.0f)
                {
                    if (!l_Unit->HasAura(eSpell::CrystallineBarrage))
                        l_Caster->CastSpell(l_Unit, eSpell::CrystallineBarrage, true);
                }
                /*else if (!l_Unit->SelectNearestAreaTrigger(at->GetSpellId(), 2.0f))
                {
                    if (l_Unit->HasAura(eSpell::CrystallineBarrage))
                        l_Unit->RemoveAura(eSpell::CrystallineBarrage);
                }*/
            }
        }
    }
        
    void OnRemove() override
    {
        if (Unit* l_Caster = at->GetCaster())
        {
            std::list<Unit*> l_TargetList;
            float l_Radius = 10.0f;

            Trinity::AnyUnfriendlyUnitInObjectRangeCheck l_Check(at, l_Caster, l_Radius);
            Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> l_Searcher(at, l_TargetList, l_Check);
            Cell::VisitAllObjects(at, l_Searcher, l_Radius);
                
            for (Unit* l_Unit : l_TargetList)
            {
                /*if (!l_Unit->SelectNearestAreaTrigger(at->GetSpellId(), 2.0f))
                {
                    if (l_Unit->HasAura(eSpell::CrystallineBarrage))
                        l_Unit->RemoveAura(eSpell::CrystallineBarrage);
                }*/
            }
        }
    }
};

/// Gift of Earth - 162894
class areatrigger_highmaul_gift_of_earth : public AreaTriggerAI
{
public:
    areatrigger_highmaul_gift_of_earth(AreaTrigger* areatrigger) : AreaTriggerAI(areatrigger) { }

    enum eSpells
    {
        Accretion       = 162288,
        Petrification   = 162892
    };

    void OnCreate() override
    {
        if (Unit* l_Tectus = at->GetCaster()->FindNearestCreature(eHighmaulCreatures::Tectus, 200.0f))
            at->SetDestination(*l_Tectus, 5000);
    }

    void OnUpdate(uint32 p_Time) override
    {
        if (Unit* l_Caster = at->GetCaster())
        {
            if (Player* l_Target = at->SelectNearestPlayer(1.0f))
            {
                l_Caster->CastSpell(l_Target, eSpells::Petrification, true);
                at->SetDuration(1);
            }
            else if (Creature* l_Tectus = at->FindNearestCreature(eHighmaulCreatures::Tectus, 1.0f))
            {
                at->SetDuration(1);

                if (Aura* l_Accretion = l_Tectus->GetAura(eSpells::Accretion) ? l_Tectus->GetAura(eSpells::Accretion) : l_Tectus->AddAura(eSpells::Accretion, l_Tectus))
                {
                    l_Accretion->ModStackAmount(10);
                    l_Accretion->RefreshDuration();
                }
            }
            else if (Creature* l_Tectus = at->FindNearestCreature(eHighmaulCreatures::Tectus, 100.0f))
                at->SetDestination(*l_Tectus, 5000);
        }
    }
};

void AddSC_boss_tectus()
{
    /// Boss
    new boss_tectus();

    /// NPCs
    new npc_highmaul_night_twisted_supplicant();
    new npc_highmaul_rokka_and_lokk();
    new npc_highmaul_oro();
    new npc_highmaul_earthen_pillar_stalker();
    new npc_highmaul_night_twisted_berserker();
    new npc_highmaul_night_twisted_earthwarper();

    /// Spells
    new spell_highmaul_meteoric_earthspire();
    new spell_highmaul_stonebolt_volley();
    new spell_highmaul_tectus_energy_gain();
    new spell_highmaul_earthen_pillar_timer();
    new spell_highmaul_accretion();
    new spell_highmaul_tectonic_upheaval();
    new spell_highmaul_spawn_dust_cloud();
    new spell_highmaul_earthen_flechettes();
    new spell_highmaul_petrification();
    new spell_highmaul_raving_assault();

    /// AreaTriggers
    RegisterAreaTriggerAI(areatrigger_highmaul_crystalline_barrage);
    RegisterAreaTriggerAI(areatrigger_highmaul_gift_of_earth);
}
