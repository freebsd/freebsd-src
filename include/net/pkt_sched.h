#ifndef __NET_PKT_SCHED_H
#define __NET_PKT_SCHED_H

#define PSCHED_GETTIMEOFDAY	1
#define PSCHED_JIFFIES 		2
#define PSCHED_CPU 		3

#define PSCHED_CLOCK_SOURCE	PSCHED_JIFFIES

#include <linux/config.h>
#include <linux/types.h>
#include <linux/pkt_sched.h>
#include <net/pkt_cls.h>

#ifdef CONFIG_X86_TSC
#include <asm/msr.h>
#endif

struct rtattr;
struct Qdisc;

struct qdisc_walker
{
	int	stop;
	int	skip;
	int	count;
	int	(*fn)(struct Qdisc *, unsigned long cl, struct qdisc_walker *);
};

struct Qdisc_class_ops
{
	/* Child qdisc manipulation */
	int			(*graft)(struct Qdisc *, unsigned long cl, struct Qdisc *, struct Qdisc **);
	struct Qdisc *		(*leaf)(struct Qdisc *, unsigned long cl);

	/* Class manipulation routines */
	unsigned long		(*get)(struct Qdisc *, u32 classid);
	void			(*put)(struct Qdisc *, unsigned long);
	int			(*change)(struct Qdisc *, u32, u32, struct rtattr **, unsigned long *);
	int			(*delete)(struct Qdisc *, unsigned long);
	void			(*walk)(struct Qdisc *, struct qdisc_walker * arg);

	/* Filter manipulation */
	struct tcf_proto **	(*tcf_chain)(struct Qdisc *, unsigned long);
	unsigned long		(*bind_tcf)(struct Qdisc *, unsigned long, u32 classid);
	void			(*unbind_tcf)(struct Qdisc *, unsigned long);

	/* rtnetlink specific */
	int			(*dump)(struct Qdisc *, unsigned long, struct sk_buff *skb, struct tcmsg*);
};

struct Qdisc_ops
{
	struct Qdisc_ops	*next;
	struct Qdisc_class_ops	*cl_ops;
	char			id[IFNAMSIZ];
	int			priv_size;

	int 			(*enqueue)(struct sk_buff *, struct Qdisc *);
	struct sk_buff *	(*dequeue)(struct Qdisc *);
	int 			(*requeue)(struct sk_buff *, struct Qdisc *);
	unsigned int		(*drop)(struct Qdisc *);

	int			(*init)(struct Qdisc *, struct rtattr *arg);
	void			(*reset)(struct Qdisc *);
	void			(*destroy)(struct Qdisc *);
	int			(*change)(struct Qdisc *, struct rtattr *arg);

	int			(*dump)(struct Qdisc *, struct sk_buff *);
};

extern rwlock_t qdisc_tree_lock;

struct Qdisc
{
	int 			(*enqueue)(struct sk_buff *skb, struct Qdisc *dev);
	struct sk_buff *	(*dequeue)(struct Qdisc *dev);
	unsigned		flags;
#define TCQ_F_BUILTIN	1
#define TCQ_F_THROTTLED	2
#define TCQ_F_INGRES	4
	struct Qdisc_ops	*ops;
	struct Qdisc		*next;
	u32			handle;
	atomic_t		refcnt;
	struct sk_buff_head	q;
	struct net_device	*dev;

	struct tc_stats		stats;
	int			(*reshape_fail)(struct sk_buff *skb, struct Qdisc *q);

	/* This field is deprecated, but it is still used by CBQ
	 * and it will live until better solution will be invented.
	 */
	struct Qdisc		*__parent;

	char			data[0];
};

struct qdisc_rate_table
{
	struct tc_ratespec rate;
	u32		data[256];
	struct qdisc_rate_table *next;
	int		refcnt;
};

static inline void sch_tree_lock(struct Qdisc *q)
{
	write_lock(&qdisc_tree_lock);
	spin_lock_bh(&q->dev->queue_lock);
}

static inline void sch_tree_unlock(struct Qdisc *q)
{
	spin_unlock_bh(&q->dev->queue_lock);
	write_unlock(&qdisc_tree_lock);
}

