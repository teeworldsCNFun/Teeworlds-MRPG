/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/shared/config.h>
#include <game/server/gamecontext.h>
#include "items.h"

using namespace sqlstr;

std::map < int , std::map < int , ItemSql::ItemPlayer > > ItemSql::Items;
std::map < int , ItemSql::ItemInformation > ItemSql::ItemsInfo;

// Инициализация класса
void ItemSql::OnInitGlobal() 
{ 
	{ // загрузить информацию предметов
		boost::scoped_ptr<ResultSet> RES(SJK.SD("*", "tw_items_list", "WHERE ItemID > '0'"));
		while(RES->next())
		{
			const int ItemID = (int)RES->getInt("ItemID");
			str_copy(ItemsInfo[ItemID].iItemName, RES->getString("ItemName").c_str(), sizeof(ItemsInfo[ItemID].iItemName));
			str_copy(ItemsInfo[ItemID].iItemDesc, RES->getString("ItemDesc").c_str(), sizeof(ItemsInfo[ItemID].iItemDesc));
			str_copy(ItemsInfo[ItemID].iItemIcon, RES->getString("ItemIcon").c_str(), sizeof(ItemsInfo[ItemID].iItemIcon));
			ItemsInfo[ItemID].Type = (int)RES->getInt("ItemType");
			ItemsInfo[ItemID].Function = (int)RES->getInt("ItemFunction");
			ItemsInfo[ItemID].Notify = (bool)RES->getBoolean("ItemMessage");
			ItemsInfo[ItemID].Dysenthis = (int)RES->getInt("ItemDesynthesis");
			ItemsInfo[ItemID].MinimalPrice = (int)RES->getInt("ItemAuctionPrice");
			ItemsInfo[ItemID].BonusID = (int)RES->getInt("ItemBonus");
			ItemsInfo[ItemID].BonusCount = (int)RES->getInt("ItemBonusCount");
			ItemsInfo[ItemID].MaximalEnchant = (int)RES->getInt("ItemEnchantMax");
			ItemsInfo[ItemID].iItemEnchantPrice = (int)RES->getInt("ItemEnchantPrice");
			ItemsInfo[ItemID].Dropable = (bool)RES->getBoolean("ItemDropable");
		}
	}

	// загрузить аттрибуты
	boost::scoped_ptr<ResultSet> RES(SJK.SD("*", "tw_attributs", "WHERE ID > '0'"));
	while(RES->next())
	{
		int ID = RES->getInt("ID");
		str_copy(CGS::AttributInfo[ID].Name, RES->getString("name").c_str(), sizeof(CGS::AttributInfo[ID].Name));
		str_copy(CGS::AttributInfo[ID].FieldName, RES->getString("field_name").c_str(), sizeof(CGS::AttributInfo[ID].FieldName));
		CGS::AttributInfo[ID].ProcentID = RES->getInt("procent_id");
		CGS::AttributInfo[ID].UpgradePrice = RES->getInt("price");
		CGS::AttributInfo[ID].AtType = RES->getInt("at_type");		
	}
}

// Загрузка данных игрока
void ItemSql::OnInitAccount(CPlayer *pPlayer)
{
	int ClientID = pPlayer->GetCID();
	boost::scoped_ptr<ResultSet> RES(SJK.SD("ItemID, ItemCount, ItemSettings, ItemEnchant, ItemDurability", "tw_items", "WHERE OwnerID = '%d'", pPlayer->Acc().AuthID));
	while(RES->next())
	{
		const int ItemID = (int)RES->getInt("ItemID");
		Items[ClientID][ItemID].Count = (int)RES->getInt("ItemCount");
		Items[ClientID][ItemID].Settings = (int)RES->getInt("ItemSettings");
		Items[ClientID][ItemID].Enchant = (int)RES->getInt("ItemEnchant");
		Items[ClientID][ItemID].Durability = (int)RES->getInt("ItemDurability");
		Items[ClientID][ItemID].SetBasic(pPlayer, ItemID);
	}		
}

