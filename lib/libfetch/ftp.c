/*-
 * Copyright (c) 1998 Dag-Erling Coïdan Smørgrav
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 *
 * $FreeBSD$
 */

/*
 * Portions of this code were taken from or based on ftpio.c:
 *
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * Major Changelog:
 *
 * Dag-Erling Coïdan Smørgrav
 * 9 Jun 1998
 *
 * Incorporated into libfetch
 *
 * Jordan K. Hubbard
 * 17 Jan 1996
 *
 * Turned inside out. Now returns xfers as new file ids, not as a special
 * `state' of FTP_t
 *
 * $ftpioId: ftpio.c,v 1.30 1998/04/11 07:28:53 phk Exp $
 *
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "fetch.h"
#include "common.h"
#include "ftperr.h"

#define FTP_ANONYMOUS_USER	"anonymous"

#define FTP_CONNECTION_ALREADY_OPEN	125
#define FTP_OPEN_DATA_CONNECTION	150
#define FTP_OK				200
#define FTP_FILE_STATUS			213
#define FTP_SERVICE_READY		220
#define FTP_TRANSFER_COMPLETE		226
#define FTP_PASSIVE_MODE		227
#define FTP_LPASSIVE_MODE		228
#define FTP_EPASSIVE_MODE		229
#define FTP_LOGGED_IN			230
#define FTP_FILE_ACTION_OK		250
#define FTP_NEED_PASSWORD		331
#define FTP_NEED_ACCOUNT		332
#define FTP_FILE_OK			350
#define FTP_SYNTAX_ERROR		500
#define FTP_PROTOCOL_ERROR		999

static struct url cached_host;
static int cached_socket;

static char *last_reply;
static size_t lr_size, lr_length;
static int last_code;

#define isftpreply(foo) (isdigit(foo[0]) && isdigit(foo[1]) \
			 && isdigit(foo[2]) \
                         && (foo[3] == ' ' || foo[3] == '\0'))
#define isftpinfo(foo) (isdigit(foo[0]) && isdigit(foo[1]) \
			&& isdigit(foo[2]) && foo[3] == '-')

/* translate IPv4 mapped IPv6 address to IPv4 address */
static void
unmappedaddr(struct sockaddr_in6 *sin6)
{
    struct sockaddr_in *sin4;
    u_int32_t addr;
    int port;

    if (sin6->sin6_family != AF_INET6 ||
	!IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr))
	return;
    sin4 = (struct sockaddr_in *)sin6;
    addr = *(u_int32_t *)&sin6->sin6_addr.s6_addr[12];
    port = sin6->sin6_port;
    memset(sin4, 0, sizeof(struct sockaddr_in));
    sin4->sin_addr.s_addr = addr;
    sin4->sin_port = port;
    sin4->sin_family = AF_INET;
    sin4->sin_len = sizeof(struct sockaddr_in);
}

/*
 * Get server response
 */
static int
_ftp_chkerr(int cd)
{
    if (_fetch_getln(cd, &last_reply, &lr_size, &lr_length) == -1) {
	_fetch_syserr();
	return -1;
    }
    if (isftpinfo(last_reply)) {
	while (lr_length && !isftpreply(last_reply)) {
	    if (_fetch_getln(cd, &last_reply, &lr_size, &lr_length) == -1) {
		_fetch_syserr();
		return -1;
	    }
	}
    }

    while (lr_length && isspace(last_reply[lr_length-1]))
	lr_length--;
    last_reply[lr_length] = 0;
    
    if (!isftpreply(last_reply)) {
	_ftp_seterr(FTP_PROTOCOL_ERROR);
	return -1;
    }

    last_code = (last_reply[0] - '0') * 100
	+ (last_reply[1] - '0') * 10
	+ (last_reply[2] - '0');

    return last_code;
}

/*
 * Send a command and check reply
 */
static int
_ftp_cmd(int cd, char *fmt, ...)
{
    va_list ap;
    size_t len;
    char *msg;
    int r;

    va_start(ap, fmt);
    len = vasprintf(&msg, fmt, ap);
    va_end(ap);
    
    if (msg == NULL) {
	errno = ENOMEM;
	_fetch_syserr();
	return -1;
    }
    
    r = _fetch_putln(cd, msg, len);
    free(msg);
    
    if (r == -1) {
	_fetch_syserr();
	return -1;
    }
    
    return _ftp_chkerr(cd);
}

