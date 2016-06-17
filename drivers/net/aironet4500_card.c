/*
 *	 Aironet 4500 PCI-ISA-i365 driver
 *
 *		Elmer Joandi, Januar 1999
 *	Copyright GPL
 *	
 *
 *	Revision 0.1 ,started  30.12.1998
 *
 *	Revision 0.2, Feb 27, 2000
 *		Jeff Garzik - softnet, cleanups
 *
 */
#ifdef MODULE
static const char *awc_version =
"aironet4500_cards.c v0.2  Feb 27, 2000  Elmer Joandi, elmer@ylenurme.ee.\n";
#endif

#include <linux/version.h>
#include <linux/config.h>
#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/in.h>
#include <asm/io.h>
#include <asm/system.h>
#include <asm/bitops.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/if_arp.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/init.h>

#include "aironet4500.h"

#define PCI_VENDOR_ID_AIRONET 	0x14b9
#define PCI_DEVICE_AIRONET_4800_1 0x1
#define PCI_DEVICE_AIRONET_4800 0x4500
#define PCI_DEVICE_AIRONET_4500 0x4800
#define AIRONET4X00_IO_SIZE 	0x40
#define AIRONET4X00_CIS_SIZE	0x300
#define AIRONET4X00_MEM_SIZE	0x300

#define AIRONET4500_PCI 	1
#define AIRONET4500_PNP		2
#define AIRONET4500_ISA		3
#define AIRONET4500_365		4


#ifdef CONFIG_AIRONET4500_PCI

#include <linux/pci.h>

static struct pci_device_id aironet4500_card_pci_tbl[] __devinitdata = {
	{ PCI_VENDOR_ID_AIRONET, PCI_DEVICE_AIRONET_4800_1, PCI_ANY_ID, PCI_ANY_ID, },
	{ PCI_VENDOR_ID_AIRONET, PCI_DEVICE_AIRONET_4800, PCI_ANY_ID, PCI_ANY_ID, },
	{ PCI_VENDOR_ID_AIRONET, PCI_DEVICE_AIRONET_4500, PCI_ANY_ID, PCI_ANY_ID, },
	{ }			/* Terminating entry */
};
MODULE_DEVICE_TABLE(pci, aironet4500_card_pci_tbl);
MODULE_LICENSE("GPL");


static int reverse_probe;


static int awc_pci_init(struct net_device * dev, struct pci_dev *pdev,
 			int ioaddr, int cis_addr, int mem_addr,u8 pci_irq_line) ;


int awc4500_pci_probe(struct net_device *dev)
{
	int cards_found = 0;
	static int pci_index;	/* Static, for multiple probe calls. */
	u8 pci_irq_line = 0;
//	int p;

	unsigned char awc_pci_dev, awc_pci_bus;

	if (!pci_present()) 
		return -1;

	for (;pci_index < 0xff; pci_index++) {
		u16 vendor, device, pci_command, new_command;
		u32 pci_memaddr;
		u32 pci_ioaddr;
		u32 pci_cisaddr;
		struct pci_dev *pdev;

		if (pcibios_find_class	(PCI_CLASS_NETWORK_OTHER << 8,
			 reverse_probe ? 0xfe - pci_index : pci_index,
				 &awc_pci_bus, &awc_pci_dev) != PCIBIOS_SUCCESSFUL){
				if (reverse_probe){
					continue;
				} else {
					break;
				}
		}
		pdev = pci_find_slot(awc_pci_bus, awc_pci_dev);
		if (!pdev)
			continue;
		if (pci_enable_device(pdev))
			continue;
		vendor = pdev->vendor;
		device = pdev->device;
	        pci_irq_line = pdev->irq;
		pci_memaddr = pci_resource_start (pdev, 0);
                pci_cisaddr = pci_resource_start (pdev, 1);
		pci_ioaddr = pci_resource_start (pdev, 2);

//		printk("\n pci capabilities %x and ptr %x \n",pci_caps,pci_caps_ptr);
		/* Remove I/O space marker in bit 0. */

		if (vendor != PCI_VENDOR_ID_AIRONET)
			continue;
		if (device != PCI_DEVICE_AIRONET_4800_1 && 
				device != PCI_DEVICE_AIRONET_4800 &&
				device != PCI_DEVICE_AIRONET_4500 )
                        continue;

//		if (check_region(pci_ioaddr, AIRONET4X00_IO_SIZE) ||
//			check_region(pci_cisaddr, AIRONET4X00_CIS_SIZE) ||
//			check_region(pci_memaddr, AIRONET4X00_MEM_SIZE)) {
//				printk(KERN_ERR "aironet4X00 mem addrs not available for maping \n");
//				continue;
//		}
		if (!request_region(pci_ioaddr, AIRONET4X00_IO_SIZE, "aironet4x00 ioaddr"))
			continue;
//		request_region(pci_cisaddr, AIRONET4X00_CIS_SIZE, "aironet4x00 cis");
//		request_region(pci_memaddr, AIRONET4X00_MEM_SIZE, "aironet4x00 mem");

		mdelay(10);

		pci_read_config_word(pdev, PCI_COMMAND, &pci_command);
		new_command = pci_command | PCI_COMMAND_SERR;
		if (pci_command != new_command)
			pci_write_config_word(pdev, PCI_COMMAND, new_command);


/*		if (device == PCI_DEVICE_AIRONET_4800)
			pci_write_config_dword(pdev, 0x40, 0x00000000);

		udelay(1000);
*/
		if (device == PCI_DEVICE_AIRONET_4800)
			pci_write_config_dword(pdev, 0x40, 0x40000000);

		if (awc_pci_init(dev, pdev, pci_ioaddr,pci_cisaddr,pci_memaddr,pci_irq_line)){
			printk(KERN_ERR "awc4800 pci init failed \n");
			break;
		}
		dev = 0;
		cards_found++;
	}

	return cards_found ? 0 : -ENODEV;
}


