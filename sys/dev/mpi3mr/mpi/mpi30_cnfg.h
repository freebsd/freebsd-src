/*
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2016-2025, Broadcom Inc. All rights reserved.
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

#ifndef MPI30_CNFG_H
#define MPI30_CNFG_H     1

/*****************************************************************************
 *              Configuration Page Types                                     *
 ****************************************************************************/
#define MPI3_CONFIG_PAGETYPE_IO_UNIT                    (0x00)
#define MPI3_CONFIG_PAGETYPE_MANUFACTURING              (0x01)
#define MPI3_CONFIG_PAGETYPE_IOC                        (0x02)
#define MPI3_CONFIG_PAGETYPE_DRIVER                     (0x03)
#define MPI3_CONFIG_PAGETYPE_SECURITY                   (0x04)
#define MPI3_CONFIG_PAGETYPE_ENCLOSURE                  (0x11)
#define MPI3_CONFIG_PAGETYPE_DEVICE                     (0x12)
#define MPI3_CONFIG_PAGETYPE_SAS_IO_UNIT                (0x20)
#define MPI3_CONFIG_PAGETYPE_SAS_EXPANDER               (0x21)
#define MPI3_CONFIG_PAGETYPE_SAS_PHY                    (0x23)
#define MPI3_CONFIG_PAGETYPE_SAS_PORT                   (0x24)
#define MPI3_CONFIG_PAGETYPE_PCIE_IO_UNIT               (0x30)
#define MPI3_CONFIG_PAGETYPE_PCIE_SWITCH                (0x31)
#define MPI3_CONFIG_PAGETYPE_PCIE_LINK                  (0x33)

/*****************************************************************************
 *              Configuration Page Attributes                                *
 ****************************************************************************/
#define MPI3_CONFIG_PAGEATTR_MASK                       (0xF0)
#define MPI3_CONFIG_PAGEATTR_SHIFT                      (4)
#define MPI3_CONFIG_PAGEATTR_READ_ONLY                  (0x00)
#define MPI3_CONFIG_PAGEATTR_CHANGEABLE                 (0x10)
#define MPI3_CONFIG_PAGEATTR_PERSISTENT                 (0x20)

/*****************************************************************************
 *              Configuration Page Actions                                   *
 ****************************************************************************/
#define MPI3_CONFIG_ACTION_PAGE_HEADER                  (0x00)
#define MPI3_CONFIG_ACTION_READ_DEFAULT                 (0x01)
#define MPI3_CONFIG_ACTION_READ_CURRENT                 (0x02)
#define MPI3_CONFIG_ACTION_WRITE_CURRENT                (0x03)
#define MPI3_CONFIG_ACTION_READ_PERSISTENT              (0x04)
#define MPI3_CONFIG_ACTION_WRITE_PERSISTENT             (0x05)

/*****************************************************************************
 *              Configuration Page Addressing                                *
 ****************************************************************************/

/**** Device PageAddress Format ****/
#define MPI3_DEVICE_PGAD_FORM_MASK                      (0xF0000000)
#define MPI3_DEVICE_PGAD_FORM_SHIFT                     (28)
#define MPI3_DEVICE_PGAD_FORM_GET_NEXT_HANDLE           (0x00000000)
#define MPI3_DEVICE_PGAD_FORM_HANDLE                    (0x20000000)
#define MPI3_DEVICE_PGAD_HANDLE_MASK                    (0x0000FFFF)
#define MPI3_DEVICE_PGAD_HANDLE_SHIFT                   (0)

/**** SAS Expander PageAddress Format ****/
#define MPI3_SAS_EXPAND_PGAD_FORM_MASK                  (0xF0000000)
#define MPI3_SAS_EXPAND_PGAD_FORM_SHIFT                 (28)
#define MPI3_SAS_EXPAND_PGAD_FORM_GET_NEXT_HANDLE       (0x00000000)
#define MPI3_SAS_EXPAND_PGAD_FORM_HANDLE_PHY_NUM        (0x10000000)
#define MPI3_SAS_EXPAND_PGAD_FORM_HANDLE                (0x20000000)
#define MPI3_SAS_EXPAND_PGAD_PHYNUM_MASK                (0x00FF0000)
#define MPI3_SAS_EXPAND_PGAD_PHYNUM_SHIFT               (16)
#define MPI3_SAS_EXPAND_PGAD_HANDLE_MASK                (0x0000FFFF)
#define MPI3_SAS_EXPAND_PGAD_HANDLE_SHIFT               (0)

/**** SAS Phy PageAddress Format ****/
#define MPI3_SAS_PHY_PGAD_FORM_MASK                     (0xF0000000)
#define MPI3_SAS_PHY_PGAD_FORM_SHIFT                    (28)
#define MPI3_SAS_PHY_PGAD_FORM_PHY_NUMBER               (0x00000000)
#define MPI3_SAS_PHY_PGAD_PHY_NUMBER_MASK               (0x000000FF)
#define MPI3_SAS_PHY_PGAD_PHY_NUMBER_SHIFT              (0)

/**** SAS Port PageAddress Format ****/
#define MPI3_SASPORT_PGAD_FORM_MASK                     (0xF0000000)
#define MPI3_SASPORT_PGAD_FORM_SHIFT                    (28)
#define MPI3_SASPORT_PGAD_FORM_GET_NEXT_PORT            (0x00000000)
#define MPI3_SASPORT_PGAD_FORM_PORT_NUM                 (0x10000000)
#define MPI3_SASPORT_PGAD_PORT_NUMBER_MASK              (0x000000FF)
#define MPI3_SASPORT_PGAD_PORT_NUMBER_SHIFT             (0)

/**** Enclosure PageAddress Format ****/
#define MPI3_ENCLOS_PGAD_FORM_MASK                      (0xF0000000)
#define MPI3_ENCLOS_PGAD_FORM_SHIFT                     (28)
#define MPI3_ENCLOS_PGAD_FORM_GET_NEXT_HANDLE           (0x00000000)
#define MPI3_ENCLOS_PGAD_FORM_HANDLE                    (0x10000000)
#define MPI3_ENCLOS_PGAD_HANDLE_MASK                    (0x0000FFFF)
#define MPI3_ENCLOS_PGAD_HANDLE_SHIFT                   (0)

/**** PCIe Switch PageAddress Format ****/
#define MPI3_PCIE_SWITCH_PGAD_FORM_MASK                 (0xF0000000)
#define MPI3_PCIE_SWITCH_PGAD_FORM_SHIFT                (28)
#define MPI3_PCIE_SWITCH_PGAD_FORM_GET_NEXT_HANDLE      (0x00000000)
#define MPI3_PCIE_SWITCH_PGAD_FORM_HANDLE_PORT_NUM      (0x10000000)
#define MPI3_PCIE_SWITCH_PGAD_FORM_HANDLE               (0x20000000)
#define MPI3_PCIE_SWITCH_PGAD_PORTNUM_MASK              (0x00FF0000)
#define MPI3_PCIE_SWITCH_PGAD_PORTNUM_SHIFT             (16)
#define MPI3_PCIE_SWITCH_PGAD_HANDLE_MASK               (0x0000FFFF)
#define MPI3_PCIE_SWITCH_PGAD_HANDLE_SHIFT              (0)

/**** PCIe Link PageAddress Format ****/
#define MPI3_PCIE_LINK_PGAD_FORM_MASK                   (0xF0000000)
#define MPI3_PCIE_LINK_PGAD_FORM_SHIFT                  (28)
#define MPI3_PCIE_LINK_PGAD_FORM_GET_NEXT_LINK          (0x00000000)
#define MPI3_PCIE_LINK_PGAD_FORM_LINK_NUM               (0x10000000)
#define MPI3_PCIE_LINK_PGAD_LINKNUM_MASK                (0x000000FF)
#define MPI3_PCIE_LINK_PGAD_LINKNUM_SHIFT               (0)

/**** Security PageAddress Format ****/
#define MPI3_SECURITY_PGAD_FORM_MASK                    (0xF0000000)
#define MPI3_SECURITY_PGAD_FORM_SHIFT                   (28)
#define MPI3_SECURITY_PGAD_FORM_GET_NEXT_SLOT           (0x00000000)
#define MPI3_SECURITY_PGAD_FORM_SLOT_NUM                (0x10000000)
#define MPI3_SECURITY_PGAD_SLOT_GROUP_MASK              (0x0000FF00)
#define MPI3_SECURITY_PGAD_SLOT_GROUP_SHIFT             (8)
#define MPI3_SECURITY_PGAD_SLOT_MASK                    (0x000000FF)
#define MPI3_SECURITY_PGAD_SLOT_SHIFT                   (0)

/**** Instance PageAddress Format ****/
#define MPI3_INSTANCE_PGAD_INSTANCE_MASK                (0x0000FFFF)
#define MPI3_INSTANCE_PGAD_INSTANCE_SHIFT               (0)


/*****************************************************************************
 *              Configuration Request Message                                *
 ****************************************************************************/
typedef struct _MPI3_CONFIG_REQUEST
{
    U16             HostTag;                            /* 0x00 */
    U8              IOCUseOnly02;                       /* 0x02 */
    U8              Function;                           /* 0x03 */
    U16             IOCUseOnly04;                       /* 0x04 */
    U8              IOCUseOnly06;                       /* 0x06 */
    U8              MsgFlags;                           /* 0x07 */
    U16             ChangeCount;                        /* 0x08 */
    U8              ProxyIOCNumber;                     /* 0x0A */
    U8              Reserved0B;                         /* 0x0B */
    U8              PageVersion;                        /* 0x0C */
    U8              PageNumber;                         /* 0x0D */
    U8              PageType;                           /* 0x0E */
    U8              Action;                             /* 0x0F */
    U32             PageAddress;                        /* 0x10 */
    U16             PageLength;                         /* 0x14 */
    U16             Reserved16;                         /* 0x16 */
    U32             Reserved18[2];                      /* 0x18 */
    MPI3_SGE_UNION  SGL;                                /* 0x20 */
} MPI3_CONFIG_REQUEST, MPI3_POINTER PTR_MPI3_CONFIG_REQUEST,
  Mpi3ConfigRequest_t, MPI3_POINTER pMpi3ConfigRequest_t;

/*****************************************************************************
 *              Configuration Pages                                          *
 ****************************************************************************/

/*****************************************************************************
 *              Configuration Page Header                                    *
 ****************************************************************************/
typedef struct _MPI3_CONFIG_PAGE_HEADER
{
    U8              PageVersion;                        /* 0x00 */
    U8              Reserved01;                         /* 0x01 */
    U8              PageNumber;                         /* 0x02 */
    U8              PageAttribute;                      /* 0x03 */
    U16             PageLength;                         /* 0x04 */
    U8              PageType;                           /* 0x06 */
    U8              Reserved07;                         /* 0x07 */
} MPI3_CONFIG_PAGE_HEADER, MPI3_POINTER PTR_MPI3_CONFIG_PAGE_HEADER,
  Mpi3ConfigPageHeader_t, MPI3_POINTER pMpi3ConfigPageHeader_t;

/*****************************************************************************
 *              Common definitions used by Configuration Pages           *
 ****************************************************************************/

/**** Defines for NegotiatedLinkRates ****/
#define MPI3_SAS_NEG_LINK_RATE_LOGICAL_MASK                   (0xF0)
#define MPI3_SAS_NEG_LINK_RATE_LOGICAL_SHIFT                  (4)
#define MPI3_SAS_NEG_LINK_RATE_PHYSICAL_MASK                  (0x0F)
#define MPI3_SAS_NEG_LINK_RATE_PHYSICAL_SHIFT                 (0)
/*** Below defines are used in both the PhysicalLinkRate and    ***/
/*** LogicalLinkRate fields above.                              ***/
/***   (by applying the proper _SHIFT value)                    ***/
#define MPI3_SAS_NEG_LINK_RATE_UNKNOWN_LINK_RATE              (0x00)
#define MPI3_SAS_NEG_LINK_RATE_PHY_DISABLED                   (0x01)
#define MPI3_SAS_NEG_LINK_RATE_NEGOTIATION_FAILED             (0x02)
#define MPI3_SAS_NEG_LINK_RATE_SATA_OOB_COMPLETE              (0x03)
#define MPI3_SAS_NEG_LINK_RATE_PORT_SELECTOR                  (0x04)
#define MPI3_SAS_NEG_LINK_RATE_SMP_RESET_IN_PROGRESS          (0x05)
#define MPI3_SAS_NEG_LINK_RATE_UNSUPPORTED_PHY                (0x06)
#define MPI3_SAS_NEG_LINK_RATE_1_5                            (0x08)
#define MPI3_SAS_NEG_LINK_RATE_3_0                            (0x09)
#define MPI3_SAS_NEG_LINK_RATE_6_0                            (0x0A)
#define MPI3_SAS_NEG_LINK_RATE_12_0                           (0x0B)
#define MPI3_SAS_NEG_LINK_RATE_22_5                           (0x0C)

/**** Defines for the AttachedPhyInfo field ****/
#define MPI3_SAS_APHYINFO_INSIDE_ZPSDS_PERSISTENT             (0x00000040)
#define MPI3_SAS_APHYINFO_REQUESTED_INSIDE_ZPSDS              (0x00000020)
#define MPI3_SAS_APHYINFO_BREAK_REPLY_CAPABLE                 (0x00000010)

#define MPI3_SAS_APHYINFO_REASON_MASK                         (0x0000000F)
#define MPI3_SAS_APHYINFO_REASON_SHIFT                        (0)
#define MPI3_SAS_APHYINFO_REASON_UNKNOWN                      (0x00000000)
#define MPI3_SAS_APHYINFO_REASON_POWER_ON                     (0x00000001)
#define MPI3_SAS_APHYINFO_REASON_HARD_RESET                   (0x00000002)
#define MPI3_SAS_APHYINFO_REASON_SMP_PHY_CONTROL              (0x00000003)
#define MPI3_SAS_APHYINFO_REASON_LOSS_OF_SYNC                 (0x00000004)
#define MPI3_SAS_APHYINFO_REASON_MULTIPLEXING_SEQ             (0x00000005)
#define MPI3_SAS_APHYINFO_REASON_IT_NEXUS_LOSS_TIMER          (0x00000006)
#define MPI3_SAS_APHYINFO_REASON_BREAK_TIMEOUT                (0x00000007)
#define MPI3_SAS_APHYINFO_REASON_PHY_TEST_STOPPED             (0x00000008)
#define MPI3_SAS_APHYINFO_REASON_EXP_REDUCED_FUNC             (0x00000009)

/**** Defines for the PhyInfo field ****/
#define MPI3_SAS_PHYINFO_STATUS_MASK                          (0xC0000000)
#define MPI3_SAS_PHYINFO_STATUS_SHIFT                         (30)
#define MPI3_SAS_PHYINFO_STATUS_ACCESSIBLE                    (0x00000000)
#define MPI3_SAS_PHYINFO_STATUS_NOT_EXIST                     (0x40000000)
#define MPI3_SAS_PHYINFO_STATUS_VACANT                        (0x80000000)

#define MPI3_SAS_PHYINFO_PHY_POWER_CONDITION_MASK             (0x18000000)
#define MPI3_SAS_PHYINFO_PHY_POWER_CONDITION_SHIFT            (27)
#define MPI3_SAS_PHYINFO_PHY_POWER_CONDITION_ACTIVE           (0x00000000)
#define MPI3_SAS_PHYINFO_PHY_POWER_CONDITION_PARTIAL          (0x08000000)
#define MPI3_SAS_PHYINFO_PHY_POWER_CONDITION_SLUMBER          (0x10000000)

#define MPI3_SAS_PHYINFO_REQUESTED_INSIDE_ZPSDS_CHANGED_MASK  (0x04000000)
#define MPI3_SAS_PHYINFO_REQUESTED_INSIDE_ZPSDS_CHANGED_SHIFT (26)
#define MPI3_SAS_PHYINFO_INSIDE_ZPSDS_PERSISTENT_MASK         (0x02000000)
#define MPI3_SAS_PHYINFO_INSIDE_ZPSDS_PERSISTENT_SHIFT        (25)
#define MPI3_SAS_PHYINFO_REQUESTED_INSIDE_ZPSDS_MASK          (0x01000000)
#define MPI3_SAS_PHYINFO_REQUESTED_INSIDE_ZPSDS_SHIFT         (24)

#define MPI3_SAS_PHYINFO_ZONE_GROUP_PERSISTENT                (0x00400000)
#define MPI3_SAS_PHYINFO_INSIDE_ZPSDS_WITHIN                  (0x00200000)
#define MPI3_SAS_PHYINFO_ZONING_ENABLED                       (0x00100000)

#define MPI3_SAS_PHYINFO_REASON_MASK                          (0x000F0000)
#define MPI3_SAS_PHYINFO_REASON_SHIFT                         (16)
#define MPI3_SAS_PHYINFO_REASON_UNKNOWN                       (0x00000000)
#define MPI3_SAS_PHYINFO_REASON_POWER_ON                      (0x00010000)
#define MPI3_SAS_PHYINFO_REASON_HARD_RESET                    (0x00020000)
#define MPI3_SAS_PHYINFO_REASON_SMP_PHY_CONTROL               (0x00030000)
#define MPI3_SAS_PHYINFO_REASON_LOSS_OF_SYNC                  (0x00040000)
#define MPI3_SAS_PHYINFO_REASON_MULTIPLEXING_SEQ              (0x00050000)
#define MPI3_SAS_PHYINFO_REASON_IT_NEXUS_LOSS_TIMER           (0x00060000)
#define MPI3_SAS_PHYINFO_REASON_BREAK_TIMEOUT                 (0x00070000)
#define MPI3_SAS_PHYINFO_REASON_PHY_TEST_STOPPED              (0x00080000)
#define MPI3_SAS_PHYINFO_REASON_EXP_REDUCED_FUNC              (0x00090000)

#define MPI3_SAS_PHYINFO_SATA_PORT_ACTIVE                     (0x00004000)
#define MPI3_SAS_PHYINFO_SATA_PORT_SELECTOR_PRESENT           (0x00002000)
#define MPI3_SAS_PHYINFO_VIRTUAL_PHY                          (0x00001000)

#define MPI3_SAS_PHYINFO_PARTIAL_PATHWAY_TIME_MASK            (0x00000F00)
#define MPI3_SAS_PHYINFO_PARTIAL_PATHWAY_TIME_SHIFT           (8)

#define MPI3_SAS_PHYINFO_ROUTING_ATTRIBUTE_MASK               (0x000000F0)
#define MPI3_SAS_PHYINFO_ROUTING_ATTRIBUTE_SHIFT              (4)
#define MPI3_SAS_PHYINFO_ROUTING_ATTRIBUTE_DIRECT             (0x00000000)
#define MPI3_SAS_PHYINFO_ROUTING_ATTRIBUTE_SUBTRACTIVE        (0x00000010)
#define MPI3_SAS_PHYINFO_ROUTING_ATTRIBUTE_TABLE              (0x00000020)

/**** Defines for the ProgrammedLinkRate field ****/
#define MPI3_SAS_PRATE_MAX_RATE_MASK                          (0xF0)
#define MPI3_SAS_PRATE_MAX_RATE_SHIFT                         (4)
#define MPI3_SAS_PRATE_MAX_RATE_NOT_PROGRAMMABLE              (0x00)
#define MPI3_SAS_PRATE_MAX_RATE_1_5                           (0x80)
#define MPI3_SAS_PRATE_MAX_RATE_3_0                           (0x90)
#define MPI3_SAS_PRATE_MAX_RATE_6_0                           (0xA0)
#define MPI3_SAS_PRATE_MAX_RATE_12_0                          (0xB0)
#define MPI3_SAS_PRATE_MAX_RATE_22_5                          (0xC0)
#define MPI3_SAS_PRATE_MIN_RATE_MASK                          (0x0F)
#define MPI3_SAS_PRATE_MIN_RATE_SHIFT                         (0)
#define MPI3_SAS_PRATE_MIN_RATE_NOT_PROGRAMMABLE              (0x00)
#define MPI3_SAS_PRATE_MIN_RATE_1_5                           (0x08)
#define MPI3_SAS_PRATE_MIN_RATE_3_0                           (0x09)
#define MPI3_SAS_PRATE_MIN_RATE_6_0                           (0x0A)
#define MPI3_SAS_PRATE_MIN_RATE_12_0                          (0x0B)
#define MPI3_SAS_PRATE_MIN_RATE_22_5                          (0x0C)

/**** Defines for the HwLinkRate field ****/
#define MPI3_SAS_HWRATE_MAX_RATE_MASK                         (0xF0)
#define MPI3_SAS_HWRATE_MAX_RATE_SHIFT                        (4)
#define MPI3_SAS_HWRATE_MAX_RATE_1_5                          (0x80)
#define MPI3_SAS_HWRATE_MAX_RATE_3_0                          (0x90)
#define MPI3_SAS_HWRATE_MAX_RATE_6_0                          (0xA0)
#define MPI3_SAS_HWRATE_MAX_RATE_12_0                         (0xB0)
#define MPI3_SAS_HWRATE_MAX_RATE_22_5                         (0xC0)
#define MPI3_SAS_HWRATE_MIN_RATE_MASK                         (0x0F)
#define MPI3_SAS_HWRATE_MIN_RATE_SHIFT                        (0)
#define MPI3_SAS_HWRATE_MIN_RATE_1_5                          (0x08)
#define MPI3_SAS_HWRATE_MIN_RATE_3_0                          (0x09)
#define MPI3_SAS_HWRATE_MIN_RATE_6_0                          (0x0A)
#define MPI3_SAS_HWRATE_MIN_RATE_12_0                         (0x0B)
#define MPI3_SAS_HWRATE_MIN_RATE_22_5                         (0x0C)

/**** Defines for the Slot field ****/
#define MPI3_SLOT_INVALID                                     (0xFFFF)

/**** Defines for the SlotIndex field ****/
#define MPI3_SLOT_INDEX_INVALID                               (0xFFFF)

/**** Defines for the LinkChangeCount fields ****/
#define MPI3_LINK_CHANGE_COUNT_INVALID                        (0xFFFF)

/**** Defines for the RateChangeCount fields ****/
#define MPI3_RATE_CHANGE_COUNT_INVALID                        (0xFFFF)

/**** Defines for the Temp Sensor Location field ****/
#define MPI3_TEMP_SENSOR_LOCATION_INTERNAL                    (0x0)
#define MPI3_TEMP_SENSOR_LOCATION_INLET                       (0x1)
#define MPI3_TEMP_SENSOR_LOCATION_OUTLET                      (0x2)
#define MPI3_TEMP_SENSOR_LOCATION_DRAM                        (0x3)

/*****************************************************************************
 *              Manufacturing Configuration Pages                            *
 ****************************************************************************/

#define MPI3_MFGPAGE_VENDORID_BROADCOM                        (0x1000)

/* MPI v3.0 SAS Products */
#define MPI3_MFGPAGE_DEVID_SAS4116                            (0x00A5)
#define MPI3_MFGPAGE_DEVID_SAS5116_MPI                        (0x00B3)
#define MPI3_MFGPAGE_DEVID_SAS5116_NVME                       (0x00B4)
#define MPI3_MFGPAGE_DEVID_SAS5116_MPI_NS                     (0x00B5)
#define MPI3_MFGPAGE_DEVID_SAS5116_NVME_NS                    (0x00B6)
#define MPI3_MFGPAGE_DEVID_SAS5116_PCIE_SWITCH                (0x00B8)
#define MPI3_MFGPAGE_DEVID_SAS5248_MPI                        (0x00F0)
#define MPI3_MFGPAGE_DEVID_SAS5248_MPI_NS                     (0x00F1)
#define MPI3_MFGPAGE_DEVID_SAS5248_PCIE_SWITCH                (0x00F2)

/*****************************************************************************
 *              Manufacturing Page 0                                         *
 ****************************************************************************/
typedef struct _MPI3_MAN_PAGE0
{
    MPI3_CONFIG_PAGE_HEADER         Header;                 /* 0x00 */
    U8                              ChipRevision[8];        /* 0x08 */
    U8                              ChipName[32];           /* 0x10 */
    U8                              BoardName[32];          /* 0x30 */
    U8                              BoardAssembly[32];      /* 0x50 */
    U8                              BoardTracerNumber[32];  /* 0x70 */
    U32                             BoardPower;             /* 0x90 */
    U32                             Reserved94;             /* 0x94 */
    U32                             Reserved98;             /* 0x98 */
    U8                              OEM;                    /* 0x9C */
    U8                              ProfileIdentifier;      /* 0x9D */
    U16                             Flags;                  /* 0x9E */
    U8                              BoardMfgDay;            /* 0xA0 */
    U8                              BoardMfgMonth;          /* 0xA1 */
    U16                             BoardMfgYear;           /* 0xA2 */
    U8                              BoardReworkDay;         /* 0xA4 */
    U8                              BoardReworkMonth;       /* 0xA5 */
    U16                             BoardReworkYear;        /* 0xA6 */
    U8                              BoardRevision[8];       /* 0xA8 */
    U8                              EPackFRU[16];           /* 0xB0 */
    U8                              ProductName[256];       /* 0xC0 */
} MPI3_MAN_PAGE0, MPI3_POINTER PTR_MPI3_MAN_PAGE0,
  Mpi3ManPage0_t, MPI3_POINTER pMpi3ManPage0_t;

/**** Defines for the PageVersion field ****/
#define MPI3_MAN0_PAGEVERSION       (0x00)

/**** Defines for the Flags field ****/
#define MPI3_MAN0_FLAGS_SWITCH_PRESENT                       (0x0002)
#define MPI3_MAN0_FLAGS_EXPANDER_PRESENT                     (0x0001)

/*****************************************************************************
 *              Manufacturing Page 1                                         *
 ****************************************************************************/

#define MPI3_MAN1_VPD_SIZE                                   (512)

typedef struct _MPI3_MAN_PAGE1
{
    MPI3_CONFIG_PAGE_HEADER         Header;                  /* 0x00 */
    U32                             Reserved08[2];           /* 0x08 */
    U8                              VPD[MPI3_MAN1_VPD_SIZE]; /* 0x10 */
} MPI3_MAN_PAGE1, MPI3_POINTER PTR_MPI3_MAN_PAGE1,
  Mpi3ManPage1_t, MPI3_POINTER pMpi3ManPage1_t;

/**** Defines for the PageVersion field ****/
#define MPI3_MAN1_PAGEVERSION                                 (0x00)


/*****************************************************************************
 *              Manufacturing Page 2                                         *
 ****************************************************************************/

typedef struct _MPI3_MAN_PAGE2
{
    MPI3_CONFIG_PAGE_HEADER         Header;                   /* 0x00 */
    U8                              Flags;                    /* 0x08 */
    U8                              Reserved09[3];            /* 0x09 */
    U32                             Reserved0C[3];            /* 0x0C */
    U8                              OEMBoardTracerNumber[32]; /* 0x18 */
} MPI3_MAN_PAGE2, MPI3_POINTER PTR_MPI3_MAN_PAGE2,
  Mpi3ManPage2_t, MPI3_POINTER pMpi3ManPage2_t;

/**** Defines for the PageVersion field ****/
#define MPI3_MAN2_PAGEVERSION                                 (0x00)

/**** Defines for the Flags field ****/
#define MPI3_MAN2_FLAGS_TRACER_PRESENT                        (0x01)

/*****************************************************************************
 *              Manufacturing Page 5                                         *
 ****************************************************************************/
typedef struct _MPI3_MAN5_PHY_ENTRY
{
    U64     IOC_WWID;                                       /* 0x00 */
    U64     DeviceName;                                     /* 0x08 */
    U64     SATA_WWID;                                      /* 0x10 */
} MPI3_MAN5_PHY_ENTRY, MPI3_POINTER PTR_MPI3_MAN5_PHY_ENTRY,
  Mpi3Man5PhyEntry_t, MPI3_POINTER pMpi3Man5PhyEntry_t;

#ifndef MPI3_MAN5_PHY_MAX
#define MPI3_MAN5_PHY_MAX                                   (1)
#endif  /* MPI3_MAN5_PHY_MAX */

typedef struct _MPI3_MAN_PAGE5
{
    MPI3_CONFIG_PAGE_HEADER         Header;                 /* 0x00 */
    U8                              NumPhys;                /* 0x08 */
    U8                              Reserved09[3];          /* 0x09 */
    U32                             Reserved0C;             /* 0x0C */
    MPI3_MAN5_PHY_ENTRY             Phy[MPI3_MAN5_PHY_MAX]; /* 0x10 */
} MPI3_MAN_PAGE5, MPI3_POINTER PTR_MPI3_MAN_PAGE5,
  Mpi3ManPage5_t, MPI3_POINTER pMpi3ManPage5_t;

/**** Defines for the PageVersion field ****/
#define MPI3_MAN5_PAGEVERSION                                (0x00)

/*****************************************************************************
 *              Manufacturing Page 6                                         *
 ****************************************************************************/
typedef struct _MPI3_MAN6_GPIO_ENTRY
{
    U8      FunctionCode;                                                     /* 0x00 */
    U8      FunctionFlags;                                                    /* 0x01 */
    U16     Flags;                                                            /* 0x02 */
    U8      Param1;                                                           /* 0x04 */
    U8      Param2;                                                           /* 0x05 */
    U16     Reserved06;                                                       /* 0x06 */
    U32     Param3;                                                           /* 0x08 */
} MPI3_MAN6_GPIO_ENTRY, MPI3_POINTER PTR_MPI3_MAN6_GPIO_ENTRY,
  Mpi3Man6GpioEntry_t, MPI3_POINTER pMpi3Man6GpioEntry_t;

/**** Defines for the FunctionCode field ****/
#define MPI3_MAN6_GPIO_FUNCTION_GENERIC                                       (0x00)
#define MPI3_MAN6_GPIO_FUNCTION_ALTERNATE                                     (0x01)
#define MPI3_MAN6_GPIO_FUNCTION_EXT_INTERRUPT                                 (0x02)
#define MPI3_MAN6_GPIO_FUNCTION_GLOBAL_ACTIVITY                               (0x03)
#define MPI3_MAN6_GPIO_FUNCTION_OVER_TEMPERATURE                              (0x04)
#define MPI3_MAN6_GPIO_FUNCTION_PORT_STATUS_GREEN                             (0x05)
#define MPI3_MAN6_GPIO_FUNCTION_PORT_STATUS_YELLOW                            (0x06)
#define MPI3_MAN6_GPIO_FUNCTION_CABLE_MANAGEMENT                              (0x07)
#define MPI3_MAN6_GPIO_FUNCTION_BKPLANE_MGMT_TYPE                             (0x08)
#define MPI3_MAN6_GPIO_FUNCTION_ISTWI_RESET                                   (0x0A)
#define MPI3_MAN6_GPIO_FUNCTION_BACKEND_PCIE_RESET                            (0x0B)
#define MPI3_MAN6_GPIO_FUNCTION_GLOBAL_FAULT                                  (0x0C)
#define MPI3_MAN6_GPIO_FUNCTION_PBLP_STATUS_CHANGE                            (0x0D)
#define MPI3_MAN6_GPIO_FUNCTION_EPACK_ONLINE                                  (0x0E)
#define MPI3_MAN6_GPIO_FUNCTION_EPACK_FAULT                                   (0x0F)
#define MPI3_MAN6_GPIO_FUNCTION_CTRL_TYPE                                     (0x10)
#define MPI3_MAN6_GPIO_FUNCTION_LICENSE                                       (0x11)
#define MPI3_MAN6_GPIO_FUNCTION_REFCLK_CONTROL                                (0x12)
#define MPI3_MAN6_GPIO_FUNCTION_BACKEND_PCIE_RESET_CLAMP                      (0x13)
#define MPI3_MAN6_GPIO_FUNCTION_AUXILIARY_POWER                               (0x14)
#define MPI3_MAN6_GPIO_FUNCTION_RAID_DATA_CACHE_DIRTY                         (0x15)
#define MPI3_MAN6_GPIO_FUNCTION_BOARD_FAN_CONTROL                             (0x16)
#define MPI3_MAN6_GPIO_FUNCTION_BOARD_FAN_FAULT                               (0x17)
#define MPI3_MAN6_GPIO_FUNCTION_POWER_BRAKE                                   (0x18)
#define MPI3_MAN6_GPIO_FUNCTION_MGMT_CONTROLLER_RESET                         (0x19)

/**** Defines for FunctionFlags when FunctionCode is ISTWI_RESET ****/
#define MPI3_MAN6_GPIO_ISTWI_RESET_FUNCTIONFLAGS_DEVSELECT_MASK               (0x01)
#define MPI3_MAN6_GPIO_ISTWI_RESET_FUNCTIONFLAGS_DEVSELECT_SHIFT              (0)
#define MPI3_MAN6_GPIO_ISTWI_RESET_FUNCTIONFLAGS_DEVSELECT_ISTWI              (0x00)
#define MPI3_MAN6_GPIO_ISTWI_RESET_FUNCTIONFLAGS_DEVSELECT_RECEPTACLEID       (0x01)

/**** Defines for Param1 (Flags) when FunctionCode is EXT_INTERRUPT ****/
#define MPI3_MAN6_GPIO_EXTINT_PARAM1_FLAGS_SOURCE_MASK                        (0xF0)
#define MPI3_MAN6_GPIO_EXTINT_PARAM1_FLAGS_SOURCE_SHIFT                       (4)
#define MPI3_MAN6_GPIO_EXTINT_PARAM1_FLAGS_SOURCE_GENERIC                     (0x00)
#define MPI3_MAN6_GPIO_EXTINT_PARAM1_FLAGS_SOURCE_CABLE_MGMT                  (0x10)
#define MPI3_MAN6_GPIO_EXTINT_PARAM1_FLAGS_SOURCE_ACTIVE_CABLE_OVERCURRENT    (0x20)
#define MPI3_MAN6_GPIO_EXTINT_PARAM1_FLAGS_ACK_REQUIRED                       (0x02)

#define MPI3_MAN6_GPIO_EXTINT_PARAM1_FLAGS_TRIGGER_MASK                       (0x01)
#define MPI3_MAN6_GPIO_EXTINT_PARAM1_FLAGS_TRIGGER_SHIFT                      (0)
#define MPI3_MAN6_GPIO_EXTINT_PARAM1_FLAGS_TRIGGER_EDGE                       (0x00)
#define MPI3_MAN6_GPIO_EXTINT_PARAM1_FLAGS_TRIGGER_LEVEL                      (0x01)

