/*
 * Copyright (C) 2013-2014 Michio Honda. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
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

/* $FreeBSD$ */

#define LIBNETMAP_NOTHREADSAFE
#include <libnetmap.h>

#include <errno.h>
#include <stdio.h>
#include <inttypes.h>	/* PRI* macros */
#include <string.h>	/* strcmp */
#include <fcntl.h>	/* open */
#include <unistd.h>	/* close */
#include <sys/ioctl.h>	/* ioctl */
#include <sys/param.h>
#include <sys/socket.h>	/* apple needs sockaddr */
#include <net/if.h>	/* ifreq */
#include <libgen.h>	/* basename */
#include <stdlib.h>	/* atoi, free */

int verbose;

struct args {
	const char *name;
	const char *config;
	const char *mem_id;

	uint16_t nr_reqtype;
	uint32_t nr_mode;
};

static void
dump_port_info(struct nmreq_port_info_get *v)
{
	printf("memsize:    %"PRIu64"\n", v->nr_memsize);
	printf("tx_slots:   %"PRIu32"\n", v->nr_tx_slots);
	printf("rx_slots:   %"PRIu32"\n", v->nr_rx_slots);
	printf("tx_rings:   %"PRIu16"\n", v->nr_tx_rings);
	printf("rx_rings    %"PRIu16"\n", v->nr_rx_rings);
	printf("mem_id:     %"PRIu16"\n", v->nr_mem_id);
}

static void
dump_newif(struct nmreq_vale_newif *v)
{
	printf("tx_slots:   %"PRIu32"\n", v->nr_tx_slots);
	printf("rx_slots:   %"PRIu32"\n", v->nr_rx_slots);
	printf("tx_rings:   %"PRIu16"\n", v->nr_tx_rings);
	printf("rx_ring:    %"PRIu16"\n", v->nr_rx_rings);
	printf("mem_id:     %"PRIu16"\n", v->nr_mem_id);
}

static void
dump_vale_list(struct nmreq_vale_list *v)
{
	printf("bridge_idx: %"PRIu16"\n", v->nr_bridge_idx);
	printf("port_idx:   %"PRIu16"\n", v->nr_port_idx);
}


static void
parse_ring_config(const char* conf,
		uint32_t *nr_tx_slots,
		uint32_t *nr_rx_slots,
		uint16_t *nr_tx_rings,
		uint16_t *nr_rx_rings)
{
	char *w, *tok;
	int i, v;

	*nr_tx_rings = *nr_rx_rings = 0;
	*nr_tx_slots = *nr_rx_slots = 0;
	if (conf == NULL || ! *conf)
		return;
	w = strdup(conf);
	for (i = 0, tok = strtok(w, ","); tok; i++, tok = strtok(NULL, ",")) {
		v = atoi(tok);
		switch (i) {
		case 0:
			*nr_tx_slots = *nr_rx_slots = v;
			break;
		case 1:
			*nr_rx_slots = v;
			break;
		case 2:
			*nr_tx_rings = *nr_rx_rings = v;
			break;
		case 3:
			*nr_rx_rings = v;
			break;
		default:
			fprintf(stderr, "ignored config: %s", tok);
			break;
		}
	}
	ND("txr %d txd %d rxr %d rxd %d",
			*nr_tx_rings, *nr_tx_slots,
			*nr_rx_rings, *nr_rx_slots);
	free(w);
}

static int
parse_poll_config(const char *conf, struct nmreq_vale_polling *v)
{
	char *w, *tok;
	int i, p;

	if (conf == NULL || ! *conf) {
		fprintf(stderr, "invalid null/empty config\n");
		return -1;
	}
	w = strdup(conf);
	for (i = 0, tok = strtok(w, ","); tok; i++, tok = strtok(NULL, ",")) {
		p = atoi(tok);
		switch (i) {
		case 0:
			v->nr_mode = p ? NETMAP_POLLING_MODE_MULTI_CPU :
				NETMAP_POLLING_MODE_SINGLE_CPU;
			break;
		case 1:
			v->nr_first_cpu_id = p;
			break;
		case 2:
			if (v->nr_mode != NETMAP_POLLING_MODE_MULTI_CPU) {
				fprintf(stderr, "too many numbers in '%s'\n", conf);
				return -1;
			}
			v->nr_num_polling_cpus = p;
			break;
		case 3:
			fprintf(stderr, "too many numbers in '%s'\n", conf);
			return -1;
		}
	}
	free(w);
	return 0;
}

