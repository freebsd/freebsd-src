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

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/sysproto.h>
#include <sys/sysent.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/linker.h>
#include <sys/fcntl.h>
#include <sys/libkern.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/sysctl.h>

#include <vm/vm_zone.h>

#ifdef KLD_DEBUG
int kld_debug = 0;
#endif

MALLOC_DEFINE(M_LINKER, "kld", "kernel linker");
linker_file_t linker_current_file;
linker_file_t linker_kernel_file;

static struct lock lock;	/* lock for the file list */
static linker_class_list_t classes;
static linker_file_list_t linker_files;
static int next_file_id = 1;

static void
linker_init(void* arg)
{
    lockinit(&lock, PVM, "klink", 0, 0);
    TAILQ_INIT(&classes);
    TAILQ_INIT(&linker_files);
}

SYSINIT(linker, SI_SUB_KLD, SI_ORDER_FIRST, linker_init, 0);

int
linker_add_class(const char* desc, void* priv,
		 struct linker_class_ops* ops)
{
    linker_class_t lc;

    lc = malloc(sizeof(struct linker_class), M_LINKER, M_NOWAIT);
    if (!lc)
	return ENOMEM;
    bzero(lc, sizeof(*lc));

    lc->desc = desc;
    lc->priv = priv;
    lc->ops = ops;
    TAILQ_INSERT_HEAD(&classes, lc, link);

    return 0;
}

static void
linker_file_sysinit(linker_file_t lf)
{
    struct linker_set* sysinits;
    struct sysinit** sipp;
    struct sysinit** xipp;
    struct sysinit* save;
    const moduledata_t *moddata;
    int error;

    KLD_DPF(FILE, ("linker_file_sysinit: calling SYSINITs for %s\n",
		   lf->filename));

    sysinits = (struct linker_set*)
	linker_file_lookup_symbol(lf, "sysinit_set", 0);

    KLD_DPF(FILE, ("linker_file_sysinit: SYSINITs %p\n", sysinits));
    if (!sysinits)
	return;

    /* HACK ALERT! */
    for (sipp = (struct sysinit **)sysinits->ls_items; *sipp; sipp++) {
	if ((*sipp)->func == module_register_init) {
	    moddata = (*sipp)->udata;
	    error = module_register(moddata, lf);
	    if (error)
		printf("linker_file_sysinit \"%s\" failed to register! %d\n",
		    lf->filename, error);
	}
    }
	    
    /*
     * Perform a bubble sort of the system initialization objects by
     * their subsystem (primary key) and order (secondary key).
     *
     * Since some things care about execution order, this is the
     * operation which ensures continued function.
     */
    for (sipp = (struct sysinit **)sysinits->ls_items; *sipp; sipp++) {
	for (xipp = sipp + 1; *xipp; xipp++) {
	    if ((*sipp)->subsystem < (*xipp)->subsystem ||
		 ((*sipp)->subsystem == (*xipp)->subsystem &&
		  (*sipp)->order <= (*xipp)->order))
		continue;	/* skip*/
	    save = *sipp;
	    *sipp = *xipp;
	    *xipp = save;
	}
    }


    /*
     * Traverse the (now) ordered list of system initialization tasks.
     * Perform each task, and continue on to the next task.
     */
    for (sipp = (struct sysinit **)sysinits->ls_items; *sipp; sipp++) {
	if ((*sipp)->subsystem == SI_SUB_DUMMY)
	    continue;	/* skip dummy task(s)*/

	/* Call function */
	(*((*sipp)->func))((*sipp)->udata);
    }
}

