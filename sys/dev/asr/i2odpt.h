/*-
 *****************************************************************
 * Copyright (c) 1996-2000 Distributed Processing Technology Corporation
 * Copyright (c) 2000 Adaptec Corporation.
 * All rights reserved.
 *
 * $FreeBSD: src/sys/dev/asr/i2odpt.h,v 1.6.18.1 2008/11/25 02:59:29 kensmith Exp $
 *
 ****************************************************************/

#if !defined(I2O_DPT_HDR)
#define	I2O_DPT_HDR

#define	DPT_ORGANIZATION_ID 0x1B	/* For Private Messages */

/*
 *	PrivateMessageFrame.StdMessageFrame.Function = I2O_PRIVATE_MESSAGE
 *	PrivateMessageFrame.XFunctionCode = I2O_SCSI_SCB_EXEC
 */

typedef struct _PRIVATE_SCSI_SCB_EXECUTE_MESSAGE {
    I2O_PRIVATE_MESSAGE_FRAME PrivateMessageFrame;
#   if (defined(sparc) || defined(_DPT_BIG_ENDIAN))
	U32		      TID; /* Upper four bits currently are zero */
#   else
	BF		      TID:16; /* Upper four bits currently are zero */
	/* Command is interpreted by the host */
	BF		      Interpret:1;
	/* if TRUE, deal with Physical Firmware Array information */
	BF		      Physical:1;
	BF		      Reserved1:14;
#   endif
    U8			      CDBLength;
    U8			      Reserved;
    I2O_SCB_FLAGS	      SCBFlags;
    U8			      CDB[  I2O_SCSI_CDB_LENGTH	 ];
    U32			      ByteCount;
    I2O_SG_ELEMENT	      SGL;
} PRIVATE_SCSI_SCB_EXECUTE_MESSAGE, * PPRIVATE_SCSI_SCB_EXECUTE_MESSAGE;

/*
 * Flash access and programming messages
 *	PrivateMessageFrame.StdMessageFrame.Function = I2O_PRIVATE_MESSAGE
 *	PrivateMessageFrame.XFunctionCode = PRIVATE_FLAGS_REGION_*
 *
 *	SIZE	returns the total size of a region of flash
 *	READ	copies a region (or portion thereof) into the buffer specified
 *		by the SGL
 *	WRITE	writes a region (or portion thereof) using the data specified
 *		by the SGL
 *
 * Flash regions
 *
 *	0		operational-mode firmware
 *	1		software (bios/utility)
 *	2		oem nvram defaults
 *	3		hba serial number
 *	4		boot-mode firmware
 *
 * Any combination of RegionOffset and ByteCount can be specified providing
 * they fit within the size of the specified region.
 *
 * Flash messages should be targeted to the Executive TID 0x000
 */

#define	PRIVATE_FLASH_REGION_SIZE	0x0100
#define	PRIVATE_FLASH_REGION_READ	0x0101
#define	PRIVATE_FLASH_REGION_WRITE	0x0102
#define	PRIVATE_FLASH_REGION_CRC	0x0103

typedef struct _PRIVATE_FLASH_REGION_MESSAGE {
    I2O_PRIVATE_MESSAGE_FRAME PrivateMessageFrame;
    U32			      FlashRegion;
    U32			      RegionOffset;
    U32			      ByteCount;
    I2O_SG_ELEMENT	      SGL;
} PRIVATE_FLASH_REGION_MESSAGE, * PPRIVATE_FLASH_REGION_MESSAGE;

/* DPT Driver Printf message */

#define	PRIVATE_DRIVER_PRINTF 0x0200

/* FwPrintFlags */
#define	FW_FIRMWARE_FLAGS_NO_HEADER_B	0x00000001 /* Remove date header */

typedef struct _PRIVATE_DRIVER_PRINTF_MESSAGE {

    I2O_PRIVATE_MESSAGE_FRAME	PrivateMessageFrame;

    /* total bytes in PrintBuffer, including header */
    U32			PrintBufferByteCount;
    /* exact data to be copied into the serial PrintBuffer */
    U8			PrintBuffer[1];

} PRIVATE_DRIVER_PRINTF_MESSAGE, * PPRIVATE_DRIVER_PRINTF_MESSAGE;

/* DPT Enable Diagnostics message 0x0201 */

#define	PRIVATE_DIAG_ENABLE 0x0201

typedef struct _PRIVATE_DIAG_ENABLE_MESSAGE {
	I2O_PRIVATE_MESSAGE_FRAME	PrivateMessageFrame;
} PRIVATE_DIAG_MESSAGE_FRAME, * PPRIVATE_DIAG_MESSAGE_FRAME;

/* DPT Driver Get/Put message */

#define	PRIVATE_DRIVER_GET	0x300
#define	PRIVATE_DRIVER_PUT	0x301

typedef struct _PRIVATE_DRIVER_GETPUT_MESSAGE
{
	I2O_PRIVATE_MESSAGE_FRAME	PrivateMessageFrame;
	U32				Offset;
	U32				ByteCount;
	I2O_SG_ELEMENT			SGL;
} PRIVATE_DRIVER_GETPUT_MESSAGE, * PPRIVATE_DRIVER_GETPUT_MESSAGE;

/****************************************************************************/

/* DPT Peripheral Device Parameter Groups */

/****************************************************************************/

/* DPT Configuration and Operating Structures and Defines */

#define	    I2O_DPT_DEVICE_INFO_GROUP_NO	       0x8000

/* - 8000h - DPT Device Information Parameters Group defines */

/* Device Type */

