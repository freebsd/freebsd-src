/*
 * Copyright (c) 2004 Apple Computer, Inc.
 * Copyright (c) 2005 SPARTA, Inc.
 * Copyright (c) 2006 Robert N. M. Watson
 * All rights reserved.
 *
 * This code was developed in part by Robert N. M. Watson, Senior Principal
 * Scientist, SPARTA, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $P4: //depot/projects/trustedbsd/openbsm/libbsm/bsm_io.c#41 $
 */

#include <sys/types.h>

#include <config/config.h>
#ifdef HAVE_SYS_ENDIAN_H
#include <sys/endian.h>
#else /* !HAVE_SYS_ENDIAN_H */
#ifdef HAVE_MACHINE_ENDIAN_H
#include <machine/endian.h>
#else /* !HAVE_MACHINE_ENDIAN_H */
#ifdef HAVE_ENDIAN_H
#include <endian.h>
#else /* !HAVE_ENDIAN_H */
#error "No supported endian.h"
#endif /* !HAVE_ENDIAN_H */
#endif /* !HAVE_MACHINE_ENDIAN_H */
#include <compat/endian.h>
#endif /* !HAVE_SYS_ENDIAN_H */
#ifdef HAVE_FULL_QUEUE_H
#include <sys/queue.h>
#else /* !HAVE_FULL_QUEUE_H */
#include <compat/queue.h>
#endif /* !HAVE_FULL_QUEUE_H */

#include <sys/stat.h>
#include <sys/socket.h>

#include <bsm/libbsm.h>

#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>

#include <bsm/audit_internal.h>

#define	READ_TOKEN_BYTES(buf, len, dest, size, bytesread, err) do {	\
	if (bytesread + size > len) {					\
		err = 1;						\
	} else {							\
		memcpy(dest, buf + bytesread, size);			\
		bytesread += size;					\
	}								\
} while (0)

#define	READ_TOKEN_U_CHAR(buf, len, dest, bytesread, err) do {		\
	if (bytesread + sizeof(u_char) <= len) {			\
		dest = buf[bytesread];					\
		bytesread += sizeof(u_char);				\
	} else								\
		err = 1;						\
} while (0)

#define	READ_TOKEN_U_INT16(buf, len, dest, bytesread, err) do {		\
	if (bytesread + sizeof(u_int16_t) <= len) {			\
		dest = be16dec(buf + bytesread);			\
		bytesread += sizeof(u_int16_t);				\
	} else								\
		err = 1;						\
} while (0)

#define	READ_TOKEN_U_INT32(buf, len, dest, bytesread, err) do {		\
	if (bytesread + sizeof(u_int32_t) <= len) {			\
		dest = be32dec(buf + bytesread);			\
		bytesread += sizeof(u_int32_t);				\
	} else								\
		err = 1; 						\
} while (0)

#define	READ_TOKEN_U_INT64(buf, len, dest, bytesread, err) do {		\
	if (bytesread + sizeof(u_int64_t) <= len) {			\
		dest = be64dec(buf + bytesread);			\
		bytesread += sizeof(u_int64_t);				\
	} else								\
		err = 1; 						\
} while (0)

#define	SET_PTR(buf, len, ptr, size, bytesread, err) do {		\
	if ((bytesread) + (size) > (len))				\
		(err) = 1;						\
	else {								\
		(ptr) = (buf) + (bytesread);				\
		(bytesread) += (size);					\
	}								\
} while (0)

/*
 * Prints the delimiter string.
 */
static void
print_delim(FILE *fp, const char *del)
{

	fprintf(fp, "%s", del);
}

/*
 * Prints a single byte in the given format.
 */
static void
print_1_byte(FILE *fp, u_char val, const char *format)
{

	fprintf(fp, format, val);
}

/*
 * Print 2 bytes in the given format.
 */
static void
print_2_bytes(FILE *fp, u_int16_t val, const char *format)
{

	fprintf(fp, format, val);
}

/*
 * Prints 4 bytes in the given format.
 */
static void
print_4_bytes(FILE *fp, u_int32_t val, const char *format)
{

	fprintf(fp, format, val);
}

/*
 * Prints 8 bytes in the given format.
 */
static void
print_8_bytes(FILE *fp, u_int64_t val, const char *format)
{

	fprintf(fp, format, val);
}

/*
 * Prints the given size of data bytes in hex.
 */
static void
print_mem(FILE *fp, u_char *data, size_t len)
{
	int i;

	if (len > 0) {
		fprintf(fp, "0x");
		for (i = 0; i < len; i++)
			fprintf(fp, "%x", data[i]);
	}
}

/*
 * Prints the given data bytes as a string.
 */
static void
print_string(FILE *fp, u_char *str, size_t len)
{
	int i;

	if (len > 0) {
		for (i = 0; i < len; i++) {
			if (str[i] != '\0')
				fprintf(fp, "%c", str[i]);
		}
	}
}

/*
 * Prints the token type in either the raw or the default form.
 */
static void
print_tok_type(FILE *fp, u_char type, const char *tokname, char raw)
{

	if (raw)
		fprintf(fp, "%u", type);
	else
		fprintf(fp, "%s", tokname);
}

/*
 * Prints a user value.
 */
static void
print_user(FILE *fp, u_int32_t usr, char raw)
{
	struct passwd *pwent;

	if (raw)
		fprintf(fp, "%d", usr);
	else {
		pwent = getpwuid(usr);
		if (pwent != NULL)
			fprintf(fp, "%s", pwent->pw_name);
		else
			fprintf(fp, "%d", usr);
	}
}

/*
 * Prints a group value.
 */
static void
print_group(FILE *fp, u_int32_t grp, char raw)
{
	struct group *grpent;

	if (raw)
		fprintf(fp, "%d", grp);
	else {
		grpent = getgrgid(grp);
		if (grpent != NULL)
			fprintf(fp, "%s", grpent->gr_name);
		else
			fprintf(fp, "%d", grp);
	}
}

/*
 * Prints the event from the header token in either the short, default or raw
 * form.
 */
static void
print_event(FILE *fp, u_int16_t ev, char raw, char sfrm)
{
	char event_ent_name[AU_EVENT_NAME_MAX];
	char event_ent_desc[AU_EVENT_DESC_MAX];
	struct au_event_ent e, *ep;

	bzero(&e, sizeof(e));
	bzero(event_ent_name, sizeof(event_ent_name));
	bzero(event_ent_desc, sizeof(event_ent_desc));
	e.ae_name = event_ent_name;
	e.ae_desc = event_ent_desc;

	ep = getauevnum_r(&e, ev);
	if (ep == NULL) {
		fprintf(fp, "%u", ev);
		return;
	}

	if (raw)
		fprintf(fp, "%u", ev);
	else if (sfrm)
		fprintf(fp, "%s", e.ae_name);
	else
		fprintf(fp, "%s", e.ae_desc);
}


/*
 * Prints the event modifier from the header token in either the default or
 * raw form.
 */
static void
print_evmod(FILE *fp, u_int16_t evmod, char raw)
{
	if (raw)
		fprintf(fp, "%u", evmod);
	else
		fprintf(fp, "%u", evmod);
}

/*
 * Prints seconds in the ctime format.
 */
static void
print_sec32(FILE *fp, u_int32_t sec, char raw)
{
	time_t timestamp;
	char timestr[26];

	if (raw)
		fprintf(fp, "%u", sec);
	else {
		timestamp = (time_t)sec;
		ctime_r(&timestamp, timestr);
		timestr[24] = '\0'; /* No new line */
		fprintf(fp, "%s", timestr);
	}
}

/*
 * XXXRW: 64-bit token streams make use of 64-bit time stamps; since we
 * assume a 32-bit time_t, we simply truncate for now.
 */
static void
print_sec64(FILE *fp, u_int64_t sec, char raw)
{
	time_t timestamp;
	char timestr[26];

	if (raw)
		fprintf(fp, "%u", (u_int32_t)sec);
	else {
		timestamp = (time_t)sec;
		ctime_r(&timestamp, timestr);
		timestr[24] = '\0'; /* No new line */
		fprintf(fp, "%s", timestr);
	}
}

/*
 * Prints the excess milliseconds.
 */
static void
print_msec32(FILE *fp, u_int32_t msec, char raw)
{
	if (raw)
		fprintf(fp, "%u", msec);
	else
		fprintf(fp, " + %u msec", msec);
}

/*
 * XXXRW: 64-bit token streams make use of 64-bit time stamps; since we assume
 * a 32-bit msec, we simply truncate for now.
 */
static void
print_msec64(FILE *fp, u_int64_t msec, char raw)
{

	msec &= 0xffffffff;
	if (raw)
		fprintf(fp, "%u", (u_int32_t)msec);
	else
		fprintf(fp, " + %u msec", (u_int32_t)msec);
}

/*
 * Prints a dotted form for the IP address.
 */
