/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *
 *	@(#)stdlib.h	8.5 (Berkeley) 5/19/95
 * $FreeBSD$
 */

#ifndef _STDLIB_H_
#define	_STDLIB_H_

#include <sys/cdefs.h>

#include <machine/ansi.h>

#if !defined(_ANSI_SOURCE) && !defined(_POSIX_SOURCE)
#ifdef	_BSD_RUNE_T_
typedef	_BSD_RUNE_T_	rune_t;
#undef	_BSD_RUNE_T_
#endif
#endif

#ifdef	_BSD_SIZE_T_
typedef	_BSD_SIZE_T_	size_t;
#undef	_BSD_SIZE_T_
#endif

#ifdef	_BSD_WCHAR_T_
typedef	_BSD_WCHAR_T_	wchar_t;
#undef	_BSD_WCHAR_T_
#endif

typedef struct {
	int quot;		/* quotient */
	int rem;		/* remainder */
} div_t;

typedef struct {
	long quot;		/* quotient */
	long rem;		/* remainder */
} ldiv_t;

#ifndef NULL
#define	NULL	0
#endif

#define	EXIT_FAILURE	1
#define	EXIT_SUCCESS	0

#define	RAND_MAX	0x7fffffff

extern int __mb_cur_max;
#define	MB_CUR_MAX	__mb_cur_max

__BEGIN_DECLS
void	 abort __P((void)) __dead2;
int	 abs __P((int)) __pure2;
int	 atexit __P((void (*)(void)));
double	 atof __P((const char *));
int	 atoi __P((const char *));
long	 atol __P((const char *));
void	*bsearch __P((const void *, const void *, size_t,
	    size_t, int (*)(const void *, const void *)));
void	*calloc __P((size_t, size_t));
div_t	 div __P((int, int)) __pure2;
void	 exit __P((int)) __dead2;
void	 free __P((void *));
char	*getenv __P((const char *));
long	 labs __P((long)) __pure2;
ldiv_t	 ldiv __P((long, long)) __pure2;
void	*malloc __P((size_t));
void	 qsort __P((void *, size_t, size_t,
	    int (*)(const void *, const void *)));
int	 rand __P((void));
void	*realloc __P((void *, size_t));
void	 srand __P((unsigned));
double	 strtod __P((const char *, char **));
long	 strtol __P((const char *, char **, int));
long long	 
	 strtoll __P((const char *, char **, int));
unsigned long
	 strtoul __P((const char *, char **, int));
unsigned long long
	 strtoull __P((const char *, char **, int));
int	 system __P((const char *));

int	 mblen __P((const char *, size_t));
size_t	 mbstowcs __P((wchar_t *, const char *, size_t));
int	 wctomb __P((char *, wchar_t));
int	 mbtowc __P((wchar_t *, const char *, size_t));
size_t	 wcstombs __P((char *, const wchar_t *, size_t));

#if !defined(_ANSI_SOURCE) && !defined(_POSIX_SOURCE)
extern char *_malloc_options;
extern void (*_malloc_message)__P((char *p1, char *p2, char *p3, char *p4));

int	 putenv __P((const char *));
int	 setenv __P((const char *, const char *, int));

double	 drand48 __P((void));
double	 erand48 __P((unsigned short[3]));
long	 jrand48 __P((unsigned short[3]));
void	 lcong48 __P((unsigned short[7]));
long	 lrand48 __P((void));
long	 mrand48 __P((void));
long	 nrand48 __P((unsigned short[3]));
unsigned short
	*seed48 __P((unsigned short[3]));
void	 srand48 __P((long));

void	*alloca __P((size_t));		/* built-in for gcc */
					/* getcap(3) functions */
__uint32_t
	 arc4random __P((void));
void	 arc4random_addrandom __P((unsigned char *dat, int datlen));
void	 arc4random_stir __P((void));
char	*getbsize __P((int *, long *));
char	*cgetcap __P((char *, char *, int));
int	 cgetclose __P((void));
int	 cgetent __P((char **, char **, char *));
int	 cgetfirst __P((char **, char **));
int	 cgetmatch __P((char *, char *));
int	 cgetnext __P((char **, char **));
int	 cgetnum __P((char *, char *, long *));
int	 cgetset __P((char *));
int	 cgetstr __P((char *, char *, char **));
int	 cgetustr __P((char *, char *, char **));

int	 daemon __P((int, int));
char	*devname __P((int, int));
int	 getloadavg __P((double [], int));
const char *
	getprogname __P((void));

char	*group_from_gid __P((unsigned long, int));
int	 heapsort __P((void *, size_t, size_t,
	    int (*)(const void *, const void *)));
char	*initstate __P((unsigned long, char *, long));
int	 mergesort __P((void *, size_t, size_t,
	    int (*)(const void *, const void *)));
int	 radixsort __P((const unsigned char **, int, const unsigned char *,
	    unsigned));
int	 sradixsort __P((const unsigned char **, int, const unsigned char *,
	    unsigned));
int	 rand_r __P((unsigned *));
long	 random __P((void));
void    *reallocf __P((void *, size_t));
char	*realpath __P((const char *, char resolved_path[]));
void	 setprogname __P((const char *));
char	*setstate __P((char *));
void	 srandom __P((unsigned long));
void	 sranddev __P((void));
void	 srandomdev __P((void));
char	*user_from_uid __P((unsigned long, int));
#ifndef __STRICT_ANSI__
__int64_t	 strtoq __P((const char *, char **, int));
__uint64_t
	 strtouq __P((const char *, char **, int));
#endif
void	 unsetenv __P((const char *));
#endif /* !_ANSI_SOURCE && !_POSIX_SOURCE */
__END_DECLS

#endif /* !_STDLIB_H_ */
