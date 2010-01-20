#ifndef CTRL_IFACE_H
#define CTRL_IFACE_H

int hostapd_ctrl_iface_init(struct hostapd_data *hapd);
void hostapd_ctrl_iface_deinit(struct hostapd_data *hapd);
void hostapd_ctrl_iface_send(struct hostapd_data *hapd, int level,
			     char *buf, size_t len);

#endif /* CTRL_IFACE_H */
