#ifndef _INC_PERL_XSUB_H
#define _INC_PERL_XSUB_H 1

/* first, some documentation for xsubpp-generated items */

/*
=for apidoc Amn|char*|CLASS
Variable which is setup by C<xsubpp> to indicate the 
class name for a C++ XS constructor.  This is always a C<char*>.  See C<THIS>.

=for apidoc Amn|(whatever)|RETVAL
Variable which is setup by C<xsubpp> to hold the return value for an 
XSUB. This is always the proper type for the XSUB. See 
L<perlxs/"The RETVAL Variable">.

=for apidoc Amn|(whatever)|THIS
Variable which is setup by C<xsubpp> to designate the object in a C++ 
XSUB.  This is always the proper type for the C++ object.  See C<CLASS> and 
L<perlxs/"Using XS With C++">.

=for apidoc Amn|I32|items
Variable which is setup by C<xsubpp> to indicate the number of 
items on the stack.  See L<perlxs/"Variable-length Parameter Lists">.

=for apidoc Amn|I32|ix
Variable which is setup by C<xsubpp> to indicate which of an 
XSUB's aliases was used to invoke it.  See L<perlxs/"The ALIAS: Keyword">.

=for apidoc Am|SV*|ST|int ix
Used to access elements on the XSUB's stack.

=for apidoc AmU||XS
Macro to declare an XSUB and its C parameter list.  This is handled by
C<xsubpp>.

=for apidoc Ams||dXSARGS
Sets up stack and mark pointers for an XSUB, calling dSP and dMARK.  This
is usually handled automatically by C<xsubpp>.  Declares the C<items>
variable to indicate the number of items on the stack.

=for apidoc Ams||dXSI32
Sets up the C<ix> variable for an XSUB which has aliases.  This is usually
handled automatically by C<xsubpp>.

=cut
*/

#define ST(off) PL_stack_base[ax + (off)]

#if defined(__CYGWIN__) && defined(USE_DYNAMIC_LOADING)
#  define XS(name) __declspec(dllexport) void name(pTHXo_ CV* cv)
#else
#  define XS(name) void name(pTHXo_ CV* cv)
#endif

#define dXSARGS				\
	dSP; dMARK;			\
	I32 ax = mark - PL_stack_base + 1;	\
	I32 items = sp - mark

#define dXSTARG SV * targ = ((PL_op->op_private & OPpENTERSUB_HASTARG) \
			     ? PAD_SV(PL_op->op_targ) : sv_newmortal())

/* Should be used before final PUSHi etc. if not in PPCODE section. */
#define XSprePUSH (sp = PL_stack_base + ax - 1)

#define XSANY CvXSUBANY(cv)

#define dXSI32 I32 ix = XSANY.any_i32

#ifdef __cplusplus
#  define XSINTERFACE_CVT(ret,name) ret (*name)(...)
#else
#  define XSINTERFACE_CVT(ret,name) ret (*name)()
#endif
#define dXSFUNCTION(ret)		XSINTERFACE_CVT(ret,XSFUNCTION)
#define XSINTERFACE_FUNC(ret,cv,f)	((XSINTERFACE_CVT(ret,cv))(f))
#define XSINTERFACE_FUNC_SET(cv,f)	\
		CvXSUBANY(cv).any_dptr = (void (*) (pTHXo_ void*))(f)

/* Simple macros to put new mortal values onto the stack.   */
/* Typically used to return values from XS functions.       */

