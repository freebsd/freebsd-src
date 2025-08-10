/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/apputils/net-server.c - Network code for krb5 servers (kdc, kadmind) */
/*
 * Copyright 1990,2000,2007,2008,2009,2010,2016 by the Massachusetts Institute
 * of Technology.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#include "k5-int.h"
#include "adm_proto.h"
#include <sys/ioctl.h>
#include <syslog.h>

#include <stddef.h>
#include "port-sockets.h"
#include "socket-utils.h"

#include <gssrpc/rpc.h>

#ifdef HAVE_NETINET_IN_H
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#ifdef HAVE_SYS_SOCKIO_H
/* for SIOCGIFCONF, etc. */
#include <sys/sockio.h>
#endif
#include <sys/time.h>
#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#include <arpa/inet.h>

#ifndef ARPHRD_ETHER /* OpenBSD breaks on multiple inclusions */
#include <net/if.h>
#endif

#ifdef HAVE_SYS_FILIO_H
#include <sys/filio.h>          /* FIONBIO */
#endif

#include "fake-addrinfo.h"
#include "net-server.h"
#include <signal.h>
#include <netdb.h>

#include "udppktinfo.h"

/* List of systemd socket activation addresses and socket types. */
struct sockact_list {
    size_t nsockets;
    struct {
        struct sockaddr_storage addr;
        int type;
    } *fds;
};

/* When systemd socket activation is used, caller-provided sockets begin at
 * file descriptor 3. */
const int SOCKACT_START = 3;

/* XXX */
#define KDC5_NONET                               (-1779992062L)

static int stream_data_counter;
static int max_stream_data_connections = 45;

static int
setreuseaddr(int sock, int value)
{
    int st;

    st = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));
    if (st)
        return st;
#if defined(SO_REUSEPORT) && defined(__APPLE__)
    /* macOS experimentally needs this flag as well to avoid conflicts between
     * recently exited server processes and new ones. */
    st = setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &value, sizeof(value));
    if (st)
        return st;
#endif
    return 0;
}

#if defined(IPV6_V6ONLY)
static int
setv6only(int sock, int value)
{
    return setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &value, sizeof(value));
}
#endif

/* KDC data.  */

enum conn_type {
    CONN_UDP, CONN_TCP_LISTENER, CONN_TCP, CONN_RPC_LISTENER, CONN_RPC,
    CONN_UNIXSOCK_LISTENER, CONN_UNIXSOCK
};

static const char *const conn_type_names[] = {
    [CONN_UDP] = "UDP",
    [CONN_TCP_LISTENER] = "TCP listener",
    [CONN_TCP] = "TCP",
    [CONN_RPC_LISTENER] = "RPC listener",
    [CONN_RPC] = "RPC",
    [CONN_UNIXSOCK_LISTENER] = "UNIX domain socket listener",
    [CONN_UNIXSOCK] = "UNIX domain socket"
};

enum bind_type {
    UDP, TCP, RPC, UNX
};

static const char *const bind_type_names[] = {
    [UDP] = "UDP",
    [TCP] = "TCP",
    [RPC] = "RPC",
    [UNX] = "UNIXSOCK",
};

/* Per-connection info.  */
struct connection {
    void *handle;
    const char *prog;
    enum conn_type type;

    /* Connection fields (TCP or RPC) */
    struct sockaddr_storage addr_s;
    socklen_t addrlen;
    char addrbuf[128];

    /* Incoming data (TCP) */
    size_t bufsiz;
    size_t offset;
    char *buffer;
    size_t msglen;

    /* Outgoing data (TCP) */
    krb5_data *response;
    unsigned char lenbuf[4];
    sg_buf sgbuf[2];
    sg_buf *sgp;
    int sgnum;

    /* Crude denial-of-service avoidance support (TCP or RPC) */
    time_t start_time;

    /* RPC-specific fields */
    SVCXPRT *transp;
    int rpc_force_close;
};

#define SET(TYPE) struct { TYPE *data; size_t n, max; }

/* Start at the top and work down -- this should allow for deletions
   without disrupting the iteration, since we delete by overwriting
   the element to be removed with the last element.  */
#define FOREACH_ELT(set,idx,vvar)                                       \
    for (idx = set.n-1; idx >= 0 && (vvar = set.data[idx], 1); idx--)

#define GROW_SET(set, incr, tmpptr)                                     \
    ((set.max + incr < set.max                                          \
      || ((set.max + incr) * sizeof(set.data[0]) / sizeof(set.data[0])  \
          != set.max + incr))                                           \
     ? 0                         /* overflow */                         \
     : ((tmpptr = realloc(set.data,                                     \
                          (set.max + incr) * sizeof(set.data[0])))      \
        ? (set.data = tmpptr, set.max += incr, 1)                       \
        : 0))

/* 1 = success, 0 = failure */
#define ADD(set, val, tmpptr)                           \
    ((set.n < set.max || GROW_SET(set, 10, tmpptr))     \
     ? (set.data[set.n++] = val, 1)                     \
     : 0)

#define DEL(set, idx)                           \
    (set.data[idx] = set.data[--set.n], 0)

#define FREE_SET_DATA(set)                                      \
    (free(set.data), set.data = 0, set.max = 0, set.n = 0)

/*
 * N.B.: The Emacs cc-mode indentation code seems to get confused if
 * the macro argument here is one word only.  So use "unsigned short"
 * instead of the "u_short" we were using before.
 */
struct rpc_svc_data {
    u_long prognum;
    u_long versnum;
    void (*dispatch)(struct svc_req *, SVCXPRT *);
};

struct bind_address {
    char *address;
    u_short port;
    enum bind_type type;
    struct rpc_svc_data rpc_svc_data;
};

static SET(verto_ev *) events;
static SET(struct bind_address) bind_addresses;

verto_ctx *
loop_init(verto_ev_type types)
{
    types |= VERTO_EV_TYPE_IO;
    types |= VERTO_EV_TYPE_SIGNAL;
    types |= VERTO_EV_TYPE_TIMEOUT;
    return verto_default(NULL, types);
}

