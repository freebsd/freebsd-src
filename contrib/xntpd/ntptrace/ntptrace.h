/* ntptrace.h,v 3.1 1993/07/06 01:09:39 jbj Exp
 * ntptrace.h - declarations for the ntptrace program
 */

/*
 * The server structure is a much simplified version of the
 * peer structure, for ntptrace's use.  Since we always send
 * in client mode and expect to receive in server mode, this
 * leaves only a very limited number of things we need to
 * remember about the server.
 */
struct server {
	struct sockaddr_in srcadr;	/* address of remote host */
	u_char leap;			/* leap indicator */
	u_char stratum;			/* stratum of remote server */
	s_char precision;		/* server's clock precision */
	u_fp rootdelay;			/* distance from primary clock */
	u_fp rootdispersion;		/* peer clock dispersion */
	U_LONG refid;			/* peer reference ID */
	l_fp reftime;			/* time of peer's last update */
	l_fp org;			/* peer's originate time stamp */
	l_fp xmt;			/* transmit time stamp */
	u_fp delay;			/* filter estimated delay */
	u_fp dispersion;		/* filter estimated dispersion */
	l_fp offset;			/* filter estimated clock offset */
};


/*
 * Since ntptrace isn't aware of some of the things that normally get
 * put in an NTP packet, we fix some values.
 */
#define	NTPTRACE_PRECISION	(-6)		/* use this precision */
#define	NTPTRACE_DISTANCE	FP_SECOND	/* distance is 1 sec */
#define	NTPTRACE_DISP		FP_SECOND	/* so is the dispersion */
#define	NTPTRACE_REFID		(0)		/* reference ID to use */
