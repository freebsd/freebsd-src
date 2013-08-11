/*
 * Copyright (C) 2012 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */
#if !defined(lint)
static const char rcsid[] = "@(#)$Id$";
#endif
#include <sys/types.h>
#include <sys/conf.h>
#include <sys/sysconfig.h>
#include <machine/machlimits.h>
#include <net/net_globals.h>
#include <io/common/iotypes.h>
#include <sys/errno.h>
#include <sys/buf.h>
#include <sys/secdefines.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/disklabel.h>
#include <sys/user.h>
#include <sys/socket.h>
#include <sys/sysmacros.h>
#include <sys/syslog.h>
#include <sys/select.h>
#include <io/common/pt.h>
#include <io/common/devdriver.h>
#include <io/common/devio.h>
#include <io/common/devgetinfo.h>

#include <net/if.h>
#include <netinet/ip.h>
#include <netinet/ip_stat.h>
#include <netinet/in.h>
#include <netinet/firewall.h>

#include <netinet/if_ether.h>
#include <net/ether_driver.h>

#include "ip_compat.h"
#include "ip_fil.h"
#include "ip_nat.h"
#include "ip_state.h"
#include "ip_frag.h"
#include "ip_auth.h"
#include "ip_compat.h"
#include "ip_fil.h"

/* #undef	IPFDEBUG	*/

static int ipfopen(dev_t dev, int flags);
static int ipfclose(dev_t dev, int flags);
static int ipfread(dev_t dev, struct uio *);

/* function prototypes */
int	ipfilter_attach(void);
int	ipfilter_detach(void);
void	ipfilter_ifattach(void);
void	ipfilter_ifdetach(void);
int	ipfilter_ifioctl(struct ifnet *, unsigned int, caddr_t);
#if TRU64 >= 1885
int	ipfilter_ifoutput(struct ifnet *, struct mbuf *, struct sockaddr *,
			  struct rtentry *, char *);
#else
int	ipfilter_ifoutput(struct ifnet *, struct mbuf *, struct sockaddr *,
			  struct rtentry *);
#endif
void	ipfilter_in_control(struct socket *, u_int, caddr_t *, struct ifnet *);
void	ipfilter_ip_input(struct mbuf *m);
int	ipfilter_ip_output(struct ifnet *, struct mbuf *, struct in_route *,
			   int, struct ip_moptions *);
void	ipfilter_configure_callback __P((int, int, int, int));
void	ipfilter_timer(void);
void	ipfilter_clock(void *arg);
int	ipfilteropen(dev_t, int, int);
int	ipfilterread(dev_t, struct uio *, int);
int	ipfilterclose(dev_t, int, int);
int	ipfilterwrite(dev_t, struct uio *);
int	ipfilterioctl(dev_t, u_int, caddr_t, int);

extern	int	nodev(), nulldev();
extern	task_t	first_task;
extern	ipfrwlock_t	ipf_tru64;
extern	void	ipf_timer_func __P((void *));

struct	dsent	ipfilter_devsw_entry = {
	ipfilteropen,
	ipfilterclose,
	nodev,		/* d_strategy */
	ipfilterread,
	ipfilterwrite,
	ipfilterioctl,
	nodev,		/* d_dump */
	nodev,		/* d_psize */
	nulldev,	/* d_stop */
	nulldev,	/* d_reset */
	nulldev,	/* d_select */
	0,		/* d_mmap */
	0,		/* d_segmap */
	NULL,		/* d_ttys */
	DEV_FUNNEL_NULL,/* SMP-safe */
	0,		/* d_bflags */
	0		/* d_cflags */
};

ipf_main_softc_t	ipfmain;

struct controller *ipfilter_info[IPL_LOGSIZE];
static char ipfilter_name[] = "ipfilter";

struct	driver	ipfdriver = {
	NULL,		/* probe  */
	NULL,		/* slave */
	NULL,		/* cattach */
	NULL,		/* dattach */
	0,		/* go */
	0,		/* addr_list */
	0,		/* dev_name */
	0,		/* dev_list */
	ipfilter_name,	/* ctlr_name */
	ipfilter_info,	/* ctlr_list */
	0,		/* Want exclusive use of bdp's flag */
	0,		/* Size of first csr area */
	0,		/* Address space of first csr area */
	0,		/* Size of second csr area */
	0,		/* Address space of second csr area */
	NULL,		/* controller unattach routine */

	NULL,		/* dev_unattach */
};

#define NO_DEV -1
#define MAJOR_INSTANCE 1
#define IPF_NAME "ipf"

int ipfilter_config = FALSE;
int ipfilter_devno = NO_DEV;
int ipfilter_num_units=1;
int ipfilter_dev_number = NO_DEV;
int callback_return_status = ESUCCESS;
int driver_cfg_state;
int ipfilter_is_dynamic;

/* Variables referenced by attributes. 'majnum' must
 * be present to inform cfgmgr that this is a 5.0A
 * driver. 'ipfilter_version' is used to keep track of the version
 * numbers during development. It is printed out after
 * configure to indicate which version is being loaded.
 */
