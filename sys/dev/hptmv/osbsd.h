/*-
 * Copyright (c) 2003-2004 HighPoint Technologies, Inc.
 * All rights reserved.
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
 *
 * $FreeBSD$
 */
#ifndef _OSBSD_H_
#define _OSBSD_H_

#include <sys/bus.h>
#include <sys/resource.h>
#include <sys/eventhandler.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/bus.h>
#include <sys/rman.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_debug.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_periph.h>
#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>

extern intrmask_t lock_driver(void);
extern void unlock_driver(intrmask_t spl);

typedef struct 
{
	UCHAR		status;		/* 0 nonbootable; 80h bootable */
	UCHAR      	start_head;
	USHORT     	start_sector;
	UCHAR      	type;
	UCHAR      	end_head;
	USHORT     	end_sector;
	ULONG      	start_abs_sector;
	ULONG      	num_of_sector;
} partition;

typedef struct _INQUIRYDATA {
	UCHAR DeviceType : 5;
	UCHAR DeviceTypeQualifier : 3;
	UCHAR DeviceTypeModifier : 7;
	UCHAR RemovableMedia : 1;
	UCHAR Versions;
	UCHAR ResponseDataFormat;
	UCHAR AdditionalLength;
	UCHAR Reserved[2];
	UCHAR SoftReset : 1;
	UCHAR CommandQueue : 1;
	UCHAR Reserved2 : 1;
	UCHAR LinkedCommands : 1;
	UCHAR Synchronous : 1;
	UCHAR Wide16Bit : 1;
	UCHAR Wide32Bit : 1;
	UCHAR RelativeAddressing : 1;
	UCHAR VendorId[8];
	UCHAR ProductId[16];
	UCHAR ProductRevisionLevel[4];
	UCHAR VendorSpecific[20];
	UCHAR Reserved3[40];
} INQUIRYDATA, *PINQUIRYDATA;

typedef struct _READ_CAPACITY_DATA {
	ULONG LogicalBlockAddress;
	ULONG BytesPerBlock;
} READ_CAPACITY_DATA, *PREAD_CAPACITY_DATA;

#define MV_IAL_HT_SACOALT_DEFAULT	1
#define MV_IAL_HT_SAITMTH_DEFAULT	1

/****************************************/
/*          GENERAL Definitions         */
/****************************************/

/* Bits for HD_ERROR */
#define NM_ERR		0x02	/* media present */
#define ABRT_ERR	0x04	/* Command aborted */
#define MCR_ERR         0x08	/* media change request */
#define IDNF_ERR        0x10	/* ID field not found */
#define MC_ERR          0x20	/* media changed */
#define UNC_ERR         0x40	/* Uncorrect data */
#define WP_ERR          0x40	/* write protect */
#define ICRC_ERR        0x80	/* new meaning:  CRC error during transfer */

#define REQUESTS_ARRAY_SIZE	(9 * MV_EDMA_REQUEST_QUEUE_SIZE) /* 9 K bytes */
#define RESPONSES_ARRAY_SIZE	(12*MV_EDMA_RESPONSE_QUEUE_SIZE) /* 3 K bytes */

#define PRD_ENTRIES_PER_CMD	(MAX_SG_DESCRIPTORS+1)
#define PRD_ENTRIES_SIZE	(MV_EDMA_PRD_ENTRY_SIZE*PRD_ENTRIES_PER_CMD)
#define PRD_TABLES_FOR_VBUS	(MV_SATA_CHANNELS_NUM*MV_EDMA_QUEUE_LENGTH)

typedef enum _SataEvent {
	SATA_EVENT_NO_CHANGE = 0,
	SATA_EVENT_CHANNEL_CONNECTED,
	SATA_EVENT_CHANNEL_DISCONNECTED
} SATA_EVENT;

typedef ULONG_PTR dma_addr_t;

typedef struct _MV_CHANNEL {
	unsigned int		maxUltraDmaModeSupported;
	unsigned int		maxDmaModeSupported;
	unsigned int		maxPioModeSupported;
	MV_BOOLEAN		online;
} MV_CHANNEL;

struct _privCommand;

typedef struct IALAdapter {
	struct cam_path 	*path;
	device_t		hpt_dev;		/* bus device */
	struct resource		*hpt_irq;		/* interrupt */
	void			*hpt_intr;		/* interrupt handle */
	struct resource		*mem_res;
	bus_space_handle_t	mem_bsh;
	bus_space_tag_t		mem_btag;
	bus_dma_tag_t		parent_dmat;
	bus_dma_tag_t		req_dmat;
	bus_dmamap_t		req_map;
	bus_dma_tag_t		resp_dmat;
	bus_dmamap_t		resp_map;
	bus_dma_tag_t		prd_dmat;
	bus_dmamap_t		prd_map;
	bus_dma_tag_t		buf_dmat;
	
	struct IALAdapter	*next;

	MV_SATA_ADAPTER		mvSataAdapter;
	MV_CHANNEL		mvChannel[MV_SATA_CHANNELS_NUM];
	MV_U8			*requestsArrayBaseAddr;
	MV_U8			*requestsArrayBaseAlignedAddr;
	dma_addr_t		requestsArrayBaseDmaAddr;
	dma_addr_t		requestsArrayBaseDmaAlignedAddr;
	MV_U8			*responsesArrayBaseAddr;
	MV_U8			*responsesArrayBaseAlignedAddr;
	dma_addr_t		responsesArrayBaseDmaAddr;
	dma_addr_t		responsesArrayBaseDmaAlignedAddr;
	SATA_EVENT		sataEvents[MV_SATA_CHANNELS_NUM];
	
   	struct	callout_handle event_timer_connect;
  	struct	callout_handle event_timer_disconnect;

	struct _VBus		VBus;
	struct _VDevice		VDevices[MV_SATA_CHANNELS_NUM];
	PCommand		pCommandBlocks;
	struct _privCommand	*pPrivateBlocks;
	TAILQ_HEAD(, _privCommand)	PrivCmdTQH;
	PUCHAR			prdTableAddr;
	ULONG			prdTableDmaAddr;
	void*			pFreePRDLink;

	union ccb		*pending_Q;

	MV_U8   		outstandingCommands;

	UCHAR			status;
	UCHAR			ver_601;
	UCHAR			beeping;

	eventhandler_tag	eh;
} IAL_ADAPTER_T;

