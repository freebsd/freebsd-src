/*	$Id: roff.c,v 1.284 2016/01/08 17:48:10 schwarze Exp $ */
/*
 * Copyright (c) 2008-2012, 2014 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2010-2015 Ingo Schwarze <schwarze@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHORS DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include "config.h"

#include <sys/types.h>

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mandoc.h"
#include "mandoc_aux.h"
#include "roff.h"
#include "libmandoc.h"
#include "roff_int.h"
#include "libroff.h"

/* Maximum number of string expansions per line, to break infinite loops. */
#define	EXPAND_LIMIT	1000

/* --- data types --------------------------------------------------------- */

enum	rofft {
	ROFF_ab,
	ROFF_ad,
	ROFF_af,
	ROFF_aln,
	ROFF_als,
	ROFF_am,
	ROFF_am1,
	ROFF_ami,
	ROFF_ami1,
	ROFF_as,
	ROFF_as1,
	ROFF_asciify,
	ROFF_backtrace,
	ROFF_bd,
	ROFF_bleedat,
	ROFF_blm,
	ROFF_box,
	ROFF_boxa,
	ROFF_bp,
	ROFF_BP,
	/* MAN_br, MDOC_br */
	ROFF_break,
	ROFF_breakchar,
	ROFF_brnl,
	ROFF_brp,
	ROFF_brpnl,
	ROFF_c2,
	ROFF_cc,
	ROFF_ce,
	ROFF_cf,
	ROFF_cflags,
	ROFF_ch,
	ROFF_char,
	ROFF_chop,
	ROFF_class,
	ROFF_close,
	ROFF_CL,
	ROFF_color,
	ROFF_composite,
	ROFF_continue,
	ROFF_cp,
	ROFF_cropat,
	ROFF_cs,
	ROFF_cu,
	ROFF_da,
	ROFF_dch,
	ROFF_Dd,
	ROFF_de,
	ROFF_de1,
	ROFF_defcolor,
	ROFF_dei,
	ROFF_dei1,
	ROFF_device,
	ROFF_devicem,
	ROFF_di,
	ROFF_do,
	ROFF_ds,
	ROFF_ds1,
	ROFF_dwh,
	ROFF_dt,
	ROFF_ec,
	ROFF_ecr,
	ROFF_ecs,
	ROFF_el,
	ROFF_em,
	ROFF_EN,
	ROFF_eo,
	ROFF_EP,
	ROFF_EQ,
	ROFF_errprint,
	ROFF_ev,
	ROFF_evc,
	ROFF_ex,
	ROFF_fallback,
	ROFF_fam,
	ROFF_fc,
	ROFF_fchar,
	ROFF_fcolor,
	ROFF_fdeferlig,
	ROFF_feature,
	/* MAN_fi; ignored in mdoc(7) */
	ROFF_fkern,
	ROFF_fl,
	ROFF_flig,
	ROFF_fp,
	ROFF_fps,
	ROFF_fschar,
	ROFF_fspacewidth,
	ROFF_fspecial,
	/* MAN_ft; ignored in mdoc(7) */
	ROFF_ftr,
	ROFF_fzoom,
	ROFF_gcolor,
	ROFF_hc,
	ROFF_hcode,
	ROFF_hidechar,
	ROFF_hla,
	ROFF_hlm,
	ROFF_hpf,
	ROFF_hpfa,
	ROFF_hpfcode,
	ROFF_hw,
	ROFF_hy,
	ROFF_hylang,
	ROFF_hylen,
	ROFF_hym,
	ROFF_hypp,
	ROFF_hys,
	ROFF_ie,
	ROFF_if,
	ROFF_ig,
	/* MAN_in; ignored in mdoc(7) */
	ROFF_index,
	ROFF_it,
	ROFF_itc,
	ROFF_IX,
	ROFF_kern,
	ROFF_kernafter,
	ROFF_kernbefore,
	ROFF_kernpair,
	ROFF_lc,
	ROFF_lc_ctype,
	ROFF_lds,
	ROFF_length,
	ROFF_letadj,
	ROFF_lf,
	ROFF_lg,
	ROFF_lhang,
	ROFF_linetabs,
	/* MAN_ll, MDOC_ll */
	ROFF_lnr,
	ROFF_lnrf,
	ROFF_lpfx,
	ROFF_ls,
	ROFF_lsm,
	ROFF_lt,
	ROFF_mc,
	ROFF_mediasize,
	ROFF_minss,
	ROFF_mk,
	ROFF_mso,
	ROFF_na,
	ROFF_ne,
	/* MAN_nf; ignored in mdoc(7) */
	ROFF_nh,
	ROFF_nhychar,
	ROFF_nm,
	ROFF_nn,
	ROFF_nop,
	ROFF_nr,
	ROFF_nrf,
	ROFF_nroff,
	ROFF_ns,
	ROFF_nx,
	ROFF_open,
	ROFF_opena,
	ROFF_os,
	ROFF_output,
	ROFF_padj,
	ROFF_papersize,
	ROFF_pc,
	ROFF_pev,
	ROFF_pi,
	ROFF_PI,
	ROFF_pl,
	ROFF_pm,
	ROFF_pn,
	ROFF_pnr,
	ROFF_po,
	ROFF_ps,
	ROFF_psbb,
	ROFF_pshape,
	ROFF_pso,
	ROFF_ptr,
	ROFF_pvs,
	ROFF_rchar,
	ROFF_rd,
	ROFF_recursionlimit,
	ROFF_return,
	ROFF_rfschar,
	ROFF_rhang,
	ROFF_rj,
	ROFF_rm,
	ROFF_rn,
	ROFF_rnn,
	ROFF_rr,
	ROFF_rs,
	ROFF_rt,
	ROFF_schar,
	ROFF_sentchar,
	ROFF_shc,
	ROFF_shift,
	ROFF_sizes,
	ROFF_so,
	/* MAN_sp, MDOC_sp */
	ROFF_spacewidth,
	ROFF_special,
	ROFF_spreadwarn,
	ROFF_ss,
	ROFF_sty,
	ROFF_substring,
	ROFF_sv,
	ROFF_sy,
	ROFF_T_,
	ROFF_ta,
	ROFF_tc,
	ROFF_TE,
	ROFF_TH,
	ROFF_ti,
	ROFF_tkf,
	ROFF_tl,
	ROFF_tm,
	ROFF_tm1,
	ROFF_tmc,
	ROFF_tr,
	ROFF_track,
	ROFF_transchar,
	ROFF_trf,
	ROFF_trimat,
	ROFF_trin,
	ROFF_trnt,
	ROFF_troff,
	ROFF_TS,
	ROFF_uf,
	ROFF_ul,
	ROFF_unformat,
	ROFF_unwatch,
	ROFF_unwatchn,
	ROFF_vpt,
	ROFF_vs,
	ROFF_warn,
	ROFF_warnscale,
	ROFF_watch,
	ROFF_watchlength,
	ROFF_watchn,
	ROFF_wh,
	ROFF_while,
	ROFF_write,
	ROFF_writec,
	ROFF_writem,
	ROFF_xflag,
	ROFF_cblock,
	ROFF_USERDEF,
	ROFF_MAX
};

/*
 * An incredibly-simple string buffer.
 */
struct	roffstr {
	char		*p; /* nil-terminated buffer */
	size_t		 sz; /* saved strlen(p) */
};

/*
 * A key-value roffstr pair as part of a singly-linked list.
 */
struct	roffkv {
	struct roffstr	 key;
	struct roffstr	 val;
	struct roffkv	*next; /* next in list */
};

/*
 * A single number register as part of a singly-linked list.
 */
struct	roffreg {
	struct roffstr	 key;
	int		 val;
	struct roffreg	*next;
};

struct	roff {
	struct mparse	*parse; /* parse point */
	struct roffnode	*last; /* leaf of stack */
	int		*rstack; /* stack of inverted `ie' values */
	struct roffreg	*regtab; /* number registers */
	struct roffkv	*strtab; /* user-defined strings & macros */
	struct roffkv	*xmbtab; /* multi-byte trans table (`tr') */
	struct roffstr	*xtab; /* single-byte trans table (`tr') */
	const char	*current_string; /* value of last called user macro */
	struct tbl_node	*first_tbl; /* first table parsed */
	struct tbl_node	*last_tbl; /* last table parsed */
	struct tbl_node	*tbl; /* current table being parsed */
	struct eqn_node	*last_eqn; /* last equation parsed */
	struct eqn_node	*first_eqn; /* first equation parsed */
	struct eqn_node	*eqn; /* current equation being parsed */
	int		 eqn_inline; /* current equation is inline */
	int		 options; /* parse options */
	int		 rstacksz; /* current size limit of rstack */
	int		 rstackpos; /* position in rstack */
	int		 format; /* current file in mdoc or man format */
	int		 argc; /* number of args of the last macro */
	char		 control; /* control character */
};

struct	roffnode {
	enum rofft	 tok; /* type of node */
	struct roffnode	*parent; /* up one in stack */
	int		 line; /* parse line */
	int		 col; /* parse col */
	char		*name; /* node name, e.g. macro name */
	char		*end; /* end-rules: custom token */
	int		 endspan; /* end-rules: next-line or infty */
	int		 rule; /* current evaluation rule */
};

#define	ROFF_ARGS	 struct roff *r, /* parse ctx */ \
			 enum rofft tok, /* tok of macro */ \
			 struct buf *buf, /* input buffer */ \
			 int ln, /* parse line */ \
			 int ppos, /* original pos in buffer */ \
			 int pos, /* current pos in buffer */ \
			 int *offs /* reset offset of buffer data */

typedef	enum rofferr (*roffproc)(ROFF_ARGS);

struct	roffmac {
	const char	*name; /* macro name */
	roffproc	 proc; /* process new macro */
	roffproc	 text; /* process as child text of macro */
	roffproc	 sub; /* process as child of macro */
	int		 flags;
#define	ROFFMAC_STRUCT	(1 << 0) /* always interpret */
	struct roffmac	*next;
};

struct	predef {
	const char	*name; /* predefined input name */
	const char	*str; /* replacement symbol */
};

#define	PREDEF(__name, __str) \
	{ (__name), (__str) },

/* --- function prototypes ------------------------------------------------ */

static	enum rofft	 roffhash_find(const char *, size_t);
static	void		 roffhash_init(void);
static	void		 roffnode_cleanscope(struct roff *);
static	void		 roffnode_pop(struct roff *);
static	void		 roffnode_push(struct roff *, enum rofft,
				const char *, int, int);
static	enum rofferr	 roff_block(ROFF_ARGS);
static	enum rofferr	 roff_block_text(ROFF_ARGS);
static	enum rofferr	 roff_block_sub(ROFF_ARGS);
static	enum rofferr	 roff_brp(ROFF_ARGS);
static	enum rofferr	 roff_cblock(ROFF_ARGS);
static	enum rofferr	 roff_cc(ROFF_ARGS);
static	void		 roff_ccond(struct roff *, int, int);
static	enum rofferr	 roff_cond(ROFF_ARGS);
static	enum rofferr	 roff_cond_text(ROFF_ARGS);
static	enum rofferr	 roff_cond_sub(ROFF_ARGS);
static	enum rofferr	 roff_ds(ROFF_ARGS);
static	enum rofferr	 roff_eqndelim(struct roff *, struct buf *, int);
static	int		 roff_evalcond(struct roff *r, int, char *, int *);
static	int		 roff_evalnum(struct roff *, int,
				const char *, int *, int *, int);
static	int		 roff_evalpar(struct roff *, int,
				const char *, int *, int *, int);
static	int		 roff_evalstrcond(const char *, int *);
static	void		 roff_free1(struct roff *);
static	void		 roff_freereg(struct roffreg *);
static	void		 roff_freestr(struct roffkv *);
static	size_t		 roff_getname(struct roff *, char **, int, int);
static	int		 roff_getnum(const char *, int *, int *, int);
static	int		 roff_getop(const char *, int *, char *);
static	int		 roff_getregn(const struct roff *,
				const char *, size_t);
static	int		 roff_getregro(const struct roff *,
				const char *name);
static	const char	*roff_getstrn(const struct roff *,
				const char *, size_t);
static	int		 roff_hasregn(const struct roff *,
				const char *, size_t);