// Восстановить прочность всем предметам
void ItemSql::RepairDurability(CPlayer *pPlayer)
{ 
	int ClientID = pPlayer->GetCID();
	SJK.UD("tw_items", "ItemDurability = '100' WHERE OwnerID = '%d'", pPlayer->Acc().AuthID);
	for(auto& it : Items[ClientID])
		it.second.Durability = 100;
}

// Установить прочность предмету
bool ItemSql::SetDurabilityItem(CPlayer *pPlayer, int ItemID, int Durability)
{
	ResultSet* RES = SJK.SD("ID, ItemSettings", "tw_items", "WHERE ItemID = '%d' AND OwnerID = '%d'", ItemID, pPlayer->Acc().AuthID);
	if(RES->next())
	{
		const int ID = RES->getInt("ID"); 
		SJK.UD("tw_items", "ItemDurability = '%d' WHERE ID = '%d'", Durability, ID);
		pPlayer->GetItem(ItemID).Durability = Durability;
		return true;		
	}	
	return false;	
}

// Устанавливаем настройку предмету
bool ItemSql::SetSettingsItem(CPlayer *pPlayer, int ItemID, int Settings)
{
	// устанавливаем настройку и сохраняем
	boost::scoped_ptr<ResultSet> RES(SJK.SD("ID, ItemSettings", "tw_items", "WHERE ItemID = '%d' AND OwnerID = '%d'", ItemID, pPlayer->Acc().AuthID));
	if(RES->next())
	{
		const int ID = RES->getInt("ID"); 
		SJK.UD("tw_items", "ItemSettings = '%d' WHERE ID = '%d'", Settings, ID);
		pPlayer->GetItem(ItemID).Settings = Settings;
		return true;
	}	
	return false;		
}

// Устанавливаем настройку предмету
bool ItemSql::SetEnchantItem(CPlayer *pPlayer, int ItemID, int Enchant)
{
	// устанавливаем настройку и сохраняем
	boost::scoped_ptr<ResultSet> RES(SJK.SD("ID, ItemEnchant", "tw_items", "WHERE ItemID = '%d' AND OwnerID = '%d'", ItemID, pPlayer->Acc().AuthID));
	if(RES->next())
	{
		const int ID = RES->getInt("ID"); 
		SJK.UD("tw_items", "ItemEnchant = '%d' WHERE ID = '%d'", Enchant, ID);
		pPlayer->GetItem(ItemID).Enchant = Enchant;
		return true;		
	}	
	return false;		
}

// Лист предметов
void ItemSql::ListInventory(CPlayer *pPlayer, int TypeList, bool SortedFunction)
{
	const int ClientID = pPlayer->GetCID();
	GS()->AV(ClientID, "null", "");

	// показываем лист предметов игроку 
	bool Found = false;	
	for(const auto& it : Items[ClientID])
	{
		if(!it.second.Count || (SortedFunction && it.second.Info().Function != TypeList || !SortedFunction && it.second.Info().Type != TypeList))
			continue;

		ItemSelected(pPlayer, it.second);
		Found = true;
	}
	if(!Found) GS()->AVL(ClientID, "null", _("There are no items in this tab"), NULL);
}

// Выдаем предмет обработка повторная
void ItemSql::GiveItem(short *SecureCode, CPlayer *pPlayer, int ItemID, int Count, int Settings, int Enchant)
{
	// проверяем выдан ли и вправду предмет
	const int ClientID = pPlayer->GetCID();
	*SecureCode = SecureCheck(pPlayer, ItemID, Count, Settings, Enchant);
	if(*SecureCode != 1) return;

	// обновляем значения в базе
	SJK.UD("tw_items", "ItemCount = '%d', ItemSettings = '%d', ItemEnchant = '%d' WHERE ItemID = '%d' AND OwnerID = '%d'", 
		Items[ClientID][ItemID].Count, Items[ClientID][ItemID].Settings, Items[ClientID][ItemID].Enchant, ItemID, pPlayer->Acc().AuthID);
}

