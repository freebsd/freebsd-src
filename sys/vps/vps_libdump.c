/*-
 * Copyright (c) 2009-2013 Klaus P. Ohrhallinger <k@7he.at>
 * All rights reserved.
 *
 * Development of this software was partly funded by:
 *    TransIP.nl <http://www.transip.nl/>
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

static const char vpsid[] =
    "$Id: vps_libdump.c 164 2013-06-10 12:46:17Z klaus $";

/*
 * cc -o dumptest -I. -DTEST=1 vps/vps_libdump.c
 */

#ifndef _KERNEL

#ifndef VIMAGE
#define VIMAGE  1
#endif
#ifndef VPS
#define VPS     1
#endif

#else
#include "opt_ddb.h"
#include "opt_global.h"
#include "opt_compat.h"
#endif

#ifdef VPS

#ifdef _KERNEL

#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/refcount.h>
#include <sys/sched.h>
#include <sys/sockio.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/ttycom.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <sys/uio.h>
#include <sys/resourcevar.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/namei.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/syscallsubr.h>
#include <sys/mman.h>
#include <sys/sleepqueue.h>
#include <sys/filedesc.h>
#include <sys/mount.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/pipe.h>
#include <sys/tty.h>
#include <sys/syscall.h>
#include <sys/un.h>
#include <sys/unpcb.h>
#include <sys/sem.h>
#include <sys/ktrace.h>
#include <sys/buf.h>
#include <sys/jail.h>
#include <sys/event.h>
#include <sys/eventvar.h>
#include <sys/umtx.h>

#include <machine/pcb.h>

#include <net/if.h>
#include <netinet/in.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vm_kern.h>

#include <machine/pcb.h>

#ifdef DDB
#include <ddb/ddb.h>
#endif

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_clone.h>
#include <net/if_types.h>
#include <net/if_var.h>
#include <net/ethernet.h>
#include <net/radix.h>
#include <net/route.h>
#include <net/vnet.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_fsm.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>

#include "vps_account.h"
#include "vps_user.h"
#include "vps_int.h"
#include "vps.h"
#include "vps2.h"

#define _VPS_SNAPST_H_ALL
#include <vps/vps_snapst.h>

#define printf DBGCORE

/* object functions */
struct vps_dumpobj *vps_dumpobj_create(struct vps_snapst_ctx *ctx,
    int type, int how);
void *vps_dumpobj_space(struct vps_snapst_ctx *ctx, long size, int how);
int vps_dumpobj_append(struct vps_snapst_ctx *ctx, const void *data,
    long size, int how);
void vps_dumpobj_close(struct vps_snapst_ctx *ctx);
void vps_dumpobj_discard(struct vps_snapst_ctx *ctx, struct vps_dumpobj *o);
int vps_dumpobj_checkobj(struct vps_snapst_ctx *ctx, struct vps_dumpobj *o);
void vps_dumpobj_setcur(struct vps_snapst_ctx *ctx, struct vps_dumpobj *o);
struct vps_dumpobj *vps_dumpobj_next(struct vps_snapst_ctx *ctx);
struct vps_dumpobj *vps_dumpobj_prev(struct vps_snapst_ctx *ctx);
struct vps_dumpobj *vps_dumpobj_peek(struct vps_snapst_ctx *ctx);
struct vps_dumpobj *vps_dumpobj_getcur(struct vps_snapst_ctx *ctx);
int vps_dumpobj_typeofnext(struct vps_snapst_ctx *ctx);
int vps_dumpobj_nextischild(struct vps_snapst_ctx *ctx,
    struct vps_dumpobj *op);
int vps_dumpobj_recurse(struct vps_snapst_ctx *ctx, struct vps_dumpobj *o,
        void (*func)(struct vps_snapst_ctx *ctx, struct vps_dumpobj *));

/* tree functions */
int vps_dumpobj_makerelative(struct vps_snapst_ctx *ctx);
int vps_dumpobj_makeabsolute(struct vps_snapst_ctx *ctx);
int vps_dumpobj_printtree(struct vps_snapst_ctx *ctx);
int vps_dumpobj_checktree(struct vps_snapst_ctx *ctx);

/* various subroutines */
int vps_dumpobj_checkptr(struct vps_snapst_ctx *ctx, void *p, size_t off);
const char *vps_libdump_objtype2str(int objt);
int vps_libdump_checkheader(struct vps_dumpheader *h);
void vps_libdump_printheader(struct vps_dumpheader *h);

#else /* !_KERNEL */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <err.h>

#define panic	printf

#endif /* !_KERNEL */

struct vps_snapst_ctx;

#include <vps/vps_libdump.h>

static void __vps_dumpobj_printtree(struct vps_snapst_ctx *ctx,
    struct vps_dumpobj *o);

#ifdef TEST

int test01(void);
int checkfile(const char *);

int
main(int argc, char **argv, char **envv)
{
	int error;

	if (argc > 2 && strcmp(argv[1], "checkfile")==0) {
		error = checkfile(argv[2]);
	} else if (argc > 1 && strcmp(argv[1], "structsizes")==0) {
		error = structsizes();
	} else {
		error = test01();
	}

	return (error);
}

#define TOSTRING(s) #s
#define PRINT_STRUCT_SIZE(s)	\
	printf("%s: %d %s\n",	\
		TOSTRING(s),	\
		sizeof(struct s),	\
		(sizeof(struct s) % 8) ? "NOT 64bit aligned" : "ok"	\
	);

