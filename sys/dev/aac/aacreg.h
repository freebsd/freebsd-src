/*-
 * Copyright (c) 2000 Michael Smith
 * Copyright (c) 2000-2001 Scott Long
 * Copyright (c) 2000 BSDi
 * Copyright (c) 2001 Adaptec, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$FreeBSD$
 */

/*
 * Data structures defining the interface between the driver and the Adaptec
 * 'FSA' adapters.  Note that many field names and comments here are taken
 * verbatim from the Adaptec driver source in order to make comparing the
 * two slightly easier.
 */

/*
 * Misc. magic numbers.
 */
#define AAC_MAX_CONTAINERS	64
#define AAC_BLOCK_SIZE		512

/*
 * Communications interface.
 *
 * Where datastructure layouts are closely parallel to the Adaptec sample code,
 * retain their naming conventions (for now) to aid in cross-referencing.
 */

/*
 * We establish 4 command queues and matching response queues.  Queues must
 * be 16-byte aligned, and are sized as follows:
 */
#define AAC_HOST_NORM_CMD_ENTRIES	8	/* command adapter->host,
						 * normal priority */
#define AAC_HOST_HIGH_CMD_ENTRIES	4	/* command adapter->host,
						 * high priority */
#define AAC_ADAP_NORM_CMD_ENTRIES	512	/* command host->adapter,
						 * normal priority */
#define AAC_ADAP_HIGH_CMD_ENTRIES	4	/* command host->adapter,
						 * high priority */
#define AAC_HOST_NORM_RESP_ENTRIES	512	/* response, adapter->host,
						 * normal priority */
#define AAC_HOST_HIGH_RESP_ENTRIES	4	/* response, adapter->host,
						 * high priority */
#define AAC_ADAP_NORM_RESP_ENTRIES	8	/* response, host->adapter,
						 * normal priority */
#define AAC_ADAP_HIGH_RESP_ENTRIES	4	/* response, host->adapter,
						 * high priority */

#define AAC_TOTALQ_LENGTH	(AAC_HOST_HIGH_CMD_ENTRIES +	\
				 AAC_HOST_NORM_CMD_ENTRIES +	\
				 AAC_ADAP_HIGH_CMD_ENTRIES +	\
				 AAC_ADAP_NORM_CMD_ENTRIES +	\
				 AAC_HOST_HIGH_RESP_ENTRIES +	\
				 AAC_HOST_NORM_RESP_ENTRIES +	\
				 AAC_ADAP_HIGH_RESP_ENTRIES +	\
				 AAC_ADAP_NORM_RESP_ENTRIES)
#define AAC_QUEUE_COUNT		8
#define AAC_QUEUE_ALIGN		16

struct aac_queue_entry {
	u_int32_t	aq_fib_size;	/* FIB size in bytes */
	u_int32_t	aq_fib_addr;	/* receiver-space address of the FIB */
} __attribute__ ((packed));

#define AAC_PRODUCER_INDEX	0
#define AAC_CONSUMER_INDEX	1

/*
 * Table of queue indices and queues used to communicate with the
 * controller.  This structure must be aligned to AAC_QUEUE_ALIGN
 */
struct aac_queue_table {
	/* queue consumer/producer indexes (layout mandated by adapter) */
	u_int32_t			qt_qindex[AAC_QUEUE_COUNT][2];

	/* queue entry structures (layout mandated by adapter) */
	struct aac_queue_entry qt_HostNormCmdQueue [AAC_HOST_NORM_CMD_ENTRIES];
	struct aac_queue_entry qt_HostHighCmdQueue [AAC_HOST_HIGH_CMD_ENTRIES];
	struct aac_queue_entry qt_AdapNormCmdQueue [AAC_ADAP_NORM_CMD_ENTRIES];
	struct aac_queue_entry qt_AdapHighCmdQueue [AAC_ADAP_HIGH_CMD_ENTRIES];
	struct aac_queue_entry qt_HostNormRespQueue[AAC_HOST_NORM_RESP_ENTRIES];
	struct aac_queue_entry qt_HostHighRespQueue[AAC_HOST_HIGH_RESP_ENTRIES];
	struct aac_queue_entry qt_AdapNormRespQueue[AAC_ADAP_NORM_RESP_ENTRIES];
	struct aac_queue_entry qt_AdapHighRespQueue[AAC_ADAP_HIGH_RESP_ENTRIES];
} __attribute__ ((packed));

/*
 * Queue names
 *
 * Note that we base these at 0 in order to use them as array indices.  Adaptec
 * used base 1 for some unknown reason, and sorted them in a different order.
 */
#define AAC_HOST_NORM_CMD_QUEUE		0
#define AAC_HOST_HIGH_CMD_QUEUE		1
#define AAC_ADAP_NORM_CMD_QUEUE		2
#define AAC_ADAP_HIGH_CMD_QUEUE		3
#define AAC_HOST_NORM_RESP_QUEUE	4
#define AAC_HOST_HIGH_RESP_QUEUE	5
#define AAC_ADAP_NORM_RESP_QUEUE	6
#define AAC_ADAP_HIGH_RESP_QUEUE	7

/*
 * List structure used to chain FIBs (used by the adapter - we hang FIBs off
 * our private command structure and don't touch these)
 */
struct aac_fib_list_entry {
	struct fib_list_entry	*Flink;
	struct fib_list_entry	*Blink;
} __attribute__ ((packed));

/*
 * FIB (FSA Interface Block?); this is the datastructure passed between the host
 * and adapter.
 */
