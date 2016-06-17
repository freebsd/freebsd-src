#ifndef _IF_LMC_LINUXVER_
#define _IF_LMC_LINUXVER_

 /*
  * Copyright (c) 1997-2000 LAN Media Corporation (LMC)
  * All rights reserved.  www.lanmedia.com
  *
  * This code is written by:
  * Andrew Stanley-Jones (asj@cban.com)
  * Rob Braun (bbraun@vix.com),
  * Michael Graff (explorer@vix.com) and
  * Matt Thomas (matt@3am-software.com).
  *
  * This software may be used and distributed according to the terms
  * of the GNU General Public License version 2, incorporated herein by reference.
  */

 /*
  * This file defines and controls all linux version
  * differences.
  *
  * This is being done to keep 1 central location where all linux
  * version differences can be kept and maintained.  as this code was
  * found version issues where pepered throughout the source code and
  * made the souce code not only hard to read but version problems hard
  * to track down.  If I'm overiding a function/etc with something in
  * this file it will be prefixed by "LMC_" which will mean look
  * here for the version dependant change that's been done.
  *
  */

#if LINUX_VERSION_CODE < 0x20363
#define net_device device
#endif

#if LINUX_VERSION_CODE < 0x20363
#define LMC_XMITTER_BUSY(x) (x)->tbusy = 1
#define LMC_XMITTER_FREE(x) (x)->tbusy = 0
#define LMC_XMITTER_INIT(x) (x)->tbusy = 0
#else
#define LMC_XMITTER_BUSY(x) netif_stop_queue(x)
#define LMC_XMITTER_FREE(x) netif_wake_queue(x)
#define LMC_XMITTER_INIT(x) netif_start_queue(x)

#endif


#if LINUX_VERSION_CODE < 0x20100
//typedef unsigned int u_int32_t;

#define  LMC_SETUP_20_DEV {\
                             int indx; \
                             for (indx = 0; indx < DEV_NUMBUFFS; indx++) \
                                skb_queue_head_init (&dev->buffs[indx]); \
                          } \
                          dev->family = AF_INET; \
                          dev->pa_addr = 0; \
                          dev->pa_brdaddr = 0; \
                          dev->pa_mask = 0xFCFFFFFF; \
                          dev->pa_alen = 4;		/* IP addr.  sizeof(u32) */

#else

#define LMC_SETUP_20_DEV

#endif


#if LINUX_VERSION_CODE < 0x20155 /* basically 2.2 plus */

#define LMC_DEV_KFREE_SKB(skb) dev_kfree_skb((skb), FREE_WRITE)
#define LMC_PCI_PRESENT() pcibios_present()

#else /* Mostly 2.0 kernels */

#define LMC_DEV_KFREE_SKB(skb) dev_kfree_skb(skb)
#define LMC_PCI_PRESENT() pci_present()

#endif

#if LINUX_VERSION_CODE < 0x20200
#else

#endif

#if LINUX_VERSION_CODE < 0x20100
#define LMC_SKB_FREE(skb, val) (skb->free = val)
#else
#define LMC_SKB_FREE(skb, val)
#endif


#if (LINUX_VERSION_CODE >= 0x20200)

#define LMC_SPIN_FLAGS                unsigned long flags;
#define LMC_SPIN_LOCK_INIT(x)         spin_lock_init(&(x)->lmc_lock);
#define LMC_SPIN_UNLOCK(x)            ((x)->lmc_lock = SPIN_LOCK_UNLOCKED)
#define LMC_SPIN_LOCK_IRQSAVE(x)      spin_lock_irqsave (&(x)->lmc_lock, flags);
#define LMC_SPIN_UNLOCK_IRQRESTORE(x) spin_unlock_irqrestore (&(x)->lmc_lock, flags);
#else
#define LMC_SPIN_FLAGS
#define LMC_SPIN_LOCK_INIT(x)
#define LMC_SPIN_UNLOCK(x)
#define LMC_SPIN_LOCK_IRQSAVE(x)
#define LMC_SPIN_UNLOCK_IRQRESTORE(x)
#endif


#if LINUX_VERSION_CODE >= 0x20100
#define LMC_COPY_FROM_USER(x, y, z) if(copy_from_user ((x), (y), (z))) return -EFAULT
#define LMC_COPY_TO_USER(x, y, z) if(copy_to_user ((x), (y), (z))) return -EFAULT
#else
#define LMC_COPY_FROM_USER(x, y, z) if(verify_area(VERIFY_READ, (y), (z))) \
			               return -EFAULT; \
                                    memcpy_fromfs ((x), (y), (z))

#define LMC_COPY_TO_USER(x, y, z)   if(verify_area(VERIFY_WRITE, (x), (z))) \
	                               return -EFAULT; \
                                    memcpy_tofs ((x), (y), (z))
#endif


#endif
