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
 * Core ATM Services
 * -----------------
 *
 * ATM interface management
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/sockio.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/syslog.h>
#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netatm/port.h>
#include <netatm/queue.h>
#include <netatm/atm.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_sap.h>
#include <netatm/atm_cm.h>
#include <netatm/atm_if.h>
#include <netatm/atm_ioctl.h>
#include <netatm/atm_sigmgr.h>
#include <netatm/atm_stack.h>
#include <netatm/atm_pcb.h>
#include <netatm/atm_var.h>

#ifndef lint
__RCSID("@(#) $FreeBSD$");
#endif


/*
 * Local functions
 */
static int	atm_physif_ioctl(int, caddr_t, caddr_t);
static int	atm_netif_rtdel(struct radix_node *, void *);
static int	atm_if_ioctl(struct ifnet *, u_long, caddr_t);
static int	atm_ifparse(char *, char *, int, int *);

/*
 * Local variables
 */
static int	(*atm_ifouttbl[AF_MAX+1])
			(struct ifnet *, KBuffer *, struct sockaddr *)
				= {NULL};


/*
 * Register an ATM physical interface
 * 
 * Each ATM device interface must register itself here upon completing
 * its internal initialization.  This applies to both linked and loaded
 * device drivers.  The interface must be registered before a signalling
 * manager can be attached.
 *
 * Arguments:
 *	cup	pointer to interface's common unit structure
 *	name	pointer to device name string
 *	sdp	pointer to interface's stack services
 *
 * Returns:
 *	0	registration successful
 *	errno	registration failed - reason indicated
 *
 */
int
atm_physif_register(cup, name, sdp)
	Cmn_unit		*cup;
	char			*name;
	struct stack_defn	*sdp;
{
	struct atm_pif	*pip;
	int		s;

	/*
	 * See if we need to be initialized
	 */
	if (!atm_init)
		atm_initialize();

	/*
	 * Make sure we're not already registered
	 */
	if (cup->cu_flags & CUF_REGISTER) {
		return (EALREADY);
	}

	s = splnet();

	/*
	 * Make sure an interface is only registered once
	 */
	for (pip = atm_interface_head; pip != NULL; pip = pip->pif_next) {
		if ((cup->cu_unit == pip->pif_unit) && 
		    (strcmp(name, pip->pif_name) == 0)) {
			(void) splx(s);
			return (EEXIST);
		}
	}

	/*
	 * Fill in physical interface parameters
	 */
	pip = &cup->cu_pif;
	pip->pif_name = name;
	pip->pif_unit = cup->cu_unit;
	pip->pif_flags = PIF_UP;
	pip->pif_services = sdp;
	pip->pif_ioctl = atm_physif_ioctl;

	/*
	 * Link in the interface and mark us registered
	 */
	LINK2TAIL(pip, struct atm_pif, atm_interface_head, pif_next);
	cup->cu_flags |= CUF_REGISTER;

	(void) splx(s);
	return (0);
}


/*
 * De-register an ATM physical interface
 * 
 * Each ATM interface must de-register itself before downing the interface.  
 * The interface's signalling manager will be detached and any network
 * interface and VCC control blocks will be freed.  
 *
 * Arguments:
 *	cup	pointer to interface's common unit structure
 *
 * Returns:
 *	0	de-registration successful
 *	errno	de-registration failed - reason indicated
 *
 */
int
atm_physif_deregister(cup)
	Cmn_unit	*cup;
{
	struct atm_pif	*pip = (struct atm_pif *)&cup->cu_pif;
	Cmn_vcc		*cvp;
	int	err;
	int	s = splnet();

	/*
	 * Detach and deregister, if needed
	 */
	if ((cup->cu_flags & CUF_REGISTER)) {

		/*
		 * Detach from signalling manager
		 */
		if (pip->pif_sigmgr != NULL) {
			err = atm_sigmgr_detach(pip);
			if (err && (err != ENOENT)) {
				(void) splx(s);
				return (err);
			}
		}

		/*
		 * Make sure signalling manager is detached
		 */
		if (pip->pif_sigmgr != NULL) {
			(void) splx(s);
			return (EBUSY);
		}

		/*
		 * Unlink interface
		 */
		UNLINK(pip, struct atm_pif, atm_interface_head, pif_next);

		cup->cu_flags &= ~CUF_REGISTER;
	}

	/*
	 * Free all of our network interfaces
	 */
	atm_physif_freenifs(pip, cup->cu_nif_zone);

	/*
	 * Free unit's vcc information
	 */
	cvp = cup->cu_vcc;
	while (cvp) {
		uma_zfree(cup->cu_vcc_zone, cvp);
		cvp = cvp->cv_next;
	}
	cup->cu_vcc = (Cmn_vcc *)NULL;

	(void) splx(s);

	return (0);
}


