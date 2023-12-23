/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Leandro Lupori
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

#include <sys/param.h>
#include <sys/types.h>

#include <dev/aacraid/aacraid_reg.h>
#include <dev/aacraid/aacraid_endian.h>

#if _BYTE_ORDER != _LITTLE_ENDIAN

#define TOH2(field, bits)	field = le##bits##toh(field)
#define TOH(field, bits)	TOH2(field, bits)

#define TOLE2(field, bits)	field = htole##bits(field)
#define TOLE(field, bits)	TOLE2(field, bits)

/* Convert from Little-Endian to host order (TOH) */

void
aac_fib_header_toh(struct aac_fib_header *ptr)
{
	TOH(ptr->XferState, 32);
	TOH(ptr->Command, 16);
	TOH(ptr->Size, 16);
	TOH(ptr->SenderSize, 16);
	TOH(ptr->SenderFibAddress, 32);
	TOH(ptr->u.ReceiverFibAddress, 32);
	TOH(ptr->Handle, 32);
	TOH(ptr->Previous, 32);
	TOH(ptr->Next, 32);
}

void
aac_adapter_info_toh(struct aac_adapter_info *ptr)
{
	TOH(ptr->PlatformBase, 32);
	TOH(ptr->CpuArchitecture, 32);
	TOH(ptr->CpuVariant, 32);
	TOH(ptr->ClockSpeed, 32);
	TOH(ptr->ExecutionMem, 32);
	TOH(ptr->BufferMem, 32);
	TOH(ptr->TotalMem, 32);

	TOH(ptr->KernelRevision.buildNumber, 32);
	TOH(ptr->MonitorRevision.buildNumber, 32);
	TOH(ptr->HardwareRevision.buildNumber, 32);
	TOH(ptr->BIOSRevision.buildNumber, 32);

	TOH(ptr->ClusteringEnabled, 32);
	TOH(ptr->ClusterChannelMask, 32);
	TOH(ptr->SerialNumber, 64);
	TOH(ptr->batteryPlatform, 32);
	TOH(ptr->SupportedOptions, 32);
	TOH(ptr->OemVariant, 32);
}

void
aac_container_creation_toh(struct aac_container_creation *ptr)
{
	u_int32_t *date = (u_int32_t *)ptr + 1;

	*date = le32toh(*date);
	TOH(ptr->ViaAdapterSerialNumber, 64);
}

void
aac_mntobj_toh(struct aac_mntobj *ptr)
{
	TOH(ptr->ObjectId, 32);
	aac_container_creation_toh(&ptr->CreateInfo);
	TOH(ptr->Capacity, 32);
	TOH(ptr->VolType, 32);
	TOH(ptr->ObjType, 32);
	TOH(ptr->ContentState, 32);
	TOH(ptr->ObjExtension.BlockDevice.BlockSize, 32);
	TOH(ptr->ObjExtension.BlockDevice.bdLgclPhysMap, 32);
	TOH(ptr->AlterEgoId, 32);
	TOH(ptr->CapacityHigh, 32);
}

void
aac_mntinforesp_toh(struct aac_mntinforesp *ptr)
{
	TOH(ptr->Status, 32);
	TOH(ptr->MntType, 32);
	TOH(ptr->MntRespCount, 32);
	aac_mntobj_toh(&ptr->MntTable[0]);
}

void
aac_fsa_ctm_toh(struct aac_fsa_ctm *ptr)
{
	int i;

	TOH(ptr->command, 32);
	for (i = 0; i < CT_FIB_PARAMS; i++)
		TOH(ptr->param[i], 32);
}

void
aac_cnt_config_toh(struct aac_cnt_config *ptr)
{
	TOH(ptr->Command, 32);
	aac_fsa_ctm_toh(&ptr->CTCommand);
}

void
aac_ctcfg_resp_toh(struct aac_ctcfg_resp *ptr)
{
	TOH(ptr->Status, 32);
	TOH(ptr->resp, 32);
	TOH(ptr->param, 32);
}

void
aac_getbusinf_toh(struct aac_getbusinf *ptr)
{
	TOH(ptr->ProbeComplete, 32);
	TOH(ptr->BusCount, 32);
	TOH(ptr->TargetsPerBus, 32);
}

