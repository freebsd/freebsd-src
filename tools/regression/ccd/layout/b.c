/* $FreeBSD: src/tools/regression/ccd/layout/b.c,v 1.1.38.1 2010/12/21 17:10:29 kensmith Exp $ */

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
