#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>

#include <linux/memfd.h>

#include <gbm.h>

#include <hybris/gralloc/gralloc.h>

#include <hardware/gralloc.h>

int memfd_create(const char *name, unsigned int flags);

struct gbm_device {
    int dummy;
};

struct gbm_bo {
    struct gbm_device *device;
    uint32_t width;
    uint32_t height;
    uint32_t format;
    uint32_t flags;
    buffer_handle_t handle;
    int stride;
};

static int get_hal_pixel_format(uint32_t gbm_format)
{
    int format;

    switch (gbm_format) {
    case GBM_FORMAT_ABGR8888:
        format = HAL_PIXEL_FORMAT_RGBA_8888;
        break;
    case GBM_FORMAT_XBGR8888:
        format = HAL_PIXEL_FORMAT_RGBX_8888;
        break;
    case GBM_FORMAT_RGB888:
        format = HAL_PIXEL_FORMAT_RGB_888;
        break;
    case GBM_FORMAT_RGB565:
        format = HAL_PIXEL_FORMAT_RGB_565;
        break;
    case GBM_FORMAT_ARGB8888:
        format = HAL_PIXEL_FORMAT_BGRA_8888;
        break;
    case GBM_FORMAT_GR88:
        /* GR88 corresponds to YV12 which is planar */
        format = HAL_PIXEL_FORMAT_YV12;
        break;
    case GBM_FORMAT_ABGR16161616F:
        format = HAL_PIXEL_FORMAT_RGBA_FP16;
        break;
    case GBM_FORMAT_ABGR2101010:
        format = HAL_PIXEL_FORMAT_RGBA_1010102;
        break;
    default:
        format = HAL_PIXEL_FORMAT_RGBA_8888; // Invalid or unsupported format assume RGBA8888
        break;
    }

    return format;
}

struct gbm_device* gbm_create_device(int fd) {
    printf("[libgbm-hybris] gbm_create_device called with fd: %d\n", fd);
    struct gbm_device* device = (struct gbm_device*)malloc(sizeof(struct gbm_device));
    if (device) {
        device->dummy = fd;
    }
    hybris_gralloc_initialize(0);
    return device;
}

void gbm_device_destroy(struct gbm_device* device) {
    printf("[libgbm-hybris] gbm_device_destroy called\n");
    if (device) {
        free(device);
    }
}

struct gbm_bo* gbm_bo_create(struct gbm_device* device, uint32_t width, uint32_t height, uint32_t format, uint32_t flags) {
    printf("[libgbm-hybris1] gbm_bo_create called with width: %u, height: %u, format: %u, flags: %u\n", width, height, format, flags);
    if (!device) {
        fprintf(stderr, "[libgbm-hybris] Invalid GBM device.\n");
        return NULL;
    }

    struct gbm_bo *bo = (struct gbm_bo*)malloc(sizeof(struct gbm_bo));
    if (!bo) {
        fprintf(stderr, "[libgbm-hybris] Failed to allocate memory for GBM buffer object.\n");
        return NULL;
    }

    bo->device = device;
    bo->width = width;
    bo->height = height;
    bo->format = format;
    bo->flags = flags;

    int usage = 0;

    if (flags & GBM_BO_USE_SCANOUT)
        usage |= GRALLOC_USAGE_HW_FB;
    if (flags & GBM_BO_USE_RENDERING)
        usage |= GRALLOC_USAGE_HW_RENDER;
    if (flags & GBM_BO_USE_LINEAR)
        usage |= GRALLOC_USAGE_SW_READ_RARELY | GRALLOC_USAGE_SW_WRITE_RARELY;

    int stride = 0;
    buffer_handle_t handle = NULL;

    int ret = hybris_gralloc_allocate(width, height, get_hal_pixel_format(format), GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_HW_RENDER | GRALLOC_USAGE_HW_COMPOSER, &handle, &stride);
    if (ret != 0) {
        fprintf(stderr, "[libgbm-hybris] hybris_gralloc_allocate failed: %d\n", ret);
        free(bo);
        return NULL;
    }

    bo->handle = handle;
    bo->stride = stride;
    fprintf(stderr, "[libgbm-hybris] Bo created\n");
    return bo;
}

struct gbm_bo *gbm_bo_create_with_modifiers(struct gbm_device *gbm,
                             uint32_t width, uint32_t height,
                             uint32_t format,
                             const uint64_t *modifiers,
                             const unsigned int count)
{
//TBD: it do not work that way :D
   return gbm_bo_create(gbm, width, height, format, 0);
}

void gbm_bo_destroy(struct gbm_bo* bo) {
    printf("[libgbm-hybris] gbm_bo_destroy called\n");
    if (bo) {
        free(bo);
    }
}

uint32_t gbm_bo_get_width(struct gbm_bo* bo) {
    printf("[libgbm-hybris] gbm_bo_get_width called\n");
    return bo ? (uint32_t)(bo->width) : 0;
}

uint32_t gbm_bo_get_height(struct gbm_bo* bo) {
    printf("[libgbm-hybris] gbm_bo_get_height called\n");
    return bo ? (uint32_t)(bo->height) : 0;
}

uint32_t gbm_bo_get_format(struct gbm_bo* bo) {
    printf("[libgbm-hybris] gbm_bo_get_format called\n");
    return 0; // Dummy format
}

