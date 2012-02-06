/*-
 * Copyright (c) 2011 Chelsio Communications, Inc.
 * All rights reserved.
 * Written by: Navdeep Parhar <np@FreeBSD.org>
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <err.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "t4_ioctl.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static const char *progname, *nexus;

struct reg_info {
	const char *name;
	uint32_t addr;
	uint32_t len;
};

struct mod_regs {
	const char *name;
	const struct reg_info *ri;
};

#include "reg_defs_t4.c"
#include "reg_defs_t4vf.c"

static void
usage(FILE *fp)
{
	fprintf(fp, "Usage: %s <nexus> [operation]\n", progname);
	fprintf(fp,
	    "\tfilter <idx> [<param> <val>] ...    set a filter\n"
	    "\tfilter <idx> delete|clear           delete a filter\n"
	    "\tfilter list                         list all filters\n"
	    "\tfilter mode [<match>] ...           get/set global filter mode\n"
	    "\treg <address>[=<val>]               read/write register\n"
	    "\treg64 <address>[=<val>]             read/write 64 bit register\n"
	    "\tregdump [<module>] ...              dump registers\n"
	    "\tstdio                               interactive mode\n"
	    );
}

static inline unsigned int
get_card_vers(unsigned int version)
{
	return (version & 0x3ff);
}

static int
real_doit(unsigned long cmd, void *data, const char *cmdstr)
{
	static int fd = -1;
	int rc = 0;

	if (fd == -1) {
		char buf[64];

		snprintf(buf, sizeof(buf), "/dev/%s", nexus);
		if ((fd = open(buf, O_RDWR)) < 0) {
			warn("open(%s)", nexus);
			rc = errno;
			return (rc);
		}
	}

	rc = ioctl(fd, cmd, data);
	if (rc < 0) {
		warn("%s", cmdstr);
		rc = errno;
	}

	return (rc);
}
#define doit(x, y) real_doit(x, y, #x)

static char *
str_to_number(const char *s, long *val, long long *vall)
{
	char *p;

	if (vall)
		*vall = strtoll(s, &p, 0);
	else if (val)
		*val = strtol(s, &p, 0);
	else
		p = NULL;

	return (p);
}

static int
read_reg(long addr, int size, long long *val)
{
	struct t4_reg reg;
	int rc;

	reg.addr = (uint32_t) addr;
	reg.size = (uint32_t) size;
	reg.val = 0;

	rc = doit(CHELSIO_T4_GETREG, &reg);

	*val = reg.val;

	return (rc);
}

static int
write_reg(long addr, int size, long long val)
{
	struct t4_reg reg;

	reg.addr = (uint32_t) addr;
	reg.size = (uint32_t) size;
	reg.val = (uint64_t) val;

	return doit(CHELSIO_T4_SETREG, &reg);
}

static int
register_io(int argc, const char *argv[], int size)
{
	char *p, *v;
	long addr;
	long long val;
	int w = 0, rc;

	if (argc == 1) {
		/* <reg> OR <reg>=<value> */

		p = str_to_number(argv[0], &addr, NULL);
		if (*p) {
			if (*p != '=') {
				warnx("invalid register \"%s\"", argv[0]);
				return (EINVAL);
			}

			w = 1;
			v = p + 1;
			p = str_to_number(v, NULL, &val);

			if (*p) {
				warnx("invalid value \"%s\"", v);
				return (EINVAL);
			}
		}

	} else if (argc == 2) {
		/* <reg> <value> */

		w = 1;

		p = str_to_number(argv[0], &addr, NULL);
		if (*p) {
			warnx("invalid register \"%s\"", argv[0]);
			return (EINVAL);
		}

		p = str_to_number(argv[1], NULL, &val);
		if (*p) {
			warnx("invalid value \"%s\"", argv[1]);
			return (EINVAL);
		}
	} else {
		warnx("reg: invalid number of arguments (%d)", argc);
		return (EINVAL);
	}

	if (w)
		rc = write_reg(addr, size, val);
	else {
		rc = read_reg(addr, size, &val);
		if (rc == 0)
			printf("0x%llx [%llu]\n", val, val);
	}

	return (rc);
}

static inline uint32_t
xtract(uint32_t val, int shift, int len)
{
	return (val >> shift) & ((1 << len) - 1);
}

