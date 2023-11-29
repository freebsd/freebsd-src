/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020-2023, Broadcom Inc. All rights reserved.
 * Support: <fbsd-storage-driver.pdl@broadcom.com>
 *
 * Authors: Sumit Saxena <sumit.saxena@broadcom.com>
 *	    Chandrakanth Patil <chandrakanth.patil@broadcom.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 * 3. Neither the name of the Broadcom Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are
 * those of the authors and should not be interpreted as representing
 * official policies,either expressed or implied, of the FreeBSD Project.
 *
 * Mail to: Broadcom Inc 1320 Ridder Park Dr, San Jose, CA 95131
 *
 * Broadcom Inc. (Broadcom) MPI3MR Adapter FreeBSD
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include "mpi3mr_cam.h"
#include "mpi3mr_app.h"
#include "mpi3mr.h"

static d_open_t		mpi3mr_open;
static d_close_t	mpi3mr_close;
static d_ioctl_t	mpi3mr_ioctl;
static d_poll_t		mpi3mr_poll;

static struct cdevsw mpi3mr_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	0,
	.d_open =	mpi3mr_open,
	.d_close =	mpi3mr_close,
	.d_ioctl =	mpi3mr_ioctl,
	.d_poll =	mpi3mr_poll,
	.d_name =	"mpi3mr",
};

static struct mpi3mr_mgmt_info mpi3mr_mgmt_info;

static int
mpi3mr_open(struct cdev *dev, int flags, int fmt, struct thread *td)
{

	return (0);
}

static int
mpi3mr_close(struct cdev *dev, int flags, int fmt, struct thread *td)
{

	return (0);
}

/*
 * mpi3mr_app_attach - Char device registration
 * @sc: Adapter reference
 *
 * This function does char device registration.
 *
 * Return: 0 on success and proper error codes on failure
 */
int
mpi3mr_app_attach(struct mpi3mr_softc *sc)
{

	/* Create a /dev entry for Avenger controller */
	sc->mpi3mr_cdev = make_dev(&mpi3mr_cdevsw, device_get_unit(sc->mpi3mr_dev),
				   UID_ROOT, GID_OPERATOR, 0640, "mpi3mr%d",
				   device_get_unit(sc->mpi3mr_dev));

	if (sc->mpi3mr_cdev == NULL)
		return (ENOMEM);

	sc->mpi3mr_cdev->si_drv1 = sc;

	/* Assign controller instance to mgmt_info structure */
	if (device_get_unit(sc->mpi3mr_dev) == 0)
		memset(&mpi3mr_mgmt_info, 0, sizeof(mpi3mr_mgmt_info));
	mpi3mr_mgmt_info.count++;
	mpi3mr_mgmt_info.sc_ptr[mpi3mr_mgmt_info.max_index] = sc;
	mpi3mr_mgmt_info.max_index++;

	return (0);
}

void
mpi3mr_app_detach(struct mpi3mr_softc *sc)
{
	U8 i = 0;

	if (sc->mpi3mr_cdev == NULL)
		return;
	
	destroy_dev(sc->mpi3mr_cdev);
	for (i = 0; i < mpi3mr_mgmt_info.max_index; i++) {
		if (mpi3mr_mgmt_info.sc_ptr[i] == sc) {
			mpi3mr_mgmt_info.count--;
			mpi3mr_mgmt_info.sc_ptr[i] = NULL;
			break;
		}
	}
	return;
}

static int
mpi3mr_poll(struct cdev *dev, int poll_events, struct thread *td)
{
	int revents = 0;
	struct mpi3mr_softc *sc = NULL;
	sc = dev->si_drv1;

	if ((poll_events & (POLLIN | POLLRDNORM)) &&
	    (sc->mpi3mr_aen_triggered))
		revents |= poll_events & (POLLIN | POLLRDNORM);

	if (revents == 0) {
		if (poll_events & (POLLIN | POLLRDNORM)) {
			sc->mpi3mr_poll_waiting = 1;
			selrecord(td, &sc->mpi3mr_select);
		}
	}
	return revents;
}

/**
 * mpi3mr_app_get_adp_instancs - Get Adapter instance
 * @mrioc_id: Adapter ID
 *
 * This fucnction searches the Adapter reference with mrioc_id
 * upon found, returns the adapter reference otherwise returns
 * the NULL
 *
 * Return: Adapter reference on success and NULL on failure
 */
static struct mpi3mr_softc *
mpi3mr_app_get_adp_instance(U8 mrioc_id)
{
	struct mpi3mr_softc *sc = NULL;
	
	if (mrioc_id >= mpi3mr_mgmt_info.max_index)
		return NULL;

	sc = mpi3mr_mgmt_info.sc_ptr[mrioc_id];
	return sc;
}

static int 
mpi3mr_app_construct_nvme_sgl(struct mpi3mr_softc *sc,
			      Mpi3NVMeEncapsulatedRequest_t *nvme_encap_request,
			      struct mpi3mr_ioctl_mpt_dma_buffer *dma_buffers, U8 bufcnt)
{
	struct mpi3mr_nvme_pt_sge *nvme_sgl;
	U64 sgl_dma;
	U8 count;
	U16 available_sges = 0, i;
	U32 sge_element_size = sizeof(struct mpi3mr_nvme_pt_sge);
	size_t length = 0;
	struct mpi3mr_ioctl_mpt_dma_buffer *dma_buff = dma_buffers;
	U64 sgemod_mask = ((U64)((sc->facts.sge_mod_mask) <<
				 sc->facts.sge_mod_shift) << 32);
	U64 sgemod_val = ((U64)(sc->facts.sge_mod_value) <<
				sc->facts.sge_mod_shift) << 32;

	U32 size;

	nvme_sgl = (struct mpi3mr_nvme_pt_sge *)
		    ((U8 *)(nvme_encap_request->Command) + MPI3MR_NVME_CMD_SGL_OFFSET);

	/*
	 * Not all commands require a data transfer. If no data, just return
	 * without constructing any SGL.
	 */
	for (count = 0; count < bufcnt; count++, dma_buff++) {
		if ((dma_buff->data_dir == MPI3MR_APP_DDI) ||
		    (dma_buff->data_dir == MPI3MR_APP_DDO)) {
			length = dma_buff->kern_buf_len;
			break;
		}
	}
	if (!length || !dma_buff->num_dma_desc)
		return 0;

	if (dma_buff->num_dma_desc == 1) {
		available_sges = 1;
		goto build_sges;
	}
	sgl_dma = (U64)sc->ioctl_chain_sge.dma_addr;

	if (sgl_dma & sgemod_mask) {
		printf(IOCNAME "NVMe SGL address collides with SGEModifier\n",sc->name);
		return -1;
	}

	sgl_dma &= ~sgemod_mask;
	sgl_dma |= sgemod_val;

	memset(sc->ioctl_chain_sge.addr, 0, sc->ioctl_chain_sge.size);
	available_sges = sc->ioctl_chain_sge.size / sge_element_size;
	if (available_sges < dma_buff->num_dma_desc)
		return -1;
	memset(nvme_sgl, 0, sizeof(struct mpi3mr_nvme_pt_sge));
	nvme_sgl->base_addr = sgl_dma;
	size = dma_buff->num_dma_desc * sizeof(struct mpi3mr_nvme_pt_sge);
	nvme_sgl->length = htole32(size);
	nvme_sgl->type = MPI3MR_NVMESGL_LAST_SEGMENT;

	nvme_sgl = (struct mpi3mr_nvme_pt_sge *) sc->ioctl_chain_sge.addr;

build_sges:
	for (i = 0; i < dma_buff->num_dma_desc; i++) {
		sgl_dma = htole64(dma_buff->dma_desc[i].dma_addr);
		if (sgl_dma & sgemod_mask) {
			printf("%s: SGL address collides with SGE modifier\n",
			       __func__);
		return -1;
		}

		sgl_dma &= ~sgemod_mask;
		sgl_dma |= sgemod_val;

		nvme_sgl->base_addr = sgl_dma;
		nvme_sgl->length = htole32(dma_buff->dma_desc[i].size);
		nvme_sgl->type = MPI3MR_NVMESGL_DATA_SEGMENT;
		nvme_sgl++;
		available_sges--;
	}

	return 0;
}

static int
mpi3mr_app_build_nvme_prp(struct mpi3mr_softc *sc,
			  Mpi3NVMeEncapsulatedRequest_t *nvme_encap_request,
			  struct mpi3mr_ioctl_mpt_dma_buffer *dma_buffers, U8 bufcnt)
{
	int prp_size = MPI3MR_NVME_PRP_SIZE;
	U64 *prp_entry, *prp1_entry, *prp2_entry;
	U64 *prp_page;
	bus_addr_t prp_entry_dma, prp_page_dma, dma_addr;
	U32 offset, entry_len, dev_pgsz;
	U32 page_mask_result, page_mask;
	size_t length = 0, desc_len;
	U8 count;
	struct mpi3mr_ioctl_mpt_dma_buffer *dma_buff = dma_buffers;
	U64 sgemod_mask = ((U64)((sc->facts.sge_mod_mask) <<
			    sc->facts.sge_mod_shift) << 32);
	U64 sgemod_val = ((U64)(sc->facts.sge_mod_value) <<
			  sc->facts.sge_mod_shift) << 32;
	U16 dev_handle = nvme_encap_request->DevHandle;
	struct mpi3mr_target *tgtdev;
	U16 desc_count = 0;

	tgtdev = mpi3mr_find_target_by_dev_handle(sc->cam_sc, dev_handle);
	if (!tgtdev) {
		printf(IOCNAME "EncapNVMe Error: Invalid DevHandle 0x%02x\n", sc->name,
		       dev_handle);
		return -1;
	}
	if (tgtdev->dev_spec.pcie_inf.pgsz == 0) {
		printf(IOCNAME "%s: NVME device page size is zero for handle 0x%04x\n",
		       sc->name, __func__, dev_handle);
		return -1;
	}
	dev_pgsz = 1 << (tgtdev->dev_spec.pcie_inf.pgsz);

	page_mask = dev_pgsz - 1;

	if (dev_pgsz > MPI3MR_IOCTL_SGE_SIZE){
		printf("%s: NVMe device page size(%d) is greater than ioctl data sge size(%d) for handle 0x%04x\n",
		       __func__, dev_pgsz,  MPI3MR_IOCTL_SGE_SIZE, dev_handle);
		return -1;
	}

	if (MPI3MR_IOCTL_SGE_SIZE % dev_pgsz){
		printf("%s: ioctl data sge size(%d) is not a multiple of NVMe device page size(%d) for handle 0x%04x\n",
		       __func__, MPI3MR_IOCTL_SGE_SIZE, dev_pgsz, dev_handle);
		return -1;
	}

	/*
	 * Not all commands require a data transfer. If no data, just return
	 * without constructing any PRP.
	 */
	for (count = 0; count < bufcnt; count++, dma_buff++) {
		if ((dma_buff->data_dir == MPI3MR_APP_DDI) ||
		    (dma_buff->data_dir == MPI3MR_APP_DDO)) {
			length = dma_buff->kern_buf_len;
			break;
		}
	}
	if (!length || !dma_buff->num_dma_desc)
		return 0;

	for (count = 0; count < dma_buff->num_dma_desc; count++) {
		dma_addr = dma_buff->dma_desc[count].dma_addr;
		if (dma_addr & page_mask) {
			printf("%s:dma_addr 0x%lu is not aligned with page size 0x%x\n",
			       __func__,  dma_addr, dev_pgsz);
			return -1;
		}
	}

