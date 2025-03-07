#include "character_select.h"
#include "Zeal.h"
#include <random>



DWORD GetRandomZone()
{
	// Seed with a random device for better randomness
	static std::random_device rd;
	static std::mt19937 generator(rd());  // Mersenne Twister engine
	static std::uniform_int_distribution<DWORD> distribution(1, 77);

	return distribution(generator);
}

void __fastcall StartWorldDisplay(DWORD t, DWORD unused, DWORD zone_index, DWORD uhh)
{
	if (zone_index == 0xB9 && ZealService::get_instance()->charselect->ZoneIndex.get()!=-1) //loading (character select)
		zone_index = ZealService::get_instance()->charselect->ZoneIndex.get();
	
	if (Zeal::EqGame::get_zone_name_from_index(zone_index).length() == 0) //validate the zone id by checking the name
		zone_index = 0xB9;

	ZealService::get_instance()->hooks->hook_map["StartWorldDisplay"]->original(StartWorldDisplay)(t, unused, zone_index, uhh);
}

void __fastcall SelectCharacter(DWORD t, DWORD unused, DWORD unk1, DWORD unk2)
{
	ZealService::get_instance()->hooks->hook_map["SelectCharacter"]->original(SelectCharacter)(t, unused, unk1, unk2);
	Vec3* SafeCoords = (Vec3*)(0x79896C);
	*SafeCoords = ZealService::get_instance()->charselect->ZoneSafeCoords[ZealService::get_instance()->charselect->ZoneIndex.get()];
	reinterpret_cast<void(__stdcall*)(void)>(0x4B459C)();

}

CharacterSelect::CharacterSelect(ZealService* zeal)
{
	zeal->hooks->Add("StartWorldDisplay", 0x4A849E, StartWorldDisplay, hook_type_detour);
	zeal->hooks->Add("SelectCharacter", 0x40F56D, SelectCharacter, hook_type_detour);
	mem::set(0x55B4A1, 0x90, 2); //ignore connection state for mouse wheel
}
CharacterSelect::~CharacterSelect()
{

}