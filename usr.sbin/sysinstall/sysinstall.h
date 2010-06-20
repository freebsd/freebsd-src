/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated to essentially a complete rewrite.
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

#ifndef _SYSINSTALL_H_INCLUDE
#define _SYSINSTALL_H_INCLUDE

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
#include "dist.h"

/*** Defines ***/

#if defined(__i386__) || defined(__amd64__)
#define	WITH_SYSCONS
#define	WITH_MICE
#endif

#if defined(__i386__) || defined(__amd64__)
#define	WITH_SLICES
#endif

/* device limits */
#define DEV_NAME_MAX		128	/* The maximum length of a device name	*/
#define DEV_MAX			100	/* The maximum number of devices we'll deal with */
#define INTERFACE_MAX		50	/* Maximum number of network interfaces we'll deal with */
#define IO_ERROR		-2	/* Status code for I/O error rather than normal EOF */

/* Number of seconds to wait for data to come off even the slowest media */
#define MEDIA_TIMEOUT		300

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
#define TCP_CONFIGURED			"_tcpConfigured"

/* Ones that can be tweaked from config files */
#define VAR_BLANKTIME			"blanktime"
#define VAR_BOOTMGR			"bootManager"
#define VAR_BROWSER_BINARY		"browserBinary"
#define VAR_BROWSER_PACKAGE		"browserPackage"
#define VAR_COUNTRY			"country"
#define VAR_CPIO_VERBOSITY		"cpioVerbose"
#define VAR_DEBUG			"debug"
#define VAR_DESKSTYLE			"_deskStyle"
#define VAR_DISK			"disk"
#define VAR_DISKINTERACTIVE		"diskInteractive"
#define VAR_DISTS			"dists"
#define VAR_DIST_MAIN			"distMain"
#define VAR_DIST_SRC			"distSRC"
#define VAR_DIST_KERNEL			"distKernel"
#define VAR_DIST_DOC			"distDoc"
#define VAR_DEDICATE_DISK		"dedicateDisk"
#define VAR_DOMAINNAME			"domainname"
#define VAR_EDITOR			"editor"
#define VAR_EXTRAS			"ifconfig_"
#define VAR_COMMAND			"command"
#define VAR_CONFIG_FILE			"configFile"
#define VAR_FIXIT_TTY			"fixitTty"
#define VAR_FTP_DIR			"ftpDirectory"
#define VAR_FTP_PASS			"ftpPass"
#define VAR_FTP_PATH			"_ftpPath"
#define VAR_FTP_PORT			"ftpPort"
#define VAR_FTP_STATE			"ftpState"
#define VAR_FTP_USER			"ftpUser"
#define VAR_FTP_HOST			"ftpHost"
#define VAR_HTTP_PATH			"_httpPath"
#define VAR_HTTP_PROXY			"httpProxy"
#define VAR_HTTP_PORT			"httpPort"
#define VAR_HTTP_HOST			"httpHost"
#define VAR_HTTP_FTP_MODE		"httpFtpMode"
#define VAR_GATEWAY			"defaultrouter"
#define VAR_GEOMETRY			"geometry"
#define VAR_HOSTNAME			"hostname"
#define VAR_IFCONFIG			"ifconfig_"
#define VAR_INSTALL_CFG			"installConfig"
#define VAR_INSTALL_ROOT		"installRoot"
#define VAR_IPADDR			"ipaddr"
#define VAR_IPV6_ENABLE			"ipv6_enable"
#define VAR_IPV6ADDR			"ipv6addr"
#define VAR_KERN_SECURELEVEL		"kern_securelevel"
#define VAR_KEYMAP			"keymap"
#define VAR_LABEL			"label"
#define VAR_LABEL_COUNT			"labelCount"
#define VAR_LINUX_ENABLE		"linux_enable"
#define VAR_MEDIA_TYPE			"mediaType"
#define VAR_MEDIA_TIMEOUT		"MEDIA_TIMEOUT"
#define VAR_MOUSED			"moused_enable"
#define VAR_MOUSED_FLAGS                "moused_flags"
#define VAR_MOUSED_PORT			"moused_port"
#define VAR_MOUSED_TYPE			"moused_type"
#define VAR_NAMESERVER			"nameserver"
#define	VAR_NCPUS			"ncpus"
#define VAR_NETINTERACTIVE		"netInteractive"
#define VAR_NETMASK			"netmask"
#define VAR_NETWORK_DEVICE		"netDev"
#define VAR_NEWFS_ARGS			"newfsArgs"
#define VAR_NFS_PATH			"nfs"
#define VAR_NFS_HOST			"nfsHost"
#define VAR_NFS_V3			"nfs_use_v3"
#define VAR_NFS_TCP			"nfs_use_tcp"
#define VAR_NFS_SECURE			"nfs_reserved_port_only"
#define VAR_NFS_SERVER			"nfs_server_enable"
#define VAR_NO_CONFIRM			"noConfirm"
#define VAR_NO_ERROR			"noError"
#define VAR_NO_HOLOSHELL		"noHoloShell"
#define VAR_NO_INET6			"noInet6"
#define VAR_NO_WARN			"noWarn"
#define VAR_NO_USR			"noUsr"
#define VAR_NO_TMP			"noTmp"
#define VAR_NO_HOME			"noHome"
#define VAR_NONINTERACTIVE		"nonInteractive"
#define VAR_NOVELL			"novell"
#define	VAR_RPCBIND_ENABLE		"rpcbind_enable"
#define VAR_NTPDATE_FLAGS		"ntpdate_flags"
#define VAR_PACKAGE			"package"
#define VAR_PARTITION			"partition"
#define VAR_PCNFSD			"pcnfsd"
#define VAR_PKG_TMPDIR			"PKG_TMPDIR"
#define VAR_PORTS_PATH			"ports"
#define VAR_PPP_ENABLE			"ppp_enable"
#define VAR_PPP_PROFILE			"ppp_profile"
#define VAR_RELNAME			"releaseName"
#define VAR_ROOT_SIZE			"rootSize"
#define VAR_ROUTER			"router"
#define VAR_ROUTER_ENABLE		"router_enable"
#define VAR_ROUTERFLAGS			"router_flags"
#define VAR_SENDMAIL_ENABLE		"sendmail_enable"
#define VAR_SERIAL_SPEED		"serialSpeed"
#define VAR_SLOW_ETHER			"slowEthernetCard"
#define VAR_SWAP_SIZE			"swapSize"
#define VAR_TRY_DHCP			"tryDHCP"
#define VAR_TRY_RTSOL			"tryRTSOL"
#define VAR_UFS_PATH			"ufs"
#define VAR_USR_SIZE			"usrSize"
#define VAR_VAR_SIZE			"varSize"
#define VAR_TMP_SIZE			"tmpSize"
#define VAR_HOME_SIZE			"homeSize"
#define VAR_TERM			"TERM"
#define VAR_CONSTERM                    "_consterm"
#define VAR_KEEPRCCONF			"keeprcconf"

