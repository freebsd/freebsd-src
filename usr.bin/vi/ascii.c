/*-
 * Copyright (c) 1992, 1993
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
static char sccsid[] = "@(#)ascii.c	8.5 (Berkeley) 11/29/93";
#endif /* not lint */

#include <sys/types.h>

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
	{"\xa0", 1}, {"\xa1", 1}, {"\xa2", 1}, {"\xa3", 1},
	{"\xa4", 1}, {"\xa5", 1}, {"\xa6", 1}, {"\xa7", 1},
	{"\xa8", 1}, {"\xa9", 1}, {"\xaa", 1}, {"\xab", 1},
	{"\xac", 1}, {"\xad", 1}, {"\xae", 1}, {"\xaf", 1},
	{"\xb0", 1}, {"\xb1", 1}, {"\xb2", 1}, {"\xb3", 1},
	{"\xb4", 1}, {"\xb5", 1}, {"\xb6", 1}, {"\xb7", 1},
	{"\xb8", 1}, {"\xb9", 1}, {"\xba", 1}, {"\xbb", 1},
	{"\xbc", 1}, {"\xbd", 1}, {"\xbe", 1}, {"\xbf", 1},
	{"\xc0", 1}, {"\xc1", 1}, {"\xc2", 1}, {"\xc3", 1},
	{"\xc4", 1}, {"\xc5", 1}, {"\xc6", 1}, {"\xc7", 1},
	{"\xc8", 1}, {"\xc9", 1}, {"\xca", 1}, {"\xcb", 1},
	{"\xcc", 1}, {"\xcd", 1}, {"\xce", 1}, {"\xcf", 1},
	{"\xd0", 1}, {"\xd1", 1}, {"\xd2", 1}, {"\xd3", 1},
	{"\xd4", 1}, {"\xd5", 1}, {"\xd6", 1}, {"\xd7", 1},
	{"\xd8", 1}, {"\xd9", 1}, {"\xda", 1}, {"\xdb", 1},
	{"\xdc", 1}, {"\xdd", 1}, {"\xde", 1}, {"\xdf", 1},
	{"\xe0", 1}, {"\xe1", 1}, {"\xe2", 1}, {"\xe3", 1},
	{"\xe4", 1}, {"\xe5", 1}, {"\xe6", 1}, {"\xe7", 1},
	{"\xe8", 1}, {"\xe9", 1}, {"\xea", 1}, {"\xeb", 1},
	{"\xec", 1}, {"\xed", 1}, {"\xee", 1}, {"\xef", 1},
	{"\xf0", 1}, {"\xf1", 1}, {"\xf2", 1}, {"\xf3", 1},
	{"\xf4", 1}, {"\xf5", 1}, {"\xf6", 1}, {"\xf7", 1},
	{"\xf8", 1}, {"\xf9", 1}, {"\xfa", 1}, {"\xfb", 1},
	{"\xfc", 1}, {"\xfd", 1}, {"\xfe", 1}, {"\xff", 1},
};

char *
charname(sp, ch)
	SCR *sp;
	ARG_CHAR_T ch;
{
	return (sp->gp->cname[ch & UCHAR_MAX].name);
}
