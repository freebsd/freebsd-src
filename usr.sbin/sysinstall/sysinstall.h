/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated to essentially a complete rewrite.
 *
 * $Id: sysinstall.h,v 1.2 1995/04/27 18:03:55 jkh Exp $
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

/* Bitfields for menu options */
#define DMENU_NORMAL_TYPE	0x1	/* Normal dialog menu		*/
#define DMENU_RADIO_TYPE	0x2	/* Radio dialog menu		*/
#define DMENU_MULTIPLE_TYPE	0x4	/* Multiple choice menu		*/
#define DMENU_SELECTION_RETURNS	0x8	/* Select item then exit	*/

/* Types */
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
} DMenuItemType;

typedef struct _dmenuItem {
    char *title;			/* Our title			*/
    char *prompt;			/* Our prompt			*/
    DMenuItemType type;			/* What type of item we are	*/
    void *ptr;				/* Generic data ptr		*/
    int disabled;			/* Are we temporarily disabled?	*/
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
    char value[1024];
} Variable;


/* Externs */
extern int		CpioFD;	  /* The file descriptor for our CPIO floppy */
extern int		DebugFD;  /* Where diagnostic output goes	*/
extern Boolean		OnCDROM;  /* Are we running off of a CDROM?	*/
extern Boolean		OnSerial; /* Are we on a serial console?	*/
extern Boolean		DialogActive; /* Is the dialog() stuff up?	*/
extern Variable		*VarHead; /* The head of the variable chain	*/

/* All the menus to which forward references exist */
extern DMenu		MenuDocumenation, MenuInitial, MenuLanguage;


/* Prototypes */

/* globals.c */
extern void	globalsInit(void);

/* install.c */
extern int	installCustom(void);
extern int	installExpress(void);
extern int	installMaint(void);

/* system.c */
extern void	systemInitialize(int argc, char **argv);
extern void	systemShutdown(void);
extern void	systemWelcome(void);
extern int	systemExecute(char *cmd);
extern int	systemShellEscape(void);
extern int	systemDisplayFile(char *file);

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

/* media.c */
extern int	mediaSetCDROM(void);
extern int	mediaSetFloppy(void);
extern int	mediaSetDOS(void);
extern int	mediaSetTape(void);
extern int	mediaSetFTP(void);
extern int	mediaSetFS(void);

#endif
/* _SYSINSTALL_H_INCLUDE */
