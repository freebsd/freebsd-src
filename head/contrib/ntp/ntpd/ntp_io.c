/*
 * ntp_io.c - input/output routines for ntpd.	The socket-opening code
 *		   was shamelessly stolen from ntpd.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "ntp_machine.h"
#include "ntpd.h"
#include "ntp_io.h"
#include "iosignal.h"
#include "ntp_refclock.h"
#include "ntp_stdlib.h"
#include "ntp_request.h"
#include "ntp.h"
#include "ntp_unixtime.h"

/* Don't include ISC's version of IPv6 variables and structures */
#define ISC_IPV6_H 1
#include <isc/interfaceiter.h>
#include <isc/list.h>
#include <isc/result.h>

#ifdef SIM
#include "ntpsim.h"
#endif

#include <stdio.h>
#include <signal.h>
#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif /* HAVE_SYS_PARAM_H */
#ifdef HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif
#ifdef HAVE_SYS_SOCKIO_H	/* UXPV: SIOC* #defines (Frank Vance <fvance@waii.com>) */
# include <sys/sockio.h>
#endif
#ifdef HAVE_SYS_UIO_H
# include <sys/uio.h>
#endif

/*
 * setsockopt does not always have the same arg declaration
 * across all platforms. If it's not defined we make it empty
 */

#ifndef SETSOCKOPT_ARG_CAST
#define SETSOCKOPT_ARG_CAST
#endif

/* 
 * Set up some macros to look for IPv6 and IPv6 multicast
 */

#if defined(ISC_PLATFORM_HAVEIPV6) && !defined(DISABLE_IPV6)

#define INCLUDE_IPV6_SUPPORT

#if defined(INCLUDE_IPV6_SUPPORT) && defined(IPV6_JOIN_GROUP) && defined(IPV6_LEAVE_GROUP)
#define INCLUDE_IPV6_MULTICAST_SUPPORT

#endif	/* IPV6 Multicast Support */
#endif  /* IPv6 Support */

#ifdef INCLUDE_IPV6_SUPPORT
#include <netinet/in.h>
#include <net/if_var.h>
#include <netinet/in_var.h>
#endif /* !INCLUDE_IPV6_SUPPORT */

extern int listen_to_virtual_ips;
extern const char *specific_interface;

#if defined(SO_TIMESTAMP) && defined(SCM_TIMESTAMP)
#if defined(CMSG_FIRSTHDR)
#define HAVE_TIMESTAMP
#define USE_TIMESTAMP_CMSG
#ifndef TIMESTAMP_CTLMSGBUF_SIZE
#define TIMESTAMP_CTLMSGBUF_SIZE 1536 /* moderate default */
#endif
#else
/* fill in for old/other timestamp interfaces */
#endif
#endif

#if defined(SYS_WINNT)
#include <transmitbuff.h>
#include <isc/win32os.h>
/*
 * Define this macro to control the behavior of connection
 * resets on UDP sockets.  See Microsoft KnowledgeBase Article Q263823
 * for details.
 * NOTE: This requires that Windows 2000 systems install Service Pack 2
 * or later.
 */
#ifndef SIO_UDP_CONNRESET 
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR,12) 
#endif

/*
 * Windows C runtime ioctl() can't deal properly with sockets, 
 * map to ioctlsocket for this source file.
 */
#define ioctl(fd, opt, val)  ioctlsocket((fd), (opt), (u_long *)(val))
#endif  /* SYS_WINNT */

/*
 * We do asynchronous input using the SIGIO facility.  A number of
 * recvbuf buffers are preallocated for input.	In the signal
 * handler we poll to see which sockets are ready and read the
 * packets from them into the recvbuf's along with a time stamp and
 * an indication of the source host and the interface it was received
 * through.  This allows us to get as accurate receive time stamps
 * as possible independent of other processing going on.
 *
 * We watch the number of recvbufs available to the signal handler
 * and allocate more when this number drops below the low water
 * mark.  If the signal handler should run out of buffers in the
 * interim it will drop incoming frames, the idea being that it is
 * better to drop a packet than to be inaccurate.
 */


/*
 * Other statistics of possible interest
 */
volatile u_long packets_dropped;	/* total number of packets dropped on reception */
volatile u_long packets_ignored;	/* packets received on wild card interface */
volatile u_long packets_received;	/* total number of packets received */
u_long packets_sent;	/* total number of packets sent */
u_long packets_notsent; /* total number of packets which couldn't be sent */

volatile u_long handler_calls;	/* number of calls to interrupt handler */
volatile u_long handler_pkts;	/* number of pkts received by handler */
u_long io_timereset;		/* time counters were reset */

/*
 * Interface stuff
 */
struct interface *any_interface;	/* default ipv4 interface */
struct interface *any6_interface;	/* default ipv6 interface */
struct interface *loopback_interface;	/* loopback ipv4 interface */

int ninterfaces;			/* Total number of interfaces */

volatile int disable_dynamic_updates;   /* when set to != 0 dynamic updates won't happen */

#ifdef REFCLOCK
/*
 * Refclock stuff.	We keep a chain of structures with data concerning
 * the guys we are doing I/O for.
 */
static	struct refclockio *refio;
#endif /* REFCLOCK */


/*
 * Define what the possible "soft" errors can be.  These are non-fatal returns
 * of various network related functions, like recv() and so on.
 *
 * For some reason, BSDI (and perhaps others) will sometimes return <0
 * from recv() but will have errno==0.  This is broken, but we have to
 * work around it here.
 */
#define SOFT_ERROR(e)	((e) == EAGAIN || \
			 (e) == EWOULDBLOCK || \
			 (e) == EINTR || \
			 (e) == 0)

/*
 * File descriptor masks etc. for call to select
 * Not needed for I/O Completion Ports
 */
fd_set activefds;
int maxactivefd;
/*
 * bit alternating value to detect verified interfaces during an update cycle
 */
static  u_char          sys_interphase = 0;

static  struct interface *new_interface P((struct interface *));
static  void add_interface P((struct interface *));
static  int update_interfaces P((u_short, interface_receiver_t, void *));
static  void remove_interface P((struct interface *));
static  struct interface *create_interface P((u_short, struct interface *));

static int	move_fd		P((SOCKET));

/*
 * Multicast functions
 */
static	isc_boolean_t	addr_ismulticast	 P((struct sockaddr_storage *));
/*
 * Not all platforms support multicast
 */
#ifdef MCAST
static	isc_boolean_t	socket_multicast_enable	 P((struct interface *, int, struct sockaddr_storage *));
static	isc_boolean_t	socket_multicast_disable P((struct interface *, struct sockaddr_storage *));
#endif

#ifdef DEBUG
static void print_interface	P((struct interface *, char *, char *));
#define DPRINT_INTERFACE(_LVL_, _ARGS_) do { if (debug >= (_LVL_)) { print_interface _ARGS_; } } while (0)
#else
#define DPRINT_INTERFACE(_LVL_, _ARGS_) do {} while (0)
#endif

typedef struct vsock vsock_t;
enum desc_type { FD_TYPE_SOCKET, FD_TYPE_FILE };

struct vsock {
	SOCKET				fd;
	enum desc_type                  type;
	ISC_LINK(vsock_t)		link;
};

#if !defined(HAVE_IO_COMPLETION_PORT) && defined(HAS_ROUTING_SOCKET)
/*
 * async notification processing (e. g. routing sockets)
 */
/*
 * support for receiving data on fd that is not a refclock or a socket
 * like e. g. routing sockets
 */
struct asyncio_reader {
	SOCKET fd;		                    /* fd to be read */
	void  *data;		                    /* possibly local data */
	void (*receiver)(struct asyncio_reader *);  /* input handler */
	ISC_LINK(struct asyncio_reader) link;       /* the list this is being kept in */
};

ISC_LIST(struct asyncio_reader) asyncio_reader_list;

static void delete_asyncio_reader P((struct asyncio_reader *));
static struct asyncio_reader *new_asyncio_reader P((void));
static void add_asyncio_reader P((struct asyncio_reader *, enum desc_type));
static void remove_asyncio_reader P((struct asyncio_reader *));

#endif /* !defined(HAVE_IO_COMPLETION_PORT) && defined(HAS_ROUTING_SOCKET) */

static void init_async_notifications P((void));

static	int create_sockets	P((u_short));
static	SOCKET	open_socket	P((struct sockaddr_storage *, int, int, struct interface *));
static	char *	fdbits		P((int, fd_set *));
static	void	set_reuseaddr	P((int));
static	isc_boolean_t	socket_broadcast_enable	 P((struct interface *, SOCKET, struct sockaddr_storage *));
static	isc_boolean_t	socket_broadcast_disable P((struct interface *, struct sockaddr_storage *));

ISC_LIST(vsock_t)	fd_list;

typedef struct remaddr remaddr_t;

struct remaddr {
      struct sockaddr_storage	 addr;
      struct interface               *interface;
      ISC_LINK(remaddr_t)	 link;
};

ISC_LIST(remaddr_t)       remoteaddr_list;

ISC_LIST(struct interface)     inter_list;

static struct interface *wildipv4 = NULL;
static struct interface *wildipv6 = NULL;

static void	add_fd_to_list	P((SOCKET, enum desc_type));
static void	close_and_delete_fd_from_list	P((SOCKET));
static void	add_addr_to_list	P((struct sockaddr_storage *, struct interface *));
static void	delete_addr_from_list	P((struct sockaddr_storage *));
static struct interface *find_addr_in_list	P((struct sockaddr_storage *));
static struct interface *find_flagged_addr_in_list P((struct sockaddr_storage *, int));
static void	create_wildcards	P((u_short));
static isc_boolean_t	address_okay	P((struct interface *));
static void		convert_isc_if		P((isc_interface_t *, struct interface *, u_short));
static void	delete_interface_from_list	P((struct interface *));
static struct interface *getinterface	P((struct sockaddr_storage *, int));
static struct interface *findlocalinterface	P((struct sockaddr_storage *, int));
static struct interface *findlocalcastinterface	P((struct sockaddr_storage *, int));

/*
 * Routines to read the ntp packets
 */
#if !defined(HAVE_IO_COMPLETION_PORT)
static inline int     read_network_packet	P((SOCKET, struct interface *, l_fp));
static inline int     read_refclock_packet	P((SOCKET, struct refclockio *, l_fp));
#endif

#ifdef SYS_WINNT
/*
 * Windows 2000 systems incorrectly cause UDP sockets using WASRecvFrom
 * to not work correctly, returning a WSACONNRESET error when a WSASendTo
 * fails with an "ICMP port unreachable" response and preventing the
 * socket from using the WSARecvFrom in subsequent operations.
 * The function below fixes this, but requires that Windows 2000
 * Service Pack 2 or later be installed on the system.  NT 4.0
 * systems are not affected by this and work correctly.
 * See Microsoft Knowledge Base Article Q263823 for details of this.
 */
void
connection_reset_fix(
	SOCKET fd,
	struct sockaddr_storage *addr
	)
{
	DWORD dwBytesReturned = 0;
	BOOL  bNewBehavior = FALSE;
	DWORD status;

	/*
	 * disable bad behavior using IOCTL: SIO_UDP_CONNRESET
	 * NT 4.0 has no problem
	 */
	if (isc_win32os_majorversion() >= 5) {
		status = WSAIoctl(fd, SIO_UDP_CONNRESET, &bNewBehavior,
				  sizeof(bNewBehavior), NULL, 0,
				  &dwBytesReturned, NULL, NULL);
		if (SOCKET_ERROR == status)
			netsyslog(LOG_ERR, "connection_reset_fix() "
					   "failed for address %s: %m", 
					   stoa(addr));
	}
}
#endif

/*
 * on Unix systems the stdio library typically
 * makes use of file descriptors in the lower
 * integer range. stdio usually will make use
 * of the file descriptor in the range of
 * [0..FOPEN_MAX)
 * in order to keep this range clean for socket
 * file descriptors we attempt to move them above
 * FOPEM_MAX. This is not as easy as it sounds as
 * FOPEN_MAX changes from implementation to implementation
 * and may exceed to current file decriptor limits.
 * We are using following strategy:
 * - keep a current socket fd boundary initialized with
 *   max(0, min(getdtablesize() - FD_CHUNK, FOPEN_MAX))
 * - attempt to move the descriptor to the boundary or
 *   above.
 *   - if that fails and boundary > 0 set boundary
 *     to min(0, socket_fd_boundary - FD_CHUNK)
 *     -> retry
 *     if failure and boundary == 0 return old fd
 *   - on success close old fd return new fd
 *
 * effects:
 *   - fds will be moved above the socket fd boundary
 *     if at all possible.
 *   - the socket boundary will be reduced until
 *     allocation is possible or 0 is reached - at this
 *     point the algrithm will be disabled
 */
static int move_fd(SOCKET fd)
{
#if !defined(SYS_WINNT) && defined(F_DUPFD)
#ifndef FD_CHUNK
#define FD_CHUNK	10
#endif
/*
 * number of fds we would like to have for
 * stdio FILE* available.
 * we can pick a "low" number as our use of
 * FILE* is limited to log files and temporarily
 * to data and config files. Except for log files
 * we don't keep the other FILE* open beyond the
 * scope of the function that opened it.
 */
#ifndef FD_PREFERRED_SOCKBOUNDARY
#define FD_PREFERRED_SOCKBOUNDARY 48
#endif

#ifndef HAVE_GETDTABLESIZE
/*
 * if we have no idea about the max fd value set up things
 * so we will start at FOPEN_MAX
 */
#define getdtablesize() (FOPEN_MAX+FD_CHUNK)
#endif

#ifndef FOPEN_MAX
#define FOPEN_MAX	20	/* assume that for the lack of anything better */
#endif
	static SOCKET socket_boundary = -1;
	SOCKET newfd;

	/*
	 * check whether boundary has be set up
	 * already
	 */
	if (socket_boundary == -1) {
		socket_boundary = max(0, min(getdtablesize() - FD_CHUNK, 
					     min(FOPEN_MAX, FD_PREFERRED_SOCKBOUNDARY)));
#ifdef DEBUG
		msyslog(LOG_DEBUG, "ntp_io: estimated max descriptors: %d, initial socket boundary: %d",
			getdtablesize(), socket_boundary);
#endif
	}

	/*
	 * Leave a space for stdio to work in. potentially moving the
	 * socket_boundary lower until allocation succeeds.
	 */
	do {
		if (fd >= 0 && fd < socket_boundary) {
			/* inside reserved range: attempt to move fd */
			newfd = fcntl(fd, F_DUPFD, socket_boundary);
			
			if (newfd != -1) {
				/* success: drop the old one - return the new one */
				(void)close(fd);
				return (newfd);
			}
		} else {
			/* outside reserved range: no work - return the original one */
			return (fd);
		}
		socket_boundary = max(0, socket_boundary - FD_CHUNK);
#ifdef DEBUG
		msyslog(LOG_DEBUG, "ntp_io: selecting new socket boundary: %d",
			socket_boundary);
#endif
	} while (socket_boundary > 0);
#endif /* !defined(SYS_WINNT) && defined(F_DUPFD) */
	return (fd);
}

#ifdef DEBUG_TIMING
/*
 * collect timing information for various processing
 * paths. currently we only pass then on to the file
 * for later processing. this could also do histogram
 * based analysis in other to reduce the load (and skew)
 * dur to the file output
 */
void
collect_timing(struct recvbuf *rb, const char *tag, int count, l_fp *dts)
{
	char buf[2048];

	snprintf(buf, sizeof(buf), "%s %d %s %s", 
		 (rb != NULL) ? 
		 ((rb->dstadr) ? stoa(&rb->recv_srcadr) : "-REFCLOCK-") : "-",
		 count, lfptoa(dts, 9), tag);
	record_timing_stats(buf);
}
#endif
  