	dma_addr = dma_buff->dma_desc[0].dma_addr;
	desc_len = dma_buff->dma_desc[0].size;

	sc->nvme_encap_prp_sz = 0;
	if (bus_dma_tag_create(sc->mpi3mr_parent_dmat,		/* parent */
				4, 0,				/* algnmnt, boundary */
				sc->dma_loaddr,			/* lowaddr */
				sc->dma_hiaddr,			/* highaddr */
				NULL, NULL,			/* filter, filterarg */
				dev_pgsz,			/* maxsize */
                                1,				/* nsegments */
				dev_pgsz,			/* maxsegsize */
                                0,				/* flags */
                                NULL, NULL,			/* lockfunc, lockarg */
				&sc->nvme_encap_prp_list_dmatag)) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "Cannot create ioctl NVME kernel buffer dma tag\n");
		return (ENOMEM);
        }

	if (bus_dmamem_alloc(sc->nvme_encap_prp_list_dmatag, (void **)&sc->nvme_encap_prp_list,
			     BUS_DMA_NOWAIT, &sc->nvme_encap_prp_list_dma_dmamap)) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "Cannot allocate ioctl NVME dma memory\n");
		return (ENOMEM);
        }
	
	bzero(sc->nvme_encap_prp_list, dev_pgsz);
	bus_dmamap_load(sc->nvme_encap_prp_list_dmatag, sc->nvme_encap_prp_list_dma_dmamap,
			sc->nvme_encap_prp_list, dev_pgsz, mpi3mr_memaddr_cb, &sc->nvme_encap_prp_list_dma,
			0);
	
	if (!sc->nvme_encap_prp_list) {
		printf(IOCNAME "%s:%d Cannot load ioctl NVME dma memory for size: %d\n", sc->name,
		       __func__, __LINE__, dev_pgsz);
		goto err_out;
	}
	sc->nvme_encap_prp_sz = dev_pgsz;

	/*
	 * Set pointers to PRP1 and PRP2, which are in the NVMe command.
	 * PRP1 is located at a 24 byte offset from the start of the NVMe
	 * command.  Then set the current PRP entry pointer to PRP1.
	 */
	prp1_entry = (U64 *)((U8 *)(nvme_encap_request->Command) + MPI3MR_NVME_CMD_PRP1_OFFSET);
	prp2_entry = (U64 *)((U8 *)(nvme_encap_request->Command) + MPI3MR_NVME_CMD_PRP2_OFFSET);
	prp_entry = prp1_entry;
	/*
	 * For the PRP entries, use the specially allocated buffer of
	 * contiguous memory.
	 */
	prp_page = sc->nvme_encap_prp_list;
	prp_page_dma = sc->nvme_encap_prp_list_dma;

	/*
	 * Check if we are within 1 entry of a page boundary we don't
	 * want our first entry to be a PRP List entry.
	 */
	page_mask_result = (uintptr_t)((U8 *)prp_page + prp_size) & page_mask;
	if (!page_mask_result) {
		printf(IOCNAME "PRP Page is not page aligned\n", sc->name);
		goto err_out;
	}

	/*
	 * Set PRP physical pointer, which initially points to the current PRP
	 * DMA memory page.
	 */
	prp_entry_dma = prp_page_dma;


	/* Loop while the length is not zero. */
	while (length) {
		page_mask_result = (prp_entry_dma + prp_size) & page_mask;
		if (!page_mask_result && (length >  dev_pgsz)) {
			printf(IOCNAME "Single PRP page is not sufficient\n", sc->name);
			goto err_out;
		}

		/* Need to handle if entry will be part of a page. */
		offset = dma_addr & page_mask;
		entry_len = dev_pgsz - offset;

		if (prp_entry == prp1_entry) {
			/*
			 * Must fill in the first PRP pointer (PRP1) before
			 * moving on.
			 */
			*prp1_entry = dma_addr;
			if (*prp1_entry & sgemod_mask) {
				printf(IOCNAME "PRP1 address collides with SGEModifier\n", sc->name);
				goto err_out;
			}
			*prp1_entry &= ~sgemod_mask;
			*prp1_entry |= sgemod_val;

			/*
			 * Now point to the second PRP entry within the
			 * command (PRP2).
			 */
			prp_entry = prp2_entry;
		} else if (prp_entry == prp2_entry) {
			/*
			 * Should the PRP2 entry be a PRP List pointer or just
			 * a regular PRP pointer?  If there is more than one
			 * more page of data, must use a PRP List pointer.
			 */
			if (length > dev_pgsz) {
				/*
				 * PRP2 will contain a PRP List pointer because
				 * more PRP's are needed with this command. The
				 * list will start at the beginning of the
				 * contiguous buffer.
				 */
				*prp2_entry = prp_entry_dma;
				if (*prp2_entry & sgemod_mask) {
					printf(IOCNAME "PRP list address collides with SGEModifier\n", sc->name);
					goto err_out;
				}
				*prp2_entry &= ~sgemod_mask;
				*prp2_entry |= sgemod_val;

				/*
				 * The next PRP Entry will be the start of the
				 * first PRP List.
				 */
				prp_entry = prp_page;
				continue;
			} else {
				/*
				 * After this, the PRP Entries are complete.
				 * This command uses 2 PRP's and no PRP list.
				 */
				*prp2_entry = dma_addr;
				if (*prp2_entry & sgemod_mask) {
					printf(IOCNAME "PRP2 address collides with SGEModifier\n", sc->name);
					goto err_out;
				}
				*prp2_entry &= ~sgemod_mask;
				*prp2_entry |= sgemod_val;
			}
		} else {
			/*
			 * Put entry in list and bump the addresses.
			 *
			 * After PRP1 and PRP2 are filled in, this will fill in
			 * all remaining PRP entries in a PRP List, one per
			 * each time through the loop.
			 */
			*prp_entry = dma_addr;
			if (*prp_entry & sgemod_mask) {
				printf(IOCNAME "PRP address collides with SGEModifier\n", sc->name);
				goto err_out;
			}
			*prp_entry &= ~sgemod_mask;
			*prp_entry |= sgemod_val;
			prp_entry++;
			prp_entry_dma += prp_size;
		}

		/* Decrement length accounting for last partial page. */
		if (entry_len >= length)
			length = 0;
		else {
			if (entry_len <= desc_len) {
				dma_addr += entry_len;
				desc_len -= entry_len;
			}
			if (!desc_len) {
				if ((++desc_count) >=
				   dma_buff->num_dma_desc) {
					printf("%s: Invalid len %ld while building PRP\n",
					       __func__, length);
					goto err_out;
				}
				dma_addr =
				    dma_buff->dma_desc[desc_count].dma_addr;
				desc_len =
				    dma_buff->dma_desc[desc_count].size;
			}
			length -= entry_len;
		}
	}
	return 0;
err_out:
	if (sc->nvme_encap_prp_list && sc->nvme_encap_prp_list_dma) {
		bus_dmamap_unload(sc->nvme_encap_prp_list_dmatag, sc->nvme_encap_prp_list_dma_dmamap);
		bus_dmamem_free(sc->nvme_encap_prp_list_dmatag, sc->nvme_encap_prp_list, sc->nvme_encap_prp_list_dma_dmamap);
		bus_dma_tag_destroy(sc->nvme_encap_prp_list_dmatag);
		sc->nvme_encap_prp_list = NULL;
	}
	return -1;
}

 /**
+ * mpi3mr_map_data_buffer_dma - build dma descriptors for data
+ *                              buffers
+ * @sc: Adapter instance reference
+ * @dma_buff: buffer map descriptor
+ * @desc_count: Number of already consumed dma descriptors
+ *
+ * This function computes how many pre-allocated DMA descriptors
+ * are required for the given data buffer and if those number of
+ * descriptors are free, then setup the mapping of the scattered
+ * DMA address to the given data buffer, if the data direction
+ * of the buffer is DATA_OUT then the actual data is copied to
+ * the DMA buffers
+ *
+ * Return: 0 on success, -1 on failure
+ */
static int mpi3mr_map_data_buffer_dma(struct mpi3mr_softc *sc,
				      struct mpi3mr_ioctl_mpt_dma_buffer *dma_buffers,
				      U8 desc_count)
{
	U16 i, needed_desc = (dma_buffers->kern_buf_len / MPI3MR_IOCTL_SGE_SIZE);
	U32 buf_len = dma_buffers->kern_buf_len, copied_len = 0;
	int error;
	
	if (dma_buffers->kern_buf_len % MPI3MR_IOCTL_SGE_SIZE)
		needed_desc++;

	if ((needed_desc + desc_count) > MPI3MR_NUM_IOCTL_SGE) {
		printf("%s: DMA descriptor mapping error %d:%d:%d\n",
		       __func__, needed_desc, desc_count, MPI3MR_NUM_IOCTL_SGE);
		return -1;
	}

	dma_buffers->dma_desc = malloc(sizeof(*dma_buffers->dma_desc) * needed_desc,
				       M_MPI3MR, M_NOWAIT | M_ZERO);
	if (!dma_buffers->dma_desc)
		return -1;

	error = 0;
	for (i = 0; i < needed_desc; i++, desc_count++) {

		dma_buffers->dma_desc[i].addr = sc->ioctl_sge[desc_count].addr;
		dma_buffers->dma_desc[i].dma_addr = sc->ioctl_sge[desc_count].dma_addr;

		if (buf_len < sc->ioctl_sge[desc_count].size)
			dma_buffers->dma_desc[i].size = buf_len;
		else
			dma_buffers->dma_desc[i].size = sc->ioctl_sge[desc_count].size;

		buf_len -= dma_buffers->dma_desc[i].size;
		memset(dma_buffers->dma_desc[i].addr, 0, sc->ioctl_sge[desc_count].size);

		if (dma_buffers->data_dir == MPI3MR_APP_DDO) {
			error = copyin(((U8 *)dma_buffers->user_buf + copied_len),
			       dma_buffers->dma_desc[i].addr,
			       dma_buffers->dma_desc[i].size);
			if (error != 0)
				break;
			copied_len += dma_buffers->dma_desc[i].size;
		}
	}
	if (error != 0) {
		printf("%s: DMA copyin error %d\n", __func__, error);
		free(dma_buffers->dma_desc, M_MPI3MR);
		return -1;
	}

	dma_buffers->num_dma_desc = needed_desc;

	return 0;
}

static unsigned int 
mpi3mr_app_get_nvme_data_fmt(Mpi3NVMeEncapsulatedRequest_t *nvme_encap_request)
{
	U8 format = 0;

	format = ((nvme_encap_request->Command[0] & 0xc000) >> 14);
	return format;
}

static inline U16 mpi3mr_total_num_ioctl_sges(struct mpi3mr_ioctl_mpt_dma_buffer *dma_buffers,
					      U8 bufcnt)
{
	U16 i, sge_count = 0;
	for (i=0; i < bufcnt; i++, dma_buffers++) {
		if ((dma_buffers->data_dir == MPI3MR_APP_DDN) ||
		    dma_buffers->kern_buf)
			continue;
		sge_count += dma_buffers->num_dma_desc;
		if (!dma_buffers->num_dma_desc)
			sge_count++;
	}
	return sge_count;
}

