/* dhclient.c

   DHCP Client. */

/*
 * Copyright (c) 1995, 1996, 1997, 1998, 1999
 * The Internet Software Consortium.    All rights reserved.
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
 *
 * This client was substantially modified and enhanced by Elliot Poger
 * for use on Linux while he was working on the MosquitoNet project at
 * Stanford.
 *
 * The current version owes much to Elliot's Linux enhancements, but
 * was substantially reorganized and partially rewritten by Ted Lemon
 * so as to use the same networking framework that the Internet Software
 * Consortium DHCP server uses.   Much system-specific configuration code
 * was moved into a shell script so that as support for more operating
 * systems is added, it will not be necessary to port and maintain
 * system-specific configuration code to these operating systems - instead,
 * the shell script can invoke the native tools to accomplish the same
 * purpose.
 */

#ifndef lint
static char ocopyright[] =
"$Id: dhclient.c,v 1.44.2.47 2000/09/06 20:59:09 mellon Exp $ Copyright (c) 1995, 1996, 1997, 1998, 1999 The Internet Software Consortium.  All rights reserved.\n"
"$FreeBSD$\n";
#endif /* not lint */

#include "dhcpd.h"
#include "version.h"

TIME cur_time;
TIME default_lease_time = 43200; /* 12 hours... */
TIME max_lease_time = 86400; /* 24 hours... */
struct tree_cache *global_options [256];

char *path_dhclient_conf = _PATH_DHCLIENT_CONF;
char *path_dhclient_db = _PATH_DHCLIENT_DB;
char *path_dhclient_pid = _PATH_DHCLIENT_PID;

int interfaces_requested = 0;

int log_perror = 1;

struct iaddr iaddr_broadcast = { 4, { 255, 255, 255, 255 } };
struct iaddr iaddr_any = { 4, { 0, 0, 0, 0 } };
struct in_addr inaddr_any;
struct sockaddr_in sockaddr_broadcast;

/* ASSERT_STATE() does nothing now; it used to be
   assert (state_is == state_shouldbe). */
#define ASSERT_STATE(state_is, state_shouldbe) {}

u_int16_t local_port;
u_int16_t remote_port;
int log_priority;
int no_daemon;
int save_scripts;
int onetry;

static char copyright[] =
"Copyright 1995, 1996, 1997, 1998, 1999 The Internet Software Consortium.";
static char arr [] = "All rights reserved.";
static char message [] = "Internet Software Consortium DHCP Client";
static char contrib [] = "Please contribute if you find this software useful.";
static char url [] = "For info, please visit http://www.isc.org/dhcp-contrib.html";

static void usage PROTO ((char *));

int main (argc, argv, envp)
	int argc;
	char **argv, **envp;
{
	int i;
	struct servent *ent;
	struct interface_info *ip;
	int seed;
	int quiet = 1;
	char *s;

	s = strrchr (argv [0], '/');
	if (!s)
		s = argv [0];
	else
		s++;

	/* Initially, log errors to stderr as well as to syslogd. */
#ifdef SYSLOG_4_2
	openlog (s, LOG_NDELAY);
	log_priority = DHCPD_LOG_FACILITY;
#else
	openlog (s, LOG_NDELAY, DHCPD_LOG_FACILITY);
#endif

#if !(defined (DEBUG) || defined (SYSLOG_4_2) || defined (__CYGWIN32__))
	setlogmask (LOG_UPTO (LOG_INFO));
#endif	

	for (i = 1; i < argc; i++) {
		if (!strcmp (argv [i], "-p")) {
			if (++i == argc)
				usage (s);
			local_port = htons (atoi (argv [i]));
			debug ("binding to user-specified port %d",
			       ntohs (local_port));
		} else if (!strcmp (argv [i], "-d")) {
			no_daemon = 1;
		} else if (!strcmp (argv [i], "-D")) {
			save_scripts = 1;
		} else if (!strcmp (argv [i], "-cf")) {
			if (++i == argc)
				usage (s);
			path_dhclient_conf = argv [i];
		} else if (!strcmp (argv [i], "-pf")) {
			if (++i == argc)
				usage (s);
			path_dhclient_pid = argv [i];
		} else if (!strcmp (argv [i], "-lf")) {
			if (++i == argc)
				usage (s);
			path_dhclient_db = argv [i];
		} else if (!strcmp (argv [i], "-q")) {
			quiet = 1;
			quiet_interface_discovery = 1;
		} else if (!strcmp (argv [i], "-1")) {
			onetry = 1;
 		} else if (argv [i][0] == '-') {
 		    usage (s);
 		} else {
 		    struct interface_info *tmp =
 			((struct interface_info *)
 			 dmalloc (sizeof *tmp, "specified_interface"));
 		    if (!tmp)
 			error ("Insufficient memory to %s %s",
 			       "record interface", argv [i]);
 		    memset (tmp, 0, sizeof *tmp);
 		    strlcpy (tmp -> name, argv [i], IFNAMSIZ);
 		    tmp -> next = interfaces;
 		    tmp -> flags = INTERFACE_REQUESTED;
		    interfaces_requested = 1;
 		    interfaces = tmp;
 		}
	}

	if (!quiet) {
		note ("%s %s", message, DHCP_VERSION);
		note (copyright);
		note (arr);
		note ("");
		note (contrib);
		note (url);
		note ("");
	} else
		log_perror = 0;

	/* Default to the DHCP/BOOTP port. */
	if (!local_port) {
		ent = getservbyname ("dhcpc", "udp");
		if (!ent)
			local_port = htons (68);
		else
			local_port = ent -> s_port;
#ifndef __CYGWIN32__
		endservent ();
#endif
	}
	remote_port = htons (ntohs (local_port) - 1);	/* XXX */
  
	/* Get the current time... */
	GET_TIME (&cur_time);

	sockaddr_broadcast.sin_family = AF_INET;
	sockaddr_broadcast.sin_port = remote_port;
	sockaddr_broadcast.sin_addr.s_addr = INADDR_BROADCAST;
#ifdef HAVE_SA_LEN
	sockaddr_broadcast.sin_len = sizeof sockaddr_broadcast;
#endif
	inaddr_any.s_addr = INADDR_ANY;

	/* Discover all the network interfaces. */
	discover_interfaces (DISCOVER_UNCONFIGURED);

	/* Parse the dhclient.conf file. */
	read_client_conf ();

	/* Parse the lease database. */
	read_client_leases ();

	/* Rewrite the lease database... */
	rewrite_client_leases ();

	/* If no broadcast interfaces were discovered, call the script
	   and tell it so. */
	if (!interfaces) {
		script_init ((struct interface_info *)0, "NBI",
			     (struct string_list *)0);
		script_go ((struct interface_info *)0);

		note ("No broadcast interfaces found - exiting.");
		/* Nothing more to do. */
		exit (0);
	} else {
		/* Call the script with the list of interfaces. */
		for (ip = interfaces; ip; ip = ip -> next) {
			/* If interfaces were specified, don't configure
			   interfaces that weren't specified! */
			if (interfaces_requested &&
			    ((ip -> flags & (INTERFACE_REQUESTED |
					     INTERFACE_AUTOMATIC)) !=
			     INTERFACE_REQUESTED))
				continue;
			script_init (ip, "PREINIT", (struct string_list *)0);
			if (ip -> client -> alias)
				script_write_params (ip, "alias_",
						     ip -> client -> alias);
			script_go (ip);
		}
	}

	/* At this point, all the interfaces that the script thinks
	   are relevant should be running, so now we once again call
	   discover_interfaces(), and this time ask it to actually set
	   up the interfaces. */
	discover_interfaces (interfaces_requested
			     ? DISCOVER_REQUESTED
			     : DISCOVER_RUNNING);

	/* Make up a seed for the random number generator from current
	   time plus the sum of the last four bytes of each
	   interface's hardware address interpreted as an integer.
	   Not much entropy, but we're booting, so we're not likely to
	   find anything better. */
	seed = 0; /* Unfortunately, what's on the stack isn't random. :') */
	for (ip = interfaces; ip; ip = ip -> next) {
		int junk;
		memcpy (&junk,
			&ip -> hw_address.haddr [ip -> hw_address.hlen -
						 sizeof seed], sizeof seed);
		seed += junk;
	}
	srandom (seed + cur_time);

	/* Start a configuration state machine for each interface. */
	for (ip = interfaces; ip; ip = ip -> next) {
		ip -> client -> state = S_INIT;
		state_reboot (ip);
	}

	/* Set up the bootp packet handler... */
	bootp_packet_handler = do_packet;

	/* Start dispatching packets and timeouts... */
	dispatch ();

	/*NOTREACHED*/
	return 0;
}

static void usage (appname)
	char *appname;
{
	note (message);
	note (copyright);
	note (arr);
	note ("");
	note (contrib);
	note (url);
	note ("");

	warn ("Usage: %s [-D] [-d] [-p <port>] [-cf conf-file]", appname);
	error ("       [-lf lease-file] [-pf pidfile] [-q] [-1] [interface]");
}

void cleanup ()
{
	/* Make sure the pidfile is gone. */
	unlink (path_dhclient_pid);
}

