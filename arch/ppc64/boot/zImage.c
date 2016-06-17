/*
 * Copyright (C) Paul Mackerras 1997.
 *
 * Updates for PPC64 by Todd Inglett, Dave Engebretsen & Peter Bergner.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#define __KERNEL__
#include "ppc32-types.h"
#include "zlib.h"
#include <linux/elf.h>
#include <asm/processor.h>
#include <asm/page.h>
#include <asm/bootinfo.h>

void memmove(void *dst, void *im, int len);
void *memcpy(void *dest, const void *src, size_t n);
size_t strlen(const char *s);

extern void *finddevice(const char *);
extern int getprop(void *, const char *, void *, int);
extern void printk(char *fmt, ...);
extern void printf(const char *fmt, ...);
extern int sprintf(char *buf, const char *fmt, ...);
void gunzip(void *, int, unsigned char *, int *);
void *claim(unsigned int, unsigned int, unsigned int);
void flush_cache(void *, unsigned long);
void pause(void);
extern void exit(void);

static struct bi_record *make_bi_recs(unsigned long);

#define RAM_START	0x00000000
#define RAM_END		(64<<20)

/* Value picked to match that used by yaboot */
#define PROG_START	0x01400000

char *avail_ram;
char *begin_avail, *end_avail;
char *avail_high;
unsigned int heap_use;
unsigned int heap_max;

extern char _end[];
extern char _vmlinux_start[];
extern char _vmlinux_end[];
extern char _sysmap_start[];
extern char _sysmap_end[];
extern char _initrd_start[];
extern char _initrd_end[];

extern void *_vmlinux_filesize;
extern void *_vmlinux_memsize;

struct addr_range {
	unsigned long addr;
	unsigned long size;
	unsigned long memsize;
};
struct addr_range vmlinux = {0, 0, 0};
struct addr_range vmlinuz = {0, 0, 0};
struct addr_range sysmap  = {0, 0, 0};
struct addr_range initrd  = {0, 0, 0};

static char scratch[128<<10];	/* 128kB of scratch space for gunzip */

typedef void (*kernel_entry_t)( unsigned long,
		                unsigned long,
		                void *,
				struct bi_record *);


int (*prom)(void *);

void *chosen_handle;
void *stdin;
void *stdout;
void *stderr;


