#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <linux/memfd.h>

#include <gbm.h>
#include "gbm_backend_abi.h"

#include <hybris/gralloc/gralloc.h>
#include <hardware/gralloc.h>

static const struct gbm_core *core;

struct gbm_hybris_bo {
    struct gbm_bo base;
    const native_handle_t *handle;
};

static inline struct gbm_hybris_bo *gbm_hybris_bo(struct gbm_bo *bo) {
    return (struct gbm_hybris_bo *) bo;
}

int memfd_create(const char *name, unsigned int flags);

static struct gbm_bo * gbm_hybris_bo_create(struct gbm_device *device,
		uint32_t width, uint32_t height, uint32_t format, uint32_t flags,
		const uint64_t *modifiers, const unsigned int count) {
    (void)flags, (void)modifiers, (void)count;
    struct gbm_hybris_bo *bo;
    bo = calloc(1, sizeof(struct gbm_hybris_bo));
    if (!bo) {
        fprintf(stderr, "[hybris-gbm] Failed to allocate memory for GBM BO\n");
        errno = ENOMEM;
        return NULL;
    }

    bo->base.gbm = device;

    bo->base.v0.width = width;
    bo->base.v0.height = height;
    bo->base.v0.format = format;

    int ret = hybris_gralloc_allocate(width, height, 
                                      HAL_PIXEL_FORMAT_RGBA_8888, 
                                      GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_HW_RENDER | GRALLOC_USAGE_HW_COMPOSER, 
                                      &bo->handle, &bo->base.v0.stride);

    if (ret != 0) {
        fprintf(stderr, "[hybris-gbm] Failed to allocate gralloc buffer: %d\n", ret);
        free(bo);
        return NULL;
    }

    printf("[hybris-gbm] Allocated gralloc buffer: width=%u, height=%u, format=%u, stride=%u\n",
        width, height, format, bo->base.v0.stride);

    return &bo->base;
}

static void gbm_hybris_bo_destroy(struct gbm_bo *_bo) {
    struct gbm_hybris_bo *bo = gbm_hybris_bo(_bo);
    if (!bo) {
        fprintf(stderr, "[hybris-gbm] Invalid GBM BO pointer\n");
        errno = EINVAL;
        return;
    }

    // i assume this should be 1 since we did hybris_gralloc_allocate not import??
    hybris_gralloc_release(bo->handle, 1);

    free(bo);
}

static int gbm_hybris_bo_get_fd(struct gbm_bo *_bo) {
    (void)_bo;
    errno = ENOSYS;
    return -1;
}

static int gbm_hybris_bo_get_fd_for_plane(struct gbm_bo *_bo, int plane) {
    struct gbm_hybris_bo *bo = gbm_hybris_bo(_bo);
    if (!bo || !bo->handle) {
        fprintf(stderr, "[hybris-gbm] Invalid GBM BO pointer or handle\n");
        errno = EINVAL;
        return -1;
    }

    if (plane == 0) {
        return dup(bo->handle->data[0]);
    }

    if (plane == 1) {
        int fd = memfd_create("bo-plane-1", MFD_CLOEXEC | MFD_ALLOW_SEALING);
        if (fd < 0) {
            fprintf(stderr, "[hybris-gbm] Failed to create memfd for plane 1: %s\n", strerror(errno));
            return -1;
        }
        if(write(fd, &bo->handle->data[1], sizeof(int) * 7) < 0) {
            fprintf(stderr, "[hybris-gbm] Failed to write plane data to memfd: %s\n", strerror(errno));
            close(fd);
            return -1;
        }
        return fd; // caller must close fd itself
    }

    errno = EINVAL;
    return -1;
}

static uint64_t gbm_hybris_bo_get_modifier(struct gbm_bo *_bo) {
    (void)_bo;
    return 0;
}

static uint32_t gbm_hybris_bo_get_offset(struct gbm_bo *bo, int plane) {
    (void)bo, (void)plane;
    return 0;
}

static int gbm_hybris_bo_get_plane_count(struct gbm_bo *bo) {
    (void)bo;
    return 2;
}

static uint32_t gbm_hybris_bo_get_stride(struct gbm_bo *bo, int plane) {
    (void)plane;
    if (!bo) {
        fprintf(stderr, "[hybris-gbm] Invalid GBM BO pointer\n");
        errno = EINVAL;
        return 0;
    }
    return bo ? (uint32_t)(bo->v0.stride) : 0;
}

static void gbm_hybris_device_destroy(struct gbm_device *device) {
    if (!device) {
        fprintf(stderr, "[hybris-gbm] Invalid GBM device pointer\n");
        errno = EINVAL;
        return;
    }
    free(device);
}


static struct gbm_surface *gbm_hybris_surface_create(struct gbm_device *gbm, uint32_t width, uint32_t height, uint32_t format, uint32_t flags, const uint64_t *modifiers, const unsigned count) {
    (void)gbm, (void)width, (void)height, (void)format, (void)flags, (void)modifiers, (void)count;
    errno = ENOSYS;
    return NULL;
} 

static struct gbm_device *hybris_device_create(int fd, uint32_t gbm_backend_version) {
    struct gbm_device *device;
    if (gbm_backend_version != GBM_BACKEND_ABI_VERSION) {
        fprintf(stderr, "[hybris-gbm] GBM backend version mismatch: expected %u, got %u\n",
                GBM_BACKEND_ABI_VERSION, gbm_backend_version);
        return NULL;
    }

    device = calloc(1, sizeof *device);
    if (!device) {
        fprintf(stderr, "[hybris-gbm] Failed to allocate memory for GBM device\n");
        errno = ENOMEM;
        return NULL;
    }

    device->v0.fd = fd;
    device->v0.backend_version = gbm_backend_version;
    device->v0.bo_create = gbm_hybris_bo_create;
    device->v0.bo_get_fd = gbm_hybris_bo_get_fd;
    device->v0.bo_get_modifier = gbm_hybris_bo_get_modifier;
    device->v0.bo_get_offset = gbm_hybris_bo_get_offset;
    device->v0.bo_get_planes = gbm_hybris_bo_get_plane_count;
    device->v0.bo_get_plane_fd = gbm_hybris_bo_get_fd_for_plane;
    device->v0.bo_get_stride = gbm_hybris_bo_get_stride;
    device->v0.bo_destroy = gbm_hybris_bo_destroy;
    device->v0.destroy = gbm_hybris_device_destroy;
    device->v0.surface_create = gbm_hybris_surface_create;

    return device;
}

struct gbm_backend gbm_hybris_backend = {
    .v0.backend_version = GBM_BACKEND_ABI_VERSION,
    .v0.backend_name = "hybris",
    .v0.create_device = hybris_device_create,
};

struct gbm_backend * gbmint_get_backend(const struct gbm_core *gbm_core);

struct gbm_backend * gbmint_get_backend(const struct gbm_core *gbm_core) {
   core = gbm_core;
   return &gbm_hybris_backend;
}
