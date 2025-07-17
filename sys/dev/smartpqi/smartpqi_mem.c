/*-
 * Copyright 2016-2023 Microchip Technology, Inc. and/or its subsidiaries.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


#include "smartpqi_includes.h"

MALLOC_DEFINE(M_SMARTPQI, "smartpqi", "Buffers for the smartpqi driver");

/*
 * DMA map load callback function
 */
static void
os_dma_map(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	bus_addr_t *paddr = (bus_addr_t *)arg;
	*paddr = segs[0].ds_addr;
}

int
os_dma_setup(pqisrc_softstate_t *softs)
{
	DBG_FUNC("IN\n");
	DBG_FUNC("OUT\n");
	return PQI_STATUS_SUCCESS;
}

int
os_dma_destroy(pqisrc_softstate_t *softs)
{
	DBG_FUNC("IN\n");
	DBG_FUNC("OUT\n");
	return PQI_STATUS_SUCCESS;
}

void
os_update_dma_attributes(pqisrc_softstate_t *softs)
{
	DBG_FUNC("IN\n");
	DBG_FUNC("OUT\n");
}

/*
 * DMA mem resource allocation wrapper function
 */
int
os_dma_mem_alloc(pqisrc_softstate_t *softs, struct dma_mem *dma_mem)
{
	int ret = BSD_SUCCESS;

	/* DBG_FUNC("IN\n"); */

	/* Make sure the alignment is at least 4 bytes */
	ASSERT(dma_mem->align >= 4);

	/* DMA memory needed - allocate it */
	if ((ret = bus_dma_tag_create(
		softs->os_specific.pqi_parent_dmat, /* parent */
		dma_mem->align, 0,	/* algnmnt, boundary */
		BUS_SPACE_MAXADDR,      /* lowaddr */
		BUS_SPACE_MAXADDR, 	/* highaddr */
		NULL, NULL, 		/* filter, filterarg */
		dma_mem->size, 		/* maxsize */
		1,			/* nsegments */
		dma_mem->size,		/* maxsegsize */
		0,			/* flags */
		NULL, NULL,		/* No locking needed */
		&dma_mem->dma_tag)) != 0 ) {
	        DBG_ERR("can't allocate DMA tag with error = 0x%x\n", ret);
		goto err_out;
	}

	if (!dma_mem->dma_tag) {
	        DBG_ERR("dma tag is NULL\n");
		ret = ENOMEM;
		goto err_out;
	}

	if ((ret = bus_dmamem_alloc(dma_mem->dma_tag, (void **)&dma_mem->virt_addr,
		BUS_DMA_NOWAIT, &dma_mem->dma_map)) != 0) {
		DBG_ERR("can't allocate DMA memory for required object \
				with error = 0x%x\n", ret);
		goto err_mem;
	}

	if((ret = bus_dmamap_load(dma_mem->dma_tag, dma_mem->dma_map,
		dma_mem->virt_addr, dma_mem->size,
		os_dma_map, &dma_mem->dma_addr, 0)) != 0) {
		DBG_ERR("can't load DMA memory for required \
			object with error = 0x%x\n", ret);
		goto err_load;
	}

	memset(dma_mem->virt_addr, 0, dma_mem->size);

	ret = bsd_status_to_pqi_status(ret);

	/* DBG_FUNC("OUT\n"); */
	return ret;

err_load:
	if(dma_mem->virt_addr)
		bus_dmamem_free(dma_mem->dma_tag, dma_mem->virt_addr,
				dma_mem->dma_map);
err_mem:
	if(dma_mem->dma_tag)
		bus_dma_tag_destroy(dma_mem->dma_tag);
err_out:
	DBG_FUNC("failed OUT\n");

	ret = bsd_status_to_pqi_status(ret);

	return ret;
}

/*
 * DMA mem resource deallocation wrapper function
 */
void
os_dma_mem_free(pqisrc_softstate_t *softs, struct dma_mem *dma_mem)
{
	/* DBG_FUNC("IN\n"); */

	if(dma_mem->dma_addr) {
		bus_dmamap_unload(dma_mem->dma_tag, dma_mem->dma_map);
		dma_mem->dma_addr = 0;
	}

	if(dma_mem->virt_addr) {
		bus_dmamem_free(dma_mem->dma_tag, dma_mem->virt_addr,
					dma_mem->dma_map);
		dma_mem->virt_addr = NULL;
	}

	if(dma_mem->dma_tag) {
		bus_dma_tag_destroy(dma_mem->dma_tag);
		dma_mem->dma_tag = NULL;
	}

	/* DBG_FUNC("OUT\n");  */
}


/*
 * Mem resource allocation wrapper function
 */
void
*os_mem_alloc(pqisrc_softstate_t *softs, size_t size)
{
	void *addr;

	/* DBG_FUNC("IN\n");  */

	addr = malloc((unsigned long)size, M_SMARTPQI,
			M_NOWAIT | M_ZERO);

	/* DBG_FUNC("OUT\n"); */

	return addr;
}

/*
 * Mem resource deallocation wrapper function
 */
void
os_mem_free(pqisrc_softstate_t *softs, void *addr, size_t size)
{
	/* DBG_FUNC("IN\n"); */

	free((void*)addr, M_SMARTPQI);

	/* DBG_FUNC("OUT\n"); */
}

/*
 * dma/bus resource deallocation wrapper function
 */
void
os_resource_free(pqisrc_softstate_t *softs)
{
	if(softs->os_specific.pqi_parent_dmat)
		bus_dma_tag_destroy(softs->os_specific.pqi_parent_dmat);

	if (softs->os_specific.pqi_regs_res0 != NULL)
                bus_release_resource(softs->os_specific.pqi_dev,
				SYS_RES_MEMORY,
				softs->os_specific.pqi_regs_rid0,
				softs->os_specific.pqi_regs_res0);
}
