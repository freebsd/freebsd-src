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
#include <sys/sysctl.h>
#include <sys/queue.h>

int	 list(const int);
void	 mdmaybeload(void);
int	 query(const int, const int);
void	 usage(void);

struct md_ioctl mdio;

enum {UNSET, ATTACH, DETACH, LIST} action = UNSET;

int nflag;

void
usage()
{
	fprintf(stderr, "usage:\n");
	fprintf(stderr, "\tmdconfig -a -t type [-n] [-o [no]option]... [ -f file] [-s size] [-S sectorsize] [-u unit]\n");
	fprintf(stderr, "\tmdconfig -d -u unit\n");
	fprintf(stderr, "\tmdconfig -l [-n] [-u unit]\n");
	fprintf(stderr, "\t\ttype = {malloc, preload, vnode, swap}\n");
	fprintf(stderr, "\t\toption = {cluster, compress, reserve}\n");
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
		ch = getopt(argc, argv, "ab:df:lno:s:S:t:u:x:y:");
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
		case 'l':
			if (cmdline != 0)
				usage();
			action = LIST;
			mdio.md_options = MD_AUTOUNIT;
			cmdline = 3;
			break;
		case 'n':
			nflag = 1;
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
			if (cmdline != 1 && cmdline != 2)
				usage();
			if (cmdline == 1) {
				/* Imply ``-t vnode'' */
				mdio.md_type = MD_VNODE;
				mdio.md_options = MD_CLUSTER | MD_AUTOUNIT | MD_COMPRESS;
				cmdline = 2;
			}
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
			else if (!strcmp(optarg, "force"))
				mdio.md_options |= MD_FORCE;
			else if (!strcmp(optarg, "noforce"))
				mdio.md_options &= ~MD_FORCE;
			else if (!strcmp(optarg, "reserve"))
				mdio.md_options |= MD_RESERVE;
			else if (!strcmp(optarg, "noreserve"))
				mdio.md_options &= ~MD_RESERVE;
			else
				errx(1, "Unknown option.");
			break;
		case 'S':
			if (cmdline != 2)
				usage();
			mdio.md_secsize = strtoul(optarg, &p, 0);
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
			if (!strncmp(optarg, MD_NAME, sizeof(MD_NAME) - 1))
				optarg += sizeof(MD_NAME) - 1;
			mdio.md_unit = strtoul(optarg, &p, 0);
			if ((unsigned)mdio.md_unit == ULONG_MAX || *p != '\0')
				errx(1, "bad unit: %s", optarg);
			mdio.md_options &= ~MD_AUTOUNIT;
			break;
		case 'x':
			if (cmdline != 2)
				usage();
			mdio.md_fwsectors = strtoul(optarg, &p, 0);
			break;
		case 'y':
			if (cmdline != 2)
				usage();
			mdio.md_fwheads = strtoul(optarg, &p, 0);
			break;
		default:
			usage();
		}
	}
	mdio.md_version = MDIOVERSION;

	mdmaybeload();
	fd = open("/dev/" MDCTL_NAME, O_RDWR, 0);
	if (fd < 0)
		err(1, "open(/dev/%s)", MDCTL_NAME);
	if (cmdline == 2
	    && (mdio.md_type == MD_MALLOC || mdio.md_type == MD_SWAP))
		if (mdio.md_size == 0)
			errx(1, "must specify -s for -t malloc or -t swap");
	if (cmdline == 2 && mdio.md_type == MD_VNODE)
		if (mdio.md_file == NULL)
			errx(1, "must specify -f for -t vnode");
	if (action == LIST) {
		if (mdio.md_options & MD_AUTOUNIT)
			list(fd);
		else
			query(fd, mdio.md_unit);
	} else if (action == ATTACH) {
		if (cmdline < 2)
			usage();
		i = ioctl(fd, MDIOCATTACH, &mdio);
		if (i < 0)
			err(1, "ioctl(/dev/%s)", MDCTL_NAME);
		if (mdio.md_options & MD_AUTOUNIT)
			printf("%s%d\n", nflag ? "" : MD_NAME, mdio.md_unit);
	} else if (action == DETACH) {
		if (mdio.md_options & MD_AUTOUNIT)
			usage();
		i = ioctl(fd, MDIOCDETACH, &mdio);
		if (i < 0)
			err(1, "ioctl(/dev/%s)", MDCTL_NAME);
	} else
		usage();
	close (fd);
	return (0);
}

struct dl {
	int		unit;
	SLIST_ENTRY(dl)	slist;
};

SLIST_HEAD(, dl) dlist = SLIST_HEAD_INITIALIZER(&dlist);

int
list(const int fd)
{
	int unit;

	if (ioctl(fd, MDIOCLIST, &mdio) < 0)
		err(1, "ioctl(/dev/%s)", MDCTL_NAME);
	for (unit = 0; unit < mdio.md_pad[0] && unit < MDNPAD - 1; unit++) {
		printf("%s%s%d", unit > 0 ? " " : "",
		    nflag ? "" : MD_NAME, mdio.md_pad[unit + 1]);
	}
	if (mdio.md_pad[0] - unit > 0)
		printf(" ... %d more", mdio.md_pad[0] - unit);
	printf("\n");
	return (0);
}

int
query(const int fd, const int unit)
{

	mdio.md_version = MDIOVERSION;
	mdio.md_unit = unit;

	if (ioctl(fd, MDIOCQUERY, &mdio) < 0)
		err(1, "ioctl(/dev/%s)", MDCTL_NAME);

	switch (mdio.md_type) {
	case MD_MALLOC:
		(void)printf("%s%d\tmalloc\t%d KBytes\n", MD_NAME,
		    mdio.md_unit, mdio.md_size / 2);
		break;
	case MD_PRELOAD:
		(void)printf("%s%d\tpreload\t%d KBytes\n", MD_NAME,
		    mdio.md_unit, mdio.md_size / 2);
		break;
	case MD_SWAP:
		(void)printf("%s%d\tswap\t%d KBytes\n", MD_NAME,
		    mdio.md_unit, mdio.md_size / 2);
		break;
	case MD_VNODE:
		(void)printf("%s%d\tvnode\t%d KBytes\n", MD_NAME,
		    mdio.md_unit, mdio.md_size / 2);
		break;
	}

	return (0);
}

void
mdmaybeload(void)
{
        struct module_stat mstat;
        int fileid, modid;
        const char *name;
	char *cp;

	name = MD_NAME;
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

