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
 *	@(#) $FreeBSD$
 *
 */

/*
 * Server Cache Synchronization Protocol (SCSP) Support
 * ----------------------------------------------------
 *
 * SCSP-ATMARP server interface: main line code
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
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "../scspd/scsp_msg.h"
#include "../scspd/scsp_if.h"
#include "../scspd/scsp_var.h"
#include "atmarp_var.h"

#ifndef lint
__RCSID("@(#) $FreeBSD$");
#endif


/*
 * Global variables
 */
char		*prog;
int		atmarp_debug_mode = 0;
int		atmarp_max_socket = 0;
Atmarp_intf	*atmarp_intf_head = (Atmarp_intf *)0;
Atmarp_slis	*atmarp_slis_head = (Atmarp_slis *)0;
FILE		*atmarp_log_file = (FILE *)0;
char		*atmarp_log_file_name = (char *)0;
Harp_timer	cache_timer, perm_timer;


/*
 * Print a usage message
 *
 * Arguments:
 *	none
 *
 * Returns:
 *	exits, does not return
 *
 */
void
usage()
{
	fprintf(stderr, "usage: %s [-d] [-l <log_file>] <net_intf> ...\n", prog);
	exit(1);
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
static void
initialize(argc, argv)
	int	argc;
	char	*argv[];

{
	int	i, rc;

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
	 * Scan arguments, checking for options
	 */
	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			if (strcmp(argv[i], "-d") == 0) {
				atmarp_debug_mode = TRUE;
			} else if (strcmp(argv[i], "-l") == 0) {
				i++;
				if (i >= argc) {
					fprintf(stderr, "%s: Log file name missing\n",
						prog);
					exit(1);
				}
				atmarp_log_file_name = argv[i];
			} else {
				fprintf(stderr, "%s: Unrecognized option \"%s\"\n",
						prog, argv[i]);
				exit(1);
			}
		} else {
			/*
			 * Parameter is a network interface name
			 */
			rc = atmarp_cfg_netif(argv[i]);
			if (rc) {
				fprintf(stderr, "%s: Error configuring network interface %s\n",
						prog, argv[i]);
				exit(1);
			}
		}
	}

	/*
	 * Make sure we had at least one interface configured
	 */
	if (!atmarp_intf_head) {
		usage();
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
	 * Skip putting things into the background if we're
	 * in debugging mode
	 */
	if (atmarp_debug_mode)
		goto daemon_bypass;

	/*
	 * Set up syslog for error logging
	 */
	if (!atmarp_log_file) {
		openlog(prog, LOG_PID | LOG_CONS, LOG_DAEMON);
	}

	/*
	 * Put the daemon into the background
	 */
	dpid = fork();
	if (dpid < 0) {
		atmarp_log(LOG_ERR, "fork failed");
		exit(1);
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
	if (rc < 0) {
		atmarp_log(LOG_ERR, "can't change process group");
		exit(1);
	}
	fd = open(_PATH_TTY, O_RDWR);
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
	 * Open log file, if specified
	 */
	if (atmarp_log_file_name) {
		atmarp_log_file = fopen(atmarp_log_file_name, "a");
		if (!atmarp_log_file) {
			atmarp_log(LOG_ERR, "%s: Can't open log file \'%s\'\n",
					prog, atmarp_log_file_name);
			exit(1);
		}
	}

	/*
	 * Set up and start interval timer
	 */
daemon_bypass:
	init_timer();

	/*
	 * Move to a safe directory
	 */
	chdir(ATMARP_DIR);

	/*
	 * Clear the file mode creation mask
	 */
	umask(0);


	/*
	 * Set up signal handlers
	 */
	if (signal(SIGINT, atmarp_sigint) == SIG_ERR) {
		atmarp_log(LOG_ERR, "SIGINT signal setup failed");
		exit(1);
	}
}


/*
 * Main line code
 *
 * The ATMARP server resides in the kernel, while SCSP runs as a daemon
 * in user space.  This program exists to provide an interface between
 * the two.  It periodically polls the kernel to get the ATMARP cache
 * and passes information about new entries to SCSP.  It also accepts
 * new information from SCSP and passes it to the kernel.
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
	int		i, rc;
	fd_set		read_set, write_set, except_set;
	Atmarp_intf	*aip;

	/*
	 * Process command line arguments
	 */
	initialize(argc, argv);

	/*
	 * Put the daemon into the background
	 */
	start_daemon();

	/*
	 * Start the cache update timer
	 */
	HARP_TIMER(&cache_timer, ATMARP_CACHE_INTERVAL,
			atmarp_cache_timeout);

	/*
	 * Start the permanent cache entry timer
	 */
	HARP_TIMER(&perm_timer, ATMARP_PERM_INTERVAL,
			atmarp_perm_timeout);

	/*
	 * Establish a connection to SCSP for each interface.  If a
	 * connect fails, it will be retried when the cache update
	 * timer fires.
	 */
	for (aip = atmarp_intf_head; aip; aip = aip->ai_next) {
		if (atmarp_if_ready(aip)) {
			(void)atmarp_scsp_connect(aip);
		}
	}

	/*
	 * Read the cache from the kernel
	 */
	atmarp_get_updated_cache();

	/*
	 * Main program loop -- wait for data to come in from SCSP.
	 * When the timer fires, it will be handled elsewhere.
	 */
	while (1) {
		/*
		 * Wait for input from SCSP
		 */
		FD_ZERO(&read_set);
		FD_ZERO(&write_set);
		FD_ZERO(&except_set);
		for (aip = atmarp_intf_head; aip; aip = aip->ai_next) {
			if (aip->ai_scsp_sock != -1) {
				FD_SET(aip->ai_scsp_sock, &read_set);
			}
		}
		rc = select(atmarp_max_socket + 1,
				&read_set, &write_set,
				&except_set, (struct timeval *)0);
		if (rc < 0) {
			if (harp_timer_exec) {
				timer_proc();
				continue;
			} else if (errno == EINTR) {
				continue;
			} else {
				atmarp_log(LOG_ERR, "Select failed");
				abort();
			}
		}

		/*
		 * Read and process the input from SCSP
		 */
		for (i = 0; i <= atmarp_max_socket; i++) {
			if (FD_ISSET(i, &read_set)) {
				aip = atmarp_find_intf_sock(i);
				if (aip)
					rc = atmarp_scsp_read(aip);
			}
		}
	}
}
