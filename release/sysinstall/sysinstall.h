/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated to essentially a complete rewrite.
 *
 * $Id$
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

/* Types */
typedef unsigned int Boolean;

typedef enum {
    MENU_SHELL_ESCAPE,
    MENU_DISPLAY_FILE,
    MENU_SUBMENU,
    MENU_SYSTEM_COMMAND,
    MENU_SET_VARIABLE,
    MENU_CALL
} DMenuItemType;

typedef struct _dmenuItem {
    char *title;
    char *prompt;
    DMenuItemType type;
    void *ptr;
    int disabled;
} DMenuItem;

typedef struct _dmenu {
    char *title;
    char *prompt;
    char *helpline;
    char *helpfile;
    DMenuItem *items;
} DMenu;

/* A sysconfig variable */
typedef struct _variable {
    struct _variable *next;
    char value[1024];
} Variable;


/* Externs */
extern int		CpioFD;	  /* The file descriptor for our CPIO floppy */
extern int		DebugFD;  /* Where diagnostic output goes */
extern Boolean		OnCDROM;  /* Are we running off of a CDROM? */
extern Boolean		OnSerial; /* Are we on a serial console? */
extern Boolean		DialogActive; /* Is the dialog() stuff up? */
extern Variable		*VarHead; /* The head of the variable chain */

/* All the menus to which forward references exist */
extern DMenu		MenuDocumenation, MenuInitial, MenuLanguage;


/* Prototypes */
extern void	globalsInit(void);

extern void	installExpress(void);
extern void	installCustom(void);

extern void	systemInitialize(int argc, char **argv);
extern void	systemShutdown(void);
extern void	systemWelcome(void);
extern int	systemExecute(char *cmd);

extern void	dmenuOpen(DMenu *menu, int *choice, int *scroll,
			  int *curr, int *max);

extern Boolean	file_readable(char *fname);
extern Boolean	file_executable(char *fname);
extern char	*string_concat(char *p1, char *p2);
extern char	*string_prune(char *str);
extern char	*string_skipwhite(char *str);
extern void	safe_free(void *ptr);
extern char	**item_add(char **list, char *item, int *curr, int *max);
extern void	items_free(char **list, int *curr, int *max);

extern int	set_termcap(void);

extern void	msgInfo(char *fmt, ...);
extern void	msgWarn(char *fmt, ...);
extern void	msgError(char *fmt, ...);
extern void	msgFatal(char *fmt, ...);

#endif
/* _SYSINSTALL_H_INCLUDE */
