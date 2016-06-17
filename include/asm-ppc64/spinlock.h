#ifndef __ASM_SPINLOCK_H
#define __ASM_SPINLOCK_H

/*
 * Simple spin lock operations.  
 *
 * Copyright (C) 2001 Paul Mackerras <paulus@au.ibm.com>, IBM
 * Copyright (C) 2001 Anton Blanchard <anton@au.ibm.com>, IBM
 * Copyright (C) 2003 Dave Engebretsen <engebret@us.ibm.com>, IBM
 *   Rework to support virtual processors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <asm/memory.h>
#include <asm/hvcall.h>

/*
 * The following define is being used to select basic or shared processor
 * locking when running on an RPA platform.  As we do more performance
 * tuning, I would expect this selection mechanism to change.  Dave E.
 */
/* #define SPLPAR_LOCKS */

typedef struct {
	volatile unsigned long lock;
} spinlock_t;

#ifdef __KERNEL__
#define SPIN_LOCK_UNLOCKED	(spinlock_t) { 0 }

#define spin_is_locked(x)	((x)->lock != 0)

static __inline__ int spin_trylock(spinlock_t *lock)
{
	unsigned long tmp;

	__asm__ __volatile__(
"1:	ldarx		%0,0,%1		# spin_trylock\n\
	cmpdi		0,%0,0\n\
	li		%0,0\n\
	bne-		2f\n\
	li		%0,1\n\
	stdcx.		13,0,%1\n\
	bne-		1b\n\
	isync\n\
2:"	: "=&r"(tmp)
	: "r"(&lock->lock)
	: "cr0", "memory");

	return tmp;
}

/*
 * Spin lock states:
 *   0        : Unlocked
 *   Negative : Locked.  Value is paca pointer (0xc...0) of holder
 */
#ifdef CONFIG_PPC_ISERIES
static __inline__ void spin_lock(spinlock_t *lock)
{
	unsigned long tmp, tmp2;

	__asm__ __volatile__(
	"b		2f		# spin_lock\n\
1:"
	HMT_LOW
"       ldx		%0,0,%2         # load the lock value\n\
	cmpdi		0,%0,0          # if not locked, try to acquire\n\
	beq-		2f\n\
	lwz             5,0x280(%0)     # load yield counter\n\
	andi.           %1,5,1          # if even then spin\n\
	beq             1b\n\
	lwsync                          # if odd, give up cycles\n\
	ldx             %1,0,%2         # reverify the lock holder\n\
	cmpd            %0,%1\n\
	bne             1b              # new holder so restart\n\
	li              3,0x25          # yield hcall 0x8-12 \n\
	rotrdi          3,3,1           #   put the bits in the right spot\n\
	lhz             4,0x18(%0)      # processor number\n\
	sldi            4,4,32          #   move into top half of word\n\
	or              5,5,4           # r5 has yield cnt - or it in\n\
	li              4,2             # yield to processor\n\
	li              0,-1            # indicate an hcall\n\
	sc                              # do the hcall \n\
	b               1b\n\
2: \n"
	HMT_MEDIUM
" 	ldarx		%0,0,%2\n\
	cmpdi		0,%0,0\n\
	bne-		1b\n\
	stdcx.		13,0,%2\n\
	bne-		2b\n\
	isync"
	: "=&r"(tmp), "=&r"(tmp2)
	: "r"(&lock->lock)
	: "r0", "r3", "r4", "r5", "ctr", "cr0", "cr1", "cr2", "cr3", "cr4",
	  "xer", "memory");
}
#else
#ifdef SPLPAR_LOCKS
static __inline__ void spin_lock(spinlock_t *lock)
{
	unsigned long tmp, tmp2;

	__asm__ __volatile__(
	"b		2f		# spin_lock\n\
1:"
	HMT_LOW
"       ldx		%0,0,%2         # load the lock value\n\
	cmpdi		0,%0,0          # if not locked, try to acquire\n\
	beq-		2f\n\
	lwz             5,0x280(%0)     # load dispatch counter\n\
	andi.           %1,5,1          # if even then spin\n\
	beq             1b\n\
	lwsync                          # if odd, give up cycles\n\
	ldx             %1,0,%2         # reverify the lock holder\n\
	cmpd            %0,%1\n\
	bne             1b              # new holder so restart\n\
	li              3,0xE4          # give up the cycles H_CONFER\n\
	lhz             4,0x18(%0)      # processor number\n\
					# r5 has dispatch cnt already\n"
	HVSC
"        b               1b\n\
2: \n"
	HMT_MEDIUM
" 	ldarx		%0,0,%2\n\
	cmpdi		0,%0,0\n\
	bne-		1b\n\
	stdcx.		13,0,%2\n\
	bne-		2b\n\
	isync"
	: "=&r"(tmp), "=&r"(tmp2)
	: "r"(&lock->lock)
	: "r3", "r4", "r5", "cr0", "cr1", "ctr", "xer", "memory");
}
#else
static __inline__ void spin_lock(spinlock_t *lock)
{
	unsigned long tmp;

	__asm__ __volatile__(
       "b		2f		# spin_lock\n\
1:"
	HMT_LOW
"       ldx		%0,0,%1         # load the lock value\n\
	cmpdi		0,%0,0          # if not locked, try to acquire\n\
	bne+		1b\n\
2: \n"
	HMT_MEDIUM
" 	ldarx		%0,0,%1\n\
	cmpdi		0,%0,0\n\
	bne-		1b\n\
	stdcx.		13,0,%1\n\
	bne-		2b\n\
	isync"
	: "=&r"(tmp)
	: "r"(&lock->lock)
	: "cr0", "memory");
}
#endif
#endif