/* Individual States:
 * 
 * Each routine is called from the dhclient_state_machine() in one of
 * these conditions:
 * -> entering INIT state
 * -> recvpacket_flag == 0: timeout in this state
 * -> otherwise: received a packet in this state
 *
 * Return conditions as handled by dhclient_state_machine():
 * Returns 1, sendpacket_flag = 1: send packet, reset timer.
 * Returns 1, sendpacket_flag = 0: just reset the timer (wait for a milestone).
 * Returns 0: finish the nap which was interrupted for no good reason.
 *
 * Several per-interface variables are used to keep track of the process:
 *   active_lease: the lease that is being used on the interface
 *                 (null pointer if not configured yet).
 *   offered_leases: leases corresponding to DHCPOFFER messages that have
 *		     been sent to us by DHCP servers.
 *   acked_leases: leases corresponding to DHCPACK messages that have been
 *		   sent to us by DHCP servers.
 *   sendpacket: DHCP packet we're trying to send.
 *   destination: IP address to send sendpacket to
 * In addition, there are several relevant per-lease variables.
 *   T1_expiry, T2_expiry, lease_expiry: lease milestones
 * In the active lease, these control the process of renewing the lease;
 * In leases on the acked_leases list, this simply determines when we
 * can no longer legitimately use the lease.
 */

void state_reboot (ipp)
	void *ipp;
{
	struct interface_info *ip = ipp;

	/* If we don't remember an active lease, go straight to INIT. */
	if (!ip -> client -> active ||
	    ip -> client -> active -> is_bootp) {
		state_init (ip);
		return;
	}

	/* We are in the rebooting state. */
	ip -> client -> state = S_REBOOTING;

	/* make_request doesn't initialize xid because it normally comes
	   from the DHCPDISCOVER, but we haven't sent a DHCPDISCOVER,
	   so pick an xid now. */
	ip -> client -> xid = random ();

	/* Make a DHCPREQUEST packet, and set appropriate per-interface
	   flags. */
	make_request (ip, ip -> client -> active);
	ip -> client -> destination = iaddr_broadcast;
	ip -> client -> first_sending = cur_time;
	ip -> client -> interval = ip -> client -> config -> initial_interval;

	/* Zap the medium list... */
	ip -> client -> medium = (struct string_list *)0;

	/* Send out the first DHCPREQUEST packet. */
	send_request (ip);
}

/* Called when a lease has completely expired and we've been unable to
   renew it. */

void state_init (ipp)
	void *ipp;
{
	struct interface_info *ip = ipp;

	ASSERT_STATE(state, S_INIT);

	/* Make a DHCPDISCOVER packet, and set appropriate per-interface
	   flags. */
	make_discover (ip, ip -> client -> active);
	ip -> client -> xid = ip -> client -> packet.xid;
	ip -> client -> destination = iaddr_broadcast;
	ip -> client -> state = S_SELECTING;
	ip -> client -> first_sending = cur_time;
	ip -> client -> interval = ip -> client -> config -> initial_interval;

	/* Add an immediate timeout to cause the first DHCPDISCOVER packet
	   to go out. */
	send_discover (ip);
}

/* state_selecting is called when one or more DHCPOFFER packets have been
   received and a configurable period of time has passed. */

void state_selecting (ipp)
	void *ipp;
{
	struct interface_info *ip = ipp;

	struct client_lease *lp, *next, *picked;

	ASSERT_STATE(state, S_SELECTING);

	/* Cancel state_selecting and send_discover timeouts, since either
	   one could have got us here. */
	cancel_timeout (state_selecting, ip);
	cancel_timeout (send_discover, ip);

	/* We have received one or more DHCPOFFER packets.   Currently,
	   the only criterion by which we judge leases is whether or
	   not we get a response when we arp for them. */
	picked = (struct client_lease *)0;
	for (lp = ip -> client -> offered_leases; lp; lp = next) {
		next = lp -> next;

		/* Check to see if we got an ARPREPLY for the address
		   in this particular lease. */
		if (!picked) {
			script_init (ip, "ARPCHECK", lp -> medium);
			script_write_params (ip, "check_", lp);

			/* If the ARPCHECK code detects another
			   machine using the offered address, it exits
			   nonzero.  We need to send a DHCPDECLINE and
			   toss the lease. */
			if (script_go (ip)) {
				make_decline (ip, lp);
				send_decline (ip);
				goto freeit;
			}
			picked = lp;
			picked -> next = (struct client_lease *)0;
		} else {
		      freeit:
			free_client_lease (lp);
		}
	}
	ip -> client -> offered_leases = (struct client_lease *)0;

	/* If we just tossed all the leases we were offered, go back
	   to square one. */
	if (!picked) {
		ip -> client -> state = S_INIT;
		state_init (ip);
		return;
	}

	/* If it was a BOOTREPLY, we can just take the address right now. */
	if (!picked -> options [DHO_DHCP_MESSAGE_TYPE].len) {
		ip -> client -> new = picked;

		/* Make up some lease expiry times
		   XXX these should be configurable. */
		ip -> client -> new -> expiry = cur_time + 12000;
		ip -> client -> new -> renewal += cur_time + 8000;
		ip -> client -> new -> rebind += cur_time + 10000;

		ip -> client -> state = S_REQUESTING;

		/* Bind to the address we received. */
		bind_lease (ip);
		return;
	}

	/* Go to the REQUESTING state. */
	ip -> client -> destination = iaddr_broadcast;
	ip -> client -> state = S_REQUESTING;
	ip -> client -> first_sending = cur_time;
	ip -> client -> interval = ip -> client -> config -> initial_interval;

	/* Make a DHCPREQUEST packet from the lease we picked. */
	make_request (ip, picked);
	ip -> client -> xid = ip -> client -> packet.xid;

	/* Toss the lease we picked - we'll get it back in a DHCPACK. */
	free_client_lease (picked);

	/* Add an immediate timeout to send the first DHCPREQUEST packet. */
	send_request (ip);
}  

/* state_requesting is called when we receive a DHCPACK message after
   having sent out one or more DHCPREQUEST packets. */

void dhcpack (packet)
	struct packet *packet;
{
	struct interface_info *ip = packet -> interface;
	struct client_lease *lease;
	
	/* If we're not receptive to an offer right now, or if the offer
	   has an unrecognizable transaction id, then just drop it. */
	if (packet -> interface -> client -> xid != packet -> raw -> xid ||
	    (packet -> interface -> hw_address.hlen !=
	     packet -> raw -> hlen) ||
	    (memcmp (packet -> interface -> hw_address.haddr,
		     packet -> raw -> chaddr, packet -> raw -> hlen))) {
#if defined (DEBUG)
		debug ("DHCPACK in wrong transaction.");
#endif
		return;
	}

	if (ip -> client -> state != S_REBOOTING &&
	    ip -> client -> state != S_REQUESTING &&
	    ip -> client -> state != S_RENEWING &&
	    ip -> client -> state != S_REBINDING) {
#if defined (DEBUG)
		debug ("DHCPACK in wrong state.");
#endif
		return;
	}

	note ("DHCPACK from %s", piaddr (packet -> client_addr));

	lease = packet_to_lease (packet);
	if (!lease) {
		note ("packet_to_lease failed.");
		return;
	}

	ip -> client -> new = lease;

	/* Stop resending DHCPREQUEST. */
	cancel_timeout (send_request, ip);

	/* Figure out the lease time. */
	ip -> client -> new -> expiry =
		getULong (ip -> client ->
			  new -> options [DHO_DHCP_LEASE_TIME].data);
	/* A number that looks negative here is really just very large,
	   because the lease expiry offset is unsigned. */
	if (ip -> client -> new -> expiry < 0)
		ip -> client -> new -> expiry = TIME_MAX;

	/* Take the server-provided renewal time if there is one;
	   otherwise figure it out according to the spec. */
	if (ip -> client -> new -> options [DHO_DHCP_RENEWAL_TIME].len)
		ip -> client -> new -> renewal =
			getULong (ip -> client ->
				  new -> options [DHO_DHCP_RENEWAL_TIME].data);
	else
		ip -> client -> new -> renewal =
			ip -> client -> new -> expiry / 2;

	/* Same deal with the rebind time. */
	if (ip -> client -> new -> options [DHO_DHCP_REBINDING_TIME].len)
		ip -> client -> new -> rebind =
			getULong (ip -> client -> new ->
				  options [DHO_DHCP_REBINDING_TIME].data);
	else
		ip -> client -> new -> rebind =
			ip -> client -> new -> renewal +
				ip -> client -> new -> renewal / 2 +
					ip -> client -> new -> renewal / 4;

	ip -> client -> new -> expiry += cur_time;
	/* Lease lengths can never be negative. */
	if (ip -> client -> new -> expiry < cur_time)
		ip -> client -> new -> expiry = TIME_MAX;
	ip -> client -> new -> renewal += cur_time;
	if (ip -> client -> new -> renewal < cur_time)
		ip -> client -> new -> renewal = TIME_MAX;
	ip -> client -> new -> rebind += cur_time;
	if (ip -> client -> new -> rebind < cur_time)
		ip -> client -> new -> rebind = TIME_MAX;

	bind_lease (ip);
}

void bind_lease (ip)
	struct interface_info *ip;
{
	/* Remember the medium. */
	ip -> client -> new -> medium = ip -> client -> medium;