static	enum rofferr	 roff_insec(ROFF_ARGS);
static	enum rofferr	 roff_it(ROFF_ARGS);
static	enum rofferr	 roff_line_ignore(ROFF_ARGS);
static	void		 roff_man_alloc1(struct roff_man *);
static	void		 roff_man_free1(struct roff_man *);
static	enum rofferr	 roff_nr(ROFF_ARGS);
static	enum rofft	 roff_parse(struct roff *, char *, int *,
				int, int);
static	enum rofferr	 roff_parsetext(struct buf *, int, int *);
static	enum rofferr	 roff_res(struct roff *, struct buf *, int, int);
static	enum rofferr	 roff_rm(ROFF_ARGS);
static	enum rofferr	 roff_rr(ROFF_ARGS);
static	void		 roff_setstr(struct roff *,
				const char *, const char *, int);
static	void		 roff_setstrn(struct roffkv **, const char *,
				size_t, const char *, size_t, int);
static	enum rofferr	 roff_so(ROFF_ARGS);
static	enum rofferr	 roff_tr(ROFF_ARGS);
static	enum rofferr	 roff_Dd(ROFF_ARGS);
static	enum rofferr	 roff_TH(ROFF_ARGS);
static	enum rofferr	 roff_TE(ROFF_ARGS);
static	enum rofferr	 roff_TS(ROFF_ARGS);
static	enum rofferr	 roff_EQ(ROFF_ARGS);
static	enum rofferr	 roff_EN(ROFF_ARGS);
static	enum rofferr	 roff_T_(ROFF_ARGS);
static	enum rofferr	 roff_unsupp(ROFF_ARGS);
static	enum rofferr	 roff_userdef(ROFF_ARGS);

/* --- constant data ------------------------------------------------------ */

/* See roffhash_find() */

#define	ASCII_HI	 126
#define	ASCII_LO	 33
#define	HASHWIDTH	(ASCII_HI - ASCII_LO + 1)

#define	ROFFNUM_SCALE	(1 << 0)  /* Honour scaling in roff_getnum(). */
#define	ROFFNUM_WHITE	(1 << 1)  /* Skip whitespace in roff_evalnum(). */

static	struct roffmac	*hash[HASHWIDTH];

static	struct roffmac	 roffs[ROFF_MAX] = {
	{ "ab", roff_unsupp, NULL, NULL, 0, NULL },
	{ "ad", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "af", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "aln", roff_unsupp, NULL, NULL, 0, NULL },
	{ "als", roff_unsupp, NULL, NULL, 0, NULL },
	{ "am", roff_block, roff_block_text, roff_block_sub, 0, NULL },
	{ "am1", roff_block, roff_block_text, roff_block_sub, 0, NULL },
	{ "ami", roff_block, roff_block_text, roff_block_sub, 0, NULL },
	{ "ami1", roff_block, roff_block_text, roff_block_sub, 0, NULL },
	{ "as", roff_ds, NULL, NULL, 0, NULL },
	{ "as1", roff_ds, NULL, NULL, 0, NULL },
	{ "asciify", roff_unsupp, NULL, NULL, 0, NULL },
	{ "backtrace", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "bd", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "bleedat", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "blm", roff_unsupp, NULL, NULL, 0, NULL },
	{ "box", roff_unsupp, NULL, NULL, 0, NULL },
	{ "boxa", roff_unsupp, NULL, NULL, 0, NULL },
	{ "bp", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "BP", roff_unsupp, NULL, NULL, 0, NULL },
	{ "break", roff_unsupp, NULL, NULL, 0, NULL },
	{ "breakchar", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "brnl", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "brp", roff_brp, NULL, NULL, 0, NULL },
	{ "brpnl", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "c2", roff_unsupp, NULL, NULL, 0, NULL },
	{ "cc", roff_cc, NULL, NULL, 0, NULL },
	{ "ce", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "cf", roff_insec, NULL, NULL, 0, NULL },
	{ "cflags", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "ch", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "char", roff_unsupp, NULL, NULL, 0, NULL },
	{ "chop", roff_unsupp, NULL, NULL, 0, NULL },
	{ "class", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "close", roff_insec, NULL, NULL, 0, NULL },
	{ "CL", roff_unsupp, NULL, NULL, 0, NULL },
	{ "color", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "composite", roff_unsupp, NULL, NULL, 0, NULL },
	{ "continue", roff_unsupp, NULL, NULL, 0, NULL },
	{ "cp", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "cropat", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "cs", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "cu", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "da", roff_unsupp, NULL, NULL, 0, NULL },
	{ "dch", roff_unsupp, NULL, NULL, 0, NULL },
	{ "Dd", roff_Dd, NULL, NULL, 0, NULL },
	{ "de", roff_block, roff_block_text, roff_block_sub, 0, NULL },
	{ "de1", roff_block, roff_block_text, roff_block_sub, 0, NULL },
	{ "defcolor", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "dei", roff_block, roff_block_text, roff_block_sub, 0, NULL },
	{ "dei1", roff_block, roff_block_text, roff_block_sub, 0, NULL },
	{ "device", roff_unsupp, NULL, NULL, 0, NULL },
	{ "devicem", roff_unsupp, NULL, NULL, 0, NULL },
	{ "di", roff_unsupp, NULL, NULL, 0, NULL },
	{ "do", roff_unsupp, NULL, NULL, 0, NULL },
	{ "ds", roff_ds, NULL, NULL, 0, NULL },
	{ "ds1", roff_ds, NULL, NULL, 0, NULL },
	{ "dwh", roff_unsupp, NULL, NULL, 0, NULL },
	{ "dt", roff_unsupp, NULL, NULL, 0, NULL },
	{ "ec", roff_unsupp, NULL, NULL, 0, NULL },
	{ "ecr", roff_unsupp, NULL, NULL, 0, NULL },
	{ "ecs", roff_unsupp, NULL, NULL, 0, NULL },
	{ "el", roff_cond, roff_cond_text, roff_cond_sub, ROFFMAC_STRUCT, NULL },
	{ "em", roff_unsupp, NULL, NULL, 0, NULL },
	{ "EN", roff_EN, NULL, NULL, 0, NULL },
	{ "eo", roff_unsupp, NULL, NULL, 0, NULL },
	{ "EP", roff_unsupp, NULL, NULL, 0, NULL },
	{ "EQ", roff_EQ, NULL, NULL, 0, NULL },
	{ "errprint", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "ev", roff_unsupp, NULL, NULL, 0, NULL },
	{ "evc", roff_unsupp, NULL, NULL, 0, NULL },
	{ "ex", roff_unsupp, NULL, NULL, 0, NULL },
	{ "fallback", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "fam", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "fc", roff_unsupp, NULL, NULL, 0, NULL },
	{ "fchar", roff_unsupp, NULL, NULL, 0, NULL },
	{ "fcolor", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "fdeferlig", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "feature", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "fkern", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "fl", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "flig", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "fp", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "fps", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "fschar", roff_unsupp, NULL, NULL, 0, NULL },
	{ "fspacewidth", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "fspecial", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "ftr", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "fzoom", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "gcolor", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "hc", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "hcode", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "hidechar", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "hla", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "hlm", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "hpf", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "hpfa", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "hpfcode", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "hw", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "hy", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "hylang", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "hylen", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "hym", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "hypp", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "hys", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "ie", roff_cond, roff_cond_text, roff_cond_sub, ROFFMAC_STRUCT, NULL },
	{ "if", roff_cond, roff_cond_text, roff_cond_sub, ROFFMAC_STRUCT, NULL },
	{ "ig", roff_block, roff_block_text, roff_block_sub, 0, NULL },
	{ "index", roff_unsupp, NULL, NULL, 0, NULL },
	{ "it", roff_it, NULL, NULL, 0, NULL },
	{ "itc", roff_unsupp, NULL, NULL, 0, NULL },
	{ "IX", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "kern", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "kernafter", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "kernbefore", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "kernpair", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "lc", roff_unsupp, NULL, NULL, 0, NULL },
	{ "lc_ctype", roff_unsupp, NULL, NULL, 0, NULL },
	{ "lds", roff_unsupp, NULL, NULL, 0, NULL },
	{ "length", roff_unsupp, NULL, NULL, 0, NULL },
	{ "letadj", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "lf", roff_insec, NULL, NULL, 0, NULL },
	{ "lg", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "lhang", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "linetabs", roff_unsupp, NULL, NULL, 0, NULL },
	{ "lnr", roff_unsupp, NULL, NULL, 0, NULL },
	{ "lnrf", roff_unsupp, NULL, NULL, 0, NULL },
	{ "lpfx", roff_unsupp, NULL, NULL, 0, NULL },
	{ "ls", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "lsm", roff_unsupp, NULL, NULL, 0, NULL },
	{ "lt", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "mc", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "mediasize", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "minss", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "mk", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "mso", roff_insec, NULL, NULL, 0, NULL },
	{ "na", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "ne", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "nh", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "nhychar", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "nm", roff_unsupp, NULL, NULL, 0, NULL },
	{ "nn", roff_unsupp, NULL, NULL, 0, NULL },
	{ "nop", roff_unsupp, NULL, NULL, 0, NULL },
	{ "nr", roff_nr, NULL, NULL, 0, NULL },
	{ "nrf", roff_unsupp, NULL, NULL, 0, NULL },
	{ "nroff", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "ns", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "nx", roff_insec, NULL, NULL, 0, NULL },
	{ "open", roff_insec, NULL, NULL, 0, NULL },
	{ "opena", roff_insec, NULL, NULL, 0, NULL },
	{ "os", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "output", roff_unsupp, NULL, NULL, 0, NULL },
	{ "padj", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "papersize", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "pc", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "pev", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "pi", roff_insec, NULL, NULL, 0, NULL },
	{ "PI", roff_unsupp, NULL, NULL, 0, NULL },
	{ "pl", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "pm", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "pn", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "pnr", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "po", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "ps", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "psbb", roff_unsupp, NULL, NULL, 0, NULL },
	{ "pshape", roff_unsupp, NULL, NULL, 0, NULL },
	{ "pso", roff_insec, NULL, NULL, 0, NULL },
	{ "ptr", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "pvs", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "rchar", roff_unsupp, NULL, NULL, 0, NULL },
	{ "rd", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "recursionlimit", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "return", roff_unsupp, NULL, NULL, 0, NULL },
	{ "rfschar", roff_unsupp, NULL, NULL, 0, NULL },
	{ "rhang", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "rj", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "rm", roff_rm, NULL, NULL, 0, NULL },
	{ "rn", roff_unsupp, NULL, NULL, 0, NULL },
	{ "rnn", roff_unsupp, NULL, NULL, 0, NULL },
	{ "rr", roff_rr, NULL, NULL, 0, NULL },
	{ "rs", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "rt", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "schar", roff_unsupp, NULL, NULL, 0, NULL },
	{ "sentchar", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "shc", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "shift", roff_unsupp, NULL, NULL, 0, NULL },
	{ "sizes", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "so", roff_so, NULL, NULL, 0, NULL },
	{ "spacewidth", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "special", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "spreadwarn", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "ss", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "sty", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "substring", roff_unsupp, NULL, NULL, 0, NULL },
	{ "sv", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "sy", roff_insec, NULL, NULL, 0, NULL },
	{ "T&", roff_T_, NULL, NULL, 0, NULL },
	{ "ta", roff_unsupp, NULL, NULL, 0, NULL },
	{ "tc", roff_unsupp, NULL, NULL, 0, NULL },
	{ "TE", roff_TE, NULL, NULL, 0, NULL },
	{ "TH", roff_TH, NULL, NULL, 0, NULL },
	{ "ti", roff_unsupp, NULL, NULL, 0, NULL },
	{ "tkf", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "tl", roff_unsupp, NULL, NULL, 0, NULL },
	{ "tm", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "tm1", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "tmc", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "tr", roff_tr, NULL, NULL, 0, NULL },
	{ "track", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "transchar", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "trf", roff_insec, NULL, NULL, 0, NULL },
	{ "trimat", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "trin", roff_unsupp, NULL, NULL, 0, NULL },
	{ "trnt", roff_unsupp, NULL, NULL, 0, NULL },
	{ "troff", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "TS", roff_TS, NULL, NULL, 0, NULL },
	{ "uf", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "ul", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "unformat", roff_unsupp, NULL, NULL, 0, NULL },
	{ "unwatch", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "unwatchn", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "vpt", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "vs", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "warn", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "warnscale", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "watch", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "watchlength", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "watchn", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "wh", roff_unsupp, NULL, NULL, 0, NULL },
	{ "while", roff_unsupp, NULL, NULL, 0, NULL },
	{ "write", roff_insec, NULL, NULL, 0, NULL },
	{ "writec", roff_insec, NULL, NULL, 0, NULL },
	{ "writem", roff_insec, NULL, NULL, 0, NULL },
	{ "xflag", roff_line_ignore, NULL, NULL, 0, NULL },
	{ ".", roff_cblock, NULL, NULL, 0, NULL },
	{ NULL, roff_userdef, NULL, NULL, 0, NULL },
};

