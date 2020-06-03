/** @file
  Header file for SD memory card support.

  This header file contains some definitions defined in SD Physical Layer Simplified
  Specification Version 4.10 spec.

  Copyright (c) 2015, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __SD_H__
#define __SD_H__

//
// SD command index
//
#define  SD_GO_IDLE_STATE           0
#define  SD_ALL_SEND_CID            2
#define  SD_SET_RELATIVE_ADDR       3
#define  SD_SET_DSR                 4
#define  SDIO_SEND_OP_COND          5
#define  SD_SWITCH_FUNC             6
#define  SD_SELECT_DESELECT_CARD    7
#define  SD_SEND_IF_COND            8
#define  SD_SEND_CSD                9
#define  SD_SEND_CID                10
#define  SD_VOLTAGE_SWITCH          11
#define  SD_STOP_TRANSMISSION       12
#define  SD_SEND_STATUS             13
#define  SD_GO_INACTIVE_STATE       15
#define  SD_SET_BLOCKLEN            16
#define  SD_READ_SINGLE_BLOCK       17
#define  SD_READ_MULTIPLE_BLOCK     18
#define  SD_SEND_TUNING_BLOCK       19
#define  SD_SPEED_CLASS_CONTROL     20
#define  SD_SET_BLOCK_COUNT         23
#define  SD_WRITE_SINGLE_BLOCK      24
#define  SD_WRITE_MULTIPLE_BLOCK    25
#define  SD_PROGRAM_CSD             27
#define  SD_SET_WRITE_PROT          28
#define  SD_CLR_WRITE_PROT          29
#define  SD_SEND_WRITE_PROT         30
#define  SD_ERASE_WR_BLK_START      32
#define  SD_ERASE_WR_BLK_END        33
#define  SD_ERASE                   38
#define  SD_LOCK_UNLOCK             42
#define  SD_READ_EXTR_SINGLE        48
#define  SD_WRITE_EXTR_SINGLE       49
#define  SDIO_RW_DIRECT             52
#define  SDIO_RW_EXTENDED           53
#define  SD_APP_CMD                 55
#define  SD_GEN_CMD                 56
#define  SD_READ_EXTR_MULTI         58
#define  SD_WRITE_EXTR_MULTI        59

#define  SD_SET_BUS_WIDTH           6           // ACMD6
#define  SD_STATUS                  13          // ACMD13
#define  SD_SEND_NUM_WR_BLOCKS      22          // ACMD22
#define  SD_SET_WR_BLK_ERASE_COUNT  23          // ACMD23
#define  SD_SEND_OP_COND            41          // ACMD41
#define  SD_SET_CLR_CARD_DETECT     42          // ACMD42
#define  SD_SEND_SCR                51          // ACMD51

#pragma pack(1)
typedef struct {
  UINT8   NotUsed:1;                            // Not used [0:0]
  UINT8   Crc:7;                                // CRC [7:1]
  UINT16  ManufacturingDate:12;                 // Manufacturing date [19:8]
  UINT16  Reserved:4;                           // Reserved [23:20]
  UINT8   ProductSerialNumber[4];               // Product serial number [55:24]
  UINT8   ProductRevision;                      // Product revision [63:56]
  UINT8   ProductName[5];                       // Product name [103:64]
  UINT8   OemId[2];                             // OEM/Application ID [119:104]
  UINT8   ManufacturerId;                       // Manufacturer ID [127:120]
} SD_CID;

typedef struct {
  UINT32  NotUsed:1;                            // Not used [0:0]
  UINT32  Crc:7;                                // CRC [7:1]
  UINT32  Reserved:2;                           // Reserved [9:8]
  UINT32  FileFormat:2;                         // File format [11:10]
  UINT32  TmpWriteProtect:1;                    // Temporary write protection [12:12]
  UINT32  PermWriteProtect:1;                   // Permanent write protection [13:13]
  UINT32  Copy:1;                               // Copy flag (OTP) [14:14]
  UINT32  FileFormatGrp:1;                      // File format group [15:15]
  UINT32  Reserved1:5;                          // Reserved [20:16]
  UINT32  WriteBlPartial:1;                     // Partial blocks for write allowed [21:21]
  UINT32  WriteBlLen:4;                         // Max. write data block length [25:22]
  UINT32  R2WFactor:3;                          // Write speed factor [28:26]
  UINT32  Reserved2:2;                          // Manufacturer default ECC [30:29]
  UINT32  WpGrpEnable:1;                        // Write protect group enable [31:31]

  UINT32  WpGrpSize:7;                          // Write protect group size [38:32]
  UINT32  SectorSize:7;                         // Erase sector size [45:39]
  UINT32  EraseBlkEn:1;                         // Erase single block enable [46:46]
  UINT32  CSizeMul:3;                           // device size multiplier [49:47]
  UINT32  VddWCurrMax:3;                        // max. write current @VDD max [52:50]
  UINT32  VddWCurrMin:3;                        // max. write current @VDD min [55:53]
  UINT32  VddRCurrMax:3;                        // max. read current @VDD max [58:56]
  UINT32  VddRCurrMin:3;                        // max. read current @VDD min [61:59]
  UINT32  CSizeLow:2;                           // Device size low 2 bits [63:62]

  UINT32  CSizeHigh:10;                         // Device size high 10 bits [73:64]
  UINT32  Reserved4:2;                          // Reserved [75:74]
  UINT32  DsrImp:1;                             // DSR implemented [76:76]
  UINT32  ReadBlkMisalign:1;                    // Read block misalignment [77:77]
  UINT32  WriteBlkMisalign:1;                   // Write block misalignment [78:78]
  UINT32  ReadBlPartial:1;                      // Partial blocks for read allowed [79:79]
  UINT32  ReadBlLen:4;                          // Max. read data block length [83:80]
  UINT32  Ccc:12;                               // Card command classes [95:84]

  UINT32  TranSpeed:8;                          // Max. data transfer rate [103:96]
  UINT32  Nsac:8;                               // Data read access-time in CLK cycles (NSAC*100) [111:104]
  UINT32  Taac:8;                               // Data read access-time [119:112]
  UINT32  Reserved5:6;                          // Reserved [125:120]
  UINT32  CsdStructure:2;                       // CSD structure [127:126]
} SD_CSD;

typedef struct {
  UINT32  NotUsed:1;                            // Not used [0:0]
  UINT32  Crc:7;                                // CRC [7:1]
  UINT32  Reserved:2;                           // Reserved [9:8]
  UINT32  FileFormat:2;                         // File format [11:10]
  UINT32  TmpWriteProtect:1;                    // Temporary write protection [12:12]
  UINT32  PermWriteProtect:1;                   // Permanent write protection [13:13]
  UINT32  Copy:1;                               // Copy flag (OTP) [14:14]
  UINT32  FileFormatGrp:1;                      // File format group [15:15]
  UINT32  Reserved1:5;                          // Reserved [20:16]
  UINT32  WriteBlPartial:1;                     // Partial blocks for write allowed [21:21]
  UINT32  WriteBlLen:4;                         // Max. write data block length [25:22]
  UINT32  R2WFactor:3;                          // Write speed factor [28:26]
  UINT32  Reserved2:2;                          // Manufacturer default ECC [30:29]
  UINT32  WpGrpEnable:1;                        // Write protect group enable [31:31]

  UINT32  WpGrpSize:7;                          // Write protect group size [38:32]
  UINT32  SectorSize:7;                         // Erase sector size [45:39]
  UINT32  EraseBlkEn:1;                         // Erase single block enable [46:46]
  UINT32  Reserved3:1;                          // Reserved [47:47]
  UINT32  CSizeLow:16;                          // Device size low 16 bits [63:48]

  UINT32  CSizeHigh:6;                          // Device size high 6 bits [69:64]
  UINT32  Reserved4:6;                          // Reserved [75:70]
  UINT32  DsrImp:1;                             // DSR implemented [76:76]
  UINT32  ReadBlkMisalign:1;                    // Read block misalignment [77:77]
  UINT32  WriteBlkMisalign:1;                   // Write block misalignment [78:78]
  UINT32  ReadBlPartial:1;                      // Partial blocks for read allowed [79:79]
  UINT32  ReadBlLen:4;                          // Max. read data block length [83:80]
  UINT32  Ccc:12;                               // Card command classes [95:84]

  UINT32  TranSpeed:8;                          // Max. data transfer rate [103:96]
  UINT32  Nsac:8;                               // Data read access-time in CLK cycles (NSAC*100) [111:104]
  UINT32  Taac:8;                               // Data read access-time [119:112]
  UINT32  Reserved5:6;                          // Reserved [125:120]
  UINT32  CsdStructure:2;                       // CSD structure [127:126]
} SD_CSD2;

typedef struct {
  UINT32  Reserved;                             // Reserved [31:0]

  UINT32  CmdSupport:4;                         // Command Support bits [35:32]
  UINT32  Reserved1:6;                          // Reserved [41:36]
  UINT32  SdSpec4:1;                            // Spec. Version 4.00 or higher [42:42]
  UINT32  ExSecurity:4;                         // Extended Security Support [46:43]
  UINT32  SdSpec3:1;                            // Spec. Version 3.00 or higher [47:47]
  UINT32  SdBusWidths:4;                        // DAT Bus widths supported [51:48]
  UINT32  SdSecurity:3;                         // CPRM security support [54:52]
  UINT32  DataStatAfterErase:1;                 // Data status after erases [55]
  UINT32  SdSpec:4;                             // SD Memory Card Spec. Version [59:56]
  UINT32  ScrStructure:4;                       // SCR Structure [63:60]
} SD_SCR;

#pragma pack()

#endif
