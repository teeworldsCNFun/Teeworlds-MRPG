/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/shared/config.h>
#include <game/server/gamecontext.h>
#include "BotJob.h"

using namespace sqlstr;

// Структуры ботов
std::map < int , BotJob::DescDataBot > BotJob::DataBot;
std::map < int , BotJob::QuestBotInfo > BotJob::QuestBot;
std::map < int , BotJob::NpcBotInfo > BotJob::NpcBot;
std::map < int , BotJob::MobBotInfo > BotJob::MobBot;

// Загрузка всех скинов и мобов что потом использовать для свзяей с другими ботами
void BotJob::OnInitWorld(const char* pWhereLocalWorld)
{
	// загружаем связанных ботов
	LoadMainInformationBots();
	LoadQuestBots(pWhereLocalWorld);
	LoadNpcBots(pWhereLocalWorld);
	LoadMobsBots(pWhereLocalWorld);
}

// добавить нового бота
void BotJob::ConAddCharacterBot(int ClientID, const char *pCharacter)
{
	// если нет игрока то не продолжаем
	CPlayer *pPlayer = GS()->GetPlayer(ClientID);
	if(!pPlayer) return;

	// собираем данные со скина игрока
	char SkinPart[256], SkinColor[256];
	str_format(SkinPart, sizeof(SkinPart), "%s %s %s %s %s %s", pPlayer->Acc().m_aaSkinPartNames[0], pPlayer->Acc().m_aaSkinPartNames[1], 
		pPlayer->Acc().m_aaSkinPartNames[2], pPlayer->Acc().m_aaSkinPartNames[3], pPlayer->Acc().m_aaSkinPartNames[4], pPlayer->Acc().m_aaSkinPartNames[5]);

	str_format(SkinColor, sizeof(SkinColor), "%d %d %d %d %d %d", pPlayer->Acc().m_aSkinPartColors[0], pPlayer->Acc().m_aSkinPartColors[1], 
		pPlayer->Acc().m_aSkinPartColors[2], pPlayer->Acc().m_aSkinPartColors[3], pPlayer->Acc().m_aSkinPartColors[4], pPlayer->Acc().m_aSkinPartColors[5]);

	// проверяем ник если есть обновим нет добавим
	CSqlString<16> cNick = CSqlString<16>(pCharacter);
	boost::scoped_ptr<ResultSet> RES(SJK.SD("*", "tw_bots_world", "WHERE BotName = '%s' AND ID > '0'", cNick.cstr()));
	if(RES->next())
	{
		// если ник не верен из базы данных
		int ID = RES->getInt("ID");
		SJK.UD("tw_bots_world", "SkinName = '%s', SkinColor = '%s' WHERE ID = '%d'", SkinPart, SkinColor, ID);
		GS()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "parseskin", "Updated character bot!");
		return;	
	}

	// добавляем нового бота
	SJK.ID("tw_bots_world", "(BotName, SkinName, SkinColor) VALUES ('%s', '%s', '%s')", cNick.cstr(), SkinPart, SkinColor);
	GS()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "parseskin", "Added new character bot!");
}

void BotJob::ProcessingTalkingNPC(int OwnID, int TalkingID, bool PlayerTalked, const char *Message, int Style, int TalkingEmote)
{
	if(GS()->CheckClient(OwnID))
	{
		GS()->SendTalkText(OwnID, TalkingID, PlayerTalked, Message, Style, TalkingEmote);
		return;
	}

	GS()->Motd(OwnID, Message);
}

bool BotJob::TalkingBotNPC(CPlayer* pPlayer, int MobID, int Progress, int TalkedID, const char *pText)
{
	int ClientID = pPlayer->GetCID();
	if (!IsNpcBotValid(MobID) || Progress >= (int)NpcBot[MobID].m_Talk.size())
	{
		GS()->ClearTalkText(ClientID);
		return false;
	}

	if (!GS()->CheckClient(ClientID))
		GS()->SBL(ClientID, BroadcastPriority::BROADCAST_GAME_PRIORITY, 100, "Press 'F4' to continue the dialog!");

	char reformTalkedText[512];
	int BotID = NpcBot[MobID].BotID;
	int sizeTalking = NpcBot[MobID].m_Talk.size();
	if (str_comp_nocase(pText, "empty") != 0)
	{
		pPlayer->FormatTextQuest(BotID, pText);
		str_format(reformTalkedText, sizeof(reformTalkedText), "( 1 of 1 ) - %s", pPlayer->FormatedTalkedText());
		GS()->Mmo()->BotsData()->ProcessingTalkingNPC(ClientID, TalkedID, 0, reformTalkedText, 0, EMOTE_BLINK);
		pPlayer->ClearFormatQuestText();
		return true;
	}

	pPlayer->FormatTextQuest(BotID, NpcBot[MobID].m_Talk.at(Progress).m_TalkingText);
	str_format(reformTalkedText, sizeof(reformTalkedText), "( %d of %d ) - %s", 1 + Progress, sizeTalking, pPlayer->FormatedTalkedText());
	pPlayer->ClearFormatQuestText();

	GS()->Mmo()->BotsData()->ProcessingTalkingNPC(ClientID, TalkedID,
		NpcBot[MobID].m_Talk.at(Progress).m_PlayerTalked, reformTalkedText,
		NpcBot[MobID].m_Talk.at(Progress).m_Style, NpcBot[MobID].m_Talk.at(Progress).m_Emote);
	return true;
}

