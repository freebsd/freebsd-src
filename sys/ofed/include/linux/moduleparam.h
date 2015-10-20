/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013, 2014 Mellanox Technologies, Ltd.
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
 *
 * $FreeBSD$
 */
#ifndef	_LINUX_MODULEPARAM_H_
#define	_LINUX_MODULEPARAM_H_

#include <linux/types.h>

/*
 * These are presently not hooked up to anything.  In linux the parameters
 * can be set when modules are loaded.  On FreeBSD these could be mapped
 * to kenv in the future.
 */
struct kernel_param;

typedef int (*param_set_fn)(const char *val, struct kernel_param *kp);
typedef int (*param_get_fn)(char *buffer, struct kernel_param *kp);

struct kernel_param {
	const char	*name;
	u16		perm;
	u16		flags;
	param_set_fn	set;
	param_get_fn	get;
	union {
		void	*arg;
		struct kparam_string	*str;
		struct kparam_array	*arr;
	} un;
};

#define	KPARAM_ISBOOL	2

struct kparam_string {
	unsigned int maxlen;
	char *string;
};

struct kparam_array
{
	unsigned int	max;
	unsigned int	*num;
	param_set_fn	set;
	param_get_fn	get;
	unsigned int	elemsize;
	void 		*elem;
};

static inline void
param_sysinit(struct kernel_param *param)
{
}

#define	module_param_call(name, set, get, arg, perm)			\
	static struct kernel_param __param_##name =			\
	    { #name, perm, 0, set, get, { arg } };			\
	SYSINIT(name##_param_sysinit, SI_SUB_DRIVERS, SI_ORDER_FIRST,	\
	    param_sysinit, &__param_##name);

#define	module_param_string(name, string, len, perm)

#define	module_param_named(name, var, type, mode)			\
	module_param_call(name, param_set_##type, param_get_##type, &var, mode)

#define	module_param(var, type, mode)					\
	module_param_named(var, var, type, mode)

#define module_param_array(var, type, addr_argc, mode)                  \
        module_param_named(var, var, type, mode)

#define	MODULE_PARM_DESC(name, desc)

static inline int
param_set_byte(const char *val, struct kernel_param *kp)
{

	return 0;
}

static inline int
param_get_byte(char *buffer, struct kernel_param *kp)
{

	return 0;
}


static inline int
param_set_short(const char *val, struct kernel_param *kp)
{

	return 0;
}

static inline int
param_get_short(char *buffer, struct kernel_param *kp)
{

	return 0;
}


static inline int
param_set_ushort(const char *val, struct kernel_param *kp)
{

	return 0;
}

static inline int
param_get_ushort(char *buffer, struct kernel_param *kp)
{

	return 0;
}


static inline int
param_set_int(const char *val, struct kernel_param *kp)
{

	return 0;
}

static inline int
param_get_int(char *buffer, struct kernel_param *kp)
{

	return 0;
}


static inline int
param_set_uint(const char *val, struct kernel_param *kp)
{

	return 0;
}

static inline int
param_get_uint(char *buffer, struct kernel_param *kp)
{

	return 0;
}


static inline int
param_set_long(const char *val, struct kernel_param *kp)
{

	return 0;
}

static inline int
param_get_long(char *buffer, struct kernel_param *kp)
{

	return 0;
}


static inline int
param_set_ulong(const char *val, struct kernel_param *kp)
{

	return 0;
}

static inline int
param_get_ulong(char *buffer, struct kernel_param *kp)
{

	return 0;
}


static inline int
param_set_charp(const char *val, struct kernel_param *kp)
{

	return 0;
}

static inline int
param_get_charp(char *buffer, struct kernel_param *kp)
{

	return 0;
}


static inline int
param_set_bool(const char *val, struct kernel_param *kp)
{

	return 0;
}

static inline int
param_get_bool(char *buffer, struct kernel_param *kp)
{

	return 0;
}

#endif	/* _LINUX_MODULEPARAM_H_ */