static int majnum = NO_DEV;
static int ipfilter_num_installed = 1;
static u_char ipfilter_desc[100] = "IPFilter firewall";
static u_char ipfilter_unused[300] = "";
static thread_t ipf_timeout = 0;

/* protos for functions used in configuration */
void ipfilter_print_attr_error __P((cfg_attr_t *));
void ipfilter_preconfig_callback __P((int, int, ulong, ulong));
void ipfilter_postconfig_callback __P((int, int, ulong, ulong));


struct firewall_stat {
	u_long	in_control_called;
	u_long	ip_input_mbufs;
	u_long	ip_input_accept;
	u_long	ip_input_reject;
	u_long	ip_input_tooshort;
	u_long	ip_input_toosmall;
	u_long	ip_input_badlen;
	u_long	ip_input_badhlen;
	u_long	ip_input_badvers;
	u_long	ip_input_badsum;
	u_long	ip_output_mbufs;
	u_long	ip_output_accept;
	u_long	ip_output_reject;
};
struct firewall_stat ipfilter_stat;
static	int	ipfilter_registered = 0;
static	int	ipftru64_inited = 0;


cfg_subsys_attr_t ipfilter_attributes[] = {

{ "Subsystem_Description",CFG_ATTR_STRTYPE, CFG_OP_CONFIGURE | CFG_OP_QUERY,
				(caddr_t)ipfilter_desc, 2, 300, 0 },
{ "Module_Config_Name",	CFG_ATTR_STRTYPE, CFG_OP_QUERY,
				(caddr_t)"ipfilter", 2, CFG_ATTR_NAME_SZ, 0 },
{ "majnum",		CFG_ATTR_INTTYPE, CFG_OP_QUERY,
				(caddr_t)&majnum, 0, 512, 0 },
{ "Num_Installed",	CFG_ATTR_INTTYPE, CFG_OP_QUERY,
				(caddr_t)&ipfilter_num_installed, 1, 1, 1 },
{ "Num_Units",		CFG_ATTR_INTTYPE, CFG_OP_QUERY | CFG_OP_CONFIGURE,
				(caddr_t)&ipfilter_num_units,
				1, IPL_LOGSIZE, 0 },
{ "Module_Type",	CFG_ATTR_STRTYPE, CFG_OP_QUERY | CFG_OP_CONFIGURE,
				(caddr_t)ipfilter_unused,
				2, CFG_ATTR_NAME_SZ, 0 },
{ "Device_Major_Req",	CFG_ATTR_STRTYPE, CFG_OP_QUERY | CFG_OP_CONFIGURE,
				(caddr_t)ipfilter_unused,
				0, sizeof(ipfilter_unused), 0 },
{ "Device_Char_Major",	CFG_ATTR_STRTYPE, CFG_OP_QUERY | CFG_OP_CONFIGURE,
				(caddr_t)ipfilter_unused,
				0, sizeof(ipfilter_unused), 0 },
{ "Device_Char_Files",	CFG_ATTR_STRTYPE, CFG_OP_QUERY | CFG_OP_CONFIGURE,
				(caddr_t)ipfilter_unused,
				0, sizeof(ipfilter_unused), 0 },
{ "Device_Char_Minor",	CFG_ATTR_STRTYPE, CFG_OP_QUERY | CFG_OP_CONFIGURE,
				(caddr_t)ipfilter_unused, 0,
				sizeof(ipfilter_unused), 0 },
{ "version",		CFG_ATTR_STRTYPE, CFG_OP_QUERY,
				(caddr_t)ipfilter_version, 0, 5, 0 },
{ "ipf_chksrc",		CFG_ATTR_INTTYPE, CFG_OP_QUERY |
				CFG_OP_CONFIGURE | CFG_OP_RECONFIGURE,
				(caddr_t)&ipfmain.ipf_chksrc,
				0, 1, 0 },
{ "ipf_minttl",		CFG_ATTR_INTTYPE, CFG_OP_QUERY |
				CFG_OP_CONFIGURE | CFG_OP_RECONFIGURE,
				(caddr_t)&ipfmain.ipf_minttl,
				0, 255, 0 },
{ "ipf_pass",		CFG_ATTR_INTTYPE, CFG_OP_QUERY |
				CFG_OP_CONFIGURE | CFG_OP_RECONFIGURE,
				(caddr_t)&ipfmain.ipf_minttl,
				0, 0xffffffff, 0 },
{ "ipf_flags",		CFG_ATTR_INTTYPE, CFG_OP_QUERY |
				CFG_OP_CONFIGURE | CFG_OP_RECONFIGURE,
				(caddr_t)&ipfmain.ipf_flags,
				0, 0xffffffff, 0 },
{ "ipf_active",		CFG_ATTR_INTTYPE, CFG_OP_QUERY,
				(caddr_t)&ipfmain.ipf_minttl,
				0, 1, 0 },
{ "ipf_running",	CFG_ATTR_INTTYPE, CFG_OP_QUERY,
				(caddr_t)&ipfmain.ipf_minttl,
				0, 1, 0 },
{ "ipf_control_forwarding",
			CFG_ATTR_INTTYPE, CFG_OP_QUERY |
				CFG_OP_CONFIGURE | CFG_OP_RECONFIGURE,
				(caddr_t) &ipfmain.ipf_control_forwarding,
				0, 1, 0 },
{ "ipf_update_ipid",	CFG_ATTR_INTTYPE, CFG_OP_QUERY |
				CFG_OP_CONFIGURE | CFG_OP_RECONFIGURE,
				(caddr_t) &ipfmain.ipf_update_ipid,
				0, 1, 0 },
{ "",			0, 0, 0, 0, 0, 0 }
};


