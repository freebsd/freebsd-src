/* $RCSfile: doio.c,v $$Revision: 1.3 $$Date: 1995/05/30 05:03:00 $
 *
 *    Copyright (c) 1991, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * $Log: doio.c,v $
 * Revision 1.3  1995/05/30 05:03:00  rgrimes
 * Remove trailing whitespace.
 *
 * Revision 1.2  1994/09/11  03:17:32  gclarkii
 * Changed AF_LOCAL to AF_LOCAL_XX so as not to conflict with 4.4 socket.h
 * Added casts to shutup warnings in doio.c
 *
 * Revision 1.1.1.1  1994/09/10  06:27:32  gclarkii
 * Initial import of Perl 4.046 bmaked
 *
 * Revision 1.2  1994/03/09  22:24:27  ache
 * (cast) added for last argument of semctl
 *
 * Revision 1.1.1.1  1993/08/23  21:29:36  nate
 * PERL!
 *
 * Revision 4.0.1.6  92/06/11  21:08:16  lwall
 * patch34: some systems don't declare h_errno extern in header files
 *
 * Revision 4.0.1.5  92/06/08  13:00:21  lwall
 * patch20: some machines don't define ENOTSOCK in errno.h
 * patch20: new warnings for failed use of stat operators on filenames with \n
 * patch20: wait failed when STDOUT or STDERR reopened to a pipe
 * patch20: end of file latch not reset on reopen of STDIN
 * patch20: seek(HANDLE, 0, 1) went to eof because of ancient Ultrix workaround
 * patch20: fixed memory leak on system() for vfork() machines
 * patch20: get*by* routines now return something useful in a scalar context
 * patch20: h_errno now accessible via $?
 *
 * Revision 4.0.1.4  91/11/05  16:51:43  lwall
 * patch11: prepared for ctype implementations that don't define isascii()
 * patch11: perl mistook some streams for sockets because they return mode 0 too
 * patch11: reopening STDIN, STDOUT and STDERR failed on some machines
 * patch11: certain perl errors should set EBADF so that $! looks better
 * patch11: truncate on a closed filehandle could dump
 * patch11: stats of _ forgot whether prior stat was actually lstat
 * patch11: -T returned true on NFS directory
 *
 * Revision 4.0.1.3  91/06/10  01:21:19  lwall
 * patch10: read didn't work from character special files open for writing
 * patch10: close-on-exec wrongly set on system file descriptors
 *
 * Revision 4.0.1.2  91/06/07  10:53:39  lwall
 * patch4: new copyright notice
 * patch4: system fd's are now treated specially
 * patch4: added $^F variable to specify maximum system fd, default 2
 * patch4: character special files now opened with bidirectional stdio buffers
 * patch4: taintchecks could improperly modify parent in vfork()
 * patch4: many, many itty-bitty portability fixes
 *
 * Revision 4.0.1.1  91/04/11  17:41:06  lwall
 * patch1: hopefully straightened out some of the Xenix mess
 *
 * Revision 4.0  91/03/20  01:07:06  lwall
 * 4.0 baseline.
 *
 */

#include "EXTERN.h"
#include "perl.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>


#ifdef HAS_SOCKET
#include <sys/socket.h>
#include <netdb.h>
#ifndef ENOTSOCK
#include <net/errno.h>
#endif
#endif

#ifdef HAS_SELECT
#ifdef I_SYS_SELECT
#ifndef I_SYS_TIME
#include <sys/select.h>
#endif
#endif
#endif

#ifdef HOST_NOT_FOUND
extern int h_errno;
#endif

#if defined(HAS_MSG) || defined(HAS_SEM) || defined(HAS_SHM)
#include <sys/ipc.h>
#ifdef HAS_MSG
#include <sys/msg.h>
#endif
#ifdef HAS_SEM
#include <sys/sem.h>
#endif
#ifdef HAS_SHM
#include <sys/shm.h>
#endif
#endif

#ifdef I_PWD
#include <pwd.h>
#endif
#ifdef I_GRP
#include <grp.h>
#endif
#ifdef I_UTIME
#include <utime.h>
#endif
#ifdef I_FCNTL
#include <fcntl.h>
#endif
#ifdef I_SYS_FILE
#include <sys/file.h>
#endif

int laststatval = -1;
int laststype = O_STAT;

static char* warn_nl = "Unsuccessful %s on filename containing newline";

bool
do_open(stab,name,len)
STAB *stab;
register char *name;
int len;
{
    FILE *fp;
    register STIO *stio = stab_io(stab);
    char *myname = savestr(name);
    int result;
    int fd;
    int writing = 0;
    char mode[3];		/* stdio file mode ("r\0" or "r+\0") */
    FILE *saveifp = Nullfp;
    FILE *saveofp = Nullfp;
    char savetype = ' ';

    mode[0] = mode[1] = mode[2] = '\0';
    name = myname;
    forkprocess = 1;		/* assume true if no fork */
    while (len && isSPACE(name[len-1]))
	name[--len] = '\0';
    if (!stio)
	stio = stab_io(stab) = stio_new();
    else if (stio->ifp) {
	fd = fileno(stio->ifp);
	if (stio->type == '-')
	    result = 0;
	else if (fd <= maxsysfd) {
	    saveifp = stio->ifp;
	    saveofp = stio->ofp;
	    savetype = stio->type;
	    result = 0;
	}
	else if (stio->type == '|')
	    result = mypclose(stio->ifp);
	else if (stio->ifp != stio->ofp) {
	    if (stio->ofp) {
		result = fclose(stio->ofp);
		fclose(stio->ifp);	/* clear stdio, fd already closed */
	    }
	    else
		result = fclose(stio->ifp);
	}
	else
	    result = fclose(stio->ifp);
	if (result == EOF && fd > maxsysfd)
	    fprintf(stderr,"Warning: unable to close filehandle %s properly.\n",
	      stab_ename(stab));
	stio->ofp = stio->ifp = Nullfp;
    }
    if (*name == '+' && len > 1 && name[len-1] != '|') {	/* scary */
	mode[1] = *name++;
	mode[2] = '\0';
	--len;
	writing = 1;
    }
    else  {
	mode[1] = '\0';
    }
    stio->type = *name;
    if (*name == '|') {
	/*SUPPRESS 530*/
	for (name++; isSPACE(*name); name++) ;
#ifdef TAINT
	taintenv();
	taintproper("Insecure dependency in piped open");
#endif
	fp = mypopen(name,"w");
	writing = 1;
    }
    else if (*name == '>') {
#ifdef TAINT
	taintproper("Insecure dependency in open");
#endif
	name++;
	if (*name == '>') {
	    mode[0] = stio->type = 'a';
	    name++;
	}
	else
	    mode[0] = 'w';
	writing = 1;
	if (*name == '&') {
	  duplicity:
	    name++;
	    while (isSPACE(*name))
		name++;
	    if (isDIGIT(*name))
		fd = atoi(name);
	    else {
		stab = stabent(name,FALSE);
		if (!stab || !stab_io(stab)) {
#ifdef EINVAL
		    errno = EINVAL;
#endif
		    goto say_false;
		}
		if (stab_io(stab) && stab_io(stab)->ifp) {
		    fd = fileno(stab_io(stab)->ifp);
		    if (stab_io(stab)->type == 's')
			stio->type = 's';
		}
		else
		    fd = -1;
	    }
	    if (!(fp = fdopen(fd = dup(fd),mode))) {
		close(fd);
	    }
	}
	else {
	    while (isSPACE(*name))
		name++;
	    if (strEQ(name,"-")) {
		fp = stdout;
		stio->type = '-';
	    }
	    else  {
		fp = fopen(name,mode);
	    }
	}
    }
    else {
	if (*name == '<') {
	    mode[0] = 'r';
	    name++;
	    while (isSPACE(*name))
		name++;
	    if (*name == '&')
		goto duplicity;
	    if (strEQ(name,"-")) {
		fp = stdin;
		stio->type = '-';
	    }
	    else
		fp = fopen(name,mode);
	}
	else if (name[len-1] == '|') {
#ifdef TAINT
	    taintenv();
	    taintproper("Insecure dependency in piped open");
#endif
	    name[--len] = '\0';
	    while (len && isSPACE(name[len-1]))
		name[--len] = '\0';
	    /*SUPPRESS 530*/
	    for (; isSPACE(*name); name++) ;
	    fp = mypopen(name,"r");
	    stio->type = '|';
	}
	else {
	    stio->type = '<';
	    /*SUPPRESS 530*/
	    for (; isSPACE(*name); name++) ;
	    if (strEQ(name,"-")) {
		fp = stdin;
		stio->type = '-';
	    }
	    else
		fp = fopen(name,"r");
	}
    }
    if (!fp) {
	if (dowarn && stio->type == '<' && index(name, '\n'))
	    warn(warn_nl, "open");
	Safefree(myname);
	goto say_false;
    }
    Safefree(myname);
    if (stio->type &&
      stio->type != '|' && stio->type != '-') {
	if (fstat(fileno(fp),&statbuf) < 0) {
	    (void)fclose(fp);
	    goto say_false;
	}
	if (S_ISSOCK(statbuf.st_mode))
	    stio->type = 's';	/* in case a socket was passed in to us */
#ifdef HAS_SOCKET
	else if (
#ifdef S_IFMT
	    !(statbuf.st_mode & S_IFMT)
#else
	    !statbuf.st_mode
#endif
	) {
	    int buflen = sizeof tokenbuf;
	    if (getsockname(fileno(fp), (struct sockaddr * )tokenbuf, &buflen) >= 0
		|| errno != ENOTSOCK)
		stio->type = 's'; /* some OS's return 0 on fstat()ed socket */
				/* but some return 0 for streams too, sigh */
	}
#endif
    }
    if (saveifp) {		/* must use old fp? */
	fd = fileno(saveifp);
	if (saveofp) {
	    fflush(saveofp);		/* emulate fclose() */
	    if (saveofp != saveifp) {	/* was a socket? */
		fclose(saveofp);
		if (fd > 2)
		    Safefree(saveofp);
	    }
	}
	if (fd != fileno(fp)) {
	    int pid;
	    STR *str;

	    dup2(fileno(fp), fd);
	    str = afetch(fdpid,fileno(fp),TRUE);
	    pid = str->str_u.str_useful;
	    str->str_u.str_useful = 0;
	    str = afetch(fdpid,fd,TRUE);
	    str->str_u.str_useful = pid;
	    fclose(fp);

	}
	fp = saveifp;
	clearerr(fp);
    }
#if defined(HAS_FCNTL) && defined(F_SETFD)
    fd = fileno(fp);
    fcntl(fd,F_SETFD,fd > maxsysfd);
#endif
    stio->ifp = fp;
    if (writing) {
	if (stio->type == 's'
	  || (stio->type == '>' && S_ISCHR(statbuf.st_mode)) ) {
	    if (!(stio->ofp = fdopen(fileno(fp),"w"))) {
		fclose(fp);
		stio->ifp = Nullfp;
		goto say_false;
	    }
	}
	else
	    stio->ofp = fp;
    }
    return TRUE;

say_false:
    stio->ifp = saveifp;
    stio->ofp = saveofp;
    stio->type = savetype;
    return FALSE;
}