int
structsizes(void)
{

	PRINT_STRUCT_SIZE(vps_dump_sysinfo);
	PRINT_STRUCT_SIZE(vps_dump_vps);
	PRINT_STRUCT_SIZE(vps_dump_mount);
	PRINT_STRUCT_SIZE(vps_dump_vnet);
	PRINT_STRUCT_SIZE(vps_dump_vnet_ifnet);
	PRINT_STRUCT_SIZE(vps_dump_vnet_ifaddr);
	PRINT_STRUCT_SIZE(vps_dump_vnet_sockaddr);
	PRINT_STRUCT_SIZE(vps_dump_vnet_inet6_lifetime);
	PRINT_STRUCT_SIZE(vps_dump_ucred);
	PRINT_STRUCT_SIZE(vps_dump_prison);
	PRINT_STRUCT_SIZE(vps_dump_pgrp);
	PRINT_STRUCT_SIZE(vps_dump_session);
	PRINT_STRUCT_SIZE(vps_dump_proc);
	PRINT_STRUCT_SIZE(vps_dump_pargs);
	PRINT_STRUCT_SIZE(vps_dump_savefpu);
	PRINT_STRUCT_SIZE(vps_dump_sysentvec);
	PRINT_STRUCT_SIZE(vps_dump_vmmap);
	PRINT_STRUCT_SIZE(vps_dump_vmspace);
	PRINT_STRUCT_SIZE(vps_dump_vmmapentry);
	PRINT_STRUCT_SIZE(vps_dump_vmobject);
	PRINT_STRUCT_SIZE(vps_dump_vmpages);
	PRINT_STRUCT_SIZE(vps_dump_thread);
	PRINT_STRUCT_SIZE(vps_dump_filedesc);
	PRINT_STRUCT_SIZE(vps_dump_file);
	PRINT_STRUCT_SIZE(vps_dump_pipe);
	PRINT_STRUCT_SIZE(vps_dump_filepath);
	PRINT_STRUCT_SIZE(vps_dump_pts);
	PRINT_STRUCT_SIZE(vps_dump_socket);
	PRINT_STRUCT_SIZE(vps_dump_unixpcb);
	PRINT_STRUCT_SIZE(vps_dump_inetpcb);
	PRINT_STRUCT_SIZE(vps_dump_udppcb);
	PRINT_STRUCT_SIZE(vps_dump_tcppcb);
	PRINT_STRUCT_SIZE(vps_dump_sockbuf);
	PRINT_STRUCT_SIZE(vps_dump_mbufchain);
	PRINT_STRUCT_SIZE(vps_dump_mbuf);
	PRINT_STRUCT_SIZE(vps_dump_vmpageref);
	PRINT_STRUCT_SIZE(vps_dump_route);
	PRINT_STRUCT_SIZE(vps_dump_knote);
	PRINT_STRUCT_SIZE(vps_dump_accounting_val);
	PRINT_STRUCT_SIZE(vps_dump_accounting);
	PRINT_STRUCT_SIZE(vps_dump_arg_ip4);
	PRINT_STRUCT_SIZE(vps_dump_arg_ip6);
	PRINT_STRUCT_SIZE(vps_dump_arg);
	PRINT_STRUCT_SIZE(vps_dump_sysv_ipcperm);
	PRINT_STRUCT_SIZE(vps_dump_sysvmsg_msginfo);
	PRINT_STRUCT_SIZE(vps_dump_sysvmsg_msg);
	PRINT_STRUCT_SIZE(vps_dump_sysvmsg_msqid);
	PRINT_STRUCT_SIZE(vps_dump_sysvsem_seminfo);
	PRINT_STRUCT_SIZE(vps_dump_sysvsem_semid);
	PRINT_STRUCT_SIZE(vps_dump_sysvsem_sem);
	PRINT_STRUCT_SIZE(vps_dump_sysvsem_sem_undo);
	PRINT_STRUCT_SIZE(vps_dump_sysvshm_shmid);
	PRINT_STRUCT_SIZE(vps_dump_sysvshm_shminfo);
	PRINT_STRUCT_SIZE(vps_dump_sysvshm_shmmap_state);

	return (0);
}

int
checkfile(const char *path)
{
	int fd;
	int size;
	void *p;
	struct stat sb;
	struct vps_snapst_ctx *ctx;

	if ((fd = open(path, O_RDONLY)) == -1)
		err(1, "open");

	if ((fstat(fd, &sb)) == -1)
		err(1, "stat");
	size = sb.st_size;

	if ((p = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_PRIVATE,
	    fd, 0)) == MAP_FAILED)
		err(1, "mmap");

	ctx = malloc(sizeof(*ctx));
	memset(ctx, 0, sizeof(*ctx));
	ctx->data = p;
	ctx->dsize = size;
	ctx->rootobj = (struct vps_dumpobj *)
	    (ctx->data + sizeof(struct vps_dumpheader));
	ctx->relative = 1;
	ctx->elements = -1;

	vps_libdump_printheader(p);

        if (vps_dumpobj_printtree(ctx))
		printf("%s: tree is invalid !\n", __func__);
	else
		printf("%s: tree is good !\n", __func__);

	free(ctx);
	munmap(p, size);
	close(fd);

	return (0);
}