/* not currently implemented: Ds em Eq LP Me PP pp Or Rd Sf SH */
const	char *const __mdoc_reserved[] = {
	"Ac", "Ad", "An", "Ao", "Ap", "Aq", "Ar", "At",
	"Bc", "Bd", "Bf", "Bk", "Bl", "Bo", "Bq",
	"Brc", "Bro", "Brq", "Bsx", "Bt", "Bx",
	"Cd", "Cm", "Db", "Dc", "Dd", "Dl", "Do", "Dq",
	"Dt", "Dv", "Dx", "D1",
	"Ec", "Ed", "Ef", "Ek", "El", "Em",
	"En", "Eo", "Er", "Es", "Ev", "Ex",
	"Fa", "Fc", "Fd", "Fl", "Fn", "Fo", "Fr", "Ft", "Fx",
	"Hf", "Ic", "In", "It", "Lb", "Li", "Lk", "Lp",
	"Ms", "Mt", "Nd", "Nm", "No", "Ns", "Nx",
	"Oc", "Oo", "Op", "Os", "Ot", "Ox",
	"Pa", "Pc", "Pf", "Po", "Pp", "Pq",
	"Qc", "Ql", "Qo", "Qq", "Re", "Rs", "Rv",
	"Sc", "Sh", "Sm", "So", "Sq",
	"Ss", "St", "Sx", "Sy",
	"Ta", "Tn", "Ud", "Ux", "Va", "Vt", "Xc", "Xo", "Xr",
	"%A", "%B", "%C", "%D", "%I", "%J", "%N", "%O",
	"%P", "%Q", "%R", "%T", "%U", "%V",
	NULL
};

/* not currently implemented: BT DE DS ME MT PT SY TQ YS */
const	char *const __man_reserved[] = {
	"AT", "B", "BI", "BR", "DT",
	"EE", "EN", "EQ", "EX", "HP", "I", "IB", "IP", "IR",
	"LP", "OP", "P", "PD", "PP",
	"R", "RB", "RE", "RI", "RS", "SB", "SH", "SM", "SS",
	"TE", "TH", "TP", "TS", "T&", "UC", "UE", "UR",
	NULL
};

/* Array of injected predefined strings. */
#define	PREDEFS_MAX	 38
static	const struct predef predefs[PREDEFS_MAX] = {
#include "predefs.in"
};

/* See roffhash_find() */
#define	ROFF_HASH(p)	(p[0] - ASCII_LO)

static	int	 roffit_lines;  /* number of lines to delay */
static	char	*roffit_macro;  /* nil-terminated macro line */


/* --- request table ------------------------------------------------------ */

static void
roffhash_init(void)
{
	struct roffmac	 *n;
	int		  buc, i;

	for (i = 0; i < (int)ROFF_USERDEF; i++) {
		assert(roffs[i].name[0] >= ASCII_LO);
		assert(roffs[i].name[0] <= ASCII_HI);

		buc = ROFF_HASH(roffs[i].name);

		if (NULL != (n = hash[buc])) {
			for ( ; n->next; n = n->next)
				/* Do nothing. */ ;
			n->next = &roffs[i];
		} else
			hash[buc] = &roffs[i];
	}
}

/*
 * Look up a roff token by its name.  Returns ROFF_MAX if no macro by
 * the nil-terminated string name could be found.
 */
static enum rofft
roffhash_find(const char *p, size_t s)
{
	int		 buc;
	struct roffmac	*n;

	/*
	 * libroff has an extremely simple hashtable, for the time
	 * being, which simply keys on the first character, which must
	 * be printable, then walks a chain.  It works well enough until
	 * optimised.
	 */

	if (p[0] < ASCII_LO || p[0] > ASCII_HI)
		return ROFF_MAX;

	buc = ROFF_HASH(p);

	if (NULL == (n = hash[buc]))
		return ROFF_MAX;
	for ( ; n; n = n->next)
		if (0 == strncmp(n->name, p, s) && '\0' == n->name[(int)s])
			return (enum rofft)(n - roffs);

	return ROFF_MAX;
}

/* --- stack of request blocks -------------------------------------------- */

/*
 * Pop the current node off of the stack of roff instructions currently
 * pending.
 */
static void
roffnode_pop(struct roff *r)
{
	struct roffnode	*p;

	assert(r->last);
	p = r->last;

	r->last = r->last->parent;
	free(p->name);
	free(p->end);
	free(p);
}

/*
 * Push a roff node onto the instruction stack.  This must later be
 * removed with roffnode_pop().
 */
static void
roffnode_push(struct roff *r, enum rofft tok, const char *name,
		int line, int col)
{
	struct roffnode	*p;

	p = mandoc_calloc(1, sizeof(struct roffnode));
	p->tok = tok;
	if (name)
		p->name = mandoc_strdup(name);
	p->parent = r->last;
	p->line = line;
	p->col = col;
	p->rule = p->parent ? p->parent->rule : 0;

	r->last = p;
}

/* --- roff parser state data management ---------------------------------- */

static void
roff_free1(struct roff *r)
{
	struct tbl_node	*tbl;
	struct eqn_node	*e;
	int		 i;

	while (NULL != (tbl = r->first_tbl)) {
		r->first_tbl = tbl->next;
		tbl_free(tbl);
	}
	r->first_tbl = r->last_tbl = r->tbl = NULL;

	while (NULL != (e = r->first_eqn)) {
		r->first_eqn = e->next;
		eqn_free(e);
	}
	r->first_eqn = r->last_eqn = r->eqn = NULL;

	while (r->last)
		roffnode_pop(r);

	free (r->rstack);
	r->rstack = NULL;
	r->rstacksz = 0;
	r->rstackpos = -1;

	roff_freereg(r->regtab);
	r->regtab = NULL;

	roff_freestr(r->strtab);
	roff_freestr(r->xmbtab);
	r->strtab = r->xmbtab = NULL;

	if (r->xtab)
		for (i = 0; i < 128; i++)
			free(r->xtab[i].p);
	free(r->xtab);
	r->xtab = NULL;
}

void
roff_reset(struct roff *r)
{

	roff_free1(r);
	r->format = r->options & (MPARSE_MDOC | MPARSE_MAN);
	r->control = 0;
}

void
roff_free(struct roff *r)
{

	roff_free1(r);
	free(r);
}

struct roff *
roff_alloc(struct mparse *parse, int options)
{
	struct roff	*r;

	r = mandoc_calloc(1, sizeof(struct roff));
	r->parse = parse;
	r->options = options;
	r->format = options & (MPARSE_MDOC | MPARSE_MAN);
	r->rstackpos = -1;

	roffhash_init();

	return r;
}

/* --- syntax tree state data management ---------------------------------- */

static void
roff_man_free1(struct roff_man *man)
{

	if (man->first != NULL)
		roff_node_delete(man, man->first);
	free(man->meta.msec);
	free(man->meta.vol);
	free(man->meta.os);
	free(man->meta.arch);
	free(man->meta.title);
	free(man->meta.name);
	free(man->meta.date);
}

static void
roff_man_alloc1(struct roff_man *man)
{

	memset(&man->meta, 0, sizeof(man->meta));
	man->first = mandoc_calloc(1, sizeof(*man->first));
	man->first->type = ROFFT_ROOT;
	man->last = man->first;
	man->last_es = NULL;
	man->flags = 0;
	man->macroset = MACROSET_NONE;
	man->lastsec = man->lastnamed = SEC_NONE;
	man->next = ROFF_NEXT_CHILD;
}

void
roff_man_reset(struct roff_man *man)
{

	roff_man_free1(man);
	roff_man_alloc1(man);
}

void
roff_man_free(struct roff_man *man)
{

	roff_man_free1(man);
	free(man);
}

struct roff_man *
roff_man_alloc(struct roff *roff, struct mparse *parse,
	const char *defos, int quick)
{
	struct roff_man *man;

	man = mandoc_calloc(1, sizeof(*man));
	man->parse = parse;
	man->roff = roff;
	man->defos = defos;
	man->quick = quick;
	roff_man_alloc1(man);
	return man;
}

/* --- syntax tree handling ----------------------------------------------- */

struct roff_node *
roff_node_alloc(struct roff_man *man, int line, int pos,
	enum roff_type type, int tok)
{
	struct roff_node	*n;

	n = mandoc_calloc(1, sizeof(*n));
	n->line = line;
	n->pos = pos;
	n->tok = tok;
	n->type = type;
	n->sec = man->lastsec;

	if (man->flags & MDOC_SYNOPSIS)
		n->flags |= MDOC_SYNPRETTY;
	else
		n->flags &= ~MDOC_SYNPRETTY;
	if (man->flags & MDOC_NEWLINE)
		n->flags |= MDOC_LINE;
	man->flags &= ~MDOC_NEWLINE;

	return n;
}

void
roff_node_append(struct roff_man *man, struct roff_node *n)
{

	switch (man->next) {
	case ROFF_NEXT_SIBLING:
		if (man->last->next != NULL) {
			n->next = man->last->next;
			man->last->next->prev = n;
		} else
			man->last->parent->last = n;
		man->last->next = n;
		n->prev = man->last;
		n->parent = man->last->parent;
		break;
	case ROFF_NEXT_CHILD:
		man->last->child = n;
		n->parent = man->last;
		n->parent->last = n;
		break;
	default:
		abort();
	}
	man->last = n;

	switch (n->type) {
	case ROFFT_HEAD:
		n->parent->head = n;
		break;
	case ROFFT_BODY:
		if (n->end != ENDBODY_NOT)
			return;
		n->parent->body = n;
		break;
	case ROFFT_TAIL:
		n->parent->tail = n;
		break;
	default:
		return;
	}

	/*
	 * Copy over the normalised-data pointer of our parent.  Not
	 * everybody has one, but copying a null pointer is fine.
	 */

	n->norm = n->parent->norm;
	assert(n->parent->type == ROFFT_BLOCK);
}

void
roff_word_alloc(struct roff_man *man, int line, int pos, const char *word)
{
	struct roff_node	*n;

	n = roff_node_alloc(man, line, pos, ROFFT_TEXT, TOKEN_NONE);
	n->string = roff_strdup(man->roff, word);
	roff_node_append(man, n);
	if (man->macroset == MACROSET_MDOC)
		n->flags |= MDOC_VALID | MDOC_ENDED;
	else
		n->flags |= MAN_VALID;
	man->next = ROFF_NEXT_SIBLING;
}

void
roff_word_append(struct roff_man *man, const char *word)
{
	struct roff_node	*n;
	char			*addstr, *newstr;

	n = man->last;
	addstr = roff_strdup(man->roff, word);
	mandoc_asprintf(&newstr, "%s %s", n->string, addstr);
	free(addstr);
	free(n->string);
	n->string = newstr;
	man->next = ROFF_NEXT_SIBLING;
}

void
roff_elem_alloc(struct roff_man *man, int line, int pos, int tok)
{
	struct roff_node	*n;

	n = roff_node_alloc(man, line, pos, ROFFT_ELEM, tok);
	roff_node_append(man, n);
	man->next = ROFF_NEXT_CHILD;
}

struct roff_node *
roff_block_alloc(struct roff_man *man, int line, int pos, int tok)
{
	struct roff_node	*n;

	n = roff_node_alloc(man, line, pos, ROFFT_BLOCK, tok);
	roff_node_append(man, n);
	man->next = ROFF_NEXT_CHILD;
	return n;
}

struct roff_node *
roff_head_alloc(struct roff_man *man, int line, int pos, int tok)
{
	struct roff_node	*n;

	n = roff_node_alloc(man, line, pos, ROFFT_HEAD, tok);
	roff_node_append(man, n);
	man->next = ROFF_NEXT_CHILD;
	return n;
}

