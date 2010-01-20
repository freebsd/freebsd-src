/*
 * hostapd / RADIUS client
 * Copyright (c) 2002-2005, Jouni Malinen <j@w1.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

#ifndef RADIUS_CLIENT_H
#define RADIUS_CLIENT_H

#include "ip_addr.h"

struct radius_msg;

struct hostapd_radius_server {
	/* MIB prefix for shared variables:
	 * @ = radiusAuth or radiusAcc depending on the type of the server */
	struct hostapd_ip_addr addr; /* @ServerAddress */
	int port; /* @ClientServerPortNumber */
	u8 *shared_secret;
	size_t shared_secret_len;

	/* Dynamic (not from configuration file) MIB data */
	int index; /* @ServerIndex */
	int round_trip_time; /* @ClientRoundTripTime; in hundredths of a
			      * second */
	u32 requests; /* @Client{Access,}Requests */
	u32 retransmissions; /* @Client{Access,}Retransmissions */
	u32 access_accepts; /* radiusAuthClientAccessAccepts */
	u32 access_rejects; /* radiusAuthClientAccessRejects */
	u32 access_challenges; /* radiusAuthClientAccessChallenges */
	u32 responses; /* radiusAccClientResponses */
	u32 malformed_responses; /* @ClientMalformed{Access,}Responses */
	u32 bad_authenticators; /* @ClientBadAuthenticators */
	u32 timeouts; /* @ClientTimeouts */
	u32 unknown_types; /* @ClientUnknownTypes */
	u32 packets_dropped; /* @ClientPacketsDropped */
	/* @ClientPendingRequests: length of hapd->radius->msgs for matching
	 * msg_type */
};

struct hostapd_radius_servers {
	/* RADIUS Authentication and Accounting servers in priority order */
	struct hostapd_radius_server *auth_servers, *auth_server;
	int num_auth_servers;
	struct hostapd_radius_server *acct_servers, *acct_server;
	int num_acct_servers;

	int retry_primary_interval;
	int acct_interim_interval;

	int msg_dumps;

	struct hostapd_ip_addr client_addr;
	int force_client_addr;
};


typedef enum {
	RADIUS_AUTH,
	RADIUS_ACCT,
	RADIUS_ACCT_INTERIM /* used only with radius_client_send(); just like
			     * RADIUS_ACCT, but removes any pending interim
			     * RADIUS Accounting packages for the same STA
			     * before sending the new interim update */
} RadiusType;

typedef enum {
	RADIUS_RX_PROCESSED,
	RADIUS_RX_QUEUED,
	RADIUS_RX_UNKNOWN,
	RADIUS_RX_INVALID_AUTHENTICATOR
} RadiusRxResult;

struct radius_client_data;

int radius_client_register(struct radius_client_data *radius,
			   RadiusType msg_type,
			   RadiusRxResult (*handler)
			   (struct radius_msg *msg, struct radius_msg *req,
			    const u8 *shared_secret, size_t shared_secret_len,
			    void *data),
			   void *data);
int radius_client_send(struct radius_client_data *radius,
		       struct radius_msg *msg,
		       RadiusType msg_type, const u8 *addr);
u8 radius_client_get_id(struct radius_client_data *radius);

void radius_client_flush(struct radius_client_data *radius, int only_auth);
struct radius_client_data *
radius_client_init(void *ctx, struct hostapd_radius_servers *conf);
void radius_client_deinit(struct radius_client_data *radius);
void radius_client_flush_auth(struct radius_client_data *radius, u8 *addr);
int radius_client_get_mib(struct radius_client_data *radius, char *buf,
			  size_t buflen);
struct radius_client_data *
radius_client_reconfig(struct radius_client_data *old, void *ctx,
		       struct hostapd_radius_servers *oldconf,
		       struct hostapd_radius_servers *newconf);

#endif /* RADIUS_CLIENT_H */
