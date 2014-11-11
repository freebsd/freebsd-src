/*-
 * Copyright (c) 2014 SRI International
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/queue.h>

#include <machine/cheri.h>
#include <machine/cheric.h>

#include <net/bpf.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <cheri/cheri_enter.h>
#include <cheri/sandbox.h>

#include <tcpdump-helper.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cheri_tcpdump_control.h"
#include "cheri_tcpdump_system.h"

#include "extract.h"
#include "netdissect.h"
#include "interface.h"
#include "print.h"

typedef int(*sandbox_selector)(int dlt, size_t len, packetbody_t data,
	    void *selector_dta);

struct tcpdump_sandbox {
	STAILQ_ENTRY(tcpdump_sandbox)	 tds_entry;

	struct sandbox_object	*tds_sandbox_object;
	const char		*tds_name;
	const char		*tds_text_color_str;
	sandbox_selector	 tds_selector;
	void			*tds_selector_data;	/* free()'d */

	/* Lifetime stats for sandboxs matching selector */
	int		 tds_faults;	/* Number of times the sandbox has
					 * faulted. */
	int		 tds_resets;	/* Number of times a new sandbox object
					 * has been created. */
	int		 tds_total_invokes;

	/* Stats for current sandbox object. */
	int		 tds_current_invokes;	/* N invocations */
	struct timeval	 tds_current_start;	/* Time of first invoke */
};

STAILQ_HEAD(tcpdump_sandbox_list, tcpdump_sandbox);

#define	TDS_STATE_NEW		0
#define	TDS_STATE_INVOKED	1

#define	TDS_MODE_ONE_SANDBOX	CTDC_MODE_ONE_SANDBOX
#define	TDS_MODE_SEPARATE_LOCAL	CTDC_MODE_SEPARATE_LOCAL
#define	TDS_MODE_HASH_TCP	CTDC_MODE_HASH_TCP

#define	DEFAULT_COLOR		"\x1b[1;37;40m"
#define	SB_FAIL_COLOR		"\x1b[1;31;40m"
#define	SB_GREEN		"\x1b[1;32;40m"
#define	SB_YELLOW		"\x1b[1;33;40m"
#define	SB_BLUE			"\x1b[1;34;40m"
#define	SB_MAGENTA		"\x1b[1;35;40m"
#define	SB_CYAN			"\x1b[1;36;40m"

u_int32_t	g_localnet;
u_int32_t	g_mask;
int		g_type = -1;
int		g_mode;
int		g_sandboxes = 1;
int		g_max_lifetime;
int		g_max_packets;
int		g_reset;

static const char *hash_colors[] = {
	SB_GREEN,
	SB_BLUE,
	SB_CYAN
};
#define	N_HASH_COLORS	3

#define	MAX_SANDBOXES	64
static char *hash_names[MAX_SANDBOXES];

static struct cheri_tcpdump_control _ctdc;
static volatile struct cheri_tcpdump_control *ctdc;

static struct tcpdump_sandbox_list sandboxes =
    STAILQ_HEAD_INITIALIZER(sandboxes);
static struct tcpdump_sandbox *default_sandbox;

static struct sandbox_class	*tcpdump_classp;

static void
set_color_default(void)
{
	printf(DEFAULT_COLOR);
}

static struct tcpdump_sandbox *
tcpdump_sandbox_new(const char * name, const char * color,
    sandbox_selector selector, void * selector_data)
{
	struct tcpdump_sandbox *sb;

	if ((sb = malloc(sizeof(*sb))) == NULL)
		return (NULL);
	memset(sb, 0, sizeof(*sb));

	if (sandbox_object_new(tcpdump_classp, &sb->tds_sandbox_object) < 0) {
		free(sb);
		return (NULL);
	}

	sb->tds_name = name;
	sb->tds_text_color_str = color;
	sb->tds_selector = selector;
	sb->tds_selector_data = selector_data;

	return (sb);
}