/*
 * Free all network interfaces on a physical interface
 *
 * Arguments
 *	pip		pointer to physical interface structure
 *
 * Returns
 *	none
 *
 */
void
atm_physif_freenifs(pip, zone)
	struct atm_pif	*pip;
	uma_zone_t 	zone;
{
	struct atm_nif	*nip = pip->pif_nif;
	int	s = splnet();

	while ( nip ) 
	{
		/*
		 * atm_nif_detach zeros pointers - save so we can
		 * walk the chain.
		 */
		struct atm_nif	*nipp = nip->nif_pnext;

		/*
		 * Clean up network i/f trails
		 */
		atm_nif_detach(nip);
		uma_zfree(zone, nip);
		nip = nipp;
	}
	pip->pif_nif = (struct atm_nif *)NULL;

	(void) splx(s);

	return;
}

/*
 * Handle physical interface ioctl's
 *
 * See <netatm/atm_ioctl.h> for definitions.
 *
 * Called at splnet.
 *
 * Arguments:
 *	code			Ioctl function (sub)code
 *	data			Data block. On input contains command,
 *					on output, contains results
 *	arg			Optional code specific arguments
 *
 * Returns:
 *	0			Request processed successfully
 *	errno			Request failed - reason code
 *
 */
static int
atm_physif_ioctl(code, data, arg)
	int	code;
	caddr_t	data;
	caddr_t	arg;
{
	struct atminfreq	*aip = (struct atminfreq *)data;
	struct atmsetreq	*asr = (struct atmsetreq *)data;
	struct atm_pif		*pip;
	struct atm_nif		*nip;
	struct sigmgr		*smp;
	struct siginst		*sip;
	struct ifnet		*ifp;
	Cmn_unit		*cup;
	Atm_config		*acp;
	caddr_t			buf = aip->air_buf_addr;
	struct air_phy_stat_rsp	*apsp;
	struct air_int_rsp	apr;
	struct air_netif_rsp	anr;
	struct air_cfg_rsp	acr;
	int			count, len, buf_len = aip->air_buf_len;
	int			err = 0;
	char			ifname[2*IFNAMSIZ];
	struct ifaddr		*ifa;
	struct in_ifaddr	*ia;
	struct sockaddr_dl	*sdl;
 

	switch ( aip->air_opcode ) {

	case AIOCS_INF_INT:
		/*
		 * Get physical interface information
		 */
		aip = (struct atminfreq *)data;
		pip = (struct atm_pif *)arg;

		/*
		 * Make sure there's room in user buffer
		 */
		if (aip->air_buf_len < sizeof(apr)) {
			err = ENOSPC;
			break;
		}

		/*
		 * Fill in info to be returned
		 */
		bzero((caddr_t)&apr, sizeof(apr));
		smp = pip->pif_sigmgr;
		sip = pip->pif_siginst;
		(void) snprintf(apr.anp_intf, sizeof(apr.anp_intf),
			"%s%d", pip->pif_name, pip->pif_unit );
		if ( pip->pif_nif )
		{
			strcpy(apr.anp_nif_pref, pip->pif_nif->nif_if.if_name);

			nip = pip->pif_nif;
			while ( nip ) {
				apr.anp_nif_cnt++;
				nip = nip->nif_pnext;
			}
		}
		if (sip) {
			ATM_ADDR_COPY(&sip->si_addr, &apr.anp_addr);
			ATM_ADDR_COPY(&sip->si_subaddr, &apr.anp_subaddr);
			apr.anp_sig_proto = smp->sm_proto;
			apr.anp_sig_state = sip->si_state;
		}

		/*
		 * Copy data to user buffer
		 */
		err = copyout((caddr_t)&apr, aip->air_buf_addr, sizeof(apr));
		if (err)
			break;

		/*
		 * Update buffer pointer/count
		 */
		aip->air_buf_addr += sizeof(apr);
		aip->air_buf_len -= sizeof(apr);
		break;

	case AIOCS_INF_NIF:
		/*
		 * Get network interface information
		 */
		aip = (struct atminfreq *)data;
		nip = (struct atm_nif *)arg;
		ifp = &nip->nif_if;
		pip = nip->nif_pif;

		/*
		 * Make sure there's room in user buffer
		 */
		if (aip->air_buf_len < sizeof(anr)) {
			err = ENOSPC;
			break;
		}

		/*
		 * Fill in info to be returned
		 */
		bzero((caddr_t)&anr, sizeof(anr));
		(void) snprintf(anr.anp_intf, sizeof(anr.anp_intf),
		    "%s%d", ifp->if_name, ifp->if_unit);
		IFP_TO_IA(ifp, ia);
		if (ia) {
			anr.anp_proto_addr = *ia->ia_ifa.ifa_addr;
		}
		(void) snprintf(anr.anp_phy_intf, sizeof(anr.anp_phy_intf),
		    "%s%d", pip->pif_name, pip->pif_unit);

		/*
		 * Copy data to user buffer
		 */
		err = copyout((caddr_t)&anr, aip->air_buf_addr, sizeof(anr));
		if (err)
			break;

		/*
		 * Update buffer pointer/count
		 */
		aip->air_buf_addr += sizeof(anr);
		aip->air_buf_len -= sizeof(anr);
		break;

	case AIOCS_INF_PIS:
		/*
		 * Get per interface statistics
		 */
		pip = (struct atm_pif *)arg;
		if ( pip == NULL )
			return ( ENXIO );
		snprintf ( ifname, sizeof(ifname),
		    "%s%d", pip->pif_name, pip->pif_unit );

		/*
		 * Cast response into users buffer
		 */
		apsp = (struct air_phy_stat_rsp *)buf;

		/*
		 * Sanity check
		 */
		len = sizeof ( struct air_phy_stat_rsp );
		if ( buf_len < len )
			return ( ENOSPC );

		/*
		 * Copy interface name into response
		 */
		if ((err = copyout ( ifname, apsp->app_intf, IFNAMSIZ)) != 0)
			break;

		/*
		 * Copy counters
		 */
		if ((err = copyout(&pip->pif_ipdus, &apsp->app_ipdus,
		    len - sizeof(apsp->app_intf))) != 0)
			break;

		/*
		 * Adjust buffer elements
		 */
		buf += len;
		buf_len -= len;

		aip->air_buf_addr = buf;
		aip->air_buf_len = buf_len;
		break;

	case AIOCS_SET_NIF:
		/*
		 * Set NIF - allow user to configure 1 or more logical
		 *	interfaces per physical interface.
		 */

		/*
		 * Get pointer to physical interface structure from
		 * ioctl argument.
		 */
		pip = (struct atm_pif *)arg;
		cup = (Cmn_unit *)pip;

		/*
		 * Sanity check - are we already connected to something?
		 */
		if ( pip->pif_sigmgr )
		{
			err = EBUSY;
			break;
		}

		/*
		 * Free any previously allocated NIFs
		 */
		atm_physif_freenifs(pip, cup->cu_nif_zone);

		/*
		 * Add list of interfaces
		 */
		for ( count = 0; count < asr->asr_nif_cnt; count++ )
		{
			nip = uma_zalloc(cup->cu_nif_zone, M_WAITOK | M_ZERO);
			if ( nip == NULL )
			{
				/*
				 * Destroy any successful nifs
				 */
				atm_physif_freenifs(pip, cup->cu_nif_zone);
				err = ENOMEM;
				break;
			}

			nip->nif_pif = pip;
			ifp = &nip->nif_if;

			strcpy ( nip->nif_name, asr->asr_nif_pref );
			nip->nif_sel = count;

			ifp->if_name = nip->nif_name;
			ifp->if_unit = count;
			ifp->if_mtu = ATM_NIF_MTU;
			ifp->if_flags = IFF_UP | IFF_BROADCAST | IFF_RUNNING;
			ifp->if_output = atm_ifoutput;
			ifp->if_ioctl = atm_if_ioctl;
			ifp->if_snd.ifq_maxlen = ifqmaxlen;
			/*
			 * Set if_type and if_baudrate
			 */
			ifp->if_type = IFT_ATM;
			switch ( cup->cu_config.ac_media ) {
			case MEDIA_TAXI_100:
				ifp->if_baudrate = 100000000;
				break;
			case MEDIA_TAXI_140:
				ifp->if_baudrate = 140000000;
				break;
			case MEDIA_OC3C:
			case MEDIA_OC12C:
			case MEDIA_UTP155:
				ifp->if_baudrate = 155000000;
				break;
			case MEDIA_UNKNOWN:
				ifp->if_baudrate = 9600;
				break;
			}
			if ((err = atm_nif_attach(nip)) != 0) {
				uma_zfree(cup->cu_nif_zone, nip);
				/*
				 * Destroy any successful nifs
				 */
				atm_physif_freenifs(pip, cup->cu_nif_zone);
				break;
			}
			/*
			 * Set macaddr in <Link> address
			 */
			ifp->if_addrlen = 6;
			ifa = ifaddr_byindex(ifp->if_index);
			if ( ifa ) {
				sdl = (struct sockaddr_dl *)
					ifa->ifa_addr;
				sdl->sdl_type = IFT_ETHER;
				sdl->sdl_alen = ifp->if_addrlen;
				bcopy ( (caddr_t)&cup->cu_config.ac_macaddr,
					LLADDR(sdl), ifp->if_addrlen );
			}
		}
		break;

	case AIOCS_INF_CFG:
		/*
		 * Get adapter configuration information
		 */
		aip = (struct atminfreq *)data;
		pip = (struct atm_pif *)arg;
		cup = (Cmn_unit *)pip;
		acp = &cup->cu_config;

		/*
		 * Make sure there's room in user buffer
		 */
		if (aip->air_buf_len < sizeof(acr)) {
			err = ENOSPC;
			break;
		}

		/*
		 * Fill in info to be returned
		 */
		bzero((caddr_t)&acr, sizeof(acr));
		(void) snprintf(acr.acp_intf, sizeof(acr.acp_intf),
		    "%s%d", pip->pif_name, pip->pif_unit);
		bcopy((caddr_t)acp, (caddr_t)&acr.acp_cfg,
				sizeof(Atm_config));

		/*
		 * Copy data to user buffer
		 */
		err = copyout((caddr_t)&acr, aip->air_buf_addr,
				sizeof(acr));
		if (err)
			break;

		/*
		 * Update buffer pointer/count
		 */
		aip->air_buf_addr += sizeof(acr);
		aip->air_buf_len -= sizeof(acr);
		break;

	case AIOCS_INF_VST:
		/*
		 * Pass off to device-specific handler
		 */
		cup = (Cmn_unit *)arg;
		if (cup == NULL)
			err = ENXIO;
		else
			err = (*cup->cu_ioctl)(code, data, arg);
		break;

	default:
		err = ENOSYS;
	}

	return ( err );
}