static void
do_break(verto_ctx *ctx, verto_ev *ev)
{
    krb5_klog_syslog(LOG_DEBUG, _("Got signal to request exit"));
    verto_break(ctx);
}

struct sighup_context {
    void *handle;
    void (*reset)(void *);
};

static void
do_reset(verto_ctx *ctx, verto_ev *ev)
{
    struct sighup_context *sc = (struct sighup_context*) verto_get_private(ev);

    krb5_klog_syslog(LOG_DEBUG, _("Got signal to reset"));
    krb5_klog_reopen(get_context(sc->handle));
    if (sc->reset)
        sc->reset(sc->handle);
}

static void
free_sighup_context(verto_ctx *ctx, verto_ev *ev)
{
    free(verto_get_private(ev));
}

krb5_error_code
loop_setup_signals(verto_ctx *ctx, void *handle, void (*reset)(void *))
{
    struct sighup_context *sc;
    verto_ev *ev;

    if (!verto_add_signal(ctx, VERTO_EV_FLAG_PERSIST, do_break, SIGINT)  ||
        !verto_add_signal(ctx, VERTO_EV_FLAG_PERSIST, do_break, SIGTERM) ||
        !verto_add_signal(ctx, VERTO_EV_FLAG_PERSIST, do_break, SIGQUIT) ||
        !verto_add_signal(ctx, VERTO_EV_FLAG_PERSIST, VERTO_SIG_IGN, SIGPIPE))
        return ENOMEM;

    ev = verto_add_signal(ctx, VERTO_EV_FLAG_PERSIST, do_reset, SIGHUP);
    if (!ev)
        return ENOMEM;

    sc = malloc(sizeof(*sc));
    if (!sc)
        return ENOMEM;

    sc->handle = handle;
    sc->reset = reset;
    verto_set_private(ev, sc, free_sighup_context);
    return 0;
}

/*
 * Add a bind address to the loop.
 *
 * Arguments:
 * - address
 *      An address string, hostname, or UNIX socket path.
 *      Pass NULL to use the wildcard address for IP sockets.
 * - port
 *      What port the socket should be set to (for IPv4 or IPv6).
 * - type
 *      bind_type for the socket.
 * - rpc_data
 *      For RPC addresses, the svc_register() arguments to use when TCP
 *      connections are created.  Ignored for other types.
 */
static krb5_error_code
loop_add_address(const char *address, int port, enum bind_type type,
                 struct rpc_svc_data *rpc_data)
{
    struct bind_address addr, val;
    int i;
    void *tmp;
    char *addr_copy = NULL;

    assert(!(type == RPC && rpc_data == NULL));

    /* Make sure a valid port number was passed. */
    if (port < 0 || port > 65535) {
        krb5_klog_syslog(LOG_ERR, _("Invalid port %d"), port);
        return EINVAL;
    }

    /* Check for conflicting addresses. */
    FOREACH_ELT(bind_addresses, i, val) {
        if (type != val.type || port != val.port)
            continue;

        /* If a wildcard address is being added, make sure to remove any direct
         * addresses. */
        if (address == NULL && val.address != NULL) {
            krb5_klog_syslog(LOG_DEBUG,
                             _("Removing address %s since wildcard address"
                               " is being added"),
                             val.address);
            free(val.address);
            DEL(bind_addresses, i);
        } else if (val.address == NULL || !strcmp(address, val.address)) {
            krb5_klog_syslog(LOG_DEBUG,
                             _("Address already added to server"));
            return 0;
        }
    }

    /* Copy the address if it is specified. */
    if (address != NULL) {
        addr_copy = strdup(address);
        if (addr_copy == NULL)
            return ENOMEM;
    }

    /* Add the new address to bind_addresses. */
    memset(&addr, 0, sizeof(addr));
    addr.address = addr_copy;
    addr.port = port;
    addr.type = type;
    if (rpc_data != NULL)
        addr.rpc_svc_data = *rpc_data;
    if (!ADD(bind_addresses, addr, tmp)) {
        free(addr_copy);
        return ENOMEM;
    }

    return 0;
}

/*
 * Add bind addresses to the loop.
 *
 * Arguments:
 *
 * - addresses
 *      A string for the addresses.  Pass NULL to use the wildcard address.
 *      Supported delimiters can be found in ADDRESSES_DELIM.  Addresses are
 *      parsed with k5_parse_host_name().
 * - default_port
 *      What port the socket should be set to if not specified in addresses.
 * - type
 *      bind_type for the socket.
 * - rpc_data
 *      For RPC addresses, the svc_register() arguments to use when TCP
 *      connections are created.  Ignored for other types.
 */
static krb5_error_code
loop_add_addresses(const char *addresses, int default_port,
                   enum bind_type type, struct rpc_svc_data *rpc_data)
{
    krb5_error_code ret = 0;
    char *addresses_copy = NULL, *host = NULL, *saveptr, *addr;
    int port;

    /* If no addresses are set, add a wildcard address. */
    if (addresses == NULL)
        return loop_add_address(NULL, default_port, type, rpc_data);

    /* Copy the addresses string before using strtok(). */
    addresses_copy = strdup(addresses);
    if (addresses_copy == NULL) {
        ret = ENOMEM;
        goto cleanup;
    }

    /* Loop through each address in the string and add it to the loop. */
    addr = strtok_r(addresses_copy, ADDRESSES_DELIM, &saveptr);
    for (; addr != NULL; addr = strtok_r(NULL, ADDRESSES_DELIM, &saveptr)) {
        if (type == UNX) {
            /* Skip non-pathnames when binding UNIX domain sockets. */
            if (*addr != '/')
                continue;
            ret = loop_add_address(addr, 0, type, rpc_data);
            if (ret)
                goto cleanup;
            continue;
        } else if (*addr == '/') {
            /* Skip pathnames when not binding UNIX domain sockets. */
            continue;
        }

        /* Parse the host string. */
        ret = k5_parse_host_string(addr, default_port, &host, &port);
        if (ret)
            goto cleanup;

        ret = loop_add_address(host, port, type, rpc_data);
        if (ret)
            goto cleanup;

        free(host);
        host = NULL;
    }

    ret = 0;
cleanup:
    free(addresses_copy);
    free(host);
    return ret;
}

