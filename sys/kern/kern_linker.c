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
 *	$Id: kern_linker.c,v 1.2 1997/08/02 14:31:28 bde Exp $
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/sysproto.h>
#include <sys/sysent.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <machine/cpu.h>
#include <sys/module.h>
#include <sys/linker.h>

static struct lock lock;	/* lock for the file list */
static linker_class_list_t classes;
static linker_file_list_t files;
static int next_file_id = 1;

static void
linker_init(void* arg)
{
    lockinit(&lock, PVM, "klink", 0, 0);
    TAILQ_INIT(&classes);
    TAILQ_INIT(&files);
}

SYSINIT(linker, SI_SUB_KMEM, SI_ORDER_SECOND, linker_init, 0);

int
linker_add_class(const char* desc, void* priv,
		 struct linker_class_ops* ops)
{
    linker_class_t lc;

    lc = malloc(sizeof(struct linker_class), M_LINKER, M_NOWAIT);
    if (!lc)
	return ENOMEM;

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

    linker_current_file = lf;

    KLD_DPF(FILE, ("linker_file_sysinit: calling SYSINITs for %s\n",
		   lf->filename));

    sysinits = (struct linker_set*)
	linker_file_lookup_symbol(lf, "sysinit_set", 0);
    if (!sysinits)
	return;

    /*
     * Perform a bubble sort of the system initialization objects by
     * their subsystem (primary key) and order (secondary key).
     *
     * Since some things care about execution order, this is the
     * operation which ensures continued function.
     */
    for( sipp = (struct sysinit **)sysinits->ls_items; *sipp; sipp++) {
	for( xipp = sipp + 1; *xipp; xipp++) {
	    if( (*sipp)->subsystem < (*xipp)->subsystem ||
		( (*sipp)->subsystem == (*xipp)->subsystem &&
		  (*sipp)->order < (*xipp)->order))
		continue;	/* skip*/
	    save = *sipp;
	    *sipp = *xipp;
	    *xipp = save;
	}
    }


    /*
     * Traverse the (now) ordered list of system initialization tasks.
     * Perform each task, and continue on to the next task.
     *
     * The last item on the list is expected to be the scheduler,
     * which will not return.
     */
    for( sipp = (struct sysinit **)sysinits->ls_items; *sipp; sipp++) {
	if( (*sipp)->subsystem == SI_SUB_DUMMY)
	    continue;	/* skip dummy task(s)*/

	switch( (*sipp)->type) {
	case SI_TYPE_DEFAULT:
	    /* no special processing*/
	    (*((*sipp)->func))( (*sipp)->udata);
	    break;

	case SI_TYPE_KTHREAD:
	    /* kernel thread*/
	    if (fork(&proc0, NULL))
		panic("fork kernel process");
	    cpu_set_fork_handler(pfind(proc0.p_retval[0]),
		(*sipp)->func, (*sipp)->udata);
	    break;

	default:
	    panic( "linker_file_sysinit: unrecognized init type");
	}
    }
}

int
linker_load_file(const char* filename, linker_file_t* result)
{
    linker_class_t lc;
    linker_file_t lf;
    int error = 0;

    lf = linker_find_file_by_name(filename);
    if (lf) {
	KLD_DPF(FILE, ("linker_load_file: file %s is already loaded, incrementing refs\n", filename));
	*result = lf;
	lf->refs++;
	goto out;
    }

    lf = NULL;
    for (lc = TAILQ_FIRST(&classes); lc; lc = TAILQ_NEXT(lc, link)) {
	KLD_DPF(FILE, ("linker_load_file: trying to load %s as %s\n",
		       filename, lc->desc));
	if (error = lc->ops->load_file(filename, &lf))
	    goto out;
	if (lf) {
	    linker_file_sysinit(lf);

	    *result = lf;
	    goto out;
	}
    }

    error = ENOEXEC;		/* format not recognised */

out:
    return error;
}

linker_file_t
linker_find_file_by_name(const char* filename)
{
    linker_file_t lf = 0;

    lockmgr(&lock, LK_SHARED, 0, curproc);
    for (lf = TAILQ_FIRST(&files); lf; lf = TAILQ_NEXT(lf, link))
	if (!strcmp(lf->filename, filename))
	    break;
    lockmgr(&lock, LK_RELEASE, 0, curproc);

    return lf;
}

linker_file_t
linker_find_file_by_id(int fileid)
{
    linker_file_t lf = 0;

    lockmgr(&lock, LK_SHARED, 0, curproc);
    for (lf = TAILQ_FIRST(&files); lf; lf = TAILQ_NEXT(lf, link))
	if (lf->id == fileid)
	    break;
    lockmgr(&lock, LK_RELEASE, 0, curproc);

    return lf;
}

