/** @file
  OSTA Universal Disk Format (UDF) definitions.

  Copyright (C) 2014-2017 Paulo Alcantara <pcacjr@zytor.com>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef __UDF_H__
#define __UDF_H__

#define UDF_BEA_IDENTIFIER   "BEA01"
#define UDF_NSR2_IDENTIFIER  "NSR02"
#define UDF_NSR3_IDENTIFIER  "NSR03"
#define UDF_TEA_IDENTIFIER   "TEA01"

#define UDF_LOGICAL_SECTOR_SHIFT  11
#define UDF_LOGICAL_SECTOR_SIZE   ((UINT64)(1ULL << UDF_LOGICAL_SECTOR_SHIFT))
#define UDF_VRS_START_OFFSET      ((UINT64)(16ULL << UDF_LOGICAL_SECTOR_SHIFT))

typedef enum {
  UdfPrimaryVolumeDescriptor = 1,
  UdfAnchorVolumeDescriptorPointer = 2,
  UdfVolumeDescriptorPointer = 3,
  UdfImplemenationUseVolumeDescriptor = 4,
  UdfPartitionDescriptor = 5,
  UdfLogicalVolumeDescriptor = 6,
  UdfUnallocatedSpaceDescriptor = 7,
  UdfTerminatingDescriptor = 8,
  UdfLogicalVolumeIntegrityDescriptor = 9,
  UdfFileSetDescriptor = 256,
  UdfFileIdentifierDescriptor = 257,
  UdfAllocationExtentDescriptor = 258,
  UdfFileEntry = 261,
  UdfExtendedFileEntry = 266,
} UDF_VOLUME_DESCRIPTOR_ID;

#pragma pack(1)

typedef struct {
  UINT16  TagIdentifier;
  UINT16  DescriptorVersion;
  UINT8   TagChecksum;
  UINT8   Reserved;
  UINT16  TagSerialNumber;
  UINT16  DescriptorCRC;
  UINT16  DescriptorCRCLength;
  UINT32  TagLocation;
} UDF_DESCRIPTOR_TAG;

typedef struct {
  UINT32  ExtentLength;
  UINT32  ExtentLocation;
} UDF_EXTENT_AD;

typedef struct {
  UINT8           CharacterSetType;
  UINT8           CharacterSetInfo[63];
} UDF_CHAR_SPEC;

typedef struct {
  UINT8           Flags;
  UINT8           Identifier[23];
  union {
    //
    // Domain Entity Identifier
    //
    struct {
      UINT16      UdfRevision;
      UINT8       DomainFlags;
      UINT8       Reserved[5];
    } Domain;
    //
    // UDF Entity Identifier
    //
    struct {
      UINT16      UdfRevision;
      UINT8       OSClass;
      UINT8       OSIdentifier;
      UINT8       Reserved[4];
    } Entity;
    //
    // Implementation Entity Identifier
    //
    struct {
      UINT8       OSClass;
      UINT8       OSIdentifier;
      UINT8       ImplementationUseArea[6];
    } ImplementationEntity;
    //
    // Application Entity Identifier
    //
    struct {
      UINT8       ApplicationUseArea[8];
    } ApplicationEntity;
    //
    // Raw Identifier Suffix
    //
    struct {
      UINT8       Data[8];
    } Raw;
  } Suffix;
} UDF_ENTITY_ID;

typedef struct {
  UINT32        LogicalBlockNumber;
  UINT16        PartitionReferenceNumber;
} UDF_LB_ADDR;

typedef struct {
  UINT32                           ExtentLength;
  UDF_LB_ADDR                      ExtentLocation;
  UINT8                            ImplementationUse[6];
} UDF_LONG_ALLOCATION_DESCRIPTOR;

typedef struct {
  UDF_DESCRIPTOR_TAG  DescriptorTag;
  UDF_EXTENT_AD       MainVolumeDescriptorSequenceExtent;
  UDF_EXTENT_AD       ReserveVolumeDescriptorSequenceExtent;
  UINT8               Reserved[480];
} UDF_ANCHOR_VOLUME_DESCRIPTOR_POINTER;

typedef struct {
  UDF_DESCRIPTOR_TAG              DescriptorTag;
  UINT32                          VolumeDescriptorSequenceNumber;
  UDF_CHAR_SPEC                   DescriptorCharacterSet;
  UINT8                           LogicalVolumeIdentifier[128];
  UINT32                          LogicalBlockSize;
  UDF_ENTITY_ID                   DomainIdentifier;
  UDF_LONG_ALLOCATION_DESCRIPTOR  LogicalVolumeContentsUse;
  UINT32                          MapTableLength;
  UINT32                          NumberOfPartitionMaps;
  UDF_ENTITY_ID                   ImplementationIdentifier;
  UINT8                           ImplementationUse[128];
  UDF_EXTENT_AD                   IntegritySequenceExtent;
  UINT8                           PartitionMaps[6];
} UDF_LOGICAL_VOLUME_DESCRIPTOR;

#pragma pack()

#endif
