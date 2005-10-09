#ifndef DRIVER_H
#define DRIVER_H

struct driver_ops {
	const char *name;		/* as appears in the config file */

	int (*init)(struct hostapd_data *hapd);
	void (*deinit)(void *priv);

	int (*wireless_event_init)(void *priv);
	void (*wireless_event_deinit)(void *priv);

	/**
	 * set_8021x - enable/disable 802.1x support
	 * @priv: driver private data
	 * @enabled: 1 = enable, 0 = disable
	 *
	 * Returns: 0 on success, -1 on failure
	 *
	 * Configure the kernel driver to enable/disable 802.1x support.
	 * This may be an empty function if 802.1x support is always enabled.
	 */
	int (*set_ieee8021x)(void *priv, int enabled);

	/**
	 * set_privacy - enable/disable privacy
	 * @priv: driver private data
	 * @enabled: 1 = privacy enabled, 0 = disabled
	 *
	 * Return: 0 on success, -1 on failure
	 *
	 * Configure privacy.
	 */
	int (*set_privacy)(void *priv, int enabled);

	int (*set_encryption)(void *priv, const char *alg, u8 *addr,
			      int idx, u8 *key, size_t key_len);
	int (*get_seqnum)(void *priv, u8 *addr, int idx, u8 *seq);
	int (*flush)(void *priv);
	int (*set_generic_elem)(void *priv, const u8 *elem, size_t elem_len);

	int (*read_sta_data)(void *priv, struct hostap_sta_driver_data *data,
			     u8 *addr);
	int (*send_eapol)(void *priv, u8 *addr, u8 *data, size_t data_len,
			  int encrypt);
	int (*set_sta_authorized)(void *driver, u8 *addr, int authorized);
	int (*sta_deauth)(void *priv, u8 *addr, int reason);
	int (*sta_disassoc)(void *priv, u8 *addr, int reason);
	int (*sta_remove)(void *priv, u8 *addr);
	int (*get_ssid)(void *priv, u8 *buf, int len);
	int (*set_ssid)(void *priv, u8 *buf, int len);
	int (*send_mgmt_frame)(void *priv, const void *msg, size_t len,
			       int flags);
	int (*set_assoc_ap)(void *priv, u8 *addr);
	int (*sta_add)(void *priv, u8 *addr, u16 aid, u16 capability,
		       u8 tx_supp_rates);
	int (*get_inact_sec)(void *priv, u8 *addr);
	int (*sta_clear_stats)(void *priv, u8 *addr);
};

static inline int
hostapd_driver_init(struct hostapd_data *hapd)
{
	if (hapd->driver == NULL || hapd->driver->init == NULL)
		return -1;
	return hapd->driver->init(hapd);
}

static inline void
hostapd_driver_deinit(struct hostapd_data *hapd)
{
	if (hapd->driver == NULL || hapd->driver->deinit == NULL)
		return;
	hapd->driver->deinit(hapd->driver);
}

static inline int
hostapd_wireless_event_init(struct hostapd_data *hapd)
{
	if (hapd->driver == NULL ||
	    hapd->driver->wireless_event_init == NULL)
		return 0;
	return hapd->driver->wireless_event_init(hapd->driver);
}

static inline void
hostapd_wireless_event_deinit(struct hostapd_data *hapd)
{
	if (hapd->driver == NULL ||
	    hapd->driver->wireless_event_deinit == NULL)
		return;
	hapd->driver->wireless_event_deinit(hapd->driver);
}

static inline int
hostapd_set_ieee8021x(struct hostapd_data *hapd, int enabled)
{
	if (hapd->driver == NULL || hapd->driver->set_ieee8021x == NULL)
		return 0;
	return hapd->driver->set_ieee8021x(hapd->driver, enabled);
}

static inline int
hostapd_set_privacy(struct hostapd_data *hapd, int enabled)
{
	if (hapd->driver == NULL || hapd->driver->set_privacy == NULL)
		return 0;
	return hapd->driver->set_privacy(hapd->driver, enabled);
}

static inline int
hostapd_set_encryption(struct hostapd_data *hapd, const char *alg, u8 *addr,
		       int idx, u8 *key, size_t key_len)
{
	if (hapd->driver == NULL || hapd->driver->set_encryption == NULL)
		return 0;
	return hapd->driver->set_encryption(hapd->driver, alg, addr, idx, key,
					    key_len);
}

static inline int
hostapd_get_seqnum(struct hostapd_data *hapd, u8 *addr, int idx, u8 *seq)
{
	if (hapd->driver == NULL || hapd->driver->get_seqnum == NULL)
		return 0;
	return hapd->driver->get_seqnum(hapd->driver, addr, idx, seq);
}

