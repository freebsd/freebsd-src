/*
 * Copyright (c) 2000 Markus Friedl.  All rights reserved.
 */
#include "includes.h"
#include "xmalloc.h"

#include <resolv.h>

int
uuencode(unsigned char *src, unsigned int srclength,
    char *target, size_t targsize)
{
	return __b64_ntop(src, srclength, target, targsize);
}

int
uudecode(const char *src, unsigned char *target, size_t targsize)
{
	int len;
	char *encoded, *p;

	/* copy the 'readonly' source */
	encoded = xstrdup(src);
	/* skip whitespace and data */
	for (p = encoded; *p == ' ' || *p == '\t'; p++)
		;
	for (; *p != '\0' && *p != ' ' && *p != '\t'; p++)
		;
	/* and remote trailing whitespace because __b64_pton needs this */
	*p = '\0';
	len = __b64_pton(encoded, target, targsize);
	xfree(encoded);
	return len;
}

void
dump_base64(FILE *fp, unsigned char *data, int len)
{
	unsigned char *buf = xmalloc(2*len);
	int i, n;
	n = uuencode(data, len, buf, 2*len);
	for (i = 0; i < n; i++) {
		fprintf(fp, "%c", buf[i]);
		if (i % 70 == 69)
			fprintf(fp, "\n");
	}
	if (i % 70 != 69)
		fprintf(fp, "\n");
	xfree(buf);
}