#ifdef PC98
#define DEFAULT_COUNTRY		"jp"
#else
#define DEFAULT_COUNTRY		"us"
#endif

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

/* Layout array terminator. */
#define	LAYOUT_END		{ 0, 0, 0, 0, NULL, NULL, NULL, 0, NULL }

typedef enum {
    DEVICE_TYPE_NONE,
    DEVICE_TYPE_DISK,
    DEVICE_TYPE_FLOPPY,
    DEVICE_TYPE_FTP,
    DEVICE_TYPE_NETWORK,
    DEVICE_TYPE_CDROM,
    DEVICE_TYPE_USB,
    DEVICE_TYPE_DOS,
    DEVICE_TYPE_UFS,
    DEVICE_TYPE_NFS,
    DEVICE_TYPE_ANY,
    DEVICE_TYPE_HTTP,
} DeviceType;

/* CDROM mount codes */
#define CD_UNMOUNTED		0
#define CD_ALREADY_MOUNTED	1
#define CD_WE_MOUNTED_IT	2

/* A "device" from sysinstall's point of view */
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
    char *(*check)(struct _opt *);
} Option;

/* Weird index nodey things we use for keeping track of package information */
typedef enum { PACKAGE, PLACE } node_type;	/* Types of nodes */

typedef struct _pkgnode {	/* A node in the reconstructed hierarchy */
    struct _pkgnode *next;	/* My next sibling			*/
    node_type type;		/* What am I?				*/
    char *name;			/* My name				*/
    char *desc;			/* My description (Hook)		*/
    struct _pkgnode *kids;	/* My little children			*/
    void *data;			/* A place to hang my data		*/
} PkgNode;
typedef PkgNode *PkgNodePtr;

