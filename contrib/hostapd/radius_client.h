#ifndef RADIUS_CLIENT_H
#define RADIUS_CLIENT_H

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
			    u8 *shared_secret, size_t shared_secret_len,
			    void *data),
			   void *data);
int radius_client_send(struct radius_client_data *radius,
		       struct radius_msg *msg,
		       RadiusType msg_type, u8 *addr);
u8 radius_client_get_id(struct radius_client_data *radius);

void radius_client_flush(struct radius_client_data *radius);
struct radius_client_data * radius_client_init(struct hostapd_data *hapd);
void radius_client_deinit(struct radius_client_data *radius);
void radius_client_flush_auth(struct radius_client_data *radius, u8 *addr);
int radius_client_get_mib(struct radius_client_data *radius, char *buf,
			  size_t buflen);

#endif /* RADIUS_CLIENT_H */
