/*
 *  linux/drivers/message/fusion/scsi3.h
 *      SCSI-3 definitions and macros.
 *      (Ultimately) SCSI-3 definitions; for now, inheriting
 *      SCSI-2 definitions.
 *
 *  Copyright (c) 1996-2002 Steven J. Ralston
 *  Written By: Steven J. Ralston (19960517)
 *  (mailto:sjralston1@netscape.net)
 *  (mailto:mpt_linux_developer@lsil.com)
 *
 *  $Id: scsi3.h,v 1.9 2002/02/27 18:45:02 sralston Exp $
 */

#ifndef SCSI3_H_INCLUDED
#define SCSI3_H_INCLUDED
/***************************************************************************/

/****************************************************************************
 *
 *  Includes
 */
#ifdef __KERNEL__
#include <linux/types.h>
#else
    #ifndef U_STUFF_DEFINED
    #define U_STUFF_DEFINED
    typedef unsigned char u8;
    typedef unsigned short u16;
    typedef unsigned int u32;
    #endif
#endif

/****************************************************************************
 *
 *  Defines
 */

/*
 *    SCSI Commands
 */
#define CMD_TestUnitReady      0x00
#define CMD_RezeroUnit         0x01  /* direct-access devices */
#define CMD_Rewind             0x01  /* sequential-access devices */
#define CMD_RequestSense       0x03
#define CMD_FormatUnit         0x04
#define CMD_ReassignBlock      0x07
#define CMD_Read6              0x08
#define CMD_Write6             0x0A
#define CMD_WriteFilemark      0x10
#define CMD_Space              0x11
#define CMD_Inquiry            0x12
#define CMD_ModeSelect6        0x15
#define CMD_ModeSense6         0x1A
#define CMD_Reserve6           0x16
#define CMD_Release6           0x17
#define CMD_Erase              0x19
#define CMD_StartStopUnit      0x1b  /* direct-access devices */
#define CMD_LoadUnload         0x1b  /* sequential-access devices */
#define CMD_ReceiveDiagnostic  0x1C
#define CMD_SendDiagnostic     0x1D
#define CMD_ReadCapacity       0x25
#define CMD_Read10             0x28
#define CMD_Write10            0x2A
#define CMD_WriteVerify        0x2E
#define CMD_Verify             0x2F
#define CMD_SynchronizeCache   0x35
#define CMD_ReadDefectData     0x37
#define CMD_WriteBuffer        0x3B
#define CMD_ReadBuffer         0x3C
#define CMD_ReadLong           0x3E
#define CMD_LogSelect          0x4C
#define CMD_LogSense           0x4D
#define CMD_ModeSelect10       0x55
#define CMD_Reserve10          0x56
#define CMD_Release10          0x57
#define CMD_ModeSense10        0x5A
#define CMD_PersistReserveIn   0x5E
#define CMD_PersistReserveOut  0x5F
#define CMD_ReportLuns         0xA0

/*
 *    Control byte field
 */
#define CONTROL_BYTE_NACA_BIT  0x04
#define CONTROL_BYTE_Flag_BIT  0x02
#define CONTROL_BYTE_Link_BIT  0x01

/*
 *    SCSI Messages
 */
#define MSG_COMPLETE             0x00
#define MSG_EXTENDED             0x01
#define MSG_SAVE_POINTERS        0x02
#define MSG_RESTORE_POINTERS     0x03
#define MSG_DISCONNECT           0x04
#define MSG_IDERROR              0x05
#define MSG_ABORT                0x06
#define MSG_REJECT               0x07
#define MSG_NOP                  0x08
#define MSG_PARITY_ERROR         0x09
#define MSG_LINKED_CMD_COMPLETE  0x0a
#define MSG_LCMD_COMPLETE_W_FLG  0x0b
#define MSG_BUS_DEVICE_RESET     0x0c
#define MSG_ABORT_TAG            0x0d
#define MSG_CLEAR_QUEUE          0x0e
#define MSG_INITIATE_RECOVERY    0x0f

#define MSG_RELEASE_RECOVRY      0x10
#define MSG_TERMINATE_IO         0x11

#define MSG_SIMPLE_QUEUE         0x20
#define MSG_HEAD_OF_QUEUE        0x21
#define MSG_ORDERED_QUEUE        0x22
#define MSG_IGNORE_WIDE_RESIDUE  0x23

