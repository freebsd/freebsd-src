/*
 *
 * ===================================
 * HARP  |  Host ATM Research Platform
 * ===================================
 *
 *
 * This Host ATM Research Platform ("HARP") file (the "Software") is
 * made available by Network Computing Services, Inc. ("NetworkCS")
 * "AS IS".  NetworkCS does not provide maintenance, improvements or
 * support of any kind.
 *
 * NETWORKCS MAKES NO WARRANTIES OR REPRESENTATIONS, EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE, AS TO ANY ELEMENT OF THE
 * SOFTWARE OR ANY SUPPORT PROVIDED IN CONNECTION WITH THIS SOFTWARE.
 * In no event shall NetworkCS be responsible for any damages, including
 * but not limited to consequential damages, arising from or relating to
 * any use of the Software or related support.
 *
 * Copyright 1994-1998 Network Computing Services, Inc.
 *
 * Copies of this Software may be made, however, the above copyright
 * notice must be reproduced on all copies.
 *
 *	@(#) $FreeBSD: src/usr.sbin/atm/scspd/scspd.c,v 1.3 1999/08/28 01:15:34 peter Exp $
 *
 */


/*
 * Server Cache Synchronization Protocol (SCSP) Support
 * ----------------------------------------------------
 *
 * SCSP server daemon main line code
 *
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/ttycom.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netatm/queue.h>
#include <netatm/atm.h>
#include <netatm/atm_if.h>
#include <netatm/atm_sap.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_ioctl.h>
 
#include <errno.h>
#include <fcntl.h>
#include <libatm.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "scsp_msg.h"
#include "scsp_if.h"
#include "scsp_var.h"

#ifndef lint
__RCSID("@(#) $FreeBSD: src/usr.sbin/atm/scspd/scspd.c,v 1.3 1999/08/28 01:15:34 peter Exp $");
#endif


/*
 * Global variables
 */
char		*prog;
char		*scsp_config_file = SCSPD_CONFIG;
FILE		*scsp_log_file = (FILE *)0;
int		scsp_log_syslog = 0;
Scsp_server	*scsp_server_head = (Scsp_server *)0;
Scsp_pending	*scsp_pending_head = (Scsp_pending *)0;
int		scsp_max_socket = -1;
int		scsp_debug_mode = 0;
int		scsp_trace_mode = 0;


/*
 * Local variables
 */
static int	scsp_hup_signal = 0;
static int	scsp_int_signal = 0;


/*
 * SIGHUP signal handler
 *
 * Arguments:
 *	sig     signal number
 *
 * Returns:
 *	none
 *
 */
void
scsp_sighup(sig)
	int	sig;
{
	/*
	 * Flag the signal
	 */
	scsp_hup_signal = 1;
}


/*
 * SIGINT signal handler
 *
 * Arguments:
 *	sig     signal number
 *
 * Returns:
 *	none
 *
 */
void
scsp_sigint(sig)
	int	sig;
{
	/*
	 * Flag the signal
	 */
	scsp_int_signal = 1;
}


/*
 * Process command line parameters
 *
 * Arguments:
 *	argc	number of command-line arguments
 *	argv	list of pointers to command-line arguments
 *
 * Returns:
 *	none
 *
 */
void
initialize(argc, argv)
	int	argc;
	char	**argv;
{
	int	i;
	char	*cp;

	/*
	 * Save program name, ignoring any path components
	 */
	if ((prog = (char *)strrchr(argv[0], '/')) != NULL)
		prog++;
	else
		prog = argv[0];

	/*
	 * Make sure we're being invoked by the super user
	 */
	i = getuid();
	if (i != 0) {
		fprintf(stderr, "%s: You must be root to run this program\n",
				prog);
		exit(1);
	}

	/*
	 * Check for command-line options
	 */
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-d") == 0) {
			/*
			 * -d option -- set debug mode
			 */
			scsp_debug_mode = 1;
		} else if (strcmp(argv[i], "-f") == 0) {
			/*
			 * -f option -- set config file name
			 */
			i++;
			if (i >= argc) {
				fprintf(stderr, "%s: Configuration file name missing\n",
						prog);
				exit(1);
			}
			scsp_config_file = argv[i];
		} else if (strncmp(argv[i], "-T", 2) == 0) {
			/*
			 * -T option -- trace options
			 */
			for (cp = &argv[i][2]; *cp; cp++) {
				if (*cp == 'c')
					scsp_trace_mode |= SCSP_TRACE_CAFSM;
				else if (*cp == 'h')
					scsp_trace_mode |= SCSP_TRACE_HFSM;
				else if (*cp == 'i')
					scsp_trace_mode |= SCSP_TRACE_CFSM;
				else if (*cp == 'C')
					scsp_trace_mode |= SCSP_TRACE_CA_MSG;
				else if (*cp == 'H')
					scsp_trace_mode |= SCSP_TRACE_HELLO_MSG;
				else if (*cp == 'I')
					scsp_trace_mode |= SCSP_TRACE_IF_MSG;
				else
					fprintf(stderr, "Invalid trace specification '%c' ignored\n",
							*cp);
			}
		} else {
			/*
			 * Error -- unrecognized option
			 */
			fprintf(stderr, "%s: Unrecognized option \"%s\"\n",
					prog, argv[i]);
			exit(1);
		}
	}
}


