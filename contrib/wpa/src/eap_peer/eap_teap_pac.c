/*
 * EAP peer method: EAP-TEAP PAC file processing
 * Copyright (c) 2004-2019, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#include "common.h"
#include "eap_config.h"
#include "eap_i.h"
#include "eap_teap_pac.h"

/* TODO: encrypt PAC-Key in the PAC file */


/* Text data format */
static const char *pac_file_hdr =
	"wpa_supplicant EAP-TEAP PAC file - version 1";

/*
 * Binary data format
 * 4-octet magic value: 6A E4 92 1C
 * 2-octet version (big endian)
 * <version specific data>
 *
 * version=0:
 * Sequence of PAC entries:
 *   2-octet PAC-Type (big endian)
 *   32-octet PAC-Key
 *   2-octet PAC-Opaque length (big endian)
 *   <variable len> PAC-Opaque data (length bytes)
 *   2-octet PAC-Info length (big endian)
 *   <variable len> PAC-Info data (length bytes)
 */

#define EAP_TEAP_PAC_BINARY_MAGIC 0x6ae4921c
#define EAP_TEAP_PAC_BINARY_FORMAT_VERSION 0


/**
 * eap_teap_free_pac - Free PAC data
 * @pac: Pointer to the PAC entry
 *
 * Note that the PAC entry must not be in a list since this function does not
 * remove the list links.
 */
void eap_teap_free_pac(struct eap_teap_pac *pac)
{
	os_free(pac->pac_opaque);
	os_free(pac->pac_info);
	os_free(pac->a_id);
	os_free(pac->i_id);
	os_free(pac->a_id_info);
	os_free(pac);
}


/**
 * eap_teap_get_pac - Get a PAC entry based on A-ID
 * @pac_root: Pointer to root of the PAC list
 * @a_id: A-ID to search for
 * @a_id_len: Length of A-ID
 * @pac_type: PAC-Type to search for
 * Returns: Pointer to the PAC entry, or %NULL if A-ID not found
 */
struct eap_teap_pac * eap_teap_get_pac(struct eap_teap_pac *pac_root,
				       const u8 *a_id, size_t a_id_len,
				       u16 pac_type)
{
	struct eap_teap_pac *pac = pac_root;

	while (pac) {
		if (pac->pac_type == pac_type && pac->a_id_len == a_id_len &&
		    os_memcmp(pac->a_id, a_id, a_id_len) == 0) {
			return pac;
		}
		pac = pac->next;
	}
	return NULL;
}


static void eap_teap_remove_pac(struct eap_teap_pac **pac_root,
				struct eap_teap_pac **pac_current,
				const u8 *a_id, size_t a_id_len, u16 pac_type)
{
	struct eap_teap_pac *pac, *prev;

	pac = *pac_root;
	prev = NULL;

	while (pac) {
		if (pac->pac_type == pac_type && pac->a_id_len == a_id_len &&
		    os_memcmp(pac->a_id, a_id, a_id_len) == 0) {
			if (!prev)
				*pac_root = pac->next;
			else
				prev->next = pac->next;
			if (*pac_current == pac)
				*pac_current = NULL;
			eap_teap_free_pac(pac);
			break;
		}
		prev = pac;
		pac = pac->next;
	}
}


static int eap_teap_copy_buf(u8 **dst, size_t *dst_len,
			     const u8 *src, size_t src_len)
{
	if (src) {
		*dst = os_memdup(src, src_len);
		if (!(*dst))
			return -1;
		*dst_len = src_len;
	}
	return 0;
}


/**
 * eap_teap_add_pac - Add a copy of a PAC entry to a list
 * @pac_root: Pointer to PAC list root pointer
 * @pac_current: Pointer to the current PAC pointer
 * @entry: New entry to clone and add to the list
 * Returns: 0 on success, -1 on failure
 *
 * This function makes a clone of the given PAC entry and adds this copied
 * entry to the list (pac_root). If an old entry for the same A-ID is found,
 * it will be removed from the PAC list and in this case, pac_current entry
 * is set to %NULL if it was the removed entry.
 */
