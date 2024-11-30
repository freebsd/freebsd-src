/** @file

  The definitions of DMTF Platform Level Data Model (PLDM)
  SMBIOS Transfer Specification.

  Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  DMTF Platform Level Data Model (PLDM) SMBIOS Transfer Specification
  Version 1.0.1
  https://www.dmtf.org/sites/default/files/standards/documents/DSP0246_1.0.1.pdf

**/

#ifndef PLDM_SMBIOS_TRANSFER_H_
#define PLDM_SMBIOS_TRANSFER_H_

#include <IndustryStandard/Pldm.h>

#pragma pack(1)

///
/// Smbios-related definitions from PLDM for SMBIOS Transfer
/// Specification (DMTF DSP0246)
///
#define PLDM_GET_SMBIOS_STRUCTURE_TABLE_METADATA_COMMAND_CODE  0x01
#define PLDM_SET_SMBIOS_STRUCTURE_TABLE_METADATA_COMMAND_CODE  0x02
#define PLDM_GET_SMBIOS_STRUCTURE_TABLE_COMMAND_CODE           0x03
#define PLDM_SET_SMBIOS_STRUCTURE_TABLE_COMMAND_CODE           0x04
#define PLDM_GET_SMBIOS_STRUCTURE_BY_TYPE_COMMAND_CODE         0x05
#define PLDM_GET_SMBIOS_STRUCTURE_BY_HANDLE_COMMAND_CODE       0x06

///
/// PLDM SMBIOS transfer command specific completion code.
///
#define PLDM_COMPLETION_CODE_INVALID_DATA_TRANSFER_HANDLE        0x80
#define PLDM_COMPLETION_CODE_INVALID_TRANSFER_OPERATION_FLAG     0x81
#define PLDM_COMPLETION_CODE_INVALID_TRANSFER_FLAG               0x82
#define PLDM_COMPLETION_CODE_NO_SMBIOS_STRUCTURE_TABLE_METADATA  0x83
#define PLDM_COMPLETION_CODE_INVALID_DATA_INTEGRITY_CHECK        0x84
#define PLDM_COMPLETION_CODE_SMBIOS_STRUCTURE_TABLE_UNAVAILABLE  0x85

///
/// Get SMBIOS Structure Table Metadata Response.
///
typedef struct {
  UINT8     SmbiosMajorVersion;
  UINT8     SmbiosMinorVersion;
  UINT16    MaximumStructureSize;
  UINT16    SmbiosStructureTableLength;
  UINT16    NumberOfSmbiosStructures;
  UINT32    SmbiosStructureTableIntegrityChecksum;
} PLDM_SMBIOS_STRUCTURE_TABLE_METADATA;

typedef struct {
  PLDM_RESPONSE_HEADER                    ResponseHeader;
  PLDM_SMBIOS_STRUCTURE_TABLE_METADATA    SmbiosStructureTableMetadata;
} PLDM_GET_SMBIOS_STRUCTURE_TABLE_METADATA_RESPONSE_FORMAT;

///
/// Set SMBIOS Structure Table Metadata Request.
///
typedef struct {
  PLDM_REQUEST_HEADER                     RequestHeader;
  PLDM_SMBIOS_STRUCTURE_TABLE_METADATA    SmbiosStructureTableMetadata;
} PLDM_SET_SMBIOS_STRUCTURE_TABLE_METADATA_REQUEST_FORMAT;

///
/// Set SMBIOS Structure Table Metadata Response.
///
typedef struct {
  PLDM_RESPONSE_HEADER    ResponseHeader;
} PLDM_SET_SMBIOS_STRUCTURE_TABLE_METADATA_RESPONSE_FORMAT;

///
/// Get SMBIOS Structure Table Request.
///
typedef struct {
  UINT32    DataTransferHandle;
  UINT8     TransferOperationFlag;
} PLDM_GET_SMBIOS_STRUCTURE_TABLE_REQUEST;

typedef struct {
  PLDM_REQUEST_HEADER                        RequestHeader;
  PLDM_GET_SMBIOS_STRUCTURE_TABLE_REQUEST    GetSmbiosStructureTableRequest;
} PLDM_GET_SMBIOS_STRUCTURE_TABLE_REQUEST_FORMAT;

