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
	SV *tmpsv;							\
	char *vn = Nullch, *module = SvPV(ST(0),PL_na);			\
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
	if (tmpsv && (!SvOK(tmpsv) || strNE(XS_VERSION, SvPV(tmpsv, PL_na))))	\
	    croak("%s object version %s does not match %s%s%s%s %_",	\
		  module, XS_VERSION,					\
		  vn ? "$" : "", vn ? module : "", vn ? "::" : "",	\
		  vn ? vn : "bootstrap parameter", tmpsv);		\
    } STMT_END
#else
# define XS_VERSION_BOOTCHECK
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