static int awc_pci_init(struct net_device * dev, struct pci_dev *pdev,
 			int ioaddr, int cis_addr, int mem_addr, u8 pci_irq_line) {

	int i, allocd_dev = 0;

	if (!dev) {
		dev = init_etherdev(NULL, 0);	
		if (!dev)
			return -ENOMEM;
		allocd_dev = 1;
	}
	dev->priv = kmalloc(sizeof(struct awc_private),GFP_KERNEL );
        if (!dev->priv) {
                if (allocd_dev) {
                        unregister_netdev(dev);
                        kfree(dev);
                }
                return -ENOMEM;
        }       
	memset(dev->priv,0,sizeof(struct awc_private));
	if (!dev->priv) {
		printk(KERN_CRIT "aironet4x00: could not allocate device private, some unstability may follow\n");
		if (allocd_dev) {
			unregister_netdev(dev);
			kfree(dev);
		}
		return -ENOMEM;
	};

//	ether_setup(dev);

//	dev->tx_queue_len = tx_queue_len;

	dev->hard_start_xmit = 		&awc_start_xmit;
//	dev->set_config = 		&awc_config_misiganes,aga mitte awc_config;
	dev->get_stats = 		&awc_get_stats;
//	dev->set_multicast_list = 	&awc_set_multicast_list;
	dev->change_mtu		=	awc_change_mtu;
	dev->init = &awc_init;
	dev->open = &awc_open;
	dev->stop = &awc_close;
    	dev->base_addr = ioaddr;
    	dev->irq = pci_irq_line;
	dev->tx_timeout = &awc_tx_timeout;
	dev->watchdog_timeo = AWC_TX_TIMEOUT;
	

	i = request_irq(dev->irq,awc_interrupt, SA_SHIRQ | SA_INTERRUPT, dev->name, dev);
	if (i) {
		kfree(dev->priv);
		dev->priv = NULL;
		if (allocd_dev) {
			unregister_netdev(dev);
			kfree(dev);
		}
		return i;
	}

	awc_private_init( dev);
	awc_init(dev);

	i=0;
	while (aironet4500_devices[i] && i < MAX_AWCS-1) i++;
	if (!aironet4500_devices[i]){
		aironet4500_devices[i]=dev;
		((struct awc_private *)
		aironet4500_devices[i]->priv)->card_type = AIRONET4500_PCI;

		if (awc_proc_set_fun)
			awc_proc_set_fun(i);
	}

//	if (register_netdev(dev) != 0) {
//		printk(KERN_NOTICE "awc_cs: register_netdev() failed\n");
//		goto failed;
//	}

	return 0; 
//  failed:
//  	return -1;

}

