# Changelog

Summarizes notable changes to Zeal

## [1.3.5] - 2026/01/09

### New features

* Enhanced /tag with the following updates:
  - Added new /tag command options to support routing broadcasts over a chat channel
    - `/tag chat <tag_text>`: broadcasts the tag_text to a set chat channel (like local, rsay, gsay)
    - `/tag channel <Zt123>`: Broadcast to rsay or gsay (if not in raid) an auto-join channel message
    - `/tag join <channel>`: Saves and joins channel if channel else joins previously saved channel
  - Added new filtering and display options in Nameplates Tab and additional commands below:
    - `/tag filter`: Enables routing of tag messages to the "Zeal Spam" chat filter channel
      - The "Zeal Spam" chat filter is also color option "28" in Zeal colors
    - `/tag suppress`: Enables full suppression of tag messages (requires filter on for rsay and gsay)
    - `/tag prettyprint`: Translates the message into a human friendlier format (requires filter on)
    - `/tag tooltip`: Shows tag as a target window tooltip with an alignment option
    - Added option to disable adding the default arrow (adds text only w/out requiring ^-^)
    - Added option to use '*' instead of '^' for international keyboards
  - Added two new /tag shapes with single fixed colors:
    - `^s^`: Red stop sign (octagon)
    - `^p^`: Green pet paw
  - Now allows tagging players with shapes using `^R^` commands (text is ignored)
  - Now supports showing tag and tag shape even if the nameplate text is hidden
    - Still does not work on mobs w/out nameplates
  - Modified the text coloring behavior to only override with the Tagged color if no explicit shape set
  - Added a new `/tag target <text>` that allows targeting NPCs tagged with an
    exact match for the delimited tag text (case sensitive)
    - Matches after splitting by delimiter (' | ')
    - Requires the NPC to be tab targettable to succeed
    - Targets closest NPC if multiple matches
  - Updated appending tag text to avoid duplication of exact fields and truncating at field
    boundaries. A duplicated field will show up at the end.

* Added a new /linkall option that is toggleable with '/linkall compact' (defaults to on) that will collapse identical
  items into a single link followed by a " (count)" value

* Added a new nameplate options tab checkbox to allow showing healthbars for raid members
  - Requires the Quarm server to set the new rule to share raid health updates
  - Healthbar display is now unlinked from the nameplate text visibility

* Added additional '/timestamp 3' option with a 24-hour format HH:MM::SS option.

* Added new zeal general options checkbox to enable per character storing of the chat channels
   to autojoin (stored as `"ChannelAutoJoin_<name>"` in the client ini)

*  Added per character keybind support
    - Added new zeal general options tab option to enable per character
      keybinds. When enabled, these are stored in a new eqclient.ini
      section called `"Keymaps_<name>"`.
    - There is no in-game support for duplicating keybinds.  In order to re-use,
       the contents of one Keymaps ini section can be copy and pasted manually to another.

* Added new /optchat command to optionally broadcast to raid or group without error messages
  - `/optchat <rs | gs | rsgs> <message>`

* Added a new /triggers command that supports loading a list of  triggers from a specially formatted
  text file that will match to events in the chat output stream and generate on screen
  labels with timer countdowns (see repo readme)
  - This is primitive and not a GINA / eqlogparser replacement

* Updated item display to add case 110 for ranger archery accuracy spell effects

* Added new zeal option 'Log Add to Trade' that will print a chat
  message when items or coin are added to the trade window to either
  npcs or players
  - Note: This does not apply to the world crafting stations

### Infrastructure updates and bug fixes
* Pet chat fixes:
  - Moved /pet health summary line routing from My pet channel to  default routing so it gets
    clustered with the buff report lines
  - Fixed an issue where a pet name with a space (NPC) was going to other instead of my pet

*  Did some std::string cleanup to make more parameters const refs instead of refs or copies and
   fixed two cases of debug printfs not properly using .c_str().

* Suppressed the map external data mode complaint about blank lines in map files.

* Added crash logging of nosprites state if showspelleffects is on

* Fixed the nameplate map info cache to properly flush on a despawn


## [1.3.4] - 2025/12/11

### New features

* New /spelleffects command adds some visual options:
  - /spelleffects nosprites: if enabled it disables the sprite effects on the
    180 spells using old spell effects to prevent the dpvs.dll crash when
    /showspelleffects is on (not yet 100% broadly confirmed)
    -  To enable: Execute `/spelleffects nosprites` (confirm this reports True)
         then `/showspelleffects on` 

  - /spelleffects bard: optionally switches the spell effect on the 14 bard
    songs (regen/resists) that were invisible with showspelleffects
    off from a flashy blue cone to more subtle bard effects

* New /tag command features (see readme for examples):
  - The /tag command now renders an optional arrow above tagged mobs
  - The arrow color defaults to the nameplate color but can be
    explicity set using a special `^r^` prefix in the tag_text, where
    R or r = red, O or o = orange, Y or y = yellow, G or g = green,
    B or b = blue, W or w = white, and - = delete arrow
  - The /tag command now supports a special `+` prefix to append
    text to an existing tag instead of fully replacing it
  - The /tag command now supports a special `-` prefix to erase
    any existing tag text. It can be sent by itself to delete the tag
    text and the tag arrow if it is using the default color.

* Now appending meal/drink durations in item display

### Infrastructure updates and bug fixes

* Autofire fixes:
  - Re-enabled auto-disabling of auto-attack when enabling autofire
    (fixes unintentional change in behavior in 1.3.2)
  - Fixed a bug in /autofire where it was disabling combat music when
    going directly from autofire to autoattack
  




## [1.3.3] - 2025/12/6

### New features

* Confirmation dialog box updates
  - Added a tooltip countdown timer that shows up to the right of
    a confirmation dialog (if there is a timeout)
  - Added a new Zeal general option 'Sticky dialog position' that will
    allow persistent positioning of the in-game confirmation dialogs

