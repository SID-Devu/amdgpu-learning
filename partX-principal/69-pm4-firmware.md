# Chapter 69 — PM4 packets, command processor, firmware engines

The kernel doesn't actually drive the GPU's pixel pipeline. **Firmware does.** Specifically a small army of microcontrollers running AMD-signed firmware, communicating with the host through structured packet streams called **PM4**.

Understanding PM4 + the CP firmware is the *real* difference between a junior driver dev and a senior one. This chapter is the bridge.

## The firmware engines (one chip, many CPUs)

A modern AMD GPU contains, in addition to the shader cores:

| Engine | Role | Firmware filename pattern |
|---|---|---|
| **CP / ME** (Command processor / Micro Engine) | parses PM4, drives GFX ring | `*_me.bin` |
| **PFP** (Pre-Fetch Parser) | fetches/decodes PM4 ahead | `*_pfp.bin` |
| **CE** (Constant Engine, GCN) | older constant-buffer path; gone in RDNA | `*_ce.bin` |
| **MEC** (Micro Engine Compute) | drives compute rings | `*_mec.bin`, `*_mec2.bin` |
| **MES** (Micro Engine Scheduler) | newer scheduler engine, manages user queues | `*_mes.bin`, `*_mes_2.bin` |
| **RLC** (Run List Controller) | power gating, clock gating, save/restore | `*_rlc.bin` |
| **SDMA** | DMA engines, also FW-driven | `*_sdma.bin` |
| **VCN** | video codec engines | `*_vcn.bin` |
| **PSP** | security boot, FW signing | `*_sos.bin`, `*_ta.bin` |
| **SMU** | power/clock controller | `*_smc.bin`, `*_pp_table.bin` |
| **DMCUB** (DCN) | display microcontroller | `*_dmcub.bin` |

You can list yours:

```bash
ls /lib/firmware/amdgpu/
```

PSP loads, verifies signatures on, and brings up all the others. If PSP says no, the GPU doesn't run.

## What PM4 is

PM4 is a **packetized command format** parsed by ME/PFP/MEC. A PM4 packet is a 32-bit header followed by N data dwords:

```
 31         16 15  8  7   0
+-------------+-----+------+
| length-1    | OP  | type |   header
+-------------+-----+------+
| dword 1     |
+-------------+
| dword 2     |
+-------------+
...
+-------------+
| dword N     |
+-------------+
```

`type=3` is the only one used in modern flows (PM4 type-3). The opcode (`OP`) selects the packet. There are 100+ packet types; you'll meet a few constantly.

### Most common PM4 packets

| Packet | What it does |
|---|---|
| `PACKET3_NOP` | filler |
| `PACKET3_INDIRECT_BUFFER` | "switch to this IB and execute it" — chains UMD work into the ring |
| `PACKET3_WRITE_DATA` | write N dwords to memory or registers |
| `PACKET3_COPY_DATA` | copy a register to memory or vice versa |
| `PACKET3_WAIT_REG_MEM` | wait until a memory/register value matches a condition (polling) |
| `PACKET3_RELEASE_MEM` | post a fence: flush caches, then write a value to memory |
| `PACKET3_ACQUIRE_MEM` | invalidate caches before reading |
| `PACKET3_DRAW_INDEX_*`, `PACKET3_DRAW_INDIRECT` | submit draws |
| `PACKET3_DISPATCH_DIRECT` | submit a compute dispatch |
| `PACKET3_SET_*_REG` | program register state (CONTEXT, SH, UCONFIG, CONFIG groups) |
| `PACKET3_EVENT_WRITE_*` | issue events (cache flush, pipeline flush, end-of-pipe) |
| `PACKET3_PREEMPT` | request mid-task preemption |
| `PACKET3_COND_EXEC` | execute next packet only if a condition is true |

You will read PM4 streams when debugging driver bugs. Tools to do so:

- `umr` (`umr -RS gfx_0.0.0`) can dump ring contents.
- Mesa has trace replay (`AMD_DEBUG=hang`) which dumps the IB on hang.
- `amdgpu_ring` debugfs files.

A senior engineer reads PM4 like English.

## The command processor pipeline

When the kernel writes a PM4 stream into a GFX ring buffer and rings the doorbell:

```
1. CP wakes up (was idle since last work).
2. PFP fetches packets ahead of ME.
3. ME executes packets:
     - SET_CONFIG_REG → writes a register
     - WAIT_REG_MEM → CP polls until condition
     - RELEASE_MEM → CP issues an EOP event, which after pipeline flush writes a fence value
     - INDIRECT_BUFFER → CP jumps into UMD-emitted IB
4. UMD IB has draws/dispatches → CP launches waves on shader engines.
5. End of UMD IB → return to kernel ring.
6. Kernel emits a final RELEASE_MEM that signals a fence in memory.
7. CP raises an interrupt (IH path).
```

