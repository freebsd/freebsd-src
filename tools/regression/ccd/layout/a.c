/* $FreeBSD: src/tools/regression/ccd/layout/a.c,v 1.1.28.1 2008/10/02 02:57:24 kensmith Exp $ */
#include <unistd.h>

static uint32_t buf[512/4];
main()
{
	u_int u = 0;

	while (1) {
		buf[0] = u++;

		if (512 != write(1, buf, sizeof buf))
			break;
	}
	exit (0);
}
