/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/shared/config.h>
#include <game/server/gamecontext.h>
#include "PlayerQuests.h"

#include <teeother/tl/nlohmann_json.h>
using json = nlohmann::json;

CDataQuest& CPlayerQuest::Info() const { return QuestJob::ms_aDataQuests[m_QuestID]; }
std::string CPlayerQuest::GetJsonName() const { return Info().GetJsonName(m_pPlayer->Acc().m_AuthID); }

void CPlayerQuest::InitSteps()
{
	if(m_State != QuestState::QUEST_ACCEPT || !m_pPlayer)
		return;

	// checking dir
	if(!fs_is_dir("server_data/quest_tmp"))
	{
		fs_makedir("server_data");
		fs_makedir("server_data/quest_tmp");
	}

	// initialized quest steps
	m_Step = 1;
	m_StepsQuestBot = Info().CopyBasicSteps();
	json JsonQuestData;
	JsonQuestData["current_step"] = m_Step;
	for(auto& pStep : m_StepsQuestBot)
	{
		pStep.second.m_MobProgress[0] = 0;
		pStep.second.m_MobProgress[1] = 0;
		pStep.second.m_StepComplete = false;
		pStep.second.m_ClientQuitting = false;
		pStep.second.UpdateBot(m_pPlayer->GS());
		pStep.second.CreateStepArrow(m_pPlayer);

		JsonQuestData["steps"].push_back(
		{
			{ "subbotid", pStep.second.m_Bot->m_SubBotID },
			{ "mobprogress1", pStep.second.m_MobProgress[0] },
			{ "mobprogress2", pStep.second.m_MobProgress[1] },
			{ "state", pStep.second.m_StepComplete }
		});
	}

	// save file
	IOHANDLE File = io_open(GetJsonName().c_str(), IOFLAG_WRITE);
	if(!File)
		return;

	std::string Data = JsonQuestData.dump();
	io_write(File, Data.c_str(), Data.length());
	io_close(File);
}

void CPlayerQuest::LoadSteps()
{
	if(m_State != QuestState::QUEST_ACCEPT || !m_pPlayer)
		return;

	// loading file is not open pereinitilized steps
	IOHANDLE File = io_open(GetJsonName().c_str(), IOFLAG_READ);
	if(!File)
	{
		InitSteps();
		return;
	}

	const int FileSize = (int)io_length(File) + 1;
	char* pFileData = (char*)malloc(FileSize);
	mem_zero(pFileData, FileSize);
	io_read(File, pFileData, FileSize);

	// close and clear
	json JsonQuestData = json::parse(pFileData);
	mem_free(pFileData);
	io_close(File);

	// loading steps
	m_StepsQuestBot = Info().CopyBasicSteps();
	m_Step = JsonQuestData.value("current_step", 1);
	auto Steps = JsonQuestData["steps"];
	for(auto& pStep : Steps)
	{
		const int SubBotID = pStep.value("subbotid", 0);
		m_StepsQuestBot[SubBotID].m_StepComplete = pStep.value("state", false);
		m_StepsQuestBot[SubBotID].m_MobProgress[0] = pStep.value("mobprogress1", 0);
		m_StepsQuestBot[SubBotID].m_MobProgress[1] = pStep.value("mobprogress2", 0);
		m_StepsQuestBot[SubBotID].m_ClientQuitting = false;
		m_StepsQuestBot[SubBotID].UpdateBot(m_pPlayer->GS());
		m_StepsQuestBot[SubBotID].CreateStepArrow(m_pPlayer);
	}
}

void CPlayerQuest::SaveSteps()
{
	if(m_State != QuestState::QUEST_ACCEPT)
		return;

	// set json structure
	json JsonQuestData;
	JsonQuestData["current_step"] = m_Step;
	for(auto& pStep : m_StepsQuestBot)
	{
		JsonQuestData["steps"].push_back(
		{
			{ "subbotid", pStep.second.m_Bot->m_SubBotID },
			{ "mobprogress1", pStep.second.m_MobProgress[0] },
			{ "mobprogress2", pStep.second.m_MobProgress[1] },
			{ "state", pStep.second.m_StepComplete }
		});
	}

	// replace file
	IOHANDLE File = io_open(GetJsonName().c_str(), IOFLAG_WRITE);
	if(!File)
		return;

	std::string Data = JsonQuestData.dump();
	io_write(File, Data.c_str(), Data.length());
	io_close(File);
}