#ifdef MODULE
static void awc_pci_release(void) {

//	long flags;
	int i=0;

	DEBUG(0, "awc_detach \n");

	i=0;
	while ( i < MAX_AWCS) {
		if (!aironet4500_devices[i])
                        {i++; continue;};
		if (((struct awc_private *)aironet4500_devices[i]->priv)->card_type != AIRONET4500_PCI)
		                  {i++;      continue;}

		if (awc_proc_unset_fun)
			awc_proc_unset_fun(i);
		release_region(aironet4500_devices[i]->base_addr, AIRONET4X00_IO_SIZE);
//		release_region(pci_cisaddr, AIRONET4X00_CIS_SIZE, "aironet4x00 cis");
//		release_region(pci_memaddr, AIRONET4X00_MEM_SIZE, "aironet4x00 mem");

		unregister_netdev(aironet4500_devices[i]);
		free_irq(aironet4500_devices[i]->irq,aironet4500_devices[i]);
		kfree(aironet4500_devices[i]->priv);
		kfree(aironet4500_devices[i]);

		aironet4500_devices[i]=0;


		i++;
	}
	

} 


#endif //MODULE


#endif /* CONFIG_AIRONET4500_PCI */

#ifdef CONFIG_AIRONET4500_PNP

#include <linux/isapnp.h>
#define AIRONET4X00_IO_SIZE 	0x40

#define isapnp_logdev pci_dev
#define isapnp_dev    pci_bus
#define isapnp_find_device isapnp_find_card
#define isapnp_find_logdev isapnp_find_dev
#define PNP_BUS bus
#define PNP_BUS_NUMBER number
#define PNP_DEV_NUMBER devfn


int awc4500_pnp_hw_reset(struct net_device *dev){
	struct isapnp_logdev *logdev;

	DEBUG(0, "awc_pnp_reset \n");

	if (!dev->priv ) {
		printk("awc4500 no dev->priv in hw_reset\n");
		return -1;
	};

	logdev = ((struct isapnp_logdev *) ((struct awc_private *)dev->priv)->bus);

	if (!logdev ) {
		printk("awc4500 no pnp logdev in hw_reset\n");
		return -1;
	};

	if (isapnp_cfg_begin(logdev->PNP_BUS->PNP_BUS_NUMBER, logdev->PNP_DEV_NUMBER)<0)
		printk("isapnp cfg failed at release \n");
	isapnp_deactivate(logdev->PNP_DEV_NUMBER);
	isapnp_cfg_end();

	udelay(100);


	if (isapnp_cfg_begin(logdev->PNP_BUS->PNP_BUS_NUMBER, logdev->PNP_DEV_NUMBER) < 0) {
		printk("%s cfg begin failed in hw_reset for csn %x devnum %x \n",
				dev->name, logdev->PNP_BUS->PNP_BUS_NUMBER, logdev->PNP_DEV_NUMBER);
		return -EAGAIN;
	}

	isapnp_activate(logdev->PNP_DEV_NUMBER);	/* activate device */
	isapnp_cfg_end();

	return 0;
}

