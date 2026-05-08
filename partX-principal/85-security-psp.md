# Chapter 85 — Security: PSP, FW signing, attestation, SEV

The GPU is increasingly part of the system's trust boundary. AMD GPUs include a **PSP (Platform Security Processor)** — a separate ARM-based security CPU on-chip — that boots all firmware, verifies signatures, holds keys, and mediates protected operations.

This is one of the highest-NDA areas in the company. The pre-public summary below is what you can talk about at conferences; deeper details are AMD-internal.

## Why GPUs need a security processor

- Firmware tampering would let an attacker exfiltrate memory across tenants.
- Cloud customers need confidence that another tenant can't read their model weights.
- Content protection (Widevine L1, PlayReady SL3000) requires hardware-rooted keys.
- Confidential computing (SEV-SNP, TDX) needs the GPU to prove its state.

PSP solves the first two; AMD's broader security stack handles the rest.

## PSP basics

- ARM Cortex-A5 (older) / Cortex-A8 / Cortex-A9 / newer cores depending on gen.
- Tightly coupled to the rest of the GPU via internal bus.
- Has its own ROM, RAM, and persistent storage (in some packages, OTP fuses).
- Boots first, before any other engine.

PSP firmware (`*_sos.bin`) is signed by AMD with an asymmetric key whose root of trust is in PSP ROM. Successive firmware loads are chained:

```
PSP ROM (silicon) → verifies PSP SOS → loads PSP SOS
PSP SOS → verifies & loads SMU FW
        → verifies & loads GFX FW (PFP, ME, MEC, MES)
        → verifies & loads SDMA FW
        → verifies & loads DMUB FW
        → verifies & loads VCN FW
        → verifies & loads RAS TA, ASD TA, HDCP TA, others
```

Any failed signature → engine doesn't load → graceful degradation or full failure depending on which engine.

## Trusted Applications (TAs)

PSP can run small AMD-signed programs called **Trusted Applications**:

| TA | Role |
|---|---|
| **ASD** (AMD Secure Driver) | base TA all drivers use |
| **HDCP** | content protection key handling for DP/HDMI |
| **RAS** | error reporting (some flows go through PSP) |
| **DTM** (Display Topology Manager) | display security policy |
| **SecureOS / Trustlets** | per-feature mini OSes |

Loaded by the kernel via PSP ring commands. Each TA runs in PSP's protected memory, isolated from PF/VF and from each other.

## Kernel↔PSP interface

`drivers/gpu/drm/amd/amdgpu/psp_*.c` implements a ring-based command interface:

```
1. Kernel constructs a command (struct psp_gfx_cmd_resp).
2. Writes it to the PSP ring buffer.
3. Tells PSP "go" via a register or doorbell.
4. PSP processes, writes a response.
5. Kernel reads response.
```

Commands include: load a TA, invoke TA, load firmware, run RAS handlers, query firmware versions.

PSP responses use status codes; well-defined errors (`TA_OUT_OF_MEMORY`, `INVALID_CCD_ID`, etc.). Production code retries on transient errors and bails on persistent ones.

## Firmware update flow

```
1. AMD signs new firmware blobs with offline keys.
2. Distributed in `linux-firmware` package (or Windows-side equivalents).
3. On boot, kernel walks driver IP table and asks PSP to load each.
4. PSP verifies signatures against ROM-rooted key.
5. PSP loads or rejects.
6. Driver waits for confirmation, then proceeds with init.
```

If a malicious user replaces a firmware blob: PSP rejects (signature mismatch), driver fails init, GPU is dead until valid firmware restored. **Cannot bypass at the kernel level** — PSP is the gate.

## Anti-rollback

PSP enforces minimum firmware versions. Once a chip has run version N, version < N is rejected (to prevent attacker from downgrading to a version with a known vulnerability). Implemented via fuses or persistent monotonic counters in PSP storage.

Practical consequence: **never** update GPU firmware in a way that gets "downgraded" later. Production rollback plans must account for this.

## Confidential Computing on GPU (SEV-SNP era)

**AMD SEV** (Secure Encrypted Virtualization) protects guest VM memory from the hypervisor by encrypting RAM with per-VM keys. **SEV-SNP** adds cryptographic integrity (no replay/relocation attacks).

Extending this to GPUs:

- The GPU memory must be unreadable by the hypervisor.
- The GPU must support memory encryption (TEE-on-GPU).
- DMA between CPU and GPU must use encrypted channels.

AMD has been progressively adding support. MI300 has memory encryption and some confidential-compute flows. Full end-to-end (encrypted training of an AI model on a hostile cloud) is the long-term aspiration.

## Attestation

A confidential workload demands **proof** that the GPU is in a known-good state:

- A valid firmware version.
- A valid TA configured.
- Memory encryption enabled.

PSP can produce a **signed attestation report** including its measurements. The customer's enclave verifies the report against AMD's public key, and only then unseals secret data. This is the same model as TPM-based remote attestation, but for the GPU.

Quotes are emitted via PSP commands; kernel exposes them through ioctls / sysfs paths.

## HDCP and content protection

For DP/HDMI displays of premium content:

1. Source (GPU) and sink (monitor) authenticate via HDCP 2.x.
2. Negotiate a session key.
3. Encrypt every pixel on the wire.

The kernel exposes HDCP property on the connector (`drm_property_create_enum` for "Content Protection"). DC programs the encrypter; PSP holds the keys; Trustlet handles negotiation.

Bug here = loss of HDCP certification for the platform. Slow walks through this code.

## Side-channel concerns

Modern security thinking includes side-channels:

- **Timing**: a tenant's compute jobs might leak data via cache-timing variations.
- **Power**: a tenant could measure system power and infer another's activity.
- **Voltage glitching** (lab attacks): physical access to glitch the chip into bypassing checks.

Mitigations:

- Cache flush on context switch (slow, sometimes used in shared-tenancy).
- Power-gating to mask activity.
- Anti-glitch circuits in PSP (proprietary).

## Threat-model for a PMTS reviewer

When reviewing a security-related patch, a senior engineer asks:

1. Is any input from userspace / VF / network used without validation?
2. Could a memory-mapping change race with TLB flush, exposing stale data to another tenant?
3. Can the patch be exploited to issue a forbidden PM4 packet?
4. Does it touch fence/semaphore handling — could a use-after-free leak data?
5. Is there a reset path that could leave the GPU in an exploitable state?

Approval requires confidence in all of these.

## Where PSP code lives

Kernel side:

- `drivers/gpu/drm/amd/amdgpu/psp_*.c` — PSP rings.
- `drivers/gpu/drm/amd/amdgpu/amdgpu_psp.h`.
- TA-specific kernel code: e.g., `amdgpu_xgmi.c` uses TAs for cross-GPU link auth on MI200/300.

Firmware side: not in tree (pre-built, signed blobs in `linux-firmware`).

## What you should be able to do after this chapter

- Sketch PSP boot order.
- Describe TAs and what they do.
- Explain anti-rollback and why downgrade is dangerous.
- Outline confidential-compute requirements on GPU.
- Identify HDCP and TA flows in DC.

## Read deeper

- AMD security white papers (search "AMD PSP," "AMD SEV-SNP whitepaper").
- `drivers/gpu/drm/amd/amdgpu/psp_*.c`.
- AMD GPUOpen security blog posts.
- Google research on AMD platform security (yearly papers).
- OS-Conf / DEF CON talks on AMD platform security.