/* A single package */
typedef struct _indexEntry {	/* A single entry in an INDEX file */
    char *name;			/* name				*/
    char *path;			/* full path to port		*/
    char *prefix;		/* port prefix			*/
    char *comment;		/* one line description		*/
    char *descrfile;		/* path to description file	*/
    char *deps;			/* packages this depends on	*/
    int  depc;			/* how many depend on me	*/
    int  installed;		/* indicates if it is installed */
    int  vol_checked;		/* disc volume last checked for */
    char *maintainer;		/* maintainer			*/
    unsigned int volume;	/* Volume of package            */
} IndexEntry;
typedef IndexEntry *IndexEntryPtr;

typedef int (*commandFunc)(char *key, void *data);

#define HOSTNAME_FIELD_LEN	128
#define IPADDR_FIELD_LEN	16
#define EXTRAS_FIELD_LEN	128

/* This is the structure that Network devices carry around in their private, erm, structures */
typedef struct _devPriv {
    int use_rtsol;
    int use_dhcp;
    char ipaddr[IPADDR_FIELD_LEN];
    char netmask[IPADDR_FIELD_LEN];
    char extras[EXTRAS_FIELD_LEN];
} DevInfo;


/*** Externs ***/
extern jmp_buf		BailOut;		/* Used to get the heck out */
extern int		CDROMInitQuiet;		/* Don't whine if mount(2) fails */
extern int		DebugFD;		/* Where diagnostic output goes			*/
extern Boolean		Fake;			/* Don't actually modify anything - testing	*/
extern Boolean		Restarting;		/* Are we restarting sysinstall?		*/
extern Boolean		SystemWasInstalled;	/* Did we install it?				*/
extern Boolean		RunningAsInit;		/* Are we running stand-alone?			*/
extern Boolean		DialogActive;		/* Is the dialog() stuff up?			*/
extern Boolean		ColorDisplay;		/* Are we on a color display?			*/
extern Boolean		OnVTY;			/* On a syscons VTY?				*/
extern Boolean		have_volumes;		/* Media has multiple volumes                   */
extern Variable		*VarHead;		/* The head of the variable chain		*/
extern Device		*mediaDevice;		/* Where we're getting our distribution from	*/
extern unsigned int	Dists;			/* Which distributions we want			*/
extern unsigned int	DocDists;		/* Which Doc dists we want			*/
extern unsigned int	SrcDists;		/* Which src distributions we want		*/
extern unsigned int	KernelDists;		/* Which kernel dists we want			*/
extern int		BootMgr;		/* Which boot manager to use 			*/
extern int		StatusLine;		/* Where to print our status messages		*/
extern DMenu		MenuCountry;		/* Country menu				*/
extern DMenu		MenuInitial;		/* Initial installation menu			*/
extern DMenu		MenuFixit;		/* Fixit repair menu				*/
#if defined(__i386__) || defined(__amd64__)
#ifdef PC98
extern DMenu		MenuIPLType;		/* Type of IPL to write on the disk		*/
#else
extern DMenu		MenuMBRType;		/* Type of MBR to write on the disk		*/
#endif
#endif
extern DMenu		MenuConfigure;		/* Final configuration menu			*/
extern DMenu		MenuDocInstall;		/* Documentation Installation menu		*/
extern DMenu		MenuDocumentation;	/* Documentation menu				*/
extern DMenu		MenuFTPOptions;		/* FTP Installation options			*/
extern DMenu		MenuIndex;		/* Index menu					*/
extern DMenu		MenuOptions;		/* Installation options				*/
extern DMenu		MenuOptionsLanguage;	/* Language options menu			*/
extern DMenu		MenuKLD;		/* Prototype KLD menu				*/
extern DMenu		MenuConfig;		/* Prototype config menu				*/
extern DMenu		MenuMedia;		/* Media type menu				*/
#ifdef WITH_MICE
extern DMenu		MenuMouse;		/* Mouse type menu				*/
#endif
extern DMenu		MenuMediaCDROM;		/* CDROM media menu				*/
extern DMenu		MenuMediaUSB;		/* USB media menu				*/
extern DMenu		MenuMediaDOS;		/* DOS media menu				*/
extern DMenu		MenuMediaFloppy;	/* Floppy media menu				*/
extern DMenu		MenuMediaFTP;		/* FTP media menu				*/
extern DMenu		MenuNetworkDevice;	/* Network device menu				*/
extern DMenu		MenuNTP;		/* NTP time server menu				*/
extern DMenu		MenuSecurity;		/* System security options menu			*/
extern DMenu		MenuSecurelevel;	/* Securelevel menu */
extern DMenu		MenuStartup;		/* Startup services menu			*/
#ifdef WITH_SYSCONS
extern DMenu		MenuSyscons;		/* System console configuration menu		*/
extern DMenu		MenuSysconsFont;	/* System console font configuration menu	*/
extern DMenu		MenuSysconsKeymap;	/* System console keymap configuration menu	*/
extern DMenu		MenuSysconsKeyrate;	/* System console keyrate configuration menu	*/
extern DMenu		MenuSysconsSaver;	/* System console saver configuration menu	*/
extern DMenu		MenuSysconsScrnmap;	/* System console screenmap configuration menu	*/
extern DMenu            MenuSysconsTtys;        /* System console terminal type menu            */
#endif
extern DMenu		MenuNetworking;		/* Network configuration menu			*/
extern DMenu		MenuMTA;		/* MTA selection menu				*/
extern DMenu		MenuInstallCustom;	/* Custom Installation menu			*/
extern DMenu		MenuDistributions;	/* Distribution menu				*/
extern DMenu		MenuDiskDevices;	/* Disk type devices				*/
extern DMenu		MenuSubDistributions;	/* Custom distribution menu			*/
extern DMenu		MenuSrcDistributions;	/* Source distribution menu			*/
extern DMenu		MenuKernelDistributions;/* Kernel distribution menu			*/
extern DMenu		MenuHTMLDoc;		/* HTML Documentation menu			*/
extern DMenu		MenuUsermgmt;		/* User management menu				*/
extern DMenu		MenuFixit;		/* Fixit floppy/CDROM/shell menu		*/
extern int              FixItMode;              /* FixItMode starts shell on current device (ie Serial port) */
extern const char *	StartName;		/* Which name we were started as */
extern const char *	ProgName;		/* Program's proper name */
extern int		NCpus;			/* # cpus on machine */
extern int		low_volume;		/* Lowest volume number */
extern int		high_volume;		/* Highest volume number */

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