// Выдаем предмет обработка первичная
int ItemSql::SecureCheck(CPlayer *pPlayer, int ItemID, int Count, int Settings, int Enchant)
{
	// проверяем инициализируем и добавляем предмет
	const int ClientID = pPlayer->GetCID();
	std::unique_ptr<ResultSet> RES(SJK.SD("ItemCount, ItemSettings", "tw_items", "WHERE ItemID = '%d' AND OwnerID = '%d'", ItemID, pPlayer->Acc().AuthID));
	if(RES->next())
	{
		Items[ClientID][ItemID].Count = RES->getInt("ItemCount")+Count;
		Items[ClientID][ItemID].Settings = RES->getInt("ItemSettings")+Settings;
		Items[ClientID][ItemID].Enchant = Enchant;
		return 1;	
	}
	// создаем предмет если не найден
	Items[ClientID][ItemID].Count = Count;
	Items[ClientID][ItemID].Settings = Settings;
	Items[ClientID][ItemID].Enchant = Enchant;
	Items[ClientID][ItemID].Durability = 100;
	SJK.ID("tw_items", "(ItemID, OwnerID, ItemCount, ItemSettings, ItemEnchant) VALUES ('%d', '%d', '%d', '%d', '%d');",
		ItemID, pPlayer->Acc().AuthID, Count, Settings, Enchant);
	return 2;
}

// удаляем предмет второстепенная обработка
void ItemSql::RemoveItem(short *SecureCode, CPlayer *pPlayer, int ItemID, int Count, int Settings)
{
	const int ClientID = pPlayer->GetCID();
	*SecureCode = DeSecureCheck(pPlayer, ItemID, Count, Settings);
	if(*SecureCode != 1) return;

	// если прошла и предметов больше удаляемых то обновляем таблицу
	SJK.UD("tw_items", "ItemCount = ItemCount - '%d', ItemSettings = ItemSettings - '%d' "
		"WHERE ItemID = '%d' AND OwnerID = '%d'", Count, Settings, ItemID, pPlayer->Acc().AuthID);	
}

// удаление предмета первостепенная обработка
int ItemSql::DeSecureCheck(CPlayer *pPlayer, int ItemID, int Count, int Settings)
{
	// проверяем в базе данных и проверяем 
	const int ClientID = pPlayer->GetCID();
	boost::scoped_ptr<ResultSet> RES(SJK.SD("ItemCount, ItemSettings", "tw_items", "WHERE ItemID = '%d' AND OwnerID = '%d'", ItemID, pPlayer->Acc().AuthID));
	if(RES->next())
	{
		// обновляем если количество больше
		if(RES->getInt("ItemCount") > Count)
		{
			Items[ClientID][ItemID].Count = RES->getInt("ItemCount")-Count;
			Items[ClientID][ItemID].Settings = RES->getInt("ItemSettings")-Settings;
			return 1;		
		}
		// удаляем предмет если кол-во меньше положенного
		Items[ClientID][ItemID].Count = 0;
		Items[ClientID][ItemID].Settings = 0;
		Items[ClientID][ItemID].Enchant = 0;
		SJK.DD("tw_items", "WHERE ItemID = '%d' AND OwnerID = '%d'", ItemID, pPlayer->Acc().AuthID);
		return 2;		
	}
	// суда мы заходим если предметов нет и нечего удалять
	Items[ClientID][ItemID].Count = 0;
	Items[ClientID][ItemID].Settings = 0;
	Items[ClientID][ItemID].Enchant = 0;	
	return 0;		
}

