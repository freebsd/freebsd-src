/*
 * pppd.h - PPP daemon global declarations.
 *
 * Copyright (c) 1989 Carnegie Mellon University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by Carnegie Mellon University.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Id: pppd.h,v 1.9 1997/08/19 17:52:46 peter Exp $
 */

/*
 * TODO:
 */

#ifndef __PPPD_H__
#define __PPPD_H__

#include <stdio.h>		/* for FILE */
#include <sys/param.h>		/* for MAXPATHLEN and BSD4_4, if defined */
#include <sys/types.h>		/* for u_int32_t, if defined */
#include <sys/time.h>		/* for struct timeval */
#include <net/ppp_defs.h>

#if __STDC__
#include <stdarg.h>
#define __V(x)	x
#else
#include <varargs.h>
#define __V(x)	(va_alist) va_dcl
#define const
#endif

/*
 * Limits.
 */

#define NUM_PPP		1	/* One PPP interface supported (per process) */
#define MAXWORDLEN	1024	/* max length of word in file (incl null) */
#define MAXARGS		1	/* max # args to a command */
#define MAXNAMELEN	256	/* max length of hostname or name for auth */
#define MAXSECRETLEN	256	/* max length of password or secret */

/*
 * Global variables.
 */

extern int	hungup;		/* Physical layer has disconnected */
extern int	ifunit;		/* Interface unit number */
extern char	ifname[];	/* Interface name */
extern int	ttyfd;		/* Serial device file descriptor */
extern char	hostname[];	/* Our hostname */
extern u_char	outpacket_buf[]; /* Buffer for outgoing packets */
extern int	phase;		/* Current state of link - see values below */
extern int	baud_rate;	/* Current link speed in bits/sec */
extern char	*progname;	/* Name of this program */
extern int	redirect_stderr;/* Connector's stderr should go to file */
extern char	peer_authname[];/* Authenticated name of peer */
extern int	privileged;	/* We were run by real-uid root */
extern int	need_holdoff;	/* Need holdoff period after link terminates */

/*
 * Variables set by command-line options.
 */

extern int	debug;		/* Debug flag */
extern int	kdebugflag;	/* Tell kernel to print debug messages */
extern int	default_device;	/* Using /dev/tty or equivalent */
extern char	devnam[];	/* Device name */
extern int	crtscts;	/* Use hardware flow control */
extern int	modem;		/* Use modem control lines */
extern int	inspeed;	/* Input/Output speed requested */
extern u_int32_t netmask;	/* IP netmask to set on interface */
extern int	lockflag;	/* Create lock file to lock the serial dev */
extern int	nodetach;	/* Don't detach from controlling tty */
extern char	*connector;	/* Script to establish physical link */
extern char	*disconnector;	/* Script to disestablish physical link */
extern char	*welcomer;	/* Script to welcome client after connection */
extern int	maxconnect;	/* Maximum connect time (seconds) */
extern char	user[];		/* Our name for authenticating ourselves */
extern char	passwd[];	/* Password for PAP */
extern int	auth_required;	/* Peer is required to authenticate */
extern int	proxyarp;	/* Set up proxy ARP entry for peer */
extern int	persist;	/* Reopen link after it goes down */
extern int	uselogin;	/* Use /etc/passwd for checking PAP */
extern int	lcp_echo_interval; /* Interval between LCP echo-requests */
extern int	lcp_echo_fails;	/* Tolerance to unanswered echo-requests */
extern char	our_name[];	/* Our name for authentication purposes */
extern char	remote_name[];	/* Peer's name for authentication */
extern int	explicit_remote;/* remote_name specified with remotename opt */
extern int	usehostname;	/* Use hostname for our_name */
extern int	disable_defaultip; /* Don't use hostname for default IP adrs */
extern int	demand;		/* Do dial-on-demand */
extern char	*ipparam;	/* Extra parameter for ip up/down scripts */
extern int	cryptpap;	/* Others' PAP passwords are encrypted */
extern int	idle_time_limit;/* Shut down link if idle for this long */
extern int	holdoff;	/* Dead time before restarting */
extern int	refuse_pap;	/* Don't wanna auth. ourselves with PAP */
extern int	refuse_chap;	/* Don't wanna auth. ourselves with CHAP */
#ifdef PPP_FILTER
extern struct	bpf_program pass_filter;   /* Filter for pkts to pass */
extern struct	bpf_program active_filter; /* Filter for link-active pkts */
#endif


#ifdef MSLANMAN
extern int	ms_lanman;	/* Nonzero if use LanMan password instead of NT */
				/* Has meaning only with MS-CHAP challenges */
#endif

/*
 * Values for phase.
 */
