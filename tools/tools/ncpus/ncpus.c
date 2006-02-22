/* $FreeBSD$ */

#include <stdio.h>

extern int acpi_detect(void);
extern int biosmptable_detect(void);

int
main(int argc, char *argv[])
{
	printf("acpi: %d\n", acpi_detect());
	printf("mptable: %d\n", biosmptable_detect());
	return 0;
}
