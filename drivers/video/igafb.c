/*
 *  linux/drivers/video/igafb.c -- Frame buffer device for IGA 1682
 *
 *      Copyright (C) 1998  Vladimir Roganov and Gleb Raiko
 *
 *  This driver is partly based on the Frame buffer device for ATI Mach64
 *  and partially on VESA-related code.
 *
 *      Copyright (C) 1997-1998  Geert Uytterhoeven
 *      Copyright (C) 1998  Bernd Harries
 *      Copyright (C) 1998  Eddie C. Dost  (ecd@skynet.be)
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

/******************************************************************************

  TODO:
       Despite of IGA Card has advanced graphic acceleration, 
       initial version is almost dummy and does not support it.
       Support for video modes and acceleration must be added
       together with accelerated X-Windows driver implementation.

       Most important thing at this moment is that we have working
       JavaEngine1  console & X  with new console interface.

******************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fb.h>
#include <linux/selection.h>
#include <linux/console.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/nvram.h>
#include <linux/kd.h>
#include <linux/vt_kern.h>

#include <asm/io.h>

#ifdef __sparc__
#include <asm/pbm.h>
#include <asm/pcic.h>
#endif

#include <video/fbcon.h>
#include <video/fbcon-cfb8.h>
#include <video/fbcon-cfb16.h>
#include <video/fbcon-cfb24.h>
#include <video/fbcon-cfb32.h>

#include "iga.h"

static char igafb_name[16] = "IGA 1682";
static char fontname[40] __initdata = { 0 };

struct pci_mmap_map {
    unsigned long voff;
    unsigned long poff;
    unsigned long size;
    unsigned long prot_flag;
    unsigned long prot_mask;
};

struct fb_info_iga {
    struct fb_info fb_info;
    unsigned long frame_buffer_phys;
    char *frame_buffer;
    unsigned long io_base_phys;
    unsigned long io_base;
    u32 total_vram;
    struct pci_mmap_map *mmap_map;
    struct { u_short blue, green, red, pad; } palette[256];
    int video_cmap_len;
    int currcon;
    struct display disp;
    struct display_switch dispsw; 
    union {
#ifdef FBCON_HAS_CFB16
	    u16 cfb16[16];  
#endif
#ifdef FBCON_HAS_CFB24
	    u32 cfb24[16];
#endif
#ifdef FBCON_HAS_CFB32
	    u32 cfb32[16];
#endif
    } fbcon_cmap;
#ifdef __sparc__
    u8 open;
    u8 mmaped;
    int vtconsole;
    int consolecnt;
#endif
};

struct fb_var_screeninfo default_var = {
    /* 640x480, 60 Hz, Non-Interlaced (25.175 MHz dotclock) */
    640, 480, 640, 480, 0, 0, 8, 0,
    {0, 8, 0}, {0, 8, 0}, {0, 8, 0}, {0, 0, 0},
    0, 0, -1, -1, 0, 39722, 48, 16, 33, 10, 96, 2,
    0, FB_VMODE_NONINTERLACED
};

#ifdef __sparc__
struct fb_var_screeninfo default_var_1024x768 __initdata = {
    /* 1024x768, 75 Hz, Non-Interlaced (78.75 MHz dotclock) */
    1024, 768, 1024, 768, 0, 0, 8, 0,
    {0, 8, 0}, {0, 8, 0}, {0, 8, 0}, {0, 0, 0},
    0, 0, -1, -1, 0, 12699, 176, 16, 28, 1, 96, 3,
    FB_SYNC_HOR_HIGH_ACT|FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED
};

struct fb_var_screeninfo default_var_1152x900 __initdata = {
    /* 1152x900, 76 Hz, Non-Interlaced (110.0 MHz dotclock) */
    1152, 900, 1152, 900, 0, 0, 8, 0,
    {0, 8, 0}, {0, 8, 0}, {0, 8, 0}, {0, 0, 0},
    0, 0, -1, -1, 0, 9091, 234, 24, 34, 3, 100, 3,
    FB_SYNC_HOR_HIGH_ACT|FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED
};

