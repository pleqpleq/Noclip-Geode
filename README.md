# Noclip v0.2.0: hidden wave trajectory prototype

Automatic wave press and release correction: silently checks immediate trajectory against candidate delays 1..5 (user-adjustable), and delays only when a later candidate avoids a predicted collision. Old recorded timing fallback remains for other gamemodes.

**This is experimental source code, NOT a verified frame-perfect solver or tested Geode binary.** It approximates the wave with 45-degree motion and static rectangular obstacles. Moving objects, triggers, rotations, portals, unknown collisions, precise hitbox geometry and future input sequences can invalidate the result. No visible trajectory is drawn. An unrecognized section passes the input normally.

Local predictor timing tests passed, but the Windows Geode build has previously failed in GeodeBindings (SequenceTriggerState/EffectManagerState incomplete types) before mod source was compiled. See Actions for the real build status. Target GD 2.2081 / Geode 5.11.2. See GITHUB_SETUP.md and INSTALL_WINDOWS.md for instructions.

Source inspiration: https://github.com/Borixya/auto-dodge (no third-party code copied).
