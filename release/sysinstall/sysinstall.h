/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated to essentially a complete rewrite.
 *
 * $Id: sysinstall.h,v 1.42.2.5 1995/10/03 23:36:55 jkh Exp $
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
#include <sys/types.h>
#include <sys/wait.h>
#include "libdisk.h"
#include "dist.h"
#include "version.h"

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
#define MAX_FTP_RETRIES		3	/* How many times to beat our heads against the wall */

/*
 * I make some pretty gross assumptions about having a max of 50 chunks
 * total - 8 slices and 42 partitions.  I can't easily display many more
 * than that on the screen at once!
 *
 * For 2.1 I'll revisit this and try to make it more dynamic, but since
 * this will catch 99.99% of all possible cases, I'm not too worried.
 */
#define MAX_CHUNKS	50

/* Internal environment variables */
#define DISK_PARTITIONED	"_diskPartitioned"
#define DISK_LABELLED		"_diskLabelled"
#define RUNNING_ON_ROOT		"_runningOnRoot"
#define TCP_CONFIGURED		"_tcpConfigured"
#define TAPE_BLOCKSIZE		"_tapeBlocksize"
#define FTP_USER		"_ftpUser"
#define FTP_PASS		"_ftpPass"
#define RELNAME			"_releaseName"

#define DEFAULT_TAPE_BLOCKSIZE	"10"

#define OPT_NO_CONFIRM		0x0001
#define OPT_NFS_SECURE		0x0002
#define OPT_DEBUG		0x0004
#define OPT_FTP_ACTIVE		0x0008
#define OPT_FTP_PASSIVE		0x0010
#define OPT_FTP_RESELECT	0x0020
#define OPT_FTP_ABORT		0x0040
#define OPT_SLOW_ETHER		0x0080
#define OPT_EXPLORATORY_GET	0x0100
#define OPT_LEAVE_NETWORK_UP	0x0200
#define OPT_CPIO_HIGH		0x0400

#define OPT_DEFAULT_FLAGS	(OPT_FTP_PASSIVE | OPT_FTP_ABORT| OPT_CPIO_HIGH)

#define VAR_HOSTNAME		"hostname"
#define VAR_DOMAINNAME		"domainname"
#define VAR_NAMESERVER		"nameserver"
#define VAR_GATEWAY		"defaultrouter"
#define VAR_IPADDR		"ipaddr"

#define VAR_IFCONFIG		"ifconfig_"
#define VAR_INTERFACES		"network_interfaces"

/* The help file for the TCP/IP setup screen */
#define TCP_HELPFILE		"tcp"

/*** Types ***/
typedef unsigned int Boolean;
typedef struct disk Disk;
typedef struct chunk Chunk;

