# Chapter 90 — Silicon bring-up, post-silicon validation

The most "real-world" thing AMD does is bring up new silicon: a chip arrives from the fab, and a team has to make it boot, run, and ship in 6–12 months. PMTS engineers in this area drive the process.

You won't do this in your first year. You'll watch it; in your fourth or fifth year you may be doing pieces.

## The phases

```
1. Tapeout         (chip's design is "frozen" and sent to fab — TSMC, Samsung)
2. Wafer return    (silicon comes back; first samples)
3. Lab bring-up    (initial validation; one chip on a test board)
4. Pre-production  (extending support, making OS work)
5. Production      (engineering samples ship; alpha and beta customers test)
6. Public launch   (consumer/datacenter availability)
```

Pre-tapeout, the design is being verified in simulation and emulation (Synopsys ZeBu, Cadence Palladium). Drivers are being prototyped against simulators that approximate the hardware.

## Lab bring-up — the first hours

A wafer comes back. A small team in a lab in Austin / Markham / Bangalore / Shanghai puts a chip into a test PCB. Power on:

- Does the chip respond on PCIe at all? (read PCI vendor ID register)
- Does PSP boot?
- Does SMU bring clocks to base? (read clock counters)
- Can the kernel driver bind?

This is the **POR (Power On Reset)** moment. It's a milestone celebrated company-wide.

If the chip doesn't respond: hours of debug with logic analyzers, oscilloscopes, x-ray imaging, electrical measurements. Sometimes a re-spin (fix in next mask set).

## Pre-tapeout simulation work

Drivers must work before silicon exists. Two main vehicles:

### Software simulator (PAL-Sim)

A high-level functional model of the hardware. Slow (microseconds per simulated clock), but easy. The driver can run against it to validate basic flows.

### FPGA / Emulator

A real hardware emulator (millions of dollars per box) that runs the design's RTL at ~1/1000 real speed. Drivers run against this for full register-accurate testing — TLB walks, IH interrupts, real DMA. Hours to run a kernel boot, days to run a full test.

PMTS engineers writing new IP block drivers do most of their early work here.

## Bring-up artifacts

Bring-up generates a stream of artifacts:

- **Errata documents**: silicon bugs that software must work around.
- **Workaround patches**: kernel and UMD changes to mask errata.
- **Pre-public firmware**: SMU/PSP fixes for early issues.
- **Bring-up scripts** (often in Python): unique sequences to coax features alive.

Errata are taken seriously. Some are fixed in B0/C0/etc. silicon revs; some persist for the chip's life and become permanent driver code.

## Communicating with hardware

When something doesn't work in bring-up, the question is: is it software, firmware, or silicon?

- **Software**: revert recent driver patches; bisect.
- **Firmware**: ask FW team for a debug build with logging.
- **Silicon**: provide a minimal repro to ASIC team, who run it against RTL/emulator and confirm.

Silicon bugs are scary because they can't be fixed by patches — only via mask revisions or driver workarounds. A senior PMTS engineer is often the bridge: speaks to silicon, kernel, FW, and UMD teams equally.

## Post-silicon validation (PSV)

Once a chip is functional, PSV teams pound on it for months:

- **Stress**: sustained 100% load for days.
- **Power cycle**: thousands of resets.
- **Thermal**: full envelope from cold to hottest spec.
- **Voltage**: corners (low/nominal/high) per rail.
- **Process**: chips from different wafers, different process corners.
- **Real workloads**: real games, real AI training jobs.

PSV publishes failure rates. Engineering decides: ship it, fix it, defeature it.

## Driver test plans for new silicon

For each new chip the driver must support:

| Test class | Examples |
|---|---|
| Smoke | bind, query, allocate, run a hello-world kernel |
| Functional | every IP block (GFX, SDMA, VCN, DCN) end-to-end |
| Integration | real Vulkan apps, ROCm benchmarks |
| Stress | weeks of mixed workload |
| Reset/recovery | inject hangs, validate recovery |
| Power | suspend/resume, runtime PM |
| Multi-GPU | xGMI, peer-to-peer |
| Thermal | force throttle path |
| Long-tail | obscure chip variants and SKU configurations |

PMTS engineers design and review these test plans.

## Errata handling pattern

A typical erratum's lifecycle:

1. ASIC team identifies (e.g., "TLB invalidation can drop one entry under specific conditions").
2. Errata document published (internal).
3. Driver/FW team designs workaround (e.g., flush TLB twice in this path).
4. Patch lands with comment referencing errata ID.
5. Workaround stays for the chip's life unless fixed in next stepping.

You'll see comments like:

```c
/*
 * NavixxFamily Errata #123: TLB invalidate may miss entries
 * when targeted with specific VMID range. Issue a global
 * invalidate after each targeted one as a workaround.
 */
```

These accumulate. Reading the workarounds for a chip teaches you what actually went wrong in silicon.

## Pre-launch software polish

In the months before launch:

- Performance tuning across N games and workloads.
- Polish: idle power, thermal acoustics, sleep latency.
- Compatibility: every monitor, every USB-C dock, every weird DP-MST setup.
- Compliance: FCC, EMC, drop tests (laptops), thermal certifications.

The driver stabilizes; new commits decline; the chip team hands off to long-term sustaining engineers.

## After launch — sustaining

The first year after launch:

- Bug reports flood in from real-world users.
- Hot patches via driver releases.
- Backports to LTS kernels.
- Game-day bugs: a new AAA title launches, exposes a corner-case bug, must be fixed in 24 hours.

PMTS engineers in sustaining are often the same ones who did bring-up. They know the chip's quirks intimately.

## Mentoring during bring-up

Bring-up is intense, hours-long. Senior engineers must:

- Calmly mentor junior engineers in the lab at 2 AM.
- Explain why "it works on my machine" isn't enough.
- Show how to bisect against silicon revs.
- Push for proper documentation of every fix.

Failure to mentor → next-gen bring-up has the same surprises.

## What you should be able to do after this chapter

- Sketch the silicon-life-cycle phases.
- Distinguish errata from bugs.
- Outline a driver test plan for a new chip.
- Reason about RTL emulator vs software simulator.
- Recognize bring-up phases in a real org.

## Read deeper

- "Hardware Implementation of Computer Architecture" — academic but useful background.
- AMD HotChips talks on chip bring-up (occasionally surface).
- Talks/papers on RISC-V chip bring-up (analogous, public).
- Cadence / Synopsys EDA tool docs (high-level).
- Internal AMD bring-up wikis (when you have access).
