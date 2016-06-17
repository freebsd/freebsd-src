/*
 * ioctl32.c: Conversion between 32bit and 64bit native ioctls.
 *
 * Copyright (C) 2000 Silicon Graphics, Inc.
 * Written by Ulf Carlsson (ulfc@engr.sgi.com)
 * Copyright (C) 2000 Ralf Baechle
 * Copyright (C) 2002, 2003  Maciej W. Rozycki
 *
 * Mostly stolen from the sparc64 ioctl32 implementation.
 */
#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/sched.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/ioctl.h>
#include <linux/if.h>
#include <linux/slab.h>
#include <linux/hdreg.h>
#include <linux/raid/md_u.h>
#include <linux/kd.h>
#include <linux/route.h>
#include <linux/vt.h>
#include <linux/fs.h>
#include <linux/ppp_defs.h>
#include <linux/if_ppp.h>
#include <linux/if_pppox.h>
#include <linux/if_tun.h>
#include <linux/mtio.h>
#include <linux/cdrom.h>
#include <linux/loop.h>
#include <linux/auto_fs.h>
#include <linux/auto_fs4.h>
#include <linux/devfs_fs.h>
#include <linux/tty.h>
#include <linux/vt_kern.h>
#include <linux/fb.h>
#include <linux/ext2_fs.h>
#include <linux/videodev.h>
#include <linux/netdevice.h>
#include <linux/raw.h>
#include <linux/blkpg.h>
#include <linux/blk.h>
#include <linux/elevator.h>
#include <linux/rtc.h>
#include <linux/pci.h>
#if defined(CONFIG_BLK_DEV_LVM) || defined(CONFIG_BLK_DEV_LVM_MODULE)
#include <linux/lvm.h>
#endif /* LVM */

#include <scsi/scsi.h>
#undef __KERNEL__		/* This file was born to be ugly ...  */
#include <scsi/scsi_ioctl.h>
#define __KERNEL__
#include <scsi/sg.h>

#include <asm/types.h>
#include <asm/uaccess.h>
#include <linux/soundcard.h>

#include <linux/mtd/mtd.h>
#include <linux/serial.h>

#ifdef CONFIG_SIBYTE_TBPROF
#include <asm/sibyte/trace_prof.h>
#endif

long sys_ioctl(unsigned int fd, unsigned int cmd, unsigned long arg);

static int w_long(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	mm_segment_t old_fs = get_fs();
	int err;
	unsigned long val;

	set_fs (KERNEL_DS);
	err = sys_ioctl(fd, cmd, (unsigned long)&val);
	set_fs (old_fs);
	if (!err && put_user((unsigned int) val, (u32 *)arg))
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
	set_fs(KERNEL_DS);
	err = sys_ioctl(fd, cmd, (unsigned long)&val);
	set_fs (old_fs);
	if (!err && put_user(val, (u32 *)arg))
		return -EFAULT;
	return err;
}

#define A(__x) ((unsigned long)(__x))


#ifdef CONFIG_FB

struct fb_fix_screeninfo32 {
	char id[16];			/* identification string eg "TT Builtin" */
	__u32 smem_start;		/* Start of frame buffer mem */
					/* (physical address) */
	__u32 smem_len;			/* Length of frame buffer mem */
	__u32 type;			/* see FB_TYPE_*		*/
	__u32 type_aux;			/* Interleave for interleaved Planes */
	__u32 visual;			/* see FB_VISUAL_*		*/ 
	__u16 xpanstep;			/* zero if no hardware panning  */
	__u16 ypanstep;			/* zero if no hardware panning  */
	__u16 ywrapstep;		/* zero if no hardware ywrap    */
	__u32 line_length;		/* length of a line in bytes    */
	__u32 mmio_start;		/* Start of Memory Mapped I/O   */
					/* (physical address) */
	__u32 mmio_len;			/* Length of Memory Mapped I/O  */
	__u32 accel;			/* Type of acceleration available */
	__u16 reserved[3];		/* Reserved for future compatibility */
};

static int do_fbioget_fscreeninfo_ioctl(unsigned int fd, unsigned int cmd,
					unsigned long arg)
{
	mm_segment_t old_fs = get_fs();
	struct fb_fix_screeninfo fix;
	struct fb_fix_screeninfo32 *fix32 = (struct fb_fix_screeninfo32 *)arg;
	int err;

	set_fs(KERNEL_DS);
	err = sys_ioctl(fd, cmd, (unsigned long)&fix);
	set_fs(old_fs);

	if (err == 0) {
		err = __copy_to_user((char *)fix32->id, (char *)fix.id,
				     sizeof(fix.id));
		err |= __put_user((__u32)(unsigned long)fix.smem_start,
				  &fix32->smem_start);
		err |= __put_user(fix.smem_len, &fix32->smem_len);
		err |= __put_user(fix.type, &fix32->type);
		err |= __put_user(fix.type_aux, &fix32->type_aux);
		err |= __put_user(fix.visual, &fix32->visual);
		err |= __put_user(fix.xpanstep, &fix32->xpanstep);
		err |= __put_user(fix.ypanstep, &fix32->ypanstep);
		err |= __put_user(fix.ywrapstep, &fix32->ywrapstep);
		err |= __put_user(fix.line_length, &fix32->line_length);
		err |= __put_user((__u32)(unsigned long)fix.mmio_start,
				  &fix32->mmio_start);
		err |= __put_user(fix.mmio_len, &fix32->mmio_len);
		err |= __put_user(fix.accel, &fix32->accel);
		err |= __copy_to_user((char *)fix32->reserved,
				      (char *)fix.reserved,
				      sizeof(fix.reserved));
		if (err)
			err = -EFAULT;
	}

	return err;
}

struct fb_cmap32 {
	__u32 start;			/* First entry  */
	__u32 len;			/* Number of entries */
	__u32 red;			/* Red values   */
	__u32 green;
	__u32 blue;
	__u32 transp;			/* transparency, can be NULL */
};

static int do_fbiocmap_ioctl(unsigned int fd, unsigned int cmd,
			     unsigned long arg)
{
	mm_segment_t old_fs = get_fs();
	u32 red = 0, green = 0, blue = 0, transp = 0;
	struct fb_cmap cmap;
	struct fb_cmap32 *cmap32 = (struct fb_cmap32 *)arg;
	int err;

	memset(&cmap, 0, sizeof(cmap));

	err = __get_user(cmap.start, &cmap32->start);
	err |= __get_user(cmap.len, &cmap32->len);
	err |= __get_user(red, &cmap32->red);
	err |= __get_user(green, &cmap32->green);
	err |= __get_user(blue, &cmap32->blue);
	err |= __get_user(transp, &cmap32->transp);
	if (err)
		return -EFAULT;

	err = -ENOMEM;
	cmap.red = kmalloc(cmap.len * sizeof(__u16), GFP_KERNEL);
	if (!cmap.red)
		goto out;
	cmap.green = kmalloc(cmap.len * sizeof(__u16), GFP_KERNEL);
	if (!cmap.green)
		goto out;
	cmap.blue = kmalloc(cmap.len * sizeof(__u16), GFP_KERNEL);
	if (!cmap.blue)
		goto out;
	if (transp) {
		cmap.transp = kmalloc(cmap.len * sizeof(__u16), GFP_KERNEL);
		if (!cmap.transp)
			goto out;
	}
			
	if (cmd == FBIOPUTCMAP) {
		err = __copy_from_user(cmap.red, (char *)A(red),
				       cmap.len * sizeof(__u16));
		err |= __copy_from_user(cmap.green, (char *)A(green),
					cmap.len * sizeof(__u16));
		err |= __copy_from_user(cmap.blue, (char *)A(blue),
					cmap.len * sizeof(__u16));
		if (cmap.transp)
			err |= __copy_from_user(cmap.transp, (char *)A(transp),
						cmap.len * sizeof(__u16));
		if (err) {
			err = -EFAULT;
			goto out;
		}
	}

	set_fs(KERNEL_DS);
	err = sys_ioctl(fd, cmd, (unsigned long)&cmap);
	set_fs(old_fs);
	if (err)
		goto out;

	if (cmd == FBIOGETCMAP) {
		err = __copy_to_user((char *)A(red), cmap.red,
				     cmap.len * sizeof(__u16));
		err |= __copy_to_user((char *)A(green), cmap.blue,
				      cmap.len * sizeof(__u16));
		err |= __copy_to_user((char *)A(blue), cmap.blue,
				      cmap.len * sizeof(__u16));
		if (cmap.transp)
			err |= __copy_to_user((char *)A(transp), cmap.transp,
					      cmap.len * sizeof(__u16));
		if (err) {
			err = -EFAULT;
			goto out;
		}
	}

out:
	if (cmap.red)
		kfree(cmap.red);
	if (cmap.green)
		kfree(cmap.green);
	if (cmap.blue)
		kfree(cmap.blue);
	if (cmap.transp)
		kfree(cmap.transp);

	return err;
}

