/*
 * PCAP capture file writer
 * Copyright (c) 2010-2019, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"
#include <pcap.h>
#include <pcap-bpf.h>

#include "utils/common.h"
#include "wlantest.h"
#include "common/qca-vendor.h"


int write_pcap_init(struct wlantest *wt, const char *fname)
{
	int linktype = wt->ethernet ? DLT_EN10MB : DLT_IEEE802_11_RADIO;

	wt->write_pcap = pcap_open_dead(linktype, 4000);
	if (wt->write_pcap == NULL)
		return -1;
	wt->write_pcap_dumper = pcap_dump_open(wt->write_pcap, fname);
	if (wt->write_pcap_dumper == NULL) {
		pcap_close(wt->write_pcap);
		wt->write_pcap = NULL;
		return -1;
	}

	wpa_printf(MSG_DEBUG, "Writing PCAP dump to '%s'", fname);

	return 0;
}


void write_pcap_deinit(struct wlantest *wt)
{
	if (wt->write_pcap_dumper) {
		pcap_dump_close(wt->write_pcap_dumper);
		wt->write_pcap_dumper = NULL;
	}
	if (wt->write_pcap) {
		pcap_close(wt->write_pcap);
		wt->write_pcap = NULL;
	}
}


void write_pcap_captured(struct wlantest *wt, const u8 *buf, size_t len)
{
	struct pcap_pkthdr h;

	if (!wt->write_pcap_dumper)
		return;

	os_memset(&h, 0, sizeof(h));
	gettimeofday(&wt->write_pcap_time, NULL);
	h.ts = wt->write_pcap_time;
	h.caplen = len;
	h.len = len;
	pcap_dump(wt->write_pcap_dumper, &h, buf);
	if (wt->pcap_no_buffer)
		pcap_dump_flush(wt->write_pcap_dumper);
}


void write_pcap_decrypted(struct wlantest *wt, const u8 *buf1, size_t len1,
			  const u8 *buf2, size_t len2)
{
	struct pcap_pkthdr h;
	u8 rtap[] = {
		0x00 /* rev */,
		0x00 /* pad */,
		0x0e, 0x00, /* header len */
		0x00, 0x00, 0x00, 0x40, /* present flags */
		0x00, 0x13, 0x74, QCA_RADIOTAP_VID_WLANTEST,
		0x00, 0x00
	};
	u8 *buf;
	size_t len;

	if (!wt->write_pcap_dumper && !wt->pcapng)
		return;

	os_free(wt->decrypted);
	len = sizeof(rtap) + len1 + len2;
	wt->decrypted = buf = os_malloc(len);
	if (buf == NULL)
		return;
	wt->decrypted_len = len;
	os_memcpy(buf, rtap, sizeof(rtap));
	if (buf1) {
		os_memcpy(buf + sizeof(rtap), buf1, len1);
		buf[sizeof(rtap) + 1] &= ~0x40; /* Clear Protected flag */
	}
	if (buf2)
		os_memcpy(buf + sizeof(rtap) + len1, buf2, len2);

	if (!wt->write_pcap_dumper)
		return;

	os_memset(&h, 0, sizeof(h));
	h.ts = wt->write_pcap_time;
	h.caplen = len;
	h.len = len;
	pcap_dump(wt->write_pcap_dumper, &h, buf);
	if (wt->pcap_no_buffer)
		pcap_dump_flush(wt->write_pcap_dumper);
}


struct pcapng_section_header {
	u32 block_type; /* 0x0a0d0d0a */
	u32 block_total_len;
	u32 byte_order_magic;
	u16 major_version;
	u16 minor_version;
	u64 section_len;
	u32 block_total_len2;
} STRUCT_PACKED;

struct pcapng_interface_description {
	u32 block_type; /* 0x00000001 */
	u32 block_total_len;
	u16 link_type;
	u16 reserved;
	u32 snap_len;
	u32 block_total_len2;
} STRUCT_PACKED;

struct pcapng_enhanced_packet {
	u32 block_type; /* 0x00000006 */
	u32 block_total_len;
	u32 interface_id;
	u32 timestamp_high;
	u32 timestamp_low;
	u32 captured_len;
	u32 packet_len;
	/* Packet data - aligned to 32 bits */
	/* Options (variable) */
	/* Block Total Length copy */
} STRUCT_PACKED;

