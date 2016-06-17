/* 
 * ioctl32.c: Conversion between 32bit and 64bit native ioctls.
 * 
 * Based on sparc64 ioctl32.c by:
 *
 * Copyright (C) 1997-2000  Jakub Jelinek  (jakub@redhat.com)
 * Copyright (C) 1998  Eddie C. Dost  (ecd@skynet.be)
 *
 * ppc64 changes:
 *
 * Copyright (C) 2000  Ken Aaker (kdaaker@rchland.vnet.ibm.com)
 * Copyright (C) 2001  Anton Blanchard (antonb@au.ibm.com)
 *
 * These routines maintain argument size conversion between 32bit and 64bit
 * ioctls.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/ioctl.h>
#include <linux/if.h>
#include <linux/slab.h>
#include <linux/hdreg.h>
#include <linux/raid/md.h>
#include <linux/kd.h>
#include <linux/route.h>
#include <linux/in6.h>
#include <linux/ipv6_route.h>
#include <linux/skbuff.h>
#include <linux/netlink.h>
#include <linux/vt.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/fd.h>
#include <linux/ppp_defs.h>
#include <linux/if_ppp.h>
#include <linux/if_pppox.h>
#include <linux/if_tun.h>
#include <linux/mtio.h>
#include <linux/cdrom.h>
#include <linux/loop.h>
#include <linux/auto_fs.h>
#include <linux/devfs_fs.h>
#include <linux/tty.h>
#include <linux/vt_kern.h>
#include <linux/fb.h>
#include <linux/ext2_fs.h>
#include <linux/videodev.h>
#include <linux/netdevice.h>
#include <linux/raw.h>
#include <linux/smb_fs.h>
#include <linux/blkpg.h>
#include <linux/blk.h>
#include <linux/elevator.h>
#include <linux/rtc.h>
#include <linux/pci.h>
#if defined(CONFIG_BLK_DEV_LVM) || defined(CONFIG_BLK_DEV_LVM_MODULE)
#include <linux/lvm.h>
#endif /* LVM */

#include <scsi/scsi.h>
/* Ugly hack. */
#undef __KERNEL__
#include <scsi/scsi_ioctl.h>
#define __KERNEL__
#include <scsi/sg.h>

#include <asm/types.h>
#include <asm/uaccess.h>
#include <linux/ethtool.h>
#include <linux/mii.h>
#include <linux/if_bonding.h>
#include <asm/module.h>
#include <linux/soundcard.h>
#include <linux/watchdog.h>
#include <linux/lp.h>

#include <linux/atm.h>
#include <linux/atmarp.h>
#include <linux/atmclip.h>
#include <linux/atmdev.h>
#include <linux/atmioc.h>
#include <linux/atmlec.h>
#include <linux/atmmpc.h>
#include <linux/atmsvc.h>
#include <linux/atm_tcp.h>
#include <linux/sonet.h>
#include <linux/atm_suni.h>
#include <linux/mtd/mtd.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci.h>

#include <linux/usb.h>
#include <linux/usbdevice_fs.h>
#include <linux/nbd.h>
#include <linux/random.h>
#include <asm/ppc32.h>
#include <asm/ppcdebug.h>

/* Aiee. Someone does not find a difference between int and long */
#define EXT2_IOC32_GETFLAGS               _IOR('f', 1, int)
#define EXT2_IOC32_SETFLAGS               _IOW('f', 2, int)
#define EXT2_IOC32_GETVERSION             _IOR('v', 1, int)
#define EXT2_IOC32_SETVERSION             _IOW('v', 2, int)

extern asmlinkage long sys_ioctl(unsigned int fd, unsigned int cmd, unsigned long arg);

static int w_long(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	mm_segment_t old_fs = get_fs();
	int err;
	unsigned long val;
	
	set_fs (KERNEL_DS);
	err = sys_ioctl(fd, cmd, (unsigned long)&val);
	set_fs (old_fs);
	if (!err && put_user(val, (u32 *)arg))
		return -EFAULT;
	return err;
}
 
static int rw_long(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	mm_segment_t old_fs = get_fs();
	int err;
	unsigned long val;
	
	if (get_user(val, (u32 *)arg))
		return -EFAULT;
	set_fs (KERNEL_DS);
	err = sys_ioctl(fd, cmd, (unsigned long)&val);
	set_fs (old_fs);
	if (!err && put_user(val, (u32 *)arg))
		return -EFAULT;
	return err;
}

static int do_ext2_ioctl(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	/* These are just misnamed, they actually get/put from/to user an int */
	switch (cmd) {
	case EXT2_IOC32_GETFLAGS: cmd = EXT2_IOC_GETFLAGS; break;
	case EXT2_IOC32_SETFLAGS: cmd = EXT2_IOC_SETFLAGS; break;
	case EXT2_IOC32_GETVERSION: cmd = EXT2_IOC_GETVERSION; break;
	case EXT2_IOC32_SETVERSION: cmd = EXT2_IOC_SETVERSION; break;
	}
	return sys_ioctl(fd, cmd, arg);
}
 
struct video_tuner32 {
	s32 tuner;
	u8 name[32];
	u32 rangelow, rangehigh;
	u32 flags;
	u16 mode, signal;
};

static int get_video_tuner32(struct video_tuner *kp, struct video_tuner32 *up)
{
	int i;

	if (get_user(kp->tuner, &up->tuner))
		return -EFAULT;
	for(i = 0; i < 32; i++)
		__get_user(kp->name[i], &up->name[i]);
	__get_user(kp->rangelow, &up->rangelow);
	__get_user(kp->rangehigh, &up->rangehigh);
	__get_user(kp->flags, &up->flags);
	__get_user(kp->mode, &up->mode);
	__get_user(kp->signal, &up->signal);
	return 0;
}

static int put_video_tuner32(struct video_tuner *kp, struct video_tuner32 *up)
{
	int i;

	if (put_user(kp->tuner, &up->tuner))
		return -EFAULT;
	for(i = 0; i < 32; i++)
		__put_user(kp->name[i], &up->name[i]);
	__put_user(kp->rangelow, &up->rangelow);
	__put_user(kp->rangehigh, &up->rangehigh);
	__put_user(kp->flags, &up->flags);
	__put_user(kp->mode, &up->mode);
	__put_user(kp->signal, &up->signal);
	return 0;
}

struct video_buffer32 {
	/* void * */ u32 base;
	s32 height, width, depth, bytesperline;
};

static int get_video_buffer32(struct video_buffer *kp, struct video_buffer32 *up)
{
	u32 tmp;

	if (get_user(tmp, &up->base))
		return -EFAULT;
	kp->base = (void *) ((unsigned long)tmp);
	__get_user(kp->height, &up->height);
	__get_user(kp->width, &up->width);
	__get_user(kp->depth, &up->depth);
	__get_user(kp->bytesperline, &up->bytesperline);
	return 0;
}

static int put_video_buffer32(struct video_buffer *kp, struct video_buffer32 *up)
{
	u32 tmp = (u32)((unsigned long)kp->base);

	if (put_user(tmp, &up->base))
		return -EFAULT;
	__put_user(kp->height, &up->height);
	__put_user(kp->width, &up->width);
	__put_user(kp->depth, &up->depth);
	__put_user(kp->bytesperline, &up->bytesperline);
	return 0;
}

struct video_clip32 {
	s32 x, y, width, height;
	/* struct video_clip32 * */ u32 next;
};

struct video_window32 {
	u32 x, y, width, height, chromakey, flags;
	/* struct video_clip32 * */ u32 clips;
	s32 clipcount;
};

static void free_kvideo_clips(struct video_window *kp)
{
	struct video_clip *cp;

	cp = kp->clips;
	if (cp != NULL)
		kfree(cp);
}

static int get_video_window32(struct video_window *kp, struct video_window32 *up)
{
	struct video_clip32 *ucp;
	struct video_clip *kcp;
	int nclips, err, i;
	u32 tmp;

	if (get_user(kp->x, &up->x))
		return -EFAULT;
	__get_user(kp->y, &up->y);
	__get_user(kp->width, &up->width);
	__get_user(kp->height, &up->height);
	__get_user(kp->chromakey, &up->chromakey);
	__get_user(kp->flags, &up->flags);
	__get_user(kp->clipcount, &up->clipcount);
	__get_user(tmp, &up->clips);
	ucp = (struct video_clip32 *)A(tmp);
	kp->clips = NULL;

	nclips = kp->clipcount;
	if (nclips == 0)
		return 0;

	if (ucp == 0)
		return -EINVAL;

	/* Peculiar interface... */
	if (nclips < 0)
		nclips = VIDEO_CLIPMAP_SIZE;

	kcp = kmalloc(nclips * sizeof(struct video_clip), GFP_KERNEL);
	err = -ENOMEM;
	if (kcp == NULL)
		goto cleanup_and_err;

	kp->clips = kcp;
	for(i = 0; i < nclips; i++) {
		__get_user(kcp[i].x, &ucp[i].x);
		__get_user(kcp[i].y, &ucp[i].y);
		__get_user(kcp[i].width, &ucp[i].width);
		__get_user(kcp[i].height, &ucp[i].height);
		kcp[nclips].next = NULL;
	}

	return 0;

cleanup_and_err:
	free_kvideo_clips(kp);
	return err;
}

/* You get back everything except the clips... */
static int put_video_window32(struct video_window *kp, struct video_window32 *up)
{
	if (put_user(kp->x, &up->x))
		return -EFAULT;
	__put_user(kp->y, &up->y);
	__put_user(kp->width, &up->width);
	__put_user(kp->height, &up->height);
	__put_user(kp->chromakey, &up->chromakey);
	__put_user(kp->flags, &up->flags);
	__put_user(kp->clipcount, &up->clipcount);
	return 0;
}

#define VIDIOCGTUNER32		_IOWR('v',4, struct video_tuner32)
#define VIDIOCSTUNER32		_IOW('v',5, struct video_tuner32)
#define VIDIOCGWIN32		_IOR('v',9, struct video_window32)
#define VIDIOCSWIN32		_IOW('v',10, struct video_window32)
#define VIDIOCGFBUF32		_IOR('v',11, struct video_buffer32)
#define VIDIOCSFBUF32		_IOW('v',12, struct video_buffer32)
#define VIDIOCGFREQ32		_IOR('v',14, u32)
#define VIDIOCSFREQ32		_IOW('v',15, u32)

static int do_video_ioctl(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	union {
		struct video_tuner vt;
		struct video_buffer vb;
		struct video_window vw;
		unsigned long vx;
	} karg;
	mm_segment_t old_fs = get_fs();
	void *up = (void *)arg;
	int err = 0;

	/* First, convert the command. */
	switch(cmd) {
	case VIDIOCGTUNER32: cmd = VIDIOCGTUNER; break;
	case VIDIOCSTUNER32: cmd = VIDIOCSTUNER; break;
	case VIDIOCGWIN32: cmd = VIDIOCGWIN; break;
	case VIDIOCSWIN32: cmd = VIDIOCSWIN; break;
	case VIDIOCGFBUF32: cmd = VIDIOCGFBUF; break;
	case VIDIOCSFBUF32: cmd = VIDIOCSFBUF; break;
	case VIDIOCGFREQ32: cmd = VIDIOCGFREQ; break;
	case VIDIOCSFREQ32: cmd = VIDIOCSFREQ; break;
	};

	switch(cmd) {
	case VIDIOCSTUNER:
	case VIDIOCGTUNER:
		err = get_video_tuner32(&karg.vt, up);
		break;

	case VIDIOCSWIN:
		err = get_video_window32(&karg.vw, up);
		break;

	case VIDIOCSFBUF:
		err = get_video_buffer32(&karg.vb, up);
		break;

	case VIDIOCSFREQ:
		err = get_user(karg.vx, (u32 *)up);
		break;
	};
	if (err)
		goto out;

	set_fs(KERNEL_DS);
	err = sys_ioctl(fd, cmd, (unsigned long)&karg);
	set_fs(old_fs);

	if (cmd == VIDIOCSWIN)
		free_kvideo_clips(&karg.vw);

	if (err == 0) {
		switch(cmd) {
		case VIDIOCGTUNER:
			err = put_video_tuner32(&karg.vt, up);
			break;

		case VIDIOCGWIN:
			err = put_video_window32(&karg.vw, up);
			break;

		case VIDIOCGFBUF:
			err = put_video_buffer32(&karg.vb, up);
			break;

		case VIDIOCGFREQ:
			err = put_user(((u32)karg.vx), (u32 *)up);
			break;
		};
	}
out:
	return err;
}

struct timeval32 {
	int tv_sec;
	int tv_usec;
};

static int do_siocgstamp(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	struct timeval32 *up = (struct timeval32 *)arg;
	struct timeval ktv;
	mm_segment_t old_fs = get_fs();
	int err;

	set_fs(KERNEL_DS);
	err = sys_ioctl(fd, cmd, (unsigned long)&ktv);
	set_fs(old_fs);
	if (!err) {
		err = put_user(ktv.tv_sec, &up->tv_sec);
		err |= __put_user(ktv.tv_usec, &up->tv_usec);
	}
	return err;
}

struct ifmap32 {
	u32 mem_start;
	u32 mem_end;
	unsigned short base_addr;
	unsigned char irq;
	unsigned char dma;
	unsigned char port;
};

struct ifreq32 {
#define IFHWADDRLEN     6
#define IFNAMSIZ        16
        union {
                char    ifrn_name[IFNAMSIZ];            /* if name, e.g. "en0" */
        } ifr_ifrn;
        union {
                struct  sockaddr ifru_addr;
                struct  sockaddr ifru_dstaddr;
                struct  sockaddr ifru_broadaddr;
                struct  sockaddr ifru_netmask;
                struct  sockaddr ifru_hwaddr;
                short   ifru_flags;
                int     ifru_ivalue;
                int     ifru_mtu;
                struct  ifmap32 ifru_map;
                char    ifru_slave[IFNAMSIZ];   /* Just fits the size */
		char	ifru_newname[IFNAMSIZ];
                __kernel_caddr_t32 ifru_data;
        } ifr_ifru;
};

struct ifconf32 {
        int     ifc_len;                        /* size of buffer       */
        __kernel_caddr_t32  ifcbuf;
};

#ifdef CONFIG_NET
static int dev_ifname32(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	struct net_device *dev;
	struct ifreq32 ifr32;
	int err;

	if (copy_from_user(&ifr32, (struct ifreq32 *)arg, sizeof(struct ifreq32)))
		return -EFAULT;

	dev = dev_get_by_index(ifr32.ifr_ifindex);
	if (!dev)
		return -ENODEV;

	strcpy(ifr32.ifr_name, dev->name);
	dev_put(dev);

	err = copy_to_user((struct ifreq32 *)arg, &ifr32, sizeof(struct ifreq32));
	return (err ? -EFAULT : 0);
}
#endif

static int dev_ifconf(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	struct ifconf32 ifc32;
	struct ifconf ifc;
	struct ifreq32 *ifr32;
	struct ifreq *ifr;
	mm_segment_t old_fs;
	unsigned int i, j;
	int err;

	if (copy_from_user(&ifc32, (struct ifconf32 *)arg, sizeof(struct ifconf32)))
		return -EFAULT;

	if (ifc32.ifcbuf == 0) {
		ifc32.ifc_len = 0;
		ifc.ifc_len = 0;
		ifc.ifc_buf = NULL;
	} else {
		ifc.ifc_len = ((ifc32.ifc_len / sizeof (struct ifreq32)) + 1) *
			sizeof (struct ifreq);
		ifc.ifc_buf = kmalloc (ifc.ifc_len, GFP_KERNEL);
		if (!ifc.ifc_buf)
			return -ENOMEM;
	}
	ifr = ifc.ifc_req;
	ifr32 = (struct ifreq32 *)A(ifc32.ifcbuf);
	for (i = 0; i < ifc32.ifc_len; i += sizeof (struct ifreq32)) {
		if (copy_from_user(ifr++, ifr32++, sizeof (struct ifreq32))) {
			kfree (ifc.ifc_buf);
			return -EFAULT;
		}
	}
	old_fs = get_fs(); set_fs (KERNEL_DS);
	err = sys_ioctl (fd, SIOCGIFCONF, (unsigned long)&ifc);	
	set_fs (old_fs);
	if (!err) {
		ifr = ifc.ifc_req;
		ifr32 = (struct ifreq32 *)A(ifc32.ifcbuf);
		for (i = 0, j = 0; i < ifc32.ifc_len && j < ifc.ifc_len;
		     i += sizeof (struct ifreq32), j += sizeof (struct ifreq)) {
			if (copy_to_user(ifr32++, ifr++, sizeof (struct ifreq32))) {
				err = -EFAULT;
				break;
			}
		}
		if (!err) {
			if (ifc32.ifcbuf == 0) {
				/* Translate from 64-bit structure multiple to
				 * a 32-bit one.
				 */
				i = ifc.ifc_len;
				i = ((i / sizeof(struct ifreq)) * sizeof(struct ifreq32));
				ifc32.ifc_len = i;
			} else {
				if (i <= ifc32.ifc_len)
					ifc32.ifc_len = i;
				else
					ifc32.ifc_len = i - sizeof (struct ifreq32);
			}
			if (copy_to_user((struct ifconf32 *)arg, &ifc32, sizeof(struct ifconf32)))
				err = -EFAULT;
		}
	}
	if (ifc.ifc_buf != NULL)
		kfree (ifc.ifc_buf);
	return err;
}

static int ethtool_ioctl(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	struct ifreq ifr;
	mm_segment_t old_fs;
	int err, len;
	u32 data, ethcmd;
	
	if (copy_from_user(&ifr, (struct ifreq32 *)arg, sizeof(struct ifreq32)))
		return -EFAULT;
	ifr.ifr_data = (__kernel_caddr_t)get_free_page(GFP_KERNEL);
	if (!ifr.ifr_data)
		return -EAGAIN;

	__get_user(data, &(((struct ifreq32 *)arg)->ifr_ifru.ifru_data));

	if (get_user(ethcmd, (u32 *)A(data))) {
		err = -EFAULT;
		goto out;
	}
	switch (ethcmd) {
	case ETHTOOL_GSTRINGS:	{
		struct ethtool_gstrings *stringsaddr = (struct ethtool_gstrings *)A(data);
		if (get_user(len, (u32 *)&stringsaddr->len)) {
			err = -EFAULT;
			goto out;
		}
		if (len > (PAGE_SIZE - sizeof(struct ethtool_gstrings))/ETH_GSTRING_LEN ) {
			err = -EINVAL;
			goto out;
		}
		len = (len*ETH_GSTRING_LEN) + sizeof(struct ethtool_gstrings);
		break;
	}
	case ETHTOOL_GSTATS:	{
		struct ethtool_stats *statsaddr = (struct ethtool_stats *)A(data);
		if (get_user(len, (u32 *)&statsaddr->n_stats)) {
			err = -EFAULT;
			goto out;
		}
		if (len > (PAGE_SIZE - sizeof(struct ethtool_stats))/sizeof(u64) ) {
			err = -EINVAL;
			goto out;
		}
		len = (len*sizeof(u64)) + sizeof(struct ethtool_stats);
		break;
	}
	case ETHTOOL_SWOL:
	case ETHTOOL_GWOL:
		len = sizeof(struct ethtool_wolinfo);
		break;
	case ETHTOOL_GDRVINFO:
		len = sizeof(struct ethtool_drvinfo);
		break;
	case ETHTOOL_GMSGLVL:
	case ETHTOOL_SMSGLVL:
	case ETHTOOL_GLINK:
	case ETHTOOL_NWAY_RST:
	case ETHTOOL_SSG:
	case ETHTOOL_GSG:
	case ETHTOOL_GTXCSUM:
	case ETHTOOL_STXCSUM:
	case ETHTOOL_GRXCSUM:
	case ETHTOOL_SRXCSUM:
	case ETHTOOL_PHYS_ID:
		len = sizeof(struct ethtool_value);
		break;
	case ETHTOOL_GREGS: {
		struct ethtool_regs *regaddr = (struct ethtool_regs *)A(data);
		/* darned variable size arguments */
		if (get_user(len, (u32 *)&regaddr->len)) {
			err = -EFAULT;
			goto out;
		}
		if (len > PAGE_SIZE - sizeof(struct ethtool_regs)) {
			err = -EINVAL;
			goto out;
		}
		len += sizeof(struct ethtool_regs);
		break;
	}
	case ETHTOOL_GEEPROM:
	case ETHTOOL_SEEPROM: {
		struct ethtool_eeprom *promaddr = (struct ethtool_eeprom *)A(data);
		/* darned variable size arguments */
		if (get_user(len, (u32 *)&promaddr->len)) {
			err = -EFAULT;
			goto out;
		}
		if (len > PAGE_SIZE - sizeof(struct ethtool_eeprom)) {
			err = -EINVAL;
			goto out;
		}
		len += sizeof(struct ethtool_eeprom);
		break;
	}
	case ETHTOOL_GRINGPARAM:
	case ETHTOOL_SRINGPARAM:
		len = sizeof(struct ethtool_ringparam);
		break;
	case ETHTOOL_GPAUSEPARAM:
	case ETHTOOL_SPAUSEPARAM:
		len = sizeof(struct ethtool_pauseparam);
		break;
	case ETHTOOL_GCOALESCE:
	case ETHTOOL_SCOALESCE:
		len = sizeof(struct ethtool_coalesce);
		break;
	case ETHTOOL_GSET:
	case ETHTOOL_SSET:
		len = sizeof(struct ethtool_cmd);
		break;
	default:
		err = -EOPNOTSUPP;
		goto out;
	}

	if (copy_from_user(ifr.ifr_data, (char *)A(data), len)) {
		err = -EFAULT;
		goto out;
	}

	old_fs = get_fs();
	set_fs (KERNEL_DS);
	err = sys_ioctl (fd, cmd, (unsigned long)&ifr);
	set_fs (old_fs);
	if (!err) {
		u32 data;

		__get_user(data, &(((struct ifreq32 *)arg)->ifr_ifru.ifru_data));
		len = copy_to_user((char *)A(data), ifr.ifr_data, len);
		if (len)
			err = -EFAULT;
	}

out:
	free_page((unsigned long)ifr.ifr_data);
	return err;
}

static int bond_ioctl(unsigned long fd, unsigned int cmd, unsigned long arg)
{
	struct ifreq ifr;
	mm_segment_t old_fs;
	int err, len;
	u32 data;
	
	if (copy_from_user(&ifr, (struct ifreq32 *)arg, sizeof(struct ifreq32)))
		return -EFAULT;
	ifr.ifr_data = (__kernel_caddr_t)get_free_page(GFP_KERNEL);
	if (!ifr.ifr_data)
		return -EAGAIN;

	switch (cmd) {
	case SIOCBONDENSLAVE:
	case SIOCBONDRELEASE:
	case SIOCBONDSETHWADDR:
	case SIOCBONDCHANGEACTIVE:
		len = IFNAMSIZ * sizeof(char);
		break;
	case SIOCBONDSLAVEINFOQUERY:
		len = sizeof(struct ifslave);
		break;
	case SIOCBONDINFOQUERY:
		len = sizeof(struct ifbond);
		break;
	default:
		err = -EINVAL;
		goto out;
	};

	__get_user(data, &(((struct ifreq32 *)arg)->ifr_ifru.ifru_data));
	if (copy_from_user(ifr.ifr_data, (char *)A(data), len)) {
		err = -EFAULT;
		goto out;
	}

	old_fs = get_fs();
	set_fs (KERNEL_DS);
	err = sys_ioctl (fd, cmd, (unsigned long)&ifr);
	set_fs (old_fs);
	if (!err) {
		len = copy_to_user((char *)A(data), ifr.ifr_data, len);
		if (len)
			err = -EFAULT;
	}

out:
	free_page((unsigned long)ifr.ifr_data);
	return err;
}