bool ItemSql::OnParseVotingMenu(CPlayer *pPlayer, const char *CMD, const int VoteID, const int VoteID2, int Get, const char *GetText)
{
	const int ClientID = pPlayer->GetCID();

	// сортировка предметов
	if(PPSTR(CMD, "SORTEDINVENTORY") == 0)
	{
		pPlayer->m_SortTabs[SORTINVENTORY] = VoteID;
		GS()->VResetVotes(ClientID, INVENTORY);
		return true;
	}

	// выброс предмета
	if(PPSTR(CMD, "IREPAIRDROP") == 0)
	{
		ItemPlayer &PlItem = pPlayer->GetItem(VoteID);
		if(Get > PlItem.Count)
			Get = PlItem.Count;

		const int Enchant = PlItem.Enchant;
		GS()->SBL(ClientID, PRELEGENDARY, 100, _("You drop {s:name}x{i:count}"), 
			"name", PlItem.Info().GetName(pPlayer), "count", &Get, NULL);
		GS()->CreateDropItem(pPlayer->GetCharacter()->m_Core.m_Pos, -1, PlItem, Get);
		GS()->ResetVotes(ClientID, pPlayer->m_OpenVoteMenu);
		return true;
	}

	// использование предмета
	if(PPSTR(CMD, "IUSE") == 0)
	{
		// проверяем если по функции он используется 1 раз
		ItemPlayer &PlItem = pPlayer->GetItem(VoteID);
		if(Get > PlItem.Count)
			Get = PlItem.Count;

		if(PlItem.Info().Function == ONEUSEDS)
			Get = 1;

		UsedItems(ClientID, VoteID, Get);
		return true;
	}

	// уничтожение предмета
	if(PPSTR(CMD, "IDESTROY") == 0)
	{
		ItemPlayer &PlItem = pPlayer->GetItem(VoteID);
		if(Get > PlItem.Count)
			Get = PlItem.Count;

		if(PlItem.Remove(Get))
		{
			GS()->Chat(ClientID, "Destroy {STR}x{INT}", PlItem.Info().GetName(pPlayer), &Get);
			GS()->ResetVotes(ClientID, pPlayer->m_OpenVoteMenu);
		}
		return true;
	}

	// десинтез предмета
	if(PPSTR(CMD, "IDESYNTHESIS") == 0)
	{
		ItemPlayer &PlItem = pPlayer->GetItem(VoteID);
		if(Get > PlItem.Count)
			Get = PlItem.Count;

		const int ItemGive = (PlItem.Info().Function == ITPLANTS ? itGoods : itMaterial);
		ItemPlayer &ItemMaterial = pPlayer->GetItem(ItemGive); 

		int DesCount = PlItem.Info().Dysenthis*Get;
		if(PlItem.Remove(Get) && ItemMaterial.Add(DesCount))
		{
			GS()->Chat(ClientID, "Desynthesis {STR}x{INT}, you receive {INT} {STR}", 
				PlItem.Info().GetName(pPlayer), &Get, &DesCount, ItemMaterial.Info().GetName(pPlayer));
			GS()->ResetVotes(ClientID, pPlayer->m_OpenVoteMenu);
		}
		return true;
	}

	// настройка предмета
	if(PPSTR(CMD, "ISETTINGS") == 0)
	{
		pPlayer->GetItem(VoteID).EquipItem();
		if(GS()->GetItemInfo(VoteID).Type == ITEMEQUIP)
		{
			GS()->Mmo()->SaveAccount(pPlayer, SAVESTATS);
			GS()->ChangeEquipSkin(ClientID, VoteID);
		}
		GS()->ResetVotes(ClientID, pPlayer->m_OpenVoteMenu);
		return true;
	}

	// зачарование
	if(PPSTR(CMD, "IENCHANT") == 0)
	{
		ItemPlayer &PlItem = pPlayer->GetItem(VoteID);
		if(PlItem.Enchant >= PlItem.Info().MaximalEnchant)
		{
			GS()->Chat(ClientID, "You enchant max level for this item!");
			return true;			
		}

		int Price = PlItem.EnchantMaterCount();
		ItemPlayer &PlMaterial = pPlayer->GetItem(itMaterial);
		if(Price > PlMaterial.Count)
		{
			GS()->Chat(ClientID, "You need {INT} Your {INT} materials!", &Price, &PlMaterial.Count);
			return true;
		}

		if(PlMaterial.Remove(Price, 0))
		{
			int EnchantLevel = PlItem.Enchant+1;
			int BonusID = PlItem.Info().BonusID;
			int BonusCount = PlItem.Info().BonusCount*(EnchantLevel+1);

			PlItem.SetEnchant(EnchantLevel);
			GS()->Chat(-1, "{STR} enchant {STR}+{INT}({STR} +{INT})", 
				GS()->Server()->ClientName(ClientID), PlItem.Info().GetName(), &EnchantLevel, pPlayer->AtributeName(BonusID), &BonusCount);
		
			if(EnchantLevel >= EFFECTENCHANT)
				GS()->SendEquipItem(ClientID, -1);

			GS()->ResetVotes(ClientID, pPlayer->m_OpenVoteMenu);
		}
		return true;
	}

	// сортировка надевания
	if(PPSTR(CMD, "SORTEDEQUIP") == 0)
	{
		pPlayer->m_SortTabs[SORTEQUIP] = VoteID;
		GS()->VResetVotes(ClientID, EQUIPMENU);
		return true;				
	}
	return false;
}

