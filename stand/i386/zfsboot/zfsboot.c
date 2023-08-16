/*-
 * Copyright (c) 1998 Robert Nordier
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are freely
 * permitted provided that the above copyright notice and this
 * paragraph and the following disclaimer are duplicated in all
 * such forms.
 *
 * This software is provided "AS IS" and without any express or
 * implied warranties, including, without limitation, the implied
 * warranties of merchantability and fitness for a particular
 * purpose.
 */

#include <sys/cdefs.h>
#include <stand.h>

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/diskmbr.h>
#ifdef GPT
#include <sys/gpt.h>
#endif
#include <sys/reboot.h>
#include <sys/queue.h>
#ifdef LOADER_ZFS_SUPPORT
#include <sys/zfs_bootenv.h>
#endif

#include <machine/bootinfo.h>
#include <machine/elf.h>
#include <machine/pc/bios.h>

#include <stdarg.h>
#include <stddef.h>

#include <a.out.h>
#include "bootstrap.h"
#include "libi386.h"
#include <btxv86.h>

#include "lib.h"
#include "rbx.h"
#include "cons.h"
#include "bootargs.h"
#include "disk.h"
#include "part.h"
#include "paths.h"

#include "libzfs.h"

#define	ARGS			0x900
#define	NOPT			14
#define	NDEV			3

#define	BIOS_NUMDRIVES		0x475
#define	DRV_HARD		0x80
#define	DRV_MASK		0x7f

#define	TYPE_AD			0
#define	TYPE_DA			1
#define	TYPE_MAXHARD		TYPE_DA
#define	TYPE_FD			2

extern uint32_t _end;

static const char optstr[NOPT] = "DhaCcdgmnpqrsv"; /* Also 'P', 'S' */
static const unsigned char flags[NOPT] = {
    RBX_DUAL,
    RBX_SERIAL,
    RBX_ASKNAME,
    RBX_CDROM,
    RBX_CONFIG,
    RBX_KDB,
    RBX_GDB,
    RBX_MUTE,
    RBX_NOINTR,
    RBX_PAUSE,
    RBX_QUIET,
    RBX_DFLTROOT,
    RBX_SINGLE,
    RBX_VERBOSE
};
uint32_t opts;

/*
 * Paths to try loading before falling back to the boot2 prompt.
 *
 * /boot/zfsloader must be tried before /boot/loader in order to remain
 * backward compatible with ZFS boot environments where /boot/loader exists
 * but does not have ZFS support, which was the case before FreeBSD 12.
 *
 * If no loader is found, try to load a kernel directly instead.
 */
static const struct string {
	const char *p;
	size_t len;
} loadpath[] = {
	{ PATH_LOADER_ZFS, sizeof(PATH_LOADER_ZFS) },
	{ PATH_LOADER, sizeof(PATH_LOADER) },
	{ PATH_KERNEL, sizeof(PATH_KERNEL) },
};

static const unsigned char dev_maj[NDEV] = {30, 4, 2};

static struct i386_devdesc *bdev;
static char cmd[512];
static char cmddup[512];
static char kname[1024];
static int comspeed = SIOSPD;
static struct bootinfo bootinfo;
static uint32_t bootdev;
static struct zfs_boot_args zfsargs;
#ifdef LOADER_GELI_SUPPORT
static struct geli_boot_args geliargs;
#endif

extern vm_offset_t high_heap_base;
extern uint32_t	bios_basemem, bios_extmem, high_heap_size;

static char *heap_top;
static char *heap_bottom;

void exit(int);
static void i386_zfs_probe(void);
static void load(void);
static int parse_cmd(void);

#ifdef LOADER_GELI_SUPPORT
#include "geliboot.h"
static char gelipw[GELI_PW_MAXLEN];
#endif

struct arch_switch archsw;	/* MI/MD interface boundary */
static char boot_devname[2 * ZFS_MAXNAMELEN + 8]; /* disk or pool:dataset */

struct devsw *devsw[] = {
	&bioshd,
#if defined(LOADER_ZFS_SUPPORT)
	&zfs_dev,
#endif
	NULL
};

struct fs_ops *file_system[] = {
#if defined(LOADER_ZFS_SUPPORT)
	&zfs_fsops,
#endif
#if defined(LOADER_UFS_SUPPORT)
	&ufs_fsops,
#endif
	NULL
};

caddr_t
ptov(uintptr_t x)
{
	return (PTOV(x));
}

int main(void);

