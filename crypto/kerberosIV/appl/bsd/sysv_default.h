/* Author: Wietse Venema <wietse@wzv.win.tue.nl> */

/* $Id: sysv_default.h,v 1.5 1996/10/27 23:51:14 assar Exp $ */

extern char *default_console;
extern char *default_altsh;
extern char *default_passreq;
extern char *default_timezone;
extern char *default_hz;
extern char *default_path;
extern char *default_supath;
extern char *default_ulimit;
extern char *default_timeout;
extern char *default_umask;
extern char *default_sleep;
extern char *default_maxtrys;

void sysv_defaults(void);
