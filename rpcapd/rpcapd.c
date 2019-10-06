/*
 * Copyright (c) 2002 - 2003
 * NetGroup, Politecnico di Torino (Italy)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Politecnico di Torino nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ftmacros.h"

#include <errno.h>		// for the errno variable
#include <string.h>		// for strtok, etc
#include <stdlib.h>		// for malloc(), free(), ...
#include <pcap.h>		// for PCAP_ERRBUF_SIZE
#include <signal.h>		// for signal()

#include "fmtutils.h"
#include "sockutils.h"		// for socket calls
#include "varattrs.h"		// for _U_
#include "portability.h"
#include "rpcapd.h"
#include "config_params.h"	// configuration file parameters
#include "fileconf.h"		// for the configuration file management
#include "rpcap-protocol.h"
#include "daemon.h"		// the true main() method of this daemon
#include "log.h"

#ifdef _WIN32
  #include <process.h>		// for thread stuff
  #include "win32-svc.h"	// for Win32 service stuff
  #include "getopt.h"		// for getopt()-for-Windows
#else
  #include <fcntl.h>		// for open()
  #include <unistd.h>		// for exit()
  #include <sys/wait.h>		// waitpid()
#endif

//
// Element in list of sockets on which we're listening for connections.
//
struct listen_sock {
	struct listen_sock *next;
	SOCKET sock;
};

// Global variables
char hostlist[MAX_HOST_LIST + 1];		//!< Keeps the list of the hosts that are allowed to connect to this server
struct active_pars activelist[MAX_ACTIVE_LIST];	//!< Keeps the list of the hosts (host, port) on which I want to connect to (active mode)
int nullAuthAllowed;				//!< '1' if we permit NULL authentication, '0' otherwise
static struct listen_sock *listen_socks;	//!< sockets on which we listen
char loadfile[MAX_LINE + 1];			//!< Name of the file from which we have to load the configuration
static int passivemode = 1;			//!< '1' if we want to run in passive mode as well
static struct addrinfo mainhints;		//!< temporary struct to keep settings needed to open the new socket
static char address[MAX_LINE + 1];		//!< keeps the network address (either numeric or literal) to bind to
static char port[MAX_LINE + 1];			//!< keeps the network port to bind to
#ifdef _WIN32
static HANDLE state_change_event;		//!< event to signal that a state change should take place
#endif
static volatile sig_atomic_t shutdown_server;	//!< '1' if the server is to shut down
static volatile sig_atomic_t reread_config;	//!< '1' if the server is to re-read its configuration

extern char *optarg;	// for getopt()

// Function definition
#ifdef _WIN32
static unsigned __stdcall main_active(void *ptr);
static BOOL WINAPI main_ctrl_event(DWORD);
#else
static void *main_active(void *ptr);
static void main_terminate(int sign);
static void main_reread_config(int sign);
#endif
static void accept_connections(void);
static void accept_connection(SOCKET listen_sock);
#ifndef _WIN32
static void main_reap_children(int sign);
#endif
#ifdef _WIN32
static unsigned __stdcall main_passive_serviceloop_thread(void *ptr);
#endif

#define RPCAP_ACTIVE_WAIT 30		/* Waiting time between two attempts to open a connection, in active mode (default: 30 sec) */

/*!
	\brief Prints the usage screen if it is launched in console mode.
*/
static void printusage(void)
{
	const char *usagetext =
	"USAGE:"
	" "  PROGRAM_NAME " [-b <address>] [-p <port>] [-4] [-l <host_list>] [-a <host,port>]\n"
	"              [-n] [-v] [-d] "
#ifndef _WIN32
	"[-i] "
#endif
        "[-D] [-s <config_file>] [-f <config_file>]\n\n"
	"  -b <address>    the address to bind to (either numeric or literal).\n"
	"                  Default: binds to all local IPv4 and IPv6 addresses\n\n"
	"  -p <port>       the port to bind to.\n"
	"                  Default: binds to port " RPCAP_DEFAULT_NETPORT "\n\n"
	"  -4              use only IPv4.\n"
	"                  Default: use both IPv4 and IPv6 waiting sockets\n\n"
	"  -l <host_list>  a file that contains a list of hosts that are allowed\n"
	"                  to connect to this server (if more than one, list them one\n"
	"                  per line).\n"
	"                  We suggest to use literal names (instead of numeric ones)\n"
	"                  in order to avoid problems with different address families.\n\n"
	"  -n              permit NULL authentication (usually used with '-l')\n\n"
	"  -a <host,port>  run in active mode when connecting to 'host' on port 'port'\n"
	"                  In case 'port' is omitted, the default port (" RPCAP_DEFAULT_NETPORT_ACTIVE ") is used\n\n"
	"  -v              run in active mode only (default: if '-a' is specified, it\n"
	"                  accepts passive connections as well)\n\n"
	"  -d              run in daemon mode (UNIX only) or as a service (Win32 only)\n"
	"                  Warning (Win32): this switch is provided automatically when\n"
	"                  the service is started from the control panel\n\n"
#ifndef _WIN32
	"  -i              run in inetd mode (UNIX only)\n\n"
#endif
	"  -D              log debugging messages\n\n"
	"  -s <config_file> save the current configuration to file\n\n"
	"  -f <config_file> load the current configuration from file; all switches\n"
	"                  specified from the command line are ignored\n\n"
	"  -h              print this help screen\n\n";

	(void)fprintf(stderr, "RPCAPD, a remote packet capture daemon.\n"
	"Compiled with %s\n\n", pcap_lib_version());
	printf("%s", usagetext);
}