krb5_error_code
loop_add_udp_address(int default_port, const char *addresses)
{
    return loop_add_addresses(addresses, default_port, UDP, NULL);
}

krb5_error_code
loop_add_tcp_address(int default_port, const char *addresses)
{
    return loop_add_addresses(addresses, default_port, TCP, NULL);
}

krb5_error_code
loop_add_rpc_service(int default_port, const char *addresses, u_long prognum,
                     u_long versnum,
                     void (*dispatchfn)(struct svc_req *, SVCXPRT *))
{
    struct rpc_svc_data svc;

    svc.prognum = prognum;
    svc.versnum = versnum;
    svc.dispatch = dispatchfn;
    return loop_add_addresses(addresses, default_port, RPC, &svc);
}

krb5_error_code
loop_add_unix_socket(const char *socket_paths)
{
    /* There is no wildcard or default UNIX domain socket. */
    if (socket_paths == NULL)
        return 0;

    return loop_add_addresses(socket_paths, 0, UNX, NULL);
}

#define USE_AF AF_INET
#define USE_TYPE SOCK_DGRAM
#define USE_PROTO 0
#define SOCKET_ERRNO errno
#include "foreachaddr.h"

static void
free_connection(struct connection *conn)
{
    if (!conn)
        return;
    if (conn->response)
        krb5_free_data(get_context(conn->handle), conn->response);
    if (conn->buffer)
        free(conn->buffer);
    if (conn->type == CONN_RPC_LISTENER && conn->transp != NULL)
        svc_destroy(conn->transp);
    free(conn);
}

static void
remove_event_from_set(verto_ev *ev)
{
    verto_ev *tmp;
    int i;

    /* Remove the event from the events. */
    FOREACH_ELT(events, i, tmp)
        if (tmp == ev) {
            DEL(events, i);
            break;
        }
}

static void
free_socket(verto_ctx *ctx, verto_ev *ev)
{
    struct connection *conn = NULL;
    fd_set fds;
    int fd;

    remove_event_from_set(ev);

    fd = verto_get_fd(ev);
    conn = verto_get_private(ev);

    /* Close the file descriptor. */
    krb5_klog_syslog(LOG_INFO, _("closing down fd %d"), fd);
    if (fd >= 0 && (!conn || conn->type != CONN_RPC || conn->rpc_force_close))
        close(fd);

    /* Free the connection struct. */
    if (conn) {
        switch (conn->type) {
        case CONN_RPC:
            if (conn->rpc_force_close) {
                FD_ZERO(&fds);
                FD_SET(fd, &fds);
                svc_getreqset(&fds);
                if (FD_ISSET(fd, &svc_fdset)) {
                    krb5_klog_syslog(LOG_ERR,
                                     _("descriptor %d closed but still "
                                       "in svc_fdset"),
                                     fd);
                }
            }
            /* Fall through. */
        case CONN_TCP:
        case CONN_UNIXSOCK:
            stream_data_counter--;
            break;
        default:
            break;
        }

        free_connection(conn);
    }
}

static verto_ev *
make_event(verto_ctx *ctx, verto_ev_flag flags, verto_callback callback,
           int sock, struct connection *conn)
{
    verto_ev *ev;
    void *tmp;

    ev = verto_add_io(ctx, flags, callback, sock);
    if (!ev) {
        com_err(conn->prog, ENOMEM, _("cannot create io event"));
        return NULL;
    }

    if (!ADD(events, ev, tmp)) {
        com_err(conn->prog, ENOMEM, _("cannot save event"));
        verto_del(ev);
        return NULL;
    }

    verto_set_private(ev, conn, free_socket);
    return ev;
}

static krb5_error_code
add_fd(int sock, enum conn_type conntype, verto_ev_flag flags, void *handle,
       const char *prog, verto_ctx *ctx, verto_callback callback,
       verto_ev **ev_out)
{
    struct connection *newconn;

    *ev_out = NULL;

#ifndef _WIN32
    if (sock >= FD_SETSIZE) {
        com_err(prog, 0, _("file descriptor number %d too high"), sock);
        return EMFILE;
    }
#endif
    newconn = malloc(sizeof(*newconn));
    if (newconn == NULL) {
        com_err(prog, ENOMEM,
                _("cannot allocate storage for connection info"));
        return ENOMEM;
    }
    memset(newconn, 0, sizeof(*newconn));
    newconn->handle = handle;
    newconn->prog = prog;
    newconn->type = conntype;

    *ev_out = make_event(ctx, flags, callback, sock, newconn);
    return 0;
}

static void process_packet(verto_ctx *ctx, verto_ev *ev);
static void accept_stream_connection(verto_ctx *ctx, verto_ev *ev);
static void process_stream_connection_read(verto_ctx *ctx, verto_ev *ev);
static void process_stream_connection_write(verto_ctx *ctx, verto_ev *ev);
static void accept_rpc_connection(verto_ctx *ctx, verto_ev *ev);
static void process_rpc_connection(verto_ctx *ctx, verto_ev *ev);

/*
 * Create a socket and bind it to addr.  Ensure the socket will work with
 * select().  Set the socket cloexec, reuseaddr, and if applicable v6-only.
 * Does not call listen().  On failure, log an error and return an error code.
 */
