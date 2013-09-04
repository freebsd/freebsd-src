/*
 * Copyright (C) 2011, 2012  Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id$ */

/*
 * This implements external update-policy rules.  This allows permission
 * to update a zone to be checked by consulting an external daemon (e.g.,
 * kerberos).
 */

#include <config.h>
#include <errno.h>
#include <unistd.h>

#ifdef ISC_PLATFORM_HAVESYSUNH
#include <sys/socket.h>
#include <sys/un.h>
#endif

#include <isc/magic.h>
#include <isc/mem.h>
#include <isc/netaddr.h>
#include <isc/result.h>
#include <isc/string.h>
#include <isc/util.h>
#include <isc/strerror.h>

#include <dns/fixedname.h>
#include <dns/name.h>
#include <dns/ssu.h>
#include <dns/log.h>
#include <dns/rdatatype.h>

#include <dst/dst.h>


static void
ssu_e_log(int level, const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	isc_log_vwrite(dns_lctx, DNS_LOGCATEGORY_SECURITY,
		       DNS_LOGMODULE_ZONE, ISC_LOG_DEBUG(level), fmt, ap);
	va_end(ap);
}


/*
 * Connect to a UNIX domain socket.
 */
static int
ux_socket_connect(const char *path) {
	int fd = -1;
#ifdef ISC_PLATFORM_HAVESYSUNH
	struct sockaddr_un addr;

	REQUIRE(path != NULL);

	if (strlen(path) > sizeof(addr.sun_path)) {
		ssu_e_log(3, "ssu_external: socket path '%s' "
			     "longer than system maximum %u",
			  path, sizeof(addr.sun_path));
		return (-1);
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strlcpy(addr.sun_path, path, sizeof(addr.sun_path));

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd == -1) {
		char strbuf[ISC_STRERRORSIZE];
		isc__strerror(errno, strbuf, sizeof(strbuf));
		ssu_e_log(3, "ssu_external: unable to create socket - %s",
			  strbuf);
		return (-1);
	}

	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		char strbuf[ISC_STRERRORSIZE];
		isc__strerror(errno, strbuf, sizeof(strbuf));
		ssu_e_log(3, "ssu_external: unable to connect to "
			     "socket '%s' - %s",
			  path, strbuf);
		close(fd);
		return (-1);
	}
#endif
	return (fd);
}

/* Change this version if you update the format of the request */
#define SSU_EXTERNAL_VERSION 1

/*
 * Perform an update-policy rule check against an external application
 * over a socket.
 *
 * This currently only supports local: for unix domain datagram sockets.
 *
 * Note that by using a datagram socket and creating a new socket each
 * time we avoid the need for locking and allow for parallel access to
 * the authorization server.
 */
