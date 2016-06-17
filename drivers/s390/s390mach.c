/*
 *  arch/s390/kernel/s390mach.c
 *   S/390 machine check handler,
 *            currently only channel-reports are supported
 *
 *  S390 version
 *    Copyright (C) 2000 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Ingo Adlung (adlung@de.ibm.com)
 */

#include <linux/config.h>
#include <linux/spinlock.h>
#include <linux/init.h>
#include <linux/slab.h>
#ifdef CONFIG_SMP
#include <linux/smp.h>
#endif

#include <asm/irq.h>
#include <asm/lowcore.h>
#include <asm/semaphore.h>
#include <asm/s390io.h>
#include <asm/s390dyn.h>
#include <asm/s390mach.h>
#ifdef CONFIG_MACHCHK_WARNING
#include <asm/signal.h>
#endif

extern void ctrl_alt_del(void);

#define S390_MACHCHK_DEBUG

static int         s390_machine_check_handler( void * parm );
static void        s390_enqueue_mchchk( mache_t *mchchk );
static mache_t    *s390_dequeue_mchchk( void );
static void        s390_enqueue_free_mchchk( mache_t *mchchk );
static mache_t    *s390_dequeue_free_mchchk( void );
static int         s390_collect_crw_info( void );
#ifdef CONFIG_MACHCHK_WARNING
static int         s390_post_warning( void );
#endif

static mache_t    *mchchk_queue_head = NULL;
static mache_t    *mchchk_queue_tail = NULL;
static mache_t    *mchchk_queue_free = NULL;
static crwe_t     *crw_buffer_anchor = NULL;
static spinlock_t  mchchk_queue_lock = SPIN_LOCK_UNLOCKED;
static spinlock_t  crw_queue_lock    = SPIN_LOCK_UNLOCKED;

static struct semaphore s_sem;

#ifdef CONFIG_MACHCHK_WARNING
static int mchchk_wng_posted = 0;
#endif

/*
 * s390_init_machine_check
 *
 * initialize machine check handling
 */
void s390_init_machine_check( void )
{
	crwe_t  *pcrwe;	 /* CRW buffer element pointer */
	mache_t *pmache;   /* machine check element pointer */

	init_MUTEX_LOCKED( &s_sem );

	pcrwe = kmalloc( MAX_CRW_PENDING * sizeof( crwe_t), GFP_KERNEL);

	if ( pcrwe )
	{
		int i;

		crw_buffer_anchor = pcrwe;

		for ( i=0; i < MAX_CRW_PENDING-1; i++)
		{
			pcrwe->crwe_next = (crwe_t *)((unsigned long)pcrwe + sizeof(crwe_t));
   		pcrwe            = pcrwe->crwe_next;

		} /* endfor */	

		pcrwe->crwe_next = NULL;

	}
	else
	{
		panic( "s390_init_machine_check : unable to obtain memory\n");		

	} /* endif */

	pmache = kmalloc( MAX_MACH_PENDING * sizeof( mache_t), GFP_KERNEL);

	if ( pmache )
	{
		int i;

		for ( i=0; i < MAX_MACH_PENDING; i++)
		{
			s390_enqueue_free_mchchk( pmache );
		   pmache = (mache_t *)((unsigned long)pmache + sizeof(mache_t));

		} /* endfor */	
	}
	else
	{
		panic( "s390_init_machine_check : unable to obtain memory\n");		

	} /* endif */

#ifdef S390_MACHCHK_DEBUG
	printk( KERN_NOTICE "init_mach : starting machine check handler\n");
#endif	

	kernel_thread( s390_machine_check_handler, &s_sem, CLONE_FS | CLONE_FILES);

	ctl_clear_bit( 14, 25 );  // disable damage MCH 	

	ctl_set_bit( 14, 26 ); /* enable degradation MCH */
	ctl_set_bit( 14, 27 ); /* enable system recovery MCH */
#if 1
  	ctl_set_bit( 14, 28 );		// enable channel report MCH
#endif
#ifdef CONFIG_MACHCHK_WARNING
	ctl_set_bit( 14, 24);   /* enable warning MCH */
#endif

#ifdef S390_MACHCHK_DEBUG
	printk( KERN_DEBUG "init_mach : machine check buffer : head = %08X\n",
            (unsigned)&mchchk_queue_head);
	printk( KERN_DEBUG "init_mach : machine check buffer : tail = %08X\n",
            (unsigned)&mchchk_queue_tail);
	printk( KERN_DEBUG "init_mach : machine check buffer : free = %08X\n",
            (unsigned)&mchchk_queue_free);
	printk( KERN_DEBUG "init_mach : CRW entry buffer anchor = %08X\n",
            (unsigned)&crw_buffer_anchor);
	printk( KERN_DEBUG "init_mach : machine check handler ready\n");
#endif	

	return;
}

