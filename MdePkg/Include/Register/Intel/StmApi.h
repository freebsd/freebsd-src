/** @file
  STM API definition

  Copyright (c) 2015 - 2019, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Specification Reference:
  SMI Transfer Monitor (STM) User Guide Revision 1.00

**/

#ifndef _INTEL_STM_API_H_
#define _INTEL_STM_API_H_

#include <Register/Intel/StmStatusCode.h>
#include <Register/Intel/StmResourceDescriptor.h>
#include <Register/Intel/ArchitecturalMsr.h>

#pragma pack (1)

/**
  STM Header Structures
**/

typedef struct {
  UINT32  Intel64ModeSupported :1;  ///> bitfield
  UINT32  EptSupported         :1;  ///> bitfield
  UINT32  Reserved             :30; ///> must be 0
} STM_FEAT;

#define STM_SPEC_VERSION_MAJOR  1
#define STM_SPEC_VERSION_MINOR  0

typedef struct {
  UINT8     StmSpecVerMajor;
  UINT8     StmSpecVerMinor;
  ///
  /// Must be zero
  ///
  UINT16    Reserved;
  UINT32    StaticImageSize;
  UINT32    PerProcDynamicMemorySize;
  UINT32    AdditionalDynamicMemorySize;
  STM_FEAT  StmFeatures;
  UINT32    NumberOfRevIDs;
  UINT32    StmSmmRevID[1];
  ///
  /// The total STM_HEADER should be 4K.
  ///
} SOFTWARE_STM_HEADER;

typedef struct {
  MSEG_HEADER          HwStmHdr;
  SOFTWARE_STM_HEADER  SwStmHdr;
} STM_HEADER;


/**
  VMCALL API Numbers
  API number convention: BIOS facing VMCALL interfaces have bit 16 clear
**/

/**
  StmMapAddressRange enables a SMM guest to create a non-1:1 virtual to
  physical mapping of an address range into the SMM guest's virtual
  memory space.

  @param  EAX  #STM_API_MAP_ADDRESS_RANGE (0x00000001)
  @param  EBX  Low 32 bits of physical address of caller allocated
               STM_MAP_ADDRESS_RANGE_DESCRIPTOR structure.
  @param  ECX  High 32 bits of physical address of caller allocated
               STM_MAP_ADDRESS_RANGE_DESCRIPTOR structure. If Intel64Mode is
               clear (0), ECX must be 0.

  @note  All fields of STM_MAP_ADDRESS_RANGE_DESCRIPTOR are inputs only. They
         are not modified by StmMapAddressRange.

  @retval  CF   0
                No error, EAX set to STM_SUCCESS.
                The memory range was mapped as requested.
  @retval  CF   1
                An error occurred, EAX holds relevant error value.
  @retval  EAX  #ERROR_STM_SECURITY_VIOLATION
                The requested mapping contains a protected resource.
  @retval  EAX  #ERROR_STM_CACHE_TYPE_NOT_SUPPORTED
                The requested cache type could not be satisfied.
  @retval  EAX  #ERROR_STM_PAGE_NOT_FOUND
                Page count must not be zero.
  @retval  EAX  #ERROR_STM_FUNCTION_NOT_SUPPORTED
                STM supports EPT and has not implemented StmMapAddressRange().
  @retval  EAX  #ERROR_STM_UNSPECIFIED
                An unspecified error occurred.

  @note  All other registers unmodified.
**/
#define STM_API_MAP_ADDRESS_RANGE                  0x00000001

/**
  STM Map Address Range Descriptor for #STM_API_MAP_ADDRESS_RANGE VMCALL
**/
typedef struct {
  UINT64  PhysicalAddress;
  UINT64  VirtualAddress;
  UINT32  PageCount;
  UINT32  PatCacheType;
} STM_MAP_ADDRESS_RANGE_DESCRIPTOR;

/**
  Define values for PatCacheType field of #STM_MAP_ADDRESS_RANGE_DESCRIPTOR
  @{
**/
#define STM_MAP_ADDRESS_RANGE_PAT_CACHE_TYPE_ST_UC        0x00
#define STM_MAP_ADDRESS_RANGE_PAT_CACHE_TYPE_WC           0x01
#define STM_MAP_ADDRESS_RANGE_PAT_CACHE_TYPE_WT           0x04
#define STM_MAP_ADDRESS_RANGE_PAT_CACHE_TYPE_WP           0x05
#define STM_MAP_ADDRESS_RANGE_PAT_CACHE_TYPE_WB           0x06
#define STM_MAP_ADDRESS_RANGE_PAT_CACHE_TYPE_UC           0x07
#define STM_MAP_ADDRESS_RANGE_PAT_CACHE_TYPE_FOLLOW_MTRR  0xFFFFFFFF
/// @}