struct aac_fib_header {
	u_int32_t		XferState;
	u_int16_t		Command;
	u_int8_t		StructType;
	u_int8_t		Flags;
	u_int16_t		Size;
	u_int16_t		SenderSize;
	u_int32_t		SenderFibAddress;
	u_int32_t		ReceiverFibAddress;
	u_int32_t		SenderData;
	union {
		struct {
			u_int32_t	ReceiverTimeStart;
			u_int32_t	ReceiverTimeDone;
		} _s;
		struct aac_fib_list_entry FibLinks;
	} _u;
} __attribute__ ((packed));

#define AAC_FIB_DATASIZE	(512 - sizeof(struct aac_fib_header))

struct aac_fib {
	struct aac_fib_header	Header;
	u_int8_t			data[AAC_FIB_DATASIZE];
} __attribute__ ((packed));

/*
 * FIB commands
 */
typedef enum {
	TestCommandResponse =		1,
	TestAdapterCommand =		2,

	/* lowlevel and comm commands */
	LastTestCommand =		100,
	ReinitHostNormCommandQueue =	101,
	ReinitHostHighCommandQueue =	102,
	ReinitHostHighRespQueue =	103,
	ReinitHostNormRespQueue =	104,
	ReinitAdapNormCommandQueue =	105,
	ReinitAdapHighCommandQueue =	107,
	ReinitAdapHighRespQueue =	108,
	ReinitAdapNormRespQueue =	109,
	InterfaceShutdown =		110,
	DmaCommandFib =			120,
	StartProfile =			121,
	TermProfile =			122,
	SpeedTest =			123,
	TakeABreakPt =			124,
	RequestPerfData =		125,
	SetInterruptDefTimer=		126,
	SetInterruptDefCount=		127,
	GetInterruptDefStatus=		128,
	LastCommCommand =		129,

	/* filesystem commands */
	NuFileSystem =			300,
	UFS =				301,
	HostFileSystem =		302,
	LastFileSystemCommand =		303,

	/* Container Commands */
	ContainerCommand =		500,
	ContainerCommand64 =		501,

	/* Cluster Commands */
	ClusterCommand =		550,

	/* Scsi Port commands (scsi passthrough) */
	ScsiPortCommand =		600,

	/* misc house keeping and generic adapter initiated commands */
	AifRequest =			700,
	CheckRevision =			701,
	FsaHostShutdown =		702,
	RequestAdapterInfo =		703,
	IsAdapterPaused =		704,
	SendHostTime =			705,
	LastMiscCommand =		706
} AAC_FibCommands;

/*
 * FIB types
 */
#define AAC_FIBTYPE_TFIB	1
#define AAC_FIBTYPE_TQE		2
#define AAC_FIBTYPE_TCTPERF	3

/*
 * FIB transfer state
 */
#define AAC_FIBSTATE_HOSTOWNED		(1<<0)	/* owned by the host */
#define AAC_FIBSTATE_ADAPTEROWNED	(1<<1)	/* owned by the adapter */
#define AAC_FIBSTATE_INITIALISED	(1<<2)	/* initialised */
#define AAC_FIBSTATE_EMPTY		(1<<3)	/* empty */
#define AAC_FIBSTATE_FROMPOOL		(1<<4)	/* allocated from pool */
#define AAC_FIBSTATE_FROMHOST		(1<<5)	/* sent from the host */
#define AAC_FIBSTATE_FROMADAP		(1<<6)	/* sent from the adapter */
#define AAC_FIBSTATE_REXPECTED		(1<<7)	/* response is expected */
#define AAC_FIBSTATE_RNOTEXPECTED	(1<<8)	/* response is not expected */
#define AAC_FIBSTATE_DONEADAP		(1<<9)	/* processed by the adapter */
#define AAC_FIBSTATE_DONEHOST		(1<<10)	/* processed by the host */
#define AAC_FIBSTATE_HIGH		(1<<11)	/* high priority */
#define AAC_FIBSTATE_NORM		(1<<12)	/* normal priority */
#define AAC_FIBSTATE_ASYNC		(1<<13)
#define AAC_FIBSTATE_ASYNCIO		(1<<13)	/* to be removed */
#define AAC_FIBSTATE_PAGEFILEIO		(1<<14)	/* to be removed */
#define AAC_FIBSTATE_SHUTDOWN		(1<<15)
#define AAC_FIBSTATE_LAZYWRITE		(1<<16)	/* to be removed */
#define AAC_FIBSTATE_ADAPMICROFIB	(1<<17)
#define AAC_FIBSTATE_BIOSFIB		(1<<18)
#define AAC_FIBSTATE_FAST_RESPONSE	(1<<19)	/* fast response capable */
#define AAC_FIBSTATE_APIFIB		(1<<20)

/*
 * FIB error values
 */
#define AAC_ERROR_NORMAL			0x00
#define AAC_ERROR_PENDING			0x01
#define AAC_ERROR_FATAL				0x02
#define AAC_ERROR_INVALID_QUEUE			0x03
#define AAC_ERROR_NOENTRIES			0x04
#define AAC_ERROR_SENDFAILED			0x05
#define AAC_ERROR_INVALID_QUEUE_PRIORITY	0x06
#define AAC_ERROR_FIB_ALLOCATION_FAILED		0x07
#define AAC_ERROR_FIB_DEALLOCATION_FAILED	0x08

/*
 * Adapter Init Structure: this is passed to the adapter with the 
 * AAC_MONKER_INITSTRUCT command to point it at our control structures.
 */