static krb5_error_code
create_server_socket(struct sockaddr *addr, int type, const char *prog,
                     int *fd_out)
{
    int sock, e;
    char addrbuf[128];

    *fd_out = -1;

    if (addr->sa_family == AF_UNIX)
        (void)unlink(sa2sun(addr)->sun_path);
    sock = socket(addr->sa_family, type, 0);
    if (sock == -1) {
        e = errno;
        k5_print_addr_port(addr, addrbuf, sizeof(addrbuf));
        com_err(prog, e, _("Cannot create TCP server socket on %s"), addrbuf);
        return e;
    }
    set_cloexec_fd(sock);

#ifndef _WIN32                  /* Windows FD_SETSIZE is a count. */
    if (sock >= FD_SETSIZE) {
        close(sock);
        k5_print_addr_port(addr, addrbuf, sizeof(addrbuf));
        com_err(prog, 0, _("TCP socket fd number %d (for %s) too high"),
                sock, addrbuf);
        return EMFILE;
    }
#endif

    if (setreuseaddr(sock, 1) < 0)
        com_err(prog, errno, _("Cannot enable SO_REUSEADDR on fd %d"), sock);

    if (addr->sa_family == AF_INET6) {
#ifdef IPV6_V6ONLY
        if (setv6only(sock, 1)) {
            com_err(prog, errno, _("setsockopt(%d,IPV6_V6ONLY,1) failed"),
                    sock);
        } else {
            com_err(prog, 0, _("setsockopt(%d,IPV6_V6ONLY,1) worked"), sock);
        }
#else
        krb5_klog_syslog(LOG_INFO, _("no IPV6_V6ONLY socket option support"));
#endif /* IPV6_V6ONLY */
    }

    if (bind(sock, addr, sa_socklen(addr)) == -1) {
        e = errno;
        k5_print_addr_port(addr, addrbuf, sizeof(addrbuf));
        com_err(prog, e, _("Cannot bind server socket on %s"), addrbuf);
        close(sock);
        return e;
    }

    *fd_out = sock;
    return 0;
}

static const int one = 1;

static int
setnbio(int sock)
{
    return ioctlsocket(sock, FIONBIO, (const void *)&one);
}

static int
setkeepalive(int sock)
{
    return setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof(one));
}

static int
setnolinger(int s)
{
    static const struct linger ling = { 0, 0 };
    return setsockopt(s, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling));
}

/* An enum map to socket families for each bind_type. */
static const int bind_socktypes[] =
{
    [UDP] = SOCK_DGRAM,
    [TCP] = SOCK_STREAM,
    [RPC] = SOCK_STREAM,
    [UNX] = SOCK_STREAM
};

/* An enum map containing conn_type (for struct connection) for each
 * bind_type.  */
static const enum conn_type bind_conn_types[] =
{
    [UDP] = CONN_UDP,
    [TCP] = CONN_TCP_LISTENER,
    [RPC] = CONN_RPC_LISTENER,
    [UNX] = CONN_UNIXSOCK_LISTENER
};

/* If any systemd socket activation fds are indicated by the environment, set
 * them close-on-exec and put their addresses and socket types into *list. */
static void
init_sockact_list(struct sockact_list *list)
{
    const char *v;
    char *end;
    long lpid;
    int fd;
    size_t nfds, i;
    socklen_t slen;

    list->nsockets = 0;
    list->fds = NULL;

    /* Check if LISTEN_FDS is meant for this process. */
    v = getenv("LISTEN_PID");
    if (v == NULL)
        return;
    lpid = strtol(v, &end, 10);
    if (end == NULL || end == v || *end != '\0' || lpid != getpid())
        return;

    /* Get the number of activated sockets. */
    v = getenv("LISTEN_FDS");
    if (v == NULL)
        return;
    nfds = strtoul(v, &end, 10);
    if (end == NULL || end == v || *end != '\0')
        return;
    if (nfds == 0 || nfds > (size_t)INT_MAX - SOCKACT_START)
        return;

    list->fds = calloc(nfds, sizeof(*list->fds));
    if (list->fds == NULL)
        return;

    for (i = 0; i < nfds; i++) {
        fd = i + SOCKACT_START;
        set_cloexec_fd(fd);
        slen = sizeof(list->fds[i].addr);
        (void)getsockname(fd, ss2sa(&list->fds[i].addr), &slen);
        slen = sizeof(list->fds[i].type);
        (void)getsockopt(fd, SOL_SOCKET, SO_TYPE, &list->fds[i].type, &slen);
    }

    list->nsockets = nfds;
}

/* Release any storage used by *list. */
static void
fini_sockact_list(struct sockact_list *list)
{
    free(list->fds);
    list->fds = NULL;
    list->nsockets = 0;
}

/* If sa matches an address in *list, return the associated file descriptor and
 * clear the address from *list.  Otherwise return -1. */
static int
find_sockact(struct sockact_list *list, const struct sockaddr *sa, int type)
{
    size_t i;

    for (i = 0; i < list->nsockets; i++) {
        if (list->fds[i].type == type &&
            sa_equal(ss2sa(&list->fds[i].addr), sa)) {
            list->fds[i].type = -1;
            memset(&list->fds[i].addr, 0, sizeof(list->fds[i].addr));
            return i + SOCKACT_START;
        }
    }
    return -1;
}

/*
 * Set up a listening socket.
 *
 * Arguments:
 *
 * - ba
 *      The bind address and port for the socket.
 * - ai
 *      The addrinfo struct to use for creating the socket.
 * - ctype
 *      The conn_type of this socket.
 */
static krb5_error_code
setup_socket(struct bind_address *ba, struct sockaddr *sock_address,
             struct sockact_list *sockacts, void *handle, const char *prog,
             verto_ctx *ctx, int listen_backlog, verto_callback vcb,
             enum conn_type ctype)
{
    krb5_error_code ret;
    struct connection *conn;
    verto_ev_flag flags;
    verto_ev *ev = NULL;
    int sock = -1;
    char addrbuf[128];

    k5_print_addr_port(sock_address, addrbuf, sizeof(addrbuf));
    krb5_klog_syslog(LOG_DEBUG, _("Setting up %s socket for address %s"),
                     bind_type_names[ba->type], addrbuf);

    if (sockacts->nsockets > 0) {
        /* Look for a systemd socket activation fd matching sock_address. */
        sock = find_sockact(sockacts, sock_address, bind_socktypes[ba->type]);
        if (sock == -1) {
            /* Ignore configured addresses that don't match any caller-provided
             * sockets. */
            ret = 0;
            goto cleanup;
        }
    } else {
        /* We're not using socket activation; create the socket. */
        ret = create_server_socket(sock_address, bind_socktypes[ba->type],
                                   prog, &sock);
        if (ret)
            goto cleanup;

        /* Listen for backlogged connections on stream sockets.  (For RPC
         * sockets this will be done by svc_register().) */
        if ((ba->type == TCP || ba->type == UNX) &&
            listen(sock, listen_backlog) != 0) {
            ret = errno;
            com_err(prog, errno, _("Cannot listen on %s server socket on %s"),
                    bind_type_names[ba->type], addrbuf);
            goto cleanup;
        }
    }

