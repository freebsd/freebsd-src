/* dispatch.c

   Network input dispatcher... */

/*
 * Copyright (c) 1995, 1996, 1997, 1998, 1999
 * The Internet Software Consortium.   All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of The Internet Software Consortium nor the names
 *    of its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INTERNET SOFTWARE CONSORTIUM AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNET SOFTWARE CONSORTIUM OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This software has been written for the Internet Software Consortium
 * by Ted Lemon <mellon@fugue.com> in cooperation with Vixie
 * Enterprises.  To learn more about the Internet Software Consortium,
 * see ``http://www.vix.com/isc''.  To learn more about Vixie
 * Enterprises, see ``http://www.vix.com''.
 */

#ifndef lint
static char copyright[] =
"$Id: dispatch.c,v 1.47.2.15 1999/07/13 12:51:55 mellon Exp $ Copyright (c) 1995, 1996, 1997, 1998, 1999 The Internet Software Consortium.  All rights reserved.\n";
#endif /* not lint */

#include "dhcpd.h"
#include <sys/ioctl.h>

struct interface_info *interfaces, *dummy_interfaces, *fallback_interface;
struct protocol *protocols;
struct timeout *timeouts;
static struct timeout *free_timeouts;
static int interfaces_invalidated;
void (*bootp_packet_handler) PROTO ((struct interface_info *,
				     struct dhcp_packet *, int, unsigned int,
				     struct iaddr, struct hardware *));

int quiet_interface_discovery;

/* Use the SIOCGIFCONF ioctl to get a list of all the attached interfaces.
   For each interface that's of type INET and not the loopback interface,
   register that interface with the network I/O software, figure out what
   subnet it's on, and add it to the list of interfaces. */