//! Program main
int main(int argc, char *argv[])
{
	char savefile[MAX_LINE + 1];		// name of the file on which we have to save the configuration
	int log_to_systemlog = 0;		// Non-zero if we should log to the "system log" rather than the standard error
	int isdaemon = 0;			// Non-zero if the user wants to run this program as a daemon
#ifndef _WIN32
	int isrunbyinetd = 0;			// Non-zero if this is being run by inetd or something inetd-like
#endif
	int log_debug_messages = 0;		// Non-zero if the user wants debug messages logged
	int retval;				// keeps the returning value from several functions
	char errbuf[PCAP_ERRBUF_SIZE + 1];	// keeps the error string, prior to be printed
#ifndef _WIN32
	struct sigaction action;
#endif

	savefile[0] = 0;
	loadfile[0] = 0;
	hostlist[0] = 0;

	// Initialize errbuf
	memset(errbuf, 0, sizeof(errbuf));

	strncpy(address, RPCAP_DEFAULT_NETADDR, MAX_LINE);
	strncpy(port, RPCAP_DEFAULT_NETPORT, MAX_LINE);

	// Prepare to open a new server socket
	memset(&mainhints, 0, sizeof(struct addrinfo));

	mainhints.ai_family = PF_UNSPEC;
	mainhints.ai_flags = AI_PASSIVE;	// Ready to a bind() socket
	mainhints.ai_socktype = SOCK_STREAM;

	// Getting the proper command line options
	while ((retval = getopt(argc, argv, "b:dDhip:4l:na:s:f:v")) != -1)
	{
		switch (retval)
		{
			case 'D':
				log_debug_messages = 1;
				rpcapd_log_set(log_to_systemlog, log_debug_messages);
				break;
			case 'b':
				strncpy(address, optarg, MAX_LINE);
				break;
			case 'p':
				strncpy(port, optarg, MAX_LINE);
				break;
			case '4':
				mainhints.ai_family = PF_INET;		// IPv4 server only
				break;
			case 'd':
				isdaemon = 1;
				log_to_systemlog = 1;
				rpcapd_log_set(log_to_systemlog, log_debug_messages);
				break;
			case 'i':
#ifdef _WIN32
				printusage();
				exit(1);
#else
				isrunbyinetd = 1;
				log_to_systemlog = 1;
				rpcapd_log_set(log_to_systemlog, log_debug_messages);
#endif
				break;
			case 'n':
				nullAuthAllowed = 1;
				break;
			case 'v':
				passivemode = 0;
				break;
			case 'l':
			{
				strncpy(hostlist, optarg, sizeof(hostlist));
				break;
			}
			case 'a':
			{
				char *tmpaddress, *tmpport;
				char *lasts;
				int i = 0;

				tmpaddress = pcap_strtok_r(optarg, RPCAP_HOSTLIST_SEP, &lasts);

				while ((tmpaddress != NULL) && (i < MAX_ACTIVE_LIST))
				{
					tmpport = pcap_strtok_r(NULL, RPCAP_HOSTLIST_SEP, &lasts);

					pcap_strlcpy(activelist[i].address, tmpaddress, MAX_LINE);

					if ((tmpport == NULL) || (strcmp(tmpport, "DEFAULT") == 0)) // the user choose a custom port
						pcap_strlcpy(activelist[i].port, RPCAP_DEFAULT_NETPORT_ACTIVE, MAX_LINE);
					else
						pcap_strlcpy(activelist[i].port, tmpport, MAX_LINE);

					tmpaddress = pcap_strtok_r(NULL, RPCAP_HOSTLIST_SEP, &lasts);

					i++;
				}

				if (i > MAX_ACTIVE_LIST)
					rpcapd_log(LOGPRIO_ERROR, "Only MAX_ACTIVE_LIST active connections are currently supported.");

				// I don't initialize the remaining part of the structure, since
				// it is already zeroed (it is a global var)
				break;
			}
			case 'f':
				pcap_strlcpy(loadfile, optarg, MAX_LINE);
				break;
			case 's':
				pcap_strlcpy(savefile, optarg, MAX_LINE);
				break;
			case 'h':
				printusage();
				exit(0);
				/*NOTREACHED*/
			default:
				exit(1);
				/*NOTREACHED*/
		}
	}

#ifndef _WIN32
	if (isdaemon && isrunbyinetd)
	{
		rpcapd_log(LOGPRIO_ERROR, "rpcapd: -d and -i can't be used together");
		exit(1);
	}
#endif

	if (sock_init(errbuf, PCAP_ERRBUF_SIZE) == -1)
	{
		rpcapd_log(LOGPRIO_ERROR, "%s", errbuf);
		exit(-1);
	}

	if (savefile[0] && fileconf_save(savefile))
		rpcapd_log(LOGPRIO_DEBUG, "Error when saving the configuration to file");

	// If the file does not exist, it keeps the settings provided by the command line
	if (loadfile[0])
		fileconf_read();

#ifdef WIN32
	//
	// Create a handle to signal the main loop to tell it to do
	// something.
	//
	state_change_event = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (state_change_event == NULL)
	{
		sock_geterror("Can't create state change event", errbuf,
		    PCAP_ERRBUF_SIZE);
		rpcapd_log(LOGPRIO_ERROR, "%s", errbuf);
		exit(2);
	}

	//
	// Catch control signals.
	//
	if (!SetConsoleCtrlHandler(main_ctrl_event, TRUE))
	{
		sock_geterror("Can't set control handler", errbuf,
		    PCAP_ERRBUF_SIZE);
		rpcapd_log(LOGPRIO_ERROR, "%s", errbuf);
		exit(2);
	}
#else
	memset(&action, 0, sizeof (action));
	action.sa_handler = main_terminate;
	action.sa_flags = 0;
	sigemptyset(&action.sa_mask);
	sigaction(SIGTERM, &action, NULL);
	memset(&action, 0, sizeof (action));
	action.sa_handler = main_reap_children;
	action.sa_flags = 0;
	sigemptyset(&action.sa_mask);
	sigaction(SIGCHLD, &action, NULL);
	// Ignore SIGPIPE - we'll get EPIPE when trying to write to a closed
	// connection, we don't want to get killed by a signal in that case
	signal(SIGPIPE, SIG_IGN);
#endif

#ifndef _WIN32
	if (isrunbyinetd)
	{
		//
		// -i was specified, indicating that this is being run
		// by inetd or something that can run network daemons
		// as if it were inetd (xinetd, launchd, systemd, etc.).
		//
		// We assume that the program that launched us just
		// duplicated a single socket for the connection
		// to our standard input, output, and error, so we
		// can just use the standard input as our control
		// socket.
		//
		int sockctrl;
		int devnull_fd;

		//
		// Duplicate the standard input as the control socket.
		//
		sockctrl = dup(0);
		if (sockctrl == -1)
		{
			sock_geterror("Can't dup standard input", errbuf,
			    PCAP_ERRBUF_SIZE);
			rpcapd_log(LOGPRIO_ERROR, "%s", errbuf);
			exit(2);
		}

		//
		// Try to set the standard input, output, and error
		// to /dev/null.
		//
		devnull_fd = open("/dev/null", O_RDWR);
		if (devnull_fd != -1)
		{
			//
			// If this fails, just drive on.
			//
			(void)dup2(devnull_fd, 0);
			(void)dup2(devnull_fd, 1);
			(void)dup2(devnull_fd, 2);
			close(devnull_fd);
		}

		//
		// Handle this client.
		// This is passive mode, so we don't care whether we were
		// told by the client to close.
		//
		char *hostlist_copy = strdup(hostlist);
		if (hostlist_copy == NULL)
		{
			rpcapd_log(LOGPRIO_ERROR, "Out of memory copying the host/port list");
			exit(0);
		}
		(void)daemon_serviceloop(sockctrl, 0, hostlist_copy,
		    nullAuthAllowed);

		//
		// Nothing more to do.
		//
		exit(0);
	}
#endif

	if (isdaemon)
	{
		//
		// This is being run as a daemon.
		// On UN*X, it might be manually run, or run from an
		// rc file.
		//
#ifndef _WIN32
		int pid;

		//
		// Daemonize ourselves.
		//
		// Unix Network Programming, pg 336
		//
		if ((pid = fork()) != 0)
			exit(0);		// Parent terminates

		// First child continues
		// Set daemon mode
		setsid();

		// generated under unix with 'kill -HUP', needed to reload the configuration
		memset(&action, 0, sizeof (action));
		action.sa_handler = main_reread_config;
		action.sa_flags = 0;
		sigemptyset(&action.sa_mask);
		sigaction(SIGHUP, &action, NULL);

		if ((pid = fork()) != 0)
			exit(0);		// First child terminates

		// LINUX WARNING: the current linux implementation of pthreads requires a management thread
		// to handle some hidden stuff. So, as soon as you create the first thread, two threads are
		// created. Fom this point on, the number of threads active are always one more compared
		// to the number you're expecting

		// Second child continues
//		umask(0);
//		chdir("/");
#else
		//
		// This is being run as a service on Windows.
		//
		// If this call succeeds, it is blocking on Win32
		//
		if (svc_start() != 1)
			rpcapd_log(LOGPRIO_DEBUG, "Unable to start the service");

		// When the previous call returns, the entire application has to be stopped.
		exit(0);
#endif
	}
	else	// Console mode
	{
#ifndef _WIN32
		// Enable the catching of Ctrl+C
		memset(&action, 0, sizeof (action));
		action.sa_handler = main_terminate;
		action.sa_flags = 0;
		sigemptyset(&action.sa_mask);
		sigaction(SIGINT, &action, NULL);

		// generated under unix with 'kill -HUP', needed to reload the configuration
		// We do not have this kind of signal in Win32
		memset(&action, 0, sizeof (action));
		action.sa_handler = main_reread_config;
		action.sa_flags = 0;
		sigemptyset(&action.sa_mask);
		sigaction(SIGHUP, &action, NULL);
#endif

		printf("Press CTRL + C to stop the server...\n");
	}

	// If we're a Win32 service, we have already called this function in the service_main
	main_startup();

	// The code should never arrive here (since the main_startup is blocking)
	//  however this avoids a compiler warning
	exit(0);
}

