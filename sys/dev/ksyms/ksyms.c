/*-
 * Copyright (c) 2008-2009, Stacey Son <sson@freebsd.org>
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
#include <sys/systm.h>
#include <sys/kernel.h>

#include <sys/conf.h>
#include <sys/elf.h>
#include <sys/ksyms.h>
#include <sys/linker.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/resourcevar.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include <machine/elf.h>

#include <vm/pmap.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_map.h>

#include "linker_if.h"

#define SHDR_NULL	0
#define SHDR_SYMTAB	1
#define SHDR_STRTAB	2
#define SHDR_SHSTRTAB	3

#define SHDR_NUM	4

#define STR_SYMTAB	".symtab"
#define STR_STRTAB	".strtab"
#define STR_SHSTRTAB	".shstrtab" 

#define KSYMS_DNAME	"ksyms"

static	d_open_t 	ksyms_open;
static	d_read_t	ksyms_read;
static	d_close_t	ksyms_close;
static	d_ioctl_t	ksyms_ioctl;
static	d_mmap_t	ksyms_mmap;

static struct cdevsw ksyms_cdevsw = {
    .d_version	=	D_VERSION,
    .d_flags	=	D_PSEUDO | D_TRACKCLOSE,
    .d_open	=	ksyms_open,
    .d_close	=	ksyms_close,
    .d_read	=	ksyms_read,
    .d_ioctl	=	ksyms_ioctl,
    .d_mmap	=	ksyms_mmap,
    .d_name	=	KSYMS_DNAME	
};

struct ksyms_softc {
	LIST_ENTRY(ksyms_softc)	sc_list;
	vm_offset_t 		sc_uaddr;
	size_t 			sc_usize;
	pmap_t			sc_pmap;
	struct proc	       *sc_proc;
};

static struct mtx 		 ksyms_mtx;
static struct cdev 		*ksyms_dev;
static LIST_HEAD(, ksyms_softc)	 ksyms_list = 
	LIST_HEAD_INITIALIZER(ksyms_list);

static const char 	ksyms_shstrtab[] = 
	"\0" STR_SYMTAB "\0" STR_STRTAB "\0" STR_SHSTRTAB "\0";

struct ksyms_hdr {
	Elf_Ehdr	kh_ehdr;
	Elf_Phdr	kh_txtphdr;
	Elf_Phdr	kh_datphdr;
	Elf_Shdr	kh_shdr[SHDR_NUM];
	char		kh_shstrtab[sizeof(ksyms_shstrtab)];
};
	
struct tsizes {
	size_t		ts_symsz;
	size_t		ts_strsz;
};

struct toffsets {
	vm_offset_t	to_symoff;
	vm_offset_t	to_stroff;
	unsigned	to_stridx;
	size_t		to_resid;
};

static MALLOC_DEFINE(M_KSYMS, "KSYMS", "Kernel Symbol Table");

/*
 * Get the symbol and string table sizes for a kernel module. Add it to the
 * running total. 
 */
static int
ksyms_size_permod(linker_file_t lf, void *arg)
{
	struct tsizes *ts;
	const Elf_Sym *symtab;
	caddr_t strtab;
	long syms;
	
	ts = arg;
    
	syms = LINKER_SYMTAB_GET(lf, &symtab);
	ts->ts_symsz += syms * sizeof(Elf_Sym);
	ts->ts_strsz += LINKER_STRTAB_GET(lf, &strtab);
	
	return (0);
}

/*
 * For kernel module get the symbol and string table sizes, returning the
 * totals in *ts. 
 */
static void 
ksyms_size_calc(struct tsizes *ts)
{
	ts->ts_symsz = 0;
	ts->ts_strsz = 0;
    
	(void) linker_file_foreach(ksyms_size_permod, ts);
}

#define KSYMS_EMIT(src, des, sz) do {				\
		copyout(src, (void *)des, sz);			\
		des += sz;					\
	} while (0)

#define SYMBLKSZ	256 * sizeof (Elf_Sym)

/*
 * For a kernel module, add the symbol and string tables into the
 * snapshot buffer.  Fix up the offsets in the tables.
 */