struct fb_var_screeninfo default_var_1280x1024 __initdata = {
    /* 1280x1024, 75 Hz, Non-Interlaced (135.00 MHz dotclock) */
    1280, 1024, 1280, 1024, 0, 0, 8, 0,
    {0, 8, 0}, {0, 8, 0}, {0, 8, 0}, {0, 0, 0},
    0, 0, -1, -1, 0, 7408, 248, 16, 38, 1, 144, 3,
    FB_SYNC_HOR_HIGH_ACT|FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED
};

/*
 *   Memory-mapped I/O functions for Sparc PCI
 *
 * On sparc we happen to access I/O with memory mapped functions too.
 */ 
#define pci_inb(info, reg)        readb(info->io_base+(reg))
#define pci_outb(info, val, reg)  writeb(val, info->io_base+(reg))

static inline unsigned int iga_inb(struct fb_info_iga *info,
				   unsigned int reg, unsigned int idx )
{
        pci_outb(info, idx, reg);
        return pci_inb(info, reg + 1);
}

static inline void iga_outb(struct fb_info_iga *info, unsigned char val,
			    unsigned int reg, unsigned int idx )
{
        pci_outb(info, idx, reg);
        pci_outb(info, val, reg+1);
}

#endif /* __sparc__ */

/*
 *  Very important functionality for the JavaEngine1 computer:
 *  make screen border black (usign special IGA registers) 
 */
static void iga_blank_border(struct fb_info_iga *info)
{
        int i;

#if 0
	/*
	 * PROM does this for us, so keep this code as a reminder
	 * about required read from 0x3DA and writing of 0x20 in the end.
	 */
	(void) pci_inb(info, 0x3DA);		/* required for every access */
	pci_outb(info, IGA_IDX_VGA_OVERSCAN, IGA_ATTR_CTL);
	(void) pci_inb(info, IGA_ATTR_CTL+1);
	pci_outb(info, 0x38, IGA_ATTR_CTL);
	pci_outb(info, 0x20, IGA_ATTR_CTL);	/* re-enable visual */
#endif
	/*
	 * This does not work as it was designed because the overscan
	 * color is looked up in the palette. Therefore, under X11
	 * overscan changes color.
	 */
	for (i=0; i < 3; i++)
		iga_outb(info, 0, IGA_EXT_CNTRL, IGA_IDX_OVERSCAN_COLOR + i);
}


/*
 *  Frame buffer device API
 */

static int igafb_update_var(int con, struct fb_info *info)
{
        return 0;
}

static int igafb_get_fix(struct fb_fix_screeninfo *fix, int con,
                         struct fb_info *info)
{
        struct fb_info_iga *fb = (struct fb_info_iga*)info;

        memset(fix, 0, sizeof(struct fb_fix_screeninfo));
        strcpy(fix->id, igafb_name);

        fix->smem_start = (unsigned long) fb->frame_buffer;
        fix->smem_len = fb->total_vram;
        fix->xpanstep = 0;
        fix->ypanstep = 0;
        fix->ywrapstep = 0;

	fix->type = FB_TYPE_PACKED_PIXELS;
	fix->type_aux = 0;
	fix->line_length = default_var.xres * (default_var.bits_per_pixel/8);
	fix->visual = default_var.bits_per_pixel <= 8 ? FB_VISUAL_PSEUDOCOLOR
		                                      : FB_VISUAL_DIRECTCOLOR;
        return 0;
}

static int igafb_get_var(struct fb_var_screeninfo *var, int con,
                         struct fb_info *info)
{
        if(con == -1)
                memcpy(var, &default_var, sizeof(struct fb_var_screeninfo));
        else
                *var = fb_display[con].var;
        return 0;
}

static int igafb_set_var(struct fb_var_screeninfo *var, int con,
                         struct fb_info *info)
{
        memcpy(var, &default_var, sizeof(struct fb_var_screeninfo));
        return 0;
}

#ifdef __sparc__
static int igafb_mmap(struct fb_info *info, struct file *file,
		      struct vm_area_struct *vma)
{
	struct fb_info_iga *fb = (struct fb_info_iga *)info;
	unsigned int size, page, map_size = 0;
	unsigned long map_offset = 0;
	int i;

	if (!fb->mmap_map)
		return -ENXIO;

	size = vma->vm_end - vma->vm_start;

	/* To stop the swapper from even considering these pages. */
	vma->vm_flags |= (VM_SHM | VM_LOCKED);

