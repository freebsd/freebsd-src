/*
 * Copyright (c) 1992 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software_Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
 * SUP Communication Module for 4.3 BSD
 *
 * SUP COMMUNICATION MODULE SPECIFICATIONS:
 *
 * IN THIS MODULE:
 *
 * CONNECTION ROUTINES
 *
 *   FOR SERVER
 *	servicesetup (port)	establish TCP port connection
 *	  char *port;			name of service
 *	service ()		accept TCP port connection
 *	servicekill ()		close TCP port in use by another process
 *	serviceprep ()		close temp ports used to make connection
 *	serviceend ()		close TCP port
 *
 *   FOR CLIENT
 *	request (port,hostname,retry) establish TCP port connection
 *	  char *port,*hostname;		  name of service and host
 *	  int retry;			  true if retries should be used
 *	requestend ()		close TCP port
 *
 * HOST NAME CHECKING
 *	p = remotehost ()	remote host name (if known)
 *	  char *p;
 *	i = samehost ()		whether remote host is also this host
 *	  int i;
 *	i = matchhost (name)	whether remote host is same as name
 *	  int i;
 *	  char *name;
 *
 * RETURN CODES
 *	All procedures return values as indicated above.  Other routines
 *	normally return SCMOK on success, SCMERR on error.
 *
 * COMMUNICATION PROTOCOL
 *
 *	Described in scmio.c.
 *
 **********************************************************************
 * HISTORY
 *  2-Oct-92  Mary Thompson (mrt) at Carnegie-Mellon University
 *	Added conditional declarations of INADDR_NONE and INADDR_LOOPBACK
 *	since Tahoe version of <netinet/in.h> does not define them.
 *
 * $Log: scm.c,v $
 * Revision 1.1.1.1  1995/12/26 04:54:47  peter
 * Import the unmodified version of the sup that we are using.
 * The heritage of this version is not clear.  It appears to be NetBSD
 * derived from some time ago.
 *
 * Revision 1.2  1994/06/20  06:04:04  rgrimes
 * Humm.. they did a lot of #if __STDC__ but failed to properly prototype
 * the code.  Also fixed one bad argument to a wait3 call.
 *
 * It won't compile -Wall, but atleast it compiles standard without warnings
 * now.
 *
 * Revision 1.1.1.1  1993/08/21  00:46:33  jkh
 * Current sup with compression support.
 *
 * Revision 1.1.1.1  1993/05/21  14:52:17  cgd
 * initial import of CMU's SUP to NetBSD
 *
 * Revision 1.13  92/08/11  12:05:35  mrt
 * 	Added changes from stump:
 * 	  Allow for multiple interfaces, and for numeric addresses.
 * 	  Changed to use builtin port for the "supfiledbg"
 * 	    service when getservbyname() cannot find it.
 * 	  Added forward static declatations, delinted.
 * 	  Updated variable argument usage.
 * 	[92/08/08            mrt]
 * 
 * Revision 1.12  92/02/08  19:01:11  mja
 * 	Add (struct sockaddr *) casts for HC 2.1.
 * 	[92/02/08  18:59:09  mja]
 * 
 * Revision 1.11  89/08/03  19:49:03  mja
 * 	Updated to use v*printf() in place of _doprnt().
 * 	[89/04/19            mja]
 * 
 * 11-Feb-88  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Moved sleep into computeBackoff, renamed to dobackoff.
 *
 * 10-Feb-88  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Added timeout to backoff.
 *
 * 27-Dec-87  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Removed nameserver support.
 *
 * 09-Sep-87  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Fixed to depend less upon having name of remote host.
 *
 * 25-May-87  Doug Philips (dwp) at Carnegie-Mellon Universtiy
 *	Extracted backoff/sleeptime computation from "request" and
 *	created "computeBackoff" so that I could use it in sup.c when
 *	trying to get to nameservers as a group.
 *
 * 21-May-87  Chriss Stephens (chriss) at Carnegie Mellon University
 *	Merged divergent CS and EE versions.
 *
 * 02-May-87  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Added some bullet-proofing code around hostname calls.
 *
 * 31-Mar-87  Dan Nydick (dan) at Carnegie-Mellon University
 *	Fixed for 4.3.
 *
 * 30-May-86  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Added code to use known values for well-known ports if they are
 *	not found in the host table.
 *
 * 19-Feb-86  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Changed setsockopt SO_REUSEADDR to be non-fatal.  Added fourth
 *	parameter as described in 4.3 manual entry.
 *
 * 15-Feb-86  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Added call of readflush() to requestend() routine.
 *
 * 29-Dec-85  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Major rewrite for protocol version 4.  All read/write and crypt
 *	routines are now in scmio.c.
 *
 * 14-Dec-85  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Added setsockopt SO_REUSEADDR call.
 *
 * 01-Dec-85  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Removed code to "gracefully" handle unexpected messages.  This
 *	seems reasonable since it didn't work anyway, and should be
 *	handled at a higher level anyway by adhering to protocol version
 *	number conventions.
 *
 * 26-Nov-85  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Fixed scm.c to free space for remote host name when connection
 *	is closed.
 *
 * 07-Nov-85  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Fixed 4.2 retry code to reload sin values before retry.
 *
 * 22-Oct-85  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Added code to retry initial connection open request.
 *
 * 22-Sep-85  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Merged 4.1 and 4.2 versions together.
 *
 * 21-Sep-85  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Add close() calls after pipe() call.
 *
 * 12-Jun-85  Steven Shafer (sas) at Carnegie-Mellon University
 *	Converted for 4.2 sockets; added serviceprep() routine.
 *
 * 04-Jun-85  Steven Shafer (sas) at Carnegie-Mellon University
 *	Created for 4.2 BSD.
 *
 **********************************************************************
 */

