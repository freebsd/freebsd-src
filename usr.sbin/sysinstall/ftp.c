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
#include "ftp.h"
#include "sysinstall.h"

static void
debug(FTP_t ftp, const char *fmt, ...)
{
	char p[BUFSIZ];
	va_list ap;
	va_start(ap, fmt);
	(void) vsnprintf(p, sizeof p, fmt, ap);
	va_end(ap);

#if 0
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
			debug(ftp,"LIBFTP: received <%s>\n",buf);
			return buf;
		}
		i++;
	}
}

int
get_a_number(FTP_t ftp)
{
	char *p;

	while(1) {
		p = get_a_line(ftp);
		if (p[3] != ' ' && p[3] != '	')
			continue;
		return atoi(p);
	}
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
	debug(ftp,"LIBFTP: send <%s>\n",p);
	if (writes(ftp->fd_ctrl,p))
		return -1;
	if (writes(ftp->fd_ctrl,"\r\n"))
		return -1;
	i = get_a_number(ftp);
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
	return ftp;
}
#if 0
void
FtpDebug(FTP_t ftp, int i)
{
	ftp->fd_debug = i;
}
#endif
int
FtpOpen(FTP_t ftp, char *host, char *user, char *passwd)
{

	struct hostent 		*he, hdef;
	struct servent 		*se, sdef;
	struct sockaddr_in 	sin;
	int 			s;
	char 			a,*p,buf[BUFSIZ];
	unsigned long 		temp;

	if (!user)
		user = "ftp";

	if (!passwd)
		passwd = "??@??(FreeBSD:libftp)";	/* XXX */

	msgDebug("FtpOpen(ftp, %s, %s, %s)\n", host, user, passwd);

	temp = inet_addr(host);
	if (temp != INADDR_NONE)
	{
	    msgDebug("Using dotted IP address `%s'\n", host);
	    ftp->addrtype = sin.sin_family = AF_INET;
	    sin.sin_addr.s_addr = temp;
	} else {
	    msgDebug("Trying to resolve `%s'\n", host);
	    he = gethostbyname(host);
	    if (!he)
	    {
		msgDebug("Lookup of `%s' failed!\n", host);
		return ENOENT;
	    }
	    ftp->addrtype = sin.sin_family = he->h_addrtype;
	    bcopy(he->h_addr, (char *)&sin.sin_addr, he->h_length);
	}

        sin.sin_port = htons(21);

        if ((s = socket(he->h_addrtype, SOCK_STREAM, 0)) < 0) 
                return s;

        if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
                (void)close(s);
                return -1;
        }

	ftp->fd_ctrl = s;

	debug(ftp, "LIBFTP: open (%d)\n",get_a_number(ftp));

	cmd(ftp,"USER %s",user);
	cmd(ftp,"PASS %s",passwd);
	return 0;

    fail:
	close(ftp->fd_ctrl);
	ftp->fd_ctrl = -1;
	return -1;
}

int
FtpChdir(FTP_t ftp, char *dir)
{
	cmd(ftp,"CWD %s",dir);
	return 0;
}

int
FtpGet(FTP_t ftp, char *file)
{
	int fd,i,j,s;
	char p[BUFSIZ],*q;
	unsigned char addr[6];
	struct sockaddr_in sin;

	if(ftp->binary) {
		cmd(ftp,"TYPE I");
	} else {
		return -1;
	}
	if(ftp->passive) {
		if (writes(ftp->fd_ctrl,"PASV\r\n"))
			return -1;
		q = get_a_line(ftp);
		if (strncmp(q,"227 ",4))
			return -1;
		q = strchr(q,'(');
		if (!q)
			return -1;
		for(i=0;i<6;i++) {
			q++;
			addr[i] = strtol(q,&q,10);
			debug(ftp,"ADDR[%d] = %d (%c)\n",i,addr[i],*q);
		}
		if (*q != ')')
			return -1;

		sin.sin_family = ftp->addrtype;
		bcopy(addr, (char *)&sin.sin_addr, 4);
		bcopy(addr+4, (char *)&sin.sin_port, 2);
		if ((s = socket(ftp->addrtype, SOCK_STREAM, 0)) < 0) 
			return -1;
		debug(ftp,"Getsocket = %d\n",s);

		if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
			(void)close(s);
			debug(ftp,"connect, errno = %d\n",errno);
			return -1;
		}
		cmd(ftp,"RETR %s",file);
		return s;
	} else {
		return -1;
	}
}

int
FtpEOF(FTP_t ftp)
{
	get_a_number(ftp);
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
	i = FtpOpen(ftp, "ref", "ftp", "phk-libftp@");
	if (i) err(1,"FtpOpen(%d)",i);
	FtpBinary(ftp,1);
	FtpPassive(ftp,1);
	FtpChdir(ftp,"/pub");
	FtpChdir(ftp,"CTM");
	i = FtpGet(ftp,"README_CTM_MOVED");
	while(1 == read(i,&c,1))
		putchar(c);
	FtpEOF(ftp);
	return 0;
}

#endif /*STANDALONE_FTP*/
