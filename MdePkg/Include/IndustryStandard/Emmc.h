/** @file
  Header file for eMMC support.

  This header file contains some definitions defined in EMMC4.5/EMMC5.0 spec.

  Copyright (c) 2015, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __EMMC_H__
#define __EMMC_H__

//
// EMMC command index
//
#define  EMMC_GO_IDLE_STATE           0
#define  EMMC_SEND_OP_COND            1
#define  EMMC_ALL_SEND_CID            2
#define  EMMC_SET_RELATIVE_ADDR       3
#define  EMMC_SET_DSR                 4
#define  EMMC_SLEEP_AWAKE             5
#define  EMMC_SWITCH                  6
#define  EMMC_SELECT_DESELECT_CARD    7
#define  EMMC_SEND_EXT_CSD            8
#define  EMMC_SEND_CSD                9
#define  EMMC_SEND_CID                10
#define  EMMC_STOP_TRANSMISSION       12
#define  EMMC_SEND_STATUS             13
#define  EMMC_BUSTEST_R               14
#define  EMMC_GO_INACTIVE_STATE       15
#define  EMMC_SET_BLOCKLEN            16
#define  EMMC_READ_SINGLE_BLOCK       17
#define  EMMC_READ_MULTIPLE_BLOCK     18
#define  EMMC_BUSTEST_W               19
#define  EMMC_SEND_TUNING_BLOCK       21
#define  EMMC_SET_BLOCK_COUNT         23
#define  EMMC_WRITE_BLOCK             24
#define  EMMC_WRITE_MULTIPLE_BLOCK    25
#define  EMMC_PROGRAM_CID             26
#define  EMMC_PROGRAM_CSD             27
#define  EMMC_SET_WRITE_PROT          28
#define  EMMC_CLR_WRITE_PROT          29
#define  EMMC_SEND_WRITE_PROT         30
#define  EMMC_SEND_WRITE_PROT_TYPE    31
#define  EMMC_ERASE_GROUP_START       35
#define  EMMC_ERASE_GROUP_END         36
#define  EMMC_ERASE                   38
#define  EMMC_FAST_IO                 39
#define  EMMC_GO_IRQ_STATE            40
#define  EMMC_LOCK_UNLOCK             42
#define  EMMC_SET_TIME                49
#define  EMMC_PROTOCOL_RD             53
#define  EMMC_PROTOCOL_WR             54
#define  EMMC_APP_CMD                 55
#define  EMMC_GEN_CMD                 56

typedef enum {
  EmmcPartitionUserData              = 0,
  EmmcPartitionBoot1                 = 1,
  EmmcPartitionBoot2                 = 2,
  EmmcPartitionRPMB                  = 3,
  EmmcPartitionGP1                   = 4,
  EmmcPartitionGP2                   = 5,
  EmmcPartitionGP3                   = 6,
  EmmcPartitionGP4                   = 7,
  EmmcPartitionUnknown
} EMMC_PARTITION_TYPE;

#pragma pack(1)
typedef struct {
  UINT8   NotUsed:1;                              // Not used [0:0]
  UINT8   Crc:7;                                  // CRC [7:1]
  UINT8   ManufacturingDate;                      // Manufacturing date [15:8]
  UINT8   ProductSerialNumber[4];                 // Product serial number [47:16]
  UINT8   ProductRevision;                        // Product revision [55:48]
  UINT8   ProductName[6];                         // Product name [103:56]
  UINT8   OemId;                                  // OEM/Application ID [111:104]
  UINT8   DeviceType:2;                           // Device/BGA [113:112]
  UINT8   Reserved:6;                             // Reserved [119:114]
  UINT8   ManufacturerId;                         // Manufacturer ID [127:120]
} EMMC_CID;

typedef struct {
  UINT32  NotUsed:1;                              // Not used [0:0]
  UINT32  Crc:7;                                  // CRC [7:1]
  UINT32  Ecc:2;                                  // ECC code [9:8]
  UINT32  FileFormat:2;                           // File format [11:10]
  UINT32  TmpWriteProtect:1;                      // Temporary write protection [12:12]
  UINT32  PermWriteProtect:1;                     // Permanent write protection [13:13]
  UINT32  Copy:1;                                 // Copy flag (OTP) [14:14]
  UINT32  FileFormatGrp:1;                        // File format group [15:15]
  UINT32  ContentProtApp:1;                       // Content protection application [16:16]
  UINT32  Reserved:4;                             // Reserved [20:17]
  UINT32  WriteBlPartial:1;                       // Partial blocks for write allowed [21:21]
  UINT32  WriteBlLen:4;                           // Max. write data block length [25:22]
  UINT32  R2WFactor:3;                            // Write speed factor [28:26]
  UINT32  DefaultEcc:2;                           // Manufacturer default ECC [30:29]
  UINT32  WpGrpEnable:1;                          // Write protect group enable [31:31]

  UINT32  WpGrpSize:5;                            // Write protect group size [36:32]
  UINT32  EraseGrpMult:5;                         // Erase group size multiplier [41:37]
  UINT32  EraseGrpSize:5;                         // Erase group size [46:42]
  UINT32  CSizeMult:3;                            // Device size multiplier [49:47]
  UINT32  VddWCurrMax:3;                          // Max. write current @ VDD max [52:50]
  UINT32  VddWCurrMin:3;                          // Max. write current @ VDD min [55:53]
  UINT32  VddRCurrMax:3;                          // Max. read current @ VDD max [58:56]
  UINT32  VddRCurrMin:3;                          // Max. read current @ VDD min [61:59]
  UINT32  CSizeLow:2;                             // Device size low two bits [63:62]

  UINT32  CSizeHigh:10;                           // Device size high eight bits [73:64]
  UINT32  Reserved1:2;                            // Reserved [75:74]
  UINT32  DsrImp:1;                               // DSR implemented [76:76]
  UINT32  ReadBlkMisalign:1;                      // Read block misalignment [77:77]
  UINT32  WriteBlkMisalign:1;                     // Write block misalignment [78:78]
  UINT32  ReadBlPartial:1;                        // Partial blocks for read allowed [79:79]
  UINT32  ReadBlLen:4;                            // Max. read data block length [83:80]
  UINT32  Ccc:12;                                 // Device command classes [95:84]

  UINT32  TranSpeed:8;                            // Max. bus clock frequency [103:96]
  UINT32  Nsac:8;                                 // Data read access-time 2 in CLK cycles (NSAC*100) [111:104]
  UINT32  Taac:8;                                 // Data read access-time 1 [119:112]
  UINT32  Reserved2:2;                            // Reserved [121:120]
  UINT32  SpecVers:4;                             // System specification version [125:122]
  UINT32  CsdStructure:2;                         // CSD structure [127:126]
} EMMC_CSD;

typedef struct {
  //
  // Modes Segment
  //
  UINT8   Reserved[16];                           // Reserved [15:0]
  UINT8   SecureRemovalType;                      // Secure Removal Type R/W & R [16]
  UINT8   ProductStateAwarenessEnablement;        // Product state awareness enablement R/W/E & R [17]
  UINT8   MaxPreLoadingDataSize[4];               // Max pre loading data size R [21:18]
  UINT8   PreLoadingDataSize[4];                  // Pre loading data size R/W/EP [25:22]
  UINT8   FfuStatus;                              // FFU status R [26]
  UINT8   Reserved1[2];                           // Reserved [28:27]
  UINT8   ModeOperationCodes;                     // Mode operation codes W/EP [29]
  UINT8   ModeConfig;                             // Mode config R/W/EP [30]
  UINT8   Reserved2;                              // Reserved [31]
  UINT8   FlushCache;                             // Flushing of the cache W/EP [32]
  UINT8   CacheCtrl;                              // Control to turn the Cache ON/OFF R/W/EP [33]
  UINT8   PowerOffNotification;                   // Power Off Notification R/W/EP [34]
  UINT8   PackedFailureIndex;                     // Packed command failure index R [35]
  UINT8   PackedCommandStatus;                    // Packed command status R [36]
  UINT8   ContextConf[15];                        // Context configuration R/W/EP [51:37]
  UINT8   ExtPartitionsAttribute[2];              // Extended Partitions Attribute R/W [53:52]
  UINT8   ExceptionEventsStatus[2];               // Exception events status R [55:54]
  UINT8   ExceptionEventsCtrl[2];                 // Exception events control R/W/EP [57:56]
  UINT8   DyncapNeeded;                           // Number of addressed group to be Released R [58]
  UINT8   Class6Ctrl;                             // Class 6 commands control R/W/EP [59]
  UINT8   IniTimeoutEmu;                          // 1st initialization after disabling sector size emulation R [60]
  UINT8   DataSectorSize;                         // Sector size R [61]
  UINT8   UseNativeSector;                        // Sector size emulation R/W [62]
  UINT8   NativeSectorSize;                       // Native sector size R [63]
  UINT8   VendorSpecificField[64];                // Vendor Specific Fields <vendor specific> [127:64]
  UINT8   Reserved3[2];                           // Reserved [129:128]
  UINT8   ProgramCidCsdDdrSupport;                // Program CID/CSD in DDR mode support R [130]
  UINT8   PeriodicWakeup;                         // Periodic Wake-up R/W/E [131]
  UINT8   TcaseSupport;                           // Package Case Temperature is controlled W/EP [132]
  UINT8   ProductionStateAwareness;               // Production state awareness R/W/E [133]
  UINT8   SecBadBlkMgmnt;                         // Bad Block Management mode R/W [134]
  UINT8   Reserved4;                              // Reserved [135]
  UINT8   EnhStartAddr[4];                        // Enhanced User Data Start Address R/W [139:136]
  UINT8   EnhSizeMult[3];                         // Enhanced User Data Area Size R/W [142:140]
  UINT8   GpSizeMult[12];                         // General Purpose Partition Size R/W [154:143]
  UINT8   PartitionSettingCompleted;              // Partitioning Setting R/W [155]
  UINT8   PartitionsAttribute;                    // Partitions attribute R/W [156]
  UINT8   MaxEnhSizeMult[3];                      // Max Enhanced Area Size R [159:157]
  UINT8   PartitioningSupport;                    // Partitioning Support R [160]
  UINT8   HpiMgmt;                                // HPI management R/W/EP [161]
  UINT8   RstFunction;                            // H/W reset function R/W [162]
  UINT8   BkopsEn;                                // Enable background operations handshake R/W [163]
  UINT8   BkopsStart;                             // Manually start background operations W/EP [164]
  UINT8   SanitizeStart;                          // Start Sanitize operation W/EP [165]
  UINT8   WrRelParam;                             // Write reliability parameter register R [166]
  UINT8   WrRelSet;                               // Write reliability setting register R/W [167]
  UINT8   RpmbSizeMult;                           // RPMB Size R [168]
  UINT8   FwConfig;                               // FW configuration R/W [169]
  UINT8   Reserved5;                              // Reserved [170]
  UINT8   UserWp;                                 // User area write protection register R/W,R/W/CP&R/W/EP [171]
  UINT8   Reserved6;                              // Reserved [172]
  UINT8   BootWp;                                 // Boot area write protection register R/W&R/W/CP[173]
  UINT8   BootWpStatus;                           // Boot write protection status registers R [174]
  UINT8   EraseGroupDef;                          // High-density erase group definition R/W/EP [175]
  UINT8   Reserved7;                              // Reserved [176]
  UINT8   BootBusConditions;                      // Boot bus Conditions R/W/E [177]
  UINT8   BootConfigProt;                         // Boot config protection R/W&R/W/CP[178]
  UINT8   PartitionConfig;                        // Partition configuration R/W/E&R/W/EP[179]
  UINT8   Reserved8;                              // Reserved [180]
  UINT8   ErasedMemCont;                          // Erased memory content R [181]
  UINT8   Reserved9;                              // Reserved [182]
  UINT8   BusWidth;                               // Bus width mode W/EP [183]
  UINT8   Reserved10;                             // Reserved [184]
  UINT8   HsTiming;                               // High-speed interface timing R/W/EP [185]
  UINT8   Reserved11;                             // Reserved [186]
  UINT8   PowerClass;                             // Power class R/W/EP [187]
  UINT8   Reserved12;                             // Reserved [188]
  UINT8   CmdSetRev;                              // Command set revision R [189]
  UINT8   Reserved13;                             // Reserved [190]
  UINT8   CmdSet;                                 // Command set R/W/EP [191]
  //
  // Properties Segment
  //
  UINT8   ExtCsdRev;                              // Extended CSD revision [192]
  UINT8   Reserved14;                             // Reserved [193]
  UINT8   CsdStructure;                           // CSD STRUCTURE [194]
  UINT8   Reserved15;                             // Reserved [195]
  UINT8   DeviceType;                             // Device type [196]
  UINT8   DriverStrength;                         // I/O Driver Strength [197]
  UINT8   OutOfInterruptTime;                     // Out-of-interrupt busy timing[198]
  UINT8   PartitionSwitchTime;                    // Partition switching timing [199]
  UINT8   PwrCl52M195V;                           // Power class for 52MHz at 1.95V [200]
  UINT8   PwrCl26M195V;                           // Power class for 26MHz at 1.95V [201]
  UINT8   PwrCl52M360V;                           // Power class for 52MHz at 3.6V [202]
  UINT8   PwrCl26M360V;                           // Power class for 26MHz at 3.6V [203]
  UINT8   Reserved16;                             // Reserved [204]
  UINT8   MinPerfR4B26M;                          // Minimum Read Performance for 4bit at 26MHz [205]
  UINT8   MinPerfW4B26M;                          // Minimum Write Performance for 4bit at 26MHz [206]
  UINT8   MinPerfR8B26M4B52M;                     // Minimum Read Performance for 8bit at 26MHz, for 4bit at 52MHz [207]
  UINT8   MinPerfW8B26M4B52M;                     // Minimum Write Performance for 8bit at 26MHz, for 4bit at 52MHz [208]
  UINT8   MinPerfR8B52M;                          // Minimum Read Performance for 8bit at 52MHz [209]
  UINT8   MinPerfW8B52M;                          // Minimum Write Performance for 8bit at 52MHz [210]
  UINT8   Reserved17;                             // Reserved [211]
  UINT8   SecCount[4];                            // Sector Count [215:212]
  UINT8   SleepNotificationTime;                  // Sleep Notification Timeout [216]
  UINT8   SATimeout;                              // Sleep/awake timeout [217]
  UINT8   ProductionStateAwarenessTimeout;        // Production state awareness timeout [218]
  UINT8   SCVccq;                                 // Sleep current (VCCQ) [219]
  UINT8   SCVcc;                                  // Sleep current (VCC) [220]
  UINT8   HcWpGrpSize;                            // High-capacity write protect group size [221]
  UINT8   RelWrSecC;                              // Reliable write sector count [222]
  UINT8   EraseTimeoutMult;                       // High-capacity erase timeout [223]
  UINT8   HcEraseGrpSize;                         // High-capacity erase unit size [224]
  UINT8   AccSize;                                // Access size [225]
  UINT8   BootSizeMult;                           // Boot partition size [226]
  UINT8   Reserved18;                             // Reserved [227]
  UINT8   BootInfo;                               // Boot information [228]
  UINT8   SecTrimMult;                            // Secure TRIM Multiplier [229]
  UINT8   SecEraseMult;                           // Secure Erase Multiplier [230]
  UINT8   SecFeatureSupport;                      // Secure Feature support [231]
  UINT8   TrimMult;                               // TRIM Multiplier [232]
  UINT8   Reserved19;                             // Reserved [233]
  UINT8   MinPerfDdrR8b52M;                       // Minimum Read Performance for 8bit at 52MHz in DDR mode [234]
  UINT8   MinPerfDdrW8b52M;                       // Minimum Write Performance for 8bit at 52MHz in DDR mode [235]
  UINT8   PwrCl200M130V;                          // Power class for 200MHz, at VCCQ=1.3V, VCC = 3.6V [236]
  UINT8   PwrCl200M195V;                          // Power class for 200MHz at VCCQ=1.95V, VCC = 3.6V [237]
  UINT8   PwrClDdr52M195V;                        // Power class for 52MHz, DDR at VCC= 1.95V [238]
  UINT8   PwrClDdr52M360V;                        // Power class for 52MHz, DDR at VCC= 3.6V [239]
  UINT8   Reserved20;                             // Reserved [240]
  UINT8   IniTimeoutAp;                           // 1st initialization time after partitioning [241]
  UINT8   CorrectlyPrgSectorsNum[4];              // Number of correctly programmed sectors [245:242]
  UINT8   BkopsStatus;                            // Background operations status [246]
  UINT8   PowerOffLongTime;                       // Power off notification(long) timeout [247]
  UINT8   GenericCmd6Time;                        // Generic CMD6 timeout [248]
  UINT8   CacheSize[4];                           // Cache size [252:249]
  UINT8   PwrClDdr200M360V;                       // Power class for 200MHz, DDR at VCC= 3.6V [253]
  UINT8   FirmwareVersion[8];                     // Firmware version [261:254]
  UINT8   DeviceVersion[2];                       // Device version [263:262]
  UINT8   OptimalTrimUnitSize;                    // Optimal trim unit size[264]
  UINT8   OptimalWriteSize;                       // Optimal write size [265]
  UINT8   OptimalReadSize;                        // Optimal read size [266]
  UINT8   PreEolInfo;                             // Pre EOL information [267]
  UINT8   DeviceLifeTimeEstTypA;                  // Device life time estimation type A [268]
  UINT8   DeviceLifeTimeEstTypB;                  // Device life time estimation type B [269]
  UINT8   VendorProprietaryHealthReport[32];      // Vendor proprietary health report [301:270]
  UINT8   NumOfFwSectorsProgrammed[4];            // Number of FW sectors correctly programmed [305:302]
  UINT8   Reserved21[181];                        // Reserved [486:306]
  UINT8   FfuArg[4];                              // FFU Argument [490:487]
  UINT8   OperationCodeTimeout;                   // Operation codes timeout [491]
  UINT8   FfuFeatures;                            // FFU features [492]
  UINT8   SupportedModes;                         // Supported modes [493]
  UINT8   ExtSupport;                             // Extended partitions attribute support [494]
  UINT8   LargeUnitSizeM1;                        // Large Unit size [495]
  UINT8   ContextCapabilities;                    // Context management capabilities [496]
  UINT8   TagResSize;                             // Tag Resources Size [497]
  UINT8   TagUnitSize;                            // Tag Unit Size [498]
  UINT8   DataTagSupport;                         // Data Tag Support [499]
  UINT8   MaxPackedWrites;                        // Max packed write commands [500]
  UINT8   MaxPackedReads;                         // Max packed read commands[501]
  UINT8   BkOpsSupport;                           // Background operations support [502]
  UINT8   HpiFeatures;                            // HPI features [503]
  UINT8   SupportedCmdSet;                        // Supported Command Sets [504]
  UINT8   ExtSecurityErr;                         // Extended Security Commands Error [505]
  UINT8   Reserved22[6];                          // Reserved [511:506]
} EMMC_EXT_CSD;

#pragma pack()

#endif