static void s390_handle_damage(char * msg){

	unsigned long caller = (unsigned long) __builtin_return_address(0);

	printk(KERN_EMERG "%s\n", msg);
#ifdef CONFIG_SMP
	smp_send_stop();
#endif
	disabled_wait(caller);
	return;
}

/*
 * s390_do_machine_check
 *
 * mchine check pre-processor, collecting the machine check info,
 *  queueing it and posting the machine check handler for processing.
 */
void s390_do_machine_check( void )
{
	int      crw_count;
	mcic_t   mcic;

#ifdef S390_MACHCHK_DEBUG
	printk( KERN_INFO "s390_do_machine_check : starting ...\n");
#endif

	memcpy( &mcic,
	        &S390_lowcore.mcck_interruption_code,
	        sizeof(__u64));
 		
	if (mcic.mcc.mcd.sd) /* system damage */
		s390_handle_damage("received system damage machine check\n");

	if (mcic.mcc.mcd.pd) /* instruction processing damage */
		s390_handle_damage("received instruction processing damage machine check\n");

	if (mcic.mcc.mcd.se) /* storage error uncorrected */
		s390_handle_damage("received storage error uncorrected machine check\n");

	if (mcic.mcc.mcd.sc) /* storage error corrected */
		printk(KERN_WARNING "received storage error corrected machine check\n");

	if (mcic.mcc.mcd.ke) /* storage key-error uncorrected */
		s390_handle_damage("received storage key-error uncorrected machine check\n");

	if (mcic.mcc.mcd.ds && mcic.mcc.mcd.fa) /* storage degradation */
		s390_handle_damage("received storage degradation machine check\n");

	if ( mcic.mcc.mcd.cp )	// CRW pending ?
	{
		crw_count = s390_collect_crw_info();

		if ( crw_count )
		{
			up( &s_sem );

		} /* endif */

	} /* endif */
#ifdef CONFIG_MACHCHK_WARNING
/*
 * The warning may remain for a prolonged period on the bare iron.
 * (actually till the machine is powered off, or until the problem is gone)
 * So we just stop listening for the WARNING MCH and prevent continuously
 * being interrupted.  One caveat is however, that we must do this per 
 * processor and cannot use the smp version of ctl_clear_bit().
 * On VM we only get one interrupt per virtally presented machinecheck.
 * Though one suffices, we may get one interrupt per (virtual) processor. 
 */
	if ( mcic.mcc.mcd.w )	// WARNING pending ?
	{
		// Use single machine clear, as we cannot handle smp right now
		__ctl_clear_bit( 14, 24 );	// Disable WARNING MCH

		if ( ! mchchk_wng_posted )
		{ 
			mchchk_wng_posted = s390_post_warning();

			if ( mchchk_wng_posted )
			{
				up( &s_sem );

			} /* endif */

		} /* endif */

	} /* endif */
#endif

#ifdef S390_MACHCHK_DEBUG
	printk( KERN_INFO "s390_do_machine_check : done \n");
#endif

	return;
}

/*
 * s390_machine_check_handler
 *
 * machine check handler, dequeueing machine check entries
 *  and processing them
 */
