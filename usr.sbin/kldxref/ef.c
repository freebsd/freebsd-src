/*
 * Copyright (c) 2000, Boris Popov
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/exec.h>
#include <sys/queue.h>
#include <sys/kernel.h>
#include <sys/reboot.h>
#include <sys/linker.h>
#include <string.h>
#include <machine/elf.h>
#include <stand.h>
#define FREEBSD_ELF
#include <link.h>

#include <err.h>

#include "ef.h"

static void ef_print_phdr(Elf_Phdr *);
static u_long ef_get_offset(elf_file_t, Elf_Off);
static int ef_parse_dynamic(elf_file_t);

void
ef_print_phdr(Elf_Phdr *phdr)
{

	if ((phdr->p_flags & PF_W) == 0) {
		printf("text=0x%lx ", (long)phdr->p_filesz);
	} else {
		printf("data=0x%lx", (long)phdr->p_filesz);
		if (phdr->p_filesz < phdr->p_memsz)
			printf("+0x%lx", (long)(phdr->p_memsz - phdr->p_filesz));
		printf(" ");
	}
}

u_long
ef_get_offset(elf_file_t ef, Elf_Off off)
{
	Elf_Phdr *ph;
	int i;

	for (i = 0; i < ef->ef_nsegs; i++) {
		ph = ef->ef_segs[i];
		if (off >= ph->p_vaddr && off < ph->p_vaddr + ph->p_memsz) {
			return ph->p_offset + (off - ph->p_vaddr);
		}
	}
	return 0;
}

/*
 * next three functions copied from link_elf.c
 */
static unsigned long
elf_hash(const char *name)
{
	const unsigned char *p = (const unsigned char *) name;
	unsigned long h = 0;
	unsigned long g;

	while (*p != '\0') {
		h = (h << 4) + *p++;
		if ((g = h & 0xf0000000) != 0)
			h ^= g >> 24;
		h &= ~g;
	}
	return h;
}

int
ef_lookup_symbol(elf_file_t ef, const char* name, Elf_Sym** sym)
{
	unsigned long symnum;
	Elf_Sym* symp;
	char *strp;
	unsigned long hash;

	/* First, search hashed global symbols */
	hash = elf_hash(name);
	symnum = ef->ef_buckets[hash % ef->ef_nbuckets];

	while (symnum != STN_UNDEF) {
		if (symnum >= ef->ef_nchains) {
			warnx("ef_lookup_symbol: file %s have corrupted symbol table\n",
			    ef->ef_name);
			return ENOENT;
		}

		symp = ef->ef_symtab + symnum;
		if (symp->st_name == 0) {
			warnx("ef_lookup_symbol: file %s have corrupted symbol table\n",
			    ef->ef_name);
			return ENOENT;
		}

		strp = ef->ef_strtab + symp->st_name;

		if (strcmp(name, strp) == 0) {
			if (symp->st_shndx != SHN_UNDEF ||
			    (symp->st_value != 0 &&
				ELF_ST_TYPE(symp->st_info) == STT_FUNC)) {
				*sym = symp;
				return 0;
			} else
				return ENOENT;
		}

		symnum = ef->ef_chains[symnum];
	}

	return ENOENT;
}

int
ef_parse_dynamic(elf_file_t ef)
{
	Elf_Dyn *dp;
	Elf_Hashelt hashhdr[2];
/*	int plttype = DT_REL;*/
	int error;

	for (dp = ef->ef_dyn; dp->d_tag != DT_NULL; dp++) {
		switch (dp->d_tag) {
		case DT_HASH:
			error = ef_read(ef, ef_get_offset(ef, dp->d_un.d_ptr),
			    sizeof(hashhdr),  hashhdr);
			if (error) {
				warnx("can't read hash header (%lx)",
				    ef_get_offset(ef, dp->d_un.d_ptr));
				return error;
			}
			ef->ef_nbuckets = hashhdr[0];
			ef->ef_nchains = hashhdr[1];
			error = ef_read_entry(ef, -1, 
			    (hashhdr[0] + hashhdr[1]) * sizeof(Elf_Hashelt),
			    (void**)&ef->ef_hashtab);
			if (error) {
				warnx("can't read hash table");
				return error;
			}
			ef->ef_buckets = ef->ef_hashtab;
			ef->ef_chains = ef->ef_buckets + ef->ef_nbuckets;
			break;
		case DT_STRTAB:
			ef->ef_stroff = dp->d_un.d_ptr;
			break;
		case DT_STRSZ:
			ef->ef_strsz = dp->d_un.d_val;
			break;
		case DT_SYMTAB:
			ef->ef_symoff = dp->d_un.d_ptr;
			break;
		case DT_SYMENT:
			if (dp->d_un.d_val != sizeof(Elf_Sym))
				return EFTYPE;
			break;
		}
	}
	if (ef->ef_symoff == 0) {
		warnx("%s: no .dynsym section found\n", ef->ef_name);
		return EFTYPE;
	}
	if (ef->ef_stroff == 0) {
		warnx("%s: no .dynstr section found\n", ef->ef_name);
		return EFTYPE;
	}
	if (ef_read_entry(ef, ef_get_offset(ef, ef->ef_symoff),
	    ef->ef_nchains * sizeof(Elf_Sym),
		(void**)&ef->ef_symtab) != 0) {
		if (ef->ef_verbose)
			warnx("%s: can't load .dynsym section (0x%lx)",
			    ef->ef_name, (long)ef->ef_symoff);
		return EIO;
	}
	if (ef_read_entry(ef, ef_get_offset(ef, ef->ef_stroff), ef->ef_strsz,
		(void**)&ef->ef_strtab) != 0) {
		warnx("can't load .dynstr section");
		return EIO;
	}
	return 0;
}

