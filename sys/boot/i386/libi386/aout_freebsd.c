/*-
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
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
 *	$Id$
 */

#include <sys/param.h>
#include <sys/exec.h>
#include <sys/imgact_aout.h>
#include <sys/reboot.h>
#include <string.h>
#include <machine/bootinfo.h>
#include <stand.h>

#include "bootstrap.h"
#include "libi386.h"

struct aout_kernel_module
{
    struct loaded_module	m;
    vm_offset_t			m_entry;	/* module entrypoint */
    struct bootinfo		m_bi;		/* legacy bootinfo */
};

static int	aout_loadmodule(char *filename, vm_offset_t dest, struct loaded_module **result);
static int	aout_exec(struct loaded_module *amp);

struct module_format i386_aout = { MF_AOUT, aout_loadmodule, aout_exec };

static int	aout_loadimage(int fd, vm_offset_t loadaddr, struct exec *ehdr);

/*
 * Attempt to load the file (file) as an a.out module.  It will be stored at
 * (dest), and a pointer to a module structure describing the loaded object
 * will be saved in (result).
 */
static int
aout_loadmodule(char *filename, vm_offset_t dest, struct loaded_module **result)
{
    struct aout_kernel_module	*mp;
    struct exec			ehdr;
    int				fd;
    vm_offset_t			addr;
    int				err;
    u_int			pad;

    /*
     * Open the image, read and validate the a.out header 
     *
     * XXX what do kld modules look like?  We only handle kernels here.
     */
    if (filename == NULL)	/* can't handle nameless */
	return(EFTYPE);
    if ((fd = open(filename, O_RDONLY)) == -1)
	return(errno);
    if (read(fd, &ehdr, sizeof(ehdr)) != sizeof(ehdr))
	return(EFTYPE);		/* could be EIO, but may be small file */
    if (N_BADMAG(ehdr))
	return(EFTYPE);

    /* 
     * Ok, we think this is for us.
     */
    mp = malloc(sizeof(struct aout_kernel_module));
    mp->m.m_name = strdup(filename);		/* XXX should we prune the name? */
    mp->m.m_type = "a.out kernel";		/* XXX only if that's what we really are */
    mp->m.m_args = NULL;			/* XXX should we put the bootstrap args here and parse later? */
    mp->m.m_flags = MF_AOUT;			/* we're an a.out kernel */
    mp->m_entry = (vm_offset_t)(ehdr.a_entry & 0xffffff);
    if (dest == 0)
	dest = (vm_offset_t)(ehdr.a_entry & 0x100000);
    if (mod_findmodule(NULL, mp->m.m_type) != NULL) {
	printf("aout_loadmodule: kernel already loaded\n");
	err = EPERM;
	goto out;
    }
    printf("%s at 0x%x\n", filename, dest);
    mp->m.m_addr = addr = dest;

    mp->m.m_size = aout_loadimage(fd, addr, &ehdr);
    printf("\n");
    if (mp->m.m_size == 0)
	goto ioerr;

    /* XXX and if these parts don't exist? */
    mp->m_bi.bi_symtab = mp->m.m_addr + ehdr.a_text + ehdr.a_data + ehdr.a_bss;
    mp->m_bi.bi_esymtab = mp->m_bi.bi_symtab + sizeof(ehdr.a_syms) + ehdr.a_syms;
    
    /* Load OK, return module pointer */
    *result = (struct loaded_module *)mp;
    return(0);

 ioerr:
    err = EIO;
 out:
    close(fd);
    free(mp);
    return(err);
}

/*
 * With the file (fd) open on the image, and (ehdr) containing
 * the exec header, load the image at (addr)
 *
 * Fixup the a_bss field in (ehdr) to reflect the padding added to
 * align the symbol table.
 */
