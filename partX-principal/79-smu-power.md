# Chapter 79 — SMU, DPM, voltage/frequency, telemetry

The GPU's clocks aren't fixed. They scale up under load, down at idle, and adjust per-IP block all the time. The thing controlling them is the **SMU (System Management Unit)** — a microcontroller running AMD-signed firmware that's effectively the GPU's autonomic nervous system.

PMTS engineers in power management spend years tuning what SMU does.

## What SMU manages

- **Clock frequencies** for every clock domain on the chip:
  - GFXCLK (shader engine clocks)
  - SOCCLK (uncore / interconnect)
  - MEMCLK (HBM/GDDR clock)
  - FCLK (Infinity Fabric)
  - VCN clocks
  - DCN pixel clock generators
  - PCIe link state
- **Voltages** for each rail (VDDGFX, VDDSOC, VDDMEM, ...).
- **Power consumption budgeting** to stay within TGP (Total Graphics Power) and TBP (Total Board Power).
- **Thermal management** — fan speed curves, thermal throttling.
- **Telemetry** — exposes current clock/voltage/temp/power readings to the kernel.
- **Boot sequencing** — brings up clocks in order during init.

## DPM — Dynamic Power Management

DPM is the algorithm SMU runs to pick clocks. Inputs:

- Activity counters per IP (how busy is GFX? VCN? DCN?).
- Temperature sensors (junction temp, edge temp, hotspot, mem temp).
- Power readings (estimated at chip + measured at board if available).
- User policy ("performance," "balanced," "power saver").

Output: a (frequency, voltage) operating point for each clock.

DPM is implemented as a small state machine with hysteresis to avoid clock thrashing. SMU updates clocks at sub-millisecond rates.

## OPPs and DPM tables

A list of allowed (frequency, voltage) pairs lives in firmware as a "DPM table." There are typically 8 levels per clock — DPM_0 (lowest) through DPM_7 (peak). Levels are hardware-binned per chip — silicon at the high end can hit DPM_7 cleanly; weaker silicon may cap at DPM_6.

```
GFXCLK DPM table:
   DPM_0: 500 MHz, 0.65 V
   DPM_1: 800 MHz, 0.70 V
   DPM_2: 1100 MHz, 0.75 V
   ...
   DPM_7: 2700 MHz, 1.05 V
```

Userspace sees these via `/sys/class/drm/card*/device/pp_dpm_*`:

```bash
cat /sys/class/drm/card0/device/pp_dpm_sclk
0: 500Mhz
1: 800Mhz
...
7: 2700Mhz *     ← * = currently active
```

You can write to these files to force a level (force_performance_level=manual) for testing.

## Telemetry

`rocm-smi` reads SMU telemetry over a kernel interface (`hwmon` plus AMD-specific paths). Available metrics:

- Junction temperature (Tj_max usually 105–110 °C; throttle starts before that).
- Edge temp.
- HBM temp (CDNA only).
- GFX activity %.
- Memory activity %.
- Power draw (W).
- Fan RPM.
- ECC counts.

Production monitoring (Prometheus exporters, Datadog) scrape these.

## SVI3 and serial voltage

Modern AMD GPUs use **SVI3** (Serial Voltage Identification, version 3) — a serial protocol between SMU and the on-board voltage regulator. SMU sends "set rail X to Y mV"; the regulator complies. Faster transitions, finer granularity.

Old SVI2 / SVI1 were slower, coarser. Datacenter AMD GPUs may have multiple regulators per rail for current density.

## Thermal management

SMU monitors:

- **Tj** (junction): from on-die diodes. The most critical — controls throttle.
- **Tedge**: PCB edge temp.
- **Thotspot**: max of all on-die diodes.
- **Tmem**: HBM temp.
- **Tboard**: chassis sensor.

Policy:

- Below threshold: free-running.
- Threshold reached: lower DPM level. Fans speed up.
- Tj_max: emergency clock cuts.
- Critical: shutdown.

