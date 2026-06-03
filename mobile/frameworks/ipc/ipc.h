/*
 * IPC Framework - Binder-like inter-process communication
 * Uses UNIX domain sockets with binary protocol
 *
 * Protocol format:
 *   [magic:4B][version:u16][type:u8][seq:u32][payload_len:u32][payload:bytes]
 *
 * Types: REQUEST=0, RESPONSE=1, SIGNAL=2, ERROR=3
 */

#ifndef _IPC_H_
#define _IPC_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>

#define IPC_MAGIC          0x554F5353  /* "UOSS" */
#define IPC_VERSION        0x01
#define IPC_BUS_PATH       "/var/run/ipc_bus"
#define IPC_MAX_PAYLOAD    65536
#define IPC_MAX_METHODS    64
#define IPC_MAX_SERVICES   256
#define IPC_MAX_CONNS      128

#define IPC_TYPE_REQUEST   0
#define IPC_TYPE_RESPONSE  1
#define IPC_TYPE_SIGNAL    2
#define IPC_TYPE_ERROR     3

#define IPC_ERR_OK         0
#define IPC_ERR_NOSERVICE  -1
#define IPC_ERR_TIMEOUT    -2
#define IPC_ERR_ACCESS     -3
#define IPC_ERR_NOMEM      -4
#define IPC_ERR_PROTO      -5

typedef uint32_t ipc_seq_t;
typedef uint32_t ipc_len_t;

typedef struct ipc_msg {
    uint32_t    magic;
    uint16_t    version;
    uint8_t     type;
    uint32_t    seq;
    uint32_t    payload_len;
    uint8_t    *payload;
} ipc_msg_t;

typedef struct ipc_method {
    char      name[64];
    void     *(*handler)(void *args, uint32_t args_len, void *out_data, uint32_t *out_len);
    uint32_t  max_args_len;
} ipc_method_t;

typedef struct ipc_service {
    char            name[128];
    ipc_method_t    methods[IPC_MAX_METHODS];
    int             method_count;
    void           *ctx;
} ipc_service_t;

typedef struct ipc_call_args {
    const char *svc_name;
    const char *method;
    const void *args;
    uint32_t    args_len;
    void       *out_data;
    uint32_t   *out_len;
    int         timeout_ms;
} ipc_call_args_t;

/* Initialize IPC system (creates system bus) */
int ipc_init(void);

/* Shutdown IPC */
void ipc_shutdown(void);

/* Register a service with methods */
int ipc_register_service(const char *name, const ipc_method_t *methods, void *ctx);

/* Unregister a service */
int ipc_unregister_service(const char *name);

/* Get a registered service by name */
ipc_service_t *ipc_get_service(const char *name);

/* Synchronous call to service method */
int ipc_call(const char *service_name, const char *method,
             const void *args, uint32_t args_len,
             void *out_data, uint32_t *out_len);

/* Asynchronous call with callback */
typedef void (*ipc_callback_t)(int err, const void *reply, uint32_t reply_len, void *user);
int ipc_call_async(const char *service_name, const char *method,
                   const void *args, uint32_t args_len,
                   ipc_callback_t callback, void *user);

/* System-wide broadcast */
int ipc_broadcast(uint32_t signal_id, const void *data, uint32_t data_len);

/* Serialize/deserialize helpers */
int ipc_msg_encode(ipc_msg_t *msg, uint8_t **out_buf, uint32_t *out_len);
int ipc_msg_decode(const uint8_t *buf, uint32_t len, ipc_msg_t *out_msg);
void ipc_msg_free(ipc_msg_t *msg);

#endif /* _IPC_H_ */
