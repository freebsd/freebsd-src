/*-
 * Copyright (c) 2014-2015 SRI International
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
#include <sys/ucontext.h>

#include <machine/cheri.h>
#include <machine/cheric.h>

#include <net/bpf.h>
#include <net/ethernet.h>
#include <netinet/in.h>

#include <cheri/cheri_enter.h>
#include <cheri/cheri_stack.h>
#include <cheri/sandbox.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cheri_tcpdump_control.h"

#include "tcpdump-stdinc.h"
#include "extract.h"
#include "netdissect.h"
#include "interface.h"
#include "print.h"

/* Needs CHERI_TCPDUMP_CCALL from netdissect.h */
#include <tcpdump-helper.h>

struct cheri_object cheri_tcpdump;

typedef int(*sandbox_selector)(int dlt, size_t len,
	    __capability const u_char *data, void *selector_dta);

struct tcpdump_sandbox {
	STAILQ_ENTRY(tcpdump_sandbox)	 tds_entry;

	struct sandbox_object	*tds_sandbox_object;
	const char		*tds_name;
	const char		*tds_text_color_str;
	sandbox_selector	 tds_selector;
	void			*tds_selector_data;	/* free()'d */

	struct tcpdump_sandbox	**tds_proto_sandboxes;
	int			 tds_num_proto_sandboxes;
	__capability struct cheri_object *tds_proto_sandbox_objects;

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
#define	TDS_MODE_PER_PROTOCOL	CTDC_MODE_PER_PROTOCOL
#define	TDS_MODE_HASH_TCP	CTDC_MODE_HASH_TCP

#define	DEFAULT_COLOR		"\x1b[1;37;40m"
#ifdef COLORIZE
#define	SB_FAIL_COLOR		"\x1b[1;31;40m"
#define	SB_GREEN		"\x1b[1;32;40m"
#define	SB_YELLOW		"\x1b[1;33;40m"
#define	SB_BLUE			"\x1b[1;34;40m"
#define	SB_MAGENTA		"\x1b[1;35;40m"
#define	SB_CYAN			"\x1b[1;36;40m"
#else
#define	SB_FAIL_COLOR		DEFAULT_COLOR
#define	SB_GREEN		DEFAULT_COLOR
#define	SB_YELLOW		DEFAULT_COLOR
#define	SB_BLUE			DEFAULT_COLOR
#define	SB_MAGENTA		DEFAULT_COLOR
#define	SB_CYAN			DEFAULT_COLOR
#endif

#define	SANDBOX_STACK_UNWOUND	0x12345678

u_int32_t	g_localnet;
u_int32_t	g_mask;
u_int32_t	g_timezone_offset;
int		g_type = -1;
int		g_mode;
int		g_sandboxes = 1;
int		g_max_lifetime;
int		g_max_packets;
useconds_t	g_timeout;
int		g_reset;

static const char *hash_colors[] = {
	SB_GREEN,
	SB_BLUE,
	SB_CYAN
};
#define	N_HASH_COLORS	3

#define	MAX_SANDBOXES	129
static char *hash_names[MAX_SANDBOXES];
static char *proto_names[MAX_SANDBOXES];

static struct cheri_tcpdump_control _ctdc;
static volatile struct cheri_tcpdump_control *ctdc;

static struct tcpdump_sandbox_list sandboxes =
    STAILQ_HEAD_INITIALIZER(sandboxes);
static struct tcpdump_sandbox *default_sandbox;

static struct sandbox_class	*tcpdump_classp;

static void
handle_alarm(int sig, siginfo_t *info __unused, void *vuap)
{

	assert(sig == SIGALRM);

	cheri_stack_unwind(vuap, SANDBOX_STACK_UNWOUND, SANDBOX_STACK_UNWOUND,
	    0);
}

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

