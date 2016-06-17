/*****************************************************************************
* wanmain.c	WAN Multiprotocol Router Module. Main code.
*
*		This module is completely hardware-independent and provides
*		the following common services for the WAN Link Drivers:
*		 o WAN device managenment (registering, unregistering)
*		 o Network interface management
*		 o Physical connection management (dial-up, incoming calls)
*		 o Logical connection management (switched virtual circuits)
*		 o Protocol encapsulation/decapsulation
*
* Author:	Gideon Hack	
*
* Copyright:	(c) 1995-1999 Sangoma Technologies Inc.
*
*		This program is free software; you can redistribute it and/or
*		modify it under the terms of the GNU General Public License
*		as published by the Free Software Foundation; either version
*		2 of the License, or (at your option) any later version.
* ============================================================================
* Nov 24, 2000  Nenad Corbic	Updated for 2.4.X kernels 
* Nov 07, 2000  Nenad Corbic	Fixed the Mulit-Port PPP for kernels 2.2.16 and
*  				greater.
* Aug 2,  2000  Nenad Corbic	Block the Multi-Port PPP from running on
*  			        kernels 2.2.16 or greater.  The SyncPPP 
*  			        has changed.
* Jul 13, 2000  Nenad Corbic	Added SyncPPP support
* 				Added extra debugging in device_setup().
* Oct 01, 1999  Gideon Hack     Update for s514 PCI card
* Dec 27, 1996	Gene Kozin	Initial version (based on Sangoma's WANPIPE)
* Jan 16, 1997	Gene Kozin	router_devlist made public
* Jan 31, 1997  Alan Cox	Hacked it about a bit for 2.1
* Jun 27, 1997  Alan Cox	realigned with vendor code
* Oct 15, 1997  Farhan Thawar   changed wan_encapsulate to add a pad byte of 0
* Apr 20, 1998	Alan Cox	Fixed 2.1 symbols
* May 17, 1998  K. Baranowski	Fixed SNAP encapsulation in wan_encapsulate
* Dec 15, 1998  Arnaldo Melo    support for firmwares of up to 128000 bytes
*                               check wandev->setup return value
* Dec 22, 1998  Arnaldo Melo    vmalloc/vfree used in device_setup to allocate
*                               kernel memory and copy configuration data to
*                               kernel space (for big firmwares)
* Jun 02, 1999  Gideon Hack	Updates for Linux 2.0.X and 2.2.X kernels.	
*****************************************************************************/

#include <linux/version.h>
#include <linux/config.h>
#include <linux/stddef.h>	/* offsetof(), etc. */
#include <linux/errno.h>	/* return codes */
#include <linux/kernel.h>
#include <linux/module.h>	/* support for loadable modules */
#include <linux/slab.h>	/* kmalloc(), kfree() */
#include <linux/mm.h>		/* verify_area(), etc. */
#include <linux/string.h>	/* inline mem*, str* functions */

#include <asm/byteorder.h>	/* htons(), etc. */
#include <linux/wanrouter.h>	/* WAN router API definitions */


#if defined(LINUX_2_4)
 #include <linux/vmalloc.h>	/* vmalloc, vfree */
 #include <asm/uaccess.h>        /* copy_to/from_user */
 #include <linux/init.h>         /* __initfunc et al. */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,3)
 #include <net/syncppp.h>
#else
 #include <../drivers/net/wan/syncppp.h>
#endif

#elif defined(LINUX_2_1) 
 #define LINUX_2_1
 #include <linux/vmalloc.h>	/* vmalloc, vfree */
 #include <asm/uaccess.h>        /* copy_to/from_user */
 #include <linux/init.h>         /* __initfunc et al. */
 #include <../drivers/net/syncppp.h>

#else
 #include <asm/segment.h>	/* kernel <-> user copy */
#endif

#define KMEM_SAFETYZONE 8

