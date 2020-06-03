/** @file
  Support for PCI 2.2 standard.

  This file includes the definitions in the following specifications,
    PCI Local Bus Specification, 2.2
    PCI-to-PCI Bridge Architecture Specification, Revision 1.2
    PC Card Standard, 8.0
    PCI Power Management Interface Specification, Revision 1.2

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  Copyright (c) 2014 - 2015, Hewlett-Packard Development Company, L.P.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _PCI22_H_
#define _PCI22_H_

#define PCI_MAX_BUS     255
#define PCI_MAX_DEVICE  31
#define PCI_MAX_FUNC    7

#pragma pack(1)

///
/// Common header region in PCI Configuration Space
/// Section 6.1, PCI Local Bus Specification, 2.2
///
typedef struct {
  UINT16  VendorId;
  UINT16  DeviceId;
  UINT16  Command;
  UINT16  Status;
  UINT8   RevisionID;
  UINT8   ClassCode[3];
  UINT8   CacheLineSize;
  UINT8   LatencyTimer;
  UINT8   HeaderType;
  UINT8   BIST;
} PCI_DEVICE_INDEPENDENT_REGION;

///
/// PCI Device header region in PCI Configuration Space
/// Section 6.1, PCI Local Bus Specification, 2.2
///
typedef struct {
  UINT32  Bar[6];
  UINT32  CISPtr;
  UINT16  SubsystemVendorID;
  UINT16  SubsystemID;
  UINT32  ExpansionRomBar;
  UINT8   CapabilityPtr;
  UINT8   Reserved1[3];
  UINT32  Reserved2;
  UINT8   InterruptLine;
  UINT8   InterruptPin;
  UINT8   MinGnt;
  UINT8   MaxLat;
} PCI_DEVICE_HEADER_TYPE_REGION;

///
/// PCI Device Configuration Space
/// Section 6.1, PCI Local Bus Specification, 2.2
///
typedef struct {
  PCI_DEVICE_INDEPENDENT_REGION Hdr;
  PCI_DEVICE_HEADER_TYPE_REGION Device;
} PCI_TYPE00;

///
/// PCI-PCI Bridge header region in PCI Configuration Space
/// Section 3.2, PCI-PCI Bridge Architecture, Version 1.2
///
typedef struct {
  UINT32  Bar[2];
  UINT8   PrimaryBus;
  UINT8   SecondaryBus;
  UINT8   SubordinateBus;
  UINT8   SecondaryLatencyTimer;
  UINT8   IoBase;
  UINT8   IoLimit;
  UINT16  SecondaryStatus;
  UINT16  MemoryBase;
  UINT16  MemoryLimit;
  UINT16  PrefetchableMemoryBase;
  UINT16  PrefetchableMemoryLimit;
  UINT32  PrefetchableBaseUpper32;
  UINT32  PrefetchableLimitUpper32;
  UINT16  IoBaseUpper16;
  UINT16  IoLimitUpper16;
  UINT8   CapabilityPtr;
  UINT8   Reserved[3];
  UINT32  ExpansionRomBAR;
  UINT8   InterruptLine;
  UINT8   InterruptPin;
  UINT16  BridgeControl;
} PCI_BRIDGE_CONTROL_REGISTER;

///
/// PCI-to-PCI Bridge Configuration Space
/// Section 3.2, PCI-PCI Bridge Architecture, Version 1.2
///
typedef struct {
  PCI_DEVICE_INDEPENDENT_REGION Hdr;
  PCI_BRIDGE_CONTROL_REGISTER   Bridge;
} PCI_TYPE01;

typedef union {
  PCI_TYPE00  Device;
  PCI_TYPE01  Bridge;
} PCI_TYPE_GENERIC;

///
/// CardBus Controller Configuration Space,
/// Section 4.5.1, PC Card Standard. 8.0
///
typedef struct {
  UINT32  CardBusSocketReg;     ///< Cardbus Socket/ExCA Base
  UINT8   Cap_Ptr;
  UINT8   Reserved;
  UINT16  SecondaryStatus;      ///< Secondary Status
  UINT8   PciBusNumber;         ///< PCI Bus Number
  UINT8   CardBusBusNumber;     ///< CardBus Bus Number
  UINT8   SubordinateBusNumber; ///< Subordinate Bus Number
  UINT8   CardBusLatencyTimer;  ///< CardBus Latency Timer
  UINT32  MemoryBase0;          ///< Memory Base Register 0
  UINT32  MemoryLimit0;         ///< Memory Limit Register 0
  UINT32  MemoryBase1;
  UINT32  MemoryLimit1;
  UINT32  IoBase0;
  UINT32  IoLimit0;             ///< I/O Base Register 0
  UINT32  IoBase1;              ///< I/O Limit Register 0
  UINT32  IoLimit1;
  UINT8   InterruptLine;        ///< Interrupt Line
  UINT8   InterruptPin;         ///< Interrupt Pin
  UINT16  BridgeControl;        ///< Bridge Control
} PCI_CARDBUS_CONTROL_REGISTER;

//
// Definitions of PCI class bytes and manipulation macros.
//
#define PCI_CLASS_OLD                 0x00
#define   PCI_CLASS_OLD_OTHER           0x00
#define   PCI_CLASS_OLD_VGA             0x01

#define PCI_CLASS_MASS_STORAGE        0x01
#define   PCI_CLASS_MASS_STORAGE_SCSI   0x00
#define   PCI_CLASS_MASS_STORAGE_IDE    0x01
#define   PCI_CLASS_MASS_STORAGE_FLOPPY 0x02
#define   PCI_CLASS_MASS_STORAGE_IPI    0x03
#define   PCI_CLASS_MASS_STORAGE_RAID   0x04
#define   PCI_CLASS_MASS_STORAGE_OTHER  0x80

#define PCI_CLASS_NETWORK             0x02
#define   PCI_CLASS_NETWORK_ETHERNET    0x00
#define   PCI_CLASS_NETWORK_TOKENRING   0x01
#define   PCI_CLASS_NETWORK_FDDI        0x02
#define   PCI_CLASS_NETWORK_ATM         0x03
#define   PCI_CLASS_NETWORK_ISDN        0x04
#define   PCI_CLASS_NETWORK_OTHER       0x80

#define PCI_CLASS_DISPLAY             0x03
#define   PCI_CLASS_DISPLAY_VGA         0x00
#define     PCI_IF_VGA_VGA                0x00
#define     PCI_IF_VGA_8514               0x01
#define   PCI_CLASS_DISPLAY_XGA         0x01
#define   PCI_CLASS_DISPLAY_3D          0x02
#define   PCI_CLASS_DISPLAY_OTHER       0x80

#define PCI_CLASS_MEDIA               0x04
#define   PCI_CLASS_MEDIA_VIDEO         0x00
#define   PCI_CLASS_MEDIA_AUDIO         0x01
#define   PCI_CLASS_MEDIA_TELEPHONE     0x02
#define   PCI_CLASS_MEDIA_OTHER         0x80

#define PCI_CLASS_MEMORY_CONTROLLER   0x05
#define   PCI_CLASS_MEMORY_RAM          0x00
#define   PCI_CLASS_MEMORY_FLASH        0x01
#define   PCI_CLASS_MEMORY_OTHER        0x80

#define PCI_CLASS_BRIDGE              0x06
#define   PCI_CLASS_BRIDGE_HOST         0x00
#define   PCI_CLASS_BRIDGE_ISA          0x01
#define   PCI_CLASS_BRIDGE_EISA         0x02
#define   PCI_CLASS_BRIDGE_MCA          0x03
#define   PCI_CLASS_BRIDGE_P2P          0x04
#define     PCI_IF_BRIDGE_P2P             0x00
#define     PCI_IF_BRIDGE_P2P_SUBTRACTIVE 0x01
#define   PCI_CLASS_BRIDGE_PCMCIA       0x05
#define   PCI_CLASS_BRIDGE_NUBUS        0x06
#define   PCI_CLASS_BRIDGE_CARDBUS      0x07
#define   PCI_CLASS_BRIDGE_RACEWAY      0x08
#define   PCI_CLASS_BRIDGE_OTHER        0x80
#define   PCI_CLASS_BRIDGE_ISA_PDECODE  0x80

#define PCI_CLASS_SCC                 0x07  ///< Simple communications controllers
#define   PCI_SUBCLASS_SERIAL           0x00
#define     PCI_IF_GENERIC_XT             0x00
#define     PCI_IF_16450                  0x01
#define     PCI_IF_16550                  0x02
#define     PCI_IF_16650                  0x03
#define     PCI_IF_16750                  0x04
#define     PCI_IF_16850                  0x05
#define     PCI_IF_16950                  0x06
#define   PCI_SUBCLASS_PARALLEL         0x01
#define     PCI_IF_PARALLEL_PORT          0x00
#define     PCI_IF_BI_DIR_PARALLEL_PORT   0x01
#define     PCI_IF_ECP_PARALLEL_PORT      0x02
#define     PCI_IF_1284_CONTROLLER        0x03
#define     PCI_IF_1284_DEVICE            0xFE
#define   PCI_SUBCLASS_MULTIPORT_SERIAL 0x02
#define   PCI_SUBCLASS_MODEM            0x03
#define     PCI_IF_GENERIC_MODEM          0x00
#define     PCI_IF_16450_MODEM            0x01
#define     PCI_IF_16550_MODEM            0x02
#define     PCI_IF_16650_MODEM            0x03
#define     PCI_IF_16750_MODEM            0x04
#define   PCI_SUBCLASS_SCC_OTHER        0x80

#define PCI_CLASS_SYSTEM_PERIPHERAL   0x08
#define   PCI_SUBCLASS_PIC              0x00
#define     PCI_IF_8259_PIC               0x00
#define     PCI_IF_ISA_PIC                0x01
#define     PCI_IF_EISA_PIC               0x02
#define     PCI_IF_APIC_CONTROLLER        0x10  ///< I/O APIC interrupt controller , 32 byte none-prefetchable memory.
#define     PCI_IF_APIC_CONTROLLER2       0x20
#define   PCI_SUBCLASS_DMA              0x01
#define     PCI_IF_8237_DMA               0x00
#define     PCI_IF_ISA_DMA                0x01
#define     PCI_IF_EISA_DMA               0x02
#define   PCI_SUBCLASS_TIMER            0x02
#define     PCI_IF_8254_TIMER             0x00
#define     PCI_IF_ISA_TIMER              0x01
#define     PCI_IF_EISA_TIMER             0x02
#define   PCI_SUBCLASS_RTC              0x03
#define     PCI_IF_GENERIC_RTC            0x00
#define     PCI_IF_ISA_RTC                0x01
#define   PCI_SUBCLASS_PNP_CONTROLLER   0x04    ///< HotPlug Controller
#define   PCI_SUBCLASS_PERIPHERAL_OTHER 0x80

#define PCI_CLASS_INPUT_DEVICE        0x09
#define   PCI_SUBCLASS_KEYBOARD         0x00
#define   PCI_SUBCLASS_PEN              0x01
#define   PCI_SUBCLASS_MOUSE_CONTROLLER 0x02
#define   PCI_SUBCLASS_SCAN_CONTROLLER  0x03
#define   PCI_SUBCLASS_GAMEPORT         0x04
#define     PCI_IF_GAMEPORT               0x00
#define     PCI_IF_GAMEPORT1              0x10
#define   PCI_SUBCLASS_INPUT_OTHER      0x80

#define PCI_CLASS_DOCKING_STATION     0x0A
#define   PCI_SUBCLASS_DOCKING_GENERIC  0x00
#define   PCI_SUBCLASS_DOCKING_OTHER    0x80

#define PCI_CLASS_PROCESSOR           0x0B
#define   PCI_SUBCLASS_PROC_386         0x00
#define   PCI_SUBCLASS_PROC_486         0x01
#define   PCI_SUBCLASS_PROC_PENTIUM     0x02
#define   PCI_SUBCLASS_PROC_ALPHA       0x10
#define   PCI_SUBCLASS_PROC_POWERPC     0x20
#define   PCI_SUBCLASS_PROC_MIPS        0x30
#define   PCI_SUBCLASS_PROC_CO_PORC     0x40 ///< Co-Processor

#define PCI_CLASS_SERIAL              0x0C
#define   PCI_CLASS_SERIAL_FIREWIRE     0x00
#define     PCI_IF_1394                   0x00
#define     PCI_IF_1394_OPEN_HCI          0x10
#define   PCI_CLASS_SERIAL_ACCESS_BUS   0x01
#define   PCI_CLASS_SERIAL_SSA          0x02
#define   PCI_CLASS_SERIAL_USB          0x03
#define     PCI_IF_UHCI                   0x00
#define     PCI_IF_OHCI                   0x10
#define     PCI_IF_USB_OTHER              0x80
#define     PCI_IF_USB_DEVICE             0xFE
#define   PCI_CLASS_SERIAL_FIBRECHANNEL 0x04
#define   PCI_CLASS_SERIAL_SMB          0x05

#define PCI_CLASS_WIRELESS            0x0D
#define   PCI_SUBCLASS_IRDA             0x00
#define   PCI_SUBCLASS_IR               0x01
#define   PCI_SUBCLASS_RF               0x10
#define   PCI_SUBCLASS_WIRELESS_OTHER   0x80

#define PCI_CLASS_INTELLIGENT_IO      0x0E

#define PCI_CLASS_SATELLITE           0x0F
#define   PCI_SUBCLASS_TV               0x01
#define   PCI_SUBCLASS_AUDIO            0x02
#define   PCI_SUBCLASS_VOICE            0x03
#define   PCI_SUBCLASS_DATA             0x04

#define PCI_SECURITY_CONTROLLER       0x10   ///< Encryption and decryption controller
#define   PCI_SUBCLASS_NET_COMPUT       0x00
#define   PCI_SUBCLASS_ENTERTAINMENT    0x10
#define   PCI_SUBCLASS_SECURITY_OTHER   0x80

#define PCI_CLASS_DPIO                0x11
#define   PCI_SUBCLASS_DPIO             0x00
#define   PCI_SUBCLASS_DPIO_OTHER       0x80

/**
  Macro that checks whether the Base Class code of device matched.

  @param  _p      Specified device.
  @param  c       Base Class code needs matching.

  @retval TRUE    Base Class code matches the specified device.
  @retval FALSE   Base Class code doesn't match the specified device.

**/
#define IS_CLASS1(_p, c)              ((_p)->Hdr.ClassCode[2] == (c))
/**
  Macro that checks whether the Base Class code and Sub-Class code of device matched.

  @param  _p      Specified device.
  @param  c       Base Class code needs matching.
  @param  s       Sub-Class code needs matching.

  @retval TRUE    Base Class code and Sub-Class code match the specified device.
  @retval FALSE   Base Class code and Sub-Class code don't match the specified device.

**/
#define IS_CLASS2(_p, c, s)           (IS_CLASS1 (_p, c) && ((_p)->Hdr.ClassCode[1] == (s)))
/**
  Macro that checks whether the Base Class code, Sub-Class code and Interface code of device matched.

  @param  _p      Specified device.
  @param  c       Base Class code needs matching.
  @param  s       Sub-Class code needs matching.
  @param  p       Interface code needs matching.

  @retval TRUE    Base Class code, Sub-Class code and Interface code match the specified device.
  @retval FALSE   Base Class code, Sub-Class code and Interface code don't match the specified device.

**/
#define IS_CLASS3(_p, c, s, p)        (IS_CLASS2 (_p, c, s) && ((_p)->Hdr.ClassCode[0] == (p)))

