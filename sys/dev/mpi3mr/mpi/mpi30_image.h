/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2016-2023, Broadcom Inc. All rights reserved.
 * Support: <fbsd-storage-driver.pdl@broadcom.com>
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
 *
 */

#ifndef MPI30_IMAGE_H
#define MPI30_IMAGE_H     1

/* Component Image Version */
typedef struct _MPI3_COMP_IMAGE_VERSION
{
    U16     BuildNum;            /* 0x00 */
    U16     CustomerID;          /* 0x02 */
    U8      PhaseMinor;          /* 0x04 */
    U8      PhaseMajor;          /* 0x05 */
    U8      GenMinor;            /* 0x06 */
    U8      GenMajor;            /* 0x07 */
} MPI3_COMP_IMAGE_VERSION, MPI3_POINTER PTR_MPI3_COMP_IMAGE_VERSION,
  Mpi3CompImageVersion_t, MPI3_POINTER pMpi3CompImageVersion_t;

/* Hash Exclusion Format */
typedef struct _MPI3_HASH_EXCLUSION_FORMAT
{
    U32                     Offset;        /* 0x00 */
    U32                     Size;          /* 0x04 */
} MPI3_HASH_EXCLUSION_FORMAT, MPI3_POINTER PTR_MPI3_HASH_EXCLUSION_FORMAT,
Mpi3HashSxclusionFormat_t, MPI3_POINTER pMpi3HashExclusionFormat_t;

#define MPI3_IMAGE_HASH_EXCUSION_NUM                           (4)

/* FW Image Header */
typedef struct _MPI3_COMPONENT_IMAGE_HEADER
{
    U32                            Signature0;                                      /* 0x00 */
    U32                            LoadAddress;                                     /* 0x04 */
    U32                            DataSize;                                        /* 0x08 */
    U32                            StartOffset;                                     /* 0x0C */
    U32                            Signature1;                                      /* 0x10 */
    U32                            FlashOffset;                                     /* 0x14 */
    U32                            ImageSize;                                       /* 0x18 */
    U32                            VersionStringOffset;                             /* 0x1C */
    U32                            BuildDateStringOffset;                           /* 0x20 */
    U32                            BuildTimeStringOffset;                           /* 0x24 */
    U32                            EnvironmentVariableOffset;                       /* 0x28 */
    U32                            ApplicationSpecific;                             /* 0x2C */
    U32                            Signature2;                                      /* 0x30 */
    U32                            HeaderSize;                                      /* 0x34 */
    U32                            Crc;                                             /* 0x38 */
    U32                            Flags;                                           /* 0x3C */
    U32                            SecondaryFlashOffset;                            /* 0x40 */
    U32                            ETPOffset;                                       /* 0x44 */
    U32                            ETPSize;                                         /* 0x48 */
    MPI3_VERSION_UNION             RMCInterfaceVersion;                             /* 0x4C */
    MPI3_VERSION_UNION             ETPInterfaceVersion;                             /* 0x50 */
    MPI3_COMP_IMAGE_VERSION        ComponentImageVersion;                           /* 0x54 */
    MPI3_HASH_EXCLUSION_FORMAT     HashExclusion[MPI3_IMAGE_HASH_EXCUSION_NUM];     /* 0x5C */
    U32                            NextImageHeaderOffset;                           /* 0x7C */
    MPI3_VERSION_UNION             SecurityVersion;                                 /* 0x80 */
    U32                            Reserved84[31];                                  /* 0x84 -- 0xFC */
} MPI3_COMPONENT_IMAGE_HEADER, MPI3_POINTER PTR_MPI3_COMPONENT_IMAGE_HEADER,
  Mpi3ComponentImageHeader_t, MPI3_POINTER pMpi3ComponentImageHeader_t;


/**** Definitions for Signature0 field ****/
#define MPI3_IMAGE_HEADER_SIGNATURE0_MPI3                     (0xEB00003E)

/**** Definitions for LoadAddress field ****/
#define MPI3_IMAGE_HEADER_LOAD_ADDRESS_INVALID                (0x00000000)

