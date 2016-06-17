/*
 *  include/asm-s390/semaphore.h
 *
 *  S390 version
 *    Copyright (C) 1999,2000 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *
 *  Derived from "include/asm-i386/semaphore.h"
 *    (C) Copyright 1996 Linus Torvalds
 */

#ifndef _S390_SEMAPHORE_H
#define _S390_SEMAPHORE_H

#include <asm/system.h>
#include <asm/atomic.h>
#include <linux/wait.h>
#include <linux/rwsem.h>

struct semaphore {
	atomic_t count;
	int sleepers;
	wait_queue_head_t wait;
};

#define __SEM_DEBUG_INIT(name)

#define __SEMAPHORE_INITIALIZER(name,count) \
{ ATOMIC_INIT(count), 0, __WAIT_QUEUE_HEAD_INITIALIZER((name).wait) \
	__SEM_DEBUG_INIT(name) }

#define __MUTEX_INITIALIZER(name) \
	__SEMAPHORE_INITIALIZER(name,1)

#define __DECLARE_SEMAPHORE_GENERIC(name,count) \
	struct semaphore name = __SEMAPHORE_INITIALIZER(name,count)

#define DECLARE_MUTEX(name) __DECLARE_SEMAPHORE_GENERIC(name,1)
#define DECLARE_MUTEX_LOCKED(name) __DECLARE_SEMAPHORE_GENERIC(name,0)

static inline void sema_init (struct semaphore *sem, int val)
{
	*sem = (struct semaphore)__SEMAPHORE_INITIALIZER((*sem),val);
}

static inline void init_MUTEX (struct semaphore *sem)
{
	sema_init(sem, 1);
}

static inline void init_MUTEX_LOCKED (struct semaphore *sem)
{
	sema_init(sem, 0);
}

asmlinkage void __down_failed(void /* special register calling convention */);
asmlinkage int  __down_failed_interruptible(void  /* params in registers */);
asmlinkage int  __down_failed_trylock(void  /* params in registers */);
asmlinkage void __up_wakeup(void /* special register calling convention */);

asmlinkage void __down(struct semaphore * sem);
asmlinkage int  __down_interruptible(struct semaphore * sem);
asmlinkage int  __down_trylock(struct semaphore * sem);
asmlinkage void __up(struct semaphore * sem);

static inline void down(struct semaphore * sem)
{
	if (atomic_dec_return(&sem->count) < 0)
		__down(sem);
}

static inline int down_interruptible(struct semaphore * sem)
{
	int ret = 0;

	if (atomic_dec_return(&sem->count) < 0)
		ret = __down_interruptible(sem);
	return ret;
}

static inline int down_trylock(struct semaphore * sem)
{
	int ret = 0;

	if (atomic_dec_return(&sem->count) < 0)
		ret = __down_trylock(sem);
	return ret;
}

static inline void up(struct semaphore * sem)
{
	if (atomic_inc_return(&sem->count) <= 0)
		__up(sem);
}

static inline int sem_getcount(struct semaphore *sem)
{
	return atomic_read(&sem->count);
}

#endif
