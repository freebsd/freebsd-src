/******************************************************************************

  Copyright (c) 2013-2018, Intel Corporation
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

#ifndef _IXLV_VC_MGR_H_
#define _IXLV_VC_MGR_H_

#include <sys/queue.h>

struct ixl_vc_cmd;

typedef void ixl_vc_callback_t(struct ixl_vc_cmd *, void *,
	enum i40e_status_code);


#define	IXLV_VC_CMD_FLAG_BUSY		0x0001

struct ixl_vc_cmd
{
	uint32_t request;
	uint32_t flags;

	ixl_vc_callback_t *callback;
	void *arg;

	TAILQ_ENTRY(ixl_vc_cmd) next;
};

struct ixl_vc_mgr
{
	struct ixlv_sc *sc;
	struct ixl_vc_cmd *current;
	struct callout callout;	

	TAILQ_HEAD(, ixl_vc_cmd) pending;
};

#define	IXLV_VC_TIMEOUT			(2 * hz)

void	ixl_vc_init_mgr(struct ixlv_sc *, struct ixl_vc_mgr *);
void	ixl_vc_enqueue(struct ixl_vc_mgr *, struct ixl_vc_cmd *,
	    uint32_t, ixl_vc_callback_t *, void *);
void	ixl_vc_flush(struct ixl_vc_mgr *mgr);

#endif

