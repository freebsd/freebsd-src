#define SAVEt_ITEM		0
#define SAVEt_SV		1
#define SAVEt_AV		2
#define SAVEt_HV		3
#define SAVEt_INT		4
#define SAVEt_LONG		5
#define SAVEt_I32		6
#define SAVEt_IV		7
#define SAVEt_SPTR		8
#define SAVEt_APTR		9
#define SAVEt_HPTR		10
#define SAVEt_PPTR		11
#define SAVEt_NSTAB		12
#define SAVEt_SVREF		13
#define SAVEt_GP		14
#define SAVEt_FREESV		15
#define SAVEt_FREEOP		16
#define SAVEt_FREEPV		17
#define SAVEt_CLEARSV		18
#define SAVEt_DELETE		19
#define SAVEt_DESTRUCTOR	20
#define SAVEt_REGCONTEXT	21
#define SAVEt_STACK_POS		22
#define SAVEt_I16		23
#define SAVEt_AELEM		24
#define SAVEt_HELEM		25
#define SAVEt_OP		26
#define SAVEt_HINTS		27
#define SAVEt_ALLOC		28
#define SAVEt_GENERIC_SVREF	29
#define SAVEt_DESTRUCTOR_X	30
#define SAVEt_VPTR		31
#define SAVEt_I8		32
#define SAVEt_COMPPAD		33
#define SAVEt_GENERIC_PVREF	34
#define SAVEt_PADSV		35
#define SAVEt_MORTALIZESV	36

#define SSCHECK(need) if (PL_savestack_ix + need > PL_savestack_max) savestack_grow()
#define SSPUSHINT(i) (PL_savestack[PL_savestack_ix++].any_i32 = (I32)(i))
#define SSPUSHLONG(i) (PL_savestack[PL_savestack_ix++].any_long = (long)(i))
#define SSPUSHIV(i) (PL_savestack[PL_savestack_ix++].any_iv = (IV)(i))
#define SSPUSHPTR(p) (PL_savestack[PL_savestack_ix++].any_ptr = (void*)(p))
#define SSPUSHDPTR(p) (PL_savestack[PL_savestack_ix++].any_dptr = (p))
#define SSPUSHDXPTR(p) (PL_savestack[PL_savestack_ix++].any_dxptr = (p))
#define SSPOPINT (PL_savestack[--PL_savestack_ix].any_i32)
#define SSPOPLONG (PL_savestack[--PL_savestack_ix].any_long)
#define SSPOPIV (PL_savestack[--PL_savestack_ix].any_iv)
#define SSPOPPTR (PL_savestack[--PL_savestack_ix].any_ptr)
#define SSPOPDPTR (PL_savestack[--PL_savestack_ix].any_dptr)
#define SSPOPDXPTR (PL_savestack[--PL_savestack_ix].any_dxptr)

/*
=for apidoc Ams||SAVETMPS
Opening bracket for temporaries on a callback.  See C<FREETMPS> and
L<perlcall>.

=for apidoc Ams||FREETMPS
Closing bracket for temporaries on a callback.  See C<SAVETMPS> and
L<perlcall>.

=for apidoc Ams||ENTER
Opening bracket on a callback.  See C<LEAVE> and L<perlcall>.

=for apidoc Ams||LEAVE
Closing bracket on a callback.  See C<ENTER> and L<perlcall>.

=cut
*/

#define SAVETMPS save_int((int*)&PL_tmps_floor), PL_tmps_floor = PL_tmps_ix
#define FREETMPS if (PL_tmps_ix > PL_tmps_floor) free_tmps()

#ifdef DEBUGGING
#define ENTER							\
    STMT_START {						\
	push_scope();						\
	DEBUG_l(WITH_THR(Perl_deb(aTHX_ "ENTER scope %ld at %s:%d\n",	\
		    PL_scopestack_ix, __FILE__, __LINE__)));	\
    } STMT_END
#define LEAVE							\
    STMT_START {						\
	DEBUG_l(WITH_THR(Perl_deb(aTHX_ "LEAVE scope %ld at %s:%d\n",	\
		    PL_scopestack_ix, __FILE__, __LINE__)));	\
	pop_scope();						\
    } STMT_END
#else
#define ENTER push_scope()
#define LEAVE pop_scope()
#endif
#define LEAVE_SCOPE(old) if (PL_savestack_ix > old) leave_scope(old)

/*
 * Not using SOFT_CAST on SAVESPTR, SAVEGENERICSV and SAVEFREESV
 * because these are used for several kinds of pointer values
 */
