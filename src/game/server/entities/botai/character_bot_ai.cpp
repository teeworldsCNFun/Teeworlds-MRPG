/* (c) Alexandre Díaz. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <new>
#include <engine/shared/config.h>
#include <game/server/gamecontext.h>
#include <game/mapitems.h>
#include "character_bot_ai.h"
#include <game/server/playerbot.h>

#include <game/server/mmocore/GameEntities/Skills/healthturret/hearth.h>

MACRO_ALLOC_POOL_ID_IMPL(CCharacterBotAI, MAX_CLIENTS*COUNT_WORLD+MAX_CLIENTS)

CCharacterBotAI::CCharacterBotAI(CGameWorld *pWorld) : CCharacter(pWorld) {}

CCharacterBotAI::~CCharacterBotAI() 
{
	RemoveSnapProj(100, GetSnapFullID());
	RemoveSnapProj(100, GetSnapFullID(), true);
}

int CCharacterBotAI::GetSnapFullID() const { return m_pBotPlayer->GetCID() * SNAPBOTS; }

bool CCharacterBotAI::Spawn(class CPlayer *pPlayer, vec2 Pos)
{
	m_pBotPlayer = static_cast<CPlayerBot*>(pPlayer);
	if(!m_pBotPlayer)
		return false;

	if(!CCharacter::Spawn(m_pBotPlayer, Pos))
		return false;

	ClearTarget();

	// mob information
	const int SubBotID = m_pBotPlayer->GetBotSub();
	if(m_pBotPlayer->GetBotType() == BotsTypes::TYPE_BOT_MOB && BotJob::ms_aMobBot[SubBotID].m_Boss)
	{
		for(int i = 0; i < 3; i++)
		{
			CreateSnapProj(GetSnapFullID(), 1, PICKUP_HEALTH, true, false);
			CreateSnapProj(GetSnapFullID(), 1, WEAPON_HAMMER, false, true);
		}
		if (!GS()->IsDungeon())
			GS()->ChatWorldID(BotJob::ms_aMobBot[SubBotID].m_WorldID, "", "In your zone emerging {STR}!", BotJob::ms_aMobBot[SubBotID].GetName());
	}
	else if(m_pBotPlayer->GetBotType() == BotsTypes::TYPE_BOT_QUEST)
	{
		m_Core.m_LostData = true;
		GS()->SendEquipItem(m_pBotPlayer->GetCID(), -1);
		CreateSnapProj(GetSnapFullID(), 2, PICKUP_HEALTH, true, false);
		CreateSnapProj(GetSnapFullID(), 2, PICKUP_ARMOR, true, false);
	}
	else if(m_pBotPlayer->GetBotType() == BotsTypes::TYPE_BOT_NPC)
	{
		m_Core.m_LostData = true;
		const int Function = BotJob::ms_aNpcBot[SubBotID].Function;
		if(Function == FunctionsNPC::FUNCTION_NPC_GIVE_QUEST)
			CreateSnapProj(GetSnapFullID(), 3, PICKUP_ARMOR, false, false);
	}
	return true;
}

void CCharacterBotAI::ShowProgress()
{
	for(const auto & pPlayerDamage : a_ListDmgPlayers)
	{
		CPlayer *pPlayer = GS()->GetPlayer(pPlayerDamage.first, true);
		if(pPlayer)
		{
			const int Health = m_pBotPlayer->GetHealth();
			const int StartHealth = m_pBotPlayer->GetStartHealth();
			const float gethp = (Health * 100.0) / StartHealth;
			std::unique_ptr<char[]> Progress = std::move(GS()->LevelString(100, (int)gethp, 10, ':', ' '));
			GS()->SBL(pPlayerDamage.first, BroadcastPriority::BROADCAST_GAME_PRIORITY, 10, "Health {STR}({INT}/{INT})", Progress.get(), &Health, &StartHealth);
		}
	}	
}

bool CCharacterBotAI::TakeDamage(vec2 Force, int Dmg, int From, int Weapon)
{
	CPlayer* pFrom = GS()->GetPlayer(From, true);
	if (!pFrom || !m_BotActive)
		return false;

	if(m_pBotPlayer->GetBotType() != BotsTypes::TYPE_BOT_MOB || pFrom->IsBot())
		return false;

	// before and after damage to health
	int StableDamage = Health();
	const bool BotDie = CCharacter::TakeDamage(Force, Dmg, From, Weapon);
	StableDamage -= Health();

	// set up an aggression against the person who caused the damage
	if (From != m_pBotPlayer->GetCID())
	{
		a_ListDmgPlayers[From] += StableDamage;
		if (m_BotTargetID == m_pBotPlayer->GetCID())
			SetTarget(From);
	}

	// death
	if(BotDie)
	{
		if(Weapon != WEAPON_SELF && Weapon != WEAPON_WORLD)
		{
			for(const auto& ld : a_ListDmgPlayers)
			{
				const int ParseClientID = ld.first;
				CPlayer* pPlayer = GS()->GetPlayer(ParseClientID, true, true);
				if(!pPlayer || ParseClientID == m_pBotPlayer->GetCID() || distance(pPlayer->m_ViewPos, m_Core.m_Pos) > 1000.0f)
					continue;

				DieRewardPlayer(pPlayer, Force);
			}
		}
		a_ListDmgPlayers.clear();
		ClearTarget();
		Die(From, Weapon);
		return true;
	}
	return false;
}

void CCharacterBotAI::Die(int Killer, int Weapon)
{
	if(m_pBotPlayer->GetBotType() != BotsTypes::TYPE_BOT_MOB)
		return;

	CCharacter::Die(Killer, Weapon);
}

void CCharacterBotAI::CreateRandomDropItem(int DropCID, float Random, int ItemID, int Count, vec2 Force)
{
	if (DropCID < 0 || DropCID >= MAX_PLAYERS || !GS()->m_apPlayers[DropCID] || !GS()->m_apPlayers[DropCID]->GetCharacter() || !IsAlive())
		return;

	const float RandomDrop = frandom()*100.0f;
	if (RandomDrop < Random)
		GS()->CreateDropItem(m_Core.m_Pos, DropCID, ItemID, Count, 0, Force);
	return;
}

void CCharacterBotAI::DieRewardPlayer(CPlayer* pPlayer, vec2 ForceDies)
{
	const int ClientID = pPlayer->GetCID();
	const int BotID = m_pBotPlayer->GetBotID();
	const int SubID = m_pBotPlayer->GetBotSub();
	const float LuckyDrop = clamp((float)pPlayer->GetAttributeCount(Stats::StLuckyDropItem, true) / 100.0f, 0.01f, 10.0f);

	if(m_pBotPlayer->GetBotType() == BotsTypes::TYPE_BOT_MOB)
		GS()->Mmo()->Quest()->AddMobProgress(pPlayer, BotID);

	for (int i = 0; i < 5; i++)
	{
		const int DropItem = BotJob::ms_aMobBot[SubID].m_aDropItem[i];
		const int CountItem = BotJob::ms_aMobBot[SubID].m_aCountItem[i];
		if (DropItem <= 0 || CountItem <= 0)
			continue;

		const float RandomDrop = clamp(BotJob::ms_aMobBot[SubID].m_aRandomItem[i] + LuckyDrop, 0.0f, 100.0f);
		CreateRandomDropItem(ClientID, RandomDrop, DropItem, CountItem, ForceDies);
	}

	const int MultiplierExperience = kurosio::computeExperience(BotJob::ms_aMobBot[SubID].m_Level) / g_Config.m_SvKillmobsIncreaseLevel;
	const int MultiplierRaid = clamp(GS()->IncreaseExperienceRaid(MultiplierExperience), 1, GS()->IncreaseExperienceRaid(MultiplierExperience));
	pPlayer->AddExp(MultiplierRaid);

	const int MultiplierDrops = max(MultiplierRaid / 2, 1);
	GS()->CreateDropBonuses(m_Core.m_Pos, 1, MultiplierDrops, (1+random_int() % 2), ForceDies);

	const int MultiplierGolds = max(BotJob::ms_aMobBot[SubID].m_Power / g_Config.m_SvStrongGold, 1);
	pPlayer->AddMoney(MultiplierGolds);
	
	// TODO: balance depending on the difficulty, not just the level
	const int CalculateSP = (pPlayer->Acc().m_Level > BotJob::ms_aMobBot[SubID].m_Level ? 40 + min(40, (pPlayer->Acc().m_Level - BotJob::ms_aMobBot[SubID].m_Level)*2) : 40);
	if (random_int() % CalculateSP == 0)
	{
		InventoryItem& pItemSkillPlayer = pPlayer->GetItem(itSkillPoint);
		pItemSkillPlayer.Add(1);
		GS()->Chat(ClientID, "Skill points increased. Now ({INT}SP)", &pItemSkillPlayer.m_Count);
	}
}

void CCharacterBotAI::ClearTarget()
{
	const int FromID = m_pBotPlayer->GetCID();
	if (m_BotTargetID != FromID)
	{
		m_pBotPlayer->m_TargetPos = vec2(0, 0);
		m_EmotionsStyle = 1 + random_int() % 5;
		m_BotTargetID = FromID;
		m_BotTargetLife = 0;
		m_BotTargetCollised = 0;
	}
}

void CCharacterBotAI::SetTarget(int ClientID)
{
	m_EmotionsStyle = EMOTE_ANGRY;
	m_BotTargetID = ClientID;
}

void CCharacterBotAI::ChangeWeapons()
{
	const int randtime = 1+random_int()%3;
	if(Server()->Tick() % (Server()->TickSpeed()*randtime) == 0)
	{
		const int randomweapon = random_int()%4;	
		m_ActiveWeapon = clamp(randomweapon, (int)WEAPON_HAMMER, (int)WEAPON_LASER);
	}
}

bool CCharacterBotAI::GiveWeapon(int Weapon, int GiveAmmo)
{
	const int WeaponID = clamp(Weapon, (int)WEAPON_HAMMER, (int)WEAPON_NINJA);
	m_aWeapons[WeaponID].m_Got = true;
	m_aWeapons[WeaponID].m_Ammo = GiveAmmo;
	return true;
}

void CCharacterBotAI::Tick()
{
	m_BotActive = GS()->CheckingPlayersDistance(m_Core.m_Pos, 1000.0f);
	if(!m_BotActive || !IsAlive())
		return;

	if(IsAlive())
	{
		EngineBots();
		HandleEvents();
		HandleTilesets();
	}
	CCharacter::Tick();
}

void CCharacterBotAI::TickDefered()
{
	if(!m_BotActive || !IsAlive())
		return;

	CCharacter::TickDefered();
}

// interactive bots
void CCharacterBotAI::EngineBots()
{
	if(m_pBotPlayer->GetBotType() == BotsTypes::TYPE_BOT_NPC)
	{
		const int tx = m_Pos.x + m_Input.m_Direction * 45.0f;
		if(tx < 0)  
			m_Input.m_Direction = 1;
		else if(tx >= GS()->Collision()->GetWidth() * 32.0f) 
			m_Input.m_Direction = -1;

		m_LatestPrevInput = m_LatestInput;
		m_LatestInput = m_Input;

		EngineNPC();
	}
	else if(m_pBotPlayer->GetBotType() == BotsTypes::TYPE_BOT_MOB)
		EngineMobs();
	else if(m_pBotPlayer->GetBotType() == BotsTypes::TYPE_BOT_QUEST)
		EngineQuestMob();
}

// interactive NPC
void CCharacterBotAI::EngineNPC()
{
	const int MobID = m_pBotPlayer->GetBotSub();
	const int EmoteBot = BotJob::ms_aNpcBot[MobID].m_Emote;
	EmoteActions(EmoteBot);

	// direction eyes
	if(Server()->Tick() % Server()->TickSpeed() == 0)
		m_Input.m_TargetY = random_int()%4- random_int()%8;
	m_Input.m_TargetX = (m_Input.m_Direction*10+1);

	bool PlayerFinding = false;
	if(BotJob::ms_aNpcBot[MobID].Function == FunctionsNPC::FUNCTION_NPC_NURSE)
		PlayerFinding = FunctionNurseNPC();
	else
		PlayerFinding = BaseFunctionNPC();

	if (!PlayerFinding && !BotJob::ms_aNpcBot[MobID].m_Static)
	{
		if(random_int() % 50 == 0)
		{
			const int RandomDirection = random_int()%6;
			if(RandomDirection == 0 || RandomDirection == 2)
				m_Input.m_Direction = -1 + RandomDirection;
			else m_Input.m_Direction = 0;
		}
	}
}

// Interactives of quest mobiles
void CCharacterBotAI::EngineQuestMob()
{
	if(Server()->Tick() % Server()->TickSpeed() == 0)
		m_Input.m_TargetY = random_int()%4- random_int()%8;
	m_Input.m_TargetX = (m_Input.m_Direction*10+1);
	EmoteActions(EMOTE_BLINK);
	SearchTalkedPlayer();
}

// Interactive mobs hostile
void CCharacterBotAI::EngineMobs()
{
	ResetInput();
	CPlayer* pPlayer = SearchTenacityPlayer(1000.0f);
	if(pPlayer && pPlayer->GetCharacter())
	{
		m_pBotPlayer->m_TargetPos = pPlayer->GetCharacter()->GetPos();
		Action();
	}
	else if(Server()->Tick() > m_pBotPlayer->m_LastPosTick)
		m_pBotPlayer->m_TargetPos = vec2(0, 0);

	// effect sleppy
	const int MobID = m_pBotPlayer->GetBotSub();
	if(m_BotTargetID == m_pBotPlayer->GetCID() && str_comp(BotJob::ms_aMobBot[MobID].m_aBehavior, "Sleepy") == 0)
	{
		if(Server()->Tick() % (Server()->TickSpeed() / 2) == 0)
		{
			GS()->SendEmoticon(m_pBotPlayer->GetCID(), EMOTICON_ZZZ);
			SetEmote(EMOTE_BLINK, 1);
		}
		return;
	}

	const bool WeaponedBot = (BotJob::ms_aMobBot[MobID].m_Spread >= 1);
	if(WeaponedBot)
	{
		if(BotJob::ms_aMobBot[MobID].m_Boss)
			ShowProgress();
		ChangeWeapons();
	}
	Move();
		
	m_PrevPos = m_Pos;
	if(m_Input.m_Direction)
		m_PrevDirection = m_Input.m_Direction;

	EmoteActions(m_EmotionsStyle);
}

void CCharacterBotAI::Move()
{
	SetAim(m_pBotPlayer->m_TargetPos - m_Pos);
	
	int Index = -1;
	int ActiveWayPoints = 0;
	for(int i = 0; i < m_pBotPlayer->m_PathSize && i < 30 && !GS()->Collision()->IntersectLineWithInvisible(m_pBotPlayer->m_WayPoints[i], m_Pos, 0, 0); i++)
	{
		Index = i;
		ActiveWayPoints = i;
	}

	vec2 WayDir = vec2(0, 0);
	if(Index > -1)
		WayDir = normalize(m_pBotPlayer->m_WayPoints[Index] - GetPos());

	// set the direction
	if(WayDir.x < 0 && ActiveWayPoints > 3)
		m_Input.m_Direction = -1;
	else if(WayDir.x > 0 && ActiveWayPoints > 3)
		m_Input.m_Direction = 1;
	else
		m_Input.m_Direction = m_PrevDirection;

	// jumping
	const bool IsGround = IsGrounded();
	if((IsGround && WayDir.y < -0.5) || (!IsGround && WayDir.y < -0.5 && m_Core.m_Vel.y > 0))
		m_Input.m_Jump = 1;

	const bool IsCollide = GS()->Collision()->IntersectLine(m_Pos, m_Pos + vec2(m_Input.m_Direction, 0) * 150, &m_WallPos, 0x0);
	if(IsCollide)
	{
		if(IsGround && GS()->Collision()->IntersectLine(m_WallPos, m_WallPos + vec2(0, -1) * 210, 0x0, 0x0))
			m_Input.m_Jump = 1;

		if(!IsGround && GS()->Collision()->IntersectLine(m_WallPos, m_WallPos + vec2(0, -1) * 125, 0x0, 0x0))
			m_Input.m_Jump = 1;
	}

	// if way points down dont jump
	if(m_Input.m_Jump == 1 && (WayDir.y >= 0 || ActiveWayPoints < 3))
		m_Input.m_Jump = 0;

	// jump over character
	vec2 IntersectPos;
	CCharacter* pChar = GameWorld()->IntersectCharacter(GetPos(), GetPos() + vec2(m_Input.m_Direction, 0) * 128, 16.0f, IntersectPos, (CCharacter*)this);
	if (pChar && (pChar->GetPos().x < GetPos().x || !pChar->GetPlayer()->IsBot()))
		m_Input.m_Jump = 1;

	if(ActiveWayPoints > 2 && !m_Input.m_Hook && (WayDir.x != 0 || WayDir.y != 0))
	{	
		if(m_Core.m_HookState == HOOK_GRABBED && m_Core.m_HookedPlayer == -1)
		{
			vec2 HookVel = normalize(m_Core.m_HookPos - GetPos()) * GS()->Tuning()->m_HookDragAccel;
			if(HookVel.y > 0)
				HookVel.y *= 0.3f;
			if((HookVel.x < 0 && m_Input.m_Direction < 0) || (HookVel.x > 0 && m_Input.m_Direction > 0))
				HookVel.x *= 0.95f;
			else
				HookVel.x *= 0.75f;

			float ps = dot(WayDir, HookVel);
			if(ps > 0 || (WayDir.y < 0 && m_Core.m_Vel.y > 0.f && m_Core.m_HookTick < SERVER_TICK_SPEED + SERVER_TICK_SPEED / 2))
				m_Input.m_Hook = 1;
			if(m_Core.m_HookTick > 4 * SERVER_TICK_SPEED || length(m_Core.m_HookPos - GetPos()) < 20.0f)
				m_Input.m_Hook = 0;
		}
		else if(m_Core.m_HookState == HOOK_FLYING)
			m_Input.m_Hook = 1;
		else if(m_LatestInput.m_Hook == 0 && m_Core.m_HookState == HOOK_IDLE)
		{
			int NumDir = 32;
			vec2 HookDir(0.0f, 0.0f);
			float MaxForce = 0;

			for(int i = 0; i < NumDir; i++)
			{
				float a = 2 * i * pi / NumDir;
				vec2 dir = direction(a);
				vec2 Pos = GetPos() + dir * GS()->Tuning()->m_HookLength;

				if((GS()->Collision()->IntersectLine(GetPos(), Pos, &Pos, 0) & (CCollision::COLFLAG_SOLID | CCollision::COLFLAG_NOHOOK)) == CCollision::COLFLAG_SOLID)
				{
					vec2 HookVel = dir * GS()->Tuning()->m_HookDragAccel;
					if(HookVel.y > 0)
						HookVel.y *= 0.3f;
					if((HookVel.x < 0 && m_Input.m_Direction < 0) || (HookVel.x > 0 && m_Input.m_Direction > 0))
						HookVel.x *= 0.95f;
					else
						HookVel.x *= 0.75f;

					HookVel += vec2(0, 1) * GS()->Tuning()->m_Gravity;

					float ps = dot(WayDir, HookVel);
					if(ps > MaxForce)
					{
						MaxForce = ps;
						HookDir = Pos - GetPos();
					}
				}
			}
			if(length(HookDir) > 32.f)
			{
				SetAim(HookDir);
				m_Input.m_Hook = 1;
			}
		}
	}

	// in case the bot stucks
	if(m_Pos.x != m_PrevPos.x)
		m_MoveTick = Server()->Tick();

	if(m_Pos.x == m_PrevPos.x && Server()->Tick() - m_MoveTick > Server()->TickSpeed() / 2)
	{
		m_Input.m_Direction = -m_Input.m_Direction;
		m_Input.m_Jump = 1;
		m_MoveTick = Server()->Tick();
	}
}

void CCharacterBotAI::Action()
{
	CPlayer* pPlayer = GS()->GetPlayer(m_BotTargetID, true, true);
	if(m_BotTargetID == m_pBotPlayer->GetCID() || !pPlayer || m_BotTargetCollised)
		return;

	if((m_Input.m_Hook && m_Core.m_HookState == HOOK_IDLE) || m_ReloadTimer != 0)
		return;

	if(m_ActiveWeapon == WEAPON_HAMMER && distance(pPlayer->GetCharacter()->GetPos(), GetPos()) > 128.0f)
		return;

	// fire
	if((m_Input.m_Fire & 1) != 0)
	{
		m_Input.m_Fire++;
		m_LatestInput.m_Fire++;
		return;
	}

	if(!(m_Input.m_Fire & 1))
	{
		m_LatestInput.m_Fire++;
		m_Input.m_Fire++;
	}
}

void CCharacterBotAI::SetAim(vec2 Dir)
{
	m_Input.m_TargetX = (int)Dir.x;
	m_Input.m_TargetY = (int)Dir.y;
	m_LatestInput.m_TargetX = (int)Dir.x;
	m_LatestInput.m_TargetY = (int)Dir.y;
}

// searching for a player among people
CPlayer *CCharacterBotAI::SearchPlayer(int Distance)
{
	for(int i = 0 ; i < MAX_PLAYERS; i ++)
	{
		if(!GS()->m_apPlayers[i] 
			|| !GS()->m_apPlayers[i]->GetCharacter() 
			|| distance(m_Core.m_Pos, GS()->m_apPlayers[i]->GetCharacter()->m_Core.m_Pos) > Distance
			|| GS()->Collision()->IntersectLineWithInvisible(GS()->m_apPlayers[i]->GetCharacter()->m_Core.m_Pos, m_Pos, 0, 0)
			|| Server()->GetWorldID(i) != GS()->GetWorldID())
			continue;
		return GS()->m_apPlayers[i];
	}
	return nullptr;
}

// finding a player among people who have the highest fury
CPlayer *CCharacterBotAI::SearchTenacityPlayer(float Distance)
{
	const bool ActiveTargetID = m_BotTargetID != m_pBotPlayer->GetCID();
	if(!ActiveTargetID && (GS()->IsDungeon() || random_int() % 30 == 0))
	{
		CPlayer *pPlayer = SearchPlayer(Distance);
		if(pPlayer && pPlayer->GetCharacter()) 
			SetTarget(pPlayer->GetCID());
		return pPlayer;
	}

	// throw off aggression if the player is far away
	CPlayer* pPlayer = GS()->GetPlayer(m_BotTargetID, true, true);
	if (ActiveTargetID && (!pPlayer 
		|| (pPlayer && (distance(pPlayer->GetCharacter()->GetPos(), m_Pos) > 800.0f || Server()->GetWorldID(m_BotTargetID) != GS()->GetWorldID()))))
		ClearTarget();

	// non-hostile mobs
	if (!ActiveTargetID || !pPlayer)
		return nullptr; 

	// throw off the lifetime of a target
	m_BotTargetCollised = GS()->Collision()->IntersectLineWithInvisible(pPlayer->GetCharacter()->GetPos(), m_Pos, 0, 0);
	if (m_BotTargetLife && m_BotTargetCollised)
	{
		m_BotTargetLife--;
		if (!m_BotTargetLife)
			ClearTarget();
	}
	else
	{
		m_BotTargetLife = Server()->TickSpeed() * 3;
	}

	// looking for a stronger
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		// check the distance of the player
		CPlayer* pFinderHard = GS()->GetPlayer(i, true, true);
		if (!pFinderHard || distance(pFinderHard->GetCharacter()->m_Core.m_Pos, m_Core.m_Pos) > 800.0f)
			continue;

		// check if the player is tastier for the bot
		const bool FinderCollised = (bool)GS()->Collision()->IntersectLineWithInvisible(pFinderHard->GetCharacter()->m_Core.m_Pos, m_Core.m_Pos, 0, 0);
		if (!FinderCollised && ((m_BotTargetLife <= 10 && m_BotTargetCollised)
			|| pFinderHard->GetAttributeCount(Stats::StHardness, true) > pPlayer->GetAttributeCount(Stats::StHardness, true)))
			SetTarget(i);
	}

	return pPlayer;
}

bool CCharacterBotAI::SearchTalkedPlayer()
{
	bool PlayerFinding = false;
	const int MobID = m_pBotPlayer->GetBotSub();
	const bool DialoguesNotEmpty = ((bool)(m_pBotPlayer->GetBotType() == BotsTypes::TYPE_BOT_QUEST && !(BotJob::ms_aQuestBot[MobID].m_aTalk).empty())
				|| (m_pBotPlayer->GetBotType() == BotsTypes::TYPE_BOT_NPC && !(BotJob::ms_aNpcBot[MobID].m_aTalk).empty()));
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		CPlayer* pFindPlayer = GS()->GetPlayer(i, true, true);
		if(pFindPlayer && distance(pFindPlayer->GetCharacter()->m_Core.m_Pos, m_Core.m_Pos) < 128.0f &&
			!GS()->Collision()->IntersectLine(pFindPlayer->GetCharacter()->m_Core.m_Pos, m_Core.m_Pos, 0, 0) && m_pBotPlayer->IsActiveSnappingBot(i))
		{
			if (DialoguesNotEmpty)
				GS()->SBL(i, BroadcastPriority::BROADCAST_GAME_INFORMATION, 10, "Begin dialog: \"hammer hit\"");

			pFindPlayer->GetCharacter()->m_Core.m_LostData = true;
			m_Input.m_TargetX = static_cast<int>(pFindPlayer->GetCharacter()->m_Core.m_Pos.x - m_Pos.x);
			m_Input.m_TargetY = static_cast<int>(pFindPlayer->GetCharacter()->m_Core.m_Pos.y - m_Pos.y);
			m_Input.m_Direction = 0;
			PlayerFinding = true;
		}
	}
	return PlayerFinding;
}

void CCharacterBotAI::EmoteActions(int EmotionStyle)
{
	if (EmotionStyle < EMOTE_PAIN || EmotionStyle > EMOTE_BLINK)
		return;

	if ((Server()->Tick() % (Server()->TickSpeed() * 3 + random_int() % 10)) == 0)
	{
		if (EmotionStyle == EMOTE_BLINK)
		{
			SetEmote(EMOTE_BLINK, 1 + random_int() % 2);
			GS()->SendEmoticon(m_pBotPlayer->GetCID(), EMOTICON_DOTDOT);
		}
		else if (EmotionStyle == EMOTE_HAPPY)
		{
			SetEmote(EMOTE_HAPPY, 1 + random_int() % 2);
			GS()->SendEmoticon(m_pBotPlayer->GetCID(), (random_int() % 2 == 0 ? (int)EMOTICON_HEARTS : (int)EMOTICON_EYES));
		}
		else if (EmotionStyle == EMOTE_ANGRY)
		{
			SetEmote(EMOTE_ANGRY, 1 + random_int() % 2);
			GS()->SendEmoticon(m_pBotPlayer->GetCID(), (EMOTICON_SPLATTEE + random_int() % 3));
		}		
		else if (EmotionStyle == EMOTE_PAIN)
		{
			SetEmote(EMOTE_PAIN, 1 + random_int() % 2);
			GS()->SendEmoticon(m_pBotPlayer->GetCID(), EMOTICON_DROP);
		}
	}
}

// - - - - - - - - - - - - - - - - - - - BASE FUNCTION
bool CCharacterBotAI::BaseFunctionNPC()
{
	const bool PlayerFinding = SearchTalkedPlayer();
	return PlayerFinding;
}

// - - - - - - - - - - - - - - - - - - - FUNCTION NURSE
bool CCharacterBotAI::FunctionNurseNPC()
{
	char aBuf[16];
	bool PlayerFinding = false;
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		CPlayer* pFindPlayer = GS()->GetPlayer(i, true, true);
		if(!pFindPlayer || distance(pFindPlayer->GetCharacter()->m_Core.m_Pos, m_Core.m_Pos) >= 256.0f ||
			GS()->Collision()->IntersectLine(pFindPlayer->GetCharacter()->m_Core.m_Pos, m_Core.m_Pos, 0, 0))
			continue;

		// disable collision with players
		if(distance(pFindPlayer->GetCharacter()->m_Core.m_Pos, m_Core.m_Pos) < 128.0f)
			pFindPlayer->GetCharacter()->m_Core.m_LostData = true;

		// skip full health
		if(pFindPlayer->GetHealth() >= pFindPlayer->GetStartHealth())
			continue;

		// set target to player
		m_Input.m_TargetX = static_cast<int>(pFindPlayer->GetCharacter()->m_Core.m_Pos.x - m_Pos.x);
		m_Input.m_TargetY = static_cast<int>(pFindPlayer->GetCharacter()->m_Core.m_Pos.y - m_Pos.y);
		m_LatestInput.m_TargetX = m_Input.m_TargetX;
		m_LatestInput.m_TargetY = m_Input.m_TargetX;

		// health every sec
		if(Server()->Tick() % Server()->TickSpeed() != 0)	
			continue;

		// increase health for player
		const int Health = max(pFindPlayer->GetStartHealth() / 20, 1);
		vec2 DrawPosition = vec2(pFindPlayer->GetCharacter()->m_Core.m_Pos.x, pFindPlayer->GetCharacter()->m_Core.m_Pos.y - 90.0f);
		str_format(aBuf, sizeof(aBuf), "%dHP", Health);
		GS()->CreateText(NULL, false, DrawPosition, vec2(0, 0), 40, aBuf);
		new CHearth(&GS()->m_World, m_Pos, pFindPlayer, Health, pFindPlayer->GetCharacter()->m_Core.m_Vel);
	
		m_Input.m_Direction = 0;
		PlayerFinding = true;
	}
	return PlayerFinding;
}