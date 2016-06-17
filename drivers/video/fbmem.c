/*
 *  linux/drivers/video/fbmem.c
 *
 *  Copyright (C) 1994 Martin Schaller
 *
 *	2001 - Documented with DocBook
 *	- Brad Douglas <brad@neruo.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

#include <linux/config.h>
#include <linux/module.h>

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/smp_lock.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/slab.h>
#include <linux/mman.h>
#include <linux/tty.h>
#include <linux/console.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#ifdef CONFIG_KMOD
#include <linux/kmod.h>
#endif
#include <linux/devfs_fs_kernel.h>

#if defined(__mc68000__) || defined(CONFIG_APUS)
#include <asm/setup.h>
#endif

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/page.h>
#include <asm/pgtable.h>

#include <linux/fb.h>
#include <video/fbcon.h>

    /*
     *  Frame buffer device initialization and setup routines
     */

extern int acornfb_init(void);
extern int acornfb_setup(char*);
extern int amifb_init(void);
extern int amifb_setup(char*);
extern int atafb_init(void);
extern int atafb_setup(char*);
extern int macfb_init(void);
extern int macfb_setup(char*);
extern int cyberfb_init(void);
extern int cyberfb_setup(char*);
extern int pm2fb_init(void);
extern int pm2fb_setup(char*);
extern int pm3fb_init(void);
extern int pm3fb_setup(char*);
extern int cyber2000fb_init(void);
extern int retz3fb_init(void);
extern int retz3fb_setup(char*);
extern int clgenfb_init(void);
extern int clgenfb_setup(char*);
extern int hitfb_init(void);
extern int vfb_init(void);
extern int vfb_setup(char*);
extern int offb_init(void);
extern int atyfb_init(void);
extern int atyfb_setup(char*);
extern int aty128fb_init(void);
extern int aty128fb_setup(char*);
extern int neofb_init(void);
extern int neofb_setup(char*);
extern int igafb_init(void);
extern int igafb_setup(char*);
extern int imsttfb_init(void);
extern int imsttfb_setup(char*);
extern int dnfb_init(void);
extern int tgafb_init(void);
extern int tgafb_setup(char*);
extern int virgefb_init(void);
extern int virgefb_setup(char*);
extern int resolver_video_setup(char*);
extern int s3triofb_init(void);
extern int vesafb_init(void);
extern int vesafb_setup(char*);
extern int vga16fb_init(void);
extern int vga16fb_setup(char*);
extern int hgafb_init(void);
extern int hgafb_setup(char*);
extern int matroxfb_init(void);
extern int matroxfb_setup(char*);
extern int hpfb_init(void);
extern int sbusfb_init(void);
extern int sbusfb_setup(char*);
extern int control_init(void);
extern int control_setup(char*);
extern int platinum_init(void);
extern int platinum_setup(char*);
extern int valkyriefb_init(void);
extern int valkyriefb_setup(char*);
extern int chips_init(void);
extern int g364fb_init(void);
extern int sa1100fb_init(void);
extern int fm2fb_init(void);
extern int fm2fb_setup(char*);
extern int q40fb_init(void);
extern int sun3fb_init(void);
extern int sun3fb_setup(char *);
extern int sgivwfb_init(void);
extern int sgivwfb_setup(char*);
extern int rivafb_init(void);
extern int rivafb_setup(char*);
extern int tdfxfb_init(void);
extern int tdfxfb_setup(char*);
extern int tridentfb_init(void);
extern int tridentfb_setup(char*);
extern int sisfb_init(void);
extern int sisfb_setup(char*);
extern int stifb_init(void);
extern int stifb_setup(char*);
extern int pmagaafb_init(void);
extern int pmagbafb_init(void);
extern int pmagbbfb_init(void);
extern int maxinefb_init(void);
extern int tx3912fb_init(void);
extern int radeonfb_init(void);
extern int radeonfb_setup(char*);
extern int intelfb_init(void);
extern int intelfb_setup(char*);
extern int e1355fb_init(void);
extern int e1355fb_setup(char*);
extern int e1356fb_init(void);
extern int e1356fb_setup(char*);
extern int au1100fb_init(void);
extern int au1100fb_setup(char*);
extern int pvr2fb_init(void);
extern int pvr2fb_setup(char*);
extern int sstfb_init(void);
extern int sstfb_setup(char*);
extern int it8181fb_init(void);
extern int it8181fb_setup(char*);