#define SAVEI8(i)	save_I8(SOFT_CAST(I8*)&(i))
#define SAVEI16(i)	save_I16(SOFT_CAST(I16*)&(i))
#define SAVEI32(i)	save_I32(SOFT_CAST(I32*)&(i))
#define SAVEINT(i)	save_int(SOFT_CAST(int*)&(i))
#define SAVEIV(i)	save_iv(SOFT_CAST(IV*)&(i))
#define SAVELONG(l)	save_long(SOFT_CAST(long*)&(l))
#define SAVESPTR(s)	save_sptr((SV**)&(s))
#define SAVEPPTR(s)	save_pptr(SOFT_CAST(char**)&(s))
#define SAVEVPTR(s)	save_vptr((void*)&(s))
#define SAVEPADSV(s)	save_padsv(s)
#define SAVEFREESV(s)	save_freesv((SV*)(s))
#define SAVEMORTALIZESV(s)	save_mortalizesv((SV*)(s))
#define SAVEFREEOP(o)	save_freeop(SOFT_CAST(OP*)(o))
#define SAVEFREEPV(p)	save_freepv(SOFT_CAST(char*)(p))
#define SAVECLEARSV(sv)	save_clearsv(SOFT_CAST(SV**)&(sv))
#define SAVEGENERICSV(s)	save_generic_svref((SV**)&(s))
#define SAVEGENERICPV(s)	save_generic_pvref((char**)&(s))
#define SAVEDELETE(h,k,l) \
	  save_delete(SOFT_CAST(HV*)(h), SOFT_CAST(char*)(k), (I32)(l))
#define SAVEDESTRUCTOR(f,p) \
	  save_destructor((DESTRUCTORFUNC_NOCONTEXT_t)(f), SOFT_CAST(void*)(p))

#define SAVEDESTRUCTOR_X(f,p) \
	  save_destructor_x((DESTRUCTORFUNC_t)(f), SOFT_CAST(void*)(p))

#define SAVESTACK_POS() \
    STMT_START {				\
	SSCHECK(2);				\
	SSPUSHINT(PL_stack_sp - PL_stack_base);	\
	SSPUSHINT(SAVEt_STACK_POS);		\
    } STMT_END

#define SAVEOP()	save_op()

#define SAVEHINTS() \
    STMT_START {				\
	if (PL_hints & HINT_LOCALIZE_HH)	\
	    save_hints();			\
	else {					\
	    SSCHECK(2);				\
	    SSPUSHINT(PL_hints);		\
	    SSPUSHINT(SAVEt_HINTS);		\
	}					\
    } STMT_END

#define SAVECOMPPAD() \
    STMT_START {						\
	if (PL_comppad && PL_curpad == AvARRAY(PL_comppad)) {	\
	    SSCHECK(2);						\
	    SSPUSHPTR((SV*)PL_comppad);				\
	    SSPUSHINT(SAVEt_COMPPAD);				\
	}							\
	else {							\
	    SAVEVPTR(PL_curpad);				\
	    SAVESPTR(PL_comppad);				\
	}							\
    } STMT_END

#ifdef USE_ITHREADS
#  define SAVECOPSTASH(c)	SAVEPPTR(CopSTASHPV(c))
#  define SAVECOPSTASH_FREE(c)	SAVEGENERICPV(CopSTASHPV(c))
#  define SAVECOPFILE(c)	SAVEPPTR(CopFILE(c))
#  define SAVECOPFILE_FREE(c)	SAVEGENERICPV(CopFILE(c))
#else
#  define SAVECOPSTASH(c)	SAVESPTR(CopSTASH(c))
#  define SAVECOPSTASH_FREE(c)	SAVECOPSTASH(c)	/* XXX not refcounted */
#  define SAVECOPFILE(c)	SAVESPTR(CopFILEGV(c))
#  define SAVECOPFILE_FREE(c)	SAVEGENERICSV(CopFILEGV(c))
#endif

#define SAVECOPLINE(c)		SAVEI16(CopLINE(c))

/* SSNEW() temporarily allocates a specified number of bytes of data on the
 * savestack.  It returns an integer index into the savestack, because a
 * pointer would get broken if the savestack is moved on reallocation.
 * SSNEWa() works like SSNEW(), but also aligns the data to the specified
 * number of bytes.  MEM_ALIGNBYTES is perhaps the most useful.  The
 * alignment will be preserved therough savestack reallocation *only* if
 * realloc returns data aligned to a size divisible by `align'!
 *
 * SSPTR() converts the index returned by SSNEW/SSNEWa() into a pointer.
 */