static void
tcpdump_sandbox_destroy(struct tcpdump_sandbox *sb)
{

	sandbox_object_destroy(sb->tds_sandbox_object);
	free(sb);
}

static int
tcpdump_sandbox_reset(struct tcpdump_sandbox *sb)
{

	/* XXX: should have a cheap reset based on ELF loader */
	sandbox_object_destroy(sb->tds_sandbox_object);
	if (sandbox_object_new(tcpdump_classp, &sb->tds_sandbox_object) < 0) {
		fprintf(stderr, "failed to create sandbox object: %s",
		    strerror(errno));
		return (-1);
	}
	sb->tds_resets++;
	sb->tds_current_invokes = 0;

	return (0);
}

static int
tds_select_all(int dlt __unused, size_t len __unused,
    packetbody_t data __unused, void *sd __unused)
{

	/* Accept all packets */
	return (1);
}

static int
tds_select_ipv4(int dlt, size_t len, packetbody_t data, void *sd __unused)
{
	__capability const struct ether_header *eh;

	eh = (__capability struct ether_header *)data;

	if (dlt != DLT_EN10MB ||
	    len < sizeof(struct ether_header) + sizeof(struct ip))
		return (0);

	if (eh->ether_type == ETHERTYPE_IP)
		return (1);

	return (0);
}

static int
tds_select_ipv4_fromlocal(int dlt, size_t len, packetbody_t data,
    void *sd __unused)
{
	__capability const struct ip *iphdr;

	if (!tds_select_ipv4(dlt, len, data, sd))
		return (0);

	iphdr = (__capability const struct ip *)(data +
	    sizeof(struct ether_header));
	if ((EXTRACT_32BITS(&iphdr->ip_src) & g_mask) == g_localnet)
		return (1);

	return (0);
}

static int
tds_select_ipv4_hash(int dlt, size_t len, packetbody_t data, void *sd)
{
	int mod;
	packetbody_t ipaddr;
	__capability struct ip *iphdr;

	if (!tds_select_ipv4(dlt, len, data, sd))
		return (0);

	mod = (int)sd;

	iphdr = (__capability struct ip *)(data + sizeof(struct ether_header));
	ipaddr = (__capability const u_char *)&(iphdr->ip_src);
	if ((ipaddr[0] + ipaddr[1] + ipaddr[2] + ipaddr[3]) % g_sandboxes == mod)
		return (1);

	return (0);
}

static int
tcpdump_sandboxes_init(struct tcpdump_sandbox_list *list, int mode)
{
	int i;
	struct tcpdump_sandbox *sb;

	while (!STAILQ_EMPTY(list)) {
		sb = STAILQ_FIRST(list);
		STAILQ_REMOVE_HEAD(list, tds_entry);
		tcpdump_sandbox_destroy(sb);
	}
	assert(STAILQ_EMPTY(list));

	g_mode = mode;
	switch(mode) {
	case TDS_MODE_ONE_SANDBOX:
		sb = tcpdump_sandbox_new("sandbox", SB_GREEN, tds_select_all,
		    NULL);
		STAILQ_INSERT_TAIL(list, sb, tds_entry);
		default_sandbox = sb;
		break;
	case TDS_MODE_SEPARATE_LOCAL:
		sb = tcpdump_sandbox_new("ipv4_local", SB_YELLOW,
		    tds_select_ipv4_fromlocal, NULL);
		STAILQ_INSERT_TAIL(list, sb, tds_entry);
		sb = tcpdump_sandbox_new("ipv4", SB_BLUE, tds_select_ipv4,
		    NULL);
		STAILQ_INSERT_TAIL(list, sb, tds_entry);
		sb = tcpdump_sandbox_new("catchall", SB_GREEN, tds_select_all,
		    NULL);
		STAILQ_INSERT_TAIL(list, sb, tds_entry);
		default_sandbox = sb;
		break;
	case TDS_MODE_HASH_TCP:
		for (i = 0; i < g_sandboxes; i++) {
			sb = tcpdump_sandbox_new(hash_names[i],
			    hash_colors[i % N_HASH_COLORS],
			    tds_select_ipv4_hash, (void *)(long)i);
			STAILQ_INSERT_TAIL(list, sb, tds_entry);
		}
		sb = tcpdump_sandbox_new("catchall", SB_GREEN, tds_select_all,
		    NULL);
		STAILQ_INSERT_TAIL(list, sb, tds_entry);
		default_sandbox = sb;
		break;
	default:
		error("unknown sandbox mode %d", mode);
	}

	return (0);
}

