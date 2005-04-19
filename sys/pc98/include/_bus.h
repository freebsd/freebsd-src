/*-
 * Copyright (c) 2005 M. Warner Losh.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef PC98_INCLUDE__BUS_H
#define PC98_INCLUDE__BUS_H

/*
 * Bus address and size types
 */
typedef u_int bus_addr_t;
typedef u_int bus_size_t;

#define BUS_SPACE_IAT_MAXSIZE	33

/*
 * bus space tag
 */
#define	_PASCAL_CALL	(void)

#define	_BUS_SPACE_CALL_FUNCS_TAB(NAME,TYPE,BWN) \
	NAME##_space_read_##BWN, 				\
	NAME##_space_read_multi_##BWN, 				\
	NAME##_space_read_region_##BWN,				\
	NAME##_space_write_##BWN, 				\
	NAME##_space_write_multi_##BWN, 			\
	NAME##_space_write_region_##BWN,			\
	NAME##_space_set_multi_##BWN,				\
	NAME##_space_set_region_##BWN,				\
	NAME##_space_copy_region_##BWN

#define	_BUS_SPACE_CALL_FUNCS_PROTO(NAME,TYPE,BWN) \
	TYPE NAME##_space_read_##BWN _PASCAL_CALL;		\
	void NAME##_space_read_multi_##BWN _PASCAL_CALL;	\
	void NAME##_space_read_region_##BWN _PASCAL_CALL;	\
	void NAME##_space_write_##BWN _PASCAL_CALL;		\
	void NAME##_space_write_multi_##BWN _PASCAL_CALL;	\
	void NAME##_space_write_region_##BWN _PASCAL_CALL;	\
	void NAME##_space_set_multi_##BWN _PASCAL_CALL;		\
	void NAME##_space_set_region_##BWN _PASCAL_CALL;	\
	void NAME##_space_copy_region_##BWN _PASCAL_CALL;

#define	_BUS_SPACE_CALL_FUNCS(NAME,TYPE,BWN) \
	TYPE (* NAME##_read_##BWN) _PASCAL_CALL;		\
	void (* NAME##_read_multi_##BWN) _PASCAL_CALL;		\
	void (* NAME##_read_region_##BWN) _PASCAL_CALL;		\
	void (* NAME##_write_##BWN) _PASCAL_CALL;		\
	void (* NAME##_write_multi_##BWN) _PASCAL_CALL;		\
	void (* NAME##_write_region_##BWN) _PASCAL_CALL;	\
	void (* NAME##_set_multi_##BWN) _PASCAL_CALL;		\
	void (* NAME##_set_region_##BWN) _PASCAL_CALL;		\
	void (* NAME##_copy_region_##BWN) _PASCAL_CALL;	

struct bus_space_access_methods {
	/* 8 bits access methods */
	_BUS_SPACE_CALL_FUNCS(bs,u_int8_t,1)

	/* 16 bits access methods */
	_BUS_SPACE_CALL_FUNCS(bs,u_int16_t,2)

	/* 32 bits access methods */
	_BUS_SPACE_CALL_FUNCS(bs,u_int32_t,4)
};

/*
 * Access methods for bus resources and address space.
 */
struct bus_space_tag {
#define	BUS_SPACE_IO	0
#define	BUS_SPACE_MEM	1
	u_int	bs_tag;			/* bus space flags */

	struct bus_space_access_methods bs_da;	/* direct access */
	struct bus_space_access_methods bs_ra;	/* relocate access */
#if	0
	struct bus_space_access_methods bs_ida;	/* indexed direct access */
#endif
};
typedef struct bus_space_tag *bus_space_tag_t;

/*
 * bus space handle
 */
struct bus_space_handle {
	bus_addr_t	bsh_base;
	size_t		bsh_sz;

	bus_addr_t	bsh_iat[BUS_SPACE_IAT_MAXSIZE];
	size_t		bsh_maxiatsz;
	size_t		bsh_iatsz;

	struct resource	**bsh_res;
	size_t		bsh_ressz;

	struct bus_space_access_methods bsh_bam;
};
typedef struct bus_space_handle *bus_space_handle_t;

#endif /* PC98_INCLUDE__BUS_H */
