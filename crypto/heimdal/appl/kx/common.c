/*
 * Copyright (c) 1995 - 2001 Kungliga Tekniska Högskolan
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

RCSID("$Id: common.c,v 1.65 2001/08/26 01:40:38 assar Exp $");

char x_socket[MaxPathLen];

u_int32_t display_num;
char display[MaxPathLen];
int display_size = sizeof(display);
char xauthfile[MaxPathLen];
int xauthfile_size = sizeof(xauthfile);
u_char cookie[16];
size_t cookie_len = sizeof(cookie);

#ifndef X_UNIX_PATH
#define X_UNIX_PATH "/tmp/.X11-unix/X"
#endif

#ifndef X_PIPE_PATH
#define X_PIPE_PATH "/tmp/.X11-pipe/X"
#endif

/*
 * Allocate a unix domain socket in `s' for display `dpy' and with
 * filename `pattern'
 *
 * 0 if all is OK
 * -1 if bind failed badly
 * 1 if dpy is already used */

static int
try_socket (struct x_socket *s, int dpy, const char *pattern)
{
    struct sockaddr_un addr;
    int fd;

    fd = socket (AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
	err (1, "socket AF_UNIX");
    memset (&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf (addr.sun_path, sizeof(addr.sun_path), pattern, dpy);
    if(bind(fd,
	    (struct sockaddr *)&addr,
	    sizeof(addr)) < 0) {
	close (fd);
	if (errno == EADDRINUSE ||
	    errno == EACCES  /* Cray return EACCESS */
#ifdef ENOTUNIQ
	    || errno == ENOTUNIQ /* bug in Solaris 2.4 */
#endif
	    )
	    return 1;
	else
	    return -1;
    }
    s->fd = fd;
    s->pathname = strdup (addr.sun_path);
    if (s->pathname == NULL)
	errx (1, "strdup: out of memory");
    s->flags = UNIX_SOCKET;
    return 0;
}

#ifdef MAY_HAVE_X11_PIPES
/*
 * Allocate a stream (masqueraded as a named pipe)
 *
 * 0 if all is OK
 * -1 if bind failed badly
 * 1 if dpy is already used
 */

static int
try_pipe (struct x_socket *s, int dpy, const char *pattern)
{
    char path[MAXPATHLEN];
    int ret;
    int fd;
    int pipefd[2];
    
    snprintf (path, sizeof(path), pattern, dpy);
    fd = open (path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0) {
	if (errno == EEXIST)
	    return 1;
	else
	    return -1;
    }

    close (fd);

    ret = pipe (pipefd);
    if (ret < 0)
	err (1, "pipe");

    ret = ioctl (pipefd[1], I_PUSH, "connld");
    if (ret < 0) {
	if(errno == ENOSYS)
	    return -1;
	err (1, "ioctl I_PUSH");
    }

    ret = fattach (pipefd[1], path);
    if (ret < 0)
	err (1, "fattach %s", path);

    s->fd  = pipefd[0];
    close (pipefd[1]);
    s->pathname = strdup (path);
    if (s->pathname == NULL)
	errx (1, "strdup: out of memory");
    s->flags = STREAM_PIPE;
    return 0;
}
#endif /* MAY_HAVE_X11_PIPES */

/*
 * Try to create a TCP socket in `s' corresponding to display `dpy'.
 *
 * 0 if all is OK
 * -1 if bind failed badly
 * 1 if dpy is already used
 */

static int
try_tcp (struct x_socket *s, int dpy)
{
    struct sockaddr_in tcpaddr;
    struct in_addr local;
    int one = 1;
    int fd;

    memset(&local, 0, sizeof(local));
    local.s_addr = htonl(INADDR_LOOPBACK);

    fd = socket (AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
	err (1, "socket AF_INET");
#if defined(TCP_NODELAY) && defined(HAVE_SETSOCKOPT)
    setsockopt (fd, IPPROTO_TCP, TCP_NODELAY, (void *)&one,
		sizeof(one));
#endif
    memset (&tcpaddr, 0, sizeof(tcpaddr));
    tcpaddr.sin_family = AF_INET;
    tcpaddr.sin_addr = local;
    tcpaddr.sin_port = htons(6000 + dpy);
    if (bind (fd, (struct sockaddr *)&tcpaddr,
	      sizeof(tcpaddr)) < 0) {
	close (fd);
	if (errno == EADDRINUSE)
	    return 1;
	else
	    return -1;
    }
    s->fd = fd;
    s->pathname = NULL;
    s->flags = TCP;
    return 0;
}

/*
 * The potential places to create unix sockets.
 */

static char *x_sockets[] = {
X_UNIX_PATH "%u",
"/var/X/.X11-unix/X" "%u",
"/usr/spool/sockets/X11/" "%u",
NULL
};

/*
 * Dito for stream pipes.
 */

#ifdef MAY_HAVE_X11_PIPES
static char *x_pipes[] = {
X_PIPE_PATH "%u",
"/var/X/.X11-pipe/X" "%u",
NULL
};
#endif

/*
 * Create the directory corresponding to dirname of `path' or fail.
 */

static void
try_mkdir (const char *path)
{
    char *dir;
    char *p;
    int oldmask;

    if((dir = strdup (path)) == NULL)
	errx (1, "strdup: out of memory");
    p = strrchr (dir, '/');
    if (p)
	*p = '\0';

    oldmask = umask(0);
    mkdir (dir, 01777);
    umask (oldmask);
    free (dir);
}

/*
 * Allocate a display, returning the number of sockets in `number' and
 * all the corresponding sockets in `sockets'.  If `tcp_socket' is
 * true, also allcoaet a TCP socket.
 *
 * The return value is the display allocated or -1 if an error occurred.
 */

int
get_xsockets (int *number, struct x_socket **sockets, int tcp_socket)
{
     int dpy;
     struct x_socket *s;
     int n;
     int i;

     s = malloc (sizeof(*s) * 5);
     if (s == NULL)
	 errx (1, "malloc: out of memory");

     try_mkdir (X_UNIX_PATH);
     try_mkdir (X_PIPE_PATH);

     for(dpy = 4; dpy < 256; ++dpy) {
	 char **path;
	 int tmp = 0;

	 n = 0;
	 for (path = x_sockets; *path; ++path) {
	     tmp = try_socket (&s[n], dpy, *path);
	     if (tmp == -1) {
		 if (errno != ENOTDIR && errno != ENOENT)
		     return -1;
	     } else if (tmp == 1) {
		 while(--n >= 0) {
		     close (s[n].fd);
		     free (s[n].pathname);
		 }
		 break;
	     } else if (tmp == 0)
		 ++n;
	 }
	 if (tmp == 1)
	     continue;

#ifdef MAY_HAVE_X11_PIPES
	 for (path = x_pipes; *path; ++path) {
	     tmp = try_pipe (&s[n], dpy, *path);
	     if (tmp == -1) {
		 if (errno != ENOTDIR && errno != ENOENT && errno != ENOSYS)
		     return -1;
	     } else if (tmp == 1) {
		 while (--n >= 0) {
		     close (s[n].fd);
		     free (s[n].pathname);
		 }
		 break;
	     } else if (tmp == 0)
		 ++n;
	 }

	 if (tmp == 1)
	     continue;
#endif

	 if (tcp_socket) {
	     tmp = try_tcp (&s[n], dpy);
	     if (tmp == -1)
		 return -1;
	     else if (tmp == 1) {
		 while (--n >= 0) {
		     close (s[n].fd);
		     free (s[n].pathname);
		 }
		 break;
	     } else if (tmp == 0)
		 ++n;
	 }
	 break;
     }
     if (dpy == 256)
	 errx (1, "no free x-servers");
     for (i = 0; i < n; ++i)
	 if (s[i].flags & LISTENP
	     && listen (s[i].fd, SOMAXCONN) < 0)
	     err (1, "listen %s", s[i].pathname ? s[i].pathname : "tcp");
     *number = n;
     *sockets = s;
     return dpy;
}

/*
 * Change owner on the `n' sockets in `sockets' to `uid', `gid'.
 * Return 0 is succesful or -1 if an error occurred.
 */

int
chown_xsockets (int n, struct x_socket *sockets, uid_t uid, gid_t gid)
{
    int i;

    for (i = 0; i < n; ++i)
	if (sockets[i].pathname != NULL)
	    if (chown (sockets[i].pathname, uid, gid) < 0)
		return -1;
    return 0;
}

/*
 * Connect to local display `dnr' with local transport or TCP.
 * Return a file descriptor.
 */

int
connect_local_xsocket (unsigned dnr)
{
     int fd;
     char **path;

     for (path = x_sockets; *path; ++path) {
	 struct sockaddr_un addr;

	 fd = socket (AF_UNIX, SOCK_STREAM, 0);
	 if (fd < 0)
	     break;
	 memset (&addr, 0, sizeof(addr));
	 addr.sun_family = AF_UNIX;
	 snprintf (addr.sun_path, sizeof(addr.sun_path), *path, dnr);
	 if (connect (fd, (struct sockaddr *)&addr, sizeof(addr)) == 0)
	     return fd;
	 close(fd);
     }
     {
	 struct sockaddr_in addr;

	 fd = socket(AF_INET, SOCK_STREAM, 0);
	 if (fd < 0)
	     err (1, "socket AF_INET");
	 memset (&addr, 0, sizeof(addr));
	 addr.sin_family = AF_INET;
	 addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	 addr.sin_port = htons(6000 + dnr);
	 if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0)
	     return fd;
	 close(fd);
     }
     err (1, "connecting to local display %u", dnr);
}

/*
 * Create a cookie file with a random cookie for the localhost.  The
 * file name will be stored in `xauthfile' (but not larger than
 * `xauthfile_size'), and the cookie returned in `cookie', `cookie_sz'.
 * Return 0 if succesful, or errno.
 */

int
create_and_write_cookie (char *xauthfile,
			 size_t xauthfile_size,
			 u_char *cookie,
			 size_t cookie_sz)
{
     Xauth auth;
     char tmp[64];
     int fd;
     FILE *f;
     char hostname[MaxHostNameLen];
     struct in_addr loopback;
     int saved_errno;

     gethostname (hostname, sizeof(hostname));
     loopback.s_addr = htonl(INADDR_LOOPBACK);
     
     auth.family = FamilyLocal;
     auth.address = hostname;
     auth.address_length = strlen(auth.address);
     snprintf (tmp, sizeof(tmp), "%d", display_num);
     auth.number_length = strlen(tmp);
     auth.number = tmp;
     auth.name = COOKIE_TYPE;
     auth.name_length = strlen(auth.name);
     auth.data_length = cookie_sz;
     auth.data = (char*)cookie;
#ifdef KRB5
     krb5_generate_random_block (cookie, cookie_sz);
#else
     krb_generate_random_block (cookie, cookie_sz);
#endif

     strlcpy(xauthfile, "/tmp/AXXXXXX", xauthfile_size);
     fd = mkstemp(xauthfile);
     if(fd < 0) {
	 saved_errno = errno;
	 syslog(LOG_ERR, "create_and_write_cookie: mkstemp: %m");
         return saved_errno;
     }
     f = fdopen(fd, "r+");
     if(f == NULL){
	 saved_errno = errno;
	 close(fd);
	 return errno;
     }
     if(XauWriteAuth(f, &auth) == 0) {
	 saved_errno = errno;
	 fclose(f);
	 return saved_errno;
     }

     /*
      * I would like to write a cookie for localhost:n here, but some
      * stupid code in libX11 will not look for cookies of that type,
      * so we are forced to use FamilyWild instead.
      */

     auth.family  = FamilyWild;
     auth.address_length = 0;

#if 0 /* XXX */
     auth.address = (char *)&loopback;
     auth.address_length = sizeof(loopback);
#endif

     if (XauWriteAuth(f, &auth) == 0) {
	 saved_errno = errno;
	 fclose (f);
	 return saved_errno;
     }

     if(fclose(f))
	 return errno;
     return 0;
}

/*
 * Verify and remove cookies.  Read and parse a X-connection from
 * `fd'. Check the cookie used is the same as in `cookie'.  Remove the
 * cookie and copy the rest of it to `sock'.
 * Expect cookies iff cookiesp.
 * Return 0 iff ok.
 *
 * The protocol is as follows:
 *
 * C->S:	[Bl]				1
 *		unused				1
 *		protocol major version		2
 *		protocol minor version		2
 *		length of auth protocol name(n)	2
 *		length of auth protocol data	2
 *		unused				2
 *		authorization protocol name	n
 *		pad				pad(n)
 *		authorization protocol data	d
 *		pad				pad(d)
 *
 * S->C:	Failed
 *		0				1
 *		length of reason		1
 *		protocol major version		2
 *		protocol minor version		2
 *		length in 4 bytes unit of
 *		additional data (n+p)/4		2
 *		reason				n
 *		unused				p = pad(n)
 */

int
verify_and_remove_cookies (int fd, int sock, int cookiesp)
{
     u_char beg[12];
     int bigendianp;
     unsigned n, d, npad, dpad;
     char *protocol_name, *protocol_data;
     u_char zeros[6] = {0, 0, 0, 0, 0, 0};
     u_char refused[20] = {0, 10, 
			   0, 0, /* protocol major version  */
			   0, 0, /* protocol minor version */
			   0, 0, /* length of additional data / 4 */
			   'b', 'a', 'd', ' ', 'c', 'o', 'o', 'k', 'i', 'e',
			   0, 0};

     if (net_read (fd, beg, sizeof(beg)) != sizeof(beg))
	  return 1;
     if (net_write (sock, beg, 6) != 6)
	  return 1;
     bigendianp = beg[0] == 'B';
     if (bigendianp) {
	  n = (beg[6] << 8) | beg[7];
	  d = (beg[8] << 8) | beg[9];
     } else {
	  n = (beg[7] << 8) | beg[6];
	  d = (beg[9] << 8) | beg[8];
     }
     npad = (4 - (n % 4)) % 4;
     dpad = (4 - (d % 4)) % 4;
     protocol_name = malloc(n + npad);
     if (n + npad != 0 && protocol_name == NULL)
	 return 1;
     protocol_data = malloc(d + dpad);
     if (d + dpad != 0 && protocol_data == NULL) {
	 free (protocol_name);
	 return 1;
     }
     if (net_read (fd, protocol_name, n + npad) != n + npad)
	 goto fail;
     if (net_read (fd, protocol_data, d + dpad) != d + dpad)
	 goto fail;
     if (cookiesp) {
	 if (strncmp (protocol_name, COOKIE_TYPE, strlen(COOKIE_TYPE)) != 0)
	     goto refused;
	 if (d != cookie_len ||
	     memcmp (protocol_data, cookie, cookie_len) != 0)
	     goto refused;
     }
     free (protocol_name);
     free (protocol_data);
     if (net_write (sock, zeros, 6) != 6)
	  return 1;
     return 0;
refused:
     refused[2] = beg[2];
     refused[3] = beg[3];
     refused[4] = beg[4];
     refused[5] = beg[5];
     if (bigendianp)
	 refused[7] = 3;
     else
	 refused[6] = 3;

     net_write (fd, refused, sizeof(refused));
fail:
     free (protocol_name);
     free (protocol_data);
     return 1;
}

/* 
 * Return 0 iff `cookie' is compatible with the cookie for the
 * localhost with name given in `ai' (or `hostname') and display
 * number in `disp_nr'.
 */

static int
match_local_auth (Xauth* auth,
		  struct addrinfo *ai, const char *hostname, int disp_nr)
{
    int auth_disp;
    char *tmp_disp;
    struct addrinfo *a;
    
    tmp_disp = strndup (auth->number, auth->number_length);
    if (tmp_disp == NULL)
	return -1;
    auth_disp = atoi(tmp_disp);
    free (tmp_disp);
    if (auth_disp != disp_nr)
	return 1;
    for (a = ai; a != NULL; a = a->ai_next) {
	if ((auth->family == FamilyLocal
	     || auth->family == FamilyWild)
	    && a->ai_canonname != NULL
	    && strncmp (auth->address,
			a->ai_canonname,
			auth->address_length) == 0)
	    return 0;
    }
    if (hostname != NULL
	&& (auth->family    == FamilyLocal
	    || auth->family == FamilyWild)
	&& strncmp (auth->address, hostname, auth->address_length) == 0)
	return 0;
    return 1;
}

/*
 * Find `our' cookie from the cookie file `f' and return it or NULL.
 */

static Xauth*
find_auth_cookie (FILE *f)
{
    Xauth *ret = NULL;
    char local_hostname[MaxHostNameLen];
    char *display = getenv("DISPLAY");
    char d[MaxHostNameLen + 4];
    char *colon;
    struct addrinfo *ai;
    struct addrinfo hints;
    int disp;
    int error;

    if(display == NULL)
	display = ":0";
    strlcpy(d, display, sizeof(d));
    display = d;
    colon = strchr (display, ':');
    if (colon == NULL)
	disp = 0;
    else {
	*colon = '\0';
	disp = atoi (colon + 1);
    }
    if (strcmp (display, "") == 0
	|| strncmp (display, "unix", 4) == 0
	|| strncmp (display, "localhost", 9) == 0) {
	gethostname (local_hostname, sizeof(local_hostname));
	display = local_hostname;
    }
    memset (&hints, 0, sizeof(hints));
    hints.ai_flags    = AI_CANONNAME;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    error = getaddrinfo (display, NULL, &hints, &ai);
    if (error)
	ai = NULL;

    for (; (ret = XauReadAuth (f)) != NULL; XauDisposeAuth(ret)) {
	if (match_local_auth (ret, ai, display, disp) == 0) {
	    if (ai != NULL)
		freeaddrinfo (ai);
	    return ret;
	}
    }
    if (ai != NULL)
	freeaddrinfo (ai);
    return NULL;
}

/*
 * Get rid of the cookie that we were sent and get the correct one
 * from our own cookie file instead.
 */

int
replace_cookie(int xserver, int fd, char *filename, int cookiesp) /* XXX */
{
     u_char beg[12];
     int bigendianp;
     unsigned n, d, npad, dpad;
     FILE *f;
     u_char zeros[6] = {0, 0, 0, 0, 0, 0};

     if (net_read (fd, beg, sizeof(beg)) != sizeof(beg))
	  return 1;
     if (net_write (xserver, beg, 6) != 6)
	  return 1;
     bigendianp = beg[0] == 'B';
     if (bigendianp) {
	  n = (beg[6] << 8) | beg[7];
	  d = (beg[8] << 8) | beg[9];
     } else {
	  n = (beg[7] << 8) | beg[6];
	  d = (beg[9] << 8) | beg[8];
     }
     if (n != 0 || d != 0)
	  return 1;
     f = fopen(filename, "r");
     if (f != NULL) {
	 Xauth *auth = find_auth_cookie (f);
	 u_char len[6] = {0, 0, 0, 0, 0, 0};
	 
	 fclose (f);

	 if (auth != NULL) {
	     n = auth->name_length;
	     d = auth->data_length;
	 } else {
	     n = 0;
	     d = 0;
	 }
	 if (bigendianp) {
	     len[0] = n >> 8;
	     len[1] = n & 0xFF;
	     len[2] = d >> 8;
	     len[3] = d & 0xFF;
	 } else {
	     len[0] = n & 0xFF;
	     len[1] = n >> 8;
	     len[2] = d & 0xFF;
	     len[3] = d >> 8;
	 }
	 if (net_write (xserver, len, 6) != 6) {
	     XauDisposeAuth(auth);
	     return 1;
	 }
	 if(n != 0 && net_write (xserver, auth->name, n) != n) {
	     XauDisposeAuth(auth);
	     return 1;
	 }
	 npad = (4 - (n % 4)) % 4;
	 if (npad && net_write (xserver, zeros, npad) != npad) {
	     XauDisposeAuth(auth);
	     return 1;
	 }
	 if (d != 0 && net_write (xserver, auth->data, d) != d) {
	     XauDisposeAuth(auth);
	     return 1;
	 }
	 XauDisposeAuth(auth);
	 dpad = (4 - (d % 4)) % 4;
	 if (dpad && net_write (xserver, zeros, dpad) != dpad)
	     return 1;
     } else {
	 if(net_write(xserver, zeros, 6) != 6)
	     return 1;
     }
     return 0;
}

/*
 * Some simple controls on the address and corresponding socket
 */

int
suspicious_address (int sock, struct sockaddr_in addr)
{
    char data[40];
    socklen_t len = sizeof(data);

    return addr.sin_addr.s_addr != htonl(INADDR_LOOPBACK)
#if defined(IP_OPTIONS) && defined(HAVE_GETSOCKOPT)
	|| getsockopt (sock, IPPROTO_IP, IP_OPTIONS, data, &len) < 0
	|| len != 0
#endif
    ;
}

/*
 * This really sucks, but these functions are used and if we're not
 * linking against libkrb they don't exist.  Using the heimdal storage
 * functions will not work either cause we do not always link with
 * libkrb5 either.
 */

#ifndef KRB4

int
krb_get_int(void *f, u_int32_t *to, int size, int lsb)
{
    int i;
    unsigned char *from = (unsigned char *)f;

    *to = 0;
    if(lsb){
	for(i = size-1; i >= 0; i--)
	    *to = (*to << 8) | from[i];
    }else{
	for(i = 0; i < size; i++)
	    *to = (*to << 8) | from[i];
    }
    return size;
}

int
krb_put_int(u_int32_t from, void *to, size_t rem, int size)
{
    int i;
    unsigned char *p = (unsigned char *)to;

    if (rem < size)
	return -1;

    for(i = size - 1; i >= 0; i--){
	p[i] = from & 0xff;
	from >>= 8;
    }
    return size;
}

#endif /* !KRB4 */
