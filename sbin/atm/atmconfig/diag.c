/*
 * Copyright (c) 2001-2003
 *	Fraunhofer Institute for Open Communication Systems (FhG Fokus).
 * 	All rights reserved.
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
 *
 * Author: Hartmut Brandt <harti@freebsd.org>
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <net/if.h>
#include <net/if_mib.h>
#include <net/if_var.h>
#include <net/if_types.h>
#include <net/if_atm.h>
#include <net/if_media.h>
#include <netnatm/natm.h>
#include <dev/utopia/utopia.h>
#include <dev/utopia/suni.h>
#include <dev/utopia/idtphy.h>

#include "atmconfig.h"
#include "private.h"
#include "diag.h"

static void diag_list(int, char *[]);
static void diag_config(int, char *[]);
static void diag_vcc(int, char *[]);
static void diag_phy_show(int, char *[]);
static void diag_phy_set(int, char *[]);
static void diag_phy_print(int, char *[]);
static void diag_phy_stats(int, char *[]);
static void diag_stats(int, char *[]);

const struct cmdtab diag_phy_tab[] = {
	{ "show",	NULL, 		diag_phy_show },
	{ "set",	NULL, 		diag_phy_set },
	{ "stats",	NULL,		diag_phy_stats },
	{ "print",	NULL,		diag_phy_print },
	{ NULL,		NULL,		NULL },
};

const struct cmdtab diag_tab[] = {
	{ "list",	NULL,		diag_list },
	{ "config",	NULL,		diag_config },
	{ "phy",	diag_phy_tab,	NULL },
	{ "stats",	NULL,		diag_stats },
	{ "vcc",	NULL,		diag_vcc },
	{ NULL, 	NULL, 		NULL }
};

static const struct utopia_print suni_lite[] = { SUNI_PRINT_LITE };
static const struct utopia_print suni_ultra[] = { SUNI_PRINT_ULTRA };
static const struct utopia_print suni_622[] = { SUNI_PRINT_622 };
static const struct utopia_print idt77105[] = { IDTPHY_PRINT_77105 };
static const struct utopia_print idt77155[] = { IDTPHY_PRINT_77155 };

static const struct {
	const struct utopia_print *tab;
	u_int len;
	u_int type;
} phy_print[] = {
	{ suni_lite, sizeof(suni_lite) / sizeof(suni_lite[0]),
	  UTP_TYPE_SUNI_LITE },
	{ suni_ultra, sizeof(suni_ultra) / sizeof(suni_ultra[0]),
	  UTP_TYPE_SUNI_ULTRA },
	{ suni_622, sizeof(suni_622) / sizeof(suni_622[0]),
	  UTP_TYPE_SUNI_622 },
	{ idt77105, sizeof(idt77105) / sizeof(idt77105[0]),
	  UTP_TYPE_IDT77105 },
	{ idt77155, sizeof(idt77155) / sizeof(idt77155[0]),
	  UTP_TYPE_IDT77155 },
};

static const u_int utopia_addreg[] = { UTP_REG_ADD };

/*
 * Driver statistics printing
 */
static const char *const print_stats_pca200e[] = {
	"cmd_queue_full:",
	"get_stat_errors:",
	"clr_stat_errors:",
	"get_prom_errors:",
	"suni_reg_errors:",
	"tx_queue_full:",
	"tx_queue_almost_full:",
	"tx_pdu2big:",
	"tx_too_many_segs:",
	"tx_retry:",
	"fix_empty:",
	"fix_addr_copy:",
	"fix_addr_noext:",
	"fix_addr_ext:",
	"fix_len_noext:",
	"fix_len_copy:",
	"fix_len:",
	"rx_badvc:",
	"rx_closed:",
	NULL
};
static const char *const print_stats_he[] = {
	"tdprq_full:",
	"hbuf_error:",
	"crc_error:",
	"len_error:",
	"flow_closed:",
	"flow_drop:",
	"tpd_no_mem:",
	"rx_seg:",
	"empty_hbuf:",
	"short_aal5:",
	"badlen_aal5:",
	"bug_bad_isw:",
	"bug_no_irq_upd:",
	"itype_tbrq:",
	"itype_tpd:",
	"itype_rbps:",
	"itype_rbpl:",
	"itype_rbrq:",
	"itype_rbrqt:",
	"itype_unknown:",
	"itype_phys:",
	"itype_err:",
	"defrag:",
	"mcc:",
	"oec:",
	"dcc:",
	"cec:",
	"no_rcv_mbuf:",
	NULL
};
static const char *const print_stats_eni[] = {
	"ttrash:",
	"mfixaddr:",
	"mfixlen:",
	"mfixfail:",
	"txmbovr:",
	"dmaovr:",
	"txoutspace:",
	"txdtqout:",
	"launch:",
	"hwpull:",
	"swadd:",
	"rxqnotus:",
	"rxqus:",
	"rxdrqout:",
	"rxmbufout:",
	"txnomap:",
	"vtrash:",
	"otrash:",
	NULL
};