static int
mpi3mr_app_construct_sgl(struct mpi3mr_softc *sc, U8 *mpi_request, U32 sgl_offset,
			 struct mpi3mr_ioctl_mpt_dma_buffer *dma_buffers,
			 U8 bufcnt, U8 is_rmc, U8 is_rmr, U8 num_datasges)
{
	U8 *sgl = (mpi_request + sgl_offset), count = 0;
	Mpi3RequestHeader_t *mpi_header = (Mpi3RequestHeader_t *)mpi_request;
	Mpi3MgmtPassthroughRequest_t *rmgmt_req = 
		(Mpi3MgmtPassthroughRequest_t *)mpi_request;
	struct mpi3mr_ioctl_mpt_dma_buffer *dma_buff = dma_buffers;
	U8 flag, sgl_flags, sgl_flags_eob, sgl_flags_last, last_chain_sgl_flags;
	U16 available_sges, i, sges_needed;
	U32 sge_element_size = sizeof(struct _MPI3_SGE_COMMON);
	bool chain_used = false;

	sgl_flags = MPI3_SGE_FLAGS_ELEMENT_TYPE_SIMPLE |
		MPI3_SGE_FLAGS_DLAS_SYSTEM ;
	sgl_flags_eob = sgl_flags | MPI3_SGE_FLAGS_END_OF_BUFFER;
	sgl_flags_last = sgl_flags_eob | MPI3_SGE_FLAGS_END_OF_LIST;
	last_chain_sgl_flags = MPI3_SGE_FLAGS_ELEMENT_TYPE_LAST_CHAIN |
	    MPI3_SGE_FLAGS_DLAS_SYSTEM;
	
	sges_needed = mpi3mr_total_num_ioctl_sges(dma_buffers, bufcnt);

	if (is_rmc) {
		mpi3mr_add_sg_single(&rmgmt_req->CommandSGL,
		    sgl_flags_last, dma_buff->kern_buf_len,
		    dma_buff->kern_buf_dma);
		sgl = (U8 *) dma_buff->kern_buf + dma_buff->user_buf_len;
		available_sges = (dma_buff->kern_buf_len -
		    dma_buff->user_buf_len) / sge_element_size;
		if (sges_needed > available_sges)
			return -1;
		chain_used = true;
		dma_buff++;
		count++;
		if (is_rmr) {
			mpi3mr_add_sg_single(&rmgmt_req->ResponseSGL,
			    sgl_flags_last, dma_buff->kern_buf_len,
			    dma_buff->kern_buf_dma);
			dma_buff++;
			count++;
		} else
			mpi3mr_build_zero_len_sge(
			    &rmgmt_req->ResponseSGL);
		if (num_datasges) {
			i = 0;
			goto build_sges;
		}
	} else {
		if (sgl_offset >= MPI3MR_AREQ_FRAME_SZ)
			return -1;
		available_sges = (MPI3MR_AREQ_FRAME_SZ - sgl_offset) /
		    sge_element_size;
		if (!available_sges)
			return -1;
	}

	if (!num_datasges) {
		mpi3mr_build_zero_len_sge(sgl);
		return 0;
	}

	if (mpi_header->Function == MPI3_FUNCTION_SMP_PASSTHROUGH) {
		if ((sges_needed > 2) || (sges_needed > available_sges))
			return -1;
		for (; count < bufcnt; count++, dma_buff++) {
			if ((dma_buff->data_dir == MPI3MR_APP_DDN) ||
			    !dma_buff->num_dma_desc)
				continue;
			mpi3mr_add_sg_single(sgl, sgl_flags_last,
			    dma_buff->dma_desc[0].size,
			    dma_buff->dma_desc[0].dma_addr);
			sgl += sge_element_size;
		}
		return 0;
	}
	i = 0;

build_sges:
	for (; count < bufcnt; count++, dma_buff++) {
		if (dma_buff->data_dir == MPI3MR_APP_DDN)
			continue;
		if (!dma_buff->num_dma_desc) {
			if (chain_used && !available_sges)
				return -1;
			if (!chain_used && (available_sges == 1) &&
			    (sges_needed > 1))
				goto setup_chain;
			flag = sgl_flags_eob;
			if (num_datasges == 1)
				flag = sgl_flags_last;
			mpi3mr_add_sg_single(sgl, flag, 0, 0);
			sgl += sge_element_size;
			available_sges--;
			sges_needed--;
			num_datasges--;
			continue;
		}
		for (; i < dma_buff->num_dma_desc; i++) {
			if (chain_used && !available_sges)
				return -1;
			if (!chain_used && (available_sges == 1) &&
			    (sges_needed > 1))
				goto setup_chain;
			flag = sgl_flags;
			if (i == (dma_buff->num_dma_desc - 1)) {
				if (num_datasges == 1)
					flag = sgl_flags_last;
				else
					flag = sgl_flags_eob;
			}

			mpi3mr_add_sg_single(sgl, flag,
			    dma_buff->dma_desc[i].size,
			    dma_buff->dma_desc[i].dma_addr);
			sgl += sge_element_size;
			available_sges--;
			sges_needed--;
		}
		num_datasges--;
		i = 0;
	}
	return 0;

setup_chain:
	available_sges = sc->ioctl_chain_sge.size / sge_element_size;
	if (sges_needed > available_sges)
		return -1;
	mpi3mr_add_sg_single(sgl, last_chain_sgl_flags,
	    (sges_needed * sge_element_size), sc->ioctl_chain_sge.dma_addr);
	memset(sc->ioctl_chain_sge.addr, 0, sc->ioctl_chain_sge.size);
	sgl = (U8 *)sc->ioctl_chain_sge.addr;
	chain_used = true;
	goto build_sges;
}


/**
 * mpi3mr_app_mptcmds - MPI Pass through IOCTL handler
 * @dev: char device
 * @cmd: IOCTL command
 * @arg: User data payload buffer for the IOCTL
 * @flag: flags
 * @thread: threads
 *
 * This function is the top level handler for MPI Pass through
 * IOCTL, this does basic validation of the input data buffers,
 * identifies the given buffer types and MPI command, allocates
 * DMAable memory for user given buffers, construstcs SGL
 * properly and passes the command to the firmware.
 *
 * Once the MPI command is completed the driver copies the data
 * if any and reply, sense information to user provided buffers.
 * If the command is timed out then issues controller reset
 * prior to returning.
 *
 * Return: 0 on success and proper error codes on failure
 */
