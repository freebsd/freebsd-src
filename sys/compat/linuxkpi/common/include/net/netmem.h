/*-
 * Copyright (c) 2023-2025 Bjoern A. Zeeb
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef	_LINUXKPI_NET_NETMEM_H
#define	_LINUXKPI_NET_NETMEM_H

struct page_pool;

struct netmem_desc {
	struct page_pool	*pp;
};

#define	pp_page_to_nmdesc(page)						\
    (_Generic((page),							\
	const struct page *:	(const struct netmem_desc *)(page),	\
	struct page *:		(struct netmem_desc *)(page)))

#endif	/* _LINUXKPI_NET_NETMEM_H */