static void
print_ip_address(FILE *fp, u_int32_t ip)
{
	struct in_addr ipaddr;

	ipaddr.s_addr = ip;
	fprintf(fp, "%s", inet_ntoa(ipaddr));
}

/* 
 * Prints a string value for the given ip address.
 */
static void
print_ip_ex_address(FILE *fp, u_int32_t type, u_int32_t *ipaddr)
{
	struct in_addr ipv4;
	struct in6_addr ipv6;
	char dst[INET6_ADDRSTRLEN];

	switch (type) {
	case AU_IPv4:
		ipv4.s_addr = (in_addr_t)(ipaddr[0]);
		fprintf(fp, "%s", inet_ntop(AF_INET, &ipv4, dst,
		    INET6_ADDRSTRLEN));
		break;

	case AU_IPv6:
		bcopy(ipaddr, &ipv6, sizeof(ipv6));
		fprintf(fp, "%s", inet_ntop(AF_INET6, &ipv6, dst,
		    INET6_ADDRSTRLEN));
		break;

	default:
		fprintf(fp, "invalid");
	}
}

/*
 * Prints return value as success or failure.
 */
static void
print_retval(FILE *fp, u_char status, char raw)
{
	if (raw)
		fprintf(fp, "%u", status);
	else {
		if (status == 0)
			fprintf(fp, "success");
		else
			fprintf(fp, "failure : %s", strerror(status));
	}
}

/*
 * Prints the exit value.
 */
static void
print_errval(FILE *fp, u_int32_t val)
{

	fprintf(fp, "Error %u", val);
}

/*
 * Prints IPC type.
 */
static void
print_ipctype(FILE *fp, u_char type, char raw)
{
	if (raw)
		fprintf(fp, "%u", type);
	else {
		if (type == AT_IPC_MSG)
			fprintf(fp, "Message IPC");
		else if (type == AT_IPC_SEM)
			fprintf(fp, "Semaphore IPC");
		else if (type == AT_IPC_SHM)
			fprintf(fp, "Shared Memory IPC");
		else
			fprintf(fp, "%u", type);
	}
}

/*
 * record byte count       4 bytes
 * version #               1 byte    [2]
 * event type              2 bytes
 * event modifier          2 bytes
 * seconds of time         4 bytes/8 bytes (32-bit/64-bit value)
 * milliseconds of time    4 bytes/8 bytes (32-bit/64-bit value)
 */