void
start(unsigned long a1, unsigned long a2, void *promptr)
{
	unsigned long i, claim_addr, claim_size;
	unsigned long vmlinux_filesize;
	extern char _start;
	struct bi_record *bi_recs;
	kernel_entry_t kernel_entry;
	Elf64_Ehdr *elf64;
	Elf64_Phdr *elf64ph;

	prom = (int (*)(void *)) promptr;
	chosen_handle = finddevice("/chosen");
	if (chosen_handle == (void *) -1)
		exit();
	if (getprop(chosen_handle, "stdout", &stdout, sizeof(stdout)) != 4)
		exit();
	stderr = stdout;
	if (getprop(chosen_handle, "stdin", &stdin, sizeof(stdin)) != 4)
		exit();

	printf("\n\rzImage starting: loaded at 0x%x\n\r", (unsigned)&_start);

	initrd.size = (unsigned long)(_initrd_end - _initrd_start);
	initrd.memsize = initrd.size;
	if ( initrd.size > 0 ) {
		initrd.addr = (RAM_END - initrd.size) & ~0xFFF;
		a1 = a2 = 0;
		claim(initrd.addr, RAM_END - initrd.addr, 0);
		printf("initial ramdisk moving 0x%lx <- 0x%lx (%lx bytes)\n\r",
		       initrd.addr, (unsigned long)_initrd_start, initrd.size);
		memcpy((void *)initrd.addr, (void *)_initrd_start, initrd.size);
	}

	vmlinuz.addr = (unsigned long)_vmlinux_start;
	vmlinuz.size = (unsigned long)(_vmlinux_end - _vmlinux_start);
	vmlinux.addr = (unsigned long)(void *)-1;
	vmlinux_filesize = (unsigned long)&_vmlinux_filesize;
	vmlinux.size = PAGE_ALIGN(vmlinux_filesize);
	vmlinux.memsize = (unsigned long)&_vmlinux_memsize;

	claim_size = vmlinux.memsize /* PPPBBB: + fudge for bi_recs */;
	for(claim_addr = PROG_START;
	    claim_addr <= PROG_START * 8;
	    claim_addr += 0x100000) {
		printf("    trying: 0x%08lx\n\r", claim_addr);
		vmlinux.addr = (unsigned long)claim(claim_addr, claim_size, 0);
		if ((void *)vmlinux.addr != (void *)-1) break;
	}
	if ((void *)vmlinux.addr == (void *)-1) {
		printf("claim error, can't allocate kernel memory\n\r");
		exit();
	}

	/* PPPBBB: should kernel always be gziped? */
	if (*(unsigned short *)vmlinuz.addr == 0x1f8b) {
		avail_ram = scratch;
		begin_avail = avail_high = avail_ram;
		end_avail = scratch + sizeof(scratch);
		printf("gunzipping (0x%lx <- 0x%lx:0x%0lx)...",
		       vmlinux.addr, vmlinuz.addr, vmlinuz.addr+vmlinuz.size);
		gunzip((void *)vmlinux.addr, vmlinux.size,
			(unsigned char *)vmlinuz.addr, (int *)&vmlinuz.size);
		printf("done %lu bytes\n\r", vmlinuz.size);
		printf("%u bytes of heap consumed, max in use %u\n\r",
		       (unsigned)(avail_high - begin_avail), heap_max);
	} else {
		memmove((void *)vmlinux.addr,(void *)vmlinuz.addr,vmlinuz.size);
	}

	/* Skip over the ELF header */
	elf64 = (Elf64_Ehdr *)vmlinux.addr;
	if ( elf64->e_ident[EI_MAG0]  != ELFMAG0	||
	     elf64->e_ident[EI_MAG1]  != ELFMAG1	||
	     elf64->e_ident[EI_MAG2]  != ELFMAG2	||
	     elf64->e_ident[EI_MAG3]  != ELFMAG3	||
	     elf64->e_ident[EI_CLASS] != ELFCLASS64	||
	     elf64->e_ident[EI_DATA]  != ELFDATA2MSB	||
	     elf64->e_type            != ET_EXEC	||
	     elf64->e_machine         != EM_PPC64 )
	{
		printf("Error: not a valid PPC64 ELF file!\n\r");
		exit();
	}

	elf64ph = (Elf64_Phdr *)((unsigned long)elf64 +
				(unsigned long)elf64->e_phoff);
	for(i=0; i < (unsigned int)elf64->e_phnum ;i++,elf64ph++) {
		if (elf64ph->p_type == PT_LOAD && elf64ph->p_offset != 0)
			break;
	}
	printf("... skipping 0x%lx bytes of ELF header\n\r",
			(unsigned long)elf64ph->p_offset);
	vmlinux.addr += (unsigned long)elf64ph->p_offset;
	vmlinux.size -= (unsigned long)elf64ph->p_offset;

	flush_cache((void *)vmlinux.addr, vmlinux.memsize);

	bi_recs = make_bi_recs(vmlinux.addr + vmlinux.memsize);

	kernel_entry = (kernel_entry_t)vmlinux.addr;
	printf( "kernel:\n\r"
		"        entry addr = 0x%lx\n\r"
		"        a1         = 0x%lx,\n\r"
		"        a2         = 0x%lx,\n\r"
		"        prom       = 0x%lx,\n\r"
		"        bi_recs    = 0x%lx,\n\r",
		(unsigned long)kernel_entry, a1, a2,
		(unsigned long)prom, (unsigned long)bi_recs);

	kernel_entry( a1, a2, prom, bi_recs );

	printf("Error: Linux kernel returned to zImage bootloader!\n\r");

	exit();
}