static long
mpi3mr_app_mptcmds(struct cdev *dev, u_long cmd, void *uarg,
		   int flag, struct thread *td)
{
	long rval = EINVAL;
	U8 count, bufcnt = 0, is_rmcb = 0, is_rmrb = 0, din_cnt = 0, dout_cnt = 0;
	U8 invalid_be = 0, erb_offset = 0xFF, mpirep_offset = 0xFF;
	U16 desc_count = 0;
	U8 nvme_fmt = 0;
	U32 tmplen = 0, erbsz = MPI3MR_SENSEBUF_SZ, din_sz = 0, dout_sz = 0;
	U8 *kern_erb = NULL;
	U8 *mpi_request = NULL;
	Mpi3RequestHeader_t *mpi_header = NULL;
	Mpi3PELReqActionGetCount_t *pel = NULL;
	Mpi3StatusReplyDescriptor_t *status_desc = NULL;
	struct mpi3mr_softc *sc = NULL;
	struct mpi3mr_ioctl_buf_entry_list *buffer_list = NULL;
	struct mpi3mr_buf_entry *buf_entries = NULL;
	struct mpi3mr_ioctl_mpt_dma_buffer *dma_buffers = NULL, *dma_buff = NULL;
	struct mpi3mr_ioctl_mpirepbuf *mpirepbuf = NULL;
	struct mpi3mr_ioctl_mptcmd *karg = (struct mpi3mr_ioctl_mptcmd *)uarg;


	sc = mpi3mr_app_get_adp_instance(karg->mrioc_id);
	if (!sc)
		return ENODEV;

	if (!sc->ioctl_sges_allocated) {
		printf("%s: DMA memory was not allocated\n", __func__);
		return ENOMEM;
	}

	if (karg->timeout < MPI3MR_IOCTL_DEFAULT_TIMEOUT)
		karg->timeout = MPI3MR_IOCTL_DEFAULT_TIMEOUT;
	
	if (!karg->mpi_msg_size || !karg->buf_entry_list_size) {
		printf(IOCNAME "%s:%d Invalid IOCTL parameters passed\n", sc->name,
		       __func__, __LINE__);
		return rval;
	}
	if ((karg->mpi_msg_size * 4) > MPI3MR_AREQ_FRAME_SZ) {
		printf(IOCNAME "%s:%d Invalid IOCTL parameters passed\n", sc->name,
		       __func__, __LINE__);
		return rval;
	}

	mpi_request = malloc(MPI3MR_AREQ_FRAME_SZ, M_MPI3MR, M_NOWAIT | M_ZERO);
	if (!mpi_request) {
		printf(IOCNAME "%s: memory allocation failed for mpi_request\n", sc->name,
		       __func__);
		return ENOMEM;
	}
	
	mpi_header = (Mpi3RequestHeader_t *)mpi_request;
	pel = (Mpi3PELReqActionGetCount_t *)mpi_request;	
	if (copyin(karg->mpi_msg_buf, mpi_request, (karg->mpi_msg_size * 4))) {
		printf(IOCNAME "failure at %s:%d/%s()!\n", sc->name,
		       __FILE__, __LINE__, __func__);
		rval = EFAULT;
		goto out;
	}

	buffer_list = malloc(karg->buf_entry_list_size, M_MPI3MR, M_NOWAIT | M_ZERO);
	if (!buffer_list) {
		printf(IOCNAME "%s: memory allocation failed for buffer_list\n", sc->name,
		       __func__);
		rval = ENOMEM;
		goto out;
	}
	if (copyin(karg->buf_entry_list, buffer_list, karg->buf_entry_list_size)) {
		printf(IOCNAME "failure at %s:%d/%s()!\n", sc->name,
		       __FILE__, __LINE__, __func__);
		rval = EFAULT;
		goto out;
	}
	if (!buffer_list->num_of_buf_entries) {
		printf(IOCNAME "%s:%d Invalid IOCTL parameters passed\n", sc->name,
		       __func__, __LINE__);
		rval = EINVAL;
		goto out;
	}
	bufcnt = buffer_list->num_of_buf_entries;
	dma_buffers = malloc((sizeof(*dma_buffers) * bufcnt), M_MPI3MR, M_NOWAIT | M_ZERO);
	if (!dma_buffers) {
		printf(IOCNAME "%s: memory allocation failed for dma_buffers\n", sc->name,
		       __func__);
		rval = ENOMEM;
		goto out;
	}
	buf_entries = buffer_list->buf_entry;
	dma_buff = dma_buffers;
	for (count = 0; count < bufcnt; count++, buf_entries++, dma_buff++) {
		memset(dma_buff, 0, sizeof(*dma_buff));
		dma_buff->user_buf = buf_entries->buffer;
		dma_buff->user_buf_len = buf_entries->buf_len;

		switch (buf_entries->buf_type) {
		case MPI3MR_IOCTL_BUFTYPE_RAIDMGMT_CMD:
			is_rmcb = 1;
			if ((count != 0) || !buf_entries->buf_len)
				invalid_be = 1;
			dma_buff->data_dir = MPI3MR_APP_DDO;
			break;
		case MPI3MR_IOCTL_BUFTYPE_RAIDMGMT_RESP:
			is_rmrb = 1;
			if (count != 1 || !is_rmcb || !buf_entries->buf_len)
				invalid_be = 1;
			dma_buff->data_dir = MPI3MR_APP_DDI;
			break;
		case MPI3MR_IOCTL_BUFTYPE_DATA_IN:
			din_sz = dma_buff->user_buf_len;
			din_cnt++;
			if ((din_cnt > 1) && !is_rmcb)
				invalid_be = 1;
			dma_buff->data_dir = MPI3MR_APP_DDI;
			break;
		case MPI3MR_IOCTL_BUFTYPE_DATA_OUT:
			dout_sz = dma_buff->user_buf_len;
			dout_cnt++;
			if ((dout_cnt > 1) && !is_rmcb)
				invalid_be = 1;
			dma_buff->data_dir = MPI3MR_APP_DDO;
			break;
		case MPI3MR_IOCTL_BUFTYPE_MPI_REPLY:
			mpirep_offset = count;
			dma_buff->data_dir = MPI3MR_APP_DDN;
			if (!buf_entries->buf_len)
				invalid_be = 1;
			break;
		case MPI3MR_IOCTL_BUFTYPE_ERR_RESPONSE:
			erb_offset = count;
			dma_buff->data_dir = MPI3MR_APP_DDN;
			if (!buf_entries->buf_len)
				invalid_be = 1;
			break;
		default:
			invalid_be = 1;
			break;
		}
		if (invalid_be)
			break;
	}
	if (invalid_be) {
		printf(IOCNAME "%s:%d Invalid IOCTL parameters passed\n", sc->name,
		       __func__, __LINE__);
		rval = EINVAL;
		goto out;
	}

	if (is_rmcb && ((din_sz + dout_sz) > MPI3MR_MAX_IOCTL_TRANSFER_SIZE)) {
		printf("%s:%d: invalid data transfer size passed for function 0x%x"
		       "din_sz = %d, dout_size = %d\n", __func__, __LINE__,
		       mpi_header->Function, din_sz, dout_sz);
		rval = EINVAL;
		goto out;
	}

 	if ((din_sz > MPI3MR_MAX_IOCTL_TRANSFER_SIZE) ||
	    (dout_sz > MPI3MR_MAX_IOCTL_TRANSFER_SIZE)) {
		printf("%s:%d: invalid data transfer size passed for function 0x%x"
		       "din_size=%d dout_size=%d\n", __func__, __LINE__,
		       mpi_header->Function, din_sz, dout_sz);
		rval = EINVAL;
 		goto out;
 	}
	
	if (mpi_header->Function == MPI3_FUNCTION_SMP_PASSTHROUGH) {
		if ((din_sz > MPI3MR_IOCTL_SGE_SIZE) ||
		    (dout_sz > MPI3MR_IOCTL_SGE_SIZE)) {
			printf("%s:%d: invalid message size passed:%d:%d:%d:%d\n",
			       __func__, __LINE__, din_cnt, dout_cnt, din_sz, dout_sz);
			rval = EINVAL;
			goto out;
		}
	}
	
	dma_buff = dma_buffers;
	for (count = 0; count < bufcnt; count++, dma_buff++) {
		
		dma_buff->kern_buf_len = dma_buff->user_buf_len;
		
		if (is_rmcb && !count) {
			dma_buff->kern_buf = sc->ioctl_chain_sge.addr;
			dma_buff->kern_buf_len = sc->ioctl_chain_sge.size;
			dma_buff->kern_buf_dma = sc->ioctl_chain_sge.dma_addr;
			dma_buff->dma_desc = NULL;
			dma_buff->num_dma_desc = 0;
			memset(dma_buff->kern_buf, 0, dma_buff->kern_buf_len);
			tmplen = min(dma_buff->kern_buf_len, dma_buff->user_buf_len);
			if (copyin(dma_buff->user_buf, dma_buff->kern_buf, tmplen)) {
				mpi3mr_dprint(sc, MPI3MR_ERROR, "failure at %s() line: %d",
					      __func__, __LINE__);
				rval = EFAULT;
				goto out;
			}
		} else if (is_rmrb && (count == 1)) {
			dma_buff->kern_buf = sc->ioctl_resp_sge.addr;
			dma_buff->kern_buf_len = sc->ioctl_resp_sge.size;
			dma_buff->kern_buf_dma = sc->ioctl_resp_sge.dma_addr;
			dma_buff->dma_desc = NULL;
			dma_buff->num_dma_desc = 0;
			memset(dma_buff->kern_buf, 0, dma_buff->kern_buf_len);
			tmplen = min(dma_buff->kern_buf_len, dma_buff->user_buf_len);
			dma_buff->kern_buf_len = tmplen;
		} else {
			if (!dma_buff->kern_buf_len)
				continue;
			if (mpi3mr_map_data_buffer_dma(sc, dma_buff, desc_count)) {
				rval = ENOMEM;
				mpi3mr_dprint(sc, MPI3MR_ERROR, "mapping data buffers failed"
					      "at %s() line: %d\n", __func__, __LINE__);
				goto out;
			}
			desc_count += dma_buff->num_dma_desc;
		}
	}
	
	if (erb_offset != 0xFF) {
		kern_erb = malloc(erbsz, M_MPI3MR, M_NOWAIT | M_ZERO);
		if (!kern_erb) {
			printf(IOCNAME "%s:%d Cannot allocate memory for sense buffer\n", sc->name,
			       __func__, __LINE__);
			rval = ENOMEM;
			goto out;
		}
	}

	if (sc->ioctl_cmds.state & MPI3MR_CMD_PENDING) {
		printf(IOCNAME "Issue IOCTL: Ioctl command is in use/previous command is pending\n",
		       sc->name);
		rval = EAGAIN;
		goto out;
	}
	
	if (sc->unrecoverable) {
		printf(IOCNAME "Issue IOCTL: controller is in unrecoverable state\n", sc->name);
		rval = EFAULT;
		goto out;
	}
	
	if (sc->reset_in_progress) {
		printf(IOCNAME "Issue IOCTL: reset in progress\n", sc->name);
		rval = EAGAIN;
		goto out;
	}
	if (sc->block_ioctls) {
		printf(IOCNAME "Issue IOCTL: IOCTLs are blocked\n", sc->name);
		rval = EAGAIN;
		goto out;
	}
	
	if (mpi_header->Function != MPI3_FUNCTION_NVME_ENCAPSULATED) {
		if (mpi3mr_app_construct_sgl(sc, mpi_request, (karg->mpi_msg_size * 4), dma_buffers,
					     bufcnt, is_rmcb, is_rmrb, (dout_cnt + din_cnt))) {
			printf(IOCNAME "Issue IOCTL: sgl build failed\n", sc->name);
			rval = EAGAIN;
			goto out;
		}

	} else {	
		nvme_fmt = mpi3mr_app_get_nvme_data_fmt(
			   (Mpi3NVMeEncapsulatedRequest_t *)mpi_request);
		if (nvme_fmt == MPI3MR_NVME_DATA_FORMAT_PRP) {
			if (mpi3mr_app_build_nvme_prp(sc,
			    (Mpi3NVMeEncapsulatedRequest_t *) mpi_request,
			    dma_buffers, bufcnt)) {
				rval = ENOMEM;
				goto out;
			}
		} else if (nvme_fmt == MPI3MR_NVME_DATA_FORMAT_SGL1 ||
			   nvme_fmt == MPI3MR_NVME_DATA_FORMAT_SGL2) {
			if (mpi3mr_app_construct_nvme_sgl(sc, (Mpi3NVMeEncapsulatedRequest_t *) mpi_request,
			    dma_buffers, bufcnt)) {
				rval = EINVAL;
				goto out;
			}
		} else {
			printf(IOCNAME "%s: Invalid NVMe Command Format\n", sc->name,
			       __func__);
			rval = EINVAL;
			goto out;
		}
	}

	sc->ioctl_cmds.state = MPI3MR_CMD_PENDING;
	sc->ioctl_cmds.is_waiting = 1;
	sc->ioctl_cmds.callback = NULL;
	sc->ioctl_cmds.is_senseprst = 0;
	sc->ioctl_cmds.sensebuf = kern_erb;
	memset((sc->ioctl_cmds.reply), 0, sc->reply_sz);
	mpi_header->HostTag = MPI3MR_HOSTTAG_IOCTLCMDS;
	init_completion(&sc->ioctl_cmds.completion);
	rval = mpi3mr_submit_admin_cmd(sc, mpi_request, MPI3MR_AREQ_FRAME_SZ);
	if (rval) {
		printf(IOCNAME "Issue IOCTL: Admin Post failed\n", sc->name);
		goto out_failed;
	}
	wait_for_completion_timeout(&sc->ioctl_cmds.completion, karg->timeout);
	
	if (!(sc->ioctl_cmds.state & MPI3MR_CMD_COMPLETE)) {
		sc->ioctl_cmds.is_waiting = 0;
		printf(IOCNAME "Issue IOCTL: command timed out\n", sc->name);
		rval = EAGAIN;
		if (sc->ioctl_cmds.state & MPI3MR_CMD_RESET)
			goto out_failed;

		sc->reset.type = MPI3MR_TRIGGER_SOFT_RESET;
		sc->reset.reason = MPI3MR_RESET_FROM_IOCTL_TIMEOUT;
		goto out_failed;
	}
	
	if (sc->nvme_encap_prp_list && sc->nvme_encap_prp_list_dma) {
		bus_dmamap_unload(sc->nvme_encap_prp_list_dmatag, sc->nvme_encap_prp_list_dma_dmamap);
		bus_dmamem_free(sc->nvme_encap_prp_list_dmatag, sc->nvme_encap_prp_list, sc->nvme_encap_prp_list_dma_dmamap);
		bus_dma_tag_destroy(sc->nvme_encap_prp_list_dmatag);
		sc->nvme_encap_prp_list = NULL;
	}
	
	if (((sc->ioctl_cmds.ioc_status & MPI3_IOCSTATUS_STATUS_MASK)
	    != MPI3_IOCSTATUS_SUCCESS) &&
	    (sc->mpi3mr_debug & MPI3MR_DEBUG_IOCTL)) {
		printf(IOCNAME "Issue IOCTL: Failed IOCStatus(0x%04x) Loginfo(0x%08x)\n", sc->name,
		       (sc->ioctl_cmds.ioc_status & MPI3_IOCSTATUS_STATUS_MASK),
		       sc->ioctl_cmds.ioc_loginfo);
	}

	if ((mpirep_offset != 0xFF) &&
	    dma_buffers[mpirep_offset].user_buf_len) {
		dma_buff = &dma_buffers[mpirep_offset];
		dma_buff->kern_buf_len = (sizeof(*mpirepbuf) - 1 +
					   sc->reply_sz);
		mpirepbuf = malloc(dma_buff->kern_buf_len, M_MPI3MR, M_NOWAIT | M_ZERO);

		if (!mpirepbuf) {
			printf(IOCNAME "%s: failed obtaining a memory for mpi reply\n", sc->name,
			       __func__);
			rval = ENOMEM;
			goto out_failed;
		}
		if (sc->ioctl_cmds.state & MPI3MR_CMD_REPLYVALID) {
			mpirepbuf->mpirep_type =
				MPI3MR_IOCTL_MPI_REPLY_BUFTYPE_ADDRESS;
			memcpy(mpirepbuf->repbuf, sc->ioctl_cmds.reply, sc->reply_sz);
		} else {
			mpirepbuf->mpirep_type =
				MPI3MR_IOCTL_MPI_REPLY_BUFTYPE_STATUS;
			status_desc = (Mpi3StatusReplyDescriptor_t *)
			    mpirepbuf->repbuf;
			status_desc->IOCStatus = sc->ioctl_cmds.ioc_status;
			status_desc->IOCLogInfo = sc->ioctl_cmds.ioc_loginfo;
		}
		tmplen = min(dma_buff->kern_buf_len, dma_buff->user_buf_len);
		if (copyout(mpirepbuf, dma_buff->user_buf, tmplen)) {
			printf(IOCNAME "failure at %s:%d/%s()!\n", sc->name,
			       __FILE__, __LINE__, __func__);
			rval = EFAULT;
			goto out_failed;
		}
	}

	if (erb_offset != 0xFF && sc->ioctl_cmds.sensebuf &&
	    sc->ioctl_cmds.is_senseprst) {
		dma_buff = &dma_buffers[erb_offset];
		tmplen = min(erbsz, dma_buff->user_buf_len);
		if (copyout(kern_erb, dma_buff->user_buf, tmplen)) {
			printf(IOCNAME "failure at %s:%d/%s()!\n", sc->name,
			       __FILE__, __LINE__, __func__);
			rval = EFAULT;
			goto out_failed;
		}
	}

	dma_buff = dma_buffers;
	for (count = 0; count < bufcnt; count++, dma_buff++) {
		if ((count == 1) && is_rmrb) {
			if (copyout(dma_buff->kern_buf, dma_buff->user_buf,dma_buff->kern_buf_len)) {
				printf(IOCNAME "failure at %s:%d/%s()!\n", sc->name,
				       __FILE__, __LINE__, __func__);
				rval = EFAULT;
				goto out_failed;
			}
		} else if (dma_buff->data_dir == MPI3MR_APP_DDI) {
			tmplen = 0;
			for (desc_count = 0; desc_count < dma_buff->num_dma_desc; desc_count++) {
				if (copyout(dma_buff->dma_desc[desc_count].addr,
		                    (U8 *)dma_buff->user_buf+tmplen,
				    dma_buff->dma_desc[desc_count].size)) {
					printf(IOCNAME "failure at %s:%d/%s()!\n", sc->name,
					       __FILE__, __LINE__, __func__);
					rval = EFAULT;
					goto out_failed;
				}
				tmplen += dma_buff->dma_desc[desc_count].size;
			}
		}
	}

	if ((pel->Function == MPI3_FUNCTION_PERSISTENT_EVENT_LOG) &&
	    (pel->Action == MPI3_PEL_ACTION_GET_COUNT))
		sc->mpi3mr_aen_triggered = 0;

out_failed:
	sc->ioctl_cmds.is_senseprst = 0;
	sc->ioctl_cmds.sensebuf = NULL;
	sc->ioctl_cmds.state = MPI3MR_CMD_NOTUSED;
out:
	if (kern_erb)
		free(kern_erb, M_MPI3MR);
	if (buffer_list)
		free(buffer_list, M_MPI3MR);
	if (mpi_request)
		free(mpi_request, M_MPI3MR);
	if (dma_buffers) {
		dma_buff = dma_buffers;
		for (count = 0; count < bufcnt; count++, dma_buff++) {
			free(dma_buff->dma_desc, M_MPI3MR);
		}
		free(dma_buffers, M_MPI3MR);
	}
	if (mpirepbuf)
		free(mpirepbuf, M_MPI3MR);
	return rval;
}

