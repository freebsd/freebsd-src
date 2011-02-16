/* $FreeBSD: src/tools/tools/ncpus/ncpus.c,v 1.3.10.1.6.1 2010/12/21 17:09:25 kensmith Exp $ */

#include <stdio.h>

extern int acpi_detect(void);
extern int biosmptable_detect(void);

int
main(void)
{
	printf("acpi: %d\n", acpi_detect());
#if defined(__amd64__) || defined(__i386__)
	printf("mptable: %d\n", biosmptable_detect());
#endif
	return 0;
}
