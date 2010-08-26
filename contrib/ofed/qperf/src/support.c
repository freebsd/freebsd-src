/*
 * qperf - support routines.
 * Measure socket and RDMA performance.
 *
 * Copyright (c) 2002-2009 Johann George.  All rights reserved.
 * Copyright (c) 2006-2009 QLogic Corporation.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include "qperf.h"


/*
 * Configurable parameters.
 */
#define ERROR_TIMEOUT   3               /* Error timeout in seconds */


/*
 * For convenience.
 */
typedef void (SIGFUNC)(int signo, siginfo_t *siginfo, void *ucontext);


/*
 * Function prototypes.
 */
static void     buf_app(char **pp, char *end, char *str);
static void     buf_end(char **pp, char *end);
static double   get_seconds(void);
static void     remote_failure_error(void);
static char    *remote_name(void);
static int      send_recv_mesg(int sr, char *item, int fd, char *buf, int len);
static SIGFUNC  sig_alrm_remote_failure;
static SIGFUNC  sig_alrm_die;
static void     timeout_set(int seconds, SIGFUNC sigfunc);
static void     timeout_end(void);


/*
 * Static variables.
 */
static uint8_t *DecodePtr;
static uint8_t *EncodePtr;


/*
 * Initialize encode pointer.
 */
void
enc_init(void *p)
{
    EncodePtr = p;
}


/*
 * Initialize decode pointer.
 */
void
dec_init(void *p)
{
    DecodePtr = p;
}


/*
 * Encode a string.
 */
void
enc_str(char *s, int n)
{
    memcpy(EncodePtr, s, n);
    EncodePtr += n;
}


/*
 * Decode a string.
 */
void
dec_str(char *s, int  n)
{
    memcpy(s, DecodePtr, n);
    DecodePtr += n;
}


/*
 * Encode an integer.
 */
void
enc_int(int64_t l, int n)
{
    while (n--) {
        *EncodePtr++ = l;
        l >>= 8;
    }
}


/*
 * Decode an integer.
 */
int64_t
dec_int(int n)
{
    uint64_t l = 0;
    uint8_t *p = (DecodePtr += n);

    while (n--)
        l = (l << 8) | (*--p & 0xFF);
    return l;
}


/*
 * Encode a 32 bit unsigned integer.
 */
void
encode_uint32(uint32_t *p, uint32_t v)
{
    enc_init(p);
    enc_int(v, sizeof(v));
}


/*
 * Decode a 32 bit unsigned integer.
 */
uint32_t
decode_uint32(uint32_t *p)
{
    dec_init(p);
    return dec_int(sizeof(uint32_t));
}


/*
 * Call malloc and exit with an error on failure.
 */
void *
qmalloc(long n)
{
    void *p = malloc(n);

    if (!p)
        error(0, "malloc failed");
    return p;
}


/*
 * Attempt to print out a string allocating the necessary storage and exit with
 * an error on failure.
 */
char *
qasprintf(char *fmt, ...)
{
    int stat;
    char *str;
    va_list alist;

    va_start(alist, fmt);
    stat = vasprintf(&str, fmt, alist);
    va_end(alist);
    if (stat < 0)
        error(0, "out of space");
    return str;
}


/*
 * Touch data.
 */
void
touch_data(void *p, int n)
{
    uint64_t a;
    volatile uint64_t *p64 = p;

    while (n >= sizeof(*p64)) {
        a = *p64++;
        n -= sizeof(*p64);
    }
    if (n) {
        volatile uint8_t *p8 = (uint8_t *)p64;
        while (n >= sizeof(*p8)) {
            a = *p8++;
            n -= sizeof(*p8);
        }
    }
}


/*
 * Synchronize the client and server.
 */
void
synchronize(char *msg)
{
    send_sync(msg);
    recv_sync(msg);
    debug("synchronization complete");
}


/*
 * Send a synchronize message.
 */
void
send_sync(char *msg)
{
    int n = strlen(msg);

    send_mesg(msg, n, msg);
}


/*
 * Receive a synchronize message.
 */
void
recv_sync(char *msg)
{
    char data[64];
    int n = strlen(msg);

    if (n > sizeof(data))
        error(BUG, "buffer in recv_sync() too small");
    recv_mesg(data, n, msg);
    if (memcmp(data, msg, n) != 0)
        error(0, "synchronize %s failure: data does not match", msg);
}


/*
 * Send a message to the client.
 */
int
send_mesg(void *ptr, int len, char *item)
{
    if (item)
        debug("sending %s", item);
    return send_recv_mesg('s', item, RemoteFD, ptr, len);
}