/*
=for apidoc Am|void|XST_mIV|int pos|IV iv
Place an integer into the specified position C<pos> on the stack.  The
value is stored in a new mortal SV.

=for apidoc Am|void|XST_mNV|int pos|NV nv
Place a double into the specified position C<pos> on the stack.  The value
is stored in a new mortal SV.

=for apidoc Am|void|XST_mPV|int pos|char* str
Place a copy of a string into the specified position C<pos> on the stack. 
The value is stored in a new mortal SV.

=for apidoc Am|void|XST_mNO|int pos
Place C<&PL_sv_no> into the specified position C<pos> on the
stack.

=for apidoc Am|void|XST_mYES|int pos
Place C<&PL_sv_yes> into the specified position C<pos> on the
stack.

=for apidoc Am|void|XST_mUNDEF|int pos
Place C<&PL_sv_undef> into the specified position C<pos> on the
stack.

=for apidoc Am|void|XSRETURN|int nitems
Return from XSUB, indicating number of items on the stack.  This is usually
handled by C<xsubpp>.

=for apidoc Am|void|XSRETURN_IV|IV iv
Return an integer from an XSUB immediately.  Uses C<XST_mIV>.

=for apidoc Am|void|XSRETURN_NV|NV nv
Return an double from an XSUB immediately.  Uses C<XST_mNV>.

=for apidoc Am|void|XSRETURN_PV|char* str
Return a copy of a string from an XSUB immediately.  Uses C<XST_mPV>.

=for apidoc Ams||XSRETURN_NO
Return C<&PL_sv_no> from an XSUB immediately.  Uses C<XST_mNO>.

=for apidoc Ams||XSRETURN_YES
Return C<&PL_sv_yes> from an XSUB immediately.  Uses C<XST_mYES>.

=for apidoc Ams||XSRETURN_UNDEF
Return C<&PL_sv_undef> from an XSUB immediately.  Uses C<XST_mUNDEF>.

=for apidoc Ams||XSRETURN_EMPTY
Return an empty list from an XSUB immediately.

=for apidoc AmU||newXSproto
Used by C<xsubpp> to hook up XSUBs as Perl subs.  Adds Perl prototypes to
the subs.

=for apidoc AmU||XS_VERSION
The version identifier for an XS module.  This is usually
handled automatically by C<ExtUtils::MakeMaker>.  See C<XS_VERSION_BOOTCHECK>.

=for apidoc Ams||XS_VERSION_BOOTCHECK
Macro to verify that a PM module's $VERSION variable matches the XS
module's C<XS_VERSION> variable.  This is usually handled automatically by
C<xsubpp>.  See L<perlxs/"The VERSIONCHECK: Keyword">.

=cut
*/

#define XST_mIV(i,v)  (ST(i) = sv_2mortal(newSViv(v))  )
#define XST_mNV(i,v)  (ST(i) = sv_2mortal(newSVnv(v))  )
#define XST_mPV(i,v)  (ST(i) = sv_2mortal(newSVpv(v,0)))
#define XST_mPVN(i,v,n)  (ST(i) = sv_2mortal(newSVpvn(v,n)))
#define XST_mNO(i)    (ST(i) = &PL_sv_no   )
#define XST_mYES(i)   (ST(i) = &PL_sv_yes  )
#define XST_mUNDEF(i) (ST(i) = &PL_sv_undef)

#define XSRETURN(off)					\
    STMT_START {					\
	PL_stack_sp = PL_stack_base + ax + ((off) - 1);	\
	return;						\
    } STMT_END

#define XSRETURN_IV(v) STMT_START { XST_mIV(0,v);  XSRETURN(1); } STMT_END
#define XSRETURN_NV(v) STMT_START { XST_mNV(0,v);  XSRETURN(1); } STMT_END
#define XSRETURN_PV(v) STMT_START { XST_mPV(0,v);  XSRETURN(1); } STMT_END
#define XSRETURN_PVN(v,n) STMT_START { XST_mPVN(0,v,n);  XSRETURN(1); } STMT_END
#define XSRETURN_NO    STMT_START { XST_mNO(0);    XSRETURN(1); } STMT_END
#define XSRETURN_YES   STMT_START { XST_mYES(0);   XSRETURN(1); } STMT_END
#define XSRETURN_UNDEF STMT_START { XST_mUNDEF(0); XSRETURN(1); } STMT_END
#define XSRETURN_EMPTY STMT_START {                XSRETURN(0); } STMT_END

#define newXSproto(a,b,c,d)	sv_setpv((SV*)newXS(a,b,c), d)

#ifdef XS_VERSION
#  define XS_VERSION_BOOTCHECK \
    STMT_START {							\
	SV *tmpsv; STRLEN n_a;						\
	char *vn = Nullch, *module = SvPV(ST(0),n_a);			\
	if (items >= 2)	 /* version supplied as bootstrap arg */	\
	    tmpsv = ST(1);						\
	else {								\
	    /* XXX GV_ADDWARN */					\
	    tmpsv = get_sv(Perl_form(aTHX_ "%s::%s", module,		\
				vn = "XS_VERSION"), FALSE);		\
	    if (!tmpsv || !SvOK(tmpsv))					\
		tmpsv = get_sv(Perl_form(aTHX_ "%s::%s", module,	\
				    vn = "VERSION"), FALSE);		\
	}								\
	if (tmpsv && (!SvOK(tmpsv) || strNE(XS_VERSION, SvPV(tmpsv, n_a))))	\
	    Perl_croak(aTHX_ "%s object version %s does not match %s%s%s%s %"SVf,\
		  module, XS_VERSION,					\
		  vn ? "$" : "", vn ? module : "", vn ? "::" : "",	\
		  vn ? vn : "bootstrap parameter", tmpsv);		\
    } STMT_END
