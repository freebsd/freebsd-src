/*
 *
 * ===================================
 * HARP  |  Host ATM Research Platform
 * ===================================
 *
 *
 * This Host ATM Research Platform ("HARP") file (the "Software") is
 * made available by Network Computing Services, Inc. ("NetworkCS")
 * "AS IS".  NetworkCS does not provide maintenance, improvements or
 * support of any kind.
 *
 * NETWORKCS MAKES NO WARRANTIES OR REPRESENTATIONS, EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE, AS TO ANY ELEMENT OF THE
 * SOFTWARE OR ANY SUPPORT PROVIDED IN CONNECTION WITH THIS SOFTWARE.
 * In no event shall NetworkCS be responsible for any damages, including
 * but not limited to consequential damages, arising from or relating to
 * any use of the Software or related support.
 *
 * Copyright 1994-1998 Network Computing Services, Inc.
 *
 * Copies of this Software may be made, however, the above copyright
 * notice must be reproduced on all copies.
 *
 *	@(#) $FreeBSD$
 *
 */

/*
 * Efficient ENI Adapter Support
 * -----------------------------
 *
 * Handle adapter memory buffers for ENI adapters
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/malloc.h>

#include <net/if.h>

#include <netatm/port.h>
#include <netatm/queue.h>
#include <netatm/atm.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_cm.h>
#include <netatm/atm_if.h>

#include <dev/hea/eni_stats.h>
#include <dev/hea/eni.h>
#include <dev/hea/eni_var.h>

#ifndef lint
__RCSID("@(#) $FreeBSD$");
#endif

static int	eni_test_memory(Eni_unit *);

/*
 * The host is going to manage (that is, allocate and free) buffers
 * in the adapters RAM space. We are going to implement this as a
 * linked list describing FREE and INUSE memory segments. Initially,
 * the list contains one element with all memory marked free. As requests
 * are made, we search the list until we find the first free element
 * which can satisfy the request. If necessary, we will break the free
 * element into an INUSE element, and a new FREE element. When freeing
 * memory, we look at adjacent elements and if one or more are free,
 * we will combine into a single larger FREE element.
 */

/*
 * This is for testing purposes. Since there are two versions of
 * the Efficient adapter with different memory sizes, this allows
 * us to fool an adapter with more memory into thinking it has less.
 */
static int eni_mem_max = MAX_ENI_MEM;	/* Default to all available memory */

/*
 * Size and test adapter RAM
 *
 * Walk through adapter RAM writing known patterns and reading back
 * for comparison. We write more than one pattern on the off chance
 * that we "get lucky" and read what we expected.
 *
 * Arguments:
 *	eup		pointer to device unit structure
 *
 * Returns
 *	size		memory size in bytes
 */
static int
eni_test_memory ( eup )
	Eni_unit *eup;
{
	int	ram_size = 0;
	int	i;
	Eni_mem	mp;

	/*
	 * Walk through to maximum looking for RAM
	 */
	for ( i = 0; i < MAX_ENI_MEM; i += TEST_STEP ) {
		mp = (Eni_mem)((intptr_t)eup->eu_ram + i);
		/* write pattern */
		*mp = (u_long)TEST_PAT;
		/* read pattern, match? */
		if ( *mp == (u_long)TEST_PAT ) {
			/* yes - write inverse pattern */
			*mp = (u_long)~TEST_PAT;
			/* read pattern, match? */
			if ( *mp == (u_long)~TEST_PAT ) {
				/* yes - assume another 1K available */
				ram_size = i + TEST_STEP;
			} else
			    break;
		} else
			break;
	}
	/*
	 * Clear all RAM to initial value of zero.
	 * This makes sure we don't leave anything funny in the
	 * queues.
	 */
	bzero ( (void *)(uintptr_t)eup->eu_ram, ram_size );

	/*
	 * If we'd like to claim to have less memory, here's where
	 * we do so. We take the minimum of what we'd like and what
	 * we really found on the adapter.
	 */
	ram_size = MIN ( ram_size, eni_mem_max ); 

	return ( ram_size );

}

/*
 * Initialize our memory allocator.
 *
 * Arguments:
 *	eup		Pointer to per unit structure
 *
 * Returns:
 *	size		Physical RAM size
 *	-1		failed to initialize memory
 *
 */
