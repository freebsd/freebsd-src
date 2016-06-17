#define __KERNEL_SYSCALLS__
#include <linux/config.h>
#include <linux/slab.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/unistd.h>
#include <linux/ctype.h>
#include <linux/blk.h>
#include <linux/fd.h>
#include <linux/tty.h>
#include <linux/init.h>

#include <linux/nfs_fs.h>
#include <linux/nfs_fs_sb.h>
#include <linux/nfs_mount.h>
#include <linux/minix_fs.h>
#include <linux/ext2_fs.h>
#include <linux/romfs_fs.h>
#include <linux/cramfs_fs.h>

#define BUILD_CRAMDISK

extern int get_filesystem_list(char * buf);

extern asmlinkage long sys_mount(char *dev_name, char *dir_name, char *type,
	 unsigned long flags, void *data);
extern asmlinkage long sys_mkdir(const char *name, int mode);
extern asmlinkage long sys_chdir(const char *name);
extern asmlinkage long sys_fchdir(int fd);
extern asmlinkage long sys_chroot(const char *name);
extern asmlinkage long sys_unlink(const char *name);
extern asmlinkage long sys_symlink(const char *old, const char *new);
extern asmlinkage long sys_mknod(const char *name, int mode, dev_t dev);
extern asmlinkage long sys_umount(char *name, int flags);
extern asmlinkage long sys_ioctl(int fd, int cmd, unsigned long arg);

#ifdef CONFIG_BLK_DEV_INITRD
unsigned int real_root_dev;	/* do_proc_dointvec cannot handle kdev_t */
static int __initdata mount_initrd = 1;

static int __init no_initrd(char *str)
{
	mount_initrd = 0;
	return 1;
}

__setup("noinitrd", no_initrd);
#else
static int __initdata mount_initrd = 0;
#endif

int __initdata rd_doload;	/* 1 = load RAM disk, 0 = don't load */

int root_mountflags = MS_RDONLY | MS_VERBOSE;
static char root_device_name[64];

/* this is initialized in init/main.c */
kdev_t ROOT_DEV;

static int do_devfs = 0;

static int __init load_ramdisk(char *str)
{
	rd_doload = simple_strtol(str,NULL,0) & 3;
	return 1;
}
__setup("load_ramdisk=", load_ramdisk);

static int __init readonly(char *str)
{
	if (*str)
		return 0;
	root_mountflags |= MS_RDONLY;
	return 1;
}

static int __init readwrite(char *str)
{
	if (*str)
		return 0;
	root_mountflags &= ~MS_RDONLY;
	return 1;
}

__setup("ro", readonly);
__setup("rw", readwrite);