static int
ksyms_add(linker_file_t lf, void *arg)
{
	struct toffsets *to;
	const Elf_Sym *symtab;
	Elf_Sym *symp;
	caddr_t strtab;
	long symsz;
	size_t strsz, numsyms;
	linker_symval_t symval;
	char *buf;
	int i, nsyms, len;
	
	to = arg;
    
	MOD_SLOCK;
	numsyms =  LINKER_SYMTAB_GET(lf, &symtab);
	strsz = LINKER_STRTAB_GET(lf, &strtab);
	symsz = numsyms * sizeof(Elf_Sym);
	
	buf = malloc(SYMBLKSZ, M_KSYMS, M_WAITOK);
	
	while (symsz > 0) {
		len = min(SYMBLKSZ, symsz);
		bcopy(symtab, buf, len);

		/* 
		 * Fix up symbol table for kernel modules: 
		 *   string offsets need adjusted 
		 *   symbol values made absolute
		 */
		symp = (Elf_Sym *) buf;
		nsyms = len / sizeof (Elf_Sym);
		for (i = 0; i < nsyms; i++) {
			symp[i].st_name += to->to_stridx;
			if (lf->id > 1 && LINKER_SYMBOL_VALUES(lf, 
				(c_linker_sym_t) &symtab[i], &symval) == 0) {
				symp[i].st_value = (uintptr_t) symval.value;
			}
		}

		if (len > to->to_resid) { 
			MOD_SUNLOCK;
			free(buf, M_KSYMS);
			return (ENXIO);
		} else
			to->to_resid -= len;
		KSYMS_EMIT(buf, to->to_symoff, len);

		symtab += nsyms;
		symsz -= len;
	}
	free(buf, M_KSYMS);
	MOD_SUNLOCK;
	
	if (strsz > to->to_resid)
		return (ENXIO);
	else
		to->to_resid -= strsz;
	KSYMS_EMIT(strtab, to->to_stroff, strsz);
	to->to_stridx += strsz;
	
	return (0);
}

/*
 * Create a single ELF symbol table for the kernel and kernel modules loaded
 * at this time. Write this snapshot out in the process address space. Return
 * 0 on success, otherwise error.
 */