/*---------------------------------------------------------
 * Configure interface
 * 1 - Handle attribute verification.
 * 2 - Handle the differences between static and dynamic
 *     configuration.
 * Returns: EBUSY,EINVAL,ENOTSUP,ESRCH for fail,
 *          ESUCCESS for success
 *--------------------------------------------------------*/
cfg_status_t ipfilter_configure(op, indata, indata_size, outdata, outdata_size)
	cfg_op_t op;
	caddr_t indata;
	ulong indata_size;
	caddr_t outdata;
	ulong outdata_size;
{
	int ipfilter_cfg_state;
	cfg_attr_t *attr_ptr;
	int status, i;

#ifdef	IPFDEBUG
	printf("ipfilter_configure(%x,%x,%d,%x,%d)\n",
	       op, indata, indata_size, outdata, outdata_size);
#endif

	status = ESUCCESS;

	if (cfgmgr_get_state("ipfilter", &ipfilter_cfg_state) != ESUCCESS) {
		printf("ipfilter cfgmgr_get_state failed\n");
		return EINVAL;
	}

	switch (op)
	{
	case CFG_OP_CONFIGURE :
#ifdef IPFDEBUG
		printf("ipfilter_config=%d\n",ipfilter_config);
		printf("driver_cfg_state=%d\n",driver_cfg_state);
#endif
		if (ipfilter_config == TRUE) {
			/*
			 * We have already been configured
			 */
			return EINVAL;
		}
		/*
		 * cfgmgr takes care of error checking, but we have to
		 * investigate what happened and report to user.
		 */
		attr_ptr = (cfg_attr_t *)indata;
		for(i = 0; i < indata_size; i++) {
			switch(attr_ptr->type)
			{
			case CFG_ATTR_STRTYPE:
				break;
			default:
				switch(attr_ptr->status)
				{
				case CFG_FRAME_SUCCESS:
					break;
				default:
					ipfilter_print_attr_error(attr_ptr);
					break;
				}
			}
			attr_ptr++;
		}

		if (cfgmgr_get_state((char *)ipfilter_name,
				     &driver_cfg_state) != ESUCCESS) {
			printf("ipfilter_conf:Error determining state of ipf subsystem\n");
			/*
			 * Error determining state of the "ipf" subs..
			 */
			return(EINVAL);
		}

		if (driver_cfg_state == SUBSYSTEM_STATICALLY_CONFIGURED)  {
			callback_return_status = ESUCCESS;
			ipfilter_is_dynamic = 0;
			status = register_callback(ipfilter_configure_callback,
						   CFG_PT_OLD_CONF_ALL,
						   CFG_ORD_DONTCARE, 0L);
			if (status != ESUCCESS)
				break;
			status = register_callback(ipfilter_preconfig_callback,
						   CFG_PT_PRECONFIG,
						   CFG_ORD_NOMINAL, 0L);
			if (status != ESUCCESS)
				break;
			status = register_callback(ipfilter_postconfig_callback,
						   CFG_PT_POSTCONFIG,
						   CFG_ORD_NOMINAL, 0L);
		} else {
			/*
			 * ipf.mod is getting loaded dynamically.
			 */
			ipfilter_is_dynamic = 1;
			status = ipfilter_preconfig();
			if (status != ESUCCESS) {
				printf("ipfilter_conf:preconfig failed\n");
				return (status);
			}

			status = ipfilter_postconfig();
			if (status != ESUCCESS) {
				printf("ipfilter_conf:postconfig failed\n");
				return (status);
			}
			status = ipfilter_attach();
#ifdef IPFDEBUG
			printf("ipfilter_attach=%d\n",status);
#endif
		}
		break;

	case CFG_OP_QUERY :
		break;

	case CFG_OP_RECONFIGURE :
		break;

	case CFG_OP_UNCONFIGURE :
		if (ipfilter_is_dynamic) {

			status = devsw_del(ipfilter_name, MAJOR_INSTANCE);
			if (status == NO_DEV) {
				printf("ipfilter_configure: call to devsw_del failed\n");
				return ESRCH;
			}

			ipfilter_is_dynamic = 0;
			ipfilter_config = FALSE;

			printf("ipfilter_configure: dynamic module unconfigured\n");
		} else {
			/* static configuration */
			printf("ipfilter_configure: Can't unconfigure a static mod\n");
			return ESUCCESS;
		}
		if (ipfilter_cfg_state & SUBSYSTEM_STATICALLY_CONFIGURED) {
			status = EINVAL;
			break;
		}
		status = ipfilter_detach();
		break;

	default :
		status = ENOTSUP;
		break;
	}

#ifdef IPFDEBUG
	printf("ipfilter_configure(%d)=%d\n",op,status);
#endif
	return status;
}


