/*
 * Copyright (c) 1995, 1996, 1997, 1998, 1999 Kungliga Tekniska Högskolan
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

RCSID("$Id: kxd.c,v 1.69 2001/02/20 01:44:45 assar Exp $");

static pid_t wait_on_pid = -1;
static int   done        = 0;

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
       if (pid > 0 && pid == wait_on_pid)
	   done = 1;
     } while(pid > 0);
     signal (SIGCHLD, childhandler);
     SIGRETURN(0);
}

/*
 * Print the error message `format' and `...' on fd and die.
 */

void
fatal (kx_context *kc, int fd, char *format, ...)
{
    u_char msg[1024];
    u_char *p;
    va_list args;
    int len;

    va_start(args, format);
    p = msg;
    *p++ = ERROR;
    vsnprintf ((char *)p + 4, sizeof(msg) - 5, format, args);
    syslog (LOG_ERR, "%s", (char *)p + 4);
    len = strlen ((char *)p + 4);
    p += KRB_PUT_INT (len, p, 4, 4);
    p += len;
    kx_write (kc, fd, msg, p - msg);
    va_end(args);
    exit (1);
}

/*
 * Remove all sockets and cookie files.
 */

static void
cleanup(int nsockets, struct x_socket *sockets)
{
    int i;

    if(xauthfile[0])
	unlink(xauthfile);
    for (i = 0; i < nsockets; ++i) {
	if (sockets[i].pathname != NULL) {
	    unlink (sockets[i].pathname);
	    free (sockets[i].pathname);
	}
    }
}

/*
 * Prepare to receive a connection on `sock'.
 */

static int
recv_conn (int sock, kx_context *kc,
	   int *dispnr, int *nsockets, struct x_socket **sockets,
	   int tcp_flag)
{
     u_char msg[1024], *p;
     char user[256];
     socklen_t addrlen;
     struct passwd *passwd;
     struct sockaddr_in thisaddr, thataddr;
     char remotehost[MaxHostNameLen];
     char remoteaddr[INET6_ADDRSTRLEN];
     int ret = 1;
     int flags;
     int len;
     u_int32_t tmp32;

     addrlen = sizeof(thisaddr);
     if (getsockname (sock, (struct sockaddr *)&thisaddr, &addrlen) < 0 ||
	 addrlen != sizeof(thisaddr)) {
	 syslog (LOG_ERR, "getsockname: %m");
	 exit (1);
     }
     addrlen = sizeof(thataddr);
     if (getpeername (sock, (struct sockaddr *)&thataddr, &addrlen) < 0 ||
	 addrlen != sizeof(thataddr)) {
	 syslog (LOG_ERR, "getpeername: %m");
	 exit (1);
     }

     kc->thisaddr = thisaddr;
     kc->thataddr = thataddr;

     getnameinfo_verified ((struct sockaddr *)&thataddr, addrlen,
			   remotehost, sizeof(remotehost),
			   NULL, 0, 0);

     if (net_read (sock, msg, 4) != 4) {
	 syslog (LOG_ERR, "read: %m");
	 exit (1);
     }

#ifdef KRB5
     if (ret && recv_v5_auth (kc, sock, msg) == 0)
	 ret = 0;
#endif
#ifdef KRB4
     if (ret && recv_v4_auth (kc, sock, msg) == 0)
	 ret = 0;
#endif
     if (ret) {
	 syslog (LOG_ERR, "unrecognized auth protocol: %x %x %x %x",
		 msg[0], msg[1], msg[2], msg[3]);
	 exit (1);
     }

     len = kx_read (kc, sock, msg, sizeof(msg));
     if (len < 0) {
	 syslog (LOG_ERR, "kx_read failed");
	 exit (1);
     }
     p = (u_char *)msg;
     if (*p != INIT)
	 fatal(kc, sock, "Bad message");
     p++;
     p += krb_get_int (p, &tmp32, 4, 0);
     len = min(sizeof(user), tmp32);
     memcpy (user, p, len);
     p += tmp32;
     user[len] = '\0';

     passwd = k_getpwnam (user);
     if (passwd == NULL)
	 fatal (kc, sock, "cannot find uid for %s", user);

     if (context_userok (kc, user) != 0)
	 fatal (kc, sock, "%s not allowed to login as %s",
		kc->user, user);

     flags = *p++;

     if (flags & PASSIVE) {
	 pid_t pid;
	 int tmp;

	 tmp = get_xsockets (nsockets, sockets, tcp_flag);
	 if (tmp < 0) {
	     fatal (kc, sock, "Cannot create X socket(s): %s",
		    strerror(errno));
	 }
	 *dispnr = tmp;

	 if (chown_xsockets (*nsockets, *sockets,
			    passwd->pw_uid, passwd->pw_gid)) {
	     cleanup (*nsockets, *sockets);
	     fatal (kc, sock, "Cannot chown sockets: %s",
		    strerror(errno));
	 }

	 pid = fork();
	 if (pid == -1) {
	     cleanup (*nsockets, *sockets);
	     fatal (kc, sock, "fork: %s", strerror(errno));
	 } else if (pid != 0) {
	     wait_on_pid = pid;
	     while (!done)
		 pause ();
	     cleanup (*nsockets, *sockets);
	     exit (0);
	 }
     }

     if (setgid (passwd->pw_gid) ||
	 initgroups(passwd->pw_name, passwd->pw_gid) ||
#ifdef HAVE_GETUDBNAM /* XXX this happens on crays */
	 setjob(passwd->pw_uid, 0) == -1 ||
#endif
	 setuid(passwd->pw_uid)) {
	 syslog(LOG_ERR, "setting uid/groups: %m");
	 fatal (kc, sock, "cannot set uid");
     }
     inet_ntop (thataddr.sin_family,
		&thataddr.sin_addr, remoteaddr, sizeof(remoteaddr));

     syslog (LOG_INFO, "from %s(%s): %s -> %s",
	     remotehost, remoteaddr,
	     kc->user, user);
     umask(077);
     if (!(flags & PASSIVE)) {
	 p += krb_get_int (p, &tmp32, 4, 0);
	 len = min(tmp32, display_size);
	 memcpy (display, p, len);
	 display[len] = '\0';
	 p += tmp32;
	 p += krb_get_int (p, &tmp32, 4, 0);
	 len = min(tmp32, xauthfile_size);
	 memcpy (xauthfile, p, len);
	 xauthfile[len] = '\0';
	 p += tmp32;
     }
#if defined(SO_KEEPALIVE) && defined(HAVE_SETSOCKOPT)
     if (flags & KEEP_ALIVE) {
	 int one = 1;

	 setsockopt (sock, SOL_SOCKET, SO_KEEPALIVE, (void *)&one,
		     sizeof(one));
     }
#endif
     return flags;
}

