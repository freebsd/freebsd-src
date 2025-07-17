/*-
 * Copyright (c) 2005-2014 Sandvine Incorporated. All rights reserved.
 * Copyright (c) 2000 Darrell Anderson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * netdump_client.c
 * FreeBSD subsystem supporting netdump network dumps.
 * A dedicated server must be running to accept client dumps.
 */

#include <sys/cdefs.h>
#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/disk.h>
#include <sys/endian.h>
#include <sys/eventhandler.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/kerneldump.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/systm.h>

#ifdef DDB
#include <ddb/ddb.h>
#include <ddb/db_lex.h>
#endif

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_var.h>
#include <net/if_private.h>
#include <net/debugnet.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_options.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/netdump/netdump.h>

#include <machine/in_cksum.h>
#include <machine/pcb.h>

#define	NETDDEBUGV(f, ...) do {						\
	if (nd_debug > 1)						\
		printf(("%s: " f), __func__, ## __VA_ARGS__);		\
} while (0)

static void	 netdump_cleanup(void);
static int	 netdump_configure(struct diocskerneldump_arg *,
		    struct thread *);
static int	 netdump_dumper(void *priv __unused, void *virtual,
		    off_t offset, size_t length);
static bool	 netdump_enabled(void);
static int	 netdump_enabled_sysctl(SYSCTL_HANDLER_ARGS);
static int	 netdump_ioctl(struct cdev *dev __unused, u_long cmd,
		    caddr_t addr, int flags __unused, struct thread *td);
static int	 netdump_modevent(module_t mod, int type, void *priv);
static int	 netdump_start(struct dumperinfo *di, void *key,
		    uint32_t keysize);
static void	 netdump_unconfigure(void);

/* Must be at least as big as the chunks dumpsys() gives us. */
static unsigned char nd_buf[MAXDUMPPGS * PAGE_SIZE];
static int dump_failed;

/* Configuration parameters. */
static struct {
	char		 ndc_iface[IFNAMSIZ];
	union kd_ip	 ndc_server;
	union kd_ip	 ndc_client;
	union kd_ip	 ndc_gateway;
	uint8_t		 ndc_af;
	/* Runtime State */
	struct debugnet_pcb *nd_pcb;
	off_t		 nd_tx_off;
	size_t		 nd_buf_len;
} nd_conf;
#define	nd_server	nd_conf.ndc_server.in4
#define	nd_client	nd_conf.ndc_client.in4
#define	nd_gateway	nd_conf.ndc_gateway.in4

/* General dynamic settings. */
static struct sx nd_conf_lk;
SX_SYSINIT(nd_conf, &nd_conf_lk, "netdump configuration lock");
#define NETDUMP_WLOCK()			sx_xlock(&nd_conf_lk)
#define NETDUMP_WUNLOCK()		sx_xunlock(&nd_conf_lk)
#define NETDUMP_RLOCK()			sx_slock(&nd_conf_lk)
#define NETDUMP_RUNLOCK()		sx_sunlock(&nd_conf_lk)
#define NETDUMP_ASSERT_WLOCKED()	sx_assert(&nd_conf_lk, SA_XLOCKED)
#define NETDUMP_ASSERT_LOCKED()		sx_assert(&nd_conf_lk, SA_LOCKED)
static struct ifnet *nd_ifp;
static eventhandler_tag nd_detach_cookie;

FEATURE(netdump, "Netdump client support");

static SYSCTL_NODE(_net, OID_AUTO, netdump, CTLFLAG_RD | CTLFLAG_MPSAFE, NULL,
    "netdump parameters");

static int nd_debug;
SYSCTL_INT(_net_netdump, OID_AUTO, debug, CTLFLAG_RWTUN,
    &nd_debug, 0,
    "Debug message verbosity");
SYSCTL_PROC(_net_netdump, OID_AUTO, enabled,
    CTLFLAG_RD | CTLTYPE_INT | CTLFLAG_MPSAFE, NULL, 0,
    netdump_enabled_sysctl, "I",
    "netdump configuration status");
