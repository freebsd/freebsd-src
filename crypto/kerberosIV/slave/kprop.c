/*

Copyright 1987, 1988 by the Student Information Processing Board
	of the Massachusetts Institute of Technology

Permission to use, copy, modify, and distribute this software
and its documentation for any purpose and without fee is
hereby granted, provided that the above copyright notice
appear in all copies and that both that copyright notice and
this permission notice appear in supporting documentation,
and that the names of M.I.T. and the M.I.T. S.I.P.B. not be
used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.
M.I.T. and the M.I.T. S.I.P.B. make no representations about
the suitability of this software for any purpose.  It is
provided "as is" without express or implied warranty.

*/

#include "slav_locl.h"

RCSID("$Id: kprop.c,v 1.37 1999/09/16 20:41:59 assar Exp $");

#include "kprop.h"

static char kprop_version[KPROP_PROT_VERSION_LEN] = KPROP_PROT_VERSION;

int     debug = 0;

char    my_realm[REALM_SZ];
int     princ_data_size = 3 * sizeof(int32_t) + 3 * sizeof(unsigned char);
short   transfer_mode, net_transfer_mode;
int force_flag;
static char ok[] = ".dump_ok";

struct slave_host {
    u_int32_t  net_addr;
    char   *name;
    char   *instance;
    char   *realm;
    int	   not_time_yet;
    int    succeeded;
    struct slave_host *next;
};

static int
get_slaves(struct slave_host **psl,
	   const char *dir_path,
	   const char *file,
	   time_t ok_mtime)
{
    FILE   *fin;
    char    namebuf[128], *inst;
    char   *pc;
    struct hostent *host;
    struct slave_host **th;
    char   *last_prop_path;
    struct stat stbuf;

    if ((fin = fopen(file, "r")) == NULL)
	err (1, "open(%s)", file);

    th = psl;
    while(fgets(namebuf, sizeof(namebuf), fin)){
	if ((pc = strchr(namebuf, '\n'))) {
	    *pc = '\0';
	} else {
	    if(strlen(namebuf) == sizeof(namebuf) - 1){
		warnx ("Hostname too long (>= %d chars) in '%s'.",
		       (int) sizeof(namebuf), file);
		do{
		    if(fgets(namebuf, sizeof(namebuf), fin) == NULL)
			break;
		}while(strchr(namebuf, '\n') == NULL);
		continue;
	    }
	}
	if(namebuf[0] == 0 || namebuf[0] == '#')
	    continue;
	host = gethostbyname(namebuf);
	if (host == NULL) {
	    warnx ("Ignoring host '%s' in '%s': %s", 
		   namebuf, file,
		   hstrerror(h_errno));
	    continue;
	}
	(*th) = (struct slave_host *) malloc(sizeof(struct slave_host));
	if (!*th)
	    errx (1, "No memory reading host list from '%s'.",
		    file);
	memset(*th, 0, sizeof(struct slave_host));
	(*th)->name = strdup(namebuf);
	if ((*th)->name == NULL)
	    errx (1, "No memory reading host list from '%s'.",
		  file);
	/* get kerberos cannonical instance name */
	inst = krb_get_phost ((*th)->name);
	(*th)->instance = strdup(inst);
	if ((*th)->instance == NULL)
	    errx (1, "No memory reading host list from '%s'.",
		  file);
	/* what a concept, slave servers in different realms! */
	(*th)->realm = my_realm;
	memcpy(&(*th)->net_addr, host->h_addr, sizeof((*th)->net_addr));
	(*th)->not_time_yet = 0;
	(*th)->succeeded = 0;
	(*th)->next = NULL;
	asprintf(&last_prop_path, "%s%s-last-prop", dir_path, (*th)->name);
	if (last_prop_path == NULL)
	    errx (1, "malloc failed");
	if (!force_flag
	    && !stat(last_prop_path, &stbuf)
	    && stbuf.st_mtime > ok_mtime) {
	    (*th)->not_time_yet = 1;
	    (*th)->succeeded = 1;	/* no change since last success */
	}
	free(last_prop_path);
	th = &(*th)->next;
    }
    fclose(fin);
    return (1);
}

/* The master -> slave protocol looks like this:
     1) 8 byte version string
     2) 2 bytes of "transfer mode" (net byte order of course)
     3) ticket/authentication send by sendauth
     4) 4 bytes of "block" length (u_int32_t)
     5) data

     4 and 5 repeat til EOF ...
*/