static int
dump_block_regs(const struct reg_info *reg_array, const uint32_t *regs)
{
	uint32_t reg_val = 0;

	for ( ; reg_array->name; ++reg_array)
		if (!reg_array->len) {
			reg_val = regs[reg_array->addr / 4];
			printf("[%#7x] %-47s %#-10x %u\n", reg_array->addr,
			       reg_array->name, reg_val, reg_val);
		} else {
			uint32_t v = xtract(reg_val, reg_array->addr,
					    reg_array->len);

			printf("    %*u:%u %-47s %#-10x %u\n",
			       reg_array->addr < 10 ? 3 : 2,
			       reg_array->addr + reg_array->len - 1,
			       reg_array->addr, reg_array->name, v, v);
		}

	return (1);
}

static int
dump_regs_table(int argc, const char *argv[], const uint32_t *regs,
    const struct mod_regs *modtab, int nmodules)
{
	int i, j, match;

	for (i = 0; i < argc; i++) {
		for (j = 0; j < nmodules; j++) {
			if (!strcmp(argv[i], modtab[j].name))
				break;
		}

		if (j == nmodules) {
			warnx("invalid register block \"%s\"", argv[i]);
			fprintf(stderr, "\nAvailable blocks:");
			for ( ; nmodules; nmodules--, modtab++)
				fprintf(stderr, " %s", modtab->name);
			fprintf(stderr, "\n");
			return (EINVAL);
		}
	}

	for ( ; nmodules; nmodules--, modtab++) {

		match = argc == 0 ? 1 : 0;
		for (i = 0; !match && i < argc; i++) {
			if (!strcmp(argv[i], modtab->name))
				match = 1;
		}

		if (match)
			dump_block_regs(modtab->ri, regs);
	}

	return (0);
}

#define T4_MODREGS(name) { #name, t4_##name##_regs }
static int
dump_regs_t4(int argc, const char *argv[], const uint32_t *regs)
{
	static struct mod_regs t4_mod[] = {
		T4_MODREGS(sge),
		{ "pci", t4_pcie_regs },
		T4_MODREGS(dbg),
		T4_MODREGS(mc),
		T4_MODREGS(ma),
		{ "edc0", t4_edc_0_regs },
		{ "edc1", t4_edc_1_regs },
		T4_MODREGS(cim), 
		T4_MODREGS(tp),
		T4_MODREGS(ulp_rx),
		T4_MODREGS(ulp_tx),
		{ "pmrx", t4_pm_rx_regs },
		{ "pmtx", t4_pm_tx_regs },
		T4_MODREGS(mps),
		{ "cplsw", t4_cpl_switch_regs },
		T4_MODREGS(smb),
		{ "i2c", t4_i2cm_regs },
		T4_MODREGS(mi),
		T4_MODREGS(uart),
		T4_MODREGS(pmu), 
		T4_MODREGS(sf),
		T4_MODREGS(pl),
		T4_MODREGS(le),
		T4_MODREGS(ncsi),
		T4_MODREGS(xgmac)
	};

	return dump_regs_table(argc, argv, regs, t4_mod, ARRAY_SIZE(t4_mod));
}
#undef T4_MODREGS

static int
dump_regs_t4vf(int argc, const char *argv[], const uint32_t *regs)
{
	static struct mod_regs t4vf_mod[] = {
		{ "sge", t4vf_sge_regs },
		{ "mps", t4vf_mps_regs },
		{ "pl", t4vf_pl_regs },
		{ "mbdata", t4vf_mbdata_regs },
		{ "cim", t4vf_cim_regs },
	};

	return dump_regs_table(argc, argv, regs, t4vf_mod,
	    ARRAY_SIZE(t4vf_mod));
}

static int
dump_regs(int argc, const char *argv[])
{
	int vers, revision, is_pcie, rc;
	struct t4_regdump regs;

	regs.data = calloc(1, T4_REGDUMP_SIZE);
	if (regs.data == NULL) {
		warnc(ENOMEM, "regdump");
		return (ENOMEM);
	}

	regs.len = T4_REGDUMP_SIZE;
	rc = doit(CHELSIO_T4_REGDUMP, &regs);
	if (rc != 0)
		return (rc);

	vers = get_card_vers(regs.version);
	revision = (regs.version >> 10) & 0x3f;
	is_pcie = (regs.version & 0x80000000) != 0;

	if (vers == 4) {
		if (revision == 0x3f)
			rc = dump_regs_t4vf(argc, argv, regs.data);
		else
			rc = dump_regs_t4(argc, argv, regs.data);
	} else {
		warnx("%s (type %d, rev %d) is not a T4 card.",
		    nexus, vers, revision);
		return (ENOTSUP);
	}

	free(regs.data);
	return (rc);
}