static const char *const print_stats_idt77211[] = {
	"need_copy:",
	"copy_failed:",
	"out_of_tbds:",
	"no_txmaps:",
	"tx_load_err:",
	"tx_qfull:",
	NULL
};
static const char *const print_stats_idt77252[] = {
	"raw_cells:",
	"raw_no_vcc:",
	"raw_no_buf:",
	"tx_qfull:",
	"tx_out_of_tbds:",
	"tx_out_of_maps:",
	"tx_load_err:",
	NULL
};
static const char *const *const print_stats[] = {
	[ATM_DEVICE_UNKNOWN] =		NULL,
	[ATM_DEVICE_PCA200E] =		print_stats_pca200e,
	[ATM_DEVICE_HE155] =		print_stats_he,
	[ATM_DEVICE_HE622] =		print_stats_he,
	[ATM_DEVICE_ENI155P] =		print_stats_eni,
	[ATM_DEVICE_ADP155P] =		print_stats_eni,
	[ATM_DEVICE_FORELE25] =		print_stats_idt77211,
	[ATM_DEVICE_FORELE155] =	print_stats_idt77211,
	[ATM_DEVICE_NICSTAR25] =	print_stats_idt77211,
	[ATM_DEVICE_NICSTAR155] =	print_stats_idt77211,
	[ATM_DEVICE_IDTABR25] =		print_stats_idt77252,
	[ATM_DEVICE_IDTABR155] =	print_stats_idt77252,
	[ATM_DEVICE_PROATM25] =		print_stats_idt77252,
	[ATM_DEVICE_PROATM155] =	print_stats_idt77252,
};

struct diagif_list diagif_list = TAILQ_HEAD_INITIALIZER(diagif_list);

/*
 * Fetch a phy sysctl
 */
static void
phy_fetch(const char *ifname, const char *var, void *val, size_t len)
{
	char *str;

	if (asprintf(&str, "hw.atm.%s.phy_%s", ifname, var) == -1)
		err(1, NULL);
	if (sysctlbyname(str, val, &len, NULL, NULL) == -1)
		err(1, "%s", str);
	free(str);
}

/*
 * Fetch the list of all ATM network interfaces and their MIBs.
 */
