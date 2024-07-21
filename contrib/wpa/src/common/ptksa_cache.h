/*
 * RSN PTKSA cache interface
 *
 * Copyright (C) 2019 Intel Corporation
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef PTKSA_CACHE_H
#define PTKSA_CACHE_H

#include "wpa_common.h"
#include "defs.h"
#include "list.h"

/**
 * struct ptksa_cache_entry - PTKSA cache entry
 */
struct ptksa_cache_entry {
	struct dl_list list;
	struct wpa_ptk ptk;
	os_time_t expiration;
	u32 cipher;
	u8 addr[ETH_ALEN];
	u8 own_addr[ETH_ALEN];
	void (*cb)(struct ptksa_cache_entry *e);
	void *ctx;
	u32 akmp;
};


struct ptksa_cache;

struct ptksa_cache * ptksa_cache_init(void);
void ptksa_cache_deinit(struct ptksa_cache *ptksa);
struct ptksa_cache_entry * ptksa_cache_get(struct ptksa_cache *ptksa,
					   const u8 *addr, u32 cipher);
int ptksa_cache_list(struct ptksa_cache *ptksa, char *buf, size_t len);
struct ptksa_cache_entry * ptksa_cache_add(struct ptksa_cache *ptksa,
					   const u8 *own_addr,
					   const u8 *addr, u32 cipher,
					   u32 life_time,
					   const struct wpa_ptk *ptk,
					   void (*cb)
					   (struct ptksa_cache_entry *e),
					   void *ctx, u32 akmp);
void ptksa_cache_flush(struct ptksa_cache *ptksa, const u8 *addr, u32 cipher);

#endif /* PTKSA_CACHE_H */