int
test01(void)
{
	void *data;
	long size;
	struct vps_snapst_ctx ctx2;
	struct vps_snapst_ctx *ctx = &ctx2;
	struct vps_dumpheader *h;
	struct vps_dumpobj *o, *o2;

	size = 0x10000;
	data = malloc(size);

	ctx->data = ctx->cpos = data;
	ctx->maxsize = size;
	ctx->dsize = 0;
	if (vps_dumpobj_printtree(ctx))
		printf("%s: tree is invalid !\n", __func__);
	else
		printf("%s: tree is good !\n", __func__);

	memset(data, 0x0, size);

	memset(ctx, 0x0, sizeof(*ctx));
	ctx->data = ctx->cpos = data;
	ctx->maxsize = size;
	ctx->dsize = 0;
	ctx->relative = 0;

	h = (struct vps_dumpheader *)data;

	h->byteorder = VPS_DUMPH_LSB;
	h->ptrsize = VPS_DUMPH_64BIT;
	h->pageshift = 12;
	h->version = VPS_DUMPH_VERSION;
	h->magic = VPS_DUMPH_MAGIC;
	h->nsyspages = 10;
	h->nuserpages = 20;
	h->size = (h->nsyspages + h->nuserpages) << h->pageshift;

	/*
	memset(&h->hostname, 'x', 0x180);
	*/

	vps_libdump_printheader(h);

	ctx->rootobj = vps_dumpobj_create(ctx, VPS_DUMPOBJT_ROOT, 0);
	vps_dumpobj_append(ctx, main, 0x101, 0);
	  vps_dumpobj_create(ctx, VPS_DUMPOBJT_VPS, 0);
	  vps_dumpobj_close(ctx);
	  //again:
	  o2 = vps_dumpobj_create(ctx, VPS_DUMPOBJT_MOUNT, 0);
	    vps_dumpobj_create(ctx, VPS_DUMPOBJT_UCRED, 0);
	    //vps_dumpobj_discard(ctx, o2);
	    //goto again;
	    vps_dumpobj_close(ctx);
	  vps_dumpobj_close(ctx);
	  vps_dumpobj_create(ctx, VPS_DUMPOBJT_ARG, 0);
	  vps_dumpobj_close(ctx);
	vps_dumpobj_close(ctx);

	if (ctx->level != 0) {
		printf("ERROR: ctx->level = %d\n", ctx->level);
	}
	printf("ctx->elements = %d\n", ctx->elements);
	printf("ctx->dsize = %d\n", ctx->dsize);

	if (vps_dumpobj_printtree(ctx))
		printf("%s: tree is invalid !\n", __func__);
	else
		printf("%s: tree is good !\n", __func__);

	printf("####### absolute --> relative\n");
	if (vps_dumpobj_makerelative(ctx))
		printf("%s: tree is invalid !\n", __func__);
	else
		printf("%s: tree is good !\n", __func__);

	if (vps_dumpobj_printtree(ctx))
		printf("%s: tree is invalid !\n", __func__);
	else
		printf("%s: tree is good !\n", __func__);

	printf("####### relative --> absolute\n");
	if (vps_dumpobj_makeabsolute(ctx))
		printf("%s: tree is invalid !\n", __func__);
	else
		printf("%s: tree is good !\n", __func__);

	if (vps_dumpobj_printtree(ctx))
		printf("%s: tree is invalid !\n", __func__);
	else
		printf("%s: tree is good !\n", __func__);

	o = ctx->curobj = ctx->rootobj;
	do {
		if (vps_dumpobj_checkobj(ctx, o))
			break;
		__vps_dumpobj_printtree(ctx, o);
		o = vps_dumpobj_next(ctx);
	} while (o != NULL);

	free(data);

	return (0);
}
#endif /* TEST */

/* Create a new element. Is always a child of the current open element. */
struct vps_dumpobj *
vps_dumpobj_create(struct vps_snapst_ctx *ctx, int type, __unused int how)
{
	struct vps_dumpobj *o;
	int inc;

#ifdef _KERNEL
	if ((how & ~(M_WAITOK|M_NOWAIT)) != 0) {
		printf("%s: invalid alloc flag\n", __func__);
		return (NULL);
	}
#endif /* !_KERNEL */

	if (((offset)ctx->cpos & ALIGN_MASK) != 0) {
		inc = sizeof(ptr) - ((offset)ctx->cpos & ALIGN_MASK);
		printf("%s: spaced objects by %d bytes for alignment.\n",
			__func__, inc);
	} else {
		inc = 0;
	}

	/* alloc */
#ifdef _KERNEL
	if (vps_ctx_extend(ctx, NULL, sizeof(*o) + inc, how)) {
		printf("%s: allocation failed\n", __func__);
		return (NULL);
	}
#endif /* !_KERNEL */
	if (inc != 0) {
		ctx->curobj->size += inc;
		ctx->curobj->next = (char *)ctx->curobj->next + inc;
		ctx->cpos = (char *)ctx->cpos + inc;
		ctx->dsize += inc;
	}
	o = (struct vps_dumpobj *)ctx->cpos;
	ctx->cpos = (char *)ctx->cpos + sizeof(*o);
	ctx->dsize += sizeof(*o);

	o->magic = VPS_DUMPH_MAGIC;
	o->type = type;
	o->level = ++ctx->level;
	o->size = sizeof(*o);
	o->prio = 0;
	o->next = ctx->cpos;
	o->parent = ctx->curobj;
	if (ctx->elements == 0) {
		o->parent = o;
		ctx->rootobj = o;
	}

	ctx->curobj = o;
	ctx->lastobj = o;

	ctx->elements++;

	/*
	printf("o=%p, magic=%08x, type=%d, size=%u level=%d, next=%p\n",
		o, o->magic, o->type, o->size, o->level, o->next);
	*/

	return (o);
}

