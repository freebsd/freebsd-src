/* $FreeBSD: src/tools/regression/ccd/layout/a.c,v 1.1 2003/01/17 12:23:44 phk Exp $ */
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