/***********FOR DEBUGGING PURPOSES*********************************************
static void * dbg_kmalloc(unsigned int size, int prio, int line) {
	int i = 0;
	void * v = kmalloc(size+sizeof(unsigned int)+2*KMEM_SAFETYZONE*8,prio);
	char * c1 = v;	
	c1 += sizeof(unsigned int);
	*((unsigned int *)v) = size;

	for (i = 0; i < KMEM_SAFETYZONE; i++) {
		c1[0] = 'D'; c1[1] = 'E'; c1[2] = 'A'; c1[3] = 'D';
		c1[4] = 'B'; c1[5] = 'E'; c1[6] = 'E'; c1[7] = 'F';
		c1 += 8;
	}
	c1 += size;
	for (i = 0; i < KMEM_SAFETYZONE; i++) {
		c1[0] = 'M'; c1[1] = 'U'; c1[2] = 'N'; c1[3] = 'G';
		c1[4] = 'W'; c1[5] = 'A'; c1[6] = 'L'; c1[7] = 'L';
		c1 += 8;
	}
	v = ((char *)v) + sizeof(unsigned int) + KMEM_SAFETYZONE*8;
	printk(KERN_INFO "line %d  kmalloc(%d,%d) = %p\n",line,size,prio,v);
	return v;
}
static void dbg_kfree(void * v, int line) {
	unsigned int * sp = (unsigned int *)(((char *)v) - (sizeof(unsigned int) + KMEM_SAFETYZONE*8));
	unsigned int size = *sp;
	char * c1 = ((char *)v) - KMEM_SAFETYZONE*8;
	int i = 0;
	for (i = 0; i < KMEM_SAFETYZONE; i++) {
		if (   c1[0] != 'D' || c1[1] != 'E' || c1[2] != 'A' || c1[3] != 'D'
		    || c1[4] != 'B' || c1[5] != 'E' || c1[6] != 'E' || c1[7] != 'F') {
			printk(KERN_INFO "kmalloced block at %p has been corrupted (underrun)!\n",v);
			printk(KERN_INFO " %4x: %2x %2x %2x %2x %2x %2x %2x %2x\n", i*8,
			                c1[0],c1[1],c1[2],c1[3],c1[4],c1[5],c1[6],c1[7] );
		}
		c1 += 8;
	}
	c1 += size;
	for (i = 0; i < KMEM_SAFETYZONE; i++) {
		if (   c1[0] != 'M' || c1[1] != 'U' || c1[2] != 'N' || c1[3] != 'G'
		    || c1[4] != 'W' || c1[5] != 'A' || c1[6] != 'L' || c1[7] != 'L'
		   ) {
			printk(KERN_INFO "kmalloced block at %p has been corrupted (overrun):\n",v);
			printk(KERN_INFO " %4x: %2x %2x %2x %2x %2x %2x %2x %2x\n", i*8,
			                c1[0],c1[1],c1[2],c1[3],c1[4],c1[5],c1[6],c1[7] );
		}
		c1 += 8;
	}
	printk(KERN_INFO "line %d  kfree(%p)\n",line,v);
	v = ((char *)v) - (sizeof(unsigned int) + KMEM_SAFETYZONE*8);
	kfree(v);
}

#define kmalloc(x,y) dbg_kmalloc(x,y,__LINE__)
#define kfree(x) dbg_kfree(x,__LINE__)
*****************************************************************************/


/*
 * 	Function Prototypes 
 */

/* 
 * 	Kernel loadable module interface.
 */
#ifdef MODULE
int init_module (void);
void cleanup_module (void);
#endif

/* 
 *	WAN device IOCTL handlers 
 */

static int device_setup(wan_device_t *wandev, wandev_conf_t *u_conf);
static int device_stat(wan_device_t *wandev, wandev_stat_t *u_stat);
static int device_shutdown(wan_device_t *wandev);
static int device_new_if(wan_device_t *wandev, wanif_conf_t *u_conf);
static int device_del_if(wan_device_t *wandev, char *u_name);
 
/* 
 *	Miscellaneous 
 */

static wan_device_t *find_device (char *name);
static int delete_interface (wan_device_t *wandev, char *name);
void lock_adapter_irq(spinlock_t *lock, unsigned long *smp_flags);
void unlock_adapter_irq(spinlock_t *lock, unsigned long *smp_flags);



/*
 *	Global Data
 */

