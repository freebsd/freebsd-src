/*
 * Copyright 1987 by the Massachusetts Institute of Technology.
 * 
 * For copying and distribution information, please see the file
 * MIT.Copyright.
 *
 * kprop/kpropd have been abandonded by Project Athena (for good reason)
 * however they still form the basis for one of the better ways for
 * distributing kerberos databases.  This version of kpropd has been
 * adapted from the MIT distribution to work properly in a 4.4BSD
 * environment.
 * 
 * $Revision: 4.5 $ $Date: 92/10/23 15:45:46 $ $State: Exp $
 * $Source$
 * 
 * Log: kpropd.c,v
 * Revision 4.5  92/10/23  15:45:46  tytso Make it possible
 * to specify the location of the kdb_util program.
 * 
 * Revision 4.4  91/06/15  03:20:51  probe Fixed <sys/types.h> inclusion
 * 
 * Revision 4.3  89/05/16  15:06:04  wesommer Fix operator precedence stuff.
 * Programmer: John Kohl.
 * 
 * Revision 4.2  89/03/23  10:24:00  jtkohl NOENCRYPTION changes
 * 
 * Revision 4.1  89/01/24  20:33:48  root name change
 * 
 * Revision 4.0  89/01/24  18:45:06  wesommer Original version; programmer:
 * wesommer auditor: jon
 * 
 * Revision 4.5  88/01/08  18:07:46  jon formatting and rcs header changes */

/*
 * This program is run on slave servers, to catch updates "pushed" from the
 * master kerberos server in a realm.
 */

#ifndef	lint
static char     rcsid_kpropd_c[] =
"$Header: /afs/net.mit.edu/project/krb4/src/slave/RCS/kpropd.c,v 4.5 92/10/23 15:45:46 tytso Exp $";
#endif	/* lint */

#include <ctype.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <syslog.h>
#include <krb.h>
#include <krb_db.h>

#include "kprop.h"

static char     kprop_version[KPROP_PROT_VERSION_LEN] = KPROP_PROT_VERSION;

extern int      errno;
int             debug = 0;

int             pause_int = 300;	/* 5 minutes in seconds */
unsigned long   get_data_checksum();
static void     SlowDeath();
		/* leave room for private msg overhead */
static char     buf[KPROP_BUFSIZ + 64];

static void 
usage()
{
	fprintf(stderr, "\nUsage: kpropd [-r realm] [-s srvtab] [-P kdb_util] fname\n");
	exit(2);
}

main(argc, argv)
	int             argc;
	char          **argv;
{
	struct sockaddr_in from;
	struct sockaddr_in sin;
	struct servent *sp;
	int             s, s2, fd, n, fdlock;
	int             from_len;
	char            local_file[256];
	char            local_temp[256];
	struct hostent *hp;
	char            hostname[256];
	unsigned long   cksum_read;
	unsigned long   cksum_calc;
	char            from_str[128];
	u_long          length;
	long            kerror;
	AUTH_DAT        auth_dat;
	KTEXT_ST        ticket;
	char            my_instance[INST_SZ];
	char            my_realm[REALM_SZ];
	char            cmd[1024];
	short           net_transfer_mode, transfer_mode;
	Key_schedule    session_sched;
	char            version[9];
	int             c;
	extern char    *optarg;
	extern int      optind;
	int             rflag;
	char           *srvtab = "";
	char           *local_db = DBM_FILE;
	char           *kdb_util = KPROP_KDB_UTIL;

	if (argv[argc - 1][0] == 'k' && isdigit(argv[argc - 1][1])) {
		argc--;		/* ttys file hack */
	}
	while ((c = getopt(argc, argv, "r:s:d:P:")) != EOF) {
		switch (c) {
		case 'r':
			rflag++;
			strcpy(my_realm, optarg);
			break;
		case 's':
			srvtab = optarg;
			break;
		case 'd':
			local_db = optarg;
			break;
		case 'P':
			kdb_util = optarg;
			break;
		default:
			usage();
			break;
		}
	}
	if (optind != argc - 1)
		usage();

	openlog("kpropd", LOG_PID, LOG_AUTH);

	strcpy(local_file, argv[optind]);
	strcat(strcpy(local_temp, argv[optind]), ".tmp");

#ifdef	STANDALONE

	if ((sp = getservbyname("krb_prop", "tcp")) == NULL) {
		syslog(LOG_ERR, "tcp/krb_prop: unknown service.");
		SlowDeath();
	}
	bzero(&sin, sizeof sin);
	sin.sin_port = sp->s_port;
	sin.sin_family = AF_INET;

	if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		syslog(LOG_ERR, "socket: %m");
		SlowDeath();
	}
	if (bind(s, (struct sockaddr *)&sin, sizeof sin) < 0) {
		syslog(LOG_ERR, "bind: %m");
		SlowDeath();
	}