static struct dev_name_struct {
	const char *name;
	const int num;
} root_dev_names[] __initdata = {
	{ "nfs",     MKDEV(NFS_MAJOR, NFS_MINOR) },
	{ "hda",     0x0300 },
	{ "hdb",     0x0340 },
	{ "loop",    0x0700 },
	{ "hdc",     0x1600 },
	{ "hdd",     0x1640 },
	{ "hde",     0x2100 },
	{ "hdf",     0x2140 },
	{ "hdg",     0x2200 },
	{ "hdh",     0x2240 },
	{ "hdi",     0x3800 },
	{ "hdj",     0x3840 },
	{ "hdk",     0x3900 },
	{ "hdl",     0x3940 },
	{ "hdm",     0x5800 },
	{ "hdn",     0x5840 },
	{ "hdo",     0x5900 },
	{ "hdp",     0x5940 },
	{ "hdq",     0x5A00 },
	{ "hdr",     0x5A40 },
	{ "hds",     0x5B00 },
	{ "hdt",     0x5B40 },
	{ "sda",     0x0800 },
	{ "sdb",     0x0810 },
	{ "sdc",     0x0820 },
	{ "sdd",     0x0830 },
	{ "sde",     0x0840 },
	{ "sdf",     0x0850 },
	{ "sdg",     0x0860 },
	{ "sdh",     0x0870 },
	{ "sdi",     0x0880 },
	{ "sdj",     0x0890 },
	{ "sdk",     0x08a0 },
	{ "sdl",     0x08b0 },
	{ "sdm",     0x08c0 },
	{ "sdn",     0x08d0 },
	{ "sdo",     0x08e0 },
	{ "sdp",     0x08f0 },
	{ "ada",     0x1c00 },
	{ "adb",     0x1c10 },
	{ "adc",     0x1c20 },
	{ "add",     0x1c30 },
	{ "ade",     0x1c40 },
	{ "fd",      0x0200 },
	{ "md",      0x0900 },	     
	{ "xda",     0x0d00 },
	{ "xdb",     0x0d40 },
	{ "ram",     0x0100 },
	{ "scd",     0x0b00 },
	{ "mcd",     0x1700 },
	{ "cdu535",  0x1800 },
	{ "sonycd",  0x1800 },
	{ "aztcd",   0x1d00 },
	{ "cm206cd", 0x2000 },
	{ "gscd",    0x1000 },
	{ "sbpcd",   0x1900 },
	{ "eda",     0x2400 },
	{ "edb",     0x2440 },
	{ "pda",	0x2d00 },
	{ "pdb",	0x2d10 },
	{ "pdc",	0x2d20 },
	{ "pdd",	0x2d30 },
	{ "pcd",	0x2e00 },
	{ "pf",		0x2f00 },
	{ "apblock", APBLOCK_MAJOR << 8},
	{ "ddv", DDV_MAJOR << 8},
	{ "jsfd",    JSFD_MAJOR << 8},
#if defined(CONFIG_ARCH_S390)
	{ "dasda", (DASD_MAJOR << MINORBITS) },
	{ "dasdb", (DASD_MAJOR << MINORBITS) + (1 << 2) },
	{ "dasdc", (DASD_MAJOR << MINORBITS) + (2 << 2) },
	{ "dasdd", (DASD_MAJOR << MINORBITS) + (3 << 2) },
	{ "dasde", (DASD_MAJOR << MINORBITS) + (4 << 2) },
	{ "dasdf", (DASD_MAJOR << MINORBITS) + (5 << 2) },
	{ "dasdg", (DASD_MAJOR << MINORBITS) + (6 << 2) },
	{ "dasdh", (DASD_MAJOR << MINORBITS) + (7 << 2) },
#endif
	{ "ida/c0d0p",0x4800 },
	{ "ida/c0d1p",0x4810 },
	{ "ida/c0d2p",0x4820 },
	{ "ida/c0d3p",0x4830 },
	{ "ida/c0d4p",0x4840 },
	{ "ida/c0d5p",0x4850 },
	{ "ida/c0d6p",0x4860 },
	{ "ida/c0d7p",0x4870 },
	{ "ida/c0d8p",0x4880 },
	{ "ida/c0d9p",0x4890 },
	{ "ida/c0d10p",0x48A0 },
	{ "ida/c0d11p",0x48B0 },
	{ "ida/c0d12p",0x48C0 },
	{ "ida/c0d13p",0x48D0 },
	{ "ida/c0d14p",0x48E0 },
	{ "ida/c0d15p",0x48F0 },
	{ "ida/c1d0p",0x4900 },
	{ "ida/c2d0p",0x4A00 },
	{ "ida/c3d0p",0x4B00 },
	{ "ida/c4d0p",0x4C00 },
	{ "ida/c5d0p",0x4D00 },
	{ "ida/c6d0p",0x4E00 },
	{ "ida/c7d0p",0x4F00 }, 
	{ "cciss/c0d0p",0x6800 },
	{ "cciss/c0d1p",0x6810 },
	{ "cciss/c0d2p",0x6820 },
	{ "cciss/c0d3p",0x6830 },
	{ "cciss/c0d4p",0x6840 },
	{ "cciss/c0d5p",0x6850 },
	{ "cciss/c0d6p",0x6860 },
	{ "cciss/c0d7p",0x6870 },
	{ "cciss/c0d8p",0x6880 },
	{ "cciss/c0d9p",0x6890 },
	{ "cciss/c0d10p",0x68A0 },
	{ "cciss/c0d11p",0x68B0 },
	{ "cciss/c0d12p",0x68C0 },
	{ "cciss/c0d13p",0x68D0 },
	{ "cciss/c0d14p",0x68E0 },
	{ "cciss/c0d15p",0x68F0 },
	{ "cciss/c1d0p",0x6900 },
	{ "cciss/c2d0p",0x6A00 },
	{ "cciss/c3d0p",0x6B00 },
	{ "cciss/c4d0p",0x6C00 },
	{ "cciss/c5d0p",0x6D00 },
	{ "cciss/c6d0p",0x6E00 },
	{ "cciss/c7d0p",0x6F00 },
	{ "ataraid/d0p",0x7200 },
	{ "ataraid/d1p",0x7210 },
	{ "ataraid/d2p",0x7220 },
	{ "ataraid/d3p",0x7230 },
	{ "ataraid/d4p",0x7240 },
	{ "ataraid/d5p",0x7250 },
	{ "ataraid/d6p",0x7260 },
	{ "ataraid/d7p",0x7270 },
	{ "ataraid/d8p",0x7280 },
	{ "ataraid/d9p",0x7290 },
	{ "ataraid/d10p",0x72A0 },
	{ "ataraid/d11p",0x72B0 },
	{ "ataraid/d12p",0x72C0 },
	{ "ataraid/d13p",0x72D0 },
	{ "ataraid/d14p",0x72E0 },
	{ "ataraid/d15p",0x72F0 },
        { "rd/c0d0p",0x3000 },
        { "rd/c0d0p1",0x3001 },
        { "rd/c0d0p2",0x3002 },
        { "rd/c0d0p3",0x3003 },
        { "rd/c0d0p4",0x3004 },
        { "rd/c0d0p5",0x3005 },
        { "rd/c0d0p6",0x3006 },
        { "rd/c0d0p7",0x3007 },
        { "rd/c0d0p8",0x3008 },
        { "rd/c0d1p",0x3008 },
        { "rd/c0d1p1",0x3009 },
        { "rd/c0d1p2",0x300a },
        { "rd/c0d1p3",0x300b },
        { "rd/c0d1p4",0x300c },
        { "rd/c0d1p5",0x300d },
        { "rd/c0d1p6",0x300e },
        { "rd/c0d1p7",0x300f },
        { "rd/c0d1p8",0x3010 },
	{ "nftla", 0x5d00 },
	{ "nftlb", 0x5d10 },
	{ "nftlc", 0x5d20 },
	{ "nftld", 0x5d30 },
	{ "ftla", 0x2c00 },
	{ "ftlb", 0x2c08 },
	{ "ftlc", 0x2c10 },
	{ "ftld", 0x2c18 },
	{ "mtdblock", 0x1f00 },
	{ "nb", 0x2b00 },
	{ NULL, 0 }
};

