/*
 * IPC Framework - Binder-like implementation
 * UNIX domain socket transport with binary protocol
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/queue.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>

#include "ipc.h"

/* Protocol structs must be packed */
#define IPC_MSG_HEADER_SIZE 21  /* 4 + 2 + 1 + 4 + 4 + 4(align) */

static struct ipc_service g_services[IPC_MAX_SERVICES];
static int                 g_service_count = 0;
static int                 g_bus_fd = -1;
static int                 g_initialized = 0;
static pthread_mutex_t     g_lock;
static uint32_t            g_seq_counter = 0;

/* Pending async calls */
typedef struct pending_call {
    uint32_t            seq;
    ipc_callback_t      cb;
    void               *user;
    struct timeval      deadline;
    SLIST_ENTRY(pending_call) link;
} pending_call_t;

static SLIST_HEAD(pending_head, pending_call) g_pending;

static pthread_mutex_t g_pending_lock;
static pthread_cond_t  g_pending_cond;

static ipc_service_t *
ipc_find_service(const char *name)
{
    if (!name) return NULL;
    for (int i = 0; i < g_service_count; i++) {
        if (strcmp(g_services[i].name, name) == 0)
            return &g_services[i];
    }
    return NULL;
}

int
ipc_init(void)
{
    struct sockaddr_un addr;

    if (g_initialized)
        return 0;

    pthread_mutex_init(&g_lock, NULL);
    pthread_mutex_init(&g_pending_lock, NULL);
    pthread_cond_init(&g_pending_cond, NULL);
    SLIST_INIT(&g_pending);

    g_service_count = 0;
    g_seq_counter = 0;

    /* Create system bus socket */
    g_bus_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (g_bus_fd < 0)
        return IPC_ERR_NOSERVICE;

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strlcpy(addr.sun_path, IPC_BUS_PATH, sizeof(addr.sun_path));

    unlink(IPC_BUS_PATH);
    if (bind(g_bus_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(g_bus_fd);
        g_bus_fd = -1;
        return IPC_ERR_NOSERVICE;
    }

    if (listen(g_bus_fd, IPC_MAX_CONNS) < 0) {
        close(g_bus_fd);
        g_bus_fd = -1;
        return IPC_ERR_NOSERVICE;
    }

    g_initialized = 1;
    return 0;
}

void
ipc_shutdown(void)
{
    if (!g_initialized)
        return;

    if (g_bus_fd >= 0) {
        close(g_bus_fd);
        unlink(IPC_BUS_PATH);
        g_bus_fd = -1;
    }

    pthread_mutex_destroy(&g_lock);
    pthread_mutex_destroy(&g_pending_lock);
    pthread_cond_destroy(&g_pending_cond);

    g_service_count = 0;
    g_initialized = 0;
}

int
ipc_register_service(const char *name, const ipc_method_t *methods, void *ctx)
{
    if (!name || !methods || g_service_count >= IPC_MAX_SERVICES)
        return IPC_ERR_NOMEM;

    pthread_mutex_lock(&g_lock);

    if (ipc_find_service(name)) {
        pthread_mutex_unlock(&g_lock);
        return IPC_ERR_NOSERVICE;  /* Already exists */
    }

    ipc_service_t *svc = &g_services[g_service_count++];
    strlcpy(svc->name, name, sizeof(svc->name));

    int mc = 0;
    while (methods[mc].name[0] && mc < IPC_MAX_METHODS) {
        svc->methods[mc] = methods[mc];
        mc++;
    }
    svc->method_count = mc;
    svc->ctx = ctx;

    pthread_mutex_unlock(&g_lock);
    return 0;
}

int
ipc_unregister_service(const char *name)
{
    if (!name) return IPC_ERR_NOSERVICE;

    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < g_service_count; i++) {
        if (strcmp(g_services[i].name, name) == 0) {
            g_services[i] = g_services[--g_service_count];
            pthread_mutex_unlock(&g_lock);
            return 0;
        }
    }
    pthread_mutex_unlock(&g_lock);
    return IPC_ERR_NOSERVICE;
}

