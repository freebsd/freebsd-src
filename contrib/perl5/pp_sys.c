/*    pp_sys.c
 *
 *    Copyright (c) 1991-1999, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * But only a short way ahead its floor and the walls on either side were
 * cloven by a great fissure, out of which the red glare came, now leaping
 * up, now dying down into darkness; and all the while far below there was
 * a rumour and a trouble as of great engines throbbing and labouring.
 */

#include "EXTERN.h"
#include "perl.h"

/* XXX If this causes problems, set i_unistd=undef in the hint file.  */
#ifdef I_UNISTD
# include <unistd.h>
#endif

#ifdef HAS_SYSCALL   
#ifdef __cplusplus              
extern "C" int syscall(unsigned long,...);
#endif
#endif

#ifdef I_SYS_WAIT
# include <sys/wait.h>
#endif

#ifdef I_SYS_RESOURCE
# include <sys/resource.h>
#endif

#if defined(HAS_SOCKET) && !defined(VMS) /* VMS handles sockets via vmsish.h */
# include <sys/socket.h>
# ifdef I_NETDB
#  include <netdb.h>
# endif
# ifndef ENOTSOCK
#  ifdef I_NET_ERRNO
#   include <net/errno.h>
#  endif
# endif
#endif

#ifdef HAS_SELECT
#ifdef I_SYS_SELECT
#include <sys/select.h>
#endif
#endif

/* XXX Configure test needed.
   h_errno might not be a simple 'int', especially for multi-threaded
   applications, see "extern int errno in perl.h".  Creating such
   a test requires taking into account the differences between
   compiling multithreaded and singlethreaded ($ccflags et al).
   HOST_NOT_FOUND is typically defined in <netdb.h>.
*/
#if defined(HOST_NOT_FOUND) && !defined(h_errno)
extern int h_errno;
#endif

#ifdef HAS_PASSWD
# ifdef I_PWD
#  include <pwd.h>
# else
    struct passwd *getpwnam _((char *));
    struct passwd *getpwuid _((Uid_t));
# endif
# ifdef HAS_GETPWENT
  struct passwd *getpwent _((void));
# endif
#endif

#ifdef HAS_GROUP
# ifdef I_GRP
#  include <grp.h>
# else
    struct group *getgrnam _((char *));
    struct group *getgrgid _((Gid_t));
# endif
# ifdef HAS_GETGRENT
    struct group *getgrent _((void));
# endif
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

/* Put this after #includes because fork and vfork prototypes may conflict. */
#ifndef HAS_VFORK
#   define vfork fork
#endif

/* Put this after #includes because <unistd.h> defines _XOPEN_*. */
#ifndef Sock_size_t
#  if _XOPEN_VERSION >= 5 || defined(_XOPEN_SOURCE_EXTENDED) || defined(__GLIBC__)
#    define Sock_size_t Size_t
#  else
#    define Sock_size_t int
#  endif
#endif

#if !defined(HAS_MKDIR) || !defined(HAS_RMDIR)
static int dooneliner _((char *cmd, char *filename));
#endif

#ifdef HAS_CHSIZE
# ifdef my_chsize  /* Probably #defined to Perl_my_chsize in embed.h */
#   undef my_chsize
# endif
# define my_chsize PerlLIO_chsize
#endif

#ifdef HAS_FLOCK
#  define FLOCK flock
#else /* no flock() */

   /* fcntl.h might not have been included, even if it exists, because
      the current Configure only sets I_FCNTL if it's needed to pick up
      the *_OK constants.  Make sure it has been included before testing
      the fcntl() locking constants. */
#  if defined(HAS_FCNTL) && !defined(I_FCNTL)
#    include <fcntl.h>
#  endif

#  if defined(HAS_FCNTL) && defined(F_SETLK) && defined (F_SETLKW)
#    define FLOCK fcntl_emulate_flock
#    define FCNTL_EMULATE_FLOCK
#  else /* no flock() or fcntl(F_SETLK,...) */
#    ifdef HAS_LOCKF
#      define FLOCK lockf_emulate_flock
#      define LOCKF_EMULATE_FLOCK
#    endif /* lockf */
#  endif /* no flock() or fcntl(F_SETLK,...) */

#  ifdef FLOCK
     static int FLOCK _((int, int));

    /*
     * These are the flock() constants.  Since this sytems doesn't have
     * flock(), the values of the constants are probably not available.
     */
#    ifndef LOCK_SH
#      define LOCK_SH 1
#    endif
#    ifndef LOCK_EX
#      define LOCK_EX 2
#    endif
#    ifndef LOCK_NB
#      define LOCK_NB 4
#    endif
#    ifndef LOCK_UN
#      define LOCK_UN 8
#    endif
#  endif /* emulating flock() */

#endif /* no flock() */

#ifndef MAXPATHLEN
#  ifdef PATH_MAX
#    define MAXPATHLEN PATH_MAX
#  else
#    define MAXPATHLEN 1024
#  endif
#endif

#define ZBTLEN 10
static char zero_but_true[ZBTLEN + 1] = "0 but true";

/* Pushy I/O. */

PP(pp_backtick)
{
    djSP; dTARGET;
    PerlIO *fp;
    STRLEN n_a;
    char *tmps = POPpx;
    I32 gimme = GIMME_V;

    TAINT_PROPER("``");
    fp = PerlProc_popen(tmps, "r");
    if (fp) {
	if (gimme == G_VOID) {
	    char tmpbuf[256];
	    while (PerlIO_read(fp, tmpbuf, sizeof tmpbuf) > 0)
		/*SUPPRESS 530*/
		;
	}
	else if (gimme == G_SCALAR) {
	    sv_setpv(TARG, "");	/* note that this preserves previous buffer */
	    while (sv_gets(TARG, fp, SvCUR(TARG)) != Nullch)
		/*SUPPRESS 530*/
		;
	    XPUSHs(TARG);
	    SvTAINTED_on(TARG);
	}
	else {
	    SV *sv;

	    for (;;) {
		sv = NEWSV(56, 79);
		if (sv_gets(sv, fp, 0) == Nullch) {
		    SvREFCNT_dec(sv);
		    break;
		}
		XPUSHs(sv_2mortal(sv));
		if (SvLEN(sv) - SvCUR(sv) > 20) {
		    SvLEN_set(sv, SvCUR(sv)+1);
		    Renew(SvPVX(sv), SvLEN(sv), char);
		}
		SvTAINTED_on(sv);
	    }
	}
	STATUS_NATIVE_SET(PerlProc_pclose(fp));
	TAINT;		/* "I believe that this is not gratuitous!" */
    }
    else {
	STATUS_NATIVE_SET(-1);
	if (gimme == G_SCALAR)
	    RETPUSHUNDEF;
    }

    RETURN;
}

PP(pp_glob)
{
    OP *result;
    ENTER;

#ifndef VMS
    if (PL_tainting) {
	/*
	 * The external globbing program may use things we can't control,
	 * so for security reasons we must assume the worst.
	 */
	TAINT;
	taint_proper(no_security, "glob");
    }
#endif /* !VMS */

    SAVESPTR(PL_last_in_gv);	/* We don't want this to be permanent. */
    PL_last_in_gv = (GV*)*PL_stack_sp--;

    SAVESPTR(PL_rs);		/* This is not permanent, either. */
    PL_rs = sv_2mortal(newSVpv("", 1));
#ifndef DOSISH
#ifndef CSH
    *SvPVX(PL_rs) = '\n';
#endif	/* !CSH */
#endif	/* !DOSISH */

    result = do_readline();
    LEAVE;
    return result;
}

#if 0		/* XXX never used! */
PP(pp_indread)
{
    STRLEN n_a;
    PL_last_in_gv = gv_fetchpv(SvPVx(GvSV((GV*)(*PL_stack_sp--)), n_a), TRUE,SVt_PVIO);
    return do_readline();
}
#endif

PP(pp_rcatline)
{
    PL_last_in_gv = cGVOP->op_gv;
    return do_readline();
}

PP(pp_warn)
{
    djSP; dMARK;
    char *tmps;
    STRLEN n_a;
    if (SP - MARK != 1) {
	dTARGET;
	do_join(TARG, &PL_sv_no, MARK, SP);
	tmps = SvPV(TARG, n_a);
	SP = MARK + 1;
    }
    else {
	tmps = SvPV(TOPs, n_a);
    }
    if (!tmps || !*tmps) {
  	SV *error = ERRSV;
	(void)SvUPGRADE(error, SVt_PV);
	if (SvPOK(error) && SvCUR(error))
	    sv_catpv(error, "\t...caught");
	tmps = SvPV(error, n_a);
    }
    if (!tmps || !*tmps)
	tmps = "Warning: something's wrong";
    warn("%s", tmps);
    RETSETYES;
}

PP(pp_die)
{
    djSP; dMARK;
    char *tmps;
    SV *tmpsv = Nullsv;
    char *pat = "%s";
    STRLEN n_a;
    if (SP - MARK != 1) {
	dTARGET;
	do_join(TARG, &PL_sv_no, MARK, SP);
	tmps = SvPV(TARG, n_a);
	SP = MARK + 1;
    }
    else {
	tmpsv = TOPs;
	tmps = SvROK(tmpsv) ? Nullch : SvPV(tmpsv, n_a);
    }
    if (!tmps || !*tmps) {
  	SV *error = ERRSV;
	(void)SvUPGRADE(error, SVt_PV);
	if(tmpsv ? SvROK(tmpsv) : SvROK(error)) {
	    if(tmpsv)
		SvSetSV(error,tmpsv);
	    else if(sv_isobject(error)) {
		HV *stash = SvSTASH(SvRV(error));
		GV *gv = gv_fetchmethod(stash, "PROPAGATE");
		if (gv) {
		    SV *file = sv_2mortal(newSVsv(GvSV(PL_curcop->cop_filegv)));
		    SV *line = sv_2mortal(newSViv(PL_curcop->cop_line));
		    EXTEND(SP, 3);
		    PUSHMARK(SP);
		    PUSHs(error);
		    PUSHs(file);
 		    PUSHs(line);
		    PUTBACK;
		    perl_call_sv((SV*)GvCV(gv),
				 G_SCALAR|G_EVAL|G_KEEPERR);
		    sv_setsv(error,*PL_stack_sp--);
		}
	    }
	    pat = Nullch;
	}
	else {
	    if (SvPOK(error) && SvCUR(error))
		sv_catpv(error, "\t...propagated");
	    tmps = SvPV(error, n_a);
	}
    }
    if (!tmps || !*tmps)
	tmps = "Died";
    DIE(pat, tmps);
}

/* I/O. */

PP(pp_open)
{
    djSP; dTARGET;
    GV *gv;
    SV *sv;
    char *tmps;
    STRLEN len;

    if (MAXARG > 1)
	sv = POPs;
    if (!isGV(TOPs))
	DIE(no_usym, "filehandle");
    if (MAXARG <= 1)
	sv = GvSV(TOPs);
    gv = (GV*)POPs;
    if (!isGV(gv))
	DIE(no_usym, "filehandle");
    if (GvIOp(gv))
	IoFLAGS(GvIOp(gv)) &= ~IOf_UNTAINT;
    tmps = SvPV(sv, len);
    if (do_open(gv, tmps, len, FALSE, O_RDONLY, 0, Nullfp))
	PUSHi( (I32)PL_forkprocess );
    else if (PL_forkprocess == 0)		/* we are a new child */
	PUSHi(0);
    else
	RETPUSHUNDEF;
    RETURN;
}

PP(pp_close)
{
    djSP;
    GV *gv;
    MAGIC *mg;

    if (MAXARG == 0)
	gv = PL_defoutgv;
    else
	gv = (GV*)POPs;

    if (mg = SvTIED_mg((SV*)gv, 'q')) {
	PUSHMARK(SP);
	XPUSHs(SvTIED_obj((SV*)gv, mg));
	PUTBACK;
	ENTER;
	perl_call_method("CLOSE", G_SCALAR);
	LEAVE;
	SPAGAIN;
	RETURN;
    }
    EXTEND(SP, 1);
    PUSHs(boolSV(do_close(gv, TRUE)));
    RETURN;
}

PP(pp_pipe_op)
{
    djSP;
#ifdef HAS_PIPE
    GV *rgv;
    GV *wgv;
    register IO *rstio;
    register IO *wstio;
    int fd[2];

    wgv = (GV*)POPs;
    rgv = (GV*)POPs;

    if (!rgv || !wgv)
	goto badexit;

    if (SvTYPE(rgv) != SVt_PVGV || SvTYPE(wgv) != SVt_PVGV)
	DIE(no_usym, "filehandle");
    rstio = GvIOn(rgv);
    wstio = GvIOn(wgv);

    if (IoIFP(rstio))
	do_close(rgv, FALSE);
    if (IoIFP(wstio))
	do_close(wgv, FALSE);

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
#if defined(HAS_FCNTL) && defined(F_SETFD)
    fcntl(fd[0],F_SETFD,fd[0] > PL_maxsysfd);	/* ensure close-on-exec */
    fcntl(fd[1],F_SETFD,fd[1] > PL_maxsysfd);	/* ensure close-on-exec */
#endif
    RETPUSHYES;

badexit:
    RETPUSHUNDEF;
#else
    DIE(no_func, "pipe");
#endif
}

PP(pp_fileno)
{
    djSP; dTARGET;
    GV *gv;
    IO *io;
    PerlIO *fp;
    if (MAXARG < 1)
	RETPUSHUNDEF;
    gv = (GV*)POPs;
    if (!gv || !(io = GvIO(gv)) || !(fp = IoIFP(io)))
	RETPUSHUNDEF;
    PUSHi(PerlIO_fileno(fp));
    RETURN;
}

PP(pp_umask)
{
    djSP; dTARGET;
    int anum;

#ifdef HAS_UMASK
    if (MAXARG < 1) {
	anum = PerlLIO_umask(0);
	(void)PerlLIO_umask(anum);
    }
    else
	anum = PerlLIO_umask(POPi);
    TAINT_PROPER("umask");
    XPUSHi(anum);
#else
    /* Only DIE if trying to restrict permissions on `user' (self).
     * Otherwise it's harmless and more useful to just return undef
     * since 'group' and 'other' concepts probably don't exist here. */
    if (MAXARG >= 1 && (POPi & 0700))
	DIE("umask not implemented");
    XPUSHs(&PL_sv_undef);
#endif
    RETURN;
}

PP(pp_binmode)
{
    djSP;
    GV *gv;
    IO *io;
    PerlIO *fp;

    if (MAXARG < 1)
	RETPUSHUNDEF;

    gv = (GV*)POPs;

    EXTEND(SP, 1);
    if (!(io = GvIO(gv)) || !(fp = IoIFP(io)))
	RETPUSHUNDEF;

    if (do_binmode(fp,IoTYPE(io),TRUE)) 
	RETPUSHYES;
    else
	RETPUSHUNDEF;
}


PP(pp_tie)
{
    djSP;
    dMARK;
    SV *varsv;
    HV* stash;
    GV *gv;
    SV *sv;
    I32 markoff = MARK - PL_stack_base;
    char *methname;
    int how = 'P';
    U32 items;

    varsv = *++MARK;
    switch(SvTYPE(varsv)) {
	case SVt_PVHV:
	    methname = "TIEHASH";
	    break;
	case SVt_PVAV:
	    methname = "TIEARRAY";
	    break;
	case SVt_PVGV:
	    methname = "TIEHANDLE";
	    how = 'q';
	    break;
	default:
	    methname = "TIESCALAR";
	    how = 'q';
	    break;
    }
    items = SP - MARK++;
    if (sv_isobject(*MARK)) {
	ENTER;
	PUSHSTACKi(PERLSI_MAGIC);
	PUSHMARK(SP);
	EXTEND(SP,items);
	while (items--)
	    PUSHs(*MARK++);
	PUTBACK;
	perl_call_method(methname, G_SCALAR);
    } 
    else {
	/* Not clear why we don't call perl_call_method here too.
	 * perhaps to get different error message ?
	 */
	stash = gv_stashsv(*MARK, FALSE);
	if (!stash || !(gv = gv_fetchmethod(stash, methname))) {
	    STRLEN n_a;
	    DIE("Can't locate object method \"%s\" via package \"%s\"",
		 methname, SvPV(*MARK,n_a));                   
	}
	ENTER;
	PUSHSTACKi(PERLSI_MAGIC);
	PUSHMARK(SP);
	EXTEND(SP,items);
	while (items--)
	    PUSHs(*MARK++);
	PUTBACK;
	perl_call_sv((SV*)GvCV(gv), G_SCALAR);
    }
    SPAGAIN;

    sv = TOPs;
    POPSTACK;
    if (sv_isobject(sv)) {
	sv_unmagic(varsv, how);
	sv_magic(varsv, (SvRV(sv) == varsv ? Nullsv : sv), how, Nullch, 0);
    }
    LEAVE;
    SP = PL_stack_base + markoff;
    PUSHs(sv);
    RETURN;
}

