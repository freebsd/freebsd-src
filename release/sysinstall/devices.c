/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: install.c,v 1.2 1995/04/27 18:03:53 jkh Exp $
 *
 * Copyright (c) 1995
 *	Jordan Hubbard.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer, 
 *    verbatim and that no modifications are made prior to this 
 *    point in the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Jordan Hubbard
 *	for the FreeBSD Project.
 * 4. The name of Jordan Hubbard or the FreeBSD project may not be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JORDAN HUBBARD ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL JORDAN HUBBARD OR HIS PETS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, LIFE OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include "sysinstall.h"
#include "libdisk.h"

/* Get all device information for a given device class */
Device *
device_get_all(DeviceType which, int *ndevs)
{
    char **names;
    Device *devs = NULL;

    *ndevs = 0;
    if (which == DEVICE_TYPE_DISK || which == DEVICE_TYPE_ANY) {
	if ((names = Disk_Names()) != NULL) {
	    int i;

	    for (i = 0; names[i]; i++)
		++*ndevs;
	    devs = safe_malloc(sizeof(Device) * (*ndevs + 1));
	    for (i = 0; names[i]; i++) {
		strcpy(devs[i].name, names[i]);
		devs[i].type = DEVICE_TYPE_DISK;
	    }
	    devs[i].name[0] = '\0';
	    free(names);
	}
    }
    /* put detection for other classes here just as soon as I figure out how */
    return devs;
}

void    
device_print_chunk(struct chunk *c1, int offset, int *row)
{
    CHAR_N

    if (!c1)
	return;
    mvprintw(*row++, offset, "%10lu %10lu %10lu  %-8s %d    %-8s  %4d       %lx",
	     c1->offset, c1->size, c1->end, c1->name, c1->type,
	     chunk_n[c1->type], c1->subtype, c1->flags);
    device_print_chunk(c1->part, offset + 2, row);
    device_print_chunk(c1->next, offset, row);
}

int
device_slice_disk(char *disk)
{
    struct disk *d;
    char *p;
    int row;

    d = Open_Disk(disk);
    if (!d)
	msgFatal("Couldn't open disk `%s'!", disk);
    p = CheckRules(d);
    if (p) {
	msgConfirm(p);
	free(p);
    }
    dialog_clear();
    while (1) {
	clear();
	mvprintw(0, 0, "Disk name: %s,  Flags: %lx", disk, d->flags);
	mvprintw(1, 0,
		 "Real Geometry: %lu/%lu/%lu, BIOS Geometry: %lu/%lu/%lu [cyls/heads/sectors]",
		 d->real_cyl, d->real_hd, d->real_sect,
		 d->bios_cyl, d->bios_hd, d->bios_sect);
	mvprintw(4, 0, "%10s %10s %10s %-8s %4s %-8s     %4s %4s",
		 "Offset", "Size", "End", "Name", "PType", "Desc",
		 "Subtype", "Flags");
	row = 5;
	device_print_chunk(d->chunks, 0, &row);
	move(23, 0);
	addstr("Done!");
	if (getch() == 'q')
	    return 0;
    }
    return 0;
}