static int
ksyms_snapshot(struct tsizes *ts, vm_offset_t uaddr, size_t resid)
{

	struct ksyms_hdr *hdr;
	struct toffsets	 to;
	int error = 0;

	/* Be kernel stack friendly */
	hdr = malloc(sizeof (*hdr), M_KSYMS, M_WAITOK|M_ZERO);

	/* 
	 * Create the ELF header. 
	 */
	hdr->kh_ehdr.e_ident[EI_PAD] = 0;
	hdr->kh_ehdr.e_ident[EI_MAG0] = ELFMAG0;
	hdr->kh_ehdr.e_ident[EI_MAG1] = ELFMAG1;
	hdr->kh_ehdr.e_ident[EI_MAG2] = ELFMAG2;
	hdr->kh_ehdr.e_ident[EI_MAG3] = ELFMAG3;
	hdr->kh_ehdr.e_ident[EI_DATA] = ELF_DATA;
	hdr->kh_ehdr.e_ident[EI_OSABI] = ELFOSABI_FREEBSD;
	hdr->kh_ehdr.e_ident[EI_CLASS] = ELF_CLASS;
	hdr->kh_ehdr.e_ident[EI_VERSION] = EV_CURRENT;
	hdr->kh_ehdr.e_ident[EI_ABIVERSION] = 0;
	hdr->kh_ehdr.e_type = ET_EXEC;
	hdr->kh_ehdr.e_machine = ELF_ARCH;
	hdr->kh_ehdr.e_version = EV_CURRENT;
	hdr->kh_ehdr.e_entry = 0;
	hdr->kh_ehdr.e_phoff = offsetof(struct ksyms_hdr, kh_txtphdr);
	hdr->kh_ehdr.e_shoff = offsetof(struct ksyms_hdr, kh_shdr);
	hdr->kh_ehdr.e_flags = 0;
	hdr->kh_ehdr.e_ehsize = sizeof(Elf_Ehdr);
	hdr->kh_ehdr.e_phentsize = sizeof(Elf_Phdr);
	hdr->kh_ehdr.e_phnum = 2;	/* Text and Data */ 
	hdr->kh_ehdr.e_shentsize = sizeof(Elf_Shdr);
	hdr->kh_ehdr.e_shnum = SHDR_NUM;
	hdr->kh_ehdr.e_shstrndx = SHDR_SHSTRTAB;

	/* 
	 * Add both the text and data Program headers. 
	 */
	hdr->kh_txtphdr.p_type = PT_LOAD;
	/* XXX - is there a way to put the actual .text addr/size here? */
	hdr->kh_txtphdr.p_vaddr = 0;  
	hdr->kh_txtphdr.p_memsz = 0; 
	hdr->kh_txtphdr.p_flags = PF_R | PF_X;
    
	hdr->kh_datphdr.p_type = PT_LOAD;
	/* XXX - is there a way to put the actual .data addr/size here? */
	hdr->kh_datphdr.p_vaddr = 0; 
	hdr->kh_datphdr.p_memsz = 0; 
	hdr->kh_datphdr.p_flags = PF_R | PF_W | PF_X;

	/* 
	 * Add the Section headers: null, symtab, strtab, shstrtab, 
	 */

	/* First section header - null */
	
	/* Second section header - symtab */
	hdr->kh_shdr[SHDR_SYMTAB].sh_name = 1; /* String offset (skip null) */
	hdr->kh_shdr[SHDR_SYMTAB].sh_type = SHT_SYMTAB;
	hdr->kh_shdr[SHDR_SYMTAB].sh_flags = 0;
	hdr->kh_shdr[SHDR_SYMTAB].sh_addr = 0;
	hdr->kh_shdr[SHDR_SYMTAB].sh_offset = sizeof(*hdr);
	hdr->kh_shdr[SHDR_SYMTAB].sh_size = ts->ts_symsz;
	hdr->kh_shdr[SHDR_SYMTAB].sh_link = SHDR_STRTAB;	
	hdr->kh_shdr[SHDR_SYMTAB].sh_info = ts->ts_symsz / sizeof(Elf_Sym); 
	hdr->kh_shdr[SHDR_SYMTAB].sh_addralign = sizeof(long);
	hdr->kh_shdr[SHDR_SYMTAB].sh_entsize = sizeof(Elf_Sym);

	/* Third section header - strtab */
	hdr->kh_shdr[SHDR_STRTAB].sh_name = 1 + sizeof(STR_SYMTAB); 
	hdr->kh_shdr[SHDR_STRTAB].sh_type = SHT_STRTAB;
	hdr->kh_shdr[SHDR_STRTAB].sh_flags = 0;
	hdr->kh_shdr[SHDR_STRTAB].sh_addr = 0;
	hdr->kh_shdr[SHDR_STRTAB].sh_offset = 
	    hdr->kh_shdr[SHDR_SYMTAB].sh_offset + ts->ts_symsz;	
	hdr->kh_shdr[SHDR_STRTAB].sh_size = ts->ts_strsz;
	hdr->kh_shdr[SHDR_STRTAB].sh_link = 0;
	hdr->kh_shdr[SHDR_STRTAB].sh_info = 0;
	hdr->kh_shdr[SHDR_STRTAB].sh_addralign = sizeof(char);
	hdr->kh_shdr[SHDR_STRTAB].sh_entsize = 0;
	
	/* Fourth section - shstrtab */
	hdr->kh_shdr[SHDR_SHSTRTAB].sh_name = 1 + sizeof(STR_SYMTAB) +
	    sizeof(STR_STRTAB);
	hdr->kh_shdr[SHDR_SHSTRTAB].sh_type = SHT_STRTAB;
	hdr->kh_shdr[SHDR_SHSTRTAB].sh_flags = 0;
	hdr->kh_shdr[SHDR_SHSTRTAB].sh_addr = 0;
	hdr->kh_shdr[SHDR_SHSTRTAB].sh_offset = 
	    offsetof(struct ksyms_hdr, kh_shstrtab);
	hdr->kh_shdr[SHDR_SHSTRTAB].sh_size = sizeof(ksyms_shstrtab);
	hdr->kh_shdr[SHDR_SHSTRTAB].sh_link = 0;
	hdr->kh_shdr[SHDR_SHSTRTAB].sh_info = 0;
	hdr->kh_shdr[SHDR_SHSTRTAB].sh_addralign = 0 /* sizeof(char) */;
	hdr->kh_shdr[SHDR_SHSTRTAB].sh_entsize = 0;

	/* Copy shstrtab into the header */
	bcopy(ksyms_shstrtab, hdr->kh_shstrtab, sizeof(ksyms_shstrtab));
	
	to.to_symoff = uaddr + hdr->kh_shdr[SHDR_SYMTAB].sh_offset;
	to.to_stroff = uaddr + hdr->kh_shdr[SHDR_STRTAB].sh_offset;
	to.to_stridx = 0;
	if (sizeof(struct ksyms_hdr) > resid) {
		free(hdr, M_KSYMS);
		return (ENXIO);
	}
	to.to_resid = resid - sizeof(struct ksyms_hdr);

	/* Emit Header */
	copyout(hdr, (void *)uaddr, sizeof(struct ksyms_hdr));
	
	free(hdr, M_KSYMS);

	/* Add symbol and string tables for each kernelmodule */
	error = linker_file_foreach(ksyms_add, &to); 

	if (to.to_resid != 0)
		return (ENXIO);

	return (error);
}

