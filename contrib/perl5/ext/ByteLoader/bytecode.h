typedef char *pvcontents;
typedef char *strconst;
typedef U32 PV;
typedef char *op_tr_array;
typedef int comment_t;
typedef SV *svindex;
typedef OP *opindex;
typedef IV IV64;

#define BGET_FREAD(argp, len, nelem)	\
	 bs.pfread((char*)(argp),(len),(nelem),bs.data)
#define BGET_FGETC() bs.pfgetc(bs.data)

#define BGET_U32(arg)	\
	BGET_FREAD(&arg, sizeof(U32), 1); arg = PerlSock_ntohl((U32)arg)
#define BGET_I32(arg)	\
	BGET_FREAD(&arg, sizeof(I32), 1); arg = (I32)PerlSock_ntohl((U32)arg)
#define BGET_U16(arg)	\
	BGET_FREAD(&arg, sizeof(U16), 1); arg = PerlSock_ntohs((U16)arg)
#define BGET_U8(arg)	arg = BGET_FGETC()

#define BGET_PV(arg)	STMT_START {	\
	BGET_U32(arg);			\
	if (arg)			\
	    bs.pfreadpv(arg, bs.data, &bytecode_pv);	\
	else {				\
	    bytecode_pv.xpv_pv = 0;		\
	    bytecode_pv.xpv_len = 0;		\
	    bytecode_pv.xpv_cur = 0;		\
	}				\
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
	    bytecode_iv_overflows++;				\
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

#define BGET_pvcontents(arg)	arg = bytecode_pv.xpv_pv
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
	U32 ix;					\
	BGET_U32(ix);				\
	arg = (type)bytecode_obj_list[ix];		\
    } STMT_END
#define BGET_svindex(arg) BGET_objindex(arg, svindex)
#define BGET_opindex(arg) BGET_objindex(arg, opindex)

#define BSET_ldspecsv(sv, arg) sv = specialsv_list[arg]
				    
#define BSET_sv_refcnt_add(svrefcnt, arg)	svrefcnt += arg
#define BSET_gp_refcnt_add(gprefcnt, arg)	gprefcnt += arg
#define BSET_gp_share(sv, arg) STMT_START {	\
	gp_free((GV*)sv);			\
	GvGP(sv) = GvGP(arg);			\
    } STMT_END

#define BSET_gv_fetchpv(sv, arg)	sv = (SV*)gv_fetchpv(arg, TRUE, SVt_PV)
#define BSET_gv_stashpv(sv, arg)	sv = (SV*)gv_stashpv(arg, TRUE)
#define BSET_sv_magic(sv, arg)		sv_magic(sv, Nullsv, arg, 0, 0)
#define BSET_mg_pv(mg, arg)	mg->mg_ptr = arg; mg->mg_len = bytecode_pv.xpv_cur
#define BSET_sv_upgrade(sv, arg)	(void)SvUPGRADE(sv, arg)
#define BSET_xpv(sv)	do {	\
	SvPV_set(sv, bytecode_pv.xpv_pv);	\
	SvCUR_set(sv, bytecode_pv.xpv_cur);	\
	SvLEN_set(sv, bytecode_pv.xpv_len);	\
    } while (0)
#define BSET_av_extend(sv, arg)	av_extend((AV*)sv, arg)

#define BSET_av_push(sv, arg)	av_push((AV*)sv, arg)
#define BSET_hv_store(sv, arg)	\
	hv_store((HV*)sv, bytecode_pv.xpv_pv, bytecode_pv.xpv_cur, arg, 0)
#define BSET_pv_free(pv)	Safefree(pv.xpv_pv)
#define BSET_pregcomp(o, arg) \
	((PMOP*)o)->op_pmregexp = arg ? \
		CALLREGCOMP(aTHX_ arg, arg + bytecode_pv.xpv_cur, ((PMOP*)o)) : 0
#define BSET_newsv(sv, arg)	sv = NEWSV(666,0); SvUPGRADE(sv, arg)
#define BSET_newop(o, arg)	((o = (OP*)safemalloc(optype_size[arg])), \
				 memzero((char*)o,optype_size[arg]))
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
	o->op_ppaddr = PL_ppaddr[arg];		\
    } STMT_END
#define BSET_op_ppaddr(o, arg) Perl_croak(aTHX_ "op_ppaddr not yet implemented")
#define BSET_curpad(pad, arg) STMT_START {	\
	PL_comppad = (AV *)arg;			\
	pad = AvARRAY(arg);			\
    } STMT_END
#define BSET_cop_file(cop, arg)		CopFILE_set(cop,arg)
#define BSET_cop_line(cop, arg)		CopLINE_set(cop,arg)
#define BSET_cop_stashpv(cop, arg)	CopSTASHPV_set(cop,arg)

#define BSET_OBJ_STORE(obj, ix)		\
	(I32)ix > bytecode_obj_list_fill ?	\
	bset_obj_store(aTHXo_ obj, (I32)ix) : (bytecode_obj_list[ix] = obj)
