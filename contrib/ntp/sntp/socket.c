/*  Copyright (C) 1996, 2000 N.M. Maclaren
    Copyright (C) 1996, 2000 The University of Cambridge

This includes all of the code needed to handle Berkeley sockets.  It is way
outside current POSIX, unfortunately.  It should be easy to convert to a system
that uses another mechanism.  It does not currently use socklen_t, because
the only system that the author uses that has it is Linux. */



#include "config.h"

#include "header.h"
#include "internet.h"
#include <fcntl.h>

#define SOCKET
#include "kludges.h"
#undef SOCKET



/* The code needs to set some variables during the open, for use by later
functions. */

static int initial = 1,
    descriptors[MAX_SOCKETS];

#ifdef HAVE_IPV6
static struct sockaddr_storage here[MAX_SOCKETS], there[MAX_SOCKETS];
#else
static struct sockaddr_in here[MAX_SOCKETS], there[MAX_SOCKETS];
#endif

void display_in_hex(const void *, int);
#ifdef HAVE_IPV6
void display_sock_in_hex(struct sockaddr_storage *);
#else
void display_sock_in_hex (struct sockaddr_in *);
#endif

/* There needs to be some disgusting grobble for handling timeouts, that is
identical to the grobble in internet.c. */

static jmp_buf jump_buffer;

static void jump_handler (int sig) {
    longjmp(jump_buffer,1);
}

static void clear_alarm (void) {
    int k;

    k = errno;
    alarm(0);
    errno = 0;
    if (signal(SIGALRM,SIG_DFL) == SIG_ERR)
        fatal(1,"unable to reset signal handler",NULL);
    errno = k;
}



void display_in_hex (const void *data, int length) {
    int i;

    for (i = 0; i < length; ++i)
        fprintf(stderr,"%.2x",((const unsigned char *)data)[i]);
}

#ifdef HAVE_IPV6

void display_sock_in_hex (struct sockaddr_storage *sock) {
    int family;
    struct sockaddr_in *sin;
    struct sockaddr_in6 *sin6;

    family = sock->ss_family;
    switch(family) {
    case AF_INET:
	sin = (struct sockaddr_in *)sock;
	display_in_hex(&sin->sin_addr, sizeof(struct in_addr));
	fprintf(stderr,"/");
	display_in_hex(&sin->sin_port, 2);
	break;
    case AF_INET6:
	sin6 = (struct sockaddr_in6 *)sock;
	display_in_hex(&sin6->sin6_addr, sizeof(struct in6_addr));
	fprintf(stderr,"/");
	display_in_hex(&sin6->sin6_port, 2);
	break;
    }
}

#else

void display_sock_in_hex (struct sockaddr_in *sock) {
    int family, len;
    struct sockaddr_in *sin;

    family = sock->sin_family;
    switch(family) {
    case AF_INET:
	sin = (struct sockaddr_in *)sock;
	display_in_hex(&sin->sin_addr, sizeof(struct in_addr));
	fprintf(stderr,"/");
	display_in_hex(&sin->sin_port, 2);
	break;
    }
}
#endif

extern int unprivport;

#ifdef HAVE_IPV6

