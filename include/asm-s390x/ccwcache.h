/* 
 * File...........: linux/include/asm-s390/ccwcache.h
 * Author(s)......: Holger Smolinski <Holger.Smolinski@de.ibm.com>
 * Bugreports.to..: <Linux390@de.ibm.com>
 * (C) IBM Corporation, IBM Deutschland Entwicklung GmbH, 2000
 *
 * $Revision: 1.11 $
 *
 */
#ifndef CCWCACHE_H
#define CCWCACHE_H
#include <linux/slab.h>
#include <asm/irq.h>

#ifndef __KERNEL__
#define kmem_cache_t void
#endif /* __KERNEL__ */

typedef struct ccw_req_t {
	/* eye catcher plus queueing information  */
	unsigned int magic;	
	struct ccw_req_t *next;	/* pointer to next ccw_req_t in queue */
	struct ccw_req_t *int_next;	/* for internal queueing */
	struct ccw_req_t *int_prev;	/* for internal queueing */

	/* Where to execute what... */
	void *device;		/* index of the device the req is for */
	void *req;		/* pointer to originating request */
	ccw1_t *cpaddr;		/* address of channel program */
	char status;	        /* reflecting the status of this request */
	char flags;              /* see below */
	short retries;		/* A retry counter to be set when filling */

	/* ... and how */
	int options;		/* options for execution */
	char lpm;               /* logical path mask                      */
	void *data;		/* pointer to data area */
	devstat_t *dstat;	/* The device status in case of an error */

	/* these are important for recovering erroneous requests          */
	struct ccw_req_t *refers;	/* Does this request refer to another one? */
	void *function; /* refers to the originating ERP action */ ;

	unsigned long long expires;	/* expiration period */
	/* these are for profiling purposes */
	unsigned long long buildclk;	/* TOD-clock of request generation */
	unsigned long long startclk;	/* TOD-clock of request start */
	unsigned long long stopclk;	/* TOD-clock of request interrupt */
	unsigned long long endclk;	/* TOD-clock of request termination */

	/* these are for internal use */
	int cplength;		/* length of the channel program in CCWs */
	int datasize;		/* amount of additional data in bytes */
	void *lowmem_idal;      /* lowmem page for idals (if in use) */
	void *lowmem_idal_ptr;  /* ptr to the actual idal word in the idals page */
	kmem_cache_t *cache;	/* the cache this data comes from */

} __attribute__ ((aligned(4))) ccw_req_t;

/* 
 * ccw_req_t -> status can be:
 */
#define CQR_STATUS_EMPTY    0x00	/* cqr is empty */
#define CQR_STATUS_FILLED   0x01	/* cqr is ready to be preocessed */
#define CQR_STATUS_QUEUED   0x02	/* cqr is queued to be processed */
#define CQR_STATUS_IN_IO    0x03	/* cqr is currently in IO */
#define CQR_STATUS_DONE     0x04	/* cqr is completed successfully */
#define CQR_STATUS_ERROR    0x05	/* cqr is completed with error */
#define CQR_STATUS_FAILED   0x06	/* cqr is finally failed */

#define CQR_FLAGS_CHAINED   0x01	/* cqr is chained by another (last CCW is TIC) */
#define CQR_FLAGS_FINALIZED 0x02	
#define CQR_FLAGS_LM_CQR    0x04	/* cqr uses page from lowmem_pool */
#define CQR_FLAGS_LM_IDAL   0x08	/* IDALs uses page from lowmem_pool */

#ifdef __KERNEL__
#define SMALLEST_SLAB (sizeof(struct ccw_req_t) <= 128 ? 128 :\
 sizeof(struct ccw_req_t) <= 256 ? 256 : 512 )

/* SMALLEST_SLAB(1),... PAGE_SIZE(CCW_NUMBER_CACHES) */
#define CCW_NUMBER_CACHES (sizeof(struct ccw_req_t) <= 128 ? 6 :\
 sizeof(struct ccw_req_t) <= 256 ? 5 : 4 )

int ccwcache_init (void);

ccw_req_t *ccw_alloc_request (char *magic, int cplength, int additional_data);
void ccw_free_request (ccw_req_t * request);
#endif /* __KERNEL__ */
#endif				/* CCWCACHE_H */



