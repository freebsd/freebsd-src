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
#include <sys/mdioctl.h>

struct md_ioctl mdio;

enum {UNSET, ATTACH, DETACH} action = UNSET;

int
main(int argc, char **argv)
{
	int ch, fd, i;

	mdio.md_options = MD_CLUSTER | MD_AUTOUNIT;

	for (;;) {
		ch = getopt(argc, argv, "adf:o:s:t:u:");
		if (ch == -1)
			break;
		switch (ch) {
		case 'a':
			action = ATTACH;
			break;
		case 'd':
			action = DETACH;
			break;
		case 'f':
			strncpy(mdio.md_file, optarg, sizeof(mdio.md_file) - 1);
			break;
		case 'o':
			if (!strcmp(optarg, "cluster"))
				mdio.md_options |= MD_CLUSTER;
			else if (!strcmp(optarg, "nocluster"))
				mdio.md_options &= ~MD_CLUSTER;
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
			mdio.md_size = strtoul(optarg, NULL, 0);
			break;
		case 't':
			if (!strcmp(optarg, "malloc"))
				mdio.md_type = MD_MALLOC;
			else if (!strcmp(optarg, "preload"))
				mdio.md_type = MD_PRELOAD;
			else if (!strcmp(optarg, "vnode"))
				mdio.md_type = MD_VNODE;
			else if (!strcmp(optarg, "swap"))
				mdio.md_type = MD_SWAP;
			else
				errx(1, "Unknown type.");
			break;
		case 'u':
			mdio.md_unit = strtoul(optarg, NULL, 0);
			mdio.md_options &= ~MD_AUTOUNIT;
			break;
		default:
			errx(1, "Usage: %s [-ad] [-f file] [-o option] [-s size] [-t type ] [-u unit].", argv[0]);
		}
	}

	fd = open("/dev/mdctl", O_RDWR, 0);
	if (fd < 0)
		err(1, "/dev/mdctl");
	if (action == ATTACH)
		i = ioctl(fd, MDIOCATTACH, &mdio);
	else if (action == DETACH)
		i = ioctl(fd, MDIOCDETACH, &mdio);
	else
		errx(1, "Neither -a(ttach) nor -d(etach) options present.");
	if (i < 0)
		err(1, "ioctl(/dev/mdctl)");
	return (0);
}