int
eni_init_memory ( eup )
	Eni_unit *eup;
{

	/*
	 * Have we (somehow) been called before?
	 */
	if ( eup->eu_memmap != NULL )
	{
		/* Oops  - it's already been initialized */
		return -1;
	}

	/*
	 * Allocate initial element which will hold all of memory
	 */
	eup->eu_memmap = malloc(sizeof(Mbd), M_DEVBUF, M_NOWAIT);
	if (eup->eu_memmap == NULL)
		return (-1);

	/*
	 * Test and size memory
	 */
	eup->eu_ramsize = eni_test_memory ( eup );

	/*
	 * Initialize a one element list which contains
	 * all buffer memory
	 */
	eup->eu_memmap->prev = eup->eu_memmap->next = NULL;
	eup->eu_memmap->base = (caddr_t)SEGBUF_BASE;
	eup->eu_memmap->size = eup->eu_ramsize - SEGBUF_BASE;
	eup->eu_memmap->state = MEM_FREE;

	return ( eup->eu_ramsize );
}

/*
 * Allocate a buffer from adapter RAM. Due to constraints on the card,
 * we may roundup the size request to the next largest chunksize. Note
 * also that we must pay attention to address alignment within adapter
 * memory as well.
 *
 * Arguments:
 *	eup		pointer to per unit structure
 *	size		pointer to requested size - in bytes
 *
 * Returns:
 *	addr		address relative to adapter of allocated memory
 *	size		modified to reflect actual size of buffer
 *
 */
caddr_t
eni_allocate_buffer ( eup, size )
	Eni_unit *eup;
	u_long	*size;
{
	int	nsize;
	int	nclicks;
	Mbd	*eptr = eup->eu_memmap;

	/*
	 * Initial size requested
	 */
	nsize = *size;

	/*
	 * Find the buffer size which will hold this request. There
	 * are 8 possible sizes, each a power of two up, starting at
	 * 256 words or 1024 bytes.
	 */
	for ( nclicks = 0; nclicks < ENI_BUF_NBIT; nclicks++ )
		if ( ( 1 << nclicks ) * ENI_BUF_PGSZ >= nsize )
			break;

	/*
	 * Request was for larger then the card supports
	 */
	if ( nclicks >= ENI_BUF_NBIT ) {
		eup->eu_stats.eni_st_drv.drv_mm_toobig++;
		/* Indicate 0 bytes allocated */
		*size = 0;
		/* Return NULL buffer */
		return ( (caddr_t)NULL );
	}

	/*
	 * New size will be buffer size
	 */
	nsize = ( 1 << nclicks ) * ENI_BUF_PGSZ;

	/*
	 * Look through memory for a segment large enough to
	 * hold request
	 */
	while ( eptr ) {
	    /*
	     * State must be FREE and size must hold request
	     */
	    if ( eptr->state == MEM_FREE && eptr->size >= nsize )
	    {
		/*
		 * Request will fit - now check if the
		 * alignment needs fixing
		 */
		if ( ((uintptr_t)eptr->base & (nsize-1)) != 0 )
		{
		    caddr_t	nbase;

		    /*
		     * Calculate where the buffer would have to
		     * fall to be aligned.
		     */
		    nbase = (caddr_t)((uintptr_t)( eptr->base + nsize ) &
		        ~(nsize-1));
		    /*
		     * If we use this alignment, will it still fit?
		     */
		    if ( (eptr->size - (nbase - eptr->base)) >= 0 )
		    {
			Mbd	*etmp;

			/* Yep - create a new segment */
			etmp = malloc(sizeof(Mbd), M_DEVBUF, M_NOWAIT);
			if (etmp == NULL) {
				/*
				 * Couldn't allocate a new descriptor. Indicate 
				 * failure and exit now or we'll start losing
				 * memory.
				 */
				eup->eu_stats.eni_st_drv.drv_mm_nodesc++;
				*size = 0;
				return (NULL);
			}
			/* Place it in the list */
			etmp->next = eptr->next;
			if ( etmp->next )
			    etmp->next->prev = etmp;
			etmp->prev = eptr;
			eptr->next = etmp;
			/* Fill in new base and size */
			etmp->base = nbase;
			etmp->size = eptr->size - ( nbase - eptr->base );
			/* Adjust old size */
			eptr->size -= etmp->size;
			/* Mark its state */
			etmp->state = MEM_FREE;
			eptr = etmp;
			/* Done - outa here */
			break;
		    }
		} else
		    break;		/* Alignment is okay  - we're done */
	    }
	    /* Haven't found anything yet - keep looking */
	    eptr = eptr->next;
	}

	if ( eptr != NULL )
	{
	    /* Found a usable segment - grab what we need */
	    /* Exact fit? */
	    if ( eptr->size == nsize )
		/* Mark it as INUSE */
		eptr->state = MEM_INUSE;
	    else
	    {
		Mbd	*etmp;
		/* larger then we need - split it */

		etmp = (Mbd *)malloc(sizeof(Mbd), M_DEVBUF, M_NOWAIT);
		if ( etmp == (Mbd *)NULL ) {
			/*
			 * Couldn't allocate new descriptor. Indicate
			 * failure and exit now or we'll start losing
			 * memory.
			 */
			eup->eu_stats.eni_st_drv.drv_mm_nodesc++;
			*size = 0;
			return ( (caddr_t)NULL );
		}
		/* Place new element in list */
		etmp->next = eptr->next;
		if ( etmp->next )
		    etmp->next->prev = etmp;
		etmp->prev = eptr;
		eptr->next = etmp;
		/* Set new base, size and state */
		etmp->base = eptr->base + nsize;
		etmp->size = eptr->size - nsize;
		etmp->state = MEM_FREE;
		/* Adjust size and state of element we intend to use */
		eptr->size = nsize;
		eptr->state = MEM_INUSE;
	    }
	}

	/* After all that, did we find a usable buffer? */
	if ( eptr )
	{
		/* Record another inuse buffer of this size */
		if ( eptr->base )
			eup->eu_memclicks[nclicks]++;

		/*
		 * Return true size of allocated buffer
		 */
		*size = eptr->size;
		/*
		 * Make address relative to start of RAM since
		 * its (the address) for use by the adapter, not
		 * the host.
		 */
		return ((caddr_t)eptr->base);
	} else {
		eup->eu_stats.eni_st_drv.drv_mm_nobuf++;
		/* No buffer to return - indicate zero length */
		*size = 0;
		/* Return NULL buffer */
		return ( (caddr_t)NULL );
	}
}

