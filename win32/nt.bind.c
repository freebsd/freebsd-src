/*$Header: /p/tcsh/cvsroot/tcsh/win32/nt.bind.c,v 1.6 2006/03/05 08:59:36 amold Exp $*/
/*-
 * Copyright (c) 1980, 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 */
#include "sh.h"
#include "ed.h"
#include "ed.defns.h"


KEYCMD  CcEmacsMap[] = {
	/* keymap table, each index into above tbl; should be 
	   (256+extra NT bindings)*sizeof(KEYCMD) bytes long */

	F_SET_MARK,			/* ^@ */
	F_TOBEG,			/* ^A */
	F_CHARBACK,			/* ^B */
	F_TTY_INT,			/* ^C */
	F_DELNEXT_LIST_EOF,		/* ^D */
	F_TOEND,			/* ^E */
	F_CHARFWD,			/* ^F */
	F_UNASSIGNED,		/* ^G */
	F_DELPREV,			/* ^H */
	F_COMPLETE,			/* ^I */
	F_NEWLINE,			/* ^J */
	F_KILLEND,			/* ^K */
	F_CLEARDISP,		/* ^L */
	F_NEWLINE,			/* ^M */
	F_DOWN_HIST,		/* ^N */
	F_TTY_FLUSHO,		/* ^O */
	F_UP_HIST,			/* ^P */
	F_TTY_STARTO,		/* ^Q */
	F_REDISP,			/* ^R */
	F_TTY_STOPO,		/* ^S */
	F_CHARSWITCH,		/* ^T */
	F_KILLALL,			/* ^U */
	F_QUOTE,			/* ^V */
	F_KILLREGION,		/* ^W */
	F_XKEY,			/* ^X */
	F_YANK_KILL,		/* ^Y */
	F_TTY_TSUSP,		/* ^Z */
	F_METANEXT,			/* ^[ */
	F_TTY_QUIT,			/* ^\ */
	F_TTY_DSUSP,		/* ^] */
	F_UNASSIGNED,		/* ^^ */
	F_UNASSIGNED,		/* ^_ */
	F_INSERT,			/* SPACE */
	F_INSERT,			/* ! */
	F_INSERT,			/* " */
	F_INSERT,			/* # */
	F_INSERT,			/* $ */
	F_INSERT,			/* % */
	F_INSERT,			/* & */
	F_INSERT,			/* ' */
	F_INSERT,			/* ( */
	F_INSERT,			/* ) */
	F_INSERT,			/* * */
	F_INSERT,			/* + */
	F_INSERT,			/* , */
	F_INSERT,			/* - */
	F_INSERT,			/* . */
	F_INSERT,			/* / */
	F_DIGIT,			/* 0 */
	F_DIGIT,			/* 1 */
	F_DIGIT,			/* 2 */
	F_DIGIT,			/* 3 */
	F_DIGIT,			/* 4 */
	F_DIGIT,			/* 5 */
	F_DIGIT,			/* 6 */
	F_DIGIT,			/* 7 */
	F_DIGIT,			/* 8 */
	F_DIGIT,			/* 9 */
	F_INSERT,			/* : */
	F_INSERT,			/* ; */
	F_INSERT,			/* < */
	F_INSERT,			/* = */
	F_INSERT,			/* > */
	F_INSERT,			/* ? */
	F_INSERT,			/* @ */
	F_INSERT,			/* A */
	F_INSERT,			/* B */
	F_INSERT,			/* C */
	F_INSERT,			/* D */
	F_INSERT,			/* E */
	F_INSERT,			/* F */
	F_INSERT,			/* G */
	F_INSERT,			/* H */
	F_INSERT,			/* I */
	F_INSERT,			/* J */
	F_INSERT,			/* K */
	F_INSERT,			/* L */
	F_INSERT,			/* M */
	F_INSERT,			/* N */
	F_INSERT,			/* O */
	F_INSERT,			/* P */
	F_INSERT,			/* Q */
	F_INSERT,			/* R */
	F_INSERT,			/* S */
	F_INSERT,			/* T */
	F_INSERT,			/* U */
	F_INSERT,			/* V */
	F_INSERT,			/* W */
	F_INSERT,			/* X */
	F_INSERT,			/* Y */
	F_INSERT,			/* Z */
	F_INSERT,			/* [ */
	F_INSERT,			/* \ */
	F_INSERT,			/* ] */
	F_INSERT,			/* ^ */
	F_INSERT,			/* _ */
	F_INSERT,			/* ` */
	F_INSERT,			/* a */
	F_INSERT,			/* b */
	F_INSERT,			/* c */
	F_INSERT,			/* d */
	F_INSERT,			/* e */
	F_INSERT,			/* f */
	F_INSERT,			/* g */
	F_INSERT,			/* h */
	F_INSERT,			/* i */
	F_INSERT,			/* j */
	F_INSERT,			/* k */
	F_INSERT,			/* l */
	F_INSERT,			/* m */
	F_INSERT,			/* n */
	F_INSERT,			/* o */
	F_INSERT,			/* p */
	F_INSERT,			/* q */
	F_INSERT,			/* r */
	F_INSERT,			/* s */
	F_INSERT,			/* t */
	F_INSERT,			/* u */
	F_INSERT,			/* v */
	F_INSERT,			/* w */
	F_INSERT,			/* x */
	F_INSERT,			/* y */
	F_INSERT,			/* z */
	F_INSERT,			/* { */
	F_INSERT,			/* | */
	F_INSERT,			/* } */
	F_INSERT,			/* ~ */
	F_DELPREV,			/* ^? */
	F_UNASSIGNED,		/* M-^@ */
	F_UNASSIGNED,		/* M-^A */
	F_UNASSIGNED,		/* M-^B */
	F_UNASSIGNED,		/* M-^C */
	F_LIST_CHOICES,		/* M-^D */
	F_UNASSIGNED,		/* M-^E */
	F_UNASSIGNED,		/* M-^F */
	F_UNASSIGNED,		/* M-^G */
	F_DELWORDPREV,		/* M-^H */
	F_COMPLETE,			/* M-^I */
	F_UNASSIGNED,		/* M-^J */
	F_UNASSIGNED,		/* M-^K */
	F_CLEARDISP,		/* M-^L */
	F_UNASSIGNED,		/* M-^M */
	F_UNASSIGNED,		/* M-^N */
	F_UNASSIGNED,		/* M-^O */
	F_UNASSIGNED,		/* M-^P */
	F_UNASSIGNED,		/* M-^Q */
	F_UNASSIGNED,		/* M-^R */
	F_UNASSIGNED,		/* M-^S */
	F_UNASSIGNED,		/* M-^T */
	F_UNASSIGNED,		/* M-^U */
	F_UNASSIGNED,		/* M-^V */
	F_UNASSIGNED,		/* M-^W */
	F_UNASSIGNED,		/* M-^X */
	F_UNASSIGNED,		/* M-^Y */
	F_RUN_FG_EDITOR,		/* M-^Z */
	F_COMPLETE,			/* M-^[ */
	F_UNASSIGNED,		/* M-^\ */
	F_UNASSIGNED,		/* M-^] */
	F_UNASSIGNED,		/* M-^^ */
	F_COPYPREV,			/* M-^_ */
	F_EXPAND_HISTORY,		/* M-SPACE */
	F_EXPAND_HISTORY,		/* M-! */
	F_UNASSIGNED,		/* M-" */
	F_UNASSIGNED,		/* M-# */
	F_CORRECT_L,		/* M-$ */
	F_UNASSIGNED,		/* M-% */
	F_UNASSIGNED,		/* M-& */
	F_UNASSIGNED,		/* M-' */
	F_UNASSIGNED,		/* M-( */
	F_UNASSIGNED,		/* M-) */
	F_UNASSIGNED,		/* M-* */
	F_UNASSIGNED,		/* M-+ */
	F_UNASSIGNED,		/* M-, */
	F_UNASSIGNED,		/* M-- */
	F_UNASSIGNED,		/* M-. */
	F_DABBREV_EXPAND,		/* M-/ */
	F_ARGDIGIT,			/* M-0 */
	F_ARGDIGIT,			/* M-1 */
	F_ARGDIGIT,			/* M-2 */
	F_ARGDIGIT,			/* M-3 */
	F_ARGDIGIT,			/* M-4 */
	F_ARGDIGIT,			/* M-5 */
	F_ARGDIGIT,			/* M-6 */
	F_ARGDIGIT,			/* M-7 */
	F_ARGDIGIT,			/* M-8 */
	F_ARGDIGIT,			/* M-9 */
	F_UNASSIGNED,		/* M-: */
	F_UNASSIGNED,		/* M-; */
	F_UNASSIGNED,		/* M-< */
	F_UNASSIGNED,		/* M-= */
	F_UNASSIGNED,		/* M-> */
	F_WHICH,			/* M-? */
	F_UNASSIGNED,		/* M-@ */
	F_UNASSIGNED,		/* M-A */
	F_WORDBACK,			/* M-B */
	F_CASECAPITAL,		/* M-C */
	F_DELWORDNEXT,		/* M-D */
	F_UNASSIGNED,		/* M-E */
	F_WORDFWD,			/* M-F */
	F_UNASSIGNED,		/* M-G */
	F_HELPME,			/* M-H */
	F_UNASSIGNED,		/* M-I */
	F_UNASSIGNED,		/* M-J */
	F_UNASSIGNED,		/* M-K */
	F_CASELOWER,		/* M-L */
	F_UNASSIGNED,		/* M-M */
	F_DOWN_SEARCH_HIST,		/* M-N */
	F_XKEY,			/* M-O *//* extended key esc PWP Mar 88 */
	F_UP_SEARCH_HIST,		/* M-P */
	F_UNASSIGNED,		/* M-Q */
	F_TOGGLE_HIST,		/* M-R */
	F_CORRECT,			/* M-S */
	F_UNASSIGNED,		/* M-T */
	F_CASEUPPER,		/* M-U */
	F_UNASSIGNED,		/* M-V */
	F_COPYREGION,		/* M-W */
	F_UNASSIGNED,		/* M-X */
	F_UNASSIGNED,		/* M-Y */
	F_UNASSIGNED,		/* M-Z */
	F_XKEY,			/* M-[ *//* extended key esc -mf Oct 87 */
	F_UNASSIGNED,		/* M-\ */
	F_UNASSIGNED,		/* M-] */
	F_UNASSIGNED,		/* M-^ */
	F_LAST_ITEM,		/* M-_ */
	F_UNASSIGNED,		/* M-` */
	F_UNASSIGNED,		/* M-a */
	F_WORDBACK,			/* M-b */
	F_CASECAPITAL,		/* M-c */
	F_DELWORDNEXT,		/* M-d */
	F_UNASSIGNED,		/* M-e */
	F_WORDFWD,			/* M-f */
	F_UNASSIGNED,		/* M-g */
	F_HELPME,			/* M-h */
	F_UNASSIGNED,		/* M-i */
	F_UNASSIGNED,		/* M-j */
	F_UNASSIGNED,		/* M-k */
	F_CASELOWER,		/* M-l */
	F_UNASSIGNED,		/* M-m */
	F_DOWN_SEARCH_HIST,		/* M-n */
	F_UNASSIGNED,		/* M-o */
	F_UP_SEARCH_HIST,		/* M-p */
	F_UNASSIGNED,		/* M-q */
	F_TOGGLE_HIST,		/* M-r */
	F_CORRECT,			/* M-s */
	F_UNASSIGNED,		/* M-t */
	F_CASEUPPER,		/* M-u */
	F_UNASSIGNED,		/* M-v */
	F_COPYREGION,		/* M-w */
	F_UNASSIGNED,		/* M-x */
	F_UNASSIGNED,		/* M-y */
	F_UNASSIGNED,		/* M-z */
	F_UNASSIGNED,		/* M-{ */
	F_UNASSIGNED,		/* M-| */
	F_UNASSIGNED,		/* M-} */
	F_UNASSIGNED,		/* M-~ */
	F_DELWORDPREV,		/* M-^? */
	/* Extra keys begin here */
	F_UNASSIGNED,		/* f-1 */
	F_UNASSIGNED,		/* f-2 */
	F_UNASSIGNED,		/* f-3 */
	F_UNASSIGNED,		/* f-4 */
	F_UNASSIGNED,		/* f-5 */
	F_UNASSIGNED,		/* f-6 */
	F_UNASSIGNED,		/* f-7 */
	F_UNASSIGNED,		/* f-8 */
	F_UNASSIGNED,		/* f-9 */
	F_UNASSIGNED,		/* f-10 */
	F_UNASSIGNED,		/* f-11 */
	F_UNASSIGNED,		/* f-12 */
	F_UNASSIGNED,		/* f-13 */
	F_UNASSIGNED,		/* f-14 */
	F_UNASSIGNED,		/* f-15 */
	F_UNASSIGNED,		/* f-16 */
	F_UNASSIGNED,		/* f-17 */
	F_UNASSIGNED,		/* f-18 */
	F_UNASSIGNED,		/* f-19 */
	F_UNASSIGNED,		/* f-20 */
	F_UNASSIGNED,		/* f-21 */
	F_UNASSIGNED,		/* f-22 */
	F_UNASSIGNED,		/* f-23 */
	F_UNASSIGNED,		/* f-24 */
	F_UNASSIGNED,		/* PgUp */
	F_UNASSIGNED,		/* PgDn */
	F_UNASSIGNED,		/* end */
	F_UNASSIGNED,		/* home */
	F_UNASSIGNED,		/* LEFT */
	F_UNASSIGNED,		/* UP */
	F_UNASSIGNED,		/* RIGHT */
	F_UNASSIGNED,		/* DOWN */
	F_UNASSIGNED,		/* INS */
	F_UNASSIGNED,		/* DEL */
	/* ctrl key mappings */
	F_UNASSIGNED,		/* f-1 */
	F_UNASSIGNED,		/* f-2 */
	F_UNASSIGNED,		/* f-3 */
	F_UNASSIGNED,		/* f-4 */
	F_UNASSIGNED,		/* f-5 */
	F_UNASSIGNED,		/* f-6 */
	F_UNASSIGNED,		/* f-7 */
	F_UNASSIGNED,		/* f-8 */
	F_UNASSIGNED,		/* f-9 */
	F_UNASSIGNED,		/* f-10 */
	F_UNASSIGNED,		/* f-11 */
	F_UNASSIGNED,		/* f-12 */
	F_UNASSIGNED,		/* f-13 */
	F_UNASSIGNED,		/* f-14 */
	F_UNASSIGNED,		/* f-15 */
	F_UNASSIGNED,		/* f-16 */
	F_UNASSIGNED,		/* f-17 */
	F_UNASSIGNED,		/* f-18 */
	F_UNASSIGNED,		/* f-19 */
	F_UNASSIGNED,		/* f-20 */
	F_UNASSIGNED,		/* f-21 */
	F_UNASSIGNED,		/* f-22 */
	F_UNASSIGNED,		/* f-23 */
	F_UNASSIGNED,		/* f-24 */
	F_UNASSIGNED,		/* PgUp */
	F_UNASSIGNED,		/* PgDn */
	F_UNASSIGNED,		/* end */
	F_UNASSIGNED,		/* home */
	F_UNASSIGNED,		/* LEFT */
	F_UNASSIGNED,		/* UP */
	F_UNASSIGNED,		/* RIGHT */
	F_UNASSIGNED,		/* DOWN */
	F_UNASSIGNED,		/* INS */
	F_UNASSIGNED,		/* DEL */

	/* alt key mappings */
	F_UNASSIGNED,		/* f-1 */
	F_UNASSIGNED,		/* f-2 */
	F_UNASSIGNED,		/* f-3 */
	F_UNASSIGNED,		/* f-4 */
	F_UNASSIGNED,		/* f-5 */
	F_UNASSIGNED,		/* f-6 */
	F_UNASSIGNED,		/* f-7 */
	F_UNASSIGNED,		/* f-8 */
	F_UNASSIGNED,		/* f-9 */
	F_UNASSIGNED,		/* f-10 */
	F_UNASSIGNED,		/* f-11 */
	F_UNASSIGNED,		/* f-12 */
	F_UNASSIGNED,		/* f-13 */
	F_UNASSIGNED,		/* f-14 */
	F_UNASSIGNED,		/* f-15 */
	F_UNASSIGNED,		/* f-16 */
	F_UNASSIGNED,		/* f-17 */
	F_UNASSIGNED,		/* f-18 */
	F_UNASSIGNED,		/* f-19 */
	F_UNASSIGNED,		/* f-20 */
	F_UNASSIGNED,		/* f-21 */
	F_UNASSIGNED,		/* f-22 */
	F_UNASSIGNED,		/* f-23 */
	F_UNASSIGNED,		/* f-24 */
	F_UNASSIGNED,		/* PgUp */
	F_UNASSIGNED,		/* PgDn */
	F_UNASSIGNED,		/* end */
	F_UNASSIGNED,		/* home */
	F_UNASSIGNED,		/* LEFT */
	F_UNASSIGNED,		/* UP */
	F_UNASSIGNED,		/* RIGHT */
	F_UNASSIGNED,		/* DOWN */
	F_UNASSIGNED,		/* INS */
	F_UNASSIGNED,		/* DEL */
	/* shift key mappings */
	F_UNASSIGNED,		/* f-1 */
	F_UNASSIGNED,		/* f-2 */
	F_UNASSIGNED,		/* f-3 */
	F_UNASSIGNED,		/* f-4 */
	F_UNASSIGNED,		/* f-5 */
	F_UNASSIGNED,		/* f-6 */
	F_UNASSIGNED,		/* f-7 */
	F_UNASSIGNED,		/* f-8 */
	F_UNASSIGNED,		/* f-9 */
	F_UNASSIGNED,		/* f-10 */
	F_UNASSIGNED,		/* f-11 */
	F_UNASSIGNED,		/* f-12 */
	F_UNASSIGNED,		/* f-13 */
	F_UNASSIGNED,		/* f-14 */
	F_UNASSIGNED,		/* f-15 */
	F_UNASSIGNED,		/* f-16 */
	F_UNASSIGNED,		/* f-17 */
	F_UNASSIGNED,		/* f-18 */
	F_UNASSIGNED,		/* f-19 */
	F_UNASSIGNED,		/* f-20 */
	F_UNASSIGNED,		/* f-21 */
	F_UNASSIGNED,		/* f-22 */
	F_UNASSIGNED,		/* f-23 */
	F_UNASSIGNED,		/* f-24 */
	F_UNASSIGNED,		/* PgUp */
	F_UNASSIGNED,		/* PgDn */
	F_UNASSIGNED,		/* end */
	F_UNASSIGNED,		/* home */
	F_UNASSIGNED,		/* LEFT */
	F_UNASSIGNED,		/* UP */
	F_UNASSIGNED,		/* RIGHT */
	F_UNASSIGNED,		/* DOWN */
	F_UNASSIGNED,		/* INS */
	F_UNASSIGNED		/* DEL */
};

