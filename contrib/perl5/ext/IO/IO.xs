/*
 * Copyright (c) 1997-8 Graham Barr <gbarr@pobox.com>. All rights reserved.
 * This program is free software; you can redistribute it and/or
 * modify it under the same terms as Perl itself.
 */

#define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#define PERLIO_NOT_STDIO 1
#include "perl.h"
#include "XSUB.h"
#include "poll.h"
#ifdef I_UNISTD
#  include <unistd.h>
#endif
#if defined(I_FCNTL) || defined(HAS_FCNTL)
#  include <fcntl.h>
#endif

#ifdef PerlIO
typedef int SysRet;
typedef PerlIO * InputStream;
typedef PerlIO * OutputStream;
#else
#define PERLIO_IS_STDIO 1
typedef int SysRet;
typedef FILE * InputStream;
typedef FILE * OutputStream;
#endif

#define MY_start_subparse(fmt,flags) start_subparse(fmt,flags)

#ifndef gv_stashpvn
#define gv_stashpvn(str,len,flags) gv_stashpv(str,flags)
#endif

static int
not_here(char *s)
{
    croak("%s not implemented on this architecture", s);
    return -1;
}


#ifndef PerlIO
#define PerlIO_fileno(f) fileno(f)
#endif

static int
io_blocking(InputStream f, int block)
{
    int RETVAL;
    if(!f) {
	errno = EBADF;
	return -1;
    }
#if defined(HAS_FCNTL)
    RETVAL = fcntl(PerlIO_fileno(f), F_GETFL, 0);
    if (RETVAL >= 0) {
	int mode = RETVAL;
#ifdef O_NONBLOCK
	/* POSIX style */ 
#if defined(O_NDELAY) && O_NDELAY != O_NONBLOCK
	/* Ooops has O_NDELAY too - make sure we don't 
	 * get SysV behaviour by mistake. */

	/* E.g. In UNICOS and UNICOS/mk a F_GETFL returns an O_NDELAY
	 * after a successful F_SETFL of an O_NONBLOCK. */
	RETVAL = RETVAL & (O_NONBLOCK | O_NDELAY) ? 0 : 1;

	if (block >= 0) {
	    if ((mode & O_NDELAY) || ((block == 0) && !(mode & O_NONBLOCK))) {
	        int ret;
	        mode = (mode & ~O_NDELAY) | O_NONBLOCK;
	        ret = fcntl(PerlIO_fileno(f),F_SETFL,mode);
	        if(ret < 0)
		    RETVAL = ret;
	    }
	    else
              if ((mode & O_NDELAY) || ((block > 0) && (mode & O_NONBLOCK))) {
	        int ret;
	        mode &= ~(O_NONBLOCK | O_NDELAY);
	        ret = fcntl(PerlIO_fileno(f),F_SETFL,mode);
	        if(ret < 0)
		    RETVAL = ret;
              }
	}
#else
	/* Standard POSIX */ 
	RETVAL = RETVAL & O_NONBLOCK ? 0 : 1;

	if ((block == 0) && !(mode & O_NONBLOCK)) {
	    int ret;
	    mode |= O_NONBLOCK;
	    ret = fcntl(PerlIO_fileno(f),F_SETFL,mode);
	    if(ret < 0)
		RETVAL = ret;
	 }
	else if ((block > 0) && (mode & O_NONBLOCK)) {
	    int ret;
	    mode &= ~O_NONBLOCK;
	    ret = fcntl(PerlIO_fileno(f),F_SETFL,mode);
	    if(ret < 0)
		RETVAL = ret;
	 }
#endif 
#else
	/* Not POSIX - better have O_NDELAY or we can't cope.
	 * for BSD-ish machines this is an acceptable alternative
	 * for SysV we can't tell "would block" from EOF but that is 
	 * the way SysV is...
	 */
	RETVAL = RETVAL & O_NDELAY ? 0 : 1;

	if ((block == 0) && !(mode & O_NDELAY)) {
	    int ret;
	    mode |= O_NDELAY;
	    ret = fcntl(PerlIO_fileno(f),F_SETFL,mode);
	    if(ret < 0)
		RETVAL = ret;
	 }
	else if ((block > 0) && (mode & O_NDELAY)) {
	    int ret;
	    mode &= ~O_NDELAY;
	    ret = fcntl(PerlIO_fileno(f),F_SETFL,mode);
	    if(ret < 0)
		RETVAL = ret;
	 }
#endif
    }
    return RETVAL;
#else
 return -1;
#endif
}

MODULE = IO	PACKAGE = IO::Seekable	PREFIX = f