	/* Write out the new lease. */
	write_client_lease (ip, ip -> client -> new, 0);

	/* Run the client script with the new parameters. */
	script_init (ip, (ip -> client -> state == S_REQUESTING
			  ? "BOUND"
			  : (ip -> client -> state == S_RENEWING
			     ? "RENEW"
			     : (ip -> client -> state == S_REBOOTING
				? "REBOOT" : "REBIND"))),
		     ip -> client -> new -> medium);
	if (ip -> client -> active && ip -> client -> state != S_REBOOTING)
		script_write_params (ip, "old_", ip -> client -> active);
	script_write_params (ip, "new_", ip -> client -> new);
	if (ip -> client -> alias)
		script_write_params (ip, "alias_", ip -> client -> alias);
	script_go (ip);

	/* Replace the old active lease with the new one. */
	if (ip -> client -> active)
		free_client_lease (ip -> client -> active);
	ip -> client -> active = ip -> client -> new;
	ip -> client -> new = (struct client_lease *)0;

	/* Set up a timeout to start the renewal process. */
	add_timeout (ip -> client -> active -> renewal,
		     state_bound, ip);

	note ("bound to %s -- renewal in %d seconds.",
	      piaddr (ip -> client -> active -> address),
	      ip -> client -> active -> renewal - cur_time);
	ip -> client -> state = S_BOUND;
	reinitialize_interfaces ();
	go_daemon ();
}  

/* state_bound is called when we've successfully bound to a particular
   lease, but the renewal time on that lease has expired.   We are
   expected to unicast a DHCPREQUEST to the server that gave us our
   original lease. */

void state_bound (ipp)
	void *ipp;
{
	struct interface_info *ip = ipp;

	ASSERT_STATE(state, S_BOUND);

	/* T1 has expired. */
	make_request (ip, ip -> client -> active);
	ip -> client -> xid = ip -> client -> packet.xid;

	if (ip -> client -> active ->
	    options [DHO_DHCP_SERVER_IDENTIFIER].len == 4) {
		memcpy (ip -> client -> destination.iabuf,
			ip -> client -> active ->
			options [DHO_DHCP_SERVER_IDENTIFIER].data, 4);
		ip -> client -> destination.len = 4;
	} else
		ip -> client -> destination = iaddr_broadcast;

	ip -> client -> first_sending = cur_time;
	ip -> client -> interval = ip -> client -> config -> initial_interval;
	ip -> client -> state = S_RENEWING;

	/* Send the first packet immediately. */
	send_request (ip);
}  

int commit_leases ()
{
	return 0;
}

int write_lease (lease)
	struct lease *lease;
{
	return 0;
}

void db_startup ()
{
}

void bootp (packet)
	struct packet *packet;
{
	struct iaddrlist *ap;

	if (packet -> raw -> op != BOOTREPLY)
		return;

	/* If there's a reject list, make sure this packet's sender isn't
	   on it. */
	for (ap = packet -> interface -> client -> config -> reject_list;
	     ap; ap = ap -> next) {
		if (addr_eq (packet -> client_addr, ap -> addr)) {
			note ("BOOTREPLY from %s rejected.",
			      piaddr (ap -> addr));
			return;
		}
	}
	
	dhcpoffer (packet);

}

void dhcp (packet)
	struct packet *packet;
{
	struct iaddrlist *ap;
	void (*handler) PROTO ((struct packet *));
	char *type;

	switch (packet -> packet_type) {
	      case DHCPOFFER:
		handler = dhcpoffer;
		type = "DHCPOFFER";
		break;

	      case DHCPNAK:
		handler = dhcpnak;
		type = "DHCPNACK";
		break;

	      case DHCPACK:
		handler = dhcpack;
		type = "DHCPACK";
		break;

	      default:
		return;
	}

	/* If there's a reject list, make sure this packet's sender isn't
	   on it. */
	for (ap = packet -> interface -> client -> config -> reject_list;
	     ap; ap = ap -> next) {
		if (addr_eq (packet -> client_addr, ap -> addr)) {
			note ("%s from %s rejected.",
			      type, piaddr (ap -> addr));
			return;
		}
	}
	(*handler) (packet);
}

void dhcpoffer (packet)
	struct packet *packet;
{
	struct interface_info *ip = packet -> interface;
	struct client_lease *lease, *lp;
	int i;
	int arp_timeout_needed, stop_selecting;
	char *name = (packet -> options [DHO_DHCP_MESSAGE_TYPE].len
		      ? "DHCPOFFER" : "BOOTREPLY");
	
#ifdef DEBUG_PACKET
	dump_packet (packet);
#endif	

	/* If we're not receptive to an offer right now, or if the offer
	   has an unrecognizable transaction id, then just drop it. */
	if (ip -> client -> state != S_SELECTING ||
	    packet -> interface -> client -> xid != packet -> raw -> xid ||
	    (packet -> interface -> hw_address.hlen !=
	     packet -> raw -> hlen) ||
	    (memcmp (packet -> interface -> hw_address.haddr,
		     packet -> raw -> chaddr, packet -> raw -> hlen))) {
#if defined (DEBUG)
		debug ("%s in wrong transaction.", name);
#endif
		return;
	}

	note ("%s from %s", name, piaddr (packet -> client_addr));


	/* If this lease doesn't supply the minimum required parameters,
	   blow it off. */
	for (i = 0; ip -> client -> config -> required_options [i]; i++) {
		if (!packet -> options [ip -> client -> config ->
					required_options [i]].len) {
			note ("%s isn't satisfactory.", name);
			return;
		}
	}

	/* If we've already seen this lease, don't record it again. */
	for (lease = ip -> client -> offered_leases;
	     lease; lease = lease -> next) {
		if (lease -> address.len == sizeof packet -> raw -> yiaddr &&
		    !memcmp (lease -> address.iabuf,
			     &packet -> raw -> yiaddr, lease -> address.len)) {
			debug ("%s already seen.", name);
			return;
		}
	}

	lease = packet_to_lease (packet);
	if (!lease) {
		note ("packet_to_lease failed.");
		return;
	}

	/* If this lease was acquired through a BOOTREPLY, record that
	   fact. */
	if (!packet -> options [DHO_DHCP_MESSAGE_TYPE].len)
		lease -> is_bootp = 1;

	/* Record the medium under which this lease was offered. */
	lease -> medium = ip -> client -> medium;

	/* Send out an ARP Request for the offered IP address. */
	script_init (ip, "ARPSEND", lease -> medium);
	script_write_params (ip, "check_", lease);
	/* If the script can't send an ARP request without waiting, 
	   we'll be waiting when we do the ARPCHECK, so don't wait now. */
	if (script_go (ip))
		arp_timeout_needed = 0;
	else
		arp_timeout_needed = 2;

	/* Figure out when we're supposed to stop selecting. */
	stop_selecting = (ip -> client -> first_sending +
			  ip -> client -> config -> select_interval);

	/* If this is the lease we asked for, put it at the head of the
	   list, and don't mess with the arp request timeout. */
	if (lease -> address.len == ip -> client -> requested_address.len &&
	    !memcmp (lease -> address.iabuf,
		     ip -> client -> requested_address.iabuf,
		     ip -> client -> requested_address.len)) {
		lease -> next = ip -> client -> offered_leases;
		ip -> client -> offered_leases = lease;
	} else {
		/* If we already have an offer, and arping for this
		   offer would take us past the selection timeout,
		   then don't extend the timeout - just hope for the
		   best. */
		if (ip -> client -> offered_leases &&
		    (cur_time + arp_timeout_needed) > stop_selecting)
			arp_timeout_needed = 0;

		/* Put the lease at the end of the list. */
		lease -> next = (struct client_lease *)0;
		if (!ip -> client -> offered_leases)
			ip -> client -> offered_leases = lease;
		else {
			for (lp = ip -> client -> offered_leases; lp -> next;
			     lp = lp -> next)
				;
			lp -> next = lease;
		}
	}

	/* If we're supposed to stop selecting before we've had time
	   to wait for the ARPREPLY, add some delay to wait for
	   the ARPREPLY. */
	if (stop_selecting - cur_time < arp_timeout_needed)
		stop_selecting = cur_time + arp_timeout_needed;

	/* If the selecting interval has expired, go immediately to
	   state_selecting().  Otherwise, time out into
	   state_selecting at the select interval. */
	if (stop_selecting <= 0)
		state_selecting (ip);
	else {
		add_timeout (stop_selecting, state_selecting, ip);
		cancel_timeout (send_discover, ip);
	}
}

/* Allocate a client_lease structure and initialize it from the parameters
   in the specified packet. */

struct client_lease *packet_to_lease (packet)
	struct packet *packet;
{
	struct client_lease *lease;
	int i;

	lease = (struct client_lease *)malloc (sizeof (struct client_lease));

	if (!lease) {
		warn ("dhcpoffer: no memory to record lease.\n");
		return (struct client_lease *)0;
	}

	memset (lease, 0, sizeof *lease);