/**
  StmUnmapAddressRange enables a SMM guest to remove mappings from its page
  table.

  If TXT_PROCESSOR_SMM_DESCRIPTOR.EptEnabled bit is set by the STM, BIOS can
  control its own page tables. In this case, the STM implementation may
  optionally return ERROR_STM_FUNCTION_NOT_SUPPORTED.

  @param  EAX  #STM_API_UNMAP_ADDRESS_RANGE (0x00000002)
  @param  EBX  Low 32 bits of virtual address of caller allocated
               STM_UNMAP_ADDRESS_RANGE_DESCRIPTOR structure.
  @param  ECX  High 32 bits of virtual address of caller allocated
               STM_UNMAP_ADDRESS_RANGE_DESCRIPTOR structure. If Intel64Mode is
               clear (0), ECX must be zero.

  @retval  CF   0
                No error, EAX set to STM_SUCCESS. The memory range was unmapped
                as requested.
  @retval  CF   1
                An error occurred, EAX holds relevant error value.
  @retval  EAX  #ERROR_STM_FUNCTION_NOT_SUPPORTED
                STM supports EPT and has not implemented StmUnmapAddressRange().
  @retval  EAX  #ERROR_STM_UNSPECIFIED
                An unspecified error occurred.

  @note  All other registers unmodified.
**/
#define STM_API_UNMAP_ADDRESS_RANGE                0x00000002

/**
  STM Unmap Address Range Descriptor for #STM_API_UNMAP_ADDRESS_RANGE VMCALL
**/
typedef struct {
  UINT64  VirtualAddress;
  UINT32  Length;
} STM_UNMAP_ADDRESS_RANGE_DESCRIPTOR;


/**
  Since the normal OS environment runs with a different set of page tables than
  the SMM guest, virtual mappings will certainly be different. In order to do a
  guest virtual to host physical translation of an address from the normal OS
  code (EIP for example), it is necessary to walk the page tables governing the
  OS page mappings. Since the SMM guest has no direct access to the page tables,
  it must ask the STM to do this page table walk. This is supported via the
  StmAddressLookup VMCALL. All OS page table formats need to be supported,
  (e.g. PAE, PSE, Intel64, EPT, etc.)

  StmAddressLookup takes a CR3 value and a virtual address from the interrupted
  code as input and returns the corresponding physical address. It also
  optionally maps the physical address into the SMM guest's virtual address
  space. This new mapping persists ONLY for the duration of the SMI and if
  needed in subsequent SMIs it must be remapped. PAT cache types follow the
  interrupted environment's page table.

  If EPT is enabled, OS CR3 only provides guest physical address information,
  but the SMM guest might also need to know the host physical address. Since
  SMM does not have direct access rights to EPT (it is protected by the STM),
  SMM can input InterruptedEptp to let STM help to walk through it, and output
  the host physical address.

  @param  EAX  #STM_API_ADDRESS_LOOKUP (0x00000003)
  @param  EBX  Low 32 bits of virtual address of caller allocated
               STM_ADDRESS_LOOKUP_DESCRIPTOR structure.
  @param  ECX  High 32 bits of virtual address of caller allocated
               STM_ADDRESS_LOOKUP_DESCRIPTOR structure. If Intel64Mode is
               clear (0), ECX must be zero.

  @retval  CF   0
                No error, EAX set to STM_SUCCESS.  PhysicalAddress contains the
                host physical address determined by walking the interrupted SMM
                guest's page tables.  SmmGuestVirtualAddress contains the SMM
                guest's virtual mapping of the requested address.
  @retval  CF   1
                An error occurred, EAX holds relevant error value.
  @retval  EAX  #ERROR_STM_SECURITY_VIOLATION
                The requested page was a protected page.
  @retval  EAX  #ERROR_STM_PAGE_NOT_FOUND
                The requested virtual address did not exist in the page given
                page table.
  @retval  EAX  #ERROR_STM_BAD_CR3
                The CR3 input was invalid. CR3 values must be from one of the
                interrupted guest, or from the interrupted guest of another
                processor.
  @retval  EAX  #ERROR_STM_PHYSICAL_OVER_4G
                The resulting physical address is greater than 4G and no virtual
                address was supplied. The STM could not determine what address
                within the SMM guest's virtual address space to do the mapping.
                STM_ADDRESS_LOOKUP_DESCRIPTOR field PhysicalAddress contains the
                physical address determined by walking the interrupted
                environment's page tables.
  @retval  EAX  #ERROR_STM_VIRTUAL_SPACE_TOO_SMALL
                A specific virtual mapping was requested, but
                SmmGuestVirtualAddress + Length exceeds 4G and the SMI handler
                is running in 32 bit mode.
  @retval  EAX  #ERROR_STM_UNSPECIFIED
                An unspecified error occurred.

  @note  All other registers unmodified.
**/
#define STM_API_ADDRESS_LOOKUP                     0x00000003

