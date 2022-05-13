/*-
 * Copyright (c) 2021-2022 Juniper Networks
 *
 * This software was developed by Mitchell Horne <mhorne@FreeBSD.org>
 * under sponsorship from Juniper Networks and Klara Systems.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
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
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/caprights.h>
#include <sys/disk.h>
#include <sys/eventhandler.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/kerneldump.h>
#include <sys/limits.h>
#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>

#include <machine/vmparam.h>

static dumper_start_t vnode_dumper_start;
static dumper_t vnode_dump;
static dumper_hdr_t vnode_write_headers;

static struct sx livedump_sx;
SX_SYSINIT(livedump, &livedump_sx, "Livedump sx");

/*
 * Invoke a live minidump on the system.
 */
int
livedump_start(int fd, int flags, uint8_t compression)
{
#if MINIDUMP_PAGE_TRACKING == 1
	struct dumperinfo di, *livedi;
	struct diocskerneldump_arg kda;
	struct vnode *vp;
	struct file *fp;
	void *rl_cookie;
	int error;

	error = priv_check(curthread, PRIV_KMEM_READ);
	if (error != 0)
		return (error);

	if (flags != 0)
		return (EINVAL);

	error = getvnode(curthread, fd, &cap_write_rights, &fp);
	if (error != 0)
		return (error);
	vp = fp->f_vnode;

	if ((fp->f_flag & FWRITE) == 0) {
		error = EBADF;
		goto drop;
	}

	/* Set up a new dumper. */
	bzero(&di, sizeof(di));
	di.dumper_start = vnode_dumper_start;
	di.dumper = vnode_dump;
	di.dumper_hdr = vnode_write_headers;
	di.blocksize = PAGE_SIZE; /* Arbitrary. */
	di.maxiosize = MAXDUMPPGS * PAGE_SIZE;

	bzero(&kda, sizeof(kda));
	kda.kda_compression = compression;
	error = dumper_create(&di, "livedump", &kda, &livedi);
	if (error != 0)
		goto drop;

	/* Only allow one livedump to proceed at a time. */
	if (sx_try_xlock(&livedump_sx) == 0) {
		dumper_destroy(livedi);
		error = EBUSY;
		goto drop;
	}

	/* To be used by the callback functions. */
	livedi->priv = vp;

	/* Lock the entire file range and vnode. */
	rl_cookie = vn_rangelock_wlock(vp, 0, OFF_MAX);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);

	EVENTHANDLER_INVOKE(livedumper_start, &error);
	if (error != 0)
		goto out;

	dump_savectx();
	error = minidumpsys(livedi, true);

	EVENTHANDLER_INVOKE(livedumper_finish);
out:
	VOP_UNLOCK(vp);
	vn_rangelock_unlock(vp, rl_cookie);
	sx_xunlock(&livedump_sx);
	dumper_destroy(livedi);
drop:
	fdrop(fp, curthread);
	return (error);
#else
	return (EOPNOTSUPP);
#endif /* MINIDUMP_PAGE_TRACKING == 1 */
}

int
vnode_dumper_start(struct dumperinfo *di, void *key, uint32_t keysize)
{

	/* Always begin with an offset of zero. */
	di->dumpoff = 0;

	KASSERT(keysize == 0, ("encryption not supported for livedumps"));
	return (0);
}

/*
 * Callback from dumpsys() to dump a chunk of memory.
 *
 * Parameters:
 *	arg	 Opaque private pointer to vnode
 *	virtual  Virtual address (where to read the data from)
 *	offset	 Offset from start of core file
 *	length	 Data length
 *
 * Return value:
 *	0 on success
 *	errno on error
 */
int
vnode_dump(void *arg, void *virtual, off_t offset, size_t length)
{
	struct vnode *vp;
	int error = 0;

	vp = arg;
	MPASS(vp != NULL);
	ASSERT_VOP_LOCKED(vp, __func__);

	EVENTHANDLER_INVOKE(livedumper_dump, virtual, offset, length, &error);
	if (error != 0)
		return (error);

	/* Done? */
	if (virtual == NULL)
		return (0);

	error = vn_rdwr(UIO_WRITE, vp, virtual, length, offset, UIO_SYSSPACE,
	    IO_NODELOCKED, curthread->td_ucred, NOCRED, NULL, curthread);
	if (error != 0)
		uprintf("%s: error writing livedump block at offset %jx: %d\n",
		    __func__, (uintmax_t)offset, error);
	return (error);
}

/*
 * Callback from dumpsys() to write out the dump header, placed at the end.
 */
int
vnode_write_headers(struct dumperinfo *di, struct kerneldumpheader *kdh)
{
	struct vnode *vp;
	int error;
	off_t offset;

	vp = di->priv;
	MPASS(vp != NULL);
	ASSERT_VOP_LOCKED(vp, __func__);

	/* Compensate for compression/encryption adjustment of dumpoff. */
	offset = roundup2(di->dumpoff, di->blocksize);

	/* Write the kernel dump header to the end of the file. */
	error = vn_rdwr(UIO_WRITE, vp, kdh, sizeof(*kdh), offset,
	    UIO_SYSSPACE, IO_NODELOCKED, curthread->td_ucred, NOCRED, NULL,
	    curthread);
	if (error != 0)
		uprintf("%s: error writing livedump header: %d\n", __func__,
		    error);
	return (error);
}