/*
 * Return a pointer to the filename part of a path
 */
static char *
_ftp_filename(char *file)
{
    char *s;
    
    if ((s = strrchr(file, '/')) == NULL)
	return file;
    else
	return s + 1;
}

/*
 * Change working directory to the directory that contains the
 * specified file.
 */
static int
_ftp_cwd(int cd, char *file)
{
    char *s;
    int e;

    if ((s = strrchr(file, '/')) == NULL || s == file) {
	e = _ftp_cmd(cd, "CWD /");
    } else {
	e = _ftp_cmd(cd, "CWD %.*s", s - file, file);
    }
    if (e != FTP_FILE_ACTION_OK) {
	_ftp_seterr(e);
	return -1;
    }
    return 0;
}

/*
 * Request and parse file stats
 */
static int
_ftp_stat(int cd, char *file, struct url_stat *us)
{
    char *ln, *s;
    struct tm tm;
    time_t t;
    int e;

    us->size = -1;
    us->atime = us->mtime = 0;
    
    if ((s = strrchr(file, '/')) == NULL)
	s = file;
    else
	++s;
    
    if ((e = _ftp_cmd(cd, "SIZE %s", s)) != FTP_FILE_STATUS) {
	_ftp_seterr(e);
	return -1;
    }
    for (ln = last_reply + 4; *ln && isspace(*ln); ln++)
	/* nothing */ ;
    for (us->size = 0; *ln && isdigit(*ln); ln++)
	us->size = us->size * 10 + *ln - '0';
    if (*ln && !isspace(*ln)) {
	_ftp_seterr(FTP_PROTOCOL_ERROR);
	us->size = -1;
	return -1;
    }
    if (us->size == 0)
	us->size = -1;
    DEBUG(fprintf(stderr, "size: [\033[1m%lld\033[m]\n", us->size));

    if ((e = _ftp_cmd(cd, "MDTM %s", s)) != FTP_FILE_STATUS) {
	_ftp_seterr(e);
	return -1;
    }
    for (ln = last_reply + 4; *ln && isspace(*ln); ln++)
	/* nothing */ ;
    switch (strspn(ln, "0123456789")) {
    case 14:
	break;
    case 15:
	ln++;
	ln[0] = '2';
	ln[1] = '0';
	break;
    default:
	_ftp_seterr(FTP_PROTOCOL_ERROR);
	return -1;
    }
    if (sscanf(ln, "%04d%02d%02d%02d%02d%02d",
	       &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
	       &tm.tm_hour, &tm.tm_min, &tm.tm_sec) != 6) {
	_ftp_seterr(FTP_PROTOCOL_ERROR);
	return -1;
    }
    tm.tm_mon--;
    tm.tm_year -= 1900;
    tm.tm_isdst = -1;
    t = timegm(&tm);
    if (t == (time_t)-1)
	t = time(NULL);
    us->mtime = t;
    us->atime = t;
    DEBUG(fprintf(stderr, "last modified: [\033[1m%04d-%02d-%02d "
		  "%02d:%02d:%02d\033[m]\n",
		  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		  tm.tm_hour, tm.tm_min, tm.tm_sec));
    return 0;
}

/*
 * I/O functions for FTP
 */
struct ftpio {
    int		 csd;		/* Control socket descriptor */
    int		 dsd;		/* Data socket descriptor */
    int		 dir;		/* Direction */
    int		 eof;		/* EOF reached */
    int		 err;		/* Error code */
};

static int	_ftp_readfn(void *, char *, int);
static int	_ftp_writefn(void *, const char *, int);
static fpos_t	_ftp_seekfn(void *, fpos_t, int);
static int	_ftp_closefn(void *);

static int
_ftp_readfn(void *v, char *buf, int len)
{
    struct ftpio *io;
    int r;

    io = (struct ftpio *)v;
    if (io == NULL) {
	errno = EBADF;
	return -1;
    }
    if (io->csd == -1 || io->dsd == -1 || io->dir == O_WRONLY) {
	errno = EBADF;
	return -1;
    }
    if (io->err) {
	errno = io->err;
	return -1;
    }
    if (io->eof)
	return 0;
    r = read(io->dsd, buf, len);
    if (r > 0)
	return r;
    if (r == 0) {
	io->eof = 1;
	return _ftp_closefn(v);
    }
    if (errno != EINTR)
	io->err = errno;
    return -1;
}

