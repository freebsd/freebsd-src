/*-
 * Copyright (c) 2002 Juli Mallett.
 * Copyright (c) 1988, 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1989 by Berkeley Softworks
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
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
 * @(#)var.c	8.3 (Berkeley) 3/19/94
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "buf.h"
#include "config.h"
#include "str.h"
#include "util.h"
#include "var.h"

/**
 * VarHead
 *	Remove the tail of the given word and place the result in the given
 *	buffer.
 *
 * Results:
 *	TRUE if characters were added to the buffer (a space needs to be
 *	added to the buffer before the next word).
 *
 * Side Effects:
 *	The trimmed word is added to the buffer.
 */
Boolean
VarHead(const char *word, Boolean addSpace, Buffer *buf, void *dummy __unused)
{
	char *slash;

	slash = strrchr(word, '/');
	if (slash != NULL) {
		if (addSpace) {
			Buf_AddByte(buf, (Byte)' ');
		}
		Buf_AppendRange(buf, word, slash);
	} else {
		/*
		 * If no directory part, give . (q.v. the POSIX standard)
		 */
		if (addSpace) {
			Buf_Append(buf, " .");
		} else {
			Buf_AddByte(buf, (Byte)'.');
		}
	}
	return (TRUE);
}

/**
 * VarTail
 *	Remove the head of the given word and place the result in the given
 *	buffer.
 *
 * Results:
 *	TRUE if characters were added to the buffer (a space needs to be
 *	added to the buffer before the next word).
 *
 * Side Effects:
 *	The trimmed word is added to the buffer.
 */
Boolean
VarTail(const char *word, Boolean addSpace, Buffer *buf, void *dummy __unused)
{
	const char *slash;

	if (addSpace) {
		Buf_AddByte (buf, (Byte)' ');
	}

	slash = strrchr(word, '/');
	if (slash != NULL) {
		slash++;
		Buf_Append(buf, slash);
	} else {
		Buf_Append(buf, word);
	}
	return (TRUE);
}

/**
 * VarSuffix
 *	Place the suffix of the given word in the given buffer.
 *
 * Results:
 *	TRUE if characters were added to the buffer (a space needs to be
 *	added to the buffer before the next word).
 *
 * Side Effects:
 *	The suffix from the word is placed in the buffer.
 */
Boolean
VarSuffix(const char *word, Boolean addSpace, Buffer *buf, void *dummy __unused)
{
	const char *dot;

	dot = strrchr(word, '.');
	if (dot != NULL) {
		if (addSpace) {
			Buf_AddByte(buf, (Byte)' ');
		}
		dot++;
		Buf_Append(buf, dot);
		addSpace = TRUE;
	}
	return (addSpace);
}

/**
 * VarRoot
 *	Remove the suffix of the given word and place the result in the
 *	buffer.
 *
 * Results:
 *	TRUE if characters were added to the buffer (a space needs to be
 *	added to the buffer before the next word).
 *
 * Side Effects:
 *	The trimmed word is added to the buffer.
 */
Boolean
VarRoot(const char *word, Boolean addSpace, Buffer *buf, void *dummy __unused)
{
	char *dot;

	if (addSpace) {
		Buf_AddByte(buf, (Byte)' ');
	}

	dot = strrchr(word, '.');
	if (dot != NULL) {
		Buf_AppendRange(buf, word, dot);
	} else {
		Buf_Append(buf, word);
	}
	return (TRUE);
}

/**
 * VarMatch
 *	Place the word in the buffer if it matches the given pattern.
 *	Callback function for VarModify to implement the :M modifier.
 *	A space will be added if requested.  A pattern is supplied
 *	which the word must match.
 *
 * Results:
 *	TRUE if a space should be placed in the buffer before the next
 *	word.
 *
 * Side Effects:
 *	The word may be copied to the buffer.
 */
Boolean
VarMatch(const char *word, Boolean addSpace, Buffer *buf, void *pattern)
{

	if (Str_Match(word, pattern)) {
		if (addSpace) {
			Buf_AddByte(buf, (Byte)' ');
		}
		addSpace = TRUE;
		Buf_Append(buf, word);
	}
	return (addSpace);
}

#ifdef SYSVVARSUB
/**
 * VarSYSVMatch
 *	Place the word in the buffer if it matches the given pattern.
 *	Callback function for VarModify to implement the System V %
 *	modifiers.  A space is added if requested.
 *
 * Results:
 *	TRUE if a space should be placed in the buffer before the next
 *	word.
 *
 * Side Effects:
 *	The word may be copied to the buffer.
 */
Boolean
VarSYSVMatch(const char *word, Boolean addSpace, Buffer *buf, void *patp)
{
	int		len;
	const char	*ptr;
	VarPattern	*pat = (VarPattern *)patp;

	if (addSpace)
		Buf_AddByte(buf, (Byte)' ');

	addSpace = TRUE;

	if ((ptr = Str_SYSVMatch(word, Buf_Data(pat->lhs), &len)) != NULL)
		Str_SYSVSubst(buf, Buf_Data(pat->rhs), ptr, len);
	else
		Buf_Append(buf, word);

	return (addSpace);
}
#endif