* Bazaar quality of life updates
  - Updated the right click on trader name in bazaar search window to
    also both target the player and add them as a map marker to make
    it easier to find traders
  - Performing a ctrl + shift + left click on a non-empty inv slot
    while the bazaar search window is open will automatically perform
    a bazaar search for that slot's item

* Added a new /tag command command that can add a text tag to the 
  top of a npc nameplate (zeal fonts mode only)
  - Supports local only or broadcasts through rsay and gsay
  - Can clear tags locally or broadcast a clear all tags
  - Must be enabled to receive broadcasts (/tag on with Zeal fonts on)
  - Tagged nameplaces have a new Zeal color: Tagged - 30
    - The tag color overrides the con color but the target color
      will override the tag color

* Added a new Zeal options Buff Click Thru that will make the
  non-button area of a locked Buff window transparent to mouse clicks
  - Unlock the window to move it around with left click
  - Use control + right click to access the context menu (to unlock)

* Zone map updates
  - Added a new map options checkbox that enables the addition of
    the current player movement speed to the map (like /loc)
  - Added a new data mode (Both - No internal POI) that will combine
    internal map line data with external files
  - Increased the number of supported external files to 10 (up to _10.txt)

* Spellset cleanups
  - Make spellset transport spell names more consistent
    - Fixed mispelled Zepyhr for skyfire
    - Changed 'the Nexus' and 'the Combines' to remove 'the '
    - Relabeled Winds of the North and South to Succor: Skyfire and
      Succor: Emerald Jungle
    - Relabeled Tishan's and Marker's relocation to Evacuate: Skyfire
      and Evaluate: Emerald Jungle
  - Recategorized 'Shroud of Undeath' from Misc to Combat Innates

### Infrastructure updates and bug fixes

* Item display cleanups
  - Fixed stun duration for spells with level caps. There is a client
    / server calculation mismatch so now using correct fixed values.
  - Fixed misspelled invisible in spell effect info
  - Now calculating the correct total ratio on Bane DMG: lines
    in item displays

* Implemented some reliability improvements for /clc
  - Fixed a collision issue when an already STML annotated substring includes a player names
  - Changes generation of modified string to try and reduce freezes during busy raids


## [1.3.2] - 2025/12/1

### New features

* Slash command updates:
  - Added /dampenlev on or off command to reduce levitation motion
      - Reduces frequency by 8x and amplitude by 10x
      - Affects only the player (not other PCs)
  - Added a /targetprevious command that acts like the toggle last two targets keybind
  - Added /autofire on and /autofire off for explicit control (vs toggle) w/more status info
  - `/zeal help` is now alphabetically sorted

* Added two new keybinds
   - Commands: PetHold: does /pet hold (AA command)
   - Target: Assist: does /assist

* Modified the character select camera field of view calculation so that 
   wide aspect ratios (1.8 to 4+) will not chop off the character name and zone
  - This is only active when zeal camera is enabled

### Infrastructure updates and bug fixes

* Fixed an indexing error in the shift + tab FindPreviousTellWnd() call that can
  cause a crash


## [1.3.1] - 2025/11/24

