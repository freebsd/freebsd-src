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

#include <sys/types.h>

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
extern void	source(char *filename);

/* boot.c */
extern int	autoboot(int delay, char *prompt);

/* misc.c */
extern char	*unargv(int argc, char *argv[]);

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
 * Module loader.
 */
#define MF_FORMATMASK	0xf
#define MF_AOUT		0
#define MF_ELF		1

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
 */
struct loaded_module
{
    char			*m_name;	/* module name */
    char			*m_type;	/* module type, eg 'kernel', 'pnptable', etc. */
    char			*m_args;	/* arguments for the module */
    void			*m_metadata;	/* metadata that will be placed in the module directory */
    int				m_flags;	/* 0xffff reserved for arch-specific use */
    vm_offset_t			m_addr;		/* load address */
    size_t			m_size;		/* module size */
    struct loaded_module	*m_next;	/* next module */
};

struct module_format
{
    int		l_format;
    /* Load function must return EFTYPE if it can't handle the module supplied */
    int		(* l_load)(char *filename, vm_offset_t dest, struct loaded_module **result);
    int		(* l_exec)(struct loaded_module *amp);
};
extern struct module_format	*module_formats[];	/* supplied by consumer */
extern struct loaded_module	*loaded_modules;
extern int			mod_load(char *name, int argc, char *argv[]);
extern struct loaded_module	*mod_findmodule(char *name, char *type);

/* XXX these belong in <machine/bootinfo.h> */
#define MODINFO_NAME		0x0000
#define MODINFO_TYPE		0x0001
#define MODINFO_ADDR		0x0002
#define MODINFO_SIZE		0x0003
#define MODINFO_METADATA	0x8000


#if defined(__ELF__)

/*
 * Alpha GAS needs an align before the section change.  It seems to assume
 * that after the .previous, it is aligned, so the following .align 3 is
 * ignored.  Since the previous instructions often contain strings, this is
 * a problem.
 */

#ifdef __alpha__
#define MAKE_SET(set, sym)			\
	__asm(".align 3");			\
	__asm(".section .set." #set ",\"aw\"");	\
	__asm(".quad " #sym);			\
	__asm(".previous")
#else
#define MAKE_SET(set, sym)			\
	__asm(".section .set." #set ",\"aw\"");	\
	__asm(".long " #sym);			\
	__asm(".previous")
#endif
#define TEXT_SET(set, sym) MAKE_SET(set, sym)
#define DATA_SET(set, sym) MAKE_SET(set, sym)
#define BSS_SET(set, sym)  MAKE_SET(set, sym)
#define ABS_SET(set, sym)  MAKE_SET(set, sym)

#else

/*
 * Linker set support, directly from <sys/kernel.h>
 * 
 * NB: the constants defined below must match those defined in
 * ld/ld.h.  Since their calculation requires arithmetic, we
 * can't name them symbolically (e.g., 23 is N_SETT | N_EXT).
 */
#define MAKE_SET(set, sym, type) \
	static void const * const __set_##set##_sym_##sym = &sym; \
	__asm(".stabs \"_" #set "\", " #type ", 0, 0, _" #sym)
#define TEXT_SET(set, sym) MAKE_SET(set, sym, 23)
#define DATA_SET(set, sym) MAKE_SET(set, sym, 25)
#define BSS_SET(set, sym)  MAKE_SET(set, sym, 27)
#define ABS_SET(set, sym)  MAKE_SET(set, sym, 21)

#endif

struct linker_set {
    int             ls_length;
    const void      *ls_items[1];	/* really ls_length of them, trailing NULL */
};

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
 * functions called down from the generic code
 */
struct arch_switch
{
    /* Automatically load modules as required by detected hardware */
    int			(* arch_autoload)();
    /* Boot the loaded kernel (first loaded module) */
    int			(* arch_boot)(void);
    /* Locate the device for (name), return pointer to tail in (*path) */
    int			(*arch_getdev)(void **dev, char *name, char **path);
};
extern struct arch_switch archsw;

