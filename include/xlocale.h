/*-
 * Copyright (c) 2011 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by David Chisnall under sponsorship from
 * the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _XLOCALE_H_
#define _XLOCALE_H_

#include <locale.h>

__BEGIN_DECLS

/*
 * Extended locale versions of the locale-aware functions from string.h.
 *
 * Include <string.h> before <xlocale.h> to expose these.
 */
#ifdef _STRING_H_
int	 strcoll_l(const char *, const char *, locale_t);
size_t	 strxfrm_l(char *, const char *, size_t, locale_t);
int	 strcasecmp_l(const char *, const char *, locale_t);
char	*strcasestr_l(const char *, const char *, locale_t);
int	 strncasecmp_l(const char *, const char *, size_t, locale_t);
#endif
/*
 * Extended locale versions of the locale-aware functions from inttypes.h.
 *
 * Include <inttypes.h> before <xlocale.h> to expose these.
 */
#ifdef _INTTYPES_H_
intmax_t 
strtoimax_l(const char * __restrict, char ** __restrict, int, locale_t);
uintmax_t
strtoumax_l(const char * __restrict, char ** __restrict, int, locale_t);
intmax_t 
wcstoimax_l(const wchar_t * __restrict, wchar_t ** __restrict, int , locale_t);
uintmax_t
wcstoumax_l(const wchar_t * __restrict, wchar_t ** __restrict, int, locale_t);
#endif
/*
 * Extended locale versions of the locale-aware functions from monetary.h.
 *
 * Include <monetary.h> before <xlocale.h> to expose these.
 */
#ifdef _MONETARY_H_
ssize_t strfmon_l(char *, size_t, locale_t, const char *, ...)
#	if __GNUC__ > 2 || __GNUC__ == 2 && __GNUC_MINOR__ >= 7
	__attribute__((__format__ (__strfmon__, 4, 5)))
#	endif
	;
#endif

/*
 * Extended locale versions of the locale-aware functions from stdlib.h.
 *
 * Include <stdlib.h> before <xlocale.h> to expose these.
 */
#ifdef _STDLIB_H_
double	 atof_l(const char *, locale_t);
int	 atoi_l(const char *, locale_t);
long	 atol_l(const char *, locale_t);
long long	 atoll_l(const char *, locale_t);
int	 mblen_l(const char *, size_t, locale_t);
size_t
mbstowcs_l(wchar_t * __restrict, const char * __restrict, size_t, locale_t);
int
mbtowc_l(wchar_t * __restrict, const char * __restrict, size_t, locale_t);
double	 strtod_l(const char *, char **, locale_t);
float	 strtof_l(const char *, char **, locale_t);
long	 strtol_l(const char *, char **, int, locale_t);
long	 double strtold_l(const char *, char **, locale_t);
long long	 strtoll_l(const char *, char **, int, locale_t);
unsigned long	 strtoul_l(const char *, char **, int, locale_t);
unsigned long long	 strtoull_l(const char *, char **, int, locale_t);
size_t
wcstombs_l(char * __restrict, const wchar_t * __restrict, size_t, locale_t);
int	 wctomb_l(char *, wchar_t, locale_t);

int	 ___mb_cur_max_l(locale_t);
#define MB_CUR_MAX_L(x) (___mb_cur_max_l(x))

#endif
/*
 * Extended locale versions of the locale-aware functions from time.h.
 *
 * Include <time.h> before <xlocale.h> to expose these.
 */
#ifdef _TIME_H_
size_t
strftime_l(char * __restrict, size_t, const char * __restrict, const
           struct tm * __restrict, locale_t)
#	if __GNUC__ > 2 || __GNUC__ == 2 && __GNUC_MINOR__ >= 7
	__attribute__((__format__ (__strftime__, 3, 0)))
#	endif
	;
char *
strptime_l(const char * __restrict, const char * __restrict,
           struct tm * __restrict, locale_t);
#endif
#ifdef _LANGINFO_H_
char	*nl_langinfo_l(nl_item, locale_t);
#endif
#ifdef _CTYPE_H_
#include <_xlocale_ctype.h>
#endif
#ifdef _WCTYPE_H_
#define XLOCALE_WCTYPES 1
#include <_xlocale_ctype.h>
#endif

#ifdef _STDIO_H_
int	 fprintf_l(FILE * __restrict, locale_t, const char * __restrict, ...)
		__printflike(3, 4);
int	 fscanf_l(FILE * __restrict, locale_t, const char * __restrict, ...)
		__scanflike(3, 4);
int	 printf_l(locale_t, const char * __restrict, ...) __printflike(2, 3);
int	 scanf_l(locale_t, const char * __restrict, ...) __scanflike(2, 3);
int	 sprintf_l(char * __restrict, locale_t, const char * __restrict, ...)
		__printflike(3, 4);
int	 sscanf_l(const char * __restrict, locale_t, const char * __restrict, ...)
		__scanflike(3, 4);
int	 vfprintf_l(FILE * __restrict, locale_t, const char * __restrict, __va_list)
		__printflike(3, 0);
int	 vprintf_l(locale_t, const char * __restrict, __va_list) __printflike(2, 0);
int	 vsprintf_l(char * __restrict, locale_t, const char * __restrict, __va_list)
		__printflike(3, 0);

int	 snprintf_l(char * __restrict, size_t, locale_t, const char * __restrict,
		...) __printflike(4, 5);