static int dev_ifsioc(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	struct ifreq ifr;
	mm_segment_t old_fs;
	int err;
	
	switch (cmd) {
	case SIOCSIFMAP:
		err = copy_from_user(&ifr, (struct ifreq32 *)arg, sizeof(ifr.ifr_name));
		err |= __get_user(ifr.ifr_map.mem_start, &(((struct ifreq32 *)arg)->ifr_ifru.ifru_map.mem_start));
		err |= __get_user(ifr.ifr_map.mem_end, &(((struct ifreq32 *)arg)->ifr_ifru.ifru_map.mem_end));
		err |= __get_user(ifr.ifr_map.base_addr, &(((struct ifreq32 *)arg)->ifr_ifru.ifru_map.base_addr));
		err |= __get_user(ifr.ifr_map.irq, &(((struct ifreq32 *)arg)->ifr_ifru.ifru_map.irq));
		err |= __get_user(ifr.ifr_map.dma, &(((struct ifreq32 *)arg)->ifr_ifru.ifru_map.dma));
		err |= __get_user(ifr.ifr_map.port, &(((struct ifreq32 *)arg)->ifr_ifru.ifru_map.port));
		if (err)
			return -EFAULT;
		break;
	default:
		if (copy_from_user(&ifr, (struct ifreq32 *)arg, sizeof(struct ifreq32)))
			return -EFAULT;
		break;
	}
	old_fs = get_fs();
	set_fs (KERNEL_DS);
	err = sys_ioctl (fd, cmd, (unsigned long)&ifr);
	set_fs (old_fs);
	if (!err) {
		switch (cmd) {
		case SIOCGIFFLAGS:
		case SIOCGIFMETRIC:
		case SIOCGIFMTU:
		case SIOCGIFMEM:
		case SIOCGIFHWADDR:
		case SIOCGIFINDEX:
		case SIOCGIFADDR:
		case SIOCGIFBRDADDR:
		case SIOCGIFDSTADDR:
		case SIOCGIFNETMASK:
		case SIOCGIFTXQLEN:
			if (copy_to_user((struct ifreq32 *)arg, &ifr, sizeof(struct ifreq32)))
				return -EFAULT;
			break;
		case SIOCGIFMAP:
			err = copy_to_user((struct ifreq32 *)arg, &ifr, sizeof(ifr.ifr_name));
			err |= __put_user(ifr.ifr_map.mem_start, &(((struct ifreq32 *)arg)->ifr_ifru.ifru_map.mem_start));
			err |= __put_user(ifr.ifr_map.mem_end, &(((struct ifreq32 *)arg)->ifr_ifru.ifru_map.mem_end));
			err |= __put_user(ifr.ifr_map.base_addr, &(((struct ifreq32 *)arg)->ifr_ifru.ifru_map.base_addr));
			err |= __put_user(ifr.ifr_map.irq, &(((struct ifreq32 *)arg)->ifr_ifru.ifru_map.irq));
			err |= __put_user(ifr.ifr_map.dma, &(((struct ifreq32 *)arg)->ifr_ifru.ifru_map.dma));
			err |= __put_user(ifr.ifr_map.port, &(((struct ifreq32 *)arg)->ifr_ifru.ifru_map.port));
			if (err)
				err = -EFAULT;
			break;
		}
	}
	return err;
}

struct rtentry32 {
        u32   		rt_pad1;
        struct sockaddr rt_dst;         /* target address               */
        struct sockaddr rt_gateway;     /* gateway addr (RTF_GATEWAY)   */
        struct sockaddr rt_genmask;     /* target network mask (IP)     */
        unsigned short  rt_flags;
        short           rt_pad2;
        u32   		rt_pad3;
        unsigned char   rt_tos;
        unsigned char   rt_class;
        short           rt_pad4;
        short           rt_metric;      /* +1 for binary compatibility! */
        /* char * */ u32 rt_dev;        /* forcing the device at add    */
        u32   		rt_mtu;         /* per route MTU/Window         */
        u32   		rt_window;      /* Window clamping              */
        unsigned short  rt_irtt;        /* Initial RTT                  */

};

struct in6_rtmsg32 {
	struct in6_addr		rtmsg_dst;
	struct in6_addr		rtmsg_src;
	struct in6_addr		rtmsg_gateway;
	u32			rtmsg_type;
	u16			rtmsg_dst_len;
	u16			rtmsg_src_len;
	u32			rtmsg_metric;
	u32			rtmsg_info;
	u32			rtmsg_flags;
	s32			rtmsg_ifindex;
};

extern struct socket *sockfd_lookup(int fd, int *err);

extern __inline__ void sockfd_put(struct socket *sock)
{
	fput(sock->file);
}

static int routing_ioctl(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	int ret;
	void *r = NULL;
	struct in6_rtmsg r6;
	struct rtentry r4;
	char devname[16];
	u32 rtdev;
	mm_segment_t old_fs = get_fs();
	
	struct socket *mysock = sockfd_lookup(fd, &ret);

	if (mysock && mysock->sk && mysock->sk->family == AF_INET6) { /* ipv6 */
		ret = copy_from_user (&r6.rtmsg_dst, &(((struct in6_rtmsg32 *)arg)->rtmsg_dst),
			3 * sizeof(struct in6_addr));
		ret |= __get_user (r6.rtmsg_type, &(((struct in6_rtmsg32 *)arg)->rtmsg_type));
		ret |= __get_user (r6.rtmsg_dst_len, &(((struct in6_rtmsg32 *)arg)->rtmsg_dst_len));
		ret |= __get_user (r6.rtmsg_src_len, &(((struct in6_rtmsg32 *)arg)->rtmsg_src_len));
		ret |= __get_user (r6.rtmsg_metric, &(((struct in6_rtmsg32 *)arg)->rtmsg_metric));
		ret |= __get_user (r6.rtmsg_info, &(((struct in6_rtmsg32 *)arg)->rtmsg_info));
		ret |= __get_user (r6.rtmsg_flags, &(((struct in6_rtmsg32 *)arg)->rtmsg_flags));
		ret |= __get_user (r6.rtmsg_ifindex, &(((struct in6_rtmsg32 *)arg)->rtmsg_ifindex));
		
		r = (void *) &r6;
	} else { /* ipv4 */
		ret = copy_from_user (&r4.rt_dst, &(((struct rtentry32 *)arg)->rt_dst), 3 * sizeof(struct sockaddr));
		ret |= __get_user (r4.rt_flags, &(((struct rtentry32 *)arg)->rt_flags));
		ret |= __get_user (r4.rt_metric, &(((struct rtentry32 *)arg)->rt_metric));
		ret |= __get_user (r4.rt_mtu, &(((struct rtentry32 *)arg)->rt_mtu));
		ret |= __get_user (r4.rt_window, &(((struct rtentry32 *)arg)->rt_window));
		ret |= __get_user (r4.rt_irtt, &(((struct rtentry32 *)arg)->rt_irtt));
		ret |= __get_user (rtdev, &(((struct rtentry32 *)arg)->rt_dev));
		if (rtdev) {
			ret |= copy_from_user (devname, (char *)A(rtdev), 15);
			r4.rt_dev = devname; devname[15] = 0;
		} else
			r4.rt_dev = 0;

		r = (void *) &r4;
	}

	if (ret)
		return -EFAULT;

	set_fs (KERNEL_DS);
	ret = sys_ioctl (fd, cmd, (long) r);
	set_fs (old_fs);

	if (mysock)
		sockfd_put(mysock);

	return ret;
}

struct hd_geometry32 {
	unsigned char heads;
	unsigned char sectors;
	unsigned short cylinders;
	u32 start;
};
                        
static int hdio_getgeo(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	mm_segment_t old_fs = get_fs();
	struct hd_geometry geo;
	int err;
	
	set_fs (KERNEL_DS);
	err = sys_ioctl(fd, HDIO_GETGEO, (unsigned long)&geo);
	set_fs (old_fs);
	if (!err) {
		err = copy_to_user ((struct hd_geometry32 *)arg, &geo, 4);
		err |= __put_user (geo.start, &(((struct hd_geometry32 *)arg)->start));
	}
	return err ? -EFAULT : 0;
}

struct hd_big_geometry32 {
	u8 heads;
	u8 sectors;
	u32 cylinders;
	u32 start;
};

static int hdio_getgeo_big(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	mm_segment_t old_fs = get_fs();
	struct hd_big_geometry geo;
	int err;
	
	set_fs (KERNEL_DS);
	err = sys_ioctl(fd, HDIO_GETGEO_BIG, (unsigned long)&geo);
	set_fs (old_fs);
	if (err)
		return err;
	else {
		struct hd_big_geometry32 *user_geo = (struct hd_big_geometry32 *)arg;
		err = __put_user (geo.heads, &(user_geo->heads));
		err |= __put_user (geo.sectors, &(user_geo->sectors));
		err |= __put_user (geo.cylinders, &(user_geo->cylinders));
		err |= __put_user (geo.start, &(user_geo->start));
	}
	return err ? -EFAULT : 0;
}


static int hdio_ioctl_trans(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	mm_segment_t old_fs = get_fs();
	unsigned long kval;
	unsigned int *uvp;
	int error;

	set_fs(KERNEL_DS);
	error = sys_ioctl(fd, cmd, (long)&kval);
	set_fs(old_fs);

	if (error == 0) {
		uvp = (unsigned int *)arg;
		if (put_user(kval, uvp))
			error = -EFAULT;
	}
	return error;
}

struct floppy_struct32 {
	unsigned int	size;
	unsigned int	sect;
	unsigned int	head;
	unsigned int	track;
	unsigned int	stretch;
	unsigned char	gap;
	unsigned char	rate;
	unsigned char	spec1;
	unsigned char	fmt_gap;
	const __kernel_caddr_t32 name;
};

struct floppy_drive_params32 {
	char		cmos;
	u32		max_dtr;
	u32		hlt;
	u32		hut;
	u32		srt;
	u32		spinup;
	u32		spindown;
	unsigned char	spindown_offset;
	unsigned char	select_delay;
	unsigned char	rps;
	unsigned char	tracks;
	u32		timeout;
	unsigned char	interleave_sect;
	struct floppy_max_errors max_errors;
	char		flags;
	char		read_track;
	short		autodetect[8];
	int		checkfreq;
	int		native_format;
};

struct floppy_drive_struct32 {
	signed char	flags;
	u32		spinup_date;
	u32		select_date;
	u32		first_read_date;
	short		probed_format;
	short		track;
	short		maxblock;
	short		maxtrack;
	int		generation;
	int		keep_data;
	int		fd_ref;
	int		fd_device;
	int		last_checked;
	__kernel_caddr_t32 dmabuf;
	int		bufblocks;
};

struct floppy_fdc_state32 {
	int		spec1;
	int		spec2;
	int		dtr;
	unsigned char	version;
	unsigned char	dor;
	u32		address;
	unsigned int	rawcmd:2;
	unsigned int	reset:1;
	unsigned int	need_configure:1;
	unsigned int	perp_mode:2;
	unsigned int	has_fifo:1;
	unsigned int	driver_version;
	unsigned char	track[4];
};

struct floppy_write_errors32 {
	unsigned int	write_errors;
	u32		first_error_sector;
	int		first_error_generation;
	u32		last_error_sector;
	int		last_error_generation;
	unsigned int	badness;
};

#define FDSETPRM32 _IOW(2, 0x42, struct floppy_struct32)
#define FDDEFPRM32 _IOW(2, 0x43, struct floppy_struct32)
#define FDGETPRM32 _IOR(2, 0x04, struct floppy_struct32)
#define FDSETDRVPRM32 _IOW(2, 0x90, struct floppy_drive_params32)
#define FDGETDRVPRM32 _IOR(2, 0x11, struct floppy_drive_params32)
#define FDGETDRVSTAT32 _IOR(2, 0x12, struct floppy_drive_struct32)
#define FDPOLLDRVSTAT32 _IOR(2, 0x13, struct floppy_drive_struct32)
#define FDGETFDCSTAT32 _IOR(2, 0x15, struct floppy_fdc_state32)
#define FDWERRORGET32  _IOR(2, 0x17, struct floppy_write_errors32)

static struct {
	unsigned int	cmd32;
	unsigned int	cmd;
} fd_ioctl_trans_table[] = {
	{ FDSETPRM32, FDSETPRM },
	{ FDDEFPRM32, FDDEFPRM },
	{ FDGETPRM32, FDGETPRM },
	{ FDSETDRVPRM32, FDSETDRVPRM },
	{ FDGETDRVPRM32, FDGETDRVPRM },
	{ FDGETDRVSTAT32, FDGETDRVSTAT },
	{ FDPOLLDRVSTAT32, FDPOLLDRVSTAT },
	{ FDGETFDCSTAT32, FDGETFDCSTAT },
	{ FDWERRORGET32, FDWERRORGET }
};

#define NR_FD_IOCTL_TRANS (sizeof(fd_ioctl_trans_table)/sizeof(fd_ioctl_trans_table[0]))

static int fd_ioctl_trans(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	mm_segment_t old_fs = get_fs();
	void *karg = NULL;
	unsigned int kcmd = 0;
	int i, err;

	for (i = 0; i < NR_FD_IOCTL_TRANS; i++)
		if (cmd == fd_ioctl_trans_table[i].cmd32) {
			kcmd = fd_ioctl_trans_table[i].cmd;
			break;
		}
	if (!kcmd)
		return -EINVAL;

	switch (cmd) {
		case FDSETPRM32:
		case FDDEFPRM32:
		case FDGETPRM32:
		{
			struct floppy_struct *f;

			f = karg = kmalloc(sizeof(struct floppy_struct), GFP_KERNEL);
			if (!karg)
				return -ENOMEM;
			if (cmd == FDGETPRM32)
				break;
			err = __get_user(f->size, &((struct floppy_struct32 *)arg)->size);
			err |= __get_user(f->sect, &((struct floppy_struct32 *)arg)->sect);
			err |= __get_user(f->head, &((struct floppy_struct32 *)arg)->head);
			err |= __get_user(f->track, &((struct floppy_struct32 *)arg)->track);
			err |= __get_user(f->stretch, &((struct floppy_struct32 *)arg)->stretch);
			err |= __get_user(f->gap, &((struct floppy_struct32 *)arg)->gap);
			err |= __get_user(f->rate, &((struct floppy_struct32 *)arg)->rate);
			err |= __get_user(f->spec1, &((struct floppy_struct32 *)arg)->spec1);
			err |= __get_user(f->fmt_gap, &((struct floppy_struct32 *)arg)->fmt_gap);
			err |= __get_user((u64)f->name, &((struct floppy_struct32 *)arg)->name);
			if (err) {
				err = -EFAULT;
				goto out;
			}
			break;
		}
		case FDSETDRVPRM32:
		case FDGETDRVPRM32:
		{
			struct floppy_drive_params *f;

			f = karg = kmalloc(sizeof(struct floppy_drive_params), GFP_KERNEL);
			if (!karg)
				return -ENOMEM;
			if (cmd == FDGETDRVPRM32)
				break;
			err = __get_user(f->cmos, &((struct floppy_drive_params32 *)arg)->cmos);
			err |= __get_user(f->max_dtr, &((struct floppy_drive_params32 *)arg)->max_dtr);
			err |= __get_user(f->hlt, &((struct floppy_drive_params32 *)arg)->hlt);
			err |= __get_user(f->hut, &((struct floppy_drive_params32 *)arg)->hut);
			err |= __get_user(f->srt, &((struct floppy_drive_params32 *)arg)->srt);
			err |= __get_user(f->spinup, &((struct floppy_drive_params32 *)arg)->spinup);
			err |= __get_user(f->spindown, &((struct floppy_drive_params32 *)arg)->spindown);
			err |= __get_user(f->spindown_offset, &((struct floppy_drive_params32 *)arg)->spindown_offset);
			err |= __get_user(f->select_delay, &((struct floppy_drive_params32 *)arg)->select_delay);
			err |= __get_user(f->rps, &((struct floppy_drive_params32 *)arg)->rps);
			err |= __get_user(f->tracks, &((struct floppy_drive_params32 *)arg)->tracks);
			err |= __get_user(f->timeout, &((struct floppy_drive_params32 *)arg)->timeout);
			err |= __get_user(f->interleave_sect, &((struct floppy_drive_params32 *)arg)->interleave_sect);
			err |= __copy_from_user(&f->max_errors, &((struct floppy_drive_params32 *)arg)->max_errors, sizeof(f->max_errors));
			err |= __get_user(f->flags, &((struct floppy_drive_params32 *)arg)->flags);
			err |= __get_user(f->read_track, &((struct floppy_drive_params32 *)arg)->read_track);
			err |= __copy_from_user(f->autodetect, ((struct floppy_drive_params32 *)arg)->autodetect, sizeof(f->autodetect));
			err |= __get_user(f->checkfreq, &((struct floppy_drive_params32 *)arg)->checkfreq);
			err |= __get_user(f->native_format, &((struct floppy_drive_params32 *)arg)->native_format);
			if (err) {
				err = -EFAULT;
				goto out;
			}
			break;
		}
		case FDGETDRVSTAT32:
		case FDPOLLDRVSTAT32:
			karg = kmalloc(sizeof(struct floppy_drive_struct), GFP_KERNEL);
			if (!karg)
				return -ENOMEM;
			break;
		case FDGETFDCSTAT32:
			karg = kmalloc(sizeof(struct floppy_fdc_state), GFP_KERNEL);
			if (!karg)
				return -ENOMEM;
			break;
		case FDWERRORGET32:
			karg = kmalloc(sizeof(struct floppy_write_errors), GFP_KERNEL);
			if (!karg)
				return -ENOMEM;
			break;
		default:
			return -EINVAL;
	}
	set_fs (KERNEL_DS);
	err = sys_ioctl (fd, kcmd, (unsigned long)karg);
	set_fs (old_fs);
	if (err)
		goto out;
	switch (cmd) {
		case FDGETPRM32:
		{
			struct floppy_struct *f = karg;

			err = __put_user(f->size, &((struct floppy_struct32 *)arg)->size);
			err |= __put_user(f->sect, &((struct floppy_struct32 *)arg)->sect);
			err |= __put_user(f->head, &((struct floppy_struct32 *)arg)->head);
			err |= __put_user(f->track, &((struct floppy_struct32 *)arg)->track);
			err |= __put_user(f->stretch, &((struct floppy_struct32 *)arg)->stretch);
			err |= __put_user(f->gap, &((struct floppy_struct32 *)arg)->gap);
			err |= __put_user(f->rate, &((struct floppy_struct32 *)arg)->rate);
			err |= __put_user(f->spec1, &((struct floppy_struct32 *)arg)->spec1);
			err |= __put_user(f->fmt_gap, &((struct floppy_struct32 *)arg)->fmt_gap);
			err |= __put_user((u64)f->name, &((struct floppy_struct32 *)arg)->name);
			break;
		}
		case FDGETDRVPRM32:
		{
			struct floppy_drive_params *f = karg;

			err = __put_user(f->cmos, &((struct floppy_drive_params32 *)arg)->cmos);
			err |= __put_user(f->max_dtr, &((struct floppy_drive_params32 *)arg)->max_dtr);
			err |= __put_user(f->hlt, &((struct floppy_drive_params32 *)arg)->hlt);
			err |= __put_user(f->hut, &((struct floppy_drive_params32 *)arg)->hut);
			err |= __put_user(f->srt, &((struct floppy_drive_params32 *)arg)->srt);
			err |= __put_user(f->spinup, &((struct floppy_drive_params32 *)arg)->spinup);
			err |= __put_user(f->spindown, &((struct floppy_drive_params32 *)arg)->spindown);
			err |= __put_user(f->spindown_offset, &((struct floppy_drive_params32 *)arg)->spindown_offset);
			err |= __put_user(f->select_delay, &((struct floppy_drive_params32 *)arg)->select_delay);
			err |= __put_user(f->rps, &((struct floppy_drive_params32 *)arg)->rps);
			err |= __put_user(f->tracks, &((struct floppy_drive_params32 *)arg)->tracks);
			err |= __put_user(f->timeout, &((struct floppy_drive_params32 *)arg)->timeout);
			err |= __put_user(f->interleave_sect, &((struct floppy_drive_params32 *)arg)->interleave_sect);
			err |= __copy_to_user(&((struct floppy_drive_params32 *)arg)->max_errors, &f->max_errors, sizeof(f->max_errors));
			err |= __put_user(f->flags, &((struct floppy_drive_params32 *)arg)->flags);
			err |= __put_user(f->read_track, &((struct floppy_drive_params32 *)arg)->read_track);
			err |= __copy_to_user(((struct floppy_drive_params32 *)arg)->autodetect, f->autodetect, sizeof(f->autodetect));
			err |= __put_user(f->checkfreq, &((struct floppy_drive_params32 *)arg)->checkfreq);
			err |= __put_user(f->native_format, &((struct floppy_drive_params32 *)arg)->native_format);
			break;
		}
		case FDGETDRVSTAT32:
		case FDPOLLDRVSTAT32:
		{
			struct floppy_drive_struct *f = karg;

			err = __put_user(f->flags, &((struct floppy_drive_struct32 *)arg)->flags);
			err |= __put_user(f->spinup_date, &((struct floppy_drive_struct32 *)arg)->spinup_date);
			err |= __put_user(f->select_date, &((struct floppy_drive_struct32 *)arg)->select_date);
			err |= __put_user(f->first_read_date, &((struct floppy_drive_struct32 *)arg)->first_read_date);
			err |= __put_user(f->probed_format, &((struct floppy_drive_struct32 *)arg)->probed_format);
			err |= __put_user(f->track, &((struct floppy_drive_struct32 *)arg)->track);
			err |= __put_user(f->maxblock, &((struct floppy_drive_struct32 *)arg)->maxblock);
			err |= __put_user(f->maxtrack, &((struct floppy_drive_struct32 *)arg)->maxtrack);
			err |= __put_user(f->generation, &((struct floppy_drive_struct32 *)arg)->generation);
			err |= __put_user(f->keep_data, &((struct floppy_drive_struct32 *)arg)->keep_data);
			err |= __put_user(f->fd_ref, &((struct floppy_drive_struct32 *)arg)->fd_ref);
			err |= __put_user(f->fd_device, &((struct floppy_drive_struct32 *)arg)->fd_device);
			err |= __put_user(f->last_checked, &((struct floppy_drive_struct32 *)arg)->last_checked);
			err |= __put_user((u64)f->dmabuf, &((struct floppy_drive_struct32 *)arg)->dmabuf);
			err |= __put_user((u64)f->bufblocks, &((struct floppy_drive_struct32 *)arg)->bufblocks);
			break;
		}
		case FDGETFDCSTAT32:
		{
			struct floppy_fdc_state *f = karg;

			err = __put_user(f->spec1, &((struct floppy_fdc_state32 *)arg)->spec1);
			err |= __put_user(f->spec2, &((struct floppy_fdc_state32 *)arg)->spec2);
			err |= __put_user(f->dtr, &((struct floppy_fdc_state32 *)arg)->dtr);
			err |= __put_user(f->version, &((struct floppy_fdc_state32 *)arg)->version);
			err |= __put_user(f->dor, &((struct floppy_fdc_state32 *)arg)->dor);
			err |= __put_user(f->address, &((struct floppy_fdc_state32 *)arg)->address);
			err |= __copy_to_user((char *)&((struct floppy_fdc_state32 *)arg)->address
			    		   + sizeof(((struct floppy_fdc_state32 *)arg)->address),
					   (char *)&f->address + sizeof(f->address), sizeof(int));
			err |= __put_user(f->driver_version, &((struct floppy_fdc_state32 *)arg)->driver_version);
			err |= __copy_to_user(((struct floppy_fdc_state32 *)arg)->track, f->track, sizeof(f->track));
			break;
		}
		case FDWERRORGET32:
		{
			struct floppy_write_errors *f = karg;

			err = __put_user(f->write_errors, &((struct floppy_write_errors32 *)arg)->write_errors);
			err |= __put_user(f->first_error_sector, &((struct floppy_write_errors32 *)arg)->first_error_sector);
			err |= __put_user(f->first_error_generation, &((struct floppy_write_errors32 *)arg)->first_error_generation);
			err |= __put_user(f->last_error_sector, &((struct floppy_write_errors32 *)arg)->last_error_sector);
			err |= __put_user(f->last_error_generation, &((struct floppy_write_errors32 *)arg)->last_error_generation);
			err |= __put_user(f->badness, &((struct floppy_write_errors32 *)arg)->badness);
			break;
		}
		default:
			break;
	}
	if (err)
		err = -EFAULT;

out:	if (karg) kfree(karg);
	return err;
}

typedef struct sg_io_hdr32 {
	s32 interface_id;	/* [i] 'S' for SCSI generic (required) */
	s32 dxfer_direction;	/* [i] data transfer direction  */
	u8  cmd_len;		/* [i] SCSI command length ( <= 16 bytes) */
	u8  mx_sb_len;		/* [i] max length to write to sbp */
	u16 iovec_count;	/* [i] 0 implies no scatter gather */
	u32 dxfer_len;		/* [i] byte count of data transfer */
	u32 dxferp;		/* [i], [*io] points to data transfer memory
					      or scatter gather list */
	u32 cmdp;		/* [i], [*i] points to command to perform */
	u32 sbp;		/* [i], [*o] points to sense_buffer memory */
	u32 timeout;		/* [i] MAX_UINT->no timeout (unit: millisec) */
	u32 flags;		/* [i] 0 -> default, see SG_FLAG... */
	s32 pack_id;		/* [i->o] unused internally (normally) */
	u32 usr_ptr;		/* [i->o] unused internally */
	u8  status;		/* [o] scsi status */
	u8  masked_status;	/* [o] shifted, masked scsi status */
	u8  msg_status;		/* [o] messaging level data (optional) */
	u8  sb_len_wr;		/* [o] byte count actually written to sbp */
	u16 host_status;	/* [o] errors from host adapter */
	u16 driver_status;	/* [o] errors from software driver */
	s32 resid;		/* [o] dxfer_len - actual_transferred */
	u32 duration;		/* [o] time taken by cmd (unit: millisec) */
	u32 info;		/* [o] auxiliary information */
} sg_io_hdr32_t;  /* 64 bytes long (on sparc32) */

typedef struct sg_iovec32 {
	u32 iov_base;
	u32 iov_len;
} sg_iovec32_t;

#define EMU_SG_MAX 128