/*
 * Receive a response from the server.
 */
int
recv_mesg(void *ptr, int len, char *item)
{
    if (item)
        debug("waiting for %s", item);
    return send_recv_mesg('r', item, RemoteFD, ptr, len);
}


/*
 * Send or receive a message to a file descriptor timing out after a certain
 * amount of time.
 */
static int
send_recv_mesg(int sr, char *item, int fd, char *buf, int len)
{
    typedef ssize_t (IO)(int fd, void *buf, size_t count);
    double  etime;
    fd_set *fdset;
    fd_set  rfdset;
    fd_set  wfdset;
    char   *action;
    IO     *func;
    int     ioc = 0;

    if (sr == 'r') {
        func = (IO *)read;
        fdset = &rfdset;
        action = "receive";
    } else {
        func = (IO *)write;
        fdset = &wfdset;
        action = "send";
    }

    etime = get_seconds() + Req.timeout;
    while (len) {
        int n;
        double time;
        struct timeval timeval;

        errno = 0;
        time = etime - get_seconds();
        if (time <= 0) {
            if (!item)
                return ioc;
            error(0, "failed to %s %s: timed out", action, item);
        }
        n = time += 1.0 / (1000*1000);
        timeval.tv_sec  = n;
        timeval.tv_usec = (time-n) * 1000*1000;

        FD_ZERO(&rfdset);
        FD_ZERO(&wfdset);
        FD_SET(fd, fdset);
        if (select(fd+1, &rfdset, &wfdset, 0, &timeval) < 0)
            error(SYS, "failed to %s %s: select failed", action, item);
        if (!FD_ISSET(fd, fdset))
            continue;
        n = func(fd, buf, len);
        if (n <= 0) {
            if (!item)
                return ioc;
            if (n < 0)
                error(SYS, "failed to %s %s", action, item);
            if (n == 0) {
                error(0, "failed to %s %s: %s not responding",
                                                action, item, remote_name());
            }
        }
        len -= n;
        ioc += n;
        buf += n;
    }
    return ioc;
}


/*
 * Get the time of day in seconds as a floating point number.
 */
static double
get_seconds(void)
{
    struct timeval timeval;

    if (gettimeofday(&timeval, 0) < 0)
        error(SYS, "gettimeofday failed");
    return timeval.tv_sec + timeval.tv_usec/(1000.0*1000.0);
}


/*
 * Call getaddrinfo given a numeric port.  Complain on error.
 */
struct addrinfo *
getaddrinfo_port(char *node, int port, struct addrinfo *hints)
{
    struct addrinfo *res;
    char *service = qasprintf("%d", port);
    int stat = getaddrinfo(node, service, hints, &res);

    free(service);
    if (stat != 0)
        error(0, "getaddrinfo failed: %s", gai_strerror(stat));
    if (!res)
        error(0, "getaddrinfo failed: no valid entries");
    return res;
}


/*
 * A version of setsockopt that sets a parameter to 1 and exits with an error
 * on failure.
 */
void
setsockopt_one(int fd, int optname)
{
    int one = 1;

    if (setsockopt(fd, SOL_SOCKET, optname, &one, sizeof(one)) >= 0)
        return;
    error(SYS, "setsockopt %d %d to 1 failed", SOL_SOCKET, optname);
}


/*
 * This is called when a SIGURG signal is received indicating that TCP
 * out-of-band data has arrived.  This is used by the remote end to indicate
 * one of two conditions: the test has completed or an error has occurred.
 */
void
urgent(void)
{
    int z;
    char *p, *q;
    char buffer[256];

    /*
     * There is a slim chance that an urgent message arrived before accept
     * returned.  This is likely not even possible with the current code flow
     * but we check just in case.
     */
    if (RemoteFD < 0)
        return;

    /*
     * This recv could fail if for some reason our socket buffer was full of
     * in-band data and the remote side could not send the out of band data.
     * If the recv fails with EWOULDBLOCK, we should keep reading in-band data
     * until we clear the in-band data.  Since we do not send enough data for
     * this case to cause us concern in the normal case, we do not expect this
     * to ever occur.  If it does, we let the lower levels deal with it.
     */
    if (recv(RemoteFD, buffer, 1, MSG_OOB) != 1)
        return;

    /*
     * If the indication is that the other side has completed its testing,
     * indicate completion on our side also.
     */
    if (buffer[0] == '.') {
        set_finished();
        return;
    }

    /*
     * If we are the server, we only print out client error messages if we are
     * in debug mode.
     */
    if (!Debug && !is_client())
        die();

    p = buffer;
    q = p + sizeof(buffer);
    buf_app(&p, q, remote_name());
    buf_app(&p, q, ": ");
    timeout_set(ERROR_TIMEOUT, sig_alrm_remote_failure);

    for (;;) {
        int s = sockatmark(RemoteFD);

        if (s < 0)
            remote_failure_error();
        if (s)
            break;
        z = read(RemoteFD, p, q-p);
    }

    while (p < q) {
        int n = read(RemoteFD, p, q-p);

        if (n <= 0)
            break;
        p += n;
    }
    timeout_end();

    buf_end(&p, q);
    z = write(2, buffer, p+1-buffer);
    die();
}


