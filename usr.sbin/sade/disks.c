/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: install.c,v 1.5 1995/05/04 03:51:16 jkh Exp $
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

/* If the given disk has a root partition on it, return TRUE */
static Boolean
contains_root_partition(struct disk *d)
{
    return FALSE;
}

void
partition_disk(struct disk *d)
{
}

int
write_disks(struct disk **disks)
{
    int i;
    extern u_char boot1[], boot2[];
    extern u_char mbr[], bteasy17[];

    dialog_clear();
    if (!msgYesNo("Last Chance!  Are you sure you want to write your changes to disk?")) {
	for (i = 0; disks[i]; i++) {
	    if (contains_root_partition(disks[i]))
		Set_Boot_Blocks(disks[i], boot1, boot2);
	    if (i == 0 && !msgYesNo("Would you like to install a boot manager?\n\nThis will allow you to easily select between other operating systems\non the first disk, as well as boot from a driver other than the first."))
		Set_Boot_Mgr(disks[i], bteasy17);
	    else if (i == 0 && !msgYesNo("Would you like to remove an existing boot manager?"))
		Set_Boot_Mgr(disks[i], mbr);
	    Write_Disk(disks[i]);
	}
	return 0;
    }
    return 1;
}

void
make_filesystems(struct disk **disks)
{
}

void
cpio_extract(struct disk **disks)
{
}

void
extract_dists(struct disk **disks)
{
}

void
do_final_setup(struct disk **disks)
{
}