static struct {
	const char *name;
	int (*init)(void);
	int (*setup)(char*);
} fb_drivers[] __initdata = {

#ifdef CONFIG_FB_SBUS
	/*
	 * Sbusfb must be initialized _before_ other frame buffer devices that
	 * use PCI probing
	 */
	{ "sbus", sbusfb_init, sbusfb_setup },
#endif

	/*
	 * Chipset specific drivers that use resource management
	 */

#ifdef CONFIG_FB_RETINAZ3
	{ "retz3", retz3fb_init, retz3fb_setup },
#endif
#ifdef CONFIG_FB_AMIGA
	{ "amifb", amifb_init, amifb_setup },
#endif
#ifdef CONFIG_FB_CYBER
	{ "cyber", cyberfb_init, cyberfb_setup },
#endif
#ifdef CONFIG_FB_CYBER2000
	{ "cyber2000", cyber2000fb_init, NULL },
#endif
#ifdef CONFIG_FB_PM2
	{ "pm2fb", pm2fb_init, pm2fb_setup },
#endif
#ifdef CONFIG_FB_PM3
	{ "pm3fb", pm3fb_init, pm3fb_setup },
#endif
#ifdef CONFIG_FB_CLGEN
	{ "clgen", clgenfb_init, clgenfb_setup },
#endif
#ifdef CONFIG_FB_ATY
	{ "atyfb", atyfb_init, atyfb_setup },
#endif
#ifdef CONFIG_FB_MATROX
	{ "matrox", matroxfb_init, matroxfb_setup },
#endif
#ifdef CONFIG_FB_ATY128
	{ "aty128fb", aty128fb_init, aty128fb_setup },
#endif
#ifdef CONFIG_FB_NEOMAGIC
	{ "neo", neofb_init, neofb_setup },
#endif
#ifdef CONFIG_FB_VIRGE
	{ "virge", virgefb_init, virgefb_setup },
#endif
#ifdef CONFIG_FB_RIVA
	{ "riva", rivafb_init, rivafb_setup },
#endif
#ifdef CONFIG_FB_RADEON
	{ "radeon", radeonfb_init, radeonfb_setup },
#endif
#ifdef CONFIG_FB_INTEL
	{ "intelfb", intelfb_init, intelfb_setup },
#endif
#ifdef CONFIG_FB_CONTROL
	{ "controlfb", control_init, control_setup },
#endif
#ifdef CONFIG_FB_PLATINUM
	{ "platinumfb", platinum_init, platinum_setup },
#endif
#ifdef CONFIG_FB_VALKYRIE
	{ "valkyriefb", valkyriefb_init, valkyriefb_setup },
#endif
#ifdef CONFIG_FB_CT65550
	{ "chipsfb", chips_init, NULL },
#endif
#ifdef CONFIG_FB_IMSTT
	{ "imsttfb", imsttfb_init, imsttfb_setup },
#endif
#ifdef CONFIG_FB_S3TRIO
	{ "s3trio", s3triofb_init, NULL },
#endif 
#ifdef CONFIG_FB_FM2
	{ "fm2fb", fm2fb_init, fm2fb_setup },
#endif 
#ifdef CONFIG_FB_SIS
	{ "sisfb", sisfb_init, sisfb_setup },
#endif
#ifdef CONFIG_FB_TRIDENT
	{ "trident", tridentfb_init, tridentfb_setup },
#endif
#ifdef CONFIG_FB_VOODOO1
	{ "sst", sstfb_init, sstfb_setup },
#endif

	/*
	 * Generic drivers that are used as fallbacks
	 * 
	 * These depend on resource management and must be initialized
	 * _after_ all other frame buffer devices that use resource
	 * management!
	 */

#ifdef CONFIG_FB_OF
	{ "offb", offb_init, NULL },
#endif
#ifdef CONFIG_FB_VESA
	{ "vesa", vesafb_init, vesafb_setup },
#endif 

	/*
	 * Chipset specific drivers that don't use resource management (yet)
	 */

#ifdef CONFIG_FB_3DFX
	{ "tdfx", tdfxfb_init, tdfxfb_setup },
#endif
#ifdef CONFIG_FB_SGIVW
	{ "sgivw", sgivwfb_init, sgivwfb_setup },
#endif
#ifdef CONFIG_FB_ACORN
	{ "acorn", acornfb_init, acornfb_setup },
#endif
#ifdef CONFIG_FB_ATARI
	{ "atafb", atafb_init, atafb_setup },
#endif
#ifdef CONFIG_FB_MAC
	{ "macfb", macfb_init, macfb_setup },
#endif
#ifdef CONFIG_FB_HGA
	{ "hga", hgafb_init, hgafb_setup },
#endif 
#ifdef CONFIG_FB_IGA
	{ "igafb", igafb_init, igafb_setup },
#endif
#ifdef CONFIG_APOLLO
	{ "apollo", dnfb_init, NULL },
#endif
#ifdef CONFIG_FB_Q40
	{ "q40fb", q40fb_init, NULL },
#endif
#ifdef CONFIG_FB_TGA
	{ "tga", tgafb_init, tgafb_setup },
#endif
#ifdef CONFIG_FB_HP300
	{ "hpfb", hpfb_init, NULL },
#endif 
#ifdef CONFIG_FB_G364
	{ "g364", g364fb_init, NULL },
#endif
#ifdef CONFIG_FB_SA1100
	{ "sa1100", sa1100fb_init, NULL },
#endif
#ifdef CONFIG_FB_SUN3
	{ "sun3", sun3fb_init, sun3fb_setup },
#endif
#ifdef CONFIG_FB_HIT
	{ "hitfb", hitfb_init, NULL },
#endif
#ifdef CONFIG_FB_TX3912
	{ "tx3912", tx3912fb_init, NULL },
#endif
#ifdef CONFIG_FB_E1355
	{ "e1355fb", e1355fb_init, e1355fb_setup },
#endif
#ifdef CONFIG_FB_E1356
        { "e1356fb", e1356fb_init, e1356fb_setup },
#endif
#ifdef CONFIG_FB_PVR2
	{ "pvr2", pvr2fb_init, pvr2fb_setup },
#endif
#ifdef CONFIG_FB_PMAG_AA
	{ "pmagaafb", pmagaafb_init, NULL },
#endif
#ifdef CONFIG_FB_PMAG_BA
	{ "pmagbafb", pmagbafb_init, NULL },
#endif
#ifdef CONFIG_FB_PMAGB_B
	{ "pmagbbfb", pmagbbfb_init, NULL },
#endif
#ifdef CONFIG_FB_MAXINE
	{ "maxinefb", maxinefb_init, NULL },
#endif
#ifdef CONFIG_FB_AU1100
	{ "au1100fb", au1100fb_init, au1100fb_setup },
#endif 
#ifdef CONFIG_FB_IT8181
	{ "it8181fb", it8181fb_init, it8181fb_setup },
#endif


	/*
	 * Generic drivers that don't use resource management (yet)
	 */

#ifdef CONFIG_FB_VGA16
	{ "vga16", vga16fb_init, vga16fb_setup },
#endif 
#ifdef CONFIG_FB_STI
	{ "stifb", stifb_init, stifb_setup },
#endif

#ifdef CONFIG_GSP_RESOLVER
	/* Not a real frame buffer device... */
	{ "resolver", NULL, resolver_video_setup },
#endif

#ifdef CONFIG_FB_VIRTUAL
	/*
	 * Vfb must be last to avoid that it becomes your primary display if
	 * other display devices are present
	 */
	{ "vfb", vfb_init, vfb_setup },
#endif
};

