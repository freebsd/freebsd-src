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
 *	$Id: boot.h,v 1.11 1995/06/25 14:02:52 joerg Exp $
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <ufs/ufs/quota.h>
#include <ufs/ffs/fs.h>
#include <ufs/ufs/inode.h>

extern char *devs[], *iodest;
extern char *name, dflname[];
extern struct fs *fs;
extern struct inode inode;
extern int dosdev, unit, slice, part, maj, boff, poff, bnum, cnt;
extern unsigned long tw_chars;
extern int loadflags;
extern struct disklabel disklabel;

/* asm.S */
#if ASM_ONLY
void real_to_prot(void);
void prot_to_real(void);
#endif
void startprog(unsigned int physaddr, int howto, int bootdev,
	       /* XXX struct bootinfo * */ unsigned int bootinfo);
void pbzero(void *dst, size_t count);
void pcpy(const void *src, void *dst, size_t count);

/* bios.S */
int biosread(int dev, int cyl, int head, int sec, int nsec, void *offset);
void putc(int c);
int getc(void);
int ischar(void);
int get_diskinfo(int drive);
int memsize(int extended);

/* boot.c */
void boot(int drive);

/* boot2.S */
void boot2(void);

/* disk.c */
int devopen(void);
void devread(char *iodest, int sector, int cnt);
char * Bread(int dosdev, int sector);
int badsect(int dosdev, int sector);

/* io.c */
void gateA20(void);
void printf(const char *format, ...);
void putchar(int c);
int getchar(int in_buf);
void delay1ms(void);
int gets(char *buf);
int strcmp(const char *s1, const char *s2);
void bcopy(const char *from, char *to, int len);
void twiddle(void);

/* probe_keyboard.c */
int probe_keyboard(void);

/* serial.S */
void serial_putc(int ch);
int serial_getc(void);
int serial_ischar(void);
void init_serial(void);

/* sys.c */
void xread(char *addr, int size);
void read(char *buffer, int count);
int find(char *path);
int block_map(int file_block);
int openrd(void);
