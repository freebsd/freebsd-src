/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated to essentially a complete rewrite.
 *
 * $Id: sysinstall.h,v 1.15 1995/05/10 07:45:00 jkh Exp $
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
#include "dist.h"

/*** Defines ***/

/* Bitfields for menu options */
#define DMENU_NORMAL_TYPE	0x1	/* Normal dialog menu		*/
#define DMENU_RADIO_TYPE	0x2	/* Radio dialog menu		*/
#define DMENU_MULTIPLE_TYPE	0x4	/* Multiple choice menu		*/
#define DMENU_SELECTION_RETURNS	0x8	/* Select item then exit	*/
#define DMENU_CALL_FIRST	0x10	/* In multiple, use one handler */

/* variable limits */
#define VAR_NAME_MAX		128
#define VAR_VALUE_MAX		1024

/* device limits */
#define DEV_NAME_MAX		128	/* The maximum length of a device name	*/
#define DEV_MAX			200	/* The maximum number of devices we'll deal with */
#define INTERFACE_MAX		50	/* Maximum number of network interfaces we'll deal with */


/* Internal flag variables */
#define DISK_PARTITIONED	"_diskPartitioned"
#define DISK_LABELLED		"_diskLabelled"
#define TCP_CONFIGURED		"_tcpConfigured"
#define NO_CONFIRMATION		"_noConfirmation"
#define MEDIA_DEVICE		"mediaDevice"
#define MEDIA_TYPE		"mediaType"

#define VAR_HOSTNAME		"hostname"
#define VAR_DOMAINNAME		"domainname"
#define VAR_IPADDR		"ip_addr"
#define VAR_NAMESERVER		"nameserver"

#define VAR_IFCONFIG_ARGS	"if_flags"
#define VAR_NETMASK		"netmask"
#define VAR_GATEWAY		"gateway"


/*** Types ***/
typedef unsigned int Boolean;
typedef struct disk Disk;
typedef struct chunk Chunk;

typedef enum {
    DMENU_SHELL_ESCAPE,			/* Fork a shell			*/
    DMENU_DISPLAY_FILE,			/* Display a file's contents	*/
    DMENU_SUBMENU,			/* Recurse into another menu	*/
    DMENU_SYSTEM_COMMAND,		/* Run shell commmand		*/
    DMENU_SYSTEM_COMMAND_BOX,		/* Same as above, but in prgbox	*/
    DMENU_SET_VARIABLE,			/* Set an environment/system var */
    DMENU_SET_FLAG,			/* Set flag in an unsigned int	*/
    DMENU_CALL,				/* Call back a C function	*/
    DMENU_CANCEL,			/* Cancel out of this menu	*/
    DMENU_NOP,				/* Do nothing special for item	*/
} DMenuItemType;

typedef struct _dmenuItem {
    char *title;			/* Our title			*/
    char *prompt;			/* Our prompt			*/
    DMenuItemType type;			/* What type of item we are	*/
    void *ptr;				/* Generic data ptr		*/
    u_long parm;			/* Parameter for above		*/
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
    DEVICE_TYPE_NONE,
    DEVICE_TYPE_DISK,
    DEVICE_TYPE_FLOPPY,
    DEVICE_TYPE_NETWORK,
    DEVICE_TYPE_CDROM,
    DEVICE_TYPE_TAPE,
    DEVICE_TYPE_ANY,
} DeviceType;

/* A "device" from sysinstall's point of view */
typedef struct _device {
    char name[DEV_NAME_MAX];
    DeviceType type;
    Boolean enabled;
    int (*deviceInit)(void);
    int (*deviceGet)(char *fname);
    int (*deviceClose)(void);
    void *devicePrivate;
} Device;

/* Some internal representations of partitions */
typedef enum {
    PART_NONE,
    PART_SLICE,
    PART_SWAP,
    PART_FILESYSTEM,
    PART_FAT,
} PartType;

/* The longest newfs command we'll hand to system() */
#define NEWFS_CMD_MAX	256

typedef struct _part_info {
    Boolean newfs;
    char mountpoint[FILENAME_MAX];
    char newfs_cmd[NEWFS_CMD_MAX];
} PartInfo;