static int s390_machine_check_handler( void *parm)
{
	struct semaphore *sem = parm;
	unsigned long     flags;
	mache_t          *pmache;

	int               found = 0;

        /* set name to something sensible */
        strcpy (current->comm, "kmcheck");


        /* block all signals */
        sigfillset(&current->blocked);

#ifdef S390_MACHCHK_DEBUG
	printk( KERN_NOTICE "mach_handler : ready\n");
#endif	

	do {

#ifdef S390_MACHCHK_DEBUG
		printk( KERN_NOTICE "mach_handler : waiting for wakeup\n");
#endif	

		down_interruptible( sem );

#ifdef S390_MACHCHK_DEBUG
		printk( KERN_NOTICE "\nmach_handler : wakeup ... \n");
#endif	
		found = 0; /* init ... */

		__save_flags( flags );
		__cli();

		do {

		pmache = s390_dequeue_mchchk();

		if ( pmache )
		{
			found = 1;
		
			if ( pmache->mcic.mcc.mcd.cp )
			{
				crwe_t *pcrwe_n;
				crwe_t *pcrwe_h;

				s390_do_crw_pending( pmache->mc.crwe );

				pcrwe_h = pmache->mc.crwe;
				pcrwe_n = pmache->mc.crwe->crwe_next;

				pmache->mcic.mcc.mcd.cp = 0;
				pmache->mc.crwe         = NULL;

				spin_lock( &crw_queue_lock);

				while ( pcrwe_h )
				{
					pcrwe_h->crwe_next = crw_buffer_anchor;
					crw_buffer_anchor  = pcrwe_h;
					pcrwe_h            = pcrwe_n;

					if ( pcrwe_h != NULL )
						pcrwe_n = pcrwe_h->crwe_next;

				} /* endwhile */

				spin_unlock( &crw_queue_lock);

			} /* endif */

#ifdef CONFIG_MACHCHK_WARNING
			if ( pmache->mcic.mcc.mcd.w )
			{
				ctrl_alt_del();		// shutdown NOW!
#ifdef S390_MACHCHK_DEBUG
			printk( KERN_DEBUG "mach_handler : kill -SIGPWR init\n");
#endif
			} /* endif */
#endif

#ifdef CONFIG_MACHCHK_WARNING
			if ( pmache->mcic.mcc.mcd.w )
			{
				ctrl_alt_del();		// shutdown NOW!
#ifdef S390_MACHCHK_DEBUG
			printk( KERN_DEBUG "mach_handler : kill -SIGPWR init\n");
#endif
			} /* endif */
#endif

			s390_enqueue_free_mchchk( pmache );
		}
		else
		{

			// unconditional surrender ...
#ifdef S390_MACHCHK_DEBUG
			printk( KERN_DEBUG "mach_handler : nothing to do, sleeping\n");
#endif	

		} /* endif */	

		} while ( pmache );

		__restore_flags( flags );

	} while ( 1 );

	return( 0);
}

/*
 * s390_dequeue_mchchk
 *
 * Dequeue an entry from the machine check queue
 *
 * Note : The queue elements provide for a double linked list.
 *  We dequeue entries from the tail, and enqueue entries to
 *  the head.
 *
 */
static mache_t *s390_dequeue_mchchk( void )
{
	mache_t *qe;

	spin_lock( &mchchk_queue_lock );

	qe = mchchk_queue_tail;

   if ( qe != NULL )
   {
      mchchk_queue_tail = qe->prev;

      if ( mchchk_queue_tail != NULL )
      {
			mchchk_queue_tail->next = NULL;
		}
		else
      {
			mchchk_queue_head = NULL;

      } /* endif */

	} /* endif */

	spin_unlock( &mchchk_queue_lock );

	return qe;
}

/*
 * s390_enqueue_mchchk
 *
 * Enqueue an entry to the machine check queue.
 *
 * Note : The queue elements provide for a double linked list.
 *  We enqueue entries to the head, and dequeue entries from
 *  the tail.
 *
 */
static void s390_enqueue_mchchk( mache_t *pmache )
{
	spin_lock( &mchchk_queue_lock );

	if ( pmache != NULL )
	{

		if ( mchchk_queue_head == NULL )  /* first element */
		{
  			pmache->next      = NULL;
  			pmache->prev      = NULL;

			mchchk_queue_head = pmache;
			mchchk_queue_tail = pmache;
		}
		else /* new head */
		{
  			pmache->prev            = NULL;
			pmache->next            = mchchk_queue_head;

			mchchk_queue_head->prev = pmache;
			mchchk_queue_head       = pmache;

		} /* endif */

	} /* endif */

	spin_unlock( &mchchk_queue_lock );

	return;
}


/*
 * s390_enqueue_free_mchchk
 *
 * Enqueue a free entry to the free queue.
 *
 * Note : While the queue elements provide for a double linked list,
 *  the free queue entries are only concatenated by means of a
 *  single linked list (forward concatenation).
 *
 */
static void s390_enqueue_free_mchchk( mache_t *pmache )
{
	if ( pmache != NULL)
	{
		memset( pmache, '\0', sizeof( mache_t ));

		spin_lock( &mchchk_queue_lock );
		
		pmache->next = mchchk_queue_free;

		mchchk_queue_free = pmache;

		spin_unlock( &mchchk_queue_lock );

	} /* endif */

	return;
}

/*
 * s390_dequeue_free_mchchk
 *
 * Dequeue an entry from the free queue.
 *
 * Note : While the queue elements provide for a double linked list,
 *  the free queue entries are only concatenated by means of a
 *  single linked list (forward concatenation).
 *
 */
