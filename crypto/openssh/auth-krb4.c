/*
 * Copyright (c) 1999 Dug Song.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "includes.h"
#include "packet.h"
#include "xmalloc.h"
#include "ssh.h"
#include "servconf.h"

RCSID("$OpenBSD: auth-krb4.c,v 1.18 2000/09/07 20:27:49 deraadt Exp $");
RCSID("$FreeBSD$");

#ifdef KRB4
char *ticket = NULL;

extern ServerOptions options;

/*
 * try krb4 authentication,
 * return 1 on success, 0 on failure, -1 if krb4 is not available
 */

int
auth_krb4_password(struct passwd * pw, const char *password)
{
	AUTH_DAT adata;
	KTEXT_ST tkt;
	struct hostent *hp;
	unsigned long faddr;
	char localhost[MAXHOSTNAMELEN];
	char phost[INST_SZ];
	char realm[REALM_SZ];
	int r;

	/*
	 * Try Kerberos password authentication only for non-root
	 * users and only if Kerberos is installed.
	 */
	if (pw->pw_uid != 0 && krb_get_lrealm(realm, 1) == KSUCCESS) {

		/* Set up our ticket file. */
		if (!krb4_init(pw->pw_uid)) {
			log("Couldn't initialize Kerberos ticket file for %s!",
			    pw->pw_name);
			goto kerberos_auth_failure;
		}
		/* Try to get TGT using our password. */
		r = krb_get_pw_in_tkt((char *) pw->pw_name, "",
		    realm, "krbtgt", realm,
		    DEFAULT_TKT_LIFE, (char *) password);
		if (r != INTK_OK) {
			packet_send_debug("Kerberos V4 password "
			    "authentication for %s failed: %s",
			    pw->pw_name, krb_err_txt[r]);
			goto kerberos_auth_failure;
		}
		/* Successful authentication. */
		chown(tkt_string(), pw->pw_uid, pw->pw_gid);

		/*
		 * Now that we have a TGT, try to get a local
		 * "rcmd" ticket to ensure that we are not talking
		 * to a bogus Kerberos server.
		 */
		(void) gethostname(localhost, sizeof(localhost));
		(void) strlcpy(phost, (char *) krb_get_phost(localhost),
		    INST_SZ);
		r = krb_mk_req(&tkt, KRB4_SERVICE_NAME, phost, realm, 33);

		if (r == KSUCCESS) {
			if (!(hp = gethostbyname(localhost))) {
				log("Couldn't get local host address!");
				goto kerberos_auth_failure;
			}
			memmove((void *) &faddr, (void *) hp->h_addr,
			    sizeof(faddr));

			/* Verify our "rcmd" ticket. */
			r = krb_rd_req(&tkt, KRB4_SERVICE_NAME, phost,
			    faddr, &adata, "");
			if (r == RD_AP_UNDEC) {
				/*
				 * Probably didn't have a srvtab on
				 * localhost. Disallow login.
				 */
				log("Kerberos V4 TGT for %s unverifiable, "
				    "no srvtab installed? krb_rd_req: %s",
				    pw->pw_name, krb_err_txt[r]);
				goto kerberos_auth_failure;
			} else if (r != KSUCCESS) {
				log("Kerberos V4 %s ticket unverifiable: %s",
				    KRB4_SERVICE_NAME, krb_err_txt[r]);
				goto kerberos_auth_failure;
			}
		} else if (r == KDC_PR_UNKNOWN) {
			/*
			 * Disallow login if no rcmd service exists, and
			 * log the error.
			 */
			log("Kerberos V4 TGT for %s unverifiable: %s; %s.%s "
			    "not registered, or srvtab is wrong?", pw->pw_name,
			krb_err_txt[r], KRB4_SERVICE_NAME, phost);
			goto kerberos_auth_failure;
		} else {
			/*
			 * TGT is bad, forget it. Possibly spoofed!
			 */
			packet_send_debug("WARNING: Kerberos V4 TGT "
			    "possibly spoofed for %s: %s",
			    pw->pw_name, krb_err_txt[r]);
			goto kerberos_auth_failure;
		}

		/* Authentication succeeded. */
		return 1;

kerberos_auth_failure:
		krb4_cleanup_proc(NULL);

		if (!options.krb4_or_local_passwd)
			return 0;
	} else {
		/* Logging in as root or no local Kerberos realm. */
		packet_send_debug("Unable to authenticate to Kerberos.");
	}
	/* Fall back to ordinary passwd authentication. */
	return -1;
}

