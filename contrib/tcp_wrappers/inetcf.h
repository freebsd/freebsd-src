 /*
  * @(#) inetcf.h 1.1 94/12/28 17:42:30
  * 
  * Author: Wietse Venema, Eindhoven University of Technology, The Netherlands.
  */

extern char *inet_cfg(char *conf);		/* read inetd.conf file */
extern void inet_set(char *name, int type);	/* remember internet service */
extern int inet_get(char *name);		/* look up internet service */

#define	WR_UNKNOWN	(-1)		/* service unknown */
#define	WR_NOT		1		/* may not be wrapped */
#define	WR_MAYBE	2		/* may be wrapped */
#define	WR_YES		3		/* service is wrapped */
