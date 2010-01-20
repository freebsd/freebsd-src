/* $FreeBSD$
 *
 * $Log: pch.h,v $
 * Revision 2.0.1.1  87/01/30  22:47:16  lwall
 * Added do_ed_script().
 *
 * Revision 2.0  86/09/17  15:39:57  lwall
 * Baseline for netwide release.
 *
 */

EXT FILE *pfp INIT(Nullfp);		/* patch file pointer */

void	 re_patch(void);
void	 open_patch_file(char *_filename);
void	 set_hunkmax(void);
void	 grow_hunkmax(void);
bool	 there_is_another_patch(void);
int	 intuit_diff_type(void);
void	 next_intuit_at(long _file_pos, long _file_line);
void	 skip_to(long _file_pos, long _file_line);
bool	 another_hunk(void);
bool	 pch_swap(void);
char	*pfetch(LINENUM _line);
short	 pch_line_len(LINENUM _line);
LINENUM	 pch_first(void);
LINENUM	 pch_ptrn_lines(void);
LINENUM	 pch_newfirst(void);
LINENUM	 pch_repl_lines(void);
LINENUM	 pch_end(void);
LINENUM	 pch_context(void);
LINENUM	 pch_hunk_beg(void);
char	 pch_char(LINENUM _line);
char	*pfetch(LINENUM _line);
size_t	 pgets(bool _do_indent);
void	 do_ed_script(void);