#define NUM_FB_DRIVERS	(sizeof(fb_drivers)/sizeof(*fb_drivers))

extern const char *global_mode_option;

static initcall_t pref_init_funcs[FB_MAX];
static int num_pref_init_funcs __initdata = 0;


struct fb_info *registered_fb[FB_MAX];
int num_registered_fb;
extern int fbcon_softback_size; 

static int first_fb_vc;
static int last_fb_vc = MAX_NR_CONSOLES-1;
static int fbcon_is_default = 1;

#ifdef CONFIG_FB_OF
static int ofonly __initdata = 0;
#endif

static int fbmem_read_proc(char *buf, char **start, off_t offset,
			   int len, int *eof, void *private)
{
	struct fb_info **fi;
	int clen;

	clen = 0;
	for (fi = registered_fb; fi < &registered_fb[FB_MAX] && len < 4000; fi++)
		if (*fi)
			clen += sprintf(buf + clen, "%d %s\n",
				        GET_FB_IDX((*fi)->node),
				        (*fi)->modename);
	*start = buf + offset;
	if (clen > offset)
		clen -= offset;
	else
		clen = 0;
	return clen < len ? clen : len;
}

static ssize_t
fb_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	unsigned long p = *ppos;
	struct inode *inode = file->f_dentry->d_inode;
	int fbidx = GET_FB_IDX(inode->i_rdev);
	struct fb_info *info = registered_fb[fbidx];
	struct fb_ops *fb = info->fbops;
	struct fb_fix_screeninfo fix;

	if (! fb || ! info->disp)
		return -ENODEV;

	fb->fb_get_fix(&fix,PROC_CONSOLE(info), info);
	if (p >= fix.smem_len)
	    return 0;
	if (count >= fix.smem_len)
	    count = fix.smem_len;
	if (count + p > fix.smem_len)
		count = fix.smem_len - p;
	if (count) {
	    char *base_addr;

	    base_addr = info->disp->screen_base;
	    count -= copy_to_user(buf, base_addr+p, count);
	    if (!count)
		return -EFAULT;
	    *ppos += count;
	}
	return count;
}