int
main(void)
{
	unsigned i;
	int auto_boot, fd, nextboot = 0;
	struct disk_devdesc *devdesc;

	bios_getmem();

	if (high_heap_size > 0) {
		heap_top = PTOV(high_heap_base + high_heap_size);
		heap_bottom = PTOV(high_heap_base);
	} else {
		heap_bottom = (char *)
		    (roundup2(__base + (int32_t)&_end, 0x10000) - __base);
		heap_top = (char *)PTOV(bios_basemem);
	}
	setheap(heap_bottom, heap_top);

	/*
	 * Initialise the block cache. Set the upper limit.
	 */
	bcache_init(32768, 512);

	archsw.arch_autoload = NULL;
	archsw.arch_getdev = i386_getdev;
	archsw.arch_copyin = NULL;
	archsw.arch_copyout = NULL;
	archsw.arch_readin = NULL;
	archsw.arch_isainb = NULL;
	archsw.arch_isaoutb = NULL;
	archsw.arch_zfs_probe = i386_zfs_probe;

	bootinfo.bi_version = BOOTINFO_VERSION;
	bootinfo.bi_size = sizeof(bootinfo);
	bootinfo.bi_basemem = bios_basemem / 1024;
	bootinfo.bi_extmem = bios_extmem / 1024;
	bootinfo.bi_memsizes_valid++;
	bootinfo.bi_bios_dev = *(uint8_t *)PTOV(ARGS);

	/* Set up fall back device name. */
	snprintf(boot_devname, sizeof (boot_devname), "disk%d:",
	    bd_bios2unit(bootinfo.bi_bios_dev));

	/* Set up currdev variable to have hooks in place. */
	env_setenv("currdev", EV_VOLATILE, "", gen_setcurrdev,
	    env_nounset);

	devinit();

	/* XXX assumes this will be a disk, but it looks likely give above */
	disk_parsedev((struct devdesc **)&devdesc, boot_devname, NULL);

	bootdev = MAKEBOOTDEV(dev_maj[DEVT_DISK], devdesc->d_slice + 1,
	    devdesc->dd.d_unit,
	    devdesc->d_partition >= 0 ? devdesc->d_partition : 0xff);
	free(devdesc);

	/*
	 * devformat() can be called only after dv_init
	 */
	if (bdev != NULL && bdev->dd.d_dev->dv_type == DEVT_ZFS) {
		/* set up proper device name string for ZFS */
		strncpy(boot_devname, devformat(&bdev->dd), sizeof (boot_devname));
		if (zfs_get_bootonce(bdev, OS_BOOTONCE, cmd,
		    sizeof(cmd)) == 0) {
			nvlist_t *benv;

			nextboot = 1;
			memcpy(cmddup, cmd, sizeof(cmd));
			if (parse_cmd()) {
				if (!OPT_CHECK(RBX_QUIET))
					printf("failed to parse bootonce "
					    "command\n");
				exit(0);
			}
			if (!OPT_CHECK(RBX_QUIET))
				printf("zfs bootonce: %s\n", cmddup);

			if (zfs_get_bootenv(bdev, &benv) == 0) {
				nvlist_add_string(benv, OS_BOOTONCE_USED,
				    cmddup);
				zfs_set_bootenv(bdev, benv);
			}
			/* Do not process this command twice */
			*cmd = 0;
		}
	}

	/* now make sure we have bdev on all cases */
	free(bdev);
	i386_getdev((void **)&bdev, boot_devname, NULL);

	env_setenv("currdev", EV_VOLATILE, boot_devname, gen_setcurrdev,
	    env_nounset);

	/* Process configuration file */
	auto_boot = 1;

	fd = open(PATH_CONFIG, O_RDONLY);
	if (fd == -1)
		fd = open(PATH_DOTCONFIG, O_RDONLY);

	if (fd != -1) {
		ssize_t cmdlen;

		if ((cmdlen = read(fd, cmd, sizeof(cmd))) > 0)
			cmd[cmdlen] = '\0';
		else
			*cmd = '\0';
		close(fd);
	}

	if (*cmd) {
		/*
		 * Note that parse_cmd() is destructive to cmd[] and we also
		 * want to honor RBX_QUIET option that could be present in
		 * cmd[].
		 */
		memcpy(cmddup, cmd, sizeof(cmd));
		if (parse_cmd())
			auto_boot = 0;
		if (!OPT_CHECK(RBX_QUIET))
			printf("%s: %s\n", PATH_CONFIG, cmddup);
		/* Do not process this command twice */
		*cmd = 0;
	}

	/* Do not risk waiting at the prompt forever. */
	if (nextboot && !auto_boot)
		exit(0);

	if (auto_boot && !*kname) {
		/*
		 * Iterate through the list of loader and kernel paths,
		 * trying to load. If interrupted by a keypress, or in case of
		 * failure, drop the user to the boot2 prompt.
		 */
		for (i = 0; i < nitems(loadpath); i++) {
			memcpy(kname, loadpath[i].p, loadpath[i].len);
			if (keyhit(3))
				break;
			load();
		}
	}

	/* Present the user with the boot2 prompt. */

	for (;;) {
		if (!auto_boot || !OPT_CHECK(RBX_QUIET)) {
			printf("\nFreeBSD/x86 boot\n");
			printf("Default: %s%s\nboot: ", boot_devname, kname);
		}
		if (ioctrl & IO_SERIAL)
			sio_flush();
		if (!auto_boot || keyhit(5))
			getstr(cmd, sizeof(cmd));
		else if (!auto_boot || !OPT_CHECK(RBX_QUIET))
			putchar('\n');
		auto_boot = 0;
		if (parse_cmd())
			putchar('\a');
		else
			load();
	}
}