/**
  Macro that checks whether device is a display controller.

  @param  _p      Specified device.

  @retval TRUE    Device is a display controller.
  @retval FALSE   Device is not a display controller.

**/
#define IS_PCI_DISPLAY(_p)            IS_CLASS1 (_p, PCI_CLASS_DISPLAY)
/**
  Macro that checks whether device is a VGA-compatible controller.

  @param  _p      Specified device.

  @retval TRUE    Device is a VGA-compatible controller.
  @retval FALSE   Device is not a VGA-compatible controller.

**/
#define IS_PCI_VGA(_p)                IS_CLASS3 (_p, PCI_CLASS_DISPLAY, PCI_CLASS_DISPLAY_VGA, PCI_IF_VGA_VGA)
/**
  Macro that checks whether device is an 8514-compatible controller.

  @param  _p      Specified device.

  @retval TRUE    Device is an 8514-compatible controller.
  @retval FALSE   Device is not an 8514-compatible controller.

**/
#define IS_PCI_8514(_p)               IS_CLASS3 (_p, PCI_CLASS_DISPLAY, PCI_CLASS_DISPLAY_VGA, PCI_IF_VGA_8514)
/**
  Macro that checks whether device is built before the Class Code field was defined.

  @param  _p      Specified device.

  @retval TRUE    Device is an old device.
  @retval FALSE   Device is not an old device.

**/
#define IS_PCI_OLD(_p)                IS_CLASS1 (_p, PCI_CLASS_OLD)
/**
  Macro that checks whether device is a VGA-compatible device built before the Class Code field was defined.

  @param  _p      Specified device.

  @retval TRUE    Device is an old VGA-compatible device.
  @retval FALSE   Device is not an old VGA-compatible device.

**/
#define IS_PCI_OLD_VGA(_p)            IS_CLASS2 (_p, PCI_CLASS_OLD, PCI_CLASS_OLD_VGA)
/**
  Macro that checks whether device is an IDE controller.

  @param  _p      Specified device.

  @retval TRUE    Device is an IDE controller.
  @retval FALSE   Device is not an IDE controller.

**/
#define IS_PCI_IDE(_p)                IS_CLASS2 (_p, PCI_CLASS_MASS_STORAGE, PCI_CLASS_MASS_STORAGE_IDE)
/**
  Macro that checks whether device is a SCSI bus controller.

  @param  _p      Specified device.

  @retval TRUE    Device is a SCSI bus controller.
  @retval FALSE   Device is not a SCSI bus controller.

**/
#define IS_PCI_SCSI(_p)               IS_CLASS2 (_p, PCI_CLASS_MASS_STORAGE, PCI_CLASS_MASS_STORAGE_SCSI)
/**
  Macro that checks whether device is a RAID controller.

  @param  _p      Specified device.

  @retval TRUE    Device is a RAID controller.
  @retval FALSE   Device is not a RAID controller.

**/
#define IS_PCI_RAID(_p)               IS_CLASS2 (_p, PCI_CLASS_MASS_STORAGE, PCI_CLASS_MASS_STORAGE_RAID)
/**
  Macro that checks whether device is an ISA bridge.

  @param  _p      Specified device.

  @retval TRUE    Device is an ISA bridge.
  @retval FALSE   Device is not an ISA bridge.

**/
#define IS_PCI_LPC(_p)                IS_CLASS2 (_p, PCI_CLASS_BRIDGE, PCI_CLASS_BRIDGE_ISA)
/**
  Macro that checks whether device is a PCI-to-PCI bridge.

  @param  _p      Specified device.

  @retval TRUE    Device is a PCI-to-PCI bridge.
  @retval FALSE   Device is not a PCI-to-PCI bridge.

**/
#define IS_PCI_P2P(_p)                IS_CLASS3 (_p, PCI_CLASS_BRIDGE, PCI_CLASS_BRIDGE_P2P, PCI_IF_BRIDGE_P2P)
/**
  Macro that checks whether device is a Subtractive Decode PCI-to-PCI bridge.

  @param  _p      Specified device.

  @retval TRUE    Device is a Subtractive Decode PCI-to-PCI bridge.
  @retval FALSE   Device is not a Subtractive Decode PCI-to-PCI bridge.

**/
#define IS_PCI_P2P_SUB(_p)            IS_CLASS3 (_p, PCI_CLASS_BRIDGE, PCI_CLASS_BRIDGE_P2P, PCI_IF_BRIDGE_P2P_SUBTRACTIVE)
/**
  Macro that checks whether device is a 16550-compatible serial controller.

  @param  _p      Specified device.

  @retval TRUE    Device is a 16550-compatible serial controller.
  @retval FALSE   Device is not a 16550-compatible serial controller.

**/
#define IS_PCI_16550_SERIAL(_p)       IS_CLASS3 (_p, PCI_CLASS_SCC, PCI_SUBCLASS_SERIAL, PCI_IF_16550)
/**
  Macro that checks whether device is a Universal Serial Bus controller.

  @param  _p      Specified device.

  @retval TRUE    Device is a Universal Serial Bus controller.
  @retval FALSE   Device is not a Universal Serial Bus controller.

**/
#define IS_PCI_USB(_p)                IS_CLASS2 (_p, PCI_CLASS_SERIAL, PCI_CLASS_SERIAL_USB)