typedef enum {
    DMENU_DISPLAY_FILE,			/* Display a file's contents	*/
    DMENU_SUBMENU,			/* Recurse into another menu	*/
    DMENU_SYSTEM_COMMAND,		/* Run shell commmand		*/
    DMENU_SYSTEM_COMMAND_BOX,		/* Same as above, but in prgbox	*/
    DMENU_SET_VARIABLE,			/* Set an environment/system var */
    DMENU_SET_FLAG,			/* Set flag in an unsigned int	*/
    DMENU_SET_VALUE,			/* Set unsigned int to value	*/
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
    char * (*check)(struct _dmenuItem *); /* Our state                  */
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

#define MAX_ATTRIBS	200
#define MAX_NAME	511
#define MAX_VALUE	4095

typedef struct _attribs {
    char *name;
    char *value;
} Attribs;

typedef enum {
    DEVICE_TYPE_NONE,
    DEVICE_TYPE_DISK,
    DEVICE_TYPE_FLOPPY,
    DEVICE_TYPE_FTP,
    DEVICE_TYPE_NETWORK,
    DEVICE_TYPE_CDROM,
    DEVICE_TYPE_TAPE,
    DEVICE_TYPE_DOS,
    DEVICE_TYPE_UFS,
    DEVICE_TYPE_NFS,
    DEVICE_TYPE_ANY,
} DeviceType;

/* A "device" from sysinstall's point of view */
typedef struct _device {
    char name[DEV_NAME_MAX];
    char *description;
    char *devname;
    DeviceType type;
    Boolean enabled;
    Boolean (*init)(struct _device *dev);
    int (*get)(struct _device *dev, char *file, Attribs *dist_attrs);
    Boolean (*close)(struct _device *dev, int fd);
    void (*shutdown)(struct _device *dev);
    void *private;
    unsigned int flags;
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

/* An option */
typedef struct _opt {
    char *name;
    char *desc;
    enum { OPT_IS_STRING, OPT_IS_INT, OPT_IS_FLAG, OPT_IS_FUNC } type;
    void *data;
    int aux;
    char * (*check)();
} Option;

typedef int (*commandFunc)(char *key, void *data);

#define HOSTNAME_FIELD_LEN	256
#define IPADDR_FIELD_LEN	16
#define EXTRAS_FIELD_LEN	256

/* Verbosity levels for CPIO - yuck */
#define CPIO_VERBOSITY		(optionIsSet(OPT_CPIO_HIGH) ? "-v" : "-V")

/* This is the structure that Network devices carry around in their private, erm, structures */
typedef struct _devPriv {
    char ipaddr[IPADDR_FIELD_LEN];
    char netmask[IPADDR_FIELD_LEN];
    char extras[EXTRAS_FIELD_LEN];
} DevInfo;


/*** Externs ***/
extern int		DebugFD;		/* Where diagnostic output goes			*/
extern Boolean		SystemWasInstalled;	/* Did we install it?				*/
extern Boolean		RunningAsInit;		/* Are we running stand-alone?			*/
extern Boolean		DialogActive;		/* Is the dialog() stuff up?			*/
extern Boolean		ColorDisplay;		/* Are we on a color display?			*/
extern Boolean		OnVTY;			/* On a syscons VTY?				*/
extern Variable		*VarHead;		/* The head of the variable chain		*/
extern Device		*mediaDevice;		/* Where we're getting our distribution from	*/
extern unsigned int	Dists;			/* Which distributions we want			*/
extern unsigned int	DESDists;		/* Which naughty distributions we want		*/
extern unsigned int	SrcDists;		/* Which src distributions we want		*/
extern unsigned int	XF86Dists;		/* Which XFree86 dists we want			*/
extern unsigned int	XF86ServerDists;	/* The XFree86 servers we want			*/
extern unsigned int	XF86FontDists;		/* The XFree86 fonts we want			*/
extern unsigned int	OptFlags;		/* Global options */
extern int		BootMgr;		/* Which boot manager to use 			*/
extern char		*InstallPrefix;		/* A location bias				*/


extern DMenu		MenuInitial;		/* Initial installation menu			*/
extern DMenu		MenuMBRType;		/* Type of MBR to write on the disk		*/
extern DMenu		MenuConfigure;		/* Final configuration menu			*/
extern DMenu		MenuDocumentation;	/* Documentation menu				*/
extern DMenu		MenuFTPOptions;		/* FTP Installation options			*/
extern DMenu		MenuOptions;		/* Installation options				*/
extern DMenu		MenuOptionsLanguage;	/* Language options menu			*/
extern DMenu		MenuMedia;		/* Media type menu				*/
extern DMenu		MenuMediaCDROM;		/* CDROM media menu				*/
extern DMenu		MenuMediaDOS;		/* DOS media menu				*/
extern DMenu		MenuMediaFloppy;	/* Floppy media menu				*/
extern DMenu		MenuMediaFTP;		/* FTP media menu				*/
extern DMenu		MenuMediaTape;		/* Tape media menu				*/
extern DMenu		MenuNetworkDevice;	/* Network device menu				*/
extern DMenu		MenuNTP;		/* NTP time server menu				*/
extern DMenu		MenuSyscons;		/* System console configuration menu		*/
extern DMenu		MenuSysconsKeymap;	/* System console keymap configuration menu	*/
extern DMenu		MenuSysconsKeyrate;	/* System console keyrate configuration menu	*/
extern DMenu		MenuSysconsSaver;	/* System console saver configuration menu	*/
extern DMenu		MenuNetworking;		/* Network configuration menu			*/
extern DMenu		MenuInstallCustom;	/* Custom Installation menu			*/
extern DMenu		MenuInstallType;	/* Installation type menu			*/
extern DMenu		MenuDistributions;	/* Distribution menu				*/
extern DMenu		MenuDESDistributions;	/* DES distribution menu			*/
extern DMenu		MenuSrcDistributions;	/* Source distribution menu			*/
extern DMenu		MenuXF86;		/* XFree86 main menu				*/
extern DMenu		MenuXF86Select;		/* XFree86 distribution selection menu		*/
extern DMenu		MenuXF86SelectCore;	/* XFree86 core distribution menu		*/
extern DMenu		MenuXF86SelectServer;	/* XFree86 server distribution menu		*/
extern DMenu		MenuXF86SelectFonts;	/* XFree86 font selection menu			*/
extern DMenu		MenuDiskDevices;	/* Disk devices menu				*/


/*** Prototypes ***/

/* attrs.c */
extern const char	*attr_match(Attribs *attr, char *name);
extern int		attr_parse(Attribs **attr, char *file);

/* cdrom.c */
extern Boolean	mediaInitCDROM(Device *dev);
extern int	mediaGetCDROM(Device *dev, char *file, Attribs *dist_attrs);
extern void	mediaShutdownCDROM(Device *dev);

/* command.c */
extern void	command_clear(void);
extern void	command_sort(void);
extern void	command_execute(void);
extern void	command_shell_add(char *key, char *fmt, ...);
extern void	command_func_add(char *key, commandFunc func, void *data);

/* config.c */
extern void	configFstab(void);
extern void	configSysconfig(void);
extern void	configResolv(void);
extern int	configPorts(char *str);
extern int	configPackages(char *str);
extern int	configSaverTimeout(char *str);
extern int	configNTP(char *str);
extern int	configRoutedFlags(char *str);

/* crc.c */
extern int	crc(int, unsigned long *, unsigned long *);

/* decode.c */
extern DMenuItem *decode(DMenu *menu, char *name);
extern Boolean	dispatch(DMenuItem *tmp, char *name);
extern Boolean	decode_and_dispatch_multiple(DMenu *menu, char *names);

/* devices.c */
extern DMenu	*deviceCreateMenu(DMenu *menu, DeviceType type, int (*hook)());
extern void	deviceGetAll(void);
extern Device	**deviceFind(char *name, DeviceType type);
extern int	deviceCount(Device **devs);
extern Device	*new_device(char *name);
extern Device	*deviceRegister(char *name, char *desc, char *devname, DeviceType type, Boolean enabled,
				Boolean (*init)(Device *mediadev), int (*get)(Device *dev, char *file, Attribs *dist_attrs),
				Boolean (*close)(Device *mediadev, int fd), void (*shutDown)(Device *mediadev),
				void *private);
extern Boolean	dummyInit(Device *dev);
extern int	dummyGet(Device *dev, char *dist, Attribs *dist_attrs);
extern Boolean	dummyClose(Device *dev, int fd);
extern void	dummyShutdown(Device *dev);

/* disks.c */
extern int	diskPartitionEditor(char *unused);
extern int	diskPartitionWrite(char *unused);

/* dist.c */
extern int	distReset(char *str);
extern int	distSetDeveloper(char *str);
extern int	distSetXDeveloper(char *str);
extern int	distSetKernDeveloper(char *str);
extern int	distSetUser(char *str);
extern int	distSetXUser(char *str);
extern int	distSetMinimum(char *str);
extern int	distSetEverything(char *str);
extern int	distSetDES(char *str);
extern int	distSetSrc(char *str);
extern int	distSetXF86(char *str);
extern int	distExtractAll(char *str);

/* dmenu.c */
extern Boolean	dmenuOpen(DMenu *menu, int *choice, int *scroll, int *curr, int *max);
extern Boolean	dmenuOpenSimple(DMenu *menu);
extern char     *dmenuVarCheck(DMenuItem *item);
extern char     *dmenuFlagCheck(DMenuItem *item);
extern char     *dmenuRadioCheck(DMenuItem *item);

/* dos.c */
extern Boolean	mediaInitDOS(Device *dev);
extern int	mediaGetDOS(Device *dev, char *file, Attribs *dist_attrs);
extern void	mediaShutdownDOS(Device *dev);

/* floppy.c */
extern int	getRootFloppy(void);
extern Boolean	mediaInitFloppy(Device *dev);
extern int	mediaGetFloppy(Device *dev, char *file, Attribs *dist_attrs);
extern void	mediaShutdownFloppy(Device *dev);

/* ftp_strat.c */
extern Boolean	mediaCloseFTP(Device *dev, int fd);
extern Boolean	mediaInitFTP(Device *dev);
extern int	mediaGetFTP(Device *dev, char *file, Attribs *dist_attrs);
extern void	mediaShutdownFTP(Device *dev);

/* globals.c */
extern void	globalsInit(void);

/* install.c */
extern int	installCommit(char *str);
extern int	installExpress(char *str);
extern int	installFixit(char *str);
extern int	installSelectRelease(char *str);
extern Boolean	installFixup(void);
extern Boolean	installFilesystems(void);

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
extern int	diskLabelEditor(char *str);
extern int	diskLabelCommit(char *str);

/* makedevs.c (auto-generated) */
extern const char	termcap_vt100[];
extern const char	termcap_cons25[];
extern const char	termcap_cons25_m[];
extern const char	termcap_cons25r[];
extern const char	termcap_cons25r_m[];
extern const char	termcap_cons25l1[];
extern const char	termcap_cons25l1_m[];
extern const u_char	font_iso_8x16[];
extern const u_char	font_cp850_8x16[];
extern const u_char	font_cp866_8x16[];
extern const u_char	koi8_r2cp866[];
extern u_char		default_scrnmap[];

/* media.c */
extern int	mediaSetCDROM(char *str);
extern int	mediaSetFloppy(char *str);
extern int	mediaSetDOS(char *str);
extern int	mediaSetTape(char *str);
extern int	mediaSetFTP(char *str);
extern int	mediaSetFTPActive(char *str);
extern int	mediaSetFTPPassive(char *str);
extern int	mediaSetUFS(char *str);
extern int	mediaSetNFS(char *str);
extern int	mediaSetFtpUserPass(char *str);
extern int	mediaSetTapeBlocksize(char *str);
extern Boolean	mediaGetType(void);
extern Boolean	mediaExtractDist(char *dir, int fd);
extern Boolean	mediaExtractDistBegin(char *dir, int *fd, int *zpid, int *cpic);
extern Boolean	mediaExtractDistEnd(int zpid, int cpid);
extern Boolean	mediaVerify(void);

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
extern int	Mkdir(char *, void *data);
extern int	Mount(char *, void *data);

/* msg.c */
extern Boolean	isDebug(void);
extern void	msgInfo(char *fmt, ...);
extern void	msgYap(char *fmt, ...);
extern void	msgWarn(char *fmt, ...);
extern void	msgDebug(char *fmt, ...);
extern void	msgError(char *fmt, ...);
extern void	msgFatal(char *fmt, ...);
extern void	msgConfirm(char *fmt, ...);
extern void	msgNotify(char *fmt, ...);
extern void	msgWeHaveOutput(char *fmt, ...);
extern int	msgYesNo(char *fmt, ...);
extern char	*msgGetInput(char *buf, char *fmt, ...);

/* network.c */
extern Boolean	mediaInitNetwork(Device *dev);
extern void	mediaShutdownNetwork(Device *dev);

/* nfs.c */
extern Boolean	mediaInitNFS(Device *dev);
extern int	mediaGetNFS(Device *dev, char *file, Attribs *dist_attrs);
extern void	mediaShutdownNFS(Device *dev);

/* options.c */
extern int	optionsEditor(char *str);
extern Boolean	optionIsSet(int opt);

/* system.c */
extern void	systemInitialize(int argc, char **argv);
extern void	systemShutdown(void);
extern int	execExecute(char *cmd, char *name);
extern int	systemExecute(char *cmd);
extern int	systemDisplayFile(char *file);
extern char	*systemHelpFile(char *file, char *buf);
extern void	systemChangeFont(const u_char font[]);
extern void	systemChangeLang(char *lang);
extern void	systemChangeTerminal(char *color, const u_char c_termcap[],
				     char *mono, const u_char m_termcap[]);
extern void	systemChangeScreenmap(const u_char newmap[]);
extern int	vsystem(char *fmt, ...);

/* tape.c */
extern char *	mediaTapeBlocksize(void);
extern Boolean	mediaInitTape(Device *dev);
extern int	mediaGetTape(Device *dev, char *file, Attribs *dist_attrs);
extern void	mediaShutdownTape(Device *dev);

/* tcpip.c */
extern int	tcpOpenDialog(Device *dev);
extern int	tcpMenuSelect(char *str);
extern Boolean	tcpDeviceSelect(void);

/* termcap.c */
extern int	set_termcap(void);

/* ufs.c */
extern void	mediaShutdownUFS(Device *dev);
extern Boolean	mediaInitUFS(Device *dev);
extern int	mediaGetUFS(Device *dev, char *file, Attribs *dist_attrs);

/* variables.c */
extern void	variable_set(char *var);
extern void	variable_set2(char *name, char *value);

/* wizard.c */
extern void	slice_wizard(Disk *d);

#endif
/* _SYSINSTALL_H_INCLUDE */
