/* $Header: /pub/FreeBSD/FreeBSD-CVS/src/gnu/usr.bin/patch/inp.h,v 1.2 1995/05/30 05:02:33 rgrimes Exp $
 *
 * $Log: inp.h,v $
 * Revision 1.2  1995/05/30  05:02:33  rgrimes
 * Remove trailing whitespace.
 *
 * Revision 1.1.1.1  1993/06/19  14:21:52  paul
 * b-maked patch-2.10
 *
 * Revision 2.0  86/09/17  15:37:25  lwall
 * Baseline for netwide release.
 *
 */

EXT LINENUM input_lines INIT(0);	/* how long is input file in lines */
EXT LINENUM last_frozen_line INIT(0);	/* how many input lines have been */
					/* irretractibly output */

bool rev_in_string();
void scan_input();
bool plan_a();			/* returns false if insufficient memory */
void plan_b();
char *ifetch();