static void
do_show_info_header(uint32_t mode)
{
	uint32_t i;

	printf ("%4s %8s", "Idx", "Hits");
	for (i = T4_FILTER_FCoE; i <= T4_FILTER_IP_FRAGMENT; i <<= 1) {
		switch (mode & i) {
		case T4_FILTER_FCoE:
			printf (" FCoE");
			break;

		case T4_FILTER_PORT:
			printf (" Port");
			break;

		case T4_FILTER_OVLAN:
			printf ("     vld:oVLAN");
			break;

		case T4_FILTER_IVLAN:
			printf ("     vld:iVLAN");
			break;

		case T4_FILTER_IP_TOS:
			printf ("   TOS");
			break;

		case T4_FILTER_IP_PROTO:
			printf ("  Prot");
			break;

		case T4_FILTER_ETH_TYPE:
			printf ("   EthType");
			break;

		case T4_FILTER_MAC_IDX:
			printf ("  MACIdx");
			break;

		case T4_FILTER_MPS_HIT_TYPE:
			printf (" MPS");
			break;

		case T4_FILTER_IP_FRAGMENT:
			printf (" Frag");
			break;

		default:
			/* compressed filter field not enabled */
			break;
		}
	}
	printf(" %20s %20s %9s %9s %s\n",
	    "DIP", "SIP", "DPORT", "SPORT", "Action");
}

/*
 * Parse an argument sub-vector as a { <parameter name> <value>[:<mask>] }
 * ordered tuple.  If the parameter name in the argument sub-vector does not
 * match the passed in parameter name, then a zero is returned for the
 * function and no parsing is performed.  If there is a match, then the value
 * and optional mask are parsed and returned in the provided return value
 * pointers.  If no optional mask is specified, then a default mask of all 1s
 * will be returned.
 *
 * An error in parsing the value[:mask] will result in an error message and
 * program termination.
 */
static int
parse_val_mask(const char *param, const char *args[], uint32_t *val,
    uint32_t *mask)
{
	char *p;

	if (strcmp(param, args[0]) != 0)
		return (EINVAL);

	*val = strtoul(args[1], &p, 0);
	if (p > args[1]) {
		if (p[0] == 0) {
			*mask = ~0;
			return (0);
		}

		if (p[0] == ':' && p[1] != 0) {
			*mask = strtoul(p+1, &p, 0);
			if (p[0] == 0)
				return (0);
		}
	}

	warnx("parameter \"%s\" has bad \"value[:mask]\" %s",
	    args[0], args[1]);

	return (EINVAL);
}

/*
 * Parse an argument sub-vector as a { <parameter name> <addr>[/<mask>] }
 * ordered tuple.  If the parameter name in the argument sub-vector does not
 * match the passed in parameter name, then a zero is returned for the
 * function and no parsing is performed.  If there is a match, then the value
 * and optional mask are parsed and returned in the provided return value
 * pointers.  If no optional mask is specified, then a default mask of all 1s
 * will be returned.
 *
 * The value return parameter "afp" is used to specify the expected address
 * family -- IPv4 or IPv6 -- of the address[/mask] and return its actual
 * format.  A passed in value of AF_UNSPEC indicates that either IPv4 or IPv6
 * is acceptable; AF_INET means that only IPv4 addresses are acceptable; and
 * AF_INET6 means that only IPv6 are acceptable.  AF_INET is returned for IPv4
 * and AF_INET6 for IPv6 addresses, respectively.  IPv4 address/mask pairs are
 * returned in the first four bytes of the address and mask return values with
 * the address A.B.C.D returned with { A, B, C, D } returned in addresses { 0,
 * 1, 2, 3}, respectively.
 *
 * An error in parsing the value[:mask] will result in an error message and
 * program termination.
 */