int
ef_read(elf_file_t ef, Elf_Off offset, size_t len, void*dest)
{
	ssize_t r;

	if (offset != (Elf_Off)-1) {
		if (lseek(ef->ef_fd, offset, SEEK_SET) == -1)
			return EIO;
	}

	r = read(ef->ef_fd, dest, len);
	if (r != -1 && (size_t)r == len)
		return 0;
	else
		return EIO;
}

int
ef_read_entry(elf_file_t ef, Elf_Off offset, size_t len, void**ptr)
{
	int error;

	*ptr = malloc(len);
	if (*ptr == NULL)
		return ENOMEM;
	error = ef_read(ef, offset, len, *ptr);
	if (error)
		free(*ptr);
	return error;
}

int
ef_seg_read(elf_file_t ef, Elf_Off offset, size_t len, void*dest)
{
	u_long ofs = ef_get_offset(ef, offset);

	if (ofs == 0) {
		if (ef->ef_verbose)
			warnx("ef_seg_read(%s): zero offset (%lx:%ld)",
			    ef->ef_name, (long)offset, ofs);
		return EFAULT;
	}
	return ef_read(ef, ofs, len, dest);
}

int
ef_seg_read_entry(elf_file_t ef, Elf_Off offset, size_t len, void**ptr)
{
	int error;

	*ptr = malloc(len);
	if (*ptr == NULL)
		return ENOMEM;
	error = ef_seg_read(ef, offset, len, *ptr);
	if (error)
		free(*ptr);
	return error;
}

int
ef_open(const char *filename, elf_file_t ef, int verbose)
{
	Elf_Ehdr *hdr;
	int fd;
	int error;
	int phlen, res;
	int nsegs;
	Elf_Phdr *phdr, *phdyn, *phphdr, *phlimit;

	bzero(ef, sizeof(*ef));
	if (filename == NULL)
		return EFTYPE;
	ef->ef_verbose = verbose;
	if ((fd = open(filename, O_RDONLY)) == -1)
		return errno;
	ef->ef_fd = fd;
	ef->ef_name = strdup(filename);
	hdr = (Elf_Ehdr *)&ef->ef_hdr;
	do {
		res = read(fd, hdr, sizeof(*hdr));
		error = EFTYPE;
		if (res != sizeof(*hdr))
			break;
		if (!IS_ELF(*hdr))
			break;
		if (hdr->e_ident[EI_CLASS] != ELF_TARG_CLASS ||
		    hdr->e_ident[EI_DATA] != ELF_TARG_DATA ||
		    hdr->e_ident[EI_VERSION] != EV_CURRENT ||
		    hdr->e_version != EV_CURRENT ||
		    hdr->e_machine != ELF_TARG_MACH ||
		    hdr->e_phentsize != sizeof(Elf_Phdr))
			break;
		phlen = hdr->e_phnum * sizeof(Elf_Phdr);
		if (ef_read_entry(ef, hdr->e_phoff, phlen,
		    (void**)&ef->ef_ph) != 0)
			break;
		phdr = ef->ef_ph;
		phlimit = phdr + hdr->e_phnum;
		nsegs = 0;
		phdyn = NULL;
		phphdr = NULL;
		while (phdr < phlimit) {
			if (verbose > 1)
				ef_print_phdr(phdr);
			switch (phdr->p_type) {
			case PT_LOAD:
				if (nsegs == 2) {
					warnx("%s: too many sections",
					    filename);
					break;
				}
				ef->ef_segs[nsegs++] = phdr;
				break;
			case PT_PHDR:
				phphdr = phdr;
				break;
			case PT_DYNAMIC:
				phdyn = phdr;
				break;
			}
			phdr++;
		}
		if (verbose > 1)
			printf("\n");
		ef->ef_nsegs = nsegs;
		if (phdyn == NULL) {
			warnx("file isn't dynamically-linked");
			break;
		}
		if (ef_read_entry(ef, phdyn->p_offset,
			phdyn->p_filesz, (void**)&ef->ef_dyn) != 0) {
			printf("ef_read_entry failed\n");
			break;
		}
		error = ef_parse_dynamic(ef);
		if (error)
			break;
		if (hdr->e_type == ET_DYN) {
			ef->ef_type = EFT_KLD;
/*			pad = (u_int)dest & PAGE_MASK;
			if (pad)
				dest += PAGE_SIZE - pad;*/
			error = 0;
		} else if (hdr->e_type == ET_EXEC) {
/*			dest = hdr->e_entry;
			if (dest == 0)
				break;*/
			ef->ef_type = EFT_KERNEL;
			error = 0;
		} else
			break;
	} while(0);
	if (error) {
		ef_close(ef);
		if (ef->ef_verbose)
			warnc(error, "elf_open(%s)", filename);
	}
	return error;
}

int
ef_close(elf_file_t ef)
{
	close(ef->ef_fd);
/*	if (ef->ef_fpage)
		free(ef->ef_fpage);*/
	if (ef->ef_name)
		free(ef->ef_name);
	return 0;
}
