typedef char *pvcontents;
typedef char *strconst;
typedef U32 PV;
typedef char *op_tr_array;
typedef int comment_t;
typedef SV *svindex;
typedef OP *opindex;
typedef IV IV64;

#ifdef INDIRECT_BGET_MACROS
#define BGET_FREAD(argp, len, nelem)	\
	 bs.fread((char*)(argp),(len),(nelem),bs.data)
#define BGET_FGETC() bs.fgetc(bs.data)
#else
#define BGET_FREAD(argp, len, nelem) PerlIO_read(fp, (argp), (len)*(nelem))
#define BGET_FGETC() PerlIO_getc(fp)
#endif /* INDIRECT_BGET_MACROS */

#define BGET_U32(arg)	\
	BGET_FREAD(&arg, sizeof(U32), 1); arg = PerlSock_ntohl((U32)arg)
#define BGET_I32(arg)	\
	BGET_FREAD(&arg, sizeof(I32), 1); arg = (I32)PerlSock_ntohl((U32)arg)
#define BGET_U16(arg)	\
	BGET_FREAD(&arg, sizeof(U16), 1); arg = PerlSock_ntohs((U16)arg)
#define BGET_U8(arg)	arg = BGET_FGETC()

#if INDIRECT_BGET_MACROS
#define BGET_PV(arg)	STMT_START {	\
	BGET_U32(arg);			\
	if (arg)			\
	    bs.freadpv(arg, bs.data);	\
	else {				\
	    PL_bytecode_pv.xpv_pv = 0;		\
	    PL_bytecode_pv.xpv_len = 0;		\
	    PL_bytecode_pv.xpv_cur = 0;		\
	}				\
    } STMT_END
#else
#define BGET_PV(arg)	STMT_START {		\
	BGET_U32(arg);				\
	if (arg) {				\
	    New(666, PL_bytecode_pv.xpv_pv, arg, char);	\
	    PerlIO_read(fp, PL_bytecode_pv.xpv_pv, arg);	\
	    PL_bytecode_pv.xpv_len = arg;			\
	    PL_bytecode_pv.xpv_cur = arg - 1;		\
	} else {				\
	    PL_bytecode_pv.xpv_pv = 0;			\
	    PL_bytecode_pv.xpv_len = 0;			\
	    PL_bytecode_pv.xpv_cur = 0;			\
	}					\
    } STMT_END
#endif /* INDIRECT_BGET_MACROS */

#define BGET_comment_t(arg) \
	do { arg = BGET_FGETC(); } while (arg != '\n' && arg != EOF)

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
	    arg = ((IV)hi << (sizeof(IV)*4) | lo);	\
	else if (((I32)hi == -1 && (I32)lo < 0)		\
		 || ((I32)hi == 0 && (I32)lo >= 0)) {	\
	    arg = (I32)lo;				\
	}						\
	else {						\
	    PL_bytecode_iv_overflows++;				\
	    arg = 0;					\
	}						\
    } STMT_END

#define BGET_op_tr_array(arg) do {	\
	unsigned short *ary;		\
	int i;				\
	New(666, ary, 256, unsigned short); \
	BGET_FREAD(ary, 256, 2);	\
	for (i = 0; i < 256; i++)	\
	    ary[i] = PerlSock_ntohs(ary[i]);	\
	arg = (char *) ary;		\
    } while (0)

#define BGET_pvcontents(arg)	arg = PL_bytecode_pv.xpv_pv
#define BGET_strconst(arg) STMT_START {	\
	for (arg = PL_tokenbuf; (*arg = BGET_FGETC()); arg++) /* nothing */; \
	arg = PL_tokenbuf;			\
    } STMT_END

#define BGET_double(arg) STMT_START {	\
	char *str;			\
	BGET_strconst(str);		\
	arg = atof(str);		\
    } STMT_END

#define BGET_objindex(arg, type) STMT_START {	\
	U32 ix;					\
	BGET_U32(ix);				\
	arg = (type)PL_bytecode_obj_list[ix];		\
    } STMT_END
#define BGET_svindex(arg) BGET_objindex(arg, svindex)
#define BGET_opindex(arg) BGET_objindex(arg, opindex)

#define BSET_ldspecsv(sv, arg) sv = PL_specialsv_list[arg]
				    
#define BSET_sv_refcnt_add(svrefcnt, arg)	svrefcnt += arg
#define BSET_gp_refcnt_add(gprefcnt, arg)	gprefcnt += arg
#define BSET_gp_share(sv, arg) STMT_START {	\
	gp_free((GV*)sv);			\
	GvGP(sv) = GvGP(arg);			\
    } STMT_END

#define BSET_gv_fetchpv(sv, arg)	sv = (SV*)gv_fetchpv(arg, TRUE, SVt_PV)
#define BSET_gv_stashpv(sv, arg)	sv = (SV*)gv_stashpv(arg, TRUE)
#define BSET_sv_magic(sv, arg)		sv_magic(sv, Nullsv, arg, 0, 0)
#define BSET_mg_pv(mg, arg)	mg->mg_ptr = arg; mg->mg_len = PL_bytecode_pv.xpv_cur
#define BSET_sv_upgrade(sv, arg)	(void)SvUPGRADE(sv, arg)
#define BSET_xpv(sv)	do {	\
	SvPV_set(sv, PL_bytecode_pv.xpv_pv);	\
	SvCUR_set(sv, PL_bytecode_pv.xpv_cur);	\
	SvLEN_set(sv, PL_bytecode_pv.xpv_len);	\
    } while (0)
#define BSET_av_extend(sv, arg)	av_extend((AV*)sv, arg)

#define BSET_av_push(sv, arg)	av_push((AV*)sv, arg)
#define BSET_hv_store(sv, arg)	\
	hv_store((HV*)sv, PL_bytecode_pv.xpv_pv, PL_bytecode_pv.xpv_cur, arg, 0)
#define BSET_pv_free(pv)	Safefree(pv.xpv_pv)
#define BSET_pregcomp(o, arg) \
	((PMOP*)o)->op_pmregexp = arg ? \
		CALLREGCOMP(arg, arg + PL_bytecode_pv.xpv_cur, ((PMOP*)o)) : 0
#define BSET_newsv(sv, arg)	sv = NEWSV(666,0); SvUPGRADE(sv, arg)
#define BSET_newop(o, arg)	o = (OP*)safemalloc(optype_size[arg])
#define BSET_newopn(o, arg) STMT_START {	\
	OP *oldop = o;				\
	BSET_newop(o, arg);			\
	oldop->op_next = o;			\
    } STMT_END

#define BSET_ret(foo) return

/*
 * Kludge special-case workaround for OP_MAPSTART
 * which needs the ppaddr for OP_GREPSTART. Blech.
 */
#define BSET_op_type(o, arg) STMT_START {	\
	o->op_type = arg;			\
	if (arg == OP_MAPSTART)			\
	    arg = OP_GREPSTART;			\
	o->op_ppaddr = ppaddr[arg];		\
    } STMT_END
#define BSET_op_ppaddr(o, arg) croak("op_ppaddr not yet implemented")
#define BSET_curpad(pad, arg) pad = AvARRAY(arg)

#define BSET_OBJ_STORE(obj, ix)		\
	(I32)ix > PL_bytecode_obj_list_fill ?	\
	bset_obj_store(obj, (I32)ix) : (PL_bytecode_obj_list[ix] = obj)
