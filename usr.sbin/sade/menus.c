/*
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

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif

#include "sade.h"

/* All the system menus go here.
 *
 * Hardcoded things like version number strings will disappear from
 * these menus just as soon as I add the code for doing inline variable
 * expansion.
 */

DMenu MenuDiskDevices = {
    DMENU_CHECKLIST_TYPE | DMENU_SELECTION_RETURNS,
    "Select Drive(s)",
    "Please select the drive, or drives, on which you wish to perform\n"
    "this operation.  If you are attempting to install a boot partition\n"
    "on a drive other than the first one or have multiple operating\n"
    "systems on your machine, you will have the option to install a boot\n"
    "manager later.  To select a drive, use the arrow keys to move to it\n"
    "and press [SPACE] or [ENTER].  To de-select it, press it again.\n\n"
    "Use [TAB] to get to the buttons and leave this menu.",
    "Press F1 for important information regarding disk geometry!",
    "drives",
    { { NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0 } },
};

DMenu MenuMain = {
    DMENU_NORMAL_TYPE,
    "Disklabel and partitioning utility",
    "This is a utility for partitioning and/or labelling your disks.",
    "DISKUTIL",
    "main",
    { 
#ifdef WITH_SLICES
      { "1 Partition",		"Managing disk partitions",	NULL, diskPartitionEditor, NULL, NULL, 0, 0, 0, 0 },
#endif
      { "2 Label",		"Label allocated disk partitions",	NULL, diskLabelEditor, NULL, NULL, 0, 0, 0, 0 },
      { NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0 }
    },
};

#if defined(__i386__) || defined(__amd64__)
#ifdef PC98
/* IPL type menu */
DMenu MenuIPLType = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "overwrite me",		/* will be disk specific label */
    "If you want a FreeBSD Boot Manager, select \"BootMgr\".  If you would\n"
    "prefer your Boot Manager to remain untouched then select \"None\".\n\n",
    "Press F1 to read about drive setup",
    "drives",
    { { "BootMgr",	"Install the FreeBSD Boot Manager",
	dmenuRadioCheck, dmenuSetValue, NULL, &BootMgr, 0, 0, 0, 0 },
      { "None",		"Leave the IPL untouched",
	dmenuRadioCheck, dmenuSetValue, NULL, &BootMgr, '(', '*', ')', 1 },
      { NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0 } },
};
#else
/* MBR type menu */
DMenu MenuMBRType = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "overwrite me",		/* will be disk specific label */
    "FreeBSD comes with a boot manager that allows you to easily\n"
    "select between FreeBSD and any other operating systems on your machine\n"
    "at boot time.  If you have more than one drive and want to boot\n"
    "from the second one, the boot manager will also make it possible\n"
    "to do so (limitations in the PC BIOS usually prevent this otherwise).\n"
    "If you have other operating systems installed and would like a choice when\n"
    "booting, choose \"BootMgr\". If you would prefer to keep your existing\n"
    "boot manager, select \"None\".\n\n",
    "",
    "drives",
    { { "Standard",	"Install a standard MBR (non-interactive boot manager)",
	dmenuRadioCheck, dmenuSetValue, NULL, &BootMgr, '(', '*', ')', 1 },
      { "BootMgr",	"Install the FreeBSD boot manager",
	dmenuRadioCheck, dmenuSetValue, NULL, &BootMgr, '(', '*', ')', 0 },
      { "None",		"Do not install a boot manager",
	dmenuRadioCheck, dmenuSetValue, NULL, &BootMgr, '(', '*', ')', 2 },
      { NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0 } }
};
#endif /* PC98 */
#endif /* __i386__ */