// функция использования предметов
void ItemSql::UsedItems(int ClientID, int ItemID, int Count)
{
	CPlayer *pPlayer = GS()->GetPlayer(ClientID, true, true);
	if(!pPlayer || pPlayer->GetItem(ItemID).Count < Count) return;

	ItemPlayer &PlItem = pPlayer->GetItem(ItemID);
	if(ItemID == itPotionHealthRegen && PlItem.Remove(Count, 0))
	{
		pPlayer->GiveEffect("RegenHealth", 20);
		GS()->ChatFollow(ClientID, "You used {STR}x{INT}", PlItem.Info().GetName(pPlayer), &Count);
	}
	
	if(ItemID == itPotionQuenchingHunger && PlItem.Remove(Count, 0))
	{
		pPlayer->Acc().Hungry = clamp(pPlayer->Acc().Hungry + 30, 30, 100);
		GS()->ChatFollow(ClientID, "You used {STR}x{INT}", PlItem.Info().GetName(pPlayer), &Count);
		Job()->SaveAccount(pPlayer, SAVESTATS);
	}

	if(ItemID == itCapsuleSurvivalExperience && PlItem.Remove(Count, 0))
	{
		int UseCount = 500*Count;
		UseCount += rand()%UseCount;
		GS()->Chat(-1, "{STR} used {STR}x{INT} and got {INT} Survival Experience.", 
			"name", GS()->Server()->ClientName(ClientID), "item", PlItem.Info().GetName(pPlayer), "itemcount", &Count, "count", &UseCount);
		pPlayer->AddExp(UseCount);		
	}

	if(ItemID == itLittleBagGold && PlItem.Remove(Count, 0))
	{
		int UseCount = 10*Count;
		UseCount += (rand()%UseCount)*5;
		GS()->Chat(-1, "{STR} used {STR}x{INT} and got {INT} gold.", 
			GS()->Server()->ClientName(ClientID), PlItem.Info().GetName(), &Count, &UseCount);
		pPlayer->AddMoney(UseCount);				
	}
	GS()->VResetVotes(ClientID, INVENTORY);
	return;
}