static void
tcpdump_sandboxes_reset_all(struct tcpdump_sandbox_list *list)
{
	struct tcpdump_sandbox *sb;

	STAILQ_FOREACH(sb, list, tds_entry)
		tcpdump_sandbox_reset(sb);
}

static struct tcpdump_sandbox *
tcpdump_sandbox_find(struct tcpdump_sandbox_list *list, int dlt, size_t len,
    packetbody_t data)
{
	struct tcpdump_sandbox *sb;

	STAILQ_FOREACH(sb, list, tds_entry)
		if (sb->tds_selector(dlt, len, data, sb->tds_selector_data))
			return (sb);

	/* properly set up lists won't get here so make them fail */
	return (NULL);
}

static int
tcpdump_sandbox_invoke(struct tcpdump_sandbox *sb,
    const struct pcap_pkthdr *hdr, packetbody_t data)
{
	int ret;
	struct timeval now;

	/* Reset the sandbox if time or packet count exceeded */
	if (ctdc->ctdc_sb_max_packets > 0 &&
	    sb->tds_current_invokes >= ctdc->ctdc_sb_max_packets)
		tcpdump_sandbox_reset(sb);
	else if (ctdc->ctdc_sb_max_lifetime > 0) {
		gettimeofday(&now, NULL);
		if (now.tv_sec - sb->tds_current_start.tv_sec >
		    ctdc->ctdc_sb_max_lifetime)
			tcpdump_sandbox_reset(sb);
	}

	/* If the sandbox hasn't been invoked yet, call the init routine */
	if (sb->tds_current_invokes == 0) {
		if (ctdc->ctdc_sb_max_lifetime > 0)
			gettimeofday(&sb->tds_current_start, NULL);

		ret = sandbox_object_cinvoke(sb->tds_sandbox_object,
		    TCPDUMP_HELPER_OP_INIT, g_localnet, g_mask, 0, 0, 0, 0, 0,
		    sandbox_object_getsystemobject(sb->tds_sandbox_object).co_codecap,
		    sandbox_object_getsystemobject(sb->tds_sandbox_object).co_datacap,
		    cheri_ptrperm((void *)gndo, sizeof(netdissect_options),
			CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP),
		    gndo->ndo_espsecret == NULL ? cheri_zerocap() :
			cheri_ptrperm((void *)gndo->ndo_espsecret,
			strlen(gndo->ndo_espsecret) + 1,
			CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP),
		    cheri_zerocap(), cheri_zerocap(), cheri_zerocap(),
		    cheri_zerocap());
		if (ret != 0)
			fprintf(stderr, "failed to initalizse sandbox: %d \n",
			    ret);
	}

	/* Invoke */
	sb->tds_total_invokes++;
	sb->tds_current_invokes++;
	ret = sandbox_object_cinvoke(sb->tds_sandbox_object,
	    TCPDUMP_HELPER_OP_PRINT_PACKET, 0, 0, 0, 0, 0, 0, 0,
	    sandbox_object_getsystemobject(sb->tds_sandbox_object).co_codecap,
	    sandbox_object_getsystemobject(sb->tds_sandbox_object).co_datacap,
	    cheri_zerocap(), cheri_zerocap(),
	    cheri_ptrperm((void *)hdr, sizeof(*hdr),
		CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP),
	    (__capability void *)data,
	    cheri_zerocap(), cheri_zerocap());

	/* If it fails, reset it */
	if (ret < 0) {
		sb->tds_faults++;
		tcpdump_sandbox_reset(sb);
	}

	return (ret);
}