static char nd_path[MAXPATHLEN];
SYSCTL_STRING(_net_netdump, OID_AUTO, path, CTLFLAG_RW,
    nd_path, sizeof(nd_path),
    "Server path for output files");
/*
 * The following three variables were moved to debugnet(4), but these knobs
 * were retained as aliases.
 */
SYSCTL_INT(_net_netdump, OID_AUTO, polls, CTLFLAG_RWTUN,
    &debugnet_npolls, 0,
    "Number of times to poll before assuming packet loss (0.5ms per poll)");
SYSCTL_INT(_net_netdump, OID_AUTO, retries, CTLFLAG_RWTUN,
    &debugnet_nretries, 0,
    "Number of retransmit attempts before giving up");
SYSCTL_INT(_net_netdump, OID_AUTO, arp_retries, CTLFLAG_RWTUN,
    &debugnet_arp_nretries, 0,
    "Number of ARP attempts before giving up");

static bool nd_is_enabled;
static bool
netdump_enabled(void)
{

	NETDUMP_ASSERT_LOCKED();
	return (nd_is_enabled);
}

static void
netdump_set_enabled(bool status)
{

	NETDUMP_ASSERT_LOCKED();
	nd_is_enabled = status;
}

static int
netdump_enabled_sysctl(SYSCTL_HANDLER_ARGS)
{
	int en, error;

	NETDUMP_RLOCK();
	en = netdump_enabled();
	NETDUMP_RUNLOCK();

	error = SYSCTL_OUT(req, &en, sizeof(en));
	if (error != 0 || req->newptr == NULL)
		return (error);
	return (EPERM);
}

/*-
 * Dumping specific primitives.
 */

/*
 * Flush any buffered vmcore data.
 */
static int
netdump_flush_buf(void)
{
	int error;

	error = 0;
	if (nd_conf.nd_buf_len != 0) {
		struct debugnet_proto_aux auxdata = {
			.dp_offset_start = nd_conf.nd_tx_off,
		};
		error = debugnet_send(nd_conf.nd_pcb, DEBUGNET_DATA, nd_buf,
		    nd_conf.nd_buf_len, &auxdata);
		if (error == 0)
			nd_conf.nd_buf_len = 0;
	}
	return (error);
}

/*
 * Callback from dumpsys() to dump a chunk of memory.
 * Copies it out to our static buffer then sends it across the network.
 * Detects the initial KDH and makes sure it is given a special packet type.
 *
 * Parameters:
 *	priv	 Unused. Optional private pointer.
 *	virtual  Virtual address (where to read the data from)
 *	offset	 Offset from start of core file
 *	length	 Data length
 *
 * Return value:
 *	0 on success
 *	errno on error
 */
static int
netdump_dumper(void *priv __unused, void *virtual, off_t offset, size_t length)
{
	int error;

	NETDDEBUGV("netdump_dumper(NULL, %p, NULL, %ju, %zu)\n",
	    virtual, (uintmax_t)offset, length);

	if (virtual == NULL) {
		error = netdump_flush_buf();
		if (error != 0)
			dump_failed = 1;

		if (dump_failed != 0)
			printf("failed to dump the kernel core\n");
		else if (
		    debugnet_sendempty(nd_conf.nd_pcb, DEBUGNET_FINISHED) != 0)
			printf("failed to close the transaction\n");
		else
			printf("\nnetdump finished.\n");
		netdump_cleanup();
		return (0);
	}
	if (length > sizeof(nd_buf)) {
		netdump_cleanup();
		return (ENOSPC);
	}

	if (nd_conf.nd_buf_len + length > sizeof(nd_buf) ||
	    (nd_conf.nd_buf_len != 0 && nd_conf.nd_tx_off +
	    nd_conf.nd_buf_len != offset)) {
		error = netdump_flush_buf();
		if (error != 0) {
			dump_failed = 1;
			netdump_cleanup();
			return (error);
		}
		nd_conf.nd_tx_off = offset;
	}

	memmove(nd_buf + nd_conf.nd_buf_len, virtual, length);
	nd_conf.nd_buf_len += length;

	return (0);
}

