# Zeal Raid Management — Automatic Group Restoration

An optional, standalone Zeal enhancement that remembers raid-group assignments and restores members to their last confirmed group after they leave and return.

## Commands

| In-game command | Effect |
| --- | --- |
| `/raidmanagement on` | Enable ARI and capture the current raid-group arrangement. |
| `/raidmanagement off` | Disable ARI and clear the saved raid arrangement. |
| `/raidmanagement` | Display current status and usage. |

Raid management starts **off**. It is part of Zeal itself and does not require an external program, account, additional process, or network service.

## Behaviour

- Observes the existing Zeal raid roster approximately once per second.
- Remembers the latest confirmed raid group for every recognised player.
- Learns manual moves after seeing the new group in two consecutive roster checks; transient ungrouped states do not overwrite saved groups.
- Remembers each member's group if they leave the raid roster.
- When a remembered member rejoins in the wrong group or ungrouped, the raid leader's client may request a return to their saved group using Zeal's existing `#raidmove` functionality.
- Checks raid leadership and destination capacity before attempting a restoration.
- Limits restoration attempts to two, at least 15 seconds apart.
- Resets its data on a new raid identity, after 30 seconds without a raid roster, or when switched off.
- Does not simulate keyboard input or change the in-game chat entry field.

## Limitations

- **Experimental feature**: the modified `Zeal.asi` has compiled successfully using GitHub Actions against Zeal 1.4.8, but this does not establish compatibility with current upstream `main` or validate every live raid scenario.
- Saved group assignments are held in memory; restarting EverQuest or Zeal clears them.
- A disconnect is only detectable when it causes the player to disappear from the client raid roster. If an offline player remains listed without a distinguishable state change, ARI cannot detect the disconnect.
- Intentionally ungrouping someone does not change their remembered destination group; only moves to another numbered group are learned.
- Commands still require the server to permit the raid leader to move members.
- This is a modified client extension. Obtain the game server's authorization before using it online.

## Development and pull request compatibility

This feature was implemented and compiled against **Zeal 1.4.8**. A successful tag-based build does not prove it will compile against the upstream `main` branch. Maintainers should build and test the changes against the current upstream `main` branch before merging.

The existing `/ari` command remains unchanged by this feature.

## Add raid management to a Zeal fork

Use a feature branch based on the appropriate Zeal version; the existing GitHub workflow specifically builds Zeal 1.4.8.

1. Open your local copy of the fork and switch to a branch based on the matching Zeal version.
2. Extract the patch ZIP. Its directories start at the repository root (`Zeal/`, `tests/`, `.github/`).
3. Copy the extracted files **into the repository root**, retaining their folder paths and allowing the matching files to be replaced.
4. Commit and push the modified files to a branch of your fork.
5. Make sure the ARI build workflow exists in your fork's default branch if you want to trigger it manually from GitHub Actions.

The patch contains only the source files changed or added for ARI; do not upload a second, nested `Zeal-1.4.8` directory within your fork.

## Build a Windows Zeal.asi

The **full-source ZIP** includes `Zeal.sln` and all original source files. The **patch ZIP** requires the matching base Zeal 1.4.8 repository files.

### Visual Studio 2022

1. Install **Desktop development with C++**, **MSVC v143 x86 build tools**, and a Windows SDK.
2. Open a Visual Studio 2022 Developer Command Prompt.
3. From the repository root run `build_ari_windows.cmd`, or open `Zeal.sln` and build **Release / x86**.
4. If successful, the expected output is `Release\Zeal.asi`, a 32-bit Windows client extension.

### GitHub Actions

1. Push the code to your fork.
2. Open **Actions → Build Zeal ARI (experimental) → Run workflow**.
3. Select the branch with ARI and run the workflow.
4. After a successful run, download the **Zeal-1.4.8-ARI-experimental** artifact.

A new manual workflow generally needs to exist on the fork's default branch to be selectable in GitHub's Actions page.

## Run the portable tests

On a development computer with a C++20 compiler:

```sh
g++ -std=c++20 -Wall -Wextra -pedantic tests/ari_smoke.cpp -o ari_smoke
./ari_smoke
```

Tests validate the raid-group tracking and move-request decisions. They cannot prove compatibility with the live EverQuest client.

## Raid-management-specific files

- `Zeal/raid_autorejoin.h` — internal raid roster and remembered-group state machine
- `Zeal/raid.cpp` and `Zeal/raid.h` — Zeal command and raid callback integration
- `Zeal/Zeal.vcxproj` — registers the new header in the Visual Studio project
- `tests/ari_smoke.cpp` — portable logic tests
- `build_ari_windows.cmd` — Visual Studio build shortcut
- `.github/workflows/build_ari.yml` — Windows GitHub Actions build workflow