static int
tcpdump_sandbox_object_setup()
{

	if (sandbox_class_new("/usr/libexec/tcpdump-helper.bin",
	    14*1024*1024, &tcpdump_classp) < 0) {
		fprintf(stderr, "failed to create sandbox class: %s",
		    strerror(errno));
		return (-1);
	}

	(void)sandbox_class_method_declare(tcpdump_classp,
	    TCPDUMP_HELPER_OP_INIT, "init");
	(void)sandbox_class_method_declare(tcpdump_classp,
	    TCPDUMP_HELPER_OP_PRINT_PACKET, "print_packet");
	(void)sandbox_class_method_declare(tcpdump_classp,
	    TCPDUMP_HELPER_OP_HAS_PRINTER, "has_printer");

	return (tcpdump_sandboxes_init(&sandboxes,
	    ctdc->ctdc_sb_mode == 0 ?
	    TDS_MODE_ONE_SANDBOX : ctdc->ctdc_sb_mode));
}

static void
poll_ctdc_config(void)
{
	int reinit;

	if (ctdc->ctdc_die)
		exit(1);

	while (ctdc->ctdc_pause) {
		sleep(1);
	}

	reinit = 0;
	if (ctdc->ctdc_sb_mode > 0 && ctdc->ctdc_sb_mode <= CTDC_MODE_HASH_TCP && ctdc->ctdc_sb_mode != g_mode) {
		reinit = 1;
		fprintf(stderr, "changing sandbox mode from %d to %d\n",
		    g_mode, ctdc->ctdc_sb_mode);
	}
	if (CTDC_SANDBOXES(ctdc, 1, MAX_SANDBOXES) != g_sandboxes) {
		fprintf(stderr, "%s sandboxes from %d to %d\n",
		    g_sandboxes < CTDC_SANDBOXES(ctdc, 1, MAX_SANDBOXES) ?
		    "increasing" : "decreasing", g_sandboxes,
		    CTDC_SANDBOXES(ctdc, 1, MAX_SANDBOXES));
		g_sandboxes = CTDC_SANDBOXES(ctdc, 1, MAX_SANDBOXES);
		if (ctdc->ctdc_sb_mode == CTDC_MODE_HASH_TCP)
			reinit = 1;
	}
	if (g_reset != ctdc->ctdc_reset) {
		reinit = 1;
		g_reset = ctdc->ctdc_reset;
		fprintf(stderr, "resetting all sandboxs\n");
	}
	if (reinit)
		tcpdump_sandboxes_init(&sandboxes, ctdc->ctdc_sb_mode);

	if (ctdc->ctdc_sb_max_lifetime >= 0 &&
	    g_max_lifetime != ctdc->ctdc_sb_max_lifetime) {
		fprintf(stderr, "%s max sandbox lifetime from %d to %d\n",
		    g_max_lifetime < ctdc->ctdc_sb_max_lifetime ?
		    "increasing" : "decreasing",
		    g_max_lifetime, ctdc->ctdc_sb_max_lifetime);
		g_max_lifetime = ctdc->ctdc_sb_max_lifetime;
	}
	if (ctdc->ctdc_sb_max_packets >= 0 &&
	    g_max_packets != ctdc->ctdc_sb_max_packets) {
		fprintf(stderr, "%s max packets from %d to %d\n",
		    g_max_packets < ctdc->ctdc_sb_max_packets ?
		    "increasing" : "decreasing",
		    g_max_packets, ctdc->ctdc_sb_max_packets);
		g_max_packets = ctdc->ctdc_sb_max_packets;
	}
}

