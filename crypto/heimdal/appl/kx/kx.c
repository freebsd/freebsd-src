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

#include "kx.h"

RCSID("$Id: kx.c,v 1.68 2001/02/20 01:44:45 assar Exp $");

static int nchild;
static int donep;

/*
 * Signal handler that justs waits for the children when they die.
 */

static RETSIGTYPE
childhandler (int sig)
{
     pid_t pid;
     int status;

     do { 
	 pid = waitpid (-1, &status, WNOHANG|WUNTRACED);
	 if (pid > 0 && (WIFEXITED(status) || WIFSIGNALED(status)))
	     if (--nchild == 0 && donep)
		 exit (0);
     } while(pid > 0);
     signal (SIGCHLD, childhandler);
     SIGRETURN(0);
}

/*
 * Handler for SIGUSR1.
 * This signal means that we should wait until there are no children
 * left and then exit.
 */

static RETSIGTYPE
usr1handler (int sig)
{
    donep = 1;

    SIGRETURN(0);
}

/*
 * Almost the same as for SIGUSR1, except we should exit immediately
 * if there are no active children.
 */

static RETSIGTYPE
usr2handler (int sig)
{
    donep = 1;
    if (nchild == 0)
	exit (0);

    SIGRETURN(0);
}

/*
 * Establish authenticated connection.  Return socket or -1.
 */