static int
fetch_header32_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_U_INT32(buf, len, tok->tt.hdr32.size, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_CHAR(buf, len, tok->tt.hdr32.version, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT16(buf, len, tok->tt.hdr32.e_type, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT16(buf, len, tok->tt.hdr32.e_mod, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.hdr32.s, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.hdr32.ms, tok->len, err);
	if (err)
		return (-1);

	return (0);
}

static void
print_header32_tok(FILE *fp, tokenstr_t *tok, char *del, char raw, char sfrm)
{

	print_tok_type(fp, tok->id, "header", raw);
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.hdr32.size, "%u");
	print_delim(fp, del);
	print_1_byte(fp, tok->tt.hdr32.version, "%u");
	print_delim(fp, del);
	print_event(fp, tok->tt.hdr32.e_type, raw, sfrm);
	print_delim(fp, del);
	print_evmod(fp, tok->tt.hdr32.e_mod, raw);
	print_delim(fp, del);
	print_sec32(fp, tok->tt.hdr32.s, raw);
	print_delim(fp, del);
	print_msec32(fp, tok->tt.hdr32.ms, raw);
}

/*
 * The Solaris specifications for AUE_HEADER32_EX seem to differ a bit
 * depending on the bit of the specifications found.  The OpenSolaris source
 * code uses a 4-byte address length, followed by some number of bytes of
 * address data.  This contrasts with the Solaris audit.log.5 man page, which
 * specifies a 1-byte length field.  We use the Solaris 10 definition so that
 * we can parse audit trails from that system.
 *
 * record byte count       4 bytes
 * version #               1 byte     [2]
 * event type              2 bytes
 * event modifier          2 bytes
 * address type/length     4 bytes
 *   [ Solaris man page: address type/length     1 byte]
 * machine address         4 bytes/16 bytes (IPv4/IPv6 address)
 * seconds of time         4 bytes/8 bytes  (32/64-bits)
 * nanoseconds of time     4 bytes/8 bytes  (32/64-bits)
 */
static int
fetch_header32_ex_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_U_INT32(buf, len, tok->tt.hdr32_ex.size, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_CHAR(buf, len, tok->tt.hdr32_ex.version, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT16(buf, len, tok->tt.hdr32_ex.e_type, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT16(buf, len, tok->tt.hdr32_ex.e_mod, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.hdr32_ex.ad_type, tok->len, err);
	if (err)
		return (-1);

	bzero(tok->tt.hdr32_ex.addr, sizeof(tok->tt.hdr32_ex.addr));
	switch (tok->tt.hdr32_ex.ad_type) {
	case AU_IPv4:
		READ_TOKEN_BYTES(buf, len, &tok->tt.hdr32_ex.addr[0],
		    sizeof(tok->tt.hdr32_ex.addr[0]), tok->len, err);
		if (err)
			return (-1);
		break;

	case AU_IPv6:
		READ_TOKEN_BYTES(buf, len, tok->tt.hdr32_ex.addr,
		    sizeof(tok->tt.hdr32_ex.addr), tok->len, err);
		break;
	}

	READ_TOKEN_U_INT32(buf, len, tok->tt.hdr32_ex.s, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.hdr32_ex.ms, tok->len, err);
	if (err)
		return (-1);

	return (0);
}

static void
print_header32_ex_tok(FILE *fp, tokenstr_t *tok, char *del, char raw,
    char sfrm)
{

	print_tok_type(fp, tok->id, "header_ex", raw);
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.hdr32_ex.size, "%u");
	print_delim(fp, del);
	print_1_byte(fp, tok->tt.hdr32_ex.version, "%u");
	print_delim(fp, del);
	print_event(fp, tok->tt.hdr32_ex.e_type, raw, sfrm);
	print_delim(fp, del);
	print_evmod(fp, tok->tt.hdr32_ex.e_mod, raw);
	print_delim(fp, del);
	print_ip_ex_address(fp, tok->tt.hdr32_ex.ad_type,
	    tok->tt.hdr32_ex.addr);
	print_delim(fp, del);
	print_sec32(fp, tok->tt.hdr32_ex.s, raw);
	print_delim(fp, del);
	print_msec32(fp, tok->tt.hdr32_ex.ms, raw);
}

/*
 * record byte count       4 bytes
 * event type              2 bytes
 * event modifier          2 bytes
 * seconds of time         4 bytes/8 bytes (32-bit/64-bit value)
 * milliseconds of time    4 bytes/8 bytes (32-bit/64-bit value)
 * version #              
 */
static int
fetch_header64_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_U_INT32(buf, len, tok->tt.hdr64.size, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_CHAR(buf, len, tok->tt.hdr64.version, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT16(buf, len, tok->tt.hdr64.e_type, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT16(buf, len, tok->tt.hdr64.e_mod, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT64(buf, len, tok->tt.hdr64.s, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT64(buf, len, tok->tt.hdr64.ms, tok->len, err);
	if (err)
		return (-1);

	return (0);
}

static void
print_header64_tok(FILE *fp, tokenstr_t *tok, char *del, char raw, char sfrm)
{

	print_tok_type(fp, tok->id, "header", raw);
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.hdr64.size, "%u");
	print_delim(fp, del);
	print_1_byte(fp, tok->tt.hdr64.version, "%u");
	print_delim(fp, del);
	print_event(fp, tok->tt.hdr64.e_type, raw, sfrm);
	print_delim(fp, del);
	print_evmod(fp, tok->tt.hdr64.e_mod, raw);
	print_delim(fp, del);
	print_sec64(fp, tok->tt.hdr64.s, raw);
	print_delim(fp, del);
	print_msec64(fp, tok->tt.hdr64.ms, raw);
}
/*
 * record byte count       4 bytes
 * version #               1 byte     [2]
 * event type              2 bytes
 * event modifier          2 bytes
 * address type/length     4 bytes
 *   [ Solaris man page: address type/length     1 byte]
 * machine address         4 bytes/16 bytes (IPv4/IPv6 address)
 * seconds of time         4 bytes/8 bytes  (32/64-bits)
 * nanoseconds of time     4 bytes/8 bytes  (32/64-bits)
 *
 * XXXAUDIT: See comment by fetch_header32_ex_tok() for details on the
 * accuracy of the BSM spec.
 */
static int
fetch_header64_ex_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_U_INT32(buf, len, tok->tt.hdr64_ex.size, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_CHAR(buf, len, tok->tt.hdr64_ex.version, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT16(buf, len, tok->tt.hdr64_ex.e_type, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT16(buf, len, tok->tt.hdr64_ex.e_mod, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.hdr64_ex.ad_type, tok->len, err);
	if (err)
		return (-1);

	bzero(tok->tt.hdr64_ex.addr, sizeof(tok->tt.hdr64_ex.addr));
	switch (tok->tt.hdr64_ex.ad_type) {
	case AU_IPv4:
		READ_TOKEN_BYTES(buf, len, &tok->tt.hdr64_ex.addr[0],
		    sizeof(tok->tt.hdr64_ex.addr[0]), tok->len, err);
		if (err)
			return (-1);
		break;

	case AU_IPv6:
		READ_TOKEN_BYTES(buf, len, tok->tt.hdr64_ex.addr,
		    sizeof(tok->tt.hdr64_ex.addr), tok->len, err);
		break;
	}

	READ_TOKEN_U_INT64(buf, len, tok->tt.hdr64_ex.s, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT64(buf, len, tok->tt.hdr64_ex.ms, tok->len, err);
	if (err)
		return (-1);

	return (0);
}

static void
print_header64_ex_tok(FILE *fp, tokenstr_t *tok, char *del, char raw, char sfrm)
{

	print_tok_type(fp, tok->id, "header_ex", raw);
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.hdr64_ex.size, "%u");
	print_delim(fp, del);
	print_1_byte(fp, tok->tt.hdr64_ex.version, "%u");
	print_delim(fp, del);
	print_event(fp, tok->tt.hdr64_ex.e_type, raw, sfrm);
	print_delim(fp, del);
	print_evmod(fp, tok->tt.hdr64_ex.e_mod, raw);
	print_delim(fp, del);
	print_ip_ex_address(fp, tok->tt.hdr64_ex.ad_type,
	    tok->tt.hdr64_ex.addr);
	print_delim(fp, del);
	print_sec64(fp, tok->tt.hdr64_ex.s, raw);
	print_delim(fp, del);
	print_msec64(fp, tok->tt.hdr64_ex.ms, raw);
}

/*
 * trailer magic                        2 bytes
 * record size                          4 bytes
 */
static int
fetch_trailer_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_U_INT16(buf, len, tok->tt.trail.magic, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.trail.count, tok->len, err);
	if (err)
		return (-1);

	return (0);
}

static void
print_trailer_tok(FILE *fp, tokenstr_t *tok, char *del, char raw,
    __unused char sfrm)
{

	print_tok_type(fp, tok->id, "trailer", raw);
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.trail.count, "%u");
}

/*
 * argument #              1 byte
 * argument value          4 bytes/8 bytes (32-bit/64-bit value)
 * text length             2 bytes
 * text                    N bytes + 1 terminating NULL byte
 */
static int
fetch_arg32_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_U_CHAR(buf, len, tok->tt.arg32.no, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.arg32.val, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT16(buf, len, tok->tt.arg32.len, tok->len, err);
	if (err)
		return (-1);

	SET_PTR(buf, len, tok->tt.arg32.text, tok->tt.arg32.len, tok->len,
	    err);
	if (err)
		return (-1);

	return (0);
}

static void
print_arg32_tok(FILE *fp, tokenstr_t *tok, char *del, char raw,
    __unused char sfrm)
{

	print_tok_type(fp, tok->id, "argument", raw);
	print_delim(fp, del);
	print_1_byte(fp, tok->tt.arg32.no, "%u");
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.arg32.val, "0x%x");
	print_delim(fp, del);
	print_string(fp, tok->tt.arg32.text, tok->tt.arg32.len);
}

static int
fetch_arg64_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_U_CHAR(buf, len, tok->tt.arg64.no, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT64(buf, len, tok->tt.arg64.val, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT16(buf, len, tok->tt.arg64.len, tok->len, err);
	if (err)
		return (-1);

	SET_PTR(buf, len, tok->tt.arg64.text, tok->tt.arg64.len, tok->len,
	    err);
	if (err)
		return (-1);

	return (0);
}

static void
print_arg64_tok(FILE *fp, tokenstr_t *tok, char *del, char raw,
    __unused char sfrm)
{

	print_tok_type(fp, tok->id, "argument", raw);
	print_delim(fp, del);
	print_1_byte(fp, tok->tt.arg64.no, "%u");
	print_delim(fp, del);
	print_8_bytes(fp, tok->tt.arg64.val, "0x%llx");
	print_delim(fp, del);
	print_string(fp, tok->tt.arg64.text, tok->tt.arg64.len);
}

/*
 * how to print            1 byte
 * basic unit              1 byte
 * unit count              1 byte
 * data items              (depends on basic unit)
 */
static int
fetch_arb_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;
	int datasize;

	READ_TOKEN_U_CHAR(buf, len, tok->tt.arb.howtopr, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_CHAR(buf, len, tok->tt.arb.bu, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_CHAR(buf, len, tok->tt.arb.uc, tok->len, err);
	if (err)
		return (-1);

	/*
	 * Determine the size of the basic unit.
	 */
	switch(tok->tt.arb.bu) {
	case AUR_BYTE:
	/* case AUR_CHAR: */
		datasize = AUR_BYTE_SIZE;
		break;

	case AUR_SHORT:
		datasize = AUR_SHORT_SIZE;
		break;

	case AUR_INT32:
	/* case AUR_INT: */
		datasize = AUR_INT32_SIZE;
		break;

	case AUR_INT64:
		datasize = AUR_INT64_SIZE;
		break;

	default:
		return (-1);
	}

	SET_PTR(buf, len, tok->tt.arb.data, datasize * tok->tt.arb.uc,
	    tok->len, err);
	if (err)
		return (-1);

	return (0);
}

static void
print_arb_tok(FILE *fp, tokenstr_t *tok, char *del, char raw,
    __unused char sfrm)
{
	char *str;
	char *format;
	size_t size;
	int i;

	print_tok_type(fp, tok->id, "arbitrary", raw);
	print_delim(fp, del);

	switch(tok->tt.arb.howtopr) {
	case AUP_BINARY:
		str = "binary";
		format = " %c";
		break;

	case AUP_OCTAL:
		str = "octal";
		format = " %o";
		break;

	case AUP_DECIMAL:
		str = "decimal";
		format = " %d";
		break;

	case AUP_HEX:
		str = "hex";
		format = " %x";
		break;

	case AUP_STRING:
		str = "string";
		format = "%c";
		break;

	default:
		return;
	}

	print_string(fp, str, strlen(str));
	print_delim(fp, del);
	switch(tok->tt.arb.bu) {
	case AUR_BYTE:
	/* case AUR_CHAR: */
		str = "byte";
		size = AUR_BYTE_SIZE;
		print_string(fp, str, strlen(str));
		print_delim(fp, del);
		print_1_byte(fp, tok->tt.arb.uc, "%u");
		print_delim(fp, del);
		for (i = 0; i<tok->tt.arb.uc; i++)
			fprintf(fp, format, *(tok->tt.arb.data + (size * i)));
		break;

	case AUR_SHORT:
		str = "short";
		size = AUR_SHORT_SIZE;
		print_string(fp, str, strlen(str));
		print_delim(fp, del);
		print_1_byte(fp, tok->tt.arb.uc, "%u");
		print_delim(fp, del);
		for (i = 0; i < tok->tt.arb.uc; i++)
			fprintf(fp, format, *((u_int16_t *)(tok->tt.arb.data +
			    (size * i))));
		break;

	case AUR_INT32:
	/* case AUR_INT: */
		str = "int";
		size = AUR_INT32_SIZE;
		print_string(fp, str, strlen(str));
		print_delim(fp, del);
		print_1_byte(fp, tok->tt.arb.uc, "%u");
		print_delim(fp, del);
		for (i = 0; i < tok->tt.arb.uc; i++)
			fprintf(fp, format, *((u_int32_t *)(tok->tt.arb.data +
			    (size * i))));
		break;

	case AUR_INT64:
		str = "int64";
		size = AUR_INT64_SIZE;
		print_string(fp, str, strlen(str));
		print_delim(fp, del);
		print_1_byte(fp, tok->tt.arb.uc, "%u");
		print_delim(fp, del);
		for (i = 0; i < tok->tt.arb.uc; i++)
			fprintf(fp, format, *((u_int64_t *)(tok->tt.arb.data +
			    (size * i))));
		break;

	default:
		return;
	}
}

/*
 * file access mode        4 bytes
 * owner user ID           4 bytes
 * owner group ID          4 bytes
 * file system ID          4 bytes
 * node ID                 8 bytes
 * device                  4 bytes/8 bytes (32-bit/64-bit)
 */
static int
fetch_attr32_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_U_INT32(buf, len, tok->tt.attr32.mode, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.attr32.uid, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.attr32.gid, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.attr32.fsid, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT64(buf, len, tok->tt.attr32.nid, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.attr32.dev, tok->len, err);
	if (err)
		return (-1);

	return (0);
}

static void
print_attr32_tok(FILE *fp, tokenstr_t *tok, char *del, char raw,
    __unused char sfrm)
{

	print_tok_type(fp, tok->id, "attribute", raw);
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.attr32.mode, "%o");
	print_delim(fp, del);
	print_user(fp, tok->tt.attr32.uid, raw);
	print_delim(fp, del);
	print_group(fp, tok->tt.attr32.gid, raw);
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.attr32.fsid, "%u");
	print_delim(fp, del);
	print_8_bytes(fp, tok->tt.attr32.nid, "%lld");
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.attr32.dev, "%u");
}

/*
 * file access mode        4 bytes
 * owner user ID           4 bytes
 * owner group ID          4 bytes
 * file system ID          4 bytes
 * node ID                 8 bytes
 * device                  4 bytes/8 bytes (32-bit/64-bit)
 */
static int
fetch_attr64_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_U_INT32(buf, len, tok->tt.attr64.mode, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.attr64.uid, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.attr64.gid, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.attr64.fsid, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT64(buf, len, tok->tt.attr64.nid, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT64(buf, len, tok->tt.attr64.dev, tok->len, err);
	if (err)
		return (-1);

	return (0);
}

static void
print_attr64_tok(FILE *fp, tokenstr_t *tok, char *del, char raw,
    __unused char sfrm)
{

	print_tok_type(fp, tok->id, "attribute", raw);
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.attr64.mode, "%o");
	print_delim(fp, del);
	print_user(fp, tok->tt.attr64.uid, raw);
	print_delim(fp, del);
	print_group(fp, tok->tt.attr64.gid, raw);
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.attr64.fsid, "%u");
	print_delim(fp, del);
	print_8_bytes(fp, tok->tt.attr64.nid, "%lld");
	print_delim(fp, del);
	print_8_bytes(fp, tok->tt.attr64.dev, "%llu");
}

/*
 * status                  4 bytes
 * return value            4 bytes
 */
static int
fetch_exit_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_U_INT32(buf, len, tok->tt.exit.status, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.exit.ret, tok->len, err);
	if (err)
		return (-1);

	return (0);
}

static void
print_exit_tok(FILE *fp, tokenstr_t *tok, char *del, char raw,
    __unused char sfrm)
{

	print_tok_type(fp, tok->id, "exit", raw);
	print_delim(fp, del);
	print_errval(fp, tok->tt.exit.status);
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.exit.ret, "%u");
}