#define MSG_IDENTIFY             0x80
#define MSG_IDENTIFY_W_DISC      0xc0

/*
 *    SCSI Phases
 */
#define PHS_DATA_OUT  0x00
#define PHS_DATA_IN   0x01
#define PHS_COMMAND   0x02
#define PHS_STATUS    0x03
#define PHS_MSG_OUT   0x06
#define PHS_MSG_IN    0x07

/*
 *    Statuses
 */
#define STS_GOOD                        0x00
#define STS_CHECK_CONDITION             0x02
#define STS_CONDITION_MET               0x04
#define STS_BUSY                        0x08
#define STS_INTERMEDIATE                0x10
#define STS_INTERMEDIATE_CONDITION_MET  0x14
#define STS_RESERVATION_CONFLICT        0x18
#define STS_COMMAND_TERMINATED          0x22
#define STS_TASK_SET_FULL               0x28
#define    STS_QUEUE_FULL               0x28
#define STS_ACA_ACTIVE                  0x30

#define STS_VALID_MASK                  0x3e

#define SCSI_STATUS(x)  ((x) & STS_VALID_MASK)

/*
 *    SCSI QTag Types
 */
#define QTAG_SIMPLE     0x20
#define QTAG_HEAD_OF_Q  0x21
#define QTAG_ORDERED    0x22

/*
 *    SCSI Sense Key Definitons
 */
#define SK_NO_SENSE         0x00
#define SK_RECOVERED_ERROR  0x01
#define SK_NOT_READY        0x02
#define SK_MEDIUM_ERROR     0x03
#define SK_HARDWARE_ERROR   0x04
#define SK_ILLEGAL_REQUEST  0x05
#define SK_UNIT_ATTENTION   0x06
#define SK_DATA_PROTECT     0x07
#define SK_BLANK_CHECK      0x08
#define SK_VENDOR_SPECIFIC  0x09
#define SK_COPY_ABORTED     0x0a
#define SK_ABORTED_COMMAND  0x0b
#define SK_EQUAL            0x0c
#define SK_VOLUME_OVERFLOW  0x0d
#define SK_MISCOMPARE       0x0e
#define SK_RESERVED         0x0f



#define SCSI_MAX_INQUIRY_BYTES  96
#define SCSI_STD_INQUIRY_BYTES  36

#undef USE_SCSI_COMPLETE_INQDATA
/*
 *      Structure definition for SCSI Inquiry Data
 *
 *  NOTE: The following structure is 96 bytes in size
 *      iff USE_SCSI_COMPLETE_INQDATA IS defined above (i.e. w/ "#define").
 *      If USE_SCSI_COMPLETE_INQDATA is NOT defined above (i.e. w/ "#undef")
 *      then the following structure is only 36 bytes in size.
 *  THE CHOICE IS YOURS!
 */
typedef struct SCSI_Inquiry_Data
{
#ifdef USE_SCSI_COMPLETE_INQDATA
    u8   InqByte[SCSI_MAX_INQUIRY_BYTES];
#else
    u8   InqByte[SCSI_STD_INQUIRY_BYTES];
#endif

/*
 * the following structure works only for little-endian (Intel,
 * LSB first (1234) byte order) systems with 4-byte ints.
 *
        u32    Periph_Device_Type    : 5,
               Periph_Qualifier      : 3,
               Device_Type_Modifier  : 7,
               Removable_Media       : 1,
               ANSI_Version          : 3,
               ECMA_Version          : 3,
               ISO_Version           : 2,
               Response_Data_Format  : 4,
               reserved_0            : 3,
               AERC                  : 1  ;
        u32    Additional_Length     : 8,
               reserved_1            :16,
               SftReset              : 1,
               CmdQue                : 1,
               reserved_2            : 1,
               Linked                : 1,
               Sync                  : 1,
               WBus16                : 1,
               WBus32                : 1,
               RelAdr                : 1  ;
        u8     Vendor_ID[8];
        u8     Product_ID[16];
        u8     Revision_Level [4];
#ifdef USE_SCSI_COMPLETE_INQDATA
        u8     Vendor_Specific[20];
        u8     reserved_3[40];
#endif
 *
 */

} SCSI_Inquiry_Data_t;

