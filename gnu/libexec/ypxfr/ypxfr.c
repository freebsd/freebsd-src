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

	$Id: ypxfr.c,v 1.2 1995/02/06 23:35:49 wpaul Exp $
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <paths.h>
#include <rpc/rpc.h>
#include <sys/types.h>
#include <sys/param.h>
#include <db.h>
#include <limits.h>
#include <sys/stat.h>

DB *db;

#ifndef _PATH_YP
#define _PATH_YP "/var/yp/"
#endif

#define PERM_SECURE (S_IRUSR|S_IWUSR)
HASHINFO openinfo = {
	4096,		/* bsize */
	32,		/* ffactor */
	256,		/* nelem */
	2048 * 1024,	/* cachesize */
	NULL,		/* hash */
	0		/* lorder */
};

#include <rpcsvc/yp.h>

struct dom_binding {
        struct dom_binding *dom_pnext;
        char dom_domain[YPMAXDOMAIN + 1];
        struct sockaddr_in dom_server_addr;
        u_short dom_server_port;
        int dom_socket;
        CLIENT *dom_client;
        u_short dom_local_port;
        long dom_vers;
};  

#define DATUM /* Otherwise ypclnt.h redefines datum */
#include <rpcsvc/ypclnt.h>
/*
 * These are hooks to the ypclnt library in ../lib
 */
extern int _yp_bind(struct sockaddr_in *, char *);
extern int _yp_clear( char *);
extern void Perror  __P((const char *, ...));
int logflag = 0;
char *progname;
#include <errno.h>
#include <syslog.h>
#include <netdb.h>
#include <fcntl.h>
#include <ctype.h>
#include <arpa/inet.h>

extern int optind;
extern char *optarg;

static char *SourceHost=NULL, *TargetDomain=NULL, *SourceDomain=NULL;
static struct in_addr IpAddress;
static int Force=0, NoClear=0, TaskId=0, ProgramNumber=0,
	PortNumber=0;

static char *
ypxfr_err_string(enum ypxfrstat y) {
	switch(y) {
	case YPXFR_SUCC:	return "Success";
	case YPXFR_AGE:		return "Master's version not newer";
	case YPXFR_NOMAP:	return "Can't find server for map";
	case YPXFR_NODOM:	return "Domain not supported";
	case YPXFR_RSRC:	return "Local resource alloc failure";
	case YPXFR_RPC:		return "RPC failure talking to server";
	case YPXFR_MADDR:	return "Can't get master address";
	case YPXFR_YPERR:	return "YP server/map db error";
	case YPXFR_BADARGS:	return "Request arguments bad";
	case YPXFR_DBM:		return "Local dbm operation failed";
	case YPXFR_FILE:	return "Local file I/O operation failed";
	case YPXFR_SKEW:	return "Map version skew during transfer";
	case YPXFR_CLEAR:	return "Can't send \"Clear\" req to local ypserv";
	case YPXFR_FORCE:	return "No local order number in map  use -f flag.";
	case YPXFR_XFRERR:	return "ypxfr error";
	case YPXFR_REFUSED:	return "Transfer request refused by ypserv";
	}
}

ypxfr_foreach(int status, char *key, int keylen, char *val, int vallen,
	int *data)
{

	DBT outKey, outData;

	if (status==YP_NOMORE)
		return 0;
	if (status!=YP_TRUE) {
		int s=ypprot_err(status);
		Perror("%s\n",yperr_string(s));
		return 1;
	}

	outKey.data=key; outKey.size=(size_t)keylen;
	outData.data=val; outData.size=(size_t)vallen;
	(db->put)(db,&outKey,&outData,0);

	return 0;
}

