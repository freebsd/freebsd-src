/* $Header: /pub/FreeBSD/FreeBSD-CVS/src/gnu/usr.bin/patch/pch.h,v 1.2 1995/05/30 05:02:36 rgrimes Exp $
 *
 * $Log: pch.h,v $
 * Revision 1.2  1995/05/30  05:02:36  rgrimes
 * Remove trailing whitespace.
 *
 * Revision 1.1.1.1  1993/06/19  14:21:52  paul
 * b-maked patch-2.10
 *
 * Revision 2.0.1.1  87/01/30  22:47:16  lwall
 * Added do_ed_script().
 *
 * Revision 2.0  86/09/17  15:39:57  lwall
 * Baseline for netwide release.
 *
 */

EXT FILE *pfp INIT(Nullfp);		/* patch file pointer */

void re_patch();
void open_patch_file();
void set_hunkmax();
void grow_hunkmax();
bool there_is_another_patch();
int intuit_diff_type();
void next_intuit_at();
void skip_to();
bool another_hunk();
bool pch_swap();
char *pfetch();
short pch_line_len();
LINENUM pch_first();
LINENUM pch_ptrn_lines();
LINENUM pch_newfirst();
LINENUM pch_repl_lines();
LINENUM pch_end();
LINENUM pch_context();
LINENUM pch_hunk_beg();
char pch_char();
char *pfetch();
char *pgets();
void do_ed_script();
