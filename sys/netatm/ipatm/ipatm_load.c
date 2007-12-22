/*-
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
 * IP Over ATM Support
 * -------------------
 *
 * Support for running as a loadable kernel module
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#ifndef ATM_IP_MODULE
#include "opt_atm.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/syslog.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
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

#include <netatm/ipatm/ipatm.h>
#include <netatm/ipatm/ipatm_var.h>

#include <vm/uma.h>

/*
 * Global variables
 */
int		ipatm_vccnt = 0;		
int		ipatm_vcidle = IPATM_VCIDLE;		
u_long		last_map_ipdst = 0;
struct ipvcc*	last_map_ipvcc = NULL;

struct ip_nif	*ipatm_nif_head = NULL;

struct ipatm_stat	ipatm_stat = {0};

struct atm_time		ipatm_itimer = {0, 0};	/* VCC idle timer */

Atm_endpoint	ipatm_endpt = {
	NULL,
	ENDPT_IP,
	ipatm_ioctl,
	ipatm_getname,
	ipatm_connected,
	ipatm_cleared,
	ipatm_incoming,
	NULL,
	NULL,
	NULL,
	ipatm_cpcs_data,
	NULL,
	NULL,
	NULL,
	NULL
};

uma_zone_t	ipatm_vc_zone;

/*
 * net.harp.ip
 */
SYSCTL_NODE(_net_harp, OID_AUTO, ip, CTLFLAG_RW, 0, "IPv4 over ATM");

/*
 * net.harp.ip.ipatm_print
 */
int		ipatm_print = 0;
SYSCTL_INT(_net_harp_ip, OID_AUTO, ipatm_print, CTLFLAG_RW,
    &ipatm_print, 0, "dump IPv4-over-ATM packets");


/*
 * Local functions
 */
static int	ipatm_start(void);
static int	ipatm_stop(void);


/*
 * Local variables
 */
static struct atm_ncm	ipatm_ncm = {
	NULL,
	AF_INET,
	ipatm_ifoutput,
	ipatm_nifstat
};

