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

#include "bsd_locl.h"

#ifndef HAVE_FORKPTY

RCSID("$Id: forkpty.c,v 1.52 1997/05/25 07:37:01 assar Exp $");

/* Only CRAY is known to have problems with forkpty(). */
#if defined(CRAY)
static int forkpty_ok = 0;
#else
static int forkpty_ok = 1;
#endif

#ifndef HAVE_PTSNAME
static char *ptsname(int fd)
{
#ifdef HAVE_TTYNAME
  return ttyname(fd);
#else
  return NULL;
#endif
}
#endif

#ifndef HAVE_GRANTPT
#define grantpt(fdm) (0)
#endif

#ifndef HAVE_UNLOCKPT
#define unlockpt(fdm) (0)
#endif

#ifndef HAVE_VHANGUP
#define vhangup() (0)
#endif

#ifndef HAVE_REVOKE
static
void
revoke(char *line)
{
    int slave;
    RETSIGTYPE (*ofun)();

    if ( (slave = open(line, O_RDWR)) < 0)
	return;
    
    ofun = signal(SIGHUP, SIG_IGN);
    vhangup();
    signal(SIGHUP, ofun);
    /*
     * Some systems (atleast SunOS4) want to have the slave end open
     * at all times to prevent a race in the child. Login will close
     * it so it should really not be a problem. However for the
     * paranoid we use the close on exec flag so it will only be open
     * in the parent. Additionally since this will be the controlling
     * tty of rlogind the final vhangup() in rlogind should hangup all
     * processes. A working revoke would of course have been prefered
     * though (sigh).
     */
    fcntl(slave, F_SETFD, 1);
    /* close(slave); */
}
#endif


static int pty_major, pty_minor;

static void
pty_scan_start(void)
{
    pty_major = -1;
    pty_minor = 0;
}

static char *bsd_1 = "0123456789abcdefghijklmnopqrstuv";
/* there are many more */
static char *bsd_2 = "pqrstuvwxyzabcdefghijklmnoABCDEFGHIJKLMNOPQRSTUVWXYZ";

static int
pty_scan_next(char *buf, size_t sz)
{
#ifdef CRAY
    if(++pty_major >= sysconf(_SC_CRAY_NPTY))
	return -1;
    snprintf(buf, sz, "/dev/pty/%03d", pty_major);
#else
    if(++pty_major == strlen(bsd_1)){
	pty_major = 0;
	if(++pty_minor == strlen(bsd_2))
	    return -1;
    }
#ifdef __hpux
    snprintf(buf, sz, "/dev/ptym/pty%c%c", bsd_2[pty_major], bsd_1[pty_minor]);
#else
    snprintf(buf, sz, "/dev/pty%c%c", bsd_2[pty_major], bsd_1[pty_minor]);
#endif /* __hpux */
#endif /* CRAY */
    return 0;
}

static void
pty_scan_tty(char *buf, size_t sz)
{
#ifdef CRAY
    snprintf(buf, sz, "/dev/ttyp%03d", pty_major);
#elif defined(__hpux)
    snprintf(buf, sz, "/dev/pty/tty%c%c", bsd_2[pty_major], bsd_1[pty_minor]);
#else
    snprintf(buf, sz, "/dev/tty%c%c", bsd_2[pty_major], bsd_1[pty_minor]);
#endif
}

static int
ptym_open_streams_flavor(char *pts_name, int *streams_pty)
{
    /* Try clone device master ptys */
    const char *const clone[] = { "/dev/ptc", "/dev/ptmx",
				  "/dev/ptm", "/dev/ptym/clone", 0 };
    int	fdm;
    const char *const *q;
    
    for (q = clone; *q; q++) {
	fdm = open(*q, O_RDWR);
	if (fdm >= 0)
	    break;
    }
    if (fdm >= 0) {
	char *ptr1;
	if ((ptr1 = ptsname(fdm)) != NULL) /* Get slave's name */
	    strcpy(pts_name, ptr1); /* Return name of slave */  
	else {
	    close(fdm);
	    return(-4);
	}
	if (grantpt(fdm) < 0) {	/* Grant access to slave */
	    close(fdm);
	    return(-2);
	}
	if (unlockpt(fdm) < 0) {	/* Clear slave's lock flag */
	    close(fdm);
	    return(-3);
	}
	return(fdm);			/* return fd of master */
    }
    return -1;
}