#endif	/* STANDALONE */

	if (!rflag) {
		kerror = krb_get_lrealm(my_realm, 1);
		if (kerror != KSUCCESS) {
			syslog(LOG_ERR, "can't get local realm. %s",
				krb_err_txt[kerror]);
			SlowDeath();
		}
	}
	if (gethostname(my_instance, sizeof(my_instance)) != 0) {
		syslog(LOG_ERR, "gethostname: %m");
		SlowDeath();
	}

#ifdef	STANDALONE
	listen(s, 5);
	for (;;) {
		from_len = sizeof from;
		if ((s2 = accept(s, (struct sockaddr *)&from, &from_len)) < 0) {
			syslog(LOG_ERR, "accept: %m");
			continue;
		}
#else	/* !STANDALONE */

		s2 = 0;
		from_len = sizeof from;
		if (getpeername(0, (struct sockaddr *)&from, &from_len) < 0) {
			syslog(LOG_ERR, "getpeername: %m");
			SlowDeath();
		}

#endif	/* !STANDALONE */

		strcpy(from_str, inet_ntoa(from.sin_addr));

		if ((hp = gethostbyaddr((char *) &(from.sin_addr.s_addr),
					  from_len, AF_INET)) == NULL) {
			strcpy(hostname, "UNKNOWN");
		} else {
			strcpy(hostname, hp->h_name);
		}

		syslog(LOG_INFO, "connection from %s, %s", hostname, from_str);

		/* for krb_rd_{priv, safe} */
		n = sizeof sin;
		if (getsockname(s2, (struct sockaddr *)&sin, &n) != 0) {
			syslog(LOG_ERR, "can't get socketname: %m");
			SlowDeath();
		}
		if (n != sizeof(sin)) {
			syslog(LOG_ERR, "can't get socketname (length)");
			SlowDeath();
		}
		if ((fdlock = open(local_temp, O_WRONLY | O_CREAT, 0600)) < 0) {
			syslog(LOG_ERR, "open: %m");
			SlowDeath();
		}
		if (flock(fdlock, LOCK_EX | LOCK_NB)) {
			syslog(LOG_ERR, "flock: %m");
			SlowDeath();
		}
		if ((fd = creat(local_temp, 0600)) < 0) {
			syslog(LOG_ERR, "creat: %m");
			SlowDeath();
		}
		if ((n = read(s2, buf, sizeof(kprop_version)))
		    != sizeof(kprop_version)) {
			syslog(LOG_ERR,
			       "can't read protocol version (%d bytes)", n);
			SlowDeath();
		}
		if (strncmp(buf, kprop_version, sizeof(kprop_version)) != 0) {
			syslog(LOG_ERR, "unsupported version %s", buf);
			SlowDeath();
		}
		if ((n = read(s2, &net_transfer_mode,
			      sizeof(net_transfer_mode)))
		    != sizeof(net_transfer_mode)) {
			syslog(LOG_ERR, "can't read transfer mode");
			SlowDeath();
		}
		transfer_mode = ntohs(net_transfer_mode);
		kerror = krb_recvauth(KOPT_DO_MUTUAL, s2, &ticket,
				      KPROP_SERVICE_NAME,
				      my_instance,
				      &from,
				      &sin,
				      &auth_dat,
				      srvtab,
				      session_sched,
				      version);
		if (kerror != KSUCCESS) {
			syslog(LOG_ERR, "%s calling getkdata",
			       krb_err_txt[kerror]);
			SlowDeath();
		}
		syslog(LOG_INFO, "connection from %s.%s@%s",
		       auth_dat.pname, auth_dat.pinst, auth_dat.prealm);

		/*
		 * AUTHORIZATION is done here.  We might want to expand this
		 * to read an acl file at some point, but allowing for now
		 * KPROP_SERVICE_NAME.KRB_MASTER@local-realm is fine ...
		 */

		if ((strcmp(KPROP_SERVICE_NAME, auth_dat.pname) != 0) ||
		    (strcmp(KRB_MASTER, auth_dat.pinst) != 0) ||
		    (strcmp(my_realm, auth_dat.prealm) != 0)) {
			syslog(LOG_NOTICE, "authorization denied");
			SlowDeath();
		}
		switch (transfer_mode) {
		case KPROP_TRANSFER_PRIVATE:
			recv_auth(s2, fd, 1 /* private */ , &from, &sin, &auth_dat);
			break;
		case KPROP_TRANSFER_SAFE:
			recv_auth(s2, fd, 0 /* safe */ , &from, &sin, &auth_dat);
			break;
		case KPROP_TRANSFER_CLEAR:
			recv_clear(s2, fd);
			break;
		default:
			syslog(LOG_ERR, "bad transfer mode %d", transfer_mode);
			SlowDeath();
		}

		if (transfer_mode != KPROP_TRANSFER_PRIVATE) {
			syslog(LOG_ERR, "non-private transfers not supported\n");
			SlowDeath();
#ifdef doesnt_work_yet
			lseek(fd, (long) 0, L_SET);
			if (auth_dat.checksum != get_data_checksum(fd, session_sched)) {
				syslog(LOG_ERR, "checksum doesn't match");
				SlowDeath();
			}
#endif
		} else {
			struct stat     st;
			fstat(fd, &st);
			if (st.st_size != auth_dat.checksum) {
				syslog(LOG_ERR, "length doesn't match");
				SlowDeath();
			}
		}
		close(fd);
		close(s2);

		if (rename(local_temp, local_file) < 0) {
			syslog(LOG_ERR, "rename: %m");
			SlowDeath();
		}

		if (flock(fdlock, LOCK_UN)) {
			syslog(LOG_ERR, "flock (unlock): %m");
			SlowDeath();
		}
		close(fdlock);
		sprintf(cmd, "%s load %s %s\n", kdb_util, local_file, local_db);
		if (system(cmd) != 0) {
			syslog(LOG_ERR, "couldn't load database");
			SlowDeath();
		}

#ifdef	STANDALONE
	}
