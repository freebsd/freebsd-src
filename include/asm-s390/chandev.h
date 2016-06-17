/*
 *  include/asm-s390/chandev.h
 *
 *    Copyright (C) 2000 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Denis Joseph Barrow (djbarrow@de.ibm.com,barrow_dj@yahoo.com)
 * 
 *  Generic channel device initialisation support. 
 */
#ifndef __S390_CHANDEV_H
#define __S390_CHANDEV_H
#include <linux/version.h>
#include <asm/types.h>
#include <linux/netdevice.h>


/* Setting this flag to true causes a device name to be built based on the read_devno of the device */
/* this is exported so external code can look at this flags setting */
extern int chandev_use_devno_names;


/* chandev_type is a bitmask for registering & describing device types. */
typedef enum
{
	chandev_type_none=0x0,
	chandev_type_ctc=0x1,
	chandev_type_escon=0x2,
	chandev_type_lcs=0x4,
	chandev_type_osad=0x8,
	chandev_type_qeth=0x10,
	chandev_type_claw=0x20,
} chandev_type;

typedef enum
{
	chandev_category_none,
	chandev_category_network_device,
	chandev_category_serial_device,
} chandev_category;



typedef struct
{
	int     irq;
	u16     devno;
	u16     cu_type;      /* control unit type */
	u8      cu_model;     /* control unit model */
	u16     dev_type;     /* device type */
	u8      dev_model;    /* device model */
	u8      pim;          /* path installed mask */
	u8      chpid[8];     /* CHPID 0-7 (if available) */
} chandev_subchannel_info;

#define CLAW_NAMELEN 9
/* CLAW specific parameters other drivers should ignore these fields */
typedef struct
{
	
	char	 host_name[CLAW_NAMELEN];    /* local host name */
	char	 adapter_name[CLAW_NAMELEN]; /* workstation adapter name */
	char	 api_type[CLAW_NAMELEN];     /* API type either TCPIP or API */
} chandev_claw_info;

/*
 * The chandev_probeinfo structure is passed to the device driver with configuration
 * info for which irq's & ports to use when attempting to probe the device.
 */
typedef struct
{
	chandev_subchannel_info read;
	chandev_subchannel_info write;
	chandev_subchannel_info data;
	/* memory_usage_in_k is the suggested memory the driver should attempt to use for io */
	/* buffers -1 means use the driver default the driver should set this field to the */
	/* amount of memory it actually uses when returning this probeinfo to the channel */
	/* device layer with chandev_initdevice */
	s32     memory_usage_in_k;
	chandev_claw_info       claw;
	u8      data_exists; /* whether this device has a data channel */
	u8      cu_dev_info_inconsistent; /* either ctc or we possibly had a bad sense_id */
	u8      chpid_info_inconsistent;  /* either ctc or schib info bad */
        s16     port_protocol_no; /* 0 by default, set specifically when forcing */
	u8      hint_port_no;   /* lcs specific */
	u8      max_port_no;    /* lcs/qeth specific */
	chandev_type chan_type;
	u8      checksum_received_ip_pkts;
	u8      use_hw_stats; /* where available e.g. lcs */
	u8      device_forced; /* indicates the device hasn't been autodetected */
	char    *parmstr;       /* driver specific parameters added by add_parms keyword */
	/* newdevice used internally by chandev.c */
	struct  chandev_activelist *newdevice; 
	s32     devif_num; 
/* devif_num=-1 implies don't care,0 implies tr0, info used by chandev_initnetdevice */
} chandev_probeinfo;

/*
 * This is a wrapper to the machine check handler & should be used
 * instead of reqest_irq or s390_request_irq_special for anything
 * using the channel device layer.
 */
int chandev_request_irq(unsigned int   irq,
                      void           (*handler)(int, void *, struct pt_regs *),
                      unsigned long  irqflags,
                      const char    *devname,
                      void          *dev_id);
/*
 * I originally believed this function wouldn't be necessary
 * I subsequently found that reprobing failed in certain cases :-(,
 * It is just a wrapper for free irq.
 */
void chandev_free_irq(unsigned int irq, void *dev_id);

typedef enum
{
	chandev_status_good,
	chandev_status_not_oper,
	chandev_status_first_msck=chandev_status_not_oper,
	chandev_status_no_path,
	chandev_status_revalidate,
	chandev_status_gone,
	chandev_status_last_msck,
	chandev_status_all_chans_good /* pseudo machine check to indicate all channels are healthy */
} chandev_msck_status;

typedef int (*chandev_probefunc)(chandev_probeinfo *probeinfo);
typedef int (*chandev_shutdownfunc)(void *device);
typedef void (*chandev_unregfunc)(void *device);
typedef void (*chandev_msck_notification_func)(void *device,int msck_irq,
chandev_msck_status prevstatus,chandev_msck_status newstatus);