PP(pp_untie)
{
    djSP;
    SV *sv = POPs;
    char how = (SvTYPE(sv) == SVt_PVHV || SvTYPE(sv) == SVt_PVAV) ? 'P' : 'q';

    if (PL_dowarn) {
        MAGIC *mg;
        if (mg = SvTIED_mg(sv, how)) {
            if (mg->mg_obj && SvREFCNT(SvRV(mg->mg_obj)) > 1)  
		warn("untie attempted while %lu inner references still exist",
			(unsigned long)SvREFCNT(SvRV(mg->mg_obj)) - 1 ) ;
        }
    }
 
    sv_unmagic(sv, how);
    RETPUSHYES;
}

PP(pp_tied)
{
    djSP;
    SV *sv = POPs;
    char how = (SvTYPE(sv) == SVt_PVHV || SvTYPE(sv) == SVt_PVAV) ? 'P' : 'q';
    MAGIC *mg;

    if (mg = SvTIED_mg(sv, how)) {
	SV *osv = SvTIED_obj(sv, mg);
	if (osv == mg->mg_obj)
	    osv = sv_mortalcopy(osv);
	PUSHs(osv);
	RETURN;
    }
    RETPUSHUNDEF;
}

PP(pp_dbmopen)
{
    djSP;
    HV *hv;
    dPOPPOPssrl;
    HV* stash;
    GV *gv;
    SV *sv;

    hv = (HV*)POPs;

    sv = sv_mortalcopy(&PL_sv_no);
    sv_setpv(sv, "AnyDBM_File");
    stash = gv_stashsv(sv, FALSE);
    if (!stash || !(gv = gv_fetchmethod(stash, "TIEHASH"))) {
	PUTBACK;
	perl_require_pv("AnyDBM_File.pm");
	SPAGAIN;
	if (!(gv = gv_fetchmethod(stash, "TIEHASH")))
	    DIE("No dbm on this machine");
    }

    ENTER;
    PUSHMARK(SP);

    EXTEND(SP, 5);
    PUSHs(sv);
    PUSHs(left);
    if (SvIV(right))
	PUSHs(sv_2mortal(newSViv(O_RDWR|O_CREAT)));
    else
	PUSHs(sv_2mortal(newSViv(O_RDWR)));
    PUSHs(right);
    PUTBACK;
    perl_call_sv((SV*)GvCV(gv), G_SCALAR);
    SPAGAIN;

    if (!sv_isobject(TOPs)) {
	SP--;
	PUSHMARK(SP);
	PUSHs(sv);
	PUSHs(left);
	PUSHs(sv_2mortal(newSViv(O_RDONLY)));
	PUSHs(right);
	PUTBACK;
	perl_call_sv((SV*)GvCV(gv), G_SCALAR);
	SPAGAIN;
    }

    if (sv_isobject(TOPs)) {
	sv_unmagic((SV *) hv, 'P');            
	sv_magic((SV*)hv, TOPs, 'P', Nullch, 0);
    }
    LEAVE;
    RETURN;
}

PP(pp_dbmclose)
{
    return pp_untie(ARGS);
}

PP(pp_sselect)
{
    djSP; dTARGET;
#ifdef HAS_SELECT
    register I32 i;
    register I32 j;
    register char *s;
    register SV *sv;
    double value;
    I32 maxlen = 0;
    I32 nfound;
    struct timeval timebuf;
    struct timeval *tbuf = &timebuf;
    I32 growsize;
    char *fd_sets[4];
    STRLEN n_a;
#if BYTEORDER != 0x1234 && BYTEORDER != 0x12345678
	I32 masksize;
	I32 offset;
	I32 k;

#   if BYTEORDER & 0xf0000
#	define ORDERBYTE (0x88888888 - BYTEORDER)
#   else
#	define ORDERBYTE (0x4444 - BYTEORDER)
#   endif

#endif

    SP -= 4;
    for (i = 1; i <= 3; i++) {
	if (!SvPOK(SP[i]))
	    continue;
	j = SvCUR(SP[i]);
	if (maxlen < j)
	    maxlen = j;
    }

/* little endians can use vecs directly */
#if BYTEORDER == 0x1234 || BYTEORDER == 0x12345678
#  if SELECT_MIN_BITS > 1
    /* If SELECT_MIN_BITS is greater than one we most probably will want
     * to align the sizes with SELECT_MIN_BITS/8 because for example
     * in many little-endian (Intel, Alpha) systems (Linux, OS/2, Digital
     * UNIX, Solaris, NeXT) the smallest quantum select() operates on
     * (sets bit) is 32 bits.  */
    growsize = maxlen + (SELECT_MIN_BITS/8 - (maxlen % (SELECT_MIN_BITS/8)));
#else
    growsize = sizeof(fd_set);
#endif
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

    sv = SP[4];
    if (SvOK(sv)) {
	value = SvNV(sv);
	if (value < 0.0)
	    value = 0.0;
	timebuf.tv_sec = (long)value;
	value -= (double)timebuf.tv_sec;
	timebuf.tv_usec = (long)(value * 1000000.0);
    }
    else
	tbuf = Null(struct timeval*);

    for (i = 1; i <= 3; i++) {
	sv = SP[i];
	if (!SvOK(sv)) {
	    fd_sets[i] = 0;
	    continue;
	}
	else if (!SvPOK(sv))
	    SvPV_force(sv,n_a);	/* force string conversion */
	j = SvLEN(sv);
	if (j < growsize) {
	    Sv_Grow(sv, growsize);
	}
	j = SvCUR(sv);
	s = SvPVX(sv) + j;
	while (++j <= growsize) {
	    *s++ = '\0';
	}

#if BYTEORDER != 0x1234 && BYTEORDER != 0x12345678
	s = SvPVX(sv);
	New(403, fd_sets[i], growsize, char);
	for (offset = 0; offset < growsize; offset += masksize) {
	    for (j = 0, k=ORDERBYTE; j < masksize; j++, (k >>= 4))
		fd_sets[i][j+offset] = s[(k % masksize) + offset];
	}
#else
	fd_sets[i] = SvPVX(sv);
#endif
    }

    nfound = PerlSock_select(
	maxlen * 8,
	(Select_fd_set_t) fd_sets[1],
	(Select_fd_set_t) fd_sets[2],
	(Select_fd_set_t) fd_sets[3],
	tbuf);
    for (i = 1; i <= 3; i++) {
	if (fd_sets[i]) {
	    sv = SP[i];
#if BYTEORDER != 0x1234 && BYTEORDER != 0x12345678
	    s = SvPVX(sv);
	    for (offset = 0; offset < growsize; offset += masksize) {
		for (j = 0, k=ORDERBYTE; j < masksize; j++, (k >>= 4))
		    s[(k % masksize) + offset] = fd_sets[i][j+offset];
	    }
	    Safefree(fd_sets[i]);
#endif
	    SvSETMAGIC(sv);
	}
    }

    PUSHi(nfound);
    if (GIMME == G_ARRAY && tbuf) {
	value = (double)(timebuf.tv_sec) +
		(double)(timebuf.tv_usec) / 1000000.0;
	PUSHs(sv = sv_mortalcopy(&PL_sv_no));
	sv_setnv(sv, value);
    }
    RETURN;
#else
    DIE("select not implemented");
#endif
}

void
setdefout(GV *gv)
{
    dTHR;
    if (gv)
	(void)SvREFCNT_inc(gv);
    if (PL_defoutgv)
	SvREFCNT_dec(PL_defoutgv);
    PL_defoutgv = gv;
}

PP(pp_select)
{
    djSP; dTARGET;
    GV *newdefout, *egv;
    HV *hv;

    newdefout = (PL_op->op_private > 0) ? ((GV *) POPs) : (GV *) NULL;

    egv = GvEGV(PL_defoutgv);
    if (!egv)
	egv = PL_defoutgv;
    hv = GvSTASH(egv);
    if (! hv)
	XPUSHs(&PL_sv_undef);
    else {
	GV **gvp = (GV**)hv_fetch(hv, GvNAME(egv), GvNAMELEN(egv), FALSE);
	if (gvp && *gvp == egv) {
	    gv_efullname3(TARG, PL_defoutgv, Nullch);
	    XPUSHTARG;
	}
	else {
	    XPUSHs(sv_2mortal(newRV((SV*)egv)));
	}
    }

    if (newdefout) {
	if (!GvIO(newdefout))
	    gv_IOadd(newdefout);
	setdefout(newdefout);
    }

    RETURN;
}

PP(pp_getc)
{
    djSP; dTARGET;
    GV *gv;
    MAGIC *mg;

    if (MAXARG <= 0)
	gv = PL_stdingv;
    else
	gv = (GV*)POPs;
    if (!gv)
	gv = PL_argvgv;

    if (mg = SvTIED_mg((SV*)gv, 'q')) {
	I32 gimme = GIMME_V;
	PUSHMARK(SP);
	XPUSHs(SvTIED_obj((SV*)gv, mg));
	PUTBACK;
	ENTER;
	perl_call_method("GETC", gimme);
	LEAVE;
	SPAGAIN;
	if (gimme == G_SCALAR)
	    SvSetMagicSV_nosteal(TARG, TOPs);
	RETURN;
    }
    if (!gv || do_eof(gv)) /* make sure we have fp with something */
	RETPUSHUNDEF;
    TAINT;
    sv_setpv(TARG, " ");
    *SvPVX(TARG) = PerlIO_getc(IoIFP(GvIOp(gv))); /* should never be EOF */
    PUSHTARG;
    RETURN;
}

PP(pp_read)
{
    return pp_sysread(ARGS);
}

STATIC OP *
doform(CV *cv, GV *gv, OP *retop)
{
    dTHR;
    register PERL_CONTEXT *cx;
    I32 gimme = GIMME_V;
    AV* padlist = CvPADLIST(cv);
    SV** svp = AvARRAY(padlist);

    ENTER;
    SAVETMPS;

    push_return(retop);
    PUSHBLOCK(cx, CXt_SUB, PL_stack_sp);
    PUSHFORMAT(cx);
    SAVESPTR(PL_curpad);
    PL_curpad = AvARRAY((AV*)svp[1]);

    setdefout(gv);	    /* locally select filehandle so $% et al work */
    return CvSTART(cv);
}

PP(pp_enterwrite)
{
    djSP;
    register GV *gv;
    register IO *io;
    GV *fgv;
    CV *cv;

    if (MAXARG == 0)
	gv = PL_defoutgv;
    else {
	gv = (GV*)POPs;
	if (!gv)
	    gv = PL_defoutgv;
    }
    EXTEND(SP, 1);
    io = GvIO(gv);
    if (!io) {
	RETPUSHNO;
    }
    if (IoFMT_GV(io))
	fgv = IoFMT_GV(io);
    else
	fgv = gv;

    cv = GvFORM(fgv);
    if (!cv) {
	if (fgv) {
	    SV *tmpsv = sv_newmortal();
	    gv_efullname3(tmpsv, fgv, Nullch);
	    DIE("Undefined format \"%s\" called",SvPVX(tmpsv));
	}
	DIE("Not a format reference");
    }
    if (CvCLONE(cv))
	cv = (CV*)sv_2mortal((SV*)cv_clone(cv));

    IoFLAGS(io) &= ~IOf_DIDTOP;
    return doform(cv,gv,PL_op->op_next);
}

PP(pp_leavewrite)
{
    djSP;
    GV *gv = cxstack[cxstack_ix].blk_sub.gv;
    register IO *io = GvIOp(gv);
    PerlIO *ofp = IoOFP(io);
    PerlIO *fp;
    SV **newsp;
    I32 gimme;
    register PERL_CONTEXT *cx;

    DEBUG_f(PerlIO_printf(Perl_debug_log, "left=%ld, todo=%ld\n",
	  (long)IoLINES_LEFT(io), (long)FmLINES(PL_formtarget)));
    if (IoLINES_LEFT(io) < FmLINES(PL_formtarget) &&
	PL_formtarget != PL_toptarget)
    {
	GV *fgv;
	CV *cv;
	if (!IoTOP_GV(io)) {
	    GV *topgv;
	    SV *topname;

	    if (!IoTOP_NAME(io)) {
		if (!IoFMT_NAME(io))
		    IoFMT_NAME(io) = savepv(GvNAME(gv));
		topname = sv_2mortal(newSVpvf("%s_TOP", IoFMT_NAME(io)));
		topgv = gv_fetchpv(SvPVX(topname), FALSE, SVt_PVFM);
		if ((topgv && GvFORM(topgv)) ||
		  !gv_fetchpv("top",FALSE,SVt_PVFM))
		    IoTOP_NAME(io) = savepv(SvPVX(topname));
		else
		    IoTOP_NAME(io) = savepv("top");
	    }
	    topgv = gv_fetchpv(IoTOP_NAME(io),FALSE, SVt_PVFM);
	    if (!topgv || !GvFORM(topgv)) {
		IoLINES_LEFT(io) = 100000000;
		goto forget_top;
	    }
	    IoTOP_GV(io) = topgv;
	}
	if (IoFLAGS(io) & IOf_DIDTOP) {	/* Oh dear.  It still doesn't fit. */
	    I32 lines = IoLINES_LEFT(io);
	    char *s = SvPVX(PL_formtarget);
	    if (lines <= 0)		/* Yow, header didn't even fit!!! */
		goto forget_top;
	    while (lines-- > 0) {
		s = strchr(s, '\n');
		if (!s)
		    break;
		s++;
	    }
	    if (s) {
		PerlIO_write(ofp, SvPVX(PL_formtarget), s - SvPVX(PL_formtarget));
		sv_chop(PL_formtarget, s);
		FmLINES(PL_formtarget) -= IoLINES_LEFT(io);
	    }
	}
	if (IoLINES_LEFT(io) >= 0 && IoPAGE(io) > 0)
	    PerlIO_write(ofp, SvPVX(PL_formfeed), SvCUR(PL_formfeed));
	IoLINES_LEFT(io) = IoPAGE_LEN(io);
	IoPAGE(io)++;
	PL_formtarget = PL_toptarget;
	IoFLAGS(io) |= IOf_DIDTOP;
	fgv = IoTOP_GV(io);
	if (!fgv)
	    DIE("bad top format reference");
	cv = GvFORM(fgv);
	if (!cv) {
	    SV *tmpsv = sv_newmortal();
	    gv_efullname3(tmpsv, fgv, Nullch);
	    DIE("Undefined top format \"%s\" called",SvPVX(tmpsv));
	}
	if (CvCLONE(cv))
	    cv = (CV*)sv_2mortal((SV*)cv_clone(cv));
	return doform(cv,gv,PL_op);
    }

  forget_top:
    POPBLOCK(cx,PL_curpm);
    POPFORMAT(cx);
    LEAVE;

    fp = IoOFP(io);
    if (!fp) {
	if (PL_dowarn) {
	    if (IoIFP(io))
		warn("Filehandle only opened for input");
	    else
		warn("Write on closed filehandle");
	}
	PUSHs(&PL_sv_no);
    }
    else {
	if ((IoLINES_LEFT(io) -= FmLINES(PL_formtarget)) < 0) {
	    if (PL_dowarn)
		warn("page overflow");
	}
	if (!PerlIO_write(ofp, SvPVX(PL_formtarget), SvCUR(PL_formtarget)) ||
		PerlIO_error(fp))
	    PUSHs(&PL_sv_no);
	else {
	    FmLINES(PL_formtarget) = 0;
	    SvCUR_set(PL_formtarget, 0);
	    *SvEND(PL_formtarget) = '\0';
	    if (IoFLAGS(io) & IOf_FLUSH)
		(void)PerlIO_flush(fp);
	    PUSHs(&PL_sv_yes);
	}
    }
    PL_formtarget = PL_bodytarget;
    PUTBACK;
    return pop_return();
}

