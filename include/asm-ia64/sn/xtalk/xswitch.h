/* $Id$
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992-1997,2000-2003 Silicon Graphics, Inc.  All Rights Reserved.
 */
#ifndef _ASM_SN_XTALK_XSWITCH_H
#define _ASM_SN_XTALK_XSWITCH_H

/*
 * xswitch.h - controls the format of the data
 * provided by xswitch verticies back to the
 * xtalk bus providers.
 */

#ifndef __ASSEMBLY__

#include <linux/devfs_fs_kernel.h>
#include <asm/sn/xtalk/xtalk.h>

typedef struct xswitch_info_s *xswitch_info_t;

typedef int
                        xswitch_reset_link_f(vertex_hdl_t xconn);

typedef struct xswitch_provider_s {
    xswitch_reset_link_f   *reset_link;
} xswitch_provider_t;

extern void             xswitch_provider_register(vertex_hdl_t sw_vhdl, xswitch_provider_t * xsw_fns);

xswitch_reset_link_f    xswitch_reset_link;

extern xswitch_info_t   xswitch_info_new(vertex_hdl_t vhdl);

extern void             xswitch_info_link_is_ok(xswitch_info_t xswitch_info,
						xwidgetnum_t port);
extern void             xswitch_info_vhdl_set(xswitch_info_t xswitch_info,
					      xwidgetnum_t port,
					      vertex_hdl_t xwidget);
extern void             xswitch_info_master_assignment_set(xswitch_info_t xswitch_info,
						       xwidgetnum_t port,
					       vertex_hdl_t master_vhdl);

extern xswitch_info_t   xswitch_info_get(vertex_hdl_t vhdl);

extern int              xswitch_info_link_ok(xswitch_info_t xswitch_info,
					     xwidgetnum_t port);
extern vertex_hdl_t     xswitch_info_vhdl_get(xswitch_info_t xswitch_info,
					      xwidgetnum_t port);
extern vertex_hdl_t     xswitch_info_master_assignment_get(xswitch_info_t xswitch_info,
						      xwidgetnum_t port);

#endif				/* __ASSEMBLY__ */

#endif				/* _ASM_SN_XTALK_XSWITCH_H */