#endif /* CONFIG_FB */


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

#define EXT2_IOC32_GETFLAGS               _IOR('f', 1, int)
#define EXT2_IOC32_SETFLAGS               _IOW('f', 2, int)
#define EXT2_IOC32_GETVERSION             _IOR('v', 1, int)
#define EXT2_IOC32_SETVERSION             _IOW('v', 2, int)

struct ifmap32 {
	unsigned int mem_start;
	unsigned int mem_end;
	unsigned short base_addr;
	unsigned char irq;
	unsigned char dma;
	unsigned char port;
};

struct ifreq32 {
#define IFHWADDRLEN     6
#define IFNAMSIZ        16
        union {
                char    ifrn_name[IFNAMSIZ];	/* if name, e.g. "en0" */
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
	struct ireq32 *uir32 = (struct ireq32 *)arg;
	struct net_device *dev;
	struct ifreq32 ifr32;

	if (copy_from_user(&ifr32, uir32, sizeof(struct ifreq32)))
		return -EFAULT;

	read_lock(&dev_base_lock);
	dev = __dev_get_by_index(ifr32.ifr_ifindex);
	if (!dev) {
		read_unlock(&dev_base_lock);
		return -ENODEV;
	}

	strcpy(ifr32.ifr_name, dev->name);
	read_unlock(&dev_base_lock);

	if (copy_to_user(uir32, &ifr32, sizeof(struct ifreq32)))
	    return -EFAULT;

	return 0;
}

static inline int dev_ifconf(unsigned int fd, unsigned int cmd,
			     unsigned long arg)
{
	struct ioconf32 *uifc32 = (struct ioconf32 *)arg;
	struct ifconf32 ifc32;
	struct ifconf ifc;
	struct ifreq32 *ifr32;
	struct ifreq *ifr;
	mm_segment_t old_fs;
	unsigned int i, j;
	int err;

	if (copy_from_user(&ifc32, uifc32, sizeof(struct ifconf32)))
		return -EFAULT;

	if(ifc32.ifcbuf == 0) {
		ifc32.ifc_len = 0;
		ifc.ifc_len = 0;
		ifc.ifc_buf = NULL;
	} else {
		ifc.ifc_len = ((ifc32.ifc_len / sizeof (struct ifreq32))) *
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
	if (err)
		goto out;

	ifr = ifc.ifc_req;
	ifr32 = (struct ifreq32 *)A(ifc32.ifcbuf);
	for (i = 0, j = 0; i < ifc32.ifc_len && j < ifc.ifc_len;
	     i += sizeof (struct ifreq32), j += sizeof (struct ifreq)) {
		if (copy_to_user(ifr32++, ifr++, sizeof (struct ifreq32))) {
			err = -EFAULT;
			goto out;
		}
	}
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
	if (copy_to_user(uifc32, &ifc32, sizeof(struct ifconf32))) {
		err = -EFAULT;
		goto out;
	}
out:
	if(ifc.ifc_buf != NULL)
		kfree (ifc.ifc_buf);
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

struct rtentry32
{
	unsigned int	rt_pad1;
	struct sockaddr	rt_dst;		/* target address		*/
	struct sockaddr	rt_gateway;	/* gateway addr (RTF_GATEWAY)	*/
	struct sockaddr	rt_genmask;	/* target network mask (IP)	*/
	unsigned short	rt_flags;
	short		rt_pad2;
	unsigned int	rt_pad3;
	unsigned int	rt_pad4;
	short		rt_metric;	/* +1 for binary compatibility!	*/
	unsigned int	rt_dev;		/* forcing the device at add	*/
	unsigned int	rt_mtu;		/* per route MTU/Window 	*/
#ifndef __KERNEL__
#define rt_mss	rt_mtu			/* Compatibility :-(            */
#endif
	unsigned int	rt_window;	/* Window clamping 		*/
	unsigned short	rt_irtt;	/* Initial RTT			*/
};

static inline int routing_ioctl(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	struct rtentry32 *ur = (struct rtentry32 *)arg;
	struct rtentry r;
	char devname[16];
	u32 rtdev;
	int ret;
	mm_segment_t old_fs = get_fs();

	ret = copy_from_user (&r.rt_dst, &(ur->rt_dst), 3 * sizeof(struct sockaddr));
	ret |= __get_user (r.rt_flags, &(ur->rt_flags));
	ret |= __get_user (r.rt_metric, &(ur->rt_metric));
	ret |= __get_user (r.rt_mtu, &(ur->rt_mtu));
	ret |= __get_user (r.rt_window, &(ur->rt_window));
	ret |= __get_user (r.rt_irtt, &(ur->rt_irtt));
	ret |= __get_user (rtdev, &(ur->rt_dev));
	if (rtdev) {
		ret |= copy_from_user (devname, (char *)A(rtdev), 15);
		r.rt_dev = devname; devname[15] = 0;
	} else
		r.rt_dev = 0;
	if (ret)
		return -EFAULT;
	set_fs (KERNEL_DS);
	ret = sys_ioctl (fd, cmd, (long)&r);
	set_fs (old_fs);
	return ret;
}

#endif /* CONFIG_NET */

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

	if(get_user(kp->tuner, &up->tuner))
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

	if(put_user(kp->tuner, &up->tuner))
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

	if(get_user(tmp, &up->base))
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

	if(put_user(tmp, &up->base))
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
	if(cp != NULL)
		kfree(cp);
}

static int get_video_window32(struct video_window *kp, struct video_window32 *up)
{
	struct video_clip32 *ucp;
	struct video_clip *kcp;
	int nclips, err, i;
	u32 tmp;

	if(get_user(kp->x, &up->x))
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
	if(nclips == 0)
		return 0;

	if(ucp == 0)
		return -EINVAL;

	/* Peculiar interface... */
	if(nclips < 0)
		nclips = VIDEO_CLIPMAP_SIZE;

	kcp = kmalloc(nclips * sizeof(struct video_clip), GFP_KERNEL);
	err = -ENOMEM;
	if(kcp == NULL)
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
	if(put_user(kp->x, &up->x))
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
	if(err)
		goto out;

	set_fs(KERNEL_DS);
	err = sys_ioctl(fd, cmd, (unsigned long)&karg);
	set_fs(old_fs);

	if(cmd == VIDIOCSWIN)
		free_kvideo_clips(&karg.vw);

	if(err == 0) {
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

static int blkpg_ioctl_trans(unsigned int fd, unsigned int cmd,
                             struct blkpg_ioctl_arg32 *arg)
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
	default:
		return -EINVAL;
	}
	return err;
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
		} while(0);
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

#define AUTOFS_IOC_SETTIMEOUT32 _IOWR(0x93,0x64,unsigned int)

static int ioc_settimeout(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	return rw_long(fd, AUTOFS_IOC_SETTIMEOUT, arg);
}

/* serial_struct_ioctl was taken from x86_64/ia32/ia32_ioctl.c and
 * slightly modified for mips */
/* iomem_base is unsigned char * in linux/serial.h (reserved in sgiserial.h) */
struct serial_struct32 {
	int	type;
	int	line;
	unsigned int	port;
	int	irq;
	int	flags;
	int	xmit_fifo_size;
	int	custom_divisor;
	int	baud_base;
	unsigned short	close_delay;
	char	io_type;
	char	reserved_char[1];
	int	hub6;
	unsigned short	closing_wait; /* time to wait before closing */
	unsigned short	closing_wait2; /* no longer used... */
	__u32 iomem_base;
	unsigned short	iomem_reg_shift;
	unsigned int	port_high;
	int	reserved[1];
};

static int serial_struct_ioctl(unsigned fd, unsigned cmd,  void *ptr) 
{
	typedef struct serial_struct SS;
	struct serial_struct32 *ss32 = ptr; 
	int err = 0;
	struct serial_struct ss; 
	mm_segment_t oldseg = get_fs(); 
	set_fs(KERNEL_DS);
	if (cmd == TIOCSSERIAL) { 
		err = -EFAULT;
		if (copy_from_user(&ss, ss32, sizeof(struct serial_struct32)))
			goto out;
		memmove(&ss.iomem_reg_shift, ((char*)&ss.iomem_base)+4, 
			sizeof(SS)-offsetof(SS,iomem_reg_shift)); 
		ss.iomem_base = (void *)(long)ss.iomem_base; /* sign extend */
	}
	if (!err)
		err = sys_ioctl(fd,cmd,(unsigned long)(&ss)); 
	if (cmd == TIOCGSERIAL && err >= 0) { 
		__u32 base;
		if (__copy_to_user(ss32,&ss,offsetof(SS,iomem_base)) ||
		    __copy_to_user(&ss32->iomem_reg_shift,
				   &ss.iomem_reg_shift,
				   sizeof(SS) - offsetof(SS, iomem_reg_shift)))
			err = -EFAULT;
		base = (unsigned long)ss.iomem_base;
		err |= __put_user(base, &ss32->iomem_base); 		
	} 
 out:
	set_fs(oldseg);
	return err;	
}

/* loop_status was taken from sparc64/kernel/ioctl32.c */
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
		static int count;
		if (++count <= 20)
			printk("%s: Unknown loop ioctl cmd, fd(%d) "
			       "cmd(%08x) arg(%08lx)\n",
			       __FUNCTION__, fd, cmd, arg);
	}
	}
	return err;
}

