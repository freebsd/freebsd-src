/*
 * define path names
 *
 * $Id: pathnames.h,v 1.2 1994/03/30 09:31:39 jkh Exp $
 */

#ifdef STREAMS
#define _PATH_PIDFILE 	"/etc/ppp"
#else
#define _PATH_PIDFILE 	"/var/run"
#endif

#define _PATH_UPAPFILE 	"/etc/ppp/pap-secrets"
#define _PATH_CHAPFILE 	"/etc/ppp/chap-secrets"
#define _PATH_SYSOPTIONS "/etc/ppp/options"
