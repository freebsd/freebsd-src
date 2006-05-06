/* $FreeBSD: src/tools/tools/ncpus/ncpus.c,v 1.1.2.2 2006/03/10 22:04:07 sam Exp $ */

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