static int
_ftp_writefn(void *v, const char *buf, int len)
{
    struct ftpio *io;
    int w;
    
    io = (struct ftpio *)v;
    if (io == NULL) {
	errno = EBADF;
	return -1;
    }
    if (io->csd == -1 || io->dsd == -1 || io->dir == O_RDONLY) {
	errno = EBADF;
	return -1;
    }
    if (io->err) {
	errno = io->err;
	return -1;
    }
    w = write(io->dsd, buf, len);
    if (w >= 0)
	return w;
    if (errno != EINTR)
	io->err = errno;
    return -1;
}

static fpos_t
_ftp_seekfn(void *v, fpos_t pos, int whence)
{
    struct ftpio *io;
    
    io = (struct ftpio *)v;
    if (io == NULL) {
	errno = EBADF;
	return -1;
    }
    errno = ESPIPE;
    return -1;
}

static int
_ftp_closefn(void *v)
{
    struct ftpio *io;
    int r;

    io = (struct ftpio *)v;
    if (io == NULL) {
	errno = EBADF;
	return -1;
    }
    if (io->dir == -1)
	return 0;
    if (io->csd == -1 || io->dsd == -1) {
	errno = EBADF;
	return -1;
    }
    close(io->dsd);
    io->dir = -1;
    io->dsd = -1;
    DEBUG(fprintf(stderr, "Waiting for final status\n"));
    if ((r = _ftp_chkerr(io->csd)) != FTP_TRANSFER_COMPLETE)
	io->err = r;
    else
	io->err = 0;
    close(io->csd);
    io->csd = -1;
    return io->err ? -1 : 0;
}

static FILE *
_ftp_setup(int csd, int dsd, int mode)
{
    struct ftpio *io;
    FILE *f;

    if ((io = malloc(sizeof *io)) == NULL)
	return NULL;
    io->csd = dup(csd);
    io->dsd = dsd;
    io->dir = mode;
    io->eof = io->err = 0;
    f = funopen(io, _ftp_readfn, _ftp_writefn, _ftp_seekfn, _ftp_closefn);
    if (f == NULL)
	free(io);
    return f;
}

/*
 * Transfer file
 */
