typedef char *pvcontents;
typedef char *strconst;
typedef U32 PV;
typedef char *op_tr_array;
typedef int comment_t;
typedef SV *svindex;
typedef OP *opindex;
typedef char *pvindex;
typedef IV IV64;

#define BGET_FREAD(argp, len, nelem)	\
	 bl_read(bstate->bs_fdata,(char*)(argp),(len),(nelem))
#define BGET_FGETC() bl_getc(bstate->bs_fdata)

#define BGET_U32(arg)	\
	BGET_FREAD(&arg, sizeof(U32), 1)
#define BGET_I32(arg)	\
	BGET_FREAD(&arg, sizeof(I32), 1)
#define BGET_U16(arg)	\
	BGET_FREAD(&arg, sizeof(U16), 1)
#define BGET_U8(arg)	arg = BGET_FGETC()

#define BGET_PV(arg)	STMT_START {					\
	BGET_U32(arg);							\
	if (arg) {							\
	    New(666, bstate->bs_pv.xpv_pv, arg, char);			\
	    bl_read(bstate->bs_fdata, (void*)bstate->bs_pv.xpv_pv, arg, 1);	\
	    bstate->bs_pv.xpv_len = arg;				\
	    bstate->bs_pv.xpv_cur = arg - 1;				\
	} else {							\
	    bstate->bs_pv.xpv_pv = 0;					\
	    bstate->bs_pv.xpv_len = 0;					\
	    bstate->bs_pv.xpv_cur = 0;					\
	}								\
    } STMT_END

#ifdef BYTELOADER_LOG_COMMENTS
#  define BGET_comment_t(arg) \
    STMT_START {							\
	char buf[1024];							\
	int i = 0;							\
	do {								\
	    arg = BGET_FGETC();						\
	    buf[i++] = (char)arg;					\
	} while (arg != '\n' && arg != EOF);				\
	buf[i] = '\0';							\
	PerlIO_printf(PerlIO_stderr(), "%s", buf);			\
    } STMT_END
#else
#  define BGET_comment_t(arg) \
	do { arg = BGET_FGETC(); } while (arg != '\n' && arg != EOF)
#endif

/*
 * In the following, sizeof(IV)*4 is just a way of encoding 32 on 64-bit-IV
 * machines such that 32-bit machine compilers don't whine about the shift
 * count being too high even though the code is never reached there.
 */
#define BGET_IV64(arg) STMT_START {			\
	U32 hi, lo;					\
	BGET_U32(hi);					\
	BGET_U32(lo);					\
	if (sizeof(IV) == 8)				\
	    arg = ((IV)hi << (sizeof(IV)*4) | (IV)lo);	\
	else if (((I32)hi == -1 && (I32)lo < 0)		\
		 || ((I32)hi == 0 && (I32)lo >= 0)) {	\
	    arg = (I32)lo;				\
	}						\
	else {						\
	    bstate->bs_iv_overflows++;			\
	    arg = 0;					\
	}						\
    } STMT_END

#define BGET_op_tr_array(arg) do {			\
	unsigned short *ary;				\
	int i;						\
	New(666, ary, 256, unsigned short);		\
	BGET_FREAD(ary, sizeof(unsigned short), 256);	\
	arg = (char *) ary;				\
    } while (0)

#define BGET_pvcontents(arg)	arg = bstate->bs_pv.xpv_pv
#define BGET_strconst(arg) STMT_START {	\
	for (arg = PL_tokenbuf; (*arg = BGET_FGETC()); arg++) /* nothing */; \
	arg = PL_tokenbuf;			\
    } STMT_END

#define BGET_NV(arg) STMT_START {	\
	char *str;			\
	BGET_strconst(str);		\
	arg = Atof(str);		\
    } STMT_END

#define BGET_objindex(arg, type) STMT_START {	\
	BGET_U32(ix);				\
	arg = (type)bstate->bs_obj_list[ix];	\
    } STMT_END