void
diagif_fetch(void)
{
	size_t len;
	int count;
	int name[6];
	struct ifmibdata mib;
	struct ifatm_mib atm;
	int idx;
	struct diagif *d;

	while ((d = TAILQ_FIRST(&diagif_list)) != NULL) {
		if (d->vtab != NULL)
			free(d->vtab);
		TAILQ_REMOVE(&diagif_list, d, link);
		free(d);
	}

	len = sizeof(count);
	if (sysctlbyname("net.link.generic.system.ifcount", &count, &len,
	    NULL, 0) == -1)
		err(1, "ifcount");

	name[0] = CTL_NET;
	name[1] = PF_LINK;
	name[2] = NETLINK_GENERIC;
	name[3] = IFMIB_IFDATA;

	for (idx = 1; idx <= count; idx++) {
		name[4] = idx;
		name[5] = IFDATA_GENERAL;
		len = sizeof(mib);
		if (sysctl(name, 6, &mib, &len, NULL, 0) == -1)
			err(1, "interface %d: general mib", idx);
		if (mib.ifmd_data.ifi_type == IFT_ATM) {
			name[5] = IFDATA_LINKSPECIFIC;
			len = sizeof(atm);
			if (sysctl(name, 6, &atm, &len, NULL, 0) == -1)
				err(1, "interface %d: ATM mib", idx);

			d = malloc(sizeof(*d));
			if (d == NULL)
				err(1, NULL);
			bzero(d, sizeof(*d));
			d->mib = atm;
			d->index = idx;
			strcpy(d->ifname, mib.ifmd_name);
			TAILQ_INSERT_TAIL(&diagif_list, d, link);

			phy_fetch(d->ifname, "loopback", &d->phy_loopback,
			    sizeof(d->phy_loopback));
			phy_fetch(d->ifname, "type", &d->phy_type,
			    sizeof(d->phy_type));
			phy_fetch(d->ifname, "name", &d->phy_name,
			    sizeof(d->phy_name));
			phy_fetch(d->ifname, "state", &d->phy_state,
			    sizeof(d->phy_state));
			phy_fetch(d->ifname, "carrier", &d->phy_carrier,
			    sizeof(d->phy_carrier));
		}
	}
}

/*
 * "<radix><bit>STRING\011<mask><pattern>STRING\012<mask><radix>STRING"
 */
static char *
printb8(uint32_t val, const char *descr)
{
	static char buffer[1000];
	char *ptr;
	int tmp = 0;
	u_char mask, pattern;

	if (*descr++ == '\010')
		sprintf(buffer, "%#o", val);
	else
		sprintf(buffer, "%#x", val);
	ptr = buffer + strlen(buffer);

	*ptr++ = '<';
	while (*descr) {
		if (*descr == '\11') {
			descr++;
			mask = *descr++;
			pattern = *descr++;
			if ((val & mask) == pattern) {
				if (tmp++)
					*ptr++ = ',';
				while (*descr >= ' ')
					*ptr++ = *descr++;
			} else {
				while (*descr >= ' ')
					descr++;
			}
		} else if (*descr == '\12') {
			descr++;
			mask = *descr++;
			pattern = *descr++;
			if (tmp++)
				*ptr++ = ',';
			while (*descr >= ' ')
				*ptr++ = *descr++;
			*ptr++ = '=';
			if (pattern == 8)
				sprintf(ptr, "%#o",
				    (val & mask) >> (ffs(mask)-1));
			else if (pattern == 10)
				sprintf(ptr, "%u",
				    (val & mask) >> (ffs(mask)-1));
			else
				sprintf(ptr, "%#x",
				    (val & mask) >> (ffs(mask)-1));
			ptr += strlen(ptr);
		} else {
			if (val & (1 << (*descr++ - 1))) {
				if (tmp++)
					*ptr++ = ',';
				while (*descr >= ' ')
					*ptr++ = *descr++;
			} else {
				while (*descr >= ' ')
					descr++;
			}
		}
	}
	*ptr++ = '>';
	*ptr++ = '\0';

	return (buffer);
}

/*
 * "<radix><bit>STRING<bit>STRING"
 */
static char *
printb(uint32_t val, const char *descr)
{
	static char buffer[1000];
	char *ptr;
	int tmp = 0;

	if (*descr++ == '\010')
		sprintf(buffer, "%#o", val);
	else
		sprintf(buffer, "%#x", val);
	ptr = buffer + strlen(buffer);

	*ptr++ = '<';
	while (*descr) {
		if (val & (1 << (*descr++ - 1))) {
			if (tmp++)
				*ptr++ = ',';
			while (*descr > ' ')
				*ptr++ = *descr++;
		} else {
			while (*descr > ' ')
				descr++;
		}
	}
	*ptr++ = '>';
	*ptr++ = '\0';

	return (buffer);
}


