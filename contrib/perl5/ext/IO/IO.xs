#include "EXTERN.h"
#define PERLIO_NOT_STDIO 1
#include "perl.h"
#include "XSUB.h"

#ifdef I_UNISTD
#  include <unistd.h>
#endif
#ifdef I_FCNTL
#if defined(__GNUC__) && defined(__cplusplus) && defined(WIN32)
#define _NO_OLDNAMES
#endif 
#  include <fcntl.h>
#if defined(__GNUC__) && defined(__cplusplus) && defined(WIN32)
#undef _NO_OLDNAMES
#endif 

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

static int
not_here(char *s)
{
    croak("%s not implemented on this architecture", s);
    return -1;
}

static bool
constant(char *name, IV *pval)
{
    switch (*name) {
    case '_':
	if (strEQ(name, "_IOFBF"))
#ifdef _IOFBF
	    { *pval = _IOFBF; return TRUE; }
#else
	    return FALSE;
#endif
	if (strEQ(name, "_IOLBF"))
#ifdef _IOLBF
	    { *pval = _IOLBF; return TRUE; }
#else
	    return FALSE;
#endif
	if (strEQ(name, "_IONBF"))
#ifdef _IONBF
	    { *pval = _IONBF; return TRUE; }
#else
	    return FALSE;
#endif
	break;
    case 'S':
	if (strEQ(name, "SEEK_SET"))
#ifdef SEEK_SET
	    { *pval = SEEK_SET; return TRUE; }
#else
	    return FALSE;
#endif
	if (strEQ(name, "SEEK_CUR"))
#ifdef SEEK_CUR
	    { *pval = SEEK_CUR; return TRUE; }
#else
	    return FALSE;
#endif
	if (strEQ(name, "SEEK_END"))
#ifdef SEEK_END
	    { *pval = SEEK_END; return TRUE; }
#else
	    return FALSE;
#endif
	break;
    }

    return FALSE;
}


MODULE = IO	PACKAGE = IO::Seekable	PREFIX = f

SV *
fgetpos(handle)
	InputStream	handle
    CODE:
	if (handle) {
	    Fpos_t pos;
#ifdef PerlIO
	    PerlIO_getpos(handle, &pos);
#else
	    fgetpos(handle, &pos);
#endif
	    ST(0) = sv_2mortal(newSVpv((char*)&pos, sizeof(Fpos_t)));
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
	STRLEN n_a;
	if (handle && (p = SvPVx(pos, n_a)) && n_a == sizeof(Fpos_t))
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

SV *
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
	    SvREFCNT_dec(gv);	/* undo increment in newRV() */
	}
	else {
	    ST(0) = &PL_sv_undef;
	    SvREFCNT_dec(gv);
	}

MODULE = IO	PACKAGE = IO::Handle	PREFIX = f

SV *
constant(name)
	char *		name
    CODE:
	IV i;
	if (constant(name, &i))
	    ST(0) = sv_2mortal(newSViv(i));
	else
	    ST(0) = &PL_sv_undef;

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
/* Should check HAS_SETVBUF once Configure tests for that */
#if defined(PERLIO_IS_STDIO) && defined(_IOFBF)
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