int eap_teap_add_pac(struct eap_teap_pac **pac_root,
		     struct eap_teap_pac **pac_current,
		     struct eap_teap_pac *entry)
{
	struct eap_teap_pac *pac;

	if (!entry || !entry->a_id)
		return -1;

	/* Remove a possible old entry for the matching A-ID. */
	eap_teap_remove_pac(pac_root, pac_current,
			    entry->a_id, entry->a_id_len, entry->pac_type);

	/* Allocate a new entry and add it to the list of PACs. */
	pac = os_zalloc(sizeof(*pac));
	if (!pac)
		return -1;

	pac->pac_type = entry->pac_type;
	os_memcpy(pac->pac_key, entry->pac_key, EAP_TEAP_PAC_KEY_LEN);
	if (eap_teap_copy_buf(&pac->pac_opaque, &pac->pac_opaque_len,
			      entry->pac_opaque, entry->pac_opaque_len) < 0 ||
	    eap_teap_copy_buf(&pac->pac_info, &pac->pac_info_len,
			      entry->pac_info, entry->pac_info_len) < 0 ||
	    eap_teap_copy_buf(&pac->a_id, &pac->a_id_len,
			      entry->a_id, entry->a_id_len) < 0 ||
	    eap_teap_copy_buf(&pac->i_id, &pac->i_id_len,
			      entry->i_id, entry->i_id_len) < 0 ||
	    eap_teap_copy_buf(&pac->a_id_info, &pac->a_id_info_len,
			      entry->a_id_info, entry->a_id_info_len) < 0) {
		eap_teap_free_pac(pac);
		return -1;
	}

	pac->next = *pac_root;
	*pac_root = pac;

	return 0;
}


struct eap_teap_read_ctx {
	FILE *f;
	const char *pos;
	const char *end;
	int line;
	char *buf;
	size_t buf_len;
};

static int eap_teap_read_line(struct eap_teap_read_ctx *rc, char **value)
{
	char *pos;

	rc->line++;
	if (rc->f) {
		if (fgets(rc->buf, rc->buf_len, rc->f) == NULL)
			return -1;
	} else {
		const char *l_end;
		size_t len;

		if (rc->pos >= rc->end)
			return -1;
		l_end = rc->pos;
		while (l_end < rc->end && *l_end != '\n')
			l_end++;
		len = l_end - rc->pos;
		if (len >= rc->buf_len)
			len = rc->buf_len - 1;
		os_memcpy(rc->buf, rc->pos, len);
		rc->buf[len] = '\0';
		rc->pos = l_end + 1;
	}

	rc->buf[rc->buf_len - 1] = '\0';
	pos = rc->buf;
	while (*pos != '\0') {
		if (*pos == '\n' || *pos == '\r') {
			*pos = '\0';
			break;
		}
		pos++;
	}

	pos = os_strchr(rc->buf, '=');
	if (pos)
		*pos++ = '\0';
	*value = pos;

	return 0;
}


static u8 * eap_teap_parse_hex(const char *value, size_t *len)
{
	int hlen;
	u8 *buf;

	if (!value)
		return NULL;
	hlen = os_strlen(value);
	if (hlen & 1)
		return NULL;
	*len = hlen / 2;
	buf = os_malloc(*len);
	if (!buf)
		return NULL;
	if (hexstr2bin(value, buf, *len)) {
		os_free(buf);
		return NULL;
	}
	return buf;
}


static int eap_teap_init_pac_data(struct eap_sm *sm, const char *pac_file,
				  struct eap_teap_read_ctx *rc)
{
	os_memset(rc, 0, sizeof(*rc));

	rc->buf_len = 2048;
	rc->buf = os_malloc(rc->buf_len);
	if (!rc->buf)
		return -1;