struct roff_node *
roff_body_alloc(struct roff_man *man, int line, int pos, int tok)
{
	struct roff_node	*n;

	n = roff_node_alloc(man, line, pos, ROFFT_BODY, tok);
	roff_node_append(man, n);
	man->next = ROFF_NEXT_CHILD;
	return n;
}

void
roff_addeqn(struct roff_man *man, const struct eqn *eqn)
{
	struct roff_node	*n;

	n = roff_node_alloc(man, eqn->ln, eqn->pos, ROFFT_EQN, TOKEN_NONE);
	n->eqn = eqn;
	if (eqn->ln > man->last->line)
		n->flags |= MDOC_LINE;
	roff_node_append(man, n);
	man->next = ROFF_NEXT_SIBLING;
}

void
roff_addtbl(struct roff_man *man, const struct tbl_span *tbl)
{
	struct roff_node	*n;

	if (man->macroset == MACROSET_MAN)
		man_breakscope(man, TOKEN_NONE);
	n = roff_node_alloc(man, tbl->line, 0, ROFFT_TBL, TOKEN_NONE);
	n->span = tbl;
	roff_node_append(man, n);
	if (man->macroset == MACROSET_MDOC)
		n->flags |= MDOC_VALID | MDOC_ENDED;
	else
		n->flags |= MAN_VALID;
	man->next = ROFF_NEXT_SIBLING;
}

void
roff_node_unlink(struct roff_man *man, struct roff_node *n)
{

	/* Adjust siblings. */

	if (n->prev)
		n->prev->next = n->next;
	if (n->next)
		n->next->prev = n->prev;

	/* Adjust parent. */

	if (n->parent != NULL) {
		if (n->parent->child == n)
			n->parent->child = n->next;
		if (n->parent->last == n)
			n->parent->last = n->prev;
	}

	/* Adjust parse point. */

	if (man == NULL)
		return;
	if (man->last == n) {
		if (n->prev == NULL) {
			man->last = n->parent;
			man->next = ROFF_NEXT_CHILD;
		} else {
			man->last = n->prev;
			man->next = ROFF_NEXT_SIBLING;
		}
	}
	if (man->first == n)
		man->first = NULL;
}

void
roff_node_free(struct roff_node *n)
{

	if (n->args != NULL)
		mdoc_argv_free(n->args);
	if (n->type == ROFFT_BLOCK || n->type == ROFFT_ELEM)
		free(n->norm);
	free(n->string);
	free(n);
}

void
roff_node_delete(struct roff_man *man, struct roff_node *n)
{

	while (n->child != NULL)
		roff_node_delete(man, n->child);
	roff_node_unlink(man, n);
	roff_node_free(n);
}

void
deroff(char **dest, const struct roff_node *n)
{
	char	*cp;
	size_t	 sz;

	if (n->type != ROFFT_TEXT) {
		for (n = n->child; n != NULL; n = n->next)
			deroff(dest, n);
		return;
	}

	/* Skip leading whitespace and escape sequences. */

	cp = n->string;
	while (*cp != '\0') {
		if ('\\' == *cp) {
			cp++;
			mandoc_escape((const char **)&cp, NULL, NULL);
		} else if (isspace((unsigned char)*cp))
			cp++;
		else
			break;
	}

	/* Skip trailing whitespace. */

	for (sz = strlen(cp); sz; sz--)
		if ( ! isspace((unsigned char)cp[sz-1]))
			break;

	/* Skip empty strings. */

	if (sz == 0)
		return;

	if (*dest == NULL) {
		*dest = mandoc_strndup(cp, sz);
		return;
	}

	mandoc_asprintf(&cp, "%s %*s", *dest, (int)sz, cp);
	free(*dest);
	*dest = cp;
}

/* --- main functions of the roff parser ---------------------------------- */

/*
 * In the current line, expand escape sequences that tend to get
 * used in numerical expressions and conditional requests.
 * Also check the syntax of the remaining escape sequences.
 */
static enum rofferr
roff_res(struct roff *r, struct buf *buf, int ln, int pos)
{
	char		 ubuf[24]; /* buffer to print the number */
	const char	*start;	/* start of the string to process */
	char		*stesc;	/* start of an escape sequence ('\\') */
	const char	*stnam;	/* start of the name, after "[(*" */
	const char	*cp;	/* end of the name, e.g. before ']' */
	const char	*res;	/* the string to be substituted */
	char		*nbuf;	/* new buffer to copy buf->buf to */
	size_t		 maxl;  /* expected length of the escape name */
	size_t		 naml;	/* actual length of the escape name */
	enum mandoc_esc	 esc;	/* type of the escape sequence */
	int		 inaml;	/* length returned from mandoc_escape() */
	int		 expand_count;	/* to avoid infinite loops */
	int		 npos;	/* position in numeric expression */
	int		 arg_complete; /* argument not interrupted by eol */
	char		 term;	/* character terminating the escape */

	expand_count = 0;
	start = buf->buf + pos;
	stesc = strchr(start, '\0') - 1;
	while (stesc-- > start) {

		/* Search backwards for the next backslash. */

		if (*stesc != '\\')
			continue;

		/* If it is escaped, skip it. */

		for (cp = stesc - 1; cp >= start; cp--)
			if (*cp != '\\')
				break;

		if ((stesc - cp) % 2 == 0) {
			stesc = (char *)cp;
			continue;
		}

		/* Decide whether to expand or to check only. */

		term = '\0';
		cp = stesc + 1;
		switch (*cp) {
		case '*':
			res = NULL;
			break;
		case 'B':
		case 'w':
			term = cp[1];
			/* FALLTHROUGH */
		case 'n':
			res = ubuf;
			break;
		default:
			esc = mandoc_escape(&cp, &stnam, &inaml);
			if (esc == ESCAPE_ERROR ||
			    (esc == ESCAPE_SPECIAL &&
			     mchars_spec2cp(stnam, inaml) < 0))
				mandoc_vmsg(MANDOCERR_ESC_BAD,
				    r->parse, ln, (int)(stesc - buf->buf),
				    "%.*s", (int)(cp - stesc), stesc);
			continue;
		}

		if (EXPAND_LIMIT < ++expand_count) {
			mandoc_msg(MANDOCERR_ROFFLOOP, r->parse,
			    ln, (int)(stesc - buf->buf), NULL);
			return ROFF_IGN;
		}

		/*
		 * The third character decides the length
		 * of the name of the string or register.
		 * Save a pointer to the name.
		 */

		if (term == '\0') {
			switch (*++cp) {
			case '\0':
				maxl = 0;
				break;
			case '(':
				cp++;
				maxl = 2;
				break;
			case '[':
				cp++;
				term = ']';
				maxl = 0;
				break;
			default:
				maxl = 1;
				break;
			}
		} else {
			cp += 2;
			maxl = 0;
		}
		stnam = cp;

		/* Advance to the end of the name. */

		naml = 0;
		arg_complete = 1;
		while (maxl == 0 || naml < maxl) {
			if (*cp == '\0') {
				mandoc_msg(MANDOCERR_ESC_BAD, r->parse,
				    ln, (int)(stesc - buf->buf), stesc);
				arg_complete = 0;
				break;
			}
			if (maxl == 0 && *cp == term) {
				cp++;
				break;
			}
			if (*cp++ != '\\' || stesc[1] != 'w') {
				naml++;
				continue;
			}
			switch (mandoc_escape(&cp, NULL, NULL)) {
			case ESCAPE_SPECIAL:
			case ESCAPE_UNICODE:
			case ESCAPE_NUMBERED:
			case ESCAPE_OVERSTRIKE:
				naml++;
				break;
			default:
				break;
			}
		}

		/*
		 * Retrieve the replacement string; if it is
		 * undefined, resume searching for escapes.
		 */

		switch (stesc[1]) {
		case '*':
			if (arg_complete)
				res = roff_getstrn(r, stnam, naml);
			break;
		case 'B':
			npos = 0;
			ubuf[0] = arg_complete &&
			    roff_evalnum(r, ln, stnam, &npos,
			      NULL, ROFFNUM_SCALE) &&
			    stnam + npos + 1 == cp ? '1' : '0';
			ubuf[1] = '\0';
			break;
		case 'n':
			if (arg_complete)
				(void)snprintf(ubuf, sizeof(ubuf), "%d",
				    roff_getregn(r, stnam, naml));
			else
				ubuf[0] = '\0';
			break;
		case 'w':
			/* use even incomplete args */
			(void)snprintf(ubuf, sizeof(ubuf), "%d",
			    24 * (int)naml);
			break;
		}

		if (res == NULL) {
			mandoc_vmsg(MANDOCERR_STR_UNDEF,
			    r->parse, ln, (int)(stesc - buf->buf),
			    "%.*s", (int)naml, stnam);
			res = "";
		} else if (buf->sz + strlen(res) > SHRT_MAX) {
			mandoc_msg(MANDOCERR_ROFFLOOP, r->parse,
			    ln, (int)(stesc - buf->buf), NULL);
			return ROFF_IGN;
		}

		/* Replace the escape sequence by the string. */

		*stesc = '\0';
		buf->sz = mandoc_asprintf(&nbuf, "%s%s%s",
		    buf->buf, res, cp) + 1;

		/* Prepare for the next replacement. */

		start = nbuf + pos;
		stesc = nbuf + (stesc - buf->buf) + strlen(res);
		free(buf->buf);
		buf->buf = nbuf;
	}
	return ROFF_CONT;
}

/*
 * Process text streams.
 */
static enum rofferr
roff_parsetext(struct buf *buf, int pos, int *offs)
{
	size_t		 sz;
	const char	*start;
	char		*p;
	int		 isz;
	enum mandoc_esc	 esc;

	/* Spring the input line trap. */

	if (roffit_lines == 1) {
		isz = mandoc_asprintf(&p, "%s\n.%s", buf->buf, roffit_macro);
		free(buf->buf);
		buf->buf = p;
		buf->sz = isz + 1;
		*offs = 0;
		free(roffit_macro);
		roffit_lines = 0;
		return ROFF_REPARSE;
	} else if (roffit_lines > 1)
		--roffit_lines;

	/* Convert all breakable hyphens into ASCII_HYPH. */

	start = p = buf->buf + pos;

	while (*p != '\0') {
		sz = strcspn(p, "-\\");
		p += sz;

		if (*p == '\0')
			break;

		if (*p == '\\') {
			/* Skip over escapes. */
			p++;
			esc = mandoc_escape((const char **)&p, NULL, NULL);
			if (esc == ESCAPE_ERROR)
				break;
			while (*p == '-')
				p++;
			continue;
		} else if (p == start) {
			p++;
			continue;
		}

		if (isalpha((unsigned char)p[-1]) &&
		    isalpha((unsigned char)p[1]))
			*p = ASCII_HYPH;
		p++;
	}
	return ROFF_CONT;
}

