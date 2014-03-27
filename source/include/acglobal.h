/******************************************************************************
 *
 * Name: acglobal.h - Declarations for global variables
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2014, Intel Corp.
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
#define ACPI_GLOBAL(type,name) \
    extern type name; \
    type name

#define ACPI_INIT_GLOBAL(type,name,value) \
    type name=value

#else
#define ACPI_GLOBAL(type,name) \
    extern type name

#define ACPI_INIT_GLOBAL(type,name,value) \
    extern type name
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
 * interpreter strictly follows the ACPI specification. Setting to TRUE
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
ACPI_INIT_GLOBAL (UINT8,                AcpiGbl_EnableInterpreterSlack, FALSE);

/*
 * Automatically serialize all methods that create named objects? Default
 * is TRUE, meaning that all NonSerialized methods are scanned once at
 * table load time to determine those that create named objects. Methods
 * that create named objects are marked Serialized in order to prevent
 * possible run-time problems if they are entered by more than one thread.
 */
ACPI_INIT_GLOBAL (UINT8,                AcpiGbl_AutoSerializeMethods, TRUE);

/*
 * Create the predefined _OSI method in the namespace? Default is TRUE
 * because ACPICA is fully compatible with other ACPI implementations.
 * Changing this will revert ACPICA (and machine ASL) to pre-OSI behavior.
 */
ACPI_INIT_GLOBAL (UINT8,                AcpiGbl_CreateOsiMethod, TRUE);

/*
 * Optionally use default values for the ACPI register widths. Set this to
 * TRUE to use the defaults, if an FADT contains incorrect widths/lengths.
 */
ACPI_INIT_GLOBAL (UINT8,                AcpiGbl_UseDefaultRegisterWidths, TRUE);

/*
 * Optionally enable output from the AML Debug Object.
 */
ACPI_INIT_GLOBAL (UINT8,                AcpiGbl_EnableAmlDebugObject, FALSE);

/*
 * Optionally copy the entire DSDT to local memory (instead of simply
 * mapping it.) There are some BIOSs that corrupt or replace the original
 * DSDT, creating the need for this option. Default is FALSE, do not copy
 * the DSDT.
 */
ACPI_INIT_GLOBAL (UINT8,                AcpiGbl_CopyDsdtLocally, FALSE);

/*
 * Optionally ignore an XSDT if present and use the RSDT instead.
 * Although the ACPI specification requires that an XSDT be used instead
 * of the RSDT, the XSDT has been found to be corrupt or ill-formed on
 * some machines. Default behavior is to use the XSDT if present.
 */
ACPI_INIT_GLOBAL (UINT8,                AcpiGbl_DoNotUseXsdt, FALSE);

/*
 * Optionally use 32-bit FADT addresses if and when there is a conflict
 * (address mismatch) between the 32-bit and 64-bit versions of the
 * address. Although ACPICA adheres to the ACPI specification which
 * requires the use of the corresponding 64-bit address if it is non-zero,
 * some machines have been found to have a corrupted non-zero 64-bit
 * address. Default is FALSE, do not favor the 32-bit addresses.
 */
ACPI_INIT_GLOBAL (UINT8,                AcpiGbl_Use32BitFadtAddresses, FALSE);

/*
 * Optionally truncate I/O addresses to 16 bits. Provides compatibility
 * with other ACPI implementations. NOTE: During ACPICA initialization,
 * this value is set to TRUE if any Windows OSI strings have been
 * requested by the BIOS.
 */
ACPI_INIT_GLOBAL (UINT8,                AcpiGbl_TruncateIoAddresses, FALSE);

/*
 * Disable runtime checking and repair of values returned by control methods.
 * Use only if the repair is causing a problem on a particular machine.
 */
ACPI_INIT_GLOBAL (UINT8,                AcpiGbl_DisableAutoRepair, FALSE);

/*
 * Optionally do not install any SSDTs from the RSDT/XSDT during initialization.
 * This can be useful for debugging ACPI problems on some machines.
 */
ACPI_INIT_GLOBAL (UINT8,                AcpiGbl_DisableSsdtTableInstall, FALSE);

/*
 * We keep track of the latest version of Windows that has been requested by
 * the BIOS.
 */
ACPI_INIT_GLOBAL (UINT8,                AcpiGbl_OsiData, 0);

#endif /* DEFINE_ACPI_GLOBALS */


/*****************************************************************************
 *
 * ACPI Table globals
 *
 ****************************************************************************/

/*
 * Master list of all ACPI tables that were found in the RSDT/XSDT.
 */
