#ifndef __drbr_h__
#define __drbr_h__
#include <sys/param.h>
#ifdef _KERNEL
#include <sys/systm.h>
#include <sys/buf_ring.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/pcpu.h>
#include <sys/smp.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <machine/smp.h>
#include <machine/bus.h>
#include <machine/resource.h>
#endif
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_types.h>
#include <net/if_var.h>
#include <netinet/in.h>

#define DRBR_MAXQ_DEFAULT 8
#define DRBR_MIN_DEPTH 64	/* Must be power of 2 */

#define USE_LOCK

#ifdef _KERNEL
extern uint32_t drbr_maxq;
#endif

struct drbr_ring_entry {
	struct buf_ring		*re_qs;		/* Ring itself */
	u_long			re_drop_cnt;	/* Drop count in pkts */
	u_long			re_bytedrop_cnt;/* Drop count in bytes */
	u_long			re_cnt_sent;	/* Total sent in pkts */
	u_long			re_bytecnt_sent;/* Total sent in bytes */
	uint32_t		re_cnt;		/* Count on ring */
};

#define DRBR_LOCK_INIT(rng) mtx_init(&(rng)->rng_mtx, "drbr_lock", "drbr", MTX_DEF | MTX_DUPOK)
#define DRBR_LOCK_DESTROY(rng) 	mtx_destroy(&(rng)->rng_mtx)
#define DRBR_LOCK(rng) 	mtx_lock(&(rng)->rng_mtx)
#define DRBR_UNLOCK(rng) mtx_unlock(&(rng)->rng_mtx)
#define DRBR_LOCK_OWNED(rng) mtx_owned(&(rng)->rng_mtx)

struct drbr_ring {
#ifdef _KERNEL
	struct mtx 		rng_mtx;
#endif
	struct drbr_ring_entry *re;
	uint32_t		count_on_queues;
	uint32_t		lowq_with_data;
};

#ifdef _KERNEL
struct drbr_ring *
drbr_alloc(struct malloc_type *type, int flags, struct mtx *tmtx);
int drbr_enqueue(struct ifnet *ifp, struct drbr_ring *rng, struct mbuf *m);
void drbr_putback(struct ifnet *ifp, struct drbr_ring *rng, struct mbuf *new, 
	uint8_t qused);
struct mbuf *drbr_peek(struct ifnet *ifp, struct drbr_ring *rng,
	uint8_t *qused);
void drbr_flush(struct ifnet *ifp, struct drbr_ring *rng);
void drbr_free(struct drbr_ring *rng, struct malloc_type *type);
struct mbuf *drbr_dequeue(struct ifnet *ifp, struct drbr_ring *rng);
void drbr_advance(struct ifnet *ifp, struct drbr_ring *rng, uint8_t qused);
struct mbuf *
drbr_dequeue_cond(struct ifnet *ifp, struct drbr_ring *rng,
	int (*func) (struct mbuf *, void *), void *arg) ;
int drbr_empty(struct ifnet *ifp, struct drbr_ring *rng);
int drbr_needs_enqueue(struct ifnet *ifp, struct drbr_ring *rng);
int drbr_inuse(struct ifnet *ifp, struct drbr_ring *rng);
void drbr_add_sysctl_stats(device_t dev, struct sysctl_oid_list *queue_list, 
      struct drbr_ring *rng);
void 
drbr_add_sysctl_stats_nodev(struct sysctl_oid_list *queue_list, 
      struct sysctl_ctx_list *ctx,
      struct drbr_ring *rng);

int drbr_is_on_ring(struct drbr_ring *rng, struct mbuf *m);
u_long drbr_get_dropcnt(struct drbr_ring *rng);

#endif

#endif
#ifndef __drbr_h__
#define __drbr_h__
#include <sys/param.h>
#ifdef _KERNEL
#include <sys/systm.h>
#include <sys/buf_ring.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/pcpu.h>
#include <sys/smp.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <machine/smp.h>
#include <machine/bus.h>
#include <machine/resource.h>
#endif
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_types.h>
#include <net/if_var.h>
#include <netinet/in.h>

#define DRBR_MAXQ_DEFAULT 8
#define DRBR_MIN_DEPTH 64	/* Must be power of 2 */

#define USE_LOCK

#ifdef _KERNEL
extern uint32_t drbr_maxq;
#endif

struct drbr_ring_entry {
	struct buf_ring		*re_qs;		/* Ring itself */
	u_long			re_drop_cnt;	/* Drop count in pkts */
	u_long			re_bytedrop_cnt;/* Drop count in bytes */
	u_long			re_cnt_sent;	/* Total sent in pkts */
	u_long			re_bytecnt_sent;/* Total sent in bytes */
	uint32_t		re_cnt;		/* Count on ring */
};

#define DRBR_LOCK_INIT(rng) mtx_init(&(rng)->rng_mtx, "drbr_lock", "drbr", MTX_DEF | MTX_DUPOK)
#define DRBR_LOCK_DESTROY(rng) 	mtx_destroy(&(rng)->rng_mtx)
#define DRBR_LOCK(rng) 	mtx_lock(&(rng)->rng_mtx)
#define DRBR_UNLOCK(rng) mtx_unlock(&(rng)->rng_mtx)
#define DRBR_LOCK_OWNED(rng) mtx_owned(&(rng)->rng_mtx)

struct drbr_ring {
#ifdef _KERNEL
	struct mtx 		rng_mtx;
#endif
	struct drbr_ring_entry *re;
	uint32_t		count_on_queues;
	uint32_t		lowq_with_data;
};

#ifdef _KERNEL
struct drbr_ring *
drbr_alloc(struct malloc_type *type, int flags, struct mtx *tmtx);
int drbr_enqueue(struct ifnet *ifp, struct drbr_ring *rng, struct mbuf *m);
void drbr_putback(struct ifnet *ifp, struct drbr_ring *rng, struct mbuf *new, 
	uint8_t qused);
struct mbuf *drbr_peek(struct ifnet *ifp, struct drbr_ring *rng,
	uint8_t *qused);
void drbr_flush(struct ifnet *ifp, struct drbr_ring *rng);
void drbr_free(struct drbr_ring *rng, struct malloc_type *type);
struct mbuf *drbr_dequeue(struct ifnet *ifp, struct drbr_ring *rng);
void drbr_advance(struct ifnet *ifp, struct drbr_ring *rng, uint8_t qused);
struct mbuf *
drbr_dequeue_cond(struct ifnet *ifp, struct drbr_ring *rng,
	int (*func) (struct mbuf *, void *), void *arg) ;
int drbr_empty(struct ifnet *ifp, struct drbr_ring *rng);
int drbr_needs_enqueue(struct ifnet *ifp, struct drbr_ring *rng);
int drbr_inuse(struct ifnet *ifp, struct drbr_ring *rng);
void drbr_add_sysctl_stats(device_t dev, struct sysctl_oid_list *queue_list, 
      struct drbr_ring *rng);
void 
drbr_add_sysctl_stats_nodev(struct sysctl_oid_list *queue_list, 
      struct sysctl_ctx_list *ctx,
      struct drbr_ring *rng);

int drbr_is_on_ring(struct drbr_ring *rng, struct mbuf *m);
u_long drbr_get_dropcnt(struct drbr_ring *rng);

#endif

#endif