    /* Set non-blocking I/O for non-RPC listener sockets. */
    if (ba->type != RPC && setnbio(sock) != 0) {
        ret = errno;
        com_err(prog, errno,
                _("cannot set listening %s socket on %s non-blocking"),
                bind_type_names[ba->type], addrbuf);
        goto cleanup;
    }

    /* Turn off the linger option for TCP sockets. */
    if (ba->type == TCP && setnolinger(sock) != 0) {
        ret = errno;
        com_err(prog, errno, _("cannot set SO_LINGER on %s socket on %s"),
                bind_type_names[ba->type], addrbuf);
        goto cleanup;
    }

    /* Try to turn on pktinfo for UDP wildcard sockets. */
    if (ba->type == UDP && sa_is_wildcard(sock_address)) {
        krb5_klog_syslog(LOG_DEBUG, _("Setting pktinfo on socket %s"),
                         addrbuf);
        ret = set_pktinfo(sock, sock_address->sa_family);
        if (ret) {
            com_err(prog, ret,
                    _("Cannot request packet info for UDP socket address "
                      "%s port %d"), addrbuf, ba->port);
            krb5_klog_syslog(LOG_INFO, _("System does not support pktinfo yet "
                                         "binding to a wildcard address.  "
                                         "Packets are not guaranteed to "
                                         "return on the received address."));
        }
    }

    /* Add the socket to the event loop. */
    flags = VERTO_EV_FLAG_IO_READ | VERTO_EV_FLAG_PERSIST |
        VERTO_EV_FLAG_REINITIABLE;
    ret = add_fd(sock, ctype, flags, handle, prog, ctx, vcb, &ev);
    if (ret) {
        krb5_klog_syslog(LOG_ERR, _("Error attempting to add verto event"));
        goto cleanup;
    }

    if (ba->type == RPC) {
        conn = verto_get_private(ev);
        conn->transp = svctcp_create(sock, 0, 0);
        if (conn->transp == NULL) {
            ret = errno;
            krb5_klog_syslog(LOG_ERR, _("Cannot create RPC service: %s"),
                             strerror(ret));
            goto cleanup;
        }

        ret = svc_register(conn->transp, ba->rpc_svc_data.prognum,
                           ba->rpc_svc_data.versnum, ba->rpc_svc_data.dispatch,
                           0);
        if (!ret) {
            ret = errno;
            krb5_klog_syslog(LOG_ERR, _("Cannot register RPC service: %s"),
                             strerror(ret));
            goto cleanup;
        }
    }

    ev = NULL;
    sock = -1;
    ret = 0;

cleanup:
    if (sock >= 0)
        close(sock);
    if (ev != NULL)
        verto_del(ev);
    return ret;
}

/*
 * Setup all the socket addresses that the net-server should listen to.
 *
 * This function uses getaddrinfo to figure out all the addresses. This will
 * automatically figure out which socket families that should be used on the
 * host making it useful even for wildcard addresses.
 */
static krb5_error_code
setup_addresses(verto_ctx *ctx, void *handle, const char *prog,
                int listen_backlog)
{
    /* An bind_type enum map for the verto callback functions. */
    static verto_callback *const verto_callbacks[] = {
        [UDP] = &process_packet,
        [TCP] = &accept_stream_connection,
        [RPC] = &accept_rpc_connection,
        [UNX] = &accept_stream_connection
    };
    krb5_error_code ret = 0;
    size_t i;
    int err, bound_any;
    struct bind_address addr;
    struct sockaddr_un sun;
    struct addrinfo hints, *ai_list = NULL, *ai = NULL;
    struct sockact_list sockacts = { 0 };
    verto_callback vcb;
    char addrbuf[128];

    /* Check to make sure addresses were added to the server. */
    if (bind_addresses.n == 0) {
        krb5_klog_syslog(LOG_ERR, _("No addresses added to the net server"));
        return EINVAL;
    }

    /* Ask for all address families, listener addresses, and no port name
     * resolution. */
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_flags = AI_PASSIVE;
#ifdef AI_NUMERICSERV
    hints.ai_flags |= AI_NUMERICSERV;
#endif

    init_sockact_list(&sockacts);

    /* Add all the requested addresses. */
    for (i = 0; i < bind_addresses.n; i++) {
        addr = bind_addresses.data[i];
        hints.ai_socktype = bind_socktypes[addr.type];

        if (addr.type == UNX) {
            sun.sun_family = AF_UNIX;
            if (strlcpy(sun.sun_path, addr.address, sizeof(sun.sun_path)) >=
                sizeof(sun.sun_path)) {
                ret = ENAMETOOLONG;
                krb5_klog_syslog(LOG_ERR,
                                 _("UNIX domain socket path too long: %s"),
                                 addr.address);
                goto cleanup;
            }
            ret = setup_socket(&addr, (struct sockaddr *)&sun, &sockacts,
                               handle, prog, ctx, listen_backlog,
                               verto_callbacks[addr.type],
                               bind_conn_types[addr.type]);
            if (ret) {
                krb5_klog_syslog(LOG_ERR,
                                 _("Failed setting up a UNIX socket (for %s)"),
                                 addr.address);
                goto cleanup;
            }
            continue;
        }

        /* Call getaddrinfo, using a dummy port value. */
        err = getaddrinfo(addr.address, "0", &hints, &ai_list);
        if (err) {
            krb5_klog_syslog(LOG_ERR,
                             _("Failed getting address info (for %s): %s"),
                             (addr.address == NULL) ? "<wildcard>" :
                             addr.address, gai_strerror(err));
            ret = EIO;
            goto cleanup;
        }

        /*
         * Loop through all the sockets that getaddrinfo could find to match
         * the requested address.  For wildcard listeners, this should usually
         * have two results, one for each of IPv4 and IPv6, or one or the
         * other, depending on the system.  On IPv4-only systems, getaddrinfo()
         * may return both IPv4 and IPv6 addresses, but creating an IPv6 socket
         * may give an EAFNOSUPPORT error, so tolerate that error as long as we
         * can bind at least one socket.
         */
        bound_any = 0;
        for (ai = ai_list; ai != NULL; ai = ai->ai_next) {
            /* Make sure getaddrinfo returned a socket with the same type that
             * was requested. */
            assert(hints.ai_socktype == ai->ai_socktype);

            /* Set the real port number. */
            sa_setport(ai->ai_addr, addr.port);

            ret = setup_socket(&addr, ai->ai_addr, &sockacts, handle, prog,
                               ctx, listen_backlog, verto_callbacks[addr.type],
                               bind_conn_types[addr.type]);
            if (ret) {
                k5_print_addr(ai->ai_addr, addrbuf, sizeof(addrbuf));
                krb5_klog_syslog(LOG_ERR,
                                 _("Failed setting up a %s socket (for %s)"),
                                 bind_type_names[addr.type], addrbuf);
                if (ret != EAFNOSUPPORT)
                    goto cleanup;
            } else {
                bound_any = 1;
            }
        }
        if (!bound_any)
            goto cleanup;
        ret = 0;

        if (ai_list != NULL)
            freeaddrinfo(ai_list);
        ai_list = NULL;
    }

cleanup:
    if (ai_list != NULL)
        freeaddrinfo(ai_list);
    fini_sockact_list(&sockacts);
    return ret;
}