#define PCAPNG_BYTE_ORDER_MAGIC 0x1a2b3c4d
#define PCAPNG_BLOCK_IFACE_DESC 0x00000001
#define PCAPNG_BLOCK_PACKET 0x00000002
#define PCAPNG_BLOCK_SIMPLE_PACKET 0x00000003
#define PCAPNG_BLOCK_NAME_RESOLUTION 0x00000004
#define PCAPNG_BLOCK_INTERFACE_STATISTICS 0x00000005
#define PCAPNG_BLOCK_ENHANCED_PACKET 0x00000006
#define PCAPNG_BLOCK_SECTION_HEADER 0x0a0d0d0a

#define LINKTYPE_IEEE802_11 105
#define LINKTYPE_IEEE802_11_RADIO 127

#define PAD32(a) ((4 - ((a) & 3)) & 3)
#define ALIGN32(a) ((a) + PAD32((a)))


int write_pcapng_init(struct wlantest *wt, const char *fname)
{
	struct pcapng_section_header hdr;
	struct pcapng_interface_description desc;

	wt->pcapng = fopen(fname, "wb");
	if (wt->pcapng == NULL)
		return -1;

	wpa_printf(MSG_DEBUG, "Writing PCAPNG dump to '%s'", fname);

	os_memset(&hdr, 0, sizeof(hdr));
	hdr.block_type = PCAPNG_BLOCK_SECTION_HEADER;
	hdr.block_total_len = sizeof(hdr);
	hdr.byte_order_magic = PCAPNG_BYTE_ORDER_MAGIC;
	hdr.major_version = 1;
	hdr.minor_version = 0;
	hdr.section_len = -1;
	hdr.block_total_len2 = hdr.block_total_len;
	fwrite(&hdr, sizeof(hdr), 1, wt->pcapng);

	os_memset(&desc, 0, sizeof(desc));
	desc.block_type = PCAPNG_BLOCK_IFACE_DESC;
	desc.block_total_len = sizeof(desc);
	desc.block_total_len2 = desc.block_total_len;
	desc.link_type = wt->ethernet ? DLT_EN10MB : LINKTYPE_IEEE802_11_RADIO;
	desc.snap_len = 65535;
	fwrite(&desc, sizeof(desc), 1, wt->pcapng);
	if (wt->pcap_no_buffer)
		fflush(wt->pcapng);

	return 0;
}


void write_pcapng_deinit(struct wlantest *wt)
{
	if (wt->pcapng) {
		fclose(wt->pcapng);
		wt->pcapng = NULL;
	}
}


static u8 * pcapng_add_comments(struct wlantest *wt, u8 *pos)
{
	size_t i;
	u16 *len;

	if (!wt->num_notes)
		return pos;

	*((u16 *) pos) = 1 /* opt_comment */;
	pos += 2;
	len = (u16 *) pos /* length to be filled in */;
	pos += 2;

	for (i = 0; i < wt->num_notes; i++) {
		size_t nlen = os_strlen(wt->notes[i]);
		if (i > 0)
			*pos++ = '\n';
		os_memcpy(pos, wt->notes[i], nlen);
		pos += nlen;
	}
	*len = pos - (u8 *) len - 2;
	pos += PAD32(*len);

	*((u16 *) pos) = 0 /* opt_endofopt */;
	pos += 2;
	*((u16 *) pos) = 0;
	pos += 2;

	return pos;
}