The kernel's role is *mostly*: build small "trampoline" PM4 sequences that bracket UMD's work — set up VMID, set up some context state, jump into UMD IB, emit a fence, raise an IRQ. UMD does all the actual draws.

## How the kernel emits PM4 (in `amdgpu`)

Each ring type has a `funcs` table:

```c
struct amdgpu_ring_funcs {
    void (*emit_ib)(struct amdgpu_ring *ring, struct amdgpu_job *job, ...);
    void (*emit_fence)(struct amdgpu_ring *ring, u64 addr, u64 seq, unsigned flags);
    void (*emit_pipeline_sync)(struct amdgpu_ring *ring);
    void (*emit_vm_flush)(struct amdgpu_ring *ring, unsigned vmid, u64 pd_addr);
    void (*emit_reg_wait)(struct amdgpu_ring *ring, u32 reg, u32 val, u32 mask);
    /* lots more */
};
```

Per-IP files (`gfx_v10_0.c`, `gfx_v11_0.c`, ...) provide implementations that emit PM4 dwords with `amdgpu_ring_write(ring, dw)`. Reading these tells you exactly what the kernel pushes for each operation.

## MES — the next-generation scheduler

Older AMD GPUs did all queue management in the kernel: the kernel mapped one global GFX ring and a few KIQ/MEC compute rings, and sequentially streamed PM4 into them.

With **MES** (introduced gradually, GFX10 onward), the firmware itself runs a scheduler. User-mode queues (UMQs) become real: each ROCm process can have its own ring, doorbell, and queue, and MES schedules among them in firmware. KFD (`amdkfd`) heavily relies on MES for compute queue scheduling.

Implications:

- The host can submit through user queues (no syscall per submit) — huge for low-latency compute.
- Reset and recovery becomes more nuanced (MES-managed vs host-managed queues).
- The kernel's role shifts toward setup + reset, not constant scheduling.

We treat MES + user queues in chapter 74.

## RLC — the unsung hero

The RLC handles **power and clock gating**: dynamically turns off CUs/SEs when idle. It also handles **CWSR** (Compute Wave Save/Restore — chapter 75) which saves wave state when a higher-priority job preempts. Without RLC, no power efficiency.

The kernel programs RLC tables for:

- which CUs to gate when load drops,
- save/restore image layout for CWSR,
- microcode version expectations.

If RLC firmware is wrong, GPU hangs in non-obvious ways during idle transitions.

## PSP boot order

```
1. PSP ROM (silicon ROM) verifies and runs PSP SOS firmware from ROM/flash.
2. SOS verifies and loads:
   - SMU firmware
   - GFX firmware (PFP/ME/MEC/MES)
   - SDMA firmware
   - VCN firmware
   - DCN/DMCUB firmware
3. SOS may load Trusted Applications (TAs) for content protection (HDCP, ASD).
4. SOS hands control: SMU brings clocks online; GFX waits for kernel.
```

If you see "PSP failed to load" in `dmesg`, the GPU is dead until reboot. Failure modes: corrupt firmware blob, signature mismatch, broken PCIe link.

## Versioning and pain

Every chip family ships its own firmware versions. The kernel has tables of "minimum acceptable version" and refuses to bind if the system firmware is older. Production kernels fight this by either bundling FW with the kernel package or upstreaming compatibility checks.

`/sys/kernel/debug/dri/0/amdgpu_firmware_info` lists current versions.

## Attack surface

Firmware is the GPU's TCB (trusted computing base). PSP signing prevents tampered firmware. RAS handles SRAM/cache ECC. SR-IOV virtualization adds a separate trust boundary (chapter 83, 85).

A recent class of GPU exploits goes through tricking the kernel into corrupting PM4 streams — the firmware doesn't fully validate every field, and a cleverly crafted submission can read other VMID's pages, etc. Mitigations live in the CS validation path (`amdgpu_cs.c`).

## What you should be able to do after this chapter

- Name the firmware engines and what each does.
- Explain a PM4 type-3 packet structure.
- Read a PM4 dump and identify draws / fences / VM flushes.
- Describe what MES changed about queue scheduling.
- Trace from kernel `amdgpu_ring_commit` to silicon execution.

## Read deeper

- AMD `pm4` headers in the kernel: `drivers/gpu/drm/amd/include/asic_reg/gfx_*/gfx_*_offset.h` and `pm4_packet*.h`.
- `drivers/gpu/drm/amd/amdgpu/gfx_v11_0.c` for a recent-gen ring funcs implementation.
- `drivers/gpu/drm/amd/amdgpu/mes_v11_0.c` for MES setup.
- `drivers/gpu/drm/amd/amdgpu/psp_*.c` for PSP boot sequencing.
- `umr` source: <https://gitlab.freedesktop.org/tomstdenis/umr> — has packet decoders.
- AMD GPUOpen blog posts on MES.
