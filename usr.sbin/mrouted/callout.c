/*
 * The mrouted program is covered by the license in the accompanying file
 * named "LICENSE".  Use of the mrouted program represents acceptance of
 * the terms and conditions listed in that file.
 *
 * The mrouted program is COPYRIGHT 1989 by The Board of Trustees of
 * Leland Stanford Junior University.
 *
 *
 * $Id: callout.c,v 3.8 1995/11/29 22:36:57 fenner Rel $
 */

#include "defs.h"

/* the code below implements a callout queue */
static int id = 0;
static struct timeout_q  *Q = 0; /* pointer to the beginning of timeout queue */

static int in_callout = 0;

struct timeout_q {
	struct timeout_q *next;		/* next event */
	int        	 id;  
	cfunc_t          func;    	/* function to call */
	char	   	 *data;		/* func's data */
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


/*
 * signal handler for SIGALARM that is called once every second
 */
void
age_callout_queue()
{
    struct timeout_q *ptr;
    
    if (in_callout)
	return;

    in_callout = 1;
    ptr = Q;
    
    while (ptr) {
	if (!ptr->time) {
	    /* timeout has happened */
	    Q = Q->next;

	    in_callout = 0;
	    if (ptr->func)
		ptr->func(ptr->data);
	    in_callout = 1;
	    
	    free(ptr);
	    ptr = Q;
	}
	else {
	    ptr->time --;
#ifdef IGMP_DEBUG
	    log(LOG_DEBUG,0,"[callout, age_callout_queue] -- time (%d)", ptr->time);
#endif /* IGMP_DEBUG */
	    in_callout = 0; return;
	}
    }
    in_callout = 0;
    return;
}


/* 
 * sets the timer
 */
int
timer_setTimer(delay, action, data)
    int 	delay;  	/* number of units for timeout */
    cfunc_t	action; 	/* function to be called on timeout */
    char  	*data;  	/* what to call the timeout function with */
{
    struct     timeout_q  *ptr, *node, *prev;
    
    if (in_callout)
	return -1;

    in_callout = 1;
    
    /* create a node */	
    node = (struct timeout_q *)malloc(sizeof(struct timeout_q));
    if (node == 0) {
	log(LOG_WARNING, 0, "Malloc Failed in timer_settimer\n");
	in_callout = 0;
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
		in_callout = 0;
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
    print_Q();
    in_callout = 0;
    return node->id;
}


/* clears the associated timer */
void
timer_clearTimer(timer_id)
    int  timer_id;
{
    struct timeout_q  *ptr, *prev;
    
    if (in_callout)
        return;
    if (!timer_id)
	return;

    in_callout = 1;
    
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
	    
	    free(ptr->data);
	    free(ptr);
	    print_Q();
	    in_callout = 0;
	    return;
	}
	prev = ptr;
	ptr = ptr->next;
    }
    print_Q();
    in_callout = 0;
}

#ifdef IGMP_DEBUG
/*
 * debugging utility
 */
static void
print_Q()
{
    struct timeout_q  *ptr;
    
    for(ptr = Q; ptr; ptr = ptr->next)
	log(LOG_DEBUG,0,"(%d,%d) ", ptr->id, ptr->time);
}
#endif /* IGMP_DEBUG */
int
secs_remaining( timer_id)
    int  timer_id;
{
    struct timeout_q  *ptr;
    int left=0;

    for (ptr = Q; ptr && ptr->id != timer_id; ptr = ptr->next)
       left += ptr->time;

    if (!ptr) /* not found */
       return 0;

    return left + ptr->time;
}
