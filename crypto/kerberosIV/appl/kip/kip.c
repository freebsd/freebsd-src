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

RCSID("$Id: kip.c,v 1.15 1997/05/11 10:54:51 assar Exp $");

static void
usage()
{
     fprintf (stderr, "Usage: %s host\n",
	      __progname);
     exit (1);
}

/*
 * Establish authenticated connection
 */

static int
connect_host (char *host, des_cblock *key, des_key_schedule schedule)
{
     CREDENTIALS cred;
     KTEXT_ST text;
     MSG_DAT msg;
     int status;
     struct sockaddr_in thisaddr, thataddr;
     int addrlen;
     struct hostent *hostent;
     int s;
     u_char b;
     char **p;

     hostent = gethostbyname (host);
     if (hostent == NULL) {
	 warnx ("gethostbyname '%s': %s", host,
#ifdef HAVE_H_ERRNO
		hstrerror(h_errno)
#else
		"unknown error"
#endif
	     );
	  return -1;
     }

     memset (&thataddr, 0, sizeof(thataddr));
     thataddr.sin_family = AF_INET;
     thataddr.sin_port   = k_getportbyname ("kip", "tcp", htons(KIPPORT));

     for(p = hostent->h_addr_list; *p; ++p) {
	 int one = 1;

	 memcpy (&thataddr.sin_addr, *p, sizeof(thataddr.sin_addr));

	 s = socket (AF_INET, SOCK_STREAM, 0);
	 if (s < 0) {
	     warn ("socket");
	     return -1;
	 }

#if defined(TCP_NODELAY) && defined(HAVE_SETSOCKOPT)
	 setsockopt (s, IPPROTO_TCP, TCP_NODELAY, (void *)&one, sizeof(one));
#endif

	 if (connect (s, (struct sockaddr *)&thataddr, sizeof(thataddr)) < 0) {
	     warn ("connect(%s)", host);
	     close (s);
	     continue;
	 } else {
	     break;
	 }
     }
     if (*p == NULL)
	 return -1;

     addrlen = sizeof(thisaddr);
     if (getsockname (s, (struct sockaddr *)&thisaddr, &addrlen) < 0 ||
	 addrlen != sizeof(thisaddr)) {
	 warn ("getsockname(%s)", host);
	 return -1;
     }
     status = krb_sendauth (KOPT_DO_MUTUAL, s, &text, "rcmd",
			    host, krb_realmofhost (host),
			    getpid(), &msg, &cred, schedule,
			    &thisaddr, &thataddr, KIP_VERSION);
     if (status != KSUCCESS) {
	 warnx("%s: %s", host,
	       krb_get_err_text(status));
	 return -1;
     }
     if (read (s, &b, sizeof(b)) != sizeof(b)) {
	 warn ("read");
	 return -1;
     }
     if (b) {
	  char buf[BUFSIZ];

	  read (s, buf, sizeof(buf));
	  buf[BUFSIZ - 1] = '\0';

	  warnx ("%s: %s", host, buf);
	  return -1;
     }

     memcpy(key, &cred.session, sizeof(des_cblock));
     return s;
}

/*
 * Connect to the given host.
 */

static int
doit (char *host)
{
     des_key_schedule schedule;
     des_cblock iv;
     int other, this;
     struct ifreq ifreq;
     int sock;

     other = connect_host (host, &iv, schedule);
     if (other < 0)
	  return 1;
     this = tunnel_open ();
     if (this < 0)
	  return 1;
     return copy_packets (this, other, TUNMTU, &iv, schedule);
}

/*
 * kip - forward IP packets over a kerberos-encrypted channel.
 *
 */

int
main(int argc, char **argv)
{
    set_progname (argv[0]);

    if (argc != 2)
	usage ();
    return doit (argv[1]);
}