kdev_t __init name_to_kdev_t(char *line)
{
	int base = 0, offs;
	char *end;

	if (strncmp(line,"/dev/",5) == 0) {
		struct dev_name_struct *dev = root_dev_names;
		line += 5;
		do {
			int len = strlen(dev->name);
			if (strncmp(line,dev->name,len) == 0) {
				line += len;
				base = dev->num;
				break;
			}
			dev++;
		} while (dev->name);
	}
	offs = simple_strtoul(line, &end, base?10:16);
	if (*end)
		offs = 0;
	return to_kdev_t(base + offs);
}

static int __init root_dev_setup(char *line)
{
	int i;
	char ch;

	ROOT_DEV = name_to_kdev_t(line);
	memset (root_device_name, 0, sizeof root_device_name);
	if (strncmp (line, "/dev/", 5) == 0) line += 5;
	for (i = 0; i < sizeof root_device_name - 1; ++i)
	{
	    ch = line[i];
	    if ( isspace (ch) || (ch == ',') || (ch == '\0') ) break;
	    root_device_name[i] = ch;
	}
	return 1;
}

__setup("root=", root_dev_setup);

static char * __initdata root_mount_data;
static int __init root_data_setup(char *str)
{
	root_mount_data = str;
	return 1;
}

static char * __initdata root_fs_names;
static int __init fs_names_setup(char *str)
{
	root_fs_names = str;
	return 1;
}

__setup("rootflags=", root_data_setup);
__setup("rootfstype=", fs_names_setup);

static void __init get_fs_names(char *page)
{
	char *s = page;

	if (root_fs_names) {
		strcpy(page, root_fs_names);
		while (*s++) {
			if (s[-1] == ',')
				s[-1] = '\0';
		}
	} else {
		int len = get_filesystem_list(page);
		char *p, *next;

		page[len] = '\0';
		for (p = page-1; p; p = next) {
			next = strchr(++p, '\n');
			if (*p++ != '\t')
				continue;
			while ((*s++ = *p++) != '\n')
				;
			s[-1] = '\0';
		}
	}
	*s = '\0';
}
static void __init mount_block_root(char *name, int flags)
{
	char *fs_names = __getname();
	char *p;

	get_fs_names(fs_names);
retry:
	for (p = fs_names; *p; p += strlen(p)+1) {
		int err = sys_mount(name, "/root", p, flags, root_mount_data);
		switch (err) {
			case 0:
				goto out;
			case -EACCES:
				flags |= MS_RDONLY;
				goto retry;
			case -EINVAL:
		        case -EBUSY:
				continue;
		}
	        /*
		 * Allow the user to distinguish between failed open
		 * and bad superblock on root device.
		 */
		printk ("VFS: Cannot open root device \"%s\" or %s\n",
			root_device_name, kdevname (ROOT_DEV));
		printk ("Please append a correct \"root=\" boot option\n");
		panic("VFS: Unable to mount root fs on %s",
			kdevname(ROOT_DEV));
	}
	panic("VFS: Unable to mount root fs on %s", kdevname(ROOT_DEV));
out:
	putname(fs_names);
	sys_chdir("/root");
	ROOT_DEV = current->fs->pwdmnt->mnt_sb->s_dev;
	printk("VFS: Mounted root (%s filesystem)%s.\n",
		current->fs->pwdmnt->mnt_sb->s_type->name,
		(current->fs->pwdmnt->mnt_sb->s_flags & MS_RDONLY) ? " readonly" : "");
}
 
