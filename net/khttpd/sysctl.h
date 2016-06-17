#ifndef _KHTTPD_INCLUDE_GUARD_SYSCTL_H
#define _KHTTPD_INCLUDE_GUARD_SYSCTL_H

extern char 	sysctl_khttpd_docroot[200];
extern int 	sysctl_khttpd_stop;
extern int 	sysctl_khttpd_start;
extern int 	sysctl_khttpd_unload;
extern int 	sysctl_khttpd_clientport;
extern int 	sysctl_khttpd_permreq;
extern int 	sysctl_khttpd_permforbid;
extern int 	sysctl_khttpd_logging;
extern int 	sysctl_khttpd_serverport;
extern int 	sysctl_khttpd_sloppymime;
extern int 	sysctl_khttpd_threads;
extern int	sysctl_khttpd_maxconnect;

/* incremented each time sysctl_khttpd_stop goes nonzero */
extern atomic_t	khttpd_stopCount;

#endif