static void
diag_loop(int argc, char *argv[], const char *text,
    void (*func)(const struct diagif *))
{
	int i;
	struct diagif *aif;

	heading_init();
	if (argc > 0) {
		for (i = 0; i < argc; i++) {
			TAILQ_FOREACH(aif, &diagif_list, link) {
				if (strcmp(argv[i], aif->ifname) == 0) {
					heading(text);
					(*func)(aif);
					break;
				}
			}
			if (aif == NULL)
				warnx("%s: no such ATM interface", argv[i]);
		}
	} else {
		TAILQ_FOREACH(aif, &diagif_list, link) {
			heading(text);
			(*func)(aif);
		}
	}
}

/*
 * Print the config line for the given interface
 */
static void
config_line1(const struct diagif *aif)
{
	printf("%-6u%-9s%-8u%-5u%-6u%-5u%-6u%02x:%02x:%02x:%02x:%02x:%02x\n",
	    aif->index, aif->ifname, aif->mib.pcr, (1 << aif->mib.vpi_bits) - 1,
	    (1 << aif->mib.vci_bits) - 1, aif->mib.max_vpcs, aif->mib.max_vccs,
	    aif->mib.esi[0], aif->mib.esi[1], aif->mib.esi[2],
	    aif->mib.esi[3], aif->mib.esi[4], aif->mib.esi[5]);
}

static void
config_line2(const struct diagif *aif)
{
	u_int d, i;

	static const struct {
		const char *dev;
		const char *vendor;
	} devs[] = {
		ATM_DEVICE_NAMES
	};
	static const struct {
		u_int	media;
		const char *const name;
	} medias[] = IFM_SUBTYPE_ATM_DESCRIPTIONS;

	for (i = 0; medias[i].name; i++)
		if (aif->mib.media == medias[i].media)
			break;

	if ((d = aif->mib.device) >= sizeof(devs) / sizeof(devs[0]))
		d = 0;

	printf("%-6u%-9s%-12.11s%-13.12s%-8u%-6x%-6x %s\n", aif->index,
	    aif->ifname, devs[d].vendor, devs[d].dev, aif->mib.serial,
	    aif->mib.hw_version, aif->mib.sw_version,
	    medias[i].name ? medias[i].name : "unknown");
}

static void
diag_config(int argc, char *argv[])
{
	int opt;

	static int hardware;
	static int atm;

	static const struct option opts[] = {
	    { "hardware", OPT_SIMPLE, &hardware },
	    { "atm", OPT_SIMPLE, &atm },
	    { NULL, 0, NULL }
	};

	static const char config_text1[] =
	    "Interface              Max        Max\n"
	    "Index Name     PCR     VPI  VCI   VPCs VCCs  ESI          Media\n";
	static const char config_text2[] =
	    "Interface                                       Version\n"
	    "Index Name     Vendor      Card         "
	    "Serial  HW    SW     Media\n";

	while ((opt = parse_options(&argc, &argv, opts)) != -1)
		switch (opt) {
		}

	diagif_fetch();
	if (TAILQ_EMPTY(&diagif_list))
		errx(1, "no ATM interfaces found");

	if (!atm && !hardware)
		atm = 1;

	if (atm)
		diag_loop(argc, argv, config_text1, config_line1);
	if (hardware)
		diag_loop(argc, argv, config_text2, config_line2);

}

static void
diag_list(int argc, char *argv[])
{
	int opt;
	struct diagif *aif;

	static const struct option opts[] = {
	    { NULL, 0, NULL }
	};

	while ((opt = parse_options(&argc, &argv, opts)) != -1)
		switch (opt) {
		}

	if (argc > 0)
		errx(1, "no arguments required for 'diag list'");

	diagif_fetch();
	if (TAILQ_EMPTY(&diagif_list))
		errx(1, "no ATM interfaces found");

	TAILQ_FOREACH(aif, &diagif_list, link)
		printf("%s ", aif->ifname);
	printf("\n");
}

/*
 * Print the config line for the given interface
 */
static void
phy_show_line(const struct diagif *aif)
{
	printf("%-6u%-9s%-5u%-25s0x%-9x\n",
	    aif->index, aif->ifname, aif->phy_type, aif->phy_name,
	    aif->phy_loopback);
}

