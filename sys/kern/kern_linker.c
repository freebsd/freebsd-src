/*-
 * Copyright (c) 1997-2000 Doug Rabson
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/kern/kern_linker.c,v 1.149.2.4.2.1 2008/11/25 02:59:29 kensmith Exp $");

#include "opt_ddb.h"
#include "opt_hwpmc_hooks.h"
#include "opt_mac.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/sysproto.h>
#include <sys/sysent.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sx.h>
#include <sys/module.h>
#include <sys/mount.h>
#include <sys/linker.h>
#include <sys/fcntl.h>
#include <sys/libkern.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>

#include <security/mac/mac_framework.h>

#include "linker_if.h"

#ifdef HWPMC_HOOKS
#include <sys/pmckern.h>
#endif

#ifdef KLD_DEBUG
int kld_debug = 0;
#endif

#define	KLD_LOCK()		sx_xlock(&kld_sx)
#define	KLD_UNLOCK()		sx_xunlock(&kld_sx)
#define	KLD_LOCKED()		sx_xlocked(&kld_sx)
#define	KLD_LOCK_ASSERT() do {						\
	if (!cold)							\
		sx_assert(&kld_sx, SX_XLOCKED);				\
} while (0)

/*
 * static char *linker_search_path(const char *name, struct mod_depend
 * *verinfo);
 */
static const char 	*linker_basename(const char *path);

/*
 * Find a currently loaded file given its filename.
 */
static linker_file_t linker_find_file_by_name(const char* _filename);

/*
 * Find a currently loaded file given its file id.
 */
static linker_file_t linker_find_file_by_id(int _fileid);

/* Metadata from the static kernel */
SET_DECLARE(modmetadata_set, struct mod_metadata);

MALLOC_DEFINE(M_LINKER, "linker", "kernel linker");

linker_file_t linker_kernel_file;

static struct sx kld_sx;	/* kernel linker lock */

/*
 * Load counter used by clients to determine if a linker file has been
 * re-loaded. This counter is incremented for each file load.
 */
static int loadcnt;

static linker_class_list_t classes;
static linker_file_list_t linker_files;
static int next_file_id = 1;
static int linker_no_more_classes = 0;

#define	LINKER_GET_NEXT_FILE_ID(a) do {					\
	linker_file_t lftmp;						\
									\
	KLD_LOCK_ASSERT();						\
retry:									\
	TAILQ_FOREACH(lftmp, &linker_files, link) {			\
		if (next_file_id == lftmp->id) {			\
			next_file_id++;					\
			goto retry;					\
		}							\
	}								\
	(a) = next_file_id;						\
} while(0)


/* XXX wrong name; we're looking at version provision tags here, not modules */
typedef TAILQ_HEAD(, modlist) modlisthead_t;
struct modlist {
	TAILQ_ENTRY(modlist) link;	/* chain together all modules */
	linker_file_t   container;
	const char 	*name;
	int             version;
};
typedef struct modlist *modlist_t;
static modlisthead_t found_modules;

static int	linker_file_add_dependency(linker_file_t file,
		    linker_file_t dep);
static caddr_t	linker_file_lookup_symbol_internal(linker_file_t file,
		    const char* name, int deps);
static int	linker_load_module(const char *kldname,
		    const char *modname, struct linker_file *parent,
		    struct mod_depend *verinfo, struct linker_file **lfpp);
static modlist_t modlist_lookup2(const char *name, struct mod_depend *verinfo);

static char *
linker_strdup(const char *str)
{
	char *result;

	if ((result = malloc((strlen(str) + 1), M_LINKER, M_WAITOK)) != NULL)
		strcpy(result, str);
	return (result);
}

static void
linker_init(void *arg)
{

	sx_init(&kld_sx, "kernel linker");
	TAILQ_INIT(&classes);
	TAILQ_INIT(&linker_files);
}

SYSINIT(linker, SI_SUB_KLD, SI_ORDER_FIRST, linker_init, 0);

static void
linker_stop_class_add(void *arg)
{

	linker_no_more_classes = 1;
}

SYSINIT(linker_class, SI_SUB_KLD, SI_ORDER_ANY, linker_stop_class_add, NULL);

int
linker_add_class(linker_class_t lc)
{

	/*
	 * We disallow any class registration past SI_ORDER_ANY
	 * of SI_SUB_KLD.  We bump the reference count to keep the
	 * ops from being freed.
	 */
	if (linker_no_more_classes == 1)
		return (EPERM);
	kobj_class_compile((kobj_class_t) lc);
	((kobj_class_t)lc)->refs++;	/* XXX: kobj_mtx */
	TAILQ_INSERT_TAIL(&classes, lc, link);
	return (0);
}

static void
linker_file_sysinit(linker_file_t lf)
{
	struct sysinit **start, **stop, **sipp, **xipp, *save;

	KLD_DPF(FILE, ("linker_file_sysinit: calling SYSINITs for %s\n",
	    lf->filename));

	if (linker_file_lookup_set(lf, "sysinit_set", &start, &stop, NULL) != 0)
		return;
	/*
	 * Perform a bubble sort of the system initialization objects by
	 * their subsystem (primary key) and order (secondary key).
	 *
	 * Since some things care about execution order, this is the operation
	 * which ensures continued function.
	 */
	for (sipp = start; sipp < stop; sipp++) {
		for (xipp = sipp + 1; xipp < stop; xipp++) {
			if ((*sipp)->subsystem < (*xipp)->subsystem ||
			    ((*sipp)->subsystem == (*xipp)->subsystem &&
			    (*sipp)->order <= (*xipp)->order))
				continue;	/* skip */
			save = *sipp;
			*sipp = *xipp;
			*xipp = save;
		}
	}

	/*
	 * Traverse the (now) ordered list of system initialization tasks.
	 * Perform each task, and continue on to the next task.
	 */
	mtx_lock(&Giant);
	for (sipp = start; sipp < stop; sipp++) {
		if ((*sipp)->subsystem == SI_SUB_DUMMY)
			continue;	/* skip dummy task(s) */

		/* Call function */
		(*((*sipp)->func)) ((*sipp)->udata);
	}
	mtx_unlock(&Giant);
}

static void
linker_file_sysuninit(linker_file_t lf)
{
	struct sysinit **start, **stop, **sipp, **xipp, *save;

	KLD_DPF(FILE, ("linker_file_sysuninit: calling SYSUNINITs for %s\n",
	    lf->filename));

	if (linker_file_lookup_set(lf, "sysuninit_set", &start, &stop,
	    NULL) != 0)
		return;

	/*
	 * Perform a reverse bubble sort of the system initialization objects
	 * by their subsystem (primary key) and order (secondary key).
	 *
	 * Since some things care about execution order, this is the operation
	 * which ensures continued function.
	 */
	for (sipp = start; sipp < stop; sipp++) {
		for (xipp = sipp + 1; xipp < stop; xipp++) {
			if ((*sipp)->subsystem > (*xipp)->subsystem ||
			    ((*sipp)->subsystem == (*xipp)->subsystem &&
			    (*sipp)->order >= (*xipp)->order))
				continue;	/* skip */
			save = *sipp;
			*sipp = *xipp;
			*xipp = save;
		}
	}

	/*
	 * Traverse the (now) ordered list of system initialization tasks.
	 * Perform each task, and continue on to the next task.
	 */
	mtx_lock(&Giant);
	for (sipp = start; sipp < stop; sipp++) {
		if ((*sipp)->subsystem == SI_SUB_DUMMY)
			continue;	/* skip dummy task(s) */

		/* Call function */
		(*((*sipp)->func)) ((*sipp)->udata);
	}
	mtx_unlock(&Giant);
}