FILE *
nextargv(stab)
register STAB *stab;
{
    register STR *str;
#ifndef FLEXFILENAMES
    int filedev;
    int fileino;
#endif
    int fileuid;
    int filegid;
    static int filemode = 0;
    static int lastfd;
    static char *oldname;

    if (!argvoutstab)
	argvoutstab = stabent("ARGVOUT",TRUE);
    if (filemode & (S_ISUID|S_ISGID)) {
	fflush(stab_io(argvoutstab)->ifp);  /* chmod must follow last write */
#ifdef HAS_FCHMOD
	(void)fchmod(lastfd,filemode);
#else
	(void)chmod(oldname,filemode);
#endif
    }
    filemode = 0;
    while (alen(stab_xarray(stab)) >= 0) {
	str = ashift(stab_xarray(stab));
	str_sset(stab_val(stab),str);
	STABSET(stab_val(stab));
	oldname = str_get(stab_val(stab));
	if (do_open(stab,oldname,stab_val(stab)->str_cur)) {
	    if (inplace) {
#ifdef TAINT
		taintproper("Insecure dependency in inplace open");
#endif
		if (strEQ(oldname,"-")) {
		    str_free(str);
		    defoutstab = stabent("STDOUT",TRUE);
		    return stab_io(stab)->ifp;
		}
#ifndef FLEXFILENAMES
		filedev = statbuf.st_dev;
		fileino = statbuf.st_ino;
#endif
		filemode = statbuf.st_mode;
		fileuid = statbuf.st_uid;
		filegid = statbuf.st_gid;
		if (!S_ISREG(filemode)) {
		    warn("Can't do inplace edit: %s is not a regular file",
		      oldname );
		    do_close(stab,FALSE);
		    str_free(str);
		    continue;
		}
		if (*inplace) {
#ifdef SUFFIX
		    add_suffix(str,inplace);
#else
		    str_cat(str,inplace);
#endif
#ifndef FLEXFILENAMES
		    if (stat(str->str_ptr,&statbuf) >= 0
		      && statbuf.st_dev == filedev
		      && statbuf.st_ino == fileino ) {
			warn("Can't do inplace edit: %s > 14 characters",
			  str->str_ptr );
			do_close(stab,FALSE);
			str_free(str);
			continue;
		    }
#endif
#ifdef HAS_RENAME
#ifndef DOSISH
		    if (rename(oldname,str->str_ptr) < 0) {
			warn("Can't rename %s to %s: %s, skipping file",
			  oldname, str->str_ptr, strerror(errno) );
			do_close(stab,FALSE);
			str_free(str);
			continue;
		    }
#else
		    do_close(stab,FALSE);
		    (void)unlink(str->str_ptr);
		    (void)rename(oldname,str->str_ptr);
		    do_open(stab,str->str_ptr,stab_val(stab)->str_cur);
#endif /* MSDOS */
#else
		    (void)UNLINK(str->str_ptr);
		    if (link(oldname,str->str_ptr) < 0) {
			warn("Can't rename %s to %s: %s, skipping file",
			  oldname, str->str_ptr, strerror(errno) );
			do_close(stab,FALSE);
			str_free(str);
			continue;
		    }
		    (void)UNLINK(oldname);
#endif
		}
		else {
#ifndef DOSISH
		    if (UNLINK(oldname) < 0) {
			warn("Can't rename %s to %s: %s, skipping file",
			  oldname, str->str_ptr, strerror(errno) );
			do_close(stab,FALSE);
			str_free(str);
			continue;
		    }
#else
		    fatal("Can't do inplace edit without backup");
#endif
		}

		str_nset(str,">",1);
		str_cat(str,oldname);
		errno = 0;		/* in case sprintf set errno */
		if (!do_open(argvoutstab,str->str_ptr,str->str_cur)) {
		    warn("Can't do inplace edit on %s: %s",
		      oldname, strerror(errno) );
		    do_close(stab,FALSE);
		    str_free(str);
		    continue;
		}
		defoutstab = argvoutstab;
		lastfd = fileno(stab_io(argvoutstab)->ifp);
		(void)fstat(lastfd,&statbuf);
#ifdef HAS_FCHMOD
		(void)fchmod(lastfd,filemode);
#else
		(void)chmod(oldname,filemode);
#endif
		if (fileuid != statbuf.st_uid || filegid != statbuf.st_gid) {
#ifdef HAS_FCHOWN
		    (void)fchown(lastfd,fileuid,filegid);
#else
#ifdef HAS_CHOWN
		    (void)chown(oldname,fileuid,filegid);
#endif
#endif
		}
	    }
	    str_free(str);
	    return stab_io(stab)->ifp;
	}
	else
	    fprintf(stderr,"Can't open %s: %s\n",str_get(str), strerror(errno));
	str_free(str);
    }
    if (inplace) {
	(void)do_close(argvoutstab,FALSE);
	defoutstab = stabent("STDOUT",TRUE);
    }
    return Nullfp;
}

#ifdef HAS_PIPE
void
do_pipe(str, rstab, wstab)
STR *str;
STAB *rstab;
STAB *wstab;
{
    register STIO *rstio;
    register STIO *wstio;
    int fd[2];

    if (!rstab)
	goto badexit;
    if (!wstab)
	goto badexit;

    rstio = stab_io(rstab);
    wstio = stab_io(wstab);

    if (!rstio)
	rstio = stab_io(rstab) = stio_new();
    else if (rstio->ifp)
	do_close(rstab,FALSE);
    if (!wstio)
	wstio = stab_io(wstab) = stio_new();
    else if (wstio->ifp)
	do_close(wstab,FALSE);

    if (pipe(fd) < 0)
	goto badexit;
    rstio->ifp = fdopen(fd[0], "r");
    wstio->ofp = fdopen(fd[1], "w");
    wstio->ifp = wstio->ofp;
    rstio->type = '<';
    wstio->type = '>';
    if (!rstio->ifp || !wstio->ofp) {
	if (rstio->ifp) fclose(rstio->ifp);
	else close(fd[0]);
	if (wstio->ofp) fclose(wstio->ofp);
	else close(fd[1]);
	goto badexit;
    }

    str_sset(str,&str_yes);
    return;

badexit:
    str_sset(str,&str_undef);
    return;
}
#endif

bool
do_close(stab,explicit)
STAB *stab;
bool explicit;
{
    bool retval = FALSE;
    register STIO *stio;
    int status;

    if (!stab)
	stab = argvstab;
    if (!stab) {
	errno = EBADF;
	return FALSE;
    }
    stio = stab_io(stab);
    if (!stio) {		/* never opened */
	if (dowarn && explicit)
	    warn("Close on unopened file <%s>",stab_ename(stab));
	return FALSE;
    }
    if (stio->ifp) {
	if (stio->type == '|') {
	    status = mypclose(stio->ifp);
	    retval = (status == 0);
	    statusvalue = (unsigned short)status & 0xffff;
	}
	else if (stio->type == '-')
	    retval = TRUE;
	else {
	    if (stio->ofp && stio->ofp != stio->ifp) {		/* a socket */
		retval = (fclose(stio->ofp) != EOF);
		fclose(stio->ifp);	/* clear stdio, fd already closed */
	    }
	    else
		retval = (fclose(stio->ifp) != EOF);
	}
	stio->ofp = stio->ifp = Nullfp;
    }
    if (explicit)
	stio->lines = 0;
    stio->type = ' ';
    return retval;
}

bool
do_eof(stab)
STAB *stab;
{
    register STIO *stio;
    int ch;

    if (!stab) {			/* eof() */
	if (argvstab)
	    stio = stab_io(argvstab);
	else
	    return TRUE;
    }
    else
	stio = stab_io(stab);

    if (!stio)
	return TRUE;

    while (stio->ifp) {

#ifdef STDSTDIO			/* (the code works without this) */
	if (stio->ifp->_cnt > 0)	/* cheat a little, since */
	    return FALSE;		/* this is the most usual case */
#endif

	ch = getc(stio->ifp);
	if (ch != EOF) {
	    (void)ungetc(ch, stio->ifp);
	    return FALSE;
	}
#ifdef STDSTDIO
	if (stio->ifp->_cnt < -1)
	    stio->ifp->_cnt = -1;
#endif
	if (!stab) {			/* not necessarily a real EOF yet? */
	    if (!nextargv(argvstab))	/* get another fp handy */
		return TRUE;
	}
	else
	    return TRUE;		/* normal fp, definitely end of file */
    }
    return TRUE;
}

long
do_tell(stab)
STAB *stab;
{
    register STIO *stio;

    if (!stab)
	goto phooey;

    stio = stab_io(stab);
    if (!stio || !stio->ifp)
	goto phooey;

#ifdef ULTRIX_STDIO_BOTCH
    if (feof(stio->ifp))
	(void)fseek (stio->ifp, 0L, 2);		/* ultrix 1.2 workaround */
#endif

    return ftell(stio->ifp);

phooey:
    if (dowarn)
	warn("tell() on unopened file");
    errno = EBADF;
    return -1L;
}

bool
do_seek(stab, pos, whence)
STAB *stab;
long pos;
int whence;
{
    register STIO *stio;

    if (!stab)
	goto nuts;

    stio = stab_io(stab);
    if (!stio || !stio->ifp)
	goto nuts;

#ifdef ULTRIX_STDIO_BOTCH
    if (feof(stio->ifp))
	(void)fseek (stio->ifp, 0L, 2);		/* ultrix 1.2 workaround */
#endif

    return fseek(stio->ifp, pos, whence) >= 0;

nuts:
    if (dowarn)
	warn("seek() on unopened file");
    errno = EBADF;
    return FALSE;
}

