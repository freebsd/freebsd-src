/*
 *	SMP locks primitives for building ix86 locks
 *	(not yet used).
 *
 *		Alan Cox, alan@redhat.com, 1995
 */
 
/*
 *	This would be much easier but far less clear and easy
 *	to borrow for other processors if it was just assembler.
 */

extern __inline__ void prim_spin_lock(struct spinlock *sp)
{
	int processor=smp_processor_id();
	
	/*
	 *	Grab the lock bit
	 */
	 
	while(lock_set_bit(0,&sp->lock))
	{
		/*
		 *	Failed, but that's cos we own it!
		 */
		 
		if(sp->cpu==processor)
		{
			sp->users++;
			return 0;
		}
		/*
		 *	Spin in the cache S state if possible
		 */
		while(sp->lock)
		{
			/*
			 *	Wait for any invalidates to go off
			 */
			 
			if(smp_invalidate_needed&(1<<processor))
				while(lock_clear_bit(processor,&smp_invalidate_needed))
					local_flush_tlb();
			sp->spins++;
		}
		/*
		 *	Someone wrote the line, we go 'I' and get
		 *	the cache entry. Now try to regrab
		 */
	}
	sp->users++;sp->cpu=processor;
	return 1;
}

/*
 *	Release a spin lock
 */
 
extern __inline__ int prim_spin_unlock(struct spinlock *sp)
{
	/* This is safe. The decrement is still guarded by the lock. A multilock would
	   not be safe this way */
	if(!--sp->users)
	{
		sp->cpu= NO_PROC_ID;lock_clear_bit(0,&sp->lock);
		return 1;
	}
	return 0;
}


/*
 *	Non blocking lock grab
 */
 
extern __inline__ int prim_spin_lock_nb(struct spinlock *sp)
{
	if(lock_set_bit(0,&sp->lock))
		return 0;		/* Locked already */
	sp->users++;
	return 1;			/* We got the lock */
}


/*
 *	These wrap the locking primitives up for usage
 */
 
extern __inline__ void spinlock(struct spinlock *sp)
{
	if(sp->priority<current->lock_order)
		panic("lock order violation: %s (%d)\n", sp->name, current->lock_order);
	if(prim_spin_lock(sp))
	{
		/*
		 *	We got a new lock. Update the priority chain
		 */
		sp->oldpri=current->lock_order;
		current->lock_order=sp->priority;
	}
}

extern __inline__ void spinunlock(struct spinlock *sp)
{
	int pri;
	if(current->lock_order!=sp->priority)
		panic("lock release order violation %s (%d)\n", sp->name, current->lock_order);
	pri=sp->oldpri;
	if(prim_spin_unlock(sp))
	{
		/*
		 *	Update the debugging lock priority chain. We dumped
		 *	our last right to the lock.
		 */
		current->lock_order=sp->pri;
	}	
}

extern __inline__ void spintestlock(struct spinlock *sp)
{
	/*
	 *	We do no sanity checks, it's legal to optimistically
	 *	get a lower lock.
	 */
	prim_spin_lock_nb(sp);
}

extern __inline__ void spintestunlock(struct spinlock *sp)
{
	/*
	 *	A testlock doesn't update the lock chain so we
	 *	must not update it on free
	 */
	prim_spin_unlock(sp);
}
