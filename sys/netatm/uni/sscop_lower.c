/*
 *
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
 *
 *	@(#) $FreeBSD$
 *
 */

/*
 * ATM Forum UNI Support
 * ---------------------
 *
 * SSCOP - SSCOP SAP interface processing 
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/syslog.h>
#include <net/if.h>
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

#include <netatm/uni/sscop.h>
#include <netatm/uni/sscop_misc.h>
#include <netatm/uni/sscop_var.h>

#ifndef lint
__RCSID("@(#) $FreeBSD$");
#endif


/*
 * Local variables
 */
/*
 * Stack commands with arg1 containing an buffer pointer
 */
static u_char	sscop_buf1[] = {
		0,
		0,		/* SSCOP_INIT */
		0,		/* SSCOP_TERM */
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		1,		/* SSCOP_ESTABLISH_REQ */
		0,
		1,		/* SSCOP_ESTABLISH_RSP */
		0,
		1,		/* SSCOP_RELEASE_REQ */
		0,
		0,
		1,		/* SSCOP_DATA_REQ */
		0,
		1,		/* SSCOP_RESYNC_REQ */
		0,
		0,		/* SSCOP_RESYNC_RSP */
		0,
		0,
		0,		/* SSCOP_RECOVER_RSP */
		1,		/* SSCOP_UNITDATA_REQ */
		0,
		0,		/* SSCOP_RETRIEVE_REQ */
		0,
		0
};


/*
 * SSCOP Lower Stack Command Handler
 * 
 * This function will receive all of the stack commands issued from the 
 * layer above SSCOP (ie. using the SSCOP SAP).  The appropriate processing
 * function will be determined based on the received stack command and the 
 * current sscop control block state.
 *
 * Arguments:
 *	cmd	stack command code
 *	tok	session token
 *	arg1	command specific argument
 *	arg2	command specific argument
 *
 * Returns:
 *	none
 *
 */
void
sscop_lower(cmd, tok, arg1, arg2)
	int	cmd;
	void	*tok;
	int	arg1;
	int	arg2;
{
	struct sscop	*sop = (struct sscop *)tok;
	void		(**stab) __P((struct sscop *, int, int));
	void		(*func) __P((struct sscop *, int, int));
	int		val;

	ATM_DEBUG5("sscop_lower: cmd=0x%x, sop=%p, state=%d, arg1=0x%x, arg2=0x%x\n",
		cmd, sop, sop->so_state, arg1, arg2);

	/*
	 * Validate stack command
	 */
	val = cmd & STKCMD_VAL_MASK;
	if (((u_int)cmd  < (u_int)SSCOP_CMD_MIN) ||
	    ((u_int)cmd  > (u_int)SSCOP_CMD_MAX) ||
	    ((stab = (sop->so_vers == SSCOP_VERS_QSAAL ? 
			sscop_qsaal_aatab[val] : 
			sscop_q2110_aatab[val])) == NULL)) {
		log(LOG_ERR, "sscop_lower: unknown cmd 0x%x, sop=%p\n",
			cmd, sop);
		return;
	}

	/*
	 * Validate sscop state
	 */
	if (sop->so_state > SOS_MAXSTATE) {
		log(LOG_ERR, "sscop_lower: invalid state sop=%p, state=%d\n",
			sop, sop->so_state);
		/*
		 * Release possible buffer
		 */
		if (sscop_buf1[val]) {
			if (arg1)
				KB_FREEALL((KBuffer *)arg1);
		}
		return;
	}

	/*
	 * Validate command/state combination
	 */
	func = stab[sop->so_state];
	if (func == NULL) {
		log(LOG_ERR, 
			"sscop_lower: invalid cmd/state: sop=%p, cmd=0x%x, state=%d\n",
			sop, cmd, sop->so_state);
		/*
		 * Release possible buffer
		 */
		if (sscop_buf1[val]) {
			if (arg1)
				KB_FREEALL((KBuffer *)arg1);
		}
		return;
	}

	/*
	 * Call event processing function
	 */
	(*func)(sop, arg1, arg2);

	return;
}


/*
 * No-op Processor (no buffers)
 * 
 * Arguments:
 *	sop	pointer to sscop connection block
 *	arg1	command-specific argument
 *	arg2	command-specific argument
 *
 * Returns:
 *	none
 *
 */
void
sscop_aa_noop_0(sop, arg1, arg2)
	struct sscop	*sop;
	int		arg1;
	int		arg2;
{
	/*
	 * Nothing to do
	 */
	return;
}


/*
 * No-op Processor (arg1 == buffer)
 * 
 * Arguments:
 *	sop	pointer to sscop connection block
 *	arg1	command-specific argument (buffer pointer)
 *	arg2	command-specific argument
 *
 * Returns:
 *	none
 *
 */
void
sscop_aa_noop_1(sop, arg1, arg2)
	struct sscop	*sop;
	int		arg1;
	int		arg2;
{

	/*
	 * Just free buffer chain
	 */
	if (arg1)
		KB_FREEALL((KBuffer *)arg1);

	return;
}


/*
 * SSCOP_INIT / SOS_INST Command Processor
 * 
 * Arguments:
 *	sop	pointer to sscop connection block
 *	arg1	command specific argument
 *	arg2	command specific argument
 *
 * Returns:
 *	none
 *
 */
void
sscop_init_inst(sop, arg1, arg2)
	struct sscop	*sop;
	int		arg1;
	int		arg2;
{
	int		err;

	/*
	 * Make ourselves ready and pass on the INIT
	 */
	sop->so_state = SOS_IDLE;

	/*
	 * Validate SSCOP version to use
	 */
	switch ((enum sscop_vers)arg1) {
	case SSCOP_VERS_QSAAL:
		break;

	case SSCOP_VERS_Q2110:
		break;

	default:
		sscop_abort(sop, "sscop: bad version\n");
		return;
	}
	sop->so_vers = (enum sscop_vers)arg1;

	/*
	 * Copy SSCOP connection parameters to use
	 */
	sop->so_parm = *(struct sscop_parms *)arg2;

	/*
	 * Initialize lower layers
	 */
	STACK_CALL(CPCS_INIT, sop->so_lower, sop->so_tokl, sop->so_connvc,
		0, 0, err);
	if (err) {
		/*
		 * Should never happen
		 */
		sscop_abort(sop, "sscop: INIT failure\n");
		return;
	}
	return;
}


/*
 * SSCOP_TERM / SOS_* Command Processor
 * 
 * Arguments:
 *	sop	pointer to sscop connection block
 *	arg1	command specific argument
 *	arg2	command specific argument
 *
 * Returns:
 *	none
 *
 */
void
sscop_term_all(sop, arg1, arg2)
	struct sscop	*sop;
	int		arg1;
	int		arg2;
{
	int		err;

	/*
	 * Set termination state
	 */
	sop->so_state = SOS_TERM;

	/*
	 * Pass the TERM down the stack
	 */
	STACK_CALL(CPCS_TERM, sop->so_lower, sop->so_tokl, sop->so_connvc,
		0, 0, err);
	if (err) {
		/*
		 * Should never happen
		 */
		sscop_abort(sop, "sscop: TERM failure\n");
		return;
	}

	/*
	 * Unlink and free the connection block
	 */
	UNLINK(sop, struct sscop, sscop_head, so_next);
	atm_free((caddr_t)sop);
	sscop_vccnt--;
	return;
}