static int alloc_sg_iovec(sg_io_hdr_t *sgp, u32 uptr32)
{
	sg_iovec32_t *uiov = (sg_iovec32_t *) A(uptr32);
	sg_iovec_t *kiov;
	int i;

	if (sgp->iovec_count > EMU_SG_MAX)
		return -EINVAL;
	sgp->dxferp = kmalloc(sgp->iovec_count *
			      sizeof(sg_iovec_t), GFP_KERNEL);
	if (!sgp->dxferp)
		return -ENOMEM;
	memset(sgp->dxferp, 0,
	       sgp->iovec_count * sizeof(sg_iovec_t));

	kiov = (sg_iovec_t *) sgp->dxferp;
	for (i = 0; i < sgp->iovec_count; i++) {
		u32 iov_base32;
		if (__get_user(iov_base32, &uiov->iov_base) ||
		    __get_user(kiov->iov_len, &uiov->iov_len))
			return -EFAULT;
		if (verify_area(VERIFY_WRITE, (void *)A(iov_base32), kiov->iov_len))
			return -EFAULT;
		kiov->iov_base = (void *)A(iov_base32);
		uiov++;
		kiov++;
	}

	return 0;
}

static void free_sg_iovec(sg_io_hdr_t *sgp)
{
	kfree(sgp->dxferp);
	sgp->dxferp = NULL;
}

static int sg_ioctl_trans(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	sg_io_hdr32_t *sg_io32;
	sg_io_hdr_t sg_io64;
	u32 dxferp32, cmdp32, sbp32;
	mm_segment_t old_fs;
	int err = 0;

	sg_io32 = (sg_io_hdr32_t *)arg;
	err = __get_user(sg_io64.interface_id, &sg_io32->interface_id);
	err |= __get_user(sg_io64.dxfer_direction, &sg_io32->dxfer_direction);
	err |= __get_user(sg_io64.cmd_len, &sg_io32->cmd_len);
	err |= __get_user(sg_io64.mx_sb_len, &sg_io32->mx_sb_len);
	err |= __get_user(sg_io64.iovec_count, &sg_io32->iovec_count);
	err |= __get_user(sg_io64.dxfer_len, &sg_io32->dxfer_len);
	err |= __get_user(sg_io64.timeout, &sg_io32->timeout);
	err |= __get_user(sg_io64.flags, &sg_io32->flags);
	err |= __get_user(sg_io64.pack_id, &sg_io32->pack_id);

	sg_io64.dxferp = NULL;
	sg_io64.cmdp = NULL;
	sg_io64.sbp = NULL;

	err |= __get_user(cmdp32, &sg_io32->cmdp);
	if (sg_io64.cmd_len > 4*PAGE_SIZE || sg_io64.mx_sb_len > 4*PAGE_SIZE) {
		err = -EINVAL;
		goto out;
	}
	sg_io64.cmdp = kmalloc(sg_io64.cmd_len, GFP_KERNEL);
	if (!sg_io64.cmdp) {
		err = -ENOMEM;
		goto out;
	}
	if (copy_from_user(sg_io64.cmdp,
			   (void *) A(cmdp32),
			   sg_io64.cmd_len)) {
		err = -EFAULT;
		goto out;
	}

	err |= __get_user(sbp32, &sg_io32->sbp);
	sg_io64.sbp = kmalloc(sg_io64.mx_sb_len, GFP_KERNEL);
	if (!sg_io64.sbp) {
		err = -ENOMEM;
		goto out;
	}
	if (copy_from_user(sg_io64.sbp,
			   (void *) A(sbp32),
			   sg_io64.mx_sb_len)) {
		err = -EFAULT;
		goto out;
	}

	err |= __get_user(dxferp32, &sg_io32->dxferp);
	if (sg_io64.iovec_count) {
		int ret;

		if ((ret = alloc_sg_iovec(&sg_io64, dxferp32))) {
			err = ret;
			goto out;
		}
	} else {
		if (sg_io64.dxfer_len > 4*PAGE_SIZE) {
			err = -EINVAL;
			goto out;
		}
		sg_io64.dxferp = kmalloc(sg_io64.dxfer_len, GFP_KERNEL);
		if (!sg_io64.dxferp) {
			err = -ENOMEM;
			goto out;
		}
		if (copy_from_user(sg_io64.dxferp,
				   (void *) A(dxferp32),
				   sg_io64.dxfer_len)) {
			err = -EFAULT;
			goto out;
		}
	}

	/* Unused internally, do not even bother to copy it over. */
	sg_io64.usr_ptr = NULL;

	if (err)
		return -EFAULT;

	old_fs = get_fs();
	set_fs (KERNEL_DS);
	err = sys_ioctl (fd, cmd, (unsigned long) &sg_io64);
	set_fs (old_fs);

	if (err < 0)
		goto out;

	err = __put_user(sg_io64.pack_id, &sg_io32->pack_id);
	err |= __put_user(sg_io64.status, &sg_io32->status);
	err |= __put_user(sg_io64.masked_status, &sg_io32->masked_status);
	err |= __put_user(sg_io64.msg_status, &sg_io32->msg_status);
	err |= __put_user(sg_io64.sb_len_wr, &sg_io32->sb_len_wr);
	err |= __put_user(sg_io64.host_status, &sg_io32->host_status);
	err |= __put_user(sg_io64.driver_status, &sg_io32->driver_status);
	err |= __put_user(sg_io64.resid, &sg_io32->resid);
	err |= __put_user(sg_io64.duration, &sg_io32->duration);
	err |= __put_user(sg_io64.info, &sg_io32->info);
	err |= copy_to_user((void *)A(sbp32), sg_io64.sbp, sg_io64.mx_sb_len);
	if (sg_io64.dxferp) {
		if (sg_io64.iovec_count)
			;
		else
			err |= copy_to_user((void *)A(dxferp32),
					    sg_io64.dxferp,
					    sg_io64.dxfer_len);
	}
	if (err)
		err = -EFAULT;

out:
	if (sg_io64.cmdp)
		kfree(sg_io64.cmdp);
	if (sg_io64.sbp)
		kfree(sg_io64.sbp);
	if (sg_io64.dxferp) {
		if (sg_io64.iovec_count) {
			free_sg_iovec(&sg_io64);
		} else {
			kfree(sg_io64.dxferp);
		}
	}
	return err;
}

struct ppp_option_data32 {
	__kernel_caddr_t32	ptr;
	__u32			length;
	int			transmit;
};
#define PPPIOCSCOMPRESS32	_IOW('t', 77, struct ppp_option_data32)

struct ppp_idle32 {
	__kernel_time_t32 xmit_idle;
	__kernel_time_t32 recv_idle;
};
#define PPPIOCGIDLE32		_IOR('t', 63, struct ppp_idle32)

static int ppp_ioctl_trans(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	mm_segment_t old_fs = get_fs();
	struct ppp_option_data32 data32;
	struct ppp_option_data data;
	struct ppp_idle32 idle32;
	struct ppp_idle idle;
	unsigned int kcmd;
	void *karg;
	int err = 0;

	switch (cmd) {
	case PPPIOCGIDLE32:
		kcmd = PPPIOCGIDLE;
		karg = &idle;
		break;
	case PPPIOCSCOMPRESS32:
		if (copy_from_user(&data32, (struct ppp_option_data32 *)arg, sizeof(struct ppp_option_data32)))
			return -EFAULT;
		if (data32.length > PAGE_SIZE)
			return -EINVAL;
		data.ptr = kmalloc (data32.length, GFP_KERNEL);
		if (!data.ptr)
			return -ENOMEM;
		if (copy_from_user(data.ptr, (__u8 *)A(data32.ptr), data32.length)) {
			kfree(data.ptr);
			return -EFAULT;
		}
		data.length = data32.length;
		data.transmit = data32.transmit;
		kcmd = PPPIOCSCOMPRESS;
		karg = &data;
		break;
	default:
		do {
			static int count = 0;
			if (++count <= 20)
				printk("ppp_ioctl: Unknown cmd fd(%d) "
				       "cmd(%08x) arg(%08x)\n",
				       (int)fd, (unsigned int)cmd, (unsigned int)arg);
		} while (0);
		return -EINVAL;
	}
	set_fs (KERNEL_DS);
	err = sys_ioctl (fd, kcmd, (unsigned long)karg);
	set_fs (old_fs);
	switch (cmd) {
	case PPPIOCGIDLE32:
		if (err)
			return err;
		idle32.xmit_idle = idle.xmit_idle;
		idle32.recv_idle = idle.recv_idle;
		if (copy_to_user((struct ppp_idle32 *)arg, &idle32, sizeof(struct ppp_idle32)))
			return -EFAULT;
		break;
	case PPPIOCSCOMPRESS32:
		kfree(data.ptr);
		break;
	default:
		break;
	}
	return err;
}


struct mtget32 {
	__u32	mt_type;
	__u32	mt_resid;
	__u32	mt_dsreg;
	__u32	mt_gstat;
	__u32	mt_erreg;
	__kernel_daddr_t32	mt_fileno;
	__kernel_daddr_t32	mt_blkno;
};
#define MTIOCGET32	_IOR('m', 2, struct mtget32)

struct mtpos32 {
	__u32	mt_blkno;
};
#define MTIOCPOS32	_IOR('m', 3, struct mtpos32)

struct mtconfiginfo32 {
	__u32	mt_type;
	__u32	ifc_type;
	__u16	irqnr;
	__u16	dmanr;
	__u16	port;
	__u32	debug;
	__u32	have_dens:1;
	__u32	have_bsf:1;
	__u32	have_fsr:1;
	__u32	have_bsr:1;
	__u32	have_eod:1;
	__u32	have_seek:1;
	__u32	have_tell:1;
	__u32	have_ras1:1;
	__u32	have_ras2:1;
	__u32	have_ras3:1;
	__u32	have_qfa:1;
	__u32	pad1:5;
	char	reserved[10];
};
#define	MTIOCGETCONFIG32	_IOR('m', 4, struct mtconfiginfo32)
#define	MTIOCSETCONFIG32	_IOW('m', 5, struct mtconfiginfo32)

static int mt_ioctl_trans(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	mm_segment_t old_fs = get_fs();
	struct mtconfiginfo info;
	struct mtget get;
	struct mtpos pos;
	unsigned long kcmd;
	void *karg;
	int err = 0;

	switch(cmd) {
	case MTIOCPOS32:
		kcmd = MTIOCPOS;
		karg = &pos;
		break;
	case MTIOCGET32:
		kcmd = MTIOCGET;
		karg = &get;
		break;
	case MTIOCGETCONFIG32:
		kcmd = MTIOCGETCONFIG;
		karg = &info;
		break;
	case MTIOCSETCONFIG32:
		kcmd = MTIOCSETCONFIG;
		karg = &info;
		err = __get_user(info.mt_type, &((struct mtconfiginfo32 *)arg)->mt_type);
		err |= __get_user(info.ifc_type, &((struct mtconfiginfo32 *)arg)->ifc_type);
		err |= __get_user(info.irqnr, &((struct mtconfiginfo32 *)arg)->irqnr);
		err |= __get_user(info.dmanr, &((struct mtconfiginfo32 *)arg)->dmanr);
		err |= __get_user(info.port, &((struct mtconfiginfo32 *)arg)->port);
		err |= __get_user(info.debug, &((struct mtconfiginfo32 *)arg)->debug);
		err |= __copy_from_user((char *)&info.debug + sizeof(info.debug),
				     (char *)&((struct mtconfiginfo32 *)arg)->debug
				     + sizeof(((struct mtconfiginfo32 *)arg)->debug), sizeof(__u32));
		if (err)
			return -EFAULT;
		break;
	default:
		do {
			static int count = 0;
			if (++count <= 20)
				printk("mt_ioctl: Unknown cmd fd(%d) "
				       "cmd(%08x) arg(%08x)\n",
				       (int)fd, (unsigned int)cmd, (unsigned int)arg);
		} while (0);
		return -EINVAL;
	}
	set_fs (KERNEL_DS);
	err = sys_ioctl (fd, kcmd, (unsigned long)karg);
	set_fs (old_fs);
	if (err)
		return err;
	switch (cmd) {
	case MTIOCPOS32:
		err = __put_user(pos.mt_blkno, &((struct mtpos32 *)arg)->mt_blkno);
		break;
	case MTIOCGET32:
		err = __put_user(get.mt_type, &((struct mtget32 *)arg)->mt_type);
		err |= __put_user(get.mt_resid, &((struct mtget32 *)arg)->mt_resid);
		err |= __put_user(get.mt_dsreg, &((struct mtget32 *)arg)->mt_dsreg);
		err |= __put_user(get.mt_gstat, &((struct mtget32 *)arg)->mt_gstat);
		err |= __put_user(get.mt_erreg, &((struct mtget32 *)arg)->mt_erreg);
		err |= __put_user(get.mt_fileno, &((struct mtget32 *)arg)->mt_fileno);
		err |= __put_user(get.mt_blkno, &((struct mtget32 *)arg)->mt_blkno);
		break;
	case MTIOCGETCONFIG32:
		err = __put_user(info.mt_type, &((struct mtconfiginfo32 *)arg)->mt_type);
		err |= __put_user(info.ifc_type, &((struct mtconfiginfo32 *)arg)->ifc_type);
		err |= __put_user(info.irqnr, &((struct mtconfiginfo32 *)arg)->irqnr);
		err |= __put_user(info.dmanr, &((struct mtconfiginfo32 *)arg)->dmanr);
		err |= __put_user(info.port, &((struct mtconfiginfo32 *)arg)->port);
		err |= __put_user(info.debug, &((struct mtconfiginfo32 *)arg)->debug);
		err |= __copy_to_user((char *)&((struct mtconfiginfo32 *)arg)->debug
			    		   + sizeof(((struct mtconfiginfo32 *)arg)->debug),
					   (char *)&info.debug + sizeof(info.debug), sizeof(__u32));
		break;
	case MTIOCSETCONFIG32:
		break;
	}
	return err ? -EFAULT: 0;
}

struct cdrom_read_audio32 {
	union cdrom_addr	addr;
	u_char			addr_format;
	int			nframes;
	__kernel_caddr_t32	buf;
};

struct cdrom_generic_command32 {
	unsigned char		cmd[CDROM_PACKET_SIZE];
	__kernel_caddr_t32	buffer;
	unsigned int		buflen;
	int			stat;
	__kernel_caddr_t32	sense;
	__kernel_caddr_t32	reserved[3];
};

static int cdrom_ioctl_trans(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	mm_segment_t old_fs = get_fs();
	struct cdrom_read_audio cdreadaudio;
	struct cdrom_generic_command cgc;
	__kernel_caddr_t32 addr;
	char *data = 0;
	void *karg;
	int err = 0;

	switch(cmd) {
	case CDROMREADAUDIO:
		karg = &cdreadaudio;
		err = copy_from_user(&cdreadaudio.addr, &((struct cdrom_read_audio32 *)arg)->addr, sizeof(cdreadaudio.addr));
		err |= __get_user(cdreadaudio.addr_format, &((struct cdrom_read_audio32 *)arg)->addr_format);
		err |= __get_user(cdreadaudio.nframes, &((struct cdrom_read_audio32 *)arg)->nframes); 
		err |= __get_user(addr, &((struct cdrom_read_audio32 *)arg)->buf);
		if (err)
			return -EFAULT;
		if (verify_area(VERIFY_WRITE, (void *)A(addr), cdreadaudio.nframes*2352))
			return -EFAULT;
		cdreadaudio.buf = (void *)A(addr);
		break;
	case CDROM_SEND_PACKET:
		karg = &cgc;
		err = copy_from_user(cgc.cmd, &((struct cdrom_generic_command32 *)arg)->cmd, sizeof(cgc.cmd));
		err |= __get_user(addr, &((struct cdrom_generic_command32 *)arg)->buffer);
		err |= __get_user(cgc.buflen, &((struct cdrom_generic_command32 *)arg)->buflen);
		if (err)
			return -EFAULT;
		if (verify_area(VERIFY_WRITE, (void *)A(addr), cgc.buflen))
			return -EFAULT;
		cgc.buffer = (void *)A(addr);
		break;
	default:
		do {
			static int count = 0;
			if (++count <= 20)
				printk("cdrom_ioctl: Unknown cmd fd(%d) "
				       "cmd(%08x) arg(%08x)\n",
				       (int)fd, (unsigned int)cmd, (unsigned int)arg);
		} while (0);
		return -EINVAL;
	}
	set_fs (KERNEL_DS);
	err = sys_ioctl (fd, cmd, (unsigned long)karg);
	set_fs (old_fs);
out:	if (data)
		kfree(data);
	return err ? -EFAULT : 0;
}

struct loop_info32 {
	int			lo_number;      /* ioctl r/o */
	__kernel_dev_t32	lo_device;      /* ioctl r/o */
	unsigned int		lo_inode;       /* ioctl r/o */
	__kernel_dev_t32	lo_rdevice;     /* ioctl r/o */
	int			lo_offset;
	int			lo_encrypt_type;
	int			lo_encrypt_key_size;    /* ioctl w/o */
	int			lo_flags;       /* ioctl r/o */
	char			lo_name[LO_NAME_SIZE];
	unsigned char		lo_encrypt_key[LO_KEY_SIZE]; /* ioctl w/o */
	unsigned int		lo_init[2];
	char			reserved[4];
};

static int loop_status(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	mm_segment_t old_fs = get_fs();
	struct loop_info l;
	int err = -EINVAL;

	switch(cmd) {
	case LOOP_SET_STATUS:
		err = get_user(l.lo_number, &((struct loop_info32 *)arg)->lo_number);
		err |= __get_user(l.lo_device, &((struct loop_info32 *)arg)->lo_device);
		err |= __get_user(l.lo_inode, &((struct loop_info32 *)arg)->lo_inode);
		err |= __get_user(l.lo_rdevice, &((struct loop_info32 *)arg)->lo_rdevice);
		err |= __copy_from_user((char *)&l.lo_offset, (char *)&((struct loop_info32 *)arg)->lo_offset,
					   8 + (unsigned long)l.lo_init - (unsigned long)&l.lo_offset);
		if (err) {
			err = -EFAULT;
		} else {
			set_fs (KERNEL_DS);
			err = sys_ioctl (fd, cmd, (unsigned long)&l);
			set_fs (old_fs);
		}
		break;
	case LOOP_GET_STATUS:
		set_fs (KERNEL_DS);
		err = sys_ioctl (fd, cmd, (unsigned long)&l);
		set_fs (old_fs);
		if (!err) {
			err = put_user(l.lo_number, &((struct loop_info32 *)arg)->lo_number);
			err |= __put_user(l.lo_device, &((struct loop_info32 *)arg)->lo_device);
			err |= __put_user(l.lo_inode, &((struct loop_info32 *)arg)->lo_inode);
			err |= __put_user(l.lo_rdevice, &((struct loop_info32 *)arg)->lo_rdevice);
			err |= __copy_to_user((char *)&((struct loop_info32 *)arg)->lo_offset,
					   (char *)&l.lo_offset, (unsigned long)l.lo_init - (unsigned long)&l.lo_offset);
			if (err)
				err = -EFAULT;
		}
		break;
	default: {
		static int count = 0;
		if (++count <= 20)
			printk("%s: Unknown loop ioctl cmd, fd(%d) "
			       "cmd(%08x) arg(%08lx)\n",
			       __FUNCTION__, fd, cmd, arg);
	}
	}
	return err;
}

extern int tty_ioctl(struct inode * inode, struct file * file, unsigned int cmd, unsigned long arg);

#ifdef CONFIG_VT
static int vt_check(struct file *file)
{
	struct tty_struct *tty;
	struct inode *inode = file->f_dentry->d_inode;
	
	if (file->f_op->ioctl != tty_ioctl)
		return -EINVAL;
	                
	tty = (struct tty_struct *)file->private_data;
	if (tty_paranoia_check(tty, inode->i_rdev, "tty_ioctl"))
		return -EINVAL;
	                                                
	if (tty->driver.ioctl != vt_ioctl)
		return -EINVAL;
	
	/*
	 * To have permissions to do most of the vt ioctls, we either have
	 * to be the owner of the tty, or super-user.
	 */
	if (current->tty == tty || suser())
		return 1;
	return 0;                                                    
}

struct consolefontdesc32 {
	unsigned short charcount;       /* characters in font (256 or 512) */
	unsigned short charheight;      /* scan lines per character (1-32) */
	u32 chardata;			/* font data in expanded form */
};

static int do_fontx_ioctl(unsigned int fd, int cmd, struct consolefontdesc32 *user_cfd, struct file *file)
{
	struct consolefontdesc cfdarg;
	struct console_font_op op;
	int i, perm;

	perm = vt_check(file);
	if (perm < 0) return perm;
	
	if (copy_from_user(&cfdarg, user_cfd, sizeof(struct consolefontdesc32)))
		return -EFAULT;
	
	cfdarg.chardata = (unsigned char *)A(((struct consolefontdesc32 *)&cfdarg)->chardata);
 	
	switch (cmd) {
	case PIO_FONTX:
		if (!perm)
			return -EPERM;
		op.op = KD_FONT_OP_SET;
		op.flags = 0;
		op.width = 8;
		op.height = cfdarg.charheight;
		op.charcount = cfdarg.charcount;
		op.data = cfdarg.chardata;
		return con_font_op(fg_console, &op);
	case GIO_FONTX:
		if (!cfdarg.chardata)
			return 0;
		op.op = KD_FONT_OP_GET;
		op.flags = 0;
		op.width = 8;
		op.height = cfdarg.charheight;
		op.charcount = cfdarg.charcount;
		op.data = cfdarg.chardata;
		i = con_font_op(fg_console, &op);
		if (i)
			return i;
		cfdarg.charheight = op.height;
		cfdarg.charcount = op.charcount;
		((struct consolefontdesc32 *)&cfdarg)->chardata	= (unsigned long)cfdarg.chardata;
		if (copy_to_user(user_cfd, &cfdarg, sizeof(struct consolefontdesc32)))
			return -EFAULT;
		return 0;
	}
	return -EINVAL;
}

struct console_font_op32 {
	unsigned int op;        /* operation code KD_FONT_OP_* */
	unsigned int flags;     /* KD_FONT_FLAG_* */
	unsigned int width, height;     /* font size */
	unsigned int charcount;
	u32 data;    /* font data with height fixed to 32 */
};
                                        
static int do_kdfontop_ioctl(unsigned int fd, unsigned int cmd, struct console_font_op32 *fontop, struct file *file)
{
	struct console_font_op op;
	int perm = vt_check(file), i;
	struct vt_struct *vt;
	
	if (perm < 0) return perm;
	
	if (copy_from_user(&op, (void *) fontop, sizeof(struct console_font_op32)))
		return -EFAULT;
	if (!perm && op.op != KD_FONT_OP_GET)
		return -EPERM;
	op.data = (unsigned char *)A(((struct console_font_op32 *)&op)->data);
	op.flags |= KD_FONT_FLAG_OLD;
	vt = (struct vt_struct *)((struct tty_struct *)file->private_data)->driver_data;
	i = con_font_op(vt->vc_num, &op);
	if (i) return i;
	((struct console_font_op32 *)&op)->data = (unsigned long)op.data;
	if (copy_to_user((void *) fontop, &op, sizeof(struct console_font_op32)))
		return -EFAULT;
	return 0;
}

struct fb_fix_screeninfo32 {
	char id[16];			/* identification string eg "TT Builtin" */
	unsigned int smem_start;	/* Start of frame buffer mem */
					/* (physical address) */
	__u32 smem_len;			/* Length of frame buffer mem */
	__u32 type;			/* see FB_TYPE_*		*/
	__u32 type_aux;			/* Interleave for interleaved Planes */
	__u32 visual;			/* see FB_VISUAL_*		*/ 
	__u16 xpanstep;			/* zero if no hardware panning  */
	__u16 ypanstep;			/* zero if no hardware panning  */
	__u16 ywrapstep;		/* zero if no hardware ywrap    */
	__u32 line_length;		/* length of a line in bytes    */
	unsigned int mmio_start;	/* Start of Memory Mapped I/O   */
					/* (physical address) */
	__u32 mmio_len;			/* Length of Memory Mapped I/O  */
	__u32 accel;			/* Type of acceleration available */
	__u16 reserved[3];		/* Reserved for future compatibility */
};

static int do_fbioget_fscreeninfo_ioctl(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	mm_segment_t old_fs = get_fs();
	struct fb_fix_screeninfo fix;
	int err;

	set_fs(KERNEL_DS);
	err = sys_ioctl(fd, cmd, (long)&fix);
	set_fs(old_fs);

	if (err == 0) {
		unsigned int smem_start = fix.smem_start;  /* lose top 32 bits */
		unsigned int mmio_start = fix.mmio_start;  /* lose top 32 bits */
		int i;

		err = put_user(fix.id[0], &((struct fb_fix_screeninfo32 *)arg)->id[0]);
		for (i=1; i<16; i++) {
			err |= __put_user(fix.id[i], &((struct fb_fix_screeninfo32 *)arg)->id[i]);
		}
		err |= __put_user(smem_start, &((struct fb_fix_screeninfo32 *)arg)->smem_start);
		err |= __put_user(fix.smem_len, &((struct fb_fix_screeninfo32 *)arg)->smem_len);
		err |= __put_user(fix.type, &((struct fb_fix_screeninfo32 *)arg)->type);
		err |= __put_user(fix.type_aux, &((struct fb_fix_screeninfo32 *)arg)->type_aux);
		err |= __put_user(fix.visual, &((struct fb_fix_screeninfo32 *)arg)->visual);
		err |= __put_user(fix.xpanstep, &((struct fb_fix_screeninfo32 *)arg)->xpanstep);
		err |= __put_user(fix.ypanstep, &((struct fb_fix_screeninfo32 *)arg)->ypanstep);
		err |= __put_user(fix.ywrapstep, &((struct fb_fix_screeninfo32 *)arg)->ywrapstep);
		err |= __put_user(fix.line_length, &((struct fb_fix_screeninfo32 *)arg)->line_length);
		err |= __put_user(mmio_start, &((struct fb_fix_screeninfo32 *)arg)->mmio_start);
		err |= __put_user(fix.mmio_len, &((struct fb_fix_screeninfo32 *)arg)->mmio_len);
		err |= __put_user(fix.accel, &((struct fb_fix_screeninfo32 *)arg)->accel);
		err |= __put_user(fix.reserved[0], &((struct fb_fix_screeninfo32 *)arg)->reserved[0]);
		err |= __put_user(fix.reserved[1], &((struct fb_fix_screeninfo32 *)arg)->reserved[1]);
		err |= __put_user(fix.reserved[2], &((struct fb_fix_screeninfo32 *)arg)->reserved[2]);
		if (err)
			err = -EFAULT;
	}
	return err;
}

