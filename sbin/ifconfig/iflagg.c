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

char lacpbuf[120];	/* LACP peer '[(a,a,a),(p,p,p)]' */

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

static char *
lacp_format_mac(const uint8_t *mac, char *buf, size_t buflen)
{
	snprintf(buf, buflen, "%02X-%02X-%02X-%02X-%02X-%02X",
	    (int)mac[0], (int)mac[1], (int)mac[2], (int)mac[3],
	    (int)mac[4], (int)mac[5]);

	return (buf);
}

static char *
lacp_format_peer(struct lacp_opreq *req, const char *sep)
{
	char macbuf1[20];
	char macbuf2[20];

	snprintf(lacpbuf, sizeof(lacpbuf),
	    "[(%04X,%s,%04X,%04X,%04X),%s(%04X,%s,%04X,%04X,%04X)]",
	    req->actor_prio,
	    lacp_format_mac(req->actor_mac, macbuf1, sizeof(macbuf1)),
	    req->actor_key, req->actor_portprio, req->actor_portno, sep,
	    req->partner_prio,
	    lacp_format_mac(req->partner_mac, macbuf2, sizeof(macbuf2)),
	    req->partner_key, req->partner_portprio, req->partner_portno);

	return(lacpbuf);
}

static void
lagg_status(int s)
{
	struct lagg_protos lpr[] = LAGG_PROTOS;
	struct lagg_reqport rp, rpbuf[LAGG_MAX_PORTS];
	struct lagg_reqall ra;
	struct lacp_opreq *lp;
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
		lp = (struct lacp_opreq *)&ra.ra_lacpreq;

		for (i = 0; i < (sizeof(lpr) / sizeof(lpr[0])); i++) {
			if (ra.ra_proto == lpr[i].lpr_proto) {
				proto = lpr[i].lpr_name;
				break;
			}
		}

		printf("\tlaggproto %s", proto);
		if (isport)
			printf(" laggdev %s", rp.rp_ifname);
		putchar('\n');
		if (verbose && ra.ra_proto == LAGG_PROTO_LACP)
			printf("\tlag id: %s\n",
			    lacp_format_peer(lp, "\n\t\t "));

		for (i = 0; i < ra.ra_ports; i++) {
			lp = (struct lacp_opreq *)&rpbuf[i].rp_lacpreq;
			printf("\tlaggport: %s ", rpbuf[i].rp_portname);
			printb("flags", rpbuf[i].rp_flags, LAGG_PORT_BITS);
			if (verbose && ra.ra_proto == LAGG_PROTO_LACP)
				printf(" state=%X", lp->actor_state);
			putchar('\n');
			if (verbose && ra.ra_proto == LAGG_PROTO_LACP)
				printf("\t\t%s\n",
				    lacp_format_peer(lp, "\n\t\t "));
		}

		if (0 /* XXX */) {
			printf("\tsupported aggregation protocols:\n");
			for (i = 0; i < (sizeof(lpr) / sizeof(lpr[0])); i++)
				printf("\t\tlaggproto %s\n", lpr[i].lpr_name);
		}
	}
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
