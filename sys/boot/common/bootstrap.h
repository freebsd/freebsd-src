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
 * $FreeBSD: src/sys/boot/common/bootstrap.h,v 1.24.2.2 2000/07/06 00:34:02 ps Exp $
 */

#include <sys/types.h>
#include <sys/queue.h>

/* XXX debugging */
extern struct console vidconsole;
#define MARK(s, c) {vidconsole.c_out(s); vidconsole.c_out(c); while (!vidconsole.c_ready()) ; vidconsole.c_in();}

/*
 * Generic device specifier; architecture-dependant 
 * versions may be larger, but should be allowed to
 * overlap.
 */
struct devdesc 
{
    struct devsw	*d_dev;
    int			d_type;
#define DEVT_NONE	0
#define DEVT_DISK	1
#define DEVT_NET	2
};

/* Commands and return values; nonzero return sets command_errmsg != NULL */
typedef int	(bootblk_cmd_t)(int argc, char *argv[]);
extern char	*command_errmsg;	
extern char	command_errbuf[];	/* XXX blah, length */
#define CMD_OK		0
#define CMD_ERROR	1

/* interp.c */
extern void	interact(void);
extern int	include(char *filename);

/* interp_parse.c */
extern int	parse(int *argc, char ***argv, char *str);

/* interp_forth.c */
extern void	bf_init(void);
extern int	bf_run(char *line);

/* boot.c */
extern int	autoboot(int delay, char *prompt);
extern void	autoboot_maybe(void);
extern int	getrootmount(char *rootdev);

/* misc.c */
extern char	*unargv(int argc, char *argv[]);
extern void	hexdump(caddr_t region, size_t len);
extern size_t	strlenout(vm_offset_t str);
extern char	*strdupout(vm_offset_t str);

/* bcache.c */
extern int	bcache_init(int nblks, size_t bsize);
extern void	bcache_flush();

/*
 * Disk block cache
 */
struct bcache_devdata
{
    int         (*dv_strategy)(void *devdata, int rw, daddr_t blk, size_t size, void *buf, size_t *rsize);
    void	*dv_devdata;
};

/*
 * Modular console support.
 */
struct console 
{
    char	*c_name;
    char	*c_desc;
    int		c_flags;
#define C_PRESENTIN	(1<<0)
#define C_PRESENTOUT	(1<<1)
#define C_ACTIVEIN	(1<<2)
#define C_ACTIVEOUT	(1<<3)
    void	(* c_probe)(struct console *cp);	/* set c_flags to match hardware */
    int		(* c_init)(int arg);			/* reinit XXX may need more args */
    void	(* c_out)(int c);			/* emit c */
    int		(* c_in)(void);				/* wait for and return input */
    int		(* c_ready)(void);			/* return nonzer if input waiting */
};
extern struct console	*consoles[];
extern void		cons_probe(void);

/*
 * Plug-and-play enumerator/configurator interface.
 */
struct pnphandler 
{
    char	*pp_name;		/* handler/bus name */
    void	(* pp_enumerate)(void);	/* enumerate PnP devices, add to chain */
};

struct pnpident
{
    char			*id_ident;	/* ASCII identifier, actual format varies with bus/handler */
    STAILQ_ENTRY(pnpident)	id_link;
};

struct pnpinfo
{
    char			*pi_desc;	/* ASCII description, optional */
    int				pi_revision;	/* optional revision (or -1) if not supported */
    char			*pi_module;	/* module/args nominated to handle device */
    int				pi_argc;	/* module arguments */
    char			**pi_argv;
    struct pnphandler		*pi_handler;	/* handler which detected this device */
    STAILQ_HEAD(,pnpident)	pi_ident;	/* list of identifiers */
    STAILQ_ENTRY(pnpinfo)	pi_link;
};

extern struct pnphandler	*pnphandlers[];		/* provided by MD code */

extern void			pnp_addident(struct pnpinfo *pi, char *ident);
extern struct pnpinfo		*pnp_allocinfo(void);
extern void			pnp_freeinfo(struct pnpinfo *pi);
extern void			pnp_addinfo(struct pnpinfo *pi);
extern char			*pnp_eisaformat(u_int8_t *data);

/*
 *  < 0	- No ISA in system
 * == 0	- Maybe ISA, search for read data port
 *  > 0	- ISA in system, value is read data port address
 */
extern int			isapnp_readport;

/*
 * Module metadata header.
 *
 * Metadata are allocated on our heap, and copied into kernel space
 * before executing the kernel.
 */
struct module_metadata 
{
    size_t			md_size;
    u_int16_t			md_type;
    struct module_metadata	*md_next;
    char			md_data[0];	/* data are immediately appended */
};

/*
 * Loaded module information.
 *
 * At least one module (the kernel) must be loaded in order to boot.
 * The kernel is always loaded first.
 *
 * String fields (m_name, m_type) should be dynamically allocated.
 */