static void
linker_file_sysuninit(linker_file_t lf)
{
    struct linker_set* sysuninits;
    struct sysinit** sipp;
    struct sysinit** xipp;
    struct sysinit* save;

    KLD_DPF(FILE, ("linker_file_sysuninit: calling SYSUNINITs for %s\n",
		   lf->filename));

    sysuninits = (struct linker_set*)
	linker_file_lookup_symbol(lf, "sysuninit_set", 0);

    KLD_DPF(FILE, ("linker_file_sysuninit: SYSUNINITs %p\n", sysuninits));
    if (!sysuninits)
	return;

    /*
     * Perform a reverse bubble sort of the system initialization objects
     * by their subsystem (primary key) and order (secondary key).
     *
     * Since some things care about execution order, this is the
     * operation which ensures continued function.
     */
    for (sipp = (struct sysinit **)sysuninits->ls_items; *sipp; sipp++) {
	for (xipp = sipp + 1; *xipp; xipp++) {
	    if ((*sipp)->subsystem > (*xipp)->subsystem ||
		 ((*sipp)->subsystem == (*xipp)->subsystem &&
		  (*sipp)->order >= (*xipp)->order))
		continue;	/* skip*/
	    save = *sipp;
	    *sipp = *xipp;
	    *xipp = save;
	}
    }


    /*
     * Traverse the (now) ordered list of system initialization tasks.
     * Perform each task, and continue on to the next task.
     */
    for (sipp = (struct sysinit **)sysuninits->ls_items; *sipp; sipp++) {
	if ((*sipp)->subsystem == SI_SUB_DUMMY)
	    continue;	/* skip dummy task(s)*/

	/* Call function */
	(*((*sipp)->func))((*sipp)->udata);
    }
}

static void
linker_file_register_sysctls(linker_file_t lf)
{
    struct linker_set* sysctls;

    KLD_DPF(FILE, ("linker_file_register_sysctls: registering SYSCTLs for %s\n",
		   lf->filename));

    sysctls = (struct linker_set*)
	linker_file_lookup_symbol(lf, "sysctl_set", 0);

    KLD_DPF(FILE, ("linker_file_register_sysctls: SYSCTLs %p\n", sysctls));
    if (!sysctls)
	return;

    sysctl_register_set(sysctls);
}

static void
linker_file_unregister_sysctls(linker_file_t lf)
{
    struct linker_set* sysctls;

    KLD_DPF(FILE, ("linker_file_unregister_sysctls: registering SYSCTLs for %s\n",
		   lf->filename));

    sysctls = (struct linker_set*)
	linker_file_lookup_symbol(lf, "sysctl_set", 0);

    KLD_DPF(FILE, ("linker_file_unregister_sysctls: SYSCTLs %p\n", sysctls));
    if (!sysctls)
	return;

    sysctl_unregister_set(sysctls);
}

int
linker_load_file(const char* filename, linker_file_t* result)
{
    linker_class_t lc;
    linker_file_t lf;
    int foundfile, error = 0;
    char *koname = NULL;

    /* Refuse to load modules if securelevel raised */
    if (securelevel > 0)
	return EPERM; 

    lf = linker_find_file_by_name(filename);
    if (lf) {
	KLD_DPF(FILE, ("linker_load_file: file %s is already loaded, incrementing refs\n", filename));
	*result = lf;
	lf->refs++;
	goto out;
    }

    koname = malloc(strlen(filename) + 4, M_LINKER, M_WAITOK);
    if (koname == NULL) {
	error = ENOMEM;
	goto out;
    }
    sprintf(koname, "%s.ko", filename);
    lf = NULL;
    foundfile = 0;
    for (lc = TAILQ_FIRST(&classes); lc; lc = TAILQ_NEXT(lc, link)) {
	KLD_DPF(FILE, ("linker_load_file: trying to load %s as %s\n",
		       filename, lc->desc));

	error = lc->ops->load_file(koname, &lf);	/* First with .ko */
	if (lf == NULL && error == ENOENT)
	    error = lc->ops->load_file(filename, &lf);	/* Then try without */
	/*
	 * If we got something other than ENOENT, then it exists but we cannot
	 * load it for some other reason.
	 */
	if (error != ENOENT)
	    foundfile = 1;
	if (lf) {
	    linker_file_register_sysctls(lf);
	    linker_file_sysinit(lf);

	    *result = lf;
	    error = 0;
	    goto out;
	}
    }
    /*
     * Less than ideal, but tells the user whether it failed to load or
     * the module was not found.
     */
    if (foundfile)
	error = ENOEXEC;	/* Format not recognised (or unloadable) */
    else
	error = ENOENT;		/* Nothing found */

out:
    if (koname)
	free(koname, M_LINKER);
    return error;
}

linker_file_t
linker_find_file_by_name(const char* filename)
{
    linker_file_t lf = 0;
    char *koname;

    koname = malloc(strlen(filename) + 4, M_LINKER, M_WAITOK);
    if (koname == NULL)
	goto out;
    sprintf(koname, "%s.ko", filename);

    lockmgr(&lock, LK_SHARED, 0, curproc);
    for (lf = TAILQ_FIRST(&linker_files); lf; lf = TAILQ_NEXT(lf, link)) {
	if (!strcmp(lf->filename, koname))
	    break;
	if (!strcmp(lf->filename, filename))
	    break;
    }
    lockmgr(&lock, LK_RELEASE, 0, curproc);

out:
    if (koname)
	free(koname, M_LINKER);
    return lf;
}