/*
 * keymap table for vi.  Each index into above tbl; should be
 * 256 entries long.  Vi mode uses a sticky-extend to do command mode:
 * insert mode characters are in the normal keymap, and command mode
 * in the extended keymap.
 */
KEYCMD  CcViMap[] = {
#ifdef KSHVI
	F_UNASSIGNED,		/* ^@ */
	F_INSERT,			/* ^A */
	F_INSERT,			/* ^B */
	F_INSERT,			/* ^C */
	F_INSERT,			/* ^D */
	F_INSERT,			/* ^E */
	F_INSERT,			/* ^F */
	F_INSERT,			/* ^G */
	V_DELPREV,			/* ^H */   /* BackSpace key */
	F_COMPLETE,			/* ^I */   /* Tab Key  */
	F_NEWLINE,			/* ^J */
	F_INSERT,			/* ^K */
	F_INSERT,			/* ^L */
	F_NEWLINE,			/* ^M */
	F_INSERT,			/* ^N */
	F_INSERT,			/* ^O */
	F_INSERT,			/* ^P */
	F_TTY_STARTO,		/* ^Q */
	F_INSERT,			/* ^R */
	F_INSERT,			/* ^S */
	F_INSERT,			/* ^T */
	F_INSERT,			/* ^U */
	F_QUOTE,			/* ^V */
	F_DELWORDPREV,		/* ^W */  /* Only until start edit pos */
	F_INSERT,			/* ^X */
	F_INSERT,			/* ^Y */
	F_INSERT,			/* ^Z */
	V_CMD_MODE,			/* ^[ */  /* [ Esc ] key */
	F_TTY_QUIT,			/* ^\ */
	F_INSERT,			/* ^] */
	F_INSERT,			/* ^^ */
	F_INSERT,			/* ^_ */
#else /* !KSHVI */
	F_UNASSIGNED,		/* ^@ */   /* NOTE: These mapping do NOT */
	F_TOBEG,			/* ^A */   /* Correspond well to the KSH */
	F_CHARBACK,			/* ^B */   /* VI editting assignments    */
	F_TTY_INT,			/* ^C */   /* On the other hand they are */
	F_LIST_EOF,			/* ^D */   /* convenient any many people */
	F_TOEND,			/* ^E */   /* have gotten used to them   */
	F_CHARFWD,			/* ^F */
	F_LIST_GLOB,		/* ^G */
	F_DELPREV,			/* ^H */   /* BackSpace key */
	F_COMPLETE,			/* ^I */   /* Tab Key */
	F_NEWLINE,			/* ^J */
	F_KILLEND,			/* ^K */
	F_CLEARDISP,		/* ^L */
	F_NEWLINE,			/* ^M */
	F_DOWN_HIST,		/* ^N */
	F_TTY_FLUSHO,		/* ^O */
	F_UP_HIST,			/* ^P */
	F_TTY_STARTO,		/* ^Q */
	F_REDISP,			/* ^R */
	F_TTY_STOPO,		/* ^S */
	F_CHARSWITCH,		/* ^T */
	F_KILLBEG,			/* ^U */
	F_QUOTE,			/* ^V */
	F_DELWORDPREV,		/* ^W */
	F_EXPAND,			/* ^X */
	F_TTY_DSUSP,		/* ^Y */
	F_TTY_TSUSP,		/* ^Z */
	V_CMD_MODE,			/* ^[ */
	F_TTY_QUIT,			/* ^\ */
	F_UNASSIGNED,		/* ^] */
	F_UNASSIGNED,		/* ^^ */
	F_UNASSIGNED,		/* ^_ */
#endif  /* KSHVI */
	F_INSERT,			/* SPACE */
	F_INSERT,			/* ! */
	F_INSERT,			/* " */
	F_INSERT,			/* # */
	F_INSERT,			/* $ */
	F_INSERT,			/* % */
	F_INSERT,			/* & */
	F_INSERT,			/* ' */
	F_INSERT,			/* ( */
	F_INSERT,			/* ) */
	F_INSERT,			/* * */
	F_INSERT,			/* + */
	F_INSERT,			/* , */
	F_INSERT,			/* - */
	F_INSERT,			/* . */
	F_INSERT,			/* / */
	F_INSERT,			/* 0 */
	F_INSERT,			/* 1 */
	F_INSERT,			/* 2 */
	F_INSERT,			/* 3 */
	F_INSERT,			/* 4 */
	F_INSERT,			/* 5 */
	F_INSERT,			/* 6 */
	F_INSERT,			/* 7 */
	F_INSERT,			/* 8 */
	F_INSERT,			/* 9 */
	F_INSERT,			/* : */
	F_INSERT,			/* ; */
	F_INSERT,			/* < */
	F_INSERT,			/* = */
	F_INSERT,			/* > */
	F_INSERT,			/* ? */
	F_INSERT,			/* @ */
	F_INSERT,			/* A */
	F_INSERT,			/* B */
	F_INSERT,			/* C */
	F_INSERT,			/* D */
	F_INSERT,			/* E */
	F_INSERT,			/* F */
	F_INSERT,			/* G */
	F_INSERT,			/* H */
	F_INSERT,			/* I */
	F_INSERT,			/* J */
	F_INSERT,			/* K */
	F_INSERT,			/* L */
	F_INSERT,			/* M */
	F_INSERT,			/* N */
	F_INSERT,			/* O */
	F_INSERT,			/* P */
	F_INSERT,			/* Q */
	F_INSERT,			/* R */
	F_INSERT,			/* S */
	F_INSERT,			/* T */
	F_INSERT,			/* U */
	F_INSERT,			/* V */
	F_INSERT,			/* W */
	F_INSERT,			/* X */
	F_INSERT,			/* Y */
	F_INSERT,			/* Z */
	F_INSERT,			/* [ */
	F_INSERT,			/* \ */
	F_INSERT,			/* ] */
	F_INSERT,			/* ^ */
	F_INSERT,			/* _ */
	F_INSERT,			/* ` */
	F_INSERT,			/* a */
	F_INSERT,			/* b */
	F_INSERT,			/* c */
	F_INSERT,			/* d */
	F_INSERT,			/* e */
	F_INSERT,			/* f */
	F_INSERT,			/* g */
	F_INSERT,			/* h */
	F_INSERT,			/* i */
	F_INSERT,			/* j */
	F_INSERT,			/* k */
	F_INSERT,			/* l */
	F_INSERT,			/* m */
	F_INSERT,			/* n */
	F_INSERT,			/* o */
	F_INSERT,			/* p */
	F_INSERT,			/* q */
	F_INSERT,			/* r */
	F_INSERT,			/* s */
	F_INSERT,			/* t */
	F_INSERT,			/* u */
	F_INSERT,			/* v */
	F_INSERT,			/* w */
	F_INSERT,			/* x */
	F_INSERT,			/* y */
	F_INSERT,			/* z */
	F_INSERT,			/* { */
	F_INSERT,			/* | */
	F_INSERT,			/* } */
	F_INSERT,			/* ~ */
	F_DELPREV,			/* ^? */
	F_UNASSIGNED,		/* M-^@ */
	F_UNASSIGNED,		/* M-^A */
	F_UNASSIGNED,		/* M-^B */
	F_UNASSIGNED,		/* M-^C */
	F_UNASSIGNED,		/* M-^D */
	F_UNASSIGNED,		/* M-^E */
	F_UNASSIGNED,		/* M-^F */
	F_UNASSIGNED,		/* M-^G */
	F_UNASSIGNED,		/* M-^H */
	F_UNASSIGNED,		/* M-^I */
	F_UNASSIGNED,		/* M-^J */
	F_UNASSIGNED,		/* M-^K */
	F_UNASSIGNED,		/* M-^L */
	F_UNASSIGNED,		/* M-^M */
	F_UNASSIGNED,		/* M-^N */
	F_UNASSIGNED,		/* M-^O */
	F_UNASSIGNED,		/* M-^P */
	F_UNASSIGNED,		/* M-^Q */
	F_UNASSIGNED,		/* M-^R */
	F_UNASSIGNED,		/* M-^S */
	F_UNASSIGNED,		/* M-^T */
	F_UNASSIGNED,		/* M-^U */
	F_UNASSIGNED,		/* M-^V */
	F_UNASSIGNED,		/* M-^W */
	F_UNASSIGNED,		/* M-^X */
	F_UNASSIGNED,		/* M-^Y */
	F_UNASSIGNED,		/* M-^Z */
	F_UNASSIGNED,		/* M-^[ */
	F_UNASSIGNED,		/* M-^\ */
	F_UNASSIGNED,		/* M-^] */
	F_UNASSIGNED,		/* M-^^ */
	F_UNASSIGNED,		/* M-^_ */
	F_UNASSIGNED,		/* M-SPACE */
	F_UNASSIGNED,		/* M-! */
	F_UNASSIGNED,		/* M-" */
	F_UNASSIGNED,		/* M-# */
	F_UNASSIGNED,		/* M-$ */
	F_UNASSIGNED,		/* M-% */
	F_UNASSIGNED,		/* M-& */
	F_UNASSIGNED,		/* M-' */
	F_UNASSIGNED,		/* M-( */
	F_UNASSIGNED,		/* M-) */
	F_UNASSIGNED,		/* M-* */
	F_UNASSIGNED,		/* M-+ */
	F_UNASSIGNED,		/* M-, */
	F_UNASSIGNED,		/* M-- */
	F_UNASSIGNED,		/* M-. */
	F_UNASSIGNED,		/* M-/ */
	F_UNASSIGNED,		/* M-0 */
	F_UNASSIGNED,		/* M-1 */
	F_UNASSIGNED,		/* M-2 */
	F_UNASSIGNED,		/* M-3 */
	F_UNASSIGNED,		/* M-4 */
	F_UNASSIGNED,		/* M-5 */
	F_UNASSIGNED,		/* M-6 */
	F_UNASSIGNED,		/* M-7 */
	F_UNASSIGNED,		/* M-8 */
	F_UNASSIGNED,		/* M-9 */
	F_UNASSIGNED,		/* M-: */
	F_UNASSIGNED,		/* M-; */
	F_UNASSIGNED,		/* M-< */
	F_UNASSIGNED,		/* M-= */
	F_UNASSIGNED,		/* M-> */
	F_UNASSIGNED,		/* M-? */
	F_UNASSIGNED,		/* M-@ */
	F_UNASSIGNED,		/* M-A */
	F_UNASSIGNED,		/* M-B */
	F_UNASSIGNED,		/* M-C */
	F_UNASSIGNED,		/* M-D */
	F_UNASSIGNED,		/* M-E */
	F_UNASSIGNED,		/* M-F */
	F_UNASSIGNED,		/* M-G */
	F_UNASSIGNED,		/* M-H */
	F_UNASSIGNED,		/* M-I */
	F_UNASSIGNED,		/* M-J */
	F_UNASSIGNED,		/* M-K */
	F_UNASSIGNED,		/* M-L */
	F_UNASSIGNED,		/* M-M */
	F_UNASSIGNED,		/* M-N */
	F_UNASSIGNED,		/* M-O */
	F_UNASSIGNED,		/* M-P */
	F_UNASSIGNED,		/* M-Q */
	F_UNASSIGNED,		/* M-R */
	F_UNASSIGNED,		/* M-S */
	F_UNASSIGNED,		/* M-T */
	F_UNASSIGNED,		/* M-U */
	F_UNASSIGNED,		/* M-V */
	F_UNASSIGNED,		/* M-W */
	F_UNASSIGNED,		/* M-X */
	F_UNASSIGNED,		/* M-Y */
	F_UNASSIGNED,		/* M-Z */
	F_UNASSIGNED,		/* M-[ */
	F_UNASSIGNED,		/* M-\ */
	F_UNASSIGNED,		/* M-] */
	F_UNASSIGNED,		/* M-^ */
	F_UNASSIGNED,		/* M-_ */
	F_UNASSIGNED,		/* M-` */
	F_UNASSIGNED,		/* M-a */
	F_UNASSIGNED,		/* M-b */
	F_UNASSIGNED,		/* M-c */
	F_UNASSIGNED,		/* M-d */
	F_UNASSIGNED,		/* M-e */
	F_UNASSIGNED,		/* M-f */
	F_UNASSIGNED,		/* M-g */
	F_UNASSIGNED,		/* M-h */
	F_UNASSIGNED,		/* M-i */
	F_UNASSIGNED,		/* M-j */
	F_UNASSIGNED,		/* M-k */
	F_UNASSIGNED,		/* M-l */
	F_UNASSIGNED,		/* M-m */
	F_UNASSIGNED,		/* M-n */
	F_UNASSIGNED,		/* M-o */
	F_UNASSIGNED,		/* M-p */
	F_UNASSIGNED,		/* M-q */
	F_UNASSIGNED,		/* M-r */
	F_UNASSIGNED,		/* M-s */
	F_UNASSIGNED,		/* M-t */
	F_UNASSIGNED,		/* M-u */
	F_UNASSIGNED,		/* M-v */
	F_UNASSIGNED,		/* M-w */
	F_UNASSIGNED,		/* M-x */
	F_UNASSIGNED,		/* M-y */
	F_UNASSIGNED,		/* M-z */
	F_UNASSIGNED,		/* M-{ */
	F_UNASSIGNED,		/* M-| */
	F_UNASSIGNED,		/* M-} */
	F_UNASSIGNED,		/* M-~ */
	F_UNASSIGNED,		/* M-^? */
	/* Extra keys begin here */
	F_UNASSIGNED,		/* f-1 */
	F_UNASSIGNED,		/* f-2 */
	F_UNASSIGNED,		/* f-3 */
	F_UNASSIGNED,		/* f-4 */
	F_UNASSIGNED,		/* f-5 */
	F_UNASSIGNED,		/* f-6 */
	F_UNASSIGNED,		/* f-7 */
	F_UNASSIGNED,		/* f-8 */
	F_UNASSIGNED,		/* f-9 */
	F_UNASSIGNED,		/* f-10 */
	F_UNASSIGNED,		/* f-11 */
	F_UNASSIGNED,		/* f-12 */
	F_UNASSIGNED,		/* f-13 */
	F_UNASSIGNED,		/* f-14 */
	F_UNASSIGNED,		/* f-15 */
	F_UNASSIGNED,		/* f-16 */
	F_UNASSIGNED,		/* f-17 */
	F_UNASSIGNED,		/* f-18 */
	F_UNASSIGNED,		/* f-19 */
	F_UNASSIGNED,		/* f-20 */
	F_UNASSIGNED,		/* f-21 */
	F_UNASSIGNED,		/* f-22 */
	F_UNASSIGNED,		/* f-23 */
	F_UNASSIGNED,		/* f-24 */
	F_UNASSIGNED,		/* PgUp */
	F_UNASSIGNED,		/* PgDn */
	F_UNASSIGNED,		/* end */
	F_UNASSIGNED,		/* home */
	F_UNASSIGNED,		/* LEFT */
	F_UNASSIGNED,		/* UP */
	F_UNASSIGNED,		/* RIGHT */
	F_UNASSIGNED,		/* DOWN */
	F_UNASSIGNED,		/* INS */
	F_UNASSIGNED,		/* DEL */
	/* ctrl key mappings */
	F_UNASSIGNED,		/* f-1 */
	F_UNASSIGNED,		/* f-2 */
	F_UNASSIGNED,		/* f-3 */
	F_UNASSIGNED,		/* f-4 */
	F_UNASSIGNED,		/* f-5 */
	F_UNASSIGNED,		/* f-6 */
	F_UNASSIGNED,		/* f-7 */
	F_UNASSIGNED,		/* f-8 */
	F_UNASSIGNED,		/* f-9 */
	F_UNASSIGNED,		/* f-10 */
	F_UNASSIGNED,		/* f-11 */
	F_UNASSIGNED,		/* f-12 */
	F_UNASSIGNED,		/* f-13 */
	F_UNASSIGNED,		/* f-14 */
	F_UNASSIGNED,		/* f-15 */
	F_UNASSIGNED,		/* f-16 */
	F_UNASSIGNED,		/* f-17 */
	F_UNASSIGNED,		/* f-18 */
	F_UNASSIGNED,		/* f-19 */
	F_UNASSIGNED,		/* f-20 */
	F_UNASSIGNED,		/* f-21 */
	F_UNASSIGNED,		/* f-22 */
	F_UNASSIGNED,		/* f-23 */
	F_UNASSIGNED,		/* f-24 */
	F_UNASSIGNED,		/* PgUp */
	F_UNASSIGNED,		/* PgDn */
	F_UNASSIGNED,		/* end */
	F_UNASSIGNED,		/* home */
	F_UNASSIGNED,		/* LEFT */
	F_UNASSIGNED,		/* UP */
	F_UNASSIGNED,		/* RIGHT */
	F_UNASSIGNED,		/* DOWN */
	F_UNASSIGNED,		/* INS */
	F_UNASSIGNED,		/* DEL */

	/* alt key mappings */
	F_UNASSIGNED,		/* f-1 */
	F_UNASSIGNED,		/* f-2 */
	F_UNASSIGNED,		/* f-3 */
	F_UNASSIGNED,		/* f-4 */
	F_UNASSIGNED,		/* f-5 */
	F_UNASSIGNED,		/* f-6 */
	F_UNASSIGNED,		/* f-7 */
	F_UNASSIGNED,		/* f-8 */
	F_UNASSIGNED,		/* f-9 */
	F_UNASSIGNED,		/* f-10 */
	F_UNASSIGNED,		/* f-11 */
	F_UNASSIGNED,		/* f-12 */
	F_UNASSIGNED,		/* f-13 */
	F_UNASSIGNED,		/* f-14 */
	F_UNASSIGNED,		/* f-15 */
	F_UNASSIGNED,		/* f-16 */
	F_UNASSIGNED,		/* f-17 */
	F_UNASSIGNED,		/* f-18 */
	F_UNASSIGNED,		/* f-19 */
	F_UNASSIGNED,		/* f-20 */
	F_UNASSIGNED,		/* f-21 */
	F_UNASSIGNED,		/* f-22 */
	F_UNASSIGNED,		/* f-23 */
	F_UNASSIGNED,		/* f-24 */
	F_UNASSIGNED,		/* PgUp */
	F_UNASSIGNED,		/* PgDn */
	F_UNASSIGNED,		/* end */
	F_UNASSIGNED,		/* home */
	F_UNASSIGNED,		/* LEFT */
	F_UNASSIGNED,		/* UP */
	F_UNASSIGNED,		/* RIGHT */
	F_UNASSIGNED,		/* DOWN */
	F_UNASSIGNED,		/* INS */
	F_UNASSIGNED,		/* DEL */
	/* shift key mappings */
	F_UNASSIGNED,		/* f-1 */
	F_UNASSIGNED,		/* f-2 */
	F_UNASSIGNED,		/* f-3 */
	F_UNASSIGNED,		/* f-4 */
	F_UNASSIGNED,		/* f-5 */
	F_UNASSIGNED,		/* f-6 */
	F_UNASSIGNED,		/* f-7 */
	F_UNASSIGNED,		/* f-8 */
	F_UNASSIGNED,		/* f-9 */
	F_UNASSIGNED,		/* f-10 */
	F_UNASSIGNED,		/* f-11 */
	F_UNASSIGNED,		/* f-12 */
	F_UNASSIGNED,		/* f-13 */
	F_UNASSIGNED,		/* f-14 */
	F_UNASSIGNED,		/* f-15 */
	F_UNASSIGNED,		/* f-16 */
	F_UNASSIGNED,		/* f-17 */
	F_UNASSIGNED,		/* f-18 */
	F_UNASSIGNED,		/* f-19 */
	F_UNASSIGNED,		/* f-20 */
	F_UNASSIGNED,		/* f-21 */
	F_UNASSIGNED,		/* f-22 */
	F_UNASSIGNED,		/* f-23 */
	F_UNASSIGNED,		/* f-24 */
	F_UNASSIGNED,		/* PgUp */
	F_UNASSIGNED,		/* PgDn */
	F_UNASSIGNED,		/* end */
	F_UNASSIGNED,		/* home */
	F_UNASSIGNED,		/* LEFT */
	F_UNASSIGNED,		/* UP */
	F_UNASSIGNED,		/* RIGHT */
	F_UNASSIGNED,		/* DOWN */
	F_UNASSIGNED,		/* INS */
	F_UNASSIGNED		/* DEL */
};

