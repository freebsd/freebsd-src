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
 * $FreeBSD$
 */

#ifndef _SADE_H_INCLUDE
#define _SADE_H_INCLUDE

#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dialog.h>
#include "ui_objects.h"
#include "dir.h"
#include "colors.h"

/*** Defines ***/

#if defined(__i386__) || defined(__amd64__)
#define	WITH_SYSCONS
#define	WITH_MICE
#endif

#if defined(__i386__) || defined(__amd64__)
#define	WITH_SLICES
#endif

#if defined(__i386__)
#define	WITH_LINUX
#endif

/* device limits */
#define DEV_NAME_MAX		128	/* The maximum length of a device name	*/
#define DEV_MAX			100	/* The maximum number of devices we'll deal with */
#define IO_ERROR		-2	/* Status code for I/O error rather than normal EOF */

/*
 * I make some pretty gross assumptions about having a max of 50 chunks
 * total - 8 slices and 42 partitions.  I can't easily display many more
 * than that on the screen at once!
 *
 * For 2.1 I'll revisit this and try to make it more dynamic, but since
 * this will catch 99.99% of all possible cases, I'm not too worried.
 */
#define MAX_CHUNKS	40

/* Internal environment variable names */
#define DISK_PARTITIONED		"_diskPartitioned"
#define DISK_LABELLED			"_diskLabelled"
#define DISK_SELECTED			"_diskSelected"
#define SYSTEM_STATE			"_systemState"
#define RUNNING_ON_ROOT			"_runningOnRoot"

/* Ones that can be tweaked from config files */
#define VAR_BLANKTIME			"blanktime"
#define VAR_BOOTMGR			"bootManager"
#define VAR_DEBUG			"debug"
#define VAR_DISK			"disk"
#define VAR_DISKINTERACTIVE		"diskInteractive"
#define VAR_DEDICATE_DISK		"dedicateDisk"
#define VAR_COMMAND			"command"
#define VAR_CONFIG_FILE			"configFile"
#define VAR_GEOMETRY			"geometry"
#define VAR_INSTALL_CFG			"installConfig"
#define VAR_INSTALL_ROOT		"installRoot"
#define VAR_LABEL			"label"
#define VAR_LABEL_COUNT			"labelCount"
#define VAR_NEWFS_ARGS			"newfsArgs"
#define VAR_NO_CONFIRM			"noConfirm"
#define VAR_NO_ERROR			"noError"
#define VAR_NO_WARN			"noWarn"
#define VAR_NO_USR			"noUsr"
#define VAR_NO_TMP			"noTmp"
#define VAR_NO_HOME			"noHome"
#define VAR_NONINTERACTIVE		"nonInteractive"
#define VAR_PARTITION			"partition"
#define VAR_RELNAME			"releaseName"
#define VAR_ROOT_SIZE			"rootSize"
#define VAR_SWAP_SIZE			"swapSize"
#define VAR_TAPE_BLOCKSIZE		"tapeBlocksize"
#define VAR_UFS_PATH			"ufs"
#define VAR_USR_SIZE			"usrSize"
#define VAR_VAR_SIZE			"varSize"
#define VAR_TMP_SIZE			"tmpSize"
#define VAR_TERM			"TERM"
#define VAR_CONSTERM                    "_consterm"

#define DEFAULT_TAPE_BLOCKSIZE	"20"

/* One MB worth of blocks */
#define ONE_MEG				2048
#define ONE_GIG				(ONE_MEG * 1024)

/* Which selection attributes to use */
#define ATTR_SELECTED			(ColorDisplay ? item_selected_attr : item_attr)
#define ATTR_TITLE	button_active_attr

/* Handy strncpy() macro */
#define SAFE_STRCPY(to, from)	sstrncpy((to), (from), sizeof (to) - 1)

/*** Types ***/
typedef int Boolean;
typedef struct disk Disk;
typedef struct chunk Chunk;

/* Bitfields for menu options */
#define DMENU_NORMAL_TYPE	0x1     /* Normal dialog menu           */
#define DMENU_RADIO_TYPE	0x2     /* Radio dialog menu            */
#define DMENU_CHECKLIST_TYPE	0x4     /* Multiple choice menu         */
#define DMENU_SELECTION_RETURNS 0x8     /* Immediate return on item selection */

typedef struct _dmenu {
    int type;				/* What sort of menu we are	*/
    char *title;			/* Our title			*/
    char *prompt;			/* Our prompt			*/
    char *helpline;			/* Line of help at bottom	*/
    char *helpfile;			/* Help file for "F1"		*/
    dialogMenuItem items[];		/* Array of menu items		*/
} DMenu;

/* An rc.conf variable */
typedef struct _variable {
    struct _variable *next;
    char *name;
    char *value;
    int dirty;
} Variable;