/* XXX - Needed for btxld to link the boot2 binary; do not remove. */
void
exit(int x)
{
	__exit(x);
}

static void
load(void)
{
	union {
		struct exec ex;
		Elf32_Ehdr eh;
	} hdr;
	static Elf32_Phdr ep[2];
	static Elf32_Shdr es[2];
	caddr_t p;
	uint32_t addr, x;
	int fd, fmt, i, j;
	ssize_t size;

	if ((fd = open(kname, O_RDONLY)) == -1) {
		printf("\nCan't find %s\n", kname);
		return;
	}

	size = sizeof(hdr);
	if (read(fd, &hdr, sizeof (hdr)) != size) {
		close(fd);
		return;
	}
	if (N_GETMAGIC(hdr.ex) == ZMAGIC) {
		fmt = 0;
	} else if (IS_ELF(hdr.eh)) {
		fmt = 1;
	} else {
		printf("Invalid %s\n", "format");
		close(fd);
		return;
	}
	if (fmt == 0) {
		addr = hdr.ex.a_entry & 0xffffff;
		p = PTOV(addr);
		lseek(fd, PAGE_SIZE, SEEK_SET);
		size = hdr.ex.a_text;
		if (read(fd, p, hdr.ex.a_text) != size) {
			close(fd);
			return;
		}
		p += roundup2(hdr.ex.a_text, PAGE_SIZE);
		size = hdr.ex.a_data;
		if (read(fd, p, hdr.ex.a_data) != size) {
			close(fd);
			return;
		}
		p += hdr.ex.a_data + roundup2(hdr.ex.a_bss, PAGE_SIZE);
		bootinfo.bi_symtab = VTOP(p);
		memcpy(p, &hdr.ex.a_syms, sizeof(hdr.ex.a_syms));
		p += sizeof(hdr.ex.a_syms);
		if (hdr.ex.a_syms) {
			size = hdr.ex.a_syms;
			if (read(fd, p, hdr.ex.a_syms) != size) {
				close(fd);
				return;
			}
			p += hdr.ex.a_syms;
			size = sizeof (int);
			if (read(fd, p, sizeof (int)) != size) {
				close(fd);
				return;
			}
			x = *(uint32_t *)p;
			p += sizeof(int);
			x -= sizeof(int);
			size = x;
			if (read(fd, p, x) != size) {
				close(fd);
				return;
			}
			p += x;
		}
	} else {
		lseek(fd, hdr.eh.e_phoff, SEEK_SET);
		for (j = i = 0; i < hdr.eh.e_phnum && j < 2; i++) {
			size = sizeof (ep[0]);
			if (read(fd, ep + j, sizeof (ep[0])) != size) {
				close(fd);
				return;
			}
			if (ep[j].p_type == PT_LOAD)
				j++;
		}
		for (i = 0; i < 2; i++) {
			p = PTOV(ep[i].p_paddr & 0xffffff);
			lseek(fd, ep[i].p_offset, SEEK_SET);
			size = ep[i].p_filesz;
			if (read(fd, p, ep[i].p_filesz) != size) {
				close(fd);
				return;
			}
		}
		p += roundup2(ep[1].p_memsz, PAGE_SIZE);
		bootinfo.bi_symtab = VTOP(p);
		if (hdr.eh.e_shnum == hdr.eh.e_shstrndx + 3) {
			lseek(fd, hdr.eh.e_shoff +
			    sizeof (es[0]) * (hdr.eh.e_shstrndx + 1),
			    SEEK_SET);
			size = sizeof(es);
			if (read(fd, &es, sizeof (es)) != size) {
				close(fd);
				return;
			}
			for (i = 0; i < 2; i++) {
				memcpy(p, &es[i].sh_size,
				    sizeof(es[i].sh_size));
				p += sizeof(es[i].sh_size);
				lseek(fd, es[i].sh_offset, SEEK_SET);
				size = es[i].sh_size;
				if (read(fd, p, es[i].sh_size) != size) {
					close(fd);
					return;
				}
				p += es[i].sh_size;
			}
		}
		addr = hdr.eh.e_entry & 0xffffff;
	}
	close(fd);

	bootinfo.bi_esymtab = VTOP(p);
	bootinfo.bi_kernelname = VTOP(kname);
#ifdef LOADER_GELI_SUPPORT
	explicit_bzero(gelipw, sizeof(gelipw));
#endif

	if (bdev->dd.d_dev->dv_type == DEVT_ZFS) {
		zfsargs.size = sizeof(zfsargs);
		zfsargs.pool = bdev->zfs.pool_guid;
		zfsargs.root = bdev->zfs.root_guid;
#ifdef LOADER_GELI_SUPPORT
		export_geli_boot_data(&zfsargs.gelidata);
#endif
		/*
		 * Note that the zfsargs struct is passed by value, not by
		 * pointer. Code in btxldr.S copies the values from the entry
		 * stack to a fixed location within loader(8) at startup due
		 * to the presence of KARGS_FLAGS_EXTARG.
		 */
		__exec((caddr_t)addr, RB_BOOTINFO | (opts & RBX_MASK),
		    bootdev,
		    KARGS_FLAGS_ZFS | KARGS_FLAGS_EXTARG,
		    (uint32_t)bdev->zfs.pool_guid,
		    (uint32_t)(bdev->zfs.pool_guid >> 32),
		    VTOP(&bootinfo),
		    zfsargs);
	} else {
#ifdef LOADER_GELI_SUPPORT
		geliargs.size = sizeof(geliargs);
		export_geli_boot_data(&geliargs.gelidata);
#endif

		/*
		 * Note that the geliargs struct is passed by value, not by
		 * pointer. Code in btxldr.S copies the values from the entry
		 * stack to a fixed location within loader(8) at startup due
		 * to the presence of the KARGS_FLAGS_EXTARG flag.
		 */
		__exec((caddr_t)addr, RB_BOOTINFO | (opts & RBX_MASK),
		    bootdev,
#ifdef LOADER_GELI_SUPPORT
		    KARGS_FLAGS_GELI | KARGS_FLAGS_EXTARG, 0, 0,
		    VTOP(&bootinfo), geliargs
#else
		    0, 0, 0, VTOP(&bootinfo)
#endif
		    );
	}
}