//
// the definition of Header Type
//
#define HEADER_TYPE_DEVICE            0x00
#define HEADER_TYPE_PCI_TO_PCI_BRIDGE 0x01
#define HEADER_TYPE_CARDBUS_BRIDGE    0x02
#define HEADER_TYPE_MULTI_FUNCTION    0x80
//
// Mask of Header type
//
#define HEADER_LAYOUT_CODE            0x7f
/**
  Macro that checks whether device is a PCI-PCI bridge.

  @param  _p      Specified device.

  @retval TRUE    Device is a PCI-PCI bridge.
  @retval FALSE   Device is not a PCI-PCI bridge.

**/
#define IS_PCI_BRIDGE(_p)             (((_p)->Hdr.HeaderType & HEADER_LAYOUT_CODE) == (HEADER_TYPE_PCI_TO_PCI_BRIDGE))
/**
  Macro that checks whether device is a CardBus bridge.

  @param  _p      Specified device.

  @retval TRUE    Device is a CardBus bridge.
  @retval FALSE   Device is not a CardBus bridge.

**/
#define IS_CARDBUS_BRIDGE(_p)         (((_p)->Hdr.HeaderType & HEADER_LAYOUT_CODE) == (HEADER_TYPE_CARDBUS_BRIDGE))
/**
  Macro that checks whether device is a multiple functions device.

  @param  _p      Specified device.

  @retval TRUE    Device is a multiple functions device.
  @retval FALSE   Device is not a multiple functions device.

**/
#define IS_PCI_MULTI_FUNC(_p)         ((_p)->Hdr.HeaderType & HEADER_TYPE_MULTI_FUNCTION)

