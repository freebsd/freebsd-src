/* $RCSfile: handy.h,v $$Revision: 1.1.1.1 $$Date: 1994/09/10 06:27:34 $
 *
 *    Copyright (c) 1991, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * $Log: handy.h,v $
 * Revision 1.1.1.1  1994/09/10  06:27:34  gclarkii
 * Initial import of Perl 4.046 bmaked
 *
 * Revision 1.1.1.1  1993/08/23  21:29:36  nate
 * PERL!
 *
 * Revision 4.0.1.4  92/06/08  13:23:17  lwall
 * patch20: isascii() may now be supplied by a library routine
 * patch20: Perl now distinguishes overlapped copies from non-overlapped
 *
 * Revision 4.0.1.3  91/11/05  22:54:26  lwall
 * patch11: erratum
 *
 * Revision 4.0.1.2  91/11/05  17:23:38  lwall
 * patch11: prepared for ctype implementations that don't define isascii()
 *
 * Revision 4.0.1.1  91/06/07  11:09:56  lwall
 * patch4: new copyright notice
 *
 * Revision 4.0  91/03/20  01:22:15  lwall
 * 4.0 baseline.
 *
 */

#ifdef NULL
#undef NULL
#endif
#ifndef I286
#  define NULL 0
#else
#  define NULL 0L
#endif
#define Null(type) ((type)NULL)
#define Nullch Null(char*)
#define Nullfp Null(FILE*)

#ifdef UTS
#define bool int
#else
#define bool char
#endif

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

#if defined(CTYPE256) || (!defined(isascii) && !defined(HAS_ISASCII))
#define isALNUM(c) (isalpha(c) || isdigit(c) || c == '_')
#define isALPHA(c) isalpha(c)
#define isSPACE(c) isspace(c)
#define isDIGIT(c) isdigit(c)
#define isUPPER(c) isupper(c)
#define isLOWER(c) islower(c)
#else
#define isALNUM(c) (isascii(c) && (isalpha(c) || isdigit(c) || c == '_'))
#define isALPHA(c) (isascii(c) && isalpha(c))
#define isSPACE(c) (isascii(c) && isspace(c))
#define isDIGIT(c) (isascii(c) && isdigit(c))
#define isUPPER(c) (isascii(c) && isupper(c))
#define isLOWER(c) (isascii(c) && islower(c))
#endif

/* Line numbers are unsigned, 16 bits. */
typedef unsigned short line_t;
#ifdef lint
#define NOLINE ((line_t)0)
#else
#define NOLINE ((line_t) 65535)
#endif

#ifndef lint
#ifndef LEAKTEST
#ifndef safemalloc
char *safemalloc();
char *saferealloc();
void safefree();
#endif
#ifndef MSDOS
#define New(x,v,n,t)  (v = (t*)safemalloc((MEM_SIZE)((n) * sizeof(t))))
#define Newc(x,v,n,t,c)  (v = (c*)safemalloc((MEM_SIZE)((n) * sizeof(t))))
#define Newz(x,v,n,t) (v = (t*)safemalloc((MEM_SIZE)((n) * sizeof(t)))), \
    memzero((char*)(v), (n) * sizeof(t))
#define Renew(v,n,t) (v = (t*)saferealloc((char*)(v),(MEM_SIZE)((n)*sizeof(t))))
#define Renewc(v,n,t,c) (v = (c*)saferealloc((char*)(v),(MEM_SIZE)((n)*sizeof(t))))
#else
#define New(x,v,n,t)  (v = (t*)safemalloc(((unsigned long)(n) * sizeof(t))))
#define Newc(x,v,n,t,c)  (v = (c*)safemalloc(((unsigned long)(n) * sizeof(t))))
#define Newz(x,v,n,t) (v = (t*)safemalloc(((unsigned long)(n) * sizeof(t)))), \
    memzero((char*)(v), (n) * sizeof(t))
#define Renew(v,n,t) (v = (t*)saferealloc((char*)(v),((unsigned long)(n)*sizeof(t))))
#define Renewc(v,n,t,c) (v = (c*)saferealloc((char*)(v),((unsigned long)(n)*sizeof(t))))
#endif /* MSDOS */
#define Safefree(d) safefree((char*)d)
#define Str_new(x,len) str_new(len)
#else /* LEAKTEST */
char *safexmalloc();
char *safexrealloc();
void safexfree();
#define New(x,v,n,t)  (v = (t*)safexmalloc(x,(MEM_SIZE)((n) * sizeof(t))))
#define Newc(x,v,n,t,c)  (v = (c*)safexmalloc(x,(MEM_SIZE)((n) * sizeof(t))))
#define Newz(x,v,n,t) (v = (t*)safexmalloc(x,(MEM_SIZE)((n) * sizeof(t)))), \
    memzero((char*)(v), (n) * sizeof(t))
#define Renew(v,n,t) (v = (t*)safexrealloc((char*)(v),(MEM_SIZE)((n)*sizeof(t))))
#define Renewc(v,n,t,c) (v = (c*)safexrealloc((char*)(v),(MEM_SIZE)((n)*sizeof(t))))
#define Safefree(d) safexfree((char*)d)
#define Str_new(x,len) str_new(x,len)
#define MAXXCOUNT 1200
long xcount[MAXXCOUNT];
long lastxcount[MAXXCOUNT];
#endif /* LEAKTEST */
#define Move(s,d,n,t) (void)memmove((char*)(d),(char*)(s), (n) * sizeof(t))
#define Copy(s,d,n,t) (void)memcpy((char*)(d),(char*)(s), (n) * sizeof(t))
#define Zero(d,n,t) (void)memzero((char*)(d), (n) * sizeof(t))
#else /* lint */
#define New(x,v,n,s) (v = Null(s *))
#define Newc(x,v,n,s,c) (v = Null(s *))
#define Newz(x,v,n,s) (v = Null(s *))
#define Renew(v,n,s) (v = Null(s *))
#define Move(s,d,n,t)
#define Copy(s,d,n,t)
#define Zero(d,n,t)
#define Safefree(d) d = d
#endif /* lint */

#ifdef STRUCTCOPY
#define StructCopy(s,d,t) *((t*)(d)) = *((t*)(s))
#else
#define StructCopy(s,d,t) Copy(s,d,1,t)
#endif
