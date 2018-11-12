/*
 * Copyright (c) 2013-2016 Qlogic Corporation
 * All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * File: ql_ioctl.c
 * Author : David C Somayajulu, Qlogic Corporation, Aliso Viejo, CA 92656.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");


#include "ql_os.h"
#include "ql_hw.h"
#include "ql_def.h"
#include "ql_inline.h"
#include "ql_glbl.h"
#include "ql_ioctl.h"

static int ql_eioctl(struct cdev *dev, u_long cmd, caddr_t data, int fflag,
		struct thread *td);

static struct cdevsw qla_cdevsw = {
	.d_version = D_VERSION,
	.d_ioctl = ql_eioctl,
	.d_name = "qlcnic",
};

int
ql_make_cdev(qla_host_t *ha)
{
        ha->ioctl_dev = make_dev(&qla_cdevsw,
				ha->ifp->if_dunit,
                                UID_ROOT,
                                GID_WHEEL,
                                0600,
                                "%s",
                                if_name(ha->ifp));

	if (ha->ioctl_dev == NULL)
		return (-1);

        ha->ioctl_dev->si_drv1 = ha;

	return (0);
}

void
ql_del_cdev(qla_host_t *ha)
{
	if (ha->ioctl_dev != NULL)
		destroy_dev(ha->ioctl_dev);
	return;
}

static int
ql_eioctl(struct cdev *dev, u_long cmd, caddr_t data, int fflag,
	struct thread *td)
{
        qla_host_t *ha;
        int rval = 0;
	device_t pci_dev;
	struct ifnet *ifp;

	q80_offchip_mem_val_t val;
	qla_rd_pci_ids_t *pci_ids;
	qla_rd_fw_dump_t *fw_dump;
        union {
		qla_reg_val_t *rv;
	        qla_rd_flash_t *rdf;
		qla_wr_flash_t *wrf;
		qla_erase_flash_t *erf;
		qla_offchip_mem_val_t *mem;
	} u;


        if ((ha = (qla_host_t *)dev->si_drv1) == NULL)
                return ENXIO;

	pci_dev= ha->pci_dev;

        switch(cmd) {

        case QLA_RDWR_REG:

                u.rv = (qla_reg_val_t *)data;

                if (u.rv->direct) {
                        if (u.rv->rd) {
                                u.rv->val = READ_REG32(ha, u.rv->reg);
                        } else {
                                WRITE_REG32(ha, u.rv->reg, u.rv->val);
                        }
                } else {
                        if ((rval = ql_rdwr_indreg32(ha, u.rv->reg, &u.rv->val,
                                u.rv->rd)))
                                rval = ENXIO;
                }
                break;

        case QLA_RD_FLASH:

		if (!ha->hw.flags.fdt_valid) {
			rval = EIO;
			break;
		}	

                u.rdf = (qla_rd_flash_t *)data;
                if ((rval = ql_rd_flash32(ha, u.rdf->off, &u.rdf->data)))
                        rval = ENXIO;
                break;

	case QLA_WR_FLASH:

		ifp = ha->ifp;

		if (ifp == NULL) {
			rval = ENXIO;
			break;
		}

		if (ifp->if_drv_flags & (IFF_DRV_OACTIVE | IFF_DRV_RUNNING)) {
			rval = ENXIO;
			break;
		}

		if (!ha->hw.flags.fdt_valid) {
			rval = EIO;
			break;
		}	

		u.wrf = (qla_wr_flash_t *)data;
		if ((rval = ql_wr_flash_buffer(ha, u.wrf->off, u.wrf->size,
			u.wrf->buffer))) {
			printf("flash write failed[%d]\n", rval);
			rval = ENXIO;
		}
		break;

	case QLA_ERASE_FLASH:

		ifp = ha->ifp;

		if (ifp == NULL) {
			rval = ENXIO;
			break;
		}

		if (ifp->if_drv_flags & (IFF_DRV_OACTIVE | IFF_DRV_RUNNING)) {
			rval = ENXIO;
			break;
		}

		if (!ha->hw.flags.fdt_valid) {
			rval = EIO;
			break;
		}	
		
		u.erf = (qla_erase_flash_t *)data;
		if ((rval = ql_erase_flash(ha, u.erf->off, 
			u.erf->size))) {
			printf("flash erase failed[%d]\n", rval);
			rval = ENXIO;
		}
		break;

	case QLA_RDWR_MS_MEM:
		u.mem = (qla_offchip_mem_val_t *)data;

		if ((rval = ql_rdwr_offchip_mem(ha, u.mem->off, &val, 
			u.mem->rd)))
			rval = ENXIO;
		else {
			u.mem->data_lo = val.data_lo;
			u.mem->data_hi = val.data_hi;
			u.mem->data_ulo = val.data_ulo;
			u.mem->data_uhi = val.data_uhi;
		}

		break;

	case QLA_RD_FW_DUMP_SIZE:

		if (ha->hw.mdump_init == 0) {
			rval = EINVAL;
			break;
		}
		
		fw_dump = (qla_rd_fw_dump_t *)data;
		fw_dump->template_size = ha->hw.dma_buf.minidump.size;
		fw_dump->pci_func = ha->pci_func;

		break;

	case QLA_RD_FW_DUMP:

		if (ha->hw.mdump_init == 0) {
			rval = EINVAL;
			break;
		}
		
		fw_dump = (qla_rd_fw_dump_t *)data;

		if ((fw_dump->md_template == NULL) ||
			(fw_dump->template_size != ha->hw.dma_buf.minidump.size)) {
			rval = EINVAL;
			break;
		}

		if ((rval = copyout(ha->hw.dma_buf.minidump.dma_b,
			fw_dump->md_template, fw_dump->template_size)))
			rval = ENXIO;
		break;

	case QLA_RD_PCI_IDS:
		pci_ids = (qla_rd_pci_ids_t *)data;
		pci_ids->ven_id = pci_get_vendor(pci_dev);
		pci_ids->dev_id = pci_get_device(pci_dev);
		pci_ids->subsys_ven_id = pci_get_subvendor(pci_dev);
		pci_ids->subsys_dev_id = pci_get_subdevice(pci_dev);
		pci_ids->rev_id = pci_read_config(pci_dev, PCIR_REVID, 1);
		break;

        default:
                break;
        }

        return rval;
}

