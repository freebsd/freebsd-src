/*
 *  Copyright (C) 1992, 1993 Søren Schmidt
 *
 *  This program is free software; you may redistribute it and/or 
 *  modify it, provided that it retain the above copyright notice 
 *  and the following disclaimer.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *
 *	Søren Schmidt 		Email:	sos@kmd-ac.dk
 *	Tritonvej 36		UUCP:	...uunet!dkuug!kmd-ac!sos
 *	DK9210 Aalborg SO	Phone:  +45 9814 8076
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <machine/console.h>
#include <stdio.h>

#include FIL

main(int argc, char **argv)
{
	FILE *fd;

	if (argc == 2) {
		fd = fopen(argv[1], "w");
		if (fd == NULL) {
			printf("%s: cannot create file\n", argv[0]);
			exit(1);
		}
		fwrite(&keymap, sizeof(keymap_t), 1, fd);
		fclose(fd);
		exit(0);
	}
	else
		printf("usage: mkkbdfil <mapfile>\n");
		exit(1);
}
