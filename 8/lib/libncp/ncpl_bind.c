/*
 * Copyright (c) 1999, Boris Popov
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <netncp/ncp_lib.h>

static void nw_passencrypt(char *old, char *new, char *out);

int
ncp_get_bindery_object_id(NWCONN_HANDLE connid, u_int16_t object_type,
	const char *object_name, struct ncp_bindery_object *target)
{
	int error;
	DECLARE_RQ;

	ncp_init_request_s(conn, 53);
	ncp_add_word_hl(conn, object_type);
	ncp_add_pstring(conn, object_name);

	if ((error = ncp_request(connid, 23, conn)) != 0) {
		return error;
	}
	if (conn->rpsize < 54) {
		return EACCES;
	}
	target->object_id = ncp_reply_dword_hl(conn, 0);
	target->object_type = ncp_reply_word_hl(conn, 4);
	memcpy(target->object_name, ncp_reply_data(conn, 6), 48);
	return 0;
}

int
ncp_read_property_value(NWCONN_HANDLE connid, int object_type,
	const char *object_name, int segment, const char *prop_name,
	struct nw_property *target)
{
	int error;
	struct ncp_buf conn;
	ncp_init_request_s(&conn, 61);
	ncp_add_word_hl(&conn, object_type);
	ncp_add_pstring(&conn, object_name);
	ncp_add_byte(&conn, segment);
	ncp_add_pstring(&conn, prop_name);

	if ((error = ncp_request(connid,23,&conn)) != 0) {
		return error;
	}
	memcpy(&(target->value), ncp_reply_data(&conn, 0), 128);
	target->more_flag = ncp_reply_byte(&conn, 128);
	target->property_flag = ncp_reply_byte(&conn, 129);
	return 0;
}

int
ncp_scan_bindery_object(NWCONN_HANDLE connid, u_int32_t last_id,
	u_int16_t object_type, char *search_string,
	struct ncp_bindery_object *target)
{
	int error;
	DECLARE_RQ;

	ncp_init_request_s(conn, 55);
	ncp_add_dword_hl(conn, last_id);
	ncp_add_word_hl(conn, object_type);
	ncp_add_pstring(conn, search_string);
	error = ncp_request(connid, 23, conn);
	if (error) return error;
	target->object_id = ncp_reply_dword_hl(conn, 0);
	target->object_type = ncp_reply_word_hl(conn, 4);
	memcpy(target->object_name, ncp_reply_data(conn, 6),NCP_BINDERY_NAME_LEN);
	target->object_flags = ncp_reply_byte(conn, 54);
	target->object_security = ncp_reply_byte(conn, 55);
	target->object_has_prop = ncp_reply_byte(conn, 56);
	return 0;
}

int
ncp_get_bindery_object_name(NWCONN_HANDLE connid, u_int32_t object_id,
	struct ncp_bindery_object *target)
{
	int error;
	DECLARE_RQ;

	ncp_init_request_s(conn, 54);
	ncp_add_dword_hl(conn, object_id);
	if ((error = ncp_request(connid, 23, conn)) != 0)
		return error;
	target->object_id = ncp_reply_dword_hl(conn, 0);
	target->object_type = ncp_reply_word_hl(conn, 4);
	memcpy(target->object_name, ncp_reply_data(conn, 6), 48);
	return 0;
}

int
ncp_change_obj_passwd(NWCONN_HANDLE connid, 
	const struct ncp_bindery_object *object,
	const u_char *key,
	const u_char *oldpasswd,
	const u_char *newpasswd)
{
	long id = htonl(object->object_id);
	u_char cryptkey[8];
	u_char newpwd[16];	/* new passwd as stored by server */
	u_char oldpwd[16];	/* old passwd as stored by server */
	u_char len;
	DECLARE_RQ;

	memcpy(cryptkey, key, 8);
	nw_keyhash((u_char *)&id, oldpasswd, strlen(oldpasswd), oldpwd);
	nw_keyhash((u_char *)&id, newpasswd, strlen(newpasswd), newpwd);
	nw_encrypt(cryptkey, oldpwd, cryptkey);
	nw_passencrypt(oldpwd, newpwd, newpwd);
	nw_passencrypt(oldpwd + 8, newpwd + 8, newpwd + 8);
	if ((len = strlen(newpasswd)) > 63) {
		len = 63;
	}
	len = ((len ^ oldpwd[0] ^ oldpwd[1]) & 0x7f) | 0x40;

	ncp_init_request_s(conn, 75);
	ncp_add_mem(conn, cryptkey, 8);
	ncp_add_word_hl(conn, object->object_type);
	ncp_add_pstring(conn, object->object_name);
	ncp_add_byte(conn, len);
	ncp_add_mem(conn, newpwd, 16);
	return ncp_request(connid, 23, conn);
}

/*
 * target is a 8-byte buffer
 */
int
ncp_get_encryption_key(NWCONN_HANDLE cH, char *target) {
	int error;
	DECLARE_RQ;

	ncp_init_request_s(conn, 23);

	error = ncp_request(cH, 23, conn);
	if (error)
		return error;
	if (conn->rpsize < 8)
		return EACCES;
	memcpy(target, ncp_reply_data(conn, 0), 8);
	return 0;
}

int
ncp_keyed_verify_password(NWCONN_HANDLE cH, char *key, char *passwd,
	struct ncp_bindery_object *objinfo)
{
	u_long id = htonl(objinfo->object_id);
	u_char cryptkey[8];
	u_char buf[128];
	DECLARE_RQ;

