/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated to essentially a complete rewrite.
 *
 * $Id: sysinstall.h,v 1.3 1995/04/29 19:33:05 jkh Exp $
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

#ifndef _SYSINSTALL_H_INCLUDE
#define _SYSINSTALL_H_INCLUDE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dialog.h>

/*** Defines ***/

/* Bitfields for menu options */
#define DMENU_NORMAL_TYPE	0x1	/* Normal dialog menu		*/
#define DMENU_RADIO_TYPE	0x2	/* Radio dialog menu		*/
#define DMENU_MULTIPLE_TYPE	0x4	/* Multiple choice menu		*/
#define DMENU_SELECTION_RETURNS	0x8	/* Select item then exit	*/

/* variable limits */
#define VAR_NAME_MAX		128
#define VAR_VALUE_MAX		1024

/* device limits */
#define DEV_NAME_MAX		128


/*** Types ***/
typedef unsigned int Boolean;

typedef enum {
    DMENU_SHELL_ESCAPE,			/* Fork a shell			*/
    DMENU_DISPLAY_FILE,			/* Display a file's contents	*/
    DMENU_SUBMENU,			/* Recurse into another menu	*/
    DMENU_SYSTEM_COMMAND,		/* Run shell commmand		*/
    DMENU_SYSTEM_COMMAND_BOX,		/* Same as above, but in prgbox	*/
    DMENU_SET_VARIABLE,			/* Set an environment/system var */
    DMENU_CALL,				/* Call back a C function	*/
    DMENU_CANCEL,			/* Cancel out of this menu	*/
    DMENU_NOP,				/* Do nothing special for item	*/
} DMenuItemType;

typedef struct _dmenuItem {
    char *title;			/* Our title			*/
    char *prompt;			/* Our prompt			*/
    DMenuItemType type;			/* What type of item we are	*/
    void *ptr;				/* Generic data ptr		*/
    Boolean disabled;			/* Are we temporarily disabled?	*/
} DMenuItem;

typedef struct _dmenu {
    unsigned int options;		/* What sort of menu we are	*/
    char *title;			/* Our title			*/
    char *prompt;			/* Our prompt			*/
    char *helpline;			/* Line of help at bottom	*/
    char *helpfile;			/* Help file for "F1"		*/
    DMenuItem items[0];			/* Array of menu items		*/
} DMenu;

/* A sysconfig variable */
typedef struct _variable {
    struct _variable *next;
    char name[VAR_NAME_MAX];
    char value[VAR_VALUE_MAX];
} Variable;

typedef enum {
    DEVICE_TYPE_ANY,
    DEVICE_TYPE_DISK,
    DEVICE_TYPE_FLOPPY,
    DEVICE_TYPE_NETWORK,
    DEVICE_TYPE_CDROM,
    DEVICE_TYPE_TAPE,
    DEVICE_TYPE_SERIAL,
    DEVICE_TYPE_PARALLEL,
} DeviceType;

/* A "device" from sysinstall's point of view */
typedef struct _device {
    char name[DEV_NAME_MAX];
    DeviceType type;
} Device;


/*** Externs ***/
extern int		CpioFD;	  /* The file descriptor for our CPIO floppy */
extern int		DebugFD;  /* Where diagnostic output goes	*/
extern Boolean		OnCDROM;  /* Are we running off of a CDROM?	*/
extern Boolean		OnSerial; /* Are we on a serial console?	*/
extern Boolean		DialogActive; /* Is the dialog() stuff up?	*/
extern Variable		*VarHead; /* The head of the variable chain	*/

/* All the menus to which forward references exist */
extern DMenu		MenuDocumenation, MenuInitial, MenuLanguage;


/*** Prototypes ***/

/* globals.c */
extern void	globalsInit(void);

/* install.c */
extern int	installCustom(char *str);
extern int	installExpress(char *str);
extern int	installMaint(char *str);
extern int	installSetDeveloper(char *str);
extern int	installSetXDeveloper(char *str);
extern int	installSetUser(char *str);
extern int	installSetXUser(char *str);
extern int	installSetMinimum(char *str);
extern int	installSetEverything(char *str);

/* system.c */
extern void	systemInitialize(int argc, char **argv);
extern void	systemShutdown(void);
extern void	systemWelcome(void);
extern int	systemExecute(char *cmd);
extern int	systemShellEscape(void);
extern int	systemDisplayFile(char *file);
extern char	*systemHelpFile(char *file, char *buf);

/* dmenu.c */
extern void	dmenuOpen(DMenu *menu, int *choice, int *scroll,
			  int *curr, int *max);

/* misc.c */
extern Boolean	file_readable(char *fname);
extern Boolean	file_executable(char *fname);
extern char	*string_concat(char *p1, char *p2);
extern char	*string_prune(char *str);
extern char	*string_skipwhite(char *str);
extern void	safe_free(void *ptr);
extern void	*safe_malloc(size_t size);
extern char	**item_add(char **list, char *item, int *curr, int *max);
extern char	**item_add_pair(char **list, char *item1, char *item2,
				int *curr, int *max);
extern void	items_free(char **list, int *curr, int *max);

/* termcap.c */
extern int	set_termcap(void);

/* msg.c */
extern void	msgInfo(char *fmt, ...);
extern void	msgWarn(char *fmt, ...);
extern void	msgError(char *fmt, ...);
extern void	msgFatal(char *fmt, ...);
extern void	msgConfirm(char *fmt, ...);
extern int	msgYesNo(char *fmt, ...);

/* media.c */
extern int	mediaSetCDROM(char *str);
extern int	mediaSetFloppy(char *str);
extern int	mediaSetDOS(char *str);
extern int	mediaSetTape(char *str);
extern int	mediaSetFTP(char *str);
extern int	mediaSetFS(char *str);

/* devices.c */
extern Device	*device_get_all(DeviceType type, int *ndevs);
extern int	device_slice_disk(char *disk);

/* variables.c */
extern void	variable_set(char *var);
extern void	variable_set2(char *name, char *value);

#endif
/* _SYSINSTALL_H_INCLUDE */
