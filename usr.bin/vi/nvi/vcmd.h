/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)vcmd.h	8.23 (Berkeley) 1/8/94
 */

typedef struct _vikeys VIKEYS;

/* Structure passed around to functions implementing vi commands. */
typedef struct _vicmdarg {
#define	vp_startzero	buffer	/* START ZERO OUT. */
	CHAR_T	buffer;		/* Buffer. */
	CHAR_T	character;	/* Character. */
	u_long	count;		/* Count. */
	u_long	count2;		/* Second count (only used by z). */
	int	key;		/* Command key. */
	VIKEYS const *kp;	/* VIKEYS key. */
	VIKEYS const *mkp;	/* VIKEYS motion key. */
	size_t	klen;		/* Keyword length. */

/*
 * Historic vi allowed "dl" when the cursor was on the last column, deleting
 * the last character, and similarly allowed "dw" when the cursor was on the
 * last column of the file.  It didn't allow "dh" when the cursor was on
 * column 1, although these cases are not strictly analogous.  The point is
 * that some movements would succeed if they were associated with a motion
 * command, and fail otherwise.  This is part of the off-by-1 schizophrenia
 * that plagued vi.  Other examples are that "dfb" deleted everything up to
 * and including the next 'b' character, but "d/b" only deleted everything
 * up to the next 'b' character.  While this implementation regularizes the
 * interface to the extent possible, there are many special cases that can't
 * be fixed.  This is implemented by setting special flags per command so that
 * the motion routines know what's really going on.
 *
 * Note, the VC_COMMASK flags are set in the vikeys array, and therefore
 * must have values not used in the set of flags declared in the VIKEYS
 * structure below.
 */
#define	VC_C		0x0001	/* The 'c' command. */
#define	VC_D		0x0002	/* The 'd' command. */
#define	VC_SH		0x0004	/* The '>' command. */
#define	VC_Y		0x0008	/* The 'y' command. */
#define	VC_COMMASK	0x000f	/* Mask for special flags. */

#define	VC_BUFFER	0x0010	/* Buffer set. */
#define	VC_C1SET	0x0020	/* Count 1 set. */
#define	VC_C1RESET	0x0040	/* Reset the C1SET flag for dot commands. */
#define	VC_C2SET	0x0080	/* Count 2 set. */
#define	VC_LMODE	0x0100	/* Motion is line oriented. */
#define	VC_ISDOT	0x0200	/* Command was the dot command. */
#define	VC_REVMOVE	0x0400	/* Movement was before the cursor. */

	u_int flags;

#define	vp_endzero	keyword	/* END ZERO OUT. */
	char *keyword;		/* Keyword. */
	size_t kbuflen;		/* Keyword buffer length. */
} VICMDARG;

/* Vi command structure. */
struct _vikeys {			/* Underlying function. */
	int (*func) __P((SCR *, EXF *, VICMDARG *, MARK *, MARK *, MARK *));

#define	V_DONTUSE1	0x000001	/* VC_C */
#define	V_DONTUSE2	0x000002	/* VC_D */
#define	V_DONTUSE3	0x000004	/* VC_SH */
#define	V_DONTUSE4	0x000008	/* VC_Y */
#define	V_ABS		0x000010	/* Absolute movement, set '' mark. */
#define	V_CHAR		0x000020	/* Character (required, trailing). */
#define	V_CNT		0x000040	/* Count (optional, leading). */
#define	V_DOT		0x000080	/* On success, sets dot command. */
#define	V_KEYNUM	0x000100	/* Cursor referenced number. */
#define	V_KEYW		0x000200	/* Cursor referenced word. */
#define	V_LMODE		0x000400	/* Motion is line oriented. */
#define	V_MOTION	0x000800	/* Motion (required, trailing). */
#define	V_MOVE		0x001000	/* Command defines movement. */
#define	V_OBUF		0x002000	/* Buffer (optional, leading). */
#define	V_RBUF		0x004000	/* Buffer (required, trailing). */
#define	V_RCM		0x008000	/* Use relative cursor movment (RCM). */
#define	V_RCM_SET	0x010000	/* RCM: set to current position. */
#define	V_RCM_SETFNB	0x020000	/* RCM: set to first non-blank (FNB). */
#define	V_RCM_SETLAST	0x040000	/* RCM: set to last character. */
#define	V_RCM_SETLFNB	0x080000	/* RCM: set to FNB if line moved. */
#define	V_RCM_SETNNB	0x100000	/* RCM: set to next non-blank. */
	u_long flags;
	char *usage;		/* Usage line. */
	char *help;		/* Help line. */
};
#define	MAXVIKEY	126	/* List of vi commands. */
extern VIKEYS const vikeys[MAXVIKEY + 1];

/* Definition of a "word". */
#define	inword(ch)	(isalnum(ch) || (ch) == '_')

/* Character stream structure, prototypes. */
typedef struct _vcs {
	recno_t	 cs_lno;			/* Line. */
	size_t	 cs_cno;			/* Column. */
	char	*cs_bp;				/* Buffer. */
	size_t	 cs_len;			/* Length. */
	int	 cs_ch;				/* Character. */
#define	CS_EMP	1				/* Empty line. */
#define	CS_EOF	2				/* End-of-file. */
#define	CS_EOL	3				/* End-of-line. */
#define	CS_SOF	4				/* Start-of-file. */
	int	 cs_flags;			/* Return flags. */
} VCS;

