#ifndef __LINUX_ROMFS_FS_H
#define __LINUX_ROMFS_FS_H

/* The basic structures of the romfs filesystem */

#define ROMBSIZE BLOCK_SIZE
#define ROMBSBITS BLOCK_SIZE_BITS
#define ROMBMASK (ROMBSIZE-1)
#define ROMFS_MAGIC 0x7275

#define ROMFS_MAXFN 128

#define __mkw(h,l) (((h)&0x00ff)<< 8|((l)&0x00ff))
#define __mkl(h,l) (((h)&0xffff)<<16|((l)&0xffff))
#define __mk4(a,b,c,d) htonl(__mkl(__mkw(a,b),__mkw(c,d)))
#define ROMSB_WORD0 __mk4('-','r','o','m')
#define ROMSB_WORD1 __mk4('1','f','s','-')

/* On-disk "super block" */

struct romfs_super_block {
	__u32 word0;
	__u32 word1;
	__u32 size;
	__u32 checksum;
	char name[0];		/* volume name */
};

/* On disk inode */

struct romfs_inode {
	__u32 next;		/* low 4 bits see ROMFH_ */
	__u32 spec;
	__u32 size;
	__u32 checksum;
	char name[0];
};

#define ROMFH_TYPE 7
#define ROMFH_HRD 0
#define ROMFH_DIR 1
#define ROMFH_REG 2
#define ROMFH_SYM 3
#define ROMFH_BLK 4
#define ROMFH_CHR 5
#define ROMFH_SCK 6
#define ROMFH_FIF 7
#define ROMFH_EXEC 8

/* Alignment */

#define ROMFH_SIZE 16
#define ROMFH_PAD (ROMFH_SIZE-1)
#define ROMFH_MASK (~ROMFH_PAD)

#ifdef __KERNEL__

/* Not much now */

#endif /* __KERNEL__ */
#endif
