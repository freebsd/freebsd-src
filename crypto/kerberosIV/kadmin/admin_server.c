/* 
  Copyright (C) 1989 by the Massachusetts Institute of Technology

   Export of this software from the United States of America is assumed
   to require a specific license from the United States Government.
   It is the responsibility of any person or organization contemplating
   export to obtain such a license before exporting.

WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
distribute this software and its documentation for any purpose and
without fee is hereby granted, provided that the above copyright
notice appear in all copies and that both that copyright notice and
this permission notice appear in supporting documentation, and that
the name of M.I.T. not be used in advertising or publicity pertaining
to distribution of the software without specific, written prior
permission.  M.I.T. makes no representations about the suitability of
this software for any purpose.  It is provided "as is" without express
or implied warranty.

  */

/*
 * Top-level loop of the kerberos Administration server
 */

/*
  admin_server.c
  this holds the main loop and initialization and cleanup code for the server
*/

#include "kadm_locl.h"

RCSID("$Id: admin_server.c,v 1.41 1997/05/27 15:52:53 bg Exp $");

/* Almost all procs and such need this, so it is global */
admin_params prm;		/* The command line parameters struct */

/* GLOBAL */
char *acldir = DEFAULT_ACL_DIR;
static char krbrlm[REALM_SZ];

static unsigned pidarraysize = 0;
static int *pidarray = (int *)0;

static int exit_now = 0;

static
RETSIGTYPE
doexit(int sig)
{
    exit_now = 1;
    SIGRETURN(0);
}
   
static
RETSIGTYPE
do_child(int sig)
{
    int pid;
    int i, j;

    int status;

    pid = wait(&status);

    /* Reinstall signal handlers for SysV. Must be done *after* wait */
    signal(SIGCHLD, do_child);

    for (i = 0; i < pidarraysize; i++)
	if (pidarray[i] == pid) {
	    /* found it */
	    for (j = i; j < pidarraysize-1; j++)
		/* copy others down */
		pidarray[j] = pidarray[j+1];
	    pidarraysize--;
	    if ((WIFEXITED(status) && WEXITSTATUS(status) != 0)
		|| WIFSIGNALED(status))
	      krb_log("child %d: termsig %d, retcode %d", pid,
		  WTERMSIG(status), WEXITSTATUS(status));
	    SIGRETURN(0);
	}
    krb_log("child %d not in list: termsig %d, retcode %d", pid,
	WTERMSIG(status), WEXITSTATUS(status));
    SIGRETURN(0);
}

static void
kill_children(void)
{
    int i;

    for (i = 0; i < pidarraysize; i++) {
	kill(pidarray[i], SIGINT);
	krb_log("killing child %d", pidarray[i]);
    }
}

/* close the system log file */
static void
close_syslog(void)
{
   krb_log("Shutting down admin server");
}

static void
byebye(void)			/* say goodnight gracie */
{
   printf("Admin Server (kadm server) has completed operation.\n");
}

static void
clear_secrets(void)
{
    memset(server_parm.master_key, 0, sizeof(server_parm.master_key));
    memset(server_parm.master_key_schedule, 0,
	  sizeof(server_parm.master_key_schedule));
    server_parm.master_key_version = 0L;
}

#ifdef DEBUG
#define cleanexit(code) {kerb_fini(); return;}
#endif

#ifndef DEBUG
static void
cleanexit(int val)
{
    kerb_fini();
    clear_secrets();
    exit(val);
}
#endif

static void
process_client(int fd, struct sockaddr_in *who)
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

#if defined(SO_KEEPALIVE) && defined(HAVE_SETSOCKOPT)
    if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (void *)&on, sizeof(on)) < 0)
	krb_log("setsockopt keepalive: %d",errno);
