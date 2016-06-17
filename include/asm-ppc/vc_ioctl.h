#ifndef _LINUX_VC_IOCTL_H
#define _LINUX_VC_IOCTL_H

struct vc_mode {
	int	height;
	int	width;
	int	depth;
	int	pitch;
	int	mode;
	char	name[32];
	unsigned long fb_address;
	unsigned long cmap_adr_address;
	unsigned long cmap_data_address;
	unsigned long disp_reg_address;
};

#define VC_GETMODE	0x7667
#define VC_SETMODE	0x7668
#define VC_INQMODE	0x7669

#define VC_SETCMAP      0x766a
#define VC_GETCMAP      0x766b

#define VC_POWERMODE	0x766c

/* Values for the argument to the VC_POWERMODE ioctl */
#define VC_POWERMODE_INQUIRY	(-1)
#define VESA_NO_BLANKING	0
#define VESA_VSYNC_SUSPEND	1
#define VESA_HSYNC_SUSPEND	2
#define VESA_POWERDOWN		3

#ifdef __KERNEL__
extern int console_getmode(struct vc_mode *);
extern int console_setmode(struct vc_mode *, int);
extern int console_setcmap(int, unsigned char *, unsigned char *,
			   unsigned char *);
extern int console_powermode(int);
extern struct vc_mode display_info;
extern struct fb_info *console_fb_info;
#endif /* __KERNEL__ */

#endif /* _LINUX_VC_IOCTL_H */
