/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "dungeon.h"

#include <game/server/gamecontext.h>

#include "../entities/npcwall.h"
#include "../logicworld/logicwall.h"
#include "../entities/jobitems.h"

CGameControllerDungeon::CGameControllerDungeon(class CGS *pGS) : IGameController(pGS)
{
	m_pGameType = "MmoTee";

	// создание двери
	m_DungeonID = GS()->DungeonID();
	vec2 PosDoor = vec2(CGS::Dungeon[m_DungeonID].DoorX, CGS::Dungeon[m_DungeonID].DoorY);
	m_DungeonDoor = new DungeonDoor(&GS()->m_World, PosDoor);
	ChangeState(DUNGEON_WAITING);
}

bool CGameControllerDungeon::CheckFinishedDungeon()
{
	if (LeftMobsToWin() > 0)
		return false;

	m_StateDungeon = DUNGEON_FINISHED;
	return true;
}

void CGameControllerDungeon::ChangeState(int State)
{
	m_StateDungeon = State;
	switch(State) // pre-init use one how use
	{
		// Используется при смене статуса в Ожидание данжа
		case DUNGEON_WAITING:
	
		break;

		// Используется при смене статуса в Начало данжа
		case DUNGEON_STARTED:
			AllowMobsSpawn();
			for(int i = 0; i < MAX_PLAYERS; i++)
			{
				if(GS()->m_apPlayers[i] && GS()->m_apPlayers[i]->GetCharacter())
				{	
					GS()->SBL(i, 99999, 500, "Dungeon {STR} Started!", Server()->GetWorldName(GS()->GetWorldID()));
					GS()->m_apPlayers[i]->GetCharacter()->Die(i, WEAPON_SELF);
				}
			}
		break;

		// Используется при смене статуса в Завершение данжа
		case DUNGEON_FINISHED:

		break;
	}
	m_DungeonDoor->SetState(State);
}

void CGameControllerDungeon::StateTick()
{
	int Players = PlayIt();
	CGS::Dungeon[m_DungeonID].PlayIt = Players;

	// Используется в тике когда Ожидание данжа
	if (m_StateDungeon == DUNGEON_WAITING)
	{
		// пишем всем игрокам что ждем 2 игроков
		if (Players == 1)
		{
			for (int i = 0; i < MAX_PLAYERS; i++)
			{
				if (GS()->m_apPlayers[i] && GS()->m_apPlayers[i]->GetCharacter())
					GS()->SBL(i, 99999, 10, "Dungeon '{STR}' Waiting 2 players!", Server()->GetWorldName(GS()->GetWorldID()));
			}
		}
		// начинаем данж если равно 2 игрока или больше
		else if (Players > 1)
		{
			ChangeState(DUNGEON_STARTED);
		}
	}
	// Используется в тике когда Данж начат
	else if (m_StateDungeon == DUNGEON_STARTED)
	{
		// проверяем если в активном данже нет игроков
		if (Players < 1)
		{
			for (int i = 0; i < MAX_PLAYERS; i++)
			{
				if (GS()->m_apPlayers[i] && GS()->m_apPlayers[i]->GetCharacter())
					GS()->m_apPlayers[i]->GetCharacter()->Die(i, WEAPON_SELF);

				ChangeState(DUNGEON_WAITING);
			}
		}

		// завершаем данж когда успешно данж завершен
		if (CheckFinishedDungeon())
		{
			ChangeState(DUNGEON_FINISHED);
		}
	}
	// Используется в тике когда Завершение данжа
	else if (m_StateDungeon == DUNGEON_FINISHED)
	{
	
	}
}

void CGameControllerDungeon::Tick()
{
	StateTick();
	IGameController::Tick();
}

void CGameControllerDungeon::CreateLogic(int Type, int Mode, vec2 Pos, int ParseInt)
{
	if(Type == 1)
	{
		new CLogicWall(&GS()->m_World, Pos);
	}
	if(Type == 2)
	{
		new CLogicWallWall(&GS()->m_World, Pos, Mode, ParseInt);
	}
	if(Type == 3)
	{
		new CLogicDoorKey(&GS()->m_World, Pos, ParseInt, Mode);
	}
}

