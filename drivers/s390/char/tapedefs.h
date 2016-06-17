/***********************************************************************
 *  drivers/s390/char/tapedefs.h
 *    tape device driver for S/390 and zSeries tapes.
 *
 *  S390 and zSeries version
 *    Copyright (C) 2001 IBM Corporation
 *    Author(s):  Carsten Otte <cotte@de.ibm.com>
 *                Tuan Ngo-Anh <ngoanh@de.ibm.com>
 *
 *
 ***********************************************************************
 */

/* Kernel Version Compatibility section */
#include <linux/version.h>
#include <linux/blkdev.h>
#include <linux/blk.h>
#include <asm/irq.h>

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,2,17))
#define TAPE_DEBUG               // use s390 debug feature
#else
#undef TAPE_DEBUG                // debug feature not supported by our 2.2.16 code
static inline void set_normalized_cda ( ccw1_t * cp, unsigned long address ) {
    cp -> cda = address;
}
static inline void clear_normalized_cda ( ccw1_t * ccw ) {
    ccw -> cda = 0;
}
#define BUG() PRINT_FATAL("tape390: CRITICAL INTERNAL ERROR OCCURED. REPORT THIS BACK TO LINUX390@DE.IBM.COM\n")
#endif
#define CONFIG_S390_TAPE_DYNAMIC // allow devices to be attached or detached on the fly
#define TAPEBLOCK_RETRIES 20     // number of retries, when a block-dev request fails.


#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,3,98))
#define INIT_BLK_DEV(d_major,d_request_fn,d_queue_fn,d_current) \
do { \
        blk_dev[d_major].queue = d_queue_fn; \
} while(0)
static inline struct request * 
tape_next_request( request_queue_t *queue ) 
{
        return blkdev_entry_next_request(&queue->queue_head);
}
static inline void 
tape_dequeue_request( request_queue_t * q, struct request *req )
{
        blkdev_dequeue_request (req);
}
#else 
#define s390_dev_info_t dev_info_t
typedef struct request *request_queue_t;
#ifndef init_waitqueue_head
#define init_waitqueue_head(x) do { *x = NULL; } while(0)
#endif
#define blk_init_queue(x,y) do {} while(0)
#define blk_queue_headactive(x,y) do {} while(0)
#define INIT_BLK_DEV(d_major,d_request_fn,d_queue_fn,d_current) \
do { \
        blk_dev[d_major].request_fn = d_request_fn; \
        blk_dev[d_major].queue = d_queue_fn; \
        blk_dev[d_major].current_request = d_current; \
} while(0)
static inline struct request *
tape_next_request( request_queue_t *queue ) 
{
    return *queue;
}
static inline void 
tape_dequeue_request( request_queue_t * q, struct request *req )
{
        *q = req->next;
        req->next = NULL;
}
#endif 
