/*
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
 */

/*
 * Core ATM Services
 * -----------------
 *
 * Miscellaneous ATM subroutines
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <net/if.h>
#include <net/netisr.h>
#include <netatm/port.h>
#include <netatm/queue.h>
#include <netatm/atm.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_sap.h>
#include <netatm/atm_cm.h>
#include <netatm/atm_if.h>
#include <netatm/atm_stack.h>
#include <netatm/atm_pcb.h>
#include <netatm/atm_var.h>
#include <vm/uma.h>

/*
 * Global variables
 */
struct atm_pif		*atm_interface_head = NULL;
struct atm_ncm		*atm_netconv_head = NULL;
Atm_endpoint		*atm_endpoints[ENDPT_MAX+1] = {NULL};
struct stackq_entry	*atm_stackq_head = NULL, *atm_stackq_tail;
struct atm_sock_stat	atm_sock_stat = { { 0 } };
int			atm_init = 0;
int			atm_debug = 0;
int			atm_dev_print = 0;
int			atm_print_data = 0;
int			atm_version = ATM_VERSION;
struct timeval		atm_debugtime = {0, 0};
struct ifqueue		atm_intrq;

uma_zone_t atm_attributes_zone;

/*
 * Local functions
 */
static KTimeout_ret	atm_timexp(void *);
static void 		atm_intr(struct mbuf *);

/*
 * Local variables
 */
static struct atm_time	*atm_timeq = NULL;
static uma_zone_t atm_stackq_zone;

/*
 * Initialize ATM kernel
 * 
 * Performs any initialization required before things really get underway.
 * Called from ATM domain initialization or from first registration function 
 * which gets called.
 *
 * Arguments:
 *	none
 *
 * Returns:
 *	none
 *
 */
void
atm_initialize()
{
	/*
	 * Never called from interrupts, so no locking needed
	 */
	if (atm_init)
		return;
	atm_init = 1;

	atm_attributes_zone = uma_zcreate("atm attributes",
	    sizeof(Atm_attributes), NULL, NULL, NULL, NULL,
	    UMA_ALIGN_PTR, 0);
	if (atm_attributes_zone == NULL)
		panic("atm_initialize: unable to create attributes zone");
	uma_zone_set_max(atm_attributes_zone, 100);

	atm_stackq_zone = uma_zcreate("atm stackq", sizeof(struct stackq_entry),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);
	if (atm_stackq_zone == NULL)
		panic("atm_initialize: unable to create stackq zone");
	uma_zone_set_max(atm_stackq_zone, 10);

	atm_intrq.ifq_maxlen = ATM_INTRQ_MAX;
	mtx_init(&atm_intrq.ifq_mtx, "atm_inq", NULL, MTX_DEF);
	netisr_register(NETISR_ATM, atm_intr, &atm_intrq);

	/*
	 * Initialize subsystems
	 */
	atm_sock_init();
	atm_cm_init();
	atm_aal5_init();

	/*
	 * Prime the timer
	 */
	(void)timeout(atm_timexp, (void *)0, hz/ATM_HZ);
}


/*
 * Handle timer tick expiration
 * 
 * Decrement tick count in first block on timer queue.  If there
 * are blocks with expired timers, call their timeout function.
 * This function is called ATM_HZ times per second.
 *
 * Arguments:
 *	arg	argument passed on timeout() call
 *
 * Returns:
 *	none
 *
 */
static KTimeout_ret
atm_timexp(arg)
	void	*arg;
{
	struct atm_time	*tip;
	int		s = splimp();


	/*
	 * Decrement tick count
	 */
	if (((tip = atm_timeq) == NULL) || (--tip->ti_ticks > 0)) {
		goto restart;
	}

	/*
	 * Stack queue should have been drained
	 */
	KASSERT(atm_stackq_head == NULL, ("atm_timexp: stack queue not empty"));

	/*
	 * Dispatch expired timers
	 */
	while (((tip = atm_timeq) != NULL) && (tip->ti_ticks == 0)) {
		void	(*func)(struct atm_time *);

		/*
		 * Remove expired block from queue
		 */
		atm_timeq = tip->ti_next;
		tip->ti_flag &= ~TIF_QUEUED;

		/*
		 * Call timeout handler (with network interrupts locked out)
		 */
		func = tip->ti_func;
		(void) splx(s);
		s = splnet();
		(*func)(tip);
		(void) splx(s);
		s = splimp();

		/*
		 * Drain any deferred calls
		 */
		STACK_DRAIN();
	}

restart:
	/*
	 * Restart the timer
	 */
	(void) splx(s);
	(void) timeout(atm_timexp, (void *)0, hz/ATM_HZ);

	return;
}