/* Discard element (can be called in case further allocation failed). */
void
vps_dumpobj_discard(struct vps_snapst_ctx *ctx, struct vps_dumpobj *o)
{
	struct vps_dumpobj *o2, *op;
	int cnt;

	printf("%s: DISCARDING object %p: magic=%08x type=%d size=%d\n",
		__func__, o, o->magic, o->type, o->size);

	/* for debugging */
	o->magic = 0xaaaaaaaa;

	/* Edge case */
	if (o == ctx->rootobj) {
		ctx->cpos = ctx->data;
		ctx->dsize = 0;
		ctx->rootobj = NULL;
		ctx->curobj = NULL;
		ctx->lastobj = NULL;
		ctx->elements = 0;
		return;
	}

	/* Looking up the previous element. */
	op = NULL;
	o2 = ctx->rootobj;
	cnt = 0;
	do {
		if (o2->next == o) {
			op = o2;
			break;
		}
		if (vps_dumpobj_checkptr(ctx, o2->next, 0)) {
			/*vps_dumpobj_printtree(ctx);*/
			panic("%s: tree is invalid! ctx->rootobj=%p "
			    "o=%p o2=%p cnt=%d\n",
			    __func__, ctx->rootobj, o, o2, cnt);
		}
		o2 = o2->next;
		cnt++;
	} while (o2 < o);

	if (op == NULL)
		panic("%s: op == NULL (o=%p cnt=%d) !\n",
			__func__, o, cnt);

	ctx->cpos = o;
	ctx->dsize = (char *)ctx->cpos - (char *)ctx->data;
	ctx->curobj = op;
	ctx->lastobj = op;
	ctx->level = op->level;
	ctx->elements = cnt + 1;
	op->next = ctx->cpos;
}

/* Reserve space in the current element and return pointer. */
void *
vps_dumpobj_space(struct vps_snapst_ctx *ctx, long size, __unused int how)
{
	struct vps_dumpobj *o;
	void *p;

	o = ctx->curobj;

	/* alloc */
#ifdef _KERNEL
	if ((how & ~(M_WAITOK|M_NOWAIT)) != 0) {
		printf("%s: invalid alloc flag\n", __func__);
		return (NULL);
	}
	if (vps_ctx_extend(ctx, NULL, size + 8, how)) {
		printf("%s: allocation failed\n", __func__);
		return (NULL);
	}
#endif /* !_KERNEL */

	bzero(ctx->cpos, size + 8);

	p = ctx->cpos;

	ctx->cpos = (char *)ctx->cpos + size;
	ctx->dsize += size;

	o->size += size;
	o->next = (char *)o->next + size;

	return (p);
}

/* Append data to the current element. */
int
vps_dumpobj_append(struct vps_snapst_ctx *ctx, const void *data,
    long size, __unused int how)
{
	struct vps_dumpobj *o;

	o = ctx->curobj;

	/* alloc */
#ifdef _KERNEL
	if ((how & ~(M_WAITOK|M_NOWAIT)) != 0) {
		printf("%s: invalid alloc flag\n", __func__);
		return (EINVAL);
	}
	if (vps_ctx_extend(ctx, NULL, size + 8, how)) {
		printf("%s: allocation failed\n", __func__);
		return (ENOMEM);
	}
#endif /* !_KERNEL */

	memcpy(ctx->cpos, data, size);
	ctx->cpos = (char *)ctx->cpos + size;
	ctx->dsize += size;

	o->size += size;
	o->next = (char *)o->next + size;

	return (0);
}

/* Close the current element. Its parent is the new current element. */
void
vps_dumpobj_close(struct vps_snapst_ctx *ctx)
{
	struct vps_dumpobj *o;
	int inc;

	o = ctx->curobj;

	if (o == NULL) {
		printf("%s: ctx->curobj == NULL !\n", __func__);
		return;
	}

	/* _append and _space always reserve extra space for alignment. */
	if (((offset)ctx->cpos & ALIGN_MASK) != 0) {
		inc = sizeof(ptr) - ((offset)ctx->cpos & ALIGN_MASK);
		o->size += inc;
		o->next = (char *)o->next + inc;
		ctx->cpos = (char *)ctx->cpos + inc;
		ctx->dsize += inc;
		printf("%s: increased object by %d bytes for alignment.\n",
			__func__, inc);
	}

	ctx->level--;
	ctx->curobj = o->parent;

	if (ctx->level == 0)
		ctx->lastobj->next = ctx->rootobj;
}

struct vps_dumpobj *
vps_dumpobj_next(struct vps_snapst_ctx *ctx)
{
	struct vps_dumpobj *o;

	if (ctx->relative)
		return (NULL);

	/* Assumes that the current object has been validated. */
	o = ctx->curobj->next;
	if (o == ctx->rootobj)
		return (NULL);

	if (vps_dumpobj_checkobj(ctx, o))
		return (NULL);

	ctx->lastobj = ctx->curobj;
	ctx->curobj = o;

	return (o);
}

int
vps_dumpobj_typeofnext(struct vps_snapst_ctx *ctx)
{
	struct vps_dumpobj *o;

	if (ctx->relative)
		return (0);

	/* Assumes that the current object has been validated. */
	o = ctx->curobj->next;
	if (o == ctx->rootobj)
		return (0);

	if (vps_dumpobj_checkobj(ctx, o))
		return (0);

	return (o->type);
}