linker_file_t
linker_find_file_by_id(int fileid)
{
    linker_file_t lf = 0;

    lockmgr(&lock, LK_SHARED, 0, curproc);
    for (lf = TAILQ_FIRST(&linker_files); lf; lf = TAILQ_NEXT(lf, link))
	if (lf->id == fileid)
	    break;
    lockmgr(&lock, LK_RELEASE, 0, curproc);

    return lf;
}

linker_file_t
linker_make_file(const char* pathname, void* priv, struct linker_file_ops* ops)
{
    linker_file_t lf = 0;
    int namelen;
    const char *filename;

    filename = rindex(pathname, '/');
    if (filename && filename[1])
	filename++;
    else
	filename = pathname;

    KLD_DPF(FILE, ("linker_make_file: new file, filename=%s\n", filename));
    lockmgr(&lock, LK_EXCLUSIVE, 0, curproc);
    namelen = strlen(filename) + 1;
    lf = malloc(sizeof(struct linker_file) + namelen, M_LINKER, M_WAITOK);
    if (!lf)
	goto out;
    bzero(lf, sizeof(*lf));

    lf->refs = 1;
    lf->userrefs = 0;
    lf->flags = 0;
    lf->filename = (char*) (lf + 1);
    strcpy(lf->filename, filename);
    lf->id = next_file_id++;
    lf->ndeps = 0;
    lf->deps = NULL;
    STAILQ_INIT(&lf->common);
    TAILQ_INIT(&lf->modules);

    lf->priv = priv;
    lf->ops = ops;
    TAILQ_INSERT_TAIL(&linker_files, lf, link);

out:
    lockmgr(&lock, LK_RELEASE, 0, curproc);
    return lf;
}

int
linker_file_unload(linker_file_t file)
{
    module_t mod, next;
    struct common_symbol* cp;
    int error = 0;
    int i;

    /* Refuse to unload modules if securelevel raised */
    if (securelevel > 0)
	return EPERM; 

    KLD_DPF(FILE, ("linker_file_unload: lf->refs=%d\n", file->refs));
    lockmgr(&lock, LK_EXCLUSIVE, 0, curproc);
    if (file->refs == 1) {
	KLD_DPF(FILE, ("linker_file_unload: file is unloading, informing modules\n"));
	/*
	 * Inform any modules associated with this file.
	 */
	for (mod = TAILQ_FIRST(&file->modules); mod; mod = next) {
	    next = module_getfnext(mod);

	    /*
	     * Give the module a chance to veto the unload.
	     */
	    if ((error = module_unload(mod)) != 0) {
		KLD_DPF(FILE, ("linker_file_unload: module %x vetoes unload\n",
			       mod));
		lockmgr(&lock, LK_RELEASE, 0, curproc);
		goto out;
	    }

	    module_release(mod);
	}
    }

    file->refs--;
    if (file->refs > 0) {
	lockmgr(&lock, LK_RELEASE, 0, curproc);
	goto out;
    }

    /* Don't try to run SYSUNINITs if we are unloaded due to a link error */
    if (file->flags & LINKER_FILE_LINKED) {
	linker_file_sysuninit(file);
	linker_file_unregister_sysctls(file);
    }

    TAILQ_REMOVE(&linker_files, file, link);
    lockmgr(&lock, LK_RELEASE, 0, curproc);

    for (i = 0; i < file->ndeps; i++)
	linker_file_unload(file->deps[i]);
    free(file->deps, M_LINKER);

    for (cp = STAILQ_FIRST(&file->common); cp;
	 cp = STAILQ_FIRST(&file->common)) {
	STAILQ_REMOVE(&file->common, cp, common_symbol, link);
	free(cp, M_LINKER);
    }

    file->ops->unload(file);
    free(file, M_LINKER);

out:
    return error;
}

