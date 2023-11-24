/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Michael Tuexen <tuexen@FreeBSD.org>
 * Copyright (c) 2009 Juli Mallett <jmallett@FreeBSD.org>
 * Copyright (c) 2004 Markus Friedl <markus@openbsd.org>
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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>

#include <netinet/in.h>
#include <netinet/in_pcb.h>
#define TCPSTATES
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_var.h>

#include <err.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static struct xinpgen *
getxpcblist(const char *name)
{
	struct xinpgen *xinp;
	size_t len;
	int rv;

	len = 0;
	rv = sysctlbyname(name, NULL, &len, NULL, 0);
	if (rv == -1)
		err(1, "sysctlbyname %s", name);

	if (len == 0)
		errx(1, "%s is empty", name);

	xinp = malloc(len);
	if (xinp == NULL)
		errx(1, "malloc failed");

	rv = sysctlbyname(name, xinp, &len, NULL, 0);
	if (rv == -1)
		err(1, "sysctlbyname %s", name);

	return (xinp);
}

static bool
tcpsso(uint64_t id, struct sockopt_parameters *params, size_t optlen)
{
	int rv;

	params->sop_id = id;
	rv = sysctlbyname("net.inet.tcp.setsockopt", NULL, NULL, params,
	    sizeof(struct sockopt_parameters) + optlen);
	if (rv == -1) {
		warn("Failed for id %" PRIu64, params->sop_id);
		return (false);
	} else
		return (true);
}

static bool
tcpssoall(const char *ca_name, const char *stack, int state,
    struct sockopt_parameters *params, size_t optlen)
{
	struct xinpgen *head, *xinp;
	struct xtcpcb *xtp;
	struct xinpcb *xip;
	bool ok;

	ok = true;

	head = getxpcblist("net.inet.tcp.pcblist");

#define	XINP_NEXT(xinp)							\
	((struct xinpgen *)(uintptr_t)((uintptr_t)(xinp) + (xinp)->xig_len))

	for (xinp = XINP_NEXT(head); xinp->xig_len > sizeof *xinp;
	    xinp = XINP_NEXT(xinp)) {
		xtp = (struct xtcpcb *)xinp;
		xip = &xtp->xt_inp;

		/* Ignore PCBs which were freed during copyout. */
		if (xip->inp_gencnt > head->xig_gen)
			continue;


		/* Skip endpoints in TIME WAIT. */
		if (xtp->t_state == TCPS_TIME_WAIT)
			continue;

		/* If requested, skip sockets not having the requested state. */
		if ((state != -1) && (xtp->t_state != state))
			continue;

		/*
		 * If requested, skip sockets not having the requested
		 * congestion control algorithm.
		 */
		if (ca_name[0] != '\0' &&
		    strncmp(xtp->xt_cc, ca_name, TCP_CA_NAME_MAX))
			continue;

		/* If requested, skip sockets not having the requested stack. */
		if (stack[0] != '\0' &&
		    strncmp(xtp->xt_stack, stack, TCP_FUNCTION_NAME_LEN_MAX))
			continue;

		params->sop_inc = xip->inp_inc;
		if (!tcpsso(xip->inp_gencnt, params, optlen))
			ok = false;
	}
	free(head);

	return (ok);
}

struct so_level {
	int level;
	const char *name;
};

#define level_entry(level) { level, #level }

static struct so_level so_levels[] = {
	level_entry(SOL_SOCKET),
	level_entry(IPPROTO_IP),
	level_entry(IPPROTO_IPV6),
	level_entry(IPPROTO_TCP),
	{ 0, NULL }
};

struct so_name {
	int level;
	int value;
	const char *name;
};

#define sol_entry(name) { SOL_SOCKET, name, #name }
#define ip4_entry(name) { IPPROTO_IP, name, #name }
#define ip6_entry(name) { IPPROTO_IPV6, name, #name }
#define tcp_entry(name) { IPPROTO_TCP, name, #name }