### Infrastructure updates and bug fixes
* Tell window cleanups
  - Fixed tell windows so using the special trigger keys for say ('),
    emote (:), and chat (;) properly work and do not send those
    as tells
  - Fixed an issue where if always chat here was set and a tell was
    sent then any data in the always chat edit box was also sent to
    the tell recipient. This is an existing client bug and in order
    to limit the changes only applied this to tells (tell windows).

* Fixed a crash when using the escape key in the old ui

* Expanded /zeal version to also report the exported version strings
  from the new versions of eqw.dll and eqgame.dll

* Fixed left mouse pan jump in full screen mode when using the
  legacy eqw.dll and eqgame.dll

* Tweaked camera fov to be more consistent
  - Fixed the fov setting to be more consistently applied when
    in game during zoning and other transitions
  - Disabled the custom fov setting during character select

* Added a utility function for listing in-use keybind codes


## [1.3.0] - 2025/11/20

The 1.3 release includes some significant refactoring of Zeal to make it more
robust across login cycles, some minor feature polishing, and a few new quality
of life features. It also supports the new eqw_takp (eqw.dll) as a replacement
for the legacy eqw.dll 2.32.

### Feature updates
* The trip from game to character select to login / desktop and relogging back
  into character select should be more reliable
  - Note: dgVoodoo has its own bug on login to char select to login w/out entering world

* Refactored the settings so they re-initialize when exiting character select for
  more consistent restoration of defaults / character specific settings
  - Note: The TellWindows enable and history settings were upgraded and renamed and
    any changes from the defaults (off) need to be redone

* Added support for the new eqw_takp eqw.dll (retains support for legacy eqw 2.32)

* Improved `/consent` with new commands and an auto-consent response option
  - `/tellconsent`, `/tc`: Sends a "Consent me" to a targeted corpse
  - `/replyconsent`, `/rc`: Does a /consent to the sender of most recent tell
  - `/autoconsent`: Toggles an enable to do a /rc in response to a /tc from
                a raid or group member (also new general option)
  - `/consentmonks`, `/consentrogues`: Consents all monks or rogues in a raid.

* Added new Abbreviated Chat (`/abc`) command and setting (defaults to off)
  - Abbreviates most user messages to "[C] [Sender]: Message" where C is a channel prefix
  - Also abbreviates `/random` messages
  - Supports off (0), abbreviating chat only (1) or chat + log (2)

* Added new `/clc` command toggle to enable wrapping player names in chat messages
  with class-color tags.
  - Uses class colors defined in raid settings
  - Looks for player names by raid member or zone entity  (also colors "You")

* New keybind will close the most recent open tell window
  - Keeps an internal history of tell window messages (in and out)

* Updated some chat filter options
  - Suppress_lifetaps option now affects both the feel better and beam smile messages
  - Updated pet message filtering to be more reliable (includes taunting,
    waiting for your order) for my vs other pet filtering
  - Added a suppress other pets option that snuffs messages from other
    player's pets (except for leader ID messages)

* New class colorization option of group window names
  - Requires EQUI_GroupWindow.xml to have Gauge1 to Gauge5 labels

* Added `/outputfile format` setting that toggles between old (0) and new (1)
  output file formats for the inventory and spellbook
  - The new format for `outputfile inventory` and `outputfile spellbook` appends
    the host suffix similarly to the UI_ .ini files
  - Separate ear, wrists, and fingers with ear1, ear2, wrist1, wrist2, finger1, finger2
  - Export either charges or stack count in the third column

* Added new `/pipeverbose` command that toggles on/off extra raid and group fields
  in the named pipe output
  - Note that raid member HPs are guaranteed to be laggy and inaccurate due to the
    delay in server updates to the client

* Zone map updates:
  - New `/map loc` command drops a marker at your current location
  - New `/map ring` heading option toggles a directional heading projection line
    as part of the map ring

### Infrastructure updates and bug fixes

* Mystats now includes the bonuses from physical enhancement and
  lightning reflex in the avoidance calculation (in addition to combat agility)

* Login cycle / character switching improvements:
  - Pinned the zeal dll into memory so it is not unpatching / repatching on the cycle from
    character select to login back to character select
  - Pushed the creation of zeal objects to a hook after the attach to avoid restrictions
  - Improved consistency of reloading settings when switching characters
  - Modified directx hooking to cleanly install and remove through login/out sequence
  - Updated a few modules to have cleaner re-initialization when switching characters/accounts

* Swapped the use of filesystem::current_path() for a new function that retrieves
  the executable file folder (with a fallback to the current_path)

* Fix Calefaction to use spellset sub-cat Fire

* Fix to `/shownames` to make it behave like the default client (any non-numeric argument
  besides 'off' = show all)

* Fix `/spellset list` to not require a dummy third argument

* Fixed floating combat damage to correctly show "damage to me" color
  when mounted

* Fixed an issue where if the keyup was missed on a slow turn key the normal
  turn key rate would be stuck at the slow rate
  - Also optimized the logic so it only modifies the turn rate when required

* Camera cleanups and polishing (callback hook location, lmb polishing, etc)


## [1.2.2] - 2025/09/20

The 1.2 release includes some significant boot stability fixes and a
number of quality of life enhancements.

### Feature updates

* Spellsets
  - Added spellset categorization for the new Druid Zephyr spells
    - Appended (EJ) and (SF) to Wizard Kunark portals (like Druid)
  - Fixed Transon's Phantasmal Protection (2539) categorization
    to Regen from HP Buffs
  - Added a new Zeal general tab options (Alt Transport Cats) that
    switches the Transport subcategories to Self, Group, Others, Area
    from the default continent based subcategories
    - The new subcats are alphabetized by name vs ordered by level

* Added an option (use /resetexp ding, /resetexp off to control)
  to play a ding sound when gaining an AA level

* Chat filter updates
  - Added a new chat filter category for routing item speech (aka 'glow' messages)
  - Added new Zeal chat filter options to route critical hits from others
    and damage shield damage from others
  - Reduced the damage shield non-melee range message to only
    broadcast when within +/- 20 z-vertical of the source to reduce
    spam in multi-level dungeons like Velks
  - Fixed routing of other 'other pet damage' to prevent re-routing
    of damage to self to that channel.
  - New zeal option checkbox to suppress the Ahh, I feel better... messages
  - Added a /mystats chat filter channel option (hardcoded white color)
  - Also added a new Zeal general options tab button to completely suppress the
    reporting of other non-melee damage (includes damage shields)

* Map updates
  - Added map support for a new /map line command that will add
    additional line segments to the map using pairs of loc coords
  - Added new map commands(/map rsay, /map gsay) to broadcast map marker
    or line commands to the rsay or gsay channels and the recipient Zeal
    will intercept a special zeal header and execute those commands

* Disabled client-sided health tick for more accurate HP changes
  - New "/clienthptick", {"/cht"} command can be used to re-enable
    client side HP ticks (but not a persistent setting)

* /follow improvements
  - When auto-follow is enabled, ZealCam will lock to the player's
    heading in chase mode
    - Left panning is allowed with the mouse down then snaps back
  - Player pitch will now track the leader's position when zeal
    auto-follow is enabled (for use when following with lev)

* Added a /locktogglebag command and linked Zeal general options
  tab combobox that can select a single inventory bag slot
  to lock open (ignores the toggle close)

* Improved/fixed spell descriptions in spell/item info Window
  - Shows effective casting level more correctly for various items/spells, including AAs modifiers.
  - More accurately calculates values directly from spell formulas.
  - Generates/overrides some spell effect text that was missing, confusing, or exported inaccurately.
  - Shows dynamic value and value ranges for modifiable bard song effects.
    - Shows bard-modified dynamic value for buffs (beneficial only)

* Mounts
  - Slow Turn now works on mounts (slow keybinds now match the unmounted behavior)
  - Fixed visibility of the zeal font nameplates of other mounted players
    (and also F7 targeting of them)

* Added /leftclickcon <on | off> command that enables generating
  a consider message when left clicking on a npc (like right click)
  - Also did a minor clean up of the Zeal general options tab

* Added a new "My big hits" Zeal color (41) for setting the color
  of big hits (spells or melee above the threshold plus all backstabs) in
  floating combat text

* Added four new checkboxes to the Zeal options tab for enabling/disabling
  camera views in the F9 Toggle Camera keybind cycle loop
  - Checkboxes allow disabling all views except first person

* Fix horizontal sensitivity of Zeal cam when left and right mouse buttons were
  held simultaneously (long-standing behavior had effectively doubled it)

* Restored Old Stone UI basic support (see fixes below)

* Initial "big fonts" 4k mode support
  - Primarily designed for running at native 4k resolution
  - The larger fonts requires the use of custom 2x upscaled ui skins
  - The "big fonts" mode is enabled if the active ui skin folder
    contains a file named 'zeal_ui_skin.ini' in it (can be empty)
    - Note: Big fonts are activated at boot and requires a client
      re-start to transition
  - To switch modes:
    - Use `load <skin> 0` to select the new UI skin
    - Quit and restart client
    - Run `load <skin> 0` again to reset your ini file sizes

* Usability
  - Added Zeal blocking of the /load skin if the selected skin
    folder doesn't exist or the skin has contents known to be
    incompatible with Zeal or the server xml files

* Improved UI error messaging to reduce configuration confusion
  - New UI dialog that that makes UI errors more obvious
  - Can be disabled through either the new command
   '/show_ui_errors <on | off>' command or using the Cancel
    button on the dialogs
  - Reduced chances for installation confusion by adding explicit 
    whitelist for including zeal xml files w/better messaging

* Limit autofire spam
  - Added message `123 You can't hit them from here.` to the rate limiting
    message list in autofire
  - Added the 'You are stunned!' message to the rate limiting
    filter in autofire

### Infrastructure

* Fix to a memory allocation in hook wrapper (boot heap corruption fix)

* Refactored the dll to eliminate the separate Zeal lifetime thread and
  instantiate Zeal on the primary calling attach thread

* Refactored headers to reduce compile time and make dependencies more obvious
  - Eliminated catch-all "framework.h" and added module specific headers

* Refactored the ZealService initialization sequence to try and fix the
  persistent boot heap corruption check failures
  - Simplified core classes by removing dependencies on
    later initialized modules
  - Reviewed and updated the module initialization sequence
    in zeal.cpp to minimize the risk of a call to an
    uninitialized / partially initialized object
  - Moved the named pipe instantiation to the end so that
    thread spawn happens after everything else is initialized

* Cleaned up hook_wrapper classes with no raw mallocs, smart
  unique pointers for memory management, encapsulation of internal
  details, fixes to some jump calcs, and ensured that the
  trampoline and original bytes were valid for all paths

* Added a `generate_big_xml.py` to handle 4k auto-upscaling of Zeal UI XML files

* New UISkin class to centralize zeal file resource access and handles big fonts
  - Zeal now uses a hard-coded whitelist of required xml files and no longer tries
    to load other random xmls from the zeal/ folder

* Added changes to help reduce UI installation / configuration confusion
  - Reduce 'normal' errors to 'Info' (fallback to default, song buffs)
  - Filter out common UI mods (optimized bags, tracking window) from error dialogs
  - Added the error dialogs with a cancel button to disable the /uierrors setting
  - Added an XML error count field to the crash handler dialog box

* Updated xml and resource file path accesses to use std::filesystem::path

* Changed WDT_Def2 to WDT_Def in two Zeal xml files to reduce zeal xml
  template dependencies

* Refactored GameClass and Display classes to add methods and expand fields

- Also made it so `/zeal check` just does the memory check and
  reports current allocations in MB vs hex
 
* Old (Stone) UI fixes
  - Fixed an issue with the attack keybind / button that was introduced
    when explore mode support was added
  - Fixed an issue with enabling the Zeal camera introduced with 1.1
  - Fixed an old off by one bug in floating combat damage that happened
    when textures failed to load in the old ui
    - Changed the default for fcd to disabled like target ring
  - Disabled modules that heavily rely on the new ui to function
    (like melody, tell windows, item_display, ui_manager)
  - Scanned through and tried to protect against inactive new_ui
    objects
  - Functional: Zeal camera, nameplates (mostly), target ring (no
    textures), fcd (zeal fonts only), etc

## [1.1.0] - 2025/08/11

### New Features:

* Luclin horse support
  - ZealCam: Now functional in first and third person
  - Self-click thru: Now also applies to your mount
  - Spellsets: Now functional when mounted
  - /camp: Performs an auto-dismount to allow sitting and camping
  - Fix to Type 134 (spell casting name) when mounted
  - Target nearest and cycle targets now excludes mounts (horses)
  - Fixes Zeal font nameplates disappearing when mounted

* ZealCam
  - Refactored to support mounts and to slightly polish behavior

* New option (Enhanced auto-run) that tweaks the auto-run keybind behavior
  - Instead of toggling state, a key press will lock on if the forward
    key is already pressed. Also will lock strafe key.
  - Also made disabling auto-run/strafe more reliable  

* UI labels:
  - Gauge type 35: AA Exp Per Hour
  - Label type 86: AA Exp Per Hour Percent

* Experience per hour:
  - Refactored to support AAs
  - Changed calculation so it does not reset on zone (use /resetexp if desired)

* Nameplate updates:
  - Adds UI (Zeal nameplate options tab) combo boxes for selecting shownames mode
  - Adds UI combo box for optionally choosing a specific AA title (local display only)


## [1.0.0] - 2025/08/02

### New Features:

* Luclin era enabled for /mystats
* Focus effects should now show up in item info windows
* Using /rt will now also target the player in the raid window (if active)
  - **Description:** targets the last tell or active tell window player, also selects the player in your raid window
* New crash dialog with additional information for screen capturing the summary info
  - The send crash reporter exe has been removed.

## Fixes and infrastructure

* Spellsets were refactored to eliminate a rare right click context menu issue
* Fixed return signature of GetClickedActor() (used by self click thru)
* Source code house keeping (formatting, renaming)

## Known issues

* Mounts (horses) have multiple issues with ZealCam, spellsets, targeting, and more.


## [0.6.8] - 2025/06/02

### New Features:

* New optional enhanced spell info (zeal general options tab)
  - Uses spell info scraped from Icestorm's yaqds.cc site
  - Replaces the Spell display window with a new text blob
  - Appends new info text to spell scroll item display windows
  - Appends item effects (procs, clickies, worn)
  - Appends spell info for buffs (caster level is not shown)
  - Added support for item focus effects (which will become visible
    once the server enables them)

* New optional enhanced /follow (options tab, command line args):
  - Disables rapid toggling of run mode to reduce LD / crashes
  - Skips slow turning to reduce follow failures
  - Also supports an adjustable follow distance (command line only)
  - Hat tip to Solar for providing the patches

* Optional invite dialog now has accept / decline buttons and will
  now auto-dismiss when the player has accepted or declined the invites

* New option to require ctrl-key press to pop-up UI context menus:
  - Added another zeal general option that enables the requirement
    of holding down the ctrl key while right clicking on a ui
    window in order to pop up the default window context menus
    (to avoid inadvertent menu pop-ups)

* Added new keybinds options for /pet health and /loot

* Added /melody resume support
  - The `/melody resume` command will resume the last active melody's
    song list (gem index based) starting at the gem index of the
    interrupted song.
  - Supports macros like /stopsong, /cast 6, /melody resume

* Added option (Slash not poke) to use 2hs animation for 2hb

* Dropping or destroying cursor held items is now logged, including
  listing the contents of bags

* NPC (non-pet) damage from damage shields is now logged
  - Shows up as "<source> hit <npc> for X points of non-melee damage."

* Added support for /shownames options 5, 6, 7 in both the zeal font generator and the client text
  - Requires extended shownames to be enabled
  - Added override to /shownames to display the new options and print out the selected display options

* Added /singleclick support for non-stackable items (like smithing
  tools). Works the same as stackable items (shift, ctrl, target).

* UI labels:
  - Label 85 now reports the (total number - number of empty) slots
  - Also made both label 83 (num empty) and label 85 default as
    "greyer" white (like wt) and then turn yellow at 1 empty slot
    and red when 0 empty slots

* Floating combat damage (FCD):
  - Added two new options checkboxes for filtering out FCD:
    - Npcs: toggles filtering of NPC damage
    - Hp updates: toggles filtering of the HP updates
  - Also adjusted the pet filtering. The player's pet melee
    damage is now treated like the player's melee damage
    (filtering, colors). The show pets filter will filter out
    all other pets (player or npcs).

* Added filter to /mystats melee report to skip the slot if
  the weapon isn't equippable in that slot

* Modified the target hp percent label to be blank instead of
    showing "0" when there is no target or targeting a corpse

# Infrastructure / bug fixes:

* Fix auto-z fade for mischiefplane (was defaulting to outdoor zone w/no auto-fade)
  - Can override with /map level autoz <#>

* Fixed the InputDialog (save spell sets, invites) so the position is properly saved 

* Fixed the camera yaw modf calculation to wrap within 0 to 512, which
  might fix the intermittent tracking garbage and you can not see your target

* Changed the /protect file format's delimiter to a '^' from a ','
    to avoid a collision with some item names that include a comma

* Fixed the missing dynamic Zone Select button on the default
  ui for character select if the zeal.ini does not have a value
  already set for the zone index (no longer skips the Show() call)

* Fixed client bugs that generate inaccurate attack delay timer values
  - Primary weapons with ItemTypeMartial (0x2d) are not handled,
    resulting in 0 attack delay and the melee/range buttons never
    showing the actual lockout (like Primal Velium Fist Wraps)
  - It uses the fixed 3500 ms skilldict value for hand to hand
    delay and does not calculate the correct value for higher level
    mnks and bsts or for the monk epic weapon
  - These bugs also impacted the new UI attack recovery timer gauge

* Fixed monk and bst hand2hand delay in recovery gauge
  - Now uses the accurate /mystats delay calc plus the client bug fixes above

* General code cleanup:
  - Cleaned up constructors using ini (replaced with zeal settings).
  - Created template and macros to do a memory check with default constructors.
  - Added operator_overloads.h to add bitmasking for enum class style enums
    so we don't have to worry about name collisions.


## [0.6.7] - 2025/04/26

### New features:

* Added /mystats command
  - New /mystats command provides a breakdown of the components of AC
   (mitigation and avoidance) and ATK (offense and to hit) of currently
   equipped gear melee weapons w/current buffs
  - Supports /mystats <item_link> to report stats as if player was
    holding the weapon

* Added option to require holding the ctrl key to trigger right click loot
  - Controlled by either /lootctrl or a Zeal settings general option
    ('Ctrl Right Click Loot')
  - Useful to prevent inadvertent loot trigger in third person view

* Added item display when alt-left clicking on character inspect window
  - Note: This uses an item name lookup to find the items out of a slightly
    old db list, so a few items might be missing and some items with name
    collisions (like Blue Diamond Electrum Earring) may show the wrong item

* Add an optional invite (raid/group) dialog box (#342)
  - Zeal general options now has an additional Invite Dialog checkbox option
   that will enable a pop up dialog upon a raid or group invite
  - Also extended the optional invite notification sound to also
    play when invited to a raid (previously group only)

* Map
  - Added `on` and `off` arguments to `/map ring` for explicit control
    - `on` works only for tracking classes and sets to max tracking range

* Labels
  - Added Type labels for reporting the # of open slots (83) and
    the total # of inventory slots (84)


### Fixes / infrastructure:
* Fixed a bug first introduced around v0.5.5 that could cause a crash when
  trying to remove the temporary UI_Zeal.xml file during UI initialization
     - Hat tip to Fatrat for isolating the area with the problem 

* Cleaned up README.md (audited labels, commands, keybinds)

## [0.6.6] - 2025/04/05

### New features:
*  Character select screen now allows zone selection and explore mode
   - Selected zone becomes the future char select background
   - Adds a dynamic Zeal_ZoneSelect button that will appear if the
     UI_CharacterSelect.xml lacks it
   - Custom skins can control the button location and appearance by adding the button
   - The uifiles/zeal/optional has an xml that can be copied to the uifiles/default folder
     to control it in the default screen (which always appears at first startup)
   - Known issues w/explore:
      - Applying a new zone, then exploring, then exiting has issues
      - No jumping and a few other similar UI limitations

* Added options to play sounds upon group invite or a new incoming tell
  - Controlled by combobox Zeal general options.
  - Setting to None disables (default).

* Nameplate enhancements:
  - Added a 'Guild LFG' nameplate color
  - Added a 'PVP Ally' nameplate color (applies to raid, group, and guild members in PVP)

* New [/survey command](https://github.com/iamclint/Zeal?tab=readme-ov-file#polling-of-raid-using-survey) for polling raid group with yes/no questions

* Existing command enhancements:
  - Added `/protect cursor`: toggles protection of cursor item
  - Added `/protect worn`: enables protection of all equipped items
  - Added `/hidecorpse showlast`: unhides the last hidden corpse
  - Added `/hidecorpse npc`: hides all existing NPC corpses (excludes players)

* `/linkall` improvements
  - Now sorted alphabetically
  - Supports displaying more than 10 items
    - Active chat window: Add active links until there are 10
      and then it will just add remaining item as text names
    - `/linkall <channel>`: Split the item links across
      multiple channel messages to stay below the 10 link limit.

* Map
  - Corrected out of area map data for arena (hat tip to Talodar)
  - Added iceclad2 map and default Brewall maps for future zones

* Added weapon (ratio) to item display

* New keybinds:
  - Added a new UI hotkey option (244) that duplicates clicking on the buy or sell button
    with the shift key held down
  - Added a new Keyboard->Chat keybind that will deactivate all visible tell windows.
    Note that if history is enabled, unnoticed messages could be dropped. Sending a new
    tell to the person or /r will pop back up the tell window (with history if enabled).

* Added new gauge type 34 (attack recovery timer)
  - New gauge provides a countdown gauge / string tracking the
    attack (range/melee) recovery countdown

* /outputfile inventory support for expanded bank slots

* Added option checkboxes to enable linking the visibility of the
  target ring or the floating combat damage with the F10 UI
  visibility toggle ("Hide with Gui")
  - Fixed an issue with FCD where the spell icons remained visible
    but the text were hidden with the GUI

* New fps limiter option in Zeal General options

* Added the resurrection dialog message to chat and log to support 
  external triggers

* Changes to boot heap corruption check
  - Made the checks more conservative (multiple check failures required)
  - Added a speculative crit section wrapper
  - Added dialog options to retry and ignore so user can bypass

### Fixes / infrastructure:
  - Fix a possible nullptr crash in singleclick when accessing a world container
  - Removed unused build Visual Studio project build configurations
  - Add better detection of and handling for infinite crash loops
  - Added additional camera view to the enum since character select already uses type 5


## [0.6.5] - 2025/02/27

### New features:
* Additional chat filter and color options for melee specials
  - backstabs, kicks, strikes

* New /run command that controls run versus walk mode
    - /run (toggles), /run on, /run off (walk)

* New zeal general tab option to detect /assist failures
  - Clears current target and emits warning

* New zeal general tab option to suppress fizzles messages from non-grouped casters

* New /uilock command that supports on and off toggling
  of the UI Lock state for primary game windows
  - Bags must be open to take effect

* New /lootlast command that specifies an item ID (either
  by a direct number or using an item link) that will be looted
  last during /lootall of your own corpse (and thus not looted
  since /lootall leaves an item on your own corpse)

* Extended /protect to also cover NPC / pet trades
   - /protect on now blocks all trades to bankers (money, items)
   - Item and non-empty bag protection now applies to trades to NPCs
     and pets (value is not checked)

* New nameplate options to enable mana and stamina bars
  - Self-only for now until server provides more information to client
  - Also added another sample font: segoeui_bold_24

* Map:
  - Added the short and long zone names to the internal and external window title bars
  - Add map option to show raid member headings (versus triangles)

* Added a new spell recast timer option in zeal general tab that adds buff timer-like
  tooltip countdowns for each spell gem
  - Similar text-only information to Types 26-33 above but should not require xml updates
  - Has extra option to left align the timer display instead of default location

* New UI Gauges to support server tick timer and spell cast recovery times (requires XML updates)
  - Added a new gauge (Type 24) that shows a server synced 6 second timer tick gauge
  - Added a new gauge (Type 25) that shows the global cooldown (recovery) after casting
  - Added new gauges (Types 26-33) for each spell gem slot that show the recast countdown time

Note: UISkin authors, if you want to hide the gauge text number, make it go offscreen
with something like:
```
<TextOffsetX>-500</TextOffsetX>
<TextOffsetY>-500</TextOffsetY>
```

* Reduced the nameplate drop shadow offset for sharper text
  -  Added a /nameplate shadowfactor <float> to allow users to adjust the offset

* Added a new /singleclick command and linked zeal general option that toggles the
 single click automatic transfer of stacked items to an open give, trade, or crafting
 station container window as unstacked items
  - If ctrl+left click, transfers 1 item from stack over
  - If shift+left click, transfers entire stack
  - To simplify things for now, singleclick is a no-op on nodrop item stacks
  - Note: Singleclick transfers to other players is disabled until more testing
    is done (npcs still work for quests)
  - Singleclick transfer to inventory tradeskill bags requires them to be explicitly
    targeted with a /singleclick bag # command.  This is a non-persistent setting.
    0 disables, bags 1-8. Intended for use only during intensive tradeskilling sessions.

* Support auto-sit and automatic inventory / spell export across all camp pathways
  - The camp button did not support auto-sit or exports.  Binds did not support export.
  - Hook the common camp call so that /camp, keybind, button, and hotkeys all support
    both auto-sit and exports

* ItemDisplay windows updated to report the required level for clickies


## Fixes and infrastructure
* Patches to song window to support the updated game.dll
  - Add Song Window support to ui_buff
  - Added Song Window label range to labels.cpp to avoid future conflicts

* Added instruction cache flushes when modifying code

* Fix: Prevent /sit while in loss of control

* Fix: Designed out potential memory leaks in the CXSTR (client string) handling
  - The refactoring cleanup should make it "harder" to leak in the future,

* Fix: The /log off command has not been working properly since the print_chat
  hook for timestamping was added. Setting /log off will now disable logging.


## [0.6.4] - 2025/02/04

### New features:
Nameplates
* Add custom font supports to the nameplates
  - Controlled through nameplate options tab or /nameplate zealfont to toggle
  - Can use any zeal installed fonts (added more fonts with reasonable sizes)
  - Added drop shadow support, bottom align for centered text
* Enabled the target auto-attack blinking indicator. Synchronized
  it with the targetring and uses the targetring options slider for rate.
  - Added target blink always and only during auto-attack options
* Added option to show pet owner names as second line to nameplate ("Pet" for self-pet)
* Adds an option to display a health bar at the bottom of visible nameplates (custom font only)
  - Due to client data limitations, only applies to self, group, pet, target

Targetring:
* Add target_color option that uses target color instead of con color
* Add option to disable self target ring

Map:
* Added a small dull yellow position marker for self-pet if show group members is enabled
* Added a new options checkbox that enables text with your current location in the upper left corner
* Map supports loading zones not in the linked in database
  - Adds /map world command to lookup zone names

Floating combat damage:
* Added filters for suppressing damage from self, from pets, from other players, and from melee
* Switched to using the HPUpdate packets to report heal events which is mostly functional.
  - Note that self heals effectively bypass this so don't show up
* Added a "big hit" slider to make some damage outputs persist longer and stronger (values above threshold)
* Added FCD specific color settings in the zeal colors tab for controlling the FCD colors

Tab completion:
* Restored native client behavior to flush text beyond /tell and enable tab cycle list
* The recent tells list is always added to the cycle list at the end after any partial tab completion matches
* To simplify the logic, tab completion will not work in a tell window if there is more than one open.
  - The tab cycles between the tell windows. The main chat window still has tab completion.
* Added support for tab completion of /commands. Works like bash with first tab filling out
  common prefix and second tabs supporting cycling through match list.

New /protect command:
* Provides secondary protection against accidental item loss by blocking destroying or dropping
  items if a non-empty container, value is above a threshold, or the item is on a protection list.
* Also provides protection from selling items on the block list (but no value check for selling)
  - See readme for usage notes

General:
* Add /selfclickthru command and option ('u' to open doors works in 3rd person)
* Added a zeal general tab option to enable container locking
  (context menu popup will allow toggling "Lock")
* Added an optional quiet parameter (/useitem # quiet) that
  will suppress warnings if the slot doesn't have a click effect
  (empty slot or no valid click). Still complains if invalid
  command (slot #) or blocked due to casting.
* Clicking on the skills window name or value columns will now
  toggle ascending or descending order
* Add option to export inventory and spellbook on /camp

### Bug fixes and infrastructure:
* Added simple heap checks at zeal construction (boot) that will generate a modal
  dialog box if any corruption detected
* Added showspelleffects state to the reasonsfile
* Added a /zeal get_command utility that will retrieve the command info struct
  including callback function address
* Refactored BitmapFont int a virtual base class to support both a 2D transformed vertex
  class and a 3D class that supports the z-buffer with occlusion and range
* Migrate some of the game.dll features into Zeal
  - Buff timers, extended nameplates, brown skeletons, auto-stand, combat music
* RightClickToEquip now directly calls InvSlot functions rather than InvSlotWnd x/y clicks


## [0.6.3] - 2025/01/18 (ac962b2)

### New features:
General:
* Implements proper /showhelm functionality and will fix future issues with velious helmets
  * showhelm will now affect how your helmet looks to you and how others see you
* Refactored nameplate code to eliminate delay of the health / marker text when switching targets
  * Modified target marker from < > to >> <<
  * Added legacy inline guild option
* Added a * suffix to the group leader in the party window
* Zeal chatfilters moved to a contextmenu submenu
  * Added a /who filter to the chat filter list
* Added a /linkall command with arguments to directly insert item links into chat channels
* Added enhanced player name tab completion to /tell, /t, and /consent (tab or shift-tab to cycle)
  * Supports partial name search that pulls from tell history, raid, and zone
* Added a cycle nearest PC corpses keybind option
* Skills window now sorts by skill name or value when column header clicked
* Alt + Left-click item display windows now persist their locations independently

Maps:
* Updated auto z-level to a default height value of 10
* Added a default to z autofade checkbox option in the map options tab
* Added a command line override for the auto height (/map level autoz <height>)
* Reduced the allowed minimum position and marker icon sizes by 4x

### Bug fixes and infrastructure:
* Fixed a chat issue where the first word could trigger a command (ie, output = /output)
* Fixed an issue with persisting textures and fonts for directories with more than 20 files
* Minor namedpipe cleanups to reduce overhead and for thread safety
* Increase logging for external map createdevice failures


## [0.6.2] - 2024/12/28 (ac95b4e)

### New features:
Map:
* Updated the map interactive mode so that the map window is similar to other game windows (mouse sizing, positioning, minimize, close, lock, context menu)
* Removed the position and size sliders in the options tab
  - Added a persistent enable for interactive mode in map settings
* Added a default zoom selection option in map settings
* Modified external map to use standard window resizing instead of options slider (so independent of internal size)

Spellsets:
* Fixed some corner issues where the spellbookwnd state could get locked up (like trying to memorize a higher level spell after a death)
* Loading continues if it has trouble with a spell (unless corrupted ID) and skips the spell instead of spending the time to memorize then failing
* Loading can be interrupted to switch to a different set
* Loads from top to bottom
* Avoids some corner cases of stance dancing when terminating
* Added basic spellset name length constraints (1 to 32 chars)

ItemDisplay:
* Split the alt + left-click from the right click temporary windows for consistent behavior
* Supports up to 5 persistent items or songs
  - This works for item links, inventory, and the casting gem bar and spellbook
* The right click long hold uses the default Item Display window
* Simplified and centralized the item description mods
* Added some initial spell info fields (like resists, target type)
* Fixed an issue with bank and merchant window z-layers when displaying items
* Combat effect (proc level) added
* Add class levels to spell scroll info item display

Floating combat:
* Added bitmap_font support to floating damage
* If font set, uses it, else falls back to client font size
* Added options combobox support for the fonts
* Updated bitmap_font and fonts
* Added larger arial font sizes 20, 24, 32

Melody:
* Changed from empirical delay to a server message interlock before advancing songs (should increase reliability)
* Make /stopsong more reliable by using an internal function that checks if the bard is singing a bard song

Right-click to equip:
* New option to enable right click to swap equipment from a bag to equip slot
  - From inside a bag only to avoid clicky collision

Misc:
* Add transparency ring slider for Target Ring
* Fix to gemicon default ui fallback path
* Possible fix to random color picker window bug by clearing ui_manager's button states
* Various other UI fixes that may improve stability after character select swaps
* Fixed /camp autosit to not sit if command will fail due to transaction windows
* Added /lootall command

### Bug fixes and infrastructure:
* Fixed a target ring ini issue for None transparency
* Fixed false haste on items with other (worn) effects


## [0.6.1] - 2024/12/08 (4aa4bb7)

### Bug fixes:
* Fixes map crash due to uninitialized font when no labels are active
* Fixes %t chat crash and makes the dopercentconvert calls more reliable


## [0.6.0] - 2024/11/30 (6de33da)

### New features:
* Separated the Load and Delete of spellsets so that Load is a single click
  while Delete is hidden behind a subcategory menu
* Map ring now supports indoor tracking distances

### Bug fixes and infrastructure:
* Fix StringSprite crash (skellies)
* Add a redundant Zeal load check to dllmain
* Adds trim_name call to target nameplate (deal with rare crash)
* Move texture release above the filename length check so "none" will work
* Gargoyle Nameplate fix


## [0.5.9] - 2024/11/24 (ffbfccd)  

### New features:
Map:
* Add map interactive mouse support (drag, pan zoom)
* Add /map show_zone mode to browse other zones
* Add /map ring to show tracking and distances
* Add map z-level fading and auto z-level mode
* Add succor to all zones using zone data
* Change keybind to toggle vs flash raid/group names

Nameplates:
* Major changes to support color customization and new features like target marker, health, etc
 
General:
* Chat filters & custom color updates for pets
* Added /corpsedrag nearest : auto-targets the closest corpse for /drag
* Added /corpsedrop all: drop all corpses
* Updated /lead command updated to report raid and group leader information
* Add instrument mod percent to item display
* Add slot #'s to /useitem
* Camera option to unlock for key turning
* Target Ring -- cone (depth) and indicator flash delay settings
* Floating Combat -- Spell and Spell Icon toggles
* Add /cls and /ss aliases add spell icons to floating combat add non-melee damage from other players to chat
* make tellwindow history color gray, add toggle for tell window history

### Bug fixes and infrastructure:
* Moved majority of zeal settings to separate zeal.ini file
* Construct ZealService on loader thread (boot stability)
* Few patches to improve boot stability (null SpawnInfo)
* Warnings about duplicate xmls
* ZealSetting improvements
* Use client's IDArray for ID to entity LUT
* Patch client to properly Type other PC corpses
* Restrict /useitem to match server side effect list
* Misc others


## [0.5.5] - 2024/11/02 (fa5ec26)

### New features:

General:
* Add /lead command to print group leader
* Options window updates
* add cut to edit windows, fix issue with large pastes
* Add item value to item display
* Disable character selection rotation by default
* Nameplate color updates (options, naming)

Map:
* Add multiple map markers with labels support
* Add map support for showing group and raid names with adjustable length
* Add a simple map grid option
* Upgraded to use custom bitmapfont (eliminates text label fps penalty)
* Add a 'marker_only' map labels mode

### Bug fixes and infrastructure:
* Added checks for if a group/raid exists before trying to send data via Pipe
* Fix Con Color initialization ui_options.cpp
* Fix infinite loop lockup caused by print_chat_wnd not calling the trampoline properly
* Add item link limit to pasting to solve another crash issue.
* Fix bluecon everywhere to use the color index 14
* Fix print chat buffer, attempt to make DoPercentConvert safer (some crashes reported when
 using assist macros may be related). 
* Add directx BitmapFont to accelerate text
* Prevent /useitem from trying to cast "(worn)" effects


## [0.5.0] - 2024/10/10 (24a2cc9)

### Notes:
Release notes
* UI.XML is no longer needed inside the zeal folder, please delete the entire uifiles/zeal before extracting the new zip
* If you do not see a zeal options window when you open your options window something is extracted wrong.
* Extract all of the files to your game folder from the zip in the paths they are predefined to go into.

### New features:
* Target ring updates
* Map: Add external window support
* Add group & raid positions to named pipe output
* Nameplate color updates

### Bug fixes and infrastructure:
* Fix entity remove (name is not reliable as a hash key)
* Separate tabs into own xml files, Add color selections for nameplate Add color saving/loading
* Separate color tab init into own function
* Add button callbacks for ui
* Add wnd notification hook for colorpicker callback system
* Add colorpicker wnd virtual calls activate and setcolor