static int
parse_ipaddr(const char *param, const char *args[], int *afp, uint8_t addr[],
    uint8_t mask[])
{
	const char *colon, *afn;
	char *slash;
	uint8_t *m;
	int af, ret;
	unsigned int masksize;

	/*
	 * Is this our parameter?
	 */
	if (strcmp(param, args[0]) != 0)
		return (EINVAL);

	/*
	 * Fundamental IPv4 versus IPv6 selection.
	 */
	colon = strchr(args[1], ':');
	if (!colon) {
		afn = "IPv4";
		af = AF_INET;
		masksize = 32;
	} else {
		afn = "IPv6";
		af = AF_INET6;
		masksize = 128;
	}
	if (*afp == AF_UNSPEC)
		*afp = af;
	else if (*afp != af) {
		warnx("address %s is not of expected family %s",
		    args[1], *afp == AF_INET ? "IP" : "IPv6");
		return (EINVAL);
	}

	/*
	 * Parse address (temporarily stripping off any "/mask"
	 * specification).
	 */
	slash = strchr(args[1], '/');
	if (slash)
		*slash = 0;
	ret = inet_pton(af, args[1], addr);
	if (slash)
		*slash = '/';
	if (ret <= 0) {
		warnx("Cannot parse %s %s address %s", param, afn, args[1]);
		return (EINVAL);
	}

	/*
	 * Parse optional mask specification.
	 */
	if (slash) {
		char *p;
		unsigned int prefix = strtoul(slash + 1, &p, 10);

		if (p == slash + 1) {
			warnx("missing address prefix for %s", param);
			return (EINVAL);
		}
		if (*p) {
			warnx("%s is not a valid address prefix", slash + 1);
			return (EINVAL);
		}
		if (prefix > masksize) {
			warnx("prefix %u is too long for an %s address",
			     prefix, afn);
			return (EINVAL);
		}
		memset(mask, 0, masksize / 8);
		masksize = prefix;
	}

	/*
	 * Fill in mask.
	 */
	for (m = mask; masksize >= 8; m++, masksize -= 8)
		*m = ~0;
	if (masksize)
		*m = ~0 << (8 - masksize);

	return (0);
}

/*
 * Parse an argument sub-vector as a { <parameter name> <value> } ordered
 * tuple.  If the parameter name in the argument sub-vector does not match the
 * passed in parameter name, then a zero is returned for the function and no
 * parsing is performed.  If there is a match, then the value is parsed and
 * returned in the provided return value pointer.
 */
static int
parse_val(const char *param, const char *args[], uint32_t *val)
{
	char *p;

	if (strcmp(param, args[0]) != 0)
		return (EINVAL);

	*val = strtoul(args[1], &p, 0);
	if (p > args[1] && p[0] == 0)
		return (0);

	warnx("parameter \"%s\" has bad \"value\" %s", args[0], args[1]);
	return (EINVAL);
}

static void
filters_show_ipaddr(int type, uint8_t *addr, uint8_t *addrm)
{
	int noctets, octet;

	printf(" ");
	if (type == 0) {
		noctets = 4;
		printf("%3s", " ");
	} else
	noctets = 16;

	for (octet = 0; octet < noctets; octet++)
		printf("%02x", addr[octet]);
	printf("/");
	for (octet = 0; octet < noctets; octet++)
		printf("%02x", addrm[octet]);
}

