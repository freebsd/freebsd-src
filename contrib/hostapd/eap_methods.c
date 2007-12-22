/*
 * hostapd / EAP method registration
 * Copyright (c) 2004-2006, Jouni Malinen <j@w1.fi>
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

#include "includes.h"

#include "hostapd.h"
#include "eap_i.h"
#include "eap_methods.h"


static struct eap_method *eap_methods;


/**
 * eap_sm_get_eap_methods - Get EAP method based on type number
 * @vendor: EAP Vendor-Id (0 = IETF)
 * @method: EAP type number
 * Returns: Pointer to EAP method or %NULL if not found
 */
const struct eap_method * eap_sm_get_eap_methods(int vendor, EapType method)
{
	struct eap_method *m;
	for (m = eap_methods; m; m = m->next) {
		if (m->vendor == vendor && m->method == method)
			return m;
	}
	return NULL;
}


/**
 * eap_get_type - Get EAP type for the given EAP method name
 * @name: EAP method name, e.g., TLS
 * @vendor: Buffer for returning EAP Vendor-Id
 * Returns: EAP method type or %EAP_TYPE_NONE if not found
 *
 * This function maps EAP type names into EAP type numbers based on the list of
 * EAP methods included in the build.
 */
EapType eap_get_type(const char *name, int *vendor)
{
	struct eap_method *m;
	for (m = eap_methods; m; m = m->next) {
		if (strcmp(m->name, name) == 0) {
			*vendor = m->vendor;
			return m->method;
		}
	}
	*vendor = EAP_VENDOR_IETF;
	return EAP_TYPE_NONE;
}


/**
 * eap_server_method_alloc - Allocate EAP server method structure
 * @version: Version of the EAP server method interface (set to
 * EAP_SERVER_METHOD_INTERFACE_VERSION)
 * @vendor: EAP Vendor-ID (EAP_VENDOR_*) (0 = IETF)
 * @method: EAP type number (EAP_TYPE_*)
 * name: Name of the method (e.g., "TLS")
 * Returns: Allocated EAP method structure or %NULL on failure
 *
 * The returned structure should be freed with eap_server_method_free() when it
 * is not needed anymore.
 */
struct eap_method * eap_server_method_alloc(int version, int vendor,
					    EapType method, const char *name)
{
	struct eap_method *eap;
	eap = wpa_zalloc(sizeof(*eap));
	if (eap == NULL)
		return NULL;
	eap->version = version;
	eap->vendor = vendor;
	eap->method = method;
	eap->name = name;
	return eap;
}


/**
 * eap_server_method_free - Free EAP server method structure
 * @method: Method structure allocated with eap_server_method_alloc()
 */
void eap_server_method_free(struct eap_method *method)
{
	free(method);
}


/**
 * eap_server_method_register - Register an EAP server method
 * @method: EAP method to register
 * Returns: 0 on success, -1 on invalid method, or -2 if a matching EAP method
 * has already been registered
 *
 * Each EAP server method needs to call this function to register itself as a
 * supported EAP method.
 */
int eap_server_method_register(struct eap_method *method)
{
	struct eap_method *m, *last = NULL;

	if (method == NULL || method->name == NULL ||
	    method->version != EAP_SERVER_METHOD_INTERFACE_VERSION)
		return -1;

	for (m = eap_methods; m; m = m->next) {
		if ((m->vendor == method->vendor &&
		     m->method == method->method) ||
		    strcmp(m->name, method->name) == 0)
			return -2;
		last = m;
	}

	if (last)
		last->next = method;
	else
		eap_methods = method;

	return 0;
}


/**
 * eap_server_register_methods - Register statically linked EAP server methods
 * Returns: 0 on success, -1 on failure
 *
 * This function is called at program initialization to register all EAP server
 * methods that were linked in statically.
 */
int eap_server_register_methods(void)
{
	int ret = 0;

	if (ret == 0) {
		int eap_server_identity_register(void);
		ret = eap_server_identity_register();
	}

#ifdef EAP_MD5
	if (ret == 0) {
		int eap_server_md5_register(void);
		ret = eap_server_md5_register();
	}
#endif /* EAP_MD5 */

#ifdef EAP_TLS
	if (ret == 0) {
		int eap_server_tls_register(void);
		ret = eap_server_tls_register();
	}
#endif /* EAP_TLS */

#ifdef EAP_MSCHAPv2
	if (ret == 0) {
		int eap_server_mschapv2_register(void);
		ret = eap_server_mschapv2_register();
	}
#endif /* EAP_MSCHAPv2 */

#ifdef EAP_PEAP
	if (ret == 0) {
		int eap_server_peap_register(void);
		ret = eap_server_peap_register();
	}
#endif /* EAP_PEAP */

#ifdef EAP_TLV
	if (ret == 0) {
		int eap_server_tlv_register(void);
		ret = eap_server_tlv_register();
	}
#endif /* EAP_TLV */

#ifdef EAP_GTC
	if (ret == 0) {
		int eap_server_gtc_register(void);
		ret = eap_server_gtc_register();
	}
#endif /* EAP_GTC */

#ifdef EAP_TTLS
	if (ret == 0) {
		int eap_server_ttls_register(void);
		ret = eap_server_ttls_register();
	}
#endif /* EAP_TTLS */

#ifdef EAP_SIM
	if (ret == 0) {
		int eap_server_sim_register(void);
		ret = eap_server_sim_register();
	}
#endif /* EAP_SIM */

#ifdef EAP_AKA
	if (ret == 0) {
		int eap_server_aka_register(void);
		ret = eap_server_aka_register();
	}
#endif /* EAP_AKA */

#ifdef EAP_PAX
	if (ret == 0) {
		int eap_server_pax_register(void);
		ret = eap_server_pax_register();
	}
#endif /* EAP_PAX */

#ifdef EAP_PSK
	if (ret == 0) {
		int eap_server_psk_register(void);
		ret = eap_server_psk_register();
	}
#endif /* EAP_PSK */

#ifdef EAP_SAKE
	if (ret == 0) {
		int eap_server_sake_register(void);
		ret = eap_server_sake_register();
	}
#endif /* EAP_SAKE */

#ifdef EAP_GPSK
	if (ret == 0) {
		int eap_server_gpsk_register(void);
		ret = eap_server_gpsk_register();
	}
#endif /* EAP_GPSK */

#ifdef EAP_VENDOR_TEST
	if (ret == 0) {
		int eap_server_vendor_test_register(void);
		ret = eap_server_vendor_test_register();
	}
#endif /* EAP_VENDOR_TEST */

	return ret;
}


/**
 * eap_server_unregister_methods - Unregister EAP server methods
 *
 * This function is called at program termination to unregister all EAP server
 * methods.
 */
void eap_server_unregister_methods(void)
{
	struct eap_method *m;

	while (eap_methods) {
		m = eap_methods;
		eap_methods = eap_methods->next;

		if (m->free)
			m->free(m);
		else
			eap_server_method_free(m);
	}
}