/*
 * count                   4 bytes
 * text                    count null-terminated string(s)
 */
static int
fetch_execarg_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;
	int i;
	char *bptr;

	READ_TOKEN_U_INT32(buf, len, tok->tt.execarg.count, tok->len, err);
	if (err)
		return (-1);

	for (i = 0; i < tok->tt.execarg.count; i++) {
		bptr = buf + tok->len;
		if (i < AUDIT_MAX_ARGS)
			tok->tt.execarg.text[i] = bptr;

		/* Look for a null terminated string. */
		while (bptr && (*bptr != '\0')) {
			if (++tok->len >=len)
				return (-1);
			bptr = buf + tok->len;
		}
		if (!bptr)
			return (-1);
		tok->len++; /* \0 character */
	}
	if (tok->tt.execarg.count > AUDIT_MAX_ARGS)
		tok->tt.execarg.count = AUDIT_MAX_ARGS;

	return (0);
}

static void
print_execarg_tok(FILE *fp, tokenstr_t *tok, char *del, char raw,
    __unused char sfrm)
{
	int i;

	print_tok_type(fp, tok->id, "exec arg", raw);
	for (i = 0; i < tok->tt.execarg.count; i++) {
		print_delim(fp, del);
		print_string(fp, tok->tt.execarg.text[i],
		    strlen(tok->tt.execarg.text[i]));
	}
}

/*
 * count                   4 bytes
 * text                    count null-terminated string(s)
 */
static int
fetch_execenv_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;
	int i;
	char *bptr;

	READ_TOKEN_U_INT32(buf, len, tok->tt.execenv.count, tok->len, err);
	if (err)
		return (-1);

	for (i = 0; i < tok->tt.execenv.count; i++) {
		bptr = buf + tok->len;
		if (i < AUDIT_MAX_ENV)
			tok->tt.execenv.text[i] = bptr;

		/* Look for a null terminated string. */
		while (bptr && (*bptr != '\0')) {
			if (++tok->len >=len)
				return (-1);
			bptr = buf + tok->len;
		}
		if (!bptr)
			return (-1);
		tok->len++; /* \0 character */
	}
	if (tok->tt.execenv.count > AUDIT_MAX_ENV)
		tok->tt.execenv.count = AUDIT_MAX_ENV;

	return (0);
}

static void
print_execenv_tok(FILE *fp, tokenstr_t *tok, char *del, char raw,
    __unused char sfrm)
{
	int i;

	print_tok_type(fp, tok->id, "exec env", raw);
	for (i = 0; i< tok->tt.execenv.count; i++) {
		print_delim(fp, del);
		print_string(fp, tok->tt.execenv.text[i],
		    strlen(tok->tt.execenv.text[i]));
	}
}

/*
 * seconds of time          4 bytes
 * milliseconds of time     4 bytes
 * file name len            2 bytes
 * file pathname            N bytes + 1 terminating NULL byte
 */
static int
fetch_file_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_U_INT32(buf, len, tok->tt.file.s, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.file.ms, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT16(buf, len, tok->tt.file.len, tok->len, err);
	if (err)
		return (-1);

	SET_PTR(buf, len, tok->tt.file.name, tok->tt.file.len, tok->len, err);
	if (err)
		return (-1);

	return (0);
}

static void
print_file_tok(FILE *fp, tokenstr_t *tok, char *del, char raw,
    __unused char sfrm)
{

	print_tok_type(fp, tok->id, "file", raw);
	print_delim(fp, del);
	print_sec32(fp, tok->tt.file.s, raw);
	print_delim(fp, del);
	print_msec32(fp, tok->tt.file.ms, raw);
	print_delim(fp, del);
	print_string(fp, tok->tt.file.name, tok->tt.file.len);
}

/*
 * number groups           2 bytes
 * group list              count * 4 bytes
 */
static int
fetch_newgroups_tok(tokenstr_t *tok, char *buf, int len)
{
	int i;
	int err = 0;

	READ_TOKEN_U_INT16(buf, len, tok->tt.grps.no, tok->len, err);
	if (err)
		return (-1);

	for (i = 0; i<tok->tt.grps.no; i++) {
		READ_TOKEN_U_INT32(buf, len, tok->tt.grps.list[i], tok->len,
		    err);
    		if (err)
    			return (-1);
	}

	return (0);
}

static void
print_newgroups_tok(FILE *fp, tokenstr_t *tok, char *del, char raw,
    __unused char sfrm)
{
	int i;

	print_tok_type(fp, tok->id, "group", raw);
	for (i = 0; i < tok->tt.grps.no; i++) {
		print_delim(fp, del);
		print_group(fp, tok->tt.grps.list[i], raw);
	}
}

/*
 * Internet addr 4 bytes
 */
static int
fetch_inaddr_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_BYTES(buf, len, &tok->tt.inaddr.addr, sizeof(uint32_t),
	    tok->len, err);
	if (err)
		return (-1);

	return (0);

}

static void
print_inaddr_tok(FILE *fp, tokenstr_t *tok, char *del, char raw,
    __unused char sfrm)
{

	print_tok_type(fp, tok->id, "ip addr", raw);
	print_delim(fp, del);
	print_ip_address(fp, tok->tt.inaddr.addr);
}

/*
 * type 	4 bytes
 * address 16 bytes
 */
static int
fetch_inaddr_ex_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_U_INT32(buf, len, tok->tt.inaddr_ex.type, tok->len, err);
	if (err)
		return (-1);

	if (tok->tt.inaddr_ex.type == AU_IPv4) {
		READ_TOKEN_BYTES(buf, len, &tok->tt.inaddr_ex.addr[0],
		    sizeof(tok->tt.inaddr_ex.addr[0]), tok->len, err);
		if (err)
			return (-1);
	} else if (tok->tt.inaddr_ex.type == AU_IPv6) {
		READ_TOKEN_BYTES(buf, len, tok->tt.inaddr_ex.addr,
		    sizeof(tok->tt.inaddr_ex.addr), tok->len, err);
		if (err)
			return (-1);
	} else
		return (-1);

	return (0);
}

static void
print_inaddr_ex_tok(FILE *fp, tokenstr_t *tok, char *del, char raw,
    __unused char sfrm)
{

	print_tok_type(fp, tok->id, "ip addr ex", raw);
	print_delim(fp, del);
	print_ip_ex_address(fp, tok->tt.inaddr_ex.type,
	    tok->tt.inaddr_ex.addr);
}

/*
 * ip header     20 bytes
 */