///
/// Rom Base Address in Bridge, defined in PCI-to-PCI Bridge Architecture Specification,
///
#define PCI_BRIDGE_ROMBAR             0x38

#define PCI_MAX_BAR                   0x0006
#define PCI_MAX_CONFIG_OFFSET         0x0100

#define PCI_VENDOR_ID_OFFSET                        0x00
#define PCI_DEVICE_ID_OFFSET                        0x02
#define PCI_COMMAND_OFFSET                          0x04
#define PCI_PRIMARY_STATUS_OFFSET                   0x06
#define PCI_REVISION_ID_OFFSET                      0x08
#define PCI_CLASSCODE_OFFSET                        0x09
#define PCI_CACHELINE_SIZE_OFFSET                   0x0C
#define PCI_LATENCY_TIMER_OFFSET                    0x0D
#define PCI_HEADER_TYPE_OFFSET                      0x0E
#define PCI_BIST_OFFSET                             0x0F
#define PCI_BASE_ADDRESSREG_OFFSET                  0x10
#define PCI_CARDBUS_CIS_OFFSET                      0x28
#define PCI_SVID_OFFSET                             0x2C ///< SubSystem Vendor id
#define PCI_SUBSYSTEM_VENDOR_ID_OFFSET              0x2C
#define PCI_SID_OFFSET                              0x2E ///< SubSystem ID
#define PCI_SUBSYSTEM_ID_OFFSET                     0x2E
#define PCI_EXPANSION_ROM_BASE                      0x30
#define PCI_CAPBILITY_POINTER_OFFSET                0x34
#define PCI_INT_LINE_OFFSET                         0x3C ///< Interrupt Line Register
#define PCI_INT_PIN_OFFSET                          0x3D ///< Interrupt Pin Register
#define PCI_MAXGNT_OFFSET                           0x3E ///< Max Grant Register
#define PCI_MAXLAT_OFFSET                           0x3F ///< Max Latency Register

