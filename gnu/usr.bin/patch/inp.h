/* $FreeBSD$
 *
 * $Log: inp.h,v $
 * Revision 2.0  86/09/17  15:37:25  lwall
 * Baseline for netwide release.
 *
 */

EXT LINENUM input_lines INIT(0);	/* how long is input file in lines */
EXT LINENUM last_frozen_line INIT(0);	/* how many input lines have been */
					/* irretractibly output */
void	 re_input(void);
void	 scan_input(char *_filename);
bool	 plan_a(char *_filename);
void	 plan_b(char *_filename);
bool	 rev_in_string(char *_string);
char	*ifetch(LINENUM _line, int _whichbuf);

