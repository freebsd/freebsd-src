/* $FreeBSD$ */

/*
 * Copyright (c) 1995, 1996, 1997, 1998 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "kauth.h"

RCSID("$Id: kauthd.c,v 1.25.2.1 2000/06/28 19:07:58 assar Exp $");

krb_principal princ;
static char locuser[SNAME_SZ];
static int  lifetime;
static char tktfile[MaxPathLen];

struct remote_args {
     int sock;
     des_key_schedule *schedule;
     des_cblock *session;
     struct sockaddr_in *me, *her;
};

static int
decrypt_remote_tkt (const char *user,
		    const char *inst,
		    const char *realm,
		    const void *varg,
		    key_proc_t key_proc,
		    KTEXT *cipp)
{
     char buf[BUFSIZ];
     void *ptr;
     int len;
     KTEXT cip  = *cipp;
     struct remote_args *args = (struct remote_args *)varg;

     write_encrypted (args->sock, cip->dat, cip->length,
		      *args->schedule, args->session, args->me,
		      args->her);
     len = read_encrypted (args->sock, buf, sizeof(buf), &ptr, *args->schedule,
			   args->session, args->her, args->me);
     memcpy(cip->dat, ptr, cip->length);
	  
     return 0;
}

static int
doit(int sock)
{
     int status;
     KTEXT_ST ticket;
     AUTH_DAT auth;
     char instance[INST_SZ];
     des_key_schedule schedule;
     struct sockaddr_in thisaddr, thataddr;
     int addrlen;
     int len;
     char buf[BUFSIZ];
     void *data;
     struct passwd *passwd;
     char version[KRB_SENDAUTH_VLEN + 1];
     char remotehost[MaxHostNameLen];

     addrlen = sizeof(thisaddr);
     if (getsockname (sock, (struct sockaddr *)&thisaddr, &addrlen) < 0 ||
	 addrlen != sizeof(thisaddr)) {
	  return 1;
     }
     addrlen = sizeof(thataddr);
     if (getpeername (sock, (struct sockaddr *)&thataddr, &addrlen) < 0 ||
	 addrlen != sizeof(thataddr)) {
	  return 1;
     }

     inaddr2str (thataddr.sin_addr, remotehost, sizeof(remotehost));

     k_getsockinst (sock, instance, sizeof(instance));
     status = krb_recvauth (KOPT_DO_MUTUAL, sock, &ticket, "rcmd", instance,
			    &thataddr, &thisaddr, &auth, "", schedule,
			    version);
     if (status != KSUCCESS ||
	 strncmp(version, KAUTH_VERSION, KRB_SENDAUTH_VLEN) != 0) {
	  return 1;
     }
     len = read_encrypted (sock, buf, sizeof(buf), &data, schedule,
			   &auth.session, &thataddr, &thisaddr);
     if (len < 0) {
	  write_encrypted (sock, "read_enc failed",
			   sizeof("read_enc failed") - 1, schedule,
			   &auth.session, &thisaddr, &thataddr);
	  return 1;
     }
     if (unpack_args(data, &princ, &lifetime, locuser,
		     tktfile)) {
	  write_encrypted (sock, "unpack_args failed",
			   sizeof("unpack_args failed") - 1, schedule,
			   &auth.session, &thisaddr, &thataddr);
	  return 1;
     }

     if( kuserok(&auth, locuser) != 0) {
	 snprintf(buf, sizeof(buf), "%s cannot get tickets for %s",
		  locuser, krb_unparse_name(&princ));
	 syslog (LOG_ERR, "%s", buf);
	 write_encrypted (sock, buf, strlen(buf), schedule,
			  &auth.session, &thisaddr, &thataddr);
	 return 1;
     }
     passwd = k_getpwnam (locuser);
     if (passwd == NULL) {
	  snprintf (buf, sizeof(buf), "No user '%s'", locuser);
	  syslog (LOG_ERR, "%s", buf);
	  write_encrypted (sock, buf, strlen(buf), schedule,
			   &auth.session, &thisaddr, &thataddr);
	  return 1;
     }
     if (setgid (passwd->pw_gid) ||
	 initgroups(passwd->pw_name, passwd->pw_gid) ||
	 setuid(passwd->pw_uid)) {
	  snprintf (buf, sizeof(buf), "Could not change user");
	  syslog (LOG_ERR, "%s", buf);
	  write_encrypted (sock, buf, strlen(buf), schedule,
			   &auth.session, &thisaddr, &thataddr);
	  return 1;
     }
     write_encrypted (sock, "ok", sizeof("ok") - 1, schedule,
		      &auth.session, &thisaddr, &thataddr);

     if (*tktfile == 0)
	 snprintf(tktfile, sizeof(tktfile), "%s%u", TKT_ROOT, (unsigned)getuid());
     krb_set_tkt_string (tktfile);

     {
	  struct remote_args arg;

	  arg.sock     = sock;
	  arg.schedule = &schedule;
	  arg.session  = &auth.session;
	  arg.me       = &thisaddr;
	  arg.her      = &thataddr;

	  status = krb_get_in_tkt (princ.name, princ.instance, princ.realm,
				   KRB_TICKET_GRANTING_TICKET,
				   princ.realm,
				   lifetime, NULL, decrypt_remote_tkt, &arg);
     }
     if (status == KSUCCESS) {
	 syslog (LOG_INFO, "from %s(%s): %s -> %s",
		 remotehost,
		 inet_ntoa(thataddr.sin_addr),
		 locuser,
		 krb_unparse_name (&princ));
	  write_encrypted (sock, "ok", sizeof("ok") - 1, schedule,
			   &auth.session, &thisaddr, &thataddr);
	  return 0;
     } else {
	  snprintf (buf, sizeof(buf), "TGT failed: %s", krb_get_err_text(status));
	  syslog (LOG_NOTICE, "%s", buf);
	  write_encrypted (sock, buf, strlen(buf), schedule,
			   &auth.session, &thisaddr, &thataddr);
	  return 1;
     }
}

int
main (int argc, char **argv)
{
    openlog ("kauthd", LOG_ODELAY, LOG_AUTH);

    if(argc > 1 && strcmp(argv[1], "-i") == 0)
	mini_inetd (k_getportbyname("kauth", "tcp", htons(KAUTH_PORT)));
    return doit(STDIN_FILENO);
}