#define NO_ECHO_OBJ(type)	((type) | (DITEM_NO_ECHO << 16))
#define TYPE_OF_OBJ(type)	((type) & 0xff)
#define ATTR_OF_OBJ(type)	((type) >> 16)

/* A screen layout structure */
typedef struct _layout {
    int         y;              /* x & Y co-ordinates */
    int         x;
    int         len;            /* The size of the dialog on the screen */
    int         maxlen;         /* How much the user can type in ... */
    char        *prompt;        /* The string for the prompt */
    char        *help;          /* The display for the help line */
    void        *var;           /* The var to set when this changes */
    int         type;           /* The type of the dialog to create */
    void        *obj;           /* The obj pointer returned by libdialog */
} Layout;

typedef enum {
    DEVICE_TYPE_NONE,
    DEVICE_TYPE_DISK,
    DEVICE_TYPE_DOS,
    DEVICE_TYPE_UFS,
    DEVICE_TYPE_ANY,
} DeviceType;

/* A "device" from sade's point of view */
typedef struct _device {
    char name[DEV_NAME_MAX];
    char *description;
    char *devname;
    DeviceType type;
    Boolean enabled;
    Boolean (*init)(struct _device *dev);
    FILE * (*get)(struct _device *dev, char *file, Boolean probe);
    void (*shutdown)(struct _device *dev);
    void *private;
    unsigned int flags;
    unsigned int volume;
} Device;

/* Some internal representations of partitions */
typedef enum {
    PART_NONE,
    PART_SLICE,
    PART_SWAP,
    PART_FILESYSTEM,
    PART_FAT,
    PART_EFI
} PartType;

#define	NEWFS_UFS_CMD		"newfs"
#define	NEWFS_MSDOS_CMD		"newfs_msdos"

enum newfs_type { NEWFS_UFS, NEWFS_MSDOS, NEWFS_CUSTOM };
#define	NEWFS_UFS_STRING	"UFS"
#define	NEWFS_MSDOS_STRING	"FAT"
#define	NEWFS_CUSTOM_STRING	"CST"

/* The longest set of custom command line arguments we'll pass. */
#define NEWFS_CMD_ARGS_MAX	256

typedef struct _part_info {
	char mountpoint[FILENAME_MAX];

	/* Is invocation of newfs desired? */
	Boolean do_newfs;

	enum newfs_type newfs_type;
	union {
		struct {
			char user_options[NEWFS_CMD_ARGS_MAX];
			Boolean acls;		/* unused */
			Boolean multilabel;	/* unused */
			Boolean softupdates;
			Boolean ufs1;
		} newfs_ufs;
		struct {
			/* unused */
		} newfs_msdos;
		struct {
			char command[NEWFS_CMD_ARGS_MAX];
		} newfs_custom;
	} newfs_data;
} PartInfo;

/* An option */
typedef struct _opt {
    char *name;
    char *desc;
    enum { OPT_IS_STRING, OPT_IS_INT, OPT_IS_FUNC, OPT_IS_VAR } type;
    void *data;
    void *aux;
    char *(*check)(void);
} Option;

typedef int (*commandFunc)(char *key, void *data);

#define EXTRAS_FIELD_LEN	128

/*** Externs ***/
extern jmp_buf		BailOut;		/* Used to get the heck out */
extern int		DebugFD;		/* Where diagnostic output goes			*/
extern Boolean		Fake;			/* Don't actually modify anything - testing	*/
extern Boolean		Restarting;		/* Are we restarting sysinstall?		*/
extern Boolean		SystemWasInstalled;	/* Did we install it?				*/
extern Boolean		RunningAsInit;		/* Are we running stand-alone?			*/
extern Boolean		DialogActive;		/* Is the dialog() stuff up?			*/
extern Boolean		ColorDisplay;		/* Are we on a color display?			*/
extern Boolean		OnVTY;			/* On a syscons VTY?				*/
extern Variable		*VarHead;		/* The head of the variable chain		*/
extern int		BootMgr;		/* Which boot manager to use 			*/
extern int		StatusLine;		/* Where to print our status messages		*/
#if defined(__i386__) || defined(__amd64__)
#ifdef PC98
extern DMenu		MenuIPLType;		/* Type of IPL to write on the disk		*/
#else
extern DMenu		MenuMBRType;		/* Type of MBR to write on the disk		*/
#endif
#endif
extern DMenu		MenuMain;       /* New main menu */
extern DMenu    MenuDiskDevices;        /* Disk type devices                            */
extern const char *	StartName;		/* Which name we were started as */
extern const char *	ProgName;		/* Program's proper name */