	/* Each page, see which map applies */
	for (page = 0; page < size; ) {
		map_size = 0;
		for (i = 0; fb->mmap_map[i].size; i++) {
			unsigned long start = fb->mmap_map[i].voff;
			unsigned long end = start + fb->mmap_map[i].size;
			unsigned long offset = (vma->vm_pgoff << PAGE_SHIFT) + page;

			if (start > offset)
				continue;
			if (offset >= end)
				continue;

			map_size = fb->mmap_map[i].size - (offset - start);
			map_offset = fb->mmap_map[i].poff + (offset - start);
			break;
		}
		if (!map_size) {
			page += PAGE_SIZE;
			continue;
		}
		if (page + map_size > size)
			map_size = size - page;

		pgprot_val(vma->vm_page_prot) &= ~(fb->mmap_map[i].prot_mask);
		pgprot_val(vma->vm_page_prot) |= fb->mmap_map[i].prot_flag;

		if (remap_page_range(vma->vm_start + page, map_offset,
				     map_size, vma->vm_page_prot))
			return -EAGAIN;

		page += map_size;
	}

	if (!map_size)
		return -EINVAL;

	if (!fb->mmaped) {
		int lastconsole = 0;

		if (info->display_fg)
			lastconsole = info->display_fg->vc_num;
		fb->mmaped = 1;
		if (fb->consolecnt && fb_display[lastconsole].fb_info ==info) {
			fb->vtconsole = lastconsole;
			vt_cons[lastconsole]->vc_mode = KD_GRAPHICS;
		}
	}
	return 0;
}
#endif /* __sparc__ */


static int iga_getcolreg(unsigned regno, unsigned *red, unsigned *green,
                          unsigned *blue, unsigned *transp,
                          struct fb_info *fb_info)
{
        /*
         *  Read a single color register and split it into colors/transparent.
         *  Return != 0 for invalid regno.
         */
	struct fb_info_iga *info = (struct fb_info_iga*) fb_info;

        if (regno >= info->video_cmap_len)
                return 1;

	*red    = info->palette[regno].red;
	*green  = info->palette[regno].green;
	*blue   = info->palette[regno].blue;
	*transp = 0;
	return 0;
}

static int iga_setcolreg(unsigned regno, unsigned red, unsigned green,
                          unsigned blue, unsigned transp,
                          struct fb_info *fb_info)
{
        /*
         *  Set a single color register. The values supplied are
         *  already rounded down to the hardware's capabilities
         *  (according to the entries in the `var' structure). Return
         *  != 0 for invalid regno.
         */
        
	struct fb_info_iga *info = (struct fb_info_iga*) fb_info;

        if (regno >= info->video_cmap_len)
                return 1;

        info->palette[regno].red   = red;
        info->palette[regno].green = green;
        info->palette[regno].blue  = blue;

	pci_outb(info, regno, DAC_W_INDEX);
	pci_outb(info, red,   DAC_DATA);
	pci_outb(info, green, DAC_DATA);
	pci_outb(info, blue,  DAC_DATA);

	if (regno < 16) {
		switch (default_var.bits_per_pixel) {
#ifdef FBCON_HAS_CFB16
		case 16:
			info->fbcon_cmap.cfb16[regno] = 
				(regno << 10) | (regno << 5) | regno;
			break;
#endif
#ifdef FBCON_HAS_CFB24
		case 24:
			info->fbcon_cmap.cfb24[regno] = 
				(regno << 16) | (regno << 8) | regno;
		break;
#endif
#ifdef FBCON_HAS_CFB32
		case 32:
			{ int i;
			i = (regno << 8) | regno;
			info->fbcon_cmap.cfb32[regno] = (i << 16) | i;
			}
			break;
#endif
		}
	}
	return 0;
}

static void do_install_cmap(int con, struct fb_info *fb_info)
{
	struct fb_info_iga *info = (struct fb_info_iga*) fb_info;

        if (con != info->currcon)
                return;
        if (fb_display[con].cmap.len)
                fb_set_cmap(&fb_display[con].cmap, 1,
                            iga_setcolreg, &info->fb_info);
        else
                fb_set_cmap(fb_default_cmap(info->video_cmap_len), 1, 
			    iga_setcolreg, &info->fb_info);
}

