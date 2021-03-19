/*
 * Testing tool for TLSv1 client routines using HTTPS
 * Copyright (c) 2011, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"
#include <netdb.h>

#include "common.h"
#include "crypto/tls.h"


static void https_tls_event_cb(void *ctx, enum tls_event ev,
			       union tls_event_data *data)
{
	wpa_printf(MSG_DEBUG, "HTTPS: TLS event %d", ev);
}


static struct wpabuf * https_recv(int s)
{
	struct wpabuf *in;
	int len, ret;
	fd_set rfds;
	struct timeval tv;

	in = wpabuf_alloc(20000);
	if (in == NULL)
		return NULL;

	FD_ZERO(&rfds);
	FD_SET(s, &rfds);
	tv.tv_sec = 5;
	tv.tv_usec = 0;

	wpa_printf(MSG_DEBUG, "Waiting for more data");
	ret = select(s + 1, &rfds, NULL, NULL, &tv);
	if (ret < 0) {
		wpa_printf(MSG_ERROR, "select: %s", strerror(errno));
		wpabuf_free(in);
		return NULL;
	}
	if (ret == 0) {
		/* timeout */
		wpa_printf(MSG_INFO, "Timeout on waiting for data");
		wpabuf_free(in);
		return NULL;
	}

	len = recv(s, wpabuf_put(in, 0), wpabuf_tailroom(in), 0);
	if (len < 0) {
		wpa_printf(MSG_ERROR, "recv: %s", strerror(errno));
		wpabuf_free(in);
		return NULL;
	}
	if (len == 0) {
		wpa_printf(MSG_DEBUG, "No more data available");
		wpabuf_free(in);
		return NULL;
	}
	wpa_printf(MSG_DEBUG, "Received %d bytes", len);
	wpabuf_put(in, len);

	return in;
}


static int https_client(int s, const char *path)
{
	struct tls_config conf;
	void *tls;
	struct tls_connection *conn;
	struct wpabuf *in, *out, *appl;
	int res = -1;
	int need_more_data;

	os_memset(&conf, 0, sizeof(conf));
	conf.event_cb = https_tls_event_cb;
	tls = tls_init(&conf);
	if (tls == NULL)
		return -1;

	conn = tls_connection_init(tls);
	if (conn == NULL) {
		tls_deinit(tls);
		return -1;
	}

	in = NULL;

	for (;;) {
		appl = NULL;
		out = tls_connection_handshake2(tls, conn, in, &appl,
						&need_more_data);
		wpabuf_free(in);
		in = NULL;
		if (out == NULL) {
			if (need_more_data)
				goto read_more;
			goto done;
		}
		if (tls_connection_get_failed(tls, conn)) {
			wpa_printf(MSG_ERROR, "TLS handshake failed");
			goto done;
		}
		if (tls_connection_established(tls, conn))
			break;
		wpa_printf(MSG_DEBUG, "Sending %d bytes",
			   (int) wpabuf_len(out));
		if (send(s, wpabuf_head(out), wpabuf_len(out), 0) < 0) {
			wpa_printf(MSG_ERROR, "send: %s", strerror(errno));
			goto done;
		}
		wpabuf_free(out);
		out = NULL;

	read_more:
		in = https_recv(s);
		if (in == NULL)
			goto done;
	}
	wpabuf_free(out);
	out = NULL;

	wpa_printf(MSG_INFO, "TLS connection established");
	if (appl)
		wpa_hexdump_buf(MSG_DEBUG, "Received application data", appl);

	in = wpabuf_alloc(100 + os_strlen(path));
	if (in == NULL)
		goto done;
	wpabuf_put_str(in, "GET ");
	wpabuf_put_str(in, path);
	wpabuf_put_str(in, " HTTP/1.0\r\n\r\n");
	out = tls_connection_encrypt(tls, conn, in);
	wpabuf_free(in);
	in = NULL;
	if (out == NULL)
		goto done;

	wpa_printf(MSG_INFO, "Sending HTTP request: %d bytes",
		   (int) wpabuf_len(out));
	if (send(s, wpabuf_head(out), wpabuf_len(out), 0) < 0) {
		wpa_printf(MSG_ERROR, "send: %s", strerror(errno));
		goto done;
	}
	wpabuf_free(out);
	out = NULL;

	wpa_printf(MSG_INFO, "Reading HTTP response");
	for (;;) {
		int need_more_data;
		in = https_recv(s);
		if (in == NULL)
			goto done;
		out = tls_connection_decrypt2(tls, conn, in, &need_more_data);
		if (need_more_data)
			wpa_printf(MSG_DEBUG, "HTTP: Need more data");
		wpabuf_free(in);
		in = NULL;
		if (out == NULL)
			goto done;
		wpa_hexdump_ascii(MSG_INFO, "Response", wpabuf_head(out),
				  wpabuf_len(out));
		wpabuf_free(out);
		out = NULL;
	}

	res = 0;
done:
	wpabuf_free(out);
	wpabuf_free(in);
	wpabuf_free(appl);
	tls_connection_deinit(tls, conn);
	tls_deinit(tls);

	return res;
}


int main(int argc, char *argv[])
{
	struct addrinfo hints, *result, *rp;
	int res, s;

	wpa_debug_level = 0;
	wpa_debug_show_keys = 1;

	if (argc < 4) {
		wpa_printf(MSG_INFO, "usage: test-https server port path");
		return -1;
	}

	os_memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	res = getaddrinfo(argv[1], argv[2], &hints, &result);
	if (res) {
		wpa_printf(MSG_ERROR, "getaddrinfo: %s", gai_strerror(res));
		return -1;
	}

	for (rp = result; rp; rp = rp->ai_next) {
		s = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (s < 0)
			continue;
		if (connect(s, rp->ai_addr, rp->ai_addrlen) == 0)
			break;
		close(s);
	}
	freeaddrinfo(result);

	if (rp == NULL) {
		wpa_printf(MSG_ERROR, "Could not connect");
		return -1;
	}

	https_client(s, argv[3]);
	close(s);

	return 0;
}
