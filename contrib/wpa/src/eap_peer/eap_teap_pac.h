/*
 * EAP peer method: EAP-TEAP PAC file processing
 * Copyright (c) 2004-2019, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef EAP_TEAP_PAC_H
#define EAP_TEAP_PAC_H

#include "eap_common/eap_teap_common.h"

struct eap_teap_pac {
	struct eap_teap_pac *next;

	u8 pac_key[EAP_TEAP_PAC_KEY_LEN];
	u8 *pac_opaque;
	size_t pac_opaque_len;
	u8 *pac_info;
	size_t pac_info_len;
	u8 *a_id;
	size_t a_id_len;
	u8 *i_id;
	size_t i_id_len;
	u8 *a_id_info;
	size_t a_id_info_len;
	u16 pac_type;
};


void eap_teap_free_pac(struct eap_teap_pac *pac);
struct eap_teap_pac * eap_teap_get_pac(struct eap_teap_pac *pac_root,
				       const u8 *a_id, size_t a_id_len,
				       u16 pac_type);
int eap_teap_add_pac(struct eap_teap_pac **pac_root,
		     struct eap_teap_pac **pac_current,
		     struct eap_teap_pac *entry);
int eap_teap_load_pac(struct eap_sm *sm, struct eap_teap_pac **pac_root,
		      const char *pac_file);
int eap_teap_save_pac(struct eap_sm *sm, struct eap_teap_pac *pac_root,
		      const char *pac_file);
size_t eap_teap_pac_list_truncate(struct eap_teap_pac *pac_root,
				  size_t max_len);
int eap_teap_load_pac_bin(struct eap_sm *sm, struct eap_teap_pac **pac_root,
			  const char *pac_file);
int eap_teap_save_pac_bin(struct eap_sm *sm, struct eap_teap_pac *pac_root,
			  const char *pac_file);

#endif /* EAP_TEAP_PAC_H */