enum rofferr
roff_parseln(struct roff *r, int ln, struct buf *buf, int *offs)
{
	enum rofft	 t;
	enum rofferr	 e;
	int		 pos;	/* parse point */
	int		 spos;	/* saved parse point for messages */
	int		 ppos;	/* original offset in buf->buf */
	int		 ctl;	/* macro line (boolean) */

	ppos = pos = *offs;

	/* Handle in-line equation delimiters. */

	if (r->tbl == NULL &&
	    r->last_eqn != NULL && r->last_eqn->delim &&
	    (r->eqn == NULL || r->eqn_inline)) {
		e = roff_eqndelim(r, buf, pos);
		if (e == ROFF_REPARSE)
			return e;
		assert(e == ROFF_CONT);
	}

	/* Expand some escape sequences. */

	e = roff_res(r, buf, ln, pos);
	if (e == ROFF_IGN)
		return e;
	assert(e == ROFF_CONT);

	ctl = roff_getcontrol(r, buf->buf, &pos);

	/*
	 * First, if a scope is open and we're not a macro, pass the
	 * text through the macro's filter.
	 * Equations process all content themselves.
	 * Tables process almost all content themselves, but we want
	 * to warn about macros before passing it there.
	 */

	if (r->last != NULL && ! ctl) {
		t = r->last->tok;
		assert(roffs[t].text);
		e = (*roffs[t].text)(r, t, buf, ln, pos, pos, offs);
		assert(e == ROFF_IGN || e == ROFF_CONT);
		if (e != ROFF_CONT)
			return e;
	}
	if (r->eqn != NULL)
		return eqn_read(&r->eqn, ln, buf->buf, ppos, offs);
	if (r->tbl != NULL && ( ! ctl || buf->buf[pos] == '\0'))
		return tbl_read(r->tbl, ln, buf->buf, ppos);
	if ( ! ctl)
		return roff_parsetext(buf, pos, offs);

	/* Skip empty request lines. */

	if (buf->buf[pos] == '"') {
		mandoc_msg(MANDOCERR_COMMENT_BAD, r->parse,
		    ln, pos, NULL);
		return ROFF_IGN;
	} else if (buf->buf[pos] == '\0')
		return ROFF_IGN;

	/*
	 * If a scope is open, go to the child handler for that macro,
	 * as it may want to preprocess before doing anything with it.
	 * Don't do so if an equation is open.
	 */

	if (r->last) {
		t = r->last->tok;
		assert(roffs[t].sub);
		return (*roffs[t].sub)(r, t, buf, ln, ppos, pos, offs);
	}

	/* No scope is open.  This is a new request or macro. */

	spos = pos;
	t = roff_parse(r, buf->buf, &pos, ln, ppos);

	/* Tables ignore most macros. */

	if (r->tbl != NULL && (t == ROFF_MAX || t == ROFF_TS)) {
		mandoc_msg(MANDOCERR_TBLMACRO, r->parse,
		    ln, pos, buf->buf + spos);
		if (t == ROFF_TS)
			return ROFF_IGN;
		while (buf->buf[pos] != '\0' && buf->buf[pos] != ' ')
			pos++;
		while (buf->buf[pos] != '\0' && buf->buf[pos] == ' ')
			pos++;
		return tbl_read(r->tbl, ln, buf->buf, pos);
	}

	/*
	 * This is neither a roff request nor a user-defined macro.
	 * Let the standard macro set parsers handle it.
	 */

	if (t == ROFF_MAX)
		return ROFF_CONT;

	/* Execute a roff request or a user defined macro. */

	assert(roffs[t].proc);
	return (*roffs[t].proc)(r, t, buf, ln, ppos, pos, offs);
}

void
roff_endparse(struct roff *r)
{

	if (r->last)
		mandoc_msg(MANDOCERR_BLK_NOEND, r->parse,
		    r->last->line, r->last->col,
		    roffs[r->last->tok].name);

	if (r->eqn) {
		mandoc_msg(MANDOCERR_BLK_NOEND, r->parse,
		    r->eqn->eqn.ln, r->eqn->eqn.pos, "EQ");
		eqn_end(&r->eqn);
	}

	if (r->tbl) {
		mandoc_msg(MANDOCERR_BLK_NOEND, r->parse,
		    r->tbl->line, r->tbl->pos, "TS");
		tbl_end(&r->tbl);
	}
}

/*
 * Parse a roff node's type from the input buffer.  This must be in the
 * form of ".foo xxx" in the usual way.
 */
static enum rofft
roff_parse(struct roff *r, char *buf, int *pos, int ln, int ppos)
{
	char		*cp;
	const char	*mac;
	size_t		 maclen;
	enum rofft	 t;

	cp = buf + *pos;

	if ('\0' == *cp || '"' == *cp || '\t' == *cp || ' ' == *cp)
		return ROFF_MAX;

	mac = cp;
	maclen = roff_getname(r, &cp, ln, ppos);

	t = (r->current_string = roff_getstrn(r, mac, maclen))
	    ? ROFF_USERDEF : roffhash_find(mac, maclen);

	if (ROFF_MAX != t)
		*pos = cp - buf;

	return t;
}

/* --- handling of request blocks ----------------------------------------- */

static enum rofferr
roff_cblock(ROFF_ARGS)
{

	/*
	 * A block-close `..' should only be invoked as a child of an
	 * ignore macro, otherwise raise a warning and just ignore it.
	 */

	if (r->last == NULL) {
		mandoc_msg(MANDOCERR_BLK_NOTOPEN, r->parse,
		    ln, ppos, "..");
		return ROFF_IGN;
	}

	switch (r->last->tok) {
	case ROFF_am:
		/* ROFF_am1 is remapped to ROFF_am in roff_block(). */
	case ROFF_ami:
	case ROFF_de:
		/* ROFF_de1 is remapped to ROFF_de in roff_block(). */
	case ROFF_dei:
	case ROFF_ig:
		break;
	default:
		mandoc_msg(MANDOCERR_BLK_NOTOPEN, r->parse,
		    ln, ppos, "..");
		return ROFF_IGN;
	}

	if (buf->buf[pos] != '\0')
		mandoc_vmsg(MANDOCERR_ARG_SKIP, r->parse, ln, pos,
		    ".. %s", buf->buf + pos);

	roffnode_pop(r);
	roffnode_cleanscope(r);
	return ROFF_IGN;

}

static void
roffnode_cleanscope(struct roff *r)
{

	while (r->last) {
		if (--r->last->endspan != 0)
			break;
		roffnode_pop(r);
	}
}

static void
roff_ccond(struct roff *r, int ln, int ppos)
{

	if (NULL == r->last) {
		mandoc_msg(MANDOCERR_BLK_NOTOPEN, r->parse,
		    ln, ppos, "\\}");
		return;
	}

	switch (r->last->tok) {
	case ROFF_el:
	case ROFF_ie:
	case ROFF_if:
		break;
	default:
		mandoc_msg(MANDOCERR_BLK_NOTOPEN, r->parse,
		    ln, ppos, "\\}");
		return;
	}

	if (r->last->endspan > -1) {
		mandoc_msg(MANDOCERR_BLK_NOTOPEN, r->parse,
		    ln, ppos, "\\}");
		return;
	}

	roffnode_pop(r);
	roffnode_cleanscope(r);
	return;
}

static enum rofferr
roff_block(ROFF_ARGS)
{
	const char	*name;
	char		*iname, *cp;
	size_t		 namesz;

	/* Ignore groff compatibility mode for now. */

	if (tok == ROFF_de1)
		tok = ROFF_de;
	else if (tok == ROFF_dei1)
		tok = ROFF_dei;
	else if (tok == ROFF_am1)
		tok = ROFF_am;
	else if (tok == ROFF_ami1)
		tok = ROFF_ami;

	/* Parse the macro name argument. */

	cp = buf->buf + pos;
	if (tok == ROFF_ig) {
		iname = NULL;
		namesz = 0;
	} else {
		iname = cp;
		namesz = roff_getname(r, &cp, ln, ppos);
		iname[namesz] = '\0';
	}

	/* Resolve the macro name argument if it is indirect. */

	if (namesz && (tok == ROFF_dei || tok == ROFF_ami)) {
		if ((name = roff_getstrn(r, iname, namesz)) == NULL) {
			mandoc_vmsg(MANDOCERR_STR_UNDEF,
			    r->parse, ln, (int)(iname - buf->buf),
			    "%.*s", (int)namesz, iname);
			namesz = 0;
		} else
			namesz = strlen(name);
	} else
		name = iname;

	if (namesz == 0 && tok != ROFF_ig) {
		mandoc_msg(MANDOCERR_REQ_EMPTY, r->parse,
		    ln, ppos, roffs[tok].name);
		return ROFF_IGN;
	}

	roffnode_push(r, tok, name, ln, ppos);

	/*
	 * At the beginning of a `de' macro, clear the existing string
	 * with the same name, if there is one.  New content will be
	 * appended from roff_block_text() in multiline mode.
	 */

	if (tok == ROFF_de || tok == ROFF_dei)
		roff_setstrn(&r->strtab, name, namesz, "", 0, 0);

	if (*cp == '\0')
		return ROFF_IGN;

	/* Get the custom end marker. */

	iname = cp;
	namesz = roff_getname(r, &cp, ln, ppos);

	/* Resolve the end marker if it is indirect. */

	if (namesz && (tok == ROFF_dei || tok == ROFF_ami)) {
		if ((name = roff_getstrn(r, iname, namesz)) == NULL) {
			mandoc_vmsg(MANDOCERR_STR_UNDEF,
			    r->parse, ln, (int)(iname - buf->buf),
			    "%.*s", (int)namesz, iname);
			namesz = 0;
		} else
			namesz = strlen(name);
	} else
		name = iname;

	if (namesz)
		r->last->end = mandoc_strndup(name, namesz);

	if (*cp != '\0')
		mandoc_vmsg(MANDOCERR_ARG_EXCESS, r->parse,
		    ln, pos, ".%s ... %s", roffs[tok].name, cp);

	return ROFF_IGN;
}

static enum rofferr
roff_block_sub(ROFF_ARGS)
{
	enum rofft	t;
	int		i, j;

	/*
	 * First check whether a custom macro exists at this level.  If
	 * it does, then check against it.  This is some of groff's
	 * stranger behaviours.  If we encountered a custom end-scope
	 * tag and that tag also happens to be a "real" macro, then we
	 * need to try interpreting it again as a real macro.  If it's
	 * not, then return ignore.  Else continue.
	 */

	if (r->last->end) {
		for (i = pos, j = 0; r->last->end[j]; j++, i++)
			if (buf->buf[i] != r->last->end[j])
				break;

		if (r->last->end[j] == '\0' &&
		    (buf->buf[i] == '\0' ||
		     buf->buf[i] == ' ' ||
		     buf->buf[i] == '\t')) {
			roffnode_pop(r);
			roffnode_cleanscope(r);

			while (buf->buf[i] == ' ' || buf->buf[i] == '\t')
				i++;

			pos = i;
			if (roff_parse(r, buf->buf, &pos, ln, ppos) !=
			    ROFF_MAX)
				return ROFF_RERUN;
			return ROFF_IGN;
		}
	}

	/*
	 * If we have no custom end-query or lookup failed, then try
	 * pulling it out of the hashtable.
	 */

	t = roff_parse(r, buf->buf, &pos, ln, ppos);

	if (t != ROFF_cblock) {
		if (tok != ROFF_ig)
			roff_setstr(r, r->last->name, buf->buf + ppos, 2);
		return ROFF_IGN;
	}

	assert(roffs[t].proc);
	return (*roffs[t].proc)(r, t, buf, ln, ppos, pos, offs);
}

static enum rofferr
roff_block_text(ROFF_ARGS)
{

	if (tok != ROFF_ig)
		roff_setstr(r, r->last->name, buf->buf + pos, 2);

	return ROFF_IGN;
}

static enum rofferr
roff_cond_sub(ROFF_ARGS)
{
	enum rofft	 t;
	char		*ep;
	int		 rr;

	rr = r->last->rule;
	roffnode_cleanscope(r);
	t = roff_parse(r, buf->buf, &pos, ln, ppos);

	/*
	 * Fully handle known macros when they are structurally
	 * required or when the conditional evaluated to true.
	 */

	if ((t != ROFF_MAX) &&
	    (rr || roffs[t].flags & ROFFMAC_STRUCT)) {
		assert(roffs[t].proc);
		return (*roffs[t].proc)(r, t, buf, ln, ppos, pos, offs);
	}

	/*
	 * If `\}' occurs on a macro line without a preceding macro,
	 * drop the line completely.
	 */

	ep = buf->buf + pos;
	if (ep[0] == '\\' && ep[1] == '}')
		rr = 0;

	/* Always check for the closing delimiter `\}'. */

	while ((ep = strchr(ep, '\\')) != NULL) {
		if (*(++ep) == '}') {
			*ep = '&';
			roff_ccond(r, ln, ep - buf->buf - 1);
		}
		if (*ep != '\0')
			++ep;
	}
	return rr ? ROFF_CONT : ROFF_IGN;
}

static enum rofferr
roff_cond_text(ROFF_ARGS)
{
	char		*ep;
	int		 rr;

	rr = r->last->rule;
	roffnode_cleanscope(r);

	ep = buf->buf + pos;
	while ((ep = strchr(ep, '\\')) != NULL) {
		if (*(++ep) == '}') {
			*ep = '&';
			roff_ccond(r, ln, ep - buf->buf - 1);
		}
		if (*ep != '\0')
			++ep;
	}
	return rr ? ROFF_CONT : ROFF_IGN;
}

/* --- handling of numeric and conditional expressions -------------------- */