#else
#  define XS_VERSION_BOOTCHECK
#endif

#if 1		/* for compatibility */
#  define VTBL_sv		&PL_vtbl_sv
#  define VTBL_env		&PL_vtbl_env
#  define VTBL_envelem		&PL_vtbl_envelem
#  define VTBL_sig		&PL_vtbl_sig
#  define VTBL_sigelem		&PL_vtbl_sigelem
#  define VTBL_pack		&PL_vtbl_pack
#  define VTBL_packelem		&PL_vtbl_packelem
#  define VTBL_dbline		&PL_vtbl_dbline
#  define VTBL_isa		&PL_vtbl_isa
#  define VTBL_isaelem		&PL_vtbl_isaelem
#  define VTBL_arylen		&PL_vtbl_arylen
#  define VTBL_glob		&PL_vtbl_glob
#  define VTBL_mglob		&PL_vtbl_mglob
#  define VTBL_nkeys		&PL_vtbl_nkeys
#  define VTBL_taint		&PL_vtbl_taint
#  define VTBL_substr		&PL_vtbl_substr
#  define VTBL_vec		&PL_vtbl_vec
#  define VTBL_pos		&PL_vtbl_pos
#  define VTBL_bm		&PL_vtbl_bm
#  define VTBL_fm		&PL_vtbl_fm
#  define VTBL_uvar		&PL_vtbl_uvar
#  define VTBL_defelem		&PL_vtbl_defelem
#  define VTBL_regexp		&PL_vtbl_regexp
#  define VTBL_regdata		&PL_vtbl_regdata
#  define VTBL_regdatum		&PL_vtbl_regdatum
#  ifdef USE_LOCALE_COLLATE
#    define VTBL_collxfrm	&PL_vtbl_collxfrm
#  endif
#  define VTBL_amagic		&PL_vtbl_amagic
#  define VTBL_amagicelem	&PL_vtbl_amagicelem
#endif

#include "perlapi.h"
#include "objXSUB.h"

#if defined(PERL_IMPLICIT_CONTEXT) && !defined(PERL_NO_GET_CONTEXT) && !defined(PERL_CORE)
#  undef aTHX
#  undef aTHX_
#  define aTHX		PERL_GET_THX
#  define aTHX_		aTHX,
#endif

#if (defined(PERL_CAPI) || defined(PERL_IMPLICIT_SYS)) && !defined(PERL_CORE)
#  ifndef NO_XSLOCKS
#    undef closedir
#    undef opendir
#    undef stdin
#    undef stdout
#    undef stderr
#    undef feof
#    undef ferror
#    undef fgetpos
#    undef ioctl
#    undef getlogin
#    undef setjmp
#    undef getc
#    undef ungetc
#    undef fileno

