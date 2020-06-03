/** @file
  This file defines the EFI Partition Information Protocol.

  Copyright (c) 2017, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This Protocol is introduced in UEFI Specification 2.7

**/

#ifndef __PARTITION_INFO_PROTOCOL_H__
#define __PARTITION_INFO_PROTOCOL_H__

#include <IndustryStandard/Mbr.h>
#include <Uefi/UefiGpt.h>

//
// EFI Partition Information Protocol GUID value
//
#define EFI_PARTITION_INFO_PROTOCOL_GUID \
  { 0x8cf2f62c, 0xbc9b, 0x4821, { 0x80, 0x8d, 0xec, 0x9e, 0xc4, 0x21, 0xa1, 0xa0 }};


#define EFI_PARTITION_INFO_PROTOCOL_REVISION     0x0001000
#define PARTITION_TYPE_OTHER                     0x00
#define PARTITION_TYPE_MBR                       0x01
#define PARTITION_TYPE_GPT                       0x02

#pragma pack(1)

///
/// Partition Information Protocol structure.
///
typedef struct {
  //
  // Set to EFI_PARTITION_INFO_PROTOCOL_REVISION.
  //
  UINT32                     Revision;
  //
  // Partition info type (PARTITION_TYPE_MBR, PARTITION_TYPE_GPT, or PARTITION_TYPE_OTHER).
  //
  UINT32                     Type;
  //
  // If 1, partition describes an EFI System Partition.
  //
  UINT8                      System;
  UINT8                      Reserved[7];
  union {
    ///
    /// MBR data
    ///
    MBR_PARTITION_RECORD     Mbr;
    ///
    /// GPT data
    ///
    EFI_PARTITION_ENTRY      Gpt;
  } Info;
} EFI_PARTITION_INFO_PROTOCOL;

#pragma pack()

///
/// Partition Information Protocol GUID variable.
///
extern EFI_GUID gEfiPartitionInfoProtocolGuid;

#endif