/*
 * Register a Network Convergence Module
 * 
 * Each ATM network convergence module must register itself here before
 * it will receive network interface status notifications. 
 *
 * Arguments:
 *	ncp	pointer to network convergence definition structure
 *
 * Returns:
 *	0	registration successful
 *	errno	registration failed - reason indicated
 *
 */
int
atm_netconv_register(ncp)
	struct atm_ncm	*ncp;
{
	struct atm_ncm	*tdp;
	int		s = splnet();

	/*
	 * See if we need to be initialized
	 */
	if (!atm_init)
		atm_initialize();

	/*
	 * Validate protocol family
	 */
	if (ncp->ncm_family > AF_MAX) {
		(void) splx(s);
		return (EINVAL);
	}

	/*
	 * Ensure no duplicates
	 */
	for (tdp = atm_netconv_head; tdp != NULL; tdp = tdp->ncm_next) {
		if (tdp->ncm_family == ncp->ncm_family) {
			(void) splx(s);
			return (EEXIST);
		}
	}

	/*
	 * Add module to list
	 */
	LINK2TAIL(ncp, struct atm_ncm, atm_netconv_head, ncm_next);

	/*
	 * Add new interface output function
	 */
	atm_ifouttbl[ncp->ncm_family] = ncp->ncm_ifoutput;