static void
diag_phy_show(int argc, char *argv[])
{
	int opt;

	static const struct option opts[] = {
	    { NULL, 0, NULL }
	};

	static const char phy_show_text[] = 
	    "Interface      Phy\n"
	    "Index Name     Type Name                     Loopback State\n";

	while ((opt = parse_options(&argc, &argv, opts)) != -1)
		switch (opt) {
		}

	diagif_fetch();
	if (TAILQ_EMPTY(&diagif_list))
		errx(1, "no ATM interfaces found");

	diag_loop(argc, argv, phy_show_text, phy_show_line);
}

static void
diag_phy_set(int argc, char *argv[])
{
	int opt;
	uint8_t reg[3];
	u_long res;
	char *end;
	char *str;

	static const struct option opts[] = {
	    { NULL, 0, NULL }
	};

	while ((opt = parse_options(&argc, &argv, opts)) != -1)
		switch (opt) {
		}

	if (argc != 4)
		errx(1, "missing arguments for 'diag phy set'");

	errno = 0;
	res = strtoul(argv[1], &end, 0);
	if (errno != 0)
		err(1, "register number");
	if (*end != '\0')
		errx(1, "malformed register number '%s'", argv[1]);
	if (res > 0xff)
		errx(1, "register number too large");
	reg[0] = res;

	errno = 0;
	res = strtoul(argv[2], &end, 0);
	if (errno != 0)
		err(1, "mask");
	if (*end != '\0')
		errx(1, "malformed mask '%s'", argv[1]);
	if (res > 0xff)
		errx(1, "mask too large");
	reg[1] = res;

	errno = 0;
	res = strtoul(argv[3], &end, 0);
	if (errno != 0)
		err(1, "value");
	if (*end != '\0')
		errx(1, "malformed value '%s'", argv[1]);
	if (res > 0xff)
		errx(1, "value too large");
	reg[2] = res;

	if (asprintf(&str, "hw.atm.%s.phy_regs", argv[0]) == -1)
		err(1, NULL);

	if (sysctlbyname(str, NULL, NULL, reg, 3 * sizeof(uint8_t)))
		err(1, "%s", str);

	free(str);
}

static void
diag_phy_print(int argc, char *argv[])
{
	int opt;
	char *str;
	size_t len, len1;
	uint8_t *regs;
	u_int type, i;
	const struct utopia_print *p;

	static int numeric;

	static const struct option opts[] = {
	    { "numeric", OPT_SIMPLE, &numeric },
	    { NULL, 0, NULL }
	};

	while ((opt = parse_options(&argc, &argv, opts)) != -1)
		switch (opt) {
		}

	if (argc != 1)
		errx(1, "need device name for 'diag phy print'");

	if (asprintf(&str, "hw.atm.%s.phy_regs", argv[0]) == -1)
		err(1, NULL);
	len = 0;
	if (sysctlbyname(str, NULL, &len, NULL, 0))
		err(1, "'%s' not found", str);

	regs = malloc(len);
	if (regs == NULL)
		err(1, NULL);

	if (sysctlbyname(str, regs, &len, NULL, 0))
		err(1, "'%s' not found", str);
	free(str);

	if (numeric) {
		for (i = 0; i < len; i++) {
			if (i % 16 == 0)
				printf("%02x: ", i);
			if (i % 16 == 8)
				printf(" ");
			printf(" %02x", regs[i]);
			if (i % 16 == 15)
				printf("\n");
		}
		if (i % 16 != 0)
			printf("\n");
	} else {
		if (asprintf(&str, "hw.atm.%s.phy_type", argv[0]) == -1)
			err(1, NULL);
		len1 = sizeof(type);
		if (sysctlbyname(str, &type, &len1, NULL, 0))
			err(1, "'%s' not found", str);
		free(str);

		for (i = 0; i < sizeof(phy_print) / sizeof(phy_print[0]); i++)
			if (type == phy_print[i].type)
				break;
		if (i == sizeof(phy_print) / sizeof(phy_print[0]))
			errx(1, "unknown PHY chip type %u\n", type);

		for (p = phy_print[i].tab;
		    p < phy_print[i].tab + phy_print[i].len;
		    p++) {
			if (p->reg + utopia_addreg[p->type] > len)
				/* don't have this register */
				continue;

			printf("%s:%*s", p->name, 40 - (int)strlen(p->name),"");

			switch (p->type) {

			  case UTP_REGT_BITS:
				printf("%s\n", printb8(regs[p->reg], p->fmt));
				break;
			
			  case UTP_REGT_INT8:
				printf("%#x\n", regs[p->reg]);
				break;

			  case UTP_REGT_INT10BITS:
				printf("%#x %s\n", regs[p->reg] |
				    ((regs[p->reg + 1] & 0x3) << 8),
				    printb8(regs[p->reg + 1], p->fmt));
				break;

			  case UTP_REGT_INT12:
				printf("%#x\n", regs[p->reg] |
				    ((regs[p->reg + 1] & 0xf) << 8));
				break;

			  case UTP_REGT_INT16:
				printf("%#x\n", regs[p->reg] |
				    (regs[p->reg + 1] << 8));
				break;

			  case UTP_REGT_INT19:
				printf("%#x\n", regs[p->reg] |
				    (regs[p->reg + 1] << 8) |
				    ((regs[p->reg + 2] & 0x7) << 16));
				break;

			  case UTP_REGT_INT20:
				printf("%#x\n", regs[p->reg] |
				    (regs[p->reg + 1] << 8) |
				    ((regs[p->reg + 2] & 0xf) << 16));
				break;

			  case UTP_REGT_INT21:
				printf("%#x\n", regs[p->reg] |
				    (regs[p->reg + 1] << 8) |
				    ((regs[p->reg + 2] & 0x1f) << 16));
				break;

			  default:
				abort();
			}
		}
	}
	free(regs);
}