#endif

}

recv_auth(in, out, private, remote, local, ad)
	int             in, out;
	int             private;
	struct sockaddr_in *remote, *local;
	AUTH_DAT       *ad;
{
	u_long          length;
	long            kerror;
	int             n;
	MSG_DAT         msg_data;
	Key_schedule    session_sched;

	if (private)
#ifdef NOENCRYPTION
		bzero((char *) session_sched, sizeof(session_sched));
#else
		if (key_sched(ad->session, session_sched)) {
			syslog(LOG_ERR, "can't make key schedule");
			SlowDeath();
		}
#endif

	while (1) {
		n = krb_net_read(in, &length, sizeof length);
		if (n == 0)
			break;
		if (n < 0) {
			syslog(LOG_ERR, "read: %m");
			SlowDeath();
		}
		length = ntohl(length);
		if (length > sizeof buf) {
			syslog(LOG_ERR, "read length %d, bigger than buf %d",
			       length, sizeof buf);
			SlowDeath();
		}
		n = krb_net_read(in, buf, length);
		if (n < 0) {
			syslog(LOG_ERR, "kpropd: read: %m");
			SlowDeath();
		}
		if (private)
			kerror = krb_rd_priv(buf, n, session_sched, ad->session,
					     remote, local, &msg_data);
		else
			kerror = krb_rd_safe(buf, n, ad->session,
					     remote, local, &msg_data);
		if (kerror != KSUCCESS) {
			syslog(LOG_ERR, "%s: %s",
			       private ? "krb_rd_priv" : "krb_rd_safe",
			       krb_err_txt[kerror]);
			SlowDeath();
		}
		if (write(out, msg_data.app_data, msg_data.app_length) !=
		    msg_data.app_length) {
			syslog(LOG_ERR, "write: %m");
			SlowDeath();
		}
	}
}

recv_clear(in, out)
	int             in, out;
{
	int             n;

	while (1) {
		n = read(in, buf, sizeof buf);
		if (n == 0)
			break;
		if (n < 0) {
			syslog(LOG_ERR, "read: %m");
			SlowDeath();
		}
		if (write(out, buf, n) != n) {
			syslog(LOG_ERR, "write: %m");
			SlowDeath();
		}
	}
}

static void
SlowDeath()
{
#ifdef	STANDALONE
	sleep(pause_int);
#endif
	exit(1);
}

#ifdef doesnt_work_yet
unsigned long 
get_data_checksum(fd, key_sched)
	int             fd;
	Key_schedule    key_sched;
{
	unsigned long   cksum = 0;
	unsigned long   cbc_cksum();
	int             n;
	char            buf[BUFSIZ];
	char            obuf[8];

	while (n = read(fd, buf, sizeof buf)) {
		if (n < 0) {
			syslog(LOG_ERR, "read (in checksum test): %m");
			SlowDeath();
		}
#ifndef NOENCRYPTION
		cksum += cbc_cksum(buf, obuf, n, key_sched, key_sched);
#endif
	}
	return cksum;
}
#endif
