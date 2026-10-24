#pragma once

// Built into Zeal. No Book_Keeper process, network transport, or keyboard simulation.
// Portable state tracker kept separate from EQ memory access for isolated testing.

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

class RaidAutoRejoin {
 public:
  struct Member {
    std::string name;
    int group = 0;  // 1-12; zero = ungrouped.
  };
  struct Move {
    std::string name;
    int group = 0;
  };

  void enable(bool on) {
    enabled_ = on;
    // A new enable starts from the current raid roster, not a stale prior raid.
    clear();
  }
  bool enabled() const { return enabled_; }

  void clear() {
    remembered_.clear();
    have_raid_ = false;
    raid_id_ = 0;
    idle_since_ = 0;
  }

  // Observe one complete raid roster. Returns *requests*, never executes commands.
  // A returning character must first disappear from a roster observation.
  std::vector<Move> observe(uint32_t raid_id, const std::vector<Member>& members,
                            bool can_manage, uint64_t now_ms) {
    std::vector<Move> moves;
    if (!enabled_) return moves;

    if (members.empty()) {
      if (idle_since_ == 0) idle_since_ = now_ms ? now_ms : 1;
      // Protect against transient empty rosters on zoning, but discard an ended raid.
      if (now_ms >= idle_since_ && now_ms - idle_since_ >= 30000) clear_raid();
      return moves;
    }
    idle_since_ = 0;

    // The raid ID changes on a genuinely new raid. Never restore old assignments
    // into a different raid even if some names recur.
    if (!have_raid_ || (raid_id != 0 && raid_id_ != 0 && raid_id != raid_id_)) {
      clear_raid();
      raid_id_ = raid_id;
      have_raid_ = true;
      for (const auto& member : members) {
        if (member.group < 1 || member.group > 12 || !valid_name(member.name)) continue;
        remembered_.emplace(key(member.name), Entry{member.name, member.group});
      }
      return moves;  // Never move anyone on initial snapshot.
    }
    if (raid_id_ == 0 && raid_id != 0) raid_id_ = raid_id;

    std::unordered_map<std::string, Member> current;
    int group_counts[13] = {};
    for (const auto& member : members) {
      if (!valid_name(member.name) || member.group < 0 || member.group > 12) continue;
      const std::string name_key = key(member.name);
      if (current.emplace(name_key, member).second && member.group > 0) ++group_counts[member.group];
    }

    for (auto& item : remembered_) {
      Entry& entry = item.second;
      const auto found = current.find(item.first);
      if (found == current.end()) {
        entry.present = false;
        entry.candidate_group = -1;
        entry.candidate_count = 0;
        continue;
      }

      const int group = found->second.group;
      entry.name = found->second.name;
      if (!entry.present) {
        // Returning member: preserve last group even if initially ungrouped or
        // temporarily placed elsewhere by the client.
        entry.present = true;
        entry.awaiting_restore = group != entry.saved_group;
        entry.tries = 0;
        entry.last_attempt = 0;
        entry.candidate_group = -1;
        entry.candidate_count = 0;
      }

      if (group == entry.saved_group) {
        entry.awaiting_restore = false;
        entry.tries = 0;
        entry.candidate_count = 0;
      } else if (!entry.awaiting_restore) {
        // Only learn *stable*, real grouped moves. An intermediate ungrouped
        // state should never erase someone's saved group during a transfer.
        if (group > 0 && group <= 12) {
          if (entry.candidate_group == group) ++entry.candidate_count;
          else { entry.candidate_group = group; entry.candidate_count = 1; }
          if (entry.candidate_count >= 2) {
            entry.saved_group = group;
            entry.candidate_group = -1;
            entry.candidate_count = 0;
          }
        } else {
          entry.candidate_group = -1;
          entry.candidate_count = 0;
        }
      }

      if (entry.awaiting_restore && can_manage && entry.saved_group >= 1 && entry.saved_group <= 12 &&
          group_counts[entry.saved_group] < 6 && entry.tries < 2 &&
          (entry.tries == 0 || (now_ms >= entry.last_attempt && now_ms - entry.last_attempt >= 15000))) {
        moves.push_back({entry.name, entry.saved_group});
        ++entry.tries;
        entry.last_attempt = now_ms;
        // Limit to one move per observation to avoid command bursts.
        break;
      }
    }

    // Add newly joined players, but never move them on their first appearance.
    for (const auto& item : current) {
      if (remembered_.count(item.first) == 0 && item.second.group > 0) {
        remembered_.emplace(item.first, Entry{item.second.name, item.second.group});
      }
    }
    return moves;
  }

  size_t remembered_count() const { return remembered_.size(); }

 private:
  struct Entry {
    std::string name;
    int saved_group = 0;
    bool present = true;
    bool awaiting_restore = false;
    int candidate_group = -1;
    int candidate_count = 0;
    int tries = 0;
    uint64_t last_attempt = 0;
  };
  static bool valid_name(const std::string& name) {
    if (name.empty() || name.size() > 63) return false;
    // EQ player names contain letters; constrain text before it becomes a command.
    return std::all_of(name.begin(), name.end(), [](unsigned char c) {
      return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
    });
  }
  static std::string key(std::string name) {
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
      return static_cast<char>(std::tolower(c));
    });
    return name;
  }
  void clear_raid() {
    remembered_.clear();
    have_raid_ = false;
    raid_id_ = 0;
  }
  bool enabled_ = false;
  bool have_raid_ = false;
  uint32_t raid_id_ = 0;
  uint64_t idle_since_ = 0;
  std::unordered_map<std::string, Entry> remembered_;
};
