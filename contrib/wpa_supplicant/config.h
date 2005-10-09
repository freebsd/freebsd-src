#ifndef CONFIG_H
#define CONFIG_H

#ifdef CONFIG_CTRL_IFACE
#ifndef CONFIG_CTRL_IFACE_UDP
#include <grp.h>
#endif /* CONFIG_CTRL_IFACE_UDP */
#endif /* CONFIG_CTRL_IFACE */

#include "config_ssid.h"

struct wpa_config {
	struct wpa_ssid *ssid; /* global network list */
	struct wpa_ssid **pssid; /* per priority network lists (in priority
				  * order) */
	int num_prio; /* number of different priorities */
	int eapol_version;
	int ap_scan;
	char *ctrl_interface; /* directory for UNIX domain sockets */
#ifdef CONFIG_CTRL_IFACE
#ifndef CONFIG_CTRL_IFACE_UDP
	gid_t ctrl_interface_gid;
#endif /* CONFIG_CTRL_IFACE_UDP */
	int ctrl_interface_gid_set;
#endif /* CONFIG_CTRL_IFACE */
	int fast_reauth;
};


struct wpa_config * wpa_config_read(const char *config_file);
void wpa_config_free(struct wpa_config *ssid);

#endif /* CONFIG_H */
