/* 
 * File...........: linux/drivers/s390/ccwcache.c
 * Author(s)......: Holger Smolinski <Holger.Smolinski@de.ibm.com>
 *                  Martin Schiwdefsky <schwidefsky@de.ibm.com>
 * Bugreports.to..: <Linux390@de.ibm.com>
 * (C) IBM Corporation, IBM Deutschland Entwicklung GmbH, 2000a
 
 * History of changes
 * 11/14/00 redesign by Martin Schwidefsky
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/version.h>

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,3,98))
#include <linux/spinlock.h>
#else
#include <asm/spinlock.h>
#endif

#include <asm/debug.h>
#include <asm/ccwcache.h>
#include <asm/ebcdic.h>

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,3,98))
#define CCW_CACHE_SLAB_TYPE (SLAB_HWCACHE_ALIGN | SLAB_CACHE_DMA)
#define CCW_CACHE_TYPE (GFP_ATOMIC | GFP_DMA)
#else
#define CCW_CACHE_SLAB_TYPE (SLAB_HWCACHE_ALIGN)
#define CCW_CACHE_TYPE (GFP_ATOMIC)
#define kmem_cache_destroy(x) do {} while(0)
#endif

#undef PRINTK_HEADER
#define PRINTK_HEADER "ccwcache"

/* pointer to list of allocated requests */
static ccw_req_t *ccwreq_actual = NULL;
static spinlock_t ccwchain_lock;

/* pointer to debug area */
static debug_info_t *debug_area = NULL;

/* SECTION: Handling of the dynamically allocated kmem slabs */

/* a name template for the cache-names */
static char ccw_name_template[] = "ccwcache-\0\0\0\0"; /* fill name with zeroes! */
/* the cache's names */
static char ccw_cache_name[CCW_NUMBER_CACHES][sizeof(ccw_name_template)+1]; 
/* the caches itself*/
static kmem_cache_t *ccw_cache[CCW_NUMBER_CACHES]; 

/* SECTION: (de)allocation of ccw_req_t */

/* 
 * void enchain ( ccw_req_t *request )
 * enchains the request to the ringbuffer
 */
static inline void 
enchain ( ccw_req_t *request )
{
	unsigned long flags;

	/* Sanity checks */
	if ( request == NULL )
		BUG();
	spin_lock_irqsave(&ccwchain_lock,flags);
	if ( ccwreq_actual == NULL ) { /* queue empty */
		ccwreq_actual = request;
		request->int_prev = ccwreq_actual;
		request->int_next = ccwreq_actual;
	} else {
		request->int_next = ccwreq_actual;
		request->int_prev = ccwreq_actual->int_prev;
		request->int_prev->int_next = request;
		request->int_next->int_prev = request;
	}
	spin_unlock_irqrestore(&ccwchain_lock,flags);
}

/* 
 * void dechain ( ccw_req_t *request )
 * dechains the request from the ringbuffer
 */
static inline void 
dechain ( ccw_req_t *request )
{
	unsigned long flags;

	/* Sanity checks */
	if ( request == NULL || 
	     request->int_next == NULL ||
	     request->int_prev == NULL)
		BUG();
	/* first deallocate request from list of allocates requests */
	spin_lock_irqsave(&ccwchain_lock,flags);
	if ( request -> int_next == request -> int_prev ) {
		ccwreq_actual = NULL;
	} else {
		if ( ccwreq_actual == request ) {
			ccwreq_actual = request->int_next;
		}
		request->int_prev->int_next = request->int_next;
		request->int_next->int_prev = request->int_prev;
	}
	spin_unlock_irqrestore(&ccwchain_lock,flags);
}

/* 
 * ccw_req_t *ccw_alloc_request ( int cplength, int datasize )
 * allocates a ccw_req_t, that 
 * - can hold a CP of cplength CCWS
 * - can hold additional data up to datasize 
 */