void ipfilter_configure_callback(conf, order, arg, evarg)
	int conf, order, arg, evarg;
{
	int status;

#ifdef	IPFDEBUG
	printf("ipfilter_configure_callback(%x,%x,%x,%x)\n",
	       conf, order, arg, evarg);
#endif

	status = ipfilter_attach();

	if (status != ESUCCESS) {
		cfgmgr_set_status("ipfilter");
	}
}


/*
 * This hook intercepts mbufs directly off the IP input
 * queue(s) and gives them to the routine at (*ip_input_hook)(m).
 * The hook must either free the mbuf, or enqueue it back to
 * an IP input queue by calling ip_continue_input(m). It is
 * assumed that the hook returns to the caller in a timely
 * manner, enqueuing the mbuf on its own list for further
 * processing at a later time (if necesary).
*/
void ipfilter_ip_input(m)
	struct mbuf *m;
{
	struct mbuf *m0;
	struct ip *ip;
	int hlen, len;

#ifdef	IPFDEBUG
	printf("ipfilter_ip_input(%x)\n", m);
#endif
	READ_ENTER(&ipf_tru64);
	if (ipfilter_registered < 1 || ipftru64_inited == 0) {
		RWLOCK_EXIT(&ipf_tru64);
		ip_continue_input(m);
		return;
	}

	ipfilter_stat.ip_input_mbufs++;

	ip = mtod(m, struct ip *);

	/*
	 * Because this gets packets directly off the IP input queue, we need
	 * to do all of the input packet checking.  This is IPv4 only.
	 */
	if (m->m_len < sizeof(struct ip) &&
	    (m = m_pullup(m, sizeof (struct ip))) == 0) {
		ipfilter_stat.ip_input_toosmall++;
		RWLOCK_EXIT(&ipf_tru64);
		return;
	}

	if (IP_V(ip) != IPVERSION) {
		ipfilter_stat.ip_input_badvers++;
		goto bad;
	}

	hlen = IP_HL(ip) << 2;
	if (hlen < sizeof(*ip)) {
		ipfilter_stat.ip_input_badhlen++;
		goto bad;
	}

	if (hlen > m->m_len) {
		if ((m = m_pullup(m, hlen)) == 0) {
			RWLOCK_EXIT(&ipf_tru64);
			return;
		}
		ip = mtod(m, struct ip *);
	}

	/*
	 * If header checksum verification fails, do no more.
	 */
	if (in_cksum(m, hlen) != 0) {
		ipfilter_stat.ip_input_badsum++;
		goto bad;
	}

	/*
	 * Convert fields to host representation.
	 */
	len = ntohs(ip->ip_len);
	if (len < hlen) {
		ipfilter_stat.ip_input_badlen++;
		goto bad;
	}
	if (m->m_pkthdr.len < len) {
		ipfilter_stat.ip_input_tooshort++;
		goto bad;
	}
	if (m->m_pkthdr.len > len) {
		if (m->m_len == m->m_pkthdr.len) {
			m->m_len = len;
			m->m_pkthdr.len = len;
		} else
			m_adj(m, len - m->m_pkthdr.len);
	}

	if (ipf_check(&ipfmain, ip, hlen, m->m_pkthdr.rcvif, 0, &m) == 0) {
		if (m != NULL) {
			m->m_flags &= ~M_PROTOCOL_SUM|M_NOCHECKSUM|M_CHECKSUM;

			ipfilter_stat.ip_input_accept++;
			RWLOCK_EXIT(&ipf_tru64);
			ip_continue_input(m);
			return;
		}
	}
bad:
	RWLOCK_EXIT(&ipf_tru64);
	if (m != NULL)
		m_freem(m);
	return;
}


/*
 * This hook intercepts mbufs directly from the IP output
 * routine and gives them to the routine at
 * (*ip_output_hook)(ifp, m, in_ro, flags, imo).  The
 * hook must either free the mbuf, or set an mbuf flag
 * (m->m_flags |= M_OUTPUT_PROCESSING_DONE) and call
 * ip_output() again to transmit it.  It is assumed that
 * the hook returns to the caller in a timely manner, enqueuing
 * the mbuf on its own list for further processing at a later
 * time (if necesary).
 */
