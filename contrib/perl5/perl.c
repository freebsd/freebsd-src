/*    perl.c
 *
 *    Copyright (c) 1987-2001 Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * "A ship then new they built for him/of mithril and of elven glass" --Bilbo
 */

#include "EXTERN.h"
#define PERL_IN_PERL_C
#include "perl.h"
#include "patchlevel.h"			/* for local_patches */

/* XXX If this causes problems, set i_unistd=undef in the hint file.  */
#ifdef I_UNISTD
#include <unistd.h>
#endif

#if !defined(STANDARD_C) && !defined(HAS_GETENV_PROTOTYPE)
char *getenv (char *); /* Usually in <stdlib.h> */
#endif

static I32 read_e_script(pTHXo_ int idx, SV *buf_sv, int maxlen);

#ifdef IAMSUID
#ifndef DOSUID
#define DOSUID
#endif
#endif

#ifdef SETUID_SCRIPTS_ARE_SECURE_NOW
#ifdef DOSUID
#undef DOSUID
#endif
#endif

#ifdef PERL_OBJECT
#define perl_construct	Perl_construct
#define perl_parse	Perl_parse
#define perl_run	Perl_run
#define perl_destruct	Perl_destruct
#define perl_free	Perl_free
#endif

#if defined(USE_THREADS)
#  define INIT_TLS_AND_INTERP \
    STMT_START {				\
	if (!PL_curinterp) {			\
	    PERL_SET_INTERP(my_perl);		\
	    INIT_THREADS;			\
	    ALLOC_THREAD_KEY;			\
	}					\
    } STMT_END
#else
#  if defined(USE_ITHREADS)
#  define INIT_TLS_AND_INTERP \
    STMT_START {				\
	if (!PL_curinterp) {			\
	    PERL_SET_INTERP(my_perl);		\
	    INIT_THREADS;			\
	    ALLOC_THREAD_KEY;			\
	    PERL_SET_THX(my_perl);		\
	    OP_REFCNT_INIT;			\
	}					\
	else {					\
	    PERL_SET_THX(my_perl);		\
	}					\
    } STMT_END
#  else
#  define INIT_TLS_AND_INTERP \
    STMT_START {				\
	if (!PL_curinterp) {			\
	    PERL_SET_INTERP(my_perl);		\
	}					\
	PERL_SET_THX(my_perl);			\
    } STMT_END
#  endif
#endif

#ifdef PERL_IMPLICIT_SYS
PerlInterpreter *
perl_alloc_using(struct IPerlMem* ipM, struct IPerlMem* ipMS,
		 struct IPerlMem* ipMP, struct IPerlEnv* ipE,
		 struct IPerlStdIO* ipStd, struct IPerlLIO* ipLIO,
		 struct IPerlDir* ipD, struct IPerlSock* ipS,
		 struct IPerlProc* ipP)
{
    PerlInterpreter *my_perl;
#ifdef PERL_OBJECT
    my_perl = (PerlInterpreter*)new(ipM) CPerlObj(ipM, ipMS, ipMP, ipE, ipStd,
						  ipLIO, ipD, ipS, ipP);
    INIT_TLS_AND_INTERP;
#else
    /* New() needs interpreter, so call malloc() instead */
    my_perl = (PerlInterpreter*)(*ipM->pMalloc)(ipM, sizeof(PerlInterpreter));
    INIT_TLS_AND_INTERP;
    Zero(my_perl, 1, PerlInterpreter);
    PL_Mem = ipM;
    PL_MemShared = ipMS;
    PL_MemParse = ipMP;
    PL_Env = ipE;
    PL_StdIO = ipStd;
    PL_LIO = ipLIO;
    PL_Dir = ipD;
    PL_Sock = ipS;
    PL_Proc = ipP;
#endif

    return my_perl;
}
#else

/*
=for apidoc perl_alloc

Allocates a new Perl interpreter.  See L<perlembed>.

=cut
*/

PerlInterpreter *
perl_alloc(void)
{
    PerlInterpreter *my_perl;

    /* New() needs interpreter, so call malloc() instead */
    my_perl = (PerlInterpreter*)PerlMem_malloc(sizeof(PerlInterpreter));

    INIT_TLS_AND_INTERP;
    Zero(my_perl, 1, PerlInterpreter);
    return my_perl;
}
#endif /* PERL_IMPLICIT_SYS */

/*
=for apidoc perl_construct

Initializes a new Perl interpreter.  See L<perlembed>.

=cut
*/

void
perl_construct(pTHXx)
{
#ifdef USE_THREADS
    int i;
#ifndef FAKE_THREADS
    struct perl_thread *thr = NULL;
#endif /* FAKE_THREADS */
#endif /* USE_THREADS */

#ifdef MULTIPLICITY
    init_interp();
    PL_perl_destruct_level = 1; 
#else
   if (PL_perl_destruct_level > 0)
       init_interp();
#endif

   /* Init the real globals (and main thread)? */
    if (!PL_linestr) {
#ifdef USE_THREADS
	MUTEX_INIT(&PL_sv_mutex);
	/*
	 * Safe to use basic SV functions from now on (though
	 * not things like mortals or tainting yet).
	 */
	MUTEX_INIT(&PL_eval_mutex);
	COND_INIT(&PL_eval_cond);
	MUTEX_INIT(&PL_threads_mutex);
	COND_INIT(&PL_nthreads_cond);
#  ifdef EMULATE_ATOMIC_REFCOUNTS
	MUTEX_INIT(&PL_svref_mutex);
#  endif /* EMULATE_ATOMIC_REFCOUNTS */
	
	MUTEX_INIT(&PL_cred_mutex);
	MUTEX_INIT(&PL_sv_lock_mutex);
	MUTEX_INIT(&PL_fdpid_mutex);

	thr = init_main_thread();
#endif /* USE_THREADS */

#ifdef PERL_FLEXIBLE_EXCEPTIONS
	PL_protect = MEMBER_TO_FPTR(Perl_default_protect); /* for exceptions */
#endif

	PL_curcop = &PL_compiling;	/* needed by ckWARN, right away */

	PL_linestr = NEWSV(65,79);
	sv_upgrade(PL_linestr,SVt_PVIV);

	if (!SvREADONLY(&PL_sv_undef)) {
	    /* set read-only and try to insure than we wont see REFCNT==0
	       very often */

	    SvREADONLY_on(&PL_sv_undef);
	    SvREFCNT(&PL_sv_undef) = (~(U32)0)/2;

	    sv_setpv(&PL_sv_no,PL_No);
	    SvNV(&PL_sv_no);
	    SvREADONLY_on(&PL_sv_no);
	    SvREFCNT(&PL_sv_no) = (~(U32)0)/2;

	    sv_setpv(&PL_sv_yes,PL_Yes);
	    SvNV(&PL_sv_yes);
	    SvREADONLY_on(&PL_sv_yes);
	    SvREFCNT(&PL_sv_yes) = (~(U32)0)/2;
	}

#ifdef PERL_OBJECT
	/* TODO: */
	/* PL_sighandlerp = sighandler; */
#else
	PL_sighandlerp = Perl_sighandler;
#endif
	PL_pidstatus = newHV();

#ifdef MSDOS
	/*
	 * There is no way we can refer to them from Perl so close them to save
	 * space.  The other alternative would be to provide STDAUX and STDPRN
	 * filehandles.
	 */
	(void)fclose(stdaux);
	(void)fclose(stdprn);
#endif
    }

    PL_nrs = newSVpvn("\n", 1);
    PL_rs = SvREFCNT_inc(PL_nrs);

    init_stacks();

    init_ids();
    PL_lex_state = LEX_NOTPARSING;

    JMPENV_BOOTSTRAP;
    STATUS_ALL_SUCCESS;

    init_i18nl10n(1);
    SET_NUMERIC_STANDARD();

    {
	U8 *s;
	PL_patchlevel = NEWSV(0,4);
	(void)SvUPGRADE(PL_patchlevel, SVt_PVNV);
	if (PERL_REVISION > 127 || PERL_VERSION > 127 || PERL_SUBVERSION > 127)
	    SvGROW(PL_patchlevel, UTF8_MAXLEN*3+1);
	s = (U8*)SvPVX(PL_patchlevel);
	s = uv_to_utf8(s, (UV)PERL_REVISION);
	s = uv_to_utf8(s, (UV)PERL_VERSION);
	s = uv_to_utf8(s, (UV)PERL_SUBVERSION);
	*s = '\0';
	SvCUR_set(PL_patchlevel, s - (U8*)SvPVX(PL_patchlevel));
	SvPOK_on(PL_patchlevel);
	SvNVX(PL_patchlevel) = (NV)PERL_REVISION
				+ ((NV)PERL_VERSION / (NV)1000)
#if defined(PERL_SUBVERSION) && PERL_SUBVERSION > 0
				+ ((NV)PERL_SUBVERSION / (NV)1000000)
#endif
				;
	SvNOK_on(PL_patchlevel);	/* dual valued */
	SvUTF8_on(PL_patchlevel);
	SvREADONLY_on(PL_patchlevel);
    }

#if defined(LOCAL_PATCH_COUNT)
    PL_localpatches = local_patches;	/* For possible -v */
#endif

#ifdef HAVE_INTERP_INTERN
    sys_intern_init();
#endif

    PerlIO_init();			/* Hook to IO system */

    PL_fdpid = newAV();			/* for remembering popen pids by fd */
    PL_modglobal = newHV();		/* pointers to per-interpreter module globals */
    PL_errors = newSVpvn("",0);

    ENTER;
}

/*
=for apidoc perl_destruct

Shuts down a Perl interpreter.  See L<perlembed>.

=cut
*/

