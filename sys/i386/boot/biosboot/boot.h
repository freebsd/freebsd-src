/*
 * Mach Operating System
 * Copyright (c) 1992, 1991 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 *
 *	from: Mach, Revision 2.2  92/04/04  11:35:03  rpd
 *	$Id: boot.h,v 1.6 1995/01/25 21:37:39 bde Exp $
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <ufs/ufs/quota.h>
#include <ufs/ffs/fs.h>
#include <ufs/ufs/inode.h>

extern char *devs[], *name, *iodest;
extern struct fs *fs;
extern struct inode inode;
extern int dosdev, unit, slice, part, maj, boff, poff, bnum, cnt;
extern unsigned long tw_chars;
extern int loadflags;
extern int end;
extern struct disklabel disklabel;

/* asm.S */
#if ASM_ONLY
extern void real_to_prot(void);
extern void prot_to_real(void);
#endif
extern void startprog(unsigned int physaddr, int howto, int bootdev,
		      /* struct bootinfo * */ unsigned int bootinfo);
extern void pbzero(unsigned char *dst, unsigned int count);
extern void pcpy(const unsigned char *src, unsigned char *dst,
		 unsigned int count);

/* bios.S */
extern int biosread(unsigned char dev, unsigned short cyl, unsigned char head,
		    unsigned char sec, unsigned char nsec,
		    unsigned char *offset);
extern void putc(char c);
extern int getc(void);
extern int ischar(void);
extern int get_diskinfo(int drive);
extern int memsize(int extended);

/* boot.c */
extern void boot(int drive);
extern void loadprog(int howto);
extern void getbootdev(int *howto);

/* boot2.S */
extern void boot2(void);

/* disk.c */
extern int devopen(void);
extern void devread(void);
extern void Bread(int dosdev, int sector);
extern int badsect(int dosdev, int sector);

/* io.c */
extern void gateA20(void);
extern printf(const char *format, ...);
extern void putchar(int c);
extern int getchar(int in_buf);
extern void delay1ms(void);
extern int gets(char *buf);
extern int strcmp(const char *s1, const char *s2);
extern void bcopy(const char *from, char *to, int len);
extern void twiddle(void);

/* probe_keyboard.c */
extern int probe_keyboard(void);

/* serial.S */
extern void serial_putc(char ch);
extern int serial_getc(void);
extern int serial_ischar(void);
extern void init_serial(void);

/* sys.c */
extern int xread(char *addr, int size);
extern void read(char *buffer, int count);
extern int find(char *path);
extern int block_map(int file_block);
extern int openrd(void);