#ifdef CONFIG_VT

extern int tty_ioctl(struct inode * inode, struct file * file,
	unsigned int cmd, unsigned long arg);

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

#endif /* CONFIG_VT */

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
		size = l->lv_allocated_le * sizeof(pe_t);
		l->lv_current_pe = vmalloc(size);
		if (l->lv_current_pe)
			err = copy_from_user(l->lv_current_pe, (void *)A(ptr1), size);
	}
	if (!err && ptr2) {
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
#endif /* CONFIG_BLK_DEV_LVM */

struct ioctl32_handler {
	unsigned int cmd;
	int (*function)(unsigned int, unsigned int, unsigned long);
};

struct ioctl32_list {
	struct ioctl32_handler handler;
	struct ioctl32_list *next;
};

#define IOCTL32_DEFAULT(cmd)		{ { cmd, (void *) sys_ioctl }, 0 }
#define IOCTL32_HANDLER(cmd, handler)	{ { cmd, (void *) handler }, 0 }

static struct ioctl32_list ioctl32_handler_table[] = {
	IOCTL32_DEFAULT(TCGETA),
	IOCTL32_DEFAULT(TCSETA),
	IOCTL32_DEFAULT(TCSETAW),
	IOCTL32_DEFAULT(TCSETAF),
	IOCTL32_DEFAULT(TCSBRK),
	IOCTL32_DEFAULT(TCSBRKP),
	IOCTL32_DEFAULT(TCXONC),
	IOCTL32_DEFAULT(TCFLSH),
	IOCTL32_DEFAULT(TCGETS),
	IOCTL32_DEFAULT(TCSETS),
	IOCTL32_DEFAULT(TCSETSW),
	IOCTL32_DEFAULT(TCSETSF),
	IOCTL32_DEFAULT(TIOCLINUX),

	IOCTL32_DEFAULT(TIOCGETD),
	IOCTL32_DEFAULT(TIOCSETD),
	IOCTL32_DEFAULT(TIOCEXCL),
	IOCTL32_DEFAULT(TIOCNXCL),
	IOCTL32_DEFAULT(TIOCCONS),
	IOCTL32_DEFAULT(TIOCGSOFTCAR),
	IOCTL32_DEFAULT(TIOCSSOFTCAR),
	IOCTL32_DEFAULT(TIOCSWINSZ),
	IOCTL32_DEFAULT(TIOCGWINSZ),
	IOCTL32_DEFAULT(TIOCMGET),
	IOCTL32_DEFAULT(TIOCMBIC),
	IOCTL32_DEFAULT(TIOCMBIS),
	IOCTL32_DEFAULT(TIOCMSET),
	IOCTL32_DEFAULT(TIOCPKT),
	IOCTL32_DEFAULT(TIOCNOTTY),
	IOCTL32_DEFAULT(TIOCSTI),
	IOCTL32_DEFAULT(TIOCOUTQ),
	IOCTL32_DEFAULT(TIOCSPGRP),
	IOCTL32_DEFAULT(TIOCGPGRP),
	IOCTL32_DEFAULT(TIOCSCTTY),
	IOCTL32_DEFAULT(TIOCGPTN),
	IOCTL32_DEFAULT(TIOCSPTLCK),
	IOCTL32_HANDLER(TIOCGSERIAL, serial_struct_ioctl),
	IOCTL32_HANDLER(TIOCSSERIAL, serial_struct_ioctl),
	IOCTL32_DEFAULT(TIOCSERGETLSR),

	IOCTL32_DEFAULT(FIOCLEX),
	IOCTL32_DEFAULT(FIONCLEX),
	IOCTL32_DEFAULT(FIOASYNC),
	IOCTL32_DEFAULT(FIONBIO),
	IOCTL32_DEFAULT(FIONREAD),

#ifdef CONFIG_FB
	/* Big F */
	IOCTL32_DEFAULT(FBIOGET_VSCREENINFO),
	IOCTL32_DEFAULT(FBIOPUT_VSCREENINFO),
	IOCTL32_HANDLER(FBIOGET_FSCREENINFO, do_fbioget_fscreeninfo_ioctl),
	IOCTL32_HANDLER(FBIOGETCMAP, do_fbiocmap_ioctl),
	IOCTL32_HANDLER(FBIOPUTCMAP, do_fbiocmap_ioctl),
	IOCTL32_DEFAULT(FBIOPAN_DISPLAY),
#endif /* CONFIG_FB */

	/* Little v, the video4linux ioctls */
	IOCTL32_DEFAULT(VIDIOCGCAP),
	IOCTL32_DEFAULT(VIDIOCGCHAN),
	IOCTL32_DEFAULT(VIDIOCSCHAN),
	IOCTL32_DEFAULT(VIDIOCGPICT),
	IOCTL32_DEFAULT(VIDIOCSPICT),
	IOCTL32_DEFAULT(VIDIOCCAPTURE),

	IOCTL32_DEFAULT(VIDIOCGWIN),
	IOCTL32_DEFAULT(VIDIOCSWIN),
	IOCTL32_DEFAULT(VIDIOCGFBUF),
	IOCTL32_DEFAULT(VIDIOCSFBUF),

	IOCTL32_DEFAULT(VIDIOCKEY),
	IOCTL32_DEFAULT(VIDIOCGAUDIO),
	IOCTL32_DEFAULT(VIDIOCSAUDIO),
	IOCTL32_DEFAULT(VIDIOCSYNC),
	IOCTL32_DEFAULT(VIDIOCMCAPTURE),
	IOCTL32_DEFAULT(VIDIOCGMBUF),
	IOCTL32_DEFAULT(VIDIOCGUNIT),
	IOCTL32_DEFAULT(VIDIOCGCAPTURE),
	IOCTL32_DEFAULT(VIDIOCSCAPTURE),
	/* BTTV specific... */
	IOCTL32_DEFAULT(_IOW('v',  BASE_VIDIOCPRIVATE+0, char [256])),
	IOCTL32_DEFAULT(_IOR('v',  BASE_VIDIOCPRIVATE+1, char [256])),
	IOCTL32_DEFAULT(_IOR('v' , BASE_VIDIOCPRIVATE+2, unsigned int)),
	IOCTL32_DEFAULT(_IOW('v' , BASE_VIDIOCPRIVATE+3, char [16])), /* struct bttv_pll_info */
	IOCTL32_DEFAULT(_IOR('v' , BASE_VIDIOCPRIVATE+4, int)),
	IOCTL32_DEFAULT(_IOR('v' , BASE_VIDIOCPRIVATE+5, int)),
	IOCTL32_DEFAULT(_IOR('v' , BASE_VIDIOCPRIVATE+6, int)),
	IOCTL32_DEFAULT(_IOR('v' , BASE_VIDIOCPRIVATE+7, int)),

#ifdef CONFIG_NET
	/* Socket level stuff */
	IOCTL32_DEFAULT(FIOSETOWN),
	IOCTL32_DEFAULT(SIOCSPGRP),
	IOCTL32_DEFAULT(FIOGETOWN),
	IOCTL32_DEFAULT(SIOCGPGRP),
	IOCTL32_DEFAULT(SIOCATMARK),
	IOCTL32_DEFAULT(SIOCSIFLINK),
	IOCTL32_DEFAULT(SIOCSIFENCAP),
	IOCTL32_DEFAULT(SIOCGIFENCAP),
	IOCTL32_DEFAULT(SIOCSIFBR),
	IOCTL32_DEFAULT(SIOCGIFBR),
	IOCTL32_DEFAULT(SIOCSARP),
	IOCTL32_DEFAULT(SIOCGARP),
	IOCTL32_DEFAULT(SIOCDARP),
	IOCTL32_DEFAULT(SIOCSRARP),
	IOCTL32_DEFAULT(SIOCGRARP),
	IOCTL32_DEFAULT(SIOCDRARP),
	IOCTL32_DEFAULT(SIOCADDDLCI),
	IOCTL32_DEFAULT(SIOCDELDLCI),
	/* SG stuff */
	IOCTL32_DEFAULT(SG_SET_TIMEOUT),
	IOCTL32_DEFAULT(SG_GET_TIMEOUT),
	IOCTL32_DEFAULT(SG_EMULATED_HOST),
	IOCTL32_DEFAULT(SG_SET_TRANSFORM),
	IOCTL32_DEFAULT(SG_GET_TRANSFORM),
	IOCTL32_DEFAULT(SG_SET_RESERVED_SIZE),
	IOCTL32_DEFAULT(SG_GET_RESERVED_SIZE),
	IOCTL32_DEFAULT(SG_GET_SCSI_ID),
	IOCTL32_DEFAULT(SG_SET_FORCE_LOW_DMA),
	IOCTL32_DEFAULT(SG_GET_LOW_DMA),
	IOCTL32_DEFAULT(SG_SET_FORCE_PACK_ID),
	IOCTL32_DEFAULT(SG_GET_PACK_ID),
	IOCTL32_DEFAULT(SG_GET_NUM_WAITING),
	IOCTL32_DEFAULT(SG_SET_DEBUG),
	IOCTL32_DEFAULT(SG_GET_SG_TABLESIZE),
	IOCTL32_DEFAULT(SG_GET_COMMAND_Q),
	IOCTL32_DEFAULT(SG_SET_COMMAND_Q),
	IOCTL32_DEFAULT(SG_GET_VERSION_NUM),
	IOCTL32_DEFAULT(SG_NEXT_CMD_LEN),
	IOCTL32_DEFAULT(SG_SCSI_RESET),
	IOCTL32_DEFAULT(SG_IO),
	IOCTL32_DEFAULT(SG_GET_REQUEST_TABLE),
	IOCTL32_DEFAULT(SG_SET_KEEP_ORPHAN),
	IOCTL32_DEFAULT(SG_GET_KEEP_ORPHAN),
	/* PPP stuff */
	IOCTL32_DEFAULT(PPPIOCGFLAGS),
	IOCTL32_DEFAULT(PPPIOCSFLAGS),
	IOCTL32_DEFAULT(PPPIOCGASYNCMAP),
	IOCTL32_DEFAULT(PPPIOCSASYNCMAP),
	IOCTL32_DEFAULT(PPPIOCGUNIT),
	IOCTL32_DEFAULT(PPPIOCGRASYNCMAP),
	IOCTL32_DEFAULT(PPPIOCSRASYNCMAP),
	IOCTL32_DEFAULT(PPPIOCGMRU),
	IOCTL32_DEFAULT(PPPIOCSMRU),
	IOCTL32_DEFAULT(PPPIOCSMAXCID),
	IOCTL32_DEFAULT(PPPIOCGXASYNCMAP),
	IOCTL32_DEFAULT(PPPIOCSXASYNCMAP),
	IOCTL32_DEFAULT(PPPIOCXFERUNIT),
	IOCTL32_DEFAULT(PPPIOCGNPMODE),
	IOCTL32_DEFAULT(PPPIOCSNPMODE),
	IOCTL32_DEFAULT(PPPIOCGDEBUG),
	IOCTL32_DEFAULT(PPPIOCSDEBUG),
	IOCTL32_DEFAULT(PPPIOCNEWUNIT),
	IOCTL32_DEFAULT(PPPIOCATTACH),
	IOCTL32_DEFAULT(PPPIOCGCHAN),
	/* PPPOX */
	IOCTL32_DEFAULT(PPPOEIOCSFWD),
	IOCTL32_DEFAULT(PPPOEIOCDFWD),
	/* CDROM stuff */
	IOCTL32_DEFAULT(CDROMPAUSE),
	IOCTL32_DEFAULT(CDROMRESUME),
	IOCTL32_DEFAULT(CDROMPLAYMSF),
	IOCTL32_DEFAULT(CDROMPLAYTRKIND),
	IOCTL32_DEFAULT(CDROMREADTOCHDR),
	IOCTL32_DEFAULT(CDROMREADTOCENTRY),
	IOCTL32_DEFAULT(CDROMSTOP),
	IOCTL32_DEFAULT(CDROMSTART),
	IOCTL32_DEFAULT(CDROMEJECT),
	IOCTL32_DEFAULT(CDROMVOLCTRL),
	IOCTL32_DEFAULT(CDROMSUBCHNL),
	IOCTL32_DEFAULT(CDROMEJECT_SW),
	IOCTL32_DEFAULT(CDROMMULTISESSION),
	IOCTL32_DEFAULT(CDROM_GET_MCN),
	IOCTL32_DEFAULT(CDROMRESET),
	IOCTL32_DEFAULT(CDROMVOLREAD),
	IOCTL32_DEFAULT(CDROMSEEK),
	IOCTL32_DEFAULT(CDROMPLAYBLK),
	IOCTL32_DEFAULT(CDROMCLOSETRAY),
	IOCTL32_DEFAULT(CDROM_SET_OPTIONS),
	IOCTL32_DEFAULT(CDROM_CLEAR_OPTIONS),
	IOCTL32_DEFAULT(CDROM_SELECT_SPEED),
	IOCTL32_DEFAULT(CDROM_SELECT_DISC),
	IOCTL32_DEFAULT(CDROM_MEDIA_CHANGED),
	IOCTL32_DEFAULT(CDROM_DRIVE_STATUS),
	IOCTL32_DEFAULT(CDROM_DISC_STATUS),
	IOCTL32_DEFAULT(CDROM_CHANGER_NSLOTS),
	IOCTL32_DEFAULT(CDROM_LOCKDOOR),
	IOCTL32_DEFAULT(CDROM_DEBUG),
	IOCTL32_DEFAULT(CDROM_GET_CAPABILITY),
	/* DVD ioctls */
	IOCTL32_DEFAULT(DVD_READ_STRUCT),
	IOCTL32_DEFAULT(DVD_WRITE_STRUCT),
	IOCTL32_DEFAULT(DVD_AUTH),
	/* Big L */
	IOCTL32_DEFAULT(LOOP_SET_FD),
	IOCTL32_DEFAULT(LOOP_CLR_FD),
	IOCTL32_HANDLER(LOOP_SET_STATUS, loop_status),
	IOCTL32_HANDLER(LOOP_GET_STATUS, loop_status),
	/* Big Q for sound/OSS */
	IOCTL32_DEFAULT(SNDCTL_SEQ_RESET),
	IOCTL32_DEFAULT(SNDCTL_SEQ_SYNC),
	IOCTL32_DEFAULT(SNDCTL_SYNTH_INFO),
	IOCTL32_DEFAULT(SNDCTL_SEQ_CTRLRATE),
	IOCTL32_DEFAULT(SNDCTL_SEQ_GETOUTCOUNT),
	IOCTL32_DEFAULT(SNDCTL_SEQ_GETINCOUNT),
	IOCTL32_DEFAULT(SNDCTL_SEQ_PERCMODE),
	IOCTL32_DEFAULT(SNDCTL_FM_LOAD_INSTR),
	IOCTL32_DEFAULT(SNDCTL_SEQ_TESTMIDI),
	IOCTL32_DEFAULT(SNDCTL_SEQ_RESETSAMPLES),
	IOCTL32_DEFAULT(SNDCTL_SEQ_NRSYNTHS),
	IOCTL32_DEFAULT(SNDCTL_SEQ_NRMIDIS),
	IOCTL32_DEFAULT(SNDCTL_MIDI_INFO),
	IOCTL32_DEFAULT(SNDCTL_SEQ_THRESHOLD),
	IOCTL32_DEFAULT(SNDCTL_SYNTH_MEMAVL),
	IOCTL32_DEFAULT(SNDCTL_FM_4OP_ENABLE),
	IOCTL32_DEFAULT(SNDCTL_SEQ_PANIC),
	IOCTL32_DEFAULT(SNDCTL_SEQ_OUTOFBAND),
	IOCTL32_DEFAULT(SNDCTL_SEQ_GETTIME),
	IOCTL32_DEFAULT(SNDCTL_SYNTH_ID),
	IOCTL32_DEFAULT(SNDCTL_SYNTH_CONTROL),
	IOCTL32_DEFAULT(SNDCTL_SYNTH_REMOVESAMPLE),
	/* Big T for sound/OSS */
	IOCTL32_DEFAULT(SNDCTL_TMR_TIMEBASE),
	IOCTL32_DEFAULT(SNDCTL_TMR_START),
	IOCTL32_DEFAULT(SNDCTL_TMR_STOP),
	IOCTL32_DEFAULT(SNDCTL_TMR_CONTINUE),
	IOCTL32_DEFAULT(SNDCTL_TMR_TEMPO),
	IOCTL32_DEFAULT(SNDCTL_TMR_SOURCE),
	IOCTL32_DEFAULT(SNDCTL_TMR_METRONOME),
	IOCTL32_DEFAULT(SNDCTL_TMR_SELECT),
	/* Little m for sound/OSS */
	IOCTL32_DEFAULT(SNDCTL_MIDI_PRETIME),
	IOCTL32_DEFAULT(SNDCTL_MIDI_MPUMODE),
	IOCTL32_DEFAULT(SNDCTL_MIDI_MPUCMD),
	/* Big P for sound/OSS */
	IOCTL32_DEFAULT(SNDCTL_DSP_RESET),
	IOCTL32_DEFAULT(SNDCTL_DSP_SYNC),
	IOCTL32_DEFAULT(SNDCTL_DSP_SPEED),
	IOCTL32_DEFAULT(SNDCTL_DSP_STEREO),
	IOCTL32_DEFAULT(SNDCTL_DSP_GETBLKSIZE),
	IOCTL32_DEFAULT(SNDCTL_DSP_CHANNELS),
	IOCTL32_DEFAULT(SOUND_PCM_WRITE_FILTER),
	IOCTL32_DEFAULT(SNDCTL_DSP_POST),
	IOCTL32_DEFAULT(SNDCTL_DSP_SUBDIVIDE),
	IOCTL32_DEFAULT(SNDCTL_DSP_SETFRAGMENT),
	IOCTL32_DEFAULT(SNDCTL_DSP_GETFMTS),
	IOCTL32_DEFAULT(SNDCTL_DSP_SETFMT),
	IOCTL32_DEFAULT(SNDCTL_DSP_GETOSPACE),
	IOCTL32_DEFAULT(SNDCTL_DSP_GETISPACE),
	IOCTL32_DEFAULT(SNDCTL_DSP_NONBLOCK),
	IOCTL32_DEFAULT(SNDCTL_DSP_GETCAPS),
	IOCTL32_DEFAULT(SNDCTL_DSP_GETTRIGGER),
	IOCTL32_DEFAULT(SNDCTL_DSP_SETTRIGGER),
	IOCTL32_DEFAULT(SNDCTL_DSP_GETIPTR),
	IOCTL32_DEFAULT(SNDCTL_DSP_GETOPTR),
	/* SNDCTL_DSP_MAPINBUF,  XXX needs translation */
	/* SNDCTL_DSP_MAPOUTBUF,  XXX needs translation */
	IOCTL32_DEFAULT(SNDCTL_DSP_SETSYNCRO),
	IOCTL32_DEFAULT(SNDCTL_DSP_SETDUPLEX),
	IOCTL32_DEFAULT(SNDCTL_DSP_GETODELAY),
	IOCTL32_DEFAULT(SNDCTL_DSP_PROFILE),
	IOCTL32_DEFAULT(SOUND_PCM_READ_RATE),
	IOCTL32_DEFAULT(SOUND_PCM_READ_CHANNELS),
	IOCTL32_DEFAULT(SOUND_PCM_READ_BITS),
	IOCTL32_DEFAULT(SOUND_PCM_READ_FILTER),
	/* Big C for sound/OSS */
	IOCTL32_DEFAULT(SNDCTL_COPR_RESET),
	IOCTL32_DEFAULT(SNDCTL_COPR_LOAD),
	IOCTL32_DEFAULT(SNDCTL_COPR_RDATA),
	IOCTL32_DEFAULT(SNDCTL_COPR_RCODE),
	IOCTL32_DEFAULT(SNDCTL_COPR_WDATA),
	IOCTL32_DEFAULT(SNDCTL_COPR_WCODE),
	IOCTL32_DEFAULT(SNDCTL_COPR_RUN),
	IOCTL32_DEFAULT(SNDCTL_COPR_HALT),
	IOCTL32_DEFAULT(SNDCTL_COPR_SENDMSG),
	IOCTL32_DEFAULT(SNDCTL_COPR_RCVMSG),
	/* Big M for sound/OSS */
	IOCTL32_DEFAULT(SOUND_MIXER_READ_VOLUME),
	IOCTL32_DEFAULT(SOUND_MIXER_READ_BASS),
	IOCTL32_DEFAULT(SOUND_MIXER_READ_TREBLE),
	IOCTL32_DEFAULT(SOUND_MIXER_READ_SYNTH),
	IOCTL32_DEFAULT(SOUND_MIXER_READ_PCM),
	IOCTL32_DEFAULT(SOUND_MIXER_READ_SPEAKER),
	IOCTL32_DEFAULT(SOUND_MIXER_READ_LINE),
	IOCTL32_DEFAULT(SOUND_MIXER_READ_MIC),
	IOCTL32_DEFAULT(SOUND_MIXER_READ_CD),
	IOCTL32_DEFAULT(SOUND_MIXER_READ_IMIX),
	IOCTL32_DEFAULT(SOUND_MIXER_READ_ALTPCM),
	IOCTL32_DEFAULT(SOUND_MIXER_READ_RECLEV),
	IOCTL32_DEFAULT(SOUND_MIXER_READ_IGAIN),
	IOCTL32_DEFAULT(SOUND_MIXER_READ_OGAIN),
	IOCTL32_DEFAULT(SOUND_MIXER_READ_LINE1),
	IOCTL32_DEFAULT(SOUND_MIXER_READ_LINE2),
	IOCTL32_DEFAULT(SOUND_MIXER_READ_LINE3),
	IOCTL32_DEFAULT(MIXER_READ(SOUND_MIXER_DIGITAL1)),
	IOCTL32_DEFAULT(MIXER_READ(SOUND_MIXER_DIGITAL2)),
	IOCTL32_DEFAULT(MIXER_READ(SOUND_MIXER_DIGITAL3)),
	IOCTL32_DEFAULT(MIXER_READ(SOUND_MIXER_PHONEIN)),
	IOCTL32_DEFAULT(MIXER_READ(SOUND_MIXER_PHONEOUT)),
	IOCTL32_DEFAULT(MIXER_READ(SOUND_MIXER_VIDEO)),
	IOCTL32_DEFAULT(MIXER_READ(SOUND_MIXER_RADIO)),
	IOCTL32_DEFAULT(MIXER_READ(SOUND_MIXER_MONITOR)),
	IOCTL32_DEFAULT(SOUND_MIXER_READ_MUTE),
	/* SOUND_MIXER_READ_ENHANCE,  same value as READ_MUTE */
	/* SOUND_MIXER_READ_LOUD,  same value as READ_MUTE */
	IOCTL32_DEFAULT(SOUND_MIXER_READ_RECSRC),
	IOCTL32_DEFAULT(SOUND_MIXER_READ_DEVMASK),
	IOCTL32_DEFAULT(SOUND_MIXER_READ_RECMASK),
	IOCTL32_DEFAULT(SOUND_MIXER_READ_STEREODEVS),
	IOCTL32_DEFAULT(SOUND_MIXER_READ_CAPS),
	IOCTL32_DEFAULT(SOUND_MIXER_WRITE_VOLUME),
	IOCTL32_DEFAULT(SOUND_MIXER_WRITE_BASS),
	IOCTL32_DEFAULT(SOUND_MIXER_WRITE_TREBLE),
	IOCTL32_DEFAULT(SOUND_MIXER_WRITE_SYNTH),
	IOCTL32_DEFAULT(SOUND_MIXER_WRITE_PCM),
	IOCTL32_DEFAULT(SOUND_MIXER_WRITE_SPEAKER),
	IOCTL32_DEFAULT(SOUND_MIXER_WRITE_LINE),
	IOCTL32_DEFAULT(SOUND_MIXER_WRITE_MIC),
	IOCTL32_DEFAULT(SOUND_MIXER_WRITE_CD),
	IOCTL32_DEFAULT(SOUND_MIXER_WRITE_IMIX),
	IOCTL32_DEFAULT(SOUND_MIXER_WRITE_ALTPCM),
	IOCTL32_DEFAULT(SOUND_MIXER_WRITE_RECLEV),
	IOCTL32_DEFAULT(SOUND_MIXER_WRITE_IGAIN),
	IOCTL32_DEFAULT(SOUND_MIXER_WRITE_OGAIN),
	IOCTL32_DEFAULT(SOUND_MIXER_WRITE_LINE1),
	IOCTL32_DEFAULT(SOUND_MIXER_WRITE_LINE2),
	IOCTL32_DEFAULT(SOUND_MIXER_WRITE_LINE3),
	IOCTL32_DEFAULT(MIXER_WRITE(SOUND_MIXER_DIGITAL1)),
	IOCTL32_DEFAULT(MIXER_WRITE(SOUND_MIXER_DIGITAL2)),
	IOCTL32_DEFAULT(MIXER_WRITE(SOUND_MIXER_DIGITAL3)),
	IOCTL32_DEFAULT(MIXER_WRITE(SOUND_MIXER_PHONEIN)),
	IOCTL32_DEFAULT(MIXER_WRITE(SOUND_MIXER_PHONEOUT)),
	IOCTL32_DEFAULT(MIXER_WRITE(SOUND_MIXER_VIDEO)),
	IOCTL32_DEFAULT(MIXER_WRITE(SOUND_MIXER_RADIO)),
	IOCTL32_DEFAULT(MIXER_WRITE(SOUND_MIXER_MONITOR)),
	IOCTL32_DEFAULT(SOUND_MIXER_WRITE_MUTE),
	/* SOUND_MIXER_WRITE_ENHANCE,  same value as WRITE_MUTE */
	/* SOUND_MIXER_WRITE_LOUD,  same value as WRITE_MUTE */
	IOCTL32_DEFAULT(SOUND_MIXER_WRITE_RECSRC),
	IOCTL32_DEFAULT(SOUND_MIXER_INFO),
	IOCTL32_DEFAULT(SOUND_OLD_MIXER_INFO),
	IOCTL32_DEFAULT(SOUND_MIXER_ACCESS),
	IOCTL32_DEFAULT(SOUND_MIXER_PRIVATE1),
	IOCTL32_DEFAULT(SOUND_MIXER_PRIVATE2),
	IOCTL32_DEFAULT(SOUND_MIXER_PRIVATE3),
	IOCTL32_DEFAULT(SOUND_MIXER_PRIVATE4),
	IOCTL32_DEFAULT(SOUND_MIXER_PRIVATE5),
	IOCTL32_DEFAULT(SOUND_MIXER_GETLEVELS),
	IOCTL32_DEFAULT(SOUND_MIXER_SETLEVELS),
	IOCTL32_DEFAULT(OSS_GETVERSION),

	/* And these ioctls need translation */
	IOCTL32_HANDLER(SIOCGIFNAME, dev_ifname32),
	IOCTL32_HANDLER(SIOCGIFCONF, dev_ifconf),
	IOCTL32_HANDLER(SIOCGIFFLAGS, dev_ifsioc),
	IOCTL32_HANDLER(SIOCSIFFLAGS, dev_ifsioc),
	IOCTL32_HANDLER(SIOCGIFMETRIC, dev_ifsioc),
	IOCTL32_HANDLER(SIOCSIFMETRIC, dev_ifsioc),
	IOCTL32_HANDLER(SIOCGIFMTU, dev_ifsioc),
	IOCTL32_HANDLER(SIOCSIFMTU, dev_ifsioc),
	IOCTL32_HANDLER(SIOCGIFMEM, dev_ifsioc),
	IOCTL32_HANDLER(SIOCSIFMEM, dev_ifsioc),
	IOCTL32_HANDLER(SIOCGIFHWADDR, dev_ifsioc),
	IOCTL32_HANDLER(SIOCSIFHWADDR, dev_ifsioc),
	IOCTL32_HANDLER(SIOCADDMULTI, dev_ifsioc),
	IOCTL32_HANDLER(SIOCDELMULTI, dev_ifsioc),
	IOCTL32_HANDLER(SIOCGIFINDEX, dev_ifsioc),
	IOCTL32_HANDLER(SIOCGIFMAP, dev_ifsioc),
	IOCTL32_HANDLER(SIOCSIFMAP, dev_ifsioc),
	IOCTL32_HANDLER(SIOCGIFADDR, dev_ifsioc),
	IOCTL32_HANDLER(SIOCSIFADDR, dev_ifsioc),
	IOCTL32_HANDLER(SIOCGIFBRDADDR, dev_ifsioc),
	IOCTL32_HANDLER(SIOCSIFBRDADDR, dev_ifsioc),
	IOCTL32_HANDLER(SIOCGIFDSTADDR, dev_ifsioc),
	IOCTL32_HANDLER(SIOCSIFDSTADDR, dev_ifsioc),
	IOCTL32_HANDLER(SIOCGIFNETMASK, dev_ifsioc),
	IOCTL32_HANDLER(SIOCSIFNETMASK, dev_ifsioc),
	IOCTL32_HANDLER(SIOCSIFPFLAGS, dev_ifsioc),
	IOCTL32_HANDLER(SIOCGIFPFLAGS, dev_ifsioc),
	IOCTL32_HANDLER(SIOCGPPPSTATS, dev_ifsioc),
	IOCTL32_HANDLER(SIOCGPPPCSTATS, dev_ifsioc),
	IOCTL32_HANDLER(SIOCGPPPVER, dev_ifsioc),
	IOCTL32_HANDLER(SIOCGIFTXQLEN, dev_ifsioc),
	IOCTL32_HANDLER(SIOCSIFTXQLEN, dev_ifsioc),
	IOCTL32_HANDLER(SIOCADDRT, routing_ioctl),
	IOCTL32_HANDLER(SIOCDELRT, routing_ioctl),
	/*
	 * Note SIOCRTMSG is no longer, so this is safe and * the user would
	 * have seen just an -EINVAL anyways.
	 */
	IOCTL32_HANDLER(SIOCRTMSG, ret_einval),
	IOCTL32_HANDLER(SIOCGSTAMP, do_siocgstamp),

#endif /* CONFIG_NET */

	IOCTL32_HANDLER(BLKRAGET, w_long),
	IOCTL32_HANDLER(BLKGETSIZE, w_long),
	// IOCTL32_HANDLER(0x1260, broken_blkgetsize),
	IOCTL32_HANDLER(BLKFRAGET, w_long),
	IOCTL32_HANDLER(BLKSECTGET, w_long),
	IOCTL32_HANDLER(BLKPG, blkpg_ioctl_trans),

	IOCTL32_HANDLER(EXT2_IOC32_GETFLAGS, do_ext2_ioctl),
	IOCTL32_HANDLER(EXT2_IOC32_SETFLAGS, do_ext2_ioctl),
	IOCTL32_HANDLER(EXT2_IOC32_GETVERSION, do_ext2_ioctl),
	IOCTL32_HANDLER(EXT2_IOC32_SETVERSION, do_ext2_ioctl),

	IOCTL32_HANDLER(VIDIOCGTUNER32, do_video_ioctl),
	IOCTL32_HANDLER(VIDIOCSTUNER32, do_video_ioctl),
	IOCTL32_HANDLER(VIDIOCGWIN32, do_video_ioctl),
	IOCTL32_HANDLER(VIDIOCSWIN32, do_video_ioctl),
	IOCTL32_HANDLER(VIDIOCGFBUF32, do_video_ioctl),
	IOCTL32_HANDLER(VIDIOCSFBUF32, do_video_ioctl),
	IOCTL32_HANDLER(VIDIOCGFREQ32, do_video_ioctl),
	IOCTL32_HANDLER(VIDIOCSFREQ32, do_video_ioctl),

#if defined(CONFIG_BLK_DEV_LVM) || defined(CONFIG_BLK_DEV_LVM_MODULE)
	IOCTL32_HANDLER(VG_STATUS, do_lvm_ioctl),
	IOCTL32_HANDLER(VG_CREATE_OLD, do_lvm_ioctl),
	IOCTL32_HANDLER(VG_CREATE, do_lvm_ioctl),
	IOCTL32_HANDLER(VG_EXTEND, do_lvm_ioctl),
	IOCTL32_HANDLER(LV_CREATE, do_lvm_ioctl),
	IOCTL32_HANDLER(LV_REMOVE, do_lvm_ioctl),
	IOCTL32_HANDLER(LV_EXTEND, do_lvm_ioctl),
	IOCTL32_HANDLER(LV_REDUCE, do_lvm_ioctl),
	IOCTL32_HANDLER(LV_RENAME, do_lvm_ioctl),
	IOCTL32_HANDLER(LV_STATUS_BYNAME, do_lvm_ioctl),
	IOCTL32_HANDLER(LV_STATUS_BYINDEX, do_lvm_ioctl),
	IOCTL32_HANDLER(LV_STATUS_BYDEV, do_lvm_ioctl),
	IOCTL32_HANDLER(PV_CHANGE, do_lvm_ioctl),
	IOCTL32_HANDLER(PV_STATUS, do_lvm_ioctl),
#endif /* LVM */

	/* take care of sizeof(sizeof()) breakage */
	/* elevator */
	IOCTL32_HANDLER(BLKELVGET_32, do_blkelvget),
	IOCTL32_HANDLER(BLKELVSET_32, do_blkelvset),
	/* block stuff */
	IOCTL32_HANDLER(BLKBSZGET_32, do_blkbszget),
	IOCTL32_HANDLER(BLKBSZSET_32, do_blkbszset),
	IOCTL32_HANDLER(BLKGETSIZE64_32, do_blkgetsize64),

	IOCTL32_HANDLER(HDIO_GETGEO, hdio_getgeo),	/* hdreg.h ioctls  */
	IOCTL32_HANDLER(HDIO_GET_UNMASKINTR, hdio_ioctl_trans),
	IOCTL32_HANDLER(HDIO_GET_MULTCOUNT, hdio_ioctl_trans),
	// HDIO_OBSOLETE_IDENTITY
	IOCTL32_HANDLER(HDIO_GET_KEEPSETTINGS, hdio_ioctl_trans),
	IOCTL32_HANDLER(HDIO_GET_32BIT, hdio_ioctl_trans),
	IOCTL32_HANDLER(HDIO_GET_NOWERR, hdio_ioctl_trans),
	IOCTL32_HANDLER(HDIO_GET_DMA, hdio_ioctl_trans),
	IOCTL32_HANDLER(HDIO_GET_NICE, hdio_ioctl_trans),
	IOCTL32_DEFAULT(HDIO_GET_IDENTITY),
	IOCTL32_DEFAULT(HDIO_DRIVE_RESET),
	// HDIO_TRISTATE_HWIF				/* not implemented */
	// HDIO_DRIVE_TASK				/* To do, need specs */
	IOCTL32_DEFAULT(HDIO_DRIVE_CMD),
	IOCTL32_DEFAULT(HDIO_SET_MULTCOUNT),
	IOCTL32_DEFAULT(HDIO_SET_UNMASKINTR),
	IOCTL32_DEFAULT(HDIO_SET_KEEPSETTINGS),
	IOCTL32_DEFAULT(HDIO_SET_32BIT),
	IOCTL32_DEFAULT(HDIO_SET_NOWERR),
	IOCTL32_DEFAULT(HDIO_SET_DMA),
	IOCTL32_DEFAULT(HDIO_SET_PIO_MODE),
	IOCTL32_DEFAULT(HDIO_SCAN_HWIF),
	IOCTL32_DEFAULT(HDIO_SET_NICE),
	//HDIO_UNREGISTER_HWIF

	IOCTL32_DEFAULT(BLKROSET),			/* fs.h ioctls  */
	IOCTL32_DEFAULT(BLKROGET),
	IOCTL32_DEFAULT(BLKRRPART),
	IOCTL32_DEFAULT(BLKFLSBUF),
	IOCTL32_DEFAULT(BLKRASET),
	IOCTL32_DEFAULT(BLKFRASET),
	IOCTL32_DEFAULT(BLKSECTSET),
	IOCTL32_DEFAULT(BLKSSZGET),

	/* RAID */
	IOCTL32_DEFAULT(RAID_VERSION),
	IOCTL32_DEFAULT(GET_ARRAY_INFO),
	IOCTL32_DEFAULT(GET_DISK_INFO),
	IOCTL32_DEFAULT(PRINT_RAID_DEBUG),
	IOCTL32_DEFAULT(CLEAR_ARRAY),
	IOCTL32_DEFAULT(ADD_NEW_DISK),
	IOCTL32_DEFAULT(HOT_REMOVE_DISK),
	IOCTL32_DEFAULT(SET_ARRAY_INFO),
	IOCTL32_DEFAULT(SET_DISK_INFO),
	IOCTL32_DEFAULT(WRITE_RAID_INFO),
	IOCTL32_DEFAULT(UNPROTECT_ARRAY),
	IOCTL32_DEFAULT(PROTECT_ARRAY),
	IOCTL32_DEFAULT(HOT_ADD_DISK),
	IOCTL32_DEFAULT(SET_DISK_FAULTY),
	IOCTL32_DEFAULT(RUN_ARRAY),
	IOCTL32_DEFAULT(START_ARRAY),
	IOCTL32_DEFAULT(STOP_ARRAY),
	IOCTL32_DEFAULT(STOP_ARRAY_RO),
	IOCTL32_DEFAULT(RESTART_ARRAY_RW),
	IOCTL32_DEFAULT(RAID_AUTORUN),

	/* Big K */
	IOCTL32_DEFAULT(PIO_FONT),
	IOCTL32_DEFAULT(GIO_FONT),
#ifdef CONFIG_VT
	IOCTL32_HANDLER(GIO_FONTX, do_fontx_ioctl),
	IOCTL32_HANDLER(PIO_FONTX, do_fontx_ioctl),
#endif
	IOCTL32_DEFAULT(KDSIGACCEPT),
	IOCTL32_DEFAULT(KDGETKEYCODE),
	IOCTL32_DEFAULT(KDSETKEYCODE),
	IOCTL32_DEFAULT(KIOCSOUND),
	IOCTL32_DEFAULT(KDMKTONE),
	IOCTL32_DEFAULT(KDGKBTYPE),
	IOCTL32_DEFAULT(KDSETMODE),
	IOCTL32_DEFAULT(KDGETMODE),
	IOCTL32_DEFAULT(KDSKBMODE),
	IOCTL32_DEFAULT(KDGKBMODE),
	IOCTL32_DEFAULT(KDSKBMETA),
	IOCTL32_DEFAULT(KDGKBMETA),
	IOCTL32_DEFAULT(KDGKBENT),
	IOCTL32_DEFAULT(KDSKBENT),
	IOCTL32_DEFAULT(KDGKBSENT),
	IOCTL32_DEFAULT(KDSKBSENT),
	IOCTL32_DEFAULT(KDGKBDIACR),
	IOCTL32_DEFAULT(KDKBDREP),
#ifdef CONFIG_VT
	IOCTL32_HANDLER(KDFONTOP, do_kdfontop_ioctl),
#endif
	IOCTL32_DEFAULT(KDSKBDIACR),
	IOCTL32_DEFAULT(KDGKBLED),
	IOCTL32_DEFAULT(KDSKBLED),
	IOCTL32_DEFAULT(KDGETLED),
	IOCTL32_DEFAULT(KDSETLED),
	IOCTL32_DEFAULT(GIO_SCRNMAP),
	IOCTL32_DEFAULT(PIO_SCRNMAP),
	IOCTL32_DEFAULT(GIO_UNISCRNMAP),
	IOCTL32_DEFAULT(PIO_UNISCRNMAP),
	IOCTL32_DEFAULT(PIO_FONTRESET),
	IOCTL32_DEFAULT(PIO_UNIMAPCLR),

	/* Big S */
	IOCTL32_DEFAULT(SCSI_IOCTL_GET_IDLUN),
	IOCTL32_DEFAULT(SCSI_IOCTL_PROBE_HOST),
	IOCTL32_DEFAULT(SCSI_IOCTL_GET_PCI),
	IOCTL32_DEFAULT(SCSI_IOCTL_DOORLOCK),
	IOCTL32_DEFAULT(SCSI_IOCTL_DOORUNLOCK),
	IOCTL32_DEFAULT(SCSI_IOCTL_TEST_UNIT_READY),
	IOCTL32_DEFAULT(SCSI_IOCTL_TAGGED_ENABLE),
	IOCTL32_DEFAULT(SCSI_IOCTL_TAGGED_DISABLE),
	IOCTL32_DEFAULT(SCSI_IOCTL_GET_BUS_NUMBER),
	IOCTL32_DEFAULT(SCSI_IOCTL_SEND_COMMAND),

	/* Big T */
	IOCTL32_DEFAULT(TUNSETNOCSUM),
	IOCTL32_DEFAULT(TUNSETDEBUG),
	IOCTL32_DEFAULT(TUNSETIFF),
	IOCTL32_DEFAULT(TUNSETPERSIST),
	IOCTL32_DEFAULT(TUNSETOWNER),

	/* Big V */
	IOCTL32_DEFAULT(VT_SETMODE),
	IOCTL32_DEFAULT(VT_GETMODE),
	IOCTL32_DEFAULT(VT_GETSTATE),
	IOCTL32_DEFAULT(VT_OPENQRY),
	IOCTL32_DEFAULT(VT_ACTIVATE),
	IOCTL32_DEFAULT(VT_WAITACTIVE),
	IOCTL32_DEFAULT(VT_RELDISP),
	IOCTL32_DEFAULT(VT_DISALLOCATE),
	IOCTL32_DEFAULT(VT_RESIZE),
	IOCTL32_DEFAULT(VT_RESIZEX),
	IOCTL32_DEFAULT(VT_LOCKSWITCH),
	IOCTL32_DEFAULT(VT_UNLOCKSWITCH),

#ifdef CONFIG_MD
	/* status */
	IOCTL32_DEFAULT(RAID_VERSION),
	IOCTL32_DEFAULT(GET_ARRAY_INFO),
	IOCTL32_DEFAULT(GET_DISK_INFO),
	IOCTL32_DEFAULT(PRINT_RAID_DEBUG),
	IOCTL32_DEFAULT(RAID_AUTORUN),

	/* configuration */
	IOCTL32_DEFAULT(CLEAR_ARRAY),
	IOCTL32_DEFAULT(ADD_NEW_DISK),
	IOCTL32_DEFAULT(HOT_REMOVE_DISK),
	IOCTL32_DEFAULT(SET_ARRAY_INFO),
	IOCTL32_DEFAULT(SET_DISK_INFO),
	IOCTL32_DEFAULT(WRITE_RAID_INFO),
	IOCTL32_DEFAULT(UNPROTECT_ARRAY),
	IOCTL32_DEFAULT(PROTECT_ARRAY),
	IOCTL32_DEFAULT(HOT_ADD_DISK),
	IOCTL32_DEFAULT(SET_DISK_FAULTY),

	/* usage */
	IOCTL32_DEFAULT(RUN_ARRAY),
	IOCTL32_DEFAULT(START_ARRAY),
	IOCTL32_DEFAULT(STOP_ARRAY),
	IOCTL32_DEFAULT(STOP_ARRAY_RO),
	IOCTL32_DEFAULT(RESTART_ARRAY_RW),
#endif /* CONFIG_MD */

#ifdef CONFIG_SIBYTE_TBPROF
	IOCTL32_DEFAULT(SBPROF_ZBSTART),
	IOCTL32_DEFAULT(SBPROF_ZBSTOP),
	IOCTL32_DEFAULT(SBPROF_ZBWAITFULL),
#endif /* CONFIG_SIBYTE_TBPROF */

	IOCTL32_DEFAULT(MTIOCTOP),			/* mtio.h ioctls  */
	IOCTL32_HANDLER(MTIOCGET32, mt_ioctl_trans),
	IOCTL32_HANDLER(MTIOCPOS32, mt_ioctl_trans),
	IOCTL32_HANDLER(MTIOCGETCONFIG32, mt_ioctl_trans),
	IOCTL32_HANDLER(MTIOCSETCONFIG32, mt_ioctl_trans),
	// MTIOCRDFTSEG
	// MTIOCWRFTSEG
	// MTIOCVOLINFO
	// MTIOCGETSIZE
	// MTIOCFTFORMAT
	// MTIOCFTCMD

	IOCTL32_DEFAULT(AUTOFS_IOC_READY),		/* auto_fs.h ioctls */
	IOCTL32_DEFAULT(AUTOFS_IOC_FAIL),
	IOCTL32_DEFAULT(AUTOFS_IOC_CATATONIC),
	IOCTL32_DEFAULT(AUTOFS_IOC_PROTOVER),
	IOCTL32_HANDLER(AUTOFS_IOC_SETTIMEOUT32, ioc_settimeout),
	IOCTL32_DEFAULT(AUTOFS_IOC_EXPIRE),
	IOCTL32_DEFAULT(AUTOFS_IOC_EXPIRE_MULTI),

	/* DEVFS */
	IOCTL32_DEFAULT(DEVFSDIOC_GET_PROTO_REV),
	IOCTL32_DEFAULT(DEVFSDIOC_SET_EVENT_MASK),
	IOCTL32_DEFAULT(DEVFSDIOC_RELEASE_EVENT_QUEUE),
	IOCTL32_DEFAULT(DEVFSDIOC_SET_DEBUG_MASK),

	/* Raw devices */
	IOCTL32_DEFAULT(RAW_SETBIND),
	IOCTL32_DEFAULT(RAW_GETBIND),

#if defined(CONFIG_BLK_DEV_LVM) || defined(CONFIG_BLK_DEV_LVM_MODULE)
	/* 0xfe - lvm */
	IOCTL32_DEFAULT(VG_SET_EXTENDABLE),
	IOCTL32_DEFAULT(VG_STATUS_GET_COUNT),
	IOCTL32_DEFAULT(VG_STATUS_GET_NAMELIST),
	IOCTL32_DEFAULT(VG_REMOVE),
	IOCTL32_DEFAULT(VG_RENAME),
	IOCTL32_DEFAULT(VG_REDUCE),
	IOCTL32_DEFAULT(PE_LOCK_UNLOCK),
	IOCTL32_DEFAULT(PV_FLUSH),
	IOCTL32_DEFAULT(LVM_LOCK_LVM),
	IOCTL32_DEFAULT(LVM_GET_IOP_VERSION),
#ifdef LVM_TOTAL_RESET
	IOCTL32_DEFAULT(LVM_RESET),
#endif
	IOCTL32_DEFAULT(LV_SET_ACCESS),
	IOCTL32_DEFAULT(LV_SET_STATUS),
	IOCTL32_DEFAULT(LV_SET_ALLOCATION),
	IOCTL32_DEFAULT(LE_REMAP),
	IOCTL32_DEFAULT(LV_BMAP),
	IOCTL32_DEFAULT(LV_SNAPSHOT_USE_RATE),
#endif /* LVM */

	/* Little p (/dev/rtc, /dev/envctrl, etc.) */
	IOCTL32_DEFAULT(_IOR('p', 20, int[7])), /* RTCGET */
	IOCTL32_DEFAULT(_IOW('p', 21, int[7])), /* RTCSET */
	IOCTL32_DEFAULT(RTC_AIE_ON),
	IOCTL32_DEFAULT(RTC_AIE_OFF),
	IOCTL32_DEFAULT(RTC_UIE_ON),
	IOCTL32_DEFAULT(RTC_UIE_OFF),
	IOCTL32_DEFAULT(RTC_PIE_ON),
	IOCTL32_DEFAULT(RTC_PIE_OFF),
	IOCTL32_DEFAULT(RTC_WIE_ON),
	IOCTL32_DEFAULT(RTC_WIE_OFF),
	IOCTL32_DEFAULT(RTC_ALM_SET),
	IOCTL32_DEFAULT(RTC_ALM_READ),
	IOCTL32_DEFAULT(RTC_RD_TIME),
	IOCTL32_DEFAULT(RTC_SET_TIME),
	IOCTL32_DEFAULT(RTC_WKALM_SET),
	IOCTL32_DEFAULT(RTC_WKALM_RD),

#ifdef CONFIG_MTD_CHAR
	/* Big M */
	IOCTL32_DEFAULT(MEMGETINFO),
	IOCTL32_DEFAULT(MEMERASE),
	// IOCTL32_DEFAULT(MEMWRITEOOB32, mtd_rw_oob),
	// IOCTL32_DEFAULT(MEMREADOOB32, mtd_rw_oob),
	IOCTL32_DEFAULT(MEMLOCK),
	IOCTL32_DEFAULT(MEMUNLOCK),
	IOCTL32_DEFAULT(MEMGETREGIONCOUNT),
	IOCTL32_DEFAULT(MEMGETREGIONINFO),
#endif
};

#define NR_IOCTL32_HANDLERS	(sizeof(ioctl32_handler_table) /	\
				 sizeof(ioctl32_handler_table[0]))

