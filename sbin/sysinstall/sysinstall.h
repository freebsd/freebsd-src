/*
 * Copyright (c) 1994, Paul Richards.
 *
 * All rights reserved.
 *
 * This software may be used, modified, copied, distributed, and
 * sold, in both source and binary form provided that the above
 * copyright and these terms are retained, verbatim, as the first
 * lines of this file.  Under no circumstances is the author
 * responsible for the proper functioning of this software, nor does
 * the author assume any responsibility for damages incurred with
 * its use.
 */

#define TITLE	"FreeBSD 2.0-950418-SNAP Install"

#define MAX_NO_DISKS	10
#define MAX_NO_FS	30
#define MAXFS	MAX_NO_FS

#define BBSIZE		8192	/* Actually in ufs/ffs/fs.h I think */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ncurses.h>
#include <string.h>
#include <errno.h>
#include <dialog.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/dkbad.h>
#include <sys/disklabel.h>

#define SCRATCHSIZE 1024
#define ERRMSGSIZE 256
#define DEFROOTSIZE 18
#define DEFSWAPSIZE 16
#define DEFUSRSIZE 80
#define DEFFSIZE 1024
#define DEFFRAG 8

#define BOOT_MAGIC 0xAA55
#define ACTIVE 0x80

#define COPYRIGHT_FILE	"/COPYRIGHT"
#define README_FILE	"/README"
#define HELPME_FILE	"/DISKSPACE.FAQ"
#define TROUBLE_FILE	"/TROUBLESHOOTING"
#define RELNOTES_FILE	"/RELNOTES.FreeBSD"

#ifndef EXTERN
#  define EXTERN extern
#endif

extern unsigned char boot0[];
extern unsigned char boot1[];
extern unsigned char boot2[];

/* All this "disk" stuff */
EXTERN int Ndisk;
EXTERN struct disklabel *Dlbl[MAX_NO_DISKS];
EXTERN char *Dname[MAX_NO_DISKS];
EXTERN int Dfd[MAX_NO_DISKS];

EXTERN int MP[MAX_NO_DISKS][MAXPARTITIONS];

/* All this "filesystem" stuff */
EXTERN int Nfs;
EXTERN char *Fname[MAX_NO_FS+1];
EXTERN char *Fmount[MAX_NO_FS+1];
EXTERN char *Ftype[MAX_NO_FS+1];
EXTERN int Faction[MAX_NO_FS+1];
EXTERN u_long Fsize[MAX_NO_FS+1];

EXTERN int dialog_active;
EXTERN char selection[];
EXTERN int debug_fd;
EXTERN int dialog_active;
EXTERN int fixit;

EXTERN int on_serial;
EXTERN int on_cdrom;
EXTERN int cpio_fd;

extern int no_disks;
extern int inst_disk;
extern unsigned char *scratch;
extern unsigned char *errmsg;
extern u_short dkcksum(struct disklabel *);

/* utils.c */
void	Abort __P((void));
void	ExitSysinstall __P((void));
void	TellEm __P((char *fmt, ...));
void	Debug __P((char *fmt, ...));
void	stage0	__P((void));
void	*Malloc __P((size_t size));
char	*StrAlloc __P((char *str));
void	Fatal __P((char *fmt, ...));
void	AskAbort __P((char *fmt, ...));
void	MountUfs __P((char *device, char *mountpoint, int do_mkdir,int flags));
void	Mkdir __P((char *path, int die));
void	Link __P((char *from, char *to));
void	CopyFile __P((char *p1, char *p2));
u_long	PartMb(struct disklabel *lbl,int part);
char *	SetMount __P((int disk, int part, char *path));
void	CleanMount __P((int disk, int part));
void	enable_label __P((int fd));
void	disable_label __P((int fd));

/* exec.c */
int	exec __P((int magic, char *cmd, char *args, ...));
#define EXEC_MAXARG	100

/* stage0.c */
void	stage0 __P((void));

/* stage1.c */
int	stage1 __P((void));

/* stage2.c */
void	stage2 __P((void));

/* stage3.c */
void	stage3 __P((void));

/* stage5.c */
void	stage5 __P((void));

/* termcap.c */
int	set_termcap __P((void));

/* makedevs.c */
int	makedevs __P((void));

/* ourcurses.c */
int AskEm __P((WINDOW *w,char *prompt, char *answer, int len));
void ShowFile __P((char *filename, char *header));

/* mbr.c */
int	build_bootblocks __P((int dfd,struct disklabel *label,struct dos_partition *dospart));
void	Fdisk __P((void));
void	read_dospart __P((int, struct dos_partition *));

/* label.c */
void	DiskLabel __P((void));