/**
 * mpi3mr_soft_reset_from_app - Trigger controller reset
 * @sc: Adapter instance reference
 *
 * This function triggers the controller reset from the
 * watchdog context and wait for it to complete. It will
 * come out of wait upon completion or timeout exaustion. 
 *
 * Return: 0 on success and proper error codes on failure
 */
static long
mpi3mr_soft_reset_from_app(struct mpi3mr_softc *sc)
{

	U32 timeout;

	/* if reset is not in progress, trigger soft reset from watchdog context */
	if (!sc->reset_in_progress) {
		sc->reset.type = MPI3MR_TRIGGER_SOFT_RESET;
		sc->reset.reason = MPI3MR_RESET_FROM_IOCTL;
		
		/* Wait for soft reset to start */
		timeout = 50;
		while (timeout--) {
			if (sc->reset_in_progress == 1)
				break;
			DELAY(100 * 1000);
		}
		if (!timeout)
			return EFAULT;
	}

	/* Wait for soft reset to complete */
	int i = 0;
	timeout = sc->ready_timeout;
	while (timeout--) {
		if (sc->reset_in_progress == 0)
			break;
		i++;
		if (!(i % 5)) {
			mpi3mr_dprint(sc, MPI3MR_INFO,
			    "[%2ds]waiting for controller reset to be finished from %s\n", i, __func__);
		}
		DELAY(1000 * 1000);
	}

	/* 
	 * In case of soft reset failure or not completed within stipulated time,
	 * fail back to application.
	 */
	if ((!timeout || sc->reset.status))
		return EFAULT;
	
	return 0;
}


/**
 * mpi3mr_adp_reset - Issue controller reset
 * @sc: Adapter instance reference
 * @data_out_buf: User buffer with reset type
 * @data_out_sz: length of the user buffer.
 *
 * This function identifies the user provided reset type and
 * issues approporiate reset to the controller and wait for that
 * to complete and reinitialize the controller and then returns.
 *
 * Return: 0 on success and proper error codes on failure
 */
static long
mpi3mr_adp_reset(struct mpi3mr_softc *sc,
		 void *data_out_buf, U32 data_out_sz)
{
	long rval = EINVAL;
	struct mpi3mr_ioctl_adpreset adpreset;

	memset(&adpreset, 0, sizeof(adpreset));

	if (data_out_sz != sizeof(adpreset)) {
		printf(IOCNAME "Invalid user adpreset buffer size %s() line: %d\n", sc->name,
		       __func__, __LINE__);
		goto out;
	}
	
	if (copyin(data_out_buf, &adpreset, sizeof(adpreset))) {
		printf(IOCNAME "failure at %s() line:%d\n", sc->name,
		       __func__, __LINE__);
		rval = EFAULT;
		goto out;
	}

	switch (adpreset.reset_type) {
	case MPI3MR_IOCTL_ADPRESET_SOFT:
		sc->reset.ioctl_reset_snapdump = false;
		break;
	case MPI3MR_IOCTL_ADPRESET_DIAG_FAULT:
		sc->reset.ioctl_reset_snapdump = true;
		break;
	default:
		printf(IOCNAME "Unknown reset_type(0x%x) issued\n", sc->name,
		       adpreset.reset_type);
		goto out;
	}
	rval = mpi3mr_soft_reset_from_app(sc);
	if (rval)
		printf(IOCNAME "reset handler returned error (0x%lx) for reset type 0x%x\n",
		       sc->name, rval, adpreset.reset_type);

out:
	return rval;
}

void
mpi3mr_app_send_aen(struct mpi3mr_softc *sc)
{
	sc->mpi3mr_aen_triggered = 1;
	if (sc->mpi3mr_poll_waiting) {
		selwakeup(&sc->mpi3mr_select);
		sc->mpi3mr_poll_waiting = 0;
	}
	return;
}

void
mpi3mr_pel_wait_complete(struct mpi3mr_softc *sc,
			 struct mpi3mr_drvr_cmd *drvr_cmd)
{
	U8 retry = 0;
	Mpi3PELReply_t *pel_reply = NULL;
	mpi3mr_dprint(sc, MPI3MR_TRACE, "%s() line: %d\n", __func__, __LINE__);
	
	if (drvr_cmd->state & MPI3MR_CMD_RESET)
		goto cleanup_drvrcmd;
	
	if (!(drvr_cmd->state & MPI3MR_CMD_REPLYVALID)) {
		printf(IOCNAME "%s: PELGetSeqNum Failed, No Reply\n", sc->name, __func__);
		goto out_failed;
	}
	pel_reply = (Mpi3PELReply_t *)drvr_cmd->reply;

	if (((GET_IOC_STATUS(drvr_cmd->ioc_status)) != MPI3_IOCSTATUS_SUCCESS)
	    || ((le16toh(pel_reply->PELogStatus) != MPI3_PEL_STATUS_SUCCESS)
	    && (le16toh(pel_reply->PELogStatus) != MPI3_PEL_STATUS_ABORTED))){
		printf(IOCNAME "%s: PELGetSeqNum Failed, IOCStatus(0x%04x) Loginfo(0x%08x) PEL_LogStatus(0x%04x)\n",
		       sc->name, __func__, GET_IOC_STATUS(drvr_cmd->ioc_status), 
		       drvr_cmd->ioc_loginfo, le16toh(pel_reply->PELogStatus));
		retry = 1;
	}

	if (retry) {
		if (drvr_cmd->retry_count < MPI3MR_PELCMDS_RETRYCOUNT) {
			drvr_cmd->retry_count++;
			printf(IOCNAME "%s : PELWaitretry=%d\n", sc->name,
			       __func__,  drvr_cmd->retry_count);
			mpi3mr_issue_pel_wait(sc, drvr_cmd);
			return;
		}

		printf(IOCNAME "%s :PELWait failed after all retries\n", sc->name,
		    __func__);
		goto out_failed;
	}

	mpi3mr_app_send_aen(sc);
	
	if (!sc->pel_abort_requested) {
		sc->pel_cmds.retry_count = 0;
		mpi3mr_send_pel_getseq(sc, &sc->pel_cmds);
	}

	return;
out_failed:
	sc->pel_wait_pend = 0;
cleanup_drvrcmd:
	drvr_cmd->state = MPI3MR_CMD_NOTUSED;
	drvr_cmd->callback = NULL;
	drvr_cmd->retry_count = 0;
}

void
mpi3mr_issue_pel_wait(struct mpi3mr_softc *sc,
		      struct mpi3mr_drvr_cmd *drvr_cmd)
{
	U8 retry_count = 0;
	Mpi3PELReqActionWait_t pel_wait;
	mpi3mr_dprint(sc, MPI3MR_TRACE, "%s() line: %d\n", __func__, __LINE__);

	sc->pel_abort_requested = 0;

	memset(&pel_wait, 0, sizeof(pel_wait));
	drvr_cmd->state = MPI3MR_CMD_PENDING;
	drvr_cmd->is_waiting = 0;
	drvr_cmd->callback = mpi3mr_pel_wait_complete;
	drvr_cmd->ioc_status = 0;
	drvr_cmd->ioc_loginfo = 0;
	pel_wait.HostTag = htole16(MPI3MR_HOSTTAG_PELWAIT);
	pel_wait.Function = MPI3_FUNCTION_PERSISTENT_EVENT_LOG;
	pel_wait.Action = MPI3_PEL_ACTION_WAIT;
	pel_wait.StartingSequenceNumber = htole32(sc->newest_seqnum);
	pel_wait.Locale = htole16(sc->pel_locale);
	pel_wait.Class = htole16(sc->pel_class);
	pel_wait.WaitTime = MPI3_PEL_WAITTIME_INFINITE_WAIT;
	printf(IOCNAME "Issuing PELWait: seqnum %u class %u locale 0x%08x\n",
	       sc->name, sc->newest_seqnum, sc->pel_class, sc->pel_locale);
retry_pel_wait:
	if (mpi3mr_submit_admin_cmd(sc, &pel_wait, sizeof(pel_wait))) {
		printf(IOCNAME "%s: Issue PELWait IOCTL: Admin Post failed\n", sc->name, __func__);
		if (retry_count < MPI3MR_PELCMDS_RETRYCOUNT) {
			retry_count++;
			goto retry_pel_wait;
		}
		goto out_failed;
	}
	return;
out_failed:
	drvr_cmd->state = MPI3MR_CMD_NOTUSED;
	drvr_cmd->callback = NULL;
	drvr_cmd->retry_count = 0;
	sc->pel_wait_pend = 0;
	return;
}