static inline void tcf_tree_lock(struct tcf_proto *tp)
{
	write_lock(&qdisc_tree_lock);
	spin_lock_bh(&tp->q->dev->queue_lock);
}

static inline void tcf_tree_unlock(struct tcf_proto *tp)
{
	spin_unlock_bh(&tp->q->dev->queue_lock);
	write_unlock(&qdisc_tree_lock);
}


static inline unsigned long
cls_set_class(struct tcf_proto *tp, unsigned long *clp, unsigned long cl)
{
	unsigned long old_cl;

	tcf_tree_lock(tp);
	old_cl = *clp;
	*clp = cl;
	tcf_tree_unlock(tp);
	return old_cl;
}

static inline unsigned long
__cls_set_class(unsigned long *clp, unsigned long cl)
{
	unsigned long old_cl;

	old_cl = *clp;
	*clp = cl;
	return old_cl;
}


/* 
   Timer resolution MUST BE < 10% of min_schedulable_packet_size/bandwidth
   
   Normal IP packet size ~ 512byte, hence:

   0.5Kbyte/1Mbyte/sec = 0.5msec, so that we need 50usec timer for
   10Mbit ethernet.

   10msec resolution -> <50Kbit/sec.
   
   The result: [34]86 is not good choice for QoS router :-(

   The things are not so bad, because we may use artifical
   clock evaluated by integration of network data flow
   in the most critical places.

   Note: we do not use fastgettimeofday.
   The reason is that, when it is not the same thing as
   gettimeofday, it returns invalid timestamp, which is
   not updated, when net_bh is active.

   So, use PSCHED_CLOCK_SOURCE = PSCHED_CPU on alpha and pentiums
   with rtdsc. And PSCHED_JIFFIES on all other architectures, including [34]86
   and pentiums without rtdsc.
   You can use PSCHED_GETTIMEOFDAY on another architectures,
   which have fast and precise clock source, but it is too expensive.
 */

/* General note about internal clock.

   Any clock source returns time intervals, measured in units
   close to 1usec. With source PSCHED_GETTIMEOFDAY it is precisely
   microseconds, otherwise something close but different chosen to minimize
   arithmetic cost. Ratio usec/internal untis in form nominator/denominator
   may be read from /proc/net/psched.
 */


#if PSCHED_CLOCK_SOURCE == PSCHED_GETTIMEOFDAY

typedef struct timeval	psched_time_t;
typedef long		psched_tdiff_t;

#define PSCHED_GET_TIME(stamp) do_gettimeofday(&(stamp))
#define PSCHED_US2JIFFIE(usecs) (((usecs)+(1000000/HZ-1))/(1000000/HZ))
#define PSCHED_JIFFIE2US(delay) ((delay)*(1000000/HZ))

#define PSCHED_EXPORTLIST EXPORT_SYMBOL(psched_tod_diff);

#else /* PSCHED_CLOCK_SOURCE != PSCHED_GETTIMEOFDAY */

#define PSCHED_EXPORTLIST PSCHED_EXPORTLIST_1 PSCHED_EXPORTLIST_2

typedef u64	psched_time_t;
typedef long	psched_tdiff_t;

extern psched_time_t	psched_time_base;

#if PSCHED_CLOCK_SOURCE == PSCHED_JIFFIES

#if HZ < 96
#define PSCHED_JSCALE 14
#elif HZ >= 96 && HZ < 192
#define PSCHED_JSCALE 13
#elif HZ >= 192 && HZ < 384
#define PSCHED_JSCALE 12
#elif HZ >= 384 && HZ < 768
#define PSCHED_JSCALE 11
#elif HZ >= 768
#define PSCHED_JSCALE 10
#endif

#define PSCHED_EXPORTLIST_2

#if BITS_PER_LONG <= 32

#define PSCHED_WATCHER unsigned long

extern PSCHED_WATCHER psched_time_mark;

#define PSCHED_GET_TIME(stamp) ((stamp) = psched_time_base + (((unsigned long)(jiffies-psched_time_mark))<<PSCHED_JSCALE))

#define PSCHED_EXPORTLIST_1 EXPORT_SYMBOL(psched_time_base); \
                            EXPORT_SYMBOL(psched_time_mark);

#else

