/*
 * Copyright (C) 2002
 * 	Hidetoshi Shimokawa. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *
 *	This product includes software developed by Hidetoshi Shimokawa.
 *
 * 4. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <dev/firewire/firewire.h>
#include <dev/firewire/iec13213.h>

#include <netinet/in.h>
#include <fcntl.h>
#include <stdio.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>

void
usage(void)
{
	printf("bus_mgm [-g gap_count] [-b pri_req] [-c node]"
		" [-r] [-t] [-s]\n");
	printf("\t-g: broadcast gap_count by phy_config packet\n");
	printf("\t-b: set PRIORITY_BUDGET register on all supported nodes\n");
	printf("\t-c: read configuration ROM\n");
	printf("\t-r: bus reset\n");
	printf("\t-t: read topology map\n");
	printf("\t-s: read speed map\n");
	exit(0);
}

void
get_num_of_dev(int fd, struct fw_devlstreq *data)
{
	int i;
	data->n = 64;
	if( ioctl(fd, FW_GDEVLST, data) < 0) {
       			err(1, "ioctl");
	}
#if 1
	printf("%d devices\n", data->n);
	for (i = 0; i < data->n; i++) {
		printf("%d node %d eui:%08x%08x status:%d\n",
			i,
			data->dst[i],
			data->eui[i].hi,
			data->eui[i].lo,
			data->status[i]
		);
	}
#endif
}

u_int32_t
read_write_quad(int fd, struct fw_eui64 eui, u_int32_t addr_lo, int read, u_int32_t data)
{
        struct fw_asyreq *asyreq;
	u_int32_t *qld, res;

        asyreq = (struct fw_asyreq *)malloc(sizeof(struct fw_asyreq_t) + 16);
	asyreq->req.len = 16;
	asyreq->req.type = FWASREQEUI;
	asyreq->req.dst.eui = eui;
#if 0
	asyreq->pkt.mode.rreqq.dst = htons(FWLOCALBUS | node);
#endif
	asyreq->pkt.mode.rreqq.tlrt = 0;
	if (read)
		asyreq->pkt.mode.rreqq.tcode = FWTCODE_RREQQ;
	else
		asyreq->pkt.mode.rreqq.tcode = FWTCODE_WREQQ;

	asyreq->pkt.mode.rreqq.dest_hi = htons(0xffff);
	asyreq->pkt.mode.rreqq.dest_lo = htonl(addr_lo);

	qld = (u_int32_t *)&asyreq->pkt;
	if (!read)
		asyreq->pkt.mode.wreqq.data = data;

	if (ioctl(fd, FW_ASYREQ, asyreq) < 0) {
       		err(1, "ioctl");
	}
	res = qld[3];
	free(asyreq);
	if (read)
		return ntohl(res);
	else
		return 0;
}
void
send_phy_config(int fd, int root_node, int gap_count)
{
        struct fw_asyreq *asyreq;

	asyreq = (struct fw_asyreq *)malloc(sizeof(struct fw_asyreq_t) + 12);
	asyreq->req.len = 12;
	asyreq->req.type = FWASREQNODE;
	asyreq->pkt.mode.ld[0] = 0;
	asyreq->pkt.mode.ld[1] = 0;
	asyreq->pkt.mode.common.tcode = FWTCODE_PHY;
	if (root_node >= 0)
		asyreq->pkt.mode.ld[1] |= htonl((root_node & 0x3f) << 24 | 1 << 23);
	if (gap_count >= 0)
		asyreq->pkt.mode.ld[1] |= htonl(1 << 22 | (gap_count & 0x3f) << 16);
	asyreq->pkt.mode.ld[2] = ~asyreq->pkt.mode.ld[1];

	printf("send phy_config root_node=%d gap_count=%d\n",
						root_node, gap_count);

	if (ioctl(fd, FW_ASYREQ, asyreq) < 0) {
       		err(1, "ioctl");
	}
}

void
set_pri_req(int fd, int pri_req)
{
	struct fw_devlstreq data;
	u_int32_t max, reg, old;
	int i;

	get_num_of_dev(fd, &data);
#define BUGET_REG 0xf0000218
	for (i = 0; i < data.n; i++) {
		if (!data.status[i])
			continue;
		reg = read_write_quad(fd, data.eui[i], BUGET_REG, 1, 0);
		printf("%d %08x:%08x, %08x",
			data.dst[i], data.eui[i].hi, data.eui[i].lo, reg);
		if (reg > 0 && pri_req >= 0) {
			old = (reg & 0x3f);
			max = (reg & 0x3f00) >> 8;
			if (pri_req > max)
				pri_req =  max;
			printf(" 0x%x -> 0x%x\n", old, pri_req);
			read_write_quad(fd, data.eui[i], BUGET_REG, 0, pri_req);
		} else {
			printf("\n");
		}
	}
}

void
parse_text(struct csrtext *textleaf)
{
	char buf[256];
	u_int32_t *bp;
	int i, len;

	bp = (u_int32_t *)&buf[0];
	len = textleaf->crc_len - 2;
	for (i = 0; i < len; i ++) {
		*bp++ = ntohl(textleaf->text[i]);
	}
	*bp = 0;
	printf("'%s'", buf);
}

void
parse_crom(struct csrdirectory *dir, int depth)
{
	int i, j;
	struct csrreg *reg;

	printf(" len=0x%04x(%d) crc=0x%04x\n",
			dir->crc_len, dir->crc_len, dir->crc);
	if (dir->crc_len > 0xff) {
		printf(" too long!\n");
		return;
	}
	for (i = 0; i < dir->crc_len; i ++) {
		for (j = 0; j < depth; j++)
			printf("\t");
		reg = &dir->entry[i];
		printf("0x%02x 0x%06x ", reg->key, reg->val);
		switch (reg->key) {
		case 0x03:
			printf("module_vendor_ID");
			break;
		case 0x04:
			printf("XXX");
			break;
		case 0x0c:
			printf("node_capabilities");
			break;
		case 0x12:
			printf("unit_spec_ID");
			break;
		case 0x13:
			printf("unit_sw_version");
			break;
		case 0x14:
			printf("logical_unit_number");
			break;
		case 0x17:
			printf("model_ID");
			break;
		case 0x38:
			printf("command_set_spec_ID");
			break;
		case 0x39:
			printf("command_set");
			break;
		case 0x3a:
			printf("unit_characteristics");
			break;
		case 0x3b:
			printf("command_set_revision");
			break;
		case 0x3c:
			printf("firmware_revision");
			break;
		case 0x3d:
			printf("reconnect_timeout");
			break;
		case 0x54:
			printf("management_agent");
			break;
		case 0x81:
			printf("text_leaf: ");
			parse_text((struct csrtext *)(reg + reg->val));
			break;
		case 0xd1:
			printf("unit_directory");
			parse_crom((struct csrdirectory *)(reg + reg->val),
								depth + 1);
			break;
		default:
			printf("uknown");
			break;
		}
		printf("\n");
	}
}
		
void parse_bus_info_block(u_int32_t *p, int info_len)
{
	int i;

	for (i = 0; i < info_len; i++) {
		printf("bus_info: 0x%08x\n", *p++);
	}
}
void
show_crom(int fd, int node)
{
	struct fw_devlstreq data;
	struct fw_crom_buf buf;
	struct csrhdr *hdr;
	u_int32_t *p;
	int i;

	get_num_of_dev(fd, &data);
	for (i = 0; i < data.n; i++) {
		if (data.dst[i] == node && data.eui[i].lo != 0)
			break;
	}
	if (i != data.n) {
		printf("node: %d\n", node);
		buf.eui = data.eui[i];
	} else {
		printf("no such node: %d\n", node);
		return;
	}

	buf.len = 256 * 4;
	buf.ptr = malloc(buf.len);
	if (ioctl(fd, FW_GCROM, &buf) < 0) {
       		err(1, "ioctl");
	}
	p = (u_int32_t *)buf.ptr;
	printf("first quad: 0x%08x\n", *p);
	hdr = (struct csrhdr *)p;
	if (hdr->info_len == 1) {
		/* minimum ROM */
		struct csrreg *reg;
		reg = (struct csrreg *)p;
		printf("verndor ID: 0x%06x\n",  reg->val);
		return;
	}
	printf("len: %d\n", hdr->crc_len);
	parse_bus_info_block(p+1, hdr->info_len);
	p += 1 + hdr->info_len;
	printf("root_directory");
	parse_crom((struct csrdirectory *)p, 0);
}
static void
show_topology_map(int fd)
{
	struct fw_topology_map *tmap;
	union fw_self_id sid;
	int i;
	static char *port_status[] = {" ", "-", "P", "C"};
	static char *pwr_class[] = {" 0W", "15W", "30W", "45W",
					"-1W", "-2W", "-5W", "-9W"};
	static char *speed[] = {"S100", "S200", "S400", "S800"};
	tmap = malloc(sizeof(struct fw_topology_map));
	if (tmap == NULL)
		return;
	if (ioctl(fd, FW_GTPMAP, tmap) < 0) {
       		err(1, "ioctl");
	}
	printf("crc_len: %d generation:%d node_count:%d sid_count:%d\n",
		tmap->crc_len, tmap->generation,
		tmap->node_count, tmap->self_id_count);
	printf("id link gap_cnt speed delay cIRM power port0 port1 port2"
		" ini more\n");
	for (i = 0; i < tmap->crc_len - 2; i++) {
		sid = tmap->self_id[i];
		if (sid.p0.sequel) {
			printf("%02d sequel packet\n", sid.p0.phy_id);
			continue;
		}
		printf("%02d   %2d      %d  %4s     %d    %d   %3s"
				"     %s     %s     %s   %d    %d\n",
			sid.p0.phy_id,
			sid.p0.link_active,
			sid.p0.gap_count,
			speed[sid.p0.phy_speed],
			sid.p0.phy_delay,
			sid.p0.contender,
			pwr_class[sid.p0.power_class],
			port_status[sid.p0.port0],
			port_status[sid.p0.port1],
			port_status[sid.p0.port2],
			sid.p0.initiated_reset,
			sid.p0.more_packets
		);
	}
}

