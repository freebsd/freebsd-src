/*
    YPS-0.2, NIS-Server for Linux
    Copyright (C) 1994  Tobias Reber

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    Modified for use with FreeBSD 2.x by Bill Paul (wpaul@ctr.columbia.edu)

	$Id: yppush.c,v 1.3 1995/04/02 01:10:13 wpaul Exp $
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <rpc/rpc.h>
#include <paths.h>
#include <rpcsvc/yp.h>
/*
 * ypclnt.h does not have a definition for struct dom_binding,
 * although it is used there. It is defined in yp_prot.h, but
 * we cannot use it here.
 */
struct dom_binding {
	void * m;
};
#include <rpcsvc/ypclnt.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <ctype.h>
#include <netdb.h>
#include <errno.h>
#include <sys/types.h>
#include <fcntl.h>
#include <db.h>
#include <limits.h>
#include <sys/stat.h>

#ifndef _PATH_YP
#define _PATH_YP "/var/yp/"
#endif

char *progname;

#define PERM_SECURE (S_IRUSR|S_IWUSR)
HASHINFO openinfo = {
	4096,		/* bsize */
	32,		/* ffactor */
	256,		/* nelem */
	2048 * 1024,	/* cachesize */
	NULL,		/* hash */
	0		/* lorder */
};

#undef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))

static char *DomainName=NULL;
static bool_t Verbose=FALSE;

static char *ThisHost=NULL;

static char *MapName;
static u_int MapOrderNum;

static CLIENT *PushClient=NULL;

static u_int CallbackTransid;
static u_int CallbackProg=0;
static SVCXPRT *CallbackXprt;

static inline char *
yppush_err_string(enum yppush_status y) {
	switch(y) {
	case YPPUSH_SUCC:	return "Success";
	case YPPUSH_AGE:	return "Master's version not newer";
	case YPPUSH_NOMAP:	return "Can't find server for map";
	case YPPUSH_NODOM:	return "Domain not supported";
	case YPPUSH_RSRC:	return "Local resource alloc failure";
	case YPPUSH_RPC:	return "RPC failure talking to server";
	case YPPUSH_MADDR:	return "Can't get master address";
	case YPPUSH_YPERR:	return "YP server/map db error";
	case YPPUSH_BADARGS:	return "Request arguments bad";
	case YPPUSH_DBM:	return "Local dbm operation failed";
	case YPPUSH_FILE:	return "Local file I/O operation failed";
	case YPPUSH_SKEW:	return "Map version skew during transfer";
	case YPPUSH_CLEAR:	return "Can't send \"Clear\" req to local ypserv";
	case YPPUSH_FORCE:	return "No local order number in map  use -f flag.";
	case YPPUSH_XFRERR:	return "ypxfr error";
	case YPPUSH_REFUSED:	return "Transfer request refused by ypserv";
	}
}

#define YPPUSH_XFRRESPPROG ((u_long)0x40000000)
#define YPPUSH_XFRRESPVERS ((u_long)1)

#define YPPUSHPROC_NULL ((u_long)0)
static inline void *
__yppushproc_null_1(void * req, struct svc_req * rqstp) {
	static int resp;
	return &resp;
}

#define YPPUSHPROC_XFRRESP ((u_long)1)
static inline void *
__yppushproc_xfrresp_1(yppushresp_xfr *req, struct svc_req * rqstp) {
	static int resp;
	
	if (req->status!=YPPUSH_SUCC)
		fprintf(stderr, "%s: %s\n", progname, yppush_err_string(req->status));
	return &resp;
}

