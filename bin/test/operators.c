/*-
 * Copyright (c) 1993, 1994
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
 *	$FreeBSD$
 */

#ifndef lint
static char const sccsid[] = "@(#)operators.c	8.3 (Berkeley) 4/2/94";
#endif /* not lint */

/*
 * Operators used in the test command.
 */

#include <stdio.h>

#include "operators.h"

const char *const unary_op[] = {
      "!",
      "-b",
      "-c",
      "-d",
      "-e",
      "-f",
      "-g",
      "-h",
      "-k",
      "-n",
      "-p",
      "-r",
      "-s",
      "-t",
      "-u",
      "-w",
      "-x",
      "-z",
      NULL
};

const char *const binary_op[] = {
      "-o",
      "|",
      "-a",
      "&",
      "=",
      "!=",
      "-eq",
      "-ne",
      "-gt",
      "-lt",
      "-le",
      "-ge",
      NULL
};

const char *const andor_op[] = {
	"-o",
	"|",
	"-a",
	"&",
	NULL
};



const char op_priority[] = {
      3,
      12,
      12,
      12,
      12,
      12,
      12,
      12,
      12,
      12,
      12,
      12,
      12,
      12,
      12,
      12,
      12,
      12,
      1,
      1,
      2,
      2,
      4,
      4,
      4,
      4,
      4,
      4,
      4,
      4,
};

const char op_argflag[] = {
      0,
      OP_FILE,
      OP_FILE,
      OP_FILE,
      OP_FILE,
      OP_FILE,
      OP_FILE,
      OP_FILE,
      OP_FILE,
      OP_STRING,
      OP_FILE,
      OP_FILE,
      OP_FILE,
      OP_INT,
      OP_FILE,
      OP_FILE,
      OP_FILE,
      OP_STRING,
      0,
      0,
      0,
      0,
      OP_STRING,
      OP_STRING,
      OP_INT,
      OP_INT,
      OP_INT,
      OP_INT,
      OP_INT,
      OP_INT,
};