struct fb_cmap32 {
	__u32 start;			/* First entry	*/
	__u32 len;			/* Number of entries */
	__u32 redptr;			/* Red values	*/
	__u32 greenptr;
	__u32 blueptr;
	__u32 transpptr;		/* transparency, can be NULL */
};

static int do_fbiogetcmap_ioctl(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	mm_segment_t old_fs = get_fs();
	struct fb_cmap cmap;
	int err;

	set_fs(KERNEL_DS);
	err = sys_ioctl(fd, cmd, (long)&cmap);
	set_fs(old_fs);

	if (err == 0) {
		__u32 redptr = (__u32)(__u64)cmap.red;
		__u32 greenptr = (__u32)(__u64)cmap.green;
		__u32 blueptr = (__u32)(__u64)cmap.blue;
		__u32 transpptr = (__u32)(__u64)cmap.transp;

		err = put_user(cmap.start, &((struct fb_cmap32 *)arg)->start);
		err |= __put_user(cmap.len, &((struct fb_cmap32 *)arg)->len);
		err |= __put_user(redptr, &((struct fb_cmap32 *)arg)->redptr);
		err |= __put_user(greenptr, &((struct fb_cmap32 *)arg)->greenptr);
		err |= __put_user(blueptr, &((struct fb_cmap32 *)arg)->blueptr);
		err |= __put_user(transpptr, &((struct fb_cmap32 *)arg)->transpptr);
		if (err)
			err = -EFAULT;
	}
	return err;
}

static int do_fbioputcmap_ioctl(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	mm_segment_t old_fs = get_fs();
	struct fb_cmap cmap;
	__u32 redptr, greenptr, blueptr, transpptr;
	int err;

	err = get_user(cmap.start, &((struct fb_cmap32 *)arg)->start);
	err |= __get_user(cmap.len, &((struct fb_cmap32 *)arg)->len);
	err |= __get_user(redptr, &((struct fb_cmap32 *)arg)->redptr);
	err |= __get_user(greenptr, &((struct fb_cmap32 *)arg)->greenptr);
	err |= __get_user(blueptr, &((struct fb_cmap32 *)arg)->blueptr);
	err |= __get_user(transpptr, &((struct fb_cmap32 *)arg)->transpptr);

	if (err) {
		err = -EFAULT;
	} else {
		cmap.red = (__u16 *)(__u64)redptr;
		cmap.green = (__u16 *)(__u64)greenptr;
		cmap.blue = (__u16 *)(__u64)blueptr;
		cmap.transp = (__u16 *)(__u64)transpptr;
		set_fs (KERNEL_DS);
		err = sys_ioctl (fd, cmd, (unsigned long)&cmap);
		set_fs (old_fs);
	}
	return err;
}

struct unimapdesc32 {
	unsigned short entry_ct;
	u32 entries;
};

static int do_unimap_ioctl(unsigned int fd, unsigned int cmd, struct unimapdesc32 *user_ud, struct file *file)
{
	struct unimapdesc32 tmp;
	int perm = vt_check(file);
	
	if (perm < 0) return perm;
	if (copy_from_user(&tmp, user_ud, sizeof tmp))
		return -EFAULT;
	switch (cmd) {
	case PIO_UNIMAP:
		if (!perm) return -EPERM;
		return con_set_unimap(fg_console, tmp.entry_ct, (struct unipair *)A(tmp.entries));
	case GIO_UNIMAP:
		return con_get_unimap(fg_console, tmp.entry_ct, &(user_ud->entry_ct), (struct unipair *)A(tmp.entries));
	}
	return 0;
}
#endif /* CONFIG_VT */
static int do_smb_getmountuid(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	mm_segment_t old_fs = get_fs();
	__kernel_uid_t kuid;
	int err;

	cmd = SMB_IOC_GETMOUNTUID;

	set_fs(KERNEL_DS);
	err = sys_ioctl(fd, cmd, (unsigned long)&kuid);
	set_fs(old_fs);

	if (err >= 0)
		err = put_user(kuid, (__kernel_uid_t32 *)arg);

	return err;
}

struct atmif_sioc32 {
        int                number;
        int                length;
        __kernel_caddr_t32 arg;
};

struct atm_iobuf32 {
	int                length;
	__kernel_caddr_t32 buffer;
};

#define ATM_GETLINKRATE32 _IOW('a', ATMIOC_ITF+1, struct atmif_sioc32)
#define ATM_GETNAMES32    _IOW('a', ATMIOC_ITF+3, struct atm_iobuf32)
#define ATM_GETTYPE32     _IOW('a', ATMIOC_ITF+4, struct atmif_sioc32)
#define ATM_GETESI32	  _IOW('a', ATMIOC_ITF+5, struct atmif_sioc32)
#define ATM_GETADDR32	  _IOW('a', ATMIOC_ITF+6, struct atmif_sioc32)
#define ATM_RSTADDR32	  _IOW('a', ATMIOC_ITF+7, struct atmif_sioc32)
#define ATM_ADDADDR32	  _IOW('a', ATMIOC_ITF+8, struct atmif_sioc32)
#define ATM_DELADDR32	  _IOW('a', ATMIOC_ITF+9, struct atmif_sioc32)
#define ATM_GETCIRANGE32  _IOW('a', ATMIOC_ITF+10, struct atmif_sioc32)
#define ATM_SETCIRANGE32  _IOW('a', ATMIOC_ITF+11, struct atmif_sioc32)
#define ATM_SETESI32      _IOW('a', ATMIOC_ITF+12, struct atmif_sioc32)
#define ATM_SETESIF32     _IOW('a', ATMIOC_ITF+13, struct atmif_sioc32)
#define ATM_GETSTAT32     _IOW('a', ATMIOC_SARCOM+0, struct atmif_sioc32)
#define ATM_GETSTATZ32    _IOW('a', ATMIOC_SARCOM+1, struct atmif_sioc32)
#define ATM_GETLOOP32	  _IOW('a', ATMIOC_SARCOM+2, struct atmif_sioc32)
#define ATM_SETLOOP32	  _IOW('a', ATMIOC_SARCOM+3, struct atmif_sioc32)
#define ATM_QUERYLOOP32	  _IOW('a', ATMIOC_SARCOM+4, struct atmif_sioc32)

static struct {
        unsigned int cmd32;
        unsigned int cmd;
} atm_ioctl_map[] = {
        { ATM_GETLINKRATE32, ATM_GETLINKRATE },
	{ ATM_GETNAMES32,    ATM_GETNAMES },
        { ATM_GETTYPE32,     ATM_GETTYPE },
        { ATM_GETESI32,      ATM_GETESI },
        { ATM_GETADDR32,     ATM_GETADDR },
        { ATM_RSTADDR32,     ATM_RSTADDR },
        { ATM_ADDADDR32,     ATM_ADDADDR },
        { ATM_DELADDR32,     ATM_DELADDR },
        { ATM_GETCIRANGE32,  ATM_GETCIRANGE },
	{ ATM_SETCIRANGE32,  ATM_SETCIRANGE },
	{ ATM_SETESI32,      ATM_SETESI },
	{ ATM_SETESIF32,     ATM_SETESIF },
	{ ATM_GETSTAT32,     ATM_GETSTAT },
	{ ATM_GETSTATZ32,    ATM_GETSTATZ },
	{ ATM_GETLOOP32,     ATM_GETLOOP },
	{ ATM_SETLOOP32,     ATM_SETLOOP },
	{ ATM_QUERYLOOP32,   ATM_QUERYLOOP }
};

#define NR_ATM_IOCTL (sizeof(atm_ioctl_map)/sizeof(atm_ioctl_map[0]))


static int do_atm_iobuf(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	struct atm_iobuf32 iobuf32;
	struct atm_iobuf   iobuf = { 0, NULL };
	mm_segment_t old_fs;
	int err;

	err = copy_from_user(&iobuf32, (struct atm_iobuf32*)arg,
	    sizeof(struct atm_iobuf32));
	if (err)
		return -EFAULT;

	iobuf.length = iobuf32.length;

	if (iobuf32.buffer == (__kernel_caddr_t32) NULL || iobuf32.length == 0) {
		iobuf.buffer = (void*)(unsigned long)iobuf32.buffer;
	} else {
		iobuf.buffer = A(iobuf32.buffer);
		if (verify_area(VERIFY_WRITE, iobuf.buffer, iobuf.length))
			return -EINVAL;
	}

	old_fs = get_fs(); set_fs (KERNEL_DS);
	err = sys_ioctl (fd, cmd, (unsigned long)&iobuf);
	set_fs (old_fs);
	if (!err)
		err = __put_user(iobuf.length, &(((struct atm_iobuf32*)arg)->length));

	return err;
}


static int do_atmif_sioc(unsigned int fd, unsigned int cmd, unsigned long arg)
{
        struct atmif_sioc32 sioc32;
        struct atmif_sioc   sioc = { 0, 0, NULL };
        mm_segment_t old_fs;
        int err;
        
        err = copy_from_user(&sioc32, (struct atmif_sioc32*)arg,
			     sizeof(struct atmif_sioc32));
        if (err)
                return -EFAULT;

        sioc.number = sioc32.number;
        sioc.length = sioc32.length;
        
	if (sioc32.arg == (__kernel_caddr_t32) NULL || sioc32.length == 0) {
		sioc.arg = (void*)(unsigned long)sioc32.arg;
	} else {
		sioc.arg = A(sioc32.arg);
		if (verify_area(VERIFY_WRITE, sioc.arg, sioc32.length))
			return -EFAULT;
	}

	old_fs = get_fs(); set_fs (KERNEL_DS);
	err = sys_ioctl (fd, cmd, (unsigned long)&sioc);
	set_fs (old_fs);

	if (!err)
		err = __put_user(sioc.length, &(((struct atmif_sioc32*)arg)->length));

	return err;
}


static int do_atm_ioctl(unsigned int fd, unsigned int cmd32, unsigned long arg)
{
        int i;
        unsigned int cmd = 0;
        
	switch (cmd32) {
	case SONET_GETSTAT:
	case SONET_GETSTATZ:
	case SONET_GETDIAG:
	case SONET_SETDIAG:
	case SONET_CLRDIAG:
	case SONET_SETFRAMING:
	case SONET_GETFRAMING:
	case SONET_GETFRSENSE:
		return do_atmif_sioc(fd, cmd32, arg);
	}

		for (i = 0; i < NR_ATM_IOCTL; i++) {
			if (cmd32 == atm_ioctl_map[i].cmd32) {
				cmd = atm_ioctl_map[i].cmd;
				break;
			}
		}
	        if (i == NR_ATM_IOCTL) {
	        return -EINVAL;
	        }
        
        switch (cmd) {
	case ATM_GETNAMES:
		return do_atm_iobuf(fd, cmd, arg);
	    
	case ATM_GETLINKRATE:
        case ATM_GETTYPE:
        case ATM_GETESI:
        case ATM_GETADDR:
        case ATM_RSTADDR:
        case ATM_ADDADDR:
        case ATM_DELADDR:
        case ATM_GETCIRANGE:
	case ATM_SETCIRANGE:
	case ATM_SETESI:
	case ATM_SETESIF:
	case ATM_GETSTAT:
	case ATM_GETSTATZ:
	case ATM_GETLOOP:
	case ATM_SETLOOP:
	case ATM_QUERYLOOP:
                return do_atmif_sioc(fd, cmd, arg);
        }

        return -EINVAL;
}

#if defined(CONFIG_BLK_DEV_LVM) || defined(CONFIG_BLK_DEV_LVM_MODULE)
/* Ugh, LVM. Pitty it was not cleaned up before accepted :((. */
typedef struct {
	uint8_t vg_name[NAME_LEN];
	uint32_t vg_number;
	uint32_t vg_access;
	uint32_t vg_status;
	uint32_t lv_max;
	uint32_t lv_cur;
	uint32_t lv_open;
	uint32_t pv_max;
	uint32_t pv_cur;
	uint32_t pv_act;
	uint32_t dummy;
	uint32_t vgda;
	uint32_t pe_size;
	uint32_t pe_total;
	uint32_t pe_allocated;
	uint32_t pvg_total;
	u32 proc;
	u32 pv[ABS_MAX_PV + 1];
	u32 lv[ABS_MAX_LV + 1];
    	uint8_t vg_uuid[UUID_LEN+1];	/* volume group UUID */
	uint8_t dummy1[200];
} vg32_t;

typedef struct {
	uint8_t id[2];
	uint16_t version;
	lvm_disk_data_t pv_on_disk;
	lvm_disk_data_t vg_on_disk;
	lvm_disk_data_t pv_namelist_on_disk;
	lvm_disk_data_t lv_on_disk;
	lvm_disk_data_t pe_on_disk;
	uint8_t pv_name[NAME_LEN];
	uint8_t vg_name[NAME_LEN];
	uint8_t system_id[NAME_LEN];
	kdev_t pv_dev;
	uint32_t pv_number;
	uint32_t pv_status;
	uint32_t pv_allocatable;
	uint32_t pv_size;
	uint32_t lv_cur;
	uint32_t pe_size;
	uint32_t pe_total;
	uint32_t pe_allocated;
	uint32_t pe_stale;
	u32 pe;
	u32 inode;
	uint8_t pv_uuid[UUID_LEN+1];
} pv32_t;

typedef struct {
	char lv_name[NAME_LEN];
	u32 lv;
} lv_req32_t;

typedef struct {
	u32 lv_index;
	u32 lv;
	/* Transfer size because user space and kernel space differ */
	uint16_t size;
} lv_status_byindex_req32_t;

typedef struct {
	__kernel_dev_t32 dev;
	u32   lv;
} lv_status_bydev_req32_t;

typedef struct {
	uint8_t lv_name[NAME_LEN];
	kdev_t old_dev;
	kdev_t new_dev;
	u32 old_pe;
	u32 new_pe;
} le_remap_req32_t;

typedef struct {
	char pv_name[NAME_LEN];
	u32 pv;
} pv_status_req32_t;

typedef struct {
	uint8_t lv_name[NAME_LEN];
	uint8_t vg_name[NAME_LEN];
	uint32_t lv_access;
	uint32_t lv_status;
	uint32_t lv_open;
	kdev_t lv_dev;
	uint32_t lv_number;
	uint32_t lv_mirror_copies;
	uint32_t lv_recovery;
	uint32_t lv_schedule;
	uint32_t lv_size;
	u32 lv_current_pe;
	uint32_t lv_current_le;
	uint32_t lv_allocated_le;
	uint32_t lv_stripes;
	uint32_t lv_stripesize;
	uint32_t lv_badblock;
	uint32_t lv_allocation;
	uint32_t lv_io_timeout;
	uint32_t lv_read_ahead;
	/* delta to version 1 starts here */
	u32 lv_snapshot_org;
	u32 lv_snapshot_prev;
	u32 lv_snapshot_next;
	u32 lv_block_exception;
	uint32_t lv_remap_ptr;
	uint32_t lv_remap_end;
	uint32_t lv_chunk_size;
	uint32_t lv_snapshot_minor;
	char dummy[200];
} lv32_t;

typedef struct {
	u32 hash[2];
	u32 rsector_org;
	kdev_t rdev_org;
	u32 rsector_new;
	kdev_t rdev_new;
} lv_block_exception32_t;

static void put_lv_t(lv_t *l)
{
	if (l->lv_current_pe) vfree(l->lv_current_pe);
	if (l->lv_block_exception) vfree(l->lv_block_exception);
	kfree(l);
}

static lv_t *get_lv_t(u32 p, int *errp)
{
	int err, i;
	u32 ptr1, ptr2;
	size_t size;
	lv_block_exception32_t *lbe32;
	lv_block_exception_t *lbe;
	lv32_t *ul = (lv32_t *)A(p);
	lv_t *l = (lv_t *) kmalloc(sizeof(lv_t), GFP_KERNEL);

	if (!l) {
		*errp = -ENOMEM;
		return NULL;
	}
	memset(l, 0, sizeof(lv_t));
	err = copy_from_user(l, ul, (long)&((lv32_t *)0)->lv_current_pe);
	err |= __copy_from_user(&l->lv_current_le, &ul->lv_current_le,
				((long)&ul->lv_snapshot_org) - ((long)&ul->lv_current_le));
	err |= __copy_from_user(&l->lv_remap_ptr, &ul->lv_remap_ptr,
				((long)&ul->dummy[0]) - ((long)&ul->lv_remap_ptr));
	err |= __get_user(ptr1, &ul->lv_current_pe);
	err |= __get_user(ptr2, &ul->lv_block_exception);
	if (err) {
		kfree(l);
		*errp = -EFAULT;
		return NULL;
	}
	if (ptr1) {
		if (l->lv_allocated_le > 2*PAGE_SIZE/sizeof(pe_t)) {
			kfree(l);
			*errp = -EINVAL;
			return NULL;
		}
		size = l->lv_allocated_le * sizeof(pe_t);
		l->lv_current_pe = vmalloc(size);
		if (l->lv_current_pe)
			err = copy_from_user(l->lv_current_pe, (void *)A(ptr1), size);
	}
	if (!err && ptr2) {
		/* small limit */
		/* just verify area it? */
		if (l->lv_remap_end > 256*PAGE_SIZE/sizeof(lv_block_exception_t)) {
			put_lv_t(l);
			*errp = -EINVAL;
			return NULL;
		}
		size = l->lv_remap_end * sizeof(lv_block_exception_t);
		l->lv_block_exception = lbe = vmalloc(size);
		if (l->lv_block_exception) {
			lbe32 = (lv_block_exception32_t *)A(ptr2);
			memset(lbe, 0, size);
			for (i = 0; i < l->lv_remap_end; i++, lbe++, lbe32++) {
				err |= get_user(lbe->rsector_org, &lbe32->rsector_org);
				err |= __get_user(lbe->rdev_org, &lbe32->rdev_org);
				err |= __get_user(lbe->rsector_new, &lbe32->rsector_new);
				err |= __get_user(lbe->rdev_new, &lbe32->rdev_new);
			}
		}
	}
	if (err || (ptr1 && !l->lv_current_pe) || (ptr2 && !l->lv_block_exception)) {
		if (!err)
			*errp = -ENOMEM;
		else
			*errp = -EFAULT;
		put_lv_t(l);
		return NULL;
	}
	return l;
}

static int copy_lv_t(u32 ptr, lv_t *l)
{
	int err;
	lv32_t *ul = (lv32_t *)A(ptr);
	u32 ptr1;
	size_t size;

	err = get_user(ptr1, &ul->lv_current_pe);
	if (err)
		return -EFAULT;
	err = copy_to_user(ul, l, (long)&((lv32_t *)0)->lv_current_pe);
	err |= __copy_to_user(&ul->lv_current_le, &l->lv_current_le,
				((long)&ul->lv_snapshot_org) - ((long)&ul->lv_current_le));
	err |= __copy_to_user(&ul->lv_remap_ptr, &l->lv_remap_ptr,
				((long)&ul->dummy[0]) - ((long)&ul->lv_remap_ptr));
	size = l->lv_allocated_le * sizeof(pe_t);
	if (ptr1)
		err |= __copy_to_user((void *)A(ptr1), l->lv_current_pe, size);
	return err ? -EFAULT : 0;
}

static int do_lvm_ioctl(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	vg_t *v = NULL;
	union {
		lv_req_t lv_req;
		le_remap_req_t le_remap;
		lv_status_byindex_req_t lv_byindex;
	        lv_status_bydev_req_t lv_bydev;
		pv_status_req_t pv_status;
	} u;
	pv_t p;
	int err;
	u32 ptr = 0;
	int i;
	mm_segment_t old_fs;
	void *karg = &u;

	switch (cmd) {
	case VG_STATUS:
		v = kmalloc(sizeof(vg_t), GFP_KERNEL);
		if (!v)
			return -ENOMEM;
		karg = v;
		break;

	case VG_CREATE_OLD:
	case VG_CREATE:
		v = kmalloc(sizeof(vg_t), GFP_KERNEL);
		if (!v)
			return -ENOMEM;
		if (copy_from_user(v, (void *)arg, (long)&((vg32_t *)0)->proc)) {
			kfree(v);
			return -EFAULT;
		}
		/* 'proc' field is unused, just NULL it out. */
		v->proc = NULL;
		if (copy_from_user(v->vg_uuid, ((vg32_t *)arg)->vg_uuid, UUID_LEN+1)) {
			kfree(v);
			return -EFAULT;
		}
		    
		karg = v;
		memset(v->pv, 0, sizeof(v->pv) + sizeof(v->lv));
		if (v->pv_max > ABS_MAX_PV || v->lv_max > ABS_MAX_LV)
			return -EPERM;
		for (i = 0; i < v->pv_max; i++) {
			err = __get_user(ptr, &((vg32_t *)arg)->pv[i]);
			if (err)
				break;
			if (ptr) {
				v->pv[i] = kmalloc(sizeof(pv_t), GFP_KERNEL);
				if (!v->pv[i]) {
					err = -ENOMEM;
					break;
				}
				err = copy_from_user(v->pv[i], (void *)A(ptr),
						     sizeof(pv32_t) - 8 - UUID_LEN+1);
				if (err) {
					err = -EFAULT;
					break;
				}
				err = copy_from_user(v->pv[i]->pv_uuid,
						     ((pv32_t *)A(ptr))->pv_uuid,
						     UUID_LEN+1);
				if (err) {
				        err = -EFAULT;
					break;
				}

				v->pv[i]->pe = NULL;
				v->pv[i]->bd = NULL;
			}
		}
		if (!err) {
			for (i = 0; i < v->lv_max; i++) {
				err = __get_user(ptr, &((vg32_t *)arg)->lv[i]);
				if (err)
					break;
				if (ptr) {
					v->lv[i] = get_lv_t(ptr, &err);
					if (err)
						break;
				}
			}
		}
		break;

	case LV_CREATE:
	case LV_EXTEND:
	case LV_REDUCE:
	case LV_REMOVE:
	case LV_RENAME:
	case LV_STATUS_BYNAME:
	        err = copy_from_user(&u.pv_status, arg, sizeof(u.pv_status.pv_name));
		if (err)
			return -EFAULT;
		if (cmd != LV_REMOVE) {
			err = __get_user(ptr, &((lv_req32_t *)arg)->lv);
			if (err)
				return err;
			u.lv_req.lv = get_lv_t(ptr, &err);
		} else
			u.lv_req.lv = NULL;
		break;

	case LV_STATUS_BYINDEX:
		err = get_user(u.lv_byindex.lv_index,
			       &((lv_status_byindex_req32_t *)arg)->lv_index);
		err |= __get_user(ptr, &((lv_status_byindex_req32_t *)arg)->lv);
		if (err)
			return err;
		u.lv_byindex.lv = get_lv_t(ptr, &err);
		break;

	case LV_STATUS_BYDEV:
	        err = get_user(u.lv_bydev.dev, &((lv_status_bydev_req32_t *)arg)->dev);
		err |= __get_user(ptr, &((lv_status_bydev_req32_t *)arg)->lv);
		if (err)
			return err;
		u.lv_bydev.lv = get_lv_t(ptr, &err);
		break;

	case VG_EXTEND:
		err = copy_from_user(&p, (void *)arg, sizeof(pv32_t) - 8 - UUID_LEN+1);
		if (err)
			return -EFAULT;
		err = copy_from_user(p.pv_uuid, ((pv32_t *)arg)->pv_uuid, UUID_LEN+1);
		if (err)
			return -EFAULT;
		p.pe = NULL;
		p.bd = NULL;
		karg = &p;
		break;

	case PV_CHANGE:
	case PV_STATUS:
		err = copy_from_user(&u.pv_status, arg, sizeof(u.lv_req.lv_name));
		if (err)
			return -EFAULT;
		err = __get_user(ptr, &((pv_status_req32_t *)arg)->pv);
		if (err)
			return err;
		u.pv_status.pv = &p;
		if (cmd == PV_CHANGE) {
			err = copy_from_user(&p, (void *)A(ptr),
					     sizeof(pv32_t) - 8 - UUID_LEN+1);
			if (err)
				return -EFAULT;
			p.pe = NULL;
			p.bd = NULL;
		}
		break;
	};

        old_fs = get_fs(); set_fs (KERNEL_DS);
        err = sys_ioctl (fd, cmd, (unsigned long)karg);
        set_fs (old_fs);

	switch (cmd) {
	case VG_STATUS:
		if (!err) {
			if (copy_to_user((void *)arg, v, (long)&((vg32_t *)0)->proc) ||
			    clear_user(&((vg32_t *)arg)->proc, sizeof(vg32_t) - (long)&((vg32_t *)0)->proc))
				err = -EFAULT;
		}
		if (copy_to_user(((vg32_t *)arg)->vg_uuid, v->vg_uuid, UUID_LEN+1)) {
		        err = -EFAULT;
		}
		kfree(v);
		break;

	case VG_CREATE_OLD:
	case VG_CREATE:
		for (i = 0; i < v->pv_max; i++) {
			if (v->pv[i])
				kfree(v->pv[i]);
		}
		for (i = 0; i < v->lv_max; i++) {
			if (v->lv[i])
				put_lv_t(v->lv[i]);
		}
		kfree(v);
		break;

	case LV_STATUS_BYNAME:
		if (!err && u.lv_req.lv)
			err = copy_lv_t(ptr, u.lv_req.lv);
		/* Fall through */

        case LV_CREATE:
	case LV_EXTEND:
	case LV_REDUCE:
		if (u.lv_req.lv)
			put_lv_t(u.lv_req.lv);
		break;

	case LV_STATUS_BYINDEX:
		if (u.lv_byindex.lv) {
			if (!err)
				err = copy_lv_t(ptr, u.lv_byindex.lv);
			put_lv_t(u.lv_byindex.lv);
		}
		break;

	case LV_STATUS_BYDEV:
	        if (u.lv_bydev.lv) {
			if (!err)
				err = copy_lv_t(ptr, u.lv_bydev.lv);
			put_lv_t(u.lv_byindex.lv);
	        }
	        break;

	case PV_STATUS:
		if (!err) {
			err = copy_to_user((void *)A(ptr), &p, sizeof(pv32_t) - 8 - UUID_LEN+1);
			if (err)
				return -EFAULT;
			err = copy_to_user(((pv_t *)A(ptr))->pv_uuid, p.pv_uuid, UUID_LEN + 1);
			if (err)
				return -EFAULT;
		}
		break;
	};

	return err;
}
#endif