//
// defined in PCI-to-PCI Bridge Architecture Specification
//
#define PCI_BRIDGE_PRIMARY_BUS_REGISTER_OFFSET      0x18
#define PCI_BRIDGE_SECONDARY_BUS_REGISTER_OFFSET    0x19
#define PCI_BRIDGE_SUBORDINATE_BUS_REGISTER_OFFSET  0x1a
#define PCI_BRIDGE_SECONDARY_LATENCY_TIMER_OFFSET   0x1b
#define PCI_BRIDGE_STATUS_REGISTER_OFFSET           0x1E
#define PCI_BRIDGE_CONTROL_REGISTER_OFFSET          0x3E

///
/// Interrupt Line "Unknown" or "No connection" value defined for x86 based system
///
#define PCI_INT_LINE_UNKNOWN                        0xFF

///
/// PCI Access Data Format
///
typedef union {
  struct {
    UINT32  Reg : 8;
    UINT32  Func : 3;
    UINT32  Dev : 5;
    UINT32  Bus : 8;
    UINT32  Reserved : 7;
    UINT32  Enable : 1;
  } Bits;
  UINT32  Uint32;
} PCI_CONFIG_ACCESS_CF8;

#pragma pack()

#define EFI_PCI_COMMAND_IO_SPACE                        BIT0   ///< 0x0001
#define EFI_PCI_COMMAND_MEMORY_SPACE                    BIT1   ///< 0x0002
#define EFI_PCI_COMMAND_BUS_MASTER                      BIT2   ///< 0x0004
#define EFI_PCI_COMMAND_SPECIAL_CYCLE                   BIT3   ///< 0x0008
#define EFI_PCI_COMMAND_MEMORY_WRITE_AND_INVALIDATE     BIT4   ///< 0x0010
#define EFI_PCI_COMMAND_VGA_PALETTE_SNOOP               BIT5   ///< 0x0020
#define EFI_PCI_COMMAND_PARITY_ERROR_RESPOND            BIT6   ///< 0x0040
#define EFI_PCI_COMMAND_STEPPING_CONTROL                BIT7   ///< 0x0080
#define EFI_PCI_COMMAND_SERR                            BIT8   ///< 0x0100
#define EFI_PCI_COMMAND_FAST_BACK_TO_BACK               BIT9   ///< 0x0200