void
__yppush_xfrrespprog_1( struct svc_req *rqstp, SVCXPRT *transp)
{
	union {
		int fill;
	} argument;
	char *result;
	bool_t (*xdr_argument)(XDR *, void *), (*xdr_result)(XDR *, void *);
	char *(*local)( void *, struct svc_req *);

	switch (rqstp->rq_proc) {
	case YPPUSHPROC_NULL:
		xdr_argument = (bool_t (*)(XDR *, void *))xdr_void;
		xdr_result = (bool_t (*)(XDR *, void *))xdr_void;
		local = (char *(*)( void *, struct svc_req *)) __yppushproc_null_1;
		break;

	case YPPUSHPROC_XFRRESP:
		xdr_argument = xdr_yppushresp_xfr;
		xdr_result = (bool_t (*)(XDR *, void *))xdr_void;
		local = (char *(*)( void *, struct svc_req *)) __yppushproc_xfrresp_1;
		break;

	default:
		svcerr_noproc(transp);
		exit(1);
	}
	bzero((char *)&argument, sizeof(argument));
	if (!svc_getargs(transp, xdr_argument, &argument)) {
		svcerr_decode(transp);
		exit(1);
	}
	result = (*local)(&argument, rqstp);
	if (result != NULL && !svc_sendreply(transp, xdr_result, result)) {
		svcerr_systemerr(transp);
	}
	if (!svc_freeargs(transp, xdr_argument, &argument)) {
		(void)fprintf(stderr, "%s: unable to free arguments\n", progname);
		exit(1);
	if (rqstp->rq_proc!=YPPUSHPROC_NULL)
		exit(0);
	}
	exit(0);
}

/*
 * This is the rpc server side idle loop
 * Wait for input, call server program.
 */

static void
_svc_run( void)
{
#ifdef FD_SETSIZE
	fd_set readfds;
#else
        int readfds;
#endif /* def FD_SETSIZE */
	extern int errno;
	struct timeval t;

	t.tv_sec=60; t.tv_usec=0;

	for (;;) {
#ifdef FD_SETSIZE
		readfds = svc_fdset;
#else
		readfds = svc_fds;
#endif /* def FD_SETSIZE */
		switch (select(_rpc_dtablesize(), &readfds, (void *)0,
			(void *)0, &t)) {
		case -1:
			if (errno == EINTR) {
				continue;
			}
			perror("svc_run: - select failed");
			return;
		case 0:
			fprintf(stderr, "%sh: callback timed out\n, progname");
			exit(0);
		default:
			svc_getreqset(&readfds);
		}
	}
}

static void
Usage(void)
{
	fprintf(stderr, "Usage: %s [ -d domain ] [ -v ] mapname ...\n",
								progname);
	exit(1);
}

static char *
getHostName( void)
{
	static char hostname[MAXHOSTNAMELEN+1];
	struct hostent *h;
	if (gethostname(hostname, sizeof hostname)!=0) {
		fprintf(stderr,"%s:", progname);
		perror("gethostname");
		return NULL;
	}
	h=gethostbyname(hostname);
	if (h!=NULL) {
		strncpy(hostname, h->h_name, sizeof (hostname)-1);
		hostname[sizeof (hostname)-1]='\0';
	}
	return hostname;
}

static u_int
getOrderNum( void)
{
	char mapPath[MAXPATHLEN];
	DB *db;
	DBT o,d;
	int i;

	strcpy(mapPath, _PATH_YP);
	strcat(mapPath, DomainName);
	strcat(mapPath, "/");
	strcat(mapPath, MapName);

	if ((db = dbopen(mapPath, O_RDWR|O_EXCL, PERM_SECURE, DB_HASH,
		&openinfo)) == NULL) {
		fprintf(stderr, "%s: %s: cannot open\n", progname, mapPath);
		return -1;
	}
	
	o.data="YP_LAST_MODIFIED"; o.size=strlen(o.data);
	(db->get)(db,&o,&d,0);
	if (d.data==NULL) {
		fprintf(stderr, "%s: %s: cannot determine order number\n",
			progname, MapName);
		return -1;
	}

	for (i=0; i<d.size; i++) {
		if (!isdigit(*((char *)d.data+i))) {
			fprintf(stderr, "%s: %s: Invalid order number '%s'\n",
				progname, MapName, d.data);
			return -1;
		}
	}
	(void)(db->close)(db);
	return atoi(d.data);
}

