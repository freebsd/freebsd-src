/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#ifndef _PORT_SOCKET_H
#define _PORT_SOCKET_H
#if defined(_WIN32)

#include <winsock2.h>
#include <ws2tcpip.h>
#include <errno.h>

/* Some of our own infrastructure where the Winsock stuff was too hairy
 * to dump into a clean Unix program */

typedef WSABUF sg_buf;

#define SG_ADVANCE(SG, N)                       \
    ((SG)->len < (N)                            \
     ? (abort(), 0)                             \
     : ((SG)->buf += (N), (SG)->len -= (N), 0))

#define SG_LEN(SG)              ((SG)->len + 0)
#define SG_BUF(SG)              ((SG)->buf + 0)
#define SG_SET(SG, B, N)        ((SG)->buf = (char *)(B),(SG)->len = (N))

#define SOCKET_INITIALIZE()     0
#define SOCKET_CLEANUP()
#define SOCKET_ERRNO            (TranslatedWSAGetLastError())
#define SOCKET_SET_ERRNO(x)     (TranslatedWSASetLastError(x))
#define SOCKET_NFDS(f)          (0)     /* select()'s first arg is ignored */
#define SOCKET_READ(fd, b, l)   (recv(fd, b, l, 0))
#define SOCKET_WRITE(fd, b, l)  (send(fd, b, l, 0))
#define SOCKET_CONNECT          connect /* XXX */
#define SOCKET_GETSOCKNAME      getsockname /* XXX */
#define SOCKET_CLOSE            close /* XXX */
#define SOCKET_EINTR            WSAEINTR

/*
 * Return -1 for error or number of bytes written.  TMP is a temporary
 * variable; must be declared by the caller, and must be used by this macro (to
 * avoid compiler warnings).
 */
/* WSASend returns 0 or SOCKET_ERROR.  */
#define SOCKET_WRITEV_TEMP DWORD
#define SOCKET_WRITEV(FD, SG, LEN, TMP)                         \
    (WSASend((FD), (SG), (LEN), &(TMP), 0, 0, 0) ? -1 : (TMP))

#define SHUTDOWN_READ   SD_RECEIVE
#define SHUTDOWN_WRITE  SD_SEND
#define SHUTDOWN_BOTH   SD_BOTH

/*
 * Define any missing POSIX socket errors.  This is for compatibility with
 * older versions of MSVC (pre-2010).
 */
#ifndef EINPROGRESS
#define EINPROGRESS WSAEINPROGRESS
#endif
#ifndef EWOULDBLOCK
#define EWOULDBLOCK WSAEWOULDBLOCK
#endif
#ifndef ECONNRESET
#define ECONNRESET  WSAECONNRESET
#endif
#ifndef ECONNABORTED
#define ECONNABORTED WSAECONNABORTED
#endif
#ifndef ECONNREFUSED
#define ECONNREFUSED WSAECONNREFUSED
#endif
#ifndef EHOSTUNREACH
#define EHOSTUNREACH WSAEHOSTUNREACH
#endif
#ifndef ETIMEDOUT
#define ETIMEDOUT WSAETIMEDOUT
#endif

/* Translate posix_error to its Winsock counterpart and set the last Winsock
 * error to the result. */
static __inline void TranslatedWSASetLastError(int posix_error)
{
    int wsa_error;
    switch (posix_error) {
    case 0:
        wsa_error = 0; break;
    case EINPROGRESS:
        wsa_error = WSAEINPROGRESS; break;
    case EWOULDBLOCK:
        wsa_error = WSAEWOULDBLOCK; break;
    case ECONNRESET:
        wsa_error = WSAECONNRESET; break;
    case ECONNABORTED:
        wsa_error = WSAECONNABORTED; break;
    case ECONNREFUSED:
        wsa_error = WSAECONNREFUSED; break;
    case EHOSTUNREACH:
        wsa_error = WSAEHOSTUNREACH; break;
    case ETIMEDOUT:
        wsa_error = WSAETIMEDOUT; break;
    case EAFNOSUPPORT:
        wsa_error = WSAEAFNOSUPPORT; break;
    case EINVAL:
        wsa_error = WSAEINVAL; break;
    default:
        /* Ideally, we would log via k5-trace here, but we have no context. */
        wsa_error = WSAEINVAL; break;
    }
    WSASetLastError(wsa_error);
}

/*
 * Translate Winsock errors to their POSIX counterparts.  This is necessary for
 * MSVC 2010+, where both Winsock and POSIX errors are defined.
 */
static __inline int TranslatedWSAGetLastError()
{
    int err = WSAGetLastError();
    switch (err) {
    case 0:
        break;
    case WSAEINPROGRESS:
        err = EINPROGRESS; break;
    case WSAEWOULDBLOCK:
        err = EWOULDBLOCK; break;
    case WSAECONNRESET:
        err = ECONNRESET; break;
    case WSAECONNABORTED:
        err = ECONNABORTED; break;
    case WSAECONNREFUSED:
        err = ECONNREFUSED; break;
    case WSAEHOSTUNREACH:
        err = EHOSTUNREACH; break;
    case WSAETIMEDOUT:
        err = ETIMEDOUT; break;
    case WSAEAFNOSUPPORT:
        err = EAFNOSUPPORT; break;
    case WSAEINVAL:
        err = EINVAL; break;
    default:
        /* Ideally, we would log via k5-trace here, but we have no context. */
        err = EINVAL; break;
    }
    return err;
}