/* Important chunks. */
extern Chunk *HomeChunk;
extern Chunk *RootChunk;
extern Chunk *SwapChunk;
extern Chunk *TmpChunk;
extern Chunk *UsrChunk;
extern Chunk *VarChunk;
#ifdef __ia64__
extern Chunk *EfiChunk;
#endif

/* Stuff from libdialog which isn't properly declared outside */
extern void display_helpfile(void);
extern void display_helpline(WINDOW *w, int y, int width);

/*** Prototypes ***/

/* command.c */
extern void	command_clear(void);
extern void	command_sort(void);
extern void	command_execute(void);
extern void	command_shell_add(char *key, const char *fmt, ...) __printflike(2, 3);
extern void	command_func_add(char *key, commandFunc func, void *data);

/* config.c */
extern void	configEnvironmentRC_conf(void);
extern void	configRC_conf(void);
extern int	configFstab(dialogMenuItem *self);
extern int	configRC(dialogMenuItem *self);
extern int	configWriteRC_conf(dialogMenuItem *self);

/* devices.c */
extern DMenu	*deviceCreateMenu(DMenu *menu, DeviceType type, int (*hook)(dialogMenuItem *d),
				  int (*check)(dialogMenuItem *d));
extern void	deviceGetAll(void);
extern void	deviceReset(void);
extern void	deviceRescan(void);
extern Device	**deviceFind(char *name, DeviceType type);
extern Device	**deviceFindDescr(char *name, char *desc, DeviceType class);
extern int	deviceCount(Device **devs);
extern Device	*new_device(char *name);
extern Device	*deviceRegister(char *name, char *desc, char *devicename, DeviceType type, Boolean enabled,
				Boolean (*init)(Device *mediadev),
				FILE * (*get)(Device *dev, char *file, Boolean probe),
				void (*shutDown)(Device *mediadev),
				void *private);
extern Boolean	dummyInit(Device *dev);
extern FILE	*dummyGet(Device *dev, char *dist, Boolean probe);
extern void	dummyShutdown(Device *dev);

/* disks.c */
#ifdef WITH_SLICES
extern void	diskPartition(Device *dev);
extern int	diskPartitionEditor(dialogMenuItem *self);
#endif
extern int	diskPartitionWrite(dialogMenuItem *self);
extern int	diskGetSelectCount(Device ***devs);

/* dispatch.c */
extern int	dispatchCommand(char *command);
extern int	dispatch_load_floppy(dialogMenuItem *self);
extern int	dispatch_load_file_int(int);
extern int	dispatch_load_file(dialogMenuItem *self);

/* dmenu.c */
extern int	dmenuDisplayFile(dialogMenuItem *tmp);
extern int	dmenuSubmenu(dialogMenuItem *tmp);
extern int	dmenuSystemCommand(dialogMenuItem *tmp);
extern int	dmenuSystemCommandBox(dialogMenuItem *tmp);
extern int	dmenuExit(dialogMenuItem *tmp);
extern int	dmenuISetVariable(dialogMenuItem *tmp);
extern int	dmenuSetVariable(dialogMenuItem *tmp);
extern int	dmenuSetVariables(dialogMenuItem *tmp);
extern int	dmenuToggleVariable(dialogMenuItem *tmp);
extern int	dmenuSetFlag(dialogMenuItem *tmp);
extern int	dmenuSetValue(dialogMenuItem *tmp);
extern Boolean	dmenuOpen(DMenu *menu, int *choice, int *bscroll, int *curr, int *max, Boolean buttons);
extern Boolean	dmenuOpenSimple(DMenu *menu, Boolean buttons);
extern int	dmenuVarCheck(dialogMenuItem *item);
extern int	dmenuVarsCheck(dialogMenuItem *item);
extern int	dmenuFlagCheck(dialogMenuItem *item);
extern int	dmenuRadioCheck(dialogMenuItem *item);

/* dos.c */
extern Boolean mediaCloseDOS(Device *dev, FILE *fp);
extern Boolean mediaInitDOS(Device *dev);
extern FILE    *mediaGetDOS(Device *dev, char *file, Boolean probe);
extern void    mediaShutdownDOS(Device *dev);

/* globals.c */
extern void	globalsInit(void);

/* install.c */
extern Boolean	checkLabels(Boolean whinge);
extern int	installCommit(dialogMenuItem *self);
extern int	installCustomCommit(dialogMenuItem *self);
extern int	installFilesystems(dialogMenuItem *self);
extern int	installVarDefaults(dialogMenuItem *self);
extern void	installEnvironment(void);
extern Boolean	copySelf(void);

/* kget.c */
extern int	kget(char *out);

/* label.c */
extern int	diskLabelEditor(dialogMenuItem *self);
extern int	diskLabelCommit(dialogMenuItem *self);