int awc4500_pnp_probe(struct net_device *dev)
{
	int isa_index = 0;
	int isa_irq_line = 0;
	int isa_ioaddr = 0;
	int card = 0;
	int i=0;
	struct isapnp_dev * pnp_dev ;
	struct isapnp_logdev *logdev;

	while (1) {

		pnp_dev = isapnp_find_device(
						ISAPNP_VENDOR('A','W','L'), 
						ISAPNP_DEVICE(1),
						0);
	
		if (!pnp_dev) break;  
		
		isa_index++;

		logdev = isapnp_find_logdev(pnp_dev, ISAPNP_VENDOR('A','W','L'),
				    ISAPNP_FUNCTION(1),
				    0);
		if (!logdev){
			printk("No logical device found on Aironet board \n");
			return -ENODEV;
		}
		if (isapnp_cfg_begin(logdev->PNP_BUS->PNP_BUS_NUMBER, logdev->PNP_DEV_NUMBER) < 0) {
			printk("cfg begin failed for csn %x devnum %x \n",
					logdev->PNP_BUS->PNP_BUS_NUMBER, logdev->PNP_DEV_NUMBER);
			return -EAGAIN;
		}
		isapnp_activate(logdev->PNP_DEV_NUMBER);	/* activate device */
		isapnp_cfg_end();

		isa_irq_line = logdev->irq;
		isa_ioaddr = logdev->resource[0].start;
		request_region(isa_ioaddr, AIRONET4X00_IO_SIZE, "aironet4x00 ioaddr");

		if (!dev) {
			dev = init_etherdev(NULL, 0);	
			if (!dev) {
				release_region(isa_ioaddr, AIRONET4X00_IO_SIZE);
				isapnp_cfg_begin(logdev->PNP_BUS->PNP_BUS_NUMBER,
						 logdev->PNP_DEV_NUMBER);
				isapnp_deactivate(logdev->PNP_DEV_NUMBER);
				isapnp_cfg_end();
				return -ENOMEM;
			}
		}
		dev->priv = kmalloc(sizeof(struct awc_private),GFP_KERNEL );
		memset(dev->priv,0,sizeof(struct awc_private));
		if (!dev->priv) {
			printk(KERN_CRIT "aironet4x00: could not allocate device private, some unstability may follow\n");
			return -1;
		};
		((struct awc_private *)dev->priv)->bus =  logdev;

	//	ether_setup(dev);
	
	//	dev->tx_queue_len = tx_queue_len;
	
		dev->hard_start_xmit = 		&awc_start_xmit;
	//	dev->set_config = 		&awc_config_misiganes,aga mitte awc_config;
		dev->get_stats = 		&awc_get_stats;
	//	dev->set_multicast_list = 	&awc_set_multicast_list;	
		dev->change_mtu		=	awc_change_mtu;	
		dev->init = &awc_init;
		dev->open = &awc_open;
		dev->stop = &awc_close;
	    	dev->base_addr = isa_ioaddr;
	    	dev->irq = isa_irq_line;
		dev->tx_timeout = &awc_tx_timeout;
		dev->watchdog_timeo = AWC_TX_TIMEOUT;
		
		netif_start_queue (dev);
		
		request_irq(dev->irq,awc_interrupt , SA_SHIRQ | SA_INTERRUPT ,"Aironet 4X00",dev);

		awc_private_init( dev);

		((struct awc_private *)dev->priv)->bus =  logdev;

		cli();
		if ( awc_init(dev) ){
			printk("card not found at irq %x io %lx\n",dev->irq, dev->base_addr);
			if (card==0){
				sti();
				return -1;
			}
			sti();
			break;
		}
		udelay(10);
		sti();
		i=0;
		while (aironet4500_devices[i] && i < MAX_AWCS-1) i++;
		if (!aironet4500_devices[i] && i < MAX_AWCS-1 ){
			aironet4500_devices[i]=dev;

		((struct awc_private *)
		aironet4500_devices[i]->priv)->card_type = AIRONET4500_PNP;

			if (awc_proc_set_fun)
				awc_proc_set_fun(i);	
		} else { 
			printk(KERN_CRIT "Out of resources (MAX_AWCS) \n");
			return -1;
		}

		card++;	
	}

	if (card == 0) return -ENODEV;
	return 0;
}

#ifdef MODULE
static void awc_pnp_release(void) {

//	long flags;
	int i=0;
	struct isapnp_logdev *logdev;

	DEBUG(0, "awc_detach \n");

	i=0;
	while ( i < MAX_AWCS) {
		if (!aironet4500_devices[i])
		                  {i++;      continue;}
		if (((struct awc_private *)aironet4500_devices[i]->priv)->card_type != AIRONET4500_PNP)
		                  {i++;      continue;}

		logdev = ((struct isapnp_logdev *) ((struct awc_private *)aironet4500_devices[i]->priv)->bus);

		if (!logdev ) 
			printk("awc4500 no pnp logdev in pnp_release\n");

		if (awc_proc_unset_fun)
			awc_proc_unset_fun(i);
		if (isapnp_cfg_begin(logdev->PNP_BUS->PNP_BUS_NUMBER, logdev->PNP_DEV_NUMBER)<0)
			printk("isapnp cfg failed at release \n");
		isapnp_deactivate(logdev->PNP_DEV_NUMBER);
		isapnp_cfg_end();

		release_region(aironet4500_devices[i]->base_addr, AIRONET4X00_IO_SIZE);
//		release_region(isa_cisaddr, AIRONET4X00_CIS_SIZE, "aironet4x00 cis");
//		release_region(isa_memaddr, AIRONET4X00_MEM_SIZE, "aironet4x00 mem");

		unregister_netdev(aironet4500_devices[i]);
		free_irq(aironet4500_devices[i]->irq,aironet4500_devices[i]);
		kfree(aironet4500_devices[i]->priv);
		kfree(aironet4500_devices[i]);

		aironet4500_devices[i]=0;


		i++;
	}
	

} 