/**
 * VarNoMatch
 *	Place the word in the buffer if it doesn't match the given pattern.
 *	Callback function for VarModify to implement the :N modifier.  A
 *	space is added if requested.
 *
 * Results:
 *	TRUE if a space should be placed in the buffer before the next
 *	word.
 *
 * Side Effects:
 *	The word may be copied to the buffer.
 */
Boolean
VarNoMatch(const char *word, Boolean addSpace, Buffer *buf, void *pattern)
{

	if (!Str_Match(word, pattern)) {
		if (addSpace) {
			Buf_AddByte(buf, (Byte)' ');
		}
		addSpace = TRUE;
		Buf_Append(buf, word);
	}
	return (addSpace);
}

/**
 * VarSubstitute
 *	Perform a string-substitution on the given word, placing the
 *	result in the passed buffer.  A space is added if requested.
 *
 * Results:
 *	TRUE if a space is needed before more characters are added.
 */
Boolean
VarSubstitute(const char *word, Boolean addSpace, Buffer *buf, void *patternp)
{
	size_t		wordLen;	/* Length of word */
	const char	*cp;		/* General pointer */
	VarPattern	*pattern = patternp;

	wordLen = strlen(word);
	if (1) { /* substitute in each word of the variable */
		/*
		 * Break substitution down into simple anchored cases
		 * and if none of them fits, perform the general substitution
		 * case.
		 */
		if ((pattern->flags & VAR_MATCH_START) &&
		   (strncmp(word, Buf_Data(pattern->lhs),
		    Buf_Size(pattern->lhs)) == 0)) {
			/*
			 * Anchored at start and beginning of word matches
			 * pattern.
			 */
			if ((pattern->flags & VAR_MATCH_END) &&
			    (wordLen == Buf_Size(pattern->lhs))) {
				/*
				 * Also anchored at end and matches to the end
				 * (word is same length as pattern) add space
				 * and rhs only if rhs is non-null.
				 */
				if (Buf_Size(pattern->rhs) != 0) {
					if (addSpace) {
						Buf_AddByte(buf, (Byte)' ');
					}
					addSpace = TRUE;
					Buf_AppendBuf(buf, pattern->rhs);
				}

			} else if (pattern->flags & VAR_MATCH_END) {
				/*
				 * Doesn't match to end -- copy word wholesale
				 */
				goto nosub;

			} else {
				/*
				 * Matches at start but need to copy in
				 * trailing characters.
				 */
				if ((Buf_Size(pattern->rhs) + wordLen -
				    Buf_Size(pattern->lhs)) != 0) {
					if (addSpace) {
						Buf_AddByte(buf, (Byte)' ');
					}
					addSpace = TRUE;
				}
				Buf_AppendBuf(buf, pattern->rhs);
				Buf_AddBytes(buf, wordLen -
				    Buf_Size(pattern->lhs),
				    (word + Buf_Size(pattern->lhs)));
			}

		} else if (pattern->flags & VAR_MATCH_START) {
			/*
			 * Had to match at start of word and didn't -- copy
			 * whole word.
			 */
			goto nosub;

		} else if (pattern->flags & VAR_MATCH_END) {
			/*
			 * Anchored at end, Find only place match could occur
			 * (leftLen characters from the end of the word) and
			 * see if it does. Note that because the $ will be
			 * left at the end of the lhs, we have to use strncmp.
			 */
			cp = word + (wordLen - Buf_Size(pattern->lhs));
			if ((cp >= word) && (strncmp(cp, Buf_Data(pattern->lhs),
			    Buf_Size(pattern->lhs)) == 0)) {
				/*
				 * Match found. If we will place characters in
				 * the buffer, add a space before hand as
				 * indicated by addSpace, then stuff in the
				 * initial, unmatched part of the word followed
				 * by the right-hand-side.
				 */
				if ((cp - word) + Buf_Size(pattern->rhs) != 0) {
					if (addSpace) {
						Buf_AddByte(buf, (Byte)' ');
					}
					addSpace = TRUE;
				}
				Buf_AppendRange(buf, word, cp);
				Buf_AppendBuf(buf, pattern->rhs);

			} else {
				/*
				 * Had to match at end and didn't. Copy entire
				 * word.
				 */
				goto nosub;
			}
		} else {
			/*
			 * Pattern is unanchored: search for the pattern in the
			 * word using strstr(3), copying unmatched portions and
			 * the right-hand-side for each match found, handling
			 * non-global substitutions correctly, etc. When the
			 * loop is done, any remaining part of the word (word
			 * and wordLen are adjusted accordingly through the
			 * loop) is copied straight into the buffer.
			 * addSpace is set FALSE as soon as a space is added
			 * to the buffer.
			 */
			Boolean done;
			size_t origSize;

			done = FALSE;
			origSize = Buf_Size(buf);
			while (!done) {
				cp = strstr(word, Buf_Data(pattern->lhs));
				if (cp != NULL) {
					if (addSpace && (((cp - word) +
					    Buf_Size(pattern->rhs)) != 0)) {
						Buf_AddByte(buf, (Byte)' ');
						addSpace = FALSE;
					}
					Buf_AppendRange(buf, word, cp);
					Buf_AppendBuf(buf, pattern->rhs);
					wordLen -= (cp - word) +
					    Buf_Size(pattern->lhs);
					word = cp + Buf_Size(pattern->lhs);
					if (wordLen == 0 || (pattern->flags &
					    VAR_SUB_GLOBAL) == 0) {
						done = TRUE;
					}
				} else {
					done = TRUE;
				}
			}
			if (wordLen != 0) {
				if (addSpace) {
					Buf_AddByte(buf, (Byte)' ');
				}
				Buf_AddBytes(buf, wordLen, (const Byte *)word);
			}

			/*
			 * If added characters to the buffer, need to add a
			 * space before we add any more. If we didn't add any,
			 * just return the previous value of addSpace.
			 */
			return ((Buf_Size(buf) != origSize) || addSpace);
		}
		/*
		 * Common code for anchored substitutions:
		 * addSpace was set TRUE if characters were added to the buffer.
		 */
		return (addSpace);
	}
  nosub:
	if (addSpace) {
		Buf_AddByte(buf, (Byte)' ');
	}
	Buf_AddBytes(buf, wordLen, (const Byte *)word);
	return (TRUE);
}

