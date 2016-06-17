
/*
 *	New style setup code for the network devices
 */
 
#include <linux/config.h>
#include <linux/netdevice.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/netlink.h>

extern int slip_init_ctrl_dev(void);
extern int x25_asy_init_ctrl_dev(void);
  
extern int dmascc_init(void);

extern int awc4500_pci_probe(void);
extern int awc4500_isa_probe(void);
extern int awc4500_pnp_probe(void);
extern int awc4500_365_probe(void);
extern int arcnet_init(void); 
extern int scc_enet_init(void); 
extern int fec_enet_init(void); 
extern int dlci_setup(void); 
extern int sdla_setup(void); 
extern int sdla_c_setup(void); 
extern int comx_init(void);
extern int lmc_setup(void);

extern int madgemc_probe(void);
extern int uml_net_probe(void);

/* Pad device name to IFNAMSIZ=16. F.e. __PAD6 is string of 9 zeros. */
#define __PAD6 "\0\0\0\0\0\0\0\0\0"
#define __PAD5 __PAD6 "\0"
#define __PAD4 __PAD5 "\0"
#define __PAD3 __PAD4 "\0"
#define __PAD2 __PAD3 "\0"


/*
 *	Devices in this list must do new style probing. That is they must
 *	allocate their own device objects and do their own bus scans.
 */

struct net_probe
{
	int (*probe)(void);
	int status;	/* non-zero if autoprobe has failed */
};
 
static struct net_probe pci_probes[] __initdata = {
	/*
	 *	Early setup devices
	 */

#if defined(CONFIG_DMASCC)
	{dmascc_init, 0},
#endif	
#if defined(CONFIG_DLCI)
	{dlci_setup, 0},
#endif
#if defined(CONFIG_SDLA)
	{sdla_c_setup, 0},
#endif
#if defined(CONFIG_ARCNET)
	{arcnet_init, 0},
#endif
#if defined(CONFIG_SCC_ENET)
        {scc_enet_init, 0},
#endif
#if defined(CONFIG_FEC_ENET)
        {fec_enet_init, 0},
#endif
#if defined(CONFIG_COMX)
	{comx_init, 0},
#endif
	 
#if defined(CONFIG_LANMEDIA)
	{lmc_setup, 0},
#endif
	 
/*
*
*	Wireless non-HAM
*
*/
#ifdef CONFIG_AIRONET4500_NONCS

#ifdef CONFIG_AIRONET4500_PCI
	{awc4500_pci_probe,0},
#endif

#ifdef CONFIG_AIRONET4500_PNP
	{awc4500_pnp_probe,0},
#endif

#endif

/*
 *	Token Ring Drivers
 */  
#ifdef CONFIG_MADGEMC
	{madgemc_probe, 0},
#endif
#ifdef CONFIG_UML_NET
	{uml_net_probe, 0},
#endif
 
	{NULL, 0},
};


/*
 *	Run the updated device probes. These do not need a device passed
 *	into them.
 */
 
static void __init network_probe(void)
{
	struct net_probe *p = pci_probes;

	while (p->probe != NULL)
	{
		p->status = p->probe();
		p++;
	}
}


/*
 *	Initialise the line discipline drivers
 */
 
static void __init network_ldisc_init(void)
{
#if defined(CONFIG_SLIP)
	slip_init_ctrl_dev();
#endif
#if defined(CONFIG_X25_ASY)
	x25_asy_init_ctrl_dev();
#endif
}


static void __init special_device_init(void)
{
#ifdef CONFIG_NET_SB1000
	{
		extern int sb1000_probe(struct net_device *dev);
		static struct net_device sb1000_dev = 
		{
			"cm0" __PAD3, 0x0, 0x0, 0x0, 0x0, 0, 0, 0, 0, 0, NULL, sb1000_probe 
		};
		register_netdev(&sb1000_dev);
	}
#endif
}

/*
 *	Initialise network devices
 */
 
void __init net_device_init(void)
{
	/* Devices supporting the new probing API */
	network_probe();
	/* Line disciplines */
	network_ldisc_init();
	/* Special devices */
	special_device_init();
	/* That kicks off the legacy init functions */
}
