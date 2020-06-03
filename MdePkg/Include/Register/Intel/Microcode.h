/** @file
  Microcode Definitions.

  Microcode Definitions based on contents of the
  Intel(R) 64 and IA-32 Architectures Software Developer's Manual
    Volume 3A, Section 9.11  Microcode Definitions

  Copyright (c) 2016 - 2019, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Specification Reference:
  Intel(R) 64 and IA-32 Architectures Software Developer's Manual, Volume 3A,
  June 2016, Chapter 9 Processor Management and Initialization, Section 9-11.

**/

#ifndef __INTEL_MICROCODE_H__
#define __INTEL_MICROCODE_H__

///
/// CPU Microcode Date in BCD format
///
typedef union {
  struct {
    UINT32   Year:16;
    UINT32   Day:8;
    UINT32   Month:8;
  } Bits;
  UINT32     Uint32;
} CPU_MICROCODE_DATE;

///
/// CPU Microcode Processor Signature format
///
typedef union {
  struct {
    UINT32   Stepping:4;
    UINT32   Model:4;
    UINT32   Family:4;
    UINT32   Type:2;
    UINT32   Reserved1:2;
    UINT32   ExtendedModel:4;
    UINT32   ExtendedFamily:8;
    UINT32   Reserved2:4;
  } Bits;
  UINT32     Uint32;
} CPU_MICROCODE_PROCESSOR_SIGNATURE;

#pragma pack (1)

///
/// Microcode Update Format definition
///
typedef struct {
  ///
  /// Version number of the update header
  ///
  UINT32                            HeaderVersion;
  ///
  /// Unique version number for the update, the basis for the update
  /// signature provided by the processor to indicate the current update
  /// functioning within the processor. Used by the BIOS to authenticate
  /// the update and verify that the processor loads successfully. The
  /// value in this field cannot be used for processor stepping identification
  /// alone. This is a signed 32-bit number.
  ///
  UINT32                            UpdateRevision;
  ///
  /// Date of the update creation in binary format: mmddyyyy (e.g.
  /// 07/18/98 is 07181998H).
  ///
  CPU_MICROCODE_DATE                Date;
  ///
  /// Extended family, extended model, type, family, model, and stepping
  /// of processor that requires this particular update revision (e.g.,
  /// 00000650H). Each microcode update is designed specifically for a
  /// given extended family, extended model, type, family, model, and
  /// stepping of the processor.
  /// The BIOS uses the processor signature field in conjunction with the
  /// CPUID instruction to determine whether or not an update is
  /// appropriate to load on a processor. The information encoded within
  /// this field exactly corresponds to the bit representations returned by
  /// the CPUID instruction.
  ///
  CPU_MICROCODE_PROCESSOR_SIGNATURE ProcessorSignature;
  ///
  /// Checksum of Update Data and Header. Used to verify the integrity of
  /// the update header and data. Checksum is correct when the
  /// summation of all the DWORDs (including the extended Processor
  /// Signature Table) that comprise the microcode update result in
  /// 00000000H.
  ///
  UINT32                            Checksum;
  ///
  /// Version number of the loader program needed to correctly load this
  /// update. The initial version is 00000001H
  ///
  UINT32                            LoaderRevision;
  ///
  /// Platform type information is encoded in the lower 8 bits of this 4-
  /// byte field. Each bit represents a particular platform type for a given
  /// CPUID. The BIOS uses the processor flags field in conjunction with
  /// the platform Id bits in MSR (17H) to determine whether or not an
  /// update is appropriate to load on a processor. Multiple bits may be set
  /// representing support for multiple platform IDs.
  ///
  UINT32                            ProcessorFlags;
  ///
  /// Specifies the size of the encrypted data in bytes, and must be a
  /// multiple of DWORDs. If this value is 00000000H, then the microcode
  /// update encrypted data is 2000 bytes (or 500 DWORDs).
  ///
  UINT32                            DataSize;
  ///
  /// Specifies the total size of the microcode update in bytes. It is the
  /// summation of the header size, the encrypted data size and the size of
  /// the optional extended signature table. This value is always a multiple
  /// of 1024.
  ///
  UINT32                            TotalSize;
  ///
  /// Reserved fields for future expansion.
  ///
  UINT8                             Reserved[12];
} CPU_MICROCODE_HEADER;

///
/// Extended Signature Table Header Field Definitions
///
typedef struct {
  ///
  /// Specifies the number of extended signature structures (Processor
  /// Signature[n], processor flags[n] and checksum[n]) that exist in this
  /// microcode update
  ///
  UINT32                            ExtendedSignatureCount;
  ///
  /// Checksum of update extended processor signature table. Used to
  /// verify the integrity of the extended processor signature table.
  /// Checksum is correct when the summation of the DWORDs that
  /// comprise the extended processor signature table results in
  /// 00000000H.
  ///
  UINT32                            ExtendedChecksum;
  ///
  /// Reserved fields.
  ///
  UINT8                             Reserved[12];
} CPU_MICROCODE_EXTENDED_TABLE_HEADER;

///
/// Extended Signature Table Field Definitions
///
typedef struct {
  ///
  /// Extended family, extended model, type, family, model, and stepping
  /// of processor that requires this particular update revision (e.g.,
  /// 00000650H). Each microcode update is designed specifically for a
  /// given extended family, extended model, type, family, model, and
  /// stepping of the processor.
  /// The BIOS uses the processor signature field in conjunction with the
  /// CPUID instruction to determine whether or not an update is
  /// appropriate to load on a processor. The information encoded within
  /// this field exactly corresponds to the bit representations returned by
  /// the CPUID instruction.
  ///
  CPU_MICROCODE_PROCESSOR_SIGNATURE ProcessorSignature;
  ///
  /// Platform type information is encoded in the lower 8 bits of this 4-
  /// byte field. Each bit represents a particular platform type for a given
  /// CPUID. The BIOS uses the processor flags field in conjunction with
  /// the platform Id bits in MSR (17H) to determine whether or not an
  /// update is appropriate to load on a processor. Multiple bits may be set
  /// representing support for multiple platform IDs.
  ///
  UINT32                             ProcessorFlag;
  ///
  /// Used by utility software to decompose a microcode update into
  /// multiple microcode updates where each of the new updates is
  /// constructed without the optional Extended Processor Signature
  /// Table.
  /// To calculate the Checksum, substitute the Primary Processor
  /// Signature entry and the Processor Flags entry with the
  /// corresponding Extended Patch entry. Delete the Extended Processor
  /// Signature Table entries. The Checksum is correct when the
  /// summation of all DWORDs that comprise the created Extended
  /// Processor Patch results in 00000000H.
  ///
  UINT32                             Checksum;
} CPU_MICROCODE_EXTENDED_TABLE;

#pragma pack ()

#endif
