#ifndef CTRL_IFACE_H
#define CTRL_IFACE_H

#ifdef CONFIG_CTRL_IFACE

int wpa_supplicant_ctrl_iface_init(struct wpa_supplicant *wpa_s);
void wpa_supplicant_ctrl_iface_deinit(struct wpa_supplicant *wpa_s);
void wpa_supplicant_ctrl_iface_send(struct wpa_supplicant *wpa_s, int level,
				    char *buf, size_t len);

#else /* CONFIG_CTRL_IFACE */

static inline int wpa_supplicant_ctrl_iface_init(struct wpa_supplicant *wpa_s)
{
	return 0;
}

static inline void
wpa_supplicant_ctrl_iface_deinit(struct wpa_supplicant *wpa_s)
{
}

static inline void
wpa_supplicant_ctrl_iface_send(struct wpa_supplicant *wpa_s, int level,
			       char *buf, size_t len)
{
}

#endif /* CONFIG_CTRL_IFACE */

#endif /* CTRL_IFACE_H */