static void
linker_file_register_sysctls(linker_file_t lf)
{
	struct sysctl_oid **start, **stop, **oidp;

	KLD_DPF(FILE,
	    ("linker_file_register_sysctls: registering SYSCTLs for %s\n",
	    lf->filename));

	if (linker_file_lookup_set(lf, "sysctl_set", &start, &stop, NULL) != 0)
		return;

	mtx_lock(&Giant);
	for (oidp = start; oidp < stop; oidp++)
		sysctl_register_oid(*oidp);
	mtx_unlock(&Giant);
}

static void
linker_file_unregister_sysctls(linker_file_t lf)
{
	struct sysctl_oid **start, **stop, **oidp;

	KLD_DPF(FILE, ("linker_file_unregister_sysctls: registering SYSCTLs"
	    " for %s\n", lf->filename));

	if (linker_file_lookup_set(lf, "sysctl_set", &start, &stop, NULL) != 0)
		return;

	mtx_lock(&Giant);
	for (oidp = start; oidp < stop; oidp++)
		sysctl_unregister_oid(*oidp);
	mtx_unlock(&Giant);
}

static int
linker_file_register_modules(linker_file_t lf)
{
	struct mod_metadata **start, **stop, **mdp;
	const moduledata_t *moddata;
	int first_error, error;

	KLD_DPF(FILE, ("linker_file_register_modules: registering modules"
	    " in %s\n", lf->filename));

	if (linker_file_lookup_set(lf, "modmetadata_set", &start,
	    &stop, NULL) != 0) {
		/*
		 * This fallback should be unnecessary, but if we get booted
		 * from boot2 instead of loader and we are missing our
		 * metadata then we have to try the best we can.
		 */
		if (lf == linker_kernel_file) {
			start = SET_BEGIN(modmetadata_set);
			stop = SET_LIMIT(modmetadata_set);
		} else
			return (0);
	}
	first_error = 0;
	for (mdp = start; mdp < stop; mdp++) {
		if ((*mdp)->md_type != MDT_MODULE)
			continue;
		moddata = (*mdp)->md_data;
		KLD_DPF(FILE, ("Registering module %s in %s\n",
		    moddata->name, lf->filename));
		error = module_register(moddata, lf);
		if (error) {
			printf("Module %s failed to register: %d\n",
			    moddata->name, error);
			if (first_error == 0)
				first_error = error;
		}
	}
	return (first_error);
}

static void
linker_init_kernel_modules(void)
{

	linker_file_register_modules(linker_kernel_file);
}

SYSINIT(linker_kernel, SI_SUB_KLD, SI_ORDER_ANY, linker_init_kernel_modules,
    0);

static int
linker_load_file(const char *filename, linker_file_t *result)
{
	linker_class_t lc;
	linker_file_t lf;
	int foundfile, error;

	/* Refuse to load modules if securelevel raised */
	if (securelevel > 0)
		return (EPERM);

	KLD_LOCK_ASSERT();
	lf = linker_find_file_by_name(filename);
	if (lf) {
		KLD_DPF(FILE, ("linker_load_file: file %s is already loaded,"
		    " incrementing refs\n", filename));
		*result = lf;
		lf->refs++;
		return (0);
	}
	foundfile = 0;
	error = 0;

	/*
	 * We do not need to protect (lock) classes here because there is
	 * no class registration past startup (SI_SUB_KLD, SI_ORDER_ANY)
	 * and there is no class deregistration mechanism at this time.
	 */
	TAILQ_FOREACH(lc, &classes, link) {
		KLD_DPF(FILE, ("linker_load_file: trying to load %s\n",
		    filename));
		error = LINKER_LOAD_FILE(lc, filename, &lf);
		/*
		 * If we got something other than ENOENT, then it exists but
		 * we cannot load it for some other reason.
		 */
		if (error != ENOENT)
			foundfile = 1;
		if (lf) {
			error = linker_file_register_modules(lf);
			if (error == EEXIST) {
				linker_file_unload(lf, LINKER_UNLOAD_FORCE);
				return (error);
			}
			KLD_UNLOCK();
			linker_file_register_sysctls(lf);
			linker_file_sysinit(lf);
			KLD_LOCK();
			lf->flags |= LINKER_FILE_LINKED;
			*result = lf;
			return (0);
		}
	}
	/*
	 * Less than ideal, but tells the user whether it failed to load or
	 * the module was not found.
	 */
	if (foundfile) {
		/*
		 * Format not recognized or otherwise unloadable.
		 * When loading a module that is statically built into
		 * the kernel EEXIST percolates back up as the return
		 * value.  Preserve this so that apps like sysinstall
		 * can recognize this special case and not post bogus
		 * dialog boxes.
		 */
		if (error != EEXIST)
			error = ENOEXEC;
	} else
		error = ENOENT;		/* Nothing found */
	return (error);
}

int
linker_reference_module(const char *modname, struct mod_depend *verinfo,
    linker_file_t *result)
{
	modlist_t mod;
	int error;

	KLD_LOCK();
	if ((mod = modlist_lookup2(modname, verinfo)) != NULL) {
		*result = mod->container;
		(*result)->refs++;
		KLD_UNLOCK();
		return (0);
	}

	error = linker_load_module(NULL, modname, NULL, verinfo, result);
	KLD_UNLOCK();
	return (error);
}

int
linker_release_module(const char *modname, struct mod_depend *verinfo,
    linker_file_t lf)
{
	modlist_t mod;
	int error;

	KLD_LOCK();
	if (lf == NULL) {
		KASSERT(modname != NULL,
		    ("linker_release_module: no file or name"));
		mod = modlist_lookup2(modname, verinfo);
		if (mod == NULL) {
			KLD_UNLOCK();
			return (ESRCH);
		}
		lf = mod->container;
	} else
		KASSERT(modname == NULL && verinfo == NULL,
		    ("linker_release_module: both file and name"));
	error =	linker_file_unload(lf, LINKER_UNLOAD_NORMAL);
	KLD_UNLOCK();
	return (error);
}

static linker_file_t
linker_find_file_by_name(const char *filename)
{
	linker_file_t lf;
	char *koname;

	koname = malloc(strlen(filename) + 4, M_LINKER, M_WAITOK);
	sprintf(koname, "%s.ko", filename);

	KLD_LOCK_ASSERT();
	TAILQ_FOREACH(lf, &linker_files, link) {
		if (strcmp(lf->filename, koname) == 0)
			break;
		if (strcmp(lf->filename, filename) == 0)
			break;
	}
	free(koname, M_LINKER);
	return (lf);
}

static linker_file_t
linker_find_file_by_id(int fileid)
{
	linker_file_t lf;

	KLD_LOCK_ASSERT();
	TAILQ_FOREACH(lf, &linker_files, link)
		if (lf->id == fileid && lf->flags & LINKER_FILE_LINKED)
			break;
	return (lf);
}

int
linker_file_foreach(linker_predicate_t *predicate, void *context)
{
	linker_file_t lf;
	int retval = 0;

	KLD_LOCK();
	TAILQ_FOREACH(lf, &linker_files, link) {
		retval = predicate(lf, context);
		if (retval != 0)
			break;
	}
	KLD_UNLOCK();
	return (retval);
}

linker_file_t
linker_make_file(const char *pathname, linker_class_t lc)
{
	linker_file_t lf;
	const char *filename;

	KLD_LOCK_ASSERT();
	filename = linker_basename(pathname);

	KLD_DPF(FILE, ("linker_make_file: new file, filename='%s' for pathname='%s'\n", filename, pathname));
	lf = (linker_file_t)kobj_create((kobj_class_t)lc, M_LINKER, M_WAITOK);
	if (lf == NULL)
		return (NULL);
	lf->refs = 1;
	lf->userrefs = 0;
	lf->flags = 0;
	lf->filename = linker_strdup(filename);
	lf->pathname = linker_strdup(pathname);
	LINKER_GET_NEXT_FILE_ID(lf->id);
	lf->ndeps = 0;
	lf->deps = NULL;
	lf->loadcnt = ++loadcnt;
	lf->sdt_probes = NULL;
	lf->sdt_nprobes = 0;
	STAILQ_INIT(&lf->common);
	TAILQ_INIT(&lf->modules);
	TAILQ_INSERT_TAIL(&linker_files, lf, link);
	return (lf);
}

