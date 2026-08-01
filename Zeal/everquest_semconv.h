// GENERATED FILE — do not edit.
// Produced by OpenTelemetry Weaver from the everquest-semconv registry:
//   weaver registry generate -r model --templates templates cpp <out>
// The registry is the source of truth for these names; a typo here becomes a compile error
// instead of a silently split timeseries.
#pragma once

namespace everquest_semconv {

// ---- attributes ----

inline constexpr const char *kEverquestCharacterAaUnspent = "everquest.character.aa.unspent";

inline constexpr const char *kEverquestCharacterClass = "everquest.character.class";

inline constexpr const char *kEverquestCharacterDeity = "everquest.character.deity";

inline constexpr const char *kEverquestCharacterLevel = "everquest.character.level";

inline constexpr const char *kEverquestCharacterName = "everquest.character.name";

inline constexpr const char *kEverquestCharacterStat = "everquest.character.stat";

inline constexpr const char *kEverquestChatColor = "everquest.chat.color";

inline constexpr const char *kEverquestCombatDamageType = "everquest.combat.damage.type";

inline constexpr const char *kEverquestCombatDirection = "everquest.combat.direction";

inline constexpr const char *kEverquestCombatSource = "everquest.combat.source";

inline constexpr const char *kEverquestCombatSourceType = "everquest.combat.source_type";

inline constexpr const char *kEverquestCombatTarget = "everquest.combat.target";

inline constexpr const char *kEverquestDisciplineName = "everquest.discipline.name";

inline constexpr const char *kEverquestFightDamageDealt = "everquest.fight.damage.dealt";

inline constexpr const char *kEverquestFightDamageTaken = "everquest.fight.damage.taken";

inline constexpr const char *kEverquestFightOutcome = "everquest.fight.outcome";

inline constexpr const char *kEverquestSpellName = "everquest.spell.name";

inline constexpr const char *kEverquestZoneId = "everquest.zone.id";

inline constexpr const char *kEverquestZoneName = "everquest.zone.name";


// ---- metrics ----

inline constexpr const char *kEverquestCharacterAttackMetric = "everquest.character.attack";

inline constexpr const char *kEverquestCharacterHasteMetric = "everquest.character.haste";

inline constexpr const char *kEverquestCombatDamageMetric = "everquest.combat.damage";

inline constexpr const char *kEverquestCombatHealMetric = "everquest.combat.heal";


}  // namespace everquest_semconv