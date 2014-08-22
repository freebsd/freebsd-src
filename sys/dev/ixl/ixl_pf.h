/******************************************************************************

  Copyright (c) 2013-2014, Intel Corporation 
  All rights reserved.
  
  Redistribution and use in source and binary forms, with or without 
  modification, are permitted provided that the following conditions are met:
  
   1. Redistributions of source code must retain the above copyright notice, 
      this list of conditions and the following disclaimer.
  
   2. Redistributions in binary form must reproduce the above copyright 
      notice, this list of conditions and the following disclaimer in the 
      documentation and/or other materials provided with the distribution.
  
   3. Neither the name of the Intel Corporation nor the names of its 
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

******************************************************************************/
/*$FreeBSD$*/


#ifndef _IXL_PF_H_
#define _IXL_PF_H_

/* Physical controller structure */
struct ixl_pf {
	struct i40e_hw		hw;
	struct i40e_osdep	osdep;
	struct device		*dev;

	struct resource		*pci_mem;
	struct resource		*msix_mem;

	/*
	 * Interrupt resources: this set is
	 * either used for legacy, or for Link
	 * when doing MSIX
	 */
	void			*tag;
	struct resource 	*res;

	struct callout		timer;
	int			msix;
	int			if_flags;

	struct mtx		pf_mtx;

	u32			qbase;
	u32 			admvec;
	struct task     	adminq;
	struct taskqueue	*tq;

	int			advertised_speed;

	/*
	** VSI - Stations: 
	**   These are the traffic class holders, and
	**   will have a stack interface and queues 
	**   associated with them.
	** NOTE: for now using just one, so embed it.
	*/
	struct ixl_vsi		vsi;

	/* Misc stats maintained by the driver */
	u64			watchdog_events;
	u64			admin_irq;

	/* Statistics from hw */
	struct i40e_hw_port_stats 	stats;
	struct i40e_hw_port_stats	stats_offsets;
	bool 				stat_offsets_loaded;
};


#define IXL_PF_LOCK_INIT(_sc, _name) \
        mtx_init(&(_sc)->pf_mtx, _name, "IXL PF Lock", MTX_DEF)
#define IXL_PF_LOCK(_sc)              mtx_lock(&(_sc)->pf_mtx)
#define IXL_PF_UNLOCK(_sc)            mtx_unlock(&(_sc)->pf_mtx)
#define IXL_PF_LOCK_DESTROY(_sc)      mtx_destroy(&(_sc)->pf_mtx)
#define IXL_PF_LOCK_ASSERT(_sc)       mtx_assert(&(_sc)->pf_mtx, MA_OWNED)

#endif /* _IXL_PF_H_ */
