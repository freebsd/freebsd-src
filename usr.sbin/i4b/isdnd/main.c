/*
 * Copyright (c) 1997, 2001 Hellmuth Michaelis. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *---------------------------------------------------------------------------
 *
 *	i4b daemon - main program entry
 *	-------------------------------
 *
 * $FreeBSD$
 *
 *      last edit-date: [Tue Jun  5 17:06:20 2001]
 *
 *---------------------------------------------------------------------------*/

#include <locale.h>
#include <paths.h>

#ifdef I4B_EXTERNAL_MONITOR
#include "monitor.h"
#endif

#define MAIN
#include "isdnd.h"
#undef MAIN

#ifdef I4B_EXTERNAL_MONITOR

#ifdef I4B_NOTCPIP_MONITOR
/* monitor via local socket */
static void mloop(int sockfd);
#else /* I4B_NOTCPIP_MONITOR */
/* monitor via local and tcp/ip socket */
static void mloop(int localsock, int remotesock);
#endif /* I4B_NOTCPIP_MONITOR */

#else /* I4B_EXTERNAL_MONITOR */
/* no monitoring at all */
static void mloop();
#endif /* I4B_EXTERNAL_MONITOR */

#ifdef USE_CURSES
static void kbdrdhdl(void);
#endif

static void isdnrdhdl(void);
static void usage(void);

#define MSG_BUF_SIZ	1024	/* message buffer size */

/*---------------------------------------------------------------------------*
 *	usage display and exit
 *---------------------------------------------------------------------------*/
static void
usage(void)
{
	fprintf(stderr, "\n");
	fprintf(stderr, "isdnd - i4b ISDN manager daemon, version %02d.%02d.%d, %s %s\n", VERSION, REL, STEP, __DATE__, __TIME__);
#ifdef DEBUG
	fprintf(stderr, "  usage: isdnd [-c file] [-d level] [-F] [-f [-r dev] [-t termtype]]\n");
#else
	fprintf(stderr, "  usage: isdnd [-c file] [-F] [-f [-r dev] [-t termtype]]\n");
#endif	
	fprintf(stderr, "               [-l] [-L file] [-m] [-s facility] [-u time]\n");
	fprintf(stderr, "    -c <filename> configuration file name (def: %s)\n", CONFIG_FILE_DEF);
#ifdef DEBUG
	fprintf(stderr, "    -d <level>    set debug flag bits:\n");
	fprintf(stderr, "                  general = 0x%04x, rates  = 0x%04x, timing   = 0x%04x\n", DL_MSG,   DL_RATES, DL_TIME);
	fprintf(stderr, "                  state   = 0x%04x, retry  = 0x%04x, dial     = 0x%04x\n", DL_STATE, DL_RCVRY, DL_DIAL);
	fprintf(stderr, "                  process = 0x%04x, kernio = 0x%04x, ctrlstat = 0x%04x\n", DL_PROC,  DL_DRVR,  DL_CNST);
	fprintf(stderr, "                  rc-file = 0x%04x, budget = 0x%04x, valid    = 0x%04x\n", DL_RCCF,  DL_BDGT, DL_VALID);
	fprintf(stderr, "    -dn           no debug output on fullscreen display\n");
#endif
	fprintf(stderr, "    -f            fullscreen status display\n");
	fprintf(stderr, "    -F            do not become a daemon process\n");
	fprintf(stderr, "    -l            use a logfile instead of syslog\n");
	fprintf(stderr, "    -L <file>     use file instead of %s for logging\n", LOG_FILE_DEF);
	fprintf(stderr, "    -P            pretty print real config to stdout and exit\n");
	fprintf(stderr, "    -r <device>   redirect output to other device    (for -f)\n");
	fprintf(stderr, "    -s <facility> use facility instead of %d for syslog logging\n", LOG_LOCAL0 >> 3);
	fprintf(stderr, "    -t <termtype> terminal type of redirected screen (for -f)\n");
	fprintf(stderr, "    -u <time>     length of a charging unit in seconds\n");
#ifdef I4B_EXTERNAL_MONITOR
	fprintf(stderr, "    -m            inhibit network/local monitoring (protocol %02d.%02d)\n", MPROT_VERSION, MPROT_REL);
#endif	
	fprintf(stderr, "\n");
	exit(1);
}

