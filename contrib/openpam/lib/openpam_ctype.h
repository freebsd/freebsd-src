/*-
 * Copyright (c) 2012 Dag-Erling Smørgrav
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: openpam_ctype.h 578 2012-04-06 00:45:59Z des $
 */

#ifndef OPENPAM_CTYPE_H_INCLUDED
#define OPENPAM_CTYPE_H_INCLUDED

/*
 * Evaluates to non-zero if the argument is a linear whitespace character.
 * For the purposes of this macro, the definition of linear whitespace is
 * extended to include the form feed and carraige return characters.
 */
#define is_lws(ch)				\
	(ch == ' ' || ch == '\t' || ch == '\f' || ch == '\r')

/*
 * Evaluates to non-zero if the argument is a whitespace character.
 */
#define is_ws(ch)				\
	(is_lws(ch) || ch == '\n')

/*
 * Evaluates to non-zero if the argument is a printable ASCII character.
 * Assumes that the execution character set is a superset of ASCII.
 */
#define is_p(ch) \
	(ch >= '!' && ch <= '~')

/*
 * Returns non-zero if the argument belongs to the POSIX Portable Filename
 * Character Set.  Assumes that the execution character set is a superset
 * of ASCII.
 */
#define is_pfcs(ch)				\
	((ch >= '0' && ch <= '9') ||		\
	 (ch >= 'A' && ch <= 'Z') ||		\
	 (ch >= 'a' && ch <= 'z') ||		\
	 ch == '.' || ch == '_' || ch == '-')

#endif