/*
 * About dynamic interfaces, sockets, reception and more...
 *
 * the code solves following tasks:
 *
 *   - keep a current list of active interfaces in order
 *     to bind to to the interface address on NTP_PORT so that
 *     all wild and specific bindings for NTP_PORT are taken by ntpd
 *     to avoid other daemons messing with the time or sockets.
 *   - all interfaces keep a list of peers that are referencing 
 *     the interface in order to quickly re-assign the peers to
 *     new interface in case an interface is deleted (=> gone from system or
 *     down)
 *   - have a preconfigured socket ready with the right local address
 *     for transmission and reception
 *   - have an address list for all destination addresses used within ntpd
 *     to find the "right" preconfigured socket.
 *   - facilitate updating the internal interface list with respect to
 *     the current kernel state
 *
 * special issues:
 *
 *   - mapping of multicast addresses to the interface affected is not always
 *     one to one - especially on hosts with multiple interfaces
 *     the code here currently allocates a separate interface entry for those
 *     multicast addresses
 *     iff it is able to bind to a *new* socket with the multicast address (flags |= MCASTIF)
 *     in case of failure the multicast address is bound to an existing interface.
 *   - on some systems it is perfectly legal to assign the same address to
 *     multiple interfaces. Therefore this code does not keep a list of interfaces
 *     but a list of interfaces that represent a unique address as determined by the kernel
 *     by the procedure in findlocalinterface. Thus it is perfectly legal to see only
 *     one representative of a group of real interfaces if they share the same address.
 * 
 * Frank Kardel 20050910
 */

/*
 * init_io - initialize I/O data structures and call socket creation routine
 */
void
init_io(void)
{
#ifdef SYS_WINNT
	init_io_completion_port();

	if (!Win32InitSockets())
	{
		netsyslog(LOG_ERR, "No useable winsock.dll: %m");
		exit(1);
	}
	init_transmitbuff();
#endif /* SYS_WINNT */

	/*
	 * Init buffer free list and stat counters
	 */
	init_recvbuff(RECV_INIT);

	packets_dropped = packets_received = 0;
	packets_ignored = 0;
	packets_sent = packets_notsent = 0;
	handler_calls = handler_pkts = 0;
	io_timereset = 0;
	loopback_interface = NULL;
	any_interface = NULL;
	any6_interface = NULL;

#ifdef REFCLOCK
	refio = NULL;
#endif

#if defined(HAVE_SIGNALED_IO)
	(void) set_signal();
#endif

	ISC_LIST_INIT(fd_list);

#if !defined(HAVE_IO_COMPLETION_PORT) && defined(HAS_ROUTING_SOCKET)
	ISC_LIST_INIT(asyncio_reader_list);
#endif

        ISC_LIST_INIT(remoteaddr_list);

	ISC_LIST_INIT(inter_list);

	/*
	 * Create the sockets
	 */
	BLOCKIO();
	(void) create_sockets(htons(NTP_PORT));
	UNBLOCKIO();

	init_async_notifications();

	DPRINTF(3, ("init_io: maxactivefd %d\n", maxactivefd));
}

#ifdef DEBUG
/*
 * function to dump the contents of the interface structure
 * for debugging use only.
 */
void
interface_dump(struct interface *itf)
{
	u_char* cp;
	int i;
	/* Limit the size of the sockaddr_storage hex dump */
	int maxsize = min(32, sizeof(struct sockaddr_storage));

	printf("Dumping interface: %p\n", itf);
	printf("fd = %d\n", itf->fd);
	printf("bfd = %d\n", itf->bfd);
	printf("sin = %s,\n", stoa(&(itf->sin)));
	cp = (u_char*) &(itf->sin);
	for(i = 0; i < maxsize; i++)
	{
		printf("%02x", *cp++);
		if((i+1)%4 == 0)
			printf(" ");
	}
	printf("\n");
	printf("bcast = %s,\n", stoa(&(itf->bcast)));
	cp = (u_char*) &(itf->bcast);
	for(i = 0; i < maxsize; i++)
	{
		printf("%02x", *cp++);
		if((i+1)%4 == 0)
			printf(" ");
	}
	printf("\n");
	printf("mask = %s,\n", stoa(&(itf->mask)));
	cp = (u_char*) &(itf->mask);
	for(i = 0; i < maxsize; i++)
	{
		printf("%02x", *cp++);
		if((i+1)%4 == 0)
			printf(" ");
	}
	printf("\n");
	printf("name = %s\n", itf->name);
	printf("flags = 0x%08x\n", itf->flags);
	printf("last_ttl = %d\n", itf->last_ttl);
	printf("addr_refid = %08x\n", itf->addr_refid);
	printf("num_mcast = %d\n", itf->num_mcast);
	printf("received = %ld\n", itf->received);
	printf("sent = %ld\n", itf->sent);
	printf("notsent = %ld\n", itf->notsent);
	printf("ifindex = %u\n", itf->ifindex);
	printf("scopeid = %u\n", itf->scopeid);
	printf("peercnt = %u\n", itf->peercnt);
	printf("phase = %u\n", itf->phase);
}

/*
 * print_interface - helper to output debug information
 */
static void
print_interface(struct interface *iface, char *pfx, char *sfx)
{
	printf("%sinterface #%d: fd=%d, bfd=%d, name=%s, flags=0x%x, scope=%d, ifindex=%d",
	       pfx,
	       iface->ifnum,
	       iface->fd,
	       iface->bfd,
	       iface->name,
	       iface->flags,
	       iface->scopeid,
	       iface->ifindex);
	/* Leave these as three printf calls. */
	printf(", sin=%s",
	       stoa((&iface->sin)));
	if (iface->flags & INT_BROADCAST)
		printf(", bcast=%s,",
		       stoa((&iface->bcast)));
	if (iface->family == AF_INET)
	  printf(", mask=%s",
		 stoa((&iface->mask)));
	printf(", %s:%s", iface->ignore_packets == ISC_FALSE ? "Enabled" : "Disabled", sfx);
	if (debug > 4)	/* in-depth debugging only */
		interface_dump(iface);
}

#endif

#if !defined(HAVE_IO_COMPLETION_PORT) && defined(HAS_ROUTING_SOCKET)
/*
 * create an asyncio_reader structure
 */
static struct asyncio_reader *
new_asyncio_reader()
{
	struct asyncio_reader *reader;

	reader = (struct asyncio_reader *)emalloc(sizeof(struct asyncio_reader));

	memset((char *)reader, 0, sizeof(*reader));
	ISC_LINK_INIT(reader, link);
	reader->fd = INVALID_SOCKET;
	return reader;
}

/*
 * delete a reader
 */
static void
delete_asyncio_reader(struct asyncio_reader *reader)
{
	free(reader);
}

/*
 * add asynchio_reader
 */
static void
add_asyncio_reader(struct asyncio_reader *reader, enum desc_type type)
{
	ISC_LIST_APPEND(asyncio_reader_list, reader, link);
	add_fd_to_list(reader->fd, type);
}
	
/*
 * remove asynchio_reader
 */
static void
remove_asyncio_reader(struct asyncio_reader *reader)
{
	ISC_LIST_UNLINK_TYPE(asyncio_reader_list, reader, link, struct asyncio_reader);

	if (reader->fd != INVALID_SOCKET)
		close_and_delete_fd_from_list(reader->fd);

	reader->fd = INVALID_SOCKET;
}
#endif /* !defined(HAVE_IO_COMPLETION_PORT) && defined(HAS_ROUTING_SOCKET) */

/*
 * interface list enumerator - visitor pattern
 */
void
interface_enumerate(interface_receiver_t receiver, void *data)
{
	interface_info_t ifi;
        struct interface *interf;

	ifi.action = IFS_EXISTS;
	
	for (interf = ISC_LIST_HEAD(inter_list);
	     interf != NULL;
	     interf = ISC_LIST_NEXT(interf, link)) {
		ifi.interface = interf;
		receiver(data, &ifi);
	}
}

/*
 * do standard initialization of interface structure
 */
static void
init_interface(struct interface *interface)
{
	memset((char *)interface, 0, sizeof(struct interface));
	ISC_LINK_INIT(interface, link);
	ISC_LIST_INIT(interface->peers);
	interface->fd = INVALID_SOCKET;
	interface->bfd = INVALID_SOCKET;
	interface->num_mcast = 0;
	interface->received = 0;
	interface->sent = 0;
	interface->notsent = 0;
	interface->peercnt = 0;
	interface->phase = sys_interphase;
}

/*
 * create new interface structure initialize from
 * template structure or via standard initialization
 * function
 */
static struct interface *
new_interface(struct interface *interface)
{
	static u_int sys_ifnum = 0;

	struct interface *iface = (struct interface *)emalloc(sizeof(struct interface));

	if (interface != NULL)
	{
		memcpy((char*)iface, (char*)interface, sizeof(*interface));
	}
	else
	{
		init_interface(iface);
	}

	iface->ifnum = sys_ifnum++;  /* count every new instance of an interface in the system */
	iface->starttime = current_time;

	return iface;
}

/*
 * return interface storage into free memory pool
 */
static void
delete_interface(struct interface *interface)
{
	free(interface);
}

/*
 * link interface into list of known interfaces
 */
static void
add_interface(struct interface *interface)
{
	static struct interface *listhead = NULL;

	/*
	 * For ntpd, the first few interfaces (wildcard, localhost)
	 * will never be removed.  This means inter_list.head is
	 * unchanging once initialized.  Take advantage of that to
	 * watch for changes and catch corruption earlier.  This
	 * helped track down corruption caused by using FD_SET with
	 * a descriptor numerically larger than FD_SETSIZE.
	 */
	if (NULL == listhead)
		listhead = inter_list.head;

	if (listhead != inter_list.head) {
		msyslog(LOG_ERR, "add_interface inter_list.head corrupted: was %p now %p",
			listhead, inter_list.head);
		exit(1);
	}
	/*
	 * Calculate the address hash
	 */
	interface->addr_refid = addr2refid(&interface->sin);
	
	ISC_LIST_APPEND(inter_list, interface, link);
	ninterfaces++;
}

/*
 * remove interface from known interface list and clean up
 * associated resources
 */
static void
remove_interface(struct interface *interface)
{
	struct sockaddr_storage resmask;

	ISC_LIST_UNLINK_TYPE(inter_list, interface, link, struct interface);

	delete_interface_from_list(interface);
  
	if (interface->fd != INVALID_SOCKET) 
	{
		msyslog(LOG_INFO, "Deleting interface #%d %s, %s#%d, interface stats: received=%ld, sent=%ld, dropped=%ld, active_time=%ld secs",
			interface->ifnum,
			interface->name,
			stoa((&interface->sin)),
			NTP_PORT,  /* XXX should extract port from sin structure */
			interface->received,
			interface->sent,
			interface->notsent,
			current_time - interface->starttime);

		close_and_delete_fd_from_list(interface->fd);
	}
  
	if (interface->bfd != INVALID_SOCKET) 
	{
		msyslog(LOG_INFO, "Deleting interface #%d %s, broadcast address %s#%d",
			interface->ifnum,
			interface->name,
			stoa((&interface->bcast)),
			(u_short) NTP_PORT);  /* XXX extract port from sin structure */
		close_and_delete_fd_from_list(interface->bfd);
	}

	ninterfaces--;
	ntp_monclearinterface(interface);

	/* remove restrict interface entry */

	/*
	 * Blacklist bound interface address
	 */
	SET_HOSTMASK(&resmask, interface->sin.ss_family);
	hack_restrict(RESTRICT_REMOVEIF, &interface->sin, &resmask,
		      RESM_NTPONLY|RESM_INTERFACE, RES_IGNORE);
}

static void
list_if_listening(struct interface *interface, u_short port)
{
	msyslog(LOG_INFO, "Listening on interface #%d %s, %s#%d %s",
		interface->ifnum,
		interface->name,
		stoa((&interface->sin)),
		ntohs( (u_short) port),
		(interface->ignore_packets == ISC_FALSE) ?
		"Enabled": "Disabled");
}

static void
create_wildcards(u_short port) {
	isc_boolean_t okipv4 = ISC_TRUE;
	/*
	 * create pseudo-interface with wildcard IPv4 address
	 */
#ifdef IPV6_V6ONLY
	if(isc_net_probeipv4() != ISC_R_SUCCESS)
		okipv4 = ISC_FALSE;
#endif

	if(okipv4 == ISC_TRUE) {
	        struct interface *interface = new_interface(NULL);

		interface->family = AF_INET;
		interface->sin.ss_family = AF_INET;
		((struct sockaddr_in*)&interface->sin)->sin_addr.s_addr = htonl(INADDR_ANY);
		((struct sockaddr_in*)&interface->sin)->sin_port = port;
		(void) strncpy(interface->name, "wildcard", sizeof(interface->name));
		interface->mask.ss_family = AF_INET;
		((struct sockaddr_in*)&interface->mask)->sin_addr.s_addr = htonl(~(u_int32)0);
		interface->flags = INT_BROADCAST | INT_UP | INT_WILDCARD;
		interface->ignore_packets = ISC_TRUE;
#if defined(MCAST)
		/*
		 * enable possible multicast reception on the broadcast socket
		 */
		interface->bcast.ss_family = AF_INET;
		((struct sockaddr_in*)&interface->bcast)->sin_port = port;
		((struct sockaddr_in*)&interface->bcast)->sin_addr.s_addr = htonl(INADDR_ANY);
#endif /* MCAST */
		interface->fd = open_socket(&interface->sin,
				 interface->flags, 1, interface);

		if (interface->fd != INVALID_SOCKET) {
			wildipv4 = interface;
			any_interface = interface;
			
			add_addr_to_list(&interface->sin, interface);
			add_interface(interface);
			list_if_listening(interface, port);
		} else {
			msyslog(LOG_ERR, "unable to bind to wildcard socket address %s - another process may be running - EXITING",
				stoa((&interface->sin)));
			exit(1);
		}
	}

#ifdef INCLUDE_IPV6_SUPPORT
	/*
	 * create pseudo-interface with wildcard IPv6 address
	 */
	if (isc_net_probeipv6() == ISC_R_SUCCESS) {
	        struct interface *interface = new_interface(NULL);

		interface->family = AF_INET6;
		interface->sin.ss_family = AF_INET6;
		((struct sockaddr_in6*)&interface->sin)->sin6_addr = in6addr_any;
 		((struct sockaddr_in6*)&interface->sin)->sin6_port = port;
# ifdef ISC_PLATFORM_HAVESCOPEID
 		((struct sockaddr_in6*)&interface->sin)->sin6_scope_id = 0;
# endif
		(void) strncpy(interface->name, "wildcard", sizeof(interface->name));
		interface->mask.ss_family = AF_INET6;
		memset(&((struct sockaddr_in6*)&interface->mask)->sin6_addr.s6_addr, 0xff, sizeof(struct in6_addr));
		interface->flags = INT_UP | INT_WILDCARD;
		interface->ignore_packets = ISC_TRUE;

		interface->fd = open_socket(&interface->sin,
				 interface->flags, 1, interface);

		if (interface->fd != INVALID_SOCKET) {
			wildipv6 = interface;
			any6_interface = interface;
			add_addr_to_list(&interface->sin, interface);
			add_interface(interface);
			list_if_listening(interface, port);
		} else {
			msyslog(LOG_ERR, "unable to bind to wildcard socket address %s - another process may be running - EXITING",
				stoa((&interface->sin)));
			exit(1);
		}
	}
#endif
}


