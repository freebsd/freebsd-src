/*
 * Copyright 1988 by the Massachusetts Institute of Technology.
 *
 * For copying and distribution information, please see the file
 * Copyright.MIT.
 *
 * Top-level loop of the kerberos Administration server
 */

#if 0
#ifndef	lint
static char rcsid_admin_server_c[] =
"Id: admin_server.c,v 4.8 90/01/02 13:50:38 jtkohl Exp ";
static const char rcsid[] =
	"$Id";
#endif	lint
#endif

/*
  admin_server.c
  this holds the main loop and initialization and cleanup code for the server
*/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#ifndef sigmask
#define sigmask(m)	(1 <<((m)-1))
#endif
#include <sys/wait.h>
#include <errno.h>
#include <sys/socket.h>
#include <syslog.h>
#include <com_err.h>
#include <kadm.h>
#include <kadm_err.h>
#include <krb_db.h>
#include "kadm_server.h"

/* Almost all procs and such need this, so it is global */
admin_params prm;		/* The command line parameters struct */

char prog[32];			/* WHY IS THIS NEEDED??????? */
char *progname = prog;
char *acldir = DEFAULT_ACL_DIR;
char krbrlm[REALM_SZ];
extern Kadm_Server server_parm;

void cleanexit(int val);
void process_client(int fd, struct sockaddr_in *who);
void kill_children(void);
static void clear_secrets(void);
void byebye(void);
void close_syslog(void);
int kadm_listen(void);

/*
** Main does the logical thing, it sets up the database and RPC interface,
**  as well as handling the creation and maintenance of the syslog file...
*/
void
main(argc, argv)		/* admin_server main routine */
int argc;
char *argv[];
{
    int errval;
    int c;
    extern char *optarg;

    prog[sizeof(prog)-1]='\0';		/* Terminate... */
    (void) strncpy(prog, argv[0], sizeof(prog)-1);

    /* initialize the admin_params structure */
    prm.sysfile = KADM_SYSLOG;		/* default file name */
    prm.inter = 1;

    bzero(krbrlm, sizeof(krbrlm));

    while ((c = getopt(argc, argv, "f:hnd:a:r:")) != EOF)
	switch(c) {
	case 'f':			/* Syslog file name change */
	    prm.sysfile = optarg;
	    break;
	case 'n':
	    prm.inter = 0;
	    break;
	case 'a':			/* new acl directory */
	    acldir = optarg;
	    break;
	case 'd':
	    /* put code to deal with alt database place */
	    if ((errval = kerb_db_set_name(optarg))) {
		fprintf(stderr, "opening database %s: %s",
			optarg, error_message(errval));
		exit(1);
	    }
	    break;
	case 'r':
	    (void) strncpy(krbrlm, optarg, sizeof(krbrlm) - 1);
	    break;
	case 'h':			/* get help on using admin_server */
	default:
	    printf("Usage: admin_server [-h] [-n] [-r realm] [-d dbname] [-f filename] [-a acldir]\n");
	    exit(-1);			/* failure */
	}

    if (krbrlm[0] == 0)
	if (krb_get_lrealm(krbrlm, 0) != KSUCCESS) {
	    fprintf(stderr,
		    "Unable to get local realm.  Fix krb.conf or use -r.\n");
	    exit(1);
	}

    printf("KADM Server %s initializing\n",KADM_VERSTR);
    printf("Please do not use 'kill -9' to kill this job, use a\n");
    printf("regular kill instead\n\n");

    set_logfile(prm.sysfile);
    log("Admin server starting");

    (void) kerb_db_set_lockmode(KERB_DBL_NONBLOCKING);
    errval = kerb_init();		/* Open the Kerberos database */
    if (errval) {
	fprintf(stderr, "error: kerb_init() failed");
	close_syslog();
	byebye();
    }
    /* set up the server_parm struct */
    if ((errval = kadm_ser_init(prm.inter, krbrlm))==KADM_SUCCESS) {
	kerb_fini();			/* Close the Kerberos database--
					   will re-open later */
	errval = kadm_listen();		/* listen for calls to server from
					   clients */
    }
    if (errval != KADM_SUCCESS) {
	fprintf(stderr,"error:  %s\n",error_message(errval));
	kerb_fini();			/* Close if error */
    }
    close_syslog();			/* Close syslog file, print
					   closing note */
    byebye();				/* Say bye bye on the terminal
					   in use */
}					/* procedure main */


/* close the system log file */
void
close_syslog()
{
   log("Shutting down admin server");
}

void
byebye()			/* say goodnight gracie */
{
   printf("Admin Server (kadm server) has completed operation.\n");
}

