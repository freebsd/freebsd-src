/*-
 * Copyright (c) 2010-2011
 * 	Swinburne University of Technology, Melbourne, Australia.
 * All rights reserved.
 *
 * This software was developed at the Centre for Advanced Internet
 * Architectures, Swinburne University of Technology, by Sebastian Zander, made
 * possible in part by a gift from The Cisco University Research Program Fund, a
 * corporate advised fund of Silicon Valley Community Foundation.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Description:
 * Functions for control protocol.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/socket.h>
#include <sys/tree.h>

#include <arpa/inet.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/ip_fw.h>
#include <netinet/ip_diffuse.h>
#define	WITH_DIP_INFO 1
#include <netinet/ip_diffuse_export.h>

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "diffuse_proto.h"

/*
 * Print field data in val based on info element id and length.
 * XXX: IPv6 support missing.
 */
void
print_field(int idx, int id, int len, char *val)
{
	char *c;
	struct in_addr a;

	switch(idx) {
	case DIP_IE_SRC_IPV4:
	case DIP_IE_DST_IPV4:
		{
		/* XXX: Resolve to name. */
		a.s_addr = *((uint32_t *)val);
		printf("%s", inet_ntoa(a));
		break;
		}

	case DIP_IE_SRC_PORT:
	case DIP_IE_DST_PORT:
		/* XXX: Resolve to name. */
		printf("%u", ntohs(*((uint16_t *)val)));
		break;

	case DIP_IE_PROTO:
	case DIP_IE_MSG_TYPE:
	case DIP_IE_TIMEOUT_TYPE:
	case DIP_IE_IPV4_TOS:
		/* XXX: Resolve to name. */
		printf("%u", *((uint8_t *)val));
		break;

	case DIP_IE_CLASS_LABEL:
	case DIP_IE_ACTION_FLAGS:
	case DIP_IE_TIMEOUT:
		printf("%u", ntohs(*((uint16_t *)val)));
		break;

	case DIP_IE_ACTION:
	case DIP_IE_CLASSIFIER_NAME:
	case DIP_IE_EXPORT_NAME:
	case DIP_IE_ACTION_PARAMS:
		printf("%s", val);
		break;

	case DIP_IE_PCKT_CNT:
	case DIP_IE_KBYTE_CNT:
		printf("%u", ntohl(*((uint32_t *)val)));
		break;

	case DIP_IE_CLASSES:
		{
		c = val;
		while (c < val + len) {
			printf("%s:", c);
			c += strlen(val) + 1;
			printf("%u", ntohs(*((uint16_t *)c)));
			c += sizeof(uint16_t);
			if (c < val + len)
				printf(" ");
		}
		break;
		}

	default:
		printf("unknown info element %d\n", id);
		break;
	}
}

/* XXX: Not very fast. */
struct dip_info_descr
diffuse_proto_get_info(uint16_t id)
{
	int i;

	for (i = 0; i < sizeof(dip_info) / sizeof(struct dip_info_descr); i++) {
		if (dip_info[i].id == id)
			return (dip_info[i]);
	}

	return (dip_info[sizeof(dip_info) / sizeof(struct dip_info_descr) - 1]);
}

void
diffuse_proto_print_msg(char *buf, struct di_template_head *templ_list)
{
	struct di_template s, *r;
	struct dip_header *hdr;
	struct dip_info_descr info;
	struct dip_set_header *shdr;
	struct dip_templ_header *thdr;
	char time_str[128];
	char *p;
	time_t time;
	int cnt, dlen, i, offs, toffs;

	hdr = (struct dip_header *)buf;
	offs = 0;
	time = ntohl(hdr->time);
	strcpy(time_str, ctime(&time));
	p = strstr(time_str, "\n");
	if (p != NULL)
		*p = '\0';

	printf("ver %u\n", ntohs(hdr->version));
	printf("len %u\n", ntohs(hdr->msg_len));
	printf("seq %u\n", ntohl(hdr->seq_no));
	printf("time %u (%s)\n", hdr->time, time_str);
	offs += sizeof(struct dip_header);

	while (offs < ntohs(hdr->msg_len)) {
		shdr = (struct dip_set_header *)(buf + offs);
		offs += sizeof(struct dip_set_header);

		printf("set %u len %u\n", ntohs(shdr->set_id),
		    ntohs(shdr->set_len));

		if (ntohs(shdr->set_id) <= DIP_SET_ID_FLOWRULE_TPL) {
			/* Process template. */
			thdr = (struct dip_templ_header *)(buf + offs);
			offs += sizeof(struct dip_templ_header);

			s.id = ntohs(thdr->templ_id);
			r = RB_FIND(di_template_head, templ_list, &s);

			if (r == NULL) {
				/* Store template. */
				toffs = offs;
				r = malloc(sizeof(struct di_template));
				if (r == NULL)
					continue; /* XXX: Or break? */
				memset(r, 0, sizeof(struct di_template));
				r->id = s.id;

				while (offs - toffs < ntohs(shdr->set_len) -
				    sizeof(struct dip_set_header) -
				    sizeof(struct dip_templ_header)) {
					r->fields[r->fcnt].id =
					    ntohs(*((uint16_t *)(buf + offs)));
					offs += sizeof(uint16_t);
					info = diffuse_proto_get_info(
					    r->fields[r->fcnt].id);
					r->fields[r->fcnt].idx = info.idx;
					r->fields[r->fcnt].len = info.len;
					if (r->fields[r->fcnt].len == 0) {
						r->fields[r->fcnt].len =
						    ntohs(*((uint16_t *)
						    (buf + offs)));
						offs += sizeof(uint16_t);
					}
					r->fcnt++;
				}
				RB_INSERT(di_template_head, templ_list, r);
			} else {
				offs += ntohs(shdr->set_len) -
				    sizeof(struct dip_set_header) -
				    sizeof(struct dip_templ_header);
			}

			for(i = 0; i < r->fcnt; i++) {
				printf("%s(%d)",
				    diffuse_proto_get_info(
				    r->fields[i].id).name,
				    r->fields[i].len);
				if (i < r->fcnt - 1)
					printf(", ");
			}
			printf("\n");
		} else if (ntohs(shdr->set_id) >= DIP_SET_ID_DATA) {
			/* Print data. */
			s.id = ntohs(shdr->set_id);
			r = RB_FIND(di_template_head, templ_list, &s);

			if (r == NULL) {
				printf("missing template %u!\n", s.id);
				offs += ntohs(shdr->set_len) -
				    sizeof(struct dip_set_header);
			} else {
				toffs = offs;
				cnt = 0;

				while (offs - toffs < ntohs(shdr->set_len) -
				    sizeof(struct dip_set_header)) {
					if (r->fields[cnt].len == -1) {
						/* Read dynamic length */
						dlen =
						    *((unsigned char *)
						    (buf + offs));
						print_field(r->fields[cnt].idx,
						    r->fields[cnt].id, dlen - 1,
						    buf + offs + 1);
						offs += dlen;
					} else {
						print_field(r->fields[cnt].idx,
						    r->fields[cnt].id,
						    r->fields[cnt].len,
						    buf + offs);
						offs += r->fields[cnt].len;
					}
					cnt++;
					if (cnt == r->fcnt) {
						cnt = 0;
						printf("\n");
					} else {
						printf(", ");
					}
				}
			}
			printf("\n");
		} else {
			printf("unknown set type\n");
			offs += ntohs(shdr->set_len);
		}
	}
}