static struct isapnp_device_id id_table[] = {
	{	ISAPNP_ANY_ID, ISAPNP_ANY_ID,
		ISAPNP_VENDOR('A','W','L'), ISAPNP_DEVICE(1), 0 },
	{0}
};

MODULE_DEVICE_TABLE(isapnp, id_table);

#endif //MODULE
#endif /* CONFIG_AIRONET4500_PNP */

#ifdef  CONFIG_AIRONET4500_ISA 

static int irq[] = {0,0,0,0,0};
static int io[] = {0,0,0,0,0};

/* 
	EXPORT_SYMBOL(irq);
	EXPORT_SYMBOL(io);
*/
MODULE_PARM(irq,"i");
MODULE_PARM_DESC(irq,"Aironet 4x00 ISA non-PNP irqs,required");
MODULE_PARM(io,"i");
MODULE_PARM_DESC(io,"Aironet 4x00 ISA non-PNP ioports,required");



int awc4500_isa_probe(struct net_device *dev)
{
//	int cards_found = 0;
//	static int isa_index;	/* Static, for multiple probe calls. */
	int isa_irq_line = 0;
	int isa_ioaddr = 0;
//	int p;
	int card = 0;
	int i=0;

	if (! io[0] || ! irq[0]){
	
//		printk("       Both irq and io params must be supplied  for ISA mode !!!\n");
		return -ENODEV;
	}

	printk(KERN_WARNING "     Aironet ISA Card in non-PNP(ISA) mode sometimes feels bad on interrupt \n");
	printk(KERN_WARNING "     Use aironet4500_pnp if any problems(i.e. card malfunctioning). \n");
	printk(KERN_WARNING "     Note that this isa probe is not friendly... must give exact parameters \n");

	while (irq[card] != 0){
	
		isa_ioaddr = io[card];
		isa_irq_line = irq[card];

		request_region(isa_ioaddr, AIRONET4X00_IO_SIZE, "aironet4x00 ioaddr");

		if (!dev) {
			dev = init_etherdev(NULL, 0);	
			if (!dev) {
				release_region(isa_ioaddr, AIRONET4X00_IO_SIZE);
				return (card == 0) ? -ENOMEM : 0;
			}
		}
		dev->priv = kmalloc(sizeof(struct awc_private),GFP_KERNEL );
		memset(dev->priv,0,sizeof(struct awc_private));
		if (!dev->priv) {
			printk(KERN_CRIT "aironet4x00: could not allocate device private, some unstability may follow\n");
			return -1;
		};

	//	ether_setup(dev);
	
	//	dev->tx_queue_len = tx_queue_len;
	
		dev->hard_start_xmit = 		&awc_start_xmit;
	//	dev->set_config = 		&awc_config_misiganes,aga mitte awc_config;
		dev->get_stats = 		&awc_get_stats;
	//	dev->set_multicast_list = 	&awc_set_multicast_list;	
		dev->change_mtu		=	awc_change_mtu;	
		dev->init = &awc_init;
		dev->open = &awc_open;
		dev->stop = &awc_close;
	    	dev->base_addr = isa_ioaddr;
	    	dev->irq = isa_irq_line;
		dev->tx_timeout = &awc_tx_timeout;
		dev->watchdog_timeo = AWC_TX_TIMEOUT;
		
		request_irq(dev->irq,awc_interrupt ,SA_INTERRUPT ,"Aironet 4X00",dev);

		awc_private_init( dev);
		if ( awc_init(dev) ){
			printk("card not found at irq %x mem %x\n",irq[card],io[card]);
			if (card==0)
				return -1;
			break;
		}

		i=0;
		while (aironet4500_devices[i] && i < MAX_AWCS-1) i++;
		if (!aironet4500_devices[i]){
			aironet4500_devices[i]=dev;
		((struct awc_private *)
		aironet4500_devices[i]->priv)->card_type = AIRONET4500_ISA;

			if (awc_proc_set_fun)
				awc_proc_set_fun(i);	
		}

		card++;	
	}
	if (card == 0 ) {
		return -ENODEV;
	};
	return 0;
}