void 
mpi3mr_send_pel_getseq(struct mpi3mr_softc *sc,
		       struct mpi3mr_drvr_cmd *drvr_cmd)
{
	U8 retry_count = 0;
	U8 sgl_flags = MPI3MR_SGEFLAGS_SYSTEM_SIMPLE_END_OF_LIST;
	Mpi3PELReqActionGetSequenceNumbers_t pel_getseq_req;

	memset(&pel_getseq_req, 0, sizeof(pel_getseq_req));
	sc->pel_cmds.state = MPI3MR_CMD_PENDING;
	sc->pel_cmds.is_waiting = 0;
	sc->pel_cmds.ioc_status = 0;
	sc->pel_cmds.ioc_loginfo = 0;
	sc->pel_cmds.callback = mpi3mr_pel_getseq_complete;
	pel_getseq_req.HostTag = htole16(MPI3MR_HOSTTAG_PELWAIT);
	pel_getseq_req.Function = MPI3_FUNCTION_PERSISTENT_EVENT_LOG;
	pel_getseq_req.Action = MPI3_PEL_ACTION_GET_SEQNUM;
	mpi3mr_add_sg_single(&pel_getseq_req.SGL, sgl_flags,
			     sc->pel_seq_number_sz, sc->pel_seq_number_dma);

retry_pel_getseq:
	if (mpi3mr_submit_admin_cmd(sc, &pel_getseq_req, sizeof(pel_getseq_req))) {
		printf(IOCNAME "%s: Issuing PEL GetSeq IOCTL: Admin Post failed\n", sc->name, __func__);
		if (retry_count < MPI3MR_PELCMDS_RETRYCOUNT) {
			retry_count++;
			goto retry_pel_getseq;
		}
		goto out_failed;
	}
	return;
out_failed:
	drvr_cmd->state = MPI3MR_CMD_NOTUSED;
	drvr_cmd->callback = NULL;
	drvr_cmd->retry_count = 0;
	sc->pel_wait_pend = 0;
}

void
mpi3mr_pel_getseq_complete(struct mpi3mr_softc *sc,
			   struct mpi3mr_drvr_cmd *drvr_cmd)
{
	U8 retry = 0;
	Mpi3PELReply_t *pel_reply = NULL;
	Mpi3PELSeq_t *pel_seq_num = (Mpi3PELSeq_t *)sc->pel_seq_number;
	mpi3mr_dprint(sc, MPI3MR_TRACE, "%s() line: %d\n", __func__, __LINE__);

	if (drvr_cmd->state & MPI3MR_CMD_RESET)
		goto cleanup_drvrcmd;
	
	if (!(drvr_cmd->state & MPI3MR_CMD_REPLYVALID)) {
		printf(IOCNAME "%s: PELGetSeqNum Failed, No Reply\n", sc->name, __func__);
		goto out_failed;
	}
	pel_reply = (Mpi3PELReply_t *)drvr_cmd->reply;

	if (((GET_IOC_STATUS(drvr_cmd->ioc_status)) != MPI3_IOCSTATUS_SUCCESS)
	    || (le16toh(pel_reply->PELogStatus) != MPI3_PEL_STATUS_SUCCESS)){
		printf(IOCNAME "%s: PELGetSeqNum Failed, IOCStatus(0x%04x) Loginfo(0x%08x) PEL_LogStatus(0x%04x)\n",
		       sc->name, __func__, GET_IOC_STATUS(drvr_cmd->ioc_status), 
		       drvr_cmd->ioc_loginfo, le16toh(pel_reply->PELogStatus));
		retry = 1;
	}
		
	if (retry) {
		if (drvr_cmd->retry_count < MPI3MR_PELCMDS_RETRYCOUNT) {
			drvr_cmd->retry_count++;
			printf(IOCNAME "%s : PELGetSeqNUM retry=%d\n", sc->name, 
			       __func__,  drvr_cmd->retry_count);
			mpi3mr_send_pel_getseq(sc, drvr_cmd);
			return;
		}
		printf(IOCNAME "%s :PELGetSeqNUM failed after all retries\n",
		       sc->name, __func__);
		goto out_failed;
	}

	sc->newest_seqnum = le32toh(pel_seq_num->Newest) + 1;
	drvr_cmd->retry_count = 0;
	mpi3mr_issue_pel_wait(sc, drvr_cmd);
	return;
out_failed:
	sc->pel_wait_pend = 0;
cleanup_drvrcmd:
	drvr_cmd->state = MPI3MR_CMD_NOTUSED;
	drvr_cmd->callback = NULL;
	drvr_cmd->retry_count = 0;
}

static int
mpi3mr_pel_getseq(struct mpi3mr_softc *sc)
{
	int rval = 0;
	U8 sgl_flags = 0;
	Mpi3PELReqActionGetSequenceNumbers_t pel_getseq_req;
	mpi3mr_dprint(sc, MPI3MR_TRACE, "%s() line: %d\n", __func__, __LINE__);
	
	if (sc->reset_in_progress || sc->block_ioctls) {
		printf(IOCNAME "%s: IOCTL failed: reset in progress: %u ioctls blocked: %u\n",
		       sc->name, __func__, sc->reset_in_progress, sc->block_ioctls);
		return -1;
	}
	
	memset(&pel_getseq_req, 0, sizeof(pel_getseq_req));
	sgl_flags = MPI3MR_SGEFLAGS_SYSTEM_SIMPLE_END_OF_LIST;
	sc->pel_cmds.state = MPI3MR_CMD_PENDING;
	sc->pel_cmds.is_waiting = 0;
	sc->pel_cmds.retry_count = 0;
	sc->pel_cmds.ioc_status = 0;
	sc->pel_cmds.ioc_loginfo = 0;
	sc->pel_cmds.callback = mpi3mr_pel_getseq_complete;
	pel_getseq_req.HostTag = htole16(MPI3MR_HOSTTAG_PELWAIT);
	pel_getseq_req.Function = MPI3_FUNCTION_PERSISTENT_EVENT_LOG;
	pel_getseq_req.Action = MPI3_PEL_ACTION_GET_SEQNUM;
	mpi3mr_add_sg_single(&pel_getseq_req.SGL, sgl_flags,
			     sc->pel_seq_number_sz, sc->pel_seq_number_dma);

	if ((rval = mpi3mr_submit_admin_cmd(sc, &pel_getseq_req, sizeof(pel_getseq_req))))
		printf(IOCNAME "%s: Issue IOCTL: Admin Post failed\n", sc->name, __func__);

	return rval;
}

int
mpi3mr_pel_abort(struct mpi3mr_softc *sc)
{
	int retval = 0;
	U16 pel_log_status;
	Mpi3PELReqActionAbort_t pel_abort_req;
	Mpi3PELReply_t *pel_reply = NULL;

	if (sc->reset_in_progress || sc->block_ioctls) {
		printf(IOCNAME "%s: IOCTL failed: reset in progress: %u ioctls blocked: %u\n",
		       sc->name, __func__, sc->reset_in_progress, sc->block_ioctls);
		return -1;
	}

	memset(&pel_abort_req, 0, sizeof(pel_abort_req));

	mtx_lock(&sc->pel_abort_cmd.completion.lock);
	if (sc->pel_abort_cmd.state & MPI3MR_CMD_PENDING) {
		printf(IOCNAME "%s: PEL Abort command is in use\n", sc->name,  __func__);
		mtx_unlock(&sc->pel_abort_cmd.completion.lock);
		return -1;
	}

	sc->pel_abort_cmd.state = MPI3MR_CMD_PENDING;
	sc->pel_abort_cmd.is_waiting = 1;
	sc->pel_abort_cmd.callback = NULL;
	pel_abort_req.HostTag = htole16(MPI3MR_HOSTTAG_PELABORT);
	pel_abort_req.Function = MPI3_FUNCTION_PERSISTENT_EVENT_LOG;
	pel_abort_req.Action = MPI3_PEL_ACTION_ABORT;
	pel_abort_req.AbortHostTag = htole16(MPI3MR_HOSTTAG_PELWAIT);

	sc->pel_abort_requested = 1;

	init_completion(&sc->pel_abort_cmd.completion);
	retval = mpi3mr_submit_admin_cmd(sc, &pel_abort_req, sizeof(pel_abort_req));
	if (retval) {
		printf(IOCNAME "%s: Issue IOCTL: Admin Post failed\n", sc->name, __func__);
		sc->pel_abort_requested = 0;
		retval = -1;
		goto out_unlock;
	}
	wait_for_completion_timeout(&sc->pel_abort_cmd.completion, MPI3MR_INTADMCMD_TIMEOUT);

	if (!(sc->pel_abort_cmd.state & MPI3MR_CMD_COMPLETE)) {
		printf(IOCNAME "%s: PEL Abort command timedout\n",sc->name,  __func__);
		sc->pel_abort_cmd.is_waiting = 0;
		retval = -1;
		sc->reset.type = MPI3MR_TRIGGER_SOFT_RESET;
		sc->reset.reason = MPI3MR_RESET_FROM_PELABORT_TIMEOUT;
		goto out_unlock;
	}
	if (((GET_IOC_STATUS(sc->pel_abort_cmd.ioc_status)) != MPI3_IOCSTATUS_SUCCESS)
	    || (!(sc->pel_abort_cmd.state & MPI3MR_CMD_REPLYVALID))) {
		printf(IOCNAME "%s: PEL Abort command failed, ioc_status(0x%04x) log_info(0x%08x)\n",
		       sc->name, __func__, GET_IOC_STATUS(sc->pel_abort_cmd.ioc_status),
		       sc->pel_abort_cmd.ioc_loginfo);
		retval = -1;
		goto out_unlock;
	}
	
	pel_reply = (Mpi3PELReply_t *)sc->pel_abort_cmd.reply;
	pel_log_status = le16toh(pel_reply->PELogStatus);
	if (pel_log_status != MPI3_PEL_STATUS_SUCCESS) {
		printf(IOCNAME "%s: PEL abort command failed, pel_status(0x%04x)\n",
		       sc->name, __func__, pel_log_status);
		retval = -1;
	}

out_unlock:
	mtx_unlock(&sc->pel_abort_cmd.completion.lock);
	sc->pel_abort_cmd.state = MPI3MR_CMD_NOTUSED;
	return retval;
}

/**
 * mpi3mr_pel_enable - Handler for PEL enable
 * @sc: Adapter instance reference
 * @data_out_buf: User buffer containing PEL enable data
 * @data_out_sz: length of the user buffer.
 *
 * This function is the handler for PEL enable driver IOCTL.
 * Validates the application given class and locale and if
 * requires aborts the existing PEL wait request and/or issues
 * new PEL wait request to the firmware and returns.
 *
 * Return: 0 on success and proper error codes on failure.
 */
