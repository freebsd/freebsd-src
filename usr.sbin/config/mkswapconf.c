/*
 * Copyright (c) 1980, 1993
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
#if 0
static char sccsid[] = "@(#)mkswapconf.c	8.1 (Berkeley) 6/6/93";
#endif
static const char rcsid[] =
	"$Id: mkswapconf.c,v 1.19 1999/04/24 18:59:19 peter Exp $";
#endif /* not lint */

/*
 * Build a swap configuration file.
 */
#include <err.h>
#include <unistd.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "config.h"

void
swapconf()
{
	FILE *fp;
	char  newswapname[80];
	char  swapname[80];
	register struct file_list *swap;
	register struct file_list *fl;

	fl = conf_list;
	while (fl) {
		if (fl->f_type != SYSTEMSPEC) {
			fl = fl->f_next;
			continue;
		}
		break;
	}

	(void) snprintf(swapname, sizeof(swapname), "swapkernel.c");
	(void) snprintf(newswapname, sizeof(newswapname), "swapkernel.c.new");
	fp = fopen(path(newswapname), "w");
	if (fp == 0)
		err(1, "%s", path(newswapname));
	fprintf(fp, "#include <sys/param.h>\n");
	fprintf(fp, "#include <sys/conf.h>\n");
	fprintf(fp, "\n");
	fprintf(fp, "char\t*rootdevname = \"%s\";\n", fl->f_rootdev);
	fclose(fp);
	moveifchanged(path(newswapname), path(swapname));
	return;
}