/**** Definitions for Signature1 field ****/
#define MPI3_IMAGE_HEADER_SIGNATURE1_APPLICATION              (0x20505041)  /* string "APP "  */
#define MPI3_IMAGE_HEADER_SIGNATURE1_FIRST_MUTABLE            (0x20434D46)  /* string "FMC "  */
#define MPI3_IMAGE_HEADER_SIGNATURE1_BSP                      (0x20505342)  /* string "BSP "  */
#define MPI3_IMAGE_HEADER_SIGNATURE1_ROM_BIOS                 (0x534F4942)  /* string "BIOS"  */
#define MPI3_IMAGE_HEADER_SIGNATURE1_HII_X64                  (0x4D494948)  /* string "HIIM"  */
#define MPI3_IMAGE_HEADER_SIGNATURE1_HII_ARM                  (0x41494948)  /* string "HIIA"  */
#define MPI3_IMAGE_HEADER_SIGNATURE1_CPLD                     (0x444C5043)  /* string "CPLD"  */
#define MPI3_IMAGE_HEADER_SIGNATURE1_SPD                      (0x20445053)  /* string "SPD "  */
#define MPI3_IMAGE_HEADER_SIGNATURE1_GAS_GAUGE                (0x20534147)  /* string "GAS "  */
#define MPI3_IMAGE_HEADER_SIGNATURE1_PBLP                     (0x504C4250)  /* string "PBLP"  */
#define MPI3_IMAGE_HEADER_SIGNATURE1_MANIFEST                 (0x464E414D)  /* string "MANF"  */
#define MPI3_IMAGE_HEADER_SIGNATURE1_OEM                      (0x204D454F)  /* string "OEM "  */
#define MPI3_IMAGE_HEADER_SIGNATURE1_RMC                      (0x20434D52)  /* string "RMC "  */
#define MPI3_IMAGE_HEADER_SIGNATURE1_SMM                      (0x204D4D53)  /* string "SMM "  */
#define MPI3_IMAGE_HEADER_SIGNATURE1_PSW                      (0x20575350)  /* string "PSW "  */


/**** Definitions for Signature2 field ****/
#define MPI3_IMAGE_HEADER_SIGNATURE2_VALUE                    (0x50584546)

/**** Definitions for Flags field ****/
#define MPI3_IMAGE_HEADER_FLAGS_DEVICE_KEY_BASIS_MASK         (0x00000030)
#define MPI3_IMAGE_HEADER_FLAGS_DEVICE_KEY_BASIS_CDI          (0x00000000)
#define MPI3_IMAGE_HEADER_FLAGS_DEVICE_KEY_BASIS_DI           (0x00000010)
#define MPI3_IMAGE_HEADER_FLAGS_SIGNED_NVDATA                 (0x00000008)
#define MPI3_IMAGE_HEADER_FLAGS_REQUIRES_ACTIVATION           (0x00000004)
#define MPI3_IMAGE_HEADER_FLAGS_COMPRESSED                    (0x00000002)
#define MPI3_IMAGE_HEADER_FLAGS_FLASH                         (0x00000001)


/**** Offsets for Image Header Fields ****/
#define MPI3_IMAGE_HEADER_SIGNATURE0_OFFSET                   (0x00)
#define MPI3_IMAGE_HEADER_LOAD_ADDRESS_OFFSET                 (0x04)
#define MPI3_IMAGE_HEADER_DATA_SIZE_OFFSET                    (0x08)
#define MPI3_IMAGE_HEADER_START_OFFSET_OFFSET                 (0x0C)
#define MPI3_IMAGE_HEADER_SIGNATURE1_OFFSET                   (0x10)
#define MPI3_IMAGE_HEADER_FLASH_OFFSET_OFFSET                 (0x14)
#define MPI3_IMAGE_HEADER_FLASH_SIZE_OFFSET                   (0x18)
#define MPI3_IMAGE_HEADER_VERSION_STRING_OFFSET_OFFSET        (0x1C)
#define MPI3_IMAGE_HEADER_BUILD_DATE_STRING_OFFSET_OFFSET     (0x20)
#define MPI3_IMAGE_HEADER_BUILD_TIME_OFFSET_OFFSET            (0x24)
#define MPI3_IMAGE_HEADER_ENVIROMENT_VAR_OFFSET_OFFSET        (0x28)
#define MPI3_IMAGE_HEADER_APPLICATION_SPECIFIC_OFFSET         (0x2C)
#define MPI3_IMAGE_HEADER_SIGNATURE2_OFFSET                   (0x30)
#define MPI3_IMAGE_HEADER_HEADER_SIZE_OFFSET                  (0x34)
#define MPI3_IMAGE_HEADER_CRC_OFFSET                          (0x38)
#define MPI3_IMAGE_HEADER_FLAGS_OFFSET                        (0x3C)
#define MPI3_IMAGE_HEADER_SECONDARY_FLASH_OFFSET_OFFSET       (0x40)
#define MPI3_IMAGE_HEADER_ETP_OFFSET_OFFSET                   (0x44)
#define MPI3_IMAGE_HEADER_ETP_SIZE_OFFSET                     (0x48)
#define MPI3_IMAGE_HEADER_RMC_INTERFACE_VER_OFFSET            (0x4C)
#define MPI3_IMAGE_HEADER_ETP_INTERFACE_VER_OFFSET            (0x50)
#define MPI3_IMAGE_HEADER_COMPONENT_IMAGE_VER_OFFSET          (0x54)
#define MPI3_IMAGE_HEADER_HASH_EXCLUSION_OFFSET               (0x5C)
#define MPI3_IMAGE_HEADER_NEXT_IMAGE_HEADER_OFFSET_OFFSET     (0x7C)