#ifdef MODULE
static void awc_isa_release(void) {

//	long flags;
	int i=0;

	DEBUG(0, "awc_detach \n");

	i=0;
	while ( i < MAX_AWCS) {
	
		if (!aironet4500_devices[i])
		                  {i++;      continue;}
		if (((struct awc_private *)aironet4500_devices[i]->priv)->card_type != AIRONET4500_ISA)
		                  {i++;      continue;}

		if (awc_proc_unset_fun)
			awc_proc_unset_fun(i);
		release_region(aironet4500_devices[i]->base_addr, AIRONET4X00_IO_SIZE);
//		release_region(isa_cisaddr, AIRONET4X00_CIS_SIZE, "aironet4x00 cis");
//		release_region(isa_memaddr, AIRONET4X00_MEM_SIZE, "aironet4x00 mem");

		unregister_netdev(aironet4500_devices[i]);
		free_irq(aironet4500_devices[i]->irq,aironet4500_devices[i]);
		kfree(aironet4500_devices[i]->priv);
		kfree(aironet4500_devices[i]);

		aironet4500_devices[i]=0;


		i++;
	}
	

} 
           
#endif //MODULE   

#endif /* CONFIG_AIRONET4500_ISA */

#ifdef  CONFIG_AIRONET4500_I365 

#define port_range 0x40

int awc_i365_offset_ports[] = {0x3e0,0x3e0,0x3e2,0x3e2};
int awc_i365_data_ports [] = {0x3e1,0x3e1,0x3e3,0x3e3};
int awc_i365_irq[]	= {5,5,11,12};
int awc_i365_io[]	= {0x140,0x100,0x400,0x440};
int awc_i365_sockets	= 0;

struct i365_socket {
	int offset_port ;
	int data_port;
	int socket;
	int irq; 
	int io;
	int manufacturer;
	int product;
};
	
inline u8 i365_in (struct i365_socket * s, int offset) { 
	outb(offset  + (s->socket % 2)* 0x40, s->offset_port);
	return inb(s->data_port); 
};

inline void i365_out (struct i365_socket * s, int offset,int data){
	outb(offset + (s->socket % 2)* 0x40 ,s->offset_port);
	outb((data & 0xff),s->data_port)	;
	
};

