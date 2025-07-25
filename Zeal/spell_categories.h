#pragma once
#include <windows.h>

#include <algorithm>
#include <array>
#include <string>

// original credits for this go to mmofry
struct SpellCat {
  int Category;
  int SubCategory;
  const char *NewName;
  constexpr SpellCat(int cat, int subcat, const char *name = nullptr)
      : Category(cat), SubCategory(subcat), NewName(name){};
};

struct SpellCatEntry {
  int spell_id;
  SpellCat spell_cat;
  constexpr SpellCatEntry(int _spell_id, const SpellCat &_spell_cat) : spell_id(_spell_id), spell_cat(_spell_cat){};
};

static constexpr std::array<SpellCatEntry, 2127> spell_cat_lut = {{
    {3, {125, 64}},                               // Summon Corpse,
    {4, {18, 109}},                               // Summon Waterstone,
    {6, {20, 38}},                                // Ignite Blood,
    {7, {79, 43}},                                // Hymn of Restoration,
    {9, {42, 42}},                                // Superior Healing,
    {10, {125, 41}},                              // Augmentation,
    {11, {95, 6}},                                // Holy Armor,
    {12, {42, 42}},                               // Healing,
    {13, {42, 42}},                               // Complete Healing,
    {14, {25, 58}},                               // Strike,
    {15, {42, 42}},                               // Greater Healing,
    {16, {25, 58}},                               // Smite,
    {17, {42, 42}},                               // Light Healing,
    {18, {95, 6}},                                // Guard,
    {19, {95, 6}},                                // Armor of Faith,
    {20, {95, 6}},                                // Shield of Words,
    {21, {95, 96}},                               // Berserker Strength,
    {22, {25, 58}},                               // Force Snap,
    {23, {25, 58}},                               // Force Strike,
    {24, {126, 31}},                              // Strip Enchantment,
    {25, {126, 31}},                              // Pillage Enchantment,
    {26, {45, 46}},                               // Skin like Wood,
    {27, {25, 14}},                               // Pogonip,
    {28, {25, 14}},                               // Avalanche,
    {29, {25, 14}},                               // Ice,
    {31, {20, 29}},                               // Scourge,
    {32, {20, 29}},                               // Plague,
    {33, {95, 130}},                              // Brilliance,
    {34, {125, 51}},                              // Superior Camouflage,
    {35, {125, 64}},                              // Bind Affinity,
    {36, {123, 64}},                              // Gate,
    {37, {18, 110}},                              // Hammer of Striking,
    {38, {25, 58}},                               // Lightning Bolt,
    {39, {125, 41}},                              // Quickness,
    {40, {95, 96}},                               // Strengthen,
    {41, {126, 30}},                              // Weaken,
    {42, {125, 51}},                              // Invisibility,
    {43, {95, 7}},                                // Yaulp II,
    {44, {95, 7}},                                // Yaulp III,
    {45, {126, 11}},                              // Pacify,
    {46, {125, 129}},                             // Ultravision,
    {47, {126, 11}},                              // Calm,
    {48, {126, 31}},                              // Cancel Magic,
    {49, {126, 31}},                              // Nullify Magic,
    {50, {18, 108}},                              // Summon Food,
    {51, {125, 129}},                             // Glimpse,
    {52, {18, 108}},                              // Abundant Drink,
    {53, {18, 108}},                              // Abundant Food,
    {54, {25, 14}},                               // Frost Bolt,
    {55, {18, 108}},                              // Cornucopia,
    {56, {18, 108}},                              // Everfount,
    {57, {25, 38}},                               // Firestrike,
    {58, {69, 100}},                              // Elementalkin: Earth,
    {59, {126, 37}},                              // Panic the Dead,
    {60, {95, 80}},                               // Resist Fire,
    {61, {95, 80}},                               // Resist Cold,
    {62, {95, 80}},                               // Resist Poison,
    {63, {95, 80}},                               // Resist Disease,
    {64, {95, 80}},                               // Resist Magic,
    {65, {45, 87}},                               // Major Shielding,
    {66, {45, 87}},                               // Greater Shielding,
    {67, {45, 87}},                               // Arch Shielding,
    {68, {25, 38}},                               // Bolt of Flame,
    {69, {25, 38}},                               // Cinder Bolt,
    {70, {25, 38}},                               // Lava Bolt,
    {71, {25, 58}},                               // Anarchy,
    {72, {95, 80}},                               // Group Resist Magic,
    {73, {25, 58}},                               // Gravity Flux,
    {74, {126, 60}},                              // Mana Sieve,
    {75, {20, 29}},                               // Sicken,
    {76, {126, 83}},                              // Ensnaring Roots,
    {77, {126, 83}},                              // Engulfing Roots,
    {78, {20, 38}},                               // Immolate,
    {79, {125, 129}},                             // Spirit Sight,
    {80, {125, 129}},                             // See Invisible,
    {81, {79, 43}},                               // Phantom Chain,
    {82, {79, 43}},                               // Phantom Plate,
    {83, {25, 38}},                               // Rain of Fire,
    {84, {125, 129}},                             // Shifting Sight,
    {85, {25, 38}},                               // Firestorm,
    {86, {125, 64}},                              // Enduring Breath,
    {88, {25, 58}},                               // Harm Touch,
    {89, {45, 46}},                               // Daring,
    {90, {125, 129}},                             // Shadow Sight,
    {91, {25, 38}},                               // Ignite,
    {92, {25, 38}},                               // Burst of Fire,
    {93, {25, 38}},                               // Burst of Flame,
    {94, {25, 38}},                               // Burn,
    {95, {42, 19}},                               // Counteract Poison,
    {96, {42, 19}},                               // Counteract Disease,
    {97, {42, 19}},                               // Abolish Poison,
    {98, {42, 19}},                               // Abolish Disease,
    {99, {20, 58}},                               // Creeping Crud,
    {100, {18, 110}},                             // Summon Throwing Dagger,
    {101, {18, 110}},                             // Summon Arrows,
    {102, {18, 110}},                             // Spear of Warding,
    {103, {18, 109}},                             // Summon Coldstone,
    {104, {18, 110}},                             // Dagger of Symbols,
    {105, {18, 109}},                             // Summon Ring of Flight,
    {106, {69, 70}},                              // Burnout II,
    {107, {69, 70}},                              // Burnout III,
    {108, {95, 80}},                              // Elemental Shield,
    {109, {95, 80}},                              // Elemental Armor,
    {110, {126, 81}},                             // Malaise,
    {111, {126, 81}},                             // Malaisement,
    {112, {126, 81}},                             // Malosi,
    {113, {25, 58}},                              // Shock of Spikes,
    {114, {25, 58}},                              // Shock of Swords,
    {115, {25, 111}},                             // Dismiss Summoned,
    {116, {25, 111}},                             // Banish Summoned,
    {117, {25, 124}},                             // Dismiss Undead,
    {118, {25, 124}},                             // Banish Undead,
    {120, {25, 38}},                              // Blaze,
    {121, {25, 38}},                              // Rain of Lava,
    {122, {25, 38}},                              // Flame Arc,
    {123, {25, 97}},                              // Holy Might,
    {124, {25, 97}},                              // Force,
    {125, {25, 97}},                              // Sound of Force,
    {126, {126, 37}},                             // Inspire Fear,
    {127, {126, 37}},                             // Invoke Fear,
    {128, {126, 37}},                             // Wave of Fear,
    {129, {125, 21}},                             // Shield of Brambles,
    {130, {125, 52}},                             // Divine Barrier,
    {131, {126, 83}},                             // Instill,
    {132, {126, 83}},                             // Immobilize,
    {133, {126, 83}},                             // Paralyzing Earth,
    {134, {126, 9}},                              // Blinding Luminance,
    {135, {42, 42}},                              // Word of Health,
    {136, {42, 42}},                              // Word of Healing,
    {137, {79, 43}},                              // Pack Regeneration,
    {138, {79, 43}},                              // Pack Chloroplast,
    {139, {69, 70}},                              // Feral Spirit,
    {140, {69, 70}},                              // Savage Spirit,
    {141, {126, 13}},                             // Beguile Animals,
    {142, {126, 13}},                             // Allure of the Wild,
    {143, {126, 9}},                              // Sunbeam,
    {144, {79, 43}},                              // Regeneration,
    {145, {79, 43}},                              // Chloroplast,
    {146, {95, 24}},                              // Spirit of Monkey,
    {147, {95, 96}},                              // Spirit Strength,
    {148, {95, 2}},                               // Spirit of Cat,
    {149, {95, 94}},                              // Spirit of Ox,
    {150, {95, 12}},                              // Alluring Aura,
    {151, {95, 96}},                              // Raging Strength,
    {152, {95, 24}},                              // Deftness,
    {153, {95, 96}},                              // Furious Strength,
    {154, {95, 2}},                               // Agility,
    {155, {95, 12}},                              // Glamour,
    {156, {95, 12}},                              // Charisma,
    {157, {95, 24}},                              // Dexterity,
    {158, {95, 94}},                              // Stamina,
    {159, {95, 96}},                              // Strength,
    {160, {95, 2}},                               // Nimble,
    {161, {95, 94}},                              // Health,
    {162, {126, 30}},                             // Listless Power,
    {163, {126, 30}},                             // Incapacitate,
    {164, {69, 104}},                             // Companion Spirit,
    {165, {69, 104}},                             // Guardian Spirit,
    {166, {69, 104}},                             // Frenzied Spirit,
    {167, {45, 87}},                              // Talisman of Tnarg,
    {168, {45, 87}},                              // Talisman of Altuna,
    {169, {125, 65}},                             // Pack Spirit,
    {170, {125, 41}},                             // Alacrity,
    {171, {125, 41}},                             // Celerity,
    {172, {125, 41}},                             // Swift like the Wind,
    {173, {125, 3}},                              // Benevolence,
    {174, {79, 59}},                              // Clarity,
    {175, {95, 130}},                             // Insight,
    {176, {125, 84}},                             // Berserker Spirit,
    {177, {25, 97}},                              // Color Shift,
    {178, {25, 97}},                              // Color Skew,
    {179, {126, 30}},                             // Feckless Might,
    {180, {126, 30}},                             // Insipid Weakness,
    {181, {126, 30}},                             // Weakness,
    {182, {126, 13}},                             // Beguile,
    {183, {126, 13}},                             // Cajoling Whispers,
    {184, {126, 13}},                             // Allure,
    {185, {126, 88}},                             // Tepid Deeds,
    {186, {126, 88}},                             // Shiftless Deeds,
    {187, {126, 35}},                             // Enthrall,
    {188, {126, 35}},                             // Entrance,
    {189, {25, 38}},                              // Flame Flux,
    {190, {126, 35}},                             // Dazzle,
    {191, {125, 21}},                             // Feedback,
    {192, {126, 63}},                             // Mind Wipe,
    {193, {126, 63}},                             // Blanket of Forgetfulness,
    {194, {126, 63}},                             // Reoccurring Amnesia,
    {195, {20, 58}},                              // Gasping Embrace,
    {196, {126, 13}},                             // Dominate Undead,
    {197, {126, 13}},                             // Beguile Undead,
    {198, {126, 13}},                             // Cajole Undead,
    {199, {125, 52}},                             // Harmshield,
    {200, {42, 42}},                              // Minor Healing,
    {201, {126, 9}},                              // Flash of Light,
    {202, {45, 46}},                              // Courage,
    {203, {42, 19}},                              // Cure Poison,
    {204, {25, 75}},                              // Shock of Poison,
    {205, {125, 64}},                             // True North,
    {207, {125, 52}},                             // Divine Aura,
    {208, {126, 11}},                             // Lull,
    {209, {126, 37}},                             // Spook the Dead,
    {210, {95, 7}},                               // Yaulp,
    {211, {18, 108}},                             // Summon Drink,
    {212, {42, 19}},                              // Cure Blindness,
    {213, {42, 19}},                              // Cure Disease,
    {215, {95, 96}},                              // Reckless Strength,
    {216, {25, 97}},                              // Stun,
    {217, {25, 38}},                              // Combust,
    {218, {25, 124}},                             // Ward Undead,
    {219, {45, 46}},                              // Center,
    {220, {125, 65}},                             // Spirit of Cheetah,
    {221, {125, 64}},                             // Sense the Dead,
    {222, {42, 94}},                              // Invigor,
    {223, {18, 110}},                             // Hammer of Wrath,
    {224, {95, 80}},                              // Endure Fire,
    {225, {95, 80}},                              // Endure Cold,
    {226, {95, 80}},                              // Endure Disease,
    {227, {95, 80}},                              // Endure Poison,
    {228, {95, 80}},                              // Endure Magic,
    {229, {126, 37}},                             // Fear,
    {230, {126, 83}},                             // Root,
    {231, {25, 58}},                              // Word of Pain,
    {232, {125, 111}},                            // Sense Summoned,
    {233, {25, 124}},                             // Expulse Undead,
    {234, {18, 109}},                             // Halo of Light,
    {235, {125, 51}},                             // Invisibility versus Undead,
    {236, {125, 84}},                             // Shieldskin,
    {237, {18, 109}},                             // Dance of the Fireflies,
    {238, {125, 4}},                              // Sense Animals,
    {239, {20, 38}},                              // Flame Lick,
    {240, {126, 11}},                             // Lull Animal,
    {241, {126, 37}},                             // Panic Animal,
    {242, {126, 89}},                             // Snare,
    {243, {125, 49}},                             // Illusion: Iksar,
    {244, {45, 46}},                              // Bravery,
    {245, {126, 13}},                             // Befriend Animal,
    {246, {45, 87}},                              // Lesser Shielding,
    {247, {125, 51}},                             // Camouflage,
    {248, {25, 111}},                             // Ward Summoned,
    {249, {126, 83}},                             // Grasping Roots,
    {250, {126, 11}},                             // Harmony,
    {252, {25, 58}},                              // Invoke Lightning,
    {253, {25, 58}},                              // Whirling Wind,
    {254, {95, 7}},                               // Firefist,
    {255, {125, 51}},                             // Invisibility versus Animals,
    {256, {125, 21}},                             // Shield of Thistles,
    {257, {18, 109}},                             // Starshine,
    {258, {125, 48}},                             // Treeform,
    {259, {20, 58}},                              // Drones of Doom,
    {260, {126, 13}},                             // Charm Animals,
    {261, {125, 55}},                             // Levitate,
    {262, {25, 14}},                              // Cascade of Hail,
    {263, {45, 46}},                              // Skin like Rock,
    {264, {20, 58}},                              // Stinging Swarm,
    {265, {125, 17}},                             // Cannibalize,
    {266, {95, 24}},                              // Dexterous Aura,
    {267, {45, 46}},                              // Inner Fire,
    {268, {95, 96}},                              // Strength of Earth,
    {269, {95, 2}},                               // Feet like Cat,
    {270, {126, 88}},                             // Drowsy,
    {271, {95, 96}},                              // Fleeting Fury,
    {272, {18, 109}},                             // Spirit Pouch,
    {273, {125, 21}},                             // Shield of Barbs,
    {274, {95, 6}},                               // Scale Skin,
    {275, {25, 14}},                              // Frost Rift,
    {276, {125, 129}},                            // Serpent Sight,
    {277, {20, 75}},                              // Tainted Breath,
    {278, {125, 65}},                             // Spirit of Wolf,
    {279, {95, 94}},                              // Spirit of Bear,
    {280, {95, 96}},                              // Burst of Strength,
    {281, {126, 30}},                             // Disempower,
    {282, {25, 14}},                              // Spirit Strike,
    {283, {95, 6}},                               // Turtle Skin,
    {284, {95, 12}},                              // Spirit of Snake,
    {285, {69, 99}},                              // Pendril's Animation,
    {286, {20, 58}},                              // Shallow Breath,
    {287, {125, 48}},                             // Minor Illusion,
    {288, {45, 87}},                              // Minor Shielding,
    {289, {126, 31}},                             // Taper Enchantment,
    {290, {25, 97}},                              // Color Flux,
    {291, {126, 30}},                             // Enfeeblement,
    {292, {126, 35}},                             // Mesmerize,
    {293, {95, 6}},                               // Haze,
    {294, {20, 58}},                              // Suffocating Sphere,
    {295, {69, 99}},                              // Mircyl's Animation,
    {296, {25, 58}},                              // Chaotic Feedback,
    {297, {126, 9}},                              // Eye of Confusion,
    {298, {125, 3}},                              // Alliance,
    {299, {125, 64}},                             // Sentinel,
    {300, {126, 13}},                             // Charm,
    {301, {126, 63}},                             // Memory Blur,
    {302, {126, 88}},                             // Languid Pace,
    {303, {25, 97}},                              // Whirl till you hurl,
    {304, {126, 37}},                             // Chase the Moon,
    {305, {125, 64}},                             // Identify,
    {306, {25, 58}},                              // Sanity Warp,
    {307, {126, 35}},                             // Mesmerization,
    {308, {95, 96}},                              // Frenzy,
    {309, {45, 87}},                              // Shielding,
    {310, {125, 64}},                             // Flare,
    {311, {18, 110}},                             // Summon Dagger,
    {312, {45, 46}},                              // Valor,
    {313, {25, 38}},                              // Fire Flux,
    {314, {45, 46}},                              // Resolution,
    {315, {69, 105}},                             // Elementalkin: Water,
    {316, {69, 102}},                             // Elementalkin: Fire,
    {317, {69, 98}},                              // Elementalkin: Air,
    {318, {18, 109}},                             // Summon Bandages,
    {319, {18, 110}},                             // Summon Fang,
    {320, {18, 109}},                             // Summon Heatstone,
    {321, {18, 109}},                             // Summon Wisp,
    {322, {25, 38}},                              // Flame Bolt,
    {323, {125, 129}},                            // Eye of Zomm,
    {324, {25, 58}},                              // Shock of Blades,
    {325, {18, 109}},                             // Dimensional Pocket,
    {326, {95, 96}},                              // Fury,
    {327, {69, 70}},                              // Burnout,
    {328, {25, 38}},                              // Column of Fire,
    {329, {25, 58}},                              // Wrath,
    {330, {25, 58}},                              // Rain of Blades,
    {331, {69, 64}},                              // Reclaim Energy,
    {332, {125, 21}},                             // Shield of Fire,
    {333, {79, 43}},                              // Phantom Leather,
    {334, {25, 38}},                              // Shock of Flame,
    {335, {69, 100}},                             // Minor Summoning: Earth,
    {336, {69, 105}},                             // Minor Summoning: Water,
    {337, {95, 96}},                              // Rage,
    {338, {69, 103}},                             // Cavorting Bones,
    {339, {18, 109}},                             // Coldlight,
    {340, {20, 29}},                              // Disease Cloud,
    {341, {114, 43}},                             // Lifetap,
    {342, {125, 64}},                             // Locate Corpse,
    {343, {114, 76}},                             // Siphon Strength,
    {344, {20, 58}},                              // Clinging Darkness,
    {345, {125, 64}},                             // Shrink,
    {346, {95, 7}},                               // Grim Aura,
    {347, {126, 11}},                             // Numb the Dead,
    {348, {20, 75}},                              // Poison Bolt,
    {349, {95, 24}},                              // Rising Dexterity,
    {350, {25, 58}},                              // Chaos Flux,
    {351, {69, 103}},                             // Bone Walk,
    {352, {125, 129}},                            // Deadeye,
    {353, {69, 42}},                              // Mend Bones,
    {354, {125, 86}},                             // Shadow Step,
    {355, {20, 58}},                              // Engulfing Darkness,
    {356, {125, 21}},                             // Shield of Thorns,
    {357, {42, 56}},                              // Dark Empathy,
    {358, {95, 96}},                              // Impart Strength,
    {359, {125, 16}},                             // Vampiric Embrace,
    {360, {20, 38}},                              // Heat Blood,
    {361, {125, 129}},                            // Sight Graft,
    {362, {69, 103}},                             // Convoke Shadow,
    {363, {126, 30}},                             // Wave of Enfeeblement,
    {364, {125, 21}},                             // Banshee Aura,
    {365, {20, 29}},                              // Infectious Cloud,
    {366, {125, 64}},                             // Feign Death,
    {367, {20, 29}},                              // Heart Flutter,
    {368, {95, 6}},                               // Spirit Armor,
    {369, {126, 83}},                             // Hungry Earth,
    {370, {126, 30}},                             // Shadow Vortex,
    {371, {125, 64}},                             // Voice Graft,
    {372, {25, 14}},                              // Blast of Cold,
    {373, {18, 109}},                             // Sphere of Light,
    {374, {25, 14}},                              // Numbing Cold,
    {375, {125, 86}},                             // Fade,
    {376, {25, 38}},                              // Shock of Fire,
    {377, {25, 14}},                              // Icestrike,
    {378, {125, 21}},                             // O'Keils Radiation,
    {379, {25, 38}},                              // Fingers of Fire,
    {380, {25, 14}},                              // Column of Frost,
    {381, {95, 80}},                              // Resistant Skin,
    {382, {25, 14}},                              // Frost Spiral of Al'Kabor,
    {383, {25, 58}},                              // Shock of Lightning,
    {384, {125, 129}},                            // Assiduous Vision,
    {385, {25, 58}},                              // Project Lightning,
    {386, {25, 38}},                              // Pillar of Fire,
    {387, {125, 84}},                             // Leatherskin,
    {388, {42, 82}},                              // Resuscitate,
    {389, {95, 6}},                               // Guardian,
    {390, {18, 64}},                              // Thicken Mana,
    {391, {42, 82}},                              // Revive,
    {392, {42, 82}},                              // Resurrection,
    {393, {125, 84}},                             // Steelskin,
    {394, {125, 84}},                             // Diamondskin,
    {395, {69, 102}},                             // Minor Summoning: Fire,
    {396, {69, 98}},                              // Minor Summoning: Air,
    {397, {69, 100}},                             // Elementaling: Earth,
    {398, {69, 105}},                             // Elementaling: Water,
    {399, {69, 102}},                             // Elementaling: Fire,
    {400, {69, 98}},                              // Elementaling: Air,
    {401, {69, 100}},                             // Elemental: Earth,
    {402, {69, 105}},                             // Elemental: Water,
    {403, {69, 102}},                             // Elemental: Fire,
    {404, {69, 98}},                              // Elemental: Air,
    {405, {25, 58}},                              // Tremor,
    {406, {25, 58}},                              // Earthquake,
    {407, {125, 129}},                            // Cast Sight,
    {408, {126, 30}},                             // Curse of the Simple Mind,
    {409, {25, 58}},                              // Rain of Spikes,
    {410, {25, 58}},                              // Rain of Swords,
    {411, {125, 21}},                             // Shield of Flame,
    {412, {125, 21}},                             // Shield of Lava,
    {413, {25, 58}},                              // Word of Shadow,
    {414, {25, 58}},                              // Word of Spirit,
    {415, {25, 58}},                              // Word of Souls,
    {416, {25, 58}},                              // Word Divine,
    {417, {42, 94}},                              // Extinguish Fatigue,
    {418, {25, 58}},                              // Lightning Strike,
    {419, {25, 58}},                              // Careless Lightning,
    {420, {25, 58}},                              // Lightning Blast,
    {421, {45, 46}},                              // Skin like Steel,
    {422, {45, 46}},                              // Skin like Diamond,
    {423, {45, 46}},                              // Skin like Nature,
    {424, {125, 65}},                             // Scale of Wolf,
    {425, {125, 65}},                             // Wolf Form,
    {426, {125, 65}},                             // Greater Wolf Form,
    {427, {125, 65}},                             // Form of the Great Wolf,
    {428, {125, 65}},                             // Share Wolf Form,
    {429, {95, 96}},                              // Strength of Stone,
    {430, {95, 96}},                              // Storm Strength,
    {431, {95, 6}},                               // Shifting Shield,
    {432, {125, 21}},                             // Shield of Spikes,
    {433, {25, 38}},                              // Fire,
    {434, {20, 75}},                              // Envenomed Breath,
    {435, {20, 75}},                              // Venom of the Snake,
    {436, {20, 75}},                              // Envenomed Bolt,
    {437, {25, 75}},                              // Poison Storm,
    {438, {25, 75}},                              // Gale of Poison,
    {439, {18, 64}},                              // Crystallize Mana,
    {440, {69, 103}},                             // Animate Dead,
    {441, {69, 103}},                             // Summon Dead,
    {442, {69, 103}},                             // Malignant Dead,
    {443, {69, 103}},                             // Invoke Death,
    {444, {69, 42}},                              // Renew Bones,
    {445, {114, 43}},                             // Lifedraw,
    {446, {114, 43}},                             // Siphon Life,
    {447, {114, 43}},                             // Drain Soul,
    {448, {126, 11}},                             // Rest the Dead,
    {449, {69, 70}},                              // Intensify Death,
    {450, {20, 58}},                              // Suffocate,
    {451, {20, 38}},                              // Boil Blood,
    {452, {20, 58}},                              // Dooming Darkness,
    {453, {20, 58}},                              // Cascading Darkness,
    {454, {114, 33}},                             // Vampiric Curse,
    {455, {126, 30}},                             // Surge of Enfeeblement,
    {456, {114, 33}},                             // Bond of Death,
    {457, {125, 55}},                             // Dead Man Floating,
    {458, {25, 38}},                              // Fire Spiral of Al'Kabor,
    {459, {25, 58}},                              // Shock Spiral of Al'Kabor,
    {460, {25, 58}},                              // Force Spiral of Al'Kabor,
    {461, {25, 58}},                              // Cast Force,
    {462, {25, 38}},                              // Column of Lightning,
    {463, {25, 38}},                              // Circle of Force,
    {464, {25, 14}},                              // Frost Shock,
    {465, {25, 38}},                              // Inferno Shock,
    {466, {25, 58}},                              // Lightning Shock,
    {467, {25, 58}},                              // Lightning Storm,
    {468, {25, 58}},                              // Energy Storm,
    {469, {25, 38}},                              // Lava Storm,
    {470, {25, 58}},                              // Thunder Strike,
    {471, {25, 58}},                              // Thunderclap,
    {472, {126, 37}},                             // Inspire Fear2,
    {473, {126, 37}},                             // Invoke Fear II,
    {474, {126, 37}},                             // Radius of Fear2,
    {475, {126, 37}},                             // Fear2,
    {477, {25, 38}},                              // Fire Bolt,
    {478, {125, 64}},                             // Breath of the Dead,
    {479, {125, 21}},                             // Inferno Shield,
    {480, {126, 63}},                             // Atone,
    {481, {125, 84}},                             // Rune I,
    {482, {125, 84}},                             // Rune II,
    {483, {125, 84}},                             // Rune III,
    {484, {125, 84}},                             // Rune IV,
    {485, {45, 112}},                             // Symbol of Transal,
    {486, {45, 112}},                             // Symbol of Ryltan,
    {487, {45, 112}},                             // Symbol of Pinzarn,
    {488, {45, 112}},                             // Symbol of Naltron,
    {489, {95, 12}},                              // Sympathetic Aura,
    {490, {126, 83}},                             // Enveloping Roots,
    {491, {69, 103}},                             // Leering Corpse,
    {492, {69, 103}},                             // Restless Bones,
    {493, {69, 103}},                             // Haunting Corpse,
    {494, {69, 103}},                             // Invoke Shadow,
    {495, {69, 103}},                             // Cackling Bones,
    {496, {69, 100}},                             // Lesser Summoning: Earth,
    {497, {69, 105}},                             // Lesser Summoning: Water,
    {498, {69, 102}},                             // Lesser Summoning: Fire,
    {499, {69, 98}},                              // Lesser Summoning: Air,
    {500, {125, 129}},                            // Bind Sight,
    {501, {126, 11}},                             // Soothe,
    {502, {114, 43}},                             // Lifespike,
    {503, {25, 97}},                              // Tishan's Clash,
    {504, {95, 96}},                              // Frenzied Strength,
    {505, {126, 88}},                             // Walking Sleep,
    {506, {126, 88}},                             // Tagar's Insects,
    {507, {126, 88}},                             // Togor's Insects,
    {508, {25, 14}},                              // Frost Strike,
    {509, {25, 14}},                              // Winter's Roar,
    {510, {25, 14}},                              // Blizzard Blast,
    {511, {20, 29}},                              // Affliction,
    {512, {126, 89}},                             // Ensnare,
    {513, {126, 11}},                             // Calm Animal,
    {514, {126, 37}},                             // Terrorize Animal,
    {515, {125, 21}},                             // Thistlecoat,
    {516, {125, 21}},                             // Barbcoat,
    {517, {125, 21}},                             // Bramblecoat,
    {518, {125, 21}},                             // Spikecoat,
    {519, {125, 21}},                             // Thorncoat,
    {520, {25, 58}},                              // Dizzying Wind,
    {521, {20, 58}},                              // Choke,
    {522, {125, 51}},                             // Gather Shadows,
    {524, {114, 43}},                             // Spirit Tap,
    {525, {114, 43}},                             // Drain Spirit,
    {526, {126, 81}},                             // Insidious Fever,
    {527, {126, 81}},                             // Insidious Malady,
    {528, {125, 86}},                             // Yonder,
    {529, {125, 129}},                            // Gaze,
    {530, {123, 5}},                              // Ring of Karana,
    {531, {123, 5}},                              // Ring of Commons,
    {532, {123, 36}},                             // Ring of Butcher,
    {533, {123, 67}},                             // Ring of Toxxulia,
    {534, {123, 5}},                              // Ring of Lavastorm,
    {535, {123, 5}},                              // Ring of Ro,
    {536, {123, 5}},                              // Ring of Feerrott,
    {537, {123, 36}},                             // Ring of Steamfont,
    {538, {123, 5}},                              // Ring of Misty,
    {539, {125, 129}},                            // Chill Sight,
    {540, {18, 64}},                              // Clarify Mana,
    {541, {123, 67}},                             // Tox Gate,
    {542, {123, 5}},                              // North Gate,
    {543, {123, 36}},                             // Fay Gate,
    {544, {123, 5}},                              // Common Gate,
    {545, {123, 5}},                              // Nek Gate,
    {546, {123, 5}},                              // Cazic Gate,
    {547, {123, 5}},                              // Ro Gate,
    {548, {123, 5}},                              // West Gate,
    {549, {126, 35}},                             // Screaming Terror,
    {550, {123, 5}},                              // Circle of Karana,
    {551, {123, 5}},                              // Circle of Commons,
    {552, {123, 67}},                             // Circle of Toxxulia,
    {553, {123, 36}},                             // Circle of Butcher,
    {554, {123, 5}},                              // Circle of Lavastorm,
    {555, {123, 5}},                              // Circle of Ro,
    {556, {123, 5}},                              // Circle of Feerrott,
    {557, {123, 36}},                             // Circle of Steamfont,
    {558, {123, 5}},                              // Circle of Misty,
    {559, {25, 38}},                              // Ignite Bones,
    {560, {25, 58}},                              // Furor,
    {561, {123, 67}},                             // Tox Portal,
    {562, {123, 5}},                              // North Portal,
    {563, {123, 36}},                             // Fay Portal,
    {564, {123, 5}},                              // Nek Portal,
    {565, {123, 5}},                              // Cazic Portal,
    {566, {123, 5}},                              // Common Portal,
    {567, {123, 5}},                              // Ro Portal,
    {568, {123, 5}},                              // West Portal,
    {569, {69, 100}},                             // Summoning: Earth,
    {570, {69, 105}},                             // Summoning: Water,
    {571, {69, 102}},                             // Summoning: Fire,
    {572, {69, 98}},                              // Summoning: Air,
    {573, {69, 100}},                             // Greater Summoning: Earth,
    {574, {69, 105}},                             // Greater Summoning: Water,
    {575, {69, 102}},                             // Greater Summoning: Fire,
    {576, {69, 98}},                              // Greater Summoning: Air,
    {577, {69, 104}},                             // Vigilant Spirit,
    {578, {125, 129}},                            // Sight,
    {579, {125, 129}},                            // Magnify,
    {580, {125, 129}},                            // Vision,
    {581, {125, 48}},                             // Illusion: Skeleton,
    {582, {125, 49}},                             // Illusion: Human,
    {583, {125, 49}},                             // Illusion: Half-Elf,
    {584, {125, 48}},                             // Illusion: Earth Elemental,
    {585, {125, 48}},                             // Illusion: Werewolf,
    {586, {125, 49}},                             // Illusion: Barbarian,
    {587, {125, 49}},                             // Illusion: Erudite,
    {588, {125, 49}},                             // Illusion: Wood Elf,
    {589, {125, 49}},                             // Illusion: High Elf,
    {590, {125, 49}},                             // Illusion: Dark Elf,
    {591, {125, 49}},                             // Illusion: Dwarf,
    {592, {125, 49}},                             // Illusion: Troll,
    {593, {125, 49}},                             // Illusion: Ogre,
    {594, {125, 49}},                             // Illusion: Halfling,
    {595, {125, 49}},                             // Illusion: Gnome,
    {596, {125, 48}},                             // Illusion: Dry Bone,
    {597, {125, 48}},                             // Illusion: Air Elemental,
    {598, {125, 48}},                             // Illusion: Fire Elemental,
    {599, {125, 48}},                             // Illusion: Water Elemental,
    {600, {125, 48}},                             // Illusion: Spirit Wolf,
    {601, {125, 48}},                             // Illusion: Tree,
    {602, {123, 5}},                              // Evacuate: North,
    {603, {123, 36}},                             // Evacuate: Fay,
    {604, {123, 5}},                              // Evacuate: Ro,
    {605, {123, 5}},                              // Evacuate: Nek,
    {606, {123, 5}},                              // Evacuate: West,
    {607, {123, 5}},                              // Succor: East,
    {608, {123, 36}},                             // Succor: Butcher,
    {609, {123, 5}},                              // Succor: Ro,
    {610, {123, 5}},                              // Succor: Lavastorm,
    {611, {123, 5}},                              // Succor: North,
    {612, {25, 97}},                              // Markar's Clash,
    {613, {18, 110}},                             // Staff of Tracing,
    {614, {18, 110}},                             // Staff of Warding,
    {615, {18, 110}},                             // Staff of Runes,
    {616, {18, 110}},                             // Staff of Symbols,
    {617, {18, 110}},                             // Sword of Runes,
    {618, {18, 109}},                             // Dimensional Hole,
    {619, {25, 97}},                              // Dyn`s Dizzying Draught,
    {620, {69, 100}},                             // Minor Conjuration: Earth,
    {621, {69, 105}},                             // Minor Conjuration: Water,
    {622, {69, 102}},                             // Minor Conjuration: Fire,
    {623, {69, 98}},                              // Minor Conjuration: Air,
    {624, {69, 100}},                             // Lesser Conjuration: Earth,
    {625, {69, 105}},                             // Lesser Conjuration: Water,
    {626, {69, 102}},                             // Lesser Conjuration: Fire,
    {627, {69, 98}},                              // Lesser Conjuration: Air,
    {628, {69, 100}},                             // Conjuration: Earth,
    {629, {69, 105}},                             // Conjuration: Water,
    {630, {69, 102}},                             // Conjuration: Fire,
    {631, {69, 98}},                              // Conjuration: Air,
    {632, {69, 100}},                             // Greater Conjuration: Earth,
    {633, {69, 105}},                             // Greater Conjuration: Water,
    {634, {69, 102}},                             // Greater Conjuration: Fire,
    {635, {69, 98}},                              // Greater Conjuration: Air,
    {636, {126, 89}},                             // Bonds of Force,
    {640, {125, 129}},                            // Creeping Vision,
    {641, {125, 17}},                             // Dark Pact,
    {642, {125, 17}},                             // Allure of Death,
    {643, {125, 17}},                             // Call of Bones,
    {644, {125, 17}},                             // Lich,
    {645, {126, 30}},                             // Ebbing Strength,
    {646, {95, 12}},                              // Radiant Visage,
    {647, {95, 12}},                              // Adorning Grace,
    {648, {125, 84}},                             // Rampage,
    {649, {95, 6}},                               // Protect,
    {650, {95, 6}},                               // Mist,
    {651, {95, 6}},                               // Cloud,
    {652, {95, 6}},                               // Obscure,
    {653, {95, 6}},                               // Shade,
    {654, {95, 6}},                               // Shadow,
    {655, {125, 129}},                            // Eyes of the Cat,
    {656, {25, 14}},                              // Shock of Ice,
    {657, {25, 38}},                              // Flame Shock,
    {658, {25, 14}},                              // Ice Shock,
    {659, {25, 38}},                              // Conflagration,
    {660, {25, 14}},                              // Frost Storm,
    {661, {69, 70}},                              // Augment Death,
    {662, {25, 124}},                             // Expel Undead,
    {663, {25, 111}},                             // Expulse Summoned,
    {664, {25, 111}},                             // Expel Summoned,
    {665, {20, 58}},                              // Drifting Death,
    {666, {123, 116}},                            // Alter Plane: Hate,
    {667, {18, 34}},                              // Enchant Silver,
    {668, {18, 34}},                              // Enchant Electrum,
    {669, {18, 34}},                              // Enchant Gold,
    {670, {18, 34}},                              // Enchant Platinum,
    {671, {25, 38}},                              // Starfire,
    {672, {25, 58}},                              // Retribution,
    {673, {25, 58}},                              // Discordant Mind,
    {674, {123, 116}},                            // Alter Plane: Sky,
    {675, {18, 110}},                             // Hammer of Requital,
    {676, {126, 81}},                             // Tashan,
    {677, {126, 81}},                             // Tashani,
    {678, {126, 81}},                             // Tashania,
    {679, {125, 129}},                            // Heat Sight,
    {680, {125, 21}},                             // Barrier of Combustion,
    {681, {69, 99}},                              // Juli`s Animation,
    {682, {69, 99}},                              // Kilan`s Animation,
    {683, {69, 99}},                              // Shalee`s Animation,
    {684, {69, 99}},                              // Sisna`s Animation,
    {685, {69, 99}},                              // Sagar`s Animation,
    {686, {69, 99}},                              // Uleen`s Animation,
    {687, {69, 99}},                              // Boltran`s Animation,
    {688, {69, 99}},                              // Aanya's Animation,
    {689, {69, 99}},                              // Yegoreff`s Animation,
    {690, {69, 99}},                              // Kintaz`s Animation,
    {691, {25, 38}},                              // Call of Flame,
    {692, {114, 43}},                             // Life Leech,
    {693, {125, 16}},                             // Divine Might,
    {694, {42, 56}},                              // Pact of Shadow,
    {695, {18, 64}},                              // Distill Mana,
    {696, {18, 64}},                              // Purify Mana,
    {697, {79, 59}},                              // Breeze,
    {698, {125, 64}},                             // Track Corpse,
    {699, {25, 74}},                              // Defoliate,
    {700, {125, 41}},                             // Chant of Battle,
    {701, {125, 41}},                             // Anthem de Arms,
    {702, {125, 41}},                             // McVaxius` Berserker Crescendo,
    {703, {20, 58}},                              // Chords of Dissonance,
    {704, {25, 58}},                              // Brusco`s Boastful Bellow,
    {705, {126, 88}},                             // Largo's Melodic Binding,
    {706, {126, 37}},                             // Angstlich`s Appalling Screech,
    {707, {20, 58}},                              // Fufil`s Curtailing Chant,
    {708, {125, 3}},                              // Cinda`s Charismatic Carillon,
    {709, {95, 80}},                              // Guardian Rhythms,
    {710, {95, 80}},                              // Elemental Rhythms,
    {711, {95, 80}},                              // Purifying Rhythms,
    {712, {125, 21}},                             // Psalm of Warmth,
    {713, {125, 21}},                             // Psalm of Cooling,
    {714, {95, 80}},                              // Psalm of Mystic Shielding,
    {715, {125, 21}},                             // Psalm of Vitality,
    {716, {125, 21}},                             // Psalm of Purity,
    {717, {125, 65}},                             // Selo`s Accelerando,
    {718, {125, 55}},                             // Agilmente`s Aria of Eagles,
    {719, {125, 51}},                             // Shauri`s Sonorous Clouding,
    {720, {125, 64}},                             // Lyssa`s Locating Lyric,
    {721, {125, 129}},                            // Lyssa`s Solidarity of Vision,
    {722, {42, 94}},                              // Jaxan`s Jig o` Vigor,
    {723, {79, 59}},                              // Cassindra's Chorus of Clarity,
    {724, {126, 35}},                             // Kelin`s Lucid Lullaby,
    {725, {126, 13}},                             // Solon's Song of the Sirens,
    {726, {126, 31}},                             // Syvelian`s Anti-Magic Aria,
    {727, {126, 31}},                             // Alenia`s Disenchanting Melody,
    {728, {126, 11}},                             // Kelin`s Lugubrious Lament,
    {729, {125, 64}},                             // Tarew`s Aquatic Ayre,
    {730, {20, 58}},                              // Denon`s Disruptive Discord,
    {731, {25, 14}},                              // Wrath of Al'Kabor,
    {732, {25, 14}},                              // Ice Comet,
    {733, {25, 38}},                              // Supernova,
    {734, {125, 41}},                             // Jonthan's Whistling Warsong,
    {735, {125, 129}},                            // Lyssa`s Veracious Concord,
    {736, {126, 60}},                             // Denon`s Dissension,
    {737, {125, 64}},                             // Lyssa`s Cataloging Libretto,
    {738, {126, 88}},                             // Selo`s Consonant Chain,
    {739, {125, 86}},                             // Melanie`s Mellifluous Motion,
    {740, {125, 41}},                             // Vilia`s Verses of Celerity,
    {741, {126, 35}},                             // Crission`s Pixie Strike,
    {742, {25, 58}},                              // Denon`s Desperate Dirge,
    {743, {20, 38}},                              // Tuyen`s Chant of Flame,
    {744, {20, 14}},                              // Tuyen`s Chant of Frost,
    {745, {95, 130}},                             // Cassindra`s Elegy,
    {746, {126, 88}},                             // Selo`s Chords of Cessation,
    {747, {125, 41}},                             // Verses of Victory,
    {748, {125, 64}},                             // Niv`s Melody of Preservation,
    {749, {125, 41}},                             // Jonthan's Provocation,
    {750, {126, 13}},                             // Solon's Bewitching Bravura,
    {752, {126, 53}},                             // Concussion,
    {753, {126, 13}},                             // Beguile Plants,
    {754, {125, 17}},                             // Cannibalize II,
    {755, {25, 58}},                              // Rend,
    {761, {25, 75}},                              // Contact Poison I,
    {763, {25, 75}},                              // System Shock I,
    {767, {25, 124}},                             // Liquid Silver I,
    {786, {25, 38}},                              // Wurm Blaze,
    {792, {25, 38}},                              // Fist of Fire,
    {793, {25, 58}},                              // Fist of Air,
    {794, {25, 58}},                              // Fist of Earth,
    {804, {25, 0}},                               // Magi Bolt,
    {805, {25, 38}},                              // Magi Strike,
    {807, {25, 58}},                              // Magi Circle,
    {808, {25, 0}},                               // Avatar Power,
    {812, {25, 58}},                              // SumMonsterAttack,
    {817, {25, 0}},                               // Guide Bolt,
    {823, {25, 58}},                              // Divine Might Effect,
    {829, {25, 38}},                              // FireHornet,
    {831, {25, 0}},                               // Sathir's Gaze,
    {832, {25, 38}},                              // WurmBreath,
    {834, {25, 0}},                               // Sathir's Mesmerization,
    {835, {25, 58}},                              // Chaos Breath,
    {837, {25, 58}},                              // Stun Breath,
    {839, {25, 58}},                              // Lightning Breath,
    {848, {25, 58}},                              // Elemental Mastery Strike,
    {849, {25, 14}},                              // ElementalMasteryBlast,
    {851, {25, 14}},                              // Shardwurm Breath,
    {859, {25, 38}},                              // Lava Breath - Test,
    {860, {25, 38}},                              // DrakeBreath,
    {861, {25, 38}},                              // Lava Breath,
    {862, {25, 14}},                              // Frost Breath,
    {863, {25, 58}},                              // Telekinesis,
    {868, {126, 35}},                             // Sionachie`s Dreams,
    {893, {25, 58}},                              // FireElementalAttack2,
    {904, {25, 58}},                              // Knockback,
    {907, {25, 38}},                              // DryBoneFireBurst,
    {908, {25, 14}},                              // IceBoneFrostBurst,
    {910, {25, 38}},                              // SnakeEleFireBurst,
    {917, {25, 38}},                              // Smolder,
    {922, {25, 58}},                              // Sonic,
    {929, {25, 58}},                              // Harm Touch NPC,
    {931, {25, 58}},                              // Life Drain,
    {945, {25, 58}},                              // Ykesha,
    {951, {25, 38}},                              // Fiery Death,
    {952, {25, 14}},                              // Frosty Death,
    {966, {25, 38}},                              // FireElementalAttack,
    {968, {25, 14}},                              // WaterElementalAttack,
    {978, {25, 14}},                              // FrostAOE,
    {982, {25, 0}},                               // Cazic Touch,
    {985, {25, 0}},                               // Efreeti Fire,
    {987, {25, 58}},                              // Spiroc Thunder,
    {988, {25, 0}},                               // Greater Spiroc Thunder,
    {989, {25, 0}},                               // Entomb in Ice,
    {995, {25, 0}},                               // Soul Devour,
    {1009, {25, 38}},                             // FireBeetleSpit,
    {1017, {25, 38}},                             // Fishnova,
    {1020, {25, 58}},                             // Air Elemental Strike,
    {1021, {25, 14}},                             // Water Elemental Strike,
    {1024, {25, 58}},                             // Thunderclap,
    {1026, {25, 58}},                             // Thunder Call,
    {1027, {25, 58}},                             // Thunder Storm,
    {1028, {25, 58}},                             // Static Storm,
    {1030, {25, 0}},                              // Sand Storm,
    {1031, {25, 0}},                              // Stone Gale,
    {1032, {25, 58}},                             // Hail Storm,
    {1036, {25, 0}},                              // Storm Flame,
    {1043, {25, 58}},                             // Manastorm,
    {1045, {25, 58}},                             // Chain Lightning,
    {1047, {25, 14}},                             // Deluge,
    {1048, {25, 14}},                             // Monsoons,
    {1049, {25, 58}},                             // Tempest Wind,
    {1050, {25, 14}},                             // Raging Blizzard,
    {1071, {25, 58}},                             // Punishing Blow,
    {1074, {25, 38}},                             // Steam Blast,
    {1075, {25, 58}},                             // Electrical Short,
    {1077, {25, 0}},                              // Mana Beam,
    {1078, {25, 58}},                             // Gyrosonic Disruption,
    {1084, {25, 0}},                              // Barrage of Debris,
    {1100, {126, 35}},                            // Dreams of Ayonae,
    {1106, {20, 38}},                             // Sear,
    {1107, {25, 58}},                             // Tremor of Judgment,
    {1142, {25, 58}},                             // Pain Harvest,
    {1144, {25, 58}},                             // Jagged Rain,
    {1145, {25, 14}},                             // Touch of Pain,
    {1151, {25, 0}},                              // Raven Screech,
    {1155, {25, 58}},                             // Black Symbol of Agony,
    {1167, {25, 38}},                             // Draconic Rage Strike,
    {1168, {25, 38}},                             // Draconic Rage Strike,
    {1172, {25, 75}},                             // Sting of the Shissar,
    {1173, {25, 75}},                             // Bite of the Shissar,
    {1180, {25, 124}},                            // Zombie Bane,
    {1181, {25, 124}},                            // Mayong's Bane,
    {1188, {25, 75}},                             // Bixie Sting,
    {1189, {25, 75}},                             // Scoriae Bite,
    {1194, {125, 49}},                            // Illusion: Fier`dal,
    {1196, {79, 44}},                             // Ancient: Lcea's Lament,
    {1197, {126, 35}},                            // Ancient: Lullaby of Shadow,
    {1216, {25, 0}},                              // Guide Bolt,
    {1221, {126, 53}},                            // Terror of Darkness,
    {1222, {126, 53}},                            // Terror of Shadows,
    {1223, {126, 53}},                            // Terror of Death,
    {1224, {126, 53}},                            // Terror of Terris,
    {1225, {125, 128}},                           // Voice of Darkness,
    {1226, {125, 128}},                           // Voice of Shadows,
    {1227, {125, 128}},                           // Voice of Death,
    {1228, {125, 128}},                           // Voice of Terris,
    {1244, {25, 0}},                              // Magi Bolt,
    {1245, {25, 38}},                             // Magi Strike,
    {1247, {25, 58}},                             // Magi Circle,
    {1269, {25, 75}},                             // Fangol's Breath,
    {1279, {25, 14}},                             // Velium Chill of Al`Kabor,
    {1283, {42, 32}},                             // Celestial Cleansing,
    {1284, {69, 71}},                             // Valiant Companion,
    {1285, {69, 64}},                             // Summon Companion,
    {1286, {69, 71}},                             // Expedience,
    {1287, {79, 59}},                             // Cassindra`s Chant of Clarity,
    {1288, {45, 47}},                             // Divine Glory,
    {1289, {69, 70}},                             // Strengthen Death,
    {1290, {42, 42}},                             // Chloroblast,
    {1291, {42, 42}},                             // Nature's Touch,
    {1296, {126, 53}},                            // Cinder Jolt,
    {1310, {25, 8}},                              // Porlos' Fury,
    {1311, {25, 8}},                              // Hsagra's Wrath,
    {1314, {25, 58}},                             // SpectraStun,
    {1317, {25, 58}},                             // Repulse,
    {1325, {123, 54}},                            // Combine Gate,
    {1326, {123, 54}},                            // Ring of the Combines,
    {1332, {125, 17}},                            // Cannibalize IV,
    {1334, {123, 64}},                            // Translocate: Group,
    {1336, {123, 36}},                            // Translocate: Fay,
    {1337, {123, 67}},                            // Translocate: Tox,
    {1338, {123, 5}},                             // Translocate: North,
    {1339, {123, 54}},                            // Translocate: Combine,
    {1356, {25, 14}},                             // Frosty Death2,
    {1359, {18, 34}},                             // Enchant Clay,
    {1366, {25, 0}},                              // Rage of the Sky,
    {1369, {25, 75}},                             // Poisonous Chill,
    {1371, {123, 5}},                             // Translocate: Nek,
    {1372, {123, 5}},                             // Translocate: Common,
    {1373, {123, 5}},                             // Translocate: Ro,
    {1374, {123, 5}},                             // Translocate: West,
    {1375, {123, 5}},                             // Translocate: Cazic,
    {1376, {125, 64}},                            // Shroud of Undeath,
    {1377, {95, 7}},                              // Primal Avatar,
    {1382, {18, 109}},                            // Summon Holy Ale of Brell,
    {1391, {125, 55}},                            // Dead Men Floating,
    {1392, {25, 58}},                             // Fireburst,
    {1393, {114, 43}},                            // Gangrenous Touch of Zum`uul,
    {1394, {25, 58}},                             // Maelstrom of Electricity,
    {1397, {45, 47}},                             // Strength of Nature,
    {1398, {123, 127}},                           // Circle of Wakening Lands,
    {1399, {123, 127}},                           // Wakening Lands Portal,
    {1400, {69, 64}},                             // Monster Summoning I,
    {1401, {18, 109}},                            // Summon Shard of the Core,
    {1402, {69, 64}},                             // Monster Summoning II,
    {1403, {20, 58}},                             // Elemental Maelstrom,
    {1404, {69, 64}},                             // Monster Summoning III,
    {1405, {20, 58}},                             // Wrath of the Elements,
    {1406, {125, 51}},                            // Improved Invisibility,
    {1407, {126, 60}},                            // Wandering Mind,
    {1408, {95, 59}},                             // Gift of Magic,
    {1409, {95, 59}},                             // Gift of Insight,
    {1410, {95, 59}},                             // Gift of Brilliance,
    {1411, {125, 51}},                            // Improved Invis to Undead,
    {1412, {20, 75}},                             // Chilling Embrace,
    {1413, {42, 56}},                             // Corporeal Empathy,
    {1414, {69, 70}},                             // Augmentation of Death,
    {1415, {25, 75}},                             // Torbas' Acid Blast,
    {1416, {125, 17}},                            // Arch Lich,
    {1417, {123, 127}},                           // Iceclad Gate,
    {1418, {123, 127}},                           // Iceclad Portal,
    {1419, {125, 21}},                            // O'Keils Flickering Flame,
    {1420, {125, 51}},                            // Invisibility to Undead,
    {1421, {25, 38}},                             // Enticement of Flame,
    {1422, {123, 64}},                            // Translocate,
    {1423, {123, 127}},                           // Great Divide Portal,
    {1425, {123, 127}},                           // Cobalt Scar Portal,
    {1426, {25, 14}},                             // Ice Spear of Solist,
    {1427, {25, 75}},                             // Shock of the Tainted,
    {1428, {95, 96}},                             // Tumultuous Strength,
    {1429, {25, 75}},                             // Blast of Poison,
    {1430, {69, 70}},                             // Spirit Quickening,
    {1431, {125, 48}},                            // Form of the Great Bear,
    {1432, {45, 87}},                             // Focus of Spirit,
    {1433, {123, 127}},                           // Ring of Iceclad,
    {1434, {123, 127}},                           // Circle of Iceclad,
    {1435, {125, 51}},                            // Improved Superior Camouflage,
    {1436, {126, 81}},                            // Fixation of Ro,
    {1437, {126, 81}},                            // Ro's Fiery Sundering,
    {1438, {123, 127}},                           // Circle of Great Divide,
    {1439, {25, 58}},                             // Fury of Air,
    {1440, {123, 127}},                           // Circle of Cobalt Scar,
    {1442, {45, 46}},                             // Protection of the Glades,
    {1443, {20, 124}},                            // Turning of the Unnatural,
    {1444, {42, 32}},                             // Celestial Healing,
    {1445, {45, 87}},                             // Armor of Protection,
    {1446, {25, 97}},                             // Stun Command,
    {1447, {45, 1}},                              // Aegolism,
    {1448, {79, 44}},                             // Cantata of Soothing,
    {1449, {125, 41}},                            // Melody of Ervaj,
    {1450, {125, 84}},                            // Shield of Songs,
    {1451, {126, 81}},                            // Occlusion of Sound,
    {1452, {125, 41}},                            // Composition of Ervaj,
    {1453, {125, 17}},                            // Divine Purpose,
    {1454, {25, 58}},                             // Flame of Light,
    {1455, {42, 42}},                             // Wave of Healing,
    {1456, {45, 47}},                             // Divine Strength,
    {1457, {114, 76}},                            // Shroud of Hate,
    {1458, {114, 76}},                            // Shroud of Pain,
    {1459, {125, 16}},                            // Shroud of Death,
    {1460, {125, 64}},                            // Death Peace,
    {1461, {125, 16}},                            // Call of Sky,
    {1462, {125, 21}},                            // Call of Earth,
    {1463, {125, 16}},                            // Call of Fire,
    {1464, {95, 7}},                              // Call of the Predator,
    {1465, {25, 58}},                             // Call of Sky Strike,
    {1467, {25, 38}},                             // Call of Fire Strike,
    {1472, {69, 70}},                             // Burnout IV,
    {1474, {125, 16}},                            // Boon of the Garou,
    {1475, {69, 104}},                            // Nature Walkers Behest,
    {1479, {25, 14}},                             // Wave of Flame,
    {1480, {25, 0}},                              // Silver Breath,
    {1481, {25, 58}},                             // Scream of Chaos,
    {1482, {25, 58}},                             // Electric Blast,
    {1484, {25, 14}},                             // Tsunami,
    {1487, {25, 14}},                             // Rain of Cold,
    {1488, {25, 38}},                             // Rain of Molten Lava,
    {1489, {25, 14}},                             // Wave of Cold,
    {1490, {25, 38}},                             // Wave of Heat,
    {1494, {25, 38}},                             // Flame Jet,
    {1498, {25, 14}},                             // Doljons Rage,
    {1503, {18, 109}},                            // Modulating Rod,
    {1504, {69, 42}},                             // Renew Elements,
    {1505, {69, 42}},                             // Renew Summoning,
    {1508, {20, 29}},                             // Asystole,
    {1509, {114, 33}},                            // Leach,
    {1510, {42, 56}},                             // Shadow Compact,
    {1511, {126, 81}},                            // Scent of Dusk,
    {1512, {126, 81}},                            // Scent of Shadow,
    {1513, {126, 81}},                            // Scent of Darkness,
    {1514, {42, 61}},                             // Rapacious Subvention,
    {1515, {42, 61}},                             // Covetous Subversion,
    {1516, {123, 54}},                            // Combine Portal,
    {1517, {123, 54}},                            // Circle of the Combines,
    {1518, {42, 42}},                             // Remedy,
    {1519, {42, 42}},                             // Divine Light,
    {1520, {42, 42}},                             // Word of Vigor,
    {1521, {42, 42}},                             // Word of Restoration,
    {1522, {42, 32}},                             // Celestial Elixir,
    {1523, {42, 42}},                             // Word of Redemption,
    {1524, {42, 82}},                             // Reviviscence,
    {1525, {42, 19}},                             // Antidote,
    {1526, {126, 31}},                            // Annul Magic,
    {1527, {126, 37}},                            // Trepidation,
    {1528, {25, 124}},                            // Exile Undead,
    {1529, {25, 111}},                            // Exile Summoned,
    {1530, {25, 23}},                             // Banishment of Shadows,
    {1531, {25, 23}},                             // Banishment,
    {1532, {126, 37}},                            // Dread of Night,
    {1533, {45, 46}},                             // Heroism,
    {1534, {95, 7}},                              // Yaulp IV,
    {1535, {45, 112}},                            // Symbol of Marzin,
    {1536, {45, 46}},                             // Heroic Bond,
    {1537, {95, 6}},                              // Bulwark of Faith,
    {1538, {45, 46}},                             // Heroic Bond,
    {1539, {45, 46}},                             // Fortitude,
    {1540, {95, 6}},                              // Aegis,
    {1541, {126, 11}},                            // Wake of Tranquility,
    {1542, {25, 58}},                             // Upheaval,
    {1543, {25, 58}},                             // Reckoning,
    {1544, {25, 97}},                             // Enforced Reverence,
    {1545, {25, 97}},                             // The Unspoken Word,
    {1546, {125, 64}},                            // Divine Intervention,
    {1547, {125, 64}},                            // Death Pact,
    {1548, {125, 64}},                            // Mark of Karn,
    {1550, {126, 37}},                            // Repulse Animal,
    {1551, {95, 80}},                             // Circle of Winter,
    {1552, {95, 80}},                             // Circle of Summer,
    {1553, {126, 13}},                            // Call of Karana,
    {1554, {125, 65}},                            // Spirit of Scale,
    {1555, {126, 81}},                            // Glamour of Tunare,
    {1556, {126, 13}},                            // Tunare's Request,
    {1557, {95, 96}},                             // Girdle of Karana,
    {1558, {125, 21}},                            // Bladecoat,
    {1559, {45, 46}},                             // Natureskin,
    {1560, {125, 21}},                            // Shield of Blades,
    {1561, {125, 21}},                            // Legacy of Thorn,
    {1562, {125, 65}},                            // Form of the Howler,
    {1563, {125, 65}},                            // Form of the Hunter,
    {1564, {125, 48}},                            // Spirit of Oak,
    {1565, {79, 59}},                             // Mask of the Hunter,
    {1566, {123, 64}},                            // Egress,
    {1567, {123, 64}},                            // Succor,
    {1568, {79, 43}},                             // Regrowth,
    {1569, {79, 43}},                             // Regrowth of the Grove,
    {1570, {95, 80}},                             // Talisman of Jasinth,
    {1571, {95, 80}},                             // Talisman of Shadoo,
    {1572, {125, 17}},                            // Cannibalize III,
    {1573, {126, 81}},                            // Insidious Decay,
    {1574, {69, 104}},                            // Spirit of the Howler,
    {1575, {125, 129}},                           // Acumen,
    {1576, {42, 32}},                             // Torpor,
    {1577, {126, 81}},                            // Malosini,
    {1578, {126, 81}},                            // Malo,
    {1579, {95, 2}},                              // Talisman of the Cat,
    {1580, {95, 94}},                             // Talisman of the Brute,
    {1581, {95, 96}},                             // Talisman of the Rhino,
    {1582, {95, 12}},                             // Talisman of the Serpent,
    {1583, {95, 24}},                             // Talisman of the Raptor,
    {1584, {95, 6}},                              // Shroud of the Spirits,
    {1585, {45, 87}},                             // Talisman of Kragg,
    {1586, {25, 14}},                             // Ice Strike,
    {1587, {25, 75}},                             // Torrent of Poison,
    {1588, {126, 88}},                            // Turgur's Insects,
    {1589, {126, 88}},                            // Tigir's Insects,
    {1590, {20, 75}},                             // Bane of Nife,
    {1591, {20, 29}},                             // Pox of Bertoxxulous,
    {1592, {126, 30}},                            // Cripple,
    {1593, {95, 96}},                             // Maniacal Strength,
    {1594, {95, 2}},                              // Deliriously Nimble,
    {1595, {95, 94}},                             // Riotous Health,
    {1596, {95, 24}},                             // Mortal Deftness,
    {1597, {95, 12}},                             // Unfailing Reverence,
    {1598, {95, 7}},                              // Avatar,
    {1599, {95, 96}},                             // Voice of the Berserker,
    {1600, {20, 38}},                             // Breath of Ro,
    {1601, {20, 58}},                             // Winged Death,
    {1602, {25, 14}},                             // Blizzard,
    {1603, {25, 38}},                             // Scoriae,
    {1604, {25, 58}},                             // Breath of Karana,
    {1605, {25, 14}},                             // Frost,
    {1606, {25, 58}},                             // Fist of Karana,
    {1607, {25, 38}},                             // Wildfire,
    {1608, {126, 83}},                            // Entrapping Roots,
    {1609, {125, 84}},                            // Manaskin,
    {1610, {45, 87}},                             // Shield of the Magi,
    {1611, {125, 17}},                            // Demi Lich,
    {1612, {125, 52}},                            // Quivering Veil of Xarn,
    {1613, {114, 43}},                            // Deflux,
    {1614, {25, 14}},                             // Chill Bones,
    {1615, {20, 29}},                             // Cessation of Cor,
    {1616, {114, 33}},                            // Vexing Mordinia,
    {1617, {20, 38}},                             // Pyrocruor,
    {1618, {114, 43}},                            // Touch of Night,
    {1619, {20, 58}},                             // Devouring Darkness,
    {1620, {20, 58}},                             // Splurt,
    {1621, {69, 103}},                            // Minion of Shadows,
    {1622, {69, 103}},                            // Servant of Bones,
    {1623, {69, 103}},                            // Emissary of Thule,
    {1624, {126, 13}},                            // Thrall of Bones,
    {1625, {125, 51}},                            // Skin of the Shadow,
    {1626, {123, 64}},                            // Levant,
    {1627, {123, 64}},                            // Abscond,
    {1628, {123, 64}},                            // Evacuate,
    {1629, {126, 13}},                            // Enslave Death,
    {1630, {25, 74}},                             // Defoliation,
    {1631, {126, 89}},                            // Atol's Spectral Shackles,
    {1632, {125, 129}},                           // Plainsight,
    {1633, {126, 83}},                            // Fetter,
    {1634, {25, 97}},                             // Tishan's Discord,
    {1635, {25, 97}},                             // Markar's Discord,
    {1636, {25, 58}},                             // Invert Gravity,
    {1637, {25, 38}},                             // Draught of Fire,
    {1638, {25, 38}},                             // Lure of Flame,
    {1639, {25, 58}},                             // Voltaic Draught,
    {1640, {25, 58}},                             // Lure of Lightning,
    {1641, {25, 14}},                             // Draught of Ice,
    {1642, {25, 14}},                             // Lure of Frost,
    {1643, {25, 58}},                             // Draught of Jiva,
    {1644, {25, 38}},                             // Pillar of Flame,
    {1645, {25, 58}},                             // Pillar of Lightning,
    {1646, {25, 14}},                             // Pillar of Frost,
    {1647, {25, 14}},                             // Tears of Prexus,
    {1648, {25, 38}},                             // Tears of Solusek,
    {1649, {25, 58}},                             // Tears of Druzzil,
    {1650, {25, 38}},                             // Inferno of Al'Kabor,
    {1651, {25, 14}},                             // Retribution of Al'Kabor,
    {1652, {25, 58}},                             // Vengeance of Al'Kabor,
    {1653, {25, 58}},                             // Jyll's Static Pulse,
    {1654, {25, 14}},                             // Jyll's Zephyr of Ice,
    {1655, {25, 38}},                             // Jyll's Wave of Heat,
    {1656, {25, 58}},                             // Thunderbold,
    {1657, {25, 14}},                             // Winds of Gelid,
    {1658, {25, 38}},                             // Sunstrike,
    {1659, {25, 38}},                             // Scintillation,
    {1660, {25, 38}},                             // Char,
    {1661, {25, 38}},                             // Scars of Sigil,
    {1662, {25, 38}},                             // Sirocco,
    {1663, {25, 58}},                             // Shock of Steel,
    {1664, {25, 38}},                             // Seeking Flame of Seukor,
    {1665, {25, 58}},                             // Manastorm,
    {1666, {79, 43}},                             // Phantom Armor,
    {1667, {125, 21}},                            // Cadeau of Flame,
    {1668, {125, 21}},                            // Boon of Immolation,
    {1669, {125, 21}},                            // Aegis of Ro,
    {1670, {69, 71}},                             // Velocity,
    {1671, {69, 100}},                            // Vocarate: Earth,
    {1672, {69, 105}},                            // Vocarate: Water,
    {1673, {69, 102}},                            // Vocarate: Fire,
    {1674, {69, 98}},                             // Vocarate: Air,
    {1675, {69, 100}},                            // Greater Vocaration: Earth,
    {1676, {69, 105}},                            // Greater Vocaration: Water,
    {1677, {69, 102}},                            // Greater Vocaration: Fire,
    {1678, {69, 98}},                             // Greater Vocaration: Air,
    {1679, {69, 64}},                             // Dyzil's Deafening Decoy,
    {1680, {18, 108}},                            // Gift of Xev,
    {1681, {18, 109}},                            // Bristlebane's Bundle,
    {1682, {18, 110}},                            // Quiver of Marr,
    {1683, {18, 110}},                            // Bandoleer of Luclin,
    {1684, {18, 110}},                            // Pouch of Quellious,
    {1685, {18, 109}},                            // Muzzle of Mardu,
    {1686, {126, 60}},                            // Theft of Thought,
    {1687, {125, 3}},                             // Collaboration,
    {1688, {95, 130}},                            // Enlightenment,
    {1689, {125, 84}},                            // Rune V,
    {1690, {126, 35}},                            // Fascination,
    {1691, {126, 35}},                            // Glamour of Kintaz,
    {1692, {126, 35}},                            // Rapture,
    {1693, {79, 59}},                             // Clarity II,
    {1694, {79, 59}},                             // Boon of the Clear Mind,
    {1695, {79, 59}},                             // Gift of Pure Thought,
    {1696, {25, 97}},                             // Color Slant,
    {1697, {126, 31}},                            // Recant Magic,
    {1698, {25, 58}},                             // Dementia,
    {1699, {126, 81}},                            // Wind of Tashani,
    {1700, {20, 58}},                             // Torment of Argli,
    {1701, {95, 12}},                             // Overwhelming Splendor,
    {1702, {126, 81}},                            // Tashanian,
    {1703, {20, 58}},                             // Asphyxiate,
    {1704, {126, 81}},                            // Wind of Tashanian,
    {1705, {126, 13}},                            // Boltran`s Agacerie,
    {1707, {126, 13}},                            // Dictate,
    {1708, {125, 41}},                            // Aanya's Quickening,
    {1709, {125, 41}},                            // Wonderous Rapidity,
    {1710, {125, 41}},                            // Visions of Grandeur,
    {1711, {95, 6}},                              // Umbra,
    {1712, {126, 88}},                            // Forlorn Deeds,
    {1713, {125, 84}},                            // Bedlam,
    {1714, {126, 63}},                            // Memory Flux,
    {1715, {126, 60}},                            // Largarn's Lamentation,
    {1716, {126, 81}},                            // Scent of Terris,
    {1717, {42, 56}},                             // Shadowbond,
    {1718, {42, 61}},                             // Sedulous Subversion,
    {1719, {126, 83}},                            // Engorging Roots,
    {1720, {125, 129}},                           // Eye of Tallon,
    {1721, {69, 99}},                             // Unswerving Hammer of Faith,
    {1722, {69, 99}},                             // Flaming Sword of Xuzl,
    {1723, {69, 99}},                             // Zumaik`s Animation,
    {1724, {25, 23}},                             // Disintegrate,
    {1725, {125, 64}},                            // Wake of Karana,
    {1726, {125, 51}},                            // Sunskin,
    {1727, {125, 21}},                            // Legacy of Spike,
    {1728, {125, 93}},                            // Manasink,
    {1729, {125, 41}},                            // Augment,
    {1733, {42, 82}},                             // Convergence,
    {1734, {42, 61}},                             // Infusion,
    {1735, {114, 43}},                            // Trucidation,
    {1736, {123, 54, "Wind of the North (SF)"}},  // Wind of the North,
    {1737, {123, 54, "Wind of the South (EJ)"}},  // Wind of the South,
    {1738, {123, 54}},                            // Tishan's Relocation,
    {1739, {123, 54}},                            // Markar's Relocation,
    {1740, {25, 58}},                             // Dustdevil,
    {1741, {126, 53}},                            // Jolt,
    {1742, {125, 55}},                            // Bobbing Corpse,
    {1743, {45, 47}},                             // Divine Favor,
    {1744, {42, 61}},                             // Harvest,
    {1747, {25, 58}},                             // Brusco`s Bombastic Bellow,
    {1748, {20, 58}},                             // Angstlich's Assonance,
    {1749, {125, 52}},                            // Kazumi's Note of Preservation,
    {1750, {125, 65}},                            // Selo`s Song of Travel,
    {1751, {126, 88}},                            // Largo`s Absonant Binding,
    {1752, {125, 84}},                            // Nillipus` March of the Wee,
    {1753, {126, 35}},                            // Song of Twilight,
    {1754, {126, 35}},                            // Song of Dawn,
    {1755, {125, 86}},                            // Song of Highsun,
    {1756, {126, 37}},                            // Song of Midnight,
    {1757, {125, 41}},                            // Vilia`s Chorus of Celerity,
    {1758, {126, 88}},                            // Selo`s Assonant Strane,
    {1759, {79, 44}},                             // Cantata of Replenishment,
    {1760, {125, 21}},                            // McVaxius` Rousing Rondo,
    {1761, {126, 60}},                            // Cassindra's Insipid Ditty,
    {1762, {125, 41}},                            // Jonthan's Inspiration,
    {1763, {95, 6}},                              // Niv`s Harmonic,
    {1764, {20, 75}},                             // Denon`s Bereavement,
    {1765, {95, 12}},                             // Solon's Charismatic Concord,
    {1767, {126, 89}},                            // Bonds of Tunare,
    {1768, {18, 50}},                             // Sacrifice,
    {1769, {25, 14}},                             // Lure of Ice,
    {1770, {69, 64}},                             // Rage of Zomm,
    {1771, {123, 64}},                            // Call of the Hero,
    {1772, {126, 81}},                            // Mala,
    {1773, {125, 64}},                            // Conjure Corpse,
    {1774, {45, 112}},                            // Naltron's Mark,
    {1776, {125, 65}},                            // Spirit of Wolf,
    {1784, {25, 14}},                             // Velium Shards,
    {1785, {25, 38}},                             // Flamesong,
    {1793, {25, 14}},                             // Judgment of Ice,
    {1794, {25, 14}},                             // Shards of Sorrow,
    {1797, {18, 34}},                             // Enchant Velium,
    {1798, {18, 50}},                             // Imbue Opal,
    {1799, {18, 50}},                             // Imbue Topaz,
    {1800, {18, 50}},                             // Imbue Plains Pebble,
    {1802, {25, 0}},                              // Storm Strike,
    {1803, {25, 58}},                             // Shrieking Howl,
    {1807, {25, 58}},                             // Stunning Blow,
    {1812, {25, 0}},                              // Nature's Wrath,
    {1815, {25, 38}},                             // Flames of Ro,
    {1819, {95, 96}},                             // Primal Essence,
    {1820, {25, 58}},                             // Divine Wrath,
    {1827, {25, 14}},                             // Frost Shards,
    {1831, {125, 64}},                            // Diminution,
    {1834, {25, 75}},                             // Poison Animal I,
    {1835, {25, 75}},                             // Poison Summoned I,
    {1843, {25, 75}},                             // Poison Animal II,
    {1844, {25, 75}},                             // Poison Animal III,
    {1845, {25, 75}},                             // Poison Summoned II,
    {1846, {25, 75}},                             // Poison Summoned III,
    {1853, {25, 75}},                             // Contact Poison II,
    {1854, {25, 75}},                             // Contact Poison III,
    {1855, {25, 75}},                             // Contact Poison IV,
    {1860, {25, 75}},                             // System Shock II,
    {1861, {25, 75}},                             // System Shock III,
    {1862, {25, 75}},                             // System Shock IV,
    {1870, {25, 124}},                            // Liquid Silver II,
    {1871, {25, 124}},                            // Liquid Silver III,
    {1874, {125, 64}},                            // Ant Legs,
    {1881, {25, 75}},                             // System Shock V,
    {1884, {18, 50}},                             // Imbue Ivory,
    {1885, {18, 50}},                             // Imbue Amber,
    {1886, {18, 50}},                             // Imbue Sapphire,
    {1887, {18, 50}},                             // Imbue Ruby,
    {1888, {18, 50}},                             // Imbue Emerald,
    {1889, {18, 34}},                             // Enchant Mithril,
    {1890, {18, 34}},                             // Enchant Adamantite,
    {1891, {18, 50}},                             // Imbue Jade,
    {1892, {18, 34}},                             // Enchant Steel,
    {1893, {18, 34}},                             // Enchant Brellium,
    {1894, {18, 50}},                             // Imbue Black Pearl,
    {1895, {18, 50}},                             // Imbue Diamond,
    {1896, {18, 50}},                             // Imbue Rose Quartz,
    {1897, {18, 50}},                             // Imbue Black Sapphire,
    {1898, {18, 50}},                             // Imbue Peridot,
    {1899, {18, 50}},                             // Imbue Fire Opal,
    {1941, {25, 38}},                             // Lava Breath,
    {1942, {25, 14}},                             // Frost Breath,
    {1943, {25, 38}},                             // Molten Breath,
    {1944, {18, 110}},                            // Summon Orb,
    {1947, {25, 14}},                             // Ice Rend,
    {1948, {25, 0}},                              // Destroy,
    {1953, {25, 58}},                             // Mastodon Stomp,
    {1954, {25, 0}},                              // Devour Soul,
    {1955, {25, 38}},                             // DrakeBreathBig,
    {1957, {25, 58}},                             // Holy Shock,
    {1968, {25, 58}},                             // Stunning Strike,
    {1969, {25, 38}},                             // Flame of the Efreeti,
    {1970, {25, 58}},                             // Verlekarnorm's Disaster,
    {1971, {25, 58}},                             // Rocksmash,
    {2005, {25, 58}},                             // Nature's Holy Wrath,
    {2006, {25, 58}},                             // Static,
    {2014, {25, 38}},                             // Incinerate Bones,
    {2015, {25, 14}},                             // Conglaciation of Bone,
    {2016, {25, 58}},                             // Dementing Visions,
    {2019, {25, 58}},                             // Thunder Strike,
    {2020, {123, 5}},                             // Circle of Surefall Glade,
    {2021, {123, 5}},                             // Ring of Surefall Glade,
    {2022, {123, 127}},                           // Translocate: Iceclad,
    {2023, {123, 127}},                           // Translocate: Great Divide,
    {2024, {123, 127}},                           // Translocate: Wakening Lands,
    {2025, {123, 127}},                           // Translocate: Cobalt Scar,
    {2026, {123, 127}},                           // Great Divide Gate,
    {2027, {123, 127}},                           // Wakening Lands Gate,
    {2028, {123, 127}},                           // Cobalt Scar Gate,
    {2029, {123, 127}},                           // Ring of Great Divide,
    {2030, {123, 127}},                           // Ring of Wakening Lands,
    {2031, {123, 127}},                           // Ring of Cobalt Scar,
    {2035, {25, 75}},                             // Tentacle Sting,
    {2036, {25, 0}},                              // Rain of the Arch Mage,
    {2040, {25, 58}},                             // Winds of the Archmage,
    {2043, {25, 75}},                             // Kambooz's Touch,
    {2047, {25, 0}},                              // Death Shackles,
    {2048, {25, 58}},                             // Ssraeshza's Command,
    {2054, {25, 14}},                             // Icy Claws,
    {2068, {25, 14}},                             // Blast of Frost,
    {2070, {25, 38}},                             // Marauder's Wrath,
    {2075, {25, 29}},                             // Umbral Rot,
    {2076, {25, 0}},                              // Presence of Ssraeshza,
    {2085, {25, 0}},                              // Lesser Infusion,
    {2086, {25, 0}},                              // Infusion,
    {2087, {25, 0}},                              // Greater Infusion,
    {2091, {25, 0}},                              // Lesser Rejuvenation,
    {2092, {25, 0}},                              // Rejuvination,
    {2093, {25, 0}},                              // Greater Rejuvenation,
    {2094, {25, 0}},                              // Zruk Breath,
    {2101, {25, 0}},                              // Pain and Suffering,
    {2102, {25, 75}},                             // Drake Breath,
    {2103, {25, 38}},                             // Drake Breath,
    {2104, {25, 14}},                             // Drake Breath,
    {2105, {25, 29}},                             // Drake Breath,
    {2106, {25, 0}},                              // Gift of A'err,
    {2109, {45, 87}},                             // Ancient: High Priest's Bulwark,
    {2110, {45, 46}},                             // Skin like Wood,
    {2111, {25, 38}},                             // Burst of Flame,
    {2112, {95, 7}},                              // Ancient: Feral Avatar,
    {2113, {20, 75}},                             // Ancient: Scourge of Nife,
    {2114, {125, 17}},                            // Ancient: Master of Death,
    {2115, {25, 75}},                             // Ancient: Lifebane,
    {2116, {25, 14}},                             // Ancient: Destruction of Ice,
    {2117, {126, 53}},                            // Ancient: Greater Concussion,
    {2118, {25, 38}},                             // Ancient: Shock of Sun,
    {2119, {69, 70}},                             // Ancient: Burnout Blaze,
    {2120, {126, 35}},                            // Ancient: Eternal Rapture,
    {2121, {25, 58}},                             // Ancient: Chaotic Visions,
    {2122, {45, 1}},                              // Ancient: Gift of Aegolism,
    {2125, {125, 21}},                            // Ancient: Legacy of Blades,
    {2126, {25, 38}},                             // Ancient: Starfire of Ro,
    {2127, {25, 0}},                              // Tragedy at Cazic Thule,
    {2130, {25, 58}},                             // Horrific Force,
    {2131, {25, 58}},                             // Vortex of Horror,
    {2137, {25, 75}},                             // Rain of Terror,
    {2139, {125, 64}},                            // Corpse Breath,
    {2156, {25, 0}},                              // Deadly Curse of Noqufiel,
    {2157, {25, 58}},                             // Word of Command,
    {2161, {25, 58}},                             // Shock of Shadows,
    {2162, {25, 58}},                             // Black Winds,
    {2163, {25, 58}},                             // Lure of Shadows,
    {2167, {25, 0}},                              // Fling,
    {2168, {42, 82}},                             // Reanimation,
    {2169, {42, 82}},                             // Reconstitution,
    {2170, {42, 82}},                             // Reparation,
    {2171, {42, 82}},                             // Renewal,
    {2172, {42, 82}},                             // Restoration,
    {2173, {25, 0}},                              // Hand of the Gods,
    {2175, {42, 32}},                             // Celestial Health,
    {2176, {79, 44}},                             // Spiritual Light,
    {2177, {79, 44}},                             // Spiritual Radiance,
    {2178, {45, 47}},                             // Spiritual Brawn,
    {2179, {42, 42}},                             // Tunare's Renewal,
    {2180, {42, 32}},                             // Ethereal Elixir,
    {2181, {18, 110}},                            // Hammer of Judgment,
    {2182, {42, 42}},                             // Ethereal Light,
    {2183, {123, 64}},                            // Lesser Succor,
    {2184, {123, 64}},                            // Lesser Evacuate,
    {2188, {45, 46}},                             // Protection of the Cabbage,
    {2190, {25, 97}},                             // Divine Stun,
    {2202, {125, 93}},                            // Mana Shield,
    {2203, {125, 64}},                            // Donlo's Dementia,
    {2206, {25, 38}},                             // Tortured Memory,
    {2213, {125, 64}},                            // Lesser Summon Corpse,
    {2230, {18, 107}},                            // Summon Brass Choker,
    {2231, {18, 107}},                            // Summon Silver Choker,
    {2232, {18, 107}},                            // Summon Golden Choker,
    {2233, {18, 107}},                            // Summon Linen Mantle,
    {2234, {18, 107}},                            // Summon Leather Mantle,
    {2235, {18, 107}},                            // Summon Silken Mantle,
    {2236, {18, 107}},                            // Summon Jade Bracelet,
    {2237, {18, 107}},                            // Summon Opal Bracelet,
    {2238, {18, 107}},                            // Summon Ruby Bracelet,
    {2239, {18, 107}},                            // Summon Tiny Ring,
    {2240, {18, 107}},                            // Summon Twisted Ring,
    {2241, {18, 107}},                            // Summon Studded Ring,
    {2242, {18, 107}},                            // Summon Tarnished Bauble,
    {2243, {18, 107}},                            // Summon Shiny Bauble,
    {2244, {18, 107}},                            // Summon Brilliant Bauble,
    {2248, {125, 129}},                           // Acumen,
    {2249, {25, 58}},                             // River's Rancor,
    {2250, {25, 38}},                             // Fiery Retribution,
    {2251, {25, 14}},                             // Furor of the Wild,
    {2255, {25, 14}},                             // Wrath of the Wild,
    {2258, {25, 14}},                             // Frigid Dominion,
    {2261, {25, 14}},                             // Frozen Torrent,
    {2264, {25, 14}},                             // Hail of Ice,
    {2268, {25, 0}},                              // Touch of the Void,
    {2312, {79, 43}},                             // Life Bind,
    {2321, {25, 0}},                              // Energy Burst,
    {2326, {95, 7}},                              // Yaulp V,
    {2375, {25, 0}},                              // Spectral Essence,
    {2377, {25, 58}},                             // Screeching Ricochet,
    {2378, {25, 14}},                             // Drakeen Breath,
    {2379, {25, 14}},                             // Drakeen Monsoon,
    {2380, {25, 14}},                             // Drakeen Vortex,
    {2381, {25, 58}},                             // Wing Draft,
    {2382, {25, 58}},                             // Wing Gust,
    {2383, {25, 58}},                             // Wing Squall,
    {2384, {25, 58}},                             // Wing Tempest,
    {2385, {25, 14}},                             // Frost Pummel,
    {2386, {25, 14}},                             // Ice Pummel,
    {2387, {25, 14}},                             // Frigid Shard Pummel,
    {2392, {25, 0}},                              // Sweltering Carcass,
    {2417, {123, 57}},                            // Ring of Grimling,
    {2418, {123, 57}},                            // Grimling Gate,
    {2419, {123, 57}},                            // Circle of Grimling,
    {2420, {123, 57}},                            // Grimling Portal,
    {2421, {123, 57}},                            // Translocate: Grimling,
    {2422, {123, 57}},                            // Ring of Twilight,
    {2423, {123, 57}},                            // Twilight Gate,
    {2424, {123, 57}},                            // Circle of Twilight,
    {2425, {123, 57}},                            // Twilight Portal,
    {2426, {123, 57}},                            // Translocate: Twilight,
    {2427, {123, 57}},                            // Ring of Dawnshroud,
    {2428, {123, 57}},                            // Dawnshroud Gate,
    {2429, {123, 57}},                            // Circle of Dawnshroud,
    {2430, {123, 57}},                            // Dawnshroud Portal,
    {2431, {123, 57}},                            // Translocate: Dawnshroud,
    {2432, {123, 57}},                            // Circle of the Nexus,
    {2433, {123, 57}},                            // Ring of the Nexus,
    {2434, {95, 7}},                              // Avatar,
    {2435, {42, 42}},                             // Kragg's Mending,
    {2436, {25, 58}},                             // War Arrows,
    {2437, {25, 0}},                              // Hendin Arrow,
    {2443, {25, 58}},                             // Blade of Vallon,
    {2450, {25, 38}},                             // Barb of Tallon,
    {2452, {25, 75}},                             // Barb of Tallon,
    {2453, {25, 0}},                              // Thorns of Drunder,
    {2462, {42, 77}},                             // Ethereal Remedy,
    {2501, {125, 64}},                            // Sanctuary,
    {2502, {42, 32}},                             // Celestial Remedy,
    {2503, {20, 124}},                            // Sermon of the Righteous,
    {2504, {25, 58}},                             // Sacred Word,
    {2505, {45, 87}},                             // Armor of the Faithful,
    {2506, {20, 124}},                            // Epitaph of Life,
    {2507, {125, 21}},                            // Mark of Retribution,
    {2508, {25, 58}},                             // Judgment,
    {2509, {45, 87}},                             // Blessed Armor of the Risen,
    {2510, {45, 1}},                              // Blessing of Aegolism,
    {2511, {45, 46}},                             // Protection of Wood,
    {2512, {45, 46}},                             // Protection of Rock,
    {2513, {45, 46}},                             // Protection of Steel,
    {2514, {45, 46}},                             // Protection of Diamond,
    {2515, {45, 46}},                             // Protection of Nature,
    {2516, {125, 51}},                            // Foliage Shield,
    {2517, {125, 65}},                            // Spirit of Eagle,
    {2518, {126, 81}},                            // Ro's Smoldering Disjunction,
    {2519, {95, 80}},                             // Circle of Seasons,
    {2520, {42, 32}},                             // Nature's Recovery,
    {2521, {95, 96}},                             // Talisman of the Beast,
    {2522, {125, 64}},                            // Grow,
    {2523, {125, 48}},                            // Form of the Bear,
    {2524, {125, 65}},                            // Spirit of Bih`Li,
    {2525, {45, 87}},                             // Harnessing of Spirit,
    {2526, {42, 19}},                             // Disinfecting Aura,
    {2527, {126, 88}},                            // Plague of Insects,
    {2528, {79, 43}},                             // Regrowth of Dar Khura,
    {2529, {95, 80}},                             // Talisman of Epuration,
    {2530, {45, 87}},                             // Khura's Focusing,
    {2531, {18, 106}},                            // Summon Elemental Defender,
    {2532, {18, 106}},                            // Summon Phantom Leather,
    {2533, {18, 106}},                            // Summon Phantom Chain,
    {2534, {18, 106}},                            // Summon Phantom Plate,
    {2535, {18, 106}},                            // Summon Elemental Blanket,
    {2536, {69, 42}},                             // Transon's Elemental Infusion,
    {2537, {125, 51}},                            // Veil of Elements,
    {2538, {18, 109}},                            // Mass Mystical Transvergance,
    {2539, {45, 44}},                             // Transon's Phantasmal Protection,
    {2540, {25, 38}},                             // Shock of Fiery Blades,
    {2541, {69, 70}},                             // Focus Death,
    {2542, {126, 124}},                           // Shackle of Bone,
    {2543, {20, 124}},                            // Eternities Torment,
    {2544, {126, 124}},                           // Shackle of Spirit,
    {2545, {126, 89}},                            // Insidious Retrogression,
    {2546, {114, 76}},                            // Degeneration,
    {2547, {114, 76}},                            // Succussion of Shadows,
    {2548, {114, 76}},                            // Crippling Claudication,
    {2549, {126, 60}},                            // Mind Wrack,
    {2550, {114, 33}},                            // Zevfeer's Theft of Vitae,
    {2551, {125, 21}},                            // O'Keils Embers,
    {2552, {25, 58}},                             // Garrisons Mighty Mana Shock,
    {2553, {69, 101}},                            // Minor Familiar,
    {2554, {126, 83}},                            // Elnerick's Entombment of Ice,
    {2555, {69, 101}},                            // Lesser Familiar,
    {2556, {69, 71}},                             // Firetree's Familiar Augment,
    {2557, {69, 101}},                            // Familiar,
    {2558, {123, 64}},                            // Decession,
    {2559, {125, 93}},                            // Spellshield,
    {2560, {69, 101}},                            // Greater Familiar,
    {2561, {95, 39}},                             // Intellectual Advancement,
    {2562, {95, 39}},                             // Intellectual Superiority,
    {2563, {125, 128}},                           // Haunting Visage,
    {2564, {125, 128}},                           // Calming Visage,
    {2565, {125, 48}},                            // Illusion: Imp,
    {2566, {125, 16}},                            // Trickster's Augmentation,
    {2567, {125, 128}},                           // Beguiling Visage,
    {2568, {125, 128}},                           // Horrifying Visage,
    {2569, {125, 128}},                           // Glamorous Visage,
    {2570, {79, 59}},                             // Koadic's Endless Intellect,
    {2571, {114, 76}},                            // Despair,
    {2572, {114, 76}},                            // Scream of Hate,
    {2573, {114, 76}},                            // Scream of Pain,
    {2574, {125, 16}},                            // Scream of Death,
    {2575, {114, 76}},                            // Abduction of Strength,
    {2576, {125, 16}},                            // Mental Corruption,
    {2577, {114, 76}},                            // Torrent of Hate,
    {2578, {114, 76}},                            // Torrent of Pain,
    {2579, {114, 76}},                            // Torrent of Fatigue,
    {2580, {45, 87}},                             // Cloak of the Akheva,
    {2581, {25, 97}},                             // Cease,
    {2582, {25, 97}},                             // Desist,
    {2583, {125, 16}},                            // Instrument of Nife,
    {2584, {45, 47}},                             // Divine Vigor,
    {2585, {125, 41}},                            // Valor of Marr,
    {2586, {126, 60}},                            // Thunder of Karana,
    {2587, {25, 97}},                             // Quellious' Word of Tranquility,
    {2588, {125, 17}},                            // Breath of Tunare,
    {2589, {42, 42}},                             // Healing Wave of Prexus,
    {2590, {45, 47}},                             // Brell's Mountainous Barrier,
    {2591, {126, 89}},                            // Tangling Weeds,
    {2592, {125, 129}},                           // Hawk Eye,
    {2593, {125, 21}},                            // Riftwind's Protection,
    {2594, {95, 7}},                              // Nature's Precision,
    {2595, {125, 21}},                            // Force of Nature,
    {2596, {125, 129}},                           // Falcon Eye,
    {2597, {126, 53}},                            // Jolting Blades,
    {2598, {95, 7}},                              // Mark of the Predator,
    {2599, {125, 129}},                           // Eagle Eye,
    {2600, {45, 47}},                             // Warder's Protection,
    {2601, {125, 64}},                            // Magical Monologue,
    {2602, {125, 64}},                            // Song of Sustenance,
    {2603, {125, 64}},                            // Amplification,
    {2604, {125, 16}},                            // Katta's Song of Sword Dancing,
    {2605, {125, 65}},                            // Selo`s Accelerating Chorus,
    {2606, {125, 41}},                            // Battlecry of the Vah Shir,
    {2607, {95, 80}},                             // Elemental Chorus,
    {2608, {95, 80}},                             // Purifying Chorus,
    {2609, {79, 44}},                             // Chorus of Replenishment,
    {2610, {125, 41}},                            // Warsong of the Vah Shir,
    {2611, {69, 42}},                             // Sharik's Replenishing,
    {2612, {69, 104}},                            // Spirit of Sharik,
    {2613, {69, 42}},                             // Keshuval's Rejuvenation,
    {2614, {69, 104}},                            // Spirit of Keshuval,
    {2615, {69, 42}},                             // Herikol's Soothing,
    {2616, {69, 104}},                            // Spirit of Herikol,
    {2617, {69, 42}},                             // Yekan's Recovery,
    {2618, {69, 104}},                            // Spirit of Yekan,
    {2619, {69, 70}},                             // Yekan's Quickening,
    {2620, {69, 42}},                             // Vigor of Zehkes,
    {2621, {69, 104}},                            // Spirit of Kashek,
    {2622, {69, 42}},                             // Aid of Khurenz,
    {2623, {69, 104}},                            // Spirit of Omakin,
    {2624, {69, 42}},                             // Sha's Restoration,
    {2625, {69, 70}},                             // Omakin's Alacrity,
    {2626, {69, 104}},                            // Spirit of Zehkes,
    {2627, {69, 104}},                            // Spirit of Khurenz,
    {2628, {69, 70}},                             // Sha's Ferocity,
    {2629, {79, 44}},                             // Spiritual Purity,
    {2630, {45, 47}},                             // Spiritual Strength,
    {2631, {69, 104}},                            // Spirit of Khati Sha,
    {2632, {69, 104}},                            // Summon Warder,
    {2633, {69, 104}},                            // Spirit of Khaliz,
    {2634, {126, 88}},                            // Sha's Lethargy,
    {2635, {69, 71}},                             // Spirit of Lightning,
    {2636, {69, 71}},                             // Spirit of the Blizzard,
    {2637, {69, 71}},                             // Spirit of Inferno,
    {2638, {69, 71}},                             // Spirit of the Scorpion,
    {2639, {69, 71}},                             // Spirit of Vermin,
    {2640, {69, 71}},                             // Spirit of Wind,
    {2641, {69, 71}},                             // Spirit of the Storm,
    {2642, {25, 38}},                             // Claw of Khati Sha,
    {2650, {25, 38}},                             // Blazing Heat,
    {2651, {25, 58}},                             // Vibrant Might,
    {2652, {25, 58}},                             // Descending Might,
    {2654, {25, 38}},                             // Fireblast,
    {2656, {25, 58}},                             // Wrathful Strike,
    {2657, {25, 58}},                             // Terrifying Darkness,
    {2658, {25, 58}},                             // Lightning Surge,
    {2662, {25, 58}},                             // Storm of Lightning,
    {2663, {25, 58}},                             // Clash of Will,
    {2664, {25, 14}},                             // Frostcall,
    {2665, {25, 14}},                             // Wintercall,
    {2669, {25, 14}},                             // Storm of Ice,
    {2670, {25, 124}},                            // Rebuke the Dead,
    {2678, {25, 75}},                             // Fungal Vengeance,
    {2710, {25, 38}},                             // Trickster's Torment,
    {2711, {25, 38}},                             // Trickster's TormentSK,
    {2717, {25, 29}},                             // Mental Corruption Strike,
    {2720, {25, 58}},                             // Spirit of Lightning Strike,
    {2721, {25, 14}},                             // Spirit of Blizzard Strike,
    {2722, {25, 38}},                             // Spirit of Inferno Strike,
    {2723, {25, 75}},                             // Spirit of Scorpion Strike,
    {2724, {25, 29}},                             // Spirit of Vermin Strike,
    {2725, {25, 58}},                             // Spirit of Wind Strike,
    {2726, {25, 38}},                             // Spirit of Storm Strike,
    {2729, {25, 124}},                            // Condemnation of Nife,
    {2732, {25, 38}},                             // Molten Fist,
    {2734, {123, 57}},                            // The Nexus,
    {2736, {126, 31}},                            // SpellTheft1,
    {2742, {42, 19}},                             // Purify Soul,
    {2750, {125, 48}},                            // Rabid Bear,
    {2752, {69, 42}},                             // Restore Companion,
    {2754, {69, 70}},                             // Frenzied Burnout,
    {2757, {126, 37}},                            // Wave of Revulsion,
    {2758, {69, 101}},                            // Improved Familiar,
    {2759, {126, 13}},                            // Undead Pact,
    {2761, {126, 13}},                            // Dominating Gaze,
    {2762, {25, 29}},                             // Disease Touch,
    {2763, {25, 75}},                             // Poison Touch,
    {2764, {125, 64}},                            // Call to Corpse,
    {2766, {114, 43}},                            // Life Curse,
    {2767, {25, 58}},                             // Dragon Force,
    {2768, {25, 58}},                             // Grimling LT 30,
    {2770, {25, 75}},                             // Rain of Spores,
    {2802, {25, 0}},                              // Flurry of Pebbles,
    {2809, {25, 14}},                             // Wave of Death,
    {2816, {25, 58}},                             // Storm Tremor,
    {2822, {25, 58}},                             // Upheaval,
    {2826, {125, 49}},                            // Illusion: Vah Shir,
    {2833, {25, 38}},                             // AdvisorNova,
    {2836, {25, 14}},                             // Grimling Comet,
    {2842, {25, 29}},                             // Shrieker Stun,
    {2858, {25, 0}},                              // AcryliaKB,
    {2859, {25, 0}},                              // Touch of Vinitras,
    {2877, {25, 14}},                             // Moonfire,
    {2878, {25, 38}},                             // Fireclaw,
    {2879, {79, 43}},                             // Phantasmal Armor,
    {2880, {42, 19}},                             // Remove Greater Curse,
    {2881, {125, 64}},                            // Everlasting Breath,
    {2882, {69, 71}},                             // Firetree's Familiar Enhancement,
    {2883, {25, 58}},                             // Elnerick's Electrical Rending,
    {2884, {25, 38}},                             // Garrison's Superior Sundering,
    {2885, {20, 38}},                             // Funeral Pyre of Kelador,
    {2886, {125, 129}},                           // Acumen of Dar Khura,
    {2887, {79, 59}},                             // Mask of the Stalker,
    {2888, {69, 71}},                             // Spirit of Flame,
    {2889, {25, 38}},                             // Spirit of Flame Strike,
    {2890, {69, 71}},                             // Spirit of Snow,
    {2891, {25, 14}},                             // Spirit of Snow Strike,
    {2892, {125, 17}},                            // Deathly Temptation,
    {2893, {45, 112}},                            // Marzin's Mark,
    {2894, {125, 55}},                            // Levitation,
    {2895, {125, 41}},                            // Speed of the Brood,
    {2896, {69, 42}},                             // Transon's Elemental Renewal,
    {2901, {25, 0}},                              // Illumination,
    {2902, {25, 75}},                             // Shissar Broodling Poison,
    {2908, {25, 58}},                             // Banshee Wail,
    {2927, {25, 58}},                             // Storm Tremor,
    {2936, {126, 60}},                            // Ervaj's Lost Composition,
    {2941, {95, 7}},                              // Savagery,
    {2942, {126, 88}},                            // Sha's Advantage,
    {2943, {123, 57}},                            // Translocate: Nexus,
    {2944, {123, 57}},                            // Nexus Portal,
    {2945, {123, 57}},                            // Nexus Gate,
    {2946, {42, 19}},                             // Remove Curse,
    {2950, {25, 58}},                             // Grol Baku Strike,
    {2951, {25, 58}},                             // Grol Baku Strike,
    {2952, {25, 58}},                             // Strike of the Grol Baku,
    {2956, {25, 38}},                             // Fire Blast,
    {2957, {25, 14}},                             // Water Blast,
    {2969, {114, 76}},                            // Shadow Creep,
    {2973, {25, 38}},                             // Ember Strike,
    {2984, {25, 29}},                             // Lotus Spines,
    {2988, {25, 75}},                             // Wave of Toxicity,
    {2991, {25, 14}},                             // Deathly Ice,
    {2992, {25, 38}},                             // Deathly Fire,
    {2993, {25, 75}},                             // Deathly Spores,
    {2994, {25, 29}},                             // Deathly Fever,
    {2995, {25, 75}},                             // Deep Spores,
    {2996, {25, 58}},                             // Claw of the Hunter,
    {2997, {25, 38}},                             // Claw of the Beast,
    {2999, {25, 58}},                             // Claw of Bestial Fury,
    {3000, {25, 0}},                              // Devouring Nightmare,
    {3004, {25, 38}},                             // Fist of Lava,
    {3005, {25, 38}},                             // Ball of Lava,
    {3013, {25, 38}},                             // Fiery Strike,
    {3017, {25, 38}},                             // Mighty Bellow of Fire,
    {3018, {25, 38}},                             // Nova Inferno,
    {3020, {25, 38}},                             // Rain of Burning Fire,
    {3030, {126, 35}},                            // Dreams of Thule,
    {3031, {79, 44}},                             // Xegony's Phantasmal Guard,
    {3032, {114, 43}},                            // Touch of Mujaki,
    {3034, {69, 99}},                             // Aeldorb's Animation,
    {3035, {25, 75}},                             // Neurotoxin,
    {3036, {25, 14}},                             // Wrath of Ice,
    {3037, {25, 38}},                             // Wrath of Fire,
    {3038, {25, 58}},                             // Wrath of Wind,
    {3039, {125, 21}},                            // Protection of the Wild,
    {3040, {18, 64}},                             // Belt of Magi'Kot,
    {3041, {18, 64}},                             // Blade of Walnan,
    {3042, {18, 64}},                             // Fist of Ixiblat,
    {3043, {18, 64}},                             // Blade of The Kedge,
    {3044, {18, 64}},                             // Girdle of Magi'Kot,
    {3045, {18, 109}},                            // Talisman of Return,
    {3047, {45, 112}},                            // Kazad`s Mark,
    {3051, {25, 38}},                             // Fiery Assault,
    {3057, {25, 14}},                             // Tidal Freeze,
    {3063, {125, 49}},                            // Illusion: Froglok,
    {3066, {126, 88}},                            // Requiem of Time,
    {3069, {25, 58}},                             // Seething Hatred,
    {3071, {25, 38}},                             // Insipid Dreams,
    {3100, {125, 64}},                            // Mark of Retaliation,
    {3107, {125, 16}},                            // Cry of Fire,
    {3129, {25, 58}},                             // Call of Sky Strike,
    {3130, {25, 58}},                             // Call of Sky Strike,
    {3131, {25, 38}},                             // Call of Fire Strike,
    {3132, {25, 38}},                             // Call of Fire Strike,
    {3133, {25, 38}},                             // Cry of Fire Strike,
    {3134, {123, 116}},                           // Portal of Knowledge,
    {3135, {18, 110}},                            // Hammer of Divinity,
    {3136, {18, 110}},                            // Hammer of Souls,
    {3156, {25, 58}},                             // Torrent of Brambles,
    {3162, {25, 58}},                             // Wind Strike,
    {3163, {25, 58}},                             // Storm Avalanche,
    {3164, {25, 29}},                             // Froglok Misery,
    {3167, {25, 58}},                             // Strike of Marr,
    {3172, {25, 58}},                             // Denial,
    {3176, {25, 0}},                              // Butchery,
    {3177, {25, 58}},                             // Prayer of Pain,
    {3178, {125, 41}},                            // Vallon's Quickening,
    {3179, {25, 58}},                             // Spirit of Rellic Strike,
    {3180, {123, 116}},                           // Knowledge Portal,
    {3181, {123, 116}},                           // Translocate: Knowledge,
    {3182, {123, 116}},                           // Ring of Knowledge,
    {3183, {123, 116}},                           // Knowledge Gate,
    {3184, {123, 116}},                           // Circle of Knowledge,
    {3185, {125, 65}},                            // Flight of Eagles,
    {3186, {95, 7}},                              // Yaulp VI,
    {3187, {20, 124}},                            // Sermon of Penitence,
    {3188, {18, 109}},                            // Rod of Mystical Transvergance,
    {3189, {25, 38}},                             // Tears of Arlyxir,
    {3190, {42, 19}},                             // Crusader`s Touch,
    {3191, {25, 58}},                             // Shock of Magic,
    {3192, {126, 83}},                            // Earthen Roots,
    {3194, {126, 83}},                            // Greater Fetter,
    {3195, {126, 83}},                            // Greater Immobilize,
    {3196, {126, 83}},                            // Petrifying Earth,
    {3197, {126, 11}},                            // Pacification,
    {3198, {125, 21}},                            // Flameshield of Ro,
    {3199, {125, 84}},                            // Arcane Rune,
    {3205, {18, 107}},                            // Summon Platinum Choker,
    {3206, {18, 107}},                            // Summon Runed Mantle,
    {3207, {18, 107}},                            // Summon Sapphire Bracelet,
    {3208, {18, 107}},                            // Summon Spiked Ring,
    {3209, {18, 107}},                            // Summon Glowing Bauble,
    {3210, {18, 107}},                            // Summon Jewelry Bag,
    {3221, {25, 58}},                             // Shattering Glass,
    {3222, {25, 58}},                             // Web of Glass,
    {3223, {25, 58}},                             // Shards of Glass,
    {3227, {125, 16}},                            // Shroud of Chaos,
    {3229, {126, 53}},                            // Boggle,
    {3232, {42, 42}},                             // Karana's Renewal,
    {3233, {42, 42}},                             // Tnarg`s Mending,
    {3234, {45, 46}},                             // Protection of the Nine,
    {3235, {45, 87}},                             // Focus of Soul,
    {3236, {25, 75}},                             // no spell,
    {3237, {69, 70}},                             // Burnout V,
    {3238, {25, 111}},                            // Destroy Summoned,
    {3239, {69, 42}},                             // Planar Renewal,
    {3240, {125, 41}},                            // Speed of Vallon,
    {3241, {125, 16}},                            // Night`s Dark Terror,
    {3242, {95, 80}},                             // Guard of Druzzil,
    {3243, {123, 64}},                            // Teleport,
    {3244, {123, 64}},                            // Greater Decession,
    {3245, {25, 97}},                             // Force of Akilae,
    {3246, {126, 83}},                            // Shackles of Tunare,
    {3247, {45, 87}},                             // Aura of the Crusader,
    {3255, {125, 21}},                            // Wrath of the Wild,
    {3256, {125, 21}},                            // Wrath of the Wild,
    {3257, {125, 21}},                            // Wrath of the Wild,
    {3258, {125, 84}},                            // Eldritch Rune,
    {3259, {125, 84}},                            // Eldritch Rune,
    {3260, {125, 84}},                            // Eldritch Rune,
    {3264, {69, 101}},                            // Allegiant Familiar,
    {3265, {69, 102}},                            // Servant of Ro,
    {3266, {69, 102}},                            // Servant of Ro,
    {3267, {69, 102}},                            // Servant of Ro,
    {3271, {125, 48}},                            // Guardian of the Forest,
    {3272, {125, 48}},                            // Guardian of the Forest,
    {3273, {125, 48}},                            // Guardian of the Forest,
    {3274, {126, 83}},                            // Virulent Paralysis,
    {3275, {126, 83}},                            // Virulent Paralysis,
    {3276, {126, 83}},                            // Virulent Paralysis,
    {3281, {25, 38}},                             // Servant's Bolt,
    {3282, {25, 58}},                             // Boastful Bellow,
    {3289, {125, 48}},                            // Frenzy of Spirit,
    {3290, {69, 71}},                             // Hobble of Spirits,
    {3291, {79, 59}},                             // Paragon of Spirit,
    {3295, {125, 21}},                            // Legacy of Bracken,
    {3296, {45, 46}},                             // Faith,
    {3297, {42, 19}},                             // Radiant Cure1,
    {3298, {42, 19}},                             // Radiant Cure2,
    {3299, {42, 19}},                             // Radiant Cure3,
    {3300, {45, 87}},                             // Shield of the Arcane,
    {3301, {125, 84}},                            // Force Shield,
    {3302, {45, 87}},                             // Shield of Maelin,
    {3303, {20, 75}},                             // Blood of Thule,
    {3304, {69, 103}},                            // Legacy of Zek,
    {3305, {69, 70}},                             // Rune of Death,
    {3306, {114, 33}},                            // Saryrn's Kiss,
    {3308, {126, 35}},                            // Death's Silence,
    {3309, {20, 58}},                             // Embracing Darkness,
    {3310, {69, 103}},                            // Saryrn's Companion,
    {3311, {125, 17}},                            // Seduction of Saryrn,
    {3312, {69, 42}},                             // Touch of Death,
    {3314, {69, 103}},                            // Child of Bertoxxulous,
    {3315, {20, 29}},                             // Dark Plague,
    {3316, {126, 13}},                            // Word of Terris,
    {3317, {69, 98}},                             // Ward of Xegony,
    {3318, {25, 38}},                             // Firebolt of Tallon,
    {3319, {25, 38}},                             // Sun Storm,
    {3320, {69, 105}},                            // Servant of Marr,
    {3321, {25, 58}},                             // Black Steel,
    {3322, {69, 102}},                            // Child of Ro,
    {3323, {25, 58}},                             // Maelstrom of Thunder,
    {3324, {69, 100}},                            // Rathe's Son,
    {3325, {25, 38}},                             // Sun Vortex,
    {3326, {95, 80}},                             // Resistant Armor,
    {3327, {25, 38}},                             // Tears of Ro,
    {3328, {25, 58}},                             // Lure of Thunder,
    {3329, {95, 80}},                             // Elemental Barrier,
    {3330, {25, 38}},                             // Draught of Ro,
    {3331, {25, 38}},                             // Lure of Ro,
    {3332, {25, 14}},                             // Tears of Marr,
    {3333, {25, 97}},                             // Telekin,
    {3334, {25, 58}},                             // Draught of Thunder,
    {3335, {25, 58}},                             // Agnarr's Thunder,
    {3336, {25, 14}},                             // Draught of E`ci,
    {3337, {125, 64}},                            // Iceflame of E`ci,
    {3338, {42, 61}},                             // Harvest of Druzzil,
    {3339, {25, 38}},                             // Strike of Solusek,
    {3341, {126, 35}},                            // Apathy,
    {3342, {126, 81}},                            // Howl of Tashan,
    {3343, {125, 84}},                            // Rune of Zebuxoruk,
    {3344, {18, 50}},                             // Imbue Nightmare,
    {3345, {20, 58}},                             // Strangle,
    {3346, {18, 50}},                             // Imbue Storm,
    {3347, {126, 13}},                            // Beckon,
    {3348, {20, 58}},                             // Torment of Scio,
    {3349, {25, 58}},                             // Insanity,
    {3350, {79, 59}},                             // Tranquility,
    {3351, {125, 84}},                            // Uproar,
    {3352, {18, 50}},                             // Imbue Earth,
    {3353, {18, 50}},                             // Imbue Air,
    {3354, {126, 35}},                            // Sleep,
    {3355, {126, 13}},                            // Command of Druzzil,
    {3356, {18, 50}},                             // Imbue Fire,
    {3357, {18, 50}},                             // Imbue Water,
    {3358, {126, 35}},                            // Bliss,
    {3359, {126, 35}},                            // Word of Morell,
    {3360, {79, 59}},                             // Voice of Quellious,
    {3361, {126, 11}},                            // Silent Song of Quellious,
    {3362, {125, 41}},                            // Rizlona's Call of Flame,
    {3363, {20, 29}},                             // Tuyen`s Chant of the Plague,
    {3364, {126, 31}},                            // Druzzil's Disillusionment,
    {3365, {126, 88}},                            // Melody of Mischief,
    {3366, {25, 58}},                             // Saryrn's Scream of Pain,
    {3367, {20, 38}},                             // Tuyen`s Chant of Fire,
    {3368, {125, 21}},                            // Psalm of Veeshan,
    {3369, {126, 35}},                            // Dreams of Terris,
    {3370, {20, 75}},                             // Tuyen`s Chant of Venom,
    {3371, {126, 13}},                            // Call of the Banshee,
    {3372, {79, 44}},                             // Chorus of Marr,
    {3373, {20, 14}},                             // Tuyen`s Chant of Ice,
    {3374, {125, 41}},                            // Warsong of Zek,
    {3375, {126, 81}},                            // Harmony of Sound,
    {3376, {126, 35}},                            // Lullaby of Morell,
    {3377, {69, 104}},                            // True Spirit,
    {3378, {95, 2}},                              // Agility of the Wrulan,
    {3379, {25, 75}},                             // Spear of Torment,
    {3380, {126, 88}},                            // Cloud of Grummus,
    {3381, {95, 6}},                              // Ancestral Guard,
    {3382, {95, 94}},                             // Endurance of the Boar,
    {3383, {95, 2}},                              // Talisman of the Wrulan,
    {3384, {95, 80}},                             // Talisman of the Tribunal,
    {3385, {25, 75}},                             // Tears of Saryrn,
    {3386, {126, 81}},                            // Malicious Decay,
    {3387, {126, 81}},                            // Malosinia,
    {3388, {95, 96}},                             // Strength of the Diaku,
    {3389, {95, 94}},                             // Talisman of the Boar,
    {3390, {25, 14}},                             // Velium Strike,
    {3391, {125, 41}},                            // Talisman of Alacrity,
    {3392, {95, 96}},                             // Talisman of the Diaku,
    {3393, {125, 64}},                            // Tiny Terror,
    {3394, {20, 29}},                             // Breath of Ultor,
    {3395, {126, 81}},                            // Malos,
    {3396, {20, 75}},                             // Blood of Saryrn,
    {3397, {45, 87}},                             // Focus of the Seventh,
    {3398, {42, 32}},                             // Quiescence,
    {3399, {95, 7}},                              // Ferine Avatar,
    {3400, {20, 58}},                             // Festering Darkness,
    {3401, {114, 43}},                            // Touch of Volatis,
    {3403, {114, 76}},                            // Aura of Pain,
    {3405, {126, 53}},                            // Terror of Thule,
    {3406, {114, 76}},                            // Aura of Darkness,
    {3408, {114, 33}},                            // Zevfeer's Bite,
    {3410, {125, 128}},                           // Voice of Thule,
    {3411, {114, 76}},                            // Aura of Hate,
    {3413, {114, 43}},                            // Touch of Innoruuk,
    {3415, {125, 16}},                            // Nature's Rebuke,
    {3416, {25, 111}},                            // Nature's Rebuke Strike,
    {3417, {95, 7}},                              // Spirit of the Predator,
    {3418, {25, 14}},                             // Frozen Wind,
    {3419, {125, 21}},                            // Call of the Rathe,
    {3420, {125, 16}},                            // Cry of Thunder,
    {3421, {25, 58}},                             // Cry of Thunder Strike,
    {3422, {125, 16}},                            // Ward of Nife,
    {3423, {25, 124}},                            // Ward of Nife Strike,
    {3424, {125, 16}},                            // Pious Might,
    {3425, {25, 58}},                             // Pious Might Strike,
    {3426, {25, 97}},                             // Quellious' Word of Serenity,
    {3427, {42, 42}},                             // Wave of Marr,
    {3428, {25, 124}},                            // Deny Undead,
    {3429, {42, 42}},                             // Touch of Nife,
    {3430, {42, 77}},                             // Light of Nife,
    {3431, {25, 38}},                             // Brushfire,
    {3432, {45, 47}},                             // Brell's Stalwart Shield,
    {3433, {79, 43}},                             // Replenishment,
    {3434, {25, 58}},                             // Storm's Fury,
    {3435, {126, 81}},                            // Hand of Ro,
    {3436, {25, 14}},                             // Winter's Storm,
    {3437, {20, 38}},                             // Immolation of Ro,
    {3438, {25, 58}},                             // Karana's Rage,
    {3439, {95, 96}},                             // Nature's Might,
    {3440, {126, 81}},                            // Ro's Illumination,
    {3441, {79, 43}},                             // Blessing of Replenishment,
    {3442, {126, 81}},                            // E'ci's Frosty Breath,
    {3443, {42, 42}},                             // Nature's Infusion,
    {3444, {95, 80}},                             // Protection of Seasons,
    {3445, {126, 13}},                            // Command of Tunare,
    {3446, {20, 58}},                             // Swarming Death,
    {3447, {126, 83}},                            // Savage Roots,
    {3448, {125, 21}},                            // Shield of Bracken,
    {3449, {25, 38}},                             // Summer's Flame,
    {3450, {125, 21}},                            // Brackencoat,
    {3451, {45, 46}},                             // Blessing of the Nine,
    {3452, {25, 14}},                             // Winter's Frost,
    {3453, {79, 59}},                             // Mask of the Forest,
    {3454, {45, 96}},                             // Infusion of Spirit,
    {3455, {69, 42}},                             // Healing of Sorsha,
    {3456, {45, 47}},                             // Spiritual Vigor,
    {3457, {69, 104}},                            // Spirit of Arag,
    {3458, {69, 70}},                             // Arag`s Celerity,
    {3459, {69, 71}},                             // Spirit of Rellic,
    {3460, {79, 44}},                             // Spiritual Dominion,
    {3461, {69, 104}},                            // Spirit of Sorsha,
    {3462, {126, 88}},                            // Sha's Revenge,
    {3463, {95, 7}},                              // Ferocity,
    {3464, {25, 97}},                             // The Silent Command,
    {3465, {42, 77}},                             // Supernal Remedy,
    {3466, {45, 112}},                            // Symbol of Kazad,
    {3467, {45, 1}},                              // Virtue,
    {3468, {25, 124}},                            // Destroy Undead,
    {3469, {125, 64}},                            // Mark of Kings,
    {3470, {95, 6}},                              // Ward of Gallantry,
    {3471, {42, 42}},                             // Word of Replenishment,
    {3472, {125, 91}},                            // Blessing of Reverence,
    {3473, {25, 58}},                             // Catastrophe,
    {3474, {45, 87}},                             // Armor of the Zealot,
    {3475, {42, 32}},                             // Supernal Elixir,
    {3476, {25, 58}},                             // Condemnation,
    {3477, {125, 21}},                            // Mark of the Righteous,
    {3478, {18, 110}},                            // Hammer of Damnation,
    {3479, {45, 1}},                              // Hand of Virtue,
    {3480, {42, 42}},                             // Supernal Light,
    {3481, {25, 97}},                             // Tarnation,
    {3482, {25, 97}},                             // Sound of Might,
    {3483, {126, 35}},                            // Elemental Silence,
    {3484, {126, 13}},                            // Call of the Arch Mage,
    {3485, {42, 32}},                             // Supernal Cleansing,
    {3486, {125, 21}},                            // Maelstrom of Ro,
    {3487, {45, 47}},                             // Strength of Tunare,
    {3488, {125, 17}},                            // Pact of Hate,
    {3489, {20, 75}},                             // Blood of Hate,
    {3490, {45, 87}},                             // Cloak of Luclin,
    {3491, {25, 29}},                             // Spear of Decay,
    {3492, {20, 75}},                             // Scorpion Venom,
    {3493, {25, 14}},                             // Frost Spear,
    {3494, {18, 109}},                            // Luggald Blood,
    {3498, {25, 58}},                             // Gallenite's Lifetap Test,
    {3560, {25, 29}},                             // Spear of Pain,
    {3561, {25, 29}},                             // Spear of Disease,
    {3562, {25, 29}},                             // Spear of Plague,
    {3564, {25, 38}},                             // Burning Arrow,
    {3565, {25, 38}},                             // Flaming Arrow,
    {3566, {20, 75}},                             // Tuyen`s Chant of Poison,
    {3567, {20, 29}},                             // Tuyen`s Chant of Disease,
    {3568, {25, 14}},                             // Ice Spear,
    {3569, {25, 14}},                             // Frost Shard,
    {3570, {25, 14}},                             // Ice Shard,
    {3571, {25, 75}},                             // Torbas' Poison Blast,
    {3572, {25, 75}},                             // Torbas' Venom Blast,
    {3573, {25, 75}},                             // Shock of Venom,
    {3574, {25, 75}},                             // Blast of Venom,
    {3575, {125, 91}},                            // Blessing of Piety,
    {3576, {125, 91}},                            // Blessing of Faith,
    {3577, {42, 42}},                             // Wave of Life,
    {3578, {45, 47}},                             // Brell's Steadfast Aegis,
    {3579, {125, 65}},                            // Share Form of the Great Wolf,
    {3580, {125, 48}},                            // Spirit of Ash,
    {3581, {125, 55}},                            // O`Keils Levity,
    {3582, {95, 80}},                             // Elemental Cloak,
    {3583, {69, 64}},                             // Tiny Companion,
    {3584, {69, 42}},                             // Refresh Summoning,
    {3585, {126, 35}},                            // Entrancing Lights,
    {3586, {125, 65}},                            // Illusion: Scaled Wolf,
    {3587, {25, 58}},                             // Planar Strike,
    {3589, {25, 58}},                             // Ethereal Strike,
    {3591, {18, 50}},                             // Imbue Disease,
    {3592, {18, 50}},                             // Imbue Valor,
    {3593, {18, 50}},                             // Imbue War,
    {3594, {18, 50}},                             // Imbue Torment,
    {3595, {18, 50}},                             // Imbue Justice,
    {3601, {126, 11}},                            // Harmony of Nature,
    {3618, {25, 58}},                             // Eclipse Aura Strike,
    {3619, {25, 58}},                             // Eclipse Aura Strike,
    {3621, {25, 14}},                             // Frost Claw,
    {3623, {25, 38}},                             // Burning Barb,
    {3624, {25, 0}},                              // Anger,
    {3626, {25, 38}},                             // Tendrils of Fire,
    {3630, {25, 58}},                             // Time Lapse,
    {3645, {25, 58}},                             // Sting of Ayonae,
    {3646, {25, 29}},                             // Bite of Bertoxxulous,
    {3648, {25, 58}},                             // Time Snap,
    {3650, {25, 0}},                              // Dark Empathy Recourse,
    {3651, {79, 44}},                             // Wind of Marr,
    {3665, {25, 38}},                             // Curtain Call,
    {3668, {20, 29}},                             // Pawn's Plight,
    {3670, {25, 14}},                             // Queen's Checkmate,
    {3681, {42, 19}},                             // Aria of Innocence,
    {3682, {42, 19}},                             // Aria of Asceticism,
    {3683, {42, 32}},                             // Ethereal Cleansing,
    {3684, {42, 77}},                             // Light of Life,
    {3685, {125, 64}},                            // Comatose,
    {3686, {20, 75}},                             // Blood of Pain,
    {3687, {20, 58}},                             // Swarm of Pain,
    {3688, {25, 14}},                             // Icewind,
    {3689, {20, 29}},                             // Malaria,
    {3690, {69, 70}},                             // Bond of the Wild,
    {3691, {45, 0}},                              // Bond of the Wild R.,
    {3692, {45, 1}},                              // Temperance,
    {3693, {42, 19}},                             // Pure Blood,
    {3694, {42, 32}},                             // Stoicism,
    {3695, {126, 81}},                            // Frost Zephyr,
    {3696, {125, 129}},                           // Leviathan Eyes,
    {3697, {126, 60}},                            // Scryer's Trespass,
    {3699, {69, 42}},                             // Primal Remedy,
    {3700, {69, 70}},                             // Elemental Empathy,
    {3702, {114, 33}},                            // Auspice,
    {3704, {69, 0}},                              // Soul Empathy,
    {3706, {25, 14}},                             // Frozen Harpoon,
    {3748, {25, 29}},                             // Insipid Decay,
    {3753, {114, 76}},                            // Torrent of Agony,
    {3764, {25, 75}},                             // Rain of Bile,
    {3785, {25, 58}},                             // Hate's Fury,
    {3792, {123, 67}},                            // Circle of Stonebrunt,
    {3793, {123, 67}},                            // Stonebrunt Portal,
    {3794, {123, 67}},                            // Ring of Stonebrunt,
    {3795, {123, 67}},                            // Stonebrunt Gate,
    {3796, {126, 60}},                            // Mind Tap,
    {3799, {25, 97}},                             // Dismal Wind,
    {3803, {25, 58}},                             // Pique,
    {3806, {126, 53}},                            // Distraction,
    {3809, {79, 43}},                             // Reclamation,
    {3810, {25, 38}},                             // Eruption,
    {3811, {125, 129}},                           // Vision Shift,
    {3833, {123, 67}},                            // Translocate: Stonebrunt,
    {3834, {42, 42}},                             // Healing Water,
    {3842, {42, 19}},                             // Blood of Nadox,
    {3847, {125, 48}},                            // Cloak of Khala Dun,
    {3848, {25, 38}},                             // Tortured Memory II,
    {3849, {123, 116}},                           // Alter Plane: Hate II,
    {3854, {125, 48}},                            // Form of Protection,
    {3855, {125, 48}},                            // Form of Defense,
    {3856, {125, 48}},                            // Form of Endurance,
    {3857, {125, 48}},                            // Form of Rejuvenation,
    {3861, {125, 16}},                            // Pestilence Shock,
    {3864, {125, 16}},                            // Soul Claw,
    {3876, {25, 14}},                             // Frozen Shards,
    {3877, {20, 58}},                             // Nightmares,
    {3878, {25, 58}},                             // Time Rend,
    {3881, {25, 0}},                              // Hand of Retribution,
    {3909, {126, 88}},                            // Clinging Clay,
    {3910, {25, 38}},                             // Flames of Condemnation,
    {3921, {123, 64}},                            // Guide Evacuation,
    {3975, {25, 97}},                             // Force of Akera,
    {3976, {25, 58}},                             // Draught of Lightning,
    {3981, {18, 64}},                             // Mass Clarify Mana,
    {3982, {18, 64}},                             // Mass Crystallize Mana,
    {3983, {18, 64}},                             // Mass Distill Mana,
    {3984, {18, 34}},                             // Mass Enchant Adamantite,
    {3985, {18, 34}},                             // Mass Enchant Brellium,
    {3986, {18, 34}},                             // Mass Enchant Clay,
    {3987, {18, 34}},                             // Mass Enchant Electrum,
    {3988, {18, 34}},                             // Mass Enchant Gold,
    {3989, {18, 34}},                             // Mass Enchant Mithril,
    {3990, {18, 34}},                             // Mass Enchant Platinum,
    {3991, {18, 34}},                             // Mass Enchant Silver,
    {3992, {18, 34}},                             // Mass Enchant Steel,
    {3993, {18, 34}},                             // Mass Enchant Velium,
    {3994, {18, 50}},                             // Mass Imbue Amber,
    {3995, {18, 50}},                             // Mass Imbue Black Pearl,
    {3996, {18, 50}},                             // Mass Imbue Black Sapphire,
    {3997, {18, 50}},                             // Mass Imbue Diamond,
    {3998, {18, 50}},                             // Mass Imbue Emerald,
    {3999, {18, 50}},                             // Mass Imbue Fire Opal,
}};