/*
 * Map some anonymous memory in user space of size sz, rounded up to the page
 * boundary.
 */
static int
ksyms_map(struct thread *td, vm_offset_t *addr, size_t sz)
{
	struct vmspace *vms = td->td_proc->p_vmspace;
	int error;
	vm_size_t size;

	
	/* 
	 * Map somewhere after heap in process memory.
	 */
	PROC_LOCK(td->td_proc);
	*addr = round_page((vm_offset_t)vms->vm_daddr + 
	    lim_max(td->td_proc, RLIMIT_DATA));
	PROC_UNLOCK(td->td_proc);

	/* round size up to page boundry */
	size = (vm_size_t) round_page(sz);
    
	error = vm_mmap(&vms->vm_map, addr, size, PROT_READ | PROT_WRITE, 
	    VM_PROT_ALL, MAP_PRIVATE | MAP_ANON, OBJT_DEFAULT, NULL, 0);
	
	return (error);
}

/*
 * Unmap memory in user space.
 */
static int
ksyms_unmap(struct thread *td, vm_offset_t addr, size_t sz)
{
	vm_map_t map;
	vm_size_t size;
    
	map = &td->td_proc->p_vmspace->vm_map;
	size = (vm_size_t) round_page(sz);	

	if (!vm_map_remove(map, addr, addr + size))
		return (EINVAL);

	return (0);
}

static void
ksyms_cdevpriv_dtr(void *data)
{
	struct ksyms_softc *sc;

	sc = (struct ksyms_softc *)data; 

	mtx_lock(&ksyms_mtx);
	LIST_REMOVE(sc, sc_list);
	mtx_unlock(&ksyms_mtx);
	free(sc, M_KSYMS);
}

/* ARGSUSED */
static int
ksyms_open(struct cdev *dev, int flags, int fmt __unused, struct thread *td)
{
	struct tsizes ts;
	size_t total_elf_sz;
	int error, try;
	struct ksyms_softc *sc;
	
	/* 
	 *  Limit one open() per process. The process must close()
	 *  before open()'ing again.
	 */ 
	mtx_lock(&ksyms_mtx);
	LIST_FOREACH(sc, &ksyms_list, sc_list) {
		if (sc->sc_proc == td->td_proc) {
			mtx_unlock(&ksyms_mtx);
			return (EBUSY);
		}
	}

	sc = (struct ksyms_softc *) malloc(sizeof (*sc), M_KSYMS, 
	    M_NOWAIT|M_ZERO);

	if (sc == NULL) {
		mtx_unlock(&ksyms_mtx);
		return (ENOMEM);
	}
	sc->sc_proc = td->td_proc;
	sc->sc_pmap = &td->td_proc->p_vmspace->vm_pmap;
	LIST_INSERT_HEAD(&ksyms_list, sc, sc_list);
	mtx_unlock(&ksyms_mtx);

	error = devfs_set_cdevpriv(sc, ksyms_cdevpriv_dtr);
	if (error) 
		goto failed;

	/*
	 * MOD_SLOCK doesn't work here (because of a lock reversal with 
	 * KLD_SLOCK).  Therefore, simply try upto 3 times to get a "clean"
	 * snapshot of the kernel symbol table.  This should work fine in the
	 * rare case of a kernel module being loaded/unloaded at the same
	 * time. 
	 */
	for(try = 0; try < 3; try++) {
		/*
	 	* Map a buffer in the calling process memory space and
	 	* create a snapshot of the kernel symbol table in it.
	 	*/
	
		/* Compute the size of buffer needed. */
		ksyms_size_calc(&ts);
		total_elf_sz = sizeof(struct ksyms_hdr) + ts.ts_symsz + 
			ts.ts_strsz; 

		error = ksyms_map(td, &(sc->sc_uaddr), 
				(vm_size_t) total_elf_sz);
		if (error)
			break;
		sc->sc_usize = total_elf_sz;	

		error = ksyms_snapshot(&ts, sc->sc_uaddr, total_elf_sz); 
		if (!error)  {
			/* Successful Snapshot */
			return (0); 
		}
		
		/* Snapshot failed, unmap the memory and try again */ 
		(void) ksyms_unmap(td, sc->sc_uaddr, sc->sc_usize);
	}

failed:
	ksyms_cdevpriv_dtr(sc);
	return (error);
}

