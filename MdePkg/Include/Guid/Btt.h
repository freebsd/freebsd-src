/** @file
  Block Translation Table (BTT) metadata layout definition.

  BTT is a layout and set of rules for doing block I/O that provide powerfail
  write atomicity of a single block.

Copyright (c) 2017, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This metadata layout definition was introduced in UEFI Specification 2.7.

**/

#ifndef _BTT_H_
#define _BTT_H_

///
/// The BTT layout and behavior is described by the GUID as below.
///
#define EFI_BTT_ABSTRACTION_GUID \
  { \
    0x18633bfc, 0x1735, 0x4217, { 0x8a, 0xc9, 0x17, 0x23, 0x92, 0x82, 0xd3, 0xf8 } \
  }

//
// Alignment of all BTT structures
//
#define EFI_BTT_ALIGNMENT  4096

#define EFI_BTT_INFO_UNUSED_LEN  3968

#define EFI_BTT_INFO_BLOCK_SIG_LEN  16

///
/// Indicate inconsistent metadata or lost metadata due to unrecoverable media errors.
///
#define EFI_BTT_INFO_BLOCK_FLAGS_ERROR  0x00000001

#define EFI_BTT_INFO_BLOCK_MAJOR_VERSION  2
#define EFI_BTT_INFO_BLOCK_MINOR_VERSION  0

///
/// Block Translation Table (BTT) Info Block
///
typedef struct _EFI_BTT_INFO_BLOCK {
  ///
  /// Signature of the BTT Index Block data structure.
  /// Shall be "BTT_ARENA_INFO\0\0".
  ///
  CHAR8     Sig[EFI_BTT_INFO_BLOCK_SIG_LEN];

  ///
  /// UUID identifying this BTT instance.
  ///
  GUID      Uuid;

  ///
  /// UUID of containing namespace.
  ///
  GUID      ParentUuid;

  ///
  /// Attributes of this BTT Info Block.
  ///
  UINT32    Flags;

  ///
  /// Major version number. Currently at version 2.
  ///
  UINT16    Major;

  ///
  /// Minor version number. Currently at version 0.
  ///
  UINT16    Minor;

  ///
  /// Advertised LBA size in bytes. I/O requests shall be in this size chunk.
  ///
  UINT32    ExternalLbaSize;

  ///
  /// Advertised number of LBAs in this arena.
  ///
  UINT32    ExternalNLba;

  ///
  /// Internal LBA size shall be greater than or equal to ExternalLbaSize and shall not be smaller than 512 bytes.
  ///
  UINT32    InternalLbaSize;

  ///
  /// Number of internal blocks in the arena data area.
  ///
  UINT32    InternalNLba;

  ///
  /// Number of free blocks maintained for writes to this arena.
  ///
  UINT32    NFree;

  ///
  /// The size of this info block in bytes.
  ///
  UINT32    InfoSize;

  ///
  /// Offset of next arena, relative to the beginning of this arena.
  ///
  UINT64    NextOff;

  ///
  /// Offset of the data area for this arena, relative to the beginning of this arena.
  ///
  UINT64    DataOff;

  ///
  /// Offset of the map for this arena, relative to the beginning of this arena.
  ///
  UINT64    MapOff;

  ///
  /// Offset of the flog for this arena, relative to the beginning of this arena.
  ///
  UINT64    FlogOff;

  ///
  /// Offset of the backup copy of this arena's info block, relative to the beginning of this arena.
  ///
  UINT64    InfoOff;

  ///
  /// Shall be zero.
  ///
  CHAR8     Unused[EFI_BTT_INFO_UNUSED_LEN];

  ///
  /// 64-bit Fletcher64 checksum of all fields.
  ///
  UINT64    Checksum;
} EFI_BTT_INFO_BLOCK;

///
/// BTT Map entry maps an LBA that indexes into the arena, to its actual location.
///
typedef struct _EFI_BTT_MAP_ENTRY {
  ///
  /// Post-map LBA number (block number in this arena's data area)
  ///
  UINT32    PostMapLba : 30;

  ///
  /// When set and Zero is not set, reads on this block return an error.
  /// When set and Zero is set, indicate a map entry in its normal, non-error state.
  ///
  UINT32    Error      : 1;

  ///
  /// When set and Error is not set, reads on this block return a full block of zeros.
  /// When set and Error is set, indicate a map entry in its normal, non-error state.
  ///
  UINT32    Zero       : 1;
} EFI_BTT_MAP_ENTRY;

///
/// Alignment of each flog structure
///
#define EFI_BTT_FLOG_ENTRY_ALIGNMENT  64

///
/// The BTT Flog is both a free list and a log.
/// The Flog size is determined by the EFI_BTT_INFO_BLOCK.NFree which determines how many of these flog
/// entries there are.
/// The Flog location is the highest aligned address in the arena after space for the backup info block.
///
typedef struct _EFI_BTT_FLOG {
  ///
  /// Last pre-map LBA written using this flog entry.
  ///
  UINT32    Lba0;

  ///
  /// Old post-map LBA.
  ///
  UINT32    OldMap0;

  ///
  /// New post-map LBA.
  ///
  UINT32    NewMap0;

  ///
  /// The Seq0 field in each flog entry is used to determine which set of fields is newer between the two sets
  /// (Lba0, OldMap0, NewMpa0, Seq0 vs Lba1, Oldmap1, NewMap1, Seq1).
  ///
  UINT32    Seq0;

  ///
  /// Alternate lba entry.
  ///
  UINT32    Lba1;

  ///
  /// Alternate old entry.
  ///
  UINT32    OldMap1;

  ///
  /// Alternate new entry.
  ///
  UINT32    NewMap1;

  ///
  /// Alternate Seq entry.
  ///
  UINT32    Seq1;
} EFI_BTT_FLOG;

extern GUID  gEfiBttAbstractionGuid;

#endif //_BTT_H_
