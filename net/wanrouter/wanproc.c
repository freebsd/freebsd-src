/*****************************************************************************
* wanproc.c	WAN Router Module. /proc filesystem interface.
*
*		This module is completely hardware-independent and provides
*		access to the router using Linux /proc filesystem.
*
* Author: 	Gideon Hack	
*
* Copyright:	(c) 1995-1999 Sangoma Technologies Inc.
*
*		This program is free software; you can redistribute it and/or
*		modify it under the terms of the GNU General Public License
*		as published by the Free Software Foundation; either version
*		2 of the License, or (at your option) any later version.
* ============================================================================
* Jun 02, 1999  Gideon Hack	Updates for Linux 2.2.X kernels.
* Jun 29, 1997	Alan Cox	Merged with 1.0.3 vendor code
* Jan 29, 1997	Gene Kozin	v1.0.1. Implemented /proc read routines
* Jan 30, 1997	Alan Cox	Hacked around for 2.1
* Dec 13, 1996	Gene Kozin	Initial version (based on Sangoma's WANPIPE)
*****************************************************************************/

#include <linux/version.h>
#include <linux/config.h>
#include <linux/stddef.h>	/* offsetof(), etc. */
#include <linux/errno.h>	/* return codes */
#include <linux/kernel.h>
#include <linux/slab.h>	/* kmalloc(), kfree() */
#include <linux/mm.h>		/* verify_area(), etc. */
#include <linux/string.h>	/* inline mem*, str* functions */
#include <asm/byteorder.h>	/* htons(), etc. */
#include <asm/io.h>
#include <linux/wanrouter.h>	/* WAN router API definitions */



#if defined(LINUX_2_1) || defined(LINUX_2_4) 
 #include <linux/init.h>	/* __initfunc et al. */
 #include <asm/uaccess.h>       /* copy_to_user */
 #define PROC_STATS_FORMAT "%30s: %12lu\n"
#else
 #define PROC_STATS_FORMAT "%30s: %12u\n"
 #include <asm/segment.h>	/* kernel <-> user copy */
#endif


/****** Defines and Macros **************************************************/

#define	PROC_BUFSZ	4000	/* buffer size for printing proc info */

#define PROT_DECODE(prot) ((prot == WANCONFIG_FR) ? " FR" :\
			      (prot == WANCONFIG_X25) ? " X25" : \
			         (prot == WANCONFIG_PPP) ? " PPP" : \
				    (prot == WANCONFIG_CHDLC) ? " CHDLC": \
				       (prot == WANCONFIG_MPPP) ? " MPPP" : \
				           " Unknown" )
	
/****** Data Types **********************************************************/

typedef struct wan_stat_entry
{
	struct wan_stat_entry *next;
	char *description;		/* description string */
	void *data;			/* -> data */
	unsigned data_type;		/* data type */
} wan_stat_entry_t;

/****** Function Prototypes *************************************************/

#ifdef CONFIG_PROC_FS