static char fullname[]		= "Sangoma WANPIPE Router";
static char copyright[]		= "(c) 1995-2000 Sangoma Technologies Inc.";
static char modname[]		= ROUTER_NAME;	/* short module name */
wan_device_t* router_devlist 	= NULL;	/* list of registered devices */
static int devcnt 		= 0;

/* 
 *	Organize Unique Identifiers for encapsulation/decapsulation 
 */

static unsigned char oui_ether[] = { 0x00, 0x00, 0x00 };
#if 0
static unsigned char oui_802_2[] = { 0x00, 0x80, 0xC2 };
#endif

#ifndef MODULE

int wanrouter_init(void)
{
	int err;
	extern int wanpipe_init(void);
	extern int sdladrv_init(void);

	printk(KERN_INFO "%s v%u.%u %s\n",
		fullname, ROUTER_VERSION, ROUTER_RELEASE, copyright);

	err = wanrouter_proc_init();
	if (err){
		printk(KERN_INFO "%s: can't create entry in proc filesystem!\n", modname);
	}

        /*
         *      Initialise compiled in boards
         */

#ifdef CONFIG_VENDOR_SANGOMA
	sdladrv_init();
	wanpipe_init();
#endif	
 
	return err;
}


#ifdef LINUX_2_4
static void __exit wanrouter_cleanup (void)
{
	wanrouter_proc_cleanup();
}
#endif

#else

/*
 *	Kernel Loadable Module Entry Points
 */

/*
 * 	Module 'insert' entry point.
 * 	o print announcement
 * 	o initialize static data
 * 	o create /proc/net/router directory and static entries
 *
 * 	Return:	0	Ok
 *		< 0	error.
 * 	Context:	process
 */

int init_module	(void)
{
	int err;

	printk(KERN_INFO "%s v%u.%u %s\n",
		fullname, ROUTER_VERSION, ROUTER_RELEASE, copyright);
	
	err = wanrouter_proc_init();
	
	if (err){ 
		printk(KERN_INFO
		"%s: can't create entry in proc filesystem!\n", modname);
	}
	return err;
}

/*
 * 	Module 'remove' entry point.
 * 	o delete /proc/net/router directory and static entries.
 */

void cleanup_module (void)
{
	wanrouter_proc_cleanup();
}

#endif

/*
 * 	Kernel APIs
 */

/*
 * 	Register WAN device.
 * 	o verify device credentials
 * 	o create an entry for the device in the /proc/net/router directory
 * 	o initialize internally maintained fields of the wan_device structure
 * 	o link device data space to a singly-linked list
 * 	o if it's the first device, then start kernel 'thread'
 * 	o increment module use count
 *
 * 	Return:
 *	0	Ok
 *	< 0	error.
 *
 * 	Context:	process
 */


int register_wan_device(wan_device_t *wandev)
{
	int err, namelen;

	if ((wandev == NULL) || (wandev->magic != ROUTER_MAGIC) ||
	    (wandev->name == NULL))
		return -EINVAL;
 		
	namelen = strlen(wandev->name);
	if (!namelen || (namelen > WAN_DRVNAME_SZ))
		return -EINVAL;
		
	if (find_device(wandev->name) != NULL)
		return -EEXIST;

#ifdef WANDEBUG		
	printk(KERN_INFO "%s: registering WAN device %s\n",
		modname, wandev->name);
#endif

	/*
	 *	Register /proc directory entry 
	 */
	err = wanrouter_proc_add(wandev);
	if (err) {
		printk(KERN_INFO
			"%s: can't create /proc/net/router/%s entry!\n",
			modname, wandev->name);
		return err;
	}

	/*
	 *	Initialize fields of the wan_device structure maintained by the
	 *	router and update local data.
	 */
	 
	wandev->ndev = 0;
	wandev->dev  = NULL;
	wandev->next = router_devlist;
	router_devlist = wandev;
	++devcnt;
        MOD_INC_USE_COUNT;	/* prevent module from unloading */
	return 0;
}

/*
 *	Unregister WAN device.
 *	o shut down device
 *	o unlink device data space from the linked list
 *	o delete device entry in the /proc/net/router directory
 *	o decrement module use count
 *
 *	Return:		0	Ok
 *			<0	error.
 *	Context:	process
 */


