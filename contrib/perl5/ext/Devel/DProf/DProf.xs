#define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

/*#define DBG_SUB 1      */
/*#define DBG_TIMER 1    */

#ifdef DBG_SUB
#  define DBG_SUB_NOTIFY(A,B) warn(A, B)
#else
#  define DBG_SUB_NOTIFY(A,B)  /* nothing */
#endif

#ifdef DBG_TIMER
#  define DBG_TIMER_NOTIFY(A) warn(A)
#else
#  define DBG_TIMER_NOTIFY(A)  /* nothing */
#endif

/* HZ == clock ticks per second */
#ifdef VMS
#  define HZ ((I32)CLK_TCK)
#  define DPROF_HZ HZ
#  include <starlet.h>  /* prototype for sys$gettim() */
#  include <lib$routines.h>
#  define Times(ptr) (dprof_times(aTHX_ ptr))
#else
#  ifndef HZ
#    ifdef CLK_TCK
#      define HZ ((I32)CLK_TCK)
#    else
#      define HZ 60
#    endif
#  endif
#  ifdef OS2				/* times() has significant overhead */
#    define Times(ptr) (dprof_times(aTHX_ ptr))
#    define INCL_DOSPROFILE
#    define INCL_DOSERRORS
#    include <os2.h>
#    define toLongLong(arg) (*(long long*)&(arg))
#    define DPROF_HZ g_dprof_ticks
#  else
#    define Times(ptr) (times(ptr))
#    define DPROF_HZ HZ
#  endif 
#endif

XS(XS_Devel__DProf_END);        /* used by prof_mark() */

/* Everything is built on times(2).  See its manpage for a description
 * of the timings.
 */

union prof_any {
        clock_t tms_utime;  /* cpu time spent in user space */
        clock_t tms_stime;  /* cpu time spent in system */
        clock_t realtime;   /* elapsed real time, in ticks */
        char *name;
        U32 id;
        opcode ptype;
};

typedef union prof_any PROFANY;

typedef struct {
    U32		dprof_ticks;
    char*	out_file_name;	/* output file (defaults to tmon.out) */
    PerlIO*	fp;		/* pointer to tmon.out file */
    long	TIMES_LOCATION;	/* Where in the file to store the time totals */
    int		SAVE_STACK;	/* How much data to buffer until end of run */
    int		prof_pid;	/* pid of profiled process */
    struct tms	prof_start;
    struct tms	prof_end;
    clock_t	rprof_start;	/* elapsed real time ticks */
    clock_t	rprof_end;
    clock_t	wprof_u;
    clock_t	wprof_s;
    clock_t	wprof_r;
    clock_t	otms_utime;
    clock_t	otms_stime;
    clock_t	orealtime;
    PROFANY*	profstack;
    int		profstack_max;
    int		profstack_ix;
    HV*		cv_hash;
    U32		total;
    U32		lastid;
    U32		default_perldb;
    U32		depth;
#ifdef OS2
    ULONG	frequ;
    long long	start_cnt;
#endif
#ifdef PERL_IMPLICIT_CONTEXT
#  define register
    pTHX;
#  undef register
#endif
} prof_state_t;

prof_state_t g_prof_state;

#define g_dprof_ticks		g_prof_state.dprof_ticks
#define g_out_file_name		g_prof_state.out_file_name
#define g_fp			g_prof_state.fp
#define g_TIMES_LOCATION	g_prof_state.TIMES_LOCATION
#define g_SAVE_STACK		g_prof_state.SAVE_STACK
#define g_prof_pid		g_prof_state.prof_pid
#define g_prof_start		g_prof_state.prof_start
#define g_prof_end		g_prof_state.prof_end
#define g_rprof_start		g_prof_state.rprof_start
#define g_rprof_end		g_prof_state.rprof_end
#define g_wprof_u		g_prof_state.wprof_u
#define g_wprof_s		g_prof_state.wprof_s
#define g_wprof_r		g_prof_state.wprof_r
#define g_otms_utime		g_prof_state.otms_utime
#define g_otms_stime		g_prof_state.otms_stime
#define g_orealtime		g_prof_state.orealtime
#define g_profstack		g_prof_state.profstack
#define g_profstack_max		g_prof_state.profstack_max
#define g_profstack_ix		g_prof_state.profstack_ix
#define g_cv_hash		g_prof_state.cv_hash
#define g_total			g_prof_state.total
#define g_lastid		g_prof_state.lastid
#define g_default_perldb	g_prof_state.default_perldb
#define g_depth			g_prof_state.depth
#ifdef PERL_IMPLICIT_CONTEXT
#  define g_THX			g_prof_state.aTHX
#endif
#ifdef OS2
#  define g_frequ		g_prof_state.frequ
#  define g_start_cnt		g_prof_state.start_cnt
#endif