static int
fetch_ip_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_U_CHAR(buf, len, tok->tt.ip.version, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_CHAR(buf, len, tok->tt.ip.tos, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_BYTES(buf, len, &tok->tt.ip.len, sizeof(uint16_t),
	    tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_BYTES(buf, len, &tok->tt.ip.id, sizeof(uint16_t),
	    tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_BYTES(buf, len, &tok->tt.ip.offset, sizeof(uint16_t),
	    tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_CHAR(buf, len, tok->tt.ip.ttl, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_CHAR(buf, len, tok->tt.ip.prot, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_BYTES(buf, len, &tok->tt.ip.chksm, sizeof(uint16_t),
	    tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_BYTES(buf, len, &tok->tt.ip.src, sizeof(tok->tt.ip.src),
	    tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_BYTES(buf, len, &tok->tt.ip.dest, sizeof(tok->tt.ip.dest),
	    tok->len, err);
	if (err)
		return (-1);

	return (0);
}

static void
print_ip_tok(FILE *fp, tokenstr_t *tok, char *del, char raw,
    __unused char sfrm)
{

	print_tok_type(fp, tok->id, "ip", raw);
	print_delim(fp, del);
	print_mem(fp, (u_char *)(&tok->tt.ip.version), sizeof(u_char));
	print_delim(fp, del);
	print_mem(fp, (u_char *)(&tok->tt.ip.tos), sizeof(u_char));
	print_delim(fp, del);
	print_2_bytes(fp, ntohs(tok->tt.ip.len), "%u");
	print_delim(fp, del);
	print_2_bytes(fp, ntohs(tok->tt.ip.id), "%u");
	print_delim(fp, del);
	print_2_bytes(fp, ntohs(tok->tt.ip.offset), "%u");
	print_delim(fp, del);
	print_mem(fp, (u_char *)(&tok->tt.ip.ttl), sizeof(u_char));
	print_delim(fp, del);
	print_mem(fp, (u_char *)(&tok->tt.ip.prot), sizeof(u_char));
	print_delim(fp, del);
	print_2_bytes(fp, ntohs(tok->tt.ip.chksm), "%u");
	print_delim(fp, del);
	print_ip_address(fp, tok->tt.ip.src);
	print_delim(fp, del);
	print_ip_address(fp, tok->tt.ip.dest);
}

/*
 * object ID type       1 byte
 * Object ID            4 bytes
 */
static int
fetch_ipc_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_U_CHAR(buf, len, tok->tt.ipc.type, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.ipc.id, tok->len, err);
	if (err)
		return (-1);

	return (0);
}

static void
print_ipc_tok(FILE *fp, tokenstr_t *tok, char *del, char raw,
    __unused char sfrm)
{

	print_tok_type(fp, tok->id, "IPC", raw);
	print_delim(fp, del);
	print_ipctype(fp, tok->tt.ipc.type, raw);
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.ipc.id, "%u");
}

/*
 * owner user id        4 bytes
 * owner group id       4 bytes
 * creator user id      4 bytes
 * creator group id     4 bytes
 * access mode          4 bytes
 * slot seq                     4 bytes
 * key                          4 bytes
 */
static int
fetch_ipcperm_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_U_INT32(buf, len, tok->tt.ipcperm.uid, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.ipcperm.gid, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.ipcperm.puid, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.ipcperm.pgid, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.ipcperm.mode, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.ipcperm.seq, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.ipcperm.key, tok->len, err);
	if (err)
		return (-1);

	return (0);
}

static void
print_ipcperm_tok(FILE *fp, tokenstr_t *tok, char *del, char raw,
    __unused char sfrm)
{

	print_tok_type(fp, tok->id, "IPC perm", raw);
	print_delim(fp, del);
	print_user(fp, tok->tt.ipcperm.uid, raw);
	print_delim(fp, del);
	print_group(fp, tok->tt.ipcperm.gid, raw);
	print_delim(fp, del);
	print_user(fp, tok->tt.ipcperm.puid, raw);
	print_delim(fp, del);
	print_group(fp, tok->tt.ipcperm.pgid, raw);
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.ipcperm.mode, "%o");
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.ipcperm.seq, "%u");
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.ipcperm.key, "%u");
}

/*
 * port Ip address  2 bytes
 */
static int
fetch_iport_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_BYTES(buf, len, &tok->tt.iport.port, sizeof(uint16_t),
	    tok->len, err);
	if (err)
		return (-1);

	return (0);
}

static void
print_iport_tok(FILE *fp, tokenstr_t *tok, char *del, char raw,
    __unused char sfrm)
{

	print_tok_type(fp, tok->id, "ip port", raw);
	print_delim(fp, del);
	print_2_bytes(fp, ntohs(tok->tt.iport.port), "%#x");
}

/*
 * size                         2 bytes
 * data                         size bytes
 */
static int
fetch_opaque_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_U_INT16(buf, len, tok->tt.opaque.size, tok->len, err);
	if (err)
		return (-1);

	SET_PTR(buf, len, tok->tt.opaque.data, tok->tt.opaque.size, tok->len,
	    err);
	if (err)
		return (-1);

	return (0);
}

static void
print_opaque_tok(FILE *fp, tokenstr_t *tok, char *del, char raw,
    __unused char sfrm)
{

	print_tok_type(fp, tok->id, "opaque", raw);
	print_delim(fp, del);
	print_2_bytes(fp, tok->tt.opaque.size, "%u");
	print_delim(fp, del);
	print_mem(fp, tok->tt.opaque.data, tok->tt.opaque.size);
}

/*
 * size                         2 bytes
 * data                         size bytes
 */
static int
fetch_path_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_U_INT16(buf, len, tok->tt.path.len, tok->len, err);
	if (err)
		return (-1);

	SET_PTR(buf, len, tok->tt.path.path, tok->tt.path.len, tok->len, err);
	if (err)
		return (-1);

	return (0);
}

static void
print_path_tok(FILE *fp, tokenstr_t *tok, char *del, char raw,
    __unused char sfrm)
{

	print_tok_type(fp, tok->id, "path", raw);
	print_delim(fp, del);
	print_string(fp, tok->tt.path.path, tok->tt.path.len);
}

/*
 * token ID                     1 byte
 * audit ID                     4 bytes
 * euid                         4 bytes
 * egid                         4 bytes
 * ruid                         4 bytes
 * rgid                         4 bytes
 * pid                          4 bytes
 * sessid                       4 bytes
 * terminal ID
 *   portid             4 bytes
 *   machine id         4 bytes
 */
static int
fetch_process32_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_U_INT32(buf, len, tok->tt.proc32.auid, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.proc32.euid, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.proc32.egid, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.proc32.ruid, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.proc32.rgid, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.proc32.pid, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.proc32.sid, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.proc32.tid.port, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_BYTES(buf, len, &tok->tt.proc32.tid.addr,
	    sizeof(tok->tt.proc32.tid.addr), tok->len, err);
	if (err)
		return (-1);

	return (0);
}

static void
print_process32_tok(FILE *fp, tokenstr_t *tok, char *del, char raw,
    __unused char sfrm)
{

	print_tok_type(fp, tok->id, "process", raw);
	print_delim(fp, del);
	print_user(fp, tok->tt.proc32.auid, raw);
	print_delim(fp, del);
	print_user(fp, tok->tt.proc32.euid, raw);
	print_delim(fp, del);
	print_group(fp, tok->tt.proc32.egid, raw);
	print_delim(fp, del);
	print_user(fp, tok->tt.proc32.ruid, raw);
	print_delim(fp, del);
	print_group(fp, tok->tt.proc32.rgid, raw);
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.proc32.pid, "%u");
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.proc32.sid, "%u");
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.proc32.tid.port, "%u");
	print_delim(fp, del);
	print_ip_address(fp, tok->tt.proc32.tid.addr);
}

static int
fetch_process32ex_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_U_INT32(buf, len, tok->tt.proc32_ex.auid, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.proc32_ex.euid, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.proc32_ex.egid, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.proc32_ex.ruid, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.proc32_ex.rgid, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.proc32_ex.pid, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.proc32_ex.sid, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.proc32_ex.tid.port, tok->len,
	    err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.proc32_ex.tid.type, tok->len,
	    err);
	if (err)
		return (-1);

	if (tok->tt.proc32_ex.tid.type == AU_IPv4) {
		READ_TOKEN_BYTES(buf, len, &tok->tt.proc32_ex.tid.addr[0],
		    sizeof(tok->tt.proc32_ex.tid.addr[0]), tok->len, err);
		if (err)
			return (-1);
	} else if (tok->tt.proc32_ex.tid.type == AU_IPv6) {
		READ_TOKEN_BYTES(buf, len, tok->tt.proc32_ex.tid.addr,
		    sizeof(tok->tt.proc32_ex.tid.addr), tok->len, err);
		if (err)
			return (-1);
	} else
		return (-1);

	return (0);
}

