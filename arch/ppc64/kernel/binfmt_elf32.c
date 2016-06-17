/*
 * binfmt_elf32.c: Support 32-bit PPC ELF binaries on Power3 and followons.
 * based on the SPARC64 version.
 * Copyright (C) 1995, 1996, 1997, 1998 David S. Miller	(davem@redhat.com)
 * Copyright (C) 1995, 1996, 1997, 1998 Jakub Jelinek	(jj@ultra.linux.cz)
 *
 * Copyright (C) 2000,2001 Ken Aaker (kdaaker@rchland.vnet.ibm.com), IBM Corp
 * Copyright (C) 2001 Anton Blanchard (anton@au.ibm.com), IBM
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#define ELF_ARCH		EM_PPC
#define ELF_CLASS		ELFCLASS32
#define ELF_DATA		ELFDATA2MSB;

#include <asm/processor.h>
#include <linux/module.h>
#include <linux/config.h>
#include <linux/elfcore.h>

struct timeval32
{
	int tv_sec, tv_usec;
};

#define elf_prstatus elf_prstatus32
struct elf_prstatus32
{
	struct elf_siginfo pr_info;	/* Info associated with signal */
	short	pr_cursig;		/* Current signal */
	unsigned int pr_sigpend;	/* Set of pending signals */
	unsigned int pr_sighold;	/* Set of held signals */
	pid_t	pr_pid;
	pid_t	pr_ppid;
	pid_t	pr_pgrp;
	pid_t	pr_sid;
	struct timeval32 pr_utime;	/* User time */
	struct timeval32 pr_stime;	/* System time */
	struct timeval32 pr_cutime;	/* Cumulative user time */
	struct timeval32 pr_cstime;	/* Cumulative system time */
	elf_gregset_t pr_reg;		/* General purpose registers. */
	int pr_fpvalid;		/* True if math co-processor being used. */
};

#define elf_prpsinfo elf_prpsinfo32
struct elf_prpsinfo32
{
	char	pr_state;	/* numeric process state */
	char	pr_sname;	/* char for pr_state */
	char	pr_zomb;	/* zombie */
	char	pr_nice;	/* nice val */
	unsigned int pr_flag;	/* flags */
	u32	pr_uid;
	u32	pr_gid;
	pid_t	pr_pid, pr_ppid, pr_pgrp, pr_sid;
	/* Lots missing */
	char	pr_fname[16];	/* filename of executable */
	char	pr_psargs[ELF_PRARGSZ];	/* initial part of arg list */
};

extern void start_thread32(struct pt_regs *, unsigned long, unsigned long);
#undef start_thread
#define start_thread start_thread32
#define init_elf_binfmt init_elf32_binfmt

#undef CONFIG_BINFMT_ELF
#ifdef CONFIG_BINFMT_ELF32
#define CONFIG_BINFMT_ELF CONFIG_BINFMT_ELF32
#endif
#undef CONFIG_BINFMT_ELF_MODULE
#ifdef CONFIG_BINFMT_ELF32_MODULE
#define CONFIG_BINFMT_ELF_MODULE CONFIG_BINFMT_ELF32_MODULE
#endif

#include "../../../fs/binfmt_elf.c"
