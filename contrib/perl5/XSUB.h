#define ST(off) PL_stack_base[ax + (off)]

#ifdef CAN_PROTOTYPE
#ifdef PERL_OBJECT
#define XS(name) void name(CV* cv, CPerlObj* pPerl)
#else
#define XS(name) void name(CV* cv)
#endif
#else
#define XS(name) void name(cv) CV* cv;
#endif

#define dXSARGS				\
	dSP; dMARK;			\
	I32 ax = mark - PL_stack_base + 1;	\
	I32 items = sp - mark

#define XSANY CvXSUBANY(cv)

#define dXSI32 I32 ix = XSANY.any_i32

#ifdef __cplusplus
#  define XSINTERFACE_CVT(ret,name) ret (*name)(...)
#else
#  define XSINTERFACE_CVT(ret,name) ret (*name)()
#endif
#define dXSFUNCTION(ret)		XSINTERFACE_CVT(ret,XSFUNCTION)
#define XSINTERFACE_FUNC(ret,cv,f)	((XSINTERFACE_CVT(ret,))(f))
#define XSINTERFACE_FUNC_SET(cv,f)	\
		CvXSUBANY(cv).any_dptr = (void (*) _((void*)))(f)

#define XSRETURN(off)					\
    STMT_START {					\
	PL_stack_sp = PL_stack_base + ax + ((off) - 1);	\
	return;						\
    } STMT_END

/* Simple macros to put new mortal values onto the stack.   */
/* Typically used to return values from XS functions.       */
#define XST_mIV(i,v)  (ST(i) = sv_2mortal(newSViv(v))  )
#define XST_mNV(i,v)  (ST(i) = sv_2mortal(newSVnv(v))  )
#define XST_mPV(i,v)  (ST(i) = sv_2mortal(newSVpv(v,0)))
#define XST_mNO(i)    (ST(i) = &PL_sv_no   )
#define XST_mYES(i)   (ST(i) = &PL_sv_yes  )
#define XST_mUNDEF(i) (ST(i) = &PL_sv_undef)
 
#define XSRETURN_IV(v) STMT_START { XST_mIV(0,v);  XSRETURN(1); } STMT_END
#define XSRETURN_NV(v) STMT_START { XST_mNV(0,v);  XSRETURN(1); } STMT_END
#define XSRETURN_PV(v) STMT_START { XST_mPV(0,v);  XSRETURN(1); } STMT_END
#define XSRETURN_NO    STMT_START { XST_mNO(0);    XSRETURN(1); } STMT_END
#define XSRETURN_YES   STMT_START { XST_mYES(0);   XSRETURN(1); } STMT_END
#define XSRETURN_UNDEF STMT_START { XST_mUNDEF(0); XSRETURN(1); } STMT_END
#define XSRETURN_EMPTY STMT_START {                XSRETURN(0); } STMT_END

#define newXSproto(a,b,c,d)	sv_setpv((SV*)newXS(a,b,c), d)

#ifdef XS_VERSION
# define XS_VERSION_BOOTCHECK \
    STMT_START {							\
	SV *tmpsv; STRLEN n_a;						\
	char *vn = Nullch, *module = SvPV(ST(0),n_a);			\
	if (items >= 2)	 /* version supplied as bootstrap arg */	\
	    tmpsv = ST(1);						\
	else {								\
	    /* XXX GV_ADDWARN */					\
	    tmpsv = perl_get_sv(form("%s::%s", module,			\
				  vn = "XS_VERSION"), FALSE);		\
	    if (!tmpsv || !SvOK(tmpsv))					\
		tmpsv = perl_get_sv(form("%s::%s", module,		\
				      vn = "VERSION"), FALSE);		\
	}								\
	if (tmpsv && (!SvOK(tmpsv) || strNE(XS_VERSION, SvPV(tmpsv, n_a))))	\
	    croak("%s object version %s does not match %s%s%s%s %_",	\
		  module, XS_VERSION,					\
		  vn ? "$" : "", vn ? module : "", vn ? "::" : "",	\
		  vn ? vn : "bootstrap parameter", tmpsv);		\
    } STMT_END