#ifdef LINUX_2_4  /* Start of LINUX 2.4.X code */


	/* Proc filesystem interface */
	static int router_proc_perms(struct inode *, int);
	static ssize_t router_proc_read(struct file* file, char* buf, size_t count, 					loff_t *ppos);

	/* Methods for preparing data for reading proc entries */

	static int config_get_info(char* buf, char** start, off_t offs, int len);
	static int status_get_info(char* buf, char** start, off_t offs, int len);
	static int wandev_get_info(char* buf, char** start, off_t offs, int len);

	/* Miscellaneous */

	/*
	 *	Structures for interfacing with the /proc filesystem.
	 *	Router creates its own directory /proc/net/router with the folowing
	 *	entries:
	 *	config		device configuration
	 *	status		global device statistics
	 *	<device>	entry for each WAN device
	 */

	/*
	 *	Generic /proc/net/router/<file> file and inode operations 
	 */
	static struct file_operations router_fops =
	{
		read:		router_proc_read,
	};

	static struct inode_operations router_inode =
	{
		permission:	router_proc_perms,
	};

	/*
	 *	/proc/net/router/<device> file operations
	 */

	static struct file_operations wandev_fops =
	{
		read:		router_proc_read,
		ioctl:		wanrouter_ioctl,
	};

	/*
	 *	/proc/net/router 
	 */

	static struct proc_dir_entry *proc_router;

	/* Strings */
	static char conf_hdr[] =
		"Device name    | port |IRQ|DMA|  mem.addr  |mem.size|"
		"option1|option2|option3|option4\n";
		
	static char stat_hdr[] =
		"Device name    |protocol|station|interface|clocking|baud rate"
		"| MTU |ndev|link state\n";


	/*
	 *	Interface functions
	 */

	/*
	 *	Initialize router proc interface.
	 */

	int __init wanrouter_proc_init (void)
	{
		struct proc_dir_entry *p;
		proc_router = proc_mkdir(ROUTER_NAME, proc_net);
		if (!proc_router)
			goto fail;

		p = create_proc_entry("config",0,proc_router);
		if (!p)
			goto fail_config;
		p->proc_fops = &router_fops;
		p->proc_iops = &router_inode;
		p->get_info = config_get_info;
		p = create_proc_entry("status",0,proc_router);
		if (!p)
			goto fail_stat;
		p->proc_fops = &router_fops;
		p->proc_iops = &router_inode;
		p->get_info = status_get_info;
		return 0;
	fail_stat:
		remove_proc_entry("config", proc_router);
	fail_config:
		remove_proc_entry(ROUTER_NAME, proc_net);
	fail:
		return -ENOMEM;
	}

	/*
	 *	Clean up router proc interface.
	 */

	void wanrouter_proc_cleanup (void)
	{
		remove_proc_entry("config", proc_router);
		remove_proc_entry("status", proc_router);
		remove_proc_entry(ROUTER_NAME,proc_net);
	}

	/*
	 *	Add directory entry for WAN device.
	 */

	int wanrouter_proc_add (wan_device_t* wandev)
	{
		if (wandev->magic != ROUTER_MAGIC)
			return -EINVAL;
			
		wandev->dent = create_proc_entry(wandev->name, 0, proc_router);
		if (!wandev->dent)
			return -ENOMEM;
		wandev->dent->proc_fops	= &wandev_fops;
		wandev->dent->proc_iops	= &router_inode;
		wandev->dent->get_info	= wandev_get_info;
		wandev->dent->data	= wandev;
		return 0;
	}

	/*
	 *	Delete directory entry for WAN device.
	 */
	 
	int wanrouter_proc_delete(wan_device_t* wandev)
	{
		if (wandev->magic != ROUTER_MAGIC)
			return -EINVAL;
		remove_proc_entry(wandev->name, proc_router);
		return 0;
	}

	/****** Proc filesystem entry points ****************************************/

	/*
	 *	Verify access rights.
	 */

	static int router_proc_perms (struct inode* inode, int op)
	{
		return 0;
	}

	/*
	 *	Read router proc directory entry.
	 *	This is universal routine for reading all entries in /proc/net/wanrouter
	 *	directory.  Each directory entry contains a pointer to the 'method' for
	 *	preparing data for that entry.
	 *	o verify arguments
	 *	o allocate kernel buffer
	 *	o call get_info() to prepare data
	 *	o copy data to user space
	 *	o release kernel buffer
	 *
	 *	Return:	number of bytes copied to user space (0, if no data)
	 *		<0	error
	 */

	static ssize_t router_proc_read(struct file* file, char* buf, size_t count,
					loff_t *ppos)
	{
		struct inode *inode = file->f_dentry->d_inode;
		struct proc_dir_entry* dent;
		char* page;
		int pos, offs, len;

		if (count <= 0)
			return 0;
			
		dent = inode->u.generic_ip;
		if ((dent == NULL) || (dent->get_info == NULL))
			return 0;
			
		page = kmalloc(PROC_BUFSZ, GFP_KERNEL);
		if (page == NULL)
			return -ENOBUFS;
			
		pos = dent->get_info(page, dent->data, 0, 0);
		offs = file->f_pos;
		if (offs < pos) {
			len = min_t(unsigned int, pos - offs, count);
			if (copy_to_user(buf, (page + offs), len)) {
				kfree(page);
				return -EFAULT;
			}
			file->f_pos += len;
		}
		else
			len = 0;
		kfree(page);
		return len;
	}

	/*
	 *	Prepare data for reading 'Config' entry.
	 *	Return length of data.
	 */

	static int config_get_info(char* buf, char** start, off_t offs, int len)
	{
		int cnt = sizeof(conf_hdr) - 1;
		wan_device_t* wandev;
		strcpy(buf, conf_hdr);
		for (wandev = router_devlist;
		     wandev && (cnt < (PROC_BUFSZ - 120));
		     wandev = wandev->next) {
			if (wandev->state) cnt += sprintf(&buf[cnt],
				"%-15s|0x%-4X|%3u|%3u| 0x%-8lX |0x%-6X|%7u|%7u|%7u|%7u\n",
				wandev->name,
				wandev->ioport,
				wandev->irq,
				wandev->dma,
				wandev->maddr,
				wandev->msize,
				wandev->hw_opt[0],
				wandev->hw_opt[1],
				wandev->hw_opt[2],
				wandev->hw_opt[3]);
		}

		return cnt;
	}

	/*
	 *	Prepare data for reading 'Status' entry.
	 *	Return length of data.
	 */

	static int status_get_info(char* buf, char** start, off_t offs, int len)
	{
		int cnt = 0;
		wan_device_t* wandev;

		//cnt += sprintf(&buf[cnt], "\nSTATUS:\n\n");
		strcpy(&buf[cnt], stat_hdr);
		cnt += sizeof(stat_hdr) - 1;

		for (wandev = router_devlist;
		     wandev && (cnt < (PROC_BUFSZ - 80));
		     wandev = wandev->next) {
			if (!wandev->state) continue;
			cnt += sprintf(&buf[cnt],
				"%-15s|%-8s|%-7s|%-9s|%-8s|%9u|%5u|%3u |",
				wandev->name,
				PROT_DECODE(wandev->config_id),
				wandev->config_id == WANCONFIG_FR ? 
					(wandev->station ? " Node" : " CPE") :
					(wandev->config_id == WANCONFIG_X25 ?
					(wandev->station ? " DCE" : " DTE") :
					(" N/A")),
				wandev->interface ? " V.35" : " RS-232",
				wandev->clocking ? "internal" : "external",
				wandev->bps,
				wandev->mtu,
				wandev->ndev);

			switch (wandev->state) {

			case WAN_UNCONFIGURED:
				cnt += sprintf(&buf[cnt], "%-12s\n", "unconfigured");
				break;

			case WAN_DISCONNECTED:
				cnt += sprintf(&buf[cnt], "%-12s\n", "disconnected");
				break;

			case WAN_CONNECTING:
				cnt += sprintf(&buf[cnt], "%-12s\n", "connecting");
				break;

			case WAN_CONNECTED:
				cnt += sprintf(&buf[cnt], "%-12s\n", "connected");
				break;

			default:
				cnt += sprintf(&buf[cnt], "%-12s\n", "invalid");
				break;
			}
		}
		return cnt;
	}

	/*
	 *	Prepare data for reading <device> entry.
	 *	Return length of data.
	 *
	 *	On entry, the 'start' argument will contain a pointer to WAN device
	 *	data space.
	 */

	static int wandev_get_info(char* buf, char** start, off_t offs, int len)
	{
		wan_device_t* wandev = (void*)start;
		int cnt = 0;
		int rslt = 0;

		if ((wandev == NULL) || (wandev->magic != ROUTER_MAGIC))
			return 0;
		if (!wandev->state)
			return sprintf(&buf[cnt], "device is not configured!\n");

		/* Update device statistics */
		if (wandev->update) {

			rslt = wandev->update(wandev);
			if(rslt) {
				switch (rslt) {
				case -EAGAIN:
					return sprintf(&buf[cnt], "Device is busy!\n");

				default:
					return sprintf(&buf[cnt],
						"Device is not configured!\n");
				}
			}
		}

		cnt += sprintf(&buf[cnt], PROC_STATS_FORMAT,
			"total packets received", wandev->stats.rx_packets);
		cnt += sprintf(&buf[cnt], PROC_STATS_FORMAT,
			"total packets transmitted", wandev->stats.tx_packets);
		cnt += sprintf(&buf[cnt], PROC_STATS_FORMAT,
			"total bytes received", wandev->stats.rx_bytes);
		cnt += sprintf(&buf[cnt], PROC_STATS_FORMAT,
			"total bytes transmitted", wandev->stats.tx_bytes);
		cnt += sprintf(&buf[cnt], PROC_STATS_FORMAT,
			"bad packets received", wandev->stats.rx_errors);
		cnt += sprintf(&buf[cnt], PROC_STATS_FORMAT,
			"packet transmit problems", wandev->stats.tx_errors);
		cnt += sprintf(&buf[cnt], PROC_STATS_FORMAT,
			"received frames dropped", wandev->stats.rx_dropped);
		cnt += sprintf(&buf[cnt], PROC_STATS_FORMAT,
			"transmit frames dropped", wandev->stats.tx_dropped);
		cnt += sprintf(&buf[cnt], PROC_STATS_FORMAT,
			"multicast packets received", wandev->stats.multicast);
		cnt += sprintf(&buf[cnt], PROC_STATS_FORMAT,
			"transmit collisions", wandev->stats.collisions);
		cnt += sprintf(&buf[cnt], PROC_STATS_FORMAT,
			"receive length errors", wandev->stats.rx_length_errors);
		cnt += sprintf(&buf[cnt], PROC_STATS_FORMAT,
			"receiver overrun errors", wandev->stats.rx_over_errors);
		cnt += sprintf(&buf[cnt], PROC_STATS_FORMAT,
			"CRC errors", wandev->stats.rx_crc_errors);
		cnt += sprintf(&buf[cnt], PROC_STATS_FORMAT,
			"frame format errors (aborts)", wandev->stats.rx_frame_errors);
		cnt += sprintf(&buf[cnt], PROC_STATS_FORMAT,
			"receiver fifo overrun", wandev->stats.rx_fifo_errors);
		cnt += sprintf(&buf[cnt], PROC_STATS_FORMAT,
			"receiver missed packet", wandev->stats.rx_missed_errors);
		cnt += sprintf(&buf[cnt], PROC_STATS_FORMAT,
			"aborted frames transmitted", wandev->stats.tx_aborted_errors);
		return cnt;
	}