/**** Defines for Param1 (LEVEL) when FunctionCode is OVER_TEMPERATURE ****/
#define MPI3_MAN6_GPIO_OVER_TEMP_PARAM1_LEVEL_WARNING                         (0x00)
#define MPI3_MAN6_GPIO_OVER_TEMP_PARAM1_LEVEL_CRITICAL                        (0x01)
#define MPI3_MAN6_GPIO_OVER_TEMP_PARAM1_LEVEL_FATAL                           (0x02)

/**** Defines for Param1 (PHY STATE) when FunctionCode is PORT_STATUS_GREEN ****/
#define MPI3_MAN6_GPIO_PORT_GREEN_PARAM1_PHY_STATUS_ALL_UP                    (0x00)
#define MPI3_MAN6_GPIO_PORT_GREEN_PARAM1_PHY_STATUS_ONE_OR_MORE_UP            (0x01)

/**** Defines for Param1 (INTERFACE_SIGNAL) when FunctionCode is CABLE_MANAGEMENT ****/
#define MPI3_MAN6_GPIO_CABLE_MGMT_PARAM1_INTERFACE_MODULE_PRESENT             (0x00)
#define MPI3_MAN6_GPIO_CABLE_MGMT_PARAM1_INTERFACE_ACTIVE_CABLE_ENABLE        (0x01)
#define MPI3_MAN6_GPIO_CABLE_MGMT_PARAM1_INTERFACE_CABLE_MGMT_ENABLE          (0x02)

/**** Defines for Param1 (LICENSE_TYPE) when FunctionCode is LICENSE ****/
#define MPI3_MAN6_GPIO_LICENSE_PARAM1_TYPE_IBUTTON                            (0x00)


/**** Defines for the Flags field ****/
#define MPI3_MAN6_GPIO_FLAGS_SLEW_RATE_MASK                                   (0x0100)
#define MPI3_MAN6_GPIO_FLAGS_SLEW_RATE_SHIFT                                  (8)
#define MPI3_MAN6_GPIO_FLAGS_SLEW_RATE_FAST_EDGE                              (0x0100)
#define MPI3_MAN6_GPIO_FLAGS_SLEW_RATE_SLOW_EDGE                              (0x0000)
#define MPI3_MAN6_GPIO_FLAGS_DRIVE_STRENGTH_MASK                              (0x00C0)
#define MPI3_MAN6_GPIO_FLAGS_DRIVE_STRENGTH_SHIFT                             (6)
#define MPI3_MAN6_GPIO_FLAGS_DRIVE_STRENGTH_100OHM                            (0x0000)
#define MPI3_MAN6_GPIO_FLAGS_DRIVE_STRENGTH_66OHM                             (0x0040)
#define MPI3_MAN6_GPIO_FLAGS_DRIVE_STRENGTH_50OHM                             (0x0080)
#define MPI3_MAN6_GPIO_FLAGS_DRIVE_STRENGTH_33OHM                             (0x00C0)
#define MPI3_MAN6_GPIO_FLAGS_ALT_DATA_SEL_MASK                                (0x0030)
#define MPI3_MAN6_GPIO_FLAGS_ALT_DATA_SEL_SHIFT                               (4)
#define MPI3_MAN6_GPIO_FLAGS_ACTIVE_HIGH                                      (0x0008)
#define MPI3_MAN6_GPIO_FLAGS_BI_DIR_ENABLED                                   (0x0004)
#define MPI3_MAN6_GPIO_FLAGS_DIRECTION_MASK                                   (0x0003)
#define MPI3_MAN6_GPIO_FLAGS_DIRECTION_SHIFT                                  (0)
#define MPI3_MAN6_GPIO_FLAGS_DIRECTION_INPUT                                  (0x0000)
#define MPI3_MAN6_GPIO_FLAGS_DIRECTION_OPEN_DRAIN_OUTPUT                      (0x0001)
#define MPI3_MAN6_GPIO_FLAGS_DIRECTION_OPEN_SOURCE_OUTPUT                     (0x0002)
#define MPI3_MAN6_GPIO_FLAGS_DIRECTION_PUSH_PULL_OUTPUT                       (0x0003)

#ifndef MPI3_MAN6_GPIO_MAX
#define MPI3_MAN6_GPIO_MAX                                                    (1)
#endif  /* MPI3_MAN6_GPIO_MAX */

typedef struct _MPI3_MAN_PAGE6
{
    MPI3_CONFIG_PAGE_HEADER         Header;                                   /* 0x00 */
    U16                             Flags;                                    /* 0x08 */
    U16                             Reserved0A;                               /* 0x0A */
    U8                              NumGPIO;                                  /* 0x0C */
    U8                              Reserved0D[3];                            /* 0x0D */
    MPI3_MAN6_GPIO_ENTRY            GPIO[MPI3_MAN6_GPIO_MAX];                 /* 0x10 */
} MPI3_MAN_PAGE6, MPI3_POINTER PTR_MPI3_MAN_PAGE6,
  Mpi3ManPage6_t, MPI3_POINTER pMpi3ManPage6_t;

/**** Defines for the PageVersion field ****/
#define MPI3_MAN6_PAGEVERSION                                                 (0x00)

/**** Defines for the Flags field ****/
#define MPI3_MAN6_FLAGS_HEARTBEAT_LED_DISABLED                                (0x0001)

/*****************************************************************************
 *              Manufacturing Page 7                                         *
 ****************************************************************************/
typedef struct _MPI3_MAN7_RECEPTACLE_INFO
{
    U32                             Name[4];                    /* 0x00 */
    U8                              Location;                   /* 0x10 */
    U8                              ConnectorType;              /* 0x11 */
    U8                              PEDClk;                     /* 0x12 */
    U8                              ConnectorID;                /* 0x13 */
    U32                             Reserved14;                 /* 0x14 */
} MPI3_MAN7_RECEPTACLE_INFO, MPI3_POINTER PTR_MPI3_MAN7_RECEPTACLE_INFO,
 Mpi3Man7ReceptacleInfo_t, MPI3_POINTER pMpi3Man7ReceptacleInfo_t;

/**** Defines for Location field ****/
#define MPI3_MAN7_LOCATION_UNKNOWN                         (0x00)
#define MPI3_MAN7_LOCATION_INTERNAL                        (0x01)
#define MPI3_MAN7_LOCATION_EXTERNAL                        (0x02)
#define MPI3_MAN7_LOCATION_VIRTUAL                         (0x03)
#define MPI3_MAN7_LOCATION_HOST                            (0x04)

/**** Defines for ConnectorType - Use definitions from SES-4 ****/
#define MPI3_MAN7_CONNECTOR_TYPE_NO_INFO                   (0x00)

/**** Defines for PEDClk field ****/
#define MPI3_MAN7_PEDCLK_ROUTING_MASK                      (0x10)
#define MPI3_MAN7_PEDCLK_ROUTING_SHIFT                     (4)
#define MPI3_MAN7_PEDCLK_ROUTING_DIRECT                    (0x00)
#define MPI3_MAN7_PEDCLK_ROUTING_CLOCK_BUFFER              (0x10)
#define MPI3_MAN7_PEDCLK_ID_MASK                           (0x0F)
#define MPI3_MAN7_PEDCLK_ID_SHIFT                          (0)

#ifndef MPI3_MAN7_RECEPTACLE_INFO_MAX
#define MPI3_MAN7_RECEPTACLE_INFO_MAX                      (1)
#endif  /* MPI3_MAN7_RECEPTACLE_INFO_MAX */

typedef struct _MPI3_MAN_PAGE7
{
    MPI3_CONFIG_PAGE_HEADER         Header;                                           /* 0x00 */
    U32                             Flags;                                            /* 0x08 */
    U8                              NumReceptacles;                                   /* 0x0C */
    U8                              Reserved0D[3];                                    /* 0x0D */
    U32                             EnclosureName[4];                                 /* 0x10 */
    MPI3_MAN7_RECEPTACLE_INFO       ReceptacleInfo[MPI3_MAN7_RECEPTACLE_INFO_MAX];    /* 0x20 */   /* variable length array */
} MPI3_MAN_PAGE7, MPI3_POINTER PTR_MPI3_MAN_PAGE7,
  Mpi3ManPage7_t, MPI3_POINTER pMpi3ManPage7_t;

/**** Defines for the PageVersion field ****/
#define MPI3_MAN7_PAGEVERSION                              (0x00)

/**** Defines for Flags field ****/
#define MPI3_MAN7_FLAGS_BASE_ENCLOSURE_LEVEL_MASK          (0x01)
#define MPI3_MAN7_FLAGS_BASE_ENCLOSURE_LEVEL_SHIFT         (0)
#define MPI3_MAN7_FLAGS_BASE_ENCLOSURE_LEVEL_0             (0x00)
#define MPI3_MAN7_FLAGS_BASE_ENCLOSURE_LEVEL_1             (0x01)


/*****************************************************************************
 *              Manufacturing Page 8                                         *
 ****************************************************************************/

typedef struct _MPI3_MAN8_PHY_INFO
{
    U8                              ReceptacleID;               /* 0x00 */
    U8                              ConnectorLane;              /* 0x01 */
    U16                             Reserved02;                 /* 0x02 */
    U16                             Slotx1;                     /* 0x04 */
    U16                             Slotx2;                     /* 0x06 */
    U16                             Slotx4;                     /* 0x08 */
    U16                             Reserved0A;                 /* 0x0A */
    U32                             Reserved0C;                 /* 0x0C */
} MPI3_MAN8_PHY_INFO, MPI3_POINTER PTR_MPI3_MAN8_PHY_INFO,
  Mpi3Man8PhyInfo_t, MPI3_POINTER pMpi3Man8PhyInfo_t;

/**** Defines for ReceptacleID field ****/
#define MPI3_MAN8_PHY_INFO_RECEPTACLE_ID_NOT_ASSOCIATED    (0xFF)

/**** Defines for ConnectorLane field ****/
#define MPI3_MAN8_PHY_INFO_CONNECTOR_LANE_NOT_ASSOCIATED   (0xFF)

#ifndef MPI3_MAN8_PHY_INFO_MAX
#define MPI3_MAN8_PHY_INFO_MAX                      (1)
#endif  /* MPI3_MAN8_PHY_INFO_MAX */

typedef struct _MPI3_MAN_PAGE8
{
    MPI3_CONFIG_PAGE_HEADER         Header;                            /* 0x00 */
    U32                             Reserved08;                        /* 0x08 */
    U8                              NumPhys;                           /* 0x0C */
    U8                              Reserved0D[3];                     /* 0x0D */
    MPI3_MAN8_PHY_INFO              PhyInfo[MPI3_MAN8_PHY_INFO_MAX];   /* 0x10 */  /* variable length array */
} MPI3_MAN_PAGE8, MPI3_POINTER PTR_MPI3_MAN_PAGE8,
  Mpi3ManPage8_t, MPI3_POINTER pMpi3ManPage8_t;

/**** Defines for the PageVersion field ****/
#define MPI3_MAN8_PAGEVERSION                   (0x00)

/*****************************************************************************
 *              Manufacturing Page 9                                         *
 ****************************************************************************/
typedef struct _MPI3_MAN9_RSRC_ENTRY
{
    U32     Maximum;        /* 0x00 */
    U32     Decrement;      /* 0x04 */
    U32     Minimum;        /* 0x08 */
    U32     Actual;         /* 0x0C */
} MPI3_MAN9_RSRC_ENTRY, MPI3_POINTER PTR_MPI3_MAN9_RSRC_ENTRY,
  Mpi3Man9RsrcEntry_t, MPI3_POINTER pMpi3Man9RsrcEntry_t;

typedef enum _MPI3_MAN9_RESOURCES
{
    MPI3_MAN9_RSRC_OUTSTANDING_REQS    = 0,
    MPI3_MAN9_RSRC_TARGET_CMDS         = 1,
    MPI3_MAN9_RSRC_RESERVED02          = 2,
    MPI3_MAN9_RSRC_NVME                = 3,
    MPI3_MAN9_RSRC_INITIATORS          = 4,
    MPI3_MAN9_RSRC_VDS                 = 5,
    MPI3_MAN9_RSRC_ENCLOSURES          = 6,
    MPI3_MAN9_RSRC_ENCLOSURE_PHYS      = 7,
    MPI3_MAN9_RSRC_EXPANDERS           = 8,
    MPI3_MAN9_RSRC_PCIE_SWITCHES       = 9,
    MPI3_MAN9_RSRC_RESERVED10          = 10,
    MPI3_MAN9_RSRC_HOST_PD_DRIVES      = 11,
    MPI3_MAN9_RSRC_ADV_HOST_PD_DRIVES  = 12,
    MPI3_MAN9_RSRC_RAID_PD_DRIVES      = 13,
    MPI3_MAN9_RSRC_DRV_DIAG_BUF        = 14,
    MPI3_MAN9_RSRC_NAMESPACE_COUNT     = 15,
    MPI3_MAN9_RSRC_NUM_RESOURCES
} MPI3_MAN9_RESOURCES;

#define MPI3_MAN9_MIN_OUTSTANDING_REQS      (1)
#define MPI3_MAN9_MAX_OUTSTANDING_REQS      (65000)

#define MPI3_MAN9_MIN_TARGET_CMDS           (0)
#define MPI3_MAN9_MAX_TARGET_CMDS           (65535)

#define MPI3_MAN9_MIN_NVME_TARGETS          (0)
/* Max NVMe Targets is product specific */

#define MPI3_MAN9_MIN_INITIATORS            (0)
/* Max Initiators is product specific */

#define MPI3_MAN9_MIN_VDS                   (0)
/* Max VDs is product specific */

#define MPI3_MAN9_MIN_ENCLOSURES            (1)
#define MPI3_MAN9_MAX_ENCLOSURES            (65535)

#define MPI3_MAN9_MIN_ENCLOSURE_PHYS        (0)
/* Max Enclosure Phys is product specific */

#define MPI3_MAN9_MIN_EXPANDERS             (0)
#define MPI3_MAN9_MAX_EXPANDERS             (65535)

#define MPI3_MAN9_MIN_PCIE_SWITCHES         (0)
/* Max PCIe Switches is product specific */

#define MPI3_MAN9_MIN_HOST_PD_DRIVES        (0)
/* Max Host PD Drives is product specific */

#define MPI3_MAN9_ADV_HOST_PD_DRIVES        (0)
/* Max Advanced Host PD Drives is product specific */

#define MPI3_MAN9_RAID_PD_DRIVES            (0)
/* Max RAID PD Drives is product specific */

#define MPI3_MAN9_DRIVER_DIAG_BUFFER        (0)
/* Max Driver Diag Buffer is product specific */

#define MPI3_MAN9_MIN_NAMESPACE_COUNT       (1)

#define MPI3_MAN9_MIN_EXPANDERS             (0)
#define MPI3_MAN9_MAX_EXPANDERS             (65535)


typedef struct _MPI3_MAN_PAGE9
{
    MPI3_CONFIG_PAGE_HEADER         Header;                                 /* 0x00 */
    U8                              NumResources;                           /* 0x08 */
    U8                              Reserved09;                             /* 0x09 */
    U16                             Reserved0A;                             /* 0x0A */
    U32                             Reserved0C;                             /* 0x0C */
    U32                             Reserved10;                             /* 0x10 */
    U32                             Reserved14;                             /* 0x14 */
    U32                             Reserved18;                             /* 0x18 */
    U32                             Reserved1C;                             /* 0x1C */
    MPI3_MAN9_RSRC_ENTRY            Resource[MPI3_MAN9_RSRC_NUM_RESOURCES]; /* 0x20 */
} MPI3_MAN_PAGE9, MPI3_POINTER PTR_MPI3_MAN_PAGE9,
  Mpi3ManPage9_t, MPI3_POINTER pMpi3ManPage9_t;

/**** Defines for the PageVersion field ****/
#define MPI3_MAN9_PAGEVERSION                   (0x00)

/*****************************************************************************
 *              Manufacturing Page 10                                        *
 ****************************************************************************/
typedef struct _MPI3_MAN10_ISTWI_CTRLR_ENTRY
{
    U16     TargetAddress;      /* 0x00 */
    U16     Flags;              /* 0x02 */
    U8      SCLLowOverride;     /* 0x04 */
    U8      SCLHighOverride;    /* 0x05 */
    U16     Reserved06;         /* 0x06 */
} MPI3_MAN10_ISTWI_CTRLR_ENTRY, MPI3_POINTER PTR_MPI3_MAN10_ISTWI_CTRLR_ENTRY,
  Mpi3Man10IstwiCtrlrEntry_t, MPI3_POINTER pMpi3Man10IstwiCtrlrEntry_t;

/**** Defines for the Flags field ****/

#define MPI3_MAN10_ISTWI_CTRLR_FLAGS_I2C_GLICH_FLTR_MASK        (0xC000)
#define MPI3_MAN10_ISTWI_CTRLR_FLAGS_I2C_GLICH_FLTR_SHIFT       (14)
#define MPI3_MAN10_ISTWI_CTRLR_FLAGS_I2C_GLICH_FLTR_50_NS       (0x0000)
#define MPI3_MAN10_ISTWI_CTRLR_FLAGS_I2C_GLICH_FLTR_10_NS       (0x4000)
#define MPI3_MAN10_ISTWI_CTRLR_FLAGS_I2C_GLICH_FLTR_5_NS        (0x8000)
#define MPI3_MAN10_ISTWI_CTRLR_FLAGS_I2C_GLICH_FLTR_0_NS        (0xC000)
#define MPI3_MAN10_ISTWI_CTRLR_FLAGS_BUS_TYPE_MASK              (0x3000)
#define MPI3_MAN10_ISTWI_CTRLR_FLAGS_BUS_TYPE_SHIFT             (12)
#define MPI3_MAN10_ISTWI_CTRLR_FLAGS_BUS_TYPE_I2C               (0x0000)
#define MPI3_MAN10_ISTWI_CTRLR_FLAGS_BUS_TYPE_I3C               (0x1000)
#define MPI3_MAN10_ISTWI_CTRLR_FLAGS_BUS_TYPE_AUTO              (0x2000)
#define MPI3_MAN10_ISTWI_CTRLR_FLAGS_I3C_MAX_DATA_RATE_MASK     (0x0E00)
#define MPI3_MAN10_ISTWI_CTRLR_FLAGS_I3C_MAX_DATA_RATE_SHIFT    (9)
#define MPI3_MAN10_ISTWI_CTRLR_FLAGS_I3C_MAX_DATA_RATE_12_5_MHZ (0x0000)
#define MPI3_MAN10_ISTWI_CTRLR_FLAGS_I3C_MAX_DATA_RATE_8_MHZ    (0x0200)
#define MPI3_MAN10_ISTWI_CTRLR_FLAGS_I3C_MAX_DATA_RATE_6_MHZ    (0x0400)
#define MPI3_MAN10_ISTWI_CTRLR_FLAGS_I3C_MAX_DATA_RATE_4_MHZ    (0x0600)
#define MPI3_MAN10_ISTWI_CTRLR_FLAGS_I3C_MAX_DATA_RATE_2_MHZ    (0x0800)
#define MPI3_MAN10_ISTWI_CTRLR_FLAGS_BUS_SPEED_MASK             (0x000C)
#define MPI3_MAN10_ISTWI_CTRLR_FLAGS_BUS_SPEED_SHIFT            (0)
#define MPI3_MAN10_ISTWI_CTRLR_FLAGS_BUS_SPEED_100_KHZ          (0x0000)
#define MPI3_MAN10_ISTWI_CTRLR_FLAGS_BUS_SPEED_400_KHZ          (0x0004)
#define MPI3_MAN10_ISTWI_CTRLR_FLAGS_TARGET_ENABLED             (0x0002)
#define MPI3_MAN10_ISTWI_CTRLR_FLAGS_INITIATOR_ENABLED          (0x0001)

#ifndef MPI3_MAN10_ISTWI_CTRLR_MAX
#define MPI3_MAN10_ISTWI_CTRLR_MAX          (1)
#endif  /* MPI3_MAN10_ISTWI_CTRLR_MAX */

typedef struct _MPI3_MAN_PAGE10
{
    MPI3_CONFIG_PAGE_HEADER         Header;                                         /* 0x00 */
    U32                             Reserved08;                                     /* 0x08 */
    U8                              NumISTWICtrl;                                   /* 0x0C */
    U8                              Reserved0D[3];                                  /* 0x0D */
    MPI3_MAN10_ISTWI_CTRLR_ENTRY    ISTWIController[MPI3_MAN10_ISTWI_CTRLR_MAX];    /* 0x10 */
} MPI3_MAN_PAGE10, MPI3_POINTER PTR_MPI3_MAN_PAGE10,
  Mpi3ManPage10_t, MPI3_POINTER pMpi3ManPage10_t;

/**** Defines for the PageVersion field ****/
#define MPI3_MAN10_PAGEVERSION                  (0x00)

/*****************************************************************************
 *              Manufacturing Page 11                                        *
 ****************************************************************************/
typedef struct _MPI3_MAN11_MUX_DEVICE_FORMAT
{
    U8      MaxChannel;         /* 0x00 */
    U8      Reserved01[3];      /* 0x01 */
    U32     Reserved04;         /* 0x04 */
} MPI3_MAN11_MUX_DEVICE_FORMAT, MPI3_POINTER PTR_MPI3_MAN11_MUX_DEVICE_FORMAT,
  Mpi3Man11MuxDeviceFormat_t, MPI3_POINTER pMpi3Man11MuxDeviceFormat_t;

typedef struct _MPI3_MAN11_TEMP_SENSOR_DEVICE_FORMAT
{
    U8      Type;               /* 0x00 */
    U8      Reserved01[3];      /* 0x01 */
    U8      TempChannel[4];     /* 0x04 */
} MPI3_MAN11_TEMP_SENSOR_DEVICE_FORMAT, MPI3_POINTER PTR_MPI3_MAN11_TEMP_SENSOR_DEVICE_FORMAT,
  Mpi3Man11TempSensorDeviceFormat_t, MPI3_POINTER pMpi3Man11TempSensorDeviceFormat_t;

/**** Defines for the Type field ****/
#define MPI3_MAN11_TEMP_SENSOR_TYPE_MAX6654                (0x00)
#define MPI3_MAN11_TEMP_SENSOR_TYPE_EMC1442                (0x01)
#define MPI3_MAN11_TEMP_SENSOR_TYPE_ADT7476                (0x02)
#define MPI3_MAN11_TEMP_SENSOR_TYPE_SE97B                  (0x03)

/**** Define for the TempChannel field ****/
#define MPI3_MAN11_TEMP_SENSOR_CHANNEL_LOCATION_MASK       (0xE0)
#define MPI3_MAN11_TEMP_SENSOR_CHANNEL_LOCATION_SHIFT      (5)
/**** for the Location field values - use MPI3_TEMP_SENSOR_LOCATION_ defines ****/
#define MPI3_MAN11_TEMP_SENSOR_CHANNEL_ENABLED             (0x01)


typedef struct _MPI3_MAN11_SEEPROM_DEVICE_FORMAT
{
    U8      Size;               /* 0x00 */
    U8      PageWriteSize;      /* 0x01 */
    U16     Reserved02;         /* 0x02 */
    U32     Reserved04;         /* 0x04 */
} MPI3_MAN11_SEEPROM_DEVICE_FORMAT, MPI3_POINTER PTR_MPI3_MAN11_SEEPROM_DEVICE_FORMAT,
  Mpi3Man11SeepromDeviceFormat_t, MPI3_POINTER pMpi3Man11SeepromDeviceFormat_t;

/**** Defines for the Size field ****/
#define MPI3_MAN11_SEEPROM_SIZE_1KBITS              (0x01)
#define MPI3_MAN11_SEEPROM_SIZE_2KBITS              (0x02)
#define MPI3_MAN11_SEEPROM_SIZE_4KBITS              (0x03)
#define MPI3_MAN11_SEEPROM_SIZE_8KBITS              (0x04)
#define MPI3_MAN11_SEEPROM_SIZE_16KBITS             (0x05)
#define MPI3_MAN11_SEEPROM_SIZE_32KBITS             (0x06)
#define MPI3_MAN11_SEEPROM_SIZE_64KBITS             (0x07)
#define MPI3_MAN11_SEEPROM_SIZE_128KBITS            (0x08)

typedef struct _MPI3_MAN11_DDR_SPD_DEVICE_FORMAT
{
    U8      Channel;            /* 0x00 */
    U8      Reserved01[3];      /* 0x01 */
    U32     Reserved04;         /* 0x04 */
} MPI3_MAN11_DDR_SPD_DEVICE_FORMAT, MPI3_POINTER PTR_MPI3_MAN11_DDR_SPD_DEVICE_FORMAT,
  Mpi3Man11DdrSpdDeviceFormat_t, MPI3_POINTER pMpi3Man11DdrSpdDeviceFormat_t;

typedef struct _MPI3_MAN11_CABLE_MGMT_DEVICE_FORMAT
{
    U8      Type;               /* 0x00 */
    U8      ReceptacleID;       /* 0x01 */
    U16     Reserved02;         /* 0x02 */
    U32     Reserved04;         /* 0x04 */
} MPI3_MAN11_CABLE_MGMT_DEVICE_FORMAT, MPI3_POINTER PTR_MPI3_MAN11_CABLE_MGMT_DEVICE_FORMAT,
  Mpi3Man11CableMgmtDeviceFormat_t, MPI3_POINTER pMpi3Man11CableMgmtDeviceFormat_t;

/**** Defines for the Type field ****/
#define MPI3_MAN11_CABLE_MGMT_TYPE_SFF_8636           (0x00)

typedef struct _MPI3_MAN11_BKPLANE_SPEC_UBM_FORMAT
{
    U16     Flags;              /* 0x00 */
    U16     Reserved02;         /* 0x02 */
} MPI3_MAN11_BKPLANE_SPEC_UBM_FORMAT, MPI3_POINTER PTR_MPI3_MAN11_BKPLANE_SPEC_UBM_FORMAT,
  Mpi3Man11BkplaneSpecUBMFormat_t, MPI3_POINTER pMpi3Man11BkplaneSpecUBMFormat_t;

/**** Defines for the Flags field ****/
#define MPI3_MAN11_BKPLANE_UBM_FLAGS_REFCLK_POLICY_ALWAYS_ENABLED  (0x0200)
#define MPI3_MAN11_BKPLANE_UBM_FLAGS_FORCE_POLLING                 (0x0100)
#define MPI3_MAN11_BKPLANE_UBM_FLAGS_MAX_FRU_MASK                  (0x00F0)
#define MPI3_MAN11_BKPLANE_UBM_FLAGS_MAX_FRU_SHIFT                 (4)
#define MPI3_MAN11_BKPLANE_UBM_FLAGS_POLL_INTERVAL_MASK            (0x000F)
#define MPI3_MAN11_BKPLANE_UBM_FLAGS_POLL_INTERVAL_SHIFT           (0)

typedef struct _MPI3_MAN11_BKPLANE_SPEC_NON_UBM_FORMAT
{
    U16     Flags;              /* 0x00 */
    U8      Reserved02;         /* 0x02 */
    U8      Type;               /* 0x03 */
} MPI3_MAN11_BKPLANE_SPEC_NON_UBM_FORMAT, MPI3_POINTER PTR_MPI3_MAN11_BKPLANE_SPEC_NON_UBM_FORMAT,
  Mpi3Man11BkplaneSpecNonUBMFormat_t, MPI3_POINTER pMpi3Man11BkplaneSpecNonUBMFormat_t;

/**** Defines for the Flags field ****/
#define MPI3_MAN11_BKPLANE_NON_UBM_FLAGS_GROUP_MASK                    (0xF000)
#define MPI3_MAN11_BKPLANE_NON_UBM_FLAGS_GROUP_SHIFT                   (12)
#define MPI3_MAN11_BKPLANE_NON_UBM_FLAGS_REFCLK_POLICY_MASK            (0x0600)
#define MPI3_MAN11_BKPLANE_NON_UBM_FLAGS_REFCLK_POLICY_SHIFT           (9)
#define MPI3_MAN11_BKPLANE_NON_UBM_FLAGS_REFCLK_POLICY_DEVICE_PRESENT  (0x0000)
#define MPI3_MAN11_BKPLANE_NON_UBM_FLAGS_REFCLK_POLICY_ALWAYS_ENABLED  (0x0200)
#define MPI3_MAN11_BKPLANE_NON_UBM_FLAGS_REFCLK_POLICY_SRIS            (0x0400)
#define MPI3_MAN11_BKPLANE_NON_UBM_FLAGS_LINKWIDTH_MASK                (0x00C0)
#define MPI3_MAN11_BKPLANE_NON_UBM_FLAGS_LINKWIDTH_SHIFT               (6)
#define MPI3_MAN11_BKPLANE_NON_UBM_FLAGS_LINKWIDTH_4                   (0x0000)
#define MPI3_MAN11_BKPLANE_NON_UBM_FLAGS_LINKWIDTH_2                   (0x0040)
#define MPI3_MAN11_BKPLANE_NON_UBM_FLAGS_LINKWIDTH_1                   (0x0080)
#define MPI3_MAN11_BKPLANE_NON_UBM_FLAGS_PRESENCE_DETECT_MASK          (0x0030)
#define MPI3_MAN11_BKPLANE_NON_UBM_FLAGS_PRESENCE_DETECT_SHIFT         (4)
#define MPI3_MAN11_BKPLANE_NON_UBM_FLAGS_PRESENCE_DETECT_GPIO          (0x0000)
#define MPI3_MAN11_BKPLANE_NON_UBM_FLAGS_PRESENCE_DETECT_REG           (0x0010)
#define MPI3_MAN11_BKPLANE_NON_UBM_FLAGS_POLL_INTERVAL_MASK            (0x000F)
#define MPI3_MAN11_BKPLANE_NON_UBM_FLAGS_POLL_INTERVAL_SHIFT           (0)

/**** Defines for the Type field ****/
#define MPI3_MAN11_BKPLANE_NON_UBM_TYPE_VPP                            (0x00)

typedef union _MPI3_MAN11_BKPLANE_SPEC_FORMAT
{
    MPI3_MAN11_BKPLANE_SPEC_UBM_FORMAT         Ubm;
    MPI3_MAN11_BKPLANE_SPEC_NON_UBM_FORMAT     NonUbm;
} MPI3_MAN11_BKPLANE_SPEC_FORMAT, MPI3_POINTER PTR_MPI3_MAN11_BKPLANE_SPEC_FORMAT,
  Mpi3Man11BkplaneSpecFormat_t, MPI3_POINTER pMpi3Man11BkplaneSpecFormat_t;

typedef struct _MPI3_MAN11_BKPLANE_MGMT_DEVICE_FORMAT
{
    U8                                     Type;                   /* 0x00 */
    U8                                     ReceptacleID;           /* 0x01 */
    U8                                     ResetInfo;              /* 0x02 */
    U8                                     Reserved03;             /* 0x03 */
    MPI3_MAN11_BKPLANE_SPEC_FORMAT         BackplaneMgmtSpecific;  /* 0x04 */
} MPI3_MAN11_BKPLANE_MGMT_DEVICE_FORMAT, MPI3_POINTER PTR_MPI3_MAN11_BKPLANE_MGMT_DEVICE_FORMAT,
  Mpi3Man11BkplaneMgmtDeviceFormat_t, MPI3_POINTER pMpi3Man11BkplaneMgmtDeviceFormat_t;

/**** Defines for the Type field ****/
#define MPI3_MAN11_BKPLANE_MGMT_TYPE_UBM            (0x00)
#define MPI3_MAN11_BKPLANE_MGMT_TYPE_NON_UBM        (0x01)

/**** Defines for the ResetInfo field ****/
#define MPI3_MAN11_BACKPLANE_RESETINFO_ASSERT_TIME_MASK       (0xF0)
#define MPI3_MAN11_BACKPLANE_RESETINFO_ASSERT_TIME_SHIFT      (4)
#define MPI3_MAN11_BACKPLANE_RESETINFO_READY_TIME_MASK        (0x0F)
#define MPI3_MAN11_BACKPLANE_RESETINFO_READY_TIME_SHIFT       (0)

typedef struct _MPI3_MAN11_GAS_GAUGE_DEVICE_FORMAT
{
    U8      Type;               /* 0x00 */
    U8      Reserved01[3];      /* 0x01 */
    U32     Reserved04;         /* 0x04 */
} MPI3_MAN11_GAS_GAUGE_DEVICE_FORMAT, MPI3_POINTER PTR_MPI3_MAN11_GAS_GAUGE_DEVICE_FORMAT,
  Mpi3Man11GasGaugeDeviceFormat_t, MPI3_POINTER pMpi3Man11GasGaugeDeviceFormat_t;

/**** Defines for the Type field ****/
#define MPI3_MAN11_GAS_GAUGE_TYPE_STANDARD          (0x00)

typedef struct _MPI3_MAN11_MGMT_CTRLR_DEVICE_FORMAT
{
    U32     Reserved00;         /* 0x00 */
    U32     Reserved04;         /* 0x04 */
} MPI3_MAN11_MGMT_CTRLR_DEVICE_FORMAT, MPI3_POINTER PTR_MPI3_MAN11_MGMT_CTRLR_DEVICE_FORMAT,
  Mpi3Man11MgmtCtrlrDeviceFormat_t, MPI3_POINTER pMpi3Man11MgmtCtrlrDeviceFormat_t;

typedef struct _MPI3_MAN11_BOARD_FAN_DEVICE_FORMAT
{
    U8      Flags;              /* 0x00 */
    U8      Reserved01;         /* 0x01 */
    U8      MinFanSpeed;        /* 0x02 */
    U8      MaxFanSpeed;        /* 0x03 */
    U32     Reserved04;         /* 0x04 */
} MPI3_MAN11_BOARD_FAN_DEVICE_FORMAT, MPI3_POINTER PTR_MPI3_MAN11_BOARD_FAN_DEVICE_FORMAT,
  Mpi3Man11BoardFanDeviceFormat_t, MPI3_POINTER pMpi3Man11BoardFanDeviceFormat_t;

/**** Defines for the Flags field ****/
#define MPI3_MAN11_BOARD_FAN_FLAGS_FAN_CTRLR_TYPE_MASK        (0x07)
#define MPI3_MAN11_BOARD_FAN_FLAGS_FAN_CTRLR_TYPE_SHIFT       (0)
#define MPI3_MAN11_BOARD_FAN_FLAGS_FAN_CTRLR_TYPE_AMC6821     (0x00)