#else
# define XS_VERSION_BOOTCHECK
#endif

#ifdef PERL_CAPI
#  define VTBL_sv		get_vtbl(want_vtbl_sv)
#  define VTBL_env		get_vtbl(want_vtbl_env)
#  define VTBL_envelem		get_vtbl(want_vtbl_envelem)
#  define VTBL_sig		get_vtbl(want_vtbl_sig)
#  define VTBL_sigelem		get_vtbl(want_vtbl_sigelem)
#  define VTBL_pack		get_vtbl(want_vtbl_pack)
#  define VTBL_packelem		get_vtbl(want_vtbl_packelem)
#  define VTBL_dbline		get_vtbl(want_vtbl_dbline)
#  define VTBL_isa		get_vtbl(want_vtbl_isa)
#  define VTBL_isaelem		get_vtbl(want_vtbl_isaelem)
#  define VTBL_arylen		get_vtbl(want_vtbl_arylen)
#  define VTBL_glob		get_vtbl(want_vtbl_glob)
#  define VTBL_mglob		get_vtbl(want_vtbl_mglob)
#  define VTBL_nkeys		get_vtbl(want_vtbl_nkeys)
#  define VTBL_taint		get_vtbl(want_vtbl_taint)
#  define VTBL_substr		get_vtbl(want_vtbl_substr)
#  define VTBL_vec		get_vtbl(want_vtbl_vec)
#  define VTBL_pos		get_vtbl(want_vtbl_pos)
#  define VTBL_bm		get_vtbl(want_vtbl_bm)
#  define VTBL_fm		get_vtbl(want_vtbl_fm)
#  define VTBL_uvar		get_vtbl(want_vtbl_uvar)
#  define VTBL_defelem		get_vtbl(want_vtbl_defelem)
#  define VTBL_regexp		get_vtbl(want_vtbl_regexp)
#  ifdef USE_LOCALE_COLLATE
#    define VTBL_collxfrm	get_vtbl(want_vtbl_collxfrm)
#  endif
#  ifdef OVERLOAD
#    define VTBL_amagic		get_vtbl(want_vtbl_amagic)
#    define VTBL_amagicelem	get_vtbl(want_vtbl_amagicelem)
#  endif
#else
#  define VTBL_sv		&vtbl_sv
#  define VTBL_env		&vtbl_env
#  define VTBL_envelem		&vtbl_envelem
#  define VTBL_sig		&vtbl_sig
#  define VTBL_sigelem		&vtbl_sigelem
#  define VTBL_pack		&vtbl_pack
#  define VTBL_packelem		&vtbl_packelem
#  define VTBL_dbline		&vtbl_dbline
#  define VTBL_isa		&vtbl_isa
#  define VTBL_isaelem		&vtbl_isaelem
#  define VTBL_arylen		&vtbl_arylen
#  define VTBL_glob		&vtbl_glob
#  define VTBL_mglob		&vtbl_mglob
#  define VTBL_nkeys		&vtbl_nkeys
#  define VTBL_taint		&vtbl_taint
#  define VTBL_substr		&vtbl_substr
#  define VTBL_vec		&vtbl_vec
#  define VTBL_pos		&vtbl_pos
#  define VTBL_bm		&vtbl_bm
#  define VTBL_fm		&vtbl_fm
#  define VTBL_uvar		&vtbl_uvar
#  define VTBL_defelem		&vtbl_defelem
#  define VTBL_regexp		&vtbl_regexp
#  ifdef USE_LOCALE_COLLATE
#    define VTBL_collxfrm	&vtbl_collxfrm
#  endif
#  ifdef OVERLOAD
#    define VTBL_amagic		&vtbl_amagic
#    define VTBL_amagicelem	&vtbl_amagicelem
#  endif
#endif

#ifdef PERL_OBJECT
#include "objXSUB.h"
#ifndef NO_XSLOCKS
#ifdef WIN32
#include "XSlock.h"
#endif  /* WIN32 */
#endif  /* NO_XSLOCKS */
#else
#ifdef PERL_CAPI
#include "perlCAPI.h"
#endif
#endif	/* PERL_OBJECT */