#else /* ------------------- END OF LINUX 2.4.X VERSION -------------*/



	/* Proc filesystem interface */
	static int router_proc_perms(struct inode *, int);
#ifdef LINUX_2_1
	static ssize_t router_proc_read(struct file *file, char *buf, size_t count, 					loff_t *ppos);
#else
	static int router_proc_read(
		struct inode* inode, struct file* file, char* buf, int count);
	static int device_write(
		struct inode* inode, struct file* file, const char* buf, int count);
#endif

	/* Methods for preparing data for reading proc entries */
	static int config_get_info(char* buf, char** start, off_t offs, int len,
		int dummy);
	static int status_get_info(char* buf, char** start, off_t offs, int len,
		int dummy);
	static int wandev_get_info(char* buf, char** start, off_t offs, int len,
		int dummy);

	/* Miscellaneous */

	/*
	 *	Global Data
	 */

	/*
	 *	Names of the proc directory entries 
	 */

	static char name_root[]	= ROUTER_NAME;
	static char name_conf[]	= "config";
	static char name_stat[]	= "status";

	/*
	 *	Structures for interfacing with the /proc filesystem.
	 *	Router creates its own directory /proc/net/router with the folowing
	 *	entries:
	 *	config		device configuration
	 *	status		global device statistics
	 *	<device>	entry for each WAN device
	 */

	/*
	 *	Generic /proc/net/router/<file> file and inode operations 
	 */