PP(pp_prtf)
{
    djSP; dMARK; dORIGMARK;
    GV *gv;
    IO *io;
    PerlIO *fp;
    SV *sv;
    MAGIC *mg;
    STRLEN n_a;

    if (PL_op->op_flags & OPf_STACKED)
	gv = (GV*)*++MARK;
    else
	gv = PL_defoutgv;

    if (mg = SvTIED_mg((SV*)gv, 'q')) {
	if (MARK == ORIGMARK) {
	    MEXTEND(SP, 1);
	    ++MARK;
	    Move(MARK, MARK + 1, (SP - MARK) + 1, SV*);
	    ++SP;
	}
	PUSHMARK(MARK - 1);
	*MARK = SvTIED_obj((SV*)gv, mg);
	PUTBACK;
	ENTER;
	perl_call_method("PRINTF", G_SCALAR);
	LEAVE;
	SPAGAIN;
	MARK = ORIGMARK + 1;
	*MARK = *SP;
	SP = MARK;
	RETURN;
    }

    sv = NEWSV(0,0);
    if (!(io = GvIO(gv))) {
	if (PL_dowarn) {
	    gv_fullname3(sv, gv, Nullch);
	    warn("Filehandle %s never opened", SvPV(sv,n_a));
	}
	SETERRNO(EBADF,RMS$_IFI);
	goto just_say_no;
    }
    else if (!(fp = IoOFP(io))) {
	if (PL_dowarn)  {
	    gv_fullname3(sv, gv, Nullch);
	    if (IoIFP(io))
		warn("Filehandle %s opened only for input", SvPV(sv,n_a));
	    else
		warn("printf on closed filehandle %s", SvPV(sv,n_a));
	}
	SETERRNO(EBADF,IoIFP(io)?RMS$_FAC:RMS$_IFI);
	goto just_say_no;
    }
    else {
#ifdef USE_LOCALE_NUMERIC
	if (PL_op->op_private & OPpLOCALE)
	    SET_NUMERIC_LOCAL();
	else
	    SET_NUMERIC_STANDARD();
#endif
	do_sprintf(sv, SP - MARK, MARK + 1);
	if (!do_print(sv, fp))
	    goto just_say_no;

	if (IoFLAGS(io) & IOf_FLUSH)
	    if (PerlIO_flush(fp) == EOF)
		goto just_say_no;
    }
    SvREFCNT_dec(sv);
    SP = ORIGMARK;
    PUSHs(&PL_sv_yes);
    RETURN;

  just_say_no:
    SvREFCNT_dec(sv);
    SP = ORIGMARK;
    PUSHs(&PL_sv_undef);
    RETURN;
}

PP(pp_sysopen)
{
    djSP;
    GV *gv;
    SV *sv;
    char *tmps;
    STRLEN len;
    int mode, perm;

    if (MAXARG > 3)
	perm = POPi;
    else
	perm = 0666;
    mode = POPi;
    sv = POPs;
    gv = (GV *)POPs;

    tmps = SvPV(sv, len);
    if (do_open(gv, tmps, len, TRUE, mode, perm, Nullfp)) {
	IoLINES(GvIOp(gv)) = 0;
	PUSHs(&PL_sv_yes);
    }
    else {
	PUSHs(&PL_sv_undef);
    }
    RETURN;
}

PP(pp_sysread)
{
    djSP; dMARK; dORIGMARK; dTARGET;
    int offset;
    GV *gv;
    IO *io;
    char *buffer;
    SSize_t length;
    Sock_size_t bufsize;
    SV *bufsv;
    STRLEN blen;
    MAGIC *mg;

    gv = (GV*)*++MARK;
    if ((PL_op->op_type == OP_READ || PL_op->op_type == OP_SYSREAD) &&
	(mg = SvTIED_mg((SV*)gv, 'q')))
    {
	SV *sv;
	
	PUSHMARK(MARK-1);
	*MARK = SvTIED_obj((SV*)gv, mg);
	ENTER;
	perl_call_method("READ", G_SCALAR);
	LEAVE;
	SPAGAIN;
	sv = POPs;
	SP = ORIGMARK;
	PUSHs(sv);
	RETURN;
    }

    if (!gv)
	goto say_undef;
    bufsv = *++MARK;
    if (! SvOK(bufsv))
	sv_setpvn(bufsv, "", 0);
    buffer = SvPV_force(bufsv, blen);
    length = SvIVx(*++MARK);
    if (length < 0)
	DIE("Negative length");
    SETERRNO(0,0);
    if (MARK < SP)
	offset = SvIVx(*++MARK);
    else
	offset = 0;
    io = GvIO(gv);
    if (!io || !IoIFP(io))
	goto say_undef;
#ifdef HAS_SOCKET
    if (PL_op->op_type == OP_RECV) {
	char namebuf[MAXPATHLEN];
#if (defined(VMS_DO_SOCKETS) && defined(DECCRTL_SOCKETS)) || defined(MPE)
	bufsize = sizeof (struct sockaddr_in);
#else
	bufsize = sizeof namebuf;
#endif
	buffer = SvGROW(bufsv, length+1);
	/* 'offset' means 'flags' here */
	length = PerlSock_recvfrom(PerlIO_fileno(IoIFP(io)), buffer, length, offset,
			  (struct sockaddr *)namebuf, &bufsize);
	if (length < 0)
	    RETPUSHUNDEF;
	SvCUR_set(bufsv, length);
	*SvEND(bufsv) = '\0';
	(void)SvPOK_only(bufsv);
	SvSETMAGIC(bufsv);
	/* This should not be marked tainted if the fp is marked clean */
	if (!(IoFLAGS(io) & IOf_UNTAINT))
	    SvTAINTED_on(bufsv);
	SP = ORIGMARK;
	sv_setpvn(TARG, namebuf, bufsize);
	PUSHs(TARG);
	RETURN;
    }
#else
    if (PL_op->op_type == OP_RECV)
	DIE(no_sock_func, "recv");
#endif
    if (offset < 0) {
	if (-offset > blen)
	    DIE("Offset outside string");
	offset += blen;
    }
    bufsize = SvCUR(bufsv);
    buffer = SvGROW(bufsv, length+offset+1);
    if (offset > bufsize) { /* Zero any newly allocated space */
    	Zero(buffer+bufsize, offset-bufsize, char);
    }
    if (PL_op->op_type == OP_SYSREAD) {
#ifdef PERL_SOCK_SYSREAD_IS_RECV
	if (IoTYPE(io) == 's') {
	    length = PerlSock_recv(PerlIO_fileno(IoIFP(io)),
				   buffer+offset, length, 0);
	}
	else
#endif
	{
	    length = PerlLIO_read(PerlIO_fileno(IoIFP(io)),
				  buffer+offset, length);
	}
    }
    else
#ifdef HAS_SOCKET__bad_code_maybe
    if (IoTYPE(io) == 's') {
	char namebuf[MAXPATHLEN];
#if defined(VMS_DO_SOCKETS) && defined(DECCRTL_SOCKETS)
	bufsize = sizeof (struct sockaddr_in);
#else
	bufsize = sizeof namebuf;
#endif
	length = PerlSock_recvfrom(PerlIO_fileno(IoIFP(io)), buffer+offset, length, 0,
			  (struct sockaddr *)namebuf, &bufsize);
    }
    else
#endif
    {
	length = PerlIO_read(IoIFP(io), buffer+offset, length);
	/* fread() returns 0 on both error and EOF */
	if (length == 0 && PerlIO_error(IoIFP(io)))
	    length = -1;
    }
    if (length < 0)
	goto say_undef;
    SvCUR_set(bufsv, length+offset);
    *SvEND(bufsv) = '\0';
    (void)SvPOK_only(bufsv);
    SvSETMAGIC(bufsv);
    /* This should not be marked tainted if the fp is marked clean */
    if (!(IoFLAGS(io) & IOf_UNTAINT))
	SvTAINTED_on(bufsv);
    SP = ORIGMARK;
    PUSHi(length);
    RETURN;

  say_undef:
    SP = ORIGMARK;
    RETPUSHUNDEF;
}

PP(pp_syswrite)
{
    djSP;
    int items = (SP - PL_stack_base) - TOPMARK;
    if (items == 2) {
	SV *sv;
        EXTEND(SP, 1);
	sv = sv_2mortal(newSViv(sv_len(*SP)));
        PUSHs(sv);
        PUTBACK;
    }
    return pp_send(ARGS);
}

PP(pp_send)
{
    djSP; dMARK; dORIGMARK; dTARGET;
    GV *gv;
    IO *io;
    int offset;
    SV *bufsv;
    char *buffer;
    int length;
    STRLEN blen;
    MAGIC *mg;

    gv = (GV*)*++MARK;
    if (PL_op->op_type == OP_SYSWRITE && (mg = SvTIED_mg((SV*)gv, 'q'))) {
	SV *sv;
	
	PUSHMARK(MARK-1);
	*MARK = SvTIED_obj((SV*)gv, mg);
	ENTER;
	perl_call_method("WRITE", G_SCALAR);
	LEAVE;
	SPAGAIN;
	sv = POPs;
	SP = ORIGMARK;
	PUSHs(sv);
	RETURN;
    }
    if (!gv)
	goto say_undef;
    bufsv = *++MARK;
    buffer = SvPV(bufsv, blen);
    length = SvIVx(*++MARK);
    if (length < 0)
	DIE("Negative length");
    SETERRNO(0,0);
    io = GvIO(gv);
    if (!io || !IoIFP(io)) {
	length = -1;
	if (PL_dowarn) {
	    if (PL_op->op_type == OP_SYSWRITE)
		warn("Syswrite on closed filehandle");
	    else
		warn("Send on closed socket");
	}
    }
    else if (PL_op->op_type == OP_SYSWRITE) {
	if (MARK < SP) {
	    offset = SvIVx(*++MARK);
	    if (offset < 0) {
		if (-offset > blen)
		    DIE("Offset outside string");
		offset += blen;
	    } else if (offset >= blen && blen > 0)
		DIE("Offset outside string");
	} else
	    offset = 0;
	if (length > blen - offset)
	    length = blen - offset;
#ifdef PERL_SOCK_SYSWRITE_IS_SEND
	if (IoTYPE(io) == 's') {
	    length = PerlSock_send(PerlIO_fileno(IoIFP(io)),
				   buffer+offset, length, 0);
	}
	else
#endif
	{
	    length = PerlLIO_write(PerlIO_fileno(IoIFP(io)),
				   buffer+offset, length);
	}
    }
#ifdef HAS_SOCKET
    else if (SP > MARK) {
	char *sockbuf;
	STRLEN mlen;
	sockbuf = SvPVx(*++MARK, mlen);
	length = PerlSock_sendto(PerlIO_fileno(IoIFP(io)), buffer, blen, length,
				(struct sockaddr *)sockbuf, mlen);
    }
    else
	length = PerlSock_send(PerlIO_fileno(IoIFP(io)), buffer, blen, length);

#else
    else
	DIE(no_sock_func, "send");
#endif
    if (length < 0)
	goto say_undef;
    SP = ORIGMARK;
    PUSHi(length);
    RETURN;

  say_undef:
    SP = ORIGMARK;
    RETPUSHUNDEF;
}

PP(pp_recv)
{
    return pp_sysread(ARGS);
}

PP(pp_eof)
{
    djSP;
    GV *gv;

    if (MAXARG <= 0)
	gv = PL_last_in_gv;
    else
	gv = PL_last_in_gv = (GV*)POPs;
    PUSHs(boolSV(!gv || do_eof(gv)));
    RETURN;
}

PP(pp_tell)
{
    djSP; dTARGET;
    GV *gv;

    if (MAXARG <= 0)
	gv = PL_last_in_gv;
    else
	gv = PL_last_in_gv = (GV*)POPs;
    PUSHi( do_tell(gv) );
    RETURN;
}

PP(pp_seek)
{
    return pp_sysseek(ARGS);
}

PP(pp_sysseek)
{
    djSP;
    GV *gv;
    int whence = POPi;
    long offset = POPl;

    gv = PL_last_in_gv = (GV*)POPs;
    if (PL_op->op_type == OP_SEEK)
	PUSHs(boolSV(do_seek(gv, offset, whence)));
    else {
	long n = do_sysseek(gv, offset, whence);
	PUSHs((n < 0) ? &PL_sv_undef
	      : sv_2mortal(n ? newSViv((IV)n)
			   : newSVpv(zero_but_true, ZBTLEN)));
    }
    RETURN;
}

PP(pp_truncate)
{
    djSP;
    Off_t len = (Off_t)POPn;
    int result = 1;
    GV *tmpgv;
    STRLEN n_a;

    SETERRNO(0,0);
#if defined(HAS_TRUNCATE) || defined(HAS_CHSIZE) || defined(F_FREESP)
    if (PL_op->op_flags & OPf_SPECIAL) {
	tmpgv = gv_fetchpv(POPpx, FALSE, SVt_PVIO);
    do_ftruncate:
	TAINT_PROPER("truncate");
	if (!GvIO(tmpgv) || !IoIFP(GvIOp(tmpgv)) ||
#ifdef HAS_TRUNCATE
	  ftruncate(PerlIO_fileno(IoIFP(GvIOn(tmpgv))), len) < 0)
#else 
	  my_chsize(PerlIO_fileno(IoIFP(GvIOn(tmpgv))), len) < 0)
#endif
	    result = 0;
    }
    else {
	SV *sv = POPs;
	char *name;

	if (SvTYPE(sv) == SVt_PVGV) {
	    tmpgv = (GV*)sv;		/* *main::FRED for example */
	    goto do_ftruncate;
	}
	else if (SvROK(sv) && SvTYPE(SvRV(sv)) == SVt_PVGV) {
	    tmpgv = (GV*) SvRV(sv);	/* \*main::FRED for example */
	    goto do_ftruncate;
	}

	name = SvPV(sv, n_a);
	TAINT_PROPER("truncate");
#ifdef HAS_TRUNCATE
	if (truncate(name, len) < 0)
	    result = 0;
#else
	{
	    int tmpfd;
	    if ((tmpfd = PerlLIO_open(name, O_RDWR)) < 0)
		result = 0;
	    else {
		if (my_chsize(tmpfd, len) < 0)
		    result = 0;
		PerlLIO_close(tmpfd);
	    }
	}
#endif
    }

    if (result)
	RETPUSHYES;
    if (!errno)
	SETERRNO(EBADF,RMS$_IFI);
    RETPUSHUNDEF;
#else
    DIE("truncate not implemented");
#endif
}

PP(pp_fcntl)
{
    return pp_ioctl(ARGS);
}

PP(pp_ioctl)
{
    djSP; dTARGET;
    SV *argsv = POPs;
    unsigned int func = U_I(POPn);
    int optype = PL_op->op_type;
    char *s;
    IV retval;
    GV *gv = (GV*)POPs;
    IO *io = GvIOn(gv);

    if (!io || !argsv || !IoIFP(io)) {
	SETERRNO(EBADF,RMS$_IFI);	/* well, sort of... */
	RETPUSHUNDEF;
    }

    if (SvPOK(argsv) || !SvNIOK(argsv)) {
	STRLEN len;
	STRLEN need;
	s = SvPV_force(argsv, len);
	need = IOCPARM_LEN(func);
	if (len < need) {
	    s = Sv_Grow(argsv, need + 1);
	    SvCUR_set(argsv, need);
	}

	s[SvCUR(argsv)] = 17;	/* a little sanity check here */
    }
    else {
	retval = SvIV(argsv);
	s = (char*)retval;		/* ouch */
    }

    TAINT_PROPER(optype == OP_IOCTL ? "ioctl" : "fcntl");

    if (optype == OP_IOCTL)
#ifdef HAS_IOCTL
	retval = PerlLIO_ioctl(PerlIO_fileno(IoIFP(io)), func, s);
#else
	DIE("ioctl is not implemented");
#endif
    else
#ifdef HAS_FCNTL
#if defined(OS2) && defined(__EMX__)
	retval = fcntl(PerlIO_fileno(IoIFP(io)), func, (int)s);
#else
	retval = fcntl(PerlIO_fileno(IoIFP(io)), func, s);
#endif 
#else
	DIE("fcntl is not implemented");
#endif

    if (SvPOK(argsv)) {
	if (s[SvCUR(argsv)] != 17)
	    DIE("Possible memory corruption: %s overflowed 3rd argument",
		op_name[optype]);
	s[SvCUR(argsv)] = 0;		/* put our null back */
	SvSETMAGIC(argsv);		/* Assume it has changed */
    }

    if (retval == -1)
	RETPUSHUNDEF;
    if (retval != 0) {
	PUSHi(retval);
    }
    else {
	PUSHp(zero_but_true, ZBTLEN);
    }
    RETURN;
}

PP(pp_flock)
{
    djSP; dTARGET;
    I32 value;
    int argtype;
    GV *gv;
    PerlIO *fp;

#ifdef FLOCK
    argtype = POPi;
    if (MAXARG <= 0)
	gv = PL_last_in_gv;
    else
	gv = (GV*)POPs;
    if (gv && GvIO(gv))
	fp = IoIFP(GvIOp(gv));
    else
	fp = Nullfp;
    if (fp) {
	(void)PerlIO_flush(fp);
	value = (I32)(PerlLIO_flock(PerlIO_fileno(fp), argtype) >= 0);
    }
    else
	value = 0;
    PUSHi(value);
    RETURN;
#else
    DIE(no_func, "flock()");
#endif
}

/* Sockets. */

PP(pp_socket)
{
    djSP;
#ifdef HAS_SOCKET
    GV *gv;
    register IO *io;
    int protocol = POPi;
    int type = POPi;
    int domain = POPi;
    int fd;

    gv = (GV*)POPs;

    if (!gv) {
	SETERRNO(EBADF,LIB$_INVARG);
	RETPUSHUNDEF;
    }

    io = GvIOn(gv);
    if (IoIFP(io))
	do_close(gv, FALSE);

    TAINT_PROPER("socket");
    fd = PerlSock_socket(domain, type, protocol);
    if (fd < 0)
	RETPUSHUNDEF;
    IoIFP(io) = PerlIO_fdopen(fd, "r");	/* stdio gets confused about sockets */
    IoOFP(io) = PerlIO_fdopen(fd, "w");
    IoTYPE(io) = 's';
    if (!IoIFP(io) || !IoOFP(io)) {
	if (IoIFP(io)) PerlIO_close(IoIFP(io));
	if (IoOFP(io)) PerlIO_close(IoOFP(io));
	if (!IoIFP(io) && !IoOFP(io)) PerlLIO_close(fd);
	RETPUSHUNDEF;
    }

    RETPUSHYES;
#else
    DIE(no_sock_func, "socket");
#endif
}

