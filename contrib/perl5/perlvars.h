/****************/
/* Truly global */
/****************/

/* Don't forget to re-run embed.pl to propagate changes! */

/* This file describes the "global" variables used by perl
 * This used to be in perl.h directly but we want to abstract out into
 * distinct files which are per-thread, per-interpreter or really global,
 * and how they're initialized.
 *
 * The 'G' prefix is only needed for vars that need appropriate #defines
 * generated when built with or without EMBED.  It is also used to generate
 * the appropriate export list for win32.
 *
 * Avoid build-specific #ifdefs here, like DEBUGGING.  That way,
 * we can keep binary compatibility of the curinterp structure */


/* global state */
PERLVAR(Gcurinterp,	PerlInterpreter *)
					/* currently running interpreter */
#ifdef USE_THREADS
PERLVAR(Gthr_key,	perl_key)	/* For per-thread struct perl_thread* */
PERLVAR(Gsv_mutex,	perl_mutex)	/* Mutex for allocating SVs in sv.c */
PERLVAR(Gmalloc_mutex,	perl_mutex)	/* Mutex for malloc */
PERLVAR(Geval_mutex,	perl_mutex)	/* Mutex for doeval */
PERLVAR(Geval_cond,	perl_cond)	/* Condition variable for doeval */
PERLVAR(Geval_owner,	struct perl_thread *)
					/* Owner thread for doeval */
PERLVAR(Gnthreads,	int)		/* Number of threads currently */
PERLVAR(Gthreads_mutex,	perl_mutex)	/* Mutex for nthreads and thread list */
PERLVAR(Gnthreads_cond,	perl_cond)	/* Condition variable for nthreads */
PERLVAR(Gsvref_mutex,	perl_mutex)	/* Mutex for SvREFCNT_{inc,dec} */
PERLVARI(Gthreadsv_names,char *,	THREADSV_NAMES)
#ifdef FAKE_THREADS
PERLVAR(Gcurthr,	struct perl_thread *)
					/* Currently executing (fake) thread */
#endif
#endif /* USE_THREADS */

PERLVAR(Gninterps,	int)		/* number of active interpreters */

PERLVAR(Guid,		int)		/* current real user id */
PERLVAR(Geuid,		int)		/* current effective user id */
PERLVAR(Ggid,		int)		/* current real group id */
PERLVAR(Gegid,		int)		/* current effective group id */
PERLVAR(Gnomemok,	bool)		/* let malloc context handle nomem */
PERLVAR(Gan,		U32)		/* malloc sequence number */
PERLVAR(Gcop_seqmax,	U32)		/* statement sequence number */
PERLVAR(Gop_seqmax,	U16)		/* op sequence number */
PERLVAR(Gevalseq,	U32)		/* eval sequence number */
PERLVAR(Gorigenviron,	char **)
PERLVAR(Gorigalen,	U32)
PERLVAR(Gpidstatus,	HV *)		/* pid-to-status mappings for waitpid */
PERLVARI(Gmaxo,	int,	MAXO)		/* maximum number of ops */
PERLVAR(Gosname,	char *)		/* operating system */
PERLVARI(Gsh_path,	char *,	SH_PATH)/* full path of shell */
PERLVAR(Gsighandlerp,	Sighandler_t)

PERLVAR(Gxiv_arenaroot,	XPV*)		/* list of allocated xiv areas */
PERLVAR(Gxiv_root,	IV *)		/* free xiv list--shared by interpreters */
PERLVAR(Gxnv_root,	double *)	/* free xnv list--shared by interpreters */
PERLVAR(Gxrv_root,	XRV *)		/* free xrv list--shared by interpreters */
PERLVAR(Gxpv_root,	XPV *)		/* free xpv list--shared by interpreters */
PERLVAR(Ghe_root,	HE *)		/* free he list--shared by interpreters */
PERLVAR(Gnice_chunk,	char *)		/* a nice chunk of memory to reuse */
PERLVAR(Gnice_chunk_size,	U32)	/* how nice the chunk of memory is */

#ifdef PERL_OBJECT
PERLVARI(Grunops,	runops_proc_t,	FUNC_NAME_TO_PTR(RUNOPS_DEFAULT))
#else
PERLVARI(Grunops,	runops_proc_t *,	RUNOPS_DEFAULT)
#endif

PERLVAR(Gtokenbuf[256],	char)
PERLVAR(Gna,		STRLEN)		/* for use in SvPV when length is
					   Not Applicable */

PERLVAR(Gsv_undef,	SV)
PERLVAR(Gsv_no,		SV)
PERLVAR(Gsv_yes,	SV)
#ifdef CSH
PERLVARI(Gcshname,	char *,	CSH)
PERLVAR(Gcshlen,	I32)
#endif