static int
ptym_open_bsd_flavor(char *pts_name, int *streams_pty)
{
    int fdm;
    char ptm[MaxPathLen];

    pty_scan_start();

    while (pty_scan_next(ptm, sizeof(ptm)) != -1) {
	fdm = open(ptm, O_RDWR);
	if (fdm < 0)
	    continue;
#if SunOS == 4
	/* Avoid a bug in SunOS4 ttydriver */
	if (fdm > 0) {
	    int pgrp;
	    if ((ioctl(fdm, TIOCGPGRP, &pgrp) == -1)
		&& (errno == EIO))
		/* All fine */;
	    else {
		close(fdm);
		continue;
	    }
	}
#endif
	pty_scan_tty(pts_name, sizeof(ptm));
#if CRAY
	/* this is some magic from the telnet code */
	{
	    struct stat sb;
	    if(stat(pts_name, &sb) < 0) {
		close(fdm);
		continue;
	    }
	    if(sb.st_uid || sb.st_gid || sb.st_mode != 0600) {
		chown(pts_name, 0, 0);
		chmod(pts_name, 0600);
		close(fdm);
		fdm = open(ptm, 2);
		if (fdm < 0)
		    continue;
	    }
	}
	/*
	 * Now it should be safe...check for accessability.
	 */
	if (access(pts_name, 6) != 0){
	    /* no tty side to pty so skip it */
	    close(fdm);
	    continue;
	}
#endif
	return fdm;	/* All done! */
    }
    
    /* We failed to find BSD style pty */
    errno = ENOENT;
    return -1;
}

/*
 *
 * Open a master pty either using the STREAM flavor or the BSD flavor.
 * Depending on if there are any free ptys in the different classes we
 * need to try both. Normally try STREAMS first and then BSD.
 *
 * Kludge alert: Under HP-UX 10 and perhaps other systems STREAM ptys
 * doesn't get initialized properly so we try them in different order
 * until the problem has been resolved.
 *
 */
static int
ptym_open(char *pts_name, size_t pts_name_sz, int *streams_pty)
{
    int	fdm;

#ifdef HAVE__GETPTY
    {
	char *p = _getpty(&fdm, O_RDWR, 0600, 1);
	if (p) {
	    *streams_pty = 1;
	    strcpy (pts_name, p);
	    return fdm;
	}
    }
#endif

#ifdef STREAMSPTY
    fdm = ptym_open_streams_flavor(pts_name, streams_pty);
    if (fdm >= 0)
      {
	*streams_pty = 1;
	return fdm;
      }
#endif
    
    fdm = ptym_open_bsd_flavor(pts_name, streams_pty);
    if (fdm >= 0)
      {
	*streams_pty = 0;
	return fdm;
      }

#ifndef STREAMSPTY
    fdm = ptym_open_streams_flavor(pts_name, streams_pty);
    if (fdm >= 0)
      {
	*streams_pty = 1;
	return fdm;
      }
#endif
    
    return -1;
}

static int
maybe_push_modules(int fd, char **modules)
{
#ifdef I_PUSH
  char **p;
  int err;

  for(p=modules; *p; p++){
    err=ioctl(fd, I_FIND, *p);
    if(err == 1)
      break;
    if(err < 0 && errno != EINVAL)
      return -17;
    /* module not pushed or does not exist */
  }
  /* p points to null or to an already pushed module, now push all
     modules before this one */

  for(p--; p >= modules; p--){
    err = ioctl(fd, I_PUSH, *p);
    if(err < 0 && errno != EINVAL)
      return -17;
  }
#endif
  return 0;
}

