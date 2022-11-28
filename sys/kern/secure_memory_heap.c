#include <sys/secure_memory_heap.h>

/**  Durand's Amazing Super Duper Memory functions.  */

#define VERSION 	"1.1"
#define ALIGNMENT	16ul//4ul				///< This is the byte alignment that memory must be allocated on. IMPORTANT for GTK and other stuff.

#define ALIGN_TYPE		char ///unsigned char[16] /// unsigned short
#define ALIGN_INFO		sizeof(ALIGN_TYPE)*16	///< Alignment information is stored right before the pointer. This is the number of bytes of information stored there.


#define USE_CASE1
#define USE_CASE2
#define USE_CASE3
#define USE_CASE4
#define USE_CASE5

#define UNALIGN( ptr )													\
		if ( ALIGNMENT > 1 )											\
		{																\
			uintptr_t diff = *((ALIGN_TYPE*)((uintptr_t)ptr - ALIGN_INFO));	\
			if ( diff < (ALIGNMENT + ALIGN_INFO) )						\
			{															\
				ptr = (void*)((uintptr_t)ptr - diff);					\
			}															\
		}
				


#define LIBALLOC_MAGIC	0xc001c0de
#define LIBALLOC_DEAD	0xdeaddead

/** A structure found at the top of all system allocated 
 * memory blocks. It details the usage of the memory block.
 */
struct liballoc_major
{
	struct liballoc_major *prev;		///< Linked list information.
	struct liballoc_major *next;		///< Linked list information.
	unsigned int pages;					///< The number of pages in the block.
	unsigned int size;					///< The number of pages in the block.
	unsigned int usage;					///< The number of bytes used in the block.
	struct liballoc_minor *first;		///< A pointer to the first allocated memory in the block.	
};


/** This is a structure found at the beginning of all
 * sections in a major block which were allocated by a
 * malloc, calloc, realloc call.
 */
struct	liballoc_minor
{
	struct liballoc_minor *prev;		///< Linked list information.
	struct liballoc_minor *next;		///< Linked list information.
	struct liballoc_major *block;		///< The owning block. A pointer to the major structure.
	unsigned int magic;					///< A magic number to idenfity correctness.
	unsigned int size; 					///< The size of the memory allocated. Could be 1 byte or more.
	unsigned int req_size;				///< The size of memory requested.
};

static unsigned int l_pageSize  = PAGE_SIZE;			///< The size of an individual page. Set up in liballoc_init.
static unsigned int l_pageCount = 16;			///< The number of pages to request per chunk. Set up in liballoc_init.
static unsigned long long l_allocated = 0;		///< Running total of allocated memory.
static unsigned long long l_inuse	 = 0;		///< Running total of used memory.


static long long l_warningCount = 0;		///< Number of warnings encountered
static long long l_errorCount = 0;			///< Number of actual errors
static long long l_possibleOverruns = 0;	///< Number of possible overruns





// ***********   HELPER FUNCTIONS  *******************************

static void *liballoc_memset(void* s, int c, size_t n)
{
	unsigned int i;
	for ( i = 0; i < n ; i++)
		((char*)s)[i] = c;
	
	return s;
}
static void* liballoc_memcpy(void* s1, const void* s2, size_t n)
{
  char *cdest;
  const char *csrc;
  unsigned int *ldest = (unsigned int*)s1;
  const unsigned int *lsrc  = (const unsigned int*)s2;

  while ( n >= sizeof(unsigned int) )
  {
      *ldest++ = *lsrc++;
	  n -= sizeof(unsigned int);
  }

  cdest = (char*)ldest;
  csrc  = (const char*)lsrc;
  
  while ( n > 0 )
  {
      *cdest++ = *csrc++;
	  n -= 1;
  }
  
  return s1;
}
 

// ***************************************************************

static struct liballoc_major *allocate_new_page( secure_memory_heap_t smh, unsigned int size )
{
	unsigned int st;
	struct liballoc_major *maj;