static int
connect_host (kx_context *kc)
{
    struct addrinfo *ai, *a;
    struct addrinfo hints;
    int error;
    char portstr[NI_MAXSERV];
    socklen_t addrlen;
    int s;
    struct sockaddr_storage thisaddr_ss;
    struct sockaddr *thisaddr = (struct sockaddr *)&thisaddr_ss;

    memset (&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    snprintf (portstr, sizeof(portstr), "%u", ntohs(kc->port));

    error = getaddrinfo (kc->host, portstr, &hints, &ai);
    if (error) {
	warnx ("%s: %s", kc->host, gai_strerror(error));
	return -1;
    }
    
    for (a = ai; a != NULL; a = a->ai_next) {
	s = socket (a->ai_family, a->ai_socktype, a->ai_protocol);
	if (s < 0)
	    continue;
	if (connect (s, a->ai_addr, a->ai_addrlen) < 0) {
	    warn ("connect(%s)", kc->host);
	    close (s);
	    continue;
	}
	break;
    }

    if (a == NULL) {
	freeaddrinfo (ai);
	return -1;
    }

    addrlen = a->ai_addrlen;
    if (getsockname (s, thisaddr, &addrlen) < 0 ||
	addrlen != a->ai_addrlen)
	err(1, "getsockname(%s)", kc->host);
    memcpy (&kc->thisaddr, thisaddr, sizeof(kc->thisaddr));
    memcpy (&kc->thataddr, a->ai_addr, sizeof(kc->thataddr));
    freeaddrinfo (ai);
    if ((*kc->authenticate)(kc, s))
	return -1;
    return s;
}

/*
 * Get rid of the cookie that we were sent and get the correct one
 * from our own cookie file instead and then just copy data in both
 * directions.
 */

static int
passive_session (int xserver, int fd, kx_context *kc)
{
    if (replace_cookie (xserver, fd, XauFileName(), 1))
	return 1;
    else
	return copy_encrypted (kc, xserver, fd);
}

static int
active_session (int xserver, int fd, kx_context *kc)
{
    if (verify_and_remove_cookies (xserver, fd, 1))
	return 1;
    else
	return copy_encrypted (kc, xserver, fd);
}

/*
 * fork (unless debugp) and print the output that will be used by the
 * script to capture the display, xauth cookie and pid.
 */

static void
status_output (int debugp)
{
    if(debugp)
	printf ("%u\t%s\t%s\n", (unsigned)getpid(), display, xauthfile);
    else {
	pid_t pid;
	
	pid = fork();
	if (pid < 0) {
	    err(1, "fork");
	} else if (pid > 0) {
	    printf ("%u\t%s\t%s\n", (unsigned)pid, display, xauthfile);
	    exit (0);
	} else {
	    fclose(stdout);
	}
    }
}

/*
 * Obtain an authenticated connection on `kc'.  Send a kx message
 * saying we are `kc->user' and want to use passive mode.  Wait for
 * answer on that connection and fork of a child for every new
 * connection we have to make.
 */

static int
doit_passive (kx_context *kc)
{
     int otherside;
     u_char msg[1024], *p;
     int len;
     u_int32_t tmp;
     const char *host = kc->host;

     otherside = connect_host (kc);

     if (otherside < 0)
	 return 1;
#if defined(SO_KEEPALIVE) && defined(HAVE_SETSOCKOPT)
     if (kc->keepalive_flag) {
	 int one = 1;

	 setsockopt (otherside, SOL_SOCKET, SO_KEEPALIVE, (void *)&one,
		     sizeof(one));
     }
#endif

     p = msg;
     *p++ = INIT;
     len = strlen(kc->user);
     p += KRB_PUT_INT (len, p, sizeof(msg) - 1, 4);
     memcpy(p, kc->user, len);
     p += len;
     *p++ = PASSIVE | (kc->keepalive_flag ? KEEP_ALIVE : 0);
     if (kx_write (kc, otherside, msg, p - msg) != p - msg)
	 err (1, "write to %s", host);
     len = kx_read (kc, otherside, msg, sizeof(msg));
     if (len <= 0)
	 errx (1,
	       "error reading initial message from %s: "
	       "this probably means it's using an old version.",
	       host);
     p = (u_char *)msg;
     if (*p == ERROR) {
	 p++;
	 p += krb_get_int (p, &tmp, 4, 0);
	 errx (1, "%s: %.*s", host, (int)tmp, p);
     } else if (*p != ACK) {
	 errx (1, "%s: strange msg %d", host, *p);
     } else
	 p++;
     p += krb_get_int (p, &tmp, 4, 0);
     memcpy(display, p, tmp);
     display[tmp] = '\0';
     p += tmp;

     p += krb_get_int (p, &tmp, 4, 0);
     memcpy(xauthfile, p, tmp);
     xauthfile[tmp] = '\0';
     p += tmp;

     status_output (kc->debug_flag);
     for (;;) {
	 pid_t child;

	 len = kx_read (kc, otherside, msg, sizeof(msg));
	 if (len < 0)
	     err (1, "read from %s", host);
	 else if (len == 0)
	     return 0;

	 p = (u_char *)msg;
	 if (*p == ERROR) {
	     p++;
	     p += krb_get_int (p, &tmp, 4, 0);
	     errx (1, "%s: %.*s", host, (int)tmp, p);
	 } else if(*p != NEW_CONN) {
	     errx (1, "%s: strange msg %d", host, *p);
	 } else {
	     p++;
	     p += krb_get_int (p, &tmp, 4, 0);
	 }
	 
	 ++nchild;
	 child = fork ();
	 if (child < 0) {
	     warn("fork");
	     continue;
	 } else if (child == 0) {
	     struct sockaddr_in addr;
	     int fd;
	     int xserver;

	     addr = kc->thataddr;
	     close (otherside);

	     addr.sin_port = htons(tmp);
	     fd = socket (AF_INET, SOCK_STREAM, 0);
	     if (fd < 0)
		 err(1, "socket");
#if defined(TCP_NODELAY) && defined(HAVE_SETSOCKOPT)
	     {
		 int one = 1;

		 setsockopt (fd, IPPROTO_TCP, TCP_NODELAY, (void *)&one,
			     sizeof(one));
	     }
#endif
#if defined(SO_KEEPALIVE) && defined(HAVE_SETSOCKOPT)
	     if (kc->keepalive_flag) {
		 int one = 1;

		 setsockopt (fd, SOL_SOCKET, SO_KEEPALIVE, (void *)&one,
			     sizeof(one));
	     }
#endif

	     if (connect (fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
		 err(1, "connect(%s)", host);
	     {
		 int d = 0;
		 char *s;

		 s = getenv ("DISPLAY");
		 if (s != NULL) {
		     s = strchr (s, ':');
		     if (s != NULL)
			 d = atoi (s + 1);
		 }

		 xserver = connect_local_xsocket (d);
		 if (xserver < 0)
		     return 1;
	     }
	     return passive_session (xserver, fd, kc);
	 } else {
	 }
     }
}

/*
 * Allocate a local pseudo-xserver and wait for connections 
 */

static int
doit_active (kx_context *kc)
{
    int otherside;
    int nsockets;
    struct x_socket *sockets;
    u_char msg[1024], *p;
    int len = strlen(kc->user);
    int tmp, tmp2;
    char *s;
    int i;
    size_t rem;
    u_int32_t other_port;
    int error;
    const char *host = kc->host;

    otherside = connect_host (kc);
    if (otherside < 0)
	return 1;
#if defined(SO_KEEPALIVE) && defined(HAVE_SETSOCKOPT)
    if (kc->keepalive_flag) {
	int one = 1;

	setsockopt (otherside, SOL_SOCKET, SO_KEEPALIVE, (void *)&one,
		    sizeof(one));
    }
#endif
    p = msg;
    rem = sizeof(msg);
    *p++ = INIT;
    --rem;
    len = strlen(kc->user);
    tmp = KRB_PUT_INT (len, p, rem, 4);
    if (tmp < 0)
	return 1;
    p += tmp;
    rem -= tmp;
    memcpy(p, kc->user, len);
    p += len;
    rem -= len;
    *p++ = (kc->keepalive_flag ? KEEP_ALIVE : 0);
    --rem;

    s = getenv("DISPLAY");
    if (s == NULL || (s = strchr(s, ':')) == NULL) 
	s = ":0";
    len = strlen (s);
    tmp = KRB_PUT_INT (len, p, rem, 4);
    if (tmp < 0)
	return 1;
    rem -= tmp;
    p += tmp;
    memcpy (p, s, len);
    p += len;
    rem -= len;

    s = getenv("XAUTHORITY");
    if (s == NULL)
	s = "";
    len = strlen (s);
    tmp = KRB_PUT_INT (len, p, rem, 4);
    if (tmp < 0)
	return 1;
    p += len;
    rem -= len;
    memcpy (p, s, len);
    p += len;
    rem -= len;

    if (kx_write (kc, otherside, msg, p - msg) != p - msg)
	err (1, "write to %s", host);

    len = kx_read (kc, otherside, msg, sizeof(msg));
    if (len < 0)
	err (1, "read from %s", host);
    p = (u_char *)msg;
    if (*p == ERROR) {
	u_int32_t u32;

	p++;
	p += krb_get_int (p, &u32, 4, 0);
	errx (1, "%s: %.*s", host, (int)u32, p);
    } else if (*p != ACK) {
	errx (1, "%s: strange msg %d", host, *p);
    } else
	p++;

    tmp2 = get_xsockets (&nsockets, &sockets, kc->tcp_flag);
    if (tmp2 < 0)
	return 1;
    display_num = tmp2;
    if (kc->tcp_flag)
	snprintf (display, display_size, "localhost:%u", display_num);
    else
	snprintf (display, display_size, ":%u", display_num);
    error = create_and_write_cookie (xauthfile, xauthfile_size,
				     cookie, cookie_len);
    if (error) {
	warnx ("failed creating cookie file: %s", strerror(error));
	return 1;
    }
    status_output (kc->debug_flag);
    for (;;) {
	fd_set fdset;
	pid_t child;
	int fd, thisfd = -1;
	socklen_t zero = 0;

	FD_ZERO(&fdset);
	for (i = 0; i < nsockets; ++i) {
	    if (sockets[i].fd >= FD_SETSIZE) 
		errx (1, "fd too large");
	    FD_SET(sockets[i].fd, &fdset);
	}
	if (select(FD_SETSIZE, &fdset, NULL, NULL, NULL) <= 0)
	    continue;
	for (i = 0; i < nsockets; ++i)
	    if (FD_ISSET(sockets[i].fd, &fdset)) {
		thisfd = sockets[i].fd;
		break;
	    }
	fd = accept (thisfd, NULL, &zero);
	if (fd < 0) {
	    if (errno == EINTR)
		continue;
	    else
		err(1, "accept");
	}

	p = msg;
	*p++ = NEW_CONN;
	if (kx_write (kc, otherside, msg, p - msg) != p - msg)
	    err (1, "write to %s", host);
	len = kx_read (kc, otherside, msg, sizeof(msg));
	if (len < 0)
	    err (1, "read from %s", host);
	p = (u_char *)msg;
	if (*p == ERROR) {
	    u_int32_t val;

	    p++;
	    p += krb_get_int (p, &val, 4, 0);
	    errx (1, "%s: %.*s", host, (int)val, p);
	} else if (*p != NEW_CONN) {
	    errx (1, "%s: strange msg %d", host, *p);
	} else {
	    p++;
	    p += krb_get_int (p, &other_port, 4, 0);
	}

	++nchild;
	child = fork ();
	if (child < 0) {
	    warn("fork");
	    continue;
	} else if (child == 0) {
	    int s;
	    struct sockaddr_in addr;

	    for (i = 0; i < nsockets; ++i)
		close (sockets[i].fd);

	    addr = kc->thataddr;
	    close (otherside);

	    addr.sin_port = htons(other_port);
	    s = socket (AF_INET, SOCK_STREAM, 0);
	    if (s < 0)
		err(1, "socket");
#if defined(TCP_NODELAY) && defined(HAVE_SETSOCKOPT)
	    {
		int one = 1;

		setsockopt (s, IPPROTO_TCP, TCP_NODELAY, (void *)&one,
			    sizeof(one));
	    }
#endif
#if defined(SO_KEEPALIVE) && defined(HAVE_SETSOCKOPT)
	    if (kc->keepalive_flag) {
		int one = 1;

		setsockopt (s, SOL_SOCKET, SO_KEEPALIVE, (void *)&one,
			    sizeof(one));
	    }
#endif

	    if (connect (s, (struct sockaddr *)&addr, sizeof(addr)) < 0)
		err(1, "connect");

	    return active_session (fd, s, kc);
	} else {
	    close (fd);
	}
    }
}

/*
 * Should we interpret `disp' as this being a passive call?
 */

static int
check_for_passive (const char *disp)
{
    char local_hostname[MaxHostNameLen];

    gethostname (local_hostname, sizeof(local_hostname));

    return disp != NULL &&
	(*disp == ':'
	 || strncmp(disp, "unix", 4) == 0
	 || strncmp(disp, "localhost", 9) == 0
	 || strncmp(disp, local_hostname, strlen(local_hostname)) == 0);
}

/*
 * Set up signal handlers and then call the functions.
 */

static int
doit (kx_context *kc, int passive_flag)
{
    signal (SIGCHLD, childhandler);
    signal (SIGUSR1, usr1handler);
    signal (SIGUSR2, usr2handler);
    if (passive_flag)
	return doit_passive (kc);
    else
	return doit_active  (kc);
}

#ifdef KRB4

/*
 * Start a v4-authenticatated kx connection.
 */

static int
doit_v4 (const char *host, int port, const char *user, 
	 int passive_flag, int debug_flag, int keepalive_flag, int tcp_flag)
{
    int ret;
    kx_context context;

    krb4_make_context (&context);
    context_set (&context,
		 host, user, port, debug_flag, keepalive_flag, tcp_flag);

    ret = doit (&context, passive_flag);
    context_destroy (&context);
    return ret;
}
#endif /* KRB4 */

#ifdef KRB5

/*
 * Start a v5-authenticatated kx connection.
 */

static int
doit_v5 (const char *host, int port, const char *user,
	 int passive_flag, int debug_flag, int keepalive_flag, int tcp_flag)
{
    int ret;
    kx_context context;

    krb5_make_context (&context);
    context_set (&context,
		 host, user, port, debug_flag, keepalive_flag, tcp_flag);

    ret = doit (&context, passive_flag);
    context_destroy (&context);
    return ret;
}
#endif /* KRB5 */

/*
 * Variables set from the arguments
 */

#ifdef KRB4
static int use_v4		= -1;
#ifdef HAVE_KRB_ENABLE_DEBUG
static int krb_debug_flag	= 0;
#endif /* HAVE_KRB_ENABLE_DEBUG */
#endif /* KRB4 */
#ifdef KRB5
static int use_v5		= -1;
#endif
static char *port_str		= NULL;
static const char *user		= NULL;
static int tcp_flag		= 0;
static int passive_flag		= 0;
static int keepalive_flag	= 1;
static int debug_flag		= 0;
static int version_flag		= 0;
static int help_flag		= 0;

struct getargs args[] = {
#ifdef KRB4
    { "krb4",	'4', arg_flag,		&use_v4,	"Use Kerberos V4",
      NULL },
#ifdef HAVE_KRB_ENABLE_DEBUG
    { "krb4-debug", 'D', arg_flag,	&krb_debug_flag,
      "enable krb4 debugging" },
#endif /* HAVE_KRB_ENABLE_DEBUG */
#endif /* KRB4 */
#ifdef KRB5
    { "krb5",	'5', arg_flag,		&use_v5,	"Use Kerberos V5",
      NULL },
#endif
    { "port",	'p', arg_string,	&port_str,	"Use this port",
      "number-of-service" },
    { "user",	'l', arg_string,	&user,		"Run as this user",
      NULL },
    { "tcp",	't', arg_flag,		&tcp_flag,
      "Use a TCP connection for X11" },
    { "passive", 'P', arg_flag,		&passive_flag,
      "Force a passive connection" },
    { "keepalive", 'k', arg_negative_flag, &keepalive_flag,
      "disable keep-alives" },
    { "debug",	'd',	arg_flag,	&debug_flag,
      "Enable debug information" },
    { "version", 0,  arg_flag,		&version_flag,	"Print version",
      NULL },
    { "help",	 0,  arg_flag,		&help_flag,	NULL,
      NULL }
};

static void
usage(int ret)
{
    arg_printusage (args,
		    sizeof(args) / sizeof(args[0]),
		    NULL,
		    "host");
    exit (ret);
}

/*
 * kx - forward an x-connection over a kerberos-encrypted channel.
 */

int
main(int argc, char **argv)
{
    int port	= 0;
    int optind	= 0;
    int ret	= 1;
    char *host	= NULL;

    setprogname (argv[0]);

    if (getarg (args, sizeof(args) / sizeof(args[0]), argc, argv,
		&optind))
	usage (1);

    if (help_flag)
	usage (0);

    if (version_flag) {
	print_version (NULL);
	return 0;
    }

    if (optind != argc - 1)
	usage (1);

    host = argv[optind];

    if (port_str) {
	struct servent *s = roken_getservbyname (port_str, "tcp");

	if (s)
	    port = s->s_port;
	else {
	    char *ptr;

	    port = strtol (port_str, &ptr, 10);
	    if (port == 0 && ptr == port_str)
		errx (1, "Bad port `%s'", port_str);
	    port = htons(port);
	}
    }

    if (user == NULL) {
	user = get_default_username ();
	if (user == NULL)
	    errx (1, "who are you?");
    }

    if (!passive_flag)
	passive_flag = check_for_passive (getenv("DISPLAY"));

#if defined(HAVE_KERNEL_ENABLE_DEBUG)
    if (krb_debug_flag)
	krb_enable_debug ();
#endif

#if defined(KRB4) && defined(KRB5)
    if(use_v4 == -1 && use_v5 == 1)
	use_v4 = 0;
    if(use_v5 == -1 && use_v4 == 1)
	use_v5 = 0;
#endif    

#ifdef KRB5
    if (ret && use_v5) {
	if (port == 0)
	    port = krb5_getportbyname(NULL, "kx", "tcp", KX_PORT);
	ret = doit_v5 (host, port, user,
		       passive_flag, debug_flag, keepalive_flag, tcp_flag);
    }
#endif
#ifdef KRB4
    if (ret && use_v4) {
	if (port == 0)
	    port = k_getportbyname("kx", "tcp", htons(KX_PORT));
	ret = doit_v4 (host, port, user, 
		       passive_flag, debug_flag, keepalive_flag, tcp_flag);
    }
#endif
    return ret;
}
