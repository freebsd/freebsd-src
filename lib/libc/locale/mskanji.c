/*
 *    ja_JP.SJIS locale table for BSD4.4/rune
 *    version 1.0
 *    (C) Sin'ichiro MIYATANI / Phase One, Inc
 *    May 12, 1995
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
 *      This product includes software developed by Phase One, Inc.
 * 4. The name of Phase One, Inc. may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */  

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)mskanji.c	1.0 (Phase One) 5/5/95";
#endif /* LIBC_SCCS and not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>

#include <rune.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

rune_t	_MSKanji_sgetrune(const char *, size_t, char const **);
int	_MSKanji_sputrune(rune_t, char *, size_t, char **);

int
_MSKanji_init(rl)
	_RuneLocale *rl;
{
	rl->sgetrune = _MSKanji_sgetrune;
	rl->sputrune = _MSKanji_sputrune;

	_CurrentRuneLocale = rl;
	__mb_cur_max = 2;
	return (0);
}

rune_t
_MSKanji_sgetrune(string, n, result)
	const char *string;
	size_t n;
	char const **result;
{
	rune_t rune = 0;

	if (n < 1 ) {
		rune = _INVALID_RUNE;
	} else {
		rune = *( string++ ) & 0xff;
		if ( ( rune > 0x80 && rune < 0xa0 )
		|| ( rune >= 0xe0 && rune < 0xfa ) ) {
			if ( n < 2 ) {
				rune = (rune_t)_INVALID_RUNE;
				--string;
			} else {
				rune = ( rune << 8 ) | ( *( string++ ) & 0xff );
			}
		}
	}
	if (result) *result = string;
	return rune;
}

int
_MSKanji_sputrune(c, string, n, result)
	rune_t c;
	char *string, **result;
	size_t n;
{
	int	len, i;

	len = ( c > 0x100 ) ? 2 : 1;
	if ( n < len ) {
		if ( result ) *result = (char *) 0;
	} else {
		if ( result ) *result = string + len;
		for ( i = len; i-- > 0; ) {
			*( string++ ) = c >> ( i << 3 );
		}
	}
	return len;
}