	/* Copy the lease options. */
	for (i = 0; i < 256; i++) {
		if (packet -> options [i].len) {
			lease -> options [i].data =
				(unsigned char *)
					malloc (packet -> options [i].len + 1);
			if (!lease -> options [i].data) {
				warn ("dhcpoffer: no memory for option %d\n",
				      i);
				free_client_lease (lease);
				return (struct client_lease *)0;
			} else {
				memcpy (lease -> options [i].data,
					packet -> options [i].data,
					packet -> options [i].len);
				lease -> options [i].len =
					packet -> options [i].len;
				lease -> options [i].data
					[lease -> options [i].len] = 0;
			}
		}
	}

	lease -> address.len = sizeof (packet -> raw -> yiaddr);
	memcpy (lease -> address.iabuf, &packet -> raw -> yiaddr,
		lease -> address.len);

	/* If the server name was filled out, copy it. */
	if ((!packet -> options [DHO_DHCP_OPTION_OVERLOAD].len ||
	     !(packet -> options [DHO_DHCP_OPTION_OVERLOAD].data [0] & 2)) &&
	    packet -> raw -> sname [0]) {
		int len;
		/* Don't count on the NUL terminator. */
		for (len = 0; len < 64; len++)
			if (!packet -> raw -> sname [len])
				break;
		lease -> server_name = malloc (len + 1);
		if (!lease -> server_name) {
			warn ("dhcpoffer: no memory for filename.\n");
			free_client_lease (lease);
			return (struct client_lease *)0;
		} else {
			memcpy (lease -> server_name,
				packet -> raw -> sname, len);
			lease -> server_name [len] = 0;
		}
	}

	/* Ditto for the filename. */
	if ((!packet -> options [DHO_DHCP_OPTION_OVERLOAD].len ||
	     !(packet -> options [DHO_DHCP_OPTION_OVERLOAD].data [0] & 1)) &&
	    packet -> raw -> file [0]) {
		int len;
		/* Don't count on the NUL terminator. */
		for (len = 0; len < 64; len++)
			if (!packet -> raw -> file [len])
				break;
		lease -> filename = malloc (len + 1);
		if (!lease -> filename) {
			warn ("dhcpoffer: no memory for filename.\n");
			free_client_lease (lease);
			return (struct client_lease *)0;
		} else {
			memcpy (lease -> filename,
				packet -> raw -> file, len);
			lease -> filename [len] = 0;
		}
	}
	return lease;
}	

void dhcpnak (packet)
	struct packet *packet;
{
	struct interface_info *ip = packet -> interface;

	/* If we're not receptive to an offer right now, or if the offer
	   has an unrecognizable transaction id, then just drop it. */
	if (packet -> interface -> client -> xid != packet -> raw -> xid ||
	    (packet -> interface -> hw_address.hlen !=
	     packet -> raw -> hlen) ||
	    (memcmp (packet -> interface -> hw_address.haddr,
		     packet -> raw -> chaddr, packet -> raw -> hlen))) {
#if defined (DEBUG)
		debug ("DHCPNAK in wrong transaction.");
#endif
		return;
	}

	if (ip -> client -> state != S_REBOOTING &&
	    ip -> client -> state != S_REQUESTING &&
	    ip -> client -> state != S_RENEWING &&
	    ip -> client -> state != S_REBINDING) {
#if defined (DEBUG)
		debug ("DHCPNAK in wrong state.");
#endif
		return;
	}

	note ("DHCPNAK from %s", piaddr (packet -> client_addr));

	if (!ip -> client -> active) {
		note ("DHCPNAK with no active lease.\n");
		return;
	}

	free_client_lease (ip -> client -> active);
	ip -> client -> active = (struct client_lease *)0;

	/* Stop sending DHCPREQUEST packets... */
	cancel_timeout (send_request, ip);

	ip -> client -> state = S_INIT;
	state_init (ip);
}

/* Send out a DHCPDISCOVER packet, and set a timeout to send out another
   one after the right interval has expired.  If we don't get an offer by
   the time we reach the panic interval, call the panic function. */

void send_discover (ipp)
	void *ipp;
{
	struct interface_info *ip = ipp;

	int result;
	int interval;
	int increase = 1;

	/* Figure out how long it's been since we started transmitting. */
	interval = cur_time - ip -> client -> first_sending;

	/* If we're past the panic timeout, call the script and tell it
	   we haven't found anything for this interface yet. */
	if (interval > ip -> client -> config -> timeout) {
		state_panic (ip);
		return;
	}

	/* If we're selecting media, try the whole list before doing
	   the exponential backoff, but if we've already received an
	   offer, stop looping, because we obviously have it right. */
	if (!ip -> client -> offered_leases &&
	    ip -> client -> config -> media) {
		int fail = 0;
	      again:
		if (ip -> client -> medium) {
			ip -> client -> medium =
				ip -> client -> medium -> next;
			increase = 0;
		} 
		if (!ip -> client -> medium) {
			if (fail)
				error ("No valid media types for %s!",
				       ip -> name);
			ip -> client -> medium =
				ip -> client -> config -> media;
			increase = 1;
		}
			
		note ("Trying medium \"%s\" %d",
		      ip -> client -> medium -> string, increase);
		script_init (ip, "MEDIUM", ip -> client -> medium);
		if (script_go (ip)) {
			goto again;
		}
	}

	/* If we're supposed to increase the interval, do so.  If it's
	   currently zero (i.e., we haven't sent any packets yet), set
	   it to one; otherwise, add to it a random number between
	   zero and two times itself.  On average, this means that it
	   will double with every transmission. */
	if (increase) {
		if (!ip -> client -> interval)
			ip -> client -> interval =
				ip -> client -> config -> initial_interval;
		else {
			ip -> client -> interval +=
				((random () >> 2) %
				 (2 * ip -> client -> interval));
		}

		/* Don't backoff past cutoff. */
		if (ip -> client -> interval >
		    ip -> client -> config -> backoff_cutoff)
			ip -> client -> interval =
				((ip -> client -> config -> backoff_cutoff / 2)
				 + ((random () >> 2) %
				    ip -> client -> config -> backoff_cutoff));
	} else if (!ip -> client -> interval)
		ip -> client -> interval =
			ip -> client -> config -> initial_interval;
		
	/* If the backoff would take us to the panic timeout, just use that
	   as the interval. */
	if (cur_time + ip -> client -> interval >
	    ip -> client -> first_sending + ip -> client -> config -> timeout)
		ip -> client -> interval =
			(ip -> client -> first_sending +
			 ip -> client -> config -> timeout) - cur_time + 1;

	/* Record the number of seconds since we started sending. */
	if (interval < 65536)
		ip -> client -> packet.secs = htons (interval);
	else
		ip -> client -> packet.secs = htons (65535);
	ip -> client -> secs = ip -> client -> packet.secs;

	note ("DHCPDISCOVER on %s to %s port %d interval %ld",
	      ip -> name,
	      inet_ntoa (sockaddr_broadcast.sin_addr),
	      ntohs (sockaddr_broadcast.sin_port), ip -> client -> interval);

	/* Send out a packet. */
	result = send_packet (ip, (struct packet *)0,
			      &ip -> client -> packet,
			      ip -> client -> packet_length,
			      inaddr_any, &sockaddr_broadcast,
			      (struct hardware *)0);

	add_timeout (cur_time + ip -> client -> interval, send_discover, ip);
}

/* state_panic gets called if we haven't received any offers in a preset
   amount of time.   When this happens, we try to use existing leases that
   haven't yet expired, and failing that, we call the client script and
   hope it can do something. */

void state_panic (ipp)
	void *ipp;
{
	struct interface_info *ip = ipp;

	struct client_lease *loop = ip -> client -> active;
	struct client_lease *lp;

	note ("No DHCPOFFERS received.");

	/* We may not have an active lease, but we may have some
	   predefined leases that we can try. */
	if (!ip -> client -> active && ip -> client -> leases)
		goto activate_next;

	/* Run through the list of leases and see if one can be used. */
	while (ip -> client -> active) {
		if (ip -> client -> active -> expiry > cur_time) {
			note ("Trying recorded lease %s",
			      piaddr (ip -> client -> active -> address));
			/* Run the client script with the existing
			   parameters. */
			script_init (ip, "TIMEOUT",
				     ip -> client -> active -> medium);
			script_write_params (ip, "new_",
					     ip -> client -> active);
			if (ip -> client -> alias)
				script_write_params (ip, "alias_",
						     ip -> client -> alias);

			/* If the old lease is still good and doesn't
			   yet need renewal, go into BOUND state and
			   timeout at the renewal time. */
			if (!script_go (ip)) {
				if (cur_time <
				    ip -> client -> active -> renewal) {
					ip -> client -> state = S_BOUND;
					note ("bound: renewal in %d seconds.",
					      ip -> client -> active -> renewal
					      - cur_time);
					add_timeout ((ip -> client ->
						      active -> renewal),
						     state_bound, ip);
				} else {
					ip -> client -> state = S_BOUND;
					note ("bound: immediate renewal.");
					state_bound (ip);
				}
				reinitialize_interfaces ();
				go_daemon ();
				return;
			}
		}

		/* If there are no other leases, give up. */
		if (!ip -> client -> leases) {
			ip -> client -> leases = ip -> client -> active;
			ip -> client -> active = (struct client_lease *)0;
			break;
		}

	activate_next:
		/* Otherwise, put the active lease at the end of the
		   lease list, and try another lease.. */
		for (lp = ip -> client -> leases; lp -> next; lp = lp -> next)
			;
		lp -> next = ip -> client -> active;
		if (lp -> next) {
			lp -> next -> next = (struct client_lease *)0;
		}
		ip -> client -> active = ip -> client -> leases;
		ip -> client -> leases = ip -> client -> leases -> next;

		/* If we already tried this lease, we've exhausted the
		   set of leases, so we might as well give up for
		   now. */
		if (ip -> client -> active == loop)
			break;
		else if (!loop)
			loop = ip -> client -> active;
	}

	/* No leases were available, or what was available didn't work, so
	   tell the shell script that we failed to allocate an address,
	   and try again later. */
	if (onetry) {
		exit(2);
		note ("Unable to obtain a lease on first try - exiting.\n");
	}
	note ("No working leases in persistent database - sleeping.\n");
	script_init (ip, "FAIL", (struct string_list *)0);
	if (ip -> client -> alias)
		script_write_params (ip, "alias_", ip -> client -> alias);
	script_go (ip);
	ip -> client -> state = S_INIT;
	add_timeout (cur_time + ip -> client -> config -> retry_interval,
		     state_init, ip);
	go_daemon ();
}

