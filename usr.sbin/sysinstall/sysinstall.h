/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated to essentially a complete rewrite.
 *
 * $Id: sysinstall.h,v 1.5 1995/05/04 03:51:22 jkh Exp $
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
#include "libdisk.h"

/*** Defines ***/

/* Bitfields for menu options */
#define DMENU_NORMAL_TYPE	0x1	/* Normal dialog menu		*/
#define DMENU_RADIO_TYPE	0x2	/* Radio dialog menu		*/
#define DMENU_MULTIPLE_TYPE	0x4	/* Multiple choice menu		*/
#define DMENU_SELECTION_RETURNS	0x8	/* Select item then exit	*/

/* Bitfields for distributions - hope we never have more than 32! :-) */
#define DIST_BIN		0x1
#define DIST_GAMES		0x2
#define DIST_MANPAGES		0x4
#define DIST_PROFLIBS		0x8
#define DIST_DICT		0x10
#define DIST_SRC		0x20
#define DIST_DES		0x40
#define DIST_COMPAT1X		0x80
#define DIST_XFREE86		0x100
#define DIST_ALL		0xFFF

/* Canned distribution sets */
#define _DIST_DEVELOPER \
	(DIST_BIN | DIST_MANPAGES | DIST_DICT | DIST_PROFLIBS | DIST_SRC)

#define _DIST_XDEVELOPER \
	(_DIST_DEVELOPER | DIST_XFREE86)

#define _DIST_USER \
	(DIST_BIN | DIST_MANPAGES | DIST_DICT | DIST_COMPAT1X)

#define _DIST_XUSER \
	(_DIST_USER | DIST_XFREE86)


/* Subtypes for SRC distribution */
#define DIST_SRC_BASE		0x1
#define DIST_SRC_GNU		0x2
#define DIST_SRC_ETC		0x4
#define DIST_SRC_GAMES		0x8
#define DIST_SRC_INCLUDE	0x10
#define DIST_SRC_LIB		0x20
#define DIST_SRC_LIBEXEC	0x40
#define DIST_SRC_LKM		0x80
#define DIST_SRC_RELEASE	0x100
#define DIST_SRC_SBIN		0x200
#define DIST_SRC_SHARE		0x400
#define DIST_SRC_SYS		0x800
#define DIST_SRC_UBIN		0x1000
#define DIST_SRC_USBIN		0x2000
#define DIST_SRC_ALL		0xFFFF

/* variable limits */
#define VAR_NAME_MAX		128
#define VAR_VALUE_MAX		1024

/* device limits */
#define DEV_NAME_MAX		128

/* handy */
#define ONE_MEG			1048576


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
extern Boolean		SystemWasInstalled; /* Did we install it?       */ 
extern Boolean		DialogActive; /* Is the dialog() stuff up?	*/
extern Boolean		ColorDisplay; /* Are we on a color display?     */
extern Boolean		OnVTY;    /* On a syscons VTY?			*/
extern Variable		*VarHead; /* The head of the variable chain	*/
extern unsigned int	Dists;    /* Which distributions we want        */
extern unsigned int	SrcDists; /* Which src distributions we want    */


/*** Prototypes ***/

/* globals.c */
extern void	globalsInit(void);

/* install.c */
extern int	installCustom(char *str);
extern int	installExpress(char *str);
extern int	installMaint(char *str);

/* dist.c */
extern int	distSetDeveloper(char *str);
extern int	distSetXDeveloper(char *str);
extern int	distSetUser(char *str);
extern int	distSetXUser(char *str);
extern int	distSetMinimum(char *str);
extern int	distSetEverything(char *str);

/* system.c */
extern void	systemInitialize(int argc, char **argv);
extern void	systemShutdown(void);
extern void	systemWelcome(void);
extern int	systemExecute(char *cmd);
extern int	systemShellEscape(void);
extern int	systemDisplayFile(char *file);
extern char	*systemHelpFile(char *file, char *buf);
extern void	systemChangeFont(const u_char font[]);
extern void	systemChangeLang(char *lang);
extern void	systemChangeTerminal(char *color, const u_char c_termcap[],
				     char *mono, const u_char m_termcap[]);
extern void	systemChangeScreenmap(const u_char newmap[]);

/* disks.c */
extern void	partition_disks(struct disk **disks);
extern int	write_disks(struct disk **disks);
extern void	make_filesystems(struct disk **disks);
extern void	cpio_extract(struct disk **disks);
extern void	extract_dists(struct disk **disks);
extern void	do_final_setup(struct disk **disks);

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
extern void	msgYap(char *fmt, ...);
extern void	msgWarn(char *fmt, ...);
extern void	msgError(char *fmt, ...);
extern void	msgFatal(char *fmt, ...);
extern void	msgConfirm(char *fmt, ...);
extern int	msgYesNo(char *fmt, ...);
extern char	*msgGetInput(char *buf, char *fmt, ...);

/* media.c */
extern int	mediaSetCDROM(char *str);
extern int	mediaSetFloppy(char *str);
extern int	mediaSetDOS(char *str);
extern int	mediaSetTape(char *str);
extern int	mediaSetFTP(char *str);
extern int	mediaSetFS(char *str);

/* devices.c */
extern Device	*device_get_all(DeviceType type, int *ndevs);
extern struct disk *device_slice_disk(struct disk *d);
extern DMenu	*device_create_disk_menu(DMenu *menu, Device **rdevs,
					 int (*func)());

/* variables.c */
extern void	variable_set(char *var);
extern void	variable_set2(char *name, char *value);

/* lang.c */
extern void	lang_set_Danish(char *str);
extern void	lang_set_Dutch(char *str);
extern void	lang_set_English(char *str);
extern void	lang_set_French(char *str);
extern void	lang_set_German(char *str);
extern void	lang_set_Italian(char *str);
extern void	lang_set_Japanese(char *str);
extern void	lang_set_Norwegian(char *str);
extern void	lang_set_Russian(char *str);
extern void	lang_set_Spanish(char *str);
extern void	lang_set_Swedish(char *str);

/* makedevs.c (auto-generated) */
extern const char termcap_vt100[];
extern const char termcap_cons25[];
extern const char termcap_cons25_m[];
extern const char termcap_cons25r[];
extern const char termcap_cons25r_m[];
extern const char termcap_cons25l1[];
extern const char termcap_cons25l1_m[];
extern const u_char font_iso_8x14[];
extern const u_char font_cp850_8x14[];
extern const u_char font_koi8_r_8x14[];
extern const u_char koi8_r2cp866[];

/* wizard.c */
extern void	slice_wizard(struct disk *d);

#endif
/* _SYSINSTALL_H_INCLUDE */