int unregister_wan_device(char *name)
{
	wan_device_t *wandev, *prev;

	if (name == NULL)
		return -EINVAL;

	for (wandev = router_devlist, prev = NULL;
		wandev && strcmp(wandev->name, name);
		prev = wandev, wandev = wandev->next)
		;
	if (wandev == NULL)
		return -ENODEV;

#ifdef WANDEBUG		
	printk(KERN_INFO "%s: unregistering WAN device %s\n", modname, name);
#endif

	if (wandev->state != WAN_UNCONFIGURED) {
		device_shutdown(wandev);
	}
	
	if (prev){
		prev->next = wandev->next;
	}else{
		router_devlist = wandev->next;
	}
	
	--devcnt;
	wanrouter_proc_delete(wandev);
        MOD_DEC_USE_COUNT;
	return 0;
}

/*
 *	Encapsulate packet.
 *
 *	Return:	encapsulation header size
 *		< 0	- unsupported Ethertype
 *
 *	Notes:
 *	1. This function may be called on interrupt context.
 */


int wanrouter_encapsulate (struct sk_buff *skb, netdevice_t *dev,
	unsigned short type)
{
	int hdr_len = 0;

	switch (type) {
	case ETH_P_IP:		/* IP datagram encapsulation */
		hdr_len += 1;
		skb_push(skb, 1);
		skb->data[0] = NLPID_IP;
		break;

	case ETH_P_IPX:		/* SNAP encapsulation */
	case ETH_P_ARP:
		hdr_len += 7;
		skb_push(skb, 7);
		skb->data[0] = 0;
		skb->data[1] = NLPID_SNAP;
		memcpy(&skb->data[2], oui_ether, sizeof(oui_ether));
		*((unsigned short*)&skb->data[5]) = htons(type);
		break;

	default:		/* Unknown packet type */
		printk(KERN_INFO
			"%s: unsupported Ethertype 0x%04X on interface %s!\n",
			modname, type, dev->name);
		hdr_len = -EINVAL;
	}
	return hdr_len;
}


/*
 *	Decapsulate packet.
 *
 *	Return:	Ethertype (in network order)
 *			0	unknown encapsulation
 *
 *	Notes:
 *	1. This function may be called on interrupt context.
 */


unsigned short wanrouter_type_trans (struct sk_buff *skb, netdevice_t *dev)
{
	int cnt = skb->data[0] ? 0 : 1;	/* there may be a pad present */
	unsigned short ethertype;

	switch (skb->data[cnt]) {
	case NLPID_IP:		/* IP datagramm */
		ethertype = htons(ETH_P_IP);
		cnt += 1;
		break;

        case NLPID_SNAP:	/* SNAP encapsulation */
		if (memcmp(&skb->data[cnt + 1], oui_ether, sizeof(oui_ether))){
          		printk(KERN_INFO
				"%s: unsupported SNAP OUI %02X-%02X-%02X "
				"on interface %s!\n", modname,
				skb->data[cnt+1], skb->data[cnt+2],
				skb->data[cnt+3], dev->name);
			return 0;
		}	
		ethertype = *((unsigned short*)&skb->data[cnt+4]);
		cnt += 6;
		break;

	/* add other protocols, e.g. CLNP, ESIS, ISIS, if needed */

	default:
		printk(KERN_INFO
			"%s: unsupported NLPID 0x%02X on interface %s!\n",
			modname, skb->data[cnt], dev->name);
		return 0;
	}
	skb->protocol = ethertype;
	skb->pkt_type = PACKET_HOST;	/*	Physically point to point */
	skb_pull(skb, cnt);
	skb->mac.raw  = skb->data;
	return ethertype;
}


/*
 *	WAN device IOCTL.
 *	o find WAN device associated with this node
 *	o execute requested action or pass command to the device driver
 */

