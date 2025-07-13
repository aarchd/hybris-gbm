#pragma once
#include <stdint.h>
#include <semaphore.h>

#define SHM_NAME "/hybris-gbm"
#define SOCK_PATH "/tmp/hybris-gbm.sock"

#define ENABLE_LOGGING 1

#if ENABLE_LOGGING
#define LOG(fmt, ...) fprintf(stderr, "[hybris_gbm] " fmt "\n", ##__VA_ARGS__)
#else
#define LOG(fmt, ...) do {} while (0)
#endif

#define ERR(fmt, ...) fprintf(stderr, "[hybris_gbm] " fmt "\n", ##__VA_ARGS__)

typedef enum {
    ALLOT_SLOT,
    CREATE_BUF,
    DESTROY_BUF,
    GET_BUF,
    IM_DONE,
} ReqType;

struct create_buf_req {
    uint32_t width;
    uint32_t height;
};

struct create_buf_res {
    int id;
    uint32_t stride;
};

struct allot_slot_res {
    int slot;
};

struct destroy_buf_req {
    int id;
};

struct get_buf_req {
    int id;
};

struct get_buf_res {
    int ints[7];
};

union request {
    struct create_buf_req create_buf;
    struct destroy_buf_req destroy_buf;
    struct get_buf_req get_buf;
};

union response {
    struct create_buf_res create_buf;
    struct allot_slot_res allot_slot;
    struct get_buf_res get_buf;
};

struct client_slot {
    sem_t client_req;
    sem_t server_res;

    volatile int active;
    pid_t pid;

    ReqType req_type;
    union request req;
    union response res;

    int sock_fd;
};

struct shmem_global {
    struct client_slot control_slot;
    struct client_slot slots[7];
};