static struct ipatm_listener {
	Atm_attributes	attr;
	Atm_connection	*conn;
} ipatm_listeners[] = {
{
	{	NULL,			/* nif */
		CMAPI_CPCS,		/* api */
		0,			/* api_init */
		0,			/* headin */
		0,			/* headout */
		{			/* aal */
			T_ATM_PRESENT,
			ATM_AAL5
		},
		{			/* traffic */
			T_ATM_PRESENT,
			{
				{
					T_ATM_ABSENT,
					0,
					T_ATM_ABSENT,
					T_ATM_ABSENT,
					T_ATM_ABSENT,
					T_ATM_ABSENT,
					T_NO
				},
				{
					T_ATM_ABSENT,
					0,
					T_ATM_ABSENT,
					T_ATM_ABSENT,
					T_ATM_ABSENT,
					T_ATM_ABSENT,
					T_NO
				},
				T_YES
			},
		},
		{			/* bearer */
			T_ATM_ANY
		},
		{			/* bhli */
			T_ATM_ABSENT
		},
		{			/* blli */
			T_ATM_PRESENT,
			T_ATM_ABSENT,
			{
				{
					T_ATM_SIMPLE_ID,
				},
				{
					T_ATM_ABSENT
				}
			}
		},
		{			/* llc */
			T_ATM_PRESENT,
			{
				T_ATM_LLC_SHARING,
				IPATM_LLC_LEN,
				IPATM_LLC_HDR
			}
		},
		{			/* called */
			T_ATM_ANY
		},
		{			/* calling */
			T_ATM_ANY
		},
		{			/* qos */
			T_ATM_PRESENT,
			{
				T_ATM_NETWORK_CODING,
				{
					T_ATM_QOS_CLASS_0,
				},
				{
					T_ATM_QOS_CLASS_0
				}
			}
		},
		{			/* transit */
			T_ATM_ANY
		},
		{			/* cause */
			T_ATM_ABSENT
		},
	},
	NULL
},
{
	{	NULL,			/* nif */
		CMAPI_CPCS,		/* api */
		0,			/* api_init */
		0,			/* headin */
		0,			/* headout */
		{			/* aal */
			T_ATM_PRESENT,
			ATM_AAL5
		},
		{			/* traffic */
			T_ATM_PRESENT,
			{
				{
					T_ATM_ABSENT,
					0,
					T_ATM_ABSENT,
					T_ATM_ABSENT,
					T_ATM_ABSENT,
					T_ATM_ABSENT,
					T_NO
				},
				{
					T_ATM_ABSENT,
					0,
					T_ATM_ABSENT,
					T_ATM_ABSENT,
					T_ATM_ABSENT,
					T_ATM_ABSENT,
					T_NO
				},
				T_YES
			},
		},
		{			/* bearer */
			T_ATM_ANY
		},
		{			/* bhli */
			T_ATM_ABSENT
		},
		{			/* blli */
			T_ATM_ABSENT,
			T_ATM_ABSENT
		},
		{			/* llc */
			T_ATM_ABSENT
		},
		{			/* called */
			T_ATM_ANY
		},
		{			/* calling */
			T_ATM_ANY
		},
		{			/* qos */
			T_ATM_PRESENT,
			{
				T_ATM_NETWORK_CODING,
				{
					T_ATM_QOS_CLASS_0,
				},
				{
					T_ATM_QOS_CLASS_0
				}
			}
		},
		{			/* transit */
			T_ATM_ANY
		},
		{			/* cause */
			T_ATM_ABSENT
		},
	},
	NULL
},
{
	{	NULL,			/* nif */
		CMAPI_CPCS,		/* api */
		0,			/* api_init */
		0,			/* headin */
		0,			/* headout */
		{			/* aal */
			T_ATM_PRESENT,
			ATM_AAL3_4
		},
		{			/* traffic */
			T_ATM_PRESENT,
			{
				{
					T_ATM_ABSENT,
					0,
					T_ATM_ABSENT,
					T_ATM_ABSENT,
					T_ATM_ABSENT,
					T_ATM_ABSENT,
					T_NO
				},
				{
					T_ATM_ABSENT,
					0,
					T_ATM_ABSENT,
					T_ATM_ABSENT,
					T_ATM_ABSENT,
					T_ATM_ABSENT,
					T_NO
				},
				T_YES
			},
		},
		{			/* bearer */
			T_ATM_ANY
		},
		{			/* bhli */
			T_ATM_ABSENT
		},
		{			/* blli */
			T_ATM_ABSENT,
			T_ATM_ABSENT
		},
		{			/* llc */
			T_ATM_ABSENT
		},
		{			/* called */
			T_ATM_ANY
		},
		{			/* calling */
			T_ATM_ANY
		},
		{			/* qos */
			T_ATM_PRESENT,
			{
				T_ATM_NETWORK_CODING,
				{
					T_ATM_QOS_CLASS_0,
				},
				{
					T_ATM_QOS_CLASS_0
				}
			}
		},
		{			/* transit */
			T_ATM_ANY
		},
		{			/* cause */
			T_ATM_ABSENT
		},
	},
	NULL
},
};

static struct t_atm_cause	ipatm_cause = {
	T_ATM_ITU_CODING,
	T_ATM_LOC_USER,
	T_ATM_CAUSE_UNSPECIFIED_NORMAL,
	{0, 0, 0, 0}
};