int
linker_file_add_dependancy(linker_file_t file, linker_file_t dep)
{
    linker_file_t* newdeps;

    newdeps = malloc((file->ndeps + 1) * sizeof(linker_file_t*),
		     M_LINKER, M_WAITOK);
    if (newdeps == NULL)
	return ENOMEM;
    bzero(newdeps, (file->ndeps + 1) * sizeof(linker_file_t*));

    if (file->deps) {
	bcopy(file->deps, newdeps, file->ndeps * sizeof(linker_file_t*));
	free(file->deps, M_LINKER);
    }
    file->deps = newdeps;
    file->deps[file->ndeps] = dep;
    file->ndeps++;

    return 0;
}

caddr_t
linker_file_lookup_symbol(linker_file_t file, const char* name, int deps)
{
    c_linker_sym_t sym;
    linker_symval_t symval;
    linker_file_t lf;
    caddr_t address;
    size_t common_size = 0;
    int i;

    KLD_DPF(SYM, ("linker_file_lookup_symbol: file=%x, name=%s, deps=%d\n",
		  file, name, deps));

    if (file->ops->lookup_symbol(file, name, &sym) == 0) {
	file->ops->symbol_values(file, sym, &symval);
	if (symval.value == 0)
	    /*
	     * For commons, first look them up in the dependancies and
	     * only allocate space if not found there.
	     */
	    common_size = symval.size;
	else {
	    KLD_DPF(SYM, ("linker_file_lookup_symbol: symbol.value=%x\n", symval.value));
	    return symval.value;
	}
    }

    if (deps) {
	for (i = 0; i < file->ndeps; i++) {
	    address = linker_file_lookup_symbol(file->deps[i], name, 0);
	    if (address) {
		KLD_DPF(SYM, ("linker_file_lookup_symbol: deps value=%x\n", address));
		return address;
	    }
	}

	/* If we have not found it in the dependencies, search globally */
	for (lf = TAILQ_FIRST(&linker_files); lf; lf = TAILQ_NEXT(lf, link)) {
	    /* But skip the current file if it's on the list */
	    if (lf == file)
		continue;
	    /* And skip the files we searched above */
	    for (i = 0; i < file->ndeps; i++)
		if (lf == file->deps[i])
		    break;
	    if (i < file->ndeps)
		continue;
	    address = linker_file_lookup_symbol(lf, name, 0);
	    if (address) {
		KLD_DPF(SYM, ("linker_file_lookup_symbol: global value=%x\n", address));
		return address;
	    }
	}
    }

    if (common_size > 0) {
	/*
	 * This is a common symbol which was not found in the
	 * dependancies.  We maintain a simple common symbol table in
	 * the file object.
	 */
	struct common_symbol* cp;

	for (cp = STAILQ_FIRST(&file->common); cp;
	     cp = STAILQ_NEXT(cp, link))
	    if (!strcmp(cp->name, name)) {
		KLD_DPF(SYM, ("linker_file_lookup_symbol: old common value=%x\n", cp->address));
		return cp->address;
	    }

	/*
	 * Round the symbol size up to align.
	 */
	common_size = (common_size + sizeof(int) - 1) & -sizeof(int);
	cp = malloc(sizeof(struct common_symbol)
		    + common_size
		    + strlen(name) + 1,
		    M_LINKER, M_WAITOK);
	if (!cp) {
	    KLD_DPF(SYM, ("linker_file_lookup_symbol: nomem\n"));
	    return 0;
	}
	bzero(cp, sizeof(struct common_symbol) + common_size + strlen(name)+ 1);

	cp->address = (caddr_t) (cp + 1);
	cp->name = cp->address + common_size;
	strcpy(cp->name, name);
	bzero(cp->address, common_size);
	STAILQ_INSERT_TAIL(&file->common, cp, link);

	KLD_DPF(SYM, ("linker_file_lookup_symbol: new common value=%x\n", cp->address));
	return cp->address;
    }

    KLD_DPF(SYM, ("linker_file_lookup_symbol: fail\n"));
    return 0;
}

#ifdef DDB
/*
 * DDB Helpers.  DDB has to look across multiple files with their own
 * symbol tables and string tables.
 *
 * Note that we do not obey list locking protocols here.  We really don't
 * need DDB to hang because somebody's got the lock held.  We'll take the
 * chance that the files list is inconsistant instead.
 */

