# Events and Meta Observer Nexus Addon — Linux

Event reminders and a watchlist for **Guild Wars 2 + Nexus on Linux through Wine/Proton**, ported from Blish HUD's Events and Metas Observer 1.0.9.

Tracks **88 events** from the original 104 schedule definitions: world bosses, meta events, convergences, group events, adventures, day/night cycles, and server reset. Includes category/search filters, upcoming start times, per-event reminders, waypoint copying, wiki links, and movable notifications. No GW2 API key is needed.

Nexus runs inside the Windows game process, so the addon is a **Windows x64 DLL built on Linux**. Blish HUD and .NET are not required. Nexus must already be installed and working in your GW2 prefix.

## Linux distribution compatibility

**The same addon DLL is intended to work on any x86_64 Linux distribution where GW2 and Nexus already work through Wine/Proton.** It has no Fedora-specific runtime dependency. Fedora, Ubuntu, Debian, Mint, Arch, openSUSE and gaming distributions use the same DLL; game paths and Wine/Proton configuration may differ.

Tested on Fedora; other distributions have not each been tested individually. The setup script and provided DLL target x86_64 systems; native ARM builds are not provided.

Existing repository URLs and release filenames are kept for compatibility with earlier links. This does not restrict where the DLL can run. `setup-linux.sh` is the general setup command; the previous `setup-fedora.sh` command remains a compatible alias.

## Install on Linux

Close GW2 before installing. The installer copies the DLL, backs up the Nexus configuration and any existing DLL, and sets **automatic loading on the next game launch**. Run project scripts as your regular user.

### Option 1: ready-made DLL (no compilation)

```bash
git clone https://github.com/insightfulrival/Events-and-Meta-Observer-Nexus-Addon-Fedora.git
cd Events-and-Meta-Observer-Nexus-Addon-Fedora
mkdir -p dist
curl -fL --retry 3 -o dist/events-observer.dll \
  https://github.com/insightfulrival/Events-and-Meta-Observer-Nexus-Addon-Fedora/releases/latest/download/events-observer.dll
curl -fL --retry 3 -o dist/SHA256SUMS \
  https://github.com/insightfulrival/Events-and-Meta-Observer-Nexus-Addon-Fedora/releases/latest/download/SHA256SUMS
(cd dist && sha256sum --check --ignore-missing SHA256SUMS)
python3 install.py
```

If needed, install Python 3, Git, curl and sha256sum using your distribution's package manager (commands below). Only package installation uses `sudo`. Alternatively, download the complete installation bundle from [Releases](https://github.com/insightfulrival/Events-and-Meta-Observer-Nexus-Addon-Fedora/releases), extract it, and run `python3 install.py` inside its folder.

### Option 2: build and install with one command

#### Install build prerequisites for your distribution

Use the command for your distribution, then run the clone/setup commands below:

```bash
# Fedora / Nobara and other DNF-based systems
sudo dnf install git python3 curl tar xz coreutils binutils

# Ubuntu / Debian / Linux Mint / Pop!_OS
sudo apt update
sudo apt install git python3 curl tar xz-utils coreutils binutils

# Arch Linux / EndeavourOS / CachyOS
sudo pacman -Syu --needed git python curl tar xz coreutils binutils

# openSUSE
sudo zypper install git python3 curl tar xz coreutils binutils
```

Run only the command for your distribution. For other package managers, install the equivalent tools: Bash, Git, Python 3, curl, tar, xz, sha256sum, and GNU objdump. Package installation may use `sudo`; project scripts must run as your regular user. On immutable distributions such as SteamOS or Bazzite, prefer the prebuilt DLL; build in a suitable development environment if you need a custom build.

```bash
git clone https://github.com/insightfulrival/Events-and-Meta-Observer-Nexus-Addon-Fedora.git
cd Events-and-Meta-Observer-Nexus-Addon-Fedora
./setup-linux.sh
```