static void
show_speed_map(int fd)
{
	struct fw_speed_map *smap;
	int i,j;

	smap = malloc(sizeof(struct fw_speed_map));
	if (smap == NULL)
		return;
	if (ioctl(fd, FW_GSPMAP, &smap) < 0) {
       		err(1, "ioctl");
	}
	printf("crc_len: %d generation:%d\n", smap->crc_len, smap->generation);
	for (i = 0; i < 64; i ++) {
		for (j = 0; j < 64; j ++)
			printf("%d", smap->speed[i][j]);
		printf("\n");
	}
}

int
main(int argc, char **argv)
{
	char devname[] = "/dev/fw1";
	int fd, tmp;

	if ((fd = open(devname, O_RDWR)) < 0)
		err(1, "open");

	if (argc < 2) {
		usage();
	}

	argv++;
	argc--;
	while (argc > 0) {
		if (strcmp(*argv, "-g") == 0) {
			/* gap count */
			argv++;
			argc--;
			if (argc > 0) {
				tmp = strtoul(*argv, (char **)NULL, 0);
				argv++;
				argc--;
				send_phy_config(fd, -1, tmp);
			} else {
				usage();
			}
		} else if (strcmp(*argv, "-b") == 0) {
			argv++;
			argc--;
			tmp = -1;
			if (argc > 0) {
				tmp = strtoul(*argv, (char **)NULL, 0);
				argv++;
				argc--;
			} else {
				usage();
			}
			set_pri_req(fd, tmp);
		} else if (strcmp(*argv, "-r") == 0) {
			argv++;
			argc--;
			/* bus reset */
			if(ioctl(fd, FW_IBUSRST, &tmp) < 0) {
                       		err(1, "ioctl");
			}
		} else if (strcmp(*argv, "-t") == 0) {
			argv++;
			argc--;
			show_topology_map(fd);
		} else if (strcmp(*argv, "-s") == 0) {
			argv++;
			argc--;
			show_speed_map(fd);
		} else if (strcmp(*argv, "-c") == 0) {
			argv++;
			argc--;
			if (argc > 0) {
				tmp = strtoul(*argv, (char **)NULL, 0);
				argv++;
				argc--;
			} else {
				usage();
			}
			show_crom(fd, tmp);
		} else
			usage();
	}
	return 0;
}