#define PHASE_DEAD		0
#define PHASE_INITIALIZE	1
#define PHASE_DORMANT		2
#define PHASE_ESTABLISH		3
#define PHASE_AUTHENTICATE	4
#define PHASE_CALLBACK		5
#define PHASE_NETWORK		6
#define PHASE_TERMINATE		7
#define PHASE_HOLDOFF		8

/*
 * The following struct gives the addresses of procedures to call
 * for a particular protocol.
 */
struct protent {
    u_short protocol;		/* PPP protocol number */
    /* Initialization procedure */
    void (*init) __P((int unit));
    /* Process a received packet */
    void (*input) __P((int unit, u_char *pkt, int len));
    /* Process a received protocol-reject */
    void (*protrej) __P((int unit));
    /* Lower layer has come up */
    void (*lowerup) __P((int unit));
    /* Lower layer has gone down */
    void (*lowerdown) __P((int unit));
    /* Open the protocol */
    void (*open) __P((int unit));
    /* Close the protocol */
    void (*close) __P((int unit, char *reason));
    /* Print a packet in readable form */
    int  (*printpkt) __P((u_char *pkt, int len,
			  void (*printer) __P((void *, char *, ...)),
			  void *arg));
    /* Process a received data packet */
    void (*datainput) __P((int unit, u_char *pkt, int len));
    int  enabled_flag;		/* 0 iff protocol is disabled */
    char *name;			/* Text name of protocol */
    /* Check requested options, assign defaults */
    void (*check_options) __P((void));
    /* Configure interface for demand-dial */
    int  (*demand_conf) __P((int unit));
    /* Say whether to bring up link for this pkt */
    int  (*active_pkt) __P((u_char *pkt, int len));
};

/* Table of pointers to supported protocols */
extern struct protent *protocols[];

/*
 * Prototypes.
 */

/* Procedures exported from main.c. */
void die __P((int));		/* Cleanup and exit */
void quit __P((void));		/* like die(1) */
void novm __P((char *));	/* Say we ran out of memory, and die */
void timeout __P((void (*func)(void *), void *arg, int t));
				/* Call func(arg) after t seconds */
void untimeout __P((void (*func)(void *), void *arg));
				/* Cancel call to func(arg) */
int run_program __P((char *prog, char **args, int must_exist));
				/* Run program prog with args in child */
void demuxprotrej __P((int, int));
				/* Demultiplex a Protocol-Reject */
void format_packet __P((u_char *, int, void (*) (void *, char *, ...),
		void *));	/* Format a packet in human-readable form */
void log_packet __P((u_char *, int, char *, int));
				/* Format a packet and log it with syslog */
void print_string __P((char *, int,  void (*) (void *, char *, ...),
		void *));	/* Format a string for output */
int fmtmsg __P((char *, int, char *, ...));		/* sprintf++ */
int vfmtmsg __P((char *, int, char *, va_list));	/* vsprintf++ */

/* Procedures exported from auth.c */
void link_required __P((int));	  /* we are starting to use the link */
void link_terminated __P((int));  /* we are finished with the link */
void link_down __P((int));	  /* the LCP layer has left the Opened state */
void link_established __P((int)); /* the link is up; authenticate now */
void np_up __P((int, int));	  /* a network protocol has come up */
void np_down __P((int, int));	  /* a network protocol has gone down */
void np_finished __P((int, int)); /* a network protocol no longer needs link */
void auth_peer_fail __P((int, int));
				/* peer failed to authenticate itself */
void auth_peer_success __P((int, int, char *, int));
				/* peer successfully authenticated itself */
void auth_withpeer_fail __P((int, int));
				/* we failed to authenticate ourselves */
void auth_withpeer_success __P((int, int));
				/* we successfully authenticated ourselves */
void auth_check_options __P((void));
				/* check authentication options supplied */
void auth_reset __P((int));	/* check what secrets we have */
int  check_passwd __P((int, char *, int, char *, int, char **, int *));
				/* Check peer-supplied username/password */
int  get_secret __P((int, char *, char *, char *, int *, int));
				/* get "secret" for chap */
int  auth_ip_addr __P((int, u_int32_t));
				/* check if IP address is authorized */
int  bad_ip_adrs __P((u_int32_t));
				/* check if IP address is unreasonable */
void check_access __P((FILE *, char *));
				/* check permissions on secrets file */

/* Procedures exported from demand.c */
void demand_conf __P((void));	/* config interface(s) for demand-dial */
void demand_block __P((void));	/* set all NPs to queue up packets */
void demand_unblock __P((void)); /* set all NPs to pass packets */
void demand_discard __P((void)); /* set all NPs to discard packets */
void demand_rexmit __P((int));	/* retransmit saved frames for an NP */
int  loop_chars __P((unsigned char *, int)); /* process chars from loopback */
int  loop_frame __P((unsigned char *, int)); /* process frame from loopback */