/*
 * Parse a single signed integer number.  Stop at the first non-digit.
 * If there is at least one digit, return success and advance the
 * parse point, else return failure and let the parse point unchanged.
 * Ignore overflows, treat them just like the C language.
 */
static int
roff_getnum(const char *v, int *pos, int *res, int flags)
{
	int	 myres, scaled, n, p;

	if (NULL == res)
		res = &myres;

	p = *pos;
	n = v[p] == '-';
	if (n || v[p] == '+')
		p++;

	if (flags & ROFFNUM_WHITE)
		while (isspace((unsigned char)v[p]))
			p++;

	for (*res = 0; isdigit((unsigned char)v[p]); p++)
		*res = 10 * *res + v[p] - '0';
	if (p == *pos + n)
		return 0;

	if (n)
		*res = -*res;

	/* Each number may be followed by one optional scaling unit. */

	switch (v[p]) {
	case 'f':
		scaled = *res * 65536;
		break;
	case 'i':
		scaled = *res * 240;
		break;
	case 'c':
		scaled = *res * 240 / 2.54;
		break;
	case 'v':
	case 'P':
		scaled = *res * 40;
		break;
	case 'm':
	case 'n':
		scaled = *res * 24;
		break;
	case 'p':
		scaled = *res * 10 / 3;
		break;
	case 'u':
		scaled = *res;
		break;
	case 'M':
		scaled = *res * 6 / 25;
		break;
	default:
		scaled = *res;
		p--;
		break;
	}
	if (flags & ROFFNUM_SCALE)
		*res = scaled;

	*pos = p + 1;
	return 1;
}

/*
 * Evaluate a string comparison condition.
 * The first character is the delimiter.
 * Succeed if the string up to its second occurrence
 * matches the string up to its third occurence.
 * Advance the cursor after the third occurrence
 * or lacking that, to the end of the line.
 */
static int
roff_evalstrcond(const char *v, int *pos)
{
	const char	*s1, *s2, *s3;
	int		 match;

	match = 0;
	s1 = v + *pos;		/* initial delimiter */
	s2 = s1 + 1;		/* for scanning the first string */
	s3 = strchr(s2, *s1);	/* for scanning the second string */

	if (NULL == s3)		/* found no middle delimiter */
		goto out;

	while ('\0' != *++s3) {
		if (*s2 != *s3) {  /* mismatch */
			s3 = strchr(s3, *s1);
			break;
		}
		if (*s3 == *s1) {  /* found the final delimiter */
			match = 1;
			break;
		}
		s2++;
	}

out:
	if (NULL == s3)
		s3 = strchr(s2, '\0');
	else if (*s3 != '\0')
		s3++;
	*pos = s3 - v;
	return match;
}

/*
 * Evaluate an optionally negated single character, numerical,
 * or string condition.
 */
static int
roff_evalcond(struct roff *r, int ln, char *v, int *pos)
{
	char	*cp, *name;
	size_t	 sz;
	int	 number, savepos, wanttrue;

	if ('!' == v[*pos]) {
		wanttrue = 0;
		(*pos)++;
	} else
		wanttrue = 1;

	switch (v[*pos]) {
	case '\0':
		return 0;
	case 'n':
	case 'o':
		(*pos)++;
		return wanttrue;
	case 'c':
	case 'd':
	case 'e':
	case 't':
	case 'v':
		(*pos)++;
		return !wanttrue;
	case 'r':
		cp = name = v + ++*pos;
		sz = roff_getname(r, &cp, ln, *pos);
		*pos = cp - v;
		return (sz && roff_hasregn(r, name, sz)) == wanttrue;
	default:
		break;
	}

	savepos = *pos;
	if (roff_evalnum(r, ln, v, pos, &number, ROFFNUM_SCALE))
		return (number > 0) == wanttrue;
	else if (*pos == savepos)
		return roff_evalstrcond(v, pos) == wanttrue;
	else
		return 0;
}

static enum rofferr
roff_line_ignore(ROFF_ARGS)
{

	return ROFF_IGN;
}

static enum rofferr
roff_insec(ROFF_ARGS)
{

	mandoc_msg(MANDOCERR_REQ_INSEC, r->parse,
	    ln, ppos, roffs[tok].name);
	return ROFF_IGN;
}

static enum rofferr
roff_unsupp(ROFF_ARGS)
{

	mandoc_msg(MANDOCERR_REQ_UNSUPP, r->parse,
	    ln, ppos, roffs[tok].name);
	return ROFF_IGN;
}

static enum rofferr
roff_cond(ROFF_ARGS)
{

	roffnode_push(r, tok, NULL, ln, ppos);

	/*
	 * An `.el' has no conditional body: it will consume the value
	 * of the current rstack entry set in prior `ie' calls or
	 * defaults to DENY.
	 *
	 * If we're not an `el', however, then evaluate the conditional.
	 */

	r->last->rule = tok == ROFF_el ?
	    (r->rstackpos < 0 ? 0 : r->rstack[r->rstackpos--]) :
	    roff_evalcond(r, ln, buf->buf, &pos);

	/*
	 * An if-else will put the NEGATION of the current evaluated
	 * conditional into the stack of rules.
	 */

	if (tok == ROFF_ie) {
		if (r->rstackpos + 1 == r->rstacksz) {
			r->rstacksz += 16;
			r->rstack = mandoc_reallocarray(r->rstack,
			    r->rstacksz, sizeof(int));
		}
		r->rstack[++r->rstackpos] = !r->last->rule;
	}

	/* If the parent has false as its rule, then so do we. */

	if (r->last->parent && !r->last->parent->rule)
		r->last->rule = 0;

	/*
	 * Determine scope.
	 * If there is nothing on the line after the conditional,
	 * not even whitespace, use next-line scope.
	 */

	if (buf->buf[pos] == '\0') {
		r->last->endspan = 2;
		goto out;
	}

	while (buf->buf[pos] == ' ')
		pos++;

	/* An opening brace requests multiline scope. */

	if (buf->buf[pos] == '\\' && buf->buf[pos + 1] == '{') {
		r->last->endspan = -1;
		pos += 2;
		while (buf->buf[pos] == ' ')
			pos++;
		goto out;
	}

	/*
	 * Anything else following the conditional causes
	 * single-line scope.  Warn if the scope contains
	 * nothing but trailing whitespace.
	 */

	if (buf->buf[pos] == '\0')
		mandoc_msg(MANDOCERR_COND_EMPTY, r->parse,
		    ln, ppos, roffs[tok].name);

	r->last->endspan = 1;

out:
	*offs = pos;
	return ROFF_RERUN;
}

static enum rofferr
roff_ds(ROFF_ARGS)
{
	char		*string;
	const char	*name;
	size_t		 namesz;

	/* Ignore groff compatibility mode for now. */

	if (tok == ROFF_ds1)
		tok = ROFF_ds;
	else if (tok == ROFF_as1)
		tok = ROFF_as;

	/*
	 * The first word is the name of the string.
	 * If it is empty or terminated by an escape sequence,
	 * abort the `ds' request without defining anything.
	 */

	name = string = buf->buf + pos;
	if (*name == '\0')
		return ROFF_IGN;

	namesz = roff_getname(r, &string, ln, pos);
	if (name[namesz] == '\\')
		return ROFF_IGN;

	/* Read past the initial double-quote, if any. */
	if (*string == '"')
		string++;

	/* The rest is the value. */
	roff_setstrn(&r->strtab, name, namesz, string, strlen(string),
	    ROFF_as == tok);
	return ROFF_IGN;
}

/*
 * Parse a single operator, one or two characters long.
 * If the operator is recognized, return success and advance the
 * parse point, else return failure and let the parse point unchanged.
 */
static int
roff_getop(const char *v, int *pos, char *res)
{

	*res = v[*pos];

	switch (*res) {
	case '+':
	case '-':
	case '*':
	case '/':
	case '%':
	case '&':
	case ':':
		break;
	case '<':
		switch (v[*pos + 1]) {
		case '=':
			*res = 'l';
			(*pos)++;
			break;
		case '>':
			*res = '!';
			(*pos)++;
			break;
		case '?':
			*res = 'i';
			(*pos)++;
			break;
		default:
			break;
		}
		break;
	case '>':
		switch (v[*pos + 1]) {
		case '=':
			*res = 'g';
			(*pos)++;
			break;
		case '?':
			*res = 'a';
			(*pos)++;
			break;
		default:
			break;
		}
		break;
	case '=':
		if ('=' == v[*pos + 1])
			(*pos)++;
		break;
	default:
		return 0;
	}
	(*pos)++;

	return *res;
}

/*
 * Evaluate either a parenthesized numeric expression
 * or a single signed integer number.
 */
static int
roff_evalpar(struct roff *r, int ln,
	const char *v, int *pos, int *res, int flags)
{

	if ('(' != v[*pos])
		return roff_getnum(v, pos, res, flags);

	(*pos)++;
	if ( ! roff_evalnum(r, ln, v, pos, res, flags | ROFFNUM_WHITE))
		return 0;

	/*
	 * Omission of the closing parenthesis
	 * is an error in validation mode,
	 * but ignored in evaluation mode.
	 */

	if (')' == v[*pos])
		(*pos)++;
	else if (NULL == res)
		return 0;

	return 1;
}

/*
 * Evaluate a complete numeric expression.
 * Proceed left to right, there is no concept of precedence.
 */
static int
roff_evalnum(struct roff *r, int ln, const char *v,
	int *pos, int *res, int flags)
{
	int		 mypos, operand2;
	char		 operator;

	if (NULL == pos) {
		mypos = 0;
		pos = &mypos;
	}

	if (flags & ROFFNUM_WHITE)
		while (isspace((unsigned char)v[*pos]))
			(*pos)++;

	if ( ! roff_evalpar(r, ln, v, pos, res, flags))
		return 0;

	while (1) {
		if (flags & ROFFNUM_WHITE)
			while (isspace((unsigned char)v[*pos]))
				(*pos)++;

		if ( ! roff_getop(v, pos, &operator))
			break;

		if (flags & ROFFNUM_WHITE)
			while (isspace((unsigned char)v[*pos]))
				(*pos)++;

		if ( ! roff_evalpar(r, ln, v, pos, &operand2, flags))
			return 0;

		if (flags & ROFFNUM_WHITE)
			while (isspace((unsigned char)v[*pos]))
				(*pos)++;

		if (NULL == res)
			continue;

		switch (operator) {
		case '+':
			*res += operand2;
			break;
		case '-':
			*res -= operand2;
			break;
		case '*':
			*res *= operand2;
			break;
		case '/':
			if (operand2 == 0) {
				mandoc_msg(MANDOCERR_DIVZERO,
					r->parse, ln, *pos, v);
				*res = 0;
				break;
			}
			*res /= operand2;
			break;
		case '%':
			if (operand2 == 0) {
				mandoc_msg(MANDOCERR_DIVZERO,
					r->parse, ln, *pos, v);
				*res = 0;
				break;
			}
			*res %= operand2;
			break;
		case '<':
			*res = *res < operand2;
			break;
		case '>':
			*res = *res > operand2;
			break;
		case 'l':
			*res = *res <= operand2;
			break;
		case 'g':
			*res = *res >= operand2;
			break;
		case '=':
			*res = *res == operand2;
			break;
		case '!':
			*res = *res != operand2;
			break;
		case '&':
			*res = *res && operand2;
			break;
		case ':':
			*res = *res || operand2;
			break;
		case 'i':
			if (operand2 < *res)
				*res = operand2;
			break;
		case 'a':
			if (operand2 > *res)
				*res = operand2;
			break;
		default:
			abort();
		}
	}
	return 1;
}

/* --- register management ------------------------------------------------ */

void
roff_setreg(struct roff *r, const char *name, int val, char sign)
{
	struct roffreg	*reg;

	/* Search for an existing register with the same name. */
	reg = r->regtab;

	while (reg && strcmp(name, reg->key.p))
		reg = reg->next;

	if (NULL == reg) {
		/* Create a new register. */
		reg = mandoc_malloc(sizeof(struct roffreg));
		reg->key.p = mandoc_strdup(name);
		reg->key.sz = strlen(name);
		reg->val = 0;
		reg->next = r->regtab;
		r->regtab = reg;
	}

	if ('+' == sign)
		reg->val += val;
	else if ('-' == sign)
		reg->val -= val;
	else
		reg->val = val;
}

