/*
 * NDEF(NFC Data Exchange Format) routines for Wi-Fi Protected Setup
 *   Reference is "NFCForum-TS-NDEF_1.0 2006-07-24".
 * Copyright (c) 2009-2012, Masashi Honma <masashi.honma@gmail.com>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"
#include "common.h"
#include "wps/wps.h"

#define FLAG_MESSAGE_BEGIN (1 << 7)
#define FLAG_MESSAGE_END (1 << 6)
#define FLAG_CHUNK (1 << 5)
#define FLAG_SHORT_RECORD (1 << 4)
#define FLAG_ID_LENGTH_PRESENT (1 << 3)
#define FLAG_TNF_NFC_FORUM (0x01)
#define FLAG_TNF_RFC2046 (0x02)

struct ndef_record {
	const u8 *type;
	const u8 *id;
	const u8 *payload;
	u8 type_length;
	u8 id_length;
	u32 payload_length;
	u32 total_length;
};

static char wifi_handover_type[] = "application/vnd.wfa.wsc";

static int ndef_parse_record(const u8 *data, u32 size,
			     struct ndef_record *record)
{
	const u8 *pos = data + 1;

	if (size < 2)
		return -1;
	record->type_length = *pos++;
	if (data[0] & FLAG_SHORT_RECORD) {
		if (size < 3)
			return -1;
		record->payload_length = *pos++;
	} else {
		if (size < 6)
			return -1;
		record->payload_length = ntohl(*(u32 *)pos);
		pos += sizeof(u32);
	}

	if (data[0] & FLAG_ID_LENGTH_PRESENT) {
		if ((int) size < pos - data + 1)
			return -1;
		record->id_length = *pos++;
	} else
		record->id_length = 0;

	record->type = record->type_length == 0 ? NULL : pos;
	pos += record->type_length;

	record->id = record->id_length == 0 ? NULL : pos;
	pos += record->id_length;

	record->payload = record->payload_length == 0 ? NULL : pos;
	pos += record->payload_length;

	record->total_length = pos - data;
	if (record->total_length > size)
		return -1;
	return 0;
}


static struct wpabuf * ndef_parse_records(const struct wpabuf *buf,
					  int (*filter)(struct ndef_record *))
{
	struct ndef_record record;
	int len = wpabuf_len(buf);
	const u8 *data = wpabuf_head(buf);

	while (len > 0) {
		if (ndef_parse_record(data, len, &record) < 0) {
			wpa_printf(MSG_ERROR, "NDEF : Failed to parse");
			return NULL;
		}
		if (filter == NULL || filter(&record))
			return wpabuf_alloc_copy(record.payload,
						 record.payload_length);
		data += record.total_length;
		len -= record.total_length;
	}
	wpa_printf(MSG_ERROR, "NDEF : Record not found");
	return NULL;
}


static struct wpabuf * ndef_build_record(u8 flags, void *type,
					 u8 type_length, void *id,
					 u8 id_length,
					 const struct wpabuf *payload)
{
	struct wpabuf *record;
	size_t total_len;
	int short_record;
	u8 local_flag;
	size_t payload_length = wpabuf_len(payload);

	short_record = payload_length < 256 ? 1 : 0;

	total_len = 2; /* flag + type length */
	/* payload length */
	total_len += short_record ? sizeof(u8) : sizeof(u32);
	if (id_length > 0)
		total_len += 1;
	total_len += type_length + id_length + payload_length;
	record = wpabuf_alloc(total_len);
	if (record == NULL) {
		wpa_printf(MSG_ERROR, "NDEF : Failed to allocate "
			   "record for build");
		return NULL;
	}

	local_flag = flags;
	if (id_length > 0)
		local_flag |= FLAG_ID_LENGTH_PRESENT;
	if (short_record)
		local_flag |= FLAG_SHORT_RECORD;
	wpabuf_put_u8(record, local_flag);

	wpabuf_put_u8(record, type_length);

	if (short_record)
		wpabuf_put_u8(record, payload_length);
	else
		wpabuf_put_be32(record, payload_length);

	if (id_length > 0)
		wpabuf_put_u8(record, id_length);
	wpabuf_put_data(record, type, type_length);
	wpabuf_put_data(record, id, id_length);
	wpabuf_put_buf(record, payload);
	return record;
}


