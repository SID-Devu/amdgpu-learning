# Chapter 88 — Customer / ISV / game studio interaction

A staff or PMTS engineer doesn't just write code — they're the company's technical face to important external partners. Game studios, AI companies, OEMs, cloud providers. How you handle these interactions decides whether AMD wins the next design.

## Who the customers are

| Segment | Example customers | What they want |
|---|---|---|
| AAA game studios | CDPR, Sony first-party, Microsoft Studios, Capcom | Their game runs great on AMD GPUs from launch |
| Engine vendors | Epic (Unreal), Unity, Activision, EA | Their engine extracts max perf on RDNA |
| AI / ML companies | Meta, OpenAI, Anthropic, Mistral | PyTorch / JAX / Triton on AMD; cluster reliability |
| Cloud providers | AWS, Azure, GCP, Oracle, Vultr | Multi-tenant SR-IOV, reliable reset, SLA-grade |
| HPC labs | ORNL, LLNL, CERN | Long-job stability, ECC, MPI/RCCL throughput |
| OEMs | Lenovo, Dell, HP, ASUS | Power, thermal, audio, suspend-resume on laptops |
| Operating system vendors | Microsoft, Red Hat, Canonical | Stable driver ABI, kernel integration |
| Standards bodies | Khronos, PCI-SIG, VESA | Spec contributions, conformance |

Each has different priorities and different tolerance for bugs.

## What they want from you

- **Show up prepared.** Have done your homework on their workload.
- **Be honest.** If a feature won't work for 6 months, say so. Don't promise.
- **Respect their time.** Customers' engineers are senior too. Don't waste their cycles with basic questions.
- **Follow up.** A bug filed is a bug owned. Update them weekly.

## A typical engagement

### "We're shipping our game in 4 months. Help us ship on AMD."

Phase 1 — Onboarding (weeks 0-2):

- NDA signed, legal sorted.
- Dedicated Slack/Teams/email channel.
- Get build access.
- Provide them tools: RGP, drivers, validation suites.

Phase 2 — Smoke test (weeks 2-4):

- Run their game, identify obvious problems.
- Prioritize: visual bugs > crashes > perf > polish.

Phase 3 — Deep dives (weeks 4-12):

- For each prioritized issue, root cause + fix or workaround.
- Sometimes the fix is in the game (you tell them). Sometimes the fix is in driver (you write it).
- Profile, optimize hot paths together.

Phase 4 — Ship (last 4 weeks):

- Lock the AMD driver version they ship against.
- Final certification.
- Marketing coordination ("optimized for Radeon").

After ship: support the title for years. Patches, regression fixes, new card support.

## Profiling with a customer

You sit (virtually or physically) with their engineer. You both run RGP / PIX captures of their workload. You point out:

- "This shadow pass is taking 4 ms because every quad has 2 triangles — split rebatch."
- "Your post-process is back-to-back compute but with full barriers; convert to event-style."
- "DCC is being decompressed every frame because of layout transition in this pass; here's a fix."

This is the work. Hours of profile-and-suggest, sometimes with their game engine source open, sometimes through opaque closed-binary captures.

## Reporting customer-found driver bugs

When a customer finds a driver bug:

1. They send you a repro (sometimes a 50 GB game build + savefile).
2. You reproduce locally.
3. You file an internal bug, link the customer's external ticket.
4. You either fix it or hand off to the right team.
5. Ship the fix in the next driver release window.
6. Update the customer.

The cycle time matters. A customer waiting 6 months for a fix to a launch-blocker is a *lost customer*.

## Pre-silicon engagement

Months *before* a chip launches, ISVs need:

- Early hardware (or simulators).
- Driver builds with experimental support for the new arch.
- Documentation on new features.
- Direct line to AMD engineers.

PMTS engineers participate in pre-silicon co-development with key partners. The terms are strictly NDA. The relationship is built on trust over years.

## OEM dynamics

Laptop OEMs care about:

- **Power**: every milliwatt matters. AMD's APU + dGPU mux logic must be perfect.
- **Thermal**: tight chassis, weird thermal solutions, audio coupling. Driver must hit thermal targets without throttling complaints.
- **Sleep/wake**: 1000+ daily resumes per laptop. Must work.
- **Brightness, hot keys, special buttons**: ACPI / vendor BIOS quirks.

OEMs file bugs you'd never see otherwise: "On our X1234 laptop, screen flickers when on battery between 20% and 30%." You learn to love EC firmware engineers.

## Cloud customer dynamics

Cloud is performance + reliability. They care about:

- **MTBF** (mean time between failures): how often does a GPU fail in a fleet?
- **Reset behavior**: a single tenant must not take down the box.
- **Serviceability**: hot-swap, telemetry, RMA flow.
- **SR-IOV correctness**: no cross-tenant leak, ever.

Their bug reports come with population statistics ("0.1% of MI300s in our fleet show this pattern"). You become a statistician.

## ISV best practices for AMD

You'll often write or co-author "ISV best practice" docs:

- Use mesh shaders rather than tessellation when possible.
- Pre-compile PSOs at startup.
- Use async compute for shadow passes.
- Avoid full RT on midrange GPUs by default.
- Implement upscaling (FSR) before HDR.

These docs go out via GPUOpen or directly to studios. PMTS engineers contribute, edit, and stand behind them.

## Conferences

Regular ones:

- **GDC** — Game Developers Conference. AMD has a booth, a track of talks.
- **SIGGRAPH** — academic graphics.
- **HotChips** — hardware deep dives.
- **XDC** (X.Org Developers Conference) — open-source graphics.
- **FOSDEM** — open-source generally.
- **LPC** (Linux Plumbers Conference) — kernel-level.
- **DevSummit** — internal AMD events.

PMTS engineers speak at these. Either talks they've prepared, panels, or "office hours" answering questions.

## Public communication

You'll write:

- Blog posts on GPUOpen.
- Conference talks.
- Open-source patches with detailed commit messages.
- Mailing-list responses to design proposals.
- Twitter/X / Mastodon / LinkedIn for visibility (career-facing).

Quality matters. A PMTS engineer's blog post is a small AMD endorsement; care is required.

## Ethical considerations

You will sometimes:

- Be asked to optimize for *one* benchmark in a way that hurts others. Push back.
- Hear customer complaints about a competitor in confidence. Don't repeat.
- Receive proprietary game code under NDA. Treat it like radioactive material.
- Be tempted to over-promise. Don't.

Your reputation outlasts any single product cycle. Protect it.

## What you should be able to do after this chapter

- Understand who AMD's customers are.
- Plan a 4-month engagement with a game studio.
- Triage a customer-reported bug correctly.
- Communicate effectively across NDA boundaries.
- Speak at a conference about AMD GPU work.

## Read deeper

- AMD GPUOpen blog (gpuopen.com).
- GDC vault talks for "AMD Radeon" or "ATI" historical context.
- Books on technical leadership ("Staff Engineer" by Will Larson — chapter on stakeholder management).
