/* $RCSfile: INTERN.h,v $$Revision: 1.1.1.1 $$Date: 1993/08/23 21:29:33 $
 *
 *    Copyright (c) 1991, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * $Log: INTERN.h,v $
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
