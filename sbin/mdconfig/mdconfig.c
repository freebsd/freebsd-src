/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.ORG> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $FreeBSD$
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <err.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/module.h>
#include <sys/linker.h>
#include <sys/mdioctl.h>

struct md_ioctl mdio;

enum {UNSET, ATTACH, DETACH} action = UNSET;

void mdmaybeload(void);

void
usage()
{
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "\tmdconfig -a -t type [-o [no]option]... [ -f file] [-s size] [-u unit]\n");
	fprintf(stderr, "\tmdconfig -d -u unit\n");
	fprintf(stderr, "\t\ttype = {malloc, preload, vnode, swap}\n");
	fprintf(stderr, "\t\toption = {cluster, compress, reserve, autounit}\n");
	fprintf(stderr, "\t\tsize = %%d (512 byte blocks), %%dk (kB), %%dm (MB) or %%dg (GB)\n");
	exit(1);
}

int
main(int argc, char **argv)
{
	int ch, fd, i;
	char *p;
	int cmdline = 0;

	for (;;) {
		ch = getopt(argc, argv, "ab:df:o:s:t:u:");
		if (ch == -1)
			break;
		switch (ch) {
		case 'a':
			if (cmdline != 0)
				usage();
			action = ATTACH;
			cmdline = 1;
			break;
		case 'd':
			if (cmdline != 0)
				usage();
			action = DETACH;
			mdio.md_options = MD_AUTOUNIT;
			cmdline = 3;
			break;
		case 't':
			if (cmdline != 1)
				usage();
			if (!strcmp(optarg, "malloc")) {
				mdio.md_type = MD_MALLOC;
				mdio.md_options = MD_AUTOUNIT | MD_COMPRESS;
			} else if (!strcmp(optarg, "preload")) {
				mdio.md_type = MD_PRELOAD;
				mdio.md_options = 0;
			} else if (!strcmp(optarg, "vnode")) {
				mdio.md_type = MD_VNODE;
				mdio.md_options = MD_CLUSTER | MD_AUTOUNIT | MD_COMPRESS;
			} else if (!strcmp(optarg, "swap")) {
				mdio.md_type = MD_SWAP;
				mdio.md_options = MD_CLUSTER | MD_AUTOUNIT | MD_COMPRESS;
			} else {
				usage();
			}
			cmdline=2;
			break;
		case 'f':
			if (cmdline != 2)
				usage();
			mdio.md_file = optarg;
			break;
		case 'o':
			if (cmdline != 2)
				usage();
			if (!strcmp(optarg, "cluster"))
				mdio.md_options |= MD_CLUSTER;
			else if (!strcmp(optarg, "nocluster"))
				mdio.md_options &= ~MD_CLUSTER;
			else if (!strcmp(optarg, "compress"))
				mdio.md_options |= MD_COMPRESS;
			else if (!strcmp(optarg, "nocompress"))
				mdio.md_options &= ~MD_COMPRESS;
			else if (!strcmp(optarg, "reserve"))
				mdio.md_options |= MD_RESERVE;
			else if (!strcmp(optarg, "noreserve"))
				mdio.md_options &= ~MD_RESERVE;
			else if (!strcmp(optarg, "autounit"))
				mdio.md_options |= MD_AUTOUNIT;
			else if (!strcmp(optarg, "noautounit"))
				mdio.md_options &= ~MD_AUTOUNIT;
			else
				errx(1, "Unknown option.");
			break;
		case 's':
			if (cmdline != 2)
				usage();
			mdio.md_size = strtoul(optarg, &p, 0);
			if (p == NULL || *p == '\0')
				;
			else if (*p == 'k' || *p == 'K')
				mdio.md_size *= (1024 / DEV_BSIZE);
			else if (*p == 'm' || *p == 'M')
				mdio.md_size *= (1024 * 1024 / DEV_BSIZE);
			else if (*p == 'g' || *p == 'G')
				mdio.md_size *= (1024 * 1024 * 1024 / DEV_BSIZE);
			else
				errx(1, "Unknown suffix on -s argument");
			break;
		case 'u':
			if (cmdline != 2 && cmdline != 3)
				usage();
			if (!strncmp(optarg, "/dev/", 5))
				optarg += 5;
			if (!strncmp(optarg, "md", 2))
				optarg += 2;
			mdio.md_unit = strtoul(optarg, NULL, 0);
			mdio.md_unit = strtoul(optarg, NULL, 0);
			mdio.md_options &= ~MD_AUTOUNIT;
			break;
		default:
			usage();
		}
	}

	mdmaybeload();
	fd = open("/dev/mdctl", O_RDWR, 0);
	if (fd < 0)
		err(1, "open(/dev/mdctl)");
	if (action == ATTACH) {
		i = ioctl(fd, MDIOCATTACH, &mdio);
	} else {
		if (mdio.md_options & MD_AUTOUNIT)
			usage();
		i = ioctl(fd, MDIOCDETACH, &mdio);
	}
	if (i < 0)
		err(1, "ioctl(/dev/mdctl)");
	if (mdio.md_options & MD_AUTOUNIT)
		printf("md%d\n", mdio.md_unit);
	return (0);
}

void
mdmaybeload(void)
{
        struct module_stat mstat;
        int fileid, modid;
        char *name = "md";
	char *cp;

        /* scan files in kernel */
        mstat.version = sizeof(struct module_stat);
        for (fileid = kldnext(0); fileid > 0; fileid = kldnext(fileid)) {
                /* scan modules in file */
                for (modid = kldfirstmod(fileid); modid > 0;
                     modid = modfnext(modid)) {
                        if (modstat(modid, &mstat) < 0)
                                continue;
                        /* strip bus name if present */
                        if ((cp = strchr(mstat.name, '/')) != NULL) {
                                cp++;
                        } else {
                                cp = mstat.name;
                        }
                        /* already loaded? */
                        if (!strcmp(name, cp))
                                return;
                }
        }
        /* not present, we should try to load it */
        kldload(name);
}

