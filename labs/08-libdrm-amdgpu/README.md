# Lab 08 — libdrm_amdgpu hello

Open the AMD GPU from userspace, query info, allocate and map a buffer.

## Prerequisites

```bash
sudo apt install libdrm-dev libdrm-amdgpu1 pkg-config
```

You need a system with an AMD GPU and amdgpu loaded (`lsmod | grep amdgpu`).

## Build & run

```bash
make
./info
```

You should see:

```
driver: amdgpu 3.x.x
amdgpu version: 3.x
family_id=...  asic_id=0x...
...
wrote 0xcafebabe 0xdeadbeef  →  read back 0xcafebabe 0xdeadbeef
```

## Stretch (the big one)

Extend `info.c` to actually submit a tiny PM4 IB:

1. Allocate a **second** BO for the IB; map; build a `WRITE_DATA` packet writing 0xCAFEBABE into the first BO.
2. Map the IB BO into GPU VA via `amdgpu_va_range_alloc`.
3. Create a context (`amdgpu_cs_ctx_create`).
4. Submit (`amdgpu_cs_submit`) with `AMDGPU_HW_IP_GFX`.
5. Wait (`amdgpu_cs_query_fence_status`).
6. Read back BO #1 — it should contain your value, written by the GPU.

This is your first GPU-executed operation. Print the value, framed by congratulations.

The full code is left as the lab's main exercise; consult `tests/amdgpu/` in the libdrm source tree for examples.