PERLVAR(Glex_state,	U32)		/* next token is determined */
PERLVAR(Glex_defer,	U32)		/* state after determined token */
PERLVAR(Glex_expect,	expectation)	/* expect after determined token */
PERLVAR(Glex_brackets,	I32)		/* bracket count */
PERLVAR(Glex_formbrack,	I32)		/* bracket count at outer format level */
PERLVAR(Glex_fakebrack,	I32)		/* outer bracket is mere delimiter */
PERLVAR(Glex_casemods,	I32)		/* casemod count */
PERLVAR(Glex_dojoin,	I32)		/* doing an array interpolation */
PERLVAR(Glex_starts,	I32)		/* how many interps done on level */
PERLVAR(Glex_stuff,	SV *)		/* runtime pattern from m// or s/// */
PERLVAR(Glex_repl,	SV *)		/* runtime replacement from s/// */
PERLVAR(Glex_op,	OP *)		/* extra info to pass back on op */
PERLVAR(Glex_inpat,	OP *)		/* in pattern $) and $| are special */
PERLVAR(Glex_inwhat,	I32)		/* what kind of quoting are we in */
PERLVAR(Glex_brackstack,char *)		/* what kind of brackets to pop */
PERLVAR(Glex_casestack,	char *)		/* what kind of case mods in effect */

/* What we know when we're in LEX_KNOWNEXT state. */
PERLVAR(Gnextval[5],	YYSTYPE)	/* value of next token, if any */
PERLVAR(Gnexttype[5],	I32)		/* type of next token */
PERLVAR(Gnexttoke,	I32)

PERLVAR(Glinestr,	SV *)
PERLVAR(Gbufptr,	char *)
PERLVAR(Goldbufptr,	char *)
PERLVAR(Goldoldbufptr,	char *)
PERLVAR(Gbufend,	char *)
PERLVARI(Gexpect,expectation,	XSTATE)	/* how to interpret ambiguous tokens */

PERLVAR(Gmulti_start,	I32)		/* 1st line of multi-line string */
PERLVAR(Gmulti_end,	I32)		/* last line of multi-line string */
PERLVAR(Gmulti_open,	I32)		/* delimiter of said string */
PERLVAR(Gmulti_close,	I32)		/* delimiter of said string */

PERLVAR(Gerror_count,	I32)		/* how many errors so far, max 10 */
PERLVAR(Gsubline,	I32)		/* line this subroutine began on */
PERLVAR(Gsubname,	SV *)		/* name of current subroutine */

PERLVAR(Gmin_intro_pending,	I32)	/* start of vars to introduce */
PERLVAR(Gmax_intro_pending,	I32)	/* end of vars to introduce */
PERLVAR(Gpadix,		I32)		/* max used index in current "register" pad */
PERLVAR(Gpadix_floor,	I32)		/* how low may inner block reset padix */
PERLVAR(Gpad_reset_pending,	I32)	/* reset pad on next attempted alloc */

PERLVAR(Gthisexpr,	I32)		/* name id for nothing_in_common() */
PERLVAR(Glast_uni,	char *)		/* position of last named-unary op */
PERLVAR(Glast_lop,	char *)		/* position of last list operator */
PERLVAR(Glast_lop_op,	OPCODE)		/* last list operator */
PERLVAR(Gin_my,	bool)			/* we're compiling a "my" declaration */
PERLVAR(Gin_my_stash,	HV *)		/* declared class of this "my" declaration */
#ifdef FCRYPT
PERLVAR(Gcryptseen,	I32)		/* has fast crypt() been initialized? */
#endif

PERLVAR(Ghints,	U32)			/* pragma-tic compile-time flags */

PERLVAR(Gdo_undump,	bool)		/* -u or dump seen? */
PERLVAR(Gdebug,		VOL U32)	/* flags given to -D switch */


#ifdef OVERLOAD

PERLVAR(Gamagic_generation,	long)

#endif

#ifdef USE_LOCALE_COLLATE
PERLVAR(Gcollation_ix,	U32)		/* Collation generation index */
PERLVAR(Gcollation_name,char *)		/* Name of current collation */
PERLVARI(Gcollation_standard, bool,	TRUE)
					/* Assume simple collation */
PERLVAR(Gcollxfrm_base,	Size_t)		/* Basic overhead in *xfrm() */
PERLVARI(Gcollxfrm_mult,Size_t,	2)	/* Expansion factor in *xfrm() */
#endif /* USE_LOCALE_COLLATE */

#ifdef USE_LOCALE_NUMERIC

PERLVAR(Gnumeric_name,	char *)		/* Name of current numeric locale */
PERLVARI(Gnumeric_standard,	bool,	TRUE)
					/* Assume simple numerics */
PERLVARI(Gnumeric_local,	bool,	TRUE)
					/* Assume local numerics */

#endif /* !USE_LOCALE_NUMERIC */

/* constants (these are not literals to facilitate pointer comparisons) */
PERLVARIC(GYes,		char *, "1")
PERLVARIC(GNo,		char *, "")
PERLVARIC(Ghexdigit,	char *, "0123456789abcdef0123456789ABCDEF")
PERLVARIC(Gpatleave,	char *, "\\.^$@dDwWsSbB+*?|()-nrtfeaxc0123456789[{]}")

PERLVAR(Gspecialsv_list[4],SV *)	/* from byterun.h */

#ifdef USE_THREADS
PERLVAR(Gcred_mutex,      perl_mutex)     /* altered credentials in effect */
#endif