static FILE *
_ftp_transfer(int cd, char *oper, char *file,
	      int mode, off_t offset, char *flags)
{
    struct sockaddr_storage sin;
    struct sockaddr_in6 *sin6;
    struct sockaddr_in *sin4;
    int low, pasv, verbose;
    int e, sd = -1;
    socklen_t l;
    char *s;
    FILE *df;

    /* check flags */
    low = CHECK_FLAG('l');
    pasv = CHECK_FLAG('p');
    verbose = CHECK_FLAG('v');

    /* passive mode */
    if (!pasv)
	pasv = ((s = getenv("FTP_PASSIVE_MODE")) != NULL &&
		strncasecmp(s, "no", 2) != 0);

    /* find our own address, bind, and listen */
    l = sizeof sin;
    if (getsockname(cd, (struct sockaddr *)&sin, &l) == -1)
	goto sysouch;
    if (sin.ss_family == AF_INET6)
	unmappedaddr((struct sockaddr_in6 *)&sin);

    /* open data socket */
    if ((sd = socket(sin.ss_family, SOCK_STREAM, IPPROTO_TCP)) == -1) {
	_fetch_syserr();
	return NULL;
    }
    
    if (pasv) {
	u_char addr[64];
	char *ln, *p;
	int i;
	int port;
	
	/* send PASV command */
	if (verbose)
	    _fetch_info("setting passive mode");
	switch (sin.ss_family) {
	case AF_INET:
	    if ((e = _ftp_cmd(cd, "PASV")) != FTP_PASSIVE_MODE)
		goto ouch;
	    break;
	case AF_INET6:
	    if ((e = _ftp_cmd(cd, "EPSV")) != FTP_EPASSIVE_MODE) {
		if (e == -1)
		    goto ouch;
		if ((e = _ftp_cmd(cd, "LPSV")) != FTP_LPASSIVE_MODE)
		    goto ouch;
	    }
	    break;
	default:
	    e = FTP_PROTOCOL_ERROR; /* XXX: error code should be prepared */
	    goto ouch;
	}

	/*
	 * Find address and port number. The reply to the PASV command
         * is IMHO the one and only weak point in the FTP protocol.
	 */
	ln = last_reply;
      	switch (e) {
	case FTP_PASSIVE_MODE:
	case FTP_LPASSIVE_MODE:
	    for (p = ln + 3; *p && !isdigit(*p); p++)
		/* nothing */ ;
	    if (!*p) {
		e = FTP_PROTOCOL_ERROR;
		goto ouch;
	    }
	    l = (e == FTP_PASSIVE_MODE ? 6 : 21);
	    for (i = 0; *p && i < l; i++, p++)
		addr[i] = strtol(p, &p, 10);
	    if (i < l) {
		e = FTP_PROTOCOL_ERROR;
		goto ouch;
	    }
	    break;
	case FTP_EPASSIVE_MODE:
	    for (p = ln + 3; *p && *p != '('; p++)
		/* nothing */ ;
	    if (!*p) {
		e = FTP_PROTOCOL_ERROR;
		goto ouch;
	    }
	    ++p;
	    if (sscanf(p, "%c%c%c%d%c", &addr[0], &addr[1], &addr[2],
		       &port, &addr[3]) != 5 ||
		addr[0] != addr[1] ||
		addr[0] != addr[2] || addr[0] != addr[3]) {
		e = FTP_PROTOCOL_ERROR;
		goto ouch;
	    }
	    break;
	}

	/* seek to required offset */
	if (offset)
	    if (_ftp_cmd(cd, "REST %lu", (u_long)offset) != FTP_FILE_OK)
		goto sysouch;
	
	/* construct sockaddr for data socket */
	l = sizeof sin;
	if (getpeername(cd, (struct sockaddr *)&sin, &l) == -1)
	    goto sysouch;
	if (sin.ss_family == AF_INET6)
	    unmappedaddr((struct sockaddr_in6 *)&sin);
	switch (sin.ss_family) {
	case AF_INET6:
	    sin6 = (struct sockaddr_in6 *)&sin;
	    if (e == FTP_EPASSIVE_MODE)
		sin6->sin6_port = htons(port);
	    else {
		bcopy(addr + 2, (char *)&sin6->sin6_addr, 16);
		bcopy(addr + 19, (char *)&sin6->sin6_port, 2);
	    }
	    break;
	case AF_INET:
	    sin4 = (struct sockaddr_in *)&sin;
	    if (e == FTP_EPASSIVE_MODE)
		sin4->sin_port = htons(port);
	    else {
		bcopy(addr, (char *)&sin4->sin_addr, 4);
		bcopy(addr + 4, (char *)&sin4->sin_port, 2);
	    }
	    break;
	default:
	    e = FTP_PROTOCOL_ERROR; /* XXX: error code should be prepared */
	    break;
	}

	/* connect to data port */
	if (verbose)
	    _fetch_info("opening data connection");
	if (connect(sd, (struct sockaddr *)&sin, sin.ss_len) == -1)
	    goto sysouch;

	/* make the server initiate the transfer */
	if (verbose)
	    _fetch_info("initiating transfer");
	e = _ftp_cmd(cd, "%s %s", oper, _ftp_filename(file));
	if (e != FTP_CONNECTION_ALREADY_OPEN && e != FTP_OPEN_DATA_CONNECTION)
	    goto ouch;
	
    } else {
	u_int32_t a;
	u_short p;
	int arg, d;
	char *ap;
	char hname[INET6_ADDRSTRLEN];
	
	switch (sin.ss_family) {
	case AF_INET6:
	    ((struct sockaddr_in6 *)&sin)->sin6_port = 0;
#ifdef IPV6_PORTRANGE
	    arg = low ? IPV6_PORTRANGE_DEFAULT : IPV6_PORTRANGE_HIGH;
	    if (setsockopt(sd, IPPROTO_IPV6, IPV6_PORTRANGE,
			   (char *)&arg, sizeof(arg)) == -1)
		goto sysouch;
#endif
	    break;
	case AF_INET:
	    ((struct sockaddr_in *)&sin)->sin_port = 0;
	    arg = low ? IP_PORTRANGE_DEFAULT : IP_PORTRANGE_HIGH;
	    if (setsockopt(sd, IPPROTO_IP, IP_PORTRANGE,
			   (char *)&arg, sizeof arg) == -1)
		goto sysouch;
	    break;
	}
	if (verbose)
	    _fetch_info("binding data socket");
	if (bind(sd, (struct sockaddr *)&sin, sin.ss_len) == -1)
	    goto sysouch;
	if (listen(sd, 1) == -1)
	    goto sysouch;

	/* find what port we're on and tell the server */
	if (getsockname(sd, (struct sockaddr *)&sin, &l) == -1)
	    goto sysouch;
	switch (sin.ss_family) {
	case AF_INET:
	    sin4 = (struct sockaddr_in *)&sin;
	    a = ntohl(sin4->sin_addr.s_addr);
	    p = ntohs(sin4->sin_port);
	    e = _ftp_cmd(cd, "PORT %d,%d,%d,%d,%d,%d",
			 (a >> 24) & 0xff, (a >> 16) & 0xff,
			 (a >> 8) & 0xff, a & 0xff,
			 (p >> 8) & 0xff, p & 0xff);
	    break;
	case AF_INET6:
#define UC(b)	(((int)b)&0xff)
	    e = -1;
	    sin6 = (struct sockaddr_in6 *)&sin;
	    if (getnameinfo((struct sockaddr *)&sin, sin.ss_len,
			    hname, sizeof(hname),
			    NULL, 0, NI_NUMERICHOST) == 0) {
		e = _ftp_cmd(cd, "EPRT |%d|%s|%d|", 2, hname,
			     htons(sin6->sin6_port));
		if (e == -1)
		    goto ouch;
	    }
	    if (e != FTP_OK) {
		ap = (char *)&sin6->sin6_addr;
		e = _ftp_cmd(cd,
     "LPRT %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
			     6, 16,
			     UC(ap[0]), UC(ap[1]), UC(ap[2]), UC(ap[3]),
			     UC(ap[4]), UC(ap[5]), UC(ap[6]), UC(ap[7]),
			     UC(ap[8]), UC(ap[9]), UC(ap[10]), UC(ap[11]),
			     UC(ap[12]), UC(ap[13]), UC(ap[14]), UC(ap[15]),
			     2,
			     (ntohs(sin6->sin6_port) >> 8) & 0xff,
			     ntohs(sin6->sin6_port)        & 0xff);
	    }
	    break;
	default:
	    e = FTP_PROTOCOL_ERROR; /* XXX: error code should be prepared */
	    goto ouch;
	}
	if (e != FTP_OK)
	    goto ouch;

	/* seek to required offset */
	if (offset)
	    if (_ftp_cmd(cd, "REST %lu", (u_long)offset) != FTP_FILE_OK)
		goto sysouch;
	
	/* make the server initiate the transfer */
	if (verbose)
	    _fetch_info("initiating transfer");
	e = _ftp_cmd(cd, "%s %s", oper, _ftp_filename(file));
	if (e != FTP_OPEN_DATA_CONNECTION)
	    goto ouch;
	
	/* accept the incoming connection and go to town */
	if ((d = accept(sd, NULL, NULL)) == -1)
	    goto sysouch;
	close(sd);
	sd = d;
    }

    if ((df = _ftp_setup(cd, sd, mode)) == NULL)
	goto sysouch;
    return df;

sysouch:
    _fetch_syserr();
    if (sd >= 0)
	close(sd);
    return NULL;

ouch:
    if (e != -1)
	_ftp_seterr(e);
    if (sd >= 0)
	close(sd);
    return NULL;
}