isc_boolean_t
dns_ssu_external_match(dns_name_t *identity,
		       dns_name_t *signer, dns_name_t *name,
		       isc_netaddr_t *tcpaddr, dns_rdatatype_t type,
		       const dst_key_t *key, isc_mem_t *mctx)
{
	char b_identity[DNS_NAME_FORMATSIZE];
	char b_signer[DNS_NAME_FORMATSIZE];
	char b_name[DNS_NAME_FORMATSIZE];
	char b_addr[ISC_NETADDR_FORMATSIZE];
	char b_type[DNS_RDATATYPE_FORMATSIZE];
	char b_key[DST_KEY_FORMATSIZE];
	isc_buffer_t *tkey_token = NULL;
	int fd;
	const char *sock_path;
	size_t req_len;
	isc_region_t token_region;
	unsigned char *data;
	isc_buffer_t buf;
	isc_uint32_t token_len = 0;
	isc_uint32_t reply;
	ssize_t ret;

	/* The identity contains local:/path/to/socket */
	dns_name_format(identity, b_identity, sizeof(b_identity));

	/* For now only local: is supported */
	if (strncmp(b_identity, "local:", 6) != 0) {
		ssu_e_log(3, "ssu_external: invalid socket path '%s'",
			  b_identity);
		return (ISC_FALSE);
	}
	sock_path = &b_identity[6];

	fd = ux_socket_connect(sock_path);
	if (fd == -1)
		return (ISC_FALSE);

	if (key != NULL) {
		dst_key_format(key, b_key, sizeof(b_key));
		tkey_token = dst_key_tkeytoken(key);
	} else
		b_key[0] = 0;

	if (tkey_token != NULL) {
		isc_buffer_region(tkey_token, &token_region);
		token_len = token_region.length;
	}

	/* Format the request elements */
	if (signer != NULL)
		dns_name_format(signer, b_signer, sizeof(b_signer));
	else
		b_signer[0] = 0;

	dns_name_format(name, b_name, sizeof(b_name));

	if (tcpaddr != NULL)
		isc_netaddr_format(tcpaddr, b_addr, sizeof(b_addr));
	else
		b_addr[0] = 0;

	dns_rdatatype_format(type, b_type, sizeof(b_type));

	/* Work out how big the request will be */
	req_len = sizeof(isc_uint32_t)     + /* Format version */
		  sizeof(isc_uint32_t)     + /* Length */
		  strlen(b_signer) + 1 + /* Signer */
		  strlen(b_name) + 1   + /* Name */
		  strlen(b_addr) + 1   + /* Address */
		  strlen(b_type) + 1   + /* Type */
		  strlen(b_key) + 1    + /* Key */
		  sizeof(isc_uint32_t)     + /* tkey_token length */
		  token_len;             /* tkey_token */


	/* format the buffer */
	data = isc_mem_allocate(mctx, req_len);
	if (data == NULL) {
		close(fd);
		return (ISC_FALSE);
	}

	isc_buffer_init(&buf, data, req_len);
	isc_buffer_putuint32(&buf, SSU_EXTERNAL_VERSION);
	isc_buffer_putuint32(&buf, req_len);

	/* Strings must be null-terminated */
	isc_buffer_putstr(&buf, b_signer);
	isc_buffer_putuint8(&buf, 0);
	isc_buffer_putstr(&buf, b_name);
	isc_buffer_putuint8(&buf, 0);
	isc_buffer_putstr(&buf, b_addr);
	isc_buffer_putuint8(&buf, 0);
	isc_buffer_putstr(&buf, b_type);
	isc_buffer_putuint8(&buf, 0);
	isc_buffer_putstr(&buf, b_key);
	isc_buffer_putuint8(&buf, 0);

	isc_buffer_putuint32(&buf, token_len);
	if (tkey_token && token_len != 0)
		isc_buffer_putmem(&buf, token_region.base, token_len);

	ENSURE(isc_buffer_availablelength(&buf) == 0);

	/* Send the request */
	ret = write(fd, data, req_len);
	isc_mem_free(mctx, data);
	if (ret != (ssize_t) req_len) {
		char strbuf[ISC_STRERRORSIZE];
		isc__strerror(errno, strbuf, sizeof(strbuf));
		ssu_e_log(3, "ssu_external: unable to send request - %s",
			  strbuf);
		close(fd);
		return (ISC_FALSE);
	}

	/* Receive the reply */
	ret = read(fd, &reply, sizeof(isc_uint32_t));
	if (ret != (ssize_t) sizeof(isc_uint32_t)) {
		char strbuf[ISC_STRERRORSIZE];
		isc__strerror(errno, strbuf, sizeof(strbuf));
		ssu_e_log(3, "ssu_external: unable to receive reply - %s",
			  strbuf);
		close(fd);
		return (ISC_FALSE);
	}

	close(fd);

	reply = ntohl(reply);

	if (reply == 0) {
		ssu_e_log(3, "ssu_external: denied external auth for '%s'",
			  b_name);
		return (ISC_FALSE);
	} else if (reply == 1) {
		ssu_e_log(3, "ssu_external: allowed external auth for '%s'",
			  b_name);
		return (ISC_TRUE);
	}

	ssu_e_log(3, "ssu_external: invalid reply 0x%08x", reply);

	return (ISC_FALSE);
}