	if (sandbox_object_new(tcpdump_classp, 128*1024,
	    &sb->tds_sandbox_object) < 0) {
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
	int i;

	if (sb->tds_proto_sandboxes != NULL) {
		for (i = 0; i < sb->tds_num_proto_sandboxes; i++)
			tcpdump_sandbox_destroy(sb->tds_proto_sandboxes[i]);
		free(sb->tds_proto_sandboxes);
		free((void *)sb->tds_proto_sandbox_objects);
	}
	sandbox_object_destroy(sb->tds_sandbox_object);
	free(sb);
}

static int
tcpdump_sandbox_reset(struct tcpdump_sandbox *sb)
{
	int i;

	if (sb->tds_current_invokes == 0)
		return (0);
	if (sandbox_object_reset(sb->tds_sandbox_object) < 0) {
		fprintf(stderr, "failed to reset sandbox object: %s",
		    strerror(errno));
		return (-1);
	}
	if (sb->tds_proto_sandboxes != NULL)
		for (i = 0; i < sb->tds_num_proto_sandboxes; i++)
			tcpdump_sandbox_reset(sb->tds_proto_sandboxes[i]);
	sb->tds_resets++;
	sb->tds_current_invokes = 0;

	return (0);
}

static int
tds_select_all(int dlt __unused, size_t len __unused,
    __capability const u_char *data __unused, void *sd __unused)
{

	/* Accept all packets */
	return (1);
}

static int
tds_select_ipv4(int dlt, size_t len, __capability const u_char *data,
    void *sd __unused)
{
	const struct ether_header *eh;

	eh = (struct ether_header *)data;

	if (dlt != DLT_EN10MB ||
	    len < sizeof(struct ether_header) + sizeof(struct ip))
		return (0);

	if (eh->ether_type == ETHERTYPE_IP)
		return (1);

	return (0);
}

static int
tds_select_ipv4_fromlocal(int dlt, size_t len, __capability const u_char *data,
    void *sd __unused)
{
	const struct ip *iphdr;

	if (!tds_select_ipv4(dlt, len, data, sd))
		return (0);

	iphdr = (const struct ip *)(data +
	    sizeof(struct ether_header));
	if ((EXTRACT_32BITS(&iphdr->ip_src) & g_mask) == g_localnet)
		return (1);

	return (0);
}

static int
tds_select_ipv4_hash(int dlt, size_t len, __capability const u_char *data,
    void *sd)
{
	int mod;
	const u_char *ipaddr;
	struct ip *iphdr;

	if (!tds_select_ipv4(dlt, len, data, sd))
		return (0);

	mod = (int)sd;

	iphdr = (struct ip *)(data + sizeof(struct ether_header));
	ipaddr = (const u_char *)&(iphdr->ip_src);
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
		if (sb == NULL)
			return (-1);
		STAILQ_INSERT_TAIL(list, sb, tds_entry);
		default_sandbox = sb;
		break;
	case TDS_MODE_SEPARATE_LOCAL:
		sb = tcpdump_sandbox_new("ipv4_local", SB_YELLOW,
		    tds_select_ipv4_fromlocal, NULL);
		if (sb == NULL)
			return (-1);
		STAILQ_INSERT_TAIL(list, sb, tds_entry);
		sb = tcpdump_sandbox_new("ipv4", SB_BLUE, tds_select_ipv4,
		    NULL);
		if (sb == NULL)
			return (-1);
		STAILQ_INSERT_TAIL(list, sb, tds_entry);
		sb = tcpdump_sandbox_new("catchall", SB_GREEN, tds_select_all,
		    NULL);
		if (sb == NULL)
			return (-1);
		STAILQ_INSERT_TAIL(list, sb, tds_entry);
		default_sandbox = sb;
		break;
	case TDS_MODE_PER_PROTOCOL:
		sb = tcpdump_sandbox_new("per-protocol", SB_YELLOW,
		    tds_select_all, NULL);
		if (sb == NULL)
			return (-1);
		STAILQ_INSERT_TAIL(list, sb, tds_entry);
		default_sandbox = sb;

		sb->tds_num_proto_sandboxes = g_sandboxes - 1;
		sb->tds_proto_sandboxes = calloc(sb->tds_num_proto_sandboxes,
		    sizeof(*sb->tds_proto_sandboxes));
		sb->tds_proto_sandbox_objects =
		    (__capability void *)calloc(sb->tds_num_proto_sandboxes,
		    sizeof(*sb->tds_proto_sandbox_objects));
		for (i = 0; i < sb->tds_num_proto_sandboxes; i++) {
			sb->tds_proto_sandboxes[i] =
			    tcpdump_sandbox_new(proto_names[i],
			    hash_colors[i % N_HASH_COLORS], NULL, NULL);
			sb->tds_proto_sandbox_objects[i] =
			    sandbox_object_getobject(
			    sb->tds_proto_sandboxes[i]->tds_sandbox_object);
		}
		break;
	case TDS_MODE_HASH_TCP:
#ifdef TCPDUMP_BENCHMARKING
		fprintf(stderr, "creating %d IP sandboxes\n", g_sandboxes);
#endif
		for (i = 0; i < g_sandboxes; i++) {
			sb = tcpdump_sandbox_new(hash_names[i],
			    hash_colors[i % N_HASH_COLORS],
			    tds_select_ipv4_hash, (void *)(long)i);
			if (sb == NULL)
				return (-1);
			STAILQ_INSERT_TAIL(list, sb, tds_entry);
		}
		sb = tcpdump_sandbox_new("catchall", SB_GREEN, tds_select_all,
		    NULL);
		if (sb == NULL)
			return (-1);
		STAILQ_INSERT_TAIL(list, sb, tds_entry);
		default_sandbox = sb;
		break;
	default:
		error("unknown sandbox mode %d", mode);
	}
	/* Given non _cap ccalls somewhere to go. */
	cheri_tcpdump = sandbox_object_getobject(
	    default_sandbox->tds_sandbox_object);

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
    __capability const u_char *data)
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
    const struct pcap_pkthdr *hdr, __capability const u_char *data)
{
	int i, ret;
	struct timeval now;
	__capability const u_char *save_packetp, *save_snapend;

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
		struct cheri_object null_sandbox = CHERI_OBJECT_INIT_NULL;
		if (ctdc->ctdc_sb_max_lifetime > 0)
			gettimeofday(&sb->tds_current_start, NULL);

		save_packetp = gndo->ndo_packetp;
		save_snapend = gndo->ndo_snapend;
		gndo->ndo_packetp = NULL;
		gndo->ndo_snapend = NULL;

		ret = cheri_tcpdump_sandbox_init_cap(
		    sandbox_object_getobject(sb->tds_sandbox_object),
		    g_localnet, g_mask, g_timezone_offset,
		    cheri_ptrperm(gndo, sizeof(netdissect_options),
		    CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP),
		    sb->tds_num_proto_sandboxes == 0 ? null_sandbox :
		    sandbox_object_getobject(
		    sb->tds_proto_sandboxes[0]->tds_sandbox_object));
		if (ret != 0)
			error("failed to initialize sandbox: %d \n",
			    ret);
		if (sb->tds_proto_sandboxes != NULL) {
			for (i = 0; i < sb->tds_num_proto_sandboxes; i++) {
				struct cheri_object sobj;
				sobj = sandbox_object_getobject(
				    sb->tds_proto_sandboxes[i]->tds_sandbox_object);
				ret = cheri_tcpdump_sandbox_init_cap(sobj,
				    g_localnet, g_mask, g_timezone_offset,
				    cheri_ptrperm(gndo, sizeof(netdissect_options),
				    CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP),
				    i + 1 >= sb->tds_num_proto_sandboxes ?
				    null_sandbox :
				    sandbox_object_getobject(
				    sb->tds_proto_sandboxes[i+1]->tds_sandbox_object));
				if (ret != 0)
					error("failed to initialize "
					    "per-protocol sandbox %d: %d \n",
					    i, ret);
			}
		}
		gndo->ndo_snapend = save_snapend;
		gndo->ndo_packetp = save_packetp;
	}

	/* Invoke */
	sb->tds_total_invokes++;
	sb->tds_current_invokes++;
	if (g_timeout != 0)
		ualarm(g_timeout, 0);
	ret = cheri_sandbox_pretty_print_packet_cap(
	    sandbox_object_getobject(sb->tds_sandbox_object),
	    cheri_ptrperm((void *)hdr, sizeof(*hdr),
		CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP),
	    cheri_ptrperm((void *)data, hdr->caplen,
		CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP));
	if (ret == SANDBOX_STACK_UNWOUND) {
		printf("<sandbox-timeout>");
		/* XXX: dump hex here? */
		tcpdump_sandbox_reset(sb);
	} else if (g_timeout != 0)
		ualarm(0, 0);

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

	if (sandbox_class_new("/usr/libexec/tcpdump-helper",
	    8*1024*1024, &tcpdump_classp) < 0) {
		fprintf(stderr, "failed to create sandbox class: %s",
		    strerror(errno));
		return (-1);
	}

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

	if (g_timeout != ctdc->ctdc_sb_timeout) {
		fprintf(stderr, "%s max dissection time from %d to %d\n",
		    g_timeout < ctdc->ctdc_sb_timeout ?
		    "increasing" : "decreasing",
		    g_timeout, ctdc->ctdc_sb_timeout);
		g_timeout = ctdc->ctdc_sb_timeout;
	}
}