clock_t
dprof_times(pTHX_ struct tms *t)
{
#ifdef OS2
    ULONG rc;
    QWORD cnt;
    STRLEN n_a;
    
    if (!g_frequ) {
	if (CheckOSError(DosTmrQueryFreq(&g_frequ)))
	    croak("DosTmrQueryFreq: %s", SvPV(perl_get_sv("!",TRUE),n_a));
	else
	    g_frequ = g_frequ/DPROF_HZ;	/* count per tick */
	if (CheckOSError(DosTmrQueryTime(&cnt)))
	    croak("DosTmrQueryTime: %s",
		  SvPV(perl_get_sv("!",TRUE), n_a));
	g_start_cnt = toLongLong(cnt);
    }

    if (CheckOSError(DosTmrQueryTime(&cnt)))
	    croak("DosTmrQueryTime: %s", SvPV(perl_get_sv("!",TRUE), n_a));
    t->tms_stime = 0;
    return (t->tms_utime = (toLongLong(cnt) - g_start_cnt)/g_frequ);
#else		/* !OS2 */
#  ifdef VMS
    clock_t retval;
    /* Get wall time and convert to 10 ms intervals to
     * produce the return value dprof expects */
#    if defined(__DECC) && defined (__ALPHA)
#      include <ints.h>
    uint64 vmstime;
    _ckvmssts(sys$gettim(&vmstime));
    vmstime /= 100000;
    retval = vmstime & 0x7fffffff;
#    else
    /* (Older hw or ccs don't have an atomic 64-bit type, so we
     * juggle 32-bit ints (and a float) to produce a time_t result
     * with minimal loss of information.) */
    long int vmstime[2],remainder,divisor = 100000;
    _ckvmssts(sys$gettim((unsigned long int *)vmstime));
    vmstime[1] &= 0x7fff;  /* prevent overflow in EDIV */
    _ckvmssts(lib$ediv(&divisor,vmstime,(long int *)&retval,&remainder));
#    endif
    /* Fill in the struct tms using the CRTL routine . . .*/
    times((tbuffer_t *)t);
    return (clock_t) retval;
#  else		/* !VMS && !OS2 */
    return times(t);
#  endif
#endif
}

static void
prof_dumpa(pTHX_ opcode ptype, U32 id)
{
    if (ptype == OP_LEAVESUB) {
	PerlIO_printf(g_fp,"- %"UVxf"\n", (UV)id);
    }
    else if(ptype == OP_ENTERSUB) {
	PerlIO_printf(g_fp,"+ %"UVxf"\n", (UV)id);
    }
    else if(ptype == OP_GOTO) {
	PerlIO_printf(g_fp,"* %"UVxf"\n", (UV)id);
    }
    else if(ptype == OP_DIE) {
	PerlIO_printf(g_fp,"/ %"UVxf"\n", (UV)id);
    }
    else {
	PerlIO_printf(g_fp,"Profiler unknown prof code %d\n", ptype);
    }
}   

static void
prof_dumps(pTHX_ U32 id, char *pname, char *gname)
{
    PerlIO_printf(g_fp,"& %"UVxf" %s %s\n", (UV)id, pname, gname);
}   

static void
prof_dumpt(pTHX_ long tms_utime, long tms_stime, long realtime)
{
    PerlIO_printf(g_fp,"@ %ld %ld %ld\n", tms_utime, tms_stime, realtime);
}   