static void
diag_phy_stats(int argc, char *argv[])
{
	int opt;
	size_t len;
	char *str;
	struct utopia_stats1 stats1;
	u_int foo;

	static int clear;

	static const struct option opts[] = {
	    { "clear", OPT_SIMPLE, &clear },
	    { NULL, 0, NULL }
	};

	while ((opt = parse_options(&argc, &argv, opts)) != -1)
		switch (opt) {
		}

	if (argc != 1)
		errx(1, "need device name for 'diag phy stats'");

	if (asprintf(&str, "hw.atm.%s.phy_stats", argv[0]) == -1)
		err(1, NULL);

	len = sizeof(stats1);
	if (sysctlbyname(str, &stats1, &len,
	    clear ? &foo : NULL, clear ? sizeof(foo) : 0))
		err(1, "'%s' not found", str);
	if (len < sizeof(stats1.version))
		errx(1, "phy statistics too short %zu", len);

	switch (stats1.version) {

	  case 1:
		if (len != sizeof(stats1))
			errx(1, "bad phy stats length %zu (expecting %zu)",
			    len, sizeof(stats1));
		break;

	  default:
		errx(1, "unknown phy stats version %u", stats1.version);
	}

	free(str);

	printf("rx_sbip:	%llu\n", (unsigned long long)stats1.rx_sbip);
	printf("rx_lbip:	%llu\n", (unsigned long long)stats1.rx_lbip);
	printf("rx_lfebe:	%llu\n", (unsigned long long)stats1.rx_lfebe);
	printf("rx_pbip:	%llu\n", (unsigned long long)stats1.rx_pbip);
	printf("rx_pfebe:	%llu\n", (unsigned long long)stats1.rx_pfebe);
	printf("rx_cells:	%llu\n", (unsigned long long)stats1.rx_cells);
	printf("rx_corr:	%llu\n", (unsigned long long)stats1.rx_corr);
	printf("rx_uncorr:	%llu\n", (unsigned long long)stats1.rx_uncorr);
	printf("rx_symerr:	%llu\n", (unsigned long long)stats1.rx_symerr);
	printf("tx_cells:	%llu\n", (unsigned long long)stats1.tx_cells);
}

/*
 * Fetch the table of open vccs
 */