	(void) splx(s);
	return (0);
}


/*
 * De-register an ATM Network Convergence Module
 * 
 * Each ATM network convergence provider must de-register its registered 
 * service(s) before terminating.  Specifically, loaded kernel modules
 * must de-register their services before unloading themselves.
 *
 * Arguments:
 *	ncp	pointer to network convergence definition structure
 *
 * Returns:
 *	0	de-registration successful 
 *	errno	de-registration failed - reason indicated
 *
 */
int
atm_netconv_deregister(ncp)
	struct atm_ncm	*ncp;
{
	int	found, s = splnet();

	/*
	 * Remove module from list
	 */
	UNLINKF(ncp, struct atm_ncm, atm_netconv_head, ncm_next, found);

	if (!found) {
		(void) splx(s);
		return (ENOENT);
	}

	/*
	 * Remove module's interface output function
	 */
	atm_ifouttbl[ncp->ncm_family] = NULL;

	(void) splx(s);
	return (0);
}


/*
 * Attach an ATM Network Interface
 * 
 * Before an ATM network interface can be used by the system, the owning
 * device interface must attach the network interface using this function.
 * The physical interface for this network interface must have been previously
 * registered (using atm_interface_register).  The network interface will be
 * added to the kernel's interface list and to the physical interface's list.
 * The caller is responsible for initializing the control block fields.
 *
 * Arguments:
 *	nip	pointer to atm network interface control block
 *
 * Returns:
 *	0	attach successful
 *	errno	attach failed - reason indicated
 *
 */