static int32_t
parse_mem_id(const char *mem_id)
{
	int32_t id;

	if (mem_id == NULL)
		return 0;
	if (isdigit(*mem_id))
		return atoi(mem_id);
	id = nmreq_get_mem_id(&mem_id, nmctx_get());
	if (id == 0) {
		fprintf(stderr, "invalid format in '-m %s' (missing 'netmap:'?)\n", mem_id);
		return -1;
	}
	return id;
}

static int
list_all(int fd, struct nmreq_header *hdr)
{
	int error;
	struct nmreq_vale_list *vale_list =
		(struct nmreq_vale_list *)hdr->nr_body;

	for (;;) {
		hdr->nr_name[0] = '\0';
		error = ioctl(fd, NIOCCTRL, hdr);
		if (error < 0) {
			if (errno == ENOENT)
				break;

			fprintf(stderr, "failed to list all: %s\n", strerror(errno));
			return 1;
		}
		printf("%s bridge_idx %"PRIu16" port_idx %"PRIu32"\n", hdr->nr_name,
				vale_list->nr_bridge_idx, vale_list->nr_port_idx);
		vale_list->nr_port_idx++;
	}
	return 1;
}

static int
bdg_ctl(struct args *a)
{
	struct nmreq_header hdr;
	struct nmreq_vale_attach   vale_attach;
	struct nmreq_vale_detach   vale_detach;
	struct nmreq_vale_newif    vale_newif;
	struct nmreq_vale_list     vale_list;
	struct nmreq_vale_polling  vale_polling;
	struct nmreq_port_info_get port_info_get;
	int error = 0;
	int fd;
	int32_t mem_id;
	const char *action = NULL;

	fd = open("/dev/netmap", O_RDWR);
	if (fd == -1) {
		perror("/dev/netmap");
		return 1;
	}

	bzero(&hdr, sizeof(hdr));
	hdr.nr_version = NETMAP_API;
	if (a->name != NULL) { /* might be NULL */
		strncpy(hdr.nr_name, a->name, NETMAP_REQ_IFNAMSIZ - 1);
		hdr.nr_name[NETMAP_REQ_IFNAMSIZ - 1] = '\0';
	}
	hdr.nr_reqtype = a->nr_reqtype;

	switch (a->nr_reqtype) {
	case NETMAP_REQ_VALE_DELIF:
		/* no body */
		action = "remove";
		break;

	case NETMAP_REQ_VALE_NEWIF:
		memset(&vale_newif, 0, sizeof(vale_newif));
		hdr.nr_body = (uintptr_t)&vale_newif;
		parse_ring_config(a->config,
				&vale_newif.nr_tx_slots,
				&vale_newif.nr_rx_slots,
				&vale_newif.nr_tx_rings,
				&vale_newif.nr_rx_rings);
		mem_id = parse_mem_id(a->mem_id);
		if (mem_id < 0)
			return 1;
		vale_newif.nr_mem_id = mem_id;
		action = "create";
		break;

	case NETMAP_REQ_VALE_ATTACH:
		memset(&vale_attach, 0, sizeof(vale_attach));
		hdr.nr_body = (uintptr_t)&vale_attach;
		vale_attach.reg.nr_mode = a->nr_mode;
		parse_ring_config(a->config,
				&vale_attach.reg.nr_tx_slots,
				&vale_attach.reg.nr_rx_slots,
				&vale_attach.reg.nr_tx_rings,
				&vale_attach.reg.nr_rx_rings);
		mem_id = parse_mem_id(a->mem_id);
		if (mem_id < 0)
			return 1;
		vale_attach.reg.nr_mem_id = mem_id;
		action = "attach";
		break;

	case NETMAP_REQ_VALE_DETACH:
		memset(&vale_detach, 0, sizeof(vale_detach));
		hdr.nr_body = (uintptr_t)&vale_detach;
		action = "detach";
		break;

	case NETMAP_REQ_VALE_LIST:
		memset(&vale_list, 0, sizeof(vale_list));
		hdr.nr_body = (uintptr_t)&vale_list;
		if (a->name == NULL) {
			return list_all(fd, &hdr);
		}
		action = "list";
		break;

	case NETMAP_REQ_VALE_POLLING_ENABLE:
		action = "enable polling on";
		/* fall through */
	case NETMAP_REQ_VALE_POLLING_DISABLE:
		memset(&vale_polling, 0, sizeof(vale_polling));
		hdr.nr_body = (uintptr_t)&vale_polling;
		parse_poll_config(a->config, &vale_polling);
		if (action == NULL)
			action ="disable polling on";
		break;

	case NETMAP_REQ_PORT_INFO_GET:
		memset(&port_info_get, 0, sizeof(port_info_get));
		hdr.nr_body = (uintptr_t)&port_info_get;
		action = "obtain info for";
		break;
	}
	error = ioctl(fd, NIOCCTRL, &hdr);
	if (error < 0) {
		fprintf(stderr, "failed to %s %s: %s\n",
				action, a->name, strerror(errno));
		return 1;
	}
	switch (hdr.nr_reqtype) {
	case NETMAP_REQ_VALE_NEWIF:
		if (verbose) {
			dump_newif(&vale_newif);
		}
		break;

	case NETMAP_REQ_VALE_ATTACH:
		if (verbose) {
			printf("port_index: %"PRIu32"\n", vale_attach.port_index);
		}
		break;

	case NETMAP_REQ_VALE_DETACH:
		if (verbose) {
			printf("port_index: %"PRIu32"\n", vale_detach.port_index);
		}
		break;

	case NETMAP_REQ_VALE_LIST:
		dump_vale_list(&vale_list);
		break;

	case NETMAP_REQ_PORT_INFO_GET:
		dump_port_info(&port_info_get);
		break;
	}
	close(fd);
	return error;
}