int
linker_file_unload(linker_file_t file, int flags)
{
	module_t mod, next;
	modlist_t ml, nextml;
	struct common_symbol *cp;
	int error, i;

	/* Refuse to unload modules if securelevel raised. */
	if (securelevel > 0)
		return (EPERM);

	KLD_LOCK_ASSERT();
	KLD_DPF(FILE, ("linker_file_unload: lf->refs=%d\n", file->refs));

	/* Easy case of just dropping a reference. */
	if (file->refs > 1) {
		file->refs--;
		return (0);
	}

	KLD_DPF(FILE, ("linker_file_unload: file is unloading,"
	    " informing modules\n"));

	/*
	 * Inform any modules associated with this file.
	 */
	MOD_XLOCK;
	for (mod = TAILQ_FIRST(&file->modules); mod; mod = next) {
		next = module_getfnext(mod);
		MOD_XUNLOCK;

		/*
		 * Give the module a chance to veto the unload.
		 */
		if ((error = module_unload(mod, flags)) != 0) {
			KLD_DPF(FILE, ("linker_file_unload: module %p"
			    " vetoes unload\n", mod));
			return (error);
		}
		MOD_XLOCK;
		module_release(mod);
	}
	MOD_XUNLOCK;

	TAILQ_FOREACH_SAFE(ml, &found_modules, link, nextml) {
		if (ml->container == file) {
			TAILQ_REMOVE(&found_modules, ml, link);
			free(ml, M_LINKER);
		}
	}

	/*
	 * Don't try to run SYSUNINITs if we are unloaded due to a
	 * link error.
	 */
	if (file->flags & LINKER_FILE_LINKED) {
		linker_file_sysuninit(file);
		linker_file_unregister_sysctls(file);
	}
	TAILQ_REMOVE(&linker_files, file, link);

	if (file->deps) {
		for (i = 0; i < file->ndeps; i++)
			linker_file_unload(file->deps[i], flags);
		free(file->deps, M_LINKER);
		file->deps = NULL;
	}
	while ((cp = STAILQ_FIRST(&file->common)) != NULL) {
		STAILQ_REMOVE_HEAD(&file->common, link);
		free(cp, M_LINKER);
	}

	LINKER_UNLOAD(file);
	if (file->filename) {
		free(file->filename, M_LINKER);
		file->filename = NULL;
	}
	if (file->pathname) {
		free(file->pathname, M_LINKER);
		file->pathname = NULL;
	}
	kobj_delete((kobj_t) file, M_LINKER);
	return (0);
}

int
linker_ctf_get(linker_file_t file, linker_ctf_t *lc)
{
	return (LINKER_CTF_GET(file, lc));
}

static int
linker_file_add_dependency(linker_file_t file, linker_file_t dep)
{
	linker_file_t *newdeps;

	KLD_LOCK_ASSERT();
	newdeps = malloc((file->ndeps + 1) * sizeof(linker_file_t *),
	    M_LINKER, M_WAITOK | M_ZERO);
	if (newdeps == NULL)
		return (ENOMEM);

	if (file->deps) {
		bcopy(file->deps, newdeps,
		    file->ndeps * sizeof(linker_file_t *));
		free(file->deps, M_LINKER);
	}
	file->deps = newdeps;
	file->deps[file->ndeps] = dep;
	file->ndeps++;
	return (0);
}

/*
 * Locate a linker set and its contents.  This is a helper function to avoid
 * linker_if.h exposure elsewhere.  Note: firstp and lastp are really void **.
 * This function is used in this file so we can avoid having lots of (void **)
 * casts.
 */
int
linker_file_lookup_set(linker_file_t file, const char *name,
    void *firstp, void *lastp, int *countp)
{
	int error, locked;

	locked = KLD_LOCKED();
	if (!locked)
		KLD_LOCK();
	error = LINKER_LOOKUP_SET(file, name, firstp, lastp, countp);
	if (!locked)
		KLD_UNLOCK();
	return (error);
}

/*
 * List all functions in a file.
 */
int
linker_file_function_listall(linker_file_t lf,
    linker_function_nameval_callback_t callback_func, void *arg)
{
	return (LINKER_EACH_FUNCTION_NAMEVAL(lf, callback_func, arg));
}

caddr_t
linker_file_lookup_symbol(linker_file_t file, const char *name, int deps)
{
	caddr_t sym;
	int locked;

	locked = KLD_LOCKED();
	if (!locked)
		KLD_LOCK();
	sym = linker_file_lookup_symbol_internal(file, name, deps);
	if (!locked)
		KLD_UNLOCK();
	return (sym);
}

static caddr_t
linker_file_lookup_symbol_internal(linker_file_t file, const char *name,
    int deps)
{
	c_linker_sym_t sym;
	linker_symval_t symval;
	caddr_t address;
	size_t common_size = 0;
	int i;

	KLD_LOCK_ASSERT();
	KLD_DPF(SYM, ("linker_file_lookup_symbol: file=%p, name=%s, deps=%d\n",
	    file, name, deps));

	if (LINKER_LOOKUP_SYMBOL(file, name, &sym) == 0) {
		LINKER_SYMBOL_VALUES(file, sym, &symval);
		if (symval.value == 0)
			/*
			 * For commons, first look them up in the
			 * dependencies and only allocate space if not found
			 * there.
			 */
			common_size = symval.size;
		else {
			KLD_DPF(SYM, ("linker_file_lookup_symbol: symbol"
			    ".value=%p\n", symval.value));
			return (symval.value);
		}
	}
	if (deps) {
		for (i = 0; i < file->ndeps; i++) {
			address = linker_file_lookup_symbol_internal(
			    file->deps[i], name, 0);
			if (address) {
				KLD_DPF(SYM, ("linker_file_lookup_symbol:"
				    " deps value=%p\n", address));
				return (address);
			}
		}
	}
	if (common_size > 0) {
		/*
		 * This is a common symbol which was not found in the
		 * dependencies.  We maintain a simple common symbol table in
		 * the file object.
		 */
		struct common_symbol *cp;

		STAILQ_FOREACH(cp, &file->common, link) {
			if (strcmp(cp->name, name) == 0) {
				KLD_DPF(SYM, ("linker_file_lookup_symbol:"
				    " old common value=%p\n", cp->address));
				return (cp->address);
			}
		}
		/*
		 * Round the symbol size up to align.
		 */
		common_size = (common_size + sizeof(int) - 1) & -sizeof(int);
		cp = malloc(sizeof(struct common_symbol)
		    + common_size + strlen(name) + 1, M_LINKER,
		    M_WAITOK | M_ZERO);
		cp->address = (caddr_t)(cp + 1);
		cp->name = cp->address + common_size;
		strcpy(cp->name, name);
		bzero(cp->address, common_size);
		STAILQ_INSERT_TAIL(&file->common, cp, link);

		KLD_DPF(SYM, ("linker_file_lookup_symbol: new common"
		    " value=%p\n", cp->address));
		return (cp->address);
	}
	KLD_DPF(SYM, ("linker_file_lookup_symbol: fail\n"));
	return (0);
}

/*
 * Both DDB and stack(9) rely on the kernel linker to provide forward and
 * backward lookup of symbols.  However, DDB and sometimes stack(9) need to
 * do this in a lockfree manner.  We provide a set of internal helper
 * routines to perform these operations without locks, and then wrappers that
 * optionally lock.
 *
 * linker_debug_lookup() is ifdef DDB as currently it's only used by DDB.
 */