/*
 * Handle some predefined read-only number registers.
 * For now, return -1 if the requested register is not predefined;
 * in case a predefined read-only register having the value -1
 * were to turn up, another special value would have to be chosen.
 */
static int
roff_getregro(const struct roff *r, const char *name)
{

	switch (*name) {
	case '$':  /* Number of arguments of the last macro evaluated. */
		return r->argc;
	case 'A':  /* ASCII approximation mode is always off. */
		return 0;
	case 'g':  /* Groff compatibility mode is always on. */
		return 1;
	case 'H':  /* Fixed horizontal resolution. */
		return 24;
	case 'j':  /* Always adjust left margin only. */
		return 0;
	case 'T':  /* Some output device is always defined. */
		return 1;
	case 'V':  /* Fixed vertical resolution. */
		return 40;
	default:
		return -1;
	}
}

int
roff_getreg(const struct roff *r, const char *name)
{
	struct roffreg	*reg;
	int		 val;

	if ('.' == name[0] && '\0' != name[1] && '\0' == name[2]) {
		val = roff_getregro(r, name + 1);
		if (-1 != val)
			return val;
	}

	for (reg = r->regtab; reg; reg = reg->next)
		if (0 == strcmp(name, reg->key.p))
			return reg->val;

	return 0;
}

static int
roff_getregn(const struct roff *r, const char *name, size_t len)
{
	struct roffreg	*reg;
	int		 val;

	if ('.' == name[0] && 2 == len) {
		val = roff_getregro(r, name + 1);
		if (-1 != val)
			return val;
	}

	for (reg = r->regtab; reg; reg = reg->next)
		if (len == reg->key.sz &&
		    0 == strncmp(name, reg->key.p, len))
			return reg->val;

	return 0;
}

static int
roff_hasregn(const struct roff *r, const char *name, size_t len)
{
	struct roffreg	*reg;
	int		 val;

	if ('.' == name[0] && 2 == len) {
		val = roff_getregro(r, name + 1);
		if (-1 != val)
			return 1;
	}

	for (reg = r->regtab; reg; reg = reg->next)
		if (len == reg->key.sz &&
		    0 == strncmp(name, reg->key.p, len))
			return 1;

	return 0;
}

static void
roff_freereg(struct roffreg *reg)
{
	struct roffreg	*old_reg;

	while (NULL != reg) {
		free(reg->key.p);
		old_reg = reg;
		reg = reg->next;
		free(old_reg);
	}
}

static enum rofferr
roff_nr(ROFF_ARGS)
{
	char		*key, *val;
	size_t		 keysz;
	int		 iv;
	char		 sign;

	key = val = buf->buf + pos;
	if (*key == '\0')
		return ROFF_IGN;

	keysz = roff_getname(r, &val, ln, pos);
	if (key[keysz] == '\\')
		return ROFF_IGN;
	key[keysz] = '\0';

	sign = *val;
	if (sign == '+' || sign == '-')
		val++;

	if (roff_evalnum(r, ln, val, NULL, &iv, ROFFNUM_SCALE))
		roff_setreg(r, key, iv, sign);

	return ROFF_IGN;
}

static enum rofferr
roff_rr(ROFF_ARGS)
{
	struct roffreg	*reg, **prev;
	char		*name, *cp;
	size_t		 namesz;

	name = cp = buf->buf + pos;
	if (*name == '\0')
		return ROFF_IGN;
	namesz = roff_getname(r, &cp, ln, pos);
	name[namesz] = '\0';

	prev = &r->regtab;
	while (1) {
		reg = *prev;
		if (reg == NULL || !strcmp(name, reg->key.p))
			break;
		prev = &reg->next;
	}
	if (reg != NULL) {
		*prev = reg->next;
		free(reg->key.p);
		free(reg);
	}
	return ROFF_IGN;
}

/* --- handler functions for roff requests -------------------------------- */

static enum rofferr
roff_rm(ROFF_ARGS)
{
	const char	 *name;
	char		 *cp;
	size_t		  namesz;

	cp = buf->buf + pos;
	while (*cp != '\0') {
		name = cp;
		namesz = roff_getname(r, &cp, ln, (int)(cp - buf->buf));
		roff_setstrn(&r->strtab, name, namesz, NULL, 0, 0);
		if (name[namesz] == '\\')
			break;
	}
	return ROFF_IGN;
}

static enum rofferr
roff_it(ROFF_ARGS)
{
	int		 iv;

	/* Parse the number of lines. */

	if ( ! roff_evalnum(r, ln, buf->buf, &pos, &iv, 0)) {
		mandoc_msg(MANDOCERR_IT_NONUM, r->parse,
		    ln, ppos, buf->buf + 1);
		return ROFF_IGN;
	}

	while (isspace((unsigned char)buf->buf[pos]))
		pos++;

	/*
	 * Arm the input line trap.
	 * Special-casing "an-trap" is an ugly workaround to cope
	 * with DocBook stupidly fiddling with man(7) internals.
	 */

	roffit_lines = iv;
	roffit_macro = mandoc_strdup(iv != 1 ||
	    strcmp(buf->buf + pos, "an-trap") ?
	    buf->buf + pos : "br");
	return ROFF_IGN;
}

static enum rofferr
roff_Dd(ROFF_ARGS)
{
	const char *const	*cp;

	if ((r->options & (MPARSE_MDOC | MPARSE_QUICK)) == 0)
		for (cp = __mdoc_reserved; *cp; cp++)
			roff_setstr(r, *cp, NULL, 0);

	if (r->format == 0)
		r->format = MPARSE_MDOC;

	return ROFF_CONT;
}

static enum rofferr
roff_TH(ROFF_ARGS)
{
	const char *const	*cp;

	if ((r->options & MPARSE_QUICK) == 0)
		for (cp = __man_reserved; *cp; cp++)
			roff_setstr(r, *cp, NULL, 0);

	if (r->format == 0)
		r->format = MPARSE_MAN;

	return ROFF_CONT;
}

static enum rofferr
roff_TE(ROFF_ARGS)
{

	if (NULL == r->tbl)
		mandoc_msg(MANDOCERR_BLK_NOTOPEN, r->parse,
		    ln, ppos, "TE");
	else if ( ! tbl_end(&r->tbl)) {
		free(buf->buf);
		buf->buf = mandoc_strdup(".sp");
		buf->sz = 4;
		return ROFF_REPARSE;
	}
	return ROFF_IGN;
}

static enum rofferr
roff_T_(ROFF_ARGS)
{

	if (NULL == r->tbl)
		mandoc_msg(MANDOCERR_BLK_NOTOPEN, r->parse,
		    ln, ppos, "T&");
	else
		tbl_restart(ppos, ln, r->tbl);

	return ROFF_IGN;
}

/*
 * Handle in-line equation delimiters.
 */
static enum rofferr
roff_eqndelim(struct roff *r, struct buf *buf, int pos)
{
	char		*cp1, *cp2;
	const char	*bef_pr, *bef_nl, *mac, *aft_nl, *aft_pr;

	/*
	 * Outside equations, look for an opening delimiter.
	 * If we are inside an equation, we already know it is
	 * in-line, or this function wouldn't have been called;
	 * so look for a closing delimiter.
	 */

	cp1 = buf->buf + pos;
	cp2 = strchr(cp1, r->eqn == NULL ?
	    r->last_eqn->odelim : r->last_eqn->cdelim);
	if (cp2 == NULL)
		return ROFF_CONT;

	*cp2++ = '\0';
	bef_pr = bef_nl = aft_nl = aft_pr = "";

	/* Handle preceding text, protecting whitespace. */

	if (*buf->buf != '\0') {
		if (r->eqn == NULL)
			bef_pr = "\\&";
		bef_nl = "\n";
	}

	/*
	 * Prepare replacing the delimiter with an equation macro
	 * and drop leading white space from the equation.
	 */

	if (r->eqn == NULL) {
		while (*cp2 == ' ')
			cp2++;
		mac = ".EQ";
	} else
		mac = ".EN";

	/* Handle following text, protecting whitespace. */

	if (*cp2 != '\0') {
		aft_nl = "\n";
		if (r->eqn != NULL)
			aft_pr = "\\&";
	}

	/* Do the actual replacement. */

	buf->sz = mandoc_asprintf(&cp1, "%s%s%s%s%s%s%s", buf->buf,
	    bef_pr, bef_nl, mac, aft_nl, aft_pr, cp2) + 1;
	free(buf->buf);
	buf->buf = cp1;

	/* Toggle the in-line state of the eqn subsystem. */

	r->eqn_inline = r->eqn == NULL;
	return ROFF_REPARSE;
}

static enum rofferr
roff_EQ(ROFF_ARGS)
{
	struct eqn_node *e;

	assert(r->eqn == NULL);
	e = eqn_alloc(ppos, ln, r->parse);

	if (r->last_eqn) {
		r->last_eqn->next = e;
		e->delim = r->last_eqn->delim;
		e->odelim = r->last_eqn->odelim;
		e->cdelim = r->last_eqn->cdelim;
	} else
		r->first_eqn = r->last_eqn = e;

	r->eqn = r->last_eqn = e;

	if (buf->buf[pos] != '\0')
		mandoc_vmsg(MANDOCERR_ARG_SKIP, r->parse, ln, pos,
		    ".EQ %s", buf->buf + pos);

	return ROFF_IGN;
}

static enum rofferr
roff_EN(ROFF_ARGS)
{

	mandoc_msg(MANDOCERR_BLK_NOTOPEN, r->parse, ln, ppos, "EN");
	return ROFF_IGN;
}

static enum rofferr
roff_TS(ROFF_ARGS)
{
	struct tbl_node	*tbl;

	if (r->tbl) {
		mandoc_msg(MANDOCERR_BLK_BROKEN, r->parse,
		    ln, ppos, "TS breaks TS");
		tbl_end(&r->tbl);
	}

	tbl = tbl_alloc(ppos, ln, r->parse);

	if (r->last_tbl)
		r->last_tbl->next = tbl;
	else
		r->first_tbl = r->last_tbl = tbl;

	r->tbl = r->last_tbl = tbl;
	return ROFF_IGN;
}

static enum rofferr
roff_brp(ROFF_ARGS)
{

	buf->buf[pos - 1] = '\0';
	return ROFF_CONT;
}

static enum rofferr
roff_cc(ROFF_ARGS)
{
	const char	*p;

	p = buf->buf + pos;

	if (*p == '\0' || (r->control = *p++) == '.')
		r->control = 0;

	if (*p != '\0')
		mandoc_vmsg(MANDOCERR_ARG_EXCESS, r->parse,
		    ln, p - buf->buf, "cc ... %s", p);

	return ROFF_IGN;
}

static enum rofferr
roff_tr(ROFF_ARGS)
{
	const char	*p, *first, *second;
	size_t		 fsz, ssz;
	enum mandoc_esc	 esc;

	p = buf->buf + pos;

	if (*p == '\0') {
		mandoc_msg(MANDOCERR_REQ_EMPTY, r->parse, ln, ppos, "tr");
		return ROFF_IGN;
	}

	while (*p != '\0') {
		fsz = ssz = 1;

		first = p++;
		if (*first == '\\') {
			esc = mandoc_escape(&p, NULL, NULL);
			if (esc == ESCAPE_ERROR) {
				mandoc_msg(MANDOCERR_ESC_BAD, r->parse,
				    ln, (int)(p - buf->buf), first);
				return ROFF_IGN;
			}
			fsz = (size_t)(p - first);
		}

		second = p++;
		if (*second == '\\') {
			esc = mandoc_escape(&p, NULL, NULL);
			if (esc == ESCAPE_ERROR) {
				mandoc_msg(MANDOCERR_ESC_BAD, r->parse,
				    ln, (int)(p - buf->buf), second);
				return ROFF_IGN;
			}
			ssz = (size_t)(p - second);
		} else if (*second == '\0') {
			mandoc_vmsg(MANDOCERR_TR_ODD, r->parse,
			    ln, first - buf->buf, "tr %s", first);
			second = " ";
			p--;
		}

		if (fsz > 1) {
			roff_setstrn(&r->xmbtab, first, fsz,
			    second, ssz, 0);
			continue;
		}

		if (r->xtab == NULL)
			r->xtab = mandoc_calloc(128,
			    sizeof(struct roffstr));

		free(r->xtab[(int)*first].p);
		r->xtab[(int)*first].p = mandoc_strndup(second, ssz);
		r->xtab[(int)*first].sz = ssz;
	}

	return ROFF_IGN;
}