static isc_boolean_t
address_okay(struct interface *iface) {

	DPRINTF(4, ("address_okay: listen Virtual: %d, IF name: %s\n", 
		    listen_to_virtual_ips, iface->name));

	/*
	 * Always allow the loopback
	 */
	if((iface->flags & INT_LOOPBACK) != 0) {
		DPRINTF(4, ("address_okay: loopback - OK\n"));
		return (ISC_TRUE);
	}

	/*
	 * Check if the interface is specified
	 */
	if (specific_interface != NULL) {
		if (strcasecmp(iface->name, specific_interface) == 0) {
			DPRINTF(4, ("address_okay: specific interface name matched - OK\n"));
			return (ISC_TRUE);
		} else {
			DPRINTF(4, ("address_okay: specific interface name NOT matched - FAIL\n"));
			return (ISC_FALSE);
		}
	}
	else {
		if (listen_to_virtual_ips == 0  && 
		    (strchr(iface->name, (int)':') != NULL)) {
			DPRINTF(4, ("address_okay: virtual ip/alias - FAIL\n"));
			return (ISC_FALSE);
		}
	}

	DPRINTF(4, ("address_okay: OK\n"));
	return (ISC_TRUE);
}

static void
convert_isc_if(isc_interface_t *isc_if, struct interface *itf, u_short port)
{
	itf->scopeid = 0;
	itf->family = (short) isc_if->af;
	strcpy(itf->name, isc_if->name);

	if(isc_if->af == AF_INET) {
		itf->sin.ss_family = (u_short) isc_if->af;
		memcpy(&(((struct sockaddr_in*)&itf->sin)->sin_addr),
		       &(isc_if->address.type.in),
		       sizeof(struct in_addr));
		((struct sockaddr_in*)&itf->sin)->sin_port = port;

		if((isc_if->flags & INTERFACE_F_BROADCAST) != 0) {
			itf->flags |= INT_BROADCAST;
			itf->bcast.ss_family = itf->sin.ss_family;
			memcpy(&(((struct sockaddr_in*)&itf->bcast)->sin_addr),
			       &(isc_if->broadcast.type.in),
				 sizeof(struct in_addr));
			((struct sockaddr_in*)&itf->bcast)->sin_port = port;
		}

		itf->mask.ss_family = itf->sin.ss_family;
		memcpy(&(((struct sockaddr_in*)&itf->mask)->sin_addr),
		       &(isc_if->netmask.type.in),
		       sizeof(struct in_addr));
		((struct sockaddr_in*)&itf->mask)->sin_port = port;
	}
#ifdef INCLUDE_IPV6_SUPPORT
	else if (isc_if->af == AF_INET6) {
		itf->sin.ss_family = (u_short) isc_if->af;
		memcpy(&(((struct sockaddr_in6 *)&itf->sin)->sin6_addr),
		       &(isc_if->address.type.in6),
		       sizeof(((struct sockaddr_in6 *)&itf->sin)->sin6_addr));
		((struct sockaddr_in6 *)&itf->sin)->sin6_port = port;

#ifdef ISC_PLATFORM_HAVESCOPEID
		((struct sockaddr_in6 *)&itf->sin)->sin6_scope_id = isc_netaddr_getzone(&isc_if->address);
		itf->scopeid = isc_netaddr_getzone(&isc_if->address);
#endif
		itf->mask.ss_family = itf->sin.ss_family;
		memcpy(&(((struct sockaddr_in6 *)&itf->mask)->sin6_addr),
		       &(isc_if->netmask.type.in6),
		       sizeof(struct in6_addr));
		((struct sockaddr_in6 *)&itf->mask)->sin6_port = port;
		/* Copy the interface index */
		itf->ifindex = isc_if->ifindex;
	}
#endif /* INCLUDE_IPV6_SUPPORT */


	/* Process the rest of the flags */

	if((isc_if->flags & INTERFACE_F_UP) != 0)
		itf->flags |= INT_UP;
	if((isc_if->flags & INTERFACE_F_LOOPBACK) != 0)
		itf->flags |= INT_LOOPBACK;
	if((isc_if->flags & INTERFACE_F_POINTTOPOINT) != 0)
		itf->flags |= INT_PPP;
	if((isc_if->flags & INTERFACE_F_MULTICAST) != 0)
		itf->flags |= INT_MULTICAST;

}

/*
 * refresh_interface
 *
 * some OSes have been observed to keep
 * cached routes even when more specific routes
 * become available.
 * this can be mitigated by re-binding
 * the socket.
 */
static int
refresh_interface(struct interface * interface)
{
#ifdef  OS_MISSES_SPECIFIC_ROUTE_UPDATES
	if (interface->fd != INVALID_SOCKET)
	{
		close_and_delete_fd_from_list(interface->fd);
		interface->fd = open_socket(&interface->sin,
					    interface->flags, 0, interface);
		 /*
		  * reset TTL indication so TTL is is set again 
		  * next time around
		  */
		interface->last_ttl = 0;
		return interface->fd != INVALID_SOCKET;
	}
	else
	{
		return 0;	/* invalid sockets are not refreshable */
	}
#else /* !OS_MISSES_SPECIFIC_ROUTE_UPDATES */
	return interface->fd != INVALID_SOCKET;
#endif /* !OS_MISSES_SPECIFIC_ROUTE_UPDATES */
}

/*
 * interface_update - externally callable update function
 */
void
interface_update(interface_receiver_t receiver, void *data)
{
	if (!disable_dynamic_updates) {
		int new_interface_found;

		BLOCKIO();
		new_interface_found = update_interfaces(htons(NTP_PORT), receiver, data);
		UNBLOCKIO();

		if (new_interface_found) {
#ifdef DEBUG
			msyslog(LOG_DEBUG, "new interface(s) found: waking up resolver");
#endif
#ifdef SYS_WINNT
			/* wake up the resolver thread */
			if (ResolverEventHandle != NULL)
				SetEvent(ResolverEventHandle);
#else
			/* write any single byte to the pipe to wake up the resolver process */
			write( resolver_pipe_fd[1], &new_interface_found, 1 );
#endif
		}
	}
}

/*
 * find out if a given interface structure contains
 * a wildcard address
 */
static int
is_wildcard_addr(struct sockaddr_storage *sas)
{
	if (sas->ss_family == AF_INET &&
	    ((struct sockaddr_in*)sas)->sin_addr.s_addr == htonl(INADDR_ANY))
		return 1;

#ifdef INCLUDE_IPV6_SUPPORT
	if (sas->ss_family == AF_INET6 &&
	    memcmp(&((struct sockaddr_in6*)sas)->sin6_addr, &in6addr_any,
		   sizeof(in6addr_any) == 0))
		return 1;
#endif

	return 0;
}

#ifdef OS_NEEDS_REUSEADDR_FOR_IFADDRBIND
/*
 * enable/disable re-use of wildcard address socket
 */
static void
set_wildcard_reuse(int family, int on)
{
	int onvalue = 1;
	int offvalue = 0;
	int *onoff;
	SOCKET fd = INVALID_SOCKET;

	onoff = on ? &onvalue : &offvalue;

	switch (family) {
	case AF_INET:
		if (any_interface) {
			fd = any_interface->fd;
		}
		break;

#ifdef INCLUDE_IPV6_SUPPORT
	case AF_INET6:
		if (any6_interface) {
			fd = any6_interface->fd;
		}
		break;
#endif /* !INCLUDE_IPV6_SUPPORT */
	}

	if (fd != INVALID_SOCKET) {
		if (setsockopt(fd, SOL_SOCKET,
			       SO_REUSEADDR, (char *)onoff,
			       sizeof(*onoff))) {
			netsyslog(LOG_ERR, "set_wildcard_reuse: setsockopt(SO_REUSEADDR, %s) failed: %m", *onoff ? "on" : "off");
		}
		DPRINTF(4, ("set SO_REUSEADDR to %s on %s\n", *onoff ? "ON" : "OFF",
			    stoa((family == AF_INET) ?
				  &any_interface->sin : &any6_interface->sin)));
	}
}
#endif /* OS_NEEDS_REUSEADDR_FOR_IFADDRBIND */

#ifdef INCLUDE_IPV6_SUPPORT
static isc_boolean_t
is_anycast(struct sockaddr *sa, char *name)
{
#if defined(SIOCGIFAFLAG_IN6) && defined(IN6_IFF_ANYCAST)
	struct in6_ifreq ifr6;
	int fd;
	u_int32_t flags6;

	if (sa->sa_family != AF_INET6)
		return ISC_FALSE;
	if ((fd = socket(AF_INET6, SOCK_DGRAM, 0)) < 0)
		return ISC_FALSE;
	memset(&ifr6, 0, sizeof(ifr6));
	memcpy(&ifr6.ifr_addr, (struct sockaddr_in6 *)sa,
	    sizeof(struct sockaddr_in6));
	strlcpy(ifr6.ifr_name, name, IF_NAMESIZE);
	if (ioctl(fd, SIOCGIFAFLAG_IN6, &ifr6) < 0) {
		close(fd);
		return ISC_FALSE;
	}
	close(fd);
	flags6 = ifr6.ifr_ifru.ifru_flags6;
	if ((flags6 & IN6_IFF_ANYCAST) != 0)
		return ISC_TRUE;
#endif /* !SIOCGIFAFLAG_IN6 || !IN6_IFF_ANYCAST */
	return ISC_FALSE;
}
#endif /* !INCLUDE_IPV6_SUPPORT */

/*
 * update_interface strategy
 *
 * toggle configuration phase
 *
 * Phase 1:
 * forall currently existing interfaces
 *   if address is known:
 *       drop socket - rebind again
 *
 *   if address is NOT known:
 *     attempt to create a new interface entry
 *
 * Phase 2:
 * forall currently known non MCAST and WILDCARD interfaces
 *   if interface does not match configuration phase (not seen in phase 1):
 *     remove interface from known interface list
 *     forall peers associated with this interface
 *       disconnect peer from this interface
 *
 * Phase 3:
 *   attempt to re-assign interfaces to peers
 *
 */

static int
update_interfaces(
	u_short port,
	interface_receiver_t receiver,
	void *data
	)
{
	interface_info_t ifi;
	isc_mem_t *mctx = NULL;
	isc_interfaceiter_t *iter = NULL;
	isc_boolean_t scan_ipv4 = ISC_FALSE;
	isc_boolean_t scan_ipv6 = ISC_FALSE;
	isc_result_t result;
	int new_interface_found = 0;

	DPRINTF(3, ("update_interfaces(%d)\n", ntohs( (u_short) port)));

#ifdef INCLUDE_IPV6_SUPPORT
	if (isc_net_probeipv6() == ISC_R_SUCCESS)
		scan_ipv6 = ISC_TRUE;
#if defined(DEBUG)
	else
		if (debug)
			netsyslog(LOG_ERR, "no IPv6 interfaces found");
#endif
#endif
	if (isc_net_probeipv6() == ISC_R_SUCCESS)
		scan_ipv6 = ISC_TRUE;
#if defined(ISC_PLATFORM_HAVEIPV6) && defined(DEBUG)
	else
		if (debug)
			netsyslog(LOG_ERR, "no IPv6 interfaces found");
#endif

	if (isc_net_probeipv4() == ISC_R_SUCCESS)
		scan_ipv4 = ISC_TRUE;
#ifdef DEBUG
	else
		if(debug)
			netsyslog(LOG_ERR, "no IPv4 interfaces found");
#endif
	/*
	 * phase one - scan interfaces
	 * - create those that are not found
	 * - update those that are found
	 */

	result = isc_interfaceiter_create(mctx, &iter);

	if (result != ISC_R_SUCCESS)
		return 0;

	sys_interphase ^= 0x1;	/* toggle system phase for finding untouched (to be deleted) interfaces */
	
	for (result = isc_interfaceiter_first(iter);
	     result == ISC_R_SUCCESS;
	     result = isc_interfaceiter_next(iter))
	{
		isc_interface_t isc_if;
		unsigned int family;
		struct interface interface;
		struct interface *iface;
		
		result = isc_interfaceiter_current(iter, &isc_if);

		if (result != ISC_R_SUCCESS)
			break;

		/* See if we have a valid family to use */
		family = isc_if.address.family;
		if (family != AF_INET && family != AF_INET6)
			continue;
		if (scan_ipv4 == ISC_FALSE && family == AF_INET)
			continue;
		if (scan_ipv6 == ISC_FALSE && family == AF_INET6)
			continue;

		/*
		 * create prototype
		 */
		init_interface(&interface);

		convert_isc_if(&isc_if, &interface, port);

		/* 
		 * Check to see if we are going to use the interface
		 * If we don't use it we mark it to drop any packet
		 * received but we still must create the socket and
		 * bind to it. This prevents other apps binding to it
		 * and potentially causing problems with more than one
		 * process fiddling with the clock
		 */
		if (address_okay(&interface) == ISC_TRUE) {
			interface.ignore_packets = ISC_FALSE;
		}
		else {
			interface.ignore_packets = ISC_TRUE;
		}

		DPRINT_INTERFACE(4, (&interface, "examining ", "\n"));

		if (!(interface.flags & INT_UP))  { /* interfaces must be UP to be usable */
			DPRINTF(4, ("skipping interface %s (%s) - DOWN\n", interface.name, stoa(&interface.sin)));
			continue;
		}

		/*
		 * skip any interfaces UP and bound to a wildcard
		 * address - some dhcp clients produce that in the
		 * wild
		 */
		if (is_wildcard_addr(&interface.sin))
			continue;

#ifdef INCLUDE_IPV6_SUPPORT
		if (is_anycast((struct sockaddr *)&interface.sin, isc_if.name))
			continue;
#endif /* !INCLUDE_IPV6_SUPPORT */

		/*
		 * map to local *address* in order
		 * to map all duplicate interfaces to an interface structure
		 * with the appropriate socket (our name space is
		 * (ip-address) - NOT (interface name, ip-address))
		 */
		iface = getinterface(&interface.sin, INT_WILDCARD);
		
		if (iface && refresh_interface(iface)) 
		{
			/*
			 * found existing and up to date interface - mark present
			 */

			iface->phase = sys_interphase;
			DPRINT_INTERFACE(4, (iface, "updating ", " present\n"));
			ifi.action = IFS_EXISTS;
			ifi.interface = iface;
			if (receiver)
				receiver(data, &ifi);
		}
		else
		{
			/*
			 * this is new or refreshing failed - add to our interface list
			 * if refreshing failed we will delete the interface structure in
			 * phase 2 as the interface was not marked current. We can bind to
			 * the address as the refresh code already closed the offending socket
			 */
			
			iface = create_interface(port, &interface);

			if (iface)
			{
				ifi.action = IFS_CREATED;
				ifi.interface = iface;
				if (receiver)
					receiver(data, &ifi);

				new_interface_found = 1;

				DPRINT_INTERFACE(3, (iface, "updating ", " new - created\n"));
			}
			else
			{
				DPRINT_INTERFACE(3, (&interface, "updating ", " new - creation FAILED"));
			
				msyslog(LOG_INFO, "failed to initialize interface for address %s", stoa(&interface.sin));
				continue;
			}
		}
	}

	isc_interfaceiter_destroy(&iter);

	/*
	 * phase 2 - delete gone interfaces - reassigning peers to other interfaces
	 */
	{
		struct interface *interf = ISC_LIST_HEAD(inter_list);

		while (interf != NULL)
		{
			struct interface *next = ISC_LIST_NEXT(interf, link);
			  
			if (!(interf->flags & (INT_WILDCARD|INT_MCASTIF))) {
				/*
				 * if phase does not match sys_phase this interface was not
				 * enumerated during interface scan - so it is gone and
				 * will be deleted here unless it is solely an MCAST/WILDCARD interface
				 */
				if (interf->phase != sys_interphase) {
					struct peer *peer;
					DPRINT_INTERFACE(3, (interf, "updating ", "GONE - deleting\n"));
					remove_interface(interf);

					ifi.action = IFS_DELETED;
					ifi.interface = interf;
					if (receiver)
						receiver(data, &ifi);

					peer = ISC_LIST_HEAD(interf->peers);
					/*
					 * disconnect peer from deleted interface
					 */
					while (peer != NULL) {
						struct peer *npeer = ISC_LIST_NEXT(peer, ilink);
						
						/*
						 * this one just lost it's interface
						 */
						set_peerdstadr(peer, NULL);
	
						peer = npeer;
					}

					/*
					 * update globals in case we lose 
					 * a loopback interface
					 */
					if (interf == loopback_interface)
						loopback_interface = NULL;

					delete_interface(interf);
				}
			}
			interf = next;
		}
	}

	/*
	 * phase 3 - re-configure as the world has changed if necessary
	 */
	refresh_all_peerinterfaces();
	return new_interface_found;
}