static void
do_show_one_filter_info(struct t4_filter *t, uint32_t mode)
{
	uint32_t i;

	printf("%4d", t->idx);
	if (t->hits == UINT64_MAX)
		printf(" %8s", "-");
	else
		printf(" %8ju", t->hits);

	/*
	 * Compressed header portion of filter.
	 */
	for (i = T4_FILTER_FCoE; i <= T4_FILTER_IP_FRAGMENT; i <<= 1) {
		switch (mode & i) {
		case T4_FILTER_FCoE:
			printf("  %1d/%1d", t->fs.val.fcoe, t->fs.mask.fcoe);
			break;

		case T4_FILTER_PORT:
			printf("  %1d/%1d", t->fs.val.iport, t->fs.mask.iport);
			break;

		case T4_FILTER_OVLAN:
			printf(" %1d:%1x:%02x/%1d:%1x:%02x",
			    t->fs.val.ovlan_vld, (t->fs.val.ovlan >> 7) & 0x7,
			    t->fs.val.ovlan & 0x7f, t->fs.mask.ovlan_vld,
			    (t->fs.mask.ovlan >> 7) & 0x7,
			    t->fs.mask.ovlan & 0x7f);
			break;

		case T4_FILTER_IVLAN:
			printf(" %1d:%04x/%1d:%04x",
			    t->fs.val.ivlan_vld, t->fs.val.ivlan,
			    t->fs.mask.ivlan_vld, t->fs.mask.ivlan);
			break;

		case T4_FILTER_IP_TOS:
			printf(" %02x/%02x", t->fs.val.tos, t->fs.mask.tos);
			break;

		case T4_FILTER_IP_PROTO:
			printf(" %02x/%02x", t->fs.val.proto, t->fs.mask.proto);
			break;

		case T4_FILTER_ETH_TYPE:
			printf(" %04x/%04x", t->fs.val.ethtype,
			    t->fs.mask.ethtype);
			break;

		case T4_FILTER_MAC_IDX:
			printf(" %03x/%03x", t->fs.val.macidx,
			    t->fs.mask.macidx);
			break;

		case T4_FILTER_MPS_HIT_TYPE:
			printf(" %1x/%1x", t->fs.val.matchtype,
			    t->fs.mask.matchtype);
			break;

		case T4_FILTER_IP_FRAGMENT:
			printf("  %1d/%1d", t->fs.val.frag, t->fs.mask.frag);
			break;

		default:
			/* compressed filter field not enabled */
			break;
		}
	}

	/*
	 * Fixed portion of filter.
	 */
	filters_show_ipaddr(t->fs.type, t->fs.val.dip, t->fs.mask.dip);
	filters_show_ipaddr(t->fs.type, t->fs.val.sip, t->fs.mask.sip);
	printf(" %04x/%04x %04x/%04x",
		 t->fs.val.dport, t->fs.mask.dport,
		 t->fs.val.sport, t->fs.mask.sport);

	/*
	 * Variable length filter action.
	 */
	if (t->fs.action == FILTER_DROP)
		printf(" Drop");
	else if (t->fs.action == FILTER_SWITCH) {
		printf(" Switch: port=%d", t->fs.eport);
	if (t->fs.newdmac)
		printf(
			", dmac=%02x:%02x:%02x:%02x:%02x:%02x "
			", l2tidx=%d",
			t->fs.dmac[0], t->fs.dmac[1],
			t->fs.dmac[2], t->fs.dmac[3],
			t->fs.dmac[4], t->fs.dmac[5],
			t->l2tidx);
	if (t->fs.newsmac)
		printf(
			", smac=%02x:%02x:%02x:%02x:%02x:%02x "
			", smtidx=%d",
			t->fs.smac[0], t->fs.smac[1],
			t->fs.smac[2], t->fs.smac[3],
			t->fs.smac[4], t->fs.smac[5],
			t->smtidx);
	if (t->fs.newvlan == VLAN_REMOVE)
		printf(", vlan=none");
	else if (t->fs.newvlan == VLAN_INSERT)
		printf(", vlan=insert(%x)", t->fs.vlan);
	else if (t->fs.newvlan == VLAN_REWRITE)
		printf(", vlan=rewrite(%x)", t->fs.vlan);
	} else {
		printf(" Pass: Q=");
		if (t->fs.dirsteer == 0) {
			printf("RSS");
			if (t->fs.maskhash)
				printf("(TCB=hash)");
		} else {
			printf("%d", t->fs.iq);
			if (t->fs.dirsteerhash == 0)
				printf("(QID)");
			else
				printf("(hash)");
		}
	}
	if (t->fs.prio)
		printf(" Prio");
	if (t->fs.rpttid)
		printf(" RptTID");
	printf("\n");
}

static int
show_filters(void)
{
	uint32_t mode = 0, header = 0;
	struct t4_filter t;
	int rc;

	/* Get the global filter mode first */
	rc = doit(CHELSIO_T4_GET_FILTER_MODE, &mode);
	if (rc != 0)
		return (rc);

	t.idx = 0;
	for (t.idx = 0; ; t.idx++) {
		rc = doit(CHELSIO_T4_GET_FILTER, &t);
		if (rc != 0 || t.idx == 0xffffffff)
			break;

		if (!header) {
			do_show_info_header(mode);
			header = 1;
		}
		do_show_one_filter_info(&t, mode);
	};

	return (rc);
}

static int
get_filter_mode(void)
{
	uint32_t mode = 0;
	int rc;

	rc = doit(CHELSIO_T4_GET_FILTER_MODE, &mode);
	if (rc != 0)
		return (rc);

	if (mode & T4_FILTER_IPv4)
		printf("ipv4 ");

	if (mode & T4_FILTER_IPv6)
		printf("ipv6 ");

	if (mode & T4_FILTER_IP_SADDR)
		printf("sip ");
	
	if (mode & T4_FILTER_IP_DADDR)
		printf("dip ");

	if (mode & T4_FILTER_IP_SPORT)
		printf("sport ");

	if (mode & T4_FILTER_IP_DPORT)
		printf("dport ");

	if (mode & T4_FILTER_MPS_HIT_TYPE)
		printf("matchtype ");

	if (mode & T4_FILTER_MAC_IDX)
		printf("macidx ");

	if (mode & T4_FILTER_ETH_TYPE)
		printf("ethtype ");

	if (mode & T4_FILTER_IP_PROTO)
		printf("proto ");

	if (mode & T4_FILTER_IP_TOS)
		printf("tos ");

	if (mode & T4_FILTER_IVLAN)
		printf("ivlan ");

	if (mode & T4_FILTER_OVLAN)
		printf("ovlan ");

	if (mode & T4_FILTER_PORT)
		printf("iport ");

	if (mode & T4_FILTER_FCoE)
		printf("fcoe ");

	printf("\n");

	return (0);
}