static void write_pcapng_decrypted(struct wlantest *wt)
{
	size_t len;
	struct pcapng_enhanced_packet *pkt;
	u8 *pos;
	u32 *block_len;

	if (!wt->pcapng || wt->decrypted == NULL)
		return;

	add_note(wt, MSG_EXCESSIVE, "decrypted version of the previous frame");

	len = sizeof(*pkt) + wt->decrypted_len + 100 + notes_len(wt, 32);
	pkt = os_zalloc(len);
	if (pkt == NULL)
		return;

	pkt->block_type = PCAPNG_BLOCK_ENHANCED_PACKET;
	pkt->interface_id = 0;
	pkt->timestamp_high = wt->write_pcapng_time_high;
	pkt->timestamp_low = wt->write_pcapng_time_low;
	pkt->captured_len = wt->decrypted_len;
	pkt->packet_len = wt->decrypted_len;

	pos = (u8 *) (pkt + 1);

	os_memcpy(pos, wt->decrypted, wt->decrypted_len);
	pos += ALIGN32(wt->decrypted_len);

	pos = pcapng_add_comments(wt, pos);

	block_len = (u32 *) pos;
	pos += 4;
	*block_len = pkt->block_total_len = pos - (u8 *) pkt;

	fwrite(pkt, pos - (u8 *) pkt, 1, wt->pcapng);
	if (wt->pcap_no_buffer)
		fflush(wt->pcapng);

	os_free(pkt);
}


void write_pcapng_write_read(struct wlantest *wt, int dlt,
			     struct pcap_pkthdr *hdr, const u8 *data)
{
	struct pcapng_enhanced_packet *pkt;
	u8 *pos;
	u32 *block_len;
	u64 timestamp;
	size_t len, datalen = hdr->caplen;
	u8 rtap[] = {
		0x00 /* rev */,
		0x00 /* pad */,
		0x0a, 0x00, /* header len */
		0x02, 0x00, 0x00, 0x00, /* present flags */
		0x00, /* flags */
		0x00 /* pad */
	};

	if (wt->assume_fcs)
		rtap[8] |= 0x10;

	if (!wt->pcapng)
		return;

	len = sizeof(*pkt) + hdr->len + 100 + notes_len(wt, 32) + sizeof(rtap);
	pkt = os_zalloc(len);
	if (pkt == NULL)
		return;

	pkt->block_type = PCAPNG_BLOCK_ENHANCED_PACKET;
	pkt->interface_id = 0;
	timestamp = 1000000 * hdr->ts.tv_sec + hdr->ts.tv_usec;
	pkt->timestamp_high = timestamp >> 32;
	pkt->timestamp_low = timestamp & 0xffffffff;
	wt->write_pcapng_time_high = pkt->timestamp_high;
	wt->write_pcapng_time_low = pkt->timestamp_low;
	pkt->captured_len = hdr->caplen;
	pkt->packet_len = hdr->len;

	pos = (u8 *) (pkt + 1);

	switch (dlt) {
	case DLT_EN10MB:
	case DLT_IEEE802_11_RADIO:
		break;
	case DLT_PRISM_HEADER:
		/* remove prism header (could be kept ... lazy) */
		pkt->captured_len -= WPA_GET_LE32(data + 4);
		pkt->packet_len -= WPA_GET_LE32(data + 4);
		datalen -= WPA_GET_LE32(data + 4);
		data += WPA_GET_LE32(data + 4);
		/* fall through */
	case DLT_IEEE802_11:
		pkt->captured_len += sizeof(rtap);
		pkt->packet_len += sizeof(rtap);
		os_memcpy(pos, &rtap, sizeof(rtap));
		pos += sizeof(rtap);
		break;
	default:
		return;
	}

	os_memcpy(pos, data, datalen);
	pos += datalen + PAD32(pkt->captured_len);
	pos = pcapng_add_comments(wt, pos);

	block_len = (u32 *) pos;
	pos += 4;
	*block_len = pkt->block_total_len = pos - (u8 *) pkt;

	fwrite(pkt, pos - (u8 *) pkt, 1, wt->pcapng);
	if (wt->pcap_no_buffer)
		fflush(wt->pcapng);

	os_free(pkt);

	write_pcapng_decrypted(wt);
}


void write_pcapng_captured(struct wlantest *wt, const u8 *buf, size_t len)
{
	struct pcap_pkthdr h;

	if (!wt->pcapng)
		return;

	os_memset(&h, 0, sizeof(h));
	gettimeofday(&h.ts, NULL);
	h.caplen = len;
	h.len = len;
	write_pcapng_write_read(wt, wt->ethernet ? DLT_EN10MB :
				DLT_IEEE802_11_RADIO, &h, buf);
}