int
do_ctl(optype,stab,func,argstr)
int optype;
STAB *stab;
int func;
STR *argstr;
{
    register STIO *stio;
    register char *s;
    int retval;

    if (!stab || !argstr || !(stio = stab_io(stab)) || !stio->ifp) {
	errno = EBADF;	/* well, sort of... */
	return -1;
    }

    if (argstr->str_pok || !argstr->str_nok) {
	if (!argstr->str_pok)
	    s = str_get(argstr);

#ifdef IOCPARM_MASK
#ifndef IOCPARM_LEN
#define IOCPARM_LEN(x)  (((x) >> 16) & IOCPARM_MASK)
#endif
#endif
#ifdef IOCPARM_LEN
	retval = IOCPARM_LEN(func);	/* on BSDish systes we're safe */
#else
	retval = 256;			/* otherwise guess at what's safe */
#endif
	if (argstr->str_cur < retval) {
	    Str_Grow(argstr,retval+1);
	    argstr->str_cur = retval;
	}

	s = argstr->str_ptr;
	s[argstr->str_cur] = 17;	/* a little sanity check here */
    }
    else {
	retval = (int)str_gnum(argstr);
#ifdef DOSISH
	s = (char*)(long)retval;		/* ouch */
#else
	s = (char*)retval;		/* ouch */
#endif
    }

#ifndef lint
    if (optype == O_IOCTL)
	retval = ioctl(fileno(stio->ifp), func, s);
    else
#ifdef DOSISH
	fatal("fcntl is not implemented");
#else
#ifdef HAS_FCNTL
	retval = fcntl(fileno(stio->ifp), func, s);
#else
	fatal("fcntl is not implemented");
#endif
#endif
#else /* lint */
    retval = 0;
#endif /* lint */

    if (argstr->str_pok) {
	if (s[argstr->str_cur] != 17)
	    fatal("Return value overflowed string");
	s[argstr->str_cur] = 0;		/* put our null back */
    }
    return retval;
}

int
do_stat(str,arg,gimme,arglast)
STR *str;
register ARG *arg;
int gimme;
int *arglast;
{
    register ARRAY *ary = stack;
    register int sp = arglast[0] + 1;
    int max = 13;

    if ((arg[1].arg_type & A_MASK) == A_WORD) {
	tmpstab = arg[1].arg_ptr.arg_stab;
	if (tmpstab != defstab) {
	    laststype = O_STAT;
	    statstab = tmpstab;
	    str_set(statname,"");
	    if (!stab_io(tmpstab) || !stab_io(tmpstab)->ifp ||
	      fstat(fileno(stab_io(tmpstab)->ifp),&statcache) < 0) {
		max = 0;
		laststatval = -1;
	    }
	}
	else if (laststatval < 0)
	    max = 0;
    }
    else {
	str_set(statname,str_get(ary->ary_array[sp]));
	statstab = Nullstab;
#ifdef HAS_LSTAT
	laststype = arg->arg_type;
	if (arg->arg_type == O_LSTAT)
	    laststatval = lstat(str_get(statname),&statcache);
	else
#endif
	    laststatval = stat(str_get(statname),&statcache);
	if (laststatval < 0) {
	    if (dowarn && index(str_get(statname), '\n'))
		warn(warn_nl, "stat");
	    max = 0;
	}
    }

    if (gimme != G_ARRAY) {
	if (max)
	    str_sset(str,&str_yes);
	else
	    str_sset(str,&str_undef);
	STABSET(str);
	ary->ary_array[sp] = str;
	return sp;
    }
    sp--;
    if (max) {
#ifndef lint
	(void)astore(ary,++sp,
	  str_2mortal(str_nmake((double)statcache.st_dev)));
	(void)astore(ary,++sp,
	  str_2mortal(str_nmake((double)statcache.st_ino)));
	(void)astore(ary,++sp,
	  str_2mortal(str_nmake((double)statcache.st_mode)));
	(void)astore(ary,++sp,
	  str_2mortal(str_nmake((double)statcache.st_nlink)));
	(void)astore(ary,++sp,
	  str_2mortal(str_nmake((double)statcache.st_uid)));
	(void)astore(ary,++sp,
	  str_2mortal(str_nmake((double)statcache.st_gid)));
	(void)astore(ary,++sp,
	  str_2mortal(str_nmake((double)statcache.st_rdev)));
	(void)astore(ary,++sp,
	  str_2mortal(str_nmake((double)statcache.st_size)));
	(void)astore(ary,++sp,
	  str_2mortal(str_nmake((double)statcache.st_atime)));
	(void)astore(ary,++sp,
	  str_2mortal(str_nmake((double)statcache.st_mtime)));
	(void)astore(ary,++sp,
	  str_2mortal(str_nmake((double)statcache.st_ctime)));
#ifdef STATBLOCKS
	(void)astore(ary,++sp,
	  str_2mortal(str_nmake((double)statcache.st_blksize)));
	(void)astore(ary,++sp,
	  str_2mortal(str_nmake((double)statcache.st_blocks)));
#else
	(void)astore(ary,++sp,
	  str_2mortal(str_make("",0)));
	(void)astore(ary,++sp,
	  str_2mortal(str_make("",0)));
#endif
#else /* lint */
	(void)astore(ary,++sp,str_nmake(0.0));
#endif /* lint */
    }
    return sp;
}

#if !defined(HAS_TRUNCATE) && !defined(HAS_CHSIZE) && defined(F_FREESP)
	/* code courtesy of William Kucharski */
#define HAS_CHSIZE