static int
mount_root(char *arg)
{
	char *root;
	struct i386_devdesc *ddesc;
	uint8_t part;

	if (asprintf(&root, "%s:", arg) < 0)
		return (1);

	if (i386_getdev((void **)&ddesc, root, NULL)) {
		free(root);
		return (1);
	}

	/* we should have new device descriptor, free old and replace it. */
	free(bdev);
	bdev = ddesc;
	if (bdev->dd.d_dev->dv_type == DEVT_DISK) {
		if (bdev->disk.d_partition == -1)
			part = 0xff;
		else
			part = bdev->disk.d_partition;
		bootdev = MAKEBOOTDEV(dev_maj[bdev->dd.d_dev->dv_type],
		    bdev->disk.d_slice + 1, bdev->dd.d_unit, part);
		bootinfo.bi_bios_dev = bd_unit2bios(bdev);
	}
	strncpy(boot_devname, root, sizeof (boot_devname));
	setenv("currdev", root, 1);
	free(root);
	return (0);
}

static void
fs_list(char *arg)
{
	int fd;
	struct dirent *d;
	char line[80];

	fd = open(arg, O_RDONLY);
	if (fd < 0)
		return;
	pager_open();
	while ((d = readdirfd(fd)) != NULL) {
		sprintf(line, "%s\n", d->d_name);
		if (pager_output(line))
			break;
	}
	pager_close();
	close(fd);
}