#ifdef DDB
static int
linker_debug_lookup(const char *symstr, c_linker_sym_t *sym)
{
	linker_file_t lf;

	TAILQ_FOREACH(lf, &linker_files, link) {
		if (LINKER_LOOKUP_SYMBOL(lf, symstr, sym) == 0)
			return (0);
	}
	return (ENOENT);
}
#endif

static int
linker_debug_search_symbol(caddr_t value, c_linker_sym_t *sym, long *diffp)
{
	linker_file_t lf;
	c_linker_sym_t best, es;
	u_long diff, bestdiff, off;

	best = 0;
	off = (uintptr_t)value;
	bestdiff = off;
	TAILQ_FOREACH(lf, &linker_files, link) {
		if (LINKER_SEARCH_SYMBOL(lf, value, &es, &diff) != 0)
			continue;
		if (es != 0 && diff < bestdiff) {
			best = es;
			bestdiff = diff;
		}
		if (bestdiff == 0)
			break;
	}
	if (best) {
		*sym = best;
		*diffp = bestdiff;
		return (0);
	} else {
		*sym = 0;
		*diffp = off;
		return (ENOENT);
	}
}

static int
linker_debug_symbol_values(c_linker_sym_t sym, linker_symval_t *symval)
{
	linker_file_t lf;

	TAILQ_FOREACH(lf, &linker_files, link) {
		if (LINKER_SYMBOL_VALUES(lf, sym, symval) == 0)
			return (0);
	}
	return (ENOENT);
}

static int
linker_debug_search_symbol_name(caddr_t value, char *buf, u_int buflen,
    long *offset)
{
	linker_symval_t symval;
	c_linker_sym_t sym;
	int error;

	*offset = 0;
	error = linker_debug_search_symbol(value, &sym, offset);
	if (error)
		return (error);
	error = linker_debug_symbol_values(sym, &symval);
	if (error)
		return (error);
	strlcpy(buf, symval.name, buflen);
	return (0);
}

#ifdef DDB
/*
 * DDB Helpers.  DDB has to look across multiple files with their own symbol
 * tables and string tables.
 *
 * Note that we do not obey list locking protocols here.  We really don't need
 * DDB to hang because somebody's got the lock held.  We'll take the chance
 * that the files list is inconsistant instead.
 */
int
linker_ddb_lookup(const char *symstr, c_linker_sym_t *sym)
{

	return (linker_debug_lookup(symstr, sym));
}

int
linker_ddb_search_symbol(caddr_t value, c_linker_sym_t *sym, long *diffp)
{

	return (linker_debug_search_symbol(value, sym, diffp));
}

int
linker_ddb_symbol_values(c_linker_sym_t sym, linker_symval_t *symval)
{

	return (linker_debug_symbol_values(sym, symval));
}

int
linker_ddb_search_symbol_name(caddr_t value, char *buf, u_int buflen,
    long *offset)
{

	return (linker_debug_search_symbol_name(value, buf, buflen, offset));
}
#endif

/*
 * stack(9) helper for non-debugging environemnts.  Unlike DDB helpers, we do
 * obey locking protocols, and offer a significantly less complex interface.
 */
int
linker_search_symbol_name(caddr_t value, char *buf, u_int buflen,
    long *offset)
{
	int error;

	KLD_LOCK();
	error = linker_debug_search_symbol_name(value, buf, buflen, offset);
	KLD_UNLOCK();
	return (error);
}

/*
 * Syscalls.
 */
int
kern_kldload(struct thread *td, const char *file, int *fileid)
{
#ifdef HWPMC_HOOKS
	struct pmckern_map_in pkm;
#endif
	const char *kldname, *modname;
	linker_file_t lf;
	int error;

	if ((error = securelevel_gt(td->td_ucred, 0)) != 0)
		return (error);

	if ((error = priv_check(td, PRIV_KLD_LOAD)) != 0)
		return (error);

	/*
	 * If file does not contain a qualified name or any dot in it
	 * (kldname.ko, or kldname.ver.ko) treat it as an interface
	 * name.
	 */
	if (index(file, '/') || index(file, '.')) {
		kldname = file;
		modname = NULL;
	} else {
		kldname = NULL;
		modname = file;
	}

	KLD_LOCK();
	error = linker_load_module(kldname, modname, NULL, NULL, &lf);
	if (error)
		goto unlock;
#ifdef HWPMC_HOOKS
	pkm.pm_file = lf->filename;
	pkm.pm_address = (uintptr_t) lf->address;
	PMC_CALL_HOOK(td, PMC_FN_KLD_LOAD, (void *) &pkm);
#endif
	lf->userrefs++;
	if (fileid != NULL)
		*fileid = lf->id;
unlock:
	KLD_UNLOCK();
	return (error);
}

int
kldload(struct thread *td, struct kldload_args *uap)
{
	char *pathname = NULL;
	int error, fileid;

	td->td_retval[0] = -1;

	pathname = malloc(MAXPATHLEN, M_TEMP, M_WAITOK);
	error = copyinstr(uap->file, pathname, MAXPATHLEN, NULL);
	if (error == 0) {
		error = kern_kldload(td, pathname, &fileid);
		if (error == 0)
			td->td_retval[0] = fileid;
	}
	free(pathname, M_TEMP);
	return (error);
}

int
kern_kldunload(struct thread *td, int fileid, int flags)
{
#ifdef HWPMC_HOOKS
	struct pmckern_map_out pkm;
#endif
	linker_file_t lf;
	int error = 0;

	if ((error = securelevel_gt(td->td_ucred, 0)) != 0)
		return (error);

	if ((error = priv_check(td, PRIV_KLD_UNLOAD)) != 0)
		return (error);

	KLD_LOCK();
	lf = linker_find_file_by_id(fileid);
	if (lf) {
		KLD_DPF(FILE, ("kldunload: lf->userrefs=%d\n", lf->userrefs));

		/* Check if there are DTrace probes enabled on this file. */
		if (lf->nenabled > 0) {
			printf("kldunload: attempt to unload file that has"
			    " DTrace probes enabled\n");
			error = EBUSY;
		} else if (lf->userrefs == 0) {
			/*
			 * XXX: maybe LINKER_UNLOAD_FORCE should override ?
			 */
			printf("kldunload: attempt to unload file that was"
			    " loaded by the kernel\n");
			error = EBUSY;
		} else {
#ifdef HWPMC_HOOKS
			/* Save data needed by hwpmc(4) before unloading. */
			pkm.pm_address = (uintptr_t) lf->address;
			pkm.pm_size = lf->size;
#endif
			lf->userrefs--;
			error = linker_file_unload(lf, flags);
			if (error)
				lf->userrefs++;
		}
	} else
		error = ENOENT;

#ifdef HWPMC_HOOKS
	if (error == 0)
		PMC_CALL_HOOK(td, PMC_FN_KLD_UNLOAD, (void *) &pkm);
#endif
	KLD_UNLOCK();
	return (error);
}

int
kldunload(struct thread *td, struct kldunload_args *uap)
{

	return (kern_kldunload(td, uap->fileid, LINKER_UNLOAD_NORMAL));
}

int
kldunloadf(struct thread *td, struct kldunloadf_args *uap)
{

	if (uap->flags != LINKER_UNLOAD_NORMAL &&
	    uap->flags != LINKER_UNLOAD_FORCE)
		return (EINVAL);
	return (kern_kldunload(td, uap->fileid, uap->flags));
}