static int
aout_loadimage(int fd, vm_offset_t loadaddr, struct exec *ehdr)
{
    u_int		pad;
    vm_offset_t		addr;
    int			ss;

    addr = loadaddr;
    lseek(fd, N_TXTOFF(*ehdr), SEEK_SET);
    
    /* text segment */
    printf("text=0x%x ", ehdr->a_text);
    if (pread(fd, addr, ehdr->a_text) != ehdr->a_text)
	return(0);
    addr += ehdr->a_text;

    /* data segment */
    printf("data=0x%x ", ehdr->a_data);
    if (pread(fd, addr, ehdr->a_data) != ehdr->a_data)
	return(0);
    addr += ehdr->a_data;

    /* skip the BSS */
    printf("bss=0x%x ", ehdr->a_bss);
    addr += ehdr->a_bss;
    
    /* pad to a page boundary */
    pad = (u_int)addr & PAGE_MASK;
    if (pad != 0) {
	pad = PAGE_SIZE - pad;
	addr += pad;
	ehdr->a_bss += pad;
    }
    /* XXX bi_symtab = addr */

    /* symbol table size */
    vpbcopy(&ehdr->a_syms, addr, sizeof(ehdr->a_syms));
    addr += sizeof(ehdr->a_syms);

    /* symbol table */
    printf("symbols=[0x%x+0x%x+0x%x", pad, sizeof(ehdr->a_syms), ehdr->a_syms);
    if (pread(fd, addr, ehdr->a_syms) != ehdr->a_syms)
	return(0);
    addr += ehdr->a_syms;

    /* string table */
    read(fd, &ss, sizeof(ss));
    vpbcopy(&ss, addr, sizeof(ss));
    addr += sizeof(ss);
    ss -= sizeof(ss);
    printf("+0x%x+0x%x]", sizeof(ss), ss);
    if (pread(fd, addr, ss) != ss)
	return(0);
    /* XXX bi_esymtab = addr */
    addr += ss;
    return(addr - loadaddr);
}


/*
 * There is an a.out kernel and one or more a.out modules loaded.  
 * We wish to start executing the kernel image, so make such 
 * preparations as are required, and do so.
 */
static int
aout_exec(struct loaded_module *amp)
{
    struct aout_kernel_module	*mp = (struct aout_kernel_module *)amp;
    struct loaded_module	*xp;
    struct i386_devdesc		*currdev;
    u_int32_t			argv[6];	/* kernel arguments */
    int				major, bootdevnr;
    vm_offset_t			addr;
    u_int			pad;

    if ((amp->m_flags & MF_FORMATMASK) != MF_AOUT)
	return(EFTYPE);

    /* Boot from whatever the current device is */
    i386_getdev((void **)(&currdev), NULL, NULL);
    switch(currdev->d_type) {
    case DEVT_DISK:	    
	major = 0;			/* XXX in the short term, have to work out a major number here for old kernels */
	bootdevnr = MAKEBOOTDEV(major, 
				currdev->d_kind.biosdisk.slice >> 4, 
				currdev->d_kind.biosdisk.slice & 0xf, 
				currdev->d_kind.biosdisk.unit,
				currdev->d_kind.biosdisk.partition);
	break;
    default:
	printf("aout_loadmodule: WARNING - don't know how to boot from device type %d\n", currdev->d_type);
    }
    free(currdev);

    /* Device data is kept in the kernel argv array */
    argv[1] = bootdevnr;

    argv[0] = bi_getboothowto(amp->m_args);
/*    argv[2] = vtophys(bootinfo);	/* old cyl offset (do we care about this?) */
    argv[3] = 0;
    argv[4] = 0;
    argv[5] = (u_int32_t)vtophys(&(mp->m_bi));

    /* legacy bootinfo structure */
    mp->m_bi.bi_version = BOOTINFO_VERSION;
    mp->m_bi.bi_memsizes_valid = 1;
    /* XXX bi_vesa */
    mp->m_bi.bi_basemem = getbasemem();
    mp->m_bi.bi_extmem = getextmem();

    /* find the last module in the chain */
    for (xp = amp; xp->m_next != NULL; xp = xp->m_next)
	;
    addr = xp->m_addr + xp->m_size;
    /* pad to a page boundary */
    pad = (u_int)addr & PAGE_MASK;
    if (pad != 0) {
	pad = PAGE_SIZE - pad;
	addr += pad;
    }
    /* copy our environment  XXX save addr here as env pointer */
    addr = bi_copyenv(addr);

    /* pad to a page boundary */
    pad = (u_int)addr & PAGE_MASK;
    if (pad != 0) {
	pad = PAGE_SIZE - pad;
	addr += pad;
    }
    /* copy module list and metadata */
    bi_copymodules(addr);
    
#ifdef DEBUG
    {
	int i;
	for (i = 0; i < 6; i++)
	    printf("argv[%d]=%lx\n", i, argv[i]);
    }

    printf("Start @ 0x%lx ...\n", mp->m_entry);
#endif

    startprog(mp->m_entry, 6, argv, (vm_offset_t)0x90000);
    panic("exec returned");
}