/*
 * Log on to FTP server
 */
static int
_ftp_connect(struct url *url, struct url *purl, char *flags)
{
    int cd, e, direct, verbose;
#ifdef INET6
    int af = AF_UNSPEC;
#else
    int af = AF_INET;
#endif
    const char *logname;
    char *user, *pwd;
    char localhost[MAXHOSTNAMELEN];
    char pbuf[MAXHOSTNAMELEN + MAXLOGNAME + 1];

    direct = CHECK_FLAG('d');
    verbose = CHECK_FLAG('v');
    if (CHECK_FLAG('4'))
	af = AF_INET;
    else if (CHECK_FLAG('6'))
	af = AF_INET6;

    if (direct)
	purl = NULL;
    
    /* check for proxy */
    if (purl) {
	/* XXX proxy authentication! */
	cd = _fetch_connect(purl->host, purl->port, af, verbose);
    } else {
	/* no proxy, go straight to target */
	cd = _fetch_connect(url->host, url->port, af, verbose);
	purl = NULL;
    }

    /* check connection */
    if (cd == -1) {
	_fetch_syserr();
	return NULL;
    }

    /* expect welcome message */
    if ((e = _ftp_chkerr(cd)) != FTP_SERVICE_READY)
	goto fouch;

    /* XXX FTP_AUTH, and maybe .netrc */
    
    /* send user name and password */
    user = url->user;
    if (!user || !*user)
	user = getenv("FTP_LOGIN");
    if (!user || !*user)
	user = FTP_ANONYMOUS_USER;
    if (purl && url->port == _fetch_default_port(url->scheme))
	e = _ftp_cmd(cd, "USER %s@%s", user, url->host);
    else if (purl)
	e = _ftp_cmd(cd, "USER %s@%s@%d", user, url->host, url->port);
    else
	e = _ftp_cmd(cd, "USER %s", user);
    
    /* did the server request a password? */
    if (e == FTP_NEED_PASSWORD) {
	pwd = url->pwd;
	if (!pwd || !*pwd)
	    pwd = getenv("FTP_PASSWORD");
	if (!pwd || !*pwd) {
	    if ((logname = getlogin()) == 0)
		logname = FTP_ANONYMOUS_USER;
	    gethostname(localhost, sizeof localhost);
	    snprintf(pbuf, sizeof pbuf, "%s@%s", logname, localhost);
	    pwd = pbuf;
	}
	e = _ftp_cmd(cd, "PASS %s", pwd);
    }

    /* did the server request an account? */
    if (e == FTP_NEED_ACCOUNT)
	goto fouch;
    
    /* we should be done by now */
    if (e != FTP_LOGGED_IN)
	goto fouch;

    /* might as well select mode and type at once */
#ifdef FTP_FORCE_STREAM_MODE
    if ((e = _ftp_cmd(cd, "MODE S")) != FTP_OK) /* default is S */
	goto fouch;
#endif
    if ((e = _ftp_cmd(cd, "TYPE I")) != FTP_OK) /* default is A */
	goto fouch;

    /* done */
    return cd;
    
fouch:
    if (e != -1)
	_ftp_seterr(e);
    close(cd);
    return NULL;
}