int chsize(fd, length)
int fd;			/* file descriptor */
off_t length;		/* length to set file to */
{
    extern long lseek();
    struct flock fl;
    struct stat filebuf;

    if (fstat(fd, &filebuf) < 0)
	return -1;

    if (filebuf.st_size < length) {

	/* extend file length */

	if ((lseek(fd, (length - 1), 0)) < 0)
	    return -1;

	/* write a "0" byte */

	if ((write(fd, "", 1)) != 1)
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

int					/*SUPPRESS 590*/
do_truncate(str,arg,gimme,arglast)
STR *str;
register ARG *arg;
int gimme;
int *arglast;
{
    register ARRAY *ary = stack;
    register int sp = arglast[0] + 1;
    off_t len = (off_t)str_gnum(ary->ary_array[sp+1]);
    int result = 1;
    STAB *tmpstab;

#if defined(HAS_TRUNCATE) || defined(HAS_CHSIZE)
#ifdef HAS_TRUNCATE
    if ((arg[1].arg_type & A_MASK) == A_WORD) {
	tmpstab = arg[1].arg_ptr.arg_stab;
	if (!stab_io(tmpstab) || !stab_io(tmpstab)->ifp ||
	  ftruncate(fileno(stab_io(tmpstab)->ifp), len) < 0)
	    result = 0;
    }
    else if (truncate(str_get(ary->ary_array[sp]), len) < 0)
	result = 0;
#else
    if ((arg[1].arg_type & A_MASK) == A_WORD) {
	tmpstab = arg[1].arg_ptr.arg_stab;
	if (!stab_io(tmpstab) || !stab_io(tmpstab)->ifp ||
	  chsize(fileno(stab_io(tmpstab)->ifp), len) < 0)
	    result = 0;
    }
    else {
	int tmpfd;

	if ((tmpfd = open(str_get(ary->ary_array[sp]), 0)) < 0)
	    result = 0;
	else {
	    if (chsize(tmpfd, len) < 0)
		result = 0;
	    close(tmpfd);
	}
    }
#endif

    if (result)
	str_sset(str,&str_yes);
    else
	str_sset(str,&str_undef);
    STABSET(str);
    ary->ary_array[sp] = str;
    return sp;
#else
    fatal("truncate not implemented");
#endif
}

int
looks_like_number(str)
STR *str;
{
    register char *s;
    register char *send;

    if (!str->str_pok)
	return TRUE;
    s = str->str_ptr;
    send = s + str->str_cur;
    while (isSPACE(*s))
	s++;
    if (s >= send)
	return FALSE;
    if (*s == '+' || *s == '-')
	s++;
    while (isDIGIT(*s))
	s++;
    if (s == send)
	return TRUE;
    if (*s == '.')
	s++;
    else if (s == str->str_ptr)
	return FALSE;
    while (isDIGIT(*s))
	s++;
    if (s == send)
	return TRUE;
    if (*s == 'e' || *s == 'E') {
	s++;
	if (*s == '+' || *s == '-')
	    s++;
	while (isDIGIT(*s))
	    s++;
    }
    while (isSPACE(*s))
	s++;
    if (s >= send)
	return TRUE;
    return FALSE;
}

bool
do_print(str,fp)
register STR *str;
FILE *fp;
{
    register char *tmps;

    if (!fp) {
	if (dowarn)
	    warn("print to unopened file");
	errno = EBADF;
	return FALSE;
    }
    if (!str)
	return TRUE;
    if (ofmt &&
      ((str->str_nok && str->str_u.str_nval != 0.0)
       || (looks_like_number(str) && str_gnum(str) != 0.0) ) ) {
	fprintf(fp, ofmt, str->str_u.str_nval);
	return !ferror(fp);
    }
    else {
	tmps = str_get(str);
	if (*tmps == 'S' && tmps[1] == 't' && tmps[2] == 'B' && tmps[3] == '\0'
	  && str->str_cur == sizeof(STBP) && strlen(tmps) < str->str_cur) {
	    STR *tmpstr = str_mortal(&str_undef);
	    stab_efullname(tmpstr,((STAB*)str));/* a stab value, be nice */
	    str = tmpstr;
	    tmps = str->str_ptr;
	    putc('*',fp);
	}
	if (str->str_cur && (fwrite(tmps,1,str->str_cur,fp) == 0 || ferror(fp)))
	    return FALSE;
    }
    return TRUE;
}

bool
do_aprint(arg,fp,arglast)
register ARG *arg;
register FILE *fp;
int *arglast;
{
    register STR **st = stack->ary_array;
    register int sp = arglast[1];
    register int retval;
    register int items = arglast[2] - sp;

    if (!fp) {
	if (dowarn)
	    warn("print to unopened file");
	errno = EBADF;
	return FALSE;
    }
    st += ++sp;
    if (arg->arg_type == O_PRTF) {
	do_sprintf(arg->arg_ptr.arg_str,items,st);
	retval = do_print(arg->arg_ptr.arg_str,fp);
    }
    else {
	retval = (items <= 0);
	for (; items > 0; items--,st++) {
	    if (retval && ofslen) {
		if (fwrite(ofs, 1, ofslen, fp) == 0 || ferror(fp)) {
		    retval = FALSE;
		    break;
		}
	    }
	    if (!(retval = do_print(*st, fp)))
		break;
	}
	if (retval && orslen)
	    if (fwrite(ors, 1, orslen, fp) == 0 || ferror(fp))
		retval = FALSE;
    }
    return retval;
}

int
mystat(arg,str)
ARG *arg;
STR *str;
{
    STIO *stio;

    if (arg[1].arg_type & A_DONT) {
	stio = stab_io(arg[1].arg_ptr.arg_stab);
	if (stio && stio->ifp) {
	    statstab = arg[1].arg_ptr.arg_stab;
	    str_set(statname,"");
	    laststype = O_STAT;
	    return (laststatval = fstat(fileno(stio->ifp), &statcache));
	}
	else {
	    if (arg[1].arg_ptr.arg_stab == defstab)
		return laststatval;
	    if (dowarn)
		warn("Stat on unopened file <%s>",
		  stab_ename(arg[1].arg_ptr.arg_stab));
	    statstab = Nullstab;
	    str_set(statname,"");
	    return (laststatval = -1);
	}
    }
    else {
	statstab = Nullstab;
	str_set(statname,str_get(str));
	laststype = O_STAT;
	laststatval = stat(str_get(str),&statcache);
	if (laststatval < 0 && dowarn && index(str_get(str), '\n'))
	    warn(warn_nl, "stat");
	return laststatval;
    }
}

int
mylstat(arg,str)
ARG *arg;
STR *str;
{
    if (arg[1].arg_type & A_DONT) {
	if (arg[1].arg_ptr.arg_stab == defstab) {
	    if (laststype != O_LSTAT)
		fatal("The stat preceding -l _ wasn't an lstat");
	    return laststatval;
	}
	fatal("You can't use -l on a filehandle");
    }

    laststype = O_LSTAT;
    statstab = Nullstab;
    str_set(statname,str_get(str));
#ifdef HAS_LSTAT
    laststatval = lstat(str_get(str),&statcache);
#else
    laststatval = stat(str_get(str),&statcache);
#endif
    if (laststatval < 0 && dowarn && index(str_get(str), '\n'))
	warn(warn_nl, "lstat");
    return laststatval;
}

STR *
do_fttext(arg,str)
register ARG *arg;
STR *str;
{
    int i;
    int len;
    int odd = 0;
    STDCHAR tbuf[512];
    register STDCHAR *s;
    register STIO *stio;

    if (arg[1].arg_type & A_DONT) {
	if (arg[1].arg_ptr.arg_stab == defstab) {
	    if (statstab)
		stio = stab_io(statstab);
	    else {
		str = statname;
		goto really_filename;
	    }
	}
	else {
	    statstab = arg[1].arg_ptr.arg_stab;
	    str_set(statname,"");
	    stio = stab_io(statstab);
	}
	if (stio && stio->ifp) {
#if defined(STDSTDIO) || defined(atarist) /* this will work with atariST */
	    fstat(fileno(stio->ifp),&statcache);
	    if (S_ISDIR(statcache.st_mode))	/* handle NFS glitch */
		return arg->arg_type == O_FTTEXT ? &str_no : &str_yes;
	    if (stio->ifp->_cnt <= 0) {
		i = getc(stio->ifp);
		if (i != EOF)
		    (void)ungetc(i,stio->ifp);
	    }
	    if (stio->ifp->_cnt <= 0)	/* null file is anything */
		return &str_yes;
	    len = stio->ifp->_cnt + (stio->ifp->_ptr - stio->ifp->_base);
	    s = stio->ifp->_base;
#else
#if (defined(BSD) && (BSD >= 199103))
	    fstat(fileno(stio->ifp),&statcache);
	    if (S_ISDIR(statcache.st_mode))	/* handle NFS glitch */
		return arg->arg_type == O_FTTEXT ? &str_no : &str_yes;

	    if (stio->ifp->_bf._size  <=  0) {
		i = getc(stio->ifp);
		if (i != EOF)
		    (void)ungetc(i,stio->ifp);
	    }

	    if (stio->ifp->_bf._size  <=  0)
		return &str_yes;
	    len = stio->ifp->_bf._size+(stio->ifp->_p - stio->ifp->_bf._base);
	    s = stio->ifp->_bf._base;
#else
	    fatal("-T and -B not implemented on filehandles");
#endif
#endif
	}
	else {
	    if (dowarn)
		warn("Test on unopened file <%s>",
		  stab_ename(arg[1].arg_ptr.arg_stab));
	    errno = EBADF;
	    return &str_undef;
	}
    }
    else {
	statstab = Nullstab;
	str_set(statname,str_get(str));
      really_filename:
	i = open(str_get(str),0);
	if (i < 0) {
	    if (dowarn && index(str_get(str), '\n'))
		warn(warn_nl, "open");
	    return &str_undef;
	}
	fstat(i,&statcache);
	len = read(i,tbuf,512);
	(void)close(i);
	if (len <= 0) {
	    if (S_ISDIR(statcache.st_mode) && arg->arg_type == O_FTTEXT)
		return &str_no;		/* special case NFS directories */
	    return &str_yes;		/* null file is anything */
	}
	s = tbuf;
    }

    /* now scan s to look for textiness */

    for (i = 0; i < len; i++,s++) {
	if (!*s) {			/* null never allowed in text */
	    odd += len;
	    break;
	}
	else if (*s & 128)
	    odd++;
	else if (*s < 32 &&
	  *s != '\n' && *s != '\r' && *s != '\b' &&
	  *s != '\t' && *s != '\f' && *s != 27)
	    odd++;
    }

    if ((odd * 10 > len) == (arg->arg_type == O_FTTEXT)) /* allow 10% odd */
	return &str_no;
    else
	return &str_yes;
}

static char **Argv = Null(char **);
static char *Cmd = Nullch;

bool
do_aexec(really,arglast)
STR *really;
int *arglast;
{
    register STR **st = stack->ary_array;
    register int sp = arglast[1];
    register int items = arglast[2] - sp;
    register char **a;
    char *tmps;

    if (items) {
	New(401,Argv, items+1, char*);
	a = Argv;
	for (st += ++sp; items > 0; items--,st++) {
	    if (*st)
		*a++ = str_get(*st);
	    else
		*a++ = "";
	}
	*a = Nullch;
#ifdef TAINT
	if (*Argv[0] != '/')	/* will execvp use PATH? */
	    taintenv();		/* testing IFS here is overkill, probably */
#endif
	if (really && *(tmps = str_get(really)))
	    execvp(tmps,Argv);
	else
	    execvp(Argv[0],Argv);
    }
    do_execfree();
    return FALSE;
}

void
do_execfree()
{
    if (Argv) {
	Safefree(Argv);
	Argv = Null(char **);
    }
    if (Cmd) {
	Safefree(Cmd);
	Cmd = Nullch;
    }
}

bool
do_exec(cmd)
char *cmd;
{
    register char **a;
    register char *s;
    char flags[10];

    /* save an extra exec if possible */

#ifdef CSH
    if (strnEQ(cmd,cshname,cshlen) && strnEQ(cmd+cshlen," -c",3)) {
	strcpy(flags,"-c");
	s = cmd+cshlen+3;
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
		execl(cshname,"csh", flags,ncmd,(char*)0);
		*s = '\'';
		return FALSE;
	    }
	}
    }
#endif /* CSH */

    /* see if there are shell metacharacters in it */

    /*SUPPRESS 530*/
    for (s = cmd; *s && isALPHA(*s); s++) ;	/* catch VAR=val gizmo */
    if (*s == '=')
	goto doshell;
    for (s = cmd; *s; s++) {
	if (*s != ' ' && !isALPHA(*s) && index("$&*(){}[]'\";\\|?<>~`\n",*s)) {
	    if (*s == '\n' && !s[1]) {
		*s = '\0';
		break;
	    }
	  doshell:
	    execl("/bin/sh","sh","-c",cmd,(char*)0);
	    return FALSE;
	}
    }
    New(402,Argv, (s - cmd) / 2 + 2, char*);
    Cmd = nsavestr(cmd, s-cmd);
    a = Argv;
    for (s = Cmd; *s;) {
	while (*s && isSPACE(*s)) s++;
	if (*s)
	    *(a++) = s;
	while (*s && !isSPACE(*s)) s++;
	if (*s)
	    *s++ = '\0';
    }
    *a = Nullch;
    if (Argv[0]) {
	execvp(Argv[0],Argv);
	if (errno == ENOEXEC) {		/* for system V NIH syndrome */
	    do_execfree();
	    goto doshell;
	}
    }
    do_execfree();
    return FALSE;
}

#ifdef HAS_SOCKET
int
do_socket(stab, arglast)
STAB *stab;
int *arglast;
{
    register STR **st = stack->ary_array;
    register int sp = arglast[1];
    register STIO *stio;
    int domain, type, protocol, fd;

    if (!stab) {
	errno = EBADF;
	return FALSE;
    }

    stio = stab_io(stab);
    if (!stio)
	stio = stab_io(stab) = stio_new();
    else if (stio->ifp)
	do_close(stab,FALSE);

    domain = (int)str_gnum(st[++sp]);
    type = (int)str_gnum(st[++sp]);
    protocol = (int)str_gnum(st[++sp]);
#ifdef TAINT
    taintproper("Insecure dependency in socket");
#endif
    fd = socket(domain,type,protocol);
    if (fd < 0)
	return FALSE;
    stio->ifp = fdopen(fd, "r");	/* stdio gets confused about sockets */
    stio->ofp = fdopen(fd, "w");
    stio->type = 's';
    if (!stio->ifp || !stio->ofp) {
	if (stio->ifp) fclose(stio->ifp);
	if (stio->ofp) fclose(stio->ofp);
	if (!stio->ifp && !stio->ofp) close(fd);
	return FALSE;
    }

    return TRUE;
}