#define MPI3_IMAGE_HEADER_SIZE                                (0x100)



/*****************************************************************************
 *              Component Image Data                                         *
 *****************************************************************************/

/* Package Manifest Data */

#ifndef MPI3_CI_MANIFEST_MPI_MAX
#define MPI3_CI_MANIFEST_MPI_MAX                               (1)
#endif  /* MPI3_CI_MANIFEST_MPI_MAX */

typedef struct _MPI3_CI_MANIFEST_MPI_COMP_IMAGE_REF
{
    U32                                Signature1;                                    /* 0x00 */
    U32                                Reserved04[3];                                 /* 0x04 */
    MPI3_COMP_IMAGE_VERSION            ComponentImageVersion;                         /* 0x10 */
    U32                                ComponentImageVersionStringOffset;             /* 0x18 */
    U32                                CRC;                                           /* 0x1C */
} MPI3_CI_MANIFEST_MPI_COMP_IMAGE_REF, MPI3_POINTER PTR_MPI3_CI_MANIFEST_MPI_COMP_IMAGE_REF,
  Mpi3CIManifestMpiCompImageRef_t, MPI3_POINTER pMpi3CIManifestMpiCompImageRef_t;

typedef struct _MPI3_CI_MANIFEST_MPI
{
    U8                                    ManifestType;                                  /* 0x00 */
    U8                                    Reserved01[3];                                 /* 0x01 */
    U32                                   Reserved04[3];                                 /* 0x04 */
    U8                                    NumImageReferences;                            /* 0x10 */
    U8                                    ReleaseLevel;                                  /* 0x11 */
    U16                                   Reserved12;                                    /* 0x12 */
    U16                                   Reserved14;                                    /* 0x14 */
    U16                                   Flags;                                         /* 0x16 */
    U32                                   Reserved18[2];                                 /* 0x18 */
    U16                                   VendorID;                                      /* 0x20 */
    U16                                   DeviceID;                                      /* 0x22 */
    U16                                   SubsystemVendorID;                             /* 0x24 */
    U16                                   SubsystemID;                                   /* 0x26 */
    U32                                   Reserved28[2];                                 /* 0x28 */
    MPI3_VERSION_UNION                    PackageSecurityVersion;                        /* 0x30 */
    U32                                   Reserved34;                                    /* 0x34 */
    MPI3_COMP_IMAGE_VERSION               PackageVersion;                                /* 0x38 */
    U32                                   PackageVersionStringOffset;                    /* 0x40 */
    U32                                   PackageBuildDateStringOffset;                  /* 0x44 */
    U32                                   PackageBuildTimeStringOffset;                  /* 0x48 */
    U32                                   Reserved4C;                                    /* 0x4C */
    U32                                   DiagAuthorizationIdentifier[16];               /* 0x50 */
    MPI3_CI_MANIFEST_MPI_COMP_IMAGE_REF   ComponentImageRef[MPI3_CI_MANIFEST_MPI_MAX];   /* 0x90 */    /* variable length */
    /*                                    StringData - offset of this field must be calculated   */    /* variable length */
} MPI3_CI_MANIFEST_MPI, MPI3_POINTER PTR_MPI3_CI_MANIFEST_MPI,
  Mpi3CIManifestMpi_t, MPI3_POINTER pMpi3CIManifestMpi_t;

