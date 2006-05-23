/*-
 * Copyright 1998 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  M.I.T. makes
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 * 
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/sys/rman.h,v 1.22.2.1 2005/01/31 23:26:57 imp Exp $
 */

#ifndef _SYS_RMAN_H_
#define	_SYS_RMAN_H_	1

#ifndef	_KERNEL
#include <sys/queue.h>
#else
#include <machine/bus.h>
#include <machine/resource.h>
#endif

#define	RF_ALLOCATED	0x0001	/* resource has been reserved */
#define	RF_ACTIVE	0x0002	/* resource allocation has been activated */
#define	RF_SHAREABLE	0x0004	/* resource permits contemporaneous sharing */
#define	RF_TIMESHARE	0x0008	/* resource permits time-division sharing */
#define	RF_WANTED	0x0010	/* somebody is waiting for this resource */
#define	RF_FIRSTSHARE	0x0020	/* first in sharing list */
#define	RF_PREFETCHABLE	0x0040	/* resource is prefetchable */

#define	RF_ALIGNMENT_SHIFT	10 /* alignment size bit starts bit 10 */
#define	RF_ALIGNMENT_MASK	(0x003F << RF_ALIGNMENT_SHIFT)
				/* resource address alignemnt size bit mask */
#define	RF_ALIGNMENT_LOG2(x)	((x) << RF_ALIGNMENT_SHIFT)
#define	RF_ALIGNMENT(x)		(((x) & RF_ALIGNMENT_MASK) >> RF_ALIGNMENT_SHIFT)

enum	rman_type { RMAN_UNINIT = 0, RMAN_GAUGE, RMAN_ARRAY };

/*
 * String length exported to userspace for resource names, etc.
 */
#define RM_TEXTLEN	32

/*
 * Userspace-exported structures.
 */
struct u_resource {
	uintptr_t	r_handle;		/* resource uniquifier */
	uintptr_t	r_parent;		/* parent rman */
	uintptr_t	r_device;		/* device owning this resource */
	char		r_devname[RM_TEXTLEN];	/* device name XXX obsolete */

	u_long		r_start;		/* offset in resource space */
	u_long		r_size;			/* size in resource space */
	u_int		r_flags;		/* RF_* flags */
};

struct u_rman {
	uintptr_t	rm_handle;		/* rman uniquifier */
	char		rm_descr[RM_TEXTLEN];	/* rman description */

	u_long		rm_start;		/* base of managed region */
	u_long		rm_size;		/* size of managed region */
	enum rman_type	rm_type;		/* region type */
};

#ifdef _KERNEL
/*
 * We use a linked list rather than a bitmap because we need to be able to
 * represent potentially huge objects (like all of a processor's physical
 * address space).  That is also why the indices are defined to have type
 * `unsigned long' -- that being the largest integral type in ISO C (1990).
 * The 1999 version of C allows `long long'; we may need to switch to that
 * at some point in the future, particularly if we want to support 36-bit
 * addresses on IA32 hardware.
 */
TAILQ_HEAD(resource_head, resource);
#ifdef __RMAN_RESOURCE_VISIBLE
struct resource {
	TAILQ_ENTRY(resource)	r_link;
	LIST_ENTRY(resource)	r_sharelink;
	LIST_HEAD(, resource) 	*r_sharehead;
	u_long	r_start;	/* index of the first entry in this resource */
	u_long	r_end;		/* index of the last entry (inclusive) */
	u_int	r_flags;
	void	*r_virtual;	/* virtual address of this resource */
	bus_space_tag_t r_bustag; /* bus_space tag */
	bus_space_handle_t r_bushandle;	/* bus_space handle */
	struct	device *r_dev;	/* device which has allocated this resource */
	struct	rman *r_rm;	/* resource manager from whence this came */
	int	r_rid;		/* optional rid for this resource. */
};
#else
struct resource;
struct device;
#endif

struct rman {
	struct	resource_head 	rm_list;
	struct	mtx *rm_mtx;	/* mutex used to protect rm_list */
	TAILQ_ENTRY(rman)	rm_link; /* link in list of all rmans */
	u_long	rm_start;	/* index of globally first entry */
	u_long	rm_end;		/* index of globally last entry */
	enum	rman_type rm_type; /* what type of resource this is */
	const	char *rm_descr;	/* text descripion of this resource */
};
TAILQ_HEAD(rman_head, rman);

int	rman_activate_resource(struct resource *r);
int	rman_await_resource(struct resource *r, int pri, int timo);
int	rman_deactivate_resource(struct resource *r);
int	rman_fini(struct rman *rm);
int	rman_init(struct rman *rm);
int	rman_manage_region(struct rman *rm, u_long start, u_long end);
int	rman_release_resource(struct resource *r);
struct resource *rman_reserve_resource(struct rman *rm, u_long start,
					u_long end, u_long count,
					u_int flags, struct device *dev);
struct resource *rman_reserve_resource_bound(struct rman *rm, u_long start,
					u_long end, u_long count, u_long bound,
					u_int flags, struct device *dev);
uint32_t rman_make_alignment_flags(uint32_t size);

u_long	rman_get_start(struct resource *_r);
u_long	rman_get_end(struct resource *_r);
struct device *rman_get_device(struct resource *);
u_long	rman_get_size(struct resource *_r);
u_int	rman_get_flags(struct resource *_r);
void	rman_set_virtual(struct resource *_r, void *_v);
void   *rman_get_virtual(struct resource *_r);
void	rman_set_bustag(struct resource *_r, bus_space_tag_t _t);
bus_space_tag_t rman_get_bustag(struct resource *_r);
void	rman_set_bushandle(struct resource *_r, bus_space_handle_t _h);
bus_space_handle_t rman_get_bushandle(struct resource *_r);
void	rman_set_rid(struct resource *_r, int _rid);
int	rman_get_rid(struct resource *_r);
void	rman_set_start(struct resource *_r, u_long _start);
void	rman_set_end(struct resource *_r, u_long _end);

extern	struct rman_head rman_head;
#endif /* _KERNEL */

#endif /* !_SYS_RMAN_H_ */