int
do_bind(stab, arglast)
STAB *stab;
int *arglast;
{
    register STR **st = stack->ary_array;
    register int sp = arglast[1];
    register STIO *stio;
    char *addr;

    if (!stab)
	goto nuts;

    stio = stab_io(stab);
    if (!stio || !stio->ifp)
	goto nuts;

    addr = str_get(st[++sp]);
#ifdef TAINT
    taintproper("Insecure dependency in bind");
#endif
    return bind(fileno(stio->ifp), (struct sockaddr * ) addr, st[sp]->str_cur) >= 0;

nuts:
    if (dowarn)
	warn("bind() on closed fd");
    errno = EBADF;
    return FALSE;

}

int
do_connect(stab, arglast)
STAB *stab;
int *arglast;
{
    register STR **st = stack->ary_array;
    register int sp = arglast[1];
    register STIO *stio;
    char *addr;

    if (!stab)
	goto nuts;

    stio = stab_io(stab);
    if (!stio || !stio->ifp)
	goto nuts;

    addr = str_get(st[++sp]);
#ifdef TAINT
    taintproper("Insecure dependency in connect");
#endif
    return connect(fileno(stio->ifp), (struct sockaddr *) addr, st[sp]->str_cur) >= 0;

nuts:
    if (dowarn)
	warn("connect() on closed fd");
    errno = EBADF;
    return FALSE;

}

int
do_listen(stab, arglast)
STAB *stab;
int *arglast;
{
    register STR **st = stack->ary_array;
    register int sp = arglast[1];
    register STIO *stio;
    int backlog;

    if (!stab)
	goto nuts;

    stio = stab_io(stab);
    if (!stio || !stio->ifp)
	goto nuts;

    backlog = (int)str_gnum(st[++sp]);
    return listen(fileno(stio->ifp), backlog) >= 0;

nuts:
    if (dowarn)
	warn("listen() on closed fd");
    errno = EBADF;
    return FALSE;
}

void
do_accept(str, nstab, gstab)
STR *str;
STAB *nstab;
STAB *gstab;
{
    register STIO *nstio;
    register STIO *gstio;
    int len = sizeof buf;
    int fd;

    if (!nstab)
	goto badexit;
    if (!gstab)
	goto nuts;

    gstio = stab_io(gstab);
    nstio = stab_io(nstab);

    if (!gstio || !gstio->ifp)
	goto nuts;
    if (!nstio)
	nstio = stab_io(nstab) = stio_new();
    else if (nstio->ifp)
	do_close(nstab,FALSE);

    fd = accept(fileno(gstio->ifp),(struct sockaddr *)buf,&len);
    if (fd < 0)
	goto badexit;
    nstio->ifp = fdopen(fd, "r");
    nstio->ofp = fdopen(fd, "w");
    nstio->type = 's';
    if (!nstio->ifp || !nstio->ofp) {
	if (nstio->ifp) fclose(nstio->ifp);
	if (nstio->ofp) fclose(nstio->ofp);
	if (!nstio->ifp && !nstio->ofp) close(fd);
	goto badexit;
    }

    str_nset(str, buf, len);
    return;

nuts:
    if (dowarn)
	warn("accept() on closed fd");
    errno = EBADF;
badexit:
    str_sset(str,&str_undef);
    return;
}

int
do_shutdown(stab, arglast)
STAB *stab;
int *arglast;
{
    register STR **st = stack->ary_array;
    register int sp = arglast[1];
    register STIO *stio;
    int how;

    if (!stab)
	goto nuts;

    stio = stab_io(stab);
    if (!stio || !stio->ifp)
	goto nuts;

    how = (int)str_gnum(st[++sp]);
    return shutdown(fileno(stio->ifp), how) >= 0;

nuts:
    if (dowarn)
	warn("shutdown() on closed fd");
    errno = EBADF;
    return FALSE;

}

int
do_sopt(optype, stab, arglast)
int optype;
STAB *stab;
int *arglast;
{
    register STR **st = stack->ary_array;
    register int sp = arglast[1];
    register STIO *stio;
    int fd;
    unsigned int lvl;
    unsigned int optname;

    if (!stab)
	goto nuts;

    stio = stab_io(stab);
    if (!stio || !stio->ifp)
	goto nuts;

    fd = fileno(stio->ifp);
    lvl = (unsigned int)str_gnum(st[sp+1]);
    optname = (unsigned int)str_gnum(st[sp+2]);
    switch (optype) {
    case O_GSOCKOPT:
	st[sp] = str_2mortal(Str_new(22,257));
	st[sp]->str_cur = 256;
	st[sp]->str_pok = 1;
	if (getsockopt(fd, lvl, optname, st[sp]->str_ptr,
			(int*)&st[sp]->str_cur) < 0)
	    goto nuts;
	break;
    case O_SSOCKOPT:
	st[sp] = st[sp+3];
	if (setsockopt(fd, lvl, optname, st[sp]->str_ptr, st[sp]->str_cur) < 0)
	    goto nuts;
	st[sp] = &str_yes;
	break;
    }

    return sp;

nuts:
    if (dowarn)
	warn("[gs]etsockopt() on closed fd");
    st[sp] = &str_undef;
    errno = EBADF;
    return sp;

}

int
do_getsockname(optype, stab, arglast)
int optype;
STAB *stab;
int *arglast;
{
    register STR **st = stack->ary_array;
    register int sp = arglast[1];
    register STIO *stio;
    int fd;

    if (!stab)
	goto nuts;

    stio = stab_io(stab);
    if (!stio || !stio->ifp)
	goto nuts;

    st[sp] = str_2mortal(Str_new(22,257));
    st[sp]->str_cur = 256;
    st[sp]->str_pok = 1;
    fd = fileno(stio->ifp);
    switch (optype) {
    case O_GETSOCKNAME:
	if (getsockname(fd, (struct sockaddr *) st[sp]->str_ptr, (int*)&st[sp]->str_cur) < 0)
	    goto nuts2;
	break;
    case O_GETPEERNAME:
	if (getpeername(fd, (struct sockaddr *) st[sp]->str_ptr, (int*)&st[sp]->str_cur) < 0)
	    goto nuts2;
	break;
    }

    return sp;

nuts:
    if (dowarn)
	warn("get{sock,peer}name() on closed fd");
    errno = EBADF;
nuts2:
    st[sp] = &str_undef;
    return sp;

}

int
do_ghent(which,gimme,arglast)
int which;
int gimme;
int *arglast;
{
    register ARRAY *ary = stack;
    register int sp = arglast[0];
    register char **elem;
    register STR *str;
    struct hostent *gethostbyname();
    struct hostent *gethostbyaddr();
#ifdef HAS_GETHOSTENT
    struct hostent *gethostent();
#endif
    struct hostent *hent;
    unsigned long len;

    if (which == O_GHBYNAME) {
	char *name = str_get(ary->ary_array[sp+1]);

	hent = gethostbyname(name);
    }
    else if (which == O_GHBYADDR) {
	STR *addrstr = ary->ary_array[sp+1];
	int addrtype = (int)str_gnum(ary->ary_array[sp+2]);
	char *addr = str_get(addrstr);

	hent = gethostbyaddr(addr,addrstr->str_cur,addrtype);
    }
    else
#ifdef HAS_GETHOSTENT
	hent = gethostent();
#else
	fatal("gethostent not implemented");
#endif

#ifdef HOST_NOT_FOUND
    if (!hent)
	statusvalue = (unsigned short)h_errno & 0xffff;
#endif

    if (gimme != G_ARRAY) {
	astore(ary, ++sp, str = str_mortal(&str_undef));
	if (hent) {
	    if (which == O_GHBYNAME) {
#ifdef h_addr
		str_nset(str, *hent->h_addr, hent->h_length);
#else
		str_nset(str, hent->h_addr, hent->h_length);
#endif
	    }
	    else
		str_set(str, hent->h_name);
	}
	return sp;
    }

    if (hent) {
#ifndef lint
	(void)astore(ary, ++sp, str = str_mortal(&str_no));
	str_set(str, hent->h_name);
	(void)astore(ary, ++sp, str = str_mortal(&str_no));
	for (elem = hent->h_aliases; *elem; elem++) {
	    str_cat(str, *elem);
	    if (elem[1])
		str_ncat(str," ",1);
	}
	(void)astore(ary, ++sp, str = str_mortal(&str_no));
	str_numset(str, (double)hent->h_addrtype);
	(void)astore(ary, ++sp, str = str_mortal(&str_no));
	len = hent->h_length;
	str_numset(str, (double)len);
#ifdef h_addr
	for (elem = hent->h_addr_list; *elem; elem++) {
	    (void)astore(ary, ++sp, str = str_mortal(&str_no));
	    str_nset(str, *elem, len);
	}
#else
	(void)astore(ary, ++sp, str = str_mortal(&str_no));
	str_nset(str, hent->h_addr, len);
#endif /* h_addr */
#else /* lint */
	elem = Nullch;
	elem = elem;
	(void)astore(ary, ++sp, str_mortal(&str_no));
#endif /* lint */
    }

    return sp;
}

int
do_gnent(which,gimme,arglast)
int which;
int gimme;
int *arglast;
{
    register ARRAY *ary = stack;
    register int sp = arglast[0];
    register char **elem;
    register STR *str;
    struct netent *getnetbyname();
    struct netent *getnetbyaddr();
    struct netent *getnetent();
    struct netent *nent;

    if (which == O_GNBYNAME) {
	char *name = str_get(ary->ary_array[sp+1]);

	nent = getnetbyname(name);
    }
    else if (which == O_GNBYADDR) {
	unsigned long addr = U_L(str_gnum(ary->ary_array[sp+1]));
	int addrtype = (int)str_gnum(ary->ary_array[sp+2]);

	nent = getnetbyaddr((long)addr,addrtype);
    }
    else
	nent = getnetent();

    if (gimme != G_ARRAY) {
	astore(ary, ++sp, str = str_mortal(&str_undef));
	if (nent) {
	    if (which == O_GNBYNAME)
		str_numset(str, (double)nent->n_net);
	    else
		str_set(str, nent->n_name);
	}
	return sp;
    }

    if (nent) {
#ifndef lint
	(void)astore(ary, ++sp, str = str_mortal(&str_no));
	str_set(str, nent->n_name);
	(void)astore(ary, ++sp, str = str_mortal(&str_no));
	for (elem = nent->n_aliases; *elem; elem++) {
	    str_cat(str, *elem);
	    if (elem[1])
		str_ncat(str," ",1);
	}
	(void)astore(ary, ++sp, str = str_mortal(&str_no));
	str_numset(str, (double)nent->n_addrtype);
	(void)astore(ary, ++sp, str = str_mortal(&str_no));
	str_numset(str, (double)nent->n_net);
#else /* lint */
	elem = Nullch;
	elem = elem;
	(void)astore(ary, ++sp, str_mortal(&str_no));
#endif /* lint */
    }

    return sp;
}