void send_request (ipp)
	void *ipp;
{
	struct interface_info *ip = ipp;

	int result;
	int interval;
	struct sockaddr_in destination;
	struct in_addr from;

	/* Figure out how long it's been since we started transmitting. */
	interval = cur_time - ip -> client -> first_sending;

	/* If we're in the INIT-REBOOT or REQUESTING state and we're
	   past the reboot timeout, go to INIT and see if we can
	   DISCOVER an address... */
	/* XXX In the INIT-REBOOT state, if we don't get an ACK, it
	   means either that we're on a network with no DHCP server,
	   or that our server is down.  In the latter case, assuming
	   that there is a backup DHCP server, DHCPDISCOVER will get
	   us a new address, but we could also have successfully
	   reused our old address.  In the former case, we're hosed
	   anyway.  This is not a win-prone situation. */
	if ((ip -> client -> state == S_REBOOTING ||
	     ip -> client -> state == S_REQUESTING) &&
	    interval > ip -> client -> config -> reboot_timeout) {
	cancel:
		ip -> client -> state = S_INIT;
		cancel_timeout (send_request, ip);
		state_init (ip);
		return;
	}

	/* If we're in the reboot state, make sure the media is set up
	   correctly. */
	if (ip -> client -> state == S_REBOOTING &&
	    !ip -> client -> medium &&
	    ip -> client -> active -> medium ) {
		script_init (ip, "MEDIUM", ip -> client -> active -> medium);

		/* If the medium we chose won't fly, go to INIT state. */
		if (script_go (ip))
			goto cancel;

		/* Record the medium. */
		ip -> client -> medium = ip -> client -> active -> medium;
	}

	/* If the lease has expired, relinquish the address and go back
	   to the INIT state. */
	if (ip -> client -> state != S_REQUESTING &&
	    cur_time > ip -> client -> active -> expiry) {
		/* Run the client script with the new parameters. */
		script_init (ip, "EXPIRE", (struct string_list *)0);
		script_write_params (ip, "old_", ip -> client -> active);
		if (ip -> client -> alias)
			script_write_params (ip, "alias_",
					     ip -> client -> alias);
		script_go (ip);

		/* Now do a preinit on the interface so that we can
		   discover a new address. */
		script_init (ip, "PREINIT", (struct string_list *)0);
		if (ip -> client -> alias)
			script_write_params (ip, "alias_",
					     ip -> client -> alias);
		script_go (ip);

		ip -> client -> state = S_INIT;
		state_init (ip);
		return;
	}

	/* Do the exponential backoff... */
	if (!ip -> client -> interval)
		ip -> client -> interval =
			ip -> client -> config -> initial_interval;
	else {
		ip -> client -> interval +=
			((random () >> 2) %
			 (2 * ip -> client -> interval));
	}
	
	/* Don't backoff past cutoff. */
	if (ip -> client -> interval >
	    ip -> client -> config -> backoff_cutoff)
		ip -> client -> interval =
			((ip -> client -> config -> backoff_cutoff / 2)
			 + ((random () >> 2)
			    % ip -> client -> interval));

	/* If the backoff would take us to the expiry time, just set the
	   timeout to the expiry time. */
	if (ip -> client -> state != S_REQUESTING &&
	    cur_time + ip -> client -> interval >
	    ip -> client -> active -> expiry)
		ip -> client -> interval =
			ip -> client -> active -> expiry - cur_time + 1;

	/* If the lease T2 time has elapsed, or if we're not yet bound,
	   broadcast the DHCPREQUEST rather than unicasting. */
	if (ip -> client -> state == S_REQUESTING ||
	    ip -> client -> state == S_REBOOTING ||
	    cur_time > ip -> client -> active -> rebind)
		destination.sin_addr.s_addr = INADDR_BROADCAST;
	else
		memcpy (&destination.sin_addr.s_addr,
			ip -> client -> destination.iabuf,
			sizeof destination.sin_addr.s_addr);
	destination.sin_port = remote_port;
	destination.sin_family = AF_INET;
#ifdef HAVE_SA_LEN
	destination.sin_len = sizeof destination;
#endif

	if (ip -> client -> state != S_REQUESTING)
		memcpy (&from, ip -> client -> active -> address.iabuf,
			sizeof from);
	else
		from.s_addr = INADDR_ANY;

	/* Record the number of seconds since we started sending. */
	if (ip -> client -> state == S_REQUESTING)
		ip -> client -> packet.secs = ip -> client -> secs;
	else {
		if (interval < 65536)
			ip -> client -> packet.secs = htons (interval);
		else
			ip -> client -> packet.secs = htons (65535);
	}

	note ("DHCPREQUEST on %s to %s port %d", ip -> name,
	      inet_ntoa (destination.sin_addr),
	      ntohs (destination.sin_port));

	if (destination.sin_addr.s_addr != INADDR_BROADCAST &&
	    fallback_interface)
		result = send_packet (fallback_interface,
				      (struct packet *)0,
				      &ip -> client -> packet,
				      ip -> client -> packet_length,
				      from, &destination,
				      (struct hardware *)0);
	else
		/* Send out a packet. */
		result = send_packet (ip, (struct packet *)0,
				      &ip -> client -> packet,
				      ip -> client -> packet_length,
				      from, &destination,
				      (struct hardware *)0);

	add_timeout (cur_time + ip -> client -> interval,
		     send_request, ip);
}

void send_decline (ipp)
	void *ipp;
{
	struct interface_info *ip = ipp;

	int result;

	note ("DHCPDECLINE on %s to %s port %d", ip -> name,
	      inet_ntoa (sockaddr_broadcast.sin_addr),
	      ntohs (sockaddr_broadcast.sin_port));

	/* Send out a packet. */
	result = send_packet (ip, (struct packet *)0,
			      &ip -> client -> packet,
			      ip -> client -> packet_length,
			      inaddr_any, &sockaddr_broadcast,
			      (struct hardware *)0);
}

void send_release (ipp)
	void *ipp;
{
	struct interface_info *ip = ipp;

	int result;

	note ("DHCPRELEASE on %s to %s port %d", ip -> name,
	      inet_ntoa (sockaddr_broadcast.sin_addr),
	      ntohs (sockaddr_broadcast.sin_port));

	/* Send out a packet. */
	result = send_packet (ip, (struct packet *)0,
			      &ip -> client -> packet,
			      ip -> client -> packet_length,
			      inaddr_any, &sockaddr_broadcast,
			      (struct hardware *)0);
}

void make_discover (ip, lease)
	struct interface_info *ip;
	struct client_lease *lease;
{
	unsigned char discover = DHCPDISCOVER;
	int i;

	struct tree_cache *options [256];
	struct tree_cache option_elements [256];

	memset (option_elements, 0, sizeof option_elements);
	memset (options, 0, sizeof options);
	memset (&ip -> client -> packet, 0, sizeof (ip -> client -> packet));

	/* Set DHCP_MESSAGE_TYPE to DHCPDISCOVER */
	i = DHO_DHCP_MESSAGE_TYPE;
	options [i] = &option_elements [i];
	options [i] -> value = &discover;
	options [i] -> len = sizeof discover;
	options [i] -> buf_size = sizeof discover;
	options [i] -> timeout = 0xFFFFFFFF;
	options [i] -> tree = (struct tree *)0;

	/* Request the options we want */
	i  = DHO_DHCP_PARAMETER_REQUEST_LIST;
	options [i] = &option_elements [i];
	options [i] -> value = ip -> client -> config -> requested_options;
	options [i] -> len = ip -> client -> config -> requested_option_count;
	options [i] -> buf_size =
		ip -> client -> config -> requested_option_count;
	options [i] -> timeout = 0xFFFFFFFF;
	options [i] -> tree = (struct tree *)0;

	/* If we had an address, try to get it again. */
	if (lease) {
		ip -> client -> requested_address = lease -> address;
		i = DHO_DHCP_REQUESTED_ADDRESS;
		options [i] = &option_elements [i];
		options [i] -> value = lease -> address.iabuf;
		options [i] -> len = lease -> address.len;
		options [i] -> buf_size = lease -> address.len;
		options [i] -> timeout = 0xFFFFFFFF;
		options [i] -> tree = (struct tree *)0;
	} else {
		ip -> client -> requested_address.len = 0;
	}

