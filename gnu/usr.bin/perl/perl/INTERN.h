/* INTERN.h,v1.21995/05/30 05:02:47
 *
 *    Copyright (c) 1991, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * INTERN.h,v
 * Revision 1.2  1995/05/30  05:02:47  rgrimes
 * Remove trailing whitespace.
 *
 * Revision 1.1.1.1  1994/09/10  06:27:34  gclarkii
 * Initial import of Perl 4.046 bmaked
 *
 * Revision 1.1.1.1  1993/08/23  21:29:33  nate
 * PERL!
 *
 * Revision 4.0.1.1  91/06/07  10:10:42  lwall
 * patch4: new copyright notice
 *
 * Revision 4.0  91/03/20  00:58:35  lwall
 * 4.0 baseline.
 *
 */

#undef EXT
#define EXT

#undef INIT
#define INIT(x) = x

#define DOINIT