/* acpi.c */
extern int	acpi_detect(void);

/* anonFTP.c */
extern int	configAnonFTP(dialogMenuItem *self);

/* cdrom.c */
extern Boolean	mediaInitCDROM(Device *dev);
extern FILE	*mediaGetCDROM(Device *dev, char *file, Boolean probe);
extern void	mediaShutdownCDROM(Device *dev);

/* command.c */
extern void	command_clear(void);
extern void	command_sort(void);
extern void	command_execute(void);
extern void	command_shell_add(char *key, char *fmt, ...) __printflike(2, 3);
extern void	command_func_add(char *key, commandFunc func, void *data);

/* config.c */
extern void	configEnvironmentRC_conf(void);
extern void	configEnvironmentResolv(char *config);
extern void	configRC_conf(void);
extern int	configFstab(dialogMenuItem *self);
extern int	configRC(dialogMenuItem *self);
extern int	configResolv(dialogMenuItem *self);
extern int	configPackages(dialogMenuItem *self);
extern int	configSaver(dialogMenuItem *self);
extern int	configSaverTimeout(dialogMenuItem *self);
#ifdef WITH_LINUX
extern int	configLinux(dialogMenuItem *self);
#endif
extern int	configNTP(dialogMenuItem *self);
extern int	configCountry(dialogMenuItem *self);
extern int	configUsers(dialogMenuItem *self);
extern int	configRouter(dialogMenuItem *self);
extern int	configPCNFSD(dialogMenuItem *self);
extern int	configInetd(dialogMenuItem *self);
extern int	configNFSServer(dialogMenuItem *self);
extern int	configMTAPostfix(dialogMenuItem *self);
extern int	configMTAExim(dialogMenuItem *self);
extern int	configRpcBind(dialogMenuItem *self);
extern int	configWriteRC_conf(dialogMenuItem *self);
extern int	configSecurelevel(dialogMenuItem *self);
extern int	configSecurelevelDisabled(dialogMenuItem *self);
extern int	configSecurelevelSecure(dialogMenuItem *self);
extern int	configSecurelevelHighlySecure(dialogMenuItem *self);
extern int	configSecurelevelNetworkSecure(dialogMenuItem *self);
extern int	configEtcTtys(dialogMenuItem *self);
#ifdef __i386__
extern int	checkLoaderACPI(void);
extern int	configLoaderACPI(int);
#endif

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
extern Device	*deviceRegister(char *name, char *desc, char *devname, DeviceType type, Boolean enabled,
				Boolean (*init)(Device *mediadev),
				FILE * (*get)(Device *dev, char *file, Boolean probe),
				void (*shutDown)(Device *mediadev),
				void *private);