/* defines for the ReleaseLevel field */
#define MPI3_CI_MANIFEST_MPI_RELEASE_LEVEL_DEV                        (0x00)
#define MPI3_CI_MANIFEST_MPI_RELEASE_LEVEL_PREALPHA                   (0x10)
#define MPI3_CI_MANIFEST_MPI_RELEASE_LEVEL_ALPHA                      (0x20)
#define MPI3_CI_MANIFEST_MPI_RELEASE_LEVEL_BETA                       (0x30)
#define MPI3_CI_MANIFEST_MPI_RELEASE_LEVEL_RC                         (0x40)
#define MPI3_CI_MANIFEST_MPI_RELEASE_LEVEL_GCA                        (0x50)
#define MPI3_CI_MANIFEST_MPI_RELEASE_LEVEL_POINT                      (0x60)

/* defines for the Flags field */
#define MPI3_CI_MANIFEST_MPI_FLAGS_DIAG_AUTHORIZATION                 (0x01)

/* defines for the SubsystemID field */
#define MPI3_CI_MANIFEST_MPI_SUBSYSTEMID_IGNORED                   (0xFFFF)

/* defines for the PackageVersionStringOffset field */
#define MPI3_CI_MANIFEST_MPI_PKG_VER_STR_OFF_UNSPECIFIED           (0x00000000)

/* defines for the PackageBuildDateStringOffset field */
#define MPI3_CI_MANIFEST_MPI_PKG_BUILD_DATE_STR_OFF_UNSPECIFIED    (0x00000000)

/* defines for the PackageBuildTimeStringOffset field */
#define MPI3_CI_MANIFEST_MPI_PKG_BUILD_TIME_STR_OFF_UNSPECIFIED    (0x00000000)

typedef union _MPI3_CI_MANIFEST
{
    MPI3_CI_MANIFEST_MPI               Mpi;
    U32                                Dword[1];
} MPI3_CI_MANIFEST, MPI3_POINTER PTR_MPI3_CI_MANIFEST,
  Mpi3CIManifest_t, MPI3_POINTER pMpi3CIManifest_t;

/* defines for ManifestType field */
#define MPI3_CI_MANIFEST_TYPE_MPI                                  (0x00)


/*****************************************************************************
 *              Extended Image Data                                          *
 *****************************************************************************/

/* Extended Image Header */
typedef struct _MPI3_EXTENDED_IMAGE_HEADER
{
    U8                             ImageType;                   /* 0x00 */
    U8                             Reserved01[3];               /* 0x01 */
    U32                            Checksum;                    /* 0x04 */
    U32                            ImageSize;                   /* 0x08 */
    U32                            NextImageHeaderOffset;       /* 0x0C */
    U32                            Reserved10[4];               /* 0x10 */
    U32                            IdentifyString[8];           /* 0x20 */
} MPI3_EXTENDED_IMAGE_HEADER, MPI3_POINTER PTR_MPI3_EXTENDED_IMAGE_HEADER,
  Mpi3ExtendedImageHeader_t, MPI3_POINTER pMpi3ExtendedImageHeader_t;

/* useful offsets */
#define MPI3_EXT_IMAGE_IMAGETYPE_OFFSET         (0x00)
#define MPI3_EXT_IMAGE_IMAGESIZE_OFFSET         (0x08)
#define MPI3_EXT_IMAGE_NEXTIMAGE_OFFSET         (0x0C)

#define MPI3_EXT_IMAGE_HEADER_SIZE              (0x40)

/* defines for the ImageType field */
#define MPI3_EXT_IMAGE_TYPE_UNSPECIFIED             (0x00)
#define MPI3_EXT_IMAGE_TYPE_NVDATA                  (0x03)
#define MPI3_EXT_IMAGE_TYPE_SUPPORTED_DEVICES       (0x07)
#define MPI3_EXT_IMAGE_TYPE_ENCRYPTED_HASH          (0x09)
#define MPI3_EXT_IMAGE_TYPE_RDE                     (0x0A)
#define MPI3_EXT_IMAGE_TYPE_AUXILIARY_PROCESSOR     (0x0B)
#define MPI3_EXT_IMAGE_TYPE_MIN_PRODUCT_SPECIFIC    (0x80)
#define MPI3_EXT_IMAGE_TYPE_MAX_PRODUCT_SPECIFIC    (0xFF)


/* Supported Device Data Format */
typedef struct _MPI3_SUPPORTED_DEVICE
{
    U16                     DeviceID;                   /* 0x00 */
    U16                     VendorID;                   /* 0x02 */
    U16                     DeviceIDMask;               /* 0x04 */
    U16                     Reserved06;                 /* 0x06 */
    U8                      LowPCIRev;                  /* 0x08 */
    U8                      HighPCIRev;                 /* 0x09 */
    U16                     Reserved0A;                 /* 0x0A */
    U32                     Reserved0C;                 /* 0x0C */
} MPI3_SUPPORTED_DEVICE, MPI3_POINTER PTR_MPI3_SUPPORTED_DEVICE,
  Mpi3SupportedDevice_t, MPI3_POINTER pMpi3SupportedDevice_t;