	if (os_strncmp(pac_file, "blob://", 7) == 0) {
		const struct wpa_config_blob *blob;

		blob = eap_get_config_blob(sm, pac_file + 7);
		if (!blob) {
			wpa_printf(MSG_INFO,
				   "EAP-TEAP: No PAC blob '%s' - assume no PAC entries have been provisioned",
				   pac_file + 7);
			os_free(rc->buf);
			return -1;
		}
		rc->pos = (char *) blob->data;
		rc->end = (char *) blob->data + blob->len;
	} else {
		rc->f = fopen(pac_file, "rb");
		if (!rc->f) {
			wpa_printf(MSG_INFO,
				   "EAP-TEAP: No PAC file '%s' - assume no PAC entries have been provisioned",
				   pac_file);
			os_free(rc->buf);
			return -1;
		}
	}

	return 0;
}


static void eap_teap_deinit_pac_data(struct eap_teap_read_ctx *rc)
{
	os_free(rc->buf);
	if (rc->f)
		fclose(rc->f);
}


static const char * eap_teap_parse_start(struct eap_teap_pac **pac)
{
	if (*pac)
		return "START line without END";

	*pac = os_zalloc(sizeof(struct eap_teap_pac));
	if (!(*pac))
		return "No memory for PAC entry";
	(*pac)->pac_type = PAC_TYPE_TUNNEL_PAC;
	return NULL;
}


static const char * eap_teap_parse_end(struct eap_teap_pac **pac_root,
				       struct eap_teap_pac **pac)
{
	if (!(*pac))
		return "END line without START";
	if (*pac_root) {
		struct eap_teap_pac *end = *pac_root;

		while (end->next)
			end = end->next;
		end->next = *pac;
	} else
		*pac_root = *pac;

	*pac = NULL;
	return NULL;
}


static const char * eap_teap_parse_pac_type(struct eap_teap_pac *pac,
					    char *pos)
{
	if (!pos)
		return "Cannot parse pac type";
	pac->pac_type = atoi(pos);
	if (pac->pac_type != PAC_TYPE_TUNNEL_PAC)
		return "Unrecognized PAC-Type";

	return NULL;
}


static const char * eap_teap_parse_pac_key(struct eap_teap_pac *pac, char *pos)
{
	u8 *key;
	size_t key_len;

	key = eap_teap_parse_hex(pos, &key_len);
	if (!key || key_len != EAP_TEAP_PAC_KEY_LEN) {
		os_free(key);
		return "Invalid PAC-Key";
	}

	os_memcpy(pac->pac_key, key, EAP_TEAP_PAC_KEY_LEN);
	os_free(key);

	return NULL;
}


static const char * eap_teap_parse_pac_opaque(struct eap_teap_pac *pac,
					      char *pos)
{
	os_free(pac->pac_opaque);
	pac->pac_opaque = eap_teap_parse_hex(pos, &pac->pac_opaque_len);
	if (!pac->pac_opaque)
		return "Invalid PAC-Opaque";
	return NULL;
}


static const char * eap_teap_parse_a_id(struct eap_teap_pac *pac, char *pos)
{
	os_free(pac->a_id);
	pac->a_id = eap_teap_parse_hex(pos, &pac->a_id_len);
	if (!pac->a_id)
		return "Invalid A-ID";
	return NULL;
}


static const char * eap_teap_parse_i_id(struct eap_teap_pac *pac, char *pos)
{
	os_free(pac->i_id);
	pac->i_id = eap_teap_parse_hex(pos, &pac->i_id_len);
	if (!pac->i_id)
		return "Invalid I-ID";
	return NULL;
}


static const char * eap_teap_parse_a_id_info(struct eap_teap_pac *pac,
					     char *pos)
{
	os_free(pac->a_id_info);
	pac->a_id_info = eap_teap_parse_hex(pos, &pac->a_id_info_len);
	if (!pac->a_id_info)
		return "Invalid A-ID-Info";
	return NULL;
}


/**
 * eap_teap_load_pac - Load PAC entries (text format)
 * @sm: Pointer to EAP state machine allocated with eap_peer_sm_init()
 * @pac_root: Pointer to root of the PAC list (to be filled)
 * @pac_file: Name of the PAC file/blob to load
 * Returns: 0 on success, -1 on failure
 */