/* Procedures exported from sys-*.c */
void sys_init __P((void));	/* Do system-dependent initialization */
void sys_cleanup __P((void));	/* Restore system state before exiting */
void sys_check_options __P((void)); /* Check options specified */
void sys_close __P((void));	/* Clean up in a child before execing */
int  ppp_available __P((void));	/* Test whether ppp kernel support exists */
void open_ppp_loopback __P((void)); /* Open loopback for demand-dialling */
void establish_ppp __P((int));	/* Turn serial port into a ppp interface */
void restore_loop __P((void));	/* Transfer ppp unit back to loopback */
void disestablish_ppp __P((int)); /* Restore port to normal operation */
void clean_check __P((void));	/* Check if line was 8-bit clean */
void set_up_tty __P((int, int)); /* Set up port's speed, parameters, etc. */
void restore_tty __P((int));	/* Restore port's original parameters */
void setdtr __P((int, int));	/* Raise or lower port's DTR line */
void output __P((int, u_char *, int)); /* Output a PPP packet */
void wait_input __P((struct timeval *));
				/* Wait for input, with timeout */
void wait_loop_output __P((struct timeval *));
				/* Wait for pkt from loopback, with timeout */
void wait_time __P((struct timeval *)); /* Wait for given length of time */
int  read_packet __P((u_char *)); /* Read PPP packet */
int  get_loop_output __P((void)); /* Read pkts from loopback */
void ppp_send_config __P((int, int, u_int32_t, int, int));
				/* Configure i/f transmit parameters */
void ppp_set_xaccm __P((int, ext_accm));
				/* Set extended transmit ACCM */
void ppp_recv_config __P((int, int, u_int32_t, int, int));
				/* Configure i/f receive parameters */
int  ccp_test __P((int, u_char *, int, int));
				/* Test support for compression scheme */
void ccp_flags_set __P((int, int, int));
				/* Set kernel CCP state */
int  ccp_fatal_error __P((int)); /* Test for fatal decomp error in kernel */
int  get_idle_time __P((int, struct ppp_idle *));
				/* Find out how long link has been idle */
int  sifvjcomp __P((int, int, int, int));
				/* Configure VJ TCP header compression */
int  sifup __P((int));		/* Configure i/f up (for IP) */
int  sifnpmode __P((int u, int proto, enum NPmode mode));
				/* Set mode for handling packets for proto */
int  sifdown __P((int));	/* Configure i/f down (for IP) */
int  sifaddr __P((int, u_int32_t, u_int32_t, u_int32_t));
				/* Configure IP addresses for i/f */
int  cifaddr __P((int, u_int32_t, u_int32_t));
				/* Reset i/f IP addresses */
int  sifdefaultroute __P((int, u_int32_t, u_int32_t));
				/* Create default route through i/f */
int  cifdefaultroute __P((int, u_int32_t, u_int32_t));
				/* Delete default route through i/f */
int  sifproxyarp __P((int, u_int32_t));
				/* Add proxy ARP entry for peer */
int  cifproxyarp __P((int, u_int32_t));
				/* Delete proxy ARP entry for peer */
u_int32_t GetMask __P((u_int32_t)); /* Get appropriate netmask for address */
int  lock __P((char *));	/* Create lock file for device */
void unlock __P((void));	/* Delete previously-created lock file */
int  daemon __P((int, int));	/* Detach us from terminal session */
void logwtmp __P((const char *, const char *, const char *));
				/* Write entry to wtmp file */
#ifdef PPP_FILTER
int  set_filters __P((struct bpf_program *pass, struct bpf_program *active));
				/* Set filter programs in kernel */
#endif

/* Procedures exported from options.c */
int  parse_args __P((int argc, char **argv));
				/* Parse options from arguments given */
void usage __P((void));		/* Print a usage message */
int  options_from_file __P((char *filename, int must_exist, int check_prot,
			    int privileged));
				/* Parse options from an options file */
int  options_from_user __P((void)); /* Parse options from user's .ppprc */
int  options_for_tty __P((void)); /* Parse options from /etc/ppp/options.tty */
void scan_args __P((int argc, char **argv));
				/* Look for tty name in command-line args */
int  getword __P((FILE *f, char *word, int *newlinep, char *filename));
				/* Read a word from a file */
void option_error __P((char *fmt, ...));
				/* Print an error message about an option */
int setipaddr __P((char *));	/* set IP addresses */


/*
 * This structure is used to store information about certain
 * options, such as where the option value came from (/etc/ppp/options,
 * command line, etc.) and whether it came from a privileged source.
 */

struct option_info {
    int	    priv;		/* was value set by sysadmin? */
    char    *source;		/* where option came from */
};

extern struct option_info auth_req_info;
extern struct option_info connector_info;
extern struct option_info disconnector_info;
extern struct option_info welcomer_info;
extern struct option_info devnam_info;

