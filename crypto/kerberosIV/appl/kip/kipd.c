/*
 * Copyright (c) 1995, 1996, 1997 Kungliga Tekniska Högskolan
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      Högskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
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

#include "kip.h"

RCSID("$Id: kipd.c,v 1.13 1997/05/18 20:38:01 assar Exp $");

static int
fatal (int fd, char *s)
{
     u_char err = 1;

     write (fd, &err, sizeof(err));
     write (fd, s, strlen(s)+1);
     syslog(LOG_ERR, s);
     return err;
}

static int
recv_conn (int sock, des_cblock *key, des_key_schedule schedule,
	   struct sockaddr_in *retaddr)
{
     int status;
     KTEXT_ST ticket;
     AUTH_DAT auth;
     char instance[INST_SZ + 1];
     struct sockaddr_in thisaddr, thataddr;
     int addrlen;
     char version[KRB_SENDAUTH_VLEN + 1];
     u_char ok = 0;
     struct passwd *passwd;

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

     k_getsockinst (sock, instance, sizeof(instance));
     status = krb_recvauth (KOPT_DO_MUTUAL, sock, &ticket, "rcmd", instance,
			    &thataddr, &thisaddr, &auth, "", schedule,
			    version);
     if (status != KSUCCESS ||
	 strncmp(version, KIP_VERSION, KRB_SENDAUTH_VLEN) != 0) {
	  return 1;
     }
     passwd = k_getpwnam ("root");
     if (passwd == NULL)
	  return fatal (sock, "Cannot find root");
     if (kuserok(&auth, "root") != 0)
	  return fatal (sock, "Permission denied");
     if (write (sock, &ok, sizeof(ok)) != sizeof(ok))
	  return 1;

     memcpy(key, &auth.session, sizeof(des_cblock));
     *retaddr = thataddr;
     return 0;
}

static int
doit(int sock)
{
     struct sockaddr_in thataddr;
     des_key_schedule schedule;
     des_cblock key;
     int this;

     if (recv_conn (sock, &key, schedule, &thataddr))
	  return 1;
     this = tunnel_open ();
     if (this < 0)
	  fatal (sock, "Cannot open " _PATH_DEV TUNDEV);
     return copy_packets (this, sock, TUNMTU, &key, schedule);
}

/*
 * kipd - receive forwarded IP
 */

int
main (int argc, char **argv)
{
    set_progname (argv[0]);

    openlog(__progname, LOG_PID|LOG_CONS, LOG_DAEMON);
    signal (SIGCHLD, childhandler);
    return doit(0);
}