typedef struct _privCommand {
	TAILQ_ENTRY(_privCommand)	PrivEntry;
	IAL_ADAPTER_T			*pAdapter;
	union ccb			*ccb;
	bus_dmamap_t			buf_map;
} *pPrivCommand;

extern IAL_ADAPTER_T *gIal_Adapter;

/*entry.c*/
typedef void (*HPT_DPC)(IAL_ADAPTER_T *,void*,UCHAR);
int hpt_queue_dpc(HPT_DPC dpc, IAL_ADAPTER_T *pAdapter, void *arg, UCHAR flags);
void hpt_rebuild_data_block(IAL_ADAPTER_T *pAdapter, PVDevice pArray,
    UCHAR flags);
void Check_Idle_Call(IAL_ADAPTER_T *pAdapter);
int Kernel_DeviceIoControl(_VBUS_ARG DWORD dwIoControlCode, PVOID lpInBuffer,
    DWORD nInBufferSize, PVOID lpOutBuffer, DWORD nOutBufferSize,
    PDWORD lpBytesReturned);
void fRescanAllDevice(_VBUS_ARG0);
int hpt_add_disk_to_array(_VBUS_ARG DEVICEID idArray, DEVICEID idDisk);


#define __str_direct(x) #x
#define __str(x) __str_direct(x)
#define KMSG_LEADING __str(PROC_DIR_NAME) ": "
#define hpt_printk(_x_) do {	\
	printf(KMSG_LEADING);	\
	printf _x_ ;		\
} while (0)

#define DUPLICATE      0
#define INITIALIZE     1
#define REBUILD_PARITY 2
#define VERIFY         3

/**********************************************************/
static __inline struct cam_periph *
hpt_get_periph(int path_id,int target_id)
{
	struct cam_periph *periph = NULL;
	struct cam_path	*path;
	int status;

	status = xpt_create_path(&path, NULL, path_id, target_id, 0);
	if (status == CAM_REQ_CMP) {
		periph = cam_periph_find(path, NULL);
		xpt_free_path(path);
		if (periph != NULL) {
			if (strncmp(periph->periph_name, "da", 2))
				periph = NULL;
    		}
    	}
	return periph;	
}

static __inline void
FreePrivCommand(IAL_ADAPTER_T *pAdapter, pPrivCommand prvCmd)
{
	TAILQ_INSERT_TAIL(&pAdapter->PrivCmdTQH, prvCmd, PrivEntry);
}

static __inline pPrivCommand
AllocPrivCommand(IAL_ADAPTER_T *pAdapter)
{
	pPrivCommand prvCmd;

	prvCmd = TAILQ_FIRST(&pAdapter->PrivCmdTQH);
	if (prvCmd == NULL)
		return NULL;

	TAILQ_REMOVE(&pAdapter->PrivCmdTQH, prvCmd, PrivEntry);
	return (prvCmd);
}

static __inline void
mv_reg_write_byte(MV_BUS_ADDR_T base, MV_U32 offset, MV_U8 val)
{
	IAL_ADAPTER_T *pAdapter;
	
	pAdapter = base;
	bus_space_write_1(pAdapter->mem_btag, pAdapter->mem_bsh, offset, val);
}

static __inline void
mv_reg_write_word(MV_BUS_ADDR_T base, MV_U32 offset, MV_U16 val)
{
	IAL_ADAPTER_T *pAdapter;
	
	pAdapter = base;
	bus_space_write_2(pAdapter->mem_btag, pAdapter->mem_bsh, offset, val);
}

static __inline void
mv_reg_write_dword(MV_BUS_ADDR_T base, MV_U32 offset, MV_U32 val)
{
	IAL_ADAPTER_T *pAdapter;

	pAdapter = base;
	bus_space_write_4(pAdapter->mem_btag, pAdapter->mem_bsh, offset, val);
}

static __inline MV_U8
mv_reg_read_byte(MV_BUS_ADDR_T base, MV_U32 offset)
{
	IAL_ADAPTER_T *pAdapter;
	
	pAdapter = base;
	return (bus_space_read_1(pAdapter->mem_btag, pAdapter->mem_bsh,
	    offset));
}

static __inline MV_U16
mv_reg_read_word(MV_BUS_ADDR_T base, MV_U32 offset)
{
	IAL_ADAPTER_T *pAdapter;
	
	pAdapter = base;
	return (bus_space_read_2(pAdapter->mem_btag, pAdapter->mem_bsh,
	    offset));
}

static __inline MV_U32
mv_reg_read_dword(MV_BUS_ADDR_T base, MV_U32 offset)
{
	IAL_ADAPTER_T *pAdapter;
	
	pAdapter = base;
	return (bus_space_read_4(pAdapter->mem_btag, pAdapter->mem_bsh,
	    offset));
}

#endif