#ifdef CONFIG_ROOT_NFS
static int __init mount_nfs_root(void)
{
	void *data = nfs_root_data();

	if (data && sys_mount("/dev/root","/root","nfs",root_mountflags,data) == 0)
		return 1;
	return 0;
}
#endif

static int __init create_dev(char *name, kdev_t dev, char *devfs_name)
{
	void *handle;
	char path[64];
	int n;

	sys_unlink(name);
	if (!do_devfs)
		return sys_mknod(name, S_IFBLK|0600, kdev_t_to_nr(dev));

	handle = devfs_find_handle(NULL, dev ? NULL : devfs_name,
				MAJOR(dev), MINOR(dev), DEVFS_SPECIAL_BLK, 1);
	if (!handle)
		return -1;
	n = devfs_generate_path(handle, path + 5, sizeof (path) - 5);
	if (n < 0)
		return -1;
	return sys_symlink(path + n + 5, name);
}

#if defined(CONFIG_BLK_DEV_RAM) || defined(CONFIG_BLK_DEV_FD)
static void __init change_floppy(char *fmt, ...)
{
	struct termios termios;
	char buf[80];
	char c;
	int fd;
	va_list args;
	va_start(args, fmt);
	vsprintf(buf, fmt, args);
	va_end(args);
	fd = open("/dev/root", O_RDWR | O_NDELAY, 0);
	if (fd >= 0) {
		sys_ioctl(fd, FDEJECT, 0);
		close(fd);
	}
	printk(KERN_NOTICE "VFS: Insert %s and press ENTER\n", buf);
	fd = open("/dev/console", O_RDWR, 0);
	if (fd >= 0) {
		sys_ioctl(fd, TCGETS, (long)&termios);
		termios.c_lflag &= ~ICANON;
		sys_ioctl(fd, TCSETSF, (long)&termios);
		read(fd, &c, 1);
		termios.c_lflag |= ICANON;
		sys_ioctl(fd, TCSETSF, (long)&termios);
		close(fd);
	}
}
#endif

#ifdef CONFIG_BLK_DEV_RAM

int __initdata rd_prompt = 1;	/* 1 = prompt for RAM disk, 0 = don't prompt */

static int __init prompt_ramdisk(char *str)
{
	rd_prompt = simple_strtol(str,NULL,0) & 1;
	return 1;
}
__setup("prompt_ramdisk=", prompt_ramdisk);

int __initdata rd_image_start;		/* starting block # of image */

static int __init ramdisk_start_setup(char *str)
{
	rd_image_start = simple_strtol(str,NULL,0);
	return 1;
}
__setup("ramdisk_start=", ramdisk_start_setup);

static int __init crd_load(int in_fd, int out_fd);

/*
 * This routine tries to find a RAM disk image to load, and returns the
 * number of blocks to read for a non-compressed image, 0 if the image
 * is a compressed image, and -1 if an image with the right magic
 * numbers could not be found.
 *
 * We currently check for the following magic numbers:
 * 	minix
 * 	ext2
 *	romfs
 *	cramfs
 * 	gzip
 */