struct aac_adapter_init {
	u_int32_t	InitStructRevision;
#define AAC_INIT_STRUCT_REVISION	3
	u_int32_t	MiniPortRevision;
	u_int32_t	FilesystemRevision;
	u_int32_t	CommHeaderAddress;
	u_int32_t	FastIoCommAreaAddress;
	u_int32_t	AdapterFibsPhysicalAddress;
	void		*AdapterFibsVirtualAddress;
	u_int32_t	AdapterFibsSize;
	u_int32_t	AdapterFibAlign;
	u_int32_t	PrintfBufferAddress;
	u_int32_t	PrintfBufferSize;
	u_int32_t	HostPhysMemPages;
	u_int32_t	HostElapsedSeconds;
} __attribute__ ((packed));

/*
 * Shared data types
 */
/*
 * Container types
 */
typedef enum {
	CT_NONE = 0,
	CT_VOLUME,
	CT_MIRROR,
	CT_STRIPE,
	CT_RAID5,
	CT_SSRW,
	CT_SSRO,
	CT_MORPH,
	CT_PASSTHRU,
	CT_RAID4,
	CT_RAID10,                  /* stripe of mirror */
	CT_RAID00,                  /* stripe of stripe */
	CT_VOLUME_OF_MIRRORS,       /* volume of mirror */
	CT_PSEUDO_RAID3,            /* really raid4 */
	CT_RAID50,		    /* stripe of raid5 */
} AAC_FSAVolType;

/*
 * Host-addressable object types
 */
typedef enum {
	FT_REG = 1,     /* regular file */
	FT_DIR,         /* directory */
	FT_BLK,         /* "block" device - reserved */
	FT_CHR,         /* "character special" device - reserved */
	FT_LNK,         /* symbolic link */
	FT_SOCK,        /* socket */
	FT_FIFO,        /* fifo */
	FT_FILESYS,     /* ADAPTEC's "FSA"(tm) filesystem */
	FT_DRIVE,       /* physical disk - addressable in scsi by b/t/l */
	FT_SLICE,       /* virtual disk - raw volume - slice */
	FT_PARTITION,   /* FSA partition - carved out of a slice - building
			 * block for containers */
	FT_VOLUME,      /* Container - Volume Set */
	FT_STRIPE,      /* Container - Stripe Set */
	FT_MIRROR,      /* Container - Mirror Set */
	FT_RAID5,       /* Container - Raid 5 Set */
	FT_DATABASE     /* Storage object with "foreign" content manager */
} AAC_FType;

/*
 * Host-side scatter/gather list for 32-bit commands.
 */
struct aac_sg_entry {
	u_int32_t	SgAddress;
	u_int32_t	SgByteCount;
} __attribute__ ((packed));

struct aac_sg_table {
	u_int32_t		SgCount;
	struct aac_sg_entry	SgEntry[0];
} __attribute__ ((packed));

/*
 * Host-side scatter/gather list for 64-bit commands.
 */
struct aac_sg_table64 {
	u_int8_t	SgCount;
	u_int8_t	SgSectorsPerPage;
	u_int16_t	SgByteOffset;
	u_int64_t	SgEntry[0];
} __attribute__ ((packed));

/*
 * Container creation data
 */
struct aac_container_creation {
	u_int8_t	ViaBuildNumber;
	u_int8_t	MicroSecond;
	u_int8_t	Via;		/* 1 = FSU, 2 = API, etc. */
	u_int8_t	YearsSince1900;
	u_int32_t	Month:4;	/* 1-12 */
	u_int32_t	Day:6;		/* 1-32 */
	u_int32_t	Hour:6;		/* 0-23 */
	u_int32_t	Minute:6;	/* 0-59 */
	u_int32_t	Second:6;	/* 0-59 */
	u_int64_t	ViaAdapterSerialNumber;
} __attribute__ ((packed));

/*
 * Revision number handling
 */

typedef enum {
	RevApplication = 1,
	RevDkiCli,
	RevNetService,
	RevApi,
	RevFileSysDriver,
	RevMiniportDriver,
	RevAdapterSW,
	RevMonitor,
	RevRemoteApi
} RevComponent;

struct FsaRevision {
	union {
		struct {
			u_int8_t	dash;
			u_int8_t	type;
			u_int8_t	minor;
			u_int8_t	major;
		} comp;
		u_int32_t	ul;
	} external;
	u_int32_t	buildNumber;
}  __attribute__ ((packed));

/*
 * Adapter Information
 */

typedef enum {
	CPU_NTSIM = 1,
	CPU_I960,
	CPU_ARM,
	CPU_SPARC,
	CPU_POWERPC,
	CPU_ALPHA,
	CPU_P7,
	CPU_I960_RX,
	CPU__last
} AAC_CpuType;  

typedef enum {
	CPUI960_JX = 1,
	CPUI960_CX,
	CPUI960_HX,
	CPUI960_RX,
	CPUARM_SA110,
	CPUARM_xxx,
	CPUMPC_824x,
	CPUPPC_xxx,
	CPUSUBTYPE__last
} AAC_CpuSubType;

typedef enum {
	PLAT_NTSIM = 1,
	PLAT_V3ADU,
	PLAT_CYCLONE,
	PLAT_CYCLONE_HD,
	PLAT_BATBOARD,
	PLAT_BATBOARD_HD,
	PLAT_YOLO,
	PLAT_COBRA,
	PLAT_ANAHEIM,
	PLAT_JALAPENO,
	PLAT_QUEENS,
	PLAT_JALAPENO_DELL,
	PLAT_POBLANO,
	PLAT_POBLANO_OPAL,
	PLAT_POBLANO_SL0,
	PLAT_POBLANO_SL1,
	PLAT_POBLANO_SL2,
	PLAT_POBLANO_XXX,
	PLAT_JALAPENO_P2,
	PLAT_HABANERO,
	PLAT__last
} AAC_Platform;

