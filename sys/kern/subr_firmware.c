/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2005-2008, Sam Leffler <sam@errno.com>
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

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/eventhandler.h>
#include <sys/fcntl.h>
#include <sys/firmware.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/sbuf.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/taskqueue.h>

#include <sys/filedesc.h>
#include <sys/vnode.h>

/*
 * Loadable firmware support. See sys/sys/firmware.h and firmware(9)
 * form more details on the subsystem.
 *
 * 'struct firmware' is the user-visible part of the firmware table.
 * Additional internal information is stored in a 'struct priv_fw',
 * which embeds the public firmware structure.
 */

/*
 * fw.name != NULL when an image is registered; file != NULL for
 * autoloaded images whose handling has not been completed.
 *
 * The state of a slot evolves as follows:
 *	firmware_register	-->  fw.name = image_name
 *	(autoloaded image)	-->  file = module reference
 *	firmware_unregister	-->  fw.name = NULL
 *	(unloadentry complete)	-->  file = NULL
 *
 * In order for the above to work, the 'file' field must remain
 * unchanged in firmware_unregister().
 *
 * Images residing in the same module are linked to each other
 * through the 'parent' argument of firmware_register().
 * One image (typically, one with the same name as the module to let
 * the autoloading mechanism work) is considered the parent image for
 * all other images in the same module. Children affect the refcount
 * on the parent image preventing improper unloading of the image itself.
 */

struct priv_fw {
	int		refcnt;		/* reference count */
	LIST_ENTRY(priv_fw) link;	/* table linkage */

	/*
	 * parent entry, see above. Set on firmware_register(),
	 * cleared on firmware_unregister().
	 */
	struct priv_fw	*parent;

	int 		flags;
#define FW_BINARY	0x080	/* Firmware directly loaded, file == NULL */
#define FW_UNLOAD	0x100	/* record FIRMWARE_UNLOAD requests */

	/*
	 * 'file' is private info managed by the autoload/unload code.
	 * Set at the end of firmware_get(), cleared only in the
	 * firmware_unload_task, so the latter can depend on its value even
	 * while the lock is not held.
	 */
	linker_file_t   file;	/* module file, if autoloaded */

	/*
	 * 'fw' is the externally visible image information.
	 * We do not make it the first field in priv_fw, to avoid the
	 * temptation of casting pointers to each other.
	 * Use PRIV_FW(fw) to get a pointer to the cointainer of fw.
	 * Beware, PRIV_FW does not work for a NULL pointer.
	 */
	struct firmware	fw;	/* externally visible information */
};

/*
 * PRIV_FW returns the pointer to the container of struct firmware *x.
 * Cast to intptr_t to override the 'const' attribute of x
 */
#define PRIV_FW(x)	((struct priv_fw *)		\
	((intptr_t)(x) - offsetof(struct priv_fw, fw)) )

/*
 * Global firmware image registry.
 */
static LIST_HEAD(, priv_fw) firmware_table;

/*
 * Firmware module operations are handled in a separate task as they
 * might sleep and they require directory context to do i/o. We also
 * use this when loading binaries directly.
 */
static struct taskqueue *firmware_tq;
static struct task firmware_unload_task;

/*
 * This mutex protects accesses to the firmware table.
 */
static struct mtx firmware_mtx;
MTX_SYSINIT(firmware, &firmware_mtx, "firmware table", MTX_DEF);

static MALLOC_DEFINE(M_FIRMWARE, "firmware", "device firmware images");

static uint64_t firmware_max_size = 8u << 20; /* Default to 8MB cap */
SYSCTL_U64(_debug, OID_AUTO, firmware_max_size,
    CTLFLAG_RWTUN, &firmware_max_size, 0,
    "Max size permitted for a firmware file.");

/*
 * Helper function to lookup a name.
 * As a side effect, it sets the pointer to a free slot, if any.
 * This way we can concentrate most of the registry scanning in
 * this function, which makes it easier to replace the registry
 * with some other data structure.
 */
static struct priv_fw *
lookup(const char *name)
{
	struct priv_fw *fp;

	mtx_assert(&firmware_mtx, MA_OWNED);

	LIST_FOREACH(fp, &firmware_table, link) {
		if (fp->fw.name != NULL && strcasecmp(name, fp->fw.name) == 0)
			break;
	}
	return (fp);
}

/*
 * Register a firmware image with the specified name.  The
 * image name must not already be registered.  If this is a
 * subimage then parent refers to a previously registered
 * image that this should be associated with.
 */
