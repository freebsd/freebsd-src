/*-
 * Copyright (c) 1997 Doug Rabson
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

#ifndef _SYS_MODULE_H_
#define _SYS_MODULE_H_

typedef enum modeventtype {
    MOD_LOAD,
    MOD_UNLOAD,
    MOD_SHUTDOWN
} modeventtype_t;

typedef	struct module *module_t;

typedef	int (*modeventhand_t)(module_t mod, int /*modeventtype_t*/ what,
			      void *arg);

/*
 * Struct for registering modules statically via SYSINIT.
 */
typedef struct moduledata {
	char		*name;	/* module name */
	modeventhand_t	evhand;	/* event handler */
	void		*priv;	/* extra data */
} moduledata_t;

/*
 * A module can use this to report module specific data to
 * the user via kldstat(2).
 */
typedef union modspecific {
    int		intval;
    u_int	uintval;
    long	longval;
    u_long	ulongval;
} modspecific_t;

#ifdef _KERNEL

#define MODULE_METADATA(uniquifier, type, data, cval)

#define MODULE_DEPEND(mod, dep, min, pref, max)

#define DECLARE_MODULE(name, data, sub, order) \
    SYSINIT(name##module, sub, order, module_register_init, &data) \
    struct __hack

#define MODULE_VERSION(mod, ver)

void module_register_init(const void *data);
struct linker_file;
int module_register(const struct moduledata *data, struct linker_file *lf);
module_t module_lookupbyname(const char *name);
module_t module_lookupbyid(int modid);
void module_reference(module_t mod);
void module_release(module_t mod);
int module_unload(module_t mod);
int module_getid(module_t mod);
module_t module_getfnext(module_t mod);
void module_setspecific(module_t mod, modspecific_t *datap);

#ifdef MOD_DEBUG

extern int mod_debug;
#define MOD_DEBUG_REFS	1

#define MOD_DPF(cat, args)					\
	do {							\
		if (mod_debug & MOD_DEBUG_##cat) printf args;	\
	} while (0)

#else

#define MOD_DPF(cat, args)

#endif

#endif /* _KERNEL */

#define MAXMODNAME	32

struct module_stat {
    int		version;	/* set to sizeof(struct module_stat) */
    char	name[MAXMODNAME];
    int		refs;
    int		id;
    modspecific_t data;
};

#ifndef _KERNEL

#include <sys/cdefs.h>

__BEGIN_DECLS
int	modnext(int _modid);
int	modfnext(int _modid);
int	modstat(int _modid, struct module_stat* _stat);
int	modfind(const char *_name);
__END_DECLS

#endif

#endif	/* !_SYS_MODULE_H_ */
