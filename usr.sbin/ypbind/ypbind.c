/*
 * Copyright (c) 1992/3 Theo de Raadt <deraadt@fsa.ca>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef LINT
static char rcsid[] = "$Id: ypbind.c,v 1.2 1994/01/11 19:01:23 nate Exp $";
#endif

#include <sys/param.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <sys/uio.h>
#include <sys/syslog.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <dirent.h>
#include <netdb.h>
#include <string.h>
#include <rpc/rpc.h>
#include <rpc/xdr.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <rpc/pmap_clnt.h>
#include <rpc/pmap_prot.h>
#include <rpc/pmap_rmt.h>
#include <unistd.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>

#ifndef BINDINGDIR
#define BINDINGDIR "/var/yp/binding"
#endif

struct _dom_binding {
	struct _dom_binding *dom_pnext;
	char dom_domain[YPMAXDOMAIN + 1];
	struct sockaddr_in dom_server_addr;
	unsigned short int dom_server_port;
	int dom_socket;
	CLIENT *dom_client;
	long int dom_vers;
	time_t dom_check_t;
	int dom_lockfd;
	int dom_alive;
};

extern bool_t xdr_domainname(), xdr_ypbind_resp();
extern bool_t xdr_ypreq_key(), xdr_ypresp_val();
extern bool_t xdr_ypbind_setdom();

char *domainname;

struct _dom_binding *ypbindlist;
int check;

#define YPSET_NO	0
#define YPSET_LOCAL	1
#define YPSET_ALL	2
int ypsetmode = YPSET_NO;

int rpcsock;
struct rmtcallargs rmtca;
struct rmtcallres rmtcr;
char rmtcr_outval;
u_long rmtcr_port;
SVCXPRT *udptransp, *tcptransp;

void *
ypbindproc_null_2(transp, argp, clnt)
SVCXPRT *transp;
void *argp;
CLIENT *clnt;
{
	static char res;

	bzero((char *)&res, sizeof(res));
	return (void *)&res;
}

struct ypbind_resp *
ypbindproc_domain_2(transp, argp, clnt)
SVCXPRT *transp;
char *argp;
CLIENT *clnt;
{
	static struct ypbind_resp res;
	struct _dom_binding *ypdb;
	char path[MAXPATHLEN];

	bzero((char *)&res, sizeof res);
	res.ypbind_status = YPBIND_FAIL_VAL;

	for(ypdb=ypbindlist; ypdb; ypdb=ypdb->dom_pnext)
		if( strcmp(ypdb->dom_domain, argp) == 0)
			break;

	if(ypdb==NULL) {
		ypdb = (struct _dom_binding *)malloc(sizeof *ypdb);
		bzero((char *)ypdb, sizeof *ypdb);
		strncpy(ypdb->dom_domain, argp, sizeof ypdb->dom_domain);
		ypdb->dom_vers = YPVERS;
		ypdb->dom_alive = 0;
		ypdb->dom_lockfd = -1;
		sprintf(path, "%s/%s.%d", BINDINGDIR, ypdb->dom_domain, ypdb->dom_vers);
		unlink(path);
		ypdb->dom_pnext = ypbindlist;
		ypbindlist = ypdb;
		check++;
		return NULL;
	}

	if(ypdb->dom_alive==0)
		return NULL;

#if 0
	delta = ypdb->dom_check_t - ypdb->dom_ask_t;
	if( !(ypdb->dom_ask_t==0 || delta > 5)) {
		ypdb->dom_ask_t = time(NULL);
		/*
		 * Hmm. More than 2 requests in 5 seconds have indicated that my
		 * binding is possibly incorrect. Ok, make myself unalive, and
		 * find out what the actual state is.
		 */
		if(ypdb->dom_lockfd!=-1)
			close(ypdb->dom_lockfd);
		ypdb->dom_lockfd = -1;
		ypdb->dom_alive = 0;
		ypdb->dom_lockfd = -1;
		sprintf(path, "%s/%s.%d", BINDINGDIR, ypdb->dom_domain, ypdb->dom_vers);
		unlink(path);
		check++;
		return NULL;
	}
#endif

answer:
	res.ypbind_status = YPBIND_SUCC_VAL;
	res.ypbind_respbody.ypbind_bindinfo.ypbind_binding_addr.s_addr =
		ypdb->dom_server_addr.sin_addr.s_addr;
	res.ypbind_respbody.ypbind_bindinfo.ypbind_binding_port =
		ypdb->dom_server_port;
	/*printf("domain %s at %s/%d\n", ypdb->dom_domain,
		inet_ntoa(ypdb->dom_server_addr.sin_addr),
		ntohs(ypdb->dom_server_addr.sin_port));*/
	return &res;
}