int eap_teap_load_pac(struct eap_sm *sm, struct eap_teap_pac **pac_root,
		      const char *pac_file)
{
	struct eap_teap_read_ctx rc;
	struct eap_teap_pac *pac = NULL;
	int count = 0;
	char *pos;
	const char *err = NULL;

	if (!pac_file)
		return -1;

	if (eap_teap_init_pac_data(sm, pac_file, &rc) < 0)
		return 0;

	if (eap_teap_read_line(&rc, &pos) < 0) {
		/* empty file - assume it is fine to overwrite */
		eap_teap_deinit_pac_data(&rc);
		return 0;
	}
	if (os_strcmp(pac_file_hdr, rc.buf) != 0)
		err = "Unrecognized header line";

	while (!err && eap_teap_read_line(&rc, &pos) == 0) {
		if (os_strcmp(rc.buf, "START") == 0)
			err = eap_teap_parse_start(&pac);
		else if (os_strcmp(rc.buf, "END") == 0) {
			err = eap_teap_parse_end(pac_root, &pac);
			count++;
		} else if (!pac)
			err = "Unexpected line outside START/END block";
		else if (os_strcmp(rc.buf, "PAC-Type") == 0)
			err = eap_teap_parse_pac_type(pac, pos);
		else if (os_strcmp(rc.buf, "PAC-Key") == 0)
			err = eap_teap_parse_pac_key(pac, pos);
		else if (os_strcmp(rc.buf, "PAC-Opaque") == 0)
			err = eap_teap_parse_pac_opaque(pac, pos);
		else if (os_strcmp(rc.buf, "A-ID") == 0)
			err = eap_teap_parse_a_id(pac, pos);
		else if (os_strcmp(rc.buf, "I-ID") == 0)
			err = eap_teap_parse_i_id(pac, pos);
		else if (os_strcmp(rc.buf, "A-ID-Info") == 0)
			err = eap_teap_parse_a_id_info(pac, pos);
	}

	if (pac) {
		if (!err)
			err = "PAC block not terminated with END";
		eap_teap_free_pac(pac);
	}

	eap_teap_deinit_pac_data(&rc);

	if (err) {
		wpa_printf(MSG_INFO, "EAP-TEAP: %s in '%s:%d'",
			   err, pac_file, rc.line);
		return -1;
	}

	wpa_printf(MSG_DEBUG, "EAP-TEAP: Read %d PAC entries from '%s'",
		   count, pac_file);

	return 0;
}


static void eap_teap_write(char **buf, char **pos, size_t *buf_len,
			   const char *field, const u8 *data,
			   size_t len, int txt)
{
	size_t i, need;
	int ret;
	char *end;

	if (!data || !buf || !(*buf) || !pos || !(*pos) || *pos < *buf)
		return;

	need = os_strlen(field) + len * 2 + 30;
	if (txt)
		need += os_strlen(field) + len + 20;

	if (*pos - *buf + need > *buf_len) {
		char *nbuf = os_realloc(*buf, *buf_len + need);

		if (!nbuf) {
			os_free(*buf);
			*buf = NULL;
			return;
		}
		*pos = nbuf + (*pos - *buf);
		*buf = nbuf;
		*buf_len += need;
	}
	end = *buf + *buf_len;

	ret = os_snprintf(*pos, end - *pos, "%s=", field);
	if (os_snprintf_error(end - *pos, ret))
		return;
	*pos += ret;
	*pos += wpa_snprintf_hex(*pos, end - *pos, data, len);
	ret = os_snprintf(*pos, end - *pos, "\n");
	if (os_snprintf_error(end - *pos, ret))
		return;
	*pos += ret;

	if (txt) {
		ret = os_snprintf(*pos, end - *pos, "%s-txt=", field);
		if (os_snprintf_error(end - *pos, ret))
			return;
		*pos += ret;
		for (i = 0; i < len; i++) {
			ret = os_snprintf(*pos, end - *pos, "%c", data[i]);
			if (os_snprintf_error(end - *pos, ret))
				return;
			*pos += ret;
		}
		ret = os_snprintf(*pos, end - *pos, "\n");
		if (os_snprintf_error(end - *pos, ret))
			return;
		*pos += ret;
	}
}