#define SSNEW(size)             Perl_save_alloc(aTHX_ (size), 0)
#define SSNEWt(n,t)             SSNEW((n)*sizeof(t))
#define SSNEWa(size,align)	Perl_save_alloc(aTHX_ (size), \
    (align - ((int)((caddr_t)&PL_savestack[PL_savestack_ix]) % align)) % align)
#define SSNEWat(n,t,align)	SSNEWa((n)*sizeof(t), align)

#define SSPTR(off,type)         ((type)  ((char*)PL_savestack + off))
#define SSPTRt(off,type)        ((type*) ((char*)PL_savestack + off))

/* A jmpenv packages the state required to perform a proper non-local jump.
 * Note that there is a start_env initialized when perl starts, and top_env
 * points to this initially, so top_env should always be non-null.
 *
 * Existence of a non-null top_env->je_prev implies it is valid to call
 * longjmp() at that runlevel (we make sure start_env.je_prev is always
 * null to ensure this).
 *
 * je_mustcatch, when set at any runlevel to TRUE, means eval ops must
 * establish a local jmpenv to handle exception traps.  Care must be taken
 * to restore the previous value of je_mustcatch before exiting the
 * stack frame iff JMPENV_PUSH was not called in that stack frame.
 * GSAR 97-03-27
 */

struct jmpenv {
    struct jmpenv *	je_prev;
    Sigjmp_buf		je_buf;		/* only for use if !je_throw */
    int			je_ret;		/* last exception thrown */
    bool		je_mustcatch;	/* need to call longjmp()? */
#ifdef PERL_FLEXIBLE_EXCEPTIONS
    void		(*je_throw)(int v); /* last for bincompat */
    bool		je_noset;	/* no need for setjmp() */
#endif
};

typedef struct jmpenv JMPENV;

#ifdef OP_IN_REGISTER
#define OP_REG_TO_MEM	PL_opsave = op
#define OP_MEM_TO_REG	op = PL_opsave
#else
#define OP_REG_TO_MEM	NOOP
#define OP_MEM_TO_REG	NOOP
#endif

/*
 * How to build the first jmpenv.
 *
 * top_env needs to be non-zero. It points to an area
 * in which longjmp() stuff is stored, as C callstack
 * info there at least is thread specific this has to
 * be per-thread. Otherwise a 'die' in a thread gives
 * that thread the C stack of last thread to do an eval {}!
 */

#define JMPENV_BOOTSTRAP \
    STMT_START {				\
	Zero(&PL_start_env, 1, JMPENV);		\
	PL_start_env.je_ret = -1;		\
	PL_start_env.je_mustcatch = TRUE;	\
	PL_top_env = &PL_start_env;		\
    } STMT_END

#ifdef PERL_FLEXIBLE_EXCEPTIONS

/*
 * These exception-handling macros are split up to
 * ease integration with C++ exceptions.
 *
 * To use C++ try+catch to catch Perl exceptions, an extension author
 * needs to first write an extern "C" function to throw an appropriate
 * exception object; typically it will be or contain an integer,
 * because Perl's internals use integers to track exception types:
 *    extern "C" { static void thrower(int i) { throw i; } }
 *
 * Then (as shown below) the author needs to use, not the simple
 * JMPENV_PUSH, but several of its constitutent macros, to arrange for
 * the Perl internals to call thrower() rather than longjmp() to
 * report exceptions:
 *
 *    dJMPENV;
 *    JMPENV_PUSH_INIT(thrower);
 *    try {
 *        ... stuff that may throw exceptions ...
 *    }
 *    catch (int why) {  // or whatever matches thrower()
 *        JMPENV_POST_CATCH;
 *        EXCEPT_SET(why);
 *        switch (why) {
 *          ... // handle various Perl exception codes
 *        }
 *    }
 *    JMPENV_POP;  // don't forget this!
 */

/*
 * Function that catches/throws, and its callback for the
 *  body of protected processing.
 */
typedef void *(CPERLscope(*protect_body_t)) (pTHX_ va_list);
typedef void *(CPERLscope(*protect_proc_t)) (pTHX_ volatile JMPENV *pcur_env,
					     int *, protect_body_t, ...);

#define dJMPENV	JMPENV cur_env;	\
		volatile JMPENV *pcur_env = ((cur_env.je_noset = 0),&cur_env)