#ifdef LINUX_2_1
	static struct file_operations router_fops =
	{
		NULL,			/* lseek   */
		router_proc_read,	/* read	   */
		NULL,			/* write   */
		NULL,			/* readdir */
		NULL,			/* select  */
		NULL,			/* ioctl   */
		NULL,			/* mmap	   */
		NULL,			/* no special open code	   */
		NULL,			/* flush */
		NULL,			/* no special release code */
		NULL			/* can't fsync */
	};
#else
	static struct file_operations router_fops =
	{
		NULL,                   /* lseek   */
		router_proc_read,       /* read    */
		NULL,                   /* write   */
		NULL,                   /* readdir */
		NULL,                   /* select  */
		NULL,                   /* ioctl   */
		NULL,                   /* mmap    */
		NULL,                   /* no special open code    */
		NULL,                   /* no special release code */
		NULL                    /* can't fsync */
	};
#endif

	static struct inode_operations router_inode =
	{
		&router_fops,
		NULL,			/* create */
		NULL,			/* lookup */
		NULL,			/* link */
		NULL,			/* unlink */
		NULL,			/* symlink */
		NULL,			/* mkdir */
		NULL,			/* rmdir */
		NULL,			/* mknod */
		NULL,			/* rename */
		NULL,			/* follow link */
		NULL,			/* readlink */
		NULL,			/* readpage */
		NULL,			/* writepage */
		NULL,			/* bmap */
		NULL,			/* truncate */
		router_proc_perms
	};

	/*
	 *	/proc/net/router/<device> file and inode operations
	 */