#define BGET_svindex(arg) BGET_objindex(arg, svindex)
#define BGET_opindex(arg) BGET_objindex(arg, opindex)
#define BGET_pvindex(arg) STMT_START {			\
	BGET_objindex(arg, pvindex);			\
	arg = arg ? savepv(arg) : arg;			\
    } STMT_END

#define BSET_ldspecsv(sv, arg) sv = specialsv_list[arg]
#define BSET_stpv(pv, arg) STMT_START {		\
	BSET_OBJ_STORE(pv, arg);		\
	SAVEFREEPV(pv);				\
    } STMT_END
				    
#define BSET_sv_refcnt_add(svrefcnt, arg)	svrefcnt += arg
#define BSET_gp_refcnt_add(gprefcnt, arg)	gprefcnt += arg
#define BSET_gp_share(sv, arg) STMT_START {	\
	gp_free((GV*)sv);			\
	GvGP(sv) = GvGP(arg);			\
    } STMT_END

#define BSET_gv_fetchpv(sv, arg)	sv = (SV*)gv_fetchpv(arg, TRUE, SVt_PV)
#define BSET_gv_stashpv(sv, arg)	sv = (SV*)gv_stashpv(arg, TRUE)
#define BSET_sv_magic(sv, arg)		sv_magic(sv, Nullsv, arg, 0, 0)
#define BSET_mg_pv(mg, arg)	mg->mg_ptr = arg; mg->mg_len = bstate->bs_pv.xpv_cur
#define BSET_sv_upgrade(sv, arg)	(void)SvUPGRADE(sv, arg)
#define BSET_xpv(sv)	do {	\
	SvPV_set(sv, bstate->bs_pv.xpv_pv);	\
	SvCUR_set(sv, bstate->bs_pv.xpv_cur);	\
	SvLEN_set(sv, bstate->bs_pv.xpv_len);	\
    } while (0)
#define BSET_av_extend(sv, arg)	av_extend((AV*)sv, arg)

#define BSET_av_push(sv, arg)	av_push((AV*)sv, arg)
#define BSET_hv_store(sv, arg)	\
	hv_store((HV*)sv, bstate->bs_pv.xpv_pv, bstate->bs_pv.xpv_cur, arg, 0)
#define BSET_pv_free(pv)	Safefree(pv.xpv_pv)
#define BSET_pregcomp(o, arg) \
	((PMOP*)o)->op_pmregexp = arg ? \
		CALLREGCOMP(aTHX_ arg, arg + bstate->bs_pv.xpv_cur, ((PMOP*)o)) : 0
#define BSET_newsv(sv, arg)				\
	STMT_START {					\
	    sv = (arg == SVt_PVAV ? (SV*)newAV() :	\
		  arg == SVt_PVHV ? (SV*)newHV() :	\
		  NEWSV(666,0));			\
	    SvUPGRADE(sv, arg);				\
	} STMT_END
#define BSET_newop(o, arg)	((o = (OP*)safemalloc(optype_size[arg])), \
				 memzero((char*)o,optype_size[arg]))
#define BSET_newopn(o, arg) STMT_START {	\
	OP *oldop = o;				\
	BSET_newop(o, arg);			\
	oldop->op_next = o;			\
    } STMT_END

#define BSET_ret(foo) STMT_START {			\
	Safefree(bstate->bs_obj_list);			\
	return;						\
    } STMT_END

/*
 * Kludge special-case workaround for OP_MAPSTART
 * which needs the ppaddr for OP_GREPSTART. Blech.
 */
#define BSET_op_type(o, arg) STMT_START {	\
	o->op_type = arg;			\
	if (arg == OP_MAPSTART)			\
	    arg = OP_GREPSTART;			\
	o->op_ppaddr = PL_ppaddr[arg];		\
    } STMT_END
#define BSET_op_ppaddr(o, arg) Perl_croak(aTHX_ "op_ppaddr not yet implemented")
#define BSET_curpad(pad, arg) STMT_START {	\
	PL_comppad = (AV *)arg;			\
	pad = AvARRAY(arg);			\
    } STMT_END
/* this works now that Sarathy's changed the CopFILE_set macro to do the SvREFCNT_inc()
	-- BKS 6-2-2000 */