#define	I2O_DPT_DEVICE_TYPE_DIRECT	  I2O_SCSI_DEVICE_TYPE_DIRECT
#define	I2O_DPT_DEVICE_TYPE_SEQUENTIAL	  I2O_SCSI_DEVICE_TYPE_SEQUENTIAL
#define	I2O_DPT_DEVICE_TYPE_PRINTER	  I2O_SCSI_DEVICE_TYPE_PRINTER
#define	I2O_DPT_DEVICE_TYPE_PROCESSOR	  I2O_SCSI_DEVICE_TYPE_PROCESSOR
#define	I2O_DPT_DEVICE_TYPE_WORM	  I2O_SCSI_DEVICE_TYPE_WORM
#define	I2O_DPT_DEVICE_TYPE_CDROM	  I2O_SCSI_DEVICE_TYPE_CDROM
#define	I2O_DPT_DEVICE_TYPE_SCANNER	  I2O_SCSI_DEVICE_TYPE_SCANNER
#define	I2O_DPT_DEVICE_TYPE_OPTICAL	  I2O_SCSI_DEVICE_TYPE_OPTICAL
#define	I2O_DPT_DEVICE_TYPE_MEDIA_CHANGER I2O_SCSI_DEVICE_TYPE_MEDIA_CHANGER
#define	I2O_DPT_DEVICE_TYPE_COMM	  I2O_SCSI_DEVICE_TYPE_COMM
#define	I2O_DPT_DEVICE_GRAPHICS_1	  I2O_SCSI_DEVICE_GRAPHICS_1
#define	I2O_DPT_DEVICE_GRAPHICS_2	  I2O_SCSI_DEVICE_GRAPHICS_2
#define	I2O_DPT_DEVICE_TYPE_ARRAY_CONT	  I2O_SCSI_DEVICE_TYPE_ARRAY_CONT
#define	I2O_DPT_DEVICE_TYPE_UNKNOWN	  I2O_SCSI_DEVICE_TYPE_UNKNOWN

/* Flags */

#define	I2O_DPT_PERIPHERAL_TYPE_FLAG	  I2O_SCSI_PERIPHERAL_TYPE_FLAG
#define	I2O_DPT_PERIPHERAL_TYPE_PARALLEL  I2O_SCSI_PERIPHERAL_TYPE_PARALLEL
#define	I2O_DPT_PERIPHERAL_TYPE_SERIAL	  I2O_SCSI_PERIPHERAL_TYPE_SERIAL

#define	I2O_DPT_RESERVED_FLAG		  I2O_SCSI_RESERVED_FLAG

#define	I2O_DPT_DISCONNECT_FLAG		  I2O_SCSI_DISCONNECT_FLAG
#define	I2O_DPT_DISABLE_DISCONNECT	  I2O_SCSI_DISABLE_DISCONNECT
#define	I2O_DPT_ENABLE_DISCONNECT	  I2O_SCSI_ENABLE_DISCONNECT

#define	I2O_DPT_MODE_MASK		  I2O_SCSI_MODE_MASK
#define	I2O_DPT_MODE_SET_DATA		  I2O_SCSI_MODE_SET_DATA
#define	I2O_DPT_MODE_SET_DEFAULT	  I2O_SCSI_MODE_SET_DEFAULT
#define	I2O_DPT_MODE_SET_SAFEST		  I2O_SCSI_MODE_SET_SAFEST

#define	I2O_DPT_DATA_WIDTH_MASK		  I2O_SCSI_DATA_WIDTH_MASK
#define	I2O_DPT_DATA_WIDTH_8		  I2O_SCSI_DATA_WIDTH_8
#define	I2O_DPT_DATA_WIDTH_16		  I2O_SCSI_DATA_WIDTH_16
#define	I2O_DPT_DATA_WIDTH_32		  I2O_SCSI_DATA_WIDTH_32

#define	I2O_DPT_SYNC_NEGOTIATION_FLAG	  I2O_SCSI_SYNC_NEGOTIATION_FLAG
#define	I2O_DPT_DISABLE_SYNC_NEGOTIATION  I2O_SCSI_DISABLE_SYNC_NEGOTIATION
#define	I2O_DPT_ENABLE_SYNC_NEGOTIATION	  I2O_SCSI_ENABLE_SYNC_NEGOTIATION

/* DPT Device Group 8000h - Device Information Parameter Group */

typedef struct _I2O_DPT_DEVICE_INFO_SCALAR {
    U8		DeviceType;	/* Identical to I2O_SCSI_DEVICE_INFO SCALAR */
    U8		Flags;		/* Identical to I2O_SCSI_DEVICE_INFO SCALAR */
    U16		Bus;
    U32		Identifier;
    U8		LunInfo[8]; /* SCSI-2 8-bit scalar LUN goes into offset 1 */

} I2O_DPT_DEVICE_INFO_SCALAR, *PI2O_DPT_DEVICE_INFO_SCALAR;

#define	I2O_DPT_EXEC_IOP_BUFFERS_GROUP_NO    0x8000

/* DPT Exec Iop Buffers Group 8000h */

typedef struct _I2O_DPT_EXEC_IOP_BUFFERS_SCALAR {
    U32	     SerialOutputOffset;    /* offset from base address to header   */
    U32	     SerialOutputSize;	    /* size of data buffer in bytes	    */
    U32	     SerialHeaderSize;	    /* size of data buffer header in bytes  */
    U32	     SerialFlagsSupported;  /* Mask of debug flags supported	    */

} I2O_DPT_EXEC_IOP_BUFFERS_SCALAR, *PI2O_DPT_EXEC_IOP_BUFFERS_SCALAR;


#endif /* I2O_DPT_HDR */
