#include "looting.h"
#include "EqStructures.h"
#include "EqAddresses.h"
#include "EqFunctions.h"
#include "Zeal.h"
#include "string_util.h"
#include "EqPackets.h"
//void __fastcall finalize_loot(int uk, int lootwnd_ptr)
//{
//	Zeal::EqStructures::Entity* corpse =  Zeal::EqGame::get_active_corpse();
//	if (ZealService::get_instance()->gui->Options->hidecorpse_looted && corpse)
//		corpse->ActorInfo->IsInvisible = 1;
//	ZealService::get_instance()->hooks->hook_map["FinalizeLoot"]->original(finalize_loot)(uk, lootwnd_ptr);
//}

static constexpr int kMaxLinkCount = 10;
static constexpr int kMaxItemCount = 30;

void looting::link_all(const char* channel) const
{

	if (!Zeal::EqGame::Windows->Loot || !Zeal::EqGame::Windows->Loot->IsVisible)
	{
		Zeal::EqGame::print_chat("Looting window not visible");
		return;
	}

	const char* delimiter = setting_alt_delimiter.get() ? " | " : ", ";
	if (channel)
	{
		std::string link_text;
		int link_count = 0;
		for (int i = 0; i < kMaxItemCount; i++)
		{
			if (Zeal::EqGame::Windows->Loot->Item[i] && link_count < kMaxLinkCount)
			{
				if (link_count)
					link_text += delimiter;
				link_count++;
				// The AddItemTag performs an sprintf("%c%d%06d%s%c", 0x12, 0, item_id, item_name, 0x12).
				const char kMarker = 0x12;
				WORD item_id = Zeal::EqGame::Windows->Loot->Item[i]->ID;
				const char* item_name = Zeal::EqGame::Windows->Loot->Item[i]->Name;
				link_text += std::format("{0:c}0{1:06d}{2}{3:c}", kMarker, item_id, item_name, kMarker);
			}
		}
		if (link_text.empty())
			return;
		std::string select = channel;
		if (select.starts_with("rs"))
			Zeal::EqGame::send_raid_chat(link_text.c_str());
		else if (select.starts_with("gs"))
			Zeal::EqGame::do_gsay(link_text.c_str());
		else if (select.starts_with("gu"))
			Zeal::EqGame::do_guildsay(link_text.c_str());
		else if (select.starts_with("ooc"))
			Zeal::EqGame::do_ooc(link_text.c_str());
		else if (select.starts_with("auc"))
			Zeal::EqGame::do_auction(link_text.c_str());
		else
			Zeal::EqGame::do_say(false, link_text.c_str());
		return;
	}

	// No specified channel: Paste the links into the active chat window.
	Zeal::EqUI::ChatWnd* wnd = Zeal::EqGame::Windows->ChatManager->GetActiveChatWindow();
	if (!wnd) {
		Zeal::EqGame::print_chat("No active chat window found");
		return;
	}

	Zeal::EqUI::EditWnd* input_wnd = (Zeal::EqUI::EditWnd*)wnd->edit;
	bool has_added_link = false;
	for (int i = 0; i < kMaxItemCount; i++)
	{
		if (Zeal::EqGame::Windows->Loot->Item[i] && input_wnd->item_link_count < kMaxLinkCount)
		{
			if (has_added_link)
				input_wnd->ReplaceSelection(delimiter, false);
			input_wnd->AddItemTag(Zeal::EqGame::Windows->Loot->Item[i]->ID, Zeal::EqGame::Windows->Loot->Item[i]->Name);
			has_added_link = true;
		}
	}
	input_wnd->SetFocus();
}

void looting::set_hide_looted(bool val)
{
	hide_looted = val;
	ZealService::get_instance()->ini->setValue<bool>("Zeal", "HideLooted", hide_looted);
	ZealService::get_instance()->ui->options->UpdateOptions();
	if (hide_looted)
		Zeal::EqGame::print_chat("Corpses will be hidden after looting.");
	else
		Zeal::EqGame::print_chat("Corpses will no longer be hidden after looting.");
}

void __fastcall CLootWndDeactivate(int uk, int unused, int lootwnd_ptr)
{
	ZealService* zeal = ZealService::get_instance();
	Zeal::EqStructures::Entity* corpse = Zeal::EqGame::get_active_corpse();
	if (corpse && corpse->ActorInfo && zeal->looting_hook->hide_looted && corpse->Type == 2)
	{
		corpse->ActorInfo->IsInvisible = 1; //this is the flag set by /hidecorpse all (so /hidecorpse none will reshow these hidden corpses)
	}
	zeal->hooks->hook_map["CLootWndDeactivate"]->original(CLootWndDeactivate)(uk, unused, lootwnd_ptr);
}

void __fastcall release_loot(int uk, int lootwnd_ptr)
{
	ZealService* zeal = ZealService::get_instance();
	Zeal::EqStructures::Entity* corpse = Zeal::EqGame::get_active_corpse();
	if (corpse && corpse->ActorInfo && zeal->looting_hook->hide_looted && corpse->Type==2)
	{
		corpse->ActorInfo->IsInvisible = 1; //this is the flag set by /hidecorpse all (so /hidecorpse none will reshow these hidden corpses)
	}
	zeal->hooks->hook_map["ReleaseLoot"]->original(release_loot)(uk, lootwnd_ptr);
}

looting::~looting()
{
}

void looting::init_ui()
{
}

