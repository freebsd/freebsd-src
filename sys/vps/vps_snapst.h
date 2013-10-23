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

/* $Id: vps_snapst.h 162 2013-06-06 18:17:55Z klaus $ */

#ifndef _VPS_SNAPST_H
#define _VPS_SNAPST_H

#ifdef VPS

#include <sys/cdefs.h>

/* XXX for vps_printf() */
#include <machine/stdarg.h>

extern struct vm_object *shared_page_obj;

MALLOC_DECLARE(M_VPS_CORE);

struct vps_snapst_ctx;
struct vps_arg_snapst;
struct vps_dev_ctx;

int getsock(struct filedesc *fdp, int fd, struct file **fpp, u_int *fflagp);
void fdgrowtable(struct filedesc *fdp, int nfd);
void fdused(struct filedesc *fdp, int fd);
void fdunused(struct filedesc *fdp, int fd);
int vn_fullpath1(struct thread *td, struct vnode *vp, struct vnode *rdir,
    char *buf, char **retbuf, u_int buflen);
void vnet_route_init(const void *);
void vnet_route_uninit(const void *);

static int vps_ctx_extend(struct vps_snapst_ctx *ctx, struct vps *vps,
    size_t size, int how);

int vps_snapshot(struct vps_dev_ctx *ctx, struct vps *vps,
    struct vps_arg_snapst *va);
int vps_snapshot_getpage(struct vps_dev_ctx *ctx, vm_offset_t offset,
    vm_paddr_t *paddr);
int vps_snapshot_finish(struct vps_dev_ctx *ctx, struct vps *vps);

int vps_restore(struct vps_dev_ctx *ctx, struct vps_arg_snapst *va);

SLIST_HEAD(_vps_dumpobj_list, vps_dumpobj);

struct vm_object;
struct vps_page_ref;
struct vps_dumpinfo;
struct vps_snapst_errmsg;

struct vps_snapst_ctx {
	struct vps_snapst_ctx *dev;	/* Owner */
	struct vps *vps;		/* Working on this instance */
	struct ucred *vps_ucred;	/* Ucred for various restore
					   functions */
	int cmd;			/* Last/Pending ioctl */
	void *data;			/* Base of private data */
	size_t dsize;			/* Used size of private data
					   in bytes */
	size_t dsize2;			/* Allocated size of private
					   data in bytes */
	size_t maxsize;			/* Maximum size in bytes */
	void *cpos;			/* Current position */
	int pagesread;			/* Count of pages requested */
	int nsyspages;			/* Count of pages from
					   kernel memory */
	int nuserpages;			/* Count of pages from
					   userspace memory */
	int userpagelistlength;
	/* dump */
	struct vm_object *vmobj;	/* VM Object for dump */
	struct vm_page **userpagelist;	/* Array of pointers to pages */
	struct vm_page **syspagelist;	/* Array of pointers to pages */
	/* restore */
	int userpagesidx;		/* ... */
	caddr_t userpagesaddr;		/* ... */
	SLIST_HEAD(_vps_restore_obj_list, vps_restore_obj) obj_list;
	struct _vps_dumpobj_list *dumpobj_list;

	struct vm_map *user_map;
	vm_offset_t user_map_start;
	vm_offset_t user_map_offset;

	size_t page_ref_size;
	struct vps_page_ref *page_ref;

	int pager_npages_res;
	int pager_npages_swap;
	int pager_npages_miss;

	struct vps_dumpheader *dumphdr;

	struct vm_object *vps_vmobject;

	struct vps_dumpobj *rootobj;
	struct vps_dumpobj *lastobj;
	struct vps_dumpobj *curobj;
	int level;
	int elements;
	char relative;

	int extend_failcount;

	struct vps_dump_sysinfo *old_sysinfo;

	LIST_HEAD(_vps_snapst_errmsg_list, vps_snapst_errmsg) errormsgs;
};

struct vps_dumpobj;

struct vps_page_ref {
	struct vm_object *obj;
	vm_pindex_t pidx;
	enum { UNKNOWN = 0, RESIDENT, PAGER } origin;
};

struct vps_snapst_errmsg {
	LIST_ENTRY (vps_snapst_errmsg) list;
	char str[0];
};

static int vps_snapst_print_errormsgs;

/* inlining not possible */
__attribute__((unused))
static void
vps_snapst_pusherrormsg(struct vps_snapst_ctx *ctx, char *fmt, ...)
{
	struct vps_snapst_errmsg *entry;
	va_list ap;
	char *buf;

	buf = malloc(0x1000, M_TEMP, M_WAITOK);

	va_start(ap, fmt);
	vsnprintf(buf, 0x1000, fmt, ap);
	va_end(ap);

	if (vps_snapst_print_errormsgs != 0)
		printf("%s", buf);

	entry = malloc(sizeof(*entry) + strlen(buf) + 1, M_TEMP, M_WAITOK);
	memcpy(entry->str, buf, strlen(buf) + 1);

	LIST_INSERT_HEAD(&ctx->errormsgs, entry, list);

	free(buf, M_TEMP);
}