static void
prof_dump_until(pTHX_ long ix)
{
    long base = 0;
    struct tms t1, t2;
    clock_t realtime1, realtime2;

    realtime1 = Times(&t1);

    while (base < ix) {
	opcode ptype = g_profstack[base++].ptype;
	if (ptype == OP_TIME) {
	    long tms_utime = g_profstack[base++].tms_utime;
	    long tms_stime = g_profstack[base++].tms_stime;
	    long realtime = g_profstack[base++].realtime;

	    prof_dumpt(aTHX_ tms_utime, tms_stime, realtime);
	}
	else if (ptype == OP_GV) {
	    U32 id = g_profstack[base++].id;
	    char *pname = g_profstack[base++].name;
	    char *gname = g_profstack[base++].name;

	    prof_dumps(aTHX_ id, pname, gname);
	}
	else {
	    U32 id = g_profstack[base++].id;
	    prof_dumpa(aTHX_ ptype, id);
	}
    }
    PerlIO_flush(g_fp);
    realtime2 = Times(&t2);
    if (realtime2 != realtime1 || t1.tms_utime != t2.tms_utime
	|| t1.tms_stime != t2.tms_stime) {
	g_wprof_r += realtime2 - realtime1;
	g_wprof_u += t2.tms_utime - t1.tms_utime;
	g_wprof_s += t2.tms_stime - t1.tms_stime;

	PerlIO_printf(g_fp,"+ & Devel::DProf::write\n");
	PerlIO_printf(g_fp,"@ %"IVdf" %"IVdf" %"IVdf"\n", 
		      /* The (IV) casts are one possibility:
		       * the Painfully Correct Way would be to
		       * have Clock_t_f. */
		      (IV)(t2.tms_utime - t1.tms_utime),
		      (IV)(t2.tms_stime - t1.tms_stime), 
		      (IV)(realtime2 - realtime1));
	PerlIO_printf(g_fp,"- & Devel::DProf::write\n");
	g_otms_utime = t2.tms_utime;
	g_otms_stime = t2.tms_stime;
	g_orealtime = realtime2;
	PerlIO_flush(g_fp);
    }
}

static void
prof_mark(pTHX_ opcode ptype)
{
    struct tms t;
    clock_t realtime, rdelta, udelta, sdelta;
    U32 id;
    SV *Sub = GvSV(PL_DBsub);	/* name of current sub */

    if (g_SAVE_STACK) {
	if (g_profstack_ix + 5 > g_profstack_max) {
		g_profstack_max = g_profstack_max * 3 / 2;
		Renew(g_profstack, g_profstack_max, PROFANY);
	}
    }

    realtime = Times(&t);
    rdelta = realtime - g_orealtime;
    udelta = t.tms_utime - g_otms_utime;
    sdelta = t.tms_stime - g_otms_stime;
    if (rdelta || udelta || sdelta) {
	if (g_SAVE_STACK) {
	    g_profstack[g_profstack_ix++].ptype = OP_TIME;
	    g_profstack[g_profstack_ix++].tms_utime = udelta;
	    g_profstack[g_profstack_ix++].tms_stime = sdelta;
	    g_profstack[g_profstack_ix++].realtime = rdelta;
	}
	else { /* Write it to disk now so's not to eat up core */
	    if (g_prof_pid == (int)getpid()) {
		prof_dumpt(aTHX_ udelta, sdelta, rdelta);
		PerlIO_flush(g_fp);
	    }
	}
	g_orealtime = realtime;
	g_otms_stime = t.tms_stime;
	g_otms_utime = t.tms_utime;
    }

    {
	SV **svp;
	char *gname, *pname;
	CV *cv;

	cv = INT2PTR(CV*,SvIVX(Sub));
	svp = hv_fetch(g_cv_hash, (char*)&cv, sizeof(CV*), TRUE);
	if (!SvOK(*svp)) {
	    GV *gv = CvGV(cv);
		
	    sv_setiv(*svp, id = ++g_lastid);
	    pname = ((GvSTASH(gv) && HvNAME(GvSTASH(gv))) 
		     ? HvNAME(GvSTASH(gv)) 
		     : "(null)");
	    gname = GvNAME(gv);
	    if (CvXSUB(cv) == XS_Devel__DProf_END)
		return;
	    if (g_SAVE_STACK) { /* Store it for later recording  -JH */
		g_profstack[g_profstack_ix++].ptype = OP_GV;
		g_profstack[g_profstack_ix++].id = id;
		g_profstack[g_profstack_ix++].name = pname;
		g_profstack[g_profstack_ix++].name = gname;
	    }
	    else { /* Write it to disk now so's not to eat up core */
		/* Only record the parent's info */
		if (g_prof_pid == (int)getpid()) {
		    prof_dumps(aTHX_ id, pname, gname);
		    PerlIO_flush(g_fp);
		}
		else
		    PL_perldb = 0;		/* Do not debug the kid. */
	    }
	}
	else {
	    id = SvIV(*svp);
	}
    }

    g_total++;
    if (g_SAVE_STACK) { /* Store it for later recording  -JH */
	g_profstack[g_profstack_ix++].ptype = ptype;
	g_profstack[g_profstack_ix++].id = id;

	/* Only record the parent's info */
	if (g_SAVE_STACK < g_profstack_ix) {
	    if (g_prof_pid == (int)getpid())
		prof_dump_until(aTHX_ g_profstack_ix);
	    else
		PL_perldb = 0;		/* Do not debug the kid. */
	    g_profstack_ix = 0;
	}
    }
    else { /* Write it to disk now so's not to eat up core */

	/* Only record the parent's info */
	if (g_prof_pid == (int)getpid()) {
	    prof_dumpa(aTHX_ ptype, id);
	    PerlIO_flush(g_fp);
	}
	else
	    PL_perldb = 0;		/* Do not debug the kid. */
    }
}

