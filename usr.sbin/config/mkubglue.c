/*-
 * Copyright (c) 1980 The Regents of the University of California.
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
static char sccsid[] = "@(#)mkubglue.c	5.3 (Berkeley) 4/19/91";
#endif /* not lint */

/*
 * Make the uba interrupt file ubglue.s
 */
#include <stdio.h>
#include "config.h"
#include "y.tab.h"

ubglue()
{
	register FILE *fp;
	register struct device *dp, *mp;

	fp = fopen(path("ubglue.s"), "w");
	if (fp == 0) {
		perror(path("ubglue.s"));
		exit(1);
	}
	for (dp = dtab; dp != 0; dp = dp->d_next) {
		mp = dp->d_conn;
		if (mp != 0 && mp != (struct device *)-1 &&
		    !eq(mp->d_name, "mba")) {
			struct idlst *id, *id2;

			for (id = dp->d_vec; id; id = id->id_next) {
				for (id2 = dp->d_vec; id2; id2 = id2->id_next) {
					if (id2 == id) {
						dump_vec(fp, id->id, dp->d_unit);
						break;
					}
					if (!strcmp(id->id, id2->id))
						break;
				}
			}
		}
	}
	dump_std(fp);
	for (dp = dtab; dp != 0; dp = dp->d_next) {
		mp = dp->d_conn;
		if (mp != 0 && mp != (struct device *)-1 &&
		    !eq(mp->d_name, "mba")) {
			struct idlst *id, *id2;

			for (id = dp->d_vec; id; id = id->id_next) {
				for (id2 = dp->d_vec; id2; id2 = id2->id_next) {
					if (id2 == id) {
						dump_intname(fp, id->id,
							dp->d_unit);
						break;
					}
					if (!strcmp(id->id, id2->id))
						break;
				}
			}
		}
	}
	dump_ctrs(fp);
	(void) fclose(fp);
}

static int cntcnt = 0;		/* number of interrupt counters allocated */

/*
 * print an interrupt vector
 */
dump_vec(fp, vector, number)
	register FILE *fp;
	char *vector;
	int number;
{
	char nbuf[80];
	register char *v = nbuf;

	(void) sprintf(v, "%s%d", vector, number);
	fprintf(fp, "\t.globl\t_X%s\n\t.align\t2\n_X%s:\n\tpushr\t$0x3f\n",
	    v, v);
	fprintf(fp, "\tincl\t_fltintrcnt+(4*%d)\n", cntcnt++);
	if (strncmp(vector, "dzx", 3) == 0)
		fprintf(fp, "\tmovl\t$%d,r0\n\tjmp\tdzdma\n\n", number);
	else if (strncmp(vector, "dpx", 3) == 0)
		fprintf(fp, "\tmovl\t$%d,r0\n\tjmp\tdpxdma\n\n", number);
	else if (strncmp(vector, "dpr", 3) == 0)
		fprintf(fp, "\tmovl\t$%d,r0\n\tjmp\tdprdma\n\n", number);
	else {
		if (strncmp(vector, "uur", 3) == 0) {
			fprintf(fp, "#ifdef UUDMA\n");
			fprintf(fp, "\tmovl\t$%d,r0\n\tjsb\tuudma\n", number);
			fprintf(fp, "#endif\n");
		}
		fprintf(fp, "\tpushl\t$%d\n", number);
		fprintf(fp, "\tcalls\t$1,_%s\n\tpopr\t$0x3f\n", vector);
		fprintf(fp, "\tincl\t_cnt+V_INTR\n\trei\n\n");
	}
}

/*
 * Start the interrupt name table with the names
 * of the standard vectors not on the unibus.
 * The number and order of these names should correspond
 * with the definitions in scb.s.
 */
dump_std(fp)
	register FILE *fp;
{
	fprintf(fp, "\n\t.globl\t_intrnames\n");
	fprintf(fp, "\n\t.globl\t_eintrnames\n");
	fprintf(fp, "\t.data\n");
	fprintf(fp, "_intrnames:\n");
	fprintf(fp, "\t.asciz\t\"clock\"\n");
	fprintf(fp, "\t.asciz\t\"cnr\"\n");
	fprintf(fp, "\t.asciz\t\"cnx\"\n");
	fprintf(fp, "\t.asciz\t\"tur\"\n");
	fprintf(fp, "\t.asciz\t\"tux\"\n");
	fprintf(fp, "\t.asciz\t\"mba0\"\n");
	fprintf(fp, "\t.asciz\t\"mba1\"\n");
	fprintf(fp, "\t.asciz\t\"mba2\"\n");
	fprintf(fp, "\t.asciz\t\"mba3\"\n");
	fprintf(fp, "\t.asciz\t\"uba0\"\n");
	fprintf(fp, "\t.asciz\t\"uba1\"\n");
	fprintf(fp, "\t.asciz\t\"uba2\"\n");
	fprintf(fp, "\t.asciz\t\"uba3\"\n");
#define	I_FIXED		13			/* number of names above */
}

dump_intname(fp, vector, number)
	register FILE *fp;
	char *vector;
	int number;
{
	register char *cp = vector;

	fprintf(fp, "\t.asciz\t\"");
	/*
	 * Skip any "int" or "intr" in the name.
	 */
	while (*cp)
		if (cp[0] == 'i' && cp[1] == 'n' &&  cp[2] == 't') {
			cp += 3;
			if (*cp == 'r')
				cp++;
		} else {
			putc(*cp, fp);
			cp++;
		}
	fprintf(fp, "%d\"\n", number);
}

dump_ctrs(fp)
	register FILE *fp;
{
	fprintf(fp, "_eintrnames:\n");
	fprintf(fp, "\n\t.globl\t_intrcnt\n");
	fprintf(fp, "\n\t.globl\t_eintrcnt\n");
	fprintf(fp, "_intrcnt:\n", I_FIXED);
	fprintf(fp, "\t.space\t4 * %d\n", I_FIXED);
	fprintf(fp, "_fltintrcnt:\n", cntcnt);
	fprintf(fp, "\t.space\t4 * %d\n", cntcnt);
	fprintf(fp, "_eintrcnt:\n\n");
	fprintf(fp, "\t.text\n");
}