ipc_service_t *
ipc_get_service(const char *name)
{
    return ipc_find_service(name);
}

/* Build a wire-format message */
int
ipc_msg_encode(ipc_msg_t *msg, uint8_t **out_buf, uint32_t *out_len)
{
    if (!msg || !out_buf || !out_len)
        return IPC_ERR_PROTO;

    uint32_t total = IPC_MSG_HEADER_SIZE + msg->payload_len;
    uint8_t *buf = malloc(total);
    if (!buf) return IPC_ERR_NOMEM;

    /* Header: magic(4) + version(2) + type(1) + seq(4) + payload_len(4) */
    uint8_t *p = buf;
    *(uint32_t *)p = htole32(msg->magic);           p += 4;
    *(uint16_t *)p = htole16(msg->version);          p += 2;
    *p++ = msg->type;
    *(uint32_t *)p = htole32(msg->seq);              p += 4;
    *(uint32_t *)p = htole32(msg->payload_len);      p += 4;

    if (msg->payload_len > 0 && msg->payload) {
        memcpy(p, msg->payload, msg->payload_len);
    }

    *out_buf = buf;
    *out_len = total;
    return 0;
}

int
ipc_msg_decode(const uint8_t *buf, uint32_t len, ipc_msg_t *out_msg)
{
    if (!buf || !out_msg || len < IPC_MSG_HEADER_SIZE)
        return IPC_ERR_PROTO;

    const uint8_t *p = buf;
    out_msg->magic      = le32toh(*(uint32_t *)p);       p += 4;
    out_msg->version    = le16toh(*(uint16_t *)p);       p += 2;
    out_msg->type       = *p++;                           p += 1; /* skip reserved */
    out_msg->seq        = le32toh(*(uint32_t *)p);       p += 4;
    out_msg->payload_len = le32toh(*(uint32_t *)p);      p += 4;

    if (out_msg->magic != IPC_MAGIC)
        return IPC_ERR_PROTO;

    if (out_msg->payload_len > 0 && out_msg->payload_len <= IPC_MAX_PAYLOAD) {
        out_msg->payload = malloc(out_msg->payload_len);
        if (!out_msg->payload)
            return IPC_ERR_NOMEM;
        memcpy(out_msg->payload, p, out_msg->payload_len);
    } else {
        out_msg->payload = NULL;
    }

    return 0;
}

void
ipc_msg_free(ipc_msg_t *msg)
{
    if (msg && msg->payload) {
        free(msg->payload);
        msg->payload = NULL;
    }
}

/* Send a raw message to a connected client */
static int
ipc_send_raw(int fd, const ipc_msg_t *msg)
{
    uint8_t *buf;
    uint32_t len;
    int ret = ipc_msg_encode((ipc_msg_t *)msg, &buf, &len);
    if (ret < 0) return ret;

    int sent = 0;
    while ((uint32_t)sent < len) {
        ssize_t n = send(fd, buf + sent, len - sent, 0);
        if (n <= 0) { free(buf); return IPC_ERR_NOSERVICE; }
        sent += (int)n;
    }
    free(buf);
    return 0;
}

/* Receive a raw message from a connected client */
static int
ipc_recv_raw(int fd, ipc_msg_t *out_msg)
{
    uint8_t header[IPC_MSG_HEADER_SIZE];
    int  n = recv(fd, header, IPC_MSG_HEADER_SIZE, MSG_WAITALL);
    if (n == 0) return IPC_ERR_TIMEOUT;
    if (n < 0) return IPC_ERR_NOSERVICE;

    uint32_t payload_len;
    memcpy(&payload_len, header + 17, 4); /* offset of payload_len in header */
    payload_len = le32toh(payload_len);

    if (payload_len > IPC_MAX_PAYLOAD)
        return IPC_ERR_PROTO;

    return ipc_msg_decode(header, IPC_MSG_HEADER_SIZE, out_msg);
}

