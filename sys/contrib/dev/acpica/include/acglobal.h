/******************************************************************************
 *
 * Name: acglobal.h - Declarations for global variables
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2011, Intel Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 */

#ifndef __ACGLOBAL_H__
#define __ACGLOBAL_H__


/*
 * Ensure that the globals are actually defined and initialized only once.
 *
 * The use of these macros allows a single list of globals (here) in order
 * to simplify maintenance of the code.
 */
#ifdef DEFINE_ACPI_GLOBALS
#define ACPI_EXTERN
#define ACPI_INIT_GLOBAL(a,b) a=b
#else
#define ACPI_EXTERN extern
#define ACPI_INIT_GLOBAL(a,b) a
#endif


#ifdef DEFINE_ACPI_GLOBALS

/* Public globals, available from outside ACPICA subsystem */

/*****************************************************************************
 *
 * Runtime configuration (static defaults that can be overriden at runtime)
 *
 ****************************************************************************/

/*
 * Enable "slack" in the AML interpreter?  Default is FALSE, and the
 * interpreter strictly follows the ACPI specification.  Setting to TRUE
 * allows the interpreter to ignore certain errors and/or bad AML constructs.
 *
 * Currently, these features are enabled by this flag:
 *
 * 1) Allow "implicit return" of last value in a control method
 * 2) Allow access beyond the end of an operation region
 * 3) Allow access to uninitialized locals/args (auto-init to integer 0)
 * 4) Allow ANY object type to be a source operand for the Store() operator
 * 5) Allow unresolved references (invalid target name) in package objects
 * 6) Enable warning messages for behavior that is not ACPI spec compliant
 */
UINT8       ACPI_INIT_GLOBAL (AcpiGbl_EnableInterpreterSlack, FALSE);

/*
 * Automatically serialize ALL control methods? Default is FALSE, meaning
 * to use the Serialized/NotSerialized method flags on a per method basis.
 * Only change this if the ASL code is poorly written and cannot handle
 * reentrancy even though methods are marked "NotSerialized".
 */
UINT8       ACPI_INIT_GLOBAL (AcpiGbl_AllMethodsSerialized, FALSE);

/*
 * Create the predefined _OSI method in the namespace? Default is TRUE
 * because ACPI CA is fully compatible with other ACPI implementations.
 * Changing this will revert ACPI CA (and machine ASL) to pre-OSI behavior.
 */
UINT8       ACPI_INIT_GLOBAL (AcpiGbl_CreateOsiMethod, TRUE);

/*
 * Optionally use default values for the ACPI register widths. Set this to
 * TRUE to use the defaults, if an FADT contains incorrect widths/lengths.
 */
UINT8       ACPI_INIT_GLOBAL (AcpiGbl_UseDefaultRegisterWidths, TRUE);

/*
 * Optionally enable output from the AML Debug Object.
 */
UINT8       ACPI_INIT_GLOBAL (AcpiGbl_EnableAmlDebugObject, FALSE);

/*
 * Optionally copy the entire DSDT to local memory (instead of simply
 * mapping it.) There are some BIOSs that corrupt or replace the original
 * DSDT, creating the need for this option. Default is FALSE, do not copy
 * the DSDT.
 */
UINT8       ACPI_INIT_GLOBAL (AcpiGbl_CopyDsdtLocally, FALSE);

/*
 * Optionally truncate I/O addresses to 16 bits. Provides compatibility
 * with other ACPI implementations. NOTE: During ACPICA initialization,
 * this value is set to TRUE if any Windows OSI strings have been
 * requested by the BIOS.
 */
UINT8       ACPI_INIT_GLOBAL (AcpiGbl_TruncateIoAddresses, FALSE);


/* AcpiGbl_FADT is a local copy of the FADT, converted to a common format. */

ACPI_TABLE_FADT             AcpiGbl_FADT;
UINT32                      AcpiCurrentGpeCount;
UINT32                      AcpiGbl_TraceFlags;
ACPI_NAME                   AcpiGbl_TraceMethodName;
BOOLEAN                     AcpiGbl_SystemAwakeAndRunning;

