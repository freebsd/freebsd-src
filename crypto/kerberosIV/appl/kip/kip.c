/*
 * Copyright (c) 1995 - 2000 Kungliga Tekniska Högskolan
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

#include "kip.h"

RCSID("$Id: kip.c,v 1.18.2.1 2000/06/23 02:55:01 assar Exp $");

static char *cmd_str		= NULL;
static char *arg_str		= NULL;
static char *port_str		= NULL;
static int version_flag		= 0;
static int help_flag		= 0;

struct getargs args[] = {
    { "port",		'p',	arg_string,	&port_str,	"Use this port",
      "port" },
    { "cmd",		'c',	arg_string,	&cmd_str,
      "command to run when starting", "cmd"},
    { "arg",		'a',	arg_string,	&arg_str,
      "argument to above command", "arg"},
    { "version",	0, 	arg_flag,		&version_flag },
    { "help",		0, 	arg_flag,		&help_flag }
};


static RETSIGTYPE
disconnecthandler (int sig)
{
     disconnect = 1;
     SIGRETURN(0);
}

/*
 * Establish authenticated connection
 */

static int
connect_host (char *host, int port,
	      des_cblock *key, des_key_schedule schedule)
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
		hstrerror(h_errno));
	  return -1;
     }

     memset (&thataddr, 0, sizeof(thataddr));
     thataddr.sin_family = AF_INET;
     thataddr.sin_port   = port;

     for(p = hostent->h_addr_list; *p; ++p) {
	 memcpy (&thataddr.sin_addr, *p, sizeof(thataddr.sin_addr));

	 s = socket (AF_INET, SOCK_STREAM, 0);
	 if (s < 0) {
	     warn ("socket");
	     return -1;
	 }

#if defined(TCP_NODELAY) && defined(HAVE_SETSOCKOPT)
	 {
	     int one = 1;

	     setsockopt (s, IPPROTO_TCP, TCP_NODELAY,
			 (void *)&one, sizeof(one));
	 }
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
doit (char *host, int port)
{
     char tun_if_name[64];
     des_key_schedule schedule;
     des_cblock iv;
     int other, this, ret;

     other = connect_host (host, port, &iv, schedule);
     if (other < 0)
	  return 1;
     this = tunnel_open (tun_if_name, sizeof(tun_if_name));
     if (this < 0)
	  return 1;

     if (cmd_str) {
	 char buf[1024];
	 ret = kip_exec (cmd_str, buf, sizeof(buf),
			 "kip-control", "up", tun_if_name, host, arg_str,
			 NULL);
	 if (ret)
	     errx (1, "%s (up) failed: %s", cmd_str, buf);
     }

     ret = copy_packets (this, other, TUNMTU, &iv, schedule);

     if (cmd_str) {
	 char buf[1024];
	 ret = kip_exec (cmd_str, buf, sizeof(buf),
			 "kip-control", "down", tun_if_name, host, arg_str,
			 NULL);
	 if (ret)
	     errx (1, "%s (down) failed: %s", cmd_str, buf);
     }
     return 0;
}

static void
usage(int ret)
{
    arg_printusage (args,
		    sizeof(args) / sizeof(args[0]),
		    NULL,
		    "hostname");
    exit (ret);
}

/*
 * kip - forward IP packets over a kerberos-encrypted channel.
 *
 */

int
main(int argc, char **argv)
{
    int port;
    int optind = 0;
    char *hostname;

    set_progname (argv[0]);
    if (getarg (args, sizeof(args) / sizeof(args[0]), argc, argv,
		&optind))
	usage (1);

    if (help_flag)
	usage (0);

    if (version_flag) {
	print_version (NULL);
	return 0;
    }

    argv += optind;
    argc -= optind;

    if (argc != 1)
	usage (1);
    
    hostname = argv[0];

    if(port_str) {
	struct servent *s = roken_getservbyname (port_str, "tcp");

	if (s)
	    port = s->s_port;
	else {
	    char *ptr;

	    port = strtol (port_str, &ptr, 10);
	    if (port == 0 && ptr == port_str)
		errx (1, "bad port `%s'", port_str);
	    port = htons(port);
	}
    } else {
	port = k_getportbyname ("kip", "tcp", htons(KIPPORT));
    }

    signal (SIGCHLD, childhandler);
    signal (SIGHUP,  disconnecthandler);
    signal (SIGTERM, disconnecthandler);

    return doit (hostname, port);
}