struct vps_dumpobj *
vps_dumpobj_peek(struct vps_snapst_ctx *ctx)
{
	struct vps_dumpobj *o;

	if (ctx->relative)
		return (0);

	/* Assumes that the current object has been validated. */
	o = ctx->curobj->next;
	if (o == ctx->rootobj)
		return (0);

	if (vps_dumpobj_checkobj(ctx, o))
		return (0);

	return (o);
}

struct vps_dumpobj *
vps_dumpobj_getcur(struct vps_snapst_ctx *ctx)
{

	if (ctx->relative)
		return (NULL);

	return (ctx->curobj);
}

int
vps_dumpobj_nextischild(struct vps_snapst_ctx *ctx, struct vps_dumpobj *op)
{
	struct vps_dumpobj *o;

	if (ctx->relative)
		return (0);

	/* Assumes that the current object has been validated. */
	o = ctx->curobj->next;
	if (o == ctx->rootobj)
		return (0);

	if (vps_dumpobj_checkobj(ctx, o))
		return (0);

	while (o != ctx->rootobj) {
		if (o->parent == op)
			return (1);
		o = o->parent;
	}

	return (0);
}

/*
 * XXX Can be only called once since ctx->lastobj is invalid after first
 *     call without _next in between
 */
struct vps_dumpobj *
vps_dumpobj_prev(struct vps_snapst_ctx *ctx)
{

	if (ctx->relative)
		return (NULL);

	if (ctx->curobj == ctx->lastobj) {
		panic("%s: called twice !\n", __func__);
	}

	ctx->curobj = ctx->lastobj;

	return (ctx->curobj);
}

void
vps_dumpobj_setcur(struct vps_snapst_ctx *ctx, struct vps_dumpobj *o)
{

	if (ctx->relative)
		return;

	/* XXX
	if (vps_dumpobj_checkobj(ctx, o))
	*/

	ctx->curobj = o;

	/* NOTE: ctx->lastobj is invalid now */
}

/* Check a single object for sanity. */
int
vps_dumpobj_checkobj(struct vps_snapst_ctx *ctx, struct vps_dumpobj *o)
{
	struct vps_dumpobj *op, *on;

	if (vps_dumpobj_checkptr(ctx, o, 0) ||
	    vps_dumpobj_checkptr(ctx, o, sizeof(*o))) {
		printf("%s: invalid #1\n", __func__);
		return (1);
	}

	/* check parent object */
	op = o->parent;
	if (ctx->relative)
		op = (void *)((offset)op + (offset)ctx->data);

	if (vps_dumpobj_checkptr(ctx, op, 0) ||
	    vps_dumpobj_checkptr(ctx, op, sizeof(*o))) {
		printf("%s: invalid #2\n", __func__);
		return (1);
	}

	/* only the root object can have itself as parent */
	if (op == o && o != ctx->rootobj) {
		printf("%s: invalid #3\n", __func__);
		return (1);
	}
	if (op != o) {
		/* is this exactly one level lower than the parent ? */
		if (o->level != op->level + 1) {
			printf("%s: invalid #4\n", __func__);
			return (1);
		}
	}

	/* check next object */
	on = o->next;
	if (ctx->relative)
		on = (void *)((offset)on + (offset)ctx->data);

	if (vps_dumpobj_checkptr(ctx, on, 0) ||
	    vps_dumpobj_checkptr(ctx, on, sizeof(*o))) {
		printf("%s: invalid #5\n", __func__);
		return (1);
	}

	/* only the root object can have itself as next object  */
	if (on == o && (o != ctx->rootobj || ctx->elements != 1)) {
		printf("%s: invalid #6\n", __func__);
		return (1);
	}
	if (on != o) {
		if (ctx->elements != -1 && ctx->elements < 2) {
			printf("%s: invalid #7\n", __func__);
			return (1);
		}
		/* the next object must be exactly at this one
		   plus its size */
		if (on != ctx->rootobj && (offset)o + o->size !=
		    (offset)on) {
			printf("%s: invalid #8\n", __func__);
			return (1);
		}
	}

	return (0);
}

int
vps_dumpobj_checkptr(struct vps_snapst_ctx *ctx, void *p, size_t off)
{

	if ((char *)p + off < (char *)ctx->data) {
		printf("%s: invalid ptr=%p; < ctx->data=%p\n",
			__func__, (char *)p + off, ctx->data);
		return (1);
	}
	if ((char *)p + off > ((char *)ctx->data) + ctx->dsize) {
		printf("%s: invalid ptr=%p; > ctx->data+ctx->dsize=%p\n",
		    __func__, (char *)p + off,
		    ((char *)ctx->data) + ctx->dsize);
		return (1);
	}

	return (0);
}

