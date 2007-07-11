#ifndef HOSTAPD_H
#define HOSTAPD_H

/*
 * Minimal version of hostapd header files for eapol_test to build
 * radius_client.c.
 */

#include "common.h"

void hostapd_logger(void *ctx, const u8 *addr, unsigned int module, int level,
		    char *fmt, ...) PRINTF_FORMAT(5, 6);

struct hostapd_ip_addr;

const char * hostapd_ip_txt(const struct hostapd_ip_addr *addr, char *buf,
			    size_t buflen);
int hostapd_ip_diff(struct hostapd_ip_addr *a, struct hostapd_ip_addr *b);

enum {
	HOSTAPD_LEVEL_DEBUG_VERBOSE = 0,
	HOSTAPD_LEVEL_DEBUG = 1,
	HOSTAPD_LEVEL_INFO = 2,
	HOSTAPD_LEVEL_NOTICE = 3,
	HOSTAPD_LEVEL_WARNING = 4
};

#ifndef BIT
#define BIT(n) (1 << (n))
#endif

#define HOSTAPD_MODULE_IEEE80211 BIT(0)
#define HOSTAPD_MODULE_IEEE8021X BIT(1)
#define HOSTAPD_MODULE_RADIUS BIT(2)
#define HOSTAPD_MODULE_WPA BIT(3)
#define HOSTAPD_MODULE_DRIVER BIT(4)
#define HOSTAPD_MODULE_IAPP BIT(5)

#endif /* HOSTAPD_H */