/*
 * Disconnect from server
 */
static void
_ftp_disconnect(int cd)
{
    (void)_ftp_cmd(cd, "QUIT");
    close(cd);
}

/*
 * Check if we're already connected
 */
static int
_ftp_isconnected(struct url *url)
{
    return (cached_socket
	    && (strcmp(url->host, cached_host.host) == 0)
	    && (strcmp(url->user, cached_host.user) == 0)
	    && (strcmp(url->pwd, cached_host.pwd) == 0)
	    && (url->port == cached_host.port));
}

/*
 * Check the cache, reconnect if no luck
 */
static int
_ftp_cached_connect(struct url *url, struct url *purl, char *flags)
{
    int e, cd;

    cd = -1;
    
    /* set default port */
    if (!url->port)
	url->port = _fetch_default_port(url->scheme);
    
    /* try to use previously cached connection */
    if (_ftp_isconnected(url)) {
	e = _ftp_cmd(cached_socket, "NOOP");
	if (e == FTP_OK || e == FTP_SYNTAX_ERROR)
	    return cached_socket;
    }

    /* connect to server */
    if ((cd = _ftp_connect(url, purl, flags)) == -1)
	return -1;
    if (cached_socket)
	_ftp_disconnect(cached_socket);
    cached_socket = cd;
    memcpy(&cached_host, url, sizeof *url);
    return cd;
}

/*
 * Check the proxy settings
 */