static mache_t *s390_dequeue_free_mchchk( void )
{
	mache_t *qe;

	spin_lock( &mchchk_queue_lock );

	qe = mchchk_queue_free;

	if ( qe != NULL )
	{
		mchchk_queue_free = qe->next;

	} /* endif */

	spin_unlock( &mchchk_queue_lock );

	return qe;
}

/*
 * s390_collect_crw_info
 *
 * Retrieve CRWs. If a CRW was found a machine check element
 *  is dequeued from the free chain, filled and enqueued to
 *  be processed.
 *
 * The function returns the number of CRWs found.
 *
 * Note : We must always be called disabled ...
 */
static int s390_collect_crw_info( void )
{
	crw_t    tcrw;     /* temporarily holds a CRW */
	int      ccode;    /* condition code from stcrw() */
	crwe_t  *pcrwe;    /* pointer to CRW buffer entry */

	mache_t *pmache = NULL; /* ptr to mchchk entry */
	int      chain  = 0;    /* indicate chaining */
	crwe_t  *pccrw  = NULL; /* ptr to current CRW buffer entry */
	int      count  = 0;    /* CRW count */

#ifdef S390_MACHCHK_DEBUG
	printk( KERN_DEBUG "crw_info : looking for CRWs ...\n");
#endif

	do
	{
		ccode = stcrw( (__u32 *)&tcrw);

		if ( ccode == 0 )
		{
			count++;
			
#ifdef S390_MACHCHK_DEBUG
			printk( KERN_DEBUG "crw_info : CRW reports "
			        "slct=%d, oflw=%d, chn=%d, "
			        "rsc=%X, anc=%d, erc=%X, "
			        "rsid=%X\n",
			        tcrw.slct,
			        tcrw.oflw,
			        tcrw.chn,
			        tcrw.rsc,
			        tcrw.anc,
			        tcrw.erc,
			        tcrw.rsid );
#endif

			/*
			 * Dequeue a CRW entry from the free chain
			 *  and process it ...
			 */
			spin_lock( &crw_queue_lock );

			pcrwe = crw_buffer_anchor;

			if ( pcrwe == NULL )
			{
				spin_unlock( &crw_queue_lock );
				printk( KERN_CRIT"crw_info : "
				        "no CRW buffer entries available\n");
				break;

			} /* endif */
			
			crw_buffer_anchor = pcrwe->crwe_next;
			pcrwe->crwe_next  = NULL;

			spin_unlock( &crw_queue_lock );

			memcpy( &(pcrwe->crw), &tcrw, sizeof(crw_t));

			/*
			 * If it is the first CRW, chain it to the mchchk
			 *  buffer entry, otherwise to the last CRW entry.
			 */
			if ( chain == 0 )
			{
				pmache = s390_dequeue_free_mchchk();

				if ( pmache != NULL )
				{
					memset( pmache, '\0', sizeof(mache_t));

					pmache->mcic.mcc.mcd.cp = 1;
					pmache->mc.crwe         = pcrwe;
					pccrw                   = pcrwe;

				}
				else
				{
					panic( "crw_info : "
					       "unable to dequeue "
					       "free mchchk buffer");				

				} /* endif */
			}
			else
			{
				pccrw->crwe_next = pcrwe;
				pccrw            = pcrwe;

			} /* endif */	

			if ( pccrw->crw.chn )
			{
#ifdef S390_MACHCHK_DEBUG
				printk( KERN_DEBUG "crw_info : "
				        "chained CRWs pending ...\n\n");
#endif
				chain = 1;
			}
			else
			{
				chain = 0;

				/*
				 * We can enqueue the mchchk buffer if
				 *  there aren't more CRWs chained.
				 */
				s390_enqueue_mchchk( pmache);

			} /* endif */

		} /* endif */

	} while ( ccode == 0 );

	return( count );
}

#ifdef CONFIG_MACHCHK_WARNING
/*
 * s390_post_warning
 *
 * Post a warning type machine check
 *
 * The function returns 1 when succesfull (panics otherwise)
 */
static int s390_post_warning( void )
{
	mache_t  *pmache = NULL; /* ptr to mchchk entry */

	pmache = s390_dequeue_free_mchchk();

	if ( pmache != NULL )
	{
		memset( pmache, '\0', sizeof(mache_t) );

		pmache->mcic.mcc.mcd.w = 1;

		s390_enqueue_mchchk( pmache );
	}
	else
	{
		panic( 	"post_warning : "
			"unable to dequeue "
			"free mchchk buffer" );
	} /* endif */

#ifdef S390_MACHCHK_DEBUG
	printk( KERN_DEBUG "post_warning : 1 warning machine check posted\n");
#endif

	return ( 1 );
}
#endif
