/*-
 * Copyright (c) 1998 John D. Polstra
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

#include <sys/cdefs.h>
#ifdef __FBSDID
__FBSDID("$FreeBSD$");
#endif

#include <sys/param.h>
#include <objformat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PATH_OBJFORMAT	"/etc/objformat"

static int copyformat(char *, const char *, size_t);

static const char *known_formats[] = { OBJFORMAT_NAMES, NULL };

static int
copyformat(char *buf, const char *fmt, size_t bufsize)
{
	size_t		 len;

	len = strlen(fmt);
	if (len > bufsize - 1)
		return -1;
	strcpy(buf, fmt);
	return len;
}

int
getobjformat(char *buf, size_t bufsize, int *argcp, char **argv)
{
	const char	 *fmt;
	char		**src, **dst;
	const char	 *env;
	FILE		 *fp;

	fmt = NULL;

	if (argv != NULL) {
		/* 
		 * Scan for arguments setting known formats, e.g., "-elf".
		 * If "argcp" is non-NULL, delete these arguments from the
		 * list and update the argument count in "*argcp".
		 */
		for (dst = src = argv;  *src != NULL;  src++) {
			if ((*src)[0] == '-') {
				const char **p;

				for (p = known_formats;  *p != NULL;  p++)
					if (strcmp(*src + 1, *p) == 0)
						break;
				if (*p != NULL) {
					fmt = *p;
					if (argcp == NULL)  /* Don't delete */
						*dst++ = *src;
				} else
					*dst++ = *src;
			} else
				*dst++ = *src;
		}
		*dst = NULL;
		if (argcp != NULL)
			*argcp -= src - dst;
		if (fmt != NULL)
			return copyformat(buf, fmt, bufsize);
	}

	/* Check the OBJFORMAT environment variable. */
	if ((env = getenv("OBJFORMAT")) != NULL)
		return copyformat(buf, env, bufsize);

	/* Take a look at "/etc/objformat". */
	if ((fp = fopen(PATH_OBJFORMAT, "r")) != NULL) {
		char line[1024];
		int found;
		int len;

		found = len = 0;
		while (fgets(line, sizeof line, fp) != NULL) {
			if (strncmp(line, "OBJFORMAT=", 10) == 0) {
				char *p = &line[10];

				p[strcspn(p, " \t\n")] = '\0';
				len = copyformat(buf, p, bufsize);
				found = 1;
			}
		}
		fclose(fp);
		if (found)
			return len;
	}

	/* As a last resort, use the compiled in default. */
	return copyformat(buf, OBJFORMAT_DEFAULT, bufsize);
}
