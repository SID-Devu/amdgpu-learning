# Chapter 62 — Contributing to the upstream kernel and Mesa

Landing patches upstream is the single best signal of competence in our field. This chapter is the workflow.

## 62.1 The kernel patch flow

1. **Find something to fix.** Bug tracker (`gitlab.freedesktop.org/drm/amd`), checkpatch warnings, sparse warnings, regressions on the lists.
2. **Write the patch.** Small, atomic, one logical change.
3. **Test it.** Build, boot, run the relevant workload.
4. **Format it.** `git format-patch -1`. `scripts/checkpatch.pl`. Polish the commit message.
5. **Find maintainers.** `scripts/get_maintainer.pl`.
6. **Send via email** with `git send-email`.
7. **Address feedback.** Respin as v2, v3, etc. with `--in-reply-to`.
8. **Maintainer applies.** It hits a `-next` branch, then linus.

## 62.2 The commit message

Short subject line: `subsystem: short verb-led description (max ~70 chars)`. Examples:

- `drm/amdgpu: fix NULL deref in ras_recovery on early init failure`
- `drm/amdgpu: add tracepoint for fence wait latency`
- `drm/amdgpu/gfx11: correct CP_HQD_PIPE_PRIORITY register offset`

Body (mandatory, paragraphs separated by blank lines):

- **What** does the patch do.
- **Why** is it needed.
- **How** was it tested.
- **Caveats**, follow-ups, related bugs.

End with `Signed-off-by: Your Name <you@amd.com>` (`git commit -s`).

If fixing a regression, add `Fixes: <12-char SHA> ("subject")`.

If fixing a reported bug: `Reported-by:`, `Tested-by:`, `Reviewed-by:` accumulated as the patch travels.

## 62.3 The DRM/amdgpu specifics

- Mailing list: `amd-gfx@lists.freedesktop.org`.
- CC: `dri-devel@lists.freedesktop.org` for cross-cutting changes.
- Maintainers: Alex Deucher, Christian König, et al. (`get_maintainer.pl` will tell you).
- Pull-request rhythm: Christian (or Alex) collects to a `drm-next-amd` branch, then PR upstream weekly.

Big-picture rules:

- **Don't break in-tree drivers** with API changes. Convert them in the same series.
- **Add tests** when adding ioctls.
- **Get acks** from outside maintainers when touching DRM core.
- **Mind backwards compatibility** of UAPI. Adding fields to ioctl structs is fine if size grows; never reorder existing ones.

## 62.4 Mesa patch flow

Mesa lives on GitLab with merge requests, not mailing lists.

1. Fork `gitlab.freedesktop.org/mesa/mesa`.
2. Branch from `main`.
3. Make changes; commit with `Signed-off-by` (Mesa uses DCO too).
4. Push; open MR.
5. CI runs (Marge bot).
6. Reviewers comment.
7. Marge merges when approved.

Subsystems within Mesa: `radv` (Vulkan), `radeonsi` (GL), `aco` (compiler). Each has owners.

## 62.5 ROCm and KFD specifics

- ROCm code is on `github.com/ROCm`. PRs.
- KFD kernel code is in the Linux kernel tree but rapidly developed; the AMD Kfd-specific list is `amd-gfx`.
- The `kfd-staging` and `amd-staging-drm-next` branches at gitlab.freedesktop.org are where in-flight work lives before going upstream.

For ROCm-side patches, sign the CLA, follow GitHub flow.

## 62.6 Code review etiquette

- **Address every comment** even if just to say "fixed in v2" or "I disagree because…".
- **Keep diffs small.** A 100-line patch reviews; a 5000-line patch sits.
- **Don't argue** for the sake of arguing. The maintainer's word is final on style.
- **Threadlocking**: when reviewers say "this is racy," accept; show your synchronization analysis or fix.

## 62.7 Mistakes that get rejected

- Tabs/spaces wrong; line length wrong.
- Missing `Signed-off-by`.
- Function bigger than necessary; mixing logical changes.
- Not running `checkpatch.pl --strict`.
- Not testing.
- "Fixed warning" when the warning hides a real bug.
- Not handling all error paths.

## 62.8 An example first patch

```
Subject: [PATCH] drm/amdgpu: log the firmware version we attempted to load

When firmware loading fails during PSP init, we log the path but
not the version we expected.  This makes triage harder when users
report load failures on systems with mixed BIOS/PSP versions.

Add the expected version to the existing dev_err.

Tested on RX 7900 XTX with both correct and intentionally-wrong
firmware copies; in the failure case the new log line allows
quick identification of the version mismatch.

Signed-off-by: <you>
---
 drivers/gpu/drm/amd/amdgpu/psp_v13_0.c | 6 ++++--
 1 file changed, 4 insertions(+), 2 deletions(-)
```

Notice: short, concrete, tested, motivated. That patch will land.

## 62.9 Long-term contribution rhythm

Set yourself a goal: **one merged patch per month** for six months. Then **one per week**. By month six, you'll be on first-name basis with the maintainer of your area.

The transformation that happens to you: you read code differently, you suggest patches in your own internal codebase to the same standard, your code review improves dramatically. This is the actual "becoming a kernel engineer" — the upstream loop trains you faster than any internal mentorship.

## 62.10 Resources

- `Documentation/process/submitting-patches.rst` — the bible.
- `Documentation/process/coding-style.rst`.
- `lwn.net/Articles/` — search for "amdgpu," learn the ecosystem's narrative.
- `airlied.blogspot.com` — Dave Airlie's posts about DRM evolution.
- `phoronix.com` — news about kernel/Mesa changes.
- `freedesktop.org` git repos.

## 62.11 Exercises

1. Set up `git send-email`. Send a test patch to yourself.
2. Open three pages in browser tabs, then close them: `lwn.net/Kernel`, `lore.kernel.org/amd-gfx`, `gitlab.freedesktop.org/drm/amd/-/issues`. Make this a habit.
3. Pick one open issue with `kind/regression` label. Reproduce. Bisect. Send a `Reported-by`-worthy email even if you can't fix it.
4. Read every patch on `amd-gfx` for one week. Don't reply, just observe.
5. Submit one trivial patch this month. Get it merged.

---

Next: [Chapter 63 — The 12-month plan from contractor to FTE](63-twelve-month-plan.md).
