/* 
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratories.
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
 *	@(#)mkswap.c	8.1 (Berkeley) 6/6/93
 */

#include <sys/param.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"

static int mkoneswap __P((struct config *));

/*
 * Make the various swap*.c files.  Nothing to do for generic swap.
 */
int
mkswap()
{
	register struct config *cf;

	for (cf = allcf; cf != NULL; cf = cf->cf_next)
		if (cf->cf_root != NULL && mkoneswap(cf))
			return (1);
	return (0);
}

static int
mkoneswap(cf)
	register struct config *cf;
{
	register struct nvlist *nv;
	register FILE *fp;
	register char *fname;
	char buf[200];

	(void)sprintf(buf, "swap%s.c", cf->cf_name);
	fname = path(buf);
	if ((fp = fopen(fname, "w")) == NULL) {
		(void)fprintf(stderr, "config: cannot write %s: %s\n",
		    fname, strerror(errno));
		return (1);
	}
	if (fputs("\
#include <sys/param.h>\n\
#include <sys/conf.h>\n\n", fp) < 0)
		goto wrerror;
	nv = cf->cf_root;
	if (fprintf(fp, "dev_t\trootdev = makedev(%d, %d);\t/* %s */\n",
	    major(nv->nv_int), minor(nv->nv_int), nv->nv_str) < 0)
		goto wrerror;
	nv = cf->cf_dump;
	if (fprintf(fp, "dev_t\tdumpdev = makedev(%d, %d);\t/* %s */\n",
	    major(nv->nv_int), minor(nv->nv_int), nv->nv_str) < 0)
		goto wrerror;
	if (fputs("\nstruct\tswdevt swdevt[] = {\n", fp) < 0)
		goto wrerror;
	for (nv = cf->cf_swap; nv != NULL; nv = nv->nv_next)
		if (fprintf(fp, "\t{ makedev(%d, %d),\t0,\t0 },\t/* %s */\n",
		    major(nv->nv_int), minor(nv->nv_int), nv->nv_str) < 0)
			goto wrerror;
	if (fputs("\t{ NODEV, 0, 0 }\n};\n", fp) < 0)
		goto wrerror;
	if (fclose(fp)) {
		fp = NULL;
		goto wrerror;
	}
	free(fname);
	return (0);
wrerror:
	(void)fprintf(stderr, "config: error writing %s: %s\n",
	    fname, strerror(errno));
	if (fp != NULL)
		(void)fclose(fp);
	/* (void)unlink(fname); */
	free(fname);
	return (1);
}