#define INQ_PERIPHINFO_BYTE            0
#define   INQ_Periph_Qualifier_MASK      0xe0
#define   INQ_Periph_Device_Type_MASK    0x1f

#define INQ_Peripheral_Qualifier(inqp) \
    (int)((*((u8*)(inqp)+INQ_PERIPHINFO_BYTE) & INQ_Periph_Qualifier_MASK) >> 5)
#define INQ_Peripheral_Device_Type(inqp) \
    (int)(*((u8*)(inqp)+INQ_PERIPHINFO_BYTE) & INQ_Periph_Device_Type_MASK)


#define INQ_DEVTYPEMOD_BYTE            1
#define   INQ_RMB_BIT                    0x80
#define   INQ_Device_Type_Modifier_MASK  0x7f

#define INQ_Removable_Medium(inqp) \
    (int)(*((u8*)(inqp)+INQ_DEVTYPEMOD_BYTE) & INQ_RMB_BIT)
#define INQ_Device_Type_Modifier(inqp) \
    (int)(*((u8*)(inqp)+INQ_DEVTYPEMOD_BYTE) & INQ_Device_Type_Modifier_MASK)


#define INQ_VERSIONINFO_BYTE           2
#define   INQ_ISO_Version_MASK           0xc0
#define   INQ_ECMA_Version_MASK          0x38
#define   INQ_ANSI_Version_MASK          0x07

#define INQ_ISO_Version(inqp) \
    (int)(*((u8*)(inqp)+INQ_VERSIONINFO_BYTE) & INQ_ISO_Version_MASK)
#define INQ_ECMA_Version(inqp) \
    (int)(*((u8*)(inqp)+INQ_VERSIONINFO_BYTE) & INQ_ECMA_Version_MASK)
#define INQ_ANSI_Version(inqp) \
    (int)(*((u8*)(inqp)+INQ_VERSIONINFO_BYTE) & INQ_ANSI_Version_MASK)


#define INQ_BYTE3                      3
#define   INQ_AERC_BIT                   0x80
#define   INQ_TrmTsk_BIT                 0x40
#define   INQ_NormACA_BIT                0x20
#define   INQ_RDF_MASK                   0x0F

#define INQ_AER_Capable(inqp) \
    (int)(*((u8*)(inqp)+INQ_BYTE3) & INQ_AERC_BIT)
#define INQ_TrmTsk(inqp) \
    (int)(*((u8*)(inqp)+INQ_BYTE3) & INQ_TrmTsk_BIT)
#define INQ_NormACA(inqp) \
    (int)(*((u8*)(inqp)+INQ_BYTE3) & INQ_NormACA_BIT)
#define INQ_Response_Data_Format(inqp) \
    (int)(*((u8*)(inqp)+INQ_BYTE3) & INQ_RDF_MASK)


#define INQ_CAPABILITY_BYTE            7
#define   INQ_RelAdr_BIT                 0x80
#define   INQ_WBus32_BIT                 0x40
#define   INQ_WBus16_BIT                 0x20
#define   INQ_Sync_BIT                   0x10
#define   INQ_Linked_BIT                 0x08
  /*      INQ_Reserved BIT               0x40 */
#define   INQ_CmdQue_BIT                 0x02
#define   INQ_SftRe_BIT                  0x01

#define IS_RelAdr_DEV(inqp) \
    (int)(*((u8*)(inqp)+INQ_CAPABILITY_BYTE) & INQ_RelAdr_BIT)
#define IS_WBus32_DEV(inqp) \
    (int)(*((u8*)(inqp)+INQ_CAPABILITY_BYTE) & INQ_WBus32_BIT)
#define IS_WBus16_DEV(inqp) \
    (int)(*((u8*)(inqp)+INQ_CAPABILITY_BYTE) & INQ_WBus16_BIT)
#define IS_Sync_DEV(inqp) \
    (int)(*((u8*)(inqp)+INQ_CAPABILITY_BYTE) & INQ_Sync_BIT)
#define IS_Linked_DEV(inqp) \
    (int)(*((u8*)(inqp)+INQ_CAPABILITY_BYTE) & INQ_Linked_BIT)