#include <libc.h>
#include <errno.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netdb.h>
#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include "sup.h"

#ifndef INADDR_NONE
#define	INADDR_NONE		0xffffffff		/* -1 return */
#endif
#ifndef INADDR_LOOPBACK
#define	INADDR_LOOPBACK		(u_long)0x7f000001	/* 127.0.0.1 */
#endif

extern int errno;
static char *myhost ();

char scmversion[] = "4.3 BSD";

/*
 * PROTOTYPES
 */
#if __STDC__
int   scmerr  __P((int, FILE *, char *,...));
#endif
/*************************
 ***    M A C R O S    ***
 *************************/

/* networking parameters */
#define NCONNECTS 5

/*********************************************
 ***    G L O B A L   V A R I A B L E S    ***
 *********************************************/

extern char program[];			/* name of program we are running */
extern int progpid;			/* process id to display */

int netfile = -1;			/* network file descriptor */

static int sock = -1;			/* socket used to make connection */
static struct in_addr remoteaddr;	/* remote host address */
static char *remotename = NULL;		/* remote host name */
static int swapmode;			/* byte-swapping needed on server? */

/***************************************************
 ***    C O N N E C T I O N   R O U T I N E S    ***
 ***    F O R   S E R V E R                      ***
 ***************************************************/