krb5_error_code
loop_setup_network(verto_ctx *ctx, void *handle, const char *prog,
                   int listen_backlog)
{
    krb5_error_code ret;
    verto_ev *ev;
    int i;

    /* Check to make sure that at least one address was added to the loop. */
    if (bind_addresses.n == 0)
        return EINVAL;

    /* Close any open connections. */
    FOREACH_ELT(events, i, ev)
        verto_del(ev);
    events.n = 0;

    krb5_klog_syslog(LOG_INFO, _("setting up network..."));
    ret = setup_addresses(ctx, handle, prog, listen_backlog);
    if (ret) {
        com_err(prog, ret, _("Error setting up network"));
        exit(1);
    }
    krb5_klog_syslog (LOG_INFO, _("set up %d sockets"), (int) events.n);
    if (events.n == 0) {
        /* If no sockets were set up, we can't continue. */
        com_err(prog, 0, _("no sockets set up?"));
        exit (1);
    }

    return 0;
}

struct udp_dispatch_state {
    void *handle;
    const char *prog;
    int port_fd;
    struct sockaddr_storage saddr;
    struct sockaddr_storage daddr;
    aux_addressing_info auxaddr;
    krb5_data request;
    char pktbuf[MAX_DGRAM_SIZE];
};

static void
process_packet_response(void *arg, krb5_error_code code, krb5_data *response)
{
    struct udp_dispatch_state *state = arg;
    int cc;

    if (code)
        com_err(state->prog ? state->prog : NULL, code,
                _("while dispatching (udp)"));
    if (code || response == NULL)
        goto out;

    cc = send_to_from(state->port_fd, response->data,
                      (socklen_t)response->length, 0, ss2sa(&state->saddr),
                      ss2sa(&state->daddr), &state->auxaddr);
    if (cc == -1) {
        /* Note that the local address (daddr*) has no port number
         * info associated with it. */
        char sbuf[128], dbuf[128];
        int e = errno;

        k5_print_addr_port(ss2sa(&state->saddr), sbuf, sizeof(sbuf));
        k5_print_addr(ss2sa(&state->daddr), dbuf, sizeof(dbuf));
        com_err(state->prog, e, _("while sending reply to %s from %s"),
                sbuf, dbuf);
        goto out;
    }
    if ((size_t)cc != response->length) {
        com_err(state->prog, 0, _("short reply write %d vs %d\n"),
                response->length, cc);
    }

out:
    krb5_free_data(get_context(state->handle), response);
    free(state);
}

static void
process_packet(verto_ctx *ctx, verto_ev *ev)
{
    int cc;
    struct connection *conn;
    struct udp_dispatch_state *state;
    socklen_t slen;

    conn = verto_get_private(ev);

    state = malloc(sizeof(*state));
    if (!state) {
        com_err(conn->prog, ENOMEM, _("while dispatching (udp)"));
        return;
    }

    state->handle = conn->handle;
    state->prog = conn->prog;
    state->port_fd = verto_get_fd(ev);
    assert(state->port_fd >= 0);

    memset(&state->auxaddr, 0, sizeof(state->auxaddr));
    cc = recv_from_to(state->port_fd, state->pktbuf, sizeof(state->pktbuf), 0,
                      &state->saddr, &state->daddr, &state->auxaddr);
    if (cc == -1) {
        if (errno != EINTR && errno != EAGAIN
            /*
             * This is how Linux indicates that a previous transmission was
             * refused, e.g., if the client timed out before getting the
             * response packet.
             */
            && errno != ECONNREFUSED
        )
            com_err(conn->prog, errno, _("while receiving from network"));
        free(state);
        return;
    }
    if (!cc) { /* zero-length packet? */
        free(state);
        return;
    }

    if (state->daddr.ss_family == AF_UNSPEC && conn->type == CONN_UDP) {
        /*
         * An address couldn't be obtained, so the PKTINFO option probably
         * isn't available.  If the socket is bound to a specific address, then
         * try to get the address here.
         */
        slen = sizeof(state->daddr);
        (void)getsockname(state->port_fd, ss2sa(&state->daddr), &slen);
    }

    state->request.length = cc;
    state->request.data = state->pktbuf;

    dispatch(state->handle, ss2sa(&state->daddr), ss2sa(&state->saddr),
             &state->request, 0, ctx, process_packet_response, state);
}