/**
  STM Lookup Address Range Descriptor for #STM_API_ADDRESS_LOOKUP VMCALL
**/
typedef struct {
  UINT64  InterruptedGuestVirtualAddress;
  UINT32  Length;
  UINT64  InterruptedCr3;
  UINT64  InterruptedEptp;
  UINT32  MapToSmmGuest:2;
  UINT32  InterruptedCr4Pae:1;
  UINT32  InterruptedCr4Pse:1;
  UINT32  InterruptedIa32eMode:1;
  UINT32  Reserved1:27;
  UINT32  Reserved2;
  UINT64  PhysicalAddress;
  UINT64  SmmGuestVirtualAddress;
} STM_ADDRESS_LOOKUP_DESCRIPTOR;

/**
  Define values for the MapToSmmGuest field of #STM_ADDRESS_LOOKUP_DESCRIPTOR
  @{
**/
#define STM_ADDRESS_LOOKUP_DESCRIPTOR_DO_NOT_MAP                 0
#define STM_ADDRESS_LOOKUP_DESCRIPTOR_ONE_TO_ONE                 1
#define STM_ADDRESS_LOOKUP_DESCRIPTOR_VIRTUAL_ADDRESS_SPECIFIED  3
/// @}


/**
  When returning from a protection exception (see section 6.2), the SMM guest
  can instruct the STM to take one of two paths. It can either request a value
  be logged to the TXT.ERRORCODE register and subsequently reset the machine
  (indicating it couldn't resolve the problem), or it can request that the STM
  resume the SMM guest again with the specified register state.

  Unlike other VMCALL interfaces, StmReturnFromProtectionException behaves more
  like a jump or an IRET instruction than a "call". It does not return directly
  to the caller, but indirectly to a different location specified on the
  caller's stack (see section 6.2) or not at all.

  If the SMM guest STM protection exception handler itself causes a protection
  exception (e.g. a single nested exception), or more than 100 un-nested
  exceptions occur within the scope of a single SMI event, the STM must write
  STM_CRASH_PROTECTION_EXCEPTION_FAILURE to the TXT.ERRORCODE register and
  assert TXT.CMD.SYS_RESET. The reason for these restrictions is to simplify
  the code requirements while still enabling a reasonable debugging capability.

  @param  EAX  #STM_API_RETURN_FROM_PROTECTION_EXCEPTION (0x00000004)
  @param  EBX  If 0, resume SMM guest using register state found on exception
               stack.  If in range 0x01..0x0F, EBX contains a BIOS error code
               which the STM must record in the TXT.ERRORCODE register and
               subsequently reset the system via TXT.CMD.SYS_RESET. The value
               of the TXT.ERRORCODE register is calculated as follows:

                 TXT.ERRORCODE = (EBX & 0x0F) | STM_CRASH_BIOS_PANIC

               Values 0x10..0xFFFFFFFF are reserved, do not use.

**/
#define STM_API_RETURN_FROM_PROTECTION_EXCEPTION   0x00000004


/**
  VMCALL API Numbers
  API number convention: MLE facing VMCALL interfaces have bit 16 set.

  The STM configuration lifecycle is as follows:
    1. SENTER->SINIT->MLE: MLE begins execution with SMI disabled (masked).
    2. MLE invokes #STM_API_INITIALIZE_PROTECTION VMCALL to prepare STM for
       setup of initial protection profile. This is done on a single CPU and
       has global effect.
    3. MLE invokes #STM_API_PROTECT_RESOURCE VMCALL to define the initial
       protection profile. The protection profile is global across all CPUs.
    4. MLE invokes #STM_API_START VMCALL to enable the STM to begin receiving
       SMI events. This must be done on every logical CPU.
    5. MLE may invoke #STM_API_PROTECT_RESOURCE VMCALL or
       #STM_API_UNPROTECT_RESOURCE VMCALL during runtime as many times as
       necessary.
    6. MLE invokes #STM_API_STOP VMCALL to disable the STM. SMI is again masked
       following #STM_API_STOP VMCALL.
**/

/**
  StartStmVmcall() is used to configure an STM that is present in MSEG. SMIs
  should remain disabled from the invocation of GETSEC[SENTER] until they are
  re-enabled by StartStmVMCALL(). When StartStmVMCALL() returns, SMI is
  enabled and the STM has been started and is active. Prior to invoking
  StartStmVMCALL(), the MLE root should first invoke
  InitializeProtectionVMCALL() followed by as many iterations of
  ProtectResourceVMCALL() as necessary to establish the initial protection
  profile.  StartStmVmcall() must be invoked on all processor threads.

  @param  EAX  #STM_API_START (0x00010001)
  @param  EDX  STM configuration options. These provide the MLE with the
               ability to pass configuration parameters to the STM.

  @retval  CF   0
                No error, EAX set to STM_SUCCESS. The STM has been configured
                and is now active and the guarding all requested resources.
  @retval  CF   1
                An error occurred, EAX holds relevant error value.
  @retval  EAX  #ERROR_STM_ALREADY_STARTED
                The STM is already configured and active. STM remains active and
                guarding previously enabled resource list.
  @retval  EAX  #ERROR_STM_WITHOUT_SMX_UNSUPPORTED
                The StartStmVMCALL() was invoked from VMX root mode, but outside
                of SMX. This error code indicates the STM or platform does not
                support the STM outside of SMX. The SMI handler remains active
                and operates in legacy mode. See Appendix C
  @retval  EAX  #ERROR_STM_UNSUPPORTED_MSR_BIT
                The CPU doesn't support the MSR bit. The STM is not active.
  @retval  EAX  #ERROR_STM_UNSPECIFIED
                An unspecified error occurred.

  @note  All other registers unmodified.
**/
#define STM_API_START                              (BIT16 | 1)

