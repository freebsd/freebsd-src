/*
 * The mrouted program is covered by the license in the accompanying file
 * named "LICENSE".  Use of the mrouted program represents acceptance of
 * the terms and conditions listed in that file.
 *
 * The mrouted program is COPYRIGHT 1989 by The Board of Trustees of
 * Leland Stanford Junior University.
 *
 *
 * callout.c,v 3.8.4.8 1998/01/06 01:58:45 fenner Exp
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD: src/usr.sbin/mrouted/callout.c,v 1.12 1999/08/28 01:17:03 peter Exp $";
#endif /* not lint */

#include "defs.h"

/* the code below implements a callout queue */
static int id = 0;
static struct timeout_q  *Q = 0; /* pointer to the beginning of timeout queue */

struct timeout_q {
	struct timeout_q *next;		/* next event */
	int        	 id;  
	cfunc_t          func;    	/* function to call */
	void	   	 *data;		/* func's data */
	int            	 time;		/* time offset to next event*/
};

#ifdef IGMP_DEBUG
static void print_Q __P((void));
#else
#define	print_Q()	
#endif

void
callout_init()
{
    Q = (struct timeout_q *) 0;
}

void
free_all_callouts()
{
    struct timeout_q *p;

    while (Q) {
	p = Q;
	Q = Q->next;
	free(p);
    }
}


/*
 * elapsed_time seconds have passed; perform all the events that should
 * happen.
 */
void
age_callout_queue(elapsed_time)
    int elapsed_time;
{
    struct timeout_q *ptr;
    int i = 0;

    for (ptr = Q; ptr; ptr = Q, i++) {
	if (ptr->time > elapsed_time) {
	    ptr->time -= elapsed_time;
	    return;
	} else {
	    elapsed_time -= ptr->time;
	    Q = Q->next;
	    IF_DEBUG(DEBUG_TIMEOUT)
	    log(LOG_DEBUG, 0, "about to call timeout %d (#%d)", ptr->id, i);
	    if (ptr->func)
		ptr->func(ptr->data);
	    free(ptr);
	}
    }
}

/*
 * Return in how many seconds age_callout_queue() would like to be called.
 * Return -1 if there are no events pending.
 */
int
timer_nextTimer()
{
    if (Q) {
	if (Q->time < 0) {
	    log(LOG_WARNING, 0, "timer_nextTimer top of queue says %d", 
			Q->time);
	    return 0;
	}
	return Q->time;
    }
    return -1;
}

/* 
 * sets the timer
 */
int
timer_setTimer(delay, action, data)
    int 	delay;  	/* number of units for timeout */
    cfunc_t	action; 	/* function to be called on timeout */
    void  	*data;  	/* what to call the timeout function with */
{
    struct     timeout_q  *ptr, *node, *prev;
    int i = 0;
    
    /* create a node */	
    node = (struct timeout_q *)malloc(sizeof(struct timeout_q));
    if (node == 0) {
	log(LOG_WARNING, 0, "Malloc Failed in timer_settimer\n");
	return -1;
    }
    node->func = action; 
    node->data = data;
    node->time = delay; 
    node->next = 0;	
    node->id   = ++id;
    
    prev = ptr = Q;
    
    /* insert node in the queue */
    
    /* if the queue is empty, insert the node and return */
    if (!Q)
	Q = node;
    else {
	/* chase the pointer looking for the right place */
	while (ptr) {
	    
	    if (delay < ptr->time) {
		/* right place */
		
		node->next = ptr;
		if (ptr == Q)
		    Q = node;
		else
		    prev->next = node;
		ptr->time -= node->time;
		print_Q();
		IF_DEBUG(DEBUG_TIMEOUT)
		log(LOG_DEBUG, 0, "created timeout %d (#%d)", node->id, i);
		return node->id;
	    } else  {
		/* keep moving */
		
		delay -= ptr->time; node->time = delay;
		prev = ptr;
		ptr = ptr->next;
	    }
	    i++;
	}
	prev->next = node;
    }
    print_Q();
    IF_DEBUG(DEBUG_TIMEOUT)
    log(LOG_DEBUG, 0, "created timeout %d (#%d)", node->id, i);
    return node->id;
}

/* returns the time until the timer is scheduled */
int
timer_leftTimer(timer_id)
    int timer_id;
{
    struct timeout_q *ptr;
    int left = 0;

    if (!timer_id)
	return -1;

    for (ptr = Q; ptr; ptr = ptr->next) {
	left += ptr->time;
	if (ptr->id == timer_id)
	    return left;
    }
    return -1;
}

/* clears the associated timer.  Returns 1 if succeeded. */
int
timer_clearTimer(timer_id)
    int  timer_id;
{
    struct timeout_q  *ptr, *prev;
    int i = 0;
    
    if (!timer_id)
	return 0;

    prev = ptr = Q;
    
    /*
     * find the right node, delete it. the subsequent node's time
     * gets bumped up
     */
    
    print_Q();
    while (ptr) {
	if (ptr->id == timer_id) {
	    /* got the right node */
	    
	    /* unlink it from the queue */
	    if (ptr == Q)
		Q = Q->next;
	    else
		prev->next = ptr->next;
	    
	    /* increment next node if any */
	    if (ptr->next != 0)
		(ptr->next)->time += ptr->time;
	    
	    if (ptr->data)
		free(ptr->data);
	    IF_DEBUG(DEBUG_TIMEOUT)
	    log(LOG_DEBUG, 0, "deleted timer %d (#%d)", ptr->id, i);
	    free(ptr);
	    print_Q();
	    return 1;
	}
	prev = ptr;
	ptr = ptr->next;
	i++;
    }
    IF_DEBUG(DEBUG_TIMEOUT)
    log(LOG_DEBUG, 0, "failed to delete timer %d (#%d)", timer_id, i);
    print_Q();
    return 0;
}

#ifdef IGMP_DEBUG
/*
 * debugging utility
 */
static void
print_Q()
{
    struct timeout_q  *ptr;
    
    IF_DEBUG(DEBUG_TIMEOUT)
	for (ptr = Q; ptr; ptr = ptr->next)
	    log(LOG_DEBUG, 0, "(%d,%d) ", ptr->id, ptr->time);
}
#endif /* IGMP_DEBUG */
