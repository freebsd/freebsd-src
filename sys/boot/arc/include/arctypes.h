/*
 * Copyright (c) 1999, Stefan Esser <se@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 */

#ifndef _ARC_TYPES_H_
#define _ARC_TYPES_H_

#define ESUCCESS 0

typedef u_int8_t	BOOLEAN;	
typedef u_int16_t	WCHAR;

typedef int64_t		fpos_t;

/* XXX the following types are defined in ARC but are not currently used */

#if 0

typedef void		VOID;

typedef int8_t		CHAR;
typedef int16_t		SHORT;
typedef int32_t		LONG;
typedef int64_t		LONGLONG;

typedef u_int8_t	UCHAR;
typedef u_int16_t	USHORT;
typedef u_int32_t	ULONG;
typedef u_int64_t	ULONGLONG;

/*
 * The following ARC types conflict with <sys/types.h> !
 * They are not used in the ARC wrapper functions or ARC structs
 * currently, and thus may be left alone for now. In case the 
 * wrappers or structs are converted to use them, it is suggested
 * to prefix them with "arc" to avoid type clashes (if linking
 * against libstand.a which expects the FreeBSD declarations).
 */

typedef u_int32_t	size_t;
typedef int64_t		clock_t;
typedef int64_t		off_t;
typedef int32_t		time_t;

#endif /* 0 */

typedef struct {
   int32_t adr;
} arcptr;

typedef struct {
   u_int32_t	SPBSignature;
   u_int32_t	SPBLength;
   u_int16_t	Version;
   u_int16_t	Revision;
   arcptr	RestartBlockP;
   arcptr	DebugBlockP;
   arcptr	GEVectorP;
   arcptr	ULTBMissVectorP;
   u_int32_t	FirmwareVectorLength;
   arcptr	FirmwareVectorP;
   u_int32_t	PrivateVectorLength;
   arcptr	PrivateVectorP;
   u_int32_t	AdapterCount;
   struct {
      u_int32_t	AdapterType;
      u_int32_t	AdapterVectorLength;
      arcptr	AdapterVectorP;
   } Adapters[1];
} SPB;

/* ARC function specific data types */

typedef enum{
    SystemClass,
    ProcessorClass,
    CacheClass,
    AdapterClass,
    ControllerClass,
    PeripheralClass,
    MemoryClass,
    MaximumClass
} CONFIGURATION_CLASS;

typedef enum {
    ArcSystem,
    CentralProcessor,
    FloatingPointProcessor,
    PrimaryIcache,
    PrimaryDcache,
    SecondaryIcache,
    SecondaryDcache,
    SecondaryCache,
    EisaAdapter,
    TcAdapter,
    ScsiAdapter,
    DtiAdapter,
    MultiFunctionAdapter,
    DiskController,
    TapeController,
    CdromController,
    WormController,
    SerialController,
    NetworkController,
    DisplayController,
    ParallelController,
    PointerController,
    KeyboardController,
    AudioController,
    OtherController,
    DiskPeripheral,
    FloppyDiskPeripheral,
    TapePeripheral,
    ModemPeripheral,
    MonitorPeripheral,
    PrinterPeripheral,
    PointerPeripheral,
    KeyboardPeripheral,
    TerminalPeripheral,
    OtherPeripheral,
    LinePeripheral,
    NetworkPeripheral,
    SystemMemory,
    MaximumType
} CONFIGURATION_TYPE, *PCONFIGURATION_TYPE;

typedef enum {
    Failed =		0x01,
    ReadOnly =		0x02,
    Removable =		0x04,
    ConsoleIn =		0x08,
    ConsoleOut =	0x10,
    Input =		0x20,
    Output =		0x40
} IDENTIFIERFLAG;

typedef struct {
    CONFIGURATION_CLASS	Class;
    CONFIGURATION_TYPE	Type;
    IDENTIFIERFLAG	Flags;
    u_int16_t		Version;
    u_int16_t		Revision;
    u_int32_t		Key;
    u_int32_t		AffinityMask;
    u_int32_t		ConfigurationDataLength;
    u_int32_t		IdentifierLength;
    arcptr		Identifier;
} CONFIGURATION_COMPONENT, *PCONFIGURATION_COMPONENT;

