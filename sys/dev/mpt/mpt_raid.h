/* $FreeBSD$ */
/*-
 * Definitions for the integrated RAID features LSI MPT Fusion adapters.
 *
 * Copyright (c) 2005, WHEEL Sp. z o.o.
 * Copyright (c) 2004, 2005 Justin T. Gibbs
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon including
 *    a substantially similar Disclaimer requirement for further binary
 *    redistribution.
 * 3. Neither the names of the above listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF THE COPYRIGHT
 * OWNER OR CONTRIBUTOR IS ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef  _MPT_RAID_H_
#define  _MPT_RAID_H_

#include <cam/cam.h>
union ccb;

typedef enum {
	MPT_RAID_MWCE_ON,
	MPT_RAID_MWCE_OFF,
	MPT_RAID_MWCE_REBUILD_ONLY,
	MPT_RAID_MWCE_NC
} mpt_raid_mwce_t;

const char *
	mpt_vol_type(struct mpt_raid_volume *);
const char *
	mpt_vol_state(struct mpt_raid_volume *);
const char *
	mpt_disk_state(struct mpt_raid_disk *);
void	mpt_vol_prt(struct mpt_softc *, struct mpt_raid_volume *,
		    const char *fmt, ...);
void	mpt_disk_prt(struct mpt_softc *mpt, struct mpt_raid_disk *disk,
		     const char *fmt, ...);

int	mpt_issue_raid_req(struct mpt_softc *mpt, struct mpt_raid_volume *vol,
			   struct mpt_raid_disk *disk, request_t *req,
			   u_int Action, uint32_t ActionDataWord,
			   bus_addr_t addr, bus_size_t len, int write,
			   int wait);
cam_status
	mpt_map_physdisk(struct mpt_softc *mpt, union ccb *, u_int *tgt);
cam_status
	mpt_raid_quiesce_disk(struct mpt_softc *mpt,
			     struct mpt_raid_disk *mpt_disk,
			     request_t *req);
void	mpt_refresh_raid_data(struct mpt_softc *);
void	mpt_schedule_raid_refresh(struct mpt_softc *mpt);

static __inline void
mpt_raid_wakeup(struct mpt_softc *mpt)
{
	mpt->raid_wakeup++;
	wakeup(&mpt->raid_volumes);
}

#define MPT_RAID_SYNC_REPORT_INTERVAL (15 * 60 * hz)
#define MPT_RAID_RESYNC_RATE_MAX (255)
#define MPT_RAID_RESYNC_RATE_MIN (1)
#define MPT_RAID_RESYNC_RATE_NC (0)
#define MPT_RAID_RESYNC_RATE_DEFAULT MPT_RAID_RESYNC_RATE_NC

#define MPT_RAID_QUEUE_DEPTH_DEFAULT (128)

#define MPT_RAID_MWCE_DEFAULT MPT_RAID_MWCE_NC

#define RAID_VOL_FOREACH(mpt, mpt_vol)					\
	for (mpt_vol = (mpt)->raid_volumes;				\
	     mpt_vol != (mpt)->raid_volumes + (mpt)->raid_max_volumes;	\
	     mpt_vol++)

#endif /*_MPT_RAID_H_ */