const struct firmware *
firmware_register(const char *imagename, const void *data, size_t datasize,
    unsigned int version, const struct firmware *parent)
{
	struct priv_fw *frp;
	char *name;

	mtx_lock(&firmware_mtx);
	frp = lookup(imagename);
	if (frp != NULL) {
		mtx_unlock(&firmware_mtx);
		printf("%s: image %s already registered!\n",
		    __func__, imagename);
		return (NULL);
	}
	mtx_unlock(&firmware_mtx);

	frp = malloc(sizeof(*frp), M_FIRMWARE, M_WAITOK | M_ZERO);
	name = strdup(imagename, M_FIRMWARE);

	mtx_lock(&firmware_mtx);
	if (lookup(imagename) != NULL) {
		/* We lost a race. */
		mtx_unlock(&firmware_mtx);
		free(name, M_FIRMWARE);
		free(frp, M_FIRMWARE);
		return (NULL);
	}
	frp->fw.name = name;
	frp->fw.data = data;
	frp->fw.datasize = datasize;
	frp->fw.version = version;
	if (parent != NULL)
		frp->parent = PRIV_FW(parent);
	LIST_INSERT_HEAD(&firmware_table, frp, link);
	mtx_unlock(&firmware_mtx);
	if (bootverbose)
		printf("firmware: '%s' version %u: %zu bytes loaded at %p\n",
		    imagename, version, datasize, data);
	return (&frp->fw);
}

/*
 * Unregister/remove a firmware image.  If there are outstanding
 * references an error is returned and the image is not removed
 * from the registry.
 */
int
firmware_unregister(const char *imagename)
{
	struct priv_fw *fp;
	int err;

	mtx_lock(&firmware_mtx);
	fp = lookup(imagename);
	if (fp == NULL) {
		/*
		 * It is ok for the lookup to fail; this can happen
		 * when a module is unloaded on last reference and the
		 * module unload handler unregister's each of its
		 * firmware images.
		 */
		err = 0;
	} else if (fp->refcnt != 0) {	/* cannot unregister */
		err = EBUSY;
	} else {
		LIST_REMOVE(fp, link);
		free(__DECONST(char *, fp->fw.name), M_FIRMWARE);
		free(fp, M_FIRMWARE);
		err = 0;
	}
	mtx_unlock(&firmware_mtx);
	return (err);
}

struct fw_loadimage {
	const char	*imagename;
	uint32_t	flags;
};

static const char *fw_path = "/boot/firmware/";

static void
try_binary_file(const char *imagename, uint32_t flags)
{
	struct nameidata nd;
	struct thread *td = curthread;
	struct ucred *cred = td ? td->td_ucred : NULL;
	struct sbuf *sb;
	struct priv_fw *fp;
	const char *fn;
	struct vattr vattr;
	void *data = NULL;
	const struct firmware *fw;
	int flags;
	size_t resid;
	int error;
	bool warn = flags & FIRMWARE_GET_NOWARN;

	/*
	 * XXX TODO: Loop over some path instead of a single element path.
	 * and fetch this path from the 'firmware_path' kenv the loader sets.
	 */
	sb = sbuf_new_auto();
	sbuf_printf(sb, "%s%s", fw_path, imagename);
	sbuf_finish(sb);
	fn = sbuf_data(sb);
	if (bootverbose)
		printf("Trying to load binary firmware from %s\n", fn);

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_SYSSPACE, fn);
	flags = FREAD;
	error = vn_open(&nd, &flags, 0, NULL);
	if (error)
		goto err;
	NDFREE_PNBUF(&nd);
	if (nd.ni_vp->v_type != VREG)
		goto err2;
	error = VOP_GETATTR(nd.ni_vp, &vattr, cred);
	if (error)
		goto err2;

	/*
	 * Limit this to something sane, 8MB by default.
	 */
	if (vattr.va_size > firmware_max_size) {
		printf("Firmware %s is too big: %ld bytes, %ld bytes max.\n",
		    fn, vattr.va_size, firmware_max_size);
		goto err2;
	}
	data = malloc(vattr.va_size, M_FIRMWARE, M_WAITOK);
	error = vn_rdwr(UIO_READ, nd.ni_vp, (caddr_t)data, vattr.va_size, 0,
	    UIO_SYSSPACE, IO_NODELOCKED, cred, NOCRED, &resid, td);
	/* XXX make data read only? */
	VOP_UNLOCK(nd.ni_vp);
	vn_close(nd.ni_vp, FREAD, cred, td);
	nd.ni_vp = NULL;
	if (error != 0 || resid != 0)
		goto err;
	fw = firmware_register(fn, data, vattr.va_size, 0, NULL);
	if (fw == NULL)
		goto err;
	fp = PRIV_FW(fw);
	fp->flags |= FW_BINARY;
	if (bootverbose)
		printf("%s: Loaded binary firmware using %s\n", imagename, fn);
	sbuf_delete(sb);
	return;