static struct so_name so_names[] = {
	/* SOL_SOCKET level socket options. */
	sol_entry(SO_DEBUG),			/* int */
	sol_entry(SO_RCVBUF),			/* int */
	sol_entry(SO_SNDBUF),			/* int */
	sol_entry(SO_RCVLOWAT),			/* int */
	sol_entry(SO_SNDLOWAT),			/* int */
	/* IPPROTO_IP level socket options. */
	ip4_entry(IP_TTL),			/* int */
	ip4_entry(IP_TOS),			/* int */
	/* IPPROTO_IPV6 level socket options. */
	ip6_entry(IPV6_UNICAST_HOPS),		/* int */
	ip6_entry(IPV6_TCLASS),			/* int */
	ip6_entry(IPV6_USE_MIN_MTU),		/* int */
	/* IPPROTO_TCP level socket options. */
	tcp_entry(TCP_NODELAY),			/* int */
	tcp_entry(TCP_NOOPT),			/* int */
	tcp_entry(TCP_NOPUSH),			/* int */
	tcp_entry(TCP_REMOTE_UDP_ENCAPS_PORT),	/* int */
	tcp_entry(TCP_MAXSEG),			/* int */
	tcp_entry(TCP_TXTLS_MODE),		/* unsigned int */
	tcp_entry(TCP_MAXUNACKTIME),		/* unsigned int */
	tcp_entry(TCP_KEEPIDLE),		/* unsigned int */
	tcp_entry(TCP_KEEPINTVL),		/* unsigned int */
	tcp_entry(TCP_KEEPINIT),		/* unsigned int */
	tcp_entry(TCP_KEEPCNT),			/* unsigned int */
	tcp_entry(TCP_PCAP_OUT),		/* int */
	tcp_entry(TCP_PCAP_IN),			/* int */
	tcp_entry(TCP_LOG),			/* int */
	tcp_entry(TCP_LOGID),			/* char * */
	tcp_entry(TCP_LOGDUMP),			/* char * */
	tcp_entry(TCP_LOGDUMPID),		/* char * */
	tcp_entry(TCP_CONGESTION),		/* char * */
	tcp_entry(TCP_FUNCTION_BLK),		/* char * */
	tcp_entry(TCP_NO_PRR),			/* int */
	tcp_entry(TCP_HDWR_RATE_CAP),		/* int */
#if notyet
	tcp_entry(TCP_PACING_RATE_CAP),		/* uint64_t */
#endif
	tcp_entry(TCP_HDWR_UP_ONLY),		/* int */
	tcp_entry(TCP_FAST_RSM_HACK),		/* int */
	tcp_entry(TCP_DELACK),			/* int */
	tcp_entry(TCP_REC_ABC_VAL),		/* int */
	tcp_entry(TCP_USE_CMP_ACKS),		/* int */
	tcp_entry(TCP_SHARED_CWND_TIME_LIMIT),	/* int */
	tcp_entry(TCP_SHARED_CWND_ENABLE),	/* int */
	tcp_entry(TCP_DATA_AFTER_CLOSE),	/* int */
	tcp_entry(TCP_DEFER_OPTIONS),		/* int */
	tcp_entry(TCP_MAXPEAKRATE),		/* int */
	tcp_entry(TCP_TIMELY_DYN_ADJ),		/* int */
	tcp_entry(TCP_RACK_TLP_REDUCE),		/* int */
	tcp_entry(TCP_RACK_PACE_ALWAYS),	/* int */
	tcp_entry(TCP_RACK_PACE_MAX_SEG),	/* int */
	tcp_entry(TCP_RACK_FORCE_MSEG),		/* int */
	tcp_entry(TCP_RACK_PACE_RATE_CA),	/* int */
	tcp_entry(TCP_RACK_PACE_RATE_SS),	/* int */
	tcp_entry(TCP_RACK_PACE_RATE_REC),	/* int */
	tcp_entry(TCP_RACK_GP_INCREASE_CA),	/* int */
	tcp_entry(TCP_RACK_GP_INCREASE_SS),	/* int */
	tcp_entry(TCP_RACK_GP_INCREASE_REC),	/* int */
	tcp_entry(TCP_RACK_RR_CONF),		/* int */
	tcp_entry(TCP_RACK_PRR_SENDALOT),	/* int */
	tcp_entry(TCP_RACK_MIN_TO),		/* int */
	tcp_entry(TCP_RACK_EARLY_SEG),		/* int */
	tcp_entry(TCP_RACK_REORD_THRESH),	/* int */
	tcp_entry(TCP_RACK_REORD_FADE),		/* int */
	tcp_entry(TCP_RACK_TLP_THRESH),		/* int */
	tcp_entry(TCP_RACK_PKT_DELAY),		/* int */
	tcp_entry(TCP_RACK_TLP_USE),		/* int */
	tcp_entry(TCP_RACK_DO_DETECTION),	/* int */
	tcp_entry(TCP_RACK_NONRXT_CFG_RATE),	/* int */
	tcp_entry(TCP_RACK_MBUF_QUEUE),		/* int */
	tcp_entry(TCP_RACK_NO_PUSH_AT_MAX),	/* int */
	tcp_entry(TCP_RACK_PACE_TO_FILL),	/* int */
	tcp_entry(TCP_RACK_PROFILE),		/* int */
	tcp_entry(TCP_RACK_ABC_VAL),		/* int */
	tcp_entry(TCP_RACK_MEASURE_CNT),	/* int */
	tcp_entry(TCP_RACK_DSACK_OPT),		/* int */
	tcp_entry(TCP_RACK_PACING_BETA),	/* int */
	tcp_entry(TCP_RACK_PACING_BETA_ECN),	/* int */
	tcp_entry(TCP_RACK_TIMER_SLOP),		/* int */
	tcp_entry(TCP_RACK_ENABLE_HYSTART),	/* int */
	tcp_entry(TCP_BBR_RACK_RTT_USE),	/* int */
	tcp_entry(TCP_BBR_USE_RACK_RR),		/* int */
	tcp_entry(TCP_BBR_HDWR_PACE),		/* int */
	tcp_entry(TCP_BBR_RACK_INIT_RATE),	/* int */
	tcp_entry(TCP_BBR_IWINTSO),		/* int */
	tcp_entry(TCP_BBR_ALGORITHM),		/* int */
	tcp_entry(TCP_BBR_TSLIMITS),		/* int */
	tcp_entry(TCP_BBR_RECFORCE),		/* int */
	tcp_entry(TCP_BBR_STARTUP_PG),		/* int */
	tcp_entry(TCP_BBR_DRAIN_PG),		/* int */
	tcp_entry(TCP_BBR_RWND_IS_APP),		/* int */
	tcp_entry(TCP_BBR_PROBE_RTT_INT),	/* int */
	tcp_entry(TCP_BBR_PROBE_RTT_GAIN),	/* int */
	tcp_entry(TCP_BBR_PROBE_RTT_LEN),	/* int */
	tcp_entry(TCP_BBR_STARTUP_LOSS_EXIT),	/* int */
	tcp_entry(TCP_BBR_USEDEL_RATE),		/* int */
	tcp_entry(TCP_BBR_MIN_RTO),		/* int */
	tcp_entry(TCP_BBR_MAX_RTO),		/* int */
	tcp_entry(TCP_BBR_PACE_PER_SEC),	/* int */
	tcp_entry(TCP_BBR_PACE_DEL_TAR),	/* int */
	tcp_entry(TCP_BBR_SEND_IWND_IN_TSO),	/* int */
	tcp_entry(TCP_BBR_EXTRA_STATE),		/* int */
	tcp_entry(TCP_BBR_UTTER_MAX_TSO),	/* int */
	tcp_entry(TCP_BBR_MIN_TOPACEOUT),	/* int */
	tcp_entry(TCP_BBR_FLOOR_MIN_TSO),	/* int */
	tcp_entry(TCP_BBR_TSTMP_RAISES),	/* int */
	tcp_entry(TCP_BBR_POLICER_DETECT),	/* int */
	tcp_entry(TCP_BBR_USE_RACK_CHEAT),	/* int */
	tcp_entry(TCP_BBR_PACE_SEG_MAX),	/* int */
	tcp_entry(TCP_BBR_PACE_SEG_MIN),	/* int */
	tcp_entry(TCP_BBR_PACE_CROSS),		/* int */
	tcp_entry(TCP_BBR_PACE_OH),		/* int */
	tcp_entry(TCP_BBR_TMR_PACE_OH),		/* int */
	tcp_entry(TCP_BBR_RETRAN_WTSO),		/* int */
	{0, 0, NULL}
};

