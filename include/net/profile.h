#include <linux/config.h> /* for CONFIG_NET_PROFILE */
#ifndef _NET_PROFILE_H_
#define _NET_PROFILE_H_ 1

#ifdef CONFIG_NET_PROFILE

#include <linux/types.h>
#include <linux/time.h>
#include <linux/kernel.h>
#include <asm/system.h>

#ifdef CONFIG_X86_TSC
#include <asm/msr.h>
#endif

struct net_profile_slot
{
	char   id[16];
	struct net_profile_slot *next;
	struct timeval entered;
	struct timeval accumulator;
	struct timeval irq;
	int    	hits;
	int    	active;
	int	underflow;
};

extern atomic_t net_profile_active;
extern struct timeval net_profile_adjust;
extern void net_profile_irq_adjust(struct timeval *entered, struct timeval* leaved);

#ifdef CONFIG_X86_TSC

static inline void  net_profile_stamp(struct timeval *pstamp)
{
	rdtsc(pstamp->tv_usec, pstamp->tv_sec);
}

static inline void  net_profile_accumulate(struct timeval *entered,
					       struct timeval *leaved,
					       struct timeval *acc)
{
	__asm__ __volatile__ ("subl %2,%0\n\t" 
			      "sbbl %3,%1\n\t" 
			      "addl %4,%0\n\t" 
			      "adcl %5,%1\n\t" 
			      "subl " SYMBOL_NAME_STR(net_profile_adjust) "+4,%0\n\t" 
			      "sbbl $0,%1\n\t" 
			      : "=r" (acc->tv_usec), "=r" (acc->tv_sec)
			      : "g" (entered->tv_usec), "g" (entered->tv_sec),
			      "g" (leaved->tv_usec), "g" (leaved->tv_sec),
			      "0" (acc->tv_usec), "1" (acc->tv_sec));
}

static inline void  net_profile_sub(struct timeval *sub,
					struct timeval *acc)
{
	__asm__ __volatile__ ("subl %2,%0\n\t" 
			      "sbbl %3,%1\n\t" 
			      : "=r" (acc->tv_usec), "=r" (acc->tv_sec)
			      : "g" (sub->tv_usec), "g" (sub->tv_sec),
			      "0" (acc->tv_usec), "1" (acc->tv_sec));
}

static inline void  net_profile_add(struct timeval *add,
					struct timeval *acc)
{
	__asm__ __volatile__ ("addl %2,%0\n\t" 
			      "adcl %3,%1\n\t" 
			      : "=r" (acc->tv_usec), "=r" (acc->tv_sec)
			      : "g" (add->tv_usec), "g" (add->tv_sec),
			      "0" (acc->tv_usec), "1" (acc->tv_sec));
}


#elif defined (__alpha__)

extern __u32 alpha_lo;
extern long alpha_hi;

/* On alpha cycle counter has only 32 bits :-( :-( */

static inline void  net_profile_stamp(struct timeval *pstamp)
{
	__u32 result;
	__asm__ __volatile__ ("rpcc %0" : "r="(result));
	if (result <= alpha_lo)
		alpha_hi++;
	alpha_lo = result;
	pstamp->tv_sec = alpha_hi;
	pstamp->tv_usec = alpha_lo;
}

static inline void  net_profile_accumulate(struct timeval *entered,
					       struct timeval *leaved,
					       struct timeval *acc)
{
	time_t usecs = acc->tv_usec + leaved->tv_usec - entered->tv_usec
		- net_profile_adjust.tv_usec;
	time_t secs = acc->tv_sec + leaved->tv_sec - entered->tv_sec;

	if (usecs >= 0x100000000L) {
		usecs -= 0x100000000L;
		secs++;
	} else if (usecs < -0x100000000L) {
		usecs += 0x200000000L;
		secs -= 2;
	} else if (usecs < 0) {
		usecs += 0x100000000L;
		secs--;
	}
	acc->tv_sec = secs;
	acc->tv_usec = usecs;
}

static inline void  net_profile_sub(struct timeval *entered,
					struct timeval *leaved)
{
	time_t usecs = leaved->tv_usec - entered->tv_usec;
	time_t secs = leaved->tv_sec - entered->tv_sec;

	if (usecs < 0) {
		usecs += 0x100000000L;
		secs--;
	}
	leaved->tv_sec = secs;
	leaved->tv_usec = usecs;
}

static inline void  net_profile_add(struct timeval *entered, struct timeval *leaved)
{
	time_t usecs = leaved->tv_usec + entered->tv_usec;
	time_t secs = leaved->tv_sec + entered->tv_sec;

	if (usecs >= 0x100000000L) {
		usecs -= 0x100000000L;
		secs++;
	}
	leaved->tv_sec = secs;
	leaved->tv_usec = usecs;
}


#else

static inline void  net_profile_stamp(struct timeval *pstamp)
{
	/* Not "fast" counterpart! On architectures without
	   cpu clock "fast" routine is absolutely useless in this
	   situation. do_gettimeofday still says something on slow-slow-slow
	   boxes, though it eats more cpu time than the subject of
	   investigation :-) :-)
	 */
	do_gettimeofday(pstamp);
}