void main_startup(void)
{
	char errbuf[PCAP_ERRBUF_SIZE + 1];	// keeps the error string, prior to be printed
	struct addrinfo *addrinfo;		// keeps the addrinfo chain; required to open a new socket
	int i;
#ifdef _WIN32
	HANDLE threadId;			// handle for the subthread
#else
	pid_t pid;
#endif

	i = 0;
	addrinfo = NULL;
	memset(errbuf, 0, sizeof(errbuf));

	// Starts all the active threads
	while ((i < MAX_ACTIVE_LIST) && (activelist[i].address[0] != 0))
	{
		activelist[i].ai_family = mainhints.ai_family;

#ifdef _WIN32
		threadId = (HANDLE)_beginthreadex(NULL, 0, main_active,
		    (void *)&activelist[i], 0, NULL);
		if (threadId == 0)
		{
			rpcapd_log(LOGPRIO_DEBUG, "Error creating the active child threads");
			continue;
		}
		CloseHandle(threadId);
#else
		if ((pid = fork()) == 0)	// I am the child
		{
			main_active((void *) &activelist[i]);
			exit(0);
		}
#endif
		i++;
	}

	/*
	 * The code that manages the active connections is not blocking;
	 * the code that manages the passive connection is blocking.
	 * So, if the user does not want to run in passive mode, we have
	 * to block the main thread here, otherwise the program ends and
	 * all threads are stopped.
	 *
	 * WARNING: this means that in case we have only active mode,
	 * the program does not terminate even if all the child thread
	 * terminates. The user has always to press Ctrl+C (or send a
	 * SIGTERM) to terminate the program.
	 */
	if (passivemode)
	{
		struct addrinfo *tempaddrinfo;

		//
		// Get a list of sockets on which to listen.
		//
		if (sock_initaddress((address[0]) ? address : NULL, port, &mainhints, &addrinfo, errbuf, PCAP_ERRBUF_SIZE) == -1)
		{
			rpcapd_log(LOGPRIO_DEBUG, "%s", errbuf);
			return;
		}

		for (tempaddrinfo = addrinfo; tempaddrinfo;
		     tempaddrinfo = tempaddrinfo->ai_next)
		{
			SOCKET sock;
			struct listen_sock *sock_info;

			if ((sock = sock_open(tempaddrinfo, SOCKOPEN_SERVER, SOCKET_MAXCONN, errbuf, PCAP_ERRBUF_SIZE)) == INVALID_SOCKET)
			{
				switch (tempaddrinfo->ai_family)
				{
				case AF_INET:
				{
					struct sockaddr_in *in;
					char addrbuf[INET_ADDRSTRLEN];

					in = (struct sockaddr_in *)tempaddrinfo->ai_addr;
					rpcapd_log(LOGPRIO_WARNING, "Can't listen on socket for %s:%u: %s",
					    inet_ntop(AF_INET, &in->sin_addr,
						addrbuf, sizeof (addrbuf)),
					    ntohs(in->sin_port),
					    errbuf);
					break;
				}

				case AF_INET6:
				{
					struct sockaddr_in6 *in6;
					char addrbuf[INET6_ADDRSTRLEN];

					in6 = (struct sockaddr_in6 *)tempaddrinfo->ai_addr;
					rpcapd_log(LOGPRIO_WARNING, "Can't listen on socket for %s:%u: %s",
					    inet_ntop(AF_INET6, &in6->sin6_addr,
						addrbuf, sizeof (addrbuf)),
					    ntohs(in6->sin6_port),
					    errbuf);
					break;
				}

				default:
					rpcapd_log(LOGPRIO_WARNING, "Can't listen on socket for address family %u: %s",
					    tempaddrinfo->ai_family,
					    errbuf);
					break;
				}
				continue;
			}

			sock_info = (struct listen_sock *) malloc(sizeof (struct listen_sock));
			if (sock_info == NULL)
			{
				rpcapd_log(LOGPRIO_ERROR, "Can't allocate structure for listen socket");
				exit(2);
			}
			sock_info->sock = sock;
			sock_info->next = listen_socks;
			listen_socks = sock_info;
		}

		freeaddrinfo(addrinfo);

		if (listen_socks == NULL)
		{
			rpcapd_log(LOGPRIO_ERROR, "Can't listen on any address");
			exit(2);
		}

		//
		// Now listen on all of them, waiting for connections.
		//
		accept_connections();
	}

	//
	// We're done; exit.
	//
	rpcapd_log(LOGPRIO_DEBUG, PROGRAM_NAME " is closing.\n");

#ifndef _WIN32
	//
	// Sends a KILL signal to all the processes in this process's
	// process group; i.e., it kills all the child processes
	// we've created.
	//
	// XXX - that also includes us, so we will be killed as well;
	// that may cause a message to be printed or logged.
	//
	kill(0, SIGKILL);
#endif

	//
	// Just leave.  We shouldn't need to clean up sockets or
	// anything else, and if we try to do so, we'll could end
	// up closing sockets, or shutting Winsock down, out from
	// under service loops, causing all sorts of noisy error
	// messages.
	//
	// We shouldn't need to worry about cleaning up any resources
	// such as handles, sockets, threads, etc. - exit() should
	// terminate the process, causing all those resources to be
	// cleaned up (including the threads; Microsoft claims in the
	// ExitProcess() documentation that, if ExitProcess() is called,
	// "If a thread is waiting on a kernel object, it will not be
	// terminated until the wait has completed.", but claims in the
	// _beginthread()/_beginthreadex() documentation that "All threads
	// are terminated if any thread calls abort, exit, _exit, or
	// ExitProcess." - the latter appears to be the case, even for
	// threads waiting on the event for a pcap_t).
	//
	exit(0);
}