static void
print_process32ex_tok(FILE *fp, tokenstr_t *tok, char *del, char raw,
    __unused char sfrm)
{

	print_tok_type(fp, tok->id, "process_ex", raw);
	print_delim(fp, del);
	print_user(fp, tok->tt.proc32_ex.auid, raw);
	print_delim(fp, del);
	print_user(fp, tok->tt.proc32_ex.euid, raw);
	print_delim(fp, del);
	print_group(fp, tok->tt.proc32_ex.egid, raw);
	print_delim(fp, del);
	print_user(fp, tok->tt.proc32_ex.ruid, raw);
	print_delim(fp, del);
	print_group(fp, tok->tt.proc32_ex.rgid, raw);
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.proc32_ex.pid, "%u");
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.proc32_ex.sid, "%u");
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.proc32_ex.tid.port, "%u");
	print_delim(fp, del);
	print_ip_ex_address(fp, tok->tt.proc32_ex.tid.type,
	    tok->tt.proc32_ex.tid.addr);
}

/*
 * errno                        1 byte
 * return value         4 bytes
 */
static int
fetch_return32_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_U_CHAR(buf, len, tok->tt.ret32.status, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.ret32.ret, tok->len, err);
	if (err)
		return (-1);

	return (0);
}

static void
print_return32_tok(FILE *fp, tokenstr_t *tok, char *del, char raw,
    __unused char sfrm)
{

	print_tok_type(fp, tok->id, "return", raw);
	print_delim(fp, del);
	print_retval(fp, tok->tt.ret32.status, raw);
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.ret32.ret, "%u");
}

static int
fetch_return64_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_U_CHAR(buf, len, tok->tt.ret64.err, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT64(buf, len, tok->tt.ret64.val, tok->len, err);
	if (err)
		return (-1);

	return (0);
}

static void
print_return64_tok(FILE *fp, tokenstr_t *tok, char *del, char raw,
    __unused char sfrm)
{

	print_tok_type(fp, tok->id, "return", raw);
	print_delim(fp, del);
	print_retval(fp, tok->tt.ret64.err, raw);
	print_delim(fp, del);
	print_8_bytes(fp, tok->tt.ret64.val, "%lld");
}

/*
 * seq                          4 bytes
 */
static int
fetch_seq_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_U_INT32(buf, len, tok->tt.seq.seqno, tok->len, err);
	if (err)
		return (-1);

	return (0);
}

static void
print_seq_tok(FILE *fp, tokenstr_t *tok, char *del, char raw,
    __unused char sfrm)
{

	print_tok_type(fp, tok->id, "sequence", raw);
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.seq.seqno, "%u");
}

/*
 * socket family           2 bytes
 * local port              2 bytes
 * socket address          4 bytes
 */
static int
fetch_sock_inet32_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_U_INT16(buf, len, tok->tt.sockinet32.family, tok->len,
	    err);
	if (err)
		return (-1);

	READ_TOKEN_BYTES(buf, len, &tok->tt.sockinet32.port,
	    sizeof(uint16_t), tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_BYTES(buf, len, &tok->tt.sockinet32.addr,
	    sizeof(tok->tt.sockinet32.addr), tok->len, err);
	if (err)
		return (-1);

	return (0);
}

static void
print_sock_inet32_tok(FILE *fp, tokenstr_t *tok, char *del, char raw,
    __unused char sfrm)
{

	print_tok_type(fp, tok->id, "socket-inet", raw);
	print_delim(fp, del);
	print_2_bytes(fp, tok->tt.sockinet32.family, "%u");
	print_delim(fp, del);
	print_2_bytes(fp, ntohs(tok->tt.sockinet32.port), "%u");
	print_delim(fp, del);
	print_ip_address(fp, tok->tt.sockinet32.addr);
}

/*
 * socket family           2 bytes
 * path                    104 bytes
 */
static int
fetch_sock_unix_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_U_INT16(buf, len, tok->tt.sockunix.family, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_BYTES(buf, len, tok->tt.sockunix.path, 104, tok->len,
	    err);
	if (err)
		return (-1);

	return (0);
}

static void
print_sock_unix_tok(FILE *fp, tokenstr_t *tok, char *del, char raw,
    __unused char sfrm)
{

	print_tok_type(fp, tok->id, "socket-unix", raw);
	print_delim(fp, del);
	print_2_bytes(fp, tok->tt.sockunix.family, "%u");
	print_delim(fp, del);
	print_string(fp, tok->tt.sockunix.path,
	    strlen(tok->tt.sockunix.path));
}

/*
 * socket type             2 bytes
 * local port              2 bytes
 * local address           4 bytes
 * remote port             2 bytes
 * remote address          4 bytes
 */
static int
fetch_socket_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_U_INT16(buf, len, tok->tt.socket.type, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_BYTES(buf, len, &tok->tt.socket.l_port, sizeof(uint16_t),
	    tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_BYTES(buf, len, &tok->tt.socket.l_addr,
	    sizeof(tok->tt.socket.l_addr), tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_BYTES(buf, len, &tok->tt.socket.r_port, sizeof(uint16_t),
	    tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_BYTES(buf, len, &tok->tt.socket.l_addr,
	    sizeof(tok->tt.socket.r_addr), tok->len, err);
	if (err)
		return (-1);

	return (0);
}

static void
print_socket_tok(FILE *fp, tokenstr_t *tok, char *del, char raw,
    __unused char sfrm)
{

	print_tok_type(fp, tok->id, "socket", raw);
	print_delim(fp, del);
	print_2_bytes(fp, tok->tt.socket.type, "%u");
	print_delim(fp, del);
	print_2_bytes(fp, ntohs(tok->tt.socket.l_port), "%u");
	print_delim(fp, del);
	print_ip_address(fp, tok->tt.socket.l_addr);
	print_delim(fp, del);
	print_2_bytes(fp, ntohs(tok->tt.socket.r_port), "%u");
	print_delim(fp, del);
	print_ip_address(fp, tok->tt.socket.r_addr);
}

/*
 * audit ID                     4 bytes
 * euid                         4 bytes
 * egid                         4 bytes
 * ruid                         4 bytes
 * rgid                         4 bytes
 * pid                          4 bytes
 * sessid                       4 bytes
 * terminal ID
 *   portid             4 bytes/8 bytes (32-bit/64-bit value)
 *   machine id         4 bytes
 */
static int
fetch_subject32_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_U_INT32(buf, len, tok->tt.subj32.auid, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.subj32.euid, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.subj32.egid, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.subj32.ruid, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.subj32.rgid, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.subj32.pid, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.subj32.sid, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.subj32.tid.port, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_BYTES(buf, len, &tok->tt.subj32.tid.addr,
	    sizeof(tok->tt.subj32.tid.addr), tok->len, err);
	if (err)
		return (-1);

	return (0);
}

static void
print_subject32_tok(FILE *fp, tokenstr_t *tok, char *del, char raw,
    __unused char sfrm)
{

	print_tok_type(fp, tok->id, "subject", raw);
	print_delim(fp, del);
	print_user(fp, tok->tt.subj32.auid, raw);
	print_delim(fp, del);
	print_user(fp, tok->tt.subj32.euid, raw);
	print_delim(fp, del);
	print_group(fp, tok->tt.subj32.egid, raw);
	print_delim(fp, del);
	print_user(fp, tok->tt.subj32.ruid, raw);
	print_delim(fp, del);
	print_group(fp, tok->tt.subj32.rgid, raw);
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.subj32.pid, "%u");
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.subj32.sid, "%u");
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.subj32.tid.port, "%u");
	print_delim(fp, del);
	print_ip_address(fp, tok->tt.subj32.tid.addr);
}

/*
 * audit ID                     4 bytes
 * euid                         4 bytes
 * egid                         4 bytes
 * ruid                         4 bytes
 * rgid                         4 bytes
 * pid                          4 bytes
 * sessid                       4 bytes
 * terminal ID
 *   portid             4 bytes/8 bytes (32-bit/64-bit value)
 *   machine id         4 bytes
 */
static int
fetch_subject64_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_U_INT32(buf, len, tok->tt.subj64.auid, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.subj64.euid, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.subj64.egid, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.subj64.ruid, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.subj64.rgid, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.subj64.pid, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.subj64.sid, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT64(buf, len, tok->tt.subj64.tid.port, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_BYTES(buf, len, &tok->tt.subj64.tid.addr,
	    sizeof(tok->tt.subj64.tid.addr), tok->len, err);
	if (err)
		return (-1);

	return (0);
}

static void
print_subject64_tok(FILE *fp, tokenstr_t *tok, char *del, char raw,
    __unused char sfrm)
{

	print_tok_type(fp, tok->id, "subject", raw);
	print_delim(fp, del);
	print_user(fp, tok->tt.subj64.auid, raw);
	print_delim(fp, del);
	print_user(fp, tok->tt.subj64.euid, raw);
	print_delim(fp, del);
	print_group(fp, tok->tt.subj64.egid, raw);
	print_delim(fp, del);
	print_user(fp, tok->tt.subj64.ruid, raw);
	print_delim(fp, del);
	print_group(fp, tok->tt.subj64.rgid, raw);
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.subj64.pid, "%u");
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.subj64.sid, "%u");
	print_delim(fp, del);
	print_8_bytes(fp, tok->tt.subj64.tid.port, "%llu");
	print_delim(fp, del);
	print_ip_address(fp, tok->tt.subj64.tid.addr);
}