void ItemSql::ItemSelected(CPlayer *pPlayer, const ItemPlayer &PlItem, bool Dress)
{
	const int ClientID = pPlayer->GetCID();
	const int ItemID = PlItem.GetID();
	const int HideID = NUMHIDEMENU+ItemID;
	const char *NameItem = PlItem.Info().GetName(pPlayer);

	// зачеровыванный или нет
	if(PlItem.Info().BonusCount)
	{
		GS()->AVHI(ClientID, PlItem.Info().GetIcon(), HideID, vec3(50,30,25), _("{s:on}{s:item}+{i:enchant} ({i:dur}/100)"), 
			"on", (PlItem.Settings ? "☑ - " : "\0"), "item", NameItem, "enchant", &PlItem.Enchant, "dur", &PlItem.Durability);
	}
	else
	{
		GS()->AVHI(ClientID, PlItem.Info().GetIcon(), HideID, vec3(25,30,50), _("{s:on}{s:item} x{i:count}"), 
			"on", (PlItem.Settings ? "Dressed - " : "\0"), "item", NameItem, "count", &PlItem.Count);			
	}

	const char *DescItem = PlItem.Info().GetDesc(pPlayer);
	GS()->AVM(ClientID, "null", NOPE, HideID, _("{s:desc}"), "desc", DescItem, NULL);

	// квестовыый предмет
	if(PlItem.Info().Type == ITEMQUEST) return;

	// бонус предметов
	if(CGS::AttributInfo.find(PlItem.Info().BonusID) != CGS::AttributInfo.end() && PlItem.Info().BonusCount)
	{
		int BonusCountAct = PlItem.Info().BonusCount*(PlItem.Enchant+1);
		GS()->AVM(ClientID, "null", NOPE, HideID, _("Astro stats +{i:bonus} {s:name}"), 
			"bonus", &BonusCountAct, "name", pPlayer->AtributeName(PlItem.Info().BonusID), NULL);
	}

	// используемое или нет
	if(PlItem.Info().Function == ONEUSEDS || PlItem.Info().Function == USEDS)
	{
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "Bind command \"/useitem %d'\"", ItemID);
		GS()->AVM(ClientID, "null", NOPE, HideID, _("{s:text}"), "text", aBuf, NULL);
		GS()->AVM(ClientID, "IUSE", ItemID, HideID, _("Use {s:name}"), "name", NameItem, NULL);
	}

	// зелье или нет
	if(PlItem.Info().Type == ITEMPOTION)
	{
		GS()->AVM(ClientID, "ISETTINGS", ItemID, HideID, _("Auto use {s:item} - {s:on}"), 
			"item", NameItem, "on", (PlItem.Settings ? "Enable" : "Disable"));			
	}

	// поставить дома предмет
	if(PlItem.Info().Type == ITEMDECORATION)
		GS()->AVM(ClientID, "DECOSTART", ItemID, HideID, _("Added {s:name} to your house"), "name", NameItem, NULL);			

	// установить предмет как расстение
	if(PlItem.Info().Function == ITPLANTS)
	{
		const int HouseID = Job()->House()->OwnerHouseID(pPlayer->Acc().AuthID);
		const int PlantItemID = Job()->House()->GetPlantsID(HouseID);
		if(PlantItemID != ItemID)
			GS()->AVD(ClientID, "HOMEPLANTSET", ItemID, 20000, HideID, _("Change plants {s:name} to house (20000 items)"), "name", NameItem);
		else 
			GS()->AVL(ClientID, "null", _("▲ This plant is active in the house ▲"), "name", NameItem);
	}

	// снаряжение или настройка
	if(PlItem.Info().Type == ITEMEQUIP || PlItem.Info().Function == ITSETTINGS)
	{
		GS()->AVM(ClientID, "ISETTINGS", ItemID, HideID, _("{s:type} {s:item}"), 
			"type", (PlItem.Settings ? "Undress" : "Equip"), "item", NameItem, NULL);
	}

	// десинтез или уничтожение
	if(PlItem.Info().Dysenthis > 0)
	{
		GS()->AVM(ClientID, "IDESYNTHESIS", ItemID, HideID, _("Desynthesis {s:name} +{i:descount}{s:dystype}(1 item)"), 
			"name", NameItem, "descount", &PlItem.Info().Dysenthis, 
			"dystype", (PlItem.Info().Function == ITPLANTS ? "goods" : "mat"), NULL);
	}
	else 
	{
		GS()->AVM(ClientID, "IDESTROY", ItemID, HideID, _("Destroy {s:name}"), 
			"name", NameItem, NULL);			
	}

	// можно ли дропнуть 
	if(PlItem.Info().Dropable)
	{
		GS()->AVM(ClientID, "IREPAIRDROP", ItemID, HideID, _("Drop {s:repair}{s:name}"), 
			"repair", (PlItem.Info().BonusCount > 0 ? "and Repair " : ""), "name", NameItem, NULL);
	}

	if(PlItem.Info().BonusCount)
	{
		int Price = PlItem.EnchantMaterCount();
		GS()->AVM(ClientID, "IENCHANT", ItemID, HideID, _("Enchant ({s:name}+{i:enchant}) [{i:mat} material]"), 
			"name", NameItem, "enchant", &PlItem.Enchant, "mat", &Price, NULL);
	}

	// аукцион
	if(PlItem.Info().MinimalPrice)
		GS()->AVM(ClientID, "AUCTIONSLOT", ItemID, HideID, _("Create Slot Auction {s:name}"), "name", NameItem, NULL);
}