#ifndef MPI3_SUPPORTED_DEVICE_MAX
#define MPI3_SUPPORTED_DEVICE_MAX                      (1)
#endif  /* MPI3_SUPPORTED_DEVICE_MAX */

/* Supported Devices Extended Image Data */
typedef struct _MPI3_SUPPORTED_DEVICES_DATA
{
    U8                      ImageVersion;                                   /* 0x00 */
    U8                      Reserved01;                                     /* 0x01 */
    U8                      NumDevices;                                     /* 0x02 */
    U8                      Reserved03;                                     /* 0x03 */
    U32                     Reserved04;                                     /* 0x04 */
    MPI3_SUPPORTED_DEVICE   SupportedDevice[MPI3_SUPPORTED_DEVICE_MAX];     /* 0x08 */    /* variable length */
} MPI3_SUPPORTED_DEVICES_DATA, MPI3_POINTER PTR_MPI3_SUPPORTED_DEVICES_DATA,
  Mpi3SupportedDevicesData_t, MPI3_POINTER pMpi3SupportedDevicesData_t;

#ifndef MPI3_ENCRYPTED_HASH_MAX
#define MPI3_ENCRYPTED_HASH_MAX                      (1)
#endif  /* MPI3_ENCRYPTED_HASH_MAX */

/* Encrypted Hash Entry Format */
typedef struct _MPI3_ENCRYPTED_HASH_ENTRY
{
    U8                      HashImageType;                                  /* 0x00 */
    U8                      HashAlgorithm;                                  /* 0x01 */
    U8                      EncryptionAlgorithm;                            /* 0x02 */
    U8                      Reserved03;                                     /* 0x03 */
    U32                     Reserved04;                                     /* 0x04 */
    U32                     EncryptedHash[MPI3_ENCRYPTED_HASH_MAX];         /* 0x08 */   /* variable length */
} MPI3_ENCRYPTED_HASH_ENTRY, MPI3_POINTER PTR_MPI3_ENCRYPTED_HASH_ENTRY,
  Mpi3EncryptedHashEntry_t, MPI3_POINTER pMpi3EncryptedHashEntry_t;


/* defines for the HashImageType field */
#define MPI3_HASH_IMAGE_TYPE_KEY_WITH_SIGNATURE      (0x03)

/* defines for the HashAlgorithm field */
#define MPI3_HASH_ALGORITHM_VERSION_MASK             (0xE0)
#define MPI3_HASH_ALGORITHM_VERSION_NONE             (0x00)
#define MPI3_HASH_ALGORITHM_VERSION_SHA1             (0x20)   /* Obsolete */
#define MPI3_HASH_ALGORITHM_VERSION_SHA2             (0x40)
#define MPI3_HASH_ALGORITHM_VERSION_SHA3             (0x60)

#define MPI3_HASH_ALGORITHM_SIZE_MASK                (0x1F)
#define MPI3_HASH_ALGORITHM_SIZE_UNUSED              (0x00)
#define MPI3_HASH_ALGORITHM_SIZE_SHA256              (0x01)
#define MPI3_HASH_ALGORITHM_SIZE_SHA512              (0x02)
#define MPI3_HASH_ALGORITHM_SIZE_SHA384              (0x03)

/* defines for the EncryptionAlgorithm field */
#define MPI3_ENCRYPTION_ALGORITHM_UNUSED             (0x00)
#define MPI3_ENCRYPTION_ALGORITHM_RSA256             (0x01)   /* Obsolete */
#define MPI3_ENCRYPTION_ALGORITHM_RSA512             (0x02)   /* Obsolete */
#define MPI3_ENCRYPTION_ALGORITHM_RSA1024            (0x03)   /* Obsolete */
#define MPI3_ENCRYPTION_ALGORITHM_RSA2048            (0x04)
#define MPI3_ENCRYPTION_ALGORITHM_RSA4096            (0x05)
#define MPI3_ENCRYPTION_ALGORITHM_RSA3072            (0x06)
#define MPI3_ENCRYPTION_ALGORITHM_ECDSA_P256         (0x07)   /* NIST secp256r1 curve */
#define MPI3_ENCRYPTION_ALGORITHM_ECDSA_P384         (0x08)   /* NIST secp384r1 curve */
#define MPI3_ENCRYPTION_ALGORITHM_ECDSA_P521         (0x09)   /* NIST secp521r1 curve */