    // This is how much space is required.
    st  = size + sizeof(struct liballoc_major);
    st += sizeof(struct liballoc_minor);

            // Perfect amount of space?
    if ( (st % l_pageSize) == 0 )
        st  = st / (l_pageSize);
    else
        st  = st / (l_pageSize) + 1;
                        // No, add the buffer. 


    // Make sure it's >= the minimum size.
    if ( st < l_pageCount ) st = l_pageCount;

    maj = (struct liballoc_major*)smh_page_alloc( smh, st );

    if ( maj == NULL ) 
    {
        l_warningCount += 1;
        return NULL;	// uh oh, we ran out of memory.
    }

    maj->prev 	= NULL;
    maj->next 	= NULL;
    maj->pages 	= st;
    maj->size 	= st * l_pageSize;
    maj->usage 	= sizeof(struct liballoc_major);
    maj->first 	= NULL;

    l_allocated += maj->size;		
    return maj;
}


	


void *PREFIX(malloc)(secure_memory_heap_t smh, size_t req_size)
{
	int startedBet = 0;
	unsigned long long bestSize = 0;
	void *p = NULL;
	uintptr_t diff;
	struct liballoc_major *maj;
	struct liballoc_minor *min;
	struct liballoc_minor *new_min;
	unsigned long size = req_size;

	// For alignment, we adjust size so there's enough space to align.
	if ( ALIGNMENT > 1 )
	{
		size += ALIGNMENT + ALIGN_INFO;
	}
				// So, ideally, we really want an alignment of 0 or 1 in order
				// to save space.
	
	smh_lock( smh );

	if ( size == 0 )
	{
		l_warningCount += 1;
		smh_unlock( smh );
		return NULL;
	}
	

	if ( smh->l_memRoot == NULL )
	{			
		// This is the first time we are being used.
		smh->l_memRoot = allocate_new_page( smh, size );
		if ( smh->l_memRoot == NULL )
		{
		  smh_unlock( smh );
		  return NULL;
		}
	}
	// Now we need to bounce through every major and find enough space....

	maj = smh->l_memRoot;
	startedBet = 0;
	
	// Start at the best bet....
	if ( smh->l_bestBet != NULL )
	{
		bestSize = smh->l_bestBet->size - smh->l_bestBet->usage;

		if ( bestSize > (size + sizeof(struct liballoc_minor)))
		{
			maj = smh->l_bestBet;
			startedBet = 1;
		}
	}
	
	while ( maj != NULL )
	{
		diff  = maj->size - maj->usage;	
										// free memory in the block

		if ( bestSize < diff )
		{
			// Hmm.. this one has more memory then our bestBet. Remember!
			smh->l_bestBet = maj;
			bestSize = diff;
		}
		
		
#ifdef USE_CASE1
			
		// CASE 1:  There is not enough space in this major block.
		if ( diff < (size + sizeof( struct liballoc_minor )) )
		{
				// Another major block next to this one?
			if ( maj->next != NULL ) 
			{
				maj = maj->next;		// Hop to that one.
				continue;
			}

			if ( startedBet == 1 )		// If we started at the best bet,
			{							// let's start all over again.
				maj = smh->l_memRoot;
				startedBet = 0;
				continue;
			}

			// Create a new major block next to this one and...
			maj->next = allocate_new_page( smh, size );	// next one will be okay.
			if ( maj->next == NULL ) break;			// no more memory.
			maj->next->prev = maj;
			maj = maj->next;

			// .. fall through to CASE 2 ..
		}

#endif

#ifdef USE_CASE2
		
		// CASE 2: It's a brand new block.
		if ( maj->first == NULL )
		{
			maj->first = (struct liballoc_minor*)((uintptr_t)maj + sizeof(struct liballoc_major) );

			
			maj->first->magic 		= LIBALLOC_MAGIC;
			maj->first->prev 		= NULL;
			maj->first->next 		= NULL;
			maj->first->block 		= maj;
			maj->first->size 		= size;
			maj->first->req_size 	= req_size;
			maj->usage 	+= size + sizeof( struct liballoc_minor );


			l_inuse += size;
			
			
			p = (void*)((uintptr_t)(maj->first) + sizeof( struct liballoc_minor ));

			ALIGN( p );			
			smh_unlock( smh );		// release the lock
			return p;
		}

#endif
				
#ifdef USE_CASE3

		// CASE 3: Block in use and enough space at the start of the block.
		diff =  (uintptr_t)(maj->first);
		diff -= (uintptr_t)maj;
		diff -= sizeof(struct liballoc_major);

		if ( diff >= (size + sizeof(struct liballoc_minor)) )
		{
			// Yes, space in front. Squeeze in.
			maj->first->prev = (struct liballoc_minor*)((uintptr_t)maj + sizeof(struct liballoc_major) );
			maj->first->prev->next = maj->first;
			maj->first = maj->first->prev;
				
			maj->first->magic 	= LIBALLOC_MAGIC;
			maj->first->prev 	= NULL;
			maj->first->block 	= maj;
			maj->first->size 	= size;
			maj->first->req_size 	= req_size;
			maj->usage 			+= size + sizeof( struct liballoc_minor );

			l_inuse += size;

			p = (void*)((uintptr_t)(maj->first) + sizeof( struct liballoc_minor ));
			ALIGN( p );
			smh_unlock( smh );		// release the lock
			return p;
		}
		
#endif


#ifdef USE_CASE4

		// CASE 4: There is enough space in this block. But is it contiguous?
		min = maj->first;
		
			// Looping within the block now...
		while ( min != NULL )
		{
				// CASE 4.1: End of minors in a block. Space from last and end?
				if ( min->next == NULL )
				{
					// the rest of this block is free...  is it big enough?
					diff = (uintptr_t)(maj) + maj->size;
					diff -= (uintptr_t)min;
					diff -= sizeof( struct liballoc_minor );
					diff -= min->size; 
						// minus already existing usage..

					if ( diff >= (size + sizeof( struct liballoc_minor )) )
					{
						// yay....
						min->next = (struct liballoc_minor*)((uintptr_t)min + sizeof( struct liballoc_minor ) + min->size);
						min->next->prev = min;
						min = min->next;
						min->next = NULL;
						min->magic = LIBALLOC_MAGIC;
						min->block = maj;
						min->size = size;
						min->req_size = req_size;
						maj->usage += size + sizeof( struct liballoc_minor );

						l_inuse += size;
						
						p = (void*)((uintptr_t)min + sizeof( struct liballoc_minor ));
						ALIGN( p );

						smh_unlock( smh );		// release the lock
						return p;
					}
				}



				// CASE 4.2: Is there space between two minors?
				if ( min->next != NULL )
				{
					// is the difference between here and next big enough?
					diff  = (uintptr_t)(min->next);
					diff -= (uintptr_t)min;
					diff -= sizeof( struct liballoc_minor );
					diff -= min->size;
										// minus our existing usage.

					if ( diff >= (size + sizeof( struct liballoc_minor )) )
					{
						// yay......
						new_min = (struct liballoc_minor*)((uintptr_t)min + sizeof( struct liballoc_minor ) + min->size);

						new_min->magic = LIBALLOC_MAGIC;
						new_min->next = min->next;
						new_min->prev = min;
						new_min->size = size;
						new_min->req_size = req_size;
						new_min->block = maj;
						min->next->prev = new_min;
						min->next = new_min;
						maj->usage += size + sizeof( struct liballoc_minor );
						
						l_inuse += size;
						
						p = (void*)((uintptr_t)new_min + sizeof( struct liballoc_minor ));
						ALIGN( p );
						
						smh_unlock( smh );		// release the lock
						return p;
					}
				}	// min->next != NULL

				min = min->next;
		} // while min != NULL ...


#endif

#ifdef USE_CASE5

		// CASE 5: Block full! Ensure next block and loop.
		if ( maj->next == NULL ) 
		{

			if ( startedBet == 1 )
			{
				maj = smh->l_memRoot;
				startedBet = 0;
				continue;
			}
				
			// we've run out. we need more...
			maj->next = allocate_new_page( smh, size );		// next one guaranteed to be okay
			if ( maj->next == NULL ) break;			//  uh oh,  no more memory.....
			maj->next->prev = maj;

		}

#endif

		maj = maj->next;
	} // while (maj != NULL)


	
	smh_unlock( smh );		// release the lock
	return NULL;
}