#if defined(CONFIG_DRM) || defined(CONFIG_DRM_MODULE)
/* This really belongs in include/linux/drm.h -DaveM */
#include "../../../drivers/char/drm/drm.h"

typedef struct drm32_version {
	int    version_major;	  /* Major version			    */
	int    version_minor;	  /* Minor version			    */
	int    version_patchlevel;/* Patch level			    */
	int    name_len;	  /* Length of name buffer		    */
	u32    name;		  /* Name of driver			    */
	int    date_len;	  /* Length of date buffer		    */
	u32    date;		  /* User-space buffer to hold date	    */
	int    desc_len;	  /* Length of desc buffer		    */
	u32    desc;		  /* User-space buffer to hold desc	    */
} drm32_version_t;
#define DRM32_IOCTL_VERSION    DRM_IOWR(0x00, drm32_version_t)

static int drm32_version(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	drm32_version_t *uversion = (drm32_version_t *)arg;
	char *name_ptr, *date_ptr, *desc_ptr;
	u32 tmp1, tmp2, tmp3;
	drm_version_t kversion;
	mm_segment_t old_fs;
	int ret;

	memset(&kversion, 0, sizeof(kversion));
	if (get_user(kversion.name_len, &uversion->name_len) ||
	    get_user(kversion.date_len, &uversion->date_len) ||
	    get_user(kversion.desc_len, &uversion->desc_len) ||
	    get_user(tmp1, &uversion->name) ||
	    get_user(tmp2, &uversion->date) ||
	    get_user(tmp3, &uversion->desc))
		return -EFAULT;

	name_ptr = (char *) A(tmp1);
	date_ptr = (char *) A(tmp2);
	desc_ptr = (char *) A(tmp3);

	ret = -ENOMEM;
	if (kversion.name_len && name_ptr) {
		kversion.name = kmalloc(kversion.name_len, GFP_KERNEL);
		if (!kversion.name)
			goto out;
	}
	if (kversion.date_len && date_ptr) {
		kversion.date = kmalloc(kversion.date_len, GFP_KERNEL);
		if (!kversion.date)
			goto out;
	}
	if (kversion.desc_len && desc_ptr) {
		kversion.desc = kmalloc(kversion.desc_len, GFP_KERNEL);
		if (!kversion.desc)
			goto out;
	}

        old_fs = get_fs();
	set_fs(KERNEL_DS);
        ret = sys_ioctl (fd, DRM_IOCTL_VERSION, (unsigned long)&kversion);
        set_fs(old_fs);

	if (!ret) {
		if ((kversion.name &&
		     copy_to_user(name_ptr, kversion.name, kversion.name_len)) ||
		    (kversion.date &&
		     copy_to_user(date_ptr, kversion.date, kversion.date_len)) ||
		    (kversion.desc &&
		     copy_to_user(desc_ptr, kversion.desc, kversion.desc_len)))
			ret = -EFAULT;
		if (put_user(kversion.version_major, &uversion->version_major) ||
		    put_user(kversion.version_minor, &uversion->version_minor) ||
		    put_user(kversion.version_patchlevel, &uversion->version_patchlevel) ||
		    put_user(kversion.name_len, &uversion->name_len) ||
		    put_user(kversion.date_len, &uversion->date_len) ||
		    put_user(kversion.desc_len, &uversion->desc_len))
			ret = -EFAULT;
	}

out:
	if (kversion.name)
		kfree(kversion.name);
	if (kversion.date)
		kfree(kversion.date);
	if (kversion.desc)
		kfree(kversion.desc);
	return ret;
}

typedef struct drm32_unique {
	int	unique_len;	  /* Length of unique			    */
	u32	unique;		  /* Unique name for driver instantiation   */
} drm32_unique_t;
#define DRM32_IOCTL_GET_UNIQUE DRM_IOWR(0x01, drm32_unique_t)
#define DRM32_IOCTL_SET_UNIQUE DRM_IOW( 0x10, drm32_unique_t)