static struct bi_record *
make_bi_recs(unsigned long addr)
{
	struct bi_record *bi_recs;
	struct bi_record *rec;

	bi_recs = rec = bi_rec_init(addr);

	rec = bi_rec_alloc(rec, 2);
	rec->tag = BI_FIRST;
	/* rec->data[0] = ...;	# Written below before return */
	/* rec->data[1] = ...;	# Written below before return */

	rec = bi_rec_alloc_bytes(rec, strlen("chrpboot")+1);
	rec->tag = BI_BOOTLOADER_ID;
	sprintf( (char *)rec->data, "chrpboot");

	rec = bi_rec_alloc(rec, 2);
	rec->tag = BI_MACHTYPE;
	rec->data[0] = PLATFORM_PSERIES;
	rec->data[1] = 1;

	if ( initrd.size > 0 ) {
		rec = bi_rec_alloc(rec, 2);
		rec->tag = BI_INITRD;
		rec->data[0] = initrd.addr;
		rec->data[1] = initrd.size;
	}

	if ( sysmap.size > 0 ) {
		rec = bi_rec_alloc(rec, 2);
		rec->tag = BI_SYSMAP;
		rec->data[0] = (unsigned long)sysmap.addr;
		rec->data[1] = (unsigned long)sysmap.size;
	}

	rec = bi_rec_alloc(rec, 1);
	rec->tag = BI_LAST;
	rec->data[0] = (bi_rec_field)bi_recs;

	/* Save the _end_ address of the bi_rec's in the first bi_rec
	 * data field for easy access by the kernel.
	 */
	bi_recs->data[0] = (bi_rec_field)rec;
	bi_recs->data[1] = (bi_rec_field)rec + rec->size - (bi_rec_field)bi_recs;

	return bi_recs;
}

struct memchunk {
	unsigned int size;
	unsigned int pad;
	struct memchunk *next;
};

static struct memchunk *freechunks;

void *zalloc(void *x, unsigned items, unsigned size)
{
	void *p;
	struct memchunk **mpp, *mp;

	size *= items;
	size = _ALIGN(size, sizeof(struct memchunk));
	heap_use += size;
	if (heap_use > heap_max)
		heap_max = heap_use;
	for (mpp = &freechunks; (mp = *mpp) != 0; mpp = &mp->next) {
		if (mp->size == size) {
			*mpp = mp->next;
			return mp;
		}
	}
	p = avail_ram;
	avail_ram += size;
	if (avail_ram > avail_high)
		avail_high = avail_ram;
	if (avail_ram > end_avail) {
		printf("oops... out of memory\n\r");
		pause();
	}
	return p;
}

void zfree(void *x, void *addr, unsigned nb)
{
	struct memchunk *mp = addr;

	nb = _ALIGN(nb, sizeof(struct memchunk));
	heap_use -= nb;
	if (avail_ram == addr + nb) {
		avail_ram = addr;
		return;
	}
	mp->size = nb;
	mp->next = freechunks;
	freechunks = mp;
}

#define HEAD_CRC	2
#define EXTRA_FIELD	4
#define ORIG_NAME	8
#define COMMENT		0x10
#define RESERVED	0xe0

#define DEFLATED	8

void gunzip(void *dst, int dstlen, unsigned char *src, int *lenp)
{
	z_stream s;
	int r, i, flags;

	/* skip header */
	i = 10;
	flags = src[3];
	if (src[2] != DEFLATED || (flags & RESERVED) != 0) {
		printf("bad gzipped data\n\r");
		exit();
	}
	if ((flags & EXTRA_FIELD) != 0)
		i = 12 + src[10] + (src[11] << 8);
	if ((flags & ORIG_NAME) != 0)
		while (src[i++] != 0)
			;
	if ((flags & COMMENT) != 0)
		while (src[i++] != 0)
			;
	if ((flags & HEAD_CRC) != 0)
		i += 2;
	if (i >= *lenp) {
		printf("gunzip: ran out of data in header\n\r");
		exit();
	}

	s.zalloc = zalloc;
	s.zfree = zfree;
	r = inflateInit2(&s, -MAX_WBITS);
	if (r != Z_OK) {
		printf("inflateInit2 returned %d\n\r", r);
		exit();
	}
	s.next_in = src + i;
	s.avail_in = *lenp - i;
	s.next_out = dst;
	s.avail_out = dstlen;
	r = inflate(&s, Z_FINISH);
	if (r != Z_OK && r != Z_STREAM_END) {
		printf("inflate returned %d msg: %s\n\r", r, s.msg);
		exit();
	}
	*lenp = s.next_out - (unsigned char *) dst;
	inflateEnd(&s);
}

