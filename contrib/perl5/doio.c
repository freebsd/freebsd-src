/*    doio.c
 *
 *    Copyright (c) 1991-1999, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * "Far below them they saw the white waters pour into a foaming bowl, and
 * then swirl darkly about a deep oval basin in the rocks, until they found
 * their way out again through a narrow gate, and flowed away, fuming and
 * chattering, into calmer and more level reaches."
 */

#include "EXTERN.h"
#include "perl.h"

#if defined(HAS_MSG) || defined(HAS_SEM) || defined(HAS_SHM)
#ifndef HAS_SEM
#include <sys/ipc.h>
#endif
#ifdef HAS_MSG
#include <sys/msg.h>
#endif
#ifdef HAS_SHM
#include <sys/shm.h>
# ifndef HAS_SHMAT_PROTOTYPE
    extern Shmat_t shmat _((int, char *, int));
# endif
#endif
#endif

#ifdef I_UTIME
#  if defined(_MSC_VER) || defined(__MINGW32__)
#    include <sys/utime.h>
#  else
#    include <utime.h>
#  endif
#endif

#ifdef I_FCNTL
#include <fcntl.h>
#endif
#ifdef I_SYS_FILE
#include <sys/file.h>
#endif
#ifdef O_EXCL
#  define OPEN_EXCL O_EXCL
#else
#  define OPEN_EXCL 0
#endif

#if !defined(NSIG) || defined(M_UNIX) || defined(M_XENIX)
#include <signal.h>
#endif

/* XXX If this causes problems, set i_unistd=undef in the hint file.  */
#ifdef I_UNISTD
#  include <unistd.h>
#endif

#if defined(HAS_SOCKET) && !defined(VMS) /* VMS handles sockets via vmsish.h */
# include <sys/socket.h>
# include <netdb.h>
# ifndef ENOTSOCK
#  ifdef I_NET_ERRNO
#   include <net/errno.h>
#  endif
# endif
#endif

/* Put this after #includes because <unistd.h> defines _XOPEN_*. */
#ifndef Sock_size_t
#  if _XOPEN_VERSION >= 5 || defined(_XOPEN_SOURCE_EXTENDED) || defined(__GLIBC__)
#    define Sock_size_t Size_t
#  else
#    define Sock_size_t int
#  endif
#endif