static enum rofferr
roff_so(ROFF_ARGS)
{
	char *name, *cp;

	name = buf->buf + pos;
	mandoc_vmsg(MANDOCERR_SO, r->parse, ln, ppos, "so %s", name);

	/*
	 * Handle `so'.  Be EXTREMELY careful, as we shouldn't be
	 * opening anything that's not in our cwd or anything beneath
	 * it.  Thus, explicitly disallow traversing up the file-system
	 * or using absolute paths.
	 */

	if (*name == '/' || strstr(name, "../") || strstr(name, "/..")) {
		mandoc_vmsg(MANDOCERR_SO_PATH, r->parse, ln, ppos,
		    ".so %s", name);
		buf->sz = mandoc_asprintf(&cp,
		    ".sp\nSee the file %s.\n.sp", name) + 1;
		free(buf->buf);
		buf->buf = cp;
		*offs = 0;
		return ROFF_REPARSE;
	}

	*offs = pos;
	return ROFF_SO;
}

/* --- user defined strings and macros ------------------------------------ */

static enum rofferr
roff_userdef(ROFF_ARGS)
{
	const char	 *arg[9], *ap;
	char		 *cp, *n1, *n2;
	int		  i, ib, ie;
	size_t		  asz, rsz;

	/*
	 * Collect pointers to macro argument strings
	 * and NUL-terminate them.
	 */

	r->argc = 0;
	cp = buf->buf + pos;
	for (i = 0; i < 9; i++) {
		if (*cp == '\0')
			arg[i] = "";
		else {
			arg[i] = mandoc_getarg(r->parse, &cp, ln, &pos);
			r->argc = i + 1;
		}
	}

	/*
	 * Expand macro arguments.
	 */

	buf->sz = strlen(r->current_string) + 1;
	n1 = cp = mandoc_malloc(buf->sz);
	memcpy(n1, r->current_string, buf->sz);
	while (*cp != '\0') {

		/* Scan ahead for the next argument invocation. */

		if (*cp++ != '\\')
			continue;
		if (*cp++ != '$')
			continue;
		if (*cp == '*') {  /* \\$* inserts all arguments */
			ib = 0;
			ie = r->argc - 1;
		} else {  /* \\$1 .. \\$9 insert one argument */
			ib = ie = *cp - '1';
			if (ib < 0 || ib > 8)
				continue;
		}
		cp -= 2;

		/*
		 * Determine the size of the expanded argument,
		 * taking escaping of quotes into account.
		 */

		asz = ie > ib ? ie - ib : 0;  /* for blanks */
		for (i = ib; i <= ie; i++) {
			for (ap = arg[i]; *ap != '\0'; ap++) {
				asz++;
				if (*ap == '"')
					asz += 3;
			}
		}
		if (asz != 3) {

			/*
			 * Determine the size of the rest of the
			 * unexpanded macro, including the NUL.
			 */

			rsz = buf->sz - (cp - n1) - 3;

			/*
			 * When shrinking, move before
			 * releasing the storage.
			 */

			if (asz < 3)
				memmove(cp + asz, cp + 3, rsz);

			/*
			 * Resize the storage for the macro
			 * and readjust the parse pointer.
			 */

			buf->sz += asz - 3;
			n2 = mandoc_realloc(n1, buf->sz);
			cp = n2 + (cp - n1);
			n1 = n2;

			/*
			 * When growing, make room
			 * for the expanded argument.
			 */

			if (asz > 3)
				memmove(cp + asz, cp + 3, rsz);
		}

		/* Copy the expanded argument, escaping quotes. */

		n2 = cp;
		for (i = ib; i <= ie; i++) {
			for (ap = arg[i]; *ap != '\0'; ap++) {
				if (*ap == '"') {
					memcpy(n2, "\\(dq", 4);
					n2 += 4;
				} else
					*n2++ = *ap;
			}
			if (i < ie)
				*n2++ = ' ';
		}
	}

	/*
	 * Replace the macro invocation
	 * by the expanded macro.
	 */

	free(buf->buf);
	buf->buf = n1;
	*offs = 0;

	return buf->sz > 1 && buf->buf[buf->sz - 2] == '\n' ?
	   ROFF_REPARSE : ROFF_APPEND;
}

static size_t
roff_getname(struct roff *r, char **cpp, int ln, int pos)
{
	char	 *name, *cp;
	size_t	  namesz;

	name = *cpp;
	if ('\0' == *name)
		return 0;

	/* Read until end of name and terminate it with NUL. */
	for (cp = name; 1; cp++) {
		if ('\0' == *cp || ' ' == *cp) {
			namesz = cp - name;
			break;
		}
		if ('\\' != *cp)
			continue;
		namesz = cp - name;
		if ('{' == cp[1] || '}' == cp[1])
			break;
		cp++;
		if ('\\' == *cp)
			continue;
		mandoc_vmsg(MANDOCERR_NAMESC, r->parse, ln, pos,
		    "%.*s", (int)(cp - name + 1), name);
		mandoc_escape((const char **)&cp, NULL, NULL);
		break;
	}

	/* Read past spaces. */
	while (' ' == *cp)
		cp++;

	*cpp = cp;
	return namesz;
}

/*
 * Store *string into the user-defined string called *name.
 * To clear an existing entry, call with (*r, *name, NULL, 0).
 * append == 0: replace mode
 * append == 1: single-line append mode
 * append == 2: multiline append mode, append '\n' after each call
 */
static void
roff_setstr(struct roff *r, const char *name, const char *string,
	int append)
{

	roff_setstrn(&r->strtab, name, strlen(name), string,
	    string ? strlen(string) : 0, append);
}

static void
roff_setstrn(struct roffkv **r, const char *name, size_t namesz,
		const char *string, size_t stringsz, int append)
{
	struct roffkv	*n;
	char		*c;
	int		 i;
	size_t		 oldch, newch;

	/* Search for an existing string with the same name. */
	n = *r;

	while (n && (namesz != n->key.sz ||
			strncmp(n->key.p, name, namesz)))
		n = n->next;

	if (NULL == n) {
		/* Create a new string table entry. */
		n = mandoc_malloc(sizeof(struct roffkv));
		n->key.p = mandoc_strndup(name, namesz);
		n->key.sz = namesz;
		n->val.p = NULL;
		n->val.sz = 0;
		n->next = *r;
		*r = n;
	} else if (0 == append) {
		free(n->val.p);
		n->val.p = NULL;
		n->val.sz = 0;
	}

	if (NULL == string)
		return;

	/*
	 * One additional byte for the '\n' in multiline mode,
	 * and one for the terminating '\0'.
	 */
	newch = stringsz + (1 < append ? 2u : 1u);

	if (NULL == n->val.p) {
		n->val.p = mandoc_malloc(newch);
		*n->val.p = '\0';
		oldch = 0;
	} else {
		oldch = n->val.sz;
		n->val.p = mandoc_realloc(n->val.p, oldch + newch);
	}

	/* Skip existing content in the destination buffer. */
	c = n->val.p + (int)oldch;

	/* Append new content to the destination buffer. */
	i = 0;
	while (i < (int)stringsz) {
		/*
		 * Rudimentary roff copy mode:
		 * Handle escaped backslashes.
		 */
		if ('\\' == string[i] && '\\' == string[i + 1])
			i++;
		*c++ = string[i++];
	}

	/* Append terminating bytes. */
	if (1 < append)
		*c++ = '\n';

	*c = '\0';
	n->val.sz = (int)(c - n->val.p);
}

static const char *
roff_getstrn(const struct roff *r, const char *name, size_t len)
{
	const struct roffkv *n;
	int i;

	for (n = r->strtab; n; n = n->next)
		if (0 == strncmp(name, n->key.p, len) &&
		    '\0' == n->key.p[(int)len])
			return n->val.p;

	for (i = 0; i < PREDEFS_MAX; i++)
		if (0 == strncmp(name, predefs[i].name, len) &&
				'\0' == predefs[i].name[(int)len])
			return predefs[i].str;

	return NULL;
}

static void
roff_freestr(struct roffkv *r)
{
	struct roffkv	 *n, *nn;

	for (n = r; n; n = nn) {
		free(n->key.p);
		free(n->val.p);
		nn = n->next;
		free(n);
	}
}

/* --- accessors and utility functions ------------------------------------ */

const struct tbl_span *
roff_span(const struct roff *r)
{

	return r->tbl ? tbl_span(r->tbl) : NULL;
}

const struct eqn *
roff_eqn(const struct roff *r)
{

	return r->last_eqn ? &r->last_eqn->eqn : NULL;
}

/*
 * Duplicate an input string, making the appropriate character
 * conversations (as stipulated by `tr') along the way.
 * Returns a heap-allocated string with all the replacements made.
 */
char *
roff_strdup(const struct roff *r, const char *p)
{
	const struct roffkv *cp;
	char		*res;
	const char	*pp;
	size_t		 ssz, sz;
	enum mandoc_esc	 esc;

	if (NULL == r->xmbtab && NULL == r->xtab)
		return mandoc_strdup(p);
	else if ('\0' == *p)
		return mandoc_strdup("");

	/*
	 * Step through each character looking for term matches
	 * (remember that a `tr' can be invoked with an escape, which is
	 * a glyph but the escape is multi-character).
	 * We only do this if the character hash has been initialised
	 * and the string is >0 length.
	 */

	res = NULL;
	ssz = 0;

	while ('\0' != *p) {
		if ('\\' != *p && r->xtab && r->xtab[(int)*p].p) {
			sz = r->xtab[(int)*p].sz;
			res = mandoc_realloc(res, ssz + sz + 1);
			memcpy(res + ssz, r->xtab[(int)*p].p, sz);
			ssz += sz;
			p++;
			continue;
		} else if ('\\' != *p) {
			res = mandoc_realloc(res, ssz + 2);
			res[ssz++] = *p++;
			continue;
		}

		/* Search for term matches. */
		for (cp = r->xmbtab; cp; cp = cp->next)
			if (0 == strncmp(p, cp->key.p, cp->key.sz))
				break;

		if (NULL != cp) {
			/*
			 * A match has been found.
			 * Append the match to the array and move
			 * forward by its keysize.
			 */
			res = mandoc_realloc(res,
			    ssz + cp->val.sz + 1);
			memcpy(res + ssz, cp->val.p, cp->val.sz);
			ssz += cp->val.sz;
			p += (int)cp->key.sz;
			continue;
		}

		/*
		 * Handle escapes carefully: we need to copy
		 * over just the escape itself, or else we might
		 * do replacements within the escape itself.
		 * Make sure to pass along the bogus string.
		 */
		pp = p++;
		esc = mandoc_escape(&p, NULL, NULL);
		if (ESCAPE_ERROR == esc) {
			sz = strlen(pp);
			res = mandoc_realloc(res, ssz + sz + 1);
			memcpy(res + ssz, pp, sz);
			break;
		}
		/*
		 * We bail out on bad escapes.
		 * No need to warn: we already did so when
		 * roff_res() was called.
		 */
		sz = (int)(p - pp);
		res = mandoc_realloc(res, ssz + sz + 1);
		memcpy(res + ssz, pp, sz);
		ssz += sz;
	}

	res[(int)ssz] = '\0';
	return res;
}

int
roff_getformat(const struct roff *r)
{

	return r->format;
}

/*
 * Find out whether a line is a macro line or not.
 * If it is, adjust the current position and return one; if it isn't,
 * return zero and don't change the current position.
 * If the control character has been set with `.cc', then let that grain
 * precedence.
 * This is slighly contrary to groff, where using the non-breaking
 * control character when `cc' has been invoked will cause the
 * non-breaking macro contents to be printed verbatim.
 */
int
roff_getcontrol(const struct roff *r, const char *cp, int *ppos)
{
	int		pos;

	pos = *ppos;

	if (0 != r->control && cp[pos] == r->control)
		pos++;
	else if (0 != r->control)
		return 0;
	else if ('\\' == cp[pos] && '.' == cp[pos + 1])
		pos += 2;
	else if ('.' == cp[pos] || '\'' == cp[pos])
		pos++;
	else
		return 0;

	while (' ' == cp[pos] || '\t' == cp[pos])
		pos++;

	*ppos = pos;
	return 1;
}
