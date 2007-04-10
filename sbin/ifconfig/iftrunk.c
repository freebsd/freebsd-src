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
#include <net/if_trunk.h>
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
settrunkport(const char *val, int d, int s, const struct afswtch *afp)
{
	struct trunk_reqport rp;

	bzero(&rp, sizeof(rp));
	strlcpy(rp.rp_ifname, name, sizeof(rp.rp_ifname));
	strlcpy(rp.rp_portname, val, sizeof(rp.rp_portname));

	if (ioctl(s, SIOCSTRUNKPORT, &rp))
		err(1, "SIOCSTRUNKPORT");
}

static void
unsettrunkport(const char *val, int d, int s, const struct afswtch *afp)
{
	struct trunk_reqport rp;

	bzero(&rp, sizeof(rp));
	strlcpy(rp.rp_ifname, name, sizeof(rp.rp_ifname));
	strlcpy(rp.rp_portname, val, sizeof(rp.rp_portname));

	if (ioctl(s, SIOCSTRUNKDELPORT, &rp))
		err(1, "SIOCSTRUNKDELPORT");
}

static void
settrunkproto(const char *val, int d, int s, const struct afswtch *afp)
{
	struct trunk_protos tpr[] = TRUNK_PROTOS;
	struct trunk_reqall ra;
	int i;

	bzero(&ra, sizeof(ra));
	ra.ra_proto = TRUNK_PROTO_MAX;

	for (i = 0; i < (sizeof(tpr) / sizeof(tpr[0])); i++) {
		if (strcmp(val, tpr[i].tpr_name) == 0) {
			ra.ra_proto = tpr[i].tpr_proto;
			break;
		}
	}
	if (ra.ra_proto == TRUNK_PROTO_MAX)
		errx(1, "Invalid trunk protocol: %s", val);

	strlcpy(ra.ra_ifname, name, sizeof(ra.ra_ifname));
	if (ioctl(s, SIOCSTRUNK, &ra) != 0)
		err(1, "SIOCSTRUNK");
}

static void
trunk_status(int s)
{
	struct trunk_protos tpr[] = TRUNK_PROTOS;
	struct trunk_reqport rp, rpbuf[TRUNK_MAX_PORTS];
	struct trunk_reqall ra;
	const char *proto = "<unknown>";
	int i, isport = 0;

	bzero(&rp, sizeof(rp));
	bzero(&ra, sizeof(ra));

	strlcpy(rp.rp_ifname, name, sizeof(rp.rp_ifname));
	strlcpy(rp.rp_portname, name, sizeof(rp.rp_portname));

	if (ioctl(s, SIOCGTRUNKPORT, &rp) == 0)
		isport = 1;

	strlcpy(ra.ra_ifname, name, sizeof(ra.ra_ifname));
	ra.ra_size = sizeof(rpbuf);
	ra.ra_port = rpbuf;

	if (ioctl(s, SIOCGTRUNK, &ra) == 0) {
		for (i = 0; i < (sizeof(tpr) / sizeof(tpr[0])); i++) {
			if (ra.ra_proto == tpr[i].tpr_proto) {
				proto = tpr[i].tpr_name;
				break;
			}
		}

		printf("\ttrunk: trunkproto %s", proto);
		if (isport)
			printf(" trunkdev %s", rp.rp_ifname);
		putchar('\n');

		for (i = 0; i < ra.ra_ports; i++) {
			printf("\t\ttrunkport %s ", rpbuf[i].rp_portname);
			printb("", rpbuf[i].rp_flags, TRUNK_PORT_BITS);
			putchar('\n');
		}

		if (0 /* XXX */) {
			printf("\tsupported trunk protocols:\n");
			for (i = 0; i < (sizeof(tpr) / sizeof(tpr[0])); i++)
				printf("\t\ttrunkproto %s\n", tpr[i].tpr_name);
		}
	} else if (isport)
		printf("\ttrunk: trunkdev %s\n", rp.rp_ifname);
}

static struct cmd trunk_cmds[] = {
	DEF_CMD_ARG("trunkport",		settrunkport),
	DEF_CMD_ARG("-trunkport",		unsettrunkport),
	DEF_CMD_ARG("trunkproto",		settrunkproto),
};
static struct afswtch af_trunk = {
	.af_name	= "af_trunk",
	.af_af		= AF_UNSPEC,
	.af_other_status = trunk_status,
};

static __constructor void
trunk_ctor(void)
{
#define	N(a)	(sizeof(a) / sizeof(a[0]))
	int i;

	for (i = 0; i < N(trunk_cmds);  i++)
		cmd_register(&trunk_cmds[i]);
	af_register(&af_trunk);
#undef N
}
