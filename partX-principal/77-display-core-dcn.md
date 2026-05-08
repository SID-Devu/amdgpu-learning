# Chapter 77 — Display Core (DCN) deep dive

The display side of an AMD GPU is its own world: a real-time pixel pipeline running independently of compute/graphics, with its own microcontroller, its own clock domains, and a giant codebase (`drivers/gpu/drm/amd/display/`). This chapter is the high-level map.

## What "display" actually does

Once a frame is rendered to a framebuffer, the **display engine** must:

1. Read the framebuffer at the right pixel rate (e.g., 4K@120Hz = ~1.06 GP/s).
2. Apply scaling, blending of multiple planes (cursor, video overlay, UI).
3. Apply per-pixel transformations (color space conversion, gamma, HDR tone-mapping).
4. Send pixels out a physical link (DisplayPort, HDMI, eDP, VGA-equivalent legacy).
5. Manage timing (HSync, VSync, blanking intervals).
6. Coordinate hot-plug detection, link training, power states.

All in real time, with hard deadlines: missing a vblank = visible glitch.

## DCN block diagram (RDNA / CDNA-A era)

```
                  Memory (framebuffer in VRAM)
                          │
              +-----------+
              │ MMHUB / GFXHUB read clients (HUBP / DCHUBBUB)
              │
              v
+--------+     +--------+     +--------+
|  DPP   |     |  DPP   |     |  DPP   |    DPPs - per-plane processors
|        |     |        |     |        |    (scaling, color conv, gamma)
+--------+     +--------+     +--------+
     \             |             /
      \            |            /
       v           v           v
              +---------+
              |   MPC   |    Multi-Plane Controller (blending, OVL)
              +----+----+
                   │
                   v
              +---------+
              |   OPP   |    Output Pixel Processing (final color, dither)
              +----+----+
                   │
                   v
              +---------+
              |   OPTC  |    Output Pipe Timing Controller (timing gen)
              +----+----+
                   │
                   v
              +---------+
              |   DIO   |    Display I/O (link encoder, audio mux)
              +----+----+
                   │
                   v
            +---------------+
            | Stream encoder|   DP / HDMI / eDP transport
            +---------------+
                   │
                   v
                physical pins
```

Plus:

- **DCCG**: DC Clock Generator. Produces pixel clocks, link clocks.
- **DMCUB**: a microcontroller running display firmware ("DMUB"); offloads PSR (panel self-refresh), brightness control, IRQ handling, etc.
- **DSC**: Display Stream Compression block (visually-lossless compression for high-bandwidth modes).
- **HPD**: Hot-Plug Detect lines.

## Pipes

A "pipe" is a chain HUBP → DPP → MPC → OPP → OPTC → DIO bound to one display. Modern GPUs have 4–6 pipes; you can scan out to 4–6 displays simultaneously.

**Pipe split**: a single high-resolution display (e.g., 8K) can use *multiple* pipes in parallel, splitting the frame horizontally to meet bandwidth. Display Core decides this dynamically based on the validation engine's bandwidth math.

## Planes

A plane is one input layer into MPC blending. Typical planes per pipe:

- 1 primary plane (full screen, the desktop).
- 0–N overlay planes (video, cursor, secondary UI elements).
- 1 cursor plane.

Overlay planes save memory bandwidth and allow YUV video to be scanned out without conversion via the GPU shader (DPP does the conversion in flight).

Atomic KMS exposes planes as `drm_plane`s; userspace compositors use them for low-latency video playback and "client-mode" cursors.

## Atomic modeset, in DC's view

When userspace calls `drmModeAtomicCommit`:

1. DRM core builds an atomic state.
2. amdgpu_dm validates the state: can these planes/CRTCs actually work together at these rates?
3. If yes, DC's **validate engine** (`dcn*_resource.c`, `dml.c` — the famous Display Mode Library) computes detailed bandwidth/clock requirements.
4. If validate passes, DC programs all the registers to switch to the new state, atomically with vblank if requested.

DML is hundreds of formulas implementing AMD's pixel-bandwidth model. Wrong DML = pipe under-runs (display goes black, glitches). Production-quality DML is *extremely* hard.

## Modes, timings, links

A mode is a tuple: (resolution, refresh, scan polarity, vblank size, link rate, lane count, color depth, color space, ...).

For DisplayPort:

```
1080p60 SDR  → ~6 Gbps payload  → 4 lanes × HBR (2.7 Gbps) is plenty
4K60  HDR   → ~17 Gbps          → 4 lanes × HBR2 (5.4) or 4 × HBR3 (8.1)
4K120 HDR   → ~32 Gbps          → DP 1.4 + DSC, or DP 2.0 UHBR
8K60  HDR   → ~50 Gbps          → DP 2.0 UHBR (10/13.5/20)
```