#elif defined(__palmos__)

/* If this source file requires it, define struct sockaddr_in (and possibly
 * other things related to network I/O). */

#include "autoconf.h"
#include <netdb.h>
typedef int socklen_t;

#else /* UNIX variants */

#include "autoconf.h"

#include <sys/types.h>
#include <netinet/in.h>         /* For struct sockaddr_in and in_addr */
#include <arpa/inet.h>          /* For inet_ntoa */
#include <netdb.h>

#ifndef HAVE_NETDB_H_H_ERRNO
extern int h_errno;             /* In case it's missing, e.g., HP-UX 10.20. */
#endif

#include <sys/param.h>          /* For MAXHOSTNAMELEN */
#include <sys/socket.h>         /* For SOCK_*, AF_*, etc */
#include <sys/time.h>           /* For struct timeval */
#include <net/if.h>             /* For struct ifconf, for localaddr.c */
#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>            /* For struct iovec, for sg_buf */
#endif
#ifdef HAVE_SYS_FILIO_H
#include <sys/filio.h>          /* For FIONBIO on Solaris.  */
#endif

/*
 * Either size_t or int or unsigned int is probably right.  Under
 * SunOS 4, it looks like int is desired, according to the accept man
 * page.
 */
#ifndef HAVE_SOCKLEN_T
typedef int socklen_t;
#endif

#ifndef HAVE_STRUCT_SOCKADDR_STORAGE
struct krb5int_sockaddr_storage {
    struct sockaddr_in s;
    /* Plenty of slop just in case we get an ipv6 address anyways.  */
    long extra[16];
};
#define sockaddr_storage krb5int_sockaddr_storage
#endif

/* Unix equivalents of Winsock calls */
#define SOCKET          int
#define INVALID_SOCKET  ((SOCKET)~0)
#define closesocket     close
#define ioctlsocket     ioctl
#define SOCKET_ERROR    (-1)

typedef struct iovec sg_buf;

#define SG_ADVANCE(SG, N)                               \
    ((SG)->iov_len < (N)                                \
     ? (abort(), 0)                                     \
     : ((SG)->iov_base = (char *) (SG)->iov_base + (N), \
        (SG)->iov_len -= (N), 0))

#define SG_LEN(SG)              ((SG)->iov_len + 0)
#define SG_BUF(SG)              ((char*)(SG)->iov_base + 0)
#define SG_SET(SG, B, L)        ((SG)->iov_base = (char*)(B), (SG)->iov_len = (L))

#define SOCKET_INITIALIZE()     (0)     /* No error (or anything else) */
#define SOCKET_CLEANUP()        /* nothing */
#define SOCKET_ERRNO            errno
#define SOCKET_SET_ERRNO(x)     (errno = (x))
#define SOCKET_NFDS(f)          ((f)+1) /* select() arg for a single fd */
#define SOCKET_READ             read
#define SOCKET_WRITE            write
#define SOCKET_CONNECT          connect
#define SOCKET_GETSOCKNAME      getsockname
#define SOCKET_CLOSE            close
#define SOCKET_EINTR            EINTR
#define SOCKET_WRITEV_TEMP int
/* Use TMP to avoid compiler warnings and keep things consistent with
 * Windows version. */
#define SOCKET_WRITEV(FD, SG, LEN, TMP)         \
    ((TMP) = writev((FD), (SG), (LEN)), (TMP))

#define SHUTDOWN_READ   0
#define SHUTDOWN_WRITE  1
#define SHUTDOWN_BOTH   2

#ifndef HAVE_INET_NTOP
#define inet_ntop(AF,SRC,DST,CNT)                                       \
    ((AF) == AF_INET                                                    \
     ? ((CNT) < 16                                                      \
        ? (SOCKET_SET_ERRNO(ENOSPC), (const char *)NULL)                \
        : (sprintf((DST), "%d.%d.%d.%d",                                \
                   ((const unsigned char *)(const void *)(SRC))[0] & 0xff, \
                   ((const unsigned char *)(const void *)(SRC))[1] & 0xff, \
                   ((const unsigned char *)(const void *)(SRC))[2] & 0xff, \
                   ((const unsigned char *)(const void *)(SRC))[3] & 0xff), \
           (DST)))                                                      \
     : (SOCKET_SET_ERRNO(EAFNOSUPPORT), (const char *)NULL))
#define HAVE_INET_NTOP
#endif

#endif /* _WIN32 */

#if !defined(_WIN32)
/* UNIX or ...?  */
# ifdef S_SPLINT_S
extern int socket (int, int, int) /*@*/;
# endif
#endif

#endif /*_PORT_SOCKET_H*/