bool_t *
ypbindproc_setdom_2(transp, argp, clnt)
SVCXPRT *transp;
struct ypbind_setdom *argp;
CLIENT *clnt;
{
	struct sockaddr_in *fromsin, bindsin;
	char res;

	bzero((char *)&res, sizeof(res));
	fromsin = svc_getcaller(transp);

	switch(ypsetmode) {
	case YPSET_LOCAL:
		if( fromsin->sin_addr.s_addr != htonl(INADDR_LOOPBACK))
			return (void *)NULL;
		break;
	case YPSET_ALL:
		break;
	case YPSET_NO:
	default:
		return (void *)NULL;
	}

	if(ntohs(fromsin->sin_port) >= IPPORT_RESERVED)
		return (void *)&res;

	if(argp->ypsetdom_vers != YPVERS)
		return (void *)&res;

	bzero((char *)&bindsin, sizeof bindsin);
	bindsin.sin_family = AF_INET;
	bindsin.sin_addr.s_addr = argp->ypsetdom_addr.s_addr;
	bindsin.sin_port = argp->ypsetdom_port;
	rpc_received(argp->ypsetdom_domain, &bindsin, 1);

	res = 1;
	return (void *)&res;
}

static void
ypbindprog_2(rqstp, transp)
struct svc_req *rqstp;
register SVCXPRT *transp;
{
	union {
		char ypbindproc_domain_2_arg[MAXHOSTNAMELEN];
		struct ypbind_setdom ypbindproc_setdom_2_arg;
	} argument;
	struct authunix_parms *creds;
	char *result;
	bool_t (*xdr_argument)(), (*xdr_result)();
	char *(*local)();

	switch (rqstp->rq_proc) {
	case YPBINDPROC_NULL:
		xdr_argument = xdr_void;
		xdr_result = xdr_void;
		local = (char *(*)()) ypbindproc_null_2;
		break;

	case YPBINDPROC_DOMAIN:
		xdr_argument = xdr_domainname;
		xdr_result = xdr_ypbind_resp;
		local = (char *(*)()) ypbindproc_domain_2;
		break;

	case YPBINDPROC_SETDOM:
		switch(rqstp->rq_cred.oa_flavor) {
		case AUTH_UNIX:
			creds = (struct authunix_parms *)rqstp->rq_clntcred;
			if( creds->aup_uid != 0) {
				svcerr_auth(transp, AUTH_BADCRED);
				return;
			}
			break;
		default:
			svcerr_auth(transp, AUTH_TOOWEAK);
			return;
		}

		xdr_argument = xdr_ypbind_setdom;
		xdr_result = xdr_void;
		local = (char *(*)()) ypbindproc_setdom_2;
		break;

	default:
		svcerr_noproc(transp);
		return;
	}
	bzero((char *)&argument, sizeof(argument));
	if (!svc_getargs(transp, xdr_argument, &argument)) {
		svcerr_decode(transp);
		return;
	}
	result = (*local)(transp, &argument, rqstp);
	if (result != NULL && !svc_sendreply(transp, xdr_result, result)) {
		svcerr_systemerr(transp);
	}
	return;
}

