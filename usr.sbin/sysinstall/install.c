/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: install.c,v 1.4 1995/05/01 21:56:22 jkh Exp $
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

static int
installHook(char *str)
{
    int i;
    struct disk *disks[100];	/* some ridiculously large number */

    i = 0;
    /* Clip garbage off the ends */
    string_prune(str);
    str = string_skipwhite(str);
    while (str) {
	char *cp;

	cp = index(str, '\n');
	if (cp)
	   *cp++ = 0; 
	if (!*str) {
	    beep();
	    return 0;
	}
	disks[i++] = device_slice_disk(str);
	str = cp;
    }
    disks[i] = NULL;
    if (!i)
	return 0;
    else {
#ifdef notdoneyet
	partition_disks(disks);
	if (!confirm_write(disks)) {
	    for (i = 0; disks[i]; i++)
		Free_Disk(disks[i]);
	    return 0;
	}
	else {
	    make_filesystems(disks);
	    cpio_extract(disks);
	    extract_dists(disks);
	    do_final_setup(disks);
	    systemShutdown();
	}
#endif
    }
    return 1;
}

int
installCustom(char *str)
{
    int scroll, choice, curr, max;
    extern DMenu MenuDiskDevices;
    DMenu *menu;
    Device *devs;

    variable_set2("install_type", "custom");
    menu = device_create_disk_menu(&MenuDiskDevices, &devs, installHook);
    if (!menu)
	return 0;
    choice = scroll = curr = max = 0;
    dmenuOpen(menu, &choice, &scroll, &curr, &max);
    free(menu);
    free(devs);
    return 1;
}

int
installExpress(char *str)
{
    int scroll, choice, curr, max;
    extern DMenu MenuDiskDevices;
    DMenu *menu;
    Device *devs;

    variable_set2("install_type", "express");
    menu = device_create_disk_menu(&MenuDiskDevices, &devs, installHook);
    if (!menu)
	return 0;
    choice = scroll = curr = max = 0;
    dmenuOpen(menu, &choice, &scroll, &curr, &max);
    free(menu);
    free(devs);
    return 1;
}

int
installMaint(char *str)
{
    msgConfirm("Sorry, maintainance mode is not implemented in this version.");
    return 0;
}

int
installSetDeveloper(char *str)
{
    /* Dists = DIST_BIN | DIST_MAN | DIST_FOO; */
    return 0;
}

int
installSetXDeveloper(char *str)
{
    return 0;
}

int
installSetUser(char *str)
{
    return 0;
}

int
installSetXUser(char *str)
{
    return 0;
}

int
installSetMinimum(char *str)
{
    return 0;
}

int
installSetEverything(char *str)
{
    return 0;
}