/*
 * create_sockets - create a socket for each interface plus a default
 *			socket for when we don't know where to send
 */
static int
create_sockets(
	u_short port
	)
{
#ifndef HAVE_IO_COMPLETION_PORT
	/*
	 * I/O Completion Ports don't care about the select and FD_SET
	 */
	maxactivefd = 0;
	FD_ZERO(&activefds);
#endif

	DPRINTF(2, ("create_sockets(%d)\n", ntohs( (u_short) port)));

	create_wildcards(port);

	update_interfaces(port, NULL, NULL);
	
	/*
	 * Now that we have opened all the sockets, turn off the reuse
	 * flag for security.
	 */
	set_reuseaddr(0);

	DPRINTF(2, ("create_sockets: Total interfaces = %d\n", ninterfaces));

	return ninterfaces;
}

/*
 * create_interface - create a new interface for a given prototype
 *		      binding the socket.
 */
static struct interface *
create_interface(
		 u_short port,
		 struct interface *iface
		 )
{
	struct sockaddr_storage resmask;
	struct interface *interface;

	DPRINTF(2, ("create_interface(%s#%d)\n", stoa(&iface->sin), ntohs( (u_short) port)));

	/* build an interface */
	interface = new_interface(iface);
	
	/*
	 * create socket
	 */
	interface->fd = open_socket(&interface->sin,
				 interface->flags, 0, interface);

	if (interface->fd != INVALID_SOCKET)
		list_if_listening(interface, port);

	if ((interface->flags & INT_BROADCAST) &&
	    interface->bfd != INVALID_SOCKET)
	  msyslog(LOG_INFO, "Listening on broadcast address %s#%d",
		  stoa((&interface->bcast)),
		  ntohs( (u_short) port));

	if (interface->fd == INVALID_SOCKET &&
	    interface->bfd == INVALID_SOCKET) {
		msyslog(LOG_ERR, "unable to create socket on %s (%d) for %s#%d",
			interface->name,
			interface->ifnum,
			stoa((&interface->sin)),
			ntohs( (u_short) port));
		delete_interface(interface);
		return NULL;
	}
	
        /*
	 * Blacklist bound interface address
	 */
	
	SET_HOSTMASK(&resmask, interface->sin.ss_family);
	hack_restrict(RESTRICT_FLAGS, &interface->sin, &resmask,
		      RESM_NTPONLY|RESM_INTERFACE, RES_IGNORE);
	  
	/*
	 * set globals with the first found
	 * loopback interface of the appropriate class
	 */
	if ((loopback_interface == NULL) &&
	    (interface->family == AF_INET) &&
	    ((interface->flags & INT_LOOPBACK) != 0))
	{
		loopback_interface = interface;
	}

	/*
	 * put into our interface list
	 */
	add_addr_to_list(&interface->sin, interface);
	add_interface(interface);

	DPRINT_INTERFACE(2, (interface, "created ", "\n"));
	return interface;
}


#ifdef SO_EXCLUSIVEADDRUSE
static void
set_excladdruse(int fd)
{
	int one = 1;
	int failed;

	failed = setsockopt(fd, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
			    (char *)&one, sizeof(one));

	if (failed)
		netsyslog(LOG_ERR, 
			  "setsockopt(%d, SO_EXCLUSIVEADDRUSE, on): %m", fd);
}
#endif  /* SO_EXCLUSIVEADDRUSE */


/*
 * set_reuseaddr() - set/clear REUSEADDR on all sockets
 *			NB possible hole - should we be doing this on broadcast
 *			fd's also?
 */
static void
set_reuseaddr(int flag) {
	struct interface *interf;

#ifndef SO_EXCLUSIVEADDRUSE

	for (interf = ISC_LIST_HEAD(inter_list);
	     interf != NULL;
	     interf = ISC_LIST_NEXT(interf, link)) {

		if (interf->flags & INT_WILDCARD)
			continue;
	  
		/*
		 * if interf->fd  is INVALID_SOCKET, we might have a adapter
		 * configured but not present
		 */
		DPRINTF(4, ("setting SO_REUSEADDR on %.16s@%s to %s\n", interf->name, stoa(&interf->sin), flag ? "on" : "off"));
		
		if (interf->fd != INVALID_SOCKET) {
			if (setsockopt(interf->fd, SOL_SOCKET,
					SO_REUSEADDR, (char *)&flag,
					sizeof(flag))) {
				netsyslog(LOG_ERR, "set_reuseaddr: setsockopt(SO_REUSEADDR, %s) failed: %m", flag ? "on" : "off");
			}
		}
	}
#endif /* ! SO_EXCLUSIVEADDRUSE */
}

/*
 * This is just a wrapper around an internal function so we can
 * make other changes as necessary later on
 */
void
enable_broadcast(struct interface *iface, struct sockaddr_storage *baddr)
{
#ifdef SO_BROADCAST
	socket_broadcast_enable(iface, iface->fd, baddr);
#endif
}

#ifdef OPEN_BCAST_SOCKET 
/*
 * Enable a broadcast address to a given socket
 * The socket is in the inter_list all we need to do is enable
 * broadcasting. It is not this function's job to select the socket
 */
static isc_boolean_t
socket_broadcast_enable(struct interface *iface, SOCKET fd, struct sockaddr_storage *maddr)
{
#ifdef SO_BROADCAST
	int on = 1;

	if (maddr->ss_family == AF_INET)
	{
		/* if this interface can support broadcast, set SO_BROADCAST */
		if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST,
			       (char *)&on, sizeof(on)))
		{
			netsyslog(LOG_ERR, "setsockopt(SO_BROADCAST) enable failure on address %s: %m",
				stoa(maddr));
		}
#ifdef DEBUG
		else if (debug > 1) {
			printf("Broadcast enabled on socket %d for address %s\n",
				fd, stoa(maddr));
		}
#endif
	}
	iface->flags |= INT_BCASTOPEN;
	return ISC_TRUE;
#else
	return ISC_FALSE;
#endif /* SO_BROADCAST */
}

/*
 * Remove a broadcast address from a given socket
 * The socket is in the inter_list all we need to do is disable
 * broadcasting. It is not this function's job to select the socket
 */
static isc_boolean_t
socket_broadcast_disable(struct interface *iface, struct sockaddr_storage *maddr)
{
#ifdef SO_BROADCAST
	int off = 0;	/* This seems to be OK as an int */

	if (maddr->ss_family == AF_INET)
	{
		if (setsockopt(iface->fd, SOL_SOCKET, SO_BROADCAST,
			       (char *)&off, sizeof(off)))
		{
			netsyslog(LOG_ERR, "setsockopt(SO_BROADCAST) disable failure on address %s: %m",
				stoa(maddr));
		}
	}
	iface->flags &= ~INT_BCASTOPEN;
	return ISC_TRUE;
#else
	return ISC_FALSE;
#endif /* SO_BROADCAST */
}

#endif /* OPEN_BCAST_SOCKET */
/*
 * Check to see if the address is a multicast address
 */
static isc_boolean_t
addr_ismulticast(struct sockaddr_storage *maddr)
{
	switch (maddr->ss_family)
	{
	case AF_INET :
		if (!IN_CLASSD(ntohl(((struct sockaddr_in*)maddr)->sin_addr.s_addr))) {
			DPRINTF(4, ("multicast address %s not class D\n", stoa(maddr)));
			return (ISC_FALSE);
		}
		else
		{
			return (ISC_TRUE);
		}

	case AF_INET6 :
#ifdef INCLUDE_IPV6_MULTICAST_SUPPORT
		if (!IN6_IS_ADDR_MULTICAST(&((struct sockaddr_in6*)maddr)->sin6_addr)) {
			DPRINTF(4, ("address %s not IPv6 multicast address\n", stoa(maddr)));
			return (ISC_FALSE);
		}
		else
		{
			return (ISC_TRUE);
		}

/*
 * If we don't have IPV6 support any IPV6 address is not multicast
 */
#else
		return (ISC_FALSE);
#endif
	/*
	 * Never valid
	 */
	default:
		return (ISC_FALSE);
	}
}

/*
 * Multicast servers need to set the appropriate Multicast interface
 * socket option in order for it to know which interface to use for
 * send the multicast packet.
 */
void
enable_multicast_if(struct interface *iface, struct sockaddr_storage *maddr)
{
#ifdef MCAST
#ifdef IP_MULTICAST_LOOP
	/*u_char*/ TYPEOF_IP_MULTICAST_LOOP off = 0;
#endif
#ifdef IPV6_MULTICAST_LOOP
	u_int off6 = 0;		/* RFC 3493, 5.2. defines type unsigned int */
#endif

	switch (maddr->ss_family)
	{
	case AF_INET:
		if (setsockopt(iface->fd, IPPROTO_IP, IP_MULTICAST_IF,
		   (char *)&(((struct sockaddr_in*)&iface->sin)->sin_addr.s_addr),
		    sizeof(struct in_addr)) == -1) {
			netsyslog(LOG_ERR,
			"setsockopt IP_MULTICAST_IF failure: %m on socket %d, addr %s for multicast address %s",
			iface->fd, stoa(&iface->sin), stoa(maddr));
			return;
		}
#ifdef IP_MULTICAST_LOOP
		/*
		 * Don't send back to itself, but allow it to fail to set it
		 */
		if (setsockopt(iface->fd, IPPROTO_IP, IP_MULTICAST_LOOP,
		       SETSOCKOPT_ARG_CAST &off, sizeof(off)) == -1) {
			netsyslog(LOG_ERR,
			"setsockopt IP_MULTICAST_LOOP failure: %m on socket %d, addr %s for multicast address %s",
			iface->fd, stoa(&iface->sin), stoa(maddr));
		}
#endif
	DPRINTF(4, ("Added IPv4 multicast interface on socket %d, addr %s for multicast address %s\n",
			    iface->fd, stoa(&iface->sin),
			    stoa(maddr)));
		break;

	case AF_INET6:
#ifdef INCLUDE_IPV6_MULTICAST_SUPPORT
		if (setsockopt(iface->fd, IPPROTO_IPV6, IPV6_MULTICAST_IF,
		    (char *) &iface->scopeid, sizeof(iface->scopeid)) == -1) {
			netsyslog(LOG_ERR,
			"setsockopt IPV6_MULTICAST_IF failure: %m on socket %d, addr %s, scope %d for multicast address %s",
			iface->fd, stoa(&iface->sin), iface->scopeid,
			stoa(maddr));
			return;
		}
#ifdef IPV6_MULTICAST_LOOP
		/*
		 * Don't send back to itself, but allow it to fail to set it
		 */
		if (setsockopt(iface->fd, IPPROTO_IPV6, IPV6_MULTICAST_LOOP,
		       (char *) &off6, sizeof(off6)) == -1) {
			netsyslog(LOG_ERR,
			"setsockopt IPV6_MULTICAST_LOOP failure: %m on socket %d, addr %s for multicast address %s",
			iface->fd, stoa(&iface->sin), stoa(maddr));
		}
#endif
		DPRINTF(4, ("Added IPv6 multicast interface on socket %d, addr %s, scope %d for multicast address %s\n",
			    iface->fd,  stoa(&iface->sin), iface->scopeid,
			    stoa(maddr)));
		break;
#else
		return;
#endif	/* INCLUDE_IPV6_MULTICAST_SUPPORT */
	}
	return;
#endif
}

/*
 * Add a multicast address to a given socket
 * The socket is in the inter_list all we need to do is enable
 * multicasting. It is not this function's job to select the socket
 */
static isc_boolean_t
socket_multicast_enable(struct interface *iface, int lscope, struct sockaddr_storage *maddr)
{
#ifdef INCLUDE_IPV6_MULTICAST_SUPPORT
	struct ipv6_mreq mreq6;
	struct in6_addr iaddr6;
#endif /* INCLUDE_IPV6_MULTICAST_SUPPORT */

	struct ip_mreq mreq;

	if (find_addr_in_list(maddr)) {
		DPRINTF(4, ("socket_multicast_enable(%s): already enabled\n", stoa(maddr)));
		return ISC_TRUE;
	}

	switch (maddr->ss_family)
	{
	case AF_INET:
		memset((char *)&mreq, 0, sizeof(mreq));
		mreq.imr_multiaddr = (((struct sockaddr_in*)maddr)->sin_addr);
		mreq.imr_interface.s_addr = htonl(INADDR_ANY);
		if (setsockopt(iface->fd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
			(char *)&mreq, sizeof(mreq)) == -1) {
			netsyslog(LOG_ERR,
			"setsockopt IP_ADD_MEMBERSHIP failure: %m on socket %d, addr %s for %x / %x (%s)",
			iface->fd, stoa(&iface->sin),
			mreq.imr_multiaddr.s_addr,
			mreq.imr_interface.s_addr, stoa(maddr));
			return ISC_FALSE;
		}
		DPRINTF(4, ("Added IPv4 multicast membership on socket %d, addr %s for %x / %x (%s)\n",
			    iface->fd, stoa(&iface->sin),
			    mreq.imr_multiaddr.s_addr,
			    mreq.imr_interface.s_addr, stoa(maddr)));
		break;

	case AF_INET6:
#ifdef INCLUDE_IPV6_MULTICAST_SUPPORT
		/*
		 * Enable reception of multicast packets
		 * If the address is link-local we can get the interface index
		 * from the scope id. Don't do this for other types of multicast
		 * addresses. For now let the kernel figure it out.
		 */
		memset((char *)&mreq6, 0, sizeof(mreq6));
		iaddr6 = ((struct sockaddr_in6*)maddr)->sin6_addr;
		mreq6.ipv6mr_multiaddr = iaddr6;
		mreq6.ipv6mr_interface = lscope;

		if (setsockopt(iface->fd, IPPROTO_IPV6, IPV6_JOIN_GROUP,
			(char *)&mreq6, sizeof(mreq6)) == -1) {
			netsyslog(LOG_ERR,
			 "setsockopt IPV6_JOIN_GROUP failure: %m on socket %d, addr %s for interface %d(%s)",
			iface->fd, stoa(&iface->sin),
			mreq6.ipv6mr_interface, stoa(maddr));
			return ISC_FALSE;
		}
		DPRINTF(4, ("Added IPv6 multicast group on socket %d, addr %s for interface %d(%s)\n",
			    iface->fd, stoa(&iface->sin),
			    mreq6.ipv6mr_interface, stoa(maddr)));
		break;
#else
		return ISC_FALSE;
#endif	/* INCLUDE_IPV6_MULTICAST_SUPPORT */
	}
	iface->flags |= INT_MCASTOPEN;
	iface->num_mcast++;
	add_addr_to_list(maddr, iface);
	return ISC_TRUE;
}

/*
 * Remove a multicast address from a given socket
 * The socket is in the inter_list all we need to do is disable
 * multicasting. It is not this function's job to select the socket
 */