void
init_print(uint32_t localnet, uint32_t mask, uint32_t timezone_offset)
{
	char *control_file;
	int control_fd = -1, i;
	stack_t sigstk;
	struct sigaction sa;

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
	g_timezone_offset = timezone_offset;
	g_max_lifetime = ctdc->ctdc_sb_max_lifetime;
	g_max_packets = ctdc->ctdc_sb_max_packets;
	g_timeout = ctdc->ctdc_sb_timeout;
	g_reset = ctdc->ctdc_reset;
	g_sandboxes = CTDC_SANDBOXES(ctdc, 1, MAX_SANDBOXES);

	/* XXX: check returns */
	for (i = 0; i < MAX_SANDBOXES; i++)
		asprintf(&hash_names[i], "hash%02d", i);
	for (i = 0; i < MAX_SANDBOXES; i++)
		asprintf(&proto_names[i], "proto%02d", i);

	if (tcpdump_classp == NULL) {
		if (tcpdump_sandbox_object_setup() != 0)
			error("failure setting up sandbox object");
	}

	sigstk.ss_size = max(getpagesize(), SIGSTKSZ);
	if ((sigstk.ss_sp = mmap(NULL, sigstk.ss_size, PROT_READ | PROT_WRITE,
	    MAP_ANON, -1, 0)) == MAP_FAILED)
		error("failure allocating alternative signal stack");
	sigstk.ss_flags = 0;
	if (sigaltstack(&sigstk, NULL) < 0)
		error("sigaltstack");

	sa.sa_sigaction = handle_alarm;
	sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
	sigemptyset(&sa.sa_mask);
	if (sigaction(SIGALRM, &sa, NULL) < 0)
		error("sigaction(SIGALRM)");
}

int
has_printer(int type)
{

	if (tcpdump_classp == NULL &&
	    tcpdump_sandbox_object_setup() != 0)
		return (0);	/* XXX: should error? */

	return (cheri_sandbox_has_printer(type));
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
    __capability const u_char *sp, u_int packets_captured)
{
	int ret;
	struct tcpdump_sandbox *sb;

	poll_ctdc_config();

	if (ctdc->ctdc_colorize)
		set_color_default();

	if (print_info->ndo->ndo_packet_number)
		printf("%5u  ", packets_captured);

	ts_print(print_info->ndo, &h->ts);

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

	/* XXX-BD: cast should be safe as we're unsandboxed */
	raw_print(print_info->ndo, h, (const u_char *)sp, (ret >= 0 && (u_int)ret <= h->caplen) ? ret : 0);
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
ndo_default_print(netdissect_options *ndo _U_, const u_char *bp, u_int length)
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
