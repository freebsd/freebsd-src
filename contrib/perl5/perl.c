/*    perl.c
 *
 *    Copyright (c) 1987-1999 Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * "A ship then new they built for him/of mithril and of elven glass" --Bilbo
 */

#include "EXTERN.h"
#include "perl.h"
#include "patchlevel.h"

/* XXX If this causes problems, set i_unistd=undef in the hint file.  */
#ifdef I_UNISTD
#include <unistd.h>
#endif

#if !defined(STANDARD_C) && !defined(HAS_GETENV_PROTOTYPE)
char *getenv _((char *)); /* Usually in <stdlib.h> */
#endif

#ifdef I_FCNTL
#include <fcntl.h>
#endif
#ifdef I_SYS_FILE
#include <sys/file.h>
#endif

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
static I32 read_e_script _((CPerlObj* pPerl, int idx, SV *buf_sv, int maxlen));
#else
static void find_beginning _((void));
static void forbid_setid _((char *));
static void incpush _((char *, int));
static void init_interp _((void));
static void init_ids _((void));
static void init_debugger _((void));
static void init_lexer _((void));
static void init_main_stash _((void));
#ifdef USE_THREADS
static struct perl_thread * init_main_thread _((void));
#endif /* USE_THREADS */
static void init_perllib _((void));
static void init_postdump_symbols _((int, char **, char **));
static void init_predump_symbols _((void));
static void my_exit_jump _((void)) __attribute__((noreturn));
static void nuke_stacks _((void));
static void open_script _((char *, bool, SV *, int *fd));
static void usage _((char *));
#ifdef IAMSUID
static int  fd_on_nosuid_fs _((int));
#endif
static void validate_suid _((char *, char*, int));
static I32 read_e_script _((int idx, SV *buf_sv, int maxlen));
#endif

#ifdef PERL_OBJECT
CPerlObj* perl_alloc(IPerlMem* ipM, IPerlEnv* ipE, IPerlStdIO* ipStd,
					     IPerlLIO* ipLIO, IPerlDir* ipD, IPerlSock* ipS, IPerlProc* ipP)
{
    CPerlObj* pPerl = new(ipM) CPerlObj(ipM, ipE, ipStd, ipLIO, ipD, ipS, ipP);
    if(pPerl != NULL)
	pPerl->Init();

    return pPerl;
}
#else
PerlInterpreter *
perl_alloc(void)
{
    PerlInterpreter *sv_interp;

    PL_curinterp = 0;
    New(53, sv_interp, 1, PerlInterpreter);
    return sv_interp;
}
#endif /* PERL_OBJECT */