#endif

/*****************************************************************************
 *
 * ACPI Table globals
 *
 ****************************************************************************/

/*
 * AcpiGbl_RootTableList is the master list of ACPI tables that were
 * found in the RSDT/XSDT.
 */
ACPI_EXTERN ACPI_TABLE_LIST             AcpiGbl_RootTableList;
ACPI_EXTERN ACPI_TABLE_FACS            *AcpiGbl_FACS;

/* These addresses are calculated from the FADT Event Block addresses */

ACPI_EXTERN ACPI_GENERIC_ADDRESS        AcpiGbl_XPm1aStatus;
ACPI_EXTERN ACPI_GENERIC_ADDRESS        AcpiGbl_XPm1aEnable;

ACPI_EXTERN ACPI_GENERIC_ADDRESS        AcpiGbl_XPm1bStatus;
ACPI_EXTERN ACPI_GENERIC_ADDRESS        AcpiGbl_XPm1bEnable;

/* DSDT information. Used to check for DSDT corruption */

ACPI_EXTERN ACPI_TABLE_HEADER          *AcpiGbl_DSDT;
ACPI_EXTERN ACPI_TABLE_HEADER           AcpiGbl_OriginalDsdtHeader;

/*
 * Handle both ACPI 1.0 and ACPI 2.0 Integer widths. The integer width is
 * determined by the revision of the DSDT: If the DSDT revision is less than
 * 2, use only the lower 32 bits of the internal 64-bit Integer.
 */
ACPI_EXTERN UINT8                       AcpiGbl_IntegerBitWidth;
ACPI_EXTERN UINT8                       AcpiGbl_IntegerByteWidth;
ACPI_EXTERN UINT8                       AcpiGbl_IntegerNybbleWidth;


/*****************************************************************************
 *
 * Mutual exlusion within ACPICA subsystem
 *
 ****************************************************************************/

/*
 * Predefined mutex objects. This array contains the
 * actual OS mutex handles, indexed by the local ACPI_MUTEX_HANDLEs.
 * (The table maps local handles to the real OS handles)
 */
ACPI_EXTERN ACPI_MUTEX_INFO             AcpiGbl_MutexInfo[ACPI_NUM_MUTEX];

/*
 * Global lock mutex is an actual AML mutex object
 * Global lock semaphore works in conjunction with the HW global lock
 */
ACPI_EXTERN ACPI_OPERAND_OBJECT        *AcpiGbl_GlobalLockMutex;
ACPI_EXTERN ACPI_SEMAPHORE              AcpiGbl_GlobalLockSemaphore;
ACPI_EXTERN UINT16                      AcpiGbl_GlobalLockHandle;
ACPI_EXTERN BOOLEAN                     AcpiGbl_GlobalLockAcquired;
ACPI_EXTERN BOOLEAN                     AcpiGbl_GlobalLockPresent;

/*
 * Spinlocks are used for interfaces that can be possibly called at
 * interrupt level
 */
ACPI_EXTERN ACPI_SPINLOCK               AcpiGbl_GpeLock;      /* For GPE data structs and registers */
ACPI_EXTERN ACPI_SPINLOCK               AcpiGbl_HardwareLock; /* For ACPI H/W except GPE registers */

/* Mutex for _OSI support */

ACPI_EXTERN ACPI_MUTEX                  AcpiGbl_OsiMutex;

/* Reader/Writer lock is used for namespace walk and dynamic table unload */

ACPI_EXTERN ACPI_RW_LOCK                AcpiGbl_NamespaceRwLock;


/*****************************************************************************
 *
 * Miscellaneous globals
 *
 ****************************************************************************/

/* Object caches */

ACPI_EXTERN ACPI_CACHE_T               *AcpiGbl_NamespaceCache;
ACPI_EXTERN ACPI_CACHE_T               *AcpiGbl_StateCache;
ACPI_EXTERN ACPI_CACHE_T               *AcpiGbl_PsNodeCache;
ACPI_EXTERN ACPI_CACHE_T               *AcpiGbl_PsNodeExtCache;
ACPI_EXTERN ACPI_CACHE_T               *AcpiGbl_OperandCache;

