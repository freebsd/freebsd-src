/*
 * The mrouted program is covered by the license in the accompanying file
 * named "LICENSE.mrouted". Use of the mrouted program represents acceptance
 * of the terms and conditions listed in that file.
 *
 * The mrouted program is COPYRIGHT 1989 by The Board of Trustees of
 * Leland Stanford Junior University.
 *
 *
 * callout.c,v 3.8.4.5 1997/05/16 20:18:25 fenner Exp
 *
 * $FreeBSD: src/usr.sbin/pim6dd/callout.c,v 1.1.2.1 2000/07/15 07:36:29 kris Exp $
 */

#include "defs.h"

/* the code below implements a callout queue */
static int id = 0;
static struct timeout_q  *Q = 0; /* pointer to the beginning of timeout queue */

struct timeout_q {
    struct timeout_q *next;		/* next event */
    int        	     id;  
    cfunc_t          func;    	        /* function to call */
    void	     *data;		/* func's data */
    int              time;		/* time offset to next event*/
};

#ifdef CALLOUT_DEBUG
static void print_Q __P((void));
#else
#define	print_Q()	
#endif

void
callout_init()
{
    if (Q) {
	log(LOG_ERR, 0, "timer used before callout_init()");
	exit(1);
    }
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
    struct timeout_q *ptr, *expQ;
    
#ifdef CALLOUT_DEBUG
    IF_DEBUG(DEBUG_TIMEOUT)
	log(LOG_DEBUG, 0, "aging queue (elapsed time %d):", elapsed_time);
    print_Q();
#endif

    expQ = Q;
    ptr = NULL;
    
    while (Q) {
	if (Q->time > elapsed_time) {
	    Q->time -= elapsed_time;
	    if (ptr) {
		ptr->next = NULL;
		break;
	    }
	    return;
	} else {
	    elapsed_time -= Q->time;
	    ptr = Q;
	    Q = Q->next;
	}
    }
    
    /* handle queue of expired timers */
    while (expQ) {
	ptr = expQ;
	if (ptr->func)
	    ptr->func(ptr->data);
	expQ = expQ->next;
	free(ptr);
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
    
#ifdef CALLOUT_DEBUG
    IF_DEBUG(DEBUG_TIMEOUT)
	log(LOG_DEBUG, 0, "setting timer:");
    print_Q();
#endif

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
		return node->id;
	    } else  {
		/* keep moving */
		
		delay -= ptr->time; node->time = delay;
		prev = ptr;
		ptr = ptr->next;
	    }
	}
	prev->next = node;
    }
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

/* clears the associated timer */
void
timer_clearTimer(timer_id)
    int  timer_id;
{
    struct timeout_q  *ptr, *prev;
    
    if (!timer_id)
	return;
    
    prev = ptr = Q;
    
    /*
     * find the right node, delete it. the subsequent node's time
     * gets bumped up
     */
    
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
	    free(ptr);
	    return;
	}
	prev = ptr;
	ptr = ptr->next;
    }
}

#ifdef CALLOUT_DEBUG
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
#endif /* CALLOUT_DEBUG */