typedef union _MPI3_MAN11_DEVICE_SPECIFIC_FORMAT
{
    MPI3_MAN11_MUX_DEVICE_FORMAT            Mux;
    MPI3_MAN11_TEMP_SENSOR_DEVICE_FORMAT    TempSensor;
    MPI3_MAN11_SEEPROM_DEVICE_FORMAT        Seeprom;
    MPI3_MAN11_DDR_SPD_DEVICE_FORMAT        DdrSpd;
    MPI3_MAN11_CABLE_MGMT_DEVICE_FORMAT     CableMgmt;
    MPI3_MAN11_BKPLANE_MGMT_DEVICE_FORMAT   BkplaneMgmt;
    MPI3_MAN11_GAS_GAUGE_DEVICE_FORMAT      GasGauge;
    MPI3_MAN11_MGMT_CTRLR_DEVICE_FORMAT     MgmtController;
    MPI3_MAN11_BOARD_FAN_DEVICE_FORMAT      BoardFan;
    U32                                     Words[2];
} MPI3_MAN11_DEVICE_SPECIFIC_FORMAT, MPI3_POINTER PTR_MPI3_MAN11_DEVICE_SPECIFIC_FORMAT,
  Mpi3Man11DeviceSpecificFormat_t, MPI3_POINTER pMpi3Man11DeviceSpecificFormat_t;

typedef struct _MPI3_MAN11_ISTWI_DEVICE_FORMAT
{
    U8                                  DeviceType;         /* 0x00 */
    U8                                  Controller;         /* 0x01 */
    U8                                  Reserved02;         /* 0x02 */
    U8                                  Flags;              /* 0x03 */
    U16                                 DeviceAddress;      /* 0x04 */
    U8                                  MuxChannel;         /* 0x06 */
    U8                                  MuxIndex;           /* 0x07 */
    MPI3_MAN11_DEVICE_SPECIFIC_FORMAT   DeviceSpecific;     /* 0x08 */
} MPI3_MAN11_ISTWI_DEVICE_FORMAT, MPI3_POINTER PTR_MPI3_MAN11_ISTWI_DEVICE_FORMAT,
  Mpi3Man11IstwiDeviceFormat_t, MPI3_POINTER pMpi3Man11IstwiDeviceFormat_t;

/**** Defines for the DeviceType field ****/
#define MPI3_MAN11_ISTWI_DEVTYPE_MUX                  (0x00)
#define MPI3_MAN11_ISTWI_DEVTYPE_TEMP_SENSOR          (0x01)
#define MPI3_MAN11_ISTWI_DEVTYPE_SEEPROM              (0x02)
#define MPI3_MAN11_ISTWI_DEVTYPE_DDR_SPD              (0x03)
#define MPI3_MAN11_ISTWI_DEVTYPE_CABLE_MGMT           (0x04)
#define MPI3_MAN11_ISTWI_DEVTYPE_BACKPLANE_MGMT       (0x05)
#define MPI3_MAN11_ISTWI_DEVTYPE_GAS_GAUGE            (0x06)
#define MPI3_MAN11_ISTWI_DEVTYPE_MGMT_CONTROLLER      (0x07)
#define MPI3_MAN11_ISTWI_DEVTYPE_BOARD_FAN            (0x08)

/**** Defines for the Flags field ****/
#define MPI3_MAN11_ISTWI_FLAGS_MUX_PRESENT            (0x01)

#ifndef MPI3_MAN11_ISTWI_DEVICE_MAX
#define MPI3_MAN11_ISTWI_DEVICE_MAX             (1)
#endif  /* MPI3_MAN11_ISTWI_DEVICE_MAX */

typedef struct _MPI3_MAN_PAGE11
{
    MPI3_CONFIG_PAGE_HEADER         Header;                                     /* 0x00 */
    U32                             Reserved08;                                 /* 0x08 */
    U8                              NumISTWIDev;                                /* 0x0C */
    U8                              Reserved0D[3];                              /* 0x0D */
    MPI3_MAN11_ISTWI_DEVICE_FORMAT  ISTWIDevice[MPI3_MAN11_ISTWI_DEVICE_MAX];   /* 0x10 */
} MPI3_MAN_PAGE11, MPI3_POINTER PTR_MPI3_MAN_PAGE11,
  Mpi3ManPage11_t, MPI3_POINTER pMpi3ManPage11_t;

/**** Defines for the PageVersion field ****/
#define MPI3_MAN11_PAGEVERSION                  (0x00)


/*****************************************************************************
 *              Manufacturing Page 12                                        *
 ****************************************************************************/
#ifndef MPI3_MAN12_NUM_SGPIO_MAX
#define MPI3_MAN12_NUM_SGPIO_MAX                                     (1)
#endif  /* MPI3_MAN12_NUM_SGPIO_MAX */

typedef struct _MPI3_MAN12_SGPIO_INFO
{
    U8                              SlotCount;                                  /* 0x00 */
    U8                              Reserved01[3];                              /* 0x01 */
    U32                             Reserved04;                                 /* 0x04 */
    U8                              PhyOrder[32];                               /* 0x08 */
} MPI3_MAN12_SGPIO_INFO, MPI3_POINTER PTR_MPI3_MAN12_SGPIO_INFO,
  Mpi3Man12SGPIOInfo_t, MPI3_POINTER pMpi3Man12SGPIOInfo_t;

typedef struct _MPI3_MAN_PAGE12
{
    MPI3_CONFIG_PAGE_HEADER         Header;                                     /* 0x00 */
    U32                             Flags;                                      /* 0x08 */
    U32                             SClockFreq;                                 /* 0x0C */
    U32                             ActivityModulation;                         /* 0x10 */
    U8                              NumSGPIO;                                   /* 0x14 */
    U8                              Reserved15[3];                              /* 0x15 */
    U32                             Reserved18;                                 /* 0x18 */
    U32                             Reserved1C;                                 /* 0x1C */
    U32                             Pattern[8];                                 /* 0x20 */
    MPI3_MAN12_SGPIO_INFO           SGPIOInfo[MPI3_MAN12_NUM_SGPIO_MAX];        /* 0x40 */   /* variable length */
} MPI3_MAN_PAGE12, MPI3_POINTER PTR_MPI3_MAN_PAGE12,
  Mpi3ManPage12_t, MPI3_POINTER pMpi3ManPage12_t;

/**** Defines for the PageVersion field ****/
#define MPI3_MAN12_PAGEVERSION                                       (0x00)

/**** Defines for the Flags field ****/
#define MPI3_MAN12_FLAGS_ERROR_PRESENCE_ENABLED                      (0x0400)
#define MPI3_MAN12_FLAGS_ACTIVITY_INVERT_ENABLED                     (0x0200)
#define MPI3_MAN12_FLAGS_GROUP_ID_DISABLED                           (0x0100)
#define MPI3_MAN12_FLAGS_SIO_CLK_FILTER_ENABLED                      (0x0004)
#define MPI3_MAN12_FLAGS_SCLOCK_SLOAD_TYPE_MASK                      (0x0002)
#define MPI3_MAN12_FLAGS_SCLOCK_SLOAD_TYPE_SHIFT                     (1)
#define MPI3_MAN12_FLAGS_SCLOCK_SLOAD_TYPE_PUSH_PULL                 (0x0000)
#define MPI3_MAN12_FLAGS_SCLOCK_SLOAD_TYPE_OPEN_DRAIN                (0x0002)
#define MPI3_MAN12_FLAGS_SDATAOUT_TYPE_MASK                          (0x0001)
#define MPI3_MAN12_FLAGS_SDATAOUT_TYPE_SHIFT                         (0)
#define MPI3_MAN12_FLAGS_SDATAOUT_TYPE_PUSH_PULL                     (0x0000)
#define MPI3_MAN12_FLAGS_SDATAOUT_TYPE_OPEN_DRAIN                    (0x0001)

/**** Defines for the SClockFreq field ****/
#define MPI3_MAN12_SIO_CLK_FREQ_MIN                                  (32)        /* 32 Hz min SIO Clk Freq */
#define MPI3_MAN12_SIO_CLK_FREQ_MAX                                  (100000)    /* 100 KHz max SIO Clk Freq */

/**** Defines for the ActivityModulation field ****/
#define MPI3_MAN12_ACTIVITY_MODULATION_FORCE_OFF_MASK                (0x0000F000)
#define MPI3_MAN12_ACTIVITY_MODULATION_FORCE_OFF_SHIFT               (12)
#define MPI3_MAN12_ACTIVITY_MODULATION_MAX_ON_MASK                   (0x00000F00)
#define MPI3_MAN12_ACTIVITY_MODULATION_MAX_ON_SHIFT                  (8)
#define MPI3_MAN12_ACTIVITY_MODULATION_STRETCH_OFF_MASK              (0x000000F0)
#define MPI3_MAN12_ACTIVITY_MODULATION_STRETCH_OFF_SHIFT             (4)
#define MPI3_MAN12_ACTIVITY_MODULATION_STRETCH_ON_MASK               (0x0000000F)
#define MPI3_MAN12_ACTIVITY_MODULATION_STRETCH_ON_SHIFT              (0)

/*** Defines for the Pattern field ****/
#define MPI3_MAN12_PATTERN_RATE_MASK                                 (0xE0000000)
#define MPI3_MAN12_PATTERN_RATE_SHIFT                                (29)
#define MPI3_MAN12_PATTERN_RATE_2_HZ                                 (0x00000000)
#define MPI3_MAN12_PATTERN_RATE_4_HZ                                 (0x20000000)
#define MPI3_MAN12_PATTERN_RATE_8_HZ                                 (0x40000000)
#define MPI3_MAN12_PATTERN_RATE_16_HZ                                (0x60000000)
#define MPI3_MAN12_PATTERN_RATE_10_HZ                                (0x80000000)
#define MPI3_MAN12_PATTERN_RATE_20_HZ                                (0xA0000000)
#define MPI3_MAN12_PATTERN_RATE_40_HZ                                (0xC0000000)
#define MPI3_MAN12_PATTERN_LENGTH_MASK                               (0x1F000000)
#define MPI3_MAN12_PATTERN_LENGTH_SHIFT                              (24)
#define MPI3_MAN12_PATTERN_BIT_PATTERN_MASK                          (0x00FFFFFF)
#define MPI3_MAN12_PATTERN_BIT_PATTERN_SHIFT                         (0)


/*****************************************************************************
 *              Manufacturing Page 13                                        *
 ****************************************************************************/

#ifndef MPI3_MAN13_NUM_TRANSLATION_MAX
#define MPI3_MAN13_NUM_TRANSLATION_MAX                               (1)
#endif  /* MPI3_MAN13_NUM_TRANSLATION_MAX */

typedef struct _MPI3_MAN13_TRANSLATION_INFO
{
    U32                             SlotStatus;                                        /* 0x00 */
    U32                             Mask;                                              /* 0x04 */
    U8                              Activity;                                          /* 0x08 */
    U8                              Locate;                                            /* 0x09 */
    U8                              Error;                                             /* 0x0A */
    U8                              Reserved0B;                                        /* 0x0B */
} MPI3_MAN13_TRANSLATION_INFO, MPI3_POINTER PTR_MPI3_MAN13_TRANSLATION_INFO,
  Mpi3Man13TranslationInfo_t, MPI3_POINTER pMpi3Man13TranslationInfo_t;

/**** Defines for the SlotStatus field ****/
#define MPI3_MAN13_TRANSLATION_SLOTSTATUS_FAULT                     (0x20000000)
#define MPI3_MAN13_TRANSLATION_SLOTSTATUS_DEVICE_OFF                (0x10000000)
#define MPI3_MAN13_TRANSLATION_SLOTSTATUS_DEVICE_ACTIVITY           (0x00800000)
#define MPI3_MAN13_TRANSLATION_SLOTSTATUS_DO_NOT_REMOVE             (0x00400000)
#define MPI3_MAN13_TRANSLATION_SLOTSTATUS_DEVICE_MISSING            (0x00100000)
#define MPI3_MAN13_TRANSLATION_SLOTSTATUS_INSERT                    (0x00080000)
#define MPI3_MAN13_TRANSLATION_SLOTSTATUS_REMOVAL                   (0x00040000)
#define MPI3_MAN13_TRANSLATION_SLOTSTATUS_IDENTIFY                  (0x00020000)
#define MPI3_MAN13_TRANSLATION_SLOTSTATUS_OK                        (0x00008000)
#define MPI3_MAN13_TRANSLATION_SLOTSTATUS_RESERVED_DEVICE           (0x00004000)
#define MPI3_MAN13_TRANSLATION_SLOTSTATUS_HOT_SPARE                 (0x00002000)
#define MPI3_MAN13_TRANSLATION_SLOTSTATUS_CONSISTENCY_CHECK         (0x00001000)
#define MPI3_MAN13_TRANSLATION_SLOTSTATUS_IN_CRITICAL_ARRAY         (0x00000800)
#define MPI3_MAN13_TRANSLATION_SLOTSTATUS_IN_FAILED_ARRAY           (0x00000400)
#define MPI3_MAN13_TRANSLATION_SLOTSTATUS_REBUILD_REMAP             (0x00000200)
#define MPI3_MAN13_TRANSLATION_SLOTSTATUS_REBUILD_REMAP_ABORT       (0x00000100)
#define MPI3_MAN13_TRANSLATION_SLOTSTATUS_PREDICTED_FAILURE         (0x00000040)

/**** Defines for the Mask field - use MPI3_MAN13_TRANSLATION_SLOTSTATUS_ defines ****/

/**** Defines for the Activity, Locate, and Error fields ****/
#define MPI3_MAN13_BLINK_PATTERN_FORCE_OFF                          (0x00)
#define MPI3_MAN13_BLINK_PATTERN_FORCE_ON                           (0x01)
#define MPI3_MAN13_BLINK_PATTERN_PATTERN_0                          (0x02)
#define MPI3_MAN13_BLINK_PATTERN_PATTERN_1                          (0x03)
#define MPI3_MAN13_BLINK_PATTERN_PATTERN_2                          (0x04)
#define MPI3_MAN13_BLINK_PATTERN_PATTERN_3                          (0x05)
#define MPI3_MAN13_BLINK_PATTERN_PATTERN_4                          (0x06)
#define MPI3_MAN13_BLINK_PATTERN_PATTERN_5                          (0x07)
#define MPI3_MAN13_BLINK_PATTERN_PATTERN_6                          (0x08)
#define MPI3_MAN13_BLINK_PATTERN_PATTERN_7                          (0x09)
#define MPI3_MAN13_BLINK_PATTERN_ACTIVITY                           (0x0A)
#define MPI3_MAN13_BLINK_PATTERN_ACTIVITY_TRAIL                     (0x0B)

typedef struct _MPI3_MAN_PAGE13
{
    MPI3_CONFIG_PAGE_HEADER         Header;                                            /* 0x00 */
    U8                              NumTrans;                                          /* 0x08 */
    U8                              Reserved09[3];                                     /* 0x09 */
    U32                             Reserved0C;                                        /* 0x0C */
    MPI3_MAN13_TRANSLATION_INFO     Translation[MPI3_MAN13_NUM_TRANSLATION_MAX];       /* 0x10 */  /* variable length */
} MPI3_MAN_PAGE13, MPI3_POINTER PTR_MPI3_MAN_PAGE13,
  Mpi3ManPage13_t, MPI3_POINTER pMpi3ManPage13_t;

/**** Defines for the PageVersion field ****/
#define MPI3_MAN13_PAGEVERSION                                       (0x00)

/*****************************************************************************
 *              Manufacturing Page 14                                        *
 ****************************************************************************/

typedef struct _MPI3_MAN_PAGE14
{
    MPI3_CONFIG_PAGE_HEADER         Header;                                            /* 0x00 */
    U32                             Reserved08;                                        /* 0x08 */
    U8                              NumSlotGroups;                                     /* 0x0C */
    U8                              NumSlots;                                          /* 0x0D */
    U16                             MaxCertChainLength;                                /* 0x0E */
    U32                             SealedSlots;                                       /* 0x10 */
    U32                             PopulatedSlots;                                    /* 0x14 */
    U32                             MgmtPTUpdatableSlots;                              /* 0x18 */
} MPI3_MAN_PAGE14, MPI3_POINTER PTR_MPI3_MAN_PAGE14,
  Mpi3ManPage14_t, MPI3_POINTER pMpi3ManPage14_t;

/**** Defines for the PageVersion field ****/
#define MPI3_MAN14_PAGEVERSION                                       (0x00)

/**** Defines for the NumSlots field ****/
#define MPI3_MAN14_NUMSLOTS_MAX                                      (32)

/*****************************************************************************
 *              Manufacturing Page 15                                        *
 ****************************************************************************/

#ifndef MPI3_MAN15_VERSION_RECORD_MAX
#define MPI3_MAN15_VERSION_RECORD_MAX      1
#endif  /* MPI3_MAN15_VERSION_RECORD_MAX */

typedef struct _MPI3_MAN15_VERSION_RECORD
{
    U16                             SPDMVersion;                                       /* 0x00 */
    U16                             Reserved02;                                        /* 0x02 */
} MPI3_MAN15_VERSION_RECORD, MPI3_POINTER PTR_MPI3_MAN15_VERSION_RECORD,
  Mpi3Man15VersionRecord_t, MPI3_POINTER pMpi3Man15VersionRecord_t;

typedef struct _MPI3_MAN_PAGE15
{
    MPI3_CONFIG_PAGE_HEADER         Header;                                            /* 0x00 */
    U8                              NumVersionRecords;                                 /* 0x08 */
    U8                              Reserved09[3];                                     /* 0x09 */
    U32                             Reserved0C;                                        /* 0x0C */
    MPI3_MAN15_VERSION_RECORD       VersionRecord[MPI3_MAN15_VERSION_RECORD_MAX];      /* 0x10 */
} MPI3_MAN_PAGE15, MPI3_POINTER PTR_MPI3_MAN_PAGE15,
  Mpi3ManPage15_t, MPI3_POINTER pMpi3ManPage15_t;

/**** Defines for the PageVersion field ****/
#define MPI3_MAN15_PAGEVERSION                                       (0x00)

/*****************************************************************************
 *              Manufacturing Page 16                                        *
 ****************************************************************************/

#ifndef MPI3_MAN16_CERT_ALGO_MAX
#define MPI3_MAN16_CERT_ALGO_MAX      1
#endif  /* MPI3_MAN16_CERT_ALGO_MAX */

typedef struct _MPI3_MAN16_CERTIFICATE_ALGORITHM
{
    U8                                   SlotGroup;                                    /* 0x00 */
    U8                                   Reserved01[3];                                /* 0x01 */
    U32                                  BaseAsymAlgo;                                 /* 0x04 */
    U32                                  BaseHashAlgo;                                 /* 0x08 */
    U32                                  Reserved0C[3];                                /* 0x0C */
} MPI3_MAN16_CERTIFICATE_ALGORITHM, MPI3_POINTER PTR_MPI3_MAN16_CERTIFICATE_ALGORITHM,
  Mpi3Man16CertificateAlgorithm_t, MPI3_POINTER pMpi3Man16CertificateAlgorithm_t;

typedef struct _MPI3_MAN_PAGE16
{
    MPI3_CONFIG_PAGE_HEADER              Header;                                         /* 0x00 */
    U32                                  Reserved08;                                     /* 0x08 */
    U8                                   NumCertAlgos;                                   /* 0x0C */
    U8                                   Reserved0D[3];                                  /* 0x0D */
    MPI3_MAN16_CERTIFICATE_ALGORITHM     CertificateAlgorithm[MPI3_MAN16_CERT_ALGO_MAX]; /* 0x10 */
} MPI3_MAN_PAGE16, MPI3_POINTER PTR_MPI3_MAN_PAGE16,
  Mpi3ManPage16_t, MPI3_POINTER pMpi3ManPage16_t;

/**** Defines for the PageVersion field ****/
#define MPI3_MAN16_PAGEVERSION                                       (0x00)

/*****************************************************************************
 *              Manufacturing Page 17                                        *
 ****************************************************************************/

#ifndef MPI3_MAN17_HASH_ALGORITHM_MAX
#define MPI3_MAN17_HASH_ALGORITHM_MAX      1
#endif  /* MPI3_MAN17_HASH_ALGORITHM_MAX */

typedef struct _MPI3_MAN17_HASH_ALGORITHM
{
    U8                              MeasSpecification;                                 /* 0x00 */
    U8                              Reserved01[3];                                     /* 0x01 */
    U32                             MeasurementHashAlgo;                               /* 0x04 */
    U32                             Reserved08[2];                                     /* 0x08 */
} MPI3_MAN17_HASH_ALGORITHM, MPI3_POINTER PTR_MPI3_MAN17_HASH_ALGORITHM,
  Mpi3Man17HashAlgorithm_t, MPI3_POINTER pMpi3Man17HashAlgorithm_t;

typedef struct _MPI3_MAN_PAGE17
{
    MPI3_CONFIG_PAGE_HEADER         Header;                                            /* 0x00 */
    U32                             Reserved08;                                        /* 0x08 */
    U8                              NumHashAlgos;                                      /* 0x0C */
    U8                              Reserved0D[3];                                     /* 0x0D */
    MPI3_MAN17_HASH_ALGORITHM       HashAlgorithm[MPI3_MAN17_HASH_ALGORITHM_MAX];      /* 0x10 */
} MPI3_MAN_PAGE17, MPI3_POINTER PTR_MPI3_MAN_PAGE17,
  Mpi3ManPage17_t, MPI3_POINTER pMpi3ManPage17_t;

/**** Defines for the PageVersion field ****/
#define MPI3_MAN17_PAGEVERSION                                       (0x00)

/*****************************************************************************
 *              Manufacturing Page 20                                        *
 ****************************************************************************/

typedef struct _MPI3_MAN_PAGE20
{
    MPI3_CONFIG_PAGE_HEADER         Header;                                            /* 0x00 */
    U32                             Reserved08;                                        /* 0x08 */
    U32                             NonpremiumFeatures;                                /* 0x0C */
    U8                              AllowedPersonalities;                              /* 0x10 */
    U8                              Reserved11[3];                                     /* 0x11 */
} MPI3_MAN_PAGE20, MPI3_POINTER PTR_MPI3_MAN_PAGE20,
  Mpi3ManPage20_t, MPI3_POINTER pMpi3ManPage20_t;

/**** Defines for the PageVersion field ****/
#define MPI3_MAN20_PAGEVERSION                                       (0x00)

/**** Defines for the AllowedPersonalities field ****/
#define MPI3_MAN20_ALLOWEDPERSON_RAID_MASK                           (0x02)
#define MPI3_MAN20_ALLOWEDPERSON_RAID_SHIFT                          (1)
#define MPI3_MAN20_ALLOWEDPERSON_RAID_ALLOWED                        (0x02)
#define MPI3_MAN20_ALLOWEDPERSON_RAID_NOT_ALLOWED                    (0x00)
#define MPI3_MAN20_ALLOWEDPERSON_EHBA_MASK                           (0x01)
#define MPI3_MAN20_ALLOWEDPERSON_EHBA_SHIFT                          (0)
#define MPI3_MAN20_ALLOWEDPERSON_EHBA_ALLOWED                        (0x01)
#define MPI3_MAN20_ALLOWEDPERSON_EHBA_NOT_ALLOWED                    (0x00)

/**** Defines for the NonpremiumFeatures field ****/
#define MPI3_MAN20_NONPREMUIM_DISABLE_PD_DEGRADED_MASK               (0x01)
#define MPI3_MAN20_NONPREMUIM_DISABLE_PD_DEGRADED_SHIFT              (0)
#define MPI3_MAN20_NONPREMUIM_DISABLE_PD_DEGRADED_ENABLED            (0x00)
#define MPI3_MAN20_NONPREMUIM_DISABLE_PD_DEGRADED_DISABLED           (0x01)

/*****************************************************************************
 *              Manufacturing Page 21                                        *
 ****************************************************************************/

typedef struct _MPI3_MAN_PAGE21
{
    MPI3_CONFIG_PAGE_HEADER         Header;                                            /* 0x00 */
    U32                             Reserved08;                                        /* 0x08 */
    U32                             Flags;                                             /* 0x0C */
} MPI3_MAN_PAGE21, MPI3_POINTER PTR_MPI3_MAN_PAGE21,
  Mpi3ManPage21_t, MPI3_POINTER pMpi3ManPage21_t;

/**** Defines for the PageVersion field ****/
#define MPI3_MAN21_PAGEVERSION                                       (0x00)

/**** Defines for the Flags field ****/
#define MPI3_MAN21_FLAGS_UNCERTIFIED_DRIVES_MASK                     (0x00000060)
#define MPI3_MAN21_FLAGS_UNCERTIFIED_DRIVES_SHIFT                    (5)
#define MPI3_MAN21_FLAGS_UNCERTIFIED_DRIVES_BLOCK                    (0x00000000)
#define MPI3_MAN21_FLAGS_UNCERTIFIED_DRIVES_ALLOW                    (0x00000020)
#define MPI3_MAN21_FLAGS_UNCERTIFIED_DRIVES_WARN                     (0x00000040)
#define MPI3_MAN21_FLAGS_BLOCK_SSD_WR_CACHE_CHANGE_MASK              (0x00000008)
#define MPI3_MAN21_FLAGS_BLOCK_SSD_WR_CACHE_CHANGE_SHIFT             (3)
#define MPI3_MAN21_FLAGS_BLOCK_SSD_WR_CACHE_CHANGE_ALLOW             (0x00000000)
#define MPI3_MAN21_FLAGS_BLOCK_SSD_WR_CACHE_CHANGE_PREVENT           (0x00000008)
#define MPI3_MAN21_FLAGS_SES_VPD_ASSOC_MASK                          (0x00000001)
#define MPI3_MAN21_FLAGS_SES_VPD_ASSOC_SHIFT                         (0)
#define MPI3_MAN21_FLAGS_SES_VPD_ASSOC_DEFAULT                       (0x00000000)
#define MPI3_MAN21_FLAGS_SES_VPD_ASSOC_OEM_SPECIFIC                  (0x00000001)

/*****************************************************************************
 *              Manufacturing Page 22                                        *
 ****************************************************************************/

typedef struct _MPI3_MAN_PAGE22
{
    MPI3_CONFIG_PAGE_HEADER         Header;                                            /* 0x00 */
    U32                             Reserved08;                                        /* 0x08 */
    U16                             NumEUI64;                                          /* 0x0C */
    U16                             Reserved0E;                                        /* 0x0E */
    U64                             BaseEUI64;                                         /* 0x10 */
} MPI3_MAN_PAGE22, MPI3_POINTER PTR_MPI3_MAN_PAGE22,
  Mpi3ManPage22_t, MPI3_POINTER pMpi3ManPage22_t;

/**** Defines for the PageVersion field ****/
#define MPI3_MAN22_PAGEVERSION                                       (0x00)

/*****************************************************************************
 *              Manufacturing Pages 32-63 (ProductSpecific)                  *
 ****************************************************************************/
#ifndef MPI3_MAN_PROD_SPECIFIC_MAX
#define MPI3_MAN_PROD_SPECIFIC_MAX                      (1)
#endif  /* MPI3_MAN_PROD_SPECIFIC_MAX */

typedef struct _MPI3_MAN_PAGE_PRODUCT_SPECIFIC
{
    MPI3_CONFIG_PAGE_HEADER         Header;                                            /* 0x00 */
    U32                             ProductSpecificInfo[MPI3_MAN_PROD_SPECIFIC_MAX];   /* 0x08 */  /* variable length array */
} MPI3_MAN_PAGE_PRODUCT_SPECIFIC, MPI3_POINTER PTR_MPI3_MAN_PAGE_PRODUCT_SPECIFIC,
  Mpi3ManPageProductSpecific_t, MPI3_POINTER pMpi3ManPageProductSpecific_t;

/*****************************************************************************
 *              IO Unit Configuration Pages                                  *
 ****************************************************************************/

/*****************************************************************************
 *              IO Unit Page 0                                               *
 ****************************************************************************/
typedef struct _MPI3_IO_UNIT_PAGE0
{
    MPI3_CONFIG_PAGE_HEADER         Header;                     /* 0x00 */
    U64                             UniqueValue;                /* 0x08 */
    U32                             NvdataVersionDefault;       /* 0x10 */
    U32                             NvdataVersionPersistent;    /* 0x14 */
} MPI3_IO_UNIT_PAGE0, MPI3_POINTER PTR_MPI3_IO_UNIT_PAGE0,
  Mpi3IOUnitPage0_t, MPI3_POINTER pMpi3IOUnitPage0_t;

/**** Defines for the PageVersion field ****/
#define MPI3_IOUNIT0_PAGEVERSION                (0x00)

/*****************************************************************************
 *              IO Unit Page 1                                               *
 ****************************************************************************/
typedef struct _MPI3_IO_UNIT_PAGE1
{
    MPI3_CONFIG_PAGE_HEADER         Header;                     /* 0x00 */
    U32                             Flags;                      /* 0x08 */
    U8                              DMDIoDelay;                 /* 0x0C */
    U8                              DMDReportPCIe;              /* 0x0D */
    U8                              DMDReportSATA;              /* 0x0E */
    U8                              DMDReportSAS;               /* 0x0F */
} MPI3_IO_UNIT_PAGE1, MPI3_POINTER PTR_MPI3_IO_UNIT_PAGE1,
  Mpi3IOUnitPage1_t, MPI3_POINTER pMpi3IOUnitPage1_t;

/**** Defines for the PageVersion field ****/
#define MPI3_IOUNIT1_PAGEVERSION                (0x00)

/**** Defines for the Flags field ****/
#define MPI3_IOUNIT1_FLAGS_NVME_WRITE_CACHE_MASK                   (0x00000030)
#define MPI3_IOUNIT1_FLAGS_NVME_WRITE_CACHE_SHIFT                  (4)
#define MPI3_IOUNIT1_FLAGS_NVME_WRITE_CACHE_ENABLE                 (0x00000000)
#define MPI3_IOUNIT1_FLAGS_NVME_WRITE_CACHE_DISABLE                (0x00000010)
#define MPI3_IOUNIT1_FLAGS_NVME_WRITE_CACHE_NO_MODIFY              (0x00000020)
#define MPI3_IOUNIT1_FLAGS_ATA_SECURITY_FREEZE_LOCK                (0x00000008)
#define MPI3_IOUNIT1_FLAGS_WRITE_SAME_BUFFER                       (0x00000004)
#define MPI3_IOUNIT1_FLAGS_SATA_WRITE_CACHE_MASK                   (0x00000003)
#define MPI3_IOUNIT1_FLAGS_SATA_WRITE_CACHE_SHIFT                  (0)
#define MPI3_IOUNIT1_FLAGS_SATA_WRITE_CACHE_ENABLE                 (0x00000000)
#define MPI3_IOUNIT1_FLAGS_SATA_WRITE_CACHE_DISABLE                (0x00000001)
#define MPI3_IOUNIT1_FLAGS_SATA_WRITE_CACHE_UNCHANGED              (0x00000002)

/**** Defines for the DMDReport PCIe/SATA/SAS fields ****/
#define MPI3_IOUNIT1_DMD_REPORT_DELAY_TIME_MASK                    (0x7F)
#define MPI3_IOUNIT1_DMD_REPORT_DELAY_TIME_SHIFT                   (0)
#define MPI3_IOUNIT1_DMD_REPORT_UNIT_16_SEC                        (0x80)

/*****************************************************************************
 *              IO Unit Page 2                                               *
 ****************************************************************************/
#ifndef MPI3_IO_UNIT2_GPIO_VAL_MAX
#define MPI3_IO_UNIT2_GPIO_VAL_MAX      (1)
#endif  /* MPI3_IO_UNIT2_GPIO_VAL_MAX */

typedef struct _MPI3_IO_UNIT_PAGE2
{
    MPI3_CONFIG_PAGE_HEADER         Header;                                 /* 0x00 */
    U8                              GPIOCount;                              /* 0x08 */
    U8                              Reserved09[3];                          /* 0x09 */
    U16                             GPIOVal[MPI3_IO_UNIT2_GPIO_VAL_MAX];    /* 0x0C */
} MPI3_IO_UNIT_PAGE2, MPI3_POINTER PTR_MPI3_IO_UNIT_PAGE2,
  Mpi3IOUnitPage2_t, MPI3_POINTER pMpi3IOUnitPage2_t;

/**** Defines for the PageVersion field ****/
#define MPI3_IOUNIT2_PAGEVERSION                (0x00)

/**** Define for the GPIOVal field ****/
#define MPI3_IOUNIT2_GPIO_FUNCTION_MASK         (0xFFFC)
#define MPI3_IOUNIT2_GPIO_FUNCTION_SHIFT        (2)
#define MPI3_IOUNIT2_GPIO_SETTING_MASK          (0x0001)
#define MPI3_IOUNIT2_GPIO_SETTING_SHIFT         (0)
#define MPI3_IOUNIT2_GPIO_SETTING_OFF           (0x0000)
#define MPI3_IOUNIT2_GPIO_SETTING_ON            (0x0001)

/*****************************************************************************
 *              IO Unit Page 3                                               *
 ****************************************************************************/

typedef enum _MPI3_IOUNIT3_THRESHOLD
{
    MPI3_IOUNIT3_THRESHOLD_WARNING              = 0,
    MPI3_IOUNIT3_THRESHOLD_CRITICAL             = 1,
    MPI3_IOUNIT3_THRESHOLD_FATAL                = 2,
    MPI3_IOUNIT3_THRESHOLD_LOW                  = 3,
    MPI3_IOUNIT3_NUM_THRESHOLDS
} MPI3_IOUNIT3_THRESHOLD;

typedef struct _MPI3_IO_UNIT3_SENSOR
{
    U16             Flags;                                      /* 0x00 */
    U8              ThresholdMargin;                            /* 0x02 */
    U8              Reserved03;                                 /* 0x03 */
    U16             Threshold[MPI3_IOUNIT3_NUM_THRESHOLDS];     /* 0x04 */
    U32             Reserved0C;                                 /* 0x0C */
    U32             Reserved10;                                 /* 0x10 */
    U32             Reserved14;                                 /* 0x14 */
} MPI3_IO_UNIT3_SENSOR, MPI3_POINTER PTR_MPI3_IO_UNIT3_SENSOR,
  Mpi3IOUnit3Sensor_t, MPI3_POINTER pMpi3IOUnit3Sensor_t;