#define PSCHED_GET_TIME(stamp) ((stamp) = (jiffies<<PSCHED_JSCALE))

#define PSCHED_EXPORTLIST_1 

#endif

#define PSCHED_US2JIFFIE(delay) (((delay)+(1<<PSCHED_JSCALE)-1)>>PSCHED_JSCALE)
#define PSCHED_JIFFIE2US(delay) ((delay)<<PSCHED_JSCALE)

#elif PSCHED_CLOCK_SOURCE == PSCHED_CPU

extern psched_tdiff_t psched_clock_per_hz;
extern int psched_clock_scale;

#define PSCHED_EXPORTLIST_2 EXPORT_SYMBOL(psched_clock_per_hz); \
                            EXPORT_SYMBOL(psched_clock_scale);

#define PSCHED_US2JIFFIE(delay) (((delay)+psched_clock_per_hz-1)/psched_clock_per_hz)
#define PSCHED_JIFFIE2US(delay) ((delay)*psched_clock_per_hz)

#ifdef CONFIG_X86_TSC

#define PSCHED_GET_TIME(stamp) \
({ u64 __cur; \
   rdtscll(__cur); \
   (stamp) = __cur>>psched_clock_scale; \
})

#define PSCHED_EXPORTLIST_1

#elif defined (__alpha__)

#define PSCHED_WATCHER u32

extern PSCHED_WATCHER psched_time_mark;

#define PSCHED_GET_TIME(stamp) \
({ u32 __res; \
   __asm__ __volatile__ ("rpcc %0" : "r="(__res)); \
   if (__res <= psched_time_mark) psched_time_base += 0x100000000UL; \
   psched_time_mark = __res; \
   (stamp) = (psched_time_base + __res)>>psched_clock_scale; \
})

#define PSCHED_EXPORTLIST_1 EXPORT_SYMBOL(psched_time_base); \
                            EXPORT_SYMBOL(psched_time_mark);

#else

#error PSCHED_CLOCK_SOURCE=PSCHED_CPU is not supported on this arch.

#endif /* ARCH */

#endif /* PSCHED_CLOCK_SOURCE == PSCHED_JIFFIES */

#endif /* PSCHED_CLOCK_SOURCE == PSCHED_GETTIMEOFDAY */

#if PSCHED_CLOCK_SOURCE == PSCHED_GETTIMEOFDAY
#define PSCHED_TDIFF(tv1, tv2) \
({ \
	   int __delta_sec = (tv1).tv_sec - (tv2).tv_sec; \
	   int __delta = (tv1).tv_usec - (tv2).tv_usec; \
	   if (__delta_sec) { \
	           switch (__delta_sec) { \
		   default: \
			   __delta = 0; \
		   case 2: \
			   __delta += 1000000; \
		   case 1: \
			   __delta += 1000000; \
	           } \
	   } \
	   __delta; \
})

extern int psched_tod_diff(int delta_sec, int bound);

#define PSCHED_TDIFF_SAFE(tv1, tv2, bound, guard) \
({ \
	   int __delta_sec = (tv1).tv_sec - (tv2).tv_sec; \
	   int __delta = (tv1).tv_usec - (tv2).tv_usec; \
	   switch (__delta_sec) { \
	   default: \
		   __delta = psched_tod_diff(__delta_sec, bound); guard; break; \
	   case 2: \
		   __delta += 1000000; \
	   case 1: \
		   __delta += 1000000; \
	   case 0: ; \
	   } \
	   __delta; \
})

#define PSCHED_TLESS(tv1, tv2) (((tv1).tv_usec < (tv2).tv_usec && \
				(tv1).tv_sec <= (tv2).tv_sec) || \
				 (tv1).tv_sec < (tv2).tv_sec)

#define PSCHED_TADD2(tv, delta, tv_res) \
({ \
	   int __delta = (tv).tv_usec + (delta); \
	   (tv_res).tv_sec = (tv).tv_sec; \
	   if (__delta > 1000000) { (tv_res).tv_sec++; __delta -= 1000000; } \
	   (tv_res).tv_usec = __delta; \
})

#define PSCHED_TADD(tv, delta) \
({ \
	   (tv).tv_usec += (delta); \
	   if ((tv).tv_usec > 1000000) { (tv).tv_sec++; \
		 (tv).tv_usec -= 1000000; } \
})