#ifdef _WIN32
static void
send_state_change_event(void)
{
	char errbuf[PCAP_ERRBUF_SIZE + 1];	// keeps the error string, prior to be printed

	if (!SetEvent(state_change_event))
	{
		sock_geterror("SetEvent on shutdown event failed", errbuf,
		    PCAP_ERRBUF_SIZE);
		rpcapd_log(LOGPRIO_ERROR, "%s", errbuf);
	}
}

void
send_shutdown_notification(void)
{
	//
	// Indicate that the server should shut down.
	//
	shutdown_server = 1;

	//
	// Send a state change event, to wake up WSAWaitForMultipleEvents().
	//
	send_state_change_event();
}

void
send_reread_configuration_notification(void)
{
	//
	// Indicate that the server should re-read its configuration file.
	//
	reread_config = 1;

	//
	// Send a state change event, to wake up WSAWaitForMultipleEvents().
	//
	send_state_change_event();
}

static BOOL WINAPI main_ctrl_event(DWORD ctrltype)
{
	//
	// ctrltype is one of:
	//
	// CTRL_C_EVENT - we got a ^C; this is like SIGINT
	// CTRL_BREAK_EVENT - we got Ctrl+Break
	// CTRL_CLOSE_EVENT - the console was closed; this is like SIGHUP
	// CTRL_LOGOFF_EVENT - a user is logging off; this is received
	//   only by services
	// CTRL_SHUTDOWN_EVENT - the systemis shutting down; this is
	//   received only by services
	//
	// For now, we treat all but CTRL_LOGOFF_EVENT as indications
	// that we should shut down.
	//
	switch (ctrltype)
	{
		case CTRL_C_EVENT:
		case CTRL_BREAK_EVENT:
		case CTRL_CLOSE_EVENT:
		case CTRL_SHUTDOWN_EVENT:
			//
			// Set a shutdown notification.
			//
			send_shutdown_notification();
			break;

		default:
			break;
	}

	//
	// We handled this.
	//
	return TRUE;
}
#else
static void main_terminate(int sign _U_)
{
	//
	// Note that the server should shut down.
	// select() should get an EINTR error when we return,
	// so it will wake up and know it needs to check the flag.
	//
	shutdown_server = 1;
}