int
kldfind(struct thread *td, struct kldfind_args *uap)
{
	char *pathname;
	const char *filename;
	linker_file_t lf;
	int error;

#ifdef MAC
	error = mac_check_kld_stat(td->td_ucred);
	if (error)
		return (error);
#endif

	td->td_retval[0] = -1;

	pathname = malloc(MAXPATHLEN, M_TEMP, M_WAITOK);
	if ((error = copyinstr(uap->file, pathname, MAXPATHLEN, NULL)) != 0)
		goto out;

	filename = linker_basename(pathname);
	KLD_LOCK();
	lf = linker_find_file_by_name(filename);
	if (lf)
		td->td_retval[0] = lf->id;
	else
		error = ENOENT;
	KLD_UNLOCK();
out:
	free(pathname, M_TEMP);
	return (error);
}

int
kldnext(struct thread *td, struct kldnext_args *uap)
{
	linker_file_t lf;
	int error = 0;

#ifdef MAC
	error = mac_check_kld_stat(td->td_ucred);
	if (error)
		return (error);
#endif

	KLD_LOCK();
	if (uap->fileid == 0)
		lf = TAILQ_FIRST(&linker_files);
	else {
		lf = linker_find_file_by_id(uap->fileid);
		if (lf == NULL) {
			error = ENOENT;
			goto out;
		}
		lf = TAILQ_NEXT(lf, link);
	}

	/* Skip partially loaded files. */
	while (lf != NULL && !(lf->flags & LINKER_FILE_LINKED))
		lf = TAILQ_NEXT(lf, link);

	if (lf)
		td->td_retval[0] = lf->id;
	else
		td->td_retval[0] = 0;
out:
	KLD_UNLOCK();
	return (error);
}

int
kldstat(struct thread *td, struct kldstat_args *uap)
{
	struct kld_file_stat stat;
	linker_file_t lf;
	int error, namelen, version, version_num;

	/*
	 * Check the version of the user's structure.
	 */
	if ((error = copyin(&uap->stat->version, &version, sizeof(version))) != 0)
		return (error);
	if (version == sizeof(struct kld_file_stat_1))
		version_num = 1;
	else if (version == sizeof(struct kld_file_stat))
		version_num = 2;
	else
		return (EINVAL);

#ifdef MAC
	error = mac_check_kld_stat(td->td_ucred);
	if (error)
		return (error);
#endif

	KLD_LOCK();
	lf = linker_find_file_by_id(uap->fileid);
	if (lf == NULL) {
		KLD_UNLOCK();
		return (ENOENT);
	}

	/* Version 1 fields: */
	namelen = strlen(lf->filename) + 1;
	if (namelen > MAXPATHLEN)
		namelen = MAXPATHLEN;
	bcopy(lf->filename, &stat.name[0], namelen);
	stat.refs = lf->refs;
	stat.id = lf->id;
	stat.address = lf->address;
	stat.size = lf->size;
	if (version_num > 1) {
		/* Version 2 fields: */
		namelen = strlen(lf->pathname) + 1;
		if (namelen > MAXPATHLEN)
			namelen = MAXPATHLEN;
		bcopy(lf->pathname, &stat.pathname[0], namelen);
	}
	KLD_UNLOCK();

	td->td_retval[0] = 0;

	return (copyout(&stat, uap->stat, version));
}

int
kldfirstmod(struct thread *td, struct kldfirstmod_args *uap)
{
	linker_file_t lf;
	module_t mp;
	int error = 0;

#ifdef MAC
	error = mac_check_kld_stat(td->td_ucred);
	if (error)
		return (error);
#endif

	KLD_LOCK();
	lf = linker_find_file_by_id(uap->fileid);
	if (lf) {
		MOD_SLOCK;
		mp = TAILQ_FIRST(&lf->modules);
		if (mp != NULL)
			td->td_retval[0] = module_getid(mp);
		else
			td->td_retval[0] = 0;
		MOD_SUNLOCK;
	} else
		error = ENOENT;
	KLD_UNLOCK();
	return (error);
}

int
kldsym(struct thread *td, struct kldsym_args *uap)
{
	char *symstr = NULL;
	c_linker_sym_t sym;
	linker_symval_t symval;
	linker_file_t lf;
	struct kld_sym_lookup lookup;
	int error = 0;

#ifdef MAC
	error = mac_check_kld_stat(td->td_ucred);
	if (error)
		return (error);
#endif

	if ((error = copyin(uap->data, &lookup, sizeof(lookup))) != 0)
		return (error);
	if (lookup.version != sizeof(lookup) ||
	    uap->cmd != KLDSYM_LOOKUP)
		return (EINVAL);
	symstr = malloc(MAXPATHLEN, M_TEMP, M_WAITOK);
	if ((error = copyinstr(lookup.symname, symstr, MAXPATHLEN, NULL)) != 0)
		goto out;
	KLD_LOCK();
	if (uap->fileid != 0) {
		lf = linker_find_file_by_id(uap->fileid);
		if (lf == NULL)
			error = ENOENT;
		else if (LINKER_LOOKUP_SYMBOL(lf, symstr, &sym) == 0 &&
		    LINKER_SYMBOL_VALUES(lf, sym, &symval) == 0) {
			lookup.symvalue = (uintptr_t) symval.value;
			lookup.symsize = symval.size;
			error = copyout(&lookup, uap->data, sizeof(lookup));
		} else
			error = ENOENT;
	} else {
		TAILQ_FOREACH(lf, &linker_files, link) {
			if (LINKER_LOOKUP_SYMBOL(lf, symstr, &sym) == 0 &&
			    LINKER_SYMBOL_VALUES(lf, sym, &symval) == 0) {
				lookup.symvalue = (uintptr_t)symval.value;
				lookup.symsize = symval.size;
				error = copyout(&lookup, uap->data,
				    sizeof(lookup));
				break;
			}
		}
		if (lf == NULL)
			error = ENOENT;
	}
	KLD_UNLOCK();
out:
	free(symstr, M_TEMP);
	return (error);
}

/*
 * Preloaded module support
 */

static modlist_t
modlist_lookup(const char *name, int ver)
{
	modlist_t mod;

	TAILQ_FOREACH(mod, &found_modules, link) {
		if (strcmp(mod->name, name) == 0 &&
		    (ver == 0 || mod->version == ver))
			return (mod);
	}
	return (NULL);
}

static modlist_t
modlist_lookup2(const char *name, struct mod_depend *verinfo)
{
	modlist_t mod, bestmod;
	int ver;

	if (verinfo == NULL)
		return (modlist_lookup(name, 0));
	bestmod = NULL;
	TAILQ_FOREACH(mod, &found_modules, link) {
		if (strcmp(mod->name, name) != 0)
			continue;
		ver = mod->version;
		if (ver == verinfo->md_ver_preferred)
			return (mod);
		if (ver >= verinfo->md_ver_minimum &&
		    ver <= verinfo->md_ver_maximum &&
		    (bestmod == NULL || ver > bestmod->version))
			bestmod = mod;
	}
	return (bestmod);
}

static modlist_t
modlist_newmodule(const char *modname, int version, linker_file_t container)
{
	modlist_t mod;

	mod = malloc(sizeof(struct modlist), M_LINKER, M_NOWAIT | M_ZERO);
	if (mod == NULL)
		panic("no memory for module list");
	mod->container = container;
	mod->name = modname;
	mod->version = version;
	TAILQ_INSERT_TAIL(&found_modules, mod, link);
	return (mod);
}

static void
linker_addmodules(linker_file_t lf, struct mod_metadata **start,
    struct mod_metadata **stop, int preload)
{
	struct mod_metadata *mp, **mdp;
	const char *modname;
	int ver;

	for (mdp = start; mdp < stop; mdp++) {
		mp = *mdp;
		if (mp->md_type != MDT_VERSION)
			continue;
		modname = mp->md_cval;
		ver = ((struct mod_version *)mp->md_data)->mv_version;
		if (modlist_lookup(modname, ver) != NULL) {
			printf("module %s already present!\n", modname);
			/* XXX what can we do? this is a build error. :-( */
			continue;
		}
		modlist_newmodule(modname, ver, lf);
	}
}

