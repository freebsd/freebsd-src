/* dhclient.c

   DHCP Client. */

/*
 * Copyright (c) 1995-2002 Internet Software Consortium.
 * All rights reserved.
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
 * 3. Neither the name of Internet Software Consortium nor the names
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
 * This code is based on the original client state machine that was
 * written by Elliot Poger.  The code has been extensively hacked on
 * by Ted Lemon since then, so any mistakes you find are probably his
 * fault and not Elliot's.
 */

#ifndef lint
static char ocopyright[] =
"$Id: dhclient.c,v 1.129.2.12 2002/11/07 23:26:38 dhankins Exp $ Copyright (c) 1995-2002 Internet Software Consortium.  All rights reserved.\n"
"$FreeBSD$\n";
#endif /* not lint */

#include "dhcpd.h"
#include "version.h"

#ifdef __FreeBSD__
#include <sys/ioctl.h>
#include <net/if_media.h>
#include <net80211/ieee80211_ioctl.h>
#include <net80211/ieee80211.h>
#endif

TIME cur_time;
TIME default_lease_time = 43200; /* 12 hours... */
TIME max_lease_time = 86400; /* 24 hours... */

const char *path_dhclient_conf = _PATH_DHCLIENT_CONF;
const char *path_dhclient_db = _PATH_DHCLIENT_DB;
const char *path_dhclient_pid = _PATH_DHCLIENT_PID;
static char path_dhclient_script_array [] = _PATH_DHCLIENT_SCRIPT;
char *path_dhclient_script = path_dhclient_script_array;

int dhcp_max_agent_option_packet_length = 0;

int interfaces_requested = 0;

struct iaddr iaddr_broadcast = { 4, { 255, 255, 255, 255 } };
struct iaddr iaddr_any = { 4, { 0, 0, 0, 0 } };
struct in_addr inaddr_any;
struct sockaddr_in sockaddr_broadcast;
struct in_addr giaddr;

/* ASSERT_STATE() does nothing now; it used to be
   assert (state_is == state_shouldbe). */
#define ASSERT_STATE(state_is, state_shouldbe) {}

static char copyright[] = "Copyright 1995-2002 Internet Software Consortium.";
static char arr [] = "All rights reserved.";
static char message [] = "Internet Software Consortium DHCP Client";
static char url [] = "For info, please visit http://www.isc.org/products/DHCP";

u_int16_t local_port=0;
u_int16_t remote_port=0;
int no_daemon=0;
struct string_list *client_env=NULL;
int client_env_count=0;
int onetry=0;
int quiet=1;
int nowait=0;
int doinitcheck=0;
#ifdef ENABLE_POLLING_MODE
int polling_interval = 5;
#endif

static void usage PROTO ((void));

void do_release(struct client_state *);