linker_file_t
linker_make_file(const char* filename, void* priv, struct linker_file_ops* ops)
{
    linker_file_t lf = 0;
    int namelen;

    KLD_DPF(FILE, ("linker_make_file: new file, filename=%s\n", filename));
    lockmgr(&lock, LK_EXCLUSIVE|LK_RETRY, 0, curproc);
    namelen = strlen(filename) + 1;
    lf = malloc(sizeof(struct linker_file) + namelen, M_LINKER, M_WAITOK);
    if (!lf)
	goto out;

    lf->refs = 1;
    lf->userrefs = 0;
    lf->filename = (char*) (lf + 1);
    strcpy(lf->filename, filename);
    lf->id = next_file_id++;
    lf->ndeps = 0;
    lf->deps = NULL;
    STAILQ_INIT(&lf->common);
    TAILQ_INIT(&lf->modules);

    lf->priv = priv;
    lf->ops = ops;
    TAILQ_INSERT_TAIL(&files, lf, link);

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

    KLD_DPF(FILE, ("linker_file_unload: lf->refs=%d\n", lf->refs));
    lockmgr(&lock, LK_EXCLUSIVE|LK_RETRY, 0, curproc);
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
	    if (error = module_unload(mod)) {
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

    TAILQ_REMOVE(&files, file, link);
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
    caddr_t address;
    size_t size;
    size_t common_size = 0;
    int i;

    KLD_DPF(SYM, ("linker_file_lookup_symbol: file=%x, name=%s, deps=%d",
		  file, name, deps));

    if (file->ops->lookup_symbol(file, name, &address, &size) == 0)
	if (address == 0)
	    /*
	     * For commons, first look them up in the dependancies and
	     * only allocate space if not found there.
	     */
	    common_size = size;
	else
	    return address;

    if (deps)
	for (i = 0; i < file->ndeps; i++) {
	    address = linker_file_lookup_symbol(file->deps[i], name, 0);
	    if (address)
		return address;
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
	    if (!strcmp(cp->name, name))
		return cp->address;

	/*
	 * Round the symbol size up to align.
	 */
	common_size = (common_size + sizeof(int) - 1) & -sizeof(int);
	cp = malloc(sizeof(struct common_symbol)
		    + common_size
		    + strlen(name) + 1,
		    M_LINKER, M_WAITOK);
	if (!cp)
	    return 0;

	cp->address = (caddr_t) (cp + 1);
	cp->name = cp->address + common_size;
	strcpy(cp->name, name);
	bzero(cp->address, common_size);
	STAILQ_INSERT_TAIL(&file->common, cp, link);

	return cp->address;
    }

    return 0;
}

/*
 * Syscalls.
 */

int
kldload(struct proc* p, struct kldload_args* uap)
{
    char* filename = NULL;
    linker_file_t lf;
    int error = 0;

    p->p_retval[0] = -1;

    if (securelevel > 0)
	return EPERM;

    if (error = suser(p->p_ucred, &p->p_acflag))
	return error;

    filename = malloc(MAXPATHLEN, M_TEMP, M_WAITOK);
    if (error = copyinstr(SCARG(uap, file), filename, MAXPATHLEN, NULL))
	goto out;

    if (error = linker_load_file(uap->file, &lf))
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

    if (securelevel > 0)
	return EPERM;

    if (error = suser(p->p_ucred, &p->p_acflag))
	return error;

    lf = linker_find_file_by_id(SCARG(uap, fileid));
    if (lf) {
	KLD_DPF(FILE, ("kldunload: lf->userrefs=%d\n", lf->userrefs));
	if (lf->userrefs == 0) {
	    printf("linkerunload: attempt to unload file which was not loaded by user\n");
	    error = EBUSY;
	    goto out;
	}
	lf->userrefs--;
	error = linker_file_unload(lf);
    } else
	error = ENOENT;

out:
    return error;
}

int
kldfind(struct proc* p, struct kldfind_args* uap)
{
    char* filename = NULL;
    linker_file_t lf;
    int error = 0;

    p->p_retval[0] = -1;

    filename = malloc(MAXPATHLEN, M_TEMP, M_WAITOK);
    if (error = copyinstr(SCARG(uap, file), filename, MAXPATHLEN, NULL))
	goto out;

    lf = linker_find_file_by_name(filename);
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
	if (TAILQ_FIRST(&files))
	    p->p_retval[0] = TAILQ_FIRST(&files)->id;
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
    if (error = copyin(&stat->version, &version, sizeof(version)))
	goto out;
    if (version != sizeof(struct kld_file_stat)) {
	error = EINVAL;
	goto out;
    }

    namelen = strlen(lf->filename) + 1;
    if (namelen > MAXPATHLEN)
	namelen = MAXPATHLEN;
    if (error = copyout(lf->filename, &stat->name[0], namelen))
	goto out;
    if (error = copyout(&lf->refs, &stat->refs, sizeof(int)))
	goto out;
    if (error = copyout(&lf->id, &stat->id, sizeof(int)))
	goto out;
    if (error = copyout(&lf->address, &stat->address, sizeof(caddr_t)))
	goto out;
    if (error = copyout(&lf->size, &stat->size, sizeof(size_t)))
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