#define JMPENV_PUSH_INIT_ENV(ce,THROWFUNC) \
    STMT_START {					\
	(ce).je_throw = (THROWFUNC);			\
	(ce).je_ret = -1;				\
	(ce).je_mustcatch = FALSE;			\
	(ce).je_prev = PL_top_env;			\
	PL_top_env = &(ce);				\
	OP_REG_TO_MEM;					\
    } STMT_END

#define JMPENV_PUSH_INIT(THROWFUNC) JMPENV_PUSH_INIT_ENV(*(JMPENV*)pcur_env,THROWFUNC)

#define JMPENV_POST_CATCH_ENV(ce) \
    STMT_START {					\
	OP_MEM_TO_REG;					\
	PL_top_env = &(ce);				\
    } STMT_END

#define JMPENV_POST_CATCH JMPENV_POST_CATCH_ENV(*(JMPENV*)pcur_env)

#define JMPENV_PUSH_ENV(ce,v) \
    STMT_START {						\
	if (!(ce).je_noset) {					\
	    DEBUG_l(Perl_deb(aTHX_ "Setting up jumplevel %p, was %p\n",	\
			     ce, PL_top_env));			\
	    JMPENV_PUSH_INIT_ENV(ce,NULL);			\
	    EXCEPT_SET_ENV(ce,PerlProc_setjmp((ce).je_buf, 1));\
	    (ce).je_noset = 1;					\
	}							\
	else							\
	    EXCEPT_SET_ENV(ce,0);				\
	JMPENV_POST_CATCH_ENV(ce);				\
	(v) = EXCEPT_GET_ENV(ce);				\
    } STMT_END

#define JMPENV_PUSH(v) JMPENV_PUSH_ENV(*(JMPENV*)pcur_env,v)

#define JMPENV_POP_ENV(ce) \
    STMT_START {						\
	if (PL_top_env == &(ce))				\
	    PL_top_env = (ce).je_prev;				\
    } STMT_END

#define JMPENV_POP  JMPENV_POP_ENV(*(JMPENV*)pcur_env)

#define JMPENV_JUMP(v) \
    STMT_START {						\
	OP_REG_TO_MEM;						\
	if (PL_top_env->je_prev) {				\
	    if (PL_top_env->je_throw)				\
		PL_top_env->je_throw(v);			\
	    else						\
		PerlProc_longjmp(PL_top_env->je_buf, (v));	\
	}							\
	if ((v) == 2)						\
	    PerlProc_exit(STATUS_NATIVE_EXPORT);		\
	PerlIO_printf(Perl_error_log, "panic: top_env\n");	\
	PerlProc_exit(1);					\
    } STMT_END

#define EXCEPT_GET_ENV(ce)	((ce).je_ret)
#define EXCEPT_GET		EXCEPT_GET_ENV(*(JMPENV*)pcur_env)
#define EXCEPT_SET_ENV(ce,v)	((ce).je_ret = (v))
#define EXCEPT_SET(v)		EXCEPT_SET_ENV(*(JMPENV*)pcur_env,v)

#else /* !PERL_FLEXIBLE_EXCEPTIONS */

#define dJMPENV		JMPENV cur_env

#define JMPENV_PUSH(v) \
    STMT_START {							\
	DEBUG_l(Perl_deb(aTHX_ "Setting up jumplevel %p, was %p\n",	\
			 &cur_env, PL_top_env));			\
	cur_env.je_prev = PL_top_env;					\
	OP_REG_TO_MEM;							\
	cur_env.je_ret = PerlProc_setjmp(cur_env.je_buf, 1);		\
	OP_MEM_TO_REG;							\
	PL_top_env = &cur_env;						\
	cur_env.je_mustcatch = FALSE;					\
	(v) = cur_env.je_ret;						\
    } STMT_END

#define JMPENV_POP \
    STMT_START { PL_top_env = cur_env.je_prev; } STMT_END

#define JMPENV_JUMP(v) \
    STMT_START {						\
	OP_REG_TO_MEM;						\
	if (PL_top_env->je_prev)				\
	    PerlProc_longjmp(PL_top_env->je_buf, (v));		\
	if ((v) == 2)						\
	    PerlProc_exit(STATUS_NATIVE_EXPORT);		\
	PerlIO_printf(PerlIO_stderr(), "panic: top_env\n");	\
	PerlProc_exit(1);					\
    } STMT_END

#endif /* PERL_FLEXIBLE_EXCEPTIONS */

#define CATCH_GET		(PL_top_env->je_mustcatch)
#define CATCH_SET(v)		(PL_top_env->je_mustcatch = (v))
