
/******************************************************************************
 *
 * Name: acpixf.h - External interfaces to the ACPI subsystem
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


#ifndef __ACXFACE_H__
#define __ACXFACE_H__

/* Current ACPICA subsystem version in YYYYMMDD format */

#define ACPI_CA_VERSION                 0x20110211

#include <contrib/dev/acpica/include/actypes.h>
#include <contrib/dev/acpica/include/actbl.h>

/*
 * Globals that are publically available
 */
extern UINT32               AcpiCurrentGpeCount;
extern ACPI_TABLE_FADT      AcpiGbl_FADT;
extern BOOLEAN              AcpiGbl_SystemAwakeAndRunning;

/* Runtime configuration of debug print levels */

extern UINT32               AcpiDbgLevel;
extern UINT32               AcpiDbgLayer;

/* ACPICA runtime options */

extern UINT8                AcpiGbl_EnableInterpreterSlack;
extern UINT8                AcpiGbl_AllMethodsSerialized;
extern UINT8                AcpiGbl_CreateOsiMethod;
extern UINT8                AcpiGbl_UseDefaultRegisterWidths;
extern ACPI_NAME            AcpiGbl_TraceMethodName;
extern UINT32               AcpiGbl_TraceFlags;
extern UINT8                AcpiGbl_EnableAmlDebugObject;
extern UINT8                AcpiGbl_CopyDsdtLocally;
extern UINT8                AcpiGbl_TruncateIoAddresses;


/*
 * Initialization
 */
ACPI_STATUS
AcpiInitializeTables (
    ACPI_TABLE_DESC         *InitialStorage,
    UINT32                  InitialTableCount,
    BOOLEAN                 AllowResize);

ACPI_STATUS
AcpiInitializeSubsystem (
    void);

ACPI_STATUS
AcpiEnableSubsystem (
    UINT32                  Flags);

ACPI_STATUS
AcpiInitializeObjects (
    UINT32                  Flags);

ACPI_STATUS
AcpiTerminate (
    void);


/*
 * Miscellaneous global interfaces
 */
ACPI_STATUS
AcpiEnable (
    void);

ACPI_STATUS
AcpiDisable (
    void);

ACPI_STATUS
AcpiSubsystemStatus (
    void);

ACPI_STATUS
AcpiGetSystemInfo (
    ACPI_BUFFER             *RetBuffer);

ACPI_STATUS
AcpiGetStatistics (
    ACPI_STATISTICS         *Stats);

const char *
AcpiFormatException (
    ACPI_STATUS             Exception);

ACPI_STATUS
AcpiPurgeCachedObjects (
    void);

ACPI_STATUS
AcpiInstallInterface (
    ACPI_STRING             InterfaceName);

ACPI_STATUS
AcpiRemoveInterface (
    ACPI_STRING             InterfaceName);


/*
 * ACPI Memory management
 */
void *
AcpiAllocate (
    UINT32                  Size);

void *
AcpiCallocate (
    UINT32                  Size);

void
AcpiFree (
    void                    *Address);


/*
 * ACPI table manipulation interfaces
 */
ACPI_STATUS
AcpiReallocateRootTable (
    void);

ACPI_STATUS
AcpiFindRootPointer (
    ACPI_SIZE               *RsdpAddress);

ACPI_STATUS
AcpiLoadTables (
    void);

ACPI_STATUS
AcpiGetTableHeader (
    ACPI_STRING             Signature,
    UINT32                  Instance,
    ACPI_TABLE_HEADER       *OutTableHeader);

ACPI_STATUS
AcpiGetTable (
    ACPI_STRING             Signature,
    UINT32                  Instance,
    ACPI_TABLE_HEADER       **OutTable);

ACPI_STATUS
AcpiGetTableByIndex (
    UINT32                  TableIndex,
    ACPI_TABLE_HEADER       **OutTable);

ACPI_STATUS
AcpiInstallTableHandler (
    ACPI_TABLE_HANDLER      Handler,
    void                    *Context);

ACPI_STATUS
AcpiRemoveTableHandler (
    ACPI_TABLE_HANDLER      Handler);


/*
 * Namespace and name interfaces
 */