/* ARGSUSED */
static int
ksyms_read(struct cdev *dev, struct uio *uio, int flags __unused)
{
	int error;
	size_t len, sz;
	struct ksyms_softc *sc;
	off_t off;
	char *buf;
	vm_size_t ubase;
    
	error = devfs_get_cdevpriv((void **)&sc);
	if (error)
		return (error);
    
	off = uio->uio_offset;
	len = uio->uio_resid;	
    
	if (off < 0 || off > sc->sc_usize)
		return (EFAULT);

	if (len > (sc->sc_usize - off))
		len = sc->sc_usize - off;

	if (len == 0)
		return (0);

	/*
	 * Since the snapshot buffer is in the user space we have to copy it
	 * in to the kernel and then back out.  The extra copy saves valuable
	 * kernel memory.
	 */
	buf = malloc(PAGE_SIZE, M_KSYMS, M_WAITOK);
	ubase = sc->sc_uaddr + off;

	while (len) {

		sz = min(PAGE_SIZE, len);
		if (copyin((void *)ubase, buf, sz))
			error = EFAULT;	
		else
			error = uiomove(buf, sz, uio);
		
		if (error)
			break;
	
		len -= sz;
		ubase += sz;
	}
	free(buf, M_KSYMS);

	return (error);
}

/* ARGSUSED */
static int
ksyms_ioctl(struct cdev *dev, u_long cmd, caddr_t data, int32_t flag __unused,
    struct thread *td __unused)
{
	int error = 0;
	struct ksyms_softc *sc;

	error = devfs_get_cdevpriv((void **)&sc);
	if (error)
		return (error);

	switch (cmd) {
	case KIOCGSIZE:
		/* 
		 * Return the size (in bytes) of the symbol table
		 * snapshot.
		 */
		*(size_t *)data = sc->sc_usize;
		break;
		
	case KIOCGADDR:
		/*
		 * Return the address of the symbol table snapshot.
		 * XXX - compat32 version of this?
		 */
		*(void **)data = (void *)sc->sc_uaddr;
		break;
		
	default:
		error = ENOTTY;
		break;
	}

	return (error);
}

/* ARGUSED */
static int
ksyms_mmap(struct cdev *dev, vm_offset_t offset, vm_paddr_t *paddr,
		int prot __unused)
{
    	struct ksyms_softc *sc;
	int error;

	error = devfs_get_cdevpriv((void **)&sc);
	if (error)
		return (error);

	/*
	 * XXX mmap() will actually map the symbol table into the process
	 * address space again.
	 */
	if (offset > round_page(sc->sc_usize) || 
	    (*paddr = pmap_extract(sc->sc_pmap, 
	    (vm_offset_t)sc->sc_uaddr + offset)) == 0) 
		return (-1);

	return (0);
}

/* ARGUSED */
static int
ksyms_close(struct cdev *dev, int flags __unused, int fmt __unused,
		struct thread *td)
{
	int error = 0;
	struct ksyms_softc *sc;
	
	error = devfs_get_cdevpriv((void **)&sc);
	if (error)
		return (error);

	/* Unmap the buffer from the process address space. */
	error = ksyms_unmap(td, sc->sc_uaddr, sc->sc_usize);

	devfs_clear_cdevpriv();

	return (error);
}

/* ARGSUSED */
static int
ksyms_modevent(module_t mod __unused, int type, void *data __unused)
{
	int error = 0;

	switch (type) {
	case MOD_LOAD:
		mtx_init(&ksyms_mtx, "KSyms mtx", NULL, MTX_DEF);
		ksyms_dev = make_dev(&ksyms_cdevsw, 0, UID_ROOT, GID_WHEEL,
		    0444, KSYMS_DNAME);
		break;

	case MOD_UNLOAD:
		if (!LIST_EMPTY(&ksyms_list))
			return (EBUSY);
		destroy_dev(ksyms_dev);
		mtx_destroy(&ksyms_mtx);
		break;

	case MOD_SHUTDOWN:
		break;

	default:
		error = EOPNOTSUPP;
		break;
	}
	return (error);
}

DEV_MODULE(ksyms, ksyms_modevent, NULL);
MODULE_VERSION(ksyms, 1);