static int
set_filter_mode(int argc, const char *argv[])
{
	uint32_t mode = 0;

	for (; argc; argc--, argv++) {
		if (!strcmp(argv[0], "matchtype"))
			mode |= T4_FILTER_MPS_HIT_TYPE;

		if (!strcmp(argv[0], "macidx"))
			mode |= T4_FILTER_MAC_IDX;

		if (!strcmp(argv[0], "ethtype"))
			mode |= T4_FILTER_ETH_TYPE;

		if (!strcmp(argv[0], "proto"))
			mode |= T4_FILTER_IP_PROTO;

		if (!strcmp(argv[0], "tos"))
			mode |= T4_FILTER_IP_TOS;

		if (!strcmp(argv[0], "ivlan"))
			mode |= T4_FILTER_IVLAN;

		if (!strcmp(argv[0], "ovlan"))
			mode |= T4_FILTER_OVLAN;

		if (!strcmp(argv[0], "iport"))
			mode |= T4_FILTER_PORT;

		if (!strcmp(argv[0], "fcoe"))
			mode |= T4_FILTER_FCoE;
	}

	return doit(CHELSIO_T4_SET_FILTER_MODE, &mode);
}

static int
del_filter(uint32_t idx)
{
	struct t4_filter t;

	t.idx = idx;

	return doit(CHELSIO_T4_DEL_FILTER, &t);
}