/* Set/check that time is in the "past perfect";
   it depends on concrete representation of system time
 */

#define PSCHED_SET_PASTPERFECT(t)	((t).tv_sec = 0)
#define PSCHED_IS_PASTPERFECT(t)	((t).tv_sec == 0)

#define	PSCHED_AUDIT_TDIFF(t) ({ if ((t) > 2000000) (t) = 2000000; })

#else

#define PSCHED_TDIFF(tv1, tv2) (long)((tv1) - (tv2))
#define PSCHED_TDIFF_SAFE(tv1, tv2, bound, guard) \
({ \
	   long long __delta = (tv1) - (tv2); \
	   if ( __delta > (long long)(bound)) {  __delta = (bound); guard; } \
	   __delta; \
})


#define PSCHED_TLESS(tv1, tv2) ((tv1) < (tv2))
#define PSCHED_TADD2(tv, delta, tv_res) ((tv_res) = (tv) + (delta))
#define PSCHED_TADD(tv, delta) ((tv) += (delta))
#define PSCHED_SET_PASTPERFECT(t)	((t) = 0)
#define PSCHED_IS_PASTPERFECT(t)	((t) == 0)
#define	PSCHED_AUDIT_TDIFF(t)

#endif

struct tcf_police
{
	struct tcf_police *next;
	int		refcnt;
	u32		index;

	int		action;
	int		result;
	u32		ewma_rate;
	u32		burst;
	u32		mtu;

	u32		toks;
	u32		ptoks;
	psched_time_t	t_c;
	spinlock_t	lock;
	struct qdisc_rate_table *R_tab;
	struct qdisc_rate_table *P_tab;

	struct tc_stats	stats;
};

extern int qdisc_copy_stats(struct sk_buff *skb, struct tc_stats *st);
extern void tcf_police_destroy(struct tcf_police *p);
extern struct tcf_police * tcf_police_locate(struct rtattr *rta, struct rtattr *est);
extern int tcf_police_dump(struct sk_buff *skb, struct tcf_police *p);
extern int tcf_police(struct sk_buff *skb, struct tcf_police *p);

static inline void tcf_police_release(struct tcf_police *p)
{
	if (p && --p->refcnt == 0)
		tcf_police_destroy(p);
}

extern struct Qdisc noop_qdisc;
extern struct Qdisc_ops noop_qdisc_ops;
extern struct Qdisc_ops pfifo_qdisc_ops;
extern struct Qdisc_ops bfifo_qdisc_ops;

int register_qdisc(struct Qdisc_ops *qops);
int unregister_qdisc(struct Qdisc_ops *qops);
struct Qdisc *qdisc_lookup(struct net_device *dev, u32 handle);
struct Qdisc *qdisc_lookup_class(struct net_device *dev, u32 handle);
void dev_init_scheduler(struct net_device *dev);
void dev_shutdown(struct net_device *dev);
void dev_activate(struct net_device *dev);
void dev_deactivate(struct net_device *dev);
void qdisc_reset(struct Qdisc *qdisc);
void qdisc_destroy(struct Qdisc *qdisc);
struct Qdisc * qdisc_create_dflt(struct net_device *dev, struct Qdisc_ops *ops);
int qdisc_new_estimator(struct tc_stats *stats, struct rtattr *opt);
void qdisc_kill_estimator(struct tc_stats *stats);
struct qdisc_rate_table *qdisc_get_rtab(struct tc_ratespec *r, struct rtattr *tab);
void qdisc_put_rtab(struct qdisc_rate_table *tab);
int teql_init(void);
int tc_filter_init(void);
int pktsched_init(void);

extern int qdisc_restart(struct net_device *dev);

static inline void qdisc_run(struct net_device *dev)
{
	while (!netif_queue_stopped(dev) &&
	       qdisc_restart(dev)<0)
		/* NOTHING */;
}

/* Calculate maximal size of packet seen by hard_start_xmit
   routine of this device.
 */
static inline unsigned psched_mtu(struct net_device *dev)
{
	unsigned mtu = dev->mtu;
	return dev->hard_header ? mtu + dev->hard_header_len : mtu;
}

#endif
