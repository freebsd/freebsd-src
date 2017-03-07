/** @file
  IPMI 2.0 definitions from the IPMI Specification Version 2.0, Revision 1.1.

  This file contains all NetFn Storage commands, including:
    FRU Inventory Commands (Chapter 34)
    SDR Repository (Chapter 33)
    System Event Log(SEL) Commands (Chapter 31)
    SEL Record Formats (Chapter 32)

  See IPMI specification, Appendix G, Command Assignments
  and Appendix H, Sub-function Assignments.

  Copyright (c) 1999 - 2015, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#ifndef _IPMI_NET_FN_STORAGE_H_
#define _IPMI_NET_FN_STORAGE_H_

#pragma pack(1)
//
// Net function definition for Storage command
//
#define IPMI_NETFN_STORAGE  0x0A

//
// All Storage commands and their structure definitions to follow here
//

//
//  Below is Definitions for FRU Inventory Commands (Chapter 34)
//

//
//  Definitions for Get Fru Inventory Area Info command
//
#define IPMI_STORAGE_GET_FRU_INVENTORY_AREAINFO  0x10

//
//  Constants and Structure definitions for "Get Fru Inventory Area Info" command to follow here
//

//
//  Definitions for Get Fru Data command
//
#define IPMI_STORAGE_READ_FRU_DATA 0x11

//
//  Constants and Structure definitions for "Get Fru Data" command to follow here
//
typedef struct {
  UINT8   FruDeviceId;
  UINT16  FruOffset;
} IPMI_FRU_COMMON_DATA;

typedef struct {
  IPMI_FRU_COMMON_DATA Data;
  UINT8                Count;
} IPMI_FRU_READ_COMMAND;

//
//  Definitions for Write Fru Data command
//
#define IPMI_STORAGE_WRITE_FRU_DATA  0x12

//
//  Constants and Structure definitions for "Write Fru Data" command to follow here
//
typedef struct {
  IPMI_FRU_COMMON_DATA Data;
  UINT8                FruData[16];
} IPMI_FRU_WRITE_COMMAND;

//
//  Below is Definitions for SDR Repository (Chapter 33)
//

//
//  Definitions for Get SDR Repository Info command
//
#define IPMI_STORAGE_GET_SDR_REPOSITORY_INFO 0x20

//
//  Constants and Structure definitions for "Get SDR Repository Info" command to follow here
//
typedef struct {
  UINT8   CompletionCode;
  UINT8   Version;
  UINT16  RecordCount;
  UINT16  FreeSpace;
  UINT32  RecentAdditionTimeStamp;
  UINT32  RecentEraseTimeStamp;
  UINT8   SdrRepAllocInfoCmd : 1;
  UINT8   SdrRepReserveCmd : 1;
  UINT8   PartialAddSdrCmd : 1;
  UINT8   DeleteSdrRepCmd : 1;
  UINT8   Reserved : 1;
  UINT8   SdrRepUpdateOp : 2;
  UINT8   Overflow : 1;
} IPMI_GET_SDR_REPOSITORY_INFO;

//
//  Definitions for Get SDR Repository Allocateion Info command
//
#define IPMI_STORAGE_GET_SDR_REPOSITORY_ALLOCATION_INFO  0x21

//
//  Constants and Structure definitions for "Get SDR Repository Allocateion Info" command to follow here
//

//
//  Definitions for Reserve SDR Repository command
//
#define IPMI_STORAGE_RESERVE_SDR_REPOSITORY  0x22

//
//  Constants and Structure definitions for "Reserve SDR Repository" command to follow here
//

//
//  Definitions for Get SDR command
//
#define IPMI_STORAGE_GET_SDR 0x23

//
//  Constants and Structure definitions for "Get SDR" command to follow here
//

typedef struct {
  UINT16  RecordId;                     // 1
  UINT8   Version;                      // 3
  UINT8   RecordType;                   // 4
  UINT8   RecordLength;                 // 5
  UINT8   OwnerId;                      // 6
  UINT8   OwnerLun;                     // 7
  UINT8   SensorNumber;                 // 8
  UINT8   EntityId;                     // 9
  UINT8   EntityInstance;               // 10
  UINT8   EventScanningEnabled : 1;     // 11
  UINT8   EventScanningDisabled : 1;    // 11
  UINT8   InitSensorType : 1;           // 11
  UINT8   InitHysteresis : 1;           // 11
  UINT8   InitThresholds : 1;           // 11
  UINT8   InitEvent : 1;                // 11
  UINT8   InitScanning : 1;             // 11
  UINT8   Reserved : 1;                 // 11
  UINT8   EventMessageControl : 2;      // 12
  UINT8   ThresholdAccessSupport : 2;   // 12
  UINT8   HysteresisSupport : 2;        // 12
  UINT8   ReArmSupport : 1;             // 12
  UINT8   IgnoreSensor : 1;             // 12
  UINT8   SensorType;                   // 13
  UINT8   EventType;                    // 14
  UINT8   Reserved1[7];                 // 15
  UINT8   UnitType;                     // 22
  UINT8   Reserved2;                    // 23
  UINT8   Linearization : 7;            // 24
  UINT8   Reserved3 : 1;                // 24
  UINT8   MLo;                          // 25
  UINT8   Toleremce : 6;                // 26
  UINT8   MHi : 2;                      // 26
  UINT8   BLo;                          // 27
  UINT8   AccuracyLow : 6;              // 28
  UINT8   BHi : 2;                      // 28
  UINT8   Reserved4 : 2;                // 29
  UINT8   AccuracyExp : 2;              // 29
  UINT8   AccuracyHi : 4;               // 29
  UINT8   BExp : 4;                     // 30
  UINT8   RExp : 4;                     // 30
  UINT8   NominalReadingSpscified : 1;  // 31
  UINT8   NominalMaxSpscified : 1;      // 31
  UINT8   NominalMinSpscified : 1;      // 31
  UINT8   Reserved5 : 5;                // 31
  UINT8   NominalReading;               // 32
  UINT8   Reserved6[4];                 // 33
  UINT8   UpperNonRecoverThreshold;     // 37
  UINT8   UpperCriticalThreshold;       // 38
  UINT8   UpperNonCriticalThreshold;    // 39
  UINT8   LowerNonRecoverThreshold;     // 40
  UINT8   LowerCriticalThreshold;       // 41
  UINT8   LowerNonCriticalThreshold;    // 42
  UINT8   Reserved7[5];                 // 43
  UINT8   IdStringLength;               // 48
  UINT8   AsciiIdString[16];            // 49 - 64
} IPMI_SDR_RECORD_STRUCT_1;

typedef struct {
  UINT16  RecordId;                     // 1
  UINT8   Version;                      // 3
  UINT8   RecordType;                   // 4
  UINT8   RecordLength;                 // 5
  UINT8   OwnerId;                      // 6
  UINT8   OwnerLun;                     // 7
  UINT8   SensorNumber;                 // 8
  UINT8   EntityId;                     // 9
  UINT8   EntityInstance;               // 10
  UINT8   SensorScanning : 1;           // 11
  UINT8   EventScanning : 1;            // 11
  UINT8   InitSensorType : 1;           // 11
  UINT8   InitHysteresis : 1;           // 11
  UINT8   InitThresholds : 1;           // 11
  UINT8   InitEvent : 1;                // 11
  UINT8   InitScanning : 1;             // 11
  UINT8   Reserved : 1;                 // 11
  UINT8   EventMessageControl : 2;      // 12
  UINT8   ThresholdAccessSupport : 2;   // 12
  UINT8   HysteresisSupport : 2;        // 12
  UINT8   ReArmSupport : 1;             // 12
  UINT8   IgnoreSensor : 1;             // 12
  UINT8   SensorType;                   // 13
  UINT8   EventType;                    // 14
  UINT8   Reserved1[7];                 // 15
  UINT8   UnitType;                     // 22
  UINT8   Reserved2[9];                 // 23
  UINT8   IdStringLength;               // 32
  UINT8   AsciiIdString[16];            // 33 - 48
} IPMI_SDR_RECORD_STRUCT_2;

typedef struct {
  UINT8 Reserved1 : 1;
  UINT8 ControllerSlaveAddress : 7;
  UINT8 FruDeviceId;
  UINT8 BusId : 3;
  UINT8 Lun : 2;
  UINT8 Reserved : 2;
  UINT8 LogicalFruDevice : 1;
  UINT8 Reserved3 : 4;
  UINT8 ChannelNumber : 4;
} IPMI_FRU_DATA_INFO;

typedef struct {
  UINT16            RecordId;           // 1
  UINT8             Version;            // 3
  UINT8             RecordType;         // 4
  UINT8             RecordLength;       // 5
  IPMI_FRU_DATA_INFO FruDeviceData;      // 6
  UINT8             Reserved1;          // 10
  UINT8             DeviceType;         // 11
  UINT8             DeviceTypeModifier; // 12
  UINT8             FruEntityId;        // 13
  UINT8             FruEntityInstance;  // 14
  UINT8             OemReserved;        // 15
  UINT8             Length : 4;         // 16
  UINT8             Reserved2 : 1;      // 16
  UINT8             StringType : 3;     // 16
  UINT8             String[16];         // 17
} IPMI_SDR_RECORD_STRUCT_11;

typedef struct {
  UINT16              NextRecordId;       //1
  UINT16              RecordId;           //3
  UINT8               Version;            //5
  UINT8               RecordType;         //6
  UINT8               RecordLength;       //7
  UINT8               ManufacturerId[3];  //8
  UINT8               StringChars[20];
} IPMI_SDR_RECORD_STRUCT_C0;

typedef struct {
  UINT16              NextRecordId;       //1
  UINT16              RecordId;           //3
  UINT8               Version;            //5
  UINT8               RecordType;         //6
  UINT8               RecordLength;       //7
} IPMI_SDR_RECORD_STRUCT_HEADER;

typedef union {
  IPMI_SDR_RECORD_STRUCT_1  SensorType1;
  IPMI_SDR_RECORD_STRUCT_2  SensorType2;
  IPMI_SDR_RECORD_STRUCT_11 SensorType11;
  IPMI_SDR_RECORD_STRUCT_C0       SensorTypeC0;
  IPMI_SDR_RECORD_STRUCT_HEADER   SensorHeader;
} IPMI_SENSOR_RECORD_STRUCT;

typedef struct {
  UINT16      ReservationId;
  UINT16      RecordId;
  UINT8       RecordOffset;
  UINT8       BytesToRead;
} IPMI_GET_SDR_REQUEST;

//
//  Definitions for Add SDR command
//
#define IPMI_STORAGE_ADD_SDR 0x24

//
//  Constants and Structure definitions for "Add SDR" command to follow here
//

//
//  Definitions for Partial Add SDR command
//
#define IPMI_STORAGE_PARTIAL_ADD_SDR 0x25

//
//  Constants and Structure definitions for "Partial Add SDR" command to follow here
//

//
//  Definitions for Delete SDR command
//
#define IPMI_STORAGE_DELETE_SDR  0x26

//
//  Constants and Structure definitions for "Delete SDR" command to follow here
//

//
//  Definitions for Clear SDR Repository command
//
#define IPMI_STORAGE_CLEAR_SDR 0x27

//
//  Constants and Structure definitions for "Clear SDR Repository" command to follow here
//

//
//  Definitions for Get SDR Repository Time command
//
#define IPMI_STORAGE_GET_SDR_REPOSITORY_TIME 0x28

//
//  Constants and Structure definitions for "Get SDR Repository Time" command to follow here
//

//
//  Definitions for Set SDR Repository Time command
//
#define IPMI_STORAGE_SET_SDR_REPOSITORY_TIME 0x29

//
//  Constants and Structure definitions for "Set SDR Repository Time" command to follow here
//

//
//  Definitions for Enter SDR Repository Update Mode command
//
#define IPMI_STORAGE_ENTER_SDR_UPDATE_MODE 0x2A

//
//  Constants and Structure definitions for "Enter SDR Repository Update Mode" command to follow here
//

//
//  Definitions for Exit SDR Repository Update Mode command
//
#define IPMI_STORAGE_EXIT_SDR_UPDATE_MODE  0x2B

//
//  Constants and Structure definitions for "Exit SDR Repository Update Mode" command to follow here
//

//
//  Definitions for Run Initialize Agent command
//
#define IPMI_STORAGE_RUN_INIT_AGENT  0x2C

//
//  Constants and Structure definitions for "Run Initialize Agent" command to follow here
//

//
//  Below is Definitions for System Event Log(SEL) Commands (Chapter 31)
//

//
//  Definitions for Get SEL Info command
//
#define IPMI_STORAGE_GET_SEL_INFO  0x40

//
//  Constants and Structure definitions for "Get SEL Info" command to follow here
//
typedef struct {
  UINT8   CompletionCode;
  UINT8   Version;              // Version of SEL
  UINT16  NoOfEntries;          // No of Entries in the SEL
  UINT16  FreeSpace;            // Free space in Bytes
  UINT32  RecentAddTimeStamp;   // Most Recent Addition of Time Stamp
  UINT32  RecentEraseTimeStamp; // Most Recent Erasure of Time Stamp
  UINT8   OperationSupport;     // Operation Support
} IPMI_GET_SEL_INFO_RESPONSE;

//
//  Definitions for Get SEL Allocation Info command
//
#define IPMI_STORAGE_GET_SEL_ALLOCATION_INFO 0x41

//
//  Constants and Structure definitions for "Get SEL Allocation Info" command to follow here
//

//
//  Definitions for Reserve SEL command
//
#define IPMI_STORAGE_RESERVE_SEL 0x42

//
//  Constants and Structure definitions for "Reserve SEL" command to follow here
//

//
//  Definitions for Get SEL Entry command
//
#define IPMI_STORAGE_GET_SEL_ENTRY 0x43

//
//  Constants and Structure definitions for "Get SEL Entry" command to follow here
//
typedef struct {
  UINT8 ReserveId[2]; // Reservation ID, LS Byte First
  UINT8 SelRecID[2];  // Sel Record ID, LS Byte First
  UINT8 Offset;       // Offset Into Record
  UINT8 BytesToRead;  // Bytes to be Read, 0xFF for entire record
} IPMI_GET_SEL_ENTRY_REQUEST;

//
//  Definitions for Add SEL Entry command
//
#define IPMI_STORAGE_ADD_SEL_ENTRY 0x44

//
//  Constants and Structure definitions for "Add SEL Entry" command to follow here
//

//
//  Definitions for Partial Add SEL Entry command
//
#define IPMI_STORAGE_PARTIAL_ADD_SEL_ENTRY 0x45

//
//  Constants and Structure definitions for "Partial Add SEL Entry" command to follow here
//

//
//  Definitions for Delete SEL Entry command
//
#define IPMI_STORAGE_DELETE_SEL_ENTRY  0x46

//
//  Constants and Structure definitions for "Delete SEL Entry" command to follow here
//
typedef struct {
  UINT8 ReserveId[2];       // Reservation ID, LS byte first
  UINT8 RecordToDelete[2];  // Record to Delete, LS Byte First
} IPMI_DELETE_SEL_REQUEST;

//
//  Definitions for Clear SEL command
//
#define IPMI_STORAGE_CLEAR_SEL 0x47

//
//  Constants and Structure definitions for "Clear SEL" command to follow here
//
typedef struct {
  UINT8 Reserve[2]; // Reserve ID, LSB first
  UINT8 AscC;       // Ascii for 'C' (0x43)
  UINT8 AscL;       // Ascii for 'L' (0x4c)
  UINT8 AscR;       // Ascii for 'R' (0x52)
  UINT8 Erase;      // 0xAA, Initiate Erase, 0x00 Get Erase Status
} IPMI_CLEAR_SEL_REQUEST;

//
//  Definitions for Get SEL Time command
//
#define IPMI_STORAGE_GET_SEL_TIME  0x48

//
//  Constants and Structure definitions for "Get SEL Time" command to follow here
//

//
//  Definitions for Set SEL Time command
//
#define IPMI_STORAGE_SET_SEL_TIME  0x49

//
//  Constants and Structure definitions for "Set SEL Time" command to follow here
//

//
//  Definitions for Get Auxillary Log Status command
//
#define IPMI_STORAGE_GET_AUXILLARY_LOG_STATUS  0x5A

//
//  Constants and Structure definitions for "Get Auxillary Log Status" command to follow here
//

//
//  Definitions for Set Auxillary Log Status command
//
#define IPMI_STORAGE_SET_AUXILLARY_LOG_STATUS  0x5B

//
//  Constants and Structure definitions for "Set Auxillary Log Status" command to follow here
//

#define IPMI_COMPLETE_SEL_RECORD 0xFF

//
//  Below is Definitions for SEL Record Formats (Chapter 32)
//
typedef struct {
  UINT16  RecordId;
  UINT8   RecordType;
  UINT32  TimeStamp;
  UINT16  GeneratorId;
  UINT8   EvMRevision;
  UINT8   SensorType;
  UINT8   SensorNumber;
  UINT8   EventDirType;
  UINT8   OEMEvData1;
  UINT8   OEMEvData2;
  UINT8   OEMEvData3;
} IPMI_SEL_EVENT_RECORD_DATA;

#define IPMI_SEL_SYSTEM_RECORD                     0x02

#define IPMI_EVM_REVISION                          0x04
#define IPMI_BIOS_ID                               0x18
#define IPMI_FORMAT_REV                            0x00
#define IPMI_FORMAT_REV1                           0x01
#define IPMI_SOFTWARE_ID                           0x01
#define IPMI_PLATFORM_VAL_ID                       0x01
#define IPMI_GENERATOR_ID(i,f)                     ((i << 1) | (f << 1) | IPMI_SOFTWARE_ID)

#define IPMI_SENSOR_TYPE_EVENT_CODE_DISCRETE       0x6F

#define IPMI_OEM_SPECIFIC_DATA                     0x02
#define IPMI_SENSOR_SPECIFIC_DATA                  0x03

#pragma pack()
#endif