static void
linker_preload(void *arg)
{
	caddr_t modptr;
	const char *modname, *nmodname;
	char *modtype;
	linker_file_t lf, nlf;
	linker_class_t lc;
	int error;
	linker_file_list_t loaded_files;
	linker_file_list_t depended_files;
	struct mod_metadata *mp, *nmp;
	struct mod_metadata **start, **stop, **mdp, **nmdp;
	struct mod_depend *verinfo;
	int nver;
	int resolves;
	modlist_t mod;
	struct sysinit **si_start, **si_stop;

	TAILQ_INIT(&loaded_files);
	TAILQ_INIT(&depended_files);
	TAILQ_INIT(&found_modules);
	error = 0;

	modptr = NULL;
	while ((modptr = preload_search_next_name(modptr)) != NULL) {
		modname = (char *)preload_search_info(modptr, MODINFO_NAME);
		modtype = (char *)preload_search_info(modptr, MODINFO_TYPE);
		if (modname == NULL) {
			printf("Preloaded module at %p does not have a"
			    " name!\n", modptr);
			continue;
		}
		if (modtype == NULL) {
			printf("Preloaded module at %p does not have a type!\n",
			    modptr);
			continue;
		}
		if (bootverbose)
			printf("Preloaded %s \"%s\" at %p.\n", modtype, modname,
			    modptr);
		lf = NULL;
		TAILQ_FOREACH(lc, &classes, link) {
			error = LINKER_LINK_PRELOAD(lc, modname, &lf);
			if (!error)
				break;
			lf = NULL;
		}
		if (lf)
			TAILQ_INSERT_TAIL(&loaded_files, lf, loaded);
	}

	/*
	 * First get a list of stuff in the kernel.
	 */
	if (linker_file_lookup_set(linker_kernel_file, MDT_SETNAME, &start,
	    &stop, NULL) == 0)
		linker_addmodules(linker_kernel_file, start, stop, 1);

	/*
	 * This is a once-off kinky bubble sort to resolve relocation
	 * dependency requirements.
	 */
restart:
	TAILQ_FOREACH(lf, &loaded_files, loaded) {
		error = linker_file_lookup_set(lf, MDT_SETNAME, &start,
		    &stop, NULL);
		/*
		 * First, look to see if we would successfully link with this
		 * stuff.
		 */
		resolves = 1;	/* unless we know otherwise */
		if (!error) {
			for (mdp = start; mdp < stop; mdp++) {
				mp = *mdp;
				if (mp->md_type != MDT_DEPEND)
					continue;
				modname = mp->md_cval;
				verinfo = mp->md_data;
				for (nmdp = start; nmdp < stop; nmdp++) {
					nmp = *nmdp;
					if (nmp->md_type != MDT_VERSION)
						continue;
					nmodname = nmp->md_cval;
					if (strcmp(modname, nmodname) == 0)
						break;
				}
				if (nmdp < stop)   /* it's a self reference */
					continue;

				/*
				 * ok, the module isn't here yet, we
				 * are not finished
				 */
				if (modlist_lookup2(modname, verinfo) == NULL)
					resolves = 0;
			}
		}
		/*
		 * OK, if we found our modules, we can link.  So, "provide"
		 * the modules inside and add it to the end of the link order
		 * list.
		 */
		if (resolves) {
			if (!error) {
				for (mdp = start; mdp < stop; mdp++) {
					mp = *mdp;
					if (mp->md_type != MDT_VERSION)
						continue;
					modname = mp->md_cval;
					nver = ((struct mod_version *)
					    mp->md_data)->mv_version;
					if (modlist_lookup(modname,
					    nver) != NULL) {
						printf("module %s already"
						    " present!\n", modname);
						TAILQ_REMOVE(&loaded_files,
						    lf, loaded);
						linker_file_unload(lf,
						    LINKER_UNLOAD_FORCE);
						/* we changed tailq next ptr */
						goto restart;
					}
					modlist_newmodule(modname, nver, lf);
				}
			}
			TAILQ_REMOVE(&loaded_files, lf, loaded);
			TAILQ_INSERT_TAIL(&depended_files, lf, loaded);
			/*
			 * Since we provided modules, we need to restart the
			 * sort so that the previous files that depend on us
			 * have a chance. Also, we've busted the tailq next
			 * pointer with the REMOVE.
			 */
			goto restart;
		}
	}

	/*
	 * At this point, we check to see what could not be resolved..
	 */
	while ((lf = TAILQ_FIRST(&loaded_files)) != NULL) {
		TAILQ_REMOVE(&loaded_files, lf, loaded);
		printf("KLD file %s is missing dependencies\n", lf->filename);
		linker_file_unload(lf, LINKER_UNLOAD_FORCE);
	}

	/*
	 * We made it. Finish off the linking in the order we determined.
	 */
	TAILQ_FOREACH_SAFE(lf, &depended_files, loaded, nlf) {
		if (linker_kernel_file) {
			linker_kernel_file->refs++;
			error = linker_file_add_dependency(lf,
			    linker_kernel_file);
			if (error)
				panic("cannot add dependency");
		}
		lf->userrefs++;	/* so we can (try to) kldunload it */
		error = linker_file_lookup_set(lf, MDT_SETNAME, &start,
		    &stop, NULL);
		if (!error) {
			for (mdp = start; mdp < stop; mdp++) {
				mp = *mdp;
				if (mp->md_type != MDT_DEPEND)
					continue;
				modname = mp->md_cval;
				verinfo = mp->md_data;
				mod = modlist_lookup2(modname, verinfo);
				/* Don't count self-dependencies */
				if (lf == mod->container)
					continue;
				mod->container->refs++;
				error = linker_file_add_dependency(lf,
				    mod->container);
				if (error)
					panic("cannot add dependency");
			}
		}
		/*
		 * Now do relocation etc using the symbol search paths
		 * established by the dependencies
		 */
		error = LINKER_LINK_PRELOAD_FINISH(lf);
		if (error) {
			TAILQ_REMOVE(&depended_files, lf, loaded);
			printf("KLD file %s - could not finalize loading\n",
			    lf->filename);
			linker_file_unload(lf, LINKER_UNLOAD_FORCE);
			continue;
		}
		linker_file_register_modules(lf);
		if (linker_file_lookup_set(lf, "sysinit_set", &si_start,
		    &si_stop, NULL) == 0)
			sysinit_add(si_start, si_stop);
		linker_file_register_sysctls(lf);
		lf->flags |= LINKER_FILE_LINKED;
	}
	/* woohoo! we made it! */
}

SYSINIT(preload, SI_SUB_KLD, SI_ORDER_MIDDLE, linker_preload, 0);

/*
 * Search for a not-loaded module by name.
 *
 * Modules may be found in the following locations:
 *
 * - preloaded (result is just the module name) - on disk (result is full path
 * to module)
 *
 * If the module name is qualified in any way (contains path, etc.) the we
 * simply return a copy of it.
 *
 * The search path can be manipulated via sysctl.  Note that we use the ';'
 * character as a separator to be consistent with the bootloader.
 */

static char linker_hintfile[] = "linker.hints";
static char linker_path[MAXPATHLEN] = "/boot/kernel;/boot/modules";

SYSCTL_STRING(_kern, OID_AUTO, module_path, CTLFLAG_RW, linker_path,
    sizeof(linker_path), "module load search path");

TUNABLE_STR("module_path", linker_path, sizeof(linker_path));

static char *linker_ext_list[] = {
	"",
	".ko",
	NULL
};

/*
 * Check if file actually exists either with or without extension listed in
 * the linker_ext_list. (probably should be generic for the rest of the
 * kernel)
 */
