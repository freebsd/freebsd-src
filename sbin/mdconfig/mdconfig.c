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
int	 query(const int, const int);

struct md_ioctl mdio;

enum {UNSET, ATTACH, DETACH, LIST} action = UNSET;

void mdmaybeload(void);

void
usage()
{
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "\tmdconfig -a -t type [-o [no]option]... [ -f file] [-s size] [-u unit]\n");
	fprintf(stderr, "\tmdconfig -d -u unit\n");
	fprintf(stderr, "\tmdconfig -l [-u unit]\n");
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
		ch = getopt(argc, argv, "ab:df:lo:s:t:u:");
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
			else if (!strcmp(optarg, "reserve"))
				mdio.md_options |= MD_RESERVE;
			else if (!strcmp(optarg, "noreserve"))
				mdio.md_options &= ~MD_RESERVE;
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
			if (!strncmp(optarg, MD_NAME, sizeof(MD_NAME) - 1))
				optarg += 2;
			mdio.md_unit = strtoul(optarg, &p, 0);
			if ((unsigned)mdio.md_unit == ULONG_MAX || *p != '\0')
				errx(1, "bad unit: %s", optarg);
			mdio.md_options &= ~MD_AUTOUNIT;
			break;
		default:
			usage();
		}
	}

	mdmaybeload();
	fd = open("/dev/" MDCTL_NAME, O_RDWR, 0);
	if (fd < 0)
		err(1, "open(/dev/%s)", MDCTL_NAME);
	if (cmdline == 2
	    && (mdio.md_type == MD_MALLOC || mdio.md_type == MD_SWAP))
		if (mdio.md_size == 0)
			errx(1, "must specify -s for -t malloc or -t swap");
	if (action == LIST) {
		if (mdio.md_options & MD_AUTOUNIT)
			list(fd);
		else
			query(fd, mdio.md_unit);
	} else if (action == ATTACH) {
		i = ioctl(fd, MDIOCATTACH, &mdio);
		if (i < 0)
			err(1, "ioctl(/dev/%s)", MDCTL_NAME);
		if (mdio.md_options & MD_AUTOUNIT)
			printf("%s%d\n", MD_NAME, mdio.md_unit);
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
	char *disklist, *p, *p2, *p3;
	int unit, dll;
	struct dl *dp, *di, *dn;

	if (sysctlbyname("kern.disks", NULL, &dll, NULL, NULL) == -1)
		err(1, "sysctlbyname: kern.disks");
	if ( (disklist = malloc(dll)) == NULL)
		err(1, "malloc");
	if (sysctlbyname("kern.disks", disklist, &dll, NULL, NULL) == -1)
		err(1, "sysctlbyname: kern.disks");

	for (p = disklist;
	     (p2 = strsep(&p, " ")) != NULL;) {
		if (strncmp(p2, MD_NAME, sizeof(MD_NAME) - 1) != 0)
			continue;
		p2 += 2;
		unit = strtoul(p2, &p3, 10);
		if (p2 == p3)
			continue;
		dp = calloc(sizeof *dp, 1);
		dp->unit = unit;
		dn = SLIST_FIRST(&dlist);
		if (dn == NULL || dn->unit > unit) {
			SLIST_INSERT_HEAD(&dlist, dp, slist);
		} else {
			SLIST_FOREACH(di, &dlist, slist) {
				dn = SLIST_NEXT(di, slist);
				if (dn == NULL || dn->unit > unit) {
					SLIST_INSERT_AFTER(di, dp, slist);
					break;
				} 
			} 
		}
	}
	SLIST_FOREACH(di, &dlist, slist) 
		query(fd, di->unit);
	while (!SLIST_EMPTY(&dlist)) {
		di = SLIST_FIRST(&dlist);
		SLIST_REMOVE_HEAD(&dlist, slist);
		free(di);
	}
	free(disklist);
	return (0);
}

int
query(const int fd, const int unit)
{

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