servicesetup (server)		/* listen for clients */
char *server;
{
	struct sockaddr_in sin;
	struct servent *sp;
	short port;
	int one = 1;

	if (myhost () == NULL)
		return (scmerr (-1, stderr, "Local hostname not known"));
	if ((sp = getservbyname(server,"tcp")) == 0) {
		if (strcmp(server, FILEPORT) == 0)
			port = htons((u_short)FILEPORTNUM);
		else if (strcmp(server, DEBUGFPORT) == 0)
			port = htons((u_short)DEBUGFPORTNUM);
		else
			return (scmerr (-1, stderr, "Can't find %s server description",server));
		(void) scmerr (-1, stderr, "%s/tcp: unknown service: using port %d",
					server,port);
	} else
		port = sp->s_port;
	endservent ();
	sock = socket (AF_INET,SOCK_STREAM,0);
	if (sock < 0)
		return (scmerr (errno, stderr, "Can't create socket for connections"));
	if (setsockopt (sock,SOL_SOCKET,SO_REUSEADDR,(char *)&one,sizeof(int)) < 0)
		(void) scmerr (errno, stderr, "Can't set SO_REUSEADDR socket option");
	(void) bzero ((char *)&sin,sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = port;
	if (bind (sock,(struct sockaddr *)&sin,sizeof(sin)) < 0)
		return (scmerr (errno, stderr, "Can't bind socket for connections"));
	if (listen (sock,NCONNECTS) < 0)
		return (scmerr (errno, stderr, "Can't listen on socket"));
	return (SCMOK);
}

service ()
{
	struct sockaddr_in from;
	int x,len;

	remotename = NULL;
	len = sizeof (from);
	do {
		netfile = accept (sock,(struct sockaddr *)&from,&len);
	} while (netfile < 0 && errno == EINTR);
	if (netfile < 0)
		return (scmerr (errno, stderr, "Can't accept connections"));
	remoteaddr = from.sin_addr;
	if (read(netfile,(char *)&x,sizeof(int)) != sizeof(int))
		return (scmerr (errno, stderr, "Can't transmit data on connection"));
	if (x == 0x01020304)
		swapmode = 0;
	else if (x == 0x04030201)
		swapmode = 1;
	else
		return (scmerr (-1, stderr, "Unexpected byteswap mode %x",x));
	return (SCMOK);
}

serviceprep ()		/* kill temp socket in daemon */
{
	if (sock >= 0) {
		(void) close (sock);
		sock = -1;
	}
	return (SCMOK);
}

servicekill ()		/* kill net file in daemon's parent */
{
	if (netfile >= 0) {
		(void) close (netfile);
		netfile = -1;
	}
	if (remotename) {
		free (remotename);
		remotename = NULL;
	}
	return (SCMOK);
}

serviceend ()		/* kill net file after use in daemon */
{
	if (netfile >= 0) {
		(void) close (netfile);
		netfile = -1;
	}
	if (remotename) {
		free (remotename);
		remotename = NULL;
	}
	return (SCMOK);
}

/***************************************************
 ***    C O N N E C T I O N   R O U T I N E S    ***
 ***    F O R   C L I E N T                      ***
 ***************************************************/

dobackoff (t,b)
int *t,*b;
{
	struct timeval tt;
	unsigned s;

	if (*t == 0)
		return (0);
	s = *b * 30;
	if (gettimeofday (&tt,(struct timezone *)NULL) >= 0)
		s += (tt.tv_usec >> 8) % s;
	if (*b < 32) *b <<= 1;
	if (*t != -1) {
		if (s > *t)
			s = *t;
		*t -= s;
	}
	(void) scmerr (-1, stdout, "Will retry in %d seconds",s);
	sleep (s);
	return (1);
}

request (server,hostname,retry)		/* connect to server */
char *server;
char *hostname;
int *retry;
{
	int x, backoff;
	struct hostent *h;
	struct servent *sp;
	struct sockaddr_in sin, tin;
	short port;

	if ((sp = getservbyname(server,"tcp")) == 0) {
		if (strcmp(server, FILEPORT) == 0)
			port = htons((u_short)FILEPORTNUM);
		else if (strcmp(server, DEBUGFPORT) == 0)
			port = htons((u_short)DEBUGFPORTNUM);
		else
			return (scmerr (-1, stderr, "Can't find %s server description",
					server));
		(void) scmerr (-1, stderr, "%s/tcp: unknown service: using port %d",
					server,port);
	} else
		port = sp->s_port;
	(void) bzero ((char *)&sin,sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = inet_addr (hostname);
	if (sin.sin_addr.s_addr == (u_long) INADDR_NONE) {
		if ((h = gethostbyname (hostname)) == NULL)
			return (scmerr (-1, stderr, "Can't find host entry for %s",
					hostname));
		hostname = h->h_name;
		(void) bcopy (h->h_addr,(char *)&sin.sin_addr,h->h_length);
	}
	sin.sin_port = port;
	backoff = 1;
	for (;;) {
		netfile = socket (AF_INET,SOCK_STREAM,0);
		if (netfile < 0)
			return (scmerr (errno, stderr, "Can't create socket"));
		tin = sin;
		if (connect(netfile,(struct sockaddr *)&tin,sizeof(tin)) >= 0)
			break;
		(void) scmerr (errno, stderr, "Can't connect to server for %s",server);
		(void) close(netfile);
		if (!dobackoff (retry,&backoff))
			return (SCMERR);
	}
	remoteaddr = sin.sin_addr;
	remotename = salloc(hostname);
	x = 0x01020304;
	(void) write (netfile,(char *)&x,sizeof(int));
	swapmode = 0;		/* swap only on server, not client */
	return (SCMOK);
}

requestend ()			/* end connection to server */
{
	(void) readflush ();
	if (netfile >= 0) {
		(void) close (netfile);
		netfile = -1;
	}
	if (remotename) {
		free (remotename);
		remotename = NULL;
	}
	return (SCMOK);
}

/*************************************************
 ***    H O S T   N A M E   C H E C K I N G    ***
 *************************************************/

static
char *myhost ()		/* find my host name */
{
	struct hostent *h;
	static char name[MAXHOSTNAMELEN];


	if (name[0] == '\0') {
		if (gethostname (name,MAXHOSTNAMELEN) < 0)
			return (NULL);
		if ((h = gethostbyname (name)) == NULL)
			return (NULL);
		(void) strcpy (name,h->h_name);
	}
	return (name);
}

char *remotehost ()	/* remote host name (if known) */
{
	register struct hostent *h;

	if (remotename == NULL) {
		h = gethostbyaddr ((char *)&remoteaddr,sizeof(remoteaddr),
				    AF_INET);
		remotename = salloc (h ? h->h_name : inet_ntoa(remoteaddr));
		if (remotename == NULL)
			return("UNKNOWN");
	}
	return (remotename);
}

int thishost (host)
register char *host;
{
	register struct hostent *h;
	char *name;

	if ((name = myhost ()) == NULL)
		logquit (1,"Can't find my host entry");
	h = gethostbyname (host);
	if (h == NULL) return (0);
	return (strcasecmp (name,h->h_name) == 0);
}

int samehost ()		/* is remote host same as local host? */
{
	static struct in_addr *intp;
	static int nint = 0;
	struct in_addr *ifp;
	int n;

	if (nint <= 0) {
		int s;
		char buf[BUFSIZ];
		struct ifconf ifc;
		struct ifreq *ifr;
		struct sockaddr_in sin;

		if ((s = socket (AF_INET,SOCK_DGRAM,0)) < 0)
			logquit (1,"Can't create socket for SIOCGIFCONF");
		ifc.ifc_len = sizeof(buf);
		ifc.ifc_buf = buf;
		if (ioctl (s,SIOCGIFCONF,(char *)&ifc) < 0)
			logquit (1,"SIOCGIFCONF failed");
		(void) close(s);
		if ((nint = ifc.ifc_len/sizeof(struct ifreq)) <= 0)
			return (0);
		intp = (struct in_addr *)
			malloc ((unsigned) nint*sizeof(struct in_addr));
		if ((ifp = intp) == 0)
			logquit (1,"no space for interfaces");
		for (ifr = ifc.ifc_req, n = nint; n > 0; --n, ifr++) {
			(void) bcopy ((char *)&ifr->ifr_addr,(char *)&sin,sizeof(sin));
			*ifp++ = sin.sin_addr;
		}
	}
	if (remoteaddr.s_addr == htonl(INADDR_LOOPBACK))
		return (1);
	for (ifp = intp, n = nint; n > 0; --n, ifp++)
		if (remoteaddr.s_addr == ifp->s_addr)
			return (1);
	return (0);
}

int matchhost (name)	/* is this name of remote host? */
char *name;
{
	struct hostent *h;
	struct in_addr addr;
	char **ap;
	if(!strcmp(name,"*"))
		return (1);
	if ((addr.s_addr = inet_addr(name)) != (u_long) INADDR_NONE)
		return (addr.s_addr == remoteaddr.s_addr);
	if ((h = gethostbyname (name)) == 0)
		return (0);
	if (h->h_addrtype != AF_INET || h->h_length != sizeof(struct in_addr))
		return (0);
	for (ap = h->h_addr_list; *ap; ap++)
		if (bcmp ((char *)&remoteaddr,*ap,h->h_length) == 0)
			return (1);
	return (0);
}

#if __STDC__
int scmerr (int errno,FILE *filedes,char *fmt,...)
#else
/*VARARGS*//*ARGSUSED*/
int scmerr (va_alist)
va_dcl
#endif
{
#if !__STDC__
	int errno;
	FILE *filedes;
	char *fmt;
#endif
	va_list ap;

	(void) fflush (filedes);
	if (progpid > 0)
		fprintf (filedes,"%s %d: ",program,progpid);
	else
		fprintf (filedes,"%s: ",program);
#if __STDC__
	va_start(ap,fmt);
#else
	va_start(ap);
	errno = va_arg(ap,int);
	fmt = va_arg(ap,char *);
#endif
	vfprintf(filedes, fmt, ap);
	va_end(ap);
	if (errno >= 0)
		fprintf (filedes,": %s\n",errmsg(errno));
	else
		fprintf (filedes,"\n");
	(void) fflush (filedes);
	return (SCMERR);
}

/*******************************************************
 ***    I N T E G E R   B Y T E - S W A P P I N G    ***
 *******************************************************/

union intchar {
	int ui;
	char uc[sizeof(int)];
};

int byteswap (in)
int in;
{
	union intchar x,y;
	register int ix,iy;

	if (swapmode == 0)  return (in);
	x.ui = in;
	iy = sizeof(int);
	for (ix=0; ix<sizeof(int); ix++) {
		--iy;
		y.uc[iy] = x.uc[ix];
	}
	return (y.ui);
}