#define IS_CmdQue_DEV(inqp) \
    (int)(*((u8*)(inqp)+INQ_CAPABILITY_BYTE) & INQ_CmdQue_BIT)
#define IS_SftRe_DEV(inqp) \
    (int)(*((u8*)(inqp)+INQ_CAPABILITY_BYTE) & INQ_SftRe_BIT)

#define INQ_Width_BITS \
    (INQ_WBus32_BIT | INQ_WBus16_BIT)
#define IS_Wide_DEV(inqp) \
    (int)(*((u8*)(inqp)+INQ_CAPABILITY_BYTE) & INQ_Width_BITS)


/*
 *      SCSI peripheral device types
 */
#define SCSI_TYPE_DAD               0x00  /* Direct Access Device */
#define SCSI_TYPE_SAD               0x01  /* Sequential Access Device */
#define SCSI_TYPE_TAPE  SCSI_TYPE_SAD
#define SCSI_TYPE_PRT               0x02  /* Printer */
#define SCSI_TYPE_PROC              0x03  /* Processor */
#define SCSI_TYPE_WORM              0x04
#define SCSI_TYPE_CDROM             0x05
#define SCSI_TYPE_SCAN              0x06  /* Scanner */
#define SCSI_TYPE_OPTICAL           0x07  /* Magneto/Optical */
#define SCSI_TYPE_CHANGER           0x08
#define SCSI_TYPE_COMM              0x09  /* Communications device */
#define SCSI_TYPE_UNKNOWN           0x1f
#define SCSI_TYPE_UNCONFIGURED_LUN  0x7f

#define SCSI_TYPE_MAX_KNOWN         SCSI_TYPE_COMM

/*
 *      Peripheral Qualifiers
 */
#define DEVICE_PRESENT     0x00
#define LUN_NOT_PRESENT    0x01
#define LUN_NOT_SUPPORTED  0x03

/*
 *      ANSI Versions
 */
#ifndef SCSI_1
#define SCSI_1  0x01
#endif
#ifndef SCSI_2
#define SCSI_2  0x02
#endif
#ifndef SCSI_3
#define SCSI_3  0x03
#endif


#define SCSI_MAX_SENSE_BYTES  255
#define SCSI_STD_SENSE_BYTES   18
#define SCSI_PAD_SENSE_BYTES      (SCSI_MAX_SENSE_BYTES - SCSI_STD_SENSE_BYTES)

#undef USE_SCSI_COMPLETE_SENSE
/*
 *      Structure definition for SCSI Sense Data
 *
 *  NOTE: The following structure is 255 bytes in size
 *      iiff USE_SCSI_COMPLETE_SENSE IS defined above (i.e. w/ "#define").
 *      If USE_SCSI_COMPLETE_SENSE is NOT defined above (i.e. w/ "#undef")
 *      then the following structure is only 19 bytes in size.
 *  THE CHOICE IS YOURS!
 *
 */
typedef struct SCSI_Sense_Data
{
#ifdef USE_SCSI_COMPLETE_SENSE
    u8       SenseByte[SCSI_MAX_SENSE_BYTES];
#else
    u8       SenseByte[SCSI_STD_SENSE_BYTES];
#endif

/*
 * the following structure works only for little-endian (Intel,
 * LSB first (1234) byte order) systems with 4-byte ints.
 *
    u8     Error_Code                :4,            // 0x00
           Error_Class               :3,
           Valid                     :1
     ;
    u8     Segment_Number                           // 0x01
     ;
    u8     Sense_Key                 :4,            // 0x02
           Reserved                  :1,
           Incorrect_Length_Indicator:1,
           End_Of_Media              :1,
           Filemark                  :1
     ;
    u8     Information_MSB;                         // 0x03
    u8     Information_Byte2;                       // 0x04
    u8     Information_Byte1;                       // 0x05
    u8     Information_LSB;                         // 0x06
    u8     Additional_Length;                       // 0x07

    u32    Command_Specific_Information;            // 0x08 - 0x0b

    u8     Additional_Sense_Code;                   // 0x0c
    u8     Additional_Sense_Code_Qualifier;         // 0x0d
    u8     Field_Replaceable_Unit_Code;             // 0x0e
    u8     Illegal_Req_Bit_Pointer   :3,            // 0x0f
           Illegal_Req_Bit_Valid     :1,
           Illegal_Req_Reserved      :2,
           Illegal_Req_Cmd_Data      :1,
           Sense_Key_Specific_Valid  :1
     ;
    u16    Sense_Key_Specific_Data;                 // 0x10 - 0x11

#ifdef USE_SCSI_COMPLETE_SENSE
    u8     Additional_Sense_Data[SCSI_PAD_SENSE_BYTES];
#else
    u8     Additional_Sense_Data[1];
#endif
 *
 */

} SCSI_Sense_Data_t;