void
perl_destruct(pTHXx)
{
    int destruct_level;  /* 0=none, 1=full, 2=full with checks */
    HV *hv;
#ifdef USE_THREADS
    Thread t;
    dTHX;
#endif /* USE_THREADS */

    /* wait for all pseudo-forked children to finish */
    PERL_WAIT_FOR_CHILDREN;

#ifdef USE_THREADS
#ifndef FAKE_THREADS
    /* Pass 1 on any remaining threads: detach joinables, join zombies */
  retry_cleanup:
    MUTEX_LOCK(&PL_threads_mutex);
    DEBUG_S(PerlIO_printf(Perl_debug_log,
			  "perl_destruct: waiting for %d threads...\n",
			  PL_nthreads - 1));
    for (t = thr->next; t != thr; t = t->next) {
	MUTEX_LOCK(&t->mutex);
	switch (ThrSTATE(t)) {
	    AV *av;
	case THRf_ZOMBIE:
	    DEBUG_S(PerlIO_printf(Perl_debug_log,
				  "perl_destruct: joining zombie %p\n", t));
	    ThrSETSTATE(t, THRf_DEAD);
	    MUTEX_UNLOCK(&t->mutex);
	    PL_nthreads--;
	    /*
	     * The SvREFCNT_dec below may take a long time (e.g. av
	     * may contain an object scalar whose destructor gets
	     * called) so we have to unlock threads_mutex and start
	     * all over again.
	     */
	    MUTEX_UNLOCK(&PL_threads_mutex);
	    JOIN(t, &av);
	    SvREFCNT_dec((SV*)av);
	    DEBUG_S(PerlIO_printf(Perl_debug_log,
				  "perl_destruct: joined zombie %p OK\n", t));
	    goto retry_cleanup;
	case THRf_R_JOINABLE:
	    DEBUG_S(PerlIO_printf(Perl_debug_log,
				  "perl_destruct: detaching thread %p\n", t));
	    ThrSETSTATE(t, THRf_R_DETACHED);
	    /* 
	     * We unlock threads_mutex and t->mutex in the opposite order
	     * from which we locked them just so that DETACH won't
	     * deadlock if it panics. It's only a breach of good style
	     * not a bug since they are unlocks not locks.
	     */
	    MUTEX_UNLOCK(&PL_threads_mutex);
	    DETACH(t);
	    MUTEX_UNLOCK(&t->mutex);
	    goto retry_cleanup;
	default:
	    DEBUG_S(PerlIO_printf(Perl_debug_log,
				  "perl_destruct: ignoring %p (state %u)\n",
				  t, ThrSTATE(t)));
	    MUTEX_UNLOCK(&t->mutex);
	    /* fall through and out */
	}
    }
    /* We leave the above "Pass 1" loop with threads_mutex still locked */

    /* Pass 2 on remaining threads: wait for the thread count to drop to one */
    while (PL_nthreads > 1)
    {
	DEBUG_S(PerlIO_printf(Perl_debug_log,
			      "perl_destruct: final wait for %d threads\n",
			      PL_nthreads - 1));
	COND_WAIT(&PL_nthreads_cond, &PL_threads_mutex);
    }
    /* At this point, we're the last thread */
    MUTEX_UNLOCK(&PL_threads_mutex);
    DEBUG_S(PerlIO_printf(Perl_debug_log, "perl_destruct: armageddon has arrived\n"));
    MUTEX_DESTROY(&PL_threads_mutex);
    COND_DESTROY(&PL_nthreads_cond);
    PL_nthreads--;
#endif /* !defined(FAKE_THREADS) */
#endif /* USE_THREADS */

    destruct_level = PL_perl_destruct_level;
#ifdef DEBUGGING
    {
	char *s;
	if ((s = PerlEnv_getenv("PERL_DESTRUCT_LEVEL"))) {
	    int i = atoi(s);
	    if (destruct_level < i)
		destruct_level = i;
	}
    }
#endif

    LEAVE;
    FREETMPS;

    /* We must account for everything.  */

    /* Destroy the main CV and syntax tree */
    if (PL_main_root) {
	PL_curpad = AvARRAY(PL_comppad);
	op_free(PL_main_root);
	PL_main_root = Nullop;
    }
    PL_curcop = &PL_compiling;
    PL_main_start = Nullop;
    SvREFCNT_dec(PL_main_cv);
    PL_main_cv = Nullcv;
    PL_dirty = TRUE;

    if (PL_sv_objcount) {
	/*
	 * Try to destruct global references.  We do this first so that the
	 * destructors and destructees still exist.  Some sv's might remain.
	 * Non-referenced objects are on their own.
	 */
	sv_clean_objs();
    }

    /* unhook hooks which will soon be, or use, destroyed data */
    SvREFCNT_dec(PL_warnhook);
    PL_warnhook = Nullsv;
    SvREFCNT_dec(PL_diehook);
    PL_diehook = Nullsv;

    /* call exit list functions */
    while (PL_exitlistlen-- > 0)
	PL_exitlist[PL_exitlistlen].fn(aTHXo_ PL_exitlist[PL_exitlistlen].ptr);

    Safefree(PL_exitlist);

    if (destruct_level == 0){

	DEBUG_P(debprofdump());
    
	/* The exit() function will do everything that needs doing. */
	return;
    }

    /* jettison our possibly duplicated environment */

#ifdef USE_ENVIRON_ARRAY
    if (environ != PL_origenviron) {
	I32 i;

	for (i = 0; environ[i]; i++)
	    safesysfree(environ[i]);
	/* Must use safesysfree() when working with environ. */
	safesysfree(environ);		

	environ = PL_origenviron;
    }
#endif

    /* loosen bonds of global variables */

    if(PL_rsfp) {
	(void)PerlIO_close(PL_rsfp);
	PL_rsfp = Nullfp;
    }

    /* Filters for program text */
    SvREFCNT_dec(PL_rsfp_filters);
    PL_rsfp_filters = Nullav;

    /* switches */
    PL_preprocess   = FALSE;
    PL_minus_n      = FALSE;
    PL_minus_p      = FALSE;
    PL_minus_l      = FALSE;
    PL_minus_a      = FALSE;
    PL_minus_F      = FALSE;
    PL_doswitches   = FALSE;
    PL_dowarn       = G_WARN_OFF;
    PL_doextract    = FALSE;
    PL_sawampersand = FALSE;	/* must save all match strings */
    PL_unsafe       = FALSE;

    Safefree(PL_inplace);
    PL_inplace = Nullch;
    SvREFCNT_dec(PL_patchlevel);

    if (PL_e_script) {
	SvREFCNT_dec(PL_e_script);
	PL_e_script = Nullsv;
    }

    /* magical thingies */

    Safefree(PL_ofs);		/* $, */
    PL_ofs = Nullch;

    Safefree(PL_ors);		/* $\ */
    PL_ors = Nullch;

    SvREFCNT_dec(PL_rs);	/* $/ */
    PL_rs = Nullsv;

    SvREFCNT_dec(PL_nrs);	/* $/ helper */
    PL_nrs = Nullsv;

    PL_multiline = 0;		/* $* */
    Safefree(PL_osname);	/* $^O */
    PL_osname = Nullch;

    SvREFCNT_dec(PL_statname);
    PL_statname = Nullsv;
    PL_statgv = Nullgv;

    /* defgv, aka *_ should be taken care of elsewhere */

    /* clean up after study() */
    SvREFCNT_dec(PL_lastscream);
    PL_lastscream = Nullsv;
    Safefree(PL_screamfirst);
    PL_screamfirst = 0;
    Safefree(PL_screamnext);
    PL_screamnext  = 0;

    /* float buffer */
    Safefree(PL_efloatbuf);
    PL_efloatbuf = Nullch;
    PL_efloatsize = 0;

    /* startup and shutdown function lists */
    SvREFCNT_dec(PL_beginav);
    SvREFCNT_dec(PL_endav);
    SvREFCNT_dec(PL_checkav);
    SvREFCNT_dec(PL_initav);
    PL_beginav = Nullav;
    PL_endav = Nullav;
    PL_checkav = Nullav;
    PL_initav = Nullav;

    /* shortcuts just get cleared */
    PL_envgv = Nullgv;
    PL_incgv = Nullgv;
    PL_hintgv = Nullgv;
    PL_errgv = Nullgv;
    PL_argvgv = Nullgv;
    PL_argvoutgv = Nullgv;
    PL_stdingv = Nullgv;
    PL_stderrgv = Nullgv;
    PL_last_in_gv = Nullgv;
    PL_replgv = Nullgv;
    PL_debstash = Nullhv;

    /* reset so print() ends up where we expect */
    setdefout(Nullgv);

    SvREFCNT_dec(PL_argvout_stack);
    PL_argvout_stack = Nullav;

    SvREFCNT_dec(PL_modglobal);
    PL_modglobal = Nullhv;
    SvREFCNT_dec(PL_preambleav);
    PL_preambleav = Nullav;
    SvREFCNT_dec(PL_subname);
    PL_subname = Nullsv;
    SvREFCNT_dec(PL_linestr);
    PL_linestr = Nullsv;
    SvREFCNT_dec(PL_pidstatus);
    PL_pidstatus = Nullhv;
    SvREFCNT_dec(PL_toptarget);
    PL_toptarget = Nullsv;
    SvREFCNT_dec(PL_bodytarget);
    PL_bodytarget = Nullsv;
    PL_formtarget = Nullsv;

    /* free locale stuff */
#ifdef USE_LOCALE_COLLATE
    Safefree(PL_collation_name);
    PL_collation_name = Nullch;
#endif

#ifdef USE_LOCALE_NUMERIC
    Safefree(PL_numeric_name);
    PL_numeric_name = Nullch;
    SvREFCNT_dec(PL_numeric_radix_sv);
#endif

    /* clear utf8 character classes */
    SvREFCNT_dec(PL_utf8_alnum);
    SvREFCNT_dec(PL_utf8_alnumc);
    SvREFCNT_dec(PL_utf8_ascii);
    SvREFCNT_dec(PL_utf8_alpha);
    SvREFCNT_dec(PL_utf8_space);
    SvREFCNT_dec(PL_utf8_cntrl);
    SvREFCNT_dec(PL_utf8_graph);
    SvREFCNT_dec(PL_utf8_digit);
    SvREFCNT_dec(PL_utf8_upper);
    SvREFCNT_dec(PL_utf8_lower);
    SvREFCNT_dec(PL_utf8_print);
    SvREFCNT_dec(PL_utf8_punct);
    SvREFCNT_dec(PL_utf8_xdigit);
    SvREFCNT_dec(PL_utf8_mark);
    SvREFCNT_dec(PL_utf8_toupper);
    SvREFCNT_dec(PL_utf8_tolower);
    PL_utf8_alnum	= Nullsv;
    PL_utf8_alnumc	= Nullsv;
    PL_utf8_ascii	= Nullsv;
    PL_utf8_alpha	= Nullsv;
    PL_utf8_space	= Nullsv;
    PL_utf8_cntrl	= Nullsv;
    PL_utf8_graph	= Nullsv;
    PL_utf8_digit	= Nullsv;
    PL_utf8_upper	= Nullsv;
    PL_utf8_lower	= Nullsv;
    PL_utf8_print	= Nullsv;
    PL_utf8_punct	= Nullsv;
    PL_utf8_xdigit	= Nullsv;
    PL_utf8_mark	= Nullsv;
    PL_utf8_toupper	= Nullsv;
    PL_utf8_totitle	= Nullsv;
    PL_utf8_tolower	= Nullsv;

    if (!specialWARN(PL_compiling.cop_warnings))
	SvREFCNT_dec(PL_compiling.cop_warnings);
    PL_compiling.cop_warnings = Nullsv;
#ifdef USE_ITHREADS
    Safefree(CopFILE(&PL_compiling));
    CopFILE(&PL_compiling) = Nullch;
    Safefree(CopSTASHPV(&PL_compiling));
#else
    SvREFCNT_dec(CopFILEGV(&PL_compiling));
    CopFILEGV(&PL_compiling) = Nullgv;
    /* cop_stash is not refcounted */
#endif

    /* Prepare to destruct main symbol table.  */

    hv = PL_defstash;
    PL_defstash = 0;
    SvREFCNT_dec(hv);
    SvREFCNT_dec(PL_curstname);
    PL_curstname = Nullsv;

    /* clear queued errors */
    SvREFCNT_dec(PL_errors);
    PL_errors = Nullsv;

    FREETMPS;
    if (destruct_level >= 2 && ckWARN_d(WARN_INTERNAL)) {
	if (PL_scopestack_ix != 0)
	    Perl_warner(aTHX_ WARN_INTERNAL,
	         "Unbalanced scopes: %ld more ENTERs than LEAVEs\n",
		 (long)PL_scopestack_ix);
	if (PL_savestack_ix != 0)
	    Perl_warner(aTHX_ WARN_INTERNAL,
		 "Unbalanced saves: %ld more saves than restores\n",
		 (long)PL_savestack_ix);
	if (PL_tmps_floor != -1)
	    Perl_warner(aTHX_ WARN_INTERNAL,"Unbalanced tmps: %ld more allocs than frees\n",
		 (long)PL_tmps_floor + 1);
	if (cxstack_ix != -1)
	    Perl_warner(aTHX_ WARN_INTERNAL,"Unbalanced context: %ld more PUSHes than POPs\n",
		 (long)cxstack_ix + 1);
    }

    /* Now absolutely destruct everything, somehow or other, loops or no. */
    SvFLAGS(PL_fdpid) |= SVTYPEMASK;		/* don't clean out pid table now */
    SvFLAGS(PL_strtab) |= SVTYPEMASK;		/* don't clean out strtab now */

    /* the 2 is for PL_fdpid and PL_strtab */
    while (PL_sv_count > 2 && sv_clean_all())
	;

    SvFLAGS(PL_fdpid) &= ~SVTYPEMASK;
    SvFLAGS(PL_fdpid) |= SVt_PVAV;
    SvFLAGS(PL_strtab) &= ~SVTYPEMASK;
    SvFLAGS(PL_strtab) |= SVt_PVHV;

    AvREAL_off(PL_fdpid);		/* no surviving entries */
    SvREFCNT_dec(PL_fdpid);		/* needed in io_close() */
    PL_fdpid = Nullav;

#ifdef HAVE_INTERP_INTERN
    sys_intern_clear();
#endif

    /* Destruct the global string table. */
    {
	/* Yell and reset the HeVAL() slots that are still holding refcounts,
	 * so that sv_free() won't fail on them.
	 */
	I32 riter;
	I32 max;
	HE *hent;
	HE **array;

	riter = 0;
	max = HvMAX(PL_strtab);
	array = HvARRAY(PL_strtab);
	hent = array[0];
	for (;;) {
	    if (hent && ckWARN_d(WARN_INTERNAL)) {
		Perl_warner(aTHX_ WARN_INTERNAL,
		     "Unbalanced string table refcount: (%d) for \"%s\"",
		     HeVAL(hent) - Nullsv, HeKEY(hent));
		HeVAL(hent) = Nullsv;
		hent = HeNEXT(hent);
	    }
	    if (!hent) {
		if (++riter > max)
		    break;
		hent = array[riter];
	    }
	}
    }
    SvREFCNT_dec(PL_strtab);

#ifdef USE_ITHREADS
    /* free the pointer table used for cloning */
    ptr_table_free(PL_ptr_table);
#endif

    /* free special SVs */

    SvREFCNT(&PL_sv_yes) = 0;
    sv_clear(&PL_sv_yes);
    SvANY(&PL_sv_yes) = NULL;
    SvFLAGS(&PL_sv_yes) = 0;

    SvREFCNT(&PL_sv_no) = 0;
    sv_clear(&PL_sv_no);
    SvANY(&PL_sv_no) = NULL;
    SvFLAGS(&PL_sv_no) = 0;

    SvREFCNT(&PL_sv_undef) = 0;
    SvREADONLY_off(&PL_sv_undef);

    if (PL_sv_count != 0 && ckWARN_d(WARN_INTERNAL))
	Perl_warner(aTHX_ WARN_INTERNAL,"Scalars leaked: %ld\n", (long)PL_sv_count);

    Safefree(PL_origfilename);
    Safefree(PL_reg_start_tmp);
    if (PL_reg_curpm)
	Safefree(PL_reg_curpm);
    Safefree(PL_reg_poscache);
    Safefree(HeKEY_hek(&PL_hv_fetch_ent_mh));
    Safefree(PL_op_mask);
    Safefree(PL_psig_ptr);
    Safefree(PL_psig_name);
    Safefree(PL_bitcount);
    nuke_stacks();
    PL_hints = 0;		/* Reset hints. Should hints be per-interpreter ? */
    
    DEBUG_P(debprofdump());
#ifdef USE_THREADS
    MUTEX_DESTROY(&PL_strtab_mutex);
    MUTEX_DESTROY(&PL_sv_mutex);
    MUTEX_DESTROY(&PL_eval_mutex);
    MUTEX_DESTROY(&PL_cred_mutex);
    MUTEX_DESTROY(&PL_fdpid_mutex);
    COND_DESTROY(&PL_eval_cond);
#ifdef EMULATE_ATOMIC_REFCOUNTS
    MUTEX_DESTROY(&PL_svref_mutex);
#endif /* EMULATE_ATOMIC_REFCOUNTS */

    /* As the penultimate thing, free the non-arena SV for thrsv */
    Safefree(SvPVX(PL_thrsv));
    Safefree(SvANY(PL_thrsv));
    Safefree(PL_thrsv);
    PL_thrsv = Nullsv;
#endif /* USE_THREADS */

    sv_free_arenas();

    /* As the absolutely last thing, free the non-arena SV for mess() */

    if (PL_mess_sv) {
	/* it could have accumulated taint magic */
	if (SvTYPE(PL_mess_sv) >= SVt_PVMG) {
	    MAGIC* mg;
	    MAGIC* moremagic;
	    for (mg = SvMAGIC(PL_mess_sv); mg; mg = moremagic) {
		moremagic = mg->mg_moremagic;
		if (mg->mg_ptr && mg->mg_type != 'g' && mg->mg_len >= 0)
		    Safefree(mg->mg_ptr);
		Safefree(mg);
	    }
	}
	/* we know that type >= SVt_PV */
	(void)SvOOK_off(PL_mess_sv);
	Safefree(SvPVX(PL_mess_sv));
	Safefree(SvANY(PL_mess_sv));
	Safefree(PL_mess_sv);
	PL_mess_sv = Nullsv;
    }
}

/*
=for apidoc perl_free

Releases a Perl interpreter.  See L<perlembed>.

=cut
*/

void
perl_free(pTHXx)
{
#if defined(PERL_OBJECT)
    PerlMem_free(this);
#else
#  if defined(PERL_IMPLICIT_SYS) && defined(WIN32)
    void *host = w32_internal_host;
    PerlMem_free(aTHXx);
    win32_delete_internal_host(host);
#  else
    PerlMem_free(aTHXx);
#  endif
#endif
}

void
Perl_call_atexit(pTHX_ ATEXIT_t fn, void *ptr)
{
    Renew(PL_exitlist, PL_exitlistlen+1, PerlExitListEntry);
    PL_exitlist[PL_exitlistlen].fn = fn;
    PL_exitlist[PL_exitlistlen].ptr = ptr;
    ++PL_exitlistlen;
}

/*
=for apidoc perl_parse

Tells a Perl interpreter to parse a Perl script.  See L<perlembed>.

=cut
*/

int
perl_parse(pTHXx_ XSINIT_t xsinit, int argc, char **argv, char **env)
{
    I32 oldscope;
    int ret;
    dJMPENV;
#ifdef USE_THREADS
    dTHX;
#endif

#ifdef SETUID_SCRIPTS_ARE_SECURE_NOW
#ifdef IAMSUID
#undef IAMSUID
    Perl_croak(aTHX_ "suidperl is no longer needed since the kernel can now execute\n\
setuid perl scripts securely.\n");
#endif
#endif

#if defined(__DYNAMIC__) && (defined(NeXT) || defined(__NeXT__))
    _dyld_lookup_and_bind
	("__environ", (unsigned long *) &environ_pointer, NULL);
#endif /* environ */

    PL_origargv = argv;
    PL_origargc = argc;
#ifdef  USE_ENVIRON_ARRAY
    PL_origenviron = environ;
#endif

    if (PL_do_undump) {

	/* Come here if running an undumped a.out. */

	PL_origfilename = savepv(argv[0]);
	PL_do_undump = FALSE;
	cxstack_ix = -1;		/* start label stack again */
	init_ids();
	init_postdump_symbols(argc,argv,env);
	return 0;
    }

    if (PL_main_root) {
	PL_curpad = AvARRAY(PL_comppad);
	op_free(PL_main_root);
	PL_main_root = Nullop;
    }
    PL_main_start = Nullop;
    SvREFCNT_dec(PL_main_cv);
    PL_main_cv = Nullcv;

    time(&PL_basetime);
    oldscope = PL_scopestack_ix;
    PL_dowarn = G_WARN_OFF;

#ifdef PERL_FLEXIBLE_EXCEPTIONS
    CALLPROTECT(aTHX_ pcur_env, &ret, MEMBER_TO_FPTR(S_vparse_body), env, xsinit);
#else
    JMPENV_PUSH(ret);
#endif
    switch (ret) {
    case 0:
#ifndef PERL_FLEXIBLE_EXCEPTIONS
	parse_body(env,xsinit);
#endif
	if (PL_checkav)
	    call_list(oldscope, PL_checkav);
	ret = 0;
	break;
    case 1:
	STATUS_ALL_FAILURE;
	/* FALL THROUGH */
    case 2:
	/* my_exit() was called */
	while (PL_scopestack_ix > oldscope)
	    LEAVE;
	FREETMPS;
	PL_curstash = PL_defstash;
	if (PL_checkav)
	    call_list(oldscope, PL_checkav);
	ret = STATUS_NATIVE_EXPORT;
	break;
    case 3:
	PerlIO_printf(Perl_error_log, "panic: top_env\n");
	ret = 1;
	break;
    }
    JMPENV_POP;
    return ret;
}

#ifdef PERL_FLEXIBLE_EXCEPTIONS
STATIC void *
S_vparse_body(pTHX_ va_list args)
{
    char **env = va_arg(args, char**);
    XSINIT_t xsinit = va_arg(args, XSINIT_t);

    return parse_body(env, xsinit);
}
#endif