void
diagif_fetch_vcc(struct diagif *aif, int fd)
{
	struct ifreq ifr;

	if (aif->vtab != NULL)
		return;

	strncpy(ifr.ifr_name, aif->ifname, IFNAMSIZ);
	ifr.ifr_name[IFNAMSIZ] = '\0';

	aif->vtab = malloc(sizeof(*aif->vtab) + sizeof(aif->vtab->vccs[0]) *
	    aif->mib.max_vccs);
	if (aif->vtab == NULL)
		err(1, NULL);
	ifr.ifr_data = (caddr_t)aif->vtab;

	if (ioctl(fd, SIOCATMGVCCS, &ifr) == -1)
		err(1, "SIOCATMGVCCS");
}

/*
 * Print the VCC table for this interface.
 */
static void
print_channel(const struct diagif *aif)
{
	const struct atmio_vcc *v;

	static const char *const aal_tab[] = {
		[ATMIO_AAL_0] "0",
		[ATMIO_AAL_34] "3/4",
		[ATMIO_AAL_5] "5",
		[ATMIO_AAL_RAW] "raw",
	};
	static const char *const traffic_tab[] = {
		[ATMIO_TRAFFIC_UBR] "ubr",
		[ATMIO_TRAFFIC_CBR] "cbr",
		[ATMIO_TRAFFIC_ABR] "abr",
		[ATMIO_TRAFFIC_VBR] "vbr",
	};

	for (v = aif->vtab->vccs; v < &aif->vtab->vccs[aif->vtab->count]; v++) {
		printf("%-6u%-9s%-4u%-6u", aif->index, aif->ifname,
		    v->vpi, v->vci);

		if (v->aal >= sizeof(aal_tab)/sizeof(aal_tab[0]) ||
		    aal_tab[v->aal] == NULL)
			printf("bad ");
		else
			printf("%-4s", aal_tab[v->aal]);

		if (v->traffic >= sizeof(traffic_tab)/sizeof(traffic_tab[0]) ||
		    traffic_tab[v->traffic] == NULL)
			printf("bad     ");
		else
			printf("%-8s", traffic_tab[v->traffic]);

		printf("%-6u%-6u%s\n", v->rmtu, v->tmtu,
		    printb(v->flags, ATMIO_FLAGS));
	}
}

/*
 * Print the VCC table for this interface, traffic parameters.
 */
static void
print_traffic(const struct diagif *aif)
{
	const struct atmio_vcc *v;

	for (v = aif->vtab->vccs; v < &aif->vtab->vccs[aif->vtab->count]; v++) {
		printf("%-6u%-9s%-4u%-6u", aif->index, aif->ifname,
		    v->vpi, v->vci);

		switch (v->traffic) {

		  case ATMIO_TRAFFIC_CBR:
			printf("%u", v->tparam.pcr);
			break;

		  case ATMIO_TRAFFIC_UBR:
			printf("%-8u                %u", v->tparam.pcr,
			    v->tparam.mcr);
			break;

		  case ATMIO_TRAFFIC_VBR:
			printf("%-8u%-8u%-8u", v->tparam.pcr, v->tparam.scr,
			    v->tparam.mbs);
			break;

		  case ATMIO_TRAFFIC_ABR:
			printf("%-8u                %-8u",
			    v->tparam.pcr, v->tparam.mcr);
			break;
		}
		printf("\n");
	}
}

/*
 * Print the VCC table for this interface, ABR traffic parameters.
 */
static void
print_abr(const struct diagif *aif)
{
	const struct atmio_vcc *v;

	for (v = aif->vtab->vccs; v < &aif->vtab->vccs[aif->vtab->count]; v++) {
		printf("%-6u%-9s%-4u%-6u", aif->index, aif->ifname,
		    v->vpi, v->vci);

		if (v->traffic == ATMIO_TRAFFIC_ABR) {
			printf("%-8u%-8u%-4u%-4u%-5u%-5u%-5u%u",
			    v->tparam.icr, v->tparam.tbe, v->tparam.nrm,
			    v->tparam.trm, v->tparam.adtf, v->tparam.rif,
			    v->tparam.rdf, v->tparam.cdf);
		}
		printf("\n");
	}
}