	/* Send any options requested in the config file. */
	for (i = 0; i < 256; i++) {
		if (!options [i] &&
		    ip -> client -> config -> send_options [i].data) {
			options [i] = &option_elements [i];
			options [i] -> value = ip -> client -> config ->
				send_options [i].data;
			options [i] -> len = ip -> client -> config ->
				send_options [i].len;
			options [i] -> buf_size = ip -> client -> config ->
				send_options [i].len;
			options [i] -> timeout = 0xFFFFFFFF;
			options [i] -> tree = (struct tree *)0;
		}
	}

	/* Set up the option buffer... */
	ip -> client -> packet_length =
		cons_options ((struct packet *)0, &ip -> client -> packet, 0,
			      options, 0, 0, 0, (u_int8_t *)0, 0);
	if (ip -> client -> packet_length < BOOTP_MIN_LEN)
		ip -> client -> packet_length = BOOTP_MIN_LEN;

	ip -> client -> packet.op = BOOTREQUEST;
	ip -> client -> packet.htype = ip -> hw_address.htype;
	ip -> client -> packet.hlen = ip -> hw_address.hlen;
	ip -> client -> packet.hops = 0;
	ip -> client -> packet.xid = random ();
	ip -> client -> packet.secs = 0; /* filled in by send_discover. */

	if (can_receive_unicast_unconfigured (ip))
		ip -> client -> packet.flags = 0;
	else
		ip -> client -> packet.flags = htons (BOOTP_BROADCAST);

	memset (&(ip -> client -> packet.ciaddr),
		0, sizeof ip -> client -> packet.ciaddr);
	memset (&(ip -> client -> packet.yiaddr),
		0, sizeof ip -> client -> packet.yiaddr);
	memset (&(ip -> client -> packet.siaddr),
		0, sizeof ip -> client -> packet.siaddr);
	memset (&(ip -> client -> packet.giaddr),
		0, sizeof ip -> client -> packet.giaddr);
	memcpy (ip -> client -> packet.chaddr,
		ip -> hw_address.haddr, ip -> hw_address.hlen);

#ifdef DEBUG_PACKET
	dump_packet (sendpkt);
	dump_raw ((unsigned char *)ip -> client -> packet,
		  sendpkt->packet_length);
#endif
}


void make_request (ip, lease)
	struct interface_info *ip;
	struct client_lease *lease;
{
	unsigned char request = DHCPREQUEST;
	int i;

	struct tree_cache *options [256];
	struct tree_cache option_elements [256];

	memset (options, 0, sizeof options);
	memset (&ip -> client -> packet, 0, sizeof (ip -> client -> packet));

	/* Set DHCP_MESSAGE_TYPE to DHCPREQUEST */
	i = DHO_DHCP_MESSAGE_TYPE;
	options [i] = &option_elements [i];
	options [i] -> value = &request;
	options [i] -> len = sizeof request;
	options [i] -> buf_size = sizeof request;
	options [i] -> timeout = 0xFFFFFFFF;
	options [i] -> tree = (struct tree *)0;

	/* Request the options we want */
	i = DHO_DHCP_PARAMETER_REQUEST_LIST;
	options [i] = &option_elements [i];
	options [i] -> value = ip -> client -> config -> requested_options;
	options [i] -> len = ip -> client -> config -> requested_option_count;
	options [i] -> buf_size =
		ip -> client -> config -> requested_option_count;
	options [i] -> timeout = 0xFFFFFFFF;
	options [i] -> tree = (struct tree *)0;

	/* If we are requesting an address that hasn't yet been assigned
	   to us, use the DHCP Requested Address option. */
	if (ip -> client -> state == S_REQUESTING) {
		/* Send back the server identifier... */
		i = DHO_DHCP_SERVER_IDENTIFIER;
		options [i] = &option_elements [i];
		options [i] -> value = lease -> options [i].data;
		options [i] -> len = lease -> options [i].len;
		options [i] -> buf_size = lease -> options [i].len;
		options [i] -> timeout = 0xFFFFFFFF;
		options [i] -> tree = (struct tree *)0;
	}
	if (ip -> client -> state == S_REQUESTING ||
	    ip -> client -> state == S_REBOOTING) {
		ip -> client -> requested_address = lease -> address;
		i = DHO_DHCP_REQUESTED_ADDRESS;
		options [i] = &option_elements [i];
		options [i] -> value = lease -> address.iabuf;
		options [i] -> len = lease -> address.len;
		options [i] -> buf_size = lease -> address.len;
		options [i] -> timeout = 0xFFFFFFFF;
		options [i] -> tree = (struct tree *)0;
	} else {
		ip -> client -> requested_address.len = 0;
	}

	/* Send any options requested in the config file. */
	for (i = 0; i < 256; i++) {
		if (!options [i] &&
		    ip -> client -> config -> send_options [i].data) {
			options [i] = &option_elements [i];
			options [i] -> value = ip -> client -> config ->
				send_options [i].data;
			options [i] -> len = ip -> client -> config ->
				send_options [i].len;
			options [i] -> buf_size = ip -> client -> config ->
				send_options [i].len;
			options [i] -> timeout = 0xFFFFFFFF;
			options [i] -> tree = (struct tree *)0;
		}
	}

	/* Set up the option buffer... */
	ip -> client -> packet_length =
		cons_options ((struct packet *)0, &ip -> client -> packet, 0,
			      options, 0, 0, 0, (u_int8_t *)0, 0);
	if (ip -> client -> packet_length < BOOTP_MIN_LEN)
		ip -> client -> packet_length = BOOTP_MIN_LEN;

	ip -> client -> packet.op = BOOTREQUEST;
	ip -> client -> packet.htype = ip -> hw_address.htype;
	ip -> client -> packet.hlen = ip -> hw_address.hlen;
	ip -> client -> packet.hops = 0;
	ip -> client -> packet.xid = ip -> client -> xid;
	ip -> client -> packet.secs = 0; /* Filled in by send_request. */

	/* If we own the address we're requesting, put it in ciaddr;
	   otherwise set ciaddr to zero. */
	if (ip -> client -> state == S_BOUND ||
	    ip -> client -> state == S_RENEWING ||
	    ip -> client -> state == S_REBINDING) {
		memcpy (&ip -> client -> packet.ciaddr,
			lease -> address.iabuf, lease -> address.len);
		ip -> client -> packet.flags = 0;
	} else {
		memset (&ip -> client -> packet.ciaddr, 0,
			sizeof ip -> client -> packet.ciaddr);
		if (can_receive_unicast_unconfigured (ip))
			ip -> client -> packet.flags = 0;
		else
			ip -> client -> packet.flags = htons (BOOTP_BROADCAST);
	}

	memset (&ip -> client -> packet.yiaddr, 0,
		sizeof ip -> client -> packet.yiaddr);
	memset (&ip -> client -> packet.siaddr, 0,
		sizeof ip -> client -> packet.siaddr);
	memset (&ip -> client -> packet.giaddr, 0,
		sizeof ip -> client -> packet.giaddr);
	memcpy (ip -> client -> packet.chaddr,
		ip -> hw_address.haddr, ip -> hw_address.hlen);

#ifdef DEBUG_PACKET
	dump_packet (sendpkt);
	dump_raw ((unsigned char *)ip -> client -> packet, sendpkt->packet_length);
#endif
}

void make_decline (ip, lease)
	struct interface_info *ip;
	struct client_lease *lease;
{
	unsigned char decline = DHCPDECLINE;
	int i;

	struct tree_cache *options [256];
	struct tree_cache message_type_tree;
	struct tree_cache requested_address_tree;
	struct tree_cache server_id_tree;
	struct tree_cache client_id_tree;

	memset (options, 0, sizeof options);
	memset (&ip -> client -> packet, 0, sizeof (ip -> client -> packet));

	/* Set DHCP_MESSAGE_TYPE to DHCPDECLINE */
	i = DHO_DHCP_MESSAGE_TYPE;
	options [i] = &message_type_tree;
	options [i] -> value = &decline;
	options [i] -> len = sizeof decline;
	options [i] -> buf_size = sizeof decline;
	options [i] -> timeout = 0xFFFFFFFF;
	options [i] -> tree = (struct tree *)0;

	/* Send back the server identifier... */
	i = DHO_DHCP_SERVER_IDENTIFIER;
        options [i] = &server_id_tree;
        options [i] -> value = lease -> options [i].data;
        options [i] -> len = lease -> options [i].len;
        options [i] -> buf_size = lease -> options [i].len;
        options [i] -> timeout = 0xFFFFFFFF;
        options [i] -> tree = (struct tree *)0;

	/* Send back the address we're declining. */
	i = DHO_DHCP_REQUESTED_ADDRESS;
	options [i] = &requested_address_tree;
	options [i] -> value = lease -> address.iabuf;
	options [i] -> len = lease -> address.len;
	options [i] -> buf_size = lease -> address.len;
	options [i] -> timeout = 0xFFFFFFFF;
	options [i] -> tree = (struct tree *)0;

