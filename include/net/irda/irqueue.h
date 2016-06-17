/*********************************************************************
 *                
 * Filename:      irqueue.h
 * Version:       0.3
 * Description:   General queue implementation
 * Status:        Experimental.
 * Author:        Dag Brattli <dagb@cs.uit.no>
 * Created at:    Tue Jun  9 13:26:50 1998
 * Modified at:   Thu Oct  7 13:25:16 1999
 * Modified by:   Dag Brattli <dagb@cs.uit.no>
 * 
 *     Copyright (C) 1998-1999, Aage Kvalnes <aage@cs.uit.no>
 *     Copyright (c) 1998, Dag Brattli
 *     All Rights Reserved.
 *      
 *     This code is taken from the Vortex Operating System written by Aage
 *     Kvalnes and has been ported to Linux and Linux/IR by Dag Brattli
 *
 *     This program is free software; you can redistribute it and/or 
 *     modify it under the terms of the GNU General Public License as 
 *     published by the Free Software Foundation; either version 2 of 
 *     the License, or (at your option) any later version.
 *  
 *     Neither Dag Brattli nor University of Tromsø admit liability nor
 *     provide warranty for any of this software. This material is 
 *     provided "AS-IS" and at no charge.
 *     
 ********************************************************************/

#include <linux/types.h>
#include <linux/spinlock.h>

#ifndef IRDA_QUEUE_H
#define IRDA_QUEUE_H

#define NAME_SIZE      32

/*
 * Hash types
 */
#define HB_NOLOCK      0
#define HB_GLOBAL      1
#define HB_LOCAL       2
#define HB_SORTED      4

/*
 * Hash defines
 */
#define HASHBIN_SIZE   8
#define HASHBIN_MASK   0x7

#ifndef ALIGN 
#define ALIGN __attribute__((aligned))
#endif

#define Q_NULL { NULL, NULL, "", 0 }

typedef void (*FREE_FUNC)(void *arg);

/*
 * Hashbin
 */
#define GET_HASHBIN(x) ( x & HASHBIN_MASK )

struct irda_queue {
	struct irda_queue *q_next;
	struct irda_queue *q_prev;

	char   q_name[NAME_SIZE];
	__u32  q_hash;
};
typedef struct irda_queue irda_queue_t;

typedef struct hashbin_t {
	__u32      magic;
	int        hb_type;
	int        hb_size;
	spinlock_t hb_mutex[HASHBIN_SIZE] ALIGN;
	irda_queue_t   *hb_queue[HASHBIN_SIZE] ALIGN;

	irda_queue_t* hb_current;
} hashbin_t;

hashbin_t *hashbin_new(int type);
int      hashbin_delete(hashbin_t* hashbin, FREE_FUNC func);
int      hashbin_clear(hashbin_t* hashbin, FREE_FUNC free_func);
void     hashbin_insert(hashbin_t* hashbin, irda_queue_t* entry, __u32 hashv, 
			char* name);
void*    hashbin_find(hashbin_t* hashbin, __u32 hashv, char* name);
void*    hashbin_remove(hashbin_t* hashbin, __u32 hashv, char* name);
void*    hashbin_remove_first(hashbin_t *hashbin);
void*	 hashbin_remove_this( hashbin_t* hashbin, irda_queue_t* entry);
irda_queue_t *hashbin_get_first(hashbin_t *hashbin);
irda_queue_t *hashbin_get_next(hashbin_t *hashbin);

void enqueue_last(irda_queue_t **queue, irda_queue_t* element);
void enqueue_first(irda_queue_t **queue, irda_queue_t* element);
irda_queue_t *dequeue_first(irda_queue_t **queue);

#define HASHBIN_GET_SIZE(hashbin) hashbin->hb_size

#endif