void
aac_vmi_businf_resp_toh(struct aac_vmi_businf_resp *ptr)
{
	TOH(ptr->Status, 32);
	TOH(ptr->ObjType, 32);
	TOH(ptr->MethId, 32);
	TOH(ptr->ObjId, 32);
	TOH(ptr->IoctlCmd, 32);
	aac_getbusinf_toh(&ptr->BusInf);
}

void
aac_srb_response_toh(struct aac_srb_response *ptr)
{
	TOH(ptr->fib_status, 32);
	TOH(ptr->srb_status, 32);
	TOH(ptr->scsi_status, 32);
	TOH(ptr->data_len, 32);
	TOH(ptr->sense_len, 32);
}

/* Convert from host order to Little-Endian (TOLE) */

void
aac_adapter_init_tole(struct aac_adapter_init *ptr)
{
	TOLE(ptr->InitStructRevision, 32);
	TOLE(ptr->NoOfMSIXVectors, 32);
	TOLE(ptr->FilesystemRevision, 32);
	TOLE(ptr->CommHeaderAddress, 32);
	TOLE(ptr->FastIoCommAreaAddress, 32);
	TOLE(ptr->AdapterFibsPhysicalAddress, 32);
	TOLE(ptr->AdapterFibsVirtualAddress, 32);
	TOLE(ptr->AdapterFibsSize, 32);
	TOLE(ptr->AdapterFibAlign, 32);
	TOLE(ptr->PrintfBufferAddress, 32);
	TOLE(ptr->PrintfBufferSize, 32);
	TOLE(ptr->HostPhysMemPages, 32);
	TOLE(ptr->HostElapsedSeconds, 32);
	TOLE(ptr->InitFlags, 32);
	TOLE(ptr->MaxIoCommands, 32);
	TOLE(ptr->MaxIoSize, 32);
	TOLE(ptr->MaxFibSize, 32);
	TOLE(ptr->MaxNumAif, 32);
	TOLE(ptr->HostRRQ_AddrLow, 32);
	TOLE(ptr->HostRRQ_AddrHigh, 32);
}

void
aac_fib_header_tole(struct aac_fib_header *ptr)
{
	TOLE(ptr->XferState, 32);
	TOLE(ptr->Command, 16);
	TOLE(ptr->Size, 16);
	TOLE(ptr->SenderSize, 16);
	TOLE(ptr->SenderFibAddress, 32);
	TOLE(ptr->u.ReceiverFibAddress, 32);
	TOLE(ptr->Handle, 32);
	TOLE(ptr->Previous, 32);
	TOLE(ptr->Next, 32);
}

void
aac_mntinfo_tole(struct aac_mntinfo *ptr)
{
	TOLE(ptr->Command, 32);
	TOLE(ptr->MntType, 32);
	TOLE(ptr->MntCount, 32);
}

void
aac_fsa_ctm_tole(struct aac_fsa_ctm *ptr)
{
	int i;

	TOLE(ptr->command, 32);
	for (i = 0; i < CT_FIB_PARAMS; i++)
		TOLE(ptr->param[i], 32);
}

void
aac_cnt_config_tole(struct aac_cnt_config *ptr)
{
	TOLE(ptr->Command, 32);
	aac_fsa_ctm_tole(&ptr->CTCommand);
}

void
aac_raw_io_tole(struct aac_raw_io *ptr)
{
	TOLE(ptr->BlockNumber, 64);
	TOLE(ptr->ByteCount, 32);
	TOLE(ptr->ContainerId, 16);
	TOLE(ptr->Flags, 16);
	TOLE(ptr->BpTotal, 16);
	TOLE(ptr->BpComplete, 16);
}

void
aac_raw_io2_tole(struct aac_raw_io2 *ptr)
{
	TOLE(ptr->strtBlkLow, 32);
	TOLE(ptr->strtBlkHigh, 32);
	TOLE(ptr->byteCnt, 32);
	TOLE(ptr->ldNum, 16);
	TOLE(ptr->flags, 16);
	TOLE(ptr->sgeFirstSize, 32);
	TOLE(ptr->sgeNominalSize, 32);
}