void discover_interfaces (state)
	int state;
{
	struct interface_info *tmp;
	struct interface_info *last, *next;
	char buf [8192];
	struct ifconf ic;
	struct ifreq ifr;
	int i;
	int sock;
	struct subnet *subnet;
	struct shared_network *share;
	struct sockaddr_in foo;
	int ir;
	struct ifreq *tif;
#ifdef ALIAS_NAMES_PERMUTED
	char *s;
#endif

	/* Create an unbound datagram socket to do the SIOCGIFADDR ioctl on. */
	if ((sock = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
		error ("Can't create addrlist socket");

	/* Get the interface configuration information... */
	ic.ifc_len = sizeof buf;
	ic.ifc_ifcu.ifcu_buf = (caddr_t)buf;
	i = ioctl(sock, SIOCGIFCONF, &ic);

	if (i < 0)
		error ("ioctl: SIOCGIFCONF: %m");

	/* If we already have a list of interfaces, and we're running as
	   a DHCP server, the interfaces were requested. */
	if (interfaces && (state == DISCOVER_SERVER ||
			   state == DISCOVER_RELAY ||
			   state == DISCOVER_REQUESTED))
		ir = 0;
	else if (state == DISCOVER_UNCONFIGURED)
		ir = INTERFACE_REQUESTED | INTERFACE_AUTOMATIC;
	else
		ir = INTERFACE_REQUESTED;

	/* Cycle through the list of interfaces looking for IP addresses. */
	for (i = 0; i < ic.ifc_len;) {
		struct ifreq *ifp = (struct ifreq *)((caddr_t)ic.ifc_req + i);
#ifdef HAVE_SA_LEN
		if (ifp -> ifr_addr.sa_len > sizeof (struct sockaddr))
			i += (sizeof ifp -> ifr_name) + ifp -> ifr_addr.sa_len;
		else
#endif
			i += sizeof *ifp;

#ifdef ALIAS_NAMES_PERMUTED
		if ((s = strrchr (ifp -> ifr_name, ':'))) {
			*s = 0;
		}
#endif

#ifdef SKIP_DUMMY_INTERFACES
		if (!strncmp (ifp -> ifr_name, "dummy", 5))
			continue;
#endif


		/* See if this is the sort of interface we want to
		   deal with. */
		strcpy (ifr.ifr_name, ifp -> ifr_name);
		if (ioctl (sock, SIOCGIFFLAGS, &ifr) < 0)
			error ("Can't get interface flags for %s: %m",
			       ifr.ifr_name);

		/* Skip loopback, point-to-point and down interfaces,
		   except don't skip down interfaces if we're trying to
		   get a list of configurable interfaces. */
		if ((ifr.ifr_flags & IFF_LOOPBACK) ||
#ifdef HAVE_IFF_POINTOPOINT
		    (ifr.ifr_flags & IFF_POINTOPOINT) ||
#endif
		    (!(ifr.ifr_flags & IFF_UP) &&
		     state != DISCOVER_UNCONFIGURED))
			continue;
		
		/* See if we've seen an interface that matches this one. */
		for (tmp = interfaces; tmp; tmp = tmp -> next)
			if (!strcmp (tmp -> name, ifp -> ifr_name))
				break;

		/* If there isn't already an interface by this name,
		   allocate one. */
		if (!tmp) {
			tmp = ((struct interface_info *)
			       dmalloc (sizeof *tmp, "discover_interfaces"));
			if (!tmp)
				error ("Insufficient memory to %s %s",
				       "record interface", ifp -> ifr_name);
			strcpy (tmp -> name, ifp -> ifr_name);
			tmp -> next = interfaces;
			tmp -> flags = ir;
			interfaces = tmp;
		}

		/* If we have the capability, extract link information
		   and record it in a linked list. */
#ifdef HAVE_AF_LINK
		if (ifp -> ifr_addr.sa_family == AF_LINK) {
			struct sockaddr_dl *foo = ((struct sockaddr_dl *)
						   (&ifp -> ifr_addr));
			tmp -> hw_address.hlen = foo -> sdl_alen;
			tmp -> hw_address.htype = HTYPE_ETHER; /* XXX */
			memcpy (tmp -> hw_address.haddr,
				LLADDR (foo), foo -> sdl_alen);
		} else
#endif /* AF_LINK */

		if (ifp -> ifr_addr.sa_family == AF_INET) {
			struct iaddr addr;

			/* Get a pointer to the address... */
			memcpy (&foo, &ifp -> ifr_addr,
				sizeof ifp -> ifr_addr);

			/* We don't want the loopback interface. */
			if (foo.sin_addr.s_addr == htonl (INADDR_LOOPBACK))
				continue;


			/* If this is the first real IP address we've
			   found, keep a pointer to ifreq structure in
			   which we found it. */
			if (!tmp -> ifp) {
#ifdef HAVE_SA_LEN
				int len = ((sizeof ifp -> ifr_name) +
					   ifp -> ifr_addr.sa_len);
#else
				int len = sizeof *ifp;
#endif
				tif = (struct ifreq *)malloc (len);
				if (!tif)
					error ("no space to remember ifp.");
				memcpy (tif, ifp, len);
				tmp -> ifp = tif;
				tmp -> primary_address = foo.sin_addr;
			}

			/* Grab the address... */
			addr.len = 4;
			memcpy (addr.iabuf, &foo.sin_addr.s_addr,
				addr.len);

			/* If there's a registered subnet for this address,
			   connect it together... */
			if ((subnet = find_subnet (addr))) {
				/* If this interface has multiple aliases
				   on the same subnet, ignore all but the
				   first we encounter. */
				if (!subnet -> interface) {
					subnet -> interface = tmp;
					subnet -> interface_address = addr;
				} else if (subnet -> interface != tmp) {
					warn ("Multiple %s %s: %s %s", 
					      "interfaces match the",
					      "same subnet",
					      subnet -> interface -> name,
					      tmp -> name);
				}
				share = subnet -> shared_network;
				if (tmp -> shared_network &&
				    tmp -> shared_network != share) {
					warn ("Interface %s matches %s",
					      tmp -> name,
					      "multiple shared networks");
				} else {
					tmp -> shared_network = share;
				}

				if (!share -> interface) {
					share -> interface = tmp;
				} else if (share -> interface != tmp) {
					warn ("Multiple %s %s: %s %s", 
					      "interfaces match the",
					      "same shared network",
					      share -> interface -> name,
					      tmp -> name);
				}
			}
		}
	}

#if defined (LINUX_SLASHPROC_DISCOVERY)
	/* On Linux, interfaces that don't have IP addresses don't show up
	   in the SIOCGIFCONF syscall.   We got away with this prior to
	   Linux 2.1 because we would give each interface an IP address of
	   0.0.0.0 before trying to boot, but that doesn't work after 2.1
	   because we're using LPF, because we can't configure interfaces
	   with IP addresses of 0.0.0.0 anymore (grumble).   This only
	   matters for the DHCP client, of course - the relay agent and
	   server should only care about interfaces that are configured
	   with IP addresses anyway.

	   The PROCDEV_DEVICE (/proc/net/dev) is a kernel-supplied file
	   that, when read, prints a human readable network status.   We
	   extract the names of the network devices by skipping the first
	   two lines (which are header) and then parsing off everything
	   up to the colon in each subsequent line - these lines start
	   with the interface name, then a colon, then a bunch of
	   statistics.   Yes, Virgina, this is a kludge, but you work
	   with what you have. */

	if (state == DISCOVER_UNCONFIGURED) {
		FILE *proc_dev;
		char buffer [256];
		int skip = 2;

		proc_dev = fopen (PROCDEV_DEVICE, "r");
		if (!proc_dev)
			error ("%s: %m", PROCDEV_DEVICE);

		while (fgets (buffer, sizeof buffer, proc_dev)) {
			char *name = buffer;
			char *sep;

			/* Skip the first two blocks, which are header
			   lines. */
			if (skip) {
				--skip;
				continue;
			}

			sep = strrchr (buffer, ':');
			if (sep)
				*sep = '\0';
			while (*name == ' ')
				name++;

			/* See if we've seen an interface that matches
			   this one. */
			for (tmp = interfaces; tmp; tmp = tmp -> next)
				if (!strcmp (tmp -> name, name))
					break;

			/* If we found one, nothing more to do.. */
			if (tmp)
				continue;

			/* Otherwise, allocate one. */
			tmp = ((struct interface_info *)
			       dmalloc (sizeof *tmp, "discover_interfaces"));
			if (!tmp)
				error ("Insufficient memory to %s %s",
				       "record interface", name);
			memset (tmp, 0, sizeof *tmp);
			strcpy (tmp -> name, name);

			tmp -> flags = ir;
			tmp -> next = interfaces;
			interfaces = tmp;
		}
		fclose (proc_dev);
	}
#endif

	/* Now cycle through all the interfaces we found, looking for
	   hardware addresses. */
#if defined (HAVE_SIOCGIFHWADDR) && !defined (HAVE_AF_LINK)
	for (tmp = interfaces; tmp; tmp = tmp -> next) {
		struct ifreq ifr;
		struct sockaddr sa;
		int b, sk;
		
		if (!tmp -> ifp) {
			/* Make up an ifreq structure. */
			tif = (struct ifreq *)malloc (sizeof (struct ifreq));
			if (!tif)
				error ("no space to remember ifp.");
			memset (tif, 0, sizeof (struct ifreq));
			strcpy (tif -> ifr_name, tmp -> name);
			tmp -> ifp = tif;
		}

		/* Read the hardware address from this interface. */
		ifr = *tmp -> ifp;
		if (ioctl (sock, SIOCGIFHWADDR, &ifr) < 0)
			continue;
		
		sa = *(struct sockaddr *)&ifr.ifr_hwaddr;
		
		switch (sa.sa_family) {
#ifdef HAVE_ARPHRD_TUNNEL
		      case ARPHRD_TUNNEL:
			/* ignore tunnel interfaces. */
#endif
#ifdef HAVE_ARPHRD_ROSE
		      case ARPHRD_ROSE:
#endif
#ifdef HAVE_ARPHRD_LOOPBACK
		      case ARPHRD_LOOPBACK:
			/* ignore loopback interface */
			break;
#endif

		      case ARPHRD_ETHER:
			tmp -> hw_address.hlen = 6;
			tmp -> hw_address.htype = ARPHRD_ETHER;
			memcpy (tmp -> hw_address.haddr, sa.sa_data, 6);
			break;

#ifndef HAVE_ARPHRD_IEEE802
# define ARPHRD_IEEE802 HTYPE_IEEE802
#endif
		      case ARPHRD_IEEE802:
			tmp -> hw_address.hlen = 6;
			tmp -> hw_address.htype = ARPHRD_IEEE802;
			memcpy (tmp -> hw_address.haddr, sa.sa_data, 6);
			break;

#ifndef HAVE_ARPHRD_FDDI
# define ARPHRD_FDDI HTYPE_FDDI
#endif
		      case ARPHRD_FDDI:
			tmp -> hw_address.hlen = 16;
			tmp -> hw_address.htype = HTYPE_FDDI; /* XXX */
			memcpy (tmp -> hw_address.haddr, sa.sa_data, 16);
			break;

#ifdef HAVE_ARPHRD_METRICOM
		      case ARPHRD_METRICOM:
			tmp -> hw_address.hlen = 6;
			tmp -> hw_address.htype = ARPHRD_METRICOM;
			memcpy (tmp -> hw_address.haddr, sa.sa_data, 6);
			break;
#endif

#ifdef HAVE_ARPHRD_AX25
		      case ARPHRD_AX25:
			tmp -> hw_address.hlen = 6;
			tmp -> hw_address.htype = ARPHRD_AX25;
			memcpy (tmp -> hw_address.haddr, sa.sa_data, 6);
			break;
#endif

#ifdef HAVE_ARPHRD_NETROM
		      case ARPHRD_NETROM:
			tmp -> hw_address.hlen = 6;
			tmp -> hw_address.htype = ARPHRD_NETROM;
			memcpy (tmp -> hw_address.haddr, sa.sa_data, 6);
			break;
#endif

		      default:
			warn ("%s: unknown hardware address type %d",
			       ifr.ifr_name, sa.sa_family);
			break;
		}
	}
#endif /* defined (HAVE_SIOCGIFHWADDR) && !defined (HAVE_AF_LINK) */

	/* If we're just trying to get a list of interfaces that we might
	   be able to configure, we can quit now. */
	if (state == DISCOVER_UNCONFIGURED)
		return;

	/* Weed out the interfaces that did not have IP addresses. */
	last = (struct interface_info *)0;
	for (tmp = interfaces; tmp; tmp = next) {
		next = tmp -> next;
		if ((tmp -> flags & INTERFACE_AUTOMATIC) &&
		    state == DISCOVER_REQUESTED)
			tmp -> flags &= ~(INTERFACE_AUTOMATIC |
					  INTERFACE_REQUESTED);
		if (!tmp -> ifp || !(tmp -> flags & INTERFACE_REQUESTED)) {
			if ((tmp -> flags & INTERFACE_REQUESTED) != ir)
				error ("%s: not found", tmp -> name);
			if (!last)
				interfaces = interfaces -> next;
			else
				last -> next = tmp -> next;

			/* Remember the interface in case we need to know
			   about it later. */
			tmp -> next = dummy_interfaces;
			dummy_interfaces = tmp;
			continue;
		}
		last = tmp;

		memcpy (&foo, &tmp -> ifp -> ifr_addr,
			sizeof tmp -> ifp -> ifr_addr);

		/* We must have a subnet declaration for each interface. */
		if (!tmp -> shared_network && (state == DISCOVER_SERVER)) {
			warn ("No subnet declaration for %s (%s).",
			      tmp -> name, inet_ntoa (foo.sin_addr));
			warn ("Please write a subnet declaration in your %s",
			      "dhcpd.conf file for the");
			error ("network segment to which interface %s %s",
			       tmp -> name, "is attached.");
		}

		/* Find subnets that don't have valid interface
		   addresses... */
		for (subnet = (tmp -> shared_network
			       ? tmp -> shared_network -> subnets
			       : (struct subnet *)0);
		     subnet; subnet = subnet -> next_sibling) {
			if (!subnet -> interface_address.len) {
				/* Set the interface address for this subnet
				   to the first address we found. */
				subnet -> interface_address.len = 4;
				memcpy (subnet -> interface_address.iabuf,
					&foo.sin_addr.s_addr, 4);
			}
		}

		/* Register the interface... */
		if_register_receive (tmp);
		if_register_send (tmp);
	}

	/* Now register all the remaining interfaces as protocols. */
	for (tmp = interfaces; tmp; tmp = tmp -> next)
		add_protocol (tmp -> name, tmp -> rfdesc, got_one, tmp);

	close (sock);

	maybe_setup_fallback ();
}

struct interface_info *setup_fallback ()
{
	fallback_interface =
		((struct interface_info *)
		 dmalloc (sizeof *fallback_interface, "discover_interfaces"));
	if (!fallback_interface)
		error ("Insufficient memory to record fallback interface.");
	memset (fallback_interface, 0, sizeof *fallback_interface);
	strcpy (fallback_interface -> name, "fallback");
	fallback_interface -> shared_network =
		new_shared_network ("parse_statement");
	if (!fallback_interface -> shared_network)
		error ("No memory for shared subnet");
	memset (fallback_interface -> shared_network, 0,
		sizeof (struct shared_network));
	fallback_interface -> shared_network -> name = "fallback-net";
	return fallback_interface;
}

void reinitialize_interfaces ()
{
	struct interface_info *ip;

	for (ip = interfaces; ip; ip = ip -> next) {
		if_reinitialize_receive (ip);
		if_reinitialize_send (ip);
	}

	if (fallback_interface)
		if_reinitialize_send (fallback_interface);

	interfaces_invalidated = 1;
}

#ifdef USE_POLL
/* Wait for packets to come in using poll().  When a packet comes in,
   call receive_packet to receive the packet and possibly strip hardware
   addressing information from it, and then call through the
   bootp_packet_handler hook to try to do something with it. */

void dispatch ()
{
	struct protocol *l;
	int nfds = 0;
	struct pollfd *fds;
	int count;
	int i;
	int to_msec;

	nfds = 0;
	for (l = protocols; l; l = l -> next) {
		++nfds;
	}
	fds = (struct pollfd *)malloc ((nfds) * sizeof (struct pollfd));
	if (!fds)
		error ("Can't allocate poll structures.");

	do {
		/* Call any expired timeouts, and then if there's
		   still a timeout registered, time out the select
		   call then. */
	      another:
		if (timeouts) {
			struct timeout *t;
			if (timeouts -> when <= cur_time) {
				t = timeouts;
				timeouts = timeouts -> next;
				(*(t -> func)) (t -> what);
				t -> next = free_timeouts;
				free_timeouts = t;
				goto another;
			}
			/* Figure timeout in milliseconds, and check for
			   potential overflow.   We assume that integers
			   are 32 bits, which is harmless if they're 64
			   bits - we'll just get extra timeouts in that
			   case.    Lease times would have to be quite
			   long in order for a 32-bit integer to overflow,
			   anyway. */
			to_msec = timeouts -> when - cur_time;
			if (to_msec > 2147483)
				to_msec = 2147483;
			to_msec *= 1000;
		} else
			to_msec = -1;

		/* Set up the descriptors to be polled. */
		i = 0;
		for (l = protocols; l; l = l -> next) {
			fds [i].fd = l -> fd;
			fds [i].events = POLLIN;
			fds [i].revents = 0;
			++i;
		}

		/* Wait for a packet or a timeout... XXX */
		count = poll (fds, nfds, to_msec);

		/* Get the current time... */
		GET_TIME (&cur_time);

		/* Not likely to be transitory... */
		if (count < 0) {
			if (errno == EAGAIN || errno == EINTR)
				continue;
			else
				error ("poll: %m");
		}

		i = 0;
		for (l = protocols; l; l = l -> next) {
			if ((fds [i].revents & POLLIN)) {
				fds [i].revents = 0;
				if (l -> handler)
					(*(l -> handler)) (l);
				if (interfaces_invalidated)
					break;
			}
			++i;
		}
		interfaces_invalidated = 0;
	} while (1);
}
#else
/* Wait for packets to come in using select().   When one does, call
   receive_packet to receive the packet and possibly strip hardware
   addressing information from it, and then call through the
   bootp_packet_handler hook to try to do something with it. */

void dispatch ()
{
	fd_set r, w, x;
	struct protocol *l;
	int max = 0;
	int count;
	struct timeval tv, *tvp;

	FD_ZERO (&w);
	FD_ZERO (&x);

	do {
		/* Call any expired timeouts, and then if there's
		   still a timeout registered, time out the select
		   call then. */
	      another:
		if (timeouts) {
			struct timeout *t;
			if (timeouts -> when <= cur_time) {
				t = timeouts;
				timeouts = timeouts -> next;
				(*(t -> func)) (t -> what);
				t -> next = free_timeouts;
				free_timeouts = t;
				goto another;
			}
			tv.tv_sec = timeouts -> when - cur_time;
			tv.tv_usec = 0;
			tvp = &tv;
		} else
			tvp = (struct timeval *)0;

		/* Set up the read mask. */
		FD_ZERO (&r);

		for (l = protocols; l; l = l -> next) {
			FD_SET (l -> fd, &r);
			if (l -> fd > max)
				max = l -> fd;
		}

		/* Wait for a packet or a timeout... XXX */
		count = select (max + 1, &r, &w, &x, tvp);

		/* Get the current time... */
		GET_TIME (&cur_time);

		/* Not likely to be transitory... */
		if (count < 0)
			error ("select: %m");

		for (l = protocols; l; l = l -> next) {
			if (!FD_ISSET (l -> fd, &r))
				continue;
			if (l -> handler)
				(*(l -> handler)) (l);
			if (interfaces_invalidated)
				break;
		}
		interfaces_invalidated = 0;
	} while (1);
}
#endif /* USE_POLL */

void got_one (l)
	struct protocol *l;
{
	struct sockaddr_in from;
	struct hardware hfrom;
	struct iaddr ifrom;
	int result;
	union {
		unsigned char packbuf [4095]; /* Packet input buffer.
					 	 Must be as large as largest
						 possible MTU. */
		struct dhcp_packet packet;
	} u;
	struct interface_info *ip = l -> local;

	if ((result =
	     receive_packet (ip, u.packbuf, sizeof u, &from, &hfrom)) < 0) {
		warn ("receive_packet failed on %s: %m", ip -> name);
		return;
	}
	if (result == 0)
		return;

	if (bootp_packet_handler) {
		ifrom.len = 4;
		memcpy (ifrom.iabuf, &from.sin_addr, ifrom.len);

		(*bootp_packet_handler) (ip, &u.packet, result,
					 from.sin_port, ifrom, &hfrom);
	}
}

int locate_network (packet)
	struct packet *packet;
{
	struct iaddr ia;

	/* If this came through a gateway, find the corresponding subnet... */
	if (packet -> raw -> giaddr.s_addr) {
		struct subnet *subnet;
		ia.len = 4;
		memcpy (ia.iabuf, &packet -> raw -> giaddr, 4);
		subnet = find_subnet (ia);
		if (subnet)
			packet -> shared_network = subnet -> shared_network;
		else
			packet -> shared_network = (struct shared_network *)0;
	} else {
		packet -> shared_network =
			packet -> interface -> shared_network;
	}
	if (packet -> shared_network)
		return 1;
	return 0;
}

void add_timeout (when, where, what)
	TIME when;
	void (*where) PROTO ((void *));
	void *what;
{
	struct timeout *t, *q;

	/* See if this timeout supersedes an existing timeout. */
	t = (struct timeout *)0;
	for (q = timeouts; q; q = q -> next) {
		if (q -> func == where && q -> what == what) {
			if (t)
				t -> next = q -> next;
			else
				timeouts = q -> next;
			break;
		}
		t = q;
	}

	/* If we didn't supersede a timeout, allocate a timeout
	   structure now. */
	if (!q) {
		if (free_timeouts) {
			q = free_timeouts;
			free_timeouts = q -> next;
			q -> func = where;
			q -> what = what;
		} else {
			q = (struct timeout *)malloc (sizeof (struct timeout));
			if (!q)
				error ("Can't allocate timeout structure!");
			q -> func = where;
			q -> what = what;
		}
	}

	q -> when = when;

	/* Now sort this timeout into the timeout list. */

	/* Beginning of list? */
	if (!timeouts || timeouts -> when > q -> when) {
		q -> next = timeouts;
		timeouts = q;
		return;
	}

	/* Middle of list? */
	for (t = timeouts; t -> next; t = t -> next) {
		if (t -> next -> when > q -> when) {
			q -> next = t -> next;
			t -> next = q;
			return;
		}
	}

	/* End of list. */
	t -> next = q;
	q -> next = (struct timeout *)0;
}

void cancel_timeout (where, what)
	void (*where) PROTO ((void *));
	void *what;
{
	struct timeout *t, *q;

	/* Look for this timeout on the list, and unlink it if we find it. */
	t = (struct timeout *)0;
	for (q = timeouts; q; q = q -> next) {
		if (q -> func == where && q -> what == what) {
			if (t)
				t -> next = q -> next;
			else
				timeouts = q -> next;
			break;
		}
		t = q;
	}

	/* If we found the timeout, put it on the free list. */
	if (q) {
		q -> next = free_timeouts;
		free_timeouts = q;
	}
}

/* Add a protocol to the list of protocols... */
void add_protocol (name, fd, handler, local)
	char *name;
	int fd;
	void (*handler) PROTO ((struct protocol *));
	void *local;
{
	struct protocol *p;

	p = (struct protocol *)malloc (sizeof *p);
	if (!p)
		error ("can't allocate protocol struct for %s", name);

	p -> fd = fd;
	p -> handler = handler;
	p -> local = local;

	p -> next = protocols;
	protocols = p;
}

void remove_protocol (proto)
	struct protocol *proto;
{
	struct protocol *p, *next, *prev;

	prev = (struct protocol *)0;
	for (p = protocols; p; p = next) {
		next = p -> next;
		if (p == proto) {
			if (prev)
				prev -> next = p -> next;
			else
				protocols = p -> next;
			free (p);
		}
	}
}
