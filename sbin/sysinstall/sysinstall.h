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

#define TITLE	"FreeBSD 2.0.1-Development Installation"

#define MAXFS	25

#define MAX_NO_DISKS 10
#define SCRATCHSIZE 1024
#define ERRMSGSIZE 256
#define DEFROOTSIZE 16
#define DEFSWAPSIZE 16
#define DEFUSRSIZE 80		/* My disk is not that big Paul ! */

#define BOOT_MAGIC 0xAA55
#define ACTIVE 0x80

#define COPYRIGHT_FILE	"/COPYRIGHT"
#define README_FILE	"/README"

#define STATUSFILE "sysinstall.stat"
#define NOT_INSTALLED 0
#define DISK_READY 1
#define INSTALLED_BASE 2

struct sysinstall
{
	char media[90];
	int status;
	char seq_name[64];
	int seq_size;
	int seq_no;
	char archive[64];
	char root_dev[90];
};

#ifndef EXTERN
#  define EXTERN extern
#endif

EXTERN char *devicename[MAXFS+1];
EXTERN char *mountpoint[MAXFS+1];
EXTERN int dialog_active;

extern int no_disks;
extern int inst_disk;
extern unsigned char *scratch;
extern unsigned char *errmsg;
extern int *avail_fds;
extern struct disklabel *avail_disklabels;
extern u_short dkcksum(struct disklabel *);

/* utils.c */
void	TellEm __P((char *fmt, ...));
void	stage0	__P((void));
void	*Malloc __P((size_t size));
char	*StrAlloc __P((char *str));
void	Fatal __P((char *fmt, ...));
int	AskAbort __P((char *fmt, ...));
void	MountUfs __P((char *device, char *prefix, char *mountpoint, int do_mkdir));
void	Mkdir __P((char *path));

/* exec.c */
int	exec __P((int magic, char *cmd, char *args, ...));
#define EXEC_MAXARG	100

/* stage0.c */
void	stage0 __P((void));

/* stage1.c */
void	stage1 __P((void));

/* stage2.c */
void	stage2 __P((void));

/* stage3.c */
void	stage3 __P((void));

/* termcap.c */
int	set_termcap __P((void));