static isc_boolean_t
socket_multicast_disable(struct interface *iface, struct sockaddr_storage *maddr)
{
#ifdef INCLUDE_IPV6_MULTICAST_SUPPORT
	struct ipv6_mreq mreq6;
	struct in6_addr iaddr6;
#endif /* INCLUDE_IPV6_MULTICAST_SUPPORT */

	struct ip_mreq mreq;
	memset((char *)&mreq, 0, sizeof(mreq));

	if (find_addr_in_list(maddr) == NULL) {
		DPRINTF(4, ("socket_multicast_disable(%s): not enabled\n", stoa(maddr)));
		return ISC_TRUE;
	}

	switch (maddr->ss_family)
	{
	case AF_INET:
		mreq.imr_multiaddr = (((struct sockaddr_in*)&maddr)->sin_addr);
		mreq.imr_interface.s_addr = ((struct sockaddr_in*)&iface->sin)->sin_addr.s_addr;
		if (setsockopt(iface->fd, IPPROTO_IP, IP_DROP_MEMBERSHIP,
			(char *)&mreq, sizeof(mreq)) == -1) {
			netsyslog(LOG_ERR,
			"setsockopt IP_DROP_MEMBERSHIP failure: %m on socket %d, addr %s for %x / %x (%s)",
			iface->fd, stoa(&iface->sin),
			mreq.imr_multiaddr.s_addr,
			mreq.imr_interface.s_addr, stoa(maddr));
			return ISC_FALSE;
		}
		break;
	case AF_INET6:
#ifdef INCLUDE_IPV6_MULTICAST_SUPPORT
		/*
		 * Disable reception of multicast packets
		 * If the address is link-local we can get the interface index
		 * from the scope id. Don't do this for other types of multicast
		 * addresses. For now let the kernel figure it out.
		 */
		iaddr6 = ((struct sockaddr_in6*)&maddr)->sin6_addr;
		mreq6.ipv6mr_multiaddr = iaddr6;
		mreq6.ipv6mr_interface = iface->scopeid;

		if (setsockopt(iface->fd, IPPROTO_IPV6, IPV6_LEAVE_GROUP,
			(char *)&mreq6, sizeof(mreq6)) == -1) {
			netsyslog(LOG_ERR,
			"setsockopt IPV6_LEAVE_GROUP failure: %m on socket %d, addr %s for %d(%s)",
			iface->fd, stoa(&iface->sin),
			mreq6.ipv6mr_interface, stoa(maddr));
			return ISC_FALSE;
		}
		break;
#else
		return ISC_FALSE;
#endif	/* INCLUDE_IPV6_MULTICAST_SUPPORT */

	}
	iface->num_mcast--;
	if (iface->num_mcast <= 0) {
                iface->num_mcast = 0;
		iface->flags &= ~INT_MCASTOPEN;
	}
	return ISC_TRUE;
}

/*
 * io_setbclient - open the broadcast client sockets
 */
void
io_setbclient(void)
{
#ifdef OPEN_BCAST_SOCKET 
        struct interface *interf;
	int nif = 0;
	isc_boolean_t jstatus; 
	SOCKET fd;

	set_reuseaddr(1);

	for (interf = ISC_LIST_HEAD(inter_list);
	     interf != NULL;
	     interf = ISC_LIST_NEXT(interf, link)) {
	        if (interf->flags & INT_WILDCARD)
		        continue;
	  
		/* use only allowed addresses */
		if (interf->ignore_packets == ISC_TRUE)
			continue;
		/* Only IPv4 addresses are valid for broadcast */
		if (interf->sin.ss_family != AF_INET)
			continue;

		/* Is this a broadcast address? */
		if (!(interf->flags & INT_BROADCAST))
			continue;

		/* Skip the loopback addresses */
		if (interf->flags & INT_LOOPBACK)
			continue;

		/* Do we already have the broadcast address open? */
		if (interf->flags & INT_BCASTOPEN) {
		/* account for already open interfaces to aviod misleading warning below */
			nif++;
			continue;
		}

		/*
		 * Try to open the broadcast address
		 */
		interf->family = AF_INET;
		interf->bfd = open_socket(&interf->bcast,
				    INT_BROADCAST, 0, interf);

		 /*
		 * If we succeeded then we use it otherwise
		 * enable the underlying address
		 */
		if (interf->bfd == INVALID_SOCKET) {
			fd = interf->fd;
		}
		else {
			fd = interf->bfd;
		}

		/* Enable Broadcast on socket */
		jstatus = socket_broadcast_enable(interf, fd, &interf->sin);
		if (jstatus == ISC_TRUE)
		{
			nif++;
			netsyslog(LOG_INFO,"io_setbclient: Opened broadcast client on interface #%d %s, socket: %d",
				  interf->ifnum, interf->name, fd);
			interf->addr_refid = addr2refid(&interf->sin);
		}
	}
	set_reuseaddr(0);
#ifdef DEBUG
	if (debug)
		if (nif > 0)
			printf("io_setbclient: Opened broadcast clients\n");
#endif
	if (nif == 0)
		netsyslog(LOG_ERR, "Unable to listen for broadcasts, no broadcast interfaces available");
#else
	netsyslog(LOG_ERR, "io_setbclient: Broadcast Client disabled by build");
#endif
}

/*
 * io_unsetbclient - close the broadcast client sockets
 */
void
io_unsetbclient(void)
{
        struct interface *interf;
	isc_boolean_t lstatus;

	for (interf = ISC_LIST_HEAD(inter_list);
	     interf != NULL;
	     interf = ISC_LIST_NEXT(interf, link))
	{
	        if (interf->flags & INT_WILDCARD)
		    continue;
	  
		if (!(interf->flags & INT_BCASTOPEN))
		    continue;
		lstatus = socket_broadcast_disable(interf, &interf->sin);
	}
}

/*
 * io_multicast_add() - add multicast group address
 */
void
io_multicast_add(
	struct sockaddr_storage addr
	)
{
#ifdef MCAST
	struct interface *interface;
#ifndef MULTICAST_NONEWSOCKET
	struct interface *iface;
#endif
	int lscope = 0;
	
	/*
	 * Check to see if this is a multicast address
	 */
	if (addr_ismulticast(&addr) == ISC_FALSE)
		return;

	/* If we already have it we can just return */
	if (find_flagged_addr_in_list(&addr, INT_MCASTOPEN|INT_MCASTIF) != NULL)
	{
		netsyslog(LOG_INFO, "Duplicate request found for multicast address %s",
			stoa(&addr));
		return;
	}

#ifndef MULTICAST_NONEWSOCKET
	interface = new_interface(NULL);
	
	/*
	 * Open a new socket for the multicast address
	 */
	interface->sin.ss_family = addr.ss_family;
	interface->family = addr.ss_family;

	switch(addr.ss_family) {
	case AF_INET:
		memcpy(&(((struct sockaddr_in *)&interface->sin)->sin_addr),
		       &(((struct sockaddr_in*)&addr)->sin_addr),
		       sizeof(struct in_addr));
		((struct sockaddr_in*)&interface->sin)->sin_port = htons(NTP_PORT);
		memset(&((struct sockaddr_in*)&interface->mask)->sin_addr.s_addr, 0xff, sizeof(struct in_addr));
		break;
	case AF_INET6:
#ifdef INCLUDE_IPV6_MULTICAST_SUPPORT
		memcpy(&(((struct sockaddr_in6 *)&interface->sin)->sin6_addr),
		       &((struct sockaddr_in6*)&addr)->sin6_addr,
		       sizeof(struct in6_addr));
		((struct sockaddr_in6*)&interface->sin)->sin6_port = htons(NTP_PORT);
#ifdef ISC_PLATFORM_HAVESCOPEID
		((struct sockaddr_in6*)&interface->sin)->sin6_scope_id = ((struct sockaddr_in6*)&addr)->sin6_scope_id;
#endif
		memset(&((struct sockaddr_in6*)&interface->mask)->sin6_addr.s6_addr, 0xff, sizeof(struct in6_addr));
#endif
		iface = findlocalcastinterface(&addr, INT_MULTICAST);
		if (iface) {
# ifdef ISC_PLATFORM_HAVESCOPEID
			lscope = ((struct sockaddr_in6*)&iface->sin)->sin6_scope_id;
# endif
			DPRINTF(4, ("Found interface #%d %s, scope: %d for address %s\n", iface->ifnum, iface->name, lscope, stoa(&addr)));
		}
		break;
	}
		
	set_reuseaddr(1);
	interface->bfd = INVALID_SOCKET;
	interface->fd = open_socket(&interface->sin,
			    INT_MULTICAST, 0, interface);

	if (interface->fd != INVALID_SOCKET)
	{
		interface->bfd = INVALID_SOCKET;
		interface->ignore_packets = ISC_FALSE;
		interface->flags |= INT_MCASTIF;
		
		(void) strncpy(interface->name, "multicast",
			sizeof(interface->name));
		((struct sockaddr_in*)&interface->mask)->sin_addr.s_addr =
						htonl(~(u_int32)0);
		DPRINT_INTERFACE(2, (interface, "multicast add ", "\n"));
		/* socket_multicast_enable() will add this address to the addresslist */
		add_interface(interface);
		list_if_listening(interface, htons(NTP_PORT));
	}
	else
	{
		delete_interface(interface);  /* re-use existing interface */
		interface = NULL;
		if (addr.ss_family == AF_INET)
			interface = wildipv4;
		else if (addr.ss_family == AF_INET6)
			interface = wildipv6;

		if (interface != NULL) {
			/* HACK ! -- stuff in an address */
			interface->bcast = addr;
			netsyslog(LOG_ERR,
			 "...multicast address %s using wildcard interface #%d %s",
				  stoa(&addr), interface->ifnum, interface->name);
		} else {
			netsyslog(LOG_ERR,
			"No multicast socket available to use for address %s",
			stoa(&addr));
			return;
		}
	}
#else
	/*
	 * For the case where we can't use a separate socket
	 */
	interface = findlocalcastinterface(&addr, INT_MULTICAST);
	/*
	 * If we don't have a valid socket, just return
	 */
	if (!interface)
	{
		netsyslog(LOG_ERR,
		"Cannot add multicast address %s: Cannot find slot",
		stoa(&addr));
		return;
	}

#endif
	{
		isc_boolean_t jstatus;
		jstatus = socket_multicast_enable(interface, lscope, &addr);
	
		if (jstatus == ISC_TRUE)
			netsyslog(LOG_INFO, "Added Multicast Listener %s on interface #%d %s\n", stoa(&addr), interface->ifnum, interface->name);
		else
			netsyslog(LOG_ERR, "Failed to add Multicast Listener %s\n", stoa(&addr));
	}
#else /* MCAST */
	netsyslog(LOG_ERR,
		  "Cannot add multicast address %s: no Multicast support",
		  stoa(&addr));
#endif /* MCAST */
	return;
}

/*
 * io_multicast_del() - delete multicast group address
 */
void
io_multicast_del(
	struct sockaddr_storage addr
	)
{
#ifdef MCAST
        struct interface *interface;
	isc_boolean_t lstatus;

	/*
	 * Check to see if this is a multicast address
	 */
	if (addr_ismulticast(&addr) == ISC_FALSE)
	{
		netsyslog(LOG_ERR,
			 "invalid multicast address %s", stoa(&addr));
		return;
	}

	switch (addr.ss_family)
	{
	case AF_INET :
		/*
		 * Disable reception of multicast packets
		 */
		interface = find_flagged_addr_in_list(&addr, INT_MCASTOPEN);
		while ( interface != NULL) {
			lstatus = socket_multicast_disable(interface, &addr);
			interface = find_flagged_addr_in_list(&addr, INT_MCASTOPEN);
		}
		break;

#ifdef INCLUDE_IPV6_MULTICAST_SUPPORT
	case AF_INET6 :
		/*
		 * Disable reception of multicast packets
		 */
		for (interface = ISC_LIST_HEAD(inter_list);
		     interface != NULL;
		     interface = ISC_LIST_NEXT(interface, link))
		{
                        if (interface->flags & INT_WILDCARD)
			        continue;
	  
			/* Be sure it's the correct family */
			if (interface->sin.ss_family != AF_INET6)
				continue;
			if (!(interface->flags & INT_MCASTOPEN))
				continue;
			if (!(interface->fd < 0))
				continue;
			if (!SOCKCMP(&addr, &interface->sin))
				continue;
			lstatus = socket_multicast_disable(interface, &addr);
		}
		break;
#endif /* INCLUDE_IPV6_MULTICAST_SUPPORT */

	}/* switch */

        delete_addr_from_list(&addr);

#else /* not MCAST */
	netsyslog(LOG_ERR, "this function requires multicast kernel");
#endif /* not MCAST */
}

/*
 * init_nonblocking_io() - set up descriptor to be non blocking
 */
static void init_nonblocking_io(SOCKET fd)
{
	/*
	 * set non-blocking,
	 */

#ifdef USE_FIONBIO
	/* in vxWorks we use FIONBIO, but the others are defined for old systems, so
	 * all hell breaks loose if we leave them defined
	 */
#undef O_NONBLOCK
#undef FNDELAY
#undef O_NDELAY
#endif

#if defined(O_NONBLOCK) /* POSIX */
	if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
	{
		netsyslog(LOG_ERR, "fcntl(O_NONBLOCK) fails on fd #%d: %m",
			fd);
		exit(1);
		/*NOTREACHED*/
	}
#elif defined(FNDELAY)
	if (fcntl(fd, F_SETFL, FNDELAY) < 0)
	{
		netsyslog(LOG_ERR, "fcntl(FNDELAY) fails on fd #%d: %m",
			fd);
		exit(1);
		/*NOTREACHED*/
	}
#elif defined(O_NDELAY) /* generally the same as FNDELAY */
	if (fcntl(fd, F_SETFL, O_NDELAY) < 0)
	{
		netsyslog(LOG_ERR, "fcntl(O_NDELAY) fails on fd #%d: %m",
			fd);
		exit(1);
		/*NOTREACHED*/
	}
#elif defined(FIONBIO)
	{
		int on = 1;
		if (ioctl(fd,FIONBIO,&on) < 0)
		{
			netsyslog(LOG_ERR, "ioctl(FIONBIO) fails on fd #%d: %m",
				fd);
			exit(1);
			/*NOTREACHED*/
		}
	}
#elif defined(FIOSNBIO)
	if (ioctl(fd,FIOSNBIO,&on) < 0)
	{
		netsyslog(LOG_ERR, "ioctl(FIOSNBIO) fails on fd #%d: %m",
			fd);
		exit(1);
		/*NOTREACHED*/
	}
#else
# include "Bletch: Need non-blocking I/O!"
#endif
}

/*
 * open_socket - open a socket, returning the file descriptor
 */

static SOCKET
open_socket(
	struct sockaddr_storage *addr,
	int flags,
	int turn_off_reuse,
	struct interface *interf
	)
{
	int errval;
	SOCKET fd;
	/*
	 * int is OK for REUSEADR per 
	 * http://www.kohala.com/start/mcast.api.txt
	 */
	int on = 1;
	int off = 0;

#if defined(IPTOS_LOWDELAY) && defined(IPPROTO_IP) && defined(IP_TOS)
	int tos;
#endif /* IPTOS_LOWDELAY && IPPROTO_IP && IP_TOS */

	if ((addr->ss_family == AF_INET6) && (isc_net_probeipv6() != ISC_R_SUCCESS))
		return (INVALID_SOCKET);

	/* create a datagram (UDP) socket */
	fd = socket(addr->ss_family, SOCK_DGRAM, 0);
	if (INVALID_SOCKET == fd) {
#ifndef SYS_WINNT
		errval = errno;
#else
		errval = WSAGetLastError();
#endif
		netsyslog(LOG_ERR, 
			  "socket(AF_INET%s, SOCK_DGRAM, 0) failed on address %s: %m",
			  (addr->ss_family == AF_INET6) ? "6" : "",
			  stoa(addr));

		if (errval == EPROTONOSUPPORT || 
		    errval == EAFNOSUPPORT ||
		    errval == EPFNOSUPPORT)
			return (INVALID_SOCKET);
		msyslog(LOG_ERR, "unexpected error code %d (not PROTONOSUPPORT|AFNOSUPPORT|FPNOSUPPORT) - exiting", errval);
		exit(1);
		/*NOTREACHED*/
	}

#ifdef SYS_WINNT
	connection_reset_fix(fd, addr);
#endif
	/*
	 * Fixup the file descriptor for some systems
	 * See bug #530 for details of the issue.
	 */
	fd = move_fd(fd);

	/*
	 * set SO_REUSEADDR since we will be binding the same port
	 * number on each interface according to turn_off_reuse.
	 * This is undesirable on Windows versions starting with
	 * Windows XP (numeric version 5.1).
	 */
#ifdef SYS_WINNT
	if (isc_win32os_versioncheck(5, 1, 0, 0) < 0)  /* before 5.1 */
#endif
		if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
			       (char *)(turn_off_reuse 
					? &off 
					: &on), 
			       sizeof(on))) {

			netsyslog(LOG_ERR, "setsockopt SO_REUSEADDR %s"
					   " fails for address %s: %m",
					   turn_off_reuse 
						? "off" 
						: "on", 
					   stoa(addr));
			closesocket(fd);
			return INVALID_SOCKET;
		}
