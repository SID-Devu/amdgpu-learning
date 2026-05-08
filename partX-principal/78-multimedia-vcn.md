# Chapter 78 — Multimedia (VCN), codecs

The GPU is also a video processor. AMD GPUs have a dedicated **VCN (Video Core Next)** block that decodes and encodes video at hardware speed — many gigapixels per second per stream, dozens of streams concurrently.

Multimedia is its own corner of the driver, with codec details, latency requirements, and DRM (digital-rights-management) interactions.

## What VCN does

A VCN block is a fixed-function + programmable engine that handles:

- **Decode**: H.264/AVC, H.265/HEVC, AV1, VP9 (legacy MJPEG on older parts).
- **Encode**: H.264, H.265, AV1 (depending on gen).
- **JPEG decode** (separate JPEG engine on most parts).

Each codec is a hardware pipeline implementing the standard's bitstream parsing, motion compensation, transforms, quantization, entropy (CABAC/CAVLC/range), in-loop filtering.

## VCN generations

| VCN | Year | Codecs |
|---|---|---|
| VCN 1.x | Vega (2017–) | H.264, HEVC dec/enc, VP9 dec |
| VCN 2.x | Renoir, Navi 1x | + AV1 dec |
| VCN 3.x | Navi 2x | + improved AV1 dec |
| VCN 4.x | Navi 3x, MI300 | + AV1 enc |
| VCN 5.x | Navi 4x | improved AV1 enc, dual streams |

Datacenter parts (CDNA) often have multiple VCN instances per chip for transcoding farms.

## How decode works

The application (e.g., Firefox, mpv, VLC):

1. Demuxes the container (mp4, mkv, ...) → bitstream buffers.
2. Parses bitstream headers (sequence parameter set, picture parameter set, slice header) — usually in software.
3. For each frame, builds a **picture parameter buffer** describing references, motion vectors, slice data.
4. Submits decode work via VA-API or VDPAU.
5. The userspace driver (Mesa's `radeonsi` / VAAPI) translates to PM4 and SDMA writes to VCN's compute-style engine.
6. VCN reads bitstream + reference frames, decodes, writes YUV output to a buffer.
7. Output buffer is composited onto display (via DPP YUV→RGB conversion) or fed into a downstream pipeline.

## How encode works

Encode is harder — encoder must make rate/quality trade-offs in real time.

1. App provides raw YUV input frames.
2. Userspace driver (radeonsi VAAPI encoder) configures rate-control, GOP structure, target bitrate.
3. Submits frames to VCN encode pipeline.
4. VCN computes motion vectors against references, transforms, quantizes, entropy-codes, emits a bitstream.
5. App writes bitstream to file or streams it.

VCN encode is used for screen recording (OBS), game streaming (Twitch/Discord overlays), low-latency desktop sharing (Parsec, AMD ReLive), Steam Remote Play, transcoding pipelines.

## Latency targets

| Use case | End-to-end latency | Notes |
|---|---|---|
| Cloud gaming | < 50 ms | sub-frame encode, low-delay GOP |
| Game streaming local | 10–20 ms | low-delay AVC/HEVC, slim references |
| Transcoding | best-effort | optimize for throughput |
| Screen capture | 16–30 ms | balanced |
| Real-time video calls | 100–200 ms | usually CPU; VCN can help |

Achieving very-low-latency encode is constrained — small intra-period, no B-frames, slim references; lower compression efficiency.

## VA-API and the userspace stack

```
mpv / OBS / Firefox / FFmpeg
        │
   libva  (VA-API)
        │
  vdpau / vaapi user driver (Mesa: src/gallium/drivers/r600/, src/gallium/winsys/amdgpu/, src/gallium/frontends/va/)
        │
    libdrm_amdgpu
        │
    /dev/dri/card*
        │
     amdgpu kernel driver
        │
        VCN engine
```

The VAAPI driver builds VCN command streams (PM4 packets specific to VCN: `RBBM_REGISTER_*`, decoded-by-firmware indirect buffers). It manages reference picture lists, allocates DPB (Decoded Picture Buffer) memory, handles error recovery.

## DRM (digital rights management)

Protected content (Netflix 4K HDR, etc.) requires:

- A **trusted application (TA)** running in the PSP — verifies firmware, holds keys.
- An encrypted bitstream that only the TA can decrypt.
- Decode that *never* exposes plaintext to host RAM. The decoded buffer is in a "secure" region.
- Display path enforces HDCP — the link to the monitor is encrypted.

Linux's content-protection support is rudimentary; full Widevine / PlayReady L1 paths are limited. AMD's mainline kernel supports the underlying primitives; full DRM-laden playback usually requires Windows or specific Chrome OS configurations.

PMTS engineers in this area work with content providers, OEMs, and standards bodies. Lots of NDA layers.

## DPB management

Modern codecs reference **multiple previous (and sometimes future) frames** for prediction:

- H.264: up to 16 reference frames.
- HEVC: up to 16 short-term + 16 long-term.
- AV1: explicit reference signaling, up to 7.

The driver/runtime must allocate and recycle these reference frame buffers, flag their lifetimes correctly, and ensure VCN reads exactly the right frames for each decode step. DPB bugs cause subtle visual artifacts ("ghosting," "color bleed").

## JPEG engine

Many parts have a separate JPEG decoder. Useful for thumbnail decoding in image-heavy workloads (web browsing, photo apps), or for AI inference pipelines with image input. Programmable similarly to VCN but a smaller engine.

## VCN driver pieces in kernel

`drivers/gpu/drm/amd/amdgpu/`:

- `vcn_v1_0.c` ... `vcn_v5_0.c` — per-VCN-version IP block code (set up rings, interrupts, firmware load).
- `amdgpu_vcn.c` — common VCN logic, ring handling.
- `jpeg_v*.c` — JPEG engine setup.

`amdgpu_vcn.c` exposes `vcn_dec_ring` and `vcn_enc_ring` to the scheduler; UMD submits IBs through these the same way GFX rings work.

## Cross-IP coordination

VCN cooperates with:

- **GFX/compute** for any post-processing (denoise, tone-mapping) that's not VCN-resident.
- **SDMA** for buffer copies in/out.
- **DCN** for direct scan-out of decoded YUV (no copy through compositor).
- **PSP** for protected-content keys.

## Power management for VCN

VCN is power-hungry when active and gateable when idle:

- Runtime PM auto-suspends VCN when no decode/encode for ~seconds.
- DMUB or RLC handles clock-gating.
- Encoded streams have hard latency budgets, so wake-up time matters.

## Testing and validation

- **Bitstream conformance**: AMD validates against codec-spec test vectors.
- **Real content**: AAA games, streaming services, broadcast-grade test patterns.
- **Stress**: many concurrent streams, long-running transcode farms.
- **Inter-vendor interop**: encoded content must play on every device.

When a customer reports "AV1 stream stutters in Firefox on Linux at 4K," that bug starts here.

## What you should be able to do after this chapter

- Identify VCN gen on a given chip.
- Sketch decode and encode flows.
- Define VA-API and Mesa's role.
- Describe DPB and why it matters.
- Reason about latency vs quality vs bitrate trade-offs.

## Read deeper

- `drivers/gpu/drm/amd/amdgpu/vcn_v*_0.c`.
- `mesa/src/gallium/frontends/va/` — Mesa VAAPI implementation.
- VAAPI (libva) docs.
- AV1 / HEVC / H.264 specs (ITU-T / ISO/IEC).
- AMD AMF (Advanced Media Framework) documentation for cross-platform API.
- OBS Studio source as a real-world consumer example.