/* A driver should call chandev_register_and_probe when ready to be probed,
 * after registeration the drivers probefunction will be called asynchronously
 * when more devices become available at normal task time.
 * The shutdownfunc parameter is used so that the channel layer
 * can request a driver to close unregister itself & release its interrupts.
 * repoper func is used when a device becomes operational again after being temporarily
 * not operational the previous status is sent in the prevstatus variable.
 * This can be used in cases when the default handling isn't quite adequete
 * e.g. if a ssch is needed to reinitialize long running channel programs.
 *
 * This returns the number of devices found or -ENOMEM if the code didn't
 * have enough memory to allocate the chandev control block
 * or -EPERM if a duplicate entry is found.
 */
int chandev_register_and_probe(chandev_probefunc probefunc,
			       chandev_shutdownfunc shutdownfunc,
			       chandev_msck_notification_func msck_notfunc,
			       chandev_type chan_type);

/* The chandev_unregister function is typically called when a module is being removed 
 * from the system. The shutdown parameter if TRUE calls shutdownfunc for each 
 * device instance so the driver writer doesn't have to.
 */
void chandev_unregister(chandev_probefunc probefunc,int call_shutdown);

/* chandev_initdevice should be called immeadiately before returning after */
/* a successful probe. */
int chandev_initdevice(chandev_probeinfo *probeinfo,void *dev_ptr,u8 port_no,char *devname,
chandev_category category,chandev_unregfunc unreg_dev);

/* This function builds a device name & copies it into destnamebuff suitable for calling 
   init_trdev or whatever & it honours the use_devno_names flag, it is used by chandev_initnetdevice 
   setting the buildfullname flag to TRUE will cause it to always build a full unique name based 
   on basename either honouring the chandev_use_devno_names flag if set or starting at index 
   0 & checking the namespace of the channel device layer itself for a free index, this
   may be useful when one doesn't have control of the name an upper layer may choose.
   It returns NULL on error.
*/
char *chandev_build_device_name(chandev_probeinfo *probeinfo,char *destnamebuff,char *basename,int buildfullname);




/* chandev_init_netdev registers with the normal network device layer */
/* it doesn't update any of the chandev internal structures. */
/* i.e. it is optional */
/* it was part of chandev_initnetdevice but I separated it as */
/* chandev_initnetdevice may make too many assumptions for some users */
/* chandev_initnetdevice = chandev_initdevice followed by chandev_init_netdev */
#if LINUX_VERSION_CODE>=KERNEL_VERSION(2,3,0)
struct net_device *chandev_init_netdev(chandev_probeinfo *probeinfo,char *basename,
struct net_device *dev, int sizeof_priv,struct net_device *(*init_netdevfunc)(struct net_device *dev, int sizeof_priv));
#else
struct device *chandev_init_netdev(chandev_probeinfo *probeinfo,char *basename,
struct device *dev, int sizeof_priv,struct device *(*init_netdevfunc)(struct device *dev, int sizeof_priv));
#endif

/* chandev_initnetdevice registers a network device with the channel layer. 
 * It returns the device structure if successful,if dev=NULL it kmallocs it, 
 * On device initialisation failure it will kfree it under ALL curcumstances
 * i.e. if dev is not NULL on entering this routine it MUST be malloced with kmalloc. 
 * The base name is tr ( e.g. tr0 without the 0 ), for token ring eth for ethernet,
 *  ctc or escon for ctc device drivers.
 * If valid function pointers are given they will be called to setup,
 * register & unregister the device. 
 * An example of setup is eth_setup in drivers/net/net_init.c.
 * An example of init_dev is init_trdev(struct net_device *dev)
 * & an example of unregister is unregister_trdev, 
 * unregister_netdev should be used for escon & ctc
 * as there is no network unregister_ctcdev in the kernel.
*/

#if LINUX_VERSION_CODE>=KERNEL_VERSION(2,3,0)
struct net_device *chandev_initnetdevice(chandev_probeinfo *probeinfo,u8 port_no,
					 struct net_device *dev,int sizeof_priv, 
					 char *basename, 
					 struct net_device *(*init_netdevfunc)
					 (struct net_device *dev, int sizeof_priv),
					 void (*unreg_netdevfunc)(struct net_device *dev));
#else
struct device *chandev_initnetdevice(chandev_probeinfo *probeinfo,u8 port_no,
				     struct device *dev,int sizeof_priv,
				     char *basename, 
				     struct device *(*init_netdevfunc)
				     (struct device *dev, int sizeof_priv),
				     void (*unreg_netdevfunc)(struct device *dev));
#endif

/* chandev_add & delete model shouldn't normally be needed by drivers except if */
/* someone is developing a driver which the channel device layer doesn't know about */
void chandev_add_model(chandev_type chan_type,s32 cu_type,s16 cu_model,
		       s32 dev_type,s16 dev_model,u8 max_port_no,int auto_msck_recovery,
		        u8 default_checksum_received_ip_pkts,u8 default_use_hw_stats);
void chandev_del_model(s32 cu_type,s16 cu_model,s32 dev_type,s16 dev_model);

/* modules should use chandev_persist to see if they should stay loaded */
/* this is useful for debugging purposes where you may wish to examine */
/* /proc/s390dbf/ entries */
int chandev_persist(chandev_type chan_type);
#endif /* __S390_CHANDEV_H */