int ipfilter_ip_output(ifp, m, in_ro, flags, imo)
	struct ifnet *ifp;
	struct mbuf *m;
	struct in_route *in_ro;
	int flags;
	struct ip_moptions *imo;
{
	struct ip *ip;
	int hlen;

#ifdef	IPFDEBUG
	printf("ipfilter_ip_output(%x,%x,%x,%x,%x)\n",
	       ifp, m, in_ro, flags, imo);
#endif
	READ_ENTER(&ipf_tru64);
	if (ipfilter_registered < 1 || ipftru64_inited == 0) {
		RWLOCK_EXIT(&ipf_tru64);
		return (ip_output(m, NULL, in_ro, flags, imo));
	}

	ipfilter_stat.ip_output_mbufs++;

	ip = mtod(m, struct ip *);
	hlen = IP_HL(ip);
	hlen <<= 2;

	if (ipf_check(&ipfmain, ip, hlen, ifp, 1, &m) == 0) {
		if (m != NULL) {
			m->m_flags |= M_OUTPUT_PROCESSING_DONE;
			RWLOCK_EXIT(&ipf_tru64);
			return (ip_output(m, NULL, in_ro, flags, imo));
		}
	}
	RWLOCK_EXIT(&ipf_tru64);
	return 0;
}


/*
 * This is a passive routine.  It does not implement any ioctl's, just merely
 * "snoops" ioctls being sent so that we can be aware of things like address
 * changes on the interfaces.
 */
void ipfilter_in_control(so, cmd, data, ifp)
	struct socket *so;
	u_int cmd;
	caddr_t *data;
	struct ifnet *ifp;
{
#ifdef	IPFDEBUG
	printf("ipfilter_in_control(%x,%x,%x,%x)\n", so, cmd, data, ifp);
#endif

	ipfilter_stat.in_control_called++;

	READ_ENTER(&ipf_tru64);
	if (ipfilter_registered < 1 || ipftru64_inited == 0) {
		RWLOCK_EXIT(&ipf_tru64);
		return;
	}

	if (cmd == (ioctlcmd_t)SIOCAIFADDR)
		ipf_sync(&ipfmain, NULL);

	RWLOCK_EXIT(&ipf_tru64);
}


/* "protocol" ifnet structure for accessing ipf statistics */
static struct {
	struct ifnet	ifnet;
	char		pad[sizeof(struct ether_driver) - sizeof(ifnet)];
} ipfilter_if;


/*
 * Routine to attach ipf module
 */
int
ipfilter_attach(void)
{
	struct firewall ipf;
	int status;

	bzero((char *)&ipfmain, sizeof(ipfmain));

#ifdef	IPFDEBUG
	printf("ipfilter_attach(void)\n");
#endif

	if ((status = ipf_load_all()) != 0) {
#ifdef IPFDEBUG
		printf("ipf_load_all() == %d\n", status);
#endif
		return EIO;
	}

        if (ipf_create_all(&ipfmain) == NULL) {
		SPL_X(s);
#ifdef IPFDEBUG
		printf("ipf_create_all() == %d\n", status);
#endif
		return EIO;
	}

	status = ipfattach(&ipfmain);
#ifdef	IPFDEBUG
	printf("ipfattach() = %d\n", status);
#endif
	if (status != ESUCCESS) {
		(void) ipfdetach(&ipfmain);
		return status;
	}

	ipfilter_registered = 1;

	/*
	 * Initialize the struct, else the kernel might interpret
	 * a field as being valid when it's not, which would probably
	 * be fatal.
	 */
	bzero(&ipf, sizeof(struct firewall));

	/*
	 * Tell which version of the ipf structure we are passing-in.
	 * The only currently supported version is #1.
	 */
	ipf.version = FIREWALL_VERSION1;

	/*
	 * Tell which protocol family we wish to register.
	 * The only currently supported family is PF_INET.
	 */
	ipf.pf = PF_INET;

	/*
	 * Tell the kernel to disable the RPC/NFS "fastpath" code and
	 * send all NFS output packets through the "slow" IP output code.
	 */
	ipf.flags = FW_DISABLE_RPC_FASTPATH;

	/*
	 * Fill-in the hooks we wish to register.  At a minumum there
	 * must be an input and output hook.
	 */
	ipf.in_control_hook = ipfilter_in_control;
	ipf.fw_ip_input_hook = ipfilter_ip_input;
	ipf.fw_ip_output_hook = ipfilter_ip_output;

	/* make the call */
	status = register_firewall(&ipf);

	/*
	 * Additionally, if we wish to attach a protocol statistics structure,
	 * do it now.  We will only attach to counter ioctls on success.
	 */
	if (status == ESUCCESS) {
		char *defpass;

		ipfilter_registered = 2;
		ipfilter_ifattach();
		ipfmain.ipf_running = 1;

		/*
		 * Start timeout thread
		 */
		ipf_timeout = kernel_thread_w_arg(first_task, ipfilter_timer,
						  &ipfmain);
		timeout(ipfilter_clock, &ipfmain,
			(hz / IPF_HZ_DIVIDE) * IPF_HZ_MULT);

		if (FR_ISPASS(ipfmain.ipf_pass))
			defpass = "pass";
		else if (FR_ISBLOCK(ipfmain.ipf_pass))
			defpass = "block";
		else
			defpass = "no-match -> block";

		printf("%s initialized.  Default = %s all, Logging = %s%s\n",
			ipfilter_version, defpass,
#ifdef  IPFILTER_LOG
			"enabled",
#else
			"disabled",
#endif
#ifdef  IPFILTER_COMPILED
			" (COMPILED)"
#else
			""
#endif
			);
	}

	return (status);
}