/**
  Bit values for EDX input parameter to #STM_API_START VMCALL
  @{
**/
#define STM_CONFIG_SMI_UNBLOCKING_BY_VMX_OFF  BIT0
/// @}


/**
  The StopStmVMCALL() is invoked by the MLE to teardown an active STM. This is
  normally done as part of a full teardown of the SMX environment when the
  system is being shut down. At the time the call is invoked, SMI is enabled
  and the STM is active.  When the call returns, the STM has been stopped and
  all STM context is discarded and SMI is disabled.

  @param  EAX  #STM_API_STOP (0x00010002)

  @retval  CF   0
                No error, EAX set to STM_SUCCESS. The STM has been stopped and
                is no longer processing SMI events. SMI is blocked.
  @retval  CF   1
                An error occurred, EAX holds relevant error value.
  @retval  EAX  #ERROR_STM_STOPPED
                The STM was not active.
  @retval  EAX  #ERROR_STM_UNSPECIFIED
                An unspecified error occurred.

  @note  All other registers unmodified.
**/
#define STM_API_STOP                               (BIT16 | 2)


/**
  The ProtectResourceVMCALL() is invoked by the MLE root to request protection
  of specific resources. The request is defined by a STM_RESOURCE_LIST, which
  may contain more than one resource descriptor. Each resource descriptor is
  processed separately by the STM. Whether or not protection for any specific
  resource is granted is returned by the STM via the ReturnStatus bit in the
  associated STM_RSC_DESC_HEADER.

  @param  EAX  #STM_API_PROTECT_RESOURCE (0x00010003)
  @param  EBX  Low 32 bits of physical address of caller allocated
               STM_RESOURCE_LIST. Bits 11:0 are ignored and assumed to be zero,
               making the buffer 4K aligned.
  @param  ECX  High 32 bits of physical address of caller allocated
               STM_RESOURCE_LIST.

  @note  All fields of STM_RESOURCE_LIST are inputs only, except for the
         ReturnStatus bit. On input, the ReturnStatus bit must be clear. On
         return, the ReturnStatus bit is set for each resource request granted,
         and clear for each resource request denied. There are no other fields
         modified by ProtectResourceVMCALL(). The STM_RESOURCE_LIST must be
         contained entirely within a single 4K page.

  @retval  CF   0
                No error, EAX set to STM_SUCCESS. The STM has successfully
                merged the entire protection request into the active protection
                profile.  There is therefore no need to check the ReturnStatus
                bits in the STM_RESOURCE_LIST.
  @retval  CF   1
                An error occurred, EAX holds relevant error value.
  @retval  EAX  #ERROR_STM_UNPROTECTABLE_RESOURCE
                At least one of the requested resource protections intersects a
                BIOS required resource. Therefore, the caller must walk through
                the STM_RESOURCE_LIST to determine which of the requested
                resources was not granted protection. The entire list must be
                traversed since there may be multiple failures.
  @retval  EAX  #ERROR_STM_MALFORMED_RESOURCE_LIST
                The resource list could not be parsed correctly, or did not
                terminate before crossing a 4K page boundary. The caller must
                walk through the STM_RESOURCE_LIST to determine which of the
                requested resources was not granted protection. The entire list
                must be traversed since there may be multiple failures.
  @retval  EAX  #ERROR_STM_OUT_OF_RESOURCES
                The STM has encountered an internal error and cannot complete
                the request.
  @retval  EAX  #ERROR_STM_UNSPECIFIED
                An unspecified error occurred.

  @note  All other registers unmodified.
**/
#define STM_API_PROTECT_RESOURCE                   (BIT16 | 3)


/**
  The UnProtectResourceVMCALL() is invoked by the MLE root to request that the
  STM allow the SMI handler access to the specified resources.

  @param  EAX  #STM_API_UNPROTECT_RESOURCE (0x00010004)
  @param  EBX  Low 32 bits of physical address of caller allocated
               STM_RESOURCE_LIST. Bits 11:0 are ignored and assumed to be zero,
               making the buffer 4K aligned.
  @param  ECX  High 32 bits of physical address of caller allocated
               STM_RESOURCE_LIST.

  @note  All fields of STM_RESOURCE_LIST are inputs only, except for the
         ReturnStatus bit. On input, the ReturnStatus bit must be clear. On
         return, the ReturnStatus bit is set for each resource processed. For
         a properly formed STM_RESOURCE_LIST, this should be all resources
         listed. There are no other fields modified by
         UnProtectResourceVMCALL(). The STM_RESOURCE_LIST must be contained
         entirely within a single 4K page.

  @retval  CF   0
                No error, EAX set to STM_SUCCESS. The requested resources are
                not being guarded by the STM.
  @retval  CF   1
                An error occurred, EAX holds relevant error value.
  @retval  EAX  #ERROR_STM_MALFORMED_RESOURCE_LIST
                The resource list could not be parsed correctly, or did not
                terminate before crossing a 4K page boundary. The caller must
                walk through the STM_RESOURCE_LIST to determine which of the
                requested resources were not able to be unprotected. The entire
                list must be traversed since there may be multiple failures.
  @retval  EAX  #ERROR_STM_UNSPECIFIED
                An unspecified error occurred.

  @note  All other registers unmodified.
**/
#define STM_API_UNPROTECT_RESOURCE                 (BIT16 | 4)