int
atm_nif_attach(nip)
	struct atm_nif	*nip;
{
	struct atm_pif	*pip, *pip2;
	struct ifnet	*ifp;
	struct atm_ncm	*ncp;
	int		s;

	ifp = &nip->nif_if;
	pip = nip->nif_pif;

	s = splimp();

	/*
	 * Verify physical interface is registered
	 */
	for (pip2 = atm_interface_head; pip2 != NULL; pip2 = pip2->pif_next) {
		if (pip == pip2)
			break;
	}
	if ((pip == NULL) || (pip2 == NULL)) {
		(void) splx(s);
		return (EFAULT);
	}

	/*
	 * Add to system interface list 
	 */
	if_attach(ifp);

	/*
	 * Add to physical interface list
	 */
	LINK2TAIL(nip, struct atm_nif, pip->pif_nif, nif_pnext);

	/*
	 * Notify network convergence modules of new network i/f
	 */
	for (ncp = atm_netconv_head; ncp; ncp = ncp->ncm_next) {
		int	err;

		err = (*ncp->ncm_stat)(NCM_ATTACH, nip, 0);
		if (err) {
			atm_nif_detach(nip);
			(void) splx(s);
			return (err);
		}
	}

	(void) splx(s);
	return (0);
}


/*
 * Detach an ATM Network Interface
 * 
 * Before an ATM network interface control block can be freed, all kernel
 * references to/from this block must be released.  This function will delete
 * all routing references to the interface and free all interface addresses
 * for the interface.  The network interface will then be removed from the
 * kernel's interface list and from the owning physical interface's list.
 * The caller is responsible for free'ing the control block.
 *
 * Arguments:
 *	nip	pointer to atm network interface control block
 *
 * Returns:
 *	none
 *
 */
void
atm_nif_detach(nip)
	struct atm_nif	*nip;
{
	struct atm_ncm	*ncp;
	int		s, i;
	struct ifnet	*ifp = &nip->nif_if;
	struct ifaddr	*ifa;
	struct in_ifaddr	*ia;
	struct radix_node_head	*rnh;


	s = splimp();

	/*
	 * Notify convergence modules of network i/f demise
	 */
	for (ncp = atm_netconv_head; ncp; ncp = ncp->ncm_next) {
		(void) (*ncp->ncm_stat)(NCM_DETACH, nip, 0);
	}

	/*
	 * Mark interface down
	 */
	if_down(ifp);

	/*
	 * Free all interface routes and addresses
	 */
	while (1) {
		IFP_TO_IA(ifp, ia);
		if (ia == NULL)
			break;

		/* Delete interface route */
		in_ifscrub(ifp, ia);

		/* Remove interface address from queues */
		ifa = &ia->ia_ifa;
		TAILQ_REMOVE(&ifp->if_addrhead, ifa, ifa_link);
		TAILQ_REMOVE(&in_ifaddrhead, ia, ia_link);

		/* Free interface address */
		IFAFREE(ifa);
	}

