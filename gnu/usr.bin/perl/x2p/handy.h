/* $RCSfile: handy.h,v $$Revision: 1.1.1.1 $$Date: 1993/08/23 21:30:10 $
 *
 *    Copyright (c) 1991, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * $Log: handy.h,v $
 * Revision 1.1.1.1  1993/08/23  21:30:10  nate
 * PERL!
 *
 * Revision 4.0.1.2  91/06/07  12:15:43  lwall
 * patch4: new copyright notice
 * 
 * Revision 4.0.1.1  91/04/12  09:29:08  lwall
 * patch1: random cleanup in cpp namespace
 * 
 * Revision 4.0  91/03/20  01:57:45  lwall
 * 4.0 baseline.
 * 
 */

#define Null(type) ((type)0)
#define Nullch Null(char*)
#define Nullfp Null(FILE*)

#define bool char
#ifdef TRUE
#undef TRUE
#endif
#ifdef FALSE
#undef FALSE
#endif
#define TRUE (1)
#define FALSE (0)

#define Ctl(ch) (ch & 037)

#define strNE(s1,s2) (strcmp(s1,s2))
#define strEQ(s1,s2) (!strcmp(s1,s2))
#define strLT(s1,s2) (strcmp(s1,s2) < 0)
#define strLE(s1,s2) (strcmp(s1,s2) <= 0)
#define strGT(s1,s2) (strcmp(s1,s2) > 0)
#define strGE(s1,s2) (strcmp(s1,s2) >= 0)
#define strnNE(s1,s2,l) (strncmp(s1,s2,l))
#define strnEQ(s1,s2,l) (!strncmp(s1,s2,l))
