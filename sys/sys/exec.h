/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)exec.h	8.3 (Berkeley) 1/21/94
 * $FreeBSD: src/sys/sys/exec.h,v 1.32.18.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef _SYS_EXEC_H_
#define _SYS_EXEC_H_

/*
 * The following structure is found at the top of the user stack of each
 * user process. The ps program uses it to locate argv and environment
 * strings. Programs that wish ps to display other information may modify
 * it; normally ps_argvstr points to the argv vector, and ps_nargvstr
 * is the same as the program's argc. The fields ps_envstr and ps_nenvstr
 * are the equivalent for the environment.
 */
struct ps_strings {
	char	**ps_argvstr;	/* first of 0 or more argument strings */
	unsigned int ps_nargvstr; /* the number of argument strings */
	char	**ps_envstr;	/* first of 0 or more environment strings */
	unsigned int ps_nenvstr; /* the number of environment strings */
};

/*
 * Address of ps_strings structure (in user space).
 */
#define	PS_STRINGS	(USRSTACK - sizeof(struct ps_strings))
#define SPARE_USRSPACE	4096

struct image_params;

struct execsw {
	int (*ex_imgact)(struct image_params *);
	const char *ex_name;
};

#include <machine/exec.h>

#ifdef _KERNEL
#include <sys/cdefs.h>

int exec_map_first_page(struct image_params *);        
void exec_unmap_first_page(struct image_params *);       

int exec_register(const struct execsw *);
int exec_unregister(const struct execsw *);

/*
 * note: name##_mod cannot be const storage because the
 * linker_file_sysinit() function modifies _file in the
 * moduledata_t.
 */

#include <sys/module.h>

#define EXEC_SET(name, execsw_arg) \
	static int __CONCAT(name,_modevent)(module_t mod, int type, \
	    void *data) \
	{ \
		struct execsw *exec = (struct execsw *)data; \
		int error = 0; \
		switch (type) { \
		case MOD_LOAD: \
			/* printf(#name " module loaded\n"); */ \
			error = exec_register(exec); \
			if (error) \
				printf(__XSTRING(name) "register failed\n"); \
			break; \
		case MOD_UNLOAD: \
			/* printf(#name " module unloaded\n"); */ \
			error = exec_unregister(exec); \
			if (error) \
				printf(__XSTRING(name) " unregister failed\n");\
			break; \
		default: \
			error = EOPNOTSUPP; \
			break; \
		} \
		return error; \
	} \
	static moduledata_t __CONCAT(name,_mod) = { \
		__XSTRING(name), \
		__CONCAT(name,_modevent), \
		(void *)& execsw_arg \
	}; \
	DECLARE_MODULE(name, __CONCAT(name,_mod), SI_SUB_EXEC, SI_ORDER_ANY)
#endif

#endif
