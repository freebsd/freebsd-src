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

/*
**	Virtual Channel support
**		These are support functions to communication
**		between the VF and PF drivers.
*/

#include "ixl.h"
#include "ixlv.h"
#include "i40e_prototype.h"


/* busy wait delay in msec */
#define IXLV_BUSY_WAIT_DELAY 10
#define IXLV_BUSY_WAIT_COUNT 50

static void	ixl_vc_process_resp(struct ixl_vc_mgr *, uint32_t,
		    enum i40e_status_code);
static void	ixl_vc_process_next(struct ixl_vc_mgr *mgr);
static void	ixl_vc_schedule_retry(struct ixl_vc_mgr *mgr);
static void	ixl_vc_send_current(struct ixl_vc_mgr *mgr);

#ifdef IXL_DEBUG
/*
** Validate VF messages
*/
static int ixl_vc_validate_vf_msg(struct ixlv_sc *sc, u32 v_opcode,
    u8 *msg, u16 msglen)
{
	bool err_msg_format = false;
	int valid_len;

	/* Validate message length. */
	switch (v_opcode) {
	case I40E_VIRTCHNL_OP_VERSION:
		valid_len = sizeof(struct i40e_virtchnl_version_info);
		break;
	case I40E_VIRTCHNL_OP_RESET_VF:
		valid_len = 0;
		break;
	case I40E_VIRTCHNL_OP_GET_VF_RESOURCES:
		/* Valid length in api v1.0 is 0, v1.1 is 4 */
		valid_len = 4;
		break;
	case I40E_VIRTCHNL_OP_CONFIG_TX_QUEUE:
		valid_len = sizeof(struct i40e_virtchnl_txq_info);
		break;
	case I40E_VIRTCHNL_OP_CONFIG_RX_QUEUE:
		valid_len = sizeof(struct i40e_virtchnl_rxq_info);
		break;
	case I40E_VIRTCHNL_OP_CONFIG_VSI_QUEUES:
		valid_len = sizeof(struct i40e_virtchnl_vsi_queue_config_info);
		if (msglen >= valid_len) {
			struct i40e_virtchnl_vsi_queue_config_info *vqc =
			    (struct i40e_virtchnl_vsi_queue_config_info *)msg;
			valid_len += (vqc->num_queue_pairs *
				      sizeof(struct
					     i40e_virtchnl_queue_pair_info));
			if (vqc->num_queue_pairs == 0)
				err_msg_format = true;
		}
		break;
	case I40E_VIRTCHNL_OP_CONFIG_IRQ_MAP:
		valid_len = sizeof(struct i40e_virtchnl_irq_map_info);
		if (msglen >= valid_len) {
			struct i40e_virtchnl_irq_map_info *vimi =
			    (struct i40e_virtchnl_irq_map_info *)msg;
			valid_len += (vimi->num_vectors *
				      sizeof(struct i40e_virtchnl_vector_map));
			if (vimi->num_vectors == 0)
				err_msg_format = true;
		}
		break;
	case I40E_VIRTCHNL_OP_ENABLE_QUEUES:
	case I40E_VIRTCHNL_OP_DISABLE_QUEUES:
		valid_len = sizeof(struct i40e_virtchnl_queue_select);
		break;
	case I40E_VIRTCHNL_OP_ADD_ETHER_ADDRESS:
	case I40E_VIRTCHNL_OP_DEL_ETHER_ADDRESS:
		valid_len = sizeof(struct i40e_virtchnl_ether_addr_list);
		if (msglen >= valid_len) {
			struct i40e_virtchnl_ether_addr_list *veal =
			    (struct i40e_virtchnl_ether_addr_list *)msg;
			valid_len += veal->num_elements *
			    sizeof(struct i40e_virtchnl_ether_addr);
			if (veal->num_elements == 0)
				err_msg_format = true;
		}
		break;
	case I40E_VIRTCHNL_OP_ADD_VLAN:
	case I40E_VIRTCHNL_OP_DEL_VLAN:
		valid_len = sizeof(struct i40e_virtchnl_vlan_filter_list);
		if (msglen >= valid_len) {
			struct i40e_virtchnl_vlan_filter_list *vfl =
			    (struct i40e_virtchnl_vlan_filter_list *)msg;
			valid_len += vfl->num_elements * sizeof(u16);
			if (vfl->num_elements == 0)
				err_msg_format = true;
		}
		break;
	case I40E_VIRTCHNL_OP_CONFIG_PROMISCUOUS_MODE:
		valid_len = sizeof(struct i40e_virtchnl_promisc_info);
		break;
	case I40E_VIRTCHNL_OP_GET_STATS:
		valid_len = sizeof(struct i40e_virtchnl_queue_select);
		break;
	/* These are always errors coming from the VF. */
	case I40E_VIRTCHNL_OP_EVENT:
	case I40E_VIRTCHNL_OP_UNKNOWN:
	default:
		return EPERM;
		break;
	}
	/* few more checks */
	if ((valid_len != msglen) || (err_msg_format))
		return EINVAL;
	else
		return 0;
}
#endif

/*
** ixlv_send_pf_msg
**
** Send message to PF and print status if failure.
*/
static int
ixlv_send_pf_msg(struct ixlv_sc *sc,
	enum i40e_virtchnl_ops op, u8 *msg, u16 len)
{
	struct i40e_hw	*hw = &sc->hw;
	device_t	dev = sc->dev;
	i40e_status	err;

#ifdef IXL_DEBUG
	/*
	** Pre-validating messages to the PF
	*/
	int val_err;
	val_err = ixl_vc_validate_vf_msg(sc, op, msg, len);
	if (val_err)
		device_printf(dev, "Error validating msg to PF for op %d,"
		    " msglen %d: error %d\n", op, len, val_err);
#endif