/*---------------------------------------------------------------------------*
 *	program entry
 *---------------------------------------------------------------------------*/
int
main(int argc, char **argv)
{
	int i;
	msg_vr_req_t mvr;
	
#ifdef I4B_EXTERNAL_MONITOR
	int sockfd = -1;		/* local monitor socket */
#ifndef I4B_NOTCPIP_MONITOR
	int remotesockfd = -1;		/* tcp/ip monitor socket */
#endif
#endif

	setlocale (LC_ALL, "");
	
	while ((i = getopt(argc, argv, "mc:d:fFlL:Pr:s:t:u:")) != -1)
	{
		switch (i)
		{
#ifdef I4B_EXTERNAL_MONITOR
			case 'm':
				inhibit_monitor = 1;
				break;
#endif
				
			case 'c':
				configfile = optarg;
				break;

#ifdef DEBUG				
			case 'd':
				if(*optarg == 'n')
					debug_noscreen = 1;
				else if((sscanf(optarg, "%i", &debug_flags)) == 1)
					do_debug = 1;
				else
					usage();				
				break;
#endif

			case 'f':
				do_fullscreen = 1;
				do_fork = 0;			
#ifndef USE_CURSES
				fprintf(stderr, "Sorry, no fullscreen mode available - daemon compiled without USE_CURSES\n");
				exit(1);
#endif
				break;

			case 'F':
				do_fork = 0;
				break;

			case 'l':
				uselogfile = 1;
				break;

			case 'L':
				strcpy(logfile, optarg);
				break;

			case 'P':
				do_print = 1;
				break;

			case 'r':
				rdev = optarg;
				do_rdev = 1;
				break;

			case 's':
				if(isdigit(*optarg))
				{
					int facility;
					logfacility = strtoul(optarg, NULL, 10);
					facility = logfacility << 3;

					if((facility < LOG_KERN) ||
					   (facility > LOG_FTP && facility < LOG_LOCAL0) ||
					   (facility > LOG_LOCAL7))
					{
						fprintf(stderr, "Error, option -s has invalid logging facility %d", logfacility);
						usage();
					}
					logfacility = facility;
				}
				else
				{
					fprintf(stderr, "Error: option -s requires a numeric argument!\n");
					usage();
				}
				break;

			case 't':
				ttype = optarg;
				do_ttytype = 1;
				break;

			case 'u':
				if(isdigit(*optarg))
				{
					unit_length = strtoul(optarg, NULL, 10);
					if(unit_length < ULSRC_CMDLMIN)
						unit_length = ULSRC_CMDLMIN;
					else if(unit_length > ULSRC_CMDLMAX)
						unit_length = ULSRC_CMDLMAX;
					got_unitlen = 1;
				}
				else
				{
					fprintf(stderr, "Error: option -T requires a numeric argument!\n");
					usage();
				}
				break;

			case '?':
			default:
				usage();
				break;
		}
	}
#ifdef DEBUG
	if(!do_debug)
		debug_noscreen = 0;
#endif

	if(!do_print)
	{
		umask(UMASK);	/* set our umask ... */	
	
		init_log();	/* initialize the logging subsystem */
	}
	
	check_pid();	/* check if we are already running */

	if(!do_print)
	{
		if(do_fork || (do_fullscreen && do_rdev)) /* daemon mode ? */
			daemonize();
	
		write_pid();	/* write our pid to file */
			
		/* set signal handler(s) */
	
		signal(SIGCHLD, sigchild_handler); /* process handling	*/
		signal(SIGHUP,  rereadconfig);	/* reread configuration	*/
		signal(SIGUSR1, reopenfiles);	/* reopen acct/log files*/
		signal(SIGPIPE, SIG_IGN);	/* handled manually	*/
		signal(SIGINT,  do_exit);	/* clean up on SIGINT	*/
		signal(SIGTERM, do_exit);	/* clean up on SIGTERM	*/
		signal(SIGQUIT, do_exit);	/* clean up on SIGQUIT	*/	
	}

	/* open isdn device */
	
	if((isdnfd = open(I4BDEVICE, O_RDWR)) < 0)
	{
		log(LL_ERR, "main: cannot open %s: %s", I4BDEVICE, strerror(errno));
		exit(1);
	}

	/* check kernel and userland have same version/release numbers */
	
	if((ioctl(isdnfd, I4B_VR_REQ, &mvr)) < 0)
	{
		log(LL_ERR, "main: ioctl I4B_VR_REQ failed: %s", strerror(errno));
		do_exit(1);
	}

	if(mvr.version != VERSION)
	{
		log(LL_ERR, "main: version mismatch, kernel %d, daemon %d", mvr.version, VERSION);
		do_exit(1);
	}

	if(mvr.release != REL)
	{
		log(LL_ERR, "main: release mismatch, kernel %d, daemon %d", mvr.release, REL);
		do_exit(1);
	}

	if(mvr.step != STEP)
	{
		log(LL_ERR, "main: step mismatch, kernel %d, daemon %d", mvr.step, STEP);
		do_exit(1);
	}

	/* init controller state array */

	init_controller();

	/* read runtime configuration file and configure ourselves */
	
	configure(configfile, 0);

	if(config_error_flag)
	{
		log(LL_ERR, "there were %d error(s) in the configuration file, terminating!", config_error_flag);
		exit(1);
	}

	/* set controller ISDN protocol */
	
	init_controller_protocol();
	
	/* init active controllers, if any */
	
	signal(SIGCHLD, SIG_IGN);		/*XXX*/

	init_active_controller();

	signal(SIGCHLD, sigchild_handler);	/*XXX*/
	
	/* handle the rates stuff */
	
	if((i = readrates(ratesfile)) == ERROR)
	{
		if(rate_error != NULL)
			log(LL_ERR, "%s", rate_error);
		exit(1);
	}

	if(i == GOOD)
	{
		got_rate = 1;	/* flag, ratesfile read and ok */
		DBGL(DL_RCCF, (log(LL_DBG, "ratesfile %s read successfully", ratesfile)));
	}
	else
	{
		if(rate_error != NULL)
			log(LL_WRN, "%s", rate_error);
	}

	/* if writing accounting info, open file, set unbuffered */
	
	if(useacctfile)
	{
		if((acctfp = fopen(acctfile, "a")) == NULL)
		{
			log(LL_ERR, "ERROR, can't open acctfile %s for writing, terminating!", acctfile);
			exit(1);
		}
		setvbuf(acctfp, (char *)NULL, _IONBF, 0);		
	}

	/* initialize alias processing */

	if(aliasing)
		init_alias(aliasfile);

	/* init holidays */
	
	init_holidays(holidayfile);		

	/* init remote monitoring */
	
#ifdef I4B_EXTERNAL_MONITOR
	if(do_monitor)
	{
		monitor_init();
		sockfd = monitor_create_local_socket();
#ifndef I4B_NOTCPIP_MONITOR
		remotesockfd = monitor_create_remote_socket(monitorport);
#endif
	}
#endif
	
	/* in case fullscreendisplay, initialize */

#ifdef USE_CURSES
	if(do_fullscreen)
	{
		init_screen();
	}
#endif

	/* init realtime priority */
  		
#ifdef USE_RTPRIO
  	if(rt_prio != RTPRIO_NOTUSED)
  	{
  		struct rtprio rtp;

  		rtp.type = RTP_PRIO_REALTIME;
  		rtp.prio = rt_prio;

  		if((rtprio(RTP_SET, getpid(), &rtp)) == -1)
  		{
			log(LL_ERR, "rtprio failed: %s", strerror(errno));
			do_exit(1);
		}
	}
#endif

	starttime = time(NULL);	/* get starttime */
	
	srandom(580403);	/* init random number gen */
	
	mloop(		/* enter loop of no return .. */
#ifdef I4B_EXTERNAL_MONITOR
		sockfd
#ifndef I4B_NOTCPIP_MONITOR
		, remotesockfd
#endif
#endif
		);
	do_exit(0);
	return(0);
}