	/*
	 * Delete all remaining routes using this interface
	 * Unfortuneatly the only way to do this is to slog through
	 * the entire routing table looking for routes which point
	 * to this interface...oh well...
	 */
	for (i = 1; i <= AF_MAX; i++) {
		if ((rnh = rt_tables[i]) == NULL)
			continue;
		RADIX_NODE_HEAD_LOCK(rnh);
		(void) rnh->rnh_walktree(rnh, atm_netif_rtdel, ifp);
		RADIX_NODE_HEAD_UNLOCK(rnh);
	}

	/*
	 * Remove from system interface list (ie. if_detach())
	 */
	IFNET_WLOCK();
	TAILQ_REMOVE(&ifnet, ifp, if_link);
	IFNET_WUNLOCK();

	/*
	 * Remove from physical interface list
	 */
	UNLINK(nip, struct atm_nif, nip->nif_pif->pif_nif, nif_pnext);

	(void) splx(s);
}


/*
 * Delete Routes for a Network Interface
 * 
 * Called for each routing entry via the rnh->rnh_walktree() call above
 * to delete all route entries referencing a detaching network interface.
 *
 * Arguments:
 *	rn	pointer to node in the routing table
 *	arg	argument passed to rnh->rnh_walktree() - detaching interface
 *
 * Returns:
 *	0	successful
 *	errno	failed - reason indicated
 *
 */
static int
atm_netif_rtdel(rn, arg)
	struct radix_node	*rn;
	void			*arg;
{
	struct rtentry	*rt = (struct rtentry *)rn;
	struct ifnet	*ifp = arg;
	int		err;

	if (rt->rt_ifp == ifp) {

		/*
		 * Protect (sorta) against walktree recursion problems
		 * with cloned routes
		 */
		if ((rt->rt_flags & RTF_UP) == 0)
			return (0);

		err = rtrequest(RTM_DELETE, rt_key(rt), rt->rt_gateway,
				rt_mask(rt), rt->rt_flags,
				(struct rtentry **) NULL);
		if (err) {
			log(LOG_WARNING, "atm_netif_rtdel: error %d\n", err);
		}
	}

	return (0);
}


/*
 * Set an ATM Network Interface address
 * 
 * This is called from a device interface when processing an SIOCSIFADDR
 * ioctl request.  We just notify all convergence modules of the new address
 * and hope everyone has non-overlapping interests, since if someone reports
 * an error we don't go back and tell everyone to undo the change.
 *
 * Arguments:
 *	nip	pointer to atm network interface control block
 *	ifa	pointer to new interface address
 *
 * Returns:
 *	0	set successful
 *	errno	set failed - reason indicated
 *
 */
int
atm_nif_setaddr(nip, ifa)
	struct atm_nif	*nip;
	struct ifaddr	*ifa;
{
	struct atm_ncm	*ncp;
	int	err = 0, s = splnet();

	/*
	 * Notify convergence modules of network i/f change
	 */
	for (ncp = atm_netconv_head; ncp; ncp = ncp->ncm_next) {
		err = (*ncp->ncm_stat)(NCM_SETADDR, nip, (intptr_t)ifa);
		if (err)
			break;
	}
	(void) splx(s);

	return (err);
}


/*
 * ATM Interface Packet Output
 * 
 * All ATM network interfaces must have their ifnet if_output address set to
 * this function.  Since no existing network layer code is to be modified 
 * for ATM support, this function serves as the hook to allow network output
 * packets to be assigned to their proper outbound VCC.  Each network address
 * family which is to be supported over ATM must be assigned an output
 * packet processing function via atm_netconv_register().
 *
 * Arguments:
 *	ifp	pointer to ifnet structure
 *	m	pointer to packet buffer chain to be output
 *	dst	pointer to packet's network destination address
 *
 * Returns:
 *	0	packet queued to interface
 *	errno	output failed - reason indicated
 *
 */
int
atm_ifoutput(ifp, m, dst, rt)
	struct ifnet	*ifp;
	KBuffer		*m;
	struct sockaddr	*dst;
	struct rtentry	*rt;
{
	u_short		fam = dst->sa_family;
	int		(*func)(struct ifnet *, KBuffer *,
					struct sockaddr *);

	/*
	 * Validate address family
	 */
	if (fam > AF_MAX) {
		KB_FREEALL(m);
		return (EAFNOSUPPORT);
	}

	/*
	 * Hand packet off for dst-to-VCC mapping
	 */
	func = atm_ifouttbl[fam];
	if (func == NULL) {
		KB_FREEALL(m);
		return (EAFNOSUPPORT);
	}
	return ((*func)(ifp, m, dst));
}