//struct formatted_msg
//{
//	UINT16 unk;
//	UINT16 string_id;
//	UINT16 type;
//	char message[0];
//};
//
//
//struct pkt_loot_item
//{
//	WORD corpse_id;
//	WORD player_id;
//	WORD slot_id;
//	UINT8 unknown[2];
//	UINT32 auto_loot;
//};
//pkt_loot_item li;
//li.player_id = Zeal::EqGame::get_self()->SpawnId;
//li.corpse_id = Zeal::EqGame::get_active_corpse()->SpawnId;
//li.auto_loot = 1;
//li.slot_id = Zeal::EqGame::Windows->Loot->ItemSlotIndex[i];
//Zeal::EqGame::send_message(0x40a0, (int*)&li, sizeof(li), 1);

void looting::looted_item()
{
	if (Zeal::EqGame::get_char_info()->CursorItem) 
	{
		loot_all = false;
		loot_next_item_time = 0;
		return;
	}
	if (loot_all && Zeal::EqGame::Windows && Zeal::EqGame::Windows->Loot && Zeal::EqGame::Windows->Loot->IsVisible)
	{
		std::string corpse_name = Zeal::EqGame::strip_name(Zeal::EqGame::get_active_corpse()->Name);
		std::string self_name = Zeal::EqGame::get_self()->Name;
		bool is_me = corpse_name == self_name; //my own corpse
		byte nodrop_confirm_bypass[2] = { 0x74, 0x22 };
		int item_count = 0;
		for (int i = 0; i < kMaxItemCount; i++)
			if (Zeal::EqGame::Windows->Loot->Item[i])
				item_count++;

		if (is_me && item_count == 1)
		{
			Zeal::EqGame::print_chat(USERCOLOR_LOOT, "Loot all but 1 item on your own corpse for safety, you may click the item to loot it yourself.");
			loot_all = false;
			return;
		}
		for (int i = 0; i < kMaxItemCount; i++)
		{
			bool loot = false;
			if (Zeal::EqGame::Windows->Loot->Item[i])
			{
				if (is_me && item_count>1)
					loot = true;
				else if (Zeal::EqGame::Windows->Loot->Item[i]->NoDrop != 0)
					loot = true;

				if (loot && !Zeal::EqGame::can_inventory_item(Zeal::EqGame::Windows->Loot->Item[i]))
				{
					Zeal::EqGame::print_chat("Cannot loot %s - not enough inventory space", Zeal::EqGame::Windows->Loot->Item[i]->Name);
					loot_all = false;
					loot = false;
				}

				if (loot)
				{
					Zeal::EqGame::Windows->Loot->RequestLootSlot(i, true);
					return;
				}
			}
		}
		loot_all = false;
	}
	else
		loot_all = false;
}

looting::looting(ZealService* zeal)
{
	hide_looted = zeal->ini->getValue<bool>("Zeal", "HideLooted"); //just remembers the state
	zeal->callbacks->AddGeneric([this]() { init_ui(); }, callback_type::InitUI);
	zeal->callbacks->AddGeneric([this]() {
		if (loot_next_item_time == 0)
			return;
		if (!Zeal::EqGame::Windows || !Zeal::EqGame::Windows->Loot || !Zeal::EqGame::Windows->Loot->IsVisible)
		{
			loot_next_item_time = 0;
			loot_all = false;
			return;
		}
		if (GetTickCount64() >= loot_next_item_time && loot_all)
		{
			looted_item();
			loot_next_item_time = 0;
		}
	}, callback_type::MainLoop);
	zeal->callbacks->AddPacket([this](UINT opcode, char* buffer, UINT len) {
			if (opcode == 0x4031)
			{
				loot_next_item_time = GetTickCount64() + 250;
			}
		return false; 
	});
	zeal->commands_hook->Add("/hidecorpse", { "/hc", "/hideco", "/hidec" }, "Adds looted argument to hidecorpse.",
		[this](std::vector<std::string>& args) {
			if (args.size() > 1 && Zeal::String::compare_insensitive(args[1], "looted"))
			{
				set_hide_looted(!hide_looted);
				return true;
			}
			//if (args.size() > 1 && Zeal::String::compare_insensitive(args[1], "none"))
			//{
			//	set_hide_looted(false);
			//	return false; 
			//}
			return false;
		});
	zeal->commands_hook->Add("/lootall", {}, "Loot all items",
		[this](std::vector<std::string>& args) {
			if (!Zeal::EqGame::Windows->Loot || !Zeal::EqGame::Windows->Loot->IsVisible)
				Zeal::EqGame::print_chat("Looting window not visible");
			else
			{
				ZealService::get_instance()->looting_hook->loot_all = true;
				ZealService::get_instance()->looting_hook->looted_item();
			}
			return true;
		});
	zeal->commands_hook->Add("/linkall", {},
		"Link all items (opt param: say, gs, rs, gu, ooc, auc)",
		[this](std::vector<std::string>& args) {
			const char* channel = (args.size() == 2) ? args[1].c_str() : nullptr;
			ZealService::get_instance()->looting_hook->link_all(channel);
			return true;
		});
	zeal->hooks->Add("ReleaseLoot", 0x426576, release_loot, hook_type_detour);
	zeal->hooks->Add("CLootWndDeactivate", 0x426559, CLootWndDeactivate, hook_type_detour);
	
	if (Zeal::EqGame::is_in_game())
		init_ui();
}