KEYCMD  CcViCmdMap[] = {
	F_UNASSIGNED,		/* ^@ */
	F_TOBEG,			/* ^A */
	F_UNASSIGNED,		/* ^B */
	F_TTY_INT,			/* ^C */
	F_LIST_CHOICES,		/* ^D */
	F_TOEND,			/* ^E */
	F_UNASSIGNED,		/* ^F */
	F_LIST_GLOB,		/* ^G */
	F_CHARBACK,			/* ^H */
	V_CM_COMPLETE,		/* ^I */
	F_NEWLINE,			/* ^J */
	F_KILLEND,			/* ^K */
	F_CLEARDISP,		/* ^L */
	F_NEWLINE,			/* ^M */
	F_DOWN_HIST,		/* ^N */
	F_TTY_FLUSHO,		/* ^O */
	F_UP_HIST,			/* ^P */
	F_TTY_STARTO,		/* ^Q */
	F_REDISP,			/* ^R */
	F_TTY_STOPO,		/* ^S */
	F_UNASSIGNED,		/* ^T */
	F_KILLBEG,			/* ^U */
	F_UNASSIGNED,		/* ^V */
	F_DELWORDPREV,		/* ^W */
	F_EXPAND,			/* ^X */
	F_UNASSIGNED,		/* ^Y */
	F_UNASSIGNED,		/* ^Z */
	F_METANEXT,			/* ^[ */
	F_TTY_QUIT,			/* ^\ */
	F_UNASSIGNED,		/* ^] */
	F_UNASSIGNED,		/* ^^ */
	F_UNASSIGNED,		/* ^_ */
	F_CHARFWD,			/* SPACE */
	F_EXPAND_HISTORY,		/* ! */
	F_UNASSIGNED,		/* " */
	F_UNASSIGNED,		/* # */
	F_TOEND,			/* $ */
	F_UNASSIGNED,		/* % */
	F_UNASSIGNED,		/* & */
	F_UNASSIGNED,		/* ' */
	F_UNASSIGNED,		/* ( */
	F_UNASSIGNED,		/* ) */
	F_EXPAND_GLOB,		/* * */
	F_DOWN_HIST,		/* + */
	V_RCHAR_BACK,		/* , */	
	F_UP_HIST,			/* - */	
	F_UNASSIGNED,		/* . */
	V_DSH_META,			/* / */
	V_ZERO,			/* 0 */
	F_ARGDIGIT,			/* 1 */
	F_ARGDIGIT,			/* 2 */
	F_ARGDIGIT,			/* 3 */
	F_ARGDIGIT,			/* 4 */
	F_ARGDIGIT,			/* 5 */
	F_ARGDIGIT,			/* 6 */
	F_ARGDIGIT,			/* 7 */
	F_ARGDIGIT,			/* 8 */
	F_ARGDIGIT,			/* 9 */
	F_UNASSIGNED,		/* : */
	V_RCHAR_FWD,		/* ; */
	F_UNASSIGNED,		/* < */
	F_UNASSIGNED,		/* = */
	F_UNASSIGNED,		/* > */
	V_USH_META,			/* ? */
	F_UNASSIGNED,		/* @ */
	V_ADDEND,			/* A */
	V_WORDBACK,			/* B */
	V_CHGTOEND,			/* C */
	F_KILLEND,			/* D */
	V_ENDWORD,			/* E */
	V_CHAR_BACK,		/* F */
	F_UNASSIGNED,		/* G */
	F_UNASSIGNED,		/* H */
	V_INSBEG,			/* I */
	F_DOWN_SEARCH_HIST,		/* J */
	F_UP_SEARCH_HIST,		/* K */
	F_UNASSIGNED,		/* L */
	F_UNASSIGNED,		/* M */
	V_RSRCH_BACK,		/* N */
	F_XKEY,			/* O */
	F_UNASSIGNED,		/* P */
	F_UNASSIGNED,		/* Q */
	V_REPLMODE,			/* R */
	V_SUBSTLINE,		/* S */
	V_CHARTO_BACK,		/* T */
	F_UNASSIGNED,		/* U */
	F_EXPAND_VARS,		/* V */
	V_WORDFWD,			/* W */
	F_DELPREV,			/* X */
	F_UNASSIGNED,		/* Y */
	F_UNASSIGNED,		/* Z */
	F_XKEY,			/* [ */
	F_UNASSIGNED,		/* \ */
	F_UNASSIGNED,		/* ] */
	F_TOBEG,			/* ^ */
	F_UNASSIGNED,		/* _ */
	F_UNASSIGNED,		/* ` */
	V_ADD,			/* a */
	F_WORDBACK,			/* b */
	V_CHGMETA,			/* c */
	V_DELMETA,			/* d */
	V_EWORD,			/* e */
	V_CHAR_FWD,			/* f */
	F_UNASSIGNED,		/* g */
	F_CHARBACK,			/* h */
	V_INSERT,			/* i */
	F_DOWN_HIST,		/* j */
	F_UP_HIST,			/* k */
	F_CHARFWD,			/* l */
	F_UNASSIGNED,		/* m */
	V_RSRCH_FWD,		/* n */
	F_UNASSIGNED,		/* o */
	F_UNASSIGNED,		/* p */
	F_UNASSIGNED,		/* q */
	V_REPLONE,			/* r */
	V_SUBSTCHAR,		/* s */
	V_CHARTO_FWD,		/* t */
	V_UNDO,			/* u */
	F_EXPAND_VARS,		/* v */
	V_WORDBEGNEXT,		/* w */
	F_DELNEXT_EOF,		/* x */
	F_UNASSIGNED,		/* y */
	F_UNASSIGNED,		/* z */
	F_UNASSIGNED,		/* { */
	F_UNASSIGNED,		/* | */
	F_UNASSIGNED,		/* } */
	V_CHGCASE,			/* ~ */
	F_DELPREV,			/* ^? */
	F_UNASSIGNED,		/* M-^@ */
	F_UNASSIGNED,		/* M-^A */
	F_UNASSIGNED,		/* M-^B */
	F_UNASSIGNED,		/* M-^C */
	F_UNASSIGNED,		/* M-^D */
	F_UNASSIGNED,		/* M-^E */
	F_UNASSIGNED,		/* M-^F */
	F_UNASSIGNED,		/* M-^G */
	F_UNASSIGNED,		/* M-^H */
	F_UNASSIGNED,		/* M-^I */
	F_UNASSIGNED,		/* M-^J */
	F_UNASSIGNED,		/* M-^K */
	F_UNASSIGNED,		/* M-^L */
	F_UNASSIGNED,		/* M-^M */
	F_UNASSIGNED,		/* M-^N */
	F_UNASSIGNED,		/* M-^O */
	F_UNASSIGNED,		/* M-^P */
	F_UNASSIGNED,		/* M-^Q */
	F_UNASSIGNED,		/* M-^R */
	F_UNASSIGNED,		/* M-^S */
	F_UNASSIGNED,		/* M-^T */
	F_UNASSIGNED,		/* M-^U */
	F_UNASSIGNED,		/* M-^V */
	F_UNASSIGNED,		/* M-^W */
	F_UNASSIGNED,		/* M-^X */
	F_UNASSIGNED,		/* M-^Y */
	F_UNASSIGNED,		/* M-^Z */
	F_UNASSIGNED,		/* M-^[ */
	F_UNASSIGNED,		/* M-^\ */
	F_UNASSIGNED,		/* M-^] */
	F_UNASSIGNED,		/* M-^^ */
	F_UNASSIGNED,		/* M-^_ */
	F_UNASSIGNED,		/* M-SPACE */
	F_UNASSIGNED,		/* M-! */
	F_UNASSIGNED,		/* M-" */
	F_UNASSIGNED,		/* M-# */
	F_UNASSIGNED,		/* M-$ */
	F_UNASSIGNED,		/* M-% */
	F_UNASSIGNED,		/* M-& */
	F_UNASSIGNED,		/* M-' */
	F_UNASSIGNED,		/* M-( */
	F_UNASSIGNED,		/* M-) */
	F_UNASSIGNED,		/* M-* */
	F_UNASSIGNED,		/* M-+ */
	F_UNASSIGNED,		/* M-, */
	F_UNASSIGNED,		/* M-- */
	F_UNASSIGNED,		/* M-. */
	F_UNASSIGNED,		/* M-/ */
	F_UNASSIGNED,		/* M-0 */
	F_UNASSIGNED,		/* M-1 */
	F_UNASSIGNED,		/* M-2 */
	F_UNASSIGNED,		/* M-3 */
	F_UNASSIGNED,		/* M-4 */
	F_UNASSIGNED,		/* M-5 */
	F_UNASSIGNED,		/* M-6 */
	F_UNASSIGNED,		/* M-7 */
	F_UNASSIGNED,		/* M-8 */
	F_UNASSIGNED,		/* M-9 */
	F_UNASSIGNED,		/* M-: */
	F_UNASSIGNED,		/* M-; */
	F_UNASSIGNED,		/* M-< */
	F_UNASSIGNED,		/* M-= */
	F_UNASSIGNED,		/* M-> */
	F_HELPME,			/* M-? */
	F_UNASSIGNED,		/* M-@ */
	F_UNASSIGNED,		/* M-A */
	F_UNASSIGNED,		/* M-B */
	F_UNASSIGNED,		/* M-C */
	F_UNASSIGNED,		/* M-D */
	F_UNASSIGNED,		/* M-E */
	F_UNASSIGNED,		/* M-F */
	F_UNASSIGNED,		/* M-G */
	F_UNASSIGNED,		/* M-H */
	F_UNASSIGNED,		/* M-I */
	F_UNASSIGNED,		/* M-J */
	F_UNASSIGNED,		/* M-K */
	F_UNASSIGNED,		/* M-L */
	F_UNASSIGNED,		/* M-M */
	F_UNASSIGNED,		/* M-N */
	F_XKEY,			/* M-O *//* extended key esc PWP Mar 88 */
	F_UNASSIGNED,		/* M-P */
	F_UNASSIGNED,		/* M-Q */
	F_UNASSIGNED,		/* M-R */
	F_UNASSIGNED,		/* M-S */
	F_UNASSIGNED,		/* M-T */
	F_UNASSIGNED,		/* M-U */
	F_UNASSIGNED,		/* M-V */
	F_UNASSIGNED,		/* M-W */
	F_UNASSIGNED,		/* M-X */
	F_UNASSIGNED,		/* M-Y */
	F_UNASSIGNED,		/* M-Z */
	F_XKEY,			/* M-[ *//* extended key esc -mf Oct 87 */
	F_UNASSIGNED,		/* M-\ */
	F_UNASSIGNED,		/* M-] */
	F_UNASSIGNED,		/* M-^ */
	F_UNASSIGNED,		/* M-_ */
	F_UNASSIGNED,		/* M-` */
	F_UNASSIGNED,		/* M-a */
	F_UNASSIGNED,		/* M-b */
	F_UNASSIGNED,		/* M-c */
	F_UNASSIGNED,		/* M-d */
	F_UNASSIGNED,		/* M-e */
	F_UNASSIGNED,		/* M-f */
	F_UNASSIGNED,		/* M-g */
	F_UNASSIGNED,		/* M-h */
	F_UNASSIGNED,		/* M-i */
	F_UNASSIGNED,		/* M-j */
	F_UNASSIGNED,		/* M-k */
	F_UNASSIGNED,		/* M-l */
	F_UNASSIGNED,		/* M-m */
	F_UNASSIGNED,		/* M-n */
	F_UNASSIGNED,		/* M-o */
	F_UNASSIGNED,		/* M-p */
	F_UNASSIGNED,		/* M-q */
	F_UNASSIGNED,		/* M-r */
	F_UNASSIGNED,		/* M-s */
	F_UNASSIGNED,		/* M-t */
	F_UNASSIGNED,		/* M-u */
	F_UNASSIGNED,		/* M-v */
	F_UNASSIGNED,		/* M-w */
	F_UNASSIGNED,		/* M-x */
	F_UNASSIGNED,		/* M-y */
	F_UNASSIGNED,		/* M-z */
	F_UNASSIGNED,		/* M-{ */
	F_UNASSIGNED,		/* M-| */
	F_UNASSIGNED,		/* M-} */
	F_UNASSIGNED,		/* M-~ */
	F_UNASSIGNED,		/* M-^? */
	/* extra keys begin here */
	F_UNASSIGNED,		/* f-1 */
	F_UNASSIGNED,		/* f-2 */
	F_UNASSIGNED,		/* f-3 */
	F_UNASSIGNED,		/* f-4 */
	F_UNASSIGNED,		/* f-5 */
	F_UNASSIGNED,		/* f-6 */
	F_UNASSIGNED,		/* f-7 */
	F_UNASSIGNED,		/* f-8 */
	F_UNASSIGNED,		/* f-9 */
	F_UNASSIGNED,		/* f-10 */
	F_UNASSIGNED,		/* f-11 */
	F_UNASSIGNED,		/* f-12 */
	F_UNASSIGNED,		/* f-13 */
	F_UNASSIGNED,		/* f-14 */
	F_UNASSIGNED,		/* f-15 */
	F_UNASSIGNED,		/* f-16 */
	F_UNASSIGNED,		/* f-17 */
	F_UNASSIGNED,		/* f-18 */
	F_UNASSIGNED,		/* f-19 */
	F_UNASSIGNED,		/* f-20 */
	F_UNASSIGNED,		/* f-21 */
	F_UNASSIGNED,		/* f-22 */
	F_UNASSIGNED,		/* f-23 */
	F_UNASSIGNED,		/* f-24 */
	F_UNASSIGNED,		/* PgUp */
	F_UNASSIGNED,		/* PgDn */
	F_UNASSIGNED,		/* end */
	F_UNASSIGNED,		/* home */
	F_UNASSIGNED,		/* LEFT */
	F_UNASSIGNED,		/* UP */
	F_UNASSIGNED,		/* RIGHT */
	F_UNASSIGNED,		/* DOWN */
	F_UNASSIGNED,		/* INS */
	F_UNASSIGNED,		/* DEL */

	/* ctrl key mappings */
	F_UNASSIGNED,		/* f-1 */
	F_UNASSIGNED,		/* f-2 */
	F_UNASSIGNED,		/* f-3 */
	F_UNASSIGNED,		/* f-4 */
	F_UNASSIGNED,		/* f-5 */
	F_UNASSIGNED,		/* f-6 */
	F_UNASSIGNED,		/* f-7 */
	F_UNASSIGNED,		/* f-8 */
	F_UNASSIGNED,		/* f-9 */
	F_UNASSIGNED,		/* f-10 */
	F_UNASSIGNED,		/* f-11 */
	F_UNASSIGNED,		/* f-12 */
	F_UNASSIGNED,		/* f-13 */
	F_UNASSIGNED,		/* f-14 */
	F_UNASSIGNED,		/* f-15 */
	F_UNASSIGNED,		/* f-16 */
	F_UNASSIGNED,		/* f-17 */
	F_UNASSIGNED,		/* f-18 */
	F_UNASSIGNED,		/* f-19 */
	F_UNASSIGNED,		/* f-20 */
	F_UNASSIGNED,		/* f-21 */
	F_UNASSIGNED,		/* f-22 */
	F_UNASSIGNED,		/* f-23 */
	F_UNASSIGNED,		/* f-24 */
	F_UNASSIGNED,		/* PgUp */
	F_UNASSIGNED,		/* PgDn */
	F_UNASSIGNED,		/* end */
	F_UNASSIGNED,		/* home */
	F_UNASSIGNED,		/* LEFT */
	F_UNASSIGNED,		/* UP */
	F_UNASSIGNED,		/* RIGHT */
	F_UNASSIGNED,		/* DOWN */
	F_UNASSIGNED,		/* INS */
	F_UNASSIGNED,		/* DEL */

	/* alt key mappings */
	F_UNASSIGNED,		/* f-1 */
	F_UNASSIGNED,		/* f-2 */
	F_UNASSIGNED,		/* f-3 */
	F_UNASSIGNED,		/* f-4 */
	F_UNASSIGNED,		/* f-5 */
	F_UNASSIGNED,		/* f-6 */
	F_UNASSIGNED,		/* f-7 */
	F_UNASSIGNED,		/* f-8 */
	F_UNASSIGNED,		/* f-9 */
	F_UNASSIGNED,		/* f-10 */
	F_UNASSIGNED,		/* f-11 */
	F_UNASSIGNED,		/* f-12 */
	F_UNASSIGNED,		/* f-13 */
	F_UNASSIGNED,		/* f-14 */
	F_UNASSIGNED,		/* f-15 */
	F_UNASSIGNED,		/* f-16 */
	F_UNASSIGNED,		/* f-17 */
	F_UNASSIGNED,		/* f-18 */
	F_UNASSIGNED,		/* f-19 */
	F_UNASSIGNED,		/* f-20 */
	F_UNASSIGNED,		/* f-21 */
	F_UNASSIGNED,		/* f-22 */
	F_UNASSIGNED,		/* f-23 */
	F_UNASSIGNED,		/* f-24 */
	F_UNASSIGNED,		/* PgUp */
	F_UNASSIGNED,		/* PgDn */
	F_UNASSIGNED,		/* end */
	F_UNASSIGNED,		/* home */
	F_UNASSIGNED,		/* LEFT */
	F_UNASSIGNED,		/* UP */
	F_UNASSIGNED,		/* RIGHT */
	F_UNASSIGNED,		/* DOWN */
	F_UNASSIGNED,		/* INS */
	F_UNASSIGNED,		/* DEL */
	/* shift key mappings */
	F_UNASSIGNED,		/* f-1 */
	F_UNASSIGNED,		/* f-2 */
	F_UNASSIGNED,		/* f-3 */
	F_UNASSIGNED,		/* f-4 */
	F_UNASSIGNED,		/* f-5 */
	F_UNASSIGNED,		/* f-6 */
	F_UNASSIGNED,		/* f-7 */
	F_UNASSIGNED,		/* f-8 */
	F_UNASSIGNED,		/* f-9 */
	F_UNASSIGNED,		/* f-10 */
	F_UNASSIGNED,		/* f-11 */
	F_UNASSIGNED,		/* f-12 */
	F_UNASSIGNED,		/* f-13 */
	F_UNASSIGNED,		/* f-14 */
	F_UNASSIGNED,		/* f-15 */
	F_UNASSIGNED,		/* f-16 */
	F_UNASSIGNED,		/* f-17 */
	F_UNASSIGNED,		/* f-18 */
	F_UNASSIGNED,		/* f-19 */
	F_UNASSIGNED,		/* f-20 */
	F_UNASSIGNED,		/* f-21 */
	F_UNASSIGNED,		/* f-22 */
	F_UNASSIGNED,		/* f-23 */
	F_UNASSIGNED,		/* f-24 */
	F_UNASSIGNED,		/* PgUp */
	F_UNASSIGNED,		/* PgDn */
	F_UNASSIGNED,		/* end */
	F_UNASSIGNED,		/* home */
	F_UNASSIGNED,		/* LEFT */
	F_UNASSIGNED,		/* UP */
	F_UNASSIGNED,		/* RIGHT */
	F_UNASSIGNED,		/* DOWN */
	F_UNASSIGNED,		/* INS */
	F_UNASSIGNED		/* DEL */
};
	static void