/*
 * Handle interface ioctl requests. 
 *
 * Arguments:
 *	ifp		pointer to network interface structure
 *	cmd		IOCTL cmd
 *	data		arguments to/from ioctl
 *
 * Returns:
 *	error		errno value
 */
static int
atm_if_ioctl(ifp, cmd, data)
	struct ifnet *ifp;
	u_long	cmd;
	caddr_t data;
{
	register struct ifreq *ifr = (struct ifreq *)data;
	struct atm_nif	*nip = (struct atm_nif *)ifp;
	int	error = 0;
	int	s = splnet();

	switch ( cmd )
	{
	case SIOCGIFADDR:
		bcopy ( (caddr_t)&(nip->nif_pif->pif_macaddr),
			(caddr_t)ifr->ifr_addr.sa_data, 
			sizeof(struct mac_addr) );
		break;

	case SIOCSIFADDR:
		error = atm_nif_setaddr ( nip, (struct ifaddr *)data);
		ifp->if_flags |= IFF_UP | IFF_RUNNING | IFF_BROADCAST;
		break;

	case SIOCGIFFLAGS:
		*(int *)data = ifp->if_flags;
		break;

	case SIOCSIFFLAGS:
		break;

	default:
		error = EINVAL;
		break;
	}

	(void) splx(s);
	return ( error );
}


/*
 * Parse interface name
 * 
 * Parses an interface name string into a name and a unit component.
 *
 * Arguments:
 *	name	pointer to interface name string
 *	namep	address to store interface name
 *	size	size available at namep
 *	unitp	address to store interface unit number
 *
 * Returns:
 *	0 	name parsed
 *	else	parse error
 *
 */
static int
atm_ifparse(name, namep, size, unitp)
	char		*name;
	char		*namep;
	int		size;
	int		*unitp;
{
	char		*cp, *np;
	int		len = 0, unit = 0;

	/*
	 * Separate supplied string into name and unit parts.
	 */
	cp = name;
	np = namep;
	while (*cp) {
		if (*cp >= '0' && *cp <= '9')
			break;
		if (++len >= size)
			return (-1);
		*np++ = *cp++;
	}
	*np = '\0';
	while (*cp && *cp >= '0' && *cp <= '9')
		unit = 10 * unit + *cp++ - '0';

	*unitp = unit;

	return (0);
}


/*
 * Locate ATM physical interface via name
 * 
 * Uses the supplied interface name string to locate a registered
 * ATM physical interface.
 *
 * Arguments:
 *	name	pointer to interface name string
 *
 * Returns:
 *	0 	interface not found
 *	else	pointer to atm physical interface structure
 *
 */
struct atm_pif *
atm_pifname(name)
	char		*name;
{
	struct atm_pif	*pip;
	char		n[IFNAMSIZ];
	int		unit;

	/*
	 * Break down name
	 */
	if (atm_ifparse(name, n, sizeof(n), &unit))
		return ((struct atm_pif *)0);

	/*
	 * Look for the physical interface
	 */
	for (pip = atm_interface_head; pip; pip = pip->pif_next) {
		if ((pip->pif_unit == unit) && (strcmp(pip->pif_name, n) == 0))
			break;
	}

	return (pip);
}


/*
 * Locate ATM network interface via name
 * 
 * Uses the supplied interface name string to locate an ATM network interface.
 *
 * Arguments:
 *	name	pointer to interface name string
 *
 * Returns:
 *	0 	interface not found
 *	else	pointer to atm network interface structure
 *
 */
struct atm_nif *
atm_nifname(name)
	char		*name;
{
	struct atm_pif	*pip;
	struct atm_nif	*nip;
	char		n[IFNAMSIZ];
	int		unit;

	/*
	 * Break down name
	 */
	if (atm_ifparse(name, n, sizeof(n), &unit))
		return ((struct atm_nif *)0);

	/*
	 * Search thru each physical interface
	 */
	for (pip = atm_interface_head; pip; pip = pip->pif_next) {
		/*
		 * Looking for network interface
		 */
		for (nip = pip->pif_nif; nip; nip = nip->nif_pnext) {
			struct ifnet	*ifp = (struct ifnet *)nip;
			if ((ifp->if_unit == unit) && 
			    (strcmp(ifp->if_name, n) == 0))
				return (nip);
		}
	}
	return (NULL);
}

