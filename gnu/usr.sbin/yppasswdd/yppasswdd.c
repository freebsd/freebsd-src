/*
 * yppasswdd
 * Copyright 1994 Olaf Kirch, <okir@monad.swb.de>
 *
 * This program is covered by the GNU General Public License, version 2.
 * It is provided in the hope that it is useful. However, the author
 * disclaims ALL WARRANTIES, expressed or implied. See the GPL for details.
 */

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>

#include <stdlib.h>
#include <syslog.h>
#include <stdio.h>
#include <string.h>
#include <pwd.h>

#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include "yppasswd.h"

extern char *optarg;
extern void pw_init __P((void));
static char *program_name = "";
static char *version = "yppasswdd " VERSION;
char *passfile = _PATH_MASTERPASSWD;
int allow_chfn = 0, allow_chsh = 0;
char *domain;

#define xprt_addr(xprt)	(svc_getcaller(xprt)->sin_addr)
#define xprt_port(xprt)	ntohs(svc_getcaller(xprt)->sin_port)
void yppasswdprog_1( struct svc_req *rqstp, SVCXPRT *transp );
void reaper( int sig );

/*==============================================================*
 * RPC dispatch function
 *==============================================================*/
void
yppasswdprog_1(struct svc_req *rqstp, SVCXPRT *transp)
{
	union {
		yppasswd yppasswdproc_update_1_arg;
	} argument;
	char        *result;
	xdrproc_t   xdr_argument, xdr_result;
	char *(*local)();

	switch (rqstp->rq_proc) {
	case NULLPROC:
		(void)svc_sendreply(transp, (xdrproc_t)xdr_void, (char *)NULL);
		return;

	case YPPASSWDPROC_UPDATE:
		xdr_argument = (xdrproc_t) xdr_yppasswd;
		xdr_result   = (xdrproc_t) xdr_int;
		local = (char *(*)()) yppasswdproc_pwupdate_1;
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
	result = (*local)(&argument, rqstp);
	if (result != NULL
	 && !svc_sendreply(transp, (xdrproc_t)xdr_result, result)) {
		svcerr_systemerr(transp);
	}
	if (!svc_freeargs(transp, xdr_argument, &argument)) {
		(void)fprintf(stderr, "unable to free arguments\n");
		exit(1);
	}
}

static void
usage(FILE *fp, int n)
{
    fprintf (fp, "usage: %s [-m master password file] [-f] [-s] [-h] [-v]\n", program_name );
    exit(n);
}

void
reaper( int sig )
{
    extern pid_t pid;
    extern int pstat;

    pid = waitpid(pid, &pstat, 0);
}

void
install_reaper( int on )
{
    struct sigaction act, oact;

    if (on) {
	act.sa_handler = reaper;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_RESTART;
    } else {
	act.sa_handler = SIG_DFL;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_RESTART;
    }
    sigaction( SIGCHLD, &act, &oact );
}


int
main(int argc, char **argv)
{
    SVCXPRT *transp;
    char *sp;
    int opterr;
    int c;

    program_name = argv[0];
    if ((sp = strrchr(program_name, '/')) != NULL) {
    	program_name = ++sp;
    }

    /* Parse the command line options and arguments. */
    opterr = 0;
    while ((c = getopt(argc, argv, "m:fshv")) != EOF)
        switch (c) {
	case 'm':
	    passfile = strdup(optarg);
	    break;
	case 'f':
	    allow_chfn = 1;
	    break;
	case 's':
	    allow_chsh = 1;
	    break;
        case 'h':
            usage (stdout, 0);
            break;
        case 'v':
            printf("%s\n", version);
            exit(0);
        case 0:
            break;
        case '?':
        default:
            usage(stderr, 1);
        }

	if (daemon(0,0)) {
		perror("fork");
		exit(1);
	}

	if (yp_get_default_domain(&domain)) {
		fprintf(stderr, "%s: NIS domain name not set -- aborting\n",
					program_name);
		exit(1);
	}

	/*
	 * We can call this here since it does some necessary setup
	 * for us (blocking signals, setting resourse limits, etc.
	 */
	pw_init();

    /* Initialize logging.
     */
    openlog ( "yppasswdd", LOG_PID, LOG_AUTH );

    /* Register a signal handler to reap children after they terminated
     */
    install_reaper(1);

    /*
     * Create the RPC server
     */
    (void)pmap_unset(YPPASSWDPROG, YPPASSWDVERS);

    transp = svcudp_create(RPC_ANYSOCK);
    if (transp == NULL) {
        (void)fprintf(stderr, "cannot create udp service.\n");
        exit(1);
    }
    if (!svc_register(transp, YPPASSWDPROG, YPPASSWDVERS, yppasswdprog_1,
            IPPROTO_UDP)) {
        (void)fprintf(stderr, "unable to register yppaswdd udp service.\n");
        exit(1);
    }

    /*
     * Run the server
     */
    svc_run();
    (void)fprintf(stderr, "svc_run returned\n");

    return 1;
}