static struct ioctl32_list *ioctl32_hash_table[1024];

static inline int ioctl32_hash(unsigned int cmd)
{
	return ((cmd >> 6) ^ (cmd >> 4) ^ cmd) & 0x3ff;
}

int sys32_ioctl(unsigned int fd, unsigned int cmd, unsigned int arg)
{
	int (*handler)(unsigned int, unsigned int, unsigned long, struct file * filp);
	struct file *filp;
	struct ioctl32_list *l;
	int error;

	l = ioctl32_hash_table[ioctl32_hash(cmd)];

	error = -EBADF;

	filp = fget(fd);
	if (!filp)
		return error;

	if (!filp->f_op || !filp->f_op->ioctl) {
		error = sys_ioctl (fd, cmd, arg);
		goto out;
	}

	while (l && l->handler.cmd != cmd)
		l = l->next;

	if (l) {
		handler = (void *)l->handler.function;
		error = handler(fd, cmd, arg, filp);
	} else {
		error = -EINVAL;
		printk("unknown ioctl: %08x\n", cmd);
	}
out:
	fput(filp);
	return error;
}

static void ioctl32_insert(struct ioctl32_list *entry)
{
	int hash = ioctl32_hash(entry->handler.cmd);
	if (!ioctl32_hash_table[hash])
		ioctl32_hash_table[hash] = entry;
	else {
		struct ioctl32_list *l;
		l = ioctl32_hash_table[hash];
		while (l->next)
			l = l->next;
		l->next = entry;
		entry->next = 0;
	}
}

static int __init init_ioctl32(void)
{
	int i;
	for (i = 0; i < NR_IOCTL32_HANDLERS; i++)
		ioctl32_insert(&ioctl32_handler_table[i]);
	return 0;
}

__initcall(init_ioctl32);