static void
clear_secrets()
{
    bzero((char *)server_parm.master_key, sizeof(server_parm.master_key));
    bzero((char *)server_parm.master_key_schedule,
	  sizeof(server_parm.master_key_schedule));
    server_parm.master_key_version = 0L;
}

static exit_now = 0;

sigtype
doexit()
{
    exit_now = 1;
#ifdef POSIX
    return;
#else /* !POSIX */
    return(0);
#endif /* POSIX */
}

unsigned pidarraysize = 0;
int *pidarray = (int *)0;

/*
kadm_listen
listen on the admin servers port for a request
*/
int
kadm_listen()
{
    extern int errno;
    int found;
    int admin_fd;
    int peer_fd;
    fd_set mask, readfds;
    struct sockaddr_in peer;
    int addrlen;
    int pid;
    sigtype do_child();

    (void) signal(SIGINT, doexit);
    (void) signal(SIGTERM, doexit);
    (void) signal(SIGHUP, doexit);
    (void) signal(SIGQUIT, doexit);
    (void) signal(SIGPIPE, SIG_IGN); /* get errors on write() */
    (void) signal(SIGALRM, doexit);
    (void) signal(SIGCHLD, do_child);

    if ((admin_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	return KADM_NO_SOCK;
    if (bind(admin_fd, (struct sockaddr *)&server_parm.admin_addr,
	     sizeof(struct sockaddr_in)) < 0)
	return KADM_NO_BIND;
    (void) listen(admin_fd, 1);
    FD_ZERO(&mask);
    FD_SET(admin_fd, &mask);

    for (;;) {				/* loop nearly forever */
	if (exit_now) {
	    clear_secrets();
	    kill_children();
	    return(0);
	}
	readfds = mask;
	if ((found = select(admin_fd+1,&readfds,(fd_set *)0,
			    (fd_set *)0, (struct timeval *)0)) == 0)
	    continue;			/* no things read */
	if (found < 0) {
	    if (errno != EINTR)
		log("select: %s",error_message(errno));
	    continue;
	}
	if (FD_ISSET(admin_fd, &readfds)) {
	    /* accept the conn */
	    addrlen = sizeof(peer);
	    if ((peer_fd = accept(admin_fd, (struct sockaddr *)&peer,
				  &addrlen)) < 0) {
		log("accept: %s",error_message(errno));
		continue;
	    }
	    addrlen = sizeof(server_parm.admin_addr);
	    if (getsockname(peer_fd, (struct sockaddr *)&server_parm.admin_addr,
			    &addrlen)) {
		log("getsockname: %s",error_message(errno));
		continue;
	    }
#ifdef DEBUG
	    printf("Connection recieved on %s\n",
		   inet_ntoa(server_parm.admin_addr.sin_addr));
#endif /* DEBUG */
#ifndef DEBUG
	    /* if you want a sep daemon for each server */
	    if ((pid = fork())) {
		/* parent */
		if (pid < 0) {
		    log("fork: %s",error_message(errno));
		    (void) close(peer_fd);
		    continue;
		}
		/* fork succeded: keep tabs on child */
		(void) close(peer_fd);
		if (pidarray) {
		    pidarray = (int *)realloc((char *)pidarray, ++pidarraysize);
		    pidarray[pidarraysize-1] = pid;
		} else {
		    pidarray = (int *)malloc(pidarraysize = 1);
		    pidarray[0] = pid;
		}
	    } else {
		/* child */
		(void) close(admin_fd);
#endif /* DEBUG */
		/* do stuff */
		process_client (peer_fd, &peer);
#ifndef DEBUG
	    }
#endif
	} else {
	    log("something else woke me up!");
	    return(0);
	}
    }
    /*NOTREACHED*/
    return(0); /* Shut -Wall up - markm */
}

#ifdef DEBUG
#define cleanexit(code) {kerb_fini(); return;}
#endif

void
process_client(fd, who)
int fd;
struct sockaddr_in *who;
{
    u_char *dat;
    int dat_len;
    u_short dlen;
    int retval;
    int on = 1;
    Principal service;
    des_cblock skey;
    int more;
    int status;

    if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on)) < 0)
	log("setsockopt keepalive: %d",errno);

    server_parm.recv_addr = *who;

    if (kerb_init()) {			/* Open as client */
	log("can't open krb db");
	cleanexit(1);
    }
    /* need to set service key to changepw.KRB_MASTER */

    status = kerb_get_principal(server_parm.sname, server_parm.sinst, &service,
			    1, &more);
    if (status == -1) {
      /* db locked */
      u_long retcode = KADM_DB_INUSE;
      char *pdat;

      dat_len = KADM_VERSIZE + sizeof(u_long);
      dat = (u_char *) malloc((unsigned)dat_len);
      pdat = (char *) dat;
      retcode = htonl((u_long) KADM_DB_INUSE);
      (void) strncpy(pdat, KADM_ULOSE, KADM_VERSIZE);
      bcopy((char *)&retcode, &pdat[KADM_VERSIZE], sizeof(u_long));
      goto out;
    } else if (!status) {
      log("no service %s.%s",server_parm.sname, server_parm.sinst);
      cleanexit(2);
    }

    bcopy((char *)&service.key_low, (char *)skey, 4);
    bcopy((char *)&service.key_high, (char *)(((long *) skey) + 1), 4);
    bzero((char *)&service, sizeof(service));
    kdb_encrypt_key (skey, skey, server_parm.master_key,
		     server_parm.master_key_schedule, DECRYPT);
    (void) krb_set_key((char *)skey, 0); /* if error, will show up when
					    rd_req fails */
    bzero((char *)skey, sizeof(skey));

    while (1) {
	if ((retval = krb_net_read(fd, (char *)&dlen, sizeof(u_short))) !=
	    sizeof(u_short)) {
	    if (retval < 0)
		log("dlen read: %s",error_message(errno));
	    else if (retval)
		log("short dlen read: %d",retval);
	    (void) close(fd);
	    cleanexit(retval ? 3 : 0);
	}
	if (exit_now) {
	    cleanexit(0);
	}
	dat_len = (int) ntohs(dlen);
	dat = (u_char *) malloc((unsigned)dat_len);
	if (!dat) {
	    log("malloc: No memory");
	    (void) close(fd);
	    cleanexit(4);
	}
	if ((retval = krb_net_read(fd, (char *)dat, dat_len)) != dat_len) {
	    if (retval < 0)
		log("data read: %s",error_message(errno));
	    else
		log("short read: %d vs. %d", dat_len, retval);
	    (void) close(fd);
	    cleanexit(5);
	}
    	if (exit_now) {
	    cleanexit(0);
	}
	if ((retval = kadm_ser_in(&dat,&dat_len)) != KADM_SUCCESS)
	    log("processing request: %s", error_message(retval));

	/* kadm_ser_in did the processing and returned stuff in
	   dat & dat_len , return the appropriate data */

    out:
	dlen = (u_short) dat_len;

	if (dat_len != (int)dlen) {
	    clear_secrets();
	    abort();			/* XXX */
	}
	dlen = htons(dlen);

	if (krb_net_write(fd, (char *)&dlen, sizeof(u_short)) < 0) {
	    log("writing dlen to client: %s",error_message(errno));
	    (void) close(fd);
	    cleanexit(6);
	}

	if (krb_net_write(fd, (char *)dat, dat_len) < 0) {
	    log(LOG_ERR, "writing to client: %s",error_message(errno));
	    (void) close(fd);
	    cleanexit(7);
	}
	free((char *)dat);
    }
    /*NOTREACHED*/
}