static int __init 
identify_ramdisk_image(int fd, int start_block)
{
	const int size = 512;
	struct minix_super_block *minixsb;
	struct ext2_super_block *ext2sb;
	struct romfs_super_block *romfsb;
	struct cramfs_super *cramfsb;
	int nblocks = -1;
	unsigned char *buf;

	buf = kmalloc(size, GFP_KERNEL);
	if (buf == 0)
		return -1;

	minixsb = (struct minix_super_block *) buf;
	ext2sb = (struct ext2_super_block *) buf;
	romfsb = (struct romfs_super_block *) buf;
	cramfsb = (struct cramfs_super *) buf;
	memset(buf, 0xe5, size);

	/*
	 * Read block 0 to test for gzipped kernel
	 */
	lseek(fd, start_block * BLOCK_SIZE, 0);
	read(fd, buf, size);

	/*
	 * If it matches the gzip magic numbers, return -1
	 */
	if (buf[0] == 037 && ((buf[1] == 0213) || (buf[1] == 0236))) {
		printk(KERN_NOTICE
		       "RAMDISK: Compressed image found at block %d\n",
		       start_block);
		nblocks = 0;
		goto done;
	}

	/* romfs is at block zero too */
	if (romfsb->word0 == ROMSB_WORD0 &&
	    romfsb->word1 == ROMSB_WORD1) {
		printk(KERN_NOTICE
		       "RAMDISK: romfs filesystem found at block %d\n",
		       start_block);
		nblocks = (ntohl(romfsb->size)+BLOCK_SIZE-1)>>BLOCK_SIZE_BITS;
		goto done;
	}

	if (cramfsb->magic == CRAMFS_MAGIC) {
		printk(KERN_NOTICE
		       "RAMDISK: cramfs filesystem found at block %d\n",
		       start_block);
		nblocks = (cramfsb->size + BLOCK_SIZE - 1) >> BLOCK_SIZE_BITS;
		goto done;
	}

	/*
	 * Read block 1 to test for minix and ext2 superblock
	 */
	lseek(fd, (start_block+1) * BLOCK_SIZE, 0);
	read(fd, buf, size);

	/* Try minix */
	if (minixsb->s_magic == MINIX_SUPER_MAGIC ||
	    minixsb->s_magic == MINIX_SUPER_MAGIC2) {
		printk(KERN_NOTICE
		       "RAMDISK: Minix filesystem found at block %d\n",
		       start_block);
		nblocks = minixsb->s_nzones << minixsb->s_log_zone_size;
		goto done;
	}

	/* Try ext2 */
	if (ext2sb->s_magic == cpu_to_le16(EXT2_SUPER_MAGIC)) {
		printk(KERN_NOTICE
		       "RAMDISK: ext2 filesystem found at block %d\n",
		       start_block);
		nblocks = le32_to_cpu(ext2sb->s_blocks_count);
		goto done;
	}

	printk(KERN_NOTICE
	       "RAMDISK: Couldn't find valid RAM disk image starting at %d.\n",
	       start_block);
	
done:
	lseek(fd, start_block * BLOCK_SIZE, 0);
	kfree(buf);
	return nblocks;
}
#endif

static int __init rd_load_image(char *from)
{
	int res = 0;

#ifdef CONFIG_BLK_DEV_RAM
	int in_fd, out_fd;
	unsigned long rd_blocks, devblocks;
	int nblocks, i;
	char *buf;
	unsigned short rotate = 0;
#if !defined(CONFIG_ARCH_S390) && !defined(CONFIG_PPC_ISERIES)
	char rotator[4] = { '|' , '/' , '-' , '\\' };
#endif

	out_fd = open("/dev/ram", O_RDWR, 0);
	if (out_fd < 0)
		goto out;

	in_fd = open(from, O_RDONLY, 0);
	if (in_fd < 0)
		goto noclose_input;

	nblocks = identify_ramdisk_image(in_fd, rd_image_start);
	if (nblocks < 0)
		goto done;

	if (nblocks == 0) {
#ifdef BUILD_CRAMDISK
		if (crd_load(in_fd, out_fd) == 0)
			goto successful_load;
#else
		printk(KERN_NOTICE
		       "RAMDISK: Kernel does not support compressed "
		       "RAM disk images\n");
#endif
		goto done;
	}

	/*
	 * NOTE NOTE: nblocks suppose that the blocksize is BLOCK_SIZE, so
	 * rd_load_image will work only with filesystem BLOCK_SIZE wide!
	 * So make sure to use 1k blocksize while generating ext2fs
	 * ramdisk-images.
	 */
	if (sys_ioctl(out_fd, BLKGETSIZE, (unsigned long)&rd_blocks) < 0)
		rd_blocks = 0;
	else
		rd_blocks >>= 1;

	if (nblocks > rd_blocks) {
		printk("RAMDISK: image too big! (%d/%lu blocks)\n",
		       nblocks, rd_blocks);
		goto done;
	}
		
	/*
	 * OK, time to copy in the data
	 */
	buf = kmalloc(BLOCK_SIZE, GFP_KERNEL);
	if (buf == 0) {
		printk(KERN_ERR "RAMDISK: could not allocate buffer\n");
		goto done;
	}

	if (sys_ioctl(in_fd, BLKGETSIZE, (unsigned long)&devblocks) < 0)
		devblocks = 0;
	else
		devblocks >>= 1;

	if (strcmp(from, "/dev/initrd") == 0)
		devblocks = nblocks;

	if (devblocks == 0) {
		printk(KERN_ERR "RAMDISK: could not determine device size\n");
		goto done;
	}

	printk(KERN_NOTICE "RAMDISK: Loading %d blocks [%ld disk%s] into ram disk... ", 
		nblocks, ((nblocks-1)/devblocks)+1, nblocks>devblocks ? "s" : "");
	for (i=0; i < nblocks; i++) {
		if (i && (i % devblocks == 0)) {
			printk("done disk #%ld.\n", i/devblocks);
			rotate = 0;
			if (close(in_fd)) {
				printk("Error closing the disk.\n");
				goto noclose_input;
			}
			change_floppy("disk #%d", i/devblocks+1);
			in_fd = open(from, O_RDONLY, 0);
			if (in_fd < 0)  {
				printk("Error opening disk.\n");
				goto noclose_input;
			}
			printk("Loading disk #%ld... ", i/devblocks+1);
		}
		read(in_fd, buf, BLOCK_SIZE);
		write(out_fd, buf, BLOCK_SIZE);
#if !defined(CONFIG_ARCH_S390) && !defined(CONFIG_PPC_ISERIES)
		if (!(i % 16)) {
			printk("%c\b", rotator[rotate & 0x3]);
			rotate++;
		}
#endif
	}
	printk("done.\n");
	kfree(buf);

successful_load:
	res = 1;
done:
	close(in_fd);
noclose_input:
	close(out_fd);
out:
	sys_unlink("/dev/ram");
#endif
	return res;
}