/* Global handlers */

ACPI_EXTERN ACPI_OBJECT_NOTIFY_HANDLER  AcpiGbl_DeviceNotify;
ACPI_EXTERN ACPI_OBJECT_NOTIFY_HANDLER  AcpiGbl_SystemNotify;
ACPI_EXTERN ACPI_EXCEPTION_HANDLER      AcpiGbl_ExceptionHandler;
ACPI_EXTERN ACPI_INIT_HANDLER           AcpiGbl_InitHandler;
ACPI_EXTERN ACPI_TABLE_HANDLER          AcpiGbl_TableHandler;
ACPI_EXTERN void                       *AcpiGbl_TableHandlerContext;
ACPI_EXTERN ACPI_WALK_STATE            *AcpiGbl_BreakpointWalk;
ACPI_EXTERN ACPI_INTERFACE_HANDLER      AcpiGbl_InterfaceHandler;

/* Owner ID support */

ACPI_EXTERN UINT32                      AcpiGbl_OwnerIdMask[ACPI_NUM_OWNERID_MASKS];
ACPI_EXTERN UINT8                       AcpiGbl_LastOwnerIdIndex;
ACPI_EXTERN UINT8                       AcpiGbl_NextOwnerIdOffset;

/* Misc */

ACPI_EXTERN UINT32                      AcpiGbl_OriginalMode;
ACPI_EXTERN UINT32                      AcpiGbl_RsdpOriginalLocation;
ACPI_EXTERN UINT32                      AcpiGbl_NsLookupCount;
ACPI_EXTERN UINT32                      AcpiGbl_PsFindCount;
ACPI_EXTERN UINT16                      AcpiGbl_Pm1EnableRegisterSave;
ACPI_EXTERN UINT8                       AcpiGbl_DebuggerConfiguration;
ACPI_EXTERN BOOLEAN                     AcpiGbl_StepToNextCall;
ACPI_EXTERN BOOLEAN                     AcpiGbl_AcpiHardwarePresent;
ACPI_EXTERN BOOLEAN                     AcpiGbl_EventsInitialized;
ACPI_EXTERN UINT8                       AcpiGbl_OsiData;
ACPI_EXTERN ACPI_INTERFACE_INFO        *AcpiGbl_SupportedInterfaces;


#ifndef DEFINE_ACPI_GLOBALS

/* Exception codes */

extern char const                       *AcpiGbl_ExceptionNames_Env[];
extern char const                       *AcpiGbl_ExceptionNames_Pgm[];
extern char const                       *AcpiGbl_ExceptionNames_Tbl[];
extern char const                       *AcpiGbl_ExceptionNames_Aml[];
extern char const                       *AcpiGbl_ExceptionNames_Ctrl[];

/* Other miscellaneous */

extern BOOLEAN                          AcpiGbl_Shutdown;
extern UINT32                           AcpiGbl_StartupFlags;
extern const char                      *AcpiGbl_SleepStateNames[ACPI_S_STATE_COUNT];
extern const char                      *AcpiGbl_LowestDstateNames[ACPI_NUM_SxW_METHODS];
extern const char                      *AcpiGbl_HighestDstateNames[ACPI_NUM_SxD_METHODS];
extern const ACPI_OPCODE_INFO           AcpiGbl_AmlOpInfo[AML_NUM_OPCODES];
extern const char                      *AcpiGbl_RegionTypes[ACPI_NUM_PREDEFINED_REGIONS];
#endif


#ifdef ACPI_DBG_TRACK_ALLOCATIONS

/* Lists for tracking memory allocations */

ACPI_EXTERN ACPI_MEMORY_LIST           *AcpiGbl_GlobalList;
ACPI_EXTERN ACPI_MEMORY_LIST           *AcpiGbl_NsNodeList;
ACPI_EXTERN BOOLEAN                     AcpiGbl_DisplayFinalMemStats;
ACPI_EXTERN BOOLEAN                     AcpiGbl_DisableMemTracking;
#endif