static int
kill_lru_stream_connection(void *handle, verto_ev *newev)
{
    struct connection *c = NULL, *oldest_c = NULL;
    verto_ev *ev, *oldest_ev = NULL;
    int i, fd = -1;

    krb5_klog_syslog(LOG_INFO, _("too many connections"));

    FOREACH_ELT (events, i, ev) {
        if (ev == newev)
            continue;

        c = verto_get_private(ev);
        if (!c)
            continue;
        if (c->type != CONN_TCP && c->type != CONN_RPC &&
            c->type != CONN_UNIXSOCK)
            continue;
        if (oldest_c == NULL
            || oldest_c->start_time > c->start_time) {
            oldest_ev = ev;
            oldest_c = c;
        }
    }
    if (oldest_c != NULL) {
        krb5_klog_syslog(LOG_INFO, _("dropping %s fd %d from %s"),
                         conn_type_names[oldest_c->type],
                         verto_get_fd(oldest_ev), oldest_c->addrbuf);
        if (oldest_c->type == CONN_RPC)
            oldest_c->rpc_force_close = 1;
        verto_del(oldest_ev);
    }
    return fd;
}

static void
accept_stream_connection(verto_ctx *ctx, verto_ev *ev)
{
    int s;
    struct sockaddr_storage addr;
    socklen_t addrlen = sizeof(addr);
    struct connection *newconn, *conn;
    enum conn_type ctype;
    verto_ev_flag flags;
    verto_ev *newev;

    conn = verto_get_private(ev);
    s = accept(verto_get_fd(ev), ss2sa(&addr), &addrlen);
    if (s < 0)
        return;
    set_cloexec_fd(s);
#ifndef _WIN32
    if (s >= FD_SETSIZE) {
        close(s);
        return;
    }
#endif
    setnbio(s);
    setnolinger(s);
    if (addr.ss_family != AF_UNIX)
        setkeepalive(s);

    flags = VERTO_EV_FLAG_IO_READ | VERTO_EV_FLAG_PERSIST;
    ctype = (conn->type == CONN_TCP_LISTENER) ? CONN_TCP : CONN_UNIXSOCK;
    if (add_fd(s, ctype, flags, conn->handle, conn->prog, ctx,
               process_stream_connection_read, &newev) != 0) {
        close(s);
        return;
    }
    newconn = verto_get_private(newev);

    if (addr.ss_family == AF_UNIX) {
        /* accept() doesn't fill in sun_path as the client socket isn't bound.
         * For logging purposes we will use the target address. */
        addrlen = sizeof(addr);
        if (getsockname(s, ss2sa(&addr), &addrlen) < 0) {
            com_err(conn->prog, errno, _("Failed to get address for %d"), s);
            close(s);
            return;
        }
    }

    k5_print_addr_port(ss2sa(&addr), newconn->addrbuf,
                       sizeof(newconn->addrbuf));
    newconn->addr_s = addr;
    newconn->addrlen = addrlen;
    newconn->bufsiz = 1024 * 1024;
    newconn->buffer = malloc(newconn->bufsiz);
    newconn->start_time = time(0);

    if (++stream_data_counter > max_stream_data_connections)
        kill_lru_stream_connection(conn->handle, newev);

    if (newconn->buffer == 0) {
        com_err(conn->prog, errno,
                _("allocating buffer for new TCP session from %s"),
                newconn->addrbuf);
        verto_del(newev);
        return;
    }
    newconn->offset = 0;
    SG_SET(&newconn->sgbuf[0], newconn->lenbuf, 4);
    SG_SET(&newconn->sgbuf[1], 0, 0);
}

struct tcp_dispatch_state {
    struct sockaddr_storage local_saddr;
    struct connection *conn;
    krb5_data request;
    verto_ctx *ctx;
    int sock;
};

static void
process_stream_response(void *arg, krb5_error_code code, krb5_data *response)
{
    struct tcp_dispatch_state *state = arg;
    verto_ev *ev;

    assert(state);
    state->conn->response = response;

    if (code)
        com_err(state->conn->prog, code, _("while dispatching (tcp)"));
    if (code || !response)
        goto kill_tcp_connection;

    /* Queue outgoing response. */
    store_32_be(response->length, state->conn->lenbuf);
    SG_SET(&state->conn->sgbuf[1], response->data, response->length);
    state->conn->sgp = state->conn->sgbuf;
    state->conn->sgnum = 2;

    ev = make_event(state->ctx, VERTO_EV_FLAG_IO_WRITE | VERTO_EV_FLAG_PERSIST,
                    process_stream_connection_write, state->sock, state->conn);
    if (ev) {
        free(state);
        return;
    }

kill_tcp_connection:
    stream_data_counter--;
    free_connection(state->conn);
    close(state->sock);
    free(state);
}

/* Creates the tcp_dispatch_state and deletes the verto event. */
static struct tcp_dispatch_state *
prepare_for_dispatch(verto_ctx *ctx, verto_ev *ev)
{
    struct tcp_dispatch_state *state;

    state = malloc(sizeof(*state));
    if (!state) {
        krb5_klog_syslog(LOG_ERR, _("error allocating tcp dispatch private!"));
        return NULL;
    }
    state->conn = verto_get_private(ev);
    state->sock = verto_get_fd(ev);
    state->ctx = ctx;
    verto_set_private(ev, NULL, NULL); /* Don't close the fd or free conn! */
    remove_event_from_set(ev); /* Remove it from the set. */
    verto_del(ev);
    return state;
}