static void main_reread_config(int sign _U_)
{
	//
	// Note that the server should re-read its configuration file.
	// select() should get an EINTR error when we return,
	// so it will wake up and know it needs to check the flag.
	//
	reread_config = 1;
}

static void main_reap_children(int sign _U_)
{
	pid_t pid;
	int exitstat;

	// Reap all child processes that have exited.
	// For reference, Stevens, pg 128

	while ((pid = waitpid(-1, &exitstat, WNOHANG)) > 0)
		rpcapd_log(LOGPRIO_DEBUG, "Child terminated");

	return;
}
#endif

//
// Loop waiting for incoming connections and accepting them.
//
static void
accept_connections(void)
{
#ifdef _WIN32
	struct listen_sock *sock_info;
	DWORD num_events;
	WSAEVENT *events;
	int i;
	char errbuf[PCAP_ERRBUF_SIZE + 1];	// keeps the error string, prior to be printed

	//
	// How big does the set of events need to be?
	// One for the shutdown event, plus one for every socket on which
	// we'll be listening.
	//
	num_events = 1;		// shutdown event
	for (sock_info = listen_socks; sock_info;
	    sock_info = sock_info->next)
	{
		if (num_events == WSA_MAXIMUM_WAIT_EVENTS)
		{
			//
			// WSAWaitForMultipleEvents() doesn't support
			// more than WSA_MAXIMUM_WAIT_EVENTS events
			// on which to wait.
			//
			rpcapd_log(LOGPRIO_ERROR, "Too many sockets on which to listen");
			exit(2);
		}
		num_events++;
	}

	//
	// Allocate the array of events.
	//
	events = (WSAEVENT *) malloc(num_events * sizeof (WSAEVENT));
	if (events == NULL)
	{
		rpcapd_log(LOGPRIO_ERROR, "Can't allocate array of events which to listen");
		exit(2);
	}

	//
	// Fill it in.
	//
	events[0] = state_change_event;	// state change event first
	for (sock_info = listen_socks, i = 1; sock_info;
	    sock_info = sock_info->next, i++)
	{
		WSAEVENT event;

		//
		// Create an event that is signaled if there's a connection
		// to accept on the socket in question.
		//
		event = WSACreateEvent();
		if (event == WSA_INVALID_EVENT)
		{
			sock_geterror("Can't create socket event", errbuf,
			    PCAP_ERRBUF_SIZE);
			rpcapd_log(LOGPRIO_ERROR, "%s", errbuf);
			exit(2);
		}
		if (WSAEventSelect(sock_info->sock, event, FD_ACCEPT) == SOCKET_ERROR)
		{
			sock_geterror("Can't setup socket event", errbuf,
			    PCAP_ERRBUF_SIZE);
			rpcapd_log(LOGPRIO_ERROR, "%s", errbuf);
			exit(2);
		}
		events[i] = event;
	}

	for (;;)
	{
		//
		// Wait for incoming connections.
		//
		DWORD ret;

		ret = WSAWaitForMultipleEvents(num_events, events, FALSE,
		    WSA_INFINITE, FALSE);
		if (ret == WSA_WAIT_FAILED)
		{
			sock_geterror("WSAWaitForMultipleEvents failed", errbuf,
			    PCAP_ERRBUF_SIZE);
			rpcapd_log(LOGPRIO_ERROR, "%s", errbuf);
			exit(2);
		}

		if (ret == WSA_WAIT_EVENT_0)
		{
			//
			// The state change event was set.
			//
			if (shutdown_server)
			{
				//
				// Time to quit. Exit the loop.
				//
				break;
			}
			if (reread_config)
			{
				//
				// We should re-read the configuration
				// file.
				//
				reread_config = 0;	// clear the indicator
				fileconf_read();
			}
		}

		//
		// Check each socket.
		//
		for (sock_info = listen_socks, i = 1; sock_info;
		    sock_info = sock_info->next, i++)
		{
			WSANETWORKEVENTS network_events;

			if (WSAEnumNetworkEvents(sock_info->sock,
			    events[i], &network_events) == SOCKET_ERROR)
			{
				sock_geterror("WSAEnumNetworkEvents failed",
				    errbuf, PCAP_ERRBUF_SIZE);
				rpcapd_log(LOGPRIO_ERROR, "%s", errbuf);
				exit(2);
			}
			if (network_events.lNetworkEvents & FD_ACCEPT)
			{
				//
				// Did an error occur?
				//
			 	if (network_events.iErrorCode[FD_ACCEPT_BIT] != 0)
			 	{
					//
					// Yes - report it and keep going.
					//
					sock_fmterror("Socket error",
					    network_events.iErrorCode[FD_ACCEPT_BIT],
					    errbuf,
					    PCAP_ERRBUF_SIZE);
					rpcapd_log(LOGPRIO_ERROR, "%s", errbuf);
					continue;
				}

				//
				// Accept the connection.
				//
				accept_connection(sock_info->sock);
			}
		}
	}
#else
	struct listen_sock *sock_info;
	int num_sock_fds;

	//
	// How big does the bitset of sockets on which to select() have
	// to be?
	//
	num_sock_fds = 0;
	for (sock_info = listen_socks; sock_info; sock_info = sock_info->next)
	{
		if (sock_info->sock + 1 > num_sock_fds)
		{
			if ((unsigned int)(sock_info->sock + 1) >
			    (unsigned int)FD_SETSIZE)
			{
				rpcapd_log(LOGPRIO_ERROR, "Socket FD is too bit for an fd_set");
				exit(2);
			}
			num_sock_fds = sock_info->sock + 1;
		}
	}

	for (;;)
	{
		fd_set sock_fds;
		int ret;

		//
		// Set up an fd_set for all the sockets on which we're
		// listening.
		//
		// This set is modified by select(), so we have to
		// construct it anew each time.
		//
		FD_ZERO(&sock_fds);
		for (sock_info = listen_socks; sock_info;
		    sock_info = sock_info->next)
		{
			FD_SET(sock_info->sock, &sock_fds);
		}

		//
		// Wait for incoming connections.
		//
		ret = select(num_sock_fds, &sock_fds, NULL, NULL, NULL);
		if (ret == -1)
		{
			if (errno == EINTR)
			{
				//
				// If this is a "terminate the
				// server" signal, exit the loop,
				// otherwise just keep trying.
				//
				if (shutdown_server)
				{
					//
					// Time to quit.  Exit the loop.
					//
					break;
				}
				if (reread_config)
				{
					//
					// We should re-read the configuration
					// file.
					//
					reread_config = 0;	// clear the indicator
					fileconf_read();
				}

				//
				// Go back and wait again.
				//
				continue;
			}
			else
			{
				rpcapd_log(LOGPRIO_ERROR, "select failed: %s",
				    strerror(errno));
				exit(2);
			}
		}

		//
		// Check each socket.
		//
		for (sock_info = listen_socks; sock_info;
		    sock_info = sock_info->next)
		{
			if (FD_ISSET(sock_info->sock, &sock_fds))
			{
				//
				// Accept the connection.
				//
				accept_connection(sock_info->sock);
			}
		}
	}
#endif

	//
	// Close all the listen sockets.
	//
	for (sock_info = listen_socks; sock_info; sock_info = sock_info->next)
	{
		closesocket(sock_info->sock);
	}
	sock_cleanup();
}

