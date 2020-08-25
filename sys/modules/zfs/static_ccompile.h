/*
 * $FreeBSD$
 */

#ifndef	_SPL_NVLIST_H_
#define	_SPL_NVLIST_H_

#ifdef INVARIANTS
#define        ZFS_DEBUG
#endif

#define	nvlist_add_nvlist	spl_nvlist_add_nvlist
#define	nvlist_add_nvlist_array	spl_nvlist_add_nvlist_array
#define	nvlist_add_nvpair	spl_nvlist_add_nvpair
#define	nvlist_add_string	spl_nvlist_add_string
#define	nvlist_add_string_array	spl_nvlist_add_string_array
#define	nvlist_empty	spl_nvlist_empty
#define	nvlist_exists	spl_nvlist_exists
#define	nvlist_free	spl_nvlist_free
#define	nvlist_next_nvpair	spl_nvlist_next_nvpair
#define	nvlist_pack	spl_nvlist_pack
#define	nvlist_prev_nvpair	spl_nvlist_prev_nvpair
#define	nvlist_remove_nvpair	spl_nvlist_remove_nvpair
#define	nvlist_size	spl_nvlist_size
#define	nvlist_unpack	spl_nvlist_unpack
	
#define	nvpair_type	spl_nvpair_type
#define	nvpair_name	spl_nvpair_name
#endif