PP(pp_sockpair)
{
    djSP;
#ifdef HAS_SOCKETPAIR
    GV *gv1;
    GV *gv2;
    register IO *io1;
    register IO *io2;
    int protocol = POPi;
    int type = POPi;
    int domain = POPi;
    int fd[2];

    gv2 = (GV*)POPs;
    gv1 = (GV*)POPs;
    if (!gv1 || !gv2)
	RETPUSHUNDEF;

    io1 = GvIOn(gv1);
    io2 = GvIOn(gv2);
    if (IoIFP(io1))
	do_close(gv1, FALSE);
    if (IoIFP(io2))
	do_close(gv2, FALSE);

    TAINT_PROPER("socketpair");
    if (PerlSock_socketpair(domain, type, protocol, fd) < 0)
	RETPUSHUNDEF;
    IoIFP(io1) = PerlIO_fdopen(fd[0], "r");
    IoOFP(io1) = PerlIO_fdopen(fd[0], "w");
    IoTYPE(io1) = 's';
    IoIFP(io2) = PerlIO_fdopen(fd[1], "r");
    IoOFP(io2) = PerlIO_fdopen(fd[1], "w");
    IoTYPE(io2) = 's';
    if (!IoIFP(io1) || !IoOFP(io1) || !IoIFP(io2) || !IoOFP(io2)) {
	if (IoIFP(io1)) PerlIO_close(IoIFP(io1));
	if (IoOFP(io1)) PerlIO_close(IoOFP(io1));
	if (!IoIFP(io1) && !IoOFP(io1)) PerlLIO_close(fd[0]);
	if (IoIFP(io2)) PerlIO_close(IoIFP(io2));
	if (IoOFP(io2)) PerlIO_close(IoOFP(io2));
	if (!IoIFP(io2) && !IoOFP(io2)) PerlLIO_close(fd[1]);
	RETPUSHUNDEF;
    }

    RETPUSHYES;
#else
    DIE(no_sock_func, "socketpair");
#endif
}

PP(pp_bind)
{
    djSP;
#ifdef HAS_SOCKET
#ifdef MPE /* Requires PRIV mode to bind() to ports < 1024 */
    extern GETPRIVMODE();
    extern GETUSERMODE();
#endif
    SV *addrsv = POPs;
    char *addr;
    GV *gv = (GV*)POPs;
    register IO *io = GvIOn(gv);
    STRLEN len;
    int bind_ok = 0;
#ifdef MPE
    int mpeprivmode = 0;
#endif

    if (!io || !IoIFP(io))
	goto nuts;

    addr = SvPV(addrsv, len);
    TAINT_PROPER("bind");
#ifdef MPE /* Deal with MPE bind() peculiarities */
    if (((struct sockaddr *)addr)->sa_family == AF_INET) {
        /* The address *MUST* stupidly be zero. */
        ((struct sockaddr_in *)addr)->sin_addr.s_addr = INADDR_ANY;
        /* PRIV mode is required to bind() to ports < 1024. */
        if (((struct sockaddr_in *)addr)->sin_port < 1024 &&
            ((struct sockaddr_in *)addr)->sin_port > 0) {
            GETPRIVMODE(); /* If this fails, we are aborted by MPE/iX. */
	    mpeprivmode = 1;
	}
    }
#endif /* MPE */
    if (PerlSock_bind(PerlIO_fileno(IoIFP(io)),
		      (struct sockaddr *)addr, len) >= 0)
	bind_ok = 1;

#ifdef MPE /* Switch back to USER mode */
    if (mpeprivmode)
	GETUSERMODE();
#endif /* MPE */

    if (bind_ok)
	RETPUSHYES;
    else
	RETPUSHUNDEF;

nuts:
    if (PL_dowarn)
	warn("bind() on closed fd");
    SETERRNO(EBADF,SS$_IVCHAN);
    RETPUSHUNDEF;
#else
    DIE(no_sock_func, "bind");
#endif
}

PP(pp_connect)
{
    djSP;
#ifdef HAS_SOCKET
    SV *addrsv = POPs;
    char *addr;
    GV *gv = (GV*)POPs;
    register IO *io = GvIOn(gv);
    STRLEN len;

    if (!io || !IoIFP(io))
	goto nuts;

    addr = SvPV(addrsv, len);
    TAINT_PROPER("connect");
    if (PerlSock_connect(PerlIO_fileno(IoIFP(io)), (struct sockaddr *)addr, len) >= 0)
	RETPUSHYES;
    else
	RETPUSHUNDEF;

nuts:
    if (PL_dowarn)
	warn("connect() on closed fd");
    SETERRNO(EBADF,SS$_IVCHAN);
    RETPUSHUNDEF;
#else
    DIE(no_sock_func, "connect");
#endif
}

PP(pp_listen)
{
    djSP;
#ifdef HAS_SOCKET
    int backlog = POPi;
    GV *gv = (GV*)POPs;
    register IO *io = GvIOn(gv);

    if (!io || !IoIFP(io))
	goto nuts;

    if (PerlSock_listen(PerlIO_fileno(IoIFP(io)), backlog) >= 0)
	RETPUSHYES;
    else
	RETPUSHUNDEF;

nuts:
    if (PL_dowarn)
	warn("listen() on closed fd");
    SETERRNO(EBADF,SS$_IVCHAN);
    RETPUSHUNDEF;
#else
    DIE(no_sock_func, "listen");
#endif
}

PP(pp_accept)
{
    djSP; dTARGET;
#ifdef HAS_SOCKET
    GV *ngv;
    GV *ggv;
    register IO *nstio;
    register IO *gstio;
    struct sockaddr saddr;	/* use a struct to avoid alignment problems */
    Sock_size_t len = sizeof saddr;
    int fd;

    ggv = (GV*)POPs;
    ngv = (GV*)POPs;

    if (!ngv)
	goto badexit;
    if (!ggv)
	goto nuts;

    gstio = GvIO(ggv);
    if (!gstio || !IoIFP(gstio))
	goto nuts;

    nstio = GvIOn(ngv);
    if (IoIFP(nstio))
	do_close(ngv, FALSE);

    fd = PerlSock_accept(PerlIO_fileno(IoIFP(gstio)), (struct sockaddr *)&saddr, &len);
    if (fd < 0)
	goto badexit;
    IoIFP(nstio) = PerlIO_fdopen(fd, "r");
    IoOFP(nstio) = PerlIO_fdopen(fd, "w");
    IoTYPE(nstio) = 's';
    if (!IoIFP(nstio) || !IoOFP(nstio)) {
	if (IoIFP(nstio)) PerlIO_close(IoIFP(nstio));
	if (IoOFP(nstio)) PerlIO_close(IoOFP(nstio));
	if (!IoIFP(nstio) && !IoOFP(nstio)) PerlLIO_close(fd);
	goto badexit;
    }

    PUSHp((char *)&saddr, len);
    RETURN;

nuts:
    if (PL_dowarn)
	warn("accept() on closed fd");
    SETERRNO(EBADF,SS$_IVCHAN);

badexit:
    RETPUSHUNDEF;

#else
    DIE(no_sock_func, "accept");
#endif
}

PP(pp_shutdown)
{
    djSP; dTARGET;
#ifdef HAS_SOCKET
    int how = POPi;
    GV *gv = (GV*)POPs;
    register IO *io = GvIOn(gv);

    if (!io || !IoIFP(io))
	goto nuts;

    PUSHi( PerlSock_shutdown(PerlIO_fileno(IoIFP(io)), how) >= 0 );
    RETURN;

nuts:
    if (PL_dowarn)
	warn("shutdown() on closed fd");
    SETERRNO(EBADF,SS$_IVCHAN);
    RETPUSHUNDEF;
#else
    DIE(no_sock_func, "shutdown");
#endif
}

PP(pp_gsockopt)
{
#ifdef HAS_SOCKET
    return pp_ssockopt(ARGS);
#else
    DIE(no_sock_func, "getsockopt");
#endif
}

PP(pp_ssockopt)
{
    djSP;
#ifdef HAS_SOCKET
    int optype = PL_op->op_type;
    SV *sv;
    int fd;
    unsigned int optname;
    unsigned int lvl;
    GV *gv;
    register IO *io;
    Sock_size_t len;

    if (optype == OP_GSOCKOPT)
	sv = sv_2mortal(NEWSV(22, 257));
    else
	sv = POPs;
    optname = (unsigned int) POPi;
    lvl = (unsigned int) POPi;

    gv = (GV*)POPs;
    io = GvIOn(gv);
    if (!io || !IoIFP(io))
	goto nuts;

    fd = PerlIO_fileno(IoIFP(io));
    switch (optype) {
    case OP_GSOCKOPT:
	SvGROW(sv, 257);
	(void)SvPOK_only(sv);
	SvCUR_set(sv,256);
	*SvEND(sv) ='\0';
	len = SvCUR(sv);
	if (PerlSock_getsockopt(fd, lvl, optname, SvPVX(sv), &len) < 0)
	    goto nuts2;
	SvCUR_set(sv, len);
	*SvEND(sv) ='\0';
	PUSHs(sv);
	break;
    case OP_SSOCKOPT: {
	    char *buf;
	    int aint;
	    if (SvPOKp(sv)) {
		STRLEN l;
		buf = SvPV(sv, l);
		len = l;
	    }
	    else {
		aint = (int)SvIV(sv);
		buf = (char*)&aint;
		len = sizeof(int);
	    }
	    if (PerlSock_setsockopt(fd, lvl, optname, buf, len) < 0)
		goto nuts2;
	    PUSHs(&PL_sv_yes);
	}
	break;
    }
    RETURN;

nuts:
    if (PL_dowarn)
	warn("[gs]etsockopt() on closed fd");
    SETERRNO(EBADF,SS$_IVCHAN);
nuts2:
    RETPUSHUNDEF;

#else
    DIE(no_sock_func, "setsockopt");
#endif
}

PP(pp_getsockname)
{
#ifdef HAS_SOCKET
    return pp_getpeername(ARGS);
#else
    DIE(no_sock_func, "getsockname");
#endif
}

PP(pp_getpeername)
{
    djSP;
#ifdef HAS_SOCKET
    int optype = PL_op->op_type;
    SV *sv;
    int fd;
    GV *gv = (GV*)POPs;
    register IO *io = GvIOn(gv);
    Sock_size_t len;

    if (!io || !IoIFP(io))
	goto nuts;

    sv = sv_2mortal(NEWSV(22, 257));
    (void)SvPOK_only(sv);
    len = 256;
    SvCUR_set(sv, len);
    *SvEND(sv) ='\0';
    fd = PerlIO_fileno(IoIFP(io));
    switch (optype) {
    case OP_GETSOCKNAME:
	if (PerlSock_getsockname(fd, (struct sockaddr *)SvPVX(sv), &len) < 0)
	    goto nuts2;
	break;
    case OP_GETPEERNAME:
	if (PerlSock_getpeername(fd, (struct sockaddr *)SvPVX(sv), &len) < 0)
	    goto nuts2;
#if defined(VMS_DO_SOCKETS) && defined (DECCRTL_SOCKETS)
	{
	    static const char nowhere[] = "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";
	    /* If the call succeeded, make sure we don't have a zeroed port/addr */
	    if (((struct sockaddr *)SvPVX(sv))->sa_family == AF_INET &&
		!memcmp((char *)SvPVX(sv) + sizeof(u_short), nowhere,
			sizeof(u_short) + sizeof(struct in_addr))) {
		goto nuts2;	    
	    }
	}
#endif
	break;
    }
#ifdef BOGUS_GETNAME_RETURN
    /* Interactive Unix, getpeername() and getsockname()
      does not return valid namelen */
    if (len == BOGUS_GETNAME_RETURN)
	len = sizeof(struct sockaddr);
#endif
    SvCUR_set(sv, len);
    *SvEND(sv) ='\0';
    PUSHs(sv);
    RETURN;

nuts:
    if (PL_dowarn)
	warn("get{sock, peer}name() on closed fd");
    SETERRNO(EBADF,SS$_IVCHAN);
nuts2:
    RETPUSHUNDEF;

#else
    DIE(no_sock_func, "getpeername");
#endif
}

/* Stat calls. */

PP(pp_lstat)
{
    return pp_stat(ARGS);
}

PP(pp_stat)
{
    djSP;
    GV *tmpgv;
    I32 gimme;
    I32 max = 13;
    STRLEN n_a;

    if (PL_op->op_flags & OPf_REF) {
	tmpgv = cGVOP->op_gv;
      do_fstat:
	if (tmpgv != PL_defgv) {
	    PL_laststype = OP_STAT;
	    PL_statgv = tmpgv;
	    sv_setpv(PL_statname, "");
	    PL_laststatval = (GvIO(tmpgv) && IoIFP(GvIOp(tmpgv))
		? PerlLIO_fstat(PerlIO_fileno(IoIFP(GvIOn(tmpgv))), &PL_statcache) : -1);
	}
	if (PL_laststatval < 0)
	    max = 0;
    }
    else {
	SV* sv = POPs;
	if (SvTYPE(sv) == SVt_PVGV) {
	    tmpgv = (GV*)sv;
	    goto do_fstat;
	}
	else if (SvROK(sv) && SvTYPE(SvRV(sv)) == SVt_PVGV) {
	    tmpgv = (GV*)SvRV(sv);
	    goto do_fstat;
	}
	sv_setpv(PL_statname, SvPV(sv,n_a));
	PL_statgv = Nullgv;
#ifdef HAS_LSTAT
	PL_laststype = PL_op->op_type;
	if (PL_op->op_type == OP_LSTAT)
	    PL_laststatval = PerlLIO_lstat(SvPV(PL_statname, n_a), &PL_statcache);
	else
#endif
	    PL_laststatval = PerlLIO_stat(SvPV(PL_statname, n_a), &PL_statcache);
	if (PL_laststatval < 0) {
	    if (PL_dowarn && strchr(SvPV(PL_statname, n_a), '\n'))
		warn(warn_nl, "stat");
	    max = 0;
	}
    }

    gimme = GIMME_V;
    if (gimme != G_ARRAY) {
	if (gimme != G_VOID)
	    XPUSHs(boolSV(max));
	RETURN;
    }
    if (max) {
	EXTEND(SP, max);
	EXTEND_MORTAL(max);
	PUSHs(sv_2mortal(newSViv((I32)PL_statcache.st_dev)));
	PUSHs(sv_2mortal(newSViv((I32)PL_statcache.st_ino)));
	PUSHs(sv_2mortal(newSViv((I32)PL_statcache.st_mode)));
	PUSHs(sv_2mortal(newSViv((I32)PL_statcache.st_nlink)));
	PUSHs(sv_2mortal(newSViv((I32)PL_statcache.st_uid)));
	PUSHs(sv_2mortal(newSViv((I32)PL_statcache.st_gid)));
#ifdef USE_STAT_RDEV
	PUSHs(sv_2mortal(newSViv((I32)PL_statcache.st_rdev)));
#else
	PUSHs(sv_2mortal(newSVpv("", 0)));
#endif
	PUSHs(sv_2mortal(newSViv((I32)PL_statcache.st_size)));
#ifdef BIG_TIME
	PUSHs(sv_2mortal(newSVnv((U32)PL_statcache.st_atime)));
	PUSHs(sv_2mortal(newSVnv((U32)PL_statcache.st_mtime)));
	PUSHs(sv_2mortal(newSVnv((U32)PL_statcache.st_ctime)));
#else
	PUSHs(sv_2mortal(newSViv((I32)PL_statcache.st_atime)));
	PUSHs(sv_2mortal(newSViv((I32)PL_statcache.st_mtime)));
	PUSHs(sv_2mortal(newSViv((I32)PL_statcache.st_ctime)));
#endif
#ifdef USE_STAT_BLOCKS
	PUSHs(sv_2mortal(newSViv((I32)PL_statcache.st_blksize)));
	PUSHs(sv_2mortal(newSViv((I32)PL_statcache.st_blocks)));
#else
	PUSHs(sv_2mortal(newSVpv("", 0)));
	PUSHs(sv_2mortal(newSVpv("", 0)));
#endif
    }
    RETURN;
}

PP(pp_ftrread)
{
    I32 result = my_stat(ARGS);
    djSP;
    if (result < 0)
	RETPUSHUNDEF;
    if (cando(S_IRUSR, 0, &PL_statcache))
	RETPUSHYES;
    RETPUSHNO;
}

