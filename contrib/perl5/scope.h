#define SAVEt_ITEM	0
#define SAVEt_SV	1
#define SAVEt_AV	2
#define SAVEt_HV	3
#define SAVEt_INT	4
#define SAVEt_LONG	5
#define SAVEt_I32	6
#define SAVEt_IV	7
#define SAVEt_SPTR	8
#define SAVEt_APTR	9
#define SAVEt_HPTR	10
#define SAVEt_PPTR	11
#define SAVEt_NSTAB	12
#define SAVEt_SVREF	13
#define SAVEt_GP	14
#define SAVEt_FREESV	15
#define SAVEt_FREEOP	16
#define SAVEt_FREEPV	17
#define SAVEt_CLEARSV	18
#define SAVEt_DELETE	19
#define SAVEt_DESTRUCTOR 20
#define SAVEt_REGCONTEXT 21
#define SAVEt_STACK_POS  22
#define SAVEt_I16	23
#define SAVEt_AELEM     24
#define SAVEt_HELEM     25
#define SAVEt_OP	26
#define SAVEt_HINTS	27
/* #define SAVEt_ALLOC		28 */ /* defined in 5.005_5x */
#define SAVEt_GENERIC_SVREF	29

#define SSCHECK(need) if (PL_savestack_ix + need > PL_savestack_max) savestack_grow()
#define SSPUSHINT(i) (PL_savestack[PL_savestack_ix++].any_i32 = (I32)(i))
#define SSPUSHLONG(i) (PL_savestack[PL_savestack_ix++].any_long = (long)(i))
#define SSPUSHIV(i) (PL_savestack[PL_savestack_ix++].any_iv = (IV)(i))
#define SSPUSHPTR(p) (PL_savestack[PL_savestack_ix++].any_ptr = (void*)(p))
#define SSPUSHDPTR(p) (PL_savestack[PL_savestack_ix++].any_dptr = (p))
#define SSPOPINT (PL_savestack[--PL_savestack_ix].any_i32)
#define SSPOPLONG (PL_savestack[--PL_savestack_ix].any_long)
#define SSPOPIV (PL_savestack[--PL_savestack_ix].any_iv)
#define SSPOPPTR (PL_savestack[--PL_savestack_ix].any_ptr)
#define SSPOPDPTR (PL_savestack[--PL_savestack_ix].any_dptr)

#define SAVETMPS save_int((int*)&PL_tmps_floor), PL_tmps_floor = PL_tmps_ix
#define FREETMPS if (PL_tmps_ix > PL_tmps_floor) free_tmps()

#ifdef DEBUGGING
#define ENTER							\
    STMT_START {						\
	push_scope();						\
	DEBUG_l(WITH_THR(deb("ENTER scope %ld at %s:%d\n",	\
		    PL_scopestack_ix, __FILE__, __LINE__)));	\
    } STMT_END
#define LEAVE							\
    STMT_START {						\
	DEBUG_l(WITH_THR(deb("LEAVE scope %ld at %s:%d\n",	\
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
#define SAVEI16(i)	save_I16(SOFT_CAST(I16*)&(i))
#define SAVEI32(i)	save_I32(SOFT_CAST(I32*)&(i))
#define SAVEINT(i)	save_int(SOFT_CAST(int*)&(i))
#define SAVEIV(i)	save_iv(SOFT_CAST(IV*)&(i))
#define SAVELONG(l)	save_long(SOFT_CAST(long*)&(l))
#define SAVESPTR(s)	save_sptr((SV**)&(s))
#define SAVEPPTR(s)	save_pptr(SOFT_CAST(char**)&(s))
#define SAVEFREESV(s)	save_freesv((SV*)(s))
#define SAVEFREEOP(o)	save_freeop(SOFT_CAST(OP*)(o))
#define SAVEFREEPV(p)	save_freepv(SOFT_CAST(char*)(p))
#define SAVECLEARSV(sv)	save_clearsv(SOFT_CAST(SV**)&(sv))
#define SAVEGENERICSV(s)	save_generic_svref((SV**)&(s))
#define SAVEDELETE(h,k,l) \
	  save_delete(SOFT_CAST(HV*)(h), SOFT_CAST(char*)(k), (I32)(l))
#ifdef PERL_OBJECT
#define CALLDESTRUCTOR this->*SSPOPDPTR
#define SAVEDESTRUCTOR(f,p) \
	  save_destructor((DESTRUCTORFUNC)(FUNC_NAME_TO_PTR(f)),	\
			  SOFT_CAST(void*)(p))
#else
#define CALLDESTRUCTOR *SSPOPDPTR
#define SAVEDESTRUCTOR(f,p) \
	  save_destructor(SOFT_CAST(void(*)_((void*)))(FUNC_NAME_TO_PTR(f)), \
			  SOFT_CAST(void*)(p))
#endif

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
    Sigjmp_buf		je_buf;		
    int			je_ret;		/* return value of last setjmp() */
    bool		je_mustcatch;	/* longjmp()s must be caught locally */
};

typedef struct jmpenv JMPENV;

#ifdef OP_IN_REGISTER
#define OP_REG_TO_MEM	PL_opsave = op
#define OP_MEM_TO_REG	op = PL_opsave
#else
#define OP_REG_TO_MEM	NOOP
#define OP_MEM_TO_REG	NOOP
#endif

#define dJMPENV		JMPENV cur_env
#define JMPENV_PUSH(v) \
    STMT_START {					\
	cur_env.je_prev = PL_top_env;			\
	OP_REG_TO_MEM;					\
	cur_env.je_ret = PerlProc_setjmp(cur_env.je_buf, 1);	\
	OP_MEM_TO_REG;					\
	PL_top_env = &cur_env;				\
	cur_env.je_mustcatch = FALSE;			\
	(v) = cur_env.je_ret;				\
    } STMT_END
#define JMPENV_POP \
    STMT_START { PL_top_env = cur_env.je_prev; } STMT_END
#define JMPENV_JUMP(v) \
    STMT_START {						\
	OP_REG_TO_MEM;						\
	if (PL_top_env->je_prev)					\
	    PerlProc_longjmp(PL_top_env->je_buf, (v));			\
	if ((v) == 2)						\
	    PerlProc_exit(STATUS_NATIVE_EXPORT);				\
	PerlIO_printf(PerlIO_stderr(), "panic: top_env\n");	\
	PerlProc_exit(1);						\
    } STMT_END
   
#define CATCH_GET	(PL_top_env->je_mustcatch)
#define CATCH_SET(v)	(PL_top_env->je_mustcatch = (v))
   