const char *ItemSql::ClassItemInformation::GetName(CPlayer *pPlayer) const
{
	if(!pPlayer) return iItemName;
	return pPlayer->GS()->Server()->Localization()->Localize(pPlayer->GetLanguage(), _(iItemName));	
}

const char *ItemSql::ClassItemInformation::GetDesc(CPlayer *pPlayer) const
{
	if(!pPlayer) return iItemDesc;
	return pPlayer->GS()->Server()->Localization()->Localize(pPlayer->GetLanguage(), _(iItemDesc));	
}

int ItemSql::ClassItems::EnchantMaterCount() const
{
	return ItemSql::ItemsInfo[itemid_].iItemEnchantPrice*(Enchant+1);
}

bool ItemSql::ClassItems::Add(int arg_count, int arg_settings, int arg_enchant, bool arg_message)
{
	if(arg_count < 1 || !pPlayer || !pPlayer->IsAuthed()) 
		return false;

	// все что потребуется для работы функции
	CGS *GameServer = pPlayer->GS();
	const int ClientID = pPlayer->GetCID();

	// бонусный предмет только один
	if(Info().BonusCount > 0)
	{
		if(Count > 0) 
		{
			pPlayer->GS()->Chat(ClientID, "This item cannot have more than 1 item");
			return false;
		}
		arg_count = 1;
	}

	// проверить пустой слот если да тогда одеть предмет
	const bool AutoEquip = (Info().Function >= EQUIP_WINGS && Info().Function < NUM_EQUIPS && pPlayer->GetItemEquip(Info().Function) == -1) 
						|| (Info().Function == ITSETTINGS && Info().BonusCount > 0); 
	if(AutoEquip)
	{
		GameServer->Chat(ClientID, "Auto equip {STR} ({STR} +{INT})!", 
			Info().GetName(pPlayer), pPlayer->AtributeName(Info().BonusID), &Info().BonusCount);
		GameServer->Chat(ClientID, "For more detail see equip/inventory/settings in vote!");

		// обновить информацию по дискорд карточки
		if(Info().Function == EQUIP_DISCORD)  
			GameServer->Mmo()->SaveAccount(pPlayer, SAVESTATS);
	}

	// выдаем предмет
	GameServer->Mmo()->Item()->GiveItem(&pPlayer->m_SecurCheckCode, pPlayer, itemid_, arg_count, (AutoEquip ? 1 : arg_settings), arg_enchant);
	if(pPlayer->m_SecurCheckCode <= 0) 
		return false;

	// автообновление инвентаря если открыт
	if(pPlayer->m_SortTabs[SORTINVENTORY] && Info().Type == pPlayer->m_SortTabs[SORTINVENTORY])
		GameServer->VResetVotes(ClientID, INVENTORY);

	// отправить смену скина
	if(AutoEquip) GameServer->ChangeEquipSkin(ClientID, itemid_);

	// проверить квест
	GameServer->Mmo()->Quest()->CheckQuest(pPlayer);

	// если тихий режим
	if(!arg_message || Info().Type == ITEMSETTINGS) return true;

	// информация о получении себе предмета
	if(Info().Type != -1 && itemid_ != itMoney)
		GameServer->Chat(ClientID, "You got of the {STR}x{INT}!", Info().GetName(pPlayer), &arg_count);
	else if(Info().Notify)
	{
		GameServer->Chat(-1, "{STR} got of the {STR}x{INT}!", GameServer->Server()->ClientName(ClientID), Info().GetName(), &arg_count);
	}
	return true;
}