#define SD_ERRCODE_BYTE                0
#define   SD_Valid_BIT                   0x80
#define   SD_Error_Code_MASK             0x7f
#define SD_Valid(sdp) \
    (int)(*((u8*)(sdp)+SD_ERRCODE_BYTE) & SD_Valid_BIT)
#define SD_Error_Code(sdp) \
    (int)(*((u8*)(sdp)+SD_ERRCODE_BYTE) & SD_Error_Code_MASK)


#define SD_SEGNUM_BYTE                 1
#define SD_Segment_Number(sdp)  (int)(*((u8*)(sdp)+SD_SEGNUM_BYTE))


#define SD_SENSEKEY_BYTE               2
#define   SD_Filemark_BIT                0x80
#define   SD_EOM_BIT                     0x40
#define   SD_ILI_BIT                     0x20
#define   SD_Sense_Key_MASK              0x0f
#define SD_Filemark(sdp) \
    (int)(*((u8*)(sdp)+SD_SENSEKEY_BYTE) & SD_Filemark_BIT)
#define SD_EOM(sdp) \
    (int)(*((u8*)(sdp)+SD_SENSEKEY_BYTE) & SD_EOM_BIT)
#define SD_ILI(sdp) \
    (int)(*((u8*)(sdp)+SD_SENSEKEY_BYTE) & SD_ILI_BIT)
#define SD_Sense_Key(sdp) \
    (int)(*((u8*)(sdp)+SD_SENSEKEY_BYTE) & SD_Sense_Key_MASK)


#define SD_INFO3_BYTE                  3
#define SD_INFO2_BYTE                  4
#define SD_INFO1_BYTE                  5
#define SD_INFO0_BYTE                  6
#define SD_Information3(sdp)  (int)(*((u8*)(sdp)+SD_INFO3_BYTE))
#define SD_Information2(sdp)  (int)(*((u8*)(sdp)+SD_INFO2_BYTE))
#define SD_Information1(sdp)  (int)(*((u8*)(sdp)+SD_INFO1_BYTE))
#define SD_Information0(sdp)  (int)(*((u8*)(sdp)+SD_INFO0_BYTE))


#define SD_ADDL_LEN_BYTE               7
#define SD_Additional_Sense_Length(sdp) \
    (int)(*((u8*)(sdp)+SD_ADDL_LEN_BYTE))
#define SD_Addl_Sense_Len  SD_Additional_Sense_Length


#define SD_CMD_SPECIFIC3_BYTE          8
#define SD_CMD_SPECIFIC2_BYTE          9
#define SD_CMD_SPECIFIC1_BYTE         10
#define SD_CMD_SPECIFIC0_BYTE         11
#define SD_Cmd_Specific_Info3(sdp)  (int)(*((u8*)(sdp)+SD_CMD_SPECIFIC3_BYTE))
#define SD_Cmd_Specific_Info2(sdp)  (int)(*((u8*)(sdp)+SD_CMD_SPECIFIC2_BYTE))
#define SD_Cmd_Specific_Info1(sdp)  (int)(*((u8*)(sdp)+SD_CMD_SPECIFIC1_BYTE))
#define SD_Cmd_Specific_Info0(sdp)  (int)(*((u8*)(sdp)+SD_CMD_SPECIFIC0_BYTE))


#define SD_ADDL_SENSE_CODE_BYTE       12
#define SD_Additional_Sense_Code(sdp) \
    (int)(*((u8*)(sdp)+SD_ADDL_SENSE_CODE_BYTE))
#define SD_Addl_Sense_Code  SD_Additional_Sense_Code
#define SD_ASC  SD_Additional_Sense_Code


