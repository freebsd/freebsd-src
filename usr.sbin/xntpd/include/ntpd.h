/*
 * ntpd.h - Prototypes for xntpd.
 */

#include "ntp_syslog.h"
#include "ntp_fp.h"
#include "ntp.h"
#include "ntp_malloc.h"

/* ntp_config.c */
extern	void	getstartup	P((int, char **));
extern	void	getconfig	P((int, char **));

/* ntp_config.c */
extern	void	ctl_clr_stats	P((void));
extern	int	ctlclrtrap	P((struct sockaddr_in *, struct interface *, int));
extern	u_short	ctlpeerstatus	P((struct peer *));
extern	int	ctlsettrap	P((struct sockaddr_in *, struct interface *, int, int));
extern	u_short	ctlsysstatus	P((void));
extern	void	init_control	P((void));
extern	void	process_control	P((struct recvbuf *, int));
extern	void	report_event	P((int, struct peer *));

/* ntp_control.c */
/*
 * Structure for translation tables between internal system
 * variable indices and text format.
 */
struct ctl_var {
	u_short code;
	u_short flags;
	char *text;
};
/*
 * Flag values
 */
#define	CAN_READ	0x01
#define	CAN_WRITE	0x02

#define DEF		0x20
#define	PADDING		0x40
#define	EOV		0x80

#define	RO	(CAN_READ)
#define	WO	(CAN_WRITE)
#define	RW	(CAN_READ|CAN_WRITE)

extern  char *  add_var P((struct ctl_var **, unsigned long, int));
extern  void    free_varlist P((struct ctl_var *));
extern  void    set_var P((struct ctl_var **, char *, unsigned long, int));
extern  void    set_sys_var P((char *, unsigned long, int));

/* ntp_intres.c */
extern	void	ntp_intres	P((void));

/* ntp_io.c */
extern	struct interface *findbcastinter P((struct sockaddr_in *));
extern	struct interface *findinterface P((struct sockaddr_in *));
extern	void	freerecvbuf	P((struct recvbuf *));
extern	struct recvbuf *getrecvbufs	P((void));
extern	void	init_io		P((void));
extern  void    input_handler   P((l_fp *));
extern	void	io_clr_stats	P((void));
extern	void	io_setbclient	P((void));
extern	void	io_unsetbclient	P((void));
extern	void	sendpkt		P((struct sockaddr_in *, struct interface *, struct pkt *, int));
#ifdef HAVE_SIGNALED_IO
extern  void	wait_for_signal P((void));
extern  void    unblock_io_and_alarm P((void));
extern	void	block_io_and_alarm P((void));
#endif

/* ntp_leap.c */
extern	void	init_leap	P((void));
extern	void	leap_process	P((void));
extern	int	leap_setleap	P((int, int));
/*
 * there seems to be a bug in the IRIX 4 compiler which prevents
 * u_char from beeing used in prototyped functions.
 * This is also true AIX compiler.
 * So give up and define it to be int. WLJ
 */
extern	int	leap_actual	P((int));

/* ntp_loopfilter.c */
extern	void	init_loopfilter	P((void));
extern	int	local_clock	P((l_fp *, struct peer *));
extern	void	adj_host_clock	P((void));
extern	void	loop_config	P((int, l_fp *, int));
#if defined(PPS) || defined(PPSPPS) || defined(PPSCD)
extern	int	pps_sample	P((l_fp *));
#endif /* PPS || PPSDEV || PPSCD */

/* ntp_monitor.c */
extern	void	init_mon	P((void));
extern	void	mon_start	P((int));
extern	void	mon_stop	P((int));
extern	void	monitor		P((struct recvbuf *));

/* ntp_peer.c */
extern	void	init_peer	P((void));
extern	struct peer *findexistingpeer P((struct sockaddr_in *, struct peer *));
extern	struct peer *findpeer	P((struct sockaddr_in *, struct interface *));
extern	struct peer *findpeerbyassoc P((int));
extern	struct peer *newpeer	P((struct sockaddr_in *, struct interface *, int, int, int, int, U_LONG));
extern	void	peer_all_reset	P((void));
extern	void	peer_clr_stats	P((void));
extern	struct peer *peer_config P((struct sockaddr_in *, struct interface *, int, int, int, int, U_LONG, int));
extern	void	peer_reset	P((struct peer *));
extern	int	peer_unconfig	P((struct sockaddr_in *, struct interface *));
extern	void	unpeer		P((struct peer *));

/* ntp_proto.c */
extern	void	transmit	P((struct peer *));
extern	void	receive		P((struct recvbuf *));
extern	void	peer_clear	P((struct peer *));
extern	int	process_packet	P((struct peer *, struct pkt *, l_fp *, int, int));
extern	void	clock_update	P((struct peer *));

/*
 * there seems to be a bug in the IRIX 4 compiler which prevents
 * u_char from beeing used in prototyped functions.
 * This is also true AIX compiler.
 * So give up and define it to be int. WLJ
 */
extern	void	poll_update	P((struct peer *, unsigned int, int));

extern	void	clear		P((struct peer *));
extern	void	clock_filter	P((struct peer *, l_fp *, s_fp, u_fp));
extern	void	clock_select	P((void));
extern	void	clock_combine	P((struct peer **, int));
extern	void	fast_xmit	P((struct recvbuf *, int, int));
extern	void	init_proto	P((void));
extern	void	proto_config	P((int, LONG));
extern	void	proto_clr_stats	P((void));

#ifdef	REFCLOCK
/* ntp_refclock.c */
extern	int	refclock_newpeer P((struct peer *));
extern	void	refclock_unpeer	P((struct peer *));
extern	void	refclock_receive P((struct peer *, l_fp *, s_fp, u_fp, l_fp *, l_fp *, int));
extern	void	refclock_leap	P((void));
extern	void	init_refclock	P((void));
#endif	/* REFCLOCK */

/* ntp_request.c */
extern	void	init_request	P((void));
extern	void	process_private	P((struct recvbuf *, int));

/* ntp_restrict.c */
extern	void	init_restrict	P((void));
extern	int	restrictions	P((struct sockaddr_in *));
extern	void	restrict	P((int, struct sockaddr_in *, struct sockaddr_in *, int, int));

/* ntp_timer.c */
extern	void	init_timer	P((void));
extern	void	timer		P((void));
extern	void	timer_clr_stats	P((void));

/* ntp_unixclock.c */
extern	void	init_systime	P((void));

/* ntp_util.c */
extern	void	init_util	P((void));
extern	void	hourly_stats	P((void));
extern	void	stats_config	P((int, char *));
extern	void	record_peer_stats P((struct sockaddr_in *, int, l_fp *, s_fp, u_fp));
extern	void	record_loop_stats P((l_fp *, s_fp *, int));
extern	void	record_clock_stats P((struct sockaddr_in *, char *));
extern	void	getauthkeys	P((char *));
extern	void	rereadkeys	P((void));