/*
 * Remote end timed out in an attempt to find the error.
 */
static void
sig_alrm_remote_failure(int signo, siginfo_t *siginfo, void *ucontext)
{
    remote_failure_error();
}


/*
 * The remote timed out while attempting to convey an error.  Tell the user.
 */
static void
remote_failure_error(void)
{
    int z;
    char buffer[256];
    char *p = buffer;
    char *q = p + sizeof(buffer);

    buf_app(&p, q, remote_name());
    buf_app(&p, q, " failure");
    buf_end(&p, q);
    z = write(2, buffer, p+1-buffer);
    die();
}


/*
 * Return a string describing whether the remote is a client or a server.
 */
static char *
remote_name(void)
{
    if (is_client())
        return "server";
    else
        return "client";
}


/*
 * Print out an error message.  actions contain a set of flags that determine
 * what needs to get done.  If BUG is set, it is an internal error.  If SYS is
 * set, a system error is printed.  If RET is set, we return rather than exit.
 */
int
error(int actions, char *fmt, ...)
{
    int z;
    va_list alist;
    char buffer[256];
    char *p = buffer;
    char *q = p + sizeof(buffer);

    if ((actions & BUG) != 0)
        buf_app(&p, q, "internal error: ");
    va_start(alist, fmt);
    p += vsnprintf(p, q-p, fmt, alist);
    va_end(alist);
    if ((actions & SYS) != 0 && errno) {
        buf_app(&p, q, ": ");
        buf_app(&p, q, strerror(errno));
    }
    buf_end(&p, q);
    fwrite(buffer, 1, p+1-buffer, stdout);
    if ((actions & RET) != 0)
        return 0;

    if (RemoteFD >= 0) {
        send(RemoteFD, "?", 1, MSG_OOB);
        z = write(RemoteFD, buffer, p-buffer);
        shutdown(RemoteFD, SHUT_WR);
        timeout_set(ERROR_TIMEOUT, sig_alrm_die);
        while (read(RemoteFD, buffer, sizeof(buffer)) > 0)
            ;
    }
    die();
    return 0;
}


/*
 * Remote end timed out while waiting for acknowledgement that it received
 * error.
 */
static void
sig_alrm_die(int signo, siginfo_t *siginfo, void *ucontext)
{
    die();
}


/*
 * Start timeout.
 */
static void
timeout_set(int seconds, SIGFUNC sigfunc)
{
    struct itimerval itimerval = {{0}};
    struct sigaction act ={
        .sa_sigaction = sigfunc,
        .sa_flags = SA_SIGINFO
    };

    setitimer(ITIMER_REAL, &itimerval, 0);
    sigaction(SIGALRM, &act, 0);
    itimerval.it_value.tv_sec = seconds;
    setitimer(ITIMER_REAL, &itimerval, 0);
}


/*
 * End timeout.
 */
static void
timeout_end(void)
{
    struct itimerval itimerval = {{0}};

    setitimer(ITIMER_REAL, &itimerval, 0);
}


/*
 * Add a string to a buffer.
 */
static void
buf_app(char **pp, char *end, char *str)
{
    char *p = *pp;
    int n = strlen(str);
    int l = end - p;

    if (n > l)
        n = l;
    memcpy(p, str, n);
    *pp = p + n;
}


/*
 * End a buffer.
 */
static void
buf_end(char **pp, char *end)
{
    char *p = *pp;

    if (p == end) {
        char *s = " ...";
        int n = strlen(s);
        memcpy(--p-n, s, n);
    }
    *p = '\n';
    *pp = p;
}


/*
 * Print out a debug message.
 */
void
debug(char *fmt, ...)
{
    va_list alist;

    if (!Debug)
        return;
    va_start(alist, fmt);
    vfprintf(stderr, fmt, alist);
    va_end(alist);
    fprintf(stderr, "\n");
    fflush(stderr);
}


/*
 * Exit unsuccessfully.
 */
void
die(void)
{
    exit(1);
}
