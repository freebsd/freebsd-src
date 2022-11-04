/*
 * Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2022 Broadcom, All Rights Reserved.
 * The term Broadcom refers to Broadcom Limited and/or its subsidiaries
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS'
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "bnxt_mgmt.h" 
#include "bnxt.h"
#include "bnxt_hwrm.h" 
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <sys/endian.h>
#include <sys/lock.h>

/* Function prototypes */
static d_open_t      bnxt_mgmt_open;
static d_close_t     bnxt_mgmt_close;
static d_ioctl_t     bnxt_mgmt_ioctl;

/* Character device entry points */
static struct cdevsw bnxt_mgmt_cdevsw = {
	.d_version = D_VERSION,
	.d_open = bnxt_mgmt_open,
	.d_close = bnxt_mgmt_close,
	.d_ioctl = bnxt_mgmt_ioctl,
	.d_name = "bnxt_mgmt",
};

/* Global vars */
static struct cdev *bnxt_mgmt_dev;
struct mtx		mgmt_lock;

MALLOC_DEFINE(M_BNXT, "bnxt_mgmt_buffer", "buffer for bnxt_mgmt module");

/*
 * This function is called by the kld[un]load(2) system calls to
 * determine what actions to take when a module is loaded or unloaded.
 */
static int
bnxt_mgmt_loader(struct module *m, int what, void *arg)
{
	int error = 0;

	switch (what) {
	case MOD_LOAD:
		error = make_dev_p(MAKEDEV_CHECKNAME | MAKEDEV_WAITOK,
		    &bnxt_mgmt_dev,
		    &bnxt_mgmt_cdevsw,
		    0,
		    UID_ROOT,
		    GID_WHEEL,
		    0600,
		    "bnxt_mgmt");
		if (error != 0) {
			printf("%s: %s:%s:%d Failed to create the"
			       "bnxt_mgmt device node\n", DRIVER_NAME,
			       __FILE__, __FUNCTION__, __LINE__);
			return (error);
		}

		mtx_init(&mgmt_lock, "BNXT MGMT Lock", NULL, MTX_DEF);

		break;
	case MOD_UNLOAD:
		mtx_destroy(&mgmt_lock);
		destroy_dev(bnxt_mgmt_dev);
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}

	return (error);
}

