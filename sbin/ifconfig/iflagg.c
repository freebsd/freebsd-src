/*-
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sockio.h>

#include <stdlib.h>
#include <unistd.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_lagg.h>
#include <net/route.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>

#include "ifconfig.h"

static void
setlaggport(const char *val, int d, int s, const struct afswtch *afp)
{
	struct lagg_reqport rp;

	bzero(&rp, sizeof(rp));
	strlcpy(rp.rp_ifname, name, sizeof(rp.rp_ifname));
	strlcpy(rp.rp_portname, val, sizeof(rp.rp_portname));

	if (ioctl(s, SIOCSLAGGPORT, &rp))
		err(1, "SIOCSLAGGPORT");
}

static void
unsetlaggport(const char *val, int d, int s, const struct afswtch *afp)
{
	struct lagg_reqport rp;

	bzero(&rp, sizeof(rp));
	strlcpy(rp.rp_ifname, name, sizeof(rp.rp_ifname));
	strlcpy(rp.rp_portname, val, sizeof(rp.rp_portname));

	if (ioctl(s, SIOCSLAGGDELPORT, &rp))
		err(1, "SIOCSLAGGDELPORT");
}

static void
setlaggproto(const char *val, int d, int s, const struct afswtch *afp)
{
	struct lagg_protos lpr[] = LAGG_PROTOS;
	struct lagg_reqall ra;
	int i;

	bzero(&ra, sizeof(ra));
	ra.ra_proto = LAGG_PROTO_MAX;

	for (i = 0; i < (sizeof(lpr) / sizeof(lpr[0])); i++) {
		if (strcmp(val, lpr[i].lpr_name) == 0) {
			ra.ra_proto = lpr[i].lpr_proto;
			break;
		}
	}
	if (ra.ra_proto == LAGG_PROTO_MAX)
		errx(1, "Invalid aggregation protocol: %s", val);

	strlcpy(ra.ra_ifname, name, sizeof(ra.ra_ifname));
	if (ioctl(s, SIOCSLAGG, &ra) != 0)
		err(1, "SIOCSLAGG");
}

static void
lagg_status(int s)
{
	struct lagg_protos lpr[] = LAGG_PROTOS;
	struct lagg_reqport rp, rpbuf[LAGG_MAX_PORTS];
	struct lagg_reqall ra;
	const char *proto = "<unknown>";
	int i, isport = 0;

	bzero(&rp, sizeof(rp));
	bzero(&ra, sizeof(ra));

	strlcpy(rp.rp_ifname, name, sizeof(rp.rp_ifname));
	strlcpy(rp.rp_portname, name, sizeof(rp.rp_portname));

	if (ioctl(s, SIOCGLAGGPORT, &rp) == 0)
		isport = 1;

	strlcpy(ra.ra_ifname, name, sizeof(ra.ra_ifname));
	ra.ra_size = sizeof(rpbuf);
	ra.ra_port = rpbuf;

	if (ioctl(s, SIOCGLAGG, &ra) == 0) {
		for (i = 0; i < (sizeof(lpr) / sizeof(lpr[0])); i++) {
			if (ra.ra_proto == lpr[i].lpr_proto) {
				proto = lpr[i].lpr_name;
				break;
			}
		}

		printf("\tlagg: laggproto %s", proto);
		if (isport)
			printf(" laggdev %s", rp.rp_ifname);
		putchar('\n');

		for (i = 0; i < ra.ra_ports; i++) {
			printf("\t\tlaggport %s ", rpbuf[i].rp_portname);
			printb("", rpbuf[i].rp_flags, LAGG_PORT_BITS);
			putchar('\n');
		}

		if (0 /* XXX */) {
			printf("\tsupported aggregation protocols:\n");
			for (i = 0; i < (sizeof(lpr) / sizeof(lpr[0])); i++)
				printf("\t\tlaggproto %s\n", lpr[i].lpr_name);
		}
	} else if (isport)
		printf("\tlagg: laggdev %s\n", rp.rp_ifname);
}

static struct cmd lagg_cmds[] = {
	DEF_CMD_ARG("laggport",		setlaggport),
	DEF_CMD_ARG("-laggport",	unsetlaggport),
	DEF_CMD_ARG("laggproto",	setlaggproto),
};
static struct afswtch af_lagg = {
	.af_name	= "af_lagg",
	.af_af		= AF_UNSPEC,
	.af_other_status = lagg_status,
};

static __constructor void
lagg_ctor(void)
{
#define	N(a)	(sizeof(a) / sizeof(a[0]))
	int i;

	for (i = 0; i < N(lagg_cmds);  i++)
		cmd_register(&lagg_cmds[i]);
	af_register(&af_lagg);
#undef N
}