#ifdef _WIN32
//
// A structure to hold the parameters to the daemon service loop
// thread on Windows.
//
// (On UN*X, there is no need for this explicit copy since the
// fork "inherits" the parent stack.)
//
struct params_copy {
	SOCKET sockctrl;
	char *hostlist;
};
#endif

//
// Accept a connection and start a worker thread, on Windows, or a
// worker process, on UN*X, to handle the connection.
//
static void
accept_connection(SOCKET listen_sock)
{
	char errbuf[PCAP_ERRBUF_SIZE + 1];	// keeps the error string, prior to be printed
	SOCKET sockctrl;			// keeps the socket ID for this control connection
	struct sockaddr_storage from;		// generic sockaddr_storage variable
	socklen_t fromlen;			// keeps the length of the sockaddr_storage variable

#ifdef _WIN32
	HANDLE threadId;			// handle for the subthread
	u_long off = 0;
	struct params_copy *params_copy = NULL;
#else
	pid_t pid;
#endif

	// Initialize errbuf
	memset(errbuf, 0, sizeof(errbuf));

	for (;;)
	{
		// Accept the connection
		fromlen = sizeof(struct sockaddr_storage);

		sockctrl = accept(listen_sock, (struct sockaddr *) &from, &fromlen);

		if (sockctrl != INVALID_SOCKET)
		{
			// Success.
			break;
		}

		// The accept() call can return this error when a signal is catched
		// In this case, we have simply to ignore this error code
		// Stevens, pg 124
#ifdef _WIN32
		if (WSAGetLastError() == WSAEINTR)
#else
		if (errno == EINTR)
#endif
			continue;

		// Don't check for errors here, since the error can be due to the fact that the thread
		// has been killed
		sock_geterror("accept()", errbuf, PCAP_ERRBUF_SIZE);
		rpcapd_log(LOGPRIO_ERROR, "Accept of control connection from client failed: %s",
		    errbuf);
		return;
	}

#ifdef _WIN32
	//
	// Put the socket back into blocking mode; doing WSAEventSelect()
	// on the listen socket makes that socket non-blocking, and it
	// appears that sockets returned from an accept() on that socket
	// are also non-blocking.
	//
	// First, we have to un-WSAEventSelect() this socket, and then
	// we can turn non-blocking mode off.
	//
	// If this fails, we aren't guaranteed that, for example, any
	// of the error message will be sent - if it can't be put in
	// the socket queue, the send will just fail.
	//
	// So we just log the message and close the connection.
	//
	if (WSAEventSelect(sockctrl, NULL, 0) == SOCKET_ERROR)
	{
		sock_geterror("WSAEventSelect()", errbuf, PCAP_ERRBUF_SIZE);
		rpcapd_log(LOGPRIO_ERROR, "%s", errbuf);
		sock_close(sockctrl, NULL, 0);
		return;
	}
	if (ioctlsocket(sockctrl, FIONBIO, &off) == SOCKET_ERROR)
	{
		sock_geterror("ioctlsocket(FIONBIO)", errbuf, PCAP_ERRBUF_SIZE);
		rpcapd_log(LOGPRIO_ERROR, "%s", errbuf);
		sock_close(sockctrl, NULL, 0);
		return;
	}

	//
	// Make a copy of the host list to pass to the new thread, so that
	// if we update it in the main thread, it won't catch us in the
	// middle of updating it.
	//
	// daemon_serviceloop() will free it once it's done with it.
	//
	char *hostlist_copy = strdup(hostlist);
	if (hostlist_copy == NULL)
	{
		rpcapd_log(LOGPRIO_ERROR, "Out of memory copying the host/port list");
		sock_close(sockctrl, NULL, 0);
		return;
	}

	//
	// Allocate a location to hold the values of sockctrl.
	// It will be freed in the newly-created thread once it's
	// finished with it.
	//
	params_copy = malloc(sizeof(*params_copy));
	if (params_copy == NULL)
	{
		rpcapd_log(LOGPRIO_ERROR, "Out of memory allocating the parameter copy structure");
		free(hostlist_copy);
		sock_close(sockctrl, NULL, 0);
		return;
	}
	params_copy->sockctrl = sockctrl;
	params_copy->hostlist = hostlist_copy;

	threadId = (HANDLE)_beginthreadex(NULL, 0,
	    main_passive_serviceloop_thread, (void *) params_copy, 0, NULL);
	if (threadId == 0)
	{
		rpcapd_log(LOGPRIO_ERROR, "Error creating the child thread");
		free(params_copy);
		free(hostlist_copy);
		sock_close(sockctrl, NULL, 0);
		return;
	}
	CloseHandle(threadId);
#else /* _WIN32 */
	pid = fork();
	if (pid == -1)
	{
		rpcapd_log(LOGPRIO_ERROR, "Error creating the child process: %s",
		    strerror(errno));
		sock_close(sockctrl, NULL, 0);
		return;
	}
	if (pid == 0)
	{
		//
		// Child process.
		//
		// Close the socket on which we're listening (must
		// be open only in the parent).
		//
		closesocket(listen_sock);

#if 0
		//
		// Modify thread params so that it can be killed at any time
		// XXX - is this necessary?  This is the main and, currently,
		// only thread in the child process, and nobody tries to
		// cancel us, although *we* may cancel the thread that's
		// handling the capture loop.
		//
		if (pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL))
			goto end;
		if (pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL))
			goto end;
