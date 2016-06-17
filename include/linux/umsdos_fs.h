#ifndef LINUX_UMSDOS_FS_H
#define LINUX_UMSDOS_FS_H


/*#define UMS_DEBUG 1	// define for check_* functions */
/*#define UMSDOS_DEBUG 1*/
#define UMSDOS_PARANOIA 1

#define UMSDOS_VERSION	0
#define UMSDOS_RELEASE	4

#define UMSDOS_ROOT_INO 1

/* This is the file acting as a directory extension */
#define UMSDOS_EMD_FILE		"--linux-.---"
#define UMSDOS_EMD_NAMELEN	12
#define UMSDOS_PSDROOT_NAME	"linux"
#define UMSDOS_PSDROOT_LEN	5

#ifndef _LINUX_TYPES_H
#include <linux/types.h>
#endif
#ifndef _LINUX_LIMITS_H
#include <linux/limits.h>
#endif
#ifndef _LINUX_DIRENT_H
#include <linux/dirent.h>
#endif
#ifndef _LINUX_IOCTL_H
#include <linux/ioctl.h>
#endif


#ifdef __KERNEL__
/* #Specification: convention / PRINTK Printk and printk
 * Here is the convention for the use of printk inside fs/umsdos
 * 
 * printk carry important message (error or status).
 * Printk is for debugging (it is a macro defined at the beginning of
 * most source.
 * PRINTK is a nulled Printk macro.
 * 
 * This convention makes the source easier to read, and Printk easier
 * to shut off.
 */
#	define PRINTK(x)
#	ifdef UMSDOS_DEBUG
#		define Printk(x) printk x
#	else
#		define Printk(x)
#	endif
#endif


struct umsdos_fake_info {
	char fname[13];
	int len;
};

#define UMSDOS_MAXNAME	220
/* This structure is 256 bytes large, depending on the name, only part */
/* of it is written to disk */
/* nice though it would be, I can't change this and preserve backward compatibility */
struct umsdos_dirent {
	unsigned char name_len;	/* if == 0, then this entry is not used */
	unsigned char flags;	/* UMSDOS_xxxx */
	unsigned short nlink;	/* How many hard links point to this entry */
	__kernel_uid_t uid;	/* Owner user id */
	__kernel_gid_t gid;	/* Group id */
	time_t atime;		/* Access time */
	time_t mtime;		/* Last modification time */
	time_t ctime;		/* Creation time */
	dev_t rdev;		/* major and minor number of a device */
				/* special file */
	umode_t mode;		/* Standard UNIX permissions bits + type of */
	char spare[12];		/* unused bytes for future extensions */
				/* file, see linux/stat.h */
	char name[UMSDOS_MAXNAME];	/* Not '\0' terminated */
				/* but '\0' padded, so it will allow */
				/* for adding news fields in this record */
				/* by reducing the size of name[] */
};

#define UMSDOS_HIDDEN	1	/* Never show this entry in directory search */
#define UMSDOS_HLINK	2	/* It is a (pseudo) hard link */

/* #Specification: EMD file / record size
 * Entry are 64 bytes wide in the EMD file. It allows for a 30 characters
 * name. If a name is longer, contiguous entries are allocated. So a
 * umsdos_dirent may span multiple records.
 */
 
#define UMSDOS_REC_SIZE		64

/* Translation between MSDOS name and UMSDOS name */

struct umsdos_info {
	int msdos_reject;	/* Tell if the file name is invalid for MSDOS */
				/* See umsdos_parse */
	struct umsdos_fake_info fake;
	struct umsdos_dirent entry;
	off_t f_pos;		/* offset of the entry in the EMD file
				 * or offset where the entry may be store
				 * if it is a new entry
				 */
	int recsize;		/* Record size needed to store entry */
};

/* Definitions for ioctl (number randomly chosen)
 * The next ioctl commands operate only on the DOS directory
 * The file umsdos_progs/umsdosio.c contain a string table
 * based on the order of those definition. Keep it in sync
 */
#define UMSDOS_READDIR_DOS _IO(0x04,210)	/* Do a readdir of the DOS directory */
#define UMSDOS_UNLINK_DOS  _IO(0x04,211)	/* Erase in the DOS directory only */
#define UMSDOS_RMDIR_DOS   _IO(0x04,212)	/* rmdir in the DOS directory only */
#define UMSDOS_STAT_DOS    _IO(0x04,213)	/* Get info about a file */

/* The next ioctl commands operate only on the EMD file */
#define UMSDOS_CREAT_EMD   _IO(0x04,214)	/* Create a file */
#define UMSDOS_UNLINK_EMD  _IO(0x04,215)	/* unlink (rmdir) a file */
#define UMSDOS_READDIR_EMD _IO(0x04,216)	/* read the EMD file only. */
#define UMSDOS_GETVERSION  _IO(0x04,217)	/* Get the release number of UMSDOS */
#define UMSDOS_INIT_EMD    _IO(0x04,218)	/* Create the EMD file if not there */
#define UMSDOS_DOS_SETUP   _IO(0x04,219)	/* Set the defaults of the MS-DOS driver. */

#define UMSDOS_RENAME_DOS  _IO(0x04,220)	/* rename a file/directory in the DOS
						 * directory only */
struct umsdos_ioctl {
	struct dirent dos_dirent;
	struct umsdos_dirent umsdos_dirent;
	/* The following structure is used to exchange some data
	 * with utilities (umsdos_progs/util/umsdosio.c). The first
	 * releases were using struct stat from "sys/stat.h". This was
	 * causing some problem for cross compilation of the kernel
	 * Since I am not really using the structure stat, but only some field
	 * of it, I have decided to replicate the structure here
	 * for compatibility with the binaries out there
	 * FIXME PTW 1998, this has probably changed
	 */
	
	struct {
		dev_t st_dev;
		unsigned short __pad1;
		ino_t st_ino;
		umode_t st_mode;
		nlink_t st_nlink;
		__kernel_uid_t st_uid;
		__kernel_gid_t st_gid;
		dev_t st_rdev;
		unsigned short __pad2;
		off_t st_size;
		unsigned long st_blksize;
		unsigned long st_blocks;
		time_t st_atime;
		unsigned long __unused1;
		time_t st_mtime;
		unsigned long __unused2;
		time_t st_ctime;
		unsigned long __unused3;
		uid_t st_uid32;
		gid_t st_gid32;
	} stat;
	char version, release;
};

/* Different macros to access struct umsdos_dirent */
#define EDM_ENTRY_ISUSED(e) ((e)->name_len!=0)

#ifdef __KERNEL__

#ifndef LINUX_FS_H
#include <linux/fs.h>
#endif

extern struct inode_operations umsdos_dir_inode_operations;
extern struct inode_operations umsdos_rdir_inode_operations;
extern struct file_operations umsdos_dir_operations;
extern struct file_operations umsdos_rdir_operations;

#include <linux/umsdos_fs.p>

#endif				/* __KERNEL__ */

#endif
