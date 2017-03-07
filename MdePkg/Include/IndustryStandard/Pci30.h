/** @file
  Support for PCI 3.0 standard.

  Copyright (c) 2006 - 2012, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials                          
  are licensed and made available under the terms and conditions of the BSD License         
  which accompanies this distribution.  The full text of the license may be found at        
  http://opensource.org/licenses/bsd-license.php                                            

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.             

**/

#ifndef __PCI30_H__
#define __PCI30_H__


#include <IndustryStandard/Pci23.h>

///
/// PCI_CLASS_MASS_STORAGE, Base Class 01h.
///
///@{
#define PCI_CLASS_MASS_STORAGE_SATADPA   0x06
#define   PCI_IF_MASS_STORAGE_SATA         0x00
#define   PCI_IF_MASS_STORAGE_AHCI         0x01
///@}

///
/// PCI_CLASS_WIRELESS, Base Class 0Dh.
///
///@{
#define PCI_SUBCLASS_ETHERNET_80211A    0x20
#define PCI_SUBCLASS_ETHERNET_80211B    0x21
///@}

/**
  Macro that checks whether device is a SATA controller.

  @param  _p      Specified device.

  @retval TRUE    Device is a SATA controller.
  @retval FALSE   Device is not a SATA controller.

**/
#define IS_PCI_SATADPA(_p) IS_CLASS2 (_p, PCI_CLASS_MASS_STORAGE, PCI_CLASS_MASS_STORAGE_SATADPA)

///
/// PCI Capability List IDs and records
///
#define EFI_PCI_CAPABILITY_ID_PCIEXP  0x10

#pragma pack(1)

///
/// PCI Data Structure Format
/// Section 5.1.2, PCI Firmware Specification, Revision 3.0
///
typedef struct {
  UINT32  Signature;    ///< "PCIR"
  UINT16  VendorId;
  UINT16  DeviceId;
  UINT16  DeviceListOffset;
  UINT16  Length;
  UINT8   Revision;
  UINT8   ClassCode[3];
  UINT16  ImageLength;
  UINT16  CodeRevision;
  UINT8   CodeType;
  UINT8   Indicator;
  UINT16  MaxRuntimeImageLength;
  UINT16  ConfigUtilityCodeHeaderOffset;
  UINT16  DMTFCLPEntryPointOffset;
} PCI_3_0_DATA_STRUCTURE;

#pragma pack()

#endif
