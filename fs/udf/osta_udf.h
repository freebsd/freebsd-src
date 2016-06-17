/*
 * osta_udf.h
 *
 * This file is based on OSTA UDF(tm) 2.01 (March 15, 2000)
 * http://www.osta.org
 *
 * Copyright (c) 2001-2002  Ben Fennema <bfennema@falcon.csc.calpoly.edu>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU Public License ("GPL").
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "ecma_167.h"

#ifndef _OSTA_UDF_H
#define _OSTA_UDF_H 1

/* OSTA CS0 Charspec (UDF 2.01 2.1.2) */
#define UDF_CHAR_SET_TYPE		0
#define UDF_CHAR_SET_INFO		"OSTA Compressed Unicode"

/* Entity Identifier (UDF 2.01 2.1.5) */
/* Identifiers (UDF 2.01 2.1.5.2) */
#define UDF_ID_DEVELOPER		"*Linux UDFFS"
#define	UDF_ID_COMPLIANT		"*OSTA UDF Compliant"
#define UDF_ID_LV_INFO			"*UDF LV Info"
#define UDF_ID_FREE_EA			"*UDF FreeEASpace"
#define UDF_ID_FREE_APP_EA		"*UDF FreeAppEASpace"
#define UDF_ID_DVD_CGMS			"*UDF DVD CGMS Info"
#define UDF_ID_OS2_EA			"*UDF OS/2 EA"
#define UDF_ID_OS2_EA_LENGTH		"*UDF OS/2 EALength"
#define UDF_ID_MAC_VOLUME		"*UDF Mac VolumeInfo"
#define UDF_ID_MAC_FINDER		"*UDF Mac FinderInfo"
#define UDF_ID_MAC_UNIQUE		"*UDF Mac UniqueIDTable"
#define UDF_ID_MAC_RESOURCE		"*UDF Mac ResourceFork"
#define UDF_ID_VIRTUAL			"*UDF Virtual Partition"
#define UDF_ID_SPARABLE			"*UDF Sparable Partition"
#define UDF_ID_ALLOC			"*UDF Virtual Alloc Tbl"
#define UDF_ID_SPARING			"*UDF Sparing Table"

/* Identifier Suffix (UDF 2.01 2.1.5.3) */
#define IS_DF_HARD_WRITE_PROTECT	0x01
#define IS_DF_SOFT_WRITE_PROTECT	0x02

struct UDFIdentSuffix
{
	uint16_t	UDFRevision;
	uint8_t		OSClass;
	uint8_t		OSIdentifier;
	uint8_t		reserved[4];
} __attribute__ ((packed));

struct impIdentSuffix
{
	uint8_t		OSClass;
	uint8_t		OSIdentifier;
	uint8_t		reserved[6];
} __attribute__ ((packed));

struct appIdentSuffix
{
	uint8_t		impUse[8];
} __attribute__ ((packed));
 
/* Logical Volume Integrity Descriptor (UDF 2.01 2.2.6) */
/* Implementation Use (UDF 2.01 2.2.6.4) */
struct logicalVolIntegrityDescImpUse
{
	regid		impIdent;
	uint32_t	numFiles;
	uint32_t	numDirs;
	uint16_t	minUDFReadRev;
	uint16_t	minUDFWriteRev;
	uint16_t	maxUDFWriteRev;
	uint8_t		impUse[0];
} __attribute__ ((packed));

/* Implementation Use Volume Descriptor (UDF 2.01 2.2.7) */
/* Implementation Use (UDF 2.01 2.2.7.2) */
struct impUseVolDescImpUse
{
	charspec	LVICharset;
	dstring		logicalVolIdent[128];
	dstring		LVInfo1[36];
	dstring		LVInfo2[36];
	dstring		LVInfo3[36];
	regid		impIdent;
	uint8_t		impUse[128];
} __attribute__ ((packed));

struct udfPartitionMap2
{
	uint8_t		partitionMapType;
	uint8_t		partitionMapLength;
	uint8_t		reserved1[2];
	regid		partIdent;
	uint16_t	volSeqNum;
	uint16_t	partitionNum;
} __attribute__ ((packed));

/* Virtual Partition Map (UDF 2.01 2.2.8) */
struct virtualPartitionMap
{
	uint8_t		partitionMapType;
	uint8_t		partitionMapLength;
	uint8_t		reserved1[2];
	regid		partIdent;
	uint16_t	volSeqNum;
	uint16_t	partitionNum;
	uint8_t		reserved2[24];
} __attribute__ ((packed));

/* Sparable Partition Map (UDF 2.01 2.2.9) */
struct sparablePartitionMap
{
	uint8_t		partitionMapType;
	uint8_t		partitionMapLength;
	uint8_t		reserved1[2];
	regid		partIdent;
	uint16_t	volSeqNum;
	uint16_t	partitionNum;
	uint16_t	packetLength;
	uint8_t		numSparingTables;
	uint8_t		reserved2[1];
	uint32_t	sizeSparingTable;
	uint32_t	locSparingTable[4];
} __attribute__ ((packed));