/*
 * Schedule a control block timeout
 * 
 * Place the supplied timer control block on the timer queue.  The
 * function (func) will be called in 't' timer ticks with the
 * control block address as its only argument.  There are ATM_HZ
 * timer ticks per second.  The ticks value stored in each block is
 * a delta of the number of ticks from the previous block in the queue.
 * Thus, for each tick interval, only the first block in the queue 
 * needs to have its tick value decremented.
 *
 * Arguments:
 *	tip	pointer to timer control block
 *	t	number of timer ticks until expiration
 *	func	pointer to function to call at expiration 
 *
 * Returns:
 *	none
 *
 */
void
atm_timeout(tip, t, func)
	struct atm_time	*tip;
	int		t;
	void		(*func)(struct atm_time *);
{
	struct atm_time	*tip1, *tip2;
	int		s;


	/*
	 * Check for double queueing error
	 */
	if (tip->ti_flag & TIF_QUEUED)
		panic("atm_timeout: double queueing");

	/*
	 * Make sure we delay at least a little bit
	 */
	if (t <= 0)
		t = 1;

	/*
	 * Find out where we belong on the queue
	 */
	s = splimp();
	for (tip1 = NULL, tip2 = atm_timeq; tip2 && (tip2->ti_ticks <= t); 
					    tip1 = tip2, tip2 = tip1->ti_next) {
		t -= tip2->ti_ticks;
	}

	/*
	 * Place ourselves on queue and update timer deltas
	 */
	if (tip1 == NULL)
		atm_timeq = tip;
	else
		tip1->ti_next = tip;
	tip->ti_next = tip2;

	if (tip2)
		tip2->ti_ticks -= t;
	
	/*
	 * Setup timer block
	 */
	tip->ti_flag |= TIF_QUEUED;
	tip->ti_ticks = t;
	tip->ti_func = func;

	(void) splx(s);
	return;
}


/*
 * Cancel a timeout
 * 
 * Remove the supplied timer control block from the timer queue.
 *
 * Arguments:
 *	tip	pointer to timer control block
 *
 * Returns:
 *	0	control block successfully dequeued
 * 	1	control block not on timer queue
 *
 */
int
atm_untimeout(tip)
	struct atm_time	*tip;
{
	struct atm_time	*tip1, *tip2;
	int		s;

	/*
	 * Is control block queued?
	 */
	if ((tip->ti_flag & TIF_QUEUED) == 0)
		return(1);

	/*
	 * Find control block on the queue
	 */
	s = splimp();
	for (tip1 = NULL, tip2 = atm_timeq; tip2 && (tip2 != tip); 
					    tip1 = tip2, tip2 = tip1->ti_next) {
	}

	if (tip2 == NULL) {
		(void) splx(s);
		return (1);
	}

	/*
	 * Remove block from queue and update timer deltas
	 */
	tip2 = tip->ti_next;
	if (tip1 == NULL)
		atm_timeq = tip2;
	else
		tip1->ti_next = tip2;

	if (tip2)
		tip2->ti_ticks += tip->ti_ticks;
	
	/*
	 * Reset timer block
	 */
	tip->ti_flag &= ~TIF_QUEUED;

	(void) splx(s);
	return (0);
}


/*
 * Queue a Stack Call 
 * 
 * Queues a stack call which must be deferred to the global stack queue.
 * The call parameters are stored in entries which are allocated from the
 * stack queue storage pool.
 *
 * Arguments:
 *	cmd	stack command
 *	func	destination function
 *	token	destination layer's token
 *	cvp	pointer to  connection vcc
 *	arg1	command argument
 *	arg2	command argument
 *
 * Returns:
 *	0 	call queued
 *	errno	call not queued - reason indicated
 *
 */
int
atm_stack_enq(cmd, func, token, cvp, arg1, arg2)
	int		cmd;
	void		(*func)(int, void *, intptr_t, intptr_t);
	void		*token;
	Atm_connvc	*cvp;
	intptr_t	arg1;
	intptr_t	arg2;
{
	struct stackq_entry	*sqp;
	int		s = splnet();

	/*
	 * Get a new queue entry for this call
	 */
	sqp = uma_zalloc(atm_stackq_zone, M_ZERO);
	if (sqp == NULL) {
		(void) splx(s);
		return (ENOMEM);
	}

	/*
	 * Fill in new entry
	 */
	sqp->sq_next = NULL;
	sqp->sq_cmd = cmd;
	sqp->sq_func = func;
	sqp->sq_token = token;
	sqp->sq_arg1 = arg1;
	sqp->sq_arg2 = arg2;
	sqp->sq_connvc = cvp;

	/*
	 * Put new entry at end of queue
	 */
	if (atm_stackq_head == NULL)
		atm_stackq_head = sqp;
	else
		atm_stackq_tail->sq_next = sqp;
	atm_stackq_tail = sqp;

	(void) splx(s);
	return (0);
}