void
krb4_cleanup_proc(void *ignore)
{
	debug("krb4_cleanup_proc called");
	if (ticket) {
		(void) dest_tkt();
		xfree(ticket);
		ticket = NULL;
	}
}

int
krb4_init(uid_t uid)
{
	static int cleanup_registered = 0;
	const char *tkt_root = TKT_ROOT;
	struct stat st;
	int fd;

	if (!ticket) {
		/* Set unique ticket string manually since we're still root. */
		ticket = xmalloc(MAXPATHLEN);
#ifdef AFS
		if (lstat("/ticket", &st) != -1)
			tkt_root = "/ticket/";
#endif /* AFS */
		snprintf(ticket, MAXPATHLEN, "%s%u_%d", tkt_root, uid, getpid());
		(void) krb_set_tkt_string(ticket);
	}
	/* Register ticket cleanup in case of fatal error. */
	if (!cleanup_registered) {
		fatal_add_cleanup(krb4_cleanup_proc, NULL);
		cleanup_registered = 1;
	}
	/* Try to create our ticket file. */
	if ((fd = mkstemp(ticket)) != -1) {
		close(fd);
		return 1;
	}
	/* Ticket file exists - make sure user owns it (just passed ticket). */
	if (lstat(ticket, &st) != -1) {
		if (st.st_mode == (S_IFREG | S_IRUSR | S_IWUSR) &&
		    st.st_uid == uid)
			return 1;
	}
	/* Failure - cancel cleanup function, leaving bad ticket for inspection. */
	log("WARNING: bad ticket file %s", ticket);
	fatal_remove_cleanup(krb4_cleanup_proc, NULL);
	cleanup_registered = 0;
	xfree(ticket);
	ticket = NULL;

	return 0;
}

int
auth_krb4(const char *server_user, KTEXT auth, char **client)
{
	AUTH_DAT adat = {0};
	KTEXT_ST reply;
	char instance[INST_SZ];
	int r, s;
	socklen_t slen;
	u_int cksum;
	Key_schedule schedule;
	struct sockaddr_in local, foreign;

	s = packet_get_connection_in();

	slen = sizeof(local);
	memset(&local, 0, sizeof(local));
	if (getsockname(s, (struct sockaddr *) & local, &slen) < 0)
		debug("getsockname failed: %.100s", strerror(errno));
	slen = sizeof(foreign);
	memset(&foreign, 0, sizeof(foreign));
	if (getpeername(s, (struct sockaddr *) & foreign, &slen) < 0) {
		debug("getpeername failed: %.100s", strerror(errno));
		fatal_cleanup();
	}
	instance[0] = '*';
	instance[1] = 0;

	/* Get the encrypted request, challenge, and session key. */
	if ((r = krb_rd_req(auth, KRB4_SERVICE_NAME, instance, 0, &adat, ""))) {
		packet_send_debug("Kerberos V4 krb_rd_req: %.100s", krb_err_txt[r]);
		return 0;
	}
	des_key_sched((des_cblock *) adat.session, schedule);

	*client = xmalloc(MAX_K_NAME_SZ);
	(void) snprintf(*client, MAX_K_NAME_SZ, "%s%s%s@%s", adat.pname,
	    *adat.pinst ? "." : "", adat.pinst, adat.prealm);

	/* Check ~/.klogin authorization now. */
	if (kuserok(&adat, (char *) server_user) != KSUCCESS) {
		packet_send_debug("Kerberos V4 .klogin authorization failed!");
		log("Kerberos V4 .klogin authorization failed for %s to account %s",
		    *client, server_user);
		xfree(*client);
		return 0;
	}
	/* Increment the checksum, and return it encrypted with the
	   session key. */
	cksum = adat.checksum + 1;
	cksum = htonl(cksum);

	/* If we can't successfully encrypt the checksum, we send back an
	   empty message, admitting our failure. */
	if ((r = krb_mk_priv((u_char *) & cksum, reply.dat, sizeof(cksum) + 1,
	    schedule, &adat.session, &local, &foreign)) < 0) {
		packet_send_debug("Kerberos V4 mk_priv: (%d) %s", r, krb_err_txt[r]);
		reply.dat[0] = 0;
		reply.length = 0;
	} else
		reply.length = r;

	/* Clear session key. */
	memset(&adat.session, 0, sizeof(&adat.session));

	packet_start(SSH_SMSG_AUTH_KRB4_RESPONSE);
	packet_put_string((char *) reply.dat, reply.length);
	packet_send();
	packet_write_wait();
	return 1;
}
#endif /* KRB4 */