/**
  The GetBiosResourcesVMCALL() is invoked by the MLE root to request the list
  of BIOS required resources from the STM.

  @param  EAX  #STM_API_GET_BIOS_RESOURCES (0x00010005)
  @param  EBX  Low 32 bits of physical address of caller allocated destination
               buffer. Bits 11:0 are ignored and assumed to be zero, making the
               buffer 4K aligned.
  @param  ECX  High 32 bits of physical address of caller allocated destination
               buffer.
  @param  EDX  Indicates which page of the BIOS resource list to copy into the
               destination buffer. The first page is indicated by 0, the second
               page by 1, etc.

  @retval  CF   0
                No error, EAX set to STM_SUCCESS. The destination buffer
                contains the BIOS required resources. If the page retrieved is
                the last page, EDX will be cleared to 0. If there are more pages
                to retrieve, EDX is incremented to the next page index. Calling
                software should iterate on GetBiosResourcesVMCALL() until EDX is
                returned cleared to 0.
  @retval  CF   1
                An error occurred, EAX holds relevant error value.
  @retval  EAX  #ERROR_STM_PAGE_NOT_FOUND
                The page index supplied in EDX input was out of range.
  @retval  EAX  #ERROR_STM_UNSPECIFIED
                An unspecified error occurred.
  @retval  EDX  Page index of next page to read. A return of EDX=0 signifies
                that the entire list has been read.
                @note  EDX is both an input and an output register.

  @note  All other registers unmodified.
**/
#define STM_API_GET_BIOS_RESOURCES                 (BIT16 | 5)


/**
  The ManageVmcsDatabaseVMCALL() is invoked by the MLE root to add or remove an
  MLE guest (including the MLE root) from the list of protected domains.

  @param  EAX  #STM_API_MANAGE_VMCS_DATABASE (0x00010006)
  @param  EBX  Low 32 bits of physical address of caller allocated
               STM_VMCS_DATABASE_REQUEST. Bits 11:0 are ignored and assumed to
               be zero, making the buffer 4K aligned.
  @param  ECX  High 32 bits of physical address of caller allocated
               STM_VMCS_DATABASE_REQUEST.

  @note  All fields of STM_VMCS_DATABASE_REQUEST are inputs only.  They are not
         modified by ManageVmcsDatabaseVMCALL().

  @retval  CF   0
                No error, EAX set to STM_SUCCESS.
  @retval  CF   1
                An error occurred, EAX holds relevant error value.
  @retval  EAX  #ERROR_STM_INVALID_VMCS
                Indicates a request to remove a VMCS from the database was made,
                but the referenced VMCS was not found in the database.
  @retval  EAX  #ERROR_STM_VMCS_PRESENT
                Indicates a request to add a VMCS to the database was made, but
                the referenced VMCS was already present in the database.
  @retval  EAX  #ERROR_INVALID_PARAMETER
                Indicates non-zero reserved field.
  @retval  EAX  #ERROR_STM_UNSPECIFIED
                An unspecified error occurred

  @note  All other registers unmodified.
**/
#define STM_API_MANAGE_VMCS_DATABASE               (BIT16 | 6)

/**
  STM VMCS Database Request for #STM_API_MANAGE_VMCS_DATABASE VMCALL
**/
typedef struct {
  ///
  /// bits 11:0 are reserved and must be 0
  ///
  UINT64  VmcsPhysPointer;
  UINT32  DomainType :4;
  UINT32  XStatePolicy :2;
  UINT32  DegradationPolicy :4;
  ///
  /// Must be 0
  ///
  UINT32  Reserved1 :22;
  UINT32  AddOrRemove;
} STM_VMCS_DATABASE_REQUEST;

/**
  Values for the DomainType field of #STM_VMCS_DATABASE_REQUEST
  @{
**/
#define DOMAIN_UNPROTECTED            0
#define DOMAIN_DISALLOWED_IO_OUT      BIT0
#define DOMAIN_DISALLOWED_IO_IN       BIT1
#define DOMAIN_INTEGRITY              BIT2
#define DOMAIN_CONFIDENTIALITY        BIT3
#define DOMAIN_INTEGRITY_PROT_OUT_IN  (DOMAIN_INTEGRITY)
#define DOMAIN_FULLY_PROT_OUT_IN      (DOMAIN_CONFIDENTIALITY | DOMAIN_INTEGRITY)
#define DOMAIN_FULLY_PROT             (DOMAIN_FULLY_PROT_OUT_IN | DOMAIN_DISALLOWED_IO_IN | DOMAIN_DISALLOWED_IO_OUT)
/// @}

