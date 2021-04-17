/*
 * Base64 encoding/decoding (RFC1341) - test program
 * Copyright (c) 2005, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"
#include "utils/os.h"
#include "utils/base64.h"

int main(int argc, char *argv[])
{
	FILE *f;
	size_t len, elen;
	unsigned char *buf, *e;

	if (argc != 4) {
		printf("Usage: base64 <encode|decode> <in file> <out file>\n");
		return -1;
	}

	buf = (unsigned char *) os_readfile(argv[2], &len);
	if (buf == NULL)
		return -1;

	if (strcmp(argv[1], "encode") == 0)
		e = (unsigned char *) base64_encode(buf, len, &elen);
	else
		e = base64_decode((const char *) buf, len, &elen);
	if (e == NULL)
		return -2;
	f = fopen(argv[3], "w");
	if (f == NULL)
		return -3;
	fwrite(e, 1, elen, f);
	fclose(f);
	free(e);

	return 0;
}