static inline int
hostapd_flush(struct hostapd_data *hapd)
{
	if (hapd->driver == NULL || hapd->driver->flush == NULL)
		return 0;
	return hapd->driver->flush(hapd->driver);
}

static inline int
hostapd_set_generic_elem(struct hostapd_data *hapd, const u8 *elem,
			 size_t elem_len)
{
	if (hapd->driver == NULL || hapd->driver->set_generic_elem == NULL)
		return 0;
	return hapd->driver->set_generic_elem(hapd->driver, elem, elem_len);
}

static inline int
hostapd_read_sta_data(struct hostapd_data *hapd,
		      struct hostap_sta_driver_data *data, u8 *addr)
{
	if (hapd->driver == NULL || hapd->driver->read_sta_data == NULL)
		return -1;
	return hapd->driver->read_sta_data(hapd->driver, data, addr);
}

static inline int
hostapd_send_eapol(struct hostapd_data *hapd, u8 *addr, u8 *data,
		   size_t data_len, int encrypt)
{
	if (hapd->driver == NULL || hapd->driver->send_eapol == NULL)
		return 0;
	return hapd->driver->send_eapol(hapd->driver, addr, data, data_len,
					encrypt);
}

static inline int
hostapd_set_sta_authorized(struct hostapd_data *hapd, u8 *addr, int authorized)
{
	if (hapd->driver == NULL || hapd->driver->set_sta_authorized == NULL)
		return 0;
	return hapd->driver->set_sta_authorized(hapd->driver, addr,
						authorized);
}

static inline int
hostapd_sta_deauth(struct hostapd_data *hapd, u8 *addr, int reason)
{
	if (hapd->driver == NULL || hapd->driver->sta_deauth == NULL)
		return 0;
	return hapd->driver->sta_deauth(hapd->driver, addr, reason);
}

static inline int
hostapd_sta_disassoc(struct hostapd_data *hapd, u8 *addr, int reason)
{
	if (hapd->driver == NULL || hapd->driver->sta_disassoc == NULL)
		return 0;
	return hapd->driver->sta_disassoc(hapd->driver, addr, reason);
}

static inline int
hostapd_sta_remove(struct hostapd_data *hapd, u8 *addr)
{
	if (hapd->driver == NULL || hapd->driver->sta_remove == NULL)
		return 0;
	return hapd->driver->sta_remove(hapd->driver, addr);
}

static inline int
hostapd_get_ssid(struct hostapd_data *hapd, u8 *buf, size_t len)
{
	if (hapd->driver == NULL || hapd->driver->get_ssid == NULL)
		return 0;
	return hapd->driver->get_ssid(hapd->driver, buf, len);
}

static inline int
hostapd_set_ssid(struct hostapd_data *hapd, u8 *buf, size_t len)
{
	if (hapd->driver == NULL || hapd->driver->set_ssid == NULL)
		return 0;
	return hapd->driver->set_ssid(hapd->driver, buf, len);
}

static inline int
hostapd_send_mgmt_frame(struct hostapd_data *hapd, const void *msg, size_t len,
			int flags)
{
	if (hapd->driver == NULL || hapd->driver->send_mgmt_frame == NULL)
		return 0;
	return hapd->driver->send_mgmt_frame(hapd->driver, msg, len, flags);
}

static inline int
hostapd_set_assoc_ap(struct hostapd_data *hapd, u8 *addr)
{
	if (hapd->driver == NULL || hapd->driver->set_assoc_ap == NULL)
		return 0;
	return hapd->driver->set_assoc_ap(hapd->driver, addr);
}

static inline int
hostapd_sta_add(struct hostapd_data *hapd, u8 *addr, u16 aid, u16 capability,
		u8 tx_supp_rates)
{
	if (hapd->driver == NULL || hapd->driver->sta_add == NULL)
		return 0;
	return hapd->driver->sta_add(hapd->driver, addr, aid, capability,
				     tx_supp_rates);
}

static inline int
hostapd_get_inact_sec(struct hostapd_data *hapd, u8 *addr)
{
	if (hapd->driver == NULL || hapd->driver->get_inact_sec == NULL)
		return 0;
	return hapd->driver->get_inact_sec(hapd->driver, addr);
}


void driver_register(const char *name, const struct driver_ops *ops);
void driver_unregister(const char *name);
const struct driver_ops *driver_lookup(const char *name);

static inline int
hostapd_sta_clear_stats(struct hostapd_data *hapd, u8 *addr)
{
	if (hapd->driver == NULL || hapd->driver->sta_clear_stats == NULL)
		return 0;
	return hapd->driver->sta_clear_stats(hapd->driver, addr);
}

#endif /* DRIVER_H */
