#include <linux/memfd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "shared.h"

#include <gbm.h>
#include "gbm_backend_abi.h"

#include <hybris/gralloc/gralloc.h>
#include <hardware/gralloc.h>

static const struct gbm_core *core;

struct gbm_hybris_device {
    struct gbm_device base;
    struct shmem_global *shmem;
    struct client_slot *slot;
    int sock_fd;
};

static inline struct gbm_hybris_device *gbm_hybris_device(struct gbm_device *device) {
    return (struct gbm_hybris_device *) device;
}

struct gbm_hybris_bo {
    struct gbm_bo base;
    int buf_id;
};

static inline struct gbm_hybris_bo *gbm_hybris_bo(struct gbm_bo *bo) {
    return (struct gbm_hybris_bo *) bo;
}

int memfd_create(const char *name, unsigned int flags);

static int send_request(struct client_slot *state, ReqType type) {
    state->req_type = type;
    sem_post(&state->client_req);
    sem_wait(&state->server_res);
    return 0;
}

static struct gbm_bo * gbm_hybris_bo_create(struct gbm_device *device,
		uint32_t width, uint32_t height, uint32_t format, uint32_t flags,
		const uint64_t *modifiers, const unsigned int count) {
    (void)flags, (void)modifiers, (void)count;
    struct gbm_hybris_bo *bo;
    struct gbm_hybris_device *hybris = gbm_hybris_device(device);
    bo = calloc(1, sizeof(struct gbm_hybris_bo));
    if (!bo) {
        ERR("Failed to allocate memory for GBM BO");
        return NULL;
    }

    bo->base.gbm = device;

    bo->base.v0.width = width;
    bo->base.v0.height = height;
    bo->base.v0.format = format;

    hybris->slot->req.create_buf.width = width;
    hybris->slot->req.create_buf.height = height;
    send_request(hybris->slot, CREATE_BUF);

    bo->buf_id = hybris->slot->res.create_buf.id;
    bo->base.v0.stride = hybris->slot->res.create_buf.stride;

    return &bo->base;
}

static void gbm_hybris_bo_destroy(struct gbm_bo *_bo) {
    struct gbm_hybris_bo *bo = gbm_hybris_bo(_bo);
    struct gbm_hybris_device *hybris = gbm_hybris_device(bo->base.gbm);

    hybris->slot->req.destroy_buf.id = bo->buf_id;
    send_request(hybris->slot, DESTROY_BUF);
    free(bo);
}

static int gbm_hybris_bo_get_fd(struct gbm_bo *_bo) {
    struct gbm_hybris_bo *bo = gbm_hybris_bo(_bo);
    if (bo->buf_id < 0) {
        ERR("Invalid buffer ID: %d", bo->buf_id);
        return -1;
    }
    int fd = memfd_create("buf-id", MFD_CLOEXEC);
    if (write(fd, &bo->buf_id, sizeof(bo->buf_id)) < 0) {
        perror("write to memfd");
        close(fd);
        return -1;
    }

    return fd;
}

uint64_t gbm_hybris_bo_get_modifier(struct gbm_bo *_bo) {
    (void)_bo;
    return 0;
}

uint32_t gbm_hybris_bo_get_offset(struct gbm_bo *bo, int plane) {
    (void)bo, (void)plane;
    return 0;
}

int gbm_hybris_bo_get_plane_count(struct gbm_bo *bo) {
    (void)bo;
    return 1;
}

uint32_t gbm_hybris_bo_get_stride(struct gbm_bo *bo, int plane) {
    (void)plane;
    if (!bo) {
        ERR("Invalid GBM BO pointer");
        return 0;
    }
    return bo ? (uint32_t)(bo->v0.stride) : 0;
}

static void gbm_hybris_device_destroy(struct gbm_device *device) {
    struct gbm_hybris_device *hybris = gbm_hybris_device(device);
    if (hybris == NULL) {
        return;
    }

    send_request(hybris->slot, IM_DONE);
    close(hybris->slot->sock_fd);
    munmap(hybris->shmem, sizeof(*hybris->shmem));
    free(hybris);
}


struct gbm_surface *gbm_hybris_surface_create(struct gbm_device *gbm, uint32_t width, uint32_t height, uint32_t format, uint32_t flags, const uint64_t *modifiers, const unsigned count) {
    (void)gbm, (void)width, (void)height, (void)format, (void)flags, (void)modifiers, (void)count;
    errno = ENOSYS;
    return NULL;
} 

static struct gbm_device *hybris_device_create(int fd, uint32_t gbm_backend_version) {
    struct gbm_hybris_device *device;
    if (gbm_backend_version != GBM_BACKEND_ABI_VERSION) {
        ERR("GBM backend version mismatch: expected %u, got %u",
                GBM_BACKEND_ABI_VERSION, gbm_backend_version);
        return NULL;
    }

    device = calloc(1, sizeof *device);
    if (!device) {
        ERR("Failed to allocate memory for GBM device");
        return NULL;
    }

    int shm_fd = shm_open("hybris-gbm", O_RDWR, 0644);
    if (shm_fd < 0) {
        ERR("Failed to open shared memory for GBM device");
        free(device);
        return NULL;
    }

    struct shmem_global *shm = mmap(NULL, sizeof(*shm), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm == MAP_FAILED) {
        perror("mmap");
        close(shm_fd);
        return NULL;
    }

    device->shmem = shm;

    struct client_slot *ctrl = &shm->control_slot;
    ctrl->pid = getpid();
    ctrl->req_type = ALLOT_SLOT;
    
    sem_post(&ctrl->client_req);
    sem_wait(&ctrl->server_res);
    
    int slot_id = ctrl->res.allot_slot.slot;
    if (slot_id < 0 || slot_id >= 7) {
        ERR("Slot allocation failed");
        return NULL;
    }

    int sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCK_PATH, sizeof(addr.sun_path) - 1);
    if (connect(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        ERR("Failed to connect to hybris socket: %s", strerror(errno));
        close(sock_fd);
        munmap(shm, sizeof(*shm));
        close(shm_fd);
        free(device);
        return NULL;
    }
    
    write(sock_fd, &slot_id, sizeof(slot_id));
    device->sock_fd = sock_fd;

    device->slot = &shm->slots[slot_id];

    device->base.v0.fd = fd;
    device->base.v0.backend_version = gbm_backend_version;
    device->base.v0.bo_create = gbm_hybris_bo_create;
    device->base.v0.bo_get_fd = gbm_hybris_bo_get_fd;
    device->base.v0.bo_get_modifier = gbm_hybris_bo_get_modifier;
    device->base.v0.bo_get_offset = gbm_hybris_bo_get_offset;
    device->base.v0.bo_get_planes = gbm_hybris_bo_get_plane_count;
    device->base.v0.bo_get_stride = gbm_hybris_bo_get_stride;
    device->base.v0.bo_destroy = gbm_hybris_bo_destroy;
    device->base.v0.destroy = gbm_hybris_device_destroy;
    device->base.v0.surface_create = gbm_hybris_surface_create;

    return &device->base;
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