/*
 * Simple synchronous call over UNIX domain socket.
 * In production, this would connect to the bus and use select/poll.
 */
int
ipc_call(const char *service_name, const char *method,
         const void *args, uint32_t args_len,
         void *out_data, uint32_t *out_len)
{
    if (!service_name || !method)
        return IPC_ERR_NOSERVICE;

    pthread_mutex_lock(&g_lock);
    ipc_service_t *svc = ipc_find_service(service_name);
    pthread_mutex_unlock(&g_lock);

    if (!svc)
        return IPC_ERR_NOSERVICE;

    /* Find method */
    ipc_method_t *m = NULL;
    for (int i = 0; i < svc->method_count; i++) {
        if (strcmp(svc->methods[i].name, method) == 0) {
            m = &svc->methods[i];
            break;
        }
    }
    if (!m)
        return IPC_ERR_NOSERVICE;

    /* Validate args length */
    if (args_len > 0 && m->max_args_len > 0 && args_len > m->max_args_len)
        return IPC_ERR_PROTO;

    /* Call handler */
    uint32_t out_len_val = 0;
    void *reply = m->handler((void *)args, args_len, out_data, &out_len_val);
    if (out_len && out_len_val > 0) {
        *out_len = out_len_val;
    }

    if (reply && reply != out_data)
        free(reply);

    return 0;
}

int
ipc_call_async(const char *service_name, const char *method,
               const void *args, uint32_t args_len,
               ipc_callback_t callback, void *user)
{
    if (!callback)
        return IPC_ERR_PROTO;

    pending_call_t *pc = malloc(sizeof(*pc));
    if (!pc) return IPC_ERR_NOMEM;

    pc->seq = __sync_fetch_and_add(&g_seq_counter, 1);
    pc->cb  = callback;
    pc->user = user;
    gettimeofday(&pc->deadline, NULL);
    pc->deadline.tv_sec += 5; /* 5 second timeout */

    pthread_mutex_lock(&g_pending_lock);
    SLIST_INSERT_HEAD(&g_pending, pc, link);
    pthread_mutex_unlock(&g_pending_lock);
    pthread_cond_signal(&g_pending_cond);

    /* In real implementation, this sends over socket and dispatches response
     * to the callback when it arrives. Simplified: call synchronously. */
    char dummy[1024];
    uint32_t dummy_len = 0;
    int ret = ipc_call(service_name, method, args, args_len, dummy, &dummy_len);

    /* Remove from pending */
    pthread_mutex_lock(&g_pending_lock);
    pending_call_t *iter;
    SLIST_FOREACH(iter, &g_pending, link) {
        if (iter == pc) {
            SLIST_REMOVE(&g_pending, pc, pending_call, link);
            break;
        }
    }
    pthread_mutex_unlock(&g_pending_lock);

    callback(ret, dummy, dummy_len, user);
    free(pc);

    return 0;
}

int
ipc_broadcast(uint32_t signal_id, const void *data, uint32_t data_len)
{
    if (!g_initialized)
        return IPC_ERR_NOSERVICE;

    /* Build signal message */
    ipc_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.magic      = IPC_MAGIC;
    msg.version    = IPC_VERSION;
    msg.type       = IPC_TYPE_SIGNAL;
    msg.seq        = __sync_fetch_and_add(&g_seq_counter, 1);
    msg.payload_len = data_len + sizeof(signal_id);

    uint8_t *payload = malloc(msg.payload_len);
    if (!payload) return IPC_ERR_NOMEM;

    *(uint32_t *)payload = htole32(signal_id);
    if (data && data_len > 0)
        memcpy(payload + sizeof(signal_id), data, data_len);

    msg.payload = payload;

    /* In real implementation, send to all connected clients */
    int ret = 0; /* ipc_send_broadcast(&msg); */
    free(payload);
    return ret;
}