/*---------------------------------------------------------------------------*
 *	program exit
 *---------------------------------------------------------------------------*/
void
do_exit(int exitval)
{
	close_allactive();

	unlink(PIDFILE);

	log(LL_DMN, "daemon terminating, exitval = %d", exitval);
	
#ifdef USE_CURSES
	if(do_fullscreen)
		endwin();
#endif

#ifdef I4B_EXTERNAL_MONITOR
	monitor_exit();
#endif

	exit(exitval);
}

/*---------------------------------------------------------------------------*
 *	program exit
 *---------------------------------------------------------------------------*/
void
error_exit(int exitval, const char *fmt, ...)
{
	close_allactive();

	unlink(PIDFILE);

	log(LL_DMN, "fatal error, daemon terminating, exitval = %d", exitval);
	
#ifdef USE_CURSES
	if(do_fullscreen)
		endwin();
#endif

#ifdef I4B_EXTERNAL_MONITOR
	monitor_exit();
#endif

	if(mailto[0] && mailer[0])
	{

#define EXITBL 2048

		char ebuffer[EXITBL];
		char sbuffer[EXITBL];
		va_list ap;

		va_start(ap, fmt);
		vsnprintf(ebuffer, EXITBL-1, fmt, ap);
		va_end(ap);

		signal(SIGCHLD, SIG_IGN);	/* remove handler */
		
		snprintf(sbuffer, sizeof(sbuffer), "%s%s%s%s%s%s%s%s",
			"cat << ENDOFDATA | ",
			mailer,
			" -s \"i4b isdnd: fatal error, terminating\" ",
			mailto,
			"\nThe isdnd terminated because of a fatal error:\n\n",
			ebuffer,
			"\n\nYours sincerely,\n   the isdnd\n",
			"\nENDOFDATA\n");
		system(sbuffer);
	}

	exit(exitval);
}