/*
 * Routine to detach ipf module
 */
int
ipfilter_detach(void)
{
	int status;
	struct firewall ipf;

#ifdef	IPFDEBUG
	printf("ipfilter_detach(void)\n");
#endif

	WRITE_ENTER(&ipf_tru64);
	/*
	 * Initialize the struct, else the kernel might interpret
	 * a field as being valid when it's not, which would probably
	 * be fatal.
	 */
	bzero(&ipf, sizeof(struct firewall));

	/*
	 * If we had attached a protocol statistics structure, detach it now.
	 * We do this before un-registering our hooks.
	 */
	ipfilter_ifdetach();

	/*
	 * Tell which version of the ipf structure we are passing-in.
	 * The only currently supported version is #1.
	 */
	ipf.version = FIREWALL_VERSION1;

	/*
	 * Tell which protocol family we wish to register.
	 * The only currently supported family is PF_INET.
	 */
	ipf.pf = PF_INET;

	/*
	 * Tell the kernel to re-enable the RPC/NFS "fastpath" code and stop
	 * sending all NFS output packets through the "slow" IP output code.
	 */
	ipf.flags = FW_ENABLE_RPC_FASTPATH;

	/*
	 * We don't need to fill-in the hook pointers as they
	 * are not used in the unregister_ipf() routine.
	 */

	/* make the call */
	status = ESUCCESS;
	if (ipfilter_registered > 1) {
		status = unregister_firewall(&ipf);
		ipfilter_registered = 1;
	}

	if ((status == ESUCCESS) && (ipfilter_registered > 0)) {
		status = ipfdetach(&ipfmain);
#ifdef	IPFDEBUG
		printf("ipfdetach() = %d\n", status);
#endif
		ipfilter_registered = 0;
	}

	if (status == ESUCCESS) {
		ipfmain.ipf_running = 0;
#if 0
		if (ipf_timeout != 0) {
			thread_terminate(ipf_timeout);
			ipf_timeout = 0;
		}
#else
		/*
		 * Deschedule the timeout, kill the thread that is wiating on
		 * it and then wait one second for that thread to die.
		 */
		untimeout(ipfilter_clock, &ipfmain);
		thread_wakeup_one((vm_offset_t)&ipf_timeout);

		while (ipf_timeout != 0) {
			assert_wait_mesg((vm_offset_t)&ipf_timeout,
					 TRUE, "ipftimeout");
			thread_set_timeout(hz);
			thread_block();
		}
#endif
		printf("%s unloaded\n", ipfilter_version);

		ipf_destroy_all(&ipfmain);

		ipf_unload_all();
	} else {
		RWLOCK_EXIT(&ipf_tru64);
	}

	return (status);
}


/*
 * Routine to attach protocol statistics structure
 */
void ipfilter_ifattach(void)
{
	struct ifnet *ifp = &(ipfilter_if.ifnet);

#ifdef	IPFDEBUG
	printf("ipfilter_ifattach(void)\n");
#endif

	ifp->if_name = "ipf";
	ifp->if_version = "Network Packet Filter";
	ifp->if_mtu = IP_MAXPACKET;
	ifp->if_flags = IFF_UP;
	ifp->if_ioctl = ipfilter_ifioctl;
	ifp->if_output = ipfilter_ifoutput;
	ifp->if_type = IFT_OTHER;
	ifp->if_hdrlen = 8;
	ifp->if_affinity = NETALLCPU;
	ifp->if_addrlen = 0;

	if_proto_attach(ifp);
}


/*
 * Routine to detach protocol statistics structure
 */
void ipfilter_ifdetach(void)
{
	struct ifnet *ifp = &(ipfilter_if.ifnet);

#ifdef	IPFDEBUG
	printf("ipfilter_ifdetach(void)\n");
#endif

	if_proto_detach(ifp);
}


/*
 * Routine to handle protocol ioctls
 */
int ipfilter_ifioctl(ifp, cmd, data)
	register struct ifnet *ifp;
	unsigned int cmd;
	caddr_t data;
{
	return (EOPNOTSUPP);
}

/* Stub output routine.  Should never be called, but just in case... */
#if TRU64 >= 1885
int ipfilter_ifoutput(ifp, m0, dst, rt, cp)
	char *cp;
#else
int ipfilter_ifoutput(ifp, m0, dst, rt)
#endif
	struct ifnet *ifp;
	struct mbuf *m0;
	struct sockaddr *dst;
	struct rtentry *rt;
{
	return (EOPNOTSUPP);
}