extern Boolean	dummyInit(Device *dev);
extern FILE	*dummyGet(Device *dev, char *dist, Boolean probe);
extern void	dummyShutdown(Device *dev);

/* dhcp.c */
extern int	dhcpParseLeases(char *file, char *hostname, char *domain, char *nameserver,
				char *ipaddr, char *gateway, char *netmask);

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
extern int	dispatch_load_cdrom(dialogMenuItem *self);
extern int	dispatch_load_file_int(int);
extern int	dispatch_load_file(dialogMenuItem *self);
extern int	dispatch_load_menu(dialogMenuItem *self);


/* dist.c */
extern int	distReset(dialogMenuItem *self);
extern int	distConfig(dialogMenuItem *self);
extern int	distSetCustom(dialogMenuItem *self);
extern int	distUnsetCustom(dialogMenuItem *self);
extern int	distSetDeveloper(dialogMenuItem *self);
extern int	distSetKernDeveloper(dialogMenuItem *self);
extern int	distSetUser(dialogMenuItem *self);
extern int	distSetMinimum(dialogMenuItem *self);
extern int	distSetEverything(dialogMenuItem *self);
extern int	distSetSrc(dialogMenuItem *self);
extern int	distSetKernel(dialogMenuItem *self);
extern int	distSetDoc(dialogMenuItem *self);
extern int	distSetDocMenu(dialogMenuItem *self);
extern int	distExtractAll(dialogMenuItem *self);
extern int	selectKernel(void);

/* dmenu.c */
extern int	dmenuDisplayFile(dialogMenuItem *tmp);
extern int	dmenuSubmenu(dialogMenuItem *tmp);
extern int	dmenuSystemCommand(dialogMenuItem *tmp);
extern int	dmenuSystemCommandBox(dialogMenuItem *tmp);
extern int	dmenuExit(dialogMenuItem *tmp);
extern int	dmenuISetVariable(dialogMenuItem *tmp);
extern int	dmenuSetVariable(dialogMenuItem *tmp);
extern int	dmenuSetCountryVariable(dialogMenuItem *tmp);
extern int	dmenuSetKmapVariable(dialogMenuItem *tmp);
extern int	dmenuSetVariables(dialogMenuItem *tmp);
extern int	dmenuToggleVariable(dialogMenuItem *tmp);
extern int	dmenuSetFlag(dialogMenuItem *tmp);
extern int	dmenuSetValue(dialogMenuItem *tmp);
extern int	dmenuFindItem(DMenu *menu, const char *prompt, const char *title, void *data);
extern void	dmenuSetDefaultIndex(DMenu *menu, int *choice, int *scroll, int *curr, int *max);
extern int	dmenuSetDefaultItem(DMenu *menu, const char *prompt, const char *title, void *data,
				    int *choice, int *scroll, int *curr, int *max);
extern Boolean	dmenuOpen(DMenu *menu, int *choice, int *scroll, int *curr, int *max, Boolean buttons);
extern Boolean	dmenuOpenSimple(DMenu *menu, Boolean buttons);
extern int	dmenuVarCheck(dialogMenuItem *item);
extern int	dmenuVarsCheck(dialogMenuItem *item);
extern int	dmenuFlagCheck(dialogMenuItem *item);
extern int	dmenuRadioCheck(dialogMenuItem *item);

/* doc.c */
extern int	docBrowser(dialogMenuItem *self);
extern int	docShowDocument(dialogMenuItem *self);

/* dos.c */
extern Boolean	mediaCloseDOS(Device *dev, FILE *fp);
extern Boolean	mediaInitDOS(Device *dev);
extern FILE	*mediaGetDOS(Device *dev, char *file, Boolean probe);
extern void	mediaShutdownDOS(Device *dev);

/* floppy.c */
extern int	getRootFloppy(void);
extern Boolean	mediaInitFloppy(Device *dev);
extern FILE	*mediaGetFloppy(Device *dev, char *file, Boolean probe);
extern void	mediaShutdownFloppy(Device *dev);