ACPI_GLOBAL (ACPI_TABLE_LIST,           AcpiGbl_RootTableList);

/* DSDT information. Used to check for DSDT corruption */

ACPI_GLOBAL (ACPI_TABLE_HEADER *,       AcpiGbl_DSDT);
ACPI_GLOBAL (ACPI_TABLE_HEADER,         AcpiGbl_OriginalDsdtHeader);

#if (!ACPI_REDUCED_HARDWARE)
ACPI_GLOBAL (ACPI_TABLE_FACS *,         AcpiGbl_FACS);

#endif /* !ACPI_REDUCED_HARDWARE */

/* These addresses are calculated from the FADT Event Block addresses */

ACPI_GLOBAL (ACPI_GENERIC_ADDRESS,      AcpiGbl_XPm1aStatus);
ACPI_GLOBAL (ACPI_GENERIC_ADDRESS,      AcpiGbl_XPm1aEnable);

ACPI_GLOBAL (ACPI_GENERIC_ADDRESS,      AcpiGbl_XPm1bStatus);
ACPI_GLOBAL (ACPI_GENERIC_ADDRESS,      AcpiGbl_XPm1bEnable);

/*
 * Handle both ACPI 1.0 and ACPI 2.0+ Integer widths. The integer width is
 * determined by the revision of the DSDT: If the DSDT revision is less than
 * 2, use only the lower 32 bits of the internal 64-bit Integer.
 */
ACPI_GLOBAL (UINT8,                     AcpiGbl_IntegerBitWidth);
ACPI_GLOBAL (UINT8,                     AcpiGbl_IntegerByteWidth);
ACPI_GLOBAL (UINT8,                     AcpiGbl_IntegerNybbleWidth);


/*****************************************************************************
 *
 * Mutual exclusion within ACPICA subsystem
 *
 ****************************************************************************/

/*
 * Predefined mutex objects. This array contains the
 * actual OS mutex handles, indexed by the local ACPI_MUTEX_HANDLEs.
 * (The table maps local handles to the real OS handles)
 */
ACPI_GLOBAL (ACPI_MUTEX_INFO,           AcpiGbl_MutexInfo[ACPI_NUM_MUTEX]);

/*
 * Global lock mutex is an actual AML mutex object
 * Global lock semaphore works in conjunction with the actual global lock
 * Global lock spinlock is used for "pending" handshake
 */
ACPI_GLOBAL (ACPI_OPERAND_OBJECT *,     AcpiGbl_GlobalLockMutex);
ACPI_GLOBAL (ACPI_SEMAPHORE,            AcpiGbl_GlobalLockSemaphore);
ACPI_GLOBAL (ACPI_SPINLOCK,             AcpiGbl_GlobalLockPendingLock);
ACPI_GLOBAL (UINT16,                    AcpiGbl_GlobalLockHandle);
ACPI_GLOBAL (BOOLEAN,                   AcpiGbl_GlobalLockAcquired);
ACPI_GLOBAL (BOOLEAN,                   AcpiGbl_GlobalLockPresent);
ACPI_GLOBAL (BOOLEAN,                   AcpiGbl_GlobalLockPending);

/*
 * Spinlocks are used for interfaces that can be possibly called at
 * interrupt level
 */
ACPI_GLOBAL (ACPI_SPINLOCK,             AcpiGbl_GpeLock);       /* For GPE data structs and registers */
ACPI_GLOBAL (ACPI_SPINLOCK,             AcpiGbl_HardwareLock);  /* For ACPI H/W except GPE registers */
ACPI_GLOBAL (ACPI_SPINLOCK,             AcpiGbl_ReferenceCountLock);

/* Mutex for _OSI support */

ACPI_GLOBAL (ACPI_MUTEX,                AcpiGbl_OsiMutex);

/* Reader/Writer lock is used for namespace walk and dynamic table unload */

ACPI_GLOBAL (ACPI_RW_LOCK,              AcpiGbl_NamespaceRwLock);


/*****************************************************************************
 *
 * Miscellaneous globals
 *
 ****************************************************************************/

/* Object caches */

ACPI_GLOBAL (ACPI_CACHE_T *,            AcpiGbl_NamespaceCache);
ACPI_GLOBAL (ACPI_CACHE_T *,            AcpiGbl_StateCache);
ACPI_GLOBAL (ACPI_CACHE_T *,            AcpiGbl_PsNodeCache);
ACPI_GLOBAL (ACPI_CACHE_T *,            AcpiGbl_PsNodeExtCache);
ACPI_GLOBAL (ACPI_CACHE_T *,            AcpiGbl_OperandCache);