static int __init rd_load_disk(int n)
{
#ifdef CONFIG_BLK_DEV_RAM
	if (rd_prompt)
		change_floppy("root floppy disk to be loaded into RAM disk");
	create_dev("/dev/ram", MKDEV(RAMDISK_MAJOR, n), NULL);
#endif
	return rd_load_image("/dev/root");
}

#ifdef CONFIG_DEVFS_FS

static void __init convert_name(char *prefix, char *name, char *p, int part)
{
	int host, bus, target, lun;
	char dest[64];
	char src[64];
	char *base = p - 1;

	/*  Decode "c#b#t#u#"  */
	if (*p++ != 'c')
		return;
	host = simple_strtol(p, &p, 10);
	if (*p++ != 'b')
		return;
	bus = simple_strtol(p, &p, 10);
	if (*p++ != 't')
		return;
	target = simple_strtol(p, &p, 10);
	if (*p++ != 'u')
		return;
	lun = simple_strtol(p, &p, 10);
	if (!part)
		sprintf(dest, "%s/host%d/bus%d/target%d/lun%d",
				prefix, host, bus, target, lun);
	else if (*p++ == 'p')
		sprintf(dest, "%s/host%d/bus%d/target%d/lun%d/part%s",
				prefix, host, bus, target, lun, p);
	else
		sprintf(dest, "%s/host%d/bus%d/target%d/lun%d/disc",
				prefix, host, bus, target, lun);
	*base = '\0';
	sprintf(src, "/dev/%s", name);
	sys_mkdir(src, 0755);
	*base = '/';
	sprintf(src, "/dev/%s", name);
	sys_symlink(dest, src);
}

static void __init devfs_make_root(char *name)
{

	if (!strncmp(name, "sd/", 3))
		convert_name("../scsi", name, name+3, 1);
	else if (!strncmp(name, "sr/", 3))
		convert_name("../scsi", name, name+3, 0);
	else if (!strncmp(name, "ide/hd/", 7))
		convert_name("..", name, name + 7, 1);
	else if (!strncmp(name, "ide/cd/", 7))
		convert_name("..", name, name + 7, 0);
}
#else
static void __init devfs_make_root(char *name)
{
}
#endif

static void __init mount_root(void)
{
#ifdef CONFIG_ROOT_NFS
       if (MAJOR(ROOT_DEV) == NFS_MAJOR
           && MINOR(ROOT_DEV) == NFS_MINOR) {
		if (mount_nfs_root()) {
			sys_chdir("/root");
			ROOT_DEV = current->fs->pwdmnt->mnt_sb->s_dev;
			printk("VFS: Mounted root (nfs filesystem).\n");
			return;
		}
		printk(KERN_ERR "VFS: Unable to mount root fs via NFS, trying floppy.\n");
		ROOT_DEV = MKDEV(FLOPPY_MAJOR, 0);
	}
#endif
	devfs_make_root(root_device_name);
	create_dev("/dev/root", ROOT_DEV, root_device_name);
#ifdef CONFIG_BLK_DEV_FD
	if (MAJOR(ROOT_DEV) == FLOPPY_MAJOR) {
		/* rd_doload is 2 for a dual initrd/ramload setup */
		if (rd_doload==2) {
			if (rd_load_disk(1)) {
				ROOT_DEV = MKDEV(RAMDISK_MAJOR, 1);
				create_dev("/dev/root", ROOT_DEV, NULL);
			}
		} else
			change_floppy("root floppy");
	}
#endif
	mount_block_root("/dev/root", root_mountflags);
}