int
do_gpent(which,gimme,arglast)
int which;
int gimme;
int *arglast;
{
    register ARRAY *ary = stack;
    register int sp = arglast[0];
    register char **elem;
    register STR *str;
    struct protoent *getprotobyname();
    struct protoent *getprotobynumber();
    struct protoent *getprotoent();
    struct protoent *pent;

    if (which == O_GPBYNAME) {
	char *name = str_get(ary->ary_array[sp+1]);

	pent = getprotobyname(name);
    }
    else if (which == O_GPBYNUMBER) {
	int proto = (int)str_gnum(ary->ary_array[sp+1]);

	pent = getprotobynumber(proto);
    }
    else
	pent = getprotoent();

    if (gimme != G_ARRAY) {
	astore(ary, ++sp, str = str_mortal(&str_undef));
	if (pent) {
	    if (which == O_GPBYNAME)
		str_numset(str, (double)pent->p_proto);
	    else
		str_set(str, pent->p_name);
	}
	return sp;
    }

    if (pent) {
#ifndef lint
	(void)astore(ary, ++sp, str = str_mortal(&str_no));
	str_set(str, pent->p_name);
	(void)astore(ary, ++sp, str = str_mortal(&str_no));
	for (elem = pent->p_aliases; *elem; elem++) {
	    str_cat(str, *elem);
	    if (elem[1])
		str_ncat(str," ",1);
	}
	(void)astore(ary, ++sp, str = str_mortal(&str_no));
	str_numset(str, (double)pent->p_proto);
#else /* lint */
	elem = Nullch;
	elem = elem;
	(void)astore(ary, ++sp, str_mortal(&str_no));
#endif /* lint */
    }

    return sp;
}

int
do_gsent(which,gimme,arglast)
int which;
int gimme;
int *arglast;
{
    register ARRAY *ary = stack;
    register int sp = arglast[0];
    register char **elem;
    register STR *str;
    struct servent *getservbyname();
    struct servent *getservbynumber();
    struct servent *getservent();
    struct servent *sent;

    if (which == O_GSBYNAME) {
	char *name = str_get(ary->ary_array[sp+1]);
	char *proto = str_get(ary->ary_array[sp+2]);

	if (proto && !*proto)
	    proto = Nullch;

	sent = getservbyname(name,proto);
    }
    else if (which == O_GSBYPORT) {
	int port = (int)str_gnum(ary->ary_array[sp+1]);
	char *proto = str_get(ary->ary_array[sp+2]);

	sent = getservbyport(port,proto);
    }
    else
	sent = getservent();

    if (gimme != G_ARRAY) {
	astore(ary, ++sp, str = str_mortal(&str_undef));
	if (sent) {
	    if (which == O_GSBYNAME) {
#ifdef HAS_NTOHS
		str_numset(str, (double)ntohs(sent->s_port));
#else
		str_numset(str, (double)(sent->s_port));
#endif
	    }
	    else
		str_set(str, sent->s_name);
	}
	return sp;
    }

    if (sent) {
#ifndef lint
	(void)astore(ary, ++sp, str = str_mortal(&str_no));
	str_set(str, sent->s_name);
	(void)astore(ary, ++sp, str = str_mortal(&str_no));
	for (elem = sent->s_aliases; *elem; elem++) {
	    str_cat(str, *elem);
	    if (elem[1])
		str_ncat(str," ",1);
	}
	(void)astore(ary, ++sp, str = str_mortal(&str_no));
#ifdef HAS_NTOHS
	str_numset(str, (double)ntohs(sent->s_port));
#else
	str_numset(str, (double)(sent->s_port));
#endif
	(void)astore(ary, ++sp, str = str_mortal(&str_no));
	str_set(str, sent->s_proto);
#else /* lint */
	elem = Nullch;
	elem = elem;
	(void)astore(ary, ++sp, str_mortal(&str_no));
#endif /* lint */
    }

    return sp;
}

#endif /* HAS_SOCKET */

#ifdef HAS_SELECT
int
do_select(gimme,arglast)
int gimme;
int *arglast;
{
    register STR **st = stack->ary_array;
    register int sp = arglast[0];
    register int i;
    register int j;
    register char *s;
    register STR *str;
    double value;
    int maxlen = 0;
    int nfound;
    struct timeval timebuf;
    struct timeval *tbuf = &timebuf;
    int growsize;
#if BYTEORDER != 0x1234 && BYTEORDER != 0x12345678
    int masksize;
    int offset;
    char *fd_sets[4];
    int k;

#if BYTEORDER & 0xf0000
#define ORDERBYTE (0x88888888 - BYTEORDER)
#else
#define ORDERBYTE (0x4444 - BYTEORDER)
#endif

#endif

    for (i = 1; i <= 3; i++) {
	j = st[sp+i]->str_cur;
	if (maxlen < j)
	    maxlen = j;
    }

#if BYTEORDER == 0x1234 || BYTEORDER == 0x12345678
    growsize = maxlen;		/* little endians can use vecs directly */
#else
#ifdef NFDBITS

#ifndef NBBY
#define NBBY 8
#endif

    masksize = NFDBITS / NBBY;
#else
    masksize = sizeof(long);	/* documented int, everyone seems to use long */
#endif
    growsize = maxlen + (masksize - (maxlen % masksize));
    Zero(&fd_sets[0], 4, char*);
#endif

    for (i = 1; i <= 3; i++) {
	str = st[sp+i];
	j = str->str_len;
	if (j < growsize) {
	    if (str->str_pok) {
		Str_Grow(str,growsize);
		s = str_get(str) + j;
		while (++j <= growsize) {
		    *s++ = '\0';
		}
	    }
	    else if (str->str_ptr) {
		Safefree(str->str_ptr);
		str->str_ptr = Nullch;
	    }
	}
#if BYTEORDER != 0x1234 && BYTEORDER != 0x12345678
	s = str->str_ptr;
	if (s) {
	    New(403, fd_sets[i], growsize, char);
	    for (offset = 0; offset < growsize; offset += masksize) {
		for (j = 0, k=ORDERBYTE; j < masksize; j++, (k >>= 4))
		    fd_sets[i][j+offset] = s[(k % masksize) + offset];
	    }
	}
#endif
    }
    str = st[sp+4];
    if (str->str_nok || str->str_pok) {
	value = str_gnum(str);
	if (value < 0.0)
	    value = 0.0;
	timebuf.tv_sec = (long)value;
	value -= (double)timebuf.tv_sec;
	timebuf.tv_usec = (long)(value * 1000000.0);
    }
    else
	tbuf = Null(struct timeval*);

#if BYTEORDER == 0x1234 || BYTEORDER == 0x12345678
    nfound = select(
	maxlen * 8,
	(fd_set *) st[sp+1]->str_ptr,
	(fd_set *) st[sp+2]->str_ptr,
	(fd_set *) st[sp+3]->str_ptr,
	tbuf);
#else
    nfound = select(
	maxlen * 8,
	fd_sets[1],
	fd_sets[2],
	fd_sets[3],
	tbuf);
    for (i = 1; i <= 3; i++) {
	if (fd_sets[i]) {
	    str = st[sp+i];
	    s = str->str_ptr;
	    for (offset = 0; offset < growsize; offset += masksize) {
		for (j = 0, k=ORDERBYTE; j < masksize; j++, (k >>= 4))
		    s[(k % masksize) + offset] = fd_sets[i][j+offset];
	    }
	    Safefree(fd_sets[i]);
	}
    }
#endif

    st[++sp] = str_mortal(&str_no);
    str_numset(st[sp], (double)nfound);
    if (gimme == G_ARRAY && tbuf) {
	value = (double)(timebuf.tv_sec) +
		(double)(timebuf.tv_usec) / 1000000.0;
	st[++sp] = str_mortal(&str_no);
	str_numset(st[sp], value);
    }
    return sp;
}
#endif /* SELECT */

#ifdef HAS_SOCKET
int
do_spair(stab1, stab2, arglast)
STAB *stab1;
STAB *stab2;
int *arglast;
{
    register STR **st = stack->ary_array;
    register int sp = arglast[2];
    register STIO *stio1;
    register STIO *stio2;
    int domain, type, protocol, fd[2];

    if (!stab1 || !stab2)
	return FALSE;

    stio1 = stab_io(stab1);
    stio2 = stab_io(stab2);
    if (!stio1)
	stio1 = stab_io(stab1) = stio_new();
    else if (stio1->ifp)
	do_close(stab1,FALSE);
    if (!stio2)
	stio2 = stab_io(stab2) = stio_new();
    else if (stio2->ifp)
	do_close(stab2,FALSE);

    domain = (int)str_gnum(st[++sp]);
    type = (int)str_gnum(st[++sp]);
    protocol = (int)str_gnum(st[++sp]);
#ifdef TAINT
    taintproper("Insecure dependency in socketpair");
#endif
#ifdef HAS_SOCKETPAIR
    if (socketpair(domain,type,protocol,fd) < 0)
	return FALSE;
#else
    fatal("Socketpair unimplemented");
#endif
    stio1->ifp = fdopen(fd[0], "r");
    stio1->ofp = fdopen(fd[0], "w");
    stio1->type = 's';
    stio2->ifp = fdopen(fd[1], "r");
    stio2->ofp = fdopen(fd[1], "w");
    stio2->type = 's';
    if (!stio1->ifp || !stio1->ofp || !stio2->ifp || !stio2->ofp) {
	if (stio1->ifp) fclose(stio1->ifp);
	if (stio1->ofp) fclose(stio1->ofp);
	if (!stio1->ifp && !stio1->ofp) close(fd[0]);
	if (stio2->ifp) fclose(stio2->ifp);
	if (stio2->ofp) fclose(stio2->ofp);
	if (!stio2->ifp && !stio2->ofp) close(fd[1]);
	return FALSE;
    }

    return TRUE;
}

