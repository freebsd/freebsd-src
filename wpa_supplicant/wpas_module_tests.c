/*
 * wpa_supplicant module tests
 * Copyright (c) 2014, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "utils/module_tests.h"
#include "wpa_supplicant_i.h"
#include "bssid_ignore.h"


static int wpas_bssid_ignore_module_tests(void)
{
	struct wpa_supplicant wpa_s;
	int ret = -1;

	os_memset(&wpa_s, 0, sizeof(wpa_s));

	wpa_bssid_ignore_clear(&wpa_s);

	if (wpa_bssid_ignore_get(NULL, NULL) != NULL ||
	    wpa_bssid_ignore_get(NULL, (u8 *) "123456") != NULL ||
	    wpa_bssid_ignore_get(&wpa_s, NULL) != NULL ||
	    wpa_bssid_ignore_get(&wpa_s, (u8 *) "123456") != NULL)
		goto fail;

	if (wpa_bssid_ignore_add(NULL, NULL) == 0 ||
	    wpa_bssid_ignore_add(NULL, (u8 *) "123456") == 0 ||
	    wpa_bssid_ignore_add(&wpa_s, NULL) == 0)
		goto fail;

	if (wpa_bssid_ignore_del(NULL, NULL) == 0 ||
	    wpa_bssid_ignore_del(NULL, (u8 *) "123456") == 0 ||
	    wpa_bssid_ignore_del(&wpa_s, NULL) == 0 ||
	    wpa_bssid_ignore_del(&wpa_s, (u8 *) "123456") == 0)
		goto fail;

	if (wpa_bssid_ignore_add(&wpa_s, (u8 *) "111111") < 0 ||
	    wpa_bssid_ignore_add(&wpa_s, (u8 *) "111111") < 0 ||
	    wpa_bssid_ignore_add(&wpa_s, (u8 *) "222222") < 0 ||
	    wpa_bssid_ignore_add(&wpa_s, (u8 *) "333333") < 0 ||
	    wpa_bssid_ignore_add(&wpa_s, (u8 *) "444444") < 0 ||
	    wpa_bssid_ignore_del(&wpa_s, (u8 *) "333333") < 0 ||
	    wpa_bssid_ignore_del(&wpa_s, (u8 *) "xxxxxx") == 0 ||
	    wpa_bssid_ignore_get(&wpa_s, (u8 *) "xxxxxx") != NULL ||
	    wpa_bssid_ignore_get(&wpa_s, (u8 *) "111111") == NULL ||
	    wpa_bssid_ignore_get(&wpa_s, (u8 *) "222222") == NULL ||
	    wpa_bssid_ignore_get(&wpa_s, (u8 *) "444444") == NULL ||
	    wpa_bssid_ignore_del(&wpa_s, (u8 *) "111111") < 0 ||
	    wpa_bssid_ignore_del(&wpa_s, (u8 *) "222222") < 0 ||
	    wpa_bssid_ignore_del(&wpa_s, (u8 *) "444444") < 0 ||
	    wpa_bssid_ignore_add(&wpa_s, (u8 *) "111111") < 0 ||
	    wpa_bssid_ignore_add(&wpa_s, (u8 *) "222222") < 0 ||
	    wpa_bssid_ignore_add(&wpa_s, (u8 *) "333333") < 0)
		goto fail;

	wpa_bssid_ignore_clear(&wpa_s);

	if (wpa_bssid_ignore_add(&wpa_s, (u8 *) "111111") < 0 ||
	    wpa_bssid_ignore_add(&wpa_s, (u8 *) "222222") < 0 ||
	    wpa_bssid_ignore_add(&wpa_s, (u8 *) "333333") < 0 ||
	    wpa_bssid_ignore_add(&wpa_s, (u8 *) "444444") < 0 ||
	    !wpa_bssid_ignore_is_listed(&wpa_s, (u8 *) "111111") ||
	    wpa_bssid_ignore_del(&wpa_s, (u8 *) "111111") < 0 ||
	    wpa_bssid_ignore_is_listed(&wpa_s, (u8 *) "111111") ||
	    wpa_bssid_ignore_add(&wpa_s, (u8 *) "111111") < 0)
		goto fail;

	wpa_bssid_ignore_update(&wpa_s);

	if (!wpa_bssid_ignore_is_listed(&wpa_s, (u8 *) "111111"))
		goto fail;

	ret = 0;
fail:
	wpa_bssid_ignore_clear(&wpa_s);

	if (ret)
		wpa_printf(MSG_ERROR, "bssid_ignore module test failure");

	return ret;
}


int wpas_module_tests(void)
{
	int ret = 0;

	wpa_printf(MSG_INFO, "wpa_supplicant module tests");

	if (wpas_bssid_ignore_module_tests() < 0)
		ret = -1;

#ifdef CONFIG_WPS
	if (wps_module_tests() < 0)
		ret = -1;
#endif /* CONFIG_WPS */

	if (utils_module_tests() < 0)
		ret = -1;

	if (common_module_tests() < 0)
		ret = -1;

	if (crypto_module_tests() < 0)
		ret = -1;

	return ret;
}