ccw_req_t *
ccw_alloc_request ( char *magic, int cplength, int datasize )
{
	ccw_req_t * request = NULL;
        int size_needed;
	int data_offset, ccw_offset;
	int cachind;

	/* Sanity checks */
	if ( magic == NULL || datasize > PAGE_SIZE ||
	     cplength == 0 || (cplength*sizeof(ccw1_t)) > PAGE_SIZE)
		BUG();
	debug_text_event ( debug_area, 1, "ALLC");
	debug_text_event ( debug_area, 1, magic);
	debug_int_event ( debug_area, 1, cplength);
	debug_int_event ( debug_area, 1, datasize);

	/* We try to keep things together in memory */
	size_needed = (sizeof (ccw_req_t) + 7) & -8;
	data_offset = ccw_offset = 0;
	if (size_needed + datasize <= PAGE_SIZE) {
		/* Keep data with the request */
		data_offset = size_needed;
		size_needed += (datasize + 7) & -8;
	}
	if (size_needed + cplength*sizeof(ccw1_t) <= PAGE_SIZE) {
		/* Keep CCWs with request */
		ccw_offset = size_needed;
		size_needed += cplength*sizeof(ccw1_t);
	}

	/* determine cache index for the requested size */
	for (cachind = 0; cachind < CCW_NUMBER_CACHES; cachind ++ )
	   if ( size_needed <= (SMALLEST_SLAB << cachind) ) 
			break;

	/* Try to fulfill the request from a cache */
	if ( ccw_cache[cachind] == NULL )
		BUG();
	request = kmem_cache_alloc ( ccw_cache[cachind], CCW_CACHE_TYPE );
	if (request == NULL)
		return NULL;
	memset ( request, 0, (SMALLEST_SLAB << cachind));
	request->cache = ccw_cache[cachind];

	/* Allocate memory for the extra data */
	if (data_offset == 0) {
		/* Allocated memory for extra data with kmalloc */
	    request->data = (void *) kmalloc(datasize, CCW_CACHE_TYPE );
		if (request->data == NULL) {
			printk(KERN_WARNING PRINTK_HEADER 
			       "Couldn't allocate data area\n");
			kmem_cache_free(request->cache, request);
			return NULL;
		}
	} else
		/* Extra data already allocated with the request */
		request->data = (void *) ((addr_t) request + data_offset);

	/* Allocate memory for the channel program */
	if (ccw_offset == 0) {
		/* Allocated memory for the channel program with kmalloc */
		request->cpaddr = (ccw1_t *) kmalloc(cplength*sizeof(ccw1_t),
						     CCW_CACHE_TYPE);
		if (request->cpaddr == NULL) {
			printk (KERN_DEBUG PRINTK_HEADER
				"Couldn't allocate ccw area\n");
			if (data_offset == 0)
				kfree(request->data);
			kmem_cache_free(request->cache, request);
			return NULL;
		}
	} else
		/* Channel program already allocated with the request */
		request->cpaddr = (ccw1_t *) ((addr_t) request + ccw_offset);

	memset ( request->data, 0, datasize );
	memset ( request->cpaddr, 0, cplength*sizeof(ccw1_t) );
	strncpy ( (char *)(&request->magic), magic, 4);

	ASCEBC((char *)(&request->magic),4);
	request -> cplength = cplength;
	request -> datasize = datasize;
	/* enqueue request to list of allocated requests */
	enchain(request);
	debug_int_event ( debug_area, 1, (long)request);
	return request;
}

/* 
 * void ccw_free_request ( ccw_req_t * )
 * deallocates the ccw_req_t, given as argument
 */

void
ccw_free_request ( ccw_req_t * request )
{
        int size_needed;

	debug_text_event ( debug_area, 1, "FREE");
	debug_int_event ( debug_area, 1, (long)request);

	/* Sanity checks */
	if ( request == NULL || request->cache == NULL)
                BUG();

        dechain ( request);
	/* Free memory allocated with kmalloc
         * make the same decisions as in ccw_alloc_requets */
	size_needed = (sizeof (ccw_req_t) + 7) & -8;
	if (size_needed + request->datasize <= PAGE_SIZE)
		/* We kept the data with the request */
		size_needed += (request->datasize + 7) & -8;
	else
		kfree(request->data);
	if (size_needed + request->cplength*sizeof(ccw1_t) > PAGE_SIZE)
		/* We kept the CCWs with request */
                kfree(request->cpaddr);
        kmem_cache_free(request -> cache, request);
}

/* SECTION: initialization and cleanup functions */

/* 
 * ccwcache_init
 * called as an initializer function for the ccw memory management
 */

int
ccwcache_init (void)
{
	int rc = 0;
	int cachind;

        /* initialize variables */
	spin_lock_init(&ccwchain_lock);

	/* allocate a debug area */
	debug_area = debug_register( "ccwcache", 2, 4,sizeof(void*));
	if ( debug_area == NULL )
                BUG();

        debug_register_view(debug_area,&debug_hex_ascii_view);
        debug_register_view(debug_area,&debug_raw_view);
	debug_text_event ( debug_area, 0, "INIT");

	/* First allocate the kmem caches */
	for ( cachind = 0; cachind < CCW_NUMBER_CACHES; cachind ++ ) {
		int slabsize = SMALLEST_SLAB << cachind;
		debug_text_event ( debug_area, 1, "allc");
		debug_int_event ( debug_area, 1, slabsize);
		sprintf ( ccw_cache_name[cachind], 
			  "%s%d%c", ccw_name_template, slabsize, 0);
		ccw_cache[cachind] = 
			kmem_cache_create( ccw_cache_name[cachind], 
					   slabsize, 0,
					   CCW_CACHE_SLAB_TYPE,
					   NULL, NULL );
		debug_int_event ( debug_area, 1, (long)ccw_cache[cachind]);
		if (ccw_cache[cachind] == NULL)
			panic ("Allocation of CCW cache failed\n");
	}
	return rc;
}

/* 
 * ccwcache_cleanup
 * called as a cleanup function for the ccw memory management
 */

void
ccwcache_cleanup (void)
{
	int cachind;

	/* Shrink the caches, if available */
	for ( cachind = 0; cachind < CCW_NUMBER_CACHES; cachind ++ ) {
		if ( ccw_cache[cachind] ) {
#if 0 /* this is useless and could cause an OOPS in the worst case */
			if ( kmem_cache_shrink(ccw_cache[cachind]) == 0 ) {
				ccw_cache[cachind] = NULL;
			}
#endif
			kmem_cache_destroy(ccw_cache[cachind]);
		}
	}
	debug_unregister( debug_area );
}

EXPORT_SYMBOL(ccw_alloc_request);
EXPORT_SYMBOL(ccw_free_request);

