/***********************************************/
/* Global only to current interpreter instance */
/***********************************************/

/* Don't forget to re-run embed.pl to propagate changes! */

/* The 'I' prefix is only needed for vars that need appropriate #defines
 * generated when built with or without MULTIPLICITY.  It is also used
 * to generate the appropriate export list for win32.
 *
 * When building without MULTIPLICITY, these variables will be truly global.
 *
 * Avoid build-specific #ifdefs here, like DEBUGGING.  That way,
 * we can keep binary compatibility of the curinterp structure */

/* pseudo environmental stuff */
PERLVAR(Iorigargc,	int)
PERLVAR(Iorigargv,	char **)
PERLVAR(Ienvgv,		GV *)
PERLVAR(Isiggv,		GV *)
PERLVAR(Iincgv,		GV *)
PERLVAR(Ihintgv,	GV *)
PERLVAR(Iorigfilename,	char *)
PERLVAR(Idiehook,	SV *)
PERLVAR(Iwarnhook,	SV *)
PERLVAR(Iparsehook,	SV *)
PERLVAR(Icddir,		char *)		/* switches */
PERLVAR(Iminus_c,	bool)
PERLVAR(Ipatchlevel[10],char)
PERLVAR(Ilocalpatches,	char **)
PERLVARI(Isplitstr,	char *,	" ")
PERLVAR(Ipreprocess,	bool)
PERLVAR(Iminus_n,	bool)
PERLVAR(Iminus_p,	bool)
PERLVAR(Iminus_l,	bool)
PERLVAR(Iminus_a,	bool)
PERLVAR(Iminus_F,	bool)
PERLVAR(Idoswitches,	bool)
PERLVAR(Idowarn,	bool)
PERLVAR(Idoextract,	bool)
PERLVAR(Isawampersand,	bool)		/* must save all match strings */
PERLVAR(Isawstudy,	bool)		/* do fbm_instr on all strings */
PERLVAR(Isawvec,	bool)
PERLVAR(Iunsafe,	bool)
PERLVAR(Iinplace,	char *)
PERLVAR(Ie_script,	SV *)
PERLVAR(Iperldb,	U32)

/* This value may be raised by extensions for testing purposes */
/* 0=none, 1=full, 2=full with checks */
PERLVARI(Iperl_destruct_level,	int,	0)

/* magical thingies */
PERLVAR(Ibasetime,	Time_t)		/* $^T */
PERLVAR(Iformfeed,	SV *)		/* $^L */


PERLVARI(Imaxsysfd,	I32,	MAXSYSFD)
					/* top fd to pass to subprocesses */
PERLVAR(Imultiline,	int)		/* $*--do strings hold >1 line? */
PERLVAR(Istatusvalue,	I32)		/* $? */
#ifdef VMS
PERLVAR(Istatusvalue_vms,U32)
#endif

/* shortcuts to various I/O objects */
PERLVAR(Istdingv,	GV *)
PERLVAR(Idefgv,		GV *)
PERLVAR(Iargvgv,	GV *)
PERLVAR(Iargvoutgv,	GV *)

/* shortcuts to regexp stuff */
/* XXX these three aren't used anywhere */
PERLVAR(Ileftgv,	GV *)
PERLVAR(Iampergv,	GV *)
PERLVAR(Irightgv,	GV *)

/* this one needs to be moved to thrdvar.h and accessed via
 * find_threadsv() when USE_THREADS */
PERLVAR(Ireplgv,	GV *)

/* shortcuts to misc objects */
PERLVAR(Ierrgv,		GV *)

/* shortcuts to debugging objects */
PERLVAR(IDBgv,		GV *)
PERLVAR(IDBline,	GV *)
PERLVAR(IDBsub,		GV *)
PERLVAR(IDBsingle,	SV *)
PERLVAR(IDBtrace,	SV *)
PERLVAR(IDBsignal,	SV *)
PERLVAR(Ilineary,	AV *)		/* lines of script for debugger */
PERLVAR(Idbargs,	AV *)		/* args to call listed by caller function */

/* symbol tables */
PERLVAR(Idebstash,	HV *)		/* symbol table for perldb package */
PERLVAR(Iglobalstash,	HV *)		/* global keyword overrides imported here */
PERLVAR(Icurstname,	SV *)		/* name of current package */
PERLVAR(Ibeginav,	AV *)		/* names of BEGIN subroutines */
PERLVAR(Iendav,		AV *)		/* names of END subroutines */
PERLVAR(Iinitav,	AV *)		/* names of INIT subroutines */
PERLVAR(Istrtab,	HV *)		/* shared string table */
PERLVARI(Isub_generation,U32,1)		/* incr to invalidate method cache */

/* memory management */
PERLVAR(Isv_count,	I32)		/* how many SV* are currently allocated */
PERLVAR(Isv_objcount,	I32)		/* how many objects are currently allocated */
PERLVAR(Isv_root,	SV*)		/* storage for SVs belonging to interp */
PERLVAR(Isv_arenaroot,	SV*)		/* list of areas for garbage collection */

/* funky return mechanisms */
PERLVAR(Ilastspbase,	I32)
PERLVAR(Ilastsize,	I32)
PERLVAR(Iforkprocess,	int)		/* so do_open |- can return proc# */

/* subprocess state */
PERLVAR(Ifdpid,		AV *)		/* keep fd-to-pid mappings for my_popen */