int wanrouter_ioctl(struct inode *inode, struct file *file,
		unsigned int cmd, unsigned long arg)
{
	int err = 0;
	struct proc_dir_entry *dent;
	wan_device_t *wandev;

      #if defined (LINUX_2_1) || defined (LINUX_2_4)
	if (!capable(CAP_NET_ADMIN)){
		return -EPERM;
	}
      #endif
		
	if ((cmd >> 8) != ROUTER_IOCTL)
		return -EINVAL;
		
	dent = inode->u.generic_ip;
	if ((dent == NULL) || (dent->data == NULL))
		return -EINVAL;
		
	wandev = dent->data;
	if (wandev->magic != ROUTER_MAGIC)
		return -EINVAL;
		
	switch (cmd) {
	case ROUTER_SETUP:
		err = device_setup(wandev, (void*)arg);
		break;

	case ROUTER_DOWN:
		err = device_shutdown(wandev);
		break;

	case ROUTER_STAT:
		err = device_stat(wandev, (void*)arg);
		break;

	case ROUTER_IFNEW:
		err = device_new_if(wandev, (void*)arg);
		break;

	case ROUTER_IFDEL:
		err = device_del_if(wandev, (void*)arg);
		break;

	case ROUTER_IFSTAT:
		break;

	default:
		if ((cmd >= ROUTER_USER) &&
		    (cmd <= ROUTER_USER_MAX) &&
		    wandev->ioctl)
			err = wandev->ioctl(wandev, cmd, arg);
		else err = -EINVAL;
	}
	return err;
}

/*
 *	WAN Driver IOCTL Handlers
 */

/*
 *	Setup WAN link device.
 *	o verify user address space
 *	o allocate kernel memory and copy configuration data to kernel space
 *	o if configuration data includes extension, copy it to kernel space too
 *	o call driver's setup() entry point
 */

static int device_setup (wan_device_t *wandev, wandev_conf_t *u_conf)
{
	void *data = NULL;
	wandev_conf_t *conf;
	int err = -EINVAL;

	if (wandev->setup == NULL){	/* Nothing to do ? */
		printk(KERN_INFO "%s: ERROR, No setup script: wandev->setup()\n",
				wandev->name);
		return 0;
	}

      #ifdef LINUX_2_0 
	err = verify_area (VERIFY_READ, u_conf, sizeof(wandev_conf_t));
	if(err){
		return err;
	}
      #endif	

	conf = kmalloc(sizeof(wandev_conf_t), GFP_KERNEL);
	if (conf == NULL){
		printk(KERN_INFO "%s: ERROR, Failed to allocate kernel memory !\n",
				wandev->name);
		return -ENOBUFS;
	}
		
      #if defined (LINUX_2_1) || defined (LINUX_2_4)		
	if(copy_from_user(conf, u_conf, sizeof(wandev_conf_t))) {
		printk(KERN_INFO "%s: Failed to copy user config data to kernel space!\n",
				wandev->name);
		kfree(conf);
		return -EFAULT;
	}
      #else
	memcpy_fromfs ((void *)conf, (void *)u_conf, sizeof(wandev_conf_t));
      #endif
	
	if (conf->magic != ROUTER_MAGIC){
		kfree(conf);
		printk(KERN_INFO "%s: ERROR, Invalid MAGIC Number\n",
				wandev->name);
	        return -EINVAL; 
	}

	if (conf->data_size && conf->data){
		if(conf->data_size > 128000 || conf->data_size < 0) {
			printk(KERN_INFO 
			    "%s: ERROR, Invalid firmware data size %i !\n",
					wandev->name, conf->data_size);
			kfree(conf);
		        return -EINVAL;;
		}

#if defined (LINUX_2_1) || defined (LINUX_2_4)
		data = vmalloc(conf->data_size);
		if (data) {
			if(!copy_from_user(data, conf->data, conf->data_size)){
				conf->data=data;
				err = wandev->setup(wandev,conf);
			}else{ 
				printk(KERN_INFO 
				     "%s: ERROR, Faild to copy from user data !\n",
				       wandev->name);
				err = -EFAULT;
			}
		}else{ 
			printk(KERN_INFO 
			 	"%s: ERROR, Faild allocate kernel memory !\n",
				wandev->name);
			err = -ENOBUFS;
		}
			
		if (data){
			vfree(data);
		}
#else
                err = verify_area(VERIFY_READ, conf->data, conf->data_size);
                if (!err) {
                        data = kmalloc(conf->data_size, GFP_KERNEL);
                        if (data) {
                                memcpy_fromfs(data, (void*)conf->data,
                                        conf->data_size);
                                conf->data = data;
                        }else{
				printk(KERN_INFO 
				    "%s: ERROR, Faild allocate kernel memory !\n",wandev->name);
				err = -ENOMEM;
			}
                }else{
			printk(KERN_INFO 
			 	"%s: ERROR, Faild to copy from user data !\n",wandev->name);
		}

		if (!err){
			err = wandev->setup(wandev, conf);
		}
		
        	if (data){
			kfree(data);
		}
#endif
	}else{
		printk(KERN_INFO 
		    "%s: ERROR, No firmware found ! Firmware size = %i !\n",
				wandev->name, conf->data_size);
	}

	kfree(conf);
	return err;
}

