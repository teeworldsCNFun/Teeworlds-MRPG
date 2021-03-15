/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_ACCOUNT_DATA_H
#define GAME_SERVER_COMPONENT_ACCOUNT_DATA_H
#include <game/game_context.h>
#include <game/server/mmocore/Components/Shops/ShopData.h>
#include <game/server/mmocore/Utils/FieldData.h>

#include <list>
#include <map>

struct CAccountData
{
	// main
	char m_aLogin[64];
	char m_aLastLogin[64];
	char m_aLanguage[8];
	int m_AccountID;
	int m_Level;
	int m_Exp;
	int m_GuildID;
	int m_GuildRank;
	std::list< int > m_aHistoryWorld;

	// upgrades
	int m_Upgrade;
	std::map< int, int > m_aStats;

	// skins
	char m_aaSkinPartNames[NUM_SKINPARTS][MAX_SKIN_ARRAY_SIZE];
	int m_aUseCustomColors[NUM_SKINPARTS];
	int m_aSkinPartColors[NUM_SKINPARTS];

	int m_Team;
	std::map < int, bool > m_aAetherLocation;
	bool IsGuild() const { return m_GuildID > 0; }

	CFieldsData m_aMiningData;
	CFieldsData m_aPlantData;

	CAccountData()
	{
		m_AccountID = -1;
		m_Level = 0;
		m_Team = TEAM_SPECTATORS;

		m_aMiningData.add_field<int, JOB_LEVEL>("MnrLevel", "Miner level");
		m_aMiningData.add_field<int, JOB_EXPERIENCE>("MnrExp", "Miner experience");
		m_aMiningData.add_field<int, JOB_UPGRADES>("MnrUpgrade", "Miner upgrades");
		m_aMiningData.add_field<int, JOB_UPGR_COUNTS>("MnrCount", "Miner counts");

		m_aPlantData.add_field<int, JOB_LEVEL>("PlLevel", "Farmer level");
		m_aPlantData.add_field<int, JOB_EXPERIENCE>("PlExp", "Farmer experience");
		m_aPlantData.add_field<int, JOB_UPGRADES>("PlUpgrade", "Farmer upgrades");
		m_aPlantData.add_field<int, JOB_UPGR_COUNTS>("PlCounts", "Farmer counts");
	}

	static std::map < int, CAccountData > ms_aData;
};

struct CAccountTempData
{
	int m_TempDecoractionID;
	int m_TempDecorationType;
	int m_TempID3;

	CAuctionItem m_SellItem;

	// temp rankname for guild rank settings
	char m_aRankGuildBuf[32];

	// temp guild name for searching
	char m_aGuildSearchBuf[32];

	// player stats
	int m_TempHealth;
	int m_TempMana;
	int m_TempPing;

	// save pos teleport
	bool m_TempSafeSpawn;
	int m_TempTeleportX;
	int m_TempTeleportY;

	// dungeon
	int m_TempTimeDungeon;
	bool m_TempDungeonReady;
	int m_TempTankVotingDungeon;
	bool m_TempAlreadyVotedDungeon;

	static std::map < int, CAccountTempData > ms_aPlayerTempData;
};

#endif