/*
 * Initialize ipatm processing
 * 
 * This will be called during module loading.  We'll just register
 * ourselves and wait for the packets to start flying.
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
ipatm_start()
{
	struct atm_pif	*pip;
	struct atm_nif	*nip;
	int	err, s, i;

	/*
	 * Verify software version
	 */
	if (atm_version != ATM_VERSION) {
		log(LOG_ERR, "version mismatch: ipatm=%d.%d kernel=%d.%d\n",
			ATM_VERS_MAJ(ATM_VERSION), ATM_VERS_MIN(ATM_VERSION),
			ATM_VERS_MAJ(atm_version), ATM_VERS_MIN(atm_version));
		return (EINVAL);
	}

	ipatm_vc_zone = uma_zcreate("ipatm vc", sizeof(struct ipvcc), NULL,
	    NULL, NULL, NULL, UMA_ALIGN_PTR, 0);
	if (ipatm_vc_zone == NULL)
		panic("ipatm_start: unable to create ipatm_vc_zone");
		
	/*
	 * Register ourselves as a network convergence module
	 */
	err = atm_netconv_register(&ipatm_ncm);
	if (err)
		goto done;

	/*
	 * Register ourselves as an ATM endpoint
	 */
	err = atm_endpoint_register(&ipatm_endpt);
	if (err)
		goto done;

	/*
	 * Get current system configuration
	 */
	s = splnet();
	for (pip = atm_interface_head; pip; pip = pip->pif_next) {
		/*
		 * Process each network interface
		 */
		for (nip = pip->pif_nif; nip; nip = nip->nif_pnext) {
			struct ifnet    *ifp = ANIF2IFP(nip);
			struct in_ifaddr	*ia;

			/*
			 * Attach interface
			 */
			err = ipatm_nifstat(NCM_ATTACH, nip, 0);
			if (err) {
				(void) splx(s);
				goto done;
			}

			/*
			 * If IP address has been set, register it
			 */
			TAILQ_FOREACH(ia, &in_ifaddrhead, ia_link) {
				if (ia->ia_ifp == ifp)
					break;
			}
			if (ia) {
				err = ipatm_nifstat(NCM_SETADDR, nip,
				    (intptr_t)ia);
				if (err) {
					(void) splx(s);
					goto done;
				}
			}
		}
	}
	(void) splx(s);

	/*
	 * Fill in union fields
	 */
	ipatm_aal5llc.aal.v.aal5.forward_max_SDU_size =
		ATM_NIF_MTU + IPATM_LLC_LEN;
	ipatm_aal5llc.aal.v.aal5.backward_max_SDU_size =
		ATM_NIF_MTU + IPATM_LLC_LEN;
	ipatm_aal5llc.aal.v.aal5.SSCS_type = T_ATM_NULL;
	ipatm_aal5llc.blli.v.layer_2_protocol.ID.simple_ID = T_ATM_BLLI2_I8802;

	ipatm_aal5null.aal.v.aal5.forward_max_SDU_size = ATM_NIF_MTU;
	ipatm_aal5null.aal.v.aal5.backward_max_SDU_size = ATM_NIF_MTU;
	ipatm_aal5null.aal.v.aal5.SSCS_type = T_ATM_NULL;

	ipatm_aal4null.aal.v.aal4.forward_max_SDU_size = ATM_NIF_MTU;
	ipatm_aal4null.aal.v.aal4.backward_max_SDU_size = ATM_NIF_MTU;
	ipatm_aal4null.aal.v.aal4.SSCS_type = T_ATM_NULL;
	ipatm_aal4null.aal.v.aal4.mid_low = 0;
	ipatm_aal4null.aal.v.aal4.mid_high = 1023;

	/*
	 * Listen for incoming calls
	 */
	for (i = 0;
	     i < (sizeof(ipatm_listeners) / sizeof(struct ipatm_listener));
	     i++) {
		struct attr_aal	*aalp = &ipatm_listeners[i].attr.aal;
		int	maxsdu = ATM_NIF_MTU;

		/*
		 * Fill in union fields
		 */
		if (ipatm_listeners[i].attr.blli.tag_l2 == T_ATM_PRESENT) {
			struct t_atm_blli *bp = &ipatm_listeners[i].attr.blli.v;

			bp->layer_2_protocol.ID.simple_ID = T_ATM_BLLI2_I8802;
			maxsdu += IPATM_LLC_LEN;
		}
		if (aalp->type == ATM_AAL5) {
			aalp->v.aal5.forward_max_SDU_size = maxsdu;
			aalp->v.aal5.backward_max_SDU_size = maxsdu;
			aalp->v.aal5.SSCS_type = T_ATM_NULL;
		} else {
			aalp->v.aal4.forward_max_SDU_size = maxsdu;
			aalp->v.aal4.backward_max_SDU_size = maxsdu;
			aalp->v.aal4.SSCS_type = T_ATM_NULL;
			aalp->v.aal4.mid_low = 0;
			aalp->v.aal4.mid_high = 1023;
		}

		/*
		 * Now start listening
		 */
		if ((err = atm_cm_listen(NULL, &ipatm_endpt,
				(void *)(intptr_t)i, &ipatm_listeners[i].attr,
				&ipatm_listeners[i].conn, -1)) != 0)
			goto done;
	}

	/*
	 * Start background VCC idle timer
	 */
	atm_timeout(&ipatm_itimer, IPATM_IDLE_TIME, ipatm_itimeout);