/**
  Values for the XStatePolicy field of #STM_VMCS_DATABASE_REQUEST
  @{
**/
#define XSTATE_READWRITE  0x00
#define XSTATE_READONLY   0x01
#define XSTATE_SCRUB      0x03
/// @}

/**
  Values for the AddOrRemove field of #STM_VMCS_DATABASE_REQUEST
  @{
**/
#define STM_VMCS_DATABASE_REQUEST_ADD     1
#define STM_VMCS_DATABASE_REQUEST_REMOVE  0
/// @}


/**
  InitializeProtectionVMCALL() prepares the STM for setup of the initial
  protection profile which is subsequently communicated via one or more
  invocations of ProtectResourceVMCALL(), prior to invoking StartStmVMCALL().
  It is only necessary to invoke InitializeProtectionVMCALL() on one processor
  thread.  InitializeProtectionVMCALL() does not alter whether SMIs are masked
  or unmasked. The STM should return back to the MLE with "Blocking by SMI" set
  to 1 in the GUEST_INTERRUPTIBILITY field for the VMCS the STM created for the
  MLE guest.

  @param  EAX  #STM_API_INITIALIZE_PROTECTION (0x00010007)

  @retval  CF   0
                No error, EAX set to STM_SUCCESS, EBX bits set to indicate STM
                capabilities as defined below. The STM has set up an empty
                protection profile, except for the resources that it sets up to
                protect itself. The STM must not allow the SMI handler to map
                any pages from the MSEG Base to the top of TSEG. The STM must
                also not allow SMI handler access to those MSRs which the STM
                requires for its own protection.
  @retval  CF   1
                An error occurred, EAX holds relevant error value.
  @retval  EAX  #ERROR_STM_ALREADY_STARTED
                The STM is already configured and active. The STM remains active
                and guarding the previously enabled resource list.
  @retval  EAX  #ERROR_STM_UNPROTECTABLE
                The STM determines that based on the platform configuration, the
                STM is unable to protect itself. For example, the BIOS required
                resource list contains memory pages in MSEG.
  @retval  EAX  #ERROR_STM_UNSPECIFIED
                An unspecified error occurred.

  @note  All other registers unmodified.
**/
#define STM_API_INITIALIZE_PROTECTION              (BIT16 | 7)

/**
  Byte granular support bits returned in EBX from #STM_API_INITIALIZE_PROTECTION
  @{
**/
#define STM_RSC_BGI  BIT1
#define STM_RSC_BGM  BIT2
#define STM_RSC_MSR  BIT3
/// @}


/**
  The ManageEventLogVMCALL() is invoked by the MLE root to control the logging
  feature. It consists of several sub-functions to facilitate establishment of
  the log itself, configuring what events will be logged, and functions to
  start, stop, and clear the log.

  @param  EAX  #STM_API_MANAGE_EVENT_LOG (0x00010008)
  @param  EBX  Low 32 bits of physical address of caller allocated
               STM_EVENT_LOG_MANAGEMENT_REQUEST. Bits 11:0 are ignored and
               assumed to be zero, making the buffer 4K aligned.
  @param  ECX  High 32 bits of physical address of caller allocated
               STM_EVENT_LOG_MANAGEMENT_REQUEST.

  @retval  CF=0
           No error, EAX set to STM_SUCCESS.
  @retval  CF=1
           An error occurred, EAX holds relevant error value. See subfunction
           descriptions below for details.

  @note  All other registers unmodified.
**/
#define STM_API_MANAGE_EVENT_LOG                   (BIT16 | 8)

///
/// STM Event Log Management Request for #STM_API_MANAGE_EVENT_LOG VMCALL
///
typedef struct {
  UINT32      SubFunctionIndex;
  union {
    struct {
      UINT32  PageCount;
      //
      // number of elements is PageCount
      //
      UINT64  Pages[];
    } LogBuffer;
    //
    // bitmap of EVENT_TYPE
    //
    UINT32    EventEnableBitmap;
  } Data;
} STM_EVENT_LOG_MANAGEMENT_REQUEST;

/**
  Defines values for the SubFunctionIndex field of
  #STM_EVENT_LOG_MANAGEMENT_REQUEST
  @{
**/
#define STM_EVENT_LOG_MANAGEMENT_REQUEST_NEW_LOG        1
#define STM_EVENT_LOG_MANAGEMENT_REQUEST_CONFIGURE_LOG  2
#define STM_EVENT_LOG_MANAGEMENT_REQUEST_START_LOG      3
#define STM_EVENT_LOG_MANAGEMENT_REQUEST_STOP_LOG       4
#define STM_EVENT_LOG_MANAGEMENT_REQUEST_CLEAR_LOG      5
#define STM_EVENT_LOG_MANAGEMENT_REQUEST_DELETE_LOG     6
/// @}