	err = i40e_aq_send_msg_to_pf(hw, op, I40E_SUCCESS, msg, len, NULL);
	if (err)
		device_printf(dev, "Unable to send opcode %d to PF, "
		    "error %d, aq status %d\n", op, err, hw->aq.asq_last_status);
	return err;
}


/*
** ixlv_send_api_ver
**
** Send API version admin queue message to the PF. The reply is not checked
** in this function. Returns 0 if the message was successfully
** sent, or one of the I40E_ADMIN_QUEUE_ERROR_ statuses if not.
*/
int
ixlv_send_api_ver(struct ixlv_sc *sc)
{
	struct i40e_virtchnl_version_info vvi;

	vvi.major = I40E_VIRTCHNL_VERSION_MAJOR;
	vvi.minor = I40E_VIRTCHNL_VERSION_MINOR;

	return ixlv_send_pf_msg(sc, I40E_VIRTCHNL_OP_VERSION,
	    (u8 *)&vvi, sizeof(vvi));
}

/*
** ixlv_verify_api_ver
**
** Compare API versions with the PF. Must be called after admin queue is
** initialized. Returns 0 if API versions match, EIO if
** they do not, or I40E_ERR_ADMIN_QUEUE_NO_WORK if the admin queue is empty.
*/
int
ixlv_verify_api_ver(struct ixlv_sc *sc)
{
	struct i40e_virtchnl_version_info *pf_vvi;
	struct i40e_hw *hw = &sc->hw;
	struct i40e_arq_event_info event;
	device_t dev = sc->dev;
	i40e_status err;
	int retries = 0;

	event.buf_len = IXL_AQ_BUF_SZ;
	event.msg_buf = malloc(event.buf_len, M_DEVBUF, M_NOWAIT);
	if (!event.msg_buf) {
		err = ENOMEM;
		goto out;
	}

	for (;;) {
		if (++retries > IXLV_AQ_MAX_ERR)
			goto out_alloc;

		/* Initial delay here is necessary */
		i40e_msec_pause(100);
		err = i40e_clean_arq_element(hw, &event, NULL);
		if (err == I40E_ERR_ADMIN_QUEUE_NO_WORK)
			continue;
		else if (err) {
			err = EIO;
			goto out_alloc;
		}

		if ((enum i40e_virtchnl_ops)le32toh(event.desc.cookie_high) !=
		    I40E_VIRTCHNL_OP_VERSION) {
			DDPRINTF(dev, "Received unexpected op response: %d\n",
			    le32toh(event.desc.cookie_high));
		    	/* Don't stop looking for expected response */
			continue;
		}

		err = (i40e_status)le32toh(event.desc.cookie_low);
		if (err) {
			err = EIO;
			goto out_alloc;
		} else
			break;
	}

	pf_vvi = (struct i40e_virtchnl_version_info *)event.msg_buf;
	if ((pf_vvi->major > I40E_VIRTCHNL_VERSION_MAJOR) ||
	    ((pf_vvi->major == I40E_VIRTCHNL_VERSION_MAJOR) &&
	    (pf_vvi->minor > I40E_VIRTCHNL_VERSION_MINOR))) {
		device_printf(dev, "Critical PF/VF API version mismatch!\n");
		err = EIO;
	} else
		sc->pf_version = pf_vvi->minor;
	
	/* Log PF/VF api versions */
	device_printf(dev, "PF API %d.%d / VF API %d.%d\n",
	    pf_vvi->major, pf_vvi->minor,
	    I40E_VIRTCHNL_VERSION_MAJOR, I40E_VIRTCHNL_VERSION_MINOR);

out_alloc:
	free(event.msg_buf, M_DEVBUF);
out:
	return (err);
}

/*
** ixlv_send_vf_config_msg
**
** Send VF configuration request admin queue message to the PF. The reply
** is not checked in this function. Returns 0 if the message was
** successfully sent, or one of the I40E_ADMIN_QUEUE_ERROR_ statuses if not.
*/
int
ixlv_send_vf_config_msg(struct ixlv_sc *sc)
{
	u32	caps;

	caps = I40E_VIRTCHNL_VF_OFFLOAD_L2 |
	    I40E_VIRTCHNL_VF_OFFLOAD_RSS_PF |
	    I40E_VIRTCHNL_VF_OFFLOAD_VLAN;

	if (sc->pf_version == I40E_VIRTCHNL_VERSION_MINOR_NO_VF_CAPS)
		return ixlv_send_pf_msg(sc, I40E_VIRTCHNL_OP_GET_VF_RESOURCES,
				  NULL, 0);
	else
		return ixlv_send_pf_msg(sc, I40E_VIRTCHNL_OP_GET_VF_RESOURCES,
				  (u8 *)&caps, sizeof(caps));
}

