# Trajectory-based input correction: planned upgrade, NOT IMPLEMENTED

This document describes a better version of Noclip that would not require a
previous successful run. The current mod buffers input edges relative to
recorded X-position references. It has no collision-aware physics simulator.

## Desired behavior

When the player actually presses or releases Jump:

1. Clone an accurate read-only representation of player and level state.
2. Simulate the input NOW and calculate a predicted path for enough frames
   to include the upcoming obstacle, including collisions and triggers.
3. Test the same input delayed 1, 2, ..., max-window frames. The maximum
   defaults to 5 and remains user-configurable.
4. If NOW is safe, do not delay it. If NOW is dangerous, and a later timing
   is safe, choose the safest later trajectory (largest minimum clearance
   over the prediction horizon; ties choose smaller delay).
5. If none of the candidates is safe, DO NOT buffer; forward the input now.
   This also handles an input that arrives e.g. 10 frames before a narrow
   perfect window when max-window is 5.
6. For wave and ship, correctly simulate both press and release transitions,
   including subsequent inputs and held state. Do not assume that simply
   switching for one frame determines the rest of the path.

## Simulator requirements

- Faithful velocity, acceleration, gravity, time stepping, and speed changes.
- Collision shapes accurate enough for genuinely frame-perfect margins.
- Ground contacts, slopes, portals, jump orbs/pads, dash orbs, gravity flips.
- Wave size/hitbox, dual mode, robot charging, UFO, spider, swing, and
  platformer physics require their own simulation/validation.
- Other mods may change physics or frame stepping.
- Reproducible decisions without advancing or changing the LIVE PlayLayer.
- Bounded work per frame so candidate simulations do not lag the game.

A visible 'Trajectory' overlay alone is not a machine-readable, authoritative
prediction. Noclip would need the underlying simulation implementation or an
appropriate stable API. Silicate has trajectory tools; Pathfinder and the
open-source Auto Dodge project demonstrate the broad technique, but none is
assumed to expose a drop-in API usable by this project.

References:
- https://geode-sdk.org/mods/peony.silicate
- https://geode-sdk.org/mods/camila314.pathfinder
- https://github.com/Borixya/auto-dodge

No code was copied from the above projects into this repository.