int
linker_ddb_lookup(const char *symstr, c_linker_sym_t *sym)
{
    linker_file_t lf;

    for (lf = TAILQ_FIRST(&linker_files); lf; lf = TAILQ_NEXT(lf, link)) {
	if (lf->ops->lookup_symbol(lf, symstr, sym) == 0)
	    return 0;
    }
    return ENOENT;
}

int
linker_ddb_search_symbol(caddr_t value, c_linker_sym_t *sym, long *diffp)
{
    linker_file_t lf;
    u_long off = (uintptr_t)value;
    u_long diff, bestdiff;
    c_linker_sym_t best;
    c_linker_sym_t es;

    best = 0;
    bestdiff = off;
    for (lf = TAILQ_FIRST(&linker_files); lf; lf = TAILQ_NEXT(lf, link)) {
	if (lf->ops->search_symbol(lf, value, &es, &diff) != 0)
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
	return 0;
    } else {
	*sym = 0;
	*diffp = off;
	return ENOENT;
    }
}

int
linker_ddb_symbol_values(c_linker_sym_t sym, linker_symval_t *symval)
{
    linker_file_t lf;

    for (lf = TAILQ_FIRST(&linker_files); lf; lf = TAILQ_NEXT(lf, link)) {
	if (lf->ops->symbol_values(lf, sym, symval) == 0)
	    return 0;
    }
    return ENOENT;
}

#endif

/*
 * Syscalls.
 */

int
kldload(struct proc* p, struct kldload_args* uap)
{
    char* filename = NULL, *modulename;
    linker_file_t lf;
    int error = 0;

    p->p_retval[0] = -1;

    if (securelevel > 0)	/* redundant, but that's OK */
	return EPERM;

    if ((error = suser(p)) != 0)
	return error;

    filename = malloc(MAXPATHLEN, M_TEMP, M_WAITOK);
    if ((error = copyinstr(SCARG(uap, file), filename, MAXPATHLEN, NULL)) != 0)
	goto out;

    /* Can't load more than one module with the same name */
    modulename = rindex(filename, '/');
    if (modulename == NULL)
	modulename = filename;
    else
	modulename++;
    if (linker_find_file_by_name(modulename)) {
	error = EEXIST;
	goto out;
    }

    if ((error = linker_load_file(filename, &lf)) != 0)
	goto out;

    lf->userrefs++;
    p->p_retval[0] = lf->id;

out:
    if (filename)
	free(filename, M_TEMP);
    return error;
}

int
kldunload(struct proc* p, struct kldunload_args* uap)
{
    linker_file_t lf;
    int error = 0;

    if (securelevel > 0)	/* redundant, but that's OK */
	return EPERM;

    if ((error = suser(p)) != 0)
	return error;

    lf = linker_find_file_by_id(SCARG(uap, fileid));
    if (lf) {
	KLD_DPF(FILE, ("kldunload: lf->userrefs=%d\n", lf->userrefs));
	if (lf->userrefs == 0) {
	    printf("linkerunload: attempt to unload file that was loaded by the kernel\n");
	    error = EBUSY;
	    goto out;
	}
	lf->userrefs--;
	error = linker_file_unload(lf);
	if (error)
	    lf->userrefs++;
    } else
	error = ENOENT;

out:
    return error;
}

int
kldfind(struct proc* p, struct kldfind_args* uap)
{
    char* filename = NULL, *modulename;
    linker_file_t lf;
    int error = 0;

    p->p_retval[0] = -1;

    filename = malloc(MAXPATHLEN, M_TEMP, M_WAITOK);
    if ((error = copyinstr(SCARG(uap, file), filename, MAXPATHLEN, NULL)) != 0)
	goto out;

    modulename = rindex(filename, '/');
    if (modulename == NULL)
	modulename = filename;

    lf = linker_find_file_by_name(modulename);
    if (lf)
	p->p_retval[0] = lf->id;
    else
	error = ENOENT;

out:
    if (filename)
	free(filename, M_TEMP);
    return error;
}

int
kldnext(struct proc* p, struct kldnext_args* uap)
{
    linker_file_t lf;
    int error = 0;

    if (SCARG(uap, fileid) == 0) {
	if (TAILQ_FIRST(&linker_files))
	    p->p_retval[0] = TAILQ_FIRST(&linker_files)->id;
	else
	    p->p_retval[0] = 0;
	return 0;
    }

    lf = linker_find_file_by_id(SCARG(uap, fileid));
    if (lf) {
	if (TAILQ_NEXT(lf, link))
	    p->p_retval[0] = TAILQ_NEXT(lf, link)->id;
	else
	    p->p_retval[0] = 0;
    } else
	error = ENOENT;

    return error;
}