/* internal state */
PERLVAR(Itainting,	bool)		/* doing taint checks */
PERLVARI(Iop_mask,	char *,	NULL)	/* masked operations for safe evals */
PERLVAR(Ilast_proto, char *)		/* Prototype of last sub seen. */

/* trace state */
PERLVAR(Idlevel,	I32)
PERLVARI(Idlmax,	I32,	128)
PERLVAR(Idebname,	char *)
PERLVAR(Idebdelim,	char *)

/* current interpreter roots */
PERLVAR(Imain_cv,	CV *)
PERLVAR(Imain_root,	OP *)
PERLVAR(Imain_start,	OP *)
PERLVAR(Ieval_root,	OP *)
PERLVAR(Ieval_start,	OP *)

/* runtime control stuff */
PERLVARI(Icurcopdb,	COP *,	NULL)
PERLVARI(Icopline,	line_t,	NOLINE)

/* statics moved here for shared library purposes */
PERLVAR(Istrchop,	SV)		/* return value from chop */
PERLVAR(Ifilemode,	int)		/* so nextargv() can preserve mode */
PERLVAR(Ilastfd,	int)		/* what to preserve mode on */
PERLVAR(Ioldname,	char *)		/* what to preserve mode on */
PERLVAR(IArgv,		char **)	/* stuff to free from do_aexec, vfork safe */
PERLVAR(ICmd,		char *)		/* stuff to free from do_aexec, vfork safe */
PERLVAR(Imystrk,	SV *)		/* temp key string for do_each() */
PERLVAR(Idumplvl,	I32)		/* indentation level on syntax tree dump */
PERLVAR(Ioldlastpm,	PMOP *)		/* for saving regexp context in debugger */
PERLVAR(Igensym,	I32)		/* next symbol for getsym() to define */
PERLVAR(Ipreambled,	bool)
PERLVAR(Ipreambleav,	AV *)
PERLVARI(Ilaststatval,	int,	-1)
PERLVARI(Ilaststype,	I32,	OP_STAT)
PERLVAR(Imess_sv,	SV *)

/* XXX shouldn't these be per-thread? --GSAR */
PERLVAR(Iors,		char *)		/* output record separator $\ */
PERLVAR(Iorslen,	STRLEN)
PERLVAR(Iofmt,		char *)		/* output format for numbers $# */

/* interpreter atexit processing */
PERLVARI(Iexitlist,	PerlExitListEntry *, NULL)
					/* list of exit functions */
PERLVARI(Iexitlistlen,	I32, 0)		/* length of same */
PERLVAR(Imodglobal,	HV *)		/* per-interp module data */

/* these used to be in global before 5.004_68 */
PERLVARI(Iprofiledata,	U32 *,	NULL)	/* table of ops, counts */
PERLVARI(Irsfp,	PerlIO * VOL,	Nullfp) /* current source file pointer */
PERLVARI(Irsfp_filters,	AV *,	Nullav)	/* keeps active source filters */

PERLVAR(Icompiling,	COP)		/* compiling/done executing marker */

PERLVAR(Icompcv,	CV *)		/* currently compiling subroutine */
PERLVAR(Icomppad,	AV *)		/* storage for lexically scoped temporaries */
PERLVAR(Icomppad_name,	AV *)		/* variable names for "my" variables */
PERLVAR(Icomppad_name_fill,	I32)	/* last "introduced" variable offset */
PERLVAR(Icomppad_name_floor,	I32)	/* start of vars in innermost block */

#ifdef HAVE_INTERP_INTERN
PERLVAR(Isys_intern,	struct interp_intern)
					/* platform internals */
#endif

/* more statics moved here */
PERLVARI(Igeneration,	int,	100)	/* from op.c */
PERLVAR(IDBcv,		CV *)		/* from perl.c */
PERLVAR(Iarchpat_auto,	char*)		/* from perl.c */

PERLVARI(Iin_clean_objs,bool,    FALSE)	/* from sv.c */
PERLVARI(Iin_clean_all,	bool,    FALSE)	/* from sv.c */

PERLVAR(Ilinestart,	char *)		/* beg. of most recently read line */
PERLVAR(Ipending_ident,	char)		/* pending identifier lookup */
PERLVAR(Isublex_info,	SUBLEXINFO)	/* from toke.c */

#ifdef USE_THREADS
PERLVAR(Ithrsv,		SV *)		/* struct perl_thread for main thread */
PERLVARI(Ithreadnum,	U32,	0)	/* incremented each thread creation */
PERLVAR(Istrtab_mutex,	perl_mutex)	/* Mutex for string table access */
#endif /* USE_THREADS */

PERLVARI(Ibytecode_iv_overflows,int,	0)	/* from bytecode.h */
PERLVAR(Ibytecode_sv,	SV *)
PERLVAR(Ibytecode_pv,	XPV)
PERLVAR(Ibytecode_obj_list,	void **)
PERLVARI(Ibytecode_obj_list_fill, I32,	-1)

#ifdef PERL_OBJECT
PERLVARI(piMem,		IPerlMem*,  NULL)
PERLVARI(piENV,		IPerlEnv*,  NULL)
PERLVARI(piStdIO,	IPerlStdIO*, NULL)
PERLVARI(piLIO,		IPerlLIO*,  NULL)
PERLVARI(piDir,		IPerlDir*,  NULL)
PERLVARI(piSock,	IPerlSock*, NULL)
PERLVARI(piProc,	IPerlProc*, NULL)
#endif