/* ftp_strat.c */
extern Boolean	mediaCloseFTP(Device *dev, FILE *fp);
extern Boolean	mediaInitFTP(Device *dev);
extern FILE	*mediaGetFTP(Device *dev, char *file, Boolean probe);
extern void	mediaShutdownFTP(Device *dev);

/* http.c */
extern Boolean	mediaInitHTTP(Device *dev);
extern FILE	*mediaGetHTTP(Device *dev, char *file, Boolean probe);

/* globals.c */
extern void	globalsInit(void);

/* index.c */
int		index_read(FILE *fp, PkgNodePtr papa);
int		index_menu(PkgNodePtr root, PkgNodePtr top, PkgNodePtr plist, int *pos, int *scroll);
void		index_init(PkgNodePtr top, PkgNodePtr plist);
void		index_node_free(PkgNodePtr top, PkgNodePtr plist);
void		index_sort(PkgNodePtr top);
void		index_print(PkgNodePtr top, int level);
int		index_extract(Device *dev, PkgNodePtr top, PkgNodePtr who, Boolean depended, int current_volume);
int		index_initialize(char *path);
PkgNodePtr	index_search(PkgNodePtr top, char *str, PkgNodePtr *tp);

/* install.c */
extern Boolean	checkLabels(Boolean whinge);
extern int	installCommit(dialogMenuItem *self);
extern int	installCustomCommit(dialogMenuItem *self);
extern int	installExpress(dialogMenuItem *self);
extern int	installStandard(dialogMenuItem *self);
extern int	installFixitHoloShell(dialogMenuItem *self);
extern int	installFixitCDROM(dialogMenuItem *self);
extern int	installFixitUSB(dialogMenuItem *self);
extern int	installFixitFloppy(dialogMenuItem *self);
extern int	installFixupBase(dialogMenuItem *self);
extern int	installFixupKernel(dialogMenuItem *self, int dists);
extern int	installUpgrade(dialogMenuItem *self);
extern int	installFilesystems(dialogMenuItem *self);
extern int	installVarDefaults(dialogMenuItem *self);
extern void	installEnvironment(void);
extern Boolean	copySelf(void);

/* kget.c */
extern int	kget(char *out);

/* keymap.c */
extern int	keymapMenuSelect(dialogMenuItem *self);
extern int	loadKeymap(const char *lang);

/* label.c */
extern int	diskLabelEditor(dialogMenuItem *self);
extern int	diskLabelCommit(dialogMenuItem *self);

/* makedevs.c (auto-generated) */
extern const char	termcap_ansi[];
extern const char	termcap_vt100[];
extern const char	termcap_cons25w[];
extern const char	termcap_cons25[];
extern const char	termcap_cons25_m[];
extern const char	termcap_cons25r[];
extern const char	termcap_cons25r_m[];
extern const char	termcap_cons25l1[];
extern const char	termcap_cons25l1_m[];
extern const char	termcap_xterm[];
extern const u_char	font_iso_8x16[];
extern const u_char	font_cp850_8x16[];
extern const u_char	font_cp866_8x16[];
extern const u_char	koi8_r2cp866[];
extern u_char		default_scrnmap[];

/* media.c */
extern char	*cpioVerbosity(void);
extern int	mediaOpen(void);
extern void	mediaClose(void);
extern int	mediaTimeout(void);
extern int	mediaSetCDROM(dialogMenuItem *self);
extern int	mediaSetFloppy(dialogMenuItem *self);
extern int	mediaSetUSB(dialogMenuItem *self);
extern int	mediaSetDOS(dialogMenuItem *self);
extern int	mediaSetFTP(dialogMenuItem *self);
extern int	mediaSetFTPActive(dialogMenuItem *self);
extern int	mediaSetFTPPassive(dialogMenuItem *self);
extern int	mediaSetHTTP(dialogMenuItem *self);
extern int	mediaSetUFS(dialogMenuItem *self);
extern int	mediaSetNFS(dialogMenuItem *self);
extern int	mediaSetFTPUserPass(dialogMenuItem *self);
extern int	mediaSetCPIOVerbosity(dialogMenuItem *self);
extern int	mediaGetType(dialogMenuItem *self);
extern Boolean	mediaExtractDist(char *dir, char *dist, FILE *fp);
extern Boolean	mediaExtractDistBegin(char *dir, int *fd, int *zpid, int *cpic);
extern Boolean	mediaExtractDistEnd(int zpid, int cpid);
extern Boolean	mediaVerify(void);
extern FILE	*mediaGenericGet(char *base, const char *file);

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
				void *data, void *aux, int *curr, int *max);
