#!/usr/bin/env bash
# build-pdf.sh — combine the entire textbook into a single PDF.
#
# Pipeline: markdown -> (pandoc) -> HTML -> (weasyprint) -> PDF
# This handles unicode (box-drawing chars, arrows) cleanly without LaTeX.
#
# Output: ./from-zero-to-amd-gpu-engineer.pdf
set -e

cd "$(dirname "$0")"

OUT_MD="_combined.md"
OUT_HTML="_combined.html"
OUT_PDF="from-zero-to-amd-gpu-engineer.pdf"

# Order in which the parts appear in the PDF.
ORDER=(
  "README.md"
  "LEARNING-PATH.md"

  "part0-zero/README.md"
  "part0-zero/00-start-here.md"
  "part0-zero/01-what-is-a-computer.md"
  "part0-zero/02-using-linux.md"
  "part0-zero/03-files-and-paths.md"
  "part0-zero/04-text-editors.md"
  "part0-zero/05-what-is-code.md"
  "part0-zero/06-first-program-python.md"
  "part0-zero/07-decisions-and-repeats.md"
  "part0-zero/08-functions.md"
  "part0-zero/09-from-python-to-c.md"
  "part0-zero/10-when-things-break.md"
  "part0-zero/11-bridge-to-part1.md"
  "part0-zero/foundations/README.md"
  "part0-zero/foundations/A1-numbers.md"
  "part0-zero/foundations/A2-bits-and-bytes.md"
  "part0-zero/foundations/A3-boolean-logic.md"
  "part0-zero/foundations/A4-shell-power-tour.md"
  "part0-zero/foundations/A5-character-encoding.md"

  "part1-c/01-what-is-a-program.md"
  "part1-c/02-toolchain-and-first-programs.md"
  "part1-c/03-types-and-ub.md"
  "part1-c/04-pointers-and-memory.md"
  "part1-c/05-structs-unions-alignment.md"
  "part1-c/06-bit-manipulation.md"
  "part1-c/07-allocators.md"
  "part1-c/08-preprocessor-and-build.md"
  "part1-c/09-function-pointers.md"
  "part1-c/10-defensive-c.md"

  "partDSA/README.md"
  "partDSA/DSA-00-how-to-read.md"
  "partDSA/DSA-01-bigO.md"
  "partDSA/DSA-02-arrays.md"
  "partDSA/DSA-03-strings.md"
  "partDSA/DSA-04-linked-lists.md"
  "partDSA/DSA-05-stacks-queues.md"
  "partDSA/DSA-06-hash-tables.md"
  "partDSA/DSA-07-trees.md"
  "partDSA/DSA-08-balanced-trees.md"
  "partDSA/DSA-09-btrees.md"
  "partDSA/DSA-10-heaps.md"
  "partDSA/DSA-11-graphs.md"
  "partDSA/DSA-12-shortest-paths.md"
  "partDSA/DSA-13-tries.md"
  "partDSA/DSA-14-sorting.md"
  "partDSA/DSA-15-searching.md"
  "partDSA/DSA-16-recursion.md"
  "partDSA/DSA-17-dp.md"
  "partDSA/DSA-18-greedy.md"
  "partDSA/DSA-19-divide-conquer.md"
  "partDSA/DSA-20-bits.md"
  "partDSA/DSA-21-two-pointers.md"
  "partDSA/DSA-22-backtracking.md"
  "partDSA/DSA-23-union-find.md"
  "partDSA/DSA-24-advanced-trees.md"
  "partDSA/DSA-25-string-algos.md"
  "partDSA/DSA-26-system-design-ds.md"
  "partDSA/DSA-27-concurrent-ds.md"
  "partDSA/DSA-28-kernel-ds-tour.md"

  "partUSP/README.md"
  "partUSP/USP-01-unix-philosophy.md"
  "partUSP/USP-02-processes.md"
  "partUSP/USP-03-signals.md"
  "partUSP/USP-04-pipes.md"
  "partUSP/USP-05-shm-mmap.md"
  "partUSP/USP-06-mq-sem.md"
  "partUSP/USP-07-sockets.md"
  "partUSP/USP-08-epoll.md"
  "partUSP/USP-09-pthreads.md"
  "partUSP/USP-10-file-io.md"
  "partUSP/USP-11-procfs.md"
  "partUSP/USP-12-perf-tools.md"

  "part2-cpp/11-c-to-cpp.md"
  "part2-cpp/12-lifetime-and-rules.md"
  "part2-cpp/13-templates.md"
  "part2-cpp/14-modern-cpp.md"
  "part2-cpp/15-stl-and-containers.md"
  "part2-cpp/16-cpp-low-level.md"

  "part3-os/17-what-is-an-os.md"
  "part3-os/18-processes-threads-syscalls.md"
  "part3-os/19-virtual-memory.md"
  "part3-os/20-scheduling.md"
  "part3-os/21-io-stack.md"
  "part3-os/22-interrupts.md"

  "partOST/README.md"
  "partOST/OST-01-process-model.md"
  "partOST/OST-02-scheduling-theory.md"
  "partOST/OST-03-sync-theory.md"
  "partOST/OST-04-classic-problems.md"
  "partOST/OST-05-deadlock.md"
  "partOST/OST-06-memory-theory.md"
  "partOST/OST-07-page-replacement.md"
  "partOST/OST-08-fs-theory.md"
  "partOST/OST-09-crash-consistency.md"
  "partOST/OST-10-distributed-intro.md"

  "part4-multithreading/23-threads-and-memory-model.md"
  "part4-multithreading/24-locks-and-condvars.md"
  "part4-multithreading/25-atomics-and-lockfree.md"
  "part4-multithreading/26-kernel-concurrency.md"
  "part4-multithreading/27-gpu-concurrency.md"

  "part5-kernel/28-kernel-tree-and-build.md"
  "part5-kernel/29-modules.md"
  "part5-kernel/30-kernel-data-structures.md"
  "part5-kernel/31-kernel-memory.md"
  "part5-kernel/32-time-and-deferred-work.md"
  "part5-kernel/33-kernel-locking.md"
  "part5-kernel/34-driver-model.md"

  "part6-drivers/35-char-drivers.md"
  "part6-drivers/36-ioctl-mmap-poll.md"
  "part6-drivers/37-pci.md"
  "part6-drivers/38-dma-and-iommu.md"
  "part6-drivers/39-mmio-and-barriers.md"
  "part6-drivers/40-power-management.md"
  "part6-drivers/41-debug-and-trace.md"

  "part7-gpu/42-gpu-architecture.md"
  "part7-gpu/43-graphics-stack.md"
  "part7-gpu/44-drm-core.md"
  "part7-gpu/45-gem.md"
  "part7-gpu/46-ttm.md"
  "part7-gpu/47-kms.md"
  "part7-gpu/48-amdgpu-architecture.md"
  "part7-gpu/49-command-submission.md"
  "part7-gpu/50-scheduler-and-fences.md"
  "part7-gpu/51-gpu-vm.md"
  "part7-gpu/52-userspace.md"
  "part7-gpu/53-reset-and-recovery.md"

  "part8-hw-sw/54-comp-arch.md"
  "part8-hw-sw/55-caches-and-ordering.md"
  "part8-hw-sw/56-pcie.md"
  "part8-hw-sw/57-performance.md"
  "part8-hw-sw/58-debugging.md"

  "part9-career/59-what-they-want.md"
  "part9-career/60-interview-questions.md"
  "part9-career/61-portfolio-projects.md"
  "part9-career/62-upstream-contribution.md"
  "part9-career/63-twelve-month-plan.md"

  "partX-principal/README.md"
  "partX-principal/64-microarch-rdna.md"
  "partX-principal/65-microarch-cdna.md"
  "partX-principal/66-waves-occupancy-gprs.md"
  "partX-principal/67-memory-subsystem.md"
  "partX-principal/68-gpu-cache-hierarchy.md"
  "partX-principal/69-pm4-firmware.md"
  "partX-principal/70-geometry-pipeline.md"
  "partX-principal/71-rasterization.md"
  "partX-principal/72-pixel-rbe.md"
  "partX-principal/73-ray-tracing.md"
  "partX-principal/74-kfd-hsa-aql.md"
  "partX-principal/75-cwsr-preempt.md"
  "partX-principal/76-rocm-runtime.md"
  "partX-principal/77-display-core-dcn.md"
  "partX-principal/78-multimedia-vcn.md"
  "partX-principal/79-smu-power.md"
  "partX-principal/80-mesa-radv-aco.md"
  "partX-principal/81-llvm-amdgpu-backend.md"
  "partX-principal/82-vulkan-dx12.md"
  "partX-principal/83-sriov-virt.md"
  "partX-principal/84-vm-fault-recovery.md"
  "partX-principal/85-security-psp.md"
  "partX-principal/86-sqtt-rgp.md"
  "partX-principal/87-cross-stack-debug.md"
  "partX-principal/88-customer-isv.md"
  "partX-principal/89-maintainership.md"
  "partX-principal/90-silicon-bringup.md"
  "partX-principal/91-cross-vendor.md"
  "partX-principal/92-pmts-career.md"

  "appendix/toolchain.md"
  "appendix/qemu-and-kernel-dev-vm.md"
  "appendix/kernel-style.md"
  "appendix/glossary.md"
  "appendix/checklists.md"
  "appendix/reading-list.md"
  "appendix/amdgpu-source-map.md"

  "labs/README.md"
)