typedef enum {
	OEM_FLAVOR_ADAPTEC = 1,
	OEM_FLAVOR_DELL,
	OEM_FLAVOR_HP,
	OEM_FLAVOR_IBM,
	OEM_FLAVOR_CPQ,
	OEM_FLAVOR_BRAND_X,
	OEM_FLAVOR_BRAND_Y,
	OEM_FLAVOR_BRAND_Z,
	OEM_FLAVOR__last
} AAC_OemFlavor;

/*
 * XXX the aac-2622 with no battery present reports PLATFORM_BAT_OPT_PRESENT
 */
typedef enum
{ 
	PLATFORM_BAT_REQ_PRESENT = 1,	/* BATTERY REQUIRED AND PRESENT */
	PLATFORM_BAT_REQ_NOTPRESENT,	/* BATTERY REQUIRED AND NOT PRESENT */
	PLATFORM_BAT_OPT_PRESENT,	/* BATTERY OPTIONAL AND PRESENT */
	PLATFORM_BAT_OPT_NOTPRESENT,	/* BATTERY OPTIONAL AND NOT PRESENT */
	PLATFORM_BAT_NOT_SUPPORTED	/* BATTERY NOT SUPPORTED */
} AAC_BatteryPlatform;

/* 
 * options supported by this board
 * there has to be a one to one mapping of these defines and the ones in 
 * fsaapi.h, search for FSA_SUPPORT_SNAPSHOT
 */
#define AAC_SUPPORTED_SNAPSHOT		0x01
#define AAC_SUPPORTED_CLUSTERS		0x02
#define AAC_SUPPORTED_WRITE_CACHE	0x04
#define AAC_SUPPORTED_64BIT_DATA	0x08
#define AAC_SUPPORTED_HOST_TIME_FIB	0x10
#define AAC_SUPPORTED_RAID50		0x20

/* 
 * Structure used to respond to a RequestAdapterInfo fib.
 */
struct aac_adapter_info {
	AAC_Platform		PlatformBase;    /* adapter type */
	AAC_CpuType		CpuArchitecture; /* adapter CPU type */
	AAC_CpuSubType		CpuVariant;      /* adapter CPU subtype */
	u_int32_t		ClockSpeed;      /* adapter CPU clockspeed */
	u_int32_t		ExecutionMem;    /* adapter Execution Memory
						  * size */
	u_int32_t		BufferMem;       /* adapter Data Memory */
	u_int32_t		TotalMem;        /* adapter Total Memory */
	struct FsaRevision	KernelRevision;  /* adapter Kernel Software
						  * Revision */
	struct FsaRevision	MonitorRevision; /* adapter Monitor/Diagnostic
						  * Software Revision */
	struct FsaRevision	HardwareRevision;/* TBD */
	struct FsaRevision	BIOSRevision;    /* adapter BIOS Revision */
	u_int32_t		ClusteringEnabled;
	u_int32_t		ClusterChannelMask;
	u_int64_t		SerialNumber;
	AAC_BatteryPlatform	batteryPlatform;
	u_int32_t		SupportedOptions; /* supported features of this
						   * controller */
	AAC_OemFlavor	OemVariant;
} __attribute__ ((packed));

/*
 * Monitor/Kernel interface.
 */

/*
 * Synchronous commands to the monitor/kernel.
 */
#define AAC_MONKER_INITSTRUCT	0x05
#define AAC_MONKER_SYNCFIB	0x0c

/*
 *  Adapter Status Register
 *
 *  Phase Staus mailbox is 32bits:
 *  <31:16> = Phase Status
 *  <15:0>  = Phase
 *
 *  The adapter reports its present state through the phase.  Only
 *  a single phase should be ever be set.  Each phase can have multiple
 *  phase status bits to provide more detailed information about the
 *  state of the adapter.
 */
#define AAC_SELF_TEST_FAILED	0x00000004
#define AAC_UP_AND_RUNNING	0x00000080
#define AAC_KERNEL_PANIC	0x00000100

/*
 * Data types relating to control and monitoring of the NVRAM/WriteCache 
 * subsystem.
 */

#define AAC_NFILESYS	24	/* maximum number of filesystems */

/*
 * NVRAM/Write Cache subsystem states
 */
typedef enum {
	NVSTATUS_DISABLED = 0,	/* present, clean, not being used */
	NVSTATUS_ENABLED,	/* present, possibly dirty, ready for use */
	NVSTATUS_ERROR,		/* present, dirty, contains dirty data */
	NVSTATUS_BATTERY,	/* present, bad or low battery, may contain
				 * dirty data */
	NVSTATUS_UNKNOWN	/* for bad/missing device */
} AAC_NVSTATUS;

/*
 * NVRAM/Write Cache subsystem battery component states
 *
 */
typedef enum {
	NVBATTSTATUS_NONE = 0,	/* battery has no power or is not present */
	NVBATTSTATUS_LOW,	/* battery is low on power */
	NVBATTSTATUS_OK,	/* battery is okay - normal operation possible
				 * only in this state */
	NVBATTSTATUS_RECONDITIONING	/* no battery present - reconditioning
					 * in process */
} AAC_NVBATTSTATUS;

/*
 * Battery transition type
 */
