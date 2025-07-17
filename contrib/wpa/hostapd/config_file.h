/*
 * hostapd / Configuration file parser
 * Copyright (c) 2003-2009, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef CONFIG_FILE_H
#define CONFIG_FILE_H

struct hostapd_config * hostapd_config_read(const char *fname);
int hostapd_config_read_rxkh_file(struct hostapd_bss_config *conf,
				  const char *fname);
int hostapd_set_iface(struct hostapd_config *conf,
		      struct hostapd_bss_config *bss, const char *field,
		      char *value);

#endif /* CONFIG_FILE_H */