static int
bnxt_mgmt_process_hwrm(struct cdev *dev, u_long cmd, caddr_t data,
		       int flag, struct thread *td)
{
	struct bnxt_softc *softc = NULL;
	struct bnxt_mgmt_req mgmt_req = {};
	struct bnxt_mgmt_fw_msg msg_temp, *msg, *msg2 = NULL;
	struct iflib_dma_info dma_data = {};
	void *user_ptr, *req, *resp;
	int ret = 0;
	uint16_t num_ind = 0;

	memcpy(&user_ptr, data, sizeof(user_ptr));
	if (copyin(user_ptr, &mgmt_req, sizeof(struct bnxt_mgmt_req))) {	
		printf("%s: %s:%d Failed to copy data from user\n",
			DRIVER_NAME, __FUNCTION__, __LINE__);
		return -EFAULT;
	}
	softc = bnxt_find_dev(mgmt_req.hdr.domain, mgmt_req.hdr.bus,
			      mgmt_req.hdr.devfn, NULL);
	if (!softc) {
		printf("%s: %s:%d unable to find softc reference\n",
			DRIVER_NAME, __FUNCTION__, __LINE__);
		return -ENODEV;
	}

	if (copyin((void*)mgmt_req.req.hreq, &msg_temp, sizeof(msg_temp))) {
		device_printf(softc->dev, "%s:%d Failed to copy data from user\n",
			      __FUNCTION__, __LINE__);
		return -EFAULT;
	}

	if (msg_temp.len_req > BNXT_MGMT_MAX_HWRM_REQ_LENGTH ||
			msg_temp.len_resp > BNXT_MGMT_MAX_HWRM_RESP_LENGTH) {
		device_printf(softc->dev, "%s:%d Invalid length\n", 
			      __FUNCTION__, __LINE__);
		return -EINVAL;
	}

	if (msg_temp.num_dma_indications > 1) {
		device_printf(softc->dev, "%s:%d Max num_dma_indications "
			      "supported is 1 \n", __FUNCTION__, __LINE__);
		return -EINVAL;
	}

	req = malloc(msg_temp.len_req, M_BNXT, M_WAITOK | M_ZERO);
	if(!req) {
		device_printf(softc->dev, "%s:%d Memory allocation failed",
			      __FUNCTION__, __LINE__);
		return -ENOMEM;
	}
	
	resp = malloc(msg_temp.len_resp, M_BNXT, M_WAITOK | M_ZERO);
	if(!resp) {
		device_printf(softc->dev, "%s:%d Memory allocation failed",
			      __FUNCTION__, __LINE__);
		ret = -ENOMEM;
		goto end;
	}

	if (copyin((void *)msg_temp.usr_req, req, msg_temp.len_req)) {
		device_printf(softc->dev, "%s:%d Failed to copy data from user\n",
			      __FUNCTION__, __LINE__);
		ret = -EFAULT;
		goto end;
	}

	msg = &msg_temp;
	num_ind = msg_temp.num_dma_indications;
	if (num_ind) {
		int size;
		void *dma_ptr;
		uint64_t *dmap;

		size = sizeof(struct bnxt_mgmt_fw_msg) + 
			     (num_ind * sizeof(struct dma_info));

		msg2 = malloc(size, M_BNXT, M_WAITOK | M_ZERO);
		if(!msg2) {
			device_printf(softc->dev, "%s:%d Memory allocation failed",
				      __FUNCTION__, __LINE__);
			ret = -ENOMEM;
			goto end;
		}

		if (copyin((void *)mgmt_req.req.hreq, msg2, size)) { 
			device_printf(softc->dev, "%s:%d Failed to copy"
				      "data from user\n", __FUNCTION__, __LINE__);
			ret = -EFAULT;
			goto end;
		}
		msg = msg2;
		
		ret = iflib_dma_alloc(softc->ctx, msg->dma[0].length, &dma_data,
				    BUS_DMA_NOWAIT);
		if (ret) {
			device_printf(softc->dev, "%s:%d iflib_dma_alloc"
				      "failed with ret = 0x%x\n", __FUNCTION__,
				      __LINE__, ret);
			ret = -ENOMEM;
			goto end;
		}

		if (!(msg->dma[0].read_or_write)) {
			if (copyin((void *)msg->dma[0].data, 
				   dma_data.idi_vaddr, 
				   msg->dma[0].length)) {
				device_printf(softc->dev, "%s:%d Failed to copy"
					      "data from user\n", __FUNCTION__,
					      __LINE__);
				ret = -EFAULT;
				goto end;
			}
		}
		dma_ptr = (void *) ((uint64_t) req + msg->dma[0].offset);
		dmap = dma_ptr;
		*dmap = htole64(dma_data.idi_paddr);
	}
   		
	ret = bnxt_hwrm_passthrough(softc, req, msg->len_req, resp, msg->len_resp, msg->timeout);
	if(ret)
		goto end;
	
	if (num_ind) {
		if ((msg->dma[0].read_or_write)) {
			if (copyout(dma_data.idi_vaddr, 
				    (void *)msg->dma[0].data, 
				    msg->dma[0].length)) {
				device_printf(softc->dev, "%s:%d Failed to copy data"
					      "to user\n", __FUNCTION__, __LINE__);
				ret = -EFAULT;
				goto end;
			}
		}
	}
	
	if (copyout(resp, (void *) msg->usr_resp, msg->len_resp)) {
		device_printf(softc->dev, "%s:%d Failed to copy response to user\n",
			      __FUNCTION__, __LINE__);
		ret = -EFAULT;
		goto end;
	}

end:
	if (req)
		free(req, M_BNXT);
	if (resp)
		free(resp, M_BNXT);
	if (msg2)
		free(msg2, M_BNXT);
	if (dma_data.idi_paddr)
		iflib_dma_free(&dma_data);
	return ret;
}