/*---------------------------------------------------------------------------*
 *	main loop
 *---------------------------------------------------------------------------*/
static void
mloop(
#ifdef I4B_EXTERNAL_MONITOR
	int localmonitor
#ifndef I4B_NOTCPIP_MONITOR
	, int remotemonitor
#endif
#endif
)
{
	fd_set set;
	struct timeval timeout;
	int ret;
	int high_selfd;

 	/* go into loop */
	
 	log(LL_DMN, "i4b isdn daemon started (pid = %d)", getpid());
 
	for(;;)
	{
		FD_ZERO(&set);

#ifdef USE_CURSES
		if(do_fullscreen)
			FD_SET(fileno(stdin), &set);
#endif

		FD_SET(isdnfd, &set);

		high_selfd = isdnfd;
		
#ifdef I4B_EXTERNAL_MONITOR
		if(do_monitor)
		{
			if (localmonitor != -1) {
				/* always watch for new connections */
				FD_SET(localmonitor, &set);
				if(localmonitor > high_selfd)
					high_selfd = localmonitor;
			}
#ifndef I4B_NOTCPIP_MONITOR
			if (remotemonitor != -1) {
				FD_SET(remotemonitor, &set);
				if(remotemonitor > high_selfd)
					high_selfd = remotemonitor;
			}
#endif

			/* if there are client connections, let monitor module
			 * enter them into the fdset */
			if(accepted)
			{
				monitor_prepselect(&set, &high_selfd);
			}
		}
#endif
		
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

		ret = select(high_selfd + 1, &set, NULL, NULL, &timeout);

		if(ret > 0)
		{	
			if(FD_ISSET(isdnfd, &set))
				isdnrdhdl();

#ifdef USE_CURSES
			if(FD_ISSET(fileno(stdin), &set))
				kbdrdhdl();
#endif

#ifdef I4B_EXTERNAL_MONITOR
			if(do_monitor)
			{
				if(localmonitor != -1 && FD_ISSET(localmonitor, &set))
					monitor_handle_connect(localmonitor, 1);

#ifndef I4B_NOTCPIP_MONITOR
				if(remotemonitor != -1 && FD_ISSET(remotemonitor, &set))
					monitor_handle_connect(remotemonitor, 0);
#endif
				if(accepted)
					monitor_handle_input(&set);
			}
#endif
		}
		else if(ret == -1)
		{
			if(errno != EINTR)
			{
				log(LL_ERR, "mloop: ERROR, select error on isdn device, errno = %d!", errno);
				error_exit(1, "mloop: ERROR, select error on isdn device, errno = %d!", errno);
			}
		}			

		/* handle timeout and recovery */		

		handle_recovery();
	}
}