static int eap_teap_write_pac(struct eap_sm *sm, const char *pac_file,
			      char *buf, size_t len)
{
	if (os_strncmp(pac_file, "blob://", 7) == 0) {
		struct wpa_config_blob *blob;

		blob = os_zalloc(sizeof(*blob));
		if (!blob)
			return -1;
		blob->data = (u8 *) buf;
		blob->len = len;
		buf = NULL;
		blob->name = os_strdup(pac_file + 7);
		if (!blob->name) {
			os_free(blob);
			return -1;
		}
		eap_set_config_blob(sm, blob);
	} else {
		FILE *f;

		f = fopen(pac_file, "wb");
		if (!f) {
			wpa_printf(MSG_INFO,
				   "EAP-TEAP: Failed to open PAC file '%s' for writing",
				   pac_file);
			return -1;
		}
		if (fwrite(buf, 1, len, f) != len) {
			wpa_printf(MSG_INFO,
				   "EAP-TEAP: Failed to write all PACs into '%s'",
				   pac_file);
			fclose(f);
			return -1;
		}
		os_free(buf);
		fclose(f);
	}

	return 0;
}


static int eap_teap_add_pac_data(struct eap_teap_pac *pac, char **buf,
				 char **pos, size_t *buf_len)
{
	int ret;

	ret = os_snprintf(*pos, *buf + *buf_len - *pos,
			  "START\nPAC-Type=%d\n", pac->pac_type);
	if (os_snprintf_error(*buf + *buf_len - *pos, ret))
		return -1;

	*pos += ret;
	eap_teap_write(buf, pos, buf_len, "PAC-Key",
		       pac->pac_key, EAP_TEAP_PAC_KEY_LEN, 0);
	eap_teap_write(buf, pos, buf_len, "PAC-Opaque",
		       pac->pac_opaque, pac->pac_opaque_len, 0);
	eap_teap_write(buf, pos, buf_len, "PAC-Info",
		       pac->pac_info, pac->pac_info_len, 0);
	eap_teap_write(buf, pos, buf_len, "A-ID",
		       pac->a_id, pac->a_id_len, 0);
	eap_teap_write(buf, pos, buf_len, "I-ID",
		       pac->i_id, pac->i_id_len, 1);
	eap_teap_write(buf, pos, buf_len, "A-ID-Info",
		       pac->a_id_info, pac->a_id_info_len, 1);
	if (!(*buf)) {
		wpa_printf(MSG_DEBUG, "EAP-TEAP: No memory for PAC data");
		return -1;
	}
	ret = os_snprintf(*pos, *buf + *buf_len - *pos, "END\n");
	if (os_snprintf_error(*buf + *buf_len - *pos, ret))
		return -1;
	*pos += ret;

	return 0;
}


/**
 * eap_teap_save_pac - Save PAC entries (text format)
 * @sm: Pointer to EAP state machine allocated with eap_peer_sm_init()
 * @pac_root: Root of the PAC list
 * @pac_file: Name of the PAC file/blob
 * Returns: 0 on success, -1 on failure
 */
int eap_teap_save_pac(struct eap_sm *sm, struct eap_teap_pac *pac_root,
		      const char *pac_file)
{
	struct eap_teap_pac *pac;
	int ret, count = 0;
	char *buf, *pos;
	size_t buf_len;

	if (!pac_file)
		return -1;

	buf_len = 1024;
	pos = buf = os_malloc(buf_len);
	if (!buf)
		return -1;

	ret = os_snprintf(pos, buf + buf_len - pos, "%s\n", pac_file_hdr);
	if (os_snprintf_error(buf + buf_len - pos, ret)) {
		os_free(buf);
		return -1;
	}
	pos += ret;

	pac = pac_root;
	while (pac) {
		if (eap_teap_add_pac_data(pac, &buf, &pos, &buf_len)) {
			os_free(buf);
			return -1;
		}
		count++;
		pac = pac->next;
	}

	if (eap_teap_write_pac(sm, pac_file, buf, pos - buf)) {
		os_free(buf);
		return -1;
	}

	wpa_printf(MSG_DEBUG, "EAP-TEAP: Wrote %d PAC entries into '%s'",
		   count, pac_file);

	return 0;
}