#ifdef AFS
int
auth_krb4_tgt(struct passwd *pw, const char *string)
{
	CREDENTIALS creds;

	if (!radix_to_creds(string, &creds)) {
		log("Protocol error decoding Kerberos V4 tgt");
		packet_send_debug("Protocol error decoding Kerberos V4 tgt");
		goto auth_kerberos_tgt_failure;
	}
	if (strncmp(creds.service, "", 1) == 0)	/* backward compatibility */
		strlcpy(creds.service, "krbtgt", sizeof creds.service);

	if (strcmp(creds.service, "krbtgt")) {
		log("Kerberos V4 tgt (%s%s%s@%s) rejected for %s", creds.pname,
		    creds.pinst[0] ? "." : "", creds.pinst, creds.realm,
		    pw->pw_name);
		packet_send_debug("Kerberos V4 tgt (%s%s%s@%s) rejected for %s",
		    creds.pname, creds.pinst[0] ? "." : "", creds.pinst,
		    creds.realm, pw->pw_name);
		goto auth_kerberos_tgt_failure;
	}
	if (!krb4_init(pw->pw_uid))
		goto auth_kerberos_tgt_failure;

	if (in_tkt(creds.pname, creds.pinst) != KSUCCESS)
		goto auth_kerberos_tgt_failure;

	if (save_credentials(creds.service, creds.instance, creds.realm,
	    creds.session, creds.lifetime, creds.kvno,
	    &creds.ticket_st, creds.issue_date) != KSUCCESS) {
		packet_send_debug("Kerberos V4 tgt refused: couldn't save credentials");
		goto auth_kerberos_tgt_failure;
	}
	/* Successful authentication, passed all checks. */
	chown(tkt_string(), pw->pw_uid, pw->pw_gid);

	packet_send_debug("Kerberos V4 tgt accepted (%s.%s@%s, %s%s%s@%s)",
	    creds.service, creds.instance, creds.realm, creds.pname,
	    creds.pinst[0] ? "." : "", creds.pinst, creds.realm);
	memset(&creds, 0, sizeof(creds));
	packet_start(SSH_SMSG_SUCCESS);
	packet_send();
	packet_write_wait();
	return 1;

auth_kerberos_tgt_failure:
	krb4_cleanup_proc(NULL);
	memset(&creds, 0, sizeof(creds));
	packet_start(SSH_SMSG_FAILURE);
	packet_send();
	packet_write_wait();
	return 0;
}

int
auth_afs_token(struct passwd *pw, const char *token_string)
{
	CREDENTIALS creds;
	uid_t uid = pw->pw_uid;

	if (!radix_to_creds(token_string, &creds)) {
		log("Protocol error decoding AFS token");
		packet_send_debug("Protocol error decoding AFS token");
		packet_start(SSH_SMSG_FAILURE);
		packet_send();
		packet_write_wait();
		return 0;
	}
	if (strncmp(creds.service, "", 1) == 0)	/* backward compatibility */
		strlcpy(creds.service, "afs", sizeof creds.service);

	if (strncmp(creds.pname, "AFS ID ", 7) == 0)
		uid = atoi(creds.pname + 7);

	if (kafs_settoken(creds.realm, uid, &creds)) {
		log("AFS token (%s@%s) rejected for %s", creds.pname, creds.realm,
		    pw->pw_name);
		packet_send_debug("AFS token (%s@%s) rejected for %s", creds.pname,
		    creds.realm, pw->pw_name);
		memset(&creds, 0, sizeof(creds));
		packet_start(SSH_SMSG_FAILURE);
		packet_send();
		packet_write_wait();
		return 0;
	}
	packet_send_debug("AFS token accepted (%s@%s, %s@%s)", creds.service,
	    creds.realm, creds.pname, creds.realm);
	memset(&creds, 0, sizeof(creds));
	packet_start(SSH_SMSG_SUCCESS);
	packet_send();
	packet_write_wait();
	return 1;
}
#endif /* AFS */