#define SD_ADDL_SENSE_CODE_QUAL_BYTE  13
#define SD_Additional_Sense_Code_Qualifier(sdp) \
    (int)(*((u8*)(sdp)+SD_ADDL_SENSE_CODE_QUAL_BYTE))
#define SD_Addl_Sense_Code_Qual  SD_Additional_Sense_Code_Qualifier
#define SD_ASCQ  SD_Additional_Sense_Code_Qualifier


#define SD_FIELD_REPL_UNIT_CODE_BYTE  14
#define SD_Field_Replaceable_Unit_Code(sdp) \
    (int)(*((u8*)(sdp)+SD_FIELD_REPL_UNIT_CODE_BYTE))
#define SD_Field_Repl_Unit_Code  SD_Field_Replaceable_Unit_Code
#define SD_FRUC  SD_Field_Replaceable_Unit_Code
#define SD_FRU  SD_Field_Replaceable_Unit_Code


/*
 *  Sense-Key Specific offsets and macros.
 */
#define SD_SKS2_BYTE                  15
#define   SD_SKS_Valid_BIT               0x80
#define   SD_SKS_Cmd_Data_BIT            0x40
#define   SD_SKS_Bit_Ptr_Valid_BIT       0x08
#define   SD_SKS_Bit_Ptr_MASK            0x07
#define SD_SKS1_BYTE                  16
#define SD_SKS0_BYTE                  17
#define SD_Sense_Key_Specific_Valid(sdp) \
    (int)(*((u8*)(sdp)+SD_SKS2_BYTE) & SD_SKS_Valid_BIT)
#define SD_SKS_Valid  SD_Sense_Key_Specific_Valid
#define SD_SKS_CDB_Error(sdp)  \
    (int)(*((u8*)(sdp)+SD_SKS2_BYTE) & SD_SKS_Cmd_Data_BIT)
#define SD_Was_Illegal_Request  SD_SKS_CDB_Error
#define SD_SKS_Bit_Pointer_Valid(sdp)  \
    (int)(*((u8*)(sdp)+SD_SKS2_BYTE) & SD_SKS_Bit_Ptr_Valid_BIT)
#define SD_SKS_Bit_Pointer(sdp)  \
    (int)(*((u8*)(sdp)+SD_SKS2_BYTE) & SD_SKS_Bit_Ptr_MASK)
#define SD_Field_Pointer(sdp)  \
    (int)( ((u16)(*((u8*)(sdp)+SD_SKS1_BYTE)) << 8) \
      + *((u8*)(sdp)+SD_SKS0_BYTE) )
#define SD_Bad_Byte  SD_Field_Pointer
#define SD_Actual_Retry_Count  SD_Field_Pointer
#define SD_Progress_Indication  SD_Field_Pointer

/*
 *  Mode Sense Write Protect Mask
 */
#define WRITE_PROTECT_MASK      0X80

/*
 *  Medium Type Codes
 */
#define OPTICAL_DEFAULT                 0x00
#define OPTICAL_READ_ONLY_MEDIUM        0x01
#define OPTICAL_WRITE_ONCE_MEDIUM       0x02
#define OPTICAL_READ_WRITABLE_MEDIUM    0x03
#define OPTICAL_RO_OR_WO_MEDIUM         0x04
#define OPTICAL_RO_OR_RW_MEDIUM         0x05
#define OPTICAL_WO_OR_RW_MEDIUM         0x06



/*
 *    Structure definition for READ6, WRITE6 (6-byte CDB)
 */
typedef struct SCSI_RW6_CDB
{
    u32    OpCode      :8,
           LBA_HI      :5,    /* 5 MSBit's of the LBA */
           Lun         :3,
           LBA_MID     :8,    /* NOTE: total of 21 bits in LBA */
           LBA_LO      :8  ;  /* Max LBA = 0x001fffff          */
    u8     BlockCount;
    u8     Control;
} SCSI_RW6_t;

#define MAX_RW6_LBA  ((u32)0x001fffff)

/*
 *  Structure definition for READ10, WRITE10 (10-byte CDB)
 *
 *    NOTE: ParityCheck bit is applicable only for VERIFY and WRITE VERIFY for
 *    the ADP-92 DAC only.  In the SCSI2 spec. this same bit is defined as a
 *    FUA (forced unit access) bit for READs and WRITEs.  Since this driver
 *    does not use the FUA, this bit is defined as it is used by the ADP-92.
 *    Also, for READ CAPACITY, only the OpCode field is used.
 */