static void
diag_vcc_loop(void (*func)(const struct diagif *), const char *text,
    int argc, char *argv[], int fd)
{
	struct diagif *aif;

	heading_init();
	if (argc == 0) {
		TAILQ_FOREACH(aif, &diagif_list, link) {
			diagif_fetch_vcc(aif, fd);
			if (aif->vtab->count != 0) {
				heading(text);
				(*func)(aif);
			}
		}

	} else {
		for (optind = 0; optind < argc; optind++) {
			TAILQ_FOREACH(aif, &diagif_list, link)
				if (strcmp(aif->ifname, argv[optind]) == 0) {
					diagif_fetch_vcc(aif, fd);
					if (aif->vtab->count != 0) {
						heading(text);
						(*func)(aif);
					}
					break;
				}
			if (aif == NULL)
				warnx("no such interface '%s'", argv[optind]);
		}
	}
}

static void
diag_vcc(int argc, char *argv[])
{
	int opt, fd;

	static int channel, traffic, abr;
	static const struct option opts[] = {
	    { "abr", OPT_SIMPLE, &abr },
	    { "channel", OPT_SIMPLE, &channel },
	    { "traffic", OPT_SIMPLE, &traffic },
	    { NULL, 0, NULL }
	};
	static const char head_channel[] =
	    "Interface\n"
	    "Index Name     VPI VCI   AAL Traffic RxMTU TxMTU Flags\n";
	static const char head_traffic[] =
	    "Interface                Traffic parameters\n"
	    "Index Name     VPI VCI   PCR     SCR     MBS     MCR\n";
	static const char head_abr[] =
	    "Interface                ABR traffic parameters\n"
	    "Index Name     VPI VCI   ICR     TBE     NRM TRM ADTF RIF  RDF  "
	    "CDF\n";

	while ((opt = parse_options(&argc, &argv, opts)) != -1)
		switch (opt) {
		}

	fd = socket(PF_NATM, SOCK_STREAM, PROTO_NATMAAL5);
	if (fd < 0)
		err(1, "socket");

	diagif_fetch();
	if (TAILQ_EMPTY(&diagif_list))
		errx(1, "no ATM interfaces found");

	if (!channel && !traffic && !abr)
		channel = 1;

	if (channel)
		diag_vcc_loop(print_channel, head_channel, argc, argv, fd);
	if (traffic)
		diag_vcc_loop(print_traffic, head_traffic, argc, argv, fd);
	if (abr)
		diag_vcc_loop(print_abr, head_abr, argc, argv, fd);
}

/*
 * Print driver-internal statistics
 */
static void
diag_stats(int argc, char *argv[])
{
	int opt;
	char *str;
	size_t len;
	uint32_t *stats;
	struct diagif *aif;
	u_int i;

	static const struct option opts[] = {
	    { NULL, 0, NULL }
	};

	while ((opt = parse_options(&argc, &argv, opts)) != -1)
		switch (opt) {
		}

	if (argc != 1)
		errx(1, "need one arg for 'diag stats'");

	diagif_fetch();
	TAILQ_FOREACH(aif, &diagif_list, link)
		if (strcmp(aif->ifname, argv[0]) == 0)
			break;

	if (aif == NULL)
		errx(1, "interface '%s' not found", argv[0]);

	if (asprintf(&str, "hw.atm.%s.istats", argv[0]) == -1)
		err(1, NULL);
	len = 0;
	if (sysctlbyname(str, NULL, &len, NULL, 0))
		err(1, "'%s' not found", str);

	stats = malloc(len);
	if (stats == NULL)
		err(1, NULL);

	if (sysctlbyname(str, stats, &len, NULL, 0))
		err(1, "'%s' not found", str);
	free(str);

	if (aif->mib.device >= sizeof(print_stats) / sizeof(print_stats[0]) ||
	    print_stats[aif->mib.device] == NULL)
		errx(1, "unknown stats format (%u)", aif->mib.device);

	for (i = 0; print_stats[aif->mib.device][i] != NULL; i++) {
		if (i * sizeof(uint32_t) >= len)
			errx(1, "debug info too short (version mismatch?)");
		printf("%-22s%u\n", print_stats[aif->mib.device][i], stats[i]);
	}
	free(stats);

	if (i != len / sizeof(uint32_t))
		errx(1, "debug info too long (version mismatch?)");
}