static __inline__ void spin_unlock(spinlock_t *lock)
{
	__asm__ __volatile__("lwsync	# spin_unlock": : :"memory");
	lock->lock = 0;
}

/*
 * Read-write spinlocks, allowing multiple readers
 * but only one writer.
 *
 * NOTE! it is quite common to have readers in interrupts
 * but no interrupt writers. For those circumstances we
 * can "mix" irq-safe locks - any writer needs to get a
 * irq-safe write-lock, but readers can get non-irqsafe
 * read-locks.
 *
 * Write lock states:
 *   0        : Unlocked
 *   Positive : Reader count
 *   Negative : Writer locked.  Value is paca pointer (0xc...0) of holder
 *
 * If lock is not held, try to acquire.
 * If lock is held by a writer, yield cycles to the holder.
 * If lock is help by reader(s), spin.
 */
typedef struct {
	volatile signed long lock;
} rwlock_t;

#define RW_LOCK_UNLOCKED (rwlock_t) { 0 }

static __inline__ int read_trylock(rwlock_t *rw)
{
	unsigned long tmp;
	unsigned int ret;

	__asm__ __volatile__(
"1:	ldarx		%0,0,%2		# read_trylock\n\
	li		%1,0\n\
	addic.		%0,%0,1\n\
	ble-		2f\n\
	stdcx.		%0,0,%2\n\
	bne-		1b\n\
	li		%1,1\n\
	isync\n\
2:"	: "=&r"(tmp), "=&r"(ret)
	: "r"(&rw->lock)
	: "cr0", "memory");

	return ret;
}