/*
 *	Shutdown WAN device.
 *	o delete all not opened logical channels for this device
 *	o call driver's shutdown() entry point
 */
 
static int device_shutdown (wan_device_t *wandev)
{
	netdevice_t *dev;
	int err=0;
		
	if (wandev->state == WAN_UNCONFIGURED){
		return 0;
	}

	printk(KERN_INFO "\n%s: Shutting Down!\n",wandev->name);
		
	for (dev = wandev->dev; dev;) {
		if ((err=delete_interface(wandev, dev->name)) != 0){
			return err;
		}

		/* The above function deallocates the current dev
		 * structure. Therefore, we cannot use dev->priv
		 * as the next element: wandev->dev points to the
		 * next element */
		dev = wandev->dev;
	}
	
	if (wandev->ndev){
		return -EBUSY;	/* there are opened interfaces  */
	}	
	
	if (wandev->shutdown)
		err=wandev->shutdown(wandev);
	
	return err;
}

/*
 *	Get WAN device status & statistics.
 */

static int device_stat (wan_device_t *wandev, wandev_stat_t *u_stat)
{
	wandev_stat_t stat;

      #ifdef LINUX_2_0
	int err;
	err = verify_area(VERIFY_WRITE, u_stat, sizeof(wandev_stat_t));
        if (err)
                return err;
      #endif

	memset(&stat, 0, sizeof(stat));

	/* Ask device driver to update device statistics */
	if ((wandev->state != WAN_UNCONFIGURED) && wandev->update)
		wandev->update(wandev);

	/* Fill out structure */
	stat.ndev  = wandev->ndev;
	stat.state = wandev->state;

      #if defined (LINUX_2_1) || defined (LINUX_2_4)
	if(copy_to_user(u_stat, &stat, sizeof(stat)))
		return -EFAULT;
      #else
        memcpy_tofs((void*)u_stat, (void*)&stat, sizeof(stat));
      #endif

	return 0;
}

/*
 *	Create new WAN interface.
 *	o verify user address space
 *	o copy configuration data to kernel address space
 *	o allocate network interface data space
 *	o call driver's new_if() entry point
 *	o make sure there is no interface name conflict
 *	o register network interface
 */

static int device_new_if (wan_device_t *wandev, wanif_conf_t *u_conf)
{
	wanif_conf_t conf;
	netdevice_t *dev=NULL;
#ifdef CONFIG_WANPIPE_MULTPPP
	struct ppp_device *pppdev=NULL;
#endif
	int err;

	if ((wandev->state == WAN_UNCONFIGURED) || (wandev->new_if == NULL))
		return -ENODEV;
	
#if defined (LINUX_2_1) || defined (LINUX_2_4)	
	if(copy_from_user(&conf, u_conf, sizeof(wanif_conf_t)))
		return -EFAULT;
#else
        err = verify_area(VERIFY_READ, u_conf, sizeof(wanif_conf_t));
        if (err)
                return err;
        memcpy_fromfs((void*)&conf, (void*)u_conf, sizeof(wanif_conf_t));
#endif
		
	if (conf.magic != ROUTER_MAGIC)
		return -EINVAL;

	err = -EPROTONOSUPPORT;

	
#ifdef CONFIG_WANPIPE_MULTPPP
	if (conf.config_id == WANCONFIG_MPPP){

		pppdev = kmalloc(sizeof(struct ppp_device), GFP_KERNEL);
		if (pppdev == NULL){
			return -ENOBUFS;
		}
		memset(pppdev, 0, sizeof(struct ppp_device));

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,16)
		pppdev->dev = kmalloc(sizeof(netdevice_t), GFP_KERNEL);
		if (pppdev->dev == NULL){
			kfree(pppdev);
			return -ENOBUFS;
		}
		memset(pppdev->dev, 0, sizeof(netdevice_t));
#endif
		
		err = wandev->new_if(wandev, (netdevice_t *)pppdev, &conf);
		
		      #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,16)
			dev = pppdev->dev;
		      #else
			dev = &pppdev->dev;
		      #endif
			
	}else{

		dev = kmalloc(sizeof(netdevice_t), GFP_KERNEL);
		if (dev == NULL){
			return -ENOBUFS;
		}
		memset(dev, 0, sizeof(netdevice_t));	
		err = wandev->new_if(wandev, dev, &conf);
	}