static long
mpi3mr_pel_enable(struct mpi3mr_softc *sc,
		  void *data_out_buf, U32 data_out_sz)
{
	long rval = EINVAL;
	U8 tmp_class;
	U16 tmp_locale;
	struct mpi3mr_ioctl_pel_enable pel_enable;
	mpi3mr_dprint(sc, MPI3MR_TRACE, "%s() line: %d\n", __func__, __LINE__);


	if ((data_out_sz != sizeof(pel_enable) || 
	    (pel_enable.pel_class > MPI3_PEL_CLASS_FAULT))) {
		printf(IOCNAME "%s: Invalid user pel_enable buffer size %u\n",
		       sc->name, __func__, data_out_sz);
		goto out;
	}
	memset(&pel_enable, 0, sizeof(pel_enable));
	if (copyin(data_out_buf, &pel_enable, sizeof(pel_enable))) {
		printf(IOCNAME "failure at %s() line:%d\n", sc->name,
		       __func__, __LINE__);
		rval = EFAULT;
		goto out;
	}
	if (pel_enable.pel_class > MPI3_PEL_CLASS_FAULT) {
		printf(IOCNAME "%s: out of range  class %d\n",
		       sc->name, __func__, pel_enable.pel_class);
		goto out;
	}
	
	if (sc->pel_wait_pend) {
		if ((sc->pel_class <= pel_enable.pel_class) &&
		    !((sc->pel_locale & pel_enable.pel_locale) ^
		      pel_enable.pel_locale)) {
			rval = 0;
			goto out;
		} else {
			pel_enable.pel_locale |= sc->pel_locale;
			if (sc->pel_class < pel_enable.pel_class)
				pel_enable.pel_class = sc->pel_class;

			if (mpi3mr_pel_abort(sc)) {
				printf(IOCNAME "%s: pel_abort failed, status(%ld)\n",
				       sc->name, __func__, rval);
				goto out;
			}
		}
	}

	tmp_class = sc->pel_class;
	tmp_locale = sc->pel_locale;
	sc->pel_class = pel_enable.pel_class;
	sc->pel_locale = pel_enable.pel_locale;
	sc->pel_wait_pend = 1;

	if ((rval = mpi3mr_pel_getseq(sc))) {
		sc->pel_class = tmp_class;
		sc->pel_locale = tmp_locale;
		sc->pel_wait_pend = 0;
		printf(IOCNAME "%s: pel get sequence number failed, status(%ld)\n",
		       sc->name, __func__, rval);
	}

out:
	return rval;
}

void
mpi3mr_app_save_logdata(struct mpi3mr_softc *sc, char *event_data,
			U16 event_data_size)
{
	struct mpi3mr_log_data_entry *entry;
	U32 index = sc->log_data_buffer_index, sz;

	if (!(sc->log_data_buffer))
		return;

	entry = (struct mpi3mr_log_data_entry *)
		(sc->log_data_buffer + (index * sc->log_data_entry_size));
	entry->valid_entry = 1;
	sz = min(sc->log_data_entry_size, event_data_size);
	memcpy(entry->data, event_data, sz);
	sc->log_data_buffer_index =
		((++index) % MPI3MR_IOCTL_LOGDATA_MAX_ENTRIES);
	mpi3mr_app_send_aen(sc);
}

/**
 * mpi3mr_get_logdata - Handler for get log data
 * @sc: Adapter instance reference
 * @data_in_buf: User buffer to copy the logdata entries
 * @data_in_sz: length of the user buffer.
 *
 * This function copies the log data entries to the user buffer
 * when log caching is enabled in the driver.
 *
 * Return: 0 on success and proper error codes on failure
 */
static long
mpi3mr_get_logdata(struct mpi3mr_softc *sc,
		   void *data_in_buf, U32 data_in_sz)
{
	long rval = EINVAL;
	U16 num_entries = 0;
	U16 entry_sz = sc->log_data_entry_size;

	if ((!sc->log_data_buffer) || (data_in_sz < entry_sz))
		return rval;

	num_entries = data_in_sz / entry_sz;
	if (num_entries > MPI3MR_IOCTL_LOGDATA_MAX_ENTRIES)
		num_entries = MPI3MR_IOCTL_LOGDATA_MAX_ENTRIES;

        if ((rval = copyout(sc->log_data_buffer, data_in_buf, (num_entries * entry_sz)))) {
		printf(IOCNAME "%s: copy to user failed\n", sc->name, __func__);
		rval = EFAULT;
	}

	return rval;
}

/**
 * mpi3mr_logdata_enable - Handler for log data enable
 * @sc: Adapter instance reference
 * @data_in_buf: User buffer to copy the max logdata entry count
 * @data_in_sz: length of the user buffer.
 *
 * This function enables log data caching in the driver if not
 * already enabled and return the maximum number of log data
 * entries that can be cached in the driver.
 *
 * Return: 0 on success and proper error codes on failure
 */
static long
mpi3mr_logdata_enable(struct mpi3mr_softc *sc,
		      void *data_in_buf, U32 data_in_sz)
{
	long rval = EINVAL;
	struct mpi3mr_ioctl_logdata_enable logdata_enable;
	
	if (data_in_sz < sizeof(logdata_enable))
		return rval;

	if (sc->log_data_buffer)
		goto copy_data;

	sc->log_data_entry_size = (sc->reply_sz - (sizeof(Mpi3EventNotificationReply_t) - 4))
				   + MPI3MR_IOCTL_LOGDATA_ENTRY_HEADER_SZ;

	sc->log_data_buffer = malloc((MPI3MR_IOCTL_LOGDATA_MAX_ENTRIES * sc->log_data_entry_size),
				     M_MPI3MR, M_NOWAIT | M_ZERO);
	if (!sc->log_data_buffer) {
		printf(IOCNAME "%s log data buffer memory allocation failed\n", sc->name, __func__);
		return ENOMEM;
	}
	
	sc->log_data_buffer_index = 0;

copy_data:
	memset(&logdata_enable, 0, sizeof(logdata_enable));
	logdata_enable.max_entries = MPI3MR_IOCTL_LOGDATA_MAX_ENTRIES;

        if ((rval = copyout(&logdata_enable, data_in_buf, sizeof(logdata_enable)))) {
		printf(IOCNAME "%s: copy to user failed\n", sc->name, __func__);
		rval = EFAULT;
	}

	return rval;
}

/**
 * mpi3mr_get_change_count - Get topology change count
 * @sc: Adapter instance reference
 * @data_in_buf: User buffer to copy the change count
 * @data_in_sz: length of the user buffer.
 *
 * This function copies the toplogy change count provided by the
 * driver in events and cached in the driver to the user
 * provided buffer for the specific controller.
 *
 * Return: 0 on success and proper error codes on failure
 */
static long 
mpi3mr_get_change_count(struct mpi3mr_softc *sc,
			void *data_in_buf, U32 data_in_sz)
{
        long rval = EINVAL;
        struct mpi3mr_ioctl_chgcnt chg_count;
        memset(&chg_count, 0, sizeof(chg_count));

        chg_count.change_count = sc->change_count;
        if (data_in_sz >= sizeof(chg_count)) {
                if ((rval = copyout(&chg_count, data_in_buf, sizeof(chg_count)))) {
                        printf(IOCNAME "failure at %s:%d/%s()!\n", sc->name, __FILE__,
			       __LINE__, __func__);
                        rval = EFAULT;
                }
        }
        return rval;
}

/**
 * mpi3mr_get_alltgtinfo - Get all targets information
 * @sc: Adapter instance reference
 * @data_in_buf: User buffer to copy the target information
 * @data_in_sz: length of the user buffer.
 *
 * This function copies the driver managed target devices device
 * handle, persistent ID, bus ID and taret ID to the user
 * provided buffer for the specific controller. This function
 * also provides the number of devices managed by the driver for
 * the specific controller.
 *
 * Return: 0 on success and proper error codes on failure
 */
static long 
mpi3mr_get_alltgtinfo(struct mpi3mr_softc *sc,
		      void *data_in_buf, U32 data_in_sz)
{
	long rval = EINVAL;
        U8 get_count = 0;
	U16 i = 0, num_devices = 0;
        U32 min_entrylen = 0, kern_entrylen = 0, user_entrylen = 0;
	struct mpi3mr_target *tgtdev = NULL;
        struct mpi3mr_device_map_info *devmap_info = NULL;
	struct mpi3mr_cam_softc *cam_sc = sc->cam_sc;
        struct mpi3mr_ioctl_all_tgtinfo *all_tgtinfo = (struct mpi3mr_ioctl_all_tgtinfo *)data_in_buf;

        if (data_in_sz < sizeof(uint32_t)) {
                printf(IOCNAME "failure at %s:%d/%s()!\n", sc->name, __FILE__,
		       __LINE__, __func__);
                goto out;
        }
        if (data_in_sz == sizeof(uint32_t))
                get_count = 1;
	
	if (TAILQ_EMPTY(&cam_sc->tgt_list)) {
                get_count = 1;
                goto copy_usrbuf;
	}

	mtx_lock_spin(&cam_sc->sc->target_lock);
	TAILQ_FOREACH(tgtdev, &cam_sc->tgt_list, tgt_next) {
		num_devices++;
	}
	mtx_unlock_spin(&cam_sc->sc->target_lock);

        if (get_count)
                goto copy_usrbuf;

        kern_entrylen = num_devices * sizeof(*devmap_info);
	
	devmap_info = malloc(kern_entrylen, M_MPI3MR, M_NOWAIT | M_ZERO);
        if (!devmap_info) {
                printf(IOCNAME "failure at %s:%d/%s()!\n", sc->name, __FILE__,
		       __LINE__, __func__);
                rval = ENOMEM;
                goto out;
        }
        memset((U8*)devmap_info, 0xFF, kern_entrylen);

	mtx_lock_spin(&cam_sc->sc->target_lock);
	TAILQ_FOREACH(tgtdev, &cam_sc->tgt_list, tgt_next) {
                if (i < num_devices) {
                        devmap_info[i].handle = tgtdev->dev_handle;
                        devmap_info[i].per_id = tgtdev->per_id;
			/*n
			 *  For hidden/ugood device the target_id and bus_id should be 0xFFFFFFFF and 0xFF
			 */
			if (!tgtdev->exposed_to_os) {
                                devmap_info[i].target_id = 0xFFFFFFFF;
                                devmap_info[i].bus_id = 0xFF;
                        } else {
                                devmap_info[i].target_id = tgtdev->tid;
                                devmap_info[i].bus_id = 0;
			}
                        i++;
                }
        }
        num_devices = i;
	mtx_unlock_spin(&cam_sc->sc->target_lock);

copy_usrbuf:
        if (copyout(&num_devices, &all_tgtinfo->num_devices, sizeof(num_devices))) {
                printf(IOCNAME "failure at %s:%d/%s()!\n", sc->name, __FILE__,
		       __LINE__, __func__);
                rval = EFAULT;
                goto out;
        }
        user_entrylen = (data_in_sz - sizeof(uint32_t))/sizeof(*devmap_info);
        user_entrylen *= sizeof(*devmap_info);
        min_entrylen = min(user_entrylen, kern_entrylen);
        if (min_entrylen && (copyout(devmap_info, &all_tgtinfo->dmi, min_entrylen))) {
                printf(IOCNAME "failure at %s:%d/%s()!\n", sc->name,
		       __FILE__, __LINE__, __func__);
                rval = EFAULT;
                goto out;
        }
	rval = 0;
out:
        if (devmap_info)
                free(devmap_info, M_MPI3MR);

        return rval;
}

/**
 * mpi3mr_get_tgtinfo - Get specific target information
 * @sc: Adapter instance reference
 * @karg: driver ponter to users payload buffer
 *
 * This function copies the driver managed specific target device
 * info like handle, persistent ID, bus ID and taret ID to the user
 * provided buffer for the specific controller.
 *
 * Return: 0 on success and proper error codes on failure
 */
static long 
mpi3mr_get_tgtinfo(struct mpi3mr_softc *sc,
		   struct mpi3mr_ioctl_drvcmd *karg)
{
	long rval = EINVAL;
	struct mpi3mr_target *tgtdev = NULL;
	struct mpi3mr_ioctl_tgtinfo tgtinfo;
	