void
init_print(u_int32_t localnet, u_int32_t mask)
{
	char *control_file;
	int control_fd = -1, i;

	if ((control_file = getenv("DEMO_CONTROL")) == NULL ||
	    (control_fd = open(control_file, O_RDONLY)) == -1 ||
	    (ctdc = mmap(0, sizeof(*ctdc), PROT_READ, MAP_FILE|MAP_SHARED,
	     control_fd, 0)) == NULL) {
		ctdc = &_ctdc;
		ctdc->ctdc_sb_mode = TDS_MODE_HASH_TCP;
		ctdc->ctdc_colorize = 1;
		ctdc->ctdc_sandboxes = 3;
	} else
		if (control_fd != -1)
			close(control_fd);

	g_localnet = localnet;
	g_mask = mask;
	g_max_lifetime = ctdc->ctdc_sb_max_lifetime;
	g_max_packets = ctdc->ctdc_sb_max_packets;
	g_reset = ctdc->ctdc_reset;
	g_sandboxes = CTDC_SANDBOXES(ctdc, 1, MAX_SANDBOXES);

	/* XXX: check returns */
	for (i = 0; i < MAX_SANDBOXES; i++)
		asprintf(&hash_names[i], "hash%02d", i);

	cheri_system_user_register_fn(&cheri_tcpdump_system);

	if (tcpdump_classp == NULL &&
	    tcpdump_sandbox_object_setup() != 0)
		error("failure setting up sandbox object");

}

int
has_printer(int type)
{

	if (tcpdump_classp == NULL &&
	    tcpdump_sandbox_object_setup() != 0)
		return (0);	/* XXX: should error? */

	return (sandbox_object_cinvoke(default_sandbox->tds_sandbox_object,
	    TCPDUMP_HELPER_OP_HAS_PRINTER, type, 0, 0, 0, 0, 0, 0,
	    sandbox_object_getsystemobject(default_sandbox->tds_sandbox_object).co_codecap,
	    sandbox_object_getsystemobject(default_sandbox->tds_sandbox_object).co_datacap,
	    cheri_zerocap(), cheri_zerocap(), cheri_zerocap(),
	    cheri_zerocap(), cheri_zerocap(), cheri_zerocap()));
}

struct print_info
get_print_info(int type)
{
        struct print_info printinfo;

	/* If the type changes we need to reinit all the sandboxes */
	if (g_type != -1 && type != g_type)
		tcpdump_sandboxes_reset_all(&sandboxes);
	gndo->ndo_dlt = g_type = type;

	/* Not used, contents don't matter */
	memset(&printinfo, 0, sizeof(printinfo));

	return printinfo;
}

void
pretty_print_packet(struct print_info *print_info, const struct pcap_pkthdr *h,
    packetbody_t sp)
{
	int ret;
	struct tcpdump_sandbox *sb;

	poll_ctdc_config();

	if (ctdc->ctdc_colorize)
		set_color_default();

	ts_print(&h->ts);

	sb = tcpdump_sandbox_find(&sandboxes, g_type, h->caplen, sp);
	if (sb == NULL) {
		fprintf(stderr, "No sandbox matched\n");
		sb = default_sandbox;
	}
	printf("[%s] ", sb->tds_name);
	if (ctdc->ctdc_colorize)
		printf("%s", sb->tds_text_color_str);
	ret = tcpdump_sandbox_invoke(sb, h, sp);
	if (ret < 0) {
		printf("%s\nSANDBOX FAULTED. RESETTING.\n",
		    ctdc->ctdc_colorize ? SB_FAIL_COLOR : "");
		tcpdump_sandbox_reset(sb);
	}
	if (ctdc->ctdc_colorize)
		set_color_default();

	raw_print(h, sp, (ret >= 0 && (u_int)ret <= h->caplen) ? ret : 0);
}

/*
 * The following functions are stubs and should never be called outside
 * the print code.  They exist to limit modifictions to the core code.
 */
int
tcpdump_printf(netdissect_options *ndo _U_, const char *fmt, ...)
{

	abort();
}

void
ndo_default_print(netdissect_options *ndo _U_, packetbody_t bp, u_int length)
{

	abort();
}

void
ndo_error(netdissect_options *ndo _U_, const char *fmt, ...)
{

	abort();
}

void
ndo_warning(netdissect_options *ndo _U_, const char *fmt, ...)
{

	abort();
}