static void
process_stream_connection_read(verto_ctx *ctx, verto_ev *ev)
{
    struct tcp_dispatch_state *state = NULL;
    struct connection *conn = NULL;
    ssize_t nread;
    size_t len;

    conn = verto_get_private(ev);

    /*
     * Read message length and data into one big buffer, already allocated
     * at connect time.  If we have a complete message, we stop reading, so
     * we should only be here if there is no data in the buffer, or only an
     * incomplete message.
     */
    if (conn->offset < 4) {
        krb5_data *response = NULL;

        /* msglen has not been computed.  XXX Doing at least two reads
         * here, letting the kernel worry about buffering. */
        len = 4 - conn->offset;
        nread = SOCKET_READ(verto_get_fd(ev),
                            conn->buffer + conn->offset, len);
        if (nread < 0) /* error */
            goto kill_tcp_connection;
        if (nread == 0) /* eof */
            goto kill_tcp_connection;
        conn->offset += nread;
        if (conn->offset == 4) {
            unsigned char *p = (unsigned char *)conn->buffer;
            conn->msglen = load_32_be(p);
            if (conn->msglen > conn->bufsiz - 4) {
                krb5_error_code err;
                /* Message too big. */
                krb5_klog_syslog(LOG_ERR, _("TCP client %s wants %lu bytes, "
                                            "cap is %lu"), conn->addrbuf,
                                 (unsigned long) conn->msglen,
                                 (unsigned long) conn->bufsiz - 4);
                /* XXX Should return an error.  */
                err = make_toolong_error (conn->handle,
                                          &response);
                if (err) {
                    krb5_klog_syslog(LOG_ERR, _("error constructing "
                                                "KRB_ERR_FIELD_TOOLONG error! %s"),
                                     error_message(err));
                    goto kill_tcp_connection;
                }

                state = prepare_for_dispatch(ctx, ev);
                if (!state) {
                    krb5_free_data(get_context(conn->handle), response);
                    goto kill_tcp_connection;
                }
                process_stream_response(state, 0, response);
            }
        }
    } else {
        /* msglen known. */
        socklen_t local_saddrlen = sizeof(struct sockaddr_storage);

        len = conn->msglen - (conn->offset - 4);
        nread = SOCKET_READ(verto_get_fd(ev),
                            conn->buffer + conn->offset, len);
        if (nread < 0) /* error */
            goto kill_tcp_connection;
        if (nread == 0) /* eof */
            goto kill_tcp_connection;
        conn->offset += nread;
        if (conn->offset < conn->msglen + 4)
            return;

        /* Have a complete message, and exactly one message. */
        state = prepare_for_dispatch(ctx, ev);
        if (!state)
            goto kill_tcp_connection;

        state->request.length = conn->msglen;
        state->request.data = conn->buffer + 4;

        if (getsockname(verto_get_fd(ev), ss2sa(&state->local_saddr),
                        &local_saddrlen) < 0) {
            krb5_klog_syslog(LOG_ERR, _("getsockname failed: %s"),
                             error_message(errno));
            goto kill_tcp_connection;
        }
        dispatch(state->conn->handle, ss2sa(&state->local_saddr),
                 ss2sa(&conn->addr_s), &state->request, 1, ctx,
                 process_stream_response, state);
    }

    return;

kill_tcp_connection:
    verto_del(ev);
}

static void
process_stream_connection_write(verto_ctx *ctx, verto_ev *ev)
{
    struct connection *conn;
    SOCKET_WRITEV_TEMP tmp;
    ssize_t nwrote;
    int sock;

    conn = verto_get_private(ev);
    sock = verto_get_fd(ev);

    nwrote = SOCKET_WRITEV(sock, conn->sgp,
                           conn->sgnum, tmp);
    if (nwrote > 0) { /* non-error and non-eof */
        while (nwrote) {
            sg_buf *sgp = conn->sgp;
            if ((size_t)nwrote < SG_LEN(sgp)) {
                SG_ADVANCE(sgp, (size_t)nwrote);
                nwrote = 0;
            } else {
                nwrote -= SG_LEN(sgp);
                conn->sgp++;
                conn->sgnum--;
                if (conn->sgnum == 0 && nwrote != 0)
                    abort();
            }
        }

        /* If we still have more data to send, just return so that
         * the main loop can call this function again when the socket
         * is ready for more writing. */
        if (conn->sgnum > 0)
            return;
    }

    /* Finished sending.  We should go back to reading, though if we
     * sent a FIELD_TOOLONG error in reply to a length with the high
     * bit set, RFC 4120 says we have to close the TCP stream. */
    verto_del(ev);
}

void
loop_free(verto_ctx *ctx)
{
    int i;
    struct bind_address val;

    verto_free(ctx);

    /* Free each addresses added to the loop. */
    FOREACH_ELT(bind_addresses, i, val)
        free(val.address);
    FREE_SET_DATA(bind_addresses);
    FREE_SET_DATA(events);
}

static int
have_event_for_fd(int fd)
{
    verto_ev *ev;
    int i;

    FOREACH_ELT(events, i, ev) {
        if (verto_get_fd(ev) == fd)
            return 1;
    }

    return 0;
}

static void
accept_rpc_connection(verto_ctx *ctx, verto_ev *ev)
{
    verto_ev_flag flags;
    struct connection *conn;
    fd_set fds;
    int s;

    conn = verto_get_private(ev);

    /* Service the woken RPC listener descriptor. */
    FD_ZERO(&fds);
    FD_SET(verto_get_fd(ev), &fds);
    svc_getreqset(&fds);

    /* Scan svc_fdset for any new connections. */
    for (s = 0; s < FD_SETSIZE; s++) {
        struct sockaddr_storage addr;
        socklen_t addrlen = sizeof(addr);
        struct connection *newconn;
        verto_ev *newev;

        /* If we already have this fd, continue. */
        if (!FD_ISSET(s, &svc_fdset) || have_event_for_fd(s))
            continue;

        flags = VERTO_EV_FLAG_IO_READ | VERTO_EV_FLAG_PERSIST;
        if (add_fd(s, CONN_RPC, flags, conn->handle, conn->prog, ctx,
                   process_rpc_connection, &newev) != 0)
            continue;
        newconn = verto_get_private(newev);

        set_cloexec_fd(s);

        if (getpeername(s, ss2sa(&addr), &addrlen) != 0) {
            strlcpy(newconn->addrbuf, "<unknown>", sizeof(newconn->addrbuf));
        } else {
            k5_print_addr_port(ss2sa(&addr), newconn->addrbuf,
                               sizeof(newconn->addrbuf));
        }

        newconn->addr_s = addr;
        newconn->addrlen = addrlen;
        newconn->start_time = time(0);

        if (++stream_data_counter > max_stream_data_connections)
            kill_lru_stream_connection(newconn->handle, newev);
    }
}

static void
process_rpc_connection(verto_ctx *ctx, verto_ev *ev)
{
    fd_set fds;

    FD_ZERO(&fds);
    FD_SET(verto_get_fd(ev), &fds);
    svc_getreqset(&fds);

    if (!FD_ISSET(verto_get_fd(ev), &svc_fdset))
        verto_del(ev);
}

#endif /* INET */
