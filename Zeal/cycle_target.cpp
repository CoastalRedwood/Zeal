#include "cycle_target.h"

#include <algorithm>

#include "callbacks.h"
#include "game_functions.h"
#include "zeal.h"

static bool compareDist(Zeal::GameStructures::Entity *i1, Zeal::GameStructures::Entity *i2) {
  return (i1->Position.Dist(Zeal::Game::get_self()->Position) < i2->Position.Dist(Zeal::Game::get_self()->Position));
}

void CycleTarget::handle_next_target(int key_down, Zeal::GameEnums::EntityTypes type) {
  if (key_down && !Zeal::Game::GameInternal::UI_ChatInputCheck()) {
    Zeal::GameStructures::Entity *ent = get_next_ent(250, type);
    if (ent) Zeal::Game::set_target(ent);
  }
}

void CycleTarget::handle_toggle_last_two(int key_down) {
  if (!key_down || Zeal::Game::GameInternal::UI_ChatInputCheck()) return;

  Zeal::GameStructures::Entity *target = Zeal::Game::get_target();
  if (target) {
    if (last_targets.first == target->SpawnId && last_targets.second != 0) {
      Zeal::GameStructures::Entity *ent = Zeal::Game::get_entity_by_id(last_targets.second);
      if (ent) Zeal::Game::set_target(ent);
    }
  } else {
    if (last_targets.first != 0) {
      Zeal::GameStructures::Entity *ent = Zeal::Game::get_entity_by_id(last_targets.first);
      if (ent) Zeal::Game::set_target(ent);
    }
  }
}

void CycleTarget::add_index() {
  last_index++;
  if (last_index >= near_ents.size()) last_index = 0;
}

Zeal::GameStructures::Entity *CycleTarget::get_next_ent(float dist, BYTE type) {
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
  add_index();

  if (near_ents[last_index] && Zeal::Game::get_target() && Zeal::Game::get_target() == near_ents[last_index])
    add_index();

  if (near_ents[last_index])
    return near_ents[last_index];
  else
    return 0;
}

Zeal::GameStructures::Entity *CycleTarget::get_nearest_ent(float dist, BYTE type) {
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
  add_index();

  if (near_ents.front())
    return near_ents.front();
  else
    return 0;
}

void CycleTarget::main_loop() {
  if (Zeal::Game::get_target() && Zeal::Game::get_target()->SpawnId != last_targets.first) {
    last_targets.second = last_targets.first;
    last_targets.first = Zeal::Game::get_target()->SpawnId;
  }
}

void CycleTarget::on_zone() {
  last_targets.first = 0;
  last_targets.second = 0;
  last_index = -1;
  near_ents.clear();
}

CycleTarget::CycleTarget(ZealService *zeal) {
  zeal->callbacks->AddGeneric([this]() { main_loop(); }, callback_type::MainLoop);
  zeal->callbacks->AddGeneric([this]() { on_zone(); }, callback_type::EnterZone);
}