STATIC void *
S_parse_body(pTHX_ char **env, XSINIT_t xsinit)
{
    int argc = PL_origargc;
    char **argv = PL_origargv;
    char *scriptname = NULL;
    int fdscript = -1;
    VOL bool dosearch = FALSE;
    char *validarg = "";
    AV* comppadlist;
    register SV *sv;
    register char *s;
    char *cddir = Nullch;

    sv_setpvn(PL_linestr,"",0);
    sv = newSVpvn("",0);		/* first used for -I flags */
    SAVEFREESV(sv);
    init_main_stash();

    for (argc--,argv++; argc > 0; argc--,argv++) {
	if (argv[0][0] != '-' || !argv[0][1])
	    break;
#ifdef DOSUID
    if (*validarg)
	validarg = " PHOOEY ";
    else
	validarg = argv[0];
#endif
	s = argv[0]+1;
      reswitch:
	switch (*s) {
	case 'C':
#ifdef	WIN32
	    win32_argv2utf8(argc-1, argv+1);
	    /* FALL THROUGH */
#endif
#ifndef PERL_STRICT_CR
	case '\r':
#endif
	case ' ':
	case '0':
	case 'F':
	case 'a':
	case 'c':
	case 'd':
	case 'D':
	case 'h':
	case 'i':
	case 'l':
	case 'M':
	case 'm':
	case 'n':
	case 'p':
	case 's':
	case 'u':
	case 'U':
	case 'v':
	case 'W':
	case 'X':
	case 'w':
	    if ((s = moreswitches(s)))
		goto reswitch;
	    break;

	case 'T':
	    PL_tainting = TRUE;
	    s++;
	    goto reswitch;

	case 'e':
#ifdef MACOS_TRADITIONAL
	    /* ignore -e for Dev:Pseudo argument */
	    if (argv[1] && !strcmp(argv[1], "Dev:Pseudo"))
	    	break; 
#endif
	    if (PL_euid != PL_uid || PL_egid != PL_gid)
		Perl_croak(aTHX_ "No -e allowed in setuid scripts");
	    if (!PL_e_script) {
		PL_e_script = newSVpvn("",0);
		filter_add(read_e_script, NULL);
	    }
	    if (*++s)
		sv_catpv(PL_e_script, s);
	    else if (argv[1]) {
		sv_catpv(PL_e_script, argv[1]);
		argc--,argv++;
	    }
	    else
		Perl_croak(aTHX_ "No code specified for -e");
	    sv_catpv(PL_e_script, "\n");
	    break;

	case 'I':	/* -I handled both here and in moreswitches() */
	    forbid_setid("-I");
	    if (!*++s && (s=argv[1]) != Nullch) {
		argc--,argv++;
	    }
	    if (s && *s) {
		char *p;
		STRLEN len = strlen(s);
		p = savepvn(s, len);
		incpush(p, TRUE, TRUE);
		sv_catpvn(sv, "-I", 2);
		sv_catpvn(sv, p, len);
		sv_catpvn(sv, " ", 1);
		Safefree(p);
	    }
	    else
		Perl_croak(aTHX_ "No directory specified for -I");
	    break;
	case 'P':
	    forbid_setid("-P");
	    PL_preprocess = TRUE;
	    s++;
	    goto reswitch;
	case 'S':
	    forbid_setid("-S");
	    dosearch = TRUE;
	    s++;
	    goto reswitch;
	case 'V':
	    if (!PL_preambleav)
		PL_preambleav = newAV();
	    av_push(PL_preambleav, newSVpv("use Config qw(myconfig config_vars)",0));
	    if (*++s != ':')  {
		PL_Sv = newSVpv("print myconfig();",0);
#ifdef VMS
		sv_catpv(PL_Sv,"print \"\\nCharacteristics of this PERLSHR image: \\n\",");
#else
		sv_catpv(PL_Sv,"print \"\\nCharacteristics of this binary (from libperl): \\n\",");
#endif
		sv_catpv(PL_Sv,"\"  Compile-time options:");
#  ifdef DEBUGGING
		sv_catpv(PL_Sv," DEBUGGING");
#  endif
#  ifdef MULTIPLICITY
		sv_catpv(PL_Sv," MULTIPLICITY");
#  endif
#  ifdef USE_THREADS
		sv_catpv(PL_Sv," USE_THREADS");
#  endif
#  ifdef USE_ITHREADS
		sv_catpv(PL_Sv," USE_ITHREADS");
#  endif
#  ifdef USE_64_BIT_INT
		sv_catpv(PL_Sv," USE_64_BIT_INT");
#  endif
#  ifdef USE_64_BIT_ALL
		sv_catpv(PL_Sv," USE_64_BIT_ALL");
#  endif
#  ifdef USE_LONG_DOUBLE
		sv_catpv(PL_Sv," USE_LONG_DOUBLE");
#  endif
#  ifdef USE_LARGE_FILES
		sv_catpv(PL_Sv," USE_LARGE_FILES");
#  endif
#  ifdef USE_SOCKS
		sv_catpv(PL_Sv," USE_SOCKS");
#  endif
#  ifdef PERL_OBJECT
		sv_catpv(PL_Sv," PERL_OBJECT");
#  endif
#  ifdef PERL_IMPLICIT_CONTEXT
		sv_catpv(PL_Sv," PERL_IMPLICIT_CONTEXT");
#  endif
#  ifdef PERL_IMPLICIT_SYS
		sv_catpv(PL_Sv," PERL_IMPLICIT_SYS");
#  endif
		sv_catpv(PL_Sv,"\\n\",");

#if defined(LOCAL_PATCH_COUNT)
		if (LOCAL_PATCH_COUNT > 0) {
		    int i;
		    sv_catpv(PL_Sv,"\"  Locally applied patches:\\n\",");
		    for (i = 1; i <= LOCAL_PATCH_COUNT; i++) {
			if (PL_localpatches[i])
			    Perl_sv_catpvf(aTHX_ PL_Sv,"q\"  \t%s\n\",",PL_localpatches[i]);
		    }
		}
#endif
		Perl_sv_catpvf(aTHX_ PL_Sv,"\"  Built under %s\\n\"",OSNAME);
#ifdef __DATE__
#  ifdef __TIME__
		Perl_sv_catpvf(aTHX_ PL_Sv,",\"  Compiled at %s %s\\n\"",__DATE__,__TIME__);
#  else
		Perl_sv_catpvf(aTHX_ PL_Sv,",\"  Compiled on %s\\n\"",__DATE__);
#  endif
#endif
		sv_catpv(PL_Sv, "; \
$\"=\"\\n    \"; \
@env = map { \"$_=\\\"$ENV{$_}\\\"\" } sort grep {/^PERL/} keys %ENV; \
print \"  \\%ENV:\\n    @env\\n\" if @env; \
print \"  \\@INC:\\n    @INC\\n\";");
	    }
	    else {
		PL_Sv = newSVpv("config_vars(qw(",0);
		sv_catpv(PL_Sv, ++s);
		sv_catpv(PL_Sv, "))");
		s += strlen(s);
	    }
	    av_push(PL_preambleav, PL_Sv);
	    scriptname = BIT_BUCKET;	/* don't look for script or read stdin */
	    goto reswitch;
	case 'x':
	    PL_doextract = TRUE;
	    s++;
	    if (*s)
		cddir = s;
	    break;
	case 0:
	    break;
	case '-':
	    if (!*++s || isSPACE(*s)) {
		argc--,argv++;
		goto switch_end;
	    }
	    /* catch use of gnu style long options */
	    if (strEQ(s, "version")) {
		s = "v";
		goto reswitch;
	    }
	    if (strEQ(s, "help")) {
		s = "h";
		goto reswitch;
	    }
	    s--;
	    /* FALL THROUGH */
	default:
	    Perl_croak(aTHX_ "Unrecognized switch: -%s  (-h will show valid options)",s);
	}
    }
  switch_end:

    if (
#ifndef SECURE_INTERNAL_GETENV
        !PL_tainting &&
#endif
	(s = PerlEnv_getenv("PERL5OPT")))
    {
	while (isSPACE(*s))
	    s++;
	if (*s == '-' && *(s+1) == 'T')
	    PL_tainting = TRUE;
	else {
	    while (s && *s) {
	        char *d;
		while (isSPACE(*s))
		    s++;
		if (*s == '-') {
		    s++;
		    if (isSPACE(*s))
			continue;
		}
		d = s;
		if (!*s)
		    break;
		if (!strchr("DIMUdmw", *s))
		    Perl_croak(aTHX_ "Illegal switch in PERL5OPT: -%c", *s);
		while (++s && *s) {
		    if (isSPACE(*s)) {
		        *s++ = '\0';
			break;
		    }
		}
		moreswitches(d);
	    }
	}
    }

    if (!scriptname)
	scriptname = argv[0];
    if (PL_e_script) {
	argc++,argv--;
	scriptname = BIT_BUCKET;	/* don't look for script or read stdin */
    }
    else if (scriptname == Nullch) {
#ifdef MSDOS
	if ( PerlLIO_isatty(PerlIO_fileno(PerlIO_stdin())) )
	    moreswitches("h");
#endif
	scriptname = "-";
    }

    init_perllib();

    open_script(scriptname,dosearch,sv,&fdscript);

    validate_suid(validarg, scriptname,fdscript);

#if defined(SIGCHLD) || defined(SIGCLD)
    {
#ifndef SIGCHLD
#  define SIGCHLD SIGCLD
#endif
	Sighandler_t sigstate = rsignal_state(SIGCHLD);
	if (sigstate == SIG_IGN) {
	    if (ckWARN(WARN_SIGNAL))
		Perl_warner(aTHX_ WARN_SIGNAL,
			    "Can't ignore signal CHLD, forcing to default");
	    (void)rsignal(SIGCHLD, (Sighandler_t)SIG_DFL);
	}
    }
#endif

#ifdef MACOS_TRADITIONAL
    if (PL_doextract || gMacPerl_AlwaysExtract) {
#else
    if (PL_doextract) {
#endif
	find_beginning();
	if (cddir && PerlDir_chdir(cddir) < 0)
	    Perl_croak(aTHX_ "Can't chdir to %s",cddir);

    }

    PL_main_cv = PL_compcv = (CV*)NEWSV(1104,0);
    sv_upgrade((SV *)PL_compcv, SVt_PVCV);
    CvUNIQUE_on(PL_compcv);

    PL_comppad = newAV();
    av_push(PL_comppad, Nullsv);
    PL_curpad = AvARRAY(PL_comppad);
    PL_comppad_name = newAV();
    PL_comppad_name_fill = 0;
    PL_min_intro_pending = 0;
    PL_padix = 0;
#ifdef USE_THREADS
    av_store(PL_comppad_name, 0, newSVpvn("@_", 2));
    PL_curpad[0] = (SV*)newAV();
    SvPADMY_on(PL_curpad[0]);	/* XXX Needed? */
    CvOWNER(PL_compcv) = 0;
    New(666, CvMUTEXP(PL_compcv), 1, perl_mutex);
    MUTEX_INIT(CvMUTEXP(PL_compcv));
#endif /* USE_THREADS */

    comppadlist = newAV();
    AvREAL_off(comppadlist);
    av_store(comppadlist, 0, (SV*)PL_comppad_name);
    av_store(comppadlist, 1, (SV*)PL_comppad);
    CvPADLIST(PL_compcv) = comppadlist;

    boot_core_UNIVERSAL();
#ifndef PERL_MICRO
    boot_core_xsutils();
#endif

    if (xsinit)
	(*xsinit)(aTHXo);	/* in case linked C routines want magical variables */
#if defined(VMS) || defined(WIN32) || defined(DJGPP) || defined(__CYGWIN__) || defined(EPOC)
    init_os_extras();
#endif

#ifdef USE_SOCKS
#   ifdef HAS_SOCKS5_INIT
    socks5_init(argv[0]);
#   else
    SOCKSinit(argv[0]);
#   endif
#endif    

    init_predump_symbols();
    /* init_postdump_symbols not currently designed to be called */
    /* more than once (ENV isn't cleared first, for example)	 */
    /* But running with -u leaves %ENV & @ARGV undefined!    XXX */
    if (!PL_do_undump)
	init_postdump_symbols(argc,argv,env);

    init_lexer();

    /* now parse the script */

    SETERRNO(0,SS$_NORMAL);
    PL_error_count = 0;
#ifdef MACOS_TRADITIONAL
    if (gMacPerl_SyntaxError = (yyparse() || PL_error_count)) {
	if (PL_minus_c)
	    Perl_croak(aTHX_ "%s had compilation errors.\n", MacPerl_MPWFileName(PL_origfilename));
	else {
	    Perl_croak(aTHX_ "Execution of %s aborted due to compilation errors.\n",
		       MacPerl_MPWFileName(PL_origfilename));
	}
    }
#else
    if (yyparse() || PL_error_count) {
	if (PL_minus_c)
	    Perl_croak(aTHX_ "%s had compilation errors.\n", PL_origfilename);
	else {
	    Perl_croak(aTHX_ "Execution of %s aborted due to compilation errors.\n",
		       PL_origfilename);
	}
    }
#endif
    CopLINE_set(PL_curcop, 0);
    PL_curstash = PL_defstash;
    PL_preprocess = FALSE;
    if (PL_e_script) {
	SvREFCNT_dec(PL_e_script);
	PL_e_script = Nullsv;
    }

    /* now that script is parsed, we can modify record separator */
    SvREFCNT_dec(PL_rs);
    PL_rs = SvREFCNT_inc(PL_nrs);
    sv_setsv(get_sv("/", TRUE), PL_rs);
    if (PL_do_undump)
	my_unexec();

    if (isWARN_ONCE) {
	SAVECOPFILE(PL_curcop);
	SAVECOPLINE(PL_curcop);
	gv_check(PL_defstash);
    }

    LEAVE;
    FREETMPS;

#ifdef MYMALLOC
    if ((s=PerlEnv_getenv("PERL_DEBUG_MSTATS")) && atoi(s) >= 2)
	dump_mstats("after compilation:");
#endif

    ENTER;
    PL_restartop = 0;
    return NULL;
}

/*
=for apidoc perl_run

Tells a Perl interpreter to run.  See L<perlembed>.

=cut
*/

int
perl_run(pTHXx)
{
    I32 oldscope;
    int ret = 0;
    dJMPENV;
#ifdef USE_THREADS
    dTHX;
#endif

    oldscope = PL_scopestack_ix;

#ifdef PERL_FLEXIBLE_EXCEPTIONS
 redo_body:
    CALLPROTECT(aTHX_ pcur_env, &ret, MEMBER_TO_FPTR(S_vrun_body), oldscope);
#else
    JMPENV_PUSH(ret);
#endif
    switch (ret) {
    case 1:
	cxstack_ix = -1;		/* start context stack again */
	goto redo_body;
    case 0:				/* normal completion */
#ifndef PERL_FLEXIBLE_EXCEPTIONS
 redo_body:
	run_body(oldscope);
#endif
	/* FALL THROUGH */
    case 2:				/* my_exit() */
	while (PL_scopestack_ix > oldscope)
	    LEAVE;
	FREETMPS;
	PL_curstash = PL_defstash;
	if (PL_endav && !PL_minus_c)
	    call_list(oldscope, PL_endav);
#ifdef MYMALLOC
	if (PerlEnv_getenv("PERL_DEBUG_MSTATS"))
	    dump_mstats("after execution:  ");
#endif
	ret = STATUS_NATIVE_EXPORT;
	break;
    case 3:
	if (PL_restartop) {
	    POPSTACK_TO(PL_mainstack);
	    goto redo_body;
	}
	PerlIO_printf(Perl_error_log, "panic: restartop\n");
	FREETMPS;
	ret = 1;
	break;
    }

    JMPENV_POP;
    return ret;
}

#ifdef PERL_FLEXIBLE_EXCEPTIONS
STATIC void *
S_vrun_body(pTHX_ va_list args)
{
    I32 oldscope = va_arg(args, I32);

    return run_body(oldscope);
}
#endif


STATIC void *
S_run_body(pTHX_ I32 oldscope)
{
    DEBUG_r(PerlIO_printf(Perl_debug_log, "%s $` $& $' support.\n",
                    PL_sawampersand ? "Enabling" : "Omitting"));

    if (!PL_restartop) {
	DEBUG_x(dump_all());
	DEBUG(PerlIO_printf(Perl_debug_log, "\nEXECUTING...\n\n"));
	DEBUG_S(PerlIO_printf(Perl_debug_log, "main thread is 0x%"UVxf"\n",
			      PTR2UV(thr)));

	if (PL_minus_c) {
#ifdef MACOS_TRADITIONAL
	    PerlIO_printf(Perl_error_log, "%s syntax OK\n", MacPerl_MPWFileName(PL_origfilename));
#else
	    PerlIO_printf(Perl_error_log, "%s syntax OK\n", PL_origfilename);
#endif
	    my_exit(0);
	}
	if (PERLDB_SINGLE && PL_DBsingle)
	    sv_setiv(PL_DBsingle, 1); 
	if (PL_initav)
	    call_list(oldscope, PL_initav);
    }

    /* do it */

    if (PL_restartop) {
	PL_op = PL_restartop;
	PL_restartop = 0;
	CALLRUNOPS(aTHX);
    }
    else if (PL_main_start) {
	CvDEPTH(PL_main_cv) = 1;
	PL_op = PL_main_start;
	CALLRUNOPS(aTHX);
    }

    my_exit(0);
    /* NOTREACHED */
    return NULL;
}

/*
=for apidoc p||get_sv

Returns the SV of the specified Perl scalar.  If C<create> is set and the
Perl variable does not exist then it will be created.  If C<create> is not
set and the variable does not exist then NULL is returned.

=cut
*/

SV*
Perl_get_sv(pTHX_ const char *name, I32 create)
{
    GV *gv;
#ifdef USE_THREADS
    if (name[1] == '\0' && !isALPHA(name[0])) {
	PADOFFSET tmp = find_threadsv(name);
    	if (tmp != NOT_IN_PAD)
	    return THREADSV(tmp);
    }
#endif /* USE_THREADS */
    gv = gv_fetchpv(name, create, SVt_PV);
    if (gv)
	return GvSV(gv);
    return Nullsv;
}

/*
=for apidoc p||get_av

Returns the AV of the specified Perl array.  If C<create> is set and the
Perl variable does not exist then it will be created.  If C<create> is not
set and the variable does not exist then NULL is returned.

=cut
*/

AV*
Perl_get_av(pTHX_ const char *name, I32 create)
{
    GV* gv = gv_fetchpv(name, create, SVt_PVAV);
    if (create)
    	return GvAVn(gv);
    if (gv)
	return GvAV(gv);
    return Nullav;
}

/*
=for apidoc p||get_hv

Returns the HV of the specified Perl hash.  If C<create> is set and the
Perl variable does not exist then it will be created.  If C<create> is not
set and the variable does not exist then NULL is returned.

=cut
*/

HV*
Perl_get_hv(pTHX_ const char *name, I32 create)
{
    GV* gv = gv_fetchpv(name, create, SVt_PVHV);
    if (create)
    	return GvHVn(gv);
    if (gv)
	return GvHV(gv);
    return Nullhv;
}

/*
=for apidoc p||get_cv

Returns the CV of the specified Perl subroutine.  If C<create> is set and
the Perl subroutine does not exist then it will be declared (which has the
same effect as saying C<sub name;>).  If C<create> is not set and the
subroutine does not exist then NULL is returned.

=cut
*/

CV*
Perl_get_cv(pTHX_ const char *name, I32 create)
{
    GV* gv = gv_fetchpv(name, create, SVt_PVCV);
    /* XXX unsafe for threads if eval_owner isn't held */
    /* XXX this is probably not what they think they're getting.
     * It has the same effect as "sub name;", i.e. just a forward
     * declaration! */
    if (create && !GvCVu(gv))
    	return newSUB(start_subparse(FALSE, 0),
		      newSVOP(OP_CONST, 0, newSVpv(name,0)),
		      Nullop,
		      Nullop);
    if (gv)
	return GvCVu(gv);
    return Nullcv;
}

/* Be sure to refetch the stack pointer after calling these routines. */

/*
=for apidoc p||call_argv

Performs a callback to the specified Perl sub.  See L<perlcall>.

=cut
*/

I32
Perl_call_argv(pTHX_ const char *sub_name, I32 flags, register char **argv)
              
          		/* See G_* flags in cop.h */
                     	/* null terminated arg list */
{
    dSP;

    PUSHMARK(SP);
    if (argv) {
	while (*argv) {
	    XPUSHs(sv_2mortal(newSVpv(*argv,0)));
	    argv++;
	}
	PUTBACK;
    }
    return call_pv(sub_name, flags);
}

/*
=for apidoc p||call_pv

Performs a callback to the specified Perl sub.  See L<perlcall>.

=cut
*/

I32
Perl_call_pv(pTHX_ const char *sub_name, I32 flags)
              		/* name of the subroutine */
          		/* See G_* flags in cop.h */
{
    return call_sv((SV*)get_cv(sub_name, TRUE), flags);
}

/*
=for apidoc p||call_method

Performs a callback to the specified Perl method.  The blessed object must
be on the stack.  See L<perlcall>.

=cut
*/

I32
Perl_call_method(pTHX_ const char *methname, I32 flags)
               		/* name of the subroutine */
          		/* See G_* flags in cop.h */
{
    return call_sv(sv_2mortal(newSVpv(methname,0)), flags | G_METHOD);
}

/* May be called with any of a CV, a GV, or an SV containing the name. */
/*
=for apidoc p||call_sv

Performs a callback to the Perl sub whose name is in the SV.  See
L<perlcall>.

=cut
*/

I32
Perl_call_sv(pTHX_ SV *sv, I32 flags)
          		/* See G_* flags in cop.h */
{
    dSP;
    LOGOP myop;		/* fake syntax tree node */
    UNOP method_op;
    I32 oldmark;
    I32 retval;
    I32 oldscope;
    bool oldcatch = CATCH_GET;
    int ret;
    OP* oldop = PL_op;
    dJMPENV;

    if (flags & G_DISCARD) {
	ENTER;
	SAVETMPS;
    }

    Zero(&myop, 1, LOGOP);
    myop.op_next = Nullop;
    if (!(flags & G_NOARGS))
	myop.op_flags |= OPf_STACKED;
    myop.op_flags |= ((flags & G_VOID) ? OPf_WANT_VOID :
		      (flags & G_ARRAY) ? OPf_WANT_LIST :
		      OPf_WANT_SCALAR);
    SAVEOP();
    PL_op = (OP*)&myop;

    EXTEND(PL_stack_sp, 1);
    *++PL_stack_sp = sv;
    oldmark = TOPMARK;
    oldscope = PL_scopestack_ix;

    if (PERLDB_SUB && PL_curstash != PL_debstash
	   /* Handle first BEGIN of -d. */
	  && (PL_DBcv || (PL_DBcv = GvCV(PL_DBsub)))
	   /* Try harder, since this may have been a sighandler, thus
	    * curstash may be meaningless. */
	  && (SvTYPE(sv) != SVt_PVCV || CvSTASH((CV*)sv) != PL_debstash)
	  && !(flags & G_NODEBUG))
	PL_op->op_private |= OPpENTERSUB_DB;

    if (flags & G_METHOD) {
	Zero(&method_op, 1, UNOP);
	method_op.op_next = PL_op;
	method_op.op_ppaddr = PL_ppaddr[OP_METHOD];
	myop.op_ppaddr = PL_ppaddr[OP_ENTERSUB];
	PL_op = (OP*)&method_op;
    }

    if (!(flags & G_EVAL)) {
	CATCH_SET(TRUE);
	call_body((OP*)&myop, FALSE);
	retval = PL_stack_sp - (PL_stack_base + oldmark);
	CATCH_SET(oldcatch);
    }
    else {
	myop.op_other = (OP*)&myop;
	PL_markstack_ptr--;
	/* we're trying to emulate pp_entertry() here */
	{
	    register PERL_CONTEXT *cx;
	    I32 gimme = GIMME_V;
	    
	    ENTER;
	    SAVETMPS;
	    
	    push_return(Nullop);
	    PUSHBLOCK(cx, (CXt_EVAL|CXp_TRYBLOCK), PL_stack_sp);
	    PUSHEVAL(cx, 0, 0);
	    PL_eval_root = PL_op;             /* Only needed so that goto works right. */
	    
	    PL_in_eval = EVAL_INEVAL;
	    if (flags & G_KEEPERR)
		PL_in_eval |= EVAL_KEEPERR;
	    else
		sv_setpv(ERRSV,"");
	}
	PL_markstack_ptr++;

#ifdef PERL_FLEXIBLE_EXCEPTIONS
 redo_body:
	CALLPROTECT(aTHX_ pcur_env, &ret, MEMBER_TO_FPTR(S_vcall_body),
		    (OP*)&myop, FALSE);
#else
	JMPENV_PUSH(ret);
#endif
	switch (ret) {
	case 0:
#ifndef PERL_FLEXIBLE_EXCEPTIONS
 redo_body:
	    call_body((OP*)&myop, FALSE);
#endif
	    retval = PL_stack_sp - (PL_stack_base + oldmark);
	    if (!(flags & G_KEEPERR))
		sv_setpv(ERRSV,"");
	    break;
	case 1:
	    STATUS_ALL_FAILURE;
	    /* FALL THROUGH */
	case 2:
	    /* my_exit() was called */
	    PL_curstash = PL_defstash;
	    FREETMPS;
	    JMPENV_POP;
	    if (PL_statusvalue && !(PL_exit_flags & PERL_EXIT_EXPECTED))
		Perl_croak(aTHX_ "Callback called exit");
	    my_exit_jump();
	    /* NOTREACHED */
	case 3:
	    if (PL_restartop) {
		PL_op = PL_restartop;
		PL_restartop = 0;
		goto redo_body;
	    }
	    PL_stack_sp = PL_stack_base + oldmark;
	    if (flags & G_ARRAY)
		retval = 0;
	    else {
		retval = 1;
		*++PL_stack_sp = &PL_sv_undef;
	    }
	    break;
	}

	if (PL_scopestack_ix > oldscope) {
	    SV **newsp;
	    PMOP *newpm;
	    I32 gimme;
	    register PERL_CONTEXT *cx;
	    I32 optype;

	    POPBLOCK(cx,newpm);
	    POPEVAL(cx);
	    pop_return();
	    PL_curpm = newpm;
	    LEAVE;
	}
	JMPENV_POP;
    }

    if (flags & G_DISCARD) {
	PL_stack_sp = PL_stack_base + oldmark;
	retval = 0;
	FREETMPS;
	LEAVE;
    }
    PL_op = oldop;
    return retval;
}

#ifdef PERL_FLEXIBLE_EXCEPTIONS
STATIC void *
S_vcall_body(pTHX_ va_list args)
{
    OP *myop = va_arg(args, OP*);
    int is_eval = va_arg(args, int);

    call_body(myop, is_eval);
    return NULL;
}
#endif

STATIC void
S_call_body(pTHX_ OP *myop, int is_eval)
{
    if (PL_op == myop) {
	if (is_eval)
	    PL_op = Perl_pp_entereval(aTHX);	/* this doesn't do a POPMARK */
	else
	    PL_op = Perl_pp_entersub(aTHX);	/* this does */
    }
    if (PL_op)
	CALLRUNOPS(aTHX);
}

/* Eval a string. The G_EVAL flag is always assumed. */

/*
=for apidoc p||eval_sv

Tells Perl to C<eval> the string in the SV.

=cut
*/

I32
Perl_eval_sv(pTHX_ SV *sv, I32 flags)
       
          		/* See G_* flags in cop.h */
{
    dSP;
    UNOP myop;		/* fake syntax tree node */
    I32 oldmark = SP - PL_stack_base;
    I32 retval;
    I32 oldscope;
    int ret;
    OP* oldop = PL_op;
    dJMPENV;

    if (flags & G_DISCARD) {
	ENTER;
	SAVETMPS;
    }

    SAVEOP();
    PL_op = (OP*)&myop;
    Zero(PL_op, 1, UNOP);
    EXTEND(PL_stack_sp, 1);
    *++PL_stack_sp = sv;
    oldscope = PL_scopestack_ix;

    if (!(flags & G_NOARGS))
	myop.op_flags = OPf_STACKED;
    myop.op_next = Nullop;
    myop.op_type = OP_ENTEREVAL;
    myop.op_flags |= ((flags & G_VOID) ? OPf_WANT_VOID :
		      (flags & G_ARRAY) ? OPf_WANT_LIST :
		      OPf_WANT_SCALAR);
    if (flags & G_KEEPERR)
	myop.op_flags |= OPf_SPECIAL;

#ifdef PERL_FLEXIBLE_EXCEPTIONS
 redo_body:
    CALLPROTECT(aTHX_ pcur_env, &ret, MEMBER_TO_FPTR(S_vcall_body),
		(OP*)&myop, TRUE);
#else
    JMPENV_PUSH(ret);
#endif
    switch (ret) {
    case 0:
#ifndef PERL_FLEXIBLE_EXCEPTIONS
 redo_body:
	call_body((OP*)&myop,TRUE);
#endif
	retval = PL_stack_sp - (PL_stack_base + oldmark);
	if (!(flags & G_KEEPERR))
	    sv_setpv(ERRSV,"");
	break;
    case 1:
	STATUS_ALL_FAILURE;
	/* FALL THROUGH */
    case 2:
	/* my_exit() was called */
	PL_curstash = PL_defstash;
	FREETMPS;
	JMPENV_POP;
	if (PL_statusvalue && !(PL_exit_flags & PERL_EXIT_EXPECTED))
	    Perl_croak(aTHX_ "Callback called exit");
	my_exit_jump();
	/* NOTREACHED */
    case 3:
	if (PL_restartop) {
	    PL_op = PL_restartop;
	    PL_restartop = 0;
	    goto redo_body;
	}
	PL_stack_sp = PL_stack_base + oldmark;
	if (flags & G_ARRAY)
	    retval = 0;
	else {
	    retval = 1;
	    *++PL_stack_sp = &PL_sv_undef;
	}
	break;
    }

    JMPENV_POP;
    if (flags & G_DISCARD) {
	PL_stack_sp = PL_stack_base + oldmark;
	retval = 0;
	FREETMPS;
	LEAVE;
    }
    PL_op = oldop;
    return retval;
}

/*
=for apidoc p||eval_pv

Tells Perl to C<eval> the given string and return an SV* result.

=cut
*/

SV*
Perl_eval_pv(pTHX_ const char *p, I32 croak_on_error)
{
    dSP;
    SV* sv = newSVpv(p, 0);

    eval_sv(sv, G_SCALAR);
    SvREFCNT_dec(sv);

    SPAGAIN;
    sv = POPs;
    PUTBACK;

    if (croak_on_error && SvTRUE(ERRSV)) {
	STRLEN n_a;
	Perl_croak(aTHX_ SvPVx(ERRSV, n_a));
    }

    return sv;
}

/* Require a module. */

/*
=for apidoc p||require_pv

Tells Perl to C<require> a module.

=cut
*/

void
Perl_require_pv(pTHX_ const char *pv)
{
    SV* sv;
    dSP;
    PUSHSTACKi(PERLSI_REQUIRE);
    PUTBACK;
    sv = sv_newmortal();
    sv_setpv(sv, "require '");
    sv_catpv(sv, pv);
    sv_catpv(sv, "'");
    eval_sv(sv, G_DISCARD);
    SPAGAIN;
    POPSTACK;
}

void
Perl_magicname(pTHX_ char *sym, char *name, I32 namlen)
{
    register GV *gv;

    if ((gv = gv_fetchpv(sym,TRUE, SVt_PV)))
	sv_magic(GvSV(gv), (SV*)gv, 0, name, namlen);
}

STATIC void
S_usage(pTHX_ char *name)		/* XXX move this out into a module ? */
{
    /* This message really ought to be max 23 lines.
     * Removed -h because the user already knows that opton. Others? */

    static char *usage_msg[] = {
"-0[octal]       specify record separator (\\0, if no argument)",
"-a              autosplit mode with -n or -p (splits $_ into @F)",
"-C              enable native wide character system interfaces",
"-c              check syntax only (runs BEGIN and CHECK blocks)",
"-d[:debugger]   run program under debugger",
"-D[number/list] set debugging flags (argument is a bit mask or alphabets)",
"-e 'command'    one line of program (several -e's allowed, omit programfile)",
"-F/pattern/     split() pattern for -a switch (//'s are optional)",
"-i[extension]   edit <> files in place (makes backup if extension supplied)",
"-Idirectory     specify @INC/#include directory (several -I's allowed)",
"-l[octal]       enable line ending processing, specifies line terminator",
"-[mM][-]module  execute `use/no module...' before executing program",
"-n              assume 'while (<>) { ... }' loop around program",
"-p              assume loop like -n but print line also, like sed",
"-P              run program through C preprocessor before compilation",
"-s              enable rudimentary parsing for switches after programfile",
"-S              look for programfile using PATH environment variable",
"-T              enable tainting checks",
"-u              dump core after parsing program",
"-U              allow unsafe operations",
"-v              print version, subversion (includes VERY IMPORTANT perl info)",
"-V[:variable]   print configuration summary (or a single Config.pm variable)",
"-w              enable many useful warnings (RECOMMENDED)",
"-W              enable all warnings",
"-X              disable all warnings",
"-x[directory]   strip off text before #!perl line and perhaps cd to directory",
"\n",
NULL
};
    char **p = usage_msg;

    PerlIO_printf(PerlIO_stdout(),
		  "\nUsage: %s [switches] [--] [programfile] [arguments]",
		  name);
    while (*p)
	PerlIO_printf(PerlIO_stdout(), "\n  %s", *p++);
}

/* This routine handles any switches that can be given during run */

char *
Perl_moreswitches(pTHX_ char *s)
{
    STRLEN numlen;
    U32 rschar;

    switch (*s) {
    case '0':
    {
	numlen = 0;			/* disallow underscores */
	rschar = (U32)scan_oct(s, 4, &numlen);
	SvREFCNT_dec(PL_nrs);
	if (rschar & ~((U8)~0))
	    PL_nrs = &PL_sv_undef;
	else if (!rschar && numlen >= 2)
	    PL_nrs = newSVpvn("", 0);
	else {
	    char ch = rschar;
	    PL_nrs = newSVpvn(&ch, 1);
	}
	return s + numlen;
    }
    case 'C':
	PL_widesyscalls = TRUE;
	s++;
	return s;
    case 'F':
	PL_minus_F = TRUE;
	PL_splitstr = savepv(s + 1);
	s += strlen(s);
	return s;
    case 'a':
	PL_minus_a = TRUE;
	s++;
	return s;
    case 'c':
	PL_minus_c = TRUE;
	s++;
	return s;
    case 'd':
	forbid_setid("-d");
	s++;
	/* The following permits -d:Mod to accepts arguments following an =
	   in the fashion that -MSome::Mod does. */
	if (*s == ':' || *s == '=') {
	    char *start;
	    SV *sv;
	    sv = newSVpv("use Devel::", 0);
	    start = ++s;
	    /* We now allow -d:Module=Foo,Bar */
	    while(isALNUM(*s) || *s==':') ++s;
	    if (*s != '=')
		sv_catpv(sv, start);
	    else {
		sv_catpvn(sv, start, s-start);
		sv_catpv(sv, " split(/,/,q{");
		sv_catpv(sv, ++s);
		sv_catpv(sv,    "})");
	    }
	    s += strlen(s);
	    my_setenv("PERL5DB", SvPV(sv, PL_na));
	}
	if (!PL_perldb) {
	    PL_perldb = PERLDB_ALL;
	    init_debugger();
	}
	return s;
    case 'D':
    {	
#ifdef DEBUGGING
	forbid_setid("-D");
	if (isALPHA(s[1])) {
	    static char debopts[] = "psltocPmfrxuLHXDST";
	    char *d;

	    for (s++; *s && (d = strchr(debopts,*s)); s++)
		PL_debug |= 1 << (d - debopts);
	}
	else {
	    PL_debug = atoi(s+1);
	    for (s++; isDIGIT(*s); s++) ;
	}
	PL_debug |= 0x80000000;
#else
	if (ckWARN_d(WARN_DEBUGGING))
	    Perl_warner(aTHX_ WARN_DEBUGGING,
	           "Recompile perl with -DDEBUGGING to use -D switch\n");
	for (s++; isALNUM(*s); s++) ;
#endif
	/*SUPPRESS 530*/
	return s;
    }	
    case 'h':
	usage(PL_origargv[0]);    
	PerlProc_exit(0);
    case 'i':
	if (PL_inplace)
	    Safefree(PL_inplace);
	PL_inplace = savepv(s+1);
	/*SUPPRESS 530*/
	for (s = PL_inplace; *s && !isSPACE(*s); s++) ;
	if (*s) {
	    *s++ = '\0';
	    if (*s == '-')	/* Additional switches on #! line. */
	        s++;
	}
	return s;
    case 'I':	/* -I handled both here and in parse_perl() */
	forbid_setid("-I");
	++s;
	while (*s && isSPACE(*s))
	    ++s;
	if (*s) {
	    char *e, *p;
	    p = s;
	    /* ignore trailing spaces (possibly followed by other switches) */
	    do {
		for (e = p; *e && !isSPACE(*e); e++) ;
		p = e;
		while (isSPACE(*p))
		    p++;
	    } while (*p && *p != '-');
	    e = savepvn(s, e-s);
	    incpush(e, TRUE, TRUE);
	    Safefree(e);
	    s = p;
	    if (*s == '-')
		s++;
	}
	else
	    Perl_croak(aTHX_ "No directory specified for -I");
	return s;
    case 'l':
	PL_minus_l = TRUE;
	s++;
	if (PL_ors)
	    Safefree(PL_ors);
	if (isDIGIT(*s)) {
	    PL_ors = savepv("\n");
	    PL_orslen = 1;
	    numlen = 0;			/* disallow underscores */
	    *PL_ors = (char)scan_oct(s, 3 + (*s == '0'), &numlen);
	    s += numlen;
	}
	else {
	    if (RsPARA(PL_nrs)) {
		PL_ors = "\n\n";
		PL_orslen = 2;
	    }
	    else
		PL_ors = SvPV(PL_nrs, PL_orslen);
	    PL_ors = savepvn(PL_ors, PL_orslen);
	}
	return s;
    case 'M':
	forbid_setid("-M");	/* XXX ? */
	/* FALL THROUGH */
    case 'm':
	forbid_setid("-m");	/* XXX ? */
	if (*++s) {
	    char *start;
	    SV *sv;
	    char *use = "use ";
	    /* -M-foo == 'no foo'	*/
	    if (*s == '-') { use = "no "; ++s; }
	    sv = newSVpv(use,0);
	    start = s;
	    /* We allow -M'Module qw(Foo Bar)'	*/
	    while(isALNUM(*s) || *s==':') ++s;
	    if (*s != '=') {
		sv_catpv(sv, start);
		if (*(start-1) == 'm') {
		    if (*s != '\0')
			Perl_croak(aTHX_ "Can't use '%c' after -mname", *s);
		    sv_catpv( sv, " ()");
		}
	    } else {
                if (s == start)
                    Perl_croak(aTHX_ "Module name required with -%c option",
			       s[-1]);
		sv_catpvn(sv, start, s-start);
		sv_catpv(sv, " split(/,/,q{");
		sv_catpv(sv, ++s);
		sv_catpv(sv,    "})");
	    }
	    s += strlen(s);
	    if (!PL_preambleav)
		PL_preambleav = newAV();
	    av_push(PL_preambleav, sv);
	}
	else
	    Perl_croak(aTHX_ "No space allowed after -%c", *(s-1));
	return s;
    case 'n':
	PL_minus_n = TRUE;
	s++;
	return s;
    case 'p':
	PL_minus_p = TRUE;
	s++;
	return s;
    case 's':
	forbid_setid("-s");
	PL_doswitches = TRUE;
	s++;
	return s;
    case 'T':
	if (!PL_tainting)
	    Perl_croak(aTHX_ "Too late for \"-T\" option");
	s++;
	return s;
    case 'u':
#ifdef MACOS_TRADITIONAL
	Perl_croak(aTHX_ "Believe me, you don't want to use \"-u\" on a Macintosh");
#endif
	PL_do_undump = TRUE;
	s++;
	return s;
    case 'U':
	PL_unsafe = TRUE;
	s++;
	return s;
    case 'v':
	PerlIO_printf(PerlIO_stdout(),
		      Perl_form(aTHX_ "\nThis is perl, v%"VDf" built for %s",
				PL_patchlevel, ARCHNAME));
#if defined(LOCAL_PATCH_COUNT)
	if (LOCAL_PATCH_COUNT > 0)
	    PerlIO_printf(PerlIO_stdout(),
			  "\n(with %d registered patch%s, "
			  "see perl -V for more detail)",
			  (int)LOCAL_PATCH_COUNT,
			  (LOCAL_PATCH_COUNT!=1) ? "es" : "");
#endif

	PerlIO_printf(PerlIO_stdout(),
		      "\n\nCopyright 1987-2001, Larry Wall\n");
#ifdef MACOS_TRADITIONAL
	PerlIO_printf(PerlIO_stdout(),
		      "\nMac OS port Copyright (c) 1991-2001, Matthias Neeracher\n");
#endif
#ifdef MSDOS
	PerlIO_printf(PerlIO_stdout(),
		      "\nMS-DOS port Copyright (c) 1989, 1990, Diomidis Spinellis\n");
#endif
#ifdef DJGPP
	PerlIO_printf(PerlIO_stdout(),
		      "djgpp v2 port (jpl5003c) by Hirofumi Watanabe, 1996\n"
		      "djgpp v2 port (perl5004+) by Laszlo Molnar, 1997-1999\n");
#endif
#ifdef OS2
	PerlIO_printf(PerlIO_stdout(),
		      "\n\nOS/2 port Copyright (c) 1990, 1991, Raymond Chen, Kai Uwe Rommel\n"
		      "Version 5 port Copyright (c) 1994-1999, Andreas Kaiser, Ilya Zakharevich\n");
#endif
#ifdef atarist
	PerlIO_printf(PerlIO_stdout(),
		      "atariST series port, ++jrb  bammi@cadence.com\n");
#endif
#ifdef __BEOS__
	PerlIO_printf(PerlIO_stdout(),
		      "BeOS port Copyright Tom Spindler, 1997-1999\n");
#endif
#ifdef MPE
	PerlIO_printf(PerlIO_stdout(),
		      "MPE/iX port Copyright by Mark Klein and Mark Bixby, 1996-1999\n");
#endif
#ifdef OEMVS
	PerlIO_printf(PerlIO_stdout(),
		      "MVS (OS390) port by Mortice Kern Systems, 1997-1999\n");
#endif
#ifdef __VOS__
	PerlIO_printf(PerlIO_stdout(),
		      "Stratus VOS port by Paul_Green@stratus.com, 1997-1999\n");
#endif
#ifdef __OPEN_VM
	PerlIO_printf(PerlIO_stdout(),
		      "VM/ESA port by Neale Ferguson, 1998-1999\n");
#endif
#ifdef POSIX_BC
	PerlIO_printf(PerlIO_stdout(),
		      "BS2000 (POSIX) port by Start Amadeus GmbH, 1998-1999\n");
#endif
#ifdef __MINT__
	PerlIO_printf(PerlIO_stdout(),
		      "MiNT port by Guido Flohr, 1997-1999\n");
#endif
#ifdef EPOC
	PerlIO_printf(PerlIO_stdout(),
		      "EPOC port by Olaf Flebbe, 1999-2000\n");
#endif
#ifdef BINARY_BUILD_NOTICE
	BINARY_BUILD_NOTICE;
#endif
	PerlIO_printf(PerlIO_stdout(),
		      "\n\
Perl may be copied only under the terms of either the Artistic License or the\n\
GNU General Public License, which may be found in the Perl 5 source kit.\n\n\
Complete documentation for Perl, including FAQ lists, should be found on\n\
this system using `man perl' or `perldoc perl'.  If you have access to the\n\
Internet, point your browser at http://www.perl.com/, the Perl Home Page.\n\n");
	PerlProc_exit(0);
    case 'w':
	if (! (PL_dowarn & G_WARN_ALL_MASK))
	    PL_dowarn |= G_WARN_ON; 
	s++;
	return s;
    case 'W':
	PL_dowarn = G_WARN_ALL_ON|G_WARN_ON; 
	PL_compiling.cop_warnings = pWARN_ALL ;
	s++;
	return s;
    case 'X':
	PL_dowarn = G_WARN_ALL_OFF; 
	PL_compiling.cop_warnings = pWARN_NONE ;
	s++;
	return s;
    case '*':
    case ' ':
	if (s[1] == '-')	/* Additional switches on #! line. */
	    return s+2;
	break;
    case '-':
    case 0:
#if defined(WIN32) || !defined(PERL_STRICT_CR)
    case '\r':
#endif
    case '\n':
    case '\t':
	break;
#ifdef ALTERNATE_SHEBANG
    case 'S':			/* OS/2 needs -S on "extproc" line. */
	break;
#endif
    case 'P':
	if (PL_preprocess)
	    return s+1;
	/* FALL THROUGH */
    default:
	Perl_croak(aTHX_ "Can't emulate -%.1s on #! line",s);
    }
    return Nullch;
}

/* compliments of Tom Christiansen */

/* unexec() can be found in the Gnu emacs distribution */
/* Known to work with -DUNEXEC and using unexelf.c from GNU emacs-20.2 */

void
Perl_my_unexec(pTHX)
{
#ifdef UNEXEC
    SV*    prog;
    SV*    file;
    int    status = 1;
    extern int etext;

    prog = newSVpv(BIN_EXP, 0);
    sv_catpv(prog, "/perl");
    file = newSVpv(PL_origfilename, 0);
    sv_catpv(file, ".perldump");

    unexec(SvPVX(file), SvPVX(prog), &etext, sbrk(0), 0);
    /* unexec prints msg to stderr in case of failure */
    PerlProc_exit(status);
#else
#  ifdef VMS
#    include <lib$routines.h>
     lib$signal(SS$_DEBUG);  /* ssdef.h #included from vmsish.h */
#  else
    ABORT();		/* for use with undump */
#  endif
#endif
}

/* initialize curinterp */
STATIC void
S_init_interp(pTHX)
{

#ifdef PERL_OBJECT		/* XXX kludge */
#define I_REINIT \
  STMT_START {				\
    PL_chopset		= " \n-";	\
    PL_copline		= NOLINE;	\
    PL_curcop		= &PL_compiling;\
    PL_curcopdb		= NULL;		\
    PL_dbargs		= 0;		\
    PL_dumpindent	= 4;		\
    PL_laststatval	= -1;		\
    PL_laststype	= OP_STAT;	\
    PL_maxscream	= -1;		\
    PL_maxsysfd		= MAXSYSFD;	\
    PL_statname		= Nullsv;	\
    PL_tmps_floor	= -1;		\
    PL_tmps_ix		= -1;		\
    PL_op_mask		= NULL;		\
    PL_laststatval	= -1;		\
    PL_laststype	= OP_STAT;	\
    PL_mess_sv		= Nullsv;	\
    PL_splitstr		= " ";		\
    PL_generation	= 100;		\
    PL_exitlist		= NULL;		\
    PL_exitlistlen	= 0;		\
    PL_regindent	= 0;		\
    PL_in_clean_objs	= FALSE;	\
    PL_in_clean_all	= FALSE;	\
    PL_profiledata	= NULL;		\
    PL_rsfp		= Nullfp;	\
    PL_rsfp_filters	= Nullav;	\
    PL_dirty		= FALSE;	\
  } STMT_END
    I_REINIT;
#else
#  ifdef MULTIPLICITY
#    define PERLVAR(var,type)
#    define PERLVARA(var,n,type)
#    if defined(PERL_IMPLICIT_CONTEXT)
#      if defined(USE_THREADS)
#        define PERLVARI(var,type,init)		PERL_GET_INTERP->var = init;
#        define PERLVARIC(var,type,init)	PERL_GET_INTERP->var = init;
#      else /* !USE_THREADS */
#        define PERLVARI(var,type,init)		aTHX->var = init;
#        define PERLVARIC(var,type,init)	aTHX->var = init;
#      endif /* USE_THREADS */
#    else
#      define PERLVARI(var,type,init)	PERL_GET_INTERP->var = init;
#      define PERLVARIC(var,type,init)	PERL_GET_INTERP->var = init;
#    endif
#    include "intrpvar.h"
#    ifndef USE_THREADS
#      include "thrdvar.h"
#    endif
#    undef PERLVAR
#    undef PERLVARA
#    undef PERLVARI
#    undef PERLVARIC
#  else
#    define PERLVAR(var,type)
#    define PERLVARA(var,n,type)
#    define PERLVARI(var,type,init)	PL_##var = init;
#    define PERLVARIC(var,type,init)	PL_##var = init;
#    include "intrpvar.h"
#    ifndef USE_THREADS
#      include "thrdvar.h"
#    endif
#    undef PERLVAR
#    undef PERLVARA
#    undef PERLVARI
#    undef PERLVARIC
#  endif
#endif

}

STATIC void
S_init_main_stash(pTHX)
{
    GV *gv;

    /* Note that strtab is a rather special HV.  Assumptions are made
       about not iterating on it, and not adding tie magic to it.
       It is properly deallocated in perl_destruct() */
    PL_strtab = newHV();
#ifdef USE_THREADS
    MUTEX_INIT(&PL_strtab_mutex);
#endif
    HvSHAREKEYS_off(PL_strtab);			/* mandatory */
    hv_ksplit(PL_strtab, 512);
    
    PL_curstash = PL_defstash = newHV();
    PL_curstname = newSVpvn("main",4);
    gv = gv_fetchpv("main::",TRUE, SVt_PVHV);
    SvREFCNT_dec(GvHV(gv));
    GvHV(gv) = (HV*)SvREFCNT_inc(PL_defstash);
    SvREADONLY_on(gv);
    HvNAME(PL_defstash) = savepv("main");
    PL_incgv = gv_HVadd(gv_AVadd(gv_fetchpv("INC",TRUE, SVt_PVAV)));
    GvMULTI_on(PL_incgv);
    PL_hintgv = gv_fetchpv("\010",TRUE, SVt_PV); /* ^H */
    GvMULTI_on(PL_hintgv);
    PL_defgv = gv_fetchpv("_",TRUE, SVt_PVAV);
    PL_errgv = gv_HVadd(gv_fetchpv("@", TRUE, SVt_PV));
    GvMULTI_on(PL_errgv);
    PL_replgv = gv_fetchpv("\022", TRUE, SVt_PV); /* ^R */
    GvMULTI_on(PL_replgv);
    (void)Perl_form(aTHX_ "%240s","");	/* Preallocate temp - for immediate signals. */
    sv_grow(ERRSV, 240);	/* Preallocate - for immediate signals. */
    sv_setpvn(ERRSV, "", 0);
    PL_curstash = PL_defstash;
    CopSTASH_set(&PL_compiling, PL_defstash);
    PL_debstash = GvHV(gv_fetchpv("DB::", GV_ADDMULTI, SVt_PVHV));
    PL_globalstash = GvHV(gv_fetchpv("CORE::GLOBAL::", GV_ADDMULTI, SVt_PVHV));
    PL_nullstash = GvHV(gv_fetchpv("<none>::", GV_ADDMULTI, SVt_PVHV));
    /* We must init $/ before switches are processed. */
    sv_setpvn(get_sv("/", TRUE), "\n", 1);
}

STATIC void
S_open_script(pTHX_ char *scriptname, bool dosearch, SV *sv, int *fdscript)
{
    *fdscript = -1;

    if (PL_e_script) {
	PL_origfilename = savepv("-e");
    }
    else {
	/* if find_script() returns, it returns a malloc()-ed value */
	PL_origfilename = scriptname = find_script(scriptname, dosearch, NULL, 1);

	if (strnEQ(scriptname, "/dev/fd/", 8) && isDIGIT(scriptname[8]) ) {
	    char *s = scriptname + 8;
	    *fdscript = atoi(s);
	    while (isDIGIT(*s))
		s++;
	    if (*s) {
		scriptname = savepv(s + 1);
		Safefree(PL_origfilename);
		PL_origfilename = scriptname;
	    }
	}
    }

#ifdef USE_ITHREADS
    Safefree(CopFILE(PL_curcop));
#else
    SvREFCNT_dec(CopFILEGV(PL_curcop));
#endif
    CopFILE_set(PL_curcop, PL_origfilename);
    if (strEQ(PL_origfilename,"-"))
	scriptname = "";
    if (*fdscript >= 0) {
	PL_rsfp = PerlIO_fdopen(*fdscript,PERL_SCRIPT_MODE);
#if defined(HAS_FCNTL) && defined(F_SETFD)
	if (PL_rsfp)
	    fcntl(PerlIO_fileno(PL_rsfp),F_SETFD,1);  /* ensure close-on-exec */
#endif
    }
    else if (PL_preprocess) {
	char *cpp_cfg = CPPSTDIN;
	SV *cpp = newSVpvn("",0);
	SV *cmd = NEWSV(0,0);

	if (strEQ(cpp_cfg, "cppstdin"))
	    Perl_sv_catpvf(aTHX_ cpp, "%s/", BIN_EXP);
	sv_catpv(cpp, cpp_cfg);

	sv_catpvn(sv, "-I", 2);
	sv_catpv(sv,PRIVLIB_EXP);

#if defined(MSDOS) || defined(WIN32)
	Perl_sv_setpvf(aTHX_ cmd, "\
sed %s -e \"/^[^#]/b\" \
 -e \"/^#[ 	]*include[ 	]/b\" \
 -e \"/^#[ 	]*define[ 	]/b\" \
 -e \"/^#[ 	]*if[ 	]/b\" \
 -e \"/^#[ 	]*ifdef[ 	]/b\" \
 -e \"/^#[ 	]*ifndef[ 	]/b\" \
 -e \"/^#[ 	]*else/b\" \
 -e \"/^#[ 	]*elif[ 	]/b\" \
 -e \"/^#[ 	]*undef[ 	]/b\" \
 -e \"/^#[ 	]*endif/b\" \
 -e \"s/^#.*//\" \
 %s | %"SVf" -C %"SVf" %s",
	  (PL_doextract ? "-e \"1,/^#/d\n\"" : ""),
#else
#  ifdef __OPEN_VM
	Perl_sv_setpvf(aTHX_ cmd, "\
%s %s -e '/^[^#]/b' \
 -e '/^#[ 	]*include[ 	]/b' \
 -e '/^#[ 	]*define[ 	]/b' \
 -e '/^#[ 	]*if[ 	]/b' \
 -e '/^#[ 	]*ifdef[ 	]/b' \
 -e '/^#[ 	]*ifndef[ 	]/b' \
 -e '/^#[ 	]*else/b' \
 -e '/^#[ 	]*elif[ 	]/b' \
 -e '/^#[ 	]*undef[ 	]/b' \
 -e '/^#[ 	]*endif/b' \
 -e 's/^[ 	]*#.*//' \
 %s | %"SVf" %"SVf" %s",
#  else
	Perl_sv_setpvf(aTHX_ cmd, "\
%s %s -e '/^[^#]/b' \
 -e '/^#[ 	]*include[ 	]/b' \
 -e '/^#[ 	]*define[ 	]/b' \
 -e '/^#[ 	]*if[ 	]/b' \
 -e '/^#[ 	]*ifdef[ 	]/b' \
 -e '/^#[ 	]*ifndef[ 	]/b' \
 -e '/^#[ 	]*else/b' \
 -e '/^#[ 	]*elif[ 	]/b' \
 -e '/^#[ 	]*undef[ 	]/b' \
 -e '/^#[ 	]*endif/b' \
 -e 's/^[ 	]*#.*//' \
 %s | %"SVf" -C %"SVf" %s",
#  endif
#ifdef LOC_SED
	  LOC_SED,
#else
	  "sed",
#endif
	  (PL_doextract ? "-e '1,/^#/d\n'" : ""),
#endif
	  scriptname, cpp, sv, CPPMINUS);
	PL_doextract = FALSE;
#ifdef IAMSUID				/* actually, this is caught earlier */
	if (PL_euid != PL_uid && !PL_euid) {	/* if running suidperl */
#ifdef HAS_SETEUID
	    (void)seteuid(PL_uid);		/* musn't stay setuid root */
#else
#ifdef HAS_SETREUID
	    (void)setreuid((Uid_t)-1, PL_uid);
#else
#ifdef HAS_SETRESUID
	    (void)setresuid((Uid_t)-1, PL_uid, (Uid_t)-1);
#else
	    PerlProc_setuid(PL_uid);
#endif
#endif
#endif
	    if (PerlProc_geteuid() != PL_uid)
		Perl_croak(aTHX_ "Can't do seteuid!\n");
	}
#endif /* IAMSUID */
	PL_rsfp = PerlProc_popen(SvPVX(cmd), "r");
	SvREFCNT_dec(cmd);
	SvREFCNT_dec(cpp);
    }
    else if (!*scriptname) {
	forbid_setid("program input from stdin");
	PL_rsfp = PerlIO_stdin();
    }
    else {
	PL_rsfp = PerlIO_open(scriptname,PERL_SCRIPT_MODE);
#if defined(HAS_FCNTL) && defined(F_SETFD)
	if (PL_rsfp)
	    fcntl(PerlIO_fileno(PL_rsfp),F_SETFD,1);  /* ensure close-on-exec */
#endif
    }
    if (!PL_rsfp) {
#ifdef DOSUID
#ifndef IAMSUID		/* in case script is not readable before setuid */
	if (PL_euid &&
	    PerlLIO_stat(CopFILE(PL_curcop),&PL_statbuf) >= 0 &&
	    PL_statbuf.st_mode & (S_ISUID|S_ISGID))
	{
	    /* try again */
	    PerlProc_execv(Perl_form(aTHX_ "%s/sperl"PERL_FS_VER_FMT, BIN_EXP,
				     (int)PERL_REVISION, (int)PERL_VERSION,
				     (int)PERL_SUBVERSION), PL_origargv);
	    Perl_croak(aTHX_ "Can't do setuid\n");
	}
#endif
#endif
	Perl_croak(aTHX_ "Can't open perl script \"%s\": %s\n",
		   CopFILE(PL_curcop), Strerror(errno));
    }
}

/* Mention
 * I_SYSSTATVFS	HAS_FSTATVFS
 * I_SYSMOUNT
 * I_STATFS	HAS_FSTATFS	HAS_GETFSSTAT
 * I_MNTENT	HAS_GETMNTENT	HAS_HASMNTOPT
 * here so that metaconfig picks them up. */

#ifdef IAMSUID
STATIC int
S_fd_on_nosuid_fs(pTHX_ int fd)
{
    int check_okay = 0; /* able to do all the required sys/libcalls */
    int on_nosuid  = 0; /* the fd is on a nosuid fs */
/*
 * Preferred order: fstatvfs(), fstatfs(), ustat()+getmnt(), getmntent().
 * fstatvfs() is UNIX98.
 * fstatfs() is 4.3 BSD.
 * ustat()+getmnt() is pre-4.3 BSD.
 * getmntent() is O(number-of-mounted-filesystems) and can hang on
 * an irrelevant filesystem while trying to reach the right one.
 */

#undef FD_ON_NOSUID_CHECK_OKAY  /* found the syscalls to do the check? */

#   if !defined(FD_ON_NOSUID_CHECK_OKAY) && \
        defined(HAS_FSTATVFS)
#   define FD_ON_NOSUID_CHECK_OKAY
    struct statvfs stfs;

    check_okay = fstatvfs(fd, &stfs) == 0;
    on_nosuid  = check_okay && (stfs.f_flag  & ST_NOSUID);
#   endif /* fstatvfs */
 
#   if !defined(FD_ON_NOSUID_CHECK_OKAY) && \
        defined(PERL_MOUNT_NOSUID)	&& \
        defined(HAS_FSTATFS) 		&& \
        defined(HAS_STRUCT_STATFS)	&& \
        defined(HAS_STRUCT_STATFS_F_FLAGS)
#   define FD_ON_NOSUID_CHECK_OKAY
    struct statfs  stfs;

    check_okay = fstatfs(fd, &stfs)  == 0;
    on_nosuid  = check_okay && (stfs.f_flags & PERL_MOUNT_NOSUID);
#   endif /* fstatfs */

#   if !defined(FD_ON_NOSUID_CHECK_OKAY) && \
        defined(PERL_MOUNT_NOSUID)	&& \
        defined(HAS_FSTAT)		&& \
        defined(HAS_USTAT)		&& \
        defined(HAS_GETMNT)		&& \
        defined(HAS_STRUCT_FS_DATA)	&& \
        defined(NOSTAT_ONE)
#   define FD_ON_NOSUID_CHECK_OKAY
    struct stat fdst;

    if (fstat(fd, &fdst) == 0) {
        struct ustat us;
        if (ustat(fdst.st_dev, &us) == 0) {
            struct fs_data fsd;
            /* NOSTAT_ONE here because we're not examining fields which
             * vary between that case and STAT_ONE. */
            if (getmnt((int*)0, &fsd, (int)0, NOSTAT_ONE, us.f_fname) == 0) {
                size_t cmplen = sizeof(us.f_fname);
                if (sizeof(fsd.fd_req.path) < cmplen)
                    cmplen = sizeof(fsd.fd_req.path);
                if (strnEQ(fsd.fd_req.path, us.f_fname, cmplen) &&
                    fdst.st_dev == fsd.fd_req.dev) {
                        check_okay = 1;
                        on_nosuid = fsd.fd_req.flags & PERL_MOUNT_NOSUID;
                    }
                }
            }
        }
    }
#   endif /* fstat+ustat+getmnt */

#   if !defined(FD_ON_NOSUID_CHECK_OKAY) && \
        defined(HAS_GETMNTENT)		&& \
        defined(HAS_HASMNTOPT)		&& \
        defined(MNTOPT_NOSUID)
#   define FD_ON_NOSUID_CHECK_OKAY
    FILE                *mtab = fopen("/etc/mtab", "r");
    struct mntent       *entry;
    struct stat         stb, fsb;

    if (mtab && (fstat(fd, &stb) == 0)) {
        while (entry = getmntent(mtab)) {
            if (stat(entry->mnt_dir, &fsb) == 0
                && fsb.st_dev == stb.st_dev)
            {
                /* found the filesystem */
                check_okay = 1;
                if (hasmntopt(entry, MNTOPT_NOSUID))
                    on_nosuid = 1;
                break;
            } /* A single fs may well fail its stat(). */
        }
    }
    if (mtab)
        fclose(mtab);
#   endif /* getmntent+hasmntopt */

    if (!check_okay) 
	Perl_croak(aTHX_ "Can't check filesystem of script \"%s\" for nosuid", PL_origfilename);
    return on_nosuid;
}
#endif /* IAMSUID */

STATIC void
S_validate_suid(pTHX_ char *validarg, char *scriptname, int fdscript)
{
#ifdef IAMSUID
    int which;
#endif

    /* do we need to emulate setuid on scripts? */

    /* This code is for those BSD systems that have setuid #! scripts disabled
     * in the kernel because of a security problem.  Merely defining DOSUID
     * in perl will not fix that problem, but if you have disabled setuid
     * scripts in the kernel, this will attempt to emulate setuid and setgid
     * on scripts that have those now-otherwise-useless bits set.  The setuid
     * root version must be called suidperl or sperlN.NNN.  If regular perl
     * discovers that it has opened a setuid script, it calls suidperl with
     * the same argv that it had.  If suidperl finds that the script it has
     * just opened is NOT setuid root, it sets the effective uid back to the
     * uid.  We don't just make perl setuid root because that loses the
     * effective uid we had before invoking perl, if it was different from the
     * uid.
     *
     * DOSUID must be defined in both perl and suidperl, and IAMSUID must
     * be defined in suidperl only.  suidperl must be setuid root.  The
     * Configure script will set this up for you if you want it.
     */

#ifdef DOSUID
    char *s, *s2;

    if (PerlLIO_fstat(PerlIO_fileno(PL_rsfp),&PL_statbuf) < 0)	/* normal stat is insecure */
	Perl_croak(aTHX_ "Can't stat script \"%s\"",PL_origfilename);
    if (fdscript < 0 && PL_statbuf.st_mode & (S_ISUID|S_ISGID)) {
	I32 len;
	STRLEN n_a;

#ifdef IAMSUID
#ifndef HAS_SETREUID
	/* On this access check to make sure the directories are readable,
	 * there is actually a small window that the user could use to make
	 * filename point to an accessible directory.  So there is a faint
	 * chance that someone could execute a setuid script down in a
	 * non-accessible directory.  I don't know what to do about that.
	 * But I don't think it's too important.  The manual lies when
	 * it says access() is useful in setuid programs.
	 */
	if (PerlLIO_access(CopFILE(PL_curcop),1)) /*double check*/
	    Perl_croak(aTHX_ "Permission denied");
#else
	/* If we can swap euid and uid, then we can determine access rights
	 * with a simple stat of the file, and then compare device and
	 * inode to make sure we did stat() on the same file we opened.
	 * Then we just have to make sure he or she can execute it.
	 */
	{
	    struct stat tmpstatbuf;

	    if (
#ifdef HAS_SETREUID
		setreuid(PL_euid,PL_uid) < 0
#else
# if HAS_SETRESUID
		setresuid(PL_euid,PL_uid,(Uid_t)-1) < 0
# endif
#endif
		|| PerlProc_getuid() != PL_euid || PerlProc_geteuid() != PL_uid)
		Perl_croak(aTHX_ "Can't swap uid and euid");	/* really paranoid */
	    if (PerlLIO_stat(CopFILE(PL_curcop),&tmpstatbuf) < 0)
		Perl_croak(aTHX_ "Permission denied");	/* testing full pathname here */
#if defined(IAMSUID) && !defined(NO_NOSUID_CHECK)
	    if (fd_on_nosuid_fs(PerlIO_fileno(PL_rsfp)))
		Perl_croak(aTHX_ "Permission denied");
#endif
	    if (tmpstatbuf.st_dev != PL_statbuf.st_dev ||
		tmpstatbuf.st_ino != PL_statbuf.st_ino) {
		(void)PerlIO_close(PL_rsfp);
		Perl_croak(aTHX_ "Permission denied\n");
	    }
	    if (
#ifdef HAS_SETREUID
              setreuid(PL_uid,PL_euid) < 0
#else
# if defined(HAS_SETRESUID)
              setresuid(PL_uid,PL_euid,(Uid_t)-1) < 0
# endif
#endif
              || PerlProc_getuid() != PL_uid || PerlProc_geteuid() != PL_euid)
		Perl_croak(aTHX_ "Can't reswap uid and euid");
	    if (!cando(S_IXUSR,FALSE,&PL_statbuf))		/* can real uid exec? */
		Perl_croak(aTHX_ "Permission denied\n");
	}
#endif /* HAS_SETREUID */
#endif /* IAMSUID */

	if (!S_ISREG(PL_statbuf.st_mode))
	    Perl_croak(aTHX_ "Permission denied");
	if (PL_statbuf.st_mode & S_IWOTH)
	    Perl_croak(aTHX_ "Setuid/gid script is writable by world");
	PL_doswitches = FALSE;		/* -s is insecure in suid */
	CopLINE_inc(PL_curcop);
	if (sv_gets(PL_linestr, PL_rsfp, 0) == Nullch ||
	  strnNE(SvPV(PL_linestr,n_a),"#!",2) )	/* required even on Sys V */
	    Perl_croak(aTHX_ "No #! line");
	s = SvPV(PL_linestr,n_a)+2;
	if (*s == ' ') s++;
	while (!isSPACE(*s)) s++;
	for (s2 = s;  (s2 > SvPV(PL_linestr,n_a)+2 &&
		       (isDIGIT(s2[-1]) || strchr("._-", s2[-1])));  s2--) ;
	if (strnNE(s2-4,"perl",4) && strnNE(s-9,"perl",4))  /* sanity check */
	    Perl_croak(aTHX_ "Not a perl script");
	while (*s == ' ' || *s == '\t') s++;
	/*
	 * #! arg must be what we saw above.  They can invoke it by
	 * mentioning suidperl explicitly, but they may not add any strange
	 * arguments beyond what #! says if they do invoke suidperl that way.
	 */
	len = strlen(validarg);
	if (strEQ(validarg," PHOOEY ") ||
	    strnNE(s,validarg,len) || !isSPACE(s[len]))
	    Perl_croak(aTHX_ "Args must match #! line");

#ifndef IAMSUID
	if (PL_euid != PL_uid && (PL_statbuf.st_mode & S_ISUID) &&
	    PL_euid == PL_statbuf.st_uid)
	    if (!PL_do_undump)
		Perl_croak(aTHX_ "YOU HAVEN'T DISABLED SET-ID SCRIPTS IN THE KERNEL YET!\n\
FIX YOUR KERNEL, PUT A C WRAPPER AROUND THIS SCRIPT, OR USE -u AND UNDUMP!\n");
#endif /* IAMSUID */

	if (PL_euid) {	/* oops, we're not the setuid root perl */
	    (void)PerlIO_close(PL_rsfp);
#ifndef IAMSUID
	    /* try again */
	    PerlProc_execv(Perl_form(aTHX_ "%s/sperl"PERL_FS_VER_FMT, BIN_EXP,
				     (int)PERL_REVISION, (int)PERL_VERSION,
				     (int)PERL_SUBVERSION), PL_origargv);
#endif
	    Perl_croak(aTHX_ "Can't do setuid\n");
	}

	if (PL_statbuf.st_mode & S_ISGID && PL_statbuf.st_gid != PL_egid) {
#ifdef HAS_SETEGID
	    (void)setegid(PL_statbuf.st_gid);
#else
#ifdef HAS_SETREGID
           (void)setregid((Gid_t)-1,PL_statbuf.st_gid);
#else
#ifdef HAS_SETRESGID
           (void)setresgid((Gid_t)-1,PL_statbuf.st_gid,(Gid_t)-1);
#else
	    PerlProc_setgid(PL_statbuf.st_gid);
#endif
#endif
#endif
	    if (PerlProc_getegid() != PL_statbuf.st_gid)
		Perl_croak(aTHX_ "Can't do setegid!\n");
	}
	if (PL_statbuf.st_mode & S_ISUID) {
	    if (PL_statbuf.st_uid != PL_euid)
#ifdef HAS_SETEUID
		(void)seteuid(PL_statbuf.st_uid);	/* all that for this */
#else
#ifdef HAS_SETREUID
                (void)setreuid((Uid_t)-1,PL_statbuf.st_uid);
#else
#ifdef HAS_SETRESUID
                (void)setresuid((Uid_t)-1,PL_statbuf.st_uid,(Uid_t)-1);
#else
		PerlProc_setuid(PL_statbuf.st_uid);
#endif
#endif
#endif
	    if (PerlProc_geteuid() != PL_statbuf.st_uid)
		Perl_croak(aTHX_ "Can't do seteuid!\n");
	}
	else if (PL_uid) {			/* oops, mustn't run as root */
#ifdef HAS_SETEUID
          (void)seteuid((Uid_t)PL_uid);
#else
#ifdef HAS_SETREUID
          (void)setreuid((Uid_t)-1,(Uid_t)PL_uid);
#else
#ifdef HAS_SETRESUID
          (void)setresuid((Uid_t)-1,(Uid_t)PL_uid,(Uid_t)-1);
#else
          PerlProc_setuid((Uid_t)PL_uid);
#endif
#endif
#endif
	    if (PerlProc_geteuid() != PL_uid)
		Perl_croak(aTHX_ "Can't do seteuid!\n");
	}
	init_ids();
	if (!cando(S_IXUSR,TRUE,&PL_statbuf))
	    Perl_croak(aTHX_ "Permission denied\n");	/* they can't do this */
    }
#ifdef IAMSUID
    else if (PL_preprocess)
	Perl_croak(aTHX_ "-P not allowed for setuid/setgid script\n");
    else if (fdscript >= 0)
	Perl_croak(aTHX_ "fd script not allowed in suidperl\n");
    else
	Perl_croak(aTHX_ "Script is not setuid/setgid in suidperl\n");

    /* We absolutely must clear out any saved ids here, so we */
    /* exec the real perl, substituting fd script for scriptname. */
    /* (We pass script name as "subdir" of fd, which perl will grok.) */
    PerlIO_rewind(PL_rsfp);
    PerlLIO_lseek(PerlIO_fileno(PL_rsfp),(Off_t)0,0);  /* just in case rewind didn't */
    for (which = 1; PL_origargv[which] && PL_origargv[which] != scriptname; which++) ;
    if (!PL_origargv[which])
	Perl_croak(aTHX_ "Permission denied");
    PL_origargv[which] = savepv(Perl_form(aTHX_ "/dev/fd/%d/%s",
				  PerlIO_fileno(PL_rsfp), PL_origargv[which]));
#if defined(HAS_FCNTL) && defined(F_SETFD)
    fcntl(PerlIO_fileno(PL_rsfp),F_SETFD,0);	/* ensure no close-on-exec */
#endif
    PerlProc_execv(Perl_form(aTHX_ "%s/perl"PERL_FS_VER_FMT, BIN_EXP,
			     (int)PERL_REVISION, (int)PERL_VERSION,
			     (int)PERL_SUBVERSION), PL_origargv);/* try again */
    Perl_croak(aTHX_ "Can't do setuid\n");
#endif /* IAMSUID */
#else /* !DOSUID */
    if (PL_euid != PL_uid || PL_egid != PL_gid) {	/* (suidperl doesn't exist, in fact) */
#ifndef SETUID_SCRIPTS_ARE_SECURE_NOW
	PerlLIO_fstat(PerlIO_fileno(PL_rsfp),&PL_statbuf);	/* may be either wrapped or real suid */
	if ((PL_euid != PL_uid && PL_euid == PL_statbuf.st_uid && PL_statbuf.st_mode & S_ISUID)
	    ||
	    (PL_egid != PL_gid && PL_egid == PL_statbuf.st_gid && PL_statbuf.st_mode & S_ISGID)
	   )
	    if (!PL_do_undump)
		Perl_croak(aTHX_ "YOU HAVEN'T DISABLED SET-ID SCRIPTS IN THE KERNEL YET!\n\
FIX YOUR KERNEL, PUT A C WRAPPER AROUND THIS SCRIPT, OR USE -u AND UNDUMP!\n");
#endif /* SETUID_SCRIPTS_ARE_SECURE_NOW */
	/* not set-id, must be wrapped */
    }
#endif /* DOSUID */
}

STATIC void
S_find_beginning(pTHX)
{
    register char *s, *s2;

    /* skip forward in input to the real script? */

    forbid_setid("-x");
#ifdef MACOS_TRADITIONAL
    /* Since the Mac OS does not honor #! arguments for us, we do it ourselves */
    while (PL_doextract || gMacPerl_AlwaysExtract) {
	if ((s = sv_gets(PL_linestr, PL_rsfp, 0)) == Nullch) {
	    if (!gMacPerl_AlwaysExtract)
		Perl_croak(aTHX_ "No Perl script found in input\n");
		
	    if (PL_doextract)			/* require explicit override ? */
		if (!OverrideExtract(PL_origfilename))
		    Perl_croak(aTHX_ "User aborted script\n");
		else
		    PL_doextract = FALSE;
		
	    /* Pater peccavi, file does not have #! */
	    PerlIO_rewind(PL_rsfp);
	    
	    break;
	}
#else
    while (PL_doextract) {
	if ((s = sv_gets(PL_linestr, PL_rsfp, 0)) == Nullch)
	    Perl_croak(aTHX_ "No Perl script found in input\n");
#endif
	if (*s == '#' && s[1] == '!' && (s = instr(s,"perl"))) {
	    PerlIO_ungetc(PL_rsfp, '\n');		/* to keep line count right */
	    PL_doextract = FALSE;
	    while (*s && !(isSPACE (*s) || *s == '#')) s++;
	    s2 = s;
	    while (*s == ' ' || *s == '\t') s++;
	    if (*s++ == '-') {
		while (isDIGIT(s2[-1]) || strchr("-._", s2[-1])) s2--;
		if (strnEQ(s2-4,"perl",4))
		    /*SUPPRESS 530*/
		    while ((s = moreswitches(s)))
			;
	    }
	}
    }
}


STATIC void
S_init_ids(pTHX)
{
    PL_uid = PerlProc_getuid();
    PL_euid = PerlProc_geteuid();
    PL_gid = PerlProc_getgid();
    PL_egid = PerlProc_getegid();
#ifdef VMS
    PL_uid |= PL_gid << 16;
    PL_euid |= PL_egid << 16;
#endif
    PL_tainting |= (PL_uid && (PL_euid != PL_uid || PL_egid != PL_gid));
}

STATIC void
S_forbid_setid(pTHX_ char *s)
{
    if (PL_euid != PL_uid)
        Perl_croak(aTHX_ "No %s allowed while running setuid", s);
    if (PL_egid != PL_gid)
        Perl_croak(aTHX_ "No %s allowed while running setgid", s);
}

void
Perl_init_debugger(pTHX)
{
    HV *ostash = PL_curstash;

    PL_curstash = PL_debstash;
    PL_dbargs = GvAV(gv_AVadd((gv_fetchpv("args", GV_ADDMULTI, SVt_PVAV))));
    AvREAL_off(PL_dbargs);
    PL_DBgv = gv_fetchpv("DB", GV_ADDMULTI, SVt_PVGV);
    PL_DBline = gv_fetchpv("dbline", GV_ADDMULTI, SVt_PVAV);
    PL_DBsub = gv_HVadd(gv_fetchpv("sub", GV_ADDMULTI, SVt_PVHV));
    sv_upgrade(GvSV(PL_DBsub), SVt_IV);	/* IVX accessed if PERLDB_SUB_NN */
    PL_DBsingle = GvSV((gv_fetchpv("single", GV_ADDMULTI, SVt_PV)));
    sv_setiv(PL_DBsingle, 0); 
    PL_DBtrace = GvSV((gv_fetchpv("trace", GV_ADDMULTI, SVt_PV)));
    sv_setiv(PL_DBtrace, 0); 
    PL_DBsignal = GvSV((gv_fetchpv("signal", GV_ADDMULTI, SVt_PV)));
    sv_setiv(PL_DBsignal, 0); 
    PL_curstash = ostash;
}

#ifndef STRESS_REALLOC
#define REASONABLE(size) (size)
#else
#define REASONABLE(size) (1) /* unreasonable */
#endif

void
Perl_init_stacks(pTHX)
{
    /* start with 128-item stack and 8K cxstack */
    PL_curstackinfo = new_stackinfo(REASONABLE(128),
				 REASONABLE(8192/sizeof(PERL_CONTEXT) - 1));
    PL_curstackinfo->si_type = PERLSI_MAIN;
    PL_curstack = PL_curstackinfo->si_stack;
    PL_mainstack = PL_curstack;		/* remember in case we switch stacks */

    PL_stack_base = AvARRAY(PL_curstack);
    PL_stack_sp = PL_stack_base;
    PL_stack_max = PL_stack_base + AvMAX(PL_curstack);

    New(50,PL_tmps_stack,REASONABLE(128),SV*);
    PL_tmps_floor = -1;
    PL_tmps_ix = -1;
    PL_tmps_max = REASONABLE(128);

    New(54,PL_markstack,REASONABLE(32),I32);
    PL_markstack_ptr = PL_markstack;
    PL_markstack_max = PL_markstack + REASONABLE(32);

    SET_MARK_OFFSET;

    New(54,PL_scopestack,REASONABLE(32),I32);
    PL_scopestack_ix = 0;
    PL_scopestack_max = REASONABLE(32);

    New(54,PL_savestack,REASONABLE(128),ANY);
    PL_savestack_ix = 0;
    PL_savestack_max = REASONABLE(128);

    New(54,PL_retstack,REASONABLE(16),OP*);
    PL_retstack_ix = 0;
    PL_retstack_max = REASONABLE(16);
}

#undef REASONABLE

STATIC void
S_nuke_stacks(pTHX)
{
    while (PL_curstackinfo->si_next)
	PL_curstackinfo = PL_curstackinfo->si_next;
    while (PL_curstackinfo) {
	PERL_SI *p = PL_curstackinfo->si_prev;
	/* curstackinfo->si_stack got nuked by sv_free_arenas() */
	Safefree(PL_curstackinfo->si_cxstack);
	Safefree(PL_curstackinfo);
	PL_curstackinfo = p;
    }
    Safefree(PL_tmps_stack);
    Safefree(PL_markstack);
    Safefree(PL_scopestack);
    Safefree(PL_savestack);
    Safefree(PL_retstack);
}

#ifndef PERL_OBJECT
static PerlIO *tmpfp;  /* moved outside init_lexer() because of UNICOS bug */
#endif

STATIC void
S_init_lexer(pTHX)
{
#ifdef PERL_OBJECT
	PerlIO *tmpfp;
#endif
    tmpfp = PL_rsfp;
    PL_rsfp = Nullfp;
    lex_start(PL_linestr);
    PL_rsfp = tmpfp;
    PL_subname = newSVpvn("main",4);
}

STATIC void
S_init_predump_symbols(pTHX)
{
    GV *tmpgv;
    IO *io;

    sv_setpvn(get_sv("\"", TRUE), " ", 1);
    PL_stdingv = gv_fetchpv("STDIN",TRUE, SVt_PVIO);
    GvMULTI_on(PL_stdingv);
    io = GvIOp(PL_stdingv);
    IoIFP(io) = PerlIO_stdin();
    tmpgv = gv_fetchpv("stdin",TRUE, SVt_PV);
    GvMULTI_on(tmpgv);
    GvIOp(tmpgv) = (IO*)SvREFCNT_inc(io);

    tmpgv = gv_fetchpv("STDOUT",TRUE, SVt_PVIO);
    GvMULTI_on(tmpgv);
    io = GvIOp(tmpgv);
    IoOFP(io) = IoIFP(io) = PerlIO_stdout();
    setdefout(tmpgv);
    tmpgv = gv_fetchpv("stdout",TRUE, SVt_PV);
    GvMULTI_on(tmpgv);
    GvIOp(tmpgv) = (IO*)SvREFCNT_inc(io);

    PL_stderrgv = gv_fetchpv("STDERR",TRUE, SVt_PVIO);
    GvMULTI_on(PL_stderrgv);
    io = GvIOp(PL_stderrgv);
    IoOFP(io) = IoIFP(io) = PerlIO_stderr();
    tmpgv = gv_fetchpv("stderr",TRUE, SVt_PV);
    GvMULTI_on(tmpgv);
    GvIOp(tmpgv) = (IO*)SvREFCNT_inc(io);

    PL_statname = NEWSV(66,0);		/* last filename we did stat on */

    if (PL_osname)
    	Safefree(PL_osname);
    PL_osname = savepv(OSNAME);
}

STATIC void
S_init_postdump_symbols(pTHX_ register int argc, register char **argv, register char **env)
{
    char *s;
    SV *sv;
    GV* tmpgv;
    char **dup_env_base = 0;
    int dup_env_count = 0;

    argc--,argv++;	/* skip name of script */
    if (PL_doswitches) {
	for (; argc > 0 && **argv == '-'; argc--,argv++) {
	    if (!argv[0][1])
		break;
	    if (argv[0][1] == '-' && !argv[0][2]) {
		argc--,argv++;
		break;
	    }
	    if ((s = strchr(argv[0], '='))) {
		*s++ = '\0';
		sv_setpv(GvSV(gv_fetchpv(argv[0]+1,TRUE, SVt_PV)),s);
	    }
	    else
		sv_setiv(GvSV(gv_fetchpv(argv[0]+1,TRUE, SVt_PV)),1);
	}
    }
    PL_toptarget = NEWSV(0,0);
    sv_upgrade(PL_toptarget, SVt_PVFM);
    sv_setpvn(PL_toptarget, "", 0);
    PL_bodytarget = NEWSV(0,0);
    sv_upgrade(PL_bodytarget, SVt_PVFM);
    sv_setpvn(PL_bodytarget, "", 0);
    PL_formtarget = PL_bodytarget;

    TAINT;
    if ((tmpgv = gv_fetchpv("0",TRUE, SVt_PV))) {
#ifdef MACOS_TRADITIONAL
	/* $0 is not majick on a Mac */
	sv_setpv(GvSV(tmpgv),MacPerl_MPWFileName(PL_origfilename));
#else
	sv_setpv(GvSV(tmpgv),PL_origfilename);
	magicname("0", "0", 1);
#endif
    }
    if ((tmpgv = gv_fetchpv("\030",TRUE, SVt_PV)))
#ifdef OS2
	sv_setpv(GvSV(tmpgv), os2_execname(aTHX));
#else
	sv_setpv(GvSV(tmpgv),PL_origargv[0]);
#endif
    if ((PL_argvgv = gv_fetchpv("ARGV",TRUE, SVt_PVAV))) {
	GvMULTI_on(PL_argvgv);
	(void)gv_AVadd(PL_argvgv);
	av_clear(GvAVn(PL_argvgv));
	for (; argc > 0; argc--,argv++) {
	    SV *sv = newSVpv(argv[0],0);
	    av_push(GvAVn(PL_argvgv),sv);
	    if (PL_widesyscalls)
		(void)sv_utf8_decode(sv);
	}
    }
    if ((PL_envgv = gv_fetchpv("ENV",TRUE, SVt_PVHV))) {
	HV *hv;
	GvMULTI_on(PL_envgv);
	hv = GvHVn(PL_envgv);
	hv_magic(hv, Nullgv, 'E');
#ifdef USE_ENVIRON_ARRAY
	/* Note that if the supplied env parameter is actually a copy
	   of the global environ then it may now point to free'd memory
	   if the environment has been modified since. To avoid this
	   problem we treat env==NULL as meaning 'use the default'
	*/
	if (!env)
	    env = environ;
	if (env != environ)
	    environ[0] = Nullch;
#ifdef NEED_ENVIRON_DUP_FOR_MODIFY
	{
	    char **env_base;
	    for (env_base = env; *env; env++) 
		dup_env_count++;
	    if ((dup_env_base = (char **)
		 safesysmalloc( sizeof(char *) * (dup_env_count+1) ))) {
		char **dup_env;
		for (env = env_base, dup_env = dup_env_base;
		     *env;
		     env++, dup_env++) {
		    /* With environ one needs to use safesysmalloc(). */
		    *dup_env = safesysmalloc(strlen(*env) + 1);
		    (void)strcpy(*dup_env, *env);
		}
		*dup_env = Nullch;
		env = dup_env_base;
	    } /* else what? */
	}
#endif /* NEED_ENVIRON_DUP_FOR_MODIFY */
	for (; *env; env++) {
	    if (!(s = strchr(*env,'=')))
		continue;
	    *s++ = '\0';
#if defined(MSDOS)
	    (void)strupr(*env);
#endif
	    sv = newSVpv(s--,0);
	    (void)hv_store(hv, *env, s - *env, sv, 0);
	    *s = '=';
	}
#ifdef NEED_ENVIRON_DUP_FOR_MODIFY
	if (dup_env_base) {
	    char **dup_env;
	    for (dup_env = dup_env_base; *dup_env; dup_env++)
		safesysfree(*dup_env);
	    safesysfree(dup_env_base);
	}
#endif /* NEED_ENVIRON_DUP_FOR_MODIFY */
#endif /* USE_ENVIRON_ARRAY */
#ifdef DYNAMIC_ENV_FETCH
	HvNAME(hv) = savepv(ENV_HV_NAME);
#endif
    }
    TAINT_NOT;
    if ((tmpgv = gv_fetchpv("$",TRUE, SVt_PV)))
	sv_setiv(GvSV(tmpgv), (IV)PerlProc_getpid());
}

STATIC void
S_init_perllib(pTHX)
{
    char *s;
    if (!PL_tainting) {
#ifndef VMS
	s = PerlEnv_getenv("PERL5LIB");
	if (s)
	    incpush(s, TRUE, TRUE);
	else
	    incpush(PerlEnv_getenv("PERLLIB"), FALSE, FALSE);
#else /* VMS */
	/* Treat PERL5?LIB as a possible search list logical name -- the
	 * "natural" VMS idiom for a Unix path string.  We allow each
	 * element to be a set of |-separated directories for compatibility.
	 */
	char buf[256];
	int idx = 0;
	if (my_trnlnm("PERL5LIB",buf,0))
	    do { incpush(buf,TRUE,TRUE); } while (my_trnlnm("PERL5LIB",buf,++idx));
	else
	    while (my_trnlnm("PERLLIB",buf,idx++)) incpush(buf,FALSE,FALSE);
#endif /* VMS */
    }

/* Use the ~-expanded versions of APPLLIB (undocumented),
    ARCHLIB PRIVLIB SITEARCH SITELIB VENDORARCH and VENDORLIB
*/
#ifdef APPLLIB_EXP
    incpush(APPLLIB_EXP, TRUE, TRUE);
#endif

#ifdef ARCHLIB_EXP
    incpush(ARCHLIB_EXP, FALSE, FALSE);
#endif
#ifdef MACOS_TRADITIONAL
    {
	struct stat tmpstatbuf;
    	SV * privdir = NEWSV(55, 0);
	char * macperl = PerlEnv_getenv("MACPERL");
	
	if (!macperl)
	    macperl = "";
	
	Perl_sv_setpvf(aTHX_ privdir, "%slib:", macperl);
	if (PerlLIO_stat(SvPVX(privdir), &tmpstatbuf) >= 0 && S_ISDIR(tmpstatbuf.st_mode))
	    incpush(SvPVX(privdir), TRUE, FALSE);
	Perl_sv_setpvf(aTHX_ privdir, "%ssite_perl:", macperl);
	if (PerlLIO_stat(SvPVX(privdir), &tmpstatbuf) >= 0 && S_ISDIR(tmpstatbuf.st_mode))
	    incpush(SvPVX(privdir), TRUE, FALSE);
	    
   	SvREFCNT_dec(privdir);
    }
    if (!PL_tainting)
	incpush(":", FALSE, FALSE);
#else
#ifndef PRIVLIB_EXP
#  define PRIVLIB_EXP "/usr/local/lib/perl5:/usr/local/lib/perl"
#endif
#if defined(WIN32) 
    incpush(PRIVLIB_EXP, TRUE, FALSE);
#else
    incpush(PRIVLIB_EXP, FALSE, FALSE);
#endif

#ifdef SITEARCH_EXP
    /* sitearch is always relative to sitelib on Windows for
     * DLL-based path intuition to work correctly */
#  if !defined(WIN32)
    incpush(SITEARCH_EXP, FALSE, FALSE);
#  endif
#endif

#ifdef SITELIB_EXP
#  if defined(WIN32)
    incpush(SITELIB_EXP, TRUE, FALSE);	/* this picks up sitearch as well */
#  else
    incpush(SITELIB_EXP, FALSE, FALSE);
#  endif
#endif

#ifdef SITELIB_STEM /* Search for version-specific dirs below here */
    incpush(SITELIB_STEM, FALSE, TRUE);
#endif

#ifdef PERL_VENDORARCH_EXP
    /* vendorarch is always relative to vendorlib on Windows for
     * DLL-based path intuition to work correctly */
#  if !defined(WIN32)
    incpush(PERL_VENDORARCH_EXP, FALSE, FALSE);
#  endif
#endif

#ifdef PERL_VENDORLIB_EXP
#  if defined(WIN32)
    incpush(PERL_VENDORLIB_EXP, TRUE, FALSE);	/* this picks up vendorarch as well */
#  else
    incpush(PERL_VENDORLIB_EXP, FALSE, FALSE);
#  endif
#endif

#ifdef PERL_VENDORLIB_STEM /* Search for version-specific dirs below here */
    incpush(PERL_VENDORLIB_STEM, FALSE, TRUE);
#endif

#ifdef PERL_OTHERLIBDIRS
    incpush(PERL_OTHERLIBDIRS, TRUE, TRUE);
#endif

    if (!PL_tainting)
	incpush(".", FALSE, FALSE);
#endif /* MACOS_TRADITIONAL */
}

#if defined(DOSISH) || defined(EPOC)
#    define PERLLIB_SEP ';'
#else
#  if defined(VMS)
#    define PERLLIB_SEP '|'
#  else
#    if defined(MACOS_TRADITIONAL)
#      define PERLLIB_SEP ','
#    else
#      define PERLLIB_SEP ':'
#    endif
#  endif
#endif
#ifndef PERLLIB_MANGLE
#  define PERLLIB_MANGLE(s,n) (s)
#endif 

STATIC void
S_incpush(pTHX_ char *p, int addsubdirs, int addoldvers)
{
    SV *subdir = Nullsv;

    if (!p || !*p)
	return;

    if (addsubdirs || addoldvers) {
	subdir = sv_newmortal();
    }

    /* Break at all separators */
    while (p && *p) {
	SV *libdir = NEWSV(55,0);
	char *s;

	/* skip any consecutive separators */
	while ( *p == PERLLIB_SEP ) {
	    /* Uncomment the next line for PATH semantics */
	    /* av_push(GvAVn(PL_incgv), newSVpvn(".", 1)); */
	    p++;
	}

	if ( (s = strchr(p, PERLLIB_SEP)) != Nullch ) {
	    sv_setpvn(libdir, PERLLIB_MANGLE(p, (STRLEN)(s - p)),
		      (STRLEN)(s - p));
	    p = s + 1;
	}
	else {
	    sv_setpv(libdir, PERLLIB_MANGLE(p, 0));
	    p = Nullch;	/* break out */
	}
#ifdef MACOS_TRADITIONAL
	if (!strchr(SvPVX(libdir), ':'))
	    sv_insert(libdir, 0, 0, ":", 1);
	if (SvPVX(libdir)[SvCUR(libdir)-1] != ':')
	    sv_catpv(libdir, ":");
#endif

	/*
	 * BEFORE pushing libdir onto @INC we may first push version- and
	 * archname-specific sub-directories.
	 */
	if (addsubdirs || addoldvers) {
#ifdef PERL_INC_VERSION_LIST
	    /* Configure terminates PERL_INC_VERSION_LIST with a NULL */
	    const char *incverlist[] = { PERL_INC_VERSION_LIST };
	    const char **incver;
#endif
	    struct stat tmpstatbuf;
#ifdef VMS
	    char *unix;
	    STRLEN len;

	    if ((unix = tounixspec_ts(SvPV(libdir,len),Nullch)) != Nullch) {
		len = strlen(unix);
		while (unix[len-1] == '/') len--;  /* Cosmetic */
		sv_usepvn(libdir,unix,len);
	    }
	    else
		PerlIO_printf(Perl_error_log,
		              "Failed to unixify @INC element \"%s\"\n",
			      SvPV(libdir,len));
#endif
	    if (addsubdirs) {
#ifdef MACOS_TRADITIONAL
#define PERL_AV_SUFFIX_FMT	""
#define PERL_ARCH_FMT 		"%s:"
#define PERL_ARCH_FMT_PATH	PERL_FS_VER_FMT PERL_AV_SUFFIX_FMT
#else
#define PERL_AV_SUFFIX_FMT 	"/"
#define PERL_ARCH_FMT 		"/%s"
#define PERL_ARCH_FMT_PATH	PERL_AV_SUFFIX_FMT PERL_FS_VER_FMT
#endif
		/* .../version/archname if -d .../version/archname */
		Perl_sv_setpvf(aTHX_ subdir, "%"SVf PERL_ARCH_FMT_PATH PERL_ARCH_FMT,
				libdir,
			       (int)PERL_REVISION, (int)PERL_VERSION,
			       (int)PERL_SUBVERSION, ARCHNAME);
		if (PerlLIO_stat(SvPVX(subdir), &tmpstatbuf) >= 0 &&
		      S_ISDIR(tmpstatbuf.st_mode))
		    av_push(GvAVn(PL_incgv), newSVsv(subdir));

		/* .../version if -d .../version */
		Perl_sv_setpvf(aTHX_ subdir, "%"SVf PERL_ARCH_FMT_PATH, libdir,
			       (int)PERL_REVISION, (int)PERL_VERSION,
			       (int)PERL_SUBVERSION);
		if (PerlLIO_stat(SvPVX(subdir), &tmpstatbuf) >= 0 &&
		      S_ISDIR(tmpstatbuf.st_mode))
		    av_push(GvAVn(PL_incgv), newSVsv(subdir));

		/* .../archname if -d .../archname */
		Perl_sv_setpvf(aTHX_ subdir, "%"SVf PERL_ARCH_FMT, libdir, ARCHNAME);
		if (PerlLIO_stat(SvPVX(subdir), &tmpstatbuf) >= 0 &&
		      S_ISDIR(tmpstatbuf.st_mode))
		    av_push(GvAVn(PL_incgv), newSVsv(subdir));
	    }

#ifdef PERL_INC_VERSION_LIST
	    if (addoldvers) {
		for (incver = incverlist; *incver; incver++) {
		    /* .../xxx if -d .../xxx */
		    Perl_sv_setpvf(aTHX_ subdir, "%"SVf PERL_ARCH_FMT, libdir, *incver);
		    if (PerlLIO_stat(SvPVX(subdir), &tmpstatbuf) >= 0 &&
			  S_ISDIR(tmpstatbuf.st_mode))
			av_push(GvAVn(PL_incgv), newSVsv(subdir));
		}
	    }
#endif
	}

	/* finally push this lib directory on the end of @INC */
	av_push(GvAVn(PL_incgv), libdir);
    }
}

#ifdef USE_THREADS
STATIC struct perl_thread *
S_init_main_thread(pTHX)
{
#if !defined(PERL_IMPLICIT_CONTEXT)
    struct perl_thread *thr;
#endif
    XPV *xpv;

    Newz(53, thr, 1, struct perl_thread);
    PL_curcop = &PL_compiling;
    thr->interp = PERL_GET_INTERP;
    thr->cvcache = newHV();
    thr->threadsv = newAV();
    /* thr->threadsvp is set when find_threadsv is called */
    thr->specific = newAV();
    thr->flags = THRf_R_JOINABLE;
    MUTEX_INIT(&thr->mutex);
    /* Handcraft thrsv similarly to mess_sv */
    New(53, PL_thrsv, 1, SV);
    Newz(53, xpv, 1, XPV);
    SvFLAGS(PL_thrsv) = SVt_PV;
    SvANY(PL_thrsv) = (void*)xpv;
    SvREFCNT(PL_thrsv) = 1 << 30;	/* practically infinite */
    SvPVX(PL_thrsv) = (char*)thr;
    SvCUR_set(PL_thrsv, sizeof(thr));
    SvLEN_set(PL_thrsv, sizeof(thr));
    *SvEND(PL_thrsv) = '\0';	/* in the trailing_nul field */
    thr->oursv = PL_thrsv;
    PL_chopset = " \n-";
    PL_dumpindent = 4;

    MUTEX_LOCK(&PL_threads_mutex);
    PL_nthreads++;
    thr->tid = 0;
    thr->next = thr;
    thr->prev = thr;
    MUTEX_UNLOCK(&PL_threads_mutex);

#ifdef HAVE_THREAD_INTERN
    Perl_init_thread_intern(thr);
#endif

#ifdef SET_THREAD_SELF
    SET_THREAD_SELF(thr);
#else
    thr->self = pthread_self();
#endif /* SET_THREAD_SELF */
    PERL_SET_THX(thr);

    /*
     * These must come after the thread self setting
     * because sv_setpvn does SvTAINT and the taint
     * fields thread selfness being set.
     */
    PL_toptarget = NEWSV(0,0);
    sv_upgrade(PL_toptarget, SVt_PVFM);
    sv_setpvn(PL_toptarget, "", 0);
    PL_bodytarget = NEWSV(0,0);
    sv_upgrade(PL_bodytarget, SVt_PVFM);
    sv_setpvn(PL_bodytarget, "", 0);
    PL_formtarget = PL_bodytarget;
    thr->errsv = newSVpvn("", 0);
    (void) find_threadsv("@");	/* Ensure $@ is initialised early */

    PL_maxscream = -1;
    PL_regcompp = MEMBER_TO_FPTR(Perl_pregcomp);
    PL_regexecp = MEMBER_TO_FPTR(Perl_regexec_flags);
    PL_regint_start = MEMBER_TO_FPTR(Perl_re_intuit_start);
    PL_regint_string = MEMBER_TO_FPTR(Perl_re_intuit_string);
    PL_regfree = MEMBER_TO_FPTR(Perl_pregfree);
    PL_regindent = 0;
    PL_reginterp_cnt = 0;

    return thr;
}
#endif /* USE_THREADS */

void
Perl_call_list(pTHX_ I32 oldscope, AV *paramList)
{
    SV *atsv;
    line_t oldline = CopLINE(PL_curcop);
    CV *cv;
    STRLEN len;
    int ret;
    dJMPENV;

    while (AvFILL(paramList) >= 0) {
	cv = (CV*)av_shift(paramList);
	if ((PL_minus_c & 0x10) && (paramList == PL_beginav)) {
		/* save PL_beginav for compiler */
	    if (! PL_beginav_save)
		PL_beginav_save = newAV();
	    av_push(PL_beginav_save, (SV*)cv);
	} else {
	    SAVEFREESV(cv);
	}
#ifdef PERL_FLEXIBLE_EXCEPTIONS
	CALLPROTECT(aTHX_ pcur_env, &ret, MEMBER_TO_FPTR(S_vcall_list_body), cv);
#else
	JMPENV_PUSH(ret);
#endif
	switch (ret) {
	case 0:
#ifndef PERL_FLEXIBLE_EXCEPTIONS
	    call_list_body(cv);
#endif
	    atsv = ERRSV;
	    (void)SvPV(atsv, len);
	    if (len) {
		STRLEN n_a;
		PL_curcop = &PL_compiling;
		CopLINE_set(PL_curcop, oldline);
		if (paramList == PL_beginav)
		    sv_catpv(atsv, "BEGIN failed--compilation aborted");
		else
		    Perl_sv_catpvf(aTHX_ atsv,
				   "%s failed--call queue aborted",
				   paramList == PL_checkav ? "CHECK"
				   : paramList == PL_initav ? "INIT"
				   : "END");
		while (PL_scopestack_ix > oldscope)
		    LEAVE;
		JMPENV_POP;
		Perl_croak(aTHX_ "%s", SvPVx(atsv, n_a));
	    }
	    break;
	case 1:
	    STATUS_ALL_FAILURE;
	    /* FALL THROUGH */
	case 2:
	    /* my_exit() was called */
	    while (PL_scopestack_ix > oldscope)
		LEAVE;
	    FREETMPS;
	    PL_curstash = PL_defstash;
	    PL_curcop = &PL_compiling;
	    CopLINE_set(PL_curcop, oldline);
	    JMPENV_POP;
	    if (PL_statusvalue && !(PL_exit_flags & PERL_EXIT_EXPECTED)) {
		if (paramList == PL_beginav)
		    Perl_croak(aTHX_ "BEGIN failed--compilation aborted");
		else
		    Perl_croak(aTHX_ "%s failed--call queue aborted",
			       paramList == PL_checkav ? "CHECK"
			       : paramList == PL_initav ? "INIT"
			       : "END");
	    }
	    my_exit_jump();
	    /* NOTREACHED */
	case 3:
	    if (PL_restartop) {
		PL_curcop = &PL_compiling;
		CopLINE_set(PL_curcop, oldline);
		JMPENV_JUMP(3);
	    }
	    PerlIO_printf(Perl_error_log, "panic: restartop\n");
	    FREETMPS;
	    break;
	}
	JMPENV_POP;
    }
}

#ifdef PERL_FLEXIBLE_EXCEPTIONS
STATIC void *
S_vcall_list_body(pTHX_ va_list args)
{
    CV *cv = va_arg(args, CV*);
    return call_list_body(cv);
}
#endif

STATIC void *
S_call_list_body(pTHX_ CV *cv)
{
    PUSHMARK(PL_stack_sp);
    call_sv((SV*)cv, G_EVAL|G_DISCARD);
    return NULL;
}

void
Perl_my_exit(pTHX_ U32 status)
{
    DEBUG_S(PerlIO_printf(Perl_debug_log, "my_exit: thread %p, status %lu\n",
			  thr, (unsigned long) status));
    switch (status) {
    case 0:
	STATUS_ALL_SUCCESS;
	break;
    case 1:
	STATUS_ALL_FAILURE;
	break;
    default:
	STATUS_NATIVE_SET(status);
	break;
    }
    my_exit_jump();
}

void
Perl_my_failure_exit(pTHX)
{
#ifdef VMS
    if (vaxc$errno & 1) {
	if (STATUS_NATIVE & 1)		/* fortuitiously includes "-1" */
	    STATUS_NATIVE_SET(44);
    }
    else {
	if (!vaxc$errno && errno)	/* unlikely */
	    STATUS_NATIVE_SET(44);
	else
	    STATUS_NATIVE_SET(vaxc$errno);
    }
#else
    int exitstatus;
    if (errno & 255)
	STATUS_POSIX_SET(errno);
    else {
	exitstatus = STATUS_POSIX >> 8; 
	if (exitstatus & 255)
	    STATUS_POSIX_SET(exitstatus);
	else
	    STATUS_POSIX_SET(255);
    }
#endif
    my_exit_jump();
}

STATIC void
S_my_exit_jump(pTHX)
{
    register PERL_CONTEXT *cx;
    I32 gimme;
    SV **newsp;

    if (PL_e_script) {
	SvREFCNT_dec(PL_e_script);
	PL_e_script = Nullsv;
    }

    POPSTACK_TO(PL_mainstack);
    if (cxstack_ix >= 0) {
	if (cxstack_ix > 0)
	    dounwind(0);
	POPBLOCK(cx,PL_curpm);
	LEAVE;
    }

    JMPENV_JUMP(2);
}

#ifdef PERL_OBJECT
#include "XSUB.h"
#endif

static I32
read_e_script(pTHXo_ int idx, SV *buf_sv, int maxlen)
{
    char *p, *nl;
    p  = SvPVX(PL_e_script);
    nl = strchr(p, '\n');
    nl = (nl) ? nl+1 : SvEND(PL_e_script);
    if (nl-p == 0) {
	filter_del(read_e_script);
	return 0;
    }
    sv_catpvn(buf_sv, p, nl-p);
    sv_chop(PL_e_script, nl);
    return 1;
}