bool BotJob::TalkingBotQuest(CPlayer* pPlayer, int MobID, int Progress, int TalkedID)
{
	int ClientID = pPlayer->GetCID();
	if (!IsQuestBotValid(MobID) || Progress >= (int)QuestBot[MobID].m_Talk.size())
	{
		GS()->ClearTalkText(ClientID);
		return false;
	}

	if (!GS()->CheckClient(ClientID))
		GS()->SBL(ClientID, BroadcastPriority::BROADCAST_GAME_PRIORITY, 50, "Press 'F4' to continue the dialog!");

	int BotID = QuestBot[MobID].BotID;
	char reformTalkedText[512];
	int sizeTalking = QuestBot[MobID].m_Talk.size();
	pPlayer->FormatTextQuest(BotID, QuestBot[MobID].m_Talk.at(Progress).m_TalkingText);
	str_format(reformTalkedText, sizeof(reformTalkedText), "( %d of %d ) - %s", 1 + Progress, sizeTalking, pPlayer->FormatedTalkedText());
	pPlayer->ClearFormatQuestText();

	GS()->Mmo()->BotsData()->ProcessingTalkingNPC(ClientID, TalkedID,
		QuestBot[MobID].m_Talk.at(Progress).m_PlayerTalked, reformTalkedText,
		QuestBot[MobID].m_Talk.at(Progress).m_Style, QuestBot[MobID].m_Talk.at(Progress).m_Emote);
	return true;
}

void BotJob::ShowBotQuestTaskInfo(CPlayer* pPlayer, int MobID, int Progress)
{
	int ClientID = pPlayer->GetCID();
	if (!IsQuestBotValid(MobID) || Progress >= (int)QuestBot[MobID].m_Talk.size())
	{
		GS()->ClearTalkText(ClientID);
		return;
	}

	// vanila clients
	int BotID = BotJob::QuestBot[MobID].BotID;
	int sizeTalking = BotJob::QuestBot[MobID].m_Talk.size();
	if (!GS()->CheckClient(ClientID))
	{
		char reformTalkedText[512];
		pPlayer->FormatTextQuest(BotID, BotJob::QuestBot[MobID].m_Talk.at(Progress).m_TalkingText);
		str_format(reformTalkedText, sizeof(reformTalkedText), "( %d of %d ) - %s", 1 + Progress, sizeTalking, pPlayer->FormatedTalkedText());
		pPlayer->ClearFormatQuestText();
		GS()->Mmo()->Quest()->QuestTableShowRequired(pPlayer, BotJob::QuestBot[MobID], reformTalkedText);
		return;
	}

	// mmo clients
	GS()->Mmo()->Quest()->QuestTableShowRequired(pPlayer, BotJob::QuestBot[MobID], "\0");
}

bool BotJob::IsGiveNPCQuest(int MobID) const
{
	if (!IsNpcBotValid(MobID))
		return false;
		
	for (const auto& npc : NpcBot[MobID].m_Talk)
	{
		if (npc.m_GivingQuest > 0)
			return true;
	}
	return false;
}


