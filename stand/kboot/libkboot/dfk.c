/*
 * Copyright (c) 2025 Netflix, Inc
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Common macros to allow compiling this as a Linux binary or in libsa.
 */
#ifdef _STANDALONE
#include "stand.h"
/* Not ideal, but these are missing in libsa */
#define perror(msg) printf("ERROR %d: %s\n", errno, msg)
#define fprintf(x, ...) printf( __VA_ARGS__ )
#include <machine/elf.h>
#include <sys/param.h>
#include "util.h"
#else
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <asm/bootparam.h>

#define PAGE_SIZE 4096
#define	IS_ELF(ehdr)	((ehdr).e_ident[EI_MAG0] == ELFMAG0 && \
			 (ehdr).e_ident[EI_MAG1] == ELFMAG1 && \
			 (ehdr).e_ident[EI_MAG2] == ELFMAG2 && \
			 (ehdr).e_ident[EI_MAG3] == ELFMAG3)

#define ELF_TARG_CLASS  ELFCLASS64
#define ELF_TARG_MACH   EM_X86_64
#define ELF_TARG_DATA	ELFDATA2LSB
#endif

#define KCORE_PATH "/proc/kcore"
#define KALLSYMS_PATH "/proc/kallsyms"

struct elf_file
{
	uint8_t		buf[PAGE_SIZE];
	int		fd;
};

// All the line_buffer stuff can be replaced by fgetstr()

struct line_buffer
{
	int		fd;
	char		buf[PAGE_SIZE];
	char		*pos;
	char		*eos;
};

/*
 * We just assume we have to fill if we are called.
 */
static bool
lb_fill(struct line_buffer *lb)
{
	ssize_t rv;

	lb->pos = lb->eos = lb->buf;	// Reset to no data condition
	rv = read(lb->fd, lb->buf, sizeof(lb->buf));
	if (rv <= 0)
		return (false);
	lb->pos = lb->buf;
	lb->eos = lb->buf + rv;
	return (true);
}

static bool
lb_fini(struct line_buffer *lb)
{
	close(lb->fd);
	return (true);
}

static bool
lb_init(struct line_buffer *lb, const char *fn)
{
	lb->fd = open(fn, O_RDONLY);
	if (lb->fd == -1)
		return (false);
	lb->pos = lb->eos = lb->buf;
	if (!lb_fill(lb)) {
		lb_fini(lb);
		return (false);
	}
	return (true);
}

// True -> data returned
// False -> EOF / ERROR w/o data
static bool
lb_1line(struct line_buffer *lb, char *buffer, size_t buflen)
{
	char *bufeos = buffer + buflen - 1;	// point at byte for NUL at eos
	char *walker = buffer;

	while (walker < bufeos) {		// < to exclude space for NUL
		if (lb->pos >= lb->eos) {	// Refill empty buffer
			if (!lb_fill(lb)) {	// Hit EOF / error
				if (walker > buffer) // Have data? return it
					break;
				// No data, signal EOF/Error
				return (false);
			}
		}
		*walker = *lb->pos++;
		if (*walker == '\n')
			break;
		walker++;
	}
	/*
	 * We know walker <= bufeos, so NUL will fit.
	 */
	*++walker = '\0';
	return (true);
}

/*
 * Scan /proc/kallsyms to find @symbol and return the value it finds there.
 */
unsigned long
symbol_addr(const char *symbol)
{
	struct line_buffer lb;
	unsigned long addr;
	char line[256];

	if (!lb_init(&lb, KALLSYMS_PATH))
		return (0);
	while (lb_1line(&lb, line, sizeof(line))) {
		char *val, *name, *x, t;

		/*
		 * Parse lines of the form
		 *	val<sp>t<sp>name\n
		 * looking for one with t in [dDbB] (so data) name == symbol,
		 * skipping lines that don't match the pattern.
		 */
		val = line;
		x = strchr(val, ' ');
		if (x == NULL)
			continue;	/* No 1st <sp> */
		*x++ = '\0';
		t = *x++;
		if (strchr("dDbB", t) == NULL)
			continue;	/* Only data types */
		if (*x++ != ' ')
			continue;	/* No 2nd <sp> */
		name = x;
		x = strchr(x, '\n');
		if (x == NULL)
			continue;	/* No traling newline */
		*x++ = '\0';
		if (strcmp(name, symbol) == 0) {
			unsigned long v;
			char *eop = NULL;
			lb_fini(&lb);
			v = strtoul(val, &eop, 16);
			if (*eop == '\0')
				return (v);
			return (0);	/* PARSE ERROR -- what to do? */
		}
		/* No match, try next */
	}

	lb_fini(&lb);
	return (0);
}