ACPI_STATUS
AcpiWalkNamespace (
    ACPI_OBJECT_TYPE        Type,
    ACPI_HANDLE             StartObject,
    UINT32                  MaxDepth,
    ACPI_WALK_CALLBACK      PreOrderVisit,
    ACPI_WALK_CALLBACK      PostOrderVisit,
    void                    *Context,
    void                    **ReturnValue);

ACPI_STATUS
AcpiGetDevices (
    char                    *HID,
    ACPI_WALK_CALLBACK      UserFunction,
    void                    *Context,
    void                    **ReturnValue);

ACPI_STATUS
AcpiGetName (
    ACPI_HANDLE             Object,
    UINT32                  NameType,
    ACPI_BUFFER             *RetPathPtr);

ACPI_STATUS
AcpiGetHandle (
    ACPI_HANDLE             Parent,
    ACPI_STRING             Pathname,
    ACPI_HANDLE             *RetHandle);

ACPI_STATUS
AcpiAttachData (
    ACPI_HANDLE             Object,
    ACPI_OBJECT_HANDLER     Handler,
    void                    *Data);

ACPI_STATUS
AcpiDetachData (
    ACPI_HANDLE             Object,
    ACPI_OBJECT_HANDLER     Handler);

ACPI_STATUS
AcpiGetData (
    ACPI_HANDLE             Object,
    ACPI_OBJECT_HANDLER     Handler,
    void                    **Data);

ACPI_STATUS
AcpiDebugTrace (
    char                    *Name,
    UINT32                  DebugLevel,
    UINT32                  DebugLayer,
    UINT32                  Flags);


/*
 * Object manipulation and enumeration
 */
ACPI_STATUS
AcpiEvaluateObject (
    ACPI_HANDLE             Object,
    ACPI_STRING             Pathname,
    ACPI_OBJECT_LIST        *ParameterObjects,
    ACPI_BUFFER             *ReturnObjectBuffer);

ACPI_STATUS
AcpiEvaluateObjectTyped (
    ACPI_HANDLE             Object,
    ACPI_STRING             Pathname,
    ACPI_OBJECT_LIST        *ExternalParams,
    ACPI_BUFFER             *ReturnBuffer,
    ACPI_OBJECT_TYPE        ReturnType);

ACPI_STATUS
AcpiGetObjectInfo (
    ACPI_HANDLE             Object,
    ACPI_DEVICE_INFO        **ReturnBuffer);

ACPI_STATUS
AcpiInstallMethod (
    UINT8                   *Buffer);

ACPI_STATUS
AcpiGetNextObject (
    ACPI_OBJECT_TYPE        Type,
    ACPI_HANDLE             Parent,
    ACPI_HANDLE             Child,
    ACPI_HANDLE             *OutHandle);

ACPI_STATUS
AcpiGetType (
    ACPI_HANDLE             Object,
    ACPI_OBJECT_TYPE        *OutType);

ACPI_STATUS
AcpiGetParent (
    ACPI_HANDLE             Object,
    ACPI_HANDLE             *OutHandle);


/*
 * Handler interfaces
 */
ACPI_STATUS
AcpiInstallInitializationHandler (
    ACPI_INIT_HANDLER       Handler,
    UINT32                  Function);

ACPI_STATUS
AcpiInstallGlobalEventHandler (
    ACPI_GBL_EVENT_HANDLER  Handler,
    void                    *Context);

ACPI_STATUS
AcpiInstallFixedEventHandler (
    UINT32                  AcpiEvent,
    ACPI_EVENT_HANDLER      Handler,
    void                    *Context);

ACPI_STATUS
AcpiRemoveFixedEventHandler (
    UINT32                  AcpiEvent,
    ACPI_EVENT_HANDLER      Handler);

ACPI_STATUS
AcpiInstallGpeHandler (
    ACPI_HANDLE             GpeDevice,
    UINT32                  GpeNumber,
    UINT32                  Type,
    ACPI_GPE_HANDLER        Address,
    void                    *Context);

ACPI_STATUS
AcpiRemoveGpeHandler (
    ACPI_HANDLE             GpeDevice,
    UINT32                  GpeNumber,
    ACPI_GPE_HANDLER        Address);

ACPI_STATUS
AcpiInstallNotifyHandler (
    ACPI_HANDLE             Device,
    UINT32                  HandlerType,
    ACPI_NOTIFY_HANDLER     Handler,
    void                    *Context);

