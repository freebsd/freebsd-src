/*-
 * Copyright (c) 1992, 1993, 1994
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
 */

#ifndef lint
static char sccsid[] = "@(#)ascii.c	8.7 (Berkeley) 3/14/94";
#endif /* not lint */

#include <sys/types.h>
#include <queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <termios.h>

#include <db.h>
#include <regex.h>

#include "vi.h"

CHNAME const asciiname[UCHAR_MAX + 1] = {
	  {"^@", 2},   {"^A", 2},   {"^B", 2},   {"^C", 2},
	  {"^D", 2},   {"^E", 2},   {"^F", 2},   {"^G", 2},
	  {"^H", 2},   {"^I", 2},   {"^J", 2},   {"^K", 2},
	  {"^L", 2},   {"^M", 2},   {"^N", 2},   {"^O", 2},
	  {"^P", 2},   {"^Q", 2},   {"^R", 2},   {"^S", 2},
	  {"^T", 2},   {"^U", 2},   {"^V", 2},   {"^W", 2},
	  {"^X", 2},   {"^Y", 2},   {"^Z", 2},   {"^[", 2},
	 {"^\\", 2},   {"^]", 2},   {"^^", 2},   {"^_", 2},
	   {" ", 1},    {"!", 1},   {"\"", 1},    {"#", 1},
	   {"$", 1},    {"%", 1},    {"&", 1},    {"'", 1},
	   {"(", 1},    {")", 1},    {"*", 1},    {"+", 1},
	   {",", 1},    {"-", 1},    {".", 1},    {"/", 1},
	   {"0", 1},    {"1", 1},    {"2", 1},    {"3", 1},
	   {"4", 1},    {"5", 1},    {"6", 1},    {"7", 1},
	   {"8", 1},    {"9", 1},    {":", 1},    {";", 1},
	   {"<", 1},    {"=", 1},    {">", 1},    {"?", 1},
	   {"@", 1},    {"A", 1},    {"B", 1},    {"C", 1},
	   {"D", 1},    {"E", 1},    {"F", 1},    {"G", 1},
	   {"H", 1},    {"I", 1},    {"J", 1},    {"K", 1},
	   {"L", 1},    {"M", 1},    {"N", 1},    {"O", 1},
	   {"P", 1},    {"Q", 1},    {"R", 1},    {"S", 1},
	   {"T", 1},    {"U", 1},    {"V", 1},    {"W", 1},
	   {"X", 1},    {"Y", 1},    {"Z", 1},    {"[", 1},
	  {"\\", 1},    {"]", 1},    {"^", 1},    {"_", 1},
	   {"`", 1},    {"a", 1},    {"b", 1},    {"c", 1},
	   {"d", 1},    {"e", 1},    {"f", 1},    {"g", 1},
	   {"h", 1},    {"i", 1},    {"j", 1},    {"k", 1},
	   {"l", 1},    {"m", 1},    {"n", 1},    {"o", 1},
	   {"p", 1},    {"q", 1},    {"r", 1},    {"s", 1},
	   {"t", 1},    {"u", 1},    {"v", 1},    {"w", 1},
	   {"x", 1},    {"y", 1},    {"z", 1},    {"{", 1},
	   {"|", 1},    {"}", 1},    {"~", 1},   {"^?", 2},
	{"0x80", 4}, {"0x81", 4}, {"0x82", 4}, {"0x83", 4},
	{"0x84", 4}, {"0x85", 4}, {"0x86", 4}, {"0x87", 4},
	{"0x88", 4}, {"0x89", 4}, {"0x8a", 4}, {"0x8b", 4},
	{"0x8c", 4}, {"0x8d", 4}, {"0x8e", 4}, {"0x8f", 4},
	{"0x90", 4}, {"0x91", 4}, {"0x92", 4}, {"0x93", 4},
	{"0x94", 4}, {"0x95", 4}, {"0x96", 4}, {"0x97", 4},
	{"0x98", 4}, {"0x99", 4}, {"0x9a", 4}, {"0x9b", 4},
	{"0x9c", 4}, {"0x9d", 4}, {"0x9e", 4}, {"0x9f", 4},
	{"0xa0", 4}, {"0xa1", 4}, {"0xa2", 4}, {"0xa3", 4},
	{"0xa4", 4}, {"0xa5", 4}, {"0xa6", 4}, {"0xa7", 4},
	{"0xa8", 4}, {"0xa9", 4}, {"0xaa", 4}, {"0xab", 4},
	{"0xac", 4}, {"0xad", 4}, {"0xae", 4}, {"0xaf", 4},
	{"0xb0", 4}, {"0xb1", 4}, {"0xb2", 4}, {"0xb3", 4},
	{"0xb4", 4}, {"0xb5", 4}, {"0xb6", 4}, {"0xb7", 4},
	{"0xb8", 4}, {"0xb9", 4}, {"0xba", 4}, {"0xbb", 4},
	{"0xbc", 4}, {"0xbd", 4}, {"0xbe", 4}, {"0xbf", 4},
	{"0xc0", 4}, {"0xc1", 4}, {"0xc2", 4}, {"0xc3", 4},
	{"0xc4", 4}, {"0xc5", 4}, {"0xc6", 4}, {"0xc7", 4},
	{"0xc8", 4}, {"0xc9", 4}, {"0xca", 4}, {"0xcb", 4},
	{"0xcc", 4}, {"0xcd", 4}, {"0xce", 4}, {"0xcf", 4},
	{"0xd0", 4}, {"0xd1", 4}, {"0xd2", 4}, {"0xd3", 4},
	{"0xd4", 4}, {"0xd5", 4}, {"0xd6", 4}, {"0xd7", 4},
	{"0xd8", 4}, {"0xd9", 4}, {"0xda", 4}, {"0xdb", 4},
	{"0xdc", 4}, {"0xdd", 4}, {"0xde", 4}, {"0xdf", 4},
	{"0xe0", 4}, {"0xe1", 4}, {"0xe2", 4}, {"0xe3", 4},
	{"0xe4", 4}, {"0xe5", 4}, {"0xe6", 4}, {"0xe7", 4},
	{"0xe8", 4}, {"0xe9", 4}, {"0xea", 4}, {"0xeb", 4},
	{"0xec", 4}, {"0xed", 4}, {"0xee", 4}, {"0xef", 4},
	{"0xf0", 4}, {"0xf1", 4}, {"0xf2", 4}, {"0xf3", 4},
	{"0xf4", 4}, {"0xf5", 4}, {"0xf6", 4}, {"0xf7", 4},
	{"0xf8", 4}, {"0xf9", 4}, {"0xfa", 4}, {"0xfb", 4},
	{"0xfc", 4}, {"0xfd", 4}, {"0xfe", 4}, {"0xff", 4},
};

char *
charname(sp, ch)
	SCR *sp;
	ARG_CHAR_T ch;
{
	return (sp->gp->cname[ch & UCHAR_MAX].name);
}
