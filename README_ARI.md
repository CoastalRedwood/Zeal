# Standalone Zeal 1.4.8: Auto Raid Rejoin (ARI)

**Commands (in EverQuest)**

- `/ari rejoin on` — enable tracking and snapshot the next full raid roster.
- `/ari rejoin off` — disable and discard saved assignments.
- `/ari` — show current setting/usage.

This is implemented inside Zeal; **no Book_Keeper bot, overlay, token, named-pipe extension or external app is required.** The normal Zeal named pipe is not modified.

## Behaviour

- OFF by default. When turned on, ARI remembers each currently grouped raid member's group.
- Every second while in-game, Zeal watches raid membership in its own existing client data.
- A member's saved group is updated when a **different valid numbered group is observed in two consecutive snapshots**. An intermediate ungrouped state does not overwrite the saved assignment.
- If a previously known member disappears and later returns ungrouped or in a different group, a raid leader's client issues Zeal's normal `#raidmove PlayerName GroupNumber` internally, **without emulating keyboard input or changing the chat edit box**.
- No moves are sent by non-leaders; no moves are sent to groups with six players. Each return is attempted at most twice, with a 15-second pause between attempts.
- A genuinely new raid ID resets previous assignments; a raid roster missing for 30 seconds also resets them. Toggling OFF clears memory.
- Names must match ordinary letter-only EQ character names. The raid leader must be in the raid and have the correct server-side permission to use raid move commands.

## Important limitations

- **Experimental / not live-tested with Quarm**. The portable state logic is unit-tested, but the `Zeal.asi` has **not been compiled or game-tested** in this environment.
- Saved assignments are held **in memory only**; they do not survive restarting Zeal/Quarm. Turn `/ari rejoin on` on again to snapshot the current raid after reconnecting.
- ARI detects *leaving the roster and later reappearing*. If Quarm leaves an offline player in the roster without any observable change, it cannot reliably distinguish their disconnect from normal presence, so no restoration occurs.
- It does not learn deliberate long-term ungrouping as a new destination. This avoids forgetting a group during a brief ungrouped transition.
- Server permission must permit `#raidmove`. This is a **modified Zeal build**, not the approved standard release: request Project Quarm approval before using it online.
- Back up the installed original Zeal.asi before replacing it. Do not install alongside the original.

## Building the full Zeal.asi

### Visual Studio 2022

1. Install Visual Studio 2022 with **Desktop development with C++** and the **x86 MSVC v143 toolchain**.
2. Open a Visual Studio **Developer Command Prompt** and run `build_ari_windows.cmd` in this directory (or open `Zeal.sln` and build **Release / x86**).
3. Output is `Release\\Zeal.asi` (a **32-bit** native plugin), not a Windows EXE.

### GitHub Actions (optional)

Push this directory into a repository you control, then start **Actions → Build Zeal ARI (experimental) → Run workflow**. Download the `Zeal-1.4.8-ARI-experimental` artifact; it contains the compiled `Zeal.asi` if the Windows build succeeds.

## Portable logic test

`g++ -std=c++20 -Wall -Wextra -pedantic tests/ari_smoke.cpp -o ari_smoke && ./ari_smoke`

This verifies move tracking, restoration, leadership, full-group protection, off switch and raid identity reset; it is not a substitute for testing in the actual client.

## Files modified compared with the upstream Zeal 1.4.8 archive

- `Zeal/raid.cpp`, `Zeal/raid.h` (wiring/commands/callback)
- `Zeal/raid_autorejoin.h` (portable state machine)
- `Zeal/Zeal.vcxproj` (header registration)

No files from the earlier Book_Keeper-Zeal command bridge are used.