/*
 * Daemon housekeeping
 *
 * Arguments:
 *	None
 *
 * Returns:
 *	None
 *
 */
static void
start_daemon()

{
	int	dpid, fd, file_count, rc;

	/*
	 * Ignore selected signals
	 */
#ifdef SIGTTOU
	signal(SIGTTOU, SIG_IGN);
#endif
#ifdef SIGTTIN
	signal(SIGTTIN, SIG_IGN);
#endif
#ifdef SIGTSTP
	signal(SIGTSTP, SIG_IGN);
#endif
#ifdef SIGPIPE
	signal(SIGPIPE, SIG_IGN);
#endif


	/*
	 * Don't put the daemon into the background if
	 * we're in debug mode
	 */
	if (scsp_debug_mode)
		goto daemon_bypass;

	/*
	 * Put the daemon into the background
	 */
	dpid = fork();
	if (dpid < 0) {
		scsp_log(LOG_ERR, "fork failed");
		abort();
	}
	if (dpid > 0) {
		/*
		 * This is the parent process--just exit and let
		 * the daughter do all the work
		 */
		exit(0);
	}

	/*
	 * Disassociate from any controlling terminal
	 */
	rc = setpgrp(0, getpid());
	if (rc <0) {
		scsp_log(LOG_ERR, "can't change process group");
		exit(1);
	}
	fd = open("/dev/tty", O_RDWR);
	if (fd >= 0) {
		ioctl(fd, TIOCNOTTY, (char *)0);
		close(fd);
	}

	/*
	 * Close all open file descriptors
	 */
	file_count = getdtablesize();
	for (fd=0; fd<file_count; fd++) {
		close(fd);
	}

	/*
	 * Set up timers
	 */
daemon_bypass:
	init_timer();

	/*
	 * Move to a safe directory
	 */
	chdir(SCSPD_DIR);

	/*
	 * Clear the file mode creation mask
	 */
	umask(0);


	/*
	 * Set up signal handlers
	 */
	rc = (int)signal(SIGHUP, scsp_sighup);
	if (rc == -1) {
		scsp_log(LOG_ERR, "SIGHUP signal setup failed");
		exit(1);
	}

	rc = (int)signal(SIGINT, scsp_sigint);
	if (rc == -1) {
		scsp_log(LOG_ERR, "SIGINT signal setup failed");
		exit(1);
	}

	/*
	 * Set up syslog for error logging
	 */
	if (scsp_log_syslog || !scsp_log_file) {
		openlog(prog, LOG_PID | LOG_CONS, LOG_DAEMON);
	}
	scsp_log(LOG_INFO, "Starting SCSP daemon");
}


/*
 * Main line code
 *
 * Process command line parameters, read configuration file, connect
 * to configured clients, process data from DCSs.
 *
 * Arguments:
 *	argc	number of command-line arguments
 *	argv	list of pointers to command-line arguments
 *
 * Returns:
 *	none
 *
 */
int
main(argc, argv)
	int	argc;
	char	*argv[];

