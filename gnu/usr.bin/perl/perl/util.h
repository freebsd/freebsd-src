/* $RCSfile: util.h,v $$Revision: 1.1.1.1 $$Date: 1994/09/10 06:27:35 $
 *
 *    Copyright (c) 1991, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * $Log: util.h,v $
 * Revision 1.1.1.1  1994/09/10  06:27:35  gclarkii
 * Initial import of Perl 4.046 bmaked
 *
 * Revision 1.1.1.1  1993/08/23  21:29:40  nate
 * PERL!
 *
 * Revision 4.0.1.4  92/06/11  21:19:36  lwall
 * patch34: pidgone() wasn't declared right
 *
 * Revision 4.0.1.3  92/06/08  16:09:20  lwall
 * patch20: bcopy() and memcpy() now tested for overlap safety
 *
 * Revision 4.0.1.2  91/11/05  19:18:40  lwall
 * patch11: safe malloc code now integrated into Perl's malloc when possible
 *
 * Revision 4.0.1.1  91/06/07  12:11:00  lwall
 * patch4: new copyright notice
 *
 * Revision 4.0  91/03/20  01:56:48  lwall
 * 4.0 baseline.
 *
 */

EXT int *screamfirst INIT(Null(int*));
EXT int *screamnext INIT(Null(int*));

#ifndef safemalloc
char	*safemalloc();
char	*saferealloc();
#endif
char	*cpytill();
char	*instr();
char	*fbminstr();
char	*screaminstr();
void	fbmcompile();
char	*savestr();
void	my_setenv();
int	envix();
void	growstr();
char	*ninstr();
char	*rninstr();
char	*nsavestr();
FILE	*mypopen();
int	mypclose();
#if !defined(HAS_BCOPY) || !defined(SAFE_BCOPY)
char	*my_bcopy();
#endif
#if !defined(HAS_BZERO) && !defined(HAS_MEMSET)
char	*my_bzero();
#endif
#ifndef HAS_MEMCMP
int	my_memcmp();
#endif
unsigned long scanoct();
unsigned long scanhex();
void pidgone();