	/* Send the uid if the user supplied one. */
	i = DHO_DHCP_CLIENT_IDENTIFIER;
	if (ip -> client -> config -> send_options [i].len) {
		options [i] = &client_id_tree;
		options [i] -> value = ip -> client -> config ->
			send_options [i].data;
		options [i] -> len = ip -> client -> config ->
			send_options [i].len;
		options [i] -> buf_size = ip -> client -> config ->
			send_options [i].len;
		options [i] -> timeout = 0xFFFFFFFF;
		options [i] -> tree = (struct tree *)0;
	}


	/* Set up the option buffer... */
	ip -> client -> packet_length =
		cons_options ((struct packet *)0, &ip -> client -> packet, 0,
			      options, 0, 0, 0, (u_int8_t *)0, 0);
	if (ip -> client -> packet_length < BOOTP_MIN_LEN)
		ip -> client -> packet_length = BOOTP_MIN_LEN;

	ip -> client -> packet.op = BOOTREQUEST;
	ip -> client -> packet.htype = ip -> hw_address.htype;
	ip -> client -> packet.hlen = ip -> hw_address.hlen;
	ip -> client -> packet.hops = 0;
	ip -> client -> packet.xid = ip -> client -> xid;
	ip -> client -> packet.secs = 0; /* Filled in by send_request. */
	ip -> client -> packet.flags = 0;

	/* ciaddr must always be zero. */
	memset (&ip -> client -> packet.ciaddr, 0,
		sizeof ip -> client -> packet.ciaddr);
	memset (&ip -> client -> packet.yiaddr, 0,
		sizeof ip -> client -> packet.yiaddr);
	memset (&ip -> client -> packet.siaddr, 0,
		sizeof ip -> client -> packet.siaddr);
	memset (&ip -> client -> packet.giaddr, 0,
		sizeof ip -> client -> packet.giaddr);
	memcpy (ip -> client -> packet.chaddr,
		ip -> hw_address.haddr, ip -> hw_address.hlen);

#ifdef DEBUG_PACKET
	dump_packet (sendpkt);
	dump_raw ((unsigned char *)ip -> client -> packet, sendpkt->packet_length);
#endif
}

void make_release (ip, lease)
	struct interface_info *ip;
	struct client_lease *lease;
{
	unsigned char request = DHCPRELEASE;
	int i;

	struct tree_cache *options [256];
	struct tree_cache message_type_tree;
	struct tree_cache server_id_tree;

	memset (options, 0, sizeof options);
	memset (&ip -> client -> packet, 0, sizeof (ip -> client -> packet));

	/* Set DHCP_MESSAGE_TYPE to DHCPRELEASE */
	i = DHO_DHCP_MESSAGE_TYPE;
	options [i] = &message_type_tree;
	options [i] -> value = &request;
	options [i] -> len = sizeof request;
	options [i] -> buf_size = sizeof request;
	options [i] -> timeout = 0xFFFFFFFF;
	options [i] -> tree = (struct tree *)0;

	/* Send back the server identifier... */
	i = DHO_DHCP_SERVER_IDENTIFIER;
        options [i] = &server_id_tree;
        options [i] -> value = lease -> options [i].data;
        options [i] -> len = lease -> options [i].len;
        options [i] -> buf_size = lease -> options [i].len;
        options [i] -> timeout = 0xFFFFFFFF;
        options [i] -> tree = (struct tree *)0;

	/* Set up the option buffer... */
	ip -> client -> packet_length =
		cons_options ((struct packet *)0, &ip -> client -> packet, 0,
			      options, 0, 0, 0, (u_int8_t *)0, 0);
	if (ip -> client -> packet_length < BOOTP_MIN_LEN)
		ip -> client -> packet_length = BOOTP_MIN_LEN;

	ip -> client -> packet.op = BOOTREQUEST;
	ip -> client -> packet.htype = ip -> hw_address.htype;
	ip -> client -> packet.hlen = ip -> hw_address.hlen;
	ip -> client -> packet.hops = 0;
	ip -> client -> packet.xid = random ();
	ip -> client -> packet.secs = 0;
	ip -> client -> packet.flags = 0;

	memset (&ip -> client -> packet.ciaddr, 0,
		sizeof ip -> client -> packet.ciaddr);
	memset (&ip -> client -> packet.yiaddr, 0,
		sizeof ip -> client -> packet.yiaddr);
	memset (&ip -> client -> packet.siaddr, 0,
		sizeof ip -> client -> packet.siaddr);
	memset (&ip -> client -> packet.giaddr, 0,
		sizeof ip -> client -> packet.giaddr);
	memcpy (ip -> client -> packet.chaddr,
		ip -> hw_address.haddr, ip -> hw_address.hlen);

#ifdef DEBUG_PACKET
	dump_packet (sendpkt);
	dump_raw ((unsigned char *)ip -> client -> packet,
		  ip -> client -> packet_length);
#endif
}

void free_client_lease (lease)
	struct client_lease *lease;
{
	int i;

	if (lease -> server_name)
		free (lease -> server_name);
	if (lease -> filename)
		free (lease -> filename);
	for (i = 0; i < 256; i++) {
		if (lease -> options [i].len)
			free (lease -> options [i].data);
	}
	free (lease);
}

FILE *leaseFile;

void rewrite_client_leases ()
{
	struct interface_info *ip;
	struct client_lease *lp;

	if (leaseFile)
		fclose (leaseFile);
	leaseFile = fopen (path_dhclient_db, "w");
	if (!leaseFile)
		error ("can't create %s: %m", path_dhclient_db);

	/* Write out all the leases attached to configured interfaces that
	   we know about. */
	for (ip = interfaces; ip; ip = ip -> next) {
		for (lp = ip -> client -> leases; lp; lp = lp -> next) {
			write_client_lease (ip, lp, 1);
		}
		if (ip -> client -> active)
			write_client_lease (ip, ip -> client -> active, 1);
	}

	/* Write out any leases that are attached to interfaces that aren't
	   currently configured. */
	for (ip = dummy_interfaces; ip; ip = ip -> next) {
		for (lp = ip -> client -> leases; lp; lp = lp -> next) {
			write_client_lease (ip, lp, 1);
		}
		if (ip -> client -> active)
			write_client_lease (ip, ip -> client -> active, 1);
	}
	fflush (leaseFile);
}

void write_client_lease (ip, lease, rewrite)
	struct interface_info *ip;
	struct client_lease *lease;
	int rewrite;
{
	int i;
	struct tm *t;
	static int leases_written;

	if (!rewrite) {
		if (leases_written++ > 20) {
			rewrite_client_leases ();
			leases_written = 0;
		}
	}

	/* If the lease came from the config file, we don't need to stash
	   a copy in the lease database. */
	if (lease -> is_static)
		return;

	if (!leaseFile) {	/* XXX */
		leaseFile = fopen (path_dhclient_db, "w");
		if (!leaseFile)
			error ("can't create %s: %m", path_dhclient_db);
	}

	fprintf (leaseFile, "lease {\n");
	if (lease -> is_bootp)
		fprintf (leaseFile, "  bootp;\n");
	fprintf (leaseFile, "  interface \"%s\";\n", ip -> name);
	fprintf (leaseFile, "  fixed-address %s;\n",
		 piaddr (lease -> address));
	if (lease -> filename)
		fprintf (leaseFile, "  filename \"%s\";\n",
			 lease -> filename);
	if (lease -> server_name)
		fprintf (leaseFile, "  server-name \"%s\";\n",
			 lease -> server_name);
	if (lease -> medium)
		fprintf (leaseFile, "  medium \"%s\";\n",
			 lease -> medium -> string);
	for (i = 0; i < 256; i++) {
		if (lease -> options [i].len) {
			fprintf (leaseFile,
				 "  option %s %s;\n",
				 dhcp_options [i].name,
				 pretty_print_option
				 (i, lease -> options [i].data,
				  lease -> options [i].len, 1, 1));
		}
	}

	/* Note: the following is not a Y2K bug - it's a Y1.9K bug.   Until
	   somebody invents a time machine, I think we can safely disregard
	   it. */
	t = gmtime (&lease -> renewal);
	fprintf (leaseFile,
		 "  renew %d %d/%d/%d %02d:%02d:%02d;\n",
		 t -> tm_wday, t -> tm_year + 1900,
		 t -> tm_mon + 1, t -> tm_mday,
		 t -> tm_hour, t -> tm_min, t -> tm_sec);
	t = gmtime (&lease -> rebind);
	fprintf (leaseFile,
		 "  rebind %d %d/%d/%d %02d:%02d:%02d;\n",
		 t -> tm_wday, t -> tm_year + 1900,
		 t -> tm_mon + 1, t -> tm_mday,
		 t -> tm_hour, t -> tm_min, t -> tm_sec);
	t = gmtime (&lease -> expiry);
	fprintf (leaseFile,
		 "  expire %d %d/%d/%d %02d:%02d:%02d;\n",
		 t -> tm_wday, t -> tm_year + 1900,
		 t -> tm_mon + 1, t -> tm_mday,
		 t -> tm_hour, t -> tm_min, t -> tm_sec);
	fprintf (leaseFile, "}\n");
	fflush (leaseFile);
}

/* Variables holding name of script and file pointer for writing to
   script.   Needless to say, this is not reentrant - only one script
   can be invoked at a time. */