static int igafb_get_cmap(struct fb_cmap *cmap, int kspc, int con,
                           struct fb_info *fb_info)
{
	struct fb_info_iga *info = (struct fb_info_iga*) fb_info;
	
        if (con == info->currcon) /* current console? */
                return fb_get_cmap(cmap, kspc, iga_getcolreg, &info->fb_info);
        else if (fb_display[con].cmap.len) /* non default colormap? */
                fb_copy_cmap(&fb_display[con].cmap, cmap, kspc ? 0 : 2);
        else
                fb_copy_cmap(fb_default_cmap(info->video_cmap_len),
                     cmap, kspc ? 0 : 2);
        return 0;
}

static int igafb_set_cmap(struct fb_cmap *cmap, int kspc, int con,
	                  struct fb_info *info)
{
        int err;
	struct fb_info_iga *fb = (struct fb_info_iga*) info;

        if (!fb_display[con].cmap.len) {        /* no colormap allocated? */
                err = fb_alloc_cmap(&fb_display[con].cmap,
				    fb->video_cmap_len,0);
                if (err)
                        return err;
        }
        if (con == fb->currcon)                     /* current console? */
                return fb_set_cmap(cmap, kspc, iga_setcolreg, info);
        else
                fb_copy_cmap(cmap, &fb_display[con].cmap, kspc ? 0 : 1);
        return 0;
}

/*
 * Framebuffer option structure
 */
static struct fb_ops igafb_ops = {
	owner:		THIS_MODULE,
	fb_get_fix:	igafb_get_fix,
	fb_get_var:	igafb_get_var,
	fb_set_var:	igafb_set_var,
	fb_get_cmap:	igafb_get_cmap,
	fb_set_cmap:	igafb_set_cmap,
#ifdef __sparc__
	fb_mmap:	igafb_mmap,
#endif
};

static void igafb_set_disp(int con, struct fb_info_iga *info)
{
        struct fb_fix_screeninfo fix;
        struct display *display;
        struct display_switch *sw;

        if (con >= 0)
                display = &fb_display[con];
        else 
                display = &info->disp;        /* used during initialization */

        igafb_get_fix(&fix, con, &info->fb_info);

        memset(display, 0, sizeof(struct display));
        display->screen_base = info->frame_buffer;
        display->visual = fix.visual;
        display->type = fix.type;
        display->type_aux = fix.type_aux;
        display->ypanstep = fix.ypanstep;
        display->ywrapstep = fix.ywrapstep;
        display->line_length = fix.line_length;
        display->next_line = fix.line_length;
        display->can_soft_blank = 0; 
        display->inverse = 0;
        igafb_get_var(&display->var, -1, &info->fb_info);

        switch (default_var.bits_per_pixel) {
#ifdef FBCON_HAS_CFB8
        case 8:
                sw = &fbcon_cfb8;
                break;
#endif
#ifdef FBCON_HAS_CFB16
        case 15:
        case 16:
                sw = &fbcon_cfb16;
		display->dispsw_data = info->fbcon_cmap.cfb16;
                break;
#endif
#ifdef FBCON_HAS_CFB24
	case 24:
		sw = &fbcon_cfb24;
		display->dispsw_data = info->fbcon_cmap.cfb24;
		break;
#endif
#ifdef FBCON_HAS_CFB32
        case 32:
                sw = &fbcon_cfb32;
		display->dispsw_data = info->fbcon_cmap.cfb32;
                break;
#endif
        default:
		printk(KERN_WARNING "igafb_set_disp: unknown resolution %d\n",
		    default_var.bits_per_pixel);
                return;
        }
        memcpy(&info->dispsw, sw, sizeof(*sw));
        display->dispsw = &info->dispsw;

	display->scrollmode = SCROLL_YREDRAW;
	info->dispsw.bmove = fbcon_redraw_bmove;
}

static int igafb_switch(int con, struct fb_info *fb_info)
{
	struct fb_info_iga *info = (struct fb_info_iga*) fb_info;

        /* Do we have to save the colormap? */
        if (fb_display[info->currcon].cmap.len)
                fb_get_cmap(&fb_display[info->currcon].cmap, 1,
                            iga_getcolreg, fb_info);

	info->currcon = con;
	/* Install new colormap */
	do_install_cmap(con, fb_info);
	igafb_update_var(con, fb_info);
        return 1;
}