/*
 * Parse /proc/kcore to find if we can get the data for @len bytes that are
 * mapped in the kernel at VA @addr. It's a CORE file in ELF format that the
 * kernel exports for the 'safe' areas to touch. We can read random kernel
 * varaibles, but we can't read arbitrary addresses since it doesn't export
 * the direct map.
 */
bool
read_at_address(unsigned long addr, void *buf, size_t len)
{
	struct elf_file ef;
	Elf64_Ehdr *hdr;
	Elf64_Phdr *phdr;
	ssize_t rv;

	bzero(&ef, sizeof(ef));
	ef.fd = open(KCORE_PATH, O_RDONLY);
	if (ef.fd == -1) {
		perror("open " KCORE_PATH "\n");
		return (false);
	}

	/*
	 * Read in the first page. ELF files have a header that says how many
	 * sections are in the file, whre they are, etc. All the Phdr are in the
	 * first page. Read it, verify the headers, then loop through these Phdr
	 * to find the address where addr is mapped to read it.
	 */
	rv = read(ef.fd, ef.buf, sizeof(ef.buf));
	if (rv != sizeof(ef.buf)) {
		perror("short hdr read\n");
		close(ef.fd);
		return (false);
	}
	hdr = (Elf64_Ehdr *)&ef.buf;
	if (!IS_ELF(*hdr)) {
		fprintf(stderr, "Not Elf\n");
		close(ef.fd);
		return (false);
	}
	if (hdr->e_ident[EI_CLASS] != ELF_TARG_CLASS ||	/* Layout ? */
	    hdr->e_ident[EI_DATA] != ELF_TARG_DATA ||
	    hdr->e_ident[EI_VERSION] != EV_CURRENT ||	/* Version ? */
	    hdr->e_version != EV_CURRENT ||
	    hdr->e_machine != ELF_TARG_MACH ||		/* Machine ? */
	    hdr->e_type != ET_CORE) {
		fprintf(stderr, "Not what I expect\n");
		close(ef.fd);
		return (false);
	}

	phdr = (Elf64_Phdr *)(ef.buf + hdr->e_phoff);
	for (int i = 0; i < hdr->e_phnum; i++) {
		if (phdr[i].p_type != PT_LOAD)
			continue;
		if (addr < phdr[i].p_vaddr ||
		    addr >= phdr[i].p_vaddr + phdr[i].p_filesz)
			continue;
		lseek(ef.fd, (off_t)phdr[i].p_offset + addr - phdr[i].p_vaddr,
			SEEK_SET);
		rv = read(ef.fd, buf, len);
		if (rv != len)
			perror("Can't read buffer\n");
		close(ef.fd);
		return (rv == len);
	}

	close(ef.fd);
	return (false);
}

/*
 * Read a value from the Linux kernel. We lookup @sym and read @len bytes into
 * @buf. Returns true if we got it, false on an error.
 */
bool
data_from_kernel(const char *sym, void *buf, size_t len)
{
	unsigned long addr;

	addr = symbol_addr(sym);
	if (addr == 0) {
		fprintf(stderr, "Can't find symbol %s", sym);
		return (false);
	}
	if (!read_at_address(addr, buf, len)) {
		fprintf(stderr, "Can't read from kernel");
		return (false);
	}
	return (true);
}

#ifndef _STANDALONE
/*
 * Silly  little test case to test on a random Linux system.
 */
int
main(int argc, char **argv)
{
	struct boot_params bp;

	if (data_from_kernel("boot_params", &bp, sizeof(bp))) {
		fprintf(stderr, "Something went wrong\n");
	} else {
		printf("sig %#x systab %#lx memmap %#lx mmapsize %d md_size %d md_vers %d\n",
		    bp.efi_info.efi_loader_signature,
		    (long)(bp.efi_info.efi_systab | ((long)bp.efi_info.efi_systab_hi << 32)),
		    (long)(bp.efi_info.efi_memmap | ((long)bp.efi_info.efi_memmap_hi << 32)),
		    bp.efi_info.efi_memmap_size, bp.efi_info.efi_memdesc_size,
		    bp.efi_info.efi_memdesc_version);
	}
}
#endif
