/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1990, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)kpasswdd.c	8.1 (Berkeley) 6/4/93";
#endif /* not lint */

/*
 * kpasswdd - update a principal's passwd field in the Kerberos
 * 	      database.  Called from inetd.
 * K. Fall
 * 12-Dec-88
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/signal.h>
#include <netinet/in.h>
#include <pwd.h>
#include <syslog.h>
#include <kerberosIV/des.h>
#include <kerberosIV/krb.h>
#include <kerberosIV/krb_db.h>
#include <stdio.h>
#include "kpasswd_proto.h"

static	struct kpasswd_data	kpwd_data;
static	des_cblock		master_key, key;
static	Key_schedule		master_key_schedule,
				key_schedule, random_sched;
long				mkeyversion;
AUTH_DAT			kdata;
static	Principal		principal_data;
static	struct update_data	ud_data;

char				inst[INST_SZ];
char				version[9];
KTEXT_ST			ticket;

char	*progname;		/* for the library */

main()
{
	struct	sockaddr_in	foreign;
	int			foreign_len = sizeof(foreign);
	int			rval, more;
	static  char	name[] = "kpasswdd";

	static	struct rlimit	rl = { 0, 0 };

	progname = name;
	openlog("kpasswdd", LOG_CONS | LOG_PID, LOG_AUTH);

	signal(SIGHUP, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	if (setrlimit(RLIMIT_CORE, &rl) < 0) {
		syslog(LOG_ERR, "setrlimit: %m");
		exit(1);
	}

	if (getpeername(0, &foreign, &foreign_len) < 0) {
		syslog(LOG_ERR,"getpeername: %m");
		exit(1);
	}

	strcpy(inst, "*");
	rval = krb_recvauth(
		0L,				/* options--!MUTUAL */
		0,				/* file desc */
		&ticket,			/* client's ticket */
		SERVICE,			/* expected service */
		inst,				/* expected instance */
		&foreign,			/* foreign addr */
		(struct sockaddr_in *) 0,	/* local addr */
		&kdata,				/* returned krb data */
		"",				/* service keys file */
		(bit_64 *) NULL,		/* returned key schedule */
		version
	);


	if (rval != KSUCCESS) {
		syslog(LOG_NOTICE, "krb_recvauth: %s", krb_err_txt[rval]);
		cleanup();
		exit(1);
	}

	if (*version == '\0') {
		/* indicates error on client's side (no tickets, etc.) */
		cleanup();
		exit(0);
	} else if (strcmp(version, "KPWDV0.1") != 0) {
		syslog(LOG_NOTICE,
			"kpasswdd version conflict (recv'd %s)",
			version);
		cleanup();
		exit(1);
	}


	/* get master key */
	if (kdb_get_master_key(0, master_key, master_key_schedule) != 0) {
		syslog(LOG_ERR, "couldn't get master key");
		cleanup();
		exit(1);
	}

	mkeyversion = kdb_get_master_key(NULL, master_key, master_key_schedule);

	if (mkeyversion < 0) {
		syslog(LOG_NOTICE, "couldn't verify master key");
		cleanup();
		exit(1);
	}

	/* get principal info */
	rval = kerb_get_principal(
		kdata.pname,
		kdata.pinst,
		&principal_data,
		1,
		&more
	);

	if (rval < 0) {
		syslog(LOG_NOTICE,
			"error retrieving principal record for %s.%s",
			kdata.pname, kdata.pinst);
		cleanup();
		exit(1);
	}

	if (rval != 1 || (more != 0)) {
		syslog(LOG_NOTICE, "more than 1 dbase entry for %s.%s",
			kdata.pname, kdata.pinst);
		cleanup();
		exit(1);
	}

	/* get the user's key */

	bcopy(&principal_data.key_low, key, 4);
	bcopy(&principal_data.key_high, ((long *) key) + 1, 4);
	kdb_encrypt_key(key, key, master_key, master_key_schedule,
		DECRYPT);
	key_sched(key, key_schedule);
	des_set_key(key, key_schedule);


	/* get random key and send it over {random} Kperson */

	random_key(kpwd_data.random_key);
	strcpy(kpwd_data.secure_msg, SECURE_STRING);
	if (des_write(0, &kpwd_data, sizeof(kpwd_data)) != sizeof(kpwd_data)) {
		syslog(LOG_NOTICE, "error writing initial data");
		cleanup();
		exit(1);
	}

	bzero(key, sizeof(key));
	bzero(key_schedule, sizeof(key_schedule));

	/* now read update info: { info }Krandom */

	key_sched(kpwd_data.random_key, random_sched);
	des_set_key(kpwd_data.random_key, random_sched);
	if (des_read(0, &ud_data, sizeof(ud_data)) != sizeof(ud_data)) {
		syslog(LOG_NOTICE, "update aborted");
		cleanup();
		exit(1);
	}

	/* validate info string by looking at the embedded string */

	if (strcmp(ud_data.secure_msg, SECURE_STRING) != 0) {
		syslog(LOG_NOTICE, "invalid update from %s",
			inet_ntoa(foreign.sin_addr));
		cleanup();
		exit(1);
	}

	/* produce the new key entry in the database { key }Kmaster */
	string_to_key(ud_data.pw, key);
	kdb_encrypt_key(key, key,
		master_key, master_key_schedule,
		ENCRYPT);
	bcopy(key, &principal_data.key_low, 4);
	bcopy(((long *) key) + 1,
		&principal_data.key_high, 4);
	bzero(key, sizeof(key));
	principal_data.key_version++;
	if (kerb_put_principal(&principal_data, 1)) {
		syslog(LOG_ERR, "couldn't write new record for %s.%s",
			principal_data.name, principal_data.instance);
		cleanup();
		exit(1);
	}

	syslog(LOG_NOTICE,"wrote new password field for %s.%s from %s",
		principal_data.name,
		principal_data.instance,
		inet_ntoa(foreign.sin_addr)
	);

	send_ack(0, "Update complete.\n");
	cleanup();
	exit(0);
}

cleanup()
{
	bzero(&kpwd_data, sizeof(kpwd_data));
	bzero(master_key, sizeof(master_key));
	bzero(master_key_schedule, sizeof(master_key_schedule));
	bzero(key, sizeof(key));
	bzero(key_schedule, sizeof(key_schedule));
	bzero(random_sched, sizeof(random_sched));
	bzero(&principal_data, sizeof(principal_data));
	bzero(&ud_data, sizeof(ud_data));
}

send_ack(remote, msg)
	int	remote;
	char	*msg;
{
	int	cc;
	cc = des_write(remote, msg, strlen(msg) + 1);
	if (cc <= 0) {
		syslog(LOG_NOTICE, "error writing ack");
		cleanup();
		exit(1);
	}
}