void PREFIX(free)(secure_memory_heap_t smh, void *ptr)
{
	struct liballoc_minor *min;
	struct liballoc_major *maj;

	if ( ptr == NULL ) 
	{
		l_warningCount += 1;
		return;
	}

	UNALIGN( ptr );

	smh_lock( smh );		// lockit


	min = (struct liballoc_minor*)((uintptr_t)ptr - sizeof( struct liballoc_minor ));

	
	if ( min->magic != LIBALLOC_MAGIC ) 
	{
		l_errorCount += 1;

		// Check for overrun errors. For all bytes of LIBALLOC_MAGIC 
		if ( 
			((min->magic & 0xFFFFFF) == (LIBALLOC_MAGIC & 0xFFFFFF)) || 
			((min->magic & 0xFFFF) == (LIBALLOC_MAGIC & 0xFFFF)) || 
			((min->magic & 0xFF) == (LIBALLOC_MAGIC & 0xFF)) 
		   )
		{
			l_possibleOverruns += 1;
		}
						

		
		// being lied to...
		smh_unlock( smh );		// release the lock
        panic("Bad magic...");
		return;
	}
	

		maj = min->block;

		l_inuse -= min->size;

		maj->usage -= (min->size + sizeof( struct liballoc_minor ));
		min->magic  = LIBALLOC_DEAD;		// No mojo.

		if ( min->next != NULL ) min->next->prev = min->prev;
		if ( min->prev != NULL ) min->prev->next = min->next;

		if ( min->prev == NULL ) maj->first = min->next;	
							// Might empty the block. This was the first
							// minor.


	// We need to clean up after the majors now....

	if ( maj->first == NULL )	// Block completely unused.
	{
		if ( smh->l_memRoot == maj ) smh->l_memRoot = maj->next;
		if ( smh->l_bestBet == maj ) smh->l_bestBet = NULL;
		if ( maj->prev != NULL ) maj->prev->next = maj->next;
		if ( maj->next != NULL ) maj->next->prev = maj->prev;
		l_allocated -= maj->size;

		smh_page_free( smh, maj, maj->pages );
	}
	else
	{
		if ( smh->l_bestBet != NULL )
		{
			int bestSize = smh->l_bestBet->size  - smh->l_bestBet->usage;
			int majSize = maj->size - maj->usage;

			if ( majSize > bestSize ) smh->l_bestBet = maj;
		}

	}
	
	smh_unlock( smh );		// release the lock
}





