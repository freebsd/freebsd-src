/* $Id$
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992-1997,2000-2003 Silicon Graphics, Inc.  All Rights Reserved.
 */
#ifndef _ASM_SN_XTALK_XBOW_INFO_H
#define _ASM_SN_XTALK_XBOW_INFO_H

#include <linux/types.h>
#include <linux/devfs_fs_kernel.h>

#define XBOW_PERF_MODES	       0x03
#define XBOW_PERF_COUNTERS     0x02

#define XBOW_MONITOR_NONE      0x00
#define XBOW_MONITOR_SRC_LINK  0x01
#define XBOW_MONITOR_DEST_LINK 0x02
#define XBOW_MONITOR_INP_PKT   0x03
#define XBOW_MONITOR_MULTIPLEX 0x04

#define XBOW_LINK_MULTIPLEX    0x20

#define XBOW_PERF_TIMEOUT	4
#define XBOW_STATS_TIMEOUT	HZ

typedef struct xbow_perf_link {
    uint64_t              xlp_cumulative[XBOW_PERF_MODES];
    unsigned char           xlp_link_alive;
} xbow_perf_link_t;


typedef struct xbow_link_status {
    uint64_t              rx_err_count;
    uint64_t              tx_retry_count;
} xbow_link_status_t;



typedef struct xbow_perf {
    uint32_t              xp_current;
    unsigned char           xp_link;
    unsigned char           xp_mode;
    unsigned char           xp_curlink;
    unsigned char           xp_curmode;
    volatile uint32_t    *xp_perf_reg;
} xbow_perf_t;

extern void             xbow_update_perf_counters(vertex_hdl_t);
extern xbow_perf_link_t *xbow_get_perf_counters(vertex_hdl_t);
extern int              xbow_enable_perf_counter(vertex_hdl_t, int, int, int);

#define XBOWIOC_PERF_ENABLE	  	1
#define XBOWIOC_PERF_DISABLE	 	2
#define XBOWIOC_PERF_GET	 	3
#define XBOWIOC_LLP_ERROR_ENABLE 	4
#define XBOWIOC_LLP_ERROR_DISABLE	5
#define XBOWIOC_LLP_ERROR_GET	 	6


struct xbow_perfarg_t {
    int                     link;
    int                     mode;
    int                     counter;
};

#endif				/* _ASM_SN_XTALK_XBOW_INFO_H */