#ifdef CONFIG_PPC_ISERIES
static __inline__ void read_lock(rwlock_t *rw)
{
	unsigned long tmp, tmp2;

	__asm__ __volatile__(
	"b		2f		# read_lock\n\
1:"
	HMT_LOW
"	ldx		%0,0,%2\n\
	cmpdi		0,%0,0\n\
	bge-		2f\n\
	lwz             5,0x280(%0)     # load yield counter\n\
	andi.           %1,5,1          # if even then spin\n\
	beq             1b\n\
	lwsync                          # if odd, give up cycles\n\
	ldx             %1,0,%2         # reverify the lock holder\n\
	cmpd            %0,%1\n\
	bne             1b              # new holder so restart\n\
	li              3,0x25          # yield hcall 0x8-12 \n\
	rotrdi          3,3,1           #   put the bits in the right spot\n\
	lhz             4,0x18(%0)      # processor number\n\
	sldi            4,4,32          #   move into top half of word\n\
	or              5,5,4           # r5 has yield cnt - or it in\n\
	li              4,2             # yield to processor\n\
	li              0,-1            # indicate an hcall\n\
	sc                              # do the hcall \n\
2: \n"
	HMT_MEDIUM
" 	ldarx		%0,0,%2\n\
	addic.		%0,%0,1\n\
	ble-		1b\n\
	stdcx.		%0,0,%2\n\
	bne-		2b\n\
	isync"
	: "=&r"(tmp), "=&r"(tmp2)
	: "r"(&rw->lock)
	: "r0", "r3", "r4", "r5", "ctr", "cr0", "cr1", "cr2", "cr3", "cr4",
	  "xer", "memory");
}
#else
#ifdef SPLPAR_LOCKS
static __inline__ void read_lock(rwlock_t *rw)
{
	unsigned long tmp, tmp2;

	__asm__ __volatile__(
	"b		2f		# read_lock\n\
1:"
	HMT_LOW
"	ldx		%0,0,%2\n\
	cmpdi		0,%0,0\n\
	bge-		2f\n\
	lwz             5,0x280(%0)     # load dispatch counter\n\
	andi.           %1,5,1          # if even then spin\n\
	beq             1b\n\
	lwsync                          # if odd, give up cycles\n\
	ldx             %1,0,%2         # reverify the lock holder\n\
	cmpd            %0,%1\n\
	bne             1b              # new holder so restart\n\
	li              3,0xE4          # give up the cycles H_CONFER\n\
	lhz             4,0x18(%0)      # processor number\n\
					# r5 has dispatch cnt already\n"
	HVSC
"2: \n"
	HMT_MEDIUM
" 	ldarx		%0,0,%2\n\
	addic.		%0,%0,1\n\
	ble-		1b\n\
	stdcx.		%0,0,%2\n\
	bne-		2b\n\
	isync"
	: "=&r"(tmp), "=&r"(tmp2)
	: "r"(&rw->lock)
	: "r3", "r4", "r5", "cr0", "cr1", "ctr", "xer", "memory");
}
#else
static __inline__ void read_lock(rwlock_t *rw)
{
	unsigned long tmp;

	__asm__ __volatile__(
	"b		2f		# read_lock\n\
1:"
	HMT_LOW
"	ldx		%0,0,%1\n\
	cmpdi		0,%0,0\n\
	blt+		1b\n\
2: \n"
	HMT_MEDIUM
" 	ldarx		%0,0,%1\n\
	addic.		%0,%0,1\n\
	ble-		1b\n\
	stdcx.		%0,0,%1\n\
	bne-		2b\n\
	isync"
	: "=&r"(tmp)
	: "r"(&rw->lock)
	: "cr0", "memory");
}
#endif
#endif

static __inline__ void read_unlock(rwlock_t *rw)
{
	unsigned long tmp;

	__asm__ __volatile__(
	"eieio				# read_unlock\n\
1:	ldarx		%0,0,%1\n\
	addic		%0,%0,-1\n\
	stdcx.		%0,0,%1\n\
	bne-		1b"
	: "=&r"(tmp)
	: "r"(&rw->lock)
	: "cr0", "memory");
}

static __inline__ int write_trylock(rwlock_t *rw)
{
	unsigned long tmp;
	unsigned long ret;

	__asm__ __volatile__(
"1:	ldarx		%0,0,%2		# write_trylock\n\
	cmpdi		0,%0,0\n\
	li		%1,0\n\
	bne-		2f\n\
	stdcx.		13,0,%2\n\
	bne-		1b\n\
	li		%1,1\n\
	isync\n\
2:"	: "=&r"(tmp), "=&r"(ret)
	: "r"(&rw->lock)
	: "cr0", "memory");

	return ret;
}