#ifdef PL_NEEDED
#  define defstash PL_defstash
#endif

/* Counts overhead of prof_mark and extra XS call. */
static void
test_time(pTHX_ clock_t *r, clock_t *u, clock_t *s)
{
    CV *cv = perl_get_cv("Devel::DProf::NONESUCH_noxs", FALSE);
    int i, j, k = 0;
    HV *oldstash = PL_curstash;
    struct tms t1, t2;
    clock_t realtime1, realtime2;
    U32 ototal = g_total;
    U32 ostack = g_SAVE_STACK;
    U32 operldb = PL_perldb;

    g_SAVE_STACK = 1000000;
    realtime1 = Times(&t1);
    
    while (k < 2) {
	i = 0;
	    /* Disable debugging of perl_call_sv on second pass: */
	PL_curstash = (k == 0 ? PL_defstash : PL_debstash);
	PL_perldb = g_default_perldb;
	while (++i <= 100) {
	    j = 0;
	    g_profstack_ix = 0;		/* Do not let the stack grow */
	    while (++j <= 100) {
/* 		prof_mark(aTHX_ OP_ENTERSUB); */

		PUSHMARK(PL_stack_sp);
		perl_call_sv((SV*)cv, G_SCALAR);
		PL_stack_sp--;
/* 		prof_mark(aTHX_ OP_LEAVESUB); */
	    }
	}
	PL_curstash = oldstash;
	if (k == 0) {			/* Put time with debugging */
	    realtime2 = Times(&t2);
	    *r = realtime2 - realtime1;
	    *u = t2.tms_utime - t1.tms_utime;
	    *s = t2.tms_stime - t1.tms_stime;
	}
	else {				/* Subtract time without debug */
	    realtime1 = Times(&t1);
	    *r -= realtime1 - realtime2;
	    *u -= t1.tms_utime - t2.tms_utime;
	    *s -= t1.tms_stime - t2.tms_stime;	    
	}
	k++;
    }
    g_total = ototal;
    g_SAVE_STACK = ostack;
    PL_perldb = operldb;
}

