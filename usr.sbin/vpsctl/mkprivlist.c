/*-
 * Copyright (c) 2009-2013 Klaus P. Ohrhallinger <k@7he.at>
 * All rights reserved.
 *
 * Development of this software was partly funded by:
 *    TransIP.nl <http://www.transip.nl/>
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
 *
 * $Id: mkprivlist.c 133 2013-04-20 17:06:57Z klaus $
 *
 */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#define _WITH_GETLINE
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <regex.h>

int main(int argc, char **argv, char **envp)
{
	FILE *fp, *out1, *out2;
	char *line = NULL;
	size_t linecap = 0;
	ssize_t linelen;
	regex_t expr;
	regmatch_t results[4];
	char name[0x100], val[0x100];

	if (argc < 2) {
		fprintf(stderr, "usage: %s path/to/sys/priv.h\n", argv[0]);
		return (-1);
	}

	if ((out1 = fopen("priv_ntos.c", "w")) == NULL) {
		fprintf(stderr, "fopen(): %s\n", strerror(errno));
		return (-1);
	}

	if ((out2 = fopen("priv_ston.c", "w")) == NULL) {
		fprintf(stderr, "fopen(): %s\n", strerror(errno));
		return (-1);
	}

	if ((fp = fopen(argv[1], "r")) == NULL) {
		fprintf(stderr, "fopen([%s]): %s\n",
			argv[1], strerror(errno));
		return (-1);
	}

	if (regcomp(&expr, "^#define[ \t]+(PRIV_[^ \t]+)[ \t]+([0-9]+)", REG_EXTENDED)) {
		fprintf(stderr, "regcomp error\n");
		return (-1);
	}

	fprintf(out1,
		"/* AUTOMATICALLY GENERATED FILE */\n"
		"\n"
		"#include <sys/priv.h>\n"
		"\n"
		"const char * priv_ntos(int priv);\n"
		"\n"
		"const char *\n"
		"priv_ntos(int priv)\n"
		"{\n"
		"\n"
		"    switch (priv) {\n"
		);

	fprintf(out2,
		"/* AUTOMATICALLY GENERATED FILE */\n"
		"\n"
		"#include <sys/priv.h>\n"
		"#include <string.h>\n"
		"\n"
		"int priv_ston(const char *str);\n"
		"\n"
		"int\n"
		"priv_ston(const char *str)\n"
		"{\n"
		"\n"
		);

	while ((linelen = getline(&line, &linecap, fp)) > 0) {
		//fwrite(line, linelen, 1, stdout);

		if (regexec(&expr, line, 3, results, 0))
			continue;

		/*
		printf("results: [%d/%d] [%d/%d] [%d/%d]\n",
			results[0].rm_so, results[0].rm_eo,
			results[1].rm_so, results[1].rm_eo,
			results[2].rm_so, results[2].rm_eo);
		*/

		memcpy(name, line + results[1].rm_so, results[1].rm_eo - results[1].rm_so);
		name[results[1].rm_eo - results[1].rm_so] = '\0';

		memcpy(val, line + results[2].rm_so, results[2].rm_eo - results[2].rm_so);
		val[results[2].rm_eo - results[2].rm_so] = '\0';

		//printf("name=[%s] val=[%s]\n", name, val);

		fprintf(out1, "    case %s: return (\"%s\");\n",
			val, name);
		fprintf(out2, "    if (strcmp(str, \"%s\") == 0) return (%s);\n",
			name, val);
	}

	fclose(fp);
	regfree(&expr);

	fprintf(out2,
		"    return (0);\n"
		"}\n"
		);

	fprintf(out1,
		"    default: return (\"unknown\");\n"
		"    }\n"
		"}\n"
		);

	fclose(out2);
	fclose(out1);

	return (0);
}

/* EOF */