ACPI_STATUS
AcpiRemoveNotifyHandler (
    ACPI_HANDLE             Device,
    UINT32                  HandlerType,
    ACPI_NOTIFY_HANDLER     Handler);

ACPI_STATUS
AcpiInstallAddressSpaceHandler (
    ACPI_HANDLE             Device,
    ACPI_ADR_SPACE_TYPE     SpaceId,
    ACPI_ADR_SPACE_HANDLER  Handler,
    ACPI_ADR_SPACE_SETUP    Setup,
    void                    *Context);

ACPI_STATUS
AcpiRemoveAddressSpaceHandler (
    ACPI_HANDLE             Device,
    ACPI_ADR_SPACE_TYPE     SpaceId,
    ACPI_ADR_SPACE_HANDLER  Handler);

ACPI_STATUS
AcpiInstallExceptionHandler (
    ACPI_EXCEPTION_HANDLER  Handler);

ACPI_STATUS
AcpiInstallInterfaceHandler (
    ACPI_INTERFACE_HANDLER  Handler);


/*
 * Global Lock interfaces
 */
ACPI_STATUS
AcpiAcquireGlobalLock (
    UINT16                  Timeout,
    UINT32                  *Handle);

ACPI_STATUS
AcpiReleaseGlobalLock (
    UINT32                  Handle);


/*
 * Fixed Event interfaces
 */
ACPI_STATUS
AcpiEnableEvent (
    UINT32                  Event,
    UINT32                  Flags);

ACPI_STATUS
AcpiDisableEvent (
    UINT32                  Event,
    UINT32                  Flags);

ACPI_STATUS
AcpiClearEvent (
    UINT32                  Event);

ACPI_STATUS
AcpiGetEventStatus (
    UINT32                  Event,
    ACPI_EVENT_STATUS       *EventStatus);


/*
 * General Purpose Event (GPE) Interfaces
 */
ACPI_STATUS
AcpiUpdateAllGpes (
    void);

ACPI_STATUS
AcpiEnableGpe (
    ACPI_HANDLE             GpeDevice,
    UINT32                  GpeNumber);

ACPI_STATUS
AcpiDisableGpe (
    ACPI_HANDLE             GpeDevice,
    UINT32                  GpeNumber);

ACPI_STATUS
AcpiClearGpe (
    ACPI_HANDLE             GpeDevice,
    UINT32                  GpeNumber);

ACPI_STATUS
AcpiSetGpe (
    ACPI_HANDLE             GpeDevice,
    UINT32                  GpeNumber,
    UINT8                   Action);

ACPI_STATUS
AcpiFinishGpe (
    ACPI_HANDLE             GpeDevice,
    UINT32                  GpeNumber);

ACPI_STATUS
AcpiSetupGpeForWake (
    ACPI_HANDLE             ParentDevice,
    ACPI_HANDLE             GpeDevice,
    UINT32                  GpeNumber);

ACPI_STATUS
AcpiSetGpeWakeMask (
    ACPI_HANDLE             GpeDevice,
    UINT32                  GpeNumber,
    UINT8                   Action);

ACPI_STATUS
AcpiGetGpeStatus (
    ACPI_HANDLE             GpeDevice,
    UINT32                  GpeNumber,
    ACPI_EVENT_STATUS       *EventStatus);

ACPI_STATUS
AcpiDisableAllGpes (
    void);

ACPI_STATUS
AcpiEnableAllRuntimeGpes (
    void);

ACPI_STATUS
AcpiGetGpeDevice (
    UINT32                  GpeIndex,
    ACPI_HANDLE             *GpeDevice);

ACPI_STATUS
AcpiInstallGpeBlock (
    ACPI_HANDLE             GpeDevice,
    ACPI_GENERIC_ADDRESS    *GpeBlockAddress,
    UINT32                  RegisterCount,
    UINT32                  InterruptNumber);

ACPI_STATUS
AcpiRemoveGpeBlock (
    ACPI_HANDLE             GpeDevice);


/*
 * Resource interfaces
 */
typedef
ACPI_STATUS (*ACPI_WALK_RESOURCE_CALLBACK) (
    ACPI_RESOURCE           *Resource,
    void                    *Context);

ACPI_STATUS
AcpiGetVendorResource (
    ACPI_HANDLE             Device,
    char                    *Name,
    ACPI_VENDOR_UUID        *Uuid,
    ACPI_BUFFER             *RetBuffer);