int
vps_dumpobj_recurse(struct vps_snapst_ctx *ctx, struct vps_dumpobj *o,
	void (*func)(struct vps_snapst_ctx *ctx, struct vps_dumpobj *))
{
	struct vps_dumpobj *o2, *o3, *op;
	/* XXX */
	static int recurse = 0;

	if (o == ctx->rootobj)
		recurse = 0;

	/*printf("%s: recurse=%d o=%p\n", __func__, recurse, o);*/

	if (vps_dumpobj_checkptr(ctx, o, 0)) {
		printf("%s: invalid #1\n", __func__);
		return (1);
	}
	if (vps_dumpobj_checkptr(ctx, o, sizeof(*o))) {
		printf("%s: invalid #2\n", __func__);
		return (1);
	}

	o2 = o->next;
	if (ctx->relative)
		o2 = (void *)((offset)o2 + (offset)ctx->data);

	if (vps_dumpobj_checkobj(ctx, o))
		return (1);

	if (func != NULL)
		func(ctx, o);

	if (o2 == ctx->rootobj) {
		/*printf("%s: ret #1\n", __func__);*/
		return (0);
	}
	if (vps_dumpobj_checkptr(ctx, o2, 0)) {
		printf("%s: invalid #3\n", __func__);
		return (1);
	}
	if (vps_dumpobj_checkptr(ctx, o2, sizeof(*o2))) {
		printf("%s: invalid #4\n", __func__);
		return (1);
	}

	while (o2 != NULL) {

		o3 = o2->next;
		op = o2->parent;
		if (ctx->relative) {
			o3 = (void *)((offset)o3 + (offset)ctx->data);
			/*
			printf("%s: fixed up %p --> %p (data=%p)\n",
				__func__, o2->next, o3, ctx->data);
			*/
			op = (void *)((offset)o2->parent +
			    (offset)ctx->data);
		}

		if (op == o) {
			if (o2->level != o->level + 1) {
				printf("%s: invalid #7\n", __func__);
				return (1);
			}
			++recurse;
			vps_dumpobj_recurse(ctx, o2, func);
			--recurse;
		}

		if (o2->level <= o->level) {
			/*printf("%s: ret #3\n", __func__);*/
			return (0);
		}

		if (o3 == ctx->rootobj) {
			/*printf("%s: ret #2\n", __func__);*/
			return (0);
		}
		if (vps_dumpobj_checkptr(ctx, o3, 0)) {
			printf("%s: invalid #5\n", __func__);
			return (1);
		}
		if (vps_dumpobj_checkptr(ctx, o3, sizeof(*o3))) {
			printf("%s: invalid #6\n", __func__);
			return (1);
		}

		o2 = o3;

		/*
		printf("%s: o2=%p o2->level=%d o->level=%d recurse=%d\n",
			__func__, o2, o2->level, o->level, recurse);
		*/
	}

	/*printf("%s: --recurse\n", __func__);*/

	return (0);
}

/* Make all pointers in object tree relative to data start. */
int
vps_dumpobj_makeabsolute(struct vps_snapst_ctx *ctx)
{
	struct vps_dumpobj *o;

	if (ctx->relative == 0) {
		printf("%s: is not relative !\n", __func__);
		return (1);
	}

	o = ctx->curobj = ctx->rootobj;
	do {
		o->next = (void *)((offset)o->next + (offset)ctx->data);
		o->parent = (void *)((offset)o->parent + (offset)ctx->data);
		o = o->next;
	} while (o != ctx->rootobj);

	ctx->relative = 0;

	return (0);
}

/* Make all pointers in object tree relative to data start. */
int
vps_dumpobj_makerelative(struct vps_snapst_ctx *ctx)
{
	struct vps_dumpobj *o, *o2;

	if (ctx->relative != 0) {
		printf("%s: is already relative !\n", __func__);
		return (1);
	}

	o = ctx->curobj = ctx->rootobj;
	do {
		o2 = o->next;
		o->next = (void *)((char *)o->next - (char *)ctx->data);
		o->parent = (void *)((char *)o->parent - (char *)ctx->data);
		o = o2;
	} while (o != NULL && o != ctx->rootobj);

	ctx->relative = 1;

	return (0);
}

static void
__vps_dumpobj_printtree(__unused struct vps_snapst_ctx *ctx,
   struct vps_dumpobj *o)
{
	char ident[0x100];

	memset(ident, ' ', o->level * 4);
	ident[o->level * 4] = 0;

	printf("%so=%p\n", ident, o);
	printf("%smagic=%08x\n", ident, o->magic);
	printf("%stype=%d [%s]\n", ident, o->type,
	    vps_libdump_objtype2str(o->type));
	printf("%slevel=%d\n", ident, o->level);
	printf("%ssize=%d\n", ident, o->size);
	printf("%sparent=%p\n", ident, o->parent);
	printf("%snext=%p\n", ident, o->next);
}

int
vps_dumpobj_printtree(struct vps_snapst_ctx *ctx)
{

	return (vps_dumpobj_recurse(ctx, ctx->rootobj,
	    __vps_dumpobj_printtree));
}

int
vps_dumpobj_checktree(struct vps_snapst_ctx *ctx)
{

	return (vps_dumpobj_recurse(ctx, ctx->rootobj, NULL));
}

/* Check if a character string of given size is null-terminated. */
__attribute__((unused))
static int
vps_libdump_strterminated(const char *str, long size)
{
	const char *p = str;

	while (p < (str + size))
		if (*p++ == '\0')
			return (1);

	return (0);
}

/*
 * 0 --> valid
 */
int
vps_libdump_checkheader(struct vps_dumpheader *h)
{
	int rv = 0;

	if (h->byteorder != VPS_DUMPH_MSB && h->byteorder != VPS_DUMPH_LSB)
		++rv;
	if (h->ptrsize != VPS_DUMPH_32BIT && h->ptrsize != VPS_DUMPH_64BIT)
		++rv;
	if (h->version < 0x20120606)
		++rv;
	if (h->magic != VPS_DUMPH_MAGIC)
		++rv;
	if (h->nsyspages < 1)
		++rv;
	if ((h->nsyspages + h->nuserpages) << h->pageshift != h->size)
		++rv;
	if (h->pageshift != PAGE_SHIFT)
		++rv;

	/* check strings for null termination */
	/*
	if (!vps_libdump_strterminated(h->kernel, sizeof(h->kernel)))
		++rv;
	*/

	/* XXX check checksum of syspages stuff */

	return (rv);
}

