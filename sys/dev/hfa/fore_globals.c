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
 * FORE Systems 200-Series Adapter Support
 * ---------------------------------------
 *
 * Global variable definitions
 *
 */

#include <dev/hfa/fore_include.h>

#ifndef lint
__RCSID("@(#) $FreeBSD$");
#endif

/*
 * Device unit table
 */
Fore_unit	*fore_units[FORE_MAX_UNITS] = {NULL};
int		fore_nunits = 0;


/*
 * ATM Interface services
 */
static struct stack_defn	fore_svaal5 = {
	NULL,
	SAP_CPCS_AAL5,
	SDF_TERM,
	atm_dev_inst,
	atm_dev_lower,
	NULL,
	0,
};
static struct stack_defn	fore_svaal4 = {
	&fore_svaal5,
	SAP_CPCS_AAL3_4,
	SDF_TERM,
	atm_dev_inst,
	atm_dev_lower,
	NULL,
	0,
};
static struct stack_defn	fore_svaal0 = {
	&fore_svaal4,
	SAP_ATM,
	SDF_TERM,
	atm_dev_inst,
	atm_dev_lower,
	NULL,
	0,
};
struct stack_defn	*fore_services = &fore_svaal0;


/*
 * Storage pools
 */
struct sp_info fore_nif_pool = {
	"fore nif pool",		/* si_name */
	sizeof(struct atm_nif),		/* si_blksiz */
	5,				/* si_blkcnt */
	52				/* si_maxallow */
};

struct sp_info fore_vcc_pool = {
	"fore vcc pool",		/* si_name */
	sizeof(Fore_vcc),		/* si_blksiz */
	10,				/* si_blkcnt */
	100				/* si_maxallow */
};


/*
 * Watchdog timer
 */
struct atm_time		fore_timer = {0, 0};