/**** Defines for the Flags field ****/
#define MPI3_IOUNIT3_SENSOR_FLAGS_LOW_THRESHOLD_VALID           (0x0020)
#define MPI3_IOUNIT3_SENSOR_FLAGS_FATAL_EVENT_ENABLED           (0x0010)
#define MPI3_IOUNIT3_SENSOR_FLAGS_FATAL_ACTION_ENABLED          (0x0008)
#define MPI3_IOUNIT3_SENSOR_FLAGS_CRITICAL_EVENT_ENABLED        (0x0004)
#define MPI3_IOUNIT3_SENSOR_FLAGS_CRITICAL_ACTION_ENABLED       (0x0002)
#define MPI3_IOUNIT3_SENSOR_FLAGS_WARNING_EVENT_ENABLED         (0x0001)

#ifndef MPI3_IO_UNIT3_SENSOR_MAX
#define MPI3_IO_UNIT3_SENSOR_MAX                                (1)
#endif  /* MPI3_IO_UNIT3_SENSOR_MAX */

typedef struct _MPI3_IO_UNIT_PAGE3
{
    MPI3_CONFIG_PAGE_HEADER         Header;                             /* 0x00 */
    U32                             Reserved08;                         /* 0x08 */
    U8                              NumSensors;                         /* 0x0C */
    U8                              NominalPollInterval;                /* 0x0D */
    U8                              WarningPollInterval;                /* 0x0E */
    U8                              Reserved0F;                         /* 0x0F */
    MPI3_IO_UNIT3_SENSOR            Sensor[MPI3_IO_UNIT3_SENSOR_MAX];   /* 0x10 */
} MPI3_IO_UNIT_PAGE3, MPI3_POINTER PTR_MPI3_IO_UNIT_PAGE3,
  Mpi3IOUnitPage3_t, MPI3_POINTER pMpi3IOUnitPage3_t;

/**** Defines for the PageVersion field ****/
#define MPI3_IOUNIT3_PAGEVERSION                (0x00)


/*****************************************************************************
 *              IO Unit Page 4                                               *
 ****************************************************************************/
typedef struct _MPI3_IO_UNIT4_SENSOR
{
    U16             CurrentTemperature;     /* 0x00 */
    U16             Reserved02;             /* 0x02 */
    U8              Flags;                  /* 0x04 */
    U8              Reserved05[3];          /* 0x05 */
    U16             ISTWIIndex;             /* 0x08 */
    U8              Channel;                /* 0x0A */
    U8              Reserved0B;             /* 0x0B */
    U32             Reserved0C;             /* 0x0C */
} MPI3_IO_UNIT4_SENSOR, MPI3_POINTER PTR_MPI3_IO_UNIT4_SENSOR,
  Mpi3IOUnit4Sensor_t, MPI3_POINTER pMpi3IOUnit4Sensor_t;

/**** Defines for the Flags field ****/
#define MPI3_IOUNIT4_SENSOR_FLAGS_LOC_MASK          (0xE0)
#define MPI3_IOUNIT4_SENSOR_FLAGS_LOC_SHIFT         (5)
/**** for the Location field values - use MPI3_TEMP_SENSOR_LOCATION_ defines ****/
#define MPI3_IOUNIT4_SENSOR_FLAGS_TEMP_VALID        (0x01)


/**** Defines for the ISTWIIndex field ****/
#define MPI3_IOUNIT4_SENSOR_ISTWI_INDEX_INTERNAL    (0xFFFF)

/**** Defines for the Channel field ****/
#define MPI3_IOUNIT4_SENSOR_CHANNEL_RESERVED        (0xFF)

#ifndef MPI3_IO_UNIT4_SENSOR_MAX
#define MPI3_IO_UNIT4_SENSOR_MAX                                (1)
#endif  /* MPI3_IO_UNIT4_SENSOR_MAX */

typedef struct _MPI3_IO_UNIT_PAGE4
{
    MPI3_CONFIG_PAGE_HEADER         Header;                             /* 0x00 */
    U32                             Reserved08;                         /* 0x08 */
    U8                              NumSensors;                         /* 0x0C */
    U8                              Reserved0D[3];                      /* 0x0D */
    MPI3_IO_UNIT4_SENSOR            Sensor[MPI3_IO_UNIT4_SENSOR_MAX];   /* 0x10 */
} MPI3_IO_UNIT_PAGE4, MPI3_POINTER PTR_MPI3_IO_UNIT_PAGE4,
  Mpi3IOUnitPage4_t, MPI3_POINTER pMpi3IOUnitPage4_t;

/**** Defines for the PageVersion field ****/
#define MPI3_IOUNIT4_PAGEVERSION                (0x00)

/*****************************************************************************
 *              IO Unit Page 5                                               *
 ****************************************************************************/
typedef struct _MPI3_IO_UNIT5_SPINUP_GROUP
{
    U8              MaxTargetSpinup;    /* 0x00 */
    U8              SpinupDelay;        /* 0x01 */
    U8              SpinupFlags;        /* 0x02 */
    U8              Reserved03;         /* 0x03 */
} MPI3_IO_UNIT5_SPINUP_GROUP, MPI3_POINTER PTR_MPI3_IO_UNIT5_SPINUP_GROUP,
  Mpi3IOUnit5SpinupGroup_t, MPI3_POINTER pMpi3IOUnit5SpinupGroup_t;

/**** Defines for the SpinupFlags field ****/
#define MPI3_IOUNIT5_SPINUP_FLAGS_DISABLE       (0x01)

#ifndef MPI3_IO_UNIT5_PHY_MAX
#define MPI3_IO_UNIT5_PHY_MAX       (4)
#endif  /* MPI3_IO_UNIT5_PHY_MAX */

typedef struct _MPI3_IO_UNIT_PAGE5
{
    MPI3_CONFIG_PAGE_HEADER         Header;                     /* 0x00 */
    MPI3_IO_UNIT5_SPINUP_GROUP      SpinupGroupParameters[4];   /* 0x08 */
    U32                             Reserved18;                 /* 0x18 */
    U32                             Reserved1C;                 /* 0x1C */
    U16                             DeviceShutdown;             /* 0x20 */
    U16                             Reserved22;                 /* 0x22 */
    U8                              PCIeDeviceWaitTime;         /* 0x24 */
    U8                              SATADeviceWaitTime;         /* 0x25 */
    U8                              SpinupEnclDriveCount;       /* 0x26 */
    U8                              SpinupEnclDelay;            /* 0x27 */
    U8                              NumPhys;                    /* 0x28 */
    U8                              PEInitialSpinupDelay;       /* 0x29 */
    U8                              TopologyStableTime;         /* 0x2A */
    U8                              Flags;                      /* 0x2B */
    U8                              Phy[MPI3_IO_UNIT5_PHY_MAX]; /* 0x2C */
} MPI3_IO_UNIT_PAGE5, MPI3_POINTER PTR_MPI3_IO_UNIT_PAGE5,
  Mpi3IOUnitPage5_t, MPI3_POINTER pMpi3IOUnitPage5_t;

/**** Defines for the PageVersion field ****/
#define MPI3_IOUNIT5_PAGEVERSION                           (0x00)

/**** Defines for the DeviceShutdown field ****/
#define MPI3_IOUNIT5_DEVICE_SHUTDOWN_NO_ACTION             (0x00)
#define MPI3_IOUNIT5_DEVICE_SHUTDOWN_DIRECT_ATTACHED       (0x01)
#define MPI3_IOUNIT5_DEVICE_SHUTDOWN_EXPANDER_ATTACHED     (0x02)
#define MPI3_IOUNIT5_DEVICE_SHUTDOWN_SWITCH_ATTACHED       (0x02)
#define MPI3_IOUNIT5_DEVICE_SHUTDOWN_DIRECT_AND_EXPANDER   (0x03)
#define MPI3_IOUNIT5_DEVICE_SHUTDOWN_DIRECT_AND_SWITCH     (0x03)

#define MPI3_IOUNIT5_DEVICE_SHUTDOWN_SATA_HDD_MASK         (0x0300)
#define MPI3_IOUNIT5_DEVICE_SHUTDOWN_SATA_HDD_SHIFT        (8)
#define MPI3_IOUNIT5_DEVICE_SHUTDOWN_SAS_HDD_MASK          (0x00C0)
#define MPI3_IOUNIT5_DEVICE_SHUTDOWN_SAS_HDD_SHIFT         (6)
#define MPI3_IOUNIT5_DEVICE_SHUTDOWN_NVME_SSD_MASK         (0x0030)
#define MPI3_IOUNIT5_DEVICE_SHUTDOWN_NVME_SSD_SHIFT        (4)
#define MPI3_IOUNIT5_DEVICE_SHUTDOWN_SATA_SSD_MASK         (0x000C)
#define MPI3_IOUNIT5_DEVICE_SHUTDOWN_SATA_SSD_SHIFT        (2)
#define MPI3_IOUNIT5_DEVICE_SHUTDOWN_SAS_SSD_MASK          (0x0003)
#define MPI3_IOUNIT5_DEVICE_SHUTDOWN_SAS_SSD_SHIFT         (0)

/**** Defines for the Flags field ****/
#define MPI3_IOUNIT5_FLAGS_SATAPUIS_MASK                   (0x0C)
#define MPI3_IOUNIT5_FLAGS_SATAPUIS_SHIFT                  (2)
#define MPI3_IOUNIT5_FLAGS_SATAPUIS_NOT_SUPPORTED          (0x00)
#define MPI3_IOUNIT5_FLAGS_SATAPUIS_OS_CONTROLLED          (0x04)
#define MPI3_IOUNIT5_FLAGS_SATAPUIS_APP_CONTROLLED         (0x08)
#define MPI3_IOUNIT5_FLAGS_SATAPUIS_BLOCKED                (0x0C)
#define MPI3_IOUNIT5_FLAGS_POWER_CAPABLE_SPINUP            (0x02)
#define MPI3_IOUNIT5_FLAGS_AUTO_PORT_ENABLE                (0x01)

/**** Defines for the Phy field ****/
#define MPI3_IOUNIT5_PHY_SPINUP_GROUP_MASK                 (0x03)
#define MPI3_IOUNIT5_PHY_SPINUP_GROUP_SHIFT                (0)

/*****************************************************************************
 *              IO Unit Page 6                                               *
 ****************************************************************************/
typedef struct _MPI3_IO_UNIT_PAGE6
{
    MPI3_CONFIG_PAGE_HEADER         Header;                     /* 0x00 */
    U32                             BoardPowerRequirement;      /* 0x08 */
    U32                             PCISlotPowerAllocation;     /* 0x0C */
    U8                              Flags;                      /* 0x10 */
    U8                              Reserved11[3];              /* 0x11 */
} MPI3_IO_UNIT_PAGE6, MPI3_POINTER PTR_MPI3_IO_UNIT_PAGE6,
  Mpi3IOUnitPage6_t, MPI3_POINTER pMpi3IOUnitPage6_t;

/**** Defines for the PageVersion field ****/
#define MPI3_IOUNIT6_PAGEVERSION                (0x00)

/**** Defines for the Flags field ****/
#define MPI3_IOUNIT6_FLAGS_ACT_CABLE_PWR_EXC    (0x01)

/*****************************************************************************
 *              IO Unit Page 8                                               *
 ****************************************************************************/

#ifndef MPI3_IOUNIT8_DIGEST_MAX
#define MPI3_IOUNIT8_DIGEST_MAX                   (1)
#endif  /* MPI3_IOUNIT8_DIGEST_MAX */

typedef union _MPI3_IOUNIT8_RAW_DIGEST
{
    U32                             Dword[16];
    U16                             Word[32];
    U8                              Byte[64];
} MPI3_IOUNIT8_RAW_DIGEST, MPI3_POINTER PTR_MPI3_IOUNIT8_RAW_DIGEST,
  Mpi3IOUnit8RawDigest_t, MPI3_POINTER pMpi3IOUnit8RawDigest_t;

typedef struct _MPI3_IOUNIT8_METADATA_DIGEST
{
    U8                              SlotStatus;                        /* 0x00 */
    U8                              Reserved01[3];                     /* 0x01 */
    U32                             Reserved04[3];                     /* 0x04 */
    MPI3_IOUNIT8_RAW_DIGEST         DigestData;                        /* 0x10 */
} MPI3_IOUNIT8_METADATA_DIGEST, MPI3_POINTER PTR_MPI3_IOUNIT8_METADATA_DIGEST,
  Mpi3IOUnit8MetadataDigest_t, MPI3_POINTER pMpi3IOUnit8MetadataDigest_t;

/**** Defines for the SlotStatus field ****/
#define MPI3_IOUNIT8_METADATA_DIGEST_SLOTSTATUS_UNUSED                 (0x00)
#define MPI3_IOUNIT8_METADATA_DIGEST_SLOTSTATUS_UPDATE_PENDING         (0x01)
#define MPI3_IOUNIT8_METADATA_DIGEST_SLOTSTATUS_VALID                  (0x03)
#define MPI3_IOUNIT8_METADATA_DIGEST_SLOTSTATUS_INVALID                (0x07)

typedef union _MPI3_IOUNIT8_DIGEST
{
    MPI3_IOUNIT8_RAW_DIGEST         RawDigest[MPI3_IOUNIT8_DIGEST_MAX];
    MPI3_IOUNIT8_METADATA_DIGEST    MetadataDigest[MPI3_IOUNIT8_DIGEST_MAX];
} MPI3_IOUNIT8_DIGEST, MPI3_POINTER PTR_MPI3_IOUNIT8_DIGEST,
  Mpi3IOUnit8Digest_t, MPI3_POINTER pMpi3IOUnit8Digest_t;

typedef struct _MPI3_IO_UNIT_PAGE8
{
    MPI3_CONFIG_PAGE_HEADER         Header;                             /* 0x00 */
    U8                              SBMode;                             /* 0x08 */
    U8                              SBState;                            /* 0x09 */
    U8                              Flags;                              /* 0x0A */
    U8                              Reserved0A;                         /* 0x0B */
    U8                              NumSlots;                           /* 0x0C */
    U8                              SlotsAvailable;                     /* 0x0D */
    U8                              CurrentKeyEncryptionAlgo;           /* 0x0E */
    U8                              KeyDigestHashAlgo;                  /* 0x0F */
    MPI3_VERSION_UNION              CurrentSvn;                         /* 0x10 */
    U32                             Reserved14;                         /* 0x14 */
    U32                             CurrentKey[128];                    /* 0x18 */
    MPI3_IOUNIT8_DIGEST             Digest;                             /* 0x218 */  /* variable length */
} MPI3_IO_UNIT_PAGE8, MPI3_POINTER PTR_MPI3_IO_UNIT_PAGE8,
  Mpi3IOUnitPage8_t, MPI3_POINTER pMpi3IOUnitPage8_t;

/**** Defines for the PageVersion field ****/
#define MPI3_IOUNIT8_PAGEVERSION                                  (0x00)

/**** Defines for the SBMode field ****/
#define MPI3_IOUNIT8_SBMODE_HARD_SECURE_RECERTIFIED               (0x08)
#define MPI3_IOUNIT8_SBMODE_SECURE_DEBUG                          (0x04)
#define MPI3_IOUNIT8_SBMODE_HARD_SECURE                           (0x02)
#define MPI3_IOUNIT8_SBMODE_CONFIG_SECURE                         (0x01)

/**** Defines for the SBState field ****/
#define MPI3_IOUNIT8_SBSTATE_SVN_UPDATE_PENDING                   (0x04)
#define MPI3_IOUNIT8_SBSTATE_KEY_UPDATE_PENDING                   (0x02)
#define MPI3_IOUNIT8_SBSTATE_SECURE_BOOT_ENABLED                  (0x01)

/**** Defines for the Flags field ****/
#define MPI3_IOUNIT8_FLAGS_CURRENT_KEY_IOUNIT17                   (0x08)
#define MPI3_IOUNIT8_FLAGS_DIGESTFORM_MASK                        (0x07)
#define MPI3_IOUNIT8_FLAGS_DIGESTFORM_SHIFT                       (0)
#define MPI3_IOUNIT8_FLAGS_DIGESTFORM_RAW                         (0x00)
#define MPI3_IOUNIT8_FLAGS_DIGESTFORM_DIGEST_WITH_METADATA        (0x01)

/**** Use MPI3_ENCRYPTION_ALGORITHM_ defines (see mpi30_image.h) for the CurrentKeyEncryptionAlgo field ****/
/**** Use MPI3_HASH_ALGORITHM defines (see mpi30_image.h) for the KeyDigestHashAlgo field ****/

/*****************************************************************************
 *              IO Unit Page 9                                               *
 ****************************************************************************/

typedef struct _MPI3_IO_UNIT_PAGE9
{
    MPI3_CONFIG_PAGE_HEADER         Header;                 /* 0x00 */
    U32                             Flags;                  /* 0x08 */
    U16                             FirstDevice;            /* 0x0C */
    U16                             Reserved0E;             /* 0x0E */
} MPI3_IO_UNIT_PAGE9, MPI3_POINTER PTR_MPI3_IO_UNIT_PAGE9,
  Mpi3IOUnitPage9_t, MPI3_POINTER pMpi3IOUnitPage9_t;

/**** Defines for the PageVersion field ****/
#define MPI3_IOUNIT9_PAGEVERSION                                  (0x00)

/**** Defines for the Flags field ****/
#define MPI3_IOUNIT9_FLAGS_UBM_ENCLOSURE_ORDER_MASK               (0x00000006)
#define MPI3_IOUNIT9_FLAGS_UBM_ENCLOSURE_ORDER_SHIFT              (1)
#define MPI3_IOUNIT9_FLAGS_UBM_ENCLOSURE_ORDER_NONE               (0x00000000)
#define MPI3_IOUNIT9_FLAGS_UBM_ENCLOSURE_ORDER_RECEPTACLE         (0x00000002)
#define MPI3_IOUNIT9_FLAGS_UBM_ENCLOSURE_ORDER_BACKPLANE_TYPE     (0x00000004)
#define MPI3_IOUNIT9_FLAGS_VDFIRST_ENABLED                        (0x00000001)

/**** Defines for the FirstDevice field ****/
#define MPI3_IOUNIT9_FIRSTDEVICE_UNKNOWN                          (0xFFFF)
#define MPI3_IOUNIT9_FIRSTDEVICE_IN_DRIVER_PAGE_0                 (0xFFFE)

/*****************************************************************************
 *              IO Unit Page 10                                              *
 ****************************************************************************/

typedef struct _MPI3_IO_UNIT_PAGE10
{
    MPI3_CONFIG_PAGE_HEADER         Header;                 /* 0x00 */
    U8                              Flags;                  /* 0x08 */
    U8                              Reserved09[3];          /* 0x09 */
    U32                             SiliconID;              /* 0x0C */
    U8                              FWVersionMinor;         /* 0x10 */
    U8                              FWVersionMajor;         /* 0x11 */
    U8                              HWVersionMinor;         /* 0x12 */
    U8                              HWVersionMajor;         /* 0x13 */
    U8                              PartNumber[16];         /* 0x14 */
} MPI3_IO_UNIT_PAGE10, MPI3_POINTER PTR_MPI3_IO_UNIT_PAGE10,
  Mpi3IOUnitPage10_t, MPI3_POINTER pMpi3IOUnitPage10_t;

/**** Defines for the PageVersion field ****/
#define MPI3_IOUNIT10_PAGEVERSION                  (0x00)

/**** Defines for the Flags field ****/
#define MPI3_IOUNIT10_FLAGS_VALID                  (0x01)
#define MPI3_IOUNIT10_FLAGS_ACTIVEID_MASK          (0x02)
#define MPI3_IOUNIT10_FLAGS_ACTIVEID_SHIFT         (1)
#define MPI3_IOUNIT10_FLAGS_ACTIVEID_FIRST_REGION  (0x00)
#define MPI3_IOUNIT10_FLAGS_ACTIVEID_SECOND_REGION (0x02)
#define MPI3_IOUNIT10_FLAGS_PBLP_EXPECTED          (0x80)

/*****************************************************************************
 *              IO Unit Page 11                                              *
 ****************************************************************************/

#ifndef MPI3_IOUNIT11_PROFILE_MAX
#define MPI3_IOUNIT11_PROFILE_MAX                   (1)
#endif  /* MPI3_IOUNIT11_PROFILE_MAX */

typedef struct _MPI3_IOUNIT11_PROFILE
{
    U8                              ProfileIdentifier;                    /* 0x00 */
    U8                              Reserved01[3];                        /* 0x01 */
    U16                             MaxVDs;                               /* 0x04 */
    U16                             MaxHostPDs;                           /* 0x06 */
    U16                             MaxAdvHostPDs;                        /* 0x08 */
    U16                             MaxRAIDPDs;                           /* 0x0A */
    U16                             MaxNVMe;                              /* 0x0C */
    U16                             MaxOutstandingRequests;               /* 0x0E */
    U16                             SubsystemID;                          /* 0x10 */
    U16                             Reserved12;                           /* 0x12 */
    U32                             Reserved14[2];                        /* 0x14 */
} MPI3_IOUNIT11_PROFILE, MPI3_POINTER PTR_MPI3_IOUNIT11_PROFILE,
  Mpi3IOUnit11Profile_t, MPI3_POINTER pMpi3IOUnit11Profile_t;

typedef struct _MPI3_IO_UNIT_PAGE11
{
    MPI3_CONFIG_PAGE_HEADER         Header;                               /* 0x00 */
    U32                             Reserved08;                           /* 0x08 */
    U8                              NumProfiles;                          /* 0x0C */
    U8                              CurrentProfileIdentifier;             /* 0x0D */
    U16                             Reserved0E;                           /* 0x0E */
    MPI3_IOUNIT11_PROFILE           Profile[MPI3_IOUNIT11_PROFILE_MAX];   /* 0x10 */ /* variable length */
} MPI3_IO_UNIT_PAGE11, MPI3_POINTER PTR_MPI3_IO_UNIT_PAGE11,
  Mpi3IOUnitPage11_t, MPI3_POINTER pMpi3IOUnitPage11_t;

/**** Defines for the PageVersion field ****/
#define MPI3_IOUNIT11_PAGEVERSION                  (0x00)

/*****************************************************************************
 *              IO Unit Page 12                                              *
 ****************************************************************************/

#ifndef MPI3_IOUNIT12_BUCKET_MAX
#define MPI3_IOUNIT12_BUCKET_MAX                   (1)
#endif  /* MPI3_IOUNIT12_BUCKET_MAX */

typedef struct _MPI3_IOUNIT12_BUCKET
{
    U8                              CoalescingDepth;                      /* 0x00 */
    U8                              CoalescingTimeout;                    /* 0x01 */
    U16                             IOCountLowBoundary;                   /* 0x02 */
    U32                             Reserved04;                           /* 0x04 */
} MPI3_IOUNIT12_BUCKET, MPI3_POINTER PTR_MPI3_IOUNIT12_BUCKET,
  Mpi3IOUnit12Bucket_t, MPI3_POINTER pMpi3IOUnit12Bucket_t;

typedef struct _MPI3_IO_UNIT_PAGE12
{
    MPI3_CONFIG_PAGE_HEADER         Header;                               /* 0x00 */
    U32                             Flags;                                /* 0x08 */
    U32                             Reserved0C[4];                        /* 0x0C */
    U8                              NumBuckets;                           /* 0x1C */
    U8                              Reserved1D[3];                        /* 0x1D */
    MPI3_IOUNIT12_BUCKET            Bucket[MPI3_IOUNIT12_BUCKET_MAX];     /* 0x20 */ /* variable length */
} MPI3_IO_UNIT_PAGE12, MPI3_POINTER PTR_MPI3_IO_UNIT_PAGE12,
  Mpi3IOUnitPage12_t, MPI3_POINTER pMpi3IOUnitPage12_t;

/**** Defines for the PageVersion field ****/
#define MPI3_IOUNIT12_PAGEVERSION                  (0x00)

/**** Defines for the Flags field ****/
#define MPI3_IOUNIT12_FLAGS_NUMPASSES_MASK         (0x00000300)
#define MPI3_IOUNIT12_FLAGS_NUMPASSES_SHIFT        (8)
#define MPI3_IOUNIT12_FLAGS_NUMPASSES_8            (0x00000000)
#define MPI3_IOUNIT12_FLAGS_NUMPASSES_16           (0x00000100)
#define MPI3_IOUNIT12_FLAGS_NUMPASSES_32           (0x00000200)
#define MPI3_IOUNIT12_FLAGS_NUMPASSES_64           (0x00000300)
#define MPI3_IOUNIT12_FLAGS_PASSPERIOD_MASK        (0x00000003)
#define MPI3_IOUNIT12_FLAGS_PASSPERIOD_SHIFT       (0)
#define MPI3_IOUNIT12_FLAGS_PASSPERIOD_DISABLED    (0x00000000)
#define MPI3_IOUNIT12_FLAGS_PASSPERIOD_500US       (0x00000001)
#define MPI3_IOUNIT12_FLAGS_PASSPERIOD_1MS         (0x00000002)
#define MPI3_IOUNIT12_FLAGS_PASSPERIOD_2MS         (0x00000003)

/*****************************************************************************
 *              IO Unit Page 13                                              *
 ****************************************************************************/

#ifndef MPI3_IOUNIT13_FUNC_MAX
#define MPI3_IOUNIT13_FUNC_MAX                                     (1)
#endif  /* MPI3_IOUNIT13_FUNC_MAX */

typedef struct _MPI3_IOUNIT13_ALLOWED_FUNCTION
{
    U16                             SubFunction;                              /* 0x00 */
    U8                              FunctionCode;                             /* 0x02 */
    U8                              FunctionFlags;                            /* 0x03 */
} MPI3_IOUNIT13_ALLOWED_FUNCTION, MPI3_POINTER PTR_MPI3_IOUNIT13_ALLOWED_FUNCTION,
  Mpi3IOUnit13AllowedFunction_t, MPI3_POINTER pMpi3IOUnit13AllowedFunction_t;

/**** Defines for the FunctionFlags field ****/
#define MPI3_IOUNIT13_FUNCTION_FLAGS_ADMIN_BLOCKED                 (0x04)
#define MPI3_IOUNIT13_FUNCTION_FLAGS_OOB_BLOCKED                   (0x02)
#define MPI3_IOUNIT13_FUNCTION_FLAGS_CHECK_SUBFUNCTION_ENABLED     (0x01)

typedef struct _MPI3_IO_UNIT_PAGE13
{
    MPI3_CONFIG_PAGE_HEADER         Header;                                   /* 0x00 */
    U16                             Flags;                                    /* 0x08 */
    U16                             Reserved0A;                               /* 0x0A */
    U8                              NumAllowedFunctions;                      /* 0x0C */
    U8                              Reserved0D[3];                            /* 0x0D */
    MPI3_IOUNIT13_ALLOWED_FUNCTION  AllowedFunction[MPI3_IOUNIT13_FUNC_MAX];  /* 0x10 */ /* variable length */
} MPI3_IO_UNIT_PAGE13, MPI3_POINTER PTR_MPI3_IO_UNIT_PAGE13,
  Mpi3IOUnitPage13_t, MPI3_POINTER pMpi3IOUnitPage13_t;

/**** Defines for the PageVersion field ****/
#define MPI3_IOUNIT13_PAGEVERSION                                  (0x00)

/**** Defines for the Flags field ****/
#define MPI3_IOUNIT13_FLAGS_ADMIN_BLOCKED                          (0x0002)
#define MPI3_IOUNIT13_FLAGS_OOB_BLOCKED                            (0x0001)

/*****************************************************************************
 *              IO Unit Page 14                                              *
 ****************************************************************************/

#ifndef MPI3_IOUNIT14_MD_MAX
#define MPI3_IOUNIT14_MD_MAX                                       (1)
#endif  /* MPI3_IOUNIT14_MD_MAX */

typedef struct _MPI3_IOUNIT14_PAGEMETADATA
{
    U8                              PageType;                                 /* 0x00 */
    U8                              PageNumber;                               /* 0x01 */
    U8                              Reserved02;                               /* 0x02 */
    U8                              PageFlags;                                /* 0x03 */
} MPI3_IOUNIT14_PAGEMETADATA, MPI3_POINTER PTR_MPI3_IOUNIT14_PAGEMETADATA,
  Mpi3IOUnit14PageMetadata_t, MPI3_POINTER pMpi3IOUnit14PageMetadata_t;

/**** Defines for the PageFlags field ****/
#define MPI3_IOUNIT14_PAGEMETADATA_PAGEFLAGS_OOBWRITE_ALLOWED      (0x02)
#define MPI3_IOUNIT14_PAGEMETADATA_PAGEFLAGS_HOSTWRITE_ALLOWED     (0x01)

typedef struct _MPI3_IO_UNIT_PAGE14
{
    MPI3_CONFIG_PAGE_HEADER         Header;                                   /* 0x00 */
    U8                              Flags;                                    /* 0x08 */
    U8                              Reserved09[3];                            /* 0x09 */
    U8                              NumPages;                                 /* 0x0C */
    U8                              Reserved0D[3];                            /* 0x0D */
    MPI3_IOUNIT14_PAGEMETADATA      PageMetadata[MPI3_IOUNIT14_MD_MAX];       /* 0x10 */ /* variable length */
} MPI3_IO_UNIT_PAGE14, MPI3_POINTER PTR_MPI3_IO_UNIT_PAGE14,
  Mpi3IOUnitPage14_t, MPI3_POINTER pMpi3IOUnitPage14_t;

/**** Defines for the PageVersion field ****/
#define MPI3_IOUNIT14_PAGEVERSION                                  (0x00)

/**** Defines for the Flags field ****/
#define MPI3_IOUNIT14_FLAGS_READONLY                               (0x01)

/*****************************************************************************
 *              IO Unit Page 15                                              *
 ****************************************************************************/

#ifndef MPI3_IOUNIT15_PBD_MAX
#define MPI3_IOUNIT15_PBD_MAX                                       (1)
#endif  /* MPI3_IOUNIT15_PBD_MAX */

typedef struct _MPI3_IO_UNIT_PAGE15
{
    MPI3_CONFIG_PAGE_HEADER         Header;                                   /* 0x00 */
    U8                              Flags;                                    /* 0x08 */
    U8                              Reserved09[3];                            /* 0x09 */
    U32                             Reserved0C;                               /* 0x0C */
    U8                              PowerBudgetingCapability;                 /* 0x10 */
    U8                              Reserved11[3];                            /* 0x11 */
    U8                              NumPowerBudgetData;                       /* 0x14 */
    U8                              Reserved15[3];                            /* 0x15 */
    U32                             PowerBudgetData[MPI3_IOUNIT15_PBD_MAX];   /* 0x18 */ /* variable length */
} MPI3_IO_UNIT_PAGE15, MPI3_POINTER PTR_MPI3_IO_UNIT_PAGE15,
  Mpi3IOUnitPage15_t, MPI3_POINTER pMpi3IOUnitPage15_t;

/**** Defines for the PageVersion field ****/
#define MPI3_IOUNIT15_PAGEVERSION                                   (0x00)

/**** Defines for the Flags field ****/
#define MPI3_IOUNIT15_FLAGS_EPRINIT_INITREQUIRED                    (0x04)
#define MPI3_IOUNIT15_FLAGS_EPRSUPPORT_MASK                         (0x03)
#define MPI3_IOUNIT15_FLAGS_EPRSUPPORT_SHIFT                        (0)
#define MPI3_IOUNIT15_FLAGS_EPRSUPPORT_NOT_SUPPORTED                (0x00)
#define MPI3_IOUNIT15_FLAGS_EPRSUPPORT_WITHOUT_POWER_BRAKE_GPIO     (0x01)
#define MPI3_IOUNIT15_FLAGS_EPRSUPPORT_WITH_POWER_BRAKE_GPIO        (0x02)

/**** Defines for the NumPowerBudgetData field ****/
#define MPI3_IOUNIT15_NUMPOWERBUDGETDATA_POWER_BUDGETING_DISABLED   (0x00)

/*****************************************************************************
 *              IO Unit Page 16                                              *
 ****************************************************************************/

#ifndef MPI3_IOUNIT16_ERROR_MAX
#define MPI3_IOUNIT16_ERROR_MAX                                      (1)
#endif /* MPI3_IOUNIT16_ERROR_MAX */

typedef struct _MPI3_IOUNIT16_ERROR
{
    U32                             Offset;                                   /* 0x00 */
    U32                             Reserved04;                               /* 0x04 */
    U64                             Count;                                    /* 0x08 */
    U64                             Timestamp;                                /* 0x10 */
} MPI3_IOUNIT16_ERROR, MPI3_POINTER PTR_MPI3_IOUNIT16_ERROR,
  Mpi3IOUnit16Error_t, MPI3_POINTER pMpi3IOUnit16Error_t;

typedef struct _MPI3_IO_UNIT_PAGE16
{
    MPI3_CONFIG_PAGE_HEADER         Header;                                   /* 0x00 */
    U64                             TotalErrorCount;                          /* 0x08 */
    U32                             Reserved10[3];                            /* 0x10 */
    U8                              NumErrors;                                /* 0x1C */
    U8                              MaxErrorsTracked;                         /* 0x1D */
    U16                             Reserved1E;                               /* 0x1E */
    MPI3_IOUNIT16_ERROR             Error[MPI3_IOUNIT16_ERROR_MAX];           /* 0x20 */ /* variable length */
} MPI3_IO_UNIT_PAGE16, MPI3_POINTER PTR_MPI3_IO_UNIT_PAGE16,
  Mpi3IOUnitPage16_t, MPI3_POINTER pMpi3IOUnitPage16_t;

/**** Defines for the PageVersion field ****/
#define MPI3_IOUNIT16_PAGEVERSION                                   (0x00)

/*****************************************************************************
 *              IO Unit Page 17                                              *
 ****************************************************************************/

#ifndef MPI3_IOUNIT17_CURRENTKEY_MAX
#define MPI3_IOUNIT17_CURRENTKEY_MAX                                (1)
#endif /* MPI3_IOUNIT17_CURRENTKEY_MAX */

