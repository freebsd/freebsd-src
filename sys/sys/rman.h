/*
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
 * $FreeBSD$
 */

#ifndef _SYS_RMAN_H_
#define	_SYS_RMAN_H_	1

#ifndef	_KERNEL
#include <sys/queue.h>
#endif

/*
 * We use a linked list rather than a bitmap because we need to be able to
 * represent potentially huge objects (like all of a processor's physical
 * address space).  That is also why the indices are defined to have type
 * `unsigned long' -- that being the largest integral type in Standard C.
 */
CIRCLEQ_HEAD(resource_head, resource);
struct	resource {
	CIRCLEQ_ENTRY(resource)	r_link;
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
};

#define	RF_ALLOCATED	0x0001	/* resource has been reserved */
#define	RF_ACTIVE	0x0002	/* resource allocation has been activated */
#define	RF_SHAREABLE	0x0004	/* resource permits contemporaneous sharing */
#define	RF_TIMESHARE	0x0008	/* resource permits time-division sharing */
#define	RF_WANTED	0x0010	/* somebody is waiting for this resource */
#define	RF_FIRSTSHARE	0x0020	/* first in sharing list */

#define	RF_ALIGNMENT_SHIFT	10 /* alignment size bit starts bit 10 */
#define	RF_ALIGNMENT_MASK	(0x003F << RF_ALIGNMENT_SHIFT)
				/* resource address alignemnt size bit mask */
#define	RF_ALIGNMENT_LOG2(x)	((x) << RF_ALIGNMENT_SHIFT)
#define	RF_ALIGNMENT(x)		(((x) & RF_ALIGNMENT_MASK) >> RF_ALIGNMENT_SHIFT)

enum	rman_type { RMAN_UNINIT = 0, RMAN_GAUGE, RMAN_ARRAY };

struct	rman {
	struct	resource_head 	rm_list;
	struct	simplelock *rm_slock; /* mutex used to protect rm_list */
	TAILQ_ENTRY(rman)	rm_link; /* link in list of all rmans */
	u_long	rm_start;	/* index of globally first entry */
	u_long	rm_end;		/* index of globally last entry */
	enum	rman_type rm_type; /* what type of resource this is */
	const	char *rm_descr;	/* text descripion of this resource */
};
TAILQ_HEAD(rman_head, rman);

#ifdef _KERNEL

int	rman_activate_resource(struct resource *r);
int	rman_await_resource(struct resource *r, int pri, int timo);
int	rman_deactivate_resource(struct resource *r);
int	rman_fini(struct rman *rm);
int	rman_init(struct rman *rm);
int	rman_manage_region(struct rman *rm, u_long start, u_long end);
int	rman_release_resource(struct resource *r);
struct	resource *rman_reserve_resource(struct rman *rm, u_long start,
					u_long end, u_long count,
					u_int flags, struct device *dev);
uint32_t rman_make_alignment_flags(uint32_t size);

#define rman_get_start(r)	((r)->r_start)
#define rman_get_end(r)		((r)->r_end)
#define rman_get_flags(r)	((r)->r_flags)
#define	rman_set_virtual(r,v)	((r)->r_virtual = (v))
#define	rman_get_virtual(r)	((r)->r_virtual)
#define rman_set_bustag(r,t)	((r)->r_bustag = (t))
#define rman_get_bustag(r)	((r)->r_bustag)
#define rman_set_bushandle(r,h)	((r)->r_bushandle = (h))
#define rman_get_bushandle(r)	((r)->r_bushandle)

extern	struct rman_head rman_head;
#endif /* _KERNEL */

#endif /* !_SYS_RMAN_H_ */