/* 0 unblank, 1 blank, 2 no vsync, 3 no hsync, 4 off */

static void igafb_blank(int blank, struct fb_info *info)
{
        /* Not supported */
}


static int __init iga_init(struct fb_info_iga *info)
{
        char vramsz = iga_inb(info, IGA_EXT_CNTRL, IGA_IDX_EXT_BUS_CNTL) 
		                                         & MEM_SIZE_ALIAS;
        switch (vramsz) {
        case MEM_SIZE_1M:
                info->total_vram = 0x100000;
                break;
        case MEM_SIZE_2M:
                info->total_vram = 0x200000;
                break;
        case MEM_SIZE_4M:
        case MEM_SIZE_RESERVED:
                info->total_vram = 0x400000;
                break;
        }

        if (default_var.bits_per_pixel > 8) {
                info->video_cmap_len = 16;
        } else {
                info->video_cmap_len = 256;
        }
	{
		int j, k;
		for (j = 0; j < 16; j++) {
			k = color_table[j];
			info->palette[j].red = default_red[k];
			info->palette[j].green = default_grn[k];
			info->palette[j].blue = default_blu[k];
		}
	}

	strcpy(info->fb_info.modename, igafb_name);
	info->fb_info.node = -1;
	info->fb_info.fbops = &igafb_ops;
	info->fb_info.disp = &info->disp;
	strcpy(info->fb_info.fontname, fontname);
	info->fb_info.changevar = NULL;
	info->fb_info.switch_con = &igafb_switch;
	info->fb_info.updatevar = &igafb_update_var;
	info->fb_info.blank = &igafb_blank;
	info->fb_info.flags=FBINFO_FLAG_DEFAULT;

	igafb_set_disp(-1, info);

	if (register_framebuffer(&info->fb_info) < 0)
		return 0;

	printk("fb%d: %s frame buffer device at 0x%08lx [%dMB VRAM]\n",
	       GET_FB_IDX(info->fb_info.node), igafb_name, 
	       info->frame_buffer_phys, info->total_vram >> 20);

	iga_blank_border(info); 
	return 1;
}