static inline void  net_profile_accumulate(struct timeval *entered,
					       struct timeval *leaved,
					       struct timeval *acc)
{
	time_t usecs = acc->tv_usec + leaved->tv_usec - entered->tv_usec
		- net_profile_adjust.tv_usec;
	time_t secs = acc->tv_sec + leaved->tv_sec - entered->tv_sec;

	if (usecs >= 1000000) {
		usecs -= 1000000;
		secs++;
	} else if (usecs < -1000000) {
		usecs += 2000000;
		secs -= 2;
	} else if (usecs < 0) {
		usecs += 1000000;
		secs--;
	}
	acc->tv_sec = secs;
	acc->tv_usec = usecs;
}

static inline void  net_profile_sub(struct timeval *entered,
					struct timeval *leaved)
{
	time_t usecs = leaved->tv_usec - entered->tv_usec;
	time_t secs = leaved->tv_sec - entered->tv_sec;

	if (usecs < 0) {
		usecs += 1000000;
		secs--;
	}
	leaved->tv_sec = secs;
	leaved->tv_usec = usecs;
}

static inline void  net_profile_add(struct timeval *entered, struct timeval *leaved)
{
	time_t usecs = leaved->tv_usec + entered->tv_usec;
	time_t secs = leaved->tv_sec + entered->tv_sec;

	if (usecs >= 1000000) {
		usecs -= 1000000;
		secs++;
	}
	leaved->tv_sec = secs;
	leaved->tv_usec = usecs;
}



#endif

static inline void net_profile_enter(struct net_profile_slot *s)
{
	unsigned long flags;

	save_flags(flags);
	cli();
	if (s->active++ == 0) {
		net_profile_stamp(&s->entered);
		atomic_inc(&net_profile_active);
	}
	restore_flags(flags);
}

static inline void net_profile_leave_irq(struct net_profile_slot *s)
{
	unsigned long flags;

	save_flags(flags);
	cli();
	if (--s->active <= 0) {
		if (s->active == 0) {
			struct timeval curr_pstamp;
			net_profile_stamp(&curr_pstamp);
			net_profile_accumulate(&s->entered, &curr_pstamp, &s->accumulator);
			if (!atomic_dec_and_test(&net_profile_active))
				net_profile_irq_adjust(&s->entered, &curr_pstamp);
		} else {
			s->underflow++;
		}
	}
	s->hits++;
	restore_flags(flags);
}

static inline void net_profile_leave(struct net_profile_slot *s)
{
	unsigned long flags;
	save_flags(flags);
	cli();
	if (--s->active <= 0) {
		if (s->active == 0) {
			struct timeval curr_pstamp;
			net_profile_stamp(&curr_pstamp);
			net_profile_accumulate(&s->entered, &curr_pstamp, &s->accumulator);
			atomic_dec(&net_profile_active);
		} else {
			s->underflow++;
		}
	}
	s->hits++;
	restore_flags(flags);
}


#define NET_PROFILE_ENTER(slot) net_profile_enter(&net_prof_##slot)
#define NET_PROFILE_LEAVE(slot) net_profile_leave(&net_prof_##slot)
#define NET_PROFILE_LEAVE_IRQ(slot) net_profile_leave_irq(&net_prof_##slot)

#define NET_PROFILE_SKB_CLEAR(skb) ({ \
 skb->pstamp.tv_usec = 0; \
})

#define NET_PROFILE_SKB_INIT(skb) ({ \
 net_profile_stamp(&skb->pstamp); \
})

#define NET_PROFILE_SKB_PASSED(skb, slot) ({ \
 if (skb->pstamp.tv_usec) { \
   struct timeval cur_pstamp = skb->pstamp; \
   net_profile_stamp(&skb->pstamp); \
   net_profile_accumulate(&cur_pstamp, &skb->pstamp, &net_prof_##slot.accumulator); \
   net_prof_##slot.hits++; \
 }})

#define NET_PROFILE_DECL(slot) \
  extern struct net_profile_slot net_prof_##slot;

#define NET_PROFILE_DEFINE(slot) \
  struct net_profile_slot net_prof_##slot = { #slot, };

#define NET_PROFILE_REGISTER(slot) net_profile_register(&net_prof_##slot)
#define NET_PROFILE_UNREGISTER(slot) net_profile_unregister(&net_prof_##slot)

extern int net_profile_init(void);
extern int net_profile_register(struct net_profile_slot *);
extern int net_profile_unregister(struct net_profile_slot *);

#else

#define NET_PROFILE_ENTER(slot) do { /* nothing */ } while(0)
#define NET_PROFILE_LEAVE(slot) do { /* nothing */ } while(0)
#define NET_PROFILE_LEAVE_IRQ(slot) do { /* nothing */ } while(0)
#define NET_PROFILE_SKB_CLEAR(skb) do { /* nothing */ } while(0)
#define NET_PROFILE_SKB_INIT(skb) do { /* nothing */ } while(0)
#define NET_PROFILE_SKB_PASSED(skb, slot) do { /* nothing */ } while(0)
#define NET_PROFILE_DECL(slot)
#define NET_PROFILE_DEFINE(slot)
#define NET_PROFILE_REGISTER(slot) do { /* nothing */ } while(0)
#define NET_PROFILE_UNREGISTER(slot) do { /* nothing */ } while(0)

#endif

#endif