#ifdef LINUX_2_1
	static struct file_operations wandev_fops =
	{
		NULL,			/* lseek   */
		router_proc_read,	/* read	   */
		NULL,			/* write   */
		NULL,			/* readdir */
		NULL,			/* select  */
		wanrouter_ioctl,	/* ioctl   */
		NULL,			/* mmap	   */
		NULL,			/* no special open code	   */
		NULL,			/* flush */
		NULL,			/* no special release code */
		NULL			/* can't fsync */
	};
#else
	static struct file_operations wandev_fops =
	{
		NULL,                   /* lseek   */
		router_proc_read,       /* read    */
		device_write,           /* write   */
		NULL,                   /* readdir */
		NULL,                   /* select  */
		wanrouter_ioctl,        /* ioctl   */
		NULL,                   /* mmap    */
		NULL,                   /* no special open code    */
		NULL,                   /* no special release code */
		NULL                    /* can't fsync */
	};
#endif

	static struct inode_operations wandev_inode =
	{
		&wandev_fops,
		NULL,			/* create */
		NULL,			/* lookup */
		NULL,			/* link */
		NULL,			/* unlink */
		NULL,			/* symlink */
		NULL,			/* mkdir */
		NULL,			/* rmdir */
		NULL,			/* mknod */
		NULL,			/* rename */
		NULL,			/* readlink */
		NULL,			/* follow_link */
		NULL,			/* readpage */
		NULL,			/* writepage */
		NULL,			/* bmap */
		NULL,			/* truncate */
		router_proc_perms
	};

	/*
	 * Proc filesystem derectory entries.
	 */

	/*
	 *	/proc/net/router 
	 */
	 
	static struct proc_dir_entry proc_router =
	{
		0,			/* .low_ino */
		sizeof(name_root) - 1,	/* .namelen */
		name_root,		/* .name */
		0555 | S_IFDIR,		/* .mode */
		2,			/* .nlink */
		0,			/* .uid */
		0,			/* .gid */
		0,			/* .size */
		&proc_dir_inode_operations, /* .ops */
		NULL,			/* .get_info */
		NULL,			/* .fill_node */
		NULL,			/* .next */
		NULL,			/* .parent */
		NULL,			/* .subdir */
		NULL,			/* .data */
	};

	/*
	 *	/proc/net/router/config 
	 */
	 
	static struct proc_dir_entry proc_router_conf =
	{
		0,			/* .low_ino */
		sizeof(name_conf) - 1,	/* .namelen */
		name_conf,		/* .name */
		0444 | S_IFREG,		/* .mode */
		1,			/* .nlink */
		0,			/* .uid */
		0,			/* .gid */
		0,			/* .size */
		&router_inode,		/* .ops */
		&config_get_info,	/* .get_info */
		NULL,			/* .fill_node */
		NULL,			/* .next */
		NULL,			/* .parent */
		NULL,			/* .subdir */
		NULL,			/* .data */
	};

	/*
	 *	/proc/net/router/status 
	 */
	 
	static struct proc_dir_entry proc_router_stat =
	{
		0,			/* .low_ino */
		sizeof(name_stat) - 1,	/* .namelen */
		name_stat,		/* .name */
		0444 | S_IFREG,		/* .mode */
		1,			/* .nlink */
		0,			/* .uid */
		0,			/* .gid */
		0,			/* .size */
		&router_inode,		/* .ops */
		status_get_info,	/* .get_info */
		NULL,			/* .fill_node */
		NULL,			/* .next */
		NULL,			/* .parent */
		NULL,			/* .subdir */
		NULL,			/* .data */
	};

	/* Strings */
	static char conf_hdr[] =
		"Device name    | port |IRQ|DMA|  mem.addr  |mem.size|"
		"option1|option2|option3|option4\n";
		
	static char stat_hdr[] =
		"Device name    |protocol|station|interface|clocking|baud rate| MTU |ndev"
		"|link state\n";


	/*
	 *	Interface functions
	 */

	/*
	 *	Initialize router proc interface.
	 */