#endif

    server_parm.recv_addr = *who;

    if (kerb_init()) {			/* Open as client */
	krb_log("can't open krb db");
	cleanexit(1);
    }
    /* need to set service key to changepw.KRB_MASTER */

    status = kerb_get_principal(server_parm.sname, server_parm.sinst, &service,
			    1, &more);
    if (status == -1) {
      /* db locked */
      int32_t retcode = KADM_DB_INUSE;
      char *pdat;
      
      dat_len = KADM_VERSIZE + sizeof(retcode);
      dat = (u_char *) malloc((unsigned)dat_len);
      pdat = (char *) dat;
      retcode = htonl((u_int32_t) KADM_DB_INUSE);
      strncpy(pdat, KADM_ULOSE, KADM_VERSIZE);
      memcpy(pdat+KADM_VERSIZE, &retcode, sizeof(retcode));
      goto out;
    } else if (!status) {
      krb_log("no service %s.%s",server_parm.sname, server_parm.sinst);
      cleanexit(2);
    }

    copy_to_key(&service.key_low, &service.key_high, skey);
    memset(&service, 0, sizeof(service));
    kdb_encrypt_key (&skey, &skey, &server_parm.master_key,
		     server_parm.master_key_schedule, DES_DECRYPT);
    krb_set_key(skey, 0); /* if error, will show up when
					    rd_req fails */
    memset(skey, 0, sizeof(skey));

    while (1) {
	if ((retval = krb_net_read(fd, &dlen, sizeof(u_short))) !=
	    sizeof(u_short)) {
	    if (retval < 0)
		krb_log("dlen read: %s",error_message(errno));
	    else if (retval)
		krb_log("short dlen read: %d",retval);
	    close(fd);
	    cleanexit(retval ? 3 : 0);
	}
	if (exit_now) {
	    cleanexit(0);
	}
	dat_len = ntohs(dlen);
	dat = (u_char *) malloc(dat_len);
	if (!dat) {
	    krb_log("malloc: No memory");
	    close(fd);
	    cleanexit(4);
	}
	if ((retval = krb_net_read(fd, dat, dat_len)) != dat_len) {
	    if (retval < 0)
		krb_log("data read: %s",error_message(errno));
	    else
		krb_log("short read: %d vs. %d", dat_len, retval);
	    close(fd);
	    cleanexit(5);
	}
    	if (exit_now) {
	    cleanexit(0);
	}
	if ((retval = kadm_ser_in(&dat,&dat_len)) != KADM_SUCCESS)
	    krb_log("processing request: %s", error_message(retval));
    
	/* kadm_ser_in did the processing and returned stuff in
	   dat & dat_len , return the appropriate data */
    
    out:
	dlen = htons(dat_len);
    
	if (krb_net_write(fd, &dlen, sizeof(u_short)) < 0) {
	    krb_log("writing dlen to client: %s",error_message(errno));
	    close(fd);
	    cleanexit(6);
	}
    
	if (krb_net_write(fd, dat, dat_len) < 0) {
	    krb_log("writing to client: %s", error_message(errno));
	    close(fd);
	    cleanexit(7);
	}
	free(dat);
    }
    /*NOTREACHED*/
}

