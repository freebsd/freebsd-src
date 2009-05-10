/*-
 * Copyright (c) 1996-1997, 2001, 2005, Juniper Networks, Inc.
 * All rights reserved.
 * Jim Hayes, November 1996
 *
 * queue.h - Description of uKernel queues, for the Juniper Kernel
 *
 *	JNPR: queue.h,v 1.1 2006/08/07 05:38:57 katta
 * $FreeBSD$
 *
 */

#ifndef __QUEUE_H__
#define	__QUEUE_H__

/*---------------------------------------------------------------------------
 * QUEUE MANAGEMENT DOCUMENTATION
 */

/*
   --------
   Q_INIT()
   --------

   void q_init(void)

   Initialize the queue management system for the microkernel.
   This initializes the debugging flags and sets up accounting.

   ---------
   Q_ALLOC()
   ---------

   queue_t *q_alloc()

   Allocates a queue from kernel memory, and initializes it for you.

   The default initialization provides a queue that is unbounded.

   If you want to be bounded with special features, use q_control
   after initialization.

   q_alloc() returns NULL in the face of peril or low memory.

   --------
   Q_FREE()
   --------

   void *q_free(queue_t *queue_pointer)

   Returns a queue to kernel memory, and frees the queue contents
   for you using free() and complains (with a traceback) that you
   tried to kill of a non-empty queue.

   If any threads are waiting on the queue, wake them up.

   -----------
   Q_CONTROL()
   -----------
   void q_control(queue_t *queue_pointer, queue_size_t max_queue_size);

   For now, allows you to limit queue growth.

   ----------------
   Q_DEQUEUE_WAIT() ** MAY CAUSE THREAD TO BLOCK/CANNOT BE CALLED FROM ISRs **
   ----------------

   void *q_dequeue_wait(queue_t *queue_pointer, wakeup_mask_t *mask)

   Removes and returns a pointer to the next message in the specified
   queue.  If the queue is empty, the calling thread goes to sleep
   until something is queued to the queue.  If this call returns NULL,
   then an extraordinary event requires this thread's attention--
   check errno in this case.

   ---------
   Q_DEQUEUE	 ** CAN BE CALLED FROM ISRs **
   ---------

   void *q_dequeue(queue_t *queue_pointer)

   Just like q_dequeue_wait(), but instead of blocking, return NULL.

   -----------
   Q_ENQUEUE()		   ** CAN BE CALLED FROM ISRs **
   -----------

   boolean q_enqueue(queue_t *queue_pointer, void *element_pointer)

   Add the element to the end of the named queue.  If the add fails
   because a limit has been reached, return TRUE.  Otherwise return
   FALSE if everything went OK.

   ----------
   Q_URGENT()
   ----------

   boolean q_urgent(queue_t *queue_pointer, void *element_pointer)

   Same as q_enqueue(), except this element will be placed at the top
   of the queue, and will be picked off at the next q_dequeue_wait()
   operation.

   --------
   Q_PEEK()		   ** CAN BE CALLED FROM ISRs **
   --------

   void *q_peek(queue_t *queue_pointer)

   Returns a pointer to the top element of the queue without actually
   dequeuing it.  Returns NULL of the queue is empty.

   This routine will never block.

   ----------
   Q_DELETE()
   ----------

   void q_delete(queue_t *queue_pointer, void *element_pointer)

   Delete the element_pointer from the queue, if it exists.  This
   isn't speedy, and isn't meant for tasks requiring performance.
   It's primary use is to pull something off the queue when you know
   in the common case that it's gonna be at or near the top of the
   list.  (I.e. waking a thread from a wake list when extraordinary
   conditions exist, and you have to pluck it from the middle of the
   list.)

   This routine does not block or return anything.

   --------
   Q_SIZE()
   --------

   queue_size_t q_size(queue_t *queue_pointer)

   Returns the number of elements in the queue.

   ------------
   Q_MAX_SIZE()
   ------------

   queue_size_t q_max_size(queue_t *queue_pointer);

   Returns the maximum size of this queue, or 0 if this queue is
   unbounded.

*/

/*-------------------------------------------------------------------------
 * Basic queue management structures.
 */

/*
 * Typedefs
 */

typedef u_int32_t queue_size_t;

/*
 * Prototypes
 */

void		 q_init(void);
queue_t		*q_alloc(void);
void		*q_peek(queue_t *queue);
void		*q_dequeue(queue_t *queue);
boolean		 q_enqueue(queue_t *queue, void *item);
boolean		 q_urgent(queue_t *queue, void *item);

#endif /* __QUEUE_H__ */