//
// defined in PCI-to-PCI Bridge Architecture Specification
//
#define EFI_PCI_BRIDGE_CONTROL_PARITY_ERROR_RESPONSE    BIT0   ///< 0x0001
#define EFI_PCI_BRIDGE_CONTROL_SERR                     BIT1   ///< 0x0002
#define EFI_PCI_BRIDGE_CONTROL_ISA                      BIT2   ///< 0x0004
#define EFI_PCI_BRIDGE_CONTROL_VGA                      BIT3   ///< 0x0008
#define EFI_PCI_BRIDGE_CONTROL_VGA_16                   BIT4   ///< 0x0010
#define EFI_PCI_BRIDGE_CONTROL_MASTER_ABORT             BIT5   ///< 0x0020
#define EFI_PCI_BRIDGE_CONTROL_RESET_SECONDARY_BUS      BIT6   ///< 0x0040
#define EFI_PCI_BRIDGE_CONTROL_FAST_BACK_TO_BACK        BIT7   ///< 0x0080
#define EFI_PCI_BRIDGE_CONTROL_PRIMARY_DISCARD_TIMER    BIT8   ///< 0x0100
#define EFI_PCI_BRIDGE_CONTROL_SECONDARY_DISCARD_TIMER  BIT9   ///< 0x0200
#define EFI_PCI_BRIDGE_CONTROL_TIMER_STATUS             BIT10  ///< 0x0400
#define EFI_PCI_BRIDGE_CONTROL_DISCARD_TIMER_SERR       BIT11  ///< 0x0800

//
// Following are the PCI-CARDBUS bridge control bit, defined in PC Card Standard
//
#define EFI_PCI_BRIDGE_CONTROL_IREQINT_ENABLE           BIT7   ///< 0x0080
#define EFI_PCI_BRIDGE_CONTROL_RANGE0_MEMORY_TYPE       BIT8   ///< 0x0100
#define EFI_PCI_BRIDGE_CONTROL_RANGE1_MEMORY_TYPE       BIT9   ///< 0x0200
#define EFI_PCI_BRIDGE_CONTROL_WRITE_POSTING_ENABLE     BIT10  ///< 0x0400

