/*
 * Copyright (c) 2008 QLogic, Inc.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __VNIC_MULTICAST_H__
#define __VNIC_MULTICAST_H__

enum {
	MCAST_STATE_INVALID         = 0x00, /* join not attempted or failed */
	MCAST_STATE_JOINING         = 0x01, /* join mcgroup in progress */
	MCAST_STATE_ATTACHING       = 0x02, /* join completed with success,
					     * attach qp to mcgroup in progress
					     */
	MCAST_STATE_JOINED_ATTACHED = 0x03, /* join completed with success */
	MCAST_STATE_DETACHING       = 0x04, /* detach qp in progress */
	MCAST_STATE_RETRIED         = 0x05, /* retried join and failed */
};

#define MAX_MCAST_JOIN_RETRIES 	       5 /* used to retry join */

struct mc_info {
	u8  			state;
	spinlock_t 		lock;
	union ib_gid 		mgid;
	u16 			mlid;
	struct ib_sa_multicast 	*mc;
	u8 			retries;
};


int vnic_mc_init(struct viport *viport);
void vnic_mc_uninit(struct viport *viport);
extern char *control_ifcfg_name(struct control *control);

/* This function is called when a viport gets a multicast mgid from EVIC
   and must join the multicast group. It sets up NEED_MCAST_JOIN flag, which
   results in vnic_mc_join being called later. */
void vnic_mc_join_setup(struct viport *viport, union ib_gid *mgid);

/* This function is called when NEED_MCAST_JOIN flag is set. */
int vnic_mc_join(struct viport *viport);

/* This function is called when NEED_MCAST_COMPLETION is set.
   It finishes off the join multicast work. */
int vnic_mc_join_handle_completion(struct viport *viport);

void vnic_mc_leave(struct viport *viport);

#endif /* __VNIC_MULTICAST_H__ */