echo ">> assembling combined markdown..."
{
  cat <<'COVER'
# From Zero to AMD GPU Kernel Engineer

*A self-contained textbook covering C, DSA, userspace systems programming, OS theory, the Linux kernel, device drivers, and the AMD GPU stack — for engineers aspiring to FTE roles at AMD, NVIDIA, Qualcomm, Intel, Broadcom.*

**Author:** SID-Devu
**Repository:** https://github.com/SID-Devu/amdgpu-learning
**Generated:** 2026

---

This PDF combines the entire textbook into one document for offline reading. It includes:

- Part 0 — Absolute Zero (no programming knowledge required)
- Part 1 — C
- Part DSA — Data Structures & Algorithms (read-to-learn rewrites for chapters 1–7, 10, 14, 15)
- Part USP — Userspace Systems Programming
- Part 2 — C++
- Part 3 — Linux OS
- Part OST — Classic OS Theory
- Part 4 — Multithreading
- Part 5 — Kernel
- Part 6 — Device Drivers
- Part 7 — GPU & DRM
- Part 8 — Hardware/Software interface
- Part 9 — Career
- Part X — Principal/PMTS track
- Appendix
- Labs

Read the [LEARNING-PATH](LEARNING-PATH.md) section first for the recommended order.

\newpage

COVER

  for f in "${ORDER[@]}"; do
    if [ -f "$f" ]; then
      echo ""
      echo ""
      echo ""
      echo ""
      cat "$f"
      echo ""
    else
      echo "WARNING: missing $f" >&2
    fi
  done
} > "$OUT_MD"

WORDS=$(wc -w < "$OUT_MD")
LINES=$(wc -l < "$OUT_MD")
SIZE=$(du -h "$OUT_MD" | cut -f1)
echo ">> combined markdown: $LINES lines, $WORDS words, $SIZE"

echo ">> running pandoc -> HTML..."
pandoc "$OUT_MD" \
  --standalone \
  --toc \
  --toc-depth=2 \
  --metadata title="From Zero to AMD GPU Kernel Engineer" \
  --metadata author="SID-Devu" \
  --highlight-style=tango \
  --css=pdf-style.css \
  -o "$OUT_HTML"

HTML_SIZE=$(du -h "$OUT_HTML" | cut -f1)
echo ">> HTML: $HTML_SIZE"

echo ">> running weasyprint -> PDF (this may take 1-3 minutes)..."
python3 -c "
import sys
import weasyprint
weasyprint.HTML('$OUT_HTML', base_url='.').write_pdf(
    '$OUT_PDF',
    stylesheets=['pdf-style.css'],
)
"

PDF_SIZE=$(du -h "$OUT_PDF" | cut -f1)
echo ">> done: $OUT_PDF ($PDF_SIZE)"