static int
set_filter(uint32_t idx, int argc, const char *argv[])
{
	int af = AF_UNSPEC, start_arg = 0;
	struct t4_filter t;

	if (argc < 2) {
		warnc(EINVAL, "%s", __func__);
		return (EINVAL);
	};
	bzero(&t, sizeof (t));
	t.idx = idx;

	for (start_arg = 0; start_arg + 2 <= argc; start_arg += 2) {
		const char **args = &argv[start_arg];
		uint32_t val, mask;

		if (!strcmp(argv[start_arg], "type")) {
			int newaf;
			if (!strcasecmp(argv[start_arg + 1], "ipv4"))
				newaf = AF_INET;
			else if (!strcasecmp(argv[start_arg + 1], "ipv6"))
				newaf = AF_INET6;
			else {
				warnx("invalid type \"%s\"; "
				    "must be one of \"ipv4\" or \"ipv6\"",
				    argv[start_arg + 1]);
				return (EINVAL);
			}

			if (af != AF_UNSPEC && af != newaf) {
				warnx("conflicting IPv4/IPv6 specifications.");
				return (EINVAL);
			}
			af = newaf;
		} else if (!parse_val_mask("fcoe", args, &val, &mask)) {
			t.fs.val.fcoe = val;
			t.fs.mask.fcoe = mask;
		} else if (!parse_val_mask("iport", args, &val, &mask)) {
			t.fs.val.iport = val;
			t.fs.mask.iport = mask;
		} else if (!parse_val_mask("ovlan", args, &val, &mask)) {
			t.fs.val.ovlan = val;
			t.fs.mask.ovlan = mask;
			t.fs.val.ovlan_vld = 1;
			t.fs.mask.ovlan_vld = 1;
		} else if (!parse_val_mask("ivlan", args, &val, &mask)) {
			t.fs.val.ivlan = val;
			t.fs.mask.ivlan = mask;
			t.fs.val.ivlan_vld = 1;
			t.fs.mask.ivlan_vld = 1;
		} else if (!parse_val_mask("tos", args, &val, &mask)) {
			t.fs.val.tos = val;
			t.fs.mask.tos = mask;
		} else if (!parse_val_mask("proto", args, &val, &mask)) {
			t.fs.val.proto = val;
			t.fs.mask.proto = mask;
		} else if (!parse_val_mask("ethtype", args, &val, &mask)) {
			t.fs.val.ethtype = val;
			t.fs.mask.ethtype = mask;
		} else if (!parse_val_mask("macidx", args, &val, &mask)) {
			t.fs.val.macidx = val;
			t.fs.mask.macidx = mask;
		} else if (!parse_val_mask("matchtype", args, &val, &mask)) {
			t.fs.val.matchtype = val;
			t.fs.mask.matchtype = mask;
		} else if (!parse_val_mask("frag", args, &val, &mask)) {
			t.fs.val.frag = val;
			t.fs.mask.frag = mask;
		} else if (!parse_val_mask("dport", args, &val, &mask)) {
			t.fs.val.dport = val;
			t.fs.mask.dport = mask;
		} else if (!parse_val_mask("sport", args, &val, &mask)) {
			t.fs.val.sport = val;
			t.fs.mask.sport = mask;
		} else if (!parse_ipaddr("dip", args, &af, t.fs.val.dip,
		    t.fs.mask.dip)) {
			/* nada */;
		} else if (!parse_ipaddr("sip", args, &af, t.fs.val.sip,
		    t.fs.mask.sip)) {
			/* nada */;
		} else if (!strcmp(argv[start_arg], "action")) {
			if (!strcmp(argv[start_arg + 1], "pass"))
				t.fs.action = FILTER_PASS;
			else if (!strcmp(argv[start_arg + 1], "drop"))
				t.fs.action = FILTER_DROP;
			else if (!strcmp(argv[start_arg + 1], "switch"))
				t.fs.action = FILTER_SWITCH;
			else {
				warnx("invalid action \"%s\"; must be one of"
				     " \"pass\", \"drop\" or \"switch\"",
				     argv[start_arg + 1]);
				return (EINVAL);
			}
		} else if (!parse_val("hitcnts", args, &val)) {
			t.fs.hitcnts = val;
		} else if (!parse_val("prio", args, &val)) {
			t.fs.prio = val;
		} else if (!parse_val("rpttid", args, &val)) {
			t.fs.rpttid = 1;
		} else if (!parse_val("queue", args, &val)) {
			t.fs.dirsteer = 1;
			t.fs.iq = val;
		} else if (!parse_val("tcbhash", args, &val)) {
			t.fs.maskhash = 1;
			t.fs.dirsteerhash = 1;
		} else if (!parse_val("eport", args, &val)) {
			t.fs.eport = val;
		} else if (!strcmp(argv[start_arg], "dmac")) {
			struct ether_addr *daddr;

			daddr = ether_aton(argv[start_arg + 1]);
			if (daddr == NULL) {
				warnx("invalid dmac address \"%s\"",
				    argv[start_arg + 1]);
				return (EINVAL);
			}
			memcpy(t.fs.dmac, daddr, ETHER_ADDR_LEN);
			t.fs.newdmac = 1;
		} else if (!strcmp(argv[start_arg], "smac")) {
			struct ether_addr *saddr;

			saddr = ether_aton(argv[start_arg + 1]);
			if (saddr == NULL) {
				warnx("invalid smac address \"%s\"",
				    argv[start_arg + 1]);
				return (EINVAL);
			}
			memcpy(t.fs.smac, saddr, ETHER_ADDR_LEN);
			t.fs.newsmac = 1;
		} else if (!strcmp(argv[start_arg], "vlan")) {
			char *p;
			if (!strcmp(argv[start_arg + 1], "none")) {
				t.fs.newvlan = VLAN_REMOVE;
			} else if (argv[start_arg + 1][0] == '=') {
				t.fs.newvlan = VLAN_REWRITE;
			} else if (argv[start_arg + 1][0] == '+') {
				t.fs.newvlan = VLAN_INSERT;
			} else {
				warnx("unknown vlan parameter \"%s\"; must"
				     " be one of \"none\", \"=<vlan>\" or"
				     " \"+<vlan>\"", argv[start_arg + 1]);
				return (EINVAL);
			}
			if (t.fs.newvlan == VLAN_REWRITE ||
			    t.fs.newvlan == VLAN_INSERT) {
				t.fs.vlan = strtoul(argv[start_arg + 1] + 1,
				    &p, 0);
				if (p == argv[start_arg + 1] + 1 || p[0] != 0) {
					warnx("invalid vlan \"%s\"",
					     argv[start_arg + 1]);
					return (EINVAL);
				}
			}
		} else {
			warnx("invalid parameter \"%s\"", argv[start_arg]);
			return (EINVAL);
		}
	}
	if (start_arg != argc) {
		warnx("no value for \"%s\"", argv[start_arg]);
		return (EINVAL);
	}

	/*
	 * Check basic sanity of option combinations.
	 */
	if (t.fs.action != FILTER_SWITCH &&
	    (t.fs.eport || t.fs.newdmac || t.fs.newsmac || t.fs.newvlan)) {
		warnx("prio, port dmac, smac and vlan only make sense with"
		     " \"action switch\"");
		return (EINVAL);
	}
	if (t.fs.action != FILTER_PASS &&
	    (t.fs.rpttid || t.fs.dirsteer || t.fs.maskhash)) {
		warnx("rpttid, queue and tcbhash don't make sense with"
		     " action \"drop\" or \"switch\"");
		return (EINVAL);
	}

	t.fs.type = (af == AF_INET6 ? 1 : 0); /* default IPv4 */
	return doit(CHELSIO_T4_SET_FILTER, &t);
}