nt_bad_spec(const Char *keystr)
{
	xprintf(CGETS(20, 4, "Bad key spec %S\n"), keystr);
}
extern int lstricmp(char*,char*);
Char nt_translate_bindkey(const Char*s) {
	char *astr = short2str(s);
	short fkey;
	char corm; /* 1 for ctrl map, 2 for meta map, 3 for shift map*/
	Char keycode = 0;

	corm = 0;

	if (astr[0] == 'C') 
		corm= 1;
	else if (astr[0] == 'M')
		corm = 2;
	else if (astr[0] == 'S') /*shift keymap by avner.lottem@intel.com*/
		corm = 3;

	if (corm)
		astr += 2; /* skip C- or M- or S-*/

	fkey = (short)atoi(astr);
	if (fkey !=0) {
		keycode = (NT_SPECIFIC_BINDING_OFFSET+ (fkey-1) );
	}
	else {
		if (!_stricmp("pgup",astr)) {
			keycode =  (NT_SPECIFIC_BINDING_OFFSET + KEYPAD_MAPPING_BEGIN);
		}
		else if (!_stricmp("pgdown",astr)) {
			keycode = (NT_SPECIFIC_BINDING_OFFSET + KEYPAD_MAPPING_BEGIN + 1);
		}
		else if (!_stricmp("end",astr)) {
			keycode =  (NT_SPECIFIC_BINDING_OFFSET + KEYPAD_MAPPING_BEGIN + 2);
		}
		else if (!_stricmp("home",astr)) {
			keycode =  (NT_SPECIFIC_BINDING_OFFSET + KEYPAD_MAPPING_BEGIN + 3);
		}
		else if (!_stricmp("left",astr)) {
			keycode =  (NT_SPECIFIC_BINDING_OFFSET + KEYPAD_MAPPING_BEGIN + 4);
		}
		else if (!_stricmp("up",astr)) {
			keycode =  (NT_SPECIFIC_BINDING_OFFSET + KEYPAD_MAPPING_BEGIN + 5);
		}
		else if (!_stricmp("right",astr)) {
			keycode =  (NT_SPECIFIC_BINDING_OFFSET + KEYPAD_MAPPING_BEGIN + 6);
		}
		else if (!_stricmp("down",astr)) {
			keycode =  (NT_SPECIFIC_BINDING_OFFSET + KEYPAD_MAPPING_BEGIN + 7);
		}
		else if (!_stricmp("ins",astr)) {
			keycode =  (NT_SPECIFIC_BINDING_OFFSET + INS_DEL_MAPPING_BEGIN );
		}
		else if (!_stricmp("del",astr)) {
			keycode =  (NT_SPECIFIC_BINDING_OFFSET +INS_DEL_MAPPING_BEGIN +1 );
		}
		else
			nt_bad_spec(s);
	}
	if (keycode && corm) {
		if (corm == 1)
			keycode +=  CTRL_KEY_OFFSET;
		else if (corm == 2)
			keycode +=  ALT_KEY_OFFSET;
		else if (corm == 3)
			keycode +=  SHIFT_KEY_OFFSET;
	}

	return keycode;
}