typedef struct _MPI3_IO_UNIT_PAGE17
{
    MPI3_CONFIG_PAGE_HEADER         Header;                                   /* 0x00 */
    U8                              NumInstances;                             /* 0x08 */
    U8                              Instance;                                 /* 0x09 */
    U16                             Reserved0A;                               /* 0x0A */
    U32                             Reserved0C[4];                            /* 0x0C */
    U16                             KeyLength;                                /* 0x1C */
    U8                              EncryptionAlgorithm;                      /* 0x1E */
    U8                              Reserved1F;                               /* 0x1F */
    U32                             CurrentKey[MPI3_IOUNIT17_CURRENTKEY_MAX]; /* 0x20 */ /* variable length */
} MPI3_IO_UNIT_PAGE17, MPI3_POINTER PTR_MPI3_IO_UNIT_PAGE17,
  Mpi3IOUnitPage17_t, MPI3_POINTER pMpi3IOUnitPage17_t;

/**** Defines for the PageVersion field ****/
#define MPI3_IOUNIT17_PAGEVERSION                                   (0x00)

/**** Use MPI3_ENCRYPTION_ALGORITHM_ defines (see mpi30_image.h) for the EncryptionAlgorithm field ****/

/*****************************************************************************
 *              IO Unit Page 18                                              *
 ****************************************************************************/

typedef struct _MPI3_IO_UNIT_PAGE18
{
    MPI3_CONFIG_PAGE_HEADER         Header;                                   /* 0x00 */
    U8                              Flags;                                    /* 0x08 */
    U8                              PollInterval;                             /* 0x09 */
    U16                             Reserved0A;                               /* 0x0A */
    U32                             Reserved0C;                               /* 0x0C */
} MPI3_IO_UNIT_PAGE18, MPI3_POINTER PTR_MPI3_IO_UNIT_PAGE18,
  Mpi3IOUnitPage18_t, MPI3_POINTER pMpi3IOUnitPage18_t;

/**** Defines for the PageVersion field ****/
#define MPI3_IOUNIT18_PAGEVERSION                                   (0x00)

/**** Defines for the Flags field ****/
#define MPI3_IOUNIT18_FLAGS_DIRECTATTACHED_ENABLE                   (0x01)

/**** Defines for the PollInterval field ****/
#define MPI3_IOUNIT18_POLLINTERVAL_DISABLE                          (0x00)

/*****************************************************************************
 *              IO Unit Page 19                                              *
 ****************************************************************************/

#ifndef MPI3_IOUNIT19_DEVICE_MAX
#define MPI3_IOUNIT19_DEVICE_MAX                                    (1)
#endif /* MPI3_IOUNIT19_DEVICE_MAX */

typedef struct _MPI3_IOUNIT19_DEVICE_
{
    U16                             Temperature;                              /* 0x00 */
    U16                             DevHandle;                                /* 0x02 */
    U16                             PersistentID;                             /* 0x04 */
    U16                             Reserved06;                               /* 0x06 */
} MPI3_IOUNIT19_DEVICE, MPI3_POINTER PTR_MPI3_IOUNIT19_DEVICE,
  Mpi3IOUnit19Device_t, MPI3_POINTER pMpi3IOUnit19Device_t;

/**** Defines for the Temperature field ****/
#define MPI3_IOUNIT19_DEVICE_TEMPERATURE_UNAVAILABLE                (0x8000)

typedef struct _MPI3_IO_UNIT_PAGE19
{
    MPI3_CONFIG_PAGE_HEADER         Header;                                   /* 0x00 */
    U16                             NumDevices;                               /* 0x08 */
    U16                             Reserved0A;                               /* 0x0A */
    U32                             Reserved0C;                               /* 0x0C */
    MPI3_IOUNIT19_DEVICE            Device[MPI3_IOUNIT19_DEVICE_MAX];         /* 0x10 */
} MPI3_IO_UNIT_PAGE19, MPI3_POINTER PTR_MPI3_IO_UNIT_PAGE19,
  Mpi3IOUnitPage19_t, MPI3_POINTER pMpi3IOUnitPage19_t;

/**** Defines for the PageVersion field ****/
#define MPI3_IOUNIT19_PAGEVERSION                                   (0x00)


/*****************************************************************************
 *              IOC Configuration Pages                                      *
 ****************************************************************************/

/*****************************************************************************
 *              IOC Page 0                                                   *
 ****************************************************************************/
typedef struct _MPI3_IOC_PAGE0
{
    MPI3_CONFIG_PAGE_HEADER         Header;                 /* 0x00 */
    U32                             Reserved08;             /* 0x08 */
    U16                             VendorID;               /* 0x0C */
    U16                             DeviceID;               /* 0x0E */
    U8                              RevisionID;             /* 0x10 */
    U8                              Reserved11[3];          /* 0x11 */
    U32                             ClassCode;              /* 0x14 */
    U16                             SubsystemVendorID;      /* 0x18 */
    U16                             SubsystemID;            /* 0x1A */
} MPI3_IOC_PAGE0, MPI3_POINTER PTR_MPI3_IOC_PAGE0,
  Mpi3IOCPage0_t, MPI3_POINTER pMpi3IOCPage0_t;

/**** Defines for the PageVersion field ****/
#define MPI3_IOC0_PAGEVERSION               (0x00)

/*****************************************************************************
 *              IOC Page 1                                                   *
 ****************************************************************************/
typedef struct _MPI3_IOC_PAGE1
{
    MPI3_CONFIG_PAGE_HEADER         Header;                 /* 0x00 */
    U32                             CoalescingTimeout;      /* 0x08 */
    U8                              CoalescingDepth;        /* 0x0C */
    U8                              Obsolete;               /* 0x0D */
    U16                             Reserved0E;             /* 0x0E */
} MPI3_IOC_PAGE1, MPI3_POINTER PTR_MPI3_IOC_PAGE1,
  Mpi3IOCPage1_t, MPI3_POINTER pMpi3IOCPage1_t;

/**** Defines for the PageVersion field ****/
#define MPI3_IOC1_PAGEVERSION               (0x00)

/*****************************************************************************
 *              IOC Page 2                                                   *
 ****************************************************************************/
#ifndef MPI3_IOC2_EVENTMASK_WORDS
#define MPI3_IOC2_EVENTMASK_WORDS           (4)
#endif  /* MPI3_IOC2_EVENTMASK_WORDS */

typedef struct _MPI3_IOC_PAGE2
{
    MPI3_CONFIG_PAGE_HEADER         Header;                                 /* 0x00 */
    U32                             Reserved08;                             /* 0x08 */
    U16                             SASBroadcastPrimitiveMasks;             /* 0x0C */
    U16                             SASNotifyPrimitiveMasks;                /* 0x0E */
    U32                             EventMasks[MPI3_IOC2_EVENTMASK_WORDS];  /* 0x10 */
} MPI3_IOC_PAGE2, MPI3_POINTER PTR_MPI3_IOC_PAGE2,
  Mpi3IOCPage2_t, MPI3_POINTER pMpi3IOCPage2_t;

/**** Defines for the PageVersion field ****/
#define MPI3_IOC2_PAGEVERSION               (0x00)


/*****************************************************************************
 *              Driver Configuration Pages                                  *
 ****************************************************************************/

/**** Defines for the Flags field  in Driver Pages 10, 20, and 30 ****/
/****    NOT used in Driver Page 1 Flags field                    ****/
#define MPI3_DRIVER_FLAGS_ADMINRAIDPD_BLOCKED               (0x0010)
#define MPI3_DRIVER_FLAGS_OOBRAIDPD_BLOCKED                 (0x0008)
#define MPI3_DRIVER_FLAGS_OOBRAIDVD_BLOCKED                 (0x0004)
#define MPI3_DRIVER_FLAGS_OOBADVHOSTPD_BLOCKED              (0x0002)
#define MPI3_DRIVER_FLAGS_OOBHOSTPD_BLOCKED                 (0x0001)

typedef struct _MPI3_ALLOWED_CMD_SCSI
{
    U16                             ServiceAction;       /* 0x00 */
    U8                              OperationCode;       /* 0x02 */
    U8                              CommandFlags;        /* 0x03 */
} MPI3_ALLOWED_CMD_SCSI, MPI3_POINTER PTR_MPI3_ALLOWED_CMD_SCSI,
  Mpi3AllowedCmdScsi_t, MPI3_POINTER pMpi3AllowedCmdScsi_t;

typedef struct _MPI3_ALLOWED_CMD_ATA
{
    U8                              Subcommand;          /* 0x00 */
    U8                              Reserved01;          /* 0x01 */
    U8                              Command;             /* 0x02 */
    U8                              CommandFlags;        /* 0x03 */
} MPI3_ALLOWED_CMD_ATA, MPI3_POINTER PTR_MPI3_ALLOWED_CMD_ATA,
  Mpi3AllowedCmdAta_t, MPI3_POINTER pMpi3AllowedCmdAta_t;

typedef struct _MPI3_ALLOWED_CMD_NVME
{
    U8                              Reserved00;          /* 0x00 */
    U8                              NVMeCmdFlags;        /* 0x01 */
    U8                              OpCode;              /* 0x02 */
    U8                              CommandFlags;        /* 0x03 */
} MPI3_ALLOWED_CMD_NVME, MPI3_POINTER PTR_MPI3_ALLOWED_CMD_NVME,
  Mpi3AllowedCmdNvme_t, MPI3_POINTER pMpi3AllowedCmdNvme_t;

/**** Defines for the NVMeCmdFlags field ****/
#define MPI3_DRIVER_ALLOWEDCMD_NVMECMDFLAGS_SUBQ_TYPE_MASK     (0x80)
#define MPI3_DRIVER_ALLOWEDCMD_NVMECMDFLAGS_SUBQ_TYPE_SHIFT    (7)
#define MPI3_DRIVER_ALLOWEDCMD_NVMECMDFLAGS_SUBQ_TYPE_IO       (0x00)
#define MPI3_DRIVER_ALLOWEDCMD_NVMECMDFLAGS_SUBQ_TYPE_ADMIN    (0x80)
#define MPI3_DRIVER_ALLOWEDCMD_NVMECMDFLAGS_CMDSET_MASK        (0x3F)
#define MPI3_DRIVER_ALLOWEDCMD_NVMECMDFLAGS_CMDSET_SHIFT       (0)
#define MPI3_DRIVER_ALLOWEDCMD_NVMECMDFLAGS_CMDSET_NVM         (0x00)

typedef union _MPI3_ALLOWED_CMD
{
    MPI3_ALLOWED_CMD_SCSI           Scsi;
    MPI3_ALLOWED_CMD_ATA            Ata;
    MPI3_ALLOWED_CMD_NVME           NVMe;
} MPI3_ALLOWED_CMD, MPI3_POINTER PTR_MPI3_ALLOWED_CMD,
  Mpi3AllowedCmd_t, MPI3_POINTER pMpi3AllowedCmd_t;

/**** Defines for the CommandFlags field ****/
#define MPI3_DRIVER_ALLOWEDCMD_CMDFLAGS_ADMINRAIDPD_BLOCKED    (0x20)
#define MPI3_DRIVER_ALLOWEDCMD_CMDFLAGS_OOBRAIDPD_BLOCKED      (0x10)
#define MPI3_DRIVER_ALLOWEDCMD_CMDFLAGS_OOBRAIDVD_BLOCKED      (0x08)
#define MPI3_DRIVER_ALLOWEDCMD_CMDFLAGS_OOBADVHOSTPD_BLOCKED   (0x04)
#define MPI3_DRIVER_ALLOWEDCMD_CMDFLAGS_OOBHOSTPD_BLOCKED      (0x02)
#define MPI3_DRIVER_ALLOWEDCMD_CMDFLAGS_CHECKSUBCMD_ENABLED    (0x01)


#ifndef MPI3_ALLOWED_CMDS_MAX
#define MPI3_ALLOWED_CMDS_MAX           (1)
#endif  /* MPI3_ALLOWED_CMDS_MAX */

/*****************************************************************************
 *              Driver Page 0                                               *
 ****************************************************************************/
typedef struct _MPI3_DRIVER_PAGE0
{
    MPI3_CONFIG_PAGE_HEADER         Header;             /* 0x00 */
    U32                             BSDOptions;         /* 0x08 */
    U8                              SSUTimeout;         /* 0x0C */
    U8                              IOTimeout;          /* 0x0D */
    U8                              TURRetries;         /* 0x0E */
    U8                              TURInterval;        /* 0x0F */
    U8                              Reserved10;         /* 0x10 */
    U8                              SecurityKeyTimeout; /* 0x11 */
    U16                             FirstDevice;        /* 0x12 */
    U32                             Reserved14;         /* 0x14 */
    U32                             Reserved18;         /* 0x18 */
} MPI3_DRIVER_PAGE0, MPI3_POINTER PTR_MPI3_DRIVER_PAGE0,
  Mpi3DriverPage0_t, MPI3_POINTER pMpi3DriverPage0_t;

/**** Defines for the PageVersion field ****/
#define MPI3_DRIVER0_PAGEVERSION                                    (0x00)

/**** Defines for the BSDOptions field ****/
#define MPI3_DRIVER0_BSDOPTS_DEVICEEXPOSURE_DISABLE                 (0x00000020)
#define MPI3_DRIVER0_BSDOPTS_WRITECACHE_DISABLE                     (0x00000010)
#define MPI3_DRIVER0_BSDOPTS_HEADLESS_MODE_ENABLE                   (0x00000008)
#define MPI3_DRIVER0_BSDOPTS_DIS_HII_CONFIG_UTIL                    (0x00000004)
#define MPI3_DRIVER0_BSDOPTS_REGISTRATION_MASK                      (0x00000003)
#define MPI3_DRIVER0_BSDOPTS_REGISTRATION_SHIFT                     (0)
#define MPI3_DRIVER0_BSDOPTS_REGISTRATION_IOC_AND_DEVS              (0x00000000)
#define MPI3_DRIVER0_BSDOPTS_REGISTRATION_IOC_ONLY                  (0x00000001)
#define MPI3_DRIVER0_BSDOPTS_REGISTRATION_IOC_AND_INTERNAL_DEVS     (0x00000002)

/**** Defines for the FirstDevice field ****/
#define MPI3_DRIVER0_FIRSTDEVICE_IGNORE1                            (0x0000)
#define MPI3_DRIVER0_FIRSTDEVICE_IGNORE2                            (0xFFFF)

/*****************************************************************************
 *              Driver Page 1                                               *
 ****************************************************************************/
typedef struct _MPI3_DRIVER_PAGE1
{
    MPI3_CONFIG_PAGE_HEADER         Header;                                   /* 0x00 */
    U32                             Flags;                                    /* 0x08 */
    U8                              TimeStampUpdate;                          /* 0x0C */
    U8                              Reserved0D[3];                            /* 0x0D */
    U16                             HostDiagTraceMaxSize;                     /* 0x10 */
    U16                             HostDiagTraceMinSize;                     /* 0x12 */
    U16                             HostDiagTraceDecrementSize;               /* 0x14 */
    U16                             Reserved16;                               /* 0x16 */
    U16                             HostDiagFwMaxSize;                        /* 0x18 */
    U16                             HostDiagFwMinSize;                        /* 0x1A */
    U16                             HostDiagFwDecrementSize;                  /* 0x1C */
    U16                             Reserved1E;                               /* 0x1E */
    U16                             HostDiagDriverMaxSize;                    /* 0x20 */
    U16                             HostDiagDriverMinSize;                    /* 0x22 */
    U16                             HostDiagDriverDecrementSize;              /* 0x24 */
    U16                             Reserved26;                               /* 0x26 */
} MPI3_DRIVER_PAGE1, MPI3_POINTER PTR_MPI3_DRIVER_PAGE1,
  Mpi3DriverPage1_t, MPI3_POINTER pMpi3DriverPage1_t;

/**** Defines for the PageVersion field ****/
#define MPI3_DRIVER1_PAGEVERSION               (0x00)

/*****************************************************************************
 *              Driver Page 2                                               *
 ****************************************************************************/
#ifndef MPI3_DRIVER2_TRIGGER_MAX
#define MPI3_DRIVER2_TRIGGER_MAX           (1)
#endif  /* MPI3_DRIVER2_TRIGGER_MAX */

typedef struct _MPI3_DRIVER2_TRIGGER_EVENT
{
    U8                              Type;                                     /* 0x00 */
    U8                              Flags;                                    /* 0x01 */
    U8                              Reserved02;                               /* 0x02 */
    U8                              Event;                                    /* 0x03 */
    U32                             Reserved04[3];                            /* 0x04 */
} MPI3_DRIVER2_TRIGGER_EVENT, MPI3_POINTER PTR_MPI3_DRIVER2_TRIGGER_EVENT,
  Mpi3Driver2TriggerEvent_t, MPI3_POINTER pMpi3Driver2TriggerEvent_t;

typedef struct _MPI3_DRIVER2_TRIGGER_SCSI_SENSE
{
    U8                              Type;                                     /* 0x00 */
    U8                              Flags;                                    /* 0x01 */
    U16                             Reserved02;                               /* 0x02 */
    U8                              ASCQ;                                     /* 0x04 */
    U8                              ASC;                                      /* 0x05 */
    U8                              SenseKey;                                 /* 0x06 */
    U8                              Reserved07;                               /* 0x07 */
    U32                             Reserved08[2];                            /* 0x08 */
} MPI3_DRIVER2_TRIGGER_SCSI_SENSE, MPI3_POINTER PTR_MPI3_DRIVER2_TRIGGER_SCSI_SENSE,
  Mpi3Driver2TriggerScsiSense_t, MPI3_POINTER pMpi3Driver2TriggerScsiSense_t;

/**** Defines for the ASCQ field ****/
#define MPI3_DRIVER2_TRIGGER_SCSI_SENSE_ASCQ_MATCH_ALL                        (0xFF)

/**** Defines for the ASC field ****/
#define MPI3_DRIVER2_TRIGGER_SCSI_SENSE_ASC_MATCH_ALL                         (0xFF)

/**** Defines for the SenseKey field ****/
#define MPI3_DRIVER2_TRIGGER_SCSI_SENSE_SENSE_KEY_MATCH_ALL                   (0xFF)

typedef struct _MPI3_DRIVER2_TRIGGER_REPLY
{
    U8                              Type;                                     /* 0x00 */
    U8                              Flags;                                    /* 0x01 */
    U16                             IOCStatus;                                /* 0x02 */
    U32                             IOCLogInfo;                               /* 0x04 */
    U32                             IOCLogInfoMask;                           /* 0x08 */
    U32                             Reserved0C;                               /* 0x0C */
} MPI3_DRIVER2_TRIGGER_REPLY, MPI3_POINTER PTR_MPI3_DRIVER2_TRIGGER_REPLY,
  Mpi3Driver2TriggerReply_t, MPI3_POINTER pMpi3Driver2TriggerReply_t;

/**** Defines for the IOCStatus field ****/
#define MPI3_DRIVER2_TRIGGER_REPLY_IOCSTATUS_MATCH_ALL                        (0xFFFF)

typedef union _MPI3_DRIVER2_TRIGGER_ELEMENT
{
    MPI3_DRIVER2_TRIGGER_EVENT             Event;
    MPI3_DRIVER2_TRIGGER_SCSI_SENSE        ScsiSense;
    MPI3_DRIVER2_TRIGGER_REPLY             Reply;
} MPI3_DRIVER2_TRIGGER_ELEMENT, MPI3_POINTER PTR_MPI3_DRIVER2_TRIGGER_ELEMENT,
  Mpi3Driver2TriggerElement_t, MPI3_POINTER pMpi3Driver2TriggerElement_t;

/**** Defines for the Type field ****/
#define MPI3_DRIVER2_TRIGGER_TYPE_EVENT                                       (0x00)
#define MPI3_DRIVER2_TRIGGER_TYPE_SCSI_SENSE                                  (0x01)
#define MPI3_DRIVER2_TRIGGER_TYPE_REPLY                                       (0x02)

/**** Defines for the Flags field ****/
#define MPI3_DRIVER2_TRIGGER_FLAGS_DIAG_TRACE_RELEASE                         (0x02)
#define MPI3_DRIVER2_TRIGGER_FLAGS_DIAG_FW_RELEASE                            (0x01)

typedef struct _MPI3_DRIVER_PAGE2
{
    MPI3_CONFIG_PAGE_HEADER         Header;                                   /* 0x00 */
    U64                             GlobalTrigger;                            /* 0x08 */
    U32                             Reserved10[3];                            /* 0x10 */
    U8                              NumTriggers;                              /* 0x1C */
    U8                              Reserved1D[3];                            /* 0x1D */
    MPI3_DRIVER2_TRIGGER_ELEMENT    Trigger[MPI3_DRIVER2_TRIGGER_MAX];        /* 0x20 */   /* variable length */
} MPI3_DRIVER_PAGE2, MPI3_POINTER PTR_MPI3_DRIVER_PAGE2,
  Mpi3DriverPage2_t, MPI3_POINTER pMpi3DriverPage2_t;

/**** Defines for the PageVersion field ****/
#define MPI3_DRIVER2_PAGEVERSION               (0x00)

/**** Defines for the GlobalTrigger field ****/
#define MPI3_DRIVER2_GLOBALTRIGGER_DIAG_TRACE_RELEASE                       (0x8000000000000000ULL)
#define MPI3_DRIVER2_GLOBALTRIGGER_DIAG_FW_RELEASE                          (0x4000000000000000ULL)
#define MPI3_DRIVER2_GLOBALTRIGGER_SNAPDUMP_ENABLED                         (0x2000000000000000ULL)
#define MPI3_DRIVER2_GLOBALTRIGGER_POST_DIAG_TRACE_DISABLED                 (0x1000000000000000ULL)
#define MPI3_DRIVER2_GLOBALTRIGGER_POST_DIAG_FW_DISABLED                    (0x0800000000000000ULL)
#define MPI3_DRIVER2_GLOBALTRIGGER_DEVICE_REMOVAL_ENABLED                   (0x0000000000000004ULL)
#define MPI3_DRIVER2_GLOBALTRIGGER_TASK_MANAGEMENT_ENABLED                  (0x0000000000000002ULL)

/*****************************************************************************
 *              Driver Page 10                                              *
 ****************************************************************************/

typedef struct _MPI3_DRIVER_PAGE10
{
    MPI3_CONFIG_PAGE_HEADER         Header;                                   /* 0x00 */
    U16                             Flags;                                    /* 0x08 */
    U16                             Reserved0A;                               /* 0x0A */
    U8                              NumAllowedCommands;                       /* 0x0C */
    U8                              Reserved0D[3];                            /* 0x0D */
    MPI3_ALLOWED_CMD                AllowedCommand[MPI3_ALLOWED_CMDS_MAX];    /* 0x10 */   /* variable length */
} MPI3_DRIVER_PAGE10, MPI3_POINTER PTR_MPI3_DRIVER_PAGE10,
  Mpi3DriverPage10_t, MPI3_POINTER pMpi3DriverPage10_t;

/**** Defines for the PageVersion field ****/
#define MPI3_DRIVER10_PAGEVERSION               (0x00)

/**** Defines for the Flags field - use MPI3_DRIVER_FLAGS_ defines ****/

/*****************************************************************************
 *              Driver Page 20                                              *
 ****************************************************************************/

typedef struct _MPI3_DRIVER_PAGE20
{
    MPI3_CONFIG_PAGE_HEADER         Header;                                   /* 0x00 */
    U16                             Flags;                                    /* 0x08 */
    U16                             Reserved0A;                               /* 0x0A */
    U8                              NumAllowedCommands;                       /* 0x0C */
    U8                              Reserved0D[3];                            /* 0x0D */
    MPI3_ALLOWED_CMD                AllowedCommand[MPI3_ALLOWED_CMDS_MAX];    /* 0x10 */   /* variable length */
} MPI3_DRIVER_PAGE20, MPI3_POINTER PTR_MPI3_DRIVER_PAGE20,
  Mpi3DriverPage20_t, MPI3_POINTER pMpi3DriverPage20_t;

/**** Defines for the PageVersion field ****/
#define MPI3_DRIVER20_PAGEVERSION               (0x00)

/**** Defines for the Flags field - use MPI3_DRIVER_FLAGS_ defines ****/

/*****************************************************************************
 *              Driver Page 30                                              *
 ****************************************************************************/

typedef struct _MPI3_DRIVER_PAGE30
{
    MPI3_CONFIG_PAGE_HEADER         Header;                                   /* 0x00 */
    U16                             Flags;                                    /* 0x08 */
    U16                             Reserved0A;                               /* 0x0A */
    U8                              NumAllowedCommands;                       /* 0x0C */
    U8                              Reserved0D[3];                            /* 0x0D */
    MPI3_ALLOWED_CMD                AllowedCommand[MPI3_ALLOWED_CMDS_MAX];    /* 0x10 */   /* variable length */
} MPI3_DRIVER_PAGE30, MPI3_POINTER PTR_MPI3_DRIVER_PAGE30,
  Mpi3DriverPage30_t, MPI3_POINTER pMpi3DriverPage30_t;

/**** Defines for the PageVersion field ****/
#define MPI3_DRIVER30_PAGEVERSION               (0x00)

/**** Defines for the Flags field - use MPI3_DRIVER_FLAGS_ defines ****/

/*****************************************************************************
 *              Security Configuration Pages                                *
 ****************************************************************************/

typedef union _MPI3_SECURITY_MAC
{
    U32                             Dword[16];
    U16                             Word[32];
    U8                              Byte[64];
} MPI3_SECURITY_MAC, MPI3_POINTER PTR_MPI3_SECURITY_MAC,
  Mpi3SecurityMAC_t, MPI3_POINTER pMpi3SecurityMAC_t;

typedef union _MPI3_SECURITY_NONCE
{
    U32                             Dword[16];
    U16                             Word[32];
    U8                              Byte[64];
} MPI3_SECURITY_NONCE, MPI3_POINTER PTR_MPI3_SECURITY_NONCE,
  Mpi3SecurityNonce_t, MPI3_POINTER pMpi3SecurityNonce_t;

/*****************************************************************************
 *              Security Page 0                                             *
 ****************************************************************************/

typedef union _MPI3_SECURITY0_CERT_CHAIN
{
    U32                             Dword[1024];
    U16                             Word[2048];
    U8                              Byte[4096];
} MPI3_SECURITY0_CERT_CHAIN, MPI3_POINTER PTR_MPI3_SECURITY0_CERT_CHAIN,
  Mpi3Security0CertChain_t, MPI3_POINTER pMpi3Security0CertChain_t;

typedef struct _MPI3_SECURITY_PAGE0
{
    MPI3_CONFIG_PAGE_HEADER         Header;                                 /* 0x00 */
    U8                              SlotNumGroup;                           /* 0x08 */
    U8                              SlotNum;                                /* 0x09 */
    U16                             CertChainLength;                        /* 0x0A */
    U8                              CertChainFlags;                         /* 0x0C */
    U8                              Reserved0D[3];                          /* 0x0D */
    U32                             BaseAsymAlgo;                           /* 0x10 */
    U32                             BaseHashAlgo;                           /* 0x14 */
    U32                             Reserved18[4];                          /* 0x18 */
    MPI3_SECURITY_MAC               Mac;                                    /* 0x28 */
    MPI3_SECURITY_NONCE             Nonce;                                  /* 0x68 */
    MPI3_SECURITY0_CERT_CHAIN       CertificateChain;                       /* 0xA8 */
} MPI3_SECURITY_PAGE0, MPI3_POINTER PTR_MPI3_SECURITY_PAGE0,
  Mpi3SecurityPage0_t, MPI3_POINTER pMpi3SecurityPage0_t;

/**** Defines for the PageVersion field ****/
#define MPI3_SECURITY0_PAGEVERSION               (0x00)

/**** Defines for the CertChainFlags field ****/
#define MPI3_SECURITY0_CERTCHAIN_FLAGS_AUTH_API_MASK       (0x0E)
#define MPI3_SECURITY0_CERTCHAIN_FLAGS_AUTH_API_SHIFT      (1)
#define MPI3_SECURITY0_CERTCHAIN_FLAGS_AUTH_API_UNUSED     (0x00)
#define MPI3_SECURITY0_CERTCHAIN_FLAGS_AUTH_API_CERBERUS   (0x02)
#define MPI3_SECURITY0_CERTCHAIN_FLAGS_AUTH_API_SPDM       (0x04)
#define MPI3_SECURITY0_CERTCHAIN_FLAGS_SEALED              (0x01)

/*****************************************************************************
 *              Security Page 1                                             *
 ****************************************************************************/

#ifndef MPI3_SECURITY1_KEY_RECORD_MAX
#define MPI3_SECURITY1_KEY_RECORD_MAX      1
#endif  /* MPI3_SECURITY1_KEY_RECORD_MAX */

#ifndef MPI3_SECURITY1_PAD_MAX
#define MPI3_SECURITY1_PAD_MAX      4
#endif  /* MPI3_SECURITY1_PAD_MAX */

typedef union _MPI3_SECURITY1_KEY_DATA
{
    U32                             Dword[128];
    U16                             Word[256];
    U8                              Byte[512];
} MPI3_SECURITY1_KEY_DATA, MPI3_POINTER PTR_MPI3_SECURITY1_KEY_DATA,
  Mpi3Security1KeyData_t, MPI3_POINTER pMpi3Security1KeyData_t;

typedef struct _MPI3_SECURITY1_KEY_RECORD
{
    U8                              Flags;                                  /* 0x00 */
    U8                              Consumer;                               /* 0x01 */
    U16                             KeyDataSize;                            /* 0x02 */
    U32                             AdditionalKeyData;                      /* 0x04 */
    U32                             Reserved08[2];                          /* 0x08 */
    MPI3_SECURITY1_KEY_DATA         KeyData;                                /* 0x10 */
} MPI3_SECURITY1_KEY_RECORD, MPI3_POINTER PTR_MPI3_SECURITY1_KEY_RECORD,
  Mpi3Security1KeyRecord_t, MPI3_POINTER pMpi3Security1KeyRecord_t;

/**** Defines for the Flags field ****/
#define MPI3_SECURITY1_KEY_RECORD_FLAGS_TYPE_MASK            (0x1F)
#define MPI3_SECURITY1_KEY_RECORD_FLAGS_TYPE_SHIFT           (0)
#define MPI3_SECURITY1_KEY_RECORD_FLAGS_TYPE_NOT_VALID       (0x00)
#define MPI3_SECURITY1_KEY_RECORD_FLAGS_TYPE_HMAC            (0x01)
#define MPI3_SECURITY1_KEY_RECORD_FLAGS_TYPE_AES             (0x02)
#define MPI3_SECURITY1_KEY_RECORD_FLAGS_TYPE_ECDSA_PRIVATE   (0x03)
#define MPI3_SECURITY1_KEY_RECORD_FLAGS_TYPE_ECDSA_PUBLIC    (0x04)

/**** Defines for the Consumer field ****/
#define MPI3_SECURITY1_KEY_RECORD_CONSUMER_NOT_VALID         (0x00)
#define MPI3_SECURITY1_KEY_RECORD_CONSUMER_SAFESTORE         (0x01)
#define MPI3_SECURITY1_KEY_RECORD_CONSUMER_CERT_CHAIN        (0x02)
#define MPI3_SECURITY1_KEY_RECORD_CONSUMER_DEVICE_KEY        (0x03)
#define MPI3_SECURITY1_KEY_RECORD_CONSUMER_CACHE_OFFLOAD     (0x04)

typedef struct _MPI3_SECURITY_PAGE1
{
    MPI3_CONFIG_PAGE_HEADER         Header;                                     /* 0x00 */
    U32                             Reserved08[2];                              /* 0x08 */
    MPI3_SECURITY_MAC               Mac;                                        /* 0x10 */
    MPI3_SECURITY_NONCE             Nonce;                                      /* 0x50 */
    U8                              NumKeys;                                    /* 0x90 */
    U8                              Reserved91[3];                              /* 0x91 */
    U32                             Reserved94[3];                              /* 0x94 */
    MPI3_SECURITY1_KEY_RECORD       KeyRecord[MPI3_SECURITY1_KEY_RECORD_MAX];   /* 0xA0 */
    U8                              Pad[MPI3_SECURITY1_PAD_MAX];                /* ??  */
} MPI3_SECURITY_PAGE1, MPI3_POINTER PTR_MPI3_SECURITY_PAGE1,
  Mpi3SecurityPage1_t, MPI3_POINTER pMpi3SecurityPage1_t;

/**** Defines for the PageVersion field ****/
#define MPI3_SECURITY1_PAGEVERSION               (0x00)


/*****************************************************************************
 *              Security Page 2                                             *
 ****************************************************************************/

#ifndef MPI3_SECURITY2_TRUSTED_ROOT_MAX
#define MPI3_SECURITY2_TRUSTED_ROOT_MAX      1
#endif  /* MPI3_SECURITY2_TRUSTED_ROOT_MAX */

#ifndef MPI3_SECURITY2_ROOT_LEN
#define MPI3_SECURITY2_ROOT_LEN      4
#endif  /* MPI3_SECURITY2_ROOT_LEN */

typedef struct _MPI3_SECURITY2_TRUSTED_ROOT
{
    U8                              Level;                                        /* 0x00 */
    U8                              HashAlgorithm;                                /* 0x01 */
    U16                             TrustedRootFlags;                             /* 0x02 */
    U32                             Reserved04[3];                                /* 0x04 */
    U8                              Root[MPI3_SECURITY2_ROOT_LEN];                /* 0x10 */ /* variable length */
} MPI3_SECURITY2_TRUSTED_ROOT, MPI3_POINTER PTR_MPI3_SECURITY2_TRUSTED_ROOT,
  Mpi3Security2TrustedRoot_t, MPI3_POINTER pMpi3Security2TrustedRoot_t;

/**** Defines for the TrustedRootFlags field ****/
#define MPI3_SECURITY2_TRUSTEDROOT_TRUSTEDROOTFLAGS_ROOTFORM_MASK                  (0xF000)
#define MPI3_SECURITY2_TRUSTEDROOT_TRUSTEDROOTFLAGS_ROOTFORM_SHIFT                 (12)
#define MPI3_SECURITY2_TRUSTEDROOT_TRUSTEDROOTFLAGS_ROOTFORM_DIGEST                (0x0000)
#define MPI3_SECURITY2_TRUSTEDROOT_TRUSTEDROOTFLAGS_ROOTFORM_DERCERT               (0x1000)
#define MPI3_SECURITY2_TRUSTEDROOT_TRUSTEDROOTFLAGS_HASHALGOSOURCE_MASK            (0x0006)
#define MPI3_SECURITY2_TRUSTEDROOT_TRUSTEDROOTFLAGS_HASHALGOSOURCE_SHIFT           (1)
#define MPI3_SECURITY2_TRUSTEDROOT_TRUSTEDROOTFLAGS_HASHALGOSOURCE_HA_FIELD        (0x0000)
#define MPI3_SECURITY2_TRUSTEDROOT_TRUSTEDROOTFLAGS_HASHALGOSOURCE_AKI             (0x0002)
#define MPI3_SECURITY2_TRUSTEDROOT_TRUSTEDROOTFLAGS_USERPROVISIONED_YES            (0x0001)

