/*
 * $Id: keynames.c,v 1.2 1998/06/06 22:45:13 tom Exp $
 */

#include <test.priv.h>

int main(int argc GCC_UNUSED, char *argv[] GCC_UNUSED)
{
	int n;
	for (n = -1; n < 512; n++) {
		printf("%d(%5o):%s\n", n, n, keyname(n));
	}
	return EXIT_SUCCESS;
}
