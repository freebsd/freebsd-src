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

#ifndef EAP_METHODS_H
#define EAP_METHODS_H

const struct eap_method * eap_sm_get_eap_methods(int vendor, EapType method);
struct eap_method * eap_server_method_alloc(int version, int vendor,
					    EapType method, const char *name);
void eap_server_method_free(struct eap_method *method);
int eap_server_method_register(struct eap_method *method);

#ifdef EAP_SERVER

EapType eap_get_type(const char *name, int *vendor);
int eap_server_register_methods(void);
void eap_server_unregister_methods(void);

#else /* EAP_SERVER */

static inline EapType eap_get_type(const char *name, int *vendor)
{
	*vendor = EAP_VENDOR_IETF;
	return EAP_TYPE_NONE;
}

static inline int eap_server_register_methods(void)
{
	return 0;
}

static inline void eap_server_unregister_methods(void)
{
}

#endif /* EAP_SERVER */

#endif /* EAP_METHODS_H */