	memset(&tgtinfo, 0, sizeof(tgtinfo));

	if ((karg->data_out_size != sizeof(struct mpi3mr_ioctl_tgtinfo)) ||
	    (karg->data_in_size != sizeof(struct mpi3mr_ioctl_tgtinfo))) {
		printf(IOCNAME "Invalid user tgtinfo buffer size %s() line: %d\n", sc->name,
		       __func__, __LINE__);
		goto out;
	}
	
	if (copyin(karg->data_out_buf, &tgtinfo, sizeof(tgtinfo))) {
		printf(IOCNAME "failure at %s() line:%d\n", sc->name,
		       __func__, __LINE__);
		rval = EFAULT;
		goto out;
	}

	if ((tgtinfo.bus_id != 0xFF) && (tgtinfo.target_id != 0xFFFFFFFF)) {
		if ((tgtinfo.persistent_id != 0xFFFF) ||
		    (tgtinfo.dev_handle != 0xFFFF))
			goto out;
		tgtdev = mpi3mr_find_target_by_per_id(sc->cam_sc, tgtinfo.target_id);
	} else if (tgtinfo.persistent_id != 0xFFFF) {
		if ((tgtinfo.bus_id != 0xFF) ||
		    (tgtinfo.dev_handle !=0xFFFF) ||
		    (tgtinfo.target_id != 0xFFFFFFFF))
			goto out;
		tgtdev = mpi3mr_find_target_by_per_id(sc->cam_sc, tgtinfo.persistent_id);
	} else if (tgtinfo.dev_handle !=0xFFFF) {
		if ((tgtinfo.bus_id != 0xFF) ||
		    (tgtinfo.target_id != 0xFFFFFFFF) ||
		    (tgtinfo.persistent_id != 0xFFFF))
			goto out;
		tgtdev = mpi3mr_find_target_by_dev_handle(sc->cam_sc, tgtinfo.dev_handle);
	}
	if (!tgtdev)
		goto out;
	
	tgtinfo.target_id = tgtdev->per_id;
	tgtinfo.bus_id = 0;
	tgtinfo.dev_handle = tgtdev->dev_handle;
	tgtinfo.persistent_id = tgtdev->per_id;
	tgtinfo.seq_num = 0;

	if (copyout(&tgtinfo, karg->data_in_buf, sizeof(tgtinfo))) {
		printf(IOCNAME "failure at %s() line:%d\n", sc->name,
		       __func__, __LINE__);
		rval = EFAULT;
	}

out:
	return rval;
}

/**
 * mpi3mr_get_pciinfo - Get PCI info IOCTL handler
 * @sc: Adapter instance reference
 * @data_in_buf: User buffer to hold adapter information
 * @data_in_sz: length of the user buffer.
 *
 * This function provides the PCI spec information for the
 * given controller
 *
 * Return: 0 on success and proper error codes on failure
 */
static long
mpi3mr_get_pciinfo(struct mpi3mr_softc *sc,
		   void *data_in_buf, U32 data_in_sz)
{
	long rval = EINVAL;
	U8 i;
	struct mpi3mr_ioctl_pciinfo pciinfo;
	memset(&pciinfo, 0, sizeof(pciinfo));
	
	for (i = 0; i < 64; i++)
		pciinfo.config_space[i] = pci_read_config(sc->mpi3mr_dev, (i * 4), 4);

	if (data_in_sz >= sizeof(pciinfo)) {
		if ((rval = copyout(&pciinfo, data_in_buf, sizeof(pciinfo)))) {
			printf(IOCNAME "failure at %s:%d/%s()!\n", sc->name,
			       __FILE__, __LINE__, __func__);
			rval = EFAULT;
		}
	}
	return rval;
}

/**
 * mpi3mr_get_adpinfo - Get adapter info IOCTL handler
 * @sc: Adapter instance reference
 * @data_in_buf: User buffer to hold adapter information
 * @data_in_sz: length of the user buffer.
 *
 * This function provides adapter information for the given
 * controller
 *
 * Return: 0 on success and proper error codes on failure
 */
static long
mpi3mr_get_adpinfo(struct mpi3mr_softc *sc,
		   void *data_in_buf, U32 data_in_sz)
{
	long rval = EINVAL;
	struct mpi3mr_ioctl_adpinfo adpinfo;
	enum mpi3mr_iocstate ioc_state;
	memset(&adpinfo, 0, sizeof(adpinfo));

	adpinfo.adp_type = MPI3MR_IOCTL_ADPTYPE_AVGFAMILY;
	adpinfo.pci_dev_id = pci_get_device(sc->mpi3mr_dev);
	adpinfo.pci_dev_hw_rev = pci_read_config(sc->mpi3mr_dev, PCIR_REVID, 1);
	adpinfo.pci_subsys_dev_id = pci_get_subdevice(sc->mpi3mr_dev);
	adpinfo.pci_subsys_ven_id = pci_get_subvendor(sc->mpi3mr_dev);
	adpinfo.pci_bus = pci_get_bus(sc->mpi3mr_dev);;
	adpinfo.pci_dev = pci_get_slot(sc->mpi3mr_dev);
	adpinfo.pci_func = pci_get_function(sc->mpi3mr_dev);
	adpinfo.pci_seg_id = pci_get_domain(sc->mpi3mr_dev);
	adpinfo.ioctl_ver = MPI3MR_IOCTL_VERSION;
	memcpy((U8 *)&adpinfo.driver_info, (U8 *)&sc->driver_info, sizeof(adpinfo.driver_info));

	ioc_state = mpi3mr_get_iocstate(sc);

	if (ioc_state == MRIOC_STATE_UNRECOVERABLE)
		adpinfo.adp_state = MPI3MR_IOCTL_ADP_STATE_UNRECOVERABLE;
	else if (sc->reset_in_progress || sc->block_ioctls)
		adpinfo.adp_state = MPI3MR_IOCTL_ADP_STATE_IN_RESET;
	else if (ioc_state == MRIOC_STATE_FAULT)
		adpinfo.adp_state = MPI3MR_IOCTL_ADP_STATE_FAULT;
	else	
		adpinfo.adp_state = MPI3MR_IOCTL_ADP_STATE_OPERATIONAL;

	if (data_in_sz >= sizeof(adpinfo)) {
		if ((rval = copyout(&adpinfo, data_in_buf, sizeof(adpinfo)))) {
			printf(IOCNAME "failure at %s:%d/%s()!\n", sc->name,
			       __FILE__, __LINE__, __func__);
			rval = EFAULT;
		}
	}
	return rval;
}
/**
 * mpi3mr_app_drvrcmds - Driver IOCTL handler
 * @dev: char device
 * @cmd: IOCTL command
 * @arg: User data payload buffer for the IOCTL
 * @flag: flags
 * @thread: threads
 *
 * This function is the top level handler for driver commands,
 * this does basic validation of the buffer and identifies the
 * opcode and switches to correct sub handler.
 *
 * Return: 0 on success and proper error codes on failure
 */

static int 
mpi3mr_app_drvrcmds(struct cdev *dev, u_long cmd,
		    void *uarg, int flag, struct thread *td)
{
	long rval = EINVAL;
	struct mpi3mr_softc *sc = NULL;
	struct mpi3mr_ioctl_drvcmd *karg = (struct mpi3mr_ioctl_drvcmd *)uarg;

	sc = mpi3mr_app_get_adp_instance(karg->mrioc_id);
	if (!sc)
		return ENODEV;
	
	mtx_lock(&sc->ioctl_cmds.completion.lock);
	switch (karg->opcode) {
	case MPI3MR_DRVRIOCTL_OPCODE_ADPINFO:
		rval = mpi3mr_get_adpinfo(sc, karg->data_in_buf, karg->data_in_size);
		break;
	case MPI3MR_DRVRIOCTL_OPCODE_GETPCIINFO:
		rval = mpi3mr_get_pciinfo(sc, karg->data_in_buf, karg->data_in_size);
		break;
	case MPI3MR_DRVRIOCTL_OPCODE_TGTDEVINFO:
		rval = mpi3mr_get_tgtinfo(sc, karg);
		break;
	case MPI3MR_DRVRIOCTL_OPCODE_ALLTGTDEVINFO:
                rval = mpi3mr_get_alltgtinfo(sc, karg->data_in_buf, karg->data_in_size);
                break;
        case MPI3MR_DRVRIOCTL_OPCODE_GETCHGCNT:
                rval = mpi3mr_get_change_count(sc, karg->data_in_buf, karg->data_in_size);
                break;
	case MPI3MR_DRVRIOCTL_OPCODE_LOGDATAENABLE:
		rval = mpi3mr_logdata_enable(sc, karg->data_in_buf, karg->data_in_size);
		break;
	case MPI3MR_DRVRIOCTL_OPCODE_GETLOGDATA:
		rval = mpi3mr_get_logdata(sc, karg->data_in_buf, karg->data_in_size);
		break;
	case MPI3MR_DRVRIOCTL_OPCODE_PELENABLE:
		rval = mpi3mr_pel_enable(sc, karg->data_out_buf, karg->data_out_size);
		break;
	case MPI3MR_DRVRIOCTL_OPCODE_ADPRESET:
		rval = mpi3mr_adp_reset(sc, karg->data_out_buf, karg->data_out_size);
		break;
	case MPI3MR_DRVRIOCTL_OPCODE_UNKNOWN:
	default:
		printf("Unsupported drvr ioctl opcode 0x%x\n", karg->opcode);
		break;
	}
	mtx_unlock(&sc->ioctl_cmds.completion.lock);
	return rval;
}
/**
 * mpi3mr_ioctl - IOCTL Handler
 * @dev: char device
 * @cmd: IOCTL command
 * @arg: User data payload buffer for the IOCTL
 * @flag: flags
 * @thread: threads
 *
 * This is the IOCTL entry point which checks the command type and
 * executes proper sub handler specific for the command.
 *
 * Return: 0 on success and proper error codes on failure
 */
static int 
mpi3mr_ioctl(struct cdev *dev, u_long cmd, caddr_t arg, int flag, struct thread *td)
{
	int rval = EINVAL;

	struct mpi3mr_softc *sc = NULL;
	struct mpi3mr_ioctl_drvcmd *karg = (struct mpi3mr_ioctl_drvcmd *)arg;

	sc = mpi3mr_app_get_adp_instance(karg->mrioc_id);
	
	if (!sc)
		return ENODEV;
	
	mpi3mr_atomic_inc(&sc->pend_ioctls);

	
	if (sc->mpi3mr_flags & MPI3MR_FLAGS_SHUTDOWN) {
		mpi3mr_dprint(sc, MPI3MR_INFO,
			"Return back IOCTL, shutdown is in progress\n");
		mpi3mr_atomic_dec(&sc->pend_ioctls);
		return ENODEV;
	}

	switch (cmd) {
	case MPI3MRDRVCMD:
		rval = mpi3mr_app_drvrcmds(dev, cmd, arg, flag, td);
		break;
	case MPI3MRMPTCMD:
		mtx_lock(&sc->ioctl_cmds.completion.lock);
		rval = mpi3mr_app_mptcmds(dev, cmd, arg, flag, td);
		mtx_unlock(&sc->ioctl_cmds.completion.lock);
		break;
	default:
		printf("%s:Unsupported ioctl cmd (0x%08lx)\n", MPI3MR_DRIVER_NAME, cmd);
		break;
	}

	mpi3mr_atomic_dec(&sc->pend_ioctls);

	return rval;
}