/* System */

ACPI_INIT_GLOBAL (UINT32,               AcpiGbl_StartupFlags, 0);
ACPI_INIT_GLOBAL (BOOLEAN,              AcpiGbl_Shutdown, TRUE);

/* Global handlers */

ACPI_GLOBAL (ACPI_GLOBAL_NOTIFY_HANDLER,AcpiGbl_GlobalNotify[2]);
ACPI_GLOBAL (ACPI_EXCEPTION_HANDLER,    AcpiGbl_ExceptionHandler);
ACPI_GLOBAL (ACPI_INIT_HANDLER,         AcpiGbl_InitHandler);
ACPI_GLOBAL (ACPI_TABLE_HANDLER,        AcpiGbl_TableHandler);
ACPI_GLOBAL (void *,                    AcpiGbl_TableHandlerContext);
ACPI_GLOBAL (ACPI_WALK_STATE *,         AcpiGbl_BreakpointWalk);
ACPI_GLOBAL (ACPI_INTERFACE_HANDLER,    AcpiGbl_InterfaceHandler);
ACPI_GLOBAL (ACPI_SCI_HANDLER_INFO *,   AcpiGbl_SciHandlerList);

/* Owner ID support */

ACPI_GLOBAL (UINT32,                    AcpiGbl_OwnerIdMask[ACPI_NUM_OWNERID_MASKS]);
ACPI_GLOBAL (UINT8,                     AcpiGbl_LastOwnerIdIndex);
ACPI_GLOBAL (UINT8,                     AcpiGbl_NextOwnerIdOffset);

/* Initialization sequencing */

ACPI_GLOBAL (BOOLEAN,                   AcpiGbl_RegMethodsExecuted);

/* Misc */

ACPI_GLOBAL (UINT32,                    AcpiGbl_OriginalMode);
ACPI_GLOBAL (UINT32,                    AcpiGbl_RsdpOriginalLocation);
ACPI_GLOBAL (UINT32,                    AcpiGbl_NsLookupCount);
ACPI_GLOBAL (UINT32,                    AcpiGbl_PsFindCount);
ACPI_GLOBAL (UINT16,                    AcpiGbl_Pm1EnableRegisterSave);
ACPI_GLOBAL (UINT8,                     AcpiGbl_DebuggerConfiguration);
ACPI_GLOBAL (BOOLEAN,                   AcpiGbl_StepToNextCall);
ACPI_GLOBAL (BOOLEAN,                   AcpiGbl_AcpiHardwarePresent);
ACPI_GLOBAL (BOOLEAN,                   AcpiGbl_EventsInitialized);
ACPI_GLOBAL (ACPI_INTERFACE_INFO *,     AcpiGbl_SupportedInterfaces);
ACPI_GLOBAL (ACPI_ADDRESS_RANGE *,      AcpiGbl_AddressRangeList[ACPI_ADDRESS_RANGE_MAX]);

/* Other miscellaneous, declared and initialized in utglobal */

extern const char                      *AcpiGbl_SleepStateNames[ACPI_S_STATE_COUNT];
extern const char                      *AcpiGbl_LowestDstateNames[ACPI_NUM_SxW_METHODS];
extern const char                      *AcpiGbl_HighestDstateNames[ACPI_NUM_SxD_METHODS];
extern const char                      *AcpiGbl_RegionTypes[ACPI_NUM_PREDEFINED_REGIONS];
extern const ACPI_OPCODE_INFO           AcpiGbl_AmlOpInfo[AML_NUM_OPCODES];


#ifdef ACPI_DBG_TRACK_ALLOCATIONS

/* Lists for tracking memory allocations (debug only) */

ACPI_GLOBAL (ACPI_MEMORY_LIST *,        AcpiGbl_GlobalList);
ACPI_GLOBAL (ACPI_MEMORY_LIST *,        AcpiGbl_NsNodeList);
ACPI_GLOBAL (BOOLEAN,                   AcpiGbl_DisplayFinalMemStats);
ACPI_GLOBAL (BOOLEAN,                   AcpiGbl_DisableMemTracking);
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

ACPI_GLOBAL (ACPI_NAMESPACE_NODE,       AcpiGbl_RootNodeStruct);
ACPI_GLOBAL (ACPI_NAMESPACE_NODE *,     AcpiGbl_RootNode);
ACPI_GLOBAL (ACPI_NAMESPACE_NODE *,     AcpiGbl_FadtGpeDevice);
ACPI_GLOBAL (ACPI_OPERAND_OBJECT *,     AcpiGbl_ModuleCodeList);