/*****************************************************************************
 *
 * Namespace globals
 *
 ****************************************************************************/

#if !defined (ACPI_NO_METHOD_EXECUTION) || defined (ACPI_CONSTANT_EVAL_ONLY)
#define NUM_PREDEFINED_NAMES            10
#else
#define NUM_PREDEFINED_NAMES            9
#endif

ACPI_EXTERN ACPI_NAMESPACE_NODE         AcpiGbl_RootNodeStruct;
ACPI_EXTERN ACPI_NAMESPACE_NODE        *AcpiGbl_RootNode;
ACPI_EXTERN ACPI_NAMESPACE_NODE        *AcpiGbl_FadtGpeDevice;
ACPI_EXTERN ACPI_OPERAND_OBJECT        *AcpiGbl_ModuleCodeList;


extern const UINT8                      AcpiGbl_NsProperties [ACPI_NUM_NS_TYPES];
extern const ACPI_PREDEFINED_NAMES      AcpiGbl_PreDefinedNames [NUM_PREDEFINED_NAMES];

#ifdef ACPI_DEBUG_OUTPUT
ACPI_EXTERN UINT32                      AcpiGbl_CurrentNodeCount;
ACPI_EXTERN UINT32                      AcpiGbl_CurrentNodeSize;
ACPI_EXTERN UINT32                      AcpiGbl_MaxConcurrentNodeCount;
ACPI_EXTERN ACPI_SIZE                  *AcpiGbl_EntryStackPointer;
ACPI_EXTERN ACPI_SIZE                  *AcpiGbl_LowestStackPointer;
ACPI_EXTERN UINT32                      AcpiGbl_DeepestNesting;
#endif


/*****************************************************************************
 *
 * Interpreter globals
 *
 ****************************************************************************/


ACPI_EXTERN ACPI_THREAD_STATE          *AcpiGbl_CurrentWalkList;

/* Control method single step flag */

ACPI_EXTERN UINT8                       AcpiGbl_CmSingleStep;


/*****************************************************************************
 *
 * Hardware globals
 *
 ****************************************************************************/

extern      ACPI_BIT_REGISTER_INFO      AcpiGbl_BitRegisterInfo[ACPI_NUM_BITREG];
ACPI_EXTERN UINT8                       AcpiGbl_SleepTypeA;
ACPI_EXTERN UINT8                       AcpiGbl_SleepTypeB;


/*****************************************************************************
 *
 * Event and GPE globals
 *
 ****************************************************************************/

ACPI_EXTERN UINT8                       AcpiGbl_AllGpesInitialized;
ACPI_EXTERN ACPI_GPE_XRUPT_INFO        *AcpiGbl_GpeXruptListHead;
ACPI_EXTERN ACPI_GPE_BLOCK_INFO        *AcpiGbl_GpeFadtBlocks[ACPI_MAX_GPE_BLOCKS];
ACPI_EXTERN ACPI_GBL_EVENT_HANDLER      AcpiGbl_GlobalEventHandler;
ACPI_EXTERN void                       *AcpiGbl_GlobalEventHandlerContext;
ACPI_EXTERN ACPI_FIXED_EVENT_HANDLER    AcpiGbl_FixedEventHandlers[ACPI_NUM_FIXED_EVENTS];
extern      ACPI_FIXED_EVENT_INFO       AcpiGbl_FixedEventInfo[ACPI_NUM_FIXED_EVENTS];


/*****************************************************************************
 *
 * Debug support
 *
 ****************************************************************************/

/* Procedure nesting level for debug output */

extern      UINT32                      AcpiGbl_NestingLevel;

/* Event counters */

ACPI_EXTERN UINT32                      AcpiMethodCount;
ACPI_EXTERN UINT32                      AcpiGpeCount;
ACPI_EXTERN UINT32                      AcpiSciCount;
ACPI_EXTERN UINT32                      AcpiFixedEventCount[ACPI_NUM_FIXED_EVENTS];