bool CGameControllerDungeon::OnEntity(int Index, vec2 Pos)
{
	if(IGameController::OnEntity(Index, Pos))
		return true;

	if(Index == ENTITY_NPC_WALLUP)
	{
		new CNPCWall(&GS()->m_World, Pos, false);
		return true;
	}
	if(Index == ENTITY_NPC_WALLLEFT)
	{
		new CNPCWall(&GS()->m_World, Pos, true);
		return true;
	}
	return false;
}

int CGameControllerDungeon::OnCharacterDeath(CCharacter* pVictim, CPlayer* pKiller, int Weapon)
{
	if (!pKiller || !pVictim || !pVictim->GetPlayer())
		return 0;

	int KillerID = pKiller->GetCID();
	if (pVictim->GetPlayer()->IsBot() && pVictim->GetPlayer()->GetSpawnBot() == SPAWNMOBS)
	{
		int Progress = LeftMobsToWin();
		GS()->Chat(KillerID, "Left defeat mobs to complete the dungeon {INT}", &Progress);
	}
	return 0;
}

// Кол-во игроков что играет в данже
int CGameControllerDungeon::PlayIt() const
{
	int playIt = 0;
	for(int i = 0 ; i < MAX_PLAYERS ; i++)
	{
		if(Server()->GetWorldID(i) == GS()->GetWorldID())
			playIt++;
	}
	return playIt;
}

int CGameControllerDungeon::LeftMobsToWin() const
{
	int mobSize = 0;
	for (int i = MAX_PLAYERS; i < MAX_CLIENTS; i++)
	{
		if (GS()->m_apPlayers[i] && GS()->m_apPlayers[i]->GetSpawnBot() == SPAWNMOBS && GS()->m_apPlayers[i]->GetCharacter())
			mobSize++;
	}
	return mobSize;
}

void CGameControllerDungeon::AllowMobsSpawn()
{
	int mobSize = 0;
	for (int i = MAX_PLAYERS; i < MAX_CLIENTS; i++)
	{
		CPlayerBot* BotPlayer = static_cast<CPlayerBot*>(GS()->m_apPlayers[i]);
		if (BotPlayer && BotPlayer->GetSpawnBot() == SPAWNMOBS)
			BotPlayer->SetDungeonAllowedSpawn(true);
	}
}


// Двери
DungeonDoor::DungeonDoor(CGameWorld *pGameWorld, vec2 Pos)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_DUNGEONDOOR, Pos)
{
	m_To = vec2(Pos.x, Pos.y-140);
	m_Pos.y += 30;

	m_State = DUNGEON_WAITING;

	GameWorld()->InsertEntity(this);
}

void DungeonDoor::Tick()
{
	if(m_State != DUNGEON_STARTED)
	{
		for(CCharacter *pChar = (CCharacter*) GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); pChar; pChar = (CCharacter *)pChar->TypeNext())
		{
			if(pChar->IsAlive() && distance(pChar->m_Core.m_Pos, m_Pos) < 128.0f)
				pChar->Die(pChar->GetPlayer()->GetCID(), WEAPON_SELF);
		}
	}
}

void DungeonDoor::SetState(int State)
{
	m_State = State;
}

void DungeonDoor::Snap(int SnappingClient)
{
	if (m_State == DUNGEON_STARTED || NetworkClipped(SnappingClient))
		return;
	
	CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, GetID(), sizeof(CNetObj_Laser)));
	if (!pObj)
		return;

	pObj->m_X = int(m_Pos.x);
	pObj->m_Y = int(m_Pos.y);
	pObj->m_FromX = int(m_To.x);
	pObj->m_FromY = int(m_To.y);
	pObj->m_StartTick = Server()->Tick()-2;
}