static void
doPushClient( const char *targetHost)
{
	struct ypreq_xfr req;
	static struct timeval tv={0,0};
	
	req.map_parms.domain=DomainName;
	req.map_parms.map=(char *)MapName;
	req.map_parms.peer=ThisHost;
	req.map_parms.ordernum=MapOrderNum;
	req.transid=CallbackTransid;
	req.prog=CallbackProg;
	req.port=CallbackXprt->xp_port;

	if (Verbose)
		printf("%d: %s(%d@%s) -> %s@%s\n", req.transid,
			req.map_parms.map, req.map_parms.ordernum,
			req.map_parms.peer, targetHost,
			req.map_parms.domain);
	switch (clnt_call(PushClient, YPPROC_XFR, xdr_ypreq_xfr, &req,
		xdr_void, NULL, tv)) {
	case RPC_SUCCESS:
	case RPC_TIMEDOUT:
		break;
	default:
		clnt_perror(PushClient, "yppush: cannot call YPPROC_XFR");
		kill(CallbackTransid, SIGTERM);
	}
	return;
}

static bool_t
registerServer(void)
{
	int s;
	s=RPC_ANYSOCK;
	CallbackXprt=svcudp_create(s);
	if (CallbackXprt==NULL) {
		fprintf(stderr, "%s: Cannot create callback transport.\n",
								progname);
		return FALSE;
	}
	for (CallbackProg=0x40000000; CallbackProg<0x5fffffff; CallbackProg++) {
		if (svc_register(CallbackXprt, CallbackProg, 1,
			__yppush_xfrrespprog_1, IPPROTO_UDP))
			return TRUE;
	}
	return FALSE;
}

static bool_t
createClient(const char *targetHost)
{
	PushClient=clnt_create((char *)targetHost, YPPROG, YPVERS, "tcp");
	if (PushClient==NULL) {
		clnt_pcreateerror("yppush: cannot create client");
		return FALSE;
	}
	return TRUE;
}

static void
doPush( const char *targetHost)
{
	int s;
	struct rusage r;

	if (!createClient(targetHost))
		return;
	if (!registerServer())
		return;

	switch (CallbackTransid=fork()) {
	case -1:
		fprintf(stderr, "%s:", progname);
		perror("fork");
		exit(1);
	case 0:
		_svc_run();
		exit(0);
	default:
		close(CallbackXprt->xp_sock);
		doPushClient(targetHost);
		wait4(CallbackTransid, &s, 0, &r);
		svc_unregister(CallbackProg, 1);
		CallbackProg=0;
		if (PushClient!=NULL) {
			clnt_destroy(PushClient);
			PushClient=NULL;
		}
	}
}

static int
yppushForeach(int status, const char *inKey, int inKeylen,
	const char *inVal, int inVallen, const char *data)
{
	char targetHost[YPMAXPEER+1];

	if (status!=YP_TRUE) return 0;
	memcpy(targetHost, inKey, MIN(sizeof (targetHost)-1, inKeylen));
	targetHost[MIN(sizeof (targetHost)-1, inKeylen)]='\0';
	doPush(targetHost);
	return 0;
}
	
static void
intrHandler(int sig)
{
	if (CallbackProg!=0)
		svc_unregister(CallbackProg, 1);
	exit(1);
}

int
main(int argc, char **argv)
{
	char c;
	struct ypall_callback f;
	enum ypstat y;
	struct sigaction a;

	progname = argv[0];

	a.sa_handler=intrHandler;
	a.sa_mask=0;
	sigaction(SIGINT, &a, NULL);

	while((c=getopt(argc, argv, "d:v"))!=EOF) {
		switch(c) {
		case 'd':
			DomainName=optarg;
			break;
		case 'v':
			Verbose=TRUE;
			break;
		default:
			Usage();
		}
	}
	argc-=optind;
	argv+=optind;
	if (argc<1) Usage();

	if (DomainName==NULL) {
		if (yp_get_default_domain(&DomainName)!=0) {
			fprintf(stderr, "%s: cannot get default domain\n,
								progname");
			exit(1);
		}
	}

	ThisHost=getHostName();
	if (ThisHost==NULL) {
		fprintf(stderr, "%s: cannot determine local hostname\n,
								progname");
		exit(1);
	}

	while (*argv) {
		MapName=*argv++;
		MapOrderNum=getOrderNum();
		if (MapOrderNum==0xffffffff)
			continue;
		f.foreach=yppushForeach;
		y=yp_all(DomainName, "ypservers", &f);
		if (y && y!=YP_NOMORE) {
			fprintf(stderr, "%s: could not read ypservers map: %d %s\n",
				progname, y, yperr_string(y));
			exit(1);
		}
	}
	exit(0);
}