/*
 * audit ID                     4 bytes
 * euid                         4 bytes
 * egid                         4 bytes
 * ruid                         4 bytes
 * rgid                         4 bytes
 * pid                          4 bytes
 * sessid                       4 bytes
 * terminal ID
 *   portid             4 bytes
 *	 type				4 bytes
 *   machine id         16 bytes
 */
static int
fetch_subject32ex_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_U_INT32(buf, len, tok->tt.subj32_ex.auid, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.subj32_ex.euid, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.subj32_ex.egid, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.subj32_ex.ruid, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.subj32_ex.rgid, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.subj32_ex.pid, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.subj32_ex.sid, tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.subj32_ex.tid.port, tok->len,
	    err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.subj32_ex.tid.type, tok->len,
	    err);
	if (err)
		return (-1);

	if (tok->tt.subj32_ex.tid.type == AU_IPv4) {
		READ_TOKEN_BYTES(buf, len, &tok->tt.subj32_ex.tid.addr[0],
		    sizeof(tok->tt.subj32_ex.tid.addr[0]), tok->len, err);
		if (err)
			return (-1);
	} else if (tok->tt.subj32_ex.tid.type == AU_IPv6) {
		READ_TOKEN_BYTES(buf, len, tok->tt.subj32_ex.tid.addr,
		    sizeof(tok->tt.subj32_ex.tid.addr), tok->len, err);
		if (err)
			return (-1);
	} else
		return (-1);

	return (0);
}

static void
print_subject32ex_tok(FILE *fp, tokenstr_t *tok, char *del, char raw,
    __unused char sfrm)
{

	print_tok_type(fp, tok->id, "subject_ex", raw);
	print_delim(fp, del);
	print_user(fp, tok->tt.subj32_ex.auid, raw);
	print_delim(fp, del);
	print_user(fp, tok->tt.subj32_ex.euid, raw);
	print_delim(fp, del);
	print_group(fp, tok->tt.subj32_ex.egid, raw);
	print_delim(fp, del);
	print_user(fp, tok->tt.subj32_ex.ruid, raw);
	print_delim(fp, del);
	print_group(fp, tok->tt.subj32_ex.rgid, raw);
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.subj32_ex.pid, "%u");
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.subj32_ex.sid, "%u");
	print_delim(fp, del);
	print_4_bytes(fp, tok->tt.subj32_ex.tid.port, "%u");
	print_delim(fp, del);
	print_ip_ex_address(fp, tok->tt.subj32_ex.tid.type,
	    tok->tt.subj32_ex.tid.addr);
}

/*
 * size                         2 bytes
 * data                         size bytes
 */
static int
fetch_text_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_U_INT16(buf, len, tok->tt.text.len, tok->len, err);
	if (err)
		return (-1);

	SET_PTR(buf, len, tok->tt.text.text, tok->tt.text.len, tok->len,
	    err);
	if (err)
		return (-1);

	return (0);
}

static void
print_text_tok(FILE *fp, tokenstr_t *tok, char *del, char raw,
    __unused char sfrm)
{

	print_tok_type(fp, tok->id, "text", raw);
	print_delim(fp, del);
	print_string(fp, tok->tt.text.text, tok->tt.text.len);
}

/*
 * socket type             2 bytes
 * local port              2 bytes
 * address type/length     4 bytes
 * local Internet address  4 bytes
 * remote port             4 bytes
 * address type/length     4 bytes
 * remote Internet address 4 bytes
 */
static int
fetch_socketex32_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;

	READ_TOKEN_U_INT16(buf, len, tok->tt.socket_ex32.type, tok->len,
	    err);
	if (err)
		return (-1);

	READ_TOKEN_BYTES(buf, len, &tok->tt.socket_ex32.l_port,
	    sizeof(uint16_t), tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.socket_ex32.l_ad_type, tok->len,
	    err);
	if (err)
		return (-1);

	READ_TOKEN_BYTES(buf, len, &tok->tt.socket_ex32.l_addr,
	    sizeof(tok->tt.socket_ex32.l_addr), tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_BYTES(buf, len, &tok->tt.socket_ex32.r_port,
	    sizeof(uint16_t), tok->len, err);
	if (err)
		return (-1);

	READ_TOKEN_U_INT32(buf, len, tok->tt.socket_ex32.r_ad_type, tok->len,
	    err);
	if (err)
		return (-1);

	READ_TOKEN_BYTES(buf, len, &tok->tt.socket_ex32.r_addr,
	    sizeof(tok->tt.socket_ex32.r_addr), tok->len, err);
	if (err)
		return (-1);

	return (0);
}

static void
print_socketex32_tok(FILE *fp, tokenstr_t *tok, char *del, char raw,
    __unused char sfrm)
{

	print_tok_type(fp, tok->id, "socket", raw);
	print_delim(fp, del);
	print_2_bytes(fp, tok->tt.socket_ex32.type, "%#x");
	print_delim(fp, del);
	print_2_bytes(fp, ntohs(tok->tt.socket_ex32.l_port), "%#x");
	print_delim(fp, del);
	print_ip_address(fp, tok->tt.socket_ex32.l_addr);
	print_delim(fp, del);
	print_4_bytes(fp, ntohs(tok->tt.socket_ex32.r_port), "%#x");
	print_delim(fp, del);
	print_ip_address(fp, tok->tt.socket_ex32.r_addr);
}

static int
fetch_invalid_tok(tokenstr_t *tok, char *buf, int len)
{
	int err = 0;
	int recoversize;

	recoversize = len - (tok->len + AUDIT_TRAILER_SIZE);
	if (recoversize <= 0)
		return (-1);

	tok->tt.invalid.length = recoversize;

	SET_PTR(buf, len, tok->tt.invalid.data, recoversize, tok->len, err);
	if (err)
		return (-1);

	return (0);
}

static void
print_invalid_tok(FILE *fp, tokenstr_t *tok, char *del, char raw,
    __unused char sfrm)
{

	print_tok_type(fp, tok->id, "unknown", raw);
	print_delim(fp, del);
	print_mem(fp, tok->tt.invalid.data, tok->tt.invalid.length);
}


/*
 * Reads the token beginning at buf into tok.
 */
int
au_fetch_tok(tokenstr_t *tok, u_char *buf, int len)
{

	if (len <= 0)
		return (-1);

	tok->len = 1;
	tok->data = buf;
	tok->id = *buf;

	switch(tok->id) {
	case AUT_HEADER32:
		return (fetch_header32_tok(tok, buf, len));

	case AUT_HEADER32_EX:
		return (fetch_header32_ex_tok(tok, buf, len));

	case AUT_HEADER64:
		return (fetch_header64_tok(tok, buf, len));

	case AUT_HEADER64_EX:
		return (fetch_header64_ex_tok(tok, buf, len));

	case AUT_TRAILER:
		return (fetch_trailer_tok(tok, buf, len));

	case AUT_ARG32:
		return (fetch_arg32_tok(tok, buf, len));

	case AUT_ARG64:
		return (fetch_arg64_tok(tok, buf, len));

	case AUT_ATTR32:
		return (fetch_attr32_tok(tok, buf, len));

	case AUT_ATTR64:
		return (fetch_attr64_tok(tok, buf, len));

	case AUT_EXIT:
		return (fetch_exit_tok(tok, buf, len));

	case AUT_EXEC_ARGS:
		return (fetch_execarg_tok(tok, buf, len));

	case AUT_EXEC_ENV:
		return (fetch_execenv_tok(tok, buf, len));

	case AUT_OTHER_FILE32:
		return (fetch_file_tok(tok, buf, len));

	case AUT_NEWGROUPS:
		return (fetch_newgroups_tok(tok, buf, len));

	case AUT_IN_ADDR:
		return (fetch_inaddr_tok(tok, buf, len));

	case AUT_IN_ADDR_EX:
		return (fetch_inaddr_ex_tok(tok, buf, len));

	case AUT_IP:
		return (fetch_ip_tok(tok, buf, len));

	case AUT_IPC:
		return (fetch_ipc_tok(tok, buf, len));

	case AUT_IPC_PERM:
		return (fetch_ipcperm_tok(tok, buf, len));

	case AUT_IPORT:
		return (fetch_iport_tok(tok, buf, len));

	case AUT_OPAQUE:
		return (fetch_opaque_tok(tok, buf, len));

	case AUT_PATH:
		return (fetch_path_tok(tok, buf, len));

	case AUT_PROCESS32:
		return (fetch_process32_tok(tok, buf, len));

	case AUT_PROCESS32_EX:
		return (fetch_process32ex_tok(tok, buf, len));

	case AUT_RETURN32:
		return (fetch_return32_tok(tok, buf, len));

	case AUT_RETURN64:
		return (fetch_return64_tok(tok, buf, len));

	case AUT_SEQ:
		return (fetch_seq_tok(tok, buf, len));

	case AUT_SOCKET:
		return (fetch_socket_tok(tok, buf, len));

	case AUT_SOCKINET32:
		return (fetch_sock_inet32_tok(tok, buf, len));

	case AUT_SOCKUNIX:
		return (fetch_sock_unix_tok(tok, buf, len));

	case AUT_SUBJECT32:
		return (fetch_subject32_tok(tok, buf, len));

	case AUT_SUBJECT64:
		return (fetch_subject64_tok(tok, buf, len));

	case AUT_SUBJECT32_EX:
		return (fetch_subject32ex_tok(tok, buf, len));

	case AUT_TEXT:
		return (fetch_text_tok(tok, buf, len));

	case AUT_SOCKET_EX:
		return (fetch_socketex32_tok(tok, buf, len));

	case AUT_DATA:
		return (fetch_arb_tok(tok, buf, len));

	default:
		return (fetch_invalid_tok(tok, buf, len));
	}
}