/* called from ipfilter_configure(CFG_OP_CONFIGURE,...) */
void ipfilter_print_attr_error(cfg_attr_t *attr_ptr)
{
	printf("%s:", attr_ptr->name);

	switch(attr_ptr->status)
	{
	case CFG_ATTR_EEXISTS:
		printf("Attribute does not exist\n");
		break;
	case CFG_ATTR_EOP:
		printf("Attribute does not support operation\n");
		break;
	case CFG_ATTR_ESUBSYS:
		printf("Subsystem Failure\n");
		break;
	case CFG_ATTR_ESMALL:
		printf("Attribute size/value too small\n");
		break;
	case CFG_ATTR_ELARGE:
		printf("Attribute size/value too large\n");
		break;
	case CFG_ATTR_ETYPE:
		printf("Attribute invalid type\n");
		break;
	case CFG_ATTR_EINDEX:
		printf("Attribute invalid index\n");
		break;
	case CFG_ATTR_EMEM:
		printf("Attribute memory allocation error\n");
		break;
	default:
		printf("**Unknown attribute: ");
		printf("%x\n", attr_ptr->status);
	}
}

/*---------------------------------------------------------
* pre-configuration
* Returns: ENOMEM for fail.
*          ESUCCESS for success.
*--------------------------------------------------------*/
int ipfilter_preconfig()
{
	int status;

	return ESUCCESS;
}

void
ipfilter_preconfig_callback(int point, int order, ulong argument, ulong event_arg)
{
	int status;
	struct hwconfig *hwc;
	hwc = create_hwconfig_struct();

	if (hwc == NULL)
		return;

	/*
	 * Add ourselves to the switch table.
	 */
	majnum = devsw_add(ipfilter_name, MAJOR_INSTANCE,
			   NO_DEV, &ipfilter_devsw_entry);
	if (majnum == NO_DEV) {
		printf("devsw_add_failed\n");
		(void)cfgmgr_set_status(ipfilter_name);
		callback_return_status = ENODEV;
		return;
	}
	printf("devsw_add_majnum=%d\n", majnum);

	hwc->type = HWCONFIG_CONTROLLER_ALL | HWCONFIG_CONFIGURE_REQUEST;

	hwc->parent_bus = HWCONFIG_ALL;
	hwc->parent_bus_instance = DRIVER_WILDNUM;
	hwc->driver_name = ipfilter_name;
	hwc->driver_struct = &ipfdriver;
	hwc->instance_info = -1;

	status = driver_framework(hwc);
	free_hwconfig_struct(hwc);

	if (status != ESUCCESS) {
		printf("Cannot configure driver %s-- status = %d\n",
		       ipfilter_name, status);
		return;
	}

}

/*---------------------------------------------------------
* post configure
* Return: ENODEV on fail.
*         ESUCCESS on success.
*--------------------------------------------------------*/
int ipfilter_postconfig()
{

	majnum = devsw_add(ipfilter_name,MAJOR_INSTANCE, majnum,
			   &ipfilter_devsw_entry);

	if (majnum == ENODEV) {
		printf("ipfilter_postconfig: call to devsw_add failed\n");
		return ENODEV;
	}

	printf("ipfilter_postconfig:major # = %d\n",majnum);

	ipfilter_devno = majnum;
	ipfilter_config = TRUE;

	printf("%s configured\n", ipfilter_version);

	return ESUCCESS;
}

void
ipfilter_postconfig_callback(int point, int order, ulong argument,
			     ulong event_arg)
{
	int status;

	if (callback_return_status != ESUCCESS)
			return;

	status  = ipfilter_postconfig();
	if (status != ESUCCESS) {
		cfgmgr_set_status(ipfilter_name);
		callback_return_status = status;
	}
}


/*---------------------------------------------------------
 * Open
 * Return: ENXIO on fail.
 *         ESUCCESS on success.
 *--------------------------------------------------------*/
int ipfilteropen(dev_t dev, int flag, int format)
{
	int unit;
	int error;

	unit = minor(dev);

	if ((IPL_LOGMAX < unit) || (unit < 0)) {
		error = ENXIO;
	} else {
		switch (unit)
		{
		case IPL_LOGIPF :
		case IPL_LOGNAT :
		case IPL_LOGSTATE :
		case IPL_LOGAUTH :
		case IPL_LOGLOOKUP :
		case IPL_LOGSYNC :
#ifdef IPFILTER_SCAN
		case IPL_LOGSCAN :
#endif
			error = ESUCCESS;
			break;
		default :
			error = ENXIO;
			break;
		}
	}

	return error;
}

/*---------------------------------------------------------
* Close
* Return: ESUCCESS on success.
*         ENXIO on fail.
*--------------------------------------------------------*/
int ipfilterclose(dev_t dev, int flag, int format)
{
	int unit;

#ifdef  IPFDEBUG
	printf("ipfilter_close(%x,%x,%x)\n", dev, flag, format);
#endif

	unit = minor(dev);

	if ((IPL_LOGMAX < unit) || (unit < 0))
		unit = ENXIO;
	else
		unit = ESUCCESS;

	return unit;
}


