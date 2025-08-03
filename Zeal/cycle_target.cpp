#include "cycle_target.h"

#include <algorithm>

#include "game_addresses.h"
#include "game_functions.h"
#include "game_structures.h"
#include "string_util.h"
#include "zeal.h"

static size_t last_index = -1;
static std::vector<Zeal::GameStructures::Entity *> near_ents;

bool compareDist(Zeal::GameStructures::Entity *i1, Zeal::GameStructures::Entity *i2) {
  return (i1->Position.Dist(Zeal::Game::get_self()->Position) < i2->Position.Dist(Zeal::Game::get_self()->Position));
}

void AddIndex() {
  last_index++;
  if (last_index >= near_ents.size()) last_index = 0;
}

Zeal::GameStructures::Entity *CycleTarget::get_next_ent(float dist, byte type) {
  static ULONGLONG last_press = 0;

  std::vector<Zeal::GameStructures::Entity *> visible_ents = Zeal::Game::get_world_visible_actor_list(dist, true);
  if (GetTickCount64() - last_press >
      3000)  // if you haven't pressed the cycle key in 3 seconds reset the index so it selected the nearest
    last_index = -1;
  last_press = GetTickCount64();

  if (!visible_ents.size()) return 0;
  near_ents.clear();
  for (auto &ent : visible_ents) {
    if (ent->StructType != 0x03) continue;
    if (ent->Type == type && ent->Level > 0) {
      if (Zeal::Game::is_a_mount(ent)) continue;  // Skip mounts like horses.
      if (ent->PetOwnerSpawnId) {
        Zeal::GameStructures::Entity *owner = Zeal::Game::get_entity_by_id(ent->PetOwnerSpawnId);
        if ((owner && (owner->Type == Zeal::GameEnums::EntityTypes::NPC ||
                       owner->Type == Zeal::GameEnums::EntityTypes::NPCCorpse)) ||
            !owner)
          near_ents.push_back(ent);
      } else
        near_ents.push_back(ent);
    }
  }
  if (!near_ents.size()) return 0;

  std::sort(near_ents.begin(), near_ents.end(), compareDist);
  AddIndex();

  if (near_ents[last_index] && Zeal::Game::get_target() && Zeal::Game::get_target() == near_ents[last_index])
    AddIndex();

  if (near_ents[last_index])
    return near_ents[last_index];
  else
    return 0;
}

Zeal::GameStructures::Entity *CycleTarget::get_nearest_ent(float dist, byte type) {
  static ULONGLONG last_press = 0;
  std::vector<Zeal::GameStructures::Entity *> visible_ents;
  if (type > 0)
    visible_ents = Zeal::Game::get_world_visible_actor_list(dist, true);
  else
    visible_ents = Zeal::Game::get_world_visible_actor_list(dist, false);
  if (!visible_ents.size()) return 0;
  near_ents.clear();
  for (auto &ent : visible_ents) {
    if (ent->StructType != 0x03) continue;
    if (ent->Type == type && ent->Level > 0) {
      if (Zeal::Game::is_a_mount(ent)) continue;  // Skip mounts like horses.
      if (ent->PetOwnerSpawnId) {
        Zeal::GameStructures::Entity *owner = Zeal::Game::get_entity_by_id(ent->PetOwnerSpawnId);
        if ((owner && (owner->Type == Zeal::GameEnums::EntityTypes::NPC ||
                       owner->Type == Zeal::GameEnums::EntityTypes::NPCCorpse)) ||
            !owner)
          near_ents.push_back(ent);
      } else
        near_ents.push_back(ent);
    }
  }
  if (!near_ents.size()) return 0;

  std::sort(near_ents.begin(), near_ents.end(), compareDist);
  AddIndex();

  if (near_ents.front())
    return near_ents.front();
  else
    return 0;
}

CycleTarget::~CycleTarget() {
  // hook->remove(); //hooks being removed from dllmain
}

CycleTarget::CycleTarget(ZealService *zeal) {
  // originally this used a hook and replaced target nearest but now uses a bind
  //	hook = zeal->hooks->Add("NearestEnt", Zeal::Game::GameInternal::fn_targetnearestnpc, get_nearest_ent,
  // hook_type_detour, 6);
}