void awc_i365_card_release(struct i365_socket * s){
	
	i365_out(s, 0x5, 0); 		// clearing ints
	i365_out(s, 0x6, 0x20); 	// mem 16 bits
	i365_out(s, 0x7, 0); 		// clear IO
	i365_out(s, 0x3, 0);		// gen ctrl reset + mem mode
	i365_out(s, 0x2, 0);		// reset power
	i365_out(s, 0x2, i365_in(s, 0x2) & 0x7f ); // cardenable off
	i365_out(s, 0x2, 0);		// remove power
	

};
int awc_i365_probe_once(struct i365_socket * s ){


	int caps=i365_in(s, 0);
	int ret;
	unsigned long jiff;
//	short rev	= 0x3000;
	unsigned char cis [0x3e3];
	unsigned char * mem = phys_to_virt(0xd000);
	int i;
	int port ;
	
	DEBUG(1," i365 control ID %x \n", caps);

	if (caps & 0xC){
		return 1;
	};
	
	ret = i365_in(s, 0x1);

	if ((ret & 0xC0) != 0xC0){
		printk("card in socket %d port %x not in known state, %x \n",
			s->socket, s->offset_port, ret );
		return -1;
	};

	
	awc_i365_card_release(s);


	mdelay(100);
	
	i365_out(s, 0x2, 0x10 ); 	// power enable
	mdelay(200);
	
	i365_out(s, 0x2, 0x10 | 0x01 | 0x04 | 0x80);	//power enable
	
	mdelay(250);
	
	if (!s->irq)
		s->irq = 11;
	
	i365_out(s, 0x3, 0x40 | 0x20 | s->irq);
	
	jiff = jiffies;
	
	while (jiffies-jiff < HZ ) 
		if (i365_in(s,0x1) & 0x20)
			break;
			
	if (! (i365_in(s,0x1) & 0x20) ){
		printk("irq enable timeout on socket %x \n", s->socket);
		return -1;
	};
	
	i365_out(s,0x10,0xd0);
	i365_out(s,0x11,0x0);
	i365_out(s,0x12,0xd0);
	i365_out(s,0x13,0x0);
	i365_out(s,0x14,0x30 );
	i365_out(s,0x15,0x3f | 0x40);		// enab mem reg bit
	i365_out(s,0x06,0x01);			// enab mem 
	
	mdelay(10);
	
	cis[0] = 0x45;
	
//	memcpy_toio( 0xd3e0, &(cis[0]),0x1);

//	mem[0x3e0] = 0x0;
//	mem[0] = 0x45;

	mem[0x3e0] = 0x45;

	mdelay(10);
	
	memcpy_fromio(cis,0xD000, 0x3e0);
	
	for (i = 0; i <= 0x3e2; i++)
		printk("%02x", mem[i]);
	for (i = 0; i <= 0x3e2; i++)
		printk("%c", mem[i]);

	i=0;	
	while (i < 0x3e0){
		if (cis[i] == 0xff)
			break;
		if (cis[i] != 0x20 ){
			i = i + 2 + cis[i+1];
			continue;
		}else {
			s->manufacturer = cis[i+2] | (cis[i+3]<<8);
			s->product	= cis[i+4] | (cis[i+5]<<8);
			break;
		};
		i++;
	};
	
	DEBUG(1,"socket %x manufacturer %x product %x \n",
		s->socket, s->manufacturer,s->product);

	i365_out(s,0x07, 0x1 | 0x2); 		// enable io 16bit
	mdelay(1);
	port = s->io;
	i365_out(s,0x08, port & 0xff);
	i365_out(s,0x09, (port & 0xff00)/ 0x100);
	i365_out(s,0x0A, (port+port_range) & 0xff);
	i365_out(s,0x0B, ((port+port_range) & 0xff00) /0x100);	

	i365_out(s,0x06, 0x40); 		// enable io window

	mdelay(1);

	i365_out(s,0x3e0,0x45);
	
	outw(0x10, s->io);

	jiff = jiffies;
	while (!(inw(s->io + 0x30) & 0x10)){
	
		if (jiffies - jiff > HZ ){
		
			printk("timed out waitin for command ack \n");
			break;
		}
	};

	
	outw(0x10, s->io + 0x34);
	mdelay(10);
	
	return 0;

};


static int awc_i365_init(struct i365_socket * s) {

	struct net_device * dev;
	int i;


	dev = init_etherdev(0, sizeof(struct awc_private) );

//	dev->tx_queue_len = tx_queue_len;
	ether_setup(dev);

	dev->hard_start_xmit = 		&awc_start_xmit;
//	dev->set_config = 		&awc_config_misiganes,aga mitte awc_config;
	dev->get_stats = 		&awc_get_stats;
	dev->set_multicast_list = 	&awc_set_multicast_list;

	dev->init = &awc_init;
	dev->open = &awc_open;
	dev->stop = &awc_close;
    	dev->irq = s->irq;
    	dev->base_addr = s->io;
	dev->tx_timeout = &awc_tx_timeout;
	dev->watchdog_timeo = AWC_TX_TIMEOUT;


	awc_private_init( dev);

	i=0;
	while (aironet4500_devices[i] && i < MAX_AWCS-1) i++;
	if (!aironet4500_devices[i]){
		aironet4500_devices[i]=dev;

		((struct awc_private *)
		aironet4500_devices[i]->priv)->card_type = AIRONET4500_365;

		if (awc_proc_set_fun)
			awc_proc_set_fun(i);
	}

	if (register_netdev(dev) != 0) {
		printk(KERN_NOTICE "awc_cs: register_netdev() failed\n");
		goto failed;
	}

	return 0;
 
  failed:
  	return -1;
}