void
fgetpos(handle)
	InputStream	handle
    CODE:
	if (handle) {
	    Fpos_t pos;
	    if (
#ifdef PerlIO
		PerlIO_getpos(handle, &pos)
#else
		fgetpos(handle, &pos)
#endif
		) {
		ST(0) = &PL_sv_undef;
	    } else {
		ST(0) = sv_2mortal(newSVpv((char*)&pos, sizeof(Fpos_t)));
	    }
	}
	else {
	    ST(0) = &PL_sv_undef;
	    errno = EINVAL;
	}

SysRet
fsetpos(handle, pos)
	InputStream	handle
	SV *		pos
    CODE:
        char *p;
	STRLEN len;
	if (handle && (p = SvPV(pos,len)) && len == sizeof(Fpos_t))
#ifdef PerlIO
	    RETVAL = PerlIO_setpos(handle, (Fpos_t*)p);
#else
	    RETVAL = fsetpos(handle, (Fpos_t*)p);
#endif
	else {
	    RETVAL = -1;
	    errno = EINVAL;
	}
    OUTPUT:
	RETVAL

MODULE = IO	PACKAGE = IO::File	PREFIX = f

void
new_tmpfile(packname = "IO::File")
    char *		packname
    PREINIT:
	OutputStream fp;
	GV *gv;
    CODE:
#ifdef PerlIO
	fp = PerlIO_tmpfile();
#else
	fp = tmpfile();
#endif
	gv = (GV*)SvREFCNT_inc(newGVgen(packname));
	hv_delete(GvSTASH(gv), GvNAME(gv), GvNAMELEN(gv), G_DISCARD);
	if (do_open(gv, "+>&", 3, FALSE, 0, 0, fp)) {
	    ST(0) = sv_2mortal(newRV((SV*)gv));
	    sv_bless(ST(0), gv_stashpv(packname, TRUE));
	    SvREFCNT_dec(gv);   /* undo increment in newRV() */
	}
	else {
	    ST(0) = &PL_sv_undef;
	    SvREFCNT_dec(gv);
	}

MODULE = IO	PACKAGE = IO::Poll

void   
_poll(timeout,...)
	int timeout;
PPCODE:
{
#ifdef HAS_POLL
    int nfd = (items - 1) / 2;
    SV *tmpsv = NEWSV(999,nfd * sizeof(struct pollfd));
    struct pollfd *fds = (struct pollfd *)SvPVX(tmpsv);
    int i,j,ret;
    for(i=1, j=0  ; j < nfd ; j++) {
	fds[j].fd = SvIV(ST(i));
	i++;
	fds[j].events = SvIV(ST(i));
	i++;
	fds[j].revents = 0;
    }
    if((ret = poll(fds,nfd,timeout)) >= 0) {
	for(i=1, j=0 ; j < nfd ; j++) {
	    sv_setiv(ST(i), fds[j].fd); i++;
	    sv_setiv(ST(i), fds[j].revents); i++;
	}
    }
    SvREFCNT_dec(tmpsv);
    XSRETURN_IV(ret);
#else
	not_here("IO::Poll::poll");
#endif
}

MODULE = IO	PACKAGE = IO::Handle	PREFIX = io_

void
io_blocking(handle,blk=-1)
	InputStream	handle
	int		blk
PROTOTYPE: $;$
CODE:
{
    int ret = io_blocking(handle, items == 1 ? -1 : blk ? 1 : 0);
    if(ret >= 0)
	XSRETURN_IV(ret);
    else
	XSRETURN_UNDEF;
}

MODULE = IO	PACKAGE = IO::Handle	PREFIX = f


int
ungetc(handle, c)
	InputStream	handle
	int		c
    CODE:
	if (handle)
#ifdef PerlIO
	    RETVAL = PerlIO_ungetc(handle, c);
#else
	    RETVAL = ungetc(c, handle);
#endif
	else {
	    RETVAL = -1;
	    errno = EINVAL;
	}
    OUTPUT:
	RETVAL

int
ferror(handle)
	InputStream	handle
    CODE:
	if (handle)
#ifdef PerlIO
	    RETVAL = PerlIO_error(handle);
#else
	    RETVAL = ferror(handle);
#endif
	else {
	    RETVAL = -1;
	    errno = EINVAL;
	}
    OUTPUT:
	RETVAL

int
clearerr(handle)
	InputStream	handle
    CODE:
	if (handle) {
#ifdef PerlIO
	    PerlIO_clearerr(handle);
#else
	    clearerr(handle);
#endif
	    RETVAL = 0;
	}
	else {
	    RETVAL = -1;
	    errno = EINVAL;
	}
    OUTPUT:
	RETVAL

int
untaint(handle)
       SV *	handle
    CODE:
