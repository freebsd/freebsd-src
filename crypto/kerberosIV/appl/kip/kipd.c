/* $FreeBSD$ */

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

RCSID("$Id: kipd.c,v 1.16.2.3 2000/10/18 20:46:45 assar Exp $");

static int
recv_conn (int sock, des_cblock *key, des_key_schedule schedule,
	   struct sockaddr_in *retaddr, char *user, size_t len)
{
     int status;
     KTEXT_ST ticket;
     AUTH_DAT auth;
     char instance[INST_SZ];
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
     if (passwd == NULL) {
	  fatal (sock, "Cannot find root", schedule, &auth.session);
	  return 1;
     }
     if (kuserok(&auth, "root") != 0) {
	  fatal (sock, "Permission denied", schedule, &auth.session);
	  return 1;
     }
     if (write (sock, &ok, sizeof(ok)) != sizeof(ok))
	  return 1;

     snprintf (user, len, "%s%s%s@%s", auth.pname, 
	       auth.pinst[0] != '\0' ? "." : "",
	       auth.pinst, auth.prealm);

     memcpy(key, &auth.session, sizeof(des_cblock));
     *retaddr = thataddr;
     return 0;
}

static int
doit(int sock)
{
     char msg[1024];
     char cmd[MAXPATHLEN];
     char tun_if_name[64];
     char user[MAX_K_NAME_SZ];
     struct sockaddr_in thataddr;
     des_key_schedule schedule;
     des_cblock key;
     int this, ret, ret2;

     isserver = 1;

     if (recv_conn (sock, &key, schedule, &thataddr, user, sizeof(user)))
	  return 1;
     this = tunnel_open (tun_if_name, sizeof(tun_if_name));
     if (this < 0)
	  fatal (sock, "Cannot open " _PATH_DEV TUNDEV, schedule, &key);

     strlcpy(cmd, LIBEXECDIR "/kipd-control", sizeof(cmd));

     ret = kip_exec (cmd, msg, sizeof(msg), "kipd-control",
		     "up", tun_if_name, inet_ntoa(thataddr.sin_addr), user,
		     NULL);
     if (ret) {
	 fatal (sock, msg, schedule, &key);
	 return -1;
     }

     ret = copy_packets (this, sock, TUNMTU, &key, schedule);
     
     ret2 = kip_exec (cmd,  msg, sizeof(msg), "kipd-control",
		      "down", tun_if_name, user, NULL);
     if (ret2)
	 syslog(LOG_ERR, "%s", msg);
     return ret;
}

static char *port_str		= NULL;
static int inetd_flag		= 1;
static int version_flag		= 0;
static int help_flag		= 0;

struct getargs args[] = {
    { "inetd",		'i',	arg_negative_flag,	&inetd_flag,
      "Not started from inetd" },
    { "port",		'p',	arg_string,	&port_str,	"Use this port",
      "port" },
    { "version",	0, 	arg_flag,		&version_flag },
    { "help",		0, 	arg_flag,		&help_flag }
};

static void
usage(int ret)
{
    arg_printusage (args,
		    sizeof(args) / sizeof(args[0]),
		    NULL,
		    "");
    exit (ret);
}

/*
 * kipd - receive forwarded IP
 */

int
main (int argc, char **argv)
{
    int port;
    int optind = 0;

    set_progname (argv[0]);
    roken_openlog(__progname, LOG_PID|LOG_CONS, LOG_DAEMON);

    if (getarg (args, sizeof(args) / sizeof(args[0]), argc, argv,
		&optind))
	usage (1);

    if (help_flag)
	usage (0);

    if (version_flag) {
	print_version (NULL);
	return 0;
    }

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

    if (!inetd_flag)
	mini_inetd (port);

    signal (SIGCHLD, childhandler);
    return doit(STDIN_FILENO);
}