static int
prop_to_slaves(struct slave_host *sl,
	       int fd,
	       const char *dir_path,
	       const char *fslv)
{
    u_char buf[KPROP_BUFSIZ];
    u_char obuf[KPROP_BUFSIZ + 64]; /* leave room for private msg overhead */
    struct sockaddr_in sin, my_sin;
    int     i, n, s;
    struct slave_host *cs;	/* current slave */
    char   my_host_name[MaxHostNameLen], *p_my_host_name;
    char   kprop_service_instance[INST_SZ];
    u_int32_t cksum;
    u_int32_t length, nlength;
    long   kerror;
    KTEXT_ST     ticket;
    CREDENTIALS  cred;
    MSG_DAT msg_dat;
    static char tkstring[] = "/tmp/kproptktXXXXXX";
    des_key_schedule session_sched;
    char   *last_prop_path;

    close(mkstemp(tkstring));
    krb_set_tkt_string(tkstring);
    
    memset(&sin, 0, sizeof sin);
    sin.sin_family = AF_INET;
    sin.sin_port = k_getportbyname ("krb_prop", "tcp", htons(KPROP_PORT));
    sin.sin_addr.s_addr = INADDR_ANY;

    for (i = 0; i < 5; i++) {	/* try each slave five times max */
	for (cs = sl; cs; cs = cs->next) {
	    if (!cs->succeeded) {
		if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		    err (1, "socket");
		memcpy(&sin.sin_addr, &cs->net_addr, 
		      sizeof cs->net_addr);

		if (connect(s, (struct sockaddr *) &sin, sizeof sin) < 0) {
		    warn ("connect(%s)", cs->name);
		    close(s);
		    continue;	/*** NEXT SLAVE ***/
		}
		
		/* for krb_mk_{priv, safe} */
		memset(&my_sin, 0, sizeof my_sin);
		n = sizeof my_sin;
		if (getsockname (s, (struct sockaddr *) &my_sin, &n) != 0) {
		    warn ("getsockname(%s)", cs->name);
		    close (s);
		    continue;	/*** NEXT SLAVE ***/
		}
		if (n != sizeof (my_sin)) {
		    warnx ("can't get socketname %s length", cs->name);
		    close (s);
		    continue;	/*** NEXT SLAVE ***/
		}
		
		/* Get ticket */
		kerror = krb_mk_req (&ticket, KPROP_SERVICE_NAME, 
				     cs->instance, cs->realm, (u_int32_t) 0);
		/* if ticket has expired try to get a new one, but
		 * first get a TGT ...
		 */
		if (kerror != MK_AP_OK) {
		    if (gethostname (my_host_name, sizeof(my_host_name)) != 0) {
			warnx ("gethostname(%s): %s",
			       my_host_name,
			       hstrerror(h_errno));
			close (s);
			break;	/* next one can't work either! */
		    }
		    /* get canonical kerberos service instance name */
		    p_my_host_name = krb_get_phost (my_host_name);
		    /* copy it to make sure gethostbyname static doesn't
		     * screw us. */
		    strlcpy (kprop_service_instance,
				     p_my_host_name,
				     INST_SZ);
		    kerror = krb_get_svc_in_tkt (KPROP_SERVICE_NAME, 
#if 0
						 kprop_service_instance,
#else
						 KRB_MASTER,
#endif
						 my_realm,
						 KRB_TICKET_GRANTING_TICKET,
						 my_realm,
						 96,
						 KPROP_SRVTAB);
		    if (kerror != INTK_OK) {
			warnx ("%s: %s.  While getting initial ticket\n",
			       cs->name, krb_get_err_text(kerror));
			close (s);
			goto punt;
		    }
		    kerror = krb_mk_req (&ticket, KPROP_SERVICE_NAME, 
					 cs->instance, cs->realm,
					 (u_int32_t) 0);
		}
		if (kerror != MK_AP_OK) {
		    warnx ("%s: krb_mk_req: %s",
			   cs->name, krb_get_err_text(kerror));
		    close (s);
		    continue;	/*** NEXT SLAVE ***/
		}		    

		if (write(s, kprop_version, sizeof(kprop_version))
		    != sizeof(kprop_version)) {
		    warn ("%s", cs->name);
		    close (s);
		    continue;	/*** NEXT SLAVE ***/
		}

		net_transfer_mode = htons (transfer_mode);
		if (write(s, &net_transfer_mode, sizeof(net_transfer_mode))
		    != sizeof(net_transfer_mode)) {
		    warn ("write(%s)", cs->name);
		    close (s);
		    continue;	/*** NEXT SLAVE ***/
		}

		kerror = krb_get_cred (KPROP_SERVICE_NAME, cs->instance,
				       cs->realm, &cred);
		if (kerror != KSUCCESS) {
		    warnx ("%s: %s.  Getting session key.", 
			   cs->name, krb_get_err_text(kerror));
		    close (s);
		    continue;	/*** NEXT SLAVE ***/
		}
#ifdef NOENCRYPTION
		memset(session_sched, 0, sizeof(session_sched));
#else
		if (des_key_sched (&cred.session, session_sched)) {
		    warnx ("%s: can't make key schedule.",
			   cs->name);
		    close (s);
		    continue;	/*** NEXT SLAVE ***/
		}
#endif
		/* SAFE (quad_cksum) and CLEAR are just not good enough */
		cksum = 0;
#ifdef not_working_yet
		if (transfer_mode != KPROP_TRANSFER_PRIVATE) {
		    cksum = get_data_checksum(fd, session_sched);
		    lseek(fd, 0L, 0);
		}
		else
#endif
           	{
		    struct stat st;
		    fstat (fd, &st);
		    cksum = st.st_size;
	        }
		kerror = krb_sendauth(KOPT_DO_MUTUAL,
				      s,
				      &ticket,
				      KPROP_SERVICE_NAME,
				      cs->instance,
				      cs->realm,
				      cksum,
				      &msg_dat,
				      &cred,
				      session_sched,
				      &my_sin,
				      &sin,
				      KPROP_PROT_VERSION);
		if (kerror != KSUCCESS) {
		    warnx ("%s: krb_sendauth: %s.",
			   cs->name, krb_get_err_text(kerror));
		    close (s);
		    continue;	/*** NEXT SLAVE ***/
		}

		lseek(fd, 0L, SEEK_SET); /* Rewind file before rereading it. */
		while ((n = read(fd, buf, sizeof buf))) {
		    if (n < 0)
			err (1, "read");
		    switch (transfer_mode) {
		    case KPROP_TRANSFER_PRIVATE:
		    case KPROP_TRANSFER_SAFE:
			if (transfer_mode == KPROP_TRANSFER_PRIVATE)
			    length = krb_mk_priv (buf, obuf, n, 
						  session_sched, &cred.session,
						  &my_sin, &sin);
			else
			    length = krb_mk_safe (buf, obuf, n,
						  &cred.session,
						  &my_sin, &sin);
			if (length == -1) {
			    warnx ("%s: %s failed.",
				   cs->name,
				   (transfer_mode == KPROP_TRANSFER_PRIVATE) 
				   ? "krb_rd_priv" : "krb_rd_safe");
			    close (s);
			    continue; /*** NEXT SLAVE ***/
			}
			nlength = htonl(length);
			if (write(s, &nlength, sizeof nlength)
			    != sizeof nlength) {
			    warn ("write(%s)", cs->name);
			    close (s);
			    continue; /*** NEXT SLAVE ***/
			}
			if (write(s, obuf, length) != length) {
			    warn ("write(%s)", cs->name);
			    close(s);
			    continue; /*** NEXT SLAVE ***/
			}
			break;
		    case KPROP_TRANSFER_CLEAR:
			if (write(s, buf, n) != n) {
			    warn ("write(%s)", cs->name);
			    close(s);
			    continue; /*** NEXT SLAVE ***/
			}
			break;
		    }
		}
		close(s);
		cs->succeeded = 1;
		printf("%s: success.\n", cs->name);

		asprintf(&last_prop_path,
			 "%s%s-last-prop",
			 dir_path,
			 cs->name);
		if (last_prop_path == NULL)
		    errx (1, "malloc failed");

		unlink(last_prop_path);
		close(creat(last_prop_path, 0600));
	    }
	}
    }
punt:
    
    dest_tkt();
    for (cs = sl; cs; cs = cs->next) {
	if (!cs->succeeded)
	    return (0);		/* didn't get this slave */
    }
    return (1);
}