	nw_keyhash((u_char *)&id, passwd, strlen(passwd), buf);
	nw_encrypt(key, buf, cryptkey);

	ncp_init_request_s(conn, 74);
	ncp_add_mem(conn, cryptkey, sizeof(cryptkey));
	ncp_add_word_hl(conn, objinfo->object_type);
	ncp_add_pstring(conn, objinfo->object_name);

	return ncp_request(cH, 23, conn);
}

static char passkeys[256 + 16] = {
	0x0f, 0x08, 0x05, 0x07, 0x0c, 0x02, 0x0e, 0x09,
	0x00, 0x01, 0x06, 0x0d, 0x03, 0x04, 0x0b, 0x0a,
	0x02, 0x0c, 0x0e, 0x06, 0x0f, 0x00, 0x01, 0x08,
	0x0d, 0x03, 0x0a, 0x04, 0x09, 0x0b, 0x05, 0x07,
	0x05, 0x02, 0x09, 0x0f, 0x0c, 0x04, 0x0d, 0x00,
	0x0e, 0x0a, 0x06, 0x08, 0x0b, 0x01, 0x03, 0x07,
	0x0f, 0x0d, 0x02, 0x06, 0x07, 0x08, 0x05, 0x09,
	0x00, 0x04, 0x0c, 0x03, 0x01, 0x0a, 0x0b, 0x0e,
	0x05, 0x0e, 0x02, 0x0b, 0x0d, 0x0a, 0x07, 0x00,
	0x08, 0x06, 0x04, 0x01, 0x0f, 0x0c, 0x03, 0x09,
	0x08, 0x02, 0x0f, 0x0a, 0x05, 0x09, 0x06, 0x0c,
	0x00, 0x0b, 0x01, 0x0d, 0x07, 0x03, 0x04, 0x0e,
	0x0e, 0x08, 0x00, 0x09, 0x04, 0x0b, 0x02, 0x07,
	0x0c, 0x03, 0x0a, 0x05, 0x0d, 0x01, 0x06, 0x0f,
	0x01, 0x04, 0x08, 0x0a, 0x0d, 0x0b, 0x07, 0x0e,
	0x05, 0x0f, 0x03, 0x09, 0x00, 0x02, 0x06, 0x0c,
	0x05, 0x03, 0x0c, 0x08, 0x0b, 0x02, 0x0e, 0x0a,
	0x04, 0x01, 0x0d, 0x00, 0x06, 0x07, 0x0f, 0x09,
	0x06, 0x00, 0x0b, 0x0e, 0x0d, 0x04, 0x0c, 0x0f,
	0x07, 0x02, 0x08, 0x0a, 0x01, 0x05, 0x03, 0x09,
	0x0b, 0x05, 0x0a, 0x0e, 0x0f, 0x01, 0x0c, 0x00,
	0x06, 0x04, 0x02, 0x09, 0x03, 0x0d, 0x07, 0x08,
	0x07, 0x02, 0x0a, 0x00, 0x0e, 0x08, 0x0f, 0x04,
	0x0c, 0x0b, 0x09, 0x01, 0x05, 0x0d, 0x03, 0x06,
	0x07, 0x04, 0x0f, 0x09, 0x05, 0x01, 0x0c, 0x0b,
	0x00, 0x03, 0x08, 0x0e, 0x02, 0x0a, 0x06, 0x0d,
	0x09, 0x04, 0x08, 0x00, 0x0a, 0x03, 0x01, 0x0c,
	0x05, 0x0f, 0x07, 0x02, 0x0b, 0x0e, 0x06, 0x0d,
	0x09, 0x05, 0x04, 0x07, 0x0e, 0x08, 0x03, 0x01,
	0x0d, 0x0b, 0x0c, 0x02, 0x00, 0x0f, 0x06, 0x0a,
	0x09, 0x0a, 0x0b, 0x0d, 0x05, 0x03, 0x0f, 0x00,
	0x01, 0x0c, 0x08, 0x07, 0x06, 0x04, 0x0e, 0x02,
	0x03, 0x0e, 0x0f, 0x02, 0x0d, 0x0c, 0x04, 0x05,
	0x09, 0x06, 0x00, 0x01, 0x0b, 0x07, 0x0a, 0x08
};

static void
nw_passencrypt(char *old, char *new, char *out)
{
	char *p, v;
	char copy[8];
	int i, di, ax;

#define HIGH(x)	(((x) >> 4) & 0xf)
#define LOW(x)	((x) & 0xf)
	memcpy(copy, new, 8);

	for (i = 0; i < 16; i++) {
		for (di = 0, ax = 0, p = old; di < 8; di++, ax += 0x20, p++) {
			v = copy[di] ^ *p;
			copy[di] = (passkeys[HIGH(v) + ax + 0x10] << 4) |
				   passkeys[LOW(v) + ax];
		}
		v = old[7];
		for (p = old + 7; p > old; p--) {
			*p = HIGH(p[-1]) | ((*p) << 4);
		}
		*old = HIGH(v) | (*old) << 4;
		bzero(out, 8);

		for (di = 0; di < 16; di++) {
			v = passkeys[di + 0x100];
			v = (v & 1) ? HIGH(copy[v / 2]) : LOW(copy[v / 2]);
			out[di / 2] |= ((di & 1) ? v << 4 : v);
		}
		memcpy(copy, out, 8);
	}
}
