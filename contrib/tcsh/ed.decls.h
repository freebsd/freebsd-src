/* $Header: /src/pub/tcsh/ed.decls.h,v 3.29 2000/07/15 19:58:50 christos Exp $ */
/*
 * ed.decls.h: Editor external definitions
 */
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
 */
#ifndef _h_ed_decls
#define _h_ed_decls

/*
 * ed.chared.c
 */
extern	int	InsertStr		__P((Char *));
extern	void	DeleteBack		__P((int));

/*
 * ed.init.c
 */
#ifdef SIG_WINDOW
extern	void	check_window_size	__P((int));
extern	sigret_t window_change		__P((int));
#endif /* SIG_WINDOW */
extern	int	ed_Setup		__P((int));
extern	void	ed_Init			__P((void));
extern	int	Cookedmode		__P((void));
extern	int	Rawmode			__P((void));
extern	void	ed_set_tty_eight_bit	__P((void));

extern	void	QuoteModeOn		__P((void));
extern	void	QuoteModeOff		__P((void));
extern	void	ResetInLine		__P((int));
extern	int	Load_input_line		__P((void));

/*
 * ed.term.c:
 */
extern	void	dosetty			__P((Char **, struct command *));
extern	int	tty_getty 		__P((int, ttydata_t *));
extern	int	tty_setty 		__P((int, ttydata_t *));
extern	void	tty_getchar 		__P((ttydata_t *, unsigned char *));
extern	void	tty_setchar 		__P((ttydata_t *, unsigned char *));
extern	speed_t	tty_getspeed 		__P((ttydata_t *));
extern	int	tty_gettabs 		__P((ttydata_t *));
extern	int	tty_geteightbit		__P((ttydata_t *));
extern	int	tty_cooked_mode		__P((ttydata_t *));
#ifdef _IBMR2
extern	void	tty_setdisc		__P((int, int));
#endif /* _IBMR2 */

/*
 * ed.screen.c
 */
extern	void	terminit		__P((void));
extern	void	SetAttributes		__P((int));
extern	void	so_write		__P((Char *, int));
extern	void	ClearScreen		__P((void));
extern	void	MoveToLine		__P((int));
extern	void	MoveToChar		__P((int));
extern	void	ClearEOL		__P((int));
extern	void	Insert_write		__P((Char *, int));
extern	void	DeleteChars		__P((int));
extern	void	TellTC			__P((char *));
extern	void	SetTC			__P((char *, char *));
extern	void	EchoTC			__P((Char **));
extern	int 	SetArrowKeys		__P((CStr *, XmapVal *, int));
extern	int 	IsArrowKey		__P((Char *));
extern	void	ResetArrowKeys		__P((void));
extern	void	DefaultArrowKeys	__P((void));
extern	int 	ClearArrowKeys		__P((CStr *));
extern	void 	PrintArrowKeys		__P((CStr *));
extern	void	BindArrowKeys		__P((void));
extern	void	SoundBeep		__P((void));
extern	int	CanWeTab		__P((void));
extern	void	ChangeSize		__P((int, int));
#ifdef SIG_WINDOW
extern	int	GetSize			__P((int *, int *));
#endif /* SIG_WINDOW */
extern	void	ClearToBottom		__P((void));
extern	void	GetTermCaps		__P((void));

/*
 * ed.defns.c
 */
extern	void	editinit		__P((void));
extern	void	ed_InitNLSMaps		__P((void));
#ifdef DEBUG_EDIT
extern	void	CheckMaps		__P((void));
#endif
extern	void	ed_InitMaps		__P((void));
extern	void	ed_InitEmacsMaps	__P((void));
extern	void	ed_InitVIMaps		__P((void));