static int
filter_cmd(int argc, const char *argv[])
{
	long long val;
	uint32_t idx;
	char *s;

	if (argc == 0) {
		warnx("filter: no arguments.");
		return (EINVAL);
	};

	/* list */
	if (strcmp(argv[0], "list") == 0) {
		if (argc != 1)
			warnx("trailing arguments after \"list\" ignored.");

		return show_filters();
	}

	/* mode */
	if (argc == 1 && strcmp(argv[0], "mode") == 0)
		return get_filter_mode();

	/* mode <mode> */
	if (strcmp(argv[0], "mode") == 0)
		return set_filter_mode(argc - 1, argv + 1);

	/* <idx> ... */
	s = str_to_number(argv[0], NULL, &val);
	if (*s || val > 0xffffffffU) {
		warnx("\"%s\" is neither an index nor a filter subcommand.",
		    argv[0]);
		return (EINVAL);
	}
	idx = (uint32_t) val;

	/* <idx> delete|clear */
	if (argc == 2 &&
	    (strcmp(argv[1], "delete") == 0 || strcmp(argv[1], "clear") == 0)) {
		return del_filter(idx);
	}

	/* <idx> [<param> <val>] ... */
	return set_filter(idx, argc - 1, argv + 1);
}

static int
run_cmd(int argc, const char *argv[])
{
	int rc = -1;
	const char *cmd = argv[0];

	/* command */
	argc--;
	argv++;

	if (!strcmp(cmd, "reg") || !strcmp(cmd, "reg32"))
		rc = register_io(argc, argv, 4);
	else if (!strcmp(cmd, "reg64"))
		rc = register_io(argc, argv, 8);
	else if (!strcmp(cmd, "regdump"))
		rc = dump_regs(argc, argv);
	else if (!strcmp(cmd, "filter"))
		rc = filter_cmd(argc, argv);
	else {
		rc = EINVAL;
		warnx("invalid command \"%s\"", cmd);
	}

	return (rc);
}

#define MAX_ARGS 15
static int
run_cmd_loop(void)
{
	int i, rc = 0;
	char buffer[128], *buf;
	const char *args[MAX_ARGS + 1];

	/*
	 * Simple loop: displays a "> " prompt and processes any input as a
	 * cxgbetool command.  You're supposed to enter only the part after
	 * "cxgbetool t4nexX".  Use "quit" or "exit" to exit.
	 */
	for (;;) {
		fprintf(stdout, "> ");
		fflush(stdout);
		buf = fgets(buffer, sizeof(buffer), stdin);
		if (buf == NULL) {
			if (ferror(stdin)) {
				warn("stdin error");
				rc = errno;	/* errno from fgets */
			}
			break;
		}

		i = 0;
		while ((args[i] = strsep(&buf, " \t\n")) != NULL) {
			if (args[i][0] != 0 && ++i == MAX_ARGS)
				break;
		}
		args[i] = 0;

		if (i == 0)
			continue;	/* skip empty line */

		if (!strcmp(args[0], "quit") || !strcmp(args[0], "exit"))
			break;

		rc = run_cmd(i, args);
	}

	/* rc normally comes from the last command (not including quit/exit) */
	return (rc);
}

int
main(int argc, const char *argv[])
{
	int rc = -1;

	progname = argv[0];

	if (argc == 2) {
		if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
			usage(stdout);
			exit(0);
		}
	}

	if (argc < 3) {
		usage(stderr);
		exit(EINVAL);
	}

	nexus = argv[1];

	/* progname and nexus */
	argc -= 2;
	argv += 2;

	if (argc == 1 && !strcmp(argv[0], "stdio"))
		rc = run_cmd_loop();
	else
		rc = run_cmd(argc, argv);

	return (rc);
}
