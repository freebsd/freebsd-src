/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id$
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include "libdisk.h"

int
main(int argc, char **argv)
{
	int i;
	struct disk *d;

	for(i=1;i<argc;i++) {
		d = Open_Disk(argv[i]);
		if (!d) continue;
		Debug_Disk(d);
		if (d->chunks->size == 1411200)
			Set_Bios_Geom(d,1024,15,63);
		else
			Set_Bios_Geom(d,2003,64,32);
		Debug_Disk(d);
		printf("<%s>\n",CheckRules(d));
	}
	exit (0);
}