#ifdef USE_CURSES
/*---------------------------------------------------------------------------*
 *	data from keyboard available, read and process it 
 *---------------------------------------------------------------------------*/
static void
kbdrdhdl(void)
{
	int ch = getch();

	if(ch == ERR)
	{
		log(LL_ERR, "kbdrdhdl: ERROR, read error on controlling tty, errno = %d!", errno);
		error_exit(1, "kbdrdhdl: ERROR, read error on controlling tty, errno = %d!", errno);
	}

	switch(ch)
	{
		case 0x0c:	/* control L */
			wrefresh(curscr);
			break;
		
		case '\n':
		case '\r':
			do_menu();
			break;
	}
}
#endif

/*---------------------------------------------------------------------------*
 *	data from /dev/isdn available, read and process them
 *---------------------------------------------------------------------------*/
static void
isdnrdhdl(void)
{
	static unsigned char msg_rd_buf[MSG_BUF_SIZ];
	msg_hdr_t *hp = (msg_hdr_t *)&msg_rd_buf[0];
	
	register int len;

	if((len = read(isdnfd, msg_rd_buf, MSG_BUF_SIZ)) > 0)
	{
		switch(hp->type)
		{
			case MSG_CONNECT_IND:				
				msg_connect_ind((msg_connect_ind_t *)msg_rd_buf);
				break;
				
			case MSG_CONNECT_ACTIVE_IND:
				msg_connect_active_ind((msg_connect_active_ind_t *)msg_rd_buf);
				break;

			case MSG_DISCONNECT_IND:
				msg_disconnect_ind((msg_disconnect_ind_t *)msg_rd_buf);
				break;
				
			case MSG_DIALOUT_IND:
				msg_dialout((msg_dialout_ind_t *)msg_rd_buf);
				break;

			case MSG_ACCT_IND:
				msg_accounting((msg_accounting_ind_t *)msg_rd_buf);
				break;

			case MSG_IDLE_TIMEOUT_IND:
				msg_idle_timeout_ind((msg_idle_timeout_ind_t *)msg_rd_buf);
				break;

			case MSG_CHARGING_IND:
				msg_charging_ind((msg_charging_ind_t *)msg_rd_buf);
				break;

			case MSG_PROCEEDING_IND:
				msg_proceeding_ind((msg_proceeding_ind_t *)msg_rd_buf);
				break;

			case MSG_ALERT_IND:
				msg_alert_ind((msg_alert_ind_t *)msg_rd_buf);
				break;

			case MSG_DRVRDISC_REQ:
				msg_drvrdisc_req((msg_drvrdisc_req_t *)msg_rd_buf);
				break;

			case MSG_L12STAT_IND:
				msg_l12stat_ind((msg_l12stat_ind_t *)msg_rd_buf);
				break;

			case MSG_TEIASG_IND:
				msg_teiasg_ind((msg_teiasg_ind_t *)msg_rd_buf);
				break;

			case MSG_PDEACT_IND:
				msg_pdeact_ind((msg_pdeact_ind_t *)msg_rd_buf);
				break;

			case MSG_NEGCOMP_IND:
				msg_negcomplete_ind((msg_negcomplete_ind_t *)msg_rd_buf);
				break;

			case MSG_IFSTATE_CHANGED_IND:
				msg_ifstatechg_ind((msg_ifstatechg_ind_t *)msg_rd_buf);
				break;

			case MSG_DIALOUTNUMBER_IND:
				msg_dialoutnumber((msg_dialoutnumber_ind_t *)msg_rd_buf);
				break;

			case MSG_PACKET_IND:
				msg_packet_ind((msg_packet_ind_t *)msg_rd_buf);
				break;

			default:
				log(LL_WRN, "ERROR, unknown message received from %sisdn (0x%x)", _PATH_DEV, msg_rd_buf[0]);
				break;
		}
	}
	else
	{
		log(LL_WRN, "ERROR, read error on isdn device, errno = %d, length = %d", errno, len);
	}
}