/*
** ixlv_get_vf_config
**
** Get VF configuration from PF and populate hw structure. Must be called after
** admin queue is initialized. Busy waits until response is received from PF,
** with maximum timeout. Response from PF is returned in the buffer for further
** processing by the caller.
*/
int
ixlv_get_vf_config(struct ixlv_sc *sc)
{
	struct i40e_hw	*hw = &sc->hw;
	device_t	dev = sc->dev;
	struct i40e_arq_event_info event;
	u16 len;
	i40e_status err = 0;
	u32 retries = 0;

	/* Note this assumes a single VSI */
	len = sizeof(struct i40e_virtchnl_vf_resource) +
	    sizeof(struct i40e_virtchnl_vsi_resource);
	event.buf_len = len;
	event.msg_buf = malloc(event.buf_len, M_DEVBUF, M_NOWAIT);
	if (!event.msg_buf) {
		err = ENOMEM;
		goto out;
	}

	for (;;) {
		err = i40e_clean_arq_element(hw, &event, NULL);
		if (err == I40E_ERR_ADMIN_QUEUE_NO_WORK) {
			if (++retries <= IXLV_AQ_MAX_ERR)
				i40e_msec_pause(10);
		} else if ((enum i40e_virtchnl_ops)le32toh(event.desc.cookie_high) !=
		    I40E_VIRTCHNL_OP_GET_VF_RESOURCES) {
			DDPRINTF(dev, "Received a response from PF,"
			    " opcode %d, error %d",
			    le32toh(event.desc.cookie_high),
			    le32toh(event.desc.cookie_low));
			retries++;
			continue;
		} else {
			err = (i40e_status)le32toh(event.desc.cookie_low);
			if (err) {
				device_printf(dev, "%s: Error returned from PF,"
				    " opcode %d, error %d\n", __func__,
				    le32toh(event.desc.cookie_high),
				    le32toh(event.desc.cookie_low));
				err = EIO;
				goto out_alloc;
			}
			/* We retrieved the config message, with no errors */
			break;
		}

		if (retries > IXLV_AQ_MAX_ERR) {
			INIT_DBG_DEV(dev, "Did not receive response after %d tries.",
			    retries);
			err = ETIMEDOUT;
			goto out_alloc;
		}
	}

	memcpy(sc->vf_res, event.msg_buf, min(event.msg_len, len));
	i40e_vf_parse_hw_config(hw, sc->vf_res);

out_alloc:
	free(event.msg_buf, M_DEVBUF);
out:
	return err;
}