static char *
linker_lookup_file(const char *path, int pathlen, const char *name,
    int namelen, struct vattr *vap)
{
	struct nameidata nd;
	struct thread *td = curthread;	/* XXX */
	char *result, **cpp, *sep;
	int error, len, extlen, reclen, flags, vfslocked;
	enum vtype type;

	extlen = 0;
	for (cpp = linker_ext_list; *cpp; cpp++) {
		len = strlen(*cpp);
		if (len > extlen)
			extlen = len;
	}
	extlen++;		/* trailing '\0' */
	sep = (path[pathlen - 1] != '/') ? "/" : "";

	reclen = pathlen + strlen(sep) + namelen + extlen + 1;
	result = malloc(reclen, M_LINKER, M_WAITOK);
	for (cpp = linker_ext_list; *cpp; cpp++) {
		snprintf(result, reclen, "%.*s%s%.*s%s", pathlen, path, sep,
		    namelen, name, *cpp);
		/*
		 * Attempt to open the file, and return the path if
		 * we succeed and it's a regular file.
		 */
		NDINIT(&nd, LOOKUP, FOLLOW | MPSAFE, UIO_SYSSPACE, result, td);
		flags = FREAD;
		error = vn_open(&nd, &flags, 0, NULL);
		if (error == 0) {
			vfslocked = NDHASGIANT(&nd);
			NDFREE(&nd, NDF_ONLY_PNBUF);
			type = nd.ni_vp->v_type;
			if (vap)
				VOP_GETATTR(nd.ni_vp, vap, td->td_ucred, td);
			VOP_UNLOCK(nd.ni_vp, 0, td);
			vn_close(nd.ni_vp, FREAD, td->td_ucred, td);
			VFS_UNLOCK_GIANT(vfslocked);
			if (type == VREG)
				return (result);
		}
	}
	free(result, M_LINKER);
	return (NULL);
}

#define	INT_ALIGN(base, ptr)	ptr =					\
	(base) + (((ptr) - (base) + sizeof(int) - 1) & ~(sizeof(int) - 1))

/*
 * Lookup KLD which contains requested module in the "linker.hints" file. If
 * version specification is available, then try to find the best KLD.
 * Otherwise just find the latest one.
 */
static char *
linker_hints_lookup(const char *path, int pathlen, const char *modname,
    int modnamelen, struct mod_depend *verinfo)
{
	struct thread *td = curthread;	/* XXX */
	struct ucred *cred = td ? td->td_ucred : NULL;
	struct nameidata nd;
	struct vattr vattr, mattr;
	u_char *hints = NULL;
	u_char *cp, *recptr, *bufend, *result, *best, *pathbuf, *sep;
	int error, ival, bestver, *intp, reclen, found, flags, clen, blen;
	int vfslocked = 0;

	result = NULL;
	bestver = found = 0;

	sep = (path[pathlen - 1] != '/') ? "/" : "";
	reclen = imax(modnamelen, strlen(linker_hintfile)) + pathlen +
	    strlen(sep) + 1;
	pathbuf = malloc(reclen, M_LINKER, M_WAITOK);
	snprintf(pathbuf, reclen, "%.*s%s%s", pathlen, path, sep,
	    linker_hintfile);

	NDINIT(&nd, LOOKUP, NOFOLLOW | MPSAFE, UIO_SYSSPACE, pathbuf, td);
	flags = FREAD;
	error = vn_open(&nd, &flags, 0, NULL);
	if (error)
		goto bad;
	vfslocked = NDHASGIANT(&nd);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	if (nd.ni_vp->v_type != VREG)
		goto bad;
	best = cp = NULL;
	error = VOP_GETATTR(nd.ni_vp, &vattr, cred, td);
	if (error)
		goto bad;
	/*
	 * XXX: we need to limit this number to some reasonable value
	 */
	if (vattr.va_size > 100 * 1024) {
		printf("hints file too large %ld\n", (long)vattr.va_size);
		goto bad;
	}
	hints = malloc(vattr.va_size, M_TEMP, M_WAITOK);
	if (hints == NULL)
		goto bad;
	error = vn_rdwr(UIO_READ, nd.ni_vp, (caddr_t)hints, vattr.va_size, 0,
	    UIO_SYSSPACE, IO_NODELOCKED, cred, NOCRED, &reclen, td);
	if (error)
		goto bad;
	VOP_UNLOCK(nd.ni_vp, 0, td);
	vn_close(nd.ni_vp, FREAD, cred, td);
	VFS_UNLOCK_GIANT(vfslocked);
	nd.ni_vp = NULL;
	if (reclen != 0) {
		printf("can't read %d\n", reclen);
		goto bad;
	}
	intp = (int *)hints;
	ival = *intp++;
	if (ival != LINKER_HINTS_VERSION) {
		printf("hints file version mismatch %d\n", ival);
		goto bad;
	}
	bufend = hints + vattr.va_size;
	recptr = (u_char *)intp;
	clen = blen = 0;
	while (recptr < bufend && !found) {
		intp = (int *)recptr;
		reclen = *intp++;
		ival = *intp++;
		cp = (char *)intp;
		switch (ival) {
		case MDT_VERSION:
			clen = *cp++;
			if (clen != modnamelen || bcmp(cp, modname, clen) != 0)
				break;
			cp += clen;
			INT_ALIGN(hints, cp);
			ival = *(int *)cp;
			cp += sizeof(int);
			clen = *cp++;
			if (verinfo == NULL ||
			    ival == verinfo->md_ver_preferred) {
				found = 1;
				break;
			}
			if (ival >= verinfo->md_ver_minimum &&
			    ival <= verinfo->md_ver_maximum &&
			    ival > bestver) {
				bestver = ival;
				best = cp;
				blen = clen;
			}
			break;
		default:
			break;
		}
		recptr += reclen + sizeof(int);
	}
	/*
	 * Finally check if KLD is in the place
	 */
	if (found)
		result = linker_lookup_file(path, pathlen, cp, clen, &mattr);
	else if (best)
		result = linker_lookup_file(path, pathlen, best, blen, &mattr);

	/*
	 * KLD is newer than hints file. What we should do now?
	 */
	if (result && timespeccmp(&mattr.va_mtime, &vattr.va_mtime, >))
		printf("warning: KLD '%s' is newer than the linker.hints"
		    " file\n", result);
bad:
	free(pathbuf, M_LINKER);
	if (hints)
		free(hints, M_TEMP);
	if (nd.ni_vp != NULL) {
		VOP_UNLOCK(nd.ni_vp, 0, td);
		vn_close(nd.ni_vp, FREAD, cred, td);
		VFS_UNLOCK_GIANT(vfslocked);
	}
	/*
	 * If nothing found or hints is absent - fallback to the old
	 * way by using "kldname[.ko]" as module name.
	 */
	if (!found && !bestver && result == NULL)
		result = linker_lookup_file(path, pathlen, modname,
		    modnamelen, NULL);
	return (result);
}

/*
 * Lookup KLD which contains requested module in the all directories.
 */
static char *
linker_search_module(const char *modname, int modnamelen,
    struct mod_depend *verinfo)
{
	char *cp, *ep, *result;

	/*
	 * traverse the linker path
	 */
	for (cp = linker_path; *cp; cp = ep + 1) {
		/* find the end of this component */
		for (ep = cp; (*ep != 0) && (*ep != ';'); ep++);
		result = linker_hints_lookup(cp, ep - cp, modname,
		    modnamelen, verinfo);
		if (result != NULL)
			return (result);
		if (*ep == 0)
			break;
	}
	return (NULL);
}

/*
 * Search for module in all directories listed in the linker_path.
 */
static char *
linker_search_kld(const char *name)
{
	char *cp, *ep, *result;
	int len;

	/* qualified at all? */
	if (index(name, '/'))
		return (linker_strdup(name));

	/* traverse the linker path */
	len = strlen(name);
	for (ep = linker_path; *ep; ep++) {
		cp = ep;
		/* find the end of this component */
		for (; *ep != 0 && *ep != ';'; ep++);
		result = linker_lookup_file(cp, ep - cp, name, len, NULL);
		if (result != NULL)
			return (result);
	}
	return (NULL);
}