#if defined(_VPS_SNAPST_H_ALL) || defined(_VPS_SNAPST_H_RESTORE_OBJ)
struct vps_restore_obj {
	SLIST_ENTRY (vps_restore_obj) list;
	int type;
	struct vps_dumpobj *dumpobj;
	void * orig_ptr;
	void * new_ptr;
	int orig_id;
	void * spare[4];
};
#endif

#ifdef _VPS_SNAPST_H_ALL

extern uma_zone_t vmspace_zone;
//extern struct fileops pipeops;

#if 0
static
void nullprintf(char *fmt, ...)
{
}
#define printf nullprintf
#endif

/* XXXXXXXXXXXXXXXX duplicated file scope declarations XXXXXXXXXXXXXXX */

/* XXX from net/if_epair.c */
struct epair_softc {
        struct ifnet    *ifp;
        struct ifnet    *oifp;
        u_int           refcount;
        void            (*if_qflush)(struct ifnet *);
};

/* kern/tty_pts.c */

/*
 * Per-PTS structure.
 *
 * List of locks
 * (t)  locked by tty_lock()
 * (c)  const until freeing
 */
struct pts_softc {
        int             pts_unit;       /* (c) Device unit number. */
        unsigned int    pts_flags;      /* (t) Device flags. */
#define PTS_PKT         0x1     /* Packet mode. */
#define PTS_FINISHED    0x2     /* Return errors on read()/write(). */
        char            pts_pkt;        /* (t) Unread packet mode data. */

        struct cv       pts_inwait;     /* (t) Blocking write() on master. */
        struct selinfo  pts_inpoll;     /* (t) Select queue for write(). */
        struct cv       pts_outwait;    /* (t) Blocking read() on master. */
        struct selinfo  pts_outpoll;    /* (t) Select queue for read(). */

#ifdef PTS_EXTERNAL
        struct cdev     *pts_cdev;      /* (c) Master device node. */
#endif /* PTS_EXTERNAL */

        struct uidinfo  *pts_uidinfo;   /* (c) Resource limit. */
#ifdef VPS
        struct ucred    *pts_cred;      /* (c) */
#endif
};

#endif /* _VPS_SNAPST_H_ALL */

#define VPS_SYSENTVEC_NULL	0x0
#define VPS_SYSENTVEC_ELF32	0x1
#define VPS_SYSENTVEC_ELF64	0x2

/*
 * * * * * Support functions. * * * *
 */
VPSFUNC
__attribute__((unused))
static u_int
vps_cksum(char *ptr, int size)
{
        u_int i, sum;

        sum = 0;
        for (i = 0; i < size; i++)
                sum += (u_char)ptr[i];
        return (sum);
}


/* BEGIN DEBUG */

#define DBG_CTX_POS(t)					\
do {							\
	printf("%s: ctx->cpos=%p rel=%p (%s)\n",	\
		__func__, ctx->cpos,			\
		(void *)(ctx->cpos - ctx->data), (t));	\
} while (0)

__attribute__((unused))
static int
vps_printf(const char *fmt, ...)
{
        va_list ap;
        int retval;

	printf("%lld.%d ", (long long int)time_second, ticks);

        va_start(ap, fmt);
        retval = vprintf(fmt, ap);
        va_end(ap);

        return (retval);
}

__attribute__((unused))
static void
vps_print_ascii(const char *data, int len)
{
        const char *p;
        char line[61];
        int j;

        p = data;
        j = 0;
	memset(line, 0, 61);

        while (p < data + len) {
                if (*p > 0x20)
                        line[j] = *p;
                else
                        line[j] = '.';
                j++;
                p++;
                if (j > 60) {
                        printf("[%s]\n", line);
			memset(line, 0, 61);
                        j = 0;
                }
        }
        if (j > 0)
                printf("[%s]\n", line);
}

/* END DEBUG */

static inline
int
vps_ctx_extend(struct vps_snapst_ctx *ctx, struct vps *vps, size_t size,
    int how)
{
	int error;

	if (vps_func->vps_ctx_extend_hard == NULL)
		return (ENOSYS);

	if (how == M_WAITOK)
		WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL,
		    "%s: size=%d", __func__, size);

	/*
	 * On M_WAITOK allocations we always keep at least one page
	 * room to cover M_NOWAIT calls (usually tiny sizes).
	 */
	if ((caddr_t)ctx->data + ctx->dsize2 > (caddr_t)ctx->cpos + size +
	    (how==M_WAITOK ? PAGE_SIZE : 0)) {
		/*
		DBGS("%s: ctx->data + ctx->dsize2 = %08x     "
		    "ctx->cpos + size = %08x\n",
		    __func__, (u_int)(ctx->data + ctx->dsize2),
		    (u_int)(ctx->cpos + size));
                */
		/* Nothing to do. */
		return (0);

	} else {

		/* Actually allocate space. */
		error = vps_func->vps_ctx_extend_hard(ctx, vps, size, how);

		if (error != 0) {
			if (ctx->extend_failcount++ > 10)
				panic("%s: ctx=%p failcount=%d\n",
				    __func__, ctx, ctx->extend_failcount);
		} else {
			ctx->extend_failcount = 0;
		}

		return (error);
	}
}


#endif /* VPS */

#endif /* _VPS_SNAPST_H */

/* EOF */