#ifdef CONFIG_BLK_DEV_INITRD
static int old_fd, root_fd;
static int do_linuxrc(void * shell)
{
	static char *argv[] = { "linuxrc", NULL, };
	extern char * envp_init[];

	close(old_fd);
	close(root_fd);
	close(0);
	close(1);
	close(2);
	setsid();
	(void) open("/dev/console",O_RDWR,0);
	(void) dup(0);
	(void) dup(0);
	return execve(shell, argv, envp_init);
}

#endif

static void __init handle_initrd(void)
{
#ifdef CONFIG_BLK_DEV_INITRD
	int ram0 = kdev_t_to_nr(MKDEV(RAMDISK_MAJOR,0));
	int error;
	int i, pid;

	create_dev("/dev/root.old", ram0, NULL);
	/* mount initrd on rootfs' /root */
	mount_block_root("/dev/root.old", root_mountflags & ~MS_RDONLY);
	sys_mkdir("/old", 0700);
	root_fd = open("/", 0, 0);
	old_fd = open("/old", 0, 0);
	/* move initrd over / and chdir/chroot in initrd root */
	sys_chdir("/root");
	sys_mount(".", "/", NULL, MS_MOVE, NULL);
	sys_chroot(".");
	mount_devfs_fs ();

	pid = kernel_thread(do_linuxrc, "/linuxrc", SIGCHLD);
	if (pid > 0) {
		while (pid != wait(&i))
			yield();
	}

	/* move initrd to rootfs' /old */
	sys_fchdir(old_fd);
	sys_mount("/", ".", NULL, MS_MOVE, NULL);
	/* switch root and cwd back to / of rootfs */
	sys_fchdir(root_fd);
	sys_chroot(".");
	sys_umount("/old/dev", 0);
	close(old_fd);
	close(root_fd);

	if (real_root_dev == ram0) {
		sys_chdir("/old");
		return;
	}

	ROOT_DEV = real_root_dev;
	mount_root();

	printk(KERN_NOTICE "Trying to move old root to /initrd ... ");
	error = sys_mount("/old", "/root/initrd", NULL, MS_MOVE, NULL);
	if (!error)
		printk("okay\n");
	else {
		int fd = open("/dev/root.old", O_RDWR, 0);
		printk("failed\n");
		printk(KERN_NOTICE "Unmounting old root\n");
		sys_umount("/old", MNT_DETACH);
		printk(KERN_NOTICE "Trying to free ramdisk memory ... ");
		if (fd < 0) {
			error = fd;
		} else {
			error = sys_ioctl(fd, BLKFLSBUF, 0);
			close(fd);
		}
		printk(!error ? "okay\n" : "failed\n");
	}
#endif
}

static int __init initrd_load(void)
{
#ifdef CONFIG_BLK_DEV_INITRD
	create_dev("/dev/ram", MKDEV(RAMDISK_MAJOR, 0), NULL);
	create_dev("/dev/initrd", MKDEV(RAMDISK_MAJOR, INITRD_MINOR), NULL);
#endif
	return rd_load_image("/dev/initrd");
}

/*
 * Prepare the namespace - decide what/where to mount, load ramdisks, etc.
 */
void prepare_namespace(void)
{
	int is_floppy = MAJOR(ROOT_DEV) == FLOPPY_MAJOR;
#ifdef CONFIG_ALL_PPC
	extern void arch_discover_root(void);
	arch_discover_root();
#endif /* CONFIG_ALL_PPC */
#ifdef CONFIG_BLK_DEV_INITRD
	if (!initrd_start)
		mount_initrd = 0;
	real_root_dev = ROOT_DEV;
#endif
	sys_mkdir("/dev", 0700);
	sys_mkdir("/root", 0700);
	sys_mknod("/dev/console", S_IFCHR|0600, MKDEV(TTYAUX_MAJOR, 1));
#ifdef CONFIG_DEVFS_FS
	sys_mount("devfs", "/dev", "devfs", 0, NULL);
	do_devfs = 1;
#endif

	create_dev("/dev/root", ROOT_DEV, NULL);
	if (mount_initrd) {
		if (initrd_load() && ROOT_DEV != MKDEV(RAMDISK_MAJOR, 0)) {
			handle_initrd();
			goto out;
		}
	} else if (is_floppy && rd_doload && rd_load_disk(0))
		ROOT_DEV = MKDEV(RAMDISK_MAJOR, 0);
	mount_root();
out:
	sys_umount("/dev", 0);
	sys_mount(".", "/", NULL, MS_MOVE, NULL);
	sys_chroot(".");
	mount_devfs_fs ();
}