static void awc_i365_release(void) {

//	long flags;
	int i=0;

	DEBUG(0, "awc_detach \n");

	i=0;
	while ( i < MAX_AWCS) {
	
		if (!aironet4500_devices[i])
		         {i++; continue;}

		if (((struct awc_private *)aironet4500_devices[i]->priv)->card_type != AIRONET4500_365)
		                  {i++;      continue;}

		if (awc_proc_unset_fun)
			awc_proc_unset_fun(i);

		unregister_netdev(aironet4500_devices[i]);

		//kfree(aironet4500_devices[i]->priv);
		kfree(aironet4500_devices[i]);

		aironet4500_devices[i]=0;


		i++;
	}
	

} 





        
        
int awc_i365_probe(void) {

	int i = 1;
	int k = 0;
	int ret = 0;
	int found=0;
	
	struct i365_socket s;
	/* Always emit the version, before any failure. */

	if (!awc_i365_sockets) {
		printk("	awc i82635 4x00: use bitfiel opts awc_i365_sockets=0x3 <- (1|2) to probe sockets 0 and 1\n");
		return -1;
	};

	while (k < 4){
		if (i & awc_i365_sockets){

			s.offset_port 	= awc_i365_offset_ports[k];
			s.data_port	= awc_i365_data_ports[k];
			s.socket	= k;
			s.manufacturer	= 0;
			s.product	= 0;
			s.irq		= awc_i365_irq[k];
			s.io		= awc_i365_io[k];
			
			ret = awc_i365_probe_once(&s);
			if (!ret){
				if (awc_i365_init(&s))
					goto failed;
				else found++;
			} else if (ret == -1)
				goto failed;
		};
		k++;
		i *=2;
	};

	if (!found){
		printk("no aironet 4x00 cards found\n");
		return -1;
	}
	return 0;

failed: 
	awc_i365_release();
	return -1;
	

}

#endif /* CONFIG_AIRONET4500_I365 */

#ifdef MODULE        
int init_module(void)
{
	int found = 0;

	printk("%s\n ", awc_version);
		
#ifdef CONFIG_AIRONET4500_PCI
	if (awc4500_pci_probe(NULL) == -ENODEV){
//		printk("PCI 4X00 aironet cards not found\n");
	} else {
		found++;
//		printk("PCI 4X00 found some cards \n");
	}
#endif
#ifdef CONFIG_AIRONET4500_PNP
	if (awc4500_pnp_probe(NULL) == -ENODEV){
//		printk("PNP 4X00 aironet cards not found\n");
	} else {
		found++;
//		printk("PNP 4X00 found some cards \n");
	}
#endif
#ifdef CONFIG_AIRONET4500_365
	if ( awc_i365_probe() == -1) {
//		printk("PCMCIA 4X00 aironet cards not found for i365(without card services) initialization\n");
	} else {
		 found++ ;
//		 printk("PCMCIA 4X00 found some cards, take care, this code is not supposed to work yet \n");
	}
#endif
#ifdef CONFIG_AIRONET4500_ISA
	if (awc4500_isa_probe(NULL) == -ENODEV){
//		printk("ISA 4X00 aironet ISA-bus non-PNP-mode cards not found\n");
	} else {
		found++;
//		printk("ISA 4X00 found some cards \n");
	}
#endif
	if (!found) {
		printk(KERN_ERR "No Aironet 4X00 cards were found. Note that for ISA \n cards you should use either automatic PNP mode or \n ISA mode with both io and irq param \n Aironet is also afraid of: being second PNP controller(by slot), having anything(brandname bios weirdnesses) in range 0x100-0x180 and maybe around  0xd0000\n If you PNP type card does not get found, try non-PNP switch before complainig. \n");
		return -1;
	}
	return 0;
	

}

void cleanup_module(void)
{
	DEBUG(0, "awc_cs: unloading %c ",'\n');
#ifdef CONFIG_AIRONET4500_PCI	
	awc_pci_release();
#endif
#ifdef CONFIG_AIRONET4500_PNP
	awc_pnp_release();
#endif
#ifdef CONFIG_AIRONET4500_365
	awc_i365_release();
#endif
#ifdef CONFIG_AIRONET4500_ISA
	awc_isa_release();
#endif

}
#endif
