/*-
 * Copyright (c) 2003 Tim J. Robbins
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 */

/*
 * Test program for wordexp() and wordfree() as specified by
 * IEEE Std. 1003.1-2001.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wordexp.h>

int
main(int argc, char *argv[])
{
	wordexp_t we;
	int r;

	/* Test that the macros are there. */
	(void)(WRDE_APPEND + WRDE_DOOFS + WRDE_NOCMD + WRDE_REUSE +
	    WRDE_SHOWERR + WRDE_UNDEF);
	(void)(WRDE_BADCHAR + WRDE_BADVAL + WRDE_CMDSUB + WRDE_NOSPACE +
	    WRDE_SYNTAX);

	/* Simple test. */
	r = wordexp("hello world", &we, 0);
	assert(r == 0);
	assert(we.we_wordc == 2);
	assert(strcmp(we.we_wordv[0], "hello") == 0);
	assert(strcmp(we.we_wordv[1], "world") == 0);
	assert(we.we_wordv[2] == NULL);
	wordfree(&we);

	/* WRDE_DOOFS */
	we.we_offs = 3;
	r = wordexp("hello world", &we, WRDE_DOOFS);
	assert(r == 0);
	assert(we.we_wordc == 2);
	assert(we.we_wordv[0] == NULL);
	assert(we.we_wordv[1] == NULL);
	assert(we.we_wordv[2] == NULL);
	assert(strcmp(we.we_wordv[3], "hello") == 0);
	assert(strcmp(we.we_wordv[4], "world") == 0);
	assert(we.we_wordv[5] == NULL);
	wordfree(&we);

	/* WRDE_REUSE */
	r = wordexp("hello world", &we, 0);
	r = wordexp("hello world", &we, WRDE_REUSE);
	assert(r == 0);
	assert(we.we_wordc == 2);
	assert(strcmp(we.we_wordv[0], "hello") == 0);
	assert(strcmp(we.we_wordv[1], "world") == 0);
	assert(we.we_wordv[2] == NULL);
	wordfree(&we);

	/* WRDE_APPEND */
	r = wordexp("this is", &we, 0);
	assert(r == 0);
	r = wordexp("a test", &we, WRDE_APPEND);
	assert(r == 0);
	assert(we.we_wordc == 4);
	assert(strcmp(we.we_wordv[0], "this") == 0);
	assert(strcmp(we.we_wordv[1], "is") == 0);
	assert(strcmp(we.we_wordv[2], "a") == 0);
	assert(strcmp(we.we_wordv[3], "test") == 0);
	assert(we.we_wordv[4] == NULL);
	wordfree(&we);

	/* WRDE_DOOFS + WRDE_APPEND */
	we.we_offs = 2;
	r = wordexp("this is", &we, WRDE_DOOFS);
	assert(r == 0);
	r = wordexp("a test", &we, WRDE_APPEND|WRDE_DOOFS);
	assert(r == 0);
	r = wordexp("of wordexp", &we, WRDE_APPEND|WRDE_DOOFS);
	assert(r == 0);
	assert(we.we_wordc == 6);
	assert(we.we_wordv[0] == NULL);
	assert(we.we_wordv[1] == NULL);
	assert(strcmp(we.we_wordv[2], "this") == 0);
	assert(strcmp(we.we_wordv[3], "is") == 0);
	assert(strcmp(we.we_wordv[4], "a") == 0);
	assert(strcmp(we.we_wordv[5], "test") == 0);
	assert(strcmp(we.we_wordv[6], "of") == 0);
	assert(strcmp(we.we_wordv[7], "wordexp") == 0);
	assert(we.we_wordv[8] == NULL);
	wordfree(&we);

	/* WRDE_UNDEF */
	r = wordexp("${dont_set_me}", &we, WRDE_UNDEF);
	assert(r == WRDE_BADVAL);

	/* WRDE_NOCMD */
	r = wordexp("`date`", &we, WRDE_NOCMD);
	assert(r == WRDE_CMDSUB);
	r = wordexp("\"`date`\"", &we, WRDE_NOCMD);
	assert(r == WRDE_CMDSUB);
	r = wordexp("$(date)", &we, WRDE_NOCMD);
	assert(r == WRDE_CMDSUB);
	r = wordexp("\"$(date)\"", &we, WRDE_NOCMD);
	assert(r == WRDE_CMDSUB);
	r = wordexp("$((3+5))", &we, WRDE_NOCMD);
	assert(r == 0);
	r = wordexp("\\$\\(date\\)", &we, WRDE_NOCMD|WRDE_REUSE);
	assert(r == 0);
	r = wordexp("'`date`'", &we, WRDE_NOCMD|WRDE_REUSE);
	assert(r == 0);
	r = wordexp("'$(date)'", &we, WRDE_NOCMD|WRDE_REUSE);
	assert(r == 0);
	wordfree(&we);

	/* WRDE_BADCHAR */
	r = wordexp("'\n|&;<>(){}'", &we, 0);
	assert(r == 0);
	r = wordexp("\"\n|&;<>(){}\"", &we, WRDE_REUSE);
	assert(r == 0);
	r = wordexp("\\\n\\|\\&\\;\\<\\>\\(\\)\\{\\}", &we, WRDE_REUSE);
	assert(r == 0);
	wordfree(&we);
	r = wordexp("test \n test", &we, 0);
	assert(r == WRDE_BADCHAR);
	r = wordexp("test | test", &we, 0);
	assert(r == WRDE_BADCHAR);
	r = wordexp("test & test", &we, 0);
	assert(r == WRDE_BADCHAR);
	r = wordexp("test ; test", &we, 0);
	assert(r == WRDE_BADCHAR);
	r = wordexp("test > test", &we, 0);
	assert(r == WRDE_BADCHAR);
	r = wordexp("test < test", &we, 0);
	assert(r == WRDE_BADCHAR);
	r = wordexp("test ( test", &we, 0);
	assert(r == WRDE_BADCHAR);
	r = wordexp("test ) test", &we, 0);
	assert(r == WRDE_BADCHAR);
	r = wordexp("test { test", &we, 0);
	assert(r == WRDE_BADCHAR);
	r = wordexp("test } test", &we, 0);
	assert(r == WRDE_BADCHAR);

	printf("PASS wordexp()\n");
	printf("PASS wordfree()\n");

	return (0);
}