static ssize_t
fb_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	unsigned long p = *ppos;
	struct inode *inode = file->f_dentry->d_inode;
	int fbidx = GET_FB_IDX(inode->i_rdev);
	struct fb_info *info = registered_fb[fbidx];
	struct fb_ops *fb = info->fbops;
	struct fb_fix_screeninfo fix;
	int err;

	if (! fb || ! info->disp)
		return -ENODEV;

	fb->fb_get_fix(&fix, PROC_CONSOLE(info), info);
	if (p > fix.smem_len)
	    return -ENOSPC;
	if (count >= fix.smem_len)
	    count = fix.smem_len;
	err = 0;
	if (count + p > fix.smem_len) {
	    count = fix.smem_len - p;
	    err = -ENOSPC;
	}
	if (count) {
	    char *base_addr;

	    base_addr = info->disp->screen_base;
	    count -= copy_from_user(base_addr+p, buf, count);
	    *ppos += count;
	    err = -EFAULT;
	}
	if (count)
		return count;
	return err;
}

#ifdef CONFIG_KMOD
static void try_to_load(int fb)
{
	char modname[16];

	sprintf(modname, "fb%d", fb);
	request_module(modname);
}
#endif /* CONFIG_KMOD */

static int 
fb_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
	 unsigned long arg)
{
	int fbidx = GET_FB_IDX(inode->i_rdev);
	struct fb_info *info = registered_fb[fbidx];
	struct fb_ops *fb = info->fbops;
	struct fb_cmap cmap;
	struct fb_var_screeninfo var;
	struct fb_fix_screeninfo fix;
	struct fb_con2fbmap con2fb;
	int i;
	
	if (! fb)
		return -ENODEV;
	switch (cmd) {
	case FBIOGET_VSCREENINFO:
		if ((i = fb->fb_get_var(&var, PROC_CONSOLE(info), info)))
			return i;
		return copy_to_user((void *) arg, &var,
				    sizeof(var)) ? -EFAULT : 0;
	case FBIOPUT_VSCREENINFO:
		if (copy_from_user(&var, (void *) arg, sizeof(var)))
			return -EFAULT;
		i = var.activate & FB_ACTIVATE_ALL
			    ? set_all_vcs(fbidx, fb, &var, info)
			    : fb->fb_set_var(&var, PROC_CONSOLE(info), info);
		if (i)
			return i;
		if (copy_to_user((void *) arg, &var, sizeof(var)))
			return -EFAULT;
		return 0;
	case FBIOGET_FSCREENINFO:
		if ((i = fb->fb_get_fix(&fix, PROC_CONSOLE(info), info)))
			return i;
		return copy_to_user((void *) arg, &fix, sizeof(fix)) ?
			-EFAULT : 0;
	case FBIOPUTCMAP:
		if (copy_from_user(&cmap, (void *) arg, sizeof(cmap)))
			return -EFAULT;
		return (fb->fb_set_cmap(&cmap, 0, PROC_CONSOLE(info), info));
	case FBIOGETCMAP:
		if (copy_from_user(&cmap, (void *) arg, sizeof(cmap)))
			return -EFAULT;
		return (fb->fb_get_cmap(&cmap, 0, PROC_CONSOLE(info), info));
	case FBIOPAN_DISPLAY:
		if (copy_from_user(&var, (void *) arg, sizeof(var)))
			return -EFAULT;
		if (fb->fb_pan_display == NULL)
			return (var.xoffset || var.yoffset) ? -EINVAL : 0;
		if ((i=fb->fb_pan_display(&var, PROC_CONSOLE(info), info)))
			return i;
		if (copy_to_user((void *) arg, &var, sizeof(var)))
			return -EFAULT;
		return i;
	case FBIOGET_CON2FBMAP:
		if (copy_from_user(&con2fb, (void *)arg, sizeof(con2fb)))
			return -EFAULT;
		if (con2fb.console < 1 || con2fb.console > MAX_NR_CONSOLES)
		    return -EINVAL;
		con2fb.framebuffer = con2fb_map[con2fb.console-1];
		return copy_to_user((void *)arg, &con2fb,
				    sizeof(con2fb)) ? -EFAULT : 0;
	case FBIOPUT_CON2FBMAP:
		if (copy_from_user(&con2fb, (void *)arg, sizeof(con2fb)))
			return - EFAULT;
		if (con2fb.console < 0 || con2fb.console > MAX_NR_CONSOLES)
		    return -EINVAL;
		if (con2fb.framebuffer < 0 || con2fb.framebuffer >= FB_MAX)
		    return -EINVAL;
#ifdef CONFIG_KMOD
		if (!registered_fb[con2fb.framebuffer])
		    try_to_load(con2fb.framebuffer);
#endif /* CONFIG_KMOD */
		if (!registered_fb[con2fb.framebuffer])
		    return -EINVAL;
		if (con2fb.console != 0)
		    set_con2fb_map(con2fb.console-1, con2fb.framebuffer);
		else
		    /* set them all */
		    for (i = 0; i < MAX_NR_CONSOLES; i++)
			set_con2fb_map(i, con2fb.framebuffer);
		return 0;
	case FBIOBLANK:
		if (info->blank == 0)
			return -EINVAL;
		(*info->blank)(arg, info);
		return 0;
	default:
		if (fb->fb_ioctl == NULL)
			return -EINVAL;
		return fb->fb_ioctl(inode, file, cmd, arg, PROC_CONSOLE(info),
				    info);
	}
}