/**
 * eap_teap_pac_list_truncate - Truncate a PAC list to the given length
 * @pac_root: Root of the PAC list
 * @max_len: Maximum length of the list (>= 1)
 * Returns: Number of PAC entries removed
 */
size_t eap_teap_pac_list_truncate(struct eap_teap_pac *pac_root,
				  size_t max_len)
{
	struct eap_teap_pac *pac, *prev;
	size_t count;

	pac = pac_root;
	prev = NULL;
	count = 0;

	while (pac) {
		count++;
		if (count > max_len)
			break;
		prev = pac;
		pac = pac->next;
	}

	if (count <= max_len || !prev)
		return 0;

	count = 0;
	prev->next = NULL;

	while (pac) {
		prev = pac;
		pac = pac->next;
		eap_teap_free_pac(prev);
		count++;
	}

	return count;
}


static void eap_teap_pac_get_a_id(struct eap_teap_pac *pac)
{
	u8 *pos, *end;
	u16 type, len;

	pos = pac->pac_info;
	end = pos + pac->pac_info_len;

	while (end - pos > 4) {
		type = WPA_GET_BE16(pos);
		pos += 2;
		len = WPA_GET_BE16(pos);
		pos += 2;
		if (len > (unsigned int) (end - pos))
			break;

		if (type == PAC_TYPE_A_ID) {
			os_free(pac->a_id);
			pac->a_id = os_memdup(pos, len);
			if (!pac->a_id)
				break;
			pac->a_id_len = len;
		}

		if (type == PAC_TYPE_A_ID_INFO) {
			os_free(pac->a_id_info);
			pac->a_id_info = os_memdup(pos, len);
			if (!pac->a_id_info)
				break;
			pac->a_id_info_len = len;
		}

		pos += len;
	}
}


/**
 * eap_teap_load_pac_bin - Load PAC entries (binary format)
 * @sm: Pointer to EAP state machine allocated with eap_peer_sm_init()
 * @pac_root: Pointer to root of the PAC list (to be filled)
 * @pac_file: Name of the PAC file/blob to load
 * Returns: 0 on success, -1 on failure
 */
int eap_teap_load_pac_bin(struct eap_sm *sm, struct eap_teap_pac **pac_root,
			  const char *pac_file)
{
	const struct wpa_config_blob *blob = NULL;
	u8 *buf, *end, *pos;
	size_t len, count = 0;
	struct eap_teap_pac *pac, *prev;

	*pac_root = NULL;

	if (!pac_file)
		return -1;

	if (os_strncmp(pac_file, "blob://", 7) == 0) {
		blob = eap_get_config_blob(sm, pac_file + 7);
		if (!blob) {
			wpa_printf(MSG_INFO,
				   "EAP-TEAP: No PAC blob '%s' - assume no PAC entries have been provisioned",
				   pac_file + 7);
			return 0;
		}
		buf = blob->data;
		len = blob->len;
	} else {
		buf = (u8 *) os_readfile(pac_file, &len);
		if (!buf) {
			wpa_printf(MSG_INFO,
				   "EAP-TEAP: No PAC file '%s' - assume no PAC entries have been provisioned",
				   pac_file);
			return 0;
		}
	}

	if (len == 0) {
		if (!blob)
			os_free(buf);
		return 0;
	}

	if (len < 6 || WPA_GET_BE32(buf) != EAP_TEAP_PAC_BINARY_MAGIC ||
	    WPA_GET_BE16(buf + 4) != EAP_TEAP_PAC_BINARY_FORMAT_VERSION) {
		wpa_printf(MSG_INFO, "EAP-TEAP: Invalid PAC file '%s' (bin)",
			   pac_file);
		if (!blob)
			os_free(buf);
		return -1;
	}

	pac = prev = NULL;
	pos = buf + 6;
	end = buf + len;
	while (pos < end) {
		u16 val;

		if (end - pos < 2 + EAP_TEAP_PAC_KEY_LEN + 2 + 2) {
			pac = NULL;
			goto parse_fail;
		}

		pac = os_zalloc(sizeof(*pac));
		if (!pac)
			goto parse_fail;

		pac->pac_type = WPA_GET_BE16(pos);
		pos += 2;
		os_memcpy(pac->pac_key, pos, EAP_TEAP_PAC_KEY_LEN);
		pos += EAP_TEAP_PAC_KEY_LEN;
		val = WPA_GET_BE16(pos);
		pos += 2;
		if (val > end - pos)
			goto parse_fail;
		pac->pac_opaque_len = val;
		pac->pac_opaque = os_memdup(pos, pac->pac_opaque_len);
		if (!pac->pac_opaque)
			goto parse_fail;
		pos += pac->pac_opaque_len;
		if (end - pos < 2)
			goto parse_fail;
		val = WPA_GET_BE16(pos);
		pos += 2;
		if (val > end - pos)
			goto parse_fail;
		pac->pac_info_len = val;
		pac->pac_info = os_memdup(pos, pac->pac_info_len);
		if (!pac->pac_info)
			goto parse_fail;
		pos += pac->pac_info_len;
		eap_teap_pac_get_a_id(pac);

		count++;
		if (prev)
			prev->next = pac;
		else
			*pac_root = pac;
		prev = pac;
	}

	if (!blob)
		os_free(buf);

	wpa_printf(MSG_DEBUG, "EAP-TEAP: Read %lu PAC entries from '%s' (bin)",
		   (unsigned long) count, pac_file);

	return 0;

parse_fail:
	wpa_printf(MSG_INFO, "EAP-TEAP: Failed to parse PAC file '%s' (bin)",
		   pac_file);
	if (!blob)
		os_free(buf);
	if (pac)
		eap_teap_free_pac(pac);
	return -1;
}