static struct sockopt_parameters *
create_parameters(char *level_str, char *optname_str, char *optval_str,
    size_t *optlen)
{
	long long arg;
	int i, level, optname, optval_int;
	struct sockopt_parameters *params;
	char *end;
	bool optval_is_int;

	/* Determine level, use IPPROTO_TCP as default. */
	if (level_str == NULL)
		level = IPPROTO_TCP;
	else {
		arg = strtoll(level_str, &end, 0);
		if (*end != '\0') {
			for (i = 0; so_levels[i].name != NULL; i++)
				if (strcmp(level_str, so_levels[i].name) == 0) {
					level = so_levels[i].level;
					break;
				}
			if (so_levels[i].name == NULL)
				errx(1, "unsupported level %s", optname_str);
		} else {
			if (arg < 0)
				errx(1, "level negative %s", optname_str);
			else if (arg > INT_MAX)
				errx(1, "level too large %s", optname_str);
			else
				level = (int)arg;
		}
	}
	/* Determine option name. */
	if (optname_str == NULL || *optname_str == '\0')
		return (NULL);
	arg = strtoll(optname_str, &end, 0);
	if (*end != '\0') {
		for (i = 0; so_names[i].name != NULL; i++)
			if (strcmp(optname_str, so_names[i].name) == 0) {
				level = so_names[i].level;
				optname = so_names[i].value;
				break;
			}
		if (so_names[i].name == NULL)
			errx(1, "unsupported option name %s", optname_str);
	} else {
		if (arg < 0)
			errx(1, "option name negative %s", optname_str);
		else if (arg > INT_MAX)
			errx(1, "option name too large %s", optname_str);
		else
			optname = (int)arg;
	}
	/*
	 * Determine option value. Use int, if can be parsed as an int,
	 * else use a char *.
	 */
	if (optval_str == NULL || *optval_str == '\0')
		return (NULL);
	arg = strtol(optval_str, &end, 0);
	optval_is_int = (*end == '\0');
	if (optval_is_int) {
		if (arg < INT_MIN)
			errx(1, "option value too small %s", optval_str);
		else if (arg > INT_MAX)
			errx(1, "option value too large %s", optval_str);
		else
			optval_int = (int)arg;
	}
	switch (optname) {
	case TCP_FUNCTION_BLK:
		*optlen = sizeof(struct tcp_function_set);
		break;
	default:
		if (optval_is_int)
			*optlen = sizeof(int);
		else
			*optlen = strlen(optval_str) + 1;
		break;
	}
	/* Fill socket option parameters. */
	params = malloc(sizeof(struct sockopt_parameters) + *optlen);
	if (params == NULL)
		return (NULL);
	memset(params, 0, sizeof(struct sockopt_parameters) + *optlen);
	params->sop_level = level;
	params->sop_optname = optname;
	switch (optname) {
	case TCP_FUNCTION_BLK:
		strlcpy(params->sop_optval, optval_str,
		    TCP_FUNCTION_NAME_LEN_MAX);
		break;
	default:
		if (optval_is_int)
			memcpy(params->sop_optval, &optval_int, *optlen);
		else
			memcpy(params->sop_optval, optval_str, *optlen);
	}
	return (params);
}