bool ItemSql::ClassItems::Remove(int arg_removecount, int arg_settings)
{
	if(Count <= 0 || arg_removecount < 1 || !pPlayer) 
		return false;

	if(Count < arg_removecount)
		arg_removecount = Count;

	pPlayer->GS()->Mmo()->Item()->RemoveItem(&pPlayer->m_SecurCheckCode, pPlayer, itemid_, arg_removecount, arg_settings);
	if(pPlayer->m_SecurCheckCode <= 0) 
		return false;

	if(pPlayer->CheckEquipItem(itemid_) > -1)
	{
		int ClientID = pPlayer->GetCID();
		pPlayer->GS()->ChangeEquipSkin(ClientID, itemid_);
	}

	pPlayer->GS()->Mmo()->Quest()->CheckQuest(pPlayer);
	return true;
}

void ItemSql::ClassItems::SetEnchant(int arg_enchantlevel)
{
	if(!Count || !pPlayer) return;

	pPlayer->GS()->Mmo()->Item()->SetEnchantItem(pPlayer, itemid_, arg_enchantlevel);
}

bool ItemSql::ClassItems::SetSettings(int arg_settings)
{
	if(!Count || !pPlayer) return false;

	pPlayer->GS()->Mmo()->Item()->SetSettingsItem(pPlayer, itemid_, arg_settings);
	return true;
}

bool ItemSql::ClassItems::EquipItem()
{
	if(Count <= 0 || !pPlayer) return false;

	// если снаряжение
	int ClientID = pPlayer->GetCID();
	if(Info().Type == ITEMEQUIP)
	{
		int EquipItemID = pPlayer->GetItemEquip(Info().Function);
		while (EquipItemID >= 1)
		{
			ItemSql::ItemPlayer &EquipItem = pPlayer->GetItem(EquipItemID);
			EquipItem.Settings = 0;
			EquipItem.SetSettings(0);
			EquipItemID = pPlayer->GetItemEquip(Info().BonusID);
		}
		// добавляем инфу в броадакст
		pPlayer->AddInformationStats();
	}

	// обновляем
	Settings ^= true;
	pPlayer->GS()->Mmo()->Item()->SetSettingsItem(pPlayer, itemid_, Settings);

	// перестановка регена
	if((Info().BonusID == Stats::StAmmoRegen || Info().BonusID == Stats::StAmmoRegenQ) && pPlayer->GetCharacter())
		pPlayer->GetCharacter()->m_AmmoRegen = pPlayer->GetAttributeCount(Stats::StAmmoRegen, true);

	return true;
}

void ItemSql::ClassItems::Save()
{
	if(!pPlayer) 
		return;

	int ClientID = pPlayer->GetCID();
	SJK.UD("tw_items", "ItemCount = '%d', ItemSettings = '%d', ItemEnchant = '%d' WHERE OwnerID = '%d' AND ItemID = '%d'", 
		Count, Settings, Enchant, pPlayer->Acc().AuthID, itemid_);
}
