/*
 * Example application using RADIUS client as a library
 * Copyright (c) 2007, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#include "common.h"
#include "eloop.h"
#include "radius/radius.h"
#include "radius/radius_client.h"

struct radius_ctx {
	struct radius_client_data *radius;
	struct hostapd_radius_servers conf;
	u8 radius_identifier;
	struct in_addr own_ip_addr;
};


static void hostapd_logger_cb(void *ctx, const u8 *addr, unsigned int module,
			      int level, const char *txt, size_t len)
{
	printf("%s\n", txt);
}


/* Process the RADIUS frames from Authentication Server */
static RadiusRxResult receive_auth(struct radius_msg *msg,
				   struct radius_msg *req,
				   const u8 *shared_secret,
				   size_t shared_secret_len,
				   void *data)
{
	/* struct radius_ctx *ctx = data; */
	printf("Received RADIUS Authentication message; code=%d\n",
	       radius_msg_get_hdr(msg)->code);

	/* We're done for this example, so request eloop to terminate. */
	eloop_terminate();

	return RADIUS_RX_PROCESSED;
}


static void start_example(void *eloop_ctx, void *timeout_ctx)
{
	struct radius_ctx *ctx = eloop_ctx;
	struct radius_msg *msg;

	printf("Sending a RADIUS authentication message\n");

	ctx->radius_identifier = radius_client_get_id(ctx->radius);
	msg = radius_msg_new(RADIUS_CODE_ACCESS_REQUEST,
			     ctx->radius_identifier);
	if (msg == NULL) {
		printf("Could not create net RADIUS packet\n");
		return;
	}

	radius_msg_make_authenticator(msg);

	if (!radius_msg_add_attr(msg, RADIUS_ATTR_USER_NAME,
				 (u8 *) "user", 4)) {
		printf("Could not add User-Name\n");
		radius_msg_free(msg);
		return;
	}

	if (!radius_msg_add_attr_user_password(
		    msg, (u8 *) "password", 8,
		    ctx->conf.auth_server->shared_secret,
		    ctx->conf.auth_server->shared_secret_len)) {
		printf("Could not add User-Password\n");
		radius_msg_free(msg);
		return;
	}

	if (!radius_msg_add_attr(msg, RADIUS_ATTR_NAS_IP_ADDRESS,
				 (u8 *) &ctx->own_ip_addr, 4)) {
		printf("Could not add NAS-IP-Address\n");
		radius_msg_free(msg);
		return;
	}

	if (radius_client_send(ctx->radius, msg, RADIUS_AUTH, NULL) < 0)
		radius_msg_free(msg);
}


int main(int argc, char *argv[])
{
	struct radius_ctx ctx;
	struct hostapd_radius_server *srv;

	if (os_program_init())
		return -1;

	hostapd_logger_register_cb(hostapd_logger_cb);

	os_memset(&ctx, 0, sizeof(ctx));
	inet_aton("127.0.0.1", &ctx.own_ip_addr);

	if (eloop_init()) {
		printf("Failed to initialize event loop\n");
		return -1;
	}

	srv = os_zalloc(sizeof(*srv));
	if (srv == NULL)
		return -1;

	srv->addr.af = AF_INET;
	srv->port = 1812;
	if (hostapd_parse_ip_addr("127.0.0.1", &srv->addr) < 0) {
		printf("Failed to parse IP address\n");
		return -1;
	}
	srv->shared_secret = (u8 *) os_strdup("radius");
	srv->shared_secret_len = 6;

	ctx.conf.auth_server = ctx.conf.auth_servers = srv;
	ctx.conf.num_auth_servers = 1;
	ctx.conf.msg_dumps = 1;

	ctx.radius = radius_client_init(&ctx, &ctx.conf);
	if (ctx.radius == NULL) {
		printf("Failed to initialize RADIUS client\n");
		return -1;
	}

	if (radius_client_register(ctx.radius, RADIUS_AUTH, receive_auth,
				   &ctx) < 0) {
		printf("Failed to register RADIUS authentication handler\n");
		return -1;
	}

	eloop_register_timeout(0, 0, start_example, &ctx, NULL);

	eloop_run();

	radius_client_deinit(ctx.radius);
	os_free(srv->shared_secret);
	os_free(srv);

	eloop_destroy();
	os_program_deinit();

	return 0;
}