int
kldstat(struct proc* p, struct kldstat_args* uap)
{
    linker_file_t lf;
    int error = 0;
    int version;
    struct kld_file_stat* stat;
    int namelen;

    lf = linker_find_file_by_id(SCARG(uap, fileid));
    if (!lf) {
	error = ENOENT;
	goto out;
    }

    stat = SCARG(uap, stat);

    /*
     * Check the version of the user's structure.
     */
    if ((error = copyin(&stat->version, &version, sizeof(version))) != 0)
	goto out;
    if (version != sizeof(struct kld_file_stat)) {
	error = EINVAL;
	goto out;
    }

    namelen = strlen(lf->filename) + 1;
    if (namelen > MAXPATHLEN)
	namelen = MAXPATHLEN;
    if ((error = copyout(lf->filename, &stat->name[0], namelen)) != 0)
	goto out;
    if ((error = copyout(&lf->refs, &stat->refs, sizeof(int))) != 0)
	goto out;
    if ((error = copyout(&lf->id, &stat->id, sizeof(int))) != 0)
	goto out;
    if ((error = copyout(&lf->address, &stat->address, sizeof(caddr_t))) != 0)
	goto out;
    if ((error = copyout(&lf->size, &stat->size, sizeof(size_t))) != 0)
	goto out;

    p->p_retval[0] = 0;

out:
    return error;
}

int
kldfirstmod(struct proc* p, struct kldfirstmod_args* uap)
{
    linker_file_t lf;
    int error = 0;

    lf = linker_find_file_by_id(SCARG(uap, fileid));
    if (lf) {
	if (TAILQ_FIRST(&lf->modules))
	    p->p_retval[0] = module_getid(TAILQ_FIRST(&lf->modules));
	else
	    p->p_retval[0] = 0;
    } else
	error = ENOENT;

    return error;
}

int
kldsym(struct proc *p, struct kldsym_args *uap)
{
    char *symstr = NULL;
    c_linker_sym_t sym;
    linker_symval_t symval;
    linker_file_t lf;
    struct kld_sym_lookup lookup;
    int error = 0;

    if ((error = copyin(SCARG(uap, data), &lookup, sizeof(lookup))) != 0)
	goto out;
    if (lookup.version != sizeof(lookup) || SCARG(uap, cmd) != KLDSYM_LOOKUP) {
	error = EINVAL;
	goto out;
    }

    symstr = malloc(MAXPATHLEN, M_TEMP, M_WAITOK);
    if ((error = copyinstr(lookup.symname, symstr, MAXPATHLEN, NULL)) != 0)
	goto out;

    if (SCARG(uap, fileid) != 0) {
	lf = linker_find_file_by_id(SCARG(uap, fileid));
	if (lf == NULL) {
	    error = ENOENT;
	    goto out;
	}
	if (lf->ops->lookup_symbol(lf, symstr, &sym) == 0 &&
	    lf->ops->symbol_values(lf, sym, &symval) == 0) {
	    lookup.symvalue = (uintptr_t)symval.value;
	    lookup.symsize = symval.size;
	    error = copyout(&lookup, SCARG(uap, data), sizeof(lookup));
	} else
	    error = ENOENT;
    } else {
	for (lf = TAILQ_FIRST(&linker_files); lf; lf = TAILQ_NEXT(lf, link)) {
	    if (lf->ops->lookup_symbol(lf, symstr, &sym) == 0 &&
		lf->ops->symbol_values(lf, sym, &symval) == 0) {
		lookup.symvalue = (uintptr_t)symval.value;
		lookup.symsize = symval.size;
		error = copyout(&lookup, SCARG(uap, data), sizeof(lookup));
		break;
	    }
	}
	if (!lf)
	    error = ENOENT;
    }
out:
    if (symstr)
	free(symstr, M_TEMP);
    return error;
}

/*
 * Preloaded module support
 */