extern const UINT8                      AcpiGbl_NsProperties [ACPI_NUM_NS_TYPES];
extern const ACPI_PREDEFINED_NAMES      AcpiGbl_PreDefinedNames [NUM_PREDEFINED_NAMES];

#ifdef ACPI_DEBUG_OUTPUT
ACPI_GLOBAL (UINT32,                    AcpiGbl_CurrentNodeCount);
ACPI_GLOBAL (UINT32,                    AcpiGbl_CurrentNodeSize);
ACPI_GLOBAL (UINT32,                    AcpiGbl_MaxConcurrentNodeCount);
ACPI_GLOBAL (ACPI_SIZE *,               AcpiGbl_EntryStackPointer);
ACPI_GLOBAL (ACPI_SIZE *,               AcpiGbl_LowestStackPointer);
ACPI_GLOBAL (UINT32,                    AcpiGbl_DeepestNesting);
ACPI_INIT_GLOBAL (UINT32,               AcpiGbl_NestingLevel, 0);
#endif


/*****************************************************************************
 *
 * Interpreter globals
 *
 ****************************************************************************/

ACPI_GLOBAL (ACPI_THREAD_STATE *,       AcpiGbl_CurrentWalkList);

/* Control method single step flag */

ACPI_GLOBAL (UINT8,                     AcpiGbl_CmSingleStep);


/*****************************************************************************
 *
 * Hardware globals
 *
 ****************************************************************************/

extern ACPI_BIT_REGISTER_INFO           AcpiGbl_BitRegisterInfo[ACPI_NUM_BITREG];

ACPI_GLOBAL (UINT8,                     AcpiGbl_SleepTypeA);
ACPI_GLOBAL (UINT8,                     AcpiGbl_SleepTypeB);


/*****************************************************************************
 *
 * Event and GPE globals
 *
 ****************************************************************************/

#if (!ACPI_REDUCED_HARDWARE)

ACPI_GLOBAL (UINT8,                     AcpiGbl_AllGpesInitialized);
ACPI_GLOBAL (ACPI_GPE_XRUPT_INFO *,     AcpiGbl_GpeXruptListHead);
ACPI_GLOBAL (ACPI_GPE_BLOCK_INFO *,     AcpiGbl_GpeFadtBlocks[ACPI_MAX_GPE_BLOCKS]);
ACPI_GLOBAL (ACPI_GBL_EVENT_HANDLER,    AcpiGbl_GlobalEventHandler);
ACPI_GLOBAL (void *,                    AcpiGbl_GlobalEventHandlerContext);
ACPI_GLOBAL (ACPI_FIXED_EVENT_HANDLER,  AcpiGbl_FixedEventHandlers[ACPI_NUM_FIXED_EVENTS]);

extern ACPI_FIXED_EVENT_INFO            AcpiGbl_FixedEventInfo[ACPI_NUM_FIXED_EVENTS];

#endif /* !ACPI_REDUCED_HARDWARE */

/*****************************************************************************
 *
 * Debug support
 *
 ****************************************************************************/

/* Event counters */

ACPI_GLOBAL (UINT32,                    AcpiMethodCount);
ACPI_GLOBAL (UINT32,                    AcpiGpeCount);
ACPI_GLOBAL (UINT32,                    AcpiSciCount);
ACPI_GLOBAL (UINT32,                    AcpiFixedEventCount[ACPI_NUM_FIXED_EVENTS]);

/* Support for dynamic control method tracing mechanism */

ACPI_GLOBAL (UINT32,                    AcpiGbl_OriginalDbgLevel);
ACPI_GLOBAL (UINT32,                    AcpiGbl_OriginalDbgLayer);
ACPI_GLOBAL (UINT32,                    AcpiGbl_TraceDbgLevel);
ACPI_GLOBAL (UINT32,                    AcpiGbl_TraceDbgLayer);


/*****************************************************************************
 *
 * Debugger and Disassembler globals
 *
 ****************************************************************************/

ACPI_GLOBAL (UINT8,                     AcpiGbl_DbOutputFlags);

#ifdef ACPI_DISASSEMBLER

/* Do not disassemble buffers to resource descriptors */

ACPI_INIT_GLOBAL (UINT8,                AcpiGbl_NoResourceDisassembly, FALSE);
ACPI_INIT_GLOBAL (BOOLEAN,              AcpiGbl_IgnoreNoopOperator, FALSE);