/**
 * eap_teap_save_pac_bin - Save PAC entries (binary format)
 * @sm: Pointer to EAP state machine allocated with eap_peer_sm_init()
 * @pac_root: Root of the PAC list
 * @pac_file: Name of the PAC file/blob
 * Returns: 0 on success, -1 on failure
 */
int eap_teap_save_pac_bin(struct eap_sm *sm, struct eap_teap_pac *pac_root,
			  const char *pac_file)
{
	size_t len, count = 0;
	struct eap_teap_pac *pac;
	u8 *buf, *pos;

	len = 6;
	pac = pac_root;
	while (pac) {
		if (pac->pac_opaque_len > 65535 ||
		    pac->pac_info_len > 65535)
			return -1;
		len += 2 + EAP_TEAP_PAC_KEY_LEN + 2 + pac->pac_opaque_len +
			2 + pac->pac_info_len;
		pac = pac->next;
	}

	buf = os_malloc(len);
	if (!buf)
		return -1;

	pos = buf;
	WPA_PUT_BE32(pos, EAP_TEAP_PAC_BINARY_MAGIC);
	pos += 4;
	WPA_PUT_BE16(pos, EAP_TEAP_PAC_BINARY_FORMAT_VERSION);
	pos += 2;

	pac = pac_root;
	while (pac) {
		WPA_PUT_BE16(pos, pac->pac_type);
		pos += 2;
		os_memcpy(pos, pac->pac_key, EAP_TEAP_PAC_KEY_LEN);
		pos += EAP_TEAP_PAC_KEY_LEN;
		WPA_PUT_BE16(pos, pac->pac_opaque_len);
		pos += 2;
		os_memcpy(pos, pac->pac_opaque, pac->pac_opaque_len);
		pos += pac->pac_opaque_len;
		WPA_PUT_BE16(pos, pac->pac_info_len);
		pos += 2;
		os_memcpy(pos, pac->pac_info, pac->pac_info_len);
		pos += pac->pac_info_len;

		pac = pac->next;
		count++;
	}

	if (eap_teap_write_pac(sm, pac_file, (char *) buf, len)) {
		os_free(buf);
		return -1;
	}

	wpa_printf(MSG_DEBUG, "EAP-TEAP: Wrote %lu PAC entries into '%s' (bin)",
		   (unsigned long) count, pac_file);

	return 0;
}
