/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: ftp.c,v 1.4 1995/05/24 11:19:10 gpalmer Exp $
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include "ftp.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifndef STANDALONE_FTP
#include "sysinstall.h"
#endif /*STANDALONE_FTP*/

static void
debug(FTP_t ftp, const char *fmt, ...)
{
    char p[BUFSIZ];
    va_list ap;
    va_start(ap, fmt);
    (void) vsnprintf(p, sizeof p, fmt, ap);
    va_end(ap);
    
#ifdef STANDALONE_FTP
    write(ftp->fd_debug,p,strlen(p));
#else
    msgDebug(p);
#endif
}

static int
writes(int fd, char *s)
{
    int i = strlen(s);
    if (i != write(fd,s,i))
	return errno ? errno : -1;
    return 0;
}

static char*
get_a_line(FTP_t ftp)
{
    static char buf[BUFSIZ];
    int i,j;
    
    for(i=0;i<BUFSIZ;) {
	j = read(ftp->fd_ctrl,buf+i,1);
	if (j != 1)
	    return 0;
	if (buf[i] == '\r' || buf[i] == '\n') {
	    if (!i)
		continue;
	    buf[i] = '\0';
	    debug(ftp, "LIBFTP: received <%s>\n",buf);
	    return buf;
	}
	i++;
    }
    return buf;
}

static int
get_a_number(FTP_t ftp, char **q)
{
    char *p;
    int i = -1,j;
    
    while(1) {
	p = get_a_line(ftp);
	if (!(isdigit(p[0]) && isdigit(p[1]) && isdigit(p[2])))
	    continue;
	if (i == -1 && p[3] == '-') {
	    i = atoi(p);
	    continue;
	} 
	if (p[3] != ' ' && p[3] != '\t') 
	    continue;
	j = atoi(p);
	if (i == -1) {
	    if (q) *q = p+4;
	    return j;
	} else if (j == i) {
	    if (q) *q = p+4;
	    return j;
	}
    }
}

static int
botch(FTP_t ftp, char *func, char *state)
{
    debug(ftp, "LIBFTP: Botch: %s called outside state %s\n",func,state);
    writes(ftp->fd_ctrl,"QUIT\r\n");
    close(ftp->fd_ctrl); ftp->fd_ctrl = -1;
    close(ftp->fd_xfer); ftp->fd_xfer = -1;
    ftp->state = init;
    return -1;
}

static int
cmd(FTP_t ftp, const char *fmt, ...)
{
    char p[BUFSIZ];
    int i;
    
    va_list ap;
    va_start(ap, fmt);
    (void) vsnprintf(p, sizeof p, fmt, ap);
    va_end(ap);
    
    debug(ftp, "LIBFTP: send <%s>\n",p);
    strcat(p,"\r\n");
    if (writes(ftp->fd_ctrl,p))
	return -1;
    i = get_a_number(ftp,0);
    debug(ftp, "LIBFTP: got %d\n",i);
    return i;
}

FTP_t
FtpInit()
{
    FTP_t ftp;
    
    ftp = malloc(sizeof *ftp);
    if (!ftp)
	return ftp;
    memset(ftp, 0, sizeof *ftp);
    ftp->fd_ctrl = -1;
    ftp->fd_debug = -1;
    ftp->state = init;
    return ftp;
}

#ifdef STANDALONE_FTP
void
FtpDebug(FTP_t ftp, int i)
{
    ftp->fd_debug = i;
}
#endif

int
FtpOpen(FTP_t ftp, char *host, char *user, char *passwd)
{
    struct hostent	*he = NULL;
    struct sockaddr_in 	sin;
    int 			s;
    unsigned long 		temp;
    int i;
    
    if (ftp->state != init)
	return botch(ftp,"FtpOpen","init");
    
    if (!user)
	user = "ftp";
    
    if (!passwd)
	passwd = "??@??(FreeBSD:libftp)";	/* XXX */
    
    debug(ftp, "FtpOpen(ftp, %s, %s, %s)\n", host, user, passwd);
    
    temp = inet_addr(host);
    if (temp != INADDR_NONE)
    {
	debug(ftp, "Using dotted IP address `%s'\n", host);
	ftp->addrtype = sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = temp;
    } else {
	debug(ftp, "Trying to resolve `%s'\n", host);
	he = gethostbyname(host);
	if (!he)
	{
	    debug(ftp, "Lookup of `%s' failed!\n", host);
	    return ENOENT;
	}
	ftp->addrtype = sin.sin_family = he->h_addrtype;
	bcopy(he->h_addr, (char *)&sin.sin_addr, he->h_length);
    }
    
    sin.sin_port = htons(21);
    
    if ((s = socket(ftp->addrtype, SOCK_STREAM, 0)) < 0)
    {
	debug(ftp, "Socket open failed: %s (%i)\n", strerror(errno), errno);
	return s;
    }
    
    if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
	debug(ftp,"Connection failed: %s (%i)\n", strerror(errno), errno);
	(void)close(s);
	return -1;
    }
    
    ftp->fd_ctrl = s;
    
    debug(ftp, "LIBFTP: open (%d)\n",get_a_number(ftp,0));
    
    i = cmd(ftp,"USER %s",user);
    i = cmd(ftp,"PASS %s",passwd);
    ftp->state = isopen;
    return 0;
    
 fail:
    close(ftp->fd_ctrl);
    ftp->fd_ctrl = -1;
    return -1;
}