ACPI_GLOBAL (BOOLEAN,                   AcpiGbl_DbOpt_disasm);
ACPI_GLOBAL (BOOLEAN,                   AcpiGbl_DbOpt_verbose);
ACPI_GLOBAL (BOOLEAN,                   AcpiGbl_NumExternalMethods);
ACPI_GLOBAL (UINT32,                    AcpiGbl_ResolvedExternalMethods);
ACPI_GLOBAL (ACPI_EXTERNAL_LIST *,      AcpiGbl_ExternalList);
ACPI_GLOBAL (ACPI_EXTERNAL_FILE *,      AcpiGbl_ExternalFileList);
#endif

#ifdef ACPI_DEBUGGER

ACPI_INIT_GLOBAL (BOOLEAN,              AcpiGbl_DbTerminateThreads, FALSE);
ACPI_INIT_GLOBAL (BOOLEAN,              AcpiGbl_AbortMethod, FALSE);
ACPI_INIT_GLOBAL (BOOLEAN,              AcpiGbl_MethodExecuting, FALSE);

ACPI_GLOBAL (BOOLEAN,                   AcpiGbl_DbOpt_tables);
ACPI_GLOBAL (BOOLEAN,                   AcpiGbl_DbOpt_stats);
ACPI_GLOBAL (BOOLEAN,                   AcpiGbl_DbOpt_ini_methods);
ACPI_GLOBAL (BOOLEAN,                   AcpiGbl_DbOpt_NoRegionSupport);
ACPI_GLOBAL (BOOLEAN,                   AcpiGbl_DbOutputToFile);
ACPI_GLOBAL (char *,                    AcpiGbl_DbBuffer);
ACPI_GLOBAL (char *,                    AcpiGbl_DbFilename);
ACPI_GLOBAL (UINT32,                    AcpiGbl_DbDebugLevel);
ACPI_GLOBAL (UINT32,                    AcpiGbl_DbConsoleDebugLevel);
ACPI_GLOBAL (ACPI_NAMESPACE_NODE *,     AcpiGbl_DbScopeNode);

ACPI_GLOBAL (char *,                    AcpiGbl_DbArgs[ACPI_DEBUGGER_MAX_ARGS]);
ACPI_GLOBAL (ACPI_OBJECT_TYPE,          AcpiGbl_DbArgTypes[ACPI_DEBUGGER_MAX_ARGS]);

/* These buffers should all be the same size */

ACPI_GLOBAL (char,                      AcpiGbl_DbLineBuf[ACPI_DB_LINE_BUFFER_SIZE]);
ACPI_GLOBAL (char,                      AcpiGbl_DbParsedBuf[ACPI_DB_LINE_BUFFER_SIZE]);
ACPI_GLOBAL (char,                      AcpiGbl_DbScopeBuf[ACPI_DB_LINE_BUFFER_SIZE]);
ACPI_GLOBAL (char,                      AcpiGbl_DbDebugFilename[ACPI_DB_LINE_BUFFER_SIZE]);

/*
 * Statistic globals
 */
ACPI_GLOBAL (UINT16,                    AcpiGbl_ObjTypeCount[ACPI_TYPE_NS_NODE_MAX+1]);
ACPI_GLOBAL (UINT16,                    AcpiGbl_NodeTypeCount[ACPI_TYPE_NS_NODE_MAX+1]);
ACPI_GLOBAL (UINT16,                    AcpiGbl_ObjTypeCountMisc);
ACPI_GLOBAL (UINT16,                    AcpiGbl_NodeTypeCountMisc);
ACPI_GLOBAL (UINT32,                    AcpiGbl_NumNodes);
ACPI_GLOBAL (UINT32,                    AcpiGbl_NumObjects);

ACPI_GLOBAL (UINT32,                    AcpiGbl_SizeOfParseTree);
ACPI_GLOBAL (UINT32,                    AcpiGbl_SizeOfMethodTrees);
ACPI_GLOBAL (UINT32,                    AcpiGbl_SizeOfNodeEntries);
ACPI_GLOBAL (UINT32,                    AcpiGbl_SizeOfAcpiObjects);

#endif /* ACPI_DEBUGGER */


/*****************************************************************************
 *
 * Application globals
 *
 ****************************************************************************/

#ifdef ACPI_APPLICATION

ACPI_INIT_GLOBAL (ACPI_FILE,            AcpiGbl_DebugFile, NULL);

#endif /* ACPI_APPLICATION */


/*****************************************************************************
 *
 * Info/help support
 *
 ****************************************************************************/

extern const AH_PREDEFINED_NAME         AslPredefinedInfo[];
extern const AH_DEVICE_ID               AslDeviceIds[];


#endif /* __ACGLOBAL_H__ */
