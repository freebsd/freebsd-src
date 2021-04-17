/*
 * Testing tool for TLSv1 server routines using HTTPS
 * Copyright (c) 2011-2019, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#include "common.h"
#include "crypto/tls.h"


static void https_tls_event_cb(void *ctx, enum tls_event ev,
			       union tls_event_data *data)
{
	wpa_printf(MSG_DEBUG, "HTTPS: TLS event %d", ev);
}


static struct wpabuf * https_recv(int s, int timeout_ms)
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
	tv.tv_sec = timeout_ms / 1000;
	tv.tv_usec = timeout_ms % 1000;

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


static void https_tls_log_cb(void *ctx, const char *msg)
{
	wpa_printf(MSG_DEBUG, "TLS: %s", msg);
}


static int https_server(int s)
{
	struct tls_config conf;
	void *tls;
	struct tls_connection_params params;
	struct tls_connection *conn;
	struct wpabuf *in, *out, *appl;
	int res = -1;

	os_memset(&conf, 0, sizeof(conf));
	conf.event_cb = https_tls_event_cb;
	tls = tls_init(&conf);
	if (!tls)
		return -1;

	os_memset(&params, 0, sizeof(params));
	params.ca_cert = "hwsim/auth_serv/ca.pem";
	params.client_cert = "hwsim/auth_serv/server.pem";
	params.private_key = "hwsim/auth_serv/server.key";
	params.dh_file = "hwsim/auth_serv/dh.conf";

	if (tls_global_set_params(tls, &params)) {
		wpa_printf(MSG_ERROR, "Failed to set TLS parameters");
		tls_deinit(tls);
		return -1;
	}

	conn = tls_connection_init(tls);
	if (!conn) {
		tls_deinit(tls);
		return -1;
	}

	tls_connection_set_log_cb(conn, https_tls_log_cb, NULL);

	for (;;) {
		in = https_recv(s, 5000);
		if (!in)
			goto done;

		appl = NULL;
		out = tls_connection_server_handshake(tls, conn, in, &appl);
		wpabuf_free(in);
		in = NULL;
		if (!out) {
			if (!tls_connection_get_failed(tls, conn) &&
			    !tls_connection_established(tls, conn))
				continue;
			goto done;
		}
		wpa_printf(MSG_DEBUG, "Sending %d bytes",
			   (int) wpabuf_len(out));
		if (send(s, wpabuf_head(out), wpabuf_len(out), 0) < 0) {
			wpa_printf(MSG_ERROR, "send: %s", strerror(errno));
			goto done;
		}
		wpabuf_free(out);
		out = NULL;
		if (tls_connection_get_failed(tls, conn)) {
			wpa_printf(MSG_ERROR, "TLS handshake failed");
			goto done;
		}
		if (tls_connection_established(tls, conn))
			break;
	}
	wpabuf_free(out);
	out = NULL;

	wpa_printf(MSG_INFO, "TLS connection established");
	if (appl)
		wpa_hexdump_buf(MSG_DEBUG, "Received application data", appl);

	wpa_printf(MSG_INFO, "Reading HTTP request");
	for (;;) {
		int need_more_data;

		in = https_recv(s, 5000);
		if (!in)
			goto done;
		out = tls_connection_decrypt2(tls, conn, in, &need_more_data);
		wpabuf_free(in);
		in = NULL;
		if (need_more_data) {
			wpa_printf(MSG_DEBUG, "HTTP: Need more data");
			continue;
		}
		if (!out)
			goto done;
		wpa_hexdump_ascii(MSG_INFO, "Request",
				  wpabuf_head(out), wpabuf_len(out));
		wpabuf_free(out);
		out = NULL;
		break;
	}

	in = wpabuf_alloc(1000);
	if (!in)
		goto done;
	wpabuf_put_str(in, "HTTP/1.1 200 OK\r\n"
		       "Server: test-https_server\r\n"
		       "\r\n"
		       "<HTML><BODY>HELLO</BODY></HTML>\n");
	wpa_hexdump_ascii(MSG_DEBUG, "Response",
			  wpabuf_head(in), wpabuf_len(in));
	out = tls_connection_encrypt(tls, conn, in);
	wpabuf_free(in);
	in = NULL;
	wpa_hexdump_buf(MSG_DEBUG, "Encrypted response", out);
	if (!out)
		goto done;

	wpa_printf(MSG_INFO, "Sending HTTP response: %d bytes",
		   (int) wpabuf_len(out));
	if (send(s, wpabuf_head(out), wpabuf_len(out), 0) < 0) {
		wpa_printf(MSG_ERROR, "send: %s", strerror(errno));
		goto done;
	}
	wpabuf_free(out);
	out = NULL;

	res = 0;
done:
	wpabuf_free(out);
	wpabuf_free(in);
	wpabuf_free(appl);
	tls_connection_deinit(tls, conn);
	tls_deinit(tls);
	close(s);

	return res;
}


int main(int argc, char *argv[])
{
	struct sockaddr_in sin;
	int port, s, conn;
	int on = 1;

	wpa_debug_level = 0;
	wpa_debug_show_keys = 1;

	if (argc < 2) {
		wpa_printf(MSG_INFO, "usage: test-https_server port");
		return -1;
	}

	port = atoi(argv[1]);

	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0) {
		perror("socket");
		return -1;
	}

	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
		wpa_printf(MSG_DEBUG,
			   "HTTP: setsockopt(SO_REUSEADDR) failed: %s",
			   strerror(errno));
		/* try to continue anyway */
	}

	os_memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	if (bind(s, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
		perror("bind");
		close(s);
		return -1;
	}

	if (listen(s, 10) < 0) {
		perror("listen");
		close(s);
		return -1;
	}

	for (;;) {
		struct sockaddr_in addr;
		socklen_t addr_len = sizeof(addr);

		conn = accept(s, (struct sockaddr *) &addr, &addr_len);
		if (conn < 0) {
			perror("accept");
			break;
		}

		wpa_printf(MSG_DEBUG, "-------------------------------------");
		wpa_printf(MSG_DEBUG, "Connection from %s:%d",
			   inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

		https_server(conn);
		wpa_printf(MSG_DEBUG, "Done with the connection");
		wpa_printf(MSG_DEBUG, "-------------------------------------");
	}

	close(s);

	return 0;
}