typedef struct _MPI3_SECURITY_PAGE2
{
    MPI3_CONFIG_PAGE_HEADER         Header;                                        /* 0x00 */
    U32                             Reserved08[2];                                 /* 0x08 */
    MPI3_SECURITY_MAC               Mac;                                           /* 0x10 */
    MPI3_SECURITY_NONCE             Nonce;                                         /* 0x50 */
    U32                             Reserved90[3];                                 /* 0x90 */
    U8                              NumRoots;                                      /* 0x9C */
    U8                              Reserved9D;                                    /* 0x9D */
    U16                             RootElementSize;                               /* 0x9E */
    MPI3_SECURITY2_TRUSTED_ROOT     TrustedRoot[MPI3_SECURITY2_TRUSTED_ROOT_MAX];  /* 0xA0 */ /* variable length */
} MPI3_SECURITY_PAGE2, MPI3_POINTER PTR_MPI3_SECURITY_PAGE2,
  Mpi3SecurityPage2_t, MPI3_POINTER pMpi3SecurityPage2_t;

/**** Defines for the PageVersion field ****/
#define MPI3_SECURITY2_PAGEVERSION               (0x00)


/*****************************************************************************
 *              SAS IO Unit Configuration Pages                              *
 ****************************************************************************/

/*****************************************************************************
 *              SAS IO Unit Page 0                                           *
 ****************************************************************************/
typedef struct _MPI3_SAS_IO_UNIT0_PHY_DATA
{
    U8              IOUnitPort;                         /* 0x00 */
    U8              PortFlags;                          /* 0x01 */
    U8              PhyFlags;                           /* 0x02 */
    U8              NegotiatedLinkRate;                 /* 0x03 */
    U16             ControllerPhyDeviceInfo;            /* 0x04 */
    U16             Reserved06;                         /* 0x06 */
    U16             AttachedDevHandle;                  /* 0x08 */
    U16             ControllerDevHandle;                /* 0x0A */
    U32             DiscoveryStatus;                    /* 0x0C */
    U32             Reserved10;                         /* 0x10 */
} MPI3_SAS_IO_UNIT0_PHY_DATA, MPI3_POINTER PTR_MPI3_SAS_IO_UNIT0_PHY_DATA,
  Mpi3SasIOUnit0PhyData_t, MPI3_POINTER pMpi3SasIOUnit0PhyData_t;

#ifndef MPI3_SAS_IO_UNIT0_PHY_MAX
#define MPI3_SAS_IO_UNIT0_PHY_MAX           (1)
#endif  /* MPI3_SAS_IO_UNIT0_PHY_MAX */

typedef struct _MPI3_SAS_IO_UNIT_PAGE0
{
    MPI3_CONFIG_PAGE_HEADER         Header;                                 /* 0x00 */
    U32                             Reserved08;                             /* 0x08 */
    U8                              NumPhys;                                /* 0x0C */
    U8                              InitStatus;                             /* 0x0D */
    U16                             Reserved0E;                             /* 0x0E */
    MPI3_SAS_IO_UNIT0_PHY_DATA      PhyData[MPI3_SAS_IO_UNIT0_PHY_MAX];     /* 0x10 */
} MPI3_SAS_IO_UNIT_PAGE0, MPI3_POINTER PTR_MPI3_SAS_IO_UNIT_PAGE0,
  Mpi3SasIOUnitPage0_t, MPI3_POINTER pMpi3SasIOUnitPage0_t;

/**** Defines for the PageVersion field ****/
#define MPI3_SASIOUNIT0_PAGEVERSION                          (0x00)

/**** Defines for the InitStatus field ****/
#define MPI3_SASIOUNIT0_INITSTATUS_NO_ERRORS                 (0x00)
#define MPI3_SASIOUNIT0_INITSTATUS_NEEDS_INITIALIZATION      (0x01)
#define MPI3_SASIOUNIT0_INITSTATUS_NO_TARGETS_ALLOCATED      (0x02)
#define MPI3_SASIOUNIT0_INITSTATUS_BAD_NUM_PHYS              (0x04)
#define MPI3_SASIOUNIT0_INITSTATUS_UNSUPPORTED_CONFIG        (0x05)
#define MPI3_SASIOUNIT0_INITSTATUS_HOST_PHYS_ENABLED         (0x06)
#define MPI3_SASIOUNIT0_INITSTATUS_PRODUCT_SPECIFIC_MIN      (0xF0)
#define MPI3_SASIOUNIT0_INITSTATUS_PRODUCT_SPECIFIC_MAX      (0xFF)

/**** Defines for the PortFlags field ****/
#define MPI3_SASIOUNIT0_PORTFLAGS_DISC_IN_PROGRESS           (0x08)
#define MPI3_SASIOUNIT0_PORTFLAGS_AUTO_PORT_CONFIG_MASK      (0x03)
#define MPI3_SASIOUNIT0_PORTFLAGS_AUTO_PORT_CONFIG_SHIFT     (0)
#define MPI3_SASIOUNIT0_PORTFLAGS_AUTO_PORT_CONFIG_IOUNIT1   (0x00)
#define MPI3_SASIOUNIT0_PORTFLAGS_AUTO_PORT_CONFIG_DYNAMIC   (0x01)
#define MPI3_SASIOUNIT0_PORTFLAGS_AUTO_PORT_CONFIG_BACKPLANE (0x02)

/**** Defines for the PhyFlags field ****/
#define MPI3_SASIOUNIT0_PHYFLAGS_INIT_PERSIST_CONNECT        (0x40)
#define MPI3_SASIOUNIT0_PHYFLAGS_TARG_PERSIST_CONNECT        (0x20)
#define MPI3_SASIOUNIT0_PHYFLAGS_PHY_DISABLED                (0x08)
#define MPI3_SASIOUNIT0_PHYFLAGS_VIRTUAL_PHY                 (0x02)
#define MPI3_SASIOUNIT0_PHYFLAGS_HOST_PHY                    (0x01)

/**** Use MPI3_SAS_NEG_LINK_RATE_ defines for the NegotiatedLinkRate field ****/

/**** Use MPI3_SAS_DEVICE_INFO_ defines (see mpi30_sas.h) for the ControllerPhyDeviceInfo field ****/

/**** Use MPI3_SAS_DISC_STATUS_ defines (see mpi30_ioc.h) for the DiscoveryStatus field ****/

/*****************************************************************************
 *              SAS IO Unit Page 1                                           *
 ****************************************************************************/
typedef struct _MPI3_SAS_IO_UNIT1_PHY_DATA
{
    U8              IOUnitPort;                         /* 0x00 */
    U8              PortFlags;                          /* 0x01 */
    U8              PhyFlags;                           /* 0x02 */
    U8              MaxMinLinkRate;                     /* 0x03 */
    U16             ControllerPhyDeviceInfo;            /* 0x04 */
    U16             MaxTargetPortConnectTime;           /* 0x06 */
    U32             Reserved08;                         /* 0x08 */
} MPI3_SAS_IO_UNIT1_PHY_DATA, MPI3_POINTER PTR_MPI3_SAS_IO_UNIT1_PHY_DATA,
  Mpi3SasIOUnit1PhyData_t, MPI3_POINTER pMpi3SasIOUnit1PhyData_t;

#ifndef MPI3_SAS_IO_UNIT1_PHY_MAX
#define MPI3_SAS_IO_UNIT1_PHY_MAX           (1)
#endif  /* MPI3_SAS_IO_UNIT1_PHY_MAX */

typedef struct _MPI3_SAS_IO_UNIT_PAGE1
{
    MPI3_CONFIG_PAGE_HEADER         Header;                                 /* 0x00 */
    U16                             ControlFlags;                           /* 0x08 */
    U16                             SASNarrowMaxQueueDepth;                 /* 0x0A */
    U16                             AdditionalControlFlags;                 /* 0x0C */
    U16                             SASWideMaxQueueDepth;                   /* 0x0E */
    U8                              NumPhys;                                /* 0x10 */
    U8                              SATAMaxQDepth;                          /* 0x11 */
    U16                             Reserved12;                             /* 0x12 */
    MPI3_SAS_IO_UNIT1_PHY_DATA      PhyData[MPI3_SAS_IO_UNIT1_PHY_MAX];     /* 0x14 */
} MPI3_SAS_IO_UNIT_PAGE1, MPI3_POINTER PTR_MPI3_SAS_IO_UNIT_PAGE1,
  Mpi3SasIOUnitPage1_t, MPI3_POINTER pMpi3SasIOUnitPage1_t;

/**** Defines for the PageVersion field ****/
#define MPI3_SASIOUNIT1_PAGEVERSION                                 (0x00)

/**** Defines for the ControlFlags field ****/
#define MPI3_SASIOUNIT1_CONTROL_CONTROLLER_DEVICE_SELF_TEST         (0x8000)
#define MPI3_SASIOUNIT1_CONTROL_SATA_SW_PRESERVE                    (0x1000)
#define MPI3_SASIOUNIT1_CONTROL_SATA_48BIT_LBA_REQUIRED             (0x0080)
#define MPI3_SASIOUNIT1_CONTROL_SATA_SMART_REQUIRED                 (0x0040)
#define MPI3_SASIOUNIT1_CONTROL_SATA_NCQ_REQUIRED                   (0x0020)
#define MPI3_SASIOUNIT1_CONTROL_SATA_FUA_REQUIRED                   (0x0010)
#define MPI3_SASIOUNIT1_CONTROL_TABLE_SUBTRACTIVE_ILLEGAL           (0x0008)
#define MPI3_SASIOUNIT1_CONTROL_SUBTRACTIVE_ILLEGAL                 (0x0004)
#define MPI3_SASIOUNIT1_CONTROL_FIRST_LVL_DISC_ONLY                 (0x0002)
#define MPI3_SASIOUNIT1_CONTROL_HARD_RESET_MASK                     (0x0001)
#define MPI3_SASIOUNIT1_CONTROL_HARD_RESET_SHIFT                    (0)
#define MPI3_SASIOUNIT1_CONTROL_HARD_RESET_DEVICE_NAME              (0x0000)
#define MPI3_SASIOUNIT1_CONTROL_HARD_RESET_SAS_ADDRESS              (0x0001)

/**** Defines for the AdditionalControlFlags field ****/
#define MPI3_SASIOUNIT1_ACONTROL_DA_PERSIST_CONNECT                 (0x0100)
#define MPI3_SASIOUNIT1_ACONTROL_MULTI_PORT_DOMAIN_ILLEGAL          (0x0080)
#define MPI3_SASIOUNIT1_ACONTROL_SATA_ASYNCHROUNOUS_NOTIFICATION    (0x0040)
#define MPI3_SASIOUNIT1_ACONTROL_INVALID_TOPOLOGY_CORRECTION        (0x0020)
#define MPI3_SASIOUNIT1_ACONTROL_PORT_ENABLE_ONLY_SATA_LINK_RESET   (0x0010)
#define MPI3_SASIOUNIT1_ACONTROL_OTHER_AFFILIATION_SATA_LINK_RESET  (0x0008)
#define MPI3_SASIOUNIT1_ACONTROL_SELF_AFFILIATION_SATA_LINK_RESET   (0x0004)
#define MPI3_SASIOUNIT1_ACONTROL_NO_AFFILIATION_SATA_LINK_RESET     (0x0002)
#define MPI3_SASIOUNIT1_ACONTROL_ALLOW_TABLE_TO_TABLE               (0x0001)

/**** Defines for the PortFlags field ****/
#define MPI3_SASIOUNIT1_PORT_FLAGS_AUTO_PORT_CONFIG                 (0x01)

/**** Defines for the PhyFlags field ****/
#define MPI3_SASIOUNIT1_PHYFLAGS_INIT_PERSIST_CONNECT               (0x40)
#define MPI3_SASIOUNIT1_PHYFLAGS_TARG_PERSIST_CONNECT               (0x20)
#define MPI3_SASIOUNIT1_PHYFLAGS_PHY_DISABLE                        (0x08)

/**** Defines for the MaxMinLinkRate field ****/
#define MPI3_SASIOUNIT1_MMLR_MAX_RATE_MASK                          (0xF0)
#define MPI3_SASIOUNIT1_MMLR_MAX_RATE_SHIFT                         (4)
#define MPI3_SASIOUNIT1_MMLR_MAX_RATE_6_0                           (0xA0)
#define MPI3_SASIOUNIT1_MMLR_MAX_RATE_12_0                          (0xB0)
#define MPI3_SASIOUNIT1_MMLR_MAX_RATE_22_5                          (0xC0)
#define MPI3_SASIOUNIT1_MMLR_MIN_RATE_MASK                          (0x0F)
#define MPI3_SASIOUNIT1_MMLR_MIN_RATE_SHIFT                         (0)
#define MPI3_SASIOUNIT1_MMLR_MIN_RATE_6_0                           (0x0A)
#define MPI3_SASIOUNIT1_MMLR_MIN_RATE_12_0                          (0x0B)
#define MPI3_SASIOUNIT1_MMLR_MIN_RATE_22_5                          (0x0C)

/**** Use MPI3_SAS_DEVICE_INFO_ defines (see mpi30_sas.h) for the ControllerPhyDeviceInfo field ****/

/*****************************************************************************
 *              SAS IO Unit Page 2                                           *
 ****************************************************************************/
typedef struct _MPI3_SAS_IO_UNIT2_PHY_PM_SETTINGS
{
    U8              ControlFlags;                       /* 0x00 */
    U8              Reserved01;                         /* 0x01 */
    U16             InactivityTimerExponent;            /* 0x02 */
    U8              SATAPartialTimeout;                 /* 0x04 */
    U8              Reserved05;                         /* 0x05 */
    U8              SATASlumberTimeout;                 /* 0x06 */
    U8              Reserved07;                         /* 0x07 */
    U8              SASPartialTimeout;                  /* 0x08 */
    U8              Reserved09;                         /* 0x09 */
    U8              SASSlumberTimeout;                  /* 0x0A */
    U8              Reserved0B;                         /* 0x0B */
} MPI3_SAS_IO_UNIT2_PHY_PM_SETTINGS, MPI3_POINTER PTR_MPI3_SAS_IO_UNIT2_PHY_PM_SETTINGS,
  Mpi3SasIOUnit2PhyPmSettings_t, MPI3_POINTER pMpi3SasIOUnit2PhyPmSettings_t;

#ifndef MPI3_SAS_IO_UNIT2_PHY_MAX
#define MPI3_SAS_IO_UNIT2_PHY_MAX           (1)
#endif  /* MPI3_SAS_IO_UNIT2_PHY_MAX */

typedef struct _MPI3_SAS_IO_UNIT_PAGE2
{
    MPI3_CONFIG_PAGE_HEADER             Header;                                                     /* 0x00 */
    U8                                  NumPhys;                                                    /* 0x08 */
    U8                                  Reserved09[3];                                              /* 0x09 */
    U32                                 Reserved0C;                                                 /* 0x0C */
    MPI3_SAS_IO_UNIT2_PHY_PM_SETTINGS   SASPhyPowerManagementSettings[MPI3_SAS_IO_UNIT2_PHY_MAX];   /* 0x10 */
} MPI3_SAS_IO_UNIT_PAGE2, MPI3_POINTER PTR_MPI3_SAS_IO_UNIT_PAGE2,
  Mpi3SasIOUnitPage2_t, MPI3_POINTER pMpi3SasIOUnitPage2_t;

/**** Defines for the PageVersion field ****/
#define MPI3_SASIOUNIT2_PAGEVERSION                     (0x00)

/**** Defines for the ControlFlags field ****/
#define MPI3_SASIOUNIT2_CONTROL_SAS_SLUMBER_ENABLE      (0x08)
#define MPI3_SASIOUNIT2_CONTROL_SAS_PARTIAL_ENABLE      (0x04)
#define MPI3_SASIOUNIT2_CONTROL_SATA_SLUMBER_ENABLE     (0x02)
#define MPI3_SASIOUNIT2_CONTROL_SATA_PARTIAL_ENABLE     (0x01)

/**** Defines for the InactivityTimerExponent field ****/
#define MPI3_SASIOUNIT2_ITE_SAS_SLUMBER_MASK            (0x7000)
#define MPI3_SASIOUNIT2_ITE_SAS_SLUMBER_SHIFT           (12)
#define MPI3_SASIOUNIT2_ITE_SAS_PARTIAL_MASK            (0x0700)
#define MPI3_SASIOUNIT2_ITE_SAS_PARTIAL_SHIFT           (8)
#define MPI3_SASIOUNIT2_ITE_SATA_SLUMBER_MASK           (0x0070)
#define MPI3_SASIOUNIT2_ITE_SATA_SLUMBER_SHIFT          (4)
#define MPI3_SASIOUNIT2_ITE_SATA_PARTIAL_MASK           (0x0007)
#define MPI3_SASIOUNIT2_ITE_SATA_PARTIAL_SHIFT          (0)

#define MPI3_SASIOUNIT2_ITE_EXP_TEN_SECONDS             (7)
#define MPI3_SASIOUNIT2_ITE_EXP_ONE_SECOND              (6)
#define MPI3_SASIOUNIT2_ITE_EXP_HUNDRED_MILLISECONDS    (5)
#define MPI3_SASIOUNIT2_ITE_EXP_TEN_MILLISECONDS        (4)
#define MPI3_SASIOUNIT2_ITE_EXP_ONE_MILLISECOND         (3)
#define MPI3_SASIOUNIT2_ITE_EXP_HUNDRED_MICROSECONDS    (2)
#define MPI3_SASIOUNIT2_ITE_EXP_TEN_MICROSECONDS        (1)
#define MPI3_SASIOUNIT2_ITE_EXP_ONE_MICROSECOND         (0)

/*****************************************************************************
 *              SAS IO Unit Page 3                                           *
 ****************************************************************************/
typedef struct _MPI3_SAS_IO_UNIT_PAGE3
{
    MPI3_CONFIG_PAGE_HEADER         Header;                         /* 0x00 */
    U32                             Reserved08;                     /* 0x08 */
    U32                             PowerManagementCapabilities;    /* 0x0C */
} MPI3_SAS_IO_UNIT_PAGE3, MPI3_POINTER PTR_MPI3_SAS_IO_UNIT_PAGE3,
  Mpi3SasIOUnitPage3_t, MPI3_POINTER pMpi3SasIOUnitPage3_t;

/**** Defines for the PageVersion field ****/
#define MPI3_SASIOUNIT3_PAGEVERSION                     (0x00)

/**** Defines for the PowerManagementCapabilities field ****/
#define MPI3_SASIOUNIT3_PM_HOST_SAS_SLUMBER_MODE        (0x00000800)
#define MPI3_SASIOUNIT3_PM_HOST_SAS_PARTIAL_MODE        (0x00000400)
#define MPI3_SASIOUNIT3_PM_HOST_SATA_SLUMBER_MODE       (0x00000200)
#define MPI3_SASIOUNIT3_PM_HOST_SATA_PARTIAL_MODE       (0x00000100)
#define MPI3_SASIOUNIT3_PM_IOUNIT_SAS_SLUMBER_MODE      (0x00000008)
#define MPI3_SASIOUNIT3_PM_IOUNIT_SAS_PARTIAL_MODE      (0x00000004)
#define MPI3_SASIOUNIT3_PM_IOUNIT_SATA_SLUMBER_MODE     (0x00000002)
#define MPI3_SASIOUNIT3_PM_IOUNIT_SATA_PARTIAL_MODE     (0x00000001)


/*****************************************************************************
 *              SAS Expander Configuration Pages                             *
 ****************************************************************************/

/*****************************************************************************
 *              SAS Expander Page 0                                          *
 ****************************************************************************/
typedef struct _MPI3_SAS_EXPANDER_PAGE0
{
    MPI3_CONFIG_PAGE_HEADER         Header;                         /* 0x00 */
    U8                              IOUnitPort;                     /* 0x08 */
    U8                              ReportGenLength;                /* 0x09 */
    U16                             EnclosureHandle;                /* 0x0A */
    U32                             Reserved0C;                     /* 0x0C */
    U64                             SASAddress;                     /* 0x10 */
    U32                             DiscoveryStatus;                /* 0x18 */
    U16                             DevHandle;                      /* 0x1C */
    U16                             ParentDevHandle;                /* 0x1E */
    U16                             ExpanderChangeCount;            /* 0x20 */
    U16                             ExpanderRouteIndexes;           /* 0x22 */
    U8                              NumPhys;                        /* 0x24 */
    U8                              SASLevel;                       /* 0x25 */
    U16                             Flags;                          /* 0x26 */
    U16                             STPBusInactivityTimeLimit;      /* 0x28 */
    U16                             STPMaxConnectTimeLimit;         /* 0x2A */
    U16                             STP_SMP_NexusLossTime;          /* 0x2C */
    U16                             MaxNumRoutedSASAddresses;       /* 0x2E */
    U64                             ActiveZoneManagerSASAddress;    /* 0x30 */
    U16                             ZoneLockInactivityLimit;        /* 0x38 */
    U16                             Reserved3A;                     /* 0x3A */
    U8                              TimeToReducedFunc;              /* 0x3C */
    U8                              InitialTimeToReducedFunc;       /* 0x3D */
    U8                              MaxReducedFuncTime;             /* 0x3E */
    U8                              ExpStatus;                      /* 0x3F */
} MPI3_SAS_EXPANDER_PAGE0, MPI3_POINTER PTR_MPI3_SAS_EXPANDER_PAGE0,
  Mpi3SasExpanderPage0_t, MPI3_POINTER pMpi3SasExpanderPage0_t;

/**** Defines for the PageVersion field ****/
#define MPI3_SASEXPANDER0_PAGEVERSION                       (0x00)

/**** Use MPI3_SAS_DISC_STATUS_ defines (see mpi30_ioc.h) for the DiscoveryStatus field ****/

/**** Defines for the Flags field ****/
#define MPI3_SASEXPANDER0_FLAGS_REDUCED_FUNCTIONALITY       (0x2000)
#define MPI3_SASEXPANDER0_FLAGS_ZONE_LOCKED                 (0x1000)
#define MPI3_SASEXPANDER0_FLAGS_SUPPORTED_PHYSICAL_PRES     (0x0800)
#define MPI3_SASEXPANDER0_FLAGS_ASSERTED_PHYSICAL_PRES      (0x0400)
#define MPI3_SASEXPANDER0_FLAGS_ZONING_SUPPORT              (0x0200)
#define MPI3_SASEXPANDER0_FLAGS_ENABLED_ZONING              (0x0100)
#define MPI3_SASEXPANDER0_FLAGS_TABLE_TO_TABLE_SUPPORT      (0x0080)
#define MPI3_SASEXPANDER0_FLAGS_CONNECTOR_END_DEVICE        (0x0010)
#define MPI3_SASEXPANDER0_FLAGS_OTHERS_CONFIG               (0x0004)
#define MPI3_SASEXPANDER0_FLAGS_CONFIG_IN_PROGRESS          (0x0002)
#define MPI3_SASEXPANDER0_FLAGS_ROUTE_TABLE_CONFIG          (0x0001)

/**** Defines for the ExpStatus field ****/
#define MPI3_SASEXPANDER0_ES_NOT_RESPONDING                 (0x02)
#define MPI3_SASEXPANDER0_ES_RESPONDING                     (0x03)
#define MPI3_SASEXPANDER0_ES_DELAY_NOT_RESPONDING           (0x04)

/*****************************************************************************
 *              SAS Expander Page 1                                          *
 ****************************************************************************/
typedef struct _MPI3_SAS_EXPANDER_PAGE1
{
    MPI3_CONFIG_PAGE_HEADER         Header;                     /* 0x00 */
    U8                              IOUnitPort;                 /* 0x08 */
    U8                              Reserved09[3];              /* 0x09 */
    U8                              NumPhys;                    /* 0x0C */
    U8                              Phy;                        /* 0x0D */
    U16                             NumTableEntriesProgrammed;  /* 0x0E */
    U8                              ProgrammedLinkRate;         /* 0x10 */
    U8                              HwLinkRate;                 /* 0x11 */
    U16                             AttachedDevHandle;          /* 0x12 */
    U32                             PhyInfo;                    /* 0x14 */
    U16                             AttachedDeviceInfo;         /* 0x18 */
    U16                             Reserved1A;                 /* 0x1A */
    U16                             ExpanderDevHandle;          /* 0x1C */
    U8                              ChangeCount;                /* 0x1E */
    U8                              NegotiatedLinkRate;         /* 0x1F */
    U8                              PhyIdentifier;              /* 0x20 */
    U8                              AttachedPhyIdentifier;      /* 0x21 */
    U8                              Reserved22;                 /* 0x22 */
    U8                              DiscoveryInfo;              /* 0x23 */
    U32                             AttachedPhyInfo;            /* 0x24 */
    U8                              ZoneGroup;                  /* 0x28 */
    U8                              SelfConfigStatus;           /* 0x29 */
    U16                             Reserved2A;                 /* 0x2A */
    U16                             Slot;                       /* 0x2C */
    U16                             SlotIndex;                  /* 0x2E */
} MPI3_SAS_EXPANDER_PAGE1, MPI3_POINTER PTR_MPI3_SAS_EXPANDER_PAGE1,
  Mpi3SasExpanderPage1_t, MPI3_POINTER pMpi3SasExpanderPage1_t;

/**** Defines for the PageVersion field ****/
#define MPI3_SASEXPANDER1_PAGEVERSION                   (0x00)

/**** Defines for the ProgrammedLinkRate field - use MPI3_SAS_PRATE_ defines ****/

/**** Defines for the HwLinkRate field - use MPI3_SAS_HWRATE_ defines ****/

/**** Defines for the PhyInfo field - use MPI3_SAS_PHYINFO_ defines ****/

/**** Defines for the AttachedDeviceInfo field - use MPI3_SAS_DEVICE_INFO_ defines ****/

/**** Defines for the NegotiatedLinkRate field - use use MPI3_SAS_NEG_LINK_RATE_ defines ****/

/**** Defines for the DiscoveryInfo field ****/
#define MPI3_SASEXPANDER1_DISCINFO_BAD_PHY_DISABLED     (0x04)
#define MPI3_SASEXPANDER1_DISCINFO_LINK_STATUS_CHANGE   (0x02)
#define MPI3_SASEXPANDER1_DISCINFO_NO_ROUTING_ENTRIES   (0x01)

/**** Defines for the AttachedPhyInfo field - use MPI3_SAS_APHYINFO_ defines ****/

/**** Defines for the Slot field - use MPI3_SLOT_ defines ****/

/**** Defines for the SlotIndex field - use MPI3_SLOT_INDEX_ ****/


/*****************************************************************************
 *              SAS Expander Page 2                                          *
 ****************************************************************************/
#ifndef MPI3_SASEXPANDER2_MAX_NUM_PHYS
#define MPI3_SASEXPANDER2_MAX_NUM_PHYS                               (1)
#endif  /* MPI3_SASEXPANDER2_MAX_NUM_PHYS */

typedef struct _MPI3_SASEXPANDER2_PHY_ELEMENT
{
    U8                              LinkChangeCount;                       /* 0x00 */
    U8                              Reserved01;                            /* 0x01 */
    U16                             RateChangeCount;                       /* 0x02 */
    U32                             Reserved04;                            /* 0x04 */
} MPI3_SASEXPANDER2_PHY_ELEMENT, MPI3_POINTER PTR_MPI3_SASEXPANDER2_PHY_ELEMENT,
  Mpi3SasExpander2PhyElement_t, MPI3_POINTER pMpi3SasExpander2PhyElement_t;

typedef struct _MPI3_SAS_EXPANDER_PAGE2
{
    MPI3_CONFIG_PAGE_HEADER         Header;                                /* 0x00 */
    U8                              NumPhys;                               /* 0x08 */
    U8                              Reserved09;                            /* 0x09 */
    U16                             DevHandle;                             /* 0x0A */
    U32                             Reserved0C;                            /* 0x0C */
    MPI3_SASEXPANDER2_PHY_ELEMENT   Phy[MPI3_SASEXPANDER2_MAX_NUM_PHYS];   /* 0x10 */   /* variable length */

} MPI3_SAS_EXPANDER_PAGE2, MPI3_POINTER PTR_MPI3_SAS_EXPANDER_PAGE2,
  Mpi3SasExpanderPage2_t, MPI3_POINTER pMpi3SasExpanderPage2_t;

/**** Defines for the PageVersion field ****/
#define MPI3_SASEXPANDER2_PAGEVERSION                   (0x00)


/*****************************************************************************
 *              SAS Port Configuration Pages                                 *
 ****************************************************************************/

/*****************************************************************************
 *              SAS Port Page 0                                              *
 ****************************************************************************/
typedef struct _MPI3_SAS_PORT_PAGE0
{
    MPI3_CONFIG_PAGE_HEADER         Header;                 /* 0x00 */
    U8                              PortNumber;             /* 0x08 */
    U8                              Reserved09;             /* 0x09 */
    U8                              PortWidth;              /* 0x0A */
    U8                              Reserved0B;             /* 0x0B */
    U8                              ZoneGroup;              /* 0x0C */
    U8                              Reserved0D[3];          /* 0x0D */
    U64                             SASAddress;             /* 0x10 */
    U16                             DeviceInfo;             /* 0x18 */
    U16                             Reserved1A;             /* 0x1A */
    U32                             Reserved1C;             /* 0x1C */
} MPI3_SAS_PORT_PAGE0, MPI3_POINTER PTR_MPI3_SAS_PORT_PAGE0,
  Mpi3SasPortPage0_t, MPI3_POINTER pMpi3SasPortPage0_t;

/**** Defines for the PageVersion field ****/
#define MPI3_SASPORT0_PAGEVERSION                       (0x00)

/**** Defines for the DeviceInfo field - use MPI3_SAS_DEVICE_INFO_ defines ****/

/*****************************************************************************
 *              SAS PHY Configuration Pages                                  *
 ****************************************************************************/

/*****************************************************************************
 *              SAS PHY Page 0                                               *
 ****************************************************************************/
typedef struct _MPI3_SAS_PHY_PAGE0
{
    MPI3_CONFIG_PAGE_HEADER         Header;                 /* 0x00 */
    U16                             OwnerDevHandle;         /* 0x08 */
    U16                             Reserved0A;             /* 0x0A */
    U16                             AttachedDevHandle;      /* 0x0C */
    U8                              AttachedPhyIdentifier;  /* 0x0E */
    U8                              Reserved0F;             /* 0x0F */
    U32                             AttachedPhyInfo;        /* 0x10 */
    U8                              ProgrammedLinkRate;     /* 0x14 */
    U8                              HwLinkRate;             /* 0x15 */
    U8                              ChangeCount;            /* 0x16 */
    U8                              Flags;                  /* 0x17 */
    U32                             PhyInfo;                /* 0x18 */
    U8                              NegotiatedLinkRate;     /* 0x1C */
    U8                              Reserved1D[3];          /* 0x1D */
    U16                             Slot;                   /* 0x20 */
    U16                             SlotIndex;              /* 0x22 */
} MPI3_SAS_PHY_PAGE0, MPI3_POINTER PTR_MPI3_SAS_PHY_PAGE0,
  Mpi3SasPhyPage0_t, MPI3_POINTER pMpi3SasPhyPage0_t;

/**** Defines for the PageVersion field ****/
#define MPI3_SASPHY0_PAGEVERSION                        (0x00)

/**** Defines for the AttachedPhyInfo field - use MPI3_SAS_APHYINFO_ defines ****/

/**** Defines for the ProgrammedLinkRate field - use MPI3_SAS_PRATE_ defines ****/

/**** Defines for the HwLinkRate field - use MPI3_SAS_HWRATE_ defines ****/

/**** Defines for the Flags field ****/
#define MPI3_SASPHY0_FLAGS_SGPIO_DIRECT_ATTACH_ENC      (0x01)

/**** Defines for the PhyInfo field - use MPI3_SAS_PHYINFO_ defines ****/

/**** Defines for the NegotiatedLinkRate field - use MPI3_SAS_NEG_LINK_RATE_ defines ****/

/**** Defines for the Slot field - use MPI3_SLOT_ defines ****/

/**** Defines for the SlotIndex field - use MPI3_SLOT_INDEX_ ****/

/*****************************************************************************
 *              SAS PHY Page 1                                               *
 ****************************************************************************/
typedef struct _MPI3_SAS_PHY_PAGE1
{
    MPI3_CONFIG_PAGE_HEADER         Header;                         /* 0x00 */
    U32                             Reserved08;                     /* 0x08 */
    U32                             InvalidDwordCount;              /* 0x0C */
    U32                             RunningDisparityErrorCount;     /* 0x10 */
    U32                             LossDwordSynchCount;            /* 0x14 */
    U32                             PhyResetProblemCount;           /* 0x18 */
} MPI3_SAS_PHY_PAGE1, MPI3_POINTER PTR_MPI3_SAS_PHY_PAGE1,
  Mpi3SasPhyPage1_t, MPI3_POINTER pMpi3SasPhyPage1_t;

/**** Defines for the PageVersion field ****/
#define MPI3_SASPHY1_PAGEVERSION                        (0x00)

/*****************************************************************************
 *              SAS PHY Page 2                                               *
 ****************************************************************************/
typedef struct _MPI3_SAS_PHY2_PHY_EVENT
{
    U8      PhyEventCode;       /* 0x00 */
    U8      Reserved01[3];      /* 0x01 */
    U32     PhyEventInfo;       /* 0x04 */
} MPI3_SAS_PHY2_PHY_EVENT, MPI3_POINTER PTR_MPI3_SAS_PHY2_PHY_EVENT,
  Mpi3SasPhy2PhyEvent_t, MPI3_POINTER pMpi3SasPhy2PhyEvent_t;

/**** Defines for the PhyEventCode field - use MPI3_SASPHY3_EVENT_CODE_ defines */

#ifndef MPI3_SAS_PHY2_PHY_EVENT_MAX
#define MPI3_SAS_PHY2_PHY_EVENT_MAX         (1)
#endif  /* MPI3_SAS_PHY2_PHY_EVENT_MAX */