typedef struct SCSI_RW10_CDB
{
    u8     OpCode;
    u8     Reserved1;
    u32    LBA;
    u8     Reserved2;
    u16    BlockCount;
    u8     Control;
} SCSI_RW10_t;

#define PARITY_CHECK  0x08    /* parity check bit - byte[1], bit 3 */

    /*
     *  Structure definition for data returned by READ CAPACITY cmd;
     *  READ CAPACITY data
     */
    typedef struct READ_CAP_DATA
    {
        u32    MaxLBA;
        u32    BlockBytes;
    } SCSI_READ_CAP_DATA_t, *pSCSI_READ_CAP_DATA_t;


/*
 *  Structure definition for FORMAT UNIT CDB (6-byte CDB)
 */
typedef struct _SCSI_FORMAT_UNIT
{
    u8     OpCode;
    u8     Reserved1;
    u8     VendorSpecific;
    u16    Interleave;
    u8     Control;
} SCSI_FORMAT_UNIT_t;

/*
 *    Structure definition for REQUEST SENSE (6-byte CDB)
 */
typedef struct _SCSI_REQUEST_SENSE
{
    u8     OpCode;
    u8     Reserved1;
    u8     Reserved2;
    u8     Reserved3;
    u8     AllocLength;
    u8     Control;
} SCSI_REQ_SENSE_t;

/*
 *  Structure definition for REPORT LUNS (12-byte CDB)
 */
typedef struct _SCSI_REPORT_LUNS
{
    u8     OpCode;
    u8     Reserved1[5];
    u32    AllocationLength;
    u8     Reserved2;
    u8     Control;
} SCSI_REPORT_LUNS_t, *pSCSI_REPORT_LUNS_t;

    /*
     *  (per-level) LUN information bytes
     */
/*
 *  Following doesn't work on ARMCC compiler
 *  [apparently] because it pads every struct
 *  to be multiple of 4 bytes!
 *  So SCSI_LUN_LEVELS_t winds up being 16
 *  bytes instead of 8!
 *
    typedef struct LUN_INFO
    {
        u8     AddrMethod_plus_LunOrBusNumber;
        u8     LunOrTarget;
    } SCSI_LUN_INFO_t, *pSCSI_LUN_INFO_t;

    typedef struct LUN_LEVELS
    {
        SCSI_LUN_INFO_t  LUN_0;
        SCSI_LUN_INFO_t  LUN_1;
        SCSI_LUN_INFO_t  LUN_2;
        SCSI_LUN_INFO_t  LUN_3;
    } SCSI_LUN_LEVELS_t, *pSCSI_LUN_LEVELS_t;
*/
    /*
     *  All 4 levels (8 bytes) of LUN information
     */
    typedef struct LUN_LEVELS
    {
        u8     LVL1_AddrMethod_plus_LunOrBusNumber;
        u8     LVL1_LunOrTarget;
        u8     LVL2_AddrMethod_plus_LunOrBusNumber;
        u8     LVL2_LunOrTarget;
        u8     LVL3_AddrMethod_plus_LunOrBusNumber;
        u8     LVL3_LunOrTarget;
        u8     LVL4_AddrMethod_plus_LunOrBusNumber;
        u8     LVL4_LunOrTarget;
    } SCSI_LUN_LEVELS_t, *pSCSI_LUN_LEVELS_t;

    /*
     *  Structure definition for data returned by REPORT LUNS cmd;
     *  LUN reporting parameter list format
     */
    typedef struct LUN_REPORT
    {
        u32                LunListLength;
        u32                Reserved;
        SCSI_LUN_LEVELS_t  LunInfo[1];
    } SCSI_LUN_REPORT_t, *pSCSI_LUN_REPORT_t;

/****************************************************************************
 *
 *  Externals
 */

/****************************************************************************
 *
 *  Public Typedefs & Related Defines
 */

/****************************************************************************
 *
 *  Macros (embedded, above)
 */

/****************************************************************************
 *
 *  Public Variables
 */

/****************************************************************************
 *
 *  Public Prototypes (module entry points)
 */


/***************************************************************************/
#endif