static int
bnxt_mgmt_get_dev_info(struct cdev *dev, u_long cmd, caddr_t data,
		       int flag, struct thread *td)
{
	struct bnxt_softc *softc = NULL;
	struct bnxt_dev_info dev_info;
	void *user_ptr;
	uint32_t dev_sn_lo, dev_sn_hi;
	int dev_sn_offset = 0;
	char dsn[16];
	uint16_t lnk;
	int capreg;

	memcpy(&user_ptr, data, sizeof(user_ptr));
	if (copyin(user_ptr, &dev_info, sizeof(dev_info))) {
		printf("%s: %s:%d Failed to copy data from user\n",
			DRIVER_NAME, __FUNCTION__, __LINE__);
		return -EFAULT;
	}
	
	softc = bnxt_find_dev(0, 0, 0, dev_info.nic_info.dev_name);
	if (!softc) {
		printf("%s: %s:%d unable to find softc reference\n",
			DRIVER_NAME, __FUNCTION__, __LINE__);
		return -ENODEV;
	}

	strncpy(dev_info.nic_info.driver_version, bnxt_driver_version, 64);
	strncpy(dev_info.nic_info.driver_name, device_get_name(softc->dev), 64);
	dev_info.pci_info.domain_no = softc->domain;
	dev_info.pci_info.bus_no = softc->bus;
	dev_info.pci_info.device_no = softc->slot;
	dev_info.pci_info.function_no = softc->function;
	dev_info.pci_info.vendor_id = pci_get_vendor(softc->dev);
	dev_info.pci_info.device_id = pci_get_device(softc->dev);
	dev_info.pci_info.sub_system_vendor_id = pci_get_subvendor(softc->dev);
	dev_info.pci_info.sub_system_device_id = pci_get_subdevice(softc->dev);
	dev_info.pci_info.revision = pci_read_config(softc->dev, PCIR_REVID, 1);
	dev_info.pci_info.chip_rev_id = (dev_info.pci_info.device_id << 16);
	dev_info.pci_info.chip_rev_id |= dev_info.pci_info.revision;
	if (pci_find_extcap(softc->dev, PCIZ_SERNUM, &dev_sn_offset)) {
		device_printf(softc->dev, "%s:%d device serial number is not found"
			      "or not supported\n", __FUNCTION__, __LINE__);
	} else {
		dev_sn_lo = pci_read_config(softc->dev, dev_sn_offset + 4, 4);
		dev_sn_hi = pci_read_config(softc->dev, dev_sn_offset + 8, 4);
		snprintf(dsn, sizeof(dsn), "%02x%02x%02x%02x%02x%02x%02x%02x",
			 (dev_sn_lo & 0x000000FF),
			 (dev_sn_lo >> 8) & 0x0000FF,
			 (dev_sn_lo >> 16) & 0x00FF,
			 (dev_sn_lo >> 24 ) & 0xFF,
			 (dev_sn_hi & 0x000000FF),
			 (dev_sn_hi >> 8) & 0x0000FF,
			 (dev_sn_hi >> 16) & 0x00FF,
			 (dev_sn_hi >> 24 ) & 0xFF);
		strncpy(dev_info.nic_info.device_serial_number, dsn, sizeof(dsn));
	}
	
	if_t ifp = iflib_get_ifp(softc->ctx);
	dev_info.nic_info.mtu = ifp->if_mtu;
	memcpy(dev_info.nic_info.mac, softc->func.mac_addr, ETHER_ADDR_LEN);
	
	if (pci_find_cap(softc->dev, PCIY_EXPRESS, &capreg)) {
		device_printf(softc->dev, "%s:%d pci link capability is not found"
			      "or not supported\n", __FUNCTION__, __LINE__);
	} else {
		lnk = pci_read_config(softc->dev, capreg + PCIER_LINK_STA, 2);
		dev_info.nic_info.pci_link_speed = (lnk & PCIEM_LINK_STA_SPEED);
		dev_info.nic_info.pci_link_width = (lnk & PCIEM_LINK_STA_WIDTH) >> 4;
	}
	
	if (copyout(&dev_info, user_ptr, sizeof(dev_info))) {
		device_printf(softc->dev, "%s:%d Failed to copy data to user\n",
			      __FUNCTION__, __LINE__);
		return -EFAULT;
	}

	return 0;
}

/*
 * IOCTL entry point.
 */
static int
bnxt_mgmt_ioctl(struct cdev *dev, u_long cmd, caddr_t data, int flag,
		struct thread *td)
{
	int ret = 0;
	
	switch(cmd) {
	case BNXT_MGMT_OPCODE_GET_DEV_INFO:
		ret = bnxt_mgmt_get_dev_info(dev, cmd, data, flag, td);
		break;
	case BNXT_MGMT_OPCODE_PASSTHROUGH_HWRM:
		mtx_lock(&mgmt_lock);
		ret = bnxt_mgmt_process_hwrm(dev, cmd, data, flag, td); 
		mtx_unlock(&mgmt_lock);
		break;
	default:
		printf("%s: Unknown command 0x%lx\n", DRIVER_NAME, cmd);
		ret = -EINVAL;
		break;
	}

	return ret;		
}

static int
bnxt_mgmt_close(struct cdev *dev, int flags, int devtype, struct thread *td)
{
	return (0);
}

static int
bnxt_mgmt_open(struct cdev *dev, int flags, int devtype, struct thread *td)
{
	return (0);
}

DEV_MODULE(bnxt_mgmt, bnxt_mgmt_loader, NULL);

