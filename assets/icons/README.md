# Bundled event artwork

39 PNG snapshots from ArenaNet's public render service, fetched on 2026-10-28.
Original game artwork belongs to ArenaNet / NCSOFT; see ../../THIRD_PARTY.md.

manifest.json maps each original event icon key to a PNG filename. The original
URL is https://render.guildwars2.com/file/<key>.png. Files are embedded by
tools/generate_icons.py; builds and addon startup do not download them.

When adding an event icon, save the corresponding PNG here and update the
manifest. The generator validates exact event-key coverage and PNG signatures.
