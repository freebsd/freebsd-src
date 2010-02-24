/*-
 * Copyright (c) 2010 Ed Schouten <ed@FreeBSD.org>
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
__FBSDID("$FreeBSD$");

#include <sys/endian.h>
#include <sys/param.h>

#include <err.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <utmpx.h>

#include "../../lib/libc/gen/utxdb.h"

struct outmp {
	char	ut_line[8];
	char	ut_user[16];
	char	ut_host[16];
	int32_t	ut_time;
};

static void
usage(void)
{

	fprintf(stderr, "usage: wtmpcvt input output\n");
	exit(1);
}

static void
outmp_to_futx(const struct outmp *ui, struct futx *uo)
{

	memset(uo, 0, sizeof *uo);
#define	COPY_STRING(field) do {						\
	strncpy(uo->fu_ ## field, ui->ut_ ## field,			\
	    MIN(sizeof uo->fu_ ## field, sizeof ui->ut_ ## field));	\
} while (0)
#define	COPY_LINE_TO_ID() do {						\
	strncpy(uo->fu_id, ui->ut_line,					\
	    MIN(sizeof uo->fu_id, sizeof ui->ut_line));			\
} while (0)
#define	MATCH(field, value)	(strncmp(ui->ut_ ## field, (value),	\
					sizeof(ui->ut_ ## field)) == 0)
	if (MATCH(user, "reboot") && MATCH(line, "~"))
		uo->fu_type = BOOT_TIME;
	else if (MATCH(user, "date") && MATCH(line, "|"))
		uo->fu_type = OLD_TIME;
	else if (MATCH(user, "date") && MATCH(line, "{"))
		uo->fu_type = NEW_TIME;
	else if (MATCH(user, "shutdown") && MATCH(line, "~"))
		uo->fu_type = SHUTDOWN_TIME;
	else if (MATCH(user, "") && MATCH(host, "") && !MATCH(line, "")) {
		uo->fu_type = DEAD_PROCESS;
		COPY_LINE_TO_ID();
	} else if (!MATCH(user, "") && !MATCH(line, "") && ui->ut_time != 0) {
		uo->fu_type = USER_PROCESS;
		COPY_STRING(user);
		COPY_STRING(line);
		COPY_STRING(host);
		COPY_LINE_TO_ID();
	} else {
		uo->fu_type = EMPTY;
		return;
	}
#undef COPY_STRING
#undef COPY_LINE_TO_ID
#undef MATCH

	/* Timestamp conversion. XXX: Assumes host byte order! */
	uo->fu_tv = htobe64((uint64_t)ui->ut_time * 1000000);
}

int
main(int argc, char *argv[])
{
	FILE *in, *out;
	struct outmp ui;
	struct futx uo;
	size_t l;
	uint16_t lo;

	if (argc != 3)
		usage();

	/* Open files. */
	in = fopen(argv[1], "r");
	if (in == NULL)
		err(1, argv[1]);
	out = fopen(argv[2], "w");
	if (out == NULL)
		err(1, argv[2]);

	/* Process entries. */
	while (fread(&ui, sizeof ui, 1, in) == 1) {
		outmp_to_futx(&ui, &uo);
		if (uo.fu_type == EMPTY)
			continue;

		/* Write new entry to output file. */
		for (l = sizeof uo; l > 0 &&
		    ((const char *)&uo)[l - 1] == '\0'; l--);
		lo = htobe16(l);
		fwrite(&lo, sizeof lo, 1, out);
		fwrite(&uo, l, 1, out);
	}

	fclose(in);
	fclose(out);
	return (0);
}