static int 
fb_mmap(struct file *file, struct vm_area_struct * vma)
{
	int fbidx = GET_FB_IDX(file->f_dentry->d_inode->i_rdev);
	struct fb_info *info = registered_fb[fbidx];
	struct fb_ops *fb = info->fbops;
	unsigned long off;
#if !defined(__sparc__) || defined(__sparc_v9__)
	struct fb_fix_screeninfo fix;
	struct fb_var_screeninfo var;
	unsigned long start;
	u32 len;
#endif

	if (vma->vm_pgoff > (~0UL >> PAGE_SHIFT))
		return -EINVAL;
	off = vma->vm_pgoff << PAGE_SHIFT;
	if (!fb)
		return -ENODEV;
	if (fb->fb_mmap) {
		int res;
		lock_kernel();
		res = fb->fb_mmap(info, file, vma);
		unlock_kernel();
		/* This is an IO map - tell maydump to skip this VMA */
		vma->vm_flags |= VM_IO;
		return res;
	}

#if defined(__sparc__) && !defined(__sparc_v9__)
	/* Should never get here, all fb drivers should have their own
	   mmap routines */
	return -EINVAL;
#else
	/* !sparc32... */

	lock_kernel();
	fb->fb_get_fix(&fix, PROC_CONSOLE(info), info);

	/* frame buffer memory */
	start = fix.smem_start;
	len = PAGE_ALIGN((start & ~PAGE_MASK)+fix.smem_len);
	if (off >= len) {
		/* memory mapped io */
		off -= len;
		fb->fb_get_var(&var, PROC_CONSOLE(info), info);
		if (var.accel_flags) {
			unlock_kernel();
			return -EINVAL;
		}
		start = fix.mmio_start;
		len = PAGE_ALIGN((start & ~PAGE_MASK)+fix.mmio_len);
	}
	unlock_kernel();
	start &= PAGE_MASK;
	if ((vma->vm_end - vma->vm_start + off) > len)
		return -EINVAL;
	off += start;
	vma->vm_pgoff = off >> PAGE_SHIFT;
	/* This is an IO map - tell maydump to skip this VMA */
	vma->vm_flags |= VM_IO;
#if defined(__sparc_v9__)
	vma->vm_flags |= (VM_SHM | VM_LOCKED);
	if (io_remap_page_range(vma->vm_start, off,
				vma->vm_end - vma->vm_start, vma->vm_page_prot, 0))
		return -EAGAIN;
#else
#if defined(__mc68000__)
#if defined(CONFIG_SUN3)
	pgprot_val(vma->vm_page_prot) |= SUN3_PAGE_NOCACHE;
#else
	if (CPU_IS_020_OR_030)
		pgprot_val(vma->vm_page_prot) |= _PAGE_NOCACHE030;
	if (CPU_IS_040_OR_060) {
		pgprot_val(vma->vm_page_prot) &= _CACHEMASK040;
		/* Use no-cache mode, serialized */
		pgprot_val(vma->vm_page_prot) |= _PAGE_NOCACHE_S;
	}
#endif
#elif defined(__powerpc__)
	pgprot_val(vma->vm_page_prot) |= _PAGE_NO_CACHE|_PAGE_GUARDED;
#elif defined(__alpha__)
	/* Caching is off in the I/O space quadrant by design.  */
#elif defined(__i386__) || defined(__x86_64__)
	if (boot_cpu_data.x86 > 3)
		pgprot_val(vma->vm_page_prot) |= _PAGE_PCD;
#elif defined(__arm__) || defined(__mips__)
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
#elif defined(__sh__)
	pgprot_val(vma->vm_page_prot) &= ~_PAGE_CACHABLE;
#elif defined(__hppa__)
	pgprot_val(vma->vm_page_prot) |= _PAGE_NO_CACHE; 
#elif defined(__ia64__)
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
#elif defined(__hppa__)
	pgprot_val(vma->vm_page_prot) |= _PAGE_NO_CACHE; 
#else
#warning What do we have to do here??
#endif
	if (io_remap_page_range(vma->vm_start, off,
			     vma->vm_end - vma->vm_start, vma->vm_page_prot))
		return -EAGAIN;
#endif /* !__sparc_v9__ */
	return 0;
#endif /* !sparc32 */
}