typedef enum {
	NVBATT_TRANSITION_NONE = 0,	/* battery now has no power or is not
					 * present */
	NVBATT_TRANSITION_LOW,		/* battery is now low on power */
	NVBATT_TRANSITION_OK		/* battery is now okay - normal
					 * operation possible only in this
					 * state */
} AAC_NVBATT_TRANSITION;

/*
 * NVRAM Info structure returned for NVRAM_GetInfo call
 */
struct aac_nvramdevinfo {
	u_int32_t	NV_Enabled;	/* write caching enabled */
	u_int32_t	NV_Error;	/* device in error state */
	u_int32_t	NV_NDirty;	/* count of dirty NVRAM buffers */
	u_int32_t	NV_NActive;	/* count of NVRAM buffers being
					 * written */
} __attribute__ ((packed));

struct aac_nvraminfo {
	AAC_NVSTATUS		NV_Status;	/* nvram subsystem status */
	AAC_NVBATTSTATUS	NV_BattStatus;	/* battery status */
	u_int32_t		NV_Size;	/* size of WriteCache NVRAM in
						 * bytes */
	u_int32_t		NV_BufSize;	/* size of NVRAM buffers in
						 * bytes */
	u_int32_t		NV_NBufs;	/* number of NVRAM buffers */
	u_int32_t		NV_NDirty;	/* Num dirty NVRAM buffers */
	u_int32_t		NV_NClean;	/* Num clean NVRAM buffers */
	u_int32_t		NV_NActive;	/* Num NVRAM buffers being
						 * written */
	u_int32_t		NV_NBrokered;	/* Num brokered NVRAM buffers */
	struct aac_nvramdevinfo	NV_DevInfo[AAC_NFILESYS];	/* per device
								 * info */
	u_int32_t		NV_BattNeedsReconditioning;	/* boolean */
	u_int32_t		NV_TotalSize;	/* size of all non-volatile
						 * memories in bytes */
} __attribute__ ((packed));

/*
 * Data types relating to adapter-initiated FIBs
 *
 * Based on types and structures in <aifstruc.h>
 */

/*
 * Progress Reports
 */
typedef enum {
	AifJobStsSuccess = 1,
	AifJobStsFinished,
	AifJobStsAborted,
	AifJobStsFailed,
	AifJobStsLastReportMarker = 100,	/* All prior mean last report */
	AifJobStsSuspended,
	AifJobStsRunning
} AAC_AifJobStatus;

typedef enum {
	AifJobScsiMin = 1,		/* Minimum value for Scsi operation */
	AifJobScsiZero,			/* SCSI device clear operation */
	AifJobScsiVerify,		/* SCSI device Verify operation NO
					 * REPAIR */
	AifJobScsiExercise,		/* SCSI device Exercise operation */
	AifJobScsiVerifyRepair,		/* SCSI device Verify operation WITH
					 * repair */
	AifJobScsiMax = 99,		/* Max Scsi value */
	AifJobCtrMin,			/* Min Ctr op value */
	AifJobCtrZero,			/* Container clear operation */
	AifJobCtrCopy,			/* Container copy operation */
	AifJobCtrCreateMirror,		/* Container Create Mirror operation */
	AifJobCtrMergeMirror,		/* Container Merge Mirror operation */
	AifJobCtrScrubMirror,		/* Container Scrub Mirror operation */
	AifJobCtrRebuildRaid5,		/* Container Rebuild Raid5 operation */
	AifJobCtrScrubRaid5,		/* Container Scrub Raid5 operation */
	AifJobCtrMorph,			/* Container morph operation */
	AifJobCtrPartCopy,		/* Container Partition copy operation */
	AifJobCtrRebuildMirror,		/* Container Rebuild Mirror operation */
	AifJobCtrCrazyCache,		/* crazy cache */
	AifJobCtrMax = 199,		/* Max Ctr type operation */
	AifJobFsMin,			/* Min Fs type operation */
	AifJobFsCreate,			/* File System Create operation */
	AifJobFsVerify,			/* File System Verify operation */
	AifJobFsExtend,			/* File System Extend operation */
	AifJobFsMax = 299,		/* Max Fs type operation */
	AifJobApiFormatNTFS,		/* Format a drive to NTFS */
	AifJobApiFormatFAT,		/* Format a drive to FAT */
	AifJobApiUpdateSnapshot,	/* update the read/write half of a
					 * snapshot */
	AifJobApiFormatFAT32,		/* Format a drive to FAT32 */
	AifJobApiMax = 399,		/* Max API type operation */
	AifJobCtlContinuousCtrVerify,	/* Adapter operation */
	AifJobCtlMax = 499		/* Max Adapter type operation */
} AAC_AifJobType;

struct aac_AifContainers {
	u_int32_t	src;		/* from/master */
	u_int32_t	dst;		/* to/slave */
} __attribute__ ((packed));

union aac_AifJobClient {
	struct aac_AifContainers	container;	/* For Container and
							 * file system progress
							 * ops; */
	int32_t				scsi_dh;	/* For SCSI progress
							 * ops */
};

struct aac_AifJobDesc {
	u_int32_t		jobID;		/* DO NOT FILL IN! Will be
						 * filled in by AIF */
	AAC_AifJobType		type;		/* Operation that is being
						 * performed */
	union aac_AifJobClient	client;		/* Details */
} __attribute__ ((packed));

struct aac_AifJobProgressReport {
	struct aac_AifJobDesc	jd;
	AAC_AifJobStatus	status;
	u_int32_t		finalTick;
	u_int32_t		currentTick;
	u_int32_t		jobSpecificData1;
	u_int32_t		jobSpecificData2;
} __attribute__ ((packed));