#endif

		//
		// Run the service loop.
		// This is passive mode, so we don't care whether we were
		// told by the client to close.
		//
		char *hostlist_copy = strdup(hostlist);
		if (hostlist_copy == NULL)
		{
			rpcapd_log(LOGPRIO_ERROR, "Out of memory copying the host/port list");
			exit(0);
		}
		(void)daemon_serviceloop(sockctrl, 0, hostlist_copy,
		    nullAuthAllowed);

		exit(0);
	}

	// I am the parent
	// Close the socket for this session (must be open only in the child)
	closesocket(sockctrl);
#endif /* _WIN32 */
}

/*!
	\brief 'true' main of the program in case the active mode is turned on.

	This function loops forever trying to connect to the remote host, until the
	daemon is turned down.

	\param ptr: it keeps the 'activepars' parameters.  It is a 'void *'
	just because the thread APIs want this format.
*/
#ifdef _WIN32
static unsigned __stdcall
#else
static void *
#endif
main_active(void *ptr)
{
	char errbuf[PCAP_ERRBUF_SIZE + 1];	// keeps the error string, prior to be printed
	SOCKET sockctrl;			// keeps the socket ID for this control connection
	struct addrinfo hints;			// temporary struct to keep settings needed to open the new socket
	struct addrinfo *addrinfo;		// keeps the addrinfo chain; required to open a new socket
	struct active_pars *activepars;

	activepars = (struct active_pars *) ptr;

	// Prepare to open a new server socket
	memset(&hints, 0, sizeof(struct addrinfo));
						// WARNING Currently it supports only ONE socket family among IPv4 and IPv6
	hints.ai_family = AF_INET;		// PF_UNSPEC to have both IPv4 and IPv6 server
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = activepars->ai_family;

	rpcapd_log(LOGPRIO_DEBUG, "Connecting to host %s, port %s, using protocol %s",
	    activepars->address, activepars->port, (hints.ai_family == AF_INET) ? "IPv4":
	    (hints.ai_family == AF_INET6) ? "IPv6" : "Unspecified");

	// Initialize errbuf
	memset(errbuf, 0, sizeof(errbuf));

	// Do the work
	if (sock_initaddress(activepars->address, activepars->port, &hints, &addrinfo, errbuf, PCAP_ERRBUF_SIZE) == -1)
	{
		rpcapd_log(LOGPRIO_DEBUG, "%s", errbuf);
		return 0;
	}

	for (;;)
	{
		int activeclose;

		if ((sockctrl = sock_open(addrinfo, SOCKOPEN_CLIENT, 0, errbuf, PCAP_ERRBUF_SIZE)) == INVALID_SOCKET)
		{
			rpcapd_log(LOGPRIO_DEBUG, "%s", errbuf);

			pcap_snprintf(errbuf, PCAP_ERRBUF_SIZE, "Error connecting to host %s, port %s, using protocol %s",
					activepars->address, activepars->port, (hints.ai_family == AF_INET) ? "IPv4":
					(hints.ai_family == AF_INET6) ? "IPv6" : "Unspecified");

			rpcapd_log(LOGPRIO_DEBUG, "%s", errbuf);

			sleep_secs(RPCAP_ACTIVE_WAIT);

			continue;
		}

		char *hostlist_copy = strdup(hostlist);
		if (hostlist_copy == NULL)
		{
			rpcapd_log(LOGPRIO_ERROR, "Out of memory copying the host/port list");
			activeclose = 0;
			sock_close(sockctrl, NULL, 0);
		}
		else
		{
			//
			// daemon_serviceloop() will free the copy.
			//
			activeclose = daemon_serviceloop(sockctrl, 1,
			    hostlist_copy, nullAuthAllowed);
		}

		// If the connection is closed by the user explicitely, don't try to connect to it again
		// just exit the program
		if (activeclose == 1)
			break;
	}

	freeaddrinfo(addrinfo);
	return 0;
}

#ifdef _WIN32
//
// Main routine of a passive-mode service thread.
//
unsigned __stdcall main_passive_serviceloop_thread(void *ptr)
{
	struct params_copy params = *(struct params_copy *)ptr;
	free(ptr);

	//
	// Handle this client.
	// This is passive mode, so we don't care whether we were
	// told by the client to close.
	//
	(void)daemon_serviceloop(params.sockctrl, 0, params.hostlist,
	    nullAuthAllowed);

	return 0;
}
#endif