#ifdef CONFIG_BLK_DEV_RAM

#if defined(BUILD_CRAMDISK) && defined(CONFIG_BLK_DEV_RAM)

/*
 * gzip declarations
 */

#define OF(args)  args

#ifndef memzero
#define memzero(s, n)     memset ((s), 0, (n))
#endif

typedef unsigned char  uch;
typedef unsigned short ush;
typedef unsigned long  ulg;

#define INBUFSIZ 4096
#define WSIZE 0x8000    /* window size--must be a power of two, and */
			/*  at least 32K for zip's deflate method */

static uch *inbuf;
static uch *window;

static unsigned insize;  /* valid bytes in inbuf */
static unsigned inptr;   /* index of next byte to be processed in inbuf */
static unsigned outcnt;  /* bytes in output buffer */
static int exit_code;
static long bytes_out;
static int crd_infd, crd_outfd;

#define get_byte()  (inptr < insize ? inbuf[inptr++] : fill_inbuf())
		
/* Diagnostic functions (stubbed out) */
#define Assert(cond,msg)
#define Trace(x)
#define Tracev(x)
#define Tracevv(x)
#define Tracec(c,x)
#define Tracecv(c,x)

#define STATIC static

static int  fill_inbuf(void);
static void flush_window(void);
static void *malloc(int size);
static void free(void *where);
static void error(char *m);
static void gzip_mark(void **);
static void gzip_release(void **);

#include "../lib/inflate.c"

static void __init *malloc(int size)
{
	return kmalloc(size, GFP_KERNEL);
}

static void __init free(void *where)
{
	kfree(where);
}

static void __init gzip_mark(void **ptr)
{
}

static void __init gzip_release(void **ptr)
{
}


/* ===========================================================================
 * Fill the input buffer. This is called only when the buffer is empty
 * and at least one byte is really needed.
 */
static int __init fill_inbuf(void)
{
	if (exit_code) return -1;
	
	insize = read(crd_infd, inbuf, INBUFSIZ);
	if (insize == 0) return -1;

	inptr = 1;

	return inbuf[0];
}

/* ===========================================================================
 * Write the output window window[0..outcnt-1] and update crc and bytes_out.
 * (Used for the decompressed data only.)
 */
static void __init flush_window(void)
{
    ulg c = crc;         /* temporary variable */
    unsigned n;
    uch *in, ch;
    
    write(crd_outfd, window, outcnt);
    in = window;
    for (n = 0; n < outcnt; n++) {
	    ch = *in++;
	    c = crc_32_tab[((int)c ^ ch) & 0xff] ^ (c >> 8);
    }
    crc = c;
    bytes_out += (ulg)outcnt;
    outcnt = 0;
}

static void __init error(char *x)
{
	printk(KERN_ERR "%s", x);
	exit_code = 1;
}

static int __init crd_load(int in_fd, int out_fd)
{
	int result;

	insize = 0;		/* valid bytes in inbuf */
	inptr = 0;		/* index of next byte to be processed in inbuf */
	outcnt = 0;		/* bytes in output buffer */
	exit_code = 0;
	bytes_out = 0;
	crc = (ulg)0xffffffffL; /* shift register contents */

	crd_infd = in_fd;
	crd_outfd = out_fd;
	inbuf = kmalloc(INBUFSIZ, GFP_KERNEL);
	if (inbuf == 0) {
		printk(KERN_ERR "RAMDISK: Couldn't allocate gzip buffer\n");
		return -1;
	}
	window = kmalloc(WSIZE, GFP_KERNEL);
	if (window == 0) {
		printk(KERN_ERR "RAMDISK: Couldn't allocate gzip window\n");
		kfree(inbuf);
		return -1;
	}
	makecrc();
	result = gunzip();
	kfree(inbuf);
	kfree(window);
	return result;
}

#endif  /* BUILD_CRAMDISK && CONFIG_BLK_DEV_RAM */
#endif  /* CONFIG_BLK_DEV_RAM */