err2: /* cleanup in vn_open through vn_close */
	VOP_UNLOCK(nd.ni_vp);
	vn_close(nd.ni_vp, FREAD, cred, td);
err:
	free(data, M_FIRMWARE);
	if (bootverbose || warn)
		printf("%s: could not load binary firmware %s either\n", imagename, fn);
	sbuf_delete(sb);
}

static void
loadimage(void *arg, int npending __unused)
{
	struct fw_loadimage *fwli = arg;
	struct priv_fw *fp;
	linker_file_t result;
	int error;

	error = linker_reference_module(fwli->imagename, NULL, &result);
	if (error != 0) {
		if (bootverbose || (fwli->flags & FIRMWARE_GET_NOWARN) == 0)
			printf("%s: could not load firmware image, error %d\n",
			    fwli->imagename, error);
		try_binary_file(fwli->imagename, fwli->flags);
		mtx_lock(&firmware_mtx);
		goto done;
	}

	mtx_lock(&firmware_mtx);
	fp = lookup(fwli->imagename);
	if (fp == NULL || fp->file != NULL) {
		mtx_unlock(&firmware_mtx);
		if (fp == NULL)
			printf("%s: firmware image loaded, "
			    "but did not register\n", fwli->imagename);
		(void) linker_release_module(fwli->imagename, NULL, NULL);
		mtx_lock(&firmware_mtx);
		goto done;
	}
	fp->file = result;	/* record the module identity */
done:
	wakeup_one(arg);
	mtx_unlock(&firmware_mtx);
}

/*
 * Lookup and potentially load the specified firmware image.
 * If the firmware is not found in the registry, try to load a kernel
 * module named as the image name.
 * If the firmware is located, a reference is returned. The caller must
 * release this reference for the image to be eligible for removal/unload.
 */
const struct firmware *
firmware_get_flags(const char *imagename, uint32_t flags)
{
	struct task fwload_task;
	struct thread *td;
	struct priv_fw *fp;

	mtx_lock(&firmware_mtx);
	fp = lookup(imagename);
	if (fp != NULL)
		goto found;
	/*
	 * Image not present, try to load the module holding it.
	 */
	td = curthread;
	if (priv_check(td, PRIV_FIRMWARE_LOAD) != 0 ||
	    securelevel_gt(td->td_ucred, 0) != 0) {
		mtx_unlock(&firmware_mtx);
		printf("%s: insufficient privileges to "
		    "load firmware image %s\n", __func__, imagename);
		return NULL;
	}
	/*
	 * Defer load to a thread with known context.  linker_reference_module
	 * may do filesystem i/o which requires root & current dirs, etc.
	 * Also we must not hold any mtx's over this call which is problematic.
	 */
	if (!cold) {
		struct fw_loadimage fwli;

		fwli.imagename = imagename;
		fwli.flags = flags;
		TASK_INIT(&fwload_task, 0, loadimage, (void *)&fwli);
		taskqueue_enqueue(firmware_tq, &fwload_task);
		PHOLD(curproc);
		msleep((void *)&fwli, &firmware_mtx, 0, "fwload", 0);
		PRELE(curproc);
	}
	/*
	 * After attempting to load the module, see if the image is registered.
	 */
	fp = lookup(imagename);
	if (fp == NULL) {
		mtx_unlock(&firmware_mtx);
		return NULL;
	}
found:				/* common exit point on success */
	if (fp->refcnt == 0 && fp->parent != NULL)
		fp->parent->refcnt++;
	fp->refcnt++;
	mtx_unlock(&firmware_mtx);
	return &fp->fw;
}

const struct firmware *
firmware_get(const char *imagename)
{

	return (firmware_get_flags(imagename, 0));
}

/*
 * Release a reference to a firmware image returned by firmware_get.
 * The caller may specify, with the FIRMWARE_UNLOAD flag, its desire
 * to release the resource, but the flag is only advisory.
 *
 * If this is the last reference to the firmware image, and this is an
 * autoloaded module, wake up the firmware_unload_task to figure out
 * what to do with the associated module.
 */
void
firmware_put(const struct firmware *p, int flags)
{
	struct priv_fw *fp = PRIV_FW(p);

	mtx_lock(&firmware_mtx);
	fp->refcnt--;
	if (fp->refcnt == 0) {
		if (fp->parent != NULL)
			fp->parent->refcnt--;
		if (flags & FIRMWARE_UNLOAD)
			fp->flags |= FW_UNLOAD;
		if (fp->file)
			taskqueue_enqueue(firmware_tq, &firmware_unload_task);
	}
	mtx_unlock(&firmware_mtx);
}

/*
 * Setup directory state for the firmware_tq thread so we can do i/o.
 */
