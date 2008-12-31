/* $FreeBSD: src/tools/regression/ccd/layout/b.c,v 1.1.30.1 2008/11/25 02:59:29 kensmith Exp $ */

#include <unistd.h>
#include <fcntl.h>

static uint32_t buf[512/4];
main()
{
	u_int u = 0;

	while (1) {

		if (512 != read(0, buf, sizeof buf))
			break;

		printf("%u %u\n", u++, buf[0]);
	}
	exit (0);
}
