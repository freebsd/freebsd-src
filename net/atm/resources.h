/* net/atm/resources.h - ATM-related resources */

/* Written 1995-1998 by Werner Almesberger, EPFL LRC/ICA */


#ifndef NET_ATM_RESOURCES_H
#define NET_ATM_RESOURCES_H

#include <linux/config.h>
#include <linux/atmdev.h>


extern struct list_head atm_devs;
extern spinlock_t atm_dev_lock;


int atm_dev_ioctl(unsigned int cmd, unsigned long arg);


#ifdef CONFIG_PROC_FS

#include <linux/proc_fs.h>

int atm_proc_dev_register(struct atm_dev *dev);
void atm_proc_dev_deregister(struct atm_dev *dev);

#endif

#endif