#ifndef MPI3_PUBLIC_KEY_MAX
#define MPI3_PUBLIC_KEY_MAX                          (1)
#endif  /* MPI3_PUBLIC_KEY_MAX */

/* Encrypted Key with Hash Entry Format */
typedef struct _MPI3_ENCRYPTED_KEY_WITH_HASH_ENTRY
{
    U8                      HashImageType;                                  /* 0x00 */
    U8                      HashAlgorithm;                                  /* 0x01 */
    U8                      EncryptionAlgorithm;                            /* 0x02 */
    U8                      Reserved03;                                     /* 0x03 */
    U32                     Reserved04;                                     /* 0x04 */
    U32                     PublicKey[MPI3_PUBLIC_KEY_MAX];                 /* 0x08 */     /* variable length */
    /*                      EncryptedHash - offset of this field must be calculated */     /* variable length */
} MPI3_ENCRYPTED_KEY_WITH_HASH_ENTRY, MPI3_POINTER PTR_MPI3_ENCRYPTED_KEY_WITH_HASH_ENTRY,
  Mpi3EncryptedKeyWithHashEntry_t, MPI3_POINTER pMpi3EncryptedKeyWithHashEntry_t;

#ifndef MPI3_ENCRYPTED_HASH_ENTRY_MAX
#define MPI3_ENCRYPTED_HASH_ENTRY_MAX               (1)
#endif  /* MPI3_ENCRYPTED_HASH_ENTRY_MAX */

/* Encrypted Hash Image Data */
typedef struct _MPI3_ENCRYPTED_HASH_DATA
{
    U8                               ImageVersion;                                          /* 0x00 */
    U8                               NumHash;                                               /* 0x01 */
    U16                              Reserved02;                                            /* 0x02 */
    U32                              Reserved04;                                            /* 0x04 */
    MPI3_ENCRYPTED_HASH_ENTRY        EncryptedHashEntry[MPI3_ENCRYPTED_HASH_ENTRY_MAX];     /* 0x08 */   /* variable length */
} MPI3_ENCRYPTED_HASH_DATA, MPI3_POINTER PTR_MPI3_ENCRYPTED_HASH_DATA,
  Mpi3EncryptedHashData_t, MPI3_POINTER pMpi3EncryptedHashData_t;


#ifndef MPI3_AUX_PROC_DATA_MAX
#define MPI3_AUX_PROC_DATA_MAX               (1)
#endif  /* MPI3_ENCRYPTED_HASH_ENTRY_MAX */

/* Auxiliary Processor Extended Image Data */
typedef struct _MPI3_AUX_PROCESSOR_DATA
{
    U8                      BootMethod;                               /* 0x00 */
    U8                      NumLoadAddr;                              /* 0x01 */
    U8                      Reserved02;                               /* 0x02 */
    U8                      Type;                                     /* 0x03 */
    U32                     Version;                                  /* 0x04 */
    U32                     LoadAddress[8];                           /* 0x08 */
    U32                     Reserved28[22];                           /* 0x28 */
    U32                     AuxProcessorData[MPI3_AUX_PROC_DATA_MAX]; /* 0x80 */   /* variable length */
} MPI3_AUX_PROCESSOR_DATA, MPI3_POINTER PTR_MPI3_AUX_PROCESSOR_DATA,
  Mpi3AuxProcessorData_t, MPI3_POINTER pMpi3AuxProcessorData_t;

#define MPI3_AUX_PROC_DATA_OFFSET                                     (0x80)

/* defines for the BootMethod field */
#define MPI3_AUXPROCESSOR_BOOT_METHOD_MO_MSG                          (0x00)
#define MPI3_AUXPROCESSOR_BOOT_METHOD_MO_DOORBELL                     (0x01)
#define MPI3_AUXPROCESSOR_BOOT_METHOD_COMPONENT                       (0x02)

/* defines for the Type field */
#define MPI3_AUXPROCESSOR_TYPE_ARM_A15                                (0x00)
#define MPI3_AUXPROCESSOR_TYPE_ARM_M0                                 (0x01)
#define MPI3_AUXPROCESSOR_TYPE_ARM_R4                                 (0x02)

#endif /* MPI30_IMAGE_H */