typedef struct _MPI3_SAS_PHY_PAGE2
{
    MPI3_CONFIG_PAGE_HEADER         Header;                                     /* 0x00 */
    U32                             Reserved08;                                 /* 0x08 */
    U8                              NumPhyEvents;                               /* 0x0C */
    U8                              Reserved0D[3];                              /* 0x0D */
    MPI3_SAS_PHY2_PHY_EVENT         PhyEvent[MPI3_SAS_PHY2_PHY_EVENT_MAX];      /* 0x10 */
} MPI3_SAS_PHY_PAGE2, MPI3_POINTER PTR_MPI3_SAS_PHY_PAGE2,
  Mpi3SasPhyPage2_t, MPI3_POINTER pMpi3SasPhyPage2_t;

/**** Defines for the PageVersion field ****/
#define MPI3_SASPHY2_PAGEVERSION                        (0x00)

/*****************************************************************************
 *              SAS PHY Page 3                                               *
 ****************************************************************************/
typedef struct _MPI3_SAS_PHY3_PHY_EVENT_CONFIG
{
    U8      PhyEventCode;           /* 0x00 */
    U8      Reserved01[3];          /* 0x01 */
    U8      CounterType;            /* 0x04 */
    U8      ThresholdWindow;        /* 0x05 */
    U8      TimeUnits;              /* 0x06 */
    U8      Reserved07;             /* 0x07 */
    U32     EventThreshold;         /* 0x08 */
    U16     ThresholdFlags;         /* 0x0C */
    U16     Reserved0E;             /* 0x0E */
} MPI3_SAS_PHY3_PHY_EVENT_CONFIG, MPI3_POINTER PTR_MPI3_SAS_PHY3_PHY_EVENT_CONFIG,
  Mpi3SasPhy3PhyEventConfig_t, MPI3_POINTER pMpi3SasPhy3PhyEventConfig_t;

/**** Defines for the PhyEventCode field ****/
#define MPI3_SASPHY3_EVENT_CODE_NO_EVENT                    (0x00)
#define MPI3_SASPHY3_EVENT_CODE_INVALID_DWORD               (0x01)
#define MPI3_SASPHY3_EVENT_CODE_RUNNING_DISPARITY_ERROR     (0x02)
#define MPI3_SASPHY3_EVENT_CODE_LOSS_DWORD_SYNC             (0x03)
#define MPI3_SASPHY3_EVENT_CODE_PHY_RESET_PROBLEM           (0x04)
#define MPI3_SASPHY3_EVENT_CODE_ELASTICITY_BUF_OVERFLOW     (0x05)
#define MPI3_SASPHY3_EVENT_CODE_RX_ERROR                    (0x06)
#define MPI3_SASPHY3_EVENT_CODE_INV_SPL_PACKETS             (0x07)
#define MPI3_SASPHY3_EVENT_CODE_LOSS_SPL_PACKET_SYNC        (0x08)
#define MPI3_SASPHY3_EVENT_CODE_RX_ADDR_FRAME_ERROR         (0x20)
#define MPI3_SASPHY3_EVENT_CODE_TX_AC_OPEN_REJECT           (0x21)
#define MPI3_SASPHY3_EVENT_CODE_RX_AC_OPEN_REJECT           (0x22)
#define MPI3_SASPHY3_EVENT_CODE_TX_RC_OPEN_REJECT           (0x23)
#define MPI3_SASPHY3_EVENT_CODE_RX_RC_OPEN_REJECT           (0x24)
#define MPI3_SASPHY3_EVENT_CODE_RX_AIP_PARTIAL_WAITING_ON   (0x25)
#define MPI3_SASPHY3_EVENT_CODE_RX_AIP_CONNECT_WAITING_ON   (0x26)
#define MPI3_SASPHY3_EVENT_CODE_TX_BREAK                    (0x27)
#define MPI3_SASPHY3_EVENT_CODE_RX_BREAK                    (0x28)
#define MPI3_SASPHY3_EVENT_CODE_BREAK_TIMEOUT               (0x29)
#define MPI3_SASPHY3_EVENT_CODE_CONNECTION                  (0x2A)
#define MPI3_SASPHY3_EVENT_CODE_PEAKTX_PATHWAY_BLOCKED      (0x2B)
#define MPI3_SASPHY3_EVENT_CODE_PEAKTX_ARB_WAIT_TIME        (0x2C)
#define MPI3_SASPHY3_EVENT_CODE_PEAK_ARB_WAIT_TIME          (0x2D)
#define MPI3_SASPHY3_EVENT_CODE_PEAK_CONNECT_TIME           (0x2E)
#define MPI3_SASPHY3_EVENT_CODE_PERSIST_CONN                (0x2F)
#define MPI3_SASPHY3_EVENT_CODE_TX_SSP_FRAMES               (0x40)
#define MPI3_SASPHY3_EVENT_CODE_RX_SSP_FRAMES               (0x41)
#define MPI3_SASPHY3_EVENT_CODE_TX_SSP_ERROR_FRAMES         (0x42)
#define MPI3_SASPHY3_EVENT_CODE_RX_SSP_ERROR_FRAMES         (0x43)
#define MPI3_SASPHY3_EVENT_CODE_TX_CREDIT_BLOCKED           (0x44)
#define MPI3_SASPHY3_EVENT_CODE_RX_CREDIT_BLOCKED           (0x45)
#define MPI3_SASPHY3_EVENT_CODE_TX_SATA_FRAMES              (0x50)
#define MPI3_SASPHY3_EVENT_CODE_RX_SATA_FRAMES              (0x51)
#define MPI3_SASPHY3_EVENT_CODE_SATA_OVERFLOW               (0x52)
#define MPI3_SASPHY3_EVENT_CODE_TX_SMP_FRAMES               (0x60)
#define MPI3_SASPHY3_EVENT_CODE_RX_SMP_FRAMES               (0x61)
#define MPI3_SASPHY3_EVENT_CODE_RX_SMP_ERROR_FRAMES         (0x63)
#define MPI3_SASPHY3_EVENT_CODE_HOTPLUG_TIMEOUT             (0xD0)
#define MPI3_SASPHY3_EVENT_CODE_MISALIGNED_MUX_PRIMITIVE    (0xD1)
#define MPI3_SASPHY3_EVENT_CODE_RX_AIP                      (0xD2)
#define MPI3_SASPHY3_EVENT_CODE_LCARB_WAIT_TIME             (0xD3)
#define MPI3_SASPHY3_EVENT_CODE_RCVD_CONN_RESP_WAIT_TIME    (0xD4)
#define MPI3_SASPHY3_EVENT_CODE_LCCONN_TIME                 (0xD5)
#define MPI3_SASPHY3_EVENT_CODE_SSP_TX_START_TRANSMIT       (0xD6)
#define MPI3_SASPHY3_EVENT_CODE_SATA_TX_START               (0xD7)
#define MPI3_SASPHY3_EVENT_CODE_SMP_TX_START_TRANSMT        (0xD8)
#define MPI3_SASPHY3_EVENT_CODE_TX_SMP_BREAK_CONN           (0xD9)
#define MPI3_SASPHY3_EVENT_CODE_SSP_RX_START_RECEIVE        (0xDA)
#define MPI3_SASPHY3_EVENT_CODE_SATA_RX_START_RECEIVE       (0xDB)
#define MPI3_SASPHY3_EVENT_CODE_SMP_RX_START_RECEIVE        (0xDC)

/**** Defines for the CounterType field ****/
#define MPI3_SASPHY3_COUNTER_TYPE_WRAPPING                  (0x00)
#define MPI3_SASPHY3_COUNTER_TYPE_SATURATING                (0x01)
#define MPI3_SASPHY3_COUNTER_TYPE_PEAK_VALUE                (0x02)

/**** Defines for the TimeUnits field ****/
#define MPI3_SASPHY3_TIME_UNITS_10_MICROSECONDS             (0x00)
#define MPI3_SASPHY3_TIME_UNITS_100_MICROSECONDS            (0x01)
#define MPI3_SASPHY3_TIME_UNITS_1_MILLISECOND               (0x02)
#define MPI3_SASPHY3_TIME_UNITS_10_MILLISECONDS             (0x03)

/**** Defines for the ThresholdFlags field ****/
#define MPI3_SASPHY3_TFLAGS_PHY_RESET                       (0x0002)
#define MPI3_SASPHY3_TFLAGS_EVENT_NOTIFY                    (0x0001)

#ifndef MPI3_SAS_PHY3_PHY_EVENT_MAX
#define MPI3_SAS_PHY3_PHY_EVENT_MAX         (1)
#endif  /* MPI3_SAS_PHY3_PHY_EVENT_MAX */

typedef struct _MPI3_SAS_PHY_PAGE3
{
    MPI3_CONFIG_PAGE_HEADER         Header;                                         /* 0x00 */
    U32                             Reserved08;                                     /* 0x08 */
    U8                              NumPhyEvents;                                   /* 0x0C */
    U8                              Reserved0D[3];                                  /* 0x0D */
    MPI3_SAS_PHY3_PHY_EVENT_CONFIG  PhyEventConfig[MPI3_SAS_PHY3_PHY_EVENT_MAX];    /* 0x10 */
} MPI3_SAS_PHY_PAGE3, MPI3_POINTER PTR_MPI3_SAS_PHY_PAGE3,
  Mpi3SasPhyPage3_t, MPI3_POINTER pMpi3SasPhyPage3_t;

/**** Defines for the PageVersion field ****/
#define MPI3_SASPHY3_PAGEVERSION                        (0x00)

/*****************************************************************************
 *              SAS PHY Page 4                                               *
 ****************************************************************************/
typedef struct _MPI3_SAS_PHY_PAGE4
{
    MPI3_CONFIG_PAGE_HEADER         Header;             /* 0x00 */
    U8                              Reserved08[3];      /* 0x08 */
    U8                              Flags;              /* 0x0B */
    U8                              InitialFrame[28];   /* 0x0C */
} MPI3_SAS_PHY_PAGE4, MPI3_POINTER PTR_MPI3_SAS_PHY_PAGE4,
  Mpi3SasPhyPage4_t, MPI3_POINTER pMpi3SasPhyPage4_t;

/**** Defines for the PageVersion field ****/
#define MPI3_SASPHY4_PAGEVERSION                        (0x00)

/**** Defines for the Flags field ****/
#define MPI3_SASPHY4_FLAGS_FRAME_VALID                  (0x02)
#define MPI3_SASPHY4_FLAGS_SATA_FRAME                   (0x01)


/*****************************************************************************
 *              Common definitions used by PCIe Configuration Pages          *
 ****************************************************************************/

/**** Defines for NegotiatedLinkRates ****/
#define MPI3_PCIE_LINK_RETIMERS_MASK                    (0x30)
#define MPI3_PCIE_LINK_RETIMERS_SHIFT                   (4)
#define MPI3_PCIE_NEG_LINK_RATE_MASK                    (0x0F)
#define MPI3_PCIE_NEG_LINK_RATE_SHIFT                   (0)
#define MPI3_PCIE_NEG_LINK_RATE_UNKNOWN                 (0x00)
#define MPI3_PCIE_NEG_LINK_RATE_PHY_DISABLED            (0x01)
#define MPI3_PCIE_NEG_LINK_RATE_2_5                     (0x02)
#define MPI3_PCIE_NEG_LINK_RATE_5_0                     (0x03)
#define MPI3_PCIE_NEG_LINK_RATE_8_0                     (0x04)
#define MPI3_PCIE_NEG_LINK_RATE_16_0                    (0x05)
#define MPI3_PCIE_NEG_LINK_RATE_32_0                    (0x06)

/**** Defines for Enabled ASPM States ****/
#define MPI3_PCIE_ASPM_ENABLE_NONE                      (0x0)
#define MPI3_PCIE_ASPM_ENABLE_L0s                       (0x1)
#define MPI3_PCIE_ASPM_ENABLE_L1                        (0x2)
#define MPI3_PCIE_ASPM_ENABLE_L0s_L1                    (0x3)

/**** Defines for Enabled ASPM States ****/
#define MPI3_PCIE_ASPM_SUPPORT_NONE                     (0x0)
#define MPI3_PCIE_ASPM_SUPPORT_L0s                      (0x1)
#define MPI3_PCIE_ASPM_SUPPORT_L1                       (0x2)
#define MPI3_PCIE_ASPM_SUPPORT_L0s_L1                   (0x3)

/*****************************************************************************
 *              PCIe IO Unit Configuration Pages                             *
 ****************************************************************************/

/*****************************************************************************
 *              PCIe IO Unit Page 0                                          *
 ****************************************************************************/
typedef struct _MPI3_PCIE_IO_UNIT0_PHY_DATA
{
    U8      Link;                       /* 0x00 */
    U8      LinkFlags;                  /* 0x01 */
    U8      PhyFlags;                   /* 0x02 */
    U8      NegotiatedLinkRate;         /* 0x03 */
    U16     AttachedDevHandle;          /* 0x04 */
    U16     ControllerDevHandle;        /* 0x06 */
    U32     EnumerationStatus;          /* 0x08 */
    U8      IOUnitPort;                 /* 0x0C */
    U8      Reserved0D[3];              /* 0x0D */
} MPI3_PCIE_IO_UNIT0_PHY_DATA, MPI3_POINTER PTR_MPI3_PCIE_IO_UNIT0_PHY_DATA,
  Mpi3PcieIOUnit0PhyData_t, MPI3_POINTER pMpi3PcieIOUnit0PhyData_t;

/**** Defines for the LinkFlags field ****/
#define MPI3_PCIEIOUNIT0_LINKFLAGS_CONFIG_SOURCE_MASK      (0x10)
#define MPI3_PCIEIOUNIT0_LINKFLAGS_CONFIG_SOURCE_SHIFT     (4)
#define MPI3_PCIEIOUNIT0_LINKFLAGS_CONFIG_SOURCE_IOUNIT1   (0x00)
#define MPI3_PCIEIOUNIT0_LINKFLAGS_CONFIG_SOURCE_BKPLANE   (0x10)
#define MPI3_PCIEIOUNIT0_LINKFLAGS_ENUM_IN_PROGRESS        (0x08)

/**** Defines for the PhyFlags field ****/
#define MPI3_PCIEIOUNIT0_PHYFLAGS_PHY_DISABLED          (0x08)
#define MPI3_PCIEIOUNIT0_PHYFLAGS_HOST_PHY              (0x01)

/**** Defines for the NegotiatedLinkRate field - use MPI3_PCIE_NEG_LINK_RATE_ defines ****/

/**** Defines for the EnumerationStatus field ****/
#define MPI3_PCIEIOUNIT0_ES_MAX_SWITCH_DEPTH_EXCEEDED   (0x80000000)
#define MPI3_PCIEIOUNIT0_ES_MAX_SWITCHES_EXCEEDED       (0x40000000)
#define MPI3_PCIEIOUNIT0_ES_MAX_ENDPOINTS_EXCEEDED      (0x20000000)
#define MPI3_PCIEIOUNIT0_ES_INSUFFICIENT_RESOURCES      (0x10000000)

#ifndef MPI3_PCIE_IO_UNIT0_PHY_MAX
#define MPI3_PCIE_IO_UNIT0_PHY_MAX      (1)
#endif  /* MPI3_PCIE_IO_UNIT0_PHY_MAX */

typedef struct _MPI3_PCIE_IO_UNIT_PAGE0
{
    MPI3_CONFIG_PAGE_HEADER         Header;                                 /* 0x00 */
    U32                             Reserved08;                             /* 0x08 */
    U8                              NumPhys;                                /* 0x0C */
    U8                              InitStatus;                             /* 0x0D */
    U8                              ASPM;                                   /* 0x0E */
    U8                              Reserved0F;                             /* 0x0F */
    MPI3_PCIE_IO_UNIT0_PHY_DATA     PhyData[MPI3_PCIE_IO_UNIT0_PHY_MAX];    /* 0x10 */
} MPI3_PCIE_IO_UNIT_PAGE0, MPI3_POINTER PTR_MPI3_PCIE_IO_UNIT_PAGE0,
  Mpi3PcieIOUnitPage0_t, MPI3_POINTER pMpi3PcieIOUnitPage0_t;

/**** Defines for the PageVersion field ****/
#define MPI3_PCIEIOUNIT0_PAGEVERSION                        (0x00)

/**** Defines for the InitStatus field ****/
#define MPI3_PCIEIOUNIT0_INITSTATUS_NO_ERRORS               (0x00)
#define MPI3_PCIEIOUNIT0_INITSTATUS_NEEDS_INITIALIZATION    (0x01)
#define MPI3_PCIEIOUNIT0_INITSTATUS_NO_TARGETS_ALLOCATED    (0x02)
#define MPI3_PCIEIOUNIT0_INITSTATUS_RESOURCE_ALLOC_FAILED   (0x03)
#define MPI3_PCIEIOUNIT0_INITSTATUS_BAD_NUM_PHYS            (0x04)
#define MPI3_PCIEIOUNIT0_INITSTATUS_UNSUPPORTED_CONFIG      (0x05)
#define MPI3_PCIEIOUNIT0_INITSTATUS_HOST_PORT_MISMATCH      (0x06)
#define MPI3_PCIEIOUNIT0_INITSTATUS_PHYS_NOT_CONSECUTIVE    (0x07)
#define MPI3_PCIEIOUNIT0_INITSTATUS_BAD_CLOCKING_MODE       (0x08)
#define MPI3_PCIEIOUNIT0_INITSTATUS_PROD_SPEC_START         (0xF0)
#define MPI3_PCIEIOUNIT0_INITSTATUS_PROD_SPEC_END           (0xFF)

/**** Defines for the ASPM field ****/
#define MPI3_PCIEIOUNIT0_ASPM_SWITCH_STATES_MASK            (0xC0)
#define MPI3_PCIEIOUNIT0_ASPM_SWITCH_STATES_SHIFT              (6)
#define MPI3_PCIEIOUNIT0_ASPM_DIRECT_STATES_MASK            (0x30)
#define MPI3_PCIEIOUNIT0_ASPM_DIRECT_STATES_SHIFT              (4)
/*** use MPI3_PCIE_ASPM_ENABLE_  defines for field values ***/
#define MPI3_PCIEIOUNIT0_ASPM_SWITCH_SUPPORT_MASK           (0x0C)
#define MPI3_PCIEIOUNIT0_ASPM_SWITCH_SUPPORT_SHIFT             (2)
#define MPI3_PCIEIOUNIT0_ASPM_DIRECT_SUPPORT_MASK           (0x03)
#define MPI3_PCIEIOUNIT0_ASPM_DIRECT_SUPPORT_SHIFT             (0)
/*** use MPI3_PCIE_ASPM_SUPPORT_  defines for field values ***/

/*****************************************************************************
 *              PCIe IO Unit Page 1                                          *
 ****************************************************************************/
typedef struct _MPI3_PCIE_IO_UNIT1_PHY_DATA
{
    U8      Link;                       /* 0x00 */
    U8      LinkFlags;                  /* 0x01 */
    U8      PhyFlags;                   /* 0x02 */
    U8      MaxMinLinkRate;             /* 0x03 */
    U32     Reserved04;                 /* 0x04 */
    U32     Reserved08;                 /* 0x08 */
} MPI3_PCIE_IO_UNIT1_PHY_DATA, MPI3_POINTER PTR_MPI3_PCIE_IO_UNIT1_PHY_DATA,
  Mpi3PcieIOUnit1PhyData_t, MPI3_POINTER pMpi3PcieIOUnit1PhyData_t;

/**** Defines for the LinkFlags field ****/
#define MPI3_PCIEIOUNIT1_LINKFLAGS_PCIE_CLK_MODE_MASK                     (0x03)
#define MPI3_PCIEIOUNIT1_LINKFLAGS_PCIE_CLK_MODE_SHIFT                    (0)
#define MPI3_PCIEIOUNIT1_LINKFLAGS_PCIE_CLK_MODE_DIS_SEPARATE_REFCLK      (0x00)
#define MPI3_PCIEIOUNIT1_LINKFLAGS_PCIE_CLK_MODE_EN_SRIS                  (0x01)
#define MPI3_PCIEIOUNIT1_LINKFLAGS_PCIE_CLK_MODE_EN_SRNS                  (0x02)

/**** Defines for the PhyFlags field ****/
#define MPI3_PCIEIOUNIT1_PHYFLAGS_PHY_DISABLE                             (0x08)

/**** Defines for the MaxMinLinkRate ****/
#define MPI3_PCIEIOUNIT1_MMLR_MAX_RATE_MASK                               (0xF0)
#define MPI3_PCIEIOUNIT1_MMLR_MAX_RATE_SHIFT                                 (4)
#define MPI3_PCIEIOUNIT1_MMLR_MAX_RATE_2_5                                (0x20)
#define MPI3_PCIEIOUNIT1_MMLR_MAX_RATE_5_0                                (0x30)
#define MPI3_PCIEIOUNIT1_MMLR_MAX_RATE_8_0                                (0x40)
#define MPI3_PCIEIOUNIT1_MMLR_MAX_RATE_16_0                               (0x50)
#define MPI3_PCIEIOUNIT1_MMLR_MAX_RATE_32_0                               (0x60)

#ifndef MPI3_PCIE_IO_UNIT1_PHY_MAX
#define MPI3_PCIE_IO_UNIT1_PHY_MAX                                           (1)
#endif  /* MPI3_PCIE_IO_UNIT1_PHY_MAX */

typedef struct _MPI3_PCIE_IO_UNIT_PAGE1
{
    MPI3_CONFIG_PAGE_HEADER         Header;                                 /* 0x00 */
    U32                             ControlFlags;                           /* 0x08 */
    U32                             Reserved0C;                             /* 0x0C */
    U8                              NumPhys;                                /* 0x10 */
    U8                              Reserved11;                             /* 0x11 */
    U8                              ASPM;                                   /* 0x12 */
    U8                              Reserved13;                             /* 0x13 */
    MPI3_PCIE_IO_UNIT1_PHY_DATA     PhyData[MPI3_PCIE_IO_UNIT1_PHY_MAX];    /* 0x14 */
} MPI3_PCIE_IO_UNIT_PAGE1, MPI3_POINTER PTR_MPI3_PCIE_IO_UNIT_PAGE1,
  Mpi3PcieIOUnitPage1_t, MPI3_POINTER pMpi3PcieIOUnitPage1_t;

/**** Defines for the PageVersion field ****/
#define MPI3_PCIEIOUNIT1_PAGEVERSION                                           (0x00)

/**** Defines for the ControlFlags field ****/
#define MPI3_PCIEIOUNIT1_CONTROL_FLAGS_PERST_OVERRIDE_MASK                     (0xE0000000)
#define MPI3_PCIEIOUNIT1_CONTROL_FLAGS_PERST_OVERRIDE_SHIFT                    (29)
#define MPI3_PCIEIOUNIT1_CONTROL_FLAGS_PERST_OVERRIDE_NONE                     (0x00000000)
#define MPI3_PCIEIOUNIT1_CONTROL_FLAGS_PERST_OVERRIDE_DEASSERT                 (0x20000000)
#define MPI3_PCIEIOUNIT1_CONTROL_FLAGS_PERST_OVERRIDE_ASSERT                   (0x40000000)
#define MPI3_PCIEIOUNIT1_CONTROL_FLAGS_PERST_OVERRIDE_BACKPLANE_ERROR          (0x60000000)
#define MPI3_PCIEIOUNIT1_CONTROL_FLAGS_REFCLK_OVERRIDE_MASK                    (0x1C000000)
#define MPI3_PCIEIOUNIT1_CONTROL_FLAGS_REFCLK_OVERRIDE_SHIFT                   (26)
#define MPI3_PCIEIOUNIT1_CONTROL_FLAGS_REFCLK_OVERRIDE_NONE                    (0x00000000)
#define MPI3_PCIEIOUNIT1_CONTROL_FLAGS_REFCLK_OVERRIDE_ENABLE                  (0x04000000)
#define MPI3_PCIEIOUNIT1_CONTROL_FLAGS_REFCLK_OVERRIDE_DISABLE                 (0x08000000)
#define MPI3_PCIEIOUNIT1_CONTROL_FLAGS_REFCLK_OVERRIDE_BACKPLANE_ERROR         (0x0C000000)
#define MPI3_PCIEIOUNIT1_CONTROL_FLAGS_PARTIAL_CAPACITY_ENABLE                 (0x00000100)
#define MPI3_PCIEIOUNIT1_CONTROL_FLAGS_LINK_OVERRIDE_DISABLE                   (0x00000080)
#define MPI3_PCIEIOUNIT1_CONTROL_FLAGS_CLOCK_OVERRIDE_DISABLE                  (0x00000040)
#define MPI3_PCIEIOUNIT1_CONTROL_FLAGS_CLOCK_OVERRIDE_MODE_MASK                (0x00000030)
#define MPI3_PCIEIOUNIT1_CONTROL_FLAGS_CLOCK_OVERRIDE_MODE_SHIFT               (4)
#define MPI3_PCIEIOUNIT1_CONTROL_FLAGS_CLOCK_OVERRIDE_MODE_SRIS_SRNS_DISABLED  (0x00000000)
#define MPI3_PCIEIOUNIT1_CONTROL_FLAGS_CLOCK_OVERRIDE_MODE_SRIS_ENABLED        (0x00000010)
#define MPI3_PCIEIOUNIT1_CONTROL_FLAGS_CLOCK_OVERRIDE_MODE_SRNS_ENABLED        (0x00000020)
#define MPI3_PCIEIOUNIT1_CONTROL_FLAGS_LINK_RATE_OVERRIDE_MASK                 (0x0000000F)
#define MPI3_PCIEIOUNIT1_CONTROL_FLAGS_LINK_RATE_OVERRIDE_SHIFT                (0)
#define MPI3_PCIEIOUNIT1_CONTROL_FLAGS_LINK_RATE_OVERRIDE_USE_BACKPLANE        (0x00000000)
#define MPI3_PCIEIOUNIT1_CONTROL_FLAGS_LINK_RATE_OVERRIDE_MAX_2_5              (0x00000002)
#define MPI3_PCIEIOUNIT1_CONTROL_FLAGS_LINK_RATE_OVERRIDE_MAX_5_0              (0x00000003)
#define MPI3_PCIEIOUNIT1_CONTROL_FLAGS_LINK_RATE_OVERRIDE_MAX_8_0              (0x00000004)
#define MPI3_PCIEIOUNIT1_CONTROL_FLAGS_LINK_RATE_OVERRIDE_MAX_16_0             (0x00000005)
#define MPI3_PCIEIOUNIT1_CONTROL_FLAGS_LINK_RATE_OVERRIDE_MAX_32_0             (0x00000006)

/**** Defines for the ASPM field ****/
#define MPI3_PCIEIOUNIT1_ASPM_SWITCH_MASK                                 (0x0C)
#define MPI3_PCIEIOUNIT1_ASPM_SWITCH_SHIFT                                   (2)
#define MPI3_PCIEIOUNIT1_ASPM_DIRECT_MASK                                 (0x03)
#define MPI3_PCIEIOUNIT1_ASPM_DIRECT_SHIFT                                   (0)
/*** use MPI3_PCIE_ASPM_ENABLE_  defines for ASPM field values ***/

/*****************************************************************************
 *              PCIe IO Unit Page 2                                          *
 ****************************************************************************/
typedef struct _MPI3_PCIE_IO_UNIT_PAGE2
{
    MPI3_CONFIG_PAGE_HEADER         Header;                                 /* 0x00 */
    U16                             NVMeMaxQDx1;                            /* 0x08 */
    U16                             NVMeMaxQDx2;                            /* 0x0A */
    U8                              NVMeAbortTO;                            /* 0x0C */
    U8                              Reserved0D;                             /* 0x0D */
    U16                             NVMeMaxQDx4;                            /* 0x0E */
} MPI3_PCIE_IO_UNIT_PAGE2, MPI3_POINTER PTR_MPI3_PCIE_IO_UNIT_PAGE2,
  Mpi3PcieIOUnitPage2_t, MPI3_POINTER pMpi3PcieIOUnitPage2_t;

/**** Defines for the PageVersion field ****/
#define MPI3_PCIEIOUNIT2_PAGEVERSION                        (0x00)

/*****************************************************************************
 *              PCIe IO Unit Page 3                                          *
 ****************************************************************************/

/**** Defines for Error Indexes ****/
#define MPI3_PCIEIOUNIT3_ERROR_RECEIVER_ERROR               (0)
#define MPI3_PCIEIOUNIT3_ERROR_RECOVERY                     (1)
#define MPI3_PCIEIOUNIT3_ERROR_CORRECTABLE_ERROR_MSG        (2)
#define MPI3_PCIEIOUNIT3_ERROR_BAD_DLLP                     (3)
#define MPI3_PCIEIOUNIT3_ERROR_BAD_TLP                      (4)
#define MPI3_PCIEIOUNIT3_NUM_ERROR_INDEX                    (5)


typedef struct _MPI3_PCIE_IO_UNIT3_ERROR
{
    U16                             ThresholdCount;                         /* 0x00 */
    U16                             Reserved02;                             /* 0x02 */
} MPI3_PCIE_IO_UNIT3_ERROR, MPI3_POINTER PTR_MPI3_PCIE_IO_UNIT3_ERROR,
  Mpi3PcieIOUnit3Error_t, MPI3_POINTER pMpi3PcieIOUnit3Error_t;

typedef struct _MPI3_PCIE_IO_UNIT_PAGE3
{
    MPI3_CONFIG_PAGE_HEADER         Header;                                   /* 0x00 */
    U8                              ThresholdWindow;                          /* 0x08 */
    U8                              ThresholdAction;                          /* 0x09 */
    U8                              EscalationCount;                          /* 0x0A */
    U8                              EscalationAction;                         /* 0x0B */
    U8                              NumErrors;                                /* 0x0C */
    U8                              Reserved0D[3];                            /* 0x0D */
    MPI3_PCIE_IO_UNIT3_ERROR        Error[MPI3_PCIEIOUNIT3_NUM_ERROR_INDEX];  /* 0x10 */
} MPI3_PCIE_IO_UNIT_PAGE3, MPI3_POINTER PTR_MPI3_PCIE_IO_UNIT_PAGE3,
  Mpi3PcieIOUnitPage3_t, MPI3_POINTER pMpi3PcieIOUnitPage3_t;

/**** Defines for the PageVersion field ****/
#define MPI3_PCIEIOUNIT3_PAGEVERSION                        (0x00)

/**** Defines for the ThresholdAction and EscalationAction fields ****/
#define MPI3_PCIEIOUNIT3_ACTION_NO_ACTION                   (0x00)
#define MPI3_PCIEIOUNIT3_ACTION_HOT_RESET                   (0x01)
#define MPI3_PCIEIOUNIT3_ACTION_REDUCE_LINK_RATE_ONLY       (0x02)
#define MPI3_PCIEIOUNIT3_ACTION_REDUCE_LINK_RATE_NO_ACCESS  (0x03)

/**** Defines for Error Indexes - use MPI3_PCIEIOUNIT3_ERROR_ defines ****/

/*****************************************************************************
 *              PCIe Switch Configuration Pages                              *
 ****************************************************************************/

/*****************************************************************************
 *              PCIe Switch Page 0                                           *
 ****************************************************************************/
typedef struct _MPI3_PCIE_SWITCH_PAGE0
{
    MPI3_CONFIG_PAGE_HEADER     Header;             /* 0x00 */
    U8                          IOUnitPort;         /* 0x08 */
    U8                          SwitchStatus;       /* 0x09 */
    U8                          Reserved0A[2];      /* 0x0A */
    U16                         DevHandle;          /* 0x0C */
    U16                         ParentDevHandle;    /* 0x0E */
    U8                          NumPorts;           /* 0x10 */
    U8                          PCIeLevel;          /* 0x11 */
    U16                         Reserved12;         /* 0x12 */
    U32                         Reserved14;         /* 0x14 */
    U32                         Reserved18;         /* 0x18 */
    U32                         Reserved1C;         /* 0x1C */
} MPI3_PCIE_SWITCH_PAGE0, MPI3_POINTER PTR_MPI3_PCIE_SWITCH_PAGE0,
  Mpi3PcieSwitchPage0_t, MPI3_POINTER pMpi3PcieSwitchPage0_t;

/**** Defines for the PageVersion field ****/
#define MPI3_PCIESWITCH0_PAGEVERSION                  (0x00)

/**** Defines for the SwitchStatus field ****/
#define MPI3_PCIESWITCH0_SS_NOT_RESPONDING            (0x02)
#define MPI3_PCIESWITCH0_SS_RESPONDING                (0x03)
#define MPI3_PCIESWITCH0_SS_DELAY_NOT_RESPONDING      (0x04)

/*****************************************************************************
 *              PCIe Switch Page 1                                           *
 ****************************************************************************/
typedef struct _MPI3_PCIE_SWITCH_PAGE1
{
    MPI3_CONFIG_PAGE_HEADER     Header;                 /* 0x00 */
    U8                          IOUnitPort;             /* 0x08 */
    U8                          Flags;                  /* 0x09 */
    U16                         Reserved0A;             /* 0x0A */
    U8                          NumPorts;               /* 0x0C */
    U8                          PortNum;                /* 0x0D */
    U16                         AttachedDevHandle;      /* 0x0E */
    U16                         SwitchDevHandle;        /* 0x10 */
    U8                          NegotiatedPortWidth;    /* 0x12 */
    U8                          NegotiatedLinkRate;     /* 0x13 */
    U16                         Slot;                   /* 0x14 */
    U16                         SlotIndex;              /* 0x16 */
    U32                         Reserved18;             /* 0x18 */
} MPI3_PCIE_SWITCH_PAGE1, MPI3_POINTER PTR_MPI3_PCIE_SWITCH_PAGE1,
  Mpi3PcieSwitchPage1_t, MPI3_POINTER pMpi3PcieSwitchPage1_t;

/**** Defines for the PageVersion field ****/
#define MPI3_PCIESWITCH1_PAGEVERSION        (0x00)

/**** Defines for the Flags field ****/
#define MPI3_PCIESWITCH1_FLAGS_ASPMSTATE_MASK     (0x0C)
#define MPI3_PCIESWITCH1_FLAGS_ASPMSTATE_SHIFT    (2)