#else
	/* Sync PPP is disabled */
	if (conf.config_id != WANCONFIG_MPPP){

		dev = kmalloc(sizeof(netdevice_t), GFP_KERNEL);
		if (dev == NULL){
			return -ENOBUFS;
		}
		memset(dev, 0, sizeof(netdevice_t));	
		err = wandev->new_if(wandev, dev, &conf);
	}else{
		printk(KERN_INFO "%s: Wanpipe Mulit-Port PPP support has not been compiled in!\n",
				wandev->name);
		return err;
	}
#endif
	
	if (!err) {
		/* Register network interface. This will invoke init()
		 * function supplied by the driver.  If device registered
		 * successfully, add it to the interface list.
		 */

		if (dev->name == NULL){
			err = -EINVAL;
		}else if (dev_get(dev->name)){
			err = -EEXIST;	/* name already exists */
		}else{
				
			#ifdef WANDEBUG		
			printk(KERN_INFO "%s: registering interface %s...\n",
				modname, dev->name);
			#endif				
			
			err = register_netdev(dev);
			if (!err) {
				netdevice_t *slave=NULL;
				unsigned long smp_flags=0;
				
				lock_adapter_irq(&wandev->lock, &smp_flags);
			
				if (wandev->dev == NULL){
					wandev->dev = dev;
				}else{
					for (slave=wandev->dev;
					     *((netdevice_t**)slave->priv);
					     slave=*((netdevice_t**)slave->priv));

					*((netdevice_t**)slave->priv) = dev;
				}
				++wandev->ndev;
				
				unlock_adapter_irq(&wandev->lock, &smp_flags);
				return 0;	/* done !!! */
			}
		}
		if (wandev->del_if)
			wandev->del_if(wandev, dev);
	}

	/* This code has moved from del_if() function */
	if (dev->priv){
		kfree(dev->priv);
		dev->priv=NULL;
	}

	
      #ifdef CONFIG_WANPIPE_MULTPPP
	if (conf.config_id == WANCONFIG_MPPP){
		kfree(pppdev);
	}else{
		kfree(dev);
	}
      #else
	/* Sync PPP is disabled */
	if (conf.config_id != WANCONFIG_MPPP){
		kfree(dev);
	}
      #endif
	
	return err;
}


/*
 *	Delete WAN logical channel.
 *	 o verify user address space
 *	 o copy configuration data to kernel address space
 */

static int device_del_if (wan_device_t *wandev, char *u_name)
{
	char name[WAN_IFNAME_SZ + 1];
        int err = 0;

	if (wandev->state == WAN_UNCONFIGURED)
		return -ENODEV;
	
      #ifdef LINUX_2_0
        err = verify_area(VERIFY_READ, u_name, WAN_IFNAME_SZ);
        if (err)
		return err;
      #endif	

	memset(name, 0, sizeof(name));

      #if defined (LINUX_2_1) || defined (LINUX_2_4)
	if(copy_from_user(name, u_name, WAN_IFNAME_SZ))
		return -EFAULT;
      #else
        memcpy_fromfs((void*)name, (void*)u_name, WAN_IFNAME_SZ);
      #endif

	err = delete_interface(wandev, name);
	if (err)
		return(err);

	/* If last interface being deleted, shutdown card
	 * This helps with administration at leaf nodes
	 * (You can tell if the person at the other end of the phone 
	 * has an interface configured) and avoids DoS vulnerabilities
	 * in binary driver files - this fixes a problem with the current
	 * Sangoma driver going into strange states when all the network
	 * interfaces are deleted and the link irrecoverably disconnected.
	 */ 

        if (!wandev->ndev && wandev->shutdown){
                err = wandev->shutdown(wandev);
	}
	return err;
}


