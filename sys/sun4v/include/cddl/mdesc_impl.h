/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 * $FreeBSD: src/sys/sun4v/include/cddl/mdesc_impl.h,v 1.1.6.1 2008/11/25 02:59:29 kensmith Exp $
 */

/*
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef	_MDESC_IMPL_H_
#define	_MDESC_IMPL_H_


#ifdef __cplusplus
extern "C" {
#endif

#define	LIBMD_MAGIC	0x4d61636844657363ULL	/* MachDesc */

#ifndef _ASM

	/*
	 * Internal definitions
	 */


/*
 * Each MD has the following header to
 * provide information about each section of the MD.
 *
 * There are 3 sections:
 * The description list, the name table and the data block.
 *
 * All values are stored in network byte order.
 *
 * Elements in the first (description list) section are defined by their
 * index location within the node block. An index is simply the byte offset
 * within the block / element size (16bytes). All elements are refered to
 * by their index, to avoid bugs related to alignment etc.
 *
 * The name_len field holds the storage length of an ASCII name, NOT the strlen.
 * The header fields are written in network
 * byte order.
 */

struct md_header_s {
	uint32_t	transport_version;
	uint32_t	node_blk_sz;	/* size in bytes of the node block */
	uint32_t	name_blk_sz;	/* size in bytes of the name block */
	uint32_t	data_blk_sz;	/* size in bytes of the data block */
};

typedef struct md_header_s md_header_t;



#if defined(_BIG_ENDIAN) && !defined(lint)
#define	mdtoh8(x)	((uint8_t)(x))
#define	mdtoh16(x)	((uint16_t)(x))
#define	mdtoh32(x)	((uint32_t)(x))
#define	mdtoh64(x)	((uint64_t)(x))
#define	htomd8(x)	(x)
#define	htomd16(x)	(x)
#define	htomd32(x)	(x)
#define	htomd64(x)	(x)
#else
#define	mdtoh8(x)	((uint8_t)(x))
extern	uint16_t	mdtoh16(uint16_t);
extern	uint32_t	mdtoh32(uint32_t);
extern	uint64_t	mdtoh64(uint64_t);
#define	htomd8(x)	((uint8_t)(x))
extern	uint16_t	htomd16(uint16_t);
extern	uint32_t	htomd32(uint32_t);
extern	uint64_t	htomd64(uint64_t);
#endif



struct MD_ELEMENT {
	uint8_t		tag;
	uint8_t		name_len;
	uint16_t	_reserved;
	uint32_t	name_offset;	/* mde_str_cookie_t */
	union {
		struct {
			uint32_t	len;
			uint32_t	offset;
		} prop_data;			/* for PROP_DATA and PROP_STR */
		uint64_t	prop_val;	/* for PROP_VAL */
		uint64_t	prop_idx;	/* for PROP_ARC and NODE */
	} d;
};

typedef struct MD_ELEMENT md_element_t;

struct MACHINE_DESCRIPTION {
	caddr_t		caddr;

	void		*(*allocp)(size_t);
	void		(*freep)(void *, size_t);

	md_header_t	*headerp;
	md_element_t	*mdep;
	char		*namep;
	uint8_t		*datap;

	int		node_blk_size;
	int		name_blk_size;
	int		data_blk_size;

	int		element_count;
	int		node_count;

	mde_cookie_t	root_node;

	int		size;
	uint64_t	gen;

	uint64_t	md_magic;
};

typedef struct MACHINE_DESCRIPTION md_impl_t;

#define	MDE_TAG(_p)			mdtoh8((_p)->tag)
#define	MDE_NAME(_p)			mdtoh32((_p)->name_offset)
#define	MDE_NAME_LEN(_p)		mdtoh32((_p)->name_len)
#define	MDE_PROP_DATA_OFFSET(_p)	mdtoh32((_p)->d.prop_data.offset)
#define	MDE_PROP_DATA_LEN(_p)		mdtoh32((_p)->d.prop_data.len)
#define	MDE_PROP_VALUE(_p)		mdtoh64((_p)->d.prop_val)
#define	MDE_PROP_INDEX(_p)		mdtoh64((_p)->d.prop_idx)

extern mde_str_cookie_t md_ident_name_str(char *);

extern mde_cookie_t	md_find_node_prop(md_impl_t *,
				mde_cookie_t,
				mde_str_cookie_t,
				int);
#endif	/* _ASM */

#ifdef __cplusplus
}
#endif

#endif	/* _MDESC_IMPL_H_ */