// Загрузка основной информации о ботах
void BotJob::LoadMainInformationBots()
{
	if(DataBot.size() > 0)
		return;

	boost::scoped_ptr<ResultSet> RES(SJK.SD("*", "tw_bots_world"));
	while(RES->next())
	{
		int BotID = (int)RES->getInt("ID");
		str_copy(DataBot[BotID].NameBot, RES->getString("BotName").c_str(), sizeof(DataBot[BotID].NameBot));

		if(!sscanf(RES->getString("SkinName").c_str(), "%s %s %s %s %s %s",
			DataBot[BotID].SkinNameBot[SKINPART_BODY], DataBot[BotID].SkinNameBot[SKINPART_MARKING],
			DataBot[BotID].SkinNameBot[SKINPART_DECORATION], DataBot[BotID].SkinNameBot[SKINPART_HANDS],
			DataBot[BotID].SkinNameBot[SKINPART_FEET], DataBot[BotID].SkinNameBot[SKINPART_EYES]))
			dbg_msg("Error", "Mised bots information");

		if(!sscanf(RES->getString("SkinColor").c_str(), "%d %d %d %d %d %d",
			&DataBot[BotID].SkinColorBot[SKINPART_BODY], &DataBot[BotID].SkinColorBot[SKINPART_MARKING],
			&DataBot[BotID].SkinColorBot[SKINPART_DECORATION], &DataBot[BotID].SkinColorBot[SKINPART_HANDS],
			&DataBot[BotID].SkinColorBot[SKINPART_FEET], &DataBot[BotID].SkinColorBot[SKINPART_EYES]))
			dbg_msg("Error", "Mised bots information");

		for(int j = SKINPART_BODY; j < NUM_SKINPARTS; j++) {
			if(DataBot[BotID].SkinColorBot[j] != 0)
				DataBot[BotID].UseCustomBot[j] = true;
		}
		DataBot[BotID].EquipSlot[EQUIP_WINGS] = RES->getInt("SlotWings");
		DataBot[BotID].EquipSlot[EQUIP_HAMMER] = RES->getInt("SlotHammer");
		DataBot[BotID].EquipSlot[EQUIP_GUN] = RES->getInt("SlotGun");
		DataBot[BotID].EquipSlot[EQUIP_SHOTGUN] = RES->getInt("SlotShotgun");
		DataBot[BotID].EquipSlot[EQUIP_GRENADE] = RES->getInt("SlotGrenade");
		DataBot[BotID].EquipSlot[EQUIP_RIFLE] = RES->getInt("SlotRifle");
	}
}

// Загрузка Quest Bots
void BotJob::LoadQuestBots(const char* pWhereLocalWorld)
{
	boost::scoped_ptr<ResultSet> RES(SJK.SD("*", "tw_bots_quest", pWhereLocalWorld));
	while(RES->next())
	{
		// it for every world initilize quest progress size
		const int MobID = (int)RES->getInt("ID");
		QuestBot[MobID].SubBotID = MobID;
		QuestBot[MobID].BotID = (int)RES->getInt("BotID");
		QuestBot[MobID].QuestID = RES->getInt("QuestID");
		QuestBot[MobID].WorldID = (int)RES->getInt("WorldID");;
		QuestBot[MobID].PositionX = (int)RES->getInt("pos_x");
		QuestBot[MobID].PositionY = (int)RES->getInt("pos_y") + 1;
		QuestBot[MobID].ItemSearch[0] = (int)RES->getInt("it_need_0");
		QuestBot[MobID].ItemSearch[1] = (int)RES->getInt("it_need_1");
		QuestBot[MobID].ItemGives[0] = (int)RES->getInt("it_reward_0");
		QuestBot[MobID].ItemGives[1] = (int)RES->getInt("it_reward_1");
		QuestBot[MobID].NeedMob[0] = (int)RES->getInt("mob_0");
		QuestBot[MobID].NeedMob[1] = (int)RES->getInt("mob_1");
		QuestBot[MobID].InteractiveType = (int)RES->getInt("interactive_type");
		QuestBot[MobID].InteractiveTemp = (int)RES->getInt("interactive_temp");

		sscanf(RES->getString("it_count").c_str(), "|%d|%d|%d|%d|%d|%d|",
			&QuestBot[MobID].ItemSearchCount[0], &QuestBot[MobID].ItemSearchCount[1],
			&QuestBot[MobID].ItemGivesCount[0], &QuestBot[MobID].ItemGivesCount[1],
			&QuestBot[MobID].NeedMobCount[0], &QuestBot[MobID].NeedMobCount[1]);

		// загрузить разговоры NPC
		boost::scoped_ptr<ResultSet> RES(SJK.SD("*", "tw_talk_quest_npc", "WHERE MobID = '%d'", MobID));
		while(RES->next())
		{
			TalkingData LoadTalk;
			LoadTalk.m_Emote = RES->getInt("TalkingEmote");
			LoadTalk.m_Style = RES->getInt("Style");
			LoadTalk.m_PlayerTalked = RES->getBoolean("PlayerTalked");
			LoadTalk.m_RequestComplete = RES->getBoolean("RequestComplete");
			str_copy(LoadTalk.m_TalkingText, RES->getString("TalkText").c_str(), sizeof(LoadTalk.m_TalkingText));
			QuestBot[MobID].m_Talk.push_back(LoadTalk);
		}
	}

	for(auto& qparseprogress : QuestBot)
	{
		qparseprogress.second.Progress = 1;
		for(const auto& qbots : QuestBot)
		{
			if(qbots.second.QuestID != qparseprogress.second.QuestID)
				continue;
			if(qbots.first == qparseprogress.first)
				break;
			qparseprogress.second.Progress++;
		}
	}
}