void
aac_fib_xporthdr_tole(struct aac_fib_xporthdr *ptr)
{
	TOLE(ptr->HostAddress, 64);
	TOLE(ptr->Size, 32);
	TOLE(ptr->Handle, 32);
}

void
aac_ctcfg_tole(struct aac_ctcfg *ptr)
{
	TOLE(ptr->Command, 32);
	TOLE(ptr->cmd, 32);
	TOLE(ptr->param, 32);
}

void
aac_vmioctl_tole(struct aac_vmioctl *ptr)
{
	TOLE(ptr->Command, 32);
	TOLE(ptr->ObjType, 32);
	TOLE(ptr->MethId, 32);
	TOLE(ptr->ObjId, 32);
	TOLE(ptr->IoctlCmd, 32);
	TOLE(ptr->IoctlBuf[0], 32);
}

void
aac_pause_command_tole(struct aac_pause_command *ptr)
{
	TOLE(ptr->Command, 32);
	TOLE(ptr->Type, 32);
	TOLE(ptr->Timeout, 32);
	TOLE(ptr->Min, 32);
	TOLE(ptr->NoRescan, 32);
	TOLE(ptr->Parm3, 32);
	TOLE(ptr->Parm4, 32);
	TOLE(ptr->Count, 32);
}

void
aac_srb_tole(struct aac_srb *ptr)
{
	TOLE(ptr->function, 32);
	TOLE(ptr->bus, 32);
	TOLE(ptr->target, 32);
	TOLE(ptr->lun, 32);
	TOLE(ptr->timeout, 32);
	TOLE(ptr->flags, 32);
	TOLE(ptr->data_len, 32);
	TOLE(ptr->retry_limit, 32);
	TOLE(ptr->cdb_len, 32);
}

void
aac_sge_ieee1212_tole(struct aac_sge_ieee1212 *ptr)
{
	TOLE(ptr->addrLow, 32);
	TOLE(ptr->addrHigh, 32);
	TOLE(ptr->length, 32);
	TOLE(ptr->flags, 32);
}

void
aac_sg_entryraw_tole(struct aac_sg_entryraw *ptr)
{
	TOLE(ptr->Next, 32);
	TOLE(ptr->Prev, 32);
	TOLE(ptr->SgAddress, 64);
	TOLE(ptr->SgByteCount, 32);
	TOLE(ptr->Flags, 32);
}

void
aac_sg_entry_tole(struct aac_sg_entry *ptr)
{
	TOLE(ptr->SgAddress, 32);
	TOLE(ptr->SgByteCount, 32);
}

void
aac_sg_entry64_tole(struct aac_sg_entry64 *ptr)
{
	TOLE(ptr->SgAddress, 64);
	TOLE(ptr->SgByteCount, 32);
}

void
aac_blockread_tole(struct aac_blockread *ptr)
{
	TOLE(ptr->Command, 32);
	TOLE(ptr->ContainerId, 32);
	TOLE(ptr->BlockNumber, 32);
	TOLE(ptr->ByteCount, 32);
}

void
aac_blockwrite_tole(struct aac_blockwrite *ptr)
{
	TOLE(ptr->Command, 32);
	TOLE(ptr->ContainerId, 32);
	TOLE(ptr->BlockNumber, 32);
	TOLE(ptr->ByteCount, 32);
	TOLE(ptr->Stable, 32);
}

void
aac_blockread64_tole(struct aac_blockread64 *ptr)
{
	TOLE(ptr->Command, 32);
	TOLE(ptr->ContainerId, 16);
	TOLE(ptr->SectorCount, 16);
	TOLE(ptr->BlockNumber, 32);
	TOLE(ptr->Pad, 16);
	TOLE(ptr->Flags, 16);
}

void
aac_blockwrite64_tole(struct aac_blockwrite64 *ptr)
{
	TOLE(ptr->Command, 32);
	TOLE(ptr->ContainerId, 16);
	TOLE(ptr->SectorCount, 16);
	TOLE(ptr->BlockNumber, 32);
	TOLE(ptr->Pad, 16);
	TOLE(ptr->Flags, 16);
}

#endif
