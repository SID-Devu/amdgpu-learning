# Chapter 89 — Maintainership and upstream leadership

Sending a few patches doesn't make you a maintainer. **Owning a subsystem upstream** does. This is the trail PMTS engineers walk in the open-source world.

## What "maintainer" means in Linux

A maintainer for a subsystem (e.g., `drivers/gpu/drm/amd/`):

- Reviews and merges patches into their tree.
- Sends pull requests up the chain (Alex Deucher → DRM tree → Linus).
- Ensures conformance with kernel coding/process standards.
- Handles bug reports affecting their area.
- Makes architectural decisions.
- Mentors new contributors.

In Linux, maintainership is *distributed*: there's a maintainer for `gpu/drm/amd/`, one for `gpu/drm/amd/display/`, one for `amdkfd/`, and so on.

`MAINTAINERS` file in the kernel root lists everyone. Look:

```
RADEON and AMDGPU DRM DRIVERS
M:	Alex Deucher <alexander.deucher@amd.com>
M:	Christian König <christian.koenig@amd.com>
L:	amd-gfx@lists.freedesktop.org
S:	Supported
T:	git https://gitlab.freedesktop.org/agd5f/linux.git
F:	drivers/gpu/drm/amd/
F:	drivers/gpu/drm/radeon/
F:	include/uapi/drm/amdgpu_drm.h
F:	include/uapi/drm/radeon_drm.h
```

`M:` are maintainers; `L:` is the mailing list; `T:` is the git tree.

## How you become a maintainer

There's no "election." You earn the role:

1. Send many high-quality patches over a few years.
2. Review others' patches consistently and well.
3. Be active on the mailing list with helpful, constructive replies.
4. Show ownership of an area — when something breaks, you fix it.
5. Eventually a sitting maintainer says "I want X to take this over" or "X is my co-maintainer for Y."

It's a multi-year commitment. PMTS engineers at AMD are the standard maintainership pool.

## Reviewing patches

Reviewing is at least as important as writing.

### Review checklist (mental version)

- **Does the patch describe the problem?** Commit message is the documentation.
- **Does it fix what it claims?** Read the actual diff.
- **Side effects:** does it break other use cases? Other chips? Other configs?
- **Locking:** added new shared state — is it protected? Order maintained?
- **Memory safety:** any new allocations match deallocations? Error paths?
- **ABI:** any uapi headers changed — backwards compatible?
- **Tests:** could the change be tested? If yes, is there a test?
- **Style:** `checkpatch.pl` clean.
- **Performance:** does it regress anything?

A good review can take an hour. A great review for a complex patch can take a day. Worth it.

### Review etiquette

- Be specific. "This locking is wrong" → "Line 42: `vm->lock` is taken without disabling IRQs, but the IRQ handler at line 78 also takes it; this can deadlock."
- Suggest fixes when you can.
- Distinguish "must fix" from "would be nice."
- Don't hold over personal taste — if the patch works and follows conventions, ship it.

### Review tags

When you review, use these tags in your reply:

| Tag | Meaning |
|---|---|
| `Reviewed-by:` | "I reviewed this carefully and it's correct." |
| `Acked-by:` | "I'm OK with this; not as deep as Reviewed-by." |
| `Tested-by:` | "I ran it and it works." |
| `Reported-by:` | (for the author to add) "X reported this bug." |
| `Suggested-by:` | "X suggested the fix." |

These end up in the commit when applied. They're how you build reputation.

## Sending pull requests

When you have a queue of patches ready for the next merge window:

1. Rebase on the appropriate base.
2. Run regression tests.
3. Tag a release: `git tag -s drm-misc-next-2026-05-07 -m "..."`.
4. `git request-pull <base> <url> <tag>` and email the result to the next-up maintainer.

The PR email gets reviewed; on acceptance, the upstream pulls.

## Conflict resolution

Conflicts happen — between AMD's needs and other vendors', between maintainers, between technical visions. Senior contributors:

- State their position in writing on-list.
- Listen to counter-arguments.
- Compromise where possible.
- Escalate to broader maintainers if needed (Dave Airlie for DRM; Linus for kernel-wide).
- Don't let conflicts get personal.

You will see flame wars. Choose to be the calm one.

## ABI stability — sacred

Linux's prime directive: **don't break userspace.**

Once a uapi header is in a release, you cannot:

- Remove a struct field.
- Change a field's meaning.
- Add a required field (existing apps don't fill it).
- Change ioctl numbers.
- Reduce a permission's scope.

You *can*:

- Add new optional fields (apps that don't fill it work as before).
- Add new ioctls.
- Add new flags to existing ioctls (with new bit meanings; old bits keep semantics).

A patch breaking userspace ABI gets nacked at lightning speed. Even if "no one uses it" — that's never proven.

## Working with downstream distros

Distros (Ubuntu, Fedora, RHEL, Debian, Arch) maintain their own patches. As maintainer:

- Push fixes upstream **first**.
- Tag user-visible bugfixes with `Cc: stable@vger.kernel.org` so they auto-backport.
- Coordinate on out-of-tree patches that distros refuse to upstream (rare but happens).

Backports require careful patch isolation — your fix must be self-contained, not part of a chain.

## The 6-month merge cycle

Linux releases every ~10 weeks. Within that:

- Weeks 0–2: merge window. Maintainers send PRs to Linus. Big features land.
- Weeks 3–10: stabilization. Only fixes accepted (`-rcN` releases).
- Final: `vX.Y` released.
- Next merge window opens.

For a feature you want in `vX.Y`, you must have it reviewed-and-acked **before** the merge window opens — usually weeks of lead time. PMTS engineers plan releases backwards from this calendar.

## Mesa / Mesa-like maintainership

Mesa is GitLab-style merge requests rather than email patches. The community is smaller, more visible, and more conversational. Reviews happen in MR comments.

Becoming a Mesa maintainer:

- Many merged MRs in a specific area.
- Active in MR reviews.
- Project leadership grants Developer or Maintainer role.

Same general ethics; faster cadence (Mesa releases quarterly).

## Working with bugbots

Modern Linux upstream uses automation:

- **`syzbot`** — fuzzer that finds kernel bugs and emails reports.
- **LKP (Linux Kernel Performance)** — automated performance regression detection.
- **regzbot** — tracks regressions across releases.
- **0day robot** — build/test on every patch.

Maintainers must respond to these; ignoring `syzbot` reports is a quick way to lose credibility. PMTS engineers triage bot output and either fix, suppress (with rationale), or hand off.

## Mentoring

You'll be asked to onboard new contributors. Patient mentoring is part of the role:

- Pair-debug their first real patch.
- Walk them through how to use git format-patch / send-email.
- Explain patch comment etiquette.
- Encourage them when reviewers are harsh.

Good engineers grow good engineers. The teams that mentor well retain knowledge across decades.

## What you should be able to do after this chapter

- Read MAINTAINERS, find the right person to send a patch to.
- Conduct a thorough patch review.
- Tag patches correctly (`Reviewed-by`, `Fixes`, `Cc: stable`).
- Plan a feature delivery against the merge window calendar.
- Handle a `syzbot` report.

## Read deeper

- `Documentation/process/` in the kernel — every page.
- `Documentation/process/maintainer-handbooks.rst`.
- "Linux From Scratch — Patch Submission Howto" (multiple versions online).
- Maintainer summit talks (Linux Plumbers Conference).
- Greg KH's "kernel maintainership" essays.