/*
kadm_listen
listen on the admin servers port for a request
*/
static int
kadm_listen(void)
{
    int found;
    int admin_fd;
    int peer_fd;
    fd_set mask, readfds;
    struct sockaddr_in peer;
    int addrlen;
    int pid;

    signal(SIGINT, doexit);
    signal(SIGTERM, doexit);
    signal(SIGHUP, doexit);
    signal(SIGQUIT, doexit);
    signal(SIGPIPE, SIG_IGN); /* get errors on write() */
    signal(SIGALRM, doexit);
    signal(SIGCHLD, do_child);
    if (setsid() < 0)
        krb_log("setsid() failed");

    if ((admin_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	return KADM_NO_SOCK;
#if defined(SO_REUSEADDR) && defined(HAVE_SETSOCKOPT)
    {
      int one=1;
      setsockopt(admin_fd, SOL_SOCKET, SO_REUSEADDR, (void *)&one,
		 sizeof(one));
    }
#endif
    if (bind(admin_fd, (struct sockaddr *)&server_parm.admin_addr,
	     sizeof(struct sockaddr_in)) < 0)
	return KADM_NO_BIND;
    listen(admin_fd, 1);
    FD_ZERO(&mask);
    FD_SET(admin_fd, &mask);

    for (;;) {				/* loop nearly forever */
	if (exit_now) {
	    clear_secrets();
	    kill_children();
	    return(0);
	}
	readfds = mask;
	if ((found = select(admin_fd+1, &readfds, 0,
			    0, (struct timeval *)0)) == 0)
	    continue;			/* no things read */
	if (found < 0) {
	    if (errno != EINTR)
		krb_log("select: %s",error_message(errno));
	    continue;
	}      
	if (FD_ISSET(admin_fd, &readfds)) {
	    /* accept the conn */
	    addrlen = sizeof(peer);
	    if ((peer_fd = accept(admin_fd, (struct sockaddr *)&peer,
				  &addrlen)) < 0) {
		krb_log("accept: %s",error_message(errno));
		continue;
	    }
#ifndef DEBUG
	    /* if you want a sep daemon for each server */
	    if ((pid = fork())) {
		/* parent */
		if (pid < 0) {
		    krb_log("fork: %s",error_message(errno));
		    close(peer_fd);
		    continue;
		}
		/* fork succeded: keep tabs on child */
		close(peer_fd);
		if (pidarray) {
		    pidarray = (int *)realloc(pidarray, ++pidarraysize);
		    pidarray[pidarraysize-1] = pid;
		} else {
		    pidarray = (int *)malloc(pidarraysize = 1);
		    pidarray[0] = pid;
		}
	    } else {
		/* child */
		close(admin_fd);
#endif /* DEBUG */
		/*
		 * If we are multihomed we need to figure out which
		 * local address that is used this time since it is
		 * used in "direction" comparison.
		 */
		getsockname(peer_fd,
			    (struct sockaddr *)&server_parm.admin_addr,
			    &addrlen);
		/* do stuff */
		process_client (peer_fd, &peer);
#ifndef DEBUG
	    }
#endif
	} else {
	    krb_log("something else woke me up!");
	    return(0);
	}
    }
    /*NOTREACHED*/
}

/*
** Main does the logical thing, it sets up the database and RPC interface,
**  as well as handling the creation and maintenance of the syslog file...
*/
int
main(int argc, char **argv)		/* admin_server main routine */
{
    int errval;
    int c;

    set_progname (argv[0]);

    umask(077);		/* Create protected files */

    /* initialize the admin_params structure */
    prm.sysfile = KADM_SYSLOG;		/* default file name */
    prm.inter = 0;

    memset(krbrlm, 0, sizeof(krbrlm));

    while ((c = getopt(argc, argv, "f:hmnd:a:r:")) != EOF)
	switch(c) {
	case 'f':			/* Syslog file name change */
	    prm.sysfile = optarg;
	    break;
	case 'n':
	    prm.inter = 0;
	    break;
	case 'm':
	    prm.inter = 1;
	    break;
	case 'a':			/* new acl directory */
	    acldir = optarg;
	    break;
	case 'd':
	    /* put code to deal with alt database place */
	    if ((errval = kerb_db_set_name(optarg)))
		errx (1, "opening database %s: %s",
		      optarg, error_message(errval));
	    break;
	case 'r':
	    strncpy(krbrlm, optarg, sizeof(krbrlm) - 1);
	    break;
	case 'h':			/* get help on using admin_server */
	default:
	    errx(1, "Usage: kadmind [-h] [-n] [-m] [-r realm] [-d dbname] [-f filename] [-a acldir]");
	}

    if (krbrlm[0] == 0)
	if (krb_get_lrealm(krbrlm, 0) != KSUCCESS)
	    errx (1, "Unable to get local realm.  Fix krb.conf or use -r.");

    printf("KADM Server %s initializing\n",KADM_VERSTR);
    printf("Please do not use 'kill -9' to kill this job, use a\n");
    printf("regular kill instead\n\n");

    kset_logfile(prm.sysfile);
    krb_log("Admin server starting");

    kerb_db_set_lockmode(KERB_DBL_NONBLOCKING);
    errval = kerb_init();		/* Open the Kerberos database */
    if (errval) {
	warnx ("error: kerb_init() failed");
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
	warnx("error:  %s",error_message(errval));
	kerb_fini();			/* Close if error */
    }
    close_syslog();			/* Close syslog file, print
					   closing note */
    byebye();				/* Say bye bye on the terminal
					   in use */
    exit(1);
}					/* procedure main */