/* misc.c */
extern Boolean	file_readable(char *fname);
extern Boolean	file_executable(char *fname);
extern Boolean	directory_exists(const char *dirname);
extern char	*root_bias(char *path);
extern char	*itoa(int value);
extern char	*string_concat(char *p1, char *p2);
extern char	*string_concat3(char *p1, char *p2, char *p3);
extern char	*string_prune(char *str);
extern char	*string_skipwhite(char *str);
extern char	*string_copy(char *s1, char *s2);
extern char	*pathBaseName(const char *path);
extern void	safe_free(void *ptr);
extern void	*safe_malloc(size_t size);
extern void	*safe_realloc(void *orig, size_t size);
extern dialogMenuItem *item_add(dialogMenuItem *list, char *prompt, char *title, 
				int (*checked)(dialogMenuItem *self),
				int (*fire)(dialogMenuItem *self),
				void (*selected)(dialogMenuItem *self, int is_selected),
				void *data, int *aux, int *curr, int *max);
extern void	items_free(dialogMenuItem *list, int *curr, int *max);
extern int	Mkdir(char *);
extern int	Mkdir_command(char *key, void *data);
extern int	Mount(char *, void *data);
extern WINDOW	*openLayoutDialog(char *helpfile, char *title, int x, int y, int width, int height);
extern ComposeObj *initLayoutDialog(WINDOW *win, Layout *layout, int x, int y, int *max);
extern int	layoutDialogLoop(WINDOW *win, Layout *layout, ComposeObj **obj,
				 int *n, int max, int *cbutton, int *cancel);

extern WINDOW	*savescr(void);
extern void	restorescr(WINDOW *w);
extern char	*sstrncpy(char *dst, const char *src, int size);

/* msg.c */
extern Boolean	isDebug(void);
extern void	msgInfo(const char *fmt, ...) __printf0like(1, 2);
extern void	msgYap(const char *fmt, ...) __printflike(1, 2);
extern void	msgWarn(const char *fmt, ...) __printflike(1, 2);
extern void	msgDebug(const char *fmt, ...) __printflike(1, 2);
extern void	msgError(const char *fmt, ...) __printflike(1, 2);
extern void	msgFatal(const char *fmt, ...) __printflike(1, 2);
extern void	msgConfirm(const char *fmt, ...) __printflike(1, 2);
extern void	msgNotify(const char *fmt, ...) __printflike(1, 2);
extern void	msgWeHaveOutput(const char *fmt, ...) __printflike(1, 2);
extern int	msgYesNo(const char *fmt, ...) __printflike(1, 2);
extern int	msgNoYes(const char *fmt, ...) __printflike(1, 2);
extern char	*msgGetInput(char *buf, const char *fmt, ...) __printflike(2, 3);
extern int	msgSimpleConfirm(const char *);
extern int	msgSimpleNotify(const char *);

/* pccard.c */
extern void	pccardInitialize(void);

/* system.c */
extern void	systemInitialize(int argc, char **argv);
extern void	systemShutdown(int status);
extern int	execExecute(char *cmd, char *name);
extern int	systemExecute(char *cmd);
extern void	systemSuspendDialog(void);
extern void	systemResumeDialog(void);
extern int	systemDisplayHelp(char *file);
extern char	*systemHelpFile(char *file, char *buf);
extern void	systemChangeFont(const u_char font[]);
extern void	systemChangeLang(char *lang);
extern void	systemChangeTerminal(char *color, const u_char c_termcap[], char *mono, const u_char m_termcap[]);
extern void	systemChangeScreenmap(const u_char newmap[]);
extern int	vsystem(const char *fmt, ...) __printflike(1, 2);

/* termcap.c */
extern int	set_termcap(void);

/* variable.c */
extern void	variable_set(char *var, int dirty);
extern void	variable_set2(char *name, char *value, int dirty);
extern char 	*variable_get(char *var);
extern int 	variable_cmp(char *var, char *value);
extern void	variable_unset(char *var);
extern char	*variable_get_value(char *var, char *prompt, int dirty);
extern int 	variable_check(char *data);
extern int 	variable_check2(char *data);
extern int	dump_variables(dialogMenuItem *self);
extern void	free_variables(void);
extern void     pvariable_set(char *var);
extern char     *pvariable_get(char *var);

/* wizard.c */
extern void	slice_wizard(Disk *d);

/*
 * Macros.  Please find a better place for us!
 */
#define DEVICE_INIT(d)		((d) != NULL ? (d)->init((d)) : (Boolean)0)
#define DEVICE_GET(d, b, f)	((d) != NULL ? (d)->get((d), (b), (f)) : NULL)
#define DEVICE_SHUTDOWN(d)	((d) != NULL ? (d)->shutdown((d)) : (void)0)

#endif
/* _SYSINSTALL_H_INCLUDE */