static void
usage(int errcode)
{
	fprintf(stderr,
	    "Usage:\n"
	    "vale-ctl [arguments]\n"
	    "\t-g interface	interface name to get info\n"
	    "\t-d interface	interface name to be detached\n"
	    "\t-a interface	interface name to be attached\n"
	    "\t-h interface	interface name to be attached with the host stack\n"
	    "\t-n interface	interface name to be created\n"
	    "\t-r interface	interface name to be deleted\n"
	    "\t-l vale-port	show bridge and port indices\n"
	    "\t-C string ring/slot setting of an interface creating by -n\n"
	    "\t-p interface start polling. Additional -C x,y,z configures\n"
	    "\t\t x: 0 (REG_ALL_NIC) or 1 (REG_ONE_NIC),\n"
	    "\t\t y: CPU core id for ALL_NIC and core/ring for ONE_NIC\n"
	    "\t\t z: (ONE_NIC only) num of total cores/rings\n"
	    "\t-P interface stop polling\n"
	    "\t-m memid to use when creating a new interface\n"
	    "\t-v increase verbosity\n"
	    "with no arguments: list all existing vale ports\n");
	exit(errcode);
}

int
main(int argc, char *argv[])
{
	int ch;
	struct args a = {
		.name = NULL,
		.config = NULL,
		.mem_id = NULL,
		.nr_reqtype = 0,
		.nr_mode = NR_REG_ALL_NIC,
	};

	while ((ch = getopt(argc, argv, "d:a:h:g:l:n:r:C:p:P:m:v")) != -1) {
		switch (ch) {
		default:
			fprintf(stderr, "bad option %c %s", ch, optarg);
			usage(1);
			break;
		case 'd':
			a.nr_reqtype = NETMAP_REQ_VALE_DETACH;
			a.name = optarg;
			break;
		case 'a':
			a.nr_reqtype = NETMAP_REQ_VALE_ATTACH;
			a.nr_mode = NR_REG_ALL_NIC;
			a.name = optarg;
			break;
		case 'h':
			a.nr_reqtype = NETMAP_REQ_VALE_ATTACH;
			a.nr_mode = NR_REG_NIC_SW;
			a.name = optarg;
			break;
		case 'n':
			a.nr_reqtype = NETMAP_REQ_VALE_NEWIF;
			a.name = optarg;
			break;
		case 'r':
			a.nr_reqtype = NETMAP_REQ_VALE_DELIF;
			a.name = optarg;
			break;
		case 'g':
			a.nr_reqtype = NETMAP_REQ_PORT_INFO_GET;
			a.name = optarg;
			break;
		case 'l':
			a.nr_reqtype = NETMAP_REQ_VALE_LIST;
			a.name = optarg;
			if (strncmp(a.name, NM_BDG_NAME, strlen(NM_BDG_NAME))) {
				fprintf(stderr, "invalid vale port name: '%s'\n", a.name);
				usage(1);
			}
			break;
		case 'C':
			a.config = optarg;
			break;
		case 'p':
			a.nr_reqtype = NETMAP_REQ_VALE_POLLING_ENABLE;
			a.name = optarg;
			break;
		case 'P':
			a.nr_reqtype = NETMAP_REQ_VALE_POLLING_DISABLE;
			a.name = optarg;
			break;
		case 'm':
			a.mem_id = optarg;
			break;
		case 'v':
			verbose++;
			break;
		}
	}
	if (optind != argc) {
		usage(1);
	}
	if (argc == 1) {
		a.nr_reqtype = NETMAP_REQ_VALE_LIST;
		a.name = NULL;
	}
	if (!a.nr_reqtype) {
		usage(1);
	}
	return bdg_ctl(&a);
}