// Загрузка обычных NPC
void BotJob::LoadNpcBots(const char* pWhereLocalWorld)
{
	boost::scoped_ptr<ResultSet> RES(SJK.SD("*", "tw_bots_npc", pWhereLocalWorld));
	while(RES->next())
	{
		int MobID = (int)RES->getInt("ID");
		NpcBot[MobID].WorldID = RES->getInt("WorldID");
		NpcBot[MobID].Static = RES->getBoolean("Static");
		NpcBot[MobID].PositionX = RES->getInt("PositionX");
		NpcBot[MobID].PositionY = (NpcBot[MobID].Static ? RES->getInt("PositionY") + 1 : RES->getInt("PositionY"));
		NpcBot[MobID].Emote = RES->getInt("Emote");
		NpcBot[MobID].BotID = RES->getInt("BotID");
		NpcBot[MobID].Function = RES->getInt("Function");

		int CountMobs = RES->getInt("Count");
		for(int c = 0; c < CountMobs; c++)
			GS()->CreateBot(BotsTypes::TYPE_BOT_NPC, NpcBot[MobID].BotID, MobID);

		// загрузить разговоры NPC
		boost::scoped_ptr<ResultSet> RES(SJK.SD("*", "tw_talk_other_npc", "WHERE MobID = '%d'", MobID));
		while(RES->next())
		{
			TalkingData LoadTalk;
			LoadTalk.m_Emote = RES->getInt("TalkingEmote");
			LoadTalk.m_Style = RES->getInt("Style");
			LoadTalk.m_PlayerTalked = RES->getBoolean("PlayerTalked");
			LoadTalk.m_GivingQuest = RES->getInt("GivingQuest");
			str_copy(LoadTalk.m_TalkingText, RES->getString("TalkText").c_str(), sizeof(LoadTalk.m_TalkingText));
			NpcBot[MobID].m_Talk.push_back(LoadTalk);
		}
	}
}

// Загрузка мобов
void BotJob::LoadMobsBots(const char* pWhereLocalWorld)
{
	boost::scoped_ptr<ResultSet> RES(SJK.SD("*", "tw_bots_mobs", pWhereLocalWorld));
	while(RES->next())
	{
		int MobID = (int)RES->getInt("ID");
		int BotID = RES->getInt("BotID");
		MobBot[MobID].WorldID = RES->getInt("WorldID");
		MobBot[MobID].PositionX = RES->getInt("PositionX");
		MobBot[MobID].PositionY = RES->getInt("PositionY");
		MobBot[MobID].Power = RES->getInt("Power");
		MobBot[MobID].Spread = RES->getInt("Spread");
		MobBot[MobID].Boss = RES->getBoolean("Boss");
		MobBot[MobID].Level = RES->getInt("Level");
		MobBot[MobID].RespawnTick = RES->getInt("Respawn");
		MobBot[MobID].BotID = BotID;

		char aBuf[32];
		for(int i = 0; i < 6; i++)
		{
			str_format(aBuf, sizeof(aBuf), "it_drop_%d", i);
			MobBot[MobID].DropItem[i] = RES->getInt(aBuf);
		}

		sscanf(RES->getString("it_count").c_str(), "|%d|%d|%d|%d|%d|%d|",
			&MobBot[MobID].CountItem[0], &MobBot[MobID].CountItem[1], &MobBot[MobID].CountItem[2],
			&MobBot[MobID].CountItem[3], &MobBot[MobID].CountItem[4], &MobBot[MobID].CountItem[5]);

		sscanf(RES->getString("it_random").c_str(), "|%d|%d|%d|%d|%d|%d|",
			&MobBot[MobID].RandomItem[0], &MobBot[MobID].RandomItem[1], &MobBot[MobID].RandomItem[2],
			&MobBot[MobID].RandomItem[3], &MobBot[MobID].RandomItem[4], &MobBot[MobID].RandomItem[5]);

		int CountMobs = RES->getInt("Count");
		for(int c = 0; c < CountMobs; c++)
			GS()->CreateBot(BotsTypes::TYPE_BOT_MOB, BotID, MobID);
	}
}