static struct url *
_ftp_get_proxy(void)
{
    struct url *purl;
    char *p;
    
    if (((p = getenv("FTP_PROXY")) || (p = getenv("ftp_proxy")) ||
	 (p = getenv("HTTP_PROXY")) || (p = getenv("http_proxy"))) &&
	*p && (purl = fetchParseURL(p)) != NULL) {
	if (!*purl->scheme) {
	    if (getenv("FTP_PROXY") || getenv("ftp_proxy"))
		strcpy(purl->scheme, SCHEME_FTP);
	    else
		strcpy(purl->scheme, SCHEME_HTTP);
	}
	if (!purl->port)
	    purl->port = _fetch_default_proxy_port(purl->scheme);
	if (strcasecmp(purl->scheme, SCHEME_FTP) == 0 ||
	    strcasecmp(purl->scheme, SCHEME_HTTP) == 0)
	    return purl;
	fetchFreeURL(purl);
    }
    return NULL;
}

/*
 * Get and stat file
 */
FILE *
fetchXGetFTP(struct url *url, struct url_stat *us, char *flags)
{
    struct url *purl;
    int cd;

    /* get the proxy URL, and check if we should use HTTP instead */
    if (!CHECK_FLAG('d') && (purl = _ftp_get_proxy()) != NULL) {
	if (strcasecmp(purl->scheme, SCHEME_HTTP) == 0)
	    return _http_request(url, "GET", us, purl, flags);
    } else {
	purl = NULL;
    }
    
    /* connect to server */
    cd = _ftp_cached_connect(url, purl, flags);
    if (purl)
	fetchFreeURL(purl);
    if (cd == NULL)
	return NULL;
    
    /* change directory */
    if (_ftp_cwd(cd, url->doc) == -1)
	return NULL;
    
    /* stat file */
    if (us && _ftp_stat(cd, url->doc, us) == -1
	&& fetchLastErrCode != FETCH_PROTO
	&& fetchLastErrCode != FETCH_UNAVAIL)
	return NULL;
    
    /* initiate the transfer */
    return _ftp_transfer(cd, "RETR", url->doc, O_RDONLY, url->offset, flags);
}

/*
 * Get file
 */
FILE *
fetchGetFTP(struct url *url, char *flags)
{
    return fetchXGetFTP(url, NULL, flags);
}

/*
 * Put file
 */
FILE *
fetchPutFTP(struct url *url, char *flags)
{
    struct url *purl;
    int cd;

    /* get the proxy URL, and check if we should use HTTP instead */
    if (!CHECK_FLAG('d') && (purl = _ftp_get_proxy()) != NULL) {
	if (strcasecmp(purl->scheme, SCHEME_HTTP) == 0)
	    /* XXX HTTP PUT is not implemented, so try without the proxy */
	    purl = NULL;
    } else {
	purl = NULL;
    }
    
    /* connect to server */
    cd = _ftp_cached_connect(url, purl, flags);
    if (purl)
	fetchFreeURL(purl);
    if (cd == NULL)
	return NULL;
    
    /* change directory */
    if (_ftp_cwd(cd, url->doc) == -1)
	return NULL;
    
    /* initiate the transfer */
    return _ftp_transfer(cd, CHECK_FLAG('a') ? "APPE" : "STOR",
			 url->doc, O_WRONLY, url->offset, flags);
}

/*
 * Get file stats
 */
int
fetchStatFTP(struct url *url, struct url_stat *us, char *flags)
{
    struct url *purl;
    int cd;

    /* get the proxy URL, and check if we should use HTTP instead */
    if (!CHECK_FLAG('d') && (purl = _ftp_get_proxy()) != NULL) {
	if (strcasecmp(purl->scheme, SCHEME_HTTP) == 0) {
	    FILE *f;

	    if ((f = _http_request(url, "HEAD", us, purl, flags)) == NULL)
		return -1;
	    fclose(f);
	    return 0;
	}
    } else {
	purl = NULL;
    }
    
    /* connect to server */
    cd = _ftp_cached_connect(url, purl, flags);
    if (purl)
	fetchFreeURL(purl);
    if (cd == NULL)
	return NULL;
    
    /* change directory */
    if (_ftp_cwd(cd, url->doc) == -1)
	return -1;

    /* stat file */
    return _ftp_stat(cd, url->doc, us);
}

/*
 * List a directory
 */
extern void warnx(char *, ...);
struct url_ent *
fetchListFTP(struct url *url, char *flags)
{
    warnx("fetchListFTP(): not implemented");
    return NULL;
}
