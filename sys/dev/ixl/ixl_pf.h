/******************************************************************************

  Copyright (c) 2013-2015, Intel Corporation 
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

#define	VF_FLAG_ENABLED			0x01
#define	VF_FLAG_SET_MAC_CAP		0x02
#define	VF_FLAG_VLAN_CAP		0x04
#define	VF_FLAG_PROMISC_CAP		0x08
#define	VF_FLAG_MAC_ANTI_SPOOF		0x10

#define IXL_PF_STATE_EMPR_RESETTING	(1 << 0)

struct ixl_vf {
	struct ixl_vsi		vsi;
	uint32_t		vf_flags;

	uint8_t			mac[ETHER_ADDR_LEN];
	uint16_t		vf_num;
	uint32_t		version;

	struct sysctl_ctx_list	ctx;
};

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
	int			state;

	struct mtx		pf_mtx;

	u32			qbase;
	u32 			admvec;
	struct task     	adminq;
	struct taskqueue	*tq;

	bool			link_up;
	u32			link_speed;
	int			advertised_speed;
	int			fc; /* local flow ctrl setting */

	/*
	** Network interfaces
	**   These are the traffic class holders, and
	**   will have a stack interface and queues 
	**   associated with them.
	** NOTE: The PF has only a single interface,
	**   so it is embedded in the PF struct.
	*/
	struct ixl_vsi		vsi;

	/* Misc stats maintained by the driver */
	u64			watchdog_events;
	u64			admin_irq;

	/* Statistics from hw */
	struct i40e_hw_port_stats 	stats;
	struct i40e_hw_port_stats	stats_offsets;
	bool 				stat_offsets_loaded;

	struct ixl_vf		*vfs;
	int			num_vfs;
	uint16_t		veb_seid;
	struct task		vflr_task;
	int			vc_debug_lvl;
};

/*
 * Defines used for NVM update ioctls.
 * This value is used in the Solaris tool, too.
 */
#define I40E_NVM_ACCESS \
     (((((((('E' << 4) + '1') << 4) + 'K') << 4) + 'G') << 4) | 5)

#define IXL_DEFAULT_PHY_INT_MASK \
     ((~(I40E_AQ_EVENT_LINK_UPDOWN | I40E_AQ_EVENT_MODULE_QUAL_FAIL \
      | I40E_AQ_EVENT_MEDIA_NA)) & 0x3FF)

/*** Sysctl help messages; displayed with "sysctl -d" ***/

#define IXL_SYSCTL_HELP_SET_ADVERTISE	\
"\nControl advertised link speed.\n"	\
"Flags:\n"				\
"\t 0x1 - advertise 100M\n"		\
"\t 0x2 - advertise 1G\n"		\
"\t 0x4 - advertise 10G\n"		\
"\t 0x8 - advertise 20G\n"		\
"\t0x10 - advertise 40G\n\n"		\
"Set to 0 to disable link."

#define IXL_SYSCTL_HELP_FC				\
"\nSet flow control mode using the values below.\n" 	\
"\t0 - off\n" 						\
"\t1 - rx pause\n" 					\
"\t2 - tx pause\n"					\
"\t3 - tx and rx pause"

#define IXL_SYSCTL_HELP_LINK_STATUS					\
"\nExecutes a \"Get Link Status\" command on the Admin Queue, and displays" \
" the response."			\

/*** Functions / Macros ***/

/*
** Put the NVM, EEtrackID, and OEM version information into a string
*/
static void
ixl_nvm_version_str(struct i40e_hw *hw, struct sbuf *buf)
{
	u8 oem_ver = (u8)(hw->nvm.oem_ver >> 24);
	u16 oem_build = (u16)((hw->nvm.oem_ver >> 16) & 0xFFFF);
	u8 oem_patch = (u8)(hw->nvm.oem_ver & 0xFF);

	sbuf_printf(buf,
	    "nvm %x.%02x etid %08x oem %d.%d.%d",
	    (hw->nvm.version & IXL_NVM_VERSION_HI_MASK) >>
	    IXL_NVM_VERSION_HI_SHIFT,
	    (hw->nvm.version & IXL_NVM_VERSION_LO_MASK) >>
	    IXL_NVM_VERSION_LO_SHIFT,
	    hw->nvm.eetrack,
	    oem_ver, oem_build, oem_patch);
}

static void
ixl_print_nvm_version(struct ixl_pf *pf)
{
	struct i40e_hw *hw = &pf->hw;
	device_t dev = pf->dev;
	struct sbuf *sbuf;

	sbuf = sbuf_new_auto();
	ixl_nvm_version_str(hw, sbuf);
	sbuf_finish(sbuf);
	device_printf(dev, "%s\n", sbuf_data(sbuf));
	sbuf_delete(sbuf);
}

#define	I40E_VC_DEBUG(pf, level, ...) \
	do { \
		if ((pf)->vc_debug_lvl >= (level)) \
			device_printf((pf)->dev, __VA_ARGS__); \
	} while (0)

#define	i40e_send_vf_nack(pf, vf, op, st) \
	ixl_send_vf_nack_msg((pf), (vf), (op), (st), __FILE__, __LINE__)

#define IXL_PF_LOCK_INIT(_sc, _name) \
        mtx_init(&(_sc)->pf_mtx, _name, "IXL PF Lock", MTX_DEF)
#define IXL_PF_LOCK(_sc)              mtx_lock(&(_sc)->pf_mtx)
#define IXL_PF_UNLOCK(_sc)            mtx_unlock(&(_sc)->pf_mtx)
#define IXL_PF_LOCK_DESTROY(_sc)      mtx_destroy(&(_sc)->pf_mtx)
#define IXL_PF_LOCK_ASSERT(_sc)       mtx_assert(&(_sc)->pf_mtx, MA_OWNED)

#endif /* _IXL_PF_H_ */
