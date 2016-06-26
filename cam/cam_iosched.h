/*-
 * CAM IO Scheduler Interface
 *
 * Copyright (c) 2015 Netflix, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#ifndef _CAM_CAM_IOSCHED_H
#define _CAM_CAM_IOSCHED_H

/* No user-servicable parts in here. */
#ifdef _KERNEL

/* Forward declare all structs to keep interface thin */
struct cam_iosched_softc;
struct sysctl_ctx_list;
struct sysctl_oid;
union ccb;
struct bio;

int cam_iosched_init(struct cam_iosched_softc **, struct cam_periph *periph);
void cam_iosched_fini(struct cam_iosched_softc *);
void cam_iosched_sysctl_init(struct cam_iosched_softc *, struct sysctl_ctx_list *, struct sysctl_oid *);
struct bio *cam_iosched_next_trim(struct cam_iosched_softc *isc);
struct bio *cam_iosched_get_trim(struct cam_iosched_softc *isc);
struct bio *cam_iosched_next_bio(struct cam_iosched_softc *isc);
void cam_iosched_queue_work(struct cam_iosched_softc *isc, struct bio *bp);
void cam_iosched_flush(struct cam_iosched_softc *isc, struct devstat *stp, int err);
void cam_iosched_schedule(struct cam_iosched_softc *isc, struct cam_periph *periph);
void cam_iosched_finish_trim(struct cam_iosched_softc *isc);
void cam_iosched_submit_trim(struct cam_iosched_softc *isc);
void cam_iosched_put_back_trim(struct cam_iosched_softc *isc, struct bio *bp);
void cam_iosched_set_sort_queue(struct cam_iosched_softc *isc, int val);
int cam_iosched_has_work_flags(struct cam_iosched_softc *isc, uint32_t flags);
void cam_iosched_set_work_flags(struct cam_iosched_softc *isc, uint32_t flags);
void cam_iosched_clr_work_flags(struct cam_iosched_softc *isc, uint32_t flags);
void cam_iosched_trim_done(struct cam_iosched_softc *isc);
int cam_iosched_bio_complete(struct cam_iosched_softc *isc, struct bio *bp, union ccb *done_ccb);

#endif
#endif