static void
linker_preload(void* arg)
{
    caddr_t		modptr;
    char		*modname;
    char		*modtype;
    linker_file_t	lf;
    linker_class_t	lc;
    int			error;
    struct linker_set	*sysinits;
    struct sysinit	**sipp;
    const moduledata_t	*moddata;

    modptr = NULL;
    while ((modptr = preload_search_next_name(modptr)) != NULL) {
	modname = (char *)preload_search_info(modptr, MODINFO_NAME);
	modtype = (char *)preload_search_info(modptr, MODINFO_TYPE);
	if (modname == NULL) {
	    printf("Preloaded module at %p does not have a name!\n", modptr);
	    continue;
	}
	if (modtype == NULL) {
	    printf("Preloaded module at %p does not have a type!\n", modptr);
	    continue;
	}
	printf("Preloaded %s \"%s\" at %p.\n", modtype, modname, modptr);
	lf = linker_find_file_by_name(modname);
	if (lf) {
	    lf->userrefs++;
	    continue;
	}
	lf = NULL;
	for (lc = TAILQ_FIRST(&classes); lc; lc = TAILQ_NEXT(lc, link)) {
	    error = lc->ops->load_file(modname, &lf);
	    if (error) {
		lf = NULL;
		break;
	    }
	}
	if (lf) {
	    lf->userrefs++;

	    sysinits = (struct linker_set*)
		linker_file_lookup_symbol(lf, "sysinit_set", 0);
	    if (sysinits) {
		/* HACK ALERT!
		 * This is to set the sysinit moduledata so that the module
		 * can attach itself to the correct containing file.
		 * The sysinit could be run at *any* time.
		 */
		for (sipp = (struct sysinit **)sysinits->ls_items; *sipp; sipp++) {
		    if ((*sipp)->func == module_register_init) {
			moddata = (*sipp)->udata;
			error = module_register(moddata, lf);
			if (error)
			    printf("Preloaded %s \"%s\" failed to register: %d\n",
				modtype, modname, error);
		    }
		}
		sysinit_add((struct sysinit **)sysinits->ls_items);
	    }
	    linker_file_register_sysctls(lf);
	}
    }
}

SYSINIT(preload, SI_SUB_KLD, SI_ORDER_MIDDLE, linker_preload, 0);

/*
 * Search for a not-loaded module by name.
 *
 * Modules may be found in the following locations:
 *
 * - preloaded (result is just the module name)
 * - on disk (result is full path to module)
 *
 * If the module name is qualified in any way (contains path, etc.)
 * the we simply return a copy of it.
 *
 * The search path can be manipulated via sysctl.  Note that we use the ';'
 * character as a separator to be consistent with the bootloader.
 */

static char linker_path[MAXPATHLEN] = "/;/boot/;/modules/";

SYSCTL_STRING(_kern, OID_AUTO, module_path, CTLFLAG_RW, linker_path,
	      sizeof(linker_path), "module load search path");

static char *
linker_strdup(const char *str)
{
    char	*result;

    if ((result = malloc((strlen(str) + 1), M_LINKER, M_WAITOK)) != NULL)
	strcpy(result, str);
    return(result);
}

char *
linker_search_path(const char *name)
{
    struct nameidata	nd;
    struct proc		*p = curproc;	/* XXX */
    char		*cp, *ep, *result;
    int			error;
    enum vtype		type;

    /* qualified at all? */
    if (index(name, '/'))
	return(linker_strdup(name));

    /* traverse the linker path */
    cp = linker_path;
    for (;;) {

	/* find the end of this component */
	for (ep = cp; (*ep != 0) && (*ep != ';'); ep++)
	    ;
	result = malloc((strlen(name) + (ep - cp) + 1), M_LINKER, M_WAITOK);
	if (result == NULL)	/* actually ENOMEM */
	    return(NULL);

	strncpy(result, cp, ep - cp);
	strcpy(result + (ep - cp), name);

	/*
	 * Attempt to open the file, and return the path if we succeed and it's
	 * a regular file.
	 */
	NDINIT(&nd, LOOKUP, FOLLOW, UIO_SYSSPACE, result, p);
	error = vn_open(&nd, FREAD, 0);
	if (error == 0) {
	    NDFREE(&nd, NDF_ONLY_PNBUF);
	    type = nd.ni_vp->v_type;
	    VOP_UNLOCK(nd.ni_vp, 0, p);
	    vn_close(nd.ni_vp, FREAD, p->p_ucred, p);
	    if (type == VREG)
		return(result);
	}
	free(result, M_LINKER);

	if (*ep == 0)
	    break;
	cp = ep + 1;
    }
    return(NULL);
}