typedef int (*commandFunc)(char *key, void *data);


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
extern unsigned int	XF86Dists;/* Which XFree86 dists we want	*/
extern unsigned int	XF86ServerDists; /* The XFree86 servers we want */
extern unsigned int	XF86FontDists; /* The XFree86 fonts we want     */
extern Device		*Devices[]; /* The devices we have to work with	*/

/*** Prototypes ***/

/* command.c */
extern void	command_clear(void);
extern void	command_sort(void);
extern void	command_execute(void);
extern void	command_shell_add(char *key, char *fmt, ...);
extern void	command_func_add(char *key, commandFunc func, void *data);

/* decode.c */
extern DMenuItem *decode(DMenu *menu, char *name);
extern Boolean	dispatch(DMenuItem *tmp, char *name);
extern Boolean	decode_and_dispatch_multiple(DMenu *menu, char *names);

/* devices.c */
extern DMenu	*deviceCreateMenu(DMenu *menu, DeviceType type, int (*hook)());
extern Device	*deviceGetInfo(DeviceType which);

/* disks.c */
extern int	diskPartitionEditor(Disk *disk);

/* dist.c */
extern int	distSetDeveloper(char *str);
extern int	distSetXDeveloper(char *str);
extern int	distSetUser(char *str);
extern int	distSetXUser(char *str);
extern int	distSetMinimum(char *str);
extern int	distSetEverything(char *str);
extern int	distSetSrc(char *str);
extern void	distExtractAll(void);

/* dmenu.c */
extern void	dmenuOpen(DMenu *menu, int *choice, int *scroll,
			  int *curr, int *max);
extern void	dmenuOpenSimple(DMenu *menu);

/* globals.c */
extern void	globalsInit(void);

/* install.c */
extern int	installCustom(char *str);
extern int	installExpress(char *str);
extern int	installMaint(char *str);

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

/* label.c */
extern void	diskLabelEditor(char *str);

/* makedevs.c (auto-generated) */
extern const char	termcap_vt100[];
extern const char	termcap_cons25[];
extern const char	termcap_cons25_m[];
extern const char	termcap_cons25r[];
extern const char	termcap_cons25r_m[];
extern const char	termcap_cons25l1[];
extern const char	termcap_cons25l1_m[];
extern const u_char	font_iso_8x14[];
extern const u_char	font_cp850_8x14[];
extern const u_char	font_koi8_r_8x14[];
extern const u_char	koi8_r2cp866[];

/* media.c */
extern int	mediaSetCDROM(char *str);
extern int	mediaSetFloppy(char *str);
extern int	mediaSetDOS(char *str);
extern int	mediaSetTape(char *str);
extern int	mediaSetFTP(char *str);
extern int	mediaSetFS(char *str);

/* misc.c */
extern Boolean	file_readable(char *fname);
extern Boolean	file_executable(char *fname);
extern char	*string_concat(char *p1, char *p2);
extern char	*string_prune(char *str);
extern char	*string_skipwhite(char *str);
extern void	safe_free(void *ptr);
extern void	*safe_malloc(size_t size);
extern void	*safe_realloc(void *orig, size_t size);
extern char	**item_add(char **list, char *item, int *curr, int *max);
extern char	**item_add_pair(char **list, char *item1, char *item2,
				int *curr, int *max);
extern void	items_free(char **list, int *curr, int *max);

/* msg.c */
extern void	msgInfo(char *fmt, ...);
extern void	msgYap(char *fmt, ...);
extern void	msgWarn(char *fmt, ...);
extern void	msgDebug(char *fmt, ...);
extern void	msgError(char *fmt, ...);
extern void	msgFatal(char *fmt, ...);
extern void	msgConfirm(char *fmt, ...);
extern void	msgNotify(char *fmt, ...);
extern int	msgYesNo(char *fmt, ...);
extern char	*msgGetInput(char *buf, char *fmt, ...);

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
extern int	vsystem(char *fmt, ...);

/* termcap.c */
extern int	set_termcap(void);

/* variables.c */
extern void	variable_set(char *var);
extern void	variable_set2(char *name, char *value);

/* wizard.c */
extern void	slice_wizard(Disk *d);

#endif
/* _SYSINSTALL_H_INCLUDE */
