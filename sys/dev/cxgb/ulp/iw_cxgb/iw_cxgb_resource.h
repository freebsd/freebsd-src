/**************************************************************************

Copyright (c) 2007, 2008 Chelsio Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Neither the name of the Chelsio Corporation nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

$FreeBSD$

***************************************************************************/
#ifndef __CXIO_RESOURCE_H__
#define __CXIO_RESOURCE_H__

extern int cxio_hal_init_rhdl_resource(u32 nr_rhdl);
extern void cxio_hal_destroy_rhdl_resource(void);
extern int cxio_hal_init_resource(struct cxio_rdev *rdev_p,
				  u32 nr_tpt, u32 nr_pbl,
				  u32 nr_rqt, u32 nr_qpid, u32 nr_cqid,
				  u32 nr_pdid);
extern u32 cxio_hal_get_stag(struct cxio_hal_resource *rscp);
extern void cxio_hal_put_stag(struct cxio_hal_resource *rscp, u32 stag);
extern u32 cxio_hal_get_qpid(struct cxio_hal_resource *rscp);
extern void cxio_hal_put_qpid(struct cxio_hal_resource *rscp, u32 qpid);
extern u32 cxio_hal_get_cqid(struct cxio_hal_resource *rscp);
extern void cxio_hal_put_cqid(struct cxio_hal_resource *rscp, u32 cqid);
extern void cxio_hal_destroy_resource(struct cxio_hal_resource *rscp);

#define PBL_OFF(rdev_p, a) ( (a) - (rdev_p)->rnic_info.pbl_base )
extern int cxio_hal_pblpool_create(struct cxio_rdev *rdev_p);
extern void cxio_hal_pblpool_destroy(struct cxio_rdev *rdev_p);
extern u32 cxio_hal_pblpool_alloc(struct cxio_rdev *rdev_p, int size);
extern void cxio_hal_pblpool_free(struct cxio_rdev *rdev_p, u32 addr, int size);

#define RQT_OFF(rdev_p, a) ( (a) - (rdev_p)->rnic_info.rqt_base )
extern int cxio_hal_rqtpool_create(struct cxio_rdev *rdev_p);
extern void cxio_hal_rqtpool_destroy(struct cxio_rdev *rdev_p);
extern u32 cxio_hal_rqtpool_alloc(struct cxio_rdev *rdev_p, int size);
extern void cxio_hal_rqtpool_free(struct cxio_rdev *rdev_p, u32 addr, int size);
#endif