/*
** ixlv_configure_queues
**
** Request that the PF set up our queues.
*/
void
ixlv_configure_queues(struct ixlv_sc *sc)
{
	device_t		dev = sc->dev;
	struct ixl_vsi		*vsi = &sc->vsi;
	struct ixl_queue	*que = vsi->queues;
	struct tx_ring		*txr;
	struct rx_ring		*rxr;
	int			len, pairs;

	struct i40e_virtchnl_vsi_queue_config_info *vqci;
	struct i40e_virtchnl_queue_pair_info *vqpi;

	pairs = vsi->num_queues;
	len = sizeof(struct i40e_virtchnl_vsi_queue_config_info) +
		       (sizeof(struct i40e_virtchnl_queue_pair_info) * pairs);
	vqci = malloc(len, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (!vqci) {
		device_printf(dev, "%s: unable to allocate memory\n", __func__);
		ixl_vc_schedule_retry(&sc->vc_mgr);
		return;
	}
	vqci->vsi_id = sc->vsi_res->vsi_id;
	vqci->num_queue_pairs = pairs;
	vqpi = vqci->qpair;
	/* Size check is not needed here - HW max is 16 queue pairs, and we
	 * can fit info for 31 of them into the AQ buffer before it overflows.
	 */
	for (int i = 0; i < pairs; i++, que++, vqpi++) {
		txr = &que->txr;
		rxr = &que->rxr;
		vqpi->txq.vsi_id = vqci->vsi_id;
		vqpi->txq.queue_id = i;
		vqpi->txq.ring_len = que->num_desc;
		vqpi->txq.dma_ring_addr = txr->dma.pa;
		/* Enable Head writeback */
		vqpi->txq.headwb_enabled = 1;
		vqpi->txq.dma_headwb_addr = txr->dma.pa +
		    (que->num_desc * sizeof(struct i40e_tx_desc));

		vqpi->rxq.vsi_id = vqci->vsi_id;
		vqpi->rxq.queue_id = i;
		vqpi->rxq.ring_len = que->num_desc;
		vqpi->rxq.dma_ring_addr = rxr->dma.pa;
		vqpi->rxq.max_pkt_size = vsi->max_frame_size;
		vqpi->rxq.databuffer_size = rxr->mbuf_sz;
		vqpi->rxq.splithdr_enabled = 0;
	}

	ixlv_send_pf_msg(sc, I40E_VIRTCHNL_OP_CONFIG_VSI_QUEUES,
			   (u8 *)vqci, len);
	free(vqci, M_DEVBUF);
}

/*
** ixlv_enable_queues
**
** Request that the PF enable all of our queues.
*/
void
ixlv_enable_queues(struct ixlv_sc *sc)
{
	struct i40e_virtchnl_queue_select vqs;

	vqs.vsi_id = sc->vsi_res->vsi_id;
	vqs.tx_queues = (1 << sc->vsi_res->num_queue_pairs) - 1;
	vqs.rx_queues = vqs.tx_queues;
	ixlv_send_pf_msg(sc, I40E_VIRTCHNL_OP_ENABLE_QUEUES,
			   (u8 *)&vqs, sizeof(vqs));
}

/*
** ixlv_disable_queues
**
** Request that the PF disable all of our queues.
*/
void
ixlv_disable_queues(struct ixlv_sc *sc)
{
	struct i40e_virtchnl_queue_select vqs;

	vqs.vsi_id = sc->vsi_res->vsi_id;
	vqs.tx_queues = (1 << sc->vsi_res->num_queue_pairs) - 1;
	vqs.rx_queues = vqs.tx_queues;
	ixlv_send_pf_msg(sc, I40E_VIRTCHNL_OP_DISABLE_QUEUES,
			   (u8 *)&vqs, sizeof(vqs));
}

/*
** ixlv_map_queues
**
** Request that the PF map queues to interrupt vectors. Misc causes, including
** admin queue, are always mapped to vector 0.
*/
void
ixlv_map_queues(struct ixlv_sc *sc)
{
	struct i40e_virtchnl_irq_map_info *vm;
	int 			i, q, len;
	struct ixl_vsi		*vsi = &sc->vsi;
	struct ixl_queue	*que = vsi->queues;

	/* How many queue vectors, adminq uses one */
	q = sc->msix - 1;

	len = sizeof(struct i40e_virtchnl_irq_map_info) +
	      (sc->msix * sizeof(struct i40e_virtchnl_vector_map));
	vm = malloc(len, M_DEVBUF, M_NOWAIT);
	if (!vm) {
		printf("%s: unable to allocate memory\n", __func__);
		ixl_vc_schedule_retry(&sc->vc_mgr);
		return;
	}

	vm->num_vectors = sc->msix;
	/* Queue vectors first */
	for (i = 0; i < q; i++, que++) {
		vm->vecmap[i].vsi_id = sc->vsi_res->vsi_id;
		vm->vecmap[i].vector_id = i + 1; /* first is adminq */
		vm->vecmap[i].txq_map = (1 << que->me);
		vm->vecmap[i].rxq_map = (1 << que->me);
		vm->vecmap[i].rxitr_idx = 0;
		vm->vecmap[i].txitr_idx = 1;
	}

	/* Misc vector last - this is only for AdminQ messages */
	vm->vecmap[i].vsi_id = sc->vsi_res->vsi_id;
	vm->vecmap[i].vector_id = 0;
	vm->vecmap[i].txq_map = 0;
	vm->vecmap[i].rxq_map = 0;
	vm->vecmap[i].rxitr_idx = 0;
	vm->vecmap[i].txitr_idx = 0;

	ixlv_send_pf_msg(sc, I40E_VIRTCHNL_OP_CONFIG_IRQ_MAP,
	    (u8 *)vm, len);
	free(vm, M_DEVBUF);
}

/*
** Scan the Filter List looking for vlans that need
** to be added, then create the data to hand to the AQ
** for handling.
*/
void
ixlv_add_vlans(struct ixlv_sc *sc)
{
	struct i40e_virtchnl_vlan_filter_list	*v;
	struct ixlv_vlan_filter *f, *ftmp;
	device_t	dev = sc->dev;
	int		len, i = 0, cnt = 0;

	/* Get count of VLAN filters to add */
	SLIST_FOREACH(f, sc->vlan_filters, next) {
		if (f->flags & IXL_FILTER_ADD)
			cnt++;
	}

	if (!cnt) {  /* no work... */
		ixl_vc_process_resp(&sc->vc_mgr, IXLV_FLAG_AQ_ADD_VLAN_FILTER,
		    I40E_SUCCESS);
		return;
	}

	len = sizeof(struct i40e_virtchnl_vlan_filter_list) +
	      (cnt * sizeof(u16));

	if (len > IXL_AQ_BUF_SZ) {
		device_printf(dev, "%s: Exceeded Max AQ Buf size\n",
			__func__);
		ixl_vc_schedule_retry(&sc->vc_mgr);
		return;
	}

	v = malloc(len, M_DEVBUF, M_NOWAIT);
	if (!v) {
		device_printf(dev, "%s: unable to allocate memory\n",
			__func__);
		ixl_vc_schedule_retry(&sc->vc_mgr);
		return;
	}

	v->vsi_id = sc->vsi_res->vsi_id;
	v->num_elements = cnt;

	/* Scan the filter array */
	SLIST_FOREACH_SAFE(f, sc->vlan_filters, next, ftmp) {
                if (f->flags & IXL_FILTER_ADD) {
                        bcopy(&f->vlan, &v->vlan_id[i], sizeof(u16));
			f->flags = IXL_FILTER_USED;
                        i++;
                }
                if (i == cnt)
                        break;
	}

	ixlv_send_pf_msg(sc, I40E_VIRTCHNL_OP_ADD_VLAN, (u8 *)v, len);
	free(v, M_DEVBUF);
	/* add stats? */
}

/*
** Scan the Filter Table looking for vlans that need
** to be removed, then create the data to hand to the AQ
** for handling.
*/
void
ixlv_del_vlans(struct ixlv_sc *sc)
{
	device_t	dev = sc->dev;
	struct i40e_virtchnl_vlan_filter_list *v;
	struct ixlv_vlan_filter *f, *ftmp;
	int len, i = 0, cnt = 0;

	/* Get count of VLAN filters to delete */
	SLIST_FOREACH(f, sc->vlan_filters, next) {
		if (f->flags & IXL_FILTER_DEL)
			cnt++;
	}

	if (!cnt) {  /* no work... */
		ixl_vc_process_resp(&sc->vc_mgr, IXLV_FLAG_AQ_DEL_VLAN_FILTER,
		    I40E_SUCCESS);
		return;
	}

	len = sizeof(struct i40e_virtchnl_vlan_filter_list) +
	      (cnt * sizeof(u16));

	if (len > IXL_AQ_BUF_SZ) {
		device_printf(dev, "%s: Exceeded Max AQ Buf size\n",
			__func__);
		ixl_vc_schedule_retry(&sc->vc_mgr);
		return;
	}

	v = malloc(len, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (!v) {
		device_printf(dev, "%s: unable to allocate memory\n",
			__func__);
		ixl_vc_schedule_retry(&sc->vc_mgr);
		return;
	}

	v->vsi_id = sc->vsi_res->vsi_id;
	v->num_elements = cnt;

	/* Scan the filter array */
	SLIST_FOREACH_SAFE(f, sc->vlan_filters, next, ftmp) {
                if (f->flags & IXL_FILTER_DEL) {
                        bcopy(&f->vlan, &v->vlan_id[i], sizeof(u16));
                        i++;
                        SLIST_REMOVE(sc->vlan_filters, f, ixlv_vlan_filter, next);
                        free(f, M_DEVBUF);
                }
                if (i == cnt)
                        break;
	}

	ixlv_send_pf_msg(sc, I40E_VIRTCHNL_OP_DEL_VLAN, (u8 *)v, len);
	free(v, M_DEVBUF);
	/* add stats? */
}


/*
** This routine takes additions to the vsi filter
** table and creates an Admin Queue call to create
** the filters in the hardware.
*/
void
ixlv_add_ether_filters(struct ixlv_sc *sc)
{
	struct i40e_virtchnl_ether_addr_list *a;
	struct ixlv_mac_filter	*f;
	device_t			dev = sc->dev;
	int				len, j = 0, cnt = 0;

	/* Get count of MAC addresses to add */
	SLIST_FOREACH(f, sc->mac_filters, next) {
		if (f->flags & IXL_FILTER_ADD)
			cnt++;
	}
	if (cnt == 0) { /* Should not happen... */
		DDPRINTF(dev, "cnt == 0, exiting...");
		ixl_vc_process_resp(&sc->vc_mgr, IXLV_FLAG_AQ_ADD_MAC_FILTER,
		    I40E_SUCCESS);
		return;
	}

	len = sizeof(struct i40e_virtchnl_ether_addr_list) +
	    (cnt * sizeof(struct i40e_virtchnl_ether_addr));

	a = malloc(len, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (a == NULL) {
		device_printf(dev, "%s: Failed to get memory for "
		    "virtchnl_ether_addr_list\n", __func__);
		ixl_vc_schedule_retry(&sc->vc_mgr);
		return;
	}
	a->vsi_id = sc->vsi.id;
	a->num_elements = cnt;

	/* Scan the filter array */
	SLIST_FOREACH(f, sc->mac_filters, next) {
		if (f->flags & IXL_FILTER_ADD) {
			bcopy(f->macaddr, a->list[j].addr, ETHER_ADDR_LEN);
			f->flags &= ~IXL_FILTER_ADD;
			j++;

			DDPRINTF(dev, "ADD: " MAC_FORMAT,
			    MAC_FORMAT_ARGS(f->macaddr));
		}
		if (j == cnt)
			break;
	}
	DDPRINTF(dev, "len %d, j %d, cnt %d",
	    len, j, cnt);
	ixlv_send_pf_msg(sc,
	    I40E_VIRTCHNL_OP_ADD_ETHER_ADDRESS, (u8 *)a, len);
	/* add stats? */
	free(a, M_DEVBUF);
	return;
}

/*
** This routine takes filters flagged for deletion in the
** sc MAC filter list and creates an Admin Queue call
** to delete those filters in the hardware.
*/
void
ixlv_del_ether_filters(struct ixlv_sc *sc)
{
	struct i40e_virtchnl_ether_addr_list *d;
	device_t			dev = sc->dev;
	struct ixlv_mac_filter	*f, *f_temp;
	int				len, j = 0, cnt = 0;

	/* Get count of MAC addresses to delete */
	SLIST_FOREACH(f, sc->mac_filters, next) {
		if (f->flags & IXL_FILTER_DEL)
			cnt++;
	}
	if (cnt == 0) {
		DDPRINTF(dev, "cnt == 0, exiting...");
		ixl_vc_process_resp(&sc->vc_mgr, IXLV_FLAG_AQ_DEL_MAC_FILTER,
		    I40E_SUCCESS);
		return;
	}

	len = sizeof(struct i40e_virtchnl_ether_addr_list) +
	    (cnt * sizeof(struct i40e_virtchnl_ether_addr));

	d = malloc(len, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (d == NULL) {
		device_printf(dev, "%s: Failed to get memory for "
		    "virtchnl_ether_addr_list\n", __func__);
		ixl_vc_schedule_retry(&sc->vc_mgr);
		return;
	}
	d->vsi_id = sc->vsi.id;
	d->num_elements = cnt;

	/* Scan the filter array */
	SLIST_FOREACH_SAFE(f, sc->mac_filters, next, f_temp) {
		if (f->flags & IXL_FILTER_DEL) {
			bcopy(f->macaddr, d->list[j].addr, ETHER_ADDR_LEN);
			DDPRINTF(dev, "DEL: " MAC_FORMAT,
			    MAC_FORMAT_ARGS(f->macaddr));
			j++;
			SLIST_REMOVE(sc->mac_filters, f, ixlv_mac_filter, next);
			free(f, M_DEVBUF);
		}
		if (j == cnt)
			break;
	}
	ixlv_send_pf_msg(sc,
	    I40E_VIRTCHNL_OP_DEL_ETHER_ADDRESS, (u8 *)d, len);
	/* add stats? */
	free(d, M_DEVBUF);
	return;
}

/*
** ixlv_request_reset
** Request that the PF reset this VF. No response is expected.
*/
void
ixlv_request_reset(struct ixlv_sc *sc)
{
	/*
	** Set the reset status to "in progress" before
	** the request, this avoids any possibility of
	** a mistaken early detection of completion.
	*/
	wr32(&sc->hw, I40E_VFGEN_RSTAT, I40E_VFR_INPROGRESS);
	ixlv_send_pf_msg(sc, I40E_VIRTCHNL_OP_RESET_VF, NULL, 0);
}

/*
** ixlv_request_stats
** Request the statistics for this VF's VSI from PF.
*/
void
ixlv_request_stats(struct ixlv_sc *sc)
{
	struct i40e_virtchnl_queue_select vqs;
	int error = 0;

	vqs.vsi_id = sc->vsi_res->vsi_id;
	/* Low priority, we don't need to error check */
	error = ixlv_send_pf_msg(sc, I40E_VIRTCHNL_OP_GET_STATS,
	    (u8 *)&vqs, sizeof(vqs));
#ifdef IXL_DEBUG
	if (error)
		device_printf(sc->dev, "Error sending stats request to PF: %d\n", error);
#endif
}

/*
** Updates driver's stats counters with VSI stats returned from PF.
*/
void
ixlv_update_stats_counters(struct ixlv_sc *sc, struct i40e_eth_stats *es)
{
	struct ixl_vsi *vsi = &sc->vsi;
	uint64_t tx_discards;

	tx_discards = es->tx_discards;
	for (int i = 0; i < vsi->num_queues; i++)
		tx_discards += sc->vsi.queues[i].txr.br->br_drops;

	/* Update ifnet stats */
	IXL_SET_IPACKETS(vsi, es->rx_unicast +
	                   es->rx_multicast +
			   es->rx_broadcast);
	IXL_SET_OPACKETS(vsi, es->tx_unicast +
	                   es->tx_multicast +
			   es->tx_broadcast);
	IXL_SET_IBYTES(vsi, es->rx_bytes);
	IXL_SET_OBYTES(vsi, es->tx_bytes);
	IXL_SET_IMCASTS(vsi, es->rx_multicast);
	IXL_SET_OMCASTS(vsi, es->tx_multicast);

	IXL_SET_OERRORS(vsi, es->tx_errors);
	IXL_SET_IQDROPS(vsi, es->rx_discards);
	IXL_SET_OQDROPS(vsi, tx_discards);
	IXL_SET_NOPROTO(vsi, es->rx_unknown_protocol);
	IXL_SET_COLLISIONS(vsi, 0);

	vsi->eth_stats = *es;
}

void
ixlv_config_rss_key(struct ixlv_sc *sc)
{
	struct i40e_virtchnl_rss_key *rss_key_msg;
	int msg_len, key_length;
	u8		rss_seed[IXL_RSS_KEY_SIZE];

#ifdef RSS
	/* Fetch the configured RSS key */
	rss_getkey((uint8_t *) &rss_seed);
#else
	ixl_get_default_rss_key((u32 *)rss_seed);
#endif

	/* Send the fetched key */
	key_length = IXL_RSS_KEY_SIZE;
	msg_len = sizeof(struct i40e_virtchnl_rss_key) + (sizeof(u8) * key_length) - 1;
	rss_key_msg = malloc(msg_len, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (rss_key_msg == NULL) {
		device_printf(sc->dev, "Unable to allocate msg memory for RSS key msg.\n");
		return;
	}

	rss_key_msg->vsi_id = sc->vsi_res->vsi_id;
	rss_key_msg->key_len = key_length;
	bcopy(rss_seed, &rss_key_msg->key[0], key_length);

	DDPRINTF(sc->dev, "config_rss: vsi_id %d, key_len %d",
	    rss_key_msg->vsi_id, rss_key_msg->key_len);
	
	ixlv_send_pf_msg(sc, I40E_VIRTCHNL_OP_CONFIG_RSS_KEY,
			  (u8 *)rss_key_msg, msg_len);

	free(rss_key_msg, M_DEVBUF);
}

void
ixlv_set_rss_hena(struct ixlv_sc *sc)
{
	struct i40e_virtchnl_rss_hena hena;

	hena.hena = IXL_DEFAULT_RSS_HENA;

	ixlv_send_pf_msg(sc, I40E_VIRTCHNL_OP_SET_RSS_HENA,
			  (u8 *)&hena, sizeof(hena));
}

void
ixlv_config_rss_lut(struct ixlv_sc *sc)
{
	struct i40e_virtchnl_rss_lut *rss_lut_msg;
	int msg_len;
	u16 lut_length;
	u32 lut;
	int i, que_id;

	lut_length = IXL_RSS_VSI_LUT_SIZE;
	msg_len = sizeof(struct i40e_virtchnl_rss_lut) + (lut_length * sizeof(u8)) - 1;
	rss_lut_msg = malloc(msg_len, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (rss_lut_msg == NULL) {
		device_printf(sc->dev, "Unable to allocate msg memory for RSS lut msg.\n");
		return;
	}

	rss_lut_msg->vsi_id = sc->vsi_res->vsi_id;
	/* Each LUT entry is a max of 1 byte, so this is easy */
	rss_lut_msg->lut_entries = lut_length;

	/* Populate the LUT with max no. of queues in round robin fashion */
	for (i = 0; i < lut_length; i++) {
#ifdef RSS
		/*
		 * Fetch the RSS bucket id for the given indirection entry.
		 * Cap it at the number of configured buckets (which is
		 * num_queues.)
		 */
		que_id = rss_get_indirection_to_bucket(i);
		que_id = que_id % sc->vsi.num_queues;
#else
		que_id = i % sc->vsi.num_queues;
#endif
		lut = que_id & IXL_RSS_VSI_LUT_ENTRY_MASK;
		rss_lut_msg->lut[i] = lut;
	}

	ixlv_send_pf_msg(sc, I40E_VIRTCHNL_OP_CONFIG_RSS_LUT,
			  (u8 *)rss_lut_msg, msg_len);

	free(rss_lut_msg, M_DEVBUF);
}

/*
** ixlv_vc_completion
**
** Asynchronous completion function for admin queue messages. Rather than busy
** wait, we fire off our requests and assume that no errors will be returned.
** This function handles the reply messages.
*/
void
ixlv_vc_completion(struct ixlv_sc *sc,
    enum i40e_virtchnl_ops v_opcode,
    i40e_status v_retval, u8 *msg, u16 msglen)
{
	device_t	dev = sc->dev;
	struct ixl_vsi	*vsi = &sc->vsi;

	if (v_opcode == I40E_VIRTCHNL_OP_EVENT) {
		struct i40e_virtchnl_pf_event *vpe =
			(struct i40e_virtchnl_pf_event *)msg;

		switch (vpe->event) {
		case I40E_VIRTCHNL_EVENT_LINK_CHANGE:
#ifdef IXL_DEBUG
			device_printf(dev, "Link change: status %d, speed %d\n",
			    vpe->event_data.link_event.link_status,
			    vpe->event_data.link_event.link_speed);
#endif
			sc->link_up =
				vpe->event_data.link_event.link_status;
			sc->link_speed =
				vpe->event_data.link_event.link_speed;
			ixlv_update_link_status(sc);
			break;
		case I40E_VIRTCHNL_EVENT_RESET_IMPENDING:
			device_printf(dev, "PF initiated reset!\n");
			sc->init_state = IXLV_RESET_PENDING;
			mtx_unlock(&sc->mtx);
			ixlv_init(vsi);
			mtx_lock(&sc->mtx);
			break;
		default:
			device_printf(dev, "%s: Unknown event %d from AQ\n",
				__func__, vpe->event);
			break;
		}

		return;
	}

	/* Catch-all error response */
	if (v_retval) {
		device_printf(dev,
		    "%s: AQ returned error %d to our request %d!\n",
		    __func__, v_retval, v_opcode);
	}

#ifdef IXL_DEBUG
	if (v_opcode != I40E_VIRTCHNL_OP_GET_STATS)
		DDPRINTF(dev, "opcode %d", v_opcode);
#endif

	switch (v_opcode) {
	case I40E_VIRTCHNL_OP_GET_STATS:
		ixlv_update_stats_counters(sc, (struct i40e_eth_stats *)msg);
		break;
	case I40E_VIRTCHNL_OP_ADD_ETHER_ADDRESS:
		ixl_vc_process_resp(&sc->vc_mgr, IXLV_FLAG_AQ_ADD_MAC_FILTER,
		    v_retval);
		if (v_retval) {
			device_printf(dev, "WARNING: Error adding VF mac filter!\n");
			device_printf(dev, "WARNING: Device may not receive traffic!\n");
		}
		break;
	case I40E_VIRTCHNL_OP_DEL_ETHER_ADDRESS:
		ixl_vc_process_resp(&sc->vc_mgr, IXLV_FLAG_AQ_DEL_MAC_FILTER,
		    v_retval);
		break;
	case I40E_VIRTCHNL_OP_CONFIG_PROMISCUOUS_MODE:
		ixl_vc_process_resp(&sc->vc_mgr, IXLV_FLAG_AQ_CONFIGURE_PROMISC,
		    v_retval);
		break;
	case I40E_VIRTCHNL_OP_ADD_VLAN:
		ixl_vc_process_resp(&sc->vc_mgr, IXLV_FLAG_AQ_ADD_VLAN_FILTER,
		    v_retval);
		break;
	case I40E_VIRTCHNL_OP_DEL_VLAN:
		ixl_vc_process_resp(&sc->vc_mgr, IXLV_FLAG_AQ_DEL_VLAN_FILTER,
		    v_retval);
		break;
	case I40E_VIRTCHNL_OP_ENABLE_QUEUES:
		ixl_vc_process_resp(&sc->vc_mgr, IXLV_FLAG_AQ_ENABLE_QUEUES,
		    v_retval);
		if (v_retval == 0) {
			/* Update link status */
			ixlv_update_link_status(sc);
			/* Turn on all interrupts */
			ixlv_enable_intr(vsi);
			/* And inform the stack we're ready */
			vsi->ifp->if_drv_flags |= IFF_DRV_RUNNING;
			/* TODO: Clear a state flag, so we know we're ready to run init again */
		}
		break;
	case I40E_VIRTCHNL_OP_DISABLE_QUEUES:
		ixl_vc_process_resp(&sc->vc_mgr, IXLV_FLAG_AQ_DISABLE_QUEUES,
		    v_retval);
		if (v_retval == 0) {
			/* Turn off all interrupts */
			ixlv_disable_intr(vsi);
			/* Tell the stack that the interface is no longer active */
			vsi->ifp->if_drv_flags &= ~(IFF_DRV_RUNNING);
		}
		break;
	case I40E_VIRTCHNL_OP_CONFIG_VSI_QUEUES:
		ixl_vc_process_resp(&sc->vc_mgr, IXLV_FLAG_AQ_CONFIGURE_QUEUES,
		    v_retval);
		break;
	case I40E_VIRTCHNL_OP_CONFIG_IRQ_MAP:
		ixl_vc_process_resp(&sc->vc_mgr, IXLV_FLAG_AQ_MAP_VECTORS,
		    v_retval);
		break;
	case I40E_VIRTCHNL_OP_CONFIG_RSS_KEY:
		ixl_vc_process_resp(&sc->vc_mgr, IXLV_FLAG_AQ_CONFIG_RSS_KEY,
		    v_retval);
		break;
	case I40E_VIRTCHNL_OP_SET_RSS_HENA:
		ixl_vc_process_resp(&sc->vc_mgr, IXLV_FLAG_AQ_SET_RSS_HENA,
		    v_retval);
		break;
	case I40E_VIRTCHNL_OP_CONFIG_RSS_LUT:
		ixl_vc_process_resp(&sc->vc_mgr, IXLV_FLAG_AQ_CONFIG_RSS_LUT,
		    v_retval);
		break;
	default:
#ifdef IXL_DEBUG
		device_printf(dev,
		    "%s: Received unexpected message %d from PF.\n",
		    __func__, v_opcode);
#endif
		break;
	}
	return;
}

static void
ixl_vc_send_cmd(struct ixlv_sc *sc, uint32_t request)
{

	switch (request) {
	case IXLV_FLAG_AQ_MAP_VECTORS:
		ixlv_map_queues(sc);
		break;

	case IXLV_FLAG_AQ_ADD_MAC_FILTER:
		ixlv_add_ether_filters(sc);
		break;

	case IXLV_FLAG_AQ_ADD_VLAN_FILTER:
		ixlv_add_vlans(sc);
		break;

	case IXLV_FLAG_AQ_DEL_MAC_FILTER:
		ixlv_del_ether_filters(sc);
		break;

	case IXLV_FLAG_AQ_DEL_VLAN_FILTER:
		ixlv_del_vlans(sc);
		break;

	case IXLV_FLAG_AQ_CONFIGURE_QUEUES:
		ixlv_configure_queues(sc);
		break;

	case IXLV_FLAG_AQ_DISABLE_QUEUES:
		ixlv_disable_queues(sc);
		break;

	case IXLV_FLAG_AQ_ENABLE_QUEUES:
		ixlv_enable_queues(sc);
		break;

	case IXLV_FLAG_AQ_CONFIG_RSS_KEY:
		ixlv_config_rss_key(sc);
		break;

	case IXLV_FLAG_AQ_SET_RSS_HENA:
		ixlv_set_rss_hena(sc);
		break;

	case IXLV_FLAG_AQ_CONFIG_RSS_LUT:
		ixlv_config_rss_lut(sc);
		break;
	}
}

void
ixl_vc_init_mgr(struct ixlv_sc *sc, struct ixl_vc_mgr *mgr)
{
	mgr->sc = sc;
	mgr->current = NULL;
	TAILQ_INIT(&mgr->pending);
	callout_init_mtx(&mgr->callout, &sc->mtx, 0);
}

static void
ixl_vc_process_completion(struct ixl_vc_mgr *mgr, enum i40e_status_code err)
{
	struct ixl_vc_cmd *cmd;

	cmd = mgr->current;
	mgr->current = NULL;
	cmd->flags &= ~IXLV_VC_CMD_FLAG_BUSY;

	cmd->callback(cmd, cmd->arg, err);
	ixl_vc_process_next(mgr);
}

static void
ixl_vc_process_resp(struct ixl_vc_mgr *mgr, uint32_t request,
    enum i40e_status_code err)
{
	struct ixl_vc_cmd *cmd;

	cmd = mgr->current;
	if (cmd == NULL || cmd->request != request)
		return;

	callout_stop(&mgr->callout);
	ixl_vc_process_completion(mgr, err);
}

static void
ixl_vc_cmd_timeout(void *arg)
{
	struct ixl_vc_mgr *mgr = (struct ixl_vc_mgr *)arg;

	IXLV_CORE_LOCK_ASSERT(mgr->sc);
	ixl_vc_process_completion(mgr, I40E_ERR_TIMEOUT);
}

static void
ixl_vc_cmd_retry(void *arg)
{
	struct ixl_vc_mgr *mgr = (struct ixl_vc_mgr *)arg;

	IXLV_CORE_LOCK_ASSERT(mgr->sc);
	ixl_vc_send_current(mgr);
}

static void
ixl_vc_send_current(struct ixl_vc_mgr *mgr)
{
	struct ixl_vc_cmd *cmd;

	cmd = mgr->current;
	ixl_vc_send_cmd(mgr->sc, cmd->request);
	callout_reset(&mgr->callout, IXLV_VC_TIMEOUT, ixl_vc_cmd_timeout, mgr);
}

static void
ixl_vc_process_next(struct ixl_vc_mgr *mgr)
{
	struct ixl_vc_cmd *cmd;

	if (mgr->current != NULL)
		return;

	if (TAILQ_EMPTY(&mgr->pending))
		return;

	cmd = TAILQ_FIRST(&mgr->pending);
	TAILQ_REMOVE(&mgr->pending, cmd, next);

	mgr->current = cmd;
	ixl_vc_send_current(mgr);
}

static void
ixl_vc_schedule_retry(struct ixl_vc_mgr *mgr)
{

	callout_reset(&mgr->callout, howmany(hz, 100), ixl_vc_cmd_retry, mgr);
}

void
ixl_vc_enqueue(struct ixl_vc_mgr *mgr, struct ixl_vc_cmd *cmd,
	    uint32_t req, ixl_vc_callback_t *callback, void *arg)
{
	IXLV_CORE_LOCK_ASSERT(mgr->sc);

	if (cmd->flags & IXLV_VC_CMD_FLAG_BUSY) {
		if (mgr->current == cmd)
			mgr->current = NULL;
		else
			TAILQ_REMOVE(&mgr->pending, cmd, next);
	}

	cmd->request = req;
	cmd->callback = callback;
	cmd->arg = arg;
	cmd->flags |= IXLV_VC_CMD_FLAG_BUSY;
	TAILQ_INSERT_TAIL(&mgr->pending, cmd, next);

	ixl_vc_process_next(mgr);
}

void
ixl_vc_flush(struct ixl_vc_mgr *mgr)
{
	struct ixl_vc_cmd *cmd;

	IXLV_CORE_LOCK_ASSERT(mgr->sc);
	KASSERT(TAILQ_EMPTY(&mgr->pending) || mgr->current != NULL,
	    ("ixlv: pending commands waiting but no command in progress"));

	cmd = mgr->current;
	if (cmd != NULL) {
		mgr->current = NULL;
		cmd->flags &= ~IXLV_VC_CMD_FLAG_BUSY;
		cmd->callback(cmd, cmd->arg, I40E_ERR_ADAPTER_STOPPED);
	}

	while ((cmd = TAILQ_FIRST(&mgr->pending)) != NULL) {
		TAILQ_REMOVE(&mgr->pending, cmd, next);
		cmd->flags &= ~IXLV_VC_CMD_FLAG_BUSY;
		cmd->callback(cmd, cmd->arg, I40E_ERR_ADAPTER_STOPPED);
	}

	callout_stop(&mgr->callout);
}