static void
prof_recordheader(pTHX)
{
    clock_t r, u, s;

    /* g_fp is opened in the BOOT section */
    PerlIO_printf(g_fp, "#fOrTyTwO\n");
    PerlIO_printf(g_fp, "$hz=%"IVdf";\n", (IV)DPROF_HZ);
    PerlIO_printf(g_fp, "$XS_VERSION='DProf %s';\n", XS_VERSION);
    PerlIO_printf(g_fp, "# All values are given in HZ\n");
    test_time(aTHX_ &r, &u, &s);
    PerlIO_printf(g_fp,
		  "$over_utime=%"IVdf"; $over_stime=%"IVdf"; $over_rtime=%"IVdf";\n",
		  /* The (IV) casts are one possibility:
		   * the Painfully Correct Way would be to
		   * have Clock_t_f. */
		  (IV)u, (IV)s, (IV)r);
    PerlIO_printf(g_fp, "$over_tests=10000;\n");

    g_TIMES_LOCATION = PerlIO_tell(g_fp);

    /* Pad with whitespace. */
    /* This should be enough even for very large numbers. */
    PerlIO_printf(g_fp, "%*s\n", 240 , "");

    PerlIO_printf(g_fp, "\n");
    PerlIO_printf(g_fp, "PART2\n");

    PerlIO_flush(g_fp);
}

static void
prof_record(pTHX)
{
    /* g_fp is opened in the BOOT section */

    /* Now that we know the runtimes, fill them in at the recorded
       location -JH */

    if (g_SAVE_STACK) {
	prof_dump_until(aTHX_ g_profstack_ix);
    }
    PerlIO_seek(g_fp, g_TIMES_LOCATION, SEEK_SET);
    /* Write into reserved 240 bytes: */
    PerlIO_printf(g_fp,
		  "$rrun_utime=%"IVdf"; $rrun_stime=%"IVdf"; $rrun_rtime=%"IVdf";",
		  /* The (IV) casts are one possibility:
		   * the Painfully Correct Way would be to
		   * have Clock_t_f. */
		  (IV)(g_prof_end.tms_utime-g_prof_start.tms_utime-g_wprof_u),
		  (IV)(g_prof_end.tms_stime-g_prof_start.tms_stime-g_wprof_s),
		  (IV)(g_rprof_end-g_rprof_start-g_wprof_r));
    PerlIO_printf(g_fp, "\n$total_marks=%"IVdf, (IV)g_total);
    
    PerlIO_close(g_fp);
}

#define NONESUCH()

static void
check_depth(pTHX_ void *foo)
{
    U32 need_depth = PTR2UV(foo);
    if (need_depth != g_depth) {
	if (need_depth > g_depth) {
	    warn("garbled call depth when profiling");
	}
	else {
	    I32 marks = g_depth - need_depth;

/* 	    warn("Check_depth: got %d, expected %d\n", g_depth, need_depth); */
	    while (marks--) {
		prof_mark(aTHX_ OP_DIE);
	    }
	    g_depth = need_depth;
	}
    }
}

#define for_real
#ifdef for_real

XS(XS_DB_sub)
{
    dXSARGS;
    dORIGMARK;
    SV *Sub = GvSV(PL_DBsub);		/* name of current sub */

#ifdef PERL_IMPLICIT_CONTEXT
    /* profile only the interpreter that loaded us */
    if (g_THX != aTHX) {
        PUSHMARK(ORIGMARK);
        perl_call_sv(INT2PTR(SV*,SvIV(Sub)), GIMME | G_NODEBUG);
    }
    else
#endif
    {
	HV *oldstash = PL_curstash;

        DBG_SUB_NOTIFY("XS DBsub(%s)\n", SvPV_nolen(Sub));

	SAVEDESTRUCTOR_X(check_depth, (void*)g_depth);
	g_depth++;

        prof_mark(aTHX_ OP_ENTERSUB);
        PUSHMARK(ORIGMARK);
        perl_call_sv(INT2PTR(SV*,SvIV(Sub)), GIMME | G_NODEBUG);
        PL_curstash = oldstash;
        prof_mark(aTHX_ OP_LEAVESUB);
	g_depth--;
    }
    return;
}