/**
  Log Entry Header
**/
typedef struct {
  UINT32  EventSerialNumber;
  UINT16  Type;
  UINT16  Lock :1;
  UINT16  Valid :1;
  UINT16  ReadByMle :1;
  UINT16  Wrapped :1;
  UINT16  Reserved :12;
} LOG_ENTRY_HEADER;

/**
  Enum values for the Type field of #LOG_ENTRY_HEADER
**/
typedef enum {
  EvtLogStarted,
  EvtLogStopped,
  EvtLogInvalidParameterDetected,
  EvtHandledProtectionException,
  ///
  /// unhandled protection exceptions result in reset & cannot be logged
  ///
  EvtBiosAccessToUnclaimedResource,
  EvtMleResourceProtectionGranted,
  EvtMleResourceProtectionDenied,
  EvtMleResourceUnprotect,
  EvtMleResourceUnprotectError,
  EvtMleDomainTypeDegraded,
  ///
  /// add more here
  ///
  EvtMleMax,
  ///
  /// Not used
  ///
  EvtInvalid = 0xFFFFFFFF,
} EVENT_TYPE;

typedef struct {
  UINT32  Reserved;
} ENTRY_EVT_LOG_STARTED;

typedef struct {
  UINT32  Reserved;
} ENTRY_EVT_LOG_STOPPED;

typedef struct {
  UINT32  VmcallApiNumber;
} ENTRY_EVT_LOG_INVALID_PARAM;

typedef struct {
  STM_RSC  Resource;
} ENTRY_EVT_LOG_HANDLED_PROTECTION_EXCEPTION;

typedef struct {
  STM_RSC  Resource;
} ENTRY_EVT_BIOS_ACCESS_UNCLAIMED_RSC;

typedef struct {
  STM_RSC  Resource;
} ENTRY_EVT_MLE_RSC_PROT_GRANTED;

typedef struct {
  STM_RSC  Resource;
} ENTRY_EVT_MLE_RSC_PROT_DENIED;

typedef struct {
  STM_RSC  Resource;
} ENTRY_EVT_MLE_RSC_UNPROT;

typedef struct {
  STM_RSC  Resource;
} ENTRY_EVT_MLE_RSC_UNPROT_ERROR;

typedef struct {
  UINT64  VmcsPhysPointer;
  UINT8   ExpectedDomainType;
  UINT8   DegradedDomainType;
} ENTRY_EVT_MLE_DOMAIN_TYPE_DEGRADED;

typedef union {
  ENTRY_EVT_LOG_STARTED                       Started;
  ENTRY_EVT_LOG_STOPPED                       Stopped;
  ENTRY_EVT_LOG_INVALID_PARAM                 InvalidParam;
  ENTRY_EVT_LOG_HANDLED_PROTECTION_EXCEPTION  HandledProtectionException;
  ENTRY_EVT_BIOS_ACCESS_UNCLAIMED_RSC         BiosUnclaimedRsc;
  ENTRY_EVT_MLE_RSC_PROT_GRANTED              MleRscProtGranted;
  ENTRY_EVT_MLE_RSC_PROT_DENIED               MleRscProtDenied;
  ENTRY_EVT_MLE_RSC_UNPROT                    MleRscUnprot;
  ENTRY_EVT_MLE_RSC_UNPROT_ERROR              MleRscUnprotError;
  ENTRY_EVT_MLE_DOMAIN_TYPE_DEGRADED          MleDomainTypeDegraded;
} LOG_ENTRY_DATA;

typedef struct {
  LOG_ENTRY_HEADER  Hdr;
  LOG_ENTRY_DATA    Data;
} STM_LOG_ENTRY;

/**
  Maximum STM Log Entry Size
**/
#define STM_LOG_ENTRY_SIZE  256


/**
  STM Protection Exception Stack Frame Structures
**/

typedef struct {
  UINT32  Rdi;
  UINT32  Rsi;
  UINT32  Rbp;
  UINT32  Rdx;
  UINT32  Rcx;
  UINT32  Rbx;
  UINT32  Rax;
  UINT32  Cr3;
  UINT32  Cr2;
  UINT32  Cr0;
  UINT32  VmcsExitInstructionInfo;
  UINT32  VmcsExitInstructionLength;
  UINT64  VmcsExitQualification;
  ///
  /// An TXT_SMM_PROTECTION_EXCEPTION_TYPE num value
  ///
  UINT32  ErrorCode;
  UINT32  Rip;
  UINT32  Cs;
  UINT32  Rflags;
  UINT32  Rsp;
  UINT32  Ss;
} STM_PROTECTION_EXCEPTION_STACK_FRAME_IA32;