/*
 * Inline versions of get/put char/short/long.
 * Pointer is advanced; we assume that both arguments
 * are lvalues and will already be in registers.
 * cp MUST be u_char *.
 */
#define GETCHAR(c, cp) { \
	(c) = *(cp)++; \
}
#define PUTCHAR(c, cp) { \
	*(cp)++ = (u_char) (c); \
}


#define GETSHORT(s, cp) { \
	(s) = *(cp)++ << 8; \
	(s) |= *(cp)++; \
}
#define PUTSHORT(s, cp) { \
	*(cp)++ = (u_char) ((s) >> 8); \
	*(cp)++ = (u_char) (s); \
}

#define GETLONG(l, cp) { \
	(l) = *(cp)++ << 8; \
	(l) |= *(cp)++; (l) <<= 8; \
	(l) |= *(cp)++; (l) <<= 8; \
	(l) |= *(cp)++; \
}
#define PUTLONG(l, cp) { \
	*(cp)++ = (u_char) ((l) >> 24); \
	*(cp)++ = (u_char) ((l) >> 16); \
	*(cp)++ = (u_char) ((l) >> 8); \
	*(cp)++ = (u_char) (l); \
}

#define INCPTR(n, cp)	((cp) += (n))
#define DECPTR(n, cp)	((cp) -= (n))

#undef  FALSE
#define FALSE	0
#undef  TRUE
#define TRUE	1

/*
 * System dependent definitions for user-level 4.3BSD UNIX implementation.
 */

#define DEMUXPROTREJ(u, p)	demuxprotrej(u, p)

#define TIMEOUT(r, f, t)	timeout((r), (f), (t))
#define UNTIMEOUT(r, f)		untimeout((r), (f))

#define BCOPY(s, d, l)		memcpy(d, s, l)
#define BZERO(s, n)		memset(s, 0, n)
#define EXIT(u)			quit()

#define PRINTMSG(m, l)	{ m[l] = '\0'; syslog(LOG_INFO, "Remote message: %s", m); }

/*
 * MAKEHEADER - Add Header fields to a packet.
 */
#define MAKEHEADER(p, t) { \
    PUTCHAR(PPP_ALLSTATIONS, p); \
    PUTCHAR(PPP_UI, p); \
    PUTSHORT(t, p); }


#ifdef DEBUGALL
#define DEBUGMAIN	1
#define DEBUGFSM	1
#define DEBUGLCP	1
#define DEBUGIPCP	1
#define DEBUGUPAP	1
#define DEBUGCHAP	1
#endif

#ifndef LOG_PPP			/* we use LOG_LOCAL2 for syslog by default */
#if defined(DEBUGMAIN) || defined(DEBUGFSM) || defined(DEBUGSYS) \
  || defined(DEBUGLCP) || defined(DEBUGIPCP) || defined(DEBUGUPAP) \
  || defined(DEBUGCHAP) || defined(DEBUG)
#define LOG_PPP LOG_LOCAL2
#else
#define LOG_PPP LOG_DAEMON
#endif
#endif /* LOG_PPP */

#ifdef DEBUGMAIN
#define MAINDEBUG(x)	if (debug) syslog x
#else
#define MAINDEBUG(x)
#endif

#ifdef DEBUGSYS
#define SYSDEBUG(x)	if (debug) syslog x
#else
#define SYSDEBUG(x)
#endif

#ifdef DEBUGFSM
#define FSMDEBUG(x)	if (debug) syslog x
#else
#define FSMDEBUG(x)
#endif

#ifdef DEBUGLCP
#define LCPDEBUG(x)	if (debug) syslog x
#else
#define LCPDEBUG(x)
#endif

#ifdef DEBUGIPCP
#define IPCPDEBUG(x)	if (debug) syslog x
#else
#define IPCPDEBUG(x)
#endif

#ifdef DEBUGUPAP
#define UPAPDEBUG(x)	if (debug) syslog x
#else
#define UPAPDEBUG(x)
#endif

#ifdef DEBUGCHAP
#define CHAPDEBUG(x)	if (debug) syslog x
#else
#define CHAPDEBUG(x)
#endif

#ifdef DEBUGIPXCP
#define IPXCPDEBUG(x)	if (debug) syslog x
#else
#define IPXCPDEBUG(x)
#endif

#ifndef SIGTYPE
#if defined(sun) || defined(SYSV) || defined(POSIX_SOURCE)
#define SIGTYPE void
#else
#define SIGTYPE int
#endif /* defined(sun) || defined(SYSV) || defined(POSIX_SOURCE) */
#endif /* SIGTYPE */

#ifndef MIN
#define MIN(a, b)	((a) < (b)? (a): (b))
#endif
#ifndef MAX
#define MAX(a, b)	((a) > (b)? (a): (b))
#endif

#endif /* __PPP_H__ */