SpellCat getSpellCategoryAndSubcategory(int spellID) {
  // Perform binary search for the spellID.
  auto it = std::lower_bound(spell_cat_lut.begin(), spell_cat_lut.end(), spellID,
                             [](const SpellCatEntry &entry, int value) { return entry.spell_id < value; });

  if (it != spell_cat_lut.end() && it->spell_id == spellID) return it->spell_cat;

  return SpellCat(0, 0);  // or throw std::out_of_range("Spell ID not found");
}

static inline std::string GetSpellCategoryName(DWORD categoryID) {
  switch (categoryID) {
    case 1:
      return "Aegolism";
    case 2:
      return "Agility";
    case 3:
      return "Alliance";
    case 4:
      return "Animal";
    case 5:
      return "Antonica";
    case 6:
      return "Armor Class";
    case 7:
      return "Attack";
    case 8:
      return "Bane";
    case 9:
      return "Blind";
    case 10:
      return "Block";
    case 11:
      return "Calm";
    case 12:
      return "Charisma";
    case 13:
      return "Charm";
    case 14:
      return "Cold";
    case 15:
      return "Combat Abilities";
    case 16:
      return "Combat Innates";
    case 17:
      return "Conversions";
    case 18:
      return "Create Item";
    case 19:
      return "Cure";
    case 20:
      return "Damage Over Time";
    case 21:
      return "Damage Shield";
    case 22:
      return "Defensive";
    case 23:
      return "Destroy";
    case 24:
      return "Dexterity";
    case 25:
      return "Direct Damage ";
    case 26:
      return "Disarm Traps";
    case 27:
      return "Disciplines";
    case 28:
      return "Discord";
    case 29:
      return "Disease";
    case 30:
      return "Disempowering";
    case 31:
      return "Dispel";
    case 32:
      return "Duration Heals";
    case 33:
      return "Duration Tap";
    case 34:
      return "Enchant Metal";
    case 35:
      return "Enthrall";
    case 36:
      return "Faydwer";
    case 37:
      return "Fear";
    case 38:
      return "Fire";
    case 39:
      return "Fizzle Rate";
    case 40:
      return "Fumble";
    case 41:
      return "Haste";
    case 42:
      return "Heals";
    case 43:
      return "Health";
    case 44:
      return "Health/Mana";
    case 45:
      return "HP Buffs";
    case 46:
      return "HP type one";
    case 47:
      return "HP type two";
    case 48:
      return "Illusion: Other";
    case 49:
      return "Illusion: Player";
    case 50:
      return "Imbue Gem";
    case 51:
      return "Invisibility";
    case 52:
      return "Invulnerability";
    case 53:
      return "Jolt";
    case 54:
      return "Kunark";
    case 55:
      return "Levitate";
    case 56:
      return "Life Flow";
    case 57:
      return "Luclin";
    case 58:
      return "Magic";
    case 59:
      return "Mana";
    case 60:
      return "Mana Drain";
    case 61:
      return "Mana Flow";
    case 62:
      return "Melee Guard";
    case 63:
      return "Memory Blur";
    case 64:
      return "Misc";
    case 65:
      return "Movement";
    case 66:
      return "Objects";
    case 67:
      return "Odus";
    case 68:
      return "Offensive";
    case 69:
      return "Pet";
    case 70:
      return "Pet Haste";
    case 71:
      return "Pet Misc Buffs";
    case 72:
      return "Physical";
    case 73:
      return "Picklock";
    case 74:
      return "Plant";
    case 75:
      return "Poison";
    case 76:
      return "Power Tap";
    case 77:
      return "Quick Heal";
    case 78:
      return "Reflection";
    case 79:
      return "Regen";
    case 80:
      return "Resist Buff";
    case 81:
      return "Resist Debuffs";
    case 82:
      return "Resurrection";
    case 83:
      return "Root";
    case 84:
      return "Rune";
    case 85:
      return "Sense Trap";
    case 86:
      return "Shadowstep";
    case 87:
      return "Shielding";
    case 88:
      return "Slow";
    case 89:
      return "Snare";
    case 90:
      return "Special";
    case 91:
      return "Spell Focus";
    case 92:
      return "Spell Guard";
    case 93:
      return "Spellshield";
    case 94:
      return "Stamina";
    case 95:
      return "Statistic Buffs";
    case 96:
      return "Strength";
    case 97:
      return "Stun";
    case 98:
      return "Sum: Air";
    case 99:
      return "Sum: Animation";
    case 100:
      return "Sum: Earth";
    case 101:
      return "Sum: Familiar";
    case 102:
      return "Sum: Fire";
    case 103:
      return "Sum: Undead";
    case 104:
      return "Sum: Warder";
    case 105:
      return "Sum: Water";
    case 106:
      return "Summon Armor";
    case 107:
      return "Summon Focus";
    case 108:
      return "Summon Food/Water";
    case 109:
      return "Summon Utility";
    case 110:
      return "Summon Weapon";
    case 111:
      return "Summoned";
    case 112:
      return "Symbol";
    case 113:
      return "Taelosia";
    case 114:
      return "Taps";
    case 115:
      return "Techniques";
    case 116:
      return "The Planes";
    case 117:
      return "Timer 1";
    case 118:
      return "Timer 2";
    case 119:
      return "Timer 3";
    case 120:
      return "Timer 4";
    case 121:
      return "Timer 5";
    case 122:
      return "Timer 6";
    case 123:
      return "Transport";
    case 124:
      return "Undead";
    case 125:
      return "Utility Beneficial";
    case 126:
      return "Utility Detrimental";
    case 127:
      return "Velious";
    case 128:
      return "Visages";
    case 129:
      return "Vision";
    case 130:
      return "Wisdom/Intelligence";
    case 131:
      return "Traps";
    case 132:
      return "Auras";
    case 133:
      return "Endurance";
    case 134:
      return "Serpent's Spine";
    case 135:
      return "Corruption";
    case 136:
      return "Learning";
    case 137:
      return "Chromatic";
    case 138:
      return "Prismatic";
    case 139:
      return "Sum: Swarm";
    case 140:
      return "Delayed";
    case 141:
      return "Temporary";
    case 142:
      return "Twincast";
    case 143:
      return "Sum: Bodyguard";
    case 144:
      return "Humanoid";
    case 145:
      return "Haste/Spell Focus";
    case 146:
      return "Timer 7";
    case 147:
      return "Timer 8";
    case 148:
      return "Timer 9";
    case 149:
      return "Timer 10";
    case 150:
      return "Timer 11";
    case 151:
      return "Timer 12";
    case 152:
      return "Hatred";
    case 153:
      return "Fast";
    case 154:
      return "Illusion: Special";
    case 155:
      return "Timer 13";
    case 156:
      return "Timer 14";
    case 157:
      return "Timer 15";
    case 158:
      return "Timer 16";
    case 159:
      return "Timer 17";
    case 160:
      return "Timer 18";
    case 161:
      return "Timer 19";
    case 162:
      return "Timer 20";
    case 163:
      return "Alaris";
    default:
      return "Unknown";
  }
}