void* PREFIX(calloc)(secure_memory_heap_t smh, size_t nobj, size_t size)
{
       int real_size;
       void *p;

       real_size = nobj * size;
       
       p = PREFIX(malloc)( smh, real_size );

       liballoc_memset( p, 0, real_size );

       return p;
}



void*   PREFIX(realloc)(secure_memory_heap_t smh, void *p, size_t size)
{
	void *ptr;
	struct liballoc_minor *min;
	unsigned int real_size;
	
	// Honour the case of size == 0 => free old and return NULL
	if ( size == 0 )
	{
		PREFIX(free)( smh, p );
		return NULL;
	}

	// In the case of a NULL pointer, return a simple malloc.
	if ( p == NULL ) return PREFIX(malloc)( smh, size );

	// Unalign the pointer if required.
	ptr = p;
	UNALIGN(ptr);

	smh_lock( smh );		// lockit

		min = (struct liballoc_minor*)((uintptr_t)ptr - sizeof( struct liballoc_minor ));

		// Ensure it is a valid structure.
		if ( min->magic != LIBALLOC_MAGIC ) 
		{
			l_errorCount += 1;
            // being lied to...
			smh_unlock( smh );		// release the lock
	
			// Check for overrun errors. For all bytes of LIBALLOC_MAGIC 
			if ( 
				((min->magic & 0xFFFFFF) == (LIBALLOC_MAGIC & 0xFFFFFF)) || 
				((min->magic & 0xFFFF) == (LIBALLOC_MAGIC & 0xFFFF)) || 
				((min->magic & 0xFF) == (LIBALLOC_MAGIC & 0xFF)) 
			   )
			{
				panic("Heap corruption: overrun");
			}
							
							
			if ( min->magic == LIBALLOC_DEAD )
			{
				panic("Heap corruption: double free");
			}
			else
			{
				panic("Heap corruption: invalid magic");
			}
			
			return NULL;
		}	
		
		// Definitely a memory block.
		
		real_size = min->req_size;

		if ( real_size >= size ) 
		{
			min->req_size = size;
			smh_unlock( smh );
			return p;
		}

	smh_unlock( smh );

	// If we got here then we're reallocating to a block bigger than us.
	ptr = PREFIX(malloc)( smh, size );					// We need to allocate new memory
	liballoc_memcpy( ptr, p, real_size );
	PREFIX(free)( smh, p );

	return ptr;
}