#ifdef LINUX_2_1
	__initfunc(int wanrouter_proc_init (void))
	{
		int err = proc_register(proc_net, &proc_router);

		if (!err) {
			proc_register(&proc_router, &proc_router_conf);
			proc_register(&proc_router, &proc_router_stat);
		}
		return err;
	}
#else
	int wanrouter_proc_init (void)
	{
		int err = proc_register_dynamic(&proc_net, &proc_router);

		if (!err) {
			proc_register_dynamic(&proc_router, &proc_router_conf);
			proc_register_dynamic(&proc_router, &proc_router_stat);
		}
		return err;
	}
#endif

	/*
	 *	Clean up router proc interface.
	 */

	void wanrouter_proc_cleanup (void)
	{
		proc_unregister(&proc_router, proc_router_conf.low_ino);
		proc_unregister(&proc_router, proc_router_stat.low_ino);
#ifdef LINUX_2_1
		proc_unregister(proc_net, proc_router.low_ino);
#else
		proc_unregister(&proc_net, proc_router.low_ino);
#endif
	}

	/*
	 *	Add directory entry for WAN device.
	 */

	int wanrouter_proc_add (wan_device_t* wandev)
	{
		if (wandev->magic != ROUTER_MAGIC)
			return -EINVAL;
		
		memset(&wandev->dent, 0, sizeof(wandev->dent));
		wandev->dent.namelen	= strlen(wandev->name);
		wandev->dent.name	= wandev->name;
		wandev->dent.mode	= 0444 | S_IFREG;
		wandev->dent.nlink	= 1;
		wandev->dent.ops	= &wandev_inode;
		wandev->dent.get_info	= &wandev_get_info;
		wandev->dent.data	= wandev;
#ifdef LINUX_2_1
		return proc_register(&proc_router, &wandev->dent);
#else
		return proc_register_dynamic(&proc_router, &wandev->dent);
#endif
	}

	/*
	 *	Delete directory entry for WAN device.
	 */
	 
	int wanrouter_proc_delete(wan_device_t* wandev)
	{
		if (wandev->magic != ROUTER_MAGIC)
			return -EINVAL;
		proc_unregister(&proc_router, wandev->dent.low_ino);
		return 0;
	}

	/****** Proc filesystem entry points ****************************************/

	/*
	 *	Verify access rights.
	 */

	static int router_proc_perms (struct inode* inode, int op)
	{
		return 0;
	}

	/*
	 *	Read router proc directory entry.
	 *	This is universal routine for reading all entries in /proc/net/wanrouter
	 *	directory.  Each directory entry contains a pointer to the 'method' for
	 *	preparing data for that entry.
	 *	o verify arguments
	 *	o allocate kernel buffer
	 *	o call get_info() to prepare data
	 *	o copy data to user space
	 *	o release kernel buffer
	 *
	 *	Return:	number of bytes copied to user space (0, if no data)
	 *		<0	error
	 */
#ifdef LINUX_2_1
	static ssize_t router_proc_read(struct file* file, char* buf, size_t count,
					loff_t *ppos)
	{
		struct inode *inode = file->f_dentry->d_inode;
		struct proc_dir_entry* dent;
		char* page;
		int pos, offs, len;

		if (count <= 0)
			return 0;
			
		dent = inode->u.generic_ip;
		if ((dent == NULL) || (dent->get_info == NULL))
			return 0;
			
		page = kmalloc(PROC_BUFSZ, GFP_KERNEL);
		if (page == NULL)
			return -ENOBUFS;
			
		pos = dent->get_info(page, dent->data, 0, 0, 0);
		offs = file->f_pos;
		if (offs < pos) {
			len = min_t(unsigned int, pos - offs, count);
			if (copy_to_user(buf, (page + offs), len)) {
				kfree(page);
				return -EFAULT;
			}
			file->f_pos += len;
		}
		else
			len = 0;
		kfree(page);
		return len;
	}