static const char *
linker_basename(const char *path)
{
	const char *filename;

	filename = rindex(path, '/');
	if (filename == NULL)
		return path;
	if (filename[1])
		filename++;
	return (filename);
}

#ifdef HWPMC_HOOKS

struct hwpmc_context {
	int	nobjects;
	int	nmappings;
	struct pmckern_map_in *kobase;
};

static int
linker_hwpmc_list_object(linker_file_t lf, void *arg)
{
	struct hwpmc_context *hc;

	hc = arg;

	/* If we run out of mappings, fail. */
	if (hc->nobjects >= hc->nmappings)
		return (1);

	/* Save the info for this linker file. */
	hc->kobase[hc->nobjects].pm_file = lf->filename;
	hc->kobase[hc->nobjects].pm_address = (uintptr_t)lf->address;
	hc->nobjects++;
	return (0);
}

/*
 * Inform hwpmc about the set of kernel modules currently loaded.
 */
void *
linker_hwpmc_list_objects(void)
{
	struct hwpmc_context hc;

	hc.nmappings = 15;	/* a reasonable default */

 retry:
	/* allocate nmappings+1 entries */
	MALLOC(hc.kobase, struct pmckern_map_in *,
	    (hc.nmappings + 1) * sizeof(struct pmckern_map_in), M_LINKER,
	    M_WAITOK | M_ZERO);

	hc.nobjects = 0;
	if (linker_file_foreach(linker_hwpmc_list_object, &hc) != 0) {
		hc.nmappings = hc.nobjects;
		FREE(hc.kobase, M_LINKER);
		goto retry;
	}

	KASSERT(hc.nobjects > 0, ("linker_hpwmc_list_objects: no kernel "
		"objects?"));

	/* The last entry of the malloced area comprises of all zeros. */
	KASSERT(hc.kobase[hc.nobjects].pm_file == NULL,
	    ("linker_hwpmc_list_objects: last object not NULL"));

	return ((void *)hc.kobase);
}
#endif

/*
 * Find a file which contains given module and load it, if "parent" is not
 * NULL, register a reference to it.
 */
static int
linker_load_module(const char *kldname, const char *modname,
    struct linker_file *parent, struct mod_depend *verinfo,
    struct linker_file **lfpp)
{
	linker_file_t lfdep;
	const char *filename;
	char *pathname;
	int error;

	KLD_LOCK_ASSERT();
	if (modname == NULL) {
		/*
 		 * We have to load KLD
 		 */
		KASSERT(verinfo == NULL, ("linker_load_module: verinfo"
		    " is not NULL"));
		pathname = linker_search_kld(kldname);
	} else {
		if (modlist_lookup2(modname, verinfo) != NULL)
			return (EEXIST);
		if (kldname != NULL)
			pathname = linker_strdup(kldname);
		else if (rootvnode == NULL)
			pathname = NULL;
		else
			/*
			 * Need to find a KLD with required module
			 */
			pathname = linker_search_module(modname,
			    strlen(modname), verinfo);
	}
	if (pathname == NULL)
		return (ENOENT);

	/*
	 * Can't load more than one file with the same basename XXX:
	 * Actually it should be possible to have multiple KLDs with
	 * the same basename but different path because they can
	 * provide different versions of the same modules.
	 */
	filename = linker_basename(pathname);
	if (linker_find_file_by_name(filename))
		error = EEXIST;
	else do {
		error = linker_load_file(pathname, &lfdep);
		if (error)
			break;
		if (modname && verinfo &&
		    modlist_lookup2(modname, verinfo) == NULL) {
			linker_file_unload(lfdep, LINKER_UNLOAD_FORCE);
			error = ENOENT;
			break;
		}
		if (parent) {
			error = linker_file_add_dependency(parent, lfdep);
			if (error)
				break;
		}
		if (lfpp)
			*lfpp = lfdep;
	} while (0);
	free(pathname, M_LINKER);
	return (error);
}

/*
 * This routine is responsible for finding dependencies of userland initiated
 * kldload(2)'s of files.
 */
int
linker_load_dependencies(linker_file_t lf)
{
	linker_file_t lfdep;
	struct mod_metadata **start, **stop, **mdp, **nmdp;
	struct mod_metadata *mp, *nmp;
	struct mod_depend *verinfo;
	modlist_t mod;
	const char *modname, *nmodname;
	int ver, error = 0, count;

	/*
	 * All files are dependant on /kernel.
	 */
	KLD_LOCK_ASSERT();
	if (linker_kernel_file) {
		linker_kernel_file->refs++;
		error = linker_file_add_dependency(lf, linker_kernel_file);
		if (error)
			return (error);
	}
	if (linker_file_lookup_set(lf, MDT_SETNAME, &start, &stop,
	    &count) != 0)
		return (0);
	for (mdp = start; mdp < stop; mdp++) {
		mp = *mdp;
		if (mp->md_type != MDT_VERSION)
			continue;
		modname = mp->md_cval;
		ver = ((struct mod_version *)mp->md_data)->mv_version;
		mod = modlist_lookup(modname, ver);
		if (mod != NULL) {
			printf("interface %s.%d already present in the KLD"
			    " '%s'!\n", modname, ver,
			    mod->container->filename);
			return (EEXIST);
		}
	}

	for (mdp = start; mdp < stop; mdp++) {
		mp = *mdp;
		if (mp->md_type != MDT_DEPEND)
			continue;
		modname = mp->md_cval;
		verinfo = mp->md_data;
		nmodname = NULL;
		for (nmdp = start; nmdp < stop; nmdp++) {
			nmp = *nmdp;
			if (nmp->md_type != MDT_VERSION)
				continue;
			nmodname = nmp->md_cval;
			if (strcmp(modname, nmodname) == 0)
				break;
		}
		if (nmdp < stop)/* early exit, it's a self reference */
			continue;
		mod = modlist_lookup2(modname, verinfo);
		if (mod) {	/* woohoo, it's loaded already */
			lfdep = mod->container;
			lfdep->refs++;
			error = linker_file_add_dependency(lf, lfdep);
			if (error)
				break;
			continue;
		}
		error = linker_load_module(NULL, modname, lf, verinfo, NULL);
		if (error) {
			printf("KLD %s: depends on %s - not available\n",
			    lf->filename, modname);
			break;
		}
	}

	if (error)
		return (error);
	linker_addmodules(lf, start, stop, 0);
	return (error);
}

static int
sysctl_kern_function_list_iterate(const char *name, void *opaque)
{
	struct sysctl_req *req;

	req = opaque;
	return (SYSCTL_OUT(req, name, strlen(name) + 1));
}

/*
 * Export a nul-separated, double-nul-terminated list of all function names
 * in the kernel.
 */
static int
sysctl_kern_function_list(SYSCTL_HANDLER_ARGS)
{
	linker_file_t lf;
	int error;

#ifdef MAC
	error = mac_check_kld_stat(req->td->td_ucred);
	if (error)
		return (error);
#endif
	error = sysctl_wire_old_buffer(req, 0);
	if (error != 0)
		return (error);
	KLD_LOCK();
	TAILQ_FOREACH(lf, &linker_files, link) {
		error = LINKER_EACH_FUNCTION_NAME(lf,
		    sysctl_kern_function_list_iterate, req);
		if (error) {
			KLD_UNLOCK();
			return (error);
		}
	}
	KLD_UNLOCK();
	return (SYSCTL_OUT(req, "", 1));
}

SYSCTL_PROC(_kern, OID_AUTO, function_list, CTLFLAG_RD,
    NULL, 0, sysctl_kern_function_list, "", "kernel function list");