static void
set_rootvnode(void *arg, int npending)
{

	pwd_ensure_dirs();
	free(arg, M_TEMP);
}

/*
 * Event handler called on mounting of /; bounce a task
 * into the task queue thread to setup it's directories.
 */
static void
firmware_mountroot(void *arg)
{
	struct task *setroot_task;

	setroot_task = malloc(sizeof(struct task), M_TEMP, M_NOWAIT);
	if (setroot_task != NULL) {
		TASK_INIT(setroot_task, 0, set_rootvnode, setroot_task);
		taskqueue_enqueue(firmware_tq, setroot_task);
	} else
		printf("%s: no memory for task!\n", __func__);
}
EVENTHANDLER_DEFINE(mountroot, firmware_mountroot, NULL, 0);

/*
 * The body of the task in charge of unloading autoloaded modules
 * that are not needed anymore.
 * Images can be cross-linked so we may need to make multiple passes,
 * but the time we spend in the loop is bounded because we clear entries
 * as we touch them.
 */
static void
unloadentry(void *unused1, int unused2)
{
	struct priv_fw *fp, *tmp;

	mtx_lock(&firmware_mtx);
restart:
	LIST_FOREACH_SAFE(fp, &firmware_table, link, tmp) {
		if (((fp->flags & FW_BINARY) == 0 && fp->file == NULL) ||
		    fp->refcnt != 0 || (fp->flags & FW_UNLOAD) == 0)
			continue;

		/*
		 * If we directly loaded the firmware, then we just need to
		 * remove the entry from the list and free the entry and go to
		 * the next one.  There's no need for the indirection of the kld
		 * module case, we free memory and go to the next one.
		 */
		if ((fp->flags & FW_BINARY) != 0) {
			LIST_REMOVE(fp, link);
			free(__DECONST(char *, fp->fw.data), M_FIRMWARE);
			free(__DECONST(char *, fp->fw.name), M_FIRMWARE);
			free(fp, M_FIRMWARE);
			continue;
		}

		/*
		 * Found an entry.  This is the kld case, so we have a more
		 * complex dance.  Now:
		 * 1. make sure we scan the table again
		 * 2. clear FW_UNLOAD so we don't try this entry again.
		 * 3. release the lock while trying to unload the module.
		 */
		fp->flags &= ~FW_UNLOAD;	/* do not try again */

		/*
		 * We rely on the module to call firmware_unregister()
		 * on unload to actually free the entry.
		 */
		mtx_unlock(&firmware_mtx);
		(void)linker_release_module(NULL, NULL, fp->file);
		mtx_lock(&firmware_mtx);

		/*
		 * When we dropped the lock, another thread could have
		 * removed an element, so we must restart the scan.
		 */
		goto restart;
	}
	mtx_unlock(&firmware_mtx);
}

/*
 * Module glue.
 */
static int
firmware_modevent(module_t mod, int type, void *unused)
{
	struct priv_fw *fp;
	int err;

	err = 0;
	switch (type) {
	case MOD_LOAD:
		TASK_INIT(&firmware_unload_task, 0, unloadentry, NULL);
		firmware_tq = taskqueue_create("taskqueue_firmware", M_WAITOK,
		    taskqueue_thread_enqueue, &firmware_tq);
		/* NB: use our own loop routine that sets up context */
		(void) taskqueue_start_threads(&firmware_tq, 1, PWAIT,
		    "firmware taskq");
		if (rootvnode != NULL) {
			/*
			 * Root is already mounted so we won't get an event;
			 * simulate one here.
			 */
			firmware_mountroot(NULL);
		}
		break;

	case MOD_UNLOAD:
		/* request all autoloaded modules to be released */
		mtx_lock(&firmware_mtx);
		LIST_FOREACH(fp, &firmware_table, link)
			fp->flags |= FW_UNLOAD;
		mtx_unlock(&firmware_mtx);
		taskqueue_enqueue(firmware_tq, &firmware_unload_task);
		taskqueue_drain(firmware_tq, &firmware_unload_task);

		LIST_FOREACH(fp, &firmware_table, link) {
			if (fp->fw.name != NULL) {
				printf("%s: image %s still active, %d refs\n",
				    __func__, fp->fw.name, fp->refcnt);
				err = EINVAL;
			}
		}
		if (err == 0)
			taskqueue_free(firmware_tq);
		break;

	default:
		err = EOPNOTSUPP;
		break;
	}
	return (err);
}

static moduledata_t firmware_mod = {
	"firmware",
	firmware_modevent,
	NULL
};
DECLARE_MODULE(firmware, firmware_mod, SI_SUB_DRIVERS, SI_ORDER_FIRST);
MODULE_VERSION(firmware, 1);
