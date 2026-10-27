# Reference and implementation notes

Reference package: Blish HUD Events and Metas Observer 1.0.9, namespace `bh.general.events`.
The reference .NET assembly is not included in this repository.

- Reference DLL SHA-256: `737edc492c70c407a453a8da6f39b19f7a816b75afe6ae79138ce09e0edb100f`.
- Bundled `assets/events.json` SHA-256: `ed8b592bac11ddb17ee983a2d4fcf65fff63a59027a8a24dcaeb82b90144ed84`.
- Source used to interpret behavior: [Community-Module-Pack commit 825f377](https://github.com/blish-hud/Community-Module-Pack/tree/825f37743bc588b2a9f986be175adfe5878ab243/Events%20Module), especially Meta.cs, EventsModule.cs, and EventNotification.cs.

REA 6.7.0 static inspection classified the reference as an AnyCPU .NET Framework CIL assembly, with 28 types and 245 methods. Metadata, method anchors and strings corroborate the reference-source structure; a full instruction-by-instruction equivalence or execution of the original module was not established. The reference-source event dataset differs from the supplied package, so this port uses the supplied 1.0.9 dataset exclusively.

The loader expands UTC offset/repeat definitions and merges records sharing name and category, retaining the first record's metadata. The 104 source records become 88 events. The original exposes category/search filters, a watchlist enabled by default, alphabetical/upcoming sorting, waypoint copying, wiki buttons, notification enable/mute toggles and movable ten-second notification cards.

This implementation rebuilds the UI and lifecycle for native Nexus API v6 and shared ImGui 1.80. UTC timestamp arithmetic avoids local-time/DST recurrence errors and the original midnight-zero sentinel ambiguity. Reminder checks run once per second rather than approximately every five seconds. A per-occurrence marker suppresses repeated alerts across watchlist or notification toggles within a loaded session. All timestamps are computed independently of the displayed local/UTC option.

`src/schedule.hpp` is platform-independent. `tools/generate_events.py` validates and embeds the dataset. `src/addon.cpp` implements Nexus rendering, event textures, clipboard, browser opening, preferences and lifecycle cleanup. No addon-owned threads or texture callback functions survive unloading.

The Wine smoke harness uses the actual addon DLL and real ImGui, with Nexus service functions simulated. It cannot establish in-game DX11 integration, real texturing, clipboard transfer, browser integration or sound behavior; these require an actual GW2/Nexus session.