static enum ypxfrstat
ypxfr(char *mapName) {
	
	int localOrderNum=0;
	int masterOrderNum=0;
	char *masterName;
	struct sockaddr_in localHost;
	struct sockaddr_in masterHost;
	struct ypall_callback callback;
	char dbName[1024];
	char dbName2[1024];
	int y, masterSock;
	CLIENT *masterClient;

	memset(&localHost, '\0', sizeof localHost);
	localHost.sin_family=AF_INET;
	localHost.sin_addr.s_addr=htonl(INADDR_LOOPBACK);


	if (!SourceHost) {
		if ((y=yp_master(SourceDomain, mapName, &masterName)))
			return YPXFR_MADDR;
		SourceHost=masterName;
	}

	memset(&masterHost, '\0', sizeof masterHost);
	masterHost.sin_family=AF_INET;
	{
		struct hostent *h=gethostbyname(SourceHost);
		if (!h) {
			return YPXFR_MADDR;
		}
		memcpy(&masterHost.sin_addr, h->h_addr,
			sizeof masterHost.sin_addr);
	}

        if ((y=_yp_bind(&masterHost, SourceDomain))) return YPXFR_RPC;

	masterSock=RPC_ANYSOCK;
	masterClient=clnttcp_create(&masterHost, YPPROG, YPVERS, &masterSock, 0, 0);
	if (masterClient==NULL) {
		clnt_pcreateerror("");
		return YPXFR_RPC;
	}
	{
		static struct timeval tv = { 25, 0 };
		struct ypreq_nokey req;
		struct ypresp_order resp;
		int y;

		req.domain=SourceDomain;
		req.map=mapName;
		y=clnt_call(masterClient, YPPROC_ORDER, xdr_ypreq_nokey,
			&req, xdr_ypresp_order, &resp, tv);
		if (y!=RPC_SUCCESS) {
			clnt_perror(masterClient, "masterOrderNum");
			masterOrderNum=0x7fffffff;
		} else {
			masterOrderNum=resp.ordernum;
		}
		xdr_free(xdr_ypresp_order, (char *)&resp);
	}

	if (!Force) {
		DBT inKey, inVal;
		sprintf(dbName, "%s%s/%s", _PATH_YP, TargetDomain, mapName);
		if ((db = dbopen(dbName,O_RDWR|O_EXCL, PERM_SECURE,
				DB_HASH, &openinfo)) == NULL) {
			Perror("dbopen: %s\n", strerror(errno));
			Perror("%s: cannot open - ignored.\n", dbName);
			localOrderNum=0;
		} else {
			inKey.data="YP_LAST_MODIFIED"; inKey.size=strlen(inKey.data);
			(db->get)(db,&inKey,&inVal,0);
			if (inVal.data) {
				int i;
				char *d=inVal.data;
				for (i=0; i<inVal.size; i++, d++) {
					if (!isdigit(*d)) {
						(void)(db->close)(db);
						return YPXFR_SKEW;
					}
				}
				localOrderNum=atoi(inVal.data);
			}
			(void)(db->close)(db);
		}
		if (localOrderNum>=masterOrderNum) return YPXFR_AGE;
	}

	sprintf(dbName, "%s%s/%s~", _PATH_YP, TargetDomain, mapName);
	if ((db = dbopen(dbName,O_RDWR|O_EXCL|O_CREAT, PERM_SECURE, DB_HASH,
			&openinfo)) == NULL) {
		Perror("%s: Cannot open\n", dbName);
		return YPXFR_DBM;
	}

	{
		DBT outKey, outData;
		char orderNum[12];
		outKey.data="YP_MASTER_NAME"; outKey.size=strlen(outKey.data);
		outData.data=SourceHost; outData.size=strlen(outData.data);
		(db->put)(db,&outKey,&outData,0);
		sprintf(orderNum, "%d", masterOrderNum);
		outKey.data="YP_LAST_MODIFIED"; outKey.size=strlen(outKey.data);
		outData.data=orderNum; outData.size=strlen(outData.data);
		(db->put)(db,&outKey,&outData,0);
	}


	callback.foreach = ypxfr_foreach;
	callback.data = NULL;

	/*
	 * We have to use our own private version of yp_all() here since
	 * the yp_all() in libc doesn't allow us to specify the server that
	 * we want to talk to: it's imperative that we transfer data from
	 * the NIS master server and no one else.
	 */
	y=__yp_all(SourceDomain, mapName, &callback);

	(void)(db->close)(db);
	sprintf(dbName, "%s%s/%s~", _PATH_YP, TargetDomain, mapName);
	sprintf(dbName2, "%s%s/%s", _PATH_YP, TargetDomain, mapName);
	unlink(dbName2);
	rename(dbName, dbName2);

	if (!NoClear) {
		memset(&localHost, '\0', sizeof localHost);
		localHost.sin_family=AF_INET;
		localHost.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
		if (_yp_bind(&localHost, TargetDomain) ||
			_yp_clear(TargetDomain)) return YPXFR_CLEAR;
	} 
	return y==0?YPXFR_SUCC:YPXFR_YPERR;
}

void usage(progname)
char *progname;
{
	fprintf(stderr,"usage: %s [-f] [-c] [-d target domain] \
[-h source host]\n	[-s source domain] \
[-C taskid program-number ipaddr port] mapname\n", progname);
}

void
main (int argc, char **argv)
{

	progname = argv[0];

	if (!isatty(2)) {
		openlog(argv[0], LOG_PID, LOG_DAEMON);
		logflag = 1;
	}

	if (argc < 2)
	{
		usage(argv[0]);
		exit(1);
	}

	while(1) {
		int c=getopt(argc, argv, "fcd:h:s:C:S");
		if (c==EOF) break;
		switch (c) {
		case 'f':
			Force++;
			break;
		case 'c':
			NoClear++;
			break;
		case 'd':
			TargetDomain=optarg;
			break;
		case 'h':
			SourceHost=optarg;
			break;
		case 's':
			SourceDomain=optarg;
			break;
		case 'C':
			TaskId=atoi(optarg);
			ProgramNumber=atoi(argv[optind++]);
			IpAddress.s_addr=inet_addr(argv[optind++]);
			PortNumber=atoi(argv[optind++]);
			break;
		default:
			usage(argv[0]);
			exit(1);
		}
	}
	argc-=optind;
	argv+=optind;

	if (!TargetDomain) {
		yp_get_default_domain(&TargetDomain);
	}
	if (!SourceDomain) {
		SourceDomain=TargetDomain;
	}

	for (; *argv; argv++) {
		enum ypxfrstat y;
		if ((y=ypxfr(*argv))!=YPXFR_SUCC) {
			Perror("%s\n", ypxfr_err_string(y));
		}
		if (TaskId) {
			struct sockaddr_in addr;
			struct timeval wait;
			CLIENT *clnt;
			int s;
			ypresp_xfr resp;
			static struct timeval tv={0,0};

			memset(&addr, '\0', sizeof addr);
			addr.sin_addr=IpAddress;
			addr.sin_port=htons(PortNumber);
			addr.sin_family=AF_INET;
			wait.tv_sec=25; wait.tv_usec=0;
			s=RPC_ANYSOCK;

			clnt=clntudp_create(&addr, ProgramNumber, 1, wait, &s);
			if (!clnt) {
				clnt_pcreateerror("ypxfr_callback");
				continue;
			}

			resp.transid=TaskId;
			resp.xfrstat=y;
			switch (clnt_call(clnt, 1, xdr_ypresp_xfr, &resp,
				xdr_void, NULL, tv)) {
			case RPC_SUCCESS:
			case RPC_TIMEDOUT:
				break;
			default:
				clnt_perror(clnt, "ypxfr_callback");
			}

			clnt_destroy(clnt);
		}
	}

	exit(0);

}
