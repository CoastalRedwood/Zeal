#include "game_str.h"

#include "game_addresses.h"
#include "game_functions.h"
#include "game_structures.h"
#include "zeal.h"

GameStr::~GameStr() {}

const char *__fastcall GetString(int stringtable, int unused, int string_id, bool *valid) {
  GameStr *t = ZealService::get_instance()->gamestr_hook.get();
  if (t->str_replacements.count(string_id) > 0) {
    if (valid) *valid = true;
    return t->str_replacements[string_id];
  }
  if (t->str_noprint.count(string_id) && t->str_noprint[string_id]) return "";
  const char *d = ZealService::get_instance()->hooks->hook_map["GetString"]->original(GetString)(stringtable, unused,
                                                                                                 string_id, valid);
  return d;
}

GameStr::GameStr(ZealService *zeal) {
  str_replacements = {
      {6551, "Toggle target and myself"},      {4061, "You are already dragging as much as you can!"},
      {4062, "You are already dragging %1."},  {4063, "Someone else is dragging %1."},
      {4064, "You begin to drag %1."},         {4065, "You stop dragging the corpses."},
      {4066, "You stop dragging the corpse."},
      //{13085, "Well hello there, %1"}, //replaces Hail, player was for testing purposes
  };
  zeal->hooks->Add("GetString", 0x550EFE, GetString, hook_type_detour);  // add extra prints for new loot types
}