/*** use MPI3_PCIE_ASPM_ENABLE_ defines for ASPMState field values ***/
#define MPI3_PCIESWITCH1_FLAGS_ASPMSUPPORT_MASK     (0x03)
#define MPI3_PCIESWITCH1_FLAGS_ASPMSUPPORT_SHIFT    (0)

/*** use MPI3_PCIE_ASPM_SUPPORT_ defines for ASPMSupport field values ***/

/**** Defines for the NegotiatedLinkRate field - use MPI3_PCIE_NEG_LINK_RATE_ defines ****/

/**** Defines for the Slot field - use MPI3_SLOT_ defines ****/

/**** Defines for the SlotIndex field - use MPI3_SLOT_INDEX_ ****/

/*****************************************************************************
 *              PCIe Switch Page 2                                           *
 ****************************************************************************/
#ifndef MPI3_PCIESWITCH2_MAX_NUM_PORTS
#define MPI3_PCIESWITCH2_MAX_NUM_PORTS                               (1)
#endif  /* MPI3_PCIESWITCH2_MAX_NUM_PORTS */

typedef struct _MPI3_PCIESWITCH2_PORT_ELEMENT
{
    U16                             LinkChangeCount;                       /* 0x00 */
    U16                             RateChangeCount;                       /* 0x02 */
    U32                             Reserved04;                            /* 0x04 */
} MPI3_PCIESWITCH2_PORT_ELEMENT, MPI3_POINTER PTR_MPI3_PCIESWITCH2_PORT_ELEMENT,
  Mpi3PcieSwitch2PortElement_t, MPI3_POINTER pMpi3PcieSwitch2PortElement_t;

typedef struct _MPI3_PCIE_SWITCH_PAGE2
{
    MPI3_CONFIG_PAGE_HEADER         Header;                                  /* 0x00 */
    U8                              NumPorts;                                /* 0x08 */
    U8                              Reserved09;                              /* 0x09 */
    U16                             DevHandle;                               /* 0x0A */
    U32                             Reserved0C;                              /* 0x0C */
    MPI3_PCIESWITCH2_PORT_ELEMENT   Port[MPI3_PCIESWITCH2_MAX_NUM_PORTS];    /* 0x10 */    /* variable length */
} MPI3_PCIE_SWITCH_PAGE2, MPI3_POINTER PTR_MPI3_PCIE_SWITCH_PAGE2,
  Mpi3PcieSwitchPage2_t, MPI3_POINTER pMpi3PcieSwitchPage2_t;

/**** Defines for the PageVersion field ****/
#define MPI3_PCIESWITCH2_PAGEVERSION        (0x00)

/*****************************************************************************
 *              PCIe Link Configuration Pages                                *
 ****************************************************************************/

/*****************************************************************************
 *              PCIe Link Page 0                                             *
 ****************************************************************************/
typedef struct _MPI3_PCIE_LINK_PAGE0
{
    MPI3_CONFIG_PAGE_HEADER     Header;                 /* 0x00 */
    U8                          Link;                   /* 0x08 */
    U8                          Reserved09[3];          /* 0x09 */
    U32                         Reserved0C;             /* 0x0C */
    U32                         ReceiverErrorCount;     /* 0x10 */
    U32                         RecoveryCount;          /* 0x14 */
    U32                         CorrErrorMsgCount;      /* 0x18 */
    U32                         NonFatalErrorMsgCount;  /* 0x1C */
    U32                         FatalErrorMsgCount;     /* 0x20 */
    U32                         NonFatalErrorCount;     /* 0x24 */
    U32                         FatalErrorCount;        /* 0x28 */
    U32                         BadDLLPCount;           /* 0x2C */
    U32                         BadTLPCount;            /* 0x30 */
} MPI3_PCIE_LINK_PAGE0, MPI3_POINTER PTR_MPI3_PCIE_LINK_PAGE0,
  Mpi3PcieLinkPage0_t, MPI3_POINTER pMpi3PcieLinkPage0_t;

/**** Defines for the PageVersion field ****/
#define MPI3_PCIELINK0_PAGEVERSION          (0x00)


/*****************************************************************************
 *              Enclosure Configuration Pages                                *
 ****************************************************************************/

/*****************************************************************************
 *              Enclosure Page 0                                             *
 ****************************************************************************/
typedef struct _MPI3_ENCLOSURE_PAGE0
{
    MPI3_CONFIG_PAGE_HEADER         Header;                 /* 0x00 */
    U64                             EnclosureLogicalID;     /* 0x08 */
    U16                             Flags;                  /* 0x10 */
    U16                             EnclosureHandle;        /* 0x12 */
    U16                             NumSlots;               /* 0x14 */
    U16                             Reserved16;             /* 0x16 */
    U8                              IOUnitPort;             /* 0x18 */
    U8                              EnclosureLevel;         /* 0x19 */
    U16                             SEPDevHandle;           /* 0x1A */
    U8                              ChassisSlot;            /* 0x1C */
    U8                              Reserved1D[3];          /* 0x1D */
    U32                             ReceptacleIDs;          /* 0x20 */
    U32                             Reserved24;             /* 0x24 */
} MPI3_ENCLOSURE_PAGE0, MPI3_POINTER PTR_MPI3_ENCLOSURE_PAGE0,
  Mpi3EnclosurePage0_t, MPI3_POINTER pMpi3EnclosurePage0_t;

/**** Defines for the PageVersion field ****/
#define MPI3_ENCLOSURE0_PAGEVERSION                     (0x00)

/**** Defines for the Flags field ****/
#define MPI3_ENCLS0_FLAGS_ENCL_TYPE_MASK                (0xC000)
#define MPI3_ENCLS0_FLAGS_ENCL_TYPE_SHIFT               (0xC000)
#define MPI3_ENCLS0_FLAGS_ENCL_TYPE_VIRTUAL             (0x0000)
#define MPI3_ENCLS0_FLAGS_ENCL_TYPE_SAS                 (0x4000)
#define MPI3_ENCLS0_FLAGS_ENCL_TYPE_PCIE                (0x8000)
#define MPI3_ENCLS0_FLAGS_CHASSIS_SLOT_VALID            (0x0020)
#define MPI3_ENCLS0_FLAGS_ENCL_DEV_PRESENT_MASK         (0x0010)
#define MPI3_ENCLS0_FLAGS_ENCL_DEV_PRESENT_SHIFT        (4)
#define MPI3_ENCLS0_FLAGS_ENCL_DEV_NOT_FOUND            (0x0000)
#define MPI3_ENCLS0_FLAGS_ENCL_DEV_PRESENT              (0x0010)
#define MPI3_ENCLS0_FLAGS_MNG_MASK                      (0x000F)
#define MPI3_ENCLS0_FLAGS_MNG_SHIFT                     (0)
#define MPI3_ENCLS0_FLAGS_MNG_UNKNOWN                   (0x0000)
#define MPI3_ENCLS0_FLAGS_MNG_IOC_SES                   (0x0001)
#define MPI3_ENCLS0_FLAGS_MNG_SES_ENCLOSURE             (0x0002)

/**** Defines for the ReceptacleIDs field ****/
#define MPI3_ENCLS0_RECEPTACLEIDS_NOT_REPORTED          (0x00000000)

/*****************************************************************************
 *              Device Configuration Pages                                   *
 ****************************************************************************/

/*****************************************************************************
 *              Common definitions used by Device Configuration Pages           *
 ****************************************************************************/

/**** Defines for the DeviceForm field ****/
#define MPI3_DEVICE_DEVFORM_SAS_SATA                    (0x00)
#define MPI3_DEVICE_DEVFORM_PCIE                        (0x01)
#define MPI3_DEVICE_DEVFORM_VD                          (0x02)

/*****************************************************************************
 *              Device Page 0                                                *
 ****************************************************************************/
typedef struct _MPI3_DEVICE0_SAS_SATA_FORMAT
{
    U64     SASAddress;                 /* 0x00 */
    U16     Flags;                      /* 0x08 */
    U16     DeviceInfo;                 /* 0x0A */
    U8      PhyNum;                     /* 0x0C */
    U8      AttachedPhyIdentifier;      /* 0x0D */
    U8      MaxPortConnections;         /* 0x0E */
    U8      ZoneGroup;                  /* 0x0F */
} MPI3_DEVICE0_SAS_SATA_FORMAT, MPI3_POINTER PTR_MPI3_DEVICE0_SAS_SATA_FORMAT,
  Mpi3Device0SasSataFormat_t, MPI3_POINTER pMpi3Device0SasSataFormat_t;

/**** Defines for the Flags field ****/
#define MPI3_DEVICE0_SASSATA_FLAGS_WRITE_SAME_UNMAP_NCQ (0x0400)
#define MPI3_DEVICE0_SASSATA_FLAGS_SLUMBER_CAP          (0x0200)
#define MPI3_DEVICE0_SASSATA_FLAGS_PARTIAL_CAP          (0x0100)
#define MPI3_DEVICE0_SASSATA_FLAGS_ASYNC_NOTIFY         (0x0080)
#define MPI3_DEVICE0_SASSATA_FLAGS_SW_PRESERVE          (0x0040)
#define MPI3_DEVICE0_SASSATA_FLAGS_UNSUPP_DEV           (0x0020)
#define MPI3_DEVICE0_SASSATA_FLAGS_48BIT_LBA            (0x0010)
#define MPI3_DEVICE0_SASSATA_FLAGS_SMART_SUPP           (0x0008)
#define MPI3_DEVICE0_SASSATA_FLAGS_NCQ_SUPP             (0x0004)
#define MPI3_DEVICE0_SASSATA_FLAGS_FUA_SUPP             (0x0002)
#define MPI3_DEVICE0_SASSATA_FLAGS_PERSIST_CAP          (0x0001)

/**** Defines for the DeviceInfo field - use MPI3_SAS_DEVICE_INFO_ defines (see mpi30_sas.h) ****/

typedef struct _MPI3_DEVICE0_PCIE_FORMAT
{
    U8      SupportedLinkRates;         /* 0x00 */
    U8      MaxPortWidth;               /* 0x01 */
    U8      NegotiatedPortWidth;        /* 0x02 */
    U8      NegotiatedLinkRate;         /* 0x03 */
    U8      PortNum;                    /* 0x04 */
    U8      ControllerResetTO;          /* 0x05 */
    U16     DeviceInfo;                 /* 0x06 */
    U32     MaximumDataTransferSize;    /* 0x08 */
    U32     Capabilities;               /* 0x0C */
    U16     NOIOB;                      /* 0x10 */
    U8      NVMeAbortTO;                /* 0x12 */
    U8      PageSize;                   /* 0x13 */
    U16     ShutdownLatency;            /* 0x14 */
    U8      RecoveryInfo;               /* 0x16 */
    U8      Reserved17;                 /* 0x17 */
} MPI3_DEVICE0_PCIE_FORMAT, MPI3_POINTER PTR_MPI3_DEVICE0_PCIE_FORMAT,
  Mpi3Device0PcieFormat_t, MPI3_POINTER pMpi3Device0PcieFormat_t;

/**** Defines for the SupportedLinkRates field ****/
#define MPI3_DEVICE0_PCIE_LINK_RATE_32_0_SUPP           (0x10)
#define MPI3_DEVICE0_PCIE_LINK_RATE_16_0_SUPP           (0x08)
#define MPI3_DEVICE0_PCIE_LINK_RATE_8_0_SUPP            (0x04)
#define MPI3_DEVICE0_PCIE_LINK_RATE_5_0_SUPP            (0x02)
#define MPI3_DEVICE0_PCIE_LINK_RATE_2_5_SUPP            (0x01)

/**** Defines for the NegotiatedLinkRate field - use MPI3_PCIE_NEG_LINK_RATE_ defines ****/

/**** Defines for DeviceInfo bitfield ****/
#define MPI3_DEVICE0_PCIE_DEVICE_INFO_TYPE_MASK             (0x0007)
#define MPI3_DEVICE0_PCIE_DEVICE_INFO_TYPE_SHIFT            (0)
#define MPI3_DEVICE0_PCIE_DEVICE_INFO_TYPE_NO_DEVICE        (0x0000)
#define MPI3_DEVICE0_PCIE_DEVICE_INFO_TYPE_NVME_DEVICE      (0x0001)
#define MPI3_DEVICE0_PCIE_DEVICE_INFO_TYPE_SWITCH_DEVICE    (0x0002)
#define MPI3_DEVICE0_PCIE_DEVICE_INFO_TYPE_SCSI_DEVICE      (0x0003)
#define MPI3_DEVICE0_PCIE_DEVICE_INFO_ASPM_MASK             (0x0030)
#define MPI3_DEVICE0_PCIE_DEVICE_INFO_ASPM_SHIFT            (4)
/*** use MPI3_PCIE_ASPM_ENABLE_  defines for ASPM field values ***/
#define MPI3_DEVICE0_PCIE_DEVICE_INFO_PITYPE_MASK           (0x00C0)
#define MPI3_DEVICE0_PCIE_DEVICE_INFO_PITYPE_SHIFT          (6)
#define MPI3_DEVICE0_PCIE_DEVICE_INFO_PITYPE_0              (0x0000)
#define MPI3_DEVICE0_PCIE_DEVICE_INFO_PITYPE_1              (0x0040)
#define MPI3_DEVICE0_PCIE_DEVICE_INFO_PITYPE_2              (0x0080)
#define MPI3_DEVICE0_PCIE_DEVICE_INFO_PITYPE_3              (0x00C0)


/**** Defines for the Capabilities field ****/
#define MPI3_DEVICE0_PCIE_CAP_SGL_EXTRA_LENGTH_SUPPORTED    (0x00000020)
#define MPI3_DEVICE0_PCIE_CAP_METADATA_SEPARATED            (0x00000010)
#define MPI3_DEVICE0_PCIE_CAP_SGL_DWORD_ALIGN_REQUIRED      (0x00000008)
#define MPI3_DEVICE0_PCIE_CAP_SGL_FORMAT_SGL                (0x00000004)
#define MPI3_DEVICE0_PCIE_CAP_SGL_FORMAT_PRP                (0x00000000)
#define MPI3_DEVICE0_PCIE_CAP_BIT_BUCKET_SGL_SUPP           (0x00000002)
#define MPI3_DEVICE0_PCIE_CAP_SGL_SUPP                      (0x00000001)
#define MPI3_DEVICE0_PCIE_CAP_ASPM_MASK                     (0x000000C0)
#define MPI3_DEVICE0_PCIE_CAP_ASPM_SHIFT                    (6)
/*** use MPI3_PCIE_ASPM_SUPPORT_  defines for ASPM field values ***/

/**** Defines for the RecoveryInfo field ****/
#define MPI3_DEVICE0_PCIE_RECOVER_METHOD_MASK               (0xE0)
#define MPI3_DEVICE0_PCIE_RECOVER_METHOD_SHIFT              (5)
#define MPI3_DEVICE0_PCIE_RECOVER_METHOD_NS_MGMT            (0x00)
#define MPI3_DEVICE0_PCIE_RECOVER_METHOD_FORMAT             (0x20)
#define MPI3_DEVICE0_PCIE_RECOVER_REASON_MASK               (0x1F)
#define MPI3_DEVICE0_PCIE_RECOVER_REASON_SHIFT              (0)
#define MPI3_DEVICE0_PCIE_RECOVER_REASON_NO_NS              (0x00)
#define MPI3_DEVICE0_PCIE_RECOVER_REASON_NO_NSID_1          (0x01)
#define MPI3_DEVICE0_PCIE_RECOVER_REASON_TOO_MANY_NS        (0x02)
#define MPI3_DEVICE0_PCIE_RECOVER_REASON_PROTECTION         (0x03)
#define MPI3_DEVICE0_PCIE_RECOVER_REASON_METADATA_SZ        (0x04)
#define MPI3_DEVICE0_PCIE_RECOVER_REASON_LBA_DATA_SZ        (0x05)
#define MPI3_DEVICE0_PCIE_RECOVER_REASON_PARTIAL_CAP        (0x06)

typedef struct _MPI3_DEVICE0_VD_FORMAT
{
    U8      VdState;              /* 0x00 */
    U8      RAIDLevel;            /* 0x01 */
    U16     DeviceInfo;           /* 0x02 */
    U16     Flags;                /* 0x04 */
    U16     IOThrottleGroup;      /* 0x06 */
    U16     IOThrottleGroupLow;   /* 0x08 */
    U16     IOThrottleGroupHigh;  /* 0x0A */
    U32     Reserved0C;           /* 0x0C */
} MPI3_DEVICE0_VD_FORMAT, MPI3_POINTER PTR_MPI3_DEVICE0_VD_FORMAT,
  Mpi3Device0VdFormat_t, MPI3_POINTER pMpi3Device0VdFormat_t;

/**** Defines for the VdState field ****/
#define MPI3_DEVICE0_VD_STATE_OFFLINE                       (0x00)
#define MPI3_DEVICE0_VD_STATE_PARTIALLY_DEGRADED            (0x01)
#define MPI3_DEVICE0_VD_STATE_DEGRADED                      (0x02)
#define MPI3_DEVICE0_VD_STATE_OPTIMAL                       (0x03)

/**** Defines for RAIDLevel field ****/
#define MPI3_DEVICE0_VD_RAIDLEVEL_RAID_0                    (0)
#define MPI3_DEVICE0_VD_RAIDLEVEL_RAID_1                    (1)
#define MPI3_DEVICE0_VD_RAIDLEVEL_RAID_5                    (5)
#define MPI3_DEVICE0_VD_RAIDLEVEL_RAID_6                    (6)
#define MPI3_DEVICE0_VD_RAIDLEVEL_RAID_10                   (10)
#define MPI3_DEVICE0_VD_RAIDLEVEL_RAID_50                   (50)
#define MPI3_DEVICE0_VD_RAIDLEVEL_RAID_60                   (60)

/**** Defines for DeviceInfo field ****/
#define MPI3_DEVICE0_VD_DEVICE_INFO_HDD                     (0x0010)
#define MPI3_DEVICE0_VD_DEVICE_INFO_SSD                     (0x0008)
#define MPI3_DEVICE0_VD_DEVICE_INFO_NVME                    (0x0004)
#define MPI3_DEVICE0_VD_DEVICE_INFO_SATA                    (0x0002)
#define MPI3_DEVICE0_VD_DEVICE_INFO_SAS                     (0x0001)

/**** Defines for the Flags field ****/
#define MPI3_DEVICE0_VD_FLAGS_IO_THROTTLE_GROUP_QD_MASK     (0xF000)
#define MPI3_DEVICE0_VD_FLAGS_IO_THROTTLE_GROUP_QD_SHIFT    (12)
#define MPI3_DEVICE0_VD_FLAGS_OSEXPOSURE_MASK               (0x0003)
#define MPI3_DEVICE0_VD_FLAGS_OSEXPOSURE_SHIFT              (0)
#define MPI3_DEVICE0_VD_FLAGS_OSEXPOSURE_HDD                (0x0000)
#define MPI3_DEVICE0_VD_FLAGS_OSEXPOSURE_SSD                (0x0001)
#define MPI3_DEVICE0_VD_FLAGS_OSEXPOSURE_NO_GUIDANCE        (0x0002)

typedef union _MPI3_DEVICE0_DEV_SPEC_FORMAT
{
    MPI3_DEVICE0_SAS_SATA_FORMAT        SasSataFormat;
    MPI3_DEVICE0_PCIE_FORMAT            PcieFormat;
    MPI3_DEVICE0_VD_FORMAT              VdFormat;
} MPI3_DEVICE0_DEV_SPEC_FORMAT, MPI3_POINTER PTR_MPI3_DEVICE0_DEV_SPEC_FORMAT,
  Mpi3Device0DevSpecFormat_t, MPI3_POINTER pMpi3Device0DevSpecFormat_t;

typedef struct _MPI3_DEVICE_PAGE0
{
    MPI3_CONFIG_PAGE_HEADER         Header;                 /* 0x00 */
    U16                             DevHandle;              /* 0x08 */
    U16                             ParentDevHandle;        /* 0x0A */
    U16                             Slot;                   /* 0x0C */
    U16                             EnclosureHandle;        /* 0x0E */
    U64                             WWID;                   /* 0x10 */
    U16                             PersistentID;           /* 0x18 */
    U8                              IOUnitPort;             /* 0x1A */
    U8                              AccessStatus;           /* 0x1B */
    U16                             Flags;                  /* 0x1C */
    U16                             Reserved1E;             /* 0x1E */
    U16                             SlotIndex;              /* 0x20 */
    U16                             QueueDepth;             /* 0x22 */
    U8                              Reserved24[3];          /* 0x24 */
    U8                              DeviceForm;             /* 0x27 */
    MPI3_DEVICE0_DEV_SPEC_FORMAT    DeviceSpecific;         /* 0x28 */
} MPI3_DEVICE_PAGE0, MPI3_POINTER PTR_MPI3_DEVICE_PAGE0,
  Mpi3DevicePage0_t, MPI3_POINTER pMpi3DevicePage0_t;

/**** Defines for the PageVersion field ****/
#define MPI3_DEVICE0_PAGEVERSION                        (0x00)

/**** Defines for the ParentDevHandle field ****/
#define MPI3_DEVICE0_PARENT_INVALID                     (0xFFFF)

/**** Defines for the Slot field - use MPI3_SLOT_ defines ****/

/**** Defines for the EnclosureHandle field ****/
#define MPI3_DEVICE0_ENCLOSURE_HANDLE_NO_ENCLOSURE      (0x0000)

/**** Defines for the WWID field ****/
#define MPI3_DEVICE0_WWID_INVALID                       (0xFFFFFFFFFFFFFFFF)

/**** Defines for the PersistentID field ****/
#define MPI3_DEVICE0_PERSISTENTID_INVALID               (0xFFFF)

/**** Defines for the IOUnitPort field ****/
#define MPI3_DEVICE0_IOUNITPORT_INVALID                 (0xFF)

/**** Defines for the AccessStatus field ****/
/* Generic Access Status Codes  */
#define MPI3_DEVICE0_ASTATUS_NO_ERRORS                              (0x00)
#define MPI3_DEVICE0_ASTATUS_NEEDS_INITIALIZATION                   (0x01)
#define MPI3_DEVICE0_ASTATUS_CAP_UNSUPPORTED                        (0x02)
#define MPI3_DEVICE0_ASTATUS_DEVICE_BLOCKED                         (0x03)
#define MPI3_DEVICE0_ASTATUS_UNAUTHORIZED                           (0x04)
#define MPI3_DEVICE0_ASTATUS_DEVICE_MISSING_DELAY                   (0x05)
#define MPI3_DEVICE0_ASTATUS_PREPARE                                (0x06)
#define MPI3_DEVICE0_ASTATUS_SAFE_MODE                              (0x07)
#define MPI3_DEVICE0_ASTATUS_GENERIC_MAX                            (0x0F)
/* SAS Access Status Codes  */
#define MPI3_DEVICE0_ASTATUS_SAS_UNKNOWN                            (0x10)
#define MPI3_DEVICE0_ASTATUS_ROUTE_NOT_ADDRESSABLE                  (0x11)
#define MPI3_DEVICE0_ASTATUS_SMP_ERROR_NOT_ADDRESSABLE              (0x12)
#define MPI3_DEVICE0_ASTATUS_SAS_MAX                                (0x1F)
/* SATA Access Status Codes  */
#define MPI3_DEVICE0_ASTATUS_SIF_UNKNOWN                            (0x20)
#define MPI3_DEVICE0_ASTATUS_SIF_AFFILIATION_CONFLICT               (0x21)
#define MPI3_DEVICE0_ASTATUS_SIF_DIAG                               (0x22)
#define MPI3_DEVICE0_ASTATUS_SIF_IDENTIFICATION                     (0x23)
#define MPI3_DEVICE0_ASTATUS_SIF_CHECK_POWER                        (0x24)
#define MPI3_DEVICE0_ASTATUS_SIF_PIO_SN                             (0x25)
#define MPI3_DEVICE0_ASTATUS_SIF_MDMA_SN                            (0x26)
#define MPI3_DEVICE0_ASTATUS_SIF_UDMA_SN                            (0x27)
#define MPI3_DEVICE0_ASTATUS_SIF_ZONING_VIOLATION                   (0x28)
#define MPI3_DEVICE0_ASTATUS_SIF_NOT_ADDRESSABLE                    (0x29)
#define MPI3_DEVICE0_ASTATUS_SIF_DEVICE_FAULT                       (0x2A)
#define MPI3_DEVICE0_ASTATUS_SIF_MAX                                (0x2F)
/* PCIe Access Status Codes  */
#define MPI3_DEVICE0_ASTATUS_PCIE_UNKNOWN                           (0x30)
#define MPI3_DEVICE0_ASTATUS_PCIE_MEM_SPACE_ACCESS                  (0x31)
#define MPI3_DEVICE0_ASTATUS_PCIE_UNSUPPORTED                       (0x32)
#define MPI3_DEVICE0_ASTATUS_PCIE_MSIX_REQUIRED                     (0x33)
#define MPI3_DEVICE0_ASTATUS_PCIE_ECRC_REQUIRED                     (0x34)
#define MPI3_DEVICE0_ASTATUS_PCIE_MAX                               (0x3F)
/* NVMe Access Status Codes  */
#define MPI3_DEVICE0_ASTATUS_NVME_UNKNOWN                           (0x40)
#define MPI3_DEVICE0_ASTATUS_NVME_READY_TIMEOUT                     (0x41)
#define MPI3_DEVICE0_ASTATUS_NVME_DEVCFG_UNSUPPORTED                (0x42)
#define MPI3_DEVICE0_ASTATUS_NVME_IDENTIFY_FAILED                   (0x43)
#define MPI3_DEVICE0_ASTATUS_NVME_QCONFIG_FAILED                    (0x44)
#define MPI3_DEVICE0_ASTATUS_NVME_QCREATION_FAILED                  (0x45)
#define MPI3_DEVICE0_ASTATUS_NVME_EVENTCFG_FAILED                   (0x46)
#define MPI3_DEVICE0_ASTATUS_NVME_GET_FEATURE_STAT_FAILED           (0x47)
#define MPI3_DEVICE0_ASTATUS_NVME_IDLE_TIMEOUT                      (0x48)
#define MPI3_DEVICE0_ASTATUS_NVME_CTRL_FAILURE_STATUS               (0x49)
#define MPI3_DEVICE0_ASTATUS_NVME_INSUFFICIENT_POWER                (0x4A)
#define MPI3_DEVICE0_ASTATUS_NVME_DOORBELL_STRIDE                   (0x4B)
#define MPI3_DEVICE0_ASTATUS_NVME_MEM_PAGE_MIN_SIZE                 (0x4C)
#define MPI3_DEVICE0_ASTATUS_NVME_MEMORY_ALLOCATION                 (0x4D)
#define MPI3_DEVICE0_ASTATUS_NVME_COMPLETION_TIME                   (0x4E)
#define MPI3_DEVICE0_ASTATUS_NVME_BAR                               (0x4F)
#define MPI3_DEVICE0_ASTATUS_NVME_NS_DESCRIPTOR                     (0x50)
#define MPI3_DEVICE0_ASTATUS_NVME_INCOMPATIBLE_SETTINGS             (0x51)
#define MPI3_DEVICE0_ASTATUS_NVME_TOO_MANY_ERRORS                   (0x52)
#define MPI3_DEVICE0_ASTATUS_NVME_MAX                               (0x5F)
/* Virtual Device Access Status Codes  */
#define MPI3_DEVICE0_ASTATUS_VD_UNKNOWN                             (0x80)
#define MPI3_DEVICE0_ASTATUS_VD_MAX                                 (0x8F)

/**** Defines for the Flags field ****/
#define MPI3_DEVICE0_FLAGS_MAX_WRITE_SAME_MASK          (0xE000)
#define MPI3_DEVICE0_FLAGS_MAX_WRITE_SAME_SHIFT         (13)
#define MPI3_DEVICE0_FLAGS_MAX_WRITE_SAME_NO_LIMIT      (0x0000)
#define MPI3_DEVICE0_FLAGS_MAX_WRITE_SAME_256_LB        (0x2000)
#define MPI3_DEVICE0_FLAGS_MAX_WRITE_SAME_2048_LB       (0x4000)
#define MPI3_DEVICE0_FLAGS_CONTROLLER_DEV_HANDLE        (0x0080)
#define MPI3_DEVICE0_FLAGS_IO_THROTTLING_REQUIRED       (0x0010)
#define MPI3_DEVICE0_FLAGS_HIDDEN                       (0x0008)
#define MPI3_DEVICE0_FLAGS_ATT_METHOD_VIRTUAL           (0x0004)
#define MPI3_DEVICE0_FLAGS_ATT_METHOD_DIR_ATTACHED      (0x0002)
#define MPI3_DEVICE0_FLAGS_DEVICE_PRESENT               (0x0001)

/**** Defines for the SlotIndex field - use MPI3_SLOT_INDEX_ defines ****/

/**** Defines for the DeviceForm field - use MPI3_DEVICE_DEVFORM_ defines ****/

/**** Defines for the QueueDepth field ****/
#define MPI3_DEVICE0_QUEUE_DEPTH_NOT_APPLICABLE         (0x0000)


/*****************************************************************************
 *              Device Page 1                                                *
 ****************************************************************************/
typedef struct _MPI3_DEVICE1_SAS_SATA_FORMAT
{
    U32                             Reserved00;             /* 0x00 */
} MPI3_DEVICE1_SAS_SATA_FORMAT, MPI3_POINTER PTR_MPI3_DEVICE1_SAS_SATA_FORMAT,
  Mpi3Device1SasSataFormat_t, MPI3_POINTER pMpi3Device1SasSataFormat_t;

typedef struct _MPI3_DEVICE1_PCIE_FORMAT
{
    U16                             VendorID;               /* 0x00 */
    U16                             DeviceID;               /* 0x02 */
    U16                             SubsystemVendorID;      /* 0x04 */
    U16                             SubsystemID;            /* 0x06 */
    U16                             ReadyTimeout;           /* 0x08 */
    U16                             Reserved0A;             /* 0x0A */
    U8                              RevisionID;             /* 0x0C */
    U8                              Reserved0D;             /* 0x0D */
    U16                             PCIParameters;          /* 0x0E */
} MPI3_DEVICE1_PCIE_FORMAT, MPI3_POINTER PTR_MPI3_DEVICE1_PCIE_FORMAT,
  Mpi3Device1PcieFormat_t, MPI3_POINTER pMpi3Device1PcieFormat_t;

/**** Defines for the PCIParameters field ****/
#define MPI3_DEVICE1_PCIE_PARAMS_DATA_SIZE_128B              (0x0)
#define MPI3_DEVICE1_PCIE_PARAMS_DATA_SIZE_256B              (0x1)
#define MPI3_DEVICE1_PCIE_PARAMS_DATA_SIZE_512B              (0x2)
#define MPI3_DEVICE1_PCIE_PARAMS_DATA_SIZE_1024B             (0x3)
#define MPI3_DEVICE1_PCIE_PARAMS_DATA_SIZE_2048B             (0x4)
#define MPI3_DEVICE1_PCIE_PARAMS_DATA_SIZE_4096B             (0x5)

/*** MaxReadRequestSize, CurrentMaxPayloadSize, and MaxPayloadSizeSupported  ***/
/***  all use the size definitions above - shifted to the proper position    ***/
#define MPI3_DEVICE1_PCIE_PARAMS_MAX_READ_REQ_MASK           (0x01C0)
#define MPI3_DEVICE1_PCIE_PARAMS_MAX_READ_REQ_SHIFT          (6)
#define MPI3_DEVICE1_PCIE_PARAMS_CURR_MAX_PAYLOAD_MASK       (0x0038)
#define MPI3_DEVICE1_PCIE_PARAMS_CURR_MAX_PAYLOAD_SHIFT      (3)
#define MPI3_DEVICE1_PCIE_PARAMS_SUPP_MAX_PAYLOAD_MASK       (0x0007)
#define MPI3_DEVICE1_PCIE_PARAMS_SUPP_MAX_PAYLOAD_SHIFT      (0)

typedef struct _MPI3_DEVICE1_VD_FORMAT
{
    U32                             Reserved00;             /* 0x00 */
} MPI3_DEVICE1_VD_FORMAT, MPI3_POINTER PTR_MPI3_DEVICE1_VD_FORMAT,
  Mpi3Device1VdFormat_t, MPI3_POINTER pMpi3Device1VdFormat_t;

typedef union _MPI3_DEVICE1_DEV_SPEC_FORMAT
{
    MPI3_DEVICE1_SAS_SATA_FORMAT    SasSataFormat;
    MPI3_DEVICE1_PCIE_FORMAT        PcieFormat;
    MPI3_DEVICE1_VD_FORMAT          VdFormat;
} MPI3_DEVICE1_DEV_SPEC_FORMAT, MPI3_POINTER PTR_MPI3_DEVICE1_DEV_SPEC_FORMAT,
  Mpi3Device1DevSpecFormat_t, MPI3_POINTER pMpi3Device1DevSpecFormat_t;

typedef struct _MPI3_DEVICE_PAGE1
{
    MPI3_CONFIG_PAGE_HEADER         Header;                 /* 0x00 */
    U16                             DevHandle;              /* 0x08 */
    U16                             Reserved0A;             /* 0x0A */
    U16                             LinkChangeCount;        /* 0x0C */
    U16                             RateChangeCount;        /* 0x0E */
    U16                             TMCount;                /* 0x10 */
    U16                             Reserved12;             /* 0x12 */
    U32                             Reserved14[10];         /* 0x14 */
    U8                              Reserved3C[3];          /* 0x3C */
    U8                              DeviceForm;             /* 0x3F */
    MPI3_DEVICE1_DEV_SPEC_FORMAT    DeviceSpecific;         /* 0x40 */
} MPI3_DEVICE_PAGE1, MPI3_POINTER PTR_MPI3_DEVICE_PAGE1,
  Mpi3DevicePage1_t, MPI3_POINTER pMpi3DevicePage1_t;

/**** Defines for the PageVersion field ****/
#define MPI3_DEVICE1_PAGEVERSION                            (0x00)

/**** Defines for the LinkChangeCount, RateChangeCount, TMCount fields ****/
#define MPI3_DEVICE1_COUNTER_MAX                            (0xFFFE)
#define MPI3_DEVICE1_COUNTER_INVALID                        (0xFFFF)

/**** Defines for the DeviceForm field - use MPI3_DEVICE_DEVFORM_ defines ****/

#endif  /* MPI30_CNFG_H */