char scriptName [256];
FILE *scriptFile;

void script_init (ip, reason, medium)
	struct interface_info *ip;
	char *reason;
	struct string_list *medium;
{
	struct string_list *sl, *next;

	if (ip) {
		for (sl = ip -> client -> env; sl; sl = next) {
			next = sl -> next;
			dfree (sl, "script_init");
		}
		ip -> client -> env = (struct string_list *)0;
		ip -> client -> envc = 0;
		
		client_envadd (ip -> client, "", "interface", "%s",
			       ip -> name);
		if (medium)
			client_envadd (ip -> client,
				       "", "medium", "%s", medium -> string);

		client_envadd (ip -> client, "", "reason", "%s", reason);
	}
}

void script_write_params (ip, prefix, lease)
	struct interface_info *ip;
	char *prefix;
	struct client_lease *lease;
{
	int i;
	u_int8_t dbuf [1500];
	int len;

	client_envadd (ip -> client,
		       prefix, "ip_address", "%s", piaddr (lease -> address));

	/* For the benefit of Linux (and operating systems which may
	   have similar needs), compute the network address based on
	   the supplied ip address and netmask, if provided.  Also
	   compute the broadcast address (the host address all ones
	   broadcast address, not the host address all zeroes
	   broadcast address). */

	if (lease -> options [DHO_SUBNET_MASK].len &&
	    (lease -> options [DHO_SUBNET_MASK].len <
	     sizeof lease -> address.iabuf)) {
		struct iaddr netmask, subnet, broadcast;

		memcpy (netmask.iabuf,
			lease -> options [DHO_SUBNET_MASK].data,
			lease -> options [DHO_SUBNET_MASK].len);
		netmask.len = lease -> options [DHO_SUBNET_MASK].len;

		subnet = subnet_number (lease -> address, netmask);
		if (subnet.len) {
			client_envadd (ip -> client, prefix, "network_number",
				       "%s", piaddr (subnet));

			if (!lease -> options [DHO_BROADCAST_ADDRESS].len) {
				broadcast = broadcast_addr (subnet, netmask);
				if (broadcast.len) {
				    client_envadd (ip -> client,
						   prefix, "broadcast_address",
						   "%s", piaddr (broadcast));
				}
			}
		}
	}

	if (lease -> filename)
		client_envadd (ip -> client,
			       prefix, "filename", "%s", lease -> filename);
	if (lease -> server_name)
		client_envadd (ip -> client, prefix, "server_name",
			       "%s", lease -> server_name);
	for (i = 0; i < 256; i++) {
		u_int8_t *dp;

		if (ip -> client -> config -> defaults [i].len) {
			if (lease -> options [i].len) {
				switch (ip -> client ->
					config -> default_actions [i]) {
				      case ACTION_DEFAULT:
					dp = lease -> options [i].data;
					len = lease -> options [i].len;
					break;
				      case ACTION_SUPERSEDE:
				      supersede:
					dp = ip -> client ->
						config -> defaults [i].data;
					len = ip -> client ->
						config -> defaults [i].len;
					break;
				      case ACTION_PREPEND:
					len = (ip -> client ->
					       config -> defaults [i].len +
					       lease -> options [i].len);
					if (len > sizeof dbuf) {
						warn ("no space to %s %s",
						      "prepend option",
						      dhcp_options [i].name);
						goto supersede;
					}
					dp = dbuf;
					memcpy (dp,
						ip -> client -> 
						config -> defaults [i].data,
						ip -> client -> 
						config -> defaults [i].len);
					memcpy (dp + ip -> client -> 
						config -> defaults [i].len,
						lease -> options [i].data,
						lease -> options [i].len);
					break;
				      case ACTION_APPEND:
					len = (ip -> client ->
					       config -> defaults [i].len +
					       lease -> options [i].len);
					if (len > sizeof dbuf) {
						warn ("no space to %s %s",
						      "append option",
						      dhcp_options [i].name);
						goto supersede;
					}
					dp = dbuf;
					memcpy (dp,
						lease -> options [i].data,
						lease -> options [i].len);
					memcpy (dp + lease -> options [i].len,
						ip -> client -> 
						config -> defaults [i].data,
						ip -> client -> 
						config -> defaults [i].len);
				}
			} else {
				dp = ip -> client ->
					config -> defaults [i].data;
				len = ip -> client ->
					config -> defaults [i].len;
			}
		} else if (lease -> options [i].len) {
			len = lease -> options [i].len;
			dp = lease -> options [i].data;
		} else {
			len = 0;
		}
		if (len) {
			char name [256];
			if (dhcp_option_ev_name (name, sizeof name,
						 &dhcp_options [i])) {
			    client_envadd (ip -> client, prefix, name, "%s",
					   (pretty_print_option (i, dp,
								 len, 0, 0)));
			}
		}
	}
	client_envadd (ip -> client,
		       prefix, "expiry", "%d", (int)(lease -> expiry));
}

int script_go (ip)
	struct interface_info *ip;
{
	int rval;
	char *scriptName;
	char *argv [2];
	char **envp;
	char *epp [3];
	char reason [] = "REASON=NBI";
	static char client_path [] = CLIENT_PATH;
	int i;
	struct string_list *sp, *next;
	int pid, wpid, wstatus;

	if (ip) {
		scriptName = ip -> client -> config -> script_name;
		envp = dmalloc ((ip -> client -> envc + 2) * sizeof (char *),
				"script_go");
		if (!envp) {
			error ("No memory for client script environment.");
			return 0;
		}
		i = 0;
		for (sp = ip -> client -> env; sp; sp = sp -> next) {
			envp [i++] = sp -> string;
		}
		envp [i++] = client_path;
		envp [i] = (char *)0;
	} else {
		scriptName = top_level_config.script_name;
		epp [0] = reason;
		epp [1] = client_path;
		epp [2] = (char *)0;
		envp = epp;
	}

	argv [0] = scriptName;
	argv [1] = (char *)0;

	pid = fork ();
	if (pid < 0) {
		error ("fork: %m");
		wstatus = 0;
	} else if (pid) {
		do {
			wpid = wait (&wstatus);
		} while (wpid != pid && wpid > 0);
		if (wpid < 0) {
			error ("wait: %m");
			wstatus = 0;
		}
	} else {
		execve (scriptName, argv, envp);
		error ("execve (%s, ...): %m", scriptName);
		exit (0);
	}

	if (ip) {
		for (sp = ip -> client -> env; sp; sp = next) {
			next = sp -> next;
			dfree (sp, "script_go");
		}
		ip -> client -> env = (struct string_list *)0;
		ip -> client -> envc = 0;
		dfree (envp, "script_go");
	}
	return wstatus & 0xff;
}

void client_envadd (struct client_state *client,
		    const char *prefix, const char *name, const char *fmt, ...)
{
	char spbuf [1024];
	char *s;
	unsigned len, i;
	struct string_list *val;
	va_list list;

	va_start (list, fmt);
	len = vsnprintf (spbuf, sizeof spbuf, fmt, list);
	va_end (list);

	val = dmalloc (strlen (prefix) + strlen (name) + 1 /* = */ +
		       len + sizeof *val, "client_envadd");
	if (!val)
		return;
	s = val -> string;
	strcpy (s, prefix);
	strcat (s, name);
	s += strlen (s);
	*s++ = '=';
	if (len >= sizeof spbuf) {
		va_start (list, fmt);
		vsnprintf (s, len + 1, fmt, list);
		va_end (list);
	} else
		strcpy (s, spbuf);
	val -> next = client -> env;
	client -> env = val;
	client -> envc++;
}

int dhcp_option_ev_name (buf, buflen, option)
	char *buf;
	size_t buflen;
	struct option *option;
{
	int i;

	for (i = 0; option -> name [i]; i++) {
		if (i + 1 == buflen)
			return 0;
		if (option -> name [i] == '-')
			buf [i] = '_';
		else
			buf [i] = option -> name [i];
	}

	buf [i] = 0;
	return 1;
}
  
void go_daemon ()
{
	static int state = 0;
	int pid;

	/* Don't become a daemon if the user requested otherwise. */
	if (no_daemon) {
		write_client_pid_file ();
		return;
	}

	/* Only do it once. */
	if (state)
		return;
	state = 1;

	/* Stop logging to stderr... */
	log_perror = 0;

	/* Become a daemon... */
	if ((pid = fork ()) < 0)
		error ("Can't fork daemon: %m");
	else if (pid)
		exit (0);
	/* Become session leader and get pid... */
	pid = setsid ();

	/* Close standard I/O descriptors. */
        close(0);
        close(1);
        close(2);

	write_client_pid_file ();
}

void write_client_pid_file ()
{
	FILE *pf;
	int pfdesc;

	pfdesc = open (path_dhclient_pid, O_CREAT | O_TRUNC | O_WRONLY, 0644);

	if (pfdesc < 0) {
		warn ("Can't create %s: %m", path_dhclient_pid);
		return;
	}

	pf = fdopen (pfdesc, "w");
	if (!pf)
		warn ("Can't fdopen %s: %m", path_dhclient_pid);
	else {
		fprintf (pf, "%ld\n", (long)getpid ());
		fclose (pf);
	}
}