#if 1 /* to go away in 2.5.0 */
int GET_FB_IDX(kdev_t rdev)
{
    int fbidx = MINOR(rdev);
    if (fbidx >= 32) {
	int newfbidx = fbidx >> 5;
	static int warned;
	if (!(warned & (1<<newfbidx))) {
	    warned |= 1<<newfbidx;
	    printk("Warning: Remapping obsolete /dev/fb* minor %d to %d\n",
		   fbidx, newfbidx);
	}
	fbidx = newfbidx;
    }
    return fbidx;
}
#endif

static int
fb_open(struct inode *inode, struct file *file)
{
	int fbidx = GET_FB_IDX(inode->i_rdev);
	struct fb_info *info;
	int res = 0;

#ifdef CONFIG_KMOD
	if (!(info = registered_fb[fbidx]))
		try_to_load(fbidx);
#endif /* CONFIG_KMOD */
	if (!(info = registered_fb[fbidx]))
		return -ENODEV;
	if (info->fbops->owner)
		__MOD_INC_USE_COUNT(info->fbops->owner);
	if (info->fbops->fb_open) {
		res = info->fbops->fb_open(info,1);
		if (res && info->fbops->owner)
			__MOD_DEC_USE_COUNT(info->fbops->owner);
	}
	return res;
}