int	cs_bblank __P((SCR *, EXF *, VCS *));
int	cs_fblank __P((SCR *, EXF *, VCS *));
int	cs_fspace __P((SCR *, EXF *, VCS *));
int	cs_init __P((SCR *, EXF *, VCS *));
int	cs_next __P((SCR *, EXF *, VCS *));
int	cs_prev __P((SCR *, EXF *, VCS *));

/* Vi private, per-screen memory. */
typedef struct _vi_private {
	VICMDARG sdot;			/* Saved dot, motion command. */
	VICMDARG sdotmotion;

	CHAR_T	 rlast;			/* Last 'r' command character. */

	char	*rep;			/* Input replay buffer. */
	size_t	 rep_len;		/* Input replay buffer length. */
	size_t	 rep_cnt;		/* Input replay buffer characters. */

	CHAR_T	 inc_lastch;		/* Last increment character. */
	long	 inc_lastval;		/* Last increment value. */

	char	*paragraph;		/* Paragraph search list. */
	size_t	 paragraph_len;		/* Paragraph search list length. */

	u_long	 u_ccnt;		/* Undo command count. */
} VI_PRIVATE;

#define	VIP(sp)	((VI_PRIVATE *)((sp)->vi_private))

/* Vi function prototypes. */
int	txt_auto __P((SCR *, EXF *, recno_t, TEXT *, size_t, TEXT *));
int	v_buildparagraph __P((SCR *));
int	v_end __P((SCR *));
void	v_eof __P((SCR *, EXF *, MARK *));
void	v_eol __P((SCR *, EXF *, MARK *));
int	v_exwrite __P((void *, const char *, int));
int	v_init __P((SCR *, EXF *));
int	v_isempty __P((char *, size_t));
int	v_msgflush __P((SCR *));
int	v_ntext __P((SCR *, EXF *, TEXTH *, MARK *,
	    const char *, const size_t, MARK *, int, recno_t, u_int));
int	v_optchange __P((SCR *, int));
int	v_screen_copy __P((SCR *, SCR *));
int	v_screen_end __P((SCR *));
void	v_sof __P((SCR *, MARK *));
int	vi __P((SCR *, EXF *));

#define	VIPROTO(type, name)						\
	type name __P((SCR *, EXF *,	VICMDARG *, MARK *, MARK *, MARK *))

VIPROTO(int, v_again);
VIPROTO(int, v_at);
VIPROTO(int, v_bottom);
VIPROTO(int, v_cfirst);
VIPROTO(int, v_Change);
VIPROTO(int, v_change);
VIPROTO(int, v_chF);
VIPROTO(int, v_chf);
VIPROTO(int, v_chrepeat);
VIPROTO(int, v_chrrepeat);
VIPROTO(int, v_chT);
VIPROTO(int, v_cht);
VIPROTO(int, v_cr);
VIPROTO(int, v_Delete);
VIPROTO(int, v_delete);
VIPROTO(int, v_dollar);
VIPROTO(int, v_down);
VIPROTO(int, v_ex);
VIPROTO(int, v_exit);
VIPROTO(int, v_exmode);
VIPROTO(int, v_filter);
VIPROTO(int, v_first);
VIPROTO(int, v_gomark);
VIPROTO(int, v_home);
VIPROTO(int, v_hpagedown);
VIPROTO(int, v_hpageup);
VIPROTO(int, v_iA);
VIPROTO(int, v_ia);
VIPROTO(int, v_iI);
VIPROTO(int, v_ii);
VIPROTO(int, v_increment);
VIPROTO(int, v_iO);
VIPROTO(int, v_io);
VIPROTO(int, v_join);
VIPROTO(int, v_left);
VIPROTO(int, v_lgoto);
VIPROTO(int, v_linedown);
VIPROTO(int, v_lineup);
VIPROTO(int, v_mark);
VIPROTO(int, v_match);
VIPROTO(int, v_middle);
VIPROTO(int, v_ncol);
VIPROTO(int, v_pagedown);
VIPROTO(int, v_pageup);
VIPROTO(int, v_paragraphb);
VIPROTO(int, v_paragraphf);
VIPROTO(int, v_Put);
VIPROTO(int, v_put);
VIPROTO(int, v_redraw);
VIPROTO(int, v_Replace);
VIPROTO(int, v_replace);
VIPROTO(int, v_right);
VIPROTO(int, v_screen);
VIPROTO(int, v_searchb);
VIPROTO(int, v_searchf);
VIPROTO(int, v_searchN);
VIPROTO(int, v_searchn);
VIPROTO(int, v_searchw);
VIPROTO(int, v_sectionb);
VIPROTO(int, v_sectionf);
VIPROTO(int, v_sentenceb);
VIPROTO(int, v_sentencef);
VIPROTO(int, v_shiftl);
VIPROTO(int, v_shiftr);
VIPROTO(int, v_status);
VIPROTO(int, v_stop);
VIPROTO(int, v_Subst);
VIPROTO(int, v_subst);
VIPROTO(int, v_switch);
VIPROTO(int, v_tagpop);
VIPROTO(int, v_tagpush);
VIPROTO(int, v_ulcase);
VIPROTO(int, v_Undo);
VIPROTO(int, v_undo);
VIPROTO(int, v_up);
VIPROTO(int, v_wordB);
VIPROTO(int, v_wordb);
VIPROTO(int, v_wordE);
VIPROTO(int, v_worde);
VIPROTO(int, v_wordW);
VIPROTO(int, v_wordw);
VIPROTO(int, v_Xchar);
VIPROTO(int, v_xchar);
VIPROTO(int, v_Yank);
VIPROTO(int, v_yank);
VIPROTO(int, v_z);
VIPROTO(int, v_zero);