int main (argc, argv, envp)
	int argc;
	char **argv, **envp;
{
	int i, e;
	struct servent *ent;
	struct interface_info *ip;
	struct client_state *client;
	unsigned seed;
	char *server = (char *)0;
	char *relay = (char *)0;
	isc_result_t status;
 	int release_mode = 0;
	omapi_object_t *listener;
	isc_result_t result;
	int persist = 0;
	int omapi_port;
	int no_dhclient_conf = 0;
	int no_dhclient_db = 0;
	int no_dhclient_pid = 0;
	int no_dhclient_script = 0;
	FILE *pidfd;
	pid_t oldpid;
	char *s;

	oldpid = 0;
	/* Make sure we have stdin, stdout and stderr. */
	i = open ("/dev/null", O_RDWR);
	if (i == 0)
		i = open ("/dev/null", O_RDWR);
	if (i == 1) {
		i = open ("/dev/null", O_RDWR);
		log_perror = 0; /* No sense logging to /dev/null. */
	} else if (i != -1)
		close (i);

#ifdef SYSLOG_4_2
	openlog ("dhclient", LOG_NDELAY);
	log_priority = LOG_DAEMON;
#else
	openlog ("dhclient", LOG_NDELAY, LOG_DAEMON);
#endif

#if !(defined (DEBUG) || defined (SYSLOG_4_2) || defined (__CYGWIN32__))
	setlogmask (LOG_UPTO (LOG_INFO));
#endif	

	/* Set up the OMAPI. */
	status = omapi_init ();
	if (status != ISC_R_SUCCESS)
		log_fatal ("Can't initialize OMAPI: %s",
			   isc_result_totext (status));

	/* Set up the OMAPI wrappers for various server database internal
	   objects. */
	dhcp_common_objects_setup ();

	dhcp_interface_discovery_hook = dhclient_interface_discovery_hook;
	dhcp_interface_shutdown_hook = dhclient_interface_shutdown_hook;
	dhcp_interface_startup_hook = dhclient_interface_startup_hook;

	for (i = 1; i < argc; i++) {
		if (!strcmp (argv [i], "-r")) {
			release_mode = 1;
			no_daemon = 1;
		} else if (!strcmp (argv [i], "-p")) {
			if (++i == argc)
				usage ();
			local_port = htons (atoi (argv [i]));
			log_debug ("binding to user-specified port %d",
			       ntohs (local_port));
		} else if (!strcmp (argv [i], "-d")) {
			no_daemon = 1;
                } else if (!strcmp (argv [i], "-pf")) {
                        if (++i == argc)
                                usage ();
                        path_dhclient_pid = argv [i];
			no_dhclient_pid = 1;
                } else if (!strcmp (argv [i], "-cf")) {
                        if (++i == argc)
                                usage ();
                        path_dhclient_conf = argv [i];
			no_dhclient_conf = 1;
                } else if (!strcmp (argv [i], "-lf")) {
                        if (++i == argc)
                                usage ();
                        path_dhclient_db = argv [i];
			no_dhclient_db = 1;
		} else if (!strcmp (argv [i], "-sf")) {
			if (++i == argc)
				usage ();
                        path_dhclient_script = argv [i];
			no_dhclient_script = 1;
		} else if (!strcmp (argv [i], "-1")) {
			onetry = 1;
		} else if (!strcmp (argv [i], "-q")) {
			quiet = 1;
			quiet_interface_discovery = 1;
		} else if (!strcmp (argv [i], "-v")) {
			quiet = 0;
			quiet_interface_discovery = 0;
		} else if (!strcmp (argv [i], "-s")) {
			if (++i == argc)
				usage ();
			server = argv [i];
		} else if (!strcmp (argv [i], "-g")) {
			if (++i == argc)
				usage ();
			relay = argv [i];
		} else if (!strcmp (argv [i], "-n")) {
			/* do not start up any interfaces */
			interfaces_requested = 1;
#ifdef ENABLE_POLLING_MODE
		} else if (!strcmp (argv [i], "-i")) {
			if (++i == argc)
				usage ();
			polling_interval = (int)strtol(argv [i],
			    (char **)NULL, 10);
			if (polling_interval <= 0) {
				log_info ("Incorrect polling interval %d",
				    polling_interval);
				log_info ("Using a default of 5 seconds");
				polling_interval = 5;
			}
#endif
		} else if (!strcmp (argv [i], "-w")) {
			/* do not exit if there are no broadcast interfaces. */
			persist = 1;
		} else if (!strcmp (argv [i], "-e")) {
			struct string_list *tmp;
			if (++i == argc)
				usage ();
			tmp = dmalloc (strlen (argv [i]) + sizeof *tmp, MDL);
			if (!tmp)
				log_fatal ("No memory for %s", argv [i]);
			strcpy (tmp -> string, argv [i]);
			tmp -> next = client_env;
			client_env = tmp;
			client_env_count++;
		} else if (!strcmp (argv [i], "--version")) {
			log_info ("isc-dhclient-%s", DHCP_VERSION);
			exit (0);
		} else if (!strcmp (argv [i], "-nw")) {
			nowait = 1;
 		} else if (argv [i][0] == '-') {
 		    usage ();
		} else {
 		    struct interface_info *tmp = (struct interface_info *)0;
		    status = interface_allocate (&tmp, MDL);
 		    if (status != ISC_R_SUCCESS)
 			log_fatal ("Can't record interface %s:%s",
				   argv [i], isc_result_totext (status));
		    if (strlen (argv [i]) > sizeof tmp -> name)
			    log_fatal ("%s: interface name too long (max %ld)",
				       argv [i], (long)strlen (argv [i]));
 		    strlcpy (tmp -> name, argv [i], IFNAMSIZ);
		    set_ieee802(tmp);
		    tmp->linkstatus = interface_active(tmp);
		    if (interfaces) {
			    interface_reference (&tmp -> next,
						 interfaces, MDL);
			    interface_dereference (&interfaces, MDL);
		    }
		    interface_reference (&interfaces, tmp, MDL);
 		    tmp -> flags = INTERFACE_REQUESTED;
		    interfaces_requested = 1;
 		}
	}

	if (!no_dhclient_conf && (s = getenv ("PATH_DHCLIENT_CONF"))) {
		path_dhclient_conf = s;
	}
	if (!no_dhclient_db && (s = getenv ("PATH_DHCLIENT_DB"))) {
		path_dhclient_db = s;
	}
	if (!no_dhclient_pid && (s = getenv ("PATH_DHCLIENT_PID"))) {
		path_dhclient_pid = s;
	}
	if (!no_dhclient_script && (s = getenv ("PATH_DHCLIENT_SCRIPT"))) {
		path_dhclient_script = s;
	}

	/* first kill of any currently running client */
	if (release_mode) {

		if ((pidfd = fopen(path_dhclient_pid, "r")) != NULL) {
			e = fscanf(pidfd, "%d", &oldpid);

			if (e != 0 && e != EOF) {
				if (oldpid) {
					if (kill(oldpid, SIGKILL) == 0)
						unlink(path_dhclient_pid);
				}
			}
			fclose(pidfd);
		}
	}

	if (!quiet) {
		log_info ("%s %s", message, DHCP_VERSION);
		log_info (copyright);
		log_info (arr);
		log_info (url);
		log_info ("%s", "");
	} else
		log_perror = 0;

	/* If we're given a relay agent address to insert, for testing
	   purposes, figure out what it is. */
	if (relay) {
		if (!inet_aton (relay, &giaddr)) {
			struct hostent *he;
			he = gethostbyname (relay);
			if (he) {
				memcpy (&giaddr, he -> h_addr_list [0],
					sizeof giaddr);
			} else {
				log_fatal ("%s: no such host", relay);
			}
		}
	}

	/* Default to the DHCP/BOOTP port. */
	if (!local_port) {
		/* If we're faking a relay agent, and we're not using loopback,
		   use the server port, not the client port. */
		if (relay && giaddr.s_addr != htonl (INADDR_LOOPBACK)) {
			local_port = htons(67);
		} else {
			ent = getservbyname ("dhcpc", "udp");
			if (!ent)
				local_port = htons (68);
			else
				local_port = ent -> s_port;
#ifndef __CYGWIN32__
			endservent ();
#endif
		}
	}

	/* If we're faking a relay agent, and we're not using loopback,
	   we're using the server port, not the client port. */
	if (relay && giaddr.s_addr != htonl (INADDR_LOOPBACK)) {
		remote_port = local_port;
	} else
		remote_port = htons (ntohs (local_port) - 1);	/* XXX */

	/* Get the current time... */
	GET_TIME (&cur_time);

	sockaddr_broadcast.sin_family = AF_INET;
	sockaddr_broadcast.sin_port = remote_port;
	if (server) {
		if (!inet_aton (server, &sockaddr_broadcast.sin_addr)) {
			struct hostent *he;
			he = gethostbyname (server);
			if (he) {
				memcpy (&sockaddr_broadcast.sin_addr,
					he -> h_addr_list [0],
					sizeof sockaddr_broadcast.sin_addr);
			} else
				sockaddr_broadcast.sin_addr.s_addr =
					INADDR_BROADCAST;
		}
	} else {
		sockaddr_broadcast.sin_addr.s_addr = INADDR_BROADCAST;
	}

	inaddr_any.s_addr = INADDR_ANY;

	/* Discover all the network interfaces. */
	discover_interfaces (DISCOVER_UNCONFIGURED);

	/* Parse the dhclient.conf file. */
	read_client_conf ();

	/* Parse the lease database. */
	read_client_leases ();

	/* Rewrite the lease database... */
	rewrite_client_leases ();

	/* XXX */
/* 	config_counter(&snd_counter, &rcv_counter); */

	/* If no broadcast interfaces were discovered, call the script
	   and tell it so. */
	if (!interfaces) {
		/* Call dhclient-script with the NBI flag, in case somebody
		   cares. */
		script_init ((struct client_state *)0, "NBI",
			     (struct string_list *)0);
		script_go ((struct client_state *)0);

		/* If we haven't been asked to persist, waiting for new
		   interfaces, then just exit. */
		if (!persist) {
			/* Nothing more to do. */
			log_info ("No broadcast interfaces found - exiting.");
			exit (0);
		}
	} else if (!release_mode) {
		/* Call the script with the list of interfaces. */
		for (ip = interfaces; ip; ip = ip -> next) {
			/* If interfaces were specified, don't configure
			   interfaces that weren't specified! */
			if (interfaces_requested &&
			    ((ip -> flags & (INTERFACE_REQUESTED |
					     INTERFACE_AUTOMATIC)) !=
			     INTERFACE_REQUESTED))
				continue;
			set_ieee802(ip);
			script_init (ip -> client,
				     "PREINIT", (struct string_list *)0);
			if (ip -> client -> alias)
				script_write_params (ip -> client, "alias_",
						     ip -> client -> alias);
			script_go (ip -> client);
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
	seed = 0;
	for (ip = interfaces; ip; ip = ip -> next) {
		int junk;
		memcpy (&junk,
			&ip -> hw_address.hbuf [ip -> hw_address.hlen -
					       sizeof seed], sizeof seed);
		seed += junk;
	}
	srandom (seed + cur_time);

	/* Start a configuration state machine for each interface. */
	for (ip = interfaces; ip; ip = ip -> next) {
		ip -> flags |= INTERFACE_RUNNING;
		for (client = ip -> client; client; client = client -> next) {
			if (release_mode)
				do_release (client);
			else {
				client -> state = S_INIT;
				/* Set up a timeout to start the initialization
				   process. */
#ifdef ENABLE_POLLING_MODE
				add_timeout (cur_time + random () % 5,
					     state_link, client, 0, 0);
#else
				add_timeout(cur_time + random () % 5,
					     state_reboot, client, 0, 0);
#endif
			}
		}
	}

	if (release_mode)
		return 0;

	/* Start up a listener for the object management API protocol. */
	if (top_level_config.omapi_port != -1) {
		listener = (omapi_object_t *)0;
		result = omapi_generic_new (&listener, MDL);
		if (result != ISC_R_SUCCESS)
			log_fatal ("Can't allocate new generic object: %s\n",
				   isc_result_totext (result));
		result = omapi_protocol_listen (listener,
						(unsigned)
						top_level_config.omapi_port,
						1);
		if (result != ISC_R_SUCCESS)
			log_fatal ("Can't start OMAPI protocol: %s",
				   isc_result_totext (result));
	}

	/* Set up the bootp packet handler... */
	bootp_packet_handler = do_packet;

#if defined (DEBUG_MEMORY_LEAKAGE) || defined (DEBUG_MALLOC_POOL) || \
		defined (DEBUG_MEMORY_LEAKAGE_ON_EXIT)
	dmalloc_cutoff_generation = dmalloc_generation;
	dmalloc_longterm = dmalloc_outstanding;
	dmalloc_outstanding = 0;
#endif

	/* If we're not supposed to wait before getting the address,
	   don't. */
	if (nowait)
		go_daemon ();

	/* If we're not going to daemonize, write the pid file
	   now. */
	if (no_daemon || nowait)
		write_client_pid_file ();

	/* Start dispatching packets and timeouts... */
	dispatch ();

	/*NOTREACHED*/
	return 0;
}

static void usage ()
{
	log_info ("%s %s", message, DHCP_VERSION);
	log_info (copyright);
	log_info (arr);
	log_info (url);

	log_error ("Usage: dhclient [-1Ddqr] [-nw] [-p <port>] %s",
		   "[-s server]");
	log_error ("                [-cf config-file] [-lf lease-file]%s",
		   "[-pf pid-file] [-e VAR=val]");
	log_fatal ("                [-sf script-file] [interface]");
}

isc_result_t find_class (struct class **c,
		const char *s, const char *file, int line)
{
	return 0;
}

int check_collection (packet, lease, collection)
	struct packet *packet;
	struct lease *lease;
	struct collection *collection;
{
	return 0;
}

void classify (packet, class)
	struct packet *packet;
	struct class *class;
{
}

int unbill_class (lease, class)
	struct lease *lease;
	struct class *class;
{
	return 0;
}

int find_subnet (struct subnet **sp,
		 struct iaddr addr, const char *file, int line)
{
	return 0;
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

void state_reboot (cpp)
	void *cpp;
{
	struct client_state *client = cpp;

	/* If we don't remember an active lease, go straight to INIT. */
	if (!client -> active ||
	    client -> active -> is_bootp ||
	    client -> active -> expiry <= cur_time) {
		state_init (client);
		return;
	}

	/* We are in the rebooting state. */
	client -> state = S_REBOOTING;

	/* make_request doesn't initialize xid because it normally comes
	   from the DHCPDISCOVER, but we haven't sent a DHCPDISCOVER,
	   so pick an xid now. */
	client -> xid = random ();

	/* Make a DHCPREQUEST packet, and set appropriate per-interface
	   flags. */
	make_request (client, client -> active);
	client -> destination = iaddr_broadcast;
	client -> first_sending = cur_time;
	client -> interval = client -> config -> initial_interval;

	/* Zap the medium list... */
	client -> medium = (struct string_list *)0;

	/* Send out the first DHCPREQUEST packet. */
	send_request (client);
}

/* Called when a lease has completely expired and we've been unable to
   renew it. */

void state_init (cpp)
	void *cpp;
{
	struct client_state *client = cpp;

	ASSERT_STATE(state, S_INIT);

	/* Make a DHCPDISCOVER packet, and set appropriate per-interface
	   flags. */
	make_discover (client, client -> active);
	client -> xid = client -> packet.xid;
	client -> destination = iaddr_broadcast;
	client -> state = S_SELECTING;
	client -> first_sending = cur_time;
	client -> interval = client -> config -> initial_interval;

	/* Add an immediate timeout to cause the first DHCPDISCOVER packet
	   to go out. */
	send_discover (client);
}

/* state_selecting is called when one or more DHCPOFFER packets have been
   received and a configurable period of time has passed. */

void state_selecting (cpp)
	void *cpp;
{
	struct client_state *client = cpp;
	struct client_lease *lp, *next, *picked;


	ASSERT_STATE(state, S_SELECTING);

	/* Cancel state_selecting and send_discover timeouts, since either
	   one could have got us here. */
	cancel_timeout (state_selecting, client);
	cancel_timeout (send_discover, client);

	/* We have received one or more DHCPOFFER packets.   Currently,
	   the only criterion by which we judge leases is whether or
	   not we get a response when we arp for them. */
	picked = (struct client_lease *)0;
	for (lp = client -> offered_leases; lp; lp = next) {
		next = lp -> next;

		/* Check to see if we got an ARPREPLY for the address
		   in this particular lease. */
		if (!picked) {
			picked = lp;
			picked -> next = (struct client_lease *)0;
		} else {
		      freeit:
			destroy_client_lease (lp);
		}
	}
	client -> offered_leases = (struct client_lease *)0;

	/* If we just tossed all the leases we were offered, go back
	   to square one. */
	if (!picked) {
		client -> state = S_INIT;
		state_init (client);
		return;
	}

	/* If it was a BOOTREPLY, we can just take the address right now. */
	if (picked -> is_bootp) {
		client -> new = picked;

		/* Make up some lease expiry times
		   XXX these should be configurable. */
		client -> new -> expiry = cur_time + 12000;
		client -> new -> renewal += cur_time + 8000;
		client -> new -> rebind += cur_time + 10000;

		client -> state = S_REQUESTING;

		/* Bind to the address we received. */
		bind_lease (client);
		return;
	}

	/* Go to the REQUESTING state. */
	client -> destination = iaddr_broadcast;
	client -> state = S_REQUESTING;
	client -> first_sending = cur_time;
	client -> interval = client -> config -> initial_interval;

	/* Make a DHCPREQUEST packet from the lease we picked. */
	make_request (client, picked);
	client -> xid = client -> packet.xid;

	/* Toss the lease we picked - we'll get it back in a DHCPACK. */
	destroy_client_lease (picked);

	/* Add an immediate timeout to send the first DHCPREQUEST packet. */
	send_request (client);
}  

/* state_requesting is called when we receive a DHCPACK message after
   having sent out one or more DHCPREQUEST packets. */

void dhcpack (packet)
	struct packet *packet;
{
	struct interface_info *ip = packet -> interface;
	struct client_state *client;
	struct client_lease *lease;
	struct option_cache *oc;
	struct data_string ds;
	int i;
	
	/* If we're not receptive to an offer right now, or if the offer
	   has an unrecognizable transaction id, then just drop it. */
	for (client = ip -> client; client; client = client -> next) {
		if (client -> xid == packet -> raw -> xid)
			break;
	}
	if (!client ||
	    (packet -> interface -> hw_address.hlen - 1 !=
	     packet -> raw -> hlen) ||
	    (memcmp (&packet -> interface -> hw_address.hbuf [1],
		     packet -> raw -> chaddr, packet -> raw -> hlen))) {
#if defined (DEBUG)
		log_debug ("DHCPACK in wrong transaction.");
#endif
		return;
	}

	if (client -> state != S_REBOOTING &&
	    client -> state != S_REQUESTING &&
	    client -> state != S_RENEWING &&
	    client -> state != S_REBINDING) {
#if defined (DEBUG)
		log_debug ("DHCPACK in wrong state.");
#endif
		return;
	}

	log_info ("DHCPACK from %s", piaddr (packet -> client_addr));

	lease = packet_to_lease (packet, client);
	if (!lease) {
		log_info ("packet_to_lease failed.");
		return;
	}

	client -> new = lease;

	/* Stop resending DHCPREQUEST. */
	cancel_timeout (send_request, client);

	/* Figure out the lease time. */
	oc = lookup_option (&dhcp_universe, client -> new -> options,
			    DHO_DHCP_LEASE_TIME);
	memset (&ds, 0, sizeof ds);
	if (oc &&
	    evaluate_option_cache (&ds, packet, (struct lease *)0, client,
				   packet -> options, client -> new -> options,
				   &global_scope, oc, MDL)) {
		if (ds.len > 3)
			client -> new -> expiry = getULong (ds.data);
		else
			client -> new -> expiry = 0;
		data_string_forget (&ds, MDL);
	} else
			client -> new -> expiry = 0;

	if (!client -> new -> expiry) {
		log_error ("no expiry time on offered lease.");
		/* XXX this is going to be bad - if this _does_
		   XXX happen, we should probably dynamically 
		   XXX disqualify the DHCP server that gave us the
		   XXX bad packet from future selections and
		   XXX then go back into the init state. */
		state_init (client);
		return;
	}

	/* A number that looks negative here is really just very large,
	   because the lease expiry offset is unsigned. */
	if (client -> new -> expiry < 0)
		client -> new -> expiry = TIME_MAX;
	/* Take the server-provided renewal time if there is one. */
	oc = lookup_option (&dhcp_universe, client -> new -> options,
			    DHO_DHCP_RENEWAL_TIME);
	if (oc &&
	    evaluate_option_cache (&ds, packet, (struct lease *)0, client,
				   packet -> options, client -> new -> options,
				   &global_scope, oc, MDL)) {
		if (ds.len > 3)
			client -> new -> renewal = getULong (ds.data);
		else
			client -> new -> renewal = 0;
		data_string_forget (&ds, MDL);
	} else
			client -> new -> renewal = 0;

	/* If it wasn't specified by the server, calculate it. */
	if (!client -> new -> renewal)
		client -> new -> renewal =
			client -> new -> expiry / 2;

	/* Now introduce some randomness to the renewal time: */
	client -> new -> renewal = (((client -> new -> renewal + 3) * 3 / 4) +
				    (random () % /* XXX NUMS */
				     ((client -> new -> renewal + 3) / 4)));

	/* Same deal with the rebind time. */
	oc = lookup_option (&dhcp_universe, client -> new -> options,
			    DHO_DHCP_REBINDING_TIME);
	if (oc &&
	    evaluate_option_cache (&ds, packet, (struct lease *)0, client,
				   packet -> options, client -> new -> options,
				   &global_scope, oc, MDL)) {
		if (ds.len > 3)
			client -> new -> rebind = getULong (ds.data);
		else
			client -> new -> rebind = 0;
		data_string_forget (&ds, MDL);
	} else
			client -> new -> rebind = 0;

	if (!client -> new -> rebind)
		client -> new -> rebind =
			(client -> new -> expiry * 7) / 8; /* XXX NUMS */

	/* Make sure our randomness didn't run the renewal time past the
	   rebind time. */
	if (client -> new -> renewal > client -> new -> rebind)
		client -> new -> renewal = (client -> new -> rebind * 3) / 4;

	client -> new -> expiry += cur_time;
	/* Lease lengths can never be negative. */
	if (client -> new -> expiry < cur_time)
		client -> new -> expiry = TIME_MAX;
	client -> new -> renewal += cur_time;
	if (client -> new -> renewal < cur_time)
		client -> new -> renewal = TIME_MAX;
	client -> new -> rebind += cur_time;
	if (client -> new -> rebind < cur_time)
		client -> new -> rebind = TIME_MAX;

	bind_lease (client);
}

void bind_lease (client)
	struct client_state *client;
{
	struct interface_info *ip = client -> interface;

	/* Remember the medium. */
	client -> new -> medium = client -> medium;

	/* Run the client script with the new parameters. */
	script_init (client, (client -> state == S_REQUESTING
			  ? "BOUND"
			  : (client -> state == S_RENEWING
			     ? "RENEW"
			     : (client -> state == S_REBOOTING
				? "REBOOT" : "REBIND"))),
		     client -> new -> medium);
	if (client -> active && client -> state != S_REBOOTING)
		script_write_params (client, "old_", client -> active);
	script_write_params (client, "new_", client -> new);
	if (client -> alias)
		script_write_params (client, "alias_", client -> alias);

	/* If the BOUND/RENEW code detects another machine using the
	   offered address, it exits nonzero.  We need to send a
	   DHCPDECLINE and toss the lease. */
	if (script_go (client)) {
		make_decline (client, client -> new);
		send_decline (client);
		destroy_client_lease (client -> new);
		client -> new = (struct client_lease *)0;
		state_init (client);
		return;
	}

	/* Write out the new lease. */
	write_client_lease (client, client -> new, 0, 0);

	/* Replace the old active lease with the new one. */
	if (client -> active)
		destroy_client_lease (client -> active);
	client -> active = client -> new;
	client -> new = (struct client_lease *)0;

	/* Set up a timeout to start the renewal process. */
	add_timeout (client -> active -> renewal,
		     state_bound, client, 0, 0);

	log_info ("bound to %s -- renewal in %ld seconds.",
	      piaddr (client -> active -> address),
	      (long)(client -> active -> renewal - cur_time));
	client -> state = S_BOUND;
	reinitialize_interfaces ();
	go_daemon ();
	if (client -> config -> do_forward_update) {
		client -> dns_update_timeout = 1;
		add_timeout (cur_time + 1, client_dns_update_timeout,
			     client, 0, 0);
	}
}  

/* state_bound is called when we've successfully bound to a particular
   lease, but the renewal time on that lease has expired.   We are
   expected to unicast a DHCPREQUEST to the server that gave us our
   original lease. */

void state_bound (cpp)
	void *cpp;
{
	struct client_state *client = cpp;
	int i;
	struct option_cache *oc;
	struct data_string ds;

	ASSERT_STATE(state, S_BOUND);

	/* T1 has expired. */
	make_request (client, client -> active);
	client -> xid = client -> packet.xid;

	memset (&ds, 0, sizeof ds);
	oc = lookup_option (&dhcp_universe, client -> active -> options,
			    DHO_DHCP_SERVER_IDENTIFIER);
	if (oc &&
	    evaluate_option_cache (&ds, (struct packet *)0, (struct lease *)0,
				   client, (struct option_state *)0,
				   client -> active -> options,
				   &global_scope, oc, MDL)) {
		if (ds.len > 3) {
			memcpy (client -> destination.iabuf, ds.data, 4);
			client -> destination.len = 4;
		} else
			client -> destination = iaddr_broadcast;
	} else
		client -> destination = iaddr_broadcast;

	client -> first_sending = cur_time;
	client -> interval = client -> config -> initial_interval;
	client -> state = S_RENEWING;

	/* Send the first packet immediately. */
	send_request (client);
}  

/* state_stop is called when we've been told to shut down.   We unconfigure
   the interfaces, and then stop operating until told otherwise. */

void state_stop (cpp)
	void *cpp;
{
	struct client_state *client = cpp;
	int i;

	/* Cancel all timeouts. */
	cancel_timeout (state_selecting, client);
	cancel_timeout (send_discover, client);
	cancel_timeout (send_request, client);
	cancel_timeout (state_bound, client);

	/* If we have an address, unconfigure it. */
	if (client -> active) {
		script_init (client, "STOP", client -> active -> medium);
		script_write_params (client, "old_", client -> active);
		if (client -> alias)
			script_write_params (client, "alias_",
					     client -> alias);
		script_go (client);
	}
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

int write_host (host)
	struct host_decl *host;
{
	return 0;
}

void db_startup (testp)
	int testp;
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
			log_info ("BOOTREPLY from %s rejected.",
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
	const char *type;

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
			log_info ("%s from %s rejected.",
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
	struct client_state *client;
	struct client_lease *lease, *lp;
	int i;
	int stop_selecting;
	const char *name = packet -> packet_type ? "DHCPOFFER" : "BOOTREPLY";
	struct iaddrlist *ap;
	struct option_cache *oc;
	char obuf [1024];
	
#ifdef DEBUG_PACKET
	dump_packet (packet);
#endif	

	/* Find a client state that matches the xid... */
	for (client = ip -> client; client; client = client -> next)
		if (client -> xid == packet -> raw -> xid)
			break;

	/* If we're not receptive to an offer right now, or if the offer
	   has an unrecognizable transaction id, then just drop it. */
	if (!client ||
	    client -> state != S_SELECTING ||
	    (packet -> interface -> hw_address.hlen - 1 !=
	     packet -> raw -> hlen) ||
	    (memcmp (&packet -> interface -> hw_address.hbuf [1],
		     packet -> raw -> chaddr, packet -> raw -> hlen))) {
#if defined (DEBUG)
		log_debug ("%s in wrong transaction.", name);
#endif
		return;
	}

	sprintf (obuf, "%s from %s", name, piaddr (packet -> client_addr));


	/* If this lease doesn't supply the minimum required parameters,
	   blow it off. */
	if (client -> config -> required_options) {
	    for (i = 0; client -> config -> required_options [i]; i++) {
		if (!lookup_option
		    (&dhcp_universe, packet -> options,
		     client -> config -> required_options [i])) {
		    log_info ("%s: no %s option.",
			      obuf, (dhcp_universe.options
				     [client -> config -> required_options [i]]
				     -> name));
				return;
			}
		}
	}

	/* If we've already seen this lease, don't record it again. */
	for (lease = client -> offered_leases; lease; lease = lease -> next) {
		if (lease -> address.len == sizeof packet -> raw -> yiaddr &&
		    !memcmp (lease -> address.iabuf,
			     &packet -> raw -> yiaddr, lease -> address.len)) {
			log_debug ("%s: already seen.", obuf);
			return;
		}
	}

	lease = packet_to_lease (packet, client);
	if (!lease) {
		log_info ("%s: packet_to_lease failed.", obuf);
		return;
	}

	/* If this lease was acquired through a BOOTREPLY, record that
	   fact. */
	if (!packet -> options_valid || !packet -> packet_type)
		lease -> is_bootp = 1;

	/* Record the medium under which this lease was offered. */
	lease -> medium = client -> medium;

	/* Figure out when we're supposed to stop selecting. */
	stop_selecting = (client -> first_sending +
			  client -> config -> select_interval);

	/* If this is the lease we asked for, put it at the head of the
	   list, and don't mess with the arp request timeout. */
	if (lease -> address.len == client -> requested_address.len &&
	    !memcmp (lease -> address.iabuf,
		     client -> requested_address.iabuf,
		     client -> requested_address.len)) {
		lease -> next = client -> offered_leases;
		client -> offered_leases = lease;
	} else {
		/* Put the lease at the end of the list. */
		lease -> next = (struct client_lease *)0;
		if (!client -> offered_leases)
			client -> offered_leases = lease;
		else {
			for (lp = client -> offered_leases; lp -> next;
			     lp = lp -> next)
				;
			lp -> next = lease;
		}
	}

	/* If the selecting interval has expired, go immediately to
	   state_selecting().  Otherwise, time out into
	   state_selecting at the select interval. */
	if (stop_selecting <= 0)
		state_selecting (client);
	else {
		add_timeout (stop_selecting, state_selecting, client, 0, 0);
		cancel_timeout (send_discover, client);
	}
	log_info ("%s", obuf);
}

/* Allocate a client_lease structure and initialize it from the parameters
   in the specified packet. */

struct client_lease *packet_to_lease (packet, client)
	struct packet *packet;
	struct client_state *client;
{
	struct client_lease *lease;
	unsigned i;
	struct option_cache *oc;
	struct data_string data;

	lease = (struct client_lease *)new_client_lease (MDL);

	if (!lease) {
		log_error ("packet_to_lease: no memory to record lease.\n");
		return (struct client_lease *)0;
	}

	memset (lease, 0, sizeof *lease);

	/* Copy the lease options. */
	option_state_reference (&lease -> options, packet -> options, MDL);

	lease -> address.len = sizeof (packet -> raw -> yiaddr);
	memcpy (lease -> address.iabuf, &packet -> raw -> yiaddr,
		lease -> address.len);

	memset (&data, 0, sizeof data);

	if (client -> config -> vendor_space_name) {
		i = DHO_VENDOR_ENCAPSULATED_OPTIONS;

		/* See if there was a vendor encapsulation option. */
		oc = lookup_option (&dhcp_universe, lease -> options, i);
		if (oc &&
		    client -> config -> vendor_space_name &&
		    evaluate_option_cache (&data, packet,
					   (struct lease *)0, client,
					   packet -> options, lease -> options,
					   &global_scope, oc, MDL)) {
			if (data.len) {
				parse_encapsulated_suboptions
					(packet -> options, &dhcp_options [i],
					 data.data, data.len, &dhcp_universe,
					 client -> config -> vendor_space_name
						);
			}
			data_string_forget (&data, MDL);
		}
	} else
		i = 0;

	/* Figure out the overload flag. */
	oc = lookup_option (&dhcp_universe, lease -> options,
			    DHO_DHCP_OPTION_OVERLOAD);
	if (oc &&
	    evaluate_option_cache (&data, packet, (struct lease *)0, client,
				   packet -> options, lease -> options,
				   &global_scope, oc, MDL)) {
		if (data.len > 0)
			i = data.data [0];
		else
			i = 0;
		data_string_forget (&data, MDL);
	} else
		i = 0;

	/* If the server name was filled out, copy it. */
	if (!(i & 2) && packet -> raw -> sname [0]) {
		unsigned len;
		/* Don't count on the NUL terminator. */
		for (len = 0; len < 64; len++)
			if (!packet -> raw -> sname [len])
				break;
		lease -> server_name = dmalloc (len + 1, MDL);
		if (!lease -> server_name) {
			log_error ("dhcpoffer: no memory for filename.\n");
			destroy_client_lease (lease);
			return (struct client_lease *)0;
		} else {
			memcpy (lease -> server_name,
				packet -> raw -> sname, len);
			lease -> server_name [len] = 0;
		}
	}

	/* Ditto for the filename. */
	if (!(i & 1) && packet -> raw -> file [0]) {
		unsigned len;
		/* Don't count on the NUL terminator. */
		for (len = 0; len < 64; len++)
			if (!packet -> raw -> file [len])
				break;
		lease -> filename = dmalloc (len + 1, MDL);
		if (!lease -> filename) {
			log_error ("dhcpoffer: no memory for filename.\n");
			destroy_client_lease (lease);
			return (struct client_lease *)0;
		} else {
			memcpy (lease -> filename,
				packet -> raw -> file, len);
			lease -> filename [len] = 0;
		}
	}

	execute_statements_in_scope ((struct binding_value **)0,
				     (struct packet *)packet,
				     (struct lease *)0, client,
				     lease -> options, lease -> options,
				     &global_scope,
				     client -> config -> on_receipt,
				     (struct group *)0);

	return lease;
}	

void dhcpnak (packet)
	struct packet *packet;
{
	struct interface_info *ip = packet -> interface;
	struct client_state *client;

	/* Find a client state that matches the xid... */
	for (client = ip -> client; client; client = client -> next)
		if (client -> xid == packet -> raw -> xid)
			break;

	/* If we're not receptive to an offer right now, or if the offer
	   has an unrecognizable transaction id, then just drop it. */
	if (!client ||
	    (packet -> interface -> hw_address.hlen - 1 !=
	     packet -> raw -> hlen) ||
	    (memcmp (&packet -> interface -> hw_address.hbuf [1],
		     packet -> raw -> chaddr, packet -> raw -> hlen))) {
#if defined (DEBUG)
		log_debug ("DHCPNAK in wrong transaction.");
#endif
		return;
	}

	if (client -> state != S_REBOOTING &&
	    client -> state != S_REQUESTING &&
	    client -> state != S_RENEWING &&
	    client -> state != S_REBINDING) {
#if defined (DEBUG)
		log_debug ("DHCPNAK in wrong state.");
#endif
		return;
	}

	log_info ("DHCPNAK from %s", piaddr (packet -> client_addr));

	if (!client -> active) {
#if defined (DEBUG)
		log_info ("DHCPNAK with no active lease.\n");
#endif
		return;
	}

	destroy_client_lease (client -> active);
	client -> active = (struct client_lease *)0;

	/* Stop sending DHCPREQUEST packets... */
	cancel_timeout (send_request, client);

	client -> state = S_INIT;
	state_init (client);
}

/* Send out a DHCPDISCOVER packet, and set a timeout to send out another
   one after the right interval has expired.  If we don't get an offer by
   the time we reach the panic interval, call the panic function. */

void send_discover (cpp)
	void *cpp;
{
	struct client_state *client = cpp;

	int result;
	int interval;
	int increase = 1;

	if (interface_active(client -> interface) == 0)
		return;

	/* Figure out how long it's been since we started transmitting. */
	interval = cur_time - client -> first_sending;

	/* If we're past the panic timeout, call the script and tell it
	   we haven't found anything for this interface yet. */
	if (interval > client -> config -> timeout) {
		state_panic (client);
		return;
	}

	/* If we're selecting media, try the whole list before doing
	   the exponential backoff, but if we've already received an
	   offer, stop looping, because we obviously have it right. */
	if (!client -> offered_leases &&
	    client -> config -> media) {
		int fail = 0;
	      again:
		if (client -> medium) {
			client -> medium = client -> medium -> next;
			increase = 0;
		} 
		if (!client -> medium) {
			if (fail)
				log_fatal ("No valid media types for %s!",
				       client -> interface -> name);
			client -> medium =
				client -> config -> media;
			increase = 1;
		}
			
		log_info ("Trying medium \"%s\" %d",
			  client -> medium -> string, increase);
		script_init (client, "MEDIUM", client -> medium);
		if (script_go (client)) {
			fail = 1;
			goto again;
		}
	}

	/* If we're supposed to increase the interval, do so.  If it's
	   currently zero (i.e., we haven't sent any packets yet), set
	   it to one; otherwise, add to it a random number between
	   zero and two times itself.  On average, this means that it
	   will double with every transmission. */
	if (increase) {
		if (!client -> interval)
			client -> interval =
				client -> config -> initial_interval;
		else
			client -> interval += ((random () >> 2) %
					       (2 * client -> interval));

		/* Don't backoff past cutoff. */
		if (client -> interval >
		    client -> config -> backoff_cutoff)
			client -> interval =
				((client -> config -> backoff_cutoff / 2)
				 + ((random () >> 2) %
				    client -> config -> backoff_cutoff));
	} else if (!client -> interval)
		client -> interval = client -> config -> initial_interval;
		
	/* If the backoff would take us to the panic timeout, just use that
	   as the interval. */
	if (cur_time + client -> interval >
	    client -> first_sending + client -> config -> timeout)
		client -> interval =
			(client -> first_sending +
			 client -> config -> timeout) - cur_time + 1;

	/* Record the number of seconds since we started sending. */
	if (interval < 65536)
		client -> packet.secs = htons (interval);
	else
		client -> packet.secs = htons (65535);
	client -> secs = client -> packet.secs;

	log_info ("DHCPDISCOVER on %s to %s port %d interval %ld",
	      client -> name ? client -> name : client -> interface -> name,
	      inet_ntoa (sockaddr_broadcast.sin_addr),
	      ntohs (sockaddr_broadcast.sin_port), (long)(client -> interval));

	/* Send out a packet. */
	result = send_packet (client -> interface, (struct packet *)0,
			      &client -> packet,
			      client -> packet_length,
			      inaddr_any, &sockaddr_broadcast,
			      (struct hardware *)0);

	add_timeout (cur_time + client -> interval,
		     send_discover, client, 0, 0);
}

/* state_panic gets called if we haven't received any offers in a preset
   amount of time.   When this happens, we try to use existing leases that
   haven't yet expired, and failing that, we call the client script and
   hope it can do something. */

void state_panic (cpp)
	void *cpp;
{
	struct client_state *client = cpp;
	struct client_lease *loop;
	struct client_lease *lp;

	if (interface_active(client -> interface) == 0)
		return;

	loop = lp = client -> active;

	log_info ("No DHCPOFFERS received.");

	/* We may not have an active lease, but we may have some
	   predefined leases that we can try. */
	if (!client -> active && client -> leases)
		goto activate_next;

	/* Run through the list of leases and see if one can be used. */
	while (client -> active) {
		if (client -> active -> expiry > cur_time) {
			log_info ("Trying recorded lease %s",
			      piaddr (client -> active -> address));
			/* Run the client script with the existing
			   parameters. */
			script_init (client, "TIMEOUT",
				     client -> active -> medium);
			script_write_params (client, "new_", client -> active);
			if (client -> alias)
				script_write_params (client, "alias_",
						     client -> alias);

			/* If the old lease is still good and doesn't
			   yet need renewal, go into BOUND state and
			   timeout at the renewal time. */
			if (!script_go (client)) {
			    if (cur_time < client -> active -> renewal) {
				client -> state = S_BOUND;
				log_info ("bound: renewal in %ld %s.",
					  (long)(client -> active -> renewal -
						 cur_time), "seconds");
				add_timeout (client -> active -> renewal,
					     state_bound, client, 0, 0);
			    } else {
				client -> state = S_BOUND;
				log_info ("bound: immediate renewal.");
				state_bound (client);
			    }
			    reinitialize_interfaces ();
			    go_daemon ();
			    return;
			}
		}

		/* If there are no other leases, give up. */
		if (!client -> leases) {
			client -> leases = client -> active;
			client -> active = (struct client_lease *)0;
			break;
		}

	activate_next:
		/* Otherwise, put the active lease at the end of the
		   lease list, and try another lease.. */
		for (lp = client -> leases; lp -> next; lp = lp -> next)
			;
		lp -> next = client -> active;
		if (lp -> next) {
			lp -> next -> next = (struct client_lease *)0;
		}
		client -> active = client -> leases;
		client -> leases = client -> leases -> next;

		/* If we already tried this lease, we've exhausted the
		   set of leases, so we might as well give up for
		   now. */
		if (client -> active == loop)
			break;
		else if (!loop)
			loop = client -> active;
	}

	/* No leases were available, or what was available didn't work, so
	   tell the shell script that we failed to allocate an address,
	   and try again later. */
	if (onetry) {
		if (!quiet)
			log_info ("Unable to obtain a lease on first try.%s",
				  "  Exiting.");
		exit (2);
	}

	log_info ("No working leases in persistent database - sleeping.");
	script_init (client, "FAIL", (struct string_list *)0);
	if (client -> alias)
		script_write_params (client, "alias_", client -> alias);
	script_go (client);
	client -> state = S_INIT;
	add_timeout (cur_time +
		     ((client -> config -> retry_interval + 1) / 2 +
		      (random () % client -> config -> retry_interval)),
		     state_init, client, 0, 0);
	go_daemon ();
}

void send_request (cpp)
	void *cpp;
{
	struct client_state *client = cpp;

	int result;
	int interval;
	struct sockaddr_in destination;
	struct in_addr from;

	/* Figure out how long it's been since we started transmitting. */
	interval = cur_time - client -> first_sending;

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
	if ((client -> state == S_REBOOTING ||
	     client -> state == S_REQUESTING) &&
	    interval > client -> config -> reboot_timeout) {
	cancel:
		client -> state = S_INIT;
		cancel_timeout (send_request, client);
		state_init (client);
		return;
	}

	/* If we're in the reboot state, make sure the media is set up
	   correctly. */
	if (client -> state == S_REBOOTING &&
	    !client -> medium &&
	    client -> active -> medium ) {
		script_init (client, "MEDIUM", client -> active -> medium);

		/* If the medium we chose won't fly, go to INIT state. */
		if (script_go (client))
			goto cancel;

		/* Record the medium. */
		client -> medium = client -> active -> medium;
	}

	/* If the lease has expired, relinquish the address and go back
	   to the INIT state. */
	if (client -> state != S_REQUESTING &&
	    cur_time > client -> active -> expiry) {
		/* Run the client script with the new parameters. */
		script_init (client, "EXPIRE", (struct string_list *)0);
		script_write_params (client, "old_", client -> active);
		if (client -> alias)
			script_write_params (client, "alias_",
					     client -> alias);
		script_go (client);

		/* Now do a preinit on the interface so that we can
		   discover a new address. */
		script_init (client, "PREINIT", (struct string_list *)0);
		if (client -> alias)
			script_write_params (client, "alias_",
					     client -> alias);
		script_go (client);

		client -> state = S_INIT;
		state_init (client);
		return;
	}

	/* Do the exponential backoff... */
	if (!client -> interval)
		client -> interval = client -> config -> initial_interval;
	else {
		client -> interval += ((random () >> 2) %
				       (2 * client -> interval));
	}
	
	/* Don't backoff past cutoff. */
	if (client -> interval >
	    client -> config -> backoff_cutoff)
		client -> interval =
			((client -> config -> backoff_cutoff / 2)
			 + ((random () >> 2) % client -> interval));

	/* If the backoff would take us to the expiry time, just set the
	   timeout to the expiry time. */
	if (client -> state != S_REQUESTING &&
	    cur_time + client -> interval > client -> active -> expiry)
		client -> interval =
			client -> active -> expiry - cur_time + 1;

	/* If the lease T2 time has elapsed, or if we're not yet bound,
	   broadcast the DHCPREQUEST rather than unicasting. */
	if (client -> state == S_REQUESTING ||
	    client -> state == S_REBOOTING ||
	    cur_time > client -> active -> rebind)
		destination.sin_addr = sockaddr_broadcast.sin_addr;
	else
		memcpy (&destination.sin_addr.s_addr,
			client -> destination.iabuf,
			sizeof destination.sin_addr.s_addr);
	destination.sin_port = remote_port;
	destination.sin_family = AF_INET;
#ifdef HAVE_SA_LEN
	destination.sin_len = sizeof destination;
#endif

	if (client -> state == S_RENEWING ||
	    client -> state == S_REBINDING)
		memcpy (&from, client -> active -> address.iabuf,
			sizeof from);
	else
		from.s_addr = INADDR_ANY;

	/* Record the number of seconds since we started sending. */
	if (client -> state == S_REQUESTING)
		client -> packet.secs = client -> secs;
	else {
		if (interval < 65536)
			client -> packet.secs = htons (interval);
		else
			client -> packet.secs = htons (65535);
	}

	log_info ("DHCPREQUEST on %s to %s port %d",
	      client -> name ? client -> name : client -> interface -> name,
	      inet_ntoa (destination.sin_addr),
	      ntohs (destination.sin_port));

	if (destination.sin_addr.s_addr != INADDR_BROADCAST &&
	    fallback_interface)
		result = send_packet (fallback_interface,
				      (struct packet *)0,
				      &client -> packet,
				      client -> packet_length,
				      from, &destination,
				      (struct hardware *)0);
	else
		/* Send out a packet. */
		result = send_packet (client -> interface, (struct packet *)0,
				      &client -> packet,
				      client -> packet_length,
				      from, &destination,
				      (struct hardware *)0);

	add_timeout (cur_time + client -> interval,
		     send_request, client, 0, 0);
}

void send_decline (cpp)
	void *cpp;
{
	struct client_state *client = cpp;

	int result;

	log_info ("DHCPDECLINE on %s to %s port %d",
	      client -> name ? client -> name : client -> interface -> name,
	      inet_ntoa (sockaddr_broadcast.sin_addr),
	      ntohs (sockaddr_broadcast.sin_port));

	/* Send out a packet. */
	result = send_packet (client -> interface, (struct packet *)0,
			      &client -> packet,
			      client -> packet_length,
			      inaddr_any, &sockaddr_broadcast,
			      (struct hardware *)0);
}

void send_release (cpp)
	void *cpp;
{
	struct client_state *client = cpp;

	int result;
	struct sockaddr_in destination;
	struct in_addr from;

	memcpy (&from, client -> active -> address.iabuf,
		sizeof from);
	memcpy (&destination.sin_addr.s_addr,
		client -> destination.iabuf,
		sizeof destination.sin_addr.s_addr);
	destination.sin_port = remote_port;
	destination.sin_family = AF_INET;
#ifdef HAVE_SA_LEN
	destination.sin_len = sizeof destination;
#endif

	/* Set the lease to end now, so that we don't accidentally
	   reuse it if we restart before the old expiry time. */
	client -> active -> expiry =
		client -> active -> renewal =
		client -> active -> rebind = cur_time;
	if (!write_client_lease (client, client -> active, 1, 1)) {
		log_error ("Can't release lease: lease write failed.");
		return;
	}

	log_info ("DHCPRELEASE on %s to %s port %d",
	      client -> name ? client -> name : client -> interface -> name,
	      inet_ntoa (destination.sin_addr),
	      ntohs (destination.sin_port));

	if (fallback_interface)
		result = send_packet (fallback_interface,
				      (struct packet *)0,
				      &client -> packet,
				      client -> packet_length,
				      from, &destination,
				      (struct hardware *)0);
	else
		/* Send out a packet. */
		result = send_packet (client -> interface, (struct packet *)0,
				      &client -> packet,
				      client -> packet_length,
				      from, &destination,
				      (struct hardware *)0);
}

void make_client_options (client, lease, type, sid, rip, prl, op)
	struct client_state *client;
	struct client_lease *lease;
	u_int8_t *type;
	struct option_cache *sid;
	struct iaddr *rip;
	u_int32_t *prl;
	struct option_state **op;
{
	unsigned i;
	struct option_cache *oc;
	struct buffer *bp = (struct buffer *)0;

	/* If there are any leftover options, get rid of them. */
	if (*op)
		option_state_dereference (op, MDL);

	/* Allocate space for options. */
	option_state_allocate (op, MDL);

	/* Send the server identifier if provided. */
	if (sid)
		save_option (&dhcp_universe, *op, sid);

	oc = (struct option_cache *)0;

	/* Send the requested address if provided. */
	if (rip) {
		client -> requested_address = *rip;
		if (!(make_const_option_cache
		      (&oc, (struct buffer **)0, rip -> iabuf, rip -> len,
		       &dhcp_options [DHO_DHCP_REQUESTED_ADDRESS], MDL)))
			log_error ("can't make requested address cache.");
		else {
			save_option (&dhcp_universe, *op, oc);
			option_cache_dereference (&oc, MDL);
		}
	} else {
		client -> requested_address.len = 0;
	}

	if (!(make_const_option_cache
	      (&oc, (struct buffer **)0,
	       type, 1, &dhcp_options [DHO_DHCP_MESSAGE_TYPE], MDL)))
		log_error ("can't make message type.");
	else {
		save_option (&dhcp_universe, *op, oc);
		option_cache_dereference (&oc, MDL);
	}

	if (prl) {
		/* Figure out how many parameters were requested. */
		for (i = 0; prl [i]; i++)
			;
		if (!buffer_allocate (&bp, i, MDL))
			log_error ("can't make parameter list buffer.");
		else {
			for (i = 0; prl [i]; i++)
				bp -> data [i] = prl [i];
			if (!(make_const_option_cache
			      (&oc, &bp, (u_int8_t *)0, i,
			       &dhcp_options [DHO_DHCP_PARAMETER_REQUEST_LIST],
			       MDL)))
				log_error ("can't make option cache");
			else {
				save_option (&dhcp_universe, *op, oc);
				option_cache_dereference (&oc, MDL);
			}
		}
	}

	/* Run statements that need to be run on transmission. */
	if (client -> config -> on_transmission)
		execute_statements_in_scope
			((struct binding_value **)0,
			 (struct packet *)0, (struct lease *)0, client,
			 (lease ? lease -> options : (struct option_state *)0),
			 *op, &global_scope,
			 client -> config -> on_transmission,
			 (struct group *)0);
}

void make_discover (client, lease)
	struct client_state *client;
	struct client_lease *lease;
{
	unsigned char discover = DHCPDISCOVER;
	int i;
	struct option_state *options = (struct option_state *)0;

	memset (&client -> packet, 0, sizeof (client -> packet));

	make_client_options (client,
			     lease, &discover, (struct option_cache *)0,
			     lease ? &lease -> address : (struct iaddr *)0,
			     client -> config -> requested_options,
			     &options);

	/* Set up the option buffer... */
	client -> packet_length =
		cons_options ((struct packet *)0, &client -> packet,
			      (struct lease *)0, client, 0,
			      (struct option_state *)0, options,
			      &global_scope, 0, 0, 0, (struct data_string *)0,
			      client -> config -> vendor_space_name);
	option_state_dereference (&options, MDL);
	if (client -> packet_length < BOOTP_MIN_LEN)
		client -> packet_length = BOOTP_MIN_LEN;

	client -> packet.op = BOOTREQUEST;
	client -> packet.htype = client -> interface -> hw_address.hbuf [0];
	client -> packet.hlen = client -> interface -> hw_address.hlen - 1;
	client -> packet.hops = 0;
	client -> packet.xid = random ();
	client -> packet.secs = 0; /* filled in by send_discover. */

	if (can_receive_unicast_unconfigured (client -> interface))
		client -> packet.flags = 0;
	else
		client -> packet.flags = htons (BOOTP_BROADCAST);

	memset (&(client -> packet.ciaddr),
		0, sizeof client -> packet.ciaddr);
	memset (&(client -> packet.yiaddr),
		0, sizeof client -> packet.yiaddr);
	memset (&(client -> packet.siaddr),
		0, sizeof client -> packet.siaddr);
	client -> packet.giaddr = giaddr;
	if (client -> interface -> hw_address.hlen > 0)
	    memcpy (client -> packet.chaddr,
		    &client -> interface -> hw_address.hbuf [1],
		    (unsigned)(client -> interface -> hw_address.hlen - 1));

#ifdef DEBUG_PACKET
	dump_raw ((unsigned char *)&client -> packet, client -> packet_length);
#endif
}


void make_request (client, lease)
	struct client_state *client;
	struct client_lease *lease;
{
	unsigned char request = DHCPREQUEST;
	int i, j;
	unsigned char *tmp, *digest;
	unsigned char *old_digest_loc;
	struct option_cache *oc;

	memset (&client -> packet, 0, sizeof (client -> packet));

	if (client -> state == S_REQUESTING)
		oc = lookup_option (&dhcp_universe, lease -> options,
				    DHO_DHCP_SERVER_IDENTIFIER);
	else
		oc = (struct option_cache *)0;

	make_client_options (client, lease, &request, oc,
			     ((client -> state == S_REQUESTING ||
			       client -> state == S_REBOOTING)
			      ? &lease -> address
			      : (struct iaddr *)0),
			     client -> config -> requested_options,
			     &client -> sent_options);

	/* Set up the option buffer... */
	client -> packet_length =
		cons_options ((struct packet *)0, &client -> packet,
			      (struct lease *)0, client, 0,
			      (struct option_state *)0, client -> sent_options,
			      &global_scope, 0, 0, 0, (struct data_string *)0,
			      client -> config -> vendor_space_name);
	option_state_dereference (&client -> sent_options, MDL);
	if (client -> packet_length < BOOTP_MIN_LEN)
		client -> packet_length = BOOTP_MIN_LEN;

	client -> packet.op = BOOTREQUEST;
	client -> packet.htype = client -> interface -> hw_address.hbuf [0];
	client -> packet.hlen = client -> interface -> hw_address.hlen - 1;
	client -> packet.hops = 0;
	client -> packet.xid = client -> xid;
	client -> packet.secs = 0; /* Filled in by send_request. */

	/* If we own the address we're requesting, put it in ciaddr;
	   otherwise set ciaddr to zero. */
	if (client -> state == S_BOUND ||
	    client -> state == S_RENEWING ||
	    client -> state == S_REBINDING) {
		memcpy (&client -> packet.ciaddr,
			lease -> address.iabuf, lease -> address.len);
		client -> packet.flags = 0;
	} else {
		memset (&client -> packet.ciaddr, 0,
			sizeof client -> packet.ciaddr);
		if (can_receive_unicast_unconfigured (client -> interface))
			client -> packet.flags = 0;
		else
			client -> packet.flags = htons (BOOTP_BROADCAST);
	}

	memset (&client -> packet.yiaddr, 0,
		sizeof client -> packet.yiaddr);
	memset (&client -> packet.siaddr, 0,
		sizeof client -> packet.siaddr);
	if (client -> state != S_BOUND &&
	    client -> state != S_RENEWING)
		client -> packet.giaddr = giaddr;
	else
		memset (&client -> packet.giaddr, 0,
			sizeof client -> packet.giaddr);
	if (client -> interface -> hw_address.hlen > 0)
	    memcpy (client -> packet.chaddr,
		    &client -> interface -> hw_address.hbuf [1],
		    (unsigned)(client -> interface -> hw_address.hlen - 1));

#ifdef DEBUG_PACKET
	dump_raw ((unsigned char *)&client -> packet, client -> packet_length);
#endif
}

void make_decline (client, lease)
	struct client_state *client;
	struct client_lease *lease;
{
	unsigned char decline = DHCPDECLINE;
	int i;
	struct option_cache *oc;

	struct option_state *options = (struct option_state *)0;

	oc = lookup_option (&dhcp_universe, lease -> options,
			    DHO_DHCP_SERVER_IDENTIFIER);
	make_client_options (client, lease, &decline, oc,
			     &lease -> address, (u_int32_t *)0, &options);

	/* Set up the option buffer... */
	memset (&client -> packet, 0, sizeof (client -> packet));
	client -> packet_length =
		cons_options ((struct packet *)0, &client -> packet,
			      (struct lease *)0, client, 0,
			      (struct option_state *)0, options,
			      &global_scope, 0, 0, 0, (struct data_string *)0,
			      client -> config -> vendor_space_name);
	option_state_dereference (&options, MDL);
	if (client -> packet_length < BOOTP_MIN_LEN)
		client -> packet_length = BOOTP_MIN_LEN;
	option_state_dereference (&options, MDL);

	client -> packet.op = BOOTREQUEST;
	client -> packet.htype = client -> interface -> hw_address.hbuf [0];
	client -> packet.hlen = client -> interface -> hw_address.hlen - 1;
	client -> packet.hops = 0;
	client -> packet.xid = client -> xid;
	client -> packet.secs = 0; /* Filled in by send_request. */
	if (can_receive_unicast_unconfigured (client -> interface))
		client -> packet.flags = 0;
	else
		client -> packet.flags = htons (BOOTP_BROADCAST);

	/* ciaddr must always be zero. */
	memset (&client -> packet.ciaddr, 0,
		sizeof client -> packet.ciaddr);
	memset (&client -> packet.yiaddr, 0,
		sizeof client -> packet.yiaddr);
	memset (&client -> packet.siaddr, 0,
		sizeof client -> packet.siaddr);
	client -> packet.giaddr = giaddr;
	memcpy (client -> packet.chaddr,
		&client -> interface -> hw_address.hbuf [1],
		client -> interface -> hw_address.hlen);

#ifdef DEBUG_PACKET
	dump_raw ((unsigned char *)&client -> packet, client -> packet_length);
#endif
}

void make_release (client, lease)
	struct client_state *client;
	struct client_lease *lease;
{
	unsigned char request = DHCPRELEASE;
	int i;
	struct option_cache *oc;

	struct option_state *options = (struct option_state *)0;

	memset (&client -> packet, 0, sizeof (client -> packet));

	oc = lookup_option (&dhcp_universe, lease -> options,
			    DHO_DHCP_SERVER_IDENTIFIER);
	make_client_options (client, lease, &request, oc,
			     (struct iaddr *)0, (u_int32_t *)0,
			     &options);

	/* Set up the option buffer... */
	client -> packet_length =
		cons_options ((struct packet *)0, &client -> packet,
			      (struct lease *)0, client, 0,
			      (struct option_state *)0, options,
			      &global_scope, 0, 0, 0, (struct data_string *)0,
			      client -> config -> vendor_space_name);
	if (client -> packet_length < BOOTP_MIN_LEN)
		client -> packet_length = BOOTP_MIN_LEN;
	option_state_dereference (&options, MDL);

	client -> packet.op = BOOTREQUEST;
	client -> packet.htype = client -> interface -> hw_address.hbuf [0];
	client -> packet.hlen = client -> interface -> hw_address.hlen - 1;
	client -> packet.hops = 0;
	client -> packet.xid = random ();
	client -> packet.secs = 0;
	client -> packet.flags = 0;
	memcpy (&client -> packet.ciaddr,
		lease -> address.iabuf, lease -> address.len);
	memset (&client -> packet.yiaddr, 0,
		sizeof client -> packet.yiaddr);
	memset (&client -> packet.siaddr, 0,
		sizeof client -> packet.siaddr);
	client -> packet.giaddr = giaddr;
	memcpy (client -> packet.chaddr,
		&client -> interface -> hw_address.hbuf [1],
		client -> interface -> hw_address.hlen);

#ifdef DEBUG_PACKET
	dump_raw ((unsigned char *)&client -> packet, client -> packet_length);
#endif
}

void destroy_client_lease (lease)
	struct client_lease *lease;
{
	int i;

	if (lease -> server_name)
		dfree (lease -> server_name, MDL);
	if (lease -> filename)
		dfree (lease -> filename, MDL);
	option_state_dereference (&lease -> options, MDL);
	free_client_lease (lease, MDL);
}

FILE *leaseFile;

void rewrite_client_leases ()
{
	struct interface_info *ip;
	struct client_state *client;
	struct client_lease *lp;

	if (leaseFile)
		fclose (leaseFile);
	leaseFile = fopen (path_dhclient_db, "w");
	if (!leaseFile) {
		log_error ("can't create %s: %m", path_dhclient_db);
		return;
	}

	/* Write out all the leases attached to configured interfaces that
	   we know about. */
	for (ip = interfaces; ip; ip = ip -> next) {
		for (client = ip -> client; client; client = client -> next) {
			for (lp = client -> leases; lp; lp = lp -> next) {
				write_client_lease (client, lp, 1, 0);
			}
			if (client -> active)
				write_client_lease (client,
						    client -> active, 1, 0);
		}
	}

	/* Write out any leases that are attached to interfaces that aren't
	   currently configured. */
	for (ip = dummy_interfaces; ip; ip = ip -> next) {
		for (client = ip -> client; client; client = client -> next) {
			for (lp = client -> leases; lp; lp = lp -> next) {
				write_client_lease (client, lp, 1, 0);
			}
			if (client -> active)
				write_client_lease (client,
						    client -> active, 1, 0);
		}
	}
	fflush (leaseFile);
}

void write_lease_option (struct option_cache *oc,
			 struct packet *packet, struct lease *lease,
			 struct client_state *client_state,
			 struct option_state *in_options,
			 struct option_state *cfg_options,
			 struct binding_scope **scope,
			 struct universe *u, void *stuff)
{
	const char *name, *dot;
	struct data_string ds;
	int status;
	struct client_state *client;

	memset (&ds, 0, sizeof ds);

	if (u != &dhcp_universe) {
		name = u -> name;
		dot = ".";
	} else {
		name = "";
		dot = "";
	}
	if (evaluate_option_cache (&ds, packet, lease, client_state,
				   in_options, cfg_options, scope, oc, MDL)) {
		fprintf (leaseFile,
			 "  option %s%s%s %s;\n",
			 name, dot, oc -> option -> name,
			 pretty_print_option (oc -> option,
					      ds.data, ds.len, 1, 1));
		data_string_forget (&ds, MDL);
	}
}

int write_client_lease (client, lease, rewrite, makesure)
	struct client_state *client;
	struct client_lease *lease;
	int rewrite;
	int makesure;
{
	int i;
	struct tm *t;
	static int leases_written;
	struct option_cache *oc;
	struct data_string ds;
	pair *hash;
	int errors = 0;
	char *s;

	if (!rewrite) {
		if (leases_written++ > 20) {
			rewrite_client_leases ();
			leases_written = 0;
		}
	}

	/* If the lease came from the config file, we don't need to stash
	   a copy in the lease database. */
	if (lease -> is_static)
		return 1;

	if (!leaseFile) {	/* XXX */
		leaseFile = fopen (path_dhclient_db, "w");
		if (!leaseFile) {
			log_error ("can't create %s: %m", path_dhclient_db);
			return 0;
		}
	}

	errno = 0;
	fprintf (leaseFile, "lease {\n");
	if (lease -> is_bootp) {
		fprintf (leaseFile, "  bootp;\n");
		if (errno) {
			++errors;
			errno = 0;
		}
	}
	fprintf (leaseFile, "  interface \"%s\";\n",
		 client -> interface -> name);
	if (errno) {
		++errors;
		errno = 0;
	}
	if (client -> name) {
		fprintf (leaseFile, "  name \"%s\";\n", client -> name);
		if (errno) {
			++errors;
			errno = 0;
		}
	}
	fprintf (leaseFile, "  fixed-address %s;\n",
		 piaddr (lease -> address));
	if (errno) {
		++errors;
		errno = 0;
	}
	if (lease -> filename) {
		s = quotify_string (lease -> filename, MDL);
		if (s) {
			fprintf (leaseFile, "  filename \"%s\";\n", s);
			if (errno) {
				++errors;
				errno = 0;
			}
			dfree (s, MDL);
		} else
			errors++;

	}
	if (lease -> server_name) {
		s = quotify_string (lease -> filename, MDL);
		if (s) {
			fprintf (leaseFile, "  server-name \"%s\";\n", s);
			if (errno) {
				++errors;
				errno = 0;
			}
			dfree (s, MDL);
		} else
			++errors;
	}
	if (lease -> medium) {
		s = quotify_string (lease -> medium -> string, MDL);
		if (s) {
			fprintf (leaseFile, "  medium \"%s\";\n", s);
			if (errno) {
				++errors;
				errno = 0;
			}
			dfree (s, MDL);
		} else
			errors++;
	}
	if (errno != 0) {
		errors++;
		errno = 0;
	}

	memset (&ds, 0, sizeof ds);

	for (i = 0; i < lease -> options -> universe_count; i++) {
		option_space_foreach ((struct packet *)0, (struct lease *)0,
				      client, (struct option_state *)0,
				      lease -> options, &global_scope,
				      universes [i],
				      client, write_lease_option);
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
	if (errno != 0) {
		errors++;
		errno = 0;
	}
	t = gmtime (&lease -> rebind);
	fprintf (leaseFile,
		 "  rebind %d %d/%d/%d %02d:%02d:%02d;\n",
		 t -> tm_wday, t -> tm_year + 1900,
		 t -> tm_mon + 1, t -> tm_mday,
		 t -> tm_hour, t -> tm_min, t -> tm_sec);
	if (errno != 0) {
		errors++;
		errno = 0;
	}
	t = gmtime (&lease -> expiry);
	fprintf (leaseFile,
		 "  expire %d %d/%d/%d %02d:%02d:%02d;\n",
		 t -> tm_wday, t -> tm_year + 1900,
		 t -> tm_mon + 1, t -> tm_mday,
		 t -> tm_hour, t -> tm_min, t -> tm_sec);
	if (errno != 0) {
		errors++;
		errno = 0;
	}
	fprintf (leaseFile, "}\n");
	if (errno != 0) {
		errors++;
		errno = 0;
	}
	fflush (leaseFile);
	if (errno != 0) {
		errors++;
		errno = 0;
	}
	if (!errors && makesure) {
		if (fsync (fileno (leaseFile)) < 0) {
			log_info ("write_client_lease: %m");
			return 0;
		}
	}
	return errors ? 0 : 1;
}

/* Variables holding name of script and file pointer for writing to
   script.   Needless to say, this is not reentrant - only one script
   can be invoked at a time. */
char scriptName [256];
FILE *scriptFile;

void script_init (client, reason, medium)
	struct client_state *client;
	const char *reason;
	struct string_list *medium;
{
	struct string_list *sl, *next;

	if (client) {
		for (sl = client -> env; sl; sl = next) {
			next = sl -> next;
			dfree (sl, MDL);
		}
		client -> env = (struct string_list *)0;
		client -> envc = 0;
		
		if (client -> interface) {
			client_envadd (client, "", "interface", "%s",
				       client -> interface -> name);
		}
		if (client -> name)
			client_envadd (client,
				       "", "client", "%s", client -> name);
		if (medium)
			client_envadd (client,
				       "", "medium", "%s", medium -> string);

		client_envadd (client, "", "reason", "%s", reason);
		client_envadd (client, "", "pid", "%ld", (long int)getpid ());
	}
}

struct envadd_state {
	struct client_state *client;
	const char *prefix;
};

void client_option_envadd (struct option_cache *oc,
			   struct packet *packet, struct lease *lease,
			   struct client_state *client_state,
			   struct option_state *in_options,
			   struct option_state *cfg_options,
			   struct binding_scope **scope,
			   struct universe *u, void *stuff)
{
	struct envadd_state *es = stuff;
	struct data_string data;
	memset (&data, 0, sizeof data);

	if (evaluate_option_cache (&data, packet, lease, client_state,
				   in_options, cfg_options, scope, oc, MDL)) {
		if (data.len) {
			char name [256];
			if (dhcp_option_ev_name (name, sizeof name,
						 oc -> option)) {
				client_envadd (es -> client, es -> prefix,
					       name, "%s",
					       (pretty_print_option
						(oc -> option,
						 data.data, data.len,
						 0, 0)));
				data_string_forget (&data, MDL);
			}
		}
	}
}

void script_write_params (client, prefix, lease)
	struct client_state *client;
	const char *prefix;
	struct client_lease *lease;
{
	int i;
	struct data_string data;
	struct option_cache *oc;
	pair *hash;
	char *s, *t;
	struct envadd_state es;

	es.client = client;
	es.prefix = prefix;

	client_envadd (client,
		       prefix, "ip_address", "%s", piaddr (lease -> address));

	/* For the benefit of Linux (and operating systems which may
	   have similar needs), compute the network address based on
	   the supplied ip address and netmask, if provided.  Also
	   compute the broadcast address (the host address all ones
	   broadcast address, not the host address all zeroes
	   broadcast address). */

	memset (&data, 0, sizeof data);
	oc = lookup_option (&dhcp_universe, lease -> options, DHO_SUBNET_MASK);
	if (oc && evaluate_option_cache (&data, (struct packet *)0,
					 (struct lease *)0, client,
					 (struct option_state *)0,
					 lease -> options,
					 &global_scope, oc, MDL)) {
		if (data.len > 3) {
			struct iaddr netmask, subnet, broadcast;

			memcpy (netmask.iabuf, data.data, data.len);
			netmask.len = data.len;
			data_string_forget (&data, MDL);

			subnet = subnet_number (lease -> address, netmask);
			if (subnet.len) {
			    client_envadd (client, prefix, "network_number",
					   "%s", piaddr (subnet));

			    oc = lookup_option (&dhcp_universe,
						lease -> options,
						DHO_BROADCAST_ADDRESS);
			    if (!oc ||
				!(evaluate_option_cache
				  (&data, (struct packet *)0,
				   (struct lease *)0, client,
				   (struct option_state *)0,
				   lease -> options,
				   &global_scope, oc, MDL))) {
				broadcast = broadcast_addr (subnet, netmask);
				if (broadcast.len) {
				    client_envadd (client,
						   prefix, "broadcast_address",
						   "%s", piaddr (broadcast));
				}
			    }
			}
		}
		data_string_forget (&data, MDL);
	}

	if (lease -> filename)
		client_envadd (client,
			       prefix, "filename", "%s", lease -> filename);
	if (lease -> server_name)
		client_envadd (client, prefix, "server_name",
			       "%s", lease -> server_name);

	for (i = 0; i < lease -> options -> universe_count; i++) {
		option_space_foreach ((struct packet *)0, (struct lease *)0,
				      client, (struct option_state *)0,
				      lease -> options, &global_scope,
				      universes [i],
				      &es, client_option_envadd);
	}
	client_envadd (client, prefix, "expiry", "%d", (int)(lease -> expiry));
}

int script_go (client)
	struct client_state *client;
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

	if (client)
		scriptName = client -> config -> script_name;
	else
		scriptName = top_level_config.script_name;

	envp = dmalloc (((client ? client -> envc : 2) +
			 client_env_count + 2) * sizeof (char *), MDL);
	if (!envp) {
		log_error ("No memory for client script environment.");
		return 0;
	}
	i = 0;
	/* Copy out the environment specified on the command line,
	   if any. */
	for (sp = client_env; sp; sp = sp -> next) {
		envp [i++] = sp -> string;
	}
	/* Copy out the environment specified by dhclient. */
	if (client) {
		for (sp = client -> env; sp; sp = sp -> next) {
			envp [i++] = sp -> string;
		}
	} else {
		envp [i++] = reason;
	}
	/* Set $PATH. */
	envp [i++] = client_path;
	envp [i] = (char *)0;

	argv [0] = scriptName;
	argv [1] = (char *)0;

	pid = fork ();
	if (pid < 0) {
		log_error ("fork: %m");
		wstatus = 0;
	} else if (pid) {
		do {
			wpid = wait (&wstatus);
		} while (wpid != pid && wpid > 0);
		if (wpid < 0) {
			log_error ("wait: %m");
			wstatus = 0;
		}
	} else {
		if ((i = open(_PATH_DEVNULL, O_RDWR)) != -1) {
			dup2(i, STDIN_FILENO);
			dup2(i, STDOUT_FILENO);
			dup2(i, STDERR_FILENO);
			if (i > STDERR_FILENO)
				close(i);
		}
		execve (scriptName, argv, envp);
		log_error ("execve (%s, ...): %m", scriptName);
		exit (0);
	}

	if (client) {
		for (sp = client -> env; sp; sp = next) {
			next = sp -> next;
			dfree (sp, MDL);
		}
		client -> env = (struct string_list *)0;
		client -> envc = 0;
	}
	dfree (envp, MDL);
	GET_TIME (&cur_time);
	return (WIFEXITED (wstatus) ?
		WEXITSTATUS (wstatus) : -WTERMSIG (wstatus));
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
		       len + sizeof *val, MDL);
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
	int i, j;
	const char *s;

	j = 0;
	if (option -> universe != &dhcp_universe) {
		s = option -> universe -> name;
		i = 0;
	} else { 
		s = option -> name;
		i = 1;
	}

	do {
		while (*s) {
			if (j + 1 == buflen)
				return 0;
			if (*s == '-')
				buf [j++] = '_';
			else
				buf [j++] = *s;
			++s;
		}
		if (!i) {
			s = option -> name;
			if (j + 1 == buflen)
				return 0;
			buf [j++] = '_';
		}
		++i;
	} while (i != 2);

	buf [j] = 0;
	return 1;
}

void go_daemon ()
{
	static int state = 0;
	int pid;
	int i;

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
		log_fatal ("Can't fork daemon: %m");
	else if (pid)
		exit (0);
	/* Become session leader and get pid... */
	pid = setsid ();

	/* Close standard I/O descriptors. */
        close(0);
        close(1);
        close(2);

	/* Reopen them on /dev/null. */
	i = open ("/dev/null", O_RDWR);
	if (i == 0)
		i = open ("/dev/null", O_RDWR);
	if (i == 1) {
		i = open ("/dev/null", O_RDWR);
		log_perror = 0; /* No sense logging to /dev/null. */
	} else if (i != -1)
		close (i);

	write_client_pid_file ();
}

void write_client_pid_file ()
{
	FILE *pf;
	int pfdesc;

	pfdesc = open (path_dhclient_pid, O_CREAT | O_TRUNC | O_WRONLY, 0644);

	if (pfdesc < 0) {
		log_error ("Can't create %s: %m", path_dhclient_pid);
		return;
	}

	pf = fdopen (pfdesc, "w");
	if (!pf)
		log_error ("Can't fdopen %s: %m", path_dhclient_pid);
	else {
		fprintf (pf, "%ld\n", (long)getpid ());
		fclose (pf);
	}
}

void client_location_changed ()
{
	struct interface_info *ip;
	struct client_state *client;

	for (ip = interfaces; ip; ip = ip -> next) {
		for (client = ip -> client; client; client = client -> next) {
			switch (client -> state) {
			      case S_SELECTING:
				cancel_timeout (send_discover, client);
				break;

			      case S_BOUND:
				cancel_timeout (state_bound, client);
				break;

			      case S_REBOOTING:
			      case S_REQUESTING:
			      case S_RENEWING:
				cancel_timeout (send_request, client);
				break;

			      case S_INIT:
			      case S_REBINDING:
			      case S_STOPPED:
				break;
			}
			client -> state = S_INIT;
			if (interface_active(ip))
				state_reboot(client);
		}
	}
}

void do_release(client) 
	struct client_state *client;
{
	struct data_string ds;
	struct option_cache *oc;

	/* Pick a random xid. */
	client -> xid = random ();

	/* is there even a lease to release? */
	if (client -> active) {
		/* Make a DHCPRELEASE packet, and set appropriate per-interface
		   flags. */
		make_release (client, client -> active);

		memset (&ds, 0, sizeof ds);
		oc = lookup_option (&dhcp_universe,
				    client -> active -> options,
				    DHO_DHCP_SERVER_IDENTIFIER);
		if (oc &&
		    evaluate_option_cache (&ds, (struct packet *)0,
					   (struct lease *)0, client,
					   (struct option_state *)0,
					   client -> active -> options,
					   &global_scope, oc, MDL)) {
			if (ds.len > 3) {
				memcpy (client -> destination.iabuf,
					ds.data, 4);
				client -> destination.len = 4;
			} else
				client -> destination = iaddr_broadcast;
		} else
			client -> destination = iaddr_broadcast;
		client -> first_sending = cur_time;
		client -> interval = client -> config -> initial_interval;
	
		/* Zap the medium list... */
		client -> medium = (struct string_list *)0;
	
		/* Send out the first and only DHCPRELEASE packet. */
		send_release (client);

		/* Do the client script RELEASE operation. */
		script_init (client,
			     "RELEASE", (struct string_list *)0);
		if (client -> alias)
			script_write_params (client, "alias_",
					     client -> alias);
		script_write_params (client, "old_", client -> active);
		script_go (client);
	}

	/* Cancel any timeouts. */
	cancel_timeout (state_bound, client);
	cancel_timeout (send_discover, client);
	cancel_timeout (state_init, client);
	cancel_timeout (send_request, client);
	cancel_timeout (state_reboot, client);
	client -> state = S_STOPPED;
}

int dhclient_interface_shutdown_hook (struct interface_info *interface)
{
	do_release (interface -> client);

	return 1;
}

int dhclient_interface_discovery_hook (struct interface_info *tmp)
{
	struct interface_info *last, *ip;
	/* See if we can find the client from dummy_interfaces */
	last = 0;
	for (ip = dummy_interfaces; ip; ip = ip -> next) {
		if (!strcmp (ip -> name, tmp -> name)) {
			/* Remove from dummy_interfaces */
			if (last) {
				ip = (struct interface_info *)0;
				interface_reference (&ip, last -> next, MDL);
				interface_dereference (&last -> next, MDL);
				if (ip -> next) {
					interface_reference (&last -> next,
							     ip -> next, MDL);
					interface_dereference (&ip -> next,
							       MDL);
				}
			} else {
				ip = (struct interface_info *)0;
				interface_reference (&ip,
						     dummy_interfaces, MDL);
				interface_dereference (&dummy_interfaces, MDL);
				if (ip -> next) {
					interface_reference (&dummy_interfaces,
							     ip -> next, MDL);
					interface_dereference (&ip -> next,
							       MDL);
				}
			}
			/* Copy "client" to tmp */
			if (ip -> client) {
				tmp -> client = ip -> client;
				tmp -> client -> interface = tmp;
			}
			interface_dereference (&ip, MDL);
			break;
		}
		last = ip;
	}
	return 1;
}

isc_result_t dhclient_interface_startup_hook (struct interface_info *interface)
{
	struct interface_info *ip;
	struct client_state *client;

	/* This code needs some rethinking.   It doesn't test against
	   a signal name, and it just kind of bulls into doing something
	   that may or may not be appropriate. */

	if (interfaces) {
		interface_reference (&interface -> next, interfaces, MDL);
		interface_dereference (&interfaces, MDL);
	}
	interface_reference (&interfaces, interface, MDL);

	discover_interfaces (DISCOVER_UNCONFIGURED);

	for (ip = interfaces; ip; ip = ip -> next) {
		/* If interfaces were specified, don't configure
		   interfaces that weren't specified! */
		if (ip -> flags & INTERFACE_RUNNING ||
		   (ip -> flags & (INTERFACE_REQUESTED |
				     INTERFACE_AUTOMATIC)) !=
		     INTERFACE_REQUESTED)
			continue;
		script_init (ip -> client,
			     "PREINIT", (struct string_list *)0);
		if (ip -> client -> alias)
			script_write_params (ip -> client, "alias_",
					     ip -> client -> alias);
		script_go (ip -> client);
	}
	
	discover_interfaces (interfaces_requested
			     ? DISCOVER_REQUESTED
			     : DISCOVER_RUNNING);

	for (ip = interfaces; ip; ip = ip -> next) {
		if (ip -> flags & INTERFACE_RUNNING)
			continue;
		ip -> flags |= INTERFACE_RUNNING;
		for (client = ip -> client; client; client = client -> next) {
			client -> state = S_INIT;
			/* Set up a timeout to start the initialization
			   process. */
			if (interface_active(ip)) {
				add_timeout(cur_time + random () % 5,
					     state_reboot, client, 0, 0);
			}
		}
	}
	return ISC_R_SUCCESS;
}

/* The client should never receive a relay agent information option,
   so if it does, log it and discard it. */

int parse_agent_information_option (packet, len, data)
	struct packet *packet;
	int len;
	u_int8_t *data;
{
	return 1;
}

/* The client never sends relay agent information options. */

unsigned cons_agent_information_options (cfg_options, outpacket,
					 agentix, length)
	struct option_state *cfg_options;
	struct dhcp_packet *outpacket;
	unsigned agentix;
	unsigned length;
{
	return length;
}

static void shutdown_exit (void *foo)
{
	exit (0);
}

isc_result_t dhcp_set_control_state (control_object_state_t oldstate,
				     control_object_state_t newstate)
{
	struct interface_info *ip;
	struct client_state *client;

	/* Do the right thing for each interface. */
	for (ip = interfaces; ip; ip = ip -> next) {
	    for (client = ip -> client; client; client = client -> next) {
		switch (newstate) {
		  case server_startup:
		    return ISC_R_SUCCESS;

		  case server_running:
		    return ISC_R_SUCCESS;

		  case server_shutdown:
		    if (client -> active &&
			client -> active -> expiry > cur_time) {
			    if (client -> config -> do_forward_update)
				    client_dns_update (client, 0, 0);
			    do_release (client);
		    }
		    break;

		  case server_hibernate:
		    state_stop (client);
		    break;

		  case server_awaken:
		    if (interface_active(ip))
			    state_reboot(client);
		    break;
		}
	    }
	}
	if (newstate == server_shutdown)
		add_timeout (cur_time + 1, shutdown_exit, 0, 0, 0);
	return ISC_R_SUCCESS;
}

/* Called after a timeout if the DNS update failed on the previous try.
   Retries the update, and if it times out, schedules a retry after
   ten times as long of a wait. */

void client_dns_update_timeout (void *cp)
{
	struct client_state *client = cp;
	isc_result_t status;

	if (client -> active) {
		status = client_dns_update (client, 1,
					    (client -> active -> renewal -
					     cur_time));
		if (status == ISC_R_TIMEDOUT) {
			client -> dns_update_timeout *= 10;
			add_timeout (cur_time + client -> dns_update_timeout,
				     client_dns_update_timeout, client, 0, 0);
		}
	}
}
			
/* See if we should do a DNS update, and if so, do it. */

isc_result_t client_dns_update (struct client_state *client, int addp, int ttl)
{
	struct data_string ddns_fqdn, ddns_fwd_name,
	       ddns_dhcid, client_identifier;
	struct option_cache *oc;
	int ignorep;
	int result;
	isc_result_t rcode;

	/* If we didn't send an FQDN option, we certainly aren't going to
	   be doing an update. */
	if (!client -> sent_options)
		return ISC_R_SUCCESS;

	/* If we don't have a lease, we can't do an update. */
	if (!client -> active)
		return ISC_R_SUCCESS;

	/* If we set the no client update flag, don't do the update. */
	if ((oc = lookup_option (&fqdn_universe, client -> sent_options,
				  FQDN_NO_CLIENT_UPDATE)) &&
	    evaluate_boolean_option_cache (&ignorep, (struct packet *)0,
					   (struct lease *)0, client,
					   client -> sent_options,
					   (struct option_state *)0,
					   &global_scope, oc, MDL))
		return ISC_R_SUCCESS;
	
	/* If we set the "server, please update" flag, or didn't set it
	   to false, don't do the update. */
	if (!(oc = lookup_option (&fqdn_universe, client -> sent_options,
				  FQDN_SERVER_UPDATE)) ||
	    evaluate_boolean_option_cache (&ignorep, (struct packet *)0,
					   (struct lease *)0, client,
					   client -> sent_options,
					   (struct option_state *)0,
					   &global_scope, oc, MDL))
		return ISC_R_SUCCESS;
	
	/* If no FQDN option was supplied, don't do the update. */
	memset (&ddns_fwd_name, 0, sizeof ddns_fwd_name);
	if (!(oc = lookup_option (&fqdn_universe, client -> sent_options,
				  FQDN_FQDN)) ||
	    !evaluate_option_cache (&ddns_fwd_name, (struct packet *)0, 
				    (struct lease *)0, client,
				    client -> sent_options,
				    (struct option_state *)0,
				    &global_scope, oc, MDL))
		return ISC_R_SUCCESS;

	/* Make a dhcid string out of either the client identifier,
	   if we are sending one, or the interface's MAC address,
	   otherwise. */
	memset (&ddns_dhcid, 0, sizeof ddns_dhcid);

	memset (&client_identifier, 0, sizeof client_identifier);
	if ((oc = lookup_option (&dhcp_universe, client -> sent_options,
				 DHO_DHCP_CLIENT_IDENTIFIER)) &&
	    evaluate_option_cache (&client_identifier, (struct packet *)0, 
				   (struct lease *)0, client,
				   client -> sent_options,
				   (struct option_state *)0,
				   &global_scope, oc, MDL)) {
		result = get_dhcid (&ddns_dhcid,
				    DHO_DHCP_CLIENT_IDENTIFIER,
				    client_identifier.data,
				    client_identifier.len);
		data_string_forget (&client_identifier, MDL);
	} else
		result = get_dhcid (&ddns_dhcid, 0,
				    client -> interface -> hw_address.hbuf,
				    client -> interface -> hw_address.hlen);
	if (!result) {
		data_string_forget (&ddns_fwd_name, MDL);
		return ISC_R_SUCCESS;
	}

	/* Start the resolver, if necessary. */
	if (!resolver_inited) {
		minires_ninit (&resolver_state);
		resolver_inited = 1;
		resolver_state.retrans = 1;
		resolver_state.retry = 1;
	}

	/*
	 * Perform updates.
	 */
	if (ddns_fwd_name.len && ddns_dhcid.len) {
		if (addp)
			rcode = ddns_update_a (&ddns_fwd_name,
					       client -> active -> address,
					       &ddns_dhcid, ttl,
					       1);
		else
			rcode = ddns_remove_a (&ddns_fwd_name,
					       client -> active -> address,
					       &ddns_dhcid);
	}
	
	data_string_forget (&ddns_fwd_name, MDL);
	data_string_forget (&ddns_dhcid, MDL);
	return rcode;
}

/* Check to see if there's a wire plugged in */
int
interface_active(struct interface_info *ip) {
#ifdef __FreeBSD__
	struct ifmediareq ifmr;
	int *media_list, i;
	char *ifname;
	int sock;

	ifname = ip -> name;

	if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
		log_fatal("Can't create interface_active socket");

	(void) memset(&ifmr, 0, sizeof(ifmr));
	(void) strncpy(ifmr.ifm_name, ifname, sizeof(ifmr.ifm_name));

	if (ioctl(sock, SIOCGIFMEDIA, (caddr_t)&ifmr) < 0) {
		/*
		 * Interface doesn't support SIOCGIFMEDIA, presume okay
		 */
		close(sock);
		return (1);
	}
	close(sock);

	if (ifmr.ifm_count == 0) {
		/*
		 * this is unexpected (to me), but we'll just assume
		 * that this means interface does not support SIOCGIFMEDIA
		 */
		log_fatal("%s: no media types?", ifname);
		return (1);
	}

	if (ifmr.ifm_status & IFM_AVALID) {
		if (ip->ieee802) {
			if ((IFM_TYPE(ifmr.ifm_active) == IFM_IEEE80211) &&
			     (ifmr.ifm_status & IFM_ACTIVE))
				return (1);
		} else {
			if (ifmr.ifm_status & IFM_ACTIVE)
				return (1);
		}
	}

	return (0);
#else /* ifdef __FreeBSD__ */

	return (1);
#endif /* Other OSs */
}

#ifdef __FreeBSD__
set_ieee802 (struct interface_info *ip) {

	struct ieee80211req     ireq;
	u_int8_t                data[32];
	int                     associated = 0;
	int *media_list, i;
	char *ifname;
	int sock;

	ifname = ip -> name;

	if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
		log_fatal("Can't create interface_active socket");

	(void) memset(&ireq, 0, sizeof(ireq));
	(void) strncpy(ireq.i_name, ifname, sizeof(ireq.i_name));
	ireq.i_data = &data;
	ireq.i_type = IEEE80211_IOC_SSID;
	ireq.i_val = -1;
	/*
	 * If we can't get the SSID,
	 * this isn't an 802.11 device.
	 */
	if (ioctl(sock, SIOCG80211, &ireq) < 0)
		ip->ieee802 = 0;
	else {
#ifdef DEBUG
		printf("Device %s has 802.11\n", ifname);
#endif
		ip->ieee802 = 1;
	}
	close(sock);
}
#endif /* __FreeBSD__ */

#ifdef ENABLE_POLLING_MODE
/* Check the state of the NICs if we have link */
void state_link (cpp)
        void *cpp;
{
	struct interface_info *ip;
	struct client_state *client;

#ifdef DEBUG
	printf("Polling interface status\n");
#endif
	for (ip = interfaces; ip; ip = ip -> next) {
		if (ip->linkstatus == 0 || doinitcheck == 0) {
			if (interface_active(ip)) {
#ifdef DEBUG
				printf("%s: Found Link on interface\n", ip->name);
#endif
				for (client = ip -> client;
				     client; client = client -> next) {
					add_timeout(cur_time + random () % 5,
					             state_reboot, client, 0, 0);
				}
				ip->linkstatus = 1;
			} else {
#ifdef DEBUG
				printf("%s: No Link on interface\n", ip->name);
#endif
				for (client = ip -> client;
				     client; client = client -> next) {
					cancel_timeout(state_init, client);
			 		cancel_timeout(send_discover, client);
					cancel_timeout(send_request, client);
					/*
					 * XXX without this, dhclient does
					 * not poll on a interface if there
					 * is no cable plugged in at startup
					 * time
					 */
					if (client -> state == S_INIT) {
						add_timeout(cur_time + polling_interval,
						             state_link, client, 0, 0);
					}
			 	}
				ip->linkstatus = 0;
			}
		} else {
			if (interface_active(ip) == 0) {
#ifdef DEBUG
				printf("%s: Lost Link on interface\n", ip->name);
#endif
				ip->linkstatus = 0;
			}
		}
	}
	if (doinitcheck)
		go_daemon ();
	doinitcheck = 1;
}
#endif /* ifdef ENABLE_POLLING_MODE */