/*
 * Event Notification
 */
typedef enum {
	/* General application notifies start here */
	AifEnGeneric = 1,		/* Generic notification */
	AifEnTaskComplete,		/* Task has completed */
	AifEnConfigChange,		/* Adapter config change occurred */
	AifEnContainerChange,		/* Adapter specific container 
					 * configuration change */
	AifEnDeviceFailure,		/* SCSI device failed */
	AifEnMirrorFailover,		/* Mirror failover started */
	AifEnContainerEvent,		/* Significant container event */
	AifEnFileSystemChange,		/* File system changed */
	AifEnConfigPause,		/* Container pause event */
	AifEnConfigResume,		/* Container resume event */
	AifEnFailoverChange,		/* Failover space assignment changed */
	AifEnRAID5RebuildDone,		/* RAID5 rebuild finished */
	AifEnEnclosureManagement,	/* Enclosure management event */
	AifEnBatteryEvent,		/* Significant NV battery event */
	AifEnAddContainer,		/* A new container was created. */
	AifEnDeleteContainer,		/* A container was deleted. */
	AifEnSMARTEvent, 	       	/* SMART Event */
	AifEnBatteryNeedsRecond,	/* The battery needs reconditioning */
	AifEnClusterEvent,		/* Some cluster event */
	AifEnDiskSetEvent,		/* A disk set event occured. */
	AifDriverNotifyStart=199,	/* Notifies for host driver go here */
	/* Host driver notifications start here */
	AifDenMorphComplete, 		/* A morph operation completed */
	AifDenVolumeExtendComplete 	/* Volume expand operation completed */
} AAC_AifEventNotifyType;

struct aac_AifEnsGeneric {
	char	text[132];		/* Generic text */
} __attribute__ ((packed));

struct aac_AifEnsDeviceFailure {
	u_int32_t	deviceHandle;	/* SCSI device handle */
} __attribute__ ((packed));

struct aac_AifEnsMirrorFailover {
	u_int32_t	container;	/* Container with failed element */
	u_int32_t	failedSlice;	/* Old slice which failed */
	u_int32_t	creatingSlice;	/* New slice used for auto-create */
} __attribute__ ((packed));

struct aac_AifEnsContainerChange {
	u_int32_t	container[2];	/* container that changed, -1 if no
					 * container */
} __attribute__ ((packed));

struct aac_AifEnsContainerEvent {
	u_int32_t	container;	/* container number  */
	u_int32_t	eventType;	/* event type */
} __attribute__ ((packed));

struct aac_AifEnsEnclosureEvent {
	u_int32_t	empID;		/* enclosure management proc number  */
	u_int32_t	unitID;		/* unitId, fan id, power supply id,
					 * slot id, tempsensor id.  */
	u_int32_t	eventType;	/* event type */
} __attribute__ ((packed));

struct aac_AifEnsBatteryEvent {
	AAC_NVBATT_TRANSITION	transition_type;	/* eg from low to ok */
	AAC_NVBATTSTATUS	current_state;		/* current batt state */
	AAC_NVBATTSTATUS	prior_state;		/* prev batt state */
} __attribute__ ((packed));

struct aac_AifEnsDiskSetEvent {
	u_int32_t	eventType;
	u_int64_t	DsNum;
	u_int64_t	CreatorId;
} __attribute__ ((packed));

typedef enum {
	CLUSTER_NULL_EVENT = 0,
	CLUSTER_PARTNER_NAME_EVENT,	/* change in partner hostname or
					 * adaptername from NULL to non-NULL */
	/* (partner's agent may be up) */
	CLUSTER_PARTNER_NULL_NAME_EVENT	/* change in partner hostname or
					 * adaptername from non-null to NULL */
	/* (partner has rebooted) */
} AAC_ClusterAifEvent;

struct aac_AifEnsClusterEvent {
	AAC_ClusterAifEvent	eventType;
} __attribute__ ((packed));

struct aac_AifEventNotify {
	AAC_AifEventNotifyType	type;
	union {
		struct aac_AifEnsGeneric		EG;
		struct aac_AifEnsDeviceFailure		EDF;
		struct aac_AifEnsMirrorFailover		EMF;
		struct aac_AifEnsContainerChange	ECC;
		struct aac_AifEnsContainerEvent		ECE;
		struct aac_AifEnsEnclosureEvent		EEE;
		struct aac_AifEnsBatteryEvent		EBE;
		struct aac_AifEnsDiskSetEvent		EDS;
/*		struct aac_AifEnsSMARTEvent		ES;*/
		struct aac_AifEnsClusterEvent		ECLE;
	} data;
} __attribute__ ((packed));

/*
 * Adapter Initiated FIB command structures. Start with the adapter
 * initiated FIBs that really come from the adapter, and get responded
 * to by the host. 
 */
#define AAC_AIF_REPORT_MAX_SIZE 64

typedef enum {
	AifCmdEventNotify = 1,	/* Notify of event */
	AifCmdJobProgress,	/* Progress report */
	AifCmdAPIReport,	/* Report from other user of API */
	AifCmdDriverNotify,	/* Notify host driver of event */
	AifReqJobList = 100,	/* Gets back complete job list */
	AifReqJobsForCtr,	/* Gets back jobs for specific container */
	AifReqJobsForScsi,	/* Gets back jobs for specific SCSI device */
	AifReqJobReport,	/* Gets back a specific job report or list */
	AifReqTerminateJob,	/* Terminates job */
	AifReqSuspendJob,	/* Suspends a job */
	AifReqResumeJob,	/* Resumes a job */
	AifReqSendAPIReport,	/* API generic report requests */
	AifReqAPIJobStart,	/* Start a job from the API */
	AifReqAPIJobUpdate,	/* Update a job report from the API */
	AifReqAPIJobFinish	/* Finish a job from the API */
} AAC_AifCommand;