Fan curves are programmable but constrained by AMD's safe envelope. OEMs (board partners) tune their own curves on top.

## Power capping

Each chip has:

- **TGP** (Total Graphics Power) — the GPU's headline TDP target.
- **TBP** (Total Board Power) — including memory and VRMs.
- **PPT** (Package Power Tracking) — measured estimate.
- **EDC** (Electrical Design Current) — current limits.
- **TDC** (Thermal Design Current).

SMU continuously balances clocks against these so the chip never exceeds its envelope. If a workload would push past PPT, clocks drop until the budget fits.

## Customer/ISV-specific tuning

Some ISVs (e.g., Blender, professional CAD) request bespoke DPM behavior — minimum sustained clocks, less aggressive idle. The driver may tag a process as "compute" vs "graphics" via context flags and feed that to SMU.

PMTS engineers field weird requests: "our app on MI250 runs slower at 70 °C than 80 °C" — yes, because at 70 °C SMU is in a transitional DPM band. Investigate, fix.

## Power features per platform

- **Mobile / APU**: SMU coordinates with PMU for shared TDP between CPU and GPU. STAPM (Skin Temperature-Aware Power Management) limits long-running power based on chassis temp.
- **Desktop**: GPU has its own envelope; PCIe slot limits separately enforced.
- **Datacenter (CDNA)**: tighter constraints, ECC mandatory, often no fan (passive, server cools the slot).

## RuntimePM in the kernel

Kernel-side, runtime PM (`pm_runtime_*`) interacts with SMU:

- When the GPU is idle, kernel suspends → SMU drops to deep idle.
- Wake on activity (a draw call, a poll) → SMU spins clocks up.
- Mobile parts have additional **PX (PowerXpress)** or **D3Cold** sleeps where the whole GPU is power-gated, only PSP/SMU stays on.

Suspend/resume bugs are common: a discovery sequence times out, SMU misses an ACK, GPU comes back broken. Lots of debugging life is here.

## SMU interface — kernel side

The kernel talks to SMU through:

- A doorbell-style mailbox in MMIO.
- Message + arg + response protocol: kernel writes message ID, SMU responds with status.

`drivers/gpu/drm/amd/pm/swsmu/` has per-chip SMU drivers. Each chip family has its own message numbers, table layouts, and quirks.

```c
amdgpu_dpm_set_soft_freq_range(adev, AMD_SMU_GFXCLK, min, max);
amdgpu_dpm_get_clk_freq_by_index(adev, ...);
```

These wrap message exchanges.

## SMU firmware updating

SMU firmware ships in `*_smc.bin`; PSP loads it during boot. New firmware fixes regulator timing, adds new clock states, or addresses errata. Out-of-tree distros sometimes lag firmware → people complain about "fan ramping wildly" until firmware updates.

## What goes wrong

- DPM oscillates between levels (hysteresis bug) → audible coil whine + jitter.
- Tj reading wrong → either over-throttle or thermal damage.
- Sleep/wake races → corrupted SMU state requiring full reset.
- Power-cap miscalibrated → underperformance even with thermal headroom.
- Voltage transitions glitch under load → silicon errors / hangs.

## What you should be able to do after this chapter

- Define SMU and DPM in one sentence each.
- Explain TGP/TBP, telemetry sources.
- Read `pp_dpm_sclk` and interpret it.
- Reason about thermal vs power throttling.
- Sketch the kernel↔SMU message flow.

## Read deeper

- `drivers/gpu/drm/amd/pm/swsmu/smu13/smu_v13_*.c` — recent gen.
- `drivers/gpu/drm/amd/pm/inc/smu_*.h` — table structures.
- AMD GPUOpen: "Power Management on Radeon."
- `rocm-smi` source for the telemetry queries.
- HotChips slides on "AMD power architecture" (any year).