The script uses **Zig 0.17.0**. If a matching `zig` or `python-zig` is unavailable, it downloads the official Linux compiler into `.tools/`, verifies its pinned SHA-256 against the value from the [official Zig download index](https://ziglang.org/download/index.json), then builds, tests, and installs the addon. It does not change the system toolchain.

### Other game locations

The default is `~/.local/share/Steam/steamapps/common/Guild Wars 2`. For another Steam library or a Lutris/Bottles installation, pass the folder containing `Gw2-64.exe`, Nexus's `d3d11.dll`, and `addons/Nexus/AddonConfig.json`:

```bash
python3 install.py --game '/path/to/Guild Wars 2'
# Or build and install:
./setup-linux.sh --game '/path/to/Guild Wars 2'
```

Steam Flatpak, default library:

```bash
python3 install.py --game "$HOME/.var/app/com.valvesoftware.Steam/.local/share/Steam/steamapps/common/Guild Wars 2"
```

The scripts do not install Nexus or change Proton/DXVK settings. If Nexus is not set up, follow the [official installation guide](https://raidcore.gg/gw2/guides/tools/nexus/installation). Steam usually needs this launch option for Nexus:

```text
WINEDLLOVERRIDES="d3d11=n,b" %command%
```

Keep any other launch arguments and DLL overrides your setup requires. Never rename this addon's DLL to `d3d11.dll`.

## Start and use

1. Start GW2 normally through your existing Wine/Proton setup. The addon is configured to load automatically.
2. The window starts hidden; reminders still run in the background. Open its window using the corner icon, **Ctrl+Alt+E**, or its button in Nexus Options. Change the shortcut in Nexus Keybinds (`EVOBS_TOGGLE_WINDOW`).
3. Choose the events you want to watch. All events are watched initially, matching the original addon; `Watch none` clears the watchlist.

| Control | Action |
| --- | --- |
| Event checkbox | Enable or disable reminders for that event |
| Search / Category / Watched only | Combine filters by name, alternative name, map, category, or watchlist |
| Sort by next start | Toggle between upcoming time and alphabetical order |
| Waypoint | Copy the chat code; paste it into GW2 chat |
| Wiki | Open the event page through Wine's browser integration |
| Enable notifications / Sound | Control popups and the Wine system notification sound |
| UTC times | Switch display between local time and UTC |
| Move notifications | Drag the notification title bar, then save its position |
| Test notification | Preview a ten-second notification and sound |

Reminders normally appear 5 or 10 minutes before an event; each event's lead time comes from the bundled schedule. Left-click notification text to copy its waypoint and dismiss it; right-click to dismiss. `Dismiss` closes one notification; `Dismiss all`, immediately to its right, clears all currently displayed notifications without changing the watchlist or future reminders. Settings and the watchlist are saved in `addons/events-observer/settings.txt`.

These are static scheduled start times, not live server-state detection. The bundled schedule is based on the original 1.0.9 dataset, with Northwatch Waypoint added for Battle in Tarir (Octovine) and Defending Tarir, and Dry Top Entry Waypoint added for Crash Site and Sandstorm. It is not automatically updated with GW2 patches. Scheduling uses UTC arithmetic, so changing the displayed timezone does not change event cadence.

## Updating and uninstalling

Close GW2 before updating. For source builds:

```bash
git pull --ff-only
./setup-linux.sh
```

For the prebuilt version, repeat the DLL/checksum download commands above, then run `python3 install.py`. Pass `--game` again if needed. The installer preserves your watchlist and other addons. Timestamped backups are kept beside `AddonConfig.json` and any previous addon DLL.

To disable, turn off the addon in Nexus. To uninstall, close the game and remove `addons/events-observer.dll`. Keep `addons/events-observer/` if you want to retain preferences.

## Building and testing separately

```bash
./setup-linux.sh --build-only
# Use an existing compiler:
ZIG=/absolute/path/to/zig ./setup-linux.sh --build-only
# Install that build later:
python3 install.py
```

`./build.sh` generates the embedded schedule, runs native C++ and Python tests, compiles `dist/events-observer.dll`, and checks its architecture, Nexus export, and dependencies. Edit `assets/events.json` and rebuild to customize event times or reminder lead times.

Optional integration smoke test, using a Wine executable already installed on your machine:

```bash
WINE='/absolute/path/to/wine' ./smoke-test.sh
```

For Steam Proton Experimental, a typical path is:

```bash
WINE="$HOME/.local/share/Steam/steamapps/common/Proton - Experimental/files/bin/wine" ./smoke-test.sh
```

The smoke test uses a separate prefix under `build/wine-smoke`; it does not start GW2 or edit the game prefix. It loads the actual DLL against a simulated Nexus API and tests ImGui rendering, options, the keybind, and three load/unload cycles.

## Troubleshooting

- **Close GW2:** fully exit the game before running the installer. Nexus saves its own configuration when exiting.
- **Nexus installation was not found:** check `--game`. Launch Nexus once and exit to create its configuration.
- **Download/checksum failure:** retry after checking connectivity. Setup will not extract an unverified compiler archive.
- **Addon missing or disabled:** check `addons/Nexus/Nexus.log`; confirm the addon is enabled and your Nexus supports API v6.
- **No reminders:** enable notifications, watch the event, and check its reminder lead time. Closing the addon window does not stop reminders.
- **No sound/browser/clipboard:** Wine desktop integration depends on your prefix and environment. Test these in your actual game session.
- **Missing icons:** icons download through Nexus from ArenaNet's render service. Timers and text still work offline.

## Validation and limitations

Built on Fedora 44 using Zig 0.17.0. Tests cover all 88 events, merging duplicate definitions, UTC midnight and daily rollover, irregular boss schedules, DST independence, and reminder deduplication. The actual DLL passed three load/render/keybind/unload cycles with Wine 11 from Proton Experimental and a simulated Nexus API.

The user has confirmed that the addon works in GW2 with Nexus. Automated integration tests use a simulated Nexus API; they do not independently verify every desktop integration or game configuration.

The interface is in English and uses Nexus/ImGui styling. Blish HUD's localized interface, animations, and specific chime are not ported; the sound uses Wine's system notification. Existing Blish HUD watchlists are not imported. Reloading the addon resets in-memory reminder deduplication. This is an unofficial local port; it is not listed in the Nexus library and does not self-update.

## Source and credits

The repository includes addon sources, the original schedule dataset, pinned SDK/ImGui dependencies, tests, Linux setup, and an installer. Generated DLLs are distributed through Releases. Original .NET DLLs, PDBs, local Wine prefixes, personal configuration, and raw analysis logs are excluded.

- [Blish HUD Community Module Pack](https://github.com/blish-hud/Community-Module-Pack), reference commit `825f37743bc588b2a9f986be175adfe5878ab243`; MIT, see `LICENSE`.
- [Nexus API](https://github.com/RaidcoreGG/Nexus-API), API v6, commit `9b2c53df86c00db6495642bfcff2d0611bd957ef`; see `vendor/Nexus-LICENSE`.
- Exact ImGui 1.80 sources from [Nexus](https://github.com/RaidcoreGG/Nexus/tree/47a5b220e1f173624dd494190c3d3f2f17c8dd5d/thirdparty/imgui); provenance and license in `vendor/imgui`.
- Guild Wars 2 names and art belong to ArenaNet / NCSOFT. See `THIRD_PARTY.md`.

See [provenance and implementation notes](docs/PROVENANCE.md) for the reference artifact and behavior changes.