extern void	items_free(dialogMenuItem *list, int *curr, int *max);
extern int	Mkdir(char *);
extern int	Mkdir_command(char *key, void *data);
extern int	Mount(char *, void *data);
extern int	Mount_msdosfs(char *mountp, void *devname);
extern WINDOW	*openLayoutDialog(char *helpfile, char *title, int x, int y, int width, int height);
extern ComposeObj *initLayoutDialog(WINDOW *win, Layout *layout, int x, int y, int *max);
extern int	layoutDialogLoop(WINDOW *win, Layout *layout, ComposeObj **obj,
				 int *n, int max, int *cbutton, int *cancel);

extern WINDOW	*savescr(void);
extern void	restorescr(WINDOW *w);
extern char	*sstrncpy(char *dst, const char *src, int size);
extern char	*getsysctlbyname(const char *sysctlname);

/* modules.c */
extern void	driverFloppyCheck(void);
extern void	moduleInitialize(void);
extern int	kldBrowser(dialogMenuItem *self);

/* mouse.c */
extern int	mousedTest(dialogMenuItem *self);
extern int	mousedDisable(dialogMenuItem *self);
extern int      setMouseFlags(dialogMenuItem *self);

/* mptable.c */
extern int	biosmptable_detect(void);

/* msg.c */
extern Boolean	isDebug(void);
extern void	msgInfo(char *fmt, ...) __printf0like(1, 2);
extern void	msgYap(char *fmt, ...) __printflike(1, 2);
extern void	msgWarn(char *fmt, ...) __printflike(1, 2);
extern void	msgDebug(char *fmt, ...) __printflike(1, 2);
extern void	msgError(char *fmt, ...) __printflike(1, 2);
extern void	msgFatal(char *fmt, ...) __printflike(1, 2);
extern void	msgConfirm(char *fmt, ...) __printflike(1, 2);
extern void	msgNotify(char *fmt, ...) __printflike(1, 2);
extern void	msgWeHaveOutput(char *fmt, ...) __printflike(1, 2);
extern int	msgYesNo(char *fmt, ...) __printflike(1, 2);
extern int	msgNoYes(char *fmt, ...) __printflike(1, 2);
extern char	*msgGetInput(char *buf, char *fmt, ...) __printflike(2, 3);
extern int	msgSimpleConfirm(char *);
extern int	msgSimpleNotify(char *);

/* network.c */
extern Boolean	mediaInitNetwork(Device *dev);
extern void	mediaShutdownNetwork(Device *dev);

/* nfs.c */
extern Boolean	mediaInitNFS(Device *dev);
extern FILE	*mediaGetNFS(Device *dev, char *file, Boolean probe);
extern void	mediaShutdownNFS(Device *dev);

/* options.c */
extern int	optionsEditor(dialogMenuItem *self);

/* package.c */
extern int	packageAdd(dialogMenuItem *self);
extern int	package_add(char *name);
extern int	package_extract(Device *dev, char *name, Boolean depended);
extern Boolean	package_installed(char *name);

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
extern void	systemCreateHoloshell(void);
extern int	vsystem(char *fmt, ...) __printflike(1, 2);

/* tcpip.c */
extern int	tcpOpenDialog(Device *dev);
extern int	tcpMenuSelect(dialogMenuItem *self);
extern Device	*tcpDeviceSelect(void);

/* termcap.c */
extern int	set_termcap(void);

/* ttys.c */
extern void     configTtys(void);

/* ufs.c */
extern void	mediaShutdownUFS(Device *dev);
extern Boolean	mediaInitUFS(Device *dev);
extern FILE	*mediaGetUFS(Device *dev, char *file, Boolean probe);

/* usb.c */
extern Boolean	mediaInitUSB(Device *dev);
extern FILE	*mediaGetUSB(Device *dev, char *file, Boolean probe);
extern void	mediaShutdownUSB(Device *dev);

/* user.c */
extern int	userAddGroup(dialogMenuItem *self);
extern int	userAddUser(dialogMenuItem *self);

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

#ifdef USE_GZIP
#define UNZIPPER "gunzip"
#else
#define UNZIPPER "bunzip2"
#endif

#endif
/* _SYSINSTALL_H_INCLUDE */