void
#ifdef PERL_OBJECT
CPerlObj::perl_construct(void)
#else
perl_construct(register PerlInterpreter *sv_interp)
#endif
{
#ifdef USE_THREADS
    int i;
#ifndef FAKE_THREADS
    struct perl_thread *thr;
#endif /* FAKE_THREADS */
#endif /* USE_THREADS */
    
#ifndef PERL_OBJECT
    if (!(PL_curinterp = sv_interp))
	return;
#endif

#ifdef MULTIPLICITY
    ++PL_ninterps;
    Zero(sv_interp, 1, PerlInterpreter);
#endif

   /* Init the real globals (and main thread)? */
    if (!PL_linestr) {
#ifdef USE_THREADS

    	INIT_THREADS;
#ifdef ALLOC_THREAD_KEY
        ALLOC_THREAD_KEY;
#else
	if (pthread_key_create(&PL_thr_key, 0))
	    croak("panic: pthread_key_create");
#endif
	MUTEX_INIT(&PL_sv_mutex);
	MUTEX_INIT(&PL_cred_mutex);
	/*
	 * Safe to use basic SV functions from now on (though
	 * not things like mortals or tainting yet).
	 */
	MUTEX_INIT(&PL_eval_mutex);
	COND_INIT(&PL_eval_cond);
	MUTEX_INIT(&PL_threads_mutex);
	COND_INIT(&PL_nthreads_cond);
#ifdef EMULATE_ATOMIC_REFCOUNTS
	MUTEX_INIT(&PL_svref_mutex);
#endif /* EMULATE_ATOMIC_REFCOUNTS */
	
	thr = init_main_thread();
#endif /* USE_THREADS */

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
	PL_sighandlerp = sighandler;
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

    PL_nrs = newSVpv("\n", 1);
    PL_rs = SvREFCNT_inc(PL_nrs);

    init_stacks(ARGS);
#ifdef MULTIPLICITY
    init_interp();
    PL_perl_destruct_level = 1; 
#else
   if (PL_perl_destruct_level > 0)
       init_interp();
#endif

    init_ids();
    PL_lex_state = LEX_NOTPARSING;

    PL_start_env.je_prev = NULL;
    PL_start_env.je_ret = -1;
    PL_start_env.je_mustcatch = TRUE;
    PL_top_env     = &PL_start_env;
    STATUS_ALL_SUCCESS;

    SET_NUMERIC_STANDARD();
#if defined(SUBVERSION) && SUBVERSION > 0
    sprintf(PL_patchlevel, "%7.5f",   (double) 5 
				+ ((double) PATCHLEVEL / (double) 1000)
				+ ((double) SUBVERSION / (double) 100000));
#else
    sprintf(PL_patchlevel, "%5.3f", (double) 5 +
				((double) PATCHLEVEL / (double) 1000));
#endif

#if defined(LOCAL_PATCH_COUNT)
    PL_localpatches = local_patches;	/* For possible -v */
#endif

    PerlIO_init();			/* Hook to IO system */

    PL_fdpid = newAV();			/* for remembering popen pids by fd */
    PL_modglobal = newHV();		/* pointers to per-interpreter module globals */

    DEBUG( {
	New(51,PL_debname,128,char);
	New(52,PL_debdelim,128,char);
    } )

    ENTER;
}

void
#ifdef PERL_OBJECT
CPerlObj::perl_destruct(void)
#else
perl_destruct(register PerlInterpreter *sv_interp)
#endif
{
    dTHR;
    int destruct_level;  /* 0=none, 1=full, 2=full with checks */
    I32 last_sv_count;
    HV *hv;
#ifdef USE_THREADS
    Thread t;
#endif /* USE_THREADS */

#ifndef PERL_OBJECT
    if (!(PL_curinterp = sv_interp))
	return;
#endif

#ifdef USE_THREADS
#ifndef FAKE_THREADS
    /* Pass 1 on any remaining threads: detach joinables, join zombies */
  retry_cleanup:
    MUTEX_LOCK(&PL_threads_mutex);
    DEBUG_S(PerlIO_printf(PerlIO_stderr(),
			  "perl_destruct: waiting for %d threads...\n",
			  PL_nthreads - 1));
    for (t = thr->next; t != thr; t = t->next) {
	MUTEX_LOCK(&t->mutex);
	switch (ThrSTATE(t)) {
	    AV *av;
	case THRf_ZOMBIE:
	    DEBUG_S(PerlIO_printf(PerlIO_stderr(),
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
	    DEBUG_S(PerlIO_printf(PerlIO_stderr(),
				  "perl_destruct: joined zombie %p OK\n", t));
	    goto retry_cleanup;
	case THRf_R_JOINABLE:
	    DEBUG_S(PerlIO_printf(PerlIO_stderr(),
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
	    DEBUG_S(PerlIO_printf(PerlIO_stderr(),
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
	DEBUG_S(PerlIO_printf(PerlIO_stderr(),
			      "perl_destruct: final wait for %d threads\n",
			      PL_nthreads - 1));
	COND_WAIT(&PL_nthreads_cond, &PL_threads_mutex);
    }
    /* At this point, we're the last thread */
    MUTEX_UNLOCK(&PL_threads_mutex);
    DEBUG_S(PerlIO_printf(PerlIO_stderr(), "perl_destruct: armageddon has arrived\n"));
    MUTEX_DESTROY(&PL_threads_mutex);
    COND_DESTROY(&PL_nthreads_cond);
#endif /* !defined(FAKE_THREADS) */
#endif /* USE_THREADS */

    destruct_level = PL_perl_destruct_level;
#ifdef DEBUGGING
    {
	char *s;
	if (s = PerlEnv_getenv("PERL_DESTRUCT_LEVEL")) {
	    int i = atoi(s);
	    if (destruct_level < i)
		destruct_level = i;
	}
    }
#endif

    LEAVE;
    FREETMPS;

#ifdef MULTIPLICITY
    --PL_ninterps;
#endif

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

    if (PL_sv_objcount) {
	/*
	 * Try to destruct global references.  We do this first so that the
	 * destructors and destructees still exist.  Some sv's might remain.
	 * Non-referenced objects are on their own.
	 */
    
	PL_dirty = TRUE;
	sv_clean_objs();
    }

    /* unhook hooks which will soon be, or use, destroyed data */
    SvREFCNT_dec(PL_warnhook);
    PL_warnhook = Nullsv;
    SvREFCNT_dec(PL_diehook);
    PL_diehook = Nullsv;
    SvREFCNT_dec(PL_parsehook);
    PL_parsehook = Nullsv;

    /* call exit list functions */
    while (PL_exitlistlen-- > 0)
	PL_exitlist[PL_exitlistlen].fn(PERL_OBJECT_THIS_ PL_exitlist[PL_exitlistlen].ptr);

    Safefree(PL_exitlist);

    if (destruct_level == 0){

	DEBUG_P(debprofdump());
    
	/* The exit() function will do everything that needs doing. */
	return;
    }

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
    PL_dowarn       = FALSE;
    PL_doextract    = FALSE;
    PL_sawampersand = FALSE;	/* must save all match strings */
    PL_sawstudy     = FALSE;	/* do fbm_instr on all strings */
    PL_sawvec       = FALSE;
    PL_unsafe       = FALSE;

    Safefree(PL_inplace);
    PL_inplace = Nullch;

    if (PL_e_script) {
	SvREFCNT_dec(PL_e_script);
	PL_e_script = Nullsv;
    }

    /* magical thingies */

    Safefree(PL_ofs);	/* $, */
    PL_ofs = Nullch;

    Safefree(PL_ors);	/* $\ */
    PL_ors = Nullch;

    SvREFCNT_dec(PL_rs);	/* $/ */
    PL_rs = Nullsv;

    SvREFCNT_dec(PL_nrs);	/* $/ helper */
    PL_nrs = Nullsv;

    PL_multiline = 0;	/* $* */

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

    /* startup and shutdown function lists */
    SvREFCNT_dec(PL_beginav);
    SvREFCNT_dec(PL_endav);
    SvREFCNT_dec(PL_initav);
    PL_beginav = Nullav;
    PL_endav = Nullav;
    PL_initav = Nullav;

    /* shortcuts just get cleared */
    PL_envgv = Nullgv;
    PL_siggv = Nullgv;
    PL_incgv = Nullgv;
    PL_hintgv = Nullgv;
    PL_errgv = Nullgv;
    PL_argvgv = Nullgv;
    PL_argvoutgv = Nullgv;
    PL_stdingv = Nullgv;
    PL_last_in_gv = Nullgv;
    PL_replgv = Nullgv;

    /* reset so print() ends up where we expect */
    setdefout(Nullgv);

    /* Prepare to destruct main symbol table.  */

    hv = PL_defstash;
    PL_defstash = 0;
    SvREFCNT_dec(hv);

    FREETMPS;
    if (destruct_level >= 2) {
	if (PL_scopestack_ix != 0)
	    warn("Unbalanced scopes: %ld more ENTERs than LEAVEs\n",
		 (long)PL_scopestack_ix);
	if (PL_savestack_ix != 0)
	    warn("Unbalanced saves: %ld more saves than restores\n",
		 (long)PL_savestack_ix);
	if (PL_tmps_floor != -1)
	    warn("Unbalanced tmps: %ld more allocs than frees\n",
		 (long)PL_tmps_floor + 1);
	if (cxstack_ix != -1)
	    warn("Unbalanced context: %ld more PUSHes than POPs\n",
		 (long)cxstack_ix + 1);
    }

    /* Now absolutely destruct everything, somehow or other, loops or no. */
    last_sv_count = 0;
    SvFLAGS(PL_strtab) |= SVTYPEMASK;		/* don't clean out strtab now */
    while (PL_sv_count != 0 && PL_sv_count != last_sv_count) {
	last_sv_count = PL_sv_count;
	sv_clean_all();
    }
    SvFLAGS(PL_strtab) &= ~SVTYPEMASK;
    SvFLAGS(PL_strtab) |= SVt_PVHV;
    
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
	    if (hent) {
		warn("Unbalanced string table refcount: (%d) for \"%s\"",
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

    if (PL_sv_count != 0)
	warn("Scalars leaked: %ld\n", (long)PL_sv_count);

    sv_free_arenas();

    /* No SVs have survived, need to clean out */
    PL_linestr = NULL;
    PL_pidstatus = Nullhv;
    Safefree(PL_origfilename);
    Safefree(PL_archpat_auto);
    Safefree(PL_reg_start_tmp);
    Safefree(HeKEY_hek(&PL_hv_fetch_ent_mh));
    Safefree(PL_op_mask);
    nuke_stacks();
    PL_hints = 0;		/* Reset hints. Should hints be per-interpreter ? */
    
    DEBUG_P(debprofdump());
#ifdef USE_THREADS
    MUTEX_DESTROY(&PL_strtab_mutex);
    MUTEX_DESTROY(&PL_sv_mutex);
    MUTEX_DESTROY(&PL_cred_mutex);
    MUTEX_DESTROY(&PL_eval_mutex);
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
	SvOOK_off(PL_mess_sv);
	Safefree(SvPVX(PL_mess_sv));
	Safefree(SvANY(PL_mess_sv));
	Safefree(PL_mess_sv);
	PL_mess_sv = Nullsv;
    }
}

void
#ifdef PERL_OBJECT
CPerlObj::perl_free(void)
#else
perl_free(PerlInterpreter *sv_interp)
#endif
{
#ifdef PERL_OBJECT
	Safefree(this);
#else
    if (!(PL_curinterp = sv_interp))
	return;
    Safefree(sv_interp);
#endif
}

void
#ifdef PERL_OBJECT
CPerlObj::perl_atexit(void (*fn) (CPerlObj*,void *), void *ptr)
#else
perl_atexit(void (*fn) (void *), void *ptr)
#endif
{
    Renew(PL_exitlist, PL_exitlistlen+1, PerlExitListEntry);
    PL_exitlist[PL_exitlistlen].fn = fn;
    PL_exitlist[PL_exitlistlen].ptr = ptr;
    ++PL_exitlistlen;
}

int
#ifdef PERL_OBJECT
CPerlObj::perl_parse(void (*xsinit) (CPerlObj*), int argc, char **argv, char **env)
#else
perl_parse(PerlInterpreter *sv_interp, void (*xsinit) (void), int argc, char **argv, char **env)
#endif
{
    dTHR;
    register SV *sv;
    register char *s;
    char *scriptname = NULL;
    VOL bool dosearch = FALSE;
    char *validarg = "";
    I32 oldscope;
    AV* comppadlist;
    dJMPENV;
    int ret;
    int fdscript = -1;

#ifdef SETUID_SCRIPTS_ARE_SECURE_NOW
#ifdef IAMSUID
#undef IAMSUID
    croak("suidperl is no longer needed since the kernel can now execute\n\
setuid perl scripts securely.\n");
#endif
#endif

#ifndef PERL_OBJECT
    if (!(PL_curinterp = sv_interp))
	return 255;
#endif

#if defined(NeXT) && defined(__DYNAMIC__)
    _dyld_lookup_and_bind
	("__environ", (unsigned long *) &environ_pointer, NULL);
#endif /* environ */

    PL_origargv = argv;
    PL_origargc = argc;
#ifndef VMS  /* VMS doesn't have environ array */
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

    JMPENV_PUSH(ret);
    switch (ret) {
    case 1:
	STATUS_ALL_FAILURE;
	/* FALL THROUGH */
    case 2:
	/* my_exit() was called */
	while (PL_scopestack_ix > oldscope)
	    LEAVE;
	FREETMPS;
	PL_curstash = PL_defstash;
	if (PL_endav)
	    call_list(oldscope, PL_endav);
	JMPENV_POP;
	return STATUS_NATIVE_EXPORT;
    case 3:
	JMPENV_POP;
	PerlIO_printf(PerlIO_stderr(), "panic: top_env\n");
	return 1;
    }

    sv_setpvn(PL_linestr,"",0);
    sv = newSVpv("",0);		/* first used for -I flags */
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
	case 'w':
	    if (s = moreswitches(s))
		goto reswitch;
	    break;

	case 'T':
	    PL_tainting = TRUE;
	    s++;
	    goto reswitch;

	case 'e':
	    if (PL_euid != PL_uid || PL_egid != PL_gid)
		croak("No -e allowed in setuid scripts");
	    if (!PL_e_script) {
		PL_e_script = newSVpv("",0);
		filter_add(read_e_script, NULL);
	    }
	    if (*++s)
		sv_catpv(PL_e_script, s);
	    else if (argv[1]) {
		sv_catpv(PL_e_script, argv[1]);
		argc--,argv++;
	    }
	    else
		croak("No code specified for -e");
	    sv_catpv(PL_e_script, "\n");
	    break;

	case 'I':	/* -I handled both here and in moreswitches() */
	    forbid_setid("-I");
	    if (!*++s && (s=argv[1]) != Nullch) {
		argc--,argv++;
	    }
	    while (s && isSPACE(*s))
		++s;
	    if (s && *s) {
		char *e, *p;
		for (e = s; *e && !isSPACE(*e); e++) ;
		p = savepvn(s, e-s);
		incpush(p, TRUE);
		sv_catpv(sv,"-I");
		sv_catpv(sv,p);
		sv_catpv(sv," ");
		Safefree(p);
	    }	/* XXX else croak? */
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
#if defined(DEBUGGING) || defined(NO_EMBED) || defined(MULTIPLICITY)
		sv_catpv(PL_Sv,"\"  Compile-time options:");
#  ifdef DEBUGGING
		sv_catpv(PL_Sv," DEBUGGING");
#  endif
#  ifdef NO_EMBED
		sv_catpv(PL_Sv," NO_EMBED");
#  endif
#  ifdef MULTIPLICITY
		sv_catpv(PL_Sv," MULTIPLICITY");
#  endif
		sv_catpv(PL_Sv,"\\n\",");
#endif
#if defined(LOCAL_PATCH_COUNT)
		if (LOCAL_PATCH_COUNT > 0) {
		    int i;
		    sv_catpv(PL_Sv,"\"  Locally applied patches:\\n\",");
		    for (i = 1; i <= LOCAL_PATCH_COUNT; i++) {
			if (PL_localpatches[i])
			    sv_catpvf(PL_Sv,"\"  \\t%s\\n\",",PL_localpatches[i]);
		    }
		}
#endif
		sv_catpvf(PL_Sv,"\"  Built under %s\\n\"",OSNAME);
#ifdef __DATE__
#  ifdef __TIME__
		sv_catpvf(PL_Sv,",\"  Compiled at %s %s\\n\"",__DATE__,__TIME__);
#  else
		sv_catpvf(PL_Sv,",\"  Compiled on %s\\n\"",__DATE__);
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
		PL_cddir = savepv(s);
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
	    croak("Unrecognized switch: -%s  (-h will show valid options)",s);
	}
    }
  switch_end:

    if (!PL_tainting && (s = PerlEnv_getenv("PERL5OPT"))) {
	while (s && *s) {
	    while (isSPACE(*s))
		s++;
	    if (*s == '-') {
		s++;
		if (isSPACE(*s))
		    continue;
	    }
	    if (!*s)
		break;
	    if (!strchr("DIMUdmw", *s))
		croak("Illegal switch in PERL5OPT: -%c", *s);
	    s = moreswitches(s);
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

    if (PL_doextract)
	find_beginning();

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
    av_store(PL_comppad_name, 0, newSVpv("@_", 2));
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

    if (xsinit)
	(*xsinit)(PERL_OBJECT_THIS);	/* in case linked C routines want magical variables */
#if defined(VMS) || defined(WIN32) || defined(DJGPP)
    init_os_extras();
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
    if (yyparse() || PL_error_count) {
	if (PL_minus_c)
	    croak("%s had compilation errors.\n", PL_origfilename);
	else {
	    croak("Execution of %s aborted due to compilation errors.\n",
		PL_origfilename);
	}
    }
    PL_curcop->cop_line = 0;
    PL_curstash = PL_defstash;
    PL_preprocess = FALSE;
    if (PL_e_script) {
	SvREFCNT_dec(PL_e_script);
	PL_e_script = Nullsv;
    }

    /* now that script is parsed, we can modify record separator */
    SvREFCNT_dec(PL_rs);
    PL_rs = SvREFCNT_inc(PL_nrs);
    sv_setsv(perl_get_sv("/", TRUE), PL_rs);
    if (PL_do_undump)
	my_unexec();

    if (PL_dowarn)
	gv_check(PL_defstash);

    LEAVE;
    FREETMPS;

#ifdef MYMALLOC
    if ((s=PerlEnv_getenv("PERL_DEBUG_MSTATS")) && atoi(s) >= 2)
	dump_mstats("after compilation:");
#endif

    ENTER;
    PL_restartop = 0;
    JMPENV_POP;
    return 0;
}

int
#ifdef PERL_OBJECT
CPerlObj::perl_run(void)
#else
perl_run(PerlInterpreter *sv_interp)
#endif
{
    dSP;
    I32 oldscope;
    dJMPENV;
    int ret;

#ifndef PERL_OBJECT
    if (!(PL_curinterp = sv_interp))
	return 255;
#endif

    oldscope = PL_scopestack_ix;

    JMPENV_PUSH(ret);
    switch (ret) {
    case 1:
	cxstack_ix = -1;		/* start context stack again */
	break;
    case 2:
	/* my_exit() was called */
	while (PL_scopestack_ix > oldscope)
	    LEAVE;
	FREETMPS;
	PL_curstash = PL_defstash;
	if (PL_endav)
	    call_list(oldscope, PL_endav);
#ifdef MYMALLOC
	if (PerlEnv_getenv("PERL_DEBUG_MSTATS"))
	    dump_mstats("after execution:  ");
#endif
	JMPENV_POP;
	return STATUS_NATIVE_EXPORT;
    case 3:
	if (!PL_restartop) {
	    PerlIO_printf(PerlIO_stderr(), "panic: restartop\n");
	    FREETMPS;
	    JMPENV_POP;
	    return 1;
	}
	POPSTACK_TO(PL_mainstack);
	break;
    }

    DEBUG_r(PerlIO_printf(Perl_debug_log, "%s $` $& $' support.\n",
                    PL_sawampersand ? "Enabling" : "Omitting"));

    if (!PL_restartop) {
	DEBUG_x(dump_all());
	DEBUG(PerlIO_printf(Perl_debug_log, "\nEXECUTING...\n\n"));
	DEBUG_S(PerlIO_printf(Perl_debug_log, "main thread is 0x%lx\n",
			      (unsigned long) thr));

	if (PL_minus_c) {
	    PerlIO_printf(PerlIO_stderr(), "%s syntax OK\n", PL_origfilename);
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
	CALLRUNOPS();
    }
    else if (PL_main_start) {
	CvDEPTH(PL_main_cv) = 1;
	PL_op = PL_main_start;
	CALLRUNOPS();
    }

    my_exit(0);
    /* NOTREACHED */
    return 0;
}

SV*
perl_get_sv(char *name, I32 create)
{
    GV *gv;
#ifdef USE_THREADS
    if (name[1] == '\0' && !isALPHA(name[0])) {
	PADOFFSET tmp = find_threadsv(name);
    	if (tmp != NOT_IN_PAD) {
	    dTHR;
	    return THREADSV(tmp);
	}
    }
#endif /* USE_THREADS */
    gv = gv_fetchpv(name, create, SVt_PV);
    if (gv)
	return GvSV(gv);
    return Nullsv;
}

AV*
perl_get_av(char *name, I32 create)
{
    GV* gv = gv_fetchpv(name, create, SVt_PVAV);
    if (create)
    	return GvAVn(gv);
    if (gv)
	return GvAV(gv);
    return Nullav;
}

HV*
perl_get_hv(char *name, I32 create)
{
    GV* gv = gv_fetchpv(name, create, SVt_PVHV);
    if (create)
    	return GvHVn(gv);
    if (gv)
	return GvHV(gv);
    return Nullhv;
}

CV*
perl_get_cv(char *name, I32 create)
{
    GV* gv = gv_fetchpv(name, create, SVt_PVCV);
    /* XXX unsafe for threads if eval_owner isn't held */
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

I32
perl_call_argv(char *sub_name, I32 flags, register char **argv)
              
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
    return perl_call_pv(sub_name, flags);
}

I32
perl_call_pv(char *sub_name, I32 flags)
              		/* name of the subroutine */
          		/* See G_* flags in cop.h */
{
    return perl_call_sv((SV*)perl_get_cv(sub_name, TRUE), flags);
}

I32
perl_call_method(char *methname, I32 flags)
               		/* name of the subroutine */
          		/* See G_* flags in cop.h */
{
    dSP;
    OP myop;
    if (!PL_op)
	PL_op = &myop;
    XPUSHs(sv_2mortal(newSVpv(methname,0)));
    PUTBACK;
    pp_method(ARGS);
	if(PL_op == &myop)
		PL_op = Nullop;
    return perl_call_sv(*PL_stack_sp--, flags);
}

/* May be called with any of a CV, a GV, or an SV containing the name. */
I32
perl_call_sv(SV *sv, I32 flags)
       
          		/* See G_* flags in cop.h */
{
    dSP;
    LOGOP myop;		/* fake syntax tree node */
    I32 oldmark;
    I32 retval;
    I32 oldscope;
    bool oldcatch = CATCH_GET;
    dJMPENV;
    int ret;
    OP* oldop = PL_op;

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

    if (flags & G_EVAL) {
	cLOGOP->op_other = PL_op;
	PL_markstack_ptr--;
	/* we're trying to emulate pp_entertry() here */
	{
	    register PERL_CONTEXT *cx;
	    I32 gimme = GIMME_V;
	    
	    ENTER;
	    SAVETMPS;
	    
	    push_return(PL_op->op_next);
	    PUSHBLOCK(cx, CXt_EVAL, PL_stack_sp);
	    PUSHEVAL(cx, 0, 0);
	    PL_eval_root = PL_op;             /* Only needed so that goto works right. */
	    
	    PL_in_eval = 1;
	    if (flags & G_KEEPERR)
		PL_in_eval |= 4;
	    else
		sv_setpv(ERRSV,"");
	}
	PL_markstack_ptr++;

	JMPENV_PUSH(ret);
	switch (ret) {
	case 0:
	    break;
	case 1:
	    STATUS_ALL_FAILURE;
	    /* FALL THROUGH */
	case 2:
	    /* my_exit() was called */
	    PL_curstash = PL_defstash;
	    FREETMPS;
	    JMPENV_POP;
	    if (PL_statusvalue)
		croak("Callback called exit");
	    my_exit_jump();
	    /* NOTREACHED */
	case 3:
	    if (PL_restartop) {
		PL_op = PL_restartop;
		PL_restartop = 0;
		break;
	    }
	    PL_stack_sp = PL_stack_base + oldmark;
	    if (flags & G_ARRAY)
		retval = 0;
	    else {
		retval = 1;
		*++PL_stack_sp = &PL_sv_undef;
	    }
	    goto cleanup;
	}
    }
    else
	CATCH_SET(TRUE);

    if (PL_op == (OP*)&myop)
	PL_op = pp_entersub(ARGS);
    if (PL_op)
	CALLRUNOPS();
    retval = PL_stack_sp - (PL_stack_base + oldmark);
    if ((flags & G_EVAL) && !(flags & G_KEEPERR))
	sv_setpv(ERRSV,"");

  cleanup:
    if (flags & G_EVAL) {
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
    else
	CATCH_SET(oldcatch);

    if (flags & G_DISCARD) {
	PL_stack_sp = PL_stack_base + oldmark;
	retval = 0;
	FREETMPS;
	LEAVE;
    }
    PL_op = oldop;
    return retval;
}

/* Eval a string. The G_EVAL flag is always assumed. */

I32
perl_eval_sv(SV *sv, I32 flags)
       
          		/* See G_* flags in cop.h */
{
    dSP;
    UNOP myop;		/* fake syntax tree node */
    I32 oldmark = SP - PL_stack_base;
    I32 retval;
    I32 oldscope;
    dJMPENV;
    int ret;
    OP* oldop = PL_op;

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

    JMPENV_PUSH(ret);
    switch (ret) {
    case 0:
	break;
    case 1:
	STATUS_ALL_FAILURE;
	/* FALL THROUGH */
    case 2:
	/* my_exit() was called */
	PL_curstash = PL_defstash;
	FREETMPS;
	JMPENV_POP;
	if (PL_statusvalue)
	    croak("Callback called exit");
	my_exit_jump();
	/* NOTREACHED */
    case 3:
	if (PL_restartop) {
	    PL_op = PL_restartop;
	    PL_restartop = 0;
	    break;
	}
	PL_stack_sp = PL_stack_base + oldmark;
	if (flags & G_ARRAY)
	    retval = 0;
	else {
	    retval = 1;
	    *++PL_stack_sp = &PL_sv_undef;
	}
	goto cleanup;
    }

    if (PL_op == (OP*)&myop)
	PL_op = pp_entereval(ARGS);
    if (PL_op)
	CALLRUNOPS();
    retval = PL_stack_sp - (PL_stack_base + oldmark);
    if (!(flags & G_KEEPERR))
	sv_setpv(ERRSV,"");

  cleanup:
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

SV*
perl_eval_pv(char *p, I32 croak_on_error)
{
    dSP;
    SV* sv = newSVpv(p, 0);

    PUSHMARK(SP);
    perl_eval_sv(sv, G_SCALAR);
    SvREFCNT_dec(sv);

    SPAGAIN;
    sv = POPs;
    PUTBACK;

    if (croak_on_error && SvTRUE(ERRSV)) {
	STRLEN n_a;
	croak(SvPVx(ERRSV, n_a));
    }

    return sv;
}

/* Require a module. */

void
perl_require_pv(char *pv)
{
    SV* sv;
    dSP;
    PUSHSTACKi(PERLSI_REQUIRE);
    PUTBACK;
    sv = sv_newmortal();
    sv_setpv(sv, "require '");
    sv_catpv(sv, pv);
    sv_catpv(sv, "'");
    perl_eval_sv(sv, G_DISCARD);
    SPAGAIN;
    POPSTACK;
}

void
magicname(char *sym, char *name, I32 namlen)
{
    register GV *gv;

    if (gv = gv_fetchpv(sym,TRUE, SVt_PV))
	sv_magic(GvSV(gv), (SV*)gv, 0, name, namlen);
}

STATIC void
usage(char *name)		/* XXX move this out into a module ? */
           
{
    /* This message really ought to be max 23 lines.
     * Removed -h because the user already knows that opton. Others? */

    static char *usage_msg[] = {
"-0[octal]       specify record separator (\\0, if no argument)",
"-a              autosplit mode with -n or -p (splits $_ into @F)",
"-c              check syntax only (runs BEGIN and END blocks)",
"-d[:debugger]   run scripts under debugger",
"-D[number/list] set debugging flags (argument is a bit mask or flags)",
"-e 'command'    one line of script. Several -e's allowed. Omit [programfile].",
"-F/pattern/     split() pattern for autosplit (-a). The //'s are optional.",
"-i[extension]   edit <> files in place (make backup if extension supplied)",
"-Idirectory     specify @INC/#include directory (may be used more than once)",
"-l[octal]       enable line ending processing, specifies line terminator",
"-[mM][-]module.. executes `use/no module...' before executing your script.",
"-n              assume 'while (<>) { ... }' loop around your script",
"-p              assume loop like -n but print line also like sed",
"-P              run script through C preprocessor before compilation",
"-s              enable some switch parsing for switches after script name",
"-S              look for the script using PATH environment variable",
"-T              turn on tainting checks",
"-u              dump core after parsing script",
"-U              allow unsafe operations",
"-v              print version number, patchlevel plus VERY IMPORTANT perl info",
"-V[:variable]   print perl configuration information",
"-w              TURN WARNINGS ON FOR COMPILATION OF YOUR SCRIPT. Recommended.",
"-x[directory]   strip off text before #!perl line and perhaps cd to directory",
"\n",
NULL
};
    char **p = usage_msg;

    printf("\nUsage: %s [switches] [--] [programfile] [arguments]", name);
    while (*p)
	printf("\n  %s", *p++);
}

/* This routine handles any switches that can be given during run */

char *
moreswitches(char *s)
{
    I32 numlen;
    U32 rschar;

    switch (*s) {
    case '0':
    {
	dTHR;
	rschar = scan_oct(s, 4, &numlen);
	SvREFCNT_dec(PL_nrs);
	if (rschar & ~((U8)~0))
	    PL_nrs = &PL_sv_undef;
	else if (!rschar && numlen >= 2)
	    PL_nrs = newSVpv("", 0);
	else {
	    char ch = rschar;
	    PL_nrs = newSVpv(&ch, 1);
	}
	return s + numlen;
    }
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
	if (*s == ':' || *s == '=')  {
	    my_setenv("PERL5DB", form("use Devel::%s;", ++s));
	    s += strlen(s);
	}
	if (!PL_perldb) {
	    PL_perldb = PERLDB_ALL;
	    init_debugger();
	}
	return s;
    case 'D':
#ifdef DEBUGGING
	forbid_setid("-D");
	if (isALPHA(s[1])) {
	    static char debopts[] = "psltocPmfrxuLHXDS";
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
	warn("Recompile perl with -DDEBUGGING to use -D switch\n");
	for (s++; isALNUM(*s); s++) ;
#endif
	/*SUPPRESS 530*/
	return s;
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
	    for (e = s; *e && !isSPACE(*e); e++) ;
	    p = savepvn(s, e-s);
	    incpush(p, TRUE);
	    Safefree(p);
	    s = e;
	}
	else
	    croak("No space allowed after -I");
	return s;
    case 'l':
	PL_minus_l = TRUE;
	s++;
	if (PL_ors)
	    Safefree(PL_ors);
	if (isDIGIT(*s)) {
	    PL_ors = savepv("\n");
	    PL_orslen = 1;
	    *PL_ors = scan_oct(s, 3 + (*s == '0'), &numlen);
	    s += numlen;
	}
	else {
	    dTHR;
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
			croak("Can't use '%c' after -mname", *s);
		    sv_catpv( sv, " ()");
		}
	    } else {
		sv_catpvn(sv, start, s-start);
		sv_catpv(sv, " split(/,/,q{");
		sv_catpv(sv, ++s);
		sv_catpv(sv,    "})");
	    }
	    s += strlen(s);
	    if (PL_preambleav == NULL)
		PL_preambleav = newAV();
	    av_push(PL_preambleav, sv);
	}
	else
	    croak("No space allowed after -%c", *(s-1));
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
	    croak("Too late for \"-T\" option");
	s++;
	return s;
    case 'u':
	PL_do_undump = TRUE;
	s++;
	return s;
    case 'U':
	PL_unsafe = TRUE;
	s++;
	return s;
    case 'v':
#if defined(SUBVERSION) && SUBVERSION > 0
	printf("\nThis is perl, version 5.%03d_%02d built for %s",
	    PATCHLEVEL, SUBVERSION, ARCHNAME);
#else
	printf("\nThis is perl, version %s built for %s",
		PL_patchlevel, ARCHNAME);
#endif
#if defined(LOCAL_PATCH_COUNT)
	if (LOCAL_PATCH_COUNT > 0)
	    printf("\n(with %d registered patch%s, see perl -V for more detail)",
		LOCAL_PATCH_COUNT, (LOCAL_PATCH_COUNT!=1) ? "es" : "");
#endif

	printf("\n\nCopyright 1987-1999, Larry Wall\n");
#ifdef MSDOS
	printf("\nMS-DOS port Copyright (c) 1989, 1990, Diomidis Spinellis\n");
#endif
#ifdef DJGPP
	printf("djgpp v2 port (jpl5003c) by Hirofumi Watanabe, 1996\n");
	printf("djgpp v2 port (perl5004+) by Laszlo Molnar, 1997-1998\n");
#endif
#ifdef OS2
	printf("\n\nOS/2 port Copyright (c) 1990, 1991, Raymond Chen, Kai Uwe Rommel\n"
	    "Version 5 port Copyright (c) 1994-1998, Andreas Kaiser, Ilya Zakharevich\n");
#endif
#ifdef atarist
	printf("atariST series port, ++jrb  bammi@cadence.com\n");
#endif
#ifdef __BEOS__
	printf("BeOS port Copyright Tom Spindler, 1997-1998\n");
#endif
#ifdef MPE
	printf("MPE/iX port Copyright by Mark Klein and Mark Bixby, 1996-1998\n");
#endif
#ifdef OEMVS
	printf("MVS (OS390) port by Mortice Kern Systems, 1997-1998\n");
#endif
#ifdef __VOS__
	printf("Stratus VOS port by Paul_Green@stratus.com, 1997-1999\n");
#endif
#ifdef __MINT__
	printf("MiNT port by Guido Flohr, 1997\n");
#endif
#ifdef BINARY_BUILD_NOTICE
	BINARY_BUILD_NOTICE;
#endif
	printf("\n\
Perl may be copied only under the terms of either the Artistic License or the\n\
GNU General Public License, which may be found in the Perl 5.0 source kit.\n\n\
Complete documentation for Perl, including FAQ lists, should be found on\n\
this system using `man perl' or `perldoc perl'.  If you have access to the\n\
Internet, point your browser at http://www.perl.com/, the Perl Home Page.\n\n");
	PerlProc_exit(0);
    case 'w':
	PL_dowarn = TRUE;
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
	croak("Can't emulate -%.1s on #! line",s);
    }
    return Nullch;
}

/* compliments of Tom Christiansen */

/* unexec() can be found in the Gnu emacs distribution */
/* Known to work with -DUNEXEC and using unexelf.c from GNU emacs-20.2 */

void
my_unexec(void)
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
init_interp(void)
{

#ifdef PERL_OBJECT		/* XXX kludge */
#define I_REINIT \
  STMT_START {				\
    PL_chopset		= " \n-";	\
    PL_copline		= NOLINE;	\
    PL_curcop		= &PL_compiling;\
    PL_curcopdb		= NULL;		\
    PL_dbargs		= 0;		\
    PL_dlmax		= 128;		\
    PL_laststatval	= -1;		\
    PL_laststype	= OP_STAT;	\
    PL_maxscream	= -1;		\
    PL_maxsysfd		= MAXSYSFD;	\
    PL_statname		= Nullsv;	\
    PL_tmps_floor	= -1;		\
    PL_tmps_ix		= -1;		\
    PL_op_mask		= NULL;		\
    PL_dlmax		= 128;		\
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
  } STMT_END
    I_REINIT;
#else
#  ifdef MULTIPLICITY
#    define PERLVAR(var,type)
#    define PERLVARI(var,type,init)	PL_curinterp->var = init;
#    define PERLVARIC(var,type,init)	PL_curinterp->var = init;
#    include "intrpvar.h"
#    ifndef USE_THREADS
#      include "thrdvar.h"
#    endif
#    undef PERLVAR
#    undef PERLVARI
#    undef PERLVARIC
#    else
#    define PERLVAR(var,type)
#    define PERLVARI(var,type,init)	PL_##var = init;
#    define PERLVARIC(var,type,init)	PL_##var = init;
#    include "intrpvar.h"
#    ifndef USE_THREADS
#      include "thrdvar.h"
#    endif
#    undef PERLVAR
#    undef PERLVARI
#    undef PERLVARIC
#  endif
#endif

}

STATIC void
init_main_stash(void)
{
    dTHR;
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
    PL_curstname = newSVpv("main",4);
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
    (void)form("%240s","");	/* Preallocate temp - for immediate signals. */
    sv_grow(ERRSV, 240);	/* Preallocate - for immediate signals. */
    sv_setpvn(ERRSV, "", 0);
    PL_curstash = PL_defstash;
    PL_compiling.cop_stash = PL_defstash;
    PL_debstash = GvHV(gv_fetchpv("DB::", GV_ADDMULTI, SVt_PVHV));
    PL_globalstash = GvHV(gv_fetchpv("CORE::GLOBAL::", GV_ADDMULTI, SVt_PVHV));
    /* We must init $/ before switches are processed. */
    sv_setpvn(perl_get_sv("/", TRUE), "\n", 1);
}

STATIC void
open_script(char *scriptname, bool dosearch, SV *sv, int *fdscript)
{
    dTHR;
    register char *s;

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

    PL_curcop->cop_filegv = gv_fetchfile(PL_origfilename);
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
	SV *cpp = newSVpv("",0);
	SV *cmd = NEWSV(0,0);

	if (strEQ(cpp_cfg, "cppstdin"))
	    sv_catpvf(cpp, "%s/", BIN_EXP);
	sv_catpv(cpp, cpp_cfg);

	sv_catpv(sv,"-I");
	sv_catpv(sv,PRIVLIB_EXP);

#ifdef MSDOS
	sv_setpvf(cmd, "\
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
 %s | %_ -C %_ %s",
	  (PL_doextract ? "-e \"1,/^#/d\n\"" : ""),
#else
	sv_setpvf(cmd, "\
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
 %s | %_ -C %_ %s",
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
		croak("Can't do seteuid!\n");
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
	    PerlLIO_stat(SvPVX(GvSV(PL_curcop->cop_filegv)),&PL_statbuf) >= 0 &&
	    PL_statbuf.st_mode & (S_ISUID|S_ISGID))
	{
	    /* try again */
	    PerlProc_execv(form("%s/sperl%s", BIN_EXP, PL_patchlevel), PL_origargv);
	    croak("Can't do setuid\n");
	}
#endif
#endif
	croak("Can't open perl script \"%s\": %s\n",
	  SvPVX(GvSV(PL_curcop->cop_filegv)), Strerror(errno));
    }
}

#ifdef IAMSUID
static int
fd_on_nosuid_fs(int fd)
{
    int on_nosuid  = 0;
    int check_okay = 0;
/*
 * Preferred order: fstatvfs(), fstatfs(), getmntent().
 * fstatvfs() is UNIX98.
 * fstatfs() is BSD.
 * getmntent() is O(number-of-mounted-filesystems) and can hang.
 */

#   ifdef HAS_FSTATVFS
    struct statvfs stfs;
    check_okay = fstatvfs(fd, &stfs) == 0;
    on_nosuid  = check_okay && (stfs.f_flag  & ST_NOSUID);
#   else
#       if defined(HAS_FSTATFS) && defined(HAS_STRUCT_STATFS_FLAGS)
    struct statfs  stfs;
    check_okay = fstatfs(fd, &stfs)  == 0;
#           undef PERL_MOUNT_NOSUID
#           if !defined(PERL_MOUNT_NOSUID) && defined(MNT_NOSUID)
#              define PERL_MOUNT_NOSUID MNT_NOSUID
#           endif
#           if !defined(PERL_MOUNT_NOSUID) && defined(MS_NOSUID)
#              define PERL_MOUNT_NOSUID MS_NOSUID
#           endif
#           if !defined(PERL_MOUNT_NOSUID) && defined(M_NOSUID)
#              define PERL_MOUNT_NOSUID M_NOSUID
#           endif
#           ifdef PERL_MOUNT_NOSUID
    on_nosuid  = check_okay && (stfs.f_flags & PERL_MOUNT_NOSUID);
#           endif
#       else
#           if defined(HAS_GETMNTENT) && defined(HAS_HASMNTOPT) && defined(MNTOPT_NOSUID)
    FILE		*mtab = fopen("/etc/mtab", "r");
    struct mntent	*entry;
    struct stat		stb, fsb;

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
#           endif /* mntent */
#       endif /* statfs */
#   endif /* statvfs */
    if (!check_okay) 
	croak("Can't check filesystem of script \"%s\" for nosuid",
	      PL_origfilename);
    return on_nosuid;
}
#endif /* IAMSUID */

STATIC void
validate_suid(char *validarg, char *scriptname, int fdscript)
{
    int which;

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
    dTHR;
    char *s, *s2;

    if (PerlLIO_fstat(PerlIO_fileno(PL_rsfp),&PL_statbuf) < 0)	/* normal stat is insecure */
	croak("Can't stat script \"%s\"",PL_origfilename);
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
	if (PerlLIO_access(SvPVX(GvSV(PL_curcop->cop_filegv)),1)) /*double check*/
	    croak("Permission denied");
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
		croak("Can't swap uid and euid");	/* really paranoid */
	    if (PerlLIO_stat(SvPVX(GvSV(PL_curcop->cop_filegv)),&tmpstatbuf) < 0)
		croak("Permission denied");	/* testing full pathname here */
#if defined(IAMSUID) && !defined(NO_NOSUID_CHECK)
	    if (fd_on_nosuid_fs(PerlIO_fileno(PL_rsfp)))
		croak("Permission denied");
#endif
	    if (tmpstatbuf.st_dev != PL_statbuf.st_dev ||
		tmpstatbuf.st_ino != PL_statbuf.st_ino) {
		(void)PerlIO_close(PL_rsfp);
		if (PL_rsfp = PerlProc_popen("/bin/mail root","w")) {	/* heh, heh */
		    PerlIO_printf(PL_rsfp,
"User %ld tried to run dev %ld ino %ld in place of dev %ld ino %ld!\n\
(Filename of set-id script was %s, uid %ld gid %ld.)\n\nSincerely,\nperl\n",
			(long)PL_uid,(long)tmpstatbuf.st_dev, (long)tmpstatbuf.st_ino,
			(long)PL_statbuf.st_dev, (long)PL_statbuf.st_ino,
			SvPVX(GvSV(PL_curcop->cop_filegv)),
			(long)PL_statbuf.st_uid, (long)PL_statbuf.st_gid);
		    (void)PerlProc_pclose(PL_rsfp);
		}
		croak("Permission denied\n");
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
		croak("Can't reswap uid and euid");
	    if (!cando(S_IXUSR,FALSE,&PL_statbuf))		/* can real uid exec? */
		croak("Permission denied\n");
	}
#endif /* HAS_SETREUID */
#endif /* IAMSUID */

	if (!S_ISREG(PL_statbuf.st_mode))
	    croak("Permission denied");
	if (PL_statbuf.st_mode & S_IWOTH)
	    croak("Setuid/gid script is writable by world");
	PL_doswitches = FALSE;		/* -s is insecure in suid */
	PL_curcop->cop_line++;
	if (sv_gets(PL_linestr, PL_rsfp, 0) == Nullch ||
	  strnNE(SvPV(PL_linestr,n_a),"#!",2) )	/* required even on Sys V */
	    croak("No #! line");
	s = SvPV(PL_linestr,n_a)+2;
	if (*s == ' ') s++;
	while (!isSPACE(*s)) s++;
	for (s2 = s;  (s2 > SvPV(PL_linestr,n_a)+2 &&
		       (isDIGIT(s2[-1]) || strchr("._-", s2[-1])));  s2--) ;
	if (strnNE(s2-4,"perl",4) && strnNE(s-9,"perl",4))  /* sanity check */
	    croak("Not a perl script");
	while (*s == ' ' || *s == '\t') s++;
	/*
	 * #! arg must be what we saw above.  They can invoke it by
	 * mentioning suidperl explicitly, but they may not add any strange
	 * arguments beyond what #! says if they do invoke suidperl that way.
	 */
	len = strlen(validarg);
	if (strEQ(validarg," PHOOEY ") ||
	    strnNE(s,validarg,len) || !isSPACE(s[len]))
	    croak("Args must match #! line");

#ifndef IAMSUID
	if (PL_euid != PL_uid && (PL_statbuf.st_mode & S_ISUID) &&
	    PL_euid == PL_statbuf.st_uid)
	    if (!PL_do_undump)
		croak("YOU HAVEN'T DISABLED SET-ID SCRIPTS IN THE KERNEL YET!\n\
FIX YOUR KERNEL, PUT A C WRAPPER AROUND THIS SCRIPT, OR USE -u AND UNDUMP!\n");
#endif /* IAMSUID */

	if (PL_euid) {	/* oops, we're not the setuid root perl */
	    (void)PerlIO_close(PL_rsfp);
#ifndef IAMSUID
	    /* try again */
	    PerlProc_execv(form("%s/sperl%s", BIN_EXP, PL_patchlevel), PL_origargv);
#endif
	    croak("Can't do setuid\n");
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
		croak("Can't do setegid!\n");
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
		croak("Can't do seteuid!\n");
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
		croak("Can't do seteuid!\n");
	}
	init_ids();
	if (!cando(S_IXUSR,TRUE,&PL_statbuf))
	    croak("Permission denied\n");	/* they can't do this */
    }
#ifdef IAMSUID
    else if (PL_preprocess)
	croak("-P not allowed for setuid/setgid script\n");
    else if (fdscript >= 0)
	croak("fd script not allowed in suidperl\n");
    else
	croak("Script is not setuid/setgid in suidperl\n");

    /* We absolutely must clear out any saved ids here, so we */
    /* exec the real perl, substituting fd script for scriptname. */
    /* (We pass script name as "subdir" of fd, which perl will grok.) */
    PerlIO_rewind(PL_rsfp);
    PerlLIO_lseek(PerlIO_fileno(PL_rsfp),(Off_t)0,0);  /* just in case rewind didn't */
    for (which = 1; PL_origargv[which] && PL_origargv[which] != scriptname; which++) ;
    if (!PL_origargv[which])
	croak("Permission denied");
    PL_origargv[which] = savepv(form("/dev/fd/%d/%s",
				  PerlIO_fileno(PL_rsfp), PL_origargv[which]));
#if defined(HAS_FCNTL) && defined(F_SETFD)
    fcntl(PerlIO_fileno(PL_rsfp),F_SETFD,0);	/* ensure no close-on-exec */
#endif
    PerlProc_execv(form("%s/perl%s", BIN_EXP, PL_patchlevel), PL_origargv);/* try again */
    croak("Can't do setuid\n");
#endif /* IAMSUID */
#else /* !DOSUID */
    if (PL_euid != PL_uid || PL_egid != PL_gid) {	/* (suidperl doesn't exist, in fact) */
#ifndef SETUID_SCRIPTS_ARE_SECURE_NOW
	dTHR;
	PerlLIO_fstat(PerlIO_fileno(PL_rsfp),&PL_statbuf);	/* may be either wrapped or real suid */
	if ((PL_euid != PL_uid && PL_euid == PL_statbuf.st_uid && PL_statbuf.st_mode & S_ISUID)
	    ||
	    (PL_egid != PL_gid && PL_egid == PL_statbuf.st_gid && PL_statbuf.st_mode & S_ISGID)
	   )
	    if (!PL_do_undump)
		croak("YOU HAVEN'T DISABLED SET-ID SCRIPTS IN THE KERNEL YET!\n\
FIX YOUR KERNEL, PUT A C WRAPPER AROUND THIS SCRIPT, OR USE -u AND UNDUMP!\n");
#endif /* SETUID_SCRIPTS_ARE_SECURE_NOW */
	/* not set-id, must be wrapped */
    }
#endif /* DOSUID */
}

STATIC void
find_beginning(void)
{
    register char *s, *s2;

    /* skip forward in input to the real script? */

    forbid_setid("-x");
    while (PL_doextract) {
	if ((s = sv_gets(PL_linestr, PL_rsfp, 0)) == Nullch)
	    croak("No Perl script found in input\n");
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
		    while (s = moreswitches(s)) ;
	    }
	    if (PL_cddir && PerlDir_chdir(PL_cddir) < 0)
		croak("Can't chdir to %s",PL_cddir);
	}
    }
}


STATIC void
init_ids(void)
{
    PL_uid = (int)PerlProc_getuid();
    PL_euid = (int)PerlProc_geteuid();
    PL_gid = (int)PerlProc_getgid();
    PL_egid = (int)PerlProc_getegid();
#ifdef VMS
    PL_uid |= PL_gid << 16;
    PL_euid |= PL_egid << 16;
#endif
    PL_tainting |= (PL_uid && (PL_euid != PL_uid || PL_egid != PL_gid));
}

STATIC void
forbid_setid(char *s)
{
    if (PL_euid != PL_uid)
        croak("No %s allowed while running setuid", s);
    if (PL_egid != PL_gid)
        croak("No %s allowed while running setgid", s);
}

STATIC void
init_debugger(void)
{
    dTHR;
    PL_curstash = PL_debstash;
    PL_dbargs = GvAV(gv_AVadd((gv_fetchpv("args", GV_ADDMULTI, SVt_PVAV))));
    AvREAL_off(PL_dbargs);
    PL_DBgv = gv_fetchpv("DB", GV_ADDMULTI, SVt_PVGV);
    PL_DBline = gv_fetchpv("dbline", GV_ADDMULTI, SVt_PVAV);
    PL_DBsub = gv_HVadd(gv_fetchpv("sub", GV_ADDMULTI, SVt_PVHV));
    PL_DBsingle = GvSV((gv_fetchpv("single", GV_ADDMULTI, SVt_PV)));
    sv_setiv(PL_DBsingle, 0); 
    PL_DBtrace = GvSV((gv_fetchpv("trace", GV_ADDMULTI, SVt_PV)));
    sv_setiv(PL_DBtrace, 0); 
    PL_DBsignal = GvSV((gv_fetchpv("signal", GV_ADDMULTI, SVt_PV)));
    sv_setiv(PL_DBsignal, 0); 
    PL_curstash = PL_defstash;
}

#ifndef STRESS_REALLOC
#define REASONABLE(size) (size)
#else
#define REASONABLE(size) (1) /* unreasonable */
#endif

void
init_stacks(ARGSproto)
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

    SET_MARKBASE;

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
nuke_stacks(void)
{
    dTHR;
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
    DEBUG( {
	Safefree(PL_debname);
	Safefree(PL_debdelim);
    } )
}

#ifndef PERL_OBJECT
static PerlIO *tmpfp;  /* moved outside init_lexer() because of UNICOS bug */
#endif

STATIC void
init_lexer(void)
{
#ifdef PERL_OBJECT
	PerlIO *tmpfp;
#endif
    tmpfp = PL_rsfp;
    PL_rsfp = Nullfp;
    lex_start(PL_linestr);
    PL_rsfp = tmpfp;
    PL_subname = newSVpv("main",4);
}

STATIC void
init_predump_symbols(void)
{
    dTHR;
    GV *tmpgv;
    GV *othergv;

    sv_setpvn(perl_get_sv("\"", TRUE), " ", 1);
    PL_stdingv = gv_fetchpv("STDIN",TRUE, SVt_PVIO);
    GvMULTI_on(PL_stdingv);
    IoIFP(GvIOp(PL_stdingv)) = PerlIO_stdin();
    tmpgv = gv_fetchpv("stdin",TRUE, SVt_PV);
    GvMULTI_on(tmpgv);
    GvIOp(tmpgv) = (IO*)SvREFCNT_inc(GvIOp(PL_stdingv));

    tmpgv = gv_fetchpv("STDOUT",TRUE, SVt_PVIO);
    GvMULTI_on(tmpgv);
    IoOFP(GvIOp(tmpgv)) = IoIFP(GvIOp(tmpgv)) = PerlIO_stdout();
    setdefout(tmpgv);
    tmpgv = gv_fetchpv("stdout",TRUE, SVt_PV);
    GvMULTI_on(tmpgv);
    GvIOp(tmpgv) = (IO*)SvREFCNT_inc(GvIOp(PL_defoutgv));

    othergv = gv_fetchpv("STDERR",TRUE, SVt_PVIO);
    GvMULTI_on(othergv);
    IoOFP(GvIOp(othergv)) = IoIFP(GvIOp(othergv)) = PerlIO_stderr();
    tmpgv = gv_fetchpv("stderr",TRUE, SVt_PV);
    GvMULTI_on(tmpgv);
    GvIOp(tmpgv) = (IO*)SvREFCNT_inc(GvIOp(othergv));

    PL_statname = NEWSV(66,0);		/* last filename we did stat on */

    if (!PL_osname)
	PL_osname = savepv(OSNAME);
}

STATIC void
init_postdump_symbols(register int argc, register char **argv, register char **env)
{
    dTHR;
    char *s;
    SV *sv;
    GV* tmpgv;

    argc--,argv++;	/* skip name of script */
    if (PL_doswitches) {
	for (; argc > 0 && **argv == '-'; argc--,argv++) {
	    if (!argv[0][1])
		break;
	    if (argv[0][1] == '-') {
		argc--,argv++;
		break;
	    }
	    if (s = strchr(argv[0], '=')) {
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
    if (tmpgv = gv_fetchpv("0",TRUE, SVt_PV)) {
	sv_setpv(GvSV(tmpgv),PL_origfilename);
	magicname("0", "0", 1);
    }
    if (tmpgv = gv_fetchpv("\030",TRUE, SVt_PV))
	sv_setpv(GvSV(tmpgv),PL_origargv[0]);
    if (PL_argvgv = gv_fetchpv("ARGV",TRUE, SVt_PVAV)) {
	GvMULTI_on(PL_argvgv);
	(void)gv_AVadd(PL_argvgv);
	av_clear(GvAVn(PL_argvgv));
	for (; argc > 0; argc--,argv++) {
	    av_push(GvAVn(PL_argvgv),newSVpv(argv[0],0));
	}
    }
    if (PL_envgv = gv_fetchpv("ENV",TRUE, SVt_PVHV)) {
	HV *hv;
	GvMULTI_on(PL_envgv);
	hv = GvHVn(PL_envgv);
	hv_magic(hv, PL_envgv, 'E');
#ifndef VMS  /* VMS doesn't have environ array */
	/* Note that if the supplied env parameter is actually a copy
	   of the global environ then it may now point to free'd memory
	   if the environment has been modified since. To avoid this
	   problem we treat env==NULL as meaning 'use the default'
	*/
	if (!env)
	    env = environ;
	if (env != environ)
	    environ[0] = Nullch;
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
#if defined(__BORLANDC__) && defined(USE_WIN32_RTL_ENV)
	    /* Sins of the RTL. See note in my_setenv(). */
	    (void)PerlEnv_putenv(savepv(*env));
#endif
	}
#endif
#ifdef DYNAMIC_ENV_FETCH
	HvNAME(hv) = savepv(ENV_HV_NAME);
#endif
    }
    TAINT_NOT;
    if (tmpgv = gv_fetchpv("$",TRUE, SVt_PV))
	sv_setiv(GvSV(tmpgv), (IV)getpid());
}

STATIC void
init_perllib(void)
{
    char *s;
    if (!PL_tainting) {
#ifndef VMS
	s = PerlEnv_getenv("PERL5LIB");
	if (s)
	    incpush(s, TRUE);
	else
	    incpush(PerlEnv_getenv("PERLLIB"), FALSE);
#else /* VMS */
	/* Treat PERL5?LIB as a possible search list logical name -- the
	 * "natural" VMS idiom for a Unix path string.  We allow each
	 * element to be a set of |-separated directories for compatibility.
	 */
	char buf[256];
	int idx = 0;
	if (my_trnlnm("PERL5LIB",buf,0))
	    do { incpush(buf,TRUE); } while (my_trnlnm("PERL5LIB",buf,++idx));
	else
	    while (my_trnlnm("PERLLIB",buf,idx++)) incpush(buf,FALSE);
#endif /* VMS */
    }

/* Use the ~-expanded versions of APPLLIB (undocumented),
    ARCHLIB PRIVLIB SITEARCH and SITELIB 
*/
#ifdef APPLLIB_EXP
    incpush(APPLLIB_EXP, TRUE);
#endif

#ifdef ARCHLIB_EXP
    incpush(ARCHLIB_EXP, FALSE);
#endif
#ifndef PRIVLIB_EXP
#define PRIVLIB_EXP "/usr/local/lib/perl5:/usr/local/lib/perl"
#endif
#if defined(WIN32) 
    incpush(PRIVLIB_EXP, TRUE);
#else
    incpush(PRIVLIB_EXP, FALSE);
#endif

#ifdef SITEARCH_EXP
    incpush(SITEARCH_EXP, FALSE);
#endif
#ifdef SITELIB_EXP
#if defined(WIN32) 
    incpush(SITELIB_EXP, TRUE);
#else
    incpush(SITELIB_EXP, FALSE);
#endif
#endif
    if (!PL_tainting)
	incpush(".", FALSE);
}

#if defined(DOSISH)
#    define PERLLIB_SEP ';'
#else
#  if defined(VMS)
#    define PERLLIB_SEP '|'
#  else
#    define PERLLIB_SEP ':'
#  endif
#endif
#ifndef PERLLIB_MANGLE
#  define PERLLIB_MANGLE(s,n) (s)
#endif 

STATIC void
incpush(char *p, int addsubdirs)
{
    SV *subdir = Nullsv;

    if (!p)
	return;

    if (addsubdirs) {
	subdir = sv_newmortal();
	if (!PL_archpat_auto) {
	    STRLEN len = (sizeof(ARCHNAME) + strlen(PL_patchlevel)
			  + sizeof("//auto"));
	    New(55, PL_archpat_auto, len, char);
	    sprintf(PL_archpat_auto, "/%s/%s/auto", ARCHNAME, PL_patchlevel);
#ifdef VMS
	for (len = sizeof(ARCHNAME) + 2;
	     PL_archpat_auto[len] != '\0' && PL_archpat_auto[len] != '/'; len++)
		if (PL_archpat_auto[len] == '.') PL_archpat_auto[len] = '_';
#endif
	}
    }

    /* Break at all separators */
    while (p && *p) {
	SV *libdir = NEWSV(55,0);
	char *s;

	/* skip any consecutive separators */
	while ( *p == PERLLIB_SEP ) {
	    /* Uncomment the next line for PATH semantics */
	    /* av_push(GvAVn(PL_incgv), newSVpv(".", 1)); */
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

	/*
	 * BEFORE pushing libdir onto @INC we may first push version- and
	 * archname-specific sub-directories.
	 */
	if (addsubdirs) {
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
		PerlIO_printf(PerlIO_stderr(),
		              "Failed to unixify @INC element \"%s\"\n",
			      SvPV(libdir,len));
#endif
	    /* .../archname/version if -d .../archname/version/auto */
	    sv_setsv(subdir, libdir);
	    sv_catpv(subdir, PL_archpat_auto);
	    if (PerlLIO_stat(SvPVX(subdir), &tmpstatbuf) >= 0 &&
		  S_ISDIR(tmpstatbuf.st_mode))
		av_push(GvAVn(PL_incgv),
			newSVpv(SvPVX(subdir), SvCUR(subdir) - sizeof "auto"));

	    /* .../archname if -d .../archname/auto */
	    sv_insert(subdir, SvCUR(libdir) + sizeof(ARCHNAME),
		      strlen(PL_patchlevel) + 1, "", 0);
	    if (PerlLIO_stat(SvPVX(subdir), &tmpstatbuf) >= 0 &&
		  S_ISDIR(tmpstatbuf.st_mode))
		av_push(GvAVn(PL_incgv),
			newSVpv(SvPVX(subdir), SvCUR(subdir) - sizeof "auto"));
	}

	/* finally push this lib directory on the end of @INC */
	av_push(GvAVn(PL_incgv), libdir);
    }
}

#ifdef USE_THREADS
STATIC struct perl_thread *
init_main_thread()
{
    struct perl_thread *thr;
    XPV *xpv;

    Newz(53, thr, 1, struct perl_thread);
    PL_curcop = &PL_compiling;
    thr->cvcache = newHV();
    thr->threadsv = newAV();
    /* thr->threadsvp is set when find_threadsv is called */
    thr->specific = newAV();
    thr->errhv = newHV();
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

    MUTEX_LOCK(&PL_threads_mutex);
    PL_nthreads++;
    thr->tid = 0;
    thr->next = thr;
    thr->prev = thr;
    MUTEX_UNLOCK(&PL_threads_mutex);

#ifdef HAVE_THREAD_INTERN
    init_thread_intern(thr);
#endif

#ifdef SET_THREAD_SELF
    SET_THREAD_SELF(thr);
#else
    thr->self = pthread_self();
#endif /* SET_THREAD_SELF */
    SET_THR(thr);

    /*
     * These must come after the SET_THR because sv_setpvn does
     * SvTAINT and the taint fields require dTHR.
     */
    PL_toptarget = NEWSV(0,0);
    sv_upgrade(PL_toptarget, SVt_PVFM);
    sv_setpvn(PL_toptarget, "", 0);
    PL_bodytarget = NEWSV(0,0);
    sv_upgrade(PL_bodytarget, SVt_PVFM);
    sv_setpvn(PL_bodytarget, "", 0);
    PL_formtarget = PL_bodytarget;
    thr->errsv = newSVpv("", 0);
    (void) find_threadsv("@");	/* Ensure $@ is initialised early */

    PL_maxscream = -1;
    PL_regcompp = FUNC_NAME_TO_PTR(pregcomp);
    PL_regexecp = FUNC_NAME_TO_PTR(regexec_flags);
    PL_regindent = 0;
    PL_reginterp_cnt = 0;

    return thr;
}
#endif /* USE_THREADS */

void
call_list(I32 oldscope, AV *paramList)
{
    dTHR;
    line_t oldline = PL_curcop->cop_line;
    STRLEN len;
    dJMPENV;
    int ret;

    while (AvFILL(paramList) >= 0) {
	CV *cv = (CV*)av_shift(paramList);

	SAVEFREESV(cv);

	JMPENV_PUSH(ret);
	switch (ret) {
	case 0: {
		SV* atsv = ERRSV;
		PUSHMARK(PL_stack_sp);
		perl_call_sv((SV*)cv, G_EVAL|G_DISCARD);
		(void)SvPV(atsv, len);
		if (len) {
		    JMPENV_POP;
		    PL_curcop = &PL_compiling;
		    PL_curcop->cop_line = oldline;
		    if (paramList == PL_beginav)
			sv_catpv(atsv, "BEGIN failed--compilation aborted");
		    else
			sv_catpv(atsv, "END failed--cleanup aborted");
		    while (PL_scopestack_ix > oldscope)
			LEAVE;
		    croak("%s", SvPVX(atsv));
		}
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
	    if (PL_endav)
		call_list(oldscope, PL_endav);
	    JMPENV_POP;
	    PL_curcop = &PL_compiling;
	    PL_curcop->cop_line = oldline;
	    if (PL_statusvalue) {
		if (paramList == PL_beginav)
		    croak("BEGIN failed--compilation aborted");
		else
		    croak("END failed--cleanup aborted");
	    }
	    my_exit_jump();
	    /* NOTREACHED */
	case 3:
	    if (!PL_restartop) {
		PerlIO_printf(PerlIO_stderr(), "panic: restartop\n");
		FREETMPS;
		break;
	    }
	    JMPENV_POP;
	    PL_curcop = &PL_compiling;
	    PL_curcop->cop_line = oldline;
	    JMPENV_JUMP(3);
	}
	JMPENV_POP;
    }
}

void
my_exit(U32 status)
{
    dTHR;

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
my_failure_exit(void)
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
my_exit_jump(void)
{
    dSP;
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
#define NO_XSLOCKS
#endif  /* PERL_OBJECT */

#include "XSUB.h"

static I32
#ifdef PERL_OBJECT
read_e_script(CPerlObj *pPerl, int idx, SV *buf_sv, int maxlen)
#else
read_e_script(int idx, SV *buf_sv, int maxlen)
#endif
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