int	 vfscanf_l(FILE * __restrict, locale_t, const char * __restrict, __va_list)
		__scanflike(3, 0);
int	 vscanf_l(locale_t, const char * __restrict, __va_list) __scanflike(2, 0);
int	 vsnprintf_l(char * __restrict, size_t, locale_t, const char * __restrict,
		va_list) __printflike(4, 0);
int	 vsscanf_l(const char * __restrict, locale_t, const char * __restrict,
		va_list) __scanflike(3, 0);
int	 dprintf_l(int, locale_t, const char * __restrict, ...) __printflike(3, 4);
int	 vdprintf_l(int, locale_t, const char * __restrict, __va_list)
		__printflike(3, 0);
int	 asprintf_l(char **, locale_t, const char *, ...) __printflike(3, 4);
int	 vasprintf_l(char **, locale_t, const char *, __va_list) __printflike(3, 0);
#endif
#ifdef _WCHAR_H_
wint_t	 btowc_l(int, locale_t);
wint_t	 fgetwc_l(FILE *, locale_t);
wchar_t *
fgetws_l(wchar_t * __restrict, int, FILE * __restrict, locale_t);
wint_t	 fputwc_l(wchar_t, FILE *, locale_t);
int
fputws_l(const wchar_t * __restrict, FILE * __restrict, locale_t);
int
fwprintf_l(FILE * __restrict, locale_t, const wchar_t * __restrict,
		...);
int
fwscanf_l(FILE * __restrict, locale_t, const wchar_t * __restrict, ...);
wint_t	 getwc_l(FILE *, locale_t);
wint_t	 getwchar_l(locale_t);
size_t
mbrlen_l(const char * __restrict, size_t, mbstate_t * __restrict, locale_t);
size_t
mbrtowc_l(wchar_t * __restrict, const char * __restrict, size_t,
		mbstate_t * __restrict, locale_t);
int	 mbsinit_l(const mbstate_t *, locale_t);
size_t
mbsrtowcs_l(wchar_t * __restrict, const char ** __restrict, size_t,
		mbstate_t * __restrict, locale_t);
wint_t	 putwc_l(wchar_t, FILE *, locale_t);
wint_t	 putwchar_l(wchar_t, locale_t);
int
swprintf_l(wchar_t * __restrict, size_t n, locale_t,
		const wchar_t * __restrict, ...);
int
swscanf_l(const wchar_t * __restrict, locale_t, const wchar_t * __restrict,
		...);
wint_t	 ungetwc_l(wint_t, FILE *, locale_t);
int
vfwprintf_l(FILE * __restrict, locale_t, const wchar_t * __restrict,
		__va_list);
int
vswprintf_l(wchar_t * __restrict, size_t n, locale_t,
		const wchar_t * __restrict, __va_list);
int	 vwprintf_l(locale_t, const wchar_t * __restrict, __va_list);
size_t
wcrtomb_l(char * __restrict, wchar_t, mbstate_t * __restrict, locale_t);
int	 wcscoll_l(const wchar_t *, const wchar_t *, locale_t);
size_t
wcsftime_l(wchar_t * __restrict, size_t, const wchar_t * __restrict,
		const struct tm * __restrict, locale_t);
size_t 
wcsrtombs_l(char * __restrict, const wchar_t ** __restrict, size_t,
		mbstate_t * __restrict, locale_t);
double	 wcstod_l(const wchar_t * __restrict, wchar_t ** __restrict, locale_t);
long
wcstol_l(const wchar_t * __restrict, wchar_t ** __restrict, int, locale_t);
unsigned long
wcstoul_l(const wchar_t * __restrict, wchar_t ** __restrict, int, locale_t);
int	 wcswidth_l(const wchar_t *, size_t, locale_t);
size_t
wcsxfrm_l(wchar_t * __restrict, const wchar_t * __restrict, size_t, locale_t);
int	 wctob_l(wint_t, locale_t);
int	 wcwidth_l(wchar_t, locale_t);
int	 wprintf_l(locale_t, const wchar_t * __restrict, ...);
int	 wscanf_l(locale_t, const wchar_t * __restrict, ...);

int
vfwscanf_l(FILE * __restrict, locale_t, const wchar_t * __restrict,
		__va_list);
int	 vswscanf_l(const wchar_t * __restrict, locale_t,
const wchar_t	*__restrict, __va_list);
int	 vwscanf_l(locale_t, const wchar_t * __restrict, __va_list);
float 	wcstof_l(const wchar_t * __restrict, wchar_t ** __restrict, locale_t);
long double
wcstold_l(const wchar_t * __restrict, wchar_t ** __restrict, locale_t);
long long
wcstoll_l(const wchar_t * __restrict, wchar_t ** __restrict, int, locale_t);
unsigned long long
wcstoull_l(const wchar_t * __restrict, wchar_t ** __restrict, int, locale_t);
size_t
mbsnrtowcs_l(wchar_t * __restrict, const char ** __restrict, size_t, size_t,
		mbstate_t * __restrict, locale_t);
int	 wcscasecmp_l(const wchar_t *, const wchar_t *, locale_t);
int	 wcsncasecmp_l(const wchar_t *, const wchar_t *, size_t, locale_t);
size_t
wcsnrtombs_l(char * __restrict, const wchar_t ** __restrict, size_t, size_t,
		mbstate_t * __restrict, locale_t);

#endif

struct lconv	*localeconv_l(locale_t);
__END_DECLS

#endif