done:
	return (err);
}


/*
 * Halt ipatm processing 
 * 
 * This will be called just prior to unloading the module from
 * memory.  All IP VCCs must be terminated before the protocol can 
 * be shutdown.
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
ipatm_stop()
{
	struct ip_nif	*inp;
	int	err = 0, i;
	int	s = splnet();

	/*
	 * Any VCCs still open??
	 */
	if (ipatm_vccnt) {

		/* Yes, can't stop now */
		err = EBUSY;
		goto done;
	}

	/*
	 * Kill VCC idle timer
	 */
	(void) atm_untimeout(&ipatm_itimer);

	/*
	 * Stop listening for incoming calls
	 */
	for (i = 0;
	     i < (sizeof(ipatm_listeners) / sizeof(struct ipatm_listener));
	     i++) {
		if (ipatm_listeners[i].conn != NULL) {
			(void) atm_cm_release(ipatm_listeners[i].conn,
					&ipatm_cause);
		}
	}

	/*
	 * Detach all our interfaces
	 */
	while ((inp = ipatm_nif_head) != NULL) {
		(void) ipatm_nifstat(NCM_DETACH, inp->inf_nif, 0);
	}
	
	/*
	 * De-register from system
	 */
	(void) atm_netconv_deregister(&ipatm_ncm);
	(void) atm_endpoint_deregister(&ipatm_endpt);

	/*
	 * Free up our storage pools
	 */
	uma_zdestroy(ipatm_vc_zone);
done:
	(void) splx(s);
	return (err);
}


#ifdef ATM_IP_MODULE 
/*
 *******************************************************************
 *
 * Loadable Module Support
 *
 *******************************************************************
 */
static int	ipatm_doload(void);
static int	ipatm_dounload(void);

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
ipatm_doload()
{
	int	err = 0;

	/*
	 * Start us up
	 */
	err = ipatm_start();
	if (err)
		/* Problems, clean up */
		(void)ipatm_stop();

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
ipatm_dounload()
{
	int	err = 0;

	/*
	 * OK, try to clean up our mess
	 */
	err = ipatm_stop();

	return (err);
}




#include <sys/exec.h>
#include <sys/sysent.h>
#include <sys/lkm.h>

/*
 * Loadable miscellaneous module description
 */
MOD_MISC(ipatm);


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
ipatm_load(lkmtp, cmd)
	struct lkm_table	*lkmtp;
	int		cmd;
{
	return(ipatm_doload());
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
ipatm_unload(lkmtp, cmd)
	struct lkm_table	*lkmtp;
	int		cmd;
{
	return(ipatm_dounload());
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
ipatm_mod(lkmtp, cmd, ver)
	struct lkm_table	*lkmtp;
	int		cmd;
	int		ver;
{
	MOD_DISPATCH(ipatm, lkmtp, cmd, ver,
		ipatm_load, ipatm_unload, lkm_nullcmd);
}

#else	/* !ATM_IP_MODULE */

/*
 *******************************************************************
 *
 * Kernel Compiled Module Support
 *
 *******************************************************************
 */
static void	ipatm_doload(void *);

SYSINIT(atmipatm, SI_SUB_PROTO_END, SI_ORDER_ANY, ipatm_doload, NULL)

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
ipatm_doload(void *arg)
{
	int	err = 0;

	/*
	 * Start us up
	 */
	err = ipatm_start();
	if (err) {
		/* Problems, clean up */
		(void)ipatm_stop();

		log(LOG_ERR, "IP over ATM unable to initialize (%d)!!\n", err);
	}
	return;
}
#endif	/* ATM_IP_MODULE */

