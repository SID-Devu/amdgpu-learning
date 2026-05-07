/*
 * info.c -- minimal libdrm_amdgpu program.
 *
 * Opens /dev/dri/renderD128 (or first found), queries device info,
 * allocates a 4 KiB GTT buffer, maps it into CPU, writes a pattern,
 * verifies, frees, and exits.
 *
 * Build:  make
 * Run:    ./info
 */

#include <amdgpu.h>
#include <amdgpu_drm.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <xf86drm.h>

int main(void)
{
    int fd = open("/dev/dri/renderD128", O_RDWR);
    if (fd < 0) {
        perror("open /dev/dri/renderD128");
        return 1;
    }

    drmVersionPtr ver = drmGetVersion(fd);
    if (!ver) { perror("drmGetVersion"); return 1; }
    printf("driver: %s %d.%d.%d\n", ver->name, ver->version_major, ver->version_minor, ver->version_patchlevel);
    drmFreeVersion(ver);

    amdgpu_device_handle dev;
    uint32_t major, minor;
    if (amdgpu_device_initialize(fd, &major, &minor, &dev)) {
        fprintf(stderr, "amdgpu_device_initialize failed\n");
        return 1;
    }
    printf("amdgpu version: %u.%u\n", major, minor);

    struct amdgpu_gpu_info info;
    if (amdgpu_query_gpu_info(dev, &info)) {
        fprintf(stderr, "amdgpu_query_gpu_info failed\n");
        return 1;
    }
    printf("family_id=%u  asic_id=0x%x  chip_external_rev=0x%x\n",
           info.family_id, info.asic_id, info.chip_external_rev);
    printf("vram_type=%u  vram_bit_width=%u  num_shader_engines=%u\n",
           info.vram_type, info.vram_bit_width, info.num_shader_engines);
    printf("ce_ram_size=%u  num_tile_pipes=%u  num_cu_per_sh=%u\n",
           info.ce_ram_size, info.num_tile_pipes, info.num_cu_per_sh);

    /* Allocate a 4 KiB GTT BO, mmap, write, read, free. */
    struct amdgpu_bo_alloc_request req = {
        .alloc_size     = 4096,
        .phys_alignment = 4096,
        .preferred_heap = AMDGPU_GEM_DOMAIN_GTT,
        .flags          = 0,
    };
    amdgpu_bo_handle bo;
    if (amdgpu_bo_alloc(dev, &req, &bo)) {
        fprintf(stderr, "amdgpu_bo_alloc failed\n");
        return 1;
    }

    void *cpu;
    if (amdgpu_bo_cpu_map(bo, &cpu)) {
        fprintf(stderr, "amdgpu_bo_cpu_map failed\n");
        return 1;
    }

    memset(cpu, 0xCC, 4096);
    uint32_t *p = cpu;
    p[0] = 0xCAFEBABE;
    p[1] = 0xDEADBEEF;
    printf("wrote 0x%08x 0x%08x  →  read back 0x%08x 0x%08x\n",
           0xCAFEBABE, 0xDEADBEEF, p[0], p[1]);

    amdgpu_bo_cpu_unmap(bo);
    amdgpu_bo_free(bo);
    amdgpu_device_deinitialize(dev);
    close(fd);
    return 0;
}