/*
 * Drain the Stack Queue
 * 
 * Dequeues and processes entries from the global stack queue.  
 *
 * Arguments:
 *	none
 *
 * Returns:
 *	none
 *
 */
void
atm_stack_drain()
{
	struct stackq_entry	*sqp, *qprev, *qnext;
	int		s = splnet();
	int		cnt;

	/*
	 * Loop thru entire queue until queue is empty
	 *	(but panic rather loop forever)
	 */
	do {
		cnt = 0;
		qprev = NULL;
		for (sqp = atm_stackq_head; sqp; ) {

			/*
			 * Got an eligible entry, do STACK_CALL stuff
			 */
			if (sqp->sq_cmd & STKCMD_UP) {
				if (sqp->sq_connvc->cvc_downcnt) {

					/*
					 * Cant process now, skip it
					 */
					qprev = sqp;
					sqp = sqp->sq_next;
					continue;
				}

				/*
				 * OK, dispatch the call
				 */
				sqp->sq_connvc->cvc_upcnt++;
				(*sqp->sq_func)(sqp->sq_cmd, 
						sqp->sq_token,
						sqp->sq_arg1,
						sqp->sq_arg2);
				sqp->sq_connvc->cvc_upcnt--;
			} else {
				if (sqp->sq_connvc->cvc_upcnt) {

					/*
					 * Cant process now, skip it
					 */
					qprev = sqp;
					sqp = sqp->sq_next;
					continue;
				}

				/*
				 * OK, dispatch the call
				 */
				sqp->sq_connvc->cvc_downcnt++;
				(*sqp->sq_func)(sqp->sq_cmd, 
						sqp->sq_token,
						sqp->sq_arg1,
						sqp->sq_arg2);
				sqp->sq_connvc->cvc_downcnt--;
			}

			/*
			 * Dequeue processed entry and free it
			 */
			cnt++;
			qnext = sqp->sq_next;
			if (qprev)
				qprev->sq_next = qnext;
			else
				atm_stackq_head = qnext;
			if (qnext == NULL)
				atm_stackq_tail = qprev;
			uma_zfree(atm_stackq_zone, sqp);
			sqp = qnext;
		}
	} while (cnt > 0);

	/*
	 * Make sure entire queue was drained
	 */
	if (atm_stackq_head != NULL)
		panic("atm_stack_drain: Queue not emptied");

	(void) splx(s);
}


/*
 * Process Interrupt Queue
 * 
 * Processes entries on the ATM interrupt queue.  This queue is used by
 * device interface drivers in order to schedule events from the driver's 
 * lower (interrupt) half to the driver's stack services.
 *
 * The interrupt routines must store the stack processing function to call
 * and a token (typically a driver/stack control block) at the front of the
 * queued buffer.  We assume that the function pointer and token values are 
 * both contained (and properly aligned) in the first buffer of the chain.
 *
 * Arguments:
 *	none
 *
 * Returns:
 *	none
 *
 */
static void
atm_intr(struct mbuf *m)
{
	caddr_t		cp;
	atm_intr_func_t	func;
	void		*token;

	/*
	 * Get function to call and token value
	 */
	KB_DATASTART(m, cp, caddr_t);
	func = *(atm_intr_func_t *)cp;
	cp += sizeof(func);
	token = *(void **)cp;
	KB_HEADADJ(m, -(sizeof(func) + sizeof(token)));
	if (KB_LEN(m) == 0) {
		KBuffer		*m1;
		KB_UNLINKHEAD(m, m1);
		m = m1;
	}

	/*
	 * Call processing function
	 */
	(*func)(token, m);

	/*
	 * Drain any deferred calls
	 */
	STACK_DRAIN();
}

/*
 * Print a pdu buffer chain
 * 
 * Arguments:
 *	m	pointer to pdu buffer chain
 *	msg	pointer to message header string
 *
 * Returns:
 *	none
 *
 */
void
atm_pdu_print(m, msg)
	KBuffer		*m;
	char		*msg;
{
	caddr_t		cp;
	int		i;
	char		c = ' ';

	printf("%s:", msg);
	while (m) { 
		KB_DATASTART(m, cp, caddr_t);
		printf("%cbfr=%p data=%p len=%d: ",
			c, m, cp, KB_LEN(m));
		c = '\t';
		if (atm_print_data) {
			for (i = 0; i < KB_LEN(m); i++) {
				printf("%2x ", (u_char)*cp++);
			}
			printf("<end_bfr>\n");
		} else {
			printf("\n");
		}
		m = KB_NEXT(m);
	}
}