PP(pp_ftrwrite)
{
    I32 result = my_stat(ARGS);
    djSP;
    if (result < 0)
	RETPUSHUNDEF;
    if (cando(S_IWUSR, 0, &PL_statcache))
	RETPUSHYES;
    RETPUSHNO;
}

PP(pp_ftrexec)
{
    I32 result = my_stat(ARGS);
    djSP;
    if (result < 0)
	RETPUSHUNDEF;
    if (cando(S_IXUSR, 0, &PL_statcache))
	RETPUSHYES;
    RETPUSHNO;
}

PP(pp_fteread)
{
    I32 result = my_stat(ARGS);
    djSP;
    if (result < 0)
	RETPUSHUNDEF;
    if (cando(S_IRUSR, 1, &PL_statcache))
	RETPUSHYES;
    RETPUSHNO;
}

PP(pp_ftewrite)
{
    I32 result = my_stat(ARGS);
    djSP;
    if (result < 0)
	RETPUSHUNDEF;
    if (cando(S_IWUSR, 1, &PL_statcache))
	RETPUSHYES;
    RETPUSHNO;
}

PP(pp_fteexec)
{
    I32 result = my_stat(ARGS);
    djSP;
    if (result < 0)
	RETPUSHUNDEF;
    if (cando(S_IXUSR, 1, &PL_statcache))
	RETPUSHYES;
    RETPUSHNO;
}

PP(pp_ftis)
{
    I32 result = my_stat(ARGS);
    djSP;
    if (result < 0)
	RETPUSHUNDEF;
    RETPUSHYES;
}

PP(pp_fteowned)
{
    return pp_ftrowned(ARGS);
}

PP(pp_ftrowned)
{
    I32 result = my_stat(ARGS);
    djSP;
    if (result < 0)
	RETPUSHUNDEF;
    if (PL_statcache.st_uid == (PL_op->op_type == OP_FTEOWNED ? PL_euid : PL_uid) )
	RETPUSHYES;
    RETPUSHNO;
}

PP(pp_ftzero)
{
    I32 result = my_stat(ARGS);
    djSP;
    if (result < 0)
	RETPUSHUNDEF;
    if (!PL_statcache.st_size)
	RETPUSHYES;
    RETPUSHNO;
}

PP(pp_ftsize)
{
    I32 result = my_stat(ARGS);
    djSP; dTARGET;
    if (result < 0)
	RETPUSHUNDEF;
    PUSHi(PL_statcache.st_size);
    RETURN;
}

PP(pp_ftmtime)
{
    I32 result = my_stat(ARGS);
    djSP; dTARGET;
    if (result < 0)
	RETPUSHUNDEF;
    PUSHn( ((I32)PL_basetime - (I32)PL_statcache.st_mtime) / 86400.0 );
    RETURN;
}

PP(pp_ftatime)
{
    I32 result = my_stat(ARGS);
    djSP; dTARGET;
    if (result < 0)
	RETPUSHUNDEF;
    PUSHn( ((I32)PL_basetime - (I32)PL_statcache.st_atime) / 86400.0 );
    RETURN;
}

PP(pp_ftctime)
{
    I32 result = my_stat(ARGS);
    djSP; dTARGET;
    if (result < 0)
	RETPUSHUNDEF;
    PUSHn( ((I32)PL_basetime - (I32)PL_statcache.st_ctime) / 86400.0 );
    RETURN;
}

PP(pp_ftsock)
{
    I32 result = my_stat(ARGS);
    djSP;
    if (result < 0)
	RETPUSHUNDEF;
    if (S_ISSOCK(PL_statcache.st_mode))
	RETPUSHYES;
    RETPUSHNO;
}

PP(pp_ftchr)
{
    I32 result = my_stat(ARGS);
    djSP;
    if (result < 0)
	RETPUSHUNDEF;
    if (S_ISCHR(PL_statcache.st_mode))
	RETPUSHYES;
    RETPUSHNO;
}

PP(pp_ftblk)
{
    I32 result = my_stat(ARGS);
    djSP;
    if (result < 0)
	RETPUSHUNDEF;
    if (S_ISBLK(PL_statcache.st_mode))
	RETPUSHYES;
    RETPUSHNO;
}

PP(pp_ftfile)
{
    I32 result = my_stat(ARGS);
    djSP;
    if (result < 0)
	RETPUSHUNDEF;
    if (S_ISREG(PL_statcache.st_mode))
	RETPUSHYES;
    RETPUSHNO;
}

PP(pp_ftdir)
{
    I32 result = my_stat(ARGS);
    djSP;
    if (result < 0)
	RETPUSHUNDEF;
    if (S_ISDIR(PL_statcache.st_mode))
	RETPUSHYES;
    RETPUSHNO;
}

PP(pp_ftpipe)
{
    I32 result = my_stat(ARGS);
    djSP;
    if (result < 0)
	RETPUSHUNDEF;
    if (S_ISFIFO(PL_statcache.st_mode))
	RETPUSHYES;
    RETPUSHNO;
}

PP(pp_ftlink)
{
    I32 result = my_lstat(ARGS);
    djSP;
    if (result < 0)
	RETPUSHUNDEF;
    if (S_ISLNK(PL_statcache.st_mode))
	RETPUSHYES;
    RETPUSHNO;
}

PP(pp_ftsuid)
{
    djSP;
#ifdef S_ISUID
    I32 result = my_stat(ARGS);
    SPAGAIN;
    if (result < 0)
	RETPUSHUNDEF;
    if (PL_statcache.st_mode & S_ISUID)
	RETPUSHYES;
#endif
    RETPUSHNO;
}

PP(pp_ftsgid)
{
    djSP;
#ifdef S_ISGID
    I32 result = my_stat(ARGS);
    SPAGAIN;
    if (result < 0)
	RETPUSHUNDEF;
    if (PL_statcache.st_mode & S_ISGID)
	RETPUSHYES;
#endif
    RETPUSHNO;
}

PP(pp_ftsvtx)
{
    djSP;
#ifdef S_ISVTX
    I32 result = my_stat(ARGS);
    SPAGAIN;
    if (result < 0)
	RETPUSHUNDEF;
    if (PL_statcache.st_mode & S_ISVTX)
	RETPUSHYES;
#endif
    RETPUSHNO;
}

PP(pp_fttty)
{
    djSP;
    int fd;
    GV *gv;
    char *tmps = Nullch;
    STRLEN n_a;

    if (PL_op->op_flags & OPf_REF)
	gv = cGVOP->op_gv;
    else if (isGV(TOPs))
	gv = (GV*)POPs;
    else if (SvROK(TOPs) && isGV(SvRV(TOPs)))
	gv = (GV*)SvRV(POPs);
    else
	gv = gv_fetchpv(tmps = POPpx, FALSE, SVt_PVIO);

    if (GvIO(gv) && IoIFP(GvIOp(gv)))
	fd = PerlIO_fileno(IoIFP(GvIOp(gv)));
    else if (tmps && isDIGIT(*tmps))
	fd = atoi(tmps);
    else
	RETPUSHUNDEF;
    if (PerlLIO_isatty(fd))
	RETPUSHYES;
    RETPUSHNO;
}

#if defined(atarist) /* this will work with atariST. Configure will
			make guesses for other systems. */
# define FILE_base(f) ((f)->_base)
# define FILE_ptr(f) ((f)->_ptr)
# define FILE_cnt(f) ((f)->_cnt)
# define FILE_bufsiz(f) ((f)->_cnt + ((f)->_ptr - (f)->_base))
#endif

PP(pp_fttext)
{
    djSP;
    I32 i;
    I32 len;
    I32 odd = 0;
    STDCHAR tbuf[512];
    register STDCHAR *s;
    register IO *io;
    register SV *sv;
    GV *gv;
    STRLEN n_a;

    if (PL_op->op_flags & OPf_REF)
	gv = cGVOP->op_gv;
    else if (isGV(TOPs))
	gv = (GV*)POPs;
    else if (SvROK(TOPs) && isGV(SvRV(TOPs)))
	gv = (GV*)SvRV(POPs);
    else
	gv = Nullgv;

    if (gv) {
	EXTEND(SP, 1);
	if (gv == PL_defgv) {
	    if (PL_statgv)
		io = GvIO(PL_statgv);
	    else {
		sv = PL_statname;
		goto really_filename;
	    }
	}
	else {
	    PL_statgv = gv;
	    PL_laststatval = -1;
	    sv_setpv(PL_statname, "");
	    io = GvIO(PL_statgv);
	}
	if (io && IoIFP(io)) {
	    if (! PerlIO_has_base(IoIFP(io)))
		DIE("-T and -B not implemented on filehandles");
	    PL_laststatval = PerlLIO_fstat(PerlIO_fileno(IoIFP(io)), &PL_statcache);
	    if (PL_laststatval < 0)
		RETPUSHUNDEF;
	    if (S_ISDIR(PL_statcache.st_mode))	/* handle NFS glitch */
		if (PL_op->op_type == OP_FTTEXT)
		    RETPUSHNO;
		else
		    RETPUSHYES;
	    if (PerlIO_get_cnt(IoIFP(io)) <= 0) {
		i = PerlIO_getc(IoIFP(io));
		if (i != EOF)
		    (void)PerlIO_ungetc(IoIFP(io),i);
	    }
	    if (PerlIO_get_cnt(IoIFP(io)) <= 0)	/* null file is anything */
		RETPUSHYES;
	    len = PerlIO_get_bufsiz(IoIFP(io));
	    s = (STDCHAR *) PerlIO_get_base(IoIFP(io));
	    /* sfio can have large buffers - limit to 512 */
	    if (len > 512)
		len = 512;
	}
	else {
	    if (PL_dowarn)
		warn("Test on unopened file <%s>",
		  GvENAME(cGVOP->op_gv));
	    SETERRNO(EBADF,RMS$_IFI);
	    RETPUSHUNDEF;
	}
    }
    else {
	sv = POPs;
      really_filename:
	PL_statgv = Nullgv;
	PL_laststatval = -1;
	sv_setpv(PL_statname, SvPV(sv, n_a));
#ifdef HAS_OPEN3
	i = PerlLIO_open3(SvPV(sv, n_a), O_RDONLY, 0);
#else
	i = PerlLIO_open(SvPV(sv, n_a), 0);
#endif
	if (i < 0) {
	    if (PL_dowarn && strchr(SvPV(sv, n_a), '\n'))
		warn(warn_nl, "open");
	    RETPUSHUNDEF;
	}
	PL_laststatval = PerlLIO_fstat(i, &PL_statcache);
	if (PL_laststatval < 0)
	    RETPUSHUNDEF;
	len = PerlLIO_read(i, tbuf, 512);
	(void)PerlLIO_close(i);
	if (len <= 0) {
	    if (S_ISDIR(PL_statcache.st_mode) && PL_op->op_type == OP_FTTEXT)
		RETPUSHNO;		/* special case NFS directories */
	    RETPUSHYES;		/* null file is anything */
	}
	s = tbuf;
    }

    /* now scan s to look for textiness */
    /*   XXX ASCII dependent code */

    for (i = 0; i < len; i++, s++) {
	if (!*s) {			/* null never allowed in text */
	    odd += len;
	    break;
	}
#ifdef EBCDIC
        else if (!(isPRINT(*s) || isSPACE(*s))) 
            odd++;
#else
	else if (*s & 128)
	    odd++;
	else if (*s < 32 &&
	  *s != '\n' && *s != '\r' && *s != '\b' &&
	  *s != '\t' && *s != '\f' && *s != 27)
	    odd++;
#endif
    }

    if ((odd * 3 > len) == (PL_op->op_type == OP_FTTEXT)) /* allow 1/3 odd */
	RETPUSHNO;
    else
	RETPUSHYES;
}

PP(pp_ftbinary)
{
    return pp_fttext(ARGS);
}

/* File calls. */

PP(pp_chdir)
{
    djSP; dTARGET;
    char *tmps;
    SV **svp;
    STRLEN n_a;

    if (MAXARG < 1)
	tmps = Nullch;
    else
	tmps = POPpx;
    if (!tmps || !*tmps) {
	svp = hv_fetch(GvHVn(PL_envgv), "HOME", 4, FALSE);
	if (svp)
	    tmps = SvPV(*svp, n_a);
    }
    if (!tmps || !*tmps) {
	svp = hv_fetch(GvHVn(PL_envgv), "LOGDIR", 6, FALSE);
	if (svp)
	    tmps = SvPV(*svp, n_a);
    }
#ifdef VMS
    if (!tmps || !*tmps) {
       svp = hv_fetch(GvHVn(PL_envgv), "SYS$LOGIN", 9, FALSE);
       if (svp)
           tmps = SvPV(*svp, n_a);
    }
#endif
    TAINT_PROPER("chdir");
    PUSHi( PerlDir_chdir(tmps) >= 0 );
#ifdef VMS
    /* Clear the DEFAULT element of ENV so we'll get the new value
     * in the future. */
    hv_delete(GvHVn(PL_envgv),"DEFAULT",7,G_DISCARD);
#endif
    RETURN;
}

PP(pp_chown)
{
    djSP; dMARK; dTARGET;
    I32 value;
#ifdef HAS_CHOWN
    value = (I32)apply(PL_op->op_type, MARK, SP);
    SP = MARK;
    PUSHi(value);
    RETURN;
#else
    DIE(no_func, "Unsupported function chown");
#endif
}

PP(pp_chroot)
{
    djSP; dTARGET;
    char *tmps;
    STRLEN n_a;
#ifdef HAS_CHROOT
    tmps = POPpx;
    TAINT_PROPER("chroot");
    PUSHi( chroot(tmps) >= 0 );
    RETURN;
#else
    DIE(no_func, "chroot");
#endif
}

PP(pp_unlink)
{
    djSP; dMARK; dTARGET;
    I32 value;
    value = (I32)apply(PL_op->op_type, MARK, SP);
    SP = MARK;
    PUSHi(value);
    RETURN;
}

PP(pp_chmod)
{
    djSP; dMARK; dTARGET;
    I32 value;
    value = (I32)apply(PL_op->op_type, MARK, SP);
    SP = MARK;
    PUSHi(value);
    RETURN;
}

PP(pp_utime)
{
    djSP; dMARK; dTARGET;
    I32 value;
    value = (I32)apply(PL_op->op_type, MARK, SP);
    SP = MARK;
    PUSHi(value);
    RETURN;
}

PP(pp_rename)
{
    djSP; dTARGET;
    int anum;
    STRLEN n_a;

    char *tmps2 = POPpx;
    char *tmps = SvPV(TOPs, n_a);
    TAINT_PROPER("rename");
#ifdef HAS_RENAME
    anum = PerlLIO_rename(tmps, tmps2);
#else
    if (!(anum = PerlLIO_stat(tmps, &PL_statbuf))) {
	if (same_dirent(tmps2, tmps))	/* can always rename to same name */
	    anum = 1;
	else {
	    if (PL_euid || PerlLIO_stat(tmps2, &PL_statbuf) < 0 || !S_ISDIR(PL_statbuf.st_mode))
		(void)UNLINK(tmps2);
	    if (!(anum = link(tmps, tmps2)))
		anum = UNLINK(tmps);
	}
    }
#endif
    SETi( anum >= 0 );
    RETURN;
}

PP(pp_link)
{
    djSP; dTARGET;
#ifdef HAS_LINK
    STRLEN n_a;
    char *tmps2 = POPpx;
    char *tmps = SvPV(TOPs, n_a);
    TAINT_PROPER("link");
    SETi( link(tmps, tmps2) >= 0 );
#else
    DIE(no_func, "Unsupported function link");
#endif
    RETURN;
}

PP(pp_symlink)
{
    djSP; dTARGET;
#ifdef HAS_SYMLINK
    STRLEN n_a;
    char *tmps2 = POPpx;
    char *tmps = SvPV(TOPs, n_a);
    TAINT_PROPER("symlink");
    SETi( symlink(tmps, tmps2) >= 0 );
    RETURN;
#else
    DIE(no_func, "symlink");
#endif
}

PP(pp_readlink)
{
    djSP; dTARGET;
#ifdef HAS_SYMLINK
    char *tmps;
    char buf[MAXPATHLEN];
    int len;
    STRLEN n_a;

#ifndef INCOMPLETE_TAINTS
    TAINT;
#endif
    tmps = POPpx;
    len = readlink(tmps, buf, sizeof buf);
    EXTEND(SP, 1);
    if (len < 0)
	RETPUSHUNDEF;
    PUSHp(buf, len);
    RETURN;
#else
    EXTEND(SP, 1);
    RETSETUNDEF;		/* just pretend it's a normal file */
#endif
}