typedef struct {
    int8_t		VendorId[8];
    int8_t		ProductId[8];
} SYSTEM_ID;

typedef enum {
    MemoryExceptionBlock,
    MemorySystemBlock,
    MemoryFree,
    MemoryBad,
    MemoryLoadedProgram,
    MemoryFirmwareTemporary,
    MemoryFirmwarePermanent,
    MemoryFreeContiguous,
    MemorySpecialMemory,
    MemoryMaximum
} MEMORY_TYPE;

typedef struct {
    MEMORY_TYPE Type;
    u_int32_t		BasePage;
    u_int32_t		PageCount;
} MEMORY_DESCRIPTOR;

typedef struct _TIME_FIELDS{
    u_int16_t		Year;		/* 1601 .. */
    u_int16_t		Month;		/* 1 .. 12 */
    u_int16_t		Day;		/* 1 .. 31 */
    u_int16_t		Hour;		/* 0 .. 23 */
    u_int16_t		Minute;		/* 0 .. 59 */
    u_int16_t		Second;		/* 0 .. 59 */
    u_int16_t		Milliseconds;	/* 0 .. 999 */
    u_int16_t		Weekday;	/* 0 .. 6 = Sunday .. Saturday  */
} TIME_FIELDS, *PTIME_FIELDS;

#define StandardIn	0
#define StandardOut	1

#define ReadOnlyFile	0x01
#define HiddenFile	0x02
#define SystemFile	0x04
#define ArchiveFile	0x08
#define DirectoryFile	0x10
#define DeleteFile	0x20

typedef struct {
    u_int32_t		FileNameLength;
    u_int8_t		FileAttribute;
    int8_t		FileName[32];
} DIRECTORY_ENTRY;

typedef enum {
    OpenReadOnly,
    OpenWriteOnly,
    OpenReadWrite,
    CreateWriteOnly,
    CreateReadWrite,
    SupersedeWriteOnly,
    SupersedeReadWrite,
    OpenDirectory,
    CreateDirectory,
    OpenMaximumMode
} OPEN_MODE;

typedef enum {
    SeekAbsolute,
    SeekRelative,
    SeekMaximum
} SEEK_MODE;

typedef enum {
    MountLoadMedia,
    MountUnloadMedia,
    MountMaximum
} MOUNT_OPERATION;

typedef struct {
    fpos_t		StartingAddress;
    fpos_t		EndingAddress;
    fpos_t		CurrentAddress;
    CONFIGURATION_TYPE	Type;
    u_int32_t		FileNameLength;
    u_int8_t		Attributes;
    int8_t		FileName[32];
} FILE_INFORMATION;

typedef struct {
    u_int16_t		CursorXPosition;
    u_int16_t		CursorYPosition;
    u_int16_t		CursorMaxXPosition;
    u_int16_t		CursorMaxYPosition;
    u_int8_t		ForegroundColor;
    u_int8_t		BackgroundColor;
    BOOLEAN		HighIntensity;
    BOOLEAN		Underscored;
    BOOLEAN		ReverseVideo;
} ARC_DISPLAY_STATUS;

/* vendor function specific data types */

typedef struct {
    u_int32_t	ProcessorId;
    u_int32_t	ProcessorRevision;
    u_int32_t	ProcessorPageSize;
    u_int32_t	NumberOfPhysicalAddressBits;
    u_int32_t	MaximumAddressSpaceNumber;
    u_int32_t	ProcessorCycleCounterPeriod;
    u_int32_t	SystemRevision;
    u_int8_t	SystemSerialNumber[16];
    u_int8_t	FirmwareVersion[16];
    u_int8_t	FirmwareBuildTimeStamp[12];
} EXTENDED_SYSTEM_INFORMATION, *PEXTENDED_SYSTEM_INFORMATION; 

#endif /* _ARC_TYPES_H_ */