/* Virtual Allocation Table (UDF 1.5 2.2.10) */
struct virtualAllocationTable15
{
	uint32_t	VirtualSector[0];
	regid		ident;
	uint32_t	previousVATICB;
} __attribute__ ((packed));  

#define ICBTAG_FILE_TYPE_VAT15		0x00U

/* Virtual Allocation Table (UDF 2.01 2.2.10) */
struct virtualAllocationTable20
{
	uint16_t	lengthHeader;
	uint16_t	lengthImpUse;
	dstring		logicalVolIdent[128];
	uint32_t	previousVatICBLoc;
	uint32_t	numFIDSFiles;
	uint32_t	numFIDSDirectories;
	uint16_t	minReadRevision;
	uint16_t	minWriteRevision;
	uint16_t	maxWriteRevision;
	uint16_t	reserved;
	uint8_t		impUse[0];
	uint32_t	vatEntry[0];
} __attribute__ ((packed));

#define ICBTAG_FILE_TYPE_VAT20		0xF8U

/* Sparing Table (UDF 2.01 2.2.11) */
struct sparingEntry
{
	uint32_t	origLocation;
	uint32_t	mappedLocation;
} __attribute__ ((packed));

struct sparingTable
{
	tag 		descTag;
	regid		sparingIdent;
	uint16_t	reallocationTableLen;
	uint16_t	reserved;
	uint32_t	sequenceNum;
	struct sparingEntry
			mapEntry[0];
} __attribute__ ((packed));

/* struct long_ad ICB - ADImpUse (UDF 2.01 2.2.4.3) */
struct allocDescImpUse
{
	uint16_t	flags;
	uint8_t		impUse[4];
} __attribute__ ((packed));

#define AD_IU_EXT_ERASED		0x0001

/* Real-Time Files (UDF 2.01 6.11) */
#define ICBTAG_FILE_TYPE_REALTIME	0xF9U

/* Implementation Use Extended Attribute (UDF 2.01 3.3.4.5) */
/* FreeEASpace (UDF 2.01 3.3.4.5.1.1) */
struct freeEaSpace
{
	uint16_t	headerChecksum;
	uint8_t		freeEASpace[0];
} __attribute__ ((packed));

/* DVD Copyright Management Information (UDF 2.01 3.3.4.5.1.2) */
struct DVDCopyrightImpUse 
{
	uint16_t	headerChecksum;
	uint8_t		CGMSInfo;
	uint8_t		dataType;
	uint8_t		protectionSystemInfo[4];
} __attribute__ ((packed));

/* Application Use Extended Attribute (UDF 2.01 3.3.4.6) */
/* FreeAppEASpace (UDF 2.01 3.3.4.6.1) */
struct freeAppEASpace
{
	uint16_t	headerChecksum;
	uint8_t		freeEASpace[0];
} __attribute__ ((packed));

/* UDF Defined System Stream (UDF 2.01 3.3.7) */
#define UDF_ID_UNIQUE_ID		"*UDF Unique ID Mapping Data"
#define UDF_ID_NON_ALLOC		"*UDF Non-Allocatable Space"
#define UDF_ID_POWER_CAL		"*UDF Power Cal Table"
#define UDF_ID_BACKUP			"*UDF Backup"

/* Operating System Identifiers (UDF 2.01 6.3) */
#define UDF_OS_CLASS_UNDEF		0x00U
#define UDF_OS_CLASS_DOS		0x01U
#define UDF_OS_CLASS_OS2		0x02U
#define UDF_OS_CLASS_MAC		0x03U
#define UDF_OS_CLASS_UNIX		0x04U
#define UDF_OS_CLASS_WIN9X		0x05U
#define UDF_OS_CLASS_WINNT		0x06U
#define UDF_OS_CLASS_OS400		0x07U
#define UDF_OS_CLASS_BEOS		0x08U
#define UDF_OS_CLASS_WINCE		0x09U

#define UDF_OS_ID_UNDEF			0x00U
#define UDF_OS_ID_DOS			0x00U
#define UDF_OS_ID_OS2			0x00U
#define UDF_OS_ID_MAC			0x00U
#define UDF_OS_ID_UNIX			0x00U
#define UDF_OS_ID_AIX			0x01U
#define UDF_OS_ID_SOLARIS		0x02U
#define UDF_OS_ID_HPUX			0x03U
#define UDF_OS_ID_IRIX			0x04U
#define UDF_OS_ID_LINUX			0x05U
#define UDF_OS_ID_MKLINUX		0x06U
#define UDF_OS_ID_FREEBSD		0x07U
#define UDF_OS_ID_WIN9X			0x00U
#define UDF_OS_ID_WINNT			0x00U
#define UDF_OS_ID_OS400			0x00U
#define UDF_OS_ID_BEOS			0x00U
#define UDF_OS_ID_WINCE			0x00U

#endif /* _OSTA_UDF_H */