#ifdef SO_EXCLUSIVEADDRUSE
	/*
	 * setting SO_EXCLUSIVEADDRUSE on the wildcard we open
	 * first will cause more specific binds to fail.
	 */
	if (!(interf->flags & INT_WILDCARD))
		set_excladdruse(fd);
#endif

	/*
	 * IPv4 specific options go here
	 */
	if (addr->ss_family == AF_INET) {
#if defined(IPTOS_LOWDELAY) && defined(IPPROTO_IP) && defined(IP_TOS)
	/* set IP_TOS to minimize packet delay */
		tos = IPTOS_LOWDELAY;
		if (setsockopt(fd, IPPROTO_IP, IP_TOS, (char *) &tos, sizeof(tos)) < 0)
		{
			netsyslog(LOG_ERR, "setsockopt IPTOS_LOWDELAY on fails on address %s: %m",
				stoa(addr));
		}
#endif /* IPTOS_LOWDELAY && IPPROTO_IP && IP_TOS */
	}

	/*
	 * IPv6 specific options go here
	 */
        if (addr->ss_family == AF_INET6) {
#if defined(IPV6_V6ONLY)
                if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY,
                	(char*)&on, sizeof(on)))
                {
                	netsyslog(LOG_ERR, "setsockopt IPV6_V6ONLY on fails on address %s: %m",
				stoa(addr));
		}
#endif /* IPV6_V6ONLY */
#if defined(IPV6_BINDV6ONLY)
                if (setsockopt(fd, IPPROTO_IPV6, IPV6_BINDV6ONLY,
                	(char*)&on, sizeof(on)))
                {
                	netsyslog(LOG_ERR,
			    "setsockopt IPV6_BINDV6ONLY on fails on address %s: %m",
			    stoa(addr));
		}
#endif /* IPV6_BINDV6ONLY */
	}

#ifdef OS_NEEDS_REUSEADDR_FOR_IFADDRBIND
	/*
	 * some OSes don't allow binding to more specific
	 * addresses if a wildcard address already bound
	 * to the port and SO_REUSEADDR is not set
	 */
	if (!is_wildcard_addr(addr)) {
		set_wildcard_reuse(addr->ss_family, 1);
	}
#endif

	/*
	 * bind the local address.
	 */
	errval = bind(fd, (struct sockaddr *)addr, SOCKLEN(addr));

#ifdef OS_NEEDS_REUSEADDR_FOR_IFADDRBIND
	/*
	 * some OSes don't allow binding to more specific
	 * addresses if a wildcard address already bound
	 * to the port and REUSE_ADDR is not set
	 */
	if (!is_wildcard_addr(addr)) {
		set_wildcard_reuse(addr->ss_family, 0);
	}
#endif

	if (errval < 0) {
		/*
		 * Don't log this under all conditions
		 */
		if (turn_off_reuse == 0
#ifdef DEBUG
		    || debug > 1
#endif
			) {
			if (addr->ss_family == AF_INET)
				netsyslog(LOG_ERR,
					  "bind() fd %d, family AF_INET, port %d, addr %s, in_classd=%d flags=0x%x fails: %m",
					  fd, (int)ntohs(((struct sockaddr_in*)addr)->sin_port),
					  stoa(addr),
					  IN_CLASSD(ntohl(((struct sockaddr_in*)addr)->sin_addr.s_addr)), 
					  flags);
#ifdef INCLUDE_IPV6_SUPPORT
			else if (addr->ss_family == AF_INET6)
				netsyslog(LOG_ERR,
					  "bind() fd %d, family AF_INET6, port %d, scope %d, addr %s, mcast=%d flags=0x%x fails: %m",
					  fd, (int)ntohs(((struct sockaddr_in6*)addr)->sin6_port),
# ifdef ISC_PLATFORM_HAVESCOPEID
					  ((struct sockaddr_in6*)addr)->sin6_scope_id
# else
					  -1
# endif
					  , stoa(addr),
					  IN6_IS_ADDR_MULTICAST(&((struct sockaddr_in6*)addr)->sin6_addr), 
					  flags);
#endif
		}

		closesocket(fd);
		
		return INVALID_SOCKET;
	}

#ifdef HAVE_TIMESTAMP
	{
		if (setsockopt(fd, SOL_SOCKET, SO_TIMESTAMP,
			       (char*)&on, sizeof(on)))
		{
			netsyslog(LOG_DEBUG,
				  "setsockopt SO_TIMESTAMP on fails on address %s: %m",
				  stoa(addr));
		}
#ifdef DEBUG
		else
		{
			DPRINTF(4, ("setsockopt SO_TIMESTAMP enabled on fd %d address %s\n", fd, stoa(addr)));
		}
#endif
	}	
#endif
	DPRINTF(4, ("bind() fd %d, family %d, port %d, addr %s, flags=0x%x\n",
		   fd,
		   addr->ss_family,
		   (int)ntohs(((struct sockaddr_in*)addr)->sin_port),
		   stoa(addr),
		   interf->flags));

	init_nonblocking_io(fd);
	
#ifdef HAVE_SIGNALED_IO
	init_socket_sig(fd);
#endif /* not HAVE_SIGNALED_IO */

	add_fd_to_list(fd, FD_TYPE_SOCKET);

#if !defined(SYS_WINNT) && !defined(VMS)
	DPRINTF(4, ("flags for fd %d: 0x%x\n", fd,
		    fcntl(fd, F_GETFL, 0)));
#endif /* SYS_WINNT || VMS */

#if defined (HAVE_IO_COMPLETION_PORT)
/*
 * Add the socket to the completion port
 */
	if (io_completion_port_add_socket(fd, interf))
	{
		msyslog(LOG_ERR, "unable to set up io completion port - EXITING");
		exit(1);
	}
#endif
	return fd;
}

/* XXX ELIMINATE sendpkt similar in ntpq.c, ntpdc.c, ntp_io.c, ntptrace.c */
/*
 * sendpkt - send a packet to the specified destination. Maintain a
 * send error cache so that only the first consecutive error for a
 * destination is logged.
 */
void
sendpkt(
	struct sockaddr_storage *dest,
	struct interface *inter,
	int ttl,
	struct pkt *pkt,
	int len
	)
{
	int cc, slot;

	/*
	 * Send error caches. Empty slots have port == 0
	 * Set ERRORCACHESIZE to 0 to disable
	 */
	struct cache {
		u_short port;
		struct	in_addr addr;
	};

#ifdef INCLUDE_IPV6_SUPPORT
	struct cache6 {
		u_short port;
		struct in6_addr addr;
	};
#endif /* INCLUDE_IPV6_SUPPORT */


#ifndef ERRORCACHESIZE
#define ERRORCACHESIZE 8
#endif
#if ERRORCACHESIZE > 0
	static struct cache badaddrs[ERRORCACHESIZE];
#ifdef INCLUDE_IPV6_SUPPORT
	static struct cache6 badaddrs6[ERRORCACHESIZE];
#endif /* INCLUDE_IPV6_SUPPORT */
#else
#define badaddrs ((struct cache *)0)		/* Only used in empty loops! */
#ifdef INCLUDE_IPV6_SUPPORT
#define badaddrs6 ((struct cache6 *)0)		/* Only used in empty loops! */
#endif /* INCLUDE_IPV6_SUPPORT */
#endif
#ifdef DEBUG
	if (debug > 1)
	  {
	    if (inter != NULL) 
	      {
		printf("%ssendpkt(fd=%d dst=%s, src=%s, ttl=%d, len=%d)\n",
		       (ttl > 0) ? "\tMCAST\t***** " : "",
		       inter->fd, stoa(dest),
		       stoa(&inter->sin), ttl, len);
	      }
	    else
	      {
		printf("%ssendpkt(dst=%s, ttl=%d, len=%d): no interface - IGNORED\n",
		       (ttl > 0) ? "\tMCAST\t***** " : "",
		       stoa(dest),
		       ttl, len);
	      }
	  }
#endif

	if (inter == NULL)	/* unbound peer - drop request and wait for better network conditions */
	  return;
	
#ifdef MCAST

	/*
	 * for the moment we use the bcast option to set multicast ttl
	 */
	if (ttl > 0 && ttl != inter->last_ttl) {
		
		/*
		 * set the multicast ttl for outgoing packets
		 */
		int rtc;
		
		switch (inter->sin.ss_family) {
			
		case AF_INET :
		{
			u_char mttl = (u_char) ttl;

			rtc = setsockopt(inter->fd, IPPROTO_IP, IP_MULTICAST_TTL,
					 (const void *) &mttl, sizeof(mttl));
			break;
		}
			
#ifdef INCLUDE_IPV6_SUPPORT
		case AF_INET6 :
		{
			u_int ittl = (u_char) ttl;

			rtc = setsockopt(inter->fd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS,
					 (const void *) &ittl, sizeof(ittl));
			break;
		}

#endif /* INCLUDE_IPV6_SUPPORT */
		default:	/* just NOP if not supported */
			rtc = 0;
			break;
		}
		
		if (rtc != 0) {
			netsyslog(LOG_ERR, "setsockopt IP_MULTICAST_TTL/IPV6_MULTICAST_HOPS fails on address %s: %m",
				  stoa(&inter->sin));
		}
		else
			inter->last_ttl = ttl;
	}

#endif /* MCAST */

	for (slot = ERRORCACHESIZE; --slot >= 0; )
		if(dest->ss_family == AF_INET) {
			if (badaddrs[slot].port == SRCPORT(dest) &&
				badaddrs[slot].addr.s_addr == ((struct sockaddr_in*)dest)->sin_addr.s_addr)
			break;
		}
#ifdef INCLUDE_IPV6_SUPPORT
		else if (dest->ss_family == AF_INET6) {
			if (badaddrs6[slot].port == SRCPORT(dest) &&
				!memcmp(&badaddrs6[slot].addr, &((struct sockaddr_in6*)dest)->sin6_addr, sizeof(struct in6_addr)))
			break;
		}
#endif /* INCLUDE_IPV6_SUPPORT */

#if defined(HAVE_IO_COMPLETION_PORT)
        cc = io_completion_port_sendto(inter, pkt, len, dest);
	if (cc != ERROR_SUCCESS)
#else
#ifdef SIM
        cc = srvr_rply(&ntp_node,  dest, inter, pkt);
#else /* SIM */
	cc = sendto(inter->fd, (char *)pkt, (unsigned int)len, 0, (struct sockaddr *)dest,
		    SOCKLEN(dest));
#endif /* SIM */
	if (cc == -1)
#endif
	{
		inter->notsent++;
		packets_notsent++;

#if defined(HAVE_IO_COMPLETION_PORT)
		if (cc != WSAEWOULDBLOCK && cc != WSAENOBUFS && slot < 0)
#else
		if (errno != EWOULDBLOCK && errno != ENOBUFS && slot < 0)
#endif
		{
			/*
			 * Remember this, if there's an empty slot
			 */
			switch (dest->ss_family) {

			case AF_INET :

				for (slot = ERRORCACHESIZE; --slot >= 0; )
					if (badaddrs[slot].port == 0)
					{
						badaddrs[slot].port = SRCPORT(dest);
						badaddrs[slot].addr = ((struct sockaddr_in*)dest)->sin_addr;
						break;
					}
				break;

#ifdef INCLUDE_IPV6_SUPPORT
			case AF_INET6 :

				for (slot = ERRORCACHESIZE; --slot >= 0; )
					if (badaddrs6[slot].port == 0)
					{
						badaddrs6[slot].port = SRCPORT(dest);
						badaddrs6[slot].addr = ((struct sockaddr_in6*)dest)->sin6_addr;
						break;
					}
				break;
#endif /* INCLUDE_IPV6_SUPPORT */
			default:  /* don't care if not supported */
				break;
			}

			netsyslog(LOG_ERR, "sendto(%s) (fd=%d): %m",
				  stoa(dest), inter->fd);
		}
	}
	else
	{
		inter->sent++;
		packets_sent++;
		/*
		 * He's not bad any more
		 */
		if (slot >= 0)
		{
			netsyslog(LOG_INFO, "Connection re-established to %s", stoa(dest));
			switch (dest->ss_family) {
			case AF_INET :
				badaddrs[slot].port = 0;
				break;
#ifdef INCLUDE_IPV6_SUPPORT
			case AF_INET6 :
				badaddrs6[slot].port = 0;
				break;
#endif /* INCLUDE_IPV6_SUPPORT */
			default:  /* don't care if not supported */
				break;
			}
		}
	}
}

#if !defined(HAVE_IO_COMPLETION_PORT)
/*
 * fdbits - generate ascii representation of fd_set (FAU debug support)
 * HFDF format - highest fd first.
 */
static char *
fdbits(
	int count,
	fd_set *set
	)
{
	static char buffer[256];
	char * buf = buffer;

	count = (count < 256) ? count : 255;

	while (count >= 0)
	{
		*buf++ = FD_ISSET(count, set) ? '#' : '-';
		count--;
	}
	*buf = '\0';

	return buffer;
}

/*
 * Routine to read the refclock packets for a specific interface
 * Return the number of bytes read. That way we know if we should
 * read it again or go on to the next one if no bytes returned
 */
static inline int
read_refclock_packet(SOCKET fd, struct refclockio *rp, l_fp ts)
{
	int i;
	int buflen;
	register struct recvbuf *rb;

	rb = get_free_recv_buffer();

	if (rb == NULL)
	{
		/*
		 * No buffer space available - just drop the packet
		 */
		char buf[RX_BUFF_SIZE];

		buflen = read(fd, buf, sizeof buf);
		packets_dropped++;
		return (buflen);
	}

	i = (rp->datalen == 0
	    || rp->datalen > sizeof(rb->recv_space))
	    ? sizeof(rb->recv_space) : rp->datalen;
	buflen = read(fd, (char *)&rb->recv_space, (unsigned)i);

	if (buflen < 0)
	{
		if (errno != EINTR && errno != EAGAIN) {
			netsyslog(LOG_ERR, "clock read fd %d: %m", fd);
		}
		freerecvbuf(rb);
		return (buflen);
	}

	/*
	 * Got one. Mark how and when it got here,
	 * put it on the full list and do bookkeeping.
	 */
	rb->recv_length = buflen;
	rb->recv_srcclock = rp->srcclock;
	rb->dstadr = 0;
	rb->fd = fd;
	rb->recv_time = ts;
	rb->receiver = rp->clock_recv;

	if (rp->io_input)
	{
		/*
		 * have direct input routine for refclocks
		 */
		if (rp->io_input(rb) == 0)
		{
			/*
			 * data was consumed - nothing to pass up
			 * into block input machine
			 */
			freerecvbuf(rb);
			return (buflen);
		}
	}
	
	add_full_recv_buffer(rb);

	rp->recvcount++;
	packets_received++;
	return (buflen);
}

#ifdef HAVE_TIMESTAMP
/*
 * extract timestamps from control message buffer
 */