typedef struct {
  UINT64  R15;
  UINT64  R14;
  UINT64  R13;
  UINT64  R12;
  UINT64  R11;
  UINT64  R10;
  UINT64  R9;
  UINT64  R8;
  UINT64  Rdi;
  UINT64  Rsi;
  UINT64  Rbp;
  UINT64  Rdx;
  UINT64  Rcx;
  UINT64  Rbx;
  UINT64  Rax;
  UINT64  Cr8;
  UINT64  Cr3;
  UINT64  Cr2;
  UINT64  Cr0;
  UINT64  VmcsExitInstructionInfo;
  UINT64  VmcsExitInstructionLength;
  UINT64  VmcsExitQualification;
  ///
  /// An TXT_SMM_PROTECTION_EXCEPTION_TYPE num value
  ///
  UINT64  ErrorCode;
  UINT64  Rip;
  UINT64  Cs;
  UINT64  Rflags;
  UINT64  Rsp;
  UINT64  Ss;
} STM_PROTECTION_EXCEPTION_STACK_FRAME_X64;

typedef union {
  STM_PROTECTION_EXCEPTION_STACK_FRAME_IA32  *Ia32StackFrame;
  STM_PROTECTION_EXCEPTION_STACK_FRAME_X64   *X64StackFrame;
} STM_PROTECTION_EXCEPTION_STACK_FRAME;

/**
  Enum values for the ErrorCode field in
  #STM_PROTECTION_EXCEPTION_STACK_FRAME_IA32 and
  #STM_PROTECTION_EXCEPTION_STACK_FRAME_X64
**/
typedef enum {
  TxtSmmPageViolation = 1,
  TxtSmmMsrViolation,
  TxtSmmRegisterViolation,
  TxtSmmIoViolation,
  TxtSmmPciViolation
} TXT_SMM_PROTECTION_EXCEPTION_TYPE;

/**
  TXT Pocessor SMM Descriptor (PSD) structures
**/

typedef struct {
  UINT64  SpeRip;
  UINT64  SpeRsp;
  UINT16  SpeSs;
  UINT16  PageViolationException:1;
  UINT16  MsrViolationException:1;
  UINT16  RegisterViolationException:1;
  UINT16  IoViolationException:1;
  UINT16  PciViolationException:1;
  UINT16  Reserved1:11;
  UINT32  Reserved2;
} STM_PROTECTION_EXCEPTION_HANDLER;

typedef struct {
  UINT8  ExecutionDisableOutsideSmrr:1;
  UINT8  Intel64Mode:1;
  UINT8  Cr4Pae : 1;
  UINT8  Cr4Pse : 1;
  UINT8  Reserved1 : 4;
} STM_SMM_ENTRY_STATE;

typedef struct {
  UINT8  SmramToVmcsRestoreRequired : 1; ///> BIOS restore hint
  UINT8  ReinitializeVmcsRequired : 1;   ///> BIOS request
  UINT8  Reserved2 : 6;
} STM_SMM_RESUME_STATE;

typedef struct {
  UINT8  DomainType : 4;   ///> STM input to BIOS on each SMI
  UINT8  XStatePolicy : 2; ///> STM input to BIOS on each SMI
  UINT8  EptEnabled : 1;
  UINT8  Reserved3 : 1;
} STM_SMM_STATE;

#define TXT_SMM_PSD_OFFSET                          0xfb00
#define TXT_PROCESSOR_SMM_DESCRIPTOR_SIGNATURE      SIGNATURE_64('T', 'X', 'T', 'P', 'S', 'S', 'I', 'G')
#define TXT_PROCESSOR_SMM_DESCRIPTOR_VERSION_MAJOR  1
#define TXT_PROCESSOR_SMM_DESCRIPTOR_VERSION_MINOR  0

typedef struct {
  UINT64                            Signature;
  UINT16                            Size;
  UINT8                             SmmDescriptorVerMajor;
  UINT8                             SmmDescriptorVerMinor;
  UINT32                            LocalApicId;
  STM_SMM_ENTRY_STATE               SmmEntryState;
  STM_SMM_RESUME_STATE              SmmResumeState;
  STM_SMM_STATE                     StmSmmState;
  UINT8                             Reserved4;
  UINT16                            SmmCs;
  UINT16                            SmmDs;
  UINT16                            SmmSs;
  UINT16                            SmmOtherSegment;
  UINT16                            SmmTr;
  UINT16                            Reserved5;
  UINT64                            SmmCr3;
  UINT64                            SmmStmSetupRip;
  UINT64                            SmmStmTeardownRip;
  UINT64                            SmmSmiHandlerRip;
  UINT64                            SmmSmiHandlerRsp;
  UINT64                            SmmGdtPtr;
  UINT32                            SmmGdtSize;
  UINT32                            RequiredStmSmmRevId;
  STM_PROTECTION_EXCEPTION_HANDLER  StmProtectionExceptionHandler;
  UINT64                            Reserved6;
  UINT64                            BiosHwResourceRequirementsPtr;
  // extend area
  UINT64                            AcpiRsdp;
  UINT8                             PhysicalAddressBits;
} TXT_PROCESSOR_SMM_DESCRIPTOR;

#pragma pack ()

#endif