/* Support for dynamic control method tracing mechanism */

ACPI_EXTERN UINT32                      AcpiGbl_OriginalDbgLevel;
ACPI_EXTERN UINT32                      AcpiGbl_OriginalDbgLayer;
ACPI_EXTERN UINT32                      AcpiGbl_TraceDbgLevel;
ACPI_EXTERN UINT32                      AcpiGbl_TraceDbgLayer;


/*****************************************************************************
 *
 * Debugger globals
 *
 ****************************************************************************/

ACPI_EXTERN UINT8                       AcpiGbl_DbOutputFlags;

#ifdef ACPI_DISASSEMBLER

ACPI_EXTERN BOOLEAN                     AcpiGbl_DbOpt_disasm;
ACPI_EXTERN BOOLEAN                     AcpiGbl_DbOpt_verbose;
ACPI_EXTERN ACPI_EXTERNAL_LIST         *AcpiGbl_ExternalList;
ACPI_EXTERN ACPI_EXTERNAL_FILE         *AcpiGbl_ExternalFileList;
#endif


#ifdef ACPI_DEBUGGER

extern      BOOLEAN                     AcpiGbl_MethodExecuting;
extern      BOOLEAN                     AcpiGbl_AbortMethod;
extern      BOOLEAN                     AcpiGbl_DbTerminateThreads;

ACPI_EXTERN BOOLEAN                     AcpiGbl_DbOpt_tables;
ACPI_EXTERN BOOLEAN                     AcpiGbl_DbOpt_stats;
ACPI_EXTERN BOOLEAN                     AcpiGbl_DbOpt_ini_methods;
ACPI_EXTERN BOOLEAN                     AcpiGbl_DbOpt_NoRegionSupport;

ACPI_EXTERN char                       *AcpiGbl_DbArgs[ACPI_DEBUGGER_MAX_ARGS];
ACPI_EXTERN char                        AcpiGbl_DbLineBuf[80];
ACPI_EXTERN char                        AcpiGbl_DbParsedBuf[80];
ACPI_EXTERN char                        AcpiGbl_DbScopeBuf[40];
ACPI_EXTERN char                        AcpiGbl_DbDebugFilename[40];
ACPI_EXTERN BOOLEAN                     AcpiGbl_DbOutputToFile;
ACPI_EXTERN char                       *AcpiGbl_DbBuffer;
ACPI_EXTERN char                       *AcpiGbl_DbFilename;
ACPI_EXTERN UINT32                      AcpiGbl_DbDebugLevel;
ACPI_EXTERN UINT32                      AcpiGbl_DbConsoleDebugLevel;
ACPI_EXTERN ACPI_NAMESPACE_NODE        *AcpiGbl_DbScopeNode;

/*
 * Statistic globals
 */
ACPI_EXTERN UINT16                      AcpiGbl_ObjTypeCount[ACPI_TYPE_NS_NODE_MAX+1];
ACPI_EXTERN UINT16                      AcpiGbl_NodeTypeCount[ACPI_TYPE_NS_NODE_MAX+1];
ACPI_EXTERN UINT16                      AcpiGbl_ObjTypeCountMisc;
ACPI_EXTERN UINT16                      AcpiGbl_NodeTypeCountMisc;
ACPI_EXTERN UINT32                      AcpiGbl_NumNodes;
ACPI_EXTERN UINT32                      AcpiGbl_NumObjects;


ACPI_EXTERN UINT32                      AcpiGbl_SizeOfParseTree;
ACPI_EXTERN UINT32                      AcpiGbl_SizeOfMethodTrees;
ACPI_EXTERN UINT32                      AcpiGbl_SizeOfNodeEntries;
ACPI_EXTERN UINT32                      AcpiGbl_SizeOfAcpiObjects;

#endif /* ACPI_DEBUGGER */

#endif /* __ACGLOBAL_H__ */
