/*
 * msdos common header file
 * [obtained from mtools -wfj]
 * how to decipher DOS disk structures in coexisting with DOS
 */

#define MSECTOR_SIZE	512		/* MSDOS sector size in bytes */
#define MDIR_SIZE	32		/* MSDOS directory size in bytes */
#define MAX_CLUSTER	8192		/* largest cluster size */
#define MAX_PATH	128		/* largest MSDOS path length */
#define MAX_DIR_SECS	64		/* largest directory (in sectors) */

#define NEW		1
#define OLD		0

struct directory {
	unsigned char name[8];		/* file name */
	unsigned char ext[3];		/* file extension */
	unsigned char attr;		/* attribute byte */
	unsigned char reserved[10];	/* ?? */
	unsigned char time[2];		/* time stamp */
	unsigned char date[2];		/* date stamp */
	unsigned char start[2];		/* starting cluster number */
	unsigned char size[4];		/* size of the file */
};

struct bootsector {
	unsigned char jump[3];		/* Jump to boot code */
	unsigned char banner[8];	/* OEM name & version */
	unsigned char secsiz[2];	/* Bytes per sector hopefully 512 */
	unsigned char clsiz;		/* Cluster size in sectors */
	unsigned char nrsvsect[2];	/* Number of reserved (boot) sectors */
	unsigned char nfat;		/* Number of FAT tables hopefully 2 */
	unsigned char dirents[2];	/* Number of directory slots */
	unsigned char psect[2];		/* Total sectors on disk */
	unsigned char descr;		/* Media descriptor=first byte of FAT */
	unsigned char fatlen[2];	/* Sectors in FAT */
	unsigned char nsect[2];		/* Sectors/track */
	unsigned char nheads[2];	/* Heads */
	unsigned char nhs[4];		/* number of hidden sectors */
	unsigned char bigsect[4];	/* big total sectors */
	unsigned char junk[476];	/* who cares? */
};

/* DOS partition table -- located in boot block */

#define	DOSBBSECTOR	0	/* DOS boot block relative sector number */
#define	DOSPARTOFF	446
#define NDOSPART	4

struct dos_partition {
	unsigned char	dp_flag;	/* bootstrap flags */
	unsigned char	dp_shd;		/* starting head */
	unsigned char	dp_ssect;	/* starting sector */
	unsigned char	dp_scyl;	/* starting cylinder */
	unsigned char	dp_typ;		/* partition type */
#define		DOSPTYP_386BSD	0xa5		/* 386BSD partition type */
	unsigned char	dp_ehd;		/* end head */
	unsigned char	dp_esect;	/* end sector */
	unsigned char	dp_ecyl;	/* end cylinder */
	unsigned long	dp_start;	/* absolute starting sector number */
	unsigned long	dp_size;	/* partition size in sectors */
} dos_partitions[NDOSPART];