struct loaded_module
{
    char			*m_name;	/* module name */
    char			*m_type;	/* verbose module type, eg 'ELF kernel', 'pnptable', etc. */
    char			*m_args;	/* arguments for the module */
    struct module_metadata	*m_metadata;	/* metadata that will be placed in the module directory */
    int				m_loader;	/* index of the loader that read the file */
    vm_offset_t			m_addr;		/* load address */
    size_t			m_size;		/* module size */
    struct loaded_module	*m_next;	/* next module */
};

struct module_format
{
    /* Load function must return EFTYPE if it can't handle the module supplied */
    int		(* l_load)(char *filename, vm_offset_t dest, struct loaded_module **result);
    /* Only a loader that will load a kernel (first module) should have an exec handler */
    int		(* l_exec)(struct loaded_module *mp);
};
extern struct module_format	*module_formats[];	/* supplied by consumer */
extern struct loaded_module	*loaded_modules;
extern int			mod_load(char *name, int argc, char *argv[]);
extern int			mod_loadobj(char *type, char *name);
extern struct loaded_module	*mod_findmodule(char *name, char *type);
extern void			mod_addmetadata(struct loaded_module *mp, int type, size_t size, void *p);
extern struct module_metadata	*mod_findmetadata(struct loaded_module *mp, int type);
extern void			mod_discard(struct loaded_module *mp);
extern struct loaded_module	*mod_allocmodule(void);


/* MI module loaders */
extern int		aout_loadmodule(char *filename, vm_offset_t dest, struct loaded_module **result);
extern vm_offset_t	aout_findsym(char *name, struct loaded_module *mp);

extern int	elf_loadmodule(char *filename, vm_offset_t dest, struct loaded_module **result);

#ifndef NEW_LINKER_SET
#include <sys/linker_set.h>

/* XXX just for conversion's sake, until we move to the new linker set code */

#define SET_FOREACH(pvar, set)				\
	    for ((char*) pvar = set.ls_items;			\
		 (char*) pvar < (char*) &set.ls_items[set.ls_length];	\
		 pvar++)

#else /* NEW_LINKER_SET */

/*
 * Private macros, not to be used outside this header file.
 */
#define __MAKE_SET(set, sym)						\
	static void *__CONCAT(__setentry,__LINE__)			\
	__attribute__((__section__("set_" #set),__unused__)) = &sym
#define __SET_BEGIN(set)						\
	({ extern void *__CONCAT(__start_set_,set);			\
	    &__CONCAT(__start_set_,set); })
#define __SET_END(set)							\
	({ extern void *__CONCAT(__stop_set_,set);			\
	    &__CONCAT(__stop_set_,set); })

/*
 * Public macros.
 */

/* Add an entry to a set. */
#define DATA_SET(set, sym) __MAKE_SET(set, sym)

/*
 * Iterate over all the elements of a set.
 *
 * Sets always contain addresses of things, and "pvar" points to words
 * containing those addresses.  Thus is must be declared as "type **pvar",
 * and the address of each set item is obtained inside the loop by "*pvar".
 */
#define SET_FOREACH(pvar, set)						\
	for (pvar = (__typeof__(pvar))__SET_BEGIN(set);			\
	    pvar < (__typeof__(pvar))__SET_END(set); pvar++)
#endif

/*
 * Support for commands 
 */
struct bootblk_command 
{
    const char		*c_name;
    const char		*c_desc;
    bootblk_cmd_t	*c_fn;
};

#define COMMAND_SET(tag, key, desc, func)				\
    static bootblk_cmd_t func;						\
    static struct bootblk_command _cmd_ ## tag = { key, desc, func };	\
    DATA_SET(Xcommand_set, _cmd_ ## tag);

extern struct linker_set Xcommand_set;

/* 
 * The intention of the architecture switch is to provide a convenient
 * encapsulation of the interface between the bootstrap MI and MD code.
 * MD code may selectively populate the switch at runtime based on the
 * actual configuration of the target system.
 */
struct arch_switch
{
    /* Automatically load modules as required by detected hardware */
    int			(* arch_autoload)();
    /* Locate the device for (name), return pointer to tail in (*path) */
    int			(*arch_getdev)(void **dev, const char *name, const char **path);
    /* Copy from local address space to module address space, similar to bcopy() */
    int			(*arch_copyin)(void *src, vm_offset_t dest, size_t len);
    /* Copy to local address space from module address space, similar to bcopy() */
    int			(*arch_copyout)(vm_offset_t src, void *dest, size_t len);
    /* Read from file to module address space, same semantics as read() */
    int			(*arch_readin)(int fd, vm_offset_t dest, size_t len);
    /* Perform ISA byte port I/O (only for systems with ISA) */
    int			(*arch_isainb)(int port);
    void		(*arch_isaoutb)(int port, int value);
};
extern struct arch_switch archsw;

/* This must be provided by the MD code, but should it be in the archsw? */
extern void		delay(int delay);

extern void		dev_cleanup(void);