/*
 * Perform any initialization needed prior to transmitting the kernel core.
 */
static int
netdump_start(struct dumperinfo *di, void *key, uint32_t keysize)
{
	struct debugnet_conn_params dcp;
	struct debugnet_pcb *pcb;
	char buf[INET_ADDRSTRLEN];
	int error;

	error = 0;

	/* Check if the dumping is allowed to continue. */
	if (!netdump_enabled())
		return (EINVAL);

	if (!KERNEL_PANICKED()) {
		printf(
		    "netdump_start: netdump may only be used after a panic\n");
		return (EINVAL);
	}

	memset(&dcp, 0, sizeof(dcp));

	if (nd_server.s_addr == INADDR_ANY) {
		printf("netdump_start: can't netdump; no server IP given\n");
		return (EINVAL);
	}

	/* We start dumping at offset 0. */
	di->dumpoff = 0;

	dcp.dc_ifp = nd_ifp;

	dcp.dc_client = nd_client.s_addr;
	dcp.dc_server = nd_server.s_addr;
	dcp.dc_gateway = nd_gateway.s_addr;

	dcp.dc_herald_port = NETDUMP_PORT;
	dcp.dc_client_port = NETDUMP_ACKPORT;

	dcp.dc_herald_data = nd_path;
	dcp.dc_herald_datalen = (nd_path[0] == 0) ? 0 : strlen(nd_path) + 1;

	error = debugnet_connect(&dcp, &pcb);
	if (error != 0) {
		printf("failed to contact netdump server\n");
		/* Squash debugnet to something the dumper code understands. */
		return (EINVAL);
	}

	printf("netdumping to %s (%6D)\n", inet_ntoa_r(nd_server, buf),
	    debugnet_get_gw_mac(pcb), ":");
	nd_conf.nd_pcb = pcb;

	/* Send the key before the dump so a partial dump is still usable. */
	if (keysize > 0) {
		if (keysize > sizeof(nd_buf)) {
			printf("crypto key is too large (%u)\n", keysize);
			error = EINVAL;
			goto out;
		}
		memcpy(nd_buf, key, keysize);
		error = debugnet_send(pcb, NETDUMP_EKCD_KEY, nd_buf, keysize,
		    NULL);
		if (error != 0) {
			printf("error %d sending crypto key\n", error);
			goto out;
		}
	}

out:
	if (error != 0) {
		/* As above, squash errors. */
		error = EINVAL;
		netdump_cleanup();
	}
	return (error);
}

static int
netdump_write_headers(struct dumperinfo *di, struct kerneldumpheader *kdh)
{
	int error;

	error = netdump_flush_buf();
	if (error != 0)
		goto out;
	memcpy(nd_buf, kdh, sizeof(*kdh));
	error = debugnet_send(nd_conf.nd_pcb, NETDUMP_KDH, nd_buf,
	    sizeof(*kdh), NULL);
out:
	if (error != 0)
		netdump_cleanup();
	return (error);
}

/*
 * Cleanup routine for a possibly failed netdump.
 */
static void
netdump_cleanup(void)
{
	if (nd_conf.nd_pcb != NULL) {
		debugnet_free(nd_conf.nd_pcb);
		nd_conf.nd_pcb = NULL;
	}
}

/*-
 * KLD specific code.
 */

static struct cdevsw netdump_cdevsw = {
	.d_version =	D_VERSION,
	.d_ioctl =	netdump_ioctl,
	.d_name =	"netdump",
};

static struct cdev *netdump_cdev;