static l_fp
	fetch_timestamp(struct recvbuf *rb, struct msghdr *msghdr, l_fp ts)
{
#ifdef USE_TIMESTAMP_CMSG
	struct cmsghdr *cmsghdr;

	cmsghdr = CMSG_FIRSTHDR(msghdr);
	while (cmsghdr != NULL) {
		switch (cmsghdr->cmsg_type)
		{
		case SCM_TIMESTAMP:
		{
			struct timeval *tvp = (struct timeval *)CMSG_DATA(cmsghdr);
			double dtemp;
			l_fp nts;
			DPRINTF(4, ("fetch_timestamp: system network time stamp: %ld.%06ld\n", tvp->tv_sec, tvp->tv_usec));
			nts.l_i = tvp->tv_sec + JAN_1970;
			dtemp = tvp->tv_usec / 1e6;

 			/* fuzz lower bits not covered by precision */
 			if (sys_precision != 0)
 				dtemp += (ntp_random() / FRAC - .5) / (1 <<
 								       -sys_precision);

			nts.l_uf = (u_int32)(dtemp*FRAC);
#ifdef DEBUG_TIMING
			{
				l_fp dts = ts;
				L_SUB(&dts, &nts);
				collect_timing(rb, "input processing delay", 1, &dts);
				DPRINTF(4, ("fetch_timestamp: timestamp delta: %s (incl. prec fuzz)\n", lfptoa(&dts, 9)));
			}
#endif
			ts = nts;  /* network time stamp */
			break;
		}
		default:
			DPRINTF(4, ("fetch_timestamp: skipping control message 0x%x\n", cmsghdr->cmsg_type));
			break;
		}
		cmsghdr = CMSG_NXTHDR(msghdr, cmsghdr);
	}
#endif
	return ts;
}
#endif

/*
 * Routine to read the network NTP packets for a specific interface
 * Return the number of bytes read. That way we know if we should
 * read it again or go on to the next one if no bytes returned
 */
static inline int
read_network_packet(SOCKET fd, struct interface *itf, l_fp ts)
{
	GETSOCKNAME_SOCKLEN_TYPE fromlen;
	int buflen;
	register struct recvbuf *rb;
#ifdef HAVE_TIMESTAMP
	struct msghdr msghdr;
	struct iovec iovec;
	char control[TIMESTAMP_CTLMSGBUF_SIZE];	/* pick up control messages */
#endif

	/*
	 * Get a buffer and read the frame.  If we
	 * haven't got a buffer, or this is received
	 * on a disallowed socket, just dump the
	 * packet.
	 */

	rb = get_free_recv_buffer();

	if (rb == NULL || itf->ignore_packets == ISC_TRUE)
	{
		char buf[RX_BUFF_SIZE];
		struct sockaddr_storage from;
		if (rb != NULL)
			freerecvbuf(rb);

		fromlen = sizeof(from);
		buflen = recvfrom(fd, buf, sizeof(buf), 0,
				(struct sockaddr*)&from, &fromlen);
		DPRINTF(4, ("%s on (%lu) fd=%d from %s\n",
			(itf->ignore_packets == ISC_TRUE) ? "ignore" : "drop",
			free_recvbuffs(), fd,
			stoa(&from)));
		if (itf->ignore_packets == ISC_TRUE)
			packets_ignored++;
		else
			packets_dropped++;
		return (buflen);
	}

	fromlen = sizeof(struct sockaddr_storage);

#ifndef HAVE_TIMESTAMP
	rb->recv_length = recvfrom(fd,
			  (char *)&rb->recv_space,
			   sizeof(rb->recv_space), 0,
			   (struct sockaddr *)&rb->recv_srcadr,
			   &fromlen);
#else
	iovec.iov_base        = (void *)&rb->recv_space;
	iovec.iov_len         = sizeof(rb->recv_space);
	msghdr.msg_name       = (void *)&rb->recv_srcadr;
	msghdr.msg_namelen    = sizeof(rb->recv_srcadr);
	msghdr.msg_iov        = &iovec;
	msghdr.msg_iovlen     = 1;
	msghdr.msg_control    = (void *)&control;
	msghdr.msg_controllen = sizeof(control);
	msghdr.msg_flags      = 0;
	rb->recv_length       = recvmsg(fd, &msghdr, 0);
#endif

	buflen = rb->recv_length;

	if (buflen == 0 || (buflen == -1 && 
	    (errno==EWOULDBLOCK
#ifdef EAGAIN
	   || errno==EAGAIN
#endif
	 ))) {
		freerecvbuf(rb);
		return (buflen);
	}
	else if (buflen < 0)
	{
		netsyslog(LOG_ERR, "recvfrom(%s) fd=%d: %m",
		stoa(&rb->recv_srcadr), fd);
		DPRINTF(5, ("read_network_packet: fd=%d dropped (bad recvfrom)\n", fd));
		freerecvbuf(rb);
		return (buflen);
	}

#ifdef DEBUG
	if (debug > 2) {
		if(rb->recv_srcadr.ss_family == AF_INET)
			printf("read_network_packet: fd=%d length %d from %08lx %s\n",
				fd, buflen,
				(u_long)ntohl(((struct sockaddr_in*)&rb->recv_srcadr)->sin_addr.s_addr) &
				0x00000000ffffffff,
				stoa(&rb->recv_srcadr));
		else
			printf("read_network_packet: fd=%d length %d from %s\n",
				fd, buflen,
				stoa(&rb->recv_srcadr));
	}
#endif

	/*
	 * Got one.  Mark how and when it got here,
	 * put it on the full list and do bookkeeping.
	 */
	rb->dstadr = itf;
	rb->fd = fd;
#ifdef HAVE_TIMESTAMP
	ts = fetch_timestamp(rb, &msghdr, ts);  /* pick up a network time stamp if possible */
#endif
	rb->recv_time = ts;
	rb->receiver = receive;

	add_full_recv_buffer(rb);

	itf->received++;
	packets_received++;
	return (buflen);
}

/*
 * input_handler - receive packets asynchronously
 */
void
input_handler(
	l_fp *cts
	)
{

	int buflen;
	int n;
	int doing;
	SOCKET fd;
	struct timeval tvzero;
	l_fp ts;			/* Timestamp at BOselect() gob */
#ifdef DEBUG_TIMING
	l_fp ts_e;			/* Timestamp at EOselect() gob */
#endif
	fd_set fds;
	int select_count = 0;
	struct interface *interface;
#if defined(HAS_ROUTING_SOCKET)
	struct asyncio_reader *asyncio_reader;
#endif

	handler_calls++;

	/*
	 * If we have something to do, freeze a timestamp.
	 * See below for the other cases (nothing (left) to do or error)
	 */
	ts = *cts;

	/*
	 * Do a poll to see who has data
	 */

	fds = activefds;
	tvzero.tv_sec = tvzero.tv_usec = 0;

	n = select(maxactivefd+1, &fds, (fd_set *)0, (fd_set *)0, &tvzero);

	/*
	 * If there are no packets waiting just return
	 */
	if (n < 0)
	{
		int err = errno;
		/*
		 * extended FAU debugging output
		 */
		if (err != EINTR)
		    netsyslog(LOG_ERR,
			      "select(%d, %s, 0L, 0L, &0.0) error: %m",
			      maxactivefd+1,
			      fdbits(maxactivefd, &activefds));
		if (err == EBADF) {
			int j, b;
			fds = activefds;
			for (j = 0; j <= maxactivefd; j++)
			    if ((FD_ISSET(j, &fds) && (read(j, &b, 0) == -1)))
				netsyslog(LOG_ERR, "Bad file descriptor %d", j);
		}
		return;
	}
	else if (n == 0)
		return;

	++handler_pkts;

#ifdef REFCLOCK
	/*
	 * Check out the reference clocks first, if any
	 */

	if (refio != NULL)
	{
		register struct refclockio *rp;

		for (rp = refio; rp != NULL; rp = rp->next)
		{
			fd = rp->fd;

			if (FD_ISSET(fd, &fds))
			{
				do {
					++select_count;
					buflen = read_refclock_packet(fd, rp, ts);
				} while (buflen > 0);

			} /* End if (FD_ISSET(fd, &fds)) */
		} /* End for (rp = refio; rp != 0 && n > 0; rp = rp->next) */
	} /* End if (refio != 0) */

#endif /* REFCLOCK */

	/*
	 * Loop through the interfaces looking for data to read.
	 */
	for (interface = ISC_LIST_TAIL(inter_list);
	     interface != NULL;
	     interface = ISC_LIST_PREV(interface, link))
	{
		for (doing = 0; (doing < 2); doing++)
		{
			if (doing == 0)
			{
				fd = interface->fd;
			}
			else
			{
				if (!(interface->flags & INT_BCASTOPEN))
				    break;
				fd = interface->bfd;
			}
			if (fd < 0) continue;
			if (FD_ISSET(fd, &fds))
			{
				do {
					++select_count;
					buflen = read_network_packet(fd, interface, ts);
				} while (buflen > 0);
			}
		/* Check more interfaces */
		}
	}

#ifdef HAS_ROUTING_SOCKET
	/*
	 * scan list of asyncio readers - currently only used for routing sockets
	 */
	asyncio_reader = ISC_LIST_TAIL(asyncio_reader_list);

	while (asyncio_reader != NULL)
	{
	        struct asyncio_reader *next = ISC_LIST_PREV(asyncio_reader, link);
		if (FD_ISSET(asyncio_reader->fd, &fds)) {
			++select_count;
			asyncio_reader->receiver(asyncio_reader);
		}
		asyncio_reader = next;
	}
#endif /* HAS_ROUTING_SOCKET */
	
	/*
	 * Done everything from that select.
	 */

	/*
	 * If nothing to do, just return.
	 * If an error occurred, complain and return.
	 */
	if (select_count == 0) /* We really had nothing to do */
	{
#ifdef DEBUG
		if (debug)
		    netsyslog(LOG_DEBUG, "input_handler: select() returned 0");
#endif
		return;
	}
		/* We've done our work */
#ifdef DEBUG_TIMING
	get_systime(&ts_e);
	/*
	 * (ts_e - ts) is the amount of time we spent
	 * processing this gob of file descriptors.  Log
	 * it.
	 */
	L_SUB(&ts_e, &ts);
	collect_timing(NULL, "input handler", 1, &ts_e);
	if (debug > 3)
	    netsyslog(LOG_INFO, "input_handler: Processed a gob of fd's in %s msec", lfptoms(&ts_e, 6));
#endif
	/* just bail. */
	return;
}

#endif

/*
 * findinterface - find local interface corresponding to address
 */
struct interface *
findinterface(
	struct sockaddr_storage *addr
	)
{
	struct interface *interface;
	
	interface = findlocalinterface(addr, INT_WILDCARD);

	if (interface == NULL)
	{
		DPRINTF(4, ("Found no interface for address %s - returning wildcard\n",
			    stoa(addr)));

		return (ANY_INTERFACE_CHOOSE(addr));
	}
	else
	{
		DPRINTF(4, ("Found interface #%d %s for address %s\n",
			    interface->ifnum, interface->name, stoa(addr)));

		return (interface);
	}
}

/*
 * findlocalinterface - find local interface index corresponding to address
 *
 * This code attempts to find the local sending address for an outgoing
 * address by connecting a new socket to destinationaddress:NTP_PORT
 * and reading the sockname of the resulting connect.
 * the complicated sequence simulates the routing table lookup
 * for to first hop without duplicating any of the routing logic into
 * ntpd. preferably we would have used an API call - but its not there -
 * so this is the best we can do here short of duplicating to entire routing
 * logic in ntpd which would be a silly and really unportable thing to do.
 *
 */
static struct interface *
findlocalinterface(
	struct sockaddr_storage *addr,
	int flags
	)
{
	SOCKET s;
	int rtn;
	struct sockaddr_storage saddr;
	GETSOCKNAME_SOCKLEN_TYPE saddrlen = SOCKLEN(addr);
	struct interface *iface;

	DPRINTF(4, ("Finding interface for addr %s in list of addresses\n",
		    stoa(addr)));

	memset(&saddr, 0, sizeof(saddr));
	saddr.ss_family = addr->ss_family;
	if(addr->ss_family == AF_INET) {
		memcpy(&((struct sockaddr_in*)&saddr)->sin_addr, &((struct sockaddr_in*)addr)->sin_addr, sizeof(struct in_addr));
		((struct sockaddr_in*)&saddr)->sin_port = htons(NTP_PORT);
	}
#ifdef INCLUDE_IPV6_SUPPORT
	else if(addr->ss_family == AF_INET6) {
 		memcpy(&((struct sockaddr_in6*)&saddr)->sin6_addr, &((struct sockaddr_in6*)addr)->sin6_addr, sizeof(struct in6_addr));
		((struct sockaddr_in6*)&saddr)->sin6_port = htons(NTP_PORT);
# ifdef ISC_PLATFORM_HAVESCOPEID
		((struct sockaddr_in6*)&saddr)->sin6_scope_id = ((struct sockaddr_in6*)addr)->sin6_scope_id;
# endif
	}
#endif
	
	s = socket(addr->ss_family, SOCK_DGRAM, 0);
	if (s == INVALID_SOCKET)
		return NULL;

	rtn = connect(s, (struct sockaddr *)&saddr, SOCKLEN(&saddr));
#ifndef SYS_WINNT
	if (rtn < 0)
#else
	if (rtn == SOCKET_ERROR)
#endif
	{
		closesocket(s);
		return NULL;
	}

	rtn = getsockname(s, (struct sockaddr *)&saddr, &saddrlen);
	closesocket(s);
#ifndef SYS_WINNT
	if (rtn < 0)
#else
	if (rtn == SOCKET_ERROR)
#endif
		return NULL;

	DPRINTF(4, ("findlocalinterface: kernel maps %s to %s\n", stoa(addr), stoa(&saddr)));
	
	iface = getinterface(&saddr, flags);

	/* Don't both with ignore interfaces */
	if (iface != NULL && iface->ignore_packets == ISC_TRUE)
	{
		return NULL;
	}
	else
	{
		return iface;
	}
}

/*
 * fetch an interface structure the matches the
 * address is has the given flags not set
 */
static struct interface *
getinterface(struct sockaddr_storage *addr, int flags)
{
	struct interface *interface = find_addr_in_list(addr);

	if (interface != NULL && interface->flags & flags)
	{
		return NULL;
	}
	else
	{
		return interface;
	}
}

/*
 * findlocalcastinterface - find local *cast interface index corresponding to address
 * depending on the flags passed
 */
static struct interface *
findlocalcastinterface(
	struct sockaddr_storage *addr, int flags
	)
{
	struct interface *interface;
	struct interface *nif = NULL;
#ifdef INCLUDE_IPV6_MULTICAST_SUPPORT
	isc_boolean_t want_linklocal;
#endif 

	/*
	 * see how kernel maps the mcast address
	 */
        nif = findlocalinterface(addr, 0);

	if (nif) {
		DPRINTF(2, ("findlocalcastinterface: kernel recommends interface #%d %s\n", nif->ifnum, nif->name));
		return nif;
	}

#ifdef INCLUDE_IPV6_MULTICAST_SUPPORT
	want_linklocal = ISC_FALSE; 
	if (addr_ismulticast(addr) && flags == INT_MULTICAST)
	{
		if (IN6_IS_ADDR_MC_LINKLOCAL(&((struct sockaddr_in6*)addr)->sin6_addr))
		{
			want_linklocal = ISC_TRUE;
		}
		else if (IN6_IS_ADDR_MC_SITELOCAL(&((struct sockaddr_in6*)addr)->sin6_addr))
		{
			want_linklocal = ISC_TRUE;
		}
	}
#endif

	for (interface = ISC_LIST_HEAD(inter_list);
	     interface != NULL;
	     interface = ISC_LIST_NEXT(interface, link)) 
	  {
		/* use only allowed addresses */
		if (interface->ignore_packets == ISC_TRUE)
			continue;

		/* Skip the loopback and wildcard addresses */
		if (interface->flags & (INT_LOOPBACK|INT_WILDCARD))
			continue;

		/* Skip if different family */
		if(interface->sin.ss_family != addr->ss_family)
			continue;

		/* Is this it one of these based on flags? */
		if (!(interface->flags & flags))
			continue;

		/* for IPv6 multicast check the address for linklocal */
#ifdef INCLUDE_IPV6_MULTICAST_SUPPORT
		if (flags == INT_MULTICAST && interface->sin.ss_family == AF_INET6 &&
		   (IN6_IS_ADDR_LINKLOCAL(&((struct sockaddr_in6*)&interface->sin)->sin6_addr))
		   && want_linklocal == ISC_TRUE)
		{
			nif = interface;
			break;
		}
		/* If we want a linklocal address and this isn't it, skip */\
		if (want_linklocal == ISC_TRUE)
			continue;
#endif
		/* Otherwise just look for the flag */
		if((interface->flags & flags))
		{
			nif = interface;
			break;
		}
	}
#ifdef DEBUG
	if (debug > 2) 
	{
		if (nif)
			printf("findlocalcastinterface: found interface #%d %s\n", nif->ifnum, nif->name);
		else
			printf("findlocalcastinterface: no interface found for %s flags 0x%x\n", stoa(addr), flags);
	}
#endif
	return (nif);
}