ACPI_STATUS
AcpiGetCurrentResources (
    ACPI_HANDLE             Device,
    ACPI_BUFFER             *RetBuffer);

ACPI_STATUS
AcpiGetPossibleResources (
    ACPI_HANDLE             Device,
    ACPI_BUFFER             *RetBuffer);

ACPI_STATUS
AcpiWalkResources (
    ACPI_HANDLE                 Device,
    char                        *Name,
    ACPI_WALK_RESOURCE_CALLBACK UserFunction,
    void                        *Context);

ACPI_STATUS
AcpiSetCurrentResources (
    ACPI_HANDLE             Device,
    ACPI_BUFFER             *InBuffer);

ACPI_STATUS
AcpiGetIrqRoutingTable (
    ACPI_HANDLE             Device,
    ACPI_BUFFER             *RetBuffer);

ACPI_STATUS
AcpiResourceToAddress64 (
    ACPI_RESOURCE           *Resource,
    ACPI_RESOURCE_ADDRESS64 *Out);


/*
 * Hardware (ACPI device) interfaces
 */
ACPI_STATUS
AcpiReset (
    void);

ACPI_STATUS
AcpiRead (
    UINT64                  *Value,
    ACPI_GENERIC_ADDRESS    *Reg);

ACPI_STATUS
AcpiWrite (
    UINT64                  Value,
    ACPI_GENERIC_ADDRESS    *Reg);

ACPI_STATUS
AcpiReadBitRegister (
    UINT32                  RegisterId,
    UINT32                  *ReturnValue);

ACPI_STATUS
AcpiWriteBitRegister (
    UINT32                  RegisterId,
    UINT32                  Value);

ACPI_STATUS
AcpiGetSleepTypeData (
    UINT8                   SleepState,
    UINT8                   *Slp_TypA,
    UINT8                   *Slp_TypB);

ACPI_STATUS
AcpiEnterSleepStatePrep (
    UINT8                   SleepState);

ACPI_STATUS
AcpiEnterSleepState (
    UINT8                   SleepState);

ACPI_STATUS
AcpiEnterSleepStateS4bios (
    void);

ACPI_STATUS
AcpiLeaveSleepState (
    UINT8                   SleepState)
    ;
ACPI_STATUS
AcpiSetFirmwareWakingVector (
    UINT32                  PhysicalAddress);

#if ACPI_MACHINE_WIDTH == 64
ACPI_STATUS
AcpiSetFirmwareWakingVector64 (
    UINT64                  PhysicalAddress);
#endif


/*
 * Error/Warning output
 */
void ACPI_INTERNAL_VAR_XFACE
AcpiError (
    const char              *ModuleName,
    UINT32                  LineNumber,
    const char              *Format,
    ...) ACPI_PRINTF_LIKE(3);

void  ACPI_INTERNAL_VAR_XFACE
AcpiException (
    const char              *ModuleName,
    UINT32                  LineNumber,
    ACPI_STATUS             Status,
    const char              *Format,
    ...) ACPI_PRINTF_LIKE(4);

void ACPI_INTERNAL_VAR_XFACE
AcpiWarning (
    const char              *ModuleName,
    UINT32                  LineNumber,
    const char              *Format,
    ...) ACPI_PRINTF_LIKE(3);

void ACPI_INTERNAL_VAR_XFACE
AcpiInfo (
    const char              *ModuleName,
    UINT32                  LineNumber,
    const char              *Format,
    ...) ACPI_PRINTF_LIKE(3);


/*
 * Debug output
 */
#ifdef ACPI_DEBUG_OUTPUT

void ACPI_INTERNAL_VAR_XFACE
AcpiDebugPrint (
    UINT32                  RequestedDebugLevel,
    UINT32                  LineNumber,
    const char              *FunctionName,
    const char              *ModuleName,
    UINT32                  ComponentId,
    const char              *Format,
    ...) ACPI_PRINTF_LIKE(6);

void ACPI_INTERNAL_VAR_XFACE
AcpiDebugPrintRaw (
    UINT32                  RequestedDebugLevel,
    UINT32                  LineNumber,
    const char              *FunctionName,
    const char              *ModuleName,
    UINT32                  ComponentId,
    const char              *Format,
    ...) ACPI_PRINTF_LIKE(6);
#endif

#endif /* __ACXFACE_H__ */
