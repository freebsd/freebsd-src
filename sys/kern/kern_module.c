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
 *	$Id: kern_module.c,v 1.3 1997/10/24 05:29:07 jmg Exp $
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/sysproto.h>
#include <sys/sysent.h>
#include <sys/module.h>
#include <sys/linker.h>

#define M_MODULE	M_TEMP		/* XXX */

typedef TAILQ_HEAD(, module) modulelist_t;
struct module {
    TAILQ_ENTRY(module)	link;		/* chain together all modules */
    TAILQ_ENTRY(module)	flink;		/* all modules in a file */
    struct linker_file*	file;		/* file which contains this module */
    int			refs;		/* reference count */
    int			id;		/* unique id number */
    char		*name;		/* module name */
    modeventhand_t	handler;	/* event handler */
    void		*arg;		/* argument for handler */
};

#define MOD_EVENT(mod, type) (mod)->handler((mod), (type), (mod)->arg)

static modulelist_t modules;
static int nextid = 1;

static void module_shutdown(int, void*);

static void
module_init(void* arg)
{
    TAILQ_INIT(&modules);
    at_shutdown(module_shutdown, 0, SHUTDOWN_POST_SYNC);
}

SYSINIT(module, SI_SUB_KMEM, SI_ORDER_ANY, module_init, 0);

static void
module_shutdown(int arg1, void* arg2)
{
    module_t mod;
    int error;

    for (mod = TAILQ_FIRST(&modules); mod; mod = TAILQ_NEXT(mod, link))
	MOD_EVENT(mod, MOD_SHUTDOWN);
}

void
module_register_init(void *arg)
{
    moduledata_t* data = (moduledata_t*) arg;
    int error;

    if (error = module_register(data->name, data->evhand, data->priv))
	printf("module_register_init: module_register(%s, %x, %x) returned %d",
	       data->name, data->evhand, data->priv, error);
}

int
module_register(const char* name, modeventhand_t handler, void* arg)
{
    size_t namelen;
    module_t newmod;
    int error;

    namelen = strlen(name) + 1;
    newmod = (module_t) malloc(sizeof(struct module) + namelen,
			       M_MODULE, M_WAITOK);
    if (newmod == 0)
	return ENOMEM;

    newmod->refs = 1;
    newmod->id = nextid++;
    newmod->name = (char *) (newmod + 1);
    strcpy(newmod->name, name);
    newmod->handler = handler;
    newmod->arg = arg;
    TAILQ_INSERT_TAIL(&modules, newmod, link);

    if (linker_current_file) {
	TAILQ_INSERT_TAIL(&linker_current_file->modules, newmod, flink);
	newmod->file = linker_current_file;
    } else
	newmod->file = 0;

    if (error = MOD_EVENT(newmod, MOD_LOAD)) {
	module_release(newmod);
	return error;
    }

    return 0;
}

void
module_reference(module_t mod)
{
    MOD_DPF(REFS, ("module_reference: before, refs=%d\n", mod->refs));

    mod->refs++;
}

void
module_release(module_t mod)
{
    if (mod->refs <= 0)
	panic("module_release: bad reference count");

    MOD_DPF(REFS, ("module_release: before, refs=%d\n", mod->refs));

    mod->refs--;
    if (mod->refs == 0) {
	TAILQ_REMOVE(&modules, mod, link);
	if (mod->file) {
	    TAILQ_REMOVE(&mod->file->modules, mod, flink);
	}
	free(mod, M_MODULE);
    }
}

module_t
module_lookupbyname(const char* name)
{
    module_t mod;

    for (mod = TAILQ_FIRST(&modules); mod; mod = TAILQ_NEXT(mod, link)) {
	if (!strcmp(mod->name, name))
	    return mod;
    }

    return 0;
}

module_t
module_lookupbyid(int modid)
{
    module_t mod;

    for (mod = TAILQ_FIRST(&modules); mod; mod = TAILQ_NEXT(mod, link)) {
	if (mod->id == modid)
	    return mod;
    }

    return 0;
}

int
module_unload(module_t mod)
{
    return MOD_EVENT(mod, MOD_UNLOAD);
}

int
module_getid(module_t mod)
{
    return mod->id;
}

module_t
module_getfnext(module_t mod)
{
    return TAILQ_NEXT(mod, flink);
}

/*
 * Syscalls.
 */
int
modnext(struct proc* p, struct modnext_args* uap, int* retval)
{
    module_t mod;

    *retval = -1;
    if (SCARG(uap, modid) == 0) {
	mod = TAILQ_FIRST(&modules);
	if (mod) {
	    *retval = mod->id;
	    return 0;
	} else
	    return ENOENT;
    }

    mod = module_lookupbyid(SCARG(uap, modid));
    if (!mod)
	return ENOENT;

    if (TAILQ_NEXT(mod, link))
	*retval = TAILQ_NEXT(mod, link)->id;
    else
	*retval = 0;
    return 0;
}

int
modfnext(struct proc* p, struct modfnext_args* uap, int* retval)
{
    module_t mod;

    *retval = -1;

    mod = module_lookupbyid(SCARG(uap, modid));
    if (!mod)
	return ENOENT;

    if (TAILQ_NEXT(mod, flink))
	*retval = TAILQ_NEXT(mod, flink)->id;
    else
	*retval = 0;
    return 0;
}

int
modstat(struct proc* p, struct modstat_args* uap, int* retval)
{
    module_t mod;
    int error = 0;
    int namelen;
    int version;
    struct module_stat* stat;

    mod = module_lookupbyid(SCARG(uap, modid));
    if (!mod)
	return ENOENT;

    stat = SCARG(uap, stat);

    /*
     * Check the version of the user's structure.
     */
    if (error = copyin(&stat->version, &version, sizeof(version)))
	goto out;
    if (version != sizeof(struct module_stat)) {
	error = EINVAL;
	goto out;
    }

    namelen = strlen(mod->name) + 1;
    if (namelen > MAXMODNAME)
	namelen = MAXMODNAME;
    if (error = copyout(mod->name, &stat->name[0], namelen))
	goto out;

    if (error = copyout(&mod->refs, &stat->refs, sizeof(int)))
	goto out;
    if (error = copyout(&mod->id, &stat->id, sizeof(int)))
	goto out;

    *retval = 0;

out:
    return error;
}

int
modfind(struct proc* p, struct modfind_args* uap, int* retval)
{
    int error = 0;
    char name[MAXMODNAME];
    module_t mod;

    if (error = copyinstr(SCARG(uap, name), name, sizeof name, 0))
	goto out;

    mod = module_lookupbyname(name);
    if (!mod)
	error = ENOENT;
    else
	*retval = mod->id;

out:
    return error;
}