Link training: the source and sink negotiate the highest stable lane count + symbol rate. Failure modes are many — AMD-FreeSync or specific TVs at 4K120 are common pain points.

## EDID / DisplayID

The monitor advertises its capabilities through **EDID** (a 128-byte blob, sometimes with extensions) over the DDC line at hot-plug. DC parses EDID to know modes, color spaces, HDR metadata, audio capabilities. **DisplayID** is the modern, larger replacement.

EDID parsing has 30 years of bugs and quirks. AMD has tables of monitor quirks in `drm_edid.c` / `drm_displayid.c` and amdgpu-specific overrides.

## HDR and color

DC supports:

- **Color spaces**: sRGB, Rec.709, Rec.2020, P3.
- **HDR transfer functions**: PQ (HDR10), HLG.
- **3D LUTs** for color management — applied per-plane in DPP.
- **Tone mapping**: usually compositor-side or display-side, some hardware acceleration via DPP.

Color management ABI: per-plane / per-CRTC properties — `IN_FORMATS`, `COLOR_ENCODING`, `HDR_OUTPUT_METADATA`, etc. Userspace (KWin, weston, monado) reads these and configures.

## Variable refresh (FreeSync / VRR / Adaptive Sync)

The display can be told "refresh whenever the GPU has a new frame" rather than every 16.67 ms. Implemented over DP/HDMI VRR. DC programs the OPTC to allow extending the vblank until a flip arrives.

Driver involvement:

- Detect VRR-capable panel from EDID/DisplayID.
- Expose VRR property to userspace.
- Handle "min/max refresh" range; clamp frame timing in compositor.
- Avoid jittering between refresh-locked modes.

PMTS-level work: chasing FreeSync flicker bugs that only manifest on specific panels at specific brightness levels.

## DSC (Display Stream Compression)

For modes that exceed link bandwidth, DSC compresses ~3:1 with visually lossless quality. Pipeline:

```
DPP output → DSC encoder in DCN → compressed stream over DP → DSC decoder in monitor
```

DSC is per-pipe; resource validation accounts for DSC slices. Programming it has many tiny knobs (slice height, bits-per-pixel, line buffer config).

## DMCUB — display firmware microcontroller

DMUB firmware runs on DMCUB, handles:

- IRQ steering (HPD, vblank, pflip).
- PSR1/PSR2 (panel self-refresh, eDP only — saves power on idle frames).
- Brightness control (eDP backlight).
- Some atomic update sequences (microcontroller commits to silicon at exact timing).

You'll see `dmub_*.c` files in the DC tree. Updating DMUB firmware is part of the GPU firmware load (PSP-signed).

## Audio

DCN handles audio over DP/HDMI: pulls audio samples from PCM buffers, interleaves into the display data stream. The Linux audio side is `snd-hda-amdgpu` or `snd-hda-codec-hdmi`. Drivers cooperate over a small ALSA-DRM bridging interface.

## Reset and recovery

Display reset is **independent** from GFX reset where possible. If GFX hangs, the user should still see the screen. DC's validate engine and dependency tracking enables this.

A full PCIe-level link reset takes everything; finer mode-1/mode-2 resets might keep DC alive.

## Driver responsibilities (massive)

`drivers/gpu/drm/amd/display/` is one of the largest subdirectories of the kernel. Roughly:

- `dc/` — vendor-shared display core (no Linux-isms).
- `dc/dcn*/` — per-DCN-version specific code.
- `dc/dml/` — display mode library (math).
- `amdgpu_dm/` — Linux DRM bridging code (atomic helpers, plane/CRTC drivers).

The bring-up of a new chip's display is a months-long multi-team effort: DC engineers + ASIC + firmware + Linux integration. DC is typically maintained by a dedicated AMD team.

## What you should be able to do after this chapter

- Sketch the DCN pipeline.
- Define DPP, MPC, OPP, OPTC, DIO, DCCG, DMCUB, DML.
- Walk through atomic modeset → DC validate → register program.
- Define DSC and PSR.
- Identify VRR / FreeSync pieces.

## Read deeper

- AMD GPUOpen blog: "Introduction to Display Core."
- `drivers/gpu/drm/amd/display/dc/dml/` — DML formulas.
- `drivers/gpu/drm/amd/display/amdgpu_dm/amdgpu_dm.c` — main bridge.
- DRM atomic KMS docs (`Documentation/gpu/drm-kms.rst`).
- DisplayPort 2.0, HDMI 2.1, EDID, DisplayID specs.