struct aac_aif_command {
	AAC_AifCommand	command;	/* Tell host what type of
					 * notify this is */
	u_int32_t	seqNumber;	/* To allow ordering of
					 * reports (if necessary) */
	union {
	struct aac_AifEventNotify	EN;	/* Event notify structure */
	struct aac_AifJobProgressReport	PR[1];	/* Progress report */
	u_int8_t			AR[AAC_AIF_REPORT_MAX_SIZE];
	} data;
} __attribute__ ((packed));

/*
 * Filesystem commands/data
 *
 * The adapter has a very complex filesystem interface, most of which we ignore.
 * (And which seems not to be implemented, anyway.)
 */

/*
 * FSA commands
 * (not used?)
 */
typedef enum {
	Null = 0,
	GetAttributes,
	SetAttributes,
	Lookup,
	ReadLink,
	Read,
	Write,
	Create,
	MakeDirectory,
	SymbolicLink,
	MakeNode,
	Removex,
	RemoveDirectory,
	Rename,
	Link,
	ReadDirectory,
	ReadDirectoryPlus,
	FileSystemStatus,
	FileSystemInfo,
	PathConfigure,
	Commit,
	Mount,
	UnMount,
	Newfs,
	FsCheck,
	FsSync,
	SimReadWrite,
	SetFileSystemStatus,
	BlockRead,
	BlockWrite,
	NvramIoctl,
	FsSyncWait,
	ClearArchiveBit,
	SetAcl,
	GetAcl,
	AssignAcl,
	FaultInsertion,
	CrazyCache
} AAC_FSACommand;

/*
 * Command status values
 */
typedef enum {
	ST_OK = 0,
	ST_PERM = 1,
	ST_NOENT = 2,
	ST_IO = 5,
	ST_NXIO = 6,
	ST_E2BIG = 7,
	ST_ACCES = 13,
	ST_EXIST = 17,
	ST_XDEV = 18,
	ST_NODEV = 19,
	ST_NOTDIR = 20,
	ST_ISDIR = 21,
	ST_INVAL = 22,
	ST_FBIG = 27,
	ST_NOSPC = 28,
	ST_ROFS = 30,
	ST_MLINK = 31,
	ST_WOULDBLOCK = 35,
	ST_NAMETOOLONG = 63,
	ST_NOTEMPTY = 66,
	ST_DQUOT = 69,
	ST_STALE = 70,
	ST_REMOTE = 71,
	ST_BADHANDLE = 10001,
	ST_NOT_SYNC = 10002,
	ST_BAD_COOKIE = 10003,
	ST_NOTSUPP = 10004,
	ST_TOOSMALL = 10005,
	ST_SERVERFAULT = 10006,
	ST_BADTYPE = 10007,
	ST_JUKEBOX = 10008,
	ST_NOTMOUNTED = 10009,
	ST_MAINTMODE = 10010,
	ST_STALEACL = 10011
} AAC_FSAStatus;

/*
 * Volume manager commands
 */
typedef enum _VM_COMMANDS {
	VM_Null = 0,
	VM_NameServe,
	VM_ContainerConfig,
	VM_Ioctl,
	VM_FilesystemIoctl,
	VM_CloseAll,
	VM_CtBlockRead,
	VM_CtBlockWrite,
	VM_SliceBlockRead,	 /* raw access to configured storage objects */
	VM_SliceBlockWrite,
	VM_DriveBlockRead,	 /* raw access to physical devices */
	VM_DriveBlockWrite,
	VM_EnclosureMgt,	 /* enclosure management */
	VM_Unused,		 /* used to be diskset management */
	VM_CtBlockVerify,
	VM_CtPerf,		 /* performance test */
	VM_CtBlockRead64,
	VM_CtBlockWrite64,
	VM_CtBlockVerify64,
} AAC_VMCommand;

/*
 * "mountable object"
 */
struct aac_mntobj {
	u_int32_t			ObjectId;
	char				FileSystemName[16];
	struct aac_container_creation	CreateInfo;
	u_int32_t			Capacity;
	AAC_FSAVolType			VolType;
	AAC_FType			ObjType;
	u_int32_t			ContentState;
#define FSCS_READONLY		0x0002		/* XXX need more information
						 * than this */
	union {
		u_int32_t	pad[8];
	} ObjExtension;
	u_int32_t			AlterEgoId;
} __attribute__ ((packed));

struct aac_mntinfo {
	AAC_VMCommand		Command;
	AAC_FType		MntType;
	u_int32_t		MntCount;
} __attribute__ ((packed));

struct aac_mntinforesponse {
	AAC_FSAStatus		Status;
	AAC_FType		MntType;
	u_int32_t		MntRespCount;
	struct aac_mntobj	MntTable[1];
} __attribute__ ((packed));

/*
 * Container shutdown command.
 */
struct aac_closecommand {
	u_int32_t	Command;
	u_int32_t	ContainerId;
} __attribute__ ((packed));

/*
 * Write 'stability' options.
 */
typedef enum {
	CSTABLE = 1,
	CUNSTABLE
} AAC_CacheLevel;

