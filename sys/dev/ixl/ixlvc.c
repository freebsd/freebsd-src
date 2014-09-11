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
	case I40E_VIRTCHNL_OP_GET_VF_RESOURCES:
		valid_len = 0;
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
	int		val_err;

	/*
	** Pre-validating messages to the PF, this might be
	** removed for performance later?
	*/
	val_err = ixl_vc_validate_vf_msg(sc, op, msg, len);
	if (val_err)
		device_printf(dev, "Error validating msg to PF for op %d,"
		    " msglen %d: error %d\n", op, len, val_err);

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
int ixlv_verify_api_ver(struct ixlv_sc *sc)
{
	struct i40e_virtchnl_version_info *pf_vvi;
	struct i40e_hw *hw = &sc->hw;
	struct i40e_arq_event_info event;
	i40e_status err;
	int retries = 0;

	event.buf_len = IXL_AQ_BUFSZ;
	event.msg_buf = malloc(event.buf_len, M_DEVBUF, M_NOWAIT);
	if (!event.msg_buf) {
		err = ENOMEM;
		goto out;
	}

	do {
		if (++retries > IXLV_AQ_MAX_ERR)
			goto out_alloc;

		/* NOTE: initial delay is necessary */
		i40e_msec_delay(100);
		err = i40e_clean_arq_element(hw, &event, NULL);
	} while (err == I40E_ERR_ADMIN_QUEUE_NO_WORK);
	if (err)
		goto out_alloc;

	err = (i40e_status)le32toh(event.desc.cookie_low);
	if (err) {
		err = EIO;
		goto out_alloc;
	}

	if ((enum i40e_virtchnl_ops)le32toh(event.desc.cookie_high) !=
	    I40E_VIRTCHNL_OP_VERSION) {
		err = EIO;
		goto out_alloc;
	}

	pf_vvi = (struct i40e_virtchnl_version_info *)event.msg_buf;
	if ((pf_vvi->major != I40E_VIRTCHNL_VERSION_MAJOR) ||
	    (pf_vvi->minor != I40E_VIRTCHNL_VERSION_MINOR))
		err = EIO;

out_alloc:
	free(event.msg_buf, M_DEVBUF);
out:
	return err;
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
	return ixlv_send_pf_msg(sc, I40E_VIRTCHNL_OP_GET_VF_RESOURCES,
				  NULL, 0);
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

	do {
		err = i40e_clean_arq_element(hw, &event, NULL);
		if (err == I40E_ERR_ADMIN_QUEUE_NO_WORK) {
			if (++retries <= IXLV_AQ_MAX_ERR)
				i40e_msec_delay(100);
		} else if ((enum i40e_virtchnl_ops)le32toh(event.desc.cookie_high) !=
		    I40E_VIRTCHNL_OP_GET_VF_RESOURCES) {
			device_printf(dev, "%s: Received a response from PF,"
			    " opcode %d, error %d\n", __func__,
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
			break;
		}

		if (retries > IXLV_AQ_MAX_ERR) {
			INIT_DBG_DEV(dev, "Did not receive response after %d tries.",
			    retries);
			goto out_alloc;
		}

	} while (err);

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
	int			len, pairs;;

	struct i40e_virtchnl_vsi_queue_config_info *vqci;
	struct i40e_virtchnl_queue_pair_info *vqpi;
	

	if (sc->current_op != I40E_VIRTCHNL_OP_UNKNOWN) {
		/* bail because we already have a command pending */
#ifdef IXL_DEBUG
		device_printf(dev, "%s: command %d pending\n",
			__func__, sc->current_op);
#endif
		return;
	}

	pairs = vsi->num_queues;
	sc->current_op = I40E_VIRTCHNL_OP_CONFIG_VSI_QUEUES;
	len = sizeof(struct i40e_virtchnl_vsi_queue_config_info) +
		       (sizeof(struct i40e_virtchnl_queue_pair_info) * pairs);
	vqci = malloc(len, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (!vqci) {
		device_printf(dev, "%s: unable to allocate memory\n", __func__);
		return;
	}
	vqci->vsi_id = sc->vsi_res->vsi_id;
	vqci->num_queue_pairs = pairs;
	vqpi = vqci->qpair;
	/* Size check is not needed here - HW max is 16 queue pairs, and we
	 * can fit info for 31 of them into the AQ buffer before it overflows.
	 */
	for (int i = 0; i < pairs; i++, que++) {
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
		vqpi++;
	}

	ixlv_send_pf_msg(sc, I40E_VIRTCHNL_OP_CONFIG_VSI_QUEUES,
			   (u8 *)vqci, len);
	free(vqci, M_DEVBUF);
	sc->aq_pending |= IXLV_FLAG_AQ_CONFIGURE_QUEUES;
	sc->aq_required &= ~IXLV_FLAG_AQ_CONFIGURE_QUEUES;
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

	if (sc->current_op != I40E_VIRTCHNL_OP_UNKNOWN) {
		/* we already have a command pending */
#ifdef IXL_DEBUG
		device_printf(sc->dev, "%s: command %d pending\n",
			__func__, sc->current_op);
#endif
		return;
	}
	sc->current_op = I40E_VIRTCHNL_OP_ENABLE_QUEUES;
	vqs.vsi_id = sc->vsi_res->vsi_id;
	vqs.tx_queues = (1 << sc->vsi_res->num_queue_pairs) - 1;
	vqs.rx_queues = vqs.tx_queues;
	ixlv_send_pf_msg(sc, I40E_VIRTCHNL_OP_ENABLE_QUEUES,
			   (u8 *)&vqs, sizeof(vqs));
	sc->aq_pending |= IXLV_FLAG_AQ_ENABLE_QUEUES;
	sc->aq_required &= ~IXLV_FLAG_AQ_ENABLE_QUEUES;
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

	if (sc->current_op != I40E_VIRTCHNL_OP_UNKNOWN) {
		/* we already have a command pending */
#ifdef IXL_DEBUG
		device_printf(sc->dev, "%s: command %d pending\n",
			__func__, sc->current_op);
#endif
		return;
	}
	sc->current_op = I40E_VIRTCHNL_OP_DISABLE_QUEUES;
	vqs.vsi_id = sc->vsi_res->vsi_id;
	vqs.tx_queues = (1 << sc->vsi_res->num_queue_pairs) - 1;
	vqs.rx_queues = vqs.tx_queues;
	ixlv_send_pf_msg(sc, I40E_VIRTCHNL_OP_DISABLE_QUEUES,
			   (u8 *)&vqs, sizeof(vqs));
	sc->aq_pending |= IXLV_FLAG_AQ_DISABLE_QUEUES;
	sc->aq_required &= ~IXLV_FLAG_AQ_DISABLE_QUEUES;
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

	if (sc->current_op != I40E_VIRTCHNL_OP_UNKNOWN) {
		/* we already have a command pending */
#ifdef IXL_DEBUG
		device_printf(sc->dev, "%s: command %d pending\n",
			__func__, sc->current_op);
#endif
		return;
	}
	sc->current_op = I40E_VIRTCHNL_OP_CONFIG_IRQ_MAP;

	/* How many queue vectors, adminq uses one */
	q = sc->msix - 1;

	len = sizeof(struct i40e_virtchnl_irq_map_info) +
	      (sc->msix * sizeof(struct i40e_virtchnl_vector_map));
	vm = malloc(len, M_DEVBUF, M_NOWAIT);
	if (!vm) {
		printf("%s: unable to allocate memory\n", __func__);
		return;
	}

	vm->num_vectors = sc->msix;
	/* Queue vectors first */
	for (i = 0; i < q; i++, que++) {
		vm->vecmap[i].vsi_id = sc->vsi_res->vsi_id;
		vm->vecmap[i].vector_id = i + 1; /* first is adminq */
		vm->vecmap[i].txq_map = (1 << que->me);
		vm->vecmap[i].rxq_map = (1 << que->me);
	}

	/* Misc vector last - this is only for AdminQ messages */
	vm->vecmap[i].vsi_id = sc->vsi_res->vsi_id;
	vm->vecmap[i].vector_id = 0;
	vm->vecmap[i].txq_map = 0;
	vm->vecmap[i].rxq_map = 0;

	ixlv_send_pf_msg(sc, I40E_VIRTCHNL_OP_CONFIG_IRQ_MAP,
	    (u8 *)vm, len);
	free(vm, M_DEVBUF);
	sc->aq_pending |= IXLV_FLAG_AQ_MAP_VECTORS;
	sc->aq_required &= ~IXLV_FLAG_AQ_MAP_VECTORS;
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

	if (sc->current_op != I40E_VIRTCHNL_OP_UNKNOWN)
		return;

	sc->current_op = I40E_VIRTCHNL_OP_ADD_VLAN;

	/* Get count of VLAN filters to add */
	SLIST_FOREACH(f, sc->vlan_filters, next) {
		if (f->flags & IXL_FILTER_ADD)
			cnt++;
	}

	if (!cnt) {  /* no work... */
		sc->aq_required &= ~IXLV_FLAG_AQ_ADD_VLAN_FILTER;
		sc->current_op = I40E_VIRTCHNL_OP_UNKNOWN;
		return;
	}

	len = sizeof(struct i40e_virtchnl_vlan_filter_list) +
	      (cnt * sizeof(u16));

	if (len > IXL_AQ_BUF_SZ) {
		device_printf(dev, "%s: Exceeded Max AQ Buf size\n",
			__func__);
		return;
	}

	v = malloc(len, M_DEVBUF, M_NOWAIT);
	if (!v) {
		device_printf(dev, "%s: unable to allocate memory\n",
			__func__);
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
	if (i == 0) { /* Should not happen... */
                device_printf(dev, "%s: i == 0?\n", __func__);
                return;
	}

	ixlv_send_pf_msg(sc, I40E_VIRTCHNL_OP_ADD_VLAN, (u8 *)v, len);
	free(v, M_DEVBUF);
	/* add stats? */
	sc->aq_pending |= IXLV_FLAG_AQ_ADD_VLAN_FILTER;
	sc->aq_required &= ~IXLV_FLAG_AQ_ADD_VLAN_FILTER;
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

	if (sc->current_op != I40E_VIRTCHNL_OP_UNKNOWN)
		return;

	sc->current_op = I40E_VIRTCHNL_OP_DEL_VLAN;

	/* Get count of VLAN filters to delete */
	SLIST_FOREACH(f, sc->vlan_filters, next) {
		if (f->flags & IXL_FILTER_DEL)
			cnt++;
	}

	if (!cnt) {  /* no work... */
		sc->aq_required &= ~IXLV_FLAG_AQ_DEL_VLAN_FILTER;
		sc->current_op = I40E_VIRTCHNL_OP_UNKNOWN;
		return;
	}

	len = sizeof(struct i40e_virtchnl_vlan_filter_list) +
	      (cnt * sizeof(u16));

	if (len > IXL_AQ_BUF_SZ) {
		device_printf(dev, "%s: Exceeded Max AQ Buf size\n",
			__func__);
		return;
	}

	v = malloc(len, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (!v) {
		device_printf(dev, "%s: unable to allocate memory\n",
			__func__);
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
	if (i == 0) { /* Should not happen... */
                device_printf(dev, "%s: i == 0?\n", __func__);
                return;
	}

	ixlv_send_pf_msg(sc, I40E_VIRTCHNL_OP_DEL_VLAN, (u8 *)v, len);
	free(v, M_DEVBUF);
	/* add stats? */
	sc->aq_pending |= IXLV_FLAG_AQ_DEL_VLAN_FILTER;
	sc->aq_required &= ~IXLV_FLAG_AQ_DEL_VLAN_FILTER;
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

	if (sc->current_op != I40E_VIRTCHNL_OP_UNKNOWN)
		return;

	sc->current_op = I40E_VIRTCHNL_OP_ADD_ETHER_ADDRESS;

	/* Get count of MAC addresses to add */
	SLIST_FOREACH(f, sc->mac_filters, next) {
		if (f->flags & IXL_FILTER_ADD)
			cnt++;
	}
	if (cnt == 0) { /* Should not happen... */
		DDPRINTF(dev, "cnt == 0, exiting...");
		sc->current_op = I40E_VIRTCHNL_OP_UNKNOWN;
		sc->aq_required &= ~IXLV_FLAG_AQ_ADD_MAC_FILTER;
		wakeup(&sc->add_ether_done);
		return;
	}

	len = sizeof(struct i40e_virtchnl_ether_addr_list) +
	    (cnt * sizeof(struct i40e_virtchnl_ether_addr));

	a = malloc(len, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (a == NULL) {
		device_printf(dev, "%s: Failed to get memory for "
		    "virtchnl_ether_addr_list\n", __func__);
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
	sc->aq_pending |= IXLV_FLAG_AQ_ADD_MAC_FILTER;
	sc->aq_required &= ~IXLV_FLAG_AQ_ADD_MAC_FILTER;
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

	if (sc->current_op != I40E_VIRTCHNL_OP_UNKNOWN)
		return;

	sc->current_op = I40E_VIRTCHNL_OP_DEL_ETHER_ADDRESS;

	/* Get count of MAC addresses to delete */
	SLIST_FOREACH(f, sc->mac_filters, next) {
		if (f->flags & IXL_FILTER_DEL)
			cnt++;
	}
	if (cnt == 0) {
		DDPRINTF(dev, "cnt == 0, exiting...");
		sc->aq_required &= ~IXLV_FLAG_AQ_DEL_MAC_FILTER;
		sc->current_op = I40E_VIRTCHNL_OP_UNKNOWN;
		wakeup(&sc->del_ether_done);
		return;
	}

	len = sizeof(struct i40e_virtchnl_ether_addr_list) +
	    (cnt * sizeof(struct i40e_virtchnl_ether_addr));

	d = malloc(len, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (d == NULL) {
		device_printf(dev, "%s: Failed to get memory for "
		    "virtchnl_ether_addr_list\n", __func__);
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
	sc->aq_pending |= IXLV_FLAG_AQ_DEL_MAC_FILTER;
	sc->aq_required &= ~IXLV_FLAG_AQ_DEL_MAC_FILTER;
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
	sc->current_op = I40E_VIRTCHNL_OP_UNKNOWN;
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

	if (sc->current_op != I40E_VIRTCHNL_OP_UNKNOWN)
		return;

	sc->current_op = I40E_VIRTCHNL_OP_GET_STATS;
	vqs.vsi_id = sc->vsi_res->vsi_id;
	error = ixlv_send_pf_msg(sc, I40E_VIRTCHNL_OP_GET_STATS,
	    (u8 *)&vqs, sizeof(vqs));
	/* Low priority, ok if it fails */
	if (error)
		sc->current_op = I40E_VIRTCHNL_OP_UNKNOWN;
}

/*
** Updates driver's stats counters with VSI stats returned from PF.
*/
void
ixlv_update_stats_counters(struct ixlv_sc *sc, struct i40e_eth_stats *es)
{
	struct ifnet *ifp = sc->vsi.ifp;

	ifp->if_ipackets = es->rx_unicast +
	                   es->rx_multicast +
			   es->rx_broadcast;
	ifp->if_opackets = es->tx_unicast +
	                   es->tx_multicast +
			   es->tx_broadcast;
	ifp->if_ibytes = es->rx_bytes;
	ifp->if_obytes = es->tx_bytes;
	ifp->if_imcasts = es->rx_multicast;
	ifp->if_omcasts = es->tx_multicast;

	ifp->if_oerrors = es->tx_errors;
	ifp->if_iqdrops = es->rx_discards;
	ifp->if_noproto = es->rx_unknown_protocol;

	sc->vsi.eth_stats = *es;
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
			vsi->link_up =
				vpe->event_data.link_event.link_status;
			vsi->link_speed =
				vpe->event_data.link_event.link_speed;
			break;
		case I40E_VIRTCHNL_EVENT_RESET_IMPENDING:
			device_printf(dev, "PF initiated reset!\n");
			sc->init_state = IXLV_RESET_PENDING;
			ixlv_init(sc);
			break;
		default:
			device_printf(dev, "%s: Unknown event %d from AQ\n",
				__func__, vpe->event);
			break;
		}

		return;
	}

	if (v_opcode != sc->current_op
	    && sc->current_op != I40E_VIRTCHNL_OP_GET_STATS) {
		device_printf(dev, "%s: Pending op is %d, received %d.\n",
			__func__, sc->current_op, v_opcode);
		sc->current_op = I40E_VIRTCHNL_OP_UNKNOWN;
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
		sc->aq_pending &= ~(IXLV_FLAG_AQ_ADD_MAC_FILTER);
		if (v_retval) {
			device_printf(dev, "WARNING: Error adding VF mac filter!\n");
			device_printf(dev, "WARNING: Device may not receive traffic!\n");
		}
		break;
	case I40E_VIRTCHNL_OP_DEL_ETHER_ADDRESS:
		sc->aq_pending &= ~(IXLV_FLAG_AQ_DEL_MAC_FILTER);
		break;
	case I40E_VIRTCHNL_OP_CONFIG_PROMISCUOUS_MODE:
		sc->aq_pending &= ~(IXLV_FLAG_AQ_CONFIGURE_PROMISC);
		break;
	case I40E_VIRTCHNL_OP_ADD_VLAN:
		sc->aq_pending &= ~(IXLV_FLAG_AQ_ADD_VLAN_FILTER);
		break;
	case I40E_VIRTCHNL_OP_DEL_VLAN:
		sc->aq_pending &= ~(IXLV_FLAG_AQ_DEL_VLAN_FILTER);
		break;
	case I40E_VIRTCHNL_OP_ENABLE_QUEUES:
		sc->aq_pending &= ~(IXLV_FLAG_AQ_ENABLE_QUEUES);
		if (v_retval == 0) {
			/* Turn on all interrupts */
			ixlv_enable_intr(vsi);
			/* And inform the stack we're ready */
			vsi->ifp->if_drv_flags |= IFF_DRV_RUNNING;
			vsi->ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
		}
		break;
	case I40E_VIRTCHNL_OP_DISABLE_QUEUES:
		sc->aq_pending &= ~(IXLV_FLAG_AQ_DISABLE_QUEUES);
		if (v_retval == 0) {
			/* Turn off all interrupts */
			ixlv_disable_intr(vsi);
			/* Tell the stack that the interface is no longer active */
			vsi->ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);
		}
		break;
	case I40E_VIRTCHNL_OP_CONFIG_VSI_QUEUES:
		sc->aq_pending &= ~(IXLV_FLAG_AQ_CONFIGURE_QUEUES);
		break;
	case I40E_VIRTCHNL_OP_CONFIG_IRQ_MAP:
		sc->aq_pending &= ~(IXLV_FLAG_AQ_MAP_VECTORS);
		break;
	default:
		device_printf(dev,
		    "%s: Received unexpected message %d from PF.\n",
		    __func__, v_opcode);
		break;
	}
	sc->current_op = I40E_VIRTCHNL_OP_UNKNOWN;
	return;
}