#endif /* HAS_SOCKET */

int
do_gpwent(which,gimme,arglast)
int which;
int gimme;
int *arglast;
{
#ifdef I_PWD
    register ARRAY *ary = stack;
    register int sp = arglast[0];
    register STR *str;
    struct passwd *getpwnam();
    struct passwd *getpwuid();
    struct passwd *getpwent();
    struct passwd *pwent;

    if (which == O_GPWNAM) {
	char *name = str_get(ary->ary_array[sp+1]);

	pwent = getpwnam(name);
    }
    else if (which == O_GPWUID) {
	int uid = (int)str_gnum(ary->ary_array[sp+1]);

	pwent = getpwuid(uid);
    }
    else
	pwent = getpwent();

    if (gimme != G_ARRAY) {
	astore(ary, ++sp, str = str_mortal(&str_undef));
	if (pwent) {
	    if (which == O_GPWNAM)
		str_numset(str, (double)pwent->pw_uid);
	    else
		str_set(str, pwent->pw_name);
	}
	return sp;
    }

    if (pwent) {
	(void)astore(ary, ++sp, str = str_mortal(&str_no));
	str_set(str, pwent->pw_name);
	(void)astore(ary, ++sp, str = str_mortal(&str_no));
	str_set(str, pwent->pw_passwd);
	(void)astore(ary, ++sp, str = str_mortal(&str_no));
	str_numset(str, (double)pwent->pw_uid);
	(void)astore(ary, ++sp, str = str_mortal(&str_no));
	str_numset(str, (double)pwent->pw_gid);
	(void)astore(ary, ++sp, str = str_mortal(&str_no));
#ifdef PWCHANGE
	str_numset(str, (double)pwent->pw_change);
#else
#ifdef PWQUOTA
	str_numset(str, (double)pwent->pw_quota);
#else
#ifdef PWAGE
	str_set(str, pwent->pw_age);
#endif
#endif
#endif
	(void)astore(ary, ++sp, str = str_mortal(&str_no));
#ifdef PWCLASS
	str_set(str,pwent->pw_class);
#else
#ifdef PWCOMMENT
	str_set(str, pwent->pw_comment);
#endif
#endif
	(void)astore(ary, ++sp, str = str_mortal(&str_no));
	str_set(str, pwent->pw_gecos);
	(void)astore(ary, ++sp, str = str_mortal(&str_no));
	str_set(str, pwent->pw_dir);
	(void)astore(ary, ++sp, str = str_mortal(&str_no));
	str_set(str, pwent->pw_shell);
#ifdef PWEXPIRE
	(void)astore(ary, ++sp, str = str_mortal(&str_no));
	str_numset(str, (double)pwent->pw_expire);
#endif
    }

    return sp;
#else
    fatal("password routines not implemented");
#endif
}

int
do_ggrent(which,gimme,arglast)
int which;
int gimme;
int *arglast;
{
#ifdef I_GRP
    register ARRAY *ary = stack;
    register int sp = arglast[0];
    register char **elem;
    register STR *str;
    struct group *getgrnam();
    struct group *getgrgid();
    struct group *getgrent();
    struct group *grent;

    if (which == O_GGRNAM) {
	char *name = str_get(ary->ary_array[sp+1]);

	grent = getgrnam(name);
    }
    else if (which == O_GGRGID) {
	int gid = (int)str_gnum(ary->ary_array[sp+1]);

	grent = getgrgid(gid);
    }
    else
	grent = getgrent();

    if (gimme != G_ARRAY) {
	astore(ary, ++sp, str = str_mortal(&str_undef));
	if (grent) {
	    if (which == O_GGRNAM)
		str_numset(str, (double)grent->gr_gid);
	    else
		str_set(str, grent->gr_name);
	}
	return sp;
    }

    if (grent) {
	(void)astore(ary, ++sp, str = str_mortal(&str_no));
	str_set(str, grent->gr_name);
	(void)astore(ary, ++sp, str = str_mortal(&str_no));
	str_set(str, grent->gr_passwd);
	(void)astore(ary, ++sp, str = str_mortal(&str_no));
	str_numset(str, (double)grent->gr_gid);
	(void)astore(ary, ++sp, str = str_mortal(&str_no));
	for (elem = grent->gr_mem; *elem; elem++) {
	    str_cat(str, *elem);
	    if (elem[1])
		str_ncat(str," ",1);
	}
    }

    return sp;
#else
    fatal("group routines not implemented");
#endif
}

int
do_dirop(optype,stab,gimme,arglast)
int optype;
STAB *stab;
int gimme;
int *arglast;
{
#if defined(DIRENT) && defined(HAS_READDIR)
    register ARRAY *ary = stack;
    register STR **st = ary->ary_array;
    register int sp = arglast[1];
    register STIO *stio;
    long along;
#ifndef apollo
    struct DIRENT *readdir();
#endif
    register struct DIRENT *dp;

    if (!stab)
	goto nope;
    if (!(stio = stab_io(stab)))
	stio = stab_io(stab) = stio_new();
    if (!stio->dirp && optype != O_OPEN_DIR)
	goto nope;
    st[sp] = &str_yes;
    switch (optype) {
    case O_OPEN_DIR:
	if (stio->dirp)
	    closedir(stio->dirp);
	if (!(stio->dirp = opendir(str_get(st[sp+1]))))
	    goto nope;
	break;
    case O_READDIR:
	if (gimme == G_ARRAY) {
	    --sp;
	    /*SUPPRESS 560*/
	    while (dp = readdir(stio->dirp)) {
#ifdef DIRNAMLEN
		(void)astore(ary,++sp,
		  str_2mortal(str_make(dp->d_name,dp->d_namlen)));
#else
		(void)astore(ary,++sp,
		  str_2mortal(str_make(dp->d_name,0)));
#endif
	    }
	}
	else {
	    if (!(dp = readdir(stio->dirp)))
		goto nope;
	    st[sp] = str_mortal(&str_undef);
#ifdef DIRNAMLEN
	    str_nset(st[sp], dp->d_name, dp->d_namlen);
#else
	    str_set(st[sp], dp->d_name);
#endif
	}
	break;
#if defined(HAS_TELLDIR) || defined(telldir)
    case O_TELLDIR: {
#ifndef telldir
	    long telldir();
#endif
	    st[sp] = str_mortal(&str_undef);
	    str_numset(st[sp], (double)telldir(stio->dirp));
	    break;
	}
#endif
#if defined(HAS_SEEKDIR) || defined(seekdir)
    case O_SEEKDIR:
	st[sp] = str_mortal(&str_undef);
	along = (long)str_gnum(st[sp+1]);
	(void)seekdir(stio->dirp,along);
	break;
#endif
#if defined(HAS_REWINDDIR) || defined(rewinddir)
    case O_REWINDDIR:
	st[sp] = str_mortal(&str_undef);
	(void)rewinddir(stio->dirp);
	break;
#endif
    case O_CLOSEDIR:
	st[sp] = str_mortal(&str_undef);
	(void)closedir(stio->dirp);
	stio->dirp = 0;
	break;
    default:
	goto phooey;
    }
    return sp;

nope:
    st[sp] = &str_undef;
    if (!errno)
	errno = EBADF;
    return sp;

#endif
phooey:
    fatal("Unimplemented directory operation");
}

int
apply(type,arglast)
int type;
int *arglast;
{
    register STR **st = stack->ary_array;
    register int sp = arglast[1];
    register int items = arglast[2] - sp;
    register int val;
    register int val2;
    register int tot = 0;
    char *s;

#ifdef TAINT
    for (st += ++sp; items--; st++)
	tainted |= (*st)->str_tainted;
    st = stack->ary_array;
    sp = arglast[1];
    items = arglast[2] - sp;
#endif
    switch (type) {
    case O_CHMOD:
#ifdef TAINT
	taintproper("Insecure dependency in chmod");
#endif
	if (--items > 0) {
	    tot = items;
	    val = (int)str_gnum(st[++sp]);
	    while (items--) {
		if (chmod(str_get(st[++sp]),val))
		    tot--;
	    }
	}
	break;
#ifdef HAS_CHOWN
    case O_CHOWN:
#ifdef TAINT
	taintproper("Insecure dependency in chown");
#endif
	if (items > 2) {
	    items -= 2;
	    tot = items;
	    val = (int)str_gnum(st[++sp]);
	    val2 = (int)str_gnum(st[++sp]);
	    while (items--) {
		if (chown(str_get(st[++sp]),val,val2))
		    tot--;
	    }
	}
	break;
#endif
#ifdef HAS_KILL
    case O_KILL:
#ifdef TAINT
	taintproper("Insecure dependency in kill");
#endif
	if (--items > 0) {
	    tot = items;
	    s = str_get(st[++sp]);
	    if (isUPPER(*s)) {
		if (*s == 'S' && s[1] == 'I' && s[2] == 'G')
		    s += 3;
		if (!(val = whichsig(s)))
		    fatal("Unrecognized signal name \"%s\"",s);
	    }
	    else
		val = (int)str_gnum(st[sp]);
	    if (val < 0) {
		val = -val;
		while (items--) {
		    int proc = (int)str_gnum(st[++sp]);
#ifdef HAS_KILLPG
		    if (killpg(proc,val))	/* BSD */
#else
		    if (kill(-proc,val))	/* SYSV */
#endif
			tot--;
		}
	    }
	    else {
		while (items--) {
		    if (kill((int)(str_gnum(st[++sp])),val))
			tot--;
		}
	    }
	}
	break;
#endif
    case O_UNLINK:
#ifdef TAINT
	taintproper("Insecure dependency in unlink");
#endif
	tot = items;
	while (items--) {
	    s = str_get(st[++sp]);
	    if (euid || unsafe) {
		if (UNLINK(s))
		    tot--;
	    }
	    else {	/* don't let root wipe out directories without -U */
#ifdef HAS_LSTAT
		if (lstat(s,&statbuf) < 0 || S_ISDIR(statbuf.st_mode))
#else
		if (stat(s,&statbuf) < 0 || S_ISDIR(statbuf.st_mode))
#endif
		    tot--;
		else {
		    if (UNLINK(s))
			tot--;
		}
	    }
	}
	break;
    case O_UTIME:
#ifdef TAINT
	taintproper("Insecure dependency in utime");
#endif
	if (items > 2) {
#ifdef I_UTIME
	    struct utimbuf utbuf;
#else
	    struct {
		long    actime;
		long	modtime;
	    } utbuf;
#endif

	    Zero(&utbuf, sizeof utbuf, char);
	    utbuf.actime = (long)str_gnum(st[++sp]);    /* time accessed */
	    utbuf.modtime = (long)str_gnum(st[++sp]);    /* time modified */
	    items -= 2;
#ifndef lint
	    tot = items;
	    while (items--) {
		if (utime(str_get(st[++sp]),&utbuf))
		    tot--;
	    }
#endif
	}
	else
	    items = 0;
	break;
    }
    return tot;
}

