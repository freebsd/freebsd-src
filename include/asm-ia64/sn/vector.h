/* $Id$
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992 - 1997, 2000-2003 Silicon Graphics, Inc. All rights reserved.
 */
#ifndef _ASM_IA64_SN_VECTOR_H
#define _ASM_IA64_SN_VECTOR_H

#include <linux/config.h>

#define NET_VEC_NULL            ((net_vec_t)  0)
#define NET_VEC_BAD             ((net_vec_t) -1)

#define VEC_POLLS_W		128	/* Polls before write times out */
#define VEC_POLLS_R		128	/* Polls before read times out */
#define VEC_POLLS_X		128	/* Polls before exch times out */

#define VEC_RETRIES_W		8	/* Retries before write fails */
#define VEC_RETRIES_R           8	/* Retries before read fails */
#define VEC_RETRIES_X		4	/* Retries before exch fails */

#define NET_ERROR_NONE		0	/* No error		*/
#define NET_ERROR_HARDWARE	(-1)	/* Hardware error	*/
#define NET_ERROR_OVERRUN	(-2)	/* Extra response(s)	*/
#define NET_ERROR_REPLY		(-3)	/* Reply parms mismatch */
#define NET_ERROR_ADDRESS	(-4)	/* Addr error response	*/
#define NET_ERROR_COMMAND	(-5)	/* Cmd error response	*/
#define NET_ERROR_PROT		(-6)	/* Prot error response	*/
#define NET_ERROR_TIMEOUT	(-7)	/* Too many retries	*/
#define NET_ERROR_VECTOR	(-8)	/* Invalid vector/path	*/
#define NET_ERROR_ROUTERLOCK	(-9)	/* Timeout locking rtr	*/
#define NET_ERROR_INVAL		(-10)	/* Invalid vector request */

#ifndef __ASSEMBLY__
#include <linux/types.h>
#include <asm/sn/types.h>

typedef uint64_t              net_reg_t;
typedef uint64_t              net_vec_t;

int             vector_write(net_vec_t dest,
                              int write_id, int address,
                              uint64_t value);

int             vector_read(net_vec_t dest,
                             int write_id, int address,
                             uint64_t *value);

int             vector_write_node(net_vec_t dest, nasid_t nasid,
                              int write_id, int address,
                              uint64_t value);

int             vector_read_node(net_vec_t dest, nasid_t nasid,
                             int write_id, int address,
                             uint64_t *value);

int             vector_length(net_vec_t vec);
net_vec_t       vector_get(net_vec_t vec, int n);
net_vec_t       vector_prefix(net_vec_t vec, int n);
net_vec_t       vector_modify(net_vec_t entry, int n, int route);
net_vec_t       vector_reverse(net_vec_t vec);
net_vec_t       vector_concat(net_vec_t vec1, net_vec_t vec2);

char		*net_errmsg(int);

#ifndef _STANDALONE
int hub_vector_write(cnodeid_t cnode, net_vec_t vector, int writeid,
	int addr, net_reg_t value);
int hub_vector_read(cnodeid_t cnode, net_vec_t vector, int writeid,
	int addr, net_reg_t *value);
#endif

#endif /* __ASSEMBLY__ */

#endif /* _ASM_IA64_SN_VECTOR_H */
