#ifndef __ASM_CRIS_ATOMIC__
#define __ASM_CRIS_ATOMIC__

#include <asm/system.h>

/*
 * Atomic operations that C can't guarantee us.  Useful for
 * resource counting etc..
 */

/*
 * Make sure gcc doesn't try to be clever and move things around
 * on us. We need to use _exactly_ the address the user gave us,
 * not some alias that contains the same information.
 */

#define __atomic_fool_gcc(x) (*(struct { int a[100]; } *)x)

typedef struct { int counter; } atomic_t;

#define ATOMIC_INIT(i)  { (i) }

#define atomic_read(v) ((v)->counter)
#define atomic_set(v,i) (((v)->counter) = (i))

/* These should be written in asm but we do it in C for now. */

extern __inline__ void atomic_add(int i, volatile atomic_t *v)
{
	unsigned long flags;
	save_flags(flags);
	cli();
	v->counter += i;
	restore_flags(flags);
}

extern __inline__ void atomic_sub(int i, volatile atomic_t *v)
{
	unsigned long flags;
	save_flags(flags);
	cli();
	v->counter -= i;
	restore_flags(flags);
}

extern __inline__ int atomic_add_return(int i, volatile atomic_t *v)
{
	unsigned long flags;
	int retval;
	save_flags(flags);
	cli();
	retval = (v->counter += i);
	restore_flags(flags);
	return retval;
}

extern __inline__ int atomic_sub_return(int i, volatile atomic_t *v)
{
	unsigned long flags;
	int retval;
	save_flags(flags);
	cli();
	retval = (v->counter -= i);
	restore_flags(flags);
	return retval;
}

extern __inline__ int atomic_sub_and_test(int i, volatile atomic_t *v)
{
	int retval;
	unsigned long flags;
	save_flags(flags);
	cli();
	retval = (v->counter -= i) == 0;
	restore_flags(flags);
	return retval;
}

extern __inline__ void atomic_inc(volatile atomic_t *v)
{
	unsigned long flags;
	save_flags(flags);
	cli();
	(v->counter)++;
	restore_flags(flags);
}

extern __inline__ void atomic_dec(volatile atomic_t *v)
{
	unsigned long flags;
	save_flags(flags);
	cli();
	(v->counter)--;
	restore_flags(flags);
}

extern __inline__ int atomic_inc_return(volatile atomic_t *v)
{
	unsigned long flags;
	int retval;
	save_flags(flags);
	cli();
	retval = (v->counter)++;
	restore_flags(flags);
	return retval;
}

extern __inline__ int atomic_dec_return(volatile atomic_t *v)
{
	unsigned long flags;
	int retval;
	save_flags(flags);
	cli();
	retval = (v->counter)--;
	restore_flags(flags);
	return retval;
}
extern __inline__ int atomic_dec_and_test(volatile atomic_t *v)
{
	int retval;
	unsigned long flags;
	save_flags(flags);
	cli();
	retval = --(v->counter) == 0;
	restore_flags(flags);
	return retval;
}

extern __inline__ int atomic_inc_and_test(volatile atomic_t *v)
{
	int retval;
	unsigned long flags;
	save_flags(flags);
	cli();
	retval = ++(v->counter) == 0;
	restore_flags(flags);
	return retval;
}

/* Atomic operations are already serializing */
#define smp_mb__before_atomic_dec()    barrier()
#define smp_mb__after_atomic_dec()     barrier()
#define smp_mb__before_atomic_inc()    barrier()
#define smp_mb__after_atomic_inc()     barrier()

#endif
