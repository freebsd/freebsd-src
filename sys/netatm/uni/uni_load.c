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
 * Loadable kernel module support
 *
 */

#ifndef ATM_UNI_MODULE
#include "opt_atm.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/syslog.h>
#include <machine/clock.h>
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

#ifndef lint
__RCSID("@(#) $FreeBSD$");
#endif

/*
 * External functions
 */
int		sscop_start __P((void));
int		sscop_stop __P((void));
int		sscf_uni_start __P((void));
int		sscf_uni_stop __P((void));
int		uniip_start __P((void));
int		uniip_stop __P((void));
int		unisig_start __P((void));
int		unisig_stop __P((void));

/*
 * Local functions
 */
static int	uni_start __P((void));
static int	uni_stop __P((void));


/*
 * Initialize uni processing
 * 
 * This will be called during module loading.  We just notify all of our
 * sub-services to initialize.
 *
 * Arguments:
 *	none
 *
 * Returns:
 *	0 	startup was successful 
 *	errno	startup failed - reason indicated
 *
 */
static int
uni_start()
{
	int	err;

	/*
	 * Verify software version
	 */
	if (atm_version != ATM_VERSION) {
		log(LOG_ERR, "version mismatch: uni=%d.%d kernel=%d.%d\n",
			ATM_VERS_MAJ(ATM_VERSION), ATM_VERS_MIN(ATM_VERSION),
			ATM_VERS_MAJ(atm_version), ATM_VERS_MIN(atm_version));
		return (EINVAL);
	}

	/*
	 * Initialize uni sub-services
	 */
	err = sscop_start();
	if (err)
		goto done;

	err = sscf_uni_start();
	if (err)
		goto done;

	err = unisig_start();
	if (err)
		goto done;

	err = uniip_start();
	if (err)
		goto done;

done:
	return (err);
}


/*
 * Halt uni processing 
 * 
 * This will be called just prior to unloading the module from
 * memory.  All sub-services will be notified of the termination.
 *
 * Arguments:
 *	none
 *
 * Returns:
 *	0 	shutdown was successful 
 *	errno	shutdown failed - reason indicated
 *
 */
static int
uni_stop()
{
	int	err, s = splnet();

	/*
	 * Terminate uni sub-services
	 */
	err = uniip_stop();
	if (err)
		goto done;

	err = unisig_stop();
	if (err)
		goto done;

	err = sscf_uni_stop();
	if (err)
		goto done;

	err = sscop_stop();
	if (err)
		goto done;

done:
	(void) splx(s);
	return (err);
}


#ifdef ATM_UNI_MODULE
/*
 *******************************************************************
 *
 * Loadable Module Support
 *
 *******************************************************************
 */
static int	uni_doload __P((void));
static int	uni_dounload __P((void));

/*
 * Generic module load processing
 * 
 * This function is called by an OS-specific function when this
 * module is being loaded.
 *
 * Arguments:
 *	none
 *
 * Returns:
 *	0 	load was successful 
 *	errno	load failed - reason indicated
 *
 */
static int
uni_doload()
{
	int	err = 0;

	/*
	 * Start us up
	 */
	err = uni_start();
	if (err)
		/* Problems, clean up */
		(void)uni_stop();

	return (err);
}


/*
 * Generic module unload processing
 * 
 * This function is called by an OS-specific function when this
 * module is being unloaded.
 *
 * Arguments:
 *	none
 *
 * Returns:
 *	0 	unload was successful 
 *	errno	unload failed - reason indicated
 *
 */
static int
uni_dounload()
{
	int	err = 0;

	/*
	 * OK, try to clean up our mess
	 */
	err = uni_stop();

	return (err);
}




#include <sys/exec.h>
#include <sys/sysent.h>
#include <sys/lkm.h>

/*
 * Loadable miscellaneous module description
 */
MOD_MISC(uni);


/*
 * Loadable module support "load" entry point
 * 
 * This is the routine called by the lkm driver whenever the
 * modload(1) command is issued for this module.
 *
 * Arguments:
 *	lkmtp	pointer to lkm drivers's structure
 *	cmd	lkm command code
 *
 * Returns:
 *	0 	command was successful 
 *	errno	command failed - reason indicated
 *
 */
static int
uni_load(lkmtp, cmd)
	struct lkm_table	*lkmtp;
	int		cmd;
{
	return(uni_doload());
}


/*
 * Loadable module support "unload" entry point
 * 
 * This is the routine called by the lkm driver whenever the
 * modunload(1) command is issued for this module.
 *
 * Arguments:
 *	lkmtp	pointer to lkm drivers's structure
 *	cmd	lkm command code
 *
 * Returns:
 *	0 	command was successful 
 *	errno	command failed - reason indicated
 *
 */
static int
uni_unload(lkmtp, cmd)
	struct lkm_table	*lkmtp;
	int		cmd;
{
	return(uni_dounload());
}


/*
 * Loadable module support entry point
 * 
 * This is the routine called by the lkm driver for all loadable module
 * functions for this driver.  This routine name must be specified
 * on the modload(1) command.  This routine will be called whenever the
 * modload(1), modunload(1) or modstat(1) commands are issued for this
 * module.
 *
 * Arguments:
 *	lkmtp	pointer to lkm drivers's structure
 *	cmd	lkm command code
 *	ver	lkm version
 *
 * Returns:
 *	0 	command was successful 
 *	errno	command failed - reason indicated
 *
 */
int
uni_mod(lkmtp, cmd, ver)
	struct lkm_table	*lkmtp;
	int		cmd;
	int		ver;
{
	MOD_DISPATCH(uni, lkmtp, cmd, ver,
		uni_load, uni_unload, lkm_nullcmd);
}

#else	/* !ATM_UNI_MODULE */

/*
 *******************************************************************
 *
 * Kernel Compiled Module Support
 *
 *******************************************************************
 */
static void	uni_doload __P((void *));

SYSINIT(atmuni, SI_SUB_PROTO_END, SI_ORDER_ANY, uni_doload, NULL)

/*
 * Kernel initialization
 * 
 * Arguments:
 *	arg	Not used
 *
 * Returns:
 *	none
 *
 */
static void
uni_doload(void *arg)
{
	int	err = 0;

	/*
	 * Start us up
	 */
	err = uni_start();
	if (err) {
		/* Problems, clean up */
		(void)uni_stop();

		log(LOG_ERR, "ATM UNI unable to initialize (%d)!!\n", err);
	}
	return;
}
#endif	/* ATM_UNI_MODULE */