static int drm32_getsetunique(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	drm32_unique_t *uarg = (drm32_unique_t *)arg;
	drm_unique_t karg;
	mm_segment_t old_fs;
	char *uptr;
	u32 tmp;
	int ret;

	if (get_user(karg.unique_len, &uarg->unique_len))
		return -EFAULT;
	karg.unique = NULL;

	if (get_user(tmp, &uarg->unique))
		return -EFAULT;

	uptr = (char *) A(tmp);

	if (uptr) {
		karg.unique = kmalloc(karg.unique_len, GFP_KERNEL);
		if (!karg.unique)
			return -ENOMEM;
		if (cmd == DRM32_IOCTL_SET_UNIQUE &&
		    copy_from_user(karg.unique, uptr, karg.unique_len)) {
			kfree(karg.unique);
			return -EFAULT;
		}
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	if (cmd == DRM32_IOCTL_GET_UNIQUE)
		ret = sys_ioctl (fd, DRM_IOCTL_GET_UNIQUE, (unsigned long)&karg);
	else
		ret = sys_ioctl (fd, DRM_IOCTL_SET_UNIQUE, (unsigned long)&karg);
        set_fs(old_fs);

	if (!ret) {
		if (cmd == DRM32_IOCTL_GET_UNIQUE &&
		    uptr != NULL &&
		    copy_to_user(uptr, karg.unique, karg.unique_len))
			ret = -EFAULT;
		if (put_user(karg.unique_len, &uarg->unique_len))
			ret = -EFAULT;
	}

	if (karg.unique != NULL)
		kfree(karg.unique);

	return ret;
}

typedef struct drm32_map {
	u32		offset;	 /* Requested physical address (0 for SAREA)*/
	u32		size;	 /* Requested physical size (bytes)	    */
	drm_map_type_t	type;	 /* Type of memory to map		    */
	drm_map_flags_t flags;	 /* Flags				    */
	u32		handle;  /* User-space: "Handle" to pass to mmap    */
				 /* Kernel-space: kernel-virtual address    */
	int		mtrr;	 /* MTRR slot used			    */
				 /* Private data			    */
} drm32_map_t;
#define DRM32_IOCTL_ADD_MAP    DRM_IOWR(0x15, drm32_map_t)

static int drm32_addmap(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	drm32_map_t *uarg = (drm32_map_t *) arg;
	drm_map_t karg;
	mm_segment_t old_fs;
	u32 tmp;
	int ret;

	ret  = get_user(karg.offset, &uarg->offset);
	ret |= get_user(karg.size, &uarg->size);
	ret |= get_user(karg.type, &uarg->type);
	ret |= get_user(karg.flags, &uarg->flags);
	ret |= get_user(tmp, &uarg->handle);
	ret |= get_user(karg.mtrr, &uarg->mtrr);
	if (ret)
		return -EFAULT;

	karg.handle = (void *) A(tmp);

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	ret = sys_ioctl(fd, DRM_IOCTL_ADD_MAP, (unsigned long) &karg);
	set_fs(old_fs);

	if (!ret) {
		ret  = put_user(karg.offset, &uarg->offset);
		ret |= put_user(karg.size, &uarg->size);
		ret |= put_user(karg.type, &uarg->type);
		ret |= put_user(karg.flags, &uarg->flags);
		tmp = (u32) (long)karg.handle;
		ret |= put_user(tmp, &uarg->handle);
		ret |= put_user(karg.mtrr, &uarg->mtrr);
		if (ret)
			ret = -EFAULT;
	}

	return ret;
}

typedef struct drm32_buf_info {
	int	       count;	/* Entries in list			     */
	u32	       list;    /* (drm_buf_desc_t *) */ 
} drm32_buf_info_t;
#define DRM32_IOCTL_INFO_BUFS  DRM_IOWR(0x18, drm32_buf_info_t)

static int drm32_info_bufs(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	drm32_buf_info_t *uarg = (drm32_buf_info_t *)arg;
	drm_buf_desc_t *ulist;
	drm_buf_info_t karg;
	mm_segment_t old_fs;
	int orig_count, ret;
	u32 tmp;

	if (get_user(karg.count, &uarg->count) ||
	    get_user(tmp, &uarg->list))
		return -EFAULT;

	ulist = (drm_buf_desc_t *) A(tmp);

	orig_count = karg.count;

	karg.list = kmalloc(karg.count * sizeof(drm_buf_desc_t), GFP_KERNEL);
	if (!karg.list)
		return -EFAULT;

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	ret = sys_ioctl(fd, DRM_IOCTL_INFO_BUFS, (unsigned long) &karg);
	set_fs(old_fs);

	if (!ret) {
		if (karg.count <= orig_count &&
		    (copy_to_user(ulist, karg.list,
				  karg.count * sizeof(drm_buf_desc_t))))
			ret = -EFAULT;
		if (put_user(karg.count, &uarg->count))
			ret = -EFAULT;
	}

	kfree(karg.list);

	return ret;
}

typedef struct drm32_buf_free {
	int	       count;
	u32	       list;	/* (int *) */
} drm32_buf_free_t;
#define DRM32_IOCTL_FREE_BUFS  DRM_IOW( 0x1a, drm32_buf_free_t)

static int drm32_free_bufs(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	drm32_buf_free_t *uarg = (drm32_buf_free_t *)arg;
	drm_buf_free_t karg;
	mm_segment_t old_fs;
	int *ulist;
	int ret;
	u32 tmp;

	if (get_user(karg.count, &uarg->count) ||
	    get_user(tmp, &uarg->list))
		return -EFAULT;

	ulist = (int *) A(tmp);

	karg.list = kmalloc(karg.count * sizeof(int), GFP_KERNEL);
	if (!karg.list)
		return -ENOMEM;

	ret = -EFAULT;
	if (copy_from_user(karg.list, ulist, (karg.count * sizeof(int))))
		goto out;

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	ret = sys_ioctl(fd, DRM_IOCTL_FREE_BUFS, (unsigned long) &karg);
	set_fs(old_fs);

out:
	kfree(karg.list);

	return ret;
}

typedef struct drm32_buf_pub {
	int		  idx;	       /* Index into master buflist	     */
	int		  total;       /* Buffer size			     */
	int		  used;	       /* Amount of buffer in use (for DMA)  */
	u32		  address;     /* Address of buffer (void *)	     */
} drm32_buf_pub_t;

typedef struct drm32_buf_map {
	int	      count;	/* Length of buflist			    */
	u32	      virtual;	/* Mmaped area in user-virtual (void *)	    */
	u32 	      list;	/* Buffer information (drm_buf_pub_t *)	    */
} drm32_buf_map_t;
#define DRM32_IOCTL_MAP_BUFS   DRM_IOWR(0x19, drm32_buf_map_t)

static int drm32_map_bufs(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	drm32_buf_map_t *uarg = (drm32_buf_map_t *)arg;
	drm32_buf_pub_t *ulist;
	drm_buf_map_t karg;
	mm_segment_t old_fs;
	int orig_count, ret, i;
	u32 tmp1, tmp2;

	if (get_user(karg.count, &uarg->count) ||
	    get_user(tmp1, &uarg->virtual) ||
	    get_user(tmp2, &uarg->list))
		return -EFAULT;

	karg.virtual = (void *) A(tmp1);
	ulist = (drm32_buf_pub_t *) A(tmp2);

	orig_count = karg.count;

	karg.list = kmalloc(karg.count * sizeof(drm_buf_pub_t), GFP_KERNEL);
	if (!karg.list)
		return -ENOMEM;

	ret = -EFAULT;
	for (i = 0; i < karg.count; i++) {
		if (get_user(karg.list[i].idx, &ulist[i].idx) ||
		    get_user(karg.list[i].total, &ulist[i].total) ||
		    get_user(karg.list[i].used, &ulist[i].used) ||
		    get_user(tmp1, &ulist[i].address))
			goto out;

		karg.list[i].address = (void *) A(tmp1);
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	ret = sys_ioctl(fd, DRM_IOCTL_MAP_BUFS, (unsigned long) &karg);
	set_fs(old_fs);

	if (!ret) {
		for (i = 0; i < orig_count; i++) {
			tmp1 = (u32) (long) karg.list[i].address;
			if (put_user(karg.list[i].idx, &ulist[i].idx) ||
			    put_user(karg.list[i].total, &ulist[i].total) ||
			    put_user(karg.list[i].used, &ulist[i].used) ||
			    put_user(tmp1, &ulist[i].address)) {
				ret = -EFAULT;
				goto out;
			}
		}
		if (put_user(karg.count, &uarg->count))
			ret = -EFAULT;
	}

out:
	kfree(karg.list);
	return ret;
}

typedef struct drm32_dma {
				/* Indices here refer to the offset into
				   buflist in drm_buf_get_t.  */
	int		context;	  /* Context handle		    */
	int		send_count;	  /* Number of buffers to send	    */
	u32		send_indices;	  /* List of handles to buffers (int *) */
	u32		send_sizes;	  /* Lengths of data to send (int *) */
	drm_dma_flags_t flags;		  /* Flags			    */
	int		request_count;	  /* Number of buffers requested    */
	int		request_size;	  /* Desired size for buffers	    */
	u32		request_indices;  /* Buffer information (int *)	    */
	u32		request_sizes;    /* (int *) */
	int		granted_count;	  /* Number of buffers granted	    */
} drm32_dma_t;
#define DRM32_IOCTL_DMA	     DRM_IOWR(0x29, drm32_dma_t)

/* RED PEN	The DRM layer blindly dereferences the send/request
 * 		index/size arrays even though they are userland
 * 		pointers.  -DaveM
 */
static int drm32_dma(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	drm32_dma_t *uarg = (drm32_dma_t *) arg;
	int *u_si, *u_ss, *u_ri, *u_rs;
	drm_dma_t karg;
	mm_segment_t old_fs;
	int ret;
	u32 tmp1, tmp2, tmp3, tmp4;

	karg.send_indices = karg.send_sizes = NULL;
	karg.request_indices = karg.request_sizes = NULL;

	if (get_user(karg.context, &uarg->context) ||
	    get_user(karg.send_count, &uarg->send_count) ||
	    get_user(tmp1, &uarg->send_indices) ||
	    get_user(tmp2, &uarg->send_sizes) ||
	    get_user(karg.flags, &uarg->flags) ||
	    get_user(karg.request_count, &uarg->request_count) ||
	    get_user(karg.request_size, &uarg->request_size) ||
	    get_user(tmp3, &uarg->request_indices) ||
	    get_user(tmp4, &uarg->request_sizes) ||
	    get_user(karg.granted_count, &uarg->granted_count))
		return -EFAULT;

	u_si = (int *) A(tmp1);
	u_ss = (int *) A(tmp2);
	u_ri = (int *) A(tmp3);
	u_rs = (int *) A(tmp4);

	if (karg.send_count) {
		karg.send_indices = kmalloc(karg.send_count * sizeof(int), GFP_KERNEL);
		karg.send_sizes = kmalloc(karg.send_count * sizeof(int), GFP_KERNEL);

		ret = -ENOMEM;
		if (!karg.send_indices || !karg.send_sizes)
			goto out;

		ret = -EFAULT;
		if (copy_from_user(karg.send_indices, u_si,
				   (karg.send_count * sizeof(int))) ||
		    copy_from_user(karg.send_sizes, u_ss,
				   (karg.send_count * sizeof(int))))
			goto out;
	}

	if (karg.request_count) {
		karg.request_indices = kmalloc(karg.request_count * sizeof(int), GFP_KERNEL);
		karg.request_sizes = kmalloc(karg.request_count * sizeof(int), GFP_KERNEL);

		ret = -ENOMEM;
		if (!karg.request_indices || !karg.request_sizes)
			goto out;

		ret = -EFAULT;
		if (copy_from_user(karg.request_indices, u_ri,
				   (karg.request_count * sizeof(int))) ||
		    copy_from_user(karg.request_sizes, u_rs,
				   (karg.request_count * sizeof(int))))
			goto out;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	ret = sys_ioctl(fd, DRM_IOCTL_DMA, (unsigned long) &karg);
	set_fs(old_fs);

	if (!ret) {
		if (put_user(karg.context, &uarg->context) ||
		    put_user(karg.send_count, &uarg->send_count) ||
		    put_user(karg.flags, &uarg->flags) ||
		    put_user(karg.request_count, &uarg->request_count) ||
		    put_user(karg.request_size, &uarg->request_size) ||
		    put_user(karg.granted_count, &uarg->granted_count))
			ret = -EFAULT;

		if (karg.send_count) {
			if (copy_to_user(u_si, karg.send_indices,
					 (karg.send_count * sizeof(int))) ||
			    copy_to_user(u_ss, karg.send_sizes,
					 (karg.send_count * sizeof(int))))
				ret = -EFAULT;
		}
		if (karg.request_count) {
			if (copy_to_user(u_ri, karg.request_indices,
					 (karg.request_count * sizeof(int))) ||
			    copy_to_user(u_rs, karg.request_sizes,
					 (karg.request_count * sizeof(int))))
				ret = -EFAULT;
		}
	}

out:
	if (karg.send_indices)
		kfree(karg.send_indices);
	if (karg.send_sizes)
		kfree(karg.send_sizes);
	if (karg.request_indices)
		kfree(karg.request_indices);
	if (karg.request_sizes)
		kfree(karg.request_sizes);

	return ret;
}

typedef struct drm32_ctx_res {
	int		count;
	u32		contexts; /* (drm_ctx_t *) */
} drm32_ctx_res_t;
#define DRM32_IOCTL_RES_CTX    DRM_IOWR(0x26, drm32_ctx_res_t)

static int drm32_res_ctx(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	drm32_ctx_res_t *uarg = (drm32_ctx_res_t *) arg;
	drm_ctx_t *ulist;
	drm_ctx_res_t karg;
	mm_segment_t old_fs;
	int orig_count, ret;
	u32 tmp;

	karg.contexts = NULL;
	if (get_user(karg.count, &uarg->count) ||
	    get_user(tmp, &uarg->contexts))
		return -EFAULT;

	ulist = (drm_ctx_t *) A(tmp);

	orig_count = karg.count;
	if (karg.count && ulist) {
		karg.contexts = kmalloc((karg.count * sizeof(drm_ctx_t)), GFP_KERNEL);
		if (!karg.contexts)
			return -ENOMEM;
		if (copy_from_user(karg.contexts, ulist,
				   (karg.count * sizeof(drm_ctx_t)))) {
			kfree(karg.contexts);
			return -EFAULT;
		}
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	ret = sys_ioctl(fd, DRM_IOCTL_RES_CTX, (unsigned long) &karg);
	set_fs(old_fs);

	if (!ret) {
		if (orig_count) {
			if (copy_to_user(ulist, karg.contexts,
					 (orig_count * sizeof(drm_ctx_t))))
				ret = -EFAULT;
		}
		if (put_user(karg.count, &uarg->count))
			ret = -EFAULT;
	}

	if (karg.contexts)
		kfree(karg.contexts);

	return ret;
}

#endif

static int ret_einval(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	return -EINVAL;
}

static int broken_blkgetsize(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	/* The mkswap binary hard codes it to Intel value :-((( */
	return w_long(fd, BLKGETSIZE, arg);
}

struct blkpg_ioctl_arg32 {
	int op;
	int flags;
	int datalen;
	u32 data;
};
                                
static int blkpg_ioctl_trans(unsigned int fd, unsigned int cmd, struct blkpg_ioctl_arg32 *arg)
{
	struct blkpg_ioctl_arg a;
	struct blkpg_partition p;
	int err;
	mm_segment_t old_fs = get_fs();
	
	err = get_user(a.op, &arg->op);
	err |= __get_user(a.flags, &arg->flags);
	err |= __get_user(a.datalen, &arg->datalen);
	err |= __get_user((long)a.data, &arg->data);
	if (err) return err;
	switch (a.op) {
	case BLKPG_ADD_PARTITION:
	case BLKPG_DEL_PARTITION:
		if (a.datalen < sizeof(struct blkpg_partition))
			return -EINVAL;
                if (copy_from_user(&p, a.data, sizeof(struct blkpg_partition)))
			return -EFAULT;
		a.data = &p;
		set_fs (KERNEL_DS);
		err = sys_ioctl(fd, cmd, (unsigned long)&a);
		set_fs (old_fs);
		break;
	default:
		return -EINVAL;
	}                                        
	return err;
}

static int ioc_settimeout(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	return rw_long(fd, AUTOFS_IOC_SETTIMEOUT, arg);
}

struct usbdevfs_ctrltransfer32 {
	__u8 requesttype;
	__u8 request;
	__u16 value;
	__u16 index;
	__u16 length;
	__u32 timeout;  /* in milliseconds */
	__u32 data;
};

#define USBDEVFS_CONTROL32           _IOWR('U', 0, struct usbdevfs_ctrltransfer32)

static int do_usbdevfs_control(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	struct usbdevfs_ctrltransfer kctrl;
	struct usbdevfs_ctrltransfer32 *uctrl;
	mm_segment_t old_fs;
	__u32 udata;
	void *uptr, *kptr;
	int err;

	uctrl = (struct usbdevfs_ctrltransfer32 *) arg;

	if (copy_from_user(&kctrl, uctrl,
			   (sizeof(struct usbdevfs_ctrltransfer) -
			    sizeof(void *))))
		return -EFAULT;

	if (get_user(udata, &uctrl->data))
		return -EFAULT;
	uptr = (void *) A(udata);

	/* In usbdevice_fs, it limits the control buffer to a page,
	 * for simplicity so do we.
	 */
	if (!uptr || kctrl.length > PAGE_SIZE)
		return -EINVAL;

	kptr = (void *)__get_free_page(GFP_KERNEL);

	if ((kctrl.requesttype & 0x80) == 0) {
		err = -EFAULT;
		if (copy_from_user(kptr, uptr, kctrl.length))
			goto out;
	}

	kctrl.data = kptr;

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	err = sys_ioctl(fd, USBDEVFS_CONTROL, (unsigned long)&kctrl);
	set_fs(old_fs);

	if (err >= 0 &&
	    ((kctrl.requesttype & 0x80) != 0)) {
		if (copy_to_user(uptr, kptr, kctrl.length))
			err = -EFAULT;
	}

out:
	free_page((unsigned long) kptr);
	return err;
}

struct usbdevfs_bulktransfer32 {
	unsigned int ep;
	unsigned int len;
	unsigned int timeout; /* in milliseconds */
	__u32 data;
};

#define USBDEVFS_BULK32              _IOWR('U', 2, struct usbdevfs_bulktransfer32)

static int do_usbdevfs_bulk(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	struct usbdevfs_bulktransfer kbulk;
	struct usbdevfs_bulktransfer32 *ubulk;
	mm_segment_t old_fs;
	__u32 udata;
	void *uptr, *kptr;
	int err;

	ubulk = (struct usbdevfs_bulktransfer32 *) arg;

	if (get_user(kbulk.ep, &ubulk->ep) ||
	    get_user(kbulk.len, &ubulk->len) ||
	    get_user(kbulk.timeout, &ubulk->timeout) ||
	    get_user(udata, &ubulk->data))
		return -EFAULT;

	uptr = (void *) A(udata);

	/* In usbdevice_fs, it limits the control buffer to a page,
	 * for simplicity so do we.
	 */
	if (!uptr || kbulk.len > PAGE_SIZE)
		return -EINVAL;

	kptr = (void *) __get_free_page(GFP_KERNEL);

	if ((kbulk.ep & 0x80) == 0) {
		err = -EFAULT;
		if (copy_from_user(kptr, uptr, kbulk.len))
			goto out;
	}

	kbulk.data = kptr;

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	err = sys_ioctl(fd, USBDEVFS_BULK, (unsigned long) &kbulk);
	set_fs(old_fs);

	if (err >= 0 &&
	    ((kbulk.ep & 0x80) != 0)) {
		if (copy_to_user(uptr, kptr, kbulk.len))
			err = -EFAULT;
	}

out:
	free_page((unsigned long) kptr);
	return err;
}

/* This needs more work before we can enable it.  Unfortunately
 * because of the fancy asynchronous way URB status/error is written
 * back to userspace, we'll need to fiddle with USB devio internals
 * and/or reimplement entirely the frontend of it ourselves. -DaveM
 *
 * The issue is:
 *
 *	When an URB is submitted via usbdevicefs it is put onto an
 *	asynchronous queue.  When the URB completes, it may be reaped
 *	via another ioctl.  During this reaping the status is written
 *	back to userspace along with the length of the transfer.
 *
 *	We must translate into 64-bit kernel types so we pass in a kernel
 *	space copy of the usbdevfs_urb structure.  This would mean that we
 *	must do something to deal with the async entry reaping.  First we
 *	have to deal somehow with this transitory memory we've allocated.
 *	This is problematic since there are many call sites from which the
 *	async entries can be destroyed (and thus when we'd need to free up
 *	this kernel memory).  One of which is the close() op of usbdevicefs.
 *	To handle that we'd need to make our own file_operations struct which
 *	overrides usbdevicefs's release op with our own which runs usbdevicefs's
 *	real release op then frees up the kernel memory.
 *
 *	But how to keep track of these kernel buffers?  We'd need to either
 *	keep track of them in some table _or_ know about usbdevicefs internals
 *	(ie. the exact layout of it's file private, which is actually defined
 *	in linux/usbdevice_fs.h, the layout of the async queues are private to
 *	devio.c)
 *
 * There is one possible other solution I considered, also involving knowledge
 * of usbdevicefs internals:
 *
 *	After an URB is submitted, we "fix up" the address back to the user
 *	space one.  This would work if the status/length fields written back
 *	by the async URB completion lines up perfectly in the 32-bit type with
 *	the 64-bit kernel type.  Unfortunately, it does not because the iso
 *	frame descriptors, at the end of the struct, can be written back.
 *
 * I think we'll just need to simply duplicate the devio URB engine here.
 */
#if 0
struct usbdevfs_urb32 {
	__u8 type;
	__u8 endpoint;
	__s32 status;
	__u32 flags;
	__u32 buffer;
	__s32 buffer_length;
	__s32 actual_length;
	__s32 start_frame;
	__s32 number_of_packets;
	__s32 error_count;
	__u32 signr;
	__u32 usercontext; /* unused */
	struct usbdevfs_iso_packet_desc iso_frame_desc[0];
};

#define USBDEVFS_SUBMITURB32       _IOR('U', 10, struct usbdevfs_urb32)

static int get_urb32(struct usbdevfs_urb *kurb,
		     struct usbdevfs_urb32 *uurb)
{
	if (get_user(kurb->type, &uurb->type) ||
	    __get_user(kurb->endpoint, &uurb->endpoint) ||
	    __get_user(kurb->status, &uurb->status) ||
	    __get_user(kurb->flags, &uurb->flags) ||
	    __get_user(kurb->buffer_length, &uurb->buffer_length) ||
	    __get_user(kurb->actual_length, &uurb->actual_length) ||
	    __get_user(kurb->start_frame, &uurb->start_frame) ||
	    __get_user(kurb->number_of_packets, &uurb->number_of_packets) ||
	    __get_user(kurb->error_count, &uurb->error_count) ||
	    __get_user(kurb->signr, &uurb->signr))
		return -EFAULT;

	kurb->usercontext = 0; /* unused currently */

	return 0;
}

/* Just put back the values which usbdevfs actually changes. */
static int put_urb32(struct usbdevfs_urb *kurb,
		     struct usbdevfs_urb32 *uurb)
{
	if (put_user(kurb->status, &uurb->status) ||
	    __put_user(kurb->actual_length, &uurb->actual_length) ||
	    __put_user(kurb->error_count, &uurb->error_count))
		return -EFAULT;

	if (kurb->number_of_packets != 0) {
		int i;

		for (i = 0; i < kurb->number_of_packets; i++) {
			if (__put_user(kurb->iso_frame_desc[i].actual_length,
				       &uurb->iso_frame_desc[i].actual_length) ||
			    __put_user(kurb->iso_frame_desc[i].status,
				       &uurb->iso_frame_desc[i].status))
				return -EFAULT;
		}
	}

	return 0;
}

static int get_urb32_isoframes(struct usbdevfs_urb *kurb,
			       struct usbdevfs_urb32 *uurb)
{
	unsigned int totlen;
	int i;

	if (kurb->type != USBDEVFS_URB_TYPE_ISO) {
		kurb->number_of_packets = 0;
		return 0;
	}

	if (kurb->number_of_packets < 1 ||
	    kurb->number_of_packets > 128)
		return -EINVAL;

	if (copy_from_user(&kurb->iso_frame_desc[0],
			   &uurb->iso_frame_desc[0],
			   sizeof(struct usbdevfs_iso_packet_desc) *
			   kurb->number_of_packets))
		return -EFAULT;

	totlen = 0;
	for (i = 0; i < kurb->number_of_packets; i++) {
		unsigned int this_len;

		this_len = kurb->iso_frame_desc[i].length;
		if (this_len > 1023)
			return -EINVAL;

		totlen += this_len;
	}

	if (totlen > 32768)
		return -EINVAL;

	kurb->buffer_length = totlen;

	return 0;
}

static int do_usbdevfs_urb(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	struct usbdevfs_urb *kurb;
	struct usbdevfs_urb32 *uurb;
	mm_segment_t old_fs;
	__u32 udata;
	void *uptr, *kptr;
	unsigned int buflen;
	int err;

	uurb = (struct usbdevfs_urb32 *) arg;

	err = -ENOMEM;
	kurb = kmalloc(sizeof(struct usbdevfs_urb) +
		       (sizeof(struct usbdevfs_iso_packet_desc) * 128),
		       GFP_KERNEL);
	if (!kurb)
		goto out;

	err = -EFAULT;
	if (get_urb32(kurb, uurb))
		goto out;

	err = get_urb32_isoframes(kurb, uurb);
	if (err)
		goto out;

	err = -EFAULT;
	if (__get_user(udata, &uurb->buffer))
		goto out;
	uptr = (void *) A(udata);

	buflen = kurb->buffer_length;

	err = verify_area(VERIFY_WRITE, uptr, buflen);
	if (err)
		goto out;

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	err = sys_ioctl(fd, USBDEVFS_SUBMITURB, (unsigned long) kurb);
	set_fs(old_fs);

	if (err >= 0) {
		/* XXX Shit, this doesn't work for async URBs :-( XXX */
		if (put_urb32(kurb, uurb)) {
			err = -EFAULT;
		}
	}

out:
	kfree(kurb);
	return err;
}
#endif

#define USBDEVFS_REAPURB32         _IOW('U', 12, u32)
#define USBDEVFS_REAPURBNDELAY32   _IOW('U', 13, u32)

static int do_usbdevfs_reapurb(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	mm_segment_t old_fs;
	void *kptr;
	int err;

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	err = sys_ioctl(fd,
			(cmd == USBDEVFS_REAPURB32 ?
			 USBDEVFS_REAPURB :
			 USBDEVFS_REAPURBNDELAY),
			(unsigned long) &kptr);
	set_fs(old_fs);

	if (err >= 0 &&
	    put_user(((u32)(long)kptr), (u32 *) A(arg)))
		err = -EFAULT;

	return err;
}

struct usbdevfs_disconnectsignal32 {
	unsigned int signr;
	u32 context;
};

#define USBDEVFS_DISCSIGNAL32      _IOR('U', 14, struct usbdevfs_disconnectsignal32)

static int do_usbdevfs_discsignal(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	struct usbdevfs_disconnectsignal kdis;
	struct usbdevfs_disconnectsignal32 *udis;
	mm_segment_t old_fs;
	u32 uctx;
	int err;

	udis = (struct usbdevfs_disconnectsignal32 *) arg;

	if (get_user(kdis.signr, &udis->signr) ||
	    __get_user(uctx, &udis->context))
		return -EFAULT;

	kdis.context = (void *) (long)uctx;

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	err = sys_ioctl(fd, USBDEVFS_DISCSIGNAL, (unsigned long) &kdis);
	set_fs(old_fs);

	return err;
}

struct mtd_oob_buf32 {
	u32 start;
	u32 length;
	u32 ptr;	/* unsigned char* */
};

#define MEMWRITEOOB32 	_IOWR('M',3,struct mtd_oob_buf32)
#define MEMREADOOB32 	_IOWR('M',4,struct mtd_oob_buf32)

static inline int
mtd_rw_oob(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	mm_segment_t 			old_fs 	= get_fs();
	struct mtd_oob_buf32	*uarg 	= (struct mtd_oob_buf32 *)arg;
	struct mtd_oob_buf		karg;
	u32 tmp;
	int ret;

	if (get_user(karg.start, &uarg->start) 		||
	    get_user(karg.length, &uarg->length)	||
	    get_user(tmp, &uarg->ptr))
		return -EFAULT;

	karg.ptr = A(tmp);
	if (verify_area(VERIFY_WRITE, karg.ptr, karg.length))
		return -EFAULT;

	set_fs(KERNEL_DS);
	if (MEMREADOOB32 == cmd)
		ret = sys_ioctl(fd, MEMREADOOB, (unsigned long)&karg);
	else if (MEMWRITEOOB32 == cmd)
		ret = sys_ioctl(fd, MEMWRITEOOB, (unsigned long)&karg);
	else
		ret = -EINVAL;
	set_fs(old_fs);

	if (0 == ret && cmd == MEMREADOOB32) {
		ret = put_user(karg.start, &uarg->start);
		ret |= put_user(karg.length, &uarg->length);
	}

	return ret;
}

/* Fix sizeof(sizeof()) breakage */
#define BLKELVGET_32	_IOR(0x12,106,int)
#define BLKELVSET_32	_IOW(0x12,107,int)
#define BLKBSZGET_32	_IOR(0x12,112,int)
#define BLKBSZSET_32	_IOW(0x12,113,int)
#define BLKGETSIZE64_32	_IOR(0x12,114,int)

static int do_blkelvget(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	return sys_ioctl(fd, BLKELVGET, arg);
}

static int do_blkelvset(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	return sys_ioctl(fd, BLKELVSET, arg);
}

static int do_blkbszget(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	return sys_ioctl(fd, BLKBSZGET, arg);
}

static int do_blkbszset(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	return sys_ioctl(fd, BLKBSZSET, arg);
}

static int do_blkgetsize64(unsigned int fd, unsigned int cmd,
			   unsigned long arg)
{
	return sys_ioctl(fd, BLKGETSIZE64, arg);
}

struct ioctl_trans {
	unsigned long cmd;
	unsigned long handler;
	unsigned long next;
};

#define COMPATIBLE_IOCTL(cmd) { cmd, (unsigned long)sys_ioctl, 0 }

#define HANDLE_IOCTL(cmd,handler) { cmd, (unsigned long)handler, 0 }

#define AUTOFS_IOC_SETTIMEOUT32 _IOWR(0x93,0x64,unsigned int)
#define SMB_IOC_GETMOUNTUID_32 _IOR('u', 1, __kernel_uid_t32)

static struct ioctl_trans ioctl_translations[] = {
    /* List here explicitly which ioctl's need translation,
     * all others default to calling sys_ioctl().
     */
/* Big T */
COMPATIBLE_IOCTL(TCGETA),
COMPATIBLE_IOCTL(TCSETA),
COMPATIBLE_IOCTL(TCSETAW),
COMPATIBLE_IOCTL(TCSETAF),
COMPATIBLE_IOCTL(TCSBRK),
COMPATIBLE_IOCTL(TCSBRKP),
COMPATIBLE_IOCTL(TCXONC),
COMPATIBLE_IOCTL(TCFLSH),
COMPATIBLE_IOCTL(TCGETS),
COMPATIBLE_IOCTL(TCSETS),
COMPATIBLE_IOCTL(TCSETSW),
COMPATIBLE_IOCTL(TCSETSF),
COMPATIBLE_IOCTL(TIOCLINUX),
COMPATIBLE_IOCTL(TIOCSTART),
/* Little t */
COMPATIBLE_IOCTL(TIOCGETD),
COMPATIBLE_IOCTL(TIOCSETD),
COMPATIBLE_IOCTL(TIOCEXCL),
COMPATIBLE_IOCTL(TIOCNXCL),
COMPATIBLE_IOCTL(TIOCCONS),
COMPATIBLE_IOCTL(TIOCGSOFTCAR),
COMPATIBLE_IOCTL(TIOCSSOFTCAR),
COMPATIBLE_IOCTL(TIOCSWINSZ),
COMPATIBLE_IOCTL(TIOCGWINSZ),
COMPATIBLE_IOCTL(TIOCMGET),
COMPATIBLE_IOCTL(TIOCMBIC),
COMPATIBLE_IOCTL(TIOCMBIS),
COMPATIBLE_IOCTL(TIOCMSET),
COMPATIBLE_IOCTL(TIOCPKT),
COMPATIBLE_IOCTL(TIOCNOTTY),
COMPATIBLE_IOCTL(TIOCSTI),
COMPATIBLE_IOCTL(TIOCOUTQ),
COMPATIBLE_IOCTL(TIOCSPGRP),
COMPATIBLE_IOCTL(TIOCGPGRP),
COMPATIBLE_IOCTL(TIOCSCTTY),
COMPATIBLE_IOCTL(TIOCGPTN),
COMPATIBLE_IOCTL(TIOCSPTLCK),
COMPATIBLE_IOCTL(TIOCGSERIAL),
COMPATIBLE_IOCTL(TIOCSSERIAL),
COMPATIBLE_IOCTL(TIOCSERGETLSR),
COMPATIBLE_IOCTL(TIOCSLTC),
COMPATIBLE_IOCTL(TIOCMIWAIT),
COMPATIBLE_IOCTL(TIOCGICOUNT),
/* Big F */
COMPATIBLE_IOCTL(FBIOGET_VSCREENINFO),
COMPATIBLE_IOCTL(FBIOPUT_VSCREENINFO),
COMPATIBLE_IOCTL(FBIOPAN_DISPLAY),
COMPATIBLE_IOCTL(FBIOGET_FCURSORINFO),
COMPATIBLE_IOCTL(FBIOGET_VCURSORINFO),
COMPATIBLE_IOCTL(FBIOPUT_VCURSORINFO),
COMPATIBLE_IOCTL(FBIOGET_CURSORSTATE),
COMPATIBLE_IOCTL(FBIOPUT_CURSORSTATE),
COMPATIBLE_IOCTL(FBIOGET_CON2FBMAP),
COMPATIBLE_IOCTL(FBIOPUT_CON2FBMAP),
#if 0
COMPATIBLE_IOCTL(FBIOBLANK),
#endif
/* Little f */
COMPATIBLE_IOCTL(FIOCLEX),
COMPATIBLE_IOCTL(FIONCLEX),
COMPATIBLE_IOCTL(FIOASYNC),
COMPATIBLE_IOCTL(FIONBIO),
COMPATIBLE_IOCTL(FIONREAD),  /* This is also TIOCINQ */
/* 0x00 */
COMPATIBLE_IOCTL(FIBMAP),
COMPATIBLE_IOCTL(FIGETBSZ),
/* 0x03 -- HD/IDE ioctl's used by hdparm and friends.
 *         Some need translations, these do not.
 */
COMPATIBLE_IOCTL(HDIO_GET_IDENTITY),
COMPATIBLE_IOCTL(HDIO_SET_DMA),
COMPATIBLE_IOCTL(HDIO_SET_KEEPSETTINGS),
COMPATIBLE_IOCTL(HDIO_SET_UNMASKINTR),
COMPATIBLE_IOCTL(HDIO_SET_NOWERR),
COMPATIBLE_IOCTL(HDIO_SET_32BIT),
COMPATIBLE_IOCTL(HDIO_SET_MULTCOUNT),
COMPATIBLE_IOCTL(HDIO_DRIVE_CMD),
COMPATIBLE_IOCTL(HDIO_SET_PIO_MODE),
COMPATIBLE_IOCTL(HDIO_SCAN_HWIF),
COMPATIBLE_IOCTL(HDIO_SET_NICE),
/* 0x02 -- Floppy ioctls */
COMPATIBLE_IOCTL(FDMSGON),
COMPATIBLE_IOCTL(FDMSGOFF),
COMPATIBLE_IOCTL(FDSETEMSGTRESH),
COMPATIBLE_IOCTL(FDFLUSH),
COMPATIBLE_IOCTL(FDWERRORCLR),
COMPATIBLE_IOCTL(FDSETMAXERRS),
COMPATIBLE_IOCTL(FDGETMAXERRS),
COMPATIBLE_IOCTL(FDGETDRVTYP),
COMPATIBLE_IOCTL(FDEJECT),
COMPATIBLE_IOCTL(FDCLRPRM),
COMPATIBLE_IOCTL(FDFMTBEG),
COMPATIBLE_IOCTL(FDFMTEND),
COMPATIBLE_IOCTL(FDRESET),
COMPATIBLE_IOCTL(FDTWADDLE),
COMPATIBLE_IOCTL(FDFMTTRK),
COMPATIBLE_IOCTL(FDRAWCMD),
/* 0x12 */
COMPATIBLE_IOCTL(BLKROSET),
COMPATIBLE_IOCTL(BLKROGET),
COMPATIBLE_IOCTL(BLKRRPART),
COMPATIBLE_IOCTL(BLKFLSBUF),
COMPATIBLE_IOCTL(BLKRASET),
COMPATIBLE_IOCTL(BLKFRASET),
COMPATIBLE_IOCTL(BLKSECTSET),
COMPATIBLE_IOCTL(BLKSSZGET),
COMPATIBLE_IOCTL(BLKBSZGET),
COMPATIBLE_IOCTL(BLKBSZSET),
COMPATIBLE_IOCTL(BLKGETSIZE64),

/* RAID */
COMPATIBLE_IOCTL(RAID_VERSION),
COMPATIBLE_IOCTL(RAID_AUTORUN),
COMPATIBLE_IOCTL(GET_ARRAY_INFO),
COMPATIBLE_IOCTL(GET_DISK_INFO),
COMPATIBLE_IOCTL(PRINT_RAID_DEBUG),
COMPATIBLE_IOCTL(CLEAR_ARRAY),
COMPATIBLE_IOCTL(ADD_NEW_DISK),
COMPATIBLE_IOCTL(HOT_REMOVE_DISK),
COMPATIBLE_IOCTL(SET_ARRAY_INFO),
COMPATIBLE_IOCTL(SET_DISK_INFO),
COMPATIBLE_IOCTL(WRITE_RAID_INFO),
COMPATIBLE_IOCTL(UNPROTECT_ARRAY),
COMPATIBLE_IOCTL(PROTECT_ARRAY),
COMPATIBLE_IOCTL(HOT_ADD_DISK),
COMPATIBLE_IOCTL(SET_DISK_FAULTY),
COMPATIBLE_IOCTL(RUN_ARRAY),
COMPATIBLE_IOCTL(START_ARRAY),
COMPATIBLE_IOCTL(STOP_ARRAY),
COMPATIBLE_IOCTL(STOP_ARRAY_RO),
COMPATIBLE_IOCTL(RESTART_ARRAY_RW),
/* Big K */
COMPATIBLE_IOCTL(PIO_FONT),
COMPATIBLE_IOCTL(GIO_FONT),
COMPATIBLE_IOCTL(KDSIGACCEPT),
COMPATIBLE_IOCTL(KDGETKEYCODE),
COMPATIBLE_IOCTL(KDSETKEYCODE),
COMPATIBLE_IOCTL(KIOCSOUND),
COMPATIBLE_IOCTL(KDMKTONE),
COMPATIBLE_IOCTL(KDGKBTYPE),
COMPATIBLE_IOCTL(KDSETMODE),
COMPATIBLE_IOCTL(KDGETMODE),
COMPATIBLE_IOCTL(KDSKBMODE),
COMPATIBLE_IOCTL(KDGKBMODE),
COMPATIBLE_IOCTL(KDSKBMETA),
COMPATIBLE_IOCTL(KDGKBMETA),
COMPATIBLE_IOCTL(KDGKBENT),
COMPATIBLE_IOCTL(KDSKBENT),
COMPATIBLE_IOCTL(KDGKBSENT),
COMPATIBLE_IOCTL(KDSKBSENT),
COMPATIBLE_IOCTL(KDGKBDIACR),
COMPATIBLE_IOCTL(KDKBDREP),
COMPATIBLE_IOCTL(KDSKBDIACR),
COMPATIBLE_IOCTL(KDGKBLED),
COMPATIBLE_IOCTL(KDSKBLED),
COMPATIBLE_IOCTL(KDGETLED),
COMPATIBLE_IOCTL(KDSETLED),
COMPATIBLE_IOCTL(GIO_SCRNMAP),
COMPATIBLE_IOCTL(PIO_SCRNMAP),
COMPATIBLE_IOCTL(GIO_UNISCRNMAP),
COMPATIBLE_IOCTL(PIO_UNISCRNMAP),
COMPATIBLE_IOCTL(PIO_FONTRESET),
COMPATIBLE_IOCTL(PIO_UNIMAPCLR),
/* Big S */
COMPATIBLE_IOCTL(SCSI_IOCTL_GET_IDLUN),
COMPATIBLE_IOCTL(SCSI_IOCTL_PROBE_HOST),
COMPATIBLE_IOCTL(SCSI_IOCTL_GET_PCI),
COMPATIBLE_IOCTL(SCSI_IOCTL_DOORLOCK),
COMPATIBLE_IOCTL(SCSI_IOCTL_DOORUNLOCK),
COMPATIBLE_IOCTL(SCSI_IOCTL_TEST_UNIT_READY),
COMPATIBLE_IOCTL(SCSI_IOCTL_TAGGED_ENABLE),
COMPATIBLE_IOCTL(SCSI_IOCTL_TAGGED_DISABLE),
COMPATIBLE_IOCTL(SCSI_IOCTL_GET_BUS_NUMBER),
COMPATIBLE_IOCTL(SCSI_IOCTL_SEND_COMMAND),
/* Big T */
COMPATIBLE_IOCTL(TUNSETNOCSUM),
COMPATIBLE_IOCTL(TUNSETDEBUG),
COMPATIBLE_IOCTL(TUNSETIFF),
COMPATIBLE_IOCTL(TUNSETPERSIST),
COMPATIBLE_IOCTL(TUNSETOWNER),
/* Big V */
COMPATIBLE_IOCTL(VT_SETMODE),
COMPATIBLE_IOCTL(VT_GETMODE),
COMPATIBLE_IOCTL(VT_GETSTATE),
COMPATIBLE_IOCTL(VT_OPENQRY),
COMPATIBLE_IOCTL(VT_ACTIVATE),
COMPATIBLE_IOCTL(VT_WAITACTIVE),
COMPATIBLE_IOCTL(VT_RELDISP),
COMPATIBLE_IOCTL(VT_DISALLOCATE),
COMPATIBLE_IOCTL(VT_RESIZE),
COMPATIBLE_IOCTL(VT_RESIZEX),
COMPATIBLE_IOCTL(VT_LOCKSWITCH),
COMPATIBLE_IOCTL(VT_UNLOCKSWITCH),
/* Little v, the video4linux ioctls */
COMPATIBLE_IOCTL(VIDIOCGCAP),
COMPATIBLE_IOCTL(VIDIOCGCHAN),
COMPATIBLE_IOCTL(VIDIOCSCHAN),
COMPATIBLE_IOCTL(VIDIOCGPICT),
COMPATIBLE_IOCTL(VIDIOCSPICT),
COMPATIBLE_IOCTL(VIDIOCCAPTURE),
COMPATIBLE_IOCTL(VIDIOCKEY),
COMPATIBLE_IOCTL(VIDIOCGAUDIO),
COMPATIBLE_IOCTL(VIDIOCSAUDIO),
COMPATIBLE_IOCTL(VIDIOCSYNC),
COMPATIBLE_IOCTL(VIDIOCMCAPTURE),
COMPATIBLE_IOCTL(VIDIOCGMBUF),
COMPATIBLE_IOCTL(VIDIOCGUNIT),
COMPATIBLE_IOCTL(VIDIOCGCAPTURE),
COMPATIBLE_IOCTL(VIDIOCSCAPTURE),
/* BTTV specific... */
COMPATIBLE_IOCTL(_IOW('v',  BASE_VIDIOCPRIVATE+0, char [256])),
COMPATIBLE_IOCTL(_IOR('v',  BASE_VIDIOCPRIVATE+1, char [256])),
COMPATIBLE_IOCTL(_IOR('v' , BASE_VIDIOCPRIVATE+2, unsigned int)),
COMPATIBLE_IOCTL(_IOW('v' , BASE_VIDIOCPRIVATE+3, char [16])), /* struct bttv_pll_info */
COMPATIBLE_IOCTL(_IOR('v' , BASE_VIDIOCPRIVATE+4, int)),
COMPATIBLE_IOCTL(_IOR('v' , BASE_VIDIOCPRIVATE+5, int)),
COMPATIBLE_IOCTL(_IOR('v' , BASE_VIDIOCPRIVATE+6, int)),
COMPATIBLE_IOCTL(_IOR('v' , BASE_VIDIOCPRIVATE+7, int)),
/* Little p (/dev/rtc, /dev/envctrl, etc.) */
COMPATIBLE_IOCTL(_IOR('p', 20, int[7])), /* RTCGET */
COMPATIBLE_IOCTL(_IOW('p', 21, int[7])), /* RTCSET */
COMPATIBLE_IOCTL(RTC_AIE_ON),
COMPATIBLE_IOCTL(RTC_AIE_OFF),
COMPATIBLE_IOCTL(RTC_UIE_ON),
COMPATIBLE_IOCTL(RTC_UIE_OFF),
COMPATIBLE_IOCTL(RTC_PIE_ON),
COMPATIBLE_IOCTL(RTC_PIE_OFF),
COMPATIBLE_IOCTL(RTC_WIE_ON),
COMPATIBLE_IOCTL(RTC_WIE_OFF),
COMPATIBLE_IOCTL(RTC_ALM_SET),
COMPATIBLE_IOCTL(RTC_ALM_READ),
COMPATIBLE_IOCTL(RTC_RD_TIME),
COMPATIBLE_IOCTL(RTC_SET_TIME),
COMPATIBLE_IOCTL(RTC_WKALM_SET),
COMPATIBLE_IOCTL(RTC_WKALM_RD),
/* Little m */
COMPATIBLE_IOCTL(MTIOCTOP),
/* Socket level stuff */
COMPATIBLE_IOCTL(FIOSETOWN),
COMPATIBLE_IOCTL(SIOCSPGRP),
COMPATIBLE_IOCTL(FIOGETOWN),
COMPATIBLE_IOCTL(SIOCGPGRP),
COMPATIBLE_IOCTL(SIOCATMARK),
COMPATIBLE_IOCTL(SIOCSIFLINK),
COMPATIBLE_IOCTL(SIOCSIFENCAP),
COMPATIBLE_IOCTL(SIOCGIFENCAP),
COMPATIBLE_IOCTL(SIOCSIFBR),
COMPATIBLE_IOCTL(SIOCGIFBR),
COMPATIBLE_IOCTL(SIOCSARP),
COMPATIBLE_IOCTL(SIOCGARP),
COMPATIBLE_IOCTL(SIOCDARP),
COMPATIBLE_IOCTL(SIOCSRARP),
COMPATIBLE_IOCTL(SIOCGRARP),
COMPATIBLE_IOCTL(SIOCDRARP),
COMPATIBLE_IOCTL(SIOCADDDLCI),
COMPATIBLE_IOCTL(SIOCDELDLCI),
COMPATIBLE_IOCTL(SIOCGIFVLAN),
COMPATIBLE_IOCTL(SIOCSIFVLAN),
/* SG stuff */
COMPATIBLE_IOCTL(SG_SET_TIMEOUT),
COMPATIBLE_IOCTL(SG_GET_TIMEOUT),
COMPATIBLE_IOCTL(SG_EMULATED_HOST),
COMPATIBLE_IOCTL(SG_SET_TRANSFORM),
COMPATIBLE_IOCTL(SG_GET_TRANSFORM),
COMPATIBLE_IOCTL(SG_SET_RESERVED_SIZE),
COMPATIBLE_IOCTL(SG_GET_RESERVED_SIZE),
COMPATIBLE_IOCTL(SG_GET_SCSI_ID),
COMPATIBLE_IOCTL(SG_SET_FORCE_LOW_DMA),
COMPATIBLE_IOCTL(SG_GET_LOW_DMA),
COMPATIBLE_IOCTL(SG_SET_FORCE_PACK_ID),
COMPATIBLE_IOCTL(SG_GET_PACK_ID),
COMPATIBLE_IOCTL(SG_GET_NUM_WAITING),
COMPATIBLE_IOCTL(SG_SET_DEBUG),
COMPATIBLE_IOCTL(SG_GET_SG_TABLESIZE),
COMPATIBLE_IOCTL(SG_GET_COMMAND_Q),
COMPATIBLE_IOCTL(SG_SET_COMMAND_Q),
COMPATIBLE_IOCTL(SG_GET_VERSION_NUM),
COMPATIBLE_IOCTL(SG_NEXT_CMD_LEN),
COMPATIBLE_IOCTL(SG_SCSI_RESET),
COMPATIBLE_IOCTL(SG_GET_REQUEST_TABLE),
COMPATIBLE_IOCTL(SG_SET_KEEP_ORPHAN),
COMPATIBLE_IOCTL(SG_GET_KEEP_ORPHAN),
/* PPP stuff */
COMPATIBLE_IOCTL(PPPIOCGFLAGS),
COMPATIBLE_IOCTL(PPPIOCSFLAGS),
COMPATIBLE_IOCTL(PPPIOCGASYNCMAP),
COMPATIBLE_IOCTL(PPPIOCSASYNCMAP),
COMPATIBLE_IOCTL(PPPIOCGUNIT),
COMPATIBLE_IOCTL(PPPIOCGRASYNCMAP),
COMPATIBLE_IOCTL(PPPIOCSRASYNCMAP),
COMPATIBLE_IOCTL(PPPIOCGMRU),
COMPATIBLE_IOCTL(PPPIOCSMRU),
COMPATIBLE_IOCTL(PPPIOCSMAXCID),
COMPATIBLE_IOCTL(PPPIOCGXASYNCMAP),
COMPATIBLE_IOCTL(LPGETSTATUS),
COMPATIBLE_IOCTL(PPPIOCSXASYNCMAP),
COMPATIBLE_IOCTL(PPPIOCXFERUNIT),
COMPATIBLE_IOCTL(PPPIOCGNPMODE),
COMPATIBLE_IOCTL(PPPIOCSNPMODE),
COMPATIBLE_IOCTL(PPPIOCGDEBUG),
COMPATIBLE_IOCTL(PPPIOCSDEBUG),
COMPATIBLE_IOCTL(PPPIOCNEWUNIT),
COMPATIBLE_IOCTL(PPPIOCATTACH),
COMPATIBLE_IOCTL(PPPIOCDETACH),
COMPATIBLE_IOCTL(PPPIOCSMRRU),
COMPATIBLE_IOCTL(PPPIOCCONNECT),
COMPATIBLE_IOCTL(PPPIOCDISCONN),
COMPATIBLE_IOCTL(PPPIOCATTCHAN),
COMPATIBLE_IOCTL(PPPIOCGCHAN),
/* PPPOX */
COMPATIBLE_IOCTL(PPPOEIOCSFWD),
COMPATIBLE_IOCTL(PPPOEIOCDFWD),
/* CDROM stuff */
COMPATIBLE_IOCTL(CDROMPAUSE),
COMPATIBLE_IOCTL(CDROMRESUME),
COMPATIBLE_IOCTL(CDROMPLAYMSF),
COMPATIBLE_IOCTL(CDROMPLAYTRKIND),
COMPATIBLE_IOCTL(CDROMREADCOOKED),
COMPATIBLE_IOCTL(CDROMREADMODE1),
COMPATIBLE_IOCTL(CDROMREADMODE2),
COMPATIBLE_IOCTL(CDROMREADRAW),
COMPATIBLE_IOCTL(CDROMREADTOCHDR),
COMPATIBLE_IOCTL(CDROMREADTOCENTRY),
COMPATIBLE_IOCTL(CDROMSTOP),
COMPATIBLE_IOCTL(CDROMSTART),
COMPATIBLE_IOCTL(CDROMEJECT),
COMPATIBLE_IOCTL(CDROMVOLCTRL),
COMPATIBLE_IOCTL(CDROMSUBCHNL),
COMPATIBLE_IOCTL(CDROMEJECT_SW),
COMPATIBLE_IOCTL(CDROMMULTISESSION),
COMPATIBLE_IOCTL(CDROM_GET_MCN),
COMPATIBLE_IOCTL(CDROMRESET),
COMPATIBLE_IOCTL(CDROMVOLREAD),
COMPATIBLE_IOCTL(CDROMSEEK),
COMPATIBLE_IOCTL(CDROMPLAYBLK),
COMPATIBLE_IOCTL(CDROMCLOSETRAY),
COMPATIBLE_IOCTL(CDROM_SET_OPTIONS),
COMPATIBLE_IOCTL(CDROM_CLEAR_OPTIONS),
COMPATIBLE_IOCTL(CDROM_SELECT_SPEED),
COMPATIBLE_IOCTL(CDROM_SELECT_DISC),
COMPATIBLE_IOCTL(CDROM_MEDIA_CHANGED),
COMPATIBLE_IOCTL(CDROM_DRIVE_STATUS),
COMPATIBLE_IOCTL(CDROM_DISC_STATUS),
COMPATIBLE_IOCTL(CDROM_CHANGER_NSLOTS),
COMPATIBLE_IOCTL(CDROM_LOCKDOOR),
COMPATIBLE_IOCTL(CDROM_DEBUG),
COMPATIBLE_IOCTL(CDROM_GET_CAPABILITY),
/* DVD ioctls */
COMPATIBLE_IOCTL(DVD_READ_STRUCT),
COMPATIBLE_IOCTL(DVD_WRITE_STRUCT),
COMPATIBLE_IOCTL(DVD_AUTH),
/* Big L */
COMPATIBLE_IOCTL(LOOP_SET_FD),
COMPATIBLE_IOCTL(LOOP_CLR_FD),
/* Big Q for sound/OSS */
COMPATIBLE_IOCTL(SNDCTL_SEQ_RESET),
COMPATIBLE_IOCTL(SNDCTL_SEQ_SYNC),
COMPATIBLE_IOCTL(SNDCTL_SYNTH_INFO),
COMPATIBLE_IOCTL(SNDCTL_SEQ_CTRLRATE),
COMPATIBLE_IOCTL(SNDCTL_SEQ_GETOUTCOUNT),
COMPATIBLE_IOCTL(SNDCTL_SEQ_GETINCOUNT),
COMPATIBLE_IOCTL(SNDCTL_SEQ_PERCMODE),
COMPATIBLE_IOCTL(SNDCTL_FM_LOAD_INSTR),
COMPATIBLE_IOCTL(SNDCTL_SEQ_TESTMIDI),
COMPATIBLE_IOCTL(SNDCTL_SEQ_RESETSAMPLES),
COMPATIBLE_IOCTL(SNDCTL_SEQ_NRSYNTHS),
COMPATIBLE_IOCTL(SNDCTL_SEQ_NRMIDIS),
COMPATIBLE_IOCTL(SNDCTL_MIDI_INFO),
COMPATIBLE_IOCTL(SNDCTL_SEQ_THRESHOLD),
COMPATIBLE_IOCTL(SNDCTL_SYNTH_MEMAVL),
COMPATIBLE_IOCTL(SNDCTL_FM_4OP_ENABLE),
COMPATIBLE_IOCTL(SNDCTL_SEQ_PANIC),
COMPATIBLE_IOCTL(SNDCTL_SEQ_OUTOFBAND),
COMPATIBLE_IOCTL(SNDCTL_SEQ_GETTIME),
COMPATIBLE_IOCTL(SNDCTL_SYNTH_ID),
COMPATIBLE_IOCTL(SNDCTL_SYNTH_CONTROL),
COMPATIBLE_IOCTL(SNDCTL_SYNTH_REMOVESAMPLE),
/* Big T for sound/OSS */
COMPATIBLE_IOCTL(SNDCTL_TMR_TIMEBASE),
COMPATIBLE_IOCTL(SNDCTL_TMR_START),
COMPATIBLE_IOCTL(SNDCTL_TMR_STOP),
COMPATIBLE_IOCTL(SNDCTL_TMR_CONTINUE),
COMPATIBLE_IOCTL(SNDCTL_TMR_TEMPO),
COMPATIBLE_IOCTL(SNDCTL_TMR_SOURCE),
COMPATIBLE_IOCTL(SNDCTL_TMR_METRONOME),
COMPATIBLE_IOCTL(SNDCTL_TMR_SELECT),
/* Little m for sound/OSS */
COMPATIBLE_IOCTL(SNDCTL_MIDI_PRETIME),
COMPATIBLE_IOCTL(SNDCTL_MIDI_MPUMODE),
COMPATIBLE_IOCTL(SNDCTL_MIDI_MPUCMD),
/* Big P for sound/OSS */
COMPATIBLE_IOCTL(SNDCTL_DSP_RESET),
COMPATIBLE_IOCTL(SNDCTL_DSP_SYNC),
COMPATIBLE_IOCTL(SNDCTL_DSP_SPEED),
COMPATIBLE_IOCTL(SNDCTL_DSP_STEREO),
COMPATIBLE_IOCTL(SNDCTL_DSP_GETBLKSIZE),
COMPATIBLE_IOCTL(SNDCTL_DSP_CHANNELS),
COMPATIBLE_IOCTL(SOUND_PCM_WRITE_FILTER),
COMPATIBLE_IOCTL(SNDCTL_DSP_POST),
COMPATIBLE_IOCTL(SNDCTL_DSP_SUBDIVIDE),
COMPATIBLE_IOCTL(SNDCTL_DSP_SETFRAGMENT),
COMPATIBLE_IOCTL(SNDCTL_DSP_GETFMTS),
COMPATIBLE_IOCTL(SNDCTL_DSP_SETFMT),
COMPATIBLE_IOCTL(SNDCTL_DSP_GETOSPACE),
COMPATIBLE_IOCTL(SNDCTL_DSP_GETISPACE),
COMPATIBLE_IOCTL(SNDCTL_DSP_NONBLOCK),
COMPATIBLE_IOCTL(SNDCTL_DSP_GETCAPS),
COMPATIBLE_IOCTL(SNDCTL_DSP_GETTRIGGER),
COMPATIBLE_IOCTL(SNDCTL_DSP_SETTRIGGER),
COMPATIBLE_IOCTL(SNDCTL_DSP_GETIPTR),
COMPATIBLE_IOCTL(SNDCTL_DSP_GETOPTR),
/* SNDCTL_DSP_MAPINBUF,  XXX needs translation */
/* SNDCTL_DSP_MAPOUTBUF,  XXX needs translation */
COMPATIBLE_IOCTL(SNDCTL_DSP_SETSYNCRO),
COMPATIBLE_IOCTL(SNDCTL_DSP_SETDUPLEX),
COMPATIBLE_IOCTL(SNDCTL_DSP_GETODELAY),
COMPATIBLE_IOCTL(SNDCTL_DSP_PROFILE),
COMPATIBLE_IOCTL(SOUND_PCM_READ_RATE),
COMPATIBLE_IOCTL(SOUND_PCM_READ_CHANNELS),
COMPATIBLE_IOCTL(SOUND_PCM_READ_BITS),
COMPATIBLE_IOCTL(SOUND_PCM_READ_FILTER),
/* Big C for sound/OSS */
COMPATIBLE_IOCTL(SNDCTL_COPR_RESET),
COMPATIBLE_IOCTL(SNDCTL_COPR_LOAD),
COMPATIBLE_IOCTL(SNDCTL_COPR_RDATA),
COMPATIBLE_IOCTL(SNDCTL_COPR_RCODE),
COMPATIBLE_IOCTL(SNDCTL_COPR_WDATA),
COMPATIBLE_IOCTL(SNDCTL_COPR_WCODE),
COMPATIBLE_IOCTL(SNDCTL_COPR_RUN),
COMPATIBLE_IOCTL(SNDCTL_COPR_HALT),
COMPATIBLE_IOCTL(SNDCTL_COPR_SENDMSG),
COMPATIBLE_IOCTL(SNDCTL_COPR_RCVMSG),
/* Big M for sound/OSS */
COMPATIBLE_IOCTL(SOUND_MIXER_READ_VOLUME),
COMPATIBLE_IOCTL(SOUND_MIXER_READ_BASS),
COMPATIBLE_IOCTL(SOUND_MIXER_READ_TREBLE),
COMPATIBLE_IOCTL(SOUND_MIXER_READ_SYNTH),
COMPATIBLE_IOCTL(SOUND_MIXER_READ_PCM),
COMPATIBLE_IOCTL(SOUND_MIXER_READ_SPEAKER),
COMPATIBLE_IOCTL(SOUND_MIXER_READ_LINE),
COMPATIBLE_IOCTL(SOUND_MIXER_READ_MIC),
COMPATIBLE_IOCTL(SOUND_MIXER_READ_CD),
COMPATIBLE_IOCTL(SOUND_MIXER_READ_IMIX),
COMPATIBLE_IOCTL(SOUND_MIXER_READ_ALTPCM),
COMPATIBLE_IOCTL(SOUND_MIXER_READ_RECLEV),
COMPATIBLE_IOCTL(SOUND_MIXER_READ_IGAIN),
COMPATIBLE_IOCTL(SOUND_MIXER_READ_OGAIN),
COMPATIBLE_IOCTL(SOUND_MIXER_READ_LINE1),
COMPATIBLE_IOCTL(SOUND_MIXER_READ_LINE2),
COMPATIBLE_IOCTL(SOUND_MIXER_READ_LINE3),
COMPATIBLE_IOCTL(MIXER_READ(SOUND_MIXER_DIGITAL1)),
COMPATIBLE_IOCTL(MIXER_READ(SOUND_MIXER_DIGITAL2)),
COMPATIBLE_IOCTL(MIXER_READ(SOUND_MIXER_DIGITAL3)),
COMPATIBLE_IOCTL(MIXER_READ(SOUND_MIXER_PHONEIN)),
COMPATIBLE_IOCTL(MIXER_READ(SOUND_MIXER_PHONEOUT)),
COMPATIBLE_IOCTL(MIXER_READ(SOUND_MIXER_VIDEO)),
COMPATIBLE_IOCTL(MIXER_READ(SOUND_MIXER_RADIO)),
COMPATIBLE_IOCTL(MIXER_READ(SOUND_MIXER_MONITOR)),
COMPATIBLE_IOCTL(SOUND_MIXER_READ_MUTE),
/* SOUND_MIXER_READ_ENHANCE,  same value as READ_MUTE */
/* SOUND_MIXER_READ_LOUD,  same value as READ_MUTE */
COMPATIBLE_IOCTL(SOUND_MIXER_READ_RECSRC),
COMPATIBLE_IOCTL(SOUND_MIXER_READ_DEVMASK),
COMPATIBLE_IOCTL(SOUND_MIXER_READ_RECMASK),
COMPATIBLE_IOCTL(SOUND_MIXER_READ_STEREODEVS),
COMPATIBLE_IOCTL(SOUND_MIXER_READ_CAPS),
COMPATIBLE_IOCTL(SOUND_MIXER_WRITE_VOLUME),
COMPATIBLE_IOCTL(SOUND_MIXER_WRITE_BASS),
COMPATIBLE_IOCTL(SOUND_MIXER_WRITE_TREBLE),
COMPATIBLE_IOCTL(SOUND_MIXER_WRITE_SYNTH),
COMPATIBLE_IOCTL(SOUND_MIXER_WRITE_PCM),
COMPATIBLE_IOCTL(SOUND_MIXER_WRITE_SPEAKER),
COMPATIBLE_IOCTL(SOUND_MIXER_WRITE_LINE),
COMPATIBLE_IOCTL(SOUND_MIXER_WRITE_MIC),
COMPATIBLE_IOCTL(SOUND_MIXER_WRITE_CD),
COMPATIBLE_IOCTL(SOUND_MIXER_WRITE_IMIX),
COMPATIBLE_IOCTL(SOUND_MIXER_WRITE_ALTPCM),
COMPATIBLE_IOCTL(SOUND_MIXER_WRITE_RECLEV),
COMPATIBLE_IOCTL(SOUND_MIXER_WRITE_IGAIN),
COMPATIBLE_IOCTL(SOUND_MIXER_WRITE_OGAIN),
COMPATIBLE_IOCTL(SOUND_MIXER_WRITE_LINE1),
COMPATIBLE_IOCTL(SOUND_MIXER_WRITE_LINE2),
COMPATIBLE_IOCTL(SOUND_MIXER_WRITE_LINE3),
COMPATIBLE_IOCTL(MIXER_WRITE(SOUND_MIXER_DIGITAL1)),
COMPATIBLE_IOCTL(MIXER_WRITE(SOUND_MIXER_DIGITAL2)),
COMPATIBLE_IOCTL(MIXER_WRITE(SOUND_MIXER_DIGITAL3)),
COMPATIBLE_IOCTL(MIXER_WRITE(SOUND_MIXER_PHONEIN)),
COMPATIBLE_IOCTL(MIXER_WRITE(SOUND_MIXER_PHONEOUT)),
COMPATIBLE_IOCTL(MIXER_WRITE(SOUND_MIXER_VIDEO)),
COMPATIBLE_IOCTL(MIXER_WRITE(SOUND_MIXER_RADIO)),
COMPATIBLE_IOCTL(MIXER_WRITE(SOUND_MIXER_MONITOR)),
COMPATIBLE_IOCTL(SOUND_MIXER_WRITE_MUTE),
/* SOUND_MIXER_WRITE_ENHANCE,  same value as WRITE_MUTE */
/* SOUND_MIXER_WRITE_LOUD,  same value as WRITE_MUTE */
COMPATIBLE_IOCTL(SOUND_MIXER_WRITE_RECSRC),
COMPATIBLE_IOCTL(SOUND_MIXER_INFO),
COMPATIBLE_IOCTL(SOUND_OLD_MIXER_INFO),
COMPATIBLE_IOCTL(SOUND_MIXER_ACCESS),
COMPATIBLE_IOCTL(SOUND_MIXER_PRIVATE1),
COMPATIBLE_IOCTL(SOUND_MIXER_PRIVATE2),
COMPATIBLE_IOCTL(SOUND_MIXER_PRIVATE3),
COMPATIBLE_IOCTL(SOUND_MIXER_PRIVATE4),
COMPATIBLE_IOCTL(SOUND_MIXER_PRIVATE5),
COMPATIBLE_IOCTL(SOUND_MIXER_GETLEVELS),
COMPATIBLE_IOCTL(SOUND_MIXER_SETLEVELS),
COMPATIBLE_IOCTL(OSS_GETVERSION),
/* AUTOFS */
COMPATIBLE_IOCTL(AUTOFS_IOC_READY),
COMPATIBLE_IOCTL(AUTOFS_IOC_FAIL),
COMPATIBLE_IOCTL(AUTOFS_IOC_CATATONIC),
COMPATIBLE_IOCTL(AUTOFS_IOC_PROTOVER),
COMPATIBLE_IOCTL(AUTOFS_IOC_EXPIRE),
/* DEVFS */
COMPATIBLE_IOCTL(DEVFSDIOC_GET_PROTO_REV),
COMPATIBLE_IOCTL(DEVFSDIOC_SET_EVENT_MASK),
COMPATIBLE_IOCTL(DEVFSDIOC_RELEASE_EVENT_QUEUE),
COMPATIBLE_IOCTL(DEVFSDIOC_SET_DEBUG_MASK),
/* Raw devices */
COMPATIBLE_IOCTL(RAW_SETBIND),
COMPATIBLE_IOCTL(RAW_GETBIND),
/* SMB ioctls which do not need any translations */
COMPATIBLE_IOCTL(SMB_IOC_NEWCONN),
/* Little a */
COMPATIBLE_IOCTL(ATMSIGD_CTRL),
COMPATIBLE_IOCTL(ATMARPD_CTRL),
COMPATIBLE_IOCTL(ATMLEC_CTRL),
COMPATIBLE_IOCTL(ATMLEC_MCAST),
COMPATIBLE_IOCTL(ATMLEC_DATA),
COMPATIBLE_IOCTL(ATM_SETSC),
COMPATIBLE_IOCTL(SIOCSIFATMTCP),
COMPATIBLE_IOCTL(SIOCMKCLIP),
COMPATIBLE_IOCTL(ATMARP_MKIP),
COMPATIBLE_IOCTL(ATMARP_SETENTRY),
COMPATIBLE_IOCTL(ATMARP_ENCAP),
COMPATIBLE_IOCTL(ATMTCP_CREATE),
COMPATIBLE_IOCTL(ATMTCP_REMOVE),
COMPATIBLE_IOCTL(ATMMPC_CTRL),
COMPATIBLE_IOCTL(ATMMPC_DATA),
#if defined(CONFIG_BLK_DEV_LVM) || defined(CONFIG_BLK_DEV_LVM_MODULE)
/* 0xfe - lvm */
COMPATIBLE_IOCTL(VG_SET_EXTENDABLE),
COMPATIBLE_IOCTL(VG_STATUS_GET_COUNT),
COMPATIBLE_IOCTL(VG_STATUS_GET_NAMELIST),
COMPATIBLE_IOCTL(VG_REMOVE),
COMPATIBLE_IOCTL(VG_RENAME),
COMPATIBLE_IOCTL(VG_REDUCE),
COMPATIBLE_IOCTL(PE_LOCK_UNLOCK),
COMPATIBLE_IOCTL(PV_FLUSH),
COMPATIBLE_IOCTL(LVM_LOCK_LVM),
COMPATIBLE_IOCTL(LVM_GET_IOP_VERSION),
#ifdef LVM_TOTAL_RESET
COMPATIBLE_IOCTL(LVM_RESET),
#endif
COMPATIBLE_IOCTL(LV_SET_ACCESS),
COMPATIBLE_IOCTL(LV_SET_STATUS),
COMPATIBLE_IOCTL(LV_SET_ALLOCATION),
COMPATIBLE_IOCTL(LE_REMAP),
COMPATIBLE_IOCTL(LV_BMAP),
COMPATIBLE_IOCTL(LV_SNAPSHOT_USE_RATE),
#endif /* LVM */
#if defined(CONFIG_DRM) || defined(CONFIG_DRM_MODULE)
COMPATIBLE_IOCTL(DRM_IOCTL_GET_MAGIC),
COMPATIBLE_IOCTL(DRM_IOCTL_IRQ_BUSID),
COMPATIBLE_IOCTL(DRM_IOCTL_AUTH_MAGIC),
COMPATIBLE_IOCTL(DRM_IOCTL_BLOCK),
COMPATIBLE_IOCTL(DRM_IOCTL_UNBLOCK),
COMPATIBLE_IOCTL(DRM_IOCTL_CONTROL),
COMPATIBLE_IOCTL(DRM_IOCTL_ADD_BUFS),
COMPATIBLE_IOCTL(DRM_IOCTL_MARK_BUFS),
COMPATIBLE_IOCTL(DRM_IOCTL_ADD_CTX),
COMPATIBLE_IOCTL(DRM_IOCTL_RM_CTX),
COMPATIBLE_IOCTL(DRM_IOCTL_MOD_CTX),
COMPATIBLE_IOCTL(DRM_IOCTL_GET_CTX),
COMPATIBLE_IOCTL(DRM_IOCTL_SWITCH_CTX),
COMPATIBLE_IOCTL(DRM_IOCTL_NEW_CTX),
COMPATIBLE_IOCTL(DRM_IOCTL_ADD_DRAW),
COMPATIBLE_IOCTL(DRM_IOCTL_RM_DRAW),
COMPATIBLE_IOCTL(DRM_IOCTL_LOCK),
COMPATIBLE_IOCTL(DRM_IOCTL_UNLOCK),
COMPATIBLE_IOCTL(DRM_IOCTL_FINISH),
#endif /* DRM */
/* elevator */
COMPATIBLE_IOCTL(BLKELVGET),
COMPATIBLE_IOCTL(BLKELVSET),
/* Big W */
/* WIOC_GETSUPPORT not yet implemented -E */
COMPATIBLE_IOCTL(WDIOC_GETSTATUS),
COMPATIBLE_IOCTL(WDIOC_GETBOOTSTATUS),
COMPATIBLE_IOCTL(WDIOC_GETTEMP),
COMPATIBLE_IOCTL(WDIOC_SETOPTIONS),
COMPATIBLE_IOCTL(WDIOC_KEEPALIVE),
/* Big R */
COMPATIBLE_IOCTL(RNDGETENTCNT),
COMPATIBLE_IOCTL(RNDADDTOENTCNT),
COMPATIBLE_IOCTL(RNDGETPOOL),
COMPATIBLE_IOCTL(RNDADDENTROPY),
COMPATIBLE_IOCTL(RNDZAPENTCNT),
COMPATIBLE_IOCTL(RNDCLEARPOOL),
/* Bluetooth ioctls */
COMPATIBLE_IOCTL(HCIDEVUP),
COMPATIBLE_IOCTL(HCIDEVDOWN),
COMPATIBLE_IOCTL(HCIDEVRESET),
COMPATIBLE_IOCTL(HCIDEVRESTAT),
COMPATIBLE_IOCTL(HCIGETDEVINFO),
COMPATIBLE_IOCTL(HCIGETDEVLIST),
COMPATIBLE_IOCTL(HCISETRAW),
COMPATIBLE_IOCTL(HCISETSCAN),
COMPATIBLE_IOCTL(HCISETAUTH),
COMPATIBLE_IOCTL(HCIINQUIRY),
COMPATIBLE_IOCTL(PCIIOC_CONTROLLER),
COMPATIBLE_IOCTL(PCIIOC_MMAP_IS_IO),
COMPATIBLE_IOCTL(PCIIOC_MMAP_IS_MEM),
COMPATIBLE_IOCTL(PCIIOC_WRITE_COMBINE),
/* USB */
COMPATIBLE_IOCTL(USBDEVFS_RESETEP),
COMPATIBLE_IOCTL(USBDEVFS_SETINTERFACE),
COMPATIBLE_IOCTL(USBDEVFS_SETCONFIGURATION),
COMPATIBLE_IOCTL(USBDEVFS_GETDRIVER),
COMPATIBLE_IOCTL(USBDEVFS_DISCARDURB),
COMPATIBLE_IOCTL(USBDEVFS_CLAIMINTERFACE),
COMPATIBLE_IOCTL(USBDEVFS_RELEASEINTERFACE),
COMPATIBLE_IOCTL(USBDEVFS_CONNECTINFO),
COMPATIBLE_IOCTL(USBDEVFS_HUB_PORTINFO),
COMPATIBLE_IOCTL(USBDEVFS_RESET),
COMPATIBLE_IOCTL(USBDEVFS_CLEAR_HALT),
/* MTD */
COMPATIBLE_IOCTL(MEMGETINFO),
COMPATIBLE_IOCTL(MEMERASE),
COMPATIBLE_IOCTL(MEMLOCK),
COMPATIBLE_IOCTL(MEMUNLOCK),
COMPATIBLE_IOCTL(MEMGETREGIONCOUNT),
COMPATIBLE_IOCTL(MEMGETREGIONINFO),
/* NBD */
COMPATIBLE_IOCTL(NBD_SET_SOCK),
COMPATIBLE_IOCTL(NBD_SET_BLKSIZE),
COMPATIBLE_IOCTL(NBD_SET_SIZE),
COMPATIBLE_IOCTL(NBD_DO_IT),
COMPATIBLE_IOCTL(NBD_CLEAR_SOCK),
COMPATIBLE_IOCTL(NBD_CLEAR_QUE),
COMPATIBLE_IOCTL(NBD_PRINT_DEBUG),
COMPATIBLE_IOCTL(NBD_SET_SIZE_BLOCKS),
COMPATIBLE_IOCTL(NBD_DISCONNECT),
/* Remove *PRIVATE in 2.5 */
COMPATIBLE_IOCTL(SIOCDEVPRIVATE),
COMPATIBLE_IOCTL(SIOCDEVPRIVATE+1),
COMPATIBLE_IOCTL(SIOCDEVPRIVATE+2),
COMPATIBLE_IOCTL(SIOCGMIIPHY),
COMPATIBLE_IOCTL(SIOCGMIIREG),
COMPATIBLE_IOCTL(SIOCSMIIREG),
/* And these ioctls need translation */
HANDLE_IOCTL(MEMREADOOB32, mtd_rw_oob),
HANDLE_IOCTL(MEMWRITEOOB32, mtd_rw_oob),
#ifdef CONFIG_NET
HANDLE_IOCTL(SIOCGIFNAME, dev_ifname32),
#endif
HANDLE_IOCTL(SIOCGIFCONF, dev_ifconf),
HANDLE_IOCTL(SIOCGIFFLAGS, dev_ifsioc),
HANDLE_IOCTL(SIOCSIFFLAGS, dev_ifsioc),
HANDLE_IOCTL(SIOCGIFMETRIC, dev_ifsioc),
HANDLE_IOCTL(SIOCSIFMETRIC, dev_ifsioc),
HANDLE_IOCTL(SIOCGIFMTU, dev_ifsioc),
HANDLE_IOCTL(SIOCSIFMTU, dev_ifsioc),
HANDLE_IOCTL(SIOCGIFMEM, dev_ifsioc),
HANDLE_IOCTL(SIOCSIFMEM, dev_ifsioc),
HANDLE_IOCTL(SIOCGIFHWADDR, dev_ifsioc),
HANDLE_IOCTL(SIOCSIFHWADDR, dev_ifsioc),
HANDLE_IOCTL(SIOCADDMULTI, dev_ifsioc),
HANDLE_IOCTL(SIOCDELMULTI, dev_ifsioc),
HANDLE_IOCTL(SIOCGIFINDEX, dev_ifsioc),
HANDLE_IOCTL(SIOCGIFMAP, dev_ifsioc),
HANDLE_IOCTL(SIOCSIFMAP, dev_ifsioc),
HANDLE_IOCTL(SIOCGIFADDR, dev_ifsioc),
HANDLE_IOCTL(SIOCSIFADDR, dev_ifsioc),
HANDLE_IOCTL(SIOCGIFBRDADDR, dev_ifsioc),
HANDLE_IOCTL(SIOCSIFBRDADDR, dev_ifsioc),
HANDLE_IOCTL(SIOCGIFDSTADDR, dev_ifsioc),
HANDLE_IOCTL(SIOCSIFDSTADDR, dev_ifsioc),
HANDLE_IOCTL(SIOCGIFNETMASK, dev_ifsioc),
HANDLE_IOCTL(SIOCSIFNETMASK, dev_ifsioc),
HANDLE_IOCTL(SIOCSIFPFLAGS, dev_ifsioc),
HANDLE_IOCTL(SIOCGIFPFLAGS, dev_ifsioc),
HANDLE_IOCTL(SIOCGIFTXQLEN, dev_ifsioc),
HANDLE_IOCTL(SIOCSIFTXQLEN, dev_ifsioc),
HANDLE_IOCTL(SIOCETHTOOL, ethtool_ioctl),
HANDLE_IOCTL(SIOCBONDENSLAVE, bond_ioctl),
HANDLE_IOCTL(SIOCBONDRELEASE, bond_ioctl),
HANDLE_IOCTL(SIOCBONDSETHWADDR, bond_ioctl),
HANDLE_IOCTL(SIOCBONDSLAVEINFOQUERY, bond_ioctl),
HANDLE_IOCTL(SIOCBONDINFOQUERY, bond_ioctl),
HANDLE_IOCTL(SIOCBONDCHANGEACTIVE, bond_ioctl),
HANDLE_IOCTL(SIOCADDRT, routing_ioctl),
HANDLE_IOCTL(SIOCDELRT, routing_ioctl),
/* Note SIOCRTMSG is no longer, so this is safe and
 * the user would have seen just an -EINVAL anyways. */
HANDLE_IOCTL(SIOCRTMSG, ret_einval),
HANDLE_IOCTL(SIOCGSTAMP, do_siocgstamp),
HANDLE_IOCTL(HDIO_GETGEO, hdio_getgeo),
HANDLE_IOCTL(HDIO_GETGEO_BIG, hdio_getgeo_big),
HANDLE_IOCTL(HDIO_GETGEO_BIG_RAW, hdio_getgeo_big),
HANDLE_IOCTL(BLKRAGET, w_long),
HANDLE_IOCTL(BLKGETSIZE, w_long),
HANDLE_IOCTL(0x1260, broken_blkgetsize),
HANDLE_IOCTL(BLKFRAGET, w_long),
HANDLE_IOCTL(BLKSECTGET, w_long),
HANDLE_IOCTL(BLKPG, blkpg_ioctl_trans),
HANDLE_IOCTL(HDIO_GET_KEEPSETTINGS, hdio_ioctl_trans),
HANDLE_IOCTL(HDIO_GET_UNMASKINTR, hdio_ioctl_trans),
HANDLE_IOCTL(HDIO_GET_DMA, hdio_ioctl_trans),
HANDLE_IOCTL(HDIO_GET_32BIT, hdio_ioctl_trans),
HANDLE_IOCTL(HDIO_GET_MULTCOUNT, hdio_ioctl_trans),
HANDLE_IOCTL(HDIO_GET_NOWERR, hdio_ioctl_trans),
HANDLE_IOCTL(HDIO_GET_NICE, hdio_ioctl_trans),
HANDLE_IOCTL(FDSETPRM32, fd_ioctl_trans),
HANDLE_IOCTL(FDDEFPRM32, fd_ioctl_trans),
HANDLE_IOCTL(FDGETPRM32, fd_ioctl_trans),
HANDLE_IOCTL(FDSETDRVPRM32, fd_ioctl_trans),
HANDLE_IOCTL(FDGETDRVPRM32, fd_ioctl_trans),
HANDLE_IOCTL(FDGETDRVSTAT32, fd_ioctl_trans),
HANDLE_IOCTL(FDPOLLDRVSTAT32, fd_ioctl_trans),
HANDLE_IOCTL(FDGETFDCSTAT32, fd_ioctl_trans),
HANDLE_IOCTL(FDWERRORGET32, fd_ioctl_trans),
HANDLE_IOCTL(SG_IO,sg_ioctl_trans),
HANDLE_IOCTL(PPPIOCGIDLE32, ppp_ioctl_trans),
HANDLE_IOCTL(PPPIOCSCOMPRESS32, ppp_ioctl_trans),
HANDLE_IOCTL(MTIOCGET32, mt_ioctl_trans),
HANDLE_IOCTL(MTIOCPOS32, mt_ioctl_trans),
HANDLE_IOCTL(MTIOCGETCONFIG32, mt_ioctl_trans),
HANDLE_IOCTL(MTIOCSETCONFIG32, mt_ioctl_trans),
HANDLE_IOCTL(CDROMREADAUDIO, cdrom_ioctl_trans),
HANDLE_IOCTL(CDROMREADALL, cdrom_ioctl_trans),
HANDLE_IOCTL(CDROM_SEND_PACKET, cdrom_ioctl_trans),
HANDLE_IOCTL(LOOP_SET_STATUS, loop_status),
HANDLE_IOCTL(LOOP_GET_STATUS, loop_status),
HANDLE_IOCTL(AUTOFS_IOC_SETTIMEOUT32, ioc_settimeout),
#ifdef CONFIG_VT
HANDLE_IOCTL(PIO_FONTX, do_fontx_ioctl),
HANDLE_IOCTL(GIO_FONTX, do_fontx_ioctl),
HANDLE_IOCTL(PIO_UNIMAP, do_unimap_ioctl),
HANDLE_IOCTL(GIO_UNIMAP, do_unimap_ioctl),
HANDLE_IOCTL(KDFONTOP, do_kdfontop_ioctl),
HANDLE_IOCTL(FBIOGET_FSCREENINFO, do_fbioget_fscreeninfo_ioctl),
HANDLE_IOCTL(FBIOGETCMAP, do_fbiogetcmap_ioctl),
HANDLE_IOCTL(FBIOPUTCMAP, do_fbioputcmap_ioctl),
#endif
HANDLE_IOCTL(EXT2_IOC32_GETFLAGS, do_ext2_ioctl),
HANDLE_IOCTL(EXT2_IOC32_SETFLAGS, do_ext2_ioctl),
HANDLE_IOCTL(EXT2_IOC32_GETVERSION, do_ext2_ioctl),
HANDLE_IOCTL(EXT2_IOC32_SETVERSION, do_ext2_ioctl),
HANDLE_IOCTL(VIDIOCGTUNER32, do_video_ioctl),
HANDLE_IOCTL(VIDIOCSTUNER32, do_video_ioctl),
HANDLE_IOCTL(VIDIOCGWIN32, do_video_ioctl),
HANDLE_IOCTL(VIDIOCSWIN32, do_video_ioctl),
HANDLE_IOCTL(VIDIOCGFBUF32, do_video_ioctl),
HANDLE_IOCTL(VIDIOCSFBUF32, do_video_ioctl),
HANDLE_IOCTL(VIDIOCGFREQ32, do_video_ioctl),
HANDLE_IOCTL(VIDIOCSFREQ32, do_video_ioctl),
/* One SMB ioctl needs translations. */
HANDLE_IOCTL(SMB_IOC_GETMOUNTUID_32, do_smb_getmountuid),
HANDLE_IOCTL(ATM_GETLINKRATE32, do_atm_ioctl),
HANDLE_IOCTL(ATM_GETNAMES32, do_atm_ioctl),
HANDLE_IOCTL(ATM_GETTYPE32, do_atm_ioctl),
HANDLE_IOCTL(ATM_GETESI32, do_atm_ioctl),
HANDLE_IOCTL(ATM_GETADDR32, do_atm_ioctl),
HANDLE_IOCTL(ATM_RSTADDR32, do_atm_ioctl),
HANDLE_IOCTL(ATM_ADDADDR32, do_atm_ioctl),
HANDLE_IOCTL(ATM_DELADDR32, do_atm_ioctl),
HANDLE_IOCTL(ATM_GETCIRANGE32, do_atm_ioctl),
HANDLE_IOCTL(ATM_SETCIRANGE32, do_atm_ioctl),
HANDLE_IOCTL(ATM_SETESI32, do_atm_ioctl),
HANDLE_IOCTL(ATM_SETESIF32, do_atm_ioctl),
HANDLE_IOCTL(ATM_GETSTAT32, do_atm_ioctl),
HANDLE_IOCTL(ATM_GETSTATZ32, do_atm_ioctl),
HANDLE_IOCTL(ATM_GETLOOP32, do_atm_ioctl),
HANDLE_IOCTL(ATM_SETLOOP32, do_atm_ioctl),
HANDLE_IOCTL(ATM_QUERYLOOP32, do_atm_ioctl),
HANDLE_IOCTL(SONET_GETSTAT, do_atm_ioctl),
HANDLE_IOCTL(SONET_GETSTATZ, do_atm_ioctl),
HANDLE_IOCTL(SONET_GETDIAG, do_atm_ioctl),
HANDLE_IOCTL(SONET_SETDIAG, do_atm_ioctl),
HANDLE_IOCTL(SONET_CLRDIAG, do_atm_ioctl),
HANDLE_IOCTL(SONET_SETFRAMING, do_atm_ioctl),
HANDLE_IOCTL(SONET_GETFRAMING, do_atm_ioctl),
HANDLE_IOCTL(SONET_GETFRSENSE, do_atm_ioctl),
#if defined(CONFIG_BLK_DEV_LVM) || defined(CONFIG_BLK_DEV_LVM_MODULE)
HANDLE_IOCTL(VG_STATUS, do_lvm_ioctl),
HANDLE_IOCTL(VG_CREATE_OLD, do_lvm_ioctl),
HANDLE_IOCTL(VG_CREATE, do_lvm_ioctl),
HANDLE_IOCTL(VG_EXTEND, do_lvm_ioctl),
HANDLE_IOCTL(LV_CREATE, do_lvm_ioctl),
HANDLE_IOCTL(LV_REMOVE, do_lvm_ioctl),
HANDLE_IOCTL(LV_EXTEND, do_lvm_ioctl),
HANDLE_IOCTL(LV_REDUCE, do_lvm_ioctl),
HANDLE_IOCTL(LV_RENAME, do_lvm_ioctl),
HANDLE_IOCTL(LV_STATUS_BYNAME, do_lvm_ioctl),
HANDLE_IOCTL(LV_STATUS_BYINDEX, do_lvm_ioctl),
HANDLE_IOCTL(LV_STATUS_BYDEV, do_lvm_ioctl),
HANDLE_IOCTL(PV_CHANGE, do_lvm_ioctl),
HANDLE_IOCTL(PV_STATUS, do_lvm_ioctl),
#endif /* LVM */
#if defined(CONFIG_DRM) || defined(CONFIG_DRM_MODULE)
HANDLE_IOCTL(DRM32_IOCTL_VERSION, drm32_version),
HANDLE_IOCTL(DRM32_IOCTL_GET_UNIQUE, drm32_getsetunique),
HANDLE_IOCTL(DRM32_IOCTL_SET_UNIQUE, drm32_getsetunique),
HANDLE_IOCTL(DRM32_IOCTL_ADD_MAP, drm32_addmap),
HANDLE_IOCTL(DRM32_IOCTL_INFO_BUFS, drm32_info_bufs),
HANDLE_IOCTL(DRM32_IOCTL_FREE_BUFS, drm32_free_bufs),
HANDLE_IOCTL(DRM32_IOCTL_MAP_BUFS, drm32_map_bufs),
HANDLE_IOCTL(DRM32_IOCTL_DMA, drm32_dma),
HANDLE_IOCTL(DRM32_IOCTL_RES_CTX, drm32_res_ctx),
#endif /* DRM */
HANDLE_IOCTL(USBDEVFS_CONTROL32, do_usbdevfs_control),
HANDLE_IOCTL(USBDEVFS_BULK32, do_usbdevfs_bulk),
/*HANDLE_IOCTL(USBDEVFS_SUBMITURB32, do_usbdevfs_urb)*/
HANDLE_IOCTL(USBDEVFS_REAPURB32, do_usbdevfs_reapurb),
HANDLE_IOCTL(USBDEVFS_REAPURBNDELAY32, do_usbdevfs_reapurb),
HANDLE_IOCTL(USBDEVFS_DISCSIGNAL32, do_usbdevfs_discsignal),
/* take care of sizeof(sizeof()) breakage */
/* elevator */
HANDLE_IOCTL(BLKELVGET_32, do_blkelvget),
HANDLE_IOCTL(BLKELVSET_32, do_blkelvset),
/* block stuff */
HANDLE_IOCTL(BLKBSZGET_32, do_blkbszget),
HANDLE_IOCTL(BLKBSZSET_32, do_blkbszset),
HANDLE_IOCTL(BLKGETSIZE64_32, do_blkgetsize64),
};

unsigned long ioctl32_hash_table[1024];

static inline unsigned long ioctl32_hash(unsigned long cmd)
{
	return ((cmd >> 6) ^ (cmd >> 4) ^ cmd) & 0x3ff;
}

static void ioctl32_insert_translation(struct ioctl_trans *trans)
{
	unsigned long hash;
	struct ioctl_trans *t;

	hash = ioctl32_hash (trans->cmd);
	if (!ioctl32_hash_table[hash])
		ioctl32_hash_table[hash] = (long)trans;
	else {
		t = (struct ioctl_trans *)ioctl32_hash_table[hash];
		while (t->next)
			t = (struct ioctl_trans *)(long)t->next;
		trans->next = 0;
		t->next = (long)trans;
	}
}

static int __init init_sys32_ioctl(void)
{
        int i, size = sizeof(ioctl_translations) / sizeof(struct ioctl_trans);
        for (i=0; i < size ;i++)
                ioctl32_insert_translation(&ioctl_translations[i]);
	return 0;
}

__initcall(init_sys32_ioctl);

static struct ioctl_trans *additional_ioctls;

/* Always call these with kernel lock held! */

int register_ioctl32_conversion(unsigned int cmd, int (*handler)(unsigned int, unsigned int, unsigned long, struct file *))
{
	int i;
	if (!additional_ioctls) {
		additional_ioctls = module_map(PAGE_SIZE);
		if (!additional_ioctls)
			return -ENOMEM;
		memset(additional_ioctls, 0, PAGE_SIZE);
	}
	for (i = 0; i < PAGE_SIZE/sizeof(struct ioctl_trans); i++)
		if (!additional_ioctls[i].cmd)
			break;
	if (i == PAGE_SIZE/sizeof(struct ioctl_trans))
		return -ENOMEM;
	additional_ioctls[i].cmd = cmd;
	if (!handler)
		additional_ioctls[i].handler = (long)sys_ioctl;
	else
		additional_ioctls[i].handler = (long)handler;
	ioctl32_insert_translation(&additional_ioctls[i]);
	return 0;
}

int unregister_ioctl32_conversion(unsigned int cmd)
{
	unsigned long hash = ioctl32_hash(cmd);
	struct ioctl_trans *t, *t1;

	t = (struct ioctl_trans *)ioctl32_hash_table[hash];
	if (!t) return -EINVAL;
	if (t->cmd == cmd && t >= additional_ioctls &&
	    (unsigned long)t < ((unsigned long)additional_ioctls) + PAGE_SIZE) {
		ioctl32_hash_table[hash] = t->next;
		t->cmd = 0;
		return 0;
	} else while (t->next) {
		t1 = (struct ioctl_trans *)t->next;
		if (t1->cmd == cmd && t1 >= additional_ioctls &&
		    (unsigned long)t1 < ((unsigned long)additional_ioctls) + PAGE_SIZE) {
			t1->cmd = 0;
			t->next = t1->next;
			return 0;
		}
		t = t1;
	}
	return -EINVAL;
}

asmlinkage int sys32_ioctl(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	struct file * filp;
	int error = -EBADF;
	int (*handler)(unsigned int, unsigned int, unsigned long, struct file * filp);
	struct ioctl_trans *t;

	filp = fget(fd);
	if (!filp)
		goto out2;

	if (!filp->f_op || !filp->f_op->ioctl) {
		error = sys_ioctl (fd, cmd, arg);
		goto out;
	}

	t = (struct ioctl_trans *)ioctl32_hash_table [ioctl32_hash (cmd)];

	while (t && t->cmd != cmd)
		t = (struct ioctl_trans *)t->next;
	if (t) {
		handler = (void *)t->handler;
		error = handler(fd, cmd, arg, filp);
	} else {
		static int count = 0;
		if (++count <= 20)
			printk("sys32_ioctl(%s:%d): Unknown cmd fd(%d) "
			       "cmd(%08x) arg(%08x)\n",
			       current->comm, current->pid,
			       (int)fd, (unsigned int)cmd, (unsigned int)arg);
		error = -EINVAL;
	}
out:
	fput(filp);
out2:
	return error;
}