/*
 * Commit level response for a write request.
 */
typedef enum {
	CMFILE_SYNC_NVRAM = 1,
	CMDATA_SYNC_NVRAM,
	CMFILE_SYNC,
	CMDATA_SYNC,
	CMUNSTABLE
} AAC_CommitLevel;

/*
 * Block read/write operations.
 * These structures are packed into the 'data' area in the FIB.
 */

struct aac_blockread {
	AAC_VMCommand		Command;	/* not FSACommand! */
	u_int32_t		ContainerId;
	u_int32_t		BlockNumber;
	u_int32_t		ByteCount;
	struct aac_sg_table	SgMap;		/* variable size */
} __attribute__ ((packed));

struct aac_blockread_response {
	AAC_FSAStatus		Status;
	u_int32_t		ByteCount;
} __attribute__ ((packed));

struct aac_blockwrite {
	AAC_VMCommand		Command;	/* not FSACommand! */
	u_int32_t		ContainerId;
	u_int32_t		BlockNumber;
	u_int32_t		ByteCount;
	AAC_CacheLevel	Stable;
	struct aac_sg_table	SgMap;		/* variable size */
} __attribute__ ((packed));

struct aac_blockwrite_response {
	AAC_FSAStatus	Status;
	u_int32_t		ByteCount;
	AAC_CommitLevel	Committed;
} __attribute__ ((packed));

/*
 * Container shutdown command.
 */
struct aac_close_command {
	AAC_VMCommand      Command;
	u_int32_t          ContainerId;
};

/*
 * Register set for adapters based on the Falcon bridge and PPC core
 */

#define AAC_FA_DOORBELL0_CLEAR		0x00
#define AAC_FA_DOORBELL1_CLEAR		0x02
#define AAC_FA_DOORBELL0		0x04
#define AAC_FA_DOORBELL1		0x06
#define AAC_FA_MASK0_CLEAR		0x08
#define AAC_FA_MASK1_CLEAR		0x0a
#define	AAC_FA_MASK0			0x0c
#define AAC_FA_MASK1			0x0e
#define AAC_FA_MAILBOX			0x10
#define	AAC_FA_FWSTATUS			0x2c	/* Mailbox 7 */
#define	AAC_FA_INTSRC			0x900

#define AAC_FA_HACK(sc)	(void)AAC_GETREG4(sc, AAC_FA_INTSRC)

/*
 * Register definitions for the Adaptec AAC-364 'Jalapeno I/II' adapters, based
 * on the SA110 'StrongArm'.
 */

#define AAC_SA_DOORBELL0_CLEAR		0x98	/* doorbell 0 (adapter->host) */
#define AAC_SA_DOORBELL0_SET		0x9c
#define AAC_SA_DOORBELL0		0x9c
#define AAC_SA_MASK0_CLEAR		0xa0
#define AAC_SA_MASK0_SET		0xa4

#define AAC_SA_DOORBELL1_CLEAR		0x9a	/* doorbell 1 (host->adapter) */
#define AAC_SA_DOORBELL1_SET		0x9e
#define AAC_SA_DOORBELL1		0x9e
#define AAC_SA_MASK1_CLEAR		0xa2
#define AAC_SA_MASK1_SET		0xa6

#define AAC_SA_MAILBOX			0xa8	/* mailbox (20 bytes) */
#define AAC_SA_FWSTATUS			0xc4

/*
 * Register definitions for the Adaptec 'Pablano' adapters, based on the i960Rx,
 * and other related adapters.
 */

#define AAC_RX_IDBR		0x20	/* inbound doorbell register */
#define AAC_RX_IISR		0x24	/* inbound interrupt status register */
#define AAC_RX_IIMR		0x28	/* inbound interrupt mask register */
#define AAC_RX_ODBR		0x2c	/* outbound doorbell register */
#define AAC_RX_OISR		0x30	/* outbound interrupt status register */
#define AAC_RX_OIMR		0x34	/* outbound interrupt mask register */

#define AAC_RX_MAILBOX		0x50	/* mailbox (20 bytes) */
#define AAC_RX_FWSTATUS		0x6c

/*
 * Common bit definitions for the doorbell registers.
 */

/*
 * Status bits in the doorbell registers.
 */
#define AAC_DB_SYNC_COMMAND	(1<<0)	/* send/completed synchronous FIB */
#define AAC_DB_COMMAND_READY	(1<<1)	/* posted one or more commands */
#define AAC_DB_RESPONSE_READY	(1<<2)	/* one or more commands complete */
#define AAC_DB_COMMAND_NOT_FULL	(1<<3)	/* command queue not full */
#define AAC_DB_RESPONSE_NOT_FULL (1<<4)	/* response queue not full */

/*
 * The adapter can request the host print a message by setting the
 * DB_PRINTF flag in DOORBELL0.  The driver responds by collecting the
 * message from the printf buffer, clearing the DB_PRINTF flag in 
 * DOORBELL0 and setting it in DOORBELL1.
 * (ODBR and IDBR respectively for the i960Rx adapters)
 */
#define AAC_DB_PRINTF		(1<<5)	/* adapter requests host printf */
#define AAC_PRINTF_DONE		(1<<5)	/* Host completed printf processing */

/*
 * Mask containing the interrupt bits we care about.  We don't anticipate (or
 * want) interrupts not in this mask.
 */
#define AAC_DB_INTERRUPTS	(AAC_DB_COMMAND_READY  |	\
				 AAC_DB_RESPONSE_READY |	\
				 AAC_DB_PRINTF)
