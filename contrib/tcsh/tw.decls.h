/* $Header: /src/pub/tcsh/tw.decls.h,v 3.18 2002/03/08 17:36:47 christos Exp $ */
/*
 * tw.decls.h: Tenex external declarations
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
#ifndef _h_tw_decls
#define _h_tw_decls

/*
 * tw.help.c
 */
extern	void		  do_help		__P((Char *));

/*
 * tw.parse.c
 */
extern	 Char		 *dollar		__P((Char *, const Char *));
#ifndef __MVS__
extern	 int		  tenematch		__P((Char *, int, COMMAND));
extern	 int		  t_search		__P((Char *, Char *, COMMAND, 
						     int, int, int, Char *, 
						     int));
#endif
extern	 int		  starting_a_command	__P((Char *, Char *));
extern	 void		  copyn			__P((Char *, Char *, int));
extern	 void		  catn			__P((Char *, Char *, int));
extern	 int		  fcompare		__P((Char **, Char **));
extern	 void		  print_by_column	__P((Char *, Char *[], int, 
						     int));
extern	 int		  StrQcmp		__P((Char *, Char *));
extern	 Char		 *tgetenv		__P((Char *));

/*
 * tw.init.c
 */
extern	 void		  tw_alias_start	__P((DIR *, Char *));
extern	 void		  tw_cmd_start		__P((DIR *, Char *));
extern	 void		  tw_logname_start	__P((DIR *, Char *));
extern	 void		  tw_var_start		__P((DIR *, Char *));
extern	 void		  tw_complete_start	__P((DIR *, Char *));
extern	 void		  tw_file_start		__P((DIR *, Char *));
extern	 void		  tw_vl_start		__P((DIR *, Char *));
extern	 void		  tw_wl_start		__P((DIR *, Char *));
extern	 void		  tw_bind_start		__P((DIR *, Char *));
extern	 void		  tw_limit_start	__P((DIR *, Char *));
extern	 void		  tw_sig_start		__P((DIR *, Char *));
extern	 void		  tw_job_start		__P((DIR *, Char *));
extern	 void		  tw_grpname_start	__P((DIR *, Char *));
extern	 Char		 *tw_cmd_next		__P((Char *, int *));
extern	 Char		 *tw_logname_next	__P((Char *, int *));
extern	 Char		 *tw_shvar_next		__P((Char *, int *));
extern	 Char		 *tw_envvar_next	__P((Char *, int *));
extern	 Char		 *tw_var_next		__P((Char *, int *));
extern	 Char		 *tw_file_next		__P((Char *, int *));
extern	 Char		 *tw_wl_next		__P((Char *, int *));
extern	 Char		 *tw_bind_next		__P((Char *, int *));
extern	 Char		 *tw_limit_next		__P((Char *, int *));
extern	 Char		 *tw_sig_next		__P((Char *, int *));
extern	 Char		 *tw_job_next		__P((Char *, int *));
extern	 Char		 *tw_grpname_next	__P((Char *, int *));
extern	 void		  tw_dir_end		__P((void));
extern	 void		  tw_cmd_free		__P((void));
extern	 void		  tw_logname_end	__P((void));
extern	 void		  tw_grpname_end	__P((void));
extern	 Char		 *tw_item_add		__P((int));
extern	 Char	        **tw_item_get		__P((void));
extern	 void		  tw_item_free		__P((void));
extern	 Char		 *tw_item_find		__P((Char *));

/*
 * tw.spell.c
 */
extern	 int		  spell_me		__P((Char *, int, int,
						     Char *, int));
extern	 int		  spdir			__P((Char *, Char *, Char *, 
						     Char *));
extern	 int		  spdist		__P((Char *, Char *));

/*
 * tw.comp.c
 */
extern	 void		  docomplete		__P((Char **, 
						     struct command *));
extern	 void		  douncomplete		__P((Char **, 
						     struct command *));
extern	 int		  tw_complete		__P((Char *, Char **, 
						     Char **, int, int *));
#ifdef COLOR_LS_F
/*
 * tw.color.c
 */
extern	 void		  set_color_context	__P((void));
extern	 void		  print_with_color	__P((Char *, size_t, int));
extern	 void		  parseLS_COLORS	__P((Char *));
#endif /* COLOR_LS_F */

#endif /* _h_tw_decls */