XS(XS_DB_goto)
{
#ifdef PERL_IMPLICIT_CONTEXT
    if (g_THX == aTHX)
#endif
    {
        prof_mark(aTHX_ OP_GOTO);
        return;
    }
}

#endif /* for_real */

#ifdef testing

        MODULE = Devel::DProf           PACKAGE = DB

        void
        sub(...)
	PPCODE:
	    {
                dORIGMARK;
                HV *oldstash = PL_curstash;
		SV *Sub = GvSV(PL_DBsub);	/* name of current sub */
                /* SP -= items;  added by xsubpp */
                DBG_SUB_NOTIFY("XS DBsub(%s)\n", SvPV_nolen(Sub));

                sv_setiv(PL_DBsingle, 0);	/* disable DB single-stepping */

                prof_mark(aTHX_ OP_ENTERSUB);
                PUSHMARK(ORIGMARK);

                PL_curstash = PL_debstash;	/* To disable debugging of perl_call_sv */
                perl_call_sv(Sub, GIMME);
                PL_curstash = oldstash;

                prof_mark(aTHX_ OP_LEAVESUB);
                SPAGAIN;
                /* PUTBACK;  added by xsubpp */
	    }

#endif /* testing */

MODULE = Devel::DProf           PACKAGE = Devel::DProf

void
END()
PPCODE:
    {
        if (PL_DBsub) {
	    /* maybe the process forked--we want only
	     * the parent's profile.
	     */
	    if (
#ifdef PERL_IMPLICIT_CONTEXT
		g_THX == aTHX &&
#endif
		g_prof_pid == (int)getpid())
	    {
		g_rprof_end = Times(&g_prof_end);
		DBG_TIMER_NOTIFY("Profiler timer is off.\n");
		prof_record(aTHX);
	    }
	}
    }

void
NONESUCH()

BOOT:
    {
	g_TIMES_LOCATION = 42;
	g_SAVE_STACK = 1<<14;
    	g_profstack_max = 128;
#ifdef PERL_IMPLICIT_CONTEXT
	g_THX = aTHX;
#endif

        /* Before we go anywhere make sure we were invoked
         * properly, else we'll dump core.
         */
        if (!PL_DBsub)
	    croak("DProf: run perl with -d to use DProf.\n");

        /* When we hook up the XS DB::sub we'll be redefining
         * the DB::sub from the PM file.  Turn off warnings
         * while we do this.
         */
        {
	    I32 warn_tmp = PL_dowarn;
	    PL_dowarn = 0;
	    newXS("DB::sub", XS_DB_sub, file);
	    newXS("DB::goto", XS_DB_goto, file);
	    PL_dowarn = warn_tmp;
        }

        sv_setiv(PL_DBsingle, 0);	/* disable DB single-stepping */

	{
	    char *buffer = getenv("PERL_DPROF_BUFFER");

	    if (buffer) {
		g_SAVE_STACK = atoi(buffer);
	    }

	    buffer = getenv("PERL_DPROF_TICKS");

	    if (buffer) {
		g_dprof_ticks = atoi(buffer); /* Used under OS/2 only */
	    }
	    else {
		g_dprof_ticks = HZ;
	    }

	    buffer = getenv("PERL_DPROF_OUT_FILE_NAME");
	    g_out_file_name = savepv(buffer ? buffer : "tmon.out");
	}

        if ((g_fp = PerlIO_open(g_out_file_name, "w")) == NULL)
	    croak("DProf: unable to write '%s', errno = %d\n",
		  g_out_file_name, errno);

	g_default_perldb = PERLDBf_NONAME | PERLDBf_SUB | PERLDBf_GOTO;
	g_cv_hash = newHV();
        g_prof_pid = (int)getpid();

	New(0, g_profstack, g_profstack_max, PROFANY);
        prof_recordheader(aTHX);
        DBG_TIMER_NOTIFY("Profiler timer is on.\n");
	g_orealtime = g_rprof_start = Times(&g_prof_start);
	g_otms_utime = g_prof_start.tms_utime;
	g_otms_stime = g_prof_start.tms_stime;
	PL_perldb = g_default_perldb;
    }