#define BSET_cop_file(cop, arg)		CopFILE_set(cop,arg)
#define BSET_cop_line(cop, arg)		CopLINE_set(cop,arg)
#define BSET_cop_stashpv(cop, arg)	CopSTASHPV_set(cop,arg)

/* this is simply stolen from the code in newATTRSUB() */
#define BSET_push_begin(ary,cv)				\
	STMT_START {					\
	    I32 oldscope = PL_scopestack_ix;		\
	    ENTER;					\
	    SAVECOPFILE(&PL_compiling);			\
	    SAVECOPLINE(&PL_compiling);			\
	    save_svref(&PL_rs);				\
	    sv_setsv(PL_rs, PL_nrs);			\
	    if (!PL_beginav)				\
		PL_beginav = newAV();			\
	    av_push(PL_beginav, cv);			\
	    call_list(oldscope, PL_beginav);		\
	    PL_curcop = &PL_compiling;			\
	    PL_compiling.op_private = PL_hints;		\
	    LEAVE;					\
	} STMT_END
#define BSET_push_init(ary,cv)								\
	STMT_START {									\
	    av_unshift((PL_initav ? PL_initav : (PL_initav = newAV(), PL_initav)), 1); 	\
	    av_store(PL_initav, 0, cv);							\
	} STMT_END
#define BSET_push_end(ary,cv)									\
	STMT_START {									\
	    av_unshift((PL_endav ? PL_endav : (PL_endav = newAV(), PL_endav)), 1);	\
	    av_store(PL_endav, 0, cv);							\
	} STMT_END
#define BSET_OBJ_STORE(obj, ix)			\
	(I32)ix > bstate->bs_obj_list_fill ?	\
	bset_obj_store(aTHXo_ bstate, obj, (I32)ix) : (bstate->bs_obj_list[ix] = obj)

/* NOTE: the bytecode header only sanity-checks the bytecode. If a script cares about
 * what version of Perl it's being called under, it should do a 'require 5.6.0' or
 * equivalent. However, since the header includes checks requiring an exact match in
 * ByteLoader versions (we can't guarantee forward compatibility), you don't 
 * need to specify one:
 * 	use ByteLoader;
 * is all you need.
 *	-- BKS, June 2000
*/

#define HEADER_FAIL(f)	\
	Perl_croak(aTHX_ "Invalid bytecode for this architecture: " f)
#define HEADER_FAIL1(f, arg1)	\
	Perl_croak(aTHX_ "Invalid bytecode for this architecture: " f, arg1)
#define HEADER_FAIL2(f, arg1, arg2)	\
	Perl_croak(aTHX_ "Invalid bytecode for this architecture: " f, arg1, arg2)

#define BYTECODE_HEADER_CHECK					\
	STMT_START {						\
	    U32 sz = 0;						\
	    strconst str;					\
								\
	    BGET_U32(sz); /* Magic: 'PLBC' */			\
	    if (sz != 0x43424c50) {				\
		HEADER_FAIL1("bad magic (want 0x43424c50, got %#x)", (int)sz);		\
	    }							\
	    BGET_strconst(str);	/* archname */			\
	    if (strNE(str, ARCHNAME)) {				\
		HEADER_FAIL2("wrong architecture (want %s, you have %s)",str,ARCHNAME);	\
	    }							\
	    BGET_strconst(str); /* ByteLoader version */	\
	    if (strNE(str, VERSION)) {				\
		HEADER_FAIL2("mismatched ByteLoader versions (want %s, you have %s)",	\
			str, VERSION);				\
	    }							\
	    BGET_U32(sz); /* ivsize */				\
	    if (sz != IVSIZE) {					\
		HEADER_FAIL("different IVSIZE");		\
	    }							\
	    BGET_U32(sz); /* ptrsize */				\
	    if (sz != PTRSIZE) {				\
		HEADER_FAIL("different PTRSIZE");		\
	    }							\
	    BGET_strconst(str); /* byteorder */			\
	    if (strNE(str, STRINGIFY(BYTEORDER))) {		\
		HEADER_FAIL("different byteorder");	\
	    }							\
	} STMT_END