static int
parse_cmd(void)
{
	char *arg = cmd;
	char *ep, *p, *q;
	const char *cp;
	char line[80];
	int c, i, j;

	while ((c = *arg++)) {
		if (c == ' ' || c == '\t' || c == '\n')
			continue;
		for (p = arg; *p && *p != '\n' && *p != ' ' && *p != '\t'; p++)
			;
		ep = p;
		if (*p)
			*p++ = 0;
		if (c == '-') {
			while ((c = *arg++)) {
				if (c == 'P') {
					if (*(uint8_t *)PTOV(0x496) & 0x10) {
						cp = "yes";
					} else {
						opts |= OPT_SET(RBX_DUAL);
						opts |= OPT_SET(RBX_SERIAL);
						cp = "no";
					}
					printf("Keyboard: %s\n", cp);
					continue;
				} else if (c == 'S') {
					j = 0;
					while ((unsigned int)
					    (i = *arg++ - '0') <= 9)
						j = j * 10 + i;
					if (j > 0 && i == -'0') {
						comspeed = j;
						break;
					}
					/*
					 * Fall through to error below
					 * ('S' not in optstr[]).
					 */
				}
				for (i = 0; c != optstr[i]; i++)
					if (i == NOPT - 1)
						return (-1);
				opts ^= OPT_SET(flags[i]);
			}
			ioctrl = OPT_CHECK(RBX_DUAL) ? (IO_SERIAL|IO_KEYBOARD) :
			    OPT_CHECK(RBX_SERIAL) ? IO_SERIAL : IO_KEYBOARD;
			if (ioctrl & IO_SERIAL) {
				if (sio_init(115200 / comspeed) != 0)
					ioctrl &= ~IO_SERIAL;
			}
		} if (c == '?') {
			printf("\n");
			if (*arg == '\0')
				arg = (char *)"/";
			fs_list(arg);
			zfs_list(arg);
			return (-1);
		} else {
			char *ptr;
			printf("\n");
			arg--;

			/*
			 * Report pool status if the comment is 'status'. Lets
			 * hope no-one wants to load /status as a kernel.
			 */
			if (strcmp(arg, "status") == 0) {
				pager_open();
				for (i = 0; devsw[i] != NULL; i++) {
					if (devsw[i]->dv_print != NULL) {
						if (devsw[i]->dv_print(1))
							break;
					} else {
						snprintf(line, sizeof(line),
						    "%s: (unknown)\n",
						    devsw[i]->dv_name);
						if (pager_output(line))
							break;
					}
				}
				pager_close();
				return (-1);
			}

			/*
			 * If there is "zfs:" prefix simply ignore it.
			 */
			ptr = arg;
			if (strncmp(ptr, "zfs:", 4) == 0)
				ptr += 4;

			/*
			 * If there is a colon, switch pools.
			 */
			q = strchr(ptr, ':');
			if (q) {
				*q++ = '\0';
				if (mount_root(arg) != 0) {
					return (-1);
				}
				arg = q;
			}
			if ((i = ep - arg)) {
				if ((size_t)i >= sizeof(kname))
					return (-1);
				memcpy(kname, arg, i + 1);
			}
		}
		arg = p;
	}
	return (0);
}

/*
 * Probe all disks to discover ZFS pools. The idea is to walk all possible
 * disk devices, however, we also need to identify possible boot pool.
 * For boot pool detection we have boot disk passed us from BIOS, recorded
 * in bootinfo.bi_bios_dev.
 */
static void
i386_zfs_probe(void)
{
	char devname[32];
	int boot_unit;
	struct i386_devdesc dev;
	uint64_t pool_guid = 0;

	dev.dd.d_dev = &bioshd;
	/* Translate bios dev to our unit number. */
	boot_unit = bd_bios2unit(bootinfo.bi_bios_dev);

	/*
	 * Open all the disks we can find and see if we can reconstruct
	 * ZFS pools from them.
	 */
	for (dev.dd.d_unit = 0; bd_unit2bios(&dev) >= 0; dev.dd.d_unit++) {
		snprintf(devname, sizeof (devname), "%s%d:", bioshd.dv_name,
		    dev.dd.d_unit);
		/* If this is not boot disk, use generic probe. */
		if (dev.dd.d_unit != boot_unit)
			zfs_probe_dev(devname, NULL, true);
		else
			zfs_probe_dev(devname, &pool_guid, true);

		if (pool_guid != 0 && bdev == NULL) {
			bdev = malloc(sizeof (struct i386_devdesc));
			bzero(bdev, sizeof (struct i386_devdesc));
			bdev->zfs.dd.d_dev = &zfs_dev;
			bdev->zfs.pool_guid = pool_guid;
		}
	}
}