{
	int		i, rc, scsp_server_lsock;
	Scsp_server	*ssp;
	Scsp_dcs	*dcsp;
	Scsp_pending	*next_psp, *psp;
	fd_set		read_set, write_set, except_set;

	/*
	 * Process command line arguments
	 */
	initialize(argc, argv);

	/*
	 * Put the daemon into the background
	 */
	start_daemon();

	/*
	 * Process configuration file
	 */
	rc = scsp_config(scsp_config_file);
	if (rc) {
		scsp_log(LOG_ERR, "Found %d error%s in configuration file",
				rc, ((rc == 1) ? "" : "s"));
		exit(1);
	}

	/*
	 * Open the trace file if we need one
	 */
	if (scsp_trace_mode) {
		scsp_open_trace();
	}

	/*
	 * Listen for connections from clients
	 */
	scsp_server_lsock = scsp_server_listen();
	if (scsp_server_lsock == -1) {
		scsp_log(LOG_ERR, "server listen failed");
		abort();
	}

	/*
	 * Main program loop -- we wait for:
	 *	a server listen to complete
	 *	a DCS listen to complete
	 *	a DCS connect to complete
	 *	data from a server
	 *	data from a DCS
	 */
	while (1) {
		/*
		 * Set up the file descriptor sets and select to wait
		 * for input
		 */
		FD_ZERO(&read_set);
		FD_ZERO(&write_set);
		FD_ZERO(&except_set);
		FD_SET(scsp_server_lsock, &read_set);
		for (ssp = scsp_server_head; ssp; ssp = ssp->ss_next) {
			if (ssp->ss_dcs_lsock != -1)
				FD_SET(ssp->ss_dcs_lsock, &read_set);
			if (ssp->ss_sock != -1)
				FD_SET(ssp->ss_sock, &read_set);
			for (dcsp = ssp->ss_dcs; dcsp;
					dcsp = dcsp->sd_next) {
				if (dcsp->sd_sock != -1) {
					if (dcsp->sd_hello_state ==
							SCSP_HFSM_DOWN )
						FD_SET(dcsp->sd_sock,
							&write_set);
					else
						FD_SET(dcsp->sd_sock,
							&read_set);
				}
			}
		}
		for (psp = scsp_pending_head; psp; psp = psp->sp_next) {
			FD_SET(psp->sp_sock, &read_set);
		}
		rc = select(scsp_max_socket + 1, &read_set,
				&write_set, &except_set,
				(struct timeval *)0);
		if (rc < 0) {
			/*
			 * Select error--check for possible signals
			 */
			if (harp_timer_exec) {
				/*
				 * Timer tick--process it
				 */
				timer_proc();
				continue;
			} else if (scsp_hup_signal) {
				/*
				 * SIGHUP signal--reconfigure
				 */
				scsp_hup_signal = 0;
				scsp_reconfigure();
				continue;
			} else if (scsp_int_signal) {
				/*
				 * SIGINT signal--dump control blocks
				 */
				print_scsp_dump();
				scsp_int_signal = 0;
				continue;
			} else if (errno == EINTR) {
				/*
				 * EINTR--just ignore it
				 */
				continue;
			} else {
				/*
				 * Other error--this is a problem
				 */
				scsp_log(LOG_ERR, "Select failed");
				abort();
			}
		}

		/*
		 * Check the read set for connections from servers
		 */
		if (FD_ISSET(scsp_server_lsock, &read_set)) {
			FD_CLR(scsp_server_lsock, &read_set);
			rc = scsp_server_accept(scsp_server_lsock);
		}

		/*
		 * Check the write set for new connections to DCSs
		 */
		for (i = 0; i <= scsp_max_socket; i++) {
			if (FD_ISSET(i, &write_set)) {
				FD_CLR(i, &write_set);
				if ((dcsp = scsp_find_dcs(i)) != NULL) {
					rc = scsp_hfsm(dcsp,
						SCSP_HFSM_VC_ESTAB,
						(Scsp_msg *)0);
				}
			}
		}

		/*
		 * Check the read set for connections from DCSs
		 */
		for (ssp = scsp_server_head; ssp; ssp = ssp->ss_next) {
			if (ssp->ss_dcs_lsock != -1 &&
					FD_ISSET(ssp->ss_dcs_lsock,
						&read_set)) {
				FD_CLR(ssp->ss_dcs_lsock, &read_set);
				dcsp = scsp_dcs_accept(ssp);
				if (dcsp) {
					rc = scsp_hfsm(dcsp,
						SCSP_HFSM_VC_ESTAB,
						(Scsp_msg *)0);
				}
			}
		}

		/*
		 * Check the read set for data from pending servers
		 */
		for (psp = scsp_pending_head; psp; psp = next_psp) {
			next_psp = psp->sp_next;
			if (FD_ISSET(psp->sp_sock, &read_set)) {
				FD_CLR(psp->sp_sock, &read_set);
				rc = scsp_pending_read(psp);
			}
		}

		/*
		 * Check the read set for data from servers or DCSs
		 */
		for (i = 0; i <= scsp_max_socket; i++) {
			if (FD_ISSET(i, &read_set)) {
				if ((ssp = scsp_find_server(i)) != NULL) {
					rc = scsp_server_read(ssp);
				} else if ((dcsp = scsp_find_dcs(i)) != NULL) {
					rc = scsp_dcs_read(dcsp);
				}
			}
		}
	}
}