#if !defined(HAS_MKDIR) || !defined(HAS_RMDIR)
static int
dooneliner(cmd, filename)
char *cmd;
char *filename;
{
    char *save_filename = filename;
    char *cmdline;
    char *s;
    PerlIO *myfp;
    int anum = 1;

    New(666, cmdline, strlen(cmd) + (strlen(filename) * 2) + 10, char);
    strcpy(cmdline, cmd);
    strcat(cmdline, " ");
    for (s = cmdline + strlen(cmdline); *filename; ) {
	*s++ = '\\';
	*s++ = *filename++;
    }
    strcpy(s, " 2>&1");
    myfp = PerlProc_popen(cmdline, "r");
    Safefree(cmdline);

    if (myfp) {
	SV *tmpsv = sv_newmortal();
	/* Need to save/restore 'PL_rs' ?? */
	s = sv_gets(tmpsv, myfp, 0);
	(void)PerlProc_pclose(myfp);
	if (s != Nullch) {
	    int e;
	    for (e = 1;
#ifdef HAS_SYS_ERRLIST
		 e <= sys_nerr
#endif
		 ; e++)
	    {
		/* you don't see this */
		char *errmsg =
#ifdef HAS_SYS_ERRLIST
		    sys_errlist[e]
#else
		    strerror(e)
#endif
		    ;
		if (!errmsg)
		    break;
		if (instr(s, errmsg)) {
		    SETERRNO(e,0);
		    return 0;
		}
	    }
	    SETERRNO(0,0);
#ifndef EACCES
#define EACCES EPERM
#endif
	    if (instr(s, "cannot make"))
		SETERRNO(EEXIST,RMS$_FEX);
	    else if (instr(s, "existing file"))
		SETERRNO(EEXIST,RMS$_FEX);
	    else if (instr(s, "ile exists"))
		SETERRNO(EEXIST,RMS$_FEX);
	    else if (instr(s, "non-exist"))
		SETERRNO(ENOENT,RMS$_FNF);
	    else if (instr(s, "does not exist"))
		SETERRNO(ENOENT,RMS$_FNF);
	    else if (instr(s, "not empty"))
		SETERRNO(EBUSY,SS$_DEVOFFLINE);
	    else if (instr(s, "cannot access"))
		SETERRNO(EACCES,RMS$_PRV);
	    else
		SETERRNO(EPERM,RMS$_PRV);
	    return 0;
	}
	else {	/* some mkdirs return no failure indication */
	    anum = (PerlLIO_stat(save_filename, &PL_statbuf) >= 0);
	    if (PL_op->op_type == OP_RMDIR)
		anum = !anum;
	    if (anum)
		SETERRNO(0,0);
	    else
		SETERRNO(EACCES,RMS$_PRV);	/* a guess */
	}
	return anum;
    }
    else
	return 0;
}
#endif

PP(pp_mkdir)
{
    djSP; dTARGET;
    int mode = POPi;
#ifndef HAS_MKDIR
    int oldumask;
#endif
    STRLEN n_a;
    char *tmps = SvPV(TOPs, n_a);

    TAINT_PROPER("mkdir");
#ifdef HAS_MKDIR
    SETi( PerlDir_mkdir(tmps, mode) >= 0 );
#else
    SETi( dooneliner("mkdir", tmps) );
    oldumask = PerlLIO_umask(0);
    PerlLIO_umask(oldumask);
    PerlLIO_chmod(tmps, (mode & ~oldumask) & 0777);
#endif
    RETURN;
}

PP(pp_rmdir)
{
    djSP; dTARGET;
    char *tmps;
    STRLEN n_a;

    tmps = POPpx;
    TAINT_PROPER("rmdir");
#ifdef HAS_RMDIR
    XPUSHi( PerlDir_rmdir(tmps) >= 0 );
#else
    XPUSHi( dooneliner("rmdir", tmps) );
#endif
    RETURN;
}

/* Directory calls. */

PP(pp_open_dir)
{
    djSP;
#if defined(Direntry_t) && defined(HAS_READDIR)
    STRLEN n_a;
    char *dirname = POPpx;
    GV *gv = (GV*)POPs;
    register IO *io = GvIOn(gv);

    if (!io)
	goto nope;

    if (IoDIRP(io))
	PerlDir_close(IoDIRP(io));
    if (!(IoDIRP(io) = PerlDir_open(dirname)))
	goto nope;

    RETPUSHYES;
nope:
    if (!errno)
	SETERRNO(EBADF,RMS$_DIR);
    RETPUSHUNDEF;
#else
    DIE(no_dir_func, "opendir");
#endif
}

PP(pp_readdir)
{
    djSP;
#if defined(Direntry_t) && defined(HAS_READDIR)
#ifndef I_DIRENT
    Direntry_t *readdir _((DIR *));
#endif
    register Direntry_t *dp;
    GV *gv = (GV*)POPs;
    register IO *io = GvIOn(gv);
    SV *sv;

    if (!io || !IoDIRP(io))
	goto nope;

    if (GIMME == G_ARRAY) {
	/*SUPPRESS 560*/
	while (dp = (Direntry_t *)PerlDir_read(IoDIRP(io))) {
#ifdef DIRNAMLEN
	    sv = newSVpv(dp->d_name, dp->d_namlen);
#else
	    sv = newSVpv(dp->d_name, 0);
#endif
#ifndef INCOMPLETE_TAINTS
  	    SvTAINTED_on(sv);
#endif
	    XPUSHs(sv_2mortal(sv));
	}
    }
    else {
	if (!(dp = (Direntry_t *)PerlDir_read(IoDIRP(io))))
	    goto nope;
#ifdef DIRNAMLEN
	sv = newSVpv(dp->d_name, dp->d_namlen);
#else
	sv = newSVpv(dp->d_name, 0);
#endif
#ifndef INCOMPLETE_TAINTS
	SvTAINTED_on(sv);
#endif
	XPUSHs(sv_2mortal(sv));
    }
    RETURN;

nope:
    if (!errno)
	SETERRNO(EBADF,RMS$_ISI);
    if (GIMME == G_ARRAY)
	RETURN;
    else
	RETPUSHUNDEF;
#else
    DIE(no_dir_func, "readdir");
#endif
}

PP(pp_telldir)
{
    djSP; dTARGET;
#if defined(HAS_TELLDIR) || defined(telldir)
# ifdef NEED_TELLDIR_PROTO /* XXX does _anyone_ need this? --AD 2/20/1998 */
    long telldir _((DIR *));
# endif
    GV *gv = (GV*)POPs;
    register IO *io = GvIOn(gv);

    if (!io || !IoDIRP(io))
	goto nope;

    PUSHi( PerlDir_tell(IoDIRP(io)) );
    RETURN;
nope:
    if (!errno)
	SETERRNO(EBADF,RMS$_ISI);
    RETPUSHUNDEF;
#else
    DIE(no_dir_func, "telldir");
#endif
}

PP(pp_seekdir)
{
    djSP;
#if defined(HAS_SEEKDIR) || defined(seekdir)
    long along = POPl;
    GV *gv = (GV*)POPs;
    register IO *io = GvIOn(gv);

    if (!io || !IoDIRP(io))
	goto nope;

    (void)PerlDir_seek(IoDIRP(io), along);

    RETPUSHYES;
nope:
    if (!errno)
	SETERRNO(EBADF,RMS$_ISI);
    RETPUSHUNDEF;
#else
    DIE(no_dir_func, "seekdir");
#endif
}

PP(pp_rewinddir)
{
    djSP;
#if defined(HAS_REWINDDIR) || defined(rewinddir)
    GV *gv = (GV*)POPs;
    register IO *io = GvIOn(gv);

    if (!io || !IoDIRP(io))
	goto nope;

    (void)PerlDir_rewind(IoDIRP(io));
    RETPUSHYES;
nope:
    if (!errno)
	SETERRNO(EBADF,RMS$_ISI);
    RETPUSHUNDEF;
#else
    DIE(no_dir_func, "rewinddir");
#endif
}

PP(pp_closedir)
{
    djSP;
#if defined(Direntry_t) && defined(HAS_READDIR)
    GV *gv = (GV*)POPs;
    register IO *io = GvIOn(gv);

    if (!io || !IoDIRP(io))
	goto nope;

#ifdef VOID_CLOSEDIR
    PerlDir_close(IoDIRP(io));
#else
    if (PerlDir_close(IoDIRP(io)) < 0) {
	IoDIRP(io) = 0; /* Don't try to close again--coredumps on SysV */
	goto nope;
    }
#endif
    IoDIRP(io) = 0;

    RETPUSHYES;
nope:
    if (!errno)
	SETERRNO(EBADF,RMS$_IFI);
    RETPUSHUNDEF;
#else
    DIE(no_dir_func, "closedir");
#endif
}

/* Process control. */

PP(pp_fork)
{
#ifdef HAS_FORK
    djSP; dTARGET;
    int childpid;
    GV *tmpgv;

    EXTEND(SP, 1);
    childpid = fork();
    if (childpid < 0)
	RETSETUNDEF;
    if (!childpid) {
	/*SUPPRESS 560*/
	if (tmpgv = gv_fetchpv("$", TRUE, SVt_PV))
	    sv_setiv(GvSV(tmpgv), (IV)getpid());
	hv_clear(PL_pidstatus);	/* no kids, so don't wait for 'em */
    }
    PUSHi(childpid);
    RETURN;
#else
    DIE(no_func, "Unsupported function fork");
#endif
}

PP(pp_wait)
{
#if !defined(DOSISH) || defined(OS2) || defined(WIN32)
    djSP; dTARGET;
    int childpid;
    int argflags;

    childpid = wait4pid(-1, &argflags, 0);
    STATUS_NATIVE_SET((childpid > 0) ? argflags : -1);
    XPUSHi(childpid);
    RETURN;
#else
    DIE(no_func, "Unsupported function wait");
#endif
}

PP(pp_waitpid)
{
#if !defined(DOSISH) || defined(OS2) || defined(WIN32)
    djSP; dTARGET;
    int childpid;
    int optype;
    int argflags;

    optype = POPi;
    childpid = TOPi;
    childpid = wait4pid(childpid, &argflags, optype);
    STATUS_NATIVE_SET((childpid > 0) ? argflags : -1);
    SETi(childpid);
    RETURN;
#else
    DIE(no_func, "Unsupported function waitpid");
#endif
}

PP(pp_system)
{
    djSP; dMARK; dORIGMARK; dTARGET;
    I32 value;
    int childpid;
    int result;
    int status;
    Sigsave_t ihand,qhand;     /* place to save signals during system() */
    STRLEN n_a;

    if (SP - MARK == 1) {
	if (PL_tainting) {
	    char *junk = SvPV(TOPs, n_a);
	    TAINT_ENV();
	    TAINT_PROPER("system");
	}
    }
#if (defined(HAS_FORK) || defined(AMIGAOS)) && !defined(VMS) && !defined(OS2)
    while ((childpid = vfork()) == -1) {
	if (errno != EAGAIN) {
	    value = -1;
	    SP = ORIGMARK;
	    PUSHi(value);
	    RETURN;
	}
	sleep(5);
    }
    if (childpid > 0) {
	rsignal_save(SIGINT, SIG_IGN, &ihand);
	rsignal_save(SIGQUIT, SIG_IGN, &qhand);
	do {
	    result = wait4pid(childpid, &status, 0);
	} while (result == -1 && errno == EINTR);
	(void)rsignal_restore(SIGINT, &ihand);
	(void)rsignal_restore(SIGQUIT, &qhand);
	STATUS_NATIVE_SET(result == -1 ? -1 : status);
	do_execfree();	/* free any memory child malloced on vfork */
	SP = ORIGMARK;
	PUSHi(STATUS_CURRENT);
	RETURN;
    }
    if (PL_op->op_flags & OPf_STACKED) {
	SV *really = *++MARK;
	value = (I32)do_aexec(really, MARK, SP);
    }
    else if (SP - MARK != 1)
	value = (I32)do_aexec(Nullsv, MARK, SP);
    else {
	value = (I32)do_exec(SvPVx(sv_mortalcopy(*SP), n_a));
    }
    PerlProc__exit(-1);
#else /* ! FORK or VMS or OS/2 */
    if (PL_op->op_flags & OPf_STACKED) {
	SV *really = *++MARK;
	value = (I32)do_aspawn(really, (void **)MARK, (void **)SP);
    }
    else if (SP - MARK != 1)
	value = (I32)do_aspawn(Nullsv, (void **)MARK, (void **)SP);
    else {
	value = (I32)do_spawn(SvPVx(sv_mortalcopy(*SP), n_a));
    }
    STATUS_NATIVE_SET(value);
    do_execfree();
    SP = ORIGMARK;
    PUSHi(STATUS_CURRENT);
#endif /* !FORK or VMS */
    RETURN;
}

PP(pp_exec)
{
    djSP; dMARK; dORIGMARK; dTARGET;
    I32 value;
    STRLEN n_a;

    if (PL_op->op_flags & OPf_STACKED) {
	SV *really = *++MARK;
	value = (I32)do_aexec(really, MARK, SP);
    }
    else if (SP - MARK != 1)
#ifdef VMS
	value = (I32)vms_do_aexec(Nullsv, MARK, SP);
#else
	value = (I32)do_aexec(Nullsv, MARK, SP);
#endif
    else {
	if (PL_tainting) {
	    char *junk = SvPV(*SP, n_a);
	    TAINT_ENV();
	    TAINT_PROPER("exec");
	}
#ifdef VMS
	value = (I32)vms_do_exec(SvPVx(sv_mortalcopy(*SP), n_a));
#else
	value = (I32)do_exec(SvPVx(sv_mortalcopy(*SP), n_a));
#endif
    }
    SP = ORIGMARK;
    PUSHi(value);
    RETURN;
}

PP(pp_kill)
{
    djSP; dMARK; dTARGET;
    I32 value;
#ifdef HAS_KILL
    value = (I32)apply(PL_op->op_type, MARK, SP);
    SP = MARK;
    PUSHi(value);
    RETURN;
#else
    DIE(no_func, "Unsupported function kill");
#endif
}

PP(pp_getppid)
{
#ifdef HAS_GETPPID
    djSP; dTARGET;
    XPUSHi( getppid() );
    RETURN;
#else
    DIE(no_func, "getppid");
#endif
}

PP(pp_getpgrp)
{
#ifdef HAS_GETPGRP
    djSP; dTARGET;
    int pid;
    I32 value;

    if (MAXARG < 1)
	pid = 0;
    else
	pid = SvIVx(POPs);
#ifdef BSD_GETPGRP
    value = (I32)BSD_GETPGRP(pid);
#else
    if (pid != 0 && pid != getpid())
	DIE("POSIX getpgrp can't take an argument");
    value = (I32)getpgrp();
#endif
    XPUSHi(value);
    RETURN;
#else
    DIE(no_func, "getpgrp()");
#endif
}

PP(pp_setpgrp)
{
#ifdef HAS_SETPGRP
    djSP; dTARGET;
    int pgrp;
    int pid;
    if (MAXARG < 2) {
	pgrp = 0;
	pid = 0;
    }
    else {
	pgrp = POPi;
	pid = TOPi;
    }

    TAINT_PROPER("setpgrp");
#ifdef BSD_SETPGRP
    SETi( BSD_SETPGRP(pid, pgrp) >= 0 );
#else
    if ((pgrp != 0 && pgrp != getpid()) || (pid != 0 && pid != getpid()))
	DIE("POSIX setpgrp can't take an argument");
    SETi( setpgrp() >= 0 );
#endif /* USE_BSDPGRP */
    RETURN;
#else
    DIE(no_func, "setpgrp()");
#endif
}

PP(pp_getpriority)
{
    djSP; dTARGET;
    int which;
    int who;
#ifdef HAS_GETPRIORITY
    who = POPi;
    which = TOPi;
    SETi( getpriority(which, who) );
    RETURN;
#else
    DIE(no_func, "getpriority()");
#endif
}

PP(pp_setpriority)
{
    djSP; dTARGET;
    int which;
    int who;
    int niceval;
#ifdef HAS_SETPRIORITY
    niceval = POPi;
    who = POPi;
    which = TOPi;
    TAINT_PROPER("setpriority");
    SETi( setpriority(which, who, niceval) >= 0 );
    RETURN;
#else
    DIE(no_func, "setpriority()");
#endif
}

/* Time calls. */

PP(pp_time)
{
    djSP; dTARGET;
#ifdef BIG_TIME
    XPUSHn( time(Null(Time_t*)) );
#else
    XPUSHi( time(Null(Time_t*)) );
#endif
    RETURN;
}

/* XXX The POSIX name is CLK_TCK; it is to be preferred
   to HZ.  Probably.  For now, assume that if the system
   defines HZ, it does so correctly.  (Will this break
   on VMS?)
   Probably we ought to use _sysconf(_SC_CLK_TCK), if
   it's supported.    --AD  9/96.
*/

#ifndef HZ
#  ifdef CLK_TCK
#    define HZ CLK_TCK
#  else
#    define HZ 60
#  endif
#endif