static int 
fb_release(struct inode *inode, struct file *file)
{
	int fbidx = GET_FB_IDX(inode->i_rdev);
	struct fb_info *info;

	lock_kernel();
	info = registered_fb[fbidx];
	if (info->fbops->fb_release)
		info->fbops->fb_release(info,1);
	if (info->fbops->owner)
		__MOD_DEC_USE_COUNT(info->fbops->owner);
	unlock_kernel();
	return 0;
}

static struct file_operations fb_fops = {
	owner:		THIS_MODULE,
	read:		fb_read,
	write:		fb_write,
	ioctl:		fb_ioctl,
	mmap:		fb_mmap,
	open:		fb_open,
	release:	fb_release,
#ifdef HAVE_ARCH_FB_UNMAPPED_AREA
	get_unmapped_area: get_fb_unmapped_area,
#endif
};

static devfs_handle_t devfs_handle;


/**
 *	register_framebuffer - registers a frame buffer device
 *	@fb_info: frame buffer info structure
 *
 *	Registers a frame buffer device @fb_info.
 *
 *	Returns negative errno on error, or zero for success.
 *
 */

int
register_framebuffer(struct fb_info *fb_info)
{
	int i, j;
	char name_buf[8];
	static int fb_ever_opened[FB_MAX];
	static int first = 1;

	if (num_registered_fb == FB_MAX)
		return -ENXIO;
	num_registered_fb++;
	for (i = 0 ; i < FB_MAX; i++)
		if (!registered_fb[i])
			break;
	fb_info->node = MKDEV(FB_MAJOR, i);
	registered_fb[i] = fb_info;
	if (!fb_ever_opened[i]) {
		struct module *owner = fb_info->fbops->owner;
		/*
		 *  We assume initial frame buffer devices can be opened this
		 *  many times
		 */
		for (j = 0; j < MAX_NR_CONSOLES; j++)
			if (con2fb_map[j] == i) {
				if (owner)
					__MOD_INC_USE_COUNT(owner);
				if (!fb_info->fbops->fb_open)
					continue;
				if (!fb_info->fbops->fb_open(fb_info,0))
					continue;
				if (owner)
					__MOD_DEC_USE_COUNT(owner);
			}
		fb_ever_opened[i] = 1;
	}

	if (first) {
		first = 0;
		take_over_console(&fb_con, first_fb_vc, last_fb_vc, fbcon_is_default);
	}
	sprintf (name_buf, "%d", i);
	fb_info->devfs_handle =
	    devfs_register (devfs_handle, name_buf, DEVFS_FL_DEFAULT,
			    FB_MAJOR, i, S_IFCHR | S_IRUGO | S_IWUGO,
			    &fb_fops, NULL);

	return 0;
}


/**
 *	unregister_framebuffer - releases a frame buffer device
 *	@fb_info: frame buffer info structure
 *
 *	Unregisters a frame buffer device @fb_info.
 *
 *	Returns negative errno on error, or zero for success.
 *
 */

int
unregister_framebuffer(struct fb_info *fb_info)
{
	int i, j;

	i = GET_FB_IDX(fb_info->node);
	for (j = 0; j < MAX_NR_CONSOLES; j++)
		if (con2fb_map[j] == i)
			return -EBUSY;
	if (!registered_fb[i])
		return -EINVAL;
	devfs_unregister (fb_info->devfs_handle);
	fb_info->devfs_handle = NULL;
	devfs_unregister (fb_info->devfs_lhandle);
	fb_info->devfs_lhandle = NULL;
	registered_fb[i]=NULL;
	num_registered_fb--;
	return 0;
}