static int
ptys_open(int fdm, char *pts_name, int streams_pty)
{
    int fds;

    if (streams_pty) {
	/* Streams style slave ptys */
	if ( (fds = open(pts_name, O_RDWR)) < 0) {
	    close(fdm);
	    return(-5);
	}

	{
	  char *ttymodules[] = { "ttcompat", "ldterm", "ptem", NULL };
	  char *ptymodules[] = { "pckt", NULL };
	  
	  if(maybe_push_modules(fds, ttymodules)<0){
	    close(fdm);
	    close(fds);
	    return -6;
	  }
	  if(maybe_push_modules(fdm, ptymodules)<0){
	    close(fdm);
	    close(fds);
	    return -7;
	  }
	}
    } else {
        /* BSD style slave ptys */
	struct group *grptr;
	int gid;
	if ( (grptr = getgrnam("tty")) != NULL)
	    gid = grptr->gr_gid;
	else
	    gid = -1;	/* group tty is not in the group file */

	/* Grant access to slave */
	chown(pts_name, getuid(), gid);
	chmod(pts_name, S_IRUSR | S_IWUSR | S_IWGRP);

	if ( (fds = open(pts_name, O_RDWR)) < 0) {
	    close(fdm);
	    return(-1);
	}
    }
    return(fds);
}

int
forkpty(int *ptrfdm,
	char *slave_name,
	struct termios *slave_termios,
	struct winsize *slave_winsize)
{
    int		fdm, fds, streams_pty;
    pid_t	pid;
    char	pts_name[20];

    if (!forkpty_ok)
        fatal(0, "Protocol not yet supported, use telnet", 0);

    if ( (fdm = ptym_open(pts_name, sizeof(pts_name), &streams_pty)) < 0)
	return -1;

    if (slave_name != NULL)
	strcpy(slave_name, pts_name);	/* Return name of slave */

    pid = fork();
    if (pid < 0)
	return(-1);
    else if (pid == 0) {		/* Child */
	if (setsid() < 0)
	    fatal(0, "setsid() failure", errno);

        revoke(slave_name);

#if defined(NeXT) || defined(ultrix)
	/* The NeXT is severely broken, this makes things slightly
	 * better but we still doesn't get a working pty. If there
	 * where a TIOCSCTTY we could perhaps fix things but... The
	 * same problem also exists in xterm! */
	if (setpgrp(0, 0) < 0)
	    fatal(0, "NeXT kludge failed setpgrp", errno);
#endif

	/* SVR4 acquires controlling terminal on open() */
	if ( (fds = ptys_open(fdm, pts_name, streams_pty)) < 0)
	    return -1;
	close(fdm);		/* All done with master in child */
	
#if	defined(TIOCSCTTY) && !defined(CIBAUD) && !defined(__hpux)
	/* 44BSD way to acquire controlling terminal */
	/* !CIBAUD to avoid doing this under SunOS */
	if (ioctl(fds, TIOCSCTTY, (char *) 0) < 0)
	    return -1;
#endif
#if defined(NeXT)
	{
	    int t = open("/dev/tty", O_RDWR);
	    if (t < 0)
	        fatal(0, "Failed to open /dev/tty", errno);
	    close(fds);
	    fds = t;
	}
#endif
	/* Set slave's termios and window size */
	if (slave_termios != NULL) {
	    if (tcsetattr(fds, TCSANOW, slave_termios) < 0)
		return -1;
	}
#ifdef TIOCSWINSZ
	if (slave_winsize != NULL) {
	    if (ioctl(fds, TIOCSWINSZ, slave_winsize) < 0)
		return -1;
	}
#endif
	/* slave becomes stdin/stdout/stderr of child */
	if (dup2(fds, STDIN_FILENO) != STDIN_FILENO)
	    return -1;
	if (dup2(fds, STDOUT_FILENO) != STDOUT_FILENO)
	    return -1;
	if (dup2(fds, STDERR_FILENO) != STDERR_FILENO)
	    return -1;
	if (fds > STDERR_FILENO)
	    close(fds);
	return(0);		/* child returns 0 just like fork() */
    }
    else {			/* Parent */
	*ptrfdm = fdm;	/* Return fd of master */
	return(pid);	/* Parent returns pid of child */
    }
}
#endif /* HAVE_FORKPTY */