PP(pp_tms)
{
    djSP;

#ifndef HAS_TIMES
    DIE("times not implemented");
#else
    EXTEND(SP, 4);

#ifndef VMS
    (void)PerlProc_times(&PL_timesbuf);
#else
    (void)PerlProc_times((tbuffer_t *)&PL_timesbuf);  /* time.h uses different name for */
                                                   /* struct tms, though same data   */
                                                   /* is returned.                   */
#endif

    PUSHs(sv_2mortal(newSVnv(((double)PL_timesbuf.tms_utime)/HZ)));
    if (GIMME == G_ARRAY) {
	PUSHs(sv_2mortal(newSVnv(((double)PL_timesbuf.tms_stime)/HZ)));
	PUSHs(sv_2mortal(newSVnv(((double)PL_timesbuf.tms_cutime)/HZ)));
	PUSHs(sv_2mortal(newSVnv(((double)PL_timesbuf.tms_cstime)/HZ)));
    }
    RETURN;
#endif /* HAS_TIMES */
}

PP(pp_localtime)
{
    return pp_gmtime(ARGS);
}

PP(pp_gmtime)
{
    djSP;
    Time_t when;
    struct tm *tmbuf;
    static char *dayname[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static char *monname[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
			      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    if (MAXARG < 1)
	(void)time(&when);
    else
#ifdef BIG_TIME
	when = (Time_t)SvNVx(POPs);
#else
	when = (Time_t)SvIVx(POPs);
#endif

    if (PL_op->op_type == OP_LOCALTIME)
	tmbuf = localtime(&when);
    else
	tmbuf = gmtime(&when);

    EXTEND(SP, 9);
    EXTEND_MORTAL(9);
    if (GIMME != G_ARRAY) {
	dTARGET;
	SV *tsv;
	if (!tmbuf)
	    RETPUSHUNDEF;
	tsv = newSVpvf("%s %s %2d %02d:%02d:%02d %d",
		       dayname[tmbuf->tm_wday],
		       monname[tmbuf->tm_mon],
		       tmbuf->tm_mday,
		       tmbuf->tm_hour,
		       tmbuf->tm_min,
		       tmbuf->tm_sec,
		       tmbuf->tm_year + 1900);
	PUSHs(sv_2mortal(tsv));
    }
    else if (tmbuf) {
	PUSHs(sv_2mortal(newSViv((I32)tmbuf->tm_sec)));
	PUSHs(sv_2mortal(newSViv((I32)tmbuf->tm_min)));
	PUSHs(sv_2mortal(newSViv((I32)tmbuf->tm_hour)));
	PUSHs(sv_2mortal(newSViv((I32)tmbuf->tm_mday)));
	PUSHs(sv_2mortal(newSViv((I32)tmbuf->tm_mon)));
	PUSHs(sv_2mortal(newSViv((I32)tmbuf->tm_year)));
	PUSHs(sv_2mortal(newSViv((I32)tmbuf->tm_wday)));
	PUSHs(sv_2mortal(newSViv((I32)tmbuf->tm_yday)));
	PUSHs(sv_2mortal(newSViv((I32)tmbuf->tm_isdst)));
    }
    RETURN;
}

PP(pp_alarm)
{
    djSP; dTARGET;
    int anum;
#ifdef HAS_ALARM
    anum = POPi;
    anum = alarm((unsigned int)anum);
    EXTEND(SP, 1);
    if (anum < 0)
	RETPUSHUNDEF;
    PUSHi((I32)anum);
    RETURN;
#else
    DIE(no_func, "Unsupported function alarm");
#endif
}

PP(pp_sleep)
{
    djSP; dTARGET;
    I32 duration;
    Time_t lasttime;
    Time_t when;

    (void)time(&lasttime);
    if (MAXARG < 1)
	PerlProc_pause();
    else {
	duration = POPi;
	PerlProc_sleep((unsigned int)duration);
    }
    (void)time(&when);
    XPUSHi(when - lasttime);
    RETURN;
}

/* Shared memory. */

PP(pp_shmget)
{
    return pp_semget(ARGS);
}

PP(pp_shmctl)
{
    return pp_semctl(ARGS);
}

PP(pp_shmread)
{
    return pp_shmwrite(ARGS);
}

PP(pp_shmwrite)
{
#if defined(HAS_MSG) || defined(HAS_SEM) || defined(HAS_SHM)
    djSP; dMARK; dTARGET;
    I32 value = (I32)(do_shmio(PL_op->op_type, MARK, SP) >= 0);
    SP = MARK;
    PUSHi(value);
    RETURN;
#else
    return pp_semget(ARGS);
#endif
}

/* Message passing. */

PP(pp_msgget)
{
    return pp_semget(ARGS);
}

PP(pp_msgctl)
{
    return pp_semctl(ARGS);
}

PP(pp_msgsnd)
{
#if defined(HAS_MSG) || defined(HAS_SEM) || defined(HAS_SHM)
    djSP; dMARK; dTARGET;
    I32 value = (I32)(do_msgsnd(MARK, SP) >= 0);
    SP = MARK;
    PUSHi(value);
    RETURN;
#else
    return pp_semget(ARGS);
#endif
}

PP(pp_msgrcv)
{
#if defined(HAS_MSG) || defined(HAS_SEM) || defined(HAS_SHM)
    djSP; dMARK; dTARGET;
    I32 value = (I32)(do_msgrcv(MARK, SP) >= 0);
    SP = MARK;
    PUSHi(value);
    RETURN;
#else
    return pp_semget(ARGS);
#endif
}

/* Semaphores. */

PP(pp_semget)
{
#if defined(HAS_MSG) || defined(HAS_SEM) || defined(HAS_SHM)
    djSP; dMARK; dTARGET;
    int anum = do_ipcget(PL_op->op_type, MARK, SP);
    SP = MARK;
    if (anum == -1)
	RETPUSHUNDEF;
    PUSHi(anum);
    RETURN;
#else
    DIE("System V IPC is not implemented on this machine");
#endif
}

PP(pp_semctl)
{
#if defined(HAS_MSG) || defined(HAS_SEM) || defined(HAS_SHM)
    djSP; dMARK; dTARGET;
    int anum = do_ipcctl(PL_op->op_type, MARK, SP);
    SP = MARK;
    if (anum == -1)
	RETSETUNDEF;
    if (anum != 0) {
	PUSHi(anum);
    }
    else {
	PUSHp(zero_but_true, ZBTLEN);
    }
    RETURN;
#else
    return pp_semget(ARGS);
#endif
}

PP(pp_semop)
{
#if defined(HAS_MSG) || defined(HAS_SEM) || defined(HAS_SHM)
    djSP; dMARK; dTARGET;
    I32 value = (I32)(do_semop(MARK, SP) >= 0);
    SP = MARK;
    PUSHi(value);
    RETURN;
#else
    return pp_semget(ARGS);
#endif
}

/* Get system info. */

PP(pp_ghbyname)
{
#ifdef HAS_GETHOSTBYNAME
    return pp_ghostent(ARGS);
#else
    DIE(no_sock_func, "gethostbyname");
#endif
}

PP(pp_ghbyaddr)
{
#ifdef HAS_GETHOSTBYADDR
    return pp_ghostent(ARGS);
#else
    DIE(no_sock_func, "gethostbyaddr");
#endif
}

PP(pp_ghostent)
{
    djSP;
#if defined(HAS_GETHOSTBYNAME) || defined(HAS_GETHOSTBYADDR) || defined(HAS_GETHOSTENT)
    I32 which = PL_op->op_type;
    register char **elem;
    register SV *sv;
#ifndef HAS_GETHOST_PROTOS /* XXX Do we need individual probes? */
    struct hostent *PerlSock_gethostbyaddr(Netdb_host_t, Netdb_hlen_t, int);
    struct hostent *PerlSock_gethostbyname(Netdb_name_t);
    struct hostent *PerlSock_gethostent(void);
#endif
    struct hostent *hent;
    unsigned long len;

    EXTEND(SP, 10);
    if (which == OP_GHBYNAME) {
#ifdef HAS_GETHOSTBYNAME
	STRLEN n_a;
	hent = PerlSock_gethostbyname(POPpx);
#else
	DIE(no_sock_func, "gethostbyname");
#endif
    }
    else if (which == OP_GHBYADDR) {
#ifdef HAS_GETHOSTBYADDR
	int addrtype = POPi;
	SV *addrsv = POPs;
	STRLEN addrlen;
	Netdb_host_t addr = (Netdb_host_t) SvPV(addrsv, addrlen);

	hent = PerlSock_gethostbyaddr(addr, (Netdb_hlen_t) addrlen, addrtype);
#else
	DIE(no_sock_func, "gethostbyaddr");
#endif
    }
    else
#ifdef HAS_GETHOSTENT
	hent = PerlSock_gethostent();
#else
	DIE(no_sock_func, "gethostent");
#endif

#ifdef HOST_NOT_FOUND
    if (!hent)
	STATUS_NATIVE_SET(h_errno);
#endif

    if (GIMME != G_ARRAY) {
	PUSHs(sv = sv_newmortal());
	if (hent) {
	    if (which == OP_GHBYNAME) {
		if (hent->h_addr)
		    sv_setpvn(sv, hent->h_addr, hent->h_length);
	    }
	    else
		sv_setpv(sv, (char*)hent->h_name);
	}
	RETURN;
    }

    if (hent) {
	PUSHs(sv = sv_mortalcopy(&PL_sv_no));
	sv_setpv(sv, (char*)hent->h_name);
	PUSHs(sv = sv_mortalcopy(&PL_sv_no));
	for (elem = hent->h_aliases; elem && *elem; elem++) {
	    sv_catpv(sv, *elem);
	    if (elem[1])
		sv_catpvn(sv, " ", 1);
	}
	PUSHs(sv = sv_mortalcopy(&PL_sv_no));
	sv_setiv(sv, (IV)hent->h_addrtype);
	PUSHs(sv = sv_mortalcopy(&PL_sv_no));
	len = hent->h_length;
	sv_setiv(sv, (IV)len);
#ifdef h_addr
	for (elem = hent->h_addr_list; elem && *elem; elem++) {
	    XPUSHs(sv = sv_mortalcopy(&PL_sv_no));
	    sv_setpvn(sv, *elem, len);
	}
#else
	PUSHs(sv = sv_mortalcopy(&PL_sv_no));
	if (hent->h_addr)
	    sv_setpvn(sv, hent->h_addr, len);
#endif /* h_addr */
    }
    RETURN;
#else
    DIE(no_sock_func, "gethostent");
#endif
}

PP(pp_gnbyname)
{
#ifdef HAS_GETNETBYNAME
    return pp_gnetent(ARGS);
#else
    DIE(no_sock_func, "getnetbyname");
#endif
}

PP(pp_gnbyaddr)
{
#ifdef HAS_GETNETBYADDR
    return pp_gnetent(ARGS);
#else
    DIE(no_sock_func, "getnetbyaddr");
#endif
}

PP(pp_gnetent)
{
    djSP;
#if defined(HAS_GETNETBYNAME) || defined(HAS_GETNETBYADDR) || defined(HAS_GETNETENT)
    I32 which = PL_op->op_type;
    register char **elem;
    register SV *sv;
#ifndef HAS_GETNET_PROTOS /* XXX Do we need individual probes? */
    struct netent *PerlSock_getnetbyaddr(Netdb_net_t, int);
    struct netent *PerlSock_getnetbyname(Netdb_name_t);
    struct netent *PerlSock_getnetent(void);
#endif
    struct netent *nent;

    if (which == OP_GNBYNAME) {
#ifdef HAS_GETNETBYNAME
	STRLEN n_a;
	nent = PerlSock_getnetbyname(POPpx);
#else
        DIE(no_sock_func, "getnetbyname");
#endif
    }
    else if (which == OP_GNBYADDR) {
#ifdef HAS_GETNETBYADDR
	int addrtype = POPi;
	Netdb_net_t addr = (Netdb_net_t) U_L(POPn);
	nent = PerlSock_getnetbyaddr(addr, addrtype);
#else
	DIE(no_sock_func, "getnetbyaddr");
#endif
    }
    else
#ifdef HAS_GETNETENT
	nent = PerlSock_getnetent();
#else
        DIE(no_sock_func, "getnetent");
#endif

    EXTEND(SP, 4);
    if (GIMME != G_ARRAY) {
	PUSHs(sv = sv_newmortal());
	if (nent) {
	    if (which == OP_GNBYNAME)
		sv_setiv(sv, (IV)nent->n_net);
	    else
		sv_setpv(sv, nent->n_name);
	}
	RETURN;
    }

    if (nent) {
	PUSHs(sv = sv_mortalcopy(&PL_sv_no));
	sv_setpv(sv, nent->n_name);
	PUSHs(sv = sv_mortalcopy(&PL_sv_no));
	for (elem = nent->n_aliases; elem && *elem; elem++) {
	    sv_catpv(sv, *elem);
	    if (elem[1])
		sv_catpvn(sv, " ", 1);
	}
	PUSHs(sv = sv_mortalcopy(&PL_sv_no));
	sv_setiv(sv, (IV)nent->n_addrtype);
	PUSHs(sv = sv_mortalcopy(&PL_sv_no));
	sv_setiv(sv, (IV)nent->n_net);
    }

    RETURN;
#else
    DIE(no_sock_func, "getnetent");
#endif
}

PP(pp_gpbyname)
{
#ifdef HAS_GETPROTOBYNAME
    return pp_gprotoent(ARGS);
#else
    DIE(no_sock_func, "getprotobyname");
#endif
}

PP(pp_gpbynumber)
{
#ifdef HAS_GETPROTOBYNUMBER
    return pp_gprotoent(ARGS);
#else
    DIE(no_sock_func, "getprotobynumber");
#endif
}

PP(pp_gprotoent)
{
    djSP;
#if defined(HAS_GETPROTOBYNAME) || defined(HAS_GETPROTOBYNUMBER) || defined(HAS_GETPROTOENT)
    I32 which = PL_op->op_type;
    register char **elem;
    register SV *sv;  
#ifndef HAS_GETPROTO_PROTOS /* XXX Do we need individual probes? */
    struct protoent *PerlSock_getprotobyname(Netdb_name_t);
    struct protoent *PerlSock_getprotobynumber(int);
    struct protoent *PerlSock_getprotoent(void);
#endif
    struct protoent *pent;

    if (which == OP_GPBYNAME) {
#ifdef HAS_GETPROTOBYNAME
	STRLEN n_a;
	pent = PerlSock_getprotobyname(POPpx);
#else
	DIE(no_sock_func, "getprotobyname");
#endif
    }
    else if (which == OP_GPBYNUMBER)
#ifdef HAS_GETPROTOBYNUMBER
	pent = PerlSock_getprotobynumber(POPi);
#else
    DIE(no_sock_func, "getprotobynumber");
#endif
    else
#ifdef HAS_GETPROTOENT
	pent = PerlSock_getprotoent();
#else
	DIE(no_sock_func, "getprotoent");
#endif

    EXTEND(SP, 3);
    if (GIMME != G_ARRAY) {
	PUSHs(sv = sv_newmortal());
	if (pent) {
	    if (which == OP_GPBYNAME)
		sv_setiv(sv, (IV)pent->p_proto);
	    else
		sv_setpv(sv, pent->p_name);
	}
	RETURN;
    }

    if (pent) {
	PUSHs(sv = sv_mortalcopy(&PL_sv_no));
	sv_setpv(sv, pent->p_name);
	PUSHs(sv = sv_mortalcopy(&PL_sv_no));
	for (elem = pent->p_aliases; elem && *elem; elem++) {
	    sv_catpv(sv, *elem);
	    if (elem[1])
		sv_catpvn(sv, " ", 1);
	}
	PUSHs(sv = sv_mortalcopy(&PL_sv_no));
	sv_setiv(sv, (IV)pent->p_proto);
    }

    RETURN;
#else
    DIE(no_sock_func, "getprotoent");
#endif
}

PP(pp_gsbyname)
{
#ifdef HAS_GETSERVBYNAME
    return pp_gservent(ARGS);
#else
    DIE(no_sock_func, "getservbyname");
#endif
}

PP(pp_gsbyport)
{
#ifdef HAS_GETSERVBYPORT
    return pp_gservent(ARGS);
#else
    DIE(no_sock_func, "getservbyport");
#endif
}

PP(pp_gservent)
{
    djSP;
#if defined(HAS_GETSERVBYNAME) || defined(HAS_GETSERVBYPORT) || defined(HAS_GETSERVENT)
    I32 which = PL_op->op_type;
    register char **elem;
    register SV *sv;
#ifndef HAS_GETSERV_PROTOS /* XXX Do we need individual probes? */
    struct servent *PerlSock_getservbyname(Netdb_name_t, Netdb_name_t);
    struct servent *PerlSock_getservbyport(int, Netdb_name_t);
    struct servent *PerlSock_getservent(void);
#endif
    struct servent *sent;

    if (which == OP_GSBYNAME) {
#ifdef HAS_GETSERVBYNAME
	STRLEN n_a;
	char *proto = POPpx;
	char *name = POPpx;

	if (proto && !*proto)
	    proto = Nullch;

	sent = PerlSock_getservbyname(name, proto);
#else
	DIE(no_sock_func, "getservbyname");
#endif
    }
    else if (which == OP_GSBYPORT) {
#ifdef HAS_GETSERVBYPORT
	STRLEN n_a;
	char *proto = POPpx;
	unsigned short port = POPu;

#ifdef HAS_HTONS
	port = PerlSock_htons(port);
#endif
	sent = PerlSock_getservbyport(port, proto);
#else
	DIE(no_sock_func, "getservbyport");
#endif
    }
    else
#ifdef HAS_GETSERVENT
	sent = PerlSock_getservent();
#else
	DIE(no_sock_func, "getservent");
#endif

    EXTEND(SP, 4);
    if (GIMME != G_ARRAY) {
	PUSHs(sv = sv_newmortal());
	if (sent) {
	    if (which == OP_GSBYNAME) {
#ifdef HAS_NTOHS
		sv_setiv(sv, (IV)PerlSock_ntohs(sent->s_port));
#else
		sv_setiv(sv, (IV)(sent->s_port));
#endif
	    }
	    else
		sv_setpv(sv, sent->s_name);
	}
	RETURN;
    }

    if (sent) {
	PUSHs(sv = sv_mortalcopy(&PL_sv_no));
	sv_setpv(sv, sent->s_name);
	PUSHs(sv = sv_mortalcopy(&PL_sv_no));
	for (elem = sent->s_aliases; elem && *elem; elem++) {
	    sv_catpv(sv, *elem);
	    if (elem[1])
		sv_catpvn(sv, " ", 1);
	}
	PUSHs(sv = sv_mortalcopy(&PL_sv_no));
#ifdef HAS_NTOHS
	sv_setiv(sv, (IV)PerlSock_ntohs(sent->s_port));
#else
	sv_setiv(sv, (IV)(sent->s_port));
#endif
	PUSHs(sv = sv_mortalcopy(&PL_sv_no));
	sv_setpv(sv, sent->s_proto);
    }

    RETURN;
#else
    DIE(no_sock_func, "getservent");
#endif
}

PP(pp_shostent)
{
    djSP;
#ifdef HAS_SETHOSTENT
    PerlSock_sethostent(TOPi);
    RETSETYES;
#else
    DIE(no_sock_func, "sethostent");
#endif
}

PP(pp_snetent)
{
    djSP;
#ifdef HAS_SETNETENT
    PerlSock_setnetent(TOPi);
    RETSETYES;
#else
    DIE(no_sock_func, "setnetent");
#endif
}

PP(pp_sprotoent)
{
    djSP;
#ifdef HAS_SETPROTOENT
    PerlSock_setprotoent(TOPi);
    RETSETYES;
#else
    DIE(no_sock_func, "setprotoent");
#endif
}

PP(pp_sservent)
{
    djSP;
#ifdef HAS_SETSERVENT
    PerlSock_setservent(TOPi);
    RETSETYES;
#else
    DIE(no_sock_func, "setservent");
#endif
}

PP(pp_ehostent)
{
    djSP;
#ifdef HAS_ENDHOSTENT
    PerlSock_endhostent();
    EXTEND(SP,1);
    RETPUSHYES;
#else
    DIE(no_sock_func, "endhostent");
#endif
}

PP(pp_enetent)
{
    djSP;
#ifdef HAS_ENDNETENT
    PerlSock_endnetent();
    EXTEND(SP,1);
    RETPUSHYES;
#else
    DIE(no_sock_func, "endnetent");
#endif
}

PP(pp_eprotoent)
{
    djSP;
#ifdef HAS_ENDPROTOENT
    PerlSock_endprotoent();
    EXTEND(SP,1);
    RETPUSHYES;
#else
    DIE(no_sock_func, "endprotoent");
#endif
}

PP(pp_eservent)
{
    djSP;
#ifdef HAS_ENDSERVENT
    PerlSock_endservent();
    EXTEND(SP,1);
    RETPUSHYES;
#else
    DIE(no_sock_func, "endservent");
#endif
}

PP(pp_gpwnam)
{
#ifdef HAS_PASSWD
    return pp_gpwent(ARGS);
#else
    DIE(no_func, "getpwnam");
#endif
}

PP(pp_gpwuid)
{
#ifdef HAS_PASSWD
    return pp_gpwent(ARGS);
#else
    DIE(no_func, "getpwuid");
#endif
}

PP(pp_gpwent)
{
    djSP;
#if defined(HAS_PASSWD) && defined(HAS_GETPWENT)
    I32 which = PL_op->op_type;
    register SV *sv;
    struct passwd *pwent;
    STRLEN n_a;

    if (which == OP_GPWNAM)
	pwent = getpwnam(POPpx);
    else if (which == OP_GPWUID)
	pwent = getpwuid(POPi);
    else
	pwent = (struct passwd *)getpwent();

    EXTEND(SP, 10);
    if (GIMME != G_ARRAY) {
	PUSHs(sv = sv_newmortal());
	if (pwent) {
	    if (which == OP_GPWNAM)
		sv_setiv(sv, (IV)pwent->pw_uid);
	    else
		sv_setpv(sv, pwent->pw_name);
	}
	RETURN;
    }

    if (pwent) {
	PUSHs(sv = sv_mortalcopy(&PL_sv_no));
	sv_setpv(sv, pwent->pw_name);

	PUSHs(sv = sv_mortalcopy(&PL_sv_no));
#ifdef PWPASSWD
	sv_setpv(sv, pwent->pw_passwd);
#endif

	PUSHs(sv = sv_mortalcopy(&PL_sv_no));
	sv_setiv(sv, (IV)pwent->pw_uid);

	PUSHs(sv = sv_mortalcopy(&PL_sv_no));
	sv_setiv(sv, (IV)pwent->pw_gid);

	/* pw_change, pw_quota, and pw_age are mutually exclusive. */
	PUSHs(sv = sv_mortalcopy(&PL_sv_no));
#ifdef PWCHANGE
	sv_setiv(sv, (IV)pwent->pw_change);
#else
#   ifdef PWQUOTA
	sv_setiv(sv, (IV)pwent->pw_quota);
#   else
#       ifdef PWAGE
	sv_setpv(sv, pwent->pw_age);
#       endif
#   endif
#endif

	/* pw_class and pw_comment are mutually exclusive. */
	PUSHs(sv = sv_mortalcopy(&PL_sv_no));
#ifdef PWCLASS
	sv_setpv(sv, pwent->pw_class);
#else
#   ifdef PWCOMMENT
	sv_setpv(sv, pwent->pw_comment);
#   endif
#endif

	PUSHs(sv = sv_mortalcopy(&PL_sv_no));
#ifdef PWGECOS
	sv_setpv(sv, pwent->pw_gecos);
#endif
#ifndef INCOMPLETE_TAINTS
	/* pw_gecos is tainted because user himself can diddle with it. */
	SvTAINTED_on(sv);
#endif

	PUSHs(sv = sv_mortalcopy(&PL_sv_no));
	sv_setpv(sv, pwent->pw_dir);

	PUSHs(sv = sv_mortalcopy(&PL_sv_no));
	sv_setpv(sv, pwent->pw_shell);

#ifdef PWEXPIRE
	PUSHs(sv = sv_mortalcopy(&PL_sv_no));
	sv_setiv(sv, (IV)pwent->pw_expire);
#endif
    }
    RETURN;
#else
    DIE(no_func, "getpwent");
#endif
}

PP(pp_spwent)
{
    djSP;
#if defined(HAS_PASSWD) && defined(HAS_SETPWENT) && !defined(CYGWIN32)
    setpwent();
    RETPUSHYES;
#else
    DIE(no_func, "setpwent");
#endif
}

PP(pp_epwent)
{
    djSP;
#if defined(HAS_PASSWD) && defined(HAS_ENDPWENT)
    endpwent();
    RETPUSHYES;
#else
    DIE(no_func, "endpwent");
#endif
}

PP(pp_ggrnam)
{
#ifdef HAS_GROUP
    return pp_ggrent(ARGS);
#else
    DIE(no_func, "getgrnam");
#endif
}

PP(pp_ggrgid)
{
#ifdef HAS_GROUP
    return pp_ggrent(ARGS);
#else
    DIE(no_func, "getgrgid");
#endif
}

PP(pp_ggrent)
{
    djSP;
#if defined(HAS_GROUP) && defined(HAS_GETGRENT)
    I32 which = PL_op->op_type;
    register char **elem;
    register SV *sv;
    struct group *grent;
    STRLEN n_a;

    if (which == OP_GGRNAM)
	grent = (struct group *)getgrnam(POPpx);
    else if (which == OP_GGRGID)
	grent = (struct group *)getgrgid(POPi);
    else
	grent = (struct group *)getgrent();

    EXTEND(SP, 4);
    if (GIMME != G_ARRAY) {
	PUSHs(sv = sv_newmortal());
	if (grent) {
	    if (which == OP_GGRNAM)
		sv_setiv(sv, (IV)grent->gr_gid);
	    else
		sv_setpv(sv, grent->gr_name);
	}
	RETURN;
    }

    if (grent) {
	PUSHs(sv = sv_mortalcopy(&PL_sv_no));
	sv_setpv(sv, grent->gr_name);

	PUSHs(sv = sv_mortalcopy(&PL_sv_no));
#ifdef GRPASSWD
	sv_setpv(sv, grent->gr_passwd);
#endif

	PUSHs(sv = sv_mortalcopy(&PL_sv_no));
	sv_setiv(sv, (IV)grent->gr_gid);

	PUSHs(sv = sv_mortalcopy(&PL_sv_no));
	for (elem = grent->gr_mem; elem && *elem; elem++) {
	    sv_catpv(sv, *elem);
	    if (elem[1])
		sv_catpvn(sv, " ", 1);
	}
    }

    RETURN;
#else
    DIE(no_func, "getgrent");
#endif
}

PP(pp_sgrent)
{
    djSP;
#if defined(HAS_GROUP) && defined(HAS_SETGRENT)
    setgrent();
    RETPUSHYES;
#else
    DIE(no_func, "setgrent");
#endif
}

PP(pp_egrent)
{
    djSP;
#if defined(HAS_GROUP) && defined(HAS_ENDGRENT)
    endgrent();
    RETPUSHYES;
#else
    DIE(no_func, "endgrent");
#endif
}

PP(pp_getlogin)
{
    djSP; dTARGET;
#ifdef HAS_GETLOGIN
    char *tmps;
    EXTEND(SP, 1);
    if (!(tmps = PerlProc_getlogin()))
	RETPUSHUNDEF;
    PUSHp(tmps, strlen(tmps));
    RETURN;
#else
    DIE(no_func, "getlogin");
#endif
}

/* Miscellaneous. */

PP(pp_syscall)
{
#ifdef HAS_SYSCALL
    djSP; dMARK; dORIGMARK; dTARGET;
    register I32 items = SP - MARK;
    unsigned long a[20];
    register I32 i = 0;
    I32 retval = -1;
    MAGIC *mg;

    if (PL_tainting) {
	while (++MARK <= SP) {
	    if (SvTAINTED(*MARK)) {
		TAINT;
		break;
	    }
	}
	MARK = ORIGMARK;
	TAINT_PROPER("syscall");
    }

    /* This probably won't work on machines where sizeof(long) != sizeof(int)
     * or where sizeof(long) != sizeof(char*).  But such machines will
     * not likely have syscall implemented either, so who cares?
     */
    while (++MARK <= SP) {
	if (SvNIOK(*MARK) || !i)
	    a[i++] = SvIV(*MARK);
	else if (*MARK == &PL_sv_undef)
	    a[i++] = 0;
	else  {
	    STRLEN n_a;
	    a[i++] = (unsigned long)SvPV_force(*MARK, n_a);
	}
	if (i > 15)
	    break;
    }
    switch (items) {
    default:
	DIE("Too many args to syscall");
    case 0:
	DIE("Too few args to syscall");
    case 1:
	retval = syscall(a[0]);
	break;
    case 2:
	retval = syscall(a[0],a[1]);
	break;
    case 3:
	retval = syscall(a[0],a[1],a[2]);
	break;
    case 4:
	retval = syscall(a[0],a[1],a[2],a[3]);
	break;
    case 5:
	retval = syscall(a[0],a[1],a[2],a[3],a[4]);
	break;
    case 6:
	retval = syscall(a[0],a[1],a[2],a[3],a[4],a[5]);
	break;
    case 7:
	retval = syscall(a[0],a[1],a[2],a[3],a[4],a[5],a[6]);
	break;
    case 8:
	retval = syscall(a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7]);
	break;
#ifdef atarist
    case 9:
	retval = syscall(a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7],a[8]);
	break;
    case 10:
	retval = syscall(a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7],a[8],a[9]);
	break;
    case 11:
	retval = syscall(a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7],a[8],a[9],
	  a[10]);
	break;
    case 12:
	retval = syscall(a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7],a[8],a[9],
	  a[10],a[11]);
	break;
    case 13:
	retval = syscall(a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7],a[8],a[9],
	  a[10],a[11],a[12]);
	break;
    case 14:
	retval = syscall(a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7],a[8],a[9],
	  a[10],a[11],a[12],a[13]);
	break;