///
/// Get SMBIOS Structure Table Response.
///
typedef struct {
  UINT32    NextDataTransferHandle;
  UINT8     TransferFlag;
  UINT8     Table[0];
} PLDM_GET_SMBIOS_STRUCTURE_TABLE_RESPONSE;

typedef struct {
  PLDM_RESPONSE_HEADER                        ResponseHeader;
  PLDM_GET_SMBIOS_STRUCTURE_TABLE_RESPONSE    GetSmbiosStructureTableResponse;
} PLDM_GET_SMBIOS_STRUCTURE_TABLE_RESPONSE_FORMAT;

///
/// Set SMBIOS Structure Table Request.
///
typedef struct {
  UINT32    DataTransferHandle;
  UINT8     TransferFlag;
  UINT8     Table[0];
} PLDM_SET_SMBIOS_STRUCTURE_TABLE_REQUEST;

typedef struct {
  PLDM_REQUEST_HEADER                        RequestHeader;
  PLDM_SET_SMBIOS_STRUCTURE_TABLE_REQUEST    SetSmbiosStructureTableRequest;
} PLDM_SET_SMBIOS_STRUCTURE_TABLE_REQUEST_FORMAT;

///
/// Set SMBIOS Structure Table Response.
///
typedef struct {
  PLDM_RESPONSE_HEADER    ResponseHeader;
  UINT32                  NextDataTransferHandle;
} PLDM_SET_SMBIOS_STRUCTURE_TABLE_RESPONSE_FORMAT;

///
/// Get SMBIOS Structure by Type Request.
///
typedef struct {
  UINT32    DataTransferHandle;
  UINT8     TransferOperationFlag;
  UINT8     Type;
  UINT16    StructureInstanceId;
} PLDM_GET_SMBIOS_STRUCTURE_BY_TYPE_REQUEST;

typedef struct {
  PLDM_REQUEST_HEADER                          RequestHeader;
  PLDM_GET_SMBIOS_STRUCTURE_BY_TYPE_REQUEST    GetSmbiosStructureByTypeRequest;
} PLDM_GET_SMBIOS_STRUCTURE_BY_TYPE_REQUEST_FORMAT;

///
/// Get SMBIOS Structure by Type Response.
///
typedef struct {
  UINT32    NextDataTransferHandle;
  UINT8     TransferFlag;
  UINT8     Table[0];
} PLDM_GET_SMBIOS_STRUCTURE_BY_TYPE_RESPONSE;

typedef struct {
  PLDM_RESPONSE_HEADER                          ResponseHeader;
  PLDM_GET_SMBIOS_STRUCTURE_BY_TYPE_RESPONSE    GetSmbiosStructureByTypeResponse;
} PLDM_GET_SMBIOS_STRUCTURE_BY_TYPE_RESPONSE_FORMAT;

///
/// Get SMBIOS Structure by Handle Request.
///
typedef struct {
  UINT32    DataTransferHandle;
  UINT8     TransferOperationFlag;
  UINT16    Handle;
} PLDM_GET_SMBIOS_STRUCTURE_BY_HANDLE_REQUEST;

typedef struct {
  PLDM_REQUEST_HEADER                            RequestHeader;
  PLDM_GET_SMBIOS_STRUCTURE_BY_HANDLE_REQUEST    GetSmbiosStructureByHandleRequest;
} PLDM_GET_SMBIOS_STRUCTURE_BY_HANDLE_REQUEST_FORMAT;

///
/// Get SMBIOS Structure by Handle Response.
///
typedef struct {
  UINT32    NextDataTransferHandle;
  UINT8     TransferFlag;
  UINT8     Table[0];
} PLDM_GET_SMBIOS_STRUCTURE_BY_HANDLE_RESPONSE;

typedef struct {
  PLDM_RESPONSE_HEADER                            ResponseHeader;
  PLDM_GET_SMBIOS_STRUCTURE_BY_HANDLE_RESPONSE    GetSmbiosStructureByTypeResponse;
} PLDM_GET_SMBIOS_STRUCTURE_BY_HANDLE_RESPONSE_FORMAT;
#pragma pack()

#endif // PLDM_SMBIOS_TRANSFER_H_