#ifdef CONFIG_PPC_ISERIES
static __inline__ void write_lock(rwlock_t *rw)
{
	unsigned long tmp, tmp2;

	__asm__ __volatile__(
	"b		2f		# spin_lock\n\
1:"
	HMT_LOW
"       ldx		%0,0,%2         # load the lock value\n\
	cmpdi		0,%0,0          # if not locked(0), try to acquire\n\
	beq-		2f\n\
	bgt             1b              # negative(0xc..)->cycles to holder\n"
"3:     lwz             5,0x280(%0)     # load yield counter\n\
	andi.           %1,5,1          # if even then spin\n\
	beq             1b\n\
	lwsync                          # if odd, give up cycles\n\
	ldx             %1,0,%2         # reverify the lock holder\n\
	cmpd            %0,%1\n\
	bne             1b              # new holder so restart\n\
	lhz             4,0x18(%0)      # processor number\n\
	sldi            4,4,32          #   move into top half of word\n\
	or              5,5,4           # r5 has yield cnt - or it in\n\
	li              3,0x25          # yield hcall 0x8-12 \n\
	rotrdi          3,3,1           #   put the bits in the right spot\n\
	li              4,2             # yield to processor\n\
	li              0,-1            # indicate an hcall\n\
	sc                              # do the hcall \n\
2: \n"
	HMT_MEDIUM
" 	ldarx		%0,0,%2\n\
	cmpdi		0,%0,0\n\
	bne-		1b\n\
	stdcx.		13,0,%2\n\
	bne-		2b\n\
	isync"
	: "=&r"(tmp), "=&r"(tmp2)
	: "r"(&rw->lock)
	: "r0", "r3", "r4", "r5", "ctr", "cr0", "cr1", "cr2", "cr3", "cr4",
	  "xer", "memory");
}
#else
#ifdef SPLPAR_LOCKS
static __inline__ void write_lock(rwlock_t *rw)
{
	unsigned long tmp, tmp2;

	__asm__ __volatile__(
	"b		2f		# spin_lock\n\
1:"
	HMT_LOW
"       ldx		%0,0,%2         # load the lock value\n\
	li              3,0xE4          # give up the cycles H_CONFER\n\
	cmpdi		0,%0,0          # if not locked(0), try to acquire\n\
	beq-		2f\n\
	blt             3f              # negative(0xc..)->confer to holder\n\
	b               1b\n"
"3:      lwz             5,0x280(%0)     # load dispatch counter\n\
	andi.           %1,5,1          # if even then spin\n\
	beq             1b\n\
	lwsync                          # if odd, give up cycles\n\
	ldx             %1,0,%2         # reverify the lock holder\n\
	cmpd            %0,%1\n\
	bne             1b              # new holder so restart\n\
	lhz             4,0x18(%0)      # processor number\n\
					# r5 has dispatch cnt already\n"
	HVSC
"        b               1b\n\
2: \n"
	HMT_MEDIUM
" 	ldarx		%0,0,%2\n\
	cmpdi		0,%0,0\n\
	bne-		1b\n\
	stdcx.		13,0,%2\n\
	bne-		2b\n\
	isync"
	: "=&r"(tmp), "=&r"(tmp2)
	: "r"(&rw->lock)
	: "r3", "r4", "r5", "cr0", "cr1", "ctr", "xer", "memory");
}
#else
static __inline__ void write_lock(rwlock_t *rw)
{
	unsigned long tmp;

	__asm__ __volatile__(
	"b		2f		# spin_lock\n\
1:"
	HMT_LOW
"       ldx		%0,0,%1         # load the lock value\n\
	cmpdi		0,%0,0          # if not locked(0), try to acquire\n\
	bne+		1b\n\
2: \n"
	HMT_MEDIUM
" 	ldarx		%0,0,%1\n\
	cmpdi		0,%0,0\n\
	bne-		1b\n\
	stdcx.		13,0,%1\n\
	bne-		2b\n\
	isync"
	: "=&r"(tmp)
	: "r"(&rw->lock)
	: "cr0", "memory");
}
#endif
#endif

static __inline__ void write_unlock(rwlock_t *rw)
{
	__asm__ __volatile__("lwsync		# write_unlock": : :"memory");
	rw->lock = 0;
}

static __inline__ int is_read_locked(rwlock_t *rw)
{
	return rw->lock > 0;
}

static __inline__ int is_write_locked(rwlock_t *rw)
{
	return rw->lock < 0;
}

#define spin_lock_init(x)      do { *(x) = SPIN_LOCK_UNLOCKED; } while(0)
#define spin_unlock_wait(x)    do { barrier(); } while(spin_is_locked(x))

#define rwlock_init(x)         do { *(x) = RW_LOCK_UNLOCKED; } while(0)

#endif /* __KERNEL__ */
#endif /* __ASM_SPINLOCK_H */