/*
 *	Miscellaneous Functions
 */

/*
 *	Find WAN device by name.
 *	Return pointer to the WAN device data space or NULL if device not found.
 */

static wan_device_t *find_device(char *name)
{
	wan_device_t *wandev;

	for (wandev = router_devlist;wandev && strcmp(wandev->name, name);
		wandev = wandev->next);
	return wandev;
}

/*
 *	Delete WAN logical channel identified by its name.
 *	o find logical channel by its name
 *	o call driver's del_if() entry point
 *	o unregister network interface
 *	o unlink channel data space from linked list of channels
 *	o release channel data space
 *
 *	Return:	0		success
 *		-ENODEV		channel not found.
 *		-EBUSY		interface is open
 *
 *	Note: If (force != 0), then device will be destroyed even if interface
 *	associated with it is open. It's caller's responsibility to make
 *	sure that opened interfaces are not removed!
 */

static int delete_interface (wan_device_t *wandev, char *name)
{
	netdevice_t *dev=NULL, *prev=NULL;
	unsigned long smp_flags=0;

	lock_adapter_irq(&wandev->lock, &smp_flags);
	dev = wandev->dev;
	prev = NULL;
	while (dev && strcmp(name, dev->name)) {
		netdevice_t **slave = dev->priv;
		prev = dev;
		dev = *slave;
	}
	unlock_adapter_irq(&wandev->lock, &smp_flags);
	
	if (dev == NULL){
		return -ENODEV;	/* interface not found */
	}
		
       #ifdef LINUX_2_4
	if (netif_running(dev)){
       #else
	if (dev->start) {
       #endif
		return -EBUSY;	/* interface in use */
	}

	if (wandev->del_if)
		wandev->del_if(wandev, dev);

	lock_adapter_irq(&wandev->lock, &smp_flags);
	if (prev) {
		netdevice_t **prev_slave = prev->priv;
		netdevice_t **slave = dev->priv;

		*prev_slave = *slave;
	} else {
		netdevice_t **slave = dev->priv;
		wandev->dev = *slave;
	}
	--wandev->ndev;
	unlock_adapter_irq(&wandev->lock, &smp_flags);
	
	printk(KERN_INFO "%s: unregistering '%s'\n", wandev->name, dev->name); 
	
	/* Due to new interface linking method using dev->priv,
	 * this code has moved from del_if() function.*/
	if (dev->priv){
		kfree(dev->priv);
		dev->priv=NULL;
	}

	unregister_netdev(dev);

      #ifdef LINUX_2_4
	kfree(dev);
      #else
	if (dev->name){
		kfree(dev->name);
	}
	kfree(dev);
      #endif

	return 0;
}

void lock_adapter_irq(spinlock_t *lock, unsigned long *smp_flags)
{
      #ifdef LINUX_2_0
	save_flags(*smp_flags);
	cli();
      #else
       	spin_lock_irqsave(lock, *smp_flags);
      #endif
}


void unlock_adapter_irq(spinlock_t *lock, unsigned long *smp_flags)
{
      #ifdef LINUX_2_0
	restore_flags(*smp_flags);
      #else
	spin_unlock_irqrestore(lock, *smp_flags);
      #endif
}



#if defined (LINUX_2_1) || defined (LINUX_2_4)
EXPORT_SYMBOL(register_wan_device);
EXPORT_SYMBOL(unregister_wan_device);
EXPORT_SYMBOL(wanrouter_encapsulate);
EXPORT_SYMBOL(wanrouter_type_trans);
EXPORT_SYMBOL(lock_adapter_irq);
EXPORT_SYMBOL(unlock_adapter_irq);
#endif

/*
 *	End
 */