/**
 *	fbmem_init - init frame buffer subsystem
 *
 *	Initialize the frame buffer subsystem.
 *
 *	NOTE: This function is _only_ to be called by drivers/char/mem.c.
 *
 */

void __init 
fbmem_init(void)
{
	int i;

	create_proc_read_entry("fb", 0, 0, fbmem_read_proc, NULL);

	devfs_handle = devfs_mk_dir (NULL, "fb", NULL);
	if (devfs_register_chrdev(FB_MAJOR,"fb",&fb_fops))
		printk("unable to get major %d for fb devs\n", FB_MAJOR);

#ifdef CONFIG_FB_OF
	if (ofonly) {
		offb_init();
		return;
	}
#endif

	/*
	 *  Probe for all builtin frame buffer devices
	 */
	for (i = 0; i < num_pref_init_funcs; i++)
		pref_init_funcs[i]();

	for (i = 0; i < NUM_FB_DRIVERS; i++)
		if (fb_drivers[i].init)
			fb_drivers[i].init();
}


/**
 *	video_setup - process command line options
 *	@options: string of options
 *
 *	Process command line options for frame buffer subsystem.
 *
 *	NOTE: This function is a __setup and __init function.
 *
 *	Returns zero.
 *
 */

int __init video_setup(char *options)
{
    int i, j;

    if (!options || !*options)
	    return 0;
	    
    if (!strncmp(options, "scrollback:", 11)) {
	    options += 11;
	    if (*options) {
		fbcon_softback_size = simple_strtoul(options, &options, 0);
		if (*options == 'k' || *options == 'K') {
			fbcon_softback_size *= 1024;
			options++;
		}
		if (*options != ',')
			return 0;
		options++;
	    } else
	        return 0;
    }

    if (!strncmp(options, "map:", 4)) {
	    options += 4;
	    if (*options)
		    for (i = 0, j = 0; i < MAX_NR_CONSOLES; i++) {
			    if (!options[j])
				    j = 0;
			    con2fb_map[i] = (options[j++]-'0') % FB_MAX;
		    }
	    return 0;
    }
    
    if (!strncmp(options, "vc:", 3)) {
	    options += 3;
	    if (*options)
		first_fb_vc = simple_strtoul(options, &options, 10) - 1;
	    if (first_fb_vc < 0)
		first_fb_vc = 0;
	    if (*options++ == '-')
		last_fb_vc = simple_strtoul(options, &options, 10) - 1;
	    fbcon_is_default = 0;
    }

#ifdef CONFIG_FB_OF
    if (!strcmp(options, "ofonly")) {
	    ofonly = 1;
	    return 0;
    }
#endif

    if (num_pref_init_funcs == FB_MAX)
	    return 0;

    for (i = 0; i < NUM_FB_DRIVERS; i++) {
	    j = strlen(fb_drivers[i].name);
	    if (!strncmp(options, fb_drivers[i].name, j) &&
		options[j] == ':') {
		    if (!strcmp(options+j+1, "off"))
			    fb_drivers[i].init = NULL;
		    else {
			    if (fb_drivers[i].init) {
				    pref_init_funcs[num_pref_init_funcs++] =
					    fb_drivers[i].init;
				    fb_drivers[i].init = NULL;
			    }
			    if (fb_drivers[i].setup)
				    fb_drivers[i].setup(options+j+1);
		    }
		    return 0;
	    }
    }

    /*
     * If we get here no fb was specified.
     * We consider the argument to be a global video mode option.
     */
    global_mode_option = options;
    return 0;
}

__setup("video=", video_setup);

    /*
     *  Visible symbols for modules
     */

EXPORT_SYMBOL(register_framebuffer);
EXPORT_SYMBOL(unregister_framebuffer);
EXPORT_SYMBOL(registered_fb);
EXPORT_SYMBOL(num_registered_fb);
#if 1 /* to go away in 2.5.0 */
EXPORT_SYMBOL(GET_FB_IDX);
#endif

MODULE_LICENSE("GPL");
