#ifndef _MSDOS_FS_SB
#define _MSDOS_FS_SB
#include<linux/fat_cvf.h>

/*
 * MS-DOS file system in-core superblock data
 */

struct fat_mount_options {
	uid_t fs_uid;
	gid_t fs_gid;
	unsigned short fs_umask;
	unsigned short codepage;  /* Codepage for shortname conversions */
	char *iocharset;          /* Charset used for filename input/display */
	unsigned short shortname; /* flags for shortname display/create rule */
	unsigned char name_check; /* r = relaxed, n = normal, s = strict */
	unsigned char conversion; /* b = binary, t = text, a = auto */
	unsigned quiet:1,         /* set = fake successful chmods and chowns */
		 showexec:1,      /* set = only set x bit for com/exe/bat */
		 sys_immutable:1, /* set = system files are immutable */
		 dotsOK:1,        /* set = hidden and system files are named '.filename' */
		 isvfat:1,        /* 0=no vfat long filename support, 1=vfat support */
		 utf8:1,	  /* Use of UTF8 character set (Default) */
		 unicode_xlate:1, /* create escape sequences for unhandled Unicode */
		 posixfs:1,       /* Allow names like makefile and Makefile to coexist */
		 numtail:1,       /* Does first alias have a numeric '~1' type tail? */
		 atari:1,         /* Use Atari GEMDOS variation of MS-DOS fs */
		 fat32:1,	  /* Is this a FAT32 partition? */
		 nocase:1;	  /* Does this need case conversion? 0=need case conversion*/
};

struct msdos_sb_info {
	unsigned short cluster_size; /* sectors/cluster */
	unsigned short cluster_bits; /* sectors/cluster */
	unsigned char fats,fat_bits; /* number of FATs, FAT bits (12 or 16) */
	unsigned short fat_start;
	unsigned long fat_length;    /* FAT start & length (sec.) */
	unsigned long dir_start;
	unsigned short dir_entries;  /* root dir start & entries */
	unsigned long data_start;    /* first data sector */
	unsigned long clusters;      /* number of clusters */
	unsigned long root_cluster;  /* first cluster of the root directory */
	unsigned long fsinfo_sector; /* FAT32 fsinfo offset from start of disk */
	struct semaphore fat_lock;
	int prev_free;               /* previously returned free cluster number */
	int free_clusters;           /* -1 if undefined */
	struct fat_mount_options options;
	struct nls_table *nls_disk;  /* Codepage used on disk */
	struct nls_table *nls_io;    /* Charset used for input and display */
	struct cvf_format* cvf_format;
	void *dir_ops;		     /* Opaque; default directory operations */
	void *private_data;
	int dir_per_block;	     /* dir entries per block */
	int dir_per_block_bits;	     /* log2(dir_per_block) */
};

#endif