bool
do_open(GV *gv, register char *name, I32 len, int as_raw, int rawmode, int rawperm, PerlIO *supplied_fp)
{
    register IO *io = GvIOn(gv);
    PerlIO *saveifp = Nullfp;
    PerlIO *saveofp = Nullfp;
    char savetype = ' ';
    int writing = 0;
    PerlIO *fp;
    int fd;
    int result;
    bool was_fdopen = FALSE;

    PL_forkprocess = 1;		/* assume true if no fork */

    if (IoIFP(io)) {
	fd = PerlIO_fileno(IoIFP(io));
	if (IoTYPE(io) == '-')
	    result = 0;
	else if (fd <= PL_maxsysfd) {
	    saveifp = IoIFP(io);
	    saveofp = IoOFP(io);
	    savetype = IoTYPE(io);
	    result = 0;
	}
	else if (IoTYPE(io) == '|')
	    result = PerlProc_pclose(IoIFP(io));
	else if (IoIFP(io) != IoOFP(io)) {
	    if (IoOFP(io)) {
		result = PerlIO_close(IoOFP(io));
		PerlIO_close(IoIFP(io));	/* clear stdio, fd already closed */
	    }
	    else
		result = PerlIO_close(IoIFP(io));
	}
	else
	    result = PerlIO_close(IoIFP(io));
	if (result == EOF && fd > PL_maxsysfd)
	    PerlIO_printf(PerlIO_stderr(), "Warning: unable to close filehandle %s properly.\n",
	      GvENAME(gv));
	IoOFP(io) = IoIFP(io) = Nullfp;
    }

    if (as_raw) {
#ifndef O_ACCMODE
#define O_ACCMODE 3		/* Assume traditional implementation */
#endif
	switch (result = rawmode & O_ACCMODE) {
	case O_RDONLY:
	     IoTYPE(io) = '<';
	     break;
	case O_WRONLY:
	     IoTYPE(io) = '>';
	     break;
	case O_RDWR:
	default:
	     IoTYPE(io) = '+';
	     break;
	}

	writing = (result > 0);
	fd = PerlLIO_open3(name, rawmode, rawperm);

	if (fd == -1)
	    fp = NULL;
	else {
	    char *fpmode;
	    if (result == O_RDONLY)
		fpmode = "r";
#ifdef O_APPEND
	    else if (rawmode & O_APPEND)
		fpmode = (result == O_WRONLY) ? "a" : "a+";
#endif
	    else
		fpmode = (result == O_WRONLY) ? "w" : "r+";
	    fp = PerlIO_fdopen(fd, fpmode);
	    if (!fp)
		PerlLIO_close(fd);
	}
    }
    else {
	char *myname;
	char mode[3];		/* stdio file mode ("r\0" or "r+\0") */
	int dodup;

	myname = savepvn(name, len);
	SAVEFREEPV(myname);
	name = myname;
	while (len && isSPACE(name[len-1]))
	    name[--len] = '\0';

	mode[0] = mode[1] = mode[2] = '\0';
	IoTYPE(io) = *name;
	if (*name == '+' && len > 1 && name[len-1] != '|') { /* scary */
	    mode[1] = *name++;
	    --len;
	    writing = 1;
	}

	if (*name == '|') {
	    /*SUPPRESS 530*/
	    for (name++; isSPACE(*name); name++) ;
	    if (strNE(name,"-"))
		TAINT_ENV();
	    TAINT_PROPER("piped open");
	    if (name[strlen(name)-1] == '|') {
		name[strlen(name)-1] = '\0' ;
		if (PL_dowarn)
		    warn("Can't do bidirectional pipe");
	    }
	    fp = PerlProc_popen(name,"w");
	    writing = 1;
	}
	else if (*name == '>') {
	    TAINT_PROPER("open");
	    name++;
	    if (*name == '>') {
		mode[0] = IoTYPE(io) = 'a';
		name++;
	    }
	    else
		mode[0] = 'w';
	    writing = 1;

	    if (*name == '&') {
	      duplicity:
		dodup = 1;
		name++;
		if (*name == '=') {
		    dodup = 0;
		    name++;
		}
		if (!*name && supplied_fp)
		    fp = supplied_fp;
		else {
		    /*SUPPRESS 530*/
		    for (; isSPACE(*name); name++) ;
		    if (isDIGIT(*name))
			fd = atoi(name);
		    else {
			IO* thatio;
			gv = gv_fetchpv(name,FALSE,SVt_PVIO);
			thatio = GvIO(gv);
			if (!thatio) {
#ifdef EINVAL
			    SETERRNO(EINVAL,SS$_IVCHAN);
#endif
			    goto say_false;
			}
			if (IoIFP(thatio)) {
			    fd = PerlIO_fileno(IoIFP(thatio));
			    if (IoTYPE(thatio) == 's')
				IoTYPE(io) = 's';
			}
			else
			    fd = -1;
		    }
		    if (dodup)
			fd = PerlLIO_dup(fd);
		    else
			was_fdopen = TRUE;
		    if (!(fp = PerlIO_fdopen(fd,mode))) {
			if (dodup)
			    PerlLIO_close(fd);
			}
		}
	    }
	    else {
		/*SUPPRESS 530*/
		for (; isSPACE(*name); name++) ;
		if (strEQ(name,"-")) {
		    fp = PerlIO_stdout();
		    IoTYPE(io) = '-';
		}
		else  {
		    fp = PerlIO_open(name,mode);
		}
	    }
	}
	else if (*name == '<') {
	    /*SUPPRESS 530*/
	    for (name++; isSPACE(*name); name++) ;
	    mode[0] = 'r';
	    if (*name == '&')
		goto duplicity;
	    if (strEQ(name,"-")) {
		fp = PerlIO_stdin();
		IoTYPE(io) = '-';
	    }
	    else
		fp = PerlIO_open(name,mode);
	}
	else if (len > 1 && name[len-1] == '|') {
	    name[--len] = '\0';
	    while (len && isSPACE(name[len-1]))
		name[--len] = '\0';
	    /*SUPPRESS 530*/
	    for (; isSPACE(*name); name++) ;
	    if (strNE(name,"-"))
		TAINT_ENV();
	    TAINT_PROPER("piped open");
	    fp = PerlProc_popen(name,"r");
	    IoTYPE(io) = '|';
	}
	else {
	    IoTYPE(io) = '<';
	    /*SUPPRESS 530*/
	    for (; isSPACE(*name); name++) ;
	    if (strEQ(name,"-")) {
		fp = PerlIO_stdin();
		IoTYPE(io) = '-';
	    }
	    else
		fp = PerlIO_open(name,"r");
	}
    }
    if (!fp) {
	if (PL_dowarn && IoTYPE(io) == '<' && strchr(name, '\n'))
	    warn(warn_nl, "open");
	goto say_false;
    }
    if (IoTYPE(io) &&
      IoTYPE(io) != '|' && IoTYPE(io) != '-') {
	dTHR;
	if (PerlLIO_fstat(PerlIO_fileno(fp),&PL_statbuf) < 0) {
	    (void)PerlIO_close(fp);
	    goto say_false;
	}
	if (S_ISSOCK(PL_statbuf.st_mode))
	    IoTYPE(io) = 's';	/* in case a socket was passed in to us */
#ifdef HAS_SOCKET
	else if (
#ifdef S_IFMT
	    !(PL_statbuf.st_mode & S_IFMT)
#else
	    !PL_statbuf.st_mode
#endif
	) {
	    char tmpbuf[256];
	    Sock_size_t buflen = sizeof tmpbuf;
	    if (PerlSock_getsockname(PerlIO_fileno(fp), (struct sockaddr *)tmpbuf,
			    &buflen) >= 0
		  || errno != ENOTSOCK)
		IoTYPE(io) = 's'; /* some OS's return 0 on fstat()ed socket */
				/* but some return 0 for streams too, sigh */
	}
#endif
    }
    if (saveifp) {		/* must use old fp? */
	fd = PerlIO_fileno(saveifp);
	if (saveofp) {
	    PerlIO_flush(saveofp);		/* emulate PerlIO_close() */
	    if (saveofp != saveifp) {	/* was a socket? */
		PerlIO_close(saveofp);
		if (fd > 2)
		    Safefree(saveofp);
	    }
	}
	if (fd != PerlIO_fileno(fp)) {
	    int pid;
	    SV *sv;

	    PerlLIO_dup2(PerlIO_fileno(fp), fd);
	    sv = *av_fetch(PL_fdpid,PerlIO_fileno(fp),TRUE);
	    (void)SvUPGRADE(sv, SVt_IV);
	    pid = SvIVX(sv);
	    SvIVX(sv) = 0;
	    sv = *av_fetch(PL_fdpid,fd,TRUE);
	    (void)SvUPGRADE(sv, SVt_IV);
	    SvIVX(sv) = pid;
	    if (!was_fdopen)
		PerlIO_close(fp);

	}
	fp = saveifp;
	PerlIO_clearerr(fp);
    }
#if defined(HAS_FCNTL) && defined(F_SETFD)
    {
	int save_errno = errno;
	fd = PerlIO_fileno(fp);
	fcntl(fd,F_SETFD,fd > PL_maxsysfd); /* can change errno */
	errno = save_errno;
    }
#endif
    IoIFP(io) = fp;
    if (writing) {
	dTHR;
	if (IoTYPE(io) == 's'
	  || (IoTYPE(io) == '>' && S_ISCHR(PL_statbuf.st_mode)) ) {
	    if (!(IoOFP(io) = PerlIO_fdopen(PerlIO_fileno(fp),"w"))) {
		PerlIO_close(fp);
		IoIFP(io) = Nullfp;
		goto say_false;
	    }
	}
	else
	    IoOFP(io) = fp;
    }
    return TRUE;

say_false:
    IoIFP(io) = saveifp;
    IoOFP(io) = saveofp;
    IoTYPE(io) = savetype;
    return FALSE;
}