//
// Following are the PCI status control bit
//
#define EFI_PCI_STATUS_CAPABILITY                       BIT4   ///< 0x0010
#define EFI_PCI_STATUS_66MZ_CAPABLE                     BIT5   ///< 0x0020
#define EFI_PCI_FAST_BACK_TO_BACK_CAPABLE               BIT7   ///< 0x0080
#define EFI_PCI_MASTER_DATA_PARITY_ERROR                BIT8   ///< 0x0100

///
/// defined in PC Card Standard
///
#define EFI_PCI_CARDBUS_BRIDGE_CAPABILITY_PTR 0x14

#pragma pack(1)
//
// PCI Capability List IDs and records
//
#define EFI_PCI_CAPABILITY_ID_PMI     0x01
#define EFI_PCI_CAPABILITY_ID_AGP     0x02
#define EFI_PCI_CAPABILITY_ID_VPD     0x03
#define EFI_PCI_CAPABILITY_ID_SLOTID  0x04
#define EFI_PCI_CAPABILITY_ID_MSI     0x05
#define EFI_PCI_CAPABILITY_ID_HOTPLUG 0x06
#define EFI_PCI_CAPABILITY_ID_SHPC    0x0C

///
/// Capabilities List Header
/// Section 6.7, PCI Local Bus Specification, 2.2
///
typedef struct {
  UINT8 CapabilityID;
  UINT8 NextItemPtr;
} EFI_PCI_CAPABILITY_HDR;

///
/// PMC - Power Management Capabilities
/// Section 3.2.3, PCI Power Management Interface Specification, Revision 1.2
///
typedef union {
  struct {
    UINT16 Version : 3;
    UINT16 PmeClock : 1;
    UINT16 Reserved : 1;
    UINT16 DeviceSpecificInitialization : 1;
    UINT16 AuxCurrent : 3;
    UINT16 D1Support : 1;
    UINT16 D2Support : 1;
    UINT16 PmeSupport : 5;
  } Bits;
  UINT16 Data;
} EFI_PCI_PMC;

#define EFI_PCI_PMC_D3_COLD_MASK    (BIT15)

///
/// PMCSR - Power Management Control/Status
/// Section 3.2.4, PCI Power Management Interface Specification, Revision 1.2
///
typedef union {
  struct {
    UINT16 PowerState : 2;
    UINT16 ReservedForPciExpress : 1;
    UINT16 NoSoftReset : 1;
    UINT16 Reserved : 4;
    UINT16 PmeEnable : 1;
    UINT16 DataSelect : 4;
    UINT16 DataScale : 2;
    UINT16 PmeStatus : 1;
  } Bits;
  UINT16 Data;
} EFI_PCI_PMCSR;

#define PCI_POWER_STATE_D0     0
#define PCI_POWER_STATE_D1     1
#define PCI_POWER_STATE_D2     2
#define PCI_POWER_STATE_D3_HOT 3

///
/// PMCSR_BSE - PMCSR PCI-to-PCI Bridge Support Extensions
/// Section 3.2.5, PCI Power Management Interface Specification, Revision 1.2
///
typedef union {
  struct {
    UINT8 Reserved : 6;
    UINT8 B2B3 : 1;
    UINT8 BusPowerClockControl : 1;
  } Bits;
  UINT8   Uint8;
} EFI_PCI_PMCSR_BSE;

///
/// Power Management Register Block Definition
/// Section 3.2, PCI Power Management Interface Specification, Revision 1.2
///
typedef struct {
  EFI_PCI_CAPABILITY_HDR  Hdr;
  EFI_PCI_PMC             PMC;
  EFI_PCI_PMCSR           PMCSR;
  EFI_PCI_PMCSR_BSE       BridgeExtention;
  UINT8                   Data;
} EFI_PCI_CAPABILITY_PMI;

///
/// A.G.P Capability
/// Section 6.1.4, Accelerated Graphics Port Interface Specification, Revision 1.0
///
typedef struct {
  EFI_PCI_CAPABILITY_HDR  Hdr;
  UINT8                   Rev;
  UINT8                   Reserved;
  UINT32                  Status;
  UINT32                  Command;
} EFI_PCI_CAPABILITY_AGP;

///
/// VPD Capability Structure
/// Appendix I, PCI Local Bus Specification, 2.2
///
typedef struct {
  EFI_PCI_CAPABILITY_HDR  Hdr;
  UINT16                  AddrReg;
  UINT32                  DataReg;
} EFI_PCI_CAPABILITY_VPD;