void open_socket (int which, char *hostname, int timespan) {

/* Locate the specified NTP server, set up a couple of addresses and open a
socket. */

    int port, k, sl;
    struct sockaddr_storage address, anywhere;

/* Initialise and find out the server and port number.  Note that the port
number is in network format. */

    if (initial)
	for (k = 0; k < MAX_SOCKETS; ++k)
	    descriptors[k] = -1;
    initial = 0;
    if (which < 0 || which >= MAX_SOCKETS || descriptors[which] >= 0)
        fatal(0,"socket index out of range or already open",NULL);
    if (verbose > 2)
	fprintf(stderr,"Looking for the socket addresses\n");
    find_address(&address,&anywhere,&port,hostname,timespan);
    if (verbose > 2) {
        fprintf(stderr,"Internet address: address=");
        display_sock_in_hex(&address);
        fprintf(stderr," anywhere=");
        display_sock_in_hex(&anywhere);
        fputc('\n',stderr);
    }

/* Set up our own and the target addresses.  Note that the target address will
be reset before use in server mode. */

    memset(&here[which], 0, sizeof(struct sockaddr_storage));
    here[which] = anywhere;
    if (operation != op_listen || unprivport)
        ((struct sockaddr_in6 *)&here[which])->sin6_port = 0;
    memset(&there[which], 0, sizeof(struct sockaddr_storage));
    there[which] = address;
    if (verbose > 2) {
        fprintf(stderr,"Initial sockets: here=");
        display_sock_in_hex(&here[which]);
        fprintf(stderr," there=");
        display_sock_in_hex(&there[which]);
        fputc('\n',stderr);
    }

/* Allocate a local UDP socket and configure it. */

    switch(((struct sockaddr_in *)&there[which])->sin_family) {
    case AF_INET:
	sl = sizeof(struct sockaddr_in);
	break;
#ifdef HAVE_IPV6
    case AF_INET6:
	sl = sizeof(struct sockaddr_in6);
	break;
#endif
    default:
	sl = 0;
	break;
    }
    errno = 0;
    if ((descriptors[which] = socket(here[which].ss_family,SOCK_DGRAM,0)) < 0
	|| bind(descriptors[which],(struct sockaddr *)&here[which], sl) < 0)
        fatal(1,"unable to allocate socket for NTP",NULL);
}

#else

void open_socket (int which, char *hostname, int timespan) {

/* Locate the specified NTP server, set up a couple of addresses and open a
socket. */

    int port, k;
    struct in_addr address, anywhere;

/* Initialise and find out the server and port number.  Note that the port
number is in network format. */

    if (initial) for (k = 0; k < MAX_SOCKETS; ++k) descriptors[k] = -1;
    initial = 0;
    if (which < 0 || which >= MAX_SOCKETS || descriptors[which] >= 0)
        fatal(0,"socket index out of range or already open",NULL);
    if (verbose > 2) fprintf(stderr,"Looking for the socket addresses\n");
    find_address(&address,&anywhere,&port,hostname,timespan);
    if (verbose > 2) {
        fprintf(stderr,"Internet address: address=");
        display_in_hex(&address,sizeof(struct in_addr));
        fprintf(stderr," anywhere=");
        display_in_hex(&anywhere,sizeof(struct in_addr));
        fputc('\n',stderr);
    }

/* Set up our own and the target addresses. */

    memset(&here[which],0,sizeof(struct sockaddr_in));
    here[which].sin_family = AF_INET;
    here[which].sin_port =
        (operation == op_listen || !unprivport ? port : 0);
    here[which].sin_addr = anywhere;
    memset(&there[which],0,sizeof(struct sockaddr_in));
    there[which].sin_family = AF_INET;
    there[which].sin_port = port;
    there[which].sin_addr = address;
    if (verbose > 2) {
        fprintf(stderr,"Initial sockets: here=");
        display_in_hex(&here[which].sin_addr,sizeof(struct in_addr));
        fputc('/',stderr);
        display_in_hex(&here[which].sin_port,sizeof(here[which].sin_port));
        fprintf(stderr," there=");
        display_in_hex(&there[which].sin_addr,sizeof(struct in_addr));
        fputc('/',stderr);
        display_in_hex(&there[which].sin_port,sizeof(there[which].sin_port));
        fputc('\n',stderr);
    }

/* Allocate a local UDP socket and configure it. */

    errno = 0;
    if ((descriptors[which] = socket(AF_INET,SOCK_DGRAM,0)) < 0 ||
            bind(descriptors[which],(struct sockaddr *)&here[which],
                    sizeof(here[which]))  < 0)
        fatal(1,"unable to allocate socket for NTP",NULL);
}

#endif