PerlIO *
nextargv(register GV *gv)
{
    register SV *sv;
#ifndef FLEXFILENAMES
    int filedev;
    int fileino;
#endif
    int fileuid;
    int filegid;

    if (!PL_argvoutgv)
	PL_argvoutgv = gv_fetchpv("ARGVOUT",TRUE,SVt_PVIO);
    if (PL_filemode & (S_ISUID|S_ISGID)) {
	PerlIO_flush(IoIFP(GvIOn(PL_argvoutgv)));  /* chmod must follow last write */
#ifdef HAS_FCHMOD
	(void)fchmod(PL_lastfd,PL_filemode);
#else
	(void)PerlLIO_chmod(PL_oldname,PL_filemode);
#endif
    }
    PL_filemode = 0;
    while (av_len(GvAV(gv)) >= 0) {
	dTHR;
	STRLEN oldlen;
	sv = av_shift(GvAV(gv));
	SAVEFREESV(sv);
	sv_setsv(GvSV(gv),sv);
	SvSETMAGIC(GvSV(gv));
	PL_oldname = SvPVx(GvSV(gv), oldlen);
	if (do_open(gv,PL_oldname,oldlen,PL_inplace!=0,O_RDONLY,0,Nullfp)) {
	    if (PL_inplace) {
		TAINT_PROPER("inplace open");
		if (oldlen == 1 && *PL_oldname == '-') {
		    setdefout(gv_fetchpv("STDOUT",TRUE,SVt_PVIO));
		    return IoIFP(GvIOp(gv));
		}
#ifndef FLEXFILENAMES
		filedev = PL_statbuf.st_dev;
		fileino = PL_statbuf.st_ino;
#endif
		PL_filemode = PL_statbuf.st_mode;
		fileuid = PL_statbuf.st_uid;
		filegid = PL_statbuf.st_gid;
		if (!S_ISREG(PL_filemode)) {
		    warn("Can't do inplace edit: %s is not a regular file",
		      PL_oldname );
		    do_close(gv,FALSE);
		    continue;
		}
		if (*PL_inplace) {
		    char *star = strchr(PL_inplace, '*');
		    if (star) {
			char *begin = PL_inplace;
			sv_setpvn(sv, "", 0);
			do {
			    sv_catpvn(sv, begin, star - begin);
			    sv_catpvn(sv, PL_oldname, oldlen);
			    begin = ++star;
			} while ((star = strchr(begin, '*')));
			if (*begin)
			    sv_catpv(sv,begin);
		    }
		    else {
			sv_catpv(sv,PL_inplace);
		    }
#ifndef FLEXFILENAMES
		    if (PerlLIO_stat(SvPVX(sv),&PL_statbuf) >= 0
		      && PL_statbuf.st_dev == filedev
		      && PL_statbuf.st_ino == fileino
#ifdef DJGPP
                      || (_djstat_fail_bits & _STFAIL_TRUENAME)!=0
#endif
                      ) {
			warn("Can't do inplace edit: %s would not be uniq",
			  SvPVX(sv) );
			do_close(gv,FALSE);
			continue;
		    }
#endif
#ifdef HAS_RENAME
#ifndef DOSISH
		    if (PerlLIO_rename(PL_oldname,SvPVX(sv)) < 0) {
			warn("Can't rename %s to %s: %s, skipping file",
			  PL_oldname, SvPVX(sv), Strerror(errno) );
			do_close(gv,FALSE);
			continue;
		    }
#else
		    do_close(gv,FALSE);
		    (void)PerlLIO_unlink(SvPVX(sv));
		    (void)PerlLIO_rename(PL_oldname,SvPVX(sv));
		    do_open(gv,SvPVX(sv),SvCUR(sv),PL_inplace!=0,O_RDONLY,0,Nullfp);
#endif /* DOSISH */
#else
		    (void)UNLINK(SvPVX(sv));
		    if (link(PL_oldname,SvPVX(sv)) < 0) {
			warn("Can't rename %s to %s: %s, skipping file",
			  PL_oldname, SvPVX(sv), Strerror(errno) );
			do_close(gv,FALSE);
			continue;
		    }
		    (void)UNLINK(PL_oldname);
#endif
		}
		else {
#if !defined(DOSISH) && !defined(AMIGAOS)
#  ifndef VMS  /* Don't delete; use automatic file versioning */
		    if (UNLINK(PL_oldname) < 0) {
			warn("Can't remove %s: %s, skipping file",
			  PL_oldname, Strerror(errno) );
			do_close(gv,FALSE);
			continue;
		    }
#  endif
#else
		    croak("Can't do inplace edit without backup");
#endif
		}

		sv_setpvn(sv,">",!PL_inplace);
		sv_catpvn(sv,PL_oldname,oldlen);
		SETERRNO(0,0);		/* in case sprintf set errno */
#ifdef VMS
		if (!do_open(PL_argvoutgv,SvPVX(sv),SvCUR(sv),PL_inplace!=0,
                 O_WRONLY|O_CREAT|O_TRUNC,0,Nullfp)) { 
#else
		if (!do_open(PL_argvoutgv,SvPVX(sv),SvCUR(sv),PL_inplace!=0,
			     O_WRONLY|O_CREAT|OPEN_EXCL,0666,Nullfp)) {
#endif
		    warn("Can't do inplace edit on %s: %s",
		      PL_oldname, Strerror(errno) );
		    do_close(gv,FALSE);
		    continue;
		}
		setdefout(PL_argvoutgv);
		PL_lastfd = PerlIO_fileno(IoIFP(GvIOp(PL_argvoutgv)));
		(void)PerlLIO_fstat(PL_lastfd,&PL_statbuf);
#ifdef HAS_FCHMOD
		(void)fchmod(PL_lastfd,PL_filemode);
#else
#  if !(defined(WIN32) && defined(__BORLANDC__))
		/* Borland runtime creates a readonly file! */
		(void)PerlLIO_chmod(PL_oldname,PL_filemode);
#  endif
#endif
		if (fileuid != PL_statbuf.st_uid || filegid != PL_statbuf.st_gid) {
#ifdef HAS_FCHOWN
		    (void)fchown(PL_lastfd,fileuid,filegid);
#else
#ifdef HAS_CHOWN
		    (void)PerlLIO_chown(PL_oldname,fileuid,filegid);
#endif
#endif
		}
	    }
	    return IoIFP(GvIOp(gv));
	}
	else
	    PerlIO_printf(PerlIO_stderr(), "Can't open %s: %s\n",
	      SvPV(sv, oldlen), Strerror(errno));
    }
    if (PL_inplace) {
	(void)do_close(PL_argvoutgv,FALSE);
	setdefout(gv_fetchpv("STDOUT",TRUE,SVt_PVIO));
    }
    return Nullfp;
}

#ifdef HAS_PIPE
void
do_pipe(SV *sv, GV *rgv, GV *wgv)
{
    register IO *rstio;
    register IO *wstio;
    int fd[2];

    if (!rgv)
	goto badexit;
    if (!wgv)
	goto badexit;

    rstio = GvIOn(rgv);
    wstio = GvIOn(wgv);

    if (IoIFP(rstio))
	do_close(rgv,FALSE);
    if (IoIFP(wstio))
	do_close(wgv,FALSE);

    if (PerlProc_pipe(fd) < 0)
	goto badexit;
    IoIFP(rstio) = PerlIO_fdopen(fd[0], "r");
    IoOFP(wstio) = PerlIO_fdopen(fd[1], "w");
    IoIFP(wstio) = IoOFP(wstio);
    IoTYPE(rstio) = '<';
    IoTYPE(wstio) = '>';
    if (!IoIFP(rstio) || !IoOFP(wstio)) {
	if (IoIFP(rstio)) PerlIO_close(IoIFP(rstio));
	else PerlLIO_close(fd[0]);
	if (IoOFP(wstio)) PerlIO_close(IoOFP(wstio));
	else PerlLIO_close(fd[1]);
	goto badexit;
    }

    sv_setsv(sv,&PL_sv_yes);
    return;

badexit:
    sv_setsv(sv,&PL_sv_undef);
    return;
}
#endif

/* explicit renamed to avoid C++ conflict    -- kja */
bool
do_close(GV *gv, bool not_implicit)
{
    bool retval;
    IO *io;

    if (!gv)
	gv = PL_argvgv;
    if (!gv || SvTYPE(gv) != SVt_PVGV) {
	if (not_implicit)
	    SETERRNO(EBADF,SS$_IVCHAN);
	return FALSE;
    }
    io = GvIO(gv);
    if (!io) {		/* never opened */
	if (not_implicit) {
	    if (PL_dowarn)
		warn("Close on unopened file <%s>",GvENAME(gv));
	    SETERRNO(EBADF,SS$_IVCHAN);
	}
	return FALSE;
    }
    retval = io_close(io);
    if (not_implicit) {
	IoLINES(io) = 0;
	IoPAGE(io) = 0;
	IoLINES_LEFT(io) = IoPAGE_LEN(io);
    }
    IoTYPE(io) = ' ';
    return retval;
}

bool
io_close(IO *io)
{
    bool retval = FALSE;
    int status;

    if (IoIFP(io)) {
	if (IoTYPE(io) == '|') {
	    status = PerlProc_pclose(IoIFP(io));
	    STATUS_NATIVE_SET(status);
	    retval = (STATUS_POSIX == 0);
	}
	else if (IoTYPE(io) == '-')
	    retval = TRUE;
	else {
	    if (IoOFP(io) && IoOFP(io) != IoIFP(io)) {		/* a socket */
		retval = (PerlIO_close(IoOFP(io)) != EOF);
		PerlIO_close(IoIFP(io));	/* clear stdio, fd already closed */
	    }
	    else
		retval = (PerlIO_close(IoIFP(io)) != EOF);
	}
	IoOFP(io) = IoIFP(io) = Nullfp;
    }
    else {
	SETERRNO(EBADF,SS$_IVCHAN);
    }

    return retval;
}

bool
do_eof(GV *gv)
{
    dTHR;
    register IO *io;
    int ch;

    io = GvIO(gv);

    if (!io)
	return TRUE;

    while (IoIFP(io)) {

        if (PerlIO_has_cntptr(IoIFP(io))) {	/* (the code works without this) */
	    if (PerlIO_get_cnt(IoIFP(io)) > 0)	/* cheat a little, since */
		return FALSE;			/* this is the most usual case */
        }

	ch = PerlIO_getc(IoIFP(io));
	if (ch != EOF) {
	    (void)PerlIO_ungetc(IoIFP(io),ch);
	    return FALSE;
	}
        if (PerlIO_has_cntptr(IoIFP(io)) && PerlIO_canset_cnt(IoIFP(io))) {
	    if (PerlIO_get_cnt(IoIFP(io)) < -1)
		PerlIO_set_cnt(IoIFP(io),-1);
	}
	if (PL_op->op_flags & OPf_SPECIAL) { /* not necessarily a real EOF yet? */
	    if (!nextargv(PL_argvgv))	/* get another fp handy */
		return TRUE;
	}
	else
	    return TRUE;		/* normal fp, definitely end of file */
    }
    return TRUE;
}

long
do_tell(GV *gv)
{
    register IO *io;
    register PerlIO *fp;

    if (gv && (io = GvIO(gv)) && (fp = IoIFP(io))) {
#ifdef ULTRIX_STDIO_BOTCH
	if (PerlIO_eof(fp))
	    (void)PerlIO_seek(fp, 0L, 2);	/* ultrix 1.2 workaround */
#endif
	return PerlIO_tell(fp);
    }
    if (PL_dowarn)
	warn("tell() on unopened file");
    SETERRNO(EBADF,RMS$_IFI);
    return -1L;
}

bool
do_seek(GV *gv, long int pos, int whence)
{
    register IO *io;
    register PerlIO *fp;

    if (gv && (io = GvIO(gv)) && (fp = IoIFP(io))) {
#ifdef ULTRIX_STDIO_BOTCH
	if (PerlIO_eof(fp))
	    (void)PerlIO_seek(fp, 0L, 2);	/* ultrix 1.2 workaround */
#endif
	return PerlIO_seek(fp, pos, whence) >= 0;
    }
    if (PL_dowarn)
	warn("seek() on unopened file");
    SETERRNO(EBADF,RMS$_IFI);
    return FALSE;
}

long
do_sysseek(GV *gv, long int pos, int whence)
{
    register IO *io;
    register PerlIO *fp;

    if (gv && (io = GvIO(gv)) && (fp = IoIFP(io)))
	return PerlLIO_lseek(PerlIO_fileno(fp), pos, whence);
    if (PL_dowarn)
	warn("sysseek() on unopened file");
    SETERRNO(EBADF,RMS$_IFI);
    return -1L;
}

int
do_binmode(PerlIO *fp, int iotype, int flag)
{
    if (flag != TRUE)
	croak("panic: unsetting binmode"); /* Not implemented yet */
#ifdef DOSISH
#if defined(atarist) || defined(__MINT__)
    if (!PerlIO_flush(fp) && (fp->_flag |= _IOBIN))
	return 1;
    else
	return 0;
#else
    if (PerlLIO_setmode(PerlIO_fileno(fp), OP_BINARY) != -1) {
#if defined(WIN32) && defined(__BORLANDC__)
	/* The translation mode of the stream is maintained independent
	 * of the translation mode of the fd in the Borland RTL (heavy
	 * digging through their runtime sources reveal).  User has to
	 * set the mode explicitly for the stream (though they don't
	 * document this anywhere). GSAR 97-5-24
	 */
	PerlIO_seek(fp,0L,0);
	((FILE*)fp)->flags |= _F_BIN;
#endif
	return 1;
    }
    else
	return 0;
#endif
#else
#if defined(USEMYBINMODE)
    if (my_binmode(fp,iotype) != NULL)
	return 1;
    else
	return 0;
#else
    return 1;
#endif
#endif
}

#if !defined(HAS_TRUNCATE) && !defined(HAS_CHSIZE) && defined(F_FREESP)
	/* code courtesy of William Kucharski */
#define HAS_CHSIZE

I32 my_chsize(fd, length)
I32 fd;			/* file descriptor */
Off_t length;		/* length to set file to */
{
    struct flock fl;
    struct stat filebuf;

    if (PerlLIO_fstat(fd, &filebuf) < 0)
	return -1;

    if (filebuf.st_size < length) {

	/* extend file length */

	if ((PerlLIO_lseek(fd, (length - 1), 0)) < 0)
	    return -1;

	/* write a "0" byte */

	if ((PerlLIO_write(fd, "", 1)) != 1)
	    return -1;
    }
    else {
	/* truncate length */

	fl.l_whence = 0;
	fl.l_len = 0;
	fl.l_start = length;
	fl.l_type = F_WRLCK;    /* write lock on file space */

	/*
	* This relies on the UNDOCUMENTED F_FREESP argument to
	* fcntl(2), which truncates the file so that it ends at the
	* position indicated by fl.l_start.
	*
	* Will minor miracles never cease?
	*/

	if (fcntl(fd, F_FREESP, &fl) < 0)
	    return -1;

    }

    return 0;
}
#endif /* F_FREESP */

bool
do_print(register SV *sv, PerlIO *fp)
{
    register char *tmps;
    STRLEN len;

    /* assuming fp is checked earlier */
    if (!sv)
	return TRUE;
    if (PL_ofmt) {
	if (SvGMAGICAL(sv))
	    mg_get(sv);
        if (SvIOK(sv) && SvIVX(sv) != 0) {
	    PerlIO_printf(fp, PL_ofmt, (double)SvIVX(sv));
	    return !PerlIO_error(fp);
	}
	if (  (SvNOK(sv) && SvNVX(sv) != 0.0)
	   || (looks_like_number(sv) && sv_2nv(sv) != 0.0) ) {
	    PerlIO_printf(fp, PL_ofmt, SvNVX(sv));
	    return !PerlIO_error(fp);
	}
    }
    switch (SvTYPE(sv)) {
    case SVt_NULL:
	if (PL_dowarn)
	    warn(warn_uninit);
	return TRUE;
    case SVt_IV:
	if (SvIOK(sv)) {
	    if (SvGMAGICAL(sv))
		mg_get(sv);
	    PerlIO_printf(fp, "%ld", (long)SvIVX(sv));
	    return !PerlIO_error(fp);
	}
	/* FALL THROUGH */
    default:
	tmps = SvPV(sv, len);
	break;
    }
    if (len && (PerlIO_write(fp,tmps,len) == 0 || PerlIO_error(fp)))
	return FALSE;
    return !PerlIO_error(fp);
}

I32
my_stat(ARGSproto)
{
    djSP;
    IO *io;
    GV* tmpgv;

    if (PL_op->op_flags & OPf_REF) {
	EXTEND(SP,1);
	tmpgv = cGVOP->op_gv;
      do_fstat:
	io = GvIO(tmpgv);
	if (io && IoIFP(io)) {
	    PL_statgv = tmpgv;
	    sv_setpv(PL_statname,"");
	    PL_laststype = OP_STAT;
	    return (PL_laststatval = PerlLIO_fstat(PerlIO_fileno(IoIFP(io)), &PL_statcache));
	}
	else {
	    if (tmpgv == PL_defgv)
		return PL_laststatval;
	    if (PL_dowarn)
		warn("Stat on unopened file <%s>",
		  GvENAME(tmpgv));
	    PL_statgv = Nullgv;
	    sv_setpv(PL_statname,"");
	    return (PL_laststatval = -1);
	}
    }
    else {
	SV* sv = POPs;
	char *s;
	STRLEN n_a;
	PUTBACK;
	if (SvTYPE(sv) == SVt_PVGV) {
	    tmpgv = (GV*)sv;
	    goto do_fstat;
	}
	else if (SvROK(sv) && SvTYPE(SvRV(sv)) == SVt_PVGV) {
	    tmpgv = (GV*)SvRV(sv);
	    goto do_fstat;
	}

	s = SvPV(sv, n_a);
	PL_statgv = Nullgv;
	sv_setpv(PL_statname, s);
	PL_laststype = OP_STAT;
	PL_laststatval = PerlLIO_stat(s, &PL_statcache);
	if (PL_laststatval < 0 && PL_dowarn && strchr(s, '\n'))
	    warn(warn_nl, "stat");
	return PL_laststatval;
    }
}

I32
my_lstat(ARGSproto)
{
    djSP;
    SV *sv;
    STRLEN n_a;
    if (PL_op->op_flags & OPf_REF) {
	EXTEND(SP,1);
	if (cGVOP->op_gv == PL_defgv) {
	    if (PL_laststype != OP_LSTAT)
		croak("The stat preceding -l _ wasn't an lstat");
	    return PL_laststatval;
	}
	croak("You can't use -l on a filehandle");
    }

    PL_laststype = OP_LSTAT;
    PL_statgv = Nullgv;
    sv = POPs;
    PUTBACK;
    sv_setpv(PL_statname,SvPV(sv, n_a));
#ifdef HAS_LSTAT
    PL_laststatval = PerlLIO_lstat(SvPV(sv, n_a),&PL_statcache);
#else
    PL_laststatval = PerlLIO_stat(SvPV(sv, n_a),&PL_statcache);
#endif
    if (PL_laststatval < 0 && PL_dowarn && strchr(SvPV(sv, n_a), '\n'))
	warn(warn_nl, "lstat");
    return PL_laststatval;
}

bool
do_aexec(SV *really, register SV **mark, register SV **sp)
{
    register char **a;
    char *tmps;
    STRLEN n_a;

    if (sp > mark) {
	dTHR;
	New(401,PL_Argv, sp - mark + 1, char*);
	a = PL_Argv;
	while (++mark <= sp) {
	    if (*mark)
		*a++ = SvPVx(*mark, n_a);
	    else
		*a++ = "";
	}
	*a = Nullch;
	if (*PL_Argv[0] != '/')	/* will execvp use PATH? */
	    TAINT_ENV();		/* testing IFS here is overkill, probably */
	if (really && *(tmps = SvPV(really, n_a)))
	    PerlProc_execvp(tmps,PL_Argv);
	else
	    PerlProc_execvp(PL_Argv[0],PL_Argv);
	if (PL_dowarn)
	    warn("Can't exec \"%s\": %s", PL_Argv[0], Strerror(errno));
    }
    do_execfree();
    return FALSE;
}

void
do_execfree(void)
{
    if (PL_Argv) {
	Safefree(PL_Argv);
	PL_Argv = Null(char **);
    }
    if (PL_Cmd) {
	Safefree(PL_Cmd);
	PL_Cmd = Nullch;
    }
}

#if !defined(OS2) && !defined(WIN32) && !defined(DJGPP)

bool
do_exec(char *cmd)
{
    register char **a;
    register char *s;
    char flags[10];

    while (*cmd && isSPACE(*cmd))
	cmd++;

    /* save an extra exec if possible */

#ifdef CSH
    if (strnEQ(cmd,PL_cshname,PL_cshlen) && strnEQ(cmd+PL_cshlen," -c",3)) {
	strcpy(flags,"-c");
	s = cmd+PL_cshlen+3;
	if (*s == 'f') {
	    s++;
	    strcat(flags,"f");
	}
	if (*s == ' ')
	    s++;
	if (*s++ == '\'') {
	    char *ncmd = s;

	    while (*s)
		s++;
	    if (s[-1] == '\n')
		*--s = '\0';
	    if (s[-1] == '\'') {
		*--s = '\0';
		PerlProc_execl(PL_cshname,"csh", flags,ncmd,(char*)0);
		*s = '\'';
		return FALSE;
	    }
	}
    }
#endif /* CSH */

    /* see if there are shell metacharacters in it */

    if (*cmd == '.' && isSPACE(cmd[1]))
	goto doshell;

    if (strnEQ(cmd,"exec",4) && isSPACE(cmd[4]))
	goto doshell;

    for (s = cmd; *s && isALPHA(*s); s++) ;	/* catch VAR=val gizmo */
    if (*s == '=')
	goto doshell;

    for (s = cmd; *s; s++) {
	if (*s != ' ' && !isALPHA(*s) && strchr("$&*(){}[]'\";\\|?<>~`\n",*s)) {
	    if (*s == '\n' && !s[1]) {
		*s = '\0';
		break;
	    }
	  doshell:
	    PerlProc_execl(PL_sh_path, "sh", "-c", cmd, (char*)0);
	    return FALSE;
	}
    }

    New(402,PL_Argv, (s - cmd) / 2 + 2, char*);
    PL_Cmd = savepvn(cmd, s-cmd);
    a = PL_Argv;
    for (s = PL_Cmd; *s;) {
	while (*s && isSPACE(*s)) s++;
	if (*s)
	    *(a++) = s;
	while (*s && !isSPACE(*s)) s++;
	if (*s)
	    *s++ = '\0';
    }
    *a = Nullch;
    if (PL_Argv[0]) {
	PerlProc_execvp(PL_Argv[0],PL_Argv);
	if (errno == ENOEXEC) {		/* for system V NIH syndrome */
	    do_execfree();
	    goto doshell;
	}
	if (PL_dowarn)
	    warn("Can't exec \"%s\": %s", PL_Argv[0], Strerror(errno));
    }
    do_execfree();
    return FALSE;
}

#endif /* OS2 || WIN32 */

I32
apply(I32 type, register SV **mark, register SV **sp)
{
    dTHR;
    register I32 val;
    register I32 val2;
    register I32 tot = 0;
    char *what;
    char *s;
    SV **oldmark = mark;
    STRLEN n_a;

#define APPLY_TAINT_PROPER() \
    STMT_START {							\
	if (PL_tainted) { TAINT_PROPER(what); }				\
    } STMT_END

    /* This is a first heuristic; it doesn't catch tainting magic. */
    if (PL_tainting) {
	while (++mark <= sp) {
	    if (SvTAINTED(*mark)) {
		TAINT;
		break;
	    }
	}
	mark = oldmark;
    }
    switch (type) {
    case OP_CHMOD:
	what = "chmod";
	APPLY_TAINT_PROPER();
	if (++mark <= sp) {
	    val = SvIVx(*mark);
	    APPLY_TAINT_PROPER();
	    tot = sp - mark;
	    while (++mark <= sp) {
		char *name = SvPVx(*mark, n_a);
		APPLY_TAINT_PROPER();
		if (PerlLIO_chmod(name, val))
		    tot--;
	    }
	}
	break;
#ifdef HAS_CHOWN
    case OP_CHOWN:
	what = "chown";
	APPLY_TAINT_PROPER();
	if (sp - mark > 2) {
	    val = SvIVx(*++mark);
	    val2 = SvIVx(*++mark);
	    APPLY_TAINT_PROPER();
	    tot = sp - mark;
	    while (++mark <= sp) {
		char *name = SvPVx(*mark, n_a);
		APPLY_TAINT_PROPER();
		if (PerlLIO_chown(name, val, val2))
		    tot--;
	    }
	}
	break;
#endif
/* 
XXX Should we make lchown() directly available from perl?
For now, we'll let Configure test for HAS_LCHOWN, but do
nothing in the core.
    --AD  5/1998
*/
#ifdef HAS_KILL
    case OP_KILL:
	what = "kill";
	APPLY_TAINT_PROPER();
	if (mark == sp)
	    break;
	s = SvPVx(*++mark, n_a);
	if (isUPPER(*s)) {
	    if (*s == 'S' && s[1] == 'I' && s[2] == 'G')
		s += 3;
	    if (!(val = whichsig(s)))
		croak("Unrecognized signal name \"%s\"",s);
	}
	else
	    val = SvIVx(*mark);
	APPLY_TAINT_PROPER();
	tot = sp - mark;
#ifdef VMS
	/* kill() doesn't do process groups (job trees?) under VMS */
	if (val < 0) val = -val;
	if (val == SIGKILL) {
#	    include <starlet.h>
	    /* Use native sys$delprc() to insure that target process is
	     * deleted; supervisor-mode images don't pay attention to
	     * CRTL's emulation of Unix-style signals and kill()
	     */
	    while (++mark <= sp) {
		I32 proc = SvIVx(*mark);
		register unsigned long int __vmssts;
		APPLY_TAINT_PROPER();
		if (!((__vmssts = sys$delprc(&proc,0)) & 1)) {
		    tot--;
		    switch (__vmssts) {
			case SS$_NONEXPR:
			case SS$_NOSUCHNODE:
			    SETERRNO(ESRCH,__vmssts);
			    break;
			case SS$_NOPRIV:
			    SETERRNO(EPERM,__vmssts);
			    break;
			default:
			    SETERRNO(EVMSERR,__vmssts);
		    }
		}
	    }
	    break;
	}
#endif
	if (val < 0) {
	    val = -val;
	    while (++mark <= sp) {
		I32 proc = SvIVx(*mark);
		APPLY_TAINT_PROPER();
#ifdef HAS_KILLPG
		if (PerlProc_killpg(proc,val))	/* BSD */
#else
		if (PerlProc_kill(-proc,val))	/* SYSV */
#endif
		    tot--;
	    }
	}
	else {
	    while (++mark <= sp) {
		I32 proc = SvIVx(*mark);
		APPLY_TAINT_PROPER();
		if (PerlProc_kill(proc, val))
		    tot--;
	    }
	}
	break;
#endif
    case OP_UNLINK:
	what = "unlink";
	APPLY_TAINT_PROPER();
	tot = sp - mark;
	while (++mark <= sp) {
	    s = SvPVx(*mark, n_a);
	    APPLY_TAINT_PROPER();
	    if (PL_euid || PL_unsafe) {
		if (UNLINK(s))
		    tot--;
	    }
	    else {	/* don't let root wipe out directories without -U */
#ifdef HAS_LSTAT
		if (PerlLIO_lstat(s,&PL_statbuf) < 0 || S_ISDIR(PL_statbuf.st_mode))
#else
		if (PerlLIO_stat(s,&PL_statbuf) < 0 || S_ISDIR(PL_statbuf.st_mode))
#endif
		    tot--;
		else {
		    if (UNLINK(s))
			tot--;
		}
	    }
	}
	break;
#ifdef HAS_UTIME
    case OP_UTIME:
	what = "utime";
	APPLY_TAINT_PROPER();
	if (sp - mark > 2) {
#if defined(I_UTIME) || defined(VMS)
	    struct utimbuf utbuf;
#else
	    struct {
		Time_t	actime;
		Time_t	modtime;
	    } utbuf;
#endif

	    Zero(&utbuf, sizeof utbuf, char);
#ifdef BIG_TIME
	    utbuf.actime = (Time_t)SvNVx(*++mark);	/* time accessed */
	    utbuf.modtime = (Time_t)SvNVx(*++mark);	/* time modified */
#else
	    utbuf.actime = (Time_t)SvIVx(*++mark);	/* time accessed */
	    utbuf.modtime = (Time_t)SvIVx(*++mark);	/* time modified */
#endif
	    APPLY_TAINT_PROPER();
	    tot = sp - mark;
	    while (++mark <= sp) {
		char *name = SvPVx(*mark, n_a);
		APPLY_TAINT_PROPER();
		if (PerlLIO_utime(name, &utbuf))
		    tot--;
	    }
	}
	else
	    tot = 0;
	break;
#endif
    }
    return tot;

#undef APPLY_TAINT_PROPER
}

/* Do the permissions allow some operation?  Assumes statcache already set. */
#ifndef VMS /* VMS' cando is in vms.c */
I32
cando(I32 bit, I32 effective, register struct stat *statbufp)
{
#ifdef DOSISH
    /* [Comments and code from Len Reed]
     * MS-DOS "user" is similar to UNIX's "superuser," but can't write
     * to write-protected files.  The execute permission bit is set
     * by the Miscrosoft C library stat() function for the following:
     *		.exe files
     *		.com files
     *		.bat files
     *		directories
     * All files and directories are readable.
     * Directories and special files, e.g. "CON", cannot be
     * write-protected.
     * [Comment by Tom Dinger -- a directory can have the write-protect
     *		bit set in the file system, but DOS permits changes to
     *		the directory anyway.  In addition, all bets are off
     *		here for networked software, such as Novell and
     *		Sun's PC-NFS.]
     */

     /* Atari stat() does pretty much the same thing. we set x_bit_set_in_stat
      * too so it will actually look into the files for magic numbers
      */
     return (bit & statbufp->st_mode) ? TRUE : FALSE;

#else /* ! DOSISH */
    if ((effective ? PL_euid : PL_uid) == 0) {	/* root is special */
	if (bit == S_IXUSR) {
	    if (statbufp->st_mode & 0111 || S_ISDIR(statbufp->st_mode))
		return TRUE;
	}
	else
	    return TRUE;		/* root reads and writes anything */
	return FALSE;
    }
    if (statbufp->st_uid == (effective ? PL_euid : PL_uid) ) {
	if (statbufp->st_mode & bit)
	    return TRUE;	/* ok as "user" */
    }
    else if (ingroup((I32)statbufp->st_gid,effective)) {
	if (statbufp->st_mode & bit >> 3)
	    return TRUE;	/* ok as "group" */
    }
    else if (statbufp->st_mode & bit >> 6)
	return TRUE;	/* ok as "other" */
    return FALSE;
#endif /* ! DOSISH */
}
#endif /* ! VMS */

I32
ingroup(I32 testgid, I32 effective)
{
    if (testgid == (effective ? PL_egid : PL_gid))
	return TRUE;
#ifdef HAS_GETGROUPS
#ifndef NGROUPS
#define NGROUPS 32
#endif
    {
	Groups_t gary[NGROUPS];
	I32 anum;

	anum = getgroups(NGROUPS,gary);
	while (--anum >= 0)
	    if (gary[anum] == testgid)
		return TRUE;
    }
#endif
    return FALSE;
}

#if defined(HAS_MSG) || defined(HAS_SEM) || defined(HAS_SHM)

I32
do_ipcget(I32 optype, SV **mark, SV **sp)
{
    dTHR;
    key_t key;
    I32 n, flags;

    key = (key_t)SvNVx(*++mark);
    n = (optype == OP_MSGGET) ? 0 : SvIVx(*++mark);
    flags = SvIVx(*++mark);
    SETERRNO(0,0);
    switch (optype)
    {
#ifdef HAS_MSG
    case OP_MSGGET:
	return msgget(key, flags);
#endif
#ifdef HAS_SEM
    case OP_SEMGET:
	return semget(key, n, flags);
#endif
#ifdef HAS_SHM
    case OP_SHMGET:
	return shmget(key, n, flags);
#endif
#if !defined(HAS_MSG) || !defined(HAS_SEM) || !defined(HAS_SHM)
    default:
	croak("%s not implemented", op_desc[optype]);
#endif
    }
    return -1;			/* should never happen */
}

I32
do_ipcctl(I32 optype, SV **mark, SV **sp)
{
    dTHR;
    SV *astr;
    char *a;
    I32 id, n, cmd, infosize, getinfo;
    I32 ret = -1;

    id = SvIVx(*++mark);
    n = (optype == OP_SEMCTL) ? SvIVx(*++mark) : 0;
    cmd = SvIVx(*++mark);
    astr = *++mark;
    infosize = 0;
    getinfo = (cmd == IPC_STAT);

    switch (optype)
    {
#ifdef HAS_MSG
    case OP_MSGCTL:
	if (cmd == IPC_STAT || cmd == IPC_SET)
	    infosize = sizeof(struct msqid_ds);
	break;
#endif
#ifdef HAS_SHM
    case OP_SHMCTL:
	if (cmd == IPC_STAT || cmd == IPC_SET)
	    infosize = sizeof(struct shmid_ds);
	break;
#endif
#ifdef HAS_SEM
    case OP_SEMCTL:
	if (cmd == IPC_STAT || cmd == IPC_SET)
	    infosize = sizeof(struct semid_ds);
	else if (cmd == GETALL || cmd == SETALL)
	{
	    struct semid_ds semds;
	    union semun semun;

            semun.buf = &semds;
	    getinfo = (cmd == GETALL);
	    if (Semctl(id, 0, IPC_STAT, semun) == -1)
		return -1;
	    infosize = semds.sem_nsems * sizeof(short);
		/* "short" is technically wrong but much more portable
		   than guessing about u_?short(_t)? */
	}
	break;
#endif
#if !defined(HAS_MSG) || !defined(HAS_SEM) || !defined(HAS_SHM)
    default:
	croak("%s not implemented", op_desc[optype]);
#endif
    }

    if (infosize)
    {
	STRLEN len;
	if (getinfo)
	{
	    SvPV_force(astr, len);
	    a = SvGROW(astr, infosize+1);
	}
	else
	{
	    a = SvPV(astr, len);
	    if (len != infosize)
		croak("Bad arg length for %s, is %lu, should be %ld",
			op_desc[optype], (unsigned long)len, (long)infosize);
	}
    }
    else
    {
	IV i = SvIV(astr);
	a = (char *)i;		/* ouch */
    }
    SETERRNO(0,0);
    switch (optype)
    {
#ifdef HAS_MSG
    case OP_MSGCTL:
	ret = msgctl(id, cmd, (struct msqid_ds *)a);
	break;
#endif
#ifdef HAS_SEM
    case OP_SEMCTL: {
            union semun unsemds;

            unsemds.buf = (struct semid_ds *)a;
	    ret = Semctl(id, n, cmd, unsemds);
        }
	break;
#endif
#ifdef HAS_SHM
    case OP_SHMCTL:
	ret = shmctl(id, cmd, (struct shmid_ds *)a);
	break;
#endif
    }
    if (getinfo && ret >= 0) {
	SvCUR_set(astr, infosize);
	*SvEND(astr) = '\0';
	SvSETMAGIC(astr);
    }
    return ret;
}

I32
do_msgsnd(SV **mark, SV **sp)
{
#ifdef HAS_MSG
    dTHR;
    SV *mstr;
    char *mbuf;
    I32 id, msize, flags;
    STRLEN len;

    id = SvIVx(*++mark);
    mstr = *++mark;
    flags = SvIVx(*++mark);
    mbuf = SvPV(mstr, len);
    if ((msize = len - sizeof(long)) < 0)
	croak("Arg too short for msgsnd");
    SETERRNO(0,0);
    return msgsnd(id, (struct msgbuf *)mbuf, msize, flags);
#else
    croak("msgsnd not implemented");
#endif
}

I32
do_msgrcv(SV **mark, SV **sp)
{
#ifdef HAS_MSG
    dTHR;
    SV *mstr;
    char *mbuf;
    long mtype;
    I32 id, msize, flags, ret;
    STRLEN len;

    id = SvIVx(*++mark);
    mstr = *++mark;
    msize = SvIVx(*++mark);
    mtype = (long)SvIVx(*++mark);
    flags = SvIVx(*++mark);
    if (SvTHINKFIRST(mstr)) {
	if (SvREADONLY(mstr))
	    croak("Can't msgrcv to readonly var");
	if (SvROK(mstr))
	    sv_unref(mstr);
    }
    SvPV_force(mstr, len);
    mbuf = SvGROW(mstr, sizeof(long)+msize+1);
    
    SETERRNO(0,0);
    ret = msgrcv(id, (struct msgbuf *)mbuf, msize, mtype, flags);
    if (ret >= 0) {
	SvCUR_set(mstr, sizeof(long)+ret);
	*SvEND(mstr) = '\0';
    }
    return ret;
#else
    croak("msgrcv not implemented");
#endif
}

I32
do_semop(SV **mark, SV **sp)
{
#ifdef HAS_SEM
    dTHR;
    SV *opstr;
    char *opbuf;
    I32 id;
    STRLEN opsize;

    id = SvIVx(*++mark);
    opstr = *++mark;
    opbuf = SvPV(opstr, opsize);
    if (opsize < sizeof(struct sembuf)
	|| (opsize % sizeof(struct sembuf)) != 0) {
	SETERRNO(EINVAL,LIB$_INVARG);
	return -1;
    }
    SETERRNO(0,0);
    return semop(id, (struct sembuf *)opbuf, opsize/sizeof(struct sembuf));
#else
    croak("semop not implemented");
#endif
}

I32
do_shmio(I32 optype, SV **mark, SV **sp)
{
#ifdef HAS_SHM
    dTHR;
    SV *mstr;
    char *mbuf, *shm;
    I32 id, mpos, msize;
    STRLEN len;
    struct shmid_ds shmds;

    id = SvIVx(*++mark);
    mstr = *++mark;
    mpos = SvIVx(*++mark);
    msize = SvIVx(*++mark);
    SETERRNO(0,0);
    if (shmctl(id, IPC_STAT, &shmds) == -1)
	return -1;
    if (mpos < 0 || msize < 0 || mpos + msize > shmds.shm_segsz) {
	SETERRNO(EFAULT,SS$_ACCVIO);		/* can't do as caller requested */
	return -1;
    }
    shm = (char *)shmat(id, (char*)NULL, (optype == OP_SHMREAD) ? SHM_RDONLY : 0);
    if (shm == (char *)-1)	/* I hate System V IPC, I really do */
	return -1;
    if (optype == OP_SHMREAD) {
	SvPV_force(mstr, len);
	mbuf = SvGROW(mstr, msize+1);

	Copy(shm + mpos, mbuf, msize, char);
	SvCUR_set(mstr, msize);
	*SvEND(mstr) = '\0';
	SvSETMAGIC(mstr);
    }
    else {
	I32 n;

	mbuf = SvPV(mstr, len);
	if ((n = len) > msize)
	    n = msize;
	Copy(mbuf, shm + mpos, n, char);
	if (n < msize)
	    memzero(shm + mpos + n, msize - n);
    }
    return shmdt(shm);
#else
    croak("shm I/O not implemented");
#endif
}

#endif /* SYSV IPC */