extern  CCRETVAL	e_unassigned		__P((int));
extern	CCRETVAL	e_insert		__P((int));
extern	CCRETVAL	e_newline		__P((int));
extern	CCRETVAL	e_delprev		__P((int));
extern	CCRETVAL	e_delnext		__P((int));
/* added by mtk@ari.ncl.omron.co.jp (920818) */
extern	CCRETVAL	e_delnext_eof		__P((int));	
extern	CCRETVAL	e_delnext_list		__P((int));
extern	CCRETVAL	e_delnext_list_eof	__P((int));	/* for ^D */
extern	CCRETVAL	e_toend			__P((int));
extern	CCRETVAL	e_tobeg			__P((int));
extern	CCRETVAL	e_charback		__P((int));
extern	CCRETVAL	e_charfwd		__P((int));
extern	CCRETVAL	e_quote			__P((int));
extern	CCRETVAL	e_startover		__P((int));
extern	CCRETVAL	e_redisp		__P((int));
extern	CCRETVAL	e_wordback		__P((int));
extern	CCRETVAL	e_wordfwd		__P((int));
extern	CCRETVAL	v_wordbegnext		__P((int));
extern	CCRETVAL	e_uppercase		__P((int));
extern	CCRETVAL	e_lowercase		__P((int));
extern	CCRETVAL	e_capitolcase		__P((int));
extern	CCRETVAL	e_cleardisp		__P((int));
extern	CCRETVAL	e_complete		__P((int));
extern	CCRETVAL	e_correct		__P((int));
extern	CCRETVAL	e_correctl		__P((int));
extern	CCRETVAL	e_up_hist		__P((int));
extern	CCRETVAL	e_down_hist		__P((int));
extern	CCRETVAL	e_up_search_hist	__P((int));
extern	CCRETVAL	e_down_search_hist	__P((int));
extern	CCRETVAL	e_helpme		__P((int));
extern	CCRETVAL	e_list_choices		__P((int));
extern	CCRETVAL	e_delwordprev		__P((int));
extern	CCRETVAL	e_delwordnext		__P((int));
extern	CCRETVAL	e_digit			__P((int));
extern	CCRETVAL	e_argdigit		__P((int));
extern	CCRETVAL	v_zero			__P((int));
extern	CCRETVAL	e_killend		__P((int));
extern	CCRETVAL	e_killbeg		__P((int));
extern	CCRETVAL	e_metanext		__P((int));
#ifdef notdef
extern	CCRETVAL	e_extendnext		__P((int));
#endif
extern	CCRETVAL	e_send_eof		__P((int));
extern	CCRETVAL	e_charswitch		__P((int));
extern	CCRETVAL	e_gcharswitch		__P((int));
extern	CCRETVAL	e_which			__P((int));
extern	CCRETVAL	e_yank_kill		__P((int));
extern	CCRETVAL	e_tty_dsusp		__P((int));
extern	CCRETVAL	e_tty_flusho		__P((int));
extern	CCRETVAL	e_tty_quit		__P((int));
extern	CCRETVAL	e_tty_tsusp		__P((int));
extern	CCRETVAL	e_tty_stopo		__P((int));
extern	CCRETVAL	e_tty_starto		__P((int));
extern	CCRETVAL	e_argfour		__P((int));
extern	CCRETVAL	e_set_mark		__P((int));
extern	CCRETVAL	e_exchange_mark		__P((int));
extern	CCRETVAL	e_last_item		__P((int));
extern	CCRETVAL	v_cmd_mode		__P((int));
extern	CCRETVAL	v_insert		__P((int));
extern	CCRETVAL	v_replmode		__P((int));
extern	CCRETVAL	v_replone		__P((int));
extern	CCRETVAL	v_substline		__P((int));
extern	CCRETVAL	v_substchar		__P((int));
extern	CCRETVAL	v_add			__P((int));
extern	CCRETVAL	v_addend		__P((int));
extern	CCRETVAL	v_insbeg		__P((int));
extern	CCRETVAL	v_chgtoend		__P((int));
extern	CCRETVAL	e_killregion		__P((int));
extern	CCRETVAL	e_killall		__P((int));
extern	CCRETVAL	e_copyregion		__P((int));
extern	CCRETVAL	e_tty_int		__P((int));
extern	CCRETVAL	e_run_fg_editor		__P((int));
extern	CCRETVAL	e_list_eof		__P((int));
extern	CCRETVAL	e_expand_history	__P((int));
extern	CCRETVAL	e_magic_space		__P((int));
extern	CCRETVAL	e_list_glob		__P((int));
extern	CCRETVAL	e_expand_glob		__P((int));
extern	CCRETVAL	e_insovr		__P((int));
extern	CCRETVAL	v_cm_complete		__P((int));
extern	CCRETVAL	e_copyprev		__P((int));
extern	CCRETVAL	v_change_case		__P((int));
extern	CCRETVAL	e_expand		__P((int));
extern	CCRETVAL	e_expand_vars		__P((int));
extern	CCRETVAL	e_toggle_hist		__P((int));
extern  CCRETVAL        e_load_average		__P((int));
extern  CCRETVAL        v_delprev		__P((int));
extern  CCRETVAL        v_delmeta		__P((int));
extern  CCRETVAL        v_wordfwd		__P((int));
extern  CCRETVAL        v_wordback		__P((int));
extern  CCRETVAL        v_endword		__P((int));
extern  CCRETVAL        v_eword			__P((int));
extern  CCRETVAL        v_undo			__P((int));
extern  CCRETVAL        v_ush_meta		__P((int));
extern  CCRETVAL        v_dsh_meta		__P((int));
extern  CCRETVAL        v_rsrch_fwd		__P((int));
extern  CCRETVAL        v_rsrch_back		__P((int));
extern  CCRETVAL        v_char_fwd		__P((int));
extern  CCRETVAL        v_char_back		__P((int));
extern  CCRETVAL        v_chgmeta		__P((int));
extern	CCRETVAL	e_inc_fwd		__P((int));
extern	CCRETVAL	e_inc_back		__P((int));
extern	CCRETVAL	v_rchar_fwd		__P((int));
extern	CCRETVAL	v_rchar_back		__P((int));
extern  CCRETVAL        v_charto_fwd		__P((int));
extern  CCRETVAL        v_charto_back		__P((int));
extern  CCRETVAL        e_normalize_path	__P((int));
extern  CCRETVAL        e_normalize_command	__P((int));
extern  CCRETVAL        e_stuff_char		__P((int));
extern  CCRETVAL        e_list_all		__P((int));
extern  CCRETVAL        e_complete_all		__P((int));
extern  CCRETVAL        e_complete_fwd		__P((int));
extern  CCRETVAL        e_complete_back		__P((int));
extern  CCRETVAL        e_dabbrev_expand	__P((int));
extern  CCRETVAL	e_copy_to_clipboard	__P((int));
extern  CCRETVAL	e_paste_from_clipboard	__P((int));
extern  CCRETVAL	e_dosify_next		__P((int));
extern  CCRETVAL	e_dosify_prev		__P((int));
extern  CCRETVAL	e_page_up			__P((int));
extern  CCRETVAL	e_page_down			__P((int));

/*
 * ed.inputl.c
 */
extern	int	Inputl			__P((void));
extern	int	GetNextChar		__P((Char *));
extern	void	PushMacro		__P((Char *));

/*
 * ed.refresh.c
 */
extern	void	ClearLines		__P((void));
extern	void	ClearDisp		__P((void));
extern	void	Refresh			__P((void));
extern	void	RefCursor		__P((void));
extern	void	RefPlusOne		__P((void));
extern	void	PastBottom		__P((void));

/*
 * ed.xmap.c
 */
extern  XmapVal *XmapStr		__P((CStr *));
extern  XmapVal *XmapCmd		__P((int));
extern	void	 AddXkey		__P((CStr *, XmapVal *, int));
extern	void	 ClearXkey		__P((KEYCMD *, CStr *));
extern	int	 GetXkey		__P((CStr *, XmapVal *));
extern	void	 ResetXmap		__P((void));
extern	int	 DeleteXkey		__P((CStr *));
extern	void	 PrintXkey		__P((CStr *));
extern	int	 printOne		__P((CStr *, XmapVal *, int));
extern	int		  parseescape	__P((const Char **));
extern	unsigned char    *unparsestring	__P((CStr *, unsigned char *, Char *));

#endif /* _h_ed_decls */