/*
 * 'prints' the token out to outfp
 */
void
au_print_tok(FILE *outfp, tokenstr_t *tok, char *del, char raw, char sfrm)
{

	switch(tok->id) {
	case AUT_HEADER32:
		print_header32_tok(outfp, tok, del, raw, sfrm);
		return;

	case AUT_HEADER32_EX:
		print_header32_ex_tok(outfp, tok, del, raw, sfrm);
		return;

	case AUT_HEADER64:
		print_header64_tok(outfp, tok, del, raw, sfrm);
		return;

	case AUT_HEADER64_EX:
		print_header64_ex_tok(outfp, tok, del, raw, sfrm);
		return;

	case AUT_TRAILER:
		print_trailer_tok(outfp, tok, del, raw, sfrm);
		return;

	case AUT_ARG32:
		print_arg32_tok(outfp, tok, del, raw, sfrm);
		return;

	case AUT_ARG64:
		print_arg64_tok(outfp, tok, del, raw, sfrm);
		return;

	case AUT_DATA:
		print_arb_tok(outfp, tok, del, raw, sfrm);
		return;

	case AUT_ATTR32:
		print_attr32_tok(outfp, tok, del, raw, sfrm);
		return;

	case AUT_ATTR64:
		print_attr64_tok(outfp, tok, del, raw, sfrm);
		return;

	case AUT_EXIT:
		print_exit_tok(outfp, tok, del, raw, sfrm);
		return;

	case AUT_EXEC_ARGS:
		print_execarg_tok(outfp, tok, del, raw, sfrm);
		return;

	case AUT_EXEC_ENV:
		print_execenv_tok(outfp, tok, del, raw, sfrm);
		return;

	case AUT_OTHER_FILE32:
		print_file_tok(outfp, tok, del, raw, sfrm);
		return;

	case AUT_NEWGROUPS:
		print_newgroups_tok(outfp, tok, del, raw, sfrm);
		return;

	case AUT_IN_ADDR:
		print_inaddr_tok(outfp, tok, del, raw, sfrm);
		return;

	case AUT_IN_ADDR_EX:
		print_inaddr_ex_tok(outfp, tok, del, raw, sfrm);
		return;

	case AUT_IP:
		print_ip_tok(outfp, tok, del, raw, sfrm);
		return;

	case AUT_IPC:
		print_ipc_tok(outfp, tok, del, raw, sfrm);
		return;

	case AUT_IPC_PERM:
		print_ipcperm_tok(outfp, tok, del, raw, sfrm);
		return;

	case AUT_IPORT:
		print_iport_tok(outfp, tok, del, raw, sfrm);
		return;

	case AUT_OPAQUE:
		print_opaque_tok(outfp, tok, del, raw, sfrm);
		return;

	case AUT_PATH:
		print_path_tok(outfp, tok, del, raw, sfrm);
		return;

	case AUT_PROCESS32:
		print_process32_tok(outfp, tok, del, raw, sfrm);
		return;

	case AUT_PROCESS32_EX:
		print_process32ex_tok(outfp, tok, del, raw, sfrm);
		return;

	case AUT_RETURN32:
		print_return32_tok(outfp, tok, del, raw, sfrm);
		return;

	case AUT_RETURN64:
		print_return64_tok(outfp, tok, del, raw, sfrm);
		return;

	case AUT_SEQ:
		print_seq_tok(outfp, tok, del, raw, sfrm);
		return;

	case AUT_SOCKET:
		print_socket_tok(outfp, tok, del, raw, sfrm);
		return;

	case AUT_SOCKINET32:
		print_sock_inet32_tok(outfp, tok, del, raw, sfrm);
		return;

	case AUT_SOCKUNIX:
		print_sock_unix_tok(outfp, tok, del, raw, sfrm);
		return;

	case AUT_SUBJECT32:
		print_subject32_tok(outfp, tok, del, raw, sfrm);
		return;

	case AUT_SUBJECT64:
		print_subject64_tok(outfp, tok, del, raw, sfrm);
		return;

	case AUT_SUBJECT32_EX:
		print_subject32ex_tok(outfp, tok, del, raw, sfrm);
		return;

	case AUT_TEXT:
		print_text_tok(outfp, tok, del, raw, sfrm);
		return;

	case AUT_SOCKET_EX:
		print_socketex32_tok(outfp, tok, del, raw, sfrm);
		return;

	default:
		print_invalid_tok(outfp, tok, del, raw, sfrm);
	}
}

/*
 * Read a record from the file pointer, store data in buf memory for buf is
 * also allocated in this function and has to be free'd outside this call.
 *
 * au_read_rec() handles two possibilities: a stand-alone file token, or a
 * complete audit record.
 *
 * XXXRW: Note that if we hit an error, we leave the stream in an unusable
 * state, because it will be partly offset into a record.  We should rewind
 * or do something more intelligent.  Particularly interesting is the case
 * where we perform a partial read of a record from a non-blockable file
 * descriptor.  We should return the partial read and continue...?
 */
int
au_read_rec(FILE *fp, u_char **buf)
{
	u_char *bptr;
	u_int32_t recsize;
	u_int32_t bytestoread;
	u_char type;

	u_int32_t sec, msec;
	u_int16_t filenamelen;

	type = fgetc(fp);

	switch (type) {
	case AUT_HEADER32:
	case AUT_HEADER32_EX:
	case AUT_HEADER64:
	case AUT_HEADER64_EX:
		/* read the record size from the token */
		if (fread(&recsize, 1, sizeof(u_int32_t), fp) <
		    sizeof(u_int32_t)) {
			errno = EINVAL;
			return (-1);
		}
		recsize = be32toh(recsize);

		/* Check for recsize sanity */
		if (recsize < (sizeof(u_int32_t) + sizeof(u_char))) {
			errno = EINVAL;
			return (-1);
		}

		*buf = malloc(recsize * sizeof(u_char));
		if (*buf == NULL)
			return (-1);
		bptr = *buf;
		memset(bptr, 0, recsize);

		/* store the token contents already read, back to the buffer*/
		*bptr = type;
		bptr++;
		be32enc(bptr, recsize);
		bptr += sizeof(u_int32_t);

		/* now read remaining record bytes */
		bytestoread = recsize - (sizeof(u_int32_t) + sizeof(u_char));

		if (fread(bptr, 1, bytestoread, fp) < bytestoread) {
			free(*buf);
			errno = EINVAL;
			return (-1);
		}
		break;

	case AUT_OTHER_FILE32:
		/*
		 * The file token is variable-length, as it includes a
		 * pathname.  As a result, we have to read incrementally
		 * until we know the total length, then allocate space and
		 * read the rest.
		 */
		if (fread(&sec, 1, sizeof(sec), fp) < sizeof(sec)) {
			errno = EINVAL;
			return (-1);
		}
		if (fread(&msec, 1, sizeof(msec), fp) < sizeof(msec)) {
			errno = EINVAL;
			return (-1);
		}
		if (fread(&filenamelen, 1, sizeof(filenamelen), fp) <
		    sizeof(filenamelen)) {
			errno = EINVAL;
			return (-1);
		}
		recsize = sizeof(type) + sizeof(sec) + sizeof(msec) +
		    sizeof(filenamelen) + ntohs(filenamelen);
		*buf = malloc(recsize);
		if (*buf == NULL)
			return (-1);
		bptr = *buf;

		bcopy(&type, bptr, sizeof(type));
		bptr += sizeof(type);
		bcopy(&sec, bptr, sizeof(sec));
		bptr += sizeof(sec);
		bcopy(&msec, bptr, sizeof(msec));
		bptr += sizeof(msec);
		bcopy(&filenamelen, bptr, sizeof(filenamelen));
		bptr += sizeof(filenamelen);

		if (fread(bptr, 1, ntohs(filenamelen), fp) <
		    ntohs(filenamelen)) {
			free(buf);
			errno = EINVAL;
			return (-1);
		}
		break;

	default:
		errno = EINVAL;
		return (-1);
	}

	return (recsize);
}