void CPlayerQuest::ClearSteps()
{
	for(auto& pStepBot : m_StepsQuestBot)
	{
		pStepBot.second.UpdateBot(m_pPlayer->GS());
		pStepBot.second.CreateStepArrow(m_pPlayer);
	}
	m_StepsQuestBot.clear();
	fs_remove(GetJsonName().c_str());
}

bool CPlayerQuest::Accept()
{
	if(m_State != QuestState::QUEST_NO_ACCEPT)
		return false;

	// init quest
	m_State = QuestState::QUEST_ACCEPT;
	SJK.ID("tw_accounts_quests", "(QuestID, OwnerID, Type) VALUES ('%d', '%d', '%d')", m_QuestID, m_pPlayer->Acc().m_AuthID, QuestState::QUEST_ACCEPT);

	// init steps
	InitSteps();

	// information
	CGS* pGS = m_pPlayer->GS();
	const int ClientID = m_pPlayer->GetCID();
	const int StorySize = Info().GetStoryCount();
	const int StoryProgress = Info().GetStoryCount(m_QuestID + 1);
	pGS->Chat(ClientID, "Accepted the quest ({STR} - {STR} {INT}/{INT})!", Info().GetStory(), Info().GetName(), &StoryProgress, &StorySize);
	pGS->Chat(ClientID, "Reward for completing (Gold {INT}, Experience {INT})!", &Info().m_Gold, &Info().m_Exp);
	pGS->CreatePlayerSound(ClientID, SOUND_CTF_CAPTURE);
	return true;
}

void CPlayerQuest::Finish()
{
	if(m_State != QuestState::QUEST_ACCEPT)
		return;

	// finish quest
	m_State = QuestState::QUEST_FINISHED;
	SJK.UD("tw_accounts_quests", "Type = '%d' WHERE QuestID = '%d' AND OwnerID = '%d'", m_State, m_QuestID, m_pPlayer->Acc().m_AuthID);

	// clear steps
	ClearSteps();

	// awards and write about completion
	CGS* pGS = m_pPlayer->GS();
	const int ClientID = m_pPlayer->GetCID();
	m_pPlayer->AddMoney(Info().m_Gold);
	m_pPlayer->AddExp(Info().m_Exp);
	pGS->Chat(-1, "{STR} completed ({STR} - {STR})!", pGS->Server()->ClientName(ClientID), Info().m_aStoryLine, Info().m_aName);
	pGS->ChatDiscord(DC_PLAYER_INFO, pGS->Server()->ClientName(ClientID), "Completed ({STR} - {STR})", Info().m_aStoryLine, Info().m_aName);

	// check whether the after quest has opened something new
	pGS->Mmo()->WorldSwap()->CheckQuestingOpened(m_pPlayer, m_QuestID);
	pGS->Mmo()->Dungeon()->CheckQuestingOpened(m_pPlayer, m_QuestID);

	// save player stats and accept next story quest
	pGS->Mmo()->SaveAccount(m_pPlayer, SaveType::SAVE_STATS);
	pGS->Mmo()->Quest()->AcceptNextStoryQuestStep(m_pPlayer, m_QuestID);
}

void CPlayerQuest::CheckaAvailableNewStep()
{
	// check whether the active steps is complete
	auto pActiveStep = std::find_if(m_StepsQuestBot.begin(), m_StepsQuestBot.end(), [this](std::pair <const int, CPlayerStepQuestBot> &pItem)
	{ return (pItem.second.m_Bot->m_Step == m_Step && !pItem.second.m_StepComplete); });
	if(pActiveStep != m_StepsQuestBot.end())
		return;

	// update steps
	m_Step++;
	SaveSteps();

	// check if all steps have been completed
	bool FinalStep = true;
	CGS* pGS = m_pPlayer->GS();
	for(auto& pStepBot : m_StepsQuestBot)
	{
		if(!pStepBot.second.m_StepComplete)
			FinalStep = false;

		pStepBot.second.UpdateBot(pGS);
		pStepBot.second.CreateStepArrow(m_pPlayer);
	}
	
	// finish the quest or update the step
	if(FinalStep)
	{
		Finish();
		const int ClientID = m_pPlayer->GetCID();
		pGS->StrongUpdateVotes(ClientID, MenuList::MENU_JOURNAL_MAIN);
		pGS->StrongUpdateVotes(ClientID, MenuList::MAIN_MENU);
	}
}