#    define mkdir		PerlDir_mkdir
#    define chdir		PerlDir_chdir
#    define rmdir		PerlDir_rmdir
#    define closedir		PerlDir_close
#    define opendir		PerlDir_open
#    define readdir		PerlDir_read
#    define rewinddir		PerlDir_rewind
#    define seekdir		PerlDir_seek
#    define telldir		PerlDir_tell
#    define putenv		PerlEnv_putenv
#    define getenv		PerlEnv_getenv
#    define uname		PerlEnv_uname
#    define stdin		PerlIO_stdin()
#    define stdout		PerlIO_stdout()
#    define stderr		PerlIO_stderr()
#    define fopen		PerlIO_open
#    define fclose		PerlIO_close
#    define feof		PerlIO_eof
#    define ferror		PerlIO_error
#    define fclearerr		PerlIO_clearerr
#    define getc		PerlIO_getc
#    define fputc(c, f)		PerlIO_putc(f,c)
#    define fputs(s, f)		PerlIO_puts(f,s)
#    define fflush		PerlIO_flush
#    define ungetc(c, f)	PerlIO_ungetc((f),(c))
#    define fileno		PerlIO_fileno
#    define fdopen		PerlIO_fdopen
#    define freopen		PerlIO_reopen
#    define fread(b,s,c,f)	PerlIO_read((f),(b),(s*c))
#    define fwrite(b,s,c,f)	PerlIO_write((f),(b),(s*c))
#    define setbuf		PerlIO_setbuf
#    define setvbuf		PerlIO_setvbuf
#    define setlinebuf		PerlIO_setlinebuf
#    define stdoutf		PerlIO_stdoutf
#    define vfprintf		PerlIO_vprintf
#    define ftell		PerlIO_tell
#    define fseek		PerlIO_seek
#    define fgetpos		PerlIO_getpos
#    define fsetpos		PerlIO_setpos
#    define frewind		PerlIO_rewind
#    define tmpfile		PerlIO_tmpfile
#    define access		PerlLIO_access
#    define chmod		PerlLIO_chmod
#    define chsize		PerlLIO_chsize
#    define close		PerlLIO_close
#    define dup			PerlLIO_dup
#    define dup2		PerlLIO_dup2
#    define flock		PerlLIO_flock
#    define fstat		PerlLIO_fstat
#    define ioctl		PerlLIO_ioctl
#    define isatty		PerlLIO_isatty
#    define link                PerlLIO_link
#    define lseek		PerlLIO_lseek
#    define lstat		PerlLIO_lstat
#    define mktemp		PerlLIO_mktemp
#    define open		PerlLIO_open
#    define read		PerlLIO_read
#    define rename		PerlLIO_rename
#    define setmode		PerlLIO_setmode
#    define stat(buf,sb)	PerlLIO_stat(buf,sb)
#    define tmpnam		PerlLIO_tmpnam
#    define umask		PerlLIO_umask
#    define unlink		PerlLIO_unlink
#    define utime		PerlLIO_utime
#    define write		PerlLIO_write
#    define malloc		PerlMem_malloc
#    define realloc		PerlMem_realloc
#    define free		PerlMem_free
#    define abort		PerlProc_abort
#    define exit		PerlProc_exit
#    define _exit		PerlProc__exit
#    define execl		PerlProc_execl
#    define execv		PerlProc_execv
#    define execvp		PerlProc_execvp
#    define getuid		PerlProc_getuid
#    define geteuid		PerlProc_geteuid
#    define getgid		PerlProc_getgid
#    define getegid		PerlProc_getegid
#    define getlogin		PerlProc_getlogin
#    define kill		PerlProc_kill
#    define killpg		PerlProc_killpg
#    define pause		PerlProc_pause
#    define popen		PerlProc_popen
#    define pclose		PerlProc_pclose
#    define pipe		PerlProc_pipe
#    define setuid		PerlProc_setuid
#    define setgid		PerlProc_setgid
#    define sleep		PerlProc_sleep
#    define times		PerlProc_times
#    define wait		PerlProc_wait
#    define setjmp		PerlProc_setjmp
#    define longjmp		PerlProc_longjmp
#    define signal		PerlProc_signal
#    define getpid		PerlProc_getpid
#    define htonl		PerlSock_htonl
#    define htons		PerlSock_htons
#    define ntohl		PerlSock_ntohl
#    define ntohs		PerlSock_ntohs
#    define accept		PerlSock_accept
#    define bind		PerlSock_bind
#    define connect		PerlSock_connect
#    define endhostent		PerlSock_endhostent
#    define endnetent		PerlSock_endnetent
#    define endprotoent		PerlSock_endprotoent
#    define endservent		PerlSock_endservent
#    define gethostbyaddr	PerlSock_gethostbyaddr
#    define gethostbyname	PerlSock_gethostbyname
#    define gethostent		PerlSock_gethostent
#    define gethostname		PerlSock_gethostname
#    define getnetbyaddr	PerlSock_getnetbyaddr
#    define getnetbyname	PerlSock_getnetbyname
#    define getnetent		PerlSock_getnetent
#    define getpeername		PerlSock_getpeername
#    define getprotobyname	PerlSock_getprotobyname
#    define getprotobynumber	PerlSock_getprotobynumber
#    define getprotoent		PerlSock_getprotoent
#    define getservbyname	PerlSock_getservbyname
#    define getservbyport	PerlSock_getservbyport
#    define getservent		PerlSock_getservent
#    define getsockname		PerlSock_getsockname
#    define getsockopt		PerlSock_getsockopt
#    define inet_addr		PerlSock_inet_addr
#    define inet_ntoa		PerlSock_inet_ntoa
#    define listen		PerlSock_listen
#    define recv		PerlSock_recv
#    define recvfrom		PerlSock_recvfrom
#    define select		PerlSock_select
#    define send		PerlSock_send
#    define sendto		PerlSock_sendto
#    define sethostent		PerlSock_sethostent
#    define setnetent		PerlSock_setnetent
#    define setprotoent		PerlSock_setprotoent
#    define setservent		PerlSock_setservent
#    define setsockopt		PerlSock_setsockopt
#    define shutdown		PerlSock_shutdown
#    define socket		PerlSock_socket
#    define socketpair		PerlSock_socketpair
#  endif  /* NO_XSLOCKS */
#endif  /* PERL_CAPI */

#endif /* _INC_PERL_XSUB_H */		/* include guard */