static void
netdump_unconfigure(void)
{
	struct diocskerneldump_arg kda;

	NETDUMP_ASSERT_WLOCKED();
	KASSERT(netdump_enabled(), ("%s: not enabled", __func__));

	bzero(&kda, sizeof(kda));
	kda.kda_index = KDA_REMOVE_DEV;
	(void)dumper_remove(nd_conf.ndc_iface, &kda);

	if (nd_ifp != NULL)
		if_rele(nd_ifp);
	nd_ifp = NULL;
	netdump_set_enabled(false);

	log(LOG_WARNING, "netdump: Lost configured interface %s\n",
	    nd_conf.ndc_iface);

	bzero(&nd_conf, sizeof(nd_conf));
}

static void
netdump_ifdetach(void *arg __unused, struct ifnet *ifp)
{

	NETDUMP_WLOCK();
	if (ifp == nd_ifp)
		netdump_unconfigure();
	NETDUMP_WUNLOCK();
}

/*
 * td of NULL is a sentinel value that indicates a kernel caller (ddb(4) or
 * modload-based tunable parameters).
 */
static int
netdump_configure(struct diocskerneldump_arg *conf, struct thread *td)
{
	struct ifnet *ifp;

	NETDUMP_ASSERT_WLOCKED();

	if (conf->kda_iface[0] != 0) {
		if (td != NULL && !IS_DEFAULT_VNET(TD_TO_VNET(td)))
			return (EINVAL);
		CURVNET_SET(vnet0);
		ifp = ifunit_ref(conf->kda_iface);
		CURVNET_RESTORE();
		if (ifp == NULL)
			return (ENODEV);
		if (!DEBUGNET_SUPPORTED_NIC(ifp)) {
			if_rele(ifp);
			return (ENODEV);
		}
	} else
		ifp = NULL;

	if (nd_ifp != NULL)
		if_rele(nd_ifp);
	nd_ifp = ifp;
	netdump_set_enabled(true);

#define COPY_SIZED(elm) do {	\
	_Static_assert(sizeof(nd_conf.ndc_ ## elm) ==			\
	    sizeof(conf->kda_ ## elm), "elm " __XSTRING(elm) " mismatch"); \
	memcpy(&nd_conf.ndc_ ## elm, &conf->kda_ ## elm,		\
	    sizeof(nd_conf.ndc_ ## elm));				\
} while (0)
	COPY_SIZED(iface);
	COPY_SIZED(server);
	COPY_SIZED(client);
	COPY_SIZED(gateway);
	COPY_SIZED(af);
#undef COPY_SIZED

	return (0);
}

/*
 * ioctl(2) handler for the netdump device. This is currently only used to
 * register netdump as a dump device.
 *
 * Parameters:
 *     dev, Unused.
 *     cmd, The ioctl to be handled.
 *     addr, The parameter for the ioctl.
 *     flags, Unused.
 *     td, The thread invoking this ioctl.
 *
 * Returns:
 *     0 on success, and an errno value on failure.
 */
static int
netdump_ioctl(struct cdev *dev __unused, u_long cmd, caddr_t addr,
    int flags __unused, struct thread *td)
{
	struct diocskerneldump_arg *conf;
	struct dumperinfo dumper;
	uint8_t *encryptedkey;
	int error;

	conf = NULL;
	error = 0;
	NETDUMP_WLOCK();

	switch (cmd) {
	case DIOCGKERNELDUMP:
		conf = (void *)addr;
		/*
		 * For now, index is ignored; netdump doesn't support multiple
		 * configurations (yet).
		 */
		if (!netdump_enabled()) {
			error = ENXIO;
			conf = NULL;
			break;
		}

		if (nd_ifp != NULL)
			strlcpy(conf->kda_iface, nd_ifp->if_xname,
			    sizeof(conf->kda_iface));
		memcpy(&conf->kda_server, &nd_server, sizeof(nd_server));
		memcpy(&conf->kda_client, &nd_client, sizeof(nd_client));
		memcpy(&conf->kda_gateway, &nd_gateway, sizeof(nd_gateway));
		conf->kda_af = nd_conf.ndc_af;
		conf = NULL;
		break;
	case DIOCSKERNELDUMP:
		encryptedkey = NULL;
		conf = (void *)addr;

		/* Netdump only supports IP4 at this time. */
		if (conf->kda_af != AF_INET) {
			error = EPROTONOSUPPORT;
			break;
		}

		conf->kda_iface[sizeof(conf->kda_iface) - 1] = '\0';
		if (conf->kda_index == KDA_REMOVE ||
		    conf->kda_index == KDA_REMOVE_DEV ||
		    conf->kda_index == KDA_REMOVE_ALL) {
			if (netdump_enabled())
				netdump_unconfigure();
			if (conf->kda_index == KDA_REMOVE_ALL)
				error = dumper_remove(NULL, conf);
			break;
		}

		error = netdump_configure(conf, td);
		if (error != 0)
			break;

		if (conf->kda_encryption != KERNELDUMP_ENC_NONE) {
			if (conf->kda_encryptedkeysize <= 0 ||
			    conf->kda_encryptedkeysize >
			    KERNELDUMP_ENCKEY_MAX_SIZE) {
				error = EINVAL;
				break;
			}
			encryptedkey = malloc(conf->kda_encryptedkeysize,
			    M_TEMP, M_WAITOK);
			error = copyin(conf->kda_encryptedkey, encryptedkey,
			    conf->kda_encryptedkeysize);
			if (error != 0) {
				free(encryptedkey, M_TEMP);
				break;
			}

			conf->kda_encryptedkey = encryptedkey;
		}

		memset(&dumper, 0, sizeof(dumper));
		dumper.dumper_start = netdump_start;
		dumper.dumper_hdr = netdump_write_headers;
		dumper.dumper = netdump_dumper;
		dumper.priv = NULL;
		dumper.blocksize = NETDUMP_DATASIZE;
		dumper.maxiosize = MAXDUMPPGS * PAGE_SIZE;
		dumper.mediaoffset = 0;
		dumper.mediasize = 0;

		error = dumper_insert(&dumper, conf->kda_iface, conf);
		zfree(encryptedkey, M_TEMP);
		if (error != 0)
			netdump_unconfigure();
		break;
	default:
		error = ENOTTY;
		break;
	}
	if (conf != NULL)
		explicit_bzero(conf, sizeof(*conf));
	NETDUMP_WUNLOCK();
	return (error);
}

/*
 * Called upon system init or kld load.  Initializes the netdump parameters to
 * sane defaults (locates the first available NIC and uses the first IPv4 IP on
 * that card as the client IP).  Leaves the server IP unconfigured.
 *
 * Parameters:
 *	mod, Unused.
 *	what, The module event type.
 *	priv, Unused.
 *
 * Returns:
 *	int, An errno value if an error occurred, 0 otherwise.
 */
static int
netdump_modevent(module_t mod __unused, int what, void *priv __unused)
{
	struct diocskerneldump_arg conf;
	char *arg;
	int error;

	error = 0;
	switch (what) {
	case MOD_LOAD:
		error = make_dev_p(MAKEDEV_WAITOK, &netdump_cdev,
		    &netdump_cdevsw, 0, UID_ROOT, GID_WHEEL, 0600, "netdump");
		if (error != 0)
			return (error);

		nd_detach_cookie = EVENTHANDLER_REGISTER(ifnet_departure_event,
		    netdump_ifdetach, NULL, EVENTHANDLER_PRI_ANY);

		if ((arg = kern_getenv("net.dump.iface")) != NULL) {
			strlcpy(conf.kda_iface, arg, sizeof(conf.kda_iface));
			freeenv(arg);

			if ((arg = kern_getenv("net.dump.server")) != NULL) {
				inet_aton(arg, &conf.kda_server.in4);
				freeenv(arg);
			}
			if ((arg = kern_getenv("net.dump.client")) != NULL) {
				inet_aton(arg, &conf.kda_client.in4);
				freeenv(arg);
			}
			if ((arg = kern_getenv("net.dump.gateway")) != NULL) {
				inet_aton(arg, &conf.kda_gateway.in4);
				freeenv(arg);
			}
			conf.kda_af = AF_INET;

			/* Ignore errors; we print a message to the console. */
			NETDUMP_WLOCK();
			(void)netdump_configure(&conf, NULL);
			NETDUMP_WUNLOCK();
		}
		break;
	case MOD_UNLOAD:
		NETDUMP_WLOCK();
		if (netdump_enabled()) {
			printf("netdump: disabling dump device for unload\n");
			netdump_unconfigure();
		}
		NETDUMP_WUNLOCK();
		destroy_dev(netdump_cdev);
		EVENTHANDLER_DEREGISTER(ifnet_departure_event,
		    nd_detach_cookie);
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}
	return (error);
}

static moduledata_t netdump_mod = {
	"netdump",
	netdump_modevent,
	NULL,
};

MODULE_VERSION(netdump, 1);
DECLARE_MODULE(netdump, netdump_mod, SI_SUB_PSEUDO, SI_ORDER_ANY);

#ifdef DDB
/*
 * Usage: netdump -s <server> [-g <gateway] -c <localip> -i <interface>
 *
 * Order is not significant.
 *
 * Currently, this command does not support configuring encryption or
 * compression.
 */
DB_COMMAND_FLAGS(netdump, db_netdump_cmd, CS_OWN)
{
	static struct diocskerneldump_arg conf;
	static char blockbuf[NETDUMP_DATASIZE];
	static union {
		struct dumperinfo di;
		/* For valid di_devname. */
		char di_buf[sizeof(struct dumperinfo) + 1];
	} u;

	struct debugnet_ddb_config params;
	int error;

	error = debugnet_parse_ddb_cmd("netdump", &params);
	if (error != 0) {
		db_printf("Error configuring netdump: %d\n", error);
		return;
	}

	/* Translate to a netdump dumper config. */
	memset(&conf, 0, sizeof(conf));

	if (params.dd_ifp != NULL)
		strlcpy(conf.kda_iface, if_name(params.dd_ifp),
		    sizeof(conf.kda_iface));

	conf.kda_af = AF_INET;
	conf.kda_server.in4 = (struct in_addr) { params.dd_server };
	if (params.dd_has_client)
		conf.kda_client.in4 = (struct in_addr) { params.dd_client };
	else
		conf.kda_client.in4 = (struct in_addr) { INADDR_ANY };
	if (params.dd_has_gateway)
		conf.kda_gateway.in4 = (struct in_addr) { params.dd_gateway };
	else
		conf.kda_gateway.in4 = (struct in_addr) { INADDR_ANY };

	/* Set the global netdump config to these options. */
	error = netdump_configure(&conf, NULL);
	if (error != 0) {
		db_printf("Error enabling netdump: %d\n", error);
		return;
	}

	/* Fake the generic dump configuration list entry to avoid malloc. */
	memset(&u.di_buf, 0, sizeof(u.di_buf));
	u.di.dumper_start = netdump_start;
	u.di.dumper_hdr = netdump_write_headers;
	u.di.dumper = netdump_dumper;
	u.di.priv = NULL;
	u.di.blocksize = NETDUMP_DATASIZE;
	u.di.maxiosize = MAXDUMPPGS * PAGE_SIZE;
	u.di.mediaoffset = 0;
	u.di.mediasize = 0;
	u.di.blockbuf = blockbuf;

	dumper_ddb_insert(&u.di);

	error = doadump(false);

	dumper_ddb_remove(&u.di);
	if (error != 0)
		db_printf("Cannot dump: %d\n", error);
}
#endif /* DDB */