void
vps_libdump_printheader(struct vps_dumpheader *h)
{
	const char *byteorder;

	if (vps_libdump_checkheader(h) == 0) {
		printf("header is valid.\n");
	} else {
		printf("header is invalid.\n");
	}

	switch (h->byteorder) {
	case VPS_DUMPH_MSB: byteorder = "MSB"; break;
	case VPS_DUMPH_LSB: byteorder = "LSB"; break;
	default: byteorder = "UNKNOWN"; break;
	}
	printf("byteorder:       %s\n",
	    byteorder);
	printf("ptrsize:         %u bit\n",
	    h->ptrsize);
	printf("pageshift:       %u --> pagesize=%u\n",
	    h->pageshift, 1 << h->pageshift);
	printf("version:         %08x\n",
	    h->version);
	printf("magic:           %08x\n",
	    h->magic);
	printf("time:            %lld\n",
	    (long long signed int)h->time);
	printf("size:            0x%016llx\n",
	    (long long unsigned int)h->size);
	printf("checksum:        0x%016llx\n",
	    (long long unsigned int)h->checksum);
	printf("nsyspages:       0x%08x\n",
	    h->nsyspages);
	printf("nuserpages:      0x%08x\n",
	    h->nuserpages);
}

const char *
vps_libdump_objtype2str(int objt)
{

	switch (objt) {
	case VPS_DUMPOBJT_ROOT:
		return ("VPS_DUMPOBJT_ROOT");
	case VPS_DUMPOBJT_SYSINFO:
		return ("VPS_DUMPOBJT_SYSINFO");
	case VPS_DUMPOBJT_VPS:
		return ("VPS_DUMPOBJT_VPS");
	case VPS_DUMPOBJT_ARG:
		return ("VPS_DUMPOBJT_ARG");
	case VPS_DUMPOBJT_END:
		return ("VPS_DUMPOBJT_END");
	case VPS_DUMPOBJT_PROC:
		return ("VPS_DUMPOBJT_PROC");
	case VPS_DUMPOBJT_THREAD:
		return ("VPS_DUMPOBJT_THREAD");
	case VPS_DUMPOBJT_PGRP:
		return ("VPS_DUMPOBJT_PGRP");
	case VPS_DUMPOBJT_SESSION:
		return ("VPS_DUMPOBJT_SESSION");
	case VPS_DUMPOBJT_SYSENTVEC:
		return ("VPS_DUMPOBJT_SYSENTVEC");
	case VPS_DUMPOBJT_VMSPACE:
		return ("VPS_DUMPOBJT_VMSPACE");
	case VPS_DUMPOBJT_VMMAPENTRY:
		return ("VPS_DUMPOBJT_VMMAPENTRY");
	case VPS_DUMPOBJT_VMOBJECT:
		return ("VPS_DUMPOBJT_VMOBJECT");
	case VPS_DUMPOBJT_VMPAGE:
		return ("VPS_DUMPOBJT_VMPAGE");
	case VPS_DUMPOBJT_VMOBJ_VNPATH:
		return ("VPS_DUMPOBJT_VMOBJ_VNPATH");
	case VPS_DUMPOBJT_FDSET:
		return ("VPS_DUMPOBJT_FDSET");
	case VPS_DUMPOBJT_FILE:
		return ("VPS_DUMPOBJT_FILE");
	case VPS_DUMPOBJT_FILE_PATH:
		return ("VPS_DUMPOBJT_FILE_PATH");
	case VPS_DUMPOBJT_PTS:
		return ("VPS_DUMPOBJT_PTS");
	case VPS_DUMPOBJT_PIPE:
		return ("VPS_DUMPOBJT_PIPE");
	case VPS_DUMPOBJT_PARGS:
		return ("VPS_DUMPOBJT_PARGS");
	case VPS_DUMPOBJT_SOCKET:
		return ("VPS_DUMPOBJT_SOCKET");
	case VPS_DUMPOBJT_SOCKBUF:
		return ("VPS_DUMPOBJT_SOCKBUF");
	case VPS_DUMPOBJT_MBUFCHAIN:
		return ("VPS_DUMPOBJT_MBUFCHAIN");
	case VPS_DUMPOBJT_SOCKET_UNIX:
		return ("VPS_DUMPOBJT_SOCKET_UNIX");
	case VPS_DUMPOBJT_MOUNT:
		return ("VPS_DUMPOBJT_MOUNT");
	case VPS_DUMPOBJT_VNET_IFACE:
		return ("VPS_DUMPOBJT_VNET_IFACE");
	case VPS_DUMPOBJT_VNET_ADDR:
		return ("VPS_DUMPOBJT_VNET_ADDR");
	case VPS_DUMPOBJT_VNET_ROUTETABLE:
		return ("VPS_DUMPOBJT_VNET_ROUTETABLE");
	case VPS_DUMPOBJT_VNET_ROUTE:
		return ("VPS_DUMPOBJT_VNET_ROUTE");
	case VPS_DUMPOBJT_VNET:
		return ("VPS_DUMPOBJT_VNET");
	case VPS_DUMPOBJT_SYSVSEM_VPS:
		return ("VPS_DUMPOBJT_SYSVSEM_VPS");
	case VPS_DUMPOBJT_SYSVSEM_PROC:
		return ("VPS_DUMPOBJT_SYSVSEM_PROC");
	case VPS_DUMPOBJT_SYSVSHM_VPS:
		return ("VPS_DUMPOBJT_SYSVSHM_VPS");
	case VPS_DUMPOBJT_SYSVSHM_PROC:
		return ("VPS_DUMPOBJT_SYSVSHM_PROC");
	case VPS_DUMPOBJT_SYSVMSG_VPS:
		return ("VPS_DUMPOBJT_SYSVMSG_VPS");
	case VPS_DUMPOBJT_SYSVMSG_PROC:
		return ("VPS_DUMPOBJT_SYSVMSG_PROC");
	case VPS_DUMPOBJT_KQUEUE:
		return ("VPS_DUMPOBJT_KQUEUE");
	case VPS_DUMPOBJT_KNOTE:
		return ("VPS_DUMPOBJT_KNOTE");
	case VPS_DUMPOBJT_KEVENT:
		return ("VPS_DUMPOBJT_KEVENT");
	case VPS_DUMPOBJT_UMTX:
		return ("VPS_DUMPOBJT_UMTX");
	case VPS_DUMPOBJT_PRISON:
		return ("VPS_DUMPOBJT_PRISON");
	case VPS_DUMPOBJT_UCRED:
		return ("VPS_DUMPOBJT_UCRED");
	default:
		return ("UNKOWN");
	}
}

