/*
 * Copyright (C) 2014 Vincenzo Maffione. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
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

#if defined(__FreeBSD__)
#include <sys/cdefs.h> /* prerequisite */

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/param.h>	/* defines used in kernel.h */
#include <sys/malloc.h>	/* types used in module initialization */
#include <sys/kernel.h>	/* types used in module initialization */
#include <sys/sockio.h>
#include <sys/socketvar.h>	/* struct socket */
#include <sys/socket.h> /* sockaddrs */
#include <net/if.h>
#include <net/if_var.h>
#include <machine/bus.h>	/* bus_dmamap_* */
#include <sys/endian.h>

#elif defined(linux)

#include "bsd_glue.h"

#elif defined(__APPLE__)

#warning OSX support is only partial
#include "osx_glue.h"

#else

#error	Unsupported platform

#endif /* unsupported */

#include <net/netmap.h>
#include <dev/netmap/netmap_kern.h>



/* This routine is called by bdg_mismatch_datapath() when it finishes
 * accumulating bytes for a segment, in order to fix some fields in the
 * segment headers (which still contain the same content as the header
 * of the original GSO packet). 'buf' points to the beginning (e.g.
 * the ethernet header) of the segment, and 'len' is its length.
 */
static void gso_fix_segment(uint8_t *buf, size_t len, u_int idx,
			    u_int segmented_bytes, u_int last_segment,
			    u_int tcp, u_int iphlen)
{
	struct nm_iphdr *iph = (struct nm_iphdr *)(buf + 14);
	struct nm_ipv6hdr *ip6h = (struct nm_ipv6hdr *)(buf + 14);
	uint16_t *check = NULL;
	uint8_t *check_data = NULL;

	if (iphlen == 20) {
		/* Set the IPv4 "Total Length" field. */
		iph->tot_len = htobe16(len-14);
		ND("ip total length %u", be16toh(ip->tot_len));

		/* Set the IPv4 "Identification" field. */
		iph->id = htobe16(be16toh(iph->id) + idx);
		ND("ip identification %u", be16toh(iph->id));

		/* Compute and insert the IPv4 header checksum. */
		iph->check = 0;
		iph->check = nm_csum_ipv4(iph);
		ND("IP csum %x", be16toh(iph->check));
	} else {/* if (iphlen == 40) */
		/* Set the IPv6 "Payload Len" field. */
		ip6h->payload_len = htobe16(len-14-iphlen);
	}

	if (tcp) {
		struct nm_tcphdr *tcph = (struct nm_tcphdr *)(buf + 14 + iphlen);

		/* Set the TCP sequence number. */
		tcph->seq = htobe32(be32toh(tcph->seq) + segmented_bytes);
		ND("tcp seq %u", be32toh(tcph->seq));

		/* Zero the PSH and FIN TCP flags if this is not the last
		   segment. */
		if (!last_segment)
			tcph->flags &= ~(0x8 | 0x1);
		ND("last_segment %u", last_segment);

		check = &tcph->check;
		check_data = (uint8_t *)tcph;
	} else { /* UDP */
		struct nm_udphdr *udph = (struct nm_udphdr *)(buf + 14 + iphlen);

		/* Set the UDP 'Length' field. */
		udph->len = htobe16(len-14-iphlen);

		check = &udph->check;
		check_data = (uint8_t *)udph;
	}

	/* Compute and insert TCP/UDP checksum. */
	*check = 0;
	if (iphlen == 20)
		nm_csum_tcpudp_ipv4(iph, check_data, len-14-iphlen, check);
	else
		nm_csum_tcpudp_ipv6(ip6h, check_data, len-14-iphlen, check);

	ND("TCP/UDP csum %x", be16toh(*check));
}