/*
 * Procedure to release a buffer previously allocated from adapter
 * RAM. When possible, we'll compact memory.
 *
 * Arguments:
 *	eup		pointer to per unit structure
 *	base		base adapter address of buffer to be freed
 *
 * Returns:
 *	none
 *
 */
void
eni_free_buffer ( eup, base )
	Eni_unit *eup;
	caddr_t	base;
{
	Mbd	*eptr = eup->eu_memmap;
	int	nclicks;

	/* Look through entire list */
	while ( eptr )
	{
		/* Is this the buffer to be freed? */
		if ( eptr->base == base )
		{
			/*
			 * We're probably asking for trouble but,
			 * assume this is it.
			 */
			if ( eptr->state != MEM_INUSE )
			{
				eup->eu_stats.eni_st_drv.drv_mm_notuse++;
				/* Huh? Something's wrong */
				return;
			}
			/* Reset state to FREE */
			eptr->state = MEM_FREE;

			/* Determine size for stats info */
			for ( nclicks = 0; nclicks < ENI_BUF_NBIT; nclicks++ )
			    if ( ( 1 << nclicks ) * ENI_BUF_PGSZ == eptr->size )
				break;

			/* Valid size? Yes - decrement inuse count */
			if ( nclicks < ENI_BUF_NBIT )
				eup->eu_memclicks[nclicks]--;

			/* Try to compact neighbors */
			/* with previous */
			if ( eptr->prev )
			    if ( eptr->prev->state == MEM_FREE )
			    {
				Mbd	*etmp = eptr;
				/* Add to previous block */
				eptr->prev->size += eptr->size;
				/* Set prev block to skip this one */
				eptr->prev->next = eptr->next;
				/* Set next block to skip this one */
				if ( eptr->next )
					eptr->next->prev = eptr->prev;
				/* Reset to where we want to be */
				eptr = eptr->prev;
				/* and free this element */
				free(etmp, M_DEVBUF);
			    }
			/* with next */
			if ( eptr->next )
			    if ( eptr->next->state == MEM_FREE )
			    {
				Mbd	*etmp = eptr->next;

				/* add following block in */
				eptr->size += etmp->size;
				/* set next next block to skip next block */
				if ( etmp->next )
					etmp->next->prev = eptr;
				/* skip next block */
				eptr->next = etmp->next;
				/* and free next element */
				free(etmp, M_DEVBUF);
			    }
			/*
			 * We've freed the buffer and done any compaction,
			 * we needn't look any further...
			 */
			return;
		}
		eptr = eptr->next;
	}

	if ( eptr == NULL )
	{
		/* Oops - failed to find the buffer. This is BAD */
		eup->eu_stats.eni_st_drv.drv_mm_notfnd++;
	}

}