uint32_t gbm_bo_get_stride(struct gbm_bo* bo) {
    printf("[libgbm-hybris] gbm_bo_get_stride called\n");
    return 0;
}

uint32_t gbm_bo_get_stride_for_plane(struct gbm_bo *bo, int plane)
{
   printf("[libgbm-hybris] gbm_bo_get_stride_for_plane called\n");
   return 0;
}

uint64_t gbm_bo_get_modifier(struct gbm_bo* bo) {
    printf("[libgbm-hybris] gbm_bo_get_modifier called\n");
    return 0;
}

void* gbm_bo_map(struct gbm_bo *bo, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t flags, uint32_t *stride, void **map_data) {
    printf("[libgbm-hybris] gbm_bo_map called with x: %u, y: %u, width: %u, height: %u, flags: %u\n", x, y, width, height, flags);
    if (stride) {
        *stride = width * 4;
    }
    return malloc(width * height * 4);
}

struct gbm_bo* gbm_surface_lock_front_buffer(struct gbm_surface* surface) {
    printf("[libgbm-hybris] gbm_surface_lock_front_buffer called\n");
    return (struct gbm_bo*)malloc(sizeof(struct gbm_bo));
}

int gbm_device_get_fd(struct gbm_device* device) {
    printf("[libgbm-hybris] gbm_device_get_fd called\n");
    return device ? device->dummy : -1;
}

union gbm_bo_handle gbm_bo_get_handle(struct gbm_bo* bo) {
    printf("[libgbm-hybris] gbm_bo_get_handle called\n");
    union gbm_bo_handle ret = {
        .ptr = 0
    };
    return ret;
}

void gbm_surface_release_buffer(struct gbm_surface* surface, struct gbm_bo* bo) {
    printf("[libgbm-hybris] gbm_surface_release_buffer called\n");
    if (bo) {
        free(bo);
    }
}

void* gbm_bo_get_user_data(struct gbm_bo* bo) {
    printf("[libgbm-hybris] gbm_bo_get_user_data called\n");
    return bo ? (void*)bo : NULL;
}

int gbm_bo_get_fd(struct gbm_bo* bo) {
    if(!bo || !bo->handle) {
        printf("[libgbm-hybris] gbm_bo_get_fd missing bo or bo->handle\n");
        return -1;
    }

    printf("[libgbm-hybris] gbm_bo_get_fd called version: %d numFds: %d numInts: %d\n", bo->handle->version, bo->handle->numFds, bo->handle->numInts);

    int fd = memfd_create("whatever", MFD_CLOEXEC);

    if (fd == -1) {
        printf("[libgbm-hybris] memfd_create failed\n");
        return -1;
    }
    size_t handle_size = sizeof(native_handle_t) + sizeof(int) * (bo->handle->numFds + bo->handle->numInts);
    printf("[libgbm-hybris] fd: %d going to write %d bytes\n", fd, handle_size);
    printf("[libgbm-hybris] data:");
    for(int i=0; i<bo->handle->numFds + bo->handle->numInts; i++) {
        printf(" %d ", bo->handle->data[i]);
    }
    printf("\n");
    if(write(fd, bo->handle, handle_size) != handle_size) {
       printf("[libgbm-hybris] failed to write native_handle_t into mefd\n");
       close(fd);
       return -1;
    }

   return fd;
}

int gbm_bo_get_plane_count(struct gbm_bo *bo)
{
   printf("[libgbm-hybris] gbm_bo_get_plane_count called\n");
   return 0;
}

int gbm_bo_get_fd_for_plane(struct gbm_bo *bo, int plane)
{
   printf("[libgbm-hybris] gbm_bo_get_fd_for_plane called\n");
   return 0;
}

uint32_t gbm_bo_get_offset(struct gbm_bo *bo, int plane)
{
   printf("[libgbm-hybris] gbm_bo_get_offset called\n");
   return 0;
}

struct gbm_device* gbm_bo_get_device(struct gbm_bo* bo) {
    printf("[libgbm-hybris] gbm_bo_get_device called\n");
    return bo ? (struct gbm_device*)bo : NULL;
}

void gbm_bo_set_user_data(struct gbm_bo *bo, void *data, void (*destroy_user_data)(struct gbm_bo *, void *)){
    printf("[libgbm-hybris] gbm_bo_set_user_data called\n");
 //   if (destroy_user_data) {
 //       destroy_user_data(user_data);
 //   }
}

struct gbm_surface *gbm_surface_create(struct gbm_device *gbm, uint32_t width, uint32_t height, uint32_t format, uint32_t flags) {
    printf("[libgbm-hybris] gbm_surface_create called with width: %u, height: %u, format: %u, flags: %u\n", width, height, format, flags);
    return NULL;
}

void gbm_bo_unmap(struct gbm_bo* bo, void* map_data) {
    printf("[libgbm-hybris] gbm_bo_unmap called\n");
    if (map_data) {
        free(map_data);
    }
}

char *gbm_format_get_name(uint32_t gbm_format, struct gbm_format_name_desc *desc)
{
   //gbm_format = gbm_format_canonicalize(gbm_format);
   printf("[libgbm-hybris] gbm_format_get_name called\n");
   desc->name[0] = 0;
   desc->name[1] = 0;
   desc->name[2] = 0;
   desc->name[3] = 0;
   desc->name[4] = 0;

   return desc->name;
}