#else
	static int router_proc_read(
		struct inode* inode, struct file* file, char* buf, int count)
	{
		struct proc_dir_entry* dent;
		char* page;
		int err, pos, offs, len;

		if (count <= 0)
			return 0;
		dent = inode->u.generic_ip;
		if ((dent == NULL) || (dent->get_info == NULL))
			return -ENODATA;
		err = verify_area(VERIFY_WRITE, buf, count);
		if (err) return err;

		page = kmalloc(PROC_BUFSZ, GFP_KERNEL);
		if (page == NULL)
			return -ENOMEM;

		pos = dent->get_info(page, dent->data, 0, 0, 0);
		offs = file->f_pos;
		if (offs < pos) {
			len = min_t(unsigned int, pos - offs, count);
			memcpy_tofs((void*)buf, (void*)(page + offs), len);
			file->f_pos += len;
		}
		else len = 0;
		kfree(page);
		return len;
	}
#endif


	/*
	 *	Prepare data for reading 'Config' entry.
	 *	Return length of data.
	 */

	static int config_get_info(char* buf, char** start, off_t offs, int len, 
		int dummy)
	{
		int cnt = sizeof(conf_hdr) - 1;
		wan_device_t* wandev;
		strcpy(buf, conf_hdr);
		for (wandev = router_devlist;
		     wandev && (cnt < (PROC_BUFSZ - 120));
		     wandev = wandev->next) {
			if (wandev->state) cnt += sprintf(&buf[cnt],
				"%-15s|0x%-4X|%3u|%3u| 0x%-8lX |0x%-6X|%7u|%7u|%7u|%7u\n",
				wandev->name,
				wandev->ioport,
				wandev->irq,
				wandev->dma,
				wandev->maddr,
				wandev->msize,
				wandev->hw_opt[0],
				wandev->hw_opt[1],
				wandev->hw_opt[2],
				wandev->hw_opt[3]);
		}

		return cnt;
	}

	/*
	 *	Prepare data for reading 'Status' entry.
	 *	Return length of data.
	 */

	static int status_get_info(char* buf, char** start, off_t offs, int len, 
				int dummy)
	{
		int cnt = 0;
		wan_device_t* wandev;

		//cnt += sprintf(&buf[cnt], "\nSTATUS:\n\n");
		strcpy(&buf[cnt], stat_hdr);
		cnt += sizeof(stat_hdr) - 1;

		for (wandev = router_devlist;
		     wandev && (cnt < (PROC_BUFSZ - 80));
		     wandev = wandev->next) {
			if (!wandev->state) continue;
			cnt += sprintf(&buf[cnt],
				"%-15s|%-8s|%-7s|%-9s|%-8s|%9u|%5u|%3u |",
				wandev->name,
				PROT_DECODE(wandev->config_id),
				wandev->config_id == WANCONFIG_FR ? 
					(wandev->station ? " Node" : " CPE") :
					(wandev->config_id == WANCONFIG_X25 ?
					(wandev->station ? " DCE" : " DTE") :
					(" N/A")),
				wandev->interface ? " V.35" : " RS-232",
				wandev->clocking ? "internal" : "external",
				wandev->bps,
				wandev->mtu,
				wandev->ndev);

			switch (wandev->state) {

			case WAN_UNCONFIGURED:
				cnt += sprintf(&buf[cnt], "%-12s\n", "unconfigured");
				break;

			case WAN_DISCONNECTED:
				cnt += sprintf(&buf[cnt], "%-12s\n", "disconnected");
				break;

			case WAN_CONNECTING:
				cnt += sprintf(&buf[cnt], "%-12s\n", "connecting");
				break;

			case WAN_CONNECTED:
				cnt += sprintf(&buf[cnt], "%-12s\n", "connected");
				break;

			case WAN_FT1_READY:
				cnt += sprintf(&buf[cnt], "%-12s\n", "ft1 ready");
				break;

			default:
				cnt += sprintf(&buf[cnt], "%-12s\n", "invalid");
				break;
			}
		}
		return cnt;
	}

	/*
	 *	Prepare data for reading <device> entry.
	 *	Return length of data.
	 *
	 *	On entry, the 'start' argument will contain a pointer to WAN device
	 *	data space.
	 */

	static int wandev_get_info(char* buf, char** start, off_t offs, int len, 
				int dummy)
	{
		wan_device_t* wandev = (void*)start;
		int cnt = 0;
		int rslt = 0;

		if ((wandev == NULL) || (wandev->magic != ROUTER_MAGIC))
			return 0;
		if (!wandev->state)
			return sprintf(&buf[cnt], "Device is not configured!\n");

		/* Update device statistics */
		if (wandev->update) {

			rslt = wandev->update(wandev);
			if(rslt) {
				switch (rslt) {
				case -EAGAIN:
					return sprintf(&buf[cnt], "Device is busy!\n");

				default:
					return sprintf(&buf[cnt],
						"Device is not configured!\n");
				}
			}
		}

		cnt += sprintf(&buf[cnt], PROC_STATS_FORMAT,
			"total packets received", wandev->stats.rx_packets);
		cnt += sprintf(&buf[cnt], PROC_STATS_FORMAT,
			"total packets transmitted", wandev->stats.tx_packets);
#ifdef LINUX_2_1
		cnt += sprintf(&buf[cnt], PROC_STATS_FORMAT,
			"total bytes received", wandev->stats.rx_bytes);
		cnt += sprintf(&buf[cnt], PROC_STATS_FORMAT,
			"total bytes transmitted", wandev->stats.tx_bytes);
#endif
		cnt += sprintf(&buf[cnt], PROC_STATS_FORMAT,
			"bad packets received", wandev->stats.rx_errors);
		cnt += sprintf(&buf[cnt], PROC_STATS_FORMAT,
			"packet transmit problems", wandev->stats.tx_errors);
		cnt += sprintf(&buf[cnt], PROC_STATS_FORMAT,
			"received frames dropped", wandev->stats.rx_dropped);
		cnt += sprintf(&buf[cnt], PROC_STATS_FORMAT,
			"transmit frames dropped", wandev->stats.tx_dropped);
		cnt += sprintf(&buf[cnt], PROC_STATS_FORMAT,
			"multicast packets received", wandev->stats.multicast);
		cnt += sprintf(&buf[cnt], PROC_STATS_FORMAT,
			"transmit collisions", wandev->stats.collisions);
		cnt += sprintf(&buf[cnt], PROC_STATS_FORMAT,
			"receive length errors", wandev->stats.rx_length_errors);
		cnt += sprintf(&buf[cnt], PROC_STATS_FORMAT,
			"receiver overrun errors", wandev->stats.rx_over_errors);
		cnt += sprintf(&buf[cnt], PROC_STATS_FORMAT,
			"CRC errors", wandev->stats.rx_crc_errors);
		cnt += sprintf(&buf[cnt], PROC_STATS_FORMAT,
			"frame format errors (aborts)", wandev->stats.rx_frame_errors);
		cnt += sprintf(&buf[cnt], PROC_STATS_FORMAT,
			"receiver fifo overrun", wandev->stats.rx_fifo_errors);
		cnt += sprintf(&buf[cnt], PROC_STATS_FORMAT,
			"receiver missed packet", wandev->stats.rx_missed_errors);
		cnt += sprintf(&buf[cnt], PROC_STATS_FORMAT,
			"aborted frames transmitted", wandev->stats.tx_aborted_errors);

		return cnt;
	}

#endif /* End of ifdef LINUX_2_4 */


#else

/*
 *	No /proc - output stubs
 */

int __init wanrouter_proc_init(void)
{
	return 0;
}

void wanrouter_proc_cleanup(void)
{
	return;
}

int wanrouter_proc_add(wan_device_t *wandev)
{
	return 0;
}

int wanrouter_proc_delete(wan_device_t *wandev)
{
	return 0;
}

#endif

/*============================================================================
 * Write WAN device ???.
 * o Find WAN device associated with this node
 */
#ifdef LINUX_2_0
static int device_write(
        struct inode* inode, struct file* file, const char* buf, int count)
{
        int err = verify_area(VERIFY_READ, buf, count);
        struct proc_dir_entry* dent;
        wan_device_t* wandev;

        if (err) return err;

        dent = inode->u.generic_ip;
        if ((dent == NULL) || (dent->data == NULL))
                return -ENODATA;

        wandev = dent->data;

        printk(KERN_ERR "%s: writing %d bytes to %s...\n",
                name_root, count, dent->name);
        
	return 0;
}
#endif

/*
 *	End
 */
 
