/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef	_LINUX_MODULE_H_
#define	_LINUX_MODULE_H_

#include <linux/list.h>
#include <linux/compiler.h>
#include <linux/kobject.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>

#define MODULE_AUTHOR(name)
#define MODULE_DESCRIPTION(name)
#define MODULE_LICENSE(name)
#define	MODULE_VERSION(name)

#define	THIS_MODULE	((struct module *)0)

#define	EXPORT_SYMBOL(name)
#define	EXPORT_SYMBOL_GPL(name)

#include <sys/linker.h>

static inline void
_module_run(void *arg)
{
	void (*fn)(void);
#ifdef OFED_DEBUG_INIT
	char name[1024];
	caddr_t pc;
	long offset;

	pc = (caddr_t)arg;
	if (linker_search_symbol_name(pc, name, sizeof(name), &offset) != 0)
		printf("Running ??? (%p)\n", pc);
	else
		printf("Running %s (%p)\n", name, pc);
#endif
	fn = arg;
	DROP_GIANT();
	fn();
	PICKUP_GIANT();
}

#define	module_init(fn)							\
	SYSINIT(fn, SI_SUB_RUN_SCHEDULER, SI_ORDER_FIRST, _module_run, (fn))

/*
 * XXX This is a freebsdism designed to work around not having a module
 * load order resolver built in.
 */
#define	module_init_order(fn, order)					\
	SYSINIT(fn, SI_SUB_RUN_SCHEDULER, (order), _module_run, (fn))

#define	module_exit(fn)						\
	SYSUNINIT(fn, SI_SUB_RUN_SCHEDULER, SI_ORDER_FIRST, _module_run, (fn))

#define	module_get(module)
#define	module_put(module)
#define	try_module_get(module)	1

#endif	/* _LINUX_MODULE_H_ */