main(argc, argv)
char **argv;
{
	char path[MAXPATHLEN];
	struct timeval tv;
	fd_set fdsr;
	int width;
	int i;

	yp_get_default_domain(&domainname);
	if( domainname[0] == '\0') {
		fprintf(stderr, "domainname not set. Aborting.\n");
		exit(1);
	}

	for(i=1; i<argc; i++) {
		if( strcmp("-ypset", argv[i]) == 0)
			ypsetmode = YPSET_ALL;
		else if (strcmp("-ypsetme", argv[i]) == 0)
			ypsetmode = YPSET_LOCAL;
	}

	/* blow away everything in BINDINGDIR */



#ifdef DAEMON
	switch(fork()) {
	case 0:
		break;
	case -1:
		perror("fork");
		exit(1);
	default:
		exit(0);
	}
	setsid();
#endif

	pmap_unset(YPBINDPROG, YPBINDVERS);

	udptransp = svcudp_create(RPC_ANYSOCK);
	if (udptransp == NULL) {
		fprintf(stderr, "cannot create udp service.");
		exit(1);
	}
	if (!svc_register(udptransp, YPBINDPROG, YPBINDVERS, ypbindprog_2,
	    IPPROTO_UDP)) {
		fprintf(stderr, "unable to register (YPBINDPROG, YPBINDVERS, udp).");
		exit(1);
	}

	tcptransp = svctcp_create(RPC_ANYSOCK, 0, 0);
	if (tcptransp == NULL) {
		fprintf(stderr, "cannot create tcp service.");
		exit(1);
	}

	if (!svc_register(tcptransp, YPBINDPROG, YPBINDVERS, ypbindprog_2,
	    IPPROTO_TCP)) {
		fprintf(stderr, "unable to register (YPBINDPROG, YPBINDVERS, tcp).");
		exit(1);
	}

	if( (rpcsock=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		perror("socket");
		return -1;
	}
	
	fcntl(rpcsock, F_SETFL, fcntl(rpcsock, F_GETFL, 0) | FNDELAY);
	i = 1;
	setsockopt(rpcsock, SOL_SOCKET, SO_BROADCAST, &i, sizeof(i));
	rmtca.prog = YPPROG;
	rmtca.vers = YPVERS;
	rmtca.proc = YPPROC_DOMAIN_NONACK;
	rmtca.xdr_args = NULL;		/* set at call time */
	rmtca.args_ptr = NULL;		/* set at call time */
	rmtcr.port_ptr = &rmtcr_port;
	rmtcr.xdr_results = xdr_bool;
	rmtcr.results_ptr = (caddr_t)&rmtcr_outval;

	/* build initial domain binding, make it "unsuccessful" */
	ypbindlist = (struct _dom_binding *)malloc(sizeof *ypbindlist);
	bzero((char *)ypbindlist, sizeof *ypbindlist);
	strncpy(ypbindlist->dom_domain, domainname, sizeof ypbindlist->dom_domain);
	ypbindlist->dom_vers = YPVERS;
	ypbindlist->dom_alive = 0;
	ypbindlist->dom_lockfd = -1;
	sprintf(path, "%s/%s.%d", BINDINGDIR, ypbindlist->dom_domain,
		ypbindlist->dom_vers);
	(void)unlink(path);

	width = getdtablesize();
	while(1) {
		fdsr = svc_fdset;
		FD_SET(rpcsock, &fdsr);
		tv.tv_sec = 1;
		tv.tv_usec = 0;

		switch(select(width, &fdsr, NULL, NULL, &tv)) {
		case 0:
			checkwork();
			break;
		case -1:
			perror("select\n");
			break;
		default:
			if(FD_ISSET(rpcsock, &fdsr)) {
				FD_CLR(rpcsock, &fdsr);
				handle_replies();
			}
			svc_getreqset(&fdsr);
			if(check)
				checkwork();
			break;
		}
	}
}

/*
 * change to do something like this:
 *
 * STATE	TIME		ACTION		NEWTIME	NEWSTATE
 * no binding	t==*		broadcast 	t=2	no binding
 * binding	t==60		check server	t=10	binding
 * binding	t=10		broadcast	t=2	no binding
 */
checkwork()
{
	struct _dom_binding *ypdb;
	time_t t;

	check = 0;

	time(&t);
	for(ypdb=ypbindlist; ypdb; ypdb=ypdb->dom_pnext) {
		if(ypdb->dom_alive==0 || ypdb->dom_check_t < t) {
			broadcast(ypdb->dom_domain);
			time(&t);
			ypdb->dom_check_t = t + 5;
		}
	}
}

broadcast(dom)
char *dom;
{
	struct rpc_msg rpcmsg;
	char buf[1400], inbuf[8192];
	enum clnt_stat st;
	struct timeval tv;
	int outlen, i, sock, len;
	struct sockaddr_in bsin;
	struct ifconf ifc;
	struct ifreq ifreq, *ifr;
	struct in_addr in;
	AUTH *rpcua;
	XDR rpcxdr;

	rmtca.xdr_args = xdr_domainname;
	rmtca.args_ptr = dom;

	bzero((char *)&bsin, sizeof bsin);
	bsin.sin_family = AF_INET;
	bsin.sin_port = htons(PMAPPORT);

	bzero((char *)&rpcxdr, sizeof rpcxdr);
	bzero((char *)&rpcmsg, sizeof rpcmsg);

	rpcua = authunix_create_default();
	if( rpcua == (AUTH *)NULL) {
		/*printf("cannot get unix auth\n");*/
		return RPC_SYSTEMERROR;
	}
	rpcmsg.rm_direction = CALL;
	rpcmsg.rm_call.cb_rpcvers = RPC_MSG_VERSION;
	rpcmsg.rm_call.cb_prog = PMAPPROG;
	rpcmsg.rm_call.cb_vers = PMAPVERS;
	rpcmsg.rm_call.cb_proc = PMAPPROC_CALLIT;
	rpcmsg.rm_call.cb_cred = rpcua->ah_cred;
	rpcmsg.rm_call.cb_verf = rpcua->ah_verf;

	gettimeofday(&tv, (struct timezone *)0);
	rpcmsg.rm_xid = (int)dom;
	tv.tv_usec = 0;
	xdrmem_create(&rpcxdr, buf, sizeof buf, XDR_ENCODE);
	if( (!xdr_callmsg(&rpcxdr, &rpcmsg)) ) {
		st = RPC_CANTENCODEARGS;
		AUTH_DESTROY(rpcua);
		return st;
	}
	if( (!xdr_rmtcall_args(&rpcxdr, &rmtca)) ) {
		st = RPC_CANTENCODEARGS;
		AUTH_DESTROY(rpcua);
		return st;
	}
	outlen = (int)xdr_getpos(&rpcxdr);
	xdr_destroy(&rpcxdr);
	if(outlen<1) {
		AUTH_DESTROY(rpcua);
		return st;
	}
	AUTH_DESTROY(rpcua);

	/* find all networks and send the RPC packet out them all */
	if( (sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		perror("socket");
		return -1;
	}

	ifc.ifc_len = sizeof inbuf;
	ifc.ifc_buf = inbuf;
	if( ioctl(sock, SIOCGIFCONF, &ifc) < 0) {
		close(sock);
		perror("ioctl(SIOCGIFCONF)");
		return -1;
	}
	ifr = ifc.ifc_req;
	ifreq.ifr_name[0] = '\0';
	for(i=0; i<ifc.ifc_len; i+=len, ifr=(struct ifreq *)((caddr_t)ifr+len)) {
#if defined(BSD) && BSD >= 199103
		len = sizeof ifr->ifr_name + ifr->ifr_addr.sa_len;
#else
		len = sizeof ifc.ifc_len / sizeof(struct ifreq);
#endif
		ifreq = *ifr;
		if( ifreq.ifr_addr.sa_family != AF_INET)
			continue;
		if( ioctl(sock, SIOCGIFFLAGS, &ifreq) < 0) {
			perror("ioctl(SIOCGIFFLAGS)");
			continue;
		}
		if( (ifreq.ifr_flags & IFF_UP) == 0)
			continue;

		ifreq.ifr_flags &= (IFF_LOOPBACK | IFF_BROADCAST);
		if( ifreq.ifr_flags==IFF_BROADCAST ) {
			if( ioctl(sock, SIOCGIFBRDADDR, &ifreq) < 0 ) {
				perror("ioctl(SIOCGIFBRDADDR)");
				continue;
			}
		} else if( ifreq.ifr_flags==IFF_LOOPBACK ) {
			if( ioctl(sock, SIOCGIFADDR, &ifreq) < 0 ) {
				perror("ioctl(SIOCGIFADDR)");
				continue;
			}
		} else
			continue;

		in = ((struct sockaddr_in *)&ifreq.ifr_addr)->sin_addr;
		bsin.sin_addr = in;
		if( sendto(rpcsock, buf, outlen, 0, (struct sockaddr *)&bsin,
		   sizeof bsin) < 0 )
			perror("sendto");
	}
	close(sock);
	return 0;
}

/*enum clnt_stat*/
handle_replies()
{
	char buf[1400];
	int fromlen, inlen;
	struct sockaddr_in raddr;
	struct rpc_msg msg;
	XDR xdr;

recv_again:
	bzero((char *)&xdr, sizeof(xdr));
	bzero((char *)&msg, sizeof(msg));
	msg.acpted_rply.ar_verf = _null_auth;
	msg.acpted_rply.ar_results.where = (caddr_t)&rmtcr;
	msg.acpted_rply.ar_results.proc = xdr_rmtcallres;

try_again:
	fromlen = sizeof (struct sockaddr);
	inlen = recvfrom(rpcsock, buf, sizeof buf, 0,
		(struct sockaddr *)&raddr, &fromlen);
	if(inlen<0) {
		if(errno==EINTR)
			goto try_again;
		return RPC_CANTRECV;
	}
	if(inlen<sizeof(u_long))
		goto recv_again;

	/*
	 * see if reply transaction id matches sent id.
	 * If so, decode the results.
	 */
	xdrmem_create(&xdr, buf, (u_int)inlen, XDR_DECODE);
	if( xdr_replymsg(&xdr, &msg)) {
		if( (msg.rm_reply.rp_stat == MSG_ACCEPTED) &&
		   (msg.acpted_rply.ar_stat == SUCCESS)) {
			raddr.sin_port = htons((u_short)rmtcr_port);
			rpc_received(msg.rm_xid, &raddr, 0);
		}
	}
	xdr.x_op = XDR_FREE;
	msg.acpted_rply.ar_results.proc = xdr_void;
	xdr_destroy(&xdr);

	return RPC_SUCCESS;
}

/*
 * LOOPBACK IS MORE IMPORTANT: PUT IN HACK
 */
rpc_received(dom, raddrp, force)
char *dom;
struct sockaddr_in *raddrp;
int force;
{
	struct _dom_binding *ypdb;
	struct iovec iov[2];
	struct ypbind_resp ybr;
	char path[MAXPATHLEN];
	int fd;

	/*printf("returned from %s about %s\n", inet_ntoa(raddrp->sin_addr), dom);*/

	if(dom==NULL)
		return;

	for(ypdb=ypbindlist; ypdb; ypdb=ypdb->dom_pnext)
		if( strcmp(ypdb->dom_domain, dom) == 0)
			break;

	if(ypdb==NULL) {
		if(force==0)
			return;
		ypdb = (struct _dom_binding *)malloc(sizeof *ypdb);
		bzero((char *)ypdb, sizeof *ypdb);
		strncpy(ypdb->dom_domain, dom, sizeof ypdb->dom_domain);
		ypdb->dom_lockfd = -1;
		ypdb->dom_pnext = ypbindlist;
		ypbindlist = ypdb;
	}

	/* soft update, alive, less than 30 seconds old */
	if(ypdb->dom_alive==1 && force==0 && ypdb->dom_check_t<time(NULL)+30)
		return;

	bcopy((char *)raddrp, (char *)&ypdb->dom_server_addr,
		sizeof ypdb->dom_server_addr);
	ypdb->dom_check_t = time(NULL) + 60;	/* recheck binding in 60 seconds */
	ypdb->dom_vers = YPVERS;
	ypdb->dom_alive = 1;

	if(ypdb->dom_lockfd != -1)
		close(ypdb->dom_lockfd);

	sprintf(path, "%s/%s.%d", BINDINGDIR,
		ypdb->dom_domain, ypdb->dom_vers);
#ifdef O_SHLOCK
	if( (fd=open(path, O_CREAT|O_SHLOCK|O_RDWR|O_TRUNC, 0644)) == -1) {
		(void)mkdir(BINDINGDIR, 0755);
		if( (fd=open(path, O_CREAT|O_SHLOCK|O_RDWR|O_TRUNC, 0644)) == -1)
			return;
	}
#else
	if( (fd=open(path, O_CREAT|O_RDWR|O_TRUNC, 0644)) == -1) {
		(void)mkdir(BINDINGDIR, 0755);
		if( (fd=open(path, O_CREAT|O_RDWR|O_TRUNC, 0644)) == -1)
			return;
	}
	flock(fd, LOCK_SH);
#endif

	/*
	 * ok, if BINDINGDIR exists, and we can create the binding file,
	 * then write to it..
	 */
	ypdb->dom_lockfd = fd;

	iov[0].iov_base = (caddr_t)&(udptransp->xp_port);
	iov[0].iov_len = sizeof udptransp->xp_port;
	iov[1].iov_base = (caddr_t)&ybr;
	iov[1].iov_len = sizeof ybr;

	bzero(&ybr, sizeof ybr);
	ybr.ypbind_status = YPBIND_SUCC_VAL;
	ybr.ypbind_respbody.ypbind_bindinfo.ypbind_binding_addr = raddrp->sin_addr;
	ybr.ypbind_respbody.ypbind_bindinfo.ypbind_binding_port = raddrp->sin_port;

	if( writev(ypdb->dom_lockfd, iov, 2) != iov[0].iov_len + iov[1].iov_len) {
		perror("write");
		close(ypdb->dom_lockfd);
		ypdb->dom_lockfd = -1;
		return;
	}
}