static void
usage(void)
{
    /* already got floc and fslv, what is this? */
    fprintf(stderr,
	    "\nUsage: kprop [-force] [-realm realm] [-private"
#ifdef not_safe_yet
	    "|-safe|-clear"
#endif
	    "] [data_file [slaves_file]]\n\n");
    exit(1);
}


int
main(int argc, char **argv)
{
    int     fd, i;
    char   *floc, *floc_ok;
    char   *fslv;
    char   *dir_path;
    struct stat stbuf, stbuf_ok;
    time_t   l_init, l_final;
    char   *pc;
    int    l_diff;
    static struct slave_host *slave_host_list = NULL;
    struct slave_host *sh;

    set_progname (argv[0]);

    transfer_mode = KPROP_TRANSFER_PRIVATE;

    time(&l_init);
    pc = ctime(&l_init);
    pc[strlen(pc) - 1] = '\0';
    printf("\nStart slave propagation: %s\n", pc);
 
    floc = NULL;
    fslv = NULL;

    if (krb_get_lrealm(my_realm,1) != KSUCCESS)
      errx (1, "Getting my kerberos realm.  Check krb.conf");

    for (i = 1; i < argc; i++) 
      switch (argv[i][0]) {
      case '-':
	if (strcmp (argv[i], "-private") == 0) 
	  transfer_mode = KPROP_TRANSFER_PRIVATE;
#ifdef not_safe_yet
	else if (strcmp (argv[i], "-safe") == 0) 
	  transfer_mode = KPROP_TRANSFER_SAFE;
	else if (strcmp (argv[i], "-clear") == 0) 
	  transfer_mode = KPROP_TRANSFER_CLEAR;
#endif
	else if (strcmp (argv[i], "-realm") == 0) {
	    i++;
	    if (i < argc)
		strlcpy(my_realm, argv[i], REALM_SZ);
	    else
		usage();
	} else if (strcmp (argv[i], "-force") == 0)
	    force_flag++;
	else {
	    warnx("unknown control argument %s.", argv[i]);
	    usage ();
	}
	break;
      default:
	/* positional arguments are marginal at best ... */
	if (floc == NULL)
	  floc = argv[i];
	else {
	  if (fslv == NULL)
	    fslv = argv[i];
	  else 
	      usage();
	}
      }
    if(floc == NULL)
	floc = DB_DIR "/slave_dump";
    if(fslv == NULL)
	fslv = DB_DIR "/slaves";
	
    asprintf (&floc_ok, "%s%s", floc, ok);
    if (floc_ok == NULL)
	errx (1, "out of memory in copying %s", floc);

    dir_path = strdup(fslv);
    if(dir_path == NULL)
	errx (1, "malloc failed");
    pc = strrchr(dir_path, '/');
    if (pc != NULL)
	++pc;
    else
	pc = dir_path;
    *pc = '\0';

    if ((fd = open(floc, O_RDONLY)) < 0)
	err (1, "open(%s)", floc);
    if (flock(fd, LOCK_SH | LOCK_NB))
	err (1, "flock(%s)", floc);
    if (stat(floc, &stbuf))
	err (1, "stat(%s)", floc);
    if (stat(floc_ok, &stbuf_ok))
	err (1, "stat(%s)", floc_ok);
    if (stbuf.st_mtime > stbuf_ok.st_mtime)
	errx (1, "'%s' more recent than '%s'.", floc, floc_ok);
    if (!get_slaves(&slave_host_list, dir_path, fslv, stbuf_ok.st_mtime))
	errx (1, "can't read slave host file '%s'.", fslv);
#ifdef KPROP_DBG
    {
	struct slave_host *sh;
	int     i;
	fprintf(stderr, "\n\n");
	fflush(stderr);
	for (sh = slave_host_list; sh; sh = sh->next) {
	    fprintf(stderr, "slave %d: %s, %s", i++, sh->name,
		    inet_ntoa(sh->net_addr));
	    fflush(stderr);
	}
    }
#endif				/* KPROP_DBG */

    if (!prop_to_slaves(slave_host_list, fd, dir_path, fslv))
	errx (1, "propagation failed.");
    if (flock(fd, LOCK_UN))
	err (1, "flock(%s, LOCK_UN)", floc);
    printf("\n\n");
    for (sh = slave_host_list; sh; sh = sh->next) {
        if (sh->not_time_yet)
	    printf(         "%s:\t\tNot time yet\n", sh->name);
	else if (sh->succeeded)
	    printf(         "%s:\t\tSucceeded\n", sh->name);
	else
	    fprintf(stderr, "%s:\t\tFAILED\n", sh->name);
	fflush(stdout);
    }

    time(&l_final);
    l_diff = l_final - l_init;
    printf("propagation finished, %d:%02d:%02d elapsed\n",
	   l_diff / 3600, (l_diff % 3600) / 60, l_diff % 60);

    exit(0);
}

#ifdef doesnt_work_yet
u_long get_data_checksum(fd, key_sched)
     int fd;
     des_key_schedule key_sched;
{
	u_int32_t cksum = 0;
	int n;
	char buf[BUFSIZ];
	u_int32_t obuf[2];

	while (n = read(fd, buf, sizeof buf)) {
	    if (n < 0)
		err (1, "read");
	    cksum = cbc_cksum(buf, obuf, n, key_sched, key_sched);
	}
	return cksum;
}
#endif