/* Do the permissions allow some operation?  Assumes statcache already set. */

int
cando(bit, effective, statbufp)
int bit;
int effective;
register struct stat *statbufp;
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

#else /* ! MSDOS */
    if ((effective ? euid : uid) == 0) {	/* root is special */
	if (bit == S_IXUSR) {
	    if (statbufp->st_mode & 0111 || S_ISDIR(statbufp->st_mode))
		return TRUE;
	}
	else
	    return TRUE;		/* root reads and writes anything */
	return FALSE;
    }
    if (statbufp->st_uid == (effective ? euid : uid) ) {
	if (statbufp->st_mode & bit)
	    return TRUE;	/* ok as "user" */
    }
    else if (ingroup((int)statbufp->st_gid,effective)) {
	if (statbufp->st_mode & bit >> 3)
	    return TRUE;	/* ok as "group" */
    }
    else if (statbufp->st_mode & bit >> 6)
	return TRUE;	/* ok as "other" */
    return FALSE;
#endif /* ! MSDOS */
}

int
ingroup(testgid,effective)
int testgid;
int effective;
{
    if (testgid == (effective ? egid : gid))
	return TRUE;
#ifdef HAS_GETGROUPS
#ifndef NGROUPS
#define NGROUPS 32
#endif
    {
	GROUPSTYPE gary[NGROUPS];
	int anum;

	anum = getgroups(NGROUPS,gary);
	while (--anum >= 0)
	    if (gary[anum] == testgid)
		return TRUE;
    }
#endif
    return FALSE;
}

#if defined(HAS_MSG) || defined(HAS_SEM) || defined(HAS_SHM)

int
do_ipcget(optype, arglast)
int optype;
int *arglast;
{
    register STR **st = stack->ary_array;
    register int sp = arglast[0];
    key_t key;
    int n, flags;

    key = (key_t)str_gnum(st[++sp]);
    n = (optype == O_MSGGET) ? 0 : (int)str_gnum(st[++sp]);
    flags = (int)str_gnum(st[++sp]);
    errno = 0;
    switch (optype)
    {
#ifdef HAS_MSG
    case O_MSGGET:
	return msgget(key, flags);
#endif
#ifdef HAS_SEM
    case O_SEMGET:
	return semget(key, n, flags);
#endif
#ifdef HAS_SHM
    case O_SHMGET:
	return shmget(key, n, flags);
#endif
#if !defined(HAS_MSG) || !defined(HAS_SEM) || !defined(HAS_SHM)
    default:
	fatal("%s not implemented", opname[optype]);
#endif
    }
    return -1;			/* should never happen */
}

int
do_ipcctl(optype, arglast)
int optype;
int *arglast;
{
    register STR **st = stack->ary_array;
    register int sp = arglast[0];
    STR *astr;
    char *a;
    int id, n, cmd, infosize, getinfo, ret;

    id = (int)str_gnum(st[++sp]);
    n = (optype == O_SEMCTL) ? (int)str_gnum(st[++sp]) : 0;
    cmd = (int)str_gnum(st[++sp]);
    astr = st[++sp];

    infosize = 0;
    getinfo = (cmd == IPC_STAT);

    switch (optype)
    {
#ifdef HAS_MSG
    case O_MSGCTL:
	if (cmd == IPC_STAT || cmd == IPC_SET)
	    infosize = sizeof(struct msqid_ds);
	break;
#endif
#ifdef HAS_SHM
    case O_SHMCTL:
	if (cmd == IPC_STAT || cmd == IPC_SET)
	    infosize = sizeof(struct shmid_ds);
	break;
#endif
#ifdef HAS_SEM
    case O_SEMCTL:
	if (cmd == IPC_STAT || cmd == IPC_SET)
	    infosize = sizeof(struct semid_ds);
	else if (cmd == GETALL || cmd == SETALL)
	{
	    struct semid_ds semds;
	    if (semctl(id, 0, IPC_STAT, (union semun)&semds) == -1)
		return -1;
	    getinfo = (cmd == GETALL);
	    infosize = semds.sem_nsems * sizeof(short);
		/* "short" is technically wrong but much more portable
		   than guessing about u_?short(_t)? */
	}
	break;
#endif
#if !defined(HAS_MSG) || !defined(HAS_SEM) || !defined(HAS_SHM)
    default:
	fatal("%s not implemented", opname[optype]);
#endif
    }

    if (infosize)
    {
	if (getinfo)
	{
	    STR_GROW(astr, infosize+1);
	    a = str_get(astr);
	}
	else
	{
	    a = str_get(astr);
	    if (astr->str_cur != infosize)
	    {
		errno = EINVAL;
		return -1;
	    }
	}
    }
    else
    {
	int i = (int)str_gnum(astr);
	a = (char *)i;		/* ouch */
    }
    errno = 0;
    switch (optype)
    {
#ifdef HAS_MSG
    case O_MSGCTL:
	ret = msgctl(id, cmd, (struct msqid_ds *)a);
	break;
#endif
#ifdef HAS_SEM
    case O_SEMCTL:
	ret = semctl(id, n, cmd, (union semun)((int)a));
	break;
#endif
#ifdef HAS_SHM
    case O_SHMCTL:
	ret = shmctl(id, cmd, (struct shmid_ds *)a);
	break;
#endif
    }
    if (getinfo && ret >= 0) {
	astr->str_cur = infosize;
	astr->str_ptr[infosize] = '\0';
    }
    return ret;
}

int
do_msgsnd(arglast)
int *arglast;
{
#ifdef HAS_MSG
    register STR **st = stack->ary_array;
    register int sp = arglast[0];
    STR *mstr;
    char *mbuf;
    int id, msize, flags;

    id = (int)str_gnum(st[++sp]);
    mstr = st[++sp];
    flags = (int)str_gnum(st[++sp]);
    mbuf = str_get(mstr);
    if ((msize = mstr->str_cur - sizeof(long)) < 0) {
	errno = EINVAL;
	return -1;
    }
    errno = 0;
    return msgsnd(id, (struct msgbuf *)mbuf, msize, flags);
#else
    fatal("msgsnd not implemented");
#endif
}

int
do_msgrcv(arglast)
int *arglast;
{
#ifdef HAS_MSG
    register STR **st = stack->ary_array;
    register int sp = arglast[0];
    STR *mstr;
    char *mbuf;
    long mtype;
    int id, msize, flags, ret;

    id = (int)str_gnum(st[++sp]);
    mstr = st[++sp];
    msize = (int)str_gnum(st[++sp]);
    mtype = (long)str_gnum(st[++sp]);
    flags = (int)str_gnum(st[++sp]);
    mbuf = str_get(mstr);
    if (mstr->str_cur < sizeof(long)+msize+1) {
	STR_GROW(mstr, sizeof(long)+msize+1);
	mbuf = str_get(mstr);
    }
    errno = 0;
    ret = msgrcv(id, (struct msgbuf *)mbuf, msize, mtype, flags);
    if (ret >= 0) {
	mstr->str_cur = sizeof(long)+ret;
	mstr->str_ptr[sizeof(long)+ret] = '\0';
    }
    return ret;
#else
    fatal("msgrcv not implemented");
#endif
}

int
do_semop(arglast)
int *arglast;
{
#ifdef HAS_SEM
    register STR **st = stack->ary_array;
    register int sp = arglast[0];
    STR *opstr;
    char *opbuf;
    int id, opsize;

    id = (int)str_gnum(st[++sp]);
    opstr = st[++sp];
    opbuf = str_get(opstr);
    opsize = opstr->str_cur;
    if (opsize < sizeof(struct sembuf)
	|| (opsize % sizeof(struct sembuf)) != 0) {
	errno = EINVAL;
	return -1;
    }
    errno = 0;
    return semop(id, (struct sembuf *)opbuf, opsize/sizeof(struct sembuf));
#else
    fatal("semop not implemented");
#endif
}

int
do_shmio(optype, arglast)
int optype;
int *arglast;
{
#ifdef HAS_SHM
    register STR **st = stack->ary_array;
    register int sp = arglast[0];
    STR *mstr;
    char *mbuf, *shm;
    int id, mpos, msize;
    struct shmid_ds shmds;
#ifndef VOIDSHMAT
    extern char *shmat();
#endif

    id = (int)str_gnum(st[++sp]);
    mstr = st[++sp];
    mpos = (int)str_gnum(st[++sp]);
    msize = (int)str_gnum(st[++sp]);
    errno = 0;
    if (shmctl(id, IPC_STAT, &shmds) == -1)
	return -1;
    if (mpos < 0 || msize < 0 || mpos + msize > shmds.shm_segsz) {
	errno = EFAULT;		/* can't do as caller requested */
	return -1;
    }
    shm = (char*)shmat(id, (char*)NULL, (optype == O_SHMREAD) ? SHM_RDONLY : 0);
    if (shm == (char *)-1)	/* I hate System V IPC, I really do */
	return -1;
    mbuf = str_get(mstr);
    if (optype == O_SHMREAD) {
	if (mstr->str_cur < msize) {
	    STR_GROW(mstr, msize+1);
	    mbuf = str_get(mstr);
	}
	Copy(shm + mpos, mbuf, msize, char);
	mstr->str_cur = msize;
	mstr->str_ptr[msize] = '\0';
    }
    else {
	int n;

	if ((n = mstr->str_cur) > msize)
	    n = msize;
	Copy(mbuf, shm + mpos, n, char);
	if (n < msize)
	    memzero(shm + mpos + n, msize - n);
    }
    return shmdt(shm);
#else
    fatal("shm I/O not implemented");
#endif
}

#endif /* SYSV IPC */