sigtype
do_child()
{
    /* SIGCHLD brings us here */
    int pid;
    register int i, j;

#ifdef POSIX
    int status;
#else
    union wait status;
#endif

    pid = wait(&status);

    for (i = 0; i < pidarraysize; i++)
	if (pidarray[i] == pid) {
	    /* found it */
	    for (j = i; j < pidarraysize-1; j++)
		/* copy others down */
		pidarray[j] = pidarray[j+1];
	    pidarraysize--;
	    if (WEXITSTATUS(status) || WCOREDUMP(status) || WIFSIGNALED(status))
		log("child %d: termsig %d, coredump %d, retcode %d", pid,
		    WTERMSIG(status), WCOREDUMP(status), WEXITSTATUS(status));
#ifdef POSIX
	    return;
#else /* !POSIX */
	    return(0);
#endif /* POSIX */
	}
    log("child %d not in list: termsig %d, coredump %d, retcode %d", pid,
	WTERMSIG(status), WCOREDUMP(status), WEXITSTATUS(status));
#ifdef POSIX
    return;
#else /* !POSIX */
    return(0);
#endif /* POSIX */
}

#ifndef DEBUG
void
cleanexit(val)
    int val;
{
    kerb_fini();
    clear_secrets();
    exit(val);
}
#endif

void
kill_children()
{
    register int i;
    int osigmask;

    osigmask = sigblock(sigmask(SIGCHLD));

    for (i = 0; i < pidarraysize; i++) {
	kill(pidarray[i], SIGINT);
	log("killing child %d", pidarray[i]);
    }
    sigsetmask(osigmask);
    return;
}