///
/// Slot Numbering Capabilities Register
/// Section 3.2.6, PCI-to-PCI Bridge Architecture Specification, Revision 1.2
///
typedef struct {
  EFI_PCI_CAPABILITY_HDR  Hdr;
  UINT8                   ExpnsSlotReg;
  UINT8                   ChassisNo;
} EFI_PCI_CAPABILITY_SLOTID;

///
/// Message Capability Structure for 32-bit Message Address
/// Section 6.8.1, PCI Local Bus Specification, 2.2
///
typedef struct {
  EFI_PCI_CAPABILITY_HDR  Hdr;
  UINT16                  MsgCtrlReg;
  UINT32                  MsgAddrReg;
  UINT16                  MsgDataReg;
} EFI_PCI_CAPABILITY_MSI32;

///
/// Message Capability Structure for 64-bit Message Address
/// Section 6.8.1, PCI Local Bus Specification, 2.2
///
typedef struct {
  EFI_PCI_CAPABILITY_HDR  Hdr;
  UINT16                  MsgCtrlReg;
  UINT32                  MsgAddrRegLsdw;
  UINT32                  MsgAddrRegMsdw;
  UINT16                  MsgDataReg;
} EFI_PCI_CAPABILITY_MSI64;

///
/// Capability EFI_PCI_CAPABILITY_ID_HOTPLUG,
/// CompactPCI Hot Swap Specification PICMG 2.1, R1.0
///
typedef struct {
  EFI_PCI_CAPABILITY_HDR  Hdr;
  ///
  /// not finished - fields need to go here
  ///
} EFI_PCI_CAPABILITY_HOTPLUG;

#define PCI_BAR_IDX0        0x00
#define PCI_BAR_IDX1        0x01
#define PCI_BAR_IDX2        0x02
#define PCI_BAR_IDX3        0x03
#define PCI_BAR_IDX4        0x04
#define PCI_BAR_IDX5        0x05

///
/// EFI PCI Option ROM definitions
///
#define EFI_ROOT_BRIDGE_LIST                            'eprb'
#define EFI_PCI_EXPANSION_ROM_HEADER_EFISIGNATURE       0x0EF1  ///< defined in UEFI Spec.

#define PCI_EXPANSION_ROM_HEADER_SIGNATURE              0xaa55
#define PCI_DATA_STRUCTURE_SIGNATURE                    SIGNATURE_32 ('P', 'C', 'I', 'R')
#define PCI_CODE_TYPE_PCAT_IMAGE                        0x00
#define EFI_PCI_EXPANSION_ROM_HEADER_COMPRESSED         0x0001  ///< defined in UEFI spec.

///
/// Standard PCI Expansion ROM Header
/// Section 13.4.2, Unified Extensible Firmware Interface Specification, Version 2.1
///
typedef struct {
  UINT16  Signature;    ///< 0xaa55
  UINT8   Reserved[0x16];
  UINT16  PcirOffset;
} PCI_EXPANSION_ROM_HEADER;

///
/// Legacy ROM Header Extensions
/// Section 6.3.3.1, PCI Local Bus Specification, 2.2
///
typedef struct {
  UINT16  Signature;    ///< 0xaa55
  UINT8   Size512;
  UINT8   InitEntryPoint[3];
  UINT8   Reserved[0x12];
  UINT16  PcirOffset;
} EFI_LEGACY_EXPANSION_ROM_HEADER;

///
/// PCI Data Structure Format
/// Section 6.3.1.2, PCI Local Bus Specification, 2.2
///
typedef struct {
  UINT32  Signature;    ///< "PCIR"
  UINT16  VendorId;
  UINT16  DeviceId;
  UINT16  Reserved0;
  UINT16  Length;
  UINT8   Revision;
  UINT8   ClassCode[3];
  UINT16  ImageLength;
  UINT16  CodeRevision;
  UINT8   CodeType;
  UINT8   Indicator;
  UINT16  Reserved1;
} PCI_DATA_STRUCTURE;

///
/// EFI PCI Expansion ROM Header
/// Section 13.4.2, Unified Extensible Firmware Interface Specification, Version 2.1
///
typedef struct {
  UINT16  Signature;    ///< 0xaa55
  UINT16  InitializationSize;
  UINT32  EfiSignature; ///< 0x0EF1
  UINT16  EfiSubsystem;
  UINT16  EfiMachineType;
  UINT16  CompressionType;
  UINT8   Reserved[8];
  UINT16  EfiImageHeaderOffset;
  UINT16  PcirOffset;
} EFI_PCI_EXPANSION_ROM_HEADER;

typedef union {
  UINT8                           *Raw;
  PCI_EXPANSION_ROM_HEADER        *Generic;
  EFI_PCI_EXPANSION_ROM_HEADER    *Efi;
  EFI_LEGACY_EXPANSION_ROM_HEADER *PcAt;
} EFI_PCI_ROM_HEADER;

#pragma pack()

#endif