static int wifi_filter(struct ndef_record *record)
{
	if (record->type_length != os_strlen(wifi_handover_type))
		return 0;
	if (os_memcmp(record->type, wifi_handover_type,
		      os_strlen(wifi_handover_type)) != 0)
		return 0;
	return 1;
}


struct wpabuf * ndef_parse_wifi(const struct wpabuf *buf)
{
	return ndef_parse_records(buf, wifi_filter);
}


struct wpabuf * ndef_build_wifi(const struct wpabuf *buf)
{
	return ndef_build_record(FLAG_MESSAGE_BEGIN | FLAG_MESSAGE_END |
				 FLAG_TNF_RFC2046, wifi_handover_type,
				 os_strlen(wifi_handover_type), NULL, 0, buf);
}


struct wpabuf * ndef_build_wifi_hr(void)
{
	struct wpabuf *rn, *cr, *ac_payload, *ac, *hr_payload, *hr;
	struct wpabuf *carrier, *hc;

	rn = wpabuf_alloc(2);
	if (rn == NULL)
		return NULL;
	wpabuf_put_be16(rn, os_random() & 0xffff);

	cr = ndef_build_record(FLAG_MESSAGE_BEGIN | FLAG_TNF_NFC_FORUM, "cr", 2,
			       NULL, 0, rn);
	wpabuf_free(rn);

	if (cr == NULL)
		return NULL;

	ac_payload = wpabuf_alloc(4);
	if (ac_payload == NULL) {
		wpabuf_free(cr);
		return NULL;
	}
	wpabuf_put_u8(ac_payload, 0x01); /* Carrier Flags: CRS=1 "active" */
	wpabuf_put_u8(ac_payload, 0x01); /* Carrier Data Reference Length */
	wpabuf_put_u8(ac_payload, '0'); /* Carrier Data Reference: "0" */
	wpabuf_put_u8(ac_payload, 0); /* Aux Data Reference Count */

	ac = ndef_build_record(FLAG_MESSAGE_END | FLAG_TNF_NFC_FORUM, "ac", 2,
			       NULL, 0, ac_payload);
	wpabuf_free(ac_payload);
	if (ac == NULL) {
		wpabuf_free(cr);
		return NULL;
	}

	hr_payload = wpabuf_alloc(1 + wpabuf_len(cr) + wpabuf_len(ac));
	if (hr_payload == NULL) {
		wpabuf_free(cr);
		wpabuf_free(ac);
		return NULL;
	}

	wpabuf_put_u8(hr_payload, 0x12); /* Connection Handover Version 1.2 */
	wpabuf_put_buf(hr_payload, cr);
	wpabuf_put_buf(hr_payload, ac);
	wpabuf_free(cr);
	wpabuf_free(ac);

	hr = ndef_build_record(FLAG_MESSAGE_BEGIN | FLAG_TNF_NFC_FORUM, "Hr", 2,
			       NULL, 0, hr_payload);
	wpabuf_free(hr_payload);
	if (hr == NULL)
		return NULL;

	carrier = wpabuf_alloc(2 + os_strlen(wifi_handover_type));
	if (carrier == NULL) {
		wpabuf_free(hr);
		return NULL;
	}
	wpabuf_put_u8(carrier, 0x02); /* Carrier Type Format */
	wpabuf_put_u8(carrier, os_strlen(wifi_handover_type));
	wpabuf_put_str(carrier, wifi_handover_type);

	hc = ndef_build_record(FLAG_MESSAGE_END | FLAG_TNF_NFC_FORUM, "Hc", 2,
			       "0", 1, carrier);
	wpabuf_free(carrier);
	if (hc == NULL) {
		wpabuf_free(hr);
		return NULL;
	}

	return wpabuf_concat(hr, hc);
}