int __init igafb_init(void)
{
        struct pci_dev *pdev;
        struct fb_info_iga *info;
        unsigned long addr;
        extern int con_is_present(void);
	int iga2000 = 0;

        /* Do not attach when we have a serial console. */
        if (!con_is_present())
                return -ENXIO;

        pdev = pci_find_device(PCI_VENDOR_ID_INTERG, 
                               PCI_DEVICE_ID_INTERG_1682, 0);
	if (pdev == NULL) {
		/*
		 * XXX We tried to use cyber2000fb.c for IGS 2000.
		 * But it does not initialize the chip in JavaStation-E, alas.
		 */
        	pdev = pci_find_device(PCI_VENDOR_ID_INTERG, 0x2000, 0);
        	if(pdev == NULL) {
        	        return -ENXIO;
		}
		iga2000 = 1;
	}

        info = kmalloc(sizeof(struct fb_info_iga), GFP_ATOMIC);
        if (!info) {
                printk("igafb_init: can't alloc fb_info_iga\n");
                return -ENOMEM;
        }
        memset(info, 0, sizeof(struct fb_info_iga));

	if ((addr = pdev->resource[0].start) == 0) {
                printk("igafb_init: no memory start\n");
		kfree(info);
		return -ENXIO;
	}

	if ((info->frame_buffer = ioremap(addr, 1024*1024*2)) == 0) {
                printk("igafb_init: can't remap %lx[2M]\n", addr);
		kfree(info);
		return -ENXIO;
	}

	info->frame_buffer_phys = addr & PCI_BASE_ADDRESS_MEM_MASK;

#ifdef __sparc__
	/*
	 * The following is sparc specific and this is why:
	 *
	 * IGS2000 has its I/O memory mapped and we want
	 * to generate memory cycles on PCI, e.g. do ioremap(),
	 * then readb/writeb() as in Documentation/IO-mapping.txt.
	 *
	 * IGS1682 is more traditional, it responds to PCI I/O
	 * cycles, so we want to access it with inb()/outb().
	 *
	 * On sparc, PCIC converts CPU memory access within
	 * phys window 0x3000xxxx into PCI I/O cycles. Therefore
	 * we may use readb/writeb to access them with IGS1682.
	 *
	 * We do not take io_base_phys from resource[n].start
	 * on IGS1682 because that chip is BROKEN. It does not
	 * have a base register for I/O. We just "know" what its
	 * I/O addresses are.
	 */
	if (iga2000) {
		info->io_base_phys = info->frame_buffer_phys | 0x00800000;
	} else {
		info->io_base_phys = 0x30000000;	/* XXX */
	}
	if ((info->io_base = (int) ioremap(info->io_base_phys, 0x1000)) == 0) {
                printk("igafb_init: can't remap %lx[4K]\n", info->io_base_phys);
		iounmap((void *)info->frame_buffer);
                kfree(info);
		return -ENXIO;
	}

	/*
	 * Figure mmap addresses from PCI config space.
	 * We need two regions: for video memory and for I/O ports.
	 * Later one can add region for video coprocessor registers.
	 * However, mmap routine loops until size != 0, so we put
	 * one additional region with size == 0. 
	 */

	info->mmap_map = kmalloc(4 * sizeof(*info->mmap_map), GFP_ATOMIC);
	if (!info->mmap_map) {
		printk("igafb_init: can't alloc mmap_map\n");
		iounmap((void *)info->io_base);
		iounmap(info->frame_buffer);
                kfree(info);
		return -ENOMEM;
	}

	memset(info->mmap_map, 0, 4 * sizeof(*info->mmap_map));

	/*
	 * Set default vmode and cmode from PROM properties.
	 */
	{
                struct pcidev_cookie *cookie = pdev->sysdata;
                int node = cookie->prom_node;
                int width = prom_getintdefault(node, "width", 1024);
                int height = prom_getintdefault(node, "height", 768);
                int depth = prom_getintdefault(node, "depth", 8);
                switch (width) {
                    case 1024:
                        if (height == 768)
                            default_var = default_var_1024x768;
                        break;
                    case 1152:
                        if (height == 900)
                            default_var = default_var_1152x900;
                        break;
                    case 1280:
                        if (height == 1024)
                            default_var = default_var_1280x1024;
                        break;
                    default:
                        break;
                }

                switch (depth) {
                    case 8:
                        default_var.bits_per_pixel = 8;
                        break;
                    case 16:
                        default_var.bits_per_pixel = 16;
                        break;
                    case 24:
                        default_var.bits_per_pixel = 24;
                        break;
                    case 32:
                        default_var.bits_per_pixel = 32;
                        break;
                    default:
                        break;
                }
            }

#endif

	if (!iga_init(info)) {
		iounmap((void *)info->io_base);
		iounmap(info->frame_buffer);
		if (info->mmap_map)
			kfree(info->mmap_map);
		kfree(info);
        }

#ifdef __sparc__
	    /*
	     * Add /dev/fb mmap values.
	     */
	    
	    /* First region is for video memory */
	    info->mmap_map[0].voff = 0x0;  
	    info->mmap_map[0].poff = info->frame_buffer_phys & PAGE_MASK;
	    info->mmap_map[0].size = info->total_vram   & PAGE_MASK;
	    info->mmap_map[0].prot_mask = SRMMU_CACHE;
	    info->mmap_map[0].prot_flag = SRMMU_WRITE;

	    /* Second region is for I/O ports */
	    info->mmap_map[1].voff = info->frame_buffer_phys & PAGE_MASK;
	    info->mmap_map[1].poff = info->io_base_phys & PAGE_MASK;
	    info->mmap_map[1].size = PAGE_SIZE * 2; /* X wants 2 pages */
	    info->mmap_map[1].prot_mask = SRMMU_CACHE;
	    info->mmap_map[1].prot_flag = SRMMU_WRITE;
#endif /* __sparc__ */

	return 0;
}

int __init igafb_setup(char *options)
{
    char *this_opt;

    if (!options || !*options)
        return 0;

    while ((this_opt = strsep(&options, ",")) != NULL) {
        if (!strncmp(this_opt, "font:", 5)) {
                char *p;
                int i;

                p = this_opt + 5;
                for (i = 0; i < sizeof(fontname) - 1; i++)
                        if (!*p || *p == ' ' || *p == ',')
                                break;
                memcpy(fontname, this_opt + 5, i);
                fontname[i] = 0;
        }
    }
    return 0;
}

MODULE_LICENSE("GPL");
