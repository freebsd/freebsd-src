/*
 * Copyright (c) 1996, 1997 Shigio Yamaguchi. All rights reserved.
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
 *      This product includes software developed by Shigio Yamaguchi.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 *	test.c					 12-Dec-97
 *
 */
#include <sys/stat.h>
#include <sys/types.h>

#include <unistd.h>

#include "test.h"

/*
 * test: 
 *
 *	i)	flags	file flags
 *
 *			"f"	[ -f path ]
 *			"d"	[ -d path ]
 *			"r"	[ -r path ]
 *			"w"	[ -w path ]
 *			"x"	[ -x path ]
 *
 *	i)	path	path
 *	r)		0: no, 1: ok
 *
 * You can specify more than one character. It assumed 'and' test.
 */
int
test(flags, path)
char	*flags;
char	*path;
{
	struct stat sb;
	int	c;

	if (stat(path, &sb) < 0)
		return 0;
	while ((c = *flags++) != NULL) {
		switch (c) {
		case 'f':
	 		if (!S_ISREG(sb.st_mode))
				return 0;
			break;
		case 'd':
	 		if (!S_ISDIR(sb.st_mode))
				return 0;
			break;
		case 'r':
			if (access(path, R_OK) < 0)
				return 0;
			break;
		case 'w':
			if (access(path, W_OK) < 0)
				return 0;
			break;
		case 'x':
			if (access(path, X_OK) < 0)
				return 0;
			break;
		default:
			break;
		}
	}
	return 1;
}