/*---------------------------------------------------------
* Read -
* return: ESUCCESS on success.
*         fail: An error number from errno.h
*--------------------------------------------------------*/
int ipfilterread(dev_t dev, struct uio *uio, int flag)
{
#ifdef	IPFILTER_LOG
	int unit, status;

	unit = minor(dev);

# ifdef  IPFDEBUG
	printf("ipfread(%x,%lx,%x)\n", dev, uio, flag);
# endif
	status = ipflog_read(&ipfmain, unit, uio);

	return status;
#else
	return ENXIO;
#endif
}


/*---------------------------------------------------------
* Write -
* return: ESUCCESS on success.
*         fail: An error number from errno.h
*--------------------------------------------------------*/
int
ipfilterwrite(dev_t dev, struct uio *uio)
{
	int unit;

# ifdef  IPFDEBUG
	printf("ipfilter_write(%x,%lx)\n", dev, uio);
# endif
        if (getminor(dev) != IPL_LOGSYNC)
                return ENXIO;
        return ipfsync_write(&ipfmain, uio);

}


/*---------------------------------------------------------
* IOCTL
* Return: ESUCCESS on success.
*         fail: An error number from errno.h
*--------------------------------------------------------*/
int ipfilterioctl(dev_t dev, unsigned int cmd, caddr_t data, int flag)
{
	int err;

#ifdef	IPFDEBUG
	printf("ipfilterioctl(%d(%d),%x,%lx,%x)\n", dev,
		getminor(dev), cmd, data, flag);
#endif
	READ_ENTER(&ipf_tru64);
	if (ipfilter_registered < 1 || ipftru64_inited == 0) {
		RWLOCK_EXIT(&ipf_tru64);
#ifdef IPFDEBUG
		printf("ipfilter_registered %d ipftru64_inited %d\n",
			ipfilter_registered, ipftru64_inited);
#endif
		return EIO;
	}
	err = ipfioctl(dev, cmd, data, flag);
	RWLOCK_EXIT(&ipf_tru64);
	return err;
}


void ipfilter_clock(void *arg)
{
	thread_wakeup_one((vm_offset_t)&ipf_timeout);
	if (ipfmain.ipf_running != 0) {
		timeout(ipfilter_clock, &ipfmain,
			(hz / IPF_HZ_DIVIDE) * IPF_HZ_MULT);
	}
}


void ipfilter_timer()
{
#if 0
	lock_data_t ipfdelaylock;

	lock_init(&ipfdelaylock, TRUE);
#else
	simple_lock_data_t ipfdelaylock;

	simple_lock_init(&ipfdelaylock);
	simple_lock(&ipfdelaylock);
#endif

	while (1) {
#if 0
		lock_write(&ipfdelaylock);
		assert_wait_mesg((vm_offset_t)&ipfdelaylock, TRUE, "ipftimer");
		lock_done(&ipfdelaylock);

		thread_set_timeout((hz / IPF_HZ_DIVIDE) * IPF_HZ_MULT);

		thread_block();
#else
		if (ipfmain.ipf_running == 0)
			break;
		mpsleep((vm_offset_t)&ipf_timeout, PCATCH,
			"ipftimer", 0, &ipfdelaylock,
			MS_LOCK_SIMPLE | MS_LOCK_ON_ERROR);
#endif

		if (ipfmain.ipf_running == 0)
			break;
		simple_unlock(&ipfdelaylock);
		ipf_timer_func(&ipfmain);
		simple_lock(&ipfdelaylock);
	}

	simple_unlock(&ipfdelaylock);
	simple_lock_terminate(&ipfdelaylock);

	if (ipf_timeout != 0) {
		thread_terminate(ipf_timeout);
		ipf_timeout = 0;
	}
	thread_halt_self();
}


/*
 * routines below for saving IP headers to buffer
 */
static int ipfopen(dev, flags)
	dev_t dev;
	int flags;
{
	u_int min = minor(dev);

	if (IPL_LOGMAX < min)
		min = ENXIO;
	else
		min = 0;
	return min;
}


static int ipfclose(dev, flags)
	dev_t dev;
	int flags;
{
	u_int	min = minor(dev);

	if (IPL_LOGMAX < min)
		min = ENXIO;
	else
		min = 0;
	return min;
}

/*
 * ipfread/ipllog
 * both of these must operate with at least splnet() lest they be
 * called during packet processing and cause an inconsistancy to appear in
 * the filter lists.
 */
static int ipfread(dev, uio)
	dev_t dev;
	register struct uio *uio;
{

	if (ipfmain.ipf_running < 1)
		return EIO;

#ifdef IPFILTER_LOG
	return ipflog_read(minor(dev), uio);
#else
	return ENXIO;
#endif
}