/* The VALE mismatch datapath implementation. */
void bdg_mismatch_datapath(struct netmap_vp_adapter *na,
			   struct netmap_vp_adapter *dst_na,
			   struct nm_bdg_fwd *ft_p, struct netmap_ring *ring,
			   u_int *j, u_int lim, u_int *howmany)
{
	struct netmap_slot *slot = NULL;
	struct nm_vnet_hdr *vh = NULL;
	/* Number of source slots to process. */
	u_int frags = ft_p->ft_frags;
	struct nm_bdg_fwd *ft_end = ft_p + frags;

	/* Source and destination pointers. */
	uint8_t *dst, *src;
	size_t src_len, dst_len;

	u_int j_start = *j;
	u_int dst_slots = 0;

	/* If the source port uses the offloadings, while destination doesn't,
	 * we grab the source virtio-net header and do the offloadings here.
	 */
	if (na->virt_hdr_len && !dst_na->virt_hdr_len) {
		vh = (struct nm_vnet_hdr *)ft_p->ft_buf;
	}

	/* Init source and dest pointers. */
	src = ft_p->ft_buf;
	src_len = ft_p->ft_len;
	slot = &ring->slot[*j];
	dst = NMB(&dst_na->up, slot);
	dst_len = src_len;

	/* We are processing the first input slot and there is a mismatch
	 * between source and destination virt_hdr_len (SHL and DHL).
	 * When the a client is using virtio-net headers, the header length
	 * can be:
	 *    - 10: the header corresponds to the struct nm_vnet_hdr
	 *    - 12: the first 10 bytes correspond to the struct
	 *          virtio_net_hdr, and the last 2 bytes store the
	 *          "mergeable buffers" info, which is an optional
	 *	    hint that can be zeroed for compatibility
	 *
	 * The destination header is therefore built according to the
	 * following table:
	 *
	 * SHL | DHL | destination header
	 * -----------------------------
	 *   0 |  10 | zero
	 *   0 |  12 | zero
	 *  10 |   0 | doesn't exist
	 *  10 |  12 | first 10 bytes are copied from source header, last 2 are zero
	 *  12 |   0 | doesn't exist
	 *  12 |  10 | copied from the first 10 bytes of source header
	 */
	bzero(dst, dst_na->virt_hdr_len);
	if (na->virt_hdr_len && dst_na->virt_hdr_len)
		memcpy(dst, src, sizeof(struct nm_vnet_hdr));
	/* Skip the virtio-net headers. */
	src += na->virt_hdr_len;
	src_len -= na->virt_hdr_len;
	dst += dst_na->virt_hdr_len;
	dst_len = dst_na->virt_hdr_len + src_len;

	/* Here it could be dst_len == 0 (which implies src_len == 0),
	 * so we avoid passing a zero length fragment.
	 */
	if (dst_len == 0) {
		ft_p++;
		src = ft_p->ft_buf;
		src_len = ft_p->ft_len;
		dst_len = src_len;
	}

	if (vh && vh->gso_type != VIRTIO_NET_HDR_GSO_NONE) {
		u_int gso_bytes = 0;
		/* Length of the GSO packet header. */
		u_int gso_hdr_len = 0;
		/* Pointer to the GSO packet header. Assume it is in a single fragment. */
		uint8_t *gso_hdr = NULL;
		/* Index of the current segment. */
		u_int gso_idx = 0;
		/* Payload data bytes segmented so far (e.g. TCP data bytes). */
		u_int segmented_bytes = 0;
		/* Length of the IP header (20 if IPv4, 40 if IPv6). */
		u_int iphlen = 0;
		/* Is this a TCP or an UDP GSO packet? */
		u_int tcp = ((vh->gso_type & ~VIRTIO_NET_HDR_GSO_ECN)
				== VIRTIO_NET_HDR_GSO_UDP) ? 0 : 1;

		/* Segment the GSO packet contained into the input slots (frags). */
		while (ft_p != ft_end) {
			size_t copy;

			/* Grab the GSO header if we don't have it. */
			if (!gso_hdr) {
				uint16_t ethertype;

				gso_hdr = src;

				/* Look at the 'Ethertype' field to see if this packet
				 * is IPv4 or IPv6.
				 */
				ethertype = be16toh(*((uint16_t *)(gso_hdr  + 12)));
				if (ethertype == 0x0800)
					iphlen = 20;
				else /* if (ethertype == 0x86DD) */
					iphlen = 40;
				ND(3, "type=%04x", ethertype);

				/* Compute gso_hdr_len. For TCP we need to read the
				 * content of the 'Data Offset' field.
				 */
				if (tcp) {
					struct nm_tcphdr *tcph =
						(struct nm_tcphdr *)&gso_hdr[14+iphlen];

					gso_hdr_len = 14 + iphlen + 4*(tcph->doff >> 4);
				} else
					gso_hdr_len = 14 + iphlen + 8; /* UDP */

				ND(3, "gso_hdr_len %u gso_mtu %d", gso_hdr_len,
								dst_na->mfs);

				/* Advance source pointers. */
				src += gso_hdr_len;
				src_len -= gso_hdr_len;
				if (src_len == 0) {
					ft_p++;
					if (ft_p == ft_end)
						break;
					src = ft_p->ft_buf;
					src_len = ft_p->ft_len;
					continue;
				}
			}

			/* Fill in the header of the current segment. */
			if (gso_bytes == 0) {
				memcpy(dst, gso_hdr, gso_hdr_len);
				gso_bytes = gso_hdr_len;
			}

			/* Fill in data and update source and dest pointers. */
			copy = src_len;
			if (gso_bytes + copy > dst_na->mfs)
				copy = dst_na->mfs - gso_bytes;
			memcpy(dst + gso_bytes, src, copy);
			gso_bytes += copy;
			src += copy;
			src_len -= copy;

			/* A segment is complete or we have processed all the
			   the GSO payload bytes. */
			if (gso_bytes >= dst_na->mfs ||
				(src_len == 0 && ft_p + 1 == ft_end)) {
				/* After raw segmentation, we must fix some header
				 * fields and compute checksums, in a protocol dependent
				 * way. */
				gso_fix_segment(dst, gso_bytes, gso_idx,
						segmented_bytes,
						src_len == 0 && ft_p + 1 == ft_end,
						tcp, iphlen);

				ND("frame %u completed with %d bytes", gso_idx, (int)gso_bytes);
				slot->len = gso_bytes;
				slot->flags = 0;
				segmented_bytes += gso_bytes - gso_hdr_len;

				dst_slots++;

				/* Next destination slot. */
				*j = nm_next(*j, lim);
				slot = &ring->slot[*j];
				dst = NMB(&dst_na->up, slot);

				gso_bytes = 0;
				gso_idx++;
			}

			/* Next input slot. */
			if (src_len == 0) {
				ft_p++;
				if (ft_p == ft_end)
					break;
				src = ft_p->ft_buf;
				src_len = ft_p->ft_len;
			}
		}
		ND(3, "%d bytes segmented", segmented_bytes);

	} else {
		/* Address of a checksum field into a destination slot. */
		uint16_t *check = NULL;
		/* Accumulator for an unfolded checksum. */
		rawsum_t csum = 0;

		/* Process a non-GSO packet. */

		/* Init 'check' if necessary. */
		if (vh && (vh->flags & VIRTIO_NET_HDR_F_NEEDS_CSUM)) {
			if (unlikely(vh->csum_offset + vh->csum_start > src_len))
				D("invalid checksum request");
			else
				check = (uint16_t *)(dst + vh->csum_start +
						vh->csum_offset);
		}

		while (ft_p != ft_end) {
			/* Init/update the packet checksum if needed. */
			if (vh && (vh->flags & VIRTIO_NET_HDR_F_NEEDS_CSUM)) {
				if (!dst_slots)
					csum = nm_csum_raw(src + vh->csum_start,
								src_len - vh->csum_start, 0);
				else
					csum = nm_csum_raw(src, src_len, csum);
			}

			/* Round to a multiple of 64 */
			src_len = (src_len + 63) & ~63;

			if (ft_p->ft_flags & NS_INDIRECT) {
				if (copyin(src, dst, src_len)) {
					/* Invalid user pointer, pretend len is 0. */
					dst_len = 0;
				}
			} else {
				memcpy(dst, src, (int)src_len);
			}
			slot->len = dst_len;

			dst_slots++;

			/* Next destination slot. */
			*j = nm_next(*j, lim);
			slot = &ring->slot[*j];
			dst = NMB(&dst_na->up, slot);

			/* Next source slot. */
			ft_p++;
			src = ft_p->ft_buf;
			dst_len = src_len = ft_p->ft_len;

		}

		/* Finalize (fold) the checksum if needed. */
		if (check && vh && (vh->flags & VIRTIO_NET_HDR_F_NEEDS_CSUM)) {
			*check = nm_csum_fold(csum);
		}
		ND(3, "using %u dst_slots", dst_slots);

		/* A second pass on the desitations slots to set the slot flags,
		 * using the right number of destination slots.
		 */
		while (j_start != *j) {
			slot = &ring->slot[j_start];
			slot->flags = (dst_slots << 8)| NS_MOREFRAG;
			j_start = nm_next(j_start, lim);
		}
		/* Clear NS_MOREFRAG flag on last entry. */
		slot->flags = (dst_slots << 8);
	}

	/* Update howmany. */
	if (unlikely(dst_slots > *howmany)) {
		dst_slots = *howmany;
		D("Slot allocation error: Should never happen");
	}
	*howmany -= dst_slots;
}