/*
 *
 */

static int
passive_session (kx_context *kc, int fd, int sock, int cookiesp)
{
    if (verify_and_remove_cookies (fd, sock, cookiesp))
	return 1;
    else
	return copy_encrypted (kc, fd, sock);
}

/*
 *
 */

static int
active_session (kx_context *kc, int fd, int sock, int cookiesp)
{
    fd = connect_local_xsocket(0);

    if (replace_cookie (fd, sock, xauthfile, cookiesp))
	return 1;
    else
	return copy_encrypted (kc, fd, sock);
}

/*
 * Handle a new connection.
 */

static int
doit_conn (kx_context *kc,
	   int fd, int meta_sock, int flags, int cookiesp)
{
    int sock, sock2;
    struct sockaddr_in addr;
    struct sockaddr_in thisaddr;
    socklen_t addrlen;
    u_char msg[1024], *p;

    sock = socket (AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
	syslog (LOG_ERR, "socket: %m");
	return 1;
    }
#if defined(TCP_NODELAY) && defined(HAVE_SETSOCKOPT)
    {
	int one = 1;
	setsockopt (sock, IPPROTO_TCP, TCP_NODELAY, (void *)&one, sizeof(one));
    }
#endif
#if defined(SO_KEEPALIVE) && defined(HAVE_SETSOCKOPT)
     if (flags & KEEP_ALIVE) {
	 int one = 1;

	 setsockopt (sock, SOL_SOCKET, SO_KEEPALIVE, (void *)&one,
		     sizeof(one));
     }
#endif
    memset (&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    if (bind (sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
	syslog (LOG_ERR, "bind: %m");
	return 1;
    }
    addrlen = sizeof(addr);
    if (getsockname (sock, (struct sockaddr *)&addr, &addrlen) < 0) {
	syslog (LOG_ERR, "getsockname: %m");
	return 1;
    }
    if (listen (sock, SOMAXCONN) < 0) {
	syslog (LOG_ERR, "listen: %m");
	return 1;
    }
    p = msg;
    *p++ = NEW_CONN;
    p += KRB_PUT_INT (ntohs(addr.sin_port), p, 4, 4);

    if (kx_write (kc, meta_sock, msg, p - msg) < 0) {
	syslog (LOG_ERR, "write: %m");
	return 1;
    }

    addrlen = sizeof(thisaddr);
    sock2 = accept (sock, (struct sockaddr *)&thisaddr, &addrlen);
    if (sock2 < 0) {
	syslog (LOG_ERR, "accept: %m");
	return 1;
    }
    close (sock);
    close (meta_sock);

    if (flags & PASSIVE)
	return passive_session (kc, fd, sock2, cookiesp);
    else
	return active_session (kc, fd, sock2, cookiesp);
}

/*
 *  Is the current user the owner of the console?
 */

static void
check_user_console (kx_context *kc, int fd)
{
     struct stat sb;

     if (stat ("/dev/console", &sb) < 0)
	 fatal (kc, fd, "Cannot stat /dev/console: %s", strerror(errno));
     if (getuid() != sb.st_uid)
	 fatal (kc, fd, "Permission denied");
}

/* close down the new connection with a reasonable error message */
static void
close_connection(int fd, const char *message)
{
    char buf[264]; /* max message */
    char *p;
    int lsb = 0;
    size_t mlen;

    mlen = strlen(message);
    if(mlen > 255)
	mlen = 255;
    
    /* read first part of connection packet, to get byte order */
    if(read(fd, buf, 6) != 6) {
	close(fd);
	return;
    }
    if(buf[0] == 0x6c)
	lsb++;
    p = buf;
    *p++ = 0;				/* failed */
    *p++ = mlen;			/* length of message */
    p += 4;				/* skip protocol version */
    p += 2;				/* skip additional length */
    memcpy(p, message, mlen);		/* copy message */
    p += mlen;
    while((p - buf) % 4)		/* pad to multiple of 4 bytes */
	*p++ = 0;
	
    /* now fill in length of additional data */
    if(lsb) { 
	buf[6] = (p - buf - 8) / 4;
	buf[7] = 0;
    }else{
	buf[6] = 0;
	buf[7] = (p - buf - 8) / 4;
    }
    write(fd, buf, p - buf);
    close(fd);
}


/*
 * Handle a passive session on `sock'
 */

static int
doit_passive (kx_context *kc,
	      int sock,
	      int flags,
	      int dispnr,
	      int nsockets,
	      struct x_socket *sockets,
	      int tcp_flag)
{
    int tmp;
    int len;
    size_t rem;
    u_char msg[1024], *p;
    int error;

    display_num = dispnr;
    if (tcp_flag)
	snprintf (display, display_size, "localhost:%u", display_num);
    else
	snprintf (display, display_size, ":%u", display_num);
    error = create_and_write_cookie (xauthfile, xauthfile_size, 
				     cookie, cookie_len);
    if (error) {
	cleanup(nsockets, sockets);
	fatal (kc, sock, "Cookie-creation failed: %s", strerror(error));
	return 1;
    }

    p = msg;
    rem = sizeof(msg);
    *p++ = ACK;
    --rem;

    len = strlen (display);
    tmp = KRB_PUT_INT (len, p, rem, 4);
    if (tmp < 0 || rem < len + 4) {
	syslog (LOG_ERR, "doit: buffer too small");
	cleanup(nsockets, sockets);
	return 1;
    }
    p += tmp;
    rem -= tmp;

    memcpy (p, display, len);
    p += len;
    rem -= len;

    len = strlen (xauthfile);
    tmp = KRB_PUT_INT (len, p, rem, 4);
    if (tmp < 0 || rem < len + 4) {
	syslog (LOG_ERR, "doit: buffer too small");
	cleanup(nsockets, sockets);
	return 1;
    }
    p += tmp;
    rem -= tmp;

    memcpy (p, xauthfile, len);
    p += len;
    rem -= len;
	  
    if(kx_write (kc, sock, msg, p - msg) < 0) {
	syslog (LOG_ERR, "write: %m");
	cleanup(nsockets, sockets);
	return 1;
    }
    for (;;) {
	pid_t child;
	int fd = -1;
	fd_set fds;
	int i;
	int ret;
	int cookiesp = TRUE;
	       
	FD_ZERO(&fds);
	if (sock >= FD_SETSIZE) {
	    syslog (LOG_ERR, "fd too large");
	    cleanup(nsockets, sockets);
	    return 1;
	}

	FD_SET(sock, &fds);
	for (i = 0; i < nsockets; ++i) {
	    if (sockets[i].fd >= FD_SETSIZE) {
		syslog (LOG_ERR, "fd too large");
		cleanup(nsockets, sockets);
		return 1;
	    }
	    FD_SET(sockets[i].fd, &fds);
	}
	ret = select(FD_SETSIZE, &fds, NULL, NULL, NULL);
	if(ret <= 0)
	    continue;
	if(FD_ISSET(sock, &fds)){
	    /* there are no processes left on the remote side
	     */
	    cleanup(nsockets, sockets);
	    exit(0);
	} else if(ret) {
	    for (i = 0; i < nsockets; ++i) {
		if (FD_ISSET(sockets[i].fd, &fds)) {
		    if (sockets[i].flags == TCP) {
			struct sockaddr_in peer;
			socklen_t len = sizeof(peer);

			fd = accept (sockets[i].fd,
				     (struct sockaddr *)&peer,
				     &len);
			if (fd < 0 && errno != EINTR)
			    syslog (LOG_ERR, "accept: %m");

			/* XXX */
			if (fd >= 0 && suspicious_address (fd, peer)) {
			    close (fd);
			    fd = -1;
			    errno = EINTR;
			}
		    } else if(sockets[i].flags == UNIX_SOCKET) {
			socklen_t zero = 0;

			fd = accept (sockets[i].fd, NULL, &zero);

			if (fd < 0 && errno != EINTR)
			    syslog (LOG_ERR, "accept: %m");
#ifdef MAY_HAVE_X11_PIPES
		    } else if(sockets[i].flags == STREAM_PIPE) {
			/*
			 * this code tries to handle the
			 * send fd-over-pipe stuff for
			 * solaris
			 */

			struct strrecvfd strrecvfd;

			ret = ioctl (sockets[i].fd,
				     I_RECVFD, &strrecvfd);
			if (ret < 0 && errno != EINTR) {
			    syslog (LOG_ERR, "ioctl I_RECVFD: %m");
			}

			/* XXX */
			if (ret == 0) {
			    if (strrecvfd.uid != getuid()) {
				close (strrecvfd.fd);
				fd = -1;
				errno = EINTR;
			    } else {
				fd = strrecvfd.fd;
				cookiesp = FALSE;
			    }
			}
#endif /* MAY_HAVE_X11_PIPES */
		    } else
			abort ();
		    break;
		}
	    }
	}
	if (fd < 0) {
	    if (errno == EINTR)
		continue;
	    else
		return 1;
	}

	child = fork ();
	if (child < 0) {
	    syslog (LOG_ERR, "fork: %m");
	    if(errno != EAGAIN)
		return 1;
	    close_connection(fd, strerror(errno));
	} else if (child == 0) {
	    for (i = 0; i < nsockets; ++i)
		close (sockets[i].fd);
	    return doit_conn (kc, fd, sock, flags, cookiesp);
	} else {
	    close (fd);
	}
    }
}

/*
 * Handle an active session on `sock'
 */

static int
doit_active (kx_context *kc,
	     int sock,
	     int flags,
	     int tcp_flag)
{
    u_char msg[1024], *p;

    check_user_console (kc, sock);

    p = msg;
    *p++ = ACK;
	  
    if(kx_write (kc, sock, msg, p - msg) < 0) {
	syslog (LOG_ERR, "write: %m");
	return 1;
    }
    for (;;) {
	pid_t child;
	int len;
	      
	len = kx_read (kc, sock, msg, sizeof(msg));
	if (len < 0) {
	    syslog (LOG_ERR, "read: %m");
	    return 1;
	}
	p = (u_char *)msg;
	if (*p != NEW_CONN) {
	    syslog (LOG_ERR, "bad_message: %d", *p);
	    return 1;
	}

	child = fork ();
	if (child < 0) {
	    syslog (LOG_ERR, "fork: %m");
	    if (errno != EAGAIN)
		return 1;
	} else if (child == 0) {
	    return doit_conn (kc, sock, sock, flags, 1);
	} else {
	}
    }
}

/*
 * Receive a connection on `sock' and process it.
 */

static int
doit(int sock, int tcp_flag)
{
    int ret;
    kx_context context;
    int dispnr;
    int nsockets;
    struct x_socket *sockets;
    int flags;

    flags = recv_conn (sock, &context, &dispnr, &nsockets, &sockets, tcp_flag);

    if (flags & PASSIVE)
	ret = doit_passive (&context, sock, flags, dispnr,
			    nsockets, sockets, tcp_flag);
    else
	ret = doit_active (&context, sock, flags, tcp_flag);
    context_destroy (&context);
    return ret;
}

static char *port_str		= NULL;
static int inetd_flag		= 1;
static int tcp_flag		= 0;
static int version_flag		= 0;
static int help_flag		= 0;

struct getargs args[] = {
    { "inetd",		'i',	arg_negative_flag,	&inetd_flag,
      "Not started from inetd" },
    { "tcp",		't',	arg_flag,	&tcp_flag,	"Use TCP" },
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
		    "host");
    exit (ret);
}

/*
 * kxd - receive a forwarded X conncection
 */

int
main (int argc, char **argv)
{
    int port;
    int optind = 0;

    setprogname (argv[0]);
    roken_openlog ("kxd", LOG_ODELAY | LOG_PID, LOG_DAEMON);

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
#if defined(KRB5)
	port = krb5_getportbyname(NULL, "kx", "tcp", KX_PORT);
#elif defined(KRB4)
	port = k_getportbyname ("kx", "tcp", htons(KX_PORT));
#else
#error define KRB4 or KRB5
#endif
    }

    if (!inetd_flag)
	mini_inetd (port);

     signal (SIGCHLD, childhandler);
     return doit(STDIN_FILENO, tcp_flag);
}
