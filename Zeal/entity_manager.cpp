#include "entity_manager.h"

#include "game_addresses.h"
#include "zeal.h"

void EntityManager::Add(struct Zeal::GameStructures::Entity *ent) {
  // Perform simple validity checks before adding an entity.
  // Notes:
  // - The client StartNetworkGame method contructs a placeholder for _LocalPlayer
  // with GamePlayer::GamePlayer(nullptr,0,1,1,"load") and a zero id that is later deconstructed
  // - Entering a zone constructs a new Self GamePlayer that starts with a zero spawn_id.
  // - The server likes to reuse npc names (a_bat039) shortly after they die even while the
  // previous corpse is still there. This will overwrite the map with the new entity, so
  // the entity manager map's count will be reduced relative to the IDArray count.
  if (!ent) return;
  if (ent->SpawnId == 0 || ent == Zeal::Game::get_entity_by_id(ent->SpawnId)) entity_map[ent->Name] = ent;
}

void EntityManager::Remove(struct Zeal::GameStructures::Entity *ent) {
  // The game renames the ent->Name field (like _corpse0 suffix), so
  // ent->Name is not a reliable key to locate the hash entity to remove.
  // Removes are infrequent operations, so we can afford a linear search.
  for (auto it = entity_map.begin(); it != entity_map.end(); ++it) {
    if (it->second == ent) {
      entity_map.erase(it);  // Ignoring updated iterator.
      return;
    }
  }
}

Zeal::GameStructures::Entity *EntityManager::Get(std::string name) const {
  if (!Zeal::Game::is_in_game()) return nullptr;
  auto it = entity_map.find(name);
  if (it != entity_map.end()) {
    // Only return a hit if the manager is in sync with the client IDArray.
    auto *entity = it->second;
    if (entity && entity == Zeal::Game::get_entity_by_id(entity->SpawnId)) return entity;
  }
  return nullptr;
}

Zeal::GameStructures::Entity *EntityManager::Get(WORD id) const {
  auto *entity = Zeal::Game::get_entity_by_id(id);
  return entity;
}

std::vector<std::string> EntityManager::GetPlayerPartialMatches(const std::string &start_of_name) const {
  std::vector<std::string> result;
  Zeal::GameStructures::Entity *current_ent = Zeal::Game::get_entity_list();
  while (current_ent != nullptr) {
    if (current_ent->Type == Zeal::GameEnums::Player &&
        _strnicmp(current_ent->Name, start_of_name.c_str(), start_of_name.length()) == 0)
      result.push_back(current_ent->Name);
    current_ent = current_ent->Next;
  }
  return result;
}

// Local Dump() helper that confirms there isn't a mismatch of a hashmap key/value pair.
static bool is_valid_entity(const std::string &name, struct Zeal::GameStructures::Entity *entity) {
  if (!entity) return false;
  auto *client_entity = Zeal::Game::get_entity_by_id(entity->SpawnId);  // Returns nullptr if fails.
  if (client_entity != entity) {
    Zeal::Game::print_chat("MISMATCH: client[%i] entity does not match for name: %s", entity->SpawnId, name.c_str());
    return false;
  } else if (name != client_entity->Name) {
    if (strcmp(Zeal::Game::strip_name(name.c_str()), Zeal::Game::strip_name(client_entity->Name)) == 0)
      Zeal::Game::print_chat("NAME_CHANGED[%i]: manager: %s, client: %s", entity->SpawnId, name.c_str(),
                             client_entity->Name);
    else
      Zeal::Game::print_chat("BADNAME[%i]: manager: %s, client: %s", entity->SpawnId, name.c_str(),
                             client_entity->Name);
    return false;
  }
  return true;
}

// Local dump helper that reports any missing entries in the table.
static int report_missing_entities() {
  Zeal::GameStructures::Entity *current = Zeal::Game::get_entity_list();
  auto entity_manager = ZealService::get_instance()->entity_manager.get();  // Short-term ptr.

  int missing_corpse_count = 0;
  int missing_count = 0;
  while (current != nullptr) {
    if (!entity_manager->Get(current->Name)) {
      if (current->Type == Zeal::GameEnums::NPCCorpse)
        ++missing_corpse_count;
      else {
        ++missing_count;
        Zeal::Game::print_chat("MISSING: entity[%i]: %s", current->SpawnId, current->Name);
      }
      if (current != Zeal::Game::get_entity_by_id(current->SpawnId))
        Zeal::Game::print_chat("IDARRAY MISMATCH: entity[%i]: %s", current->SpawnId, current->Name);
    }
    current = current->Next;
  }
  if (missing_corpse_count) Zeal::Game::print_chat("Missing corpse count: %i", missing_corpse_count);

  return missing_count;
}

// Local Dump() helper that just counts up the number of game entities.
static int get_num_game_entities() {
  int count = 0;
  Zeal::GameStructures::Entity *current = Zeal::Game::get_entity_list();
  while (current != nullptr) {
    count++;
    current = current->Next;
  }
  return count;
}

void EntityManager::Dump() const {
  Zeal::Game::print_chat("Num entities: %i, Num in manager: %i", get_num_game_entities(), entity_map.size());

  for (const auto &[key, value] : entity_map) {
    // First do an integrity check to make sure the entity address is valid.
    if (is_valid_entity(key, value) && (value->Type == Zeal::GameEnums::EntityTypes::Player))
      Zeal::Game::print_chat("[%i]: name: %s, entity: 0x%08x", value->SpawnId, key.c_str(),
                             reinterpret_cast<uint32_t>(value));
  }

  report_missing_entities();
}

EntityManager::EntityManager(class ZealService *zeal) {
  zeal->callbacks->AddEntity([this](struct Zeal::GameStructures::Entity *ent) { Remove(ent); },
                             callback_type::EntityDespawn);
  zeal->callbacks->AddEntity([this](struct Zeal::GameStructures::Entity *ent) { Add(ent); },
                             callback_type::EntitySpawn);
}

EntityManager::~EntityManager() {}
