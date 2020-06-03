/** @file
  Support for USB 2.0 standard.

  Copyright (c) 2006 - 2014, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __USB_H__
#define __USB_H__

//
// Subset of Class and Subclass definitions from USB Specs
//

//
// Usb mass storage class code
//
#define USB_MASS_STORE_CLASS    0x08

//
// Usb mass storage subclass code, specify the command set used.
//
#define USB_MASS_STORE_RBC      0x01 ///< Reduced Block Commands
#define USB_MASS_STORE_8020I    0x02 ///< SFF-8020i, typically a CD/DVD device
#define USB_MASS_STORE_QIC      0x03 ///< Typically a tape device
#define USB_MASS_STORE_UFI      0x04 ///< Typically a floppy disk driver device
#define USB_MASS_STORE_8070I    0x05 ///< SFF-8070i, typically a floppy disk driver device.
#define USB_MASS_STORE_SCSI     0x06 ///< SCSI transparent command set

//
// Usb mass storage protocol code, specify the transport protocol
//
#define USB_MASS_STORE_CBI0     0x00 ///< CBI protocol with command completion interrupt
#define USB_MASS_STORE_CBI1     0x01 ///< CBI protocol without command completion interrupt
#define USB_MASS_STORE_BOT      0x50 ///< Bulk-Only Transport

//
// Standard device request and request type
// USB 2.0 spec, Section 9.4
//
#define USB_DEV_GET_STATUS                  0x00
#define USB_DEV_GET_STATUS_REQ_TYPE_D       0x80 // Receiver : Device
#define USB_DEV_GET_STATUS_REQ_TYPE_I       0x81 // Receiver : Interface
#define USB_DEV_GET_STATUS_REQ_TYPE_E       0x82 // Receiver : Endpoint

#define USB_DEV_CLEAR_FEATURE               0x01
#define USB_DEV_CLEAR_FEATURE_REQ_TYPE_D    0x00 // Receiver : Device
#define USB_DEV_CLEAR_FEATURE_REQ_TYPE_I    0x01 // Receiver : Interface
#define USB_DEV_CLEAR_FEATURE_REQ_TYPE_E    0x02 // Receiver : Endpoint

#define USB_DEV_SET_FEATURE                 0x03
#define USB_DEV_SET_FEATURE_REQ_TYPE_D      0x00 // Receiver : Device
#define USB_DEV_SET_FEATURE_REQ_TYPE_I      0x01 // Receiver : Interface
#define USB_DEV_SET_FEATURE_REQ_TYPE_E      0x02 // Receiver : Endpoint

#define USB_DEV_SET_ADDRESS                 0x05
#define USB_DEV_SET_ADDRESS_REQ_TYPE        0x00

#define USB_DEV_GET_DESCRIPTOR              0x06
#define USB_DEV_GET_DESCRIPTOR_REQ_TYPE     0x80

#define USB_DEV_SET_DESCRIPTOR              0x07
#define USB_DEV_SET_DESCRIPTOR_REQ_TYPE     0x00

#define USB_DEV_GET_CONFIGURATION           0x08
#define USB_DEV_GET_CONFIGURATION_REQ_TYPE  0x80

#define USB_DEV_SET_CONFIGURATION           0x09
#define USB_DEV_SET_CONFIGURATION_REQ_TYPE  0x00

#define USB_DEV_GET_INTERFACE               0x0A
#define USB_DEV_GET_INTERFACE_REQ_TYPE      0x81

#define USB_DEV_SET_INTERFACE               0x0B
#define USB_DEV_SET_INTERFACE_REQ_TYPE      0x01

#define USB_DEV_SYNCH_FRAME                 0x0C
#define USB_DEV_SYNCH_FRAME_REQ_TYPE        0x82


//
// USB standard descriptors and reqeust
//
#pragma pack(1)

///
/// Format of Setup Data for USB Device Requests
/// USB 2.0 spec, Section 9.3
///
typedef struct {
  UINT8           RequestType;
  UINT8           Request;
  UINT16          Value;
  UINT16          Index;
  UINT16          Length;
} USB_DEVICE_REQUEST;

///
/// Standard Device Descriptor
/// USB 2.0 spec, Section 9.6.1
///
typedef struct {
  UINT8           Length;
  UINT8           DescriptorType;
  UINT16          BcdUSB;
  UINT8           DeviceClass;
  UINT8           DeviceSubClass;
  UINT8           DeviceProtocol;
  UINT8           MaxPacketSize0;
  UINT16          IdVendor;
  UINT16          IdProduct;
  UINT16          BcdDevice;
  UINT8           StrManufacturer;
  UINT8           StrProduct;
  UINT8           StrSerialNumber;
  UINT8           NumConfigurations;
} USB_DEVICE_DESCRIPTOR;

///
/// Standard Configuration Descriptor
/// USB 2.0 spec, Section 9.6.3
///
typedef struct {
  UINT8           Length;
  UINT8           DescriptorType;
  UINT16          TotalLength;
  UINT8           NumInterfaces;
  UINT8           ConfigurationValue;
  UINT8           Configuration;
  UINT8           Attributes;
  UINT8           MaxPower;
} USB_CONFIG_DESCRIPTOR;

///
/// Standard Interface Descriptor
/// USB 2.0 spec, Section 9.6.5
///
typedef struct {
  UINT8           Length;
  UINT8           DescriptorType;
  UINT8           InterfaceNumber;
  UINT8           AlternateSetting;
  UINT8           NumEndpoints;
  UINT8           InterfaceClass;
  UINT8           InterfaceSubClass;
  UINT8           InterfaceProtocol;
  UINT8           Interface;
} USB_INTERFACE_DESCRIPTOR;

///
/// Standard Endpoint Descriptor
/// USB 2.0 spec, Section 9.6.6
///
typedef struct {
  UINT8           Length;
  UINT8           DescriptorType;
  UINT8           EndpointAddress;
  UINT8           Attributes;
  UINT16          MaxPacketSize;
  UINT8           Interval;
} USB_ENDPOINT_DESCRIPTOR;

///
/// UNICODE String Descriptor
/// USB 2.0 spec, Section 9.6.7
///
typedef struct {
  UINT8           Length;
  UINT8           DescriptorType;
  CHAR16          String[1];
} EFI_USB_STRING_DESCRIPTOR;

#pragma pack()


typedef enum {
  //
  // USB request type
  //
  USB_REQ_TYPE_STANDARD   = (0x00 << 5),
  USB_REQ_TYPE_CLASS      = (0x01 << 5),
  USB_REQ_TYPE_VENDOR     = (0x02 << 5),

  //
  // Standard control transfer request type, or the value
  // to fill in EFI_USB_DEVICE_REQUEST.Request
  //
  USB_REQ_GET_STATUS      = 0x00,
  USB_REQ_CLEAR_FEATURE   = 0x01,
  USB_REQ_SET_FEATURE     = 0x03,
  USB_REQ_SET_ADDRESS     = 0x05,
  USB_REQ_GET_DESCRIPTOR  = 0x06,
  USB_REQ_SET_DESCRIPTOR  = 0x07,
  USB_REQ_GET_CONFIG      = 0x08,
  USB_REQ_SET_CONFIG      = 0x09,
  USB_REQ_GET_INTERFACE   = 0x0A,
  USB_REQ_SET_INTERFACE   = 0x0B,
  USB_REQ_SYNCH_FRAME     = 0x0C,

  //
  // Usb control transfer target
  //
  USB_TARGET_DEVICE       = 0,
  USB_TARGET_INTERFACE    = 0x01,
  USB_TARGET_ENDPOINT     = 0x02,
  USB_TARGET_OTHER        = 0x03,

  //
  // USB Descriptor types
  //
  USB_DESC_TYPE_DEVICE    = 0x01,
  USB_DESC_TYPE_CONFIG    = 0x02,
  USB_DESC_TYPE_STRING    = 0x03,
  USB_DESC_TYPE_INTERFACE = 0x04,
  USB_DESC_TYPE_ENDPOINT  = 0x05,
  USB_DESC_TYPE_HID       = 0x21,
  USB_DESC_TYPE_REPORT    = 0x22,

  //
  // Features to be cleared by CLEAR_FEATURE requests
  //
  USB_FEATURE_ENDPOINT_HALT = 0,

  //
  // USB endpoint types: 00: control, 01: isochronous, 10: bulk, 11: interrupt
  //
  USB_ENDPOINT_CONTROL    = 0x00,
  USB_ENDPOINT_ISO        = 0x01,
  USB_ENDPOINT_BULK       = 0x02,
  USB_ENDPOINT_INTERRUPT  = 0x03,

  USB_ENDPOINT_TYPE_MASK  = 0x03,
  USB_ENDPOINT_DIR_IN     = 0x80,

  //
  //Use 200 ms to increase the error handling response time
  //
  EFI_USB_INTERRUPT_DELAY = 2000000
} USB_TYPES_DEFINITION;


//
// HID constants definition, see Device Class Definition
// for Human Interface Devices (HID) rev1.11
//

//
// HID standard GET_DESCRIPTOR request.
//
#define USB_HID_GET_DESCRIPTOR_REQ_TYPE  0x81

//
// HID specific requests.
//
#define USB_HID_CLASS_GET_REQ_TYPE       0xa1
#define USB_HID_CLASS_SET_REQ_TYPE       0x21

//
// HID report item format
//
#define HID_ITEM_FORMAT_SHORT 0
#define HID_ITEM_FORMAT_LONG  1

//
// Special tag indicating long items
//
#define HID_ITEM_TAG_LONG 15

//
// HID report descriptor item type (prefix bit 2,3)
//
#define HID_ITEM_TYPE_MAIN      0
#define HID_ITEM_TYPE_GLOBAL    1
#define HID_ITEM_TYPE_LOCAL     2
#define HID_ITEM_TYPE_RESERVED  3

//
// HID report descriptor main item tags
//
#define HID_MAIN_ITEM_TAG_INPUT             8
#define HID_MAIN_ITEM_TAG_OUTPUT            9
#define HID_MAIN_ITEM_TAG_FEATURE           11
#define HID_MAIN_ITEM_TAG_BEGIN_COLLECTION  10
#define HID_MAIN_ITEM_TAG_END_COLLECTION    12

//
// HID report descriptor main item contents
//
#define HID_MAIN_ITEM_CONSTANT      0x001
#define HID_MAIN_ITEM_VARIABLE      0x002
#define HID_MAIN_ITEM_RELATIVE      0x004
#define HID_MAIN_ITEM_WRAP          0x008
#define HID_MAIN_ITEM_NONLINEAR     0x010
#define HID_MAIN_ITEM_NO_PREFERRED  0x020
#define HID_MAIN_ITEM_NULL_STATE    0x040
#define HID_MAIN_ITEM_VOLATILE      0x080
#define HID_MAIN_ITEM_BUFFERED_BYTE 0x100

//
// HID report descriptor collection item types
//
#define HID_COLLECTION_PHYSICAL     0
#define HID_COLLECTION_APPLICATION  1
#define HID_COLLECTION_LOGICAL      2

//
// HID report descriptor global item tags
//
#define HID_GLOBAL_ITEM_TAG_USAGE_PAGE        0
#define HID_GLOBAL_ITEM_TAG_LOGICAL_MINIMUM   1
#define HID_GLOBAL_ITEM_TAG_LOGICAL_MAXIMUM   2
#define HID_GLOBAL_ITEM_TAG_PHYSICAL_MINIMUM  3
#define HID_GLOBAL_ITEM_TAG_PHYSICAL_MAXIMUM  4
#define HID_GLOBAL_ITEM_TAG_UNIT_EXPONENT     5
#define HID_GLOBAL_ITEM_TAG_UNIT              6
#define HID_GLOBAL_ITEM_TAG_REPORT_SIZE       7
#define HID_GLOBAL_ITEM_TAG_REPORT_ID         8
#define HID_GLOBAL_ITEM_TAG_REPORT_COUNT      9
#define HID_GLOBAL_ITEM_TAG_PUSH              10
#define HID_GLOBAL_ITEM_TAG_POP               11

//
// HID report descriptor local item tags
//
#define HID_LOCAL_ITEM_TAG_USAGE              0
#define HID_LOCAL_ITEM_TAG_USAGE_MINIMUM      1
#define HID_LOCAL_ITEM_TAG_USAGE_MAXIMUM      2
#define HID_LOCAL_ITEM_TAG_DESIGNATOR_INDEX   3
#define HID_LOCAL_ITEM_TAG_DESIGNATOR_MINIMUM 4
#define HID_LOCAL_ITEM_TAG_DESIGNATOR_MAXIMUM 5
#define HID_LOCAL_ITEM_TAG_STRING_INDEX       7
#define HID_LOCAL_ITEM_TAG_STRING_MINIMUM     8
#define HID_LOCAL_ITEM_TAG_STRING_MAXIMUM     9
#define HID_LOCAL_ITEM_TAG_DELIMITER          10

//
// HID report types
//
#define HID_INPUT_REPORT    1
#define HID_OUTPUT_REPORT   2
#define HID_FEATURE_REPORT  3

//
// HID class protocol request
//
#define EFI_USB_GET_REPORT_REQUEST    0x01
#define EFI_USB_GET_IDLE_REQUEST      0x02
#define EFI_USB_GET_PROTOCOL_REQUEST  0x03
#define EFI_USB_SET_REPORT_REQUEST    0x09
#define EFI_USB_SET_IDLE_REQUEST      0x0a
#define EFI_USB_SET_PROTOCOL_REQUEST  0x0b

#pragma pack(1)
///
/// Descriptor header for Report/Physical Descriptors
/// HID 1.1, section 6.2.1
///
typedef struct hid_class_descriptor {
  UINT8   DescriptorType;
  UINT16  DescriptorLength;
} EFI_USB_HID_CLASS_DESCRIPTOR;

///
/// The HID descriptor identifies the length and type
/// of subordinate descriptors for a device.
/// HID 1.1, section 6.2.1
///
typedef struct hid_descriptor {
  UINT8                         Length;
  UINT8                         DescriptorType;
  UINT16                        BcdHID;
  UINT8                         CountryCode;
  UINT8                         NumDescriptors;
  EFI_USB_HID_CLASS_DESCRIPTOR  HidClassDesc[1];
} EFI_USB_HID_DESCRIPTOR;

#pragma pack()

#endif