#endif /* atarist */
    }
    SP = ORIGMARK;
    PUSHi(retval);
    RETURN;
#else
    DIE(no_func, "syscall");
#endif
}

#ifdef FCNTL_EMULATE_FLOCK
 
/*  XXX Emulate flock() with fcntl().
    What's really needed is a good file locking module.
*/

static int
fcntl_emulate_flock(int fd, int operation)
{
    struct flock flock;
 
    switch (operation & ~LOCK_NB) {
    case LOCK_SH:
	flock.l_type = F_RDLCK;
	break;
    case LOCK_EX:
	flock.l_type = F_WRLCK;
	break;
    case LOCK_UN:
	flock.l_type = F_UNLCK;
	break;
    default:
	errno = EINVAL;
	return -1;
    }
    flock.l_whence = SEEK_SET;
    flock.l_start = flock.l_len = 0L;
 
    return fcntl(fd, (operation & LOCK_NB) ? F_SETLK : F_SETLKW, &flock);
}

#endif /* FCNTL_EMULATE_FLOCK */

#ifdef LOCKF_EMULATE_FLOCK

/*  XXX Emulate flock() with lockf().  This is just to increase
    portability of scripts.  The calls are not completely
    interchangeable.  What's really needed is a good file
    locking module.
*/

/*  The lockf() constants might have been defined in <unistd.h>.
    Unfortunately, <unistd.h> causes troubles on some mixed
    (BSD/POSIX) systems, such as SunOS 4.1.3.

   Further, the lockf() constants aren't POSIX, so they might not be
   visible if we're compiling with _POSIX_SOURCE defined.  Thus, we'll
   just stick in the SVID values and be done with it.  Sigh.
*/

# ifndef F_ULOCK
#  define F_ULOCK	0	/* Unlock a previously locked region */
# endif
# ifndef F_LOCK
#  define F_LOCK	1	/* Lock a region for exclusive use */
# endif
# ifndef F_TLOCK
#  define F_TLOCK	2	/* Test and lock a region for exclusive use */
# endif
# ifndef F_TEST
#  define F_TEST	3	/* Test a region for other processes locks */
# endif

static int
lockf_emulate_flock (fd, operation)
int fd;
int operation;
{
    int i;
    int save_errno;
    Off_t pos;

    /* flock locks entire file so for lockf we need to do the same	*/
    save_errno = errno;
    pos = PerlLIO_lseek(fd, (Off_t)0, SEEK_CUR);    /* get pos to restore later */
    if (pos > 0)	/* is seekable and needs to be repositioned	*/
	if (PerlLIO_lseek(fd, (Off_t)0, SEEK_SET) < 0)
	    pos = -1;	/* seek failed, so don't seek back afterwards	*/
    errno = save_errno;

    switch (operation) {

	/* LOCK_SH - get a shared lock */
	case LOCK_SH:
	/* LOCK_EX - get an exclusive lock */
	case LOCK_EX:
	    i = lockf (fd, F_LOCK, 0);
	    break;

	/* LOCK_SH|LOCK_NB - get a non-blocking shared lock */
	case LOCK_SH|LOCK_NB:
	/* LOCK_EX|LOCK_NB - get a non-blocking exclusive lock */
	case LOCK_EX|LOCK_NB:
	    i = lockf (fd, F_TLOCK, 0);
	    if (i == -1)
		if ((errno == EAGAIN) || (errno == EACCES))
		    errno = EWOULDBLOCK;
	    break;

	/* LOCK_UN - unlock (non-blocking is a no-op) */
	case LOCK_UN:
	case LOCK_UN|LOCK_NB:
	    i = lockf (fd, F_ULOCK, 0);
	    break;

	/* Default - can't decipher operation */
	default:
	    i = -1;
	    errno = EINVAL;
	    break;
    }

    if (pos > 0)      /* need to restore position of the handle	*/
	PerlLIO_lseek(fd, pos, SEEK_SET);	/* ignore error here	*/

    return (i);
}

#endif /* LOCKF_EMULATE_FLOCK */