/*
 * findbcastinter - find broadcast interface corresponding to address
 */
struct interface *
findbcastinter(
	struct sockaddr_storage *addr
	)
{
#if !defined(MPE) && (defined(SIOCGIFCONF) || defined(SYS_WINNT))
        struct interface *interface;
	
	
	DPRINTF(4, ("Finding broadcast/multicast interface for addr %s in list of addresses\n",
		    stoa(addr)));

	interface = findlocalinterface(addr, INT_LOOPBACK|INT_WILDCARD);
	
	if (interface != NULL)
	{
		DPRINTF(4, ("Found bcast-/mcast- interface index #%d %s\n", interface->ifnum, interface->name));
		return interface;
	}

	/* plan B - try to find something reasonable in our lists in case kernel lookup doesn't help */

	for (interface = ISC_LIST_HEAD(inter_list);
	     interface != NULL;
	     interface = ISC_LIST_NEXT(interface, link)) 
	{
	        if (interface->flags & INT_WILDCARD)
		        continue;
		
		/* Don't bother with ignored interfaces */
		if (interface->ignore_packets == ISC_TRUE)
			continue;
		
		/*
		 * First look if this is the correct family
		 */
		if(interface->sin.ss_family != addr->ss_family)
	  		continue;

		/* Skip the loopback addresses */
		if (interface->flags & INT_LOOPBACK)
			continue;

		/*
		 * If we are looking to match a multicast address grab it.
		 */
		if (addr_ismulticast(addr) == ISC_TRUE && interface->flags & INT_MULTICAST)
		{
#ifdef INCLUDE_IPV6_SUPPORT
			if(addr->ss_family == AF_INET6) {
				/* Only use link-local address for link-scope mcast */
				if(IN6_IS_ADDR_MC_LINKLOCAL(&((struct sockaddr_in6*)addr)->sin6_addr) &&
				  !IN6_IS_ADDR_LINKLOCAL(&((struct sockaddr_in6*)&interface->sin)->sin6_addr)) {
					continue;
				}
			}
#endif
			break;
		}

		/*
		 * We match only those interfaces marked as
		 * broadcastable and either the explicit broadcast
		 * address or the network portion of the IP address.
		 * Sloppy.
		 */
		if(addr->ss_family == AF_INET) {
			if (SOCKCMP(&interface->bcast, addr)) {
				break;
			}
			if ((NSRCADR(&interface->sin) &
			     NSRCADR(&interface->mask)) == (NSRCADR(addr) &
							    NSRCADR(&interface->mask)))
				break;
		}
#ifdef INCLUDE_IPV6_SUPPORT
		else if(addr->ss_family == AF_INET6) {
			if (SOCKCMP(&interface->bcast, addr)) {
				break;
			}
			if (SOCKCMP(netof(&interface->sin), netof(addr))) {
				break;
			}
		}
#endif
	}
#endif /* SIOCGIFCONF */
	if (interface == NULL) {
		DPRINTF(4, ("No bcast interface found for %s\n", stoa(addr)));
		return ANY_INTERFACE_CHOOSE(addr);
	} else {
		DPRINTF(4, ("Found bcast-/mcast- interface index #%d %s\n", interface->ifnum, interface->name));
		return interface;
	}
}


/*
 * io_clr_stats - clear I/O module statistics
 */
void
io_clr_stats(void)
{
	packets_dropped = 0;
	packets_ignored = 0;
	packets_received = 0;
	packets_sent = 0;
	packets_notsent = 0;

	handler_calls = 0;
	handler_pkts = 0;
	io_timereset = current_time;
}


#ifdef REFCLOCK
/*
 * io_addclock - add a reference clock to the list and arrange that we
 *				 get SIGIO interrupts from it.
 */
int
io_addclock(
	struct refclockio *rio
	)
{
	BLOCKIO();
	/*
	 * Stuff the I/O structure in the list and mark the descriptor
	 * in use.	There is a harmless (I hope) race condition here.
	 */
	rio->next = refio;

# ifdef HAVE_SIGNALED_IO
	if (init_clock_sig(rio))
	{
		UNBLOCKIO();
		return 0;
	}
# elif defined(HAVE_IO_COMPLETION_PORT)
	if (io_completion_port_add_clock_io(rio))
	{
		UNBLOCKIO();
		return 0;
	}
# endif

	/*
	 * enqueue
	 */
	refio = rio;

        /*
	 * register fd
	 */
	add_fd_to_list(rio->fd, FD_TYPE_FILE);

	UNBLOCKIO();
	return 1;
}

/*
 * io_closeclock - close the clock in the I/O structure given
 */
void
io_closeclock(
	struct refclockio *rio
	)
{
	BLOCKIO();
	/*
	 * Remove structure from the list
	 */
	if (refio == rio)
	{
		refio = rio->next;
	}
	else
	{
		register struct refclockio *rp;

		for (rp = refio; rp != NULL; rp = rp->next)
		    if (rp->next == rio)
		    {
			    rp->next = rio->next;
			    break;
		    }

		if (rp == NULL) {
			UNBLOCKIO();
			return;
		}
	}

	/*
	 * Close the descriptor.
	 */
	close_and_delete_fd_from_list(rio->fd);
	UNBLOCKIO();
}
#endif	/* REFCLOCK */

/*
 * On NT a SOCKET is an unsigned int so we cannot possibly keep it in
 * an array. So we use one of the ISC_LIST functions to hold the
 * socket value and use that when we want to enumerate it.
 */
void
kill_asyncio(int startfd)
{
	vsock_t *lsock;
	vsock_t *next;

	BLOCKIO();

	lsock = ISC_LIST_HEAD(fd_list);
	while (lsock != NULL) {
		/*
		 * careful here - list is being dismantled while
		 * we scan it - setting next here insures that
		 * we are able to correctly scan the list
		 */
		next = ISC_LIST_NEXT(lsock, link);
		/*
		 * will remove socket from list
		 */
		close_and_delete_fd_from_list(lsock->fd);
		lsock = next;
	}

	UNBLOCKIO();
}

/*
 * Add and delete functions for the list of open sockets
 */
static void
add_fd_to_list(SOCKET fd, enum desc_type type) {
	vsock_t *lsock = (vsock_t *)emalloc(sizeof(vsock_t));
	lsock->fd = fd;
	lsock->type = type;

	ISC_LIST_APPEND(fd_list, lsock, link);
	/*
	 * I/O Completion Ports don't care about the select and FD_SET
	 */
#ifndef HAVE_IO_COMPLETION_PORT
	if (fd < 0 || fd >= FD_SETSIZE) {
		msyslog(LOG_ERR, "Too many sockets in use, FD_SETSIZE %d exceeded",
			FD_SETSIZE);
		exit(1);
	}
	/*
	 * keep activefds in sync
	 */
	if (fd > maxactivefd)
	    maxactivefd = fd;
	FD_SET( (u_int)fd, &activefds);
#endif
}

static void
close_and_delete_fd_from_list(SOCKET fd) {

	vsock_t *next;
	vsock_t *lsock = ISC_LIST_HEAD(fd_list);

	while(lsock != NULL) {
		next = ISC_LIST_NEXT(lsock, link);
		if(lsock->fd == fd) {
			ISC_LIST_DEQUEUE_TYPE(fd_list, lsock, link, vsock_t);

			switch (lsock->type) {
			case FD_TYPE_SOCKET:
#ifdef SYS_WINNT
				closesocket(lsock->fd);
				break;
#endif
			case FD_TYPE_FILE:
				(void) close(lsock->fd);
				break;
			default:
				msyslog(LOG_ERR, "internal error - illegal descriptor type %d - EXITING", (int)lsock->type);
				exit(1);
			}

			free(lsock);
			/*
			 * I/O Completion Ports don't care about select and fd_set
			 */
#ifndef HAVE_IO_COMPLETION_PORT
			/*
			 * remove from activefds
			 */
			FD_CLR( (u_int) fd, &activefds);
			
			if (fd == maxactivefd) {
				int i, newmax = 0;
				for (i = 0; i < maxactivefd; i++)
					if (FD_ISSET(i, &activefds))
						newmax = i;
				maxactivefd = newmax;
			}
#endif
			break;
		}
		lsock = next;
	}
}

static void
add_addr_to_list(struct sockaddr_storage *addr, struct interface *interface){
#ifdef DEBUG
	if (find_addr_in_list(addr) == NULL) {
#endif
		/* not there yet - add to list */
		remaddr_t *laddr = (remaddr_t *)emalloc(sizeof(remaddr_t));
		memcpy(&laddr->addr, addr, sizeof(struct sockaddr_storage));
		laddr->interface = interface;
		
		ISC_LIST_APPEND(remoteaddr_list, laddr, link);
		
		DPRINTF(4, ("Added addr %s to list of addresses\n",
			    stoa(addr)));
#ifdef DEBUG
	} else {
		DPRINTF(4, ("WARNING: Attempt to add duplicate addr %s to address list\n",
			    stoa(addr)));
	}
#endif
}

static void
delete_addr_from_list(struct sockaddr_storage *addr) {

	remaddr_t *next;
	remaddr_t *laddr = ISC_LIST_HEAD(remoteaddr_list);

	while(laddr != NULL) {
		next = ISC_LIST_NEXT(laddr, link);
		if(SOCKCMP(&laddr->addr, addr)) {
			ISC_LIST_DEQUEUE_TYPE(remoteaddr_list, laddr, link, remaddr_t);
			DPRINTF(4, ("Deleted addr %s from list of addresses\n",
				    stoa(addr)));
			free(laddr);
			break;
		}
		laddr = next;
	}
}

static void
delete_interface_from_list(struct interface *iface) {
	remaddr_t *next;
	remaddr_t *laddr = ISC_LIST_HEAD(remoteaddr_list);

	while(laddr != NULL) {
		next = ISC_LIST_NEXT(laddr, link);
		if (laddr->interface == iface) {
			ISC_LIST_DEQUEUE_TYPE(remoteaddr_list, laddr, link, remaddr_t);
			DPRINTF(4, ("Deleted addr %s for interface #%d %s from list of addresses\n",
				    stoa(&laddr->addr), iface->ifnum, iface->name));
			free(laddr);
		}
		laddr = next;
	}
}

static struct interface *
find_addr_in_list(struct sockaddr_storage *addr) {

	remaddr_t *next;
	remaddr_t *laddr = ISC_LIST_HEAD(remoteaddr_list);
	DPRINTF(4, ("Searching for addr %s in list of addresses - ",
		    stoa(addr)));

	while(laddr != NULL) {
		next = ISC_LIST_NEXT(laddr, link);
		if(SOCKCMP(&laddr->addr, addr)) {
			DPRINTF(4, ("FOUND\n"));
			return laddr->interface;
		}
		else
			laddr = next;
	}
	DPRINTF(4, ("NOT FOUND\n"));
	return NULL; /* Not found */
}

/*
 * Find the given address with the associated flag in the list
 */
static struct interface *
find_flagged_addr_in_list(struct sockaddr_storage *addr, int flag) {

	remaddr_t *next;
	remaddr_t *laddr = ISC_LIST_HEAD(remoteaddr_list);
	DPRINTF(4, ("Finding addr %s in list of addresses\n",
		    stoa(addr)));

	while(laddr != NULL) {
		next = ISC_LIST_NEXT(laddr, link);
		if(SOCKCMP(&laddr->addr, addr) && (laddr->interface->flags & flag)) {
			return laddr->interface;
			break;
		}
		else
			laddr = next;
	}
	return NULL; /* Not found */
}

#ifdef HAS_ROUTING_SOCKET
#include <net/route.h>

#ifndef UPDATE_GRACE
#define UPDATE_GRACE	2	/* wait UPDATE_GRACE seconds before scanning */
#endif

static void
process_routing_msgs(struct asyncio_reader *reader)
{
	char buffer[5120];
	char *p = buffer;

	int cnt;
	
	if (disable_dynamic_updates) {
		/*
		 * discard ourselves if we are not need any more
		 * usually happens when running unprivileged
		 */
		remove_asyncio_reader(reader);
		delete_asyncio_reader(reader);
		return;
	}

	cnt = read(reader->fd, buffer, sizeof(buffer));
	
	if (cnt < 0) {
		msyslog(LOG_ERR, "i/o error on routing socket %m - disabling");
		remove_asyncio_reader(reader);
		delete_asyncio_reader(reader);
		return;
	}

	/*
	 * process routing message
	 */
	while ((p + sizeof(struct rt_msghdr)) <= (buffer + cnt))
	{
		struct rt_msghdr *rtm;
		
		rtm = (struct rt_msghdr *)p;
		if (rtm->rtm_version != RTM_VERSION) {
			msyslog(LOG_ERR, "version mismatch on routing socket %m - disabling");
			remove_asyncio_reader(reader);
			delete_asyncio_reader(reader);
			return;
		}
		
		switch (rtm->rtm_type) {
#ifdef RTM_NEWADDR
		case RTM_NEWADDR:
#endif
#ifdef RTM_DELADDR
		case RTM_DELADDR:
#endif
#ifdef RTM_ADD
		case RTM_ADD:
#endif
#ifdef RTM_DELETE
		case RTM_DELETE:
#endif
#ifdef RTM_REDIRECT
		case RTM_REDIRECT:
#endif
#ifdef RTM_CHANGE
		case RTM_CHANGE:
#endif
#ifdef RTM_LOSING
		case RTM_LOSING:
#endif
#ifdef RTM_IFINFO
		case RTM_IFINFO:
#endif
#ifdef RTM_IFANNOUNCE
		case RTM_IFANNOUNCE:
#endif
			/*
			 * we are keen on new and deleted addresses and if an interface goes up and down or routing changes
			 */
			DPRINTF(3, ("routing message op = %d: scheduling interface update\n", rtm->rtm_type));
			timer_interfacetimeout(current_time + UPDATE_GRACE);
			break;
		default:
			/*
			 * the rest doesn't bother us.
			 */
			DPRINTF(4, ("routing message op = %d: ignored\n", rtm->rtm_type));
			break;
		}
		p += rtm->rtm_msglen;
	}
}

/*
 * set up routing notifications
 */
static void
init_async_notifications()
{
	struct asyncio_reader *reader;
	int fd = socket(PF_ROUTE, SOCK_RAW, 0);
	
	if (fd >= 0) {
		fd = move_fd(fd);
		init_nonblocking_io(fd);
#if defined(HAVE_SIGNALED_IO)
		init_socket_sig(fd);
#endif /* HAVE_SIGNALED_IO */
		
		reader = new_asyncio_reader();

		reader->fd = fd;
		reader->receiver = process_routing_msgs;
		
		add_asyncio_reader(reader, FD_TYPE_SOCKET);
		msyslog(LOG_INFO, "Listening on routing socket on fd #%d for interface updates", fd);
	} else {
		msyslog(LOG_ERR, "unable to open routing socket (%m) - using polled interface update");
	}
}
#else
static void
init_async_notifications()
{
}
#endif
