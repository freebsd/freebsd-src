/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.ORG> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $FreeBSD$
 *
 */

#include <stdio.h>

int
main(int argc, char **argv)
{

	fprintf(stderr, "ERROR: vnconfig(8) has been discontinued\n");
	fprintf(stderr, "\tPlease used mdconfig(8).\n");
	exit (1);
}
