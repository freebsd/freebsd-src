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

RCSID("$Id: admin_server.c,v 1.49.2.2 2000/10/18 20:24:57 assar Exp $");

/* Almost all procs and such need this, so it is global */
admin_params prm;		/* The command line parameters struct */

/* GLOBAL */
char *acldir = DEFAULT_ACL_DIR;
static char krbrlm[REALM_SZ];

#define MAXCHILDREN 100

struct child {
    pid_t pid;
    int pipe_fd;
    int authenticated;
};

static unsigned nchildren = 0;
static struct child children[MAXCHILDREN];

static int exit_now = 0;

static
RETSIGTYPE
doexit(int sig)
{
    exit_now = 1;
    SIGRETURN(0);
}
   
static sig_atomic_t do_wait;

static
RETSIGTYPE
do_child(int sig)
{
    do_wait = 1;
    SIGRETURN(0);
}


static void
kill_children(void)
{
    int i;

    for (i = 0; i < nchildren; i++) {
	kill(children[i].pid, SIGINT);
	close (children[i].pipe_fd);
	krb_log("killing child %d", children[i].pid);
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

static void
cleanexit(int val)
{
    kerb_fini();
    clear_secrets();
    exit(val);
}

static RETSIGTYPE
sigalrm(int sig)
{
    cleanexit(1);
}

/*
 * handle the client on the socket `fd' from `who'
 * `signal_fd' is a pipe on which to signal when the user has been
 * authenticated
 */

static void
process_client(int fd, struct sockaddr_in *who, int signal_fd)
{
    u_char *dat;
    int dat_len;
    u_short dlen;
    int retval;
    Principal service;
    des_cblock skey;
    int more;
    int status;
    int authenticated = 0;

    /* make this connection time-out after 1 second if the user has
       not managed one transaction succesfully in kadm_ser_in */

    signal(SIGALRM, sigalrm);
    alarm(2);

#if defined(SO_KEEPALIVE) && defined(HAVE_SETSOCKOPT)
    {
	int on = 1;
	    
	if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE,
		       (void *)&on, sizeof(on)) < 0)
	    krb_log("setsockopt keepalive: %d",errno);
    }
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
      char *pdat;
      
      dat_len = KADM_VERSIZE + 4;
      dat = (u_char *) malloc(dat_len);
      if (dat == NULL) {
	  krb_log("malloc failed");
	  cleanexit(4);
      }
      pdat = (char *) dat;
      memcpy(pdat, KADM_ULOSE, KADM_VERSIZE);
      krb_put_int (KADM_DB_INUSE, pdat + KADM_VERSIZE, 4, 4);
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
	void *errpkt;

	errpkt = malloc(KADM_VERSIZE + 4);
	if (errpkt == NULL) {
	    krb_log("malloc: no memory");
	    close(fd);
	    cleanexit(4);
	}

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
	if (dat == NULL) {
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
	retval = kadm_ser_in(&dat, &dat_len, errpkt);

	if (retval == KADM_SUCCESS) {
	    if (!authenticated) {
		unsigned char one = 1;

		authenticated = 1;
		alarm (0);
		write (signal_fd, &one, 1);
	    }
	} else {
	    krb_log("processing request: %s", error_message(retval));
	}
    
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

static void
accept_client (int admin_fd)
{
    int pipe_fd[2];
    int addrlen;
    struct sockaddr_in peer;
    pid_t pid;
    int peer_fd;

    /* using up the maximum number of children, try to get rid
       of one unauthenticated one */

    if (nchildren >= MAXCHILDREN) {
	int i, nunauth = 0;
	int victim;

	for (;;) {
	    for (i = 0; i < nchildren; ++i)
		if (children[i].authenticated == 0)
		    ++nunauth;
	    if (nunauth == 0)
		return;

	    victim = rand() % nchildren;
	    if (children[victim].authenticated == 0) {
		kill(children[victim].pid, SIGINT);
		close(children[victim].pipe_fd);
		for (i = victim; i < nchildren; ++i)
		    children[i] = children[i + 1];
		--nchildren;
		break;
	    }
	}
    }

    /* accept the conn */
    addrlen = sizeof(peer);
    peer_fd = accept(admin_fd, (struct sockaddr *)&peer, &addrlen);
    if (peer_fd < 0) {
	krb_log("accept: %s",error_message(errno));
	return;
    }
    if (pipe (pipe_fd) < 0) {
	krb_log ("pipe: %s", error_message(errno));
	return;
    }

    if (pipe_fd[0] >= FD_SETSIZE
	|| pipe_fd[1] >= FD_SETSIZE) {
	krb_log ("pipe fds too large");
	close (pipe_fd[0]);
	close (pipe_fd[1]);
	return;
    }

    pid = fork ();

    if (pid < 0) {
	krb_log ("fork: %s", error_message(errno));
	close (pipe_fd[0]);
	close (pipe_fd[1]);
	return;
    }

    if (pid != 0) {
	/* parent */
	/* fork succeded: keep tabs on child */
	close(peer_fd);
	children[nchildren].pid     = pid;
	children[nchildren].pipe_fd = pipe_fd[0];
	children[nchildren].authenticated = 0;
	++nchildren;
	close (pipe_fd[1]);

    } else {
	int i;

	/* child */
	close(admin_fd);
	close(pipe_fd[0]);

	for (i = 0; i < nchildren; ++i)
	    close (children[i].pipe_fd);

	/*
	 * If we are multihomed we need to figure out which
	 * local address that is used this time since it is
	 * used in "direction" comparison.
	 */
	getsockname(peer_fd,
		    (struct sockaddr *)&server_parm.admin_addr,
		    &addrlen);
	/* do stuff */
	process_client (peer_fd, &peer, pipe_fd[1]);
    }
}

/*
 * handle data signaled from child `child' kadmind
 */

static void
handle_child_signal (int child)
{
    int ret;
    unsigned char data[1];

    ret = read (children[child].pipe_fd, data, 1);
    if (ret < 0) {
	if (errno != EINTR)
	    krb_log ("read from child %d: %s", child,
		     error_message(errno));
	return;
    }
    if (ret == 0) {
	close (children[child].pipe_fd);
	children[child].pipe_fd = -1;
	return;
    }
    if (data)
	children[child].authenticated = 1;
}

/*
 * handle dead children
 */

static void
handle_sigchld (void)
{
    pid_t pid;
    int status;
    int i, j;

    for (;;) {
	int found = 0;

	pid = waitpid(-1, &status, WNOHANG|WUNTRACED);
	if (pid == 0 || (pid < 0 && errno == ECHILD))
	    break;
	if (pid < 0) {
	    krb_log("waitpid: %s", error_message(errno));
	    break;
	}
	for (i = 0; i < nchildren; i++)
	    if (children[i].pid == pid) {
		/* found it */
		close(children[i].pipe_fd);
		for (j = i; j < nchildren; j++)
		    /* copy others down */
		    children[j] = children[j+1];
		--nchildren;
#if 0
		if ((WIFEXITED(status) && WEXITSTATUS(status) != 0)
		    || WIFSIGNALED(status))
		    krb_log("child %d: termsig %d, retcode %d", pid,
			    WTERMSIG(status), WEXITSTATUS(status));
#endif
		found = 1;
	    }
#if 0
	if (!found)
	    krb_log("child %d not in list: termsig %d, retcode %d", pid,
		    WTERMSIG(status), WEXITSTATUS(status));
#endif
    }
    do_wait = 0;
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
    fd_set readfds;

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

    if (admin_fd >= FD_SETSIZE) {
	krb_log("admin_fd too big");
	return KADM_NO_BIND;
    }
	
#if defined(SO_REUSEADDR) && defined(HAVE_SETSOCKOPT)
    {
      int one = 1;
      setsockopt(admin_fd, SOL_SOCKET, SO_REUSEADDR, (void *)&one,
		 sizeof(one));
    }
#endif
    if (bind(admin_fd, (struct sockaddr *)&server_parm.admin_addr,
	     sizeof(struct sockaddr_in)) < 0)
	return KADM_NO_BIND;
    if (listen(admin_fd, SOMAXCONN) < 0)
	return KADM_NO_BIND;

    for (;;) {				/* loop nearly forever */
	int i;
	int maxfd = -1;

	if (exit_now) {
	    clear_secrets();
	    kill_children();
	    return(0);
	}
	if (do_wait)
	    handle_sigchld ();

	FD_ZERO(&readfds);
	FD_SET(admin_fd, &readfds);
	maxfd = max(maxfd, admin_fd);
	for (i = 0; i < nchildren; ++i)
	    if (children[i].pipe_fd >= 0) {
		FD_SET(children[i].pipe_fd, &readfds);
		maxfd = max(maxfd, children[i].pipe_fd);
	    }

	found = select(maxfd + 1, &readfds, NULL, NULL, NULL);
	if (found < 0) {
	    if (errno != EINTR)
		krb_log("select: %s",error_message(errno));
	    continue;
	}
	if (FD_ISSET(admin_fd, &readfds)) 
	    accept_client (admin_fd);
	for (i = 0; i < nchildren; ++i)
	    if (children[i].pipe_fd >= 0
		&& FD_ISSET(children[i].pipe_fd, &readfds)) {
		handle_child_signal (i);
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
    struct in_addr i_addr;

    set_progname (argv[0]);

    umask(077);		/* Create protected files */

    i_addr.s_addr = INADDR_ANY;
    /* initialize the admin_params structure */
    prm.sysfile = KADM_SYSLOG;		/* default file name */
    prm.inter = 0;

    memset(krbrlm, 0, sizeof(krbrlm));

    while ((c = getopt(argc, argv, "f:hmnd:a:r:i:")) != -1)
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
	    strlcpy (krbrlm, optarg, sizeof(krbrlm));
	    break;
	case 'i':
	    /* Only listen on this address */
	    if(inet_aton (optarg, &i_addr) == 0) {
		fprintf (stderr, "Bad address: %s\n", optarg);
		exit (1);
	    }
	    break;
	case 'h':			/* get help on using admin_server */
	default:
	    errx(1, "Usage: kadmind [-h] [-n] [-m] [-r realm] [-d dbname] [-f filename] [-a acldir] [-i address_to_listen_on]");
	}

    if (krbrlm[0] == 0)
	if (krb_get_lrealm(krbrlm, 1) != KSUCCESS)
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
    if ((errval = kadm_ser_init(prm.inter, krbrlm, i_addr))==KADM_SUCCESS) {
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