static void
usage(void)
{
	fprintf(stderr,
"usage: tcpsso -i id [level] opt-name opt-value\n"
"       tcpsso -a [level] opt-name opt-value\n"
"       tcpsso -C cc-algo [-S stack] [-s state] [level] opt-name opt-value\n"
"       tcpsso [-C cc-algo] -S stack [-s state] [level] opt-name opt-value\n"
"       tcpsso [-C cc-algo] [-S stack] -s state [level] opt-name opt-value\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct sockopt_parameters *params;
	uint64_t id;
	size_t optlen;
	int ch, state;
	char stack[TCP_FUNCTION_NAME_LEN_MAX];
	char ca_name[TCP_CA_NAME_MAX];
	bool ok, apply_all, apply_subset, apply_specific;

	apply_all = false;
	apply_subset = false;
	apply_specific = false;
	ca_name[0] = '\0';
	stack[0] = '\0';
	state = -1;
	id = 0;

	while ((ch = getopt(argc, argv, "aC:i:S:s:")) != -1) {
		switch (ch) {
		case 'a':
			apply_all = true;
			break;
		case 'C':
			apply_subset = true;
			strlcpy(ca_name, optarg, sizeof(ca_name));
			break;
		case 'i':
			apply_specific = true;
			id = strtoull(optarg, NULL, 0);
			break;
		case 'S':
			apply_subset = true;
			strlcpy(stack, optarg, sizeof(stack));
			break;
		case 's':
			apply_subset = true;
			for (state = 0; state < TCP_NSTATES; state++) {
				if (strcmp(tcpstates[state], optarg) == 0)
					break;
			}
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if ((state == TCP_NSTATES) ||
	    (state == TCPS_TIME_WAIT) ||
	    (argc < 2) || (argc > 3) ||
	    (apply_all && apply_subset) ||
	    (apply_all && apply_specific) ||
	    (apply_subset && apply_specific) ||
	    !(apply_all || apply_subset || apply_specific))
		usage();
	if (argc == 2)
		params = create_parameters(NULL, argv[0], argv[1], &optlen);
	else
		params = create_parameters(argv[0], argv[1], argv[2], &optlen);
	if (params != NULL) {
		if (apply_specific)
			ok = tcpsso(id, params, optlen);
		else
			ok = tcpssoall(ca_name, stack, state, params, optlen);
		free(params);
	} else
		ok = false;
	return (ok ? 0 : 1);
}