#ifdef IOf_UNTAINT
	IO * io;
	io = sv_2io(handle);
	if (io) {
	    IoFLAGS(io) |= IOf_UNTAINT;
	    RETVAL = 0;
	}
        else {
#endif
	    RETVAL = -1;
	    errno = EINVAL;
#ifdef IOf_UNTAINT
	}
#endif
    OUTPUT:
	RETVAL

SysRet
fflush(handle)
	OutputStream	handle
    CODE:
	if (handle)
#ifdef PerlIO
	    RETVAL = PerlIO_flush(handle);
#else
	    RETVAL = Fflush(handle);
#endif
	else {
	    RETVAL = -1;
	    errno = EINVAL;
	}
    OUTPUT:
	RETVAL

void
setbuf(handle, buf)
	OutputStream	handle
	char *		buf = SvPOK(ST(1)) ? sv_grow(ST(1), BUFSIZ) : 0;
    CODE:
	if (handle)
#ifdef PERLIO_IS_STDIO
	    setbuf(handle, buf);
#else
	    not_here("IO::Handle::setbuf");
#endif

SysRet
setvbuf(handle, buf, type, size)
	OutputStream	handle
	char *		buf = SvPOK(ST(1)) ? sv_grow(ST(1), SvIV(ST(3))) : 0;
	int		type
	int		size
    CODE:
#if defined(PERLIO_IS_STDIO) && defined(_IOFBF) && defined(HAS_SETVBUF)
	if (!handle)			/* Try input stream. */
	    handle = IoIFP(sv_2io(ST(0)));
	if (handle)
	    RETVAL = setvbuf(handle, buf, type, size);
	else {
	    RETVAL = -1;
	    errno = EINVAL;
	}
#else
	RETVAL = (SysRet) not_here("IO::Handle::setvbuf");
#endif
    OUTPUT:
	RETVAL


SysRet
fsync(handle)
	OutputStream handle
    CODE:
#ifdef HAS_FSYNC
	if(handle)
	    RETVAL = fsync(PerlIO_fileno(handle));
	else {
	    RETVAL = -1;
	    errno = EINVAL;
	}
#else
	RETVAL = (SysRet) not_here("IO::Handle::sync");
#endif
    OUTPUT:
	RETVAL


BOOT:
{
    HV *stash;
    /*
     * constant subs for IO::Poll
     */
    stash = gv_stashpvn("IO::Poll", 8, TRUE);
#ifdef	POLLIN
	newCONSTSUB(stash,"POLLIN",newSViv(POLLIN));
#endif
#ifdef	POLLPRI
        newCONSTSUB(stash,"POLLPRI", newSViv(POLLPRI));
#endif
#ifdef	POLLOUT
        newCONSTSUB(stash,"POLLOUT", newSViv(POLLOUT));
#endif
#ifdef	POLLRDNORM
        newCONSTSUB(stash,"POLLRDNORM", newSViv(POLLRDNORM));
#endif
#ifdef	POLLWRNORM
        newCONSTSUB(stash,"POLLWRNORM", newSViv(POLLWRNORM));
#endif
#ifdef	POLLRDBAND
        newCONSTSUB(stash,"POLLRDBAND", newSViv(POLLRDBAND));
#endif
#ifdef	POLLWRBAND
        newCONSTSUB(stash,"POLLWRBAND", newSViv(POLLWRBAND));
#endif
#ifdef	POLLNORM
        newCONSTSUB(stash,"POLLNORM", newSViv(POLLNORM));
#endif
#ifdef	POLLERR
        newCONSTSUB(stash,"POLLERR", newSViv(POLLERR));
#endif
#ifdef	POLLHUP
        newCONSTSUB(stash,"POLLHUP", newSViv(POLLHUP));
#endif
#ifdef	POLLNVAL
        newCONSTSUB(stash,"POLLNVAL", newSViv(POLLNVAL));
#endif
    /*
     * constant subs for IO::Handle
     */
    stash = gv_stashpvn("IO::Handle", 10, TRUE);
#ifdef _IOFBF
        newCONSTSUB(stash,"_IOFBF", newSViv(_IOFBF));
#endif
#ifdef _IOLBF
        newCONSTSUB(stash,"_IOLBF", newSViv(_IOLBF));
#endif
#ifdef _IONBF
        newCONSTSUB(stash,"_IONBF", newSViv(_IONBF));
#endif
#ifdef SEEK_SET
        newCONSTSUB(stash,"SEEK_SET", newSViv(SEEK_SET));
#endif
#ifdef SEEK_CUR
        newCONSTSUB(stash,"SEEK_CUR", newSViv(SEEK_CUR));
#endif
#ifdef SEEK_END
        newCONSTSUB(stash,"SEEK_END", newSViv(SEEK_END));
#endif
}