/**
 * VarRESubstitute
 *	Perform a regex substitution on the given word, placing the
 *	result in the passed buffer.  A space is added if requested.
 *
 * Results:
 *	TRUE if a space is needed before more characters are added.
 */
Boolean
VarRESubstitute(const char *word, Boolean addSpace, Buffer *buf, void *patternp)
{
	VarPattern	*pat;
	int		xrv;
	const char	*wp;
	char		*rp;
	int		added;
	int		flags = 0;

#define	MAYBE_ADD_SPACE()			\
	if (addSpace && !added)			\
		Buf_AddByte(buf, (Byte)' ');	\
	added = 1

	added = 0;
	wp = word;
	pat = patternp;

	if ((pat->flags & (VAR_SUB_ONE | VAR_SUB_MATCHED)) ==
	    (VAR_SUB_ONE | VAR_SUB_MATCHED)) {
		xrv = REG_NOMATCH;
	} else {
  tryagain:
		xrv = regexec(&pat->re, wp, pat->nsub, pat->matches, flags);
	}

	switch (xrv) {
	  case 0:
		pat->flags |= VAR_SUB_MATCHED;
		if (pat->matches[0].rm_so > 0) {
			MAYBE_ADD_SPACE();
			Buf_AddBytes(buf, pat->matches[0].rm_so,
			    (const Byte *)wp);
		}

		for (rp = Buf_Data(pat->rhs); *rp; rp++) {
			if ((*rp == '\\') && ((rp[1] == '&') || (rp[1] == '\\'))) {
				MAYBE_ADD_SPACE();
				Buf_AddByte(buf, (Byte)rp[1]);
				rp++;

			} else if ((*rp == '&') ||
			    ((*rp == '\\') && isdigit((unsigned char)rp[1]))) {
				int	n;
				const char *subbuf;
				int	sublen;
				char	errstr[3];

				if (*rp == '&') {
					n = 0;
					errstr[0] = '&';
					errstr[1] = '\0';
				} else {
					n = rp[1] - '0';
					errstr[0] = '\\';
					errstr[1] = rp[1];
					errstr[2] = '\0';
					rp++;
				}

				if (n > pat->nsub) {
					Error("No subexpression %s",
					    &errstr[0]);
					subbuf = "";
					sublen = 0;

				} else if ((pat->matches[n].rm_so == -1) &&
				    (pat->matches[n].rm_eo == -1)) {
					Error("No match for subexpression %s",
					    &errstr[0]);
					subbuf = "";
					sublen = 0;

				} else {
					subbuf = wp + pat->matches[n].rm_so;
					sublen = pat->matches[n].rm_eo -
					    pat->matches[n].rm_so;
				}

				if (sublen > 0) {
					MAYBE_ADD_SPACE();
					Buf_AddBytes(buf, sublen,
					    (const Byte *)subbuf);
				}
			} else {
				MAYBE_ADD_SPACE();
				Buf_AddByte(buf, (Byte)*rp);
			}
		}
		wp += pat->matches[0].rm_eo;
		if (pat->flags & VAR_SUB_GLOBAL) {
			flags |= REG_NOTBOL;
			if (pat->matches[0].rm_so == 0 &&
			    pat->matches[0].rm_eo == 0) {
				MAYBE_ADD_SPACE();
				Buf_AddByte(buf, (Byte)*wp);
				wp++;
			}
			if (*wp)
				goto tryagain;
		}
		if (*wp) {
			MAYBE_ADD_SPACE();
			Buf_Append(buf, wp);
		}
		break;

	  default:
		VarREError(xrv, &pat->re, "Unexpected regex error");
		/* fall through */

	  case REG_NOMATCH:
		if (*wp) {
			MAYBE_ADD_SPACE();
			Buf_Append(buf, wp);
		}
		break;
	}
	return (addSpace || added);
}