extern void write_socket (int which, void *packet, int length) {

/* Any errors in doing this are fatal - including blocking.  Yes, this leaves a
server vulnerable to a denial of service attack. */

    int k, sl;

    switch(((struct sockaddr_in *)&there[which])->sin_family) {
    case AF_INET:
	sl = sizeof(struct sockaddr_in);
	break;
#ifdef HAVE_IPV6
    case AF_INET6:
	sl = sizeof(struct sockaddr_in6);
	break;
#endif
    default:
	sl = 0;
	break;
    }
    if (which < 0 || which >= MAX_SOCKETS || descriptors[which] < 0)
        fatal(0,"socket index out of range or not open",NULL);
    errno = 0;
    k = sendto(descriptors[which],packet,(size_t)length,0,
            (struct sockaddr *)&there[which],sl);
    if (k != length) fatal(1,"unable to send NTP packet",NULL);
}



extern int read_socket (int which, void *packet, int length, int waiting) {

/* Read a packet and return its length or -1 for failure.  Only incorrect
length and timeout are not fatal. */

#ifdef HAVE_IPV6
    struct sockaddr_storage scratch, *ptr;
#else
    struct sockaddr_in scratch, *ptr;
#endif
    int n;
    int k;

/* Under normal circumstances, set up a timeout. */

    if (which < 0 || which >= MAX_SOCKETS || descriptors[which] < 0)
        fatal(0,"socket index out of range or not open",NULL);
    if (waiting > 0) {
        if (setjmp(jump_buffer)) {
            if (verbose > 2)
                fprintf(stderr,"Receive timed out\n");
            else if (verbose > 1)
                fprintf(stderr,"%s: receive timed out after %d seconds\n",
                    argv0,waiting);
            return -1;
        }
        errno = 0;
        if (signal(SIGALRM,jump_handler) == SIG_ERR)
            fatal(1,"unable to set up signal handler",NULL);
        alarm((unsigned int)waiting);
    }

/* Get the packet and clear the timeout, if any.  */

    memcpy(ptr = &scratch,&there[which],sizeof(scratch));
    n = sizeof(scratch);
    errno = 0;
    k = recvfrom(descriptors[which],packet,(size_t)length,0,
        (struct sockaddr *)ptr,&n);
    if (waiting > 0) clear_alarm();

/* Now issue some low-level diagnostics. */

    if (k <= 0) fatal(1,"unable to receive NTP packet from server",NULL);
    if (verbose > 2) {
        fprintf(stderr,"Packet of length %d received from ",k);
        display_sock_in_hex(ptr);
        fputc('\n',stderr);
    }
    return k;
}



extern int flush_socket (int which) {

/* Get rid of any outstanding input, because it may have been hanging around
for a while.  Ignore packet length oddities and return the number of packets
skipped. */

#ifdef HAVE_IPV6
    struct sockaddr_storage scratch;
#else
    struct sockaddr_in scratch;
#endif
    int n;
    char buffer[256];
    int flags, count = 0, total = 0, k;

/* The code is the obvious. */

    if (which < 0 || which >= MAX_SOCKETS || descriptors[which] < 0)
        fatal(0,"socket index out of range or not open",NULL);
    if (verbose > 2) fprintf(stderr,"Flushing outstanding packets\n");
    errno = 0;
    if ((flags = fcntl(descriptors[which],F_GETFL,0)) < 0 ||
            fcntl(descriptors[which],F_SETFL,flags|O_NONBLOCK) == -1)
        fatal(1,"unable to set non-blocking mode",NULL);
    while (1) {
        n = sizeof(scratch);
        errno = 0;
        k = recvfrom(descriptors[which],buffer,256,0,
            (struct sockaddr *)&scratch,&n);
        if (k < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            fatal(1,"unable to flush socket",NULL);
        }
        ++count;
        total += k;
    }
    errno = 0;
    if (fcntl(descriptors[which],F_SETFL,flags) == -1)
        fatal(1,"unable to restore blocking mode",NULL);
    if (verbose > 2)
        fprintf(stderr,"Flushed %d packets totalling %d bytes\n",count,total);
    return count;
}



extern void close_socket (int which) {

/* There is little point in shielding this with a timeout, because any hangs
are unlikely to be interruptible.  It can get called when the sockets haven't
been opened, so ignore that case. */

    if (which < 0 || which >= MAX_SOCKETS)
        fatal(0,"socket index out of range",NULL);
    if (descriptors[which] < 0) return;
    errno = 0;
    if (close(descriptors[which])) fatal(1,"unable to close NTP socket",NULL);
}