void smh_init(secure_memory_heap_t smh, const char *name,
			  vm_offset_t base, vm_size_t size) {
    mtx_init(&smh->mtx, "submap heap mutex", NULL, MTX_DEF);
	smh->vmem = vmem_create(name, base, size, PAGE_SIZE, 0, M_WAITOK);
    smh->l_memRoot = NULL;
    smh->l_bestBet = NULL;
}


/** This function is supposed to lock the memory data structures. It
 * could be as simple as disabling interrupts or acquiring a spinlock.
 * It's up to you to decide. 
 *
 * \return 0 if the lock was acquired successfully. Anything else is
 * failure.
 */
int smh_lock(secure_memory_heap_t smh) {
    mtx_lock(&smh->mtx);
    return 0;
}

/** This function unlocks what was previously locked by the smh_lock
 * function.  If it disabled interrupts, it enables interrupts. If it
 * had acquiried a spinlock, it releases the spinlock. etc.
 *
 * \return 0 if the lock was successfully released.
 */
int smh_unlock(secure_memory_heap_t smh) {
    mtx_unlock(&smh->mtx);
    return 0;
}

/** This is the hook into the local system which allocates pages. It
 * accepts an integer parameter which is the number of pages
 * required.  The page size was set up in the liballoc_init function.
 *
 * \return NULL if the pages were not allocated.
 * \return A pointer to the allocated memory.
 */
void* smh_page_alloc(secure_memory_heap_t smh, size_t pg_count) {
	vm_offset_t addr;
	vm_size_t size;

	size = pg_count * PAGE_SIZE;
	if(vmem_alloc(smh->vmem, size, M_BESTFIT, &addr)) {
		return (0);
	}

	if (kmem_back(kernel_object, addr, size, M_WAITOK | M_ZERO)) {
		vmem_free(smh->vmem, addr, size);
		return (0);
	}

	return (void *)addr;
}

/** This frees previously allocated memory. The void* parameter passed
 * to the function is the exact same value returned from a previous
 * smh_page_alloc call.
 *
 * The integer value is the number of pages to free.
 *
 * \return 0 if the memory was successfully freed.
 */
int smh_page_free(secure_memory_heap_t smh, void *ptr, size_t pg_count) {
	vm_offset_t addr;
	vm_size_t size;
	addr = (vm_offset_t)ptr;
	size = pg_count * PAGE_SIZE;

	kmem_unback(kernel_object, addr, size);
	vmem_free(smh->vmem, addr, size);

    return 0;
}