#ifdef _KERNEL

static int
vps_libdump_modevent(module_t mod, int type, void *data)
{
	int error = 0;

	switch (type) {
	case MOD_LOAD:
		vps_func->vps_dumpobj_create =
		    vps_dumpobj_create;
		vps_func->vps_dumpobj_space =
		    vps_dumpobj_space;
		vps_func->vps_dumpobj_append =
		    vps_dumpobj_append;
		vps_func->vps_dumpobj_close =
		    vps_dumpobj_close;
		vps_func->vps_dumpobj_discard =
		    vps_dumpobj_discard;
		vps_func->vps_dumpobj_checkobj =
		    vps_dumpobj_checkobj;
		vps_func->vps_dumpobj_setcur =
		    vps_dumpobj_setcur;
		vps_func->vps_dumpobj_next =
		    vps_dumpobj_next;
		vps_func->vps_dumpobj_prev =
		    vps_dumpobj_prev;
		vps_func->vps_dumpobj_peek =
		    vps_dumpobj_peek;
		vps_func->vps_dumpobj_getcur =
		    vps_dumpobj_getcur;
		vps_func->vps_dumpobj_typeofnext =
		    vps_dumpobj_typeofnext;
		vps_func->vps_dumpobj_nextischild =
		    vps_dumpobj_nextischild;
		vps_func->vps_dumpobj_recurse =
		    vps_dumpobj_recurse;
		vps_func->vps_dumpobj_makerelative =
		    vps_dumpobj_makerelative;
		vps_func->vps_dumpobj_makeabsolute =
		    vps_dumpobj_makeabsolute;
		vps_func->vps_dumpobj_printtree =
		    vps_dumpobj_printtree;
		vps_func->vps_dumpobj_checktree =
		    vps_dumpobj_checktree;
		vps_func->vps_dumpobj_checkptr =
		    vps_dumpobj_checkptr;
		vps_func->vps_libdump_objtype2str =
		    vps_libdump_objtype2str;
		vps_func->vps_libdump_checkheader =
		    vps_libdump_checkheader;
		vps_func->vps_libdump_printheader =
		    vps_libdump_printheader;
		break;
	case MOD_UNLOAD:
		vps_func->vps_dumpobj_create = NULL;
		vps_func->vps_dumpobj_space = NULL;
		vps_func->vps_dumpobj_append = NULL;
		vps_func->vps_dumpobj_close = NULL;
		vps_func->vps_dumpobj_discard = NULL;
		vps_func->vps_dumpobj_checkobj = NULL;
		vps_func->vps_dumpobj_setcur = NULL;
		vps_func->vps_dumpobj_next = NULL;
		vps_func->vps_dumpobj_prev = NULL;
		vps_func->vps_dumpobj_peek = NULL;
		vps_func->vps_dumpobj_getcur = NULL;
		vps_func->vps_dumpobj_typeofnext = NULL;
		vps_func->vps_dumpobj_nextischild = NULL;
		vps_func->vps_dumpobj_recurse = NULL;
		vps_func->vps_dumpobj_makerelative = NULL;
		vps_func->vps_dumpobj_makeabsolute = NULL;
		vps_func->vps_dumpobj_printtree = NULL;
		vps_func->vps_dumpobj_checktree = NULL;
		vps_func->vps_dumpobj_checkptr = NULL;
		vps_func->vps_libdump_objtype2str = NULL;
		vps_func->vps_libdump_checkheader = NULL;
		vps_func->vps_libdump_printheader = NULL;
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}

	return (error);
}

static moduledata_t vps_libdump_mod = {
	"vps_libdump",
	vps_libdump_modevent,
	0
};

DECLARE_MODULE(vps_libdump, vps_libdump_mod, SI_SUB_PSEUDO, SI_ORDER_ANY);

#endif /* _KERNEL */

#endif /* VPS */

/* EOF */