/*---------------------------------------------------------------------------*
 *	re-read the config file on SIGHUP or menu command
 *---------------------------------------------------------------------------*/
void
rereadconfig(int dummy)
{
	extern int entrycount;

	log(LL_DMN, "re-reading configuration file");
	
	close_allactive();

#if I4B_EXTERNAL_MONITOR
	monitor_clear_rights();
#endif

	entrycount = -1;
	nentries = 0;
	
	/* read runtime configuration file and configure ourselves */
	
	configure(configfile, 1);

	if(config_error_flag)
	{
		log(LL_ERR, "rereadconfig: there were %d error(s) in the configuration file, terminating!", config_error_flag);
		error_exit(1, "rereadconfig: there were %d error(s) in the configuration file, terminating!", config_error_flag);
	}

	if(aliasing)
	{
		/* reread alias database */
		free_aliases();
		init_alias(aliasfile);
	}
}

/*---------------------------------------------------------------------------*
 *	re-open the log/acct files on SIGUSR1
 *---------------------------------------------------------------------------*/
void
reopenfiles(int dummy)
{
        if(useacctfile)
	{
		/* close file */
		
	        fflush(acctfp);
	        fclose(acctfp);

	        /* if user specified a suffix, rename the old file */
	        
	        if(rotatesuffix[0] != '\0')
	        {
	        	char filename[MAXPATHLEN];

	        	snprintf(filename, sizeof(filename), "%s%s", acctfile, rotatesuffix);

			if((rename(acctfile, filename)) != 0)
			{
				log(LL_ERR, "reopenfiles: acct rename failed, cause = %s", strerror(errno));
				error_exit(1, "reopenfiles: acct rename failed, cause = %s", strerror(errno));
			}
		}

		if((acctfp = fopen(acctfile, "a")) == NULL)
		{
			log(LL_ERR, "ERROR, can't open acctfile %s for writing, terminating!", acctfile);
			error_exit(1, "ERROR, can't open acctfile %s for writing, terminating!", acctfile);
		}
		setvbuf(acctfp, (char *)NULL, _IONBF, 0);
	}

	if(uselogfile)
	{
	        finish_log();

	        /* if user specified a suffix, rename the old file */
	        
	        if(rotatesuffix[0] != '\0')
	        {
	        	char filename[MAXPATHLEN];

	        	snprintf(filename, sizeof(filename), "%s%s", logfile, rotatesuffix);

			if((rename(logfile, filename)) != 0)
			{
				log(LL_ERR, "reopenfiles: log rename failed, cause = %s", strerror(errno));
				error_exit(1, "reopenfiles: log rename failed, cause = %s", strerror(errno));
			}
		}

	        if((logfp = fopen(logfile, "a")) == NULL)
		{
			fprintf(stderr, "ERROR, cannot open logfile %s: %s\n",
				logfile, strerror(errno));
			error_exit(1, "reopenfiles: ERROR, cannot open logfile %s: %s\n",
				logfile, strerror(errno));
		}

		/* set unbuffered operation */

		setvbuf(logfp, (char *)NULL, _IONBF, 0);
	}
}

/* EOF */
