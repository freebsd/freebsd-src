/*-
 * Copyright (c) 2005, Sam Leffler <sam@errno.com>
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/taskqueue.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/errno.h>
#include <sys/linker.h>
#include <sys/firmware.h>
#include <sys/proc.h>
#include <sys/module.h>

#define	FIRMWARE_MAX	30
static char *name_unload = "UNLOADING";
static struct firmware firmware_table[FIRMWARE_MAX];
struct task firmware_task;
struct mtx firmware_mtx;
MTX_SYSINIT(firmware, &firmware_mtx, "firmware table", MTX_DEF);

/*
 * Register a firmware image with the specified name.  The
 * image name must not already be registered.  If this is a
 * subimage then parent refers to a previously registered
 * image that this should be associated with.
 */
struct firmware *
firmware_register(const char *imagename, const void *data, size_t datasize,
    unsigned int version, struct firmware *parent)
{
	struct firmware *frp = NULL;
	int i;

	mtx_lock(&firmware_mtx);
	for (i = 0; i < FIRMWARE_MAX; i++) {
		struct firmware *fp = &firmware_table[i];

		if (fp->name == NULL) {
			if (frp == NULL)
				frp = fp;
			continue;
		}
		if (strcasecmp(imagename, fp->name) == 0) {
			mtx_unlock(&firmware_mtx);
			printf("%s: image %s already registered!\n",
				__func__, imagename);
			return NULL;
		}
	}
	if (frp == NULL) {
		mtx_unlock(&firmware_mtx);
		printf("%s: cannot register image %s, firmware table full!\n",
		    __func__, imagename);
		return NULL;
	}
	frp->name = imagename;
	frp->data = data;
	frp->datasize = datasize;
	frp->version = version;
	frp->refcnt = 0;
	if (parent != NULL)
		parent->refcnt++;
	frp->parent = parent;
	frp->file = NULL;
	mtx_unlock(&firmware_mtx);
	return frp;
}

static void
clearentry(struct firmware *fp, int keep_file)
{
	KASSERT(fp->refcnt == 0, ("image %s refcnt %u", fp->name, fp->refcnt));
	if (keep_file && (fp->file != NULL))
		fp->name = name_unload;
	else {
		fp->name = NULL;
		fp->file = NULL;
	}
	fp->data = NULL;
	fp->datasize = 0;
	fp->version = 0;
	if (fp->parent != NULL) {	/* release parent reference */
		fp->parent->refcnt--;
		fp->parent = NULL;
	}
}

static struct firmware *
lookup(const char *name)
{
	int i;

	for (i = 0; i < FIRMWARE_MAX; i++) {
		struct firmware * fp = &firmware_table[i];
		if (fp->name != NULL && strcasecmp(name, fp->name) == 0)
			return fp;
	}
	return NULL;
}

/*
 * Unregister/remove a firmware image.  If there are outstanding
 * references an error is returned and the image is not removed
 * from the registry.
 */
int
firmware_unregister(const char *imagename)
{
	struct firmware *fp;
	int refcnt = 0;

	mtx_lock(&firmware_mtx);
	/*
	 * NB: it is ok for the lookup to fail; this can happen
	 * when a module is unloaded on last reference and the
	 * module unload handler unregister's each of it's
	 * firmware images.
	 */
	fp = lookup(imagename);
	if (fp != NULL) {
		refcnt = fp->refcnt;
		if (refcnt == 0)
			clearentry(fp, 0);
	}
	mtx_unlock(&firmware_mtx);
	return (refcnt != 0 ? EBUSY : 0);
}

/*
 * Lookup and potentially load the specified firmware image.
 * If the firmware is not found in the registry attempt to
 * load a kernel module with the image name.  If the firmware
 * is located a reference is returned.  The caller must release
 * this reference for the image to be eligible for removal/unload.
 */
struct firmware *
firmware_get(const char *imagename)
{
	struct thread *td;
	struct firmware *fp;
	linker_file_t result;
	int requested_load = 0;

again:
	mtx_lock(&firmware_mtx);
	fp = lookup(imagename);
	if (fp != NULL) {
		if (requested_load)
			fp->file = result;
		fp->refcnt++;
		mtx_unlock(&firmware_mtx);
		return fp;
	}
	/*
	 * Image not present, try to load the module holding it
	 * or if we already tried give up.
	 */
	mtx_unlock(&firmware_mtx);
	if (requested_load) {
		printf("%s: failed to load firmware image %s\n",
		    __func__, imagename);
		return NULL;
	}
	td = curthread;
	if (suser(td) != 0 || securelevel_gt(td->td_ucred, 0) != 0) {
		printf("%s: insufficient privileges to "
		    "load firmware image %s\n", __func__, imagename);
		return NULL;
	}
	mtx_lock(&Giant);		/* XXX */
	(void) linker_reference_module(imagename, NULL, &result);
	mtx_unlock(&Giant);		/* XXX */
	requested_load = 1;
	goto again;		/* sort of an Algol-style for loop */
}

static void
unloadentry(void *unused1, int unused2)
{
	struct firmware *fp;

	mtx_lock(&firmware_mtx);
	while ((fp = lookup(name_unload))) {
		/*
		 * XXX: ugly, we should be able to lookup unlocked here if
		 * we properly lock around clearentry below to avoid double
		 * unload.  Play it safe for now.
		 */
		mtx_unlock(&firmware_mtx);

		linker_file_unload(fp->file, LINKER_UNLOAD_NORMAL);

		mtx_lock(&firmware_mtx);
		clearentry(fp, 0);
	}
	mtx_unlock(&firmware_mtx);
}

/*
 * Release a reference to a firmware image returned by
 * firmware_get.  The reference is released and if this is
 * the last reference to the firmware image the associated
 * module may be released/unloaded.
 */
void
firmware_put(struct firmware *fp, int flags)
{
	mtx_lock(&firmware_mtx);
	fp->refcnt--;
	if (fp->refcnt == 0 && (flags & FIRMWARE_UNLOAD))
		clearentry(fp, 1);
	if (fp->file)
		taskqueue_enqueue(taskqueue_thread, &firmware_task);
	mtx_unlock(&firmware_mtx);
}

/*
 * Module glue.
 */
static int
firmware_modevent(module_t mod, int type, void *unused)
{
	switch (type) {
	case MOD_LOAD:
		TASK_INIT(&firmware_task, 0, unloadentry, NULL);
		return 0;
	case MOD_UNLOAD:
		taskqueue_drain(taskqueue_thread, &firmware_task);
		return 0;
	}
	return EINVAL;
}

static moduledata_t firmware_mod = {
	"firmware",
	firmware_modevent,
	0
};
DECLARE_MODULE(firmware, firmware_mod, SI_SUB_DRIVERS, SI_ORDER_FIRST);
MODULE_VERSION(firmware, 1);