static inline std::string GetSpellSubCategoryName(DWORD subcategoryID) {
  switch (subcategoryID) {
    case 1:
      return "Aegolism";
    case 2:
      return "Agility";
    case 3:
      return "Alliance";
    case 4:
      return "Animal";
    case 5:
      return "Antonica";
    case 6:
      return "Armor Class";
    case 7:
      return "Attack";
    case 8:
      return "Bane";
    case 9:
      return "Blind";
    case 10:
      return "Block";
    case 11:
      return "Calm";
    case 12:
      return "Charisma";
    case 13:
      return "Charm";
    case 14:
      return "Cold";
    case 15:
      return "Combat Abilities";
    case 16:
      return "Combat Innates";
    case 17:
      return "Conversions";
    case 18:
      return "Create Item";
    case 19:
      return "Cure";
    case 20:
      return "Damage Over Time";
    case 21:
      return "Damage Shield";
    case 22:
      return "Defensive";
    case 23:
      return "Destroy";
    case 24:
      return "Dexterity";
    case 25:
      return "Direct Damage ";
    case 26:
      return "Disarm Traps";
    case 27:
      return "Disciplines";
    case 28:
      return "Discord";
    case 29:
      return "Disease";
    case 30:
      return "Disempowering";
    case 31:
      return "Dispel";
    case 32:
      return "Duration Heals";
    case 33:
      return "Duration Tap";
    case 34:
      return "Enchant Metal";
    case 35:
      return "Enthrall";
    case 36:
      return "Faydwer";
    case 37:
      return "Fear";
    case 38:
      return "Fire";
    case 39:
      return "Fizzle Rate";
    case 40:
      return "Fumble";
    case 41:
      return "Haste";
    case 42:
      return "Heals";
    case 43:
      return "Health";
    case 44:
      return "Health/Mana";
    case 45:
      return "HP Buffs";
    case 46:
      return "HP type one";
    case 47:
      return "HP type two";
    case 48:
      return "Illusion: Other";
    case 49:
      return "Illusion: Player";
    case 50:
      return "Imbue Gem";
    case 51:
      return "Invisibility";
    case 52:
      return "Invulnerability";
    case 53:
      return "Jolt";
    case 54:
      return "Kunark";
    case 55:
      return "Levitate";
    case 56:
      return "Life Flow";
    case 57:
      return "Luclin";
    case 58:
      return "Magic";
    case 59:
      return "Mana";
    case 60:
      return "Mana Drain";
    case 61:
      return "Mana Flow";
    case 62:
      return "Melee Guard";
    case 63:
      return "Memory Blur";
    case 64:
      return "Misc";
    case 65:
      return "Movement";
    case 66:
      return "Objects";
    case 67:
      return "Odus";
    case 68:
      return "Offensive";
    case 69:
      return "Pet";
    case 70:
      return "Pet Haste";
    case 71:
      return "Pet Misc Buffs";
    case 72:
      return "Physical";
    case 73:
      return "Picklock";
    case 74:
      return "Plant";
    case 75:
      return "Poison";
    case 76:
      return "Power Tap";
    case 77:
      return "Quick Heal";
    case 78:
      return "Reflection";
    case 79:
      return "Regen";
    case 80:
      return "Resist Buff";
    case 81:
      return "Resist Debuffs";
    case 82:
      return "Resurrection";
    case 83:
      return "Root";
    case 84:
      return "Rune";
    case 85:
      return "Sense Trap";
    case 86:
      return "Shadowstep";
    case 87:
      return "Shielding";
    case 88:
      return "Slow";
    case 89:
      return "Snare";
    case 90:
      return "Special";
    case 91:
      return "Spell Focus";
    case 92:
      return "Spell Guard";
    case 93:
      return "Spellshield";
    case 94:
      return "Stamina";
    case 95:
      return "Statistic Buffs";
    case 96:
      return "Strength";
    case 97:
      return "Stun";
    case 98:
      return "Sum: Air";
    case 99:
      return "Sum: Animation";
    case 100:
      return "Sum: Earth";
    case 101:
      return "Sum: Familiar";
    case 102:
      return "Sum: Fire";
    case 103:
      return "Sum: Undead";
    case 104:
      return "Sum: Warder";
    case 105:
      return "Sum: Water";
    case 106:
      return "Summon Armor";
    case 107:
      return "Summon Focus";
    case 108:
      return "Summon Food/Water";
    case 109:
      return "Summon Utility";
    case 110:
      return "Summon Weapon";
    case 111:
      return "Summoned";
    case 112:
      return "Symbol";
    case 113:
      return "Taelosia";
    case 114:
      return "Taps";
    case 115:
      return "Techniques";
    case 116:
      return "The Planes";
    case 117:
      return "Timer 1";
    case 118:
      return "Timer 2";
    case 119:
      return "Timer 3";
    case 120:
      return "Timer 4";
    case 121:
      return "Timer 5";
    case 122:
      return "Timer 6";
    case 123:
      return "Transport";
    case 124:
      return "Undead";
    case 125:
      return "Utility Beneficial";
    case 126:
      return "Utility Detrimental";
    case 127:
      return "Velious";
    case 128:
      return "Visages";
    case 129:
      return "Vision";
    case 130:
      return "Wisdom/Intelligence";
    case 131:
      return "Traps";
    case 132:
      return "Auras";
    case 133:
      return "Endurance";
    case 134:
      return "Serpent's Spine";
    case 135:
      return "Corruption";
    case 136:
      return "Learning";
    case 137:
      return "Chromatic";
    case 138:
      return "Prismatic";
    case 139:
      return "Sum: Swarm";
    case 140:
      return "Delayed";
    case 141:
      return "Temporary";
    case 142:
      return "Twincast";
    case 143:
      return "Sum: Bodyguard";
    case 144:
      return "Humanoid";
    case 145:
      return "Haste/Spell Focus";
    case 146:
      return "Timer 7";
    case 147:
      return "Timer 8";
    case 148:
      return "Timer 9";
    case 149:
      return "Timer 10";
    case 150:
      return "Timer 11";
    case 151:
      return "Timer 12";
    case 152:
      return "Hatred";
    case 153:
      return "Fast";
    case 154:
      return "Illusion: Special";
    case 155:
      return "Timer 13";
    case 156:
      return "Timer 14";
    case 157:
      return "Timer 15";
    case 158:
      return "Timer 16";
    case 159:
      return "Timer 17";
    case 160:
      return "Timer 18";
    case 161:
      return "Timer 19";
    case 162:
      return "Timer 20";
    case 163:
      return "Alaris";
    default:
      return "Unknown";
  }
}