void
FtpClose(FTP_t ftp)
{
    if (ftp->state != isopen) 
	botch(ftp,"FtpClose","open");
    
    writes(ftp->fd_ctrl,"QUIT\r\n");
    close(ftp->fd_ctrl); ftp->fd_ctrl = -1;
    close(ftp->fd_xfer); ftp->fd_xfer = -1;
    ftp->state = init;
}

int
FtpChdir(FTP_t ftp, char *dir)
{
    int i;
    if (ftp->state != isopen) 
	return botch(ftp,"FtpChdir","open");
    i = cmd(ftp,"CWD %s",dir);
    return 0;
}

int
FtpGet(FTP_t ftp, char *file)
{
    int i,s;
    char *q;
    unsigned char addr[6];
    struct sockaddr_in sin;
    
    if (ftp->state != isopen) 
	return botch(ftp,"FtpGet","open");
    if(ftp->binary) {
	i = cmd(ftp,"TYPE I");
	if (i > 299)
	    return -1;
    } else {
	return -1;
    }
    if (ftp->passive) {
	debug(ftp, "LIBFTP: send <%s>\n","PASV");
	if (writes(ftp->fd_ctrl,"PASV\r\n"))
	    return -1;
	i = get_a_number(ftp,&q);
	if (i != 227)
	    return -1;
	while (*q && !isdigit(*q))
	    q++;
	if (!*q)
	    return -1;
	q--;
	for(i=0;i<6;i++) {
	    q++;
	    addr[i] = strtol(q,&q,10);
	}
	
	sin.sin_family = ftp->addrtype;
	bcopy(addr, (char *)&sin.sin_addr, 4);
	bcopy(addr+4, (char *)&sin.sin_port, 2);
	debug(ftp, "Opening active socket to %s : %u\n", inet_ntoa(sin.sin_addr), htons(sin.sin_port));

	if ((s = socket(ftp->addrtype, SOCK_STREAM, 0)) < 0) 
	    return -1;

	debug(ftp, "Connecting to %s:%u\n", inet_ntoa(sin.sin_addr), htons(sin.sin_port));
	if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
	    (void)close(s);
	    debug(ftp, "connect: %s (%d)\n", strerror(errno), errno);
	    return -1;
	}
	
	i = cmd(ftp,"RETR %s",file);
	if (i > 299)
	    return -1;
	ftp->state = xfer;
	return s;
    } else {
	return -1;
    }
}

int
FtpEOF(FTP_t ftp)
{
    if (ftp->state != xfer) 
	return botch(ftp,"FtpEOF","xfer");
    ftp->state = isopen;
    return get_a_number(ftp,0);
}

#ifdef STANDALONE_FTP

/* main.c */

int 
main(int argc, char **argv)
{
    FTP_t ftp;
    int i;
    char c;
    
    ftp = FtpInit();
    if (!ftp) err(1,"FtpInit()");
    
    FtpDebug(ftp,1);
    i = FtpOpen(ftp, "ref.tfs.com", "ftp", "phk-libftp@");
    if (i) err(1,"FtpOpen(%d)",i);
    FtpBinary(ftp,1);
    FtpPassive(ftp,1);
    FtpChdir(ftp,"/");
    FtpChdir(ftp,"CTM");
    i = FtpGet(ftp,"README");
    while(1 == read(i,&c,1))
	putchar(c);
    FtpEOF(ftp);
    FtpClose(ftp);
    i = FtpOpen(ftp, "freefall.cdrom.com", "ftp", "phk-libftp@");
    FtpBinary(ftp,1);
    FtpPassive(ftp,1);
    FtpChdir(ftp,"/pub");
    FtpChdir(ftp,"FreeBSD");
    i = FtpGet(ftp,"README");
    while(1 == read(i,&c,1))
	putchar(c);
    FtpEOF(ftp);
    return 0;
}

#endif /*STANDALONE_FTP